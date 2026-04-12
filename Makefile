CFLAGS = -O2 -Wall
LDFLAGS = -lssl -lcrypto -lcurl
CC = gcc
CXX = g++

SRC_C = $(wildcard code/lib/*.c)
SRC_CPP = $(wildcard code/lib/*.cpp)
OBJS = $(patsubst code/lib/%.c, build/%.o, $(SRC_C)) \
       $(patsubst code/lib/%.cpp, build/%.o, $(SRC_CPP))

LIB_HDRS = $(wildcard lib/*.h)

all: 80 1001

build:
	@mkdir -p build

build/%.o: code/lib/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: code/lib/%.cpp | build
	$(CXX) $(CFLAGS) -c $< -o $@

80: code/80.cpp | build
	$(CXX) $< -o $@ $(CFLAGS)

1001: code/1001.cpp $(OBJS) $(LIB_HDRS) | build
	$(CXX) $(filter-out $(LIB_HDRS), $^) -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf build 80 1001

.PHONY: all clean