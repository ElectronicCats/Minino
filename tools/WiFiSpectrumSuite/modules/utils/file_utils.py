"""
file_utils.py - Carga robusta de archivos CSV con datos WiFi
"""

import numpy as np
import pandas as pd
from typing import Optional

from .output import console, print_error, print_success, print_warning


def robust_csv_loader(csv_file: str) -> Optional[pd.DataFrame]:
    """
    Carga un CSV probando múltiples estrategias ante archivos inconsistentes.

    Returns:
        DataFrame cargado, o ``None`` si todas las estrategias fallaron.
    """
    console.print(f"\n[cyan]Cargando archivo CSV:[/cyan] [white bold]{csv_file}[/white bold]")

    strategies = [
        lambda: pd.read_csv(csv_file, skiprows=1),
        lambda: pd.read_csv(csv_file, skiprows=1, on_bad_lines="skip", engine="python"),
        lambda: pd.read_csv(csv_file, skiprows=1, dtype=str, on_bad_lines="skip"),
        lambda: pd.read_csv(csv_file, skiprows=1, sep=None, engine="python"),
    ]

    for i, strategy in enumerate(strategies, start=1):
        try:
            console.print(f"  [dim]Intentando estrategia {i}...[/dim]")
            df = strategy()
            print_success(f"Estrategia {i} exitosa — [white bold]{len(df)}[/white bold] filas cargadas")
            return df
        except Exception as exc:
            console.print(f"  [dim red]Estrategia {i} falló:[/dim red] [dim]{exc}[/dim]")

    console.print("  [yellow]Intentando carga manual línea por línea...[/yellow]")
    try:
        return _manual_csv_loader(csv_file)
    except Exception as exc:
        print_error(f"Carga manual falló: {exc}")

    return None


def _manual_csv_loader(csv_file: str) -> pd.DataFrame:
    """Carga el CSV línea a línea para máxima tolerancia a errores."""
    with open(csv_file, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    if len(lines) < 2:
        raise ValueError("Archivo demasiado corto")

    headers = lines[1].strip().split(",")
    expected_columns = len(headers)

    console.print(f"  [dim]Encabezados detectados:[/dim] [cyan]{expected_columns}[/cyan] columnas")
    console.print(f"  [dim]Líneas totales:[/dim] [white bold]{len(lines)}[/white bold]")

    data = []
    problematic_lines = 0

    for line in lines[2:]:
        fields = line.strip().split(",")
        n = len(fields)
        if n == expected_columns:
            data.append(fields)
        elif n > expected_columns:
            data.append(fields[:expected_columns])
            problematic_lines += 1
        else:
            data.append(fields + [np.nan] * (expected_columns - n))
            problematic_lines += 1

    if problematic_lines:
        print_warning(f"Líneas problemáticas corregidas: [white bold]{problematic_lines}[/white bold]")

    return pd.DataFrame(data, columns=headers)


def clean_and_validate_data(df: pd.DataFrame) -> pd.DataFrame:
    """
    Limpia y valida los datos del DataFrame.

    Convierte RSSI y Channel a numérico y elimina filas sin datos críticos.
    """
    console.print("\n[cyan]Limpiando y validando datos...[/cyan]")
    console.print(
        f"  [dim]Forma inicial:[/dim] "
        f"[white bold]{df.shape[0]}[/white bold] filas, "
        f"[white bold]{df.shape[1]}[/white bold] columnas"
    )

    critical_columns = ["SSID", "RSSI", "Channel"]
    missing = [c for c in critical_columns if c not in df.columns]
    if missing:
        print_warning(f"Columnas faltantes: [white]{missing}[/white]")
        console.print(f"  [dim]Columnas disponibles:[/dim] [dim]{list(df.columns)}[/dim]")

    for col in ("RSSI", "Channel"):
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
            nulls = df[col].isna().sum()
            if nulls:
                print_warning(f"Valores [cyan]{col}[/cyan] no numéricos eliminados: [white bold]{nulls}[/white bold]")

    initial = len(df)
    df = df.dropna(subset=["RSSI", "Channel"])
    removed = initial - len(df)
    if removed:
        print_warning(f"Filas sin datos críticos eliminadas: [white bold]{removed}[/white bold]")

    console.print(
        f"  [dim]Forma final:[/dim] "
        f"[green bold]{df.shape[0]}[/green bold] filas, "
        f"[white bold]{df.shape[1]}[/white bold] columnas"
    )
    return df
