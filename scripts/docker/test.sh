#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

docker run --rm \
    --user "$(id -u):$(id -g)" \
    -v "$ROOT_DIR:/workspace" \
    -w /workspace \
    tcpocket-dev-env bash -c '
        if [ ! -f build/build.ninja ]; then
            cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
        fi
        cmake --build build --target unit_tests
        ctest --test-dir build --output-on-failure "$@"
    ' -- "$@"
