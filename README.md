xxHash - Extremely fast hash algorithm
======================================

xxHash is an Extremely fast Hash algorithm, running at RAM speed limits.
It successfully passes the [SMHasher](http://code.google.com/p/smhasher/wiki/SMHasher) Test suite evaluating Hash quality.

|Branch      |Status   |
|------------|---------|
|master      | [![Build Status](https://travis-ci.org/Cyan4973/xxHash.svg?branch=master)](https://travis-ci.org/Cyan4973/xxHash?branch=master) |
|dev         | [![Build Status](https://travis-ci.org/Cyan4973/xxHash.svg?branch=dev)](https://travis-ci.org/Cyan4973/xxHash?branch=dev) |


Benchmarks
-------------------------

The benchmark uses SMHasher speed test, compiled with Visual on a Windows Seven 32 bits system.
The reference system uses a Core 2 Duo @3GHz

<table>
  <tr>
    <th>Name</th><th>Speed</th><th>Q.Score</th><th>Author</th>
  </tr>
  <tr>
    <th>xxHash</th><th>5.4 GB/s</th><th>10</th><th>Y.C.</th>
  </tr>
  <tr>
    <th>MumurHash 3a</th><th>2.7 GB/s</th><th>10</th><th>Austin Appleby</th>
  </tr>
  <tr>
    <th>SBox</th><th>1.4 GB/s</th><th>9</th><th>Bret Mulvey</th>
  </tr>
  <tr>
    <th>Lookup3</th><th>1.2 GB/s</th><th>9</th><th>Bob Jenkins</th>
  </tr>
  <tr>
    <th>CityHash64</th><th>1.05 GB/s</th><th>10</th><th>Pike & Alakuijala</th>
  </tr>
  <tr>
    <th>FNV</th><th>0.55 GB/s</th><th>5</th><th>Fowler, Noll, Vo</th>
  </tr>
  <tr>
    <th>CRC32</th><th>0.43 GB/s</th><th>9</th><th></th>
  </tr>
  <tr>
    <th>SipHash</th><th>0.34 GB/s</th><th>10</th><th>Jean-Philippe Aumasson</th>
  </tr>
  <tr>
    <th>MD5-32</th><th>0.33 GB/s</th><th>10</th><th>Ronald L. Rivest</th>
  </tr>
  <tr>
    <th>SHA1-32</th><th>0.28 GB/s</th><th>10</th><th></th>
  </tr>
</table>


Q.Score is a measure of quality of the hash function.
It depends on successfully passing SMHasher test set.
10 is a perfect score.

A new version, XXH64, has been created thanks to Mathias Westerdahl contribution, which offers superior speed and dispersion for 64-bits systems. Note however that 32-bits applications will still run faster using the 32-bits version.

SMHasher speed test, compiled using GCC 4.8.2, a Linux Mint 64-bits.
The reference system uses a Core i5-3340M @2.7GHz

| Version    | Speed on 64-bits | Speed on 32-bits |
|------------|------------------|------------------|
| XXH64      | 13.8 GB/s        |  1.9 GB/s        |
| XXH32      |  6.8 GB/s        |  6.0 GB/s        |


This is an official mirror of xxHash project, [hosted on Google Code](http://code.google.com/p/xxhash/).
The intention is to offer github's capabilities to xxhash users, such as cloning, branch, pull requests or source download.

The "master" branch will reflect, the status of xxhash at its official homepage. The "dev" branch is the one where all contributions will be merged. If you plan to propose a patch, please commit into the "dev" branch. Direct commit to "master" are not permitted. Feature branches will also exist, typically to introduce new requirements, and be temporarily available for testing before merge into "dev" branch.
