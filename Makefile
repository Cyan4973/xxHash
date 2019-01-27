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
LIBVER_MAJOR_SCRIPT:=`sed -n '/define XXH_VERSION_MAJOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < xxhash.h`
LIBVER_MINOR_SCRIPT:=`sed -n '/define XXH_VERSION_MINOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < xxhash.h`
LIBVER_PATCH_SCRIPT:=`sed -n '/define XXH_VERSION_RELEASE/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < xxhash.h`
LIBVER_MAJOR := $(shell echo $(LIBVER_MAJOR_SCRIPT))
LIBVER_MINOR := $(shell echo $(LIBVER_MINOR_SCRIPT))
LIBVER_PATCH := $(shell echo $(LIBVER_PATCH_SCRIPT))
LIBVER := $(LIBVER_MAJOR).$(LIBVER_MINOR).$(LIBVER_PATCH)

CFLAGS ?= -O2
DEBUGFLAGS+=-Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
            -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wundef -Wpointer-arith -Wformat-security \
            -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            -Wredundant-decls -Wstrict-overflow=5
CFLAGS += $(DEBUGFLAGS)
FLAGS   = $(CFLAGS) $(CPPFLAGS) $(MOREFLAGS)
XXHSUM_VERSION = $(LIBVER)
MD2ROFF = ronn
MD2ROFF_FLAGS = --roff --warnings --manual="User Commands" --organization="xxhsum $(XXHSUM_VERSION)"

# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif

# OS X linker doesn't support -soname, and use different extension
# see : https://developer.apple.com/library/mac/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/DynamicLibraryDesignGuidelines.html
ifeq ($(shell uname), Darwin)
	SHARED_EXT = dylib
	SHARED_EXT_MAJOR = $(LIBVER_MAJOR).$(SHARED_EXT)
	SHARED_EXT_VER = $(LIBVER).$(SHARED_EXT)
	SONAME_FLAGS = -install_name $(LIBDIR)/libxxhash.$(SHARED_EXT_MAJOR) -compatibility_version $(LIBVER_MAJOR) -current_version $(LIBVER)
