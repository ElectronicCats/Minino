# WiFi Spectrum Suite 📶

**An integrated suite that combines CSV analysis, interference detection, and geospatial visualization of WiFi networks.**

---

## 📝 Credits & Attribution

This project integrates **three original scripts** created by **Dr. h. c. César A. Peregrino Rodríguez**:

### Original Scripts:
1. **depurador_csv** - Automatic detection and repair of date format issues in WiFi survey CSV files
2. **Analisis_Interferencias** - Deep analysis of interference between WiFi channels and optimization recommendations
3. **WiFi_Wardriving** - Complete geospatial data analysis with heatmaps and network density visualization

**Original Repository:** 🔗 [WiFi-Spectrum-Suite by DarkAlguty](https://github.com/DarkAlguty/WiFi-Spectrum-Suite)

---

## 🎯 Minino Integration - Wardriving

**WiFi Spectrum Suite** is designed to work seamlessly with **[Minino](https://github.com/ElectronicCats/Minino)**, a portable GPS-enabled device for wireless network scanning and wardriving.

### Minino + WiFi Spectrum Suite Workflow

```
Minino (Scanning) → GPS CSV → Suite (Analysis) → Maps & Reports
```

#### 1. **Scanning with Minino**
   - Make sure you have a strong GPS signal first (be in open spaces).
   - Verify that the **date and time** on the Minino (Applications > GPS > Date & Time) match your time zone
   - Change the time zone if it's not the same as yours (Settings > System > GPS > Time Zone)
   - Insert the microSD card (if not, Wardriving won't start).
   - Use the **Wardriving** app on Minino (Applications > GPS > Wardrive)
   - Select "AP Start" to begin
   - The device records: SSID, BSSID, RSSI, channel, frequency, **GPS coordinates**, time and date
   - Data is automatically saved as CSV on the microSD card

#### 2. **Data Extraction**
   - To cancel Wardriving, press "Back"
   - Remove the microSD from Minino
   - Insert the microSD card into a computer.
   - Copy the CSV file (usually in `Warfi/` folder for WiFi)
   - Typical file: `Warfi_YYYY-MM-DD_hh-mm-ss.csv` with location columns.

#### 3. **Processing with WiFi Spectrum Suite**
   - **Date Repair**: Automatically fixes format inconsistencies
   - **Interference Analysis**: Identifies channel conflicts
   - **Geospatial Visualization**: Generates interactive maps with network density

#### 4. **Advanced Visualization**
   - RSSI heatmaps by geographic location
   - Location maps with network markers
   - Distribution graphs by channel and frequency
   - Signal strength temporal analysis
   - Detailed reports with recommendations

---

## 🔧 Suite Functionality

### **1. CSV Debugger** (debug)
Automatically detects and repairs common CSV file issues:
- Inconsistent date formats
- Empty or invalid field values
- Incorrect character encoding
- Damaged column structures

**Supports multiple date formats:**
- ISO 8601: `2024-01-15 10:30:00`
- US: `01/15/2024 10:30:00`
- EU: `15/01/2024 10:30:00`
- Compact: `20240115103000`

### **2. Interference Analyzer** (interference)
Analyzes WiFi channel patterns and interference:
- Network distribution by channel
- Channel overlap detection (20/40/80 MHz)
- Average signal strength calculation per channel
- Weak signal network identification
- Channel optimization recommendations
- Statistical interference graphs

**Non-overlapping channels:** 1, 6, 11 (in 2.4GHz band)

### **3. Wardriving Analyzer** (wardriving)
Transforms scan data into geospatial visualizations:
- **RSSI Heatmaps**: Visualizes signal intensity by location
- **Location Maps**: Markers for each network with detailed info
- **Advanced Graphics**: Temporal analysis, channel distribution, histograms
- **Security Analysis**: Detection of OPEN, WEP, WPA2 networks
- **HTML Reports**: Interactive maps based on Folium/OpenStreetMap

---

## 📦 Script Structure

```
wifi_spectrum.py

├─ PART 1: CSV DEBUGGER
│  ├─ analyze_date_problems()          → Detects date issues
│  ├─ repair_date_issues()              → Repairs CSVs
│  └─ validate_date_repair()            → Validates repair
│
├─ PART 2: INTERFERENCE ANALYZER
│  ├─ robust_csv_loader()              → Robust CSV loading
│  ├─ clean_and_validate_data()        → Data cleaning
│  ├─ analyze_wifi_interference()      → Interference analysis
│  └─ generate_comprehensive_analysis()  → Detailed analysis
│
└─ PART 3: WARDRIVING ANALYZER
   ├─ WardrivingAnalyzer.load_data()       → Loads wardriving data
   ├─ generate_heat_map()                  → RSSI heatmap
   ├─ generate_location_map()              → Location map
   ├─ generate_plots()                     → Advanced graphics
   └─ generate_report()                    → Detailed report
```

---

## 💻 Installation

### Requirements
- Python 3.7+
- pip (Python package manager)

### Install Dependencies

```bash
pip install pandas matplotlib seaborn numpy folium click rich
```

Or using requirements.txt if available:

```bash
pip install -r requirements.txt
```

---

## 🚀 Usage - Terminal Commands

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
1. ✅ Date repair → `*_fixed.csv`
2. ✅ Interference analysis → graphs and report
3. ✅ Complete wardriving (maps, graphs, report)

**Generated files:**
- `wardriving_data_fixed.csv` - Repaired CSV
- `wardriving_data_fixed_report.txt` - Interference report
- `wardriving_data_fixed_analysis.png` - Interference graphs
- `heat_map_wardriving_data_fixed.html` - RSSI intensity map
- `location_map_wardriving_data_fixed.html` - Network location map
- `advanced_plots_wardriving_data_fixed.png` - Temporal and distribution analysis

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

## 📊 Input File Format - Expected CSV

Typical Minino export format:

```csv
Index,SSID,WiFi Address,BSSID,Channel,Frequency,RSSI,Security,FirstSeen,LastSeen,CurrentLatitude,CurrentLongitude,AuthMode
1,MyNetwork,00:11:22:33:44:55,00:11:22:33:44:55,1,2412,-45,WPA2,2024-01-15 10:30:00,2024-01-15 14:45:00,40.7128,-74.0060,WPA2
2,SecureWiFi,AA:BB:CC:DD:EE:FF,AA:BB:CC:DD:EE:FF,6,2437,-62,WPA2,2024-01-15 10:32:15,2024-01-15 14:46:30,40.7129,-74.0061,WPA2
```

### Recognized Columns:
- **SSID** - Network name
- **BSSID/WiFi Address** - MAC address
- **Channel** - WiFi channel (1-13 in 2.4GHz, 36-165 in 5GHz)
- **Frequency** - Frequency in MHz
- **RSSI** - Signal strength (dBm, negative values)
- **CurrentLatitude/Latitude** - Geographic coordinate (latitude)
- **CurrentLongitude/Longitude** - Geographic coordinate (longitude)
- **FirstSeen/Timestamp** - Detection date and time
- **AuthMode/Security** - Encryption type (OPEN, WEP, WPA, WPA2)

---

## 📈 Output Files

| File | Description |
|------|------------|
| `*_fixed.csv` | Repaired CSV with corrected dates |
| `*_report.txt` | Interference analysis report |
| `*_analysis.png` | Statistical graphs |
| `heat_map_*.html` | Interactive RSSI intensity map |
| `location_map_*.html` | Interactive network location map |
| `advanced_plots_*.png` | Advanced graphs (6 analyses) |

---

## 🗺️ Output Interpretation

### RSSI Heatmap
- **Intense red color** → Very strong signal (-40 dBm)
- **Yellow color** → Moderate signal (-70 dBm)
- **Light blue color** → Weak signal (-90 dBm)

### Wardriving Graphics
1. **RSSI Distribution by Channel** - Boxplot showing variability
2. **Average RSSI per Channel** - Mean intensity of each channel
3. **Temporal Evolution** - Signal change during scanning
4. **Authentication Methods** - Security distribution
5. **Density & RSSI (Hexbin)** - Geographic concentration
6. **RSSI Histogram** - Intensity distribution

### Interference Report
- **Total networks detected**
- **Unique networks by SSID**
- **Channels used**
- **Channel distribution**
- **Problematic channels**
- **Strategic recommendations**

---

### Debug/Verbose
The script prints detailed progress at each phase:
- Lines processed
- Errors detected
- Generated files
- Analysis statistics

---

## 📋 Use Cases

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
# Processes end-to-end
```

---

## 🔍 RSSI Metrics Interpretation

| RSSI (dBm) | Quality | Recommendation |
|------------|---------|----------------|
| -30 to -50  | Excellent | Optimal |
| -50 to -60  | Very Good | Normal |
| -60 to -70  | Good | Acceptable |
| -70 to -80  | Fair | Consider repeater |
| -80 to -90  | Weak | Poor coverage |
| < -90      | Very Weak | Unusable |

---

## 🛑 Troubleshooting

### "Error: File does not exist"
```bash
# Verify the path is correct
ls Warfi.csv
# Or use absolute path
wifi-spectrum-suite full /full/path/file.csv
```

### "Missing columns / Error processing CSV"
```bash
# The script attempts automatic column mapping
# If it fails, edit CSV to have: SSID, Channel, RSSI, BSSID, FirstSeen, CurrentLatitude, CurrentLongitude
```

### "HTML maps won't open"
```bash
# Open in browser manually
# Windows: start heat_map_file.html
# Linux: xdg-open heat_map_file.html
# Mac: open heat_map_file.html
```

---

## 📚 Additional Information

- **Original Repository**: [WiFi-Spectrum-Suite by DarkAlguty](https://github.com/DarkAlguty/WiFi-Spectrum-Suite)
- **Minino Wiki**: [Minino 4.4 GPS & Wardriving](https://github.com/ElectronicCats/Minino/wiki/4.4-GPS#wardriving)
- **WiGLE.net**: [Upload and visualize data](https://wigle.net/)

---

## 📄 License

Project provided for educational and research purposes.

---

## ✨ Summary

**WiFi Spectrum Suite** transforms raw WiFi scan data into:
- ✅ Detailed analysis reports
- ✅ Interactive geospatial maps
- ✅ Statistical graphics
- ✅ Optimization recommendations

**Perfect integration with Minino** for end-to-end wardriving workflows.

---

**Status**: Active | **Last Updated**: March 2026 | **Python**: 3.7+