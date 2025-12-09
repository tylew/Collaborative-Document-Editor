# Collaborative Document Editor - Project Report

I built this project as a educational dive into solving the problem of **Real-Time Collaborative Editing**, allowing multiple users to edit a shared document simultaneously without conflicts or data loss. Traditional locking mechanisms (like checking out a file) are inefficient for real-time collaboration. We need a system that allows concurrent edits and resolves conflicts automatically and consistently across all connected clients.

![demo](/assets/recording.gif)

This problem addresses High Performance Computing (HPC) concepts due to the **concurrency and scalability requirements**:
1.  **Massive Concurrency:** A popular document might have hundreds or thousands of active users. The server must handle these concurrent connections and broadcast updates with minimal latency.
2.  **State Management:** Maintaining a consistent state across distributed systems requires efficient synchronization primitives (locks, atomic operations) to prevent race conditions, a core concept in parallel programming.
3.  **Throughput:** Processing and broadcasting Delta updates (small changes) to $N$ peers requires efficient data handling that can benefit from parallel processing to reduce the $O(N)$ broadcast latency.

## Background

### Algorithms Used
1.  **CRDT (Conflict-free Replicated Data Types):** CRDT refers to a data model which ensures every client converges to the same document without conflict resolution by a central authority.
2.  **YATA (Yet Another Transformation Approach):** This is the specific merge algorithm in the Yjs/Yrs package that maps concurrent operations into a single deterministic order. All edits remain commutative, associative, and idempotent, guaranteeing eventual consistency across replicas.
3.  **Differential Synchronization:** Instead of sending the full document, we use a 2-step sync protocol:
    *   **Step 1:** Exchange state vectors (compressed summary of known updates).
    *   **Step 2:** Calculate and transmit only the missing "deltas" (diffs).
3.  **VarInt Encoding:** A variable-length integer encoding algorithm is used to compress message headers and payload lengths, reducing bandwidth usage.

### Relevant HPC Concepts
*   **Concurrency Control:** We utilize **Mutual Exclusion (Mutex)** via OpenMP locks to protect shared resources (the global peer list) from race conditions.
*   **Critical Sections:** Code segments that access shared memory (e.g., adding/removing peers, iterating for broadcast) are identified as critical sections and protected.
*   **Latency vs. Throughput:** The system is designed to minimize the time between a user's keystroke and the update appearing on other screens (Latency), while maximizing the number of concurrent updates processed (Throughput).

## High-Level Architecture & Data Flow

### 1. Server Architecture
The server performs two critical roles simultaneously:
*   **Relay (Real-Time):** It acts as a high-speed relay for differential updates. Small "Deltas" (e.g., a single keystroke) received from one client are immediately broadcast to all other connected peers, ensuring low-latency collaboration.
*   **Snapshot Authority (Persistence):** The server maintains a **"living" snapshot** of the reconciled document state in memory. This allows new clients to sync immediately from the server's state rather than waiting for peer-to-peer transfers, and in a production setting would enable data persistence.

### 2. Communication Protocol
We use a custom binary protocol over WebSockets for efficiency:

#### A. Initial Handshake (Sync)
When a new client joins, they must catch up to the current state:
1.  **Client:** Sends `SYNC_STEP1` with a **State Vector** (a tiny map of "what data I already have").
2.  **Server:** Compares this vector against its global state.
3.  **Server:** Computes the minimal difference and sends one consolidated `SYNC_STEP2` message containing the **Entire Missing State**.
    *   *Note: New clients receive one optimized blob, not a history of every individual edit. For my example implementation clients always execute a SYNC_STEP1 with an empty state vector, however in a production system clients may cache a local copy of the last known state. I did not implement this functionality because at times during development I am running multiple clients simultanously in my browser and don't want them to interfere with eachother.*

#### B. Real-Time Collaboration
Once synced, the protocol switches to differential updates:
1.  **Client:** User types a character -> Client computes a **Delta**.
2.  **Client:** Sends `SYNC_STEP2` (Delta) to Server.
3.  **Server:** Applies Delta to its snapshot AND Broadcasts it to all other peers.

#### C. Awareness
*   **Message:** `AWARENESS`
*   **Content:** Ephemeral data like Cursor Position, Selection, Username, and Color.
*   **Behavior:** Broadcast to all peers but *not* saved to the document history.

## Methods

### How we used concurrency
Concurrency is managed at two levels:

1.  **Network Level:** An asynchronous I/O loop (`libwebsockets`) handles concurrent WebSocket connections without blocking, allowing potentially thousands of clients to stay connected simultaneously.
2.  **Application Level:** **OpenMP** locks (`omp_lock_t`) are used to protect shared in-memory data structures (specifically the global peer list). This ensures that as clients connect and disconnect, the critical sections of code that modify or traverse this list are thread-safe and free from race conditions.

**Result:**
1.  **Consistency:** Ensures updates are **only** broadcast to peers who have completed the initial handshake (`p->synced == true`). The lock prevents a race where a peer might be marked "synced" halfway through a broadcast, potentially receiving a delta before their initial snapshot, which would corrupt their state.
2.  **Crash Prevention:** Effectively prevents Segmentation Faults caused by race conditions when a user disconnects while the server is broadcasting.

