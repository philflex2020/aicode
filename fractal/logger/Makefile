CXX := g++
CXXFLAGS := -g -std=c++17 -Wall -O2 -pthread

# Your source files
SRCS := logger_server.cpp data_packet.cpp
OBJS := $(SRCS:.cpp=.o)

TARGET := logger_server

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
