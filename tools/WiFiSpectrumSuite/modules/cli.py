"""
cli.py - Interfaz de línea de comandos para WiFi Spectrum Suite (Click)
"""

import logging
import os
import tempfile
import sys
import queue

# Internal
from .core.csv_debugger import repair_date_issues, validate_date_repair
from .core.interference import analyze_wifi_interference
#from .core.wardriving import WardrivingAnalyzer

# External
import click
from rich.logging import RichHandler
from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich import box
from rich.style import Style

# ---------------------------------------------------------------------------
# Estilos de consola
# ---------------------------------------------------------------------------

def _banner() -> None:
    click.echo("=" * 70)
    click.echo("WiFi SPECTRUM SUITE - Suite Completa de Análisis WiFi")
    click.echo("=" * 70)


def _phase_header(title: str) -> None:
    click.echo("\n" + "█" * 70)
    click.echo(title)
    click.echo("█" * 70)


# ---------------------------------------------------------------------------
# Grupo raíz
# ---------------------------------------------------------------------------

@click.group(
    context_settings={"help_option_names": ["-h", "--help"]},
    invoke_without_command=True,
)
@click.version_option("1.0.0", prog_name="wifi-spectrum-suite")
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
    "-o", "--output",
    default=None,
    show_default=True,
    help="Ruta del archivo CSV de salida. Por defecto: <archivo>_fixed.csv",
)
@click.option(
    "--validar", "-v",
    is_flag=True,
    default=False,
    help="Validar el archivo reparado tras la corrección.",
)
def cmd_debug(archivo: str, output: str, validar: bool) -> None:
    """
    Detecta y repara problemas de formato de fecha en ARCHIVO CSV.

    \b
    Ejemplo:
      wifi-spectrum-suite debug datos.csv -o datos_reparados.csv --validar
    """
    _banner()
    _phase_header("FASE 1: DEPURACIÓN DE PROBLEMAS DE FECHA")

    repaired_file = repair_date_issues(archivo, output)

    if repaired_file is None:
        click.secho("Error: no se pudo reparar el archivo.", fg="red", err=True)
        sys.exit(1)

    if validar:
        validate_date_repair(repaired_file)

    click.secho(f"\n✓ Archivo reparado: {repaired_file}", fg="green")


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
    _banner()
    _phase_header("FASE 2: ANÁLISIS DE INTERFERENCIAS WiFi")

    df = analyze_wifi_interference(archivo)

    if df is None:
        click.secho("Error: no se pudo analizar el archivo.", fg="red", err=True)
        sys.exit(1)

    click.secho("\n✓ Análisis de interferencias completado.", fg="green")


# ---------------------------------------------------------------------------
# Subcomando: wardriving
# ---------------------------------------------------------------------------

