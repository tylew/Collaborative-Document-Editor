#!/bin/bash
#
# Run JavaScript Delta Generator (on host)
#

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
JS_DIR="$SCRIPT_DIR/js-delta-generator"

echo "========================================"
echo "JavaScript Delta Generator"
echo "========================================"
echo "Running on: Host machine"
echo "Directory: $JS_DIR"
echo ""

# Check if Node.js is available
if ! command -v node &> /dev/null; then
    echo "Error: Node.js is not installed"
    echo "Please install Node.js 18+ to run the generator"
    exit 1
fi

# Navigate to js directory
cd "$JS_DIR"

# Install dependencies if needed
if [ ! -d "node_modules" ]; then
    echo "Installing dependencies..."
    npm install
    echo ""
fi

# Run generator
echo "Generating test deltas..."
npm run generate

echo ""
echo "========================================"
echo "Generation complete!"
echo "========================================"
echo ""
echo "Test data generated in: $SCRIPT_DIR/test-data"
echo ""
echo "Next step: Run C++ tests in Docker"
echo "  ./run-cpp-tests.sh"

