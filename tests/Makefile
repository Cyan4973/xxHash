CFLAGS += -Wall -Wextra -g

NM = nm
GREP = grep

.PHONY: default
default: all

.PHONY: all
all: test

.PHONY: test
test: test_multiinclude

.PHONY: test_multiinclude
test_multiinclude:
	@$(MAKE) clean
	# compile without xxhash.o, ensure symbols exist within target
	# note : built using only default rules
	$(MAKE) multiInclude
	@$(MAKE) clean
	# compile with xxhash.o, to detect duplicated symbols
	$(MAKE) multiInclude_withxxhash
	@$(MAKE) clean
	# Note : XXH_INLINE_ALL with XXH_NAMESPACE is currently disabled
	# compile with XXH_NAMESPACE
	# CPPFLAGS=-DXXH_NAMESPACE=TESTN_ $(MAKE) multiInclude_withxxhash
	# no symbol prefixed TESTN_ should exist
	# ! $(NM) multiInclude_withxxhash | $(GREP) TESTN_
	#$(MAKE) clean
	# compile with XXH_NAMESPACE and without xxhash.o
	# CPPFLAGS=-DXXH_NAMESPACE=TESTN_ $(MAKE) multiInclude
	# no symbol prefixed TESTN_ should exist
	# ! $(NM) multiInclude | $(GREP) TESTN_
	#@$(MAKE) clean

xxhash.o: ../xxhash.c ../xxhash.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -c -o $@ $<

multiInclude_withxxhash: multiInclude.o xxhash.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^

clean:
	@$(RM) *.o
	@$(RM) multiInclude multiInclude_withxxhash
