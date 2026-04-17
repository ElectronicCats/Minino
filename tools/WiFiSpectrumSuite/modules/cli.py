#!/usr/bin/env python3

# Electronic Cats
# Original Creation Date: Mar 17, 2026
# This code is beerware; if you see me (or any other Electronic Cats
# member) at the local, and you've found our code helpful,
# please buy us a round!
# Distributed as-is; no warranty is given.

import logging
import os
import sys
import random as _random
from typing import Optional

# Internal
from .core.csv_debugger import repair_date_issues, validate_date_repair
from .core.interference import analyze_wifi_interference
from .core.wardriving import WardrivingAnalyzer

# External
import click
from rich.logging import RichHandler
from rich.panel import Panel

from .utils.output import (
    STYLES,
    console,
    print_error,
    print_info,
    print_success,
    print_warning,
)

# ---------------------------------------------------------------------------
# App metadata
# ---------------------------------------------------------------------------

VERSION_NUMBER = "1.1.0.0"
COMPANY = "Electronic Cats & Dr. h. c. César A. Peregrino Rodríguez"
OUTPUT_DIR = "generatedFiles"

_FUNNY_PHRASES = [
    "Your WiFi is an open book.",
    "Signals don't lie. People do.",
    "2.4GHz: where chaos lives.",
    "Channel 6 is always crowded.",
    "Making invisible waves visible.",
    "Your neighbor's router has secrets.",
    "RSSI below -80? That's not WiFi, that's hope.",
    "5GHz: the road less traveled.",
    "802.11: because cables are so last century.",
    "Legally (probably) sniffing since 2024.",
    "WPA2 is not a suggestion.",
    "Detecting your smart fridge since 2024.",
    "The spectrum never lies.",
    "Channel 11 is always a mess.",
    "If it broadcasts, we see it.",
    "SSID hidden ≠ SSID safe.",
    "Not all heroes wear capes. Some carry antennas.",
    "Open networks are just public confessions.",
    "Your IoT device is talking. We're listening.",
    "The air is full of data. Help yourself.",
]

FUNNY_PHRASE = _random.choice(_FUNNY_PHRASES)

logger = logging.getLogger("rich")
logging.basicConfig(
    level="WARNING",
    format="%(message)s",
    datefmt="[%X]",
    handlers=[RichHandler(markup=True)],
)

# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------

def print_header(module: Optional[str] = None) -> None:
    """Print the ASCII art header inside a Rich Panel."""
    label = f"wifi-spectrum-suite {module}" if module else "WiFi Spectrum Suite"

    ascii_art = f"""      :=--             --=-       |   
      -====-         -=====       |
      :===================-       |      
       ===================:       |      
  -   :==--===========--==-   -   |  {label} 
 -===:===-   :=====-   -==-.-=--  |  v{VERSION_NUMBER}
--    ====-   :===-   -====    -- |  {FUNNY_PHRASE}
-=:   :===================-   .=- |
 ---=-- -===============-  -=---  |
 ---       --=======--        --  |"""

    panel = Panel(
        f"[cyan bold]{ascii_art}[/cyan bold]",
        title=f"[cyan]{COMPANY}[/cyan]",
        border_style=STYLES["header"],
        title_align="left",
        padding=(1, 2),
    )
    console.print(panel)


def _phase_header(title: str) -> None:
    console.rule(f"[cyan bold]{title}[/cyan bold]", style="cyan")


# ---------------------------------------------------------------------------
# Root group
# ---------------------------------------------------------------------------

@click.group(
    context_settings={"help_option_names": ["-h", "--help"]},
    invoke_without_command=True,
)
@click.version_option(VERSION_NUMBER, prog_name="wifi-spectrum-suite")
@click.pass_context
def cli(ctx: click.Context) -> None:
    """
    WiFi Spectrum Suite — Complete WiFi analysis suite.

    \b
    Available submodules:
      debug        Debug and repair dates in CSV
      interference Channel interference analysis
      wardriving   Wardriving analysis (maps and plots)
      full         Execute all analyses sequentially

    Use ``wifi-spectrum-suite <subcommand> --help`` for more information.
    """
    if ctx.invoked_subcommand is None:
        click.echo(ctx.get_help())


# ---------------------------------------------------------------------------
# Subcommand: debug
# ---------------------------------------------------------------------------

@cli.command("debug")
@click.argument("csv_file", type=click.Path(exists=True, readable=True))
@click.option(
    "-o", "--output", default=None,
    help="Output CSV file path. Default: <csv_file>_fixed.csv",
)
@click.option(
    "--validate", "-v", is_flag=True, default=False,
    help="Validate the repaired file after correction.",
)
def cmd_debug(csv_file: str, output: str, validate: bool) -> None:
    """
    Detects and repairs date format issues in a CSV FILE.

    \b
    Example:
      wifi-spectrum-suite debug data.csv -o data_fixed.csv --validate
    """
    _phase_header("PHASE 1: DATE ISSUES DEBUGGING")

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    repaired_file = repair_date_issues(csv_file, output, output_dir=OUTPUT_DIR)

    if repaired_file is None:
        print_error("Could not repair the file.")
        sys.exit(1)

    if validate:
        validate_date_repair(repaired_file)

    print_success(f"File repaired: {repaired_file}")


# ---------------------------------------------------------------------------
# Subcommand: interference
# ---------------------------------------------------------------------------

