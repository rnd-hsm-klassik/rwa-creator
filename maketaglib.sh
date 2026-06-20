#!/bin/bash
cd "$(dirname "$0")"

if [ -f taglib/build/taglib/libtag.a ]
then
    echo "Found taglib"
else
    echo "Compiling taglib..."
    cd taglib
    mkdir -p build
    cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_TESTING=OFF \
        -DBUILD_EXAMPLES=OFF
    make -j4
    cd ../..
fi
