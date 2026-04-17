"""
test_file_utils.py - Unit tests for modules/utils/file_utils.py
"""

import numpy as np
import pandas as pd
import pytest
from unittest.mock import patch

from modules.utils.file_utils import clean_and_validate_data, _manual_csv_loader


class TestCleanAndValidateData:

    def test_removes_rows_with_null_rssi(self, df_with_nulls):
        result = clean_and_validate_data(df_with_nulls.copy())
        assert result["RSSI"].isna().sum() == 0

    def test_removes_rows_with_null_channel(self, df_with_nulls):
        result = clean_and_validate_data(df_with_nulls.copy())
        assert result["Channel"].isna().sum() == 0

    def test_keeps_complete_rows(self, df_with_nulls):
        result = clean_and_validate_data(df_with_nulls.copy())
        # df_with_nulls has 4 rows; Net2 (null RSSI) and Net3 (null Channel) are removed
        assert len(result) == 2

    def test_converts_rssi_to_numeric(self):
        df = pd.DataFrame({
            "SSID":    ["Net1", "Net2"],
            "RSSI":    ["-50", "-70"],   # strings
            "Channel": [1, 6],
        })
        result = clean_and_validate_data(df)
        assert pd.api.types.is_float_dtype(result["RSSI"]) or pd.api.types.is_integer_dtype(result["RSSI"])

    def test_converts_channel_to_numeric(self):
        df = pd.DataFrame({
            "SSID":    ["Net1", "Net2"],
            "RSSI":    [-50.0, -70.0],
            "Channel": ["1", "6"],   # strings
        })
        result = clean_and_validate_data(df)
        assert pd.api.types.is_numeric_dtype(result["Channel"])

    def test_discards_non_numeric_rssi(self):
        df = pd.DataFrame({
            "SSID":    ["Net1", "Net2", "Net3"],
            "RSSI":    [-50.0, "invalid", -70.0],
            "Channel": [1, 6, 11],
        })
        result = clean_and_validate_data(df)
        assert len(result) == 2

    def test_empty_df_returns_empty(self):
        df = pd.DataFrame({"SSID": [], "RSSI": [], "Channel": []})
        result = clean_and_validate_data(df)
        assert result.empty

    def test_missing_rssi_column_raises_key_error(self):
        # dropna(subset=["RSSI", "Channel"]) fails if RSSI does not exist → known bug
        df = pd.DataFrame({"SSID": ["Net1"], "Channel": [1]})
        with pytest.raises(KeyError):
            clean_and_validate_data(df)

    def test_keeps_extra_columns(self, valid_wifi_df):
        result = clean_and_validate_data(valid_wifi_df.copy())
        assert "AuthMode" in result.columns
        assert "CurrentLatitude" in result.columns


class TestManualCsvLoader:

    def test_loads_simple_csv(self, valid_csv):
        df = _manual_csv_loader(valid_csv)
        assert df is not None
        assert len(df) == 3
        assert "SSID" in df.columns

    def test_error_on_short_file(self, short_csv):
        with pytest.raises(ValueError, match="too short"):
            _manual_csv_loader(short_csv)

    def test_fills_missing_columns_with_nan(self, tmp_path):
        # Line with fewer fields than the header
        content = (
            "WigleWifi-1.4,appRelease=2.73\n"
            "MAC,SSID,AuthMode,Channel\n"
            "AA:BB,Net1,WPA2\n"       # Channel missing
            "AA:CC,Net2,WPA2,6\n"
        )
        file_path = tmp_path / "incomplete.csv"
        file_path.write_text(content, encoding="utf-8")

        df = _manual_csv_loader(str(file_path))
        assert len(df) == 2
        # The first row should have NaN in Channel
        assert pd.isna(df.iloc[0]["Channel"]) or df.iloc[0]["Channel"] != df.iloc[0]["Channel"]

    def test_truncates_extra_columns(self, tmp_path):
        # Line with more fields than the header
        content = (
            "WigleWifi-1.4,appRelease=2.73\n"
            "MAC,SSID,Channel\n"
            "AA:BB,Net1,1,extra_field,another_extra\n"
            "AA:CC,Net2,6\n"
        )
        file_path = tmp_path / "extra.csv"
        file_path.write_text(content, encoding="utf-8")

        df = _manual_csv_loader(str(file_path))
        assert df.shape[1] == 3   # only 3 columns from the header
