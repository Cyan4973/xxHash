# ################################################################
# xxHash Makefile
# Copyright (C) Yann Collet 2012-2014
# GPL v2 License
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# You can contact the author at :
#  - xxHash source repository : http://code.google.com/p/xxhash/
# ################################################################
# xxHash.exe : benchmark program, to demonstrate xxHash speed
# ################################################################

CC=gcc
CFLAGS+= -I. -std=c99 -O3 -Wall -Wextra -Wundef -Wshadow -Wstrict-prototypes


OS := $(shell uname)
ifeq ($(OS),Linux)
EXT =
else
EXT =.exe
endif

# Minimize test target for Travis CI's Build Matrix
ifeq ($(XXH_TRAVIS_CI_ENV),-m32)
TEST_TARGETS=test-32
else ifeq ($(XXH_TRAVIS_CI_ENV),-m64)
TEST_TARGETS=test-64
else
TEST_TARGETS=test-64 test-32
endif

default: xxHash

all: xxHash xxHash32

xxHash: xxhash.c bench.c
	$(CC)      $(CFLAGS) $^ -o $@$(EXT)

xxHash32: xxhash.c bench.c
	$(CC) -m32 $(CFLAGS) $^ -o $@$(EXT)

test: $(TEST_TARGETS)

test-64: xxHash
	./xxHash bench.c
	valgrind ./xxHash -i1 bench.c

test-32: xxHash32
	./xxHash32 bench.c

clean:
	rm -f core *.o xxHash$(EXT) xxHash32$(EXT)


