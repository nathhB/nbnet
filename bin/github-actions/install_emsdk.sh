#!/bin/bash

git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install $1
./emsdk activate $1
source ./emsdk_env.sh
emcc -v # make sure emcc is available
