#!/bin/bash
cd "$(dirname "$0")"

if [ -f portaudio/lib/.libs/libportaudio.dylib ]
then
    echo "Found portaudio"
else
    echo "Building portaudio"
    cd portaudio
    make clean
    make distclean
    sed -i '' 's/-Werror//g' configure configure.in
    ./configure && make -j"$(sysctl -n hw.logicalcpu)"
fi
