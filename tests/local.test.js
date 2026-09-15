// The page promises that whatever you hash stays on your machine. This watches
// every way a page can send data out while a file is hashed, and requires that
// none of them is used.
//
// The structural argument is stronger than the count: jsdom has no network, so
// a digest that still comes out correct can only have been computed locally.

const { render, until, choose, suite, sample, SAMPLE_SIZE, SAMPLE_VECTORS } = require("./harness");

module.exports = async function () {
  const t = suite("nothing leaves the browser");

  const page = render({ spy: true, blobStream: false, fetch: () => Promise.reject(new Error("blocked")) });
  const { document: d, log } = page;
  await until(() => d.querySelector('[data-h="xxhash3"]').textContent !== "—");

  const before = { resources: log.resources.length, fetches: log.fetches.length };

  choose(page, "private.bin", sample(SAMPLE_SIZE));
  await until(() => /10\.0 MB/.test(d.getElementById("subject").textContent));

  t.section("during the file hash");
  t.is("resource loads", log.resources.length - before.resources, 0);
  t.is("fetch() calls", log.fetches.length - before.fetches, 0);
  t.is("XMLHttpRequest", log.xhr.length, 0);
  t.is("sendBeacon", log.beacons.length, 0);
  t.is("WebSocket", log.sockets.length, 0);
  t.is("forms in the document", d.querySelectorAll("form").length, 0);

  t.section("across the whole page lifetime");
  t.is("off-origin resources", log.resources.filter((u) => !u.startsWith("file:///")).length, 0);
  t.is("fetch targets", log.fetches.map((f) => f.url).join(","), "https://api.github.com/repos/Cyan4973/xxHash");
  t.is("requests carrying a body", log.fetches.filter((f) => f.body).length, 0);

  t.section("and the answer is still right, with no network available");
  t.is("XXH3", d.querySelector('[data-h="xxhash3"]').textContent, SAMPLE_VECTORS.xxhash3);

  await page.close();
  return t.report();
};
