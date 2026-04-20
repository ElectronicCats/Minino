"""
plots.py - Generation of charts for WiFi analysis
"""

import os

import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np
import pandas as pd
from matplotlib.colors import LinearSegmentedColormap

from ..utils.output import console, print_error, print_success

# ── Visual theme ──────────────────────────────────────────────────────────────
_FIG_BG     = "#0d1117"
_AX_BG      = "#161b22"
_TEXT       = "#e6edf3"
_SUBTEXT    = "#8b949e"
_GRID       = "#21262d"
_BORDER     = "#30363d"
_PALETTE    = ["#58a6ff", "#f78166", "#3fb950", "#d2a8ff", "#ffa657",
               "#79c0ff", "#ff7b72", "#56d364", "#bc8cff", "#ffa198"]
_CMAP_MAIN  = "plasma"
_CMAP_HEAT  = LinearSegmentedColormap.from_list(
    "wifi_heat", ["#0d1117", "#1f6feb", "#58a6ff", "#00d2ff", "#39ff14"]
)

def _style_axes(ax, title: str = "", xlabel: str = "", ylabel: str = "") -> None:
    """Apply the dark theme to a single Axes."""
    ax.set_facecolor(_AX_BG)
    for spine in ax.spines.values():
        spine.set_edgecolor(_BORDER)
    ax.tick_params(colors=_SUBTEXT, labelsize=9)
    ax.xaxis.label.set_color(_SUBTEXT)
    ax.yaxis.label.set_color(_SUBTEXT)
    ax.grid(color=_GRID, linestyle="--", linewidth=0.6, alpha=0.8)
    ax.set_axisbelow(True)
    if title:
        ax.set_title(title, color=_TEXT, fontsize=11, fontweight="bold", pad=10)
    if xlabel:
        ax.set_xlabel(xlabel, color=_SUBTEXT, fontsize=9)
    if ylabel:
        ax.set_ylabel(ylabel, color=_SUBTEXT, fontsize=9)


def _make_figure(rows: int, cols: int, figsize: tuple, suptitle: str):
    """Create a styled figure with dark background."""
    fig, axes = plt.subplots(rows, cols, figsize=figsize)
    fig.patch.set_facecolor(_FIG_BG)
    fig.suptitle(
        suptitle,
        fontsize=15,
        fontweight="bold",
        color=_TEXT,
        y=0.98,
    )
    return fig, axes


# ── Interference plots ────────────────────────────────────────────────────────

