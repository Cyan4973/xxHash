CC=gcc
CFLAGS=-I. -std=c99 -Wall -W -Wundef -Wno-implicit-function-declaration

OS := $(shell uname)
ifeq ($(OS),Linux)
EXT =
else
EXT =.exe
endif

default: xxHash

all: xxHash

xxHash: xxhash.c bench.c
	$(CC) -O2 $(CFLAGS) $^ -o $@$(EXT)

clean:
	rm -f core *.o xxHash$(EXT)
