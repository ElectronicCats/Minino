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
from pathlib import Path
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

import platform

# ---------------------------------------------------------------------------
# App metadata
# ---------------------------------------------------------------------------

VERSION_NUMBER = (Path(__file__).parent.parent / "VERSION").read_text(encoding="utf-8").strip()
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
    label = f"WiFi Spectrum Suite"

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
@click.version_option(VERSION_NUMBER, prog_name="wifi-spectrum")
@click.pass_context
def cli(ctx: click.Context) -> None:
    """
    WiFi Spectrum Suite — Complete WiFi analysis suite.

    Use ``wifi-spectrum <subcommand> --help`` for more information.
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
      wifi-spectrum debug data.csv -o data_fixed.csv --validate
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
      wifi-spectrum interference data.csv
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
      wifi-spectrum wardriving data.csv --all
      wifi-spectrum wardriving data.csv --heat-map --plots
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
      wifi-spectrum full data.csv --validate
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


# ===================== Shell Completion Commands =====================


@click.group(context_settings={"help_option_names": ["-h", "--help"]})
def completion():
    """Install shell tab completion for wifi-spectrum."""
    pass

@completion.command("install")
@click.option(
    "--shell",
    type=click.Choice(["bash", "zsh", "fish"]),
    default=None,
    help="Shell to install completion for (auto-detected if omitted)",
)
def completion_install(shell):
    """Install tab completion for your shell.

    Run this once, then restart your shell (or source your rc file).

    \b
        wifi-spectrum completion install          # auto-detect shell
        wifi-spectrum completion install --shell zsh
    """
    if platform.system() == "Windows":
        print_error("Shell completion is not supported on Windows.")
        sys.exit(1)

    import subprocess as _sp
    from pathlib import Path

    # Auto-detect shell
    if shell is None:
        shell_env = os.environ.get("SHELL", "")
        if "zsh" in shell_env:
            shell = "zsh"
        elif "fish" in shell_env:
            shell = "fish"
        elif "bash" in shell_env:
            shell = "bash"
        else:
            print_error("Could not detect shell. Use --shell bash|zsh|fish.")
            sys.exit(1)
        print_info(f"Detected shell: {shell}")

    env_var = "_WIFI_SPECTRUM_COMPLETE"

    # Absolute path to this script and the Python interpreter running it.
    # We always want completions to call "python /abs/path/to/wifi-spectrum.py" so
    # that they work regardless of whether wifi-spectrum is on PATH.
    script_abs = str(Path(sys.argv[0]).resolve())
    python_abs = sys.executable
    # The full command string that the completion script will execute
    cmd_to_call = f"{python_abs} {script_abs}"

    if shell == "bash":
        target = (
            Path.home()
            / ".local"
            / "share"
            / "bash-completion"
            / "completions"
            / "wifi-spectrum"
        )
        source_flag = "bash_source"
        rc_note = None
    elif shell == "zsh":
        target = Path.home() / ".zfunc" / "_wifi-spectrum"
        source_flag = "zsh_source"
        rc_note = "fpath=(~/.zfunc $fpath)\nautoload -Uz compinit && compinit"
    elif shell == "fish":
        target = Path.home() / ".config" / "fish" / "completions" / "wifi-spectrum.fish"
        source_flag = "fish_source"
        rc_note = None

    try:
        result = _sp.run(
            [python_abs, script_abs],
            env={**os.environ, env_var: source_flag},
            capture_output=True,
            text=True,
        )
        script = result.stdout
    except Exception as e:
        print_error(f"Failed to generate completion script: {e}")
        sys.exit(1)

    if not script.strip():
        print_error(
            "Empty completion script generated.\n"
            "Make sure you are running this command via:\n"
            f"  python {script_abs} completion install"
        )
        sys.exit(1)

    # ------------------------------------------------------------------ #
    # Post-process: replace the bare 'wifi-spectrum' program name that Click      #
    # embeds in the script with the full "python /abs/path/wifi-spectrum.py"      #
    # invocation.  We handle every pattern Click 7.x / 8.x can emit.      #
    # ------------------------------------------------------------------ #
    if shell == "zsh":
        # 1. #compdef directive — register for all the names a user might type
        script = script.replace(
            "#compdef wifi-spectrum", "#compdef wifi-spectrum wifi_spectrum.py ./wifi_spectrum.py"
        )
        # 2. The guard that aborts when the command is not found in $commands[].
        #    We neutralise it because we use an absolute path, not a PATH entry.
        script = script.replace(
            "(( ! $+commands[wifi-spectrum] ))",
            "false",  # 'false' evaluates to 1 so the (( )) block never returns
        )
        # 3. The line that actually calls the program to obtain completions.
        #    Click 8 emits:  _WIFISPECTRUMSUITE_COMPLETE=zsh_complete wifi-spectrum
        script = script.replace(
            f"{env_var}=zsh_complete wifi-spectrum", f"{env_var}=zsh_complete {cmd_to_call}"
        )
        # 4. The compdef registration at the bottom of the script
        script = script.replace(
            "compdef _wifi_spectrum_completion wifi-spectrum",
            f"compdef _wifi_spectrum_completion wifi-spectrum wifi_spectrum.py ./wifi_spectrum.py",
        )

        # 5. Append an explicit wrapper so that "python wifi_spectrum.py <TAB>" also
        #    triggers completion.
        extra = (
            "\n"
            "# Enable completion when invoked as 'python wifi_spectrum.py' or './wifi_spectrum.py'\n"
            "_wifi_spectrum_completion_python_wrapper() {\n"
            "  local script_name=${words[2]:t}  # basename of the script argument\n"
            "  if [[ $script_name == wifi_spectrum.py ]]; then\n"
            f"    (( ! $+functions[_wifi_spectrum_completion] )) && source {target}\n"
            '    words=(wifi-spectrum "${words[@]:2}")\n'
            "    (( CURRENT-- ))\n"
            "    _wifi_spectrum_completion\n"
            "  else\n"
            "    _files\n"
            "  fi\n"
            "}\n"
            "compdef _wifi_spectrum_completion_python_wrapper python python3\n"
        )
        script += extra
    elif shell == "bash":
        # Click 8 emits:  _wifi-spectrum_COMPLETE=bash_complete wifi-spectrum
        script = script.replace(
            f"{env_var}=bash_complete wifi-spectrum", f"{env_var}=bash_complete {cmd_to_call}"
        )
        # Register for both 'wifi-spectrum' and 'wifi-spectrum.py'
        script = script.replace(
            "complete -F _wifi_spectrum_completion wifi-spectrum",
            "complete -F _wifi_spectrum_completion wifi-spectrum wifi-spectrum.py",
        )
        # Append a wrapper that intercepts 'python wifi_spectrum.py <TAB>'
        extra = (
            "\n"
            "# Enable completion when invoked as 'python wifi_spectrum.py'\n"
            "_wifi_spectrum_completion_python_wrapper() {\n"
            "    local cur script_arg\n"
            '    cur="${COMP_WORDS[COMP_CWORD]}"\n'
            '    script_arg="${COMP_WORDS[1]}"\n'
            '    if [[ "$(basename "$script_arg")" == "wifi_spectrum.py" ]]; then\n'
            '        local new_words=(wifi-spectrum "${COMP_WORDS[@]:2}")\n'
            '        COMP_WORDS=("${new_words[@]}")\n'
            "        COMP_CWORD=$(( COMP_CWORD - 1 ))\n"
            "        _wifi_spectrum_completion\n"
            "    else\n"
            '        COMPREPLY=( $(compgen -f -- "$cur") )\n'
            "    fi\n"
            "}\n"
            "complete -F _wifi_spectrum_completion_python_wrapper python python3\n"
        )
        script += extra

    elif shell == "fish":
        # Fish uses a different mechanism; just replace the bare program name
        script = script.replace(
            f"{env_var}=fish_complete wifi-spectrum", f"{env_var}=fish_complete {cmd_to_call}"
        )

    # Write script
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(script)
    print_success(f"Completion script written to: {target}")

    # zsh needs fpath entry in .zshrc
    if rc_note:
        zshrc = Path.home() / ".zshrc"
        existing = zshrc.read_text() if zshrc.exists() else ""
        if "~/.zfunc" not in existing and ".zfunc" not in existing:
            with zshrc.open("a") as f:
                f.write(f"\n# wifi-spectrum tab completion\n{rc_note}\n")
            print_success(f"Added fpath entry to {zshrc}")
        else:
            console.print(
                "[dim]  ~/.zfunc already in fpath — skipping .zshrc edit[/dim]"
            )

    console.print("")
    if shell == "bash":
        console.print("Restart your shell or run:")
        console.print(f"  [green]source {target}[/green]")
    elif shell == "zsh":
        console.print("Restart your shell or run:")
        console.print("  [green]source ~/.zshrc && compinit -u[/green]")
    elif shell == "fish":
        console.print("Completion is active immediately in new fish sessions.")

# ---------------------------------------------------------------------------
# Entry-point
# ---------------------------------------------------------------------------

def main() -> None:
    if platform.system() in ["Linux", "Darwin"]:
        cli.add_command(completion)
    if not os.environ.get("_WIFI_SPECTRUM_COMPLETE"):
        module = next((a for a in sys.argv[1:] if not a.startswith("-")), None)
        print_header(module)
    cli(prog_name="wifi-spectrum")

if __name__ == "__main__":
    main()
