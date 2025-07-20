# Simple Makefile for sox_ws
CXX      := g++
CXXFLAGS := -std=c++17 -O2 -pthread -I. -I inc
TARGET   := sox_ws
SRCS     := src/sox_ws.cpp
OBJS     := $(SRCS:.cpp=.o)

# You already have shared_mem.h + httplib.h + nlohmann/json.hpp in same dir or include path

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
