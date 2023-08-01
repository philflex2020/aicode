# Compiler and Flags
CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -Iinclude -std=c++11

# Directories
SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build
TEST_TARGET = $(BUILD_DIR)/test
TARGET = $(BUILD_DIR)/main

# Source files
SOURCE_FILES = $(wildcard $(SRC_DIR)/*.cpp)
TEST_FILES = $(wildcard $(TEST_DIR)/*.cpp)

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCE_FILES))
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(TEST_FILES))

# Dependencies
DEPENDENCIES = -lgtest -lgtest_main -pthread

# Targets
all: $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) 
	$(CXX) $(CXXFLAGS) $^ $(DEPENDENCIES) -o $@

$(TEST_TARGET): $(OBJECTS) $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(DEPENDENCIES) -o $@

.PHONY: clean test

clean:
	rm -rf $(BUILD_DIR)

test: $(TARGET)
	$(TARGET)

run: $(TARGET)
	./$(TARGET)