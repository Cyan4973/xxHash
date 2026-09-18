// The doc/ links used to name a release in three places. build.js now derives
// the version from what is actually on disk, so these cover the picker -- above
// all the ordering, which stays invisibly wrong until the release that trips it.

const fs = require("fs");
const os = require("os");
const path = require("path");
const { suite, ROOT } = require("./harness");
const { latestDocVersion } = require("../build.js");

// Builds a throwaway doc/ tree. A name suffixed with "!" gets no index.html.
function tree(...names) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "xxh-doc-"));
  for (const name of names) {
    const empty = name.endsWith("!");
    const sub = path.join(dir, empty ? name.slice(0, -1) : name);
    fs.mkdirSync(sub);
    if (!empty) fs.writeFileSync(path.join(sub, "index.html"), "");
  }
  return dir;
}

const pick = (...names) => {
  const dir = tree(...names);
  try { return latestDocVersion(dir); } finally { fs.rmSync(dir, { recursive: true, force: true }); }
};

const throws = (...names) => {
  try { pick(...names); return false; } catch (e) { return true; }
};

module.exports = async function () {
  const t = suite("doc version picker");

  t.section("newest wins, counting numerically");
  t.is("plain case", pick("v0.8.2", "v0.8.3"), "v0.8.3");
  t.is("order on disk is irrelevant", pick("v0.8.3", "v0.8.2", "v0.7.0"), "v0.8.3");
  // The one a string sort gets backwards: "v0.8.10" < "v0.8.9" alphabetically.
  t.is("two-digit patch", pick("v0.8.9", "v0.8.10"), "v0.8.10");
  t.is("two-digit minor", pick("v0.9.0", "v0.10.0"), "v0.10.0");
  t.is("new major", pick("v0.9.9", "v1.0.0"), "v1.0.0");
  t.is("shorter is the same as trailing zeros", pick("v1", "v1.0.1"), "v1.0.1");

  t.section("a link has to land somewhere");
  t.is("skips a version with no index.html", pick("v0.8.2", "v0.9.0!"), "v0.8.2");
  t.is("ignores directories that are not versions", pick("v0.8.2", "images", "latest"), "v0.8.2");
  t.ok("throws when nothing qualifies", throws("images", "v0.9.0!"));
  t.ok("throws on an empty doc/", throws());

  t.section("against the real doc/ directory");
  const real = latestDocVersion(path.join(ROOT, "doc"));
  t.ok("names a release", /^v\d+(\.\d+)*$/.test(real));
  t.ok("that page exists", fs.existsSync(path.join(ROOT, "doc", real, "index.html")));

  return t.report();
};
