"""
file_utils.py - Carga robusta de archivos CSV con datos WiFi
"""

import numpy as np
import pandas as pd
from typing import Optional


def robust_csv_loader(csv_file: str) -> Optional[pd.DataFrame]:
    """
    Carga un CSV probando múltiples estrategias ante archivos inconsistentes.

    Returns:
        DataFrame cargado, o ``None`` si todas las estrategias fallaron.
    """
    print(f"Cargando archivo CSV: {csv_file}")

    strategies = [
        lambda: pd.read_csv(csv_file, skiprows=1),
        lambda: pd.read_csv(csv_file, skiprows=1, on_bad_lines="skip", engine="python"),
        lambda: pd.read_csv(csv_file, skiprows=1, dtype=str, on_bad_lines="skip"),
        lambda: pd.read_csv(csv_file, skiprows=1, sep=None, engine="python"),
    ]

    for i, strategy in enumerate(strategies, start=1):
        try:
            print(f"    Intentando estrategia {i}...")
            df = strategy()
            print(f"    Estrategia {i} exitosa — {len(df)} filas cargadas")
            return df
        except Exception as exc:
            print(f"    Estrategia {i} falló: {exc}")

    print("    Intentando carga manual línea por línea...")
    try:
        return _manual_csv_loader(csv_file)
    except Exception as exc:
        print(f"    Carga manual falló: {exc}")

    return None


def _manual_csv_loader(csv_file: str) -> pd.DataFrame:
    """Carga el CSV línea a línea para máxima tolerancia a errores."""
    with open(csv_file, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    if len(lines) < 2:
        raise ValueError("Archivo demasiado corto")

    headers = lines[1].strip().split(",")
    expected_columns = len(headers)

    print(f"   Encabezados detectados: {expected_columns} columnas")
    print(f"   Líneas totales: {len(lines)}")

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
        print(f"     Líneas problemáticas corregidas: {problematic_lines}")

    return pd.DataFrame(data, columns=headers)


def clean_and_validate_data(df: pd.DataFrame) -> pd.DataFrame:
    """
    Limpia y valida los datos del DataFrame.

    Convierte RSSI y Channel a numérico y elimina filas sin datos críticos.
    """
    print("Limpiando y validando datos...")
    print(f"    Forma inicial: {df.shape[0]} filas, {df.shape[1]} columnas")

    critical_columns = ["SSID", "RSSI", "Channel"]
    missing = [c for c in critical_columns if c not in df.columns]
    if missing:
        print(f"     Columnas faltantes: {missing}")
        print(f"    Columnas disponibles: {list(df.columns)}")

    for col in ("RSSI", "Channel"):
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
            nulls = df[col].isna().sum()
            if nulls:
                print(f"     Valores {col} no numéricos eliminados: {nulls}")

    initial = len(df)
    df = df.dropna(subset=["RSSI", "Channel"])
    removed = initial - len(df)
    if removed:
        print(f"   Filas sin datos críticos eliminadas: {removed}")

    print(f"    Forma final: {df.shape[0]} filas, {df.shape[1]} columnas")
    return df
