CC = g++
CFLAGS = -std=c++11 -Wall -Iinclude
LDFLAGS = -lgtest -lpthread

SRC = src/LoggingControl.cpp
TEST_SRC  = test/LoggingControlTest.cpp

OBJ = build/LoggingControl
TEST_OBJ  = build/LoggingControlTest

all: build  $(OBJ) $(TEST_OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(TEST_OBJ): $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) -o $@ $(LDFLAGS)

.PHONY: clean

build:
	mkdir -p build

clean:
	rm -f $(TEST_OBJ) $(OBJ)