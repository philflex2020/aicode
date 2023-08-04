# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Directories
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = test
BUILD_DIR = build

# Source files
#SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
MAIN_SOURCE = $(SRC_DIR)/loggen.cpp
#TEST_SOURCE = $(TEST_DIR)/logging_main_test.cpp

# Header files
INCLUDE_FILES = $(wildcard $(INCLUDE_DIR)/*.h)

# Output files
MAIN_EXECUTABLE = $(BUILD_DIR)/loggen
#TEST_EXECUTABLE = $(BUILD_DIR)/logging_control_test

# GTest settings
GTEST_LIBS = -lgtest -lgtest_main -pthread

.PHONY: all clean build


#all: build $(MAIN_EXECUTABLE) $(TEST_EXECUTABLE)
all: build $(MAIN_EXECUTABLE)


build:
	mkdir -p $(BUILD_DIR)

$(MAIN_EXECUTABLE): $(MAIN_SOURCE) $(INCLUDE_FILES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $(MAIN_SOURCE) -o $(MAIN_EXECUTABLE)

$(TEST_EXECUTABLE): $(TEST_SOURCE) $(INCLUDE_FILES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR)   $(TEST_SOURCE) $(GTEST_LIBS) -o $(TEST_EXECUTABLE)

clean:
	rm -rf $(BUILD_DIR)

#test: $(TEST_EXECUTABLE)
#	./$(TEST_EXECUTABLE)
