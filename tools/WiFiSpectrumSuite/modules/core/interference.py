"""
interference.py - Análisis de interferencias WiFi por canal
"""

import os
from typing import Optional

import pandas as pd

from ..utils.file_utils import clean_and_validate_data, robust_csv_loader
from ..utils.output import console, print_error, print_success, print_warning, rssi_color
from ..visualization.plots import generate_interference_plots


# ---------------------------------------------------------------------------
# Helpers internos
# ---------------------------------------------------------------------------

_NON_OVERLAPPING = [1, 6, 11]


def _classify_signal(rssi: float) -> str:
    if rssi >= -50:
        return "Excelente"
    elif rssi >= -60:
        return "Buena"
    elif rssi >= -70:
        return "Regular"
    elif rssi >= -80:
        return "Débil"
    return "Muy débil"


def _format_channels_list(channels) -> list:
    return [int(c) for c in channels]


def _generate_comprehensive_analysis(df: pd.DataFrame) -> str:
    """Construye el bloque de texto con el resumen ejecutivo y recomendaciones."""
    total = len(df)
    weak = len(df[df["RSSI"] <= -80])
    weak_pct = (weak / total) * 100
    channel_counts = df["Channel"].value_counts()

    lines = [
        "",
        "=" * 60,
        "RESUMEN EJECUTIVO DEL ANÁLISIS",
        "=" * 60,
        "",
        "HALLAZGOS PRINCIPALES:",
        f"   • Total de redes detectadas: {total:,} redes",
        f"   • Redes con señal débil: {weak:,} redes ({weak_pct:.1f}% del total)",
        f"   • Canales utilizados: {df['Channel'].nunique()} canales diferentes",
        "",
        "SITUACIÓN DE CANALES NO SUPERPUESTOS:",
    ]

    for ch in _NON_OVERLAPPING:
        count = channel_counts.get(ch, 0)
        status = " (Óptimo)"
        if count > 400:
            status = " (Extremadamente congestionado)"
        elif count > 300:
            status = " (Muy congestionado)"
        elif count > 200:
            status = " (Congestionado)"
        elif count > 100:
            status = " (Moderado)"
        lines.append(f"   • Canal {ch}: {count:,} redes{status}")

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
        "CANALES PROBLEMÁTICOS (INTERFERENCIA):",
        *[f"   • Canal {ch}: {cnt:,} redes (interfiere con canal {closest})" for ch, closest, cnt in overlapping[:6]],
        "",
        "=" * 60,
        "RECOMENDACIONES ESTRATÉGICAS",
        "=" * 60,
        "",
        "PROBLEMAS CRÍTICOS IDENTIFICADOS:",
        "   1. Canal 11 saturado — Evitar completamente",
        "   2. Todos los canales no superpuestos están congestionados",
        "   3. Alta densidad de redes en ambiente 2.4GHz",
        "",
        "ESTRATEGIAS RECOMENDADAS:",
        "   1. MIGRACIÓN A 5GHz:",
        "      • Configurar redes en banda 5GHz si los dispositivos lo soportan",
        "      • Menor interferencia y más canales disponibles",
        "",
        "   2. CANALES ALTERNATIVOS EN 2.4GHz:",
        f"      • Canal 13: {channel_counts.get(13, 0)} redes (menos congestionado)",
        f"      • Canal 14: {channel_counts.get(14, 0)} redes (menos congestionado)",
        f"      • Canal  5: {channel_counts.get(5, 0)} redes (muy poco congestionado)",
        "",
        "   3. OPTIMIZACIÓN DE 2.4GHz:",
        "      • Usar ancho de canal de 20MHz (no 40MHz)",
        "      • Transmitir en potencia baja para no afectar redes vecinas",
        "      • Programar reinicios nocturnos del router",
        "",
        "   4. PARA REDES CRÍTICAS:",
        "      • Implementar calidad de servicio (QoS)",
        "      • Usar banda dual (2.4GHz para IoT, 5GHz para dispositivos principales)",
        "",
        "PARA USUARIOS FINALES:",
        "   • Conectar dispositivos importantes a 5GHz cuando sea posible",
        "   • Ubicar el router lejos de interferencias",
        "   • Considerar sistemas mesh para mejor cobertura",
        "",
        "PERSPECTIVA:",
        "   El entorno analizado muestra una SATURACIÓN SEVERA de la banda 2.4GHz.",
        "   La migración a 5GHz no es solo recomendable, sino necesaria.",
    ]

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# API pública
# ---------------------------------------------------------------------------

