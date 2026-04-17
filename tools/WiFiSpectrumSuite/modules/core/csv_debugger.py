"""
csv_debugger.py - Detección y reparación de problemas de fecha en archivos CSV
"""

import os
import csv
import io

import pandas as pd
from typing import List, Optional, Tuple

from ..utils.validators import looks_like_date, repair_date_field
from ..utils.output import console, print_error, print_success, print_warning


# ---------------------------------------------------------------------------
# Helpers internos
# ---------------------------------------------------------------------------

def _split_csv_line(line: str) -> List[str]:
    """Divide una línea CSV respetando los campos entre comillas."""
    if not line.strip():
        return []
    return next(csv.reader([line]))

def _join_csv_line(fields: List[str]) -> str:
    """Une campos en una línea CSV escapando comillas si es necesario."""
    output = io.StringIO()
    writer = csv.writer(output, lineterminator="")
    writer.writerow(fields)
    return output.getvalue()

def _find_date_columns(headers: List[str]) -> List[Tuple[int, str]]:
    keywords = {"TIME", "DATE", "SEEN", "FIRST", "LAST"}
    return [
        (i, header)
        for i, header in enumerate(headers)
        if any(kw in header.upper() for kw in keywords)
    ]


# ---------------------------------------------------------------------------
# API pública
# ---------------------------------------------------------------------------

