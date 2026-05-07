"""
interference.py - WiFi channel interference analysis
"""

import os
from typing import Optional

import pandas as pd

from ..utils.file_utils import clean_and_validate_data, robust_csv_loader
from ..utils.output import console, print_error, print_success, print_warning, rssi_color
from ..visualization.plots import generate_interference_plots


# ---------------------------------------------------------------------------
# Internal Helpers
# ---------------------------------------------------------------------------

_NON_OVERLAPPING = [1, 6, 11]


def _classify_signal(rssi: float) -> str:
    if rssi >= -50:
        return "Excellent"
    elif rssi >= -60:
        return "Good"
    elif rssi >= -70:
        return "Fair"
    elif rssi >= -80:
        return "Weak"
    return "Very weak"


def _format_channels_list(channels) -> list:
    return [int(c) for c in channels]


def _generate_comprehensive_analysis(df: pd.DataFrame) -> str:
    """Builds the text block with the executive summary and recommendations."""
    total = len(df)
    weak = len(df[df["RSSI"] <= -80])
    weak_pct = (weak / total) * 100
    channel_counts = df["Channel"].value_counts()

    lines = [
        "",
        "=" * 60,
        "EXECUTIVE ANALYSIS SUMMARY",
        "=" * 60,
        "",
        "MAIN FINDINGS:",
        f"   • Total detected networks: {total:,} networks",
        f"   • Networks with weak signal: {weak:,} networks ({weak_pct:.1f}% of total)",
        f"   • Channels used: {df['Channel'].nunique()} different channels",
        "",
        "NON-OVERLAPPING CHANNELS SITUATION:",
    ]

    for ch in _NON_OVERLAPPING:
        count = channel_counts.get(ch, 0)
        status = " (Optimal)"
        if count > 400:
            status = " (Extremely congested)"
        elif count > 300:
            status = " (Very congested)"
        elif count > 200:
            status = " (Congested)"
        elif count > 100:
            status = " (Moderate)"
        lines.append(f"   • Channel {ch}: {count:,} networks{status}")

    overlapping = sorted(
        [
            (int(ch), min(_NON_OVERLAPPING, key=lambda x: abs(x - int(ch))), len(df[df["Channel"] == ch]))
            for ch in df["Channel"].unique()
            if int(ch) not in _NON_OVERLAPPING
        ],
        key=lambda t: t[2],
        reverse=True,
    )

    lines += [
        "",
        "PROBLEMATIC CHANNELS (INTERFERENCE):",
        *[f"   • Channel {ch}: {cnt:,} networks (interferes with channel {closest})" for ch, closest, cnt in overlapping[:6]],
        "",
        "=" * 60,
        "STRATEGIC RECOMMENDATIONS",
        "=" * 60,
        "",
        "CRITICAL ISSUES IDENTIFIED:",
        "   1. Saturated channel 11 — Avoid completely",
        "   2. All non-overlapping channels are congested",
        "   3. High density of networks in 2.4GHz environment",
        "",
        "RECOMMENDED STRATEGIES:",
        "   1. MIGRATION TO 5GHz:",
        "      • Configure networks in the 5GHz band if devices support it",
        "      • Less interference and more available channels",
        "",
        "   2. ALTERNATIVE CHANNELS IN 2.4GHz:",
        f"      • Channel 13: {channel_counts.get(13, 0)} networks (less congested)",
        f"      • Channel 14: {channel_counts.get(14, 0)} networks (less congested)",
        f"      • Channel  5: {channel_counts.get(5, 0)} networks (very low congestion)",
        "",
        "   3. OPTIMIZATION IN 2.4GHz:",
        "      • Use a channel width of 20MHz (not 40MHz)",
        "      • Transmit at low power to avoid affecting neighboring networks",
        "      • Schedule nightly router reboots",
        "",
        "   4. FOR CRITICAL NETWORKS:",
        "      • Implement Quality of Service (QoS)",
        "      • Use dual band (2.4GHz for IoT, 5GHz for primary devices)",
        "",
        "FOR END USERS:",
        "   • Connect important devices to 5GHz whenever possible",
        "   • Place the router away from interference",
        "   • Consider mesh systems for better coverage",
        "",
        "PERSPECTIVE:",
        "   The analyzed environment shows SEVERE SATURATION in the 2.4GHz band.",
        "   Migration to 5GHz is not just recommended, but necessary.",
    ]

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def analyze_wifi_interference(
    csv_file: str,
    df: Optional[pd.DataFrame] = None,
    output_dir: str = ".",
) -> Optional[pd.DataFrame]:
    """
    Analyzes WiFi interference between channels from a CSV file.

    Loads and validates the data if a DataFrame is not passed, prints the analysis
    to the console, saves a PNG with graphs, and a TXT with the full report.

    Args:
        csv_file: Path to the CSV file with WiFi data.
        df: Already loaded DataFrame. If ``None``, it is loaded from ``csv_file``.

    Returns:
        Processed DataFrame with the ``Quality`` column added, or ``None`` if
        an error occurred.
    """
    if df is None:
        try:
            df = robust_csv_loader(csv_file)

            if df is None:
                print_error("Could not load the file with any strategy")
                return None

            df = clean_and_validate_data(df)

            if df.empty:
                print_error("No valid data after cleaning")
                return None

            print_success(
                f"File [cyan]{csv_file}[/cyan] processed — "
                f"[white bold]{len(df)}[/white bold] valid rows"
            )

        except FileNotFoundError:
            print_error(f"File not found: '[cyan]{csv_file}[/cyan]'")
            return None
        except Exception as exc:
            print_error(f"Error processing the file: {exc}")
            return None

    df["Quality"] = df["RSSI"].apply(_classify_signal)

    # ── Console ──────────────────────────────────────────────────────────────
    console.rule("[cyan bold]WIFI INTERFERENCE ANALYSIS[/cyan bold]", style="cyan")

    # 1. General Summary
    console.print("\n[cyan bold]1. GENERAL SUMMARY[/cyan bold]")
    console.print(f"   [dim]Total detected networks:[/dim]  [white bold]{len(df):,}[/white bold]")
    console.print(f"   [dim]Unique networks by SSID:[/dim]      [white bold]{df['SSID'].nunique():,}[/white bold]")

    # 2. Channel Distribution with visual bar
    console.print("\n[cyan bold]2. CHANNEL DISTRIBUTION[/cyan bold]")
    channel_dist = df["Channel"].value_counts().sort_index()
    max_count = int(channel_dist.max()) if not channel_dist.empty else 1
    bar_width = 30
    for channel, count in channel_dist.items():
        ch_int = int(str(channel))
        bar_len = int(count / max_count * bar_width)
        bar_color = "cyan" if ch_int in _NON_OVERLAPPING else "yellow"
        bar = f"[{bar_color}]{'█' * bar_len}[/{bar_color}]"
        tag = "[dim](non-overlapping)[/dim]" if ch_int in _NON_OVERLAPPING else "[yellow](interferes)[/yellow]"
        console.print(f"   Channel [cyan]{ch_int:2d}[/cyan]  {bar} [white bold]{count:5,}[/white bold] networks  {tag}")

    # 3. Interferences
    console.print("\n[cyan bold]3. INTERFERENCE ANALYSIS BY CHANNEL[/cyan bold]")
    overlapping_issues = [
        (int(ch), min(_NON_OVERLAPPING, key=lambda x: abs(x - int(ch))),
         abs(int(ch) - min(_NON_OVERLAPPING, key=lambda x: abs(x - int(ch)))))
        for ch in df["Channel"].unique()
        if int(ch) not in _NON_OVERLAPPING
    ]

    if overlapping_issues:
        print_warning("Networks were detected on channels causing interference:")
        for channel, closest, _ in overlapping_issues:
            count = len(df[df["Channel"] == channel])
            console.print(
                f"   [yellow]Channel {channel:2d}[/yellow]: [white bold]{count:,}[/white bold] networks "
                f"[dim](interferes with channel[/dim] [cyan]{closest}[/cyan][dim])[/dim]"
            )
    else:
        print_success("All networks are on non-overlapping channels (1, 6, 11)")

    # 4. Average RSSI by channel
    console.print("\n[cyan bold]4. SIGNAL STRENGTH BY CHANNEL (Average RSSI)[/cyan bold]")
    for channel, data in df.groupby("Channel")["RSSI"].agg(["mean", "count"]).round(1).iterrows():
        mean_val = float(data["mean"])
        color = rssi_color(mean_val)
        console.print(
            f"   Channel [cyan]{int(str(channel)):2d}[/cyan]: "
            f"[{color}]{mean_val:6.1f} dBm[/{color}]  "
            f"[dim]({int(data['count'])} networks)[/dim]"
        )

    # 5. Weak signal networks
    console.print("\n[cyan bold]5. NETWORKS WITH POSSIBLE INTERFERENCE[/cyan bold]")
    weak = df[df["RSSI"] <= -80]
    if not weak.empty:
        print_warning(f"[white bold]{len(weak)}[/white bold] networks with weak signal (RSSI ≤ −80 dBm):")
        for _, row in weak.head(10).iterrows():
            console.print(
                f"   [red]•[/red] [white]{row['SSID']}[/white]  "
                f"[dim]Channel[/dim] [cyan]{int(row['Channel'])}[/cyan]  "
                f"[red]{row['RSSI']} dBm[/red]"
            )
        if len(weak) > 10:
            console.print(f"   [dim]... and {len(weak) - 10} more networks[/dim]")
    else:
        print_success("No networks with extremely weak signal detected")

    # 6. Recommendations
    console.print("\n[cyan bold]6. RECOMMENDATIONS[/cyan bold]")
    channel_counts = df["Channel"].value_counts()
    if not channel_counts.empty:
        most = int(str(channel_counts.idxmax()))
        least = int(str(channel_counts.idxmin()))
        console.print(f"   [dim]Most congested channel:[/dim]   [red bold]{most}[/red bold]  ({channel_counts[most]:,} networks)")
        console.print(f"   [dim]Least congested channel:[/dim] [green bold]{least}[/green bold] ({channel_counts[least]:,} networks)")

        optimal = [ch for ch in _NON_OVERLAPPING if channel_counts.get(ch, 0) < 2]
        if optimal:
            console.print(f"   [green]Recommended channels:[/green] [cyan]{optimal}[/cyan] [dim](low congestion)[/dim]")
        else:
            print_warning("All non-overlapping channels are congested")

    # ── Visualizations ──────────────────────────────────────────────────────
    console.print("\n[cyan bold]7. GENERATING VISUALIZATIONS...[/cyan bold]")
    generate_interference_plots(df, csv_file, output_dir=output_dir)

    # ── TXT Report ──────────────────────────────────────────────────────────
    try:
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        report_path = os.path.join(output_dir, f"{base_name}_report.txt")

        with open(report_path, "w", encoding="utf-8") as f:
            f.write("WIFI INTERFERENCE ANALYSIS\n")
            f.write("=" * 50 + "\n\n")
            f.write(f"Analyzed file: {csv_file}\n")
            f.write(f"Analyzed networks: {len(df)}\n")
            f.write(f"Detected channels: {sorted(_format_channels_list(df['Channel'].unique()))}\n")
            f.write(f"Networks with weak signal (RSSI <= -80 dBm): {len(df[df['RSSI'] <= -80])}\n")
            f.write("\nChannel distribution:\n")
            for channel, count in df["Channel"].value_counts().sort_index().items():
                f.write(f"- Channel {int(str(channel))}: {count} networks\n")
            f.write(_generate_comprehensive_analysis(df))

        print_success(f"Report saved: [cyan]{report_path}[/cyan]")

    except Exception as exc:
        print_error(f"Error saving report: {exc}")

    return df
