# CRDT Server Makefile

CC = g++
CFLAGS = -O2 -fopenmp -pthread -Wall -Wextra
INCLUDES = -Iinclude -I/usr/include -I/usr/local/include
LIBDIRS = -L/usr/lib -L/usr/local/lib
LIBS = -lwebsockets -lyrs

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TARGET = crdt_server

SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/server.cpp \
          $(SRC_DIR)/peer.cpp \
          $(SRC_DIR)/document.cpp \
          $(SRC_DIR)/persistence.cpp \
          $(SRC_DIR)/protocol.cpp

OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

.PHONY: all clean run

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)
	@echo "Build complete: $(TARGET)"

run: $(TARGET)
	./$(TARGET) 9000

clean:
	rm -rf $(BUILD_DIR) $(TARGET) crdt_document.bin

.PHONY: help
help:
	@echo "CRDT Server Build System"
	@echo "------------------------"
	@echo "make          - Build the server"
	@echo "make run      - Build and run on port 9000"
	@echo "make clean    - Remove build artifacts"

