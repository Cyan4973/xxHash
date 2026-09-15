// Shared plumbing for the page tests.
//
// The page is rendered in jsdom with its scripts executed for real, so the
// assertions exercise the same code a browser runs. jsdom validates structure,
// content and computed styles -- it cannot see layout, paint, or anything inside
// an iframe, so a green run does not mean the page *looks* right.

const { JSDOM, ResourceLoader, VirtualConsole } = require("jsdom");
const fs = require("fs");
const path = require("path");

const ROOT = path.join(__dirname, "..");
const PAGE = path.join(ROOT, "preview.html");

/* -------------------------------- vectors -------------------------------- */
// Digests are frozen by the xxHash format specification, so they are safe to
// hard-code. Produced with xxhsum 0.8.3. If one of these ever fails, the page is
// wrong -- do not "fix" the test by updating the vector.

const TEXT = "Hello, xxHash!";
const TEXT_VECTORS = {
  xxhash3: "dea3adc7524fc027",
  xxhash128: "43dea029798676ea45750c54f3dbdc62",
  xxhash64: "197cf9539342bf65",
  xxhash32: "4bbbeba2",
};

const EMPTY_XXH3 = "2d06800538d394c2";

// 10 MB, which spans several read chunks with a ragged tail
const SAMPLE_SIZE = 10 * 1024 * 1024;
const SAMPLE_VECTORS = {
  xxhash3: "0fe669c6b8b8bb7e",
  xxhash128: "4dbd8a7eedf00c0e0fe669c6b8b8bb7e",
  xxhash64: "a716fee65003bda3",
  xxhash32: "b9a4f62a",
};

// xorshift32 in pure uint32 arithmetic: identical bytes on any JS engine
function sample(n) {
  const b = Buffer.alloc(n);
  let x = 0x9e3779b9 >>> 0;
  for (let i = 0; i < n; i++) {
    x ^= x << 13; x >>>= 0;
    x ^= x >>> 17;
    x ^= x << 5;  x >>>= 0;
    b[i] = x & 0xff;
  }
  return b;
}

/* --------------------------------- render --------------------------------- */

/**
 * Render preview.html with scripts running.
 *   opts.fetch      - stub for window.fetch; omit to leave it undefined
 *   opts.blobStream - true to provide Blob.stream(), false to remove it
 *   opts.storage    - object seeded into localStorage before scripts run
 *   opts.origin     - url to serve from (https:// when localStorage is needed)
 *   opts.spy        - true to record XHR / sendBeacon / WebSocket use
 */
