CXX = g++
CXXFLAGS += -std=c++17 -Wall

SRCDIR = src
INCDIR = include

TARGET = main
SRC = $(wildcard $(SRCDIR)/*.cpp)
OBJ = $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
