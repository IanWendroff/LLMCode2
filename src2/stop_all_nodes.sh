#!/usr/bin/env bash
set -euo pipefail

# Stop all DHT nodes on all 4 hosts.
#
# Usage:
#   ./stop_all_nodes.sh <ssh_user> [process_owner]

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <ssh_user> [process_owner]"
  exit 1
fi

SSH_USER="$1"
PROCESS_OWNER="${2:-$SSH_USER}"
HOSTS=("128.180.120.65" "128.180.120.66" "128.180.120.77" "128.180.120.68")
SSH_OPTS=(
  -o BatchMode=yes
  -o ConnectTimeout=10
  -o StrictHostKeyChecking=accept-new
  -o ServerAliveInterval=5
  -o ServerAliveCountMax=2
)

for host in "${HOSTS[@]}"; do
  echo "[$host] stopping my_program for user=$PROCESS_OWNER"
  if ssh -n "${SSH_OPTS[@]}" "${SSH_USER}@${host}" \
    "matches=\$(pgrep -u '$PROCESS_OWNER' -f '/my_program( |$)' || true)
     if [[ -n \"\$matches\" ]]; then
       pkill -u '$PROCESS_OWNER' -f '/my_program( |$)' || true
       sleep 1
       remaining=\$(pgrep -u '$PROCESS_OWNER' -f '/my_program( |$)' || true)
       if [[ -n \"\$remaining\" ]]; then
         echo 'stop failed: owned my_program processes still running'
         echo "\$remaining" | tr '\n' ' '
         echo
         exit 2
       fi
       echo 'stopped owned my_program processes'
     else
       echo 'no owned my_program processes found'
     fi"; then
    :
  else
    rc=$?
    if [[ $rc -eq 255 ]]; then
      echo "[$host] SSH connection/auth failed"
    else
      echo "[$host] stop command failed (remote exit=$rc)"
    fi
  fi
done

echo "Stop commands sent to all hosts."
