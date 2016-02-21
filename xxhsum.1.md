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
md5sum(1).  Differences are: `xxhsum` doesn't have text/binary mode switch
(`-b`, `-t`);  `xxhsum` always treats file as binary file;  `xxhsum` doesn't
have short option switch for warning (`-w`).  `xxhsum` has hash bit width
switch (`-H`);

OPTIONS
-------

* `-c`, `--check`:
  Read xxHash sums from the <FILE>s and check them

* `-H`<HASHTYPE>:
  Hash selection.  <HASHTYPE> means `0`=32bits, `1`=64bits.
  Default value is `1` (64bits)

**The following four options are useful only when verifying checksums (`-c`)**

* `--quiet`:
  Exit non-zero for improperly formatted checksum lines

* `--strict`:
  Don't print OK for each successfully verified file

* `--status`:
  Don't output anything, status code shows success

* `--warn`:
  Warn about improperly formatted checksum lines

EXIT STATUS
-----------

`xxhsum` exit `0` on success, `1` if at least one file couldn't be read or
doesn't have the same checksum as the `-c` option.

EXAMPLES
--------

Output xxHash (64bit) checksum values of specific files to standard output

    $ xxhsum -H1 foo bar baz

Output xxHash (32bit) checksum values of specific files to standard output,
and redirect it to `xyz.xxhash`

    $ xxhsum -H0 foo bar baz > xyz.xxhash

Read xxHash sums from specific file and check them

    $ xxhsum -c xyz.xxhash

BUGS
----

Report bugs at: https://github.com/Cyan4973/xxHash/issues/

AUTHOR
------

Yann Collet
