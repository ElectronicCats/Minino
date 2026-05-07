"""
csv_debugger.py - Detection and repair of date issues in CSV files
"""

import os
import csv
import io

import pandas as pd
from typing import List, Optional, Tuple

from ..utils.validators import looks_like_date, repair_date_field
from ..utils.output import console, print_error, print_success, print_warning


# ---------------------------------------------------------------------------
# Internal Helpers
# ---------------------------------------------------------------------------

def _split_csv_line(line: str) -> List[str]:
    """Splits a CSV line respecting quoted fields."""
    if not line.strip():
        return []
    return next(csv.reader([line]))

def _join_csv_line(fields: List[str]) -> str:
    """Joins fields into a CSV line, escaping quotes if necessary."""
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
# Public API
# ---------------------------------------------------------------------------

def analyze_date_problems(
    csv_file: str,
) -> Tuple[Optional[List[str]], List[dict], List[Tuple[int, str]]]:
    """
    Analyzes date format issues in the CSV file.

    Returns:
        Tuple ``(headers, problematic_lines, date_columns)``.
    """
    console.rule(f"[cyan bold]ANALYZING DATE ISSUES — {csv_file}[/cyan bold]", style="cyan")

    try:
        # File size validation (Limit: 500 MB)
        max_size_mb = 500
        file_size_mb = os.path.getsize(csv_file) / (1024 * 1024)
        if file_size_mb > max_size_mb:
            print_error(f"The file is too large ({file_size_mb:.2f} MB). Safety limit: {max_size_mb} MB.")
            return None, [], []

        with open(csv_file, "r", encoding="utf-8", errors="ignore") as f:
            # Read only the first 12 lines for analysis
            lines = [line for _, line in zip(range(12), f)]

        if len(lines) < 2:
            print_error("The file is empty or has too few lines")
            return None, [], []

        console.print("\n[cyan bold]FILE STRUCTURE[/cyan bold]")
        headers = _split_csv_line(lines[1].strip())
        console.print(f"  [dim]Detected headers:[/dim] [white bold]{len(headers)}[/white bold]  [dim]{headers}[/dim]")

        date_columns = _find_date_columns(headers)
        console.print(f"  [dim]Potential date columns:[/dim] [cyan]{date_columns}[/cyan]")

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
                                "reason": "Does not look like a date",
                            }
                        )

        console.print("\n[cyan bold]DATE SAMPLES[/cyan bold]")
        for col_name, samples in date_samples.items():
            console.print(f"  [cyan]{col_name}:[/cyan] [dim]{samples}[/dim]")

        if problematic_lines:
            print_warning(f"[white bold]{len(problematic_lines)}[/white bold] problematic values detected")
        else:
            print_success("No problematic values detected in the first 10 lines")

        return headers, problematic_lines, date_columns

    except Exception as exc:
        print_error(f"Error during analysis: {exc}")
        return None, [], []


