#!/bin/bash
#
# Run Full Test Suite (JS Generator + C++ Tests)
#

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "========================================"
echo "CRDT Delta Test Suite"
echo "========================================"
echo ""
echo "This will:"
echo "  1. Generate test deltas (Node.js on host)"
echo "  2. Run C++ validation tests (in Docker)"
echo ""

# Step 1: Generate deltas
echo "Step 1: Generating test deltas..."
echo ""
"$SCRIPT_DIR/run-js-generator.sh"

echo ""
echo "----------------------------------------"
echo ""

# Step 2: Run C++ tests
echo "Step 2: Running C++ tests..."
echo ""
"$SCRIPT_DIR/run-cpp-tests.sh"

echo ""
echo "========================================"
echo "Full test suite complete!"
echo "========================================"

