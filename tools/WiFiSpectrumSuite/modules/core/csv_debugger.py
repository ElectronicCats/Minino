"""
csv_debugger.py - Detección y reparación de problemas de fecha en archivos CSV
"""

import pandas as pd
from typing import List, Optional, Tuple

from ..utils.validators import looks_like_date, repair_date_field


# ---------------------------------------------------------------------------
# Helpers internos
# ---------------------------------------------------------------------------

def _find_date_columns(headers: List[str]) -> List[Tuple[int, str]]:
    """
    Identifica columnas que probablemente contienen fechas por su nombre.

    Args:
        headers: Lista de encabezados del CSV.

    Returns:
        Lista de tuplas (índice, nombre) para columnas candidatas.
    """
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

    Lee las primeras 10 líneas de datos y muestra muestras de las columnas
    de fecha detectadas.

    Args:
        csv_file: Ruta al archivo CSV.

    Returns:
        Tupla ``(headers, problematic_lines, date_columns)`` donde:
        - *headers* es la lista de encabezados, o ``None`` si falló.
        - *problematic_lines* lista de dicts con info de líneas con problemas.
        - *date_columns* lista de ``(índice, nombre)`` de columnas de fecha.
    """
    print(f"ANALIZANDO PROBLEMAS DE FECHA EN: {csv_file}")
    print("=" * 60)

    try:
        with open(csv_file, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()

        print(f"Total de líneas en el archivo: {len(lines)}")

        if len(lines) < 2:
            print("El archivo está vacío o tiene muy pocas líneas")
            return None, [], []

        print("\nANALIZANDO ESTRUCTURA:")
        headers = lines[1].strip().split(",")
        print(f"Encabezados detectados ({len(headers)}): {headers}")

        date_columns = _find_date_columns(headers)
        print(f"Columnas potencialmente de fecha: {date_columns}")

        date_samples: dict = {}
        problematic_lines: List[dict] = []

        for line_num, line in enumerate(lines[2:12], start=3):
            fields = line.strip().split(",")
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

        print("\nMUESTRAS DE FECHAS:")
        for col_name, samples in date_samples.items():
            print(f"  {col_name}: {samples}")

        return headers, problematic_lines, date_columns

    except Exception as exc:
        print(f"ERROR durante el análisis: {exc}")
        return None, [], []


def repair_date_issues(
    csv_file: str,
    output_file: Optional[str] = None,
) -> Optional[str]:
    """
    Repara problemas de formato de fecha en el archivo CSV.

    Escribe un nuevo archivo con las correcciones aplicadas.

    Args:
        csv_file: Ruta al archivo CSV original.
        output_file: Ruta para el archivo reparado. Si es ``None`` se usa
                     ``<nombre_original>_fixed.csv``.

    Returns:
        Ruta al archivo reparado, o ``None`` si ocurrió un error.
    """
    if output_file is None:
        output_file = csv_file.replace(".csv", "_fixed.csv")

    print("\nREPARANDO PROBLEMAS DE FECHA")
    print("=" * 50)

    headers, _, date_columns = analyze_date_problems(csv_file)

    if not headers:
        print("No se puede proceder con la reparación")
        return None

    try:
        with open(csv_file, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()

        repaired_lines: List[str] = []
        corrections_made = 0
        date_column_indices = [idx for idx, _ in date_columns]

        print("\nAPLICANDO CORRECCIONES DE FECHA...")

        for line_num, line in enumerate(lines):
            original_line = line.strip()

            # Las dos primeras líneas (metadata + headers) se conservan tal cual
            if line_num < 2:
                repaired_lines.append(original_line)
                continue

            fields = original_line.split(",")
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
                            print(
                                f"  Línea {line_num}: '{original_value}' → '{repaired_value}'"
                            )

            repaired_lines.append(",".join(fields))

            if line_num % 100 == 0 and line_num > 0:
                print(f"  Procesadas {line_num} líneas...")

        with open(output_file, "w", encoding="utf-8") as f:
            for repaired_line in repaired_lines:
                f.write(repaired_line + "\n")

        print("\nREPARACIÓN DE FECHAS COMPLETADA")
        print("=" * 40)
        print(f"ESTADÍSTICAS:")
        print(f"   - Archivo original: {csv_file}")
        print(f"   - Archivo reparado: {output_file}")
        print(f"   - Líneas procesadas: {len(repaired_lines)}")
        print(f"   - Correcciones de fecha aplicadas: {corrections_made}")
        print(
            f"   - Columnas de fecha identificadas: {[name for _, name in date_columns]}"
        )

        return output_file

    except Exception as exc:
        print(f"ERROR durante la reparación: {exc}")
        return None


def validate_date_repair(repaired_file: str) -> bool:
    """
    Valida que las fechas en el archivo reparado sean correctas.

    Muestra estadísticas de fechas válidas/inválidas por columna.

    Args:
        repaired_file: Ruta al archivo CSV reparado.

    Returns:
        ``True`` si la validación se completó sin errores, ``False`` en caso
        contrario.
    """
    print(f"\nVALIDANDO REPARACIÓN DE FECHAS: {repaired_file}")

    try:
        df = pd.read_csv(repaired_file, skiprows=1)

        print("INFORMACIÓN DEL DATAFRAME REPARADO:")
        print(f"   - Filas: {len(df)}")
        print(f"   - Columnas: {list(df.columns)}")

        date_cols = [
            col
            for col in df.columns
            if any(
                kw in col.upper() for kw in ["TIME", "DATE", "SEEN", "FIRST", "LAST"]
            )
        ]
        print(f"   - Columnas de fecha identificadas: {date_cols}")

        for col in date_cols:
            if col not in df.columns:
                continue

            print(f"\nANÁLISIS DE LA COLUMNA '{col}':")
            unique_types = df[col].apply(lambda x: type(x).__name__).unique()
            print(f"   - Tipos de datos: {unique_types}")
            print(f"   - Valores únicos (primeros 5): {df[col].dropna().unique()[:5]}")

            try:
                date_series = pd.to_datetime(df[col], errors="coerce", format="mixed")
                valid_dates = date_series.notna().sum()
                invalid_dates = date_series.isna().sum()

                print(f"   - Fechas válidas: {valid_dates}")
                print(f"   - Fechas inválidas: {invalid_dates}")

                if valid_dates > 0:
                    print(
                        f"   - Rango de fechas: {date_series.min()} a {date_series.max()}"
                    )
            except Exception as exc:
                print(f"  Error al convertir fechas: {exc}")

        print("\nVALIDACIÓN COMPLETADA")
        return True

    except Exception as exc:
        print(f"ERROR durante la validación: {exc}")
        return False