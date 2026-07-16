CFLAGS = -O2 -Wall
LDFLAGS = -lssl -lcrypto -lcurl
CC = gcc
CXX = g++

SRC_C = $(wildcard code/lib/*.c)
SRC_CPP = $(wildcard code/lib/*.cpp)
OBJS = $(patsubst code/lib/%.c, build/%.o, $(SRC_C)) \
       $(patsubst code/lib/%.cpp, build/%.o, $(SRC_CPP))

LIB_HDRS = $(wildcard code/lib/*.h)

SRC_MAIN = $(wildcard code/*.cpp)

BINS = $(patsubst code/%.cpp, build/%, $(SRC_MAIN))

all: $(BINS)

build:
	@mkdir -p build

build/%.o: code/lib/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: code/lib/%.cpp | build
	$(CXX) $(CFLAGS) -c $< -o $@

build/%: code/%.cpp $(OBJS) $(LIB_HDRS) | build
	$(CXX) $(filter-out $(LIB_HDRS), $^) -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf build

.PRECIOUS: build/%.o

.PHONY: all clean