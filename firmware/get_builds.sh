#!/bin/bash

PROJECT_NAME="minino"
CONFIG_FILE="sdkconfig.version"
PROJECT_VERSION=$(grep '^CONFIG_PROJECT_VERSION=' "$CONFIG_FILE" | cut -d'=' -f2 | tr -d '"')

MININO_BUILD_DIR="build-minino"
BSIDES_BUILD_DIR="build-bSides"
DRAGONJAR_BUILD_DIR="build-dragonJar"
EKOPARTY_BUILD_DIR="build-ekoParty"
BUGCON_BUILD_DIR="build-bugCon"

FINAL_BUILD_DIR="minino-release-$PROJECT_VERSION"

make builds
make mergebins

if [ -d "$FINAL_BUILD_DIR" ]; then
  rm -rf $FINAL_BUILD_DIR
fi

mkdir -p $FINAL_BUILD_DIR
cp $MININO_BUILD_DIR/mininoBin.bin $FINAL_BUILD_DIR
cp $BSIDES_BUILD_DIR/bSidesBin.bin $FINAL_BUILD_DIR
cp $DRAGONJAR_BUILD_DIR/dragonJarBin.bin $FINAL_BUILD_DIR
cp $EKOPARTY_BUILD_DIR/ekoPartyBin.bin $FINAL_BUILD_DIR
cp $BUGCON_BUILD_DIR/bugConBin.bin $FINAL_BUILD_DIR

# make clean_builds

