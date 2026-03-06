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

### **1. CSV Debugger** (depurador_csv)
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

### **2. Interference Analyzer** (Analisis_Interferencias)
Analyzes WiFi channel patterns and interference:
- Network distribution by channel
- Channel overlap detection (20/40/80 MHz)
- Average signal strength calculation per channel
- Weak signal network identification
- Channel optimization recommendations
- Statistical interference graphs

**Non-overlapping channels:** 1, 6, 11 (in 2.4GHz band)

### **3. Wardriving Analyzer** (WiFi_Wardriving)
Transforms scan data into geospatial visualizations:
- **RSSI Heatmaps**: Visualizes signal intensity by location
- **Location Maps**: Markers for each network with detailed info
- **Advanced Graphics**: Temporal analysis, channel distribution, histograms
- **Security Analysis**: Detection of OPEN, WEP, WPA2 networks
- **HTML Reports**: Interactive maps based on Folium/OpenStreetMap

---

## 📦 Script Structure

```
WiFi_Spectrum_Suite.py

├─ PART 1: CSV DEBUGGER (lines 30-300)
│  ├─ analyze_date_problems()          → Detects date issues
│  ├─ repair_date_issues()              → Repairs CSVs
│  └─ validate_date_repair()            → Validates repair
│
├─ PART 2: INTERFERENCE ANALYZER (lines 301-800)
│  ├─ robust_csv_loader()              → Robust CSV loading
│  ├─ clean_and_validate_data()        → Data cleaning
│  ├─ analyze_wifi_interference()      → Interference analysis
│  └─ generate_comprehensive_analysis()  → Detailed analysis
│
└─ PART 3: WARDRIVING ANALYZER (lines 801-1200)
   ├─ WardrivingAnalyzer.cargar_datos()    → Loads wardriving data
   ├─ generar_mapa_calor()            → RSSI heatmap
   ├─ generar_mapa_localizacion()     → Location map
   ├─ generar_graficos()              → Advanced graphics
   └─ generar_reporte()               → Detailed report
```

---

## 💻 Installation

### Requirements
- Python 3.7+
- pip (Python package manager)

### Install Dependencies

```bash
pip install pandas matplotlib seaborn numpy folium
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
python WiFi_Spectrum_Suite.py Warfi_file.csv --r-f --v-f -o Warfi_file_fixed.csv
```
- Detects date format issues
- Generates `wardriving_data_fixed.csv`
- Repairs dates and validates result

#### 2. Analyze Interference Only
```bash
python WiFi_Spectrum_Suite.py wardriving_data_fixed.csv --a-i
```
- Generates:
  - `wardriving_data_analysis.png` (graphs)
  - `wardriving_data_report.txt` (report)

#### 3. Generate Heatmap Only
```bash
python WiFi_Spectrum_Suite.py wardriving_data_fixed.csv --wd --mapa-calor
```
- Generates: `mapa_calor_wardriving_data.html` (interactive map) opcionalmente usar -mc después de --wd

#### 4. Generate Location Map
```bash
python WiFi_Spectrum_Suite.py wardriving_data_fixed.csv --wd --mapa-localizacion
```
- Generates: `mapa_localizacion_wardriving_data.html` (network markers) opcionalmente usar -ml después de --wd

#### 5. Generate Graphics
```bash
python WiFi_Spectrum_Suite.py wardriving_data_fixed.csv --wd --graficos
```
- Generates: `graficos_Avanzados_wardriving_data.png` (6 analysis graphs) opcionalmente usar -g después de --wd

#### 6. Generate Wardriving Report
```bash
python WiFi_Spectrum_Suite.py wardriving_data_fixed.csv --wd --reporte
```
- Prints complete analysis to termina, opcionalmente usar -r después de --wd

### **Complete Analysis**

#### Option A: Total Integrated Analysis (Recommended)
```bash
python WiFi_Spectrum_Suite.py wardriving_data.csv --completo
```

**Automatically executes:**
1. ✅ Date repair → `*_fixed.csv`
2. ✅ Interference analysis → graphs and report
3. ✅ Complete wardriving (maps, graphs, report)

**Generated files:**
- `wardriving_data_fixed.csv` - Repaired CSV
- `wardriving_data_report.txt` - Interference report
- `wardriving_data_analysis.png` - Interference graphs
- `mapa_calor_wardriving_data.html` - RSSI intensity map
- `mapa_localizacion_wardriving_data.html` - Network location map
- `graficos_Avanzados_wardriving_data.png` - Temporal and distribution analysis

