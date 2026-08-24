#!/bin/bash
# Usage: ./scripts/kill_node.sh <port_name>
port=$1
pid=$(grep "^$port " logs/pids.txt | awk '{print $2}')
if [ -z "$pid" ]; then
    echo "No running node found on port $port."
    exit 1
fi
kill -9 $pid
echo "Killed node on port $port (PID $pid)."