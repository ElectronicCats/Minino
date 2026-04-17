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
COMPANY = "Electronic Cats"

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
    label = f"wifi-spectrum-suite {module}" if module else "wifi-spectrum-suite"

    ascii_art = f"""      :-:              :--       |
      ++++=.        .=++++       |
      =+++++===++===++++++       |
      -++++++++++++++++++-       |
 .:   =++---++++++++---++=   :.  |  {label}
 ::---+++.   -++++-   .+++---::  |  v{VERSION_NUMBER}
::1..:-++++:   ++++   :++++-::.::|  {FUNNY_PHRASE}
.:...:=++++++++++++++++++=:...:. |
 :---.  -++++++++++++++-  .---:  |
 ..        .:------:.        ..  |"""

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
# Grupo raíz
# ---------------------------------------------------------------------------

@click.group(
    context_settings={"help_option_names": ["-h", "--help"]},
    invoke_without_command=True,
)
@click.version_option(VERSION_NUMBER, prog_name="wifi-spectrum-suite")
@click.pass_context
def cli(ctx: click.Context) -> None:
    """
    WiFi Spectrum Suite — Suite completa de análisis WiFi.

    \b
    Submódulos disponibles:
      debug        Depuración y reparación de fechas en CSV
      interference Análisis de interferencias por canal
      wardriving   Análisis de wardriving (mapas y gráficos)
      full         Ejecutar todos los análisis en secuencia

    Usa ``wifi-spectrum-suite <subcomando> --help`` para más información.
    """
    if ctx.invoked_subcommand is None:
        click.echo(ctx.get_help())


# ---------------------------------------------------------------------------
# Subcomando: debug
# ---------------------------------------------------------------------------

@cli.command("debug")
@click.argument("archivo", type=click.Path(exists=True, readable=True))
@click.option(
    "-o", "--output", default=None,
    help="Ruta del archivo CSV de salida. Por defecto: <archivo>_fixed.csv",
)
@click.option(
    "--validar", "-v", is_flag=True, default=False,
    help="Validar el archivo reparado tras la corrección.",
)
def cmd_debug(archivo: str, output: str, validar: bool) -> None:
    """
    Detecta y repara problemas de formato de fecha en ARCHIVO CSV.

    \b
    Ejemplo:
      wifi-spectrum-suite debug datos.csv -o datos_reparados.csv --validar
    """
    _phase_header("FASE 1: DEPURACIÓN DE PROBLEMAS DE FECHA")

    repaired_file = repair_date_issues(archivo, output)

    if repaired_file is None:
        print_error("No se pudo reparar el archivo.")
        sys.exit(1)

    if validar:
        validate_date_repair(repaired_file)

    print_success(f"Archivo reparado: {repaired_file}")


# ---------------------------------------------------------------------------
# Subcomando: interference
# ---------------------------------------------------------------------------

@cli.command("interference")
@click.argument("archivo", type=click.Path(exists=True, readable=True))
def cmd_interference(archivo: str) -> None:
    """
    Analiza interferencias WiFi entre canales en ARCHIVO CSV.

    Genera un reporte TXT y una imagen PNG con gráficos en el directorio
    de trabajo actual.

    \b
    Ejemplo:
      wifi-spectrum-suite interference datos.csv
    """
    _phase_header("FASE 2: ANÁLISIS DE INTERFERENCIAS WiFi")

    df = analyze_wifi_interference(archivo)

    if df is None:
        print_error("No se pudo analizar el archivo.")
        sys.exit(1)

    print_success("Análisis de interferencias completado.")


# ---------------------------------------------------------------------------
# Subcomando: wardriving
# ---------------------------------------------------------------------------