### How we applied OpenMP
We utilized the **OpenMP Runtime Library** (`<omp.h>`) specifically for its portable locking mechanisms to manage the shared state of connected peers. This ensures that as clients connect and disconnect (modifying the linked list tracking active connections), the broadcasting thread does not access invalid memory.

### Code Snippet: Thread-Safe Broadcast
The following snippet demonstrates how we protect the broadcast loop using OpenMP locks:

```cpp
void server_broadcast(
    const uint8_t* data, size_t len, struct lws* exclude
) {
    if (len == 0) return;

    omp_set_lock(&g_peers_lock); 

    Peer* p = g_peers;
    while (p) {
        if (p->wsi != exclude && p->synced) {
            peer_queue_message(p, data, len);
        }
        p = p->next;
    }

    omp_unset_lock(&g_peers_lock); 
}
```

### Significance of this Pattern
The code snippets above demonstrate a classic **Reader-Writer Lock** pattern implemented with OpenMP:
1.  **Atomicity:** The `omp_set_lock` ensures that the multi-step operation of adding a peer (allocating memory, initializing fields, linking next pointer) happens atomically. No other thread can traverse the list while it is in an inconsistent state.
2.  **State Consistency:** By initializing `p->synced = false` *before* adding it to the global list (inside the lock), we guarantee that the broadcast loop (which checks `p->synced`) will safely skip this new peer until the handshake is explicitly completed.
3.  **Memory Safety:** The lock prevents "Use-After-Free" errors. Without it, `server_broadcast` could be reading `p->next` at the exact moment another thread calls `free(p)` in `peers_remove`, causing a segmentation fault.

### Challenges
*   **Race Conditions:** A major challenge was ensuring that a peer wasn't removed from memory (on disconnect) while the server was iterating through the list to send an update. OpenMP locks solved this.
*   **Load Balancing:** Ensuring that one slow client doesn't block the entire broadcast loop. We solved this by queuing messages per peer rather than blocking on write.

## Experiments & Results

### Hardware Info
*   **Environment:** Docker Container (Linux)
*   **Host CPU:** 4 vCPUs (Simulated)
*   **Memory:** 4GB RAM

### Results Discussion
*   **Speedup:** While the current implementation uses OpenMP for safety rather than data parallelism (splitting the loop), the architecture allows for low-overhead synchronization. The "speedup" here is qualitative: the system maintains stability under concurrent load where a non-thread-safe version would crash.
*   **Latency:** The differential synchronization (CRDT) proved highly efficient. Initial sync times remained under 100ms for small documents, and incremental updates were processed in <10ms.
*   **Benchmark Metrics:**
    | Clients | Avg Latency (ms) | Min (ms) | Max (ms) |
    | :--- | :--- | :--- | :--- |
    | 10 | 6.00 | 5 | 7 |
    | 50 | 4.45 | 1 | 6 |
    | 100 | 9.43 | 4 | 13 |
    | 200 | 14.48 | 6 | 17 |
    
    *(Note: Latency scales linearly $O(N)$ as expected due to the serial broadcast loop, doubling from 100 to 200 clients.)*

### Bottlenecks Identified
*   **Serial Broadcast Loop:** The `while(p)` loop inside `server_broadcast` processes peers sequentially. For many users, this becomes a bottleneck. This is a prime candidate for `parallel for` optimization.

## Discussion

### What worked well
*   **CRDT Integration:** The Yrs library handled the complex math of conflict resolution perfectly, allowing us to focus on the transport layer.
*   **OpenMP Synchronization:** Using `omp_lock_t` was simpler and more portable than managing raw `pthread_mutex_t` or C++11 `std::mutex` for this specific C-style linked list structure.

### Limitations
*   **Single-Threaded Event Loop:** `libwebsockets` default event loop is single-threaded. While our data structures are thread-safe, the actual network packet processing is serialized on a single core.
*   **Memory Management:** Manual memory management (malloc/free) for the peer list and message queues introduced complexity and potential leaks if not handled carefully in the destructor.

## Conclusion

I learned that **high performance compute is not just about parallelism; it's about correctness.** Even in a mostly serial event loop, handling callbacks that modify shared state requires rigorous critical section management. I also gained practical experience integrating C++ backends with modern React frontends via binary WebSocket protocols.

### What should be done next?
1.  **Parallelize Broadcast:** Use `#pragma omp parallel` to parallelize the message queuing process for connected peers.
2.  **Multithreaded Network Loop:** Configure `libwebsockets` to run on multiple threads, fully utilizing the server's CPU cores.
3. **Multiple Document Sessions**. Instead of one global `g_peers` list, we would shard peers by Document ID, allowing different documents to be processed in parallel on different threads without lock contention.

## References

1.  **Yjs / Yrs (CRDT Library):** https://github.com/y-crdt/y-crdt
2.  **libwebsockets:** https://libwebsockets.org/
3.  **OpenMP Specification:** https://www.openmp.org/
4.  **React & Quill:** https://reactjs.org/, https://quilljs.com/
