#!/bin/bash
set -e

VERSION="0.8.1"
QT_PATH="$HOME/Qt/5.15.2/clang_64"

# Clean previous build
rm -rf build/release
mkdir -p build/release
cd build/release

# Build release
echo "Building release..."
"$QT_PATH/bin/qmake" ../rwacreator.pro CONFIG+=release CONFIG+=sdk_no_version_check
make -j4

# Deploy Qt frameworks
echo "Deploying Qt frameworks..."
"$QT_PATH/bin/macdeployqt" rwacreator.app

# Ad-hoc sign
echo "Signing app..."
codesign --force --deep --sign - rwacreator.app

# Create DMG
echo "Creating DMG..."
mkdir dmg-temp
cp -R rwacreator.app dmg-temp/
ln -s /Applications dmg-temp/Applications

hdiutil create -volname "RWA Creator $VERSION" \
  -srcfolder dmg-temp \
  -ov -format UDZO \
  -fs HFS+ \
  "rwacreator-$VERSION.dmg"

rm -rf dmg-temp

echo "Done! DMG created: rwacreator-$VERSION.dmg"