@cli.command("interference")
@click.argument("csv_file", type=click.Path(exists=True, readable=True))
def cmd_interference(csv_file: str) -> None:
    """
    Analyzes WiFi channel interference in a CSV FILE.

    Generates a TXT report and a PNG image with plots in the current working directory.

    \b
    Example:
      wifi-spectrum-suite interference data.csv
    """
    _phase_header("PHASE 2: WIFI INTERFERENCE ANALYSIS")

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    df = analyze_wifi_interference(csv_file, output_dir=OUTPUT_DIR)

    if df is None:
        print_error("Could not analyze the file.")
        sys.exit(1)

    print_success("Interference analysis completed.")


# ---------------------------------------------------------------------------
# Subcommand: wardriving
# ---------------------------------------------------------------------------

@cli.command("wardriving")
@click.argument("csv_file", type=click.Path(exists=True, readable=True))
@click.option(
    "--heat-map/--no-heat-map", default=False,
    help="Generate HTML heatmap.",
)
@click.option(
    "--location-map/--no-location-map", default=False,
    help="Generate HTML location map.",
)
@click.option(
    "--plots/--no-plots", default=False,
    help="Generate advanced PNG plots.",
)
@click.option(
    "--report/--no-report", default=False,
    help="Print detailed report to console.",
)
@click.option(
    "--all", "-a", "run_all", is_flag=True, default=False,
    help="Execute all wardriving options.",
)
def cmd_wardriving(
    csv_file: str,
    heat_map: bool,
    location_map: bool,
    plots: bool,
    report: bool,
    run_all: bool,
) -> None:
    """
    Executes wardriving analysis on a CSV FILE.

    \b
    Examples:
      wifi-spectrum-suite wardriving data.csv --all
      wifi-spectrum-suite wardriving data.csv --heat-map --plots
    """
    _phase_header("PHASE 3: WARDRIVING ANALYSIS")

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    analyzer = WardrivingAnalyzer(csv_file, output_dir=OUTPUT_DIR)

    if not analyzer.load_data():
        print_error("Could not load data. Check the CSV file.")
        sys.exit(1)

    console.rule(
        f"[cyan]WARDRIVING ANALYSIS — {analyzer.base_name}[/cyan]",
        style="cyan dim",
    )

    generated_files = []

    if run_all or report:
        analyzer.generate_report()
    if run_all or heat_map:
        out = analyzer.generate_heat_map()
        if out:
            generated_files.append(out)
    if run_all or location_map:
        out = analyzer.generate_location_map()
        if out:
            generated_files.append(out)
    if run_all or plots:
        out = analyzer.generate_plots()
        if out:
            generated_files.append(out)

    if generated_files:
        console.print("\n[cyan bold]GENERATED FILES:[/cyan bold]")
        for out in generated_files:
            print_success(out)
    else:
        print_warning("No artifacts generated. Use --all or indicate at least one option.")


# ---------------------------------------------------------------------------
# Subcommand: full (complete pipeline)
# ---------------------------------------------------------------------------

@cli.command("full")
@click.argument("csv_file", type=click.Path(exists=True, readable=True))
@click.option(
    "-o", "--output", default=None,
    help="Repaired CSV path. Default: <csv_file>_fixed.csv",
)
@click.option(
    "--validate", "-v", is_flag=True, default=False,
    help="Validate dates after repair.",
)
def cmd_full(csv_file: str, output: str, validate: bool) -> None:
    """
    Executes the complete pipeline: debug → interference → wardriving.

    \b
    Example:
      wifi-spectrum-suite full data.csv --validate
    """
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # ── Phase 1 ────────────────────────────────────────────────────────
    _phase_header("PHASE 1: DATE ISSUES DEBUGGING")
    repaired_file = repair_date_issues(csv_file, output, output_dir=OUTPUT_DIR)
    analysis_file = repaired_file if repaired_file else csv_file

    if repaired_file and validate:
        validate_date_repair(repaired_file)

    # ── Phase 2 ────────────────────────────────────────────────────────
    _phase_header("PHASE 2: WIFI INTERFERENCE ANALYSIS")
    analyze_wifi_interference(analysis_file, output_dir=OUTPUT_DIR)

    # ── Phase 3 ────────────────────────────────────────────────────────
    _phase_header("PHASE 3: WARDRIVING ANALYSIS")
    analyzer = WardrivingAnalyzer(analysis_file, output_dir=OUTPUT_DIR)

    if not analyzer.load_data():
        print_error("Could not load wardriving data.")
        sys.exit(1)

    console.rule(
        f"[cyan]WARDRIVING ANALYSIS — {analyzer.base_name}[/cyan]",
        style="cyan dim",
    )

    generated_files = []
    analyzer.generate_report()

    for gen_fn in (
        analyzer.generate_heat_map,
        analyzer.generate_location_map,
        analyzer.generate_plots,
    ):
        out = gen_fn()
        if out:
            generated_files.append(out)

    # ── Final summary ─────────────────────────────────────────────────
    console.rule("[cyan bold]ANALYSIS SUCCESSFULLY COMPLETED[/cyan bold]", style="cyan")
    print_success("Date debugging")
    print_success("Interference analysis")
    print_success("Wardriving analysis")

    if generated_files:
        console.print("\n[cyan bold]GENERATED FILES:[/cyan bold]")
        for out in generated_files:
            print_success(out)

    console.print()


# ---------------------------------------------------------------------------
# Entry-point
# ---------------------------------------------------------------------------

def main() -> None:
    if not os.environ.get("_WIFI_SPECTRUM_COMPLETE"):
        module = next((a for a in sys.argv[1:] if not a.startswith("-")), None)
        print_header(module)
    cli(prog_name="wifi-spectrum-suite")


if __name__ == "__main__":
    main()
