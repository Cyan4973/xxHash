# ################################################################
# xxHash benchHash Makefile
# Copyright (C) 2019-2021 Yann Collet
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
# You can contact the author at:
#   - xxHash homepage: https://www.xxhash.com
#   - xxHash source repository: https://github.com/Cyan4973/xxHash
# ################################################################
# benchHash: A generic benchmark for hash algorithms
#            measuring throughput, latency and bandwidth
# ################################################################


CPPFLAGS += -I../..   # directory of xxHash source files
CFLAGS   ?= -O3
CFLAGS   += -Wall -Wextra -Wstrict-aliasing=1 \
            -std=c99
CFLAGS   += $(MOREFLAGS)   # custom way to add flags
CXXFLAGS ?= -O3
LDFLAGS  += $(MOREFLAGS)
UNAME    ?= $(shell uname)
ARCH     ?= $(shell uname -p)


OBJ_LIST  = main.o bhDisplay.o benchHash.o benchfn.o timefn.o
ifeq ($(DISPATCH),1)
  ifeq ($(UNAME), Darwin)
    ifeq ($(ARCH), arm)
      OBJ_LIST += xxh_arm64dispatch.o xxh_arm64sve.o
      CFLAGS   += -DARM64_DISPATCH=1
    endif
  else
    ifeq ($(ARCH), aarch64)
      OBJ_LIST += xxh_arm64dispatch.o xxh_arm64sve.o
      CFLAGS   += -DARM64_DISPATCH=1
    endif
  endif
endif


default: benchHash

all: benchHash

benchHash32: CFLAGS   += -m32
benchHash32: CXXFLAGS += -m32

benchHash_avx2: CFLAGS   += -mavx2
benchHash_avx2: CXXFLAGS += -mavx2

benchHash_hw: CPPFLAGS += -DHARDWARE_SUPPORT
benchHash_hw: CFLAGS   += -mavx2 -maes
benchHash_hw: CXXFLAGS += -mavx2 -mpclmul -std=c++14

benchHash benchHash32 benchHash_avx2 benchHash_nosimd benchHash_hw: $(OBJ_LIST)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) -o $@


main.o: bhDisplay.h hashes.h

bhDisplay.o: bhDisplay.h benchHash.h

benchHash.o: benchHash.h

ifeq ($(DISPATCH),1)
  ifeq ($(UNAME), Darwin)
    ifeq ($(ARCH), arm)
      xxh_arm64dispatch.o: ../../xxh_arm64dispatch.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o xxh_arm64dispatch.o -c ../../xxh_arm64dispatch.c
      xxh_arm64sve.o: ../../xxh_arm64sve.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -o xxh_arm64sve.o -c ../../xxh_arm64sve.S
    endif
  else
    ifeq ($(ARCH), aarch64)
      xxh_arm64dispatch.o: ../../xxh_arm64dispatch.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o xxh_arm64dispatch.o -c ../../xxh_arm64dispatch.c
      xxh_arm64sve.o: ../../xxh_arm64sve.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -o xxh_arm64sve.o -c ../../xxh_arm64sve.S
    endif
  endif
endif


clean:
	$(RM) *.o benchHash benchHash32 benchHash_avx2 benchHash_hw
