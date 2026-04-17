"""
maps.py - Generación de mapas interactivos con Folium
"""

import folium
import pandas as pd
from folium.plugins import HeatMap

from ..utils.output import console, print_error, print_success


def generate_heat_map(df: pd.DataFrame, nombre_base: str) -> str:
    """
    Genera un mapa de calor de RSSI y lo guarda como HTML.
    """
    console.print("\n[cyan]Generando mapa de calor de RSSI...[/cyan]")

    try:
        centro_lat = df["CurrentLatitude"].mean()
        centro_lon = df["CurrentLongitude"].mean()

        mapa = folium.Map(location=[centro_lat, centro_lon], zoom_start=16, tiles="OpenStreetMap")

        heat_data = [
            [row["CurrentLatitude"], row["CurrentLongitude"], max(0.1, min(1.0, (row["RSSI"] + 100) / 40))]
            for _, row in df.iterrows()
        ]

        HeatMap(heat_data, radius=15, blur=10, max_zoom=1).add_to(mapa)

        output_path = f"mapa_calor_{nombre_base}.html"
        mapa.save(output_path)
        print_success(f"Mapa de calor guardado: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error generando mapa de calor: {exc}")
        return ""


def generate_location_map(df: pd.DataFrame, nombre_base: str) -> str:
    """
    Genera un mapa de localización con marcadores por punto de acceso y lo guarda como HTML.
    """
    console.print("\n[cyan]Generando mapa de localización...[/cyan]")

    try:
        centro_lat = df["CurrentLatitude"].mean()
        centro_lon = df["CurrentLongitude"].mean()

        mapa = folium.Map(location=[centro_lat, centro_lon], zoom_start=16, tiles="OpenStreetMap")

        colores = ["red", "blue", "green", "purple", "orange", "darkred", "darkblue"]
        grupos = df.groupby(["SSID", "CurrentLatitude", "CurrentLongitude"])

        for color_idx, ((ssid, lat, lon), group) in enumerate(grupos):
            lat, lon = float(str(lat)), float(str(lon))
            rssi_promedio = group["RSSI"].mean()
            cantidad = len(group)

            popup_text = (
                f"<b>{ssid}</b><br>"
                f"Ubicación: {lat:.6f}, {lon:.6f}<br>"
                f"RSSI: {rssi_promedio:.1f} dBm<br>"
                f"Detecciones: {cantidad}"
            )

            folium.CircleMarker(
                location=[lat, lon],
                radius=8,
                popup=popup_text,
                color=colores[color_idx % len(colores)],
                fill=True,
                fillOpacity=0.6,
            ).add_to(mapa)

        output_path = f"mapa_localizacion_{nombre_base}.html"
        mapa.save(output_path)
        print_success(f"Mapa de localización guardado: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error generando mapa de localización: {exc}")
        return ""
