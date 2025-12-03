# CRDT Delta Testing Framework

A lightweight test framework to validate CRDT delta synchronization between JavaScript (Yjs) clients and C++ (libyrs) server.

## Problem Statement

The current system has a synchronization issue:
- **JS clients** can reconcile documents with each other ✓
- **C++ server** fails to apply deltas correctly ✗
- New clients receive corrupted state from server

This framework isolates the delta application logic to debug the C++/libyrs integration without WebSocket complexity.

---

## Architecture

```
tests/
├── README.md                    # This file
├── js-delta-generator/          # Node.js: Creates test deltas
│   ├── package.json
│   ├── generate-test-deltas.js
│   └── scenarios.js
├── cpp-delta-consumer/          # C++: Applies and validates deltas
│   ├── Makefile
│   ├── test-deltas.cpp
│   └── test-runner.sh
└── test-data/                   # Generated test files
    ├── 001-single-insert/
    │   ├── delta.bin            # Raw Yjs update
    │   ├── expected.txt         # Expected final text
    │   └── meta.json            # Test metadata
    ├── 002-string-insert/
    ├── 003-sequential-edits/
    ├── 004-delete-operation/
    └── 005-concurrent-merge/
```

---

## Quick Start

### Option 1: Run Everything (Recommended)

```bash
cd tests
chmod +x *.sh
./run-all-tests.sh
```

This will:
1. Generate test deltas using Node.js (on host)
2. Build and run C++ tests in Docker container
3. Report results

### Option 2: Run Steps Separately

**Step 1: Generate Test Deltas (JavaScript on Host)**

```bash
cd tests
./run-js-generator.sh
```

This creates test scenarios in `test-data/` with:
- Binary delta files (`.bin`)
- Expected output text (`.txt`)
- Metadata (`.json`)

**Step 2: Run Delta Tests (C++ in Docker)**

```bash
cd tests
./run-cpp-tests.sh
```

This:
- Builds test binary in Docker container
- Reads all test scenarios from `test-data/`
- Creates fresh YDoc for each test
- Applies deltas sequentially
- Compares output with expected result
- Reports pass/fail with details

### Option 3: Manual Testing

**Generate deltas manually:**
```bash
cd tests/js-delta-generator
npm install
npm run generate
```

**Run C++ tests manually in Docker:**
```bash
docker run --rm -v $(pwd)/..:/app -w /app/tests/cpp-delta-consumer crdt-cpp-host-base:latest bash -c "make test"
```

---

## Test Scenarios

### Level 1: Basic Operations
1. **001-single-insert**: Insert single character "a"
2. **002-string-insert**: Insert "hello world"
3. **003-sequential-edits**: Insert "hello", then insert " world"

### Level 2: Modifications
4. **004-delete-operation**: Insert "hello", delete character

### Level 3: Complex
5. **005-concurrent-merge**: Simulate two clients editing simultaneously

### Custom: Real Failures
Add captured production deltas that fail in real system

---

## Workflow

### Generate Phase (Node.js)

```javascript
// For each test scenario:
const ydoc = new Y.Doc();
const ytext = ydoc.getText('document');

// Perform operations
ydoc.transact(() => {
  ytext.insert(0, 'hello');
});

// Capture delta
const update = Y.encodeStateAsUpdate(ydoc);
fs.writeFileSync('delta.bin', update);

// Capture expected result
fs.writeFileSync('expected.txt', ytext.toString());
```

### Consume Phase (C++)

```cpp
// For each test scenario:
YDoc *doc = ydoc_new();
Branch *text = ytext(doc, "document");

// Apply delta
YTransaction *txn = ydoc_write_transaction(doc, 0, NULL);
uint8_t err = ytransaction_apply(txn, delta_data, delta_len);
ytransaction_commit(txn);

// Verify result
const char *result = ytext_string(text, read_txn);
bool pass = strcmp(result, expected) == 0;
```

---

## Test Output

```
========================================
CRDT Delta Testing Framework
========================================

[001-single-insert]
  ✓ Delta loaded (42 bytes)
  ✓ Delta applied successfully
  ✓ Content matches: "a"
  PASS

[002-string-insert]
  ✓ Delta loaded (58 bytes)
  ✓ Delta applied successfully
  ✓ Content matches: "hello world"
  PASS

[003-sequential-edits]
  ✓ Delta 1 loaded (45 bytes)
  ✓ Delta 1 applied
  ✓ Delta 2 loaded (52 bytes)
  ✓ Delta 2 applied
  ✓ Content matches: "hello world"
  PASS

[004-delete-operation]
  ✓ Delta 1 loaded (48 bytes)
  ✓ Delta 1 applied
  ✓ Delta 2 loaded (39 bytes)
  ✗ Delta 2 application failed (error: 3)
  Expected: "helo"
  Got: "hello"
  FAIL

========================================
Results: 3 passed, 1 failed
========================================
```

