from pathlib import Path
from setuptools import setup, find_packages

long_description = Path("README.md").read_text(encoding="utf-8")

setup(
    name="WifiSpectrum",
    version="1.1.0.0",
    packages=find_packages(include=["modules", "modules.*"]),
    description="WiFi Spectrum Suite — Suite completa de análisis WiFi.",
    long_description=long_description,
    long_description_content_type="text/markdown",
    install_requires=[
        "click>=8.0.0",
        "matplotlib>=3.10.0",
        "pandas>=3.0.1",
        "numpy>=2.0.0",
        "rich>=14.0.0",
        "seaborn>=0.13.2",
        "folium>=0.20.0",
    ],
    py_modules=["wifi_spectrum"],
    entry_points={
        "console_scripts": [
            "wifi-spectrum-suite=wifi_spectrum:main",
        ],
    },
    author="Electronic Cats & Dr. h. c. César A. Peregrino Rodríguez",
    author_email="support@electroniccats.com",
    python_requires=">=3.12",
)
