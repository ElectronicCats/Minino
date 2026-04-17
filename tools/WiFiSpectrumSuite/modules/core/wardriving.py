"""
wardriving.py - Análisis completo de datos de wardriving
"""

import csv
import os
from typing import Any, Dict, Optional

import pandas as pd

from ..utils.output import console, print_error, print_success, print_warning, rssi_color
from ..visualization.maps import generate_heat_map, generate_location_map
from ..visualization.plots import generate_wardriving_plots


# Mapeo de nombres alternativos de columnas al nombre canónico esperado
_COLUMN_ALIASES: Dict[str, list] = {
    "SSID":             ["SSID", "ssid", "Ssid"],
    "FirstSeen":        ["FirstSeen", "First seen", "firstseen", "Timestamp"],
    "Channel":          ["Channel", "channel", "CH"],
    "Frequency":        ["Frequency", "frequency", "Freq"],
    "RSSI":             ["RSSI", "rssi", "Signal"],
    "CurrentLatitude":  ["CurrentLatitude", "Latitude", "Lat", "latitude"],
    "CurrentLongitude": ["CurrentLongitude", "Longitude", "Lon", "longitude"],
    "AuthMode":         ["AuthMode", "Authentication", "Encryption", "auth"],
}


class WardrivingAnalyzer:
    """Análisis completo de datos de wardriving a partir de un CSV Kismet/Wigle."""

    def __init__(self, archivo_csv: str, output_dir: str = ".") -> None:
        self.archivo_csv = archivo_csv
        self.df: Optional[pd.DataFrame] = None
        self.nombre_base = os.path.splitext(os.path.basename(archivo_csv))[0]
        self.output_dir = output_dir

    # ------------------------------------------------------------------
    # Carga de datos
    # ------------------------------------------------------------------

    def cargar_datos(self) -> bool:
        """
        Carga y prepara los datos del archivo CSV.

        Intenta varias estrategias de lectura, normaliza nombres de columnas
        y convierte los tipos numéricos necesarios.

        Returns:
            ``True`` si los datos se cargaron correctamente, ``False`` en caso
            contrario.
        """
        if not os.path.exists(self.archivo_csv):
            print_error(f"El archivo '[cyan]{self.archivo_csv}[/cyan]' no existe")
            return False

        try:
            self.df = self._leer_csv()
        except Exception as exc:
            print(f"Error al cargar datos: {exc}")
            return False

        if self.df is None or self.df.empty:
            print_error("No se pudieron cargar datos válidos")
            return False

        self.df.columns = self.df.columns.str.strip()
        self._mapear_columnas()

        try:
            self._convertir_tipos()
        except Exception as exc:
            print(f"Error al preparar datos: {exc}")
            return False

        print_success(f"Datos preparados: [white bold]{len(self.df)}[/white bold] registros válidos")
        return True

    def _leer_csv(self) -> pd.DataFrame:
        """Intenta cargar el CSV con múltiples estrategias."""
        try:
            df = pd.read_csv(
                self.archivo_csv, skiprows=1,
                engine="python", quoting=csv.QUOTE_MINIMAL, on_bad_lines="warn",
            )
            print_success("Datos cargados con engine de Python")
            return df
        except Exception as exc:
            print_warning(f"Primer intento falló: [dim]{exc}[/dim]")
            console.print("  [dim]Intentando método alternativo...[/dim]")

        # Segunda estrategia: sin skiprows, detectar si tiene encabezado SSID
        temp_df = pd.read_csv(
            self.archivo_csv, engine="python",
            quoting=csv.QUOTE_MINIMAL, on_bad_lines="skip",
        )
        if len(temp_df.columns) > 1 and any("SSID" in str(c) for c in temp_df.columns):
            print_success("Datos cargados sin skiprows")
            return temp_df

        df = pd.read_csv(self.archivo_csv, skiprows=1, on_bad_lines="skip")
        print_success("Datos cargados ignorando líneas problemáticas")
        return df

    def _mapear_columnas(self) -> None:
        """Renombra columnas alternativas al nombre canónico si faltan."""
        faltantes = [c for c in _COLUMN_ALIASES if c not in self.df.columns]
        if faltantes:
            print_warning(f"Columnas faltantes: [white]{faltantes}[/white]")
            console.print(f"  [dim]Disponibles: {list(self.df.columns)}[/dim]")
            for col in faltantes:
                for alias in _COLUMN_ALIASES[col]:
                    if alias in self.df.columns:
                        self.df[col] = self.df[alias]
                        console.print(f"  [dim]Mapeada [/dim][cyan]{alias}[/cyan][dim] → [/dim][cyan]{col}[/cyan]")
                        break

    def _convertir_tipos(self) -> None:
        """Convierte columnas a los tipos numéricos y temporales correctos."""
        self.df["Timestamp"] = pd.to_datetime(self.df["FirstSeen"], errors="coerce")

        for col in ("Channel", "Frequency"):
            if col in self.df.columns:
                self.df[col] = pd.to_numeric(self.df[col], errors="coerce").fillna(0).astype(int)

        if "RSSI" in self.df.columns:
            self.df["RSSI"] = pd.to_numeric(self.df["RSSI"], errors="coerce")

        for col in ("CurrentLatitude", "CurrentLongitude"):
            if col in self.df.columns:
                self.df[col] = pd.to_numeric(self.df[col], errors="coerce")
                self.df = self.df.dropna(subset=[col])

    # ------------------------------------------------------------------
    # Análisis
    # ------------------------------------------------------------------

    def analizar_general(self) -> Dict[str, Any]:
        """
        Calcula métricas generales del dataset.

        Returns:
            Diccionario con total de registros, período de captura, redes
            únicas, top 5 SSIDs y métricas de RSSI. Devuelve ``{}`` si no
            hay datos.
        """
        if self.df is None or self.df.empty:
            return {}

        try:
            return {
                "total_registros": len(self.df),
                "periodo_captura": f"{self.df['FirstSeen'].min()} - {self.df['FirstSeen'].max()}",
                "redes_unicas": self.df["SSID"].nunique(),
                "top_redes": self.df["SSID"].value_counts().head(5).to_dict(),
                "metricas_rssi": {
                    "promedio":   self.df["RSSI"].mean(),
                    "minimo":     self.df["RSSI"].min(),
                    "maximo":     self.df["RSSI"].max(),
                    "desviacion": self.df["RSSI"].std(),
                },
            }
        except Exception as exc:
            print(f"Error en análisis general: {exc}")
            return {}

    # ------------------------------------------------------------------
    # Visualizaciones (delegadas a módulos de visualización)
    # ------------------------------------------------------------------

    def generar_mapa_calor(self) -> str:
        """Genera mapa de calor HTML de intensidad RSSI."""
        return generate_heat_map(self.df, self.nombre_base, output_dir=self.output_dir)

    def generar_mapa_localizacion(self) -> str:
        """Genera mapa HTML con marcadores por punto de acceso."""
        return generate_location_map(self.df, self.nombre_base, output_dir=self.output_dir)

    def generar_graficos(self) -> str:
        """Genera gráficos PNG avanzados de wardriving."""
        return generate_wardriving_plots(self.df, self.nombre_base, output_dir=self.output_dir)

    # ------------------------------------------------------------------
    # Reporte en consola
    # ------------------------------------------------------------------

    def generar_reporte(self) -> None:
        """Imprime un reporte detallado del análisis en consola."""
        console.rule(f"[cyan bold]REPORTE DETALLADO — {self.nombre_base}[/cyan bold]", style="cyan")

        if self.df is None or self.df.empty:
            print_error("No hay datos para generar reporte")
            return

        analisis = self.analizar_general()
        if not analisis:
            print_error("No se pudo realizar el análisis general")
            return

        console.print("\n[cyan bold]INFORMACIÓN GENERAL[/cyan bold]")
        console.print(f"  [dim]Total de registros:[/dim]      [white bold]{analisis['total_registros']:,}[/white bold]")
        console.print(f"  [dim]Período de captura:[/dim]      [white]{analisis['periodo_captura']}[/white]")
        console.print(f"  [dim]Redes únicas detectadas:[/dim] [white bold]{analisis['redes_unicas']:,}[/white bold]")

        if "Channel" in self.df.columns and "Frequency" in self.df.columns:
            canales = sorted(int(c) for c in self.df["Channel"].dropna().unique())
            frecuencias = sorted(int(f) for f in self.df["Frequency"].dropna().unique())
            console.print("\n[cyan bold]CANALES Y FRECUENCIAS[/cyan bold]")
            console.print(f"  [dim]Canales utilizados:[/dim]      [cyan]{canales}[/cyan]")
            console.print(f"  [dim]Frecuencias utilizadas:[/dim]  [cyan]{frecuencias}[/cyan] MHz")
            console.print(
                f"  [dim]Total canales:[/dim] [white bold]{len(canales)}[/white bold]  "
                f"[dim]Total frecuencias:[/dim] [white bold]{len(frecuencias)}[/white bold]"
            )

        rssi_m = analisis["metricas_rssi"]
        avg_color = rssi_color(rssi_m["promedio"])
        console.print("\n[cyan bold]MÉTRICAS DE SEÑAL[/cyan bold]")
        console.print(f"  [dim]RSSI promedio:[/dim] [{avg_color}]{rssi_m['promedio']:.1f} dBm[/{avg_color}]")
        console.print(
            f"  [dim]RSSI mínimo:[/dim]  [red]{rssi_m['minimo']} dBm[/red]  "
            f"[dim]Máximo:[/dim]  [green]{rssi_m['maximo']} dBm[/green]"
        )

        console.print("\n[cyan bold]TOP 5 REDES[/cyan bold]")
        for ssid, count in analisis["top_redes"].items():
            rssi_prom = self.df.loc[self.df["SSID"] == ssid, "RSSI"].mean() if "RSSI" in self.df.columns else 0.0
            color = rssi_color(rssi_prom)
            console.print(
                f"  [white bold]{ssid}[/white bold]  "
                f"[dim]{count} detecciones[/dim]  "
                f"[{color}]{rssi_prom:.1f} dBm[/{color}]"
            )

        if "AuthMode" in self.df.columns:
            self._analizar_seguridad()

        if "RSSI" in self.df.columns:
            self._analizar_calidad_señal()

        console.print("\n[cyan bold]RECOMENDACIONES[/cyan bold]")
        console.print("  [dim]1.[/dim] Analizar interferencias entre canales cercanos")
        console.print("  [dim]2.[/dim] Verificar seguridad de redes con encriptación débil")
        console.print("  [dim]3.[/dim] Optimizar ubicación de puntos de acceso")
        console.print("  [dim]4.[/dim] Considerar repetidores en áreas de señal débil")

    def _analizar_seguridad(self) -> None:
        console.print("\n[cyan bold]ANÁLISIS DE SEGURIDAD[/cyan bold]")

        abiertas = self.df[self.df["AuthMode"] == "OPEN"]
        if not abiertas.empty:
            console.print(f"  [red]⚠[/red] Redes abiertas: [red bold]{abiertas['SSID'].nunique()}[/red bold]")
            for ssid in abiertas["SSID"].unique()[:5]:
                console.print(f"    [red]•[/red] [white]{ssid}[/white]")

        wep = self.df[self.df["AuthMode"] == "WEP"]
        if not wep.empty:
            console.print(f"  [orange3]⚠[/orange3] Redes WEP (encriptación débil): [orange3 bold]{wep['SSID'].nunique()}[/orange3 bold]")
            for ssid in wep["SSID"].unique()[:5]:
                console.print(f"    [orange3]•[/orange3] [white]{ssid}[/white]")

        wpa2 = self.df[self.df["AuthMode"].str.contains("WPA2", na=False)]
        if not wpa2.empty:
            console.print(f"  [green]✓[/green] Redes WPA2: [green bold]{wpa2['SSID'].nunique()}[/green bold]")

    def _analizar_calidad_señal(self) -> None:
        console.print("\n[cyan bold]CALIDAD DE SEÑAL[/cyan bold]")
        total = len(self.df)
        excelente = len(self.df[self.df["RSSI"] > -65])
        buena     = len(self.df[(self.df["RSSI"] >= -75) & (self.df["RSSI"] <= -65)])
        aceptable = len(self.df[(self.df["RSSI"] >= -85) & (self.df["RSSI"] < -75)])
        debil     = len(self.df[self.df["RSSI"] < -85])

        rows = [
            ("Excelente", "> −65 dBm",      excelente, "green bold"),
            ("Buena",     "−65 a −75 dBm",  buena,     "green"),
            ("Aceptable", "−75 a −85 dBm",  aceptable, "yellow"),
            ("Débil",     "< −85 dBm",       debil,     "red"),
        ]
        for label, rango, n, color in rows:
            pct = n / total * 100
            bar_len = int(pct / 100 * 20)
            bar = f"[{color}]{'█' * bar_len}[/{color}][dim]{'░' * (20 - bar_len)}[/dim]"
            console.print(
                f"  [{color}]{label:<10}[/{color}] [dim]{rango:<18}[/dim] "
                f"{bar} [{color}]{n:5,}[/{color}] [dim]({pct:.1f}%)[/dim]"
            )
