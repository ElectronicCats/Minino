"""
output.py - Shared Rich console and output helpers for all modules
"""

from rich.console import Console
from rich.style import Style

STYLES = {
    "header":  Style(color="cyan",    bold=True),
    "success": Style(color="green",   bold=True),
    "warning": Style(color="yellow",  bold=True),
    "error":   Style(color="red",     bold=True),
    "info":    Style(color="blue",    bold=True),
    "prompt":  Style(color="magenta", bold=True),
}

console = Console()


def print_success(message: str) -> None:
    console.print(f"[green]✓[/green] {message}", style=STYLES["success"])


def print_warning(message: str) -> None:
    console.print(f"[yellow]⚠[/yellow] {message}", style=STYLES["warning"])


def print_error(message: str) -> None:
    console.print(f"[red]✗[/red] {message}", style=STYLES["error"])


def print_info(message: str) -> None:
    console.print(f"[blue]ℹ[/blue] {message}", style=STYLES["info"])


def rssi_color(rssi: float) -> str:
    """Returns the corresponding Rich color for the given RSSI signal quality."""
    if rssi >= -50:
        return "green bold"
    if rssi >= -60:
        return "green"
    if rssi >= -70:
        return "yellow"
    if rssi >= -80:
        return "orange3"
    return "red"
