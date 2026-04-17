"""
plots.py - Generation of charts for WiFi analysis
"""

import os

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from ..utils.output import console, print_error, print_success


def generate_interference_plots(df: pd.DataFrame, csv_file: str, output_dir: str = ".") -> str:
    """
    Generates four interference analysis plots and saves them as PNG.
    """
    try:
        plt.style.use("default")
        fig, axes = plt.subplots(2, 2, figsize=(15, 12))
        fig.suptitle("WiFi Interference Analysis", fontsize=16)

        channel_counts = df["Channel"].value_counts().sort_index()
        axes[0, 0].bar(channel_counts.index.astype(str), channel_counts.values, color="skyblue")
        axes[0, 0].set_title("Network Distribution by Channel")
        axes[0, 0].set_xlabel("Channel")
        axes[0, 0].set_ylabel("Number of Networks")

        channel_rssi = df.groupby("Channel")["RSSI"].mean()
        axes[0, 1].bar(channel_rssi.index.astype(str), channel_rssi.values, color="lightcoral")
        axes[0, 1].set_title("Average Signal Strength by Channel")
        axes[0, 1].set_xlabel("Channel")
        axes[0, 1].set_ylabel("Average RSSI (dBm)")

        quality_counts = df["Quality"].value_counts()
        colors = ["#4CAF50", "#8BC34A", "#FFC107", "#FF9800", "#F44336"]
        axes[1, 0].pie(quality_counts.values, labels=quality_counts.index, autopct="%1.1f%%", colors=colors)
        axes[1, 0].set_title("Signal Quality Distribution")

        ssid_counts = df["SSID"].value_counts().head(8)
        axes[1, 1].bar(range(len(ssid_counts)), ssid_counts.values, color="mediumpurple")
        axes[1, 1].set_title("Networks by SSID (Top 8)")
        axes[1, 1].set_xlabel("SSID")
        axes[1, 1].set_ylabel("Number of Networks")
        axes[1, 1].set_xticks(range(len(ssid_counts)))
        axes[1, 1].set_xticklabels(ssid_counts.index, rotation=45, ha="right")

        plt.tight_layout()
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        output_path = os.path.join(output_dir, f"{base_name}_analysis.png")
        plt.savefig(output_path, dpi=300, bbox_inches="tight")
        plt.close()

        print_success(f"Plots saved: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error generating plots: {exc}")
        return ""


def generate_wardriving_plots(df: pd.DataFrame, base_name: str, output_dir: str = ".") -> str:
    """
    Generates six advanced wardriving plots and saves them as PNG.
    """
    try:
        fig, axes = plt.subplots(3, 2, figsize=(15, 15))
        fig.suptitle(f"Wardriving Analysis — {base_name}", fontsize=16, fontweight="bold")

        if "Channel" in df.columns and "RSSI" in df.columns:
            channels = sorted(df["Channel"].dropna().unique())
            data = [df[df["Channel"] == c]["RSSI"] for c in channels]
            axes[0, 0].boxplot(data, tick_labels=channels)
            axes[0, 0].set_title("RSSI Distribution by Channel")
            axes[0, 0].set_xlabel("Channel")
            axes[0, 0].set_ylabel("RSSI (dBm)")
            axes[0, 0].grid(True, alpha=0.3)

        if "Channel" in df.columns and "RSSI" in df.columns:
            channel_rssi = df.groupby("Channel")["RSSI"].mean().reset_index()
            sc = axes[0, 1].scatter(
                channel_rssi["Channel"], channel_rssi["RSSI"],
                c=channel_rssi["RSSI"], cmap="viridis", s=100,
            )
            plt.colorbar(sc, ax=axes[0, 1], label="Average RSSI (dBm)")
            axes[0, 1].set_title("Average RSSI by Channel")
            axes[0, 1].set_xlabel("Channel")
            axes[0, 1].set_ylabel("Average RSSI (dBm)")
            axes[0, 1].grid(True, alpha=0.3)

        if "Timestamp" in df.columns and "RSSI" in df.columns:
            df_sorted = df.sort_values("Timestamp")
            axes[1, 0].plot(df_sorted["Timestamp"], df_sorted["RSSI"], "o-", alpha=0.7, markersize=2)
            axes[1, 0].set_title("Temporal Signal Evolution")
            axes[1, 0].set_xlabel("Time")
            axes[1, 0].set_ylabel("RSSI (dBm)")
            axes[1, 0].tick_params(axis="x", rotation=45)
            axes[1, 0].grid(True, alpha=0.3)

        if "AuthMode" in df.columns:
            auth_counts = df["AuthMode"].value_counts()
            axes[1, 1].pie(auth_counts.values, labels=auth_counts.index, autopct="%1.1f%%")
            axes[1, 1].set_title("Authentication Methods")

        if all(c in df.columns for c in ["CurrentLongitude", "CurrentLatitude", "RSSI"]):
            hb = axes[2, 0].hexbin(
                df["CurrentLongitude"], df["CurrentLatitude"],
                C=df["RSSI"], gridsize=15, cmap="viridis",
                reduce_C_function=np.mean,
            )
            plt.colorbar(hb, ax=axes[2, 0], label="Average RSSI (dBm)")
            axes[2, 0].set_title("Network Density and Signal Strength")
            axes[2, 0].set_xlabel("Longitude")
            axes[2, 0].set_ylabel("Latitude")

        if "RSSI" in df.columns:
            axes[2, 1].hist(df["RSSI"], bins=20, alpha=0.7, edgecolor="black")
            axes[2, 1].set_title("Signal Strength Distribution")
            axes[2, 1].set_xlabel("RSSI (dBm)")
            axes[2, 1].set_ylabel("Frequency")
            axes[2, 1].grid(True, alpha=0.3)

        plt.tight_layout()
        output_path = os.path.join(output_dir, f"advanced_plots_{base_name}.png")
        plt.savefig(output_path, dpi=300, bbox_inches="tight")
        plt.close()

        print_success(f"Advanced plots saved: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error generating plots: {exc}")
        return ""
