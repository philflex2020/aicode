# Directories
SRC_DIR := src
INC_DIR := inc
BUILD_DIR := build

# Target Executables
TARGET := $(BUILD_DIR)/compare
ARM_TARGET := $(BUILD_DIR)/compare_arm

# Source Files
SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC))

# Compiler & Flags
CXX := g++
CROSS_COMPILE_ARM := aarch64-linux-gnu-g++

# Detect architecture
ARCH := $(shell uname -m)
ifeq ($(ARCH), x86_64)
    CXXFLAGS := -O2 -march=native -mavx -pthread -I$(INC_DIR)
else ifeq ($(ARCH), aarch64)
    CXXFLAGS := -O2 -mcpu=cortex-a55 -mfpu=neon -pthread -I$(INC_DIR)
else
    $(error Unsupported architecture: $(ARCH))
endif

ARM_FLAGS := -O2 -mcpu=cortex-a55 -mfpu=neon -pthread -I$(INC_DIR)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile for x86
all: $(BUILD_DIR) $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

# Compile for ARM (Cross-Compilation)
arm: $(BUILD_DIR) $(OBJ)
	$(CROSS_COMPILE_ARM) $(ARM_FLAGS) -o $(ARM_TARGET) $(OBJ)

# Compile Object Files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all arm clean
