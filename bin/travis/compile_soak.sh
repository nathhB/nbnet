#!/bin/bash

cd soak
mkdir build
cd build

if [ "$TRAVIS_OS_NAME" = "windows" ]
then
    cmake -G "MinGW Makefiles" ..
else
    cmake ..
fi

if [ "$TRAVIS_OS_NAME" = "windows" ]
then
    mingw32-make
else
    make
fi