def generate_interference_plots(df: pd.DataFrame, csv_file: str, output_dir: str = ".") -> str:
    """
    Generates four interference analysis plots and saves them as PNG.
    """
    try:
        fig, axes = _make_figure(2, 2, (15, 12), "WiFi Interference Analysis")

        # — Networks by channel (bar) ——————————————————————————————————————
        ax = axes[0, 0]
        channel_counts = df["Channel"].value_counts().sort_index()
        bars = ax.bar(
            channel_counts.index.astype(str),
            channel_counts.values,
            color=_PALETTE[0],
            edgecolor=_BORDER,
            linewidth=0.7,
            zorder=3,
        )
        # Gradient tint: darker top edge per bar
        for bar, val in zip(bars, channel_counts.values):
            bar.set_alpha(0.85 + 0.15 * (val / channel_counts.max()))
        _style_axes(ax, "Network Distribution by Channel", "Channel", "Number of Networks")
        ax.yaxis.set_major_locator(mticker.MaxNLocator(integer=True))

        # — Average RSSI by channel (bar) ——————————————————————————————————
        ax = axes[0, 1]
        channel_rssi = df.groupby("Channel")["RSSI"].mean()
        ax.bar(
            channel_rssi.index.astype(str),
            channel_rssi.values,
            color=_PALETTE[1],
            edgecolor=_BORDER,
            linewidth=0.7,
            alpha=0.88,
            zorder=3,
        )
        _style_axes(ax, "Average Signal Strength by Channel", "Channel", "Average RSSI (dBm)")

        # — Signal quality pie ——————————————————————————————————————————————
        ax = axes[1, 0]
        quality_counts = df["Quality"].value_counts()
        wedges, texts, autotexts = ax.pie(
            quality_counts.values,
            labels=quality_counts.index,
            autopct="%1.1f%%",
            colors=_PALETTE[: len(quality_counts)],
            startangle=90,
            pctdistance=0.82,
            wedgeprops={"linewidth": 1.2, "edgecolor": _FIG_BG},
            shadow=False,
        )
        for t in texts:
            t.set_color(_SUBTEXT)
            t.set_fontsize(9)
        for at in autotexts:
            at.set_color(_TEXT)
            at.set_fontsize(8)
            at.set_fontweight("bold")
        ax.set_facecolor(_AX_BG)
        ax.set_title("Signal Quality Distribution", color=_TEXT, fontsize=11, fontweight="bold", pad=10)

        # — Top-8 SSIDs (bar) ————————————————————————————————————————————
        ax = axes[1, 1]
        ssid_counts = df["SSID"].value_counts().head(8)
        ax.bar(
            range(len(ssid_counts)),
            ssid_counts.values,
            color=_PALETTE[3],
            edgecolor=_BORDER,
            linewidth=0.7,
            alpha=0.88,
            zorder=3,
        )
        ax.set_xticks(range(len(ssid_counts)))
        ax.set_xticklabels(ssid_counts.index, rotation=40, ha="right", fontsize=8, color=_SUBTEXT)
        _style_axes(ax, "Networks by SSID (Top 8)", "SSID", "Number of Networks")
        ax.yaxis.set_major_locator(mticker.MaxNLocator(integer=True))

        plt.tight_layout(rect=[0, 0, 1, 0.96])
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        output_path = os.path.join(output_dir, f"{base_name}_analysis.png")
        plt.savefig(output_path, dpi=300, bbox_inches="tight", facecolor=_FIG_BG)
        plt.close()

        print_success(f"Plots saved: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error generating plots: {exc}")
        return ""


# ── Wardriving plots ──────────────────────────────────────────────────────────

