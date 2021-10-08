#!/bin/bash

cd soak/build

echo "Booting soak server..."

./server --packet_loss=0.4 --packet_duplication=0.5 --ping=0.4 --jitter=0.2 &
sleep 2

echo "OK."
echo "Running soak client..."

./client --message_count=100 --packet_loss=0.6 --packet_duplication=0.5 --ping=0.2 --jitter=0.1 &> soak_cli_out