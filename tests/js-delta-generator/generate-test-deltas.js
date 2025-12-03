/**
 * CRDT Delta Generator
 * 
 * Generates test deltas using Yjs for validation against C++ libyrs implementation.
 * Creates binary delta files, expected output, and metadata for each test scenario.
 */

import * as Y from 'yjs';
import { scenarios } from './scenarios.js';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const TEST_DATA_DIR = path.join(__dirname, '..', 'test-data');

// Ensure test-data directory exists
if (!fs.existsSync(TEST_DATA_DIR)) {
  fs.mkdirSync(TEST_DATA_DIR, { recursive: true });
}

/**
 * Generate test case for a single scenario
 */
function generateTestCase(scenario) {
  console.log(`\n[${scenario.id}] ${scenario.name}`);
  console.log(`  Description: ${scenario.description}`);
  
  const testDir = path.join(TEST_DATA_DIR, scenario.id);
  
  // Create scenario directory
  if (!fs.existsSync(testDir)) {
    fs.mkdirSync(testDir, { recursive: true });
  }
  
  if (scenario.concurrent) {
    generateConcurrentTestCase(scenario, testDir);
  } else {
    generateSequentialTestCase(scenario, testDir);
  }
}

/**
 * Generate sequential test case (standard flow)
 */
function generateSequentialTestCase(scenario, testDir) {
  // Create YDoc with shared type name matching server
  const ydoc = new Y.Doc();
  const ytext = ydoc.getText('document');  // Must match server's "document"
  
  const deltas = [];
  let operationCount = 0;
  
  // Apply each operation and capture delta
  for (const op of scenario.operations) {
    operationCount++;
    
    // Capture update for this operation
    let update = null;
    
    ydoc.transact(() => {
      if (op.type === 'insert') {
        ytext.insert(op.index, op.text);
        console.log(`  Op ${operationCount}: Insert "${op.text}" at index ${op.index}`);
      } else if (op.type === 'delete') {
        ytext.delete(op.index, op.length);
        console.log(`  Op ${operationCount}: Delete ${op.length} char(s) at index ${op.index}`);
      }
    });
    
    // Capture the delta after this transaction
    // Use state vector to get incremental update
    if (operationCount === 1) {
      // First operation: encode full state
      update = Y.encodeStateAsUpdate(ydoc);
    } else {
      // Subsequent operations: encode diff from previous state
      // For simplicity in testing, we'll encode full state each time
      // This matches how new clients receive initial state
      update = Y.encodeStateAsUpdate(ydoc);
    }
    
    deltas.push({
      operation: operationCount,
      data: update,
      size: update.length
    });
  }
  
  // Get final expected text
  const expectedText = ytext.toString();
  console.log(`  Final text: "${expectedText}"`);
  console.log(`  Generated ${deltas.length} delta(s)`);
  
  // Write deltas
  if (deltas.length === 1) {
    // Single delta - write as delta.bin
    fs.writeFileSync(
      path.join(testDir, 'delta.bin'),
      Buffer.from(deltas[0].data)
    );
    console.log(`  ✓ Wrote delta.bin (${deltas[0].size} bytes)`);
  } else {
    // Multiple deltas - write as delta_001.bin, delta_002.bin, etc.
    deltas.forEach((delta, idx) => {
      const filename = `delta_${String(idx + 1).padStart(3, '0')}.bin`;
      fs.writeFileSync(
        path.join(testDir, filename),
        Buffer.from(delta.data)
      );
      console.log(`  ✓ Wrote ${filename} (${delta.size} bytes)`);
    });
  }
  
  // Write expected output
  fs.writeFileSync(
    path.join(testDir, 'expected.txt'),
    expectedText,
    'utf8'
  );
  console.log(`  ✓ Wrote expected.txt`);
  
  // Write metadata
  const metadata = {
    id: scenario.id,
    name: scenario.name,
    description: scenario.description,
    encoding: 'yjs-v1',
    sharedTypeName: 'document',
    operations: scenario.operations,
    deltaCount: deltas.length,
    expectedLength: expectedText.length,
    expectedText: expectedText,
    generatedAt: new Date().toISOString()
  };
  
  fs.writeFileSync(
    path.join(testDir, 'meta.json'),
    JSON.stringify(metadata, null, 2),
    'utf8'
  );
  console.log(`  ✓ Wrote meta.json`);
}

