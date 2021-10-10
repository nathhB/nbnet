#!/bin/bash

if [ -n "$EMSCRIPTEN" ]
then
    cd /nbnet/soak
    npm install
else
    cd soak/build
fi

if [ "$TRAVIS_OS_NAME" = "windows" ] && [ "$CMAKE_GENERATOR" != "MinGW Makefiles" ]
then
    # MSVC

    cd Debug # go to VS Debug folder that contains client.exe and server.exe
fi

echo "Starting soak server..."

if [ -n "$EMSCRIPTEN" ]
then
    npm run server -- --packet_loss=0.6 --packet_duplication=0.5 --ping=0.3 --jitter=0.2 &> soak_serv_out &
else
    ./server --packet_loss=0.6 --packet_duplication=0.5 --ping=0.3 --jitter=0.2 &> soak_serv_out &
fi

SERV_PID=$!
sleep 3

echo "Server started (PID: $SERV_PID)"
echo "Running soak test..."

if [ -n "$EMSCRIPTEN" ]
then
    npm run client -- --message_count=500 --packet_loss=0.4 --packet_duplication=0.5 --ping=0.3 --jitter=0.2 &> soak_cli_out
else
    ./client --message_count=500 --packet_loss=0.4 --packet_duplication=0.5 --ping=0.3 --jitter=0.2 &> soak_cli_out
fi

RESULT=$?

if [ $RESULT -eq 0 ]
then
    echo "Soak test completed with success!"
    echo "Printing the end of client logs..."

    cat soak_cli_out | grep -A10 -B10 "Received all soak message echoes"
else
    echo "Soak test failed!"
fi

kill $SERV_PID

exit $RESULT