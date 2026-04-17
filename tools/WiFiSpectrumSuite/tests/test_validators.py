"""
test_validators.py - Unit tests for modules/utils/validators.py
"""

import pytest
from modules.utils.validators import looks_like_date, repair_date_field


class TestLooksLikeDate:

    # --- Cases that SHOULD be recognized as a date ---

    @pytest.mark.parametrize("value", [
        "2024-01-15 10:30:00",
        "2024-01-15",
        "01/15/2024 10:30:00",
        "01/15/2024",
        "15-01-2024 10:30:00",
        "2024/01/15 10:30:00",
        "2023-12-25",
        "2025-06-01 00:00:00",
        "10 Jan 2024",
        "3:00 PM",
        "12:00 am",
    ])
    def test_recognizes_valid_dates(self, value):
        assert looks_like_date(value) is True

    # --- Cases that are NOT dates ---

    @pytest.mark.parametrize("value", [
        "WPA2",
        "WEP",
        "OPEN",
        "HomeNet",
        "192.168.1.1",
        "-65",
        "6",
    ])
    def test_rejects_non_dates(self, value):
        assert looks_like_date(value) is False

    # --- Edge cases ---

    def test_empty_string(self):
        assert looks_like_date("") is False

    def test_only_spaces(self):
        assert looks_like_date("   ") is False

    def test_none(self):
        assert looks_like_date(None) is False

    def test_integer_number(self):
        # A number without a recognizable year is not a date
        assert looks_like_date(42) is False

    def test_recognizable_year(self):
        # Contains "2024" as a substring → True by keywords
        assert looks_like_date("Event 2024") is True


class TestRepairDateField:

    # --- Values that are already valid dates → normalizes to canonical format ---

    def test_iso_format_without_change(self):
        result = repair_date_field("2024-01-15 10:30:00")
        assert result == "2024-01-15 10:30:00"

    def test_day_month_year_format(self):
        result = repair_date_field("15/01/2024 10:30:00")
        assert result == "2024-01-15 10:30:00"

    def test_month_day_year_format(self):
        result = repair_date_field("01/15/2024 10:30:00")
        assert result == "2024-01-15 10:30:00"

    def test_year_slash_format(self):
        result = repair_date_field("2024/01/15 10:30:00")
        assert result == "2024-01-15 10:30:00"

    def test_day_dash_month_year_format(self):
        result = repair_date_field("15-01-2024 10:30:00")
        assert result == "2024-01-15 10:30:00"

    # --- Values that are WiFi protocols → replace with current date ---

    @pytest.mark.parametrize("protocol", ["WPA2", "WPA", "WEP", "OPN", "OPEN", "UNKNOWN", "N/A", "NULL"])
    def test_wifi_protocol_returns_date(self, protocol):
        result = repair_date_field(protocol)
        # Should be parseable as an ISO date
        from datetime import datetime
        parsed = datetime.strptime(result, "%Y-%m-%d %H:%M:%S")
        assert parsed is not None

    # --- Edge cases ---

    def test_empty_string_returns_same_value(self):
        assert repair_date_field("") == ""

    def test_none_returns_none(self):
        assert repair_date_field(None) is None

    def test_unrecognizable_value_returns_original(self):
        value = "unformatted_text"
        assert repair_date_field(value) == value

    def test_lowercase_protocol_is_detected(self):
        # The check uses value_str.upper(), so "wpa2" → "WPA2" which IS in non_date_values
        from datetime import datetime
        result = repair_date_field("wpa2")
        parsed = datetime.strptime(result, "%Y-%m-%d %H:%M:%S")
        assert parsed is not None
