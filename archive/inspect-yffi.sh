#!/bin/bash
# Script to inspect yffi installation
# Run this inside the dev container

echo "=== Inspecting yffi installation ==="
echo ""

echo "1. Checking installed libraries in /usr/local/lib:"
ls -la /usr/local/lib/ | grep -E "yffi|yrs" || echo "  No yffi/yrs libraries found"

echo ""
echo "2. Checking all .so files in /usr/local/lib:"
find /usr/local/lib -name "*.so" -type f | grep -E "yffi|yrs" || echo "  No matching .so files"

echo ""
echo "3. Checking installed headers in /usr/local/include:"
ls -la /usr/local/include/ | grep -E "yffi|yrs" || echo "  No yffi/yrs headers found"

echo ""
echo "4. Checking for yffi.h specifically:"
if [ -f "/usr/local/include/yffi.h" ]; then
    echo "  ✓ Found: /usr/local/include/yffi.h"
    head -20 /usr/local/include/yffi.h
else
    echo "  ✗ Not found: /usr/local/include/yffi.h"
    echo "  Searching for any .h files:"
    find /usr/local/include -name "*yffi*" -o -name "*yrs*" 2>/dev/null
fi

echo ""
echo "5. Checking library cache:"
ldconfig -p | grep -E "yffi|yrs" || echo "  No yffi/yrs in ldconfig cache"

echo ""
echo "6. Testing library linking:"
if ldconfig -p | grep -q yffi; then
    echo "  ✓ yffi library is available for linking"
else
    echo "  ✗ yffi library not found in linker cache"
    echo "  Try running: ldconfig"
fi

echo ""
echo "7. Checking if we can find the library by name:"
gcc -lyffi -v 2>&1 | grep -A 5 "library.*yffi" || echo "  Cannot find yffi library"

