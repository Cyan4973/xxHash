# ################################################################
# xxHash Makefile
# Copyright (C) Yann Collet 2012-present
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

CFLAGS ?= -O3
DEBUGFLAGS+=-Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -Wshadow \
            -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wundef -Wpointer-arith -Wformat-security \
            -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            -Wredundant-decls -Wstrict-overflow=2
CFLAGS += $(DEBUGFLAGS)
FLAGS   = $(CFLAGS) $(CPPFLAGS) $(MOREFLAGS)
XXHSUM_VERSION = $(LIBVER)

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
default:  ## generate CLI and libraries in release mode (default for `make`)
default: DEBUGFLAGS=
default: lib xxhsum_and_links

.PHONY: all
all: lib xxhsum xxhsum_inlinedXXH

xxhsum: xxhash.o xxhsum.o  ## generate command line interface (CLI)

xxhsum32: CFLAGS += -m32  ## generate CLI in 32-bits mode
xxhsum32: xxhash.c xxhsum.c  ## do not generate object (avoid mixing different ABI)
	$(CC) $(FLAGS) $^ $(LDFLAGS) -o $@$(EXT)

xxhash.o: xxhash.h xxh3.h

xxhsum.o: xxhash.h

.PHONY: xxhsum_and_links
xxhsum_and_links: xxhsum xxh32sum xxh64sum xxh128sum

xxh32sum xxh64sum xxh128sum: xxhsum
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
$(LIBXXH): xxhash.c
	$(CC) $(FLAGS) $^ $(LDFLAGS) $(SONAME_FLAGS) -o $@
	ln -sf $@ libxxhash.$(SHARED_EXT_MAJOR)
	ln -sf $@ libxxhash.$(SHARED_EXT)

.PHONY: libxxhash
libxxhash:  ## generate dynamic xxhash library
libxxhash: $(LIBXXH)

.PHONY: lib
lib:  ## generate static and dynamic xxhash libraries
lib: libxxhash.a libxxhash


# helper targets

AWK = awk
GREP = grep
SORT = sort

.PHONY: list
list:  ## list all Makefile targets
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | $(AWK) -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | $(SORT) | egrep -v -e '^[^[:alnum:]]' -e '^$@$$' | xargs

.PHONY: help
help:  ## list documented targets
	@$(GREP) -E '^[0-9a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
	$(SORT) | \
	$(AWK) 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

.PHONY: clean
clean:  ## remove all build artifacts
	@$(RM) -r *.dSYM   # Mac OS-X specific
	@$(RM) core *.o libxxhash.*
	@$(RM) xxhsum$(EXT) xxhsum32$(EXT) xxhsum_inlinedXXH$(EXT) xxh32sum xxh64sum
	@echo cleaning completed


# =================================================
# tests
# =================================================

# make check can be run with cross-compiled binaries on emulated environments (qemu user mode)
# by setting $(RUN_ENV) to the target emulation environment
.PHONY: check
check: xxhsum   ## basic tests for xxhsum CLI, set RUN_ENV for emulated environments
	# stdin
	$(RUN_ENV) ./xxhsum < xxhash.c
	# multiple files
	$(RUN_ENV) ./xxhsum xxhash.* xxhsum.*
	# internal bench
	$(RUN_ENV) ./xxhsum -bi1
	# file bench
	$(RUN_ENV) ./xxhsum -bi1 xxhash.c
	# 32-bit
	$(RUN_ENV) ./xxhsum -H0 xxhash.c
	# 128-bit
	$(RUN_ENV) ./xxhsum -H2 xxhash.c
	# request incorrect variant
	$(RUN_ENV) ./xxhsum -H9 xxhash.c ; test $$? -eq 1


.PHONY: test-mem
VALGRIND = valgrind --leak-check=yes --error-exitcode=1
test-mem: RUN_ENV = $(VALGRIND)
test-mem: xxhsum check

.PHONY: test32
test32: clean xxhsum32
	@echo ---- test 32-bit ----
	./xxhsum32 -bi1 xxhash.c

.PHONY: test-xxhsum-c
test-xxhsum-c: xxhsum
	# xxhsum to/from pipe
	./xxhsum xxh* | ./xxhsum -c -
	./xxhsum -H0 xxh* | ./xxhsum -c -
	# xxhsum -q does not display "Loading" message into stderr (#251)
	! ./xxhsum -q xxh* 2>&1 | grep Loading
	# xxhsum to/from file, shell redirection
	./xxhsum xxh* > .test.xxh64
	./xxhsum -H0 xxh* > .test.xxh32
	./xxhsum -H2 xxh* > .test.xxh128
	./xxhsum -c .test.xxh64
	./xxhsum -c .test.xxh32
	./xxhsum -c .test.xxh128
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
	@$(RM) -f .test.xxh32 .test.xxh64 .test.xxh128

.PHONY: armtest
armtest: clean
	@echo ---- test ARM compilation ----
	CC=arm-linux-gnueabi-gcc MOREFLAGS="-Werror -static" $(MAKE) xxhsum

.PHONY: clangtest
clangtest: clean
	@echo ---- test clang compilation ----
	CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion" $(MAKE) all

.PHONY: cxxtest
cxxtest: clean
	@echo ---- test C++ compilation ----
	CC="$(CXX) -Wno-deprecated" $(MAKE) all CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror -fPIC"

