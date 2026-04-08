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
SSH_OPTS=(
  -o BatchMode=yes
  -o ConnectTimeout=10
  -o StrictHostKeyChecking=accept-new
  -o ServerAliveInterval=5
  -o ServerAliveCountMax=2
)

for host in "${HOSTS[@]}"; do
  echo "[$host] stopping my_program"
  ssh -n "${SSH_OPTS[@]}" "${SSH_USER}@${host}" "pkill -f my_program || true" \
    || echo "[$host] stop command failed (check SSH key/auth)"
done

echo "Stop commands sent to all hosts."
