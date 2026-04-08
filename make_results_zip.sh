#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./make_results_zip.sh [ssh_user]
# Example:
#   ./make_results_zip.sh itw227

SSH_USER="${1:-itw227}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$PROJECT_ROOT/src2"

HOSTS=(
  "128.180.120.65"
  "128.180.120.66"
  "128.180.120.77"
  "128.180.120.68"
)

echo "[1/4] Running quick test with key_range=5"
cd "$SRC_DIR"
./run_all_nodes.sh quick "$SSH_USER" 5

echo "[2/4] Running quick test with key_range=10"
./run_all_nodes.sh quick "$SSH_USER" 10

echo "Waiting briefly for nodes to finish..."
sleep 10

echo "[3/4] Collecting logs and metrics from remote hosts"
for i in "${!HOSTS[@]}"; do
  host="${HOSTS[$i]}"
  scp "$SSH_USER@$host:/home/$SSH_USER/CSE376/LLMCode2/src2/node${i}.log" "$SRC_DIR/"
  scp "$SSH_USER@$host:/home/$SSH_USER/CSE376/LLMCode2/src2/metrics_node_${i}_keyrange_5.csv" "$SRC_DIR/"
  scp "$SSH_USER@$host:/home/$SSH_USER/CSE376/LLMCode2/src2/metrics_node_${i}_keyrange_10.csv" "$SRC_DIR/"
done

echo "[4/4] Building results submission archive"
cd "$PROJECT_ROOT"
rm -f LLMCode2_submission_with_results.zip
zip -r LLMCode2_submission_with_results.zip . \
  -x '*.zip' \
  -x '*.git*' \
  -x '*/.git/*' \
  -x '*.o' \
  -x 'src2/my_program' \
  -x '*.DS_Store' \
  -x '__MACOSX/*'

echo
echo "Created: $PROJECT_ROOT/LLMCode2_submission_with_results.zip"
echo "Verification:" 
unzip -l LLMCode2_submission_with_results.zip | egrep 'node[0-3]\.log|keyrange_5|keyrange_10' || true
