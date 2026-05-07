"""
test_csv_debugger.py - Unit tests for modules/core/csv_debugger.py
"""

import pytest
from unittest.mock import patch

from modules.core.csv_debugger import (
    _find_date_columns,
    analyze_date_problems,
    repair_date_issues,
)


class TestFindDateColumns:

    def test_detects_time_column(self):
        headers = ["MAC", "SSID", "FirstTime", "Channel"]
        result = _find_date_columns(headers)
        names = [name for _, name in result]
        assert "FirstTime" in names

    def test_detects_date_column(self):
        headers = ["MAC", "SSID", "ScanDate", "RSSI"]
        result = _find_date_columns(headers)
        names = [name for _, name in result]
        assert "ScanDate" in names

    def test_detects_firstseen_and_lastseen(self):
        headers = ["MAC", "SSID", "FirstSeen", "LastSeen", "Channel"]
        result = _find_date_columns(headers)
        names = [name for _, name in result]
        assert "FirstSeen" in names
        assert "LastSeen" in names

    def test_does_not_detect_normal_columns(self):
        headers = ["MAC", "SSID", "Channel", "RSSI", "AuthMode"]
        result = _find_date_columns(headers)
        assert result == []

    def test_detects_by_first_keyword(self):
        headers = ["FirstSeen", "LastSeen", "RSSI"]
        result = _find_date_columns(headers)
        assert len(result) == 2

    def test_returns_correct_indices(self):
        headers = ["MAC", "FirstSeen", "SSID", "LastSeen"]
        result = _find_date_columns(headers)
        indices = {name: idx for idx, name in result}
        assert indices["FirstSeen"] == 1
        assert indices["LastSeen"] == 3

    def test_case_insensitive(self):
        # "FIRSTSEEN" contains "FIRST" → should be detected
        headers = ["MAC", "FIRSTSEEN", "SSID"]
        result = _find_date_columns(headers)
        assert len(result) == 1

    def test_empty_list(self):
        assert _find_date_columns([]) == []


class TestAnalyzeDateProblems:

    def test_returns_headers_on_valid_csv(self, valid_csv):
        headers, _, _ = analyze_date_problems(valid_csv)
        assert headers is not None
        assert "SSID" in headers
        assert "FirstSeen" in headers

    def test_detects_date_columns(self, valid_csv):
        _, _, date_columns = analyze_date_problems(valid_csv)
        names = [name for _, name in date_columns]
        assert "FirstSeen" in names

    def test_csv_without_date_problems(self, valid_csv):
        _, problematic, _ = analyze_date_problems(valid_csv)
        assert len(problematic) == 0

    def test_detects_broken_values(self, broken_dates_csv):
        _, problematic, _ = analyze_date_problems(broken_dates_csv)
        # "WPA2" in FirstSeen does not look like a date
        assert len(problematic) > 0

    def test_returns_none_on_nonexistent_file(self):
        headers, problematic, date_cols = analyze_date_problems("/does/not/exist.csv")
        assert headers is None
        assert problematic == []
        assert date_cols == []

    def test_returns_none_on_very_short_file(self, short_csv):
        headers, _, _ = analyze_date_problems(short_csv)
        assert headers is None


class TestRepairDateIssues:

    def test_generates_repaired_file(self, broken_dates_csv, tmp_path):
        output = str(tmp_path / "repaired.csv")
        result = repair_date_issues(broken_dates_csv, output_file=output)
        assert result == output

    def test_repaired_file_exists(self, broken_dates_csv, tmp_path):
        import os
        output = str(tmp_path / "repaired.csv")
        repair_date_issues(broken_dates_csv, output_file=output)
        assert os.path.exists(output)

    def test_default_output_name(self, broken_dates_csv, tmp_path):
        import os
        result = repair_date_issues(broken_dates_csv, output_dir=str(tmp_path))
        assert result is not None
        assert result.endswith("_fixed.csv")
        assert os.path.exists(result)

    def test_returns_none_on_nonexistent_file(self):
        result = repair_date_issues("/does/not/exist.csv")
        assert result is None

    def test_preserves_lines(self, valid_csv, tmp_path):
        output = str(tmp_path / "repaired.csv")
        repair_date_issues(valid_csv, output_file=output)
        with open(output, encoding="utf-8") as f:
            lines = f.readlines()
        # The CSV has 2 header lines + 3 data lines = 5 lines
        assert len(lines) == 5
