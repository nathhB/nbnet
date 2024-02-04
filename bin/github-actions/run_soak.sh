#!/bin/bash

PACKET_LOSS=0.3
PACKET_DUPLICATION=0.2
PING=0.15
JITTER=0.1
CHANNEL_COUNT=3
MESSAGE_COUNT=500
NODE_CMD="$EMSDK_NODE --experimental-wasm-threads"

run_client () {
    node_client=$1
    echo "Running soak client (run in mode: $node_client)..."

    if [ $node_client -eq 1 ]
    then
        # WASM WebRTC client
        $NODE_CMD build_web/client.js --message_count=$MESSAGE_COUNT --channel_count=$CHANNEL_COUNT --packet_loss=$PACKET_LOSS --packet_duplication=$PACKET_DUPLICATION --ping=$PING --jitter=$JITTER &> soak_cli_out
    elif [ $node_client -eq 2 ]
    then
        # native WebRTC client
        ./build/client --webrtc --message_count=$MESSAGE_COUNT --channel_count=$CHANNEL_COUNT --packet_loss=$PACKET_LOSS --packet_duplication=$PACKET_DUPLICATION --ping=$PING --jitter=$JITTER &> soak_cli_out
    else
        # UDP client
        ./build/client --message_count=$MESSAGE_COUNT --channel_count=$CHANNEL_COUNT --packet_loss=$PACKET_LOSS --packet_duplication=$PACKET_DUPLICATION --ping=$PING --jitter=$JITTER &> soak_cli_out
    fi

    RESULT=$?

    # when running the soak test in the latest version of emscripten with node 16
    # the client aborts at the end when calling emscripten_force_exit
    # I could not figure out why, hence the condition
    [[ $node_client -eq 1 ]] && EXPECTED_RESULT=7 || EXPECTED_RESULT=0
    
    if [ $RESULT -eq $EXPECTED_RESULT ]
    then
        echo "Soak test completed with success!"
        echo "Printing the end of client logs..."
    
        tail -n 150 soak_cli_out
    
        return 0
    else
        echo "Soak test failed! (code: $RESULT)"
        echo "Printing the end of client logs..."
        tail -n 150 soak_cli_out
        echo "Printing the end of server logs..."
        tail -n 150 soak_serv_out

        return 1
    fi
}

exit_soak () {
    kill -SIGINT $SERV_PID 2> /dev/null

    exit $1
}

cd soak
echo "Starting soak server..."

if [ -n "$WEBRTC" ]
then
     $NODE_CMD build_web/server.js --channel_count=$CHANNEL_COUNT --packet_loss=$PACKET_LOSS --packet_duplication=$PACKET_DUPLICATION --ping=$PING --jitter=$JITTER &> soak_serv_out &
else
    ./build/server --channel_count=$CHANNEL_COUNT --packet_loss=$PACKET_LOSS --packet_duplication=$PACKET_DUPLICATION --ping=$PING --jitter=$JITTER &> soak_serv_out &
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

if [ -n "$WEBRTC" ]
then
    run_client 1

    exit_soak $?
else
    if [ -n "$WEBRTC_NATIVE" ]
    then
        # run a UDP client, a webrtc WASM client (emscripten) and a native webrtc client (all connecting to the same server)

        if run_client 0 && run_client 1 && run_client 2; then
            exit_soak 0
        else
            exit_soak 1
        fi
    fi
fi
