# CRDT Server Makefile

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -I./include
LDFLAGS = -lwebsockets -lyrs -lpthread -lm -ldl -lgomp
BUILD_DIR = build
TARGET_NAME = crdt_server

# Source files
SRCS = $(wildcard src/*.cpp)
OBJS = $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

# Target binary
TARGET = $(BUILD_DIR)/$(TARGET_NAME)

# Default target
all: $(TARGET)

# Link binary
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files with automatic dependency generation
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

run-docker:
	@echo "Building and running server in Docker container..."
	docker run --rm -v "$(PWD)":/workspace -w /workspace crdt-v2-server make
	docker run --rm -v "$(PWD)":/workspace -w /workspace -p 9000:9000 \
		--name crdt-server-container crdt-v2-server \
		bash -c "LD_LIBRARY_PATH=/usr/local/lib stdbuf -o0 ./build/crdt_server 9000"

# build
build: clean $(TARGET)

compile_commands: clean
	# Clean playground as well to ensure full rebuild capture
	$(MAKE) -C playground clean
	# Capture build for both root and playground
	bear --output compile_commands.json -- sh -c "$(MAKE) $(TARGET) && $(MAKE) -C playground objs"

.PHONY: all clean run build compile_commands
