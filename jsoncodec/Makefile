CXX := g++
CXXFLAGS := -g -std=c++17 -Wall -Wextra -Iinclude
LDFLAGS := -lz

SRCDIR := src
INCDIR := include
BUILDDIR := build
BINDIR := bin

SRC := $(wildcard $(SRCDIR)/*.cpp)
OBJ := $(SRC:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)
DEP := $(OBJ:.o=.d)

TARGET := $(BINDIR)/json_codec

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

-include $(DEP)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

clean:
	@rm -rf $(BUILDDIR) $(BINDIR)


