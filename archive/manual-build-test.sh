#!/bin/bash
# Manual build test to see what cargo actually produces

echo "=== Manual yffi build test ==="
cd /tmp

# Clone fresh
echo "1. Cloning y-crdt..."
rm -rf y-crdt-test
git clone --depth 1 https://github.com/y-crdt/y-crdt.git y-crdt-test

echo ""
echo "2. Checking repository structure:"
ls -la y-crdt-test/

echo ""
echo "3. Checking yffi directory:"
if [ -d "y-crdt-test/yffi" ]; then
    echo "  ✓ yffi directory exists"
    ls -la y-crdt-test/yffi/
    
    echo ""
    echo "4. Checking Cargo.toml:"
    cat y-crdt-test/yffi/Cargo.toml | head -50
    
    echo ""
    echo "5. Looking for header files:"
    find y-crdt-test/yffi -name "*.h" -o -name "*.hpp"
    
    echo ""
    echo "6. Building yffi:"
    cd y-crdt-test/yffi
    cargo build --release
    
    echo ""
    echo "7. What was built:"
    echo "  Checking workspace root target (yffi is in a workspace):"
    ls -la ../target/release/ 2>/dev/null | head -30 || echo "  Not in workspace root"
    
    echo ""
    echo "  Checking yffi target:"
    ls -la target/release/ 2>/dev/null | head -30 || echo "  No target/release in yffi"
    
    echo ""
    echo "  Searching for .so files in workspace:"
    find ../target/release -name "*.so" -type f 2>/dev/null
    
    echo ""
    echo "  Searching for .a files in workspace:"
    find ../target/release -name "*.a" -type f 2>/dev/null | grep yffi
    
    echo ""
    echo "  Searching for .rlib files:"
    find ../target/release -name "*yffi*.rlib" -type f 2>/dev/null
    
    echo ""
    echo "8. Checking Cargo.toml for library type:"
    cat Cargo.toml | grep -A 5 "\[lib\]"
    
    echo ""
    echo "8. Checking for header files after build:"
    find . -name "*.h" -o -name "*.hpp"
    
else
    echo "  ✗ yffi directory not found!"
    echo "  Available directories:"
    ls -la y-crdt-test/
fi

