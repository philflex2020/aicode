CC=g++
CPPFLAGS+=-std=c++17 -pthread -lgtest -lgtest_main -Iinclude
SRC = src/TimerList.cpp
OBJ = build/TimerList
INC = include/TimerList.h

all: build build/timer_manager_test $(OBJ)

build/timer_manager_test: src/timer_91.cpp build_obj/timer_base.o $(INC)
	$(CC) $(CPPFLAGS) -o build/timer_manager_test src/timer_91.cpp build_obj/timer_base.o

build_obj/timer_base.o: src/timer_base.cpp $(INC)
	$(CC) $(CPPFLAGS) -o $@ -c src/timer_base.cpp

$(OBJ): $(SRC)  $(INC)
	$(CC) $(CPPFLAGS) -o build/TimerList src/TimerList.cpp 

.PHONY: clean

build:
	mkdir -p build
	mkdir -p build_obj

clean:
	rm -f build/timer_manager_test build_obj/timer_base.o $(OBJ)
