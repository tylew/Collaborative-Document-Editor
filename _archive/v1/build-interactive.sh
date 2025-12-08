#!/bin/bash
# Interactive build script - run this inside the container

echo "=== Interactive yffi build ==="
echo ""

# Make sure we're in the workspace root
cd /tmp/y-crdt-test

echo "1. Current directory structure:"
ls -la

echo ""
echo "2. Building yffi package:"
cargo build --release --package yffi

echo ""
echo "3. What was built:"
echo "  Library files:"
ls -lh target/release/libyrs.*

echo ""
echo "4. Checking for header file:"
find yffi -name "*.h" -o -name "*.hpp"

echo ""
echo "5. Installing manually:"
if [ -f "target/release/libyrs.so" ]; then
    echo "  Copying libyrs.so..."
    cp target/release/libyrs.so /usr/local/lib/
    echo "  ✓ Copied"
else
    echo "  ✗ libyrs.so not found"
fi

if [ -f "yffi/yffi.h" ]; then
    echo "  Copying yffi.h..."
    cp yffi/yffi.h /usr/local/include/
    echo "  ✓ Copied"
else
    echo "  Searching for header..."
    find yffi -name "*.h" -exec sh -c 'echo "Found: $1"; cp "$1" /usr/local/include/yffi.h' _ {} \;
fi

echo ""
echo "6. Verifying installation:"
ls -la /usr/local/lib/libyrs* 2>/dev/null || echo "  No libyrs in /usr/local/lib"
ls -la /usr/local/include/yffi.h 2>/dev/null || echo "  No yffi.h in /usr/local/include"

echo ""
echo "7. Running ldconfig:"
ldconfig

echo ""
echo "8. Testing library availability:"
ldconfig -p | grep yrs || echo "  Not in cache yet"

echo ""
echo "=== Done ==="


