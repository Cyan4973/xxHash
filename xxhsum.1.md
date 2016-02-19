xxhsum(1) -- print or check xxHash checksums
============================================

SYNOPSIS
--------

`xxhsum` [<OPTION>] ... [<FILE>] ...

DESCRIPTION
-----------

Print or check xxHash (32 or 64bit) checksums.  When <FILE> is `-`, read
standard input.

`xxhsum` supports a command line syntax similar but not indentical to
md5sum(1).  Differences are: `xxhsum` doesn't have text/binary mode switch;
`xxhsum` always treats file as binary file;  `xxhsum` have hash bit width
switch (`-H`).

OPTIONS
-------

* `-c`, `--check`:
  Read xxHash sums from the <FILE>s and check them

* `-H`<HASHTYPE>:
  Hash selection.  <HASHTYPE> means `0`=32bits, `1`=64bits.
  Default value is `1` (64bits)

The following four options are useful only when verifying checksums (`-c`)

* `--strict`:
  Don't print OK for each successfully verified file

* `--status`:
  Don't output anything, status code shows success

* `--quiet`:
  Exit non-zero for improperly formatted checksum lines

* `--warn`:
  Warn about improperly formatted checksum lines

BUGS
----

Report bugs at: https://github.com/Cyan4973/xxHash/issues/

AUTHOR
------

Yann Collet
