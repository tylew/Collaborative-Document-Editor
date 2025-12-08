#!/bin/bash
# Helper script to run dev container (handles iCloud path issues)

# Get absolute path with proper escaping
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Run docker with properly quoted path
docker run -it -v "${SCRIPT_DIR}":/app -p 9000:9000 crdt-host-dev


