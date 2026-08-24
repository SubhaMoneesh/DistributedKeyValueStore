#!/bin/bash
mkdir -p logs
rm -f logs/pids.txt
PORTS=(50051 50052 50053 50054 50055 50056)
for port in "${PORTS[@]}"; do
    ./build/kv_server "localhost:$port" cluster.conf > "logs/node_$port.log" 2>&1 &
    pid=$!
    echo "$port $pid" >> logs/pids.txt
    echo "Started node on localhost:$port (PID $pid)"
done
