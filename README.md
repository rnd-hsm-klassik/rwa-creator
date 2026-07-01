# rwacreator
Desktop Application for Real World Audio (RWA)

Around 2015 I started with the development of the Real World Audio (RWA) environment. RWA is a middleware for creating interactive binaural soundwalks. It includes a Desktop "Creator" Software, an iOS client and a ESP32-based head tracking device for dynamic binaural synthesis. The RWA Creator is a cross-platform application written in C++/Qt and can be extended with Pure Data patches. Besides location-based audio playback it facilitates mechanisms for implementing more complex game logic without the need for writing code. Once audio files, Pd patches and other assets are placed on the map, the corresponding RWA game can be exported to the iOS client.

![RwaScreenshot](https://user-images.githubusercontent.com/10684202/161531985-9940b234-253b-4754-8ad6-8750f697cc78.png)

## Build Instructions

### Prerequisites

#### macOS

- macOS 11.0 (Big Sur) or later
- Xcode 13.1 or later with Command Line Tools
- Qt 5.15.2 (installed via official Qt installer)
- CMake 3.5+ (for building TagLib dependency)

#### Installing Dependencies

**Qt 5.15.2:**

Install using `aqtinstall` (Qt Online Installer):

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install aqtinstall
# check for version: aqt list-qt mac desktop
aqt install-qt mac desktop 5.15.2 clang_64 --outputdir /Users/Shared/Qt
echo 'export PATH="/Users/Shared/Qt/5.15.2/clang_64/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

**CMake:**

```bash
# Option 1: Using Homebrew (if available)
brew install cmake

# Option 2: Download pre-built binary
cd ~/Downloads
curl -L -O https://github.com/Kitware/CMake/releases/download/v3.27.9/cmake-3.27.9-macos-universal.tar.gz
tar xzf cmake-3.27.9-macos-universal.tar.gz
echo 'export PATH="$HOME/Downloads/cmake-3.27.9-macos-universal/CMake.app/Contents/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

### Building

Clone the repository with submodules:

```bash
# set up git to use HTTPS instead of SSH for GitHub
git config --global --unset-all url."https://".insteadOf
git config --global --add url."https://".insteadOf git://
git config --global --add url."https://".insteadOf http://
git config --global --add url."https://github.com/".insteadOf "git@github.com:"

# verify the configuration
git config --global --get-all url."https://".insteadOf

# clone the repository with submodules
git clone --recursive -b legacy \
    https://github.com/rnd-hsm-klassik/rwa-creator.git
cd rwa-creator
```

### Debug Build

```bash
mkdir -p build/qmake-debug
cd $_
qmake ../../rwacreator.pro CONFIG+=debug CONFIG+=sdk_no_version_check
# make clean # if necessary
make -j"$(sysctl -n hw.logicalcpu)"
```

The first build will:

- Compile portaudio (audio I/O library)
- Compile libpd (Pure Data library)
- Compile TagLib (audio metadata library)
- Build all rwacreator sources
- Copy resources (PureData patches, images) to the app bundle

**Run the application:**

```bash
open rwacreator.app
```

### Release Build

```bash
mkdir -p build/release
cd $_
qmake ../../rwacreator.pro CONFIG+=release CONFIG+=sdk_no_version_check
# make clean # if necessary
make -j"$(sysctl -n hw.logicalcpu)"
```

## Troubleshooting

### SDK Version Warning

If you see warnings about platform SDK mismatch, add `CONFIG+=sdk_no_version_check` to the qmake command.

### Portaudio Build Issues

If portaudio fails to build, you may need to remove `-Werror` from its Makefile:

```bash
cd portaudio
./configure
# Edit Makefile to remove -Werror
make
```

### Application Bundle Structure

After building, the app bundle contains:

```
rwacreator.app/
├── Contents/
│   ├── Info.plist
│   ├── MacOS/
│   │   └── rwacreator           # Main executable
│   ├── Frameworks/
│   │   ├── libpd.dylib
│   │   └── libportaudio.2.dylib
│   └── Resources/
│       ├── puredata/            # Pure Data patches
│       │   ├── *.pd
│       │   └── fabian_dir256.txt
│       └── images/              # UI resources
│           └── *.png, *.svg
```

## Deployment

### Creating a Distributable App Bundle

After building a release version, use `macdeployqt` to bundle Qt frameworks and prepare for distribution:

1. Build release version

```bash
mkdir -p build/release
cd $_
qmake ../../rwacreator.pro CONFIG+=release CONFIG+=sdk_no_version_check
make clean
make -j"$(sysctl -n hw.logicalcpu)"
```

1. Run macdeployqt

```bash
macdeployqt rwacreator.app
```

This will:

- Copy all required Qt frameworks into the app bundle
- Update library paths to use `@rpath`
- Fix dependencies automatically

1. Verify the bundle

```bash
# Check that Qt frameworks are bundled
ls -la rwacreator.app/Contents/Frameworks/

# Verify it runs without Qt in PATH
open rwacreator.app
```

### Ad-hoc Code Signing

For local distribution or testing on other Macs without Developer ID:

```bash
codesign --force --deep --sign - rwacreator.app
```

Verify the signature:

```bash
codesign --verify --verbose rwacreator.app
spctl --assess --verbose rwacreator.app
```

### Creating a DMG

Option 1: Using macdeployqt

```bash
macdeployqt rwacreator.app -dmg
```

### Complete Release Build Script

Run `build_release.sh` to build and package the release.

### Version Information

Version number is defined in `rwacreator.pro`.
This version is automatically embedded in the app bundle's `Info.plist`.

### Compatibility

- **Qt Version:** 5.15.2 (Qt 6 not supported - requires macOS 13+)
- **C++ Standard:** C++17
- **Deployment Target:** macOS 11.0 (Big Sur)
- **Architecture:** x86_64 (Intel)
- **Tested on:** macOS 11.7, 12.7

### Known Issues

- TagLib requires CMake for building. If CMake is not available, TagLib support will be disabled (audio metadata won't be auto-detected from files).
- The project uses Qt5 Bluetooth API which has some differences from Qt6. Version checks ensure compatibility.
- Deployment target warnings (12.7 vs 11.0) are harmless - the app will run on the system it was built on.
