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

# Version numbers
LIBVER_MAJOR:=`sed -n '/define XXH_VERSION_MAJOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < xxhash.h`
LIBVER_MINOR:=`sed -n '/define XXH_VERSION_MINOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < xxhash.h`
LIBVER_PATCH:=`sed -n '/define XXH_VERSION_RELEASE/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < xxhash.h`
LIBVER := $(LIBVER_MAJOR).$(LIBVER_MINOR).$(LIBVER_PATCH)

CFLAGS ?= -O3
CFLAGS += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
          -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
		  -Wstrict-prototypes -Wundef
FLAGS  := $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(MOREFLAGS)
XXHSUM_VERSION=$(LIBVER)
MD2ROFF = ronn
MD2ROFF_FLAGS = --roff --warnings --manual="User Commands" --organization="xxhsum $(XXHSUM_VERSION)"

# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif

.PHONY: clean all

default: xxhsum

all: xxhsum xxhsum32 xxhsum_inlinedXXH

xxhsum: xxhash.c xxhsum.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)
	ln -sf $@ xxh32sum
	ln -sf $@ xxh64sum

xxhsum32: xxhash.c xxhsum.c
	$(CC) -m32 $(FLAGS) $^ -o $@$(EXT)

xxhsum_inlinedXXH: xxhsum.c
	$(CC) $(FLAGS) -DXXH_PRIVATE_API $^ -o $@$(EXT)

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

test-xxhsum-c: xxhsum
	# xxhsum to/from pipe
	./xxhsum * | ./xxhsum -c -
	./xxhsum -H0 * | ./xxhsum -c -
	# xxhsum to/from file, shell redirection
	./xxhsum * > .test.xxh64
	./xxhsum -H0 * > .test.xxh32
	./xxhsum -c .test.xxh64
	./xxhsum -c .test.xxh32
	./xxhsum -c < .test.xxh64
	./xxhsum -c < .test.xxh32
	# xxhsum -c warns improperly format lines.
	cat .test.xxh64 .test.xxh32 | ./xxhsum -c -
	cat .test.xxh32 .test.xxh64 | ./xxhsum -c -
	# Expects "FAILED"
	echo "0000000000000000  LICENSE" | ./xxhsum -c -; test $$? -eq 1
	echo "00000000  LICENSE" | ./xxhsum -c -; test $$? -eq 1
	# Expects "FAILED open or read"
	echo "0000000000000000  test-expects-file-not-found" | ./xxhsum -c -; test $$? -eq 1
	echo "00000000  test-expects-file-not-found" | ./xxhsum -c -; test $$? -eq 1

clean-xxhsum-c:
	@$(RM) -f .test.xxh32 .test.xxh64

armtest: clean
	@echo ---- test ARM compilation ----
	$(MAKE) xxhsum CC=arm-linux-gnueabi-gcc MOREFLAGS="-Werror -static"

clangtest: clean
	@echo ---- test clang compilation ----
	$(MAKE) all CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion"

gpptest: clean
	@echo ---- test g++ compilation ----
	$(MAKE) all CC=g++ CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror"

c90test: clean
	@echo ---- test strict C90 compilation [xxh32 only] ----
	$(CC) -std=c90 -Werror -pedantic -DXXH_NO_LONG_LONG -c xxhash.c
	$(RM) xxhash.o

usan: clean
	@echo ---- check undefined behavior - sanitize ----
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=undefined"

staticAnalyze: clean
	@echo ---- static analyzer - scan-build ----
	CFLAGS="-g -Werror" scan-build --status-bugs -v $(MAKE) all

namespaceTest:
	$(CC) -c xxhash.c
	$(CC) -DXXH_NAMESPACE=TEST_ -c xxhash.c -o xxhash2.o
	$(CC) xxhash.o xxhash2.o xxhsum.c -o xxhsum2  # will fail if one namespace missing (symbol collision)
	$(RM) *.o xxhsum2  # clean

xxhsum.1: xxhsum.1.md
	cat $^ | $(MD2ROFF) $(MD2ROFF_FLAGS) | sed -n '/^\.\\\".*/!p' > $@

man: xxhsum.1

clean-man:
	$(RM) xxhsum.1

preview-man: clean-man man
	man ./xxhsum.1

test-all: clean all namespaceTest test test32 test-xxhsum-c clean-xxhsum-c \
	armtest clangtest gpptest c90test usan staticAnalyze

clean: clean-xxhsum-c
	@$(RM) -f core *.o xxhsum$(EXT) xxhsum32$(EXT) xxhsum_inlinedXXH$(EXT) xxh32sum xxh64sum
	@echo cleaning completed
