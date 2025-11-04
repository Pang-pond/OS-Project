#!/usr/bin/env bash
set -euo pipefail
SESSION="ipc_chat_demo"
cd /app
tmux new-session -d -s "$SESSION" './server'
tmux split-window -h -t "$SESSION":0 './client 1001 Alice'
tmux split-window -v -t "$SESSION":0.1 './client 1002 Bob'
tmux select-pane -t "$SESSION":0.1
tmux attach -t "$SESSION"
