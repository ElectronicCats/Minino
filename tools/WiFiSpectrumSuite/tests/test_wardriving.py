"""
test_wardriving.py - Unit tests for modules/core/wardriving.py
"""

import pandas as pd
import pytest
from unittest.mock import MagicMock, call, patch

from modules.core.wardriving import WardrivingAnalyzer, _COLUMN_ALIASES


class TestColumnAliases:

    def test_contains_canonical_columns(self):
        expected = {"SSID", "MAC", "FirstSeen", "Channel", "Frequency", "RSSI",
                     "CurrentLatitude", "CurrentLongitude", "AuthMode"}
        assert set(_COLUMN_ALIASES.keys()) == expected

    def test_ssid_has_alias(self):
        assert len(_COLUMN_ALIASES["SSID"]) >= 1
        assert "SSID" in _COLUMN_ALIASES["SSID"]

    def test_rssi_has_signal_alias(self):
        assert "Signal" in _COLUMN_ALIASES["RSSI"]

    def test_latitude_has_alias(self):
        assert "Latitude" in _COLUMN_ALIASES["CurrentLatitude"]

    def test_longitude_has_alias(self):
        assert "Longitude" in _COLUMN_ALIASES["CurrentLongitude"]


class TestWardrivingAnalyzerMapColumns:

    def _analyzer_with_df(self, df, csv_path="fake.csv"):
        analyzer = WardrivingAnalyzer(csv_path)
        analyzer.df = df.copy()
        return analyzer

    def test_maps_signal_to_rssi(self):
        df = pd.DataFrame({"SSID": ["Net1"], "Signal": [-50.0], "Channel": [1]})
        analyzer = self._analyzer_with_df(df)
        analyzer._map_columns()
        assert "RSSI" in analyzer.df.columns

    def test_maps_latitude_to_currentlatitude(self):
        df = pd.DataFrame({
            "SSID": ["Net1"], "RSSI": [-50.0], "Channel": [1],
            "Latitude": [19.43], "Longitude": [-99.13],
        })
        analyzer = self._analyzer_with_df(df)
        analyzer._map_columns()
        assert "CurrentLatitude" in analyzer.df.columns

    def test_does_not_overwrite_existing_column(self):
        df = pd.DataFrame({
            "SSID": ["Net1"], "RSSI": [-50.0], "Signal": [-99.0], "Channel": [1],
        })
        analyzer = self._analyzer_with_df(df)
        analyzer._map_columns()
        # RSSI already exists → should not be overwritten by Signal
        assert analyzer.df["RSSI"].iloc[0] == -50.0

    def test_without_available_aliases_does_not_fail(self):
        df = pd.DataFrame({"SSID": ["Net1"], "RSSI": [-50.0], "Channel": [1]})
        analyzer = self._analyzer_with_df(df)
        analyzer._map_columns()   # Should not throw exception


class TestWardrivingAnalyzerConvertTypes:

    def _analyzer_with_df(self, df):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = df.copy()
        return analyzer

    def test_converts_channel_to_int(self):
        df = pd.DataFrame({
            "SSID": ["Net1"], "RSSI": [-50.0],
            "Channel": ["6"], "Frequency": ["2437"], "FirstSeen": ["2024-01-15 10:00:00"],
        })
        analyzer = self._analyzer_with_df(df)
        analyzer._convert_types()
        assert analyzer.df["Channel"].dtype in (int, "int64", "int32")

    def test_converts_rssi_to_numeric(self):
        # pd.to_numeric can infer int or float depending on the value
        df = pd.DataFrame({
            "SSID": ["Net1"], "RSSI": ["-50"],
            "Channel": [6], "Frequency": [2437], "FirstSeen": ["2024-01-15 10:00:00"],
        })
        analyzer = self._analyzer_with_df(df)
        analyzer._convert_types()
        assert pd.api.types.is_numeric_dtype(analyzer.df["RSSI"])

    def test_creates_timestamp_column(self):
        df = pd.DataFrame({
            "SSID": ["Net1"], "RSSI": [-50.0],
            "Channel": [6], "Frequency": [2437],
            "FirstSeen": ["2024-01-15 10:00:00"],
        })
        analyzer = self._analyzer_with_df(df)
        analyzer._convert_types()
        assert "Timestamp" in analyzer.df.columns

    def test_removes_rows_without_coordinates(self):
        df = pd.DataFrame({
            "SSID": ["Net1", "Net2"],
            "RSSI": [-50.0, -60.0],
            "Channel": [1, 6],
            "Frequency": [2412, 2437],
            "FirstSeen": ["2024-01-15 10:00:00", "2024-01-15 10:01:00"],
            "CurrentLatitude":  [19.43, None],
            "CurrentLongitude": [-99.13, None],
        })
        analyzer = self._analyzer_with_df(df)
        analyzer._convert_types()
        assert len(analyzer.df) == 1