---

## Debugging Tips

### Verbose Mode

Enable detailed logging in C++ consumer:
```cpp
#define VERBOSE 1
```

Shows:
- Hex dump of delta bytes
- V1/V2 format detection
- Step-by-step application
- State vectors
- Character-by-character comparison

### Capture Real Deltas

Add logging to production server:
```cpp
// In document_apply_update()
FILE *f = fopen("debug_delta.bin", "wb");
fwrite(data, 1, len, f);
fclose(f);
```

Then add to test framework:
```bash
cp server/debug_delta.bin tests/test-data/999-real-failure/delta.bin
```

### Compare Encodings

Check if JS is sending V1 or V2:
```javascript
// In generate-test-deltas.js
const update = Y.encodeStateAsUpdate(ydoc);
console.log('Update format:', update[0]); // 0 = V1, 1 = V2
```

---

## Key Insights to Debug

### 1. Encoding Format Mismatch
- **Question**: Is Yjs sending V1 or V2 updates?
- **Test**: Check `meta.json` encoding field
- **Fix**: Use matching `ytransaction_apply()` or `ytransaction_apply_v2()`

### 2. Shared Type Name
- **Question**: Does "document" match between JS and C++?
- **Test**: Generate delta with different name, should fail
- **Fix**: Ensure both use `getText('document')`

### 3. State Vector Issues
- **Question**: Is server missing initialization steps?
- **Test**: Apply delta to empty vs pre-initialized doc
- **Fix**: Ensure proper YDoc/YText creation

### 4. Sequential Application
- **Question**: Can we apply multiple deltas in order?
- **Test**: Scenario 003 (sequential edits)
- **Fix**: Check transaction commit between applications

### 5. Binary Integrity
- **Question**: Are deltas corrupted in WebSocket transit?
- **Test**: Compare hex dump of sent vs received
- **Fix**: Check WebSocket binary mode, endianness

---

## Benefits

✅ **Isolation**: Tests libyrs without WebSocket/networking  
✅ **Reproducibility**: Deterministic test files  
✅ **Speed**: Runs in seconds, not minutes  
✅ **Debugging**: Verbose logging pinpoints exact failure  
✅ **Regression**: Save real failures as permanent tests  
✅ **CI-Ready**: Automated pass/fail for continuous integration  

---

## Advanced Usage

### Add New Test Scenario

1. Edit `js-delta-generator/scenarios.js`:
```javascript
{
  name: '006-my-test',
  description: 'Tests my specific case',
  operations: [
    { type: 'insert', index: 0, text: 'test' }
  ]
}
```

2. Regenerate: `npm run generate`
3. Test: `cd cpp-delta-consumer && make test`

### Run Single Test

```bash
cd cpp-delta-consumer
./test-deltas ../test-data/001-single-insert
```

### Export for Production Debugging

```bash
# After fixing in test framework
cp test-data/004-delete-operation/delta.bin \
   ../server/scripts/test_delta.bin
```

---

## Requirements

### JavaScript
- Node.js 18+
- Yjs (installed via npm)

### C++
- g++ with C++11 support
- libyrs (Yrs C FFI bindings)
- OpenMP (optional, for parallelization)

### Environment
- Works inside Docker container (recommended)
- Works on host with libyrs installed
- No WebSocket/networking dependencies

---

## Next Steps

1. **Generate deltas**: `cd js-delta-generator && npm run generate`
2. **Run tests**: `cd cpp-delta-consumer && make test`
3. **Fix failures**: Add verbose logging, check error codes
4. **Iterate**: Add more complex scenarios
5. **Deploy fix**: Apply learnings to production server

---

## Troubleshooting

### "libyrs.h not found"
Run inside Docker container or build the image:
```bash
cd ..
docker compose build server-base-image
```

### "npm install fails"
```bash
cd js-delta-generator
rm -rf node_modules package-lock.json
npm install
```

### "All tests fail"
Check shared type name matches:
- JS: `ydoc.getText('document')`
- C++: `ytext(doc, "document")`

### "Delta format error"
Check encoding version in `meta.json`, use correct apply function:
- V1: `ytransaction_apply()`
- V2: `ytransaction_apply_v2()`

---

## License

Part of the CRDT Collaborative Document Editor project.

