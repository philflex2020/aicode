CC := g++
CFLAGS := -std=c++11 -Iinclude -pthread
SRCDIR := src
BUILDDIR := build
TESTDIR := test
LIBDIR := lib
BINDIR := bin
TESTBINDIR := $(TESTDIR)/bin

SRC := $(wildcard $(SRCDIR)/*.cpp)
OBJ := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRC))
LIB := $(LIBDIR)/libproducer.a $(LIBDIR)/libconsumer.a
TESTSRC := $(wildcard $(TESTDIR)/*.cpp)
TESTOBJ := $(patsubst $(TESTDIR)/%.cpp,$(BUILDDIR)/%.o,$(TESTSRC))

all: directories libraries producer_consumer main

directories:
	mkdir -p $(BUILDDIR)
	mkdir -p $(LIBDIR)
	mkdir -p $(BINDIR)

producer_consumer: $(OBJ)
	$(CC) $(CFLAGS) $^ -L$(LIBDIR) -lproducer -lconsumer -o $(BINDIR)/$@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

libraries: $(LIB)

$(LIBDIR)/libproducer.a: $(BUILDDIR)/producer.o
	ar rcs $@ $^

$(LIBDIR)/libconsumer.a: $(BUILDDIR)/consumer.o
	ar rcs $@ $^

main: $(filter-out $(BUILDDIR)/main_test.o, $(OBJ))
	$(CC) $(CFLAGS) $^ -L$(LIBDIR) -lproducer -lconsumer -o $(BINDIR)/main

test: CFLAGS += -I$(SRCDIR) -I$(TESTDIR)
test: $(TESTOBJ) $(filter-out $(BUILDDIR)/main.o, $(OBJ))
	mkdir -p $(TESTBINDIR)
	mkdir -p $(LIBDIR)
	$(CC) $(CFLAGS) -o $(TESTBINDIR)/test $^ -L$(LIBDIR) -L$(LIBDIR) -lproducer -lconsumer -lgtest

$(BUILDDIR)/%.o: $(TESTDIR)/%.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(LIBDIR) $(BINDIR) $(TESTBINDIR)