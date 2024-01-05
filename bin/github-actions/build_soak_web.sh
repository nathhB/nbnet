#!/bin/bash

cd emsdk
source ./emsdk_env.sh
cd ../soak
mkdir build_web
cd build_web
emcmake cmake ..
make
cd ..
npm install
