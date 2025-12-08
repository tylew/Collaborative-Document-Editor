# CRDT Delta Test Framework - Quick Reference

## What Was Built

A lightweight test framework to debug CRDT delta synchronization between JavaScript (Yjs) and C++ (libyrs).

## Directory Structure

```
tests/
├── README.md                    # Complete documentation
├── run-all-tests.sh            # Run full test suite
├── run-js-generator.sh         # Generate deltas (host)
├── run-cpp-tests.sh            # Run tests (Docker)
│
├── js-delta-generator/         # JavaScript delta creator
│   ├── package.json
│   ├── generate-test-deltas.js # Main generator script
│   └── scenarios.js            # 10 test scenarios
│
├── cpp-delta-consumer/         # C++ delta validator
│   ├── Makefile
│   ├── test-deltas.cpp         # Test harness
│   └── test-runner.sh          # Runs all tests
│
└── test-data/                  # Generated test files
    ├── 001-single-insert/
    ├── 002-string-insert/
    ├── 003-sequential-edits/
    └── ... (10 scenarios)
```

## Usage

### Quick Start (Easiest)

```bash
cd tests
chmod +x *.sh
./run-all-tests.sh
```

### Step by Step

**1. Generate test deltas (on host):**
```bash
cd tests
./run-js-generator.sh
```

**2. Run C++ tests (in Docker):**
```bash
cd tests
./run-cpp-tests.sh
```

## Test Scenarios

1. **001-single-insert** - Insert "a"
2. **002-string-insert** - Insert "hello world"
3. **003-sequential-edits** - Insert "hello", then " world"
4. **004-delete-operation** - Insert "hello", delete "l"
5. **005-multiple-deletes** - Insert "hello world", delete "o w"
6. **006-insert-middle** - Insert "helo", then "l" in middle
7. **007-replace-text** - Insert, delete all, insert new
8. **008-empty-to-text** - Long text insertion
9. **009-concurrent-merge** - Simulate two docs merging
10. **010-rapid-edits** - Simulate typing character by character

## What Gets Tested

✓ **Single character inserts**  
✓ **String insertions**  
✓ **Sequential operations**  
✓ **Delete operations**  
✓ **Insert in middle**  
✓ **Replace text**  
✓ **Concurrent merges**  
✓ **Rapid typing simulation**  

## Output Format

```
========================================
CRDT Delta Test Runner
========================================
Found 10 test(s)

✓ 001-single-insert
✓ 002-string-insert
✗ 004-delete-operation
  Expected: "helo"
  Got: "hello"
  Error: Content mismatch

========================================
Test Summary
========================================
Total:  10
Passed: 9
Failed: 1
```

## Key Features

- **Isolated**: Tests libyrs without WebSocket complexity
- **Fast**: Runs in seconds
- **Reproducible**: Same deltas every time
- **Debuggable**: Hex dumps, verbose logging
- **Cross-platform**: JS on host, C++ in Docker

## Debugging a Failure

**1. Run single test:**
```bash
docker run --rm -v $(pwd)/..:/app -w /app/tests/cpp-delta-consumer crdt-cpp-host-base:latest \
    bash -c "make && LD_LIBRARY_PATH=/usr/local/lib ./test-deltas ../test-data/004-delete-operation"
```

**2. Check test files:**
```bash
cd test-data/004-delete-operation
ls -lh              # See files
cat expected.txt    # See expected output
hexdump -C delta.bin | head  # See binary delta
cat meta.json       # See metadata
```

**3. Add verbose logging:**
Edit `cpp-delta-consumer/test-deltas.cpp`, add more `printf` statements

## Adding New Test Scenarios

**1. Edit scenarios:**
```bash
vim js-delta-generator/scenarios.js
```

**2. Add scenario:**
```javascript
{
  id: '011-my-test',
  name: 'My Test',
  description: 'Tests my specific case',
  operations: [
    { type: 'insert', index: 0, text: 'test' }
  ]
}
```

**3. Regenerate and test:**
```bash
./run-all-tests.sh
```

## Capture Real Production Deltas

**In your server code:**
```cpp
// In document_apply_update()
FILE *f = fopen("production_delta.bin", "wb");
fwrite(data, 1, len, f);
fclose(f);
```

**Add to test suite:**
```bash
mkdir test-data/999-production-failure
cp production_delta.bin test-data/999-production-failure/delta.bin
echo "expected text" > test-data/999-production-failure/expected.txt
./run-cpp-tests.sh
```

## Troubleshooting

**"Docker image not found"**
- The script will auto-build it using: `docker compose build server-base-image`

**"No test data found"**
- Run `./run-js-generator.sh` first

**"All tests fail"**
- Check shared type name: JS uses `getText('document')`, C++ uses `ytext(doc, "document")`
- Verify libyrs is installed in Docker image

**Scripts not executable**
```bash
chmod +x tests/*.sh
```

## Next Steps

1. **Run tests**: `./run-all-tests.sh`
2. **Check failures**: Look at test output
3. **Debug specific tests**: Run individual tests with detailed output
4. **Add real deltas**: Capture failing production deltas
5. **Fix server**: Apply learnings to main server code

