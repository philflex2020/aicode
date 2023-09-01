CXX = g++
CXXFLAGS = -std=c++17 -Iinclude
LDFLAGS = -lpthread -lgtest -lgtest_main
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
TEST_DIR = test
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(BUILD_DIR)/%.o)

all: $(BUILD_DIR)/main $(BUILD_DIR)/test_Timer

$(BUILD_DIR)/main: $(BUILD_DIR)/main.o $(BUILD_DIR)/Timer.o
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_Timer: $(BUILD_DIR)/test_Timer.o $(BUILD_DIR)/Timer.o
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir $@

clean:
	rm -r $(BUILD_DIR)

.PHONY: all clean
