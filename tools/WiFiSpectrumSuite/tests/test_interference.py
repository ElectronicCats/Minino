"""
test_interference.py - Unit tests for modules/core/interference.py
"""

import pandas as pd
import pytest
from unittest.mock import patch

from modules.core.interference import (
    _classify_signal,
    _format_channels_list,
    _generate_comprehensive_analysis,
    _NON_OVERLAPPING,
)


class TestClassifySignal:

    @pytest.mark.parametrize("rssi,expected", [
        (-49,  "Excellent"),
        (-50,  "Excellent"),
        (-51,  "Good"),
        (-59,  "Good"),
        (-60,  "Good"),
        (-61,  "Fair"),
        (-69,  "Fair"),
        (-70,  "Fair"),
        (-71,  "Weak"),
        (-79,  "Weak"),
        (-80,  "Weak"),
        (-81,  "Very weak"),
        (-100, "Very weak"),
    ])
    def test_classification_by_threshold(self, rssi, expected):
        assert _classify_signal(rssi) == expected

    def test_perfect_signal(self):
        assert _classify_signal(0) == "Excellent"

    def test_very_far_signal(self):
        assert _classify_signal(-120) == "Very weak"


class TestFormatChannelsList:

    def test_converts_strings_to_int(self):
        result = _format_channels_list(["1", "6", "11"])
        assert result == [1, 6, 11]

    def test_converts_floats_to_int(self):
        result = _format_channels_list([1.0, 6.0, 11.0])
        assert result == [1, 6, 11]

    def test_empty_list(self):
        assert _format_channels_list([]) == []

    def test_return_type_is_list(self):
        assert isinstance(_format_channels_list([1, 6]), list)

    def test_all_elements_are_int(self):
        # int() does not accept "7.0" as string → pass already convertible values
        result = _format_channels_list(["3", 7, 11])
        assert all(isinstance(c, int) for c in result)


class TestNonOverlapping:

    def test_non_overlapping_channels_are_correct(self):
        assert _NON_OVERLAPPING == [1, 6, 11]

    def test_is_list(self):
        assert isinstance(_NON_OVERLAPPING, list)


class TestGenerateComprehensiveAnalysis:

    def test_returns_string(self, interference_channels_df):
        df = interference_channels_df.copy()
        result = _generate_comprehensive_analysis(df)
        assert isinstance(result, str)

    def test_contains_executive_summary(self, interference_channels_df):
        result = _generate_comprehensive_analysis(interference_channels_df)
        assert "EXECUTIVE ANALYSIS SUMMARY" in result

    def test_contains_total_networks(self, interference_channels_df):
        result = _generate_comprehensive_analysis(interference_channels_df)
        assert "20" in result  # 20 networks in the fixture

    def test_contains_non_overlapping_channels(self, interference_channels_df):
        result = _generate_comprehensive_analysis(interference_channels_df)
        assert "Channel 1" in result
        assert "Channel 6" in result
        assert "Channel 11" in result

    def test_contains_recommendations(self, interference_channels_df):
        result = _generate_comprehensive_analysis(interference_channels_df)
        assert "RECOMMENDATIONS" in result

    def test_calculates_weak_signal_percentage(self, interference_channels_df):
        df = interference_channels_df.copy()
        result = _generate_comprehensive_analysis(df)
        # 5 networks with RSSI -85 are <= -80 → 25%
        assert "25.0%" in result

    def test_with_all_strong_signals(self):
        df = pd.DataFrame({
            "SSID":    [f"Net{i}" for i in range(5)],
            "RSSI":    [-45.0] * 5,
            "Channel": [1, 1, 6, 6, 11],
        })
        result = _generate_comprehensive_analysis(df)
        assert "0 networks (0.0% of total)" in result
