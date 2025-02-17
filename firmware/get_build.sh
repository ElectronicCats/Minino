#!/bin/bash

# This script is used to get the build files needed to flash the badge using the esptool.py script.

PROJECT_NAME="minino"
CONFIG_FILE="sdkconfig.defaults"
OTA_BUILD_DIR="build-ota"
NO_OTA_BUILD_DIR="build-noota"
TEMPORARY_BUILD_DIR="build_files"

PROJECT_VERSION=$(grep '^CONFIG_PROJECT_VERSION=' "$CONFIG_FILE" | cut -d'=' -f2 | tr -d '"')
OTA_BUILD_FINAL_DIR="minino-ota-firmware-$PROJECT_VERSION"
NO_OTA_BUILD_FINAL_DIR="minino-noota-firmware-$PROJECT_VERSION"

# Build the firmware
echo "Building ota firmware..."
idf.py @profiles/ota build
mkdir -p $TEMPORARY_BUILD_DIR

# Copy the firmware files to the build directory
echo "Copying firmware files to build directory..."
cp $OTA_BUILD_DIR/$PROJECT_NAME.bin $TEMPORARY_BUILD_DIR
cp $OTA_BUILD_DIR/partition_table/partition-table.bin $TEMPORARY_BUILD_DIR
cp $OTA_BUILD_DIR/bootloader/bootloader.bin $TEMPORARY_BUILD_DIR
cp $OTA_BUILD_DIR/ota_data_initial.bin $TEMPORARY_BUILD_DIR
mv $TEMPORARY_BUILD_DIR $OTA_BUILD_FINAL_DIR

echo "Building no ota firmware..."
idf.py @profiles/noota build
rm -rf $TEMPORARY_BUILD_DIR
mkdir -p $TEMPORARY_BUILD_DIR

echo "Copying firmware files to build directory..."
cp $NO_OTA_BUILD_DIR/$PROJECT_NAME.bin $TEMPORARY_BUILD_DIR
cp $NO_OTA_BUILD_DIR/partition_table/partition-table.bin $TEMPORARY_BUILD_DIR
cp $NO_OTA_BUILD_DIR/bootloader/bootloader.bin $TEMPORARY_BUILD_DIR
mv $TEMPORARY_BUILD_DIR $NO_OTA_BUILD_FINAL_DIR

# Compress build_files and delete directory
echo "Compressing ota build files..."
# zip -r build_files_$PROJECT_VERSION.zip build_files
zip -r $OTA_BUILD_FINAL_DIR.zip $OTA_BUILD_FINAL_DIR
echo "Compressing no ota build files..."
zip -r $NO_OTA_BUILD_FINAL_DIR.zip $NO_OTA_BUILD_FINAL_DIR

echo "Cleaning build files..."
rm -rf $TEMPORARY_BUILD_DIR
rm -rf $OTA_BUILD_FINAL_DIR
rm -rf $NO_OTA_BUILD_FINAL_DIR
rm -rf build-noota
rm -rf build-ota

echo "Done!"
