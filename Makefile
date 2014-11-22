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

CC     := $(CC)
CFLAGS ?= -O3
CFLAGS += -I. -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif


default: xxhsum

all: xxhsum xxhsum32

xxhsum: xxhash.c xxhsum.c
	$(CC)      $(CFLAGS) $^ -o $@$(EXT)
	ln -sf $@ xxh32sum
	ln -sf $@ xxh64sum

xxhsum32: xxhash.c xxhsum.c
	$(CC) -m32 $(CFLAGS) $^ -o $@$(EXT)

test: $(TEST_TARGETS)

test: xxhsum
	./xxhsum < xxhash.c
	./xxhsum -b xxhash.c
	valgrind --leak-check=yes ./xxhsum -bi1 xxhash.c
	valgrind --leak-check=yes ./xxhsum -H0 xxhash.c
	valgrind --leak-check=yes ./xxhsum -H1 xxhash.c

test-all: test xxhsum32
	./xxhsum32 -b xxhash.c

clean:
	@rm -f core *.o xxhsum$(EXT) xxhsum32$(EXT) xxh32sum xxh64sum
	@echo cleaning completed


