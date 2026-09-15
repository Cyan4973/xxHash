# xxHash homepage data

Edit the tables below and re-run `node build.js`. Prose and layout live in
`src/template.html`; this file is only the lists that change often.

## variants

All four are supported. Keep the recommended default first.

|Variant           |Output |Notes
|----------------- |------ |--
|`XXH3_64bits`     |64-bit |Fastest, and the strongest on small inputs. The recommended default.
|`XXH3_128bits`    |128-bit|Same speed as `XXH3_64bits`, with a wider output.
|`XXH64`           |64-bit |Fast on 64-bit platforms, and widely deployed.
|`XXH32`           |32-bit |Fastest of the 32-bit hashes, and favoured on 32-bit targets.

## bandwidth

Single-threaded, 100 KB input (cache resident). Bars are scaled per platform by
the build script, so only the numbers need maintaining. Put `reference` in the
last column for rows that are not an xxHash measurement.

|Platform       |Variant             |Bandwidth |Kind
|-------------- |------------------- |--------- |--
|Intel i7-9700K |__XXH3__ (AVX2)     |59.4 GB/s |
|Intel i7-9700K |__XXH128__ (AVX2)   |57.9 GB/s |
|Intel i7-9700K |__XXH3__ (SSE2)     |31.5 GB/s |
|Intel i7-9700K |__XXH128__ (SSE2)   |29.6 GB/s |
|Intel i7-9700K |_memcpy_, from RAM  |28.0 GB/s |reference
|Intel i7-9700K |__XXH64__           |19.4 GB/s |
|Intel i7-9700K |__XXH32__           |9.7 GB/s  |
|Apple M1 Pro   |_memcpy_, from RAM  |45 GB/s   |reference
|Apple M1 Pro   |__XXH3__ (NEON)     |36.3 GB/s |
|Apple M1 Pro   |__XXH128__ (NEON)   |35.1 GB/s |
|Apple M1 Pro   |__XXH64__           |13.4 GB/s |
|Apple M1 Pro   |__XXH32__           |6.6 GB/s  |
|AMD Zen 5      |__XXH3__ (AVX512)   |159 GB/s  |
|AMD Zen 5      |__XXH128__ (AVX512) |156 GB/s  |
|AMD Zen 5      |__XXH3__ (AVX2)     |84.0 GB/s |
|AMD Zen 5      |__XXH128__ (AVX2)   |83.5 GB/s |
|AMD Zen 5      |__XXH3__ (SSE2)     |38.9 GB/s |
|AMD Zen 5      |__XXH128__ (SSE2)   |37.4 GB/s |
|AMD Zen 5      |_memcpy_, from RAM  |28.5 GB/s |reference
|AMD Zen 5      |__XXH64__           |27.8 GB/s |
|AMD Zen 5      |__XXH32__           |13.8 GB/s |

## comparison

Intel i7-9700K, clang v10.0 -O3. Bold the name to mark it as one of ours.