.PHONY: c90test
c90test: CPPFLAGS += -DXXH_NO_LONG_LONG
c90test: CFLAGS += -std=c90 -Werror -pedantic
c90test: xxhash.c
	@echo ---- test strict C90 compilation [xxh32 only] ----
	$(RM) xxhash.o
	$(CC) $(FLAGS) $^ $(LDFLAGS) -c
	$(RM) xxhash.o

usan: CC=clang
usan:  ## check CLI runtime for undefined behavior, using clang's sanitizer
	@echo ---- check undefined behavior - sanitize ----
	$(MAKE) clean
	$(MAKE) test CC=$(CC) MOREFLAGS="-g -fsanitize=undefined -fno-sanitize-recover=all"

.PHONY: staticAnalyze
SCANBUILD ?= scan-build
staticAnalyze: clean  ## check C source files using $(SCANBUILD) static analyzer
	@echo ---- static analyzer - $(SCANBUILD) ----
	CFLAGS="-g -Werror" $(SCANBUILD) --status-bugs -v $(MAKE) all

CPPCHECK ?= cppcheck
.PHONY: cppcheck
cppcheck:  ## check C source files using $(CPPCHECK) static analyzer
	@echo ---- static analyzer - $(CPPCHECK) ----
	$(CPPCHECK) . --force --enable=warning,portability,performance,style --error-exitcode=1 > /dev/null

.PHONY: namespaceTest
namespaceTest:  ## ensure XXH_NAMESPACE redefines all public symbols
	$(CC) -c xxhash.c
	$(CC) -DXXH_NAMESPACE=TEST_ -c xxhash.c -o xxhash2.o
	$(CC) xxhash.o xxhash2.o xxhsum.c -o xxhsum2  # will fail if one namespace missing (symbol collision)
	$(RM) *.o xxhsum2  # clean

MD2ROFF ?= ronn
MD2ROFF_FLAGS ?= --roff --warnings --manual="User Commands" --organization="xxhsum $(XXHSUM_VERSION)"
xxhsum.1: xxhsum.1.md xxhash.h
	cat $< | $(MD2ROFF) $(MD2ROFF_FLAGS) | sed -n '/^\.\\\".*/!p' > $@

.PHONY: man
man: xxhsum.1  ## generate man page from markdown source

.PHONY: clean-man
clean-man:
	$(RM) xxhsum.1

.PHONY: preview-man
preview-man: man
	man ./xxhsum.1

.PHONY: test
test: DEBUGFLAGS += -DDEBUGLEVEL=1
test: all namespaceTest check test-xxhsum-c c90test test-tools

.PHONY: test-all
test-all: CFLAGS += -Werror
test-all: test test32 clangtest cxxtest usan listL120 trailingWhitespace staticAnalyze

.PHONY: test-tools
test-tools:
	MOREFLAGS=-Werror $(MAKE) -C tests/bench

.PHONY: listL120
listL120:  # extract lines >= 120 characters in *.{c,h}, by Takayuki Matsuoka (note : $$, for Makefile compatibility)
	find . -type f -name '*.c' -o -name '*.h' | while read -r filename; do awk 'length > 120 {print FILENAME "(" FNR "): " $$0}' $$filename; done

.PHONY: trailingWhitespace
trailingWhitespace:
	! $(GREP) -E "`printf '[ \\t]$$'`" xxhsum.1 *.c *.h LICENSE Makefile cmake_unofficial/CMakeLists.txt


# =========================================================
# make install is validated only for the following targets
# =========================================================
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS))

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
install: lib xxhsum  ## install libraries, CLI, links and man page
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
	@ln -sf xxhsum $(DESTDIR)$(BINDIR)/xxh64sum
	@ln -sf xxhsum $(DESTDIR)$(BINDIR)/xxh128sum
	@echo Installing man pages
	@$(INSTALL_DATA) xxhsum.1 $(DESTDIR)$(MANDIR)/xxhsum.1
	@ln -sf xxhsum.1 $(DESTDIR)$(MANDIR)/xxh32sum.1
	@ln -sf xxhsum.1 $(DESTDIR)$(MANDIR)/xxh64sum.1
	@ln -sf xxhsum.1 $(DESTDIR)$(MANDIR)/xxh128sum.1
	@echo xxhash installation completed

.PHONY: uninstall
uninstall:  ## uninstall libraries, CLI, links and man page
	@$(RM) $(DESTDIR)$(LIBDIR)/libxxhash.a
	@$(RM) $(DESTDIR)$(LIBDIR)/libxxhash.$(SHARED_EXT)
	@$(RM) $(DESTDIR)$(LIBDIR)/libxxhash.$(SHARED_EXT_MAJOR)
	@$(RM) $(DESTDIR)$(LIBDIR)/$(LIBXXH)
	@$(RM) $(DESTDIR)$(INCLUDEDIR)/xxhash.h
	@$(RM) $(DESTDIR)$(BINDIR)/xxh32sum
	@$(RM) $(DESTDIR)$(BINDIR)/xxh64sum
	@$(RM) $(DESTDIR)$(BINDIR)/xxh128sum
	@$(RM) $(DESTDIR)$(BINDIR)/xxhsum
	@$(RM) $(DESTDIR)$(MANDIR)/xxh32sum.1
	@$(RM) $(DESTDIR)$(MANDIR)/xxh64sum.1
	@$(RM) $(DESTDIR)$(MANDIR)/xxh128sum.1
	@$(RM) $(DESTDIR)$(MANDIR)/xxhsum.1
	@echo xxhsum successfully uninstalled

endif
