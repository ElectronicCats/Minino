"""
conftest.py - Fixtures compartidos para todos los tests
"""

import textwrap
from unittest.mock import MagicMock, patch

import pandas as pd
import pytest


# ---------------------------------------------------------------------------
# Fixture: silenciar Rich console en todos los tests
# ---------------------------------------------------------------------------

@pytest.fixture(autouse=True)
def silenciar_consola():
    """Suprime toda la salida de Rich para mantener el output de pytest limpio."""
    with (
        patch("modules.utils.output.console") as mock_console,
        patch("modules.utils.output.print_error"),
        patch("modules.utils.output.print_warning"),
        patch("modules.utils.output.print_success"),
    ):
        mock_console.print = MagicMock()
        mock_console.rule = MagicMock()
        yield


# ---------------------------------------------------------------------------
# Fixtures: DataFrames de prueba
# ---------------------------------------------------------------------------

@pytest.fixture
def df_wifi_valido():
    """DataFrame con datos WiFi completos y válidos."""
    return pd.DataFrame({
        "SSID":             ["RedCasa", "Oficina_WiFi", "RedVecino", "CafeNet", "MiRed"],
        "RSSI":             [-45.0, -62.0, -75.0, -82.0, -91.0],
        "Channel":          [1, 6, 11, 6, 1],
        "Frequency":        [2412, 2437, 2462, 2437, 2412],
        "CurrentLatitude":  [19.4326, 19.4327, 19.4328, 19.4329, 19.4330],
        "CurrentLongitude": [-99.1332, -99.1333, -99.1334, -99.1335, -99.1336],
        "AuthMode":         ["WPA2", "WPA2", "WEP", "OPEN", "WPA2"],
        "FirstSeen":        [
            "2024-01-15 10:00:00",
            "2024-01-15 10:01:00",
            "2024-01-15 10:02:00",
            "2024-01-15 10:03:00",
            "2024-01-15 10:04:00",
        ],
    })


@pytest.fixture
def df_con_nulos():
    """DataFrame con algunos valores nulos en columnas críticas."""
    return pd.DataFrame({
        "SSID":    ["Red1", "Red2", "Red3", "Red4"],
        "RSSI":    [-50.0, None, -70.0, -80.0],
        "Channel": [1, 6, None, 11],
    })


@pytest.fixture
def df_canales_interferencia():
    """DataFrame con redes en canales superpuestos para pruebas de interferencia."""
    return pd.DataFrame({
        "SSID":    [f"Red{i}" for i in range(20)],
        "RSSI":    [-50.0] * 5 + [-65.0] * 5 + [-75.0] * 5 + [-85.0] * 5,
        "Channel": [1] * 5 + [6] * 5 + [11] * 5 + [3] * 5,  # Canal 3 interfiere
    })


# ---------------------------------------------------------------------------
# Fixtures: archivos CSV temporales
# ---------------------------------------------------------------------------

CABECERA_WIGLE = "WigleWifi-1.4,appRelease=2.73\n"

CSV_VALIDO = textwrap.dedent("""\
    WigleWifi-1.4,appRelease=2.73
    MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude
    AA:BB:CC:DD:EE:01,RedCasa,WPA2,2024-01-15 10:00:00,1,2412,-45,19.4326,-99.1332
    AA:BB:CC:DD:EE:02,Oficina_WiFi,WPA2,2024-01-15 10:01:00,6,2437,-62,19.4327,-99.1333
    AA:BB:CC:DD:EE:03,RedVecino,WEP,2024-01-15 10:02:00,11,2462,-75,19.4328,-99.1334
""")

CSV_CON_FECHAS_ROTAS = textwrap.dedent("""\
    WigleWifi-1.4,appRelease=2.73
    MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude
    AA:BB:CC:DD:EE:01,RedCasa,WPA2,WPA2,1,2412,-45,19.4326,-99.1332
    AA:BB:CC:DD:EE:02,Oficina_WiFi,WPA2,2024-01-15 10:01:00,6,2437,-62,19.4327,-99.1333
""")

CSV_CORTO = "WigleWifi-1.4,appRelease=2.73\n"


@pytest.fixture
def csv_valido(tmp_path):
    archivo = tmp_path / "wifi_test.csv"
    archivo.write_text(CSV_VALIDO, encoding="utf-8")
    return str(archivo)


@pytest.fixture
def csv_con_fechas_rotas(tmp_path):
    archivo = tmp_path / "wifi_fechas_rotas.csv"
    archivo.write_text(CSV_CON_FECHAS_ROTAS, encoding="utf-8")
    return str(archivo)


@pytest.fixture
def csv_corto(tmp_path):
    archivo = tmp_path / "wifi_corto.csv"
    archivo.write_text(CSV_CORTO, encoding="utf-8")
    return str(archivo)
