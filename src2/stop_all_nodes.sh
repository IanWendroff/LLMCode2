#!/usr/bin/env bash
set -euo pipefail

# Stop all DHT nodes on all 4 hosts.
#
# Usage:
#   ./stop_all_nodes.sh <ssh_user>

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <ssh_user>"
  exit 1
fi

SSH_USER="$1"
HOSTS=("128.180.120.65" "128.180.120.66" "128.180.120.77" "128.180.120.68")

for host in "${HOSTS[@]}"; do
  echo "[$host] stopping my_program"
  ssh "${SSH_USER}@${host}" "pkill -f my_program || true"
done

echo "Stop commands sent to all hosts."
