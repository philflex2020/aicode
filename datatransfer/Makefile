CC = g++
CFLAGS = -std=c++11 -Wall
LIBS = -lpthread

SRC = main.cpp DataTransferManager.cpp
OBJS = $(SRC:.cpp=.o)
EXECUTABLE = data_transfer_example

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXECUTABLE) $(LIBS)

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXECUTABLE)
