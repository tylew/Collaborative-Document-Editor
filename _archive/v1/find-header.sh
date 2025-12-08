#!/bin/bash
# Find where the header file actually is

cd /tmp/y-crdt-test

echo "=== Searching for header files ==="
echo ""

echo "1. Searching entire workspace:"
find . -name "*.h" -o -name "*.hpp" 2>/dev/null | head -20

echo ""
echo "2. Checking yffi directory structure:"
ls -la yffi/

echo ""
echo "3. Checking yffi/src:"
ls -la yffi/src/ 2>/dev/null | head -20

echo ""
echo "4. Checking if header is generated in target:"
find target -name "*.h" -o -name "*.hpp" 2>/dev/null

echo ""
echo "5. Checking Cargo.toml for header generation:"
grep -i "header\|bindgen\|cbindgen" yffi/Cargo.toml

echo ""
echo "6. Checking if there's a build script:"
ls -la yffi/build.rs 2>/dev/null
if [ -f "yffi/build.rs" ]; then
    echo "  build.rs exists, checking contents:"
    head -50 yffi/build.rs
fi

echo ""
echo "7. Checking yffi README or docs:"
find yffi -name "README*" -o -name "*.md" | head -5
if [ -f "yffi/README.md" ]; then
    echo "  Checking README for header info:"
    grep -i "header\|include\|\.h" yffi/README.md | head -10
fi


