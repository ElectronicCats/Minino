# WiFi Spectrum Suite – Complete User Guide
> **Current Version:** v1.1.0
> **Developed by:** Electronic Cats - Dr. h. c. César A. Peregrino Rodríguez

---

## Table of Contents

- [WiFi Spectrum Suite – Complete User Guide](#wifi-spectrum-suite--complete-user-guide)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
    - [Who is this tool for?](#who-is-this-tool-for)
  - [Project Architecture](#project-architecture)
    - [Directory Structure](#directory-structure)
    - [Main Components](#main-components)
      - [1. **wifi\_spectrum.py** (Entry Point)](#1-wifi_spectrumpy-entry-point)
      - [2. **`modules/`** (Application Core)](#2-modules-application-core)
        - [1. **`core/`**](#1-core)
        - [2. **`utils/`**](#2-utils)
        - [3. **`visualization/`**](#3-visualization)
  - [Capabilities and Features](#capabilities-and-features)
    - [Main Features](#main-features)
  - [Installation](#installation)
    - [Prerequisites](#prerequisites)
    - [Virtual Environment Installation](#virtual-environment-installation)
  - [Quick Start](#quick-start)
    - [Minino Integration - Wardriving](#minino-integration---wardriving)
      - [Minino + WiFi Spectrum Suite Workflow](#minino--wifi-spectrum-suite-workflow)
        - [1. **Scanning with Minino**](#1-scanning-with-minino)
        - [2. **Data Extraction**](#2-data-extraction)
        - [3. **Processing with WiFi Spectrum Suite**](#3-processing-with-wifi-spectrum-suite)
        - [4. **Advanced Visualization**](#4-advanced-visualization)
  - [Suite Functionality](#suite-functionality)
    - [**1. CSV Debugger** (debug)](#1-csv-debugger-debug)
    - [**2. Interference Analyzer** (interference)](#2-interference-analyzer-interference)
    - [**3. Wardriving Analyzer** (wardriving)](#3-wardriving-analyzer-wardriving)
  - [Module Structure](#module-structure)
  - [Installation](#installation-1)
    - [Requirements](#requirements)
    - [Install Dependencies](#install-dependencies)
  - [Usage - Terminal Commands](#usage---terminal-commands)
    - [**Partial Analysis**](#partial-analysis)
      - [1. Repair Format Issues](#1-repair-format-issues)
      - [2. Analyze Interference Only](#2-analyze-interference-only)
      - [3. Generate Heatmap Only](#3-generate-heatmap-only)
      - [4. Generate Location Map](#4-generate-location-map)
      - [5. Generate Graphics](#5-generate-graphics)
      - [6. Generate Wardriving Report](#6-generate-wardriving-report)
    - [**Complete Analysis**](#complete-analysis)
      - [Option A: Total Integrated Analysis (Recommended)](#option-a-total-integrated-analysis-recommended)
      - [Option B: All Wardriving Options](#option-b-all-wardriving-options)
      - [Option C: Recommended Minino Workflow (Step by Step)](#option-c-recommended-minino-workflow-step-by-step)
  - [Input File Format - Expected CSV](#input-file-format---expected-csv)
    - [Recognized Columns:](#recognized-columns)
  - [Output Files](#output-files)
  - [Output Interpretation](#output-interpretation)
    - [RSSI Heatmap](#rssi-heatmap)
    - [Wardriving Graphics](#wardriving-graphics)
    - [Interference Report](#interference-report)
    - [Spoofing Report](#spoofing-report)
    - [Debug/Verbose](#debugverbose)
  - [Use Cases](#use-cases)
    - [Case 1: WiFi Security Audit](#case-1-wifi-security-audit)
    - [Case 2: Coverage Optimization](#case-2-coverage-optimization)
    - [Case 3: Community Mapping (like Wigle)](#case-3-community-mapping-like-wigle)
    - [Case 4: Automatic Minino Pipeline](#case-4-automatic-minino-pipeline)
  - [RSSI Metrics Interpretation](#rssi-metrics-interpretation)
  - [Troubleshooting](#troubleshooting)
    - ["Error: File does not exist"](#error-file-does-not-exist)
    - ["Missing columns / Error processing CSV"](#missing-columns--error-processing-csv)
    - ["HTML maps won't open"](#html-maps-wont-open)
  - [Additional Information](#additional-information)
  - [Summary](#summary)
  - [License](#license)
  - [Credits \& Attribution](#credits--attribution)
    - [Original Scripts:](#original-scripts)

---

## Introduction

WiFi Spectrum Suite is a comprehensive Python tool designed to analyze WiFi scan data, particularly from wardriving activities. It provides functionalities for:
- Repairing CSV files with date format issues
- Analyzing WiFi channel interference
- Generating geospatial visualizations (heatmaps, location maps)
- Detecting evil twin and spoofing attempts
- Creating detailed reports and advanced graphics

### Who is this tool for?

This tool is aimed at:
- **Wardrivers**: Individuals who scan for WiFi networks while moving (e.g., in a car) to map coverage and security.
- **Security Auditors**: Professionals assessing WiFi security and interference in specific areas.
- **Community Mappers**: Contributors to platforms like Wigle.net who want to visualize and analyze their wardriving data.

---

## Project Architecture

### Directory Structure

```text
WiFiSpectrumSuite/
├── generatedFiles/        # Folder for generated outputs (maps, reports, graphs)
├── __init__.py
├── Legacy/                # Original scripts by Dr. h. c. César A. Peregrino Rodríguez
├── modules/               # Contains the main modules of the suite
│   ├── cli.py             # Click-based command-line interface
│   ├── __init__.py
│   ├── core/              # Core analysis functionalities
│   │   ├── csv_debugger.py
│   │   ├── __init__.py
│   │   ├── interference.py
│   │   └── wardriving.py
│   ├── utils/             # Shared utility functions
│   │   ├── file_utils.py
│   │   ├── __init__.py
│   │   ├── output.py
│   │   └── validators.py
│   └── visualization/     # Output generation (charts and maps)
│       ├── __init__.py
│       ├── maps.py
│       └── plots.py
├── pytest.ini
├── README.md
├── requirements.txt
├── setup.py
├── tests/
│   ├── conftest.py
│   ├── test_csv_debugger.py
│   ├── test_file_utils.py
│   ├── test_interference.py
│   ├── test_validators.py
│   └── test_wardriving.py
└── wifi_spectrum.py
```

### Main Components

#### 1. **wifi_spectrum.py** (Entry Point)
Main script that exposes all system functionalities through structured CLI commands via the `wifi-spectrum-suite` entrypoint.

#### 2. **`modules/`** (Application Core)

##### 1. **`core/`**
Contains the three main analysis engines:
- **`csv_debugger.py`** — Detects and repairs date format inconsistencies in CSV files. Supports ISO 8601, US, EU, and compact date formats. Processes files line-by-line for memory efficiency (up to 500 MB).
- **`interference.py`** — Analyzes WiFi channel congestion, RSSI metrics, overlapping channels, and generates optimization recommendations.
- **`wardriving.py`** — The `WardrivingAnalyzer` class. Loads wardriving data with multi-strategy parsing, normalizes column aliases, calculates metrics, detects security anomalies (evil twins, spoofing), and drives all output generation.

##### 2. **`utils/`**
Shared helpers used across modules:
- **`validators.py`** — Date pattern detection (`looks_like_date`) and single-field date repair (`repair_date_field`). Handles non-date sentinel values (WPA2, WEP, OPEN, N/A, NULL).
- **`file_utils.py`** — Fault-tolerant CSV loading (`robust_csv_loader`) with five fallback strategies and data cleaning (`clean_and_validate_data`).
- **`output.py`** — Rich console helpers (`print_success`, `print_warning`, `print_error`, `print_info`) and RSSI-based color coding (`rssi_color`).

##### 3. **`visualization/`**
Generates all visual output:
- **`plots.py`** — PNG chart generation using matplotlib and seaborn. Produces the 4-subplot interference chart and the 6-subplot wardriving analysis.
- **`maps.py`** — Interactive HTML map generation using folium. Produces RSSI heatmaps and network location marker maps.

---

## Capabilities and Features

### Main Features

| Feature | Description |
|---------|-------------|
| **Date Repair** | Detects and fixes 7+ date format inconsistencies in CSV files |
| **Fault-Tolerant Loading** | 5-strategy CSV parser handles malformed, encoded, or partially corrupt files |
| **Channel Interference Analysis** | Identifies congested and overlapping WiFi channels with optimization recommendations |
| **RSSI Signal Classification** | 5-tier quality classification (Excellent → Very Weak) with color-coded console output |
| **Interactive RSSI Heatmap** | Folium/OpenStreetMap heatmap weighted by signal strength |
| **Geographic Location Map** | Network markers grouped by SSID with popup signal stats |
| **Advanced Wardriving Plots** | 6-panel analysis: channel boxplot, temporal evolution, auth distribution, spatial hexbin, and RSSI histogram |
| **Security Analysis** | Detects OPEN, WEP, WPA, WPA2 networks and flags insecure configurations |
| **Evil Twin / Spoofing Detection** | Identifies SSIDs with multiple MACs using mixed security — potential evil twin attacks |
| **Spoofing CSV Report** | Exports suspected spoofing entries with SSID, MAC, AuthMode, and alert classification |
| **Rich Console UX** | Color-coded output, progress feedback, and ASCII banner with random WiFi-themed phrases |
| **Full Pipeline Command** | Single command to run debug → interference → wardriving in sequence |
| **File Size Guard** | Rejects files over 500 MB to prevent memory exhaustion |

---

## Installation

### Prerequisites

- Python 3.12 or higher
- Git installed on the system

### Virtual Environment Installation
Recommended for development or to avoid conflicts with other Python dependencies on the system.
```bash
git clone https://github.com/ElectronicCats/Minino.git
cd tools/WiFiSpectrumSuite
python -m venv .venv
source .venv/bin/activate     # On Windows: .venv\Scripts\activate
pip install -e .
```

**Installation verification:**
```bash
wifi-spectrum-suite --help
```

---

## Quick Start

### Minino Integration - Wardriving

**WiFi Spectrum Suite** is designed to work seamlessly with **[Minino](https://github.com/ElectronicCats/Minino)**, a portable GPS-enabled device for wireless network scanning and wardriving.

#### Minino + WiFi Spectrum Suite Workflow

```
Minino (Scanning) → GPS CSV → Suite (Analysis) → Maps & Reports
```

##### 1. **Scanning with Minino**
   - Make sure you have a strong GPS signal first (be in open spaces).
   - Verify that the **date and time** on the Minino (`Applications` > `GPS` > `Date & Time`) match your time zone
   - Change the time zone if it's not the same as yours (`Settings` > `System` > `GPS` > `Time Zone`)
   - Insert the microSD card (if not, Wardriving won't start).
   - Use the **Wardriving** app on Minino (`Applications` > `GPS` > `Wardrive`)
   - Select **`AP Start`** to begin
   - The device records: `SSID`, `BSSID`, `RSSI`, `channel`, `frequency`, `GPS coordinates`, `time and date`
   - Data is automatically saved as CSV on the microSD card

##### 2. **Data Extraction**
   - To cancel Wardriving, press `Back`
   - Remove the microSD from Minino
   - Insert the microSD card into a computer.
   - Copy the CSV file (usually in `Warfi/` folder for WiFi)
   - Typical file: `Warfi_YYYY-MM-DD_hh-mm-ss.csv` with location columns.

##### 3. **Processing with WiFi Spectrum Suite**
   - **Date Repair**: Automatically fixes format inconsistencies
   - **Interference Analysis**: Identifies channel conflicts
   - **Geospatial Visualization**: Generates interactive maps with network density

##### 4. **Advanced Visualization**
   - RSSI heatmaps by geographic location
   - Location maps with network markers
   - Distribution graphs by channel and frequency
   - Signal strength temporal analysis
   - Detailed reports with recommendations
   - Evil twin and spoofing detection reports

---

## Suite Functionality

### **1. CSV Debugger** (debug)
Automatically detects and repairs common CSV file issues:
- Inconsistent date formats
- Empty or invalid field values
- Non-date sentinel values in date columns (WPA2, OPEN, N/A, NULL → replaced with timestamp)
- Damaged column structures

**Supports multiple date formats:**
- ISO 8601: `2024-01-15 10:30:00`
- US: `01/15/2024 10:30:00`
- EU: `15/01/2024 10:30:00`
- Compact: `20240115103000`

### **2. Interference Analyzer** (interference)
Analyzes WiFi channel patterns and interference:
- Network distribution by channel
- Channel overlap detection (non-overlapping: 1, 6, 11 in 2.4 GHz)
- Average signal strength calculation per channel
- Weak signal network identification (RSSI ≤ −80 dBm)
- Channel optimization recommendations
- Statistical interference graphs (4 subplots)

**Signal quality classification:**
- Excellent: ≥ −50 dBm
- Good: −60 to −50 dBm
- Fair: −70 to −60 dBm
- Weak: −80 to −70 dBm
- Very Weak: < −80 dBm

### **3. Wardriving Analyzer** (wardriving)
Transforms scan data into geospatial visualizations:
- **RSSI Heatmaps**: Visualizes signal intensity by location
- **Location Maps**: Markers for each network with detailed info
- **Advanced Graphics**: Temporal analysis, channel distribution, histograms
- **Security Analysis**: Detection of OPEN, WEP, WPA, WPA2 networks
- **Evil Twin Detection**: Identifies SSIDs with multiple MACs and mixed security profiles
- **Spoofing Report**: CSV export flagging suspect networks with alert classification
- **HTML Reports**: Interactive maps based on Folium/OpenStreetMap

---

## Module Structure

```
modules/
│
├─ core/csv_debugger.py
│  ├─ analyze_date_problems()      → Detects date column issues (first 10 lines)
│  ├─ repair_date_issues()         → Repairs and saves fixed CSV
│  └─ validate_date_repair()       → Re-validates repaired file with pandas
│
├─ core/interference.py
│  └─ analyze_wifi_interference()  → Full interference analysis + plots + report
│
├─ core/wardriving.py  [WardrivingAnalyzer]
│  ├─ load_data()                  → Multi-strategy CSV loading with alias mapping
│  ├─ analyze_general()            → Computes capture period, RSSI stats, top networks
│  ├─ generate_heat_map()          → RSSI intensity heatmap (HTML)
│  ├─ generate_location_map()      → Network markers map (HTML)
│  ├─ generate_plots()             → 6-panel advanced analysis (PNG)
│  ├─ generate_report()            → Detailed console report
│  ├─ _analyze_security()          → Flags OPEN/WEP/WPA2 networks
│  ├─ _analyze_signal_quality()    → 4-tier quality distribution
│  └─ _analyze_spoofing()          → Evil twin and multi-MAC detection
│
├─ utils/validators.py
│  ├─ looks_like_date()            → Pattern & keyword date detection
│  └─ repair_date_field()          → Single-field date repair
│
├─ utils/file_utils.py
│  ├─ robust_csv_loader()          → 5-strategy fault-tolerant loading
│  └─ clean_and_validate_data()    → Type coercion, NaN removal, column validation
│
├─ utils/output.py
│  ├─ print_success/warning/error/info()  → Rich-formatted console helpers
│  └─ rssi_color()                 → RSSI-to-color mapping for Rich
│
├─ visualization/plots.py
│  ├─ generate_interference_plots()  → 4-subplot interference PNG
│  └─ generate_wardriving_plots()    → 6-subplot wardriving PNG
│
└─ visualization/maps.py
   ├─ generate_heat_map()          → RSSI-weighted HTML heatmap (folium)
   └─ generate_location_map()      → CircleMarker network map (folium)
```

---

## Installation

### Requirements
- Python 3.12+
- pip (Python package manager)

### Install Dependencies

```bash
pip install -r requirements.txt
```

Or install the package directly (recommended):

```bash
pip install -e .
```

---

## Usage - Terminal Commands

### **Partial Analysis**

#### 1. Repair Format Issues
```bash
wifi-spectrum-suite debug Warfi_file.csv --validate -o Warfi_file_fixed.csv
```
- Detects date format issues
- Generates `Warfi_file_fixed.csv`
- Repairs dates and validates result

#### 2. Analyze Interference Only
```bash
wifi-spectrum-suite interference wardriving_data_fixed.csv
```
- Generates:
  - `wardriving_data_fixed_analysis.png` (graphs)
  - `wardriving_data_fixed_report.txt` (report)

#### 3. Generate Heatmap Only
```bash
wifi-spectrum-suite wardriving wardriving_data_fixed.csv --heat-map
```
- Generates: `heat_map_wardriving_data_fixed.html` (interactive map)

#### 4. Generate Location Map
```bash
wifi-spectrum-suite wardriving wardriving_data_fixed.csv --location-map
```
- Generates: `location_map_wardriving_data_fixed.html` (network markers)

#### 5. Generate Graphics
```bash
wifi-spectrum-suite wardriving wardriving_data_fixed.csv --plots
```
- Generates: `advanced_plots_wardriving_data_fixed.png` (advanced analysis graphs)

#### 6. Generate Wardriving Report
```bash
wifi-spectrum-suite wardriving wardriving_data_fixed.csv --report
```
- Prints complete analysis to terminal.

### **Complete Analysis**

#### Option A: Total Integrated Analysis (Recommended)
```bash
wifi-spectrum-suite full wardriving_data.csv --validate
```

**Automatically executes:**
1. Date repair → `*_fixed.csv`
2. Interference analysis → graphs and report
3. Complete wardriving (maps, graphs, report)

**Generated files:**
- `wardriving_data_fixed.csv` - Repaired CSV
- `wardriving_data_fixed_report.txt` - Interference report
- `wardriving_data_fixed_analysis.png` - Interference graphs
- `heat_map_wardriving_data_fixed.html` - RSSI intensity map
- `location_map_wardriving_data_fixed.html` - Network location map
- `advanced_plots_wardriving_data_fixed.png` - Temporal and distribution analysis
- `wardriving_data_fixed_spoofing.csv` - Evil twin / spoofing alerts

#### Option B: All Wardriving Options
```bash
wifi-spectrum-suite wardriving wardriving_data_fixed.csv --all
```

**Executes:**
- Detailed report
- Heatmap
- Location map
- Advanced graphics

#### Option C: Recommended Minino Workflow (Step by Step)
```bash
# Step 1: Repair and validate
wifi-spectrum-suite debug minino_wardriving.csv --validate

# Step 2: Analyze interference
wifi-spectrum-suite interference minino_wardriving_fixed.csv

# Step 3: Generate complete wardriving visualization
wifi-spectrum-suite wardriving minino_wardriving_fixed.csv --all
```

---

## Input File Format - Expected CSV

Typical Minino export format:

```csv
Index,SSID,WiFi Address,BSSID,Channel,Frequency,RSSI,Security,FirstSeen,LastSeen,CurrentLatitude,CurrentLongitude,AuthMode
1,MyNetwork,00:11:22:33:44:55,00:11:22:33:44:55,1,2412,-45,WPA2,2024-01-15 10:30:00,2024-01-15 14:45:00,40.7128,-74.0060,WPA2
2,SecureWiFi,AA:BB:CC:DD:EE:FF,AA:BB:CC:DD:EE:FF,6,2437,-62,WPA2,2024-01-15 10:32:15,2024-01-15 14:46:30,40.7129,-74.0061,WPA2
```

### Recognized Columns:
- **SSID** - Network name
- **BSSID / WiFi Address** - MAC address
- **Channel** - WiFi channel (1–13 in 2.4 GHz, 36–165 in 5 GHz)
- **Frequency** - Frequency in MHz
- **RSSI** - Signal strength (dBm, negative values)
- **CurrentLatitude / Latitude** - Geographic coordinate (latitude)
- **CurrentLongitude / Longitude** - Geographic coordinate (longitude)
- **FirstSeen / Timestamp** - Detection date and time
- **AuthMode / Security** - Encryption type (OPEN, WEP, WPA, WPA2)

---

## Output Files

| File | Description |
|------|------------|
| `*_fixed.csv` | Repaired CSV with corrected dates |
| `*_report.txt` | Interference analysis report with recommendations |
| `*_analysis.png` | 4-subplot interference analysis chart |
| `heat_map_*.html` | Interactive RSSI intensity heatmap (folium) |
| `location_map_*.html` | Interactive network location map (folium) |
| `advanced_plots_*.png` | 6-subplot wardriving analysis chart |
| `*_spoofing.csv` | Suspected evil twin and multi-MAC spoofing alerts |

All files are saved to the `generatedFiles/` directory.

---

## Output Interpretation

### RSSI Heatmap
- **Intense red color** → Very strong signal (−40 dBm)
- **Yellow color** → Moderate signal (−70 dBm)
- **Light blue color** → Weak signal (−90 dBm)

### Wardriving Graphics
1. **RSSI Distribution by Channel** - Boxplot showing signal variability per channel
2. **Average RSSI per Channel** - Mean intensity with color coding
3. **Temporal Evolution** - Signal change during scanning session
4. **Authentication Methods** - Security protocol distribution (pie chart)
5. **Density & RSSI (Hexbin)** - Geographic concentration and signal strength
6. **RSSI Histogram** - Overall intensity distribution

### Interference Report
- **Total networks detected**
- **Unique networks by SSID**
- **Channels used**
- **Channel distribution**
- **Problematic channels** (overlapping with non-optimal channels)
- **Strategic recommendations** (2.4 GHz optimization, 5 GHz migration, QoS)

### Spoofing Report
The `*_spoofing.csv` file contains:
- **SSID** - Network name flagged for analysis
- **MAC** - MAC address associated with the SSID
- **AuthMode** - Observed authentication method
- **Alert** - Classification: `evil_twin` (mixed security across MACs) or `multi_mac` (corporate/mesh networks with multiple MACs)

> **Note:** Multi-MAC entries are not necessarily malicious — they may indicate mesh networks or enterprise APs. Review `evil_twin` alerts first.

### Debug/Verbose
The suite prints detailed progress at each phase:
- Lines processed
- Errors detected
- Generated files
- Analysis statistics

---

## Use Cases

### Case 1: WiFi Security Audit
```bash
wifi-spectrum-suite interference scan.csv
# Identifies insecure networks and saturated channels
```

### Case 2: Coverage Optimization
```bash
wifi-spectrum-suite wardriving scan.csv --heat-map --plots
# Visualizes coverage gaps
```

### Case 3: Community Mapping (like Wigle)
```bash
wifi-spectrum-suite wardriving Warfi_file_fixed.csv --all
# Generates interactive maps for community analysis
```

### Case 4: Automatic Minino Pipeline
```bash
wifi-spectrum-suite full Warfi_file.csv
# Processes end-to-end: repair → interference → wardriving
```

---

## RSSI Metrics Interpretation

| RSSI (dBm) | Quality | Recommendation |
|------------|---------|----------------|
| −30 to −50 | Excellent | Optimal |
| −50 to −60 | Very Good | Normal |
| −60 to −70 | Good | Acceptable |
| −70 to −80 | Fair | Consider repeater |
| −80 to −90 | Weak | Poor coverage |
| < −90 | Very Weak | Unusable |

---

## Troubleshooting

### "Error: File does not exist"
```bash
# Verify the path is correct
ls Warfi.csv
# Or use absolute path
wifi-spectrum-suite full /full/path/file.csv
```

### "Missing columns / Error processing CSV"
```bash
# The loader attempts automatic column mapping across 5 strategies.
# If it fails, ensure the CSV contains at least: SSID, Channel, RSSI, BSSID, FirstSeen, CurrentLatitude, CurrentLongitude
```

### "HTML maps won't open"
```bash
# Open in browser manually
# Linux:   xdg-open heat_map_file.html
# Mac:     open heat_map_file.html
# Windows: start heat_map_file.html
```

---

## Additional Information

- **Original Repository**: [WiFi-Spectrum-Suite by DarkAlguty](https://github.com/DarkAlguty/WiFi-Spectrum-Suite)
- **Minino Wiki**: [Minino 4.4 GPS & Wardriving](https://github.com/ElectronicCats/Minino/wiki/4.4-GPS#wardriving)
- **WiGLE.net**: [Upload and visualize data](https://wigle.net/)

---

## Summary

**WiFi Spectrum Suite** transforms raw WiFi scan data into:
- Detailed analysis reports
- Interactive geospatial maps
- Statistical graphics
- Evil twin and spoofing detection
- Optimization recommendations

**Perfect integration with Minino** for end-to-end wardriving workflows.

---

**Status**: Active | **Last Updated**: April 2026 | **Python**: 3.12+

---

## License
This project is licensed under the terms specified in the official repository. Check the [LICENSE](../../LICENSE) file for more details.

---

## Credits & Attribution

This project integrates **three original scripts** created by **Dr. h. c. César A. Peregrino Rodríguez**:

### Original Scripts:
1. **depurador_csv** - Automatic detection and repair of date format issues in WiFi survey CSV files
2. **Analisis_Interferencias** - Deep analysis of interference between WiFi channels and optimization recommendations
3. **WiFi_Wardriving** - Complete geospatial data analysis with heatmaps and network density visualization

**Original Repository:** 🔗 [WiFi-Spectrum-Suite by DarkAlguty](https://github.com/DarkAlguty/WiFi-Spectrum-Suite)
