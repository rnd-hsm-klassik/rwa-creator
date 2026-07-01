#!/usr/bin/env bash
set -euo pipefail

# Path to your Qt5 installation
QT_PATH="/Users/Shared/Qt/5.15.2/clang_64"

# Out-of-source build directory
BUILD_DIR="$(dirname "$0")/build/qmake-debug"

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
"$QT_PATH/bin/qmake" ../../rwacreator.pro CONFIG+=debug CONFIG+=sdk_no_version_check

# =============================================================================
# Build
# =============================================================================
echo "==> Building..."
make -j "$(sysctl -n hw.logicalcpu)"

echo "Ready to launch in debugger"
exit 0
