CC = g++
CFLAGS = -std=c++17 -Iinclude
LDFLAGS = -lgtest -lgmock -lpthread


all: build build/cmdargs build/cmdargs_test

build:
	mkdir -p build

build/cmdargs: src/CommandArgs.cpp
	$(CC) $(CFLAGS)  -o $@ $<

build/cmdargs_test: test/CommandArgsTest.cpp
	$(CC) $(CFLAGS)  -o $@ $< $(LDFLAGS)

clean:
	rm -f build/cmdargs
	rm -f build/cmdargs_test

test: build build/cmdargs_test
	./build/cmdargs_test
