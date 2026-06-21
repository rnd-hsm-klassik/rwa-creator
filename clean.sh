cd "$(dirname "$0")"
cd portaudio
make clean
cd ..

cd libpd
make clean
rm -rf libs/libpd.dylib
cd ..

cd taglib/
rm -rf build
