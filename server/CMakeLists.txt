cmake_minimum_required(VERSION 3.15)
project(crdt_server VERSION 1.0 LANGUAGES CXX)

# Generate compile_commands.json for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# OpenMP support
find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
endif()

# Threading support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    /usr/include
    /usr/local/include
)

# Library directories
link_directories(
    /usr/lib
    /usr/local/lib
)

# Source files
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/peer.cpp
    src/document.cpp
    src/persistence.cpp
    src/protocol.cpp
)

# Header files (for IDEs)
set(HEADERS
    include/server.h
    include/peer.h
    include/document.h
    include/persistence.h
    include/protocol.h
)

# Create executable
add_executable(crdt_server ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(crdt_server
    PRIVATE
        OpenMP::OpenMP_CXX
        Threads::Threads
        websockets
        yrs
)

# Output directory
set_target_properties(crdt_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Custom target for running the server
add_custom_target(run
    COMMAND crdt_server 9000
    DEPENDS crdt_server
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running CRDT server on port 9000"
)

# Print build information
message(STATUS "=================================")
message(STATUS "CRDT Server Build Configuration")
message(STATUS "=================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Source directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "=================================")

