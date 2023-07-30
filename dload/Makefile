# Compiler
CXX := g++

# Directories
FUNC_DIR := func
SRC_DIR := src
INCLUDE_DIR := include
BUILD_SHARE_DIR := build_share
BUILD_DIR := build
INSTALL_DIR := /usr/local/lib/myfun


# Source files
FUNC_SOURCES := $(wildcard $(FUNC_DIR)/*.cpp)
SRC_SOURCES := $(wildcard $(SRC_DIR)/*.cpp)

# Object files
FUNC_OBJECTS := $(FUNC_SOURCES:$(FUNC_DIR)/%.cpp=$(BUILD_SHARE_DIR)/%.o)
SRC_OBJECTS := $(SRC_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_SHARE_DIR)/%.o)

# Compiler flags
CXXFLAGS := -std=c++17 -I$(INCLUDE_DIR) -fPIC

# Targets
all: $(BUILD_DIR) $(INSTALL_DIR) $(BUILD_DIR)/main_program

$(BUILD_DIR)/main_program: $(SRC_OBJECTS) $(BUILD_SHARE_DIR)/multiply_function.so
	$(CXX) $(CXXFLAGS) -o $@ $(SRC_OBJECTS) -ldl

$(BUILD_SHARE_DIR)/multiply_function.so: $(FUNC_OBJECTS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $^

$(BUILD_SHARE_DIR)/%.o: $(FUNC_DIR)/%.cpp | $(BUILD_SHARE_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_SHARE_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_SHARE_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_SHARE_DIR):
	mkdir -p $@

$(BUILD_DIR):
	mkdir -p $@

$(INSTALL_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_SHARE_DIR)/*.o $(BUILD_SHARE_DIR)/multiply_function.so $(BUILD_DIR)/main_program

install: $(BUILD_SHARE_DIR)/multiply_function.so
	install -m 0755 $< $(INSTALL_DIR)

.PHONY: all clean