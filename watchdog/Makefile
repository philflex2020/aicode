CC = g++
CFLAGS = -std=c++17 -pthread -lgtest -lgmock 

SRC = src/Watchdog.cpp
TEST_SRC = test/WatchdogTest.cpp

OBJ = build/Watchdog
TEST_OBJ = build/WatchdogTest

all: build $(OBJ) $(TEST_OBJ)


build:
	mkdir -p build

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -I./include  -o $@ $<

$(TEST_OBJ): $(TEST_SRC)
	$(CC) $(CFLAGS) -I./include  -o $@ $<


clean:
	rm -f $(OBJ)
	rm -f $(TEST_OBJ)



test: build $(TEST_OBJ)
	./$(TEST_OBJ)
