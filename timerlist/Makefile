CC=g++
CFLAGS=-std=c++17 -pthread -lgtest -lgtest_main -Iinclude

all: build build/timer_manager_test build/TimerList

build/timer_manager_test: src/timer_91.cpp build_obj/timer_base.o include/timer_1.h
	$(CC) $(CFLAGS) -o build/timer_manager_test src/timer_91.cpp build_obj/timer_base.o

build_obj/timer_base.o: src/timer_base.cpp include/timer_1.h
	$(CC) $(CFLAGS) -o $@ -c src/timer_base.cpp

build/TimerList: src/TimerList.cpp  include/TimerList.h
	$(CC) $(CFLAGS) -o build/TimerList src/TimerList.cpp 

.PHONY: clean

build:
	mkdir -p build
	mkdir -p build_obj

clean:
	rm -f build/timer_manager_test build_obj/timer_base.o build/TimerList