def generate_wardriving_plots(df: pd.DataFrame, base_name: str, output_dir: str = ".") -> str:
    """
    Generates six advanced wardriving plots and saves them as PNG.
    """
    try:
        fig, axes = _make_figure(3, 2, (15, 15), f"Wardriving Analysis — {base_name}")

        # — RSSI boxplot by channel ————————————————————————————————————————
        ax = axes[0, 0]
        if "Channel" in df.columns and "RSSI" in df.columns:
            channels = sorted(df["Channel"].dropna().unique())
            data = [df[df["Channel"] == c]["RSSI"].dropna() for c in channels]
            bp = ax.boxplot(
                data,
                tick_labels=channels,
                patch_artist=True,
                notch=False,
                whiskerprops={"color": _SUBTEXT, "linewidth": 1.2},
                capprops={"color": _SUBTEXT, "linewidth": 1.2},
                medianprops={"color": _PALETTE[4], "linewidth": 2},
                flierprops={"markerfacecolor": _PALETTE[1], "markersize": 3, "alpha": 0.6},
                boxprops={"linewidth": 0.8},
            )
            for patch, color in zip(bp["boxes"], [_PALETTE[i % len(_PALETTE)] for i in range(len(channels))]):
                patch.set_facecolor(color)
                patch.set_alpha(0.6)
        _style_axes(ax, "RSSI Distribution by Channel", "Channel", "RSSI (dBm)")

        # — Average RSSI scatter ———————————————————————————————————————————
        ax = axes[0, 1]
        if "Channel" in df.columns and "RSSI" in df.columns:
            channel_rssi = df.groupby("Channel")["RSSI"].mean().reset_index()
            sc = ax.scatter(
                channel_rssi["Channel"],
                channel_rssi["RSSI"],
                c=channel_rssi["RSSI"],
                cmap=_CMAP_MAIN,
                s=120,
                edgecolors=_BORDER,
                linewidths=0.8,
                zorder=3,
            )
            cbar = plt.colorbar(sc, ax=ax)
            cbar.set_label("Average RSSI (dBm)", color=_SUBTEXT, fontsize=9)
            cbar.ax.yaxis.set_tick_params(color=_SUBTEXT)
            plt.setp(cbar.ax.yaxis.get_ticklabels(), color=_SUBTEXT, fontsize=8)
            cbar.outline.set_edgecolor(_BORDER)
        _style_axes(ax, "Average RSSI by Channel", "Channel", "Average RSSI (dBm)")

        # — Temporal signal evolution (line) ——————————————————————————————
        ax = axes[1, 0]
        if "Timestamp" in df.columns and "RSSI" in df.columns:
            df_sorted = df.sort_values("Timestamp")
            ax.plot(
                df_sorted["Timestamp"],
                df_sorted["RSSI"],
                color=_PALETTE[0],
                linewidth=1.0,
                alpha=0.85,
                marker="o",
                markersize=2,
                markerfacecolor=_PALETTE[4],
                markeredgewidth=0,
                zorder=3,
            )
            ax.tick_params(axis="x", rotation=45, labelsize=8)
        _style_axes(ax, "Temporal Signal Evolution", "Time", "RSSI (dBm)")

        # — Authentication methods pie ————————————————————————————————————
        ax = axes[1, 1]
        if "AuthMode" in df.columns:
            auth_counts = df["AuthMode"].value_counts()
            wedges, texts, autotexts = ax.pie(
                auth_counts.values,
                labels=auth_counts.index,
                autopct="%1.1f%%",
                colors=_PALETTE[: len(auth_counts)],
                startangle=90,
                pctdistance=0.82,
                wedgeprops={"linewidth": 1.2, "edgecolor": _FIG_BG},
            )
            for t in texts:
                t.set_color(_SUBTEXT)
                t.set_fontsize(9)
            for at in autotexts:
                at.set_color(_TEXT)
                at.set_fontsize(8)
                at.set_fontweight("bold")
        ax.set_facecolor(_AX_BG)
        ax.set_title("Authentication Methods", color=_TEXT, fontsize=11, fontweight="bold", pad=10)

        # — Hexbin density map ————————————————————————————————————————————
        ax = axes[2, 0]
        if all(c in df.columns for c in ["CurrentLongitude", "CurrentLatitude", "RSSI"]):
            hb = ax.hexbin(
                df["CurrentLongitude"],
                df["CurrentLatitude"],
                C=df["RSSI"],
                gridsize=15,
                cmap=_CMAP_HEAT,
                reduce_C_function=np.mean,
                linewidths=0.3,
            )
            cbar = plt.colorbar(hb, ax=ax)
            cbar.set_label("Average RSSI (dBm)", color=_SUBTEXT, fontsize=9)
            cbar.ax.yaxis.set_tick_params(color=_SUBTEXT)
            plt.setp(cbar.ax.yaxis.get_ticklabels(), color=_SUBTEXT, fontsize=8)
            cbar.outline.set_edgecolor(_BORDER)
        _style_axes(ax, "Network Density and Signal Strength", "Longitude", "Latitude")

        # — RSSI histogram ————————————————————————————————————————————————
        ax = axes[2, 1]
        if "RSSI" in df.columns:
            n, bins, patches = ax.hist(
                df["RSSI"],
                bins=20,
                color=_PALETTE[2],
                edgecolor=_BORDER,
                linewidth=0.6,
                alpha=0.85,
                zorder=3,
            )
            # Color gradient from weak to strong signal
            norm = plt.Normalize(bins.min(), bins.max())
            for patch, left in zip(patches, bins[:-1]):
                patch.set_facecolor(plt.cm.plasma(norm(left)))
                patch.set_alpha(0.85)
        _style_axes(ax, "Signal Strength Distribution", "RSSI (dBm)", "Frequency")

        plt.tight_layout(rect=[0, 0, 1, 0.96])
        output_path = os.path.join(output_dir, f"advanced_plots_{base_name}.png")
        plt.savefig(output_path, dpi=300, bbox_inches="tight", facecolor=_FIG_BG)
        plt.close()

        print_success(f"Advanced plots saved: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error generating plots: {exc}")
        return ""
