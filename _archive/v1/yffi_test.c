#include <stdio.h>
#include "libyrs.h"

int main(void) {
    YDoc* doc = ydoc_new();
    YText* txt = ytext(doc, "name");
    YTransaction* txn = ydoc_write_transaction(doc);
    
    // append text to our collaborative document with no attributes
    ytext_insert(txt, txn, 0, "hello world", NULL);
    
    // simulate update with remote peer
    YDoc* remote_doc = ydoc_new();
    YText* remote_txt = ytext(remote_doc, "name");
    YTransaction* remote_txn = ydoc_write_transaction(remote_doc);
    
    // in order to exchange data with other documents
    // we first need to create a state vector
    int sv_length = 0;
    unsigned char* remote_sv = ytransaction_state_vector_v1(remote_txn, &sv_length);
    
    // now compute a differential update based on remote document's state vector
    int update_length = 0;
    unsigned char* update = ytransaction_state_diff_v1(txn, remote_sv, sv_length, &update_length);
    
    // release resources no longer in use in the rest of the example
    ybinary_destroy(remote_sv, sv_length);
    ytransaction_commit(txn);
    ydoc_destroy(doc);
    
    // both update and state vector are serializable, we can pass them over the wire
    // now apply update to a remote document
    int err_code = ytransaction_apply(remote_txn, update, update_length);
    if (0 != err_code) {
        // error occurred when trying to apply an update
        exit(err_code);
    }
    ybinary_destroy(update, update_length);
    
    // retrieve string from remote peer YText instance
    char* str = ytext_string(remote_txt, remote_txn);
    printf("%s", str);
    
    // release remaining resources
    ystring_destroy(str);
    ytransaction_commit(remote_txn);
    ydoc_destroy(remote_doc);
    
    return 0;
}