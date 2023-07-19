CC = g++
CFLAGS = -std=c++17 -Wall -Iinclude
LIBS = -lpthread

SRC = src/main.cpp 
#src/DataTransferManager.cpp
OBJS = build/main.o 
#build/DataTransferManager.o
EXECUTABLE = build/data_transfer_example

all: build $(EXECUTABLE)

build:
	mkdir -p build

$(EXECUTABLE): $(OBJS)
	$(CC) $(CFLAGS) build/*.o -o $(EXECUTABLE) $(LIBS)

build/main.o: src/main.cpp
	$(CC) $(CFLAGS) -c $< -o $@

build/DataTransferManager.o: src/DataTransferManager.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXECUTABLE)
