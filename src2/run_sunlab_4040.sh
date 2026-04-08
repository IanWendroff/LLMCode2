#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./run_sunlab_4040.sh quick <node_id>
#   ./run_sunlab_4040.sh stress <node_id>
#
# Node mapping (from network.cpp):
#   node 0 -> 128.180.120.65
#   node 1 -> 128.180.120.66
#   node 2 -> 128.180.120.77
#   node 3 -> 128.180.120.68
#
# Run this script ON the matching machine for its node_id.

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <quick|stress> <node_id>"
  exit 1
fi

mode="$1"
node_id="$2"

if [[ "$node_id" != "0" && "$node_id" != "1" && "$node_id" != "2" && "$node_id" != "3" ]]; then
  echo "node_id must be 0, 1, 2, or 3"
  exit 1
fi

if [[ ! -x ./my_program ]]; then
  echo "Building my_program..."
  make
fi

case "$mode" in
  quick)
    threads=1
    ops=5
    ;;
  stress)
    threads=3
    ops=1000
    ;;
  *)
    echo "mode must be quick or stress"
    exit 1
    ;;
esac

log_file="node${node_id}.log"

echo "Starting node ${node_id} with threads=${threads}, ops=${ops} on port 4040 config..."
./my_program "$node_id" "$threads" "$ops" > "$log_file" 2>&1 &
echo "Started PID $!"
echo "Log: $log_file"
echo "To stop all nodes: pkill -f my_program"
