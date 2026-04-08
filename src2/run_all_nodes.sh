#!/usr/bin/env bash
set -euo pipefail

# Launch all 4 DHT nodes from one machine via SSH.
#
# Usage:
#   ./run_all_nodes.sh quick <your_user>
#   ./run_all_nodes.sh stress <your_user>
#
# Modes:
#   quick  -> threads=1, ops=5
#   stress -> threads=3, ops=1000
#
# Node mapping (network.cpp):
#   node 0 -> 128.180.120.65
#   node 1 -> 128.180.120.66
#   node 2 -> 128.180.120.77
#   node 3 -> 128.180.120.68

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <quick|stress> <ssh_user>"
  exit 1
fi

MODE="$1"
SSH_USER="$2"
BASE_DIR="/home/itw227/CSE376/LLMCode2/src2"
HOSTS=("128.180.120.65" "128.180.120.66" "128.180.120.77" "128.180.120.68")

case "$MODE" in
  quick)
    THREADS=1
    OPS=5
    ;;
  stress)
    THREADS=3
    OPS=1000
    ;;
  *)
    echo "mode must be quick or stress"
    exit 1
    ;;
esac

echo "Launching mode=$MODE (threads=$THREADS ops=$OPS)"

for i in 0 1 2 3; do
  host="${HOSTS[$i]}"
  echo "[$host] starting node $i"
  ssh "${SSH_USER}@${host}" "cd \"$BASE_DIR\" && make >/dev/null && nohup ./my_program $i $THREADS $OPS > node${i}.log 2>&1 & echo started node $i"
done

echo "All launch commands sent."
echo "To stop all nodes, run: ./stop_all_nodes.sh <ssh_user>"
