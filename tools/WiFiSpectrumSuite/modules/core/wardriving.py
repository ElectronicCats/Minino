"""
wardriving.py - Complete analysis of wardriving data
"""

import csv
import os
from typing import Any, Dict, Optional

import pandas as pd

from ..utils.output import console, print_error, print_success, print_warning, rssi_color
from ..visualization.maps import generate_heat_map, generate_location_map
from ..visualization.plots import generate_wardriving_plots


# Mapping of alternative column names to the expected canonical name
_COLUMN_ALIASES: Dict[str, list] = {
    "SSID":             ["SSID", "ssid", "Ssid"],
    "MAC":              ["BSSID", "MAC", "bssid", "mac"],
    "FirstSeen":        ["FirstSeen", "First seen", "firstseen", "Timestamp"],
    "Channel":          ["Channel", "channel", "CH"],
    "Frequency":        ["Frequency", "frequency", "Freq"],
    "RSSI":             ["RSSI", "rssi", "Signal"],
    "CurrentLatitude":  ["CurrentLatitude", "Latitude", "Lat", "latitude"],
    "CurrentLongitude": ["CurrentLongitude", "Longitude", "Lon", "longitude"],
    "AuthMode":         ["AuthMode", "Authentication", "Encryption", "auth"],
}


class WardrivingAnalyzer:
    """Complete analysis of wardriving data from a Kismet/Wigle CSV."""

    def __init__(self, csv_file: str, output_dir: str = ".") -> None:
        self.csv_file = csv_file
        self.df: Optional[pd.DataFrame] = None
        self.base_name = os.path.splitext(os.path.basename(csv_file))[0]
        self.output_dir = output_dir

    # ------------------------------------------------------------------
    # Data Loading
    # ------------------------------------------------------------------

    def load_data(self) -> bool:
        """
        Loads and prepares data from the CSV file.

        Attempts multiple reading strategies, normalizes column names,
        and converts necessary numeric types.

        Returns:
            ``True`` if the data was loaded successfully, ``False`` otherwise.
        """
        if not os.path.exists(self.csv_file):
            print_error(f"The file '[cyan]{self.csv_file}[/cyan]' does not exist")
            return False

        # File size validation (Limit: 500 MB)
        max_size_mb = 500
        file_size_mb = os.path.getsize(self.csv_file) / (1024 * 1024)
        if file_size_mb > max_size_mb:
            print_error(f"The file is too large ({file_size_mb:.2f} MB). Safety limit: {max_size_mb} MB.")
            return False

        try:
            self.df = self._read_csv()
        except Exception as exc:
            print_error(f"Error loading data: {exc}")
            return False

        if self.df is None or self.df.empty:
            print_error("Could not load valid data")
            return False

        self.df.columns = self.df.columns.str.strip()
        self._map_columns()

        try:
            self._convert_types()
        except Exception as exc:
            print_error(f"Error preparing data: {exc}")
            return False

        print_success(f"Data prepared: [white bold]{len(self.df)}[/white bold] valid records")
        return True

    def _read_csv(self) -> pd.DataFrame:
        """Attempts to load the CSV using multiple strategies."""
        try:
            df = pd.read_csv(
                self.csv_file, skiprows=1,
                engine="python", quoting=csv.QUOTE_MINIMAL, on_bad_lines="warn",
            )
            print_success("Data loaded with Python engine")
            return df
        except Exception as exc:
            print_warning(f"First attempt failed: [dim]{exc}[/dim]")
            console.print("  [dim]Trying alternative method...[/dim]")

        # Second strategy: no skiprows, detect if it has SSID header
        temp_df = pd.read_csv(
            self.csv_file, engine="python",
            quoting=csv.QUOTE_MINIMAL, on_bad_lines="skip",
        )
        if len(temp_df.columns) > 1 and any("SSID" in str(c) for c in temp_df.columns):
            print_success("Data loaded without skiprows")
            return temp_df

        df = pd.read_csv(self.csv_file, skiprows=1, on_bad_lines="skip")
        print_success("Data loaded ignoring problematic lines")
        return df

    def _map_columns(self) -> None:
        """Renames alternative columns to the canonical name if missing."""
        missing = [c for c in _COLUMN_ALIASES if c not in self.df.columns]
        if missing:
            print_warning(f"Missing columns: [white]{missing}[/white]")
            console.print(f"  [dim]Available: {list(self.df.columns)}[/dim]")
            for col in missing:
                for alias in _COLUMN_ALIASES[col]:
                    if alias in self.df.columns:
                        self.df[col] = self.df[alias]
                        console.print(f"  [dim]Mapped [/dim][cyan]{alias}[/cyan][dim] → [/dim][cyan]{col}[/cyan]")
                        break

    def _convert_types(self) -> None:
        """Converts columns to the correct numeric and datetime types."""
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
    # Analysis
    # ------------------------------------------------------------------

    def analyze_general(self) -> Dict[str, Any]:
        """
        Calculates general metrics of the dataset.

        Returns:
            Dictionary with total records, capture period, unique networks,
            top 5 SSIDs, and RSSI metrics. Returns ``{}`` if there is no data.
        """
        if self.df is None or self.df.empty:
            return {}

        try:
            return {
                "total_records": len(self.df),
                "capture_period": f"{self.df['FirstSeen'].min()} - {self.df['FirstSeen'].max()}",
                "unique_networks": self.df["SSID"].nunique(),
                "top_networks": self.df["SSID"].value_counts().head(5).to_dict(),
                "rssi_metrics": {
                    "average":   self.df["RSSI"].mean(),
                    "minimum":     self.df["RSSI"].min(),
                    "maximum":     self.df["RSSI"].max(),
                    "deviation": self.df["RSSI"].std(),
                },
            }
        except Exception as exc:
            print_error(f"Error in general analysis: {exc}")
            return {}

    # ------------------------------------------------------------------
    # Visualizations (delegated to visualization modules)
    # ------------------------------------------------------------------

    def generate_heat_map(self) -> str:
        """Generates an HTML heatmap of RSSI intensity."""
        return generate_heat_map(self.df, self.base_name, output_dir=self.output_dir)

    def generate_location_map(self) -> str:
        """Generates an HTML map with markers per access point."""
        return generate_location_map(self.df, self.base_name, output_dir=self.output_dir)

    def generate_plots(self) -> str:
        """Generates advanced PNG plots for wardriving."""
        return generate_wardriving_plots(self.df, self.base_name, output_dir=self.output_dir)

    # ------------------------------------------------------------------
    # Console Report
    # ------------------------------------------------------------------

    def generate_report(self) -> None:
        """Prints a detailed analysis report to the console."""
        console.rule(f"[cyan bold]DETAILED REPORT — {self.base_name}[/cyan bold]", style="cyan")

        if self.df is None or self.df.empty:
            print_error("No data to generate report")
            return

        analysis = self.analyze_general()
        if not analysis:
            print_error("Could not perform general analysis")
            return

        console.print("\n[cyan bold]GENERAL INFORMATION[/cyan bold]")
        console.print(f"  [dim]Total records:[/dim]      [white bold]{analysis['total_records']:,}[/white bold]")
        console.print(f"  [dim]Capture period:[/dim]      [white]{analysis['capture_period']}[/white]")
        console.print(f"  [dim]Unique networks detected:[/dim] [white bold]{analysis['unique_networks']:,}[/white bold]")

        if "Channel" in self.df.columns and "Frequency" in self.df.columns:
            canales = sorted(int(c) for c in self.df["Channel"].dropna().unique())
            frecuencias = sorted(int(f) for f in self.df["Frequency"].dropna().unique())
            console.print("\n[cyan bold]CHANNELS AND FREQUENCIES[/cyan bold]")
            console.print(f"  [dim]Channels used:[/dim]      [cyan]{canales}[/cyan]")
            console.print(f"  [dim]Frequencies used:[/dim]  [cyan]{frecuencias}[/cyan] MHz")
            console.print(
                f"  [dim]Total channels:[/dim] [white bold]{len(canales)}[/white bold]  "
                f"[dim]Total frequencies:[/dim] [white bold]{len(frecuencias)}[/white bold]"
            )

        rssi_m = analysis["rssi_metrics"]
        avg_color = rssi_color(rssi_m["average"])
        console.print("\n[cyan bold]SIGNAL METRICS[/cyan bold]")
        console.print(f"  [dim]Average RSSI:[/dim] [{avg_color}]{rssi_m['average']:.1f} dBm[/{avg_color}]")
        console.print(
            f"  [dim]Minimum RSSI:[/dim]  [red]{rssi_m['minimum']} dBm[/red]  "
            f"[dim]Maximum:[/dim]  [green]{rssi_m['maximum']} dBm[/green]"
        )

        console.print("\n[cyan bold]TOP 5 NETWORKS[/cyan bold]")
        for ssid, count in analysis["top_networks"].items():
            rssi_prom = self.df.loc[self.df["SSID"] == ssid, "RSSI"].mean() if "RSSI" in self.df.columns else 0.0
            color = rssi_color(rssi_prom)
            console.print(
                f"  [white bold]{ssid}[/white bold]  "
                f"[dim]{count} detections[/dim]  "
                f"[{color}]{rssi_prom:.1f} dBm[/{color}]"
            )

        if "AuthMode" in self.df.columns:
            self._analyze_security()

        if "RSSI" in self.df.columns:
            self._analyze_signal_quality()

        if "MAC" in self.df.columns and "SSID" in self.df.columns and "AuthMode" in self.df.columns:
            self._analyze_spoofing()

        console.print("\n[cyan bold]RECOMMENDATIONS[/cyan bold]")
        console.print("  [dim]1.[/dim] Analyze interference between nearby channels")
        console.print("  [dim]2.[/dim] Verify security of networks with weak encryption")
        console.print("  [dim]3.[/dim] Optimize placement of access points")
        console.print("  [dim]4.[/dim] Consider repeaters in areas with weak signals")

    def _analyze_security(self) -> None:
        console.print("\n[cyan bold]SECURITY ANALYSIS[/cyan bold]")

        abiertas = self.df[self.df["AuthMode"] == "OPEN"]
        if not abiertas.empty:
            console.print(f"  [red]⚠[/red] Open networks: [red bold]{abiertas['SSID'].nunique()}[/red bold]")
            for ssid in abiertas["SSID"].unique()[:5]:
                console.print(f"    [red]•[/red] [white]{ssid}[/white]")

        wep = self.df[self.df["AuthMode"] == "WEP"]
        if not wep.empty:
            console.print(f"  [orange3]⚠[/orange3] WEP networks (weak encryption): [orange3 bold]{wep['SSID'].nunique()}[/orange3 bold]")
            for ssid in wep["SSID"].unique()[:5]:
                console.print(f"    [orange3]•[/orange3] [white]{ssid}[/white]")

        wpa2 = self.df[self.df["AuthMode"].str.contains("WPA2", na=False)]
        if not wpa2.empty:
            console.print(f"  [green]✓[/green] WPA2 networks: [green bold]{wpa2['SSID'].nunique()}[/green bold]")

    def _analyze_signal_quality(self) -> None:
        console.print("\n[cyan bold]SIGNAL QUALITY[/cyan bold]")
        total = len(self.df)
        excelente = len(self.df[self.df["RSSI"] > -65])
        buena     = len(self.df[(self.df["RSSI"] >= -75) & (self.df["RSSI"] <= -65)])
        aceptable = len(self.df[(self.df["RSSI"] >= -85) & (self.df["RSSI"] < -75)])
        debil     = len(self.df[self.df["RSSI"] < -85])

        rows = [
            ("Excellent", "> −65 dBm",      excelente, "green bold"),
            ("Good",      "−65 to −75 dBm", buena,     "green"),
            ("Fair",      "−75 to −85 dBm", aceptable, "yellow"),
            ("Weak",      "< −85 dBm",      debil,     "red"),
        ]
        for label, rango, n, color in rows:
            pct = n / total * 100
            bar_len = int(pct / 100 * 20)
            bar = f"[{color}]{'█' * bar_len}[/{color}][dim]{'░' * (20 - bar_len)}[/dim]"
            console.print(
                f"  [{color}]{label:<10}[/{color}] [dim]{rango:<18}[/dim] "
                f"{bar} [{color}]{n:5,}[/{color}] [dim]({pct:.1f}%)[/dim]"
            )

    def _analyze_spoofing(self) -> None:
        console.print("\n[cyan bold]SPOOFING / EVIL TWIN ANALYSIS[/cyan bold]")

        df_valid = self.df.dropna(subset=["MAC", "SSID", "AuthMode"])
        unique_aps = df_valid.drop_duplicates(subset=["MAC", "SSID", "AuthMode"])

        evil_twins_detectados = 0
        multi_mac_ssids = 0
        reporte_rows: list = []

        for ssid, group in unique_aps.groupby("SSID"):
            macs = group["MAC"].unique()
            if len(macs) > 1:
                multi_mac_ssids += 1

                auth_modes = group["AuthMode"].unique()
                is_open_present    = any("OPEN" in str(a).upper() for a in auth_modes)
                is_secure_present  = any("WPA"  in str(a).upper() or "WEP" in str(a).upper() for a in auth_modes)

                if is_open_present and is_secure_present:
                    evil_twins_detectados += 1
                    console.print(f"  [red bold]⚠ EVIL TWIN ALERT! SSID: '{ssid}'[/red bold]")
                    console.print("    [red]Multiple MACs with security mismatch detected:[/red]")
                    for _, row in group.iterrows():
                        color = "red" if "OPEN" in str(row["AuthMode"]).upper() else "green"
                        console.print(f"    [dim]- MAC:[/dim] [white]{row['MAC']}[/white] [dim]| Security:[/dim] [{color}]{row['AuthMode']}[/{color}]")
                        reporte_rows.append({
                            "SSID":     ssid,
                            "MAC":      row["MAC"],
                            "AuthMode": row["AuthMode"],
                            "Alert":    "Evil Twin",
                        })
                else:
                    for _, row in group.iterrows():
                        reporte_rows.append({
                            "SSID":     ssid,
                            "MAC":      row["MAC"],
                            "AuthMode": row["AuthMode"],
                            "Alert":    "Multi-MAC (possible Corporate/Mesh)",
                        })

        if evil_twins_detectados == 0:
            console.print("  [green]✓[/green] No critical Evil Twin indicators detected (security mismatches).")

        if multi_mac_ssids > 0:
            console.print(f"  [dim]Note: {multi_mac_ssids} SSIDs are being broadcast by multiple MACs (possible Corporate/Mesh networks).[/dim]")

        if reporte_rows:
            self._save_spoofing_report(reporte_rows)

    def _save_spoofing_report(self, rows: list) -> str:
        """Writes a CSV with the spoofing findings and returns the file path."""
        output_path = os.path.join(self.output_dir, f"{self.base_name}_spoofing.csv")
        try:
            with open(output_path, "w", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(f, fieldnames=["SSID", "MAC", "AuthMode", "Alert"])
                writer.writeheader()
                writer.writerows(rows)
            print_success(f"Spoofing report saved: [cyan]{output_path}[/cyan]")
        except Exception as exc:
            print_error(f"Error saving spoofing report: {exc}")
            return ""
        return output_path
