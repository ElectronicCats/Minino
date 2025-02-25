#!/bin/bash
PROJECT_NAME="minino"
CONFIG_FILE="sdkconfig.version"

if [ -z "$1" ]; then
  echo "Version parameter is missing. Example: ./get_builds.sh v1.1.7.0"
  exit 1
fi
PROJECT_VERSION="$1"
sed -i "s/^CONFIG_PROJECT_VERSION=.*/CONFIG_PROJECT_VERSION=\"$PROJECT_VERSION\"/" "$CONFIG_FILE"


MININO_BUILD_DIR="build-minino"
BSIDES_BUILD_DIR="build-bsides"
DRAGONJAR_BUILD_DIR="build-dragonjar"
EKOPARTY_BUILD_DIR="build-ekoparty"
BUGCON_BUILD_DIR="build-bugcon"

FINAL_BUILD_DIR="minino-release-$PROJECT_VERSION"

make builds
make mergebins

if [ -d "$FINAL_BUILD_DIR" ]; then
  rm -rf $FINAL_BUILD_DIR
fi

mkdir -p $FINAL_BUILD_DIR
cp $MININO_BUILD_DIR/mininoBin.bin $FINAL_BUILD_DIR/minino_$PROJECT_VERSION.bin
cp $BSIDES_BUILD_DIR/bsidesBin.bin $FINAL_BUILD_DIR/bsides_$PROJECT_VERSION.bin
cp $DRAGONJAR_BUILD_DIR/dragonjarBin.bin $FINAL_BUILD_DIR/dragonjar_$PROJECT_VERSION.bin
cp $EKOPARTY_BUILD_DIR/ekopartyBin.bin $FINAL_BUILD_DIR/ekoparty_$PROJECT_VERSION.bin
cp $BUGCON_BUILD_DIR/bugconBin.bin $FINAL_BUILD_DIR/bugcon_$PROJECT_VERSION.bin

zip $FINAL_BUILD_DIR/minino_$PROJECT_VERSION.zip -j $FINAL_BUILD_DIR/minino_$PROJECT_VERSION.bin
zip $FINAL_BUILD_DIR/bsides_$PROJECT_VERSION.zip -j $FINAL_BUILD_DIR/bsides_$PROJECT_VERSION.bin
zip $FINAL_BUILD_DIR/dragonjar_$PROJECT_VERSION.zip -j $FINAL_BUILD_DIR/dragonjar_$PROJECT_VERSION.bin
zip $FINAL_BUILD_DIR/ekoparty_$PROJECT_VERSION.zip -j $FINAL_BUILD_DIR/ekoparty_$PROJECT_VERSION.bin
zip $FINAL_BUILD_DIR/bugcon_$PROJECT_VERSION.zip -j $FINAL_BUILD_DIR/bugcon_$PROJECT_VERSION.bin

rm $FINAL_BUILD_DIR/*.bin

make clean_builds

