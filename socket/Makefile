# Directories
INC_DIR := include
SRC_DIR := src
BUILD_DIR := build

# Compiler and flags
CC := g++
CFLAGS := -g -std=c++11 -Wall -Wextra -I$(INC_DIR) -lpthread

# Source files
CLIENT_SRCS := $(wildcard $(SRC_DIR)/client.cpp $(SRC_DIR)/perf.cpp)
SERVER_SRCS := $(wildcard $(SRC_DIR)/server.cpp $(SRC_DIR)/perf.cpp)

# Object files
CLIENT_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))
SERVER_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SERVER_SRCS))

# Targets
CLIENT_TARGET := client
SERVER_TARGET := server

all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o $@

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(CFLAGS) $(SERVER_OBJS) -o $@

clean:
	rm -rf $(BUILD_DIR) $(CLIENT_TARGET) $(SERVER_TARGET)

.PHONY: all clean
