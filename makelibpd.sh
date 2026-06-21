#!/bin/bash
cd "$(dirname "$0")"

if [ -f libpd/libs/libpd.dylib ]
then
    echo "Found libpd"
else
    echo "Building libpd"
    cd libpd
    make clean
    make -j4 EXTRA=true
fi
