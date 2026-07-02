#!/usr/bin/env bash
set -euo pipefail

# Parse arguments
CLEAN=false
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=true ;;
    *)
      echo "Unknown argument: $arg" >&2
      exit 1
      ;;
  esac
done

# Path to your Qt5 installation
QT_PATH="/Users/Shared/Qt/5.15.2/clang_64"

# Out-of-source build directory
BUILD_DIR="$(dirname "$0")/build/qmake-debug"

# =============================================================================
# Clean
# =============================================================================

if [ "$CLEAN" = true ]; then
  echo "==> Cleaning previous build..."
  rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

# =============================================================================
# qmake configure
# =============================================================================

echo "==> Configuring..."
"$QT_PATH/bin/qmake" -o "$BUILD_DIR/Makefile" rwacreator.pro \
    CONFIG+=debug CONFIG+=sdk_no_version_check

# =============================================================================
# Build
# =============================================================================
echo "==> Building..."
make -C "$BUILD_DIR" -j "$(sysctl -n hw.logicalcpu)"

echo "Ready to launch in debugger"
exit 0