else
	SONAME_FLAGS = -Wl,-soname=libxxhash.$(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT = so
	SHARED_EXT_MAJOR = $(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT_VER = $(SHARED_EXT).$(LIBVER)
endif

LIBXXH = libxxhash.$(SHARED_EXT_VER)


.PHONY: default
default: DEBUGFLAGS=
default: lib xxhsum_and_links

.PHONY: all
all: lib xxhsum xxhsum_inlinedXXH

xxhsum : xxhash.o xxhsum.o

xxhash.o: %.o: %.c xxhash-vec.h xxhash.h
xxhsum.o: %.o: %.c xxhash.h

xxhsum32: CFLAGS += -m32
xxhsum32: xxhash.c xxhsum.c
	$(CC) $(FLAGS) $^ $(LDFLAGS) -o $@$(EXT)

.PHONY: xxhsum_and_links
xxhsum_and_links: xxhsum xxh32sum xxh32asum xxh64sum xxh64asum

xxh32sum xxh32asum xxh64sum xxh64asum: xxhsum
	ln -sf $^ $@

xxhsum_inlinedXXH: CPPFLAGS += -DXXH_INLINE_ALL
xxhsum_inlinedXXH: xxhsum.c
	$(CC) $(FLAGS) $^ -o $@$(EXT)


# library

libxxhash.a: ARFLAGS = rcs
libxxhash.a: xxhash.o
	$(AR) $(ARFLAGS) $@ $^

$(LIBXXH): LDFLAGS += -shared
ifeq (,$(filter Windows%,$(OS)))
$(LIBXXH): CFLAGS += -fPIC
endif
$(LIBXXH): xxhash.c xxhash-vec.h xxhash.h
	$(CC) $(FLAGS) $< $(LDFLAGS) $(SONAME_FLAGS) -o $@
	ln -sf $@ libxxhash.$(SHARED_EXT_MAJOR)
	ln -sf $@ libxxhash.$(SHARED_EXT)

libxxhash : $(LIBXXH)

.PHONY: lib
lib: libxxhash.a libxxhash


# tests

.PHONY: check
check: xxhsum
	# stdin
	./xxhsum < xxhash.c
	# multiple files
	./xxhsum xxhash.* xxhsum.*
	# internal bench
	./xxhsum -bi1
	# file bench
	./xxhsum -bi1 xxhash.c

.PHONY: test-mem
test-mem: xxhsum
	# memory tests
	valgrind --leak-check=yes --error-exitcode=1 ./xxhsum -bi1 xxhash.c
	valgrind --leak-check=yes --error-exitcode=1 ./xxhsum -H0  xxhash.c
	valgrind --leak-check=yes --error-exitcode=1 ./xxhsum -H1  xxhash.c

.PHONY: test32
test32: clean xxhsum32
	@echo ---- test 32-bit ----
	./xxhsum32 -bi1 xxhash.c

test-xxhsum-c: xxhsum
	# xxhsum to/from pipe
	./xxhsum lib* | ./xxhsum -c -
	./xxhsum -H0 lib* | ./xxhsum -c -
	# xxhsum to/from file, shell redirection
	./xxhsum lib* > .test.xxh64
	./xxhsum -H0 lib* > .test.xxh32
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
	@$(RM) -f .test.xxh32 .test.xxh64

armtest: clean
ifeq (,$(shell which arm-linux-gnueabi-gcc 2>&1 || true))
	@echo Skipping ARM compilation, arm-linux-gnueabi-gcc not found
else
	@echo ---- test ARM compilation ----
	CC=arm-linux-gnueabi-gcc MOREFLAGS="-Werror -static" $(MAKE) xxhsum
endif

clangtest: clean
	@echo ---- test clang compilation ----
	CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion" $(MAKE) all

cxxtest: clean
	@echo ---- test C++ compilation ----
	CC="$(CXX) -Wno-deprecated" $(MAKE) all CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror -fPIC"

.PHONY: c90test
c90test: CPPFLAGS += -DXXH_NO_LONG_LONG -DXXH_NO_ALT_HASHES -DXXH_VECTORIZE=0
c90test: CFLAGS += -std=c90 -Werror -pedantic
c90test: xxhash.c
	@echo ---- test strict C90 compilation [xxh32 only] ----
	$(RM) xxhash.o
	$(CC) $(FLAGS) $^ $(LDFLAGS) -c
	$(RM) xxhash.o

usan: CC=clang
usan: clean
	@echo ---- check undefined behavior - sanitize ----
	$(MAKE) clean test CC=$(CC) MOREFLAGS="-g -fsanitize=undefined -fno-sanitize-recover=all"

.PHONY: staticAnalyze
staticAnalyze: clean
	@echo ---- static analyzer - scan-build ----
	CFLAGS="-g -Werror" scan-build --status-bugs -v $(MAKE) all

.PHONY: cppcheck
cppcheck:
	@echo ---- static analyzer - cppcheck ----
	cppcheck . --force --enable=warning,portability,performance,style --inline-suppr --error-exitcode=1 --suppress=knownConditionTrueFalse,duplicateBranch > /dev/null

.PHONY: namespaceTest
namespaceTest:
	$(CC) -c xxhash.c
	$(CC) -DXXH_NAMESPACE=TEST_ -c xxhash.c -o xxhash2.o
	$(CC) xxhash.o xxhash2.o xxhsum.c -o xxhsum2  # will fail if one namespace missing (symbol collision)
	$(RM) *.o *.obj xxhsum2  # clean

xxhsum.1: xxhsum.1.md
	cat $^ | $(MD2ROFF) $(MD2ROFF_FLAGS) | sed -n '/^\.\\\".*/!p' > $@

.PHONY: man
man: xxhsum.1

clean-man:
	$(RM) xxhsum.1

preview-man: clean-man man
	man ./xxhsum.1

.PHONY: test
test: all namespaceTest check test-xxhsum-c c90test

.PHONY: test-all

# macOS disabled 32-bit support in recent Xcode versions.
ifeq ($(shell uname 2> /dev/null || true),Darwin)
test-all: test armtest clangtest cxxtest usan listL120 trailingWhitespace staticAnalyze cppcheck
else
test-all: test test32 armtest clangtest cxxtest usan listL120 trailingWhitespace staticAnalyze cppcheck
endif

.PHONY: listL120
listL120:  # extract lines >= 120 characters in *.{c,h}, by Takayuki Matsuoka (note : $$, for Makefile compatibility)
	find . -type f -name '*.c' -o -name '*.h' | while read -r filename; do awk 'length > 120 {print FILENAME "(" FNR "): " $$0}' $$filename; done

.PHONY: trailingWhitespace
trailingWhitespace:
	! grep -E "`printf '[ \\t]$$'`" *.1 *.c *.h LICENSE Makefile cmake_unofficial/CMakeLists.txt

.PHONY: clean
clean:
	@$(RM) -r *.dSYM   # Mac OS-X specific
	@$(RM) core *.o *.obj libxxhash.*
	@$(RM) xxhsum$(EXT) xxhsum32$(EXT) xxhsum_inlinedXXH$(EXT) xxh64asum xxh32asum xxh32sum xxh64sum
	@echo cleaning completed


#-----------------------------------------------------------------------------
# make install is validated only for the following targets
#-----------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS))

.PHONY: list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$' | xargs

DESTDIR     ?=
# directory variables : GNU conventions prefer lowercase
# see https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html
# support both lower and uppercase (BSD), use uppercase in script
prefix      ?= /usr/local
PREFIX      ?= $(prefix)
exec_prefix ?= $(PREFIX)
libdir      ?= $(exec_prefix)/lib
LIBDIR      ?= $(libdir)
includedir  ?= $(PREFIX)/include
INCLUDEDIR  ?= $(includedir)
bindir      ?= $(exec_prefix)/bin
BINDIR      ?= $(bindir)
datarootdir ?= $(PREFIX)/share
mandir      ?= $(datarootdir)/man
man1dir     ?= $(mandir)/man1

ifneq (,$(filter $(shell uname),OpenBSD FreeBSD NetBSD DragonFly SunOS))
MANDIR  ?= $(PREFIX)/man/man1
else
MANDIR  ?= $(man1dir)
endif

ifneq (,$(filter $(shell uname),SunOS))
INSTALL ?= ginstall
else
INSTALL ?= install
endif

INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA    ?= $(INSTALL) -m 644


.PHONY: install
install: lib xxhsum
	@echo Installing libxxhash
	@$(INSTALL) -d -m 755 $(DESTDIR)$(LIBDIR)
	@$(INSTALL_DATA) libxxhash.a $(DESTDIR)$(LIBDIR)
	@$(INSTALL_PROGRAM) $(LIBXXH) $(DESTDIR)$(LIBDIR)
	@ln -sf $(LIBXXH) $(DESTDIR)$(LIBDIR)/libxxhash.$(SHARED_EXT_MAJOR)
	@ln -sf $(LIBXXH) $(DESTDIR)$(LIBDIR)/libxxhash.$(SHARED_EXT)
	@$(INSTALL) -d -m 755 $(DESTDIR)$(INCLUDEDIR)   # includes
	@$(INSTALL_DATA) xxhash.h $(DESTDIR)$(INCLUDEDIR)
	@echo Installing xxhsum
	@$(INSTALL) -d -m 755 $(DESTDIR)$(BINDIR)/ $(DESTDIR)$(MANDIR)/
	@$(INSTALL_PROGRAM) xxhsum $(DESTDIR)$(BINDIR)/xxhsum
	@ln -sf xxhsum $(DESTDIR)$(BINDIR)/xxh32sum
	@ln -sf xxhsum $(DESTDIR)$(BINDIR)/xxh32asum
	@ln -sf xxhsum $(DESTDIR)$(BINDIR)/xxh64sum
	@ln -sf xxhsum $(DESTDIR)$(BINDIR)/xxh64asum
	@echo Installing man pages
	@$(INSTALL_DATA) xxhsum.1 $(DESTDIR)$(MANDIR)/xxhsum.1
	@ln -sf xxhsum.1 $(DESTDIR)$(MANDIR)/xxh32sum.1
	@ln -sf xxhsum.1 $(DESTDIR)$(MANDIR)/xxh64sum.1
	@echo xxhash installation completed

.PHONY: uninstall
uninstall:
	@$(RM) $(DESTDIR)$(LIBDIR)/libxxhash.a
	@$(RM) $(DESTDIR)$(LIBDIR)/libxxhash.$(SHARED_EXT)
	@$(RM) $(DESTDIR)$(LIBDIR)/libxxhash.$(SHARED_EXT_MAJOR)
	@$(RM) $(DESTDIR)$(LIBDIR)/$(LIBXXH)
	@$(RM) $(DESTDIR)$(INCLUDEDIR)/xxhash.h
	@$(RM) $(DESTDIR)$(BINDIR)/xxh32sum
	@$(RM) $(DESTDIR)$(BINDIR)/xxh32asum
	@$(RM) $(DESTDIR)$(BINDIR)/xxh64sum
	@$(RM) $(DESTDIR)$(BINDIR)/xxh64asum
	@$(RM) $(DESTDIR)$(BINDIR)/xxhsum
	@$(RM) $(DESTDIR)$(MANDIR)/xxh32sum.1
	@$(RM) $(DESTDIR)$(MANDIR)/xxh64sum.1
	@$(RM) $(DESTDIR)$(MANDIR)/xxhsum.1
	@echo xxhsum successfully uninstalled

endif
