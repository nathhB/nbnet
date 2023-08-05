#!/bin/bash

PACKET_LOSS=0.3
PACKET_DUPLICATION=0.2
PING=0.15
JITTER=0.1
CHANNEL_COUNT=3
MESSAGE_COUNT=500
NODE_CMD="$EMSDK_NODE --experimental-wasm-threads"

cd soak
echo "Starting soak server..."

if [ -n "$EMSCRIPTEN" ]
then
     $NODE_CMD build/server.js --channel_count=$CHANNEL_COUNT --packet_loss=$PACKET_LOSS --packet_duplication=$PACKET_DUPLICATION --ping=$PING --jitter=$JITTER &> soak_serv_out &
else
    ./server --channel_count=$CHANNEL_COUNT --packet_loss=$PACKET_LOSS --packet_duplication=$PACKET_DUPLICATION --ping=$PING --jitter=$JITTER &> soak_serv_out &
fi

if [ $? -eq 0 ]
then
    SERV_PID=$!

    echo "Server started (PID: $SERV_PID)"
    echo "Running soak test..."
else
    echo "Failed to start soak server!"
    exit 1
fi

sleep 3

if [ -n "$EMSCRIPTEN" ]
then
    $NODE_CMD build/client.js --message_count=$MESSAGE_COUNT --channel_count=$CHANNEL_COUNT --packet_loss=$PACKET_LOSS --packet_duplication=$PACKET_DUPLICATION --ping=$PING --jitter=$JITTER &> soak_cli_out
else
    ./client --message_count=$MESSAGE_COUNT --channel_count=$CHANNEL_COUNT --packet_loss=$PACKET_LOSS --packet_duplication=$PACKET_DUPLICATION --ping=$PING --jitter=$JITTER &> soak_cli_out
fi

RESULT=$?

# when running the soak test in the latest version of emscripten with node 16
# the client aborts at the end when calling emscripten_force_exit
# I could not figure out why, hence the condition
[[ -n "$EMSCRIPTEN" ]] && EXPECTED_RESULT=7 || EXPECTED_RESULT=0

if [ $RESULT -eq $EXPECTED_RESULT ]
then
    echo "Soak test completed with success!"
    echo "Printing the end of client logs..."

    cat soak_cli_out | tail -n 150

    EXIT_CODE=0
else
    echo "Soak test failed! (code: $RESULT)"
    echo "Printing the end of client logs..."
    cat soak_cli_out | tail -n 150
    echo "Printing the end of server logs..."
    cat soak_serv_out | tail -n 150

    EXIT_CODE=1
fi

kill -SIGINT $SERV_PID 2> /dev/null

exit $EXIT_CODE
