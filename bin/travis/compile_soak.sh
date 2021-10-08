#!/bin/bash

cd soak
mkdir build
cd build

if [ "$TRAVIS_OS_NAME" = "windows" ]
then
    cmake -G "$CMAKE_GENERATOR" -DCPP_COMPILE=$CPP_COMPILE ..
else
    cmake -DCPP_COMPILE=$CPP_COMPILE ..
fi

if [ "$TRAVIS_OS_NAME" = "windows" ]
then
    if [ "$CMAKE_GENERATOR" = "MinGW Makefiles" ]
    then
        mingw32-make
    else
        ls -l
        MSBuild.exe -p:Configuration=Debug soak.sln
    fi
else
    make
fi