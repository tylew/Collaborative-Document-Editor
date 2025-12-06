# V2 CRDT Server Makefile

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -I./include
LDFLAGS = -lwebsockets -lyrs -lpthread -lm -ldl -lgomp

# Source files
SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

# Target binary
TARGET = crdt_server

# Default target
all: $(TARGET)

# Link binary
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files with automatic dependency generation
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Include dependencies
-include $(DEPS)

# Clean build artifacts
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

# Run server
run: $(TARGET)
	LD_LIBRARY_PATH=/usr/local/lib ./$(TARGET) 9000

# Docker build
docker-build:
	docker build -f Dockerfile -t crdt-v2-server .

# Docker run with build (interactive - shows output, Ctrl+C to stop)
docker-run:
	docker run -it --rm -v "$(PWD)":/app -w /app -p 9000:9000 crdt-v2-server \
		bash -c "make clean && make && LD_LIBRARY_PATH=/usr/local/lib ./crdt_server 9000"

.PHONY: all clean run docker-build docker-run
