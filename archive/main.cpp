#include <cstdio>
#include <cstring>

extern "C" {
    #include <libyrs.h>
}

// Helper function to print map as JSON-like output
void print_map(YDoc* doc, Branch* map) {
    YTransaction* read_txn = ydoc_read_transaction(doc);
    
    // Note: yffi doesn't have a direct toJSON() equivalent
    // We'll print the keys we know about
    printf("{ ");
    
    // Check for keyA
    YOutput* valA = ymap_get(map, read_txn, "keyA");
    if (valA && valA->tag == Y_JSON_STR) {
        char* strA = youtput_read_string(valA);
        printf("\"keyA\": \"%s\"", strA);
        // Don't call ystring_destroy - it's freed by youtput_destroy
    }
    
    // Check for keyB
    YOutput* valB = ymap_get(map, read_txn, "keyB");
    if (valB && valB->tag == Y_JSON_STR) {
        if (valA && valA->tag == Y_JSON_STR) printf(", ");
        char* strB = youtput_read_string(valB);
        printf("\"keyB\": \"%s\"", strB);
        // Don't call ystring_destroy - it's freed by youtput_destroy
    }
    
    printf(" }\n");
    
    // Clean up outputs (this also frees the strings)
    if (valA) youtput_destroy(valA);
    if (valB) youtput_destroy(valB);
    
    ytransaction_commit(read_txn);
}

int main() {
    // Yjs documents are collections of shared objects that sync automatically
    YDoc* ydoc = ydoc_new();
    
    // Define a shared Y.Map instance
    // In yffi, we get/create a map by name
    Branch* ymap = ::ymap(ydoc, "root");
    
    // Set keyA
    YTransaction* txn1 = ydoc_write_transaction(ydoc, 0, NULL);
    YInput keyA_val = yinput_string("valueA");
    ymap_insert(ymap, txn1, "keyA", &keyA_val);
    ytransaction_commit(txn1);
    
    // Create another Yjs document (simulating a remote user)
    // and create some conflicting changes
    YDoc* ydocRemote = ydoc_new();
    Branch* ymapRemote = ::ymap(ydocRemote, "root");
    
    // Set keyB in remote
    YTransaction* txn2 = ydoc_write_transaction(ydocRemote, 0, NULL);
    YInput keyB_val = yinput_string("valueB");
    ymap_insert(ymapRemote, txn2, "keyB", &keyB_val);
    ytransaction_commit(txn2);
    
    // Merge changes from remote
    // Encode remote document state as update
    YTransaction* read_txn_remote = ydoc_read_transaction(ydocRemote);
    uint32_t update_len = 0;
    char* update = ytransaction_state_diff_v1(read_txn_remote, NULL, 0, &update_len);
    ytransaction_commit(read_txn_remote);
    
    // Apply update to local document
    YTransaction* txn3 = ydoc_write_transaction(ydoc, 0, NULL);
    uint8_t err_code = ytransaction_apply(txn3, update, update_len);
    if (err_code != 0) {
        fprintf(stderr, "Error applying update: %d\n", err_code);
    }
    ytransaction_commit(txn3);
    
    // Observe that the changes have merged
    print_map(ydoc, ymap); // => { "keyA": "valueA", "keyB": "valueB" }
    
    // Clean up
    ybinary_destroy(update, update_len);
    ydoc_destroy(ydoc);
    ydoc_destroy(ydocRemote);
    
    return 0;
}