@cli.command("wardriving")
@click.argument("archivo", type=click.Path(exists=True, readable=True))
@click.option(
    "--mapa-calor/--sin-mapa-calor",
    default=False,
    show_default=True,
    help="Generar mapa de calor HTML.",
)
@click.option(
    "--mapa-loc/--sin-mapa-loc",
    default=False,
    show_default=True,
    help="Generar mapa de localización HTML.",
)
@click.option(
    "--graficos/--sin-graficos",
    default=False,
    show_default=True,
    help="Generar gráficos PNG avanzados.",
)
@click.option(
    "--reporte/--sin-reporte",
    default=False,
    show_default=True,
    help="Imprimir reporte detallado en consola.",
)
@click.option(
    "--todo", "-a",
    is_flag=True,
    default=False,
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
    _banner()
    _phase_header("FASE 3: ANÁLISIS DE WARDRIVING")

    analyzer = WardrivingAnalyzer(archivo)

    if not analyzer.cargar_datos():
        click.secho(
            "Error: no se pudieron cargar los datos. Verifica el archivo CSV.",
            fg="red",
            err=True,
        )
        sys.exit(1)

    click.echo("=" * 70)
    click.echo(f"ANÁLISIS WARDRIVING - {analyzer.nombre_base}")
    click.echo("=" * 70)

    archivos_generados = []

    if todo or reporte:
        analyzer.generar_reporte()

    if todo or mapa_calor:
        archivo_out = analyzer.generar_mapa_calor()
        if archivo_out:
            archivos_generados.append(archivo_out)

    if todo or mapa_loc:
        archivo_out = analyzer.generar_mapa_localizacion()
        if archivo_out:
            archivos_generados.append(archivo_out)

    if todo or graficos:
        archivo_out = analyzer.generar_graficos()
        if archivo_out:
            archivos_generados.append(archivo_out)

    if archivos_generados:
        click.echo("\nARCHIVOS GENERADOS (Wardriving):")
        for archivo_out in archivos_generados:
            click.secho(f"   ✓ {archivo_out}", fg="green")
    else:
        click.echo(
            "\nNingún artefacto generado. Usa --todo o indica al menos una opción."
        )


# ---------------------------------------------------------------------------
# Subcomando: full (pipeline completo)
# ---------------------------------------------------------------------------

@cli.command("full")
@click.argument("archivo", type=click.Path(exists=True, readable=True))
@click.option(
    "-o", "--output",
    default=None,
    help="Ruta del CSV reparado. Por defecto: <archivo>_fixed.csv",
)
@click.option(
    "--validar", "-v",
    is_flag=True,
    default=False,
    help="Validar fechas después de la reparación.",
)
def cmd_full(archivo: str, output: str, validar: bool) -> None:
    """
    Ejecuta el pipeline completo: debug → interference → wardriving.

    \b
    Ejemplo:
      wifi-spectrum-suite full datos.csv --validar
    """
    _banner()

    # ── Fase 1: Depuración ────────────────────────────────────────────
    _phase_header("FASE 1: DEPURACIÓN DE PROBLEMAS DE FECHA")
    repaired_file = repair_date_issues(archivo, output)
    analysis_file = repaired_file if repaired_file else archivo

    if repaired_file and validar:
        validate_date_repair(repaired_file)

    # ── Fase 2: Interferencias ────────────────────────────────────────
    _phase_header("FASE 2: ANÁLISIS DE INTERFERENCIAS WiFi")
    analyze_wifi_interference(analysis_file)

    # ── Fase 3: Wardriving ────────────────────────────────────────────
    _phase_header("FASE 3: ANÁLISIS DE WARDRIVING")
    analyzer = WardrivingAnalyzer(analysis_file)

    if not analyzer.cargar_datos():
        click.secho(
            "Error: no se pudieron cargar los datos de wardriving.",
            fg="red",
            err=True,
        )
        sys.exit(1)

    click.echo("=" * 70)
    click.echo(f"ANÁLISIS WARDRIVING - {analyzer.nombre_base}")
    click.echo("=" * 70)

    archivos_generados = []

    analyzer.generar_reporte()

    for gen_fn, label in [
        (analyzer.generar_mapa_calor, "mapa de calor"),
        (analyzer.generar_mapa_localizacion, "mapa de localización"),
        (analyzer.generar_graficos, "gráficos"),
    ]:
        archivo_out = gen_fn()
        if archivo_out:
            archivos_generados.append(archivo_out)

    # ── Resumen final ─────────────────────────────────────────────────
    click.echo("\n" + "=" * 70)
    click.secho("ANÁLISIS COMPLETADO EXITOSAMENTE", fg="green", bold=True)
    click.echo("=" * 70)
    click.echo("\nResumen de lo ejecutado:")
    click.secho("  ✓ Depuración de fechas", fg="green")
    click.secho("  ✓ Análisis de interferencias", fg="green")
    click.secho("  ✓ Análisis de wardriving", fg="green")

    if archivos_generados:
        click.echo("\nARCHIVOS GENERADOS:")
        for archivo_out in archivos_generados:
            click.secho(f"   ✓ {archivo_out}", fg="green")

    click.echo()


# ---------------------------------------------------------------------------
# Entry-point directo
# ---------------------------------------------------------------------------

def main() -> None:
    cli()


if __name__ == "__main__":
    main()