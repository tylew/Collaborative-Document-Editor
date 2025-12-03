#!/bin/bash
#
# Test Runner - Runs all delta tests in test-data directory
#
# Usage: ./test-runner.sh [-v|--verbose]
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
VERBOSE=false
if [ "$1" = "-v" ] || [ "$1" = "--verbose" ]; then
    VERBOSE=true
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEST_DATA_DIR="$SCRIPT_DIR/../test-data"
TEST_BIN="$SCRIPT_DIR/test-deltas"

# Check if test binary exists
if [ ! -f "$TEST_BIN" ]; then
    echo "Error: test-deltas binary not found"
    echo "Run 'make' first to build the test binary"
    exit 1
fi

# Check if test data directory exists
if [ ! -d "$TEST_DATA_DIR" ]; then
    echo "Error: test-data directory not found at $TEST_DATA_DIR"
    echo "Run 'cd ../js-delta-generator && npm run generate' first"
    exit 1
fi

# Count test directories
TEST_DIRS=()
for dir in "$TEST_DATA_DIR"/*; do
    if [ -d "$dir" ]; then
        TEST_DIRS+=("$dir")
    fi
done

if [ ${#TEST_DIRS[@]} -eq 0 ]; then
    echo "Error: No test directories found in $TEST_DATA_DIR"
    echo "Run 'cd ../js-delta-generator && npm run generate' first"
    exit 1
fi

echo "========================================"
echo "CRDT Delta Test Runner"
echo "========================================"
echo "Test data: $TEST_DATA_DIR"
echo "Found ${#TEST_DIRS[@]} test(s)"
if [ "$VERBOSE" = true ]; then
    echo "Mode: VERBOSE (showing all output)"
else
    echo "Mode: SUMMARY (use -v for verbose)"
fi
echo ""

# Run each test
PASSED=0
FAILED=0
FAILED_TESTS=()

for test_dir in "${TEST_DIRS[@]}"; do
    test_name=$(basename "$test_dir")
    
    if [ "$VERBOSE" = true ]; then
        # Verbose mode: show all output
        echo ""
        echo "----------------------------------------"
        echo "Running: $test_name"
        echo "----------------------------------------"
        if "$TEST_BIN" "$test_dir"; then
            echo -e "${GREEN}✓ PASSED${NC}"
            PASSED=$((PASSED + 1))
        else
            echo -e "${RED}✗ FAILED${NC}"
            FAILED=$((FAILED + 1))
            FAILED_TESTS+=("$test_name")
        fi
    else
        # Summary mode: capture output, only show failures
        if "$TEST_BIN" "$test_dir" > /tmp/test_output_$$.txt 2>&1; then
            echo -e "${GREEN}✓${NC} $test_name"
            PASSED=$((PASSED + 1))
        else
            echo -e "${RED}✗${NC} $test_name"
            FAILED=$((FAILED + 1))
            FAILED_TESTS+=("$test_name")
            
            # Show error output
            if [ -f /tmp/test_output_$$.txt ]; then
                echo "  Output:"
                cat /tmp/test_output_$$.txt | sed 's/^/    /'
            fi
        fi
        
        # Cleanup temp file
        rm -f /tmp/test_output_$$.txt
    fi
done

echo ""
echo "========================================"
echo "Test Summary"
echo "========================================"
echo -e "Total:  ${#TEST_DIRS[@]}"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"

if [ $FAILED -gt 0 ]; then
    echo ""
    echo "Failed tests:"
    for test in "${FAILED_TESTS[@]}"; do
        echo "  - $test"
    done
    echo ""
    echo "To debug a specific test:"
    echo "  ./test-deltas ../test-data/<test-name>"
    exit 1
else
    echo ""
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi

