"""
conftest.py - Shared fixtures for all tests
"""

import textwrap
from unittest.mock import MagicMock, patch

import pandas as pd
import pytest


# ---------------------------------------------------------------------------
# Fixture: silence Rich console in all tests
# ---------------------------------------------------------------------------

@pytest.fixture(autouse=True)
def silence_console():
    """Suppresses all Rich output to keep pytest output clean."""
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
# Fixtures: Test DataFrames
# ---------------------------------------------------------------------------

@pytest.fixture
def valid_wifi_df():
    """DataFrame with complete and valid WiFi data."""
    return pd.DataFrame({
        "SSID":             ["HomeNet", "Office_WiFi", "NeighborNet", "CafeNet", "MyNet"],
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
def df_with_nulls():
    """DataFrame with some null values in critical columns."""
    return pd.DataFrame({
        "SSID":    ["Net1", "Net2", "Net3", "Net4"],
        "RSSI":    [-50.0, None, -70.0, -80.0],
        "Channel": [1, 6, None, 11],
    })


@pytest.fixture
def interference_channels_df():
    """DataFrame with networks on overlapping channels for interference testing."""
    return pd.DataFrame({
        "SSID":    [f"Net{i}" for i in range(20)],
        "RSSI":    [-50.0] * 5 + [-65.0] * 5 + [-75.0] * 5 + [-85.0] * 5,
        "Channel": [1] * 5 + [6] * 5 + [11] * 5 + [3] * 5,  # Channel 3 interferes
    })


# ---------------------------------------------------------------------------
# Fixtures: Temporary CSV files
# ---------------------------------------------------------------------------

WIGLE_HEADER = "WigleWifi-1.4,appRelease=2.73\n"

VALID_CSV = textwrap.dedent("""\
    WigleWifi-1.4,appRelease=2.73
    MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude
    AA:BB:CC:DD:EE:01,HomeNet,WPA2,2024-01-15 10:00:00,1,2412,-45,19.4326,-99.1332
    AA:BB:CC:DD:EE:02,Office_WiFi,WPA2,2024-01-15 10:01:00,6,2437,-62,19.4327,-99.1333
    AA:BB:CC:DD:EE:03,NeighborNet,WEP,2024-01-15 10:02:00,11,2462,-75,19.4328,-99.1334
""")

CSV_WITH_BROKEN_DATES = textwrap.dedent("""\
    WigleWifi-1.4,appRelease=2.73
    MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude
    AA:BB:CC:DD:EE:01,HomeNet,WPA2,WPA2,1,2412,-45,19.4326,-99.1332
    AA:BB:CC:DD:EE:02,Office_WiFi,WPA2,2024-01-15 10:01:00,6,2437,-62,19.4327,-99.1333
""")

SHORT_CSV = "WigleWifi-1.4,appRelease=2.73\n"


@pytest.fixture
def valid_csv(tmp_path):
    file_path = tmp_path / "wifi_test.csv"
    file_path.write_text(VALID_CSV, encoding="utf-8")
    return str(file_path)


@pytest.fixture
def broken_dates_csv(tmp_path):
    file_path = tmp_path / "wifi_broken_dates.csv"
    file_path.write_text(CSV_WITH_BROKEN_DATES, encoding="utf-8")
    return str(file_path)


@pytest.fixture
def short_csv(tmp_path):
    file_path = tmp_path / "wifi_short.csv"
    file_path.write_text(SHORT_CSV, encoding="utf-8")
    return str(file_path)
