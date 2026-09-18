#!/usr/bin/env node
//
//   node tests/run.js            run every suite
//   node tests/run.js hashing    run the suites whose name contains "hashing"
//
// Tests the *generated* index.html, so run `node build.js` first if you have
// edited anything under src/. Requires jsdom: `npm install`.

const fs = require("fs");
const path = require("path");

const SUITES = ["build", "page", "hashing", "star", "local"];

// A closed jsdom window can still have callbacks queued against its document.
// Those surface here as unhandled rejections and are teardown noise, not page
// defects -- real page errors are collected per-render in harness.js `log.errors`.
process.on("unhandledRejection", (e) => {
  const msg = (e && e.message) || String(e);
  if (/Cannot read properties of (undefined|null)/.test(msg)) return;
  console.error("unhandled rejection:", e);
  process.exit(2);
});

(async () => {
  if (!fs.existsSync(path.join(__dirname, "..", "index.html"))) {
    console.error("index.html is missing -- run `node build.js` first.");
    process.exit(2);
  }
  try {
    require.resolve("jsdom");
  } catch (e) {
    console.error("jsdom is not installed -- run `npm install`.");
    process.exit(2);
  }

  const wanted = process.argv.slice(2);
  const chosen = wanted.length
    ? SUITES.filter((s) => wanted.some((w) => s.includes(w)))
    : SUITES;
  if (!chosen.length) {
    console.error("no suite matches " + wanted.join(", ") + "; known: " + SUITES.join(", "));
    process.exit(2);
  }

  const started = Date.now();
  let pass = 0, fail = 0;
  for (const name of chosen) {
    const result = await require("./" + name + ".test.js")();
    pass += result.pass;
    fail += result.fail;
  }

  const secs = ((Date.now() - started) / 1000).toFixed(1);
  console.log(
    "\n" + (fail ? "\x1b[31m" : "\x1b[32m") +
    pass + " passed, " + fail + " failed\x1b[0m" +
    "  (" + chosen.length + " suites, " + secs + "s)"
  );
  console.log("\x1b[2mjsdom checks structure, content and computed styles -- not layout or paint.\x1b[0m");
  process.exit(fail ? 1 : 0);
})().catch((e) => {
  console.error(e);
  process.exit(2);
});