function render(opts = {}) {
  const origin = opts.origin || "file://" + PAGE;
  const log = { resources: [], fetches: [], xhr: [], beacons: [], sockets: [], errors: [] };
  const state = { closing: false };

  // Page errors are collected rather than printed, so a suite can assert there
  // were none. Errors raised *after* close() are jsdom tearing down in-flight
  // script loads, not page defects, so they are dropped.
  const virtualConsole = new VirtualConsole();
  virtualConsole.on("jsdomError", (e) => {
    if (!state.closing) log.errors.push(e.message.split("\n")[0]);
  });

  class Loader extends ResourceLoader {
    fetch(url, options) {
      log.resources.push(url);
      if (url.startsWith("file:///")) return super.fetch(url, options);
      // assets are served from disk even when the page pretends to be on https
      const rel = url.replace(/^https?:\/\/[^/]+\//, "");
      const onDisk = path.join(ROOT, rel);
      if (rel && fs.existsSync(onDisk)) return Promise.resolve(fs.readFileSync(onDisk));
      return Promise.resolve(Buffer.from(""));
    }
  }

  const dom = new JSDOM(fs.readFileSync(PAGE, "utf8"), {
    url: origin,
    runScripts: "dangerously",
    resources: new Loader(),
    virtualConsole,
    pretendToBeVisual: true,
    beforeParse(w) {
      // present in every target browser, missing from jsdom
      w.TextEncoder = require("util").TextEncoder;
      w.IntersectionObserver = class {
        constructor(cb) { this.cb = cb; }
        observe() { this.cb([{ isIntersecting: true }]); }
        disconnect() {}
      };

      if (opts.fetch) {
        w.fetch = (u, o) => { log.fetches.push({ url: u, body: o && o.body }); return opts.fetch(u, o); };
      }

      if (opts.blobStream === true) {
        // hands the blob over in 64 KB pieces, as a browser's own stream does
        w.Blob.prototype.stream = function () {
          const bytes = this._testBytes;
          let off = 0;
          return { getReader: () => ({
            read: () => Promise.resolve(
              off >= bytes.length
                ? { done: true }
                : { done: false, value: new Uint8Array(bytes.subarray(off, (off += 65536))) }
            ),
            cancel: () => Promise.resolve(),
          })};
        };
      } else if (opts.blobStream === false) {
        delete w.Blob.prototype.stream;
      }

      if (opts.storage) {
        try {
          for (const [k, v] of Object.entries(opts.storage)) w.localStorage.setItem(k, JSON.stringify(v));
        } catch (e) { /* opaque origin */ }
      }

      if (opts.spy) {
        w.navigator.sendBeacon = (u, d) => { log.beacons.push({ url: u, data: d }); return true; };
        w.WebSocket = function (u) { log.sockets.push(u); };
        const Real = w.XMLHttpRequest;
        w.XMLHttpRequest = function () {
          const x = new Real();
          const open = x.open, send = x.send;
          x.open = function (m, u) { log.xhr.push(u); return open.apply(x, arguments); };
          x.send = function (b) { log.xhr.push({ body: b }); return send.apply(x, arguments); };
          return x;
        };
      }
    },
  });

  const page = { dom, window: dom.window, document: dom.window.document, log };

  // Closing frees the wasm instances between renders. Wait for the lazy module
  // loads to land first, otherwise close() aborts them mid-flight.
  page.close = async () => {
    // let the lazy module loads and any in-flight digest promises land first,
    // otherwise closing tears the document out from under their callbacks
    await until(() => dom.window.hashwasm, { timeout: 5000, every: 25 });
    await wait(150);
    state.closing = true;
    try { dom.window.close(); } catch (e) { /* already gone */ }
  };
  return page;
}

/* ------------------------------- utilities ------------------------------- */

const wait = (ms) => new Promise((r) => setTimeout(r, ms));

// poll until the predicate holds, so tests are not pinned to fixed sleeps
async function until(fn, { timeout = 20000, every = 100 } = {}) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    if (fn()) return true;
    await wait(every);
  }
  return false;
}

// hand a File to the page's <input type=file> the way a picker would
function choose({ window, document }, name, bytes) {
  const file = new window.File([new Uint8Array(bytes)], name);
  file._testBytes = bytes;                    // read by the Blob.stream() stub
  const picker = document.getElementById("file");
  Object.defineProperty(picker, "files", { value: [file], configurable: true });
  picker.dispatchEvent(new window.Event("change"));
  return file;
}

/* ------------------------------- assertions ------------------------------- */

function suite(title) {
  const results = [];
  const api = {
    section(name) { results.push({ section: name }); return api; },
    is(name, got, want) {
      const ok = String(got) === String(want);
      results.push({ name, ok, got: String(got), want: String(want) });
      return api;
    },
    ok(name, value) { return api.is(name, !!value, "true"); },
    report() {
      let pass = 0, fail = 0;
      console.log("\n\x1b[1m" + title + "\x1b[0m");
      for (const r of results) {
        if (r.section) { console.log("  " + r.section); continue; }
        r.ok ? pass++ : fail++;
        console.log(
          (r.ok ? "    \x1b[32mPASS\x1b[0m " : "    \x1b[31mFAIL\x1b[0m ") +
          r.name.padEnd(44) + r.got + (r.ok ? "" : "  \x1b[31m(want " + r.want + ")\x1b[0m")
        );
      }
      return { pass, fail };
    },
  };
  return api;
}

module.exports = {
  ROOT, PAGE, render, wait, until, choose, suite, sample,
  TEXT, TEXT_VECTORS, EMPTY_XXH3, SAMPLE_SIZE, SAMPLE_VECTORS,
};
