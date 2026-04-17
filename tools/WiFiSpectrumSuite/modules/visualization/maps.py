"""
maps.py - Generation of interactive maps with Folium
"""

import os

import folium
import pandas as pd
from folium.plugins import HeatMap

from ..utils.output import console, print_error, print_success


def generate_heat_map(df: pd.DataFrame, base_name: str, output_dir: str = ".") -> str:
    """
    Generates an RSSI heatmap and saves it as HTML.
    """
    console.print("\n[cyan]Generating RSSI heatmap...[/cyan]")

    try:
        center_lat = df["CurrentLatitude"].mean()
        center_lon = df["CurrentLongitude"].mean()

        map_obj = folium.Map(location=[center_lat, center_lon], zoom_start=16, tiles="OpenStreetMap")

        heat_data = [
            [row["CurrentLatitude"], row["CurrentLongitude"], max(0.1, min(1.0, (row["RSSI"] + 100) / 40))]
            for _, row in df.iterrows()
        ]

        HeatMap(heat_data, radius=15, blur=10, max_zoom=1).add_to(map_obj)

        output_path = os.path.join(output_dir, f"heat_map_{base_name}.html")
        map_obj.save(output_path)
        print_success(f"Heatmap saved: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error generating heatmap: {exc}")
        return ""


def generate_location_map(df: pd.DataFrame, base_name: str, output_dir: str = ".") -> str:
    """
    Generates a location map with markers per access point and saves it as HTML.
    """
    console.print("\n[cyan]Generating location map...[/cyan]")

    try:
        center_lat = df["CurrentLatitude"].mean()
        center_lon = df["CurrentLongitude"].mean()

        map_obj = folium.Map(location=[center_lat, center_lon], zoom_start=16, tiles="OpenStreetMap")

        colors = ["red", "blue", "green", "purple", "orange", "darkred", "darkblue"]
        groups = df.groupby(["SSID", "CurrentLatitude", "CurrentLongitude"])

        for color_idx, ((ssid, lat, lon), group) in enumerate(groups):
            lat, lon = float(str(lat)), float(str(lon))
            avg_rssi = group["RSSI"].mean()
            count = len(group)

            popup_text = (
                f"<b>{ssid}</b><br>"
                f"Location: {lat:.6f}, {lon:.6f}<br>"
                f"RSSI: {avg_rssi:.1f} dBm<br>"
                f"Detections: {count}"
            )

            folium.CircleMarker(
                location=[lat, lon],
                radius=8,
                popup=popup_text,
                color=colors[color_idx % len(colors)],
                fill=True,
                fillOpacity=0.6,
            ).add_to(map_obj)

        output_path = os.path.join(output_dir, f"location_map_{base_name}.html")
        map_obj.save(output_path)
        print_success(f"Location map saved: [cyan]{output_path}[/cyan]")
        return output_path

    except Exception as exc:
        print_error(f"Error generating location map: {exc}")
        return ""