def analyze_date_problems(
    csv_file: str,
) -> Tuple[Optional[List[str]], List[dict], List[Tuple[int, str]]]:
    """
    Analiza problemas con formatos de fecha en el archivo CSV.

    Returns:
        Tupla ``(headers, problematic_lines, date_columns)``.
    """
    console.rule(f"[cyan bold]ANALIZANDO PROBLEMAS DE FECHA — {csv_file}[/cyan bold]", style="cyan")

    try:
        with open(csv_file, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()

        console.print(f"  [dim]Total de líneas en el archivo:[/dim] [white bold]{len(lines)}[/white bold]")

        if len(lines) < 2:
            print_error("El archivo está vacío o tiene muy pocas líneas")
            return None, [], []

        console.print("\n[cyan bold]ESTRUCTURA DEL ARCHIVO[/cyan bold]")
        headers = _split_csv_line(lines[1].strip())
        console.print(f"  [dim]Encabezados detectados:[/dim] [white bold]{len(headers)}[/white bold]  [dim]{headers}[/dim]")

        date_columns = _find_date_columns(headers)
        console.print(f"  [dim]Columnas potencialmente de fecha:[/dim] [cyan]{date_columns}[/cyan]")

        date_samples: dict = {}
        problematic_lines: List[dict] = []

        for line_num, line in enumerate(lines[2:12], start=3):
            fields = _split_csv_line(line.strip())
            for col_idx, col_name in date_columns:
                if col_idx < len(fields):
                    value = fields[col_idx]
                    date_samples.setdefault(col_name, []).append(value)

                    if not looks_like_date(value):
                        problematic_lines.append(
                            {
                                "line": line_num,
                                "column": col_name,
                                "value": value,
                                "reason": "No parece fecha",
                            }
                        )

        console.print("\n[cyan bold]MUESTRAS DE FECHAS[/cyan bold]")
        for col_name, samples in date_samples.items():
            console.print(f"  [cyan]{col_name}:[/cyan] [dim]{samples}[/dim]")

        if problematic_lines:
            print_warning(f"[white bold]{len(problematic_lines)}[/white bold] valores problemáticos detectados")
        else:
            print_success("No se detectaron valores problemáticos en las primeras 10 líneas")

        return headers, problematic_lines, date_columns

    except Exception as exc:
        print_error(f"Error durante el análisis: {exc}")
        return None, [], []


def repair_date_issues(
    csv_file: str,
    output_file: Optional[str] = None,
    output_dir: str = ".",
) -> Optional[str]:
    """
    Repara problemas de formato de fecha en el archivo CSV.

    Returns:
        Ruta al archivo reparado, o ``None`` si ocurrió un error.
    """
    if output_file is None:
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        output_file = os.path.join(output_dir, f"{base_name}_fixed.csv")

    console.rule("[cyan bold]REPARANDO PROBLEMAS DE FECHA[/cyan bold]", style="cyan")

    headers, _, date_columns = analyze_date_problems(csv_file)

    if not headers:
        print_error("No se puede proceder con la reparación")
        return None

    try:
        with open(csv_file, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()

        repaired_lines: List[str] = []
        corrections_made = 0
        date_column_indices = [idx for idx, _ in date_columns]

        console.print("\n[cyan bold]APLICANDO CORRECCIONES DE FECHA...[/cyan bold]")

        for line_num, line in enumerate(lines):
            original_line = line.strip()

            if line_num < 2:
                repaired_lines.append(original_line)
                continue

            fields = _split_csv_line(original_line)
            line_corrected = False

            for col_idx in date_column_indices:
                if col_idx < len(fields):
                    original_value = fields[col_idx]
                    repaired_value = repair_date_field(original_value)

                    if repaired_value != original_value:
                        fields[col_idx] = repaired_value
                        line_corrected = True
                        corrections_made += 1

                        if corrections_made <= 5:
                            console.print(
                                f"  [dim]Línea {line_num}:[/dim] "
                                f"[yellow]'{original_value}'[/yellow] "
                                f"[dim]→[/dim] "
                                f"[green]'{repaired_value}'[/green]"
                            )

            repaired_lines.append(_join_csv_line(fields))

            if line_num % 100 == 0 and line_num > 0:
                console.print(f"  [dim]Procesadas [white bold]{line_num}[/white bold] líneas...[/dim]")

        with open(output_file, "w", encoding="utf-8") as f:
            for repaired_line in repaired_lines:
                f.write(repaired_line + "\n")

        console.print("\n[cyan bold]ESTADÍSTICAS DE REPARACIÓN[/cyan bold]")
        console.print(f"  [dim]Archivo original:[/dim]  [white]{csv_file}[/white]")
        console.print(f"  [dim]Archivo reparado:[/dim]  [cyan]{output_file}[/cyan]")
        console.print(f"  [dim]Líneas procesadas:[/dim] [white bold]{len(repaired_lines)}[/white bold]")
        console.print(
            f"  [dim]Correcciones de fecha:[/dim] "
            f"[{'green bold' if corrections_made else 'dim'}]{corrections_made}[/{'green bold' if corrections_made else 'dim'}]"
        )
        console.print(f"  [dim]Columnas de fecha:[/dim] [cyan]{[name for _, name in date_columns]}[/cyan]")
        print_success(f"Reparación completada: [cyan]{output_file}[/cyan]")

        return output_file

    except Exception as exc:
        print_error(f"Error durante la reparación: {exc}")
        return None


def validate_date_repair(repaired_file: str) -> bool:
    """
    Valida que las fechas en el archivo reparado sean correctas.

    Returns:
        ``True`` si la validación se completó sin errores, ``False`` en caso
        contrario.
    """
    console.rule(f"[cyan bold]VALIDANDO REPARACIÓN — {repaired_file}[/cyan bold]", style="cyan")

    try:
        df = pd.read_csv(repaired_file, skiprows=1)

        console.print("\n[cyan bold]DATAFRAME REPARADO[/cyan bold]")
        console.print(f"  [dim]Filas:[/dim]    [white bold]{len(df)}[/white bold]")
        console.print(f"  [dim]Columnas:[/dim] [dim]{list(df.columns)}[/dim]")

        date_cols = [
            col
            for col in df.columns
            if any(kw in col.upper() for kw in ["TIME", "DATE", "SEEN", "FIRST", "LAST"])
        ]
        console.print(f"  [dim]Columnas de fecha:[/dim] [cyan]{date_cols}[/cyan]")

        for col in date_cols:
            if col not in df.columns:
                continue

            console.print(f"\n[cyan bold]COLUMNA '{col}'[/cyan bold]")
            unique_types = df[col].apply(lambda x: type(x).__name__).unique()
            console.print(f"  [dim]Tipos de datos:[/dim]           [dim]{unique_types}[/dim]")
            console.print(f"  [dim]Valores únicos (primeros 5):[/dim] [dim]{df[col].dropna().unique()[:5]}[/dim]")

            try:
                date_series = pd.to_datetime(df[col], errors="coerce", format="mixed")
                valid_dates = date_series.notna().sum()
                invalid_dates = date_series.isna().sum()

                console.print(f"  [dim]Fechas válidas:[/dim]   [green bold]{valid_dates}[/green bold]")
                if invalid_dates:
                    console.print(f"  [dim]Fechas inválidas:[/dim] [red bold]{invalid_dates}[/red bold]")

                if valid_dates > 0:
                    console.print(
                        f"  [dim]Rango de fechas:[/dim]  "
                        f"[white]{date_series.min()}[/white] [dim]→[/dim] [white]{date_series.max()}[/white]"
                    )
            except Exception as exc:
                print_warning(f"Error al convertir fechas en '{col}': {exc}")

        print_success("Validación completada")
        return True

    except Exception as exc:
        print_error(f"Error durante la validación: {exc}")
        return False
