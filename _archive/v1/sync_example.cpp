#include <cstdio>
#include <cstring>
#include <cstdlib>

extern "C" {
    #include <libyrs.h>
}

// Global pointers to the two documents for cross-observation
YDoc* g_doc1 = NULL;
YDoc* g_doc2 = NULL;

// Flags to prevent recursive update loops
bool g_applying_to_doc1 = false;
bool g_applying_to_doc2 = false;

// Callback for doc1 updates - apply them to doc2
void doc1_update_callback(void* state, uint32_t len, const char* update) {
    // Prevent recursive loop: if this update came from doc2, don't send it back
    if (g_applying_to_doc1) return;
    
    printf("[Doc1 Update] Received %u bytes, applying to doc2\n", len);
    
    if (g_doc2) {
        g_applying_to_doc2 = true;
        YTransaction* txn = ydoc_write_transaction(g_doc2, 0, NULL);
        uint8_t err = ytransaction_apply(txn, update, len);
        if (err != 0) {
            fprintf(stderr, "Error applying update to doc2: %d\n", err);
        }
        ytransaction_commit(txn);
        g_applying_to_doc2 = false;
    }
}

// Callback for doc2 updates - apply them to doc1
void doc2_update_callback(void* state, uint32_t len, const char* update) {
    // Prevent recursive loop: if this update came from doc1, don't send it back
    if (g_applying_to_doc2) return;
    
    printf("[Doc2 Update] Received %u bytes, applying to doc1\n", len);
    
    if (g_doc1) {
        g_applying_to_doc1 = true;
        YTransaction* txn = ydoc_write_transaction(g_doc1, 0, NULL);
        uint8_t err = ytransaction_apply(txn, update, len);
        if (err != 0) {
            fprintf(stderr, "Error applying update to doc1: %d\n", err);
        }
        ytransaction_commit(txn);
        g_applying_to_doc1 = false;
    }
}

void print_array_contents(YDoc* doc, Branch* array, const char* label) {
    YTransaction* read_txn = ydoc_read_transaction(doc);
    uint32_t array_len = yarray_len(array);
    
    printf("%s Array contents (%u items): [ ", label, array_len);
    for (uint32_t i = 0; i < array_len; i++) {
        YOutput* item = yarray_get(array, read_txn, i);
        if (item && item->tag == Y_JSON_STR) {
            char* str = youtput_read_string(item);
            printf("\"%s\"", str);
            if (i < array_len - 1) printf(", ");
        }
        if (item) youtput_destroy(item);
    }
    printf(" ]\n");
    
    ytransaction_commit(read_txn);
}