#### Option B: All Wardriving Options
```bash
python WiFi_Spectrum_Suite.py wardriving_data_fixed.csv --wd --todo
```

**Executes:**
- Detailed report
- Heatmap
- Location map
- Advanced graphics

#### Option C: Recommended Minino Workflow (Step by Step)
```bash
# Step 1: Repair and validate
python WiFi_Spectrum_Suite.py minino_wardriving.csv --r-f --v-f

# Step 2: Analyze interference
python WiFi_Spectrum_Suite.py minino_wardriving_fixed.csv --a-i

# Step 3: Generate complete wardriving visualization
python WiFi_Spectrum_Suite.py minino_wardriving_fixed.csv --wd --todo
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
| `*_analysis.png` | Statistical graphs (4 graphs) |
| `mapa_calor_*.html` | Interactive RSSI intensity map |
| `mapa_localizacion_*.html` | Interactive network location map |
| `graficos_Avanzados_*.png` | Advanced graphs (6 analyses) |

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
python WiFi_Spectrum_Suite.py scan.csv --a-i
# Identifies insecure networks and saturated channels
```

### Case 2: Coverage Optimization
```bash
python WiFi_Spectrum_Suite.py scan.csv --wd --mapa-calor --graficos
# Visualizes coverage gaps
```

### Case 3: Community Mapping (like Wigle)
```bash
python WiFi_Spectrum_Suite.py Warfi_file_fixed.csv --wd --todo
# Generates interactive maps for community analysis
```

### Case 4: Automatic Minino Pipeline
```bash
python WiFi_Spectrum_Suite.py Warfi_file.csv --completo
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
python WiFi_Spectrum_Suite.py /full/path/file.csv --completo
```

### "Missing columns / Error processing CSV"
```bash
# The script attempts automatic column mapping
# If it fails, edit CSV to have: SSID, Channel, RSSI, BSSID, FirstSeen, CurrentLatitude, CurrentLongitude
```

