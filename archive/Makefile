# Makefile for building the CRDT host

CC = g++
CFLAGS = -O2 -fopenmp -pthread
LIBS = -lwebsockets -lyrs
INCLUDES = -I/usr/include -I/usr/local/include
LIBDIRS = -L/usr/lib -L/usr/local/lib

TARGET = host
SOURCE = host.cpp

.PHONY: all clean run run-crdt-server run-sync-example docker-build docker-run docker-up docker-down sync-example crdt-server

all: $(TARGET)

sync-example: sync_example.cpp
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBDIRS) sync_example.cpp -o sync_example $(LIBS)

crdt-server: crdt_server.cpp
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBDIRS) crdt_server.cpp -o crdt_server $(LIBS)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBDIRS) $(SOURCE) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) sync_example main crdt_server

run: $(TARGET)
	./$(TARGET) 9000

run-crdt-server: crdt-server
	./crdt_server 9000

run-sync-example: sync-example
	./sync_example

# Docker targets
docker-build:
	docker build -t crdt-host .

docker-run:
	docker run -p 9000:9000 crdt-host

# Development - interactive container
docker-dev-build:
	docker build -f Dockerfile.dev -t crdt-host-dev .

docker-dev-run:
	docker run -it -v "$$(pwd)":/app -p 9000:9000 crdt-host-dev

docker-up:
	docker-compose up --build

docker-dev:
	docker-compose --profile dev up crdt-host-dev

docker-dev-shell:
	docker-compose --profile dev run --rm crdt-host-dev bash

docker-down:
	docker-compose down
