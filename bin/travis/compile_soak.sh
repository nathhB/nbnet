#!/bin/bash

cd soak
mkdir build
cd build

if [ "$TRAVIS_OS_NAME" = "windows" ]
then
    cmake -G "MinGW Makefiles" -DCPP_COMPILE=$CPP_COMPILE ..
else
    cmake -DCPP_COMPILE=$CPP_COMPILE ..
fi

if [ "$TRAVIS_OS_NAME" = "windows" ]
then
    mingw32-make
else
    make
fi