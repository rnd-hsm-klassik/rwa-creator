#!/usr/bin/env bash
# THIS IS STILL MISSING NOTARISATION!
set -euo pipefail

# read from .env
# if no .env, variables from CI pipeline are expected
cd "$(dirname "$0")"
if [ -f .env ];
then
  echo "Loading .env file..."
  set -a
  source .env
  set +a
else
  echo "No .env file, running on env variables"
fi

# =============================================================================
# Configuration — check .env_example and create .env
# =============================================================================

# Read from qmake config
VERSION="$(sed -n 's/^VERSION[[:space:]]*=[[:space:]]*//p' rwacreator.pro | head -n1)"
if [ -z "$VERSION" ]; then
  echo "Could not read VERSION from rwacreator.pro" >&2
  exit 1
fi

# Path to your Qt5 installation
QT_PATH="/Users/Shared/Qt/5.15.2/clang_64"

# Out-of-source build directory
BUILD_DIR="$(dirname "$0")/build/qmake-release"

# App bundle produced by qmake
APP_BUNDLE="rwacreator.app"

# Where to put the final DMG / zip
DIST_DIR="$(dirname "$0")/dist"
APP_NAME="RWACreator"
ARCHIVE="$DIST_DIR/$APP_NAME.zip"
DMG_TEMP="$DIST_DIR/dmg_temp"
DMG="$DIST_DIR/$APP_NAME.dmg"

# =============================================================================
# Clean
# =============================================================================

# Clean previous build
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# =============================================================================
# qmake configure
# =============================================================================

echo "==> Configuring..."
cd "$BUILD_DIR"
"$QT_PATH/bin/qmake" ../../rwacreator.pro CONFIG+=release CONFIG+=sdk_no_version_check

# =============================================================================
# Build
# =============================================================================
echo "==> Building..."
make -j "$(sysctl -n hw.logicalcpu)"

# =============================================================================
# Deploy Qt frameworks
# =============================================================================

echo "==> Deploying Qt frameworks..."
"$QT_PATH/bin/macdeployqt" "$APP_BUNDLE"

## # =============================================================================
## # Ad-hoc sign
## # =============================================================================
##
## echo "==> Signing app (Ad-hoc)..."
## codesign --force --deep --sign - "$APP_BUNDLE"

# =============================================================================
# Inside-out signing (required for notarization / hardened runtime)
# Sign all nested binaries first, then the bundle itself.
# --deep is NOT used: it doesn't reliably propagate --options runtime
# to nested components, causing Gatekeeper to reject the app.
# =============================================================================

CODESIGN_ARGS=(--sign "$SIGN_IDENTITY" --options runtime --timestamp --force)

echo "==> Signing nested dylibs and .so files..."
find "$APP_BUNDLE" -type f \( -name "*.dylib" -o -name "*.so" \) | while IFS= read -r f; do
    codesign "${CODESIGN_ARGS[@]}" "$f"
done

echo "==> Signing frameworks (deepest path first)..."
find "$APP_BUNDLE" -name "*.framework" -type d | sort -r | while IFS= read -r f; do
    codesign "${CODESIGN_ARGS[@]}" "$f"
done

echo "==> Signing app bundle..."
codesign "${CODESIGN_ARGS[@]}" "$APP_BUNDLE"

# Verify the signature
codesign --verify --deep --strict "$APP_BUNDLE"
echo "    Signature OK"

# =============================================================================
# Package for notarization (zip is simpler than DMG for submission)
# =============================================================================

mkdir -p "$DIST_DIR"
echo "==> Creating archive for notarization..."
ditto -c -k --keepParent "$APP_BUNDLE" "$ARCHIVE"

# =============================================================================
# Notarize
# --wait  blocks until Apple returns a result (typically 1–5 min)
# =============================================================================

echo "==> Submitting for notarization..."
xcrun notarytool submit "$ARCHIVE" \
    --keychain-profile "$NOTARY_PROFILE" \
    --wait

# =============================================================================
# Staple the notarization ticket to the bundle
# =============================================================================

echo "==> Stapling..."
xcrun stapler staple "$APP_BUNDLE"
xcrun stapler validate "$APP_BUNDLE"

# =============================================================================
# Create DMG from bundle
# =============================================================================

mkdir -p "$DMG_TEMP"
echo "==> Creating DMG..."
rm -f "$DMG"
cp -R "$APP_BUNDLE" "$DMG_TEMP"/
ln -s /Applications "$DMG_TEMP"/Applications

hdiutil create \
  -volname "RWA Creator $VERSION" \
  -srcfolder "$DMG_TEMP" \
  -ov \
  -format UDZO \
  -fs HFS+ \
  "rwacreator-$VERSION.dmg"

rm -rf dmg-temp
echo ""
echo "Done! DMG created: rwacreator-$VERSION.dmg"
