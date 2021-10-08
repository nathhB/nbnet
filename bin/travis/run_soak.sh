#!/bin/bash

cd soak/build

if [ "$TRAVIS_OS_NAME" = "windows" ] && [ "$CMAKE_GENERATOR" != "MinGW Makefiles" ]
then
    # MSVC

    cd Debug # go to VS Debug folder that contains client.exe and server.exe
fi

echo "Starting soak server..."

./server &> soak_serv_out &
sleep 3

echo "OK."
echo "Running soak test..."

./client --message_count=100 &> soak_cli_out