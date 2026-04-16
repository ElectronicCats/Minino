"""
interference.py - Análisis de interferencias WiFi por canal
"""

import os
from typing import Optional

import pandas as pd

from ..utils.file_utils import clean_and_validate_data, robust_csv_loader
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
                print("No se pudo cargar el archivo con ninguna estrategia")
                return None

            df = clean_and_validate_data(df)

            if df.empty:
                print("No hay datos válidos después de la limpieza")
                return None

            print(f"Archivo '{csv_file}' procesado correctamente")
            print(f"Filas válidas: {len(df)}")

        except FileNotFoundError:
            print(f"Error: No se encontró el archivo '{csv_file}'")
            return None
        except Exception as exc:
            print(f"Error al procesar el archivo: {exc}")
            return None

    df["Calidad"] = df["RSSI"].apply(_classify_signal)

    # ── Consola ──────────────────────────────────────────────────────────────
    print("\n" + "=" * 50)
    print("ANÁLISIS DE INTERFERENCIAS WiFi")
    print("=" * 50)

    print("\n1. RESUMEN GENERAL DE REDES DETECTADAS:")
    print(f"   - Total de redes detectadas: {len(df)}")
    print(f"   - Redes únicas por SSID: {df['SSID'].nunique()}")

    print("\n2. DISTRIBUCIÓN POR CANAL:")
    for channel, count in df["Channel"].value_counts().sort_index().items():
        print(f"   - Canal {int(str(channel))}: {count} redes")

    print("\n3. ANÁLISIS DE INTERFERENCIAS POR CANAL:")
    overlapping_issues = [
        (int(ch), min(_NON_OVERLAPPING, key=lambda x: abs(x - int(ch))), abs(int(ch) - min(_NON_OVERLAPPING, key=lambda x: abs(x - int(ch)))))
        for ch in df["Channel"].unique()
        if int(ch) not in _NON_OVERLAPPING
    ]

    if overlapping_issues:
        print("    Se detectaron redes en canales que causan interferencia:")
        for channel, closest, _ in overlapping_issues:
            count = len(df[df["Channel"] == channel])
            print(f"     - Canal {channel}: {count} redes (interfiere con canal {closest})")
    else:
        print("    Todas las redes están en canales no superpuestos (1, 6, 11)")

    print("\n4. INTENSIDAD DE SEÑAL POR CANAL (RSSI promedio):")
    for channel, data in df.groupby("Channel")["RSSI"].agg(["mean", "count"]).round(1).iterrows():
        print(f"   - Canal {int(str(channel))}: {data['mean']} dBm ({int(data['count'])} redes)")

    print("\n5. REDES CON POSIBLE INTERFERENCIA:")
    weak = df[df["RSSI"] <= -80]
    if not weak.empty:
        print(f"    Se detectaron {len(weak)} redes con señal débil (RSSI <= -80 dBm):")
        for _, row in weak.head(10).iterrows():
            print(f"     - {row['SSID']} (Canal {int(row['Channel'])}, RSSI: {row['RSSI']} dBm)")
        if len(weak) > 10:
            print(f"     ... y {len(weak) - 10} redes más")
    else:
        print("    No se detectaron redes con señal extremadamente débil")

    print("\n6. RECOMENDACIONES:")
    channel_counts = df["Channel"].value_counts()
    if not channel_counts.empty:
        most = int(str(channel_counts.idxmax()))
        least = int(str(channel_counts.idxmin()))
        print(f"   - Canal más congestionado:  {most}  ({channel_counts[most]} redes)")
        print(f"   - Canal menos congestionado: {least} ({channel_counts[least]} redes)")

        optimal = [ch for ch in _NON_OVERLAPPING if channel_counts.get(ch, 0) < 2]
        if optimal:
            print(f"   - Canales recomendados: {optimal} (poca congestión)")
        else:
            print("   - Todos los canales no superpuestos están congestionados")

    # ── Visualizaciones ──────────────────────────────────────────────────────
    print("\n7. GENERANDO VISUALIZACIONES...")
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

        print(f"    Reporte guardado como '{report_path}'")

    except Exception as exc:
        print(f"    Error al guardar reporte: {exc}")

    return df
