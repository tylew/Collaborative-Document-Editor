/**
 * Test Scenarios for CRDT Delta Testing
 * 
 * Each scenario defines a sequence of operations to perform on a Yjs document.
 * The generator will create corresponding deltas and expected outputs.
 */

export const scenarios = [
  {
    id: '001-single-insert',
    name: 'Single Character Insert',
    description: 'Insert a single character "a"',
    operations: [
      { type: 'insert', index: 0, text: 'a' }
    ]
  },
  
  {
    id: '002-string-insert',
    name: 'String Insert',
    description: 'Insert "hello world"',
    operations: [
      { type: 'insert', index: 0, text: 'hello world' }
    ]
  },
  
  {
    id: '003-sequential-edits',
    name: 'Sequential Edits',
    description: 'Insert "hello", then insert " world"',
    operations: [
      { type: 'insert', index: 0, text: 'hello' },
      { type: 'insert', index: 5, text: ' world' }
    ]
  },
  
  {
    id: '004-delete-operation',
    name: 'Delete Operation',
    description: 'Insert "hello", then delete one "l"',
    operations: [
      { type: 'insert', index: 0, text: 'hello' },
      { type: 'delete', index: 3, length: 1 }  // Delete second 'l'
    ]
  },
  
  {
    id: '005-multiple-deletes',
    name: 'Multiple Deletes',
    description: 'Insert "hello world", delete "o w"',
    operations: [
      { type: 'insert', index: 0, text: 'hello world' },
      { type: 'delete', index: 4, length: 3 }  // Delete "o w"
    ]
  },
  
  {
    id: '006-insert-middle',
    name: 'Insert in Middle',
    description: 'Insert "helo", then insert "l" in the middle',
    operations: [
      { type: 'insert', index: 0, text: 'helo' },
      { type: 'insert', index: 3, text: 'l' }
    ]
  },
  
  {
    id: '007-replace-text',
    name: 'Replace Text',
    description: 'Insert "hello", delete all, insert "world"',
    operations: [
      { type: 'insert', index: 0, text: 'hello' },
      { type: 'delete', index: 0, length: 5 },
      { type: 'insert', index: 0, text: 'world' }
    ]
  },
  
  {
    id: '008-empty-to-text',
    name: 'Empty to Text',
    description: 'Start with empty doc, insert text',
    operations: [
      { type: 'insert', index: 0, text: 'The quick brown fox jumps over the lazy dog' }
    ]
  },
  
  {
    id: '009-concurrent-merge',
    name: 'Concurrent Merge Simulation',
    description: 'Two separate documents merge their changes',
    // This will be handled specially - creates two docs and merges
    concurrent: true,
    operations: [
      { doc: 1, type: 'insert', index: 0, text: 'Hello' },
      { doc: 2, type: 'insert', index: 0, text: 'World' }
    ]
  },
  
  {
    id: '010-rapid-edits',
    name: 'Rapid Sequential Edits',
    description: 'Multiple rapid insertions simulating typing',
    operations: [
      { type: 'insert', index: 0, text: 'H' },
      { type: 'insert', index: 1, text: 'e' },
      { type: 'insert', index: 2, text: 'l' },
      { type: 'insert', index: 3, text: 'l' },
      { type: 'insert', index: 4, text: 'o' }
    ]
  }
];

