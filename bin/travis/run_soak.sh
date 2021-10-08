#!/bin/bash

cd soak/build

echo "Booting server..."

./server --packet_loss=0.4 --packet_duplication=0.5 --ping=0.4 --jitter=0.2 &
sleep 2

echo "Booting client..."

./client --message_count=100 --packet_loss=0.6 --packet_duplication=0.5 --ping=0.2 --jitter=0.1