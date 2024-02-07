#!/bin/bash

git clone https://github.com/paullouisageneau/libdatachannel.git
cd libdatachannel
git submodule update --init --recursive --depth 1
cmake -B build -DUSE_GNUTLS=0 -DUSE_NICE=0 -DCMAKE_BUILD_TYPE=Release
cd build
make -j2
