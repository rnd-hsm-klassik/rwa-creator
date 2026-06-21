#!/bin/bash
cd "$(dirname "$0")"

if [ -f portaudio/lib/.libs/libportaudio.dylib ]
then
    echo "Found portaudio"
else
    echo "Building portaudio"
    cd portaudio
    make clean
    ./configure && make -j4
fi
