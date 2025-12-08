#!/bin/bash
#
# Run C++ Delta Tests (in Docker container)
#

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."
CPP_DIR="$SCRIPT_DIR/cpp-delta-consumer"

echo "========================================"
echo "C++ Delta Consumer Tests"
echo "========================================"
echo "Running in: Docker container"
echo "Image: crdt-cpp-host-base:latest"
echo ""

# Check if test data exists
if [ ! -d "$SCRIPT_DIR/test-data" ] || [ -z "$(ls -A "$SCRIPT_DIR/test-data")" ]; then
    echo "Error: No test data found"
    echo "Please run the JavaScript generator first:"
    echo "  ./run-js-generator.sh"
    exit 1
fi

# Check if Docker image exists, if not build via docker-compose
if ! docker images | grep -q "crdt-cpp-host-base"; then
    echo "Docker image 'crdt-cpp-host-base' not found"
    echo "Building image via docker-compose..."
    echo ""
    cd "$PROJECT_ROOT"
    docker compose build server-base-image
    cd "$SCRIPT_DIR"
    echo ""
fi

echo "Running tests in Docker container..."
echo ""

# Run tests in Docker container with volume mounts
# Mount the entire project root (as per docker-compose.yml)
docker run --rm \
    -v "$PROJECT_ROOT:/app" \
    -w /app/tests/cpp-delta-consumer \
    crdt-cpp-host-base:latest \
    bash -c "
        set -e
        echo 'Building test binary...'
        make clean
        make
        echo ''
        echo 'Running tests in VERBOSE mode...'
        echo ''
        chmod +x test-runner.sh
        LD_LIBRARY_PATH=/usr/local/lib ./test-runner.sh --verbose
    "

echo ""
echo "========================================"
echo "Tests complete!"
echo "========================================"

