#!/bin/bash

cd soak/build

if [ "$TRAVIS_OS_NAME" = "windows" ] && [ "$CMAKE_GENERATOR" != "MinGW Makefiles" ]
then
    # MSVC

    cd Debug # go to VS Debug folder that contains client.exe and server.exe
fi

echo "Starting soak server..."

./server &> soak_serv_out &
SERV_PID=$!
sleep 3

echo "Server started (PID: $SERV_PID)"
echo "Running soak test..."

./client --message_count=100 &> soak_cli_out

RESULT=$?

if [ $RESULT -eq 0 ]; then
    echo "Soak test completed with success!"
    echo "Printing the end of client logs..."

    cat soak_cli_out | grep -A10 -B10 "Received all soak message echoes"
else
    echo "Soak test failed!"
fi

kill $SERV_PID

exit $RESULT