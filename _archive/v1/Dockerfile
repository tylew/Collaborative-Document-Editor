# Dockerfile for CRDT Document Editor Host
# Build: docker build -t crdt-host .
# Run:   docker run -p 9000:9000 crdt-host

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    libwebsockets-dev \
    libomp-dev \
    curl \
    git \
    && rm -rf /var/lib/apt/lists/*

# Install Rust/Cargo for building yffi
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Build yffi (Yrs FFI bindings) from source
RUN git clone --depth 1 https://github.com/y-crdt/y-crdt.git /tmp/y-crdt && \
    cd /tmp/y-crdt && \
    cargo build --release --package yffi && \
    cp target/release/libyrs.so /usr/local/lib/ && \
    cp tests-ffi/include/libyrs.h /usr/local/include/ && \
    ldconfig && \
    rm -rf /tmp/y-crdt ~/.cargo/registry ~/.cargo/git

WORKDIR /app

# Copy and build host (for production)
COPY host.cpp ./
RUN g++ -O2 -fopenmp host.cpp -o host \
    -lwebsockets \
    -lyrs \
    -pthread \
    -I/usr/include \
    -I/usr/local/include \
    -L/usr/lib \
    -L/usr/local/lib

EXPOSE 9000

CMD ["./host", "9000"]