/**
 * Generate concurrent test case (merge simulation)
 */
function generateConcurrentTestCase(scenario, testDir) {
  console.log(`  Type: Concurrent merge`);
  
  // Create two separate documents
  const ydoc1 = new Y.Doc();
  const ytext1 = ydoc1.getText('document');
  
  const ydoc2 = new Y.Doc();
  const ytext2 = ydoc2.getText('document');
  
  // Apply operations to respective docs
  const doc1Ops = scenario.operations.filter(op => op.doc === 1);
  const doc2Ops = scenario.operations.filter(op => op.doc === 2);
  
  console.log(`  Doc1 operations: ${doc1Ops.length}`);
  doc1Ops.forEach(op => {
    ydoc1.transact(() => {
      if (op.type === 'insert') {
        ytext1.insert(op.index, op.text);
      }
    });
  });
  
  console.log(`  Doc2 operations: ${doc2Ops.length}`);
  doc2Ops.forEach(op => {
    ydoc2.transact(() => {
      if (op.type === 'insert') {
        ytext2.insert(op.index, op.text);
      }
    });
  });
  
  // Capture states before merge
  const update1 = Y.encodeStateAsUpdate(ydoc1);
  const update2 = Y.encodeStateAsUpdate(ydoc2);
  
  // Merge: apply doc2's update to doc1
  Y.applyUpdate(ydoc1, update2);
  
  const expectedText = ytext1.toString();
  console.log(`  Merged text: "${expectedText}"`);
  
  // Write both deltas
  fs.writeFileSync(
    path.join(testDir, 'delta_001.bin'),
    Buffer.from(update1)
  );
  console.log(`  ✓ Wrote delta_001.bin (${update1.length} bytes)`);
  
  fs.writeFileSync(
    path.join(testDir, 'delta_002.bin'),
    Buffer.from(update2)
  );
  console.log(`  ✓ Wrote delta_002.bin (${update2.length} bytes)`);
  
  // Write expected output (merged)
  fs.writeFileSync(
    path.join(testDir, 'expected.txt'),
    expectedText,
    'utf8'
  );
  console.log(`  ✓ Wrote expected.txt`);
  
  // Write metadata
  const metadata = {
    id: scenario.id,
    name: scenario.name,
    description: scenario.description,
    encoding: 'yjs-v1',
    sharedTypeName: 'document',
    type: 'concurrent',
    operations: scenario.operations,
    deltaCount: 2,
    expectedLength: expectedText.length,
    expectedText: expectedText,
    generatedAt: new Date().toISOString()
  };
  
  fs.writeFileSync(
    path.join(testDir, 'meta.json'),
    JSON.stringify(metadata, null, 2),
    'utf8'
  );
  console.log(`  ✓ Wrote meta.json`);
}

/**
 * Main generation function
 */
function generateAllTests() {
  console.log('========================================');
  console.log('CRDT Delta Generator');
  console.log('========================================');
  console.log(`Output directory: ${TEST_DATA_DIR}`);
  console.log(`Scenarios: ${scenarios.length}`);
  
  // Generate each scenario
  scenarios.forEach(scenario => {
    try {
      generateTestCase(scenario);
    } catch (error) {
      console.error(`  ✗ Error generating ${scenario.id}:`, error.message);
    }
  });
  
  console.log('\n========================================');
  console.log('Generation complete!');
  console.log('========================================');
  console.log(`\nTest data written to: ${TEST_DATA_DIR}`);
  console.log(`\nNext steps:`);
  console.log(`  cd ../cpp-delta-consumer`);
  console.log(`  make test`);
}

// Run generation
generateAllTests();