### "HTML maps won't open"
```bash
# Open in browser manually
# Windows: start mapa_calor_file.html
# Linux: xdg-open mapa_calor_file.html
# Mac: open mapa_calor_file.html
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

## Features

### 1. CSV Debugger 🔧
- Automatically detect date format issues in WiFi survey exports
- Repair corrupted or malformed CSV files
- Validate data integrity before analysis
- Smart field reconstruction for missing or invalid values
- Supports multiple date formats (ISO 8601, US, EU formats)

### 2. Interference Analysis 📊
- Detect overlapping WiFi channels
- Identify potential interference sources
- Channel frequency analysis
- SSID and signal strength correlation
- Visualize interference hotspots on maps

### 3. Wardriving Analyzer 🗺️
- Geospatial mapping of WiFi networks
- Heat maps showing network density
- Interactive Folium-based maps
- GPS coordinate parsing and validation
- Network distribution analysis by location

### 4. Data Visualization 📈
- Signal strength distribution charts
- Channel frequency heatmaps
- Network density maps
- Time-based analysis plots
- Custom Seaborn/Matplotlib visualizations

## Installation

### Requirements
- Python 3.7+
- pandas
- matplotlib
- seaborn
- numpy
- folium
- 

### Package Installation

```bash
pip install pandas matplotlib seaborn numpy folium
```

Or install all dependencies at once:

```bash
pip install -r requirements.txt
```

## Usage

### Basic Usage

```bash
python WiFi_Spectrum_Suite.py -i input.csv -o output_directory
```

### Command Line Arguments

| Argument | Description | Required |
|----------|-------------|----------|
| `-i, --input` | Input CSV file path | Yes |
| `-o, --output` | Output directory for results | Yes |
| `-a, --analyze` | Run interference analysis | No |
| `-w, --wardriving` | Generate wardriving maps | No |
| `-r, --repair` | Repair date issues automatically | No |
| `-v, --validate` | Validate repaired data | No |
| `-d, --debug` | Enable debug output | No |

### Examples

#### Repair CSV with Date Issues
```bash
python WiFi_Spectrum_Suite.py -i survey.csv -o output/ -r
```

#### Full Analysis Pipeline
```bash
python WiFi_Spectrum_Suite.py -i survey.csv -o output/ -r -a -w -v
```

#### Wardriving Map Generation
```bash
python WiFi_Spectrum_Suite.py -i survey.csv -o maps/ -w
```

#### Interference Analysis Only
```bash
python WiFi_Spectrum_Suite.py -i survey.csv -o analysis/ -a
```

## Input File Format

Expected CSV format for WiFi survey data:

```csv
Index,SSID,WiFi Address,BSSID,Channel,RSSI,Frequency,Security,First Seen,Last Seen,Latitude,Longitude
1,MyNetwork,00:11:22:33:44:55,00:11:22:33:44:55,1,-45,2412,WPA2,2024-01-15 10:30:00,2024-01-15 14:45:00,40.7128,-74.0060
```

### Supported Data Columns

| Column | Purpose | Required |
|--------|---------|----------|
| SSID | Network name | Yes |
| BSSID/WiFi Address | MAC address | Yes |
| Channel | WiFi channel (1-13 or 1-14) | Yes |
| RSSI | Signal strength (dBm) | Yes |
| Latitude/Long | GPS coordinates | For wardriving |
| First Seen | First detection time | For interference analysis |
| Security | Encryption type (WPA2, WPA, WEP) | No |

## Output Files

### Generated Reports

- `wifi_analysis_report.txt` - Summary statistics and analysis
- `channel_overlap_analysis.csv` - Interference matrix
- `network_density_map.html` - Interactive Folium map
- `signal_distribution.png` - Signal strength visualization
- `channel_heatmap.png` - Channel frequency analysis
- `wardriving_analysis.html` - Geospatial mapping
- `*_fixed.csv` - Repaired CSV data

## Advanced Features

### Date Repair Engine
Automatically fixes common date format issues:
- Detects mixed date formats
- Converts between ISO 8601, US, and EU formats
- Replaces invalid values with current timestamp
- Validates repaired data integrity

### Smart Channel Analysis
- Identifies overlapping channels (20/40/80 MHz)
- Calculates interference risk scores
- Suggests optimal channel placement
- Visualizes frequency spectrum usage

### Geospatial Heatmaps
- Creates multi-layer maps showing:
  - Network density
  - Signal strength distribution
  - Security type distribution
  - Channel allocation patterns

## Troubleshooting

### Common Issues

**Issue: "Date parsing failed"**
```bash
Solution: Use the -r (repair) flag to automatically fix date fields
python WiFi_Spectrum_Suite.py -i survey.csv -o output/ -r
```

**Issue: "Missing latitude/longitude data"**
```bash
Solution: Wardriving features require GPS coordinates. Ensure input file has these columns.
```

**Issue: "Encoding errors with non-ASCII characters"**
```bash
Solution: The tool automatically handles UTF-8 encoding. If issues persist, convert CSV to UTF-8:
iconv -f ISO-8859-1 -t UTF-8 input.csv > input_utf8.csv
```

## Performance Tips

- For large datasets (>100k networks), run analysis on subsets
- Use `-d` flag to monitor memory usage
- Maps load faster with filtered geographic regions
- Cache intermediate results in the output directory

## Data Privacy

⚠️ **Important**: WiFi survey data often contains location information. 

- Never share raw survey files publicly
- Anonymize SSIDs before archiving
- Filter sensitive geographic regions
- Comply with local regulations on wireless scanning

## Technical Details

### Dependencies
- **pandas**: Data manipulation and CSV processing
- **matplotlib/seaborn**: Statistical visualization
- **numpy**: Numerical analysis
- **folium**: Geospatial mapping
- **Python regex**: Date format detection and repair

### Architecture
The suite is organized into three main modules:
1. **CSV Debugger** - Lines 30-400: Data validation and repair
2. **Interference Analyzer** - Lines 400-800: Channel analysis
3. **Wardriving Analyzer** - Lines 800-1234: Geospatial visualization

## Contributing

Contributions are welcome! Areas for improvement:
- Support for additional WiFi data formats
- Real-time network scanning (requires libpcap)
- Machine learning-based interference prediction
- Support for 6GHz and WiFi 7 analysis

## License

This project is provided as-is for educational and research purposes.

## FAQ

**Q: Can I use this for WiFi hacking?**  
A: This tool is for legitimate network analysis, testing, and optimization only. Always obtain permission before analyzing networks you don't own.

**Q: What's the maximum dataset size?**  
A: Tested with up to 500,000 network entries. Performance depends on your system RAM.

**Q: Can I integrate this with other tools?**  
A: Yes! The CSV output can be imported into mapping tools, databases, or analysis platforms.

**Q: Does it work on Windows/Mac/Linux?**  
A: Yes, it's cross-platform. Python 3.7+ required on all systems.

---

**Status**: Active Development  
**Last Updated**: March 2026  
**Python**: 3.7+
