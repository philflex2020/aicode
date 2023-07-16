CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic

SRC = src/TimeTracker.cpp
INC = include/TimeTracker.h
TEST_SRC = test/TimeTrackerTest.cpp
BUILD_DIR = build
TARGET = $(BUILD_DIR)/TimeTracker 
TEST_TARGET = $(BUILD_DIR)/TimeTrackerTest 

LIB_PATH = /usr/local/lib
INCLUDE_PATH = ./include

all: build $(TEST_TARGET) $(TARGET)

build:
	mkdir -p $(BUILD_DIR)

$(TEST_TARGET): $(TEST_SRC) $(INC) 
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_PATH) $< -o $@ -L$(LIB_PATH) -lsimdjson -lgtest -lspdlog -lpthread

$(TARGET): $(SRC) $(INC)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_PATH) $< -o $@ -L$(LIB_PATH) -lsimdjson -lspdlog

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET)
	rm -f $(TEST_TARGET)