int main() {
    printf("=== Example 1: Auto-sync with Update Observers ===\n\n");
    
    // Create two documents
    g_doc1 = ydoc_new();
    g_doc2 = ydoc_new();
    
    // Set up bidirectional observation
    // doc1.on('update', update => Y.applyUpdate(doc2, update))
    YSubscription* sub1 = ydoc_observe_updates_v1(g_doc1, NULL, doc1_update_callback);
    
    // doc2.on('update', update => Y.applyUpdate(doc1, update))
    YSubscription* sub2 = ydoc_observe_updates_v1(g_doc2, NULL, doc2_update_callback);
    
    // Get arrays from both documents
    Branch* array1 = yarray(g_doc1, "myarray");
    Branch* array2 = yarray(g_doc2, "myarray");
    
    // Insert into doc1's array - this should automatically sync to doc2
    printf("Inserting into doc1...\n");
    YTransaction* txn1 = ydoc_write_transaction(g_doc1, 0, NULL);
    YInput items[] = {
        yinput_string("Hello doc2, you got this?")
    };
    yarray_insert_range(array1, txn1, 0, items, 1);
    ytransaction_commit(txn1);
    
    printf("\n");
    print_array_contents(g_doc1, array1, "Doc1");
    print_array_contents(g_doc2, array2, "Doc2");
    
    // Insert into doc2's array - this should automatically sync to doc1
    printf("\nInserting into doc2...\n");
    YTransaction* txn2 = ydoc_write_transaction(g_doc2, 0, NULL);
    YInput items2[] = {
        yinput_string("Yes! I got it from doc1!")
    };
    yarray_insert_range(array2, txn2, 1, items2, 1);
    ytransaction_commit(txn2);
    
    printf("\n");
    print_array_contents(g_doc1, array1, "Doc1");
    print_array_contents(g_doc2, array2, "Doc2");
    
    // Clean up subscriptions
    yunobserve(sub1);
    yunobserve(sub2);
    
    ydoc_destroy(g_doc1);
    ydoc_destroy(g_doc2);
    
    printf("\n\n=== Example 2: State Vector-Based Sync (Minimal Bandwidth) ===\n\n");
    
    // Create two new documents
    YDoc* ydoc1 = ydoc_new();
    YDoc* ydoc2 = ydoc_new();
    
    // Make independent changes to each document
    Branch* arr1 = yarray(ydoc1, "data");
    Branch* arr2 = yarray(ydoc2, "data");
    
    printf("Making independent changes to both documents...\n");
    
    // Doc1 gets items A and B
    YTransaction* t1 = ydoc_write_transaction(ydoc1, 0, NULL);
    YInput doc1_items[] = {
        yinput_string("Item A"),
        yinput_string("Item B")
    };
    yarray_insert_range(arr1, t1, 0, doc1_items, 2);
    ytransaction_commit(t1);
    
    // Doc2 gets items C and D
    YTransaction* t2 = ydoc_write_transaction(ydoc2, 0, NULL);
    YInput doc2_items[] = {
        yinput_string("Item C"),
        yinput_string("Item D")
    };
    yarray_insert_range(arr2, t2, 0, doc2_items, 2);
    ytransaction_commit(t2);
    
    printf("Before sync:\n");
    print_array_contents(ydoc1, arr1, "  Doc1");
    print_array_contents(ydoc2, arr2, "  Doc2");
    
    // Now sync using state vectors (minimal bandwidth approach)
    printf("\nSyncing using state vectors...\n");
    
    // Get state vectors from both documents
    YTransaction* read1 = ydoc_read_transaction(ydoc1);
    YTransaction* read2 = ydoc_read_transaction(ydoc2);
    
    uint32_t sv1_len = 0, sv2_len = 0;
    char* stateVector1 = ytransaction_state_vector_v1(read1, &sv1_len);
    char* stateVector2 = ytransaction_state_vector_v1(read2, &sv2_len);
    
    printf("  State vector 1: %u bytes\n", sv1_len);
    printf("  State vector 2: %u bytes\n", sv2_len);
    
    // Compute diffs based on remote state vectors
    uint32_t diff1_len = 0, diff2_len = 0;
    char* diff1 = ytransaction_state_diff_v1(read1, stateVector2, sv2_len, &diff1_len);
    char* diff2 = ytransaction_state_diff_v1(read2, stateVector1, sv1_len, &diff2_len);
    
    printf("  Diff 1->2: %u bytes\n", diff1_len);
    printf("  Diff 2->1: %u bytes\n", diff2_len);
    
    ytransaction_commit(read1);
    ytransaction_commit(read2);
    
    // Apply diffs
    YTransaction* apply1 = ydoc_write_transaction(ydoc1, 0, NULL);
    ytransaction_apply(apply1, diff2, diff2_len);
    ytransaction_commit(apply1);
    
    YTransaction* apply2 = ydoc_write_transaction(ydoc2, 0, NULL);
    ytransaction_apply(apply2, diff1, diff1_len);
    ytransaction_commit(apply2);
    
    printf("\nAfter sync:\n");
    print_array_contents(ydoc1, arr1, "  Doc1");
    print_array_contents(ydoc2, arr2, "  Doc2");
    
    // Clean up
    ybinary_destroy(stateVector1, sv1_len);
    ybinary_destroy(stateVector2, sv2_len);
    ybinary_destroy(diff1, diff1_len);
    ybinary_destroy(diff2, diff2_len);
    
    ydoc_destroy(ydoc1);
    ydoc_destroy(ydoc2);
    
    printf("\n=== All examples completed successfully! ===\n");
    
    return 0;
}