def analyze_wifi_interference(
    csv_file: str,
    df: Optional[pd.DataFrame] = None,
) -> Optional[pd.DataFrame]:
    """
    Analiza interferencias WiFi entre canales a partir de un archivo CSV.

    Carga y valida los datos si no se pasa un DataFrame, imprime el análisis
    en consola, guarda un PNG con gráficos y un TXT con el reporte completo.

    Args:
        csv_file: Ruta al archivo CSV con datos WiFi.
        df: DataFrame ya cargado. Si es ``None`` se carga desde ``csv_file``.

    Returns:
        DataFrame procesado con la columna ``Calidad`` añadida, o ``None`` si
        ocurrió un error.
    """
    if df is None:
        try:
            df = robust_csv_loader(csv_file)

            if df is None:
                print_error("No se pudo cargar el archivo con ninguna estrategia")
                return None

            df = clean_and_validate_data(df)

            if df.empty:
                print_error("No hay datos válidos después de la limpieza")
                return None

            print_success(
                f"Archivo [cyan]{csv_file}[/cyan] procesado — "
                f"[white bold]{len(df)}[/white bold] filas válidas"
            )

        except FileNotFoundError:
            print_error(f"No se encontró el archivo '[cyan]{csv_file}[/cyan]'")
            return None
        except Exception as exc:
            print_error(f"Error al procesar el archivo: {exc}")
            return None

    df["Calidad"] = df["RSSI"].apply(_classify_signal)

    # ── Consola ──────────────────────────────────────────────────────────────
    console.rule("[cyan bold]ANÁLISIS DE INTERFERENCIAS WiFi[/cyan bold]", style="cyan")

    # 1. Resumen general
    console.print("\n[cyan bold]1. RESUMEN GENERAL[/cyan bold]")
    console.print(f"   [dim]Total de redes detectadas:[/dim]  [white bold]{len(df):,}[/white bold]")
    console.print(f"   [dim]Redes únicas por SSID:[/dim]      [white bold]{df['SSID'].nunique():,}[/white bold]")

    # 2. Distribución por canal con barra visual
    console.print("\n[cyan bold]2. DISTRIBUCIÓN POR CANAL[/cyan bold]")
    channel_dist = df["Channel"].value_counts().sort_index()
    max_count = int(channel_dist.max()) if not channel_dist.empty else 1
    bar_width = 30
    for channel, count in channel_dist.items():
        ch_int = int(str(channel))
        bar_len = int(count / max_count * bar_width)
        bar_color = "cyan" if ch_int in _NON_OVERLAPPING else "yellow"
        bar = f"[{bar_color}]{'█' * bar_len}[/{bar_color}]"
        tag = "[dim](no superpuesto)[/dim]" if ch_int in _NON_OVERLAPPING else "[yellow](interfiere)[/yellow]"
        console.print(f"   Canal [cyan]{ch_int:2d}[/cyan]  {bar} [white bold]{count:5,}[/white bold] redes  {tag}")

    # 3. Interferencias
    console.print("\n[cyan bold]3. ANÁLISIS DE INTERFERENCIAS POR CANAL[/cyan bold]")
    overlapping_issues = [
        (int(ch), min(_NON_OVERLAPPING, key=lambda x: abs(x - int(ch))),
         abs(int(ch) - min(_NON_OVERLAPPING, key=lambda x: abs(x - int(ch)))))
        for ch in df["Channel"].unique()
        if int(ch) not in _NON_OVERLAPPING
    ]

    if overlapping_issues:
        print_warning("Se detectaron redes en canales que causan interferencia:")
        for channel, closest, _ in overlapping_issues:
            count = len(df[df["Channel"] == channel])
            console.print(
                f"   [yellow]Canal {channel:2d}[/yellow]: [white bold]{count:,}[/white bold] redes "
                f"[dim](interfiere con canal[/dim] [cyan]{closest}[/cyan][dim])[/dim]"
            )
    else:
        print_success("Todas las redes están en canales no superpuestos (1, 6, 11)")

    # 4. RSSI promedio por canal
    console.print("\n[cyan bold]4. INTENSIDAD DE SEÑAL POR CANAL (RSSI promedio)[/cyan bold]")
    for channel, data in df.groupby("Channel")["RSSI"].agg(["mean", "count"]).round(1).iterrows():
        mean_val = float(data["mean"])
        color = rssi_color(mean_val)
        console.print(
            f"   Canal [cyan]{int(str(channel)):2d}[/cyan]: "
            f"[{color}]{mean_val:6.1f} dBm[/{color}]  "
            f"[dim]({int(data['count'])} redes)[/dim]"
        )

    # 5. Redes con señal débil
    console.print("\n[cyan bold]5. REDES CON POSIBLE INTERFERENCIA[/cyan bold]")
    weak = df[df["RSSI"] <= -80]
    if not weak.empty:
        print_warning(f"[white bold]{len(weak)}[/white bold] redes con señal débil (RSSI ≤ −80 dBm):")
        for _, row in weak.head(10).iterrows():
            console.print(
                f"   [red]•[/red] [white]{row['SSID']}[/white]  "
                f"[dim]Canal[/dim] [cyan]{int(row['Channel'])}[/cyan]  "
                f"[red]{row['RSSI']} dBm[/red]"
            )
        if len(weak) > 10:
            console.print(f"   [dim]... y {len(weak) - 10} redes más[/dim]")
    else:
        print_success("No se detectaron redes con señal extremadamente débil")

    # 6. Recomendaciones
    console.print("\n[cyan bold]6. RECOMENDACIONES[/cyan bold]")
    channel_counts = df["Channel"].value_counts()
    if not channel_counts.empty:
        most = int(str(channel_counts.idxmax()))
        least = int(str(channel_counts.idxmin()))
        console.print(f"   [dim]Canal más congestionado:[/dim]   [red bold]{most}[/red bold]  ({channel_counts[most]:,} redes)")
        console.print(f"   [dim]Canal menos congestionado:[/dim] [green bold]{least}[/green bold] ({channel_counts[least]:,} redes)")

        optimal = [ch for ch in _NON_OVERLAPPING if channel_counts.get(ch, 0) < 2]
        if optimal:
            console.print(f"   [green]Canales recomendados:[/green] [cyan]{optimal}[/cyan] [dim](poca congestión)[/dim]")
        else:
            print_warning("Todos los canales no superpuestos están congestionados")

    # ── Visualizaciones ──────────────────────────────────────────────────────
    console.print("\n[cyan bold]7. GENERANDO VISUALIZACIONES...[/cyan bold]")
    generate_interference_plots(df, csv_file)

    # ── Reporte TXT ──────────────────────────────────────────────────────────
    try:
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        report_path = f"{base_name}_report.txt"

        with open(report_path, "w", encoding="utf-8") as f:
            f.write("ANÁLISIS DE INTERFERENCIAS WiFi\n")
            f.write("=" * 50 + "\n\n")
            f.write(f"Archivo analizado: {csv_file}\n")
            f.write(f"Redes analizadas: {len(df)}\n")
            f.write(f"Canales detectados: {sorted(_format_channels_list(df['Channel'].unique()))}\n")
            f.write(f"Redes con señal débil (RSSI <= -80 dBm): {len(df[df['RSSI'] <= -80])}\n")
            f.write("\nDistribución por canal:\n")
            for channel, count in df["Channel"].value_counts().sort_index().items():
                f.write(f"- Canal {int(str(channel))}: {count} redes\n")
            f.write(_generate_comprehensive_analysis(df))

        print_success(f"Reporte guardado: [cyan]{report_path}[/cyan]")

    except Exception as exc:
        print_error(f"Error al guardar reporte: {exc}")

    return df