|Hash              |Width |Bandwidth |Small data |Note
|----------------- |----- |--------- |---------- |--
|__XXH3__ (AVX2)   |64    |59.4 GB/s |133.1      |AVX2 support (optional)
|__XXH128__ (AVX2) |128   |57.9 GB/s |118.1      |AVX2 support (optional)
|__XXH3__ (SSE2)   |64    |31.5 GB/s |133.1      |
|__XXH128__ (SSE2) |128   |29.6 GB/s |118.1      |
|_memcpy_          |—     |28.0 GB/s |—          |_from RAM, for reference_
|City64            |64    |22.0 GB/s |76.6       |
|T1ha2             |64    |22.0 GB/s |99.0       |Slightly worse [collisions](https://github.com/Cyan4973/xxHash/wiki/Collision-ratio-comparison#collision-study)
|City128           |128   |21.7 GB/s |57.7       |
|__XXH64__         |64    |19.4 GB/s |71.0       |
|SpookyHash        |64    |19.3 GB/s |53.2       |
|Mum               |64    |18.0 GB/s |67.0       |Slightly worse [collisions](https://github.com/Cyan4973/xxHash/wiki/Collision-ratio-comparison#collision-study)
|__XXH32__         |32    |9.7 GB/s  |71.9       |
|City32            |32    |9.1 GB/s  |66.0       |
|Murmur3           |32    |3.9 GB/s  |56.1       |
|SipHash           |64    |3.0 GB/s  |43.2       |
|FNV64             |64    |1.2 GB/s  |62.7       |Poor avalanche properties
|Blake2            |256   |1.1 GB/s  |5.1        |Cryptographic
|SHA1              |160   |0.8 GB/s  |5.6        |Cryptographic but broken
|MD5               |128   |0.6 GB/s  |7.8        |Cryptographic but broken

## implementations

Sorted alphabetically on the language label as written. A language's own
variants keep their relative order.

|Language                   |Author              |URL
|-------------------------- |------------------- |--
|__C__ multithreaded        |Shawn Bayern        |https://github.com/shawnbayern/xxHash
|__C#__ (std pkg)           |Microsoft           |https://learn.microsoft.com/en-us/dotnet/api/system.io.hashing?view=net-9.0-pp
|__C#__ (port)              |Melnik Alexander    |https://github.com/uranium62/xxHash
|__C#__ (.net std 2.0)      |Sedat Kapanoğlu     |https://github.com/ssg/HashDepot#xxhash
|__C++__ (simple)           |Stefan Brumme       |https://create.stephan-brumme.com/xxhash/
|__C++__ constexpr (XXH64)  |Daniel Kirchner     |https://github.com/ekpyron/xxhashct
|__C++__ constexpr (XXH32)  |Takayuki Matsuoka   |https://github.com/Cyan4973/xxHash/issues/496
|__C++__ constexpr (XXH3)   |chys87              |https://github.com/chys87/constexpr-xxh3
|__C++ 17__                 |Red Gavin           |https://github.com/RedSpah/xxhash_cpp
|__Crystal__                |Lucjan Suski        |https://github.com/methyl/xxhash
|__D__                      |Masahiro Nakagawa   |https://github.com/repeatedly/xxhash-d
|__Dart__ (XXH3)            |SamJakob            |https://pub.dev/packages/xxh3
|__Elixir__ (nif)           |Ali Farhadi         |https://github.com/farhadi/xxh3
|__Elixir__ (port)          |Mykola Konyk        |https://github.com/ttvd/elixir-xxhash
|__Erlang__                 |Pierre Matri        |https://github.com/pierresforge/erlang-xxhash
|__Erlang__ (XXH3)          |Ali Farhadi         |https://github.com/farhadi/xxh3
|__Go__ (XXH64)             |Ahmed Waheed        |https://github.com/OneOfOne/xxhash
|__Go__ (XXH3)              |Jeff Wendling       |https://github.com/zeebo/xxh3
|__Go + ASM__               |Caleb Spare         |https://github.com/cespare/xxhash
|__Haskell__                |Henri Verroken      |http://hackage.haskell.org/package/xxhash-ffi
|__Haskell__ (port)         |Christian Marie     |http://hackage.haskell.org/package/xxhash
|__Java__                   |Adrien Grand        |https://github.com/lz4/lz4-java/tree/master/src/java/net/jpountz/xxhash
|__Java__ (all, port)       |Dynatrace           |https://github.com/dynatrace-oss/hash4j/blob/main/src/main/java/com/dynatrace/hash4j/hashing/XXH3_64.java
|__Java__ (XXH3, XXH128)    |James Z.M. Gao      |https://github.com/OpenHFT/Zero-Allocation-Hashing/blob/master/src/main/java/net/openhft/hashing/XXH3.java
|__JavaScript__ (WebAssembly) |Michael Jungo       |https://www.npmjs.com/package/xxhash-wasm
|__JavaScript__ (port)      |Pierre Curto        |https://npmjs.org/package/xxhashjs
|__JavaScript__ (nodeJS)    |Brian White         |https://npmjs.org/package/xxhash
|__JavaScript__ (nodeJS, xxh3) |Nhan Khong          |https://github.com/ktrongnhan/xxhash-addon
|__JavaScript__ (React Native) |Alex Shumihin       |https://github.com/pioner92/react-native-xxhash
|__JSX__ (static JavaScript) |Yoshiki Shibukawa   |https://www.npmjs.org/package/xxhash.jsx
|__Julia__                  |Hanan Rosemarin     |https://github.com/hros/XXhash.jl
|__Kotlin__                 |Matthew Dolan       |https://github.com/appmattus/crypto/tree/main/cryptohash
|__Lua__ (binding)          |Masatoshi Teruya    |https://github.com/mah0x211/lua-xxhash
|__Lua__ (jit, XXH32)       |szensk              |https://github.com/szensk/luaxxhash
|__Lua__ (jit, XXH64)       |Soojin Nam          |https://github.com/sjnam/luajit-xxHash
|__OCaml__                  |Pieter Goetschalckx |http://opam.ocaml.org/packages/xxhash/
|__Pascal__                 |Vojtěch Čihák       |http://sourceforge.net/projects/xxhashfpc
|__Perl__                   |Sanko Robinson      |https://metacpan.org/module/Digest::xxHash
|__Perl__ (streaming)       |Bela Bodecs         |https://github.com/DoubleBB/digest-xxhash64
|__PHP__                    |Nir Heimann         |https://github.com/nheimann1/php-xxhash
|__PHP__ (port)             |Scott Dutton        |https://github.com/exussum12/xxhash
|__PHP7__                   |Craig R Megasaxon   |https://github.com/Megasaxon/php-xxhash
|__PHP8__                   |Anatol Belski       |https://php.watch/versions/8.1/xxHash
|__PicoLisp__               |mpech               |https://git.envs.net/mpech/xxhash-picolisp
|__Python__                 |Yue Du              |https://pypi.python.org/pypi/xxhash/
|__R__                      |Dirk Eddelbuettel   |https://github.com/eddelbuettel/digest
|__R__ (XXH3)               |mikefc              |https://github.com/coolbutuseless/xxhashlite
|__Ruby__ (port)            |Justin W Smith      |http://rubygems.org/gems/ruby-xxHash
|__Ruby__ (wrapper)         |Vasiliy Ermolovich  |https://rubygems.org/gems/xxhash
|__Ruby__ (digest::class)   |konsolebox          |https://rubygems.org/gems/digest-xxhash
|__Rust__                   |Jake Goulding       |https://libraries.io/cargo/twox-hash
|__Rust__ (const xxh3)      |Arthur Martirosyan  |https://crates.io/crates/xxhash-rust
|__Scala__                  |Desmond Yeung       |https://github.com/desmondyeung/scala-hashing
|__Swift__                  |Daisuke T           |https://github.com/daisuke-t-jp/xxHash-Swift
|__Tcl__ (XXH32, port)      |D. Bohdan           |https://wiki.tcl-lang.org/48790
|__Zig__                    |ziglang             |https://github.com/Cyan4973/xxHash/issues/1001

## shells

|Language                 |Author         |URL
|------------------------ |-------------- |--
|__Bash__ (port)          |Devin Hussey   |https://github.com/easyaspi314/xxbash
|__Batch__ (XXH32, port)  |Antonio Aacini |https://github.com/Aacini/xxHash32
|__sh__ (port)            |Jan Chren      |https://gitlab.com/rindeal/xxHashish
|__x86 assembly__ (XXH32) |Antonio Aacini |https://github.com/Aacini/xxHash32

## used-by

One `###` per category. Logo filenames are relative to `images/logo50/`; leave
the cell empty to fall back to the placeholder. A leading `*` on the project
name also shows that logo in the strip under the hero.

### Systems & Infrastructure

|Project          |Logo        |URL
|---------------- |----------- |--
|*Linux           |linux.png   |https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=59e1a2f4bf83744e748636415fde7d1e9f557e05
|*Microsoft Azure |azure50.png |https://docs.microsoft.com/en-us/azure/kusto/query/hashfunction
|Qemu             |qemu.png    |https://www.qemu.org/
|btrfs            |btrfs.png   |https://btrfs.wiki.kernel.org/index.php/Main_Page
|PKG              |freebsd.png |https://wiki.freebsd.org/pkgng
|Dorado           |huawei.png  |https://forum.huawei.com/enterprise/en/how-to-understand-inline-deduplication-and-compression/thread/604642-891

### Databases

|Project     |Logo           |URL
|----------- |-------------- |--
|PrestoDB    |prestoDB.png   |http://prestodb.io/
|*RocksDB    |rocksdb.png    |https://rocksdb.org/
|*MySQL      |mysql.png      |https://www.mysql.com/
|*ClickHouse |clickhouse.png |https://clickhouse.com/
|ArangoDB    |arangoDB.png   |https://www.arangodb.org/
|PGroonga    |pgroonga.png   |https://pgroonga.github.io/
|Spark       |spark.png      |http://spark.apache.org/
|MariaDB     |mariadb.png    |https://mariadb.org/
|Groonga     |groonga.png    |https://groonga.org/docs/news.html#release-10-0-8

### Games

|Project         |Logo          |URL
|--------------- |------------- |--
|CoD Black Ops   |CoD_BO_CW.png |https://youtu.be/DkspHgt27Io?t=717
|Mafia           |mafiade.png   |https://mafiagame.fandom.com/wiki/Mafia:_Definitive_Edition_Credits
|*Minecraft      |minecraft.png |https://www.minecraft.net
|PPSSPP          |ppsspp.png    |https://ppsspp.org/
|Dolphin         |dolphin.png   |https://dolphin-emu.org/
|Cxbx-reloaded   |cxbx.png      |https://cxbx-reloaded.co.uk
|Cocos2D         |cocos2D.png   |http://www.cocos2d.org/
|LWJGL           |lwjgl.png     |https://www.lwjgl.org/
|O3DE Gem        |heathen.png   |https://codeberg.org/Heathen-Engineering/O3DE-xxHash
|Freecell Solver |              |http://fc-solve.shlomifish.org/

### Filters

|Project                 |Logo        |URL
|----------------------- |----------- |--
|*HAProxy                |haproxy.png |https://www.haproxy.org/
|Rspamd                  |rspamd.png  |https://rspamd.com/
|pfSense                 |pfsense.png |https://www.pfsense.org/
|fio                     |            |http://freecode.com/projects/fio/
|bloomxx                 |            |https://npmjs.org/package/bloomxx/
|C & Python Bloom Filter |            |http://devisedbydavid.com/open_source/bloom_filter
|LUA Bloom Filter        |mozilla.png |https://github.com/mozilla-services/lua_bloom_filter

### File Transfer

|Project     |Logo            |URL
|----------- |--------------- |--
|*Netflix    |netflix.png     |https://partnerhelp.netflixstudios.com/hc/en-us/articles/360000581207-Production-Assets-Data-Management
|*rsync      |rsync.png       |https://rsync.samba.org/
|LZ4         |lz4.png         |http://www.lz4.org/
|Silverstack |silverstack.png |https://pomfort.com/silverstack/
|Rapidcopy   |rapidcopy.png   |http://www.lespace.co.jp/file_bl/rapidcopy/rapidcopy.html
|Hedge       |syncFactory.png |https://www.hedgeformac.com/
|fastcopy    |fastcopy.png    |https://ipmsg.org/tools/fastcopy.html.en
|TeraCopy    |teracopy.png    |https://codesector.com/teracopy
|Hammer 2    |hammer.png      |https://www.dragonflybsd.org/hammer/
|CHK         |chk48.png       |https://compressme.net/

### Other

|Project         |Logo           |URL
|--------------- |-------------- |--
|Visual Studio   |microsoft.png  |https://devblogs.microsoft.com/cppblog/linker-throughput-improvement-in-visual-studio-2019/
|*NSight compute |nvidia.png     |https://docs.nvidia.com/nsight-compute/CopyrightAndLicenses/index.html
|Xpra            |xpra.png       |https://www.xpra.org/
|TeamViewer      |teamviewer.png |http://www.teamviewer.com/
|Factor          |factor.png     |http://factorcode.org/
|nVBio           |nvbio.png      |http://nvlabs.github.io/nvbio/index.html
|Genozip         |genozip.png    |https://genozip.com/
|dvisvgm         |               |http://dvisvgm.de/
|FastBuild       |fastbuild.png  |http://www.fastbuild.org/
|Keypirinha      |keypirinha.png |http://keypirinha.com/
|QuickHash       |quickhash.png  |http://quickhash-gui.org/

