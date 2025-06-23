# CXX := g++
# CXXFLAGS := -g -std=c++17 -Wall -O2 -pthread

# # Your source files
# SRCS := logger_server.cpp data_packet.cpp
# OBJS := $(SRCS:.cpp=.o)

# TARGET := logger_server

# # Test tool
# REPLAY_SRC := log_replay.cpp
# REPLAY_BIN := log_replay

# all: $(TARGET)

# $(TARGET): $(OBJS)
# 	$(CXX) $(CXXFLAGS) -o $@ $^

# %.o: %.cpp
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# # 't' = build log_replay
# t: $(REPLAY_BIN)

# $(REPLAY_BIN): $(REPLAY_SRC)
# 	$(CXX) $(CXXFLAGS) -o $@ $<

# clean:
# 	rm -f $(TARGET) $(REPLAY_BIN) $(OBJS)



CXX := g++
CXXFLAGS := -g -std=c++17 -Wall -O2 -pthread

# Server build
SRCS := logger_server.cpp data_packet.cpp
OBJS := $(SRCS:.cpp=.o)
TARGET := logger_server

# Log replay tool
REPLAY_SRC := log_replay.cpp
REPLAY_BIN := log_replay

# Logger client
CLIENT_SRC := logger_client.cpp
CLIENT_BIN := logger_client

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 't' = build log_replay
t: $(REPLAY_BIN)

$(REPLAY_BIN): $(REPLAY_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

# 'client' = build logger_client
client: $(CLIENT_BIN)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) $(REPLAY_BIN) $(CLIENT_BIN) $(OBJS)