class TestWardrivingAnalyzerAnalyzeGeneral:

    def test_returns_dict_with_expected_keys(self, valid_wifi_df):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = valid_wifi_df.copy()
        result = analyzer.analyze_general()

        assert "total_records" in result
        assert "unique_networks" in result
        assert "top_networks" in result
        assert "rssi_metrics" in result

    def test_total_records_correct(self, valid_wifi_df):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = valid_wifi_df.copy()
        result = analyzer.analyze_general()
        assert result["total_records"] == 5

    def test_unique_networks_correct(self, valid_wifi_df):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = valid_wifi_df.copy()
        result = analyzer.analyze_general()
        assert result["unique_networks"] == 5

    def test_rssi_metrics_correct(self, valid_wifi_df):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = valid_wifi_df.copy()
        result = analyzer.analyze_general()
        rssi = result["rssi_metrics"]

        assert rssi["minimum"] == -91.0
        assert rssi["maximum"] == -45.0
        assert abs(rssi["average"] - (-71.0)) < 0.1

    def test_null_df_returns_empty_dict(self):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = None
        assert analyzer.analyze_general() == {}

    def test_empty_df_returns_empty_dict(self):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = pd.DataFrame()
        assert analyzer.analyze_general() == {}

    def test_top_networks_is_dict(self, valid_wifi_df):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = valid_wifi_df.copy()
        result = analyzer.analyze_general()
        assert isinstance(result["top_networks"], dict)


class TestWardrivingAnalyzerLoadData:

    def test_returns_false_if_file_does_not_exist(self):
        analyzer = WardrivingAnalyzer("/does/not/exist/file.csv")
        assert analyzer.load_data() is False

    def test_returns_true_with_valid_csv(self, valid_csv):
        analyzer = WardrivingAnalyzer(valid_csv)
        assert analyzer.load_data() is True

    def test_loaded_df_is_not_none(self, valid_csv):
        analyzer = WardrivingAnalyzer(valid_csv)
        analyzer.load_data()
        assert analyzer.df is not None

    def test_df_has_data(self, valid_csv):
        analyzer = WardrivingAnalyzer(valid_csv)
        analyzer.load_data()
        assert len(analyzer.df) > 0


