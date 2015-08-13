# ################################################################
# xxHash Makefile
# Copyright (C) Yann Collet 2012-2015
#
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
# xxhsum : provides 32/64 bits hash of one or multiple files, or stdin
# ################################################################

CFLAGS ?= -O3
CFLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-qual -Wcast-align -Wstrict-prototypes -pedantic 
FLAGS  := -I. $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(MOREFLAGS)


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif

.PHONY: clean all

default: xxhsum

all: xxhsum xxhsum32

xxhsum: xxhash.c xxhsum.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)
	ln -sf $@ xxh32sum
	ln -sf $@ xxh64sum

xxhsum32: xxhash.c xxhsum.c
	$(CC) -m32 $(FLAGS) $^ -o $@$(EXT)

test: clean xxhsum
	# stdin
	./xxhsum < xxhash.c
	# multiple files
	./xxhsum *
	# internal bench
	./xxhsum -bi1
	# file bench
	./xxhsum -bi1 xxhash.c
	# memory tests
	valgrind --leak-check=yes --error-exitcode=1 ./xxhsum -bi1 xxhash.c
	valgrind --leak-check=yes --error-exitcode=1 ./xxhsum -H0 xxhash.c
	valgrind --leak-check=yes --error-exitcode=1 ./xxhsum -H1 xxhash.c

test32: clean xxhsum32
	@echo ---- test 32-bits ----
	./xxhsum32 -bi1 xxhash.c

armtest: clean
	@echo ---- test ARM compilation ----
	$(MAKE) xxhsum CC=arm-linux-gnueabi-gcc MOREFLAGS="-Werror"

clangtest: clean
	@echo ---- test clang compilation ----
	$(MAKE) all CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion"

gpptest: clean
	@echo ---- test g++ compilation ----
	$(MAKE) all CC=g++ CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror"

sanitize: clean
	@echo ---- check undefined behavior - sanitize ----
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=undefined"

staticAnalyze: clean
	@echo ---- static analyzer - scan-build ----
	scan-build --status-bugs -v $(MAKE) all MOREFLAGS=-g

test-all: test test32 armtest clangtest gpptest sanitize staticAnalyze

clean:
	@rm -f core *.o xxhsum$(EXT) xxhsum32$(EXT) xxh32sum xxh64sum
	@echo cleaning completed


