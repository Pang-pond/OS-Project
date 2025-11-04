#!/usr/bin/env bash
set -euo pipefail
cd /app
CID="${1:-1001}"
NAME="${2:-Alice}"
exec ./client "$CID" "$NAME"
