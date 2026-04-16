"""
wardriving.py - Análisis completo de datos de wardriving
"""

import csv
import os
from typing import Any, Dict, Optional

import pandas as pd

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

    def __init__(self, archivo_csv: str) -> None:
        self.archivo_csv = archivo_csv
        self.df: Optional[pd.DataFrame] = None
        self.nombre_base = os.path.splitext(os.path.basename(archivo_csv))[0]

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
            print(f"Error: El archivo '{self.archivo_csv}' no existe")
            return False

        try:
            self.df = self._leer_csv()
        except Exception as exc:
            print(f"Error al cargar datos: {exc}")
            return False

        if self.df is None or self.df.empty:
            print("No se pudieron cargar datos válidos")
            return False

        self.df.columns = self.df.columns.str.strip()
        self._mapear_columnas()

        try:
            self._convertir_tipos()
        except Exception as exc:
            print(f"Error al preparar datos: {exc}")
            return False

        print(f"Datos preparados: {len(self.df)} registros válidos")
        return True

    def _leer_csv(self) -> pd.DataFrame:
        """Intenta cargar el CSV con múltiples estrategias."""
        try:
            df = pd.read_csv(
                self.archivo_csv, skiprows=1,
                engine="python", quoting=csv.QUOTE_MINIMAL, on_bad_lines="warn",
            )
            print("Datos cargados con engine de Python")
            return df
        except Exception as exc:
            print(f"Primer intento falló: {exc}")
            print("Intentando método alternativo...")

        # Segunda estrategia: sin skiprows, detectar si tiene encabezado SSID
        temp_df = pd.read_csv(
            self.archivo_csv, engine="python",
            quoting=csv.QUOTE_MINIMAL, on_bad_lines="skip",
        )
        if len(temp_df.columns) > 1 and any("SSID" in str(c) for c in temp_df.columns):
            print("Datos cargados sin skiprows")
            return temp_df

        # Tercera estrategia: skiprows + ignorar líneas malas
        df = pd.read_csv(
            self.archivo_csv, skiprows=1, on_bad_lines="skip",
        )
        print("Datos cargados ignorando líneas problemáticas")
        return df

    def _mapear_columnas(self) -> None:
        """Renombra columnas alternativas al nombre canónico si faltan."""
        faltantes = [c for c in _COLUMN_ALIASES if c not in self.df.columns]
        if faltantes:
            print(f"Columnas faltantes: {faltantes}")
            print(f"Columnas disponibles: {list(self.df.columns)}")
            for col in faltantes:
                for alias in _COLUMN_ALIASES[col]:
                    if alias in self.df.columns:
                        self.df[col] = self.df[alias]
                        print(f"Mapeada columna '{alias}' → '{col}'")
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
        return generate_heat_map(self.df, self.nombre_base)

    def generar_mapa_localizacion(self) -> str:
        """Genera mapa HTML con marcadores por punto de acceso."""
        return generate_location_map(self.df, self.nombre_base)

    def generar_graficos(self) -> str:
        """Genera gráficos PNG avanzados de wardriving."""
        return generate_wardriving_plots(self.df, self.nombre_base)

    # ------------------------------------------------------------------
    # Reporte en consola
    # ------------------------------------------------------------------

    def generar_reporte(self) -> None:
        """Imprime un reporte detallado del análisis en consola."""
        print("\n" + "=" * 60)
        print(f"REPORTE DETALLADO — {self.nombre_base}")
        print("=" * 60)

        if self.df is None or self.df.empty:
            print("No hay datos para generar reporte")
            return

        analisis = self.analizar_general()
        if not analisis:
            print("No se pudo realizar el análisis general")
            return

        print("\nINFORMACIÓN GENERAL:")
        print(f"Total de registros: {analisis['total_registros']}")
        print(f"Período de captura: {analisis['periodo_captura']}")
        print(f"Redes únicas detectadas: {analisis['redes_unicas']}")

        if "Channel" in self.df.columns and "Frequency" in self.df.columns:
            canales = sorted(int(c) for c in self.df["Channel"].dropna().unique())
            frecuencias = sorted(int(f) for f in self.df["Frequency"].dropna().unique())
            print("\nCANALES Y FRECUENCIAS:")
            print(f"Canales utilizados: {canales}")
            print(f"Frecuencias utilizadas: {frecuencias} MHz")
            print(f"Total canales: {len(canales)}, Total frecuencias: {len(frecuencias)}")

        rssi = analisis["metricas_rssi"]
        print("\nMÉTRICAS DE SEÑAL:")
        print(f"RSSI promedio: {rssi['promedio']:.1f} dBm")
        print(f"RSSI mínimo: {rssi['minimo']} dBm, Máximo: {rssi['maximo']} dBm")

        print("\nTOP 5 REDES:")
        for ssid, count in analisis["top_redes"].items():
            rssi_prom = self.df.loc[self.df["SSID"] == ssid, "RSSI"].mean() if "RSSI" in self.df.columns else 0.0
            print(f"  - {ssid}: {count} detecciones | RSSI: {rssi_prom:.1f} dBm")

        if "AuthMode" in self.df.columns:
            self._analizar_seguridad()

        if "RSSI" in self.df.columns:
            self._analizar_calidad_señal()

        print("\nRECOMENDACIONES:")
        print("1. Analizar interferencias entre canales cercanos")
        print("2. Verificar seguridad de redes con encriptación débil")
        print("3. Optimizar ubicación de puntos de acceso")
        print("4. Considerar repetidores en áreas de señal débil")

    def _analizar_seguridad(self) -> None:
        print("\nANÁLISIS DE SEGURIDAD:")

        abiertas = self.df[self.df["AuthMode"] == "OPEN"]
        if not abiertas.empty:
            print(f"  Redes abiertas detectadas: {abiertas['SSID'].nunique()}")
            for ssid in abiertas["SSID"].unique()[:5]:
                print(f"    - {ssid}")

        wep = self.df[self.df["AuthMode"] == "WEP"]
        if not wep.empty:
            print(f"  Redes WEP (encriptación débil): {wep['SSID'].nunique()}")
            for ssid in wep["SSID"].unique()[:5]:
                print(f"    - {ssid}")

        wpa2 = self.df[self.df["AuthMode"].str.contains("WPA2", na=False)]
        if not wpa2.empty:
            print(f"  Redes con encriptación WPA2: {wpa2['SSID'].nunique()}")

    def _analizar_calidad_señal(self) -> None:
        print("\nCALIDAD DE SEÑAL:")
        total = len(self.df)
        excelente = len(self.df[self.df["RSSI"] > -65])
        buena     = len(self.df[(self.df["RSSI"] >= -75) & (self.df["RSSI"] <= -65)])
        aceptable = len(self.df[(self.df["RSSI"] >= -85) & (self.df["RSSI"] < -75)])
        debil     = len(self.df[self.df["RSSI"] < -85])

        print(f"Excelente (> -65 dBm):      {excelente} ({excelente / total * 100:.1f}%)")
        print(f"Buena     (-65 a -75 dBm):  {buena}     ({buena     / total * 100:.1f}%)")
        print(f"Aceptable (-75 a -85 dBm):  {aceptable} ({aceptable / total * 100:.1f}%)")
        print(f"Débil     (< -85 dBm):      {debil}     ({debil     / total * 100:.1f}%)")
