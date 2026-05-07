"""
validators.py - Validation and repair of individual date values
"""

import re
from datetime import datetime


def looks_like_date(value) -> bool:
    """Determines if a value looks like a date."""
    if not value or str(value).strip() == "":
        return False

    value_str = str(value).strip()

    date_patterns = [
        r"\d{4}-\d{2}-\d{2}",
        r"\d{2}/\d{2}/\d{4}",
        r"\d{2}-\d{2}-\d{4}",
        r"\d{4}/\d{2}/\d{2}",
    ]

    if any(re.search(p, value_str) for p in date_patterns):
        return True

    date_keywords = [
        "2024", "2025", "2026", "2023",
        "jan", "feb", "mar", "apr", "may", "jun",
        "jul", "aug", "sep", "oct", "nov", "dec",
        "am", "pm",
    ]
    return any(kw in value_str.lower() for kw in date_keywords)


def repair_date_field(value) -> str:
    """Repairs a single field that should be a date."""
    if not value or str(value).strip() == "":
        return value

    value_str = str(value).strip()

    non_date_values = {"WPA2", "WPA", "WEP", "OPN", "OPEN", "UNKNOWN", "N/A", "NULL"}
    if value_str.upper() in non_date_values:
        return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    formats_to_try = [
        "%Y-%m-%d %H:%M:%S",
        "%d/%m/%Y %H:%M:%S",
        "%m/%d/%Y %H:%M:%S",
        "%Y/%m/%d %H:%M:%S",
        "%d-%m-%Y %H:%M:%S",
        "%m-%d-%Y %H:%M:%S",
        "%Y%m%d%H%M%S",
    ]

    for fmt in formats_to_try:
        try:
            return datetime.strptime(value_str, fmt).strftime("%Y-%m-%d %H:%M:%S")
        except ValueError:
            continue

    return value_str
