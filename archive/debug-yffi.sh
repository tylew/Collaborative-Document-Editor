#!/bin/bash
# Debug script to figure out what happened with yffi build

echo "=== Debugging yffi installation ==="
echo ""

echo "1. Checking if Rust/Cargo is installed:"
which cargo || echo "  Cargo not found"
cargo --version 2>/dev/null || echo "  Cargo not working"

echo ""
echo "2. Checking if y-crdt repo exists:"
if [ -d "/tmp/y-crdt" ]; then
    echo "  ✓ /tmp/y-crdt exists"
    ls -la /tmp/y-crdt/
else
    echo "  ✗ /tmp/y-crdt does not exist (was cleaned up)"
fi

echo ""
echo "3. Checking yffi directory structure (if repo exists):"
if [ -d "/tmp/y-crdt/yffi" ]; then
    echo "  yffi directory contents:"
    ls -la /tmp/y-crdt/yffi/
    echo ""
    echo "  Cargo.toml:"
    cat /tmp/y-crdt/yffi/Cargo.toml 2>/dev/null | head -30 || echo "  No Cargo.toml"
    echo ""
    echo "  Build output:"
    ls -la /tmp/y-crdt/yffi/target/release/ 2>/dev/null | head -20 || echo "  No target/release directory"
    echo ""
    echo "  Header files:"
    find /tmp/y-crdt/yffi -name "*.h" -o -name "*.hpp" 2>/dev/null
else
    echo "  ✗ yffi directory not found"
fi

echo ""
echo "4. Let's try to clone and build manually:"
echo "  Cloning y-crdt..."
cd /tmp
if [ -d "y-crdt-test" ]; then
    rm -rf y-crdt-test
fi
git clone --depth 1 https://github.com/y-crdt/y-crdt.git y-crdt-test 2>&1 | tail -5

echo ""
echo "5. Checking yffi directory in fresh clone:"
if [ -d "y-crdt-test/yffi" ]; then
    echo "  ✓ yffi directory exists"
    ls -la y-crdt-test/yffi/ | head -20
    echo ""
    echo "  Looking for header files:"
    find y-crdt-test/yffi -name "*.h" -o -name "*.hpp" 2>/dev/null
    echo ""
    echo "  Checking Cargo.toml for library name:"
    grep -E "name|lib|cdylib" y-crdt-test/yffi/Cargo.toml 2>/dev/null | head -10
else
    echo "  ✗ yffi directory not found in clone"
    echo "  Repository structure:"
    ls -la y-crdt-test/ | head -20
fi

echo ""
echo "6. Trying to build yffi manually:"
if [ -d "y-crdt-test/yffi" ]; then
    cd y-crdt-test/yffi
    echo "  Running: cargo build --release"
    cargo build --release 2>&1 | tail -20
    echo ""
    echo "  Build output files:"
    find target/release -name "*.so" -o -name "*.a" 2>/dev/null
fi


