CC = g++
CFLAGS = -std=c++17 -pthread -lgtest -lgmock

all: build build/watchdog build/watchdogtest

build:
	mkdir -p build

build/watchdog: src/Watchdog.cpp
	$(CC) $(CFLAGS) -I./include  -o $@ $<

build/watchdogtest: test/WatchdogTest.cpp
	$(CC) $(CFLAGS) -I./include  -o $@ $<

clean:
	rm -f build/watchdog
	rm -f build/watchdogtest



test: build build/watchdogtest
	./build/watchdogtest