class TestAnalyzeSpoofing:

    def _analyzer_with_df(self, df, output_dir="."):
        analyzer = WardrivingAnalyzer("fake.csv", output_dir=output_dir)
        analyzer.df = df.copy()
        return analyzer

    def _calls_str(self, mock_console):
        return " ".join(str(c) for c in mock_console.print.call_args_list)

    def test_no_spoofing_no_evil_twin_alert(self, tmp_path):
        """SSIDs with a single MAC do not generate an Evil Twin alert."""
        df = pd.DataFrame({
            "SSID":     ["HomeNet", "Office", "CafeNet"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02", "AA:BB:CC:DD:EE:03"],
            "AuthMode": ["WPA2", "WPA2", "WPA2"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analyze_spoofing()
            assert "EVIL TWIN ALERT!" not in self._calls_str(mock_console)

    def test_evil_twin_open_vs_wpa2(self, tmp_path):
        """SSID with OPEN and WPA2 on different MACs should trigger an Evil Twin alert."""
        df = pd.DataFrame({
            "SSID":     ["PublicNet", "PublicNet"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analyze_spoofing()
            assert "EVIL TWIN ALERT!" in self._calls_str(mock_console)

    def test_evil_twin_wep_vs_open(self, tmp_path):
        """WEP + OPEN on the same SSID should also detect an Evil Twin."""
        df = pd.DataFrame({
            "SSID":     ["LegacyNet", "LegacyNet"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["WEP", "OPEN"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analyze_spoofing()
            assert "EVIL TWIN ALERT!" in self._calls_str(mock_console)

    def test_multi_mac_same_auth_is_not_evil_twin(self, tmp_path):
        """Multiple MACs with the same security (mesh/corporate) is not an Evil Twin."""
        df = pd.DataFrame({
            "SSID":     ["CorpWiFi"] * 3,
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02", "AA:BB:CC:DD:EE:03"],
            "AuthMode": ["WPA2"] * 3,
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analyze_spoofing()
            assert "EVIL TWIN ALERT!" not in self._calls_str(mock_console)

    def test_multi_mac_note_appears(self, tmp_path):
        """If there are SSIDs with multiple MACs, the informative note should be shown."""
        df = pd.DataFrame({
            "SSID":     ["CorpWiFi", "CorpWiFi"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02"],
            "AuthMode": ["WPA2", "WPA2"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analyze_spoofing()
            assert "multiple MACs" in self._calls_str(mock_console)

    def test_df_only_nulls_does_not_fail(self, tmp_path):
        """DataFrame with only NaN in key columns should not raise an exception."""
        df = pd.DataFrame({
            "SSID":     [None, None],
            "MAC":      [None, None],
            "AuthMode": [None, None],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        analyzer._analyze_spoofing()

    def test_message_without_indicators_when_no_evil_twin(self, tmp_path):
        """When there are no Evil Twins, the absence of indicators message is printed."""
        df = pd.DataFrame({
            "SSID":     ["HomeNet"],
            "MAC":      ["AA:BB:CC:DD:EE:01"],
            "AuthMode": ["WPA2"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analyze_spoofing()
            assert "indicators" in self._calls_str(mock_console).lower()

    # ------------------------------------------------------------------
    # CSV report generation tests
    # ------------------------------------------------------------------

    def test_without_multi_mac_does_not_generate_csv(self, tmp_path):
        """With single MAC SSIDs, no CSV file is generated."""
        df = pd.DataFrame({
            "SSID":     ["HomeNet", "Office"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02"],
            "AuthMode": ["WPA2", "WPA2"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        analyzer._analyze_spoofing()
        assert not any(tmp_path.iterdir())

    def test_evil_twin_generates_csv(self, tmp_path):
        """When an Evil Twin is detected, the CSV should be generated in output_dir."""
        df = pd.DataFrame({
            "SSID":     ["PublicNet", "PublicNet"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        analyzer._analyze_spoofing()
        files = list(tmp_path.iterdir())
        assert len(files) == 1
        assert files[0].suffix == ".csv"

    def test_csv_contains_evil_twin_alert(self, tmp_path):
        """The CSV generated for an Evil Twin should contain the Alert column with 'Evil Twin'."""
        df = pd.DataFrame({
            "SSID":     ["PublicNet", "PublicNet"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        analyzer._analyze_spoofing()
        csv_file = next(tmp_path.iterdir())
        content = csv_file.read_text(encoding="utf-8")
        assert "Evil Twin" in content
        assert "PublicNet" in content

    def test_csv_contains_multi_mac_without_evil_twin(self, tmp_path):
        """The CSV generated for a mesh/corporate SSID should reflect its alert type."""
        df = pd.DataFrame({
            "SSID":     ["CorpWiFi", "CorpWiFi"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02"],
            "AuthMode": ["WPA2", "WPA2"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        analyzer._analyze_spoofing()
        csv_file = next(tmp_path.iterdir())
        content = csv_file.read_text(encoding="utf-8")
        assert "Multi-MAC" in content
        assert "CorpWiFi" in content

    def test_csv_has_correct_headers(self, tmp_path):
        """The CSV must contain the four defined columns: SSID, MAC, AuthMode, Alert."""
        df = pd.DataFrame({
            "SSID":     ["PublicNet", "PublicNet"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = self._analyzer_with_df(df, output_dir=str(tmp_path))
        analyzer._analyze_spoofing()
        csv_file = next(tmp_path.iterdir())
        first_line = csv_file.read_text(encoding="utf-8").splitlines()[0]
        assert first_line == "SSID,MAC,AuthMode,Alert"

    def test_csv_name_includes_base_name(self, tmp_path):
        """The name of the CSV file must include the base name of the source CSV."""
        df = pd.DataFrame({
            "SSID":     ["PublicNet", "PublicNet"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = WardrivingAnalyzer("my_data.csv", output_dir=str(tmp_path))
        analyzer.df = df.copy()
        analyzer._analyze_spoofing()
        files = [f.name for f in tmp_path.iterdir()]
        assert any("my_data" in name for name in files)
