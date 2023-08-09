# Compiler
CXX := g++
# Compiler flags
CXXFLAGS := -std=c++17 -Wall -Iinclude
# Library search paths
LDFLAGS := -L/usr/local/lib -lcjson

# Source directory
SRC_DIR := src
# Build directory
BUILD_DIR := build

# Get a list of all .cpp files in the src directory
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
# Generate a list of corresponding object files in the build directory
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# The name of the final executable
TARGET := $(BUILD_DIR)/main

# Default target
all: $(TARGET)

# Rule to compile .cpp files into .o files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to link object files into the final executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Declare phony targets
.PHONY: all clean
