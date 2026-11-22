#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Allocate TTY if stdin and stdout are terminals
DOCKER_INTERACTIVE=()
if [ -t 0 ] && [ -t 1 ]; then
    DOCKER_INTERACTIVE+=("-it")
elif [ -t 0 ]; then
    DOCKER_INTERACTIVE+=("-i")
fi

docker run --rm ${DOCKER_INTERACTIVE:+"${DOCKER_INTERACTIVE[@]}"} \
    --net=host \
    --user "$(id -u):$(id -g)" \
    -v "$ROOT_DIR:/workspace" \
    -w /workspace \
    tcpocket-dev-env bash -c '
        set -euo pipefail

        # Ensure build directory is configured with BUILD_EXAMPLES=ON
        if [ ! -f build/build.ninja ]; then
            cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_EXAMPLES=ON
        else
            cmake -B build -DBUILD_EXAMPLES=ON
        fi

        # If no arguments provided or help requested, build all examples and show usage
        if [ $# -eq 0 ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
            echo "Building all sample applications..."
            cmake --build build --target echo_server echo_client http_get
            echo ""
            echo "Available examples built in build/:"
            echo "  - echo_server [port]               (e.g., ./scripts/docker/example.sh echo_server 8080)"
            echo "  - echo_client [host] [port] [msg]  (e.g., ./scripts/docker/example.sh echo_client 127.0.0.1 8080)"
            echo "  - http_get    [host] [path] [port] (e.g., ./scripts/docker/example.sh http_get example.com / 80)"
            echo ""
            echo "Usage: ./scripts/docker/example.sh <example_name> [args...]"
            exit 0
        fi

        EXAMPLE="$1"
        shift

        case "$EXAMPLE" in
            echo_server|echo_client|http_get)
                cmake --build build --target "$EXAMPLE"
                exec "./build/$EXAMPLE" "$@"
                ;;
            build)
                echo "Building all sample applications..."
                cmake --build build --target echo_server echo_client http_get
                echo "Examples built successfully."
                ;;
            *)
                echo "Unknown example: $EXAMPLE" >&2
                echo "Available examples: echo_server, echo_client, http_get" >&2
                exit 1
                ;;
        esac
    ' -- "$@"
