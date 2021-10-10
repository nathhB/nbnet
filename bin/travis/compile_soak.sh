#!/bin/bash

if [ -n "$EMSCRIPTEN" ]
then
    cd /nbnet
fi

cd soak
mkdir build
cd build

if [ "$TRAVIS_OS_NAME" = "windows" ]
then
    cmake -G "$CMAKE_GENERATOR" -DCPP_COMPILE=$CPP_COMPILE -DCMAKE_BUILD_TYPE=Debug ..
elif [ -n "$EMSCRIPTEN" ]
then
    emcmake cmake -DCPP_COMPILE=$CPP_COMPILE -DCMAKE_BUILD_TYPE=Debug ..
else
    cmake -DCPP_COMPILE=$CPP_COMPILE -DCMAKE_BUILD_TYPE=Debug ..
fi

if [ "$TRAVIS_OS_NAME" = "windows" ]
then
    if [ "$CMAKE_GENERATOR" = "MinGW Makefiles" ]
    then
        mingw32-make
    else
        C:/Program\ Files\ \(x86\)/Microsoft\ Visual\ Studio/2019/BuildTools/MSBuild/Current/Bin/MSBuild.exe -p:Configuration=Debug soak.sln
    fi
else
    make
fi