@cli.command("wardriving")
@click.argument("archivo", type=click.Path(exists=True, readable=True))
@click.option(
    "--mapa-calor/--sin-mapa-calor", default=False,
    help="Generar mapa de calor HTML.",
)
@click.option(
    "--mapa-loc/--sin-mapa-loc", default=False,
    help="Generar mapa de localización HTML.",
)
@click.option(
    "--graficos/--sin-graficos", default=False,
    help="Generar gráficos PNG avanzados.",
)
@click.option(
    "--reporte/--sin-reporte", default=False,
    help="Imprimir reporte detallado en consola.",
)
@click.option(
    "--todo", "-a", is_flag=True, default=False,
    help="Ejecutar todas las opciones de wardriving.",
)
def cmd_wardriving(
    archivo: str,
    mapa_calor: bool,
    mapa_loc: bool,
    graficos: bool,
    reporte: bool,
    todo: bool,
) -> None:
    """
    Ejecuta análisis de wardriving sobre ARCHIVO CSV.

    \b
    Ejemplos:
      wifi-spectrum-suite wardriving datos.csv --todo
      wifi-spectrum-suite wardriving datos.csv --mapa-calor --graficos
    """
    _phase_header("FASE 3: ANÁLISIS DE WARDRIVING")

    analyzer = WardrivingAnalyzer(archivo)

    if not analyzer.cargar_datos():
        print_error("No se pudieron cargar los datos. Verifica el archivo CSV.")
        sys.exit(1)

    console.rule(
        f"[cyan]ANÁLISIS WARDRIVING — {analyzer.nombre_base}[/cyan]",
        style="cyan dim",
    )

    archivos_generados = []

    if todo or reporte:
        analyzer.generar_reporte()
    if todo or mapa_calor:
        out = analyzer.generar_mapa_calor()
        if out:
            archivos_generados.append(out)
    if todo or mapa_loc:
        out = analyzer.generar_mapa_localizacion()
        if out:
            archivos_generados.append(out)
    if todo or graficos:
        out = analyzer.generar_graficos()
        if out:
            archivos_generados.append(out)

    if archivos_generados:
        console.print("\n[cyan bold]ARCHIVOS GENERADOS:[/cyan bold]")
        for out in archivos_generados:
            print_success(out)
    else:
        print_warning("Ningún artefacto generado. Usa --todo o indica al menos una opción.")


# ---------------------------------------------------------------------------
# Subcomando: full (pipeline completo)
# ---------------------------------------------------------------------------

@cli.command("full")
@click.argument("archivo", type=click.Path(exists=True, readable=True))
@click.option(
    "-o", "--output", default=None,
    help="Ruta del CSV reparado. Por defecto: <archivo>_fixed.csv",
)
@click.option(
    "--validar", "-v", is_flag=True, default=False,
    help="Validar fechas después de la reparación.",
)
def cmd_full(archivo: str, output: str, validar: bool) -> None:
    """
    Ejecuta el pipeline completo: debug → interference → wardriving.

    \b
    Ejemplo:
      wifi-spectrum-suite full datos.csv --validar
    """
    # ── Fase 1 ────────────────────────────────────────────────────────
    _phase_header("FASE 1: DEPURACIÓN DE PROBLEMAS DE FECHA")
    repaired_file = repair_date_issues(archivo, output)
    analysis_file = repaired_file if repaired_file else archivo

    if repaired_file and validar:
        validate_date_repair(repaired_file)

    # ── Fase 2 ────────────────────────────────────────────────────────
    _phase_header("FASE 2: ANÁLISIS DE INTERFERENCIAS WiFi")
    analyze_wifi_interference(analysis_file)

    # ── Fase 3 ────────────────────────────────────────────────────────
    _phase_header("FASE 3: ANÁLISIS DE WARDRIVING")
    analyzer = WardrivingAnalyzer(analysis_file)

    if not analyzer.cargar_datos():
        print_error("No se pudieron cargar los datos de wardriving.")
        sys.exit(1)

    console.rule(
        f"[cyan]ANÁLISIS WARDRIVING — {analyzer.nombre_base}[/cyan]",
        style="cyan dim",
    )

    archivos_generados = []
    analyzer.generar_reporte()

    for gen_fn in (
        analyzer.generar_mapa_calor,
        analyzer.generar_mapa_localizacion,
        analyzer.generar_graficos,
    ):
        out = gen_fn()
        if out:
            archivos_generados.append(out)

    # ── Resumen final ─────────────────────────────────────────────────
    console.rule("[cyan bold]ANÁLISIS COMPLETADO EXITOSAMENTE[/cyan bold]", style="cyan")
    print_success("Depuración de fechas")
    print_success("Análisis de interferencias")
    print_success("Análisis de wardriving")

    if archivos_generados:
        console.print("\n[cyan bold]ARCHIVOS GENERADOS:[/cyan bold]")
        for out in archivos_generados:
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
