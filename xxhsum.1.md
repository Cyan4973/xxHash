xxhsum(1) -- print or check xxHash non-cryptographic checksums
==============================================================

SYNOPSIS
--------

`xxhsum [<OPTION>] ... [<FILE>] ...`  
`xxhsum -b [<OPTION>] ...`

`xxh32sum` is equivalent to `xxhsum -H0`  
`xxh64sum` is equivalent to `xxhsum -H1`  
`xxh128sum` is equivalent to `xxhsum -H2`


DESCRIPTION
-----------

Print or check xxHash (32, 64 or 128 bits) checksums.  When <FILE> is `-`, read
standard input.

`xxhsum` supports a command line syntax similar but not identical to
md5sum(1).  Differences are:
`xxhsum` doesn't have text/binary mode switch (`-b`, `-t`);
`xxhsum` always treats file as binary file;
`xxhsum` has hash bit width switch (`-H`);

As xxHash is a fast non-cryptographic checksum algorithm,
`xxhsum` should not be used for security related purposes.

`xxhsum -b` invokes benchmark mode. See [OPTIONS](#OPTIONS) and [EXAMPLES](#EXAMPLES) for details.

OPTIONS
-------

* `-V`, `--version`:
  Display xxhsum version and exits

* `-H`<HASHTYPE>:
  Hash selection.  <HASHTYPE> means `0`=32bits, `1`=64bits, `2`=128bits.
  Default value is `1` (64bits)

* `-q`, `--quiet`:
  Remove status messages like "Loading ..." written to `stderr` .

* `--little-endian`:
  Set output hexadecimal checksum value as little endian convention.
  By default, value is displayed as big endian.

* `-h`, `--help`:
  Display help and exit

**The following four options are useful only when verifying checksums (`-c`)**

* `-c`, `--check`:
  Read xxHash sums from the <FILE>s and check them

* `q`, `--quiet`:
  Exit non-zero for improperly formatted checksum lines

* `--strict`:
  Don't print OK for each successfully verified file

* `--status`:
  Don't output anything, status code shows success

* `-w`, `--warn`:
  Warn about improperly formatted checksum lines

**The following options are useful only benchmark purpose**

* `-b`:
  Benchmark mode.  See [EXAMPLES](#EXAMPLES) for details.

* `-B`<BLOCKSIZE>:
  Only useful for benchmark mode (`-b`). See [EXAMPLES](#EXAMPLES) for details.
  <BLOCKSIZE> specifies benchmark mode's test data block size in bytes.
  Default value is 102400

* `-i`<ITERATIONS>:
  Only useful for benchmark mode (`-b`). See [EXAMPLES](#EXAMPLES) for details.
  <ITERATIONS> specifies number of iterations in benchmark. Single iteration
  lasts approximately 1000 milliseconds. Default value is 3

EXIT STATUS
-----------

`xxhsum` exit `0` on success, `1` if at least one file couldn't be read or
doesn't have the same checksum as the `-c` option.

EXAMPLES
--------

Output xxHash (64bit) checksum values of specific files to standard output

    $ xxhsum -H1 foo bar baz

Output xxHash (32bit and 64bit) checksum values of specific files to standard
output, and redirect it to `xyz.xxh32` and `qux.xxh64`

    $ xxhsum -H0 foo bar baz > xyz.xxh32
    $ xxhsum -H1 foo bar baz > qux.xxh64

Read xxHash sums from specific files and check them

    $ xxhsum -c xyz.xxh32 qux.xxh64

Benchmark xxHash algorithm for 16384 bytes data in 10 times. `xxhsum`
benchmarks all xxHash variants and output results to standard output.  
First column means algorithm, second column is source data size in bytes,
third column is number of hashes generated per second (throughput),
and finally last column translates speed in mega-bytes per seconds.

    $ xxhsum -b -i10 -B16384

BUGS
----

Report bugs at: https://github.com/Cyan4973/xxHash/issues/

AUTHOR
------

Yann Collet

SEE ALSO
--------

md5sum(1)
