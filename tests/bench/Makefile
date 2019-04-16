

CPPFLAGS += -I../..
CFLAGS   ?= -O3
CFLAGS   += -std=c99 -Wall -Wextra -Wstrict-aliasing=1
CXXFLAGS ?= -O3


OBJ_LIST  = main.o bhDisplay.o benchHash.o benchfn.o timefn.o


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


clean:
	$(RM) *.o benchHash benchHash32 benchHash_avx2 benchHash_hw
