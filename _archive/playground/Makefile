# CRDT Playground Makefile

CXX = g++
CC = gcc
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -I./include
CFLAGS = -std=c11 -Wall -Wextra -O2 -I./include
LDFLAGS = -lwebsockets -lyrs -lpthread -lm -ldl -lgomp
BUILD_DIR = build
TARGET_NAME = playground_crdt

# Source files
CSRCS = $(wildcard src/*.c)
CXXSRCS = $(wildcard src/*.cpp)
OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CSRCS)) \
       $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(CXXSRCS))
DEPS = $(OBJS:.o=.d)

# Target binary
TARGET = $(BUILD_DIR)/$(TARGET_NAME)

# Default target
all: $(TARGET)

# Object-only build (no link)
objs: $(OBJS)

# Link binary
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files with automatic dependency generation
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)/
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)/
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/:
	mkdir -p $(BUILD_DIR)

# Include dependencies
-include $(DEPS)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) compile_commands.json

# # Run server
run: $(TARGET)
	LD_LIBRARY_PATH=/usr/local/lib ./$(TARGET) 9000

# build
build: clean $(TARGET)

compile_commands: clean
	bear --output compile_commands.json -- $(MAKE) objs

.PHONY: all clean run build compile_commands objs
