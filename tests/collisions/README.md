
__collisionsTest__ is a brute force hash analyzer
which will measure a 64-bit hash algorithm's collision rate
by generating billions of hashes,
and comparing the result to an "ideal" target.

The test requires a very large amount of memory.
By default, it will generate 24 billion of 64-bit hashes,
requiring 192 GB of RAM for their storage.
The number of hashes can be modified using command `--nbh=`,
but beware that requiring too few hashes will not provide meaningful information on the algorithm's collision performance.

To reduce RAM usage, an optional filter can be requested, with `--filter`.
It reduces the nb of candidates to analyze, hence associated RAM budget.
Be aware that the filter also requires RAM
(32 GB by default, can be modified using `--filterlog=`,
a too small filter will not be efficient, aim at ~2 bytes per hash),
and that managing the filter costs a significant CPU budget.

The RAM budget will be completed by a list of candidates,
which will be a fraction of original hash list.
Using default settings (24 billions hashes, 32 GB filter),
the number of potential candidates should be reduced to less than 2 billions,
requiring ~14 GB for their storage.
Such a result also depends on hash algorithm's efficiency.
The number of effective candidates is likely to be lower, at ~ 1 billion,
but storage must allocate an upper bound.

For the default test, the expected "optimal" collision rate for a 64-bit hash function is ~18 collisions.

Here are a few results produced with this tester :

| Name        | nb Collisions | Notes |
| ---         | ---           | ---   |
| XXH3_64bits |   |   |
| XXH64       |   |   |
| XXH32       |   |   |
| badsum      |   |   |
