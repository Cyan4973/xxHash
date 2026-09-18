// The "Try it" box: text, files, both file-reading paths, and the size limit.
// Expected digests are the format specification's, so a mismatch means the page
// is wrong -- never that the vectors need updating.

const {
  render, until, choose, suite, sample,
  TEXT_VECTORS, EMPTY_XXH3, SAMPLE_SIZE, SAMPLE_VECTORS,
} = require("./harness");

const ALGOS = Object.keys(TEXT_VECTORS);

async function hashFile(t, label, bytes, blobStream) {
  const page = render({ blobStream });
  const { document: d } = page;
  const subject = () => d.getElementById("subject").textContent.replace(/\s+/g, " ").trim();

  await until(() => d.querySelector('[data-h="xxhash3"]').textContent !== "—");
  choose(page, "sample.bin", bytes);
  const done = await until(() => /10\.0 MB/.test(subject()));

  t.is(label + ": finished", done, "true");
  for (const a of ALGOS) t.is(label + ": " + a, d.querySelector('[data-h="' + a + '"]').textContent, SAMPLE_VECTORS[a]);
  t.is(label + ": caption", subject(), "sample.bin — 10.0 MB");
  await page.close();
}

module.exports = async function () {
  const t = suite("hashing");
  const bytes = sample(SAMPLE_SIZE);

  t.section("text, against the spec's vectors");
  {
    const page = render();
    const { window: w, document: d } = page;
    await until(() => d.querySelector('[data-h="xxhash3"]').textContent !== "—");
    for (const a of ALGOS) t.is(a, d.querySelector('[data-h="' + a + '"]').textContent, TEXT_VECTORS[a]);

    const input = d.getElementById("input");
    input.value = "";
    input.dispatchEvent(new w.Event("input"));
    await until(() => d.querySelector('[data-h="xxhash3"]').textContent === EMPTY_XXH3);
    t.is("empty string", d.querySelector('[data-h="xxhash3"]').textContent, EMPTY_XXH3);
    await page.close();
  }

  t.section("files, both read paths, 10 MB spanning several chunks");
  await hashFile(t, "stream", bytes, true);
  await hashFile(t, "slice ", bytes, false);

  t.section("edge cases");
  {
    const page = render({ blobStream: false });
    const { document: d } = page;
    const subject = () => d.getElementById("subject").textContent.replace(/\s+/g, " ").trim();
    await until(() => d.querySelector('[data-h="xxhash3"]').textContent !== "—");

    choose(page, "empty.bin", Buffer.alloc(0));
    // must match the finished caption, not the "reading ..." message
    await until(() => /^empty\.bin — /.test(subject()));
    t.is("empty file caption", subject(), "empty.bin — 0 bytes");
    t.is("empty file is valid input", d.querySelector('[data-h="xxhash3"]').textContent, EMPTY_XXH3);

    // a file over the limit must be refused without a single byte being read
    const huge = {
      name: "huge.iso",
      size: 900 * 1024 * 1024,
      slice: () => { throw new Error("must not read an oversized file"); },
      stream: () => { throw new Error("must not read an oversized file"); },
    };
    const picker = d.getElementById("file");
    Object.defineProperty(picker, "files", { value: [huge], configurable: true });
    picker.dispatchEvent(new page.window.Event("change"));
    await until(() => /huge\.iso/.test(subject()));

    t.ok("oversized states the limit", /stops at 512\.0 MB/.test(subject()));
    t.ok("oversized states the file size", /huge\.iso is 900\.0 MB/.test(subject()));
    t.ok("oversized points at xxhsum", /xxhsum/.test(subject()));
    {
      // the refusal is the moment the CLI becomes the answer, so name it and
      // say where to read about it, rather than leaving the reader to search
      const manual = d.querySelector("#subject a");
      t.is("and links its manual", manual && manual.getAttribute("href"),
        "https://github.com/Cyan4973/xxHash/blob/release/cli/xxhsum.1.md");
      t.is("on the word itself", manual && manual.textContent.trim(), "xxhsum");
    }
    t.ok("oversized flagged as an error", d.getElementById("subject").classList.contains("error"));
    t.is("oversized clears stale digests", d.querySelector('[data-h="xxhash3"]').textContent, "—");
    await page.close();
  }

  t.section("the box makes no speed claim");
  {
    const page = render({ blobStream: false });
    const { document: d } = page;
    await until(() => d.querySelector('[data-h="xxhash3"]').textContent !== "—");
    choose(page, "sample.bin", bytes);
    await until(() => /10\.0 MB/.test(d.getElementById("subject").textContent));
    const caption = d.getElementById("subject").textContent;
    // browser wasm is ~10x slower than the native library, so any throughput
    // printed here would contradict the benchmark section
    t.ok("no GB/s or MB/s", !/[GM]B\/s/.test(caption));
    t.ok("no millisecond timings", !/\d+\s*ms/.test(caption));
    t.is("no in-page speed test button", d.querySelectorAll("#benchgo").length, 0);
    await page.close();
  }

  return t.report();
};
