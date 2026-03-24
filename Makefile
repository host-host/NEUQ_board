CFLAGS = -O2 -Wall
LDFLAGS = -lssl -lcrypto -lcurl
CC = gcc
CXX = g++

OBJS = build/cJSON.o build/ndb.o build/http.o
LIB_HDRS = $(wildcard lib/*.h)

all: 80 1001

build:
	@mkdir -p build

80: code/80.cpp | build
	$(CXX) $< -o $@ $(CFLAGS)

build/%.o: code/lib/%.c code/lib/%.h | build
	$(CC) -c $< -o $@ $(CFLAGS)

1001: code/1001.cpp $(OBJS) $(LIB_HDRS) | build
	$(CXX) $^ -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf build 80 1001

.PHONY: all clean