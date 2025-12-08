# Docker Setup

## Production

```bash
# Build and run
docker build -t crdt-host .
docker run -p 9000:9000 crdt-host

# Or use docker-compose
docker-compose up --build
```

## Development (Interactive)

For interactive development where you build and run manually:

```bash
# Build dev image (installs all deps including yffi)
make docker-dev-build
# or
docker build -f Dockerfile.dev -t crdt-host-dev .

# Run interactive container (use one of these methods):

# Option 1: Using docker-compose (handles paths better)
docker-compose --profile dev up crdt-host-dev

# Option 2: Using helper script (handles iCloud paths)
./run-dev.sh

# Option 3: Using make (if path quoting works)
make docker-dev-run

# Option 4: Manual docker command with proper quoting
docker run -it -v "$(pwd)":/app -p 9000:9000 crdt-host-dev

# Inside container, inspect where files are located:
inspect-yffi.sh

# Or manually check:
# find /tmp/y-crdt/yffi/target/release -name "*.so"
# find /tmp/y-crdt/yffi -name "*.h"
# ls -la /usr/local/lib/*yffi*
# ls -la /usr/local/include/yffi.h

# Build and run:
# g++ -O2 -fopenmp host.cpp -o host -lwebsockets -lyrs -pthread -I/usr/include -I/usr/local/include -L/usr/lib -L/usr/local/lib
# ./host 9000
```

Or use docker-compose:
```bash
docker-compose --profile dev up crdt-host-dev
```

## What Gets Installed

- Build tools (gcc, g++, make)
- libwebsockets-dev
- OpenMP (libomp-dev)
- Rust/Cargo
- yffi (Yrs FFI bindings) built from source

Port 9000 is exposed for WebSocket connections.
