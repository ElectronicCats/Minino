"""
file_utils.py - Robust loading of CSV files with WiFi data
"""

import numpy as np
import pandas as pd
from typing import Optional

from .output import console, print_error, print_success, print_warning


def robust_csv_loader(csv_file: str) -> Optional[pd.DataFrame]:
    """
    Loads a CSV trying multiple strategies for inconsistent files.

    Returns:
        Loaded DataFrame, or ``None`` if all strategies failed.
    """
    console.print(f"\n[cyan]Loading CSV file:[/cyan] [white bold]{csv_file}[/white bold]")

    strategies = [
        lambda: pd.read_csv(csv_file, skiprows=1),
        lambda: pd.read_csv(csv_file, skiprows=1, on_bad_lines="skip", engine="python"),
        lambda: pd.read_csv(csv_file, skiprows=1, dtype=str, on_bad_lines="skip"),
        lambda: pd.read_csv(csv_file, skiprows=1, sep=None, engine="python"),
    ]

    for i, strategy in enumerate(strategies, start=1):
        try:
            console.print(f"  [dim]Trying strategy {i}...[/dim]")
            df = strategy()
            print_success(f"Strategy {i} successful — [white bold]{len(df)}[/white bold] rows loaded")
            return df
        except Exception as exc:
            console.print(f"  [dim red]Strategy {i} failed:[/dim red] [dim]{exc}[/dim]")

    console.print("  [yellow]Attempting manual line-by-line loading...[/yellow]")
    try:
        return _manual_csv_loader(csv_file)
    except Exception as exc:
        print_error(f"Manual loading failed: {exc}")

    return None


def _manual_csv_loader(csv_file: str) -> pd.DataFrame:
    """Loads the CSV line by line for maximum fault tolerance."""
    with open(csv_file, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    if len(lines) < 2:
        raise ValueError("File is too short")

    headers = lines[1].strip().split(",")
    expected_columns = len(headers)

    console.print(f"  [dim]Headers detected:[/dim] [cyan]{expected_columns}[/cyan] columns")
    console.print(f"  [dim]Total lines:[/dim] [white bold]{len(lines)}[/white bold]")

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
        print_warning(f"Problematic lines corrected: [white bold]{problematic_lines}[/white bold]")

    return pd.DataFrame(data, columns=headers)


def clean_and_validate_data(df: pd.DataFrame) -> pd.DataFrame:
    """
    Cleans and validates the DataFrame data.

    Converts RSSI and Channel to numeric and removes rows without critical data.
    """
    console.print("\n[cyan]Cleaning and validating data...[/cyan]")
    console.print(
        f"  [dim]Initial shape:[/dim] "
        f"[white bold]{df.shape[0]}[/white bold] rows, "
        f"[white bold]{df.shape[1]}[/white bold] columns"
    )

    critical_columns = ["SSID", "RSSI", "Channel"]
    missing = [c for c in critical_columns if c not in df.columns]
    if missing:
        print_warning(f"Missing columns: [white]{missing}[/white]")
        console.print(f"  [dim]Available columns:[/dim] [dim]{list(df.columns)}[/dim]")

    for col in ("RSSI", "Channel"):
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
            nulls = df[col].isna().sum()
            if nulls:
                print_warning(f"Non-numeric [cyan]{col}[/cyan] values removed: [white bold]{nulls}[/white bold]")

    initial = len(df)
    df = df.dropna(subset=["RSSI", "Channel"])
    removed = initial - len(df)
    if removed:
        print_warning(f"Rows without critical data removed: [white bold]{removed}[/white bold]")

    console.print(
        f"  [dim]Final shape:[/dim] "
        f"[green bold]{df.shape[0]}[/green bold] rows, "
        f"[white bold]{df.shape[1]}[/white bold] columns"
    )
    return df
