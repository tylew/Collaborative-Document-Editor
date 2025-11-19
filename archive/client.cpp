// client.cpp — CRDT document editor client
// Build: c++ -O2 -pthread client.cpp -o client -lwebsockets -lyrs
// Run:   ./client [host] [port]
//        Default: localhost 9000
//
// Note: The Yrs API functions used (ytext_insert, ytext_remove_range, etc.)
//       may need to be adjusted based on your actual libyrs.h API.

#define _POSIX_C_SOURCE 200809L
#include <libwebsockets.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

extern "C" {
#include "libyrs.h"
}

static volatile int running = 1;
static volatile int connected = 0;
static volatile int received_initial_state = 0;
static volatile int doc_changed = 0;  // flag to indicate document was updated
static struct lws *client_wsi = NULL;

// ----- CRDT local doc -----
static YDoc *g_doc = NULL;
static YText *g_text = NULL;  // main text object for editing

// ----- Client state -----
typedef struct {
    unsigned char *pending_send;
    size_t pending_send_len;
    bool send_pending;
} client_state;

static client_state g_state = {NULL, 0, false};

// ----- WebSocket protocol callback -----
static int callback_crdt_client(struct lws *wsi, enum lws_callback_reasons reason,
                                void *user, void *in, size_t len) {
    client_state *state = (client_state *)user;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED: {
            printf("Connected to server\n");
            connected = 1;
            client_wsi = wsi;
            break;
        }

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            // Receive CRDT update from server
            if (!g_doc) {
                fprintf(stderr, "Error: Document not initialized\n");
                break;
            }

            // Apply update to local document
            YTransaction *txn = ydoc_write_transaction(g_doc, 0, NULL);
            ytransaction_apply(txn, (const char*)in, (uint32_t)len);
            ytransaction_commit(txn);

            if (!received_initial_state) {
                printf("Received initial state (%zu bytes), document synchronized\n", len);
                received_initial_state = 1;
                doc_changed = 1;  // trigger display refresh
            } else {
                printf("Received update (%zu bytes), applied to local document\n", len);
                doc_changed = 1;  // trigger display refresh
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            // Send pending update if any
            if (state && state->send_pending && state->pending_send && state->pending_send_len > 0) {
                size_t out_len = LWS_PRE + state->pending_send_len;
                unsigned char *out = (unsigned char*)malloc(out_len);
                if (!out) {
                    fprintf(stderr, "Error: Failed to allocate send buffer\n");
                    break;
                }
                memcpy(out + LWS_PRE, state->pending_send, state->pending_send_len);
                
                int ret = lws_write(wsi, out + LWS_PRE, state->pending_send_len, LWS_WRITE_BINARY);
                if (ret < 0) {
                    fprintf(stderr, "Error: Failed to write to server\n");
                } else {
                    printf("Sent update (%zu bytes) to server\n", state->pending_send_len);
                }
                
                free(out);
                free(state->pending_send);
                state->pending_send = NULL;
                state->pending_send_len = 0;
                state->send_pending = false;
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            fprintf(stderr, "Connection error\n");
            running = 0;
            break;

        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("Disconnected from server\n");
            connected = 0;
            client_wsi = NULL;
            running = 0;
            break;

        default:
            break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    { "crdt-proto", callback_crdt_client, sizeof(client_state), 64 * 1024, 0, NULL, 0 },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

// ----- Helper function to send an update -----
static void send_update(const unsigned char *data, size_t len) {
    if (!connected || !client_wsi) {
        fprintf(stderr, "Error: Not connected to server\n");
        return;
    }

    if (g_state.pending_send) {
        fprintf(stderr, "Warning: Overwriting pending send\n");
        free(g_state.pending_send);
    }

    g_state.pending_send = (unsigned char*)malloc(len);
    if (!g_state.pending_send) {
        fprintf(stderr, "Error: Failed to allocate send buffer\n");
        return;
    }

    memcpy(g_state.pending_send, data, len);
    g_state.pending_send_len = len;
    g_state.send_pending = true;
    lws_callback_on_writable(client_wsi);
}

// ----- Document editing functions -----

// Get or create the main text object
static YText* get_or_create_text() {
    if (!g_doc) return NULL;
    
    if (!g_text) {
        // Get or create a text object named "content"
        // Note: Exact API depends on libyrs.h - this is a common pattern
        // If your API differs, adjust accordingly
        YMap *root = ydoc_get_map(g_doc, "root");
        if (!root) {
            // Create root map if it doesn't exist
            YTransaction *txn = ydoc_write_transaction(g_doc, 0, NULL);
            root = ymap_new(txn);
            ytransaction_commit(txn);
        }
        
        YTransaction *txn = ydoc_write_transaction(g_doc, 0, NULL);
        YValue val = ymap_get(txn, root, "content");
        if (val.type == Y_VALUE_NULL) {
            // Create new text object
            g_text = ytext_new(txn);
            ymap_insert(txn, root, "content", yvalue_text(g_text));
        } else if (val.type == Y_VALUE_TEXT) {
            g_text = val.text;
        }
        ytransaction_commit(txn);
    }
    
    return g_text;
}

// Insert text at position
static void insert_text(size_t index, const char *text) {
    if (!g_doc || !text) return;
    
    YText *txt = get_or_create_text();
    if (!txt) {
        fprintf(stderr, "Error: Failed to get text object\n");
        return;
    }
    
    YTransaction *txn = ydoc_write_transaction(g_doc, 0, NULL);
    ytext_insert(txt, txn, (uint32_t)index, text, NULL);
    
    // Encode and send the diff
    unsigned char *update = NULL;
    int update_len = 0;
    ytransaction_encode_diff_v1(txn, &update, &update_len);
    
    if (update && update_len > 0) {
        send_update(update, (size_t)update_len);
        ybinary_destroy((char*)update, update_len);
    }
    
    ytransaction_commit(txn);
    doc_changed = 1;
}

// Delete text at position
static void delete_text(size_t index, size_t len) {
    if (!g_doc || len == 0) return;
    
    YText *txt = get_or_create_text();
    if (!txt) {
        fprintf(stderr, "Error: Failed to get text object\n");
        return;
    }
    
    YTransaction *txn = ydoc_write_transaction(g_doc, 0, NULL);
    ytext_remove_range(txt, txn, (uint32_t)index, (uint32_t)len);
    
    // Encode and send the diff
    unsigned char *update = NULL;
    int update_len = 0;
    ytransaction_encode_diff_v1(txn, &update, &update_len);
    
    if (update && update_len > 0) {
        send_update(update, (size_t)update_len);
        ybinary_destroy((char*)update, update_len);
    }
    
    ytransaction_commit(txn);
    doc_changed = 1;
}

// Get document content as string
static char* get_document_string() {
    if (!g_doc) return NULL;
    
    YText *txt = get_or_create_text();
    if (!txt) return strdup("");
    
    YTransaction *txn = ydoc_read_transaction(g_doc);
    const char *str = ytext_string(txt, txn);
    char *result = str ? strdup(str) : strdup("");
    ytransaction_commit(txn);
    
    return result;
}

// Display the document
static void display_document() {
    char *content = get_document_string();
    if (content) {
        printf("\r\033[K");  // Clear line
        printf("Document: [%s]\n", content);
        printf("> ");
        fflush(stdout);
        free(content);
    }
}

// ----- Input handling thread -----
static void* input_thread(void *arg) {
    char line[4096];
    
    printf("\n=== CRDT Document Editor ===\n");
    printf("Commands:\n");
    printf("  insert <index> <text>  - Insert text at position\n");
    printf("  delete <index> <len>  - Delete text at position\n");
    printf("  show                   - Display document\n");
    printf("  quit                   - Exit\n\n");
    
    while (running) {
        printf("> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) break;
            continue;
        }
        
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        
        if (len == 0) continue;
        
        // Parse command
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            running = 0;
            break;
        } else if (strcmp(line, "show") == 0) {
            display_document();
        } else if (strncmp(line, "insert ", 7) == 0) {
            size_t index;
            char text[4000];
            if (sscanf(line + 7, "%zu %3999[^\n]", &index, text) == 2) {
                insert_text(index, text);
                printf("Inserted '%s' at position %zu\n", text, index);
            } else {
                printf("Usage: insert <index> <text>\n");
            }
        } else if (strncmp(line, "delete ", 7) == 0) {
            size_t index, del_len;
            if (sscanf(line + 7, "%zu %zu", &index, &del_len) == 2) {
                delete_text(index, del_len);
                printf("Deleted %zu characters from position %zu\n", del_len, index);
            } else {
                printf("Usage: delete <index> <length>\n");
            }
        } else {
            printf("Unknown command. Type 'quit' to exit.\n");
        }
        
        // Display document if it changed
        if (doc_changed) {
            display_document();
            doc_changed = 0;
        }
    }
    
    return NULL;
}

static void on_signal(int sig) {
    (void)sig;
    printf("\nShutting down...\n");
    running = 0;
}

int main(int argc, char **argv) {
    const char *host = "localhost";
    int port = 9000;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    // Initialize local CRDT document
    g_doc = ydoc_new();
    if (!g_doc) {
        fprintf(stderr, "Error: Failed to create document\n");
        return 1;
    }

    printf("Connecting to %s:%d...\n", host, port);

    // Create libwebsockets client context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Error: Failed to create WebSocket context\n");
        ydoc_destroy(g_doc);
        return 1;
    }

    // Build connection info
    char uri[256];
    snprintf(uri, sizeof(uri), "/?protocol=crdt-proto");

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = host;
    ccinfo.port = port;
    ccinfo.path = uri;
    ccinfo.host = lws_canonical_hostname(context);
    ccinfo.origin = "crdt-client";
    ccinfo.protocol = "crdt-proto";
    ccinfo.ietf_version_or_minus_one = -1;
    ccinfo.userdata = &g_state;

    struct lws *wsi = lws_client_connect_via_info(&ccinfo);
    if (!wsi) {
        fprintf(stderr, "Error: Failed to initiate connection\n");
        lws_context_destroy(context);
        ydoc_destroy(g_doc);
        return 1;
    }

    printf("Connection initiated, waiting for server...\n");

    // Event loop
    int retry_count = 0;
    const int max_retries = 100; // Wait up to 5 seconds for connection
    
    while (running && retry_count < max_retries && !connected) {
        lws_service(context, 50);
        retry_count++;
    }

    if (!connected) {
        fprintf(stderr, "Error: Failed to connect to server\n");
        lws_context_destroy(context);
        ydoc_destroy(g_doc);
        return 1;
    }

    printf("Connected! Entering event loop. Press Ctrl+C to exit.\n");
    printf("Waiting for initial state from server...\n");

    // Wait for initial state
    int wait_count = 0;
    while (running && !received_initial_state && wait_count < 200) {
        lws_service(context, 50);
        wait_count++;
    }

    if (!received_initial_state) {
        fprintf(stderr, "Warning: Did not receive initial state from server\n");
    }

    printf("\nClient ready. Starting editor...\n");
    
    // Initialize text object
    get_or_create_text();
    
    // Start input thread
    pthread_t input_tid;
    if (pthread_create(&input_tid, NULL, input_thread, NULL) != 0) {
        fprintf(stderr, "Error: Failed to create input thread\n");
        lws_context_destroy(context);
        ydoc_destroy(g_doc);
        return 1;
    }
    
    // Main event loop - process WebSocket events and check for document changes
    while (running) {
        lws_service(context, 50);
        
        // Display document if it changed from remote updates
        if (doc_changed) {
            display_document();
            doc_changed = 0;
        }
    }
    
    // Wait for input thread to finish
    pthread_join(input_tid, NULL);

    // Cleanup
    if (g_state.pending_send) {
        free(g_state.pending_send);
    }
    lws_context_destroy(context);
    ydoc_destroy(g_doc);

    return 0;
}

