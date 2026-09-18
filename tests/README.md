# Tests

```sh
npm install          # once: pulls in jsdom
npm test             # node build.js && node tests/run.js
node tests/run.js    # tests whatever index.html currently is
node tests/run.js star hashing    # only some suites
```

Most suites render the generated `index.html` in jsdom **with its scripts
running**, so they exercise the same code a browser does. `build` is the
exception: it calls into `build.js` directly, with no page involved.

| suite | covers |
| --- | --- |
| `build` | picking the newest `doc/vX.Y.Z/` the three API-docs links point at |
| `page` | structure and content, the platform selector, the implementation filter |
| `hashing` | digests for text and files, both file-reading paths, the size limit |
| `star` | the GitHub star count and every way it can fail |
| `local` | that nothing you hash leaves the browser |

## What this can and cannot tell you

jsdom checks the DOM, text content and **computed styles**. It cannot see
layout, paint, fonts, or anything inside an iframe. A green run means the data
is right and the handlers fire — it does not mean the page looks right. Use the
preview server for that:

```sh
python3 -m http.server 8321      # then browse to index.html
```

Three bugs that reached a browser before a test caught them, as a reminder of
where the blind spots are: a markdown comment that swallowed a 57-row table, a
platform selector that showed all three charts at once, and a star button that
rendered without its count.

## Two rules

**Digests are ground truth.** The expected hashes in `harness.js` come from the
xxHash format specification, cross-checked against `xxhsum 0.8.3`. They are
frozen for all time. If one fails, the page is broken — never update the vector
to match the output.

**Assert what the user sees.** The platform-selector tests read
`getComputedStyle().display`, not the `hidden` property, because `[hidden]`
loses to any author `display` rule that comes later in the stylesheet. Testing
that an attribute was set proves the code ran, not that it worked.

## Regenerating the vectors

`SAMPLE_VECTORS` covers a deterministic 10 MB buffer built by `sample()` in
`harness.js` — big enough to span several read chunks with a ragged tail. To
re-derive them from a local xxHash build:

```sh
node -e 'const {sample,SAMPLE_SIZE} = require("./tests/harness");
         require("fs").writeFileSync("/tmp/vec.bin", sample(SAMPLE_SIZE))'
for h in 0 1 3 2; do path/to/xxhsum -H$h /tmp/vec.bin; done
```

They should come back unchanged. `-H0/1/3/2` are XXH32, XXH64, XXH3 and XXH128.
