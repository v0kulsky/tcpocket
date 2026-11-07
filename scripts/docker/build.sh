#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

docker run --rm \
    --user "$(id -u):$(id -g)" \
    -v "$ROOT_DIR:/workspace" \
    -w /workspace \
    tcpocket-dev-env bash -c 'cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build "$@"' -- "$@"