def repair_date_issues(
    csv_file: str,
    output_file: Optional[str] = None,
    output_dir: str = ".",
) -> Optional[str]:
    """
    Repairs date format issues in the CSV file.

    Returns:
        Path to the repaired file, or ``None`` if an error occurred.
    """
    if output_file is None:
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        output_file = os.path.join(output_dir, f"{base_name}_fixed.csv")

    console.rule("[cyan bold]REPAIRING DATE ISSUES[/cyan bold]", style="cyan")

    headers, _, date_columns = analyze_date_problems(csv_file)

    if not headers:
        print_error("Cannot proceed with repair")
        return None

    try:
        corrections_made = 0
        date_column_indices = [idx for idx, _ in date_columns]
        lines_processed = 0

        console.print("\n[cyan bold]APPLYING DATE CORRECTIONS...[/cyan bold]")

        with open(csv_file, "r", encoding="utf-8", errors="ignore") as f_in, \
             open(output_file, "w", encoding="utf-8") as f_out:
            
            for line_num, line in enumerate(f_in):
                lines_processed += 1
                original_line = line.strip()

                if line_num < 2:
                    f_out.write(original_line + "\n")
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
                                    f"  [dim]Line {line_num}:[/dim] "
                                    f"[yellow]'{original_value}'[/yellow] "
                                    f"[dim]→[/dim] "
                                    f"[green]'{repaired_value}'[/green]"
                                )

                f_out.write(_join_csv_line(fields) + "\n")

                if line_num % 1000 == 0 and line_num > 0:
                    console.print(f"  [dim]Processed [white bold]{line_num}[/white bold] lines...[/dim]")

        console.print("\n[cyan bold]REPAIR STATISTICS[/cyan bold]")
        console.print(f"  [dim]Original file:[/dim]  [white]{csv_file}[/white]")
        console.print(f"  [dim]Repaired file:[/dim]  [cyan]{output_file}[/cyan]")
        console.print(f"  [dim]Lines processed:[/dim] [white bold]{lines_processed}[/white bold]")
        console.print(
            f"  [dim]Date corrections:[/dim] "
            f"[{'green bold' if corrections_made else 'dim'}]{corrections_made}[/{'green bold' if corrections_made else 'dim'}]"
        )
        console.print(f"  [dim]Date columns:[/dim] [cyan]{[name for _, name in date_columns]}[/cyan]")
        print_success(f"Repair completed: [cyan]{output_file}[/cyan]")

        return output_file

    except Exception as exc:
        print_error(f"Error during repair: {exc}")
        return None


def validate_date_repair(repaired_file: str) -> bool:
    """
    Validates that the dates in the repaired file are correct.

    Returns:
        ``True`` if validation completed without errors, ``False`` otherwise.
    """
    console.rule(f"[cyan bold]VALIDATING REPAIR — {repaired_file}[/cyan bold]", style="cyan")

    try:
        df = pd.read_csv(repaired_file, skiprows=1)

        console.print("\n[cyan bold]REPAIRED DATAFRAME[/cyan bold]")
        console.print(f"  [dim]Rows:[/dim]    [white bold]{len(df)}[/white bold]")
        console.print(f"  [dim]Columns:[/dim] [dim]{list(df.columns)}[/dim]")

        date_cols = [
            col
            for col in df.columns
            if any(kw in col.upper() for kw in ["TIME", "DATE", "SEEN", "FIRST", "LAST"])
        ]
        console.print(f"  [dim]Date columns:[/dim] [cyan]{date_cols}[/cyan]")

        for col in date_cols:
            if col not in df.columns:
                continue

            console.print(f"\n[cyan bold]COLUMN '{col}'[/cyan bold]")
            unique_types = df[col].apply(lambda x: type(x).__name__).unique()
            console.print(f"  [dim]Data types:[/dim]           [dim]{unique_types}[/dim]")
            console.print(f"  [dim]Unique values (first 5):[/dim] [dim]{df[col].dropna().unique()[:5]}[/dim]")

            try:
                date_series = pd.to_datetime(df[col], errors="coerce", format="mixed")
                valid_dates = date_series.notna().sum()
                invalid_dates = date_series.isna().sum()

                console.print(f"  [dim]Valid dates:[/dim]   [green bold]{valid_dates}[/green bold]")
                if invalid_dates:
                    console.print(f"  [dim]Invalid dates:[/dim] [red bold]{invalid_dates}[/red bold]")

                if valid_dates > 0:
                    console.print(
                        f"  [dim]Date range:[/dim]  "
                        f"[white]{date_series.min()}[/white] [dim]→[/dim] [white]{date_series.max()}[/white]"
                    )
            except Exception as exc:
                print_warning(f"Error converting dates in '{col}': {exc}")

        print_success("Validation completed")
        return True

    except Exception as exc:
        print_error(f"Error during validation: {exc}")
        return False
