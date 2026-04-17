"""
plots.py - Generación de gráficos para análisis WiFi
"""

import os

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from ..utils.output import console, print_error, print_success


def generate_interference_plots(df: pd.DataFrame, csv_file: str) -> str:
    """
    Genera cuatro gráficos de análisis de interferencias y los guarda como PNG.
    """
    try:
        plt.style.use("default")
        fig, axes = plt.subplots(2, 2, figsize=(15, 12))
        fig.suptitle("Análisis de Interferencias WiFi", fontsize=16)

        channel_counts = df["Channel"].value_counts().sort_index()
        axes[0, 0].bar(channel_counts.index.astype(str), channel_counts.values, color="skyblue")
        axes[0, 0].set_title("Distribución de Redes por Canal")
        axes[0, 0].set_xlabel("Canal")
        axes[0, 0].set_ylabel("Número de Redes")

        channel_rssi = df.groupby("Channel")["RSSI"].mean()
        axes[0, 1].bar(channel_rssi.index.astype(str), channel_rssi.values, color="lightcoral")
        axes[0, 1].set_title("Intensidad Promedio de Señal por Canal")
        axes[0, 1].set_xlabel("Canal")
        axes[0, 1].set_ylabel("RSSI Promedio (dBm)")

        quality_counts = df["Calidad"].value_counts()
        colors = ["#4CAF50", "#8BC34A", "#FFC107", "#FF9800", "#F44336"]
        axes[1, 0].pie(quality_counts.values, labels=quality_counts.index, autopct="%1.1f%%", colors=colors)
        axes[1, 0].set_title("Distribución de Calidad de Señal")

        ssid_counts = df["SSID"].value_counts().head(8)
        axes[1, 1].bar(range(len(ssid_counts)), ssid_counts.values, color="mediumpurple")
        axes[1, 1].set_title("Redes por SSID (Top 8)")
        axes[1, 1].set_xlabel("SSID")
        axes[1, 1].set_ylabel("Número de Redes")
        axes[1, 1].set_xticks(range(len(ssid_counts)))
        axes[1, 1].set_xticklabels(ssid_counts.index, rotation=45, ha="right")

        plt.tight_layout()
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        output_path = f"{base_name}_analysis.png"
        plt.savefig(output_path, dpi=300, bbox_inches="tight")
        plt.close()

        print_success(f"Gráficas guardadas: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error al generar visualizaciones: {exc}")
        return ""


def generate_wardriving_plots(df: pd.DataFrame, nombre_base: str) -> str:
    """
    Genera seis gráficos avanzados de wardriving y los guarda como PNG.
    """
    try:
        fig, axes = plt.subplots(3, 2, figsize=(15, 15))
        fig.suptitle(f"Análisis Wardriving — {nombre_base}", fontsize=16, fontweight="bold")

        if "Channel" in df.columns and "RSSI" in df.columns:
            canales = sorted(df["Channel"].dropna().unique())
            datos = [df[df["Channel"] == c]["RSSI"] for c in canales]
            axes[0, 0].boxplot(datos, tick_labels=canales)
            axes[0, 0].set_title("Distribución de RSSI por Canal")
            axes[0, 0].set_xlabel("Canal")
            axes[0, 0].set_ylabel("RSSI (dBm)")
            axes[0, 0].grid(True, alpha=0.3)

        if "Channel" in df.columns and "RSSI" in df.columns:
            canal_rssi = df.groupby("Channel")["RSSI"].mean().reset_index()
            sc = axes[0, 1].scatter(
                canal_rssi["Channel"], canal_rssi["RSSI"],
                c=canal_rssi["RSSI"], cmap="viridis", s=100,
            )
            plt.colorbar(sc, ax=axes[0, 1], label="RSSI Promedio (dBm)")
            axes[0, 1].set_title("RSSI Promedio por Canal")
            axes[0, 1].set_xlabel("Canal")
            axes[0, 1].set_ylabel("RSSI Promedio (dBm)")
            axes[0, 1].grid(True, alpha=0.3)

        if "Timestamp" in df.columns and "RSSI" in df.columns:
            df_sorted = df.sort_values("Timestamp")
            axes[1, 0].plot(df_sorted["Timestamp"], df_sorted["RSSI"], "o-", alpha=0.7, markersize=2)
            axes[1, 0].set_title("Evolución Temporal de la Señal")
            axes[1, 0].set_xlabel("Tiempo")
            axes[1, 0].set_ylabel("RSSI (dBm)")
            axes[1, 0].tick_params(axis="x", rotation=45)
            axes[1, 0].grid(True, alpha=0.3)

        if "AuthMode" in df.columns:
            auth_counts = df["AuthMode"].value_counts()
            axes[1, 1].pie(auth_counts.values, labels=auth_counts.index, autopct="%1.1f%%")
            axes[1, 1].set_title("Métodos de Autenticación")

        if all(c in df.columns for c in ["CurrentLongitude", "CurrentLatitude", "RSSI"]):
            hb = axes[2, 0].hexbin(
                df["CurrentLongitude"], df["CurrentLatitude"],
                C=df["RSSI"], gridsize=15, cmap="viridis",
                reduce_C_function=np.mean,
            )
            plt.colorbar(hb, ax=axes[2, 0], label="RSSI Promedio (dBm)")
            axes[2, 0].set_title("Densidad de Redes e Intensidad de Señal")
            axes[2, 0].set_xlabel("Longitud")
            axes[2, 0].set_ylabel("Latitud")

        if "RSSI" in df.columns:
            axes[2, 1].hist(df["RSSI"], bins=20, alpha=0.7, edgecolor="black")
            axes[2, 1].set_title("Distribución de Intensidad de Señal")
            axes[2, 1].set_xlabel("RSSI (dBm)")
            axes[2, 1].set_ylabel("Frecuencia")
            axes[2, 1].grid(True, alpha=0.3)

        plt.tight_layout()
        output_path = f"graficos_Avanzados_{nombre_base}.png"
        plt.savefig(output_path, dpi=300, bbox_inches="tight")
        plt.close()

        print_success(f"Gráficos avanzados guardados: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error generando gráficos: {exc}")
        return ""
