#!/bin/bash

# This script is used to get the build files needed to flash the badge using the esptool.py script.

PROJECT_NAME="minino"
CONFIG_FILE="sdkconfig"

project_version=$(grep '^CONFIG_PROJECT_VERSION=' "$CONFIG_FILE" | cut -d'=' -f2 | tr -d '"')
# Build the firmware
make compile

mkdir -p build_files/
mkdir -p build_files/partition_table/
mkdir -p build_files/bootloader/

# Copy the firmware files to the build directory
echo "Copying firmware files to build directory..."
cp build/$PROJECT_NAME.bin build_files/
cp build/partition_table/partition-table.bin build_files/partition_table/
cp build/bootloader/bootloader.bin build_files/bootloader/
cp build/ota_data_initial.bin build_files/

# Compress build_files and delete directory
echo "Compressing build_files..."
zip -r build_files_$project_version.zip build_files
rm -rf build_files/

echo "Done!"
