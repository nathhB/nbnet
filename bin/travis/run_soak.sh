#!/bin/bash

cd soak/build

if [ "$TRAVIS_OS_NAME" = "windows" ]
then
    if [ "$CMAKE_GENERATOR" != "MinGW Makefiles" ]
    then
        # MSVC

        cd Debug # go to VS Debug folder that contains client.exe and server.exe
    fi
fi

echo "Starting soak server..."

./server &> soak_serv_out &
SERV_PID=$!
sleep 3

echo "Server started (PID: $SERV_PID)"
echo "Running soak test..."

./client --message_count=100 &> soak_cli_out

echo "Printing end of client logs..."

cat soak_cli_out | grep -A10 -B10 "Received all soak message echoes"

kill $SERV_PID