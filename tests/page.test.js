// Structure and content of the generated page, plus the two interactive bits
// that are pure DOM: the platform selector and the implementation filter.

const fs = require("fs");
const path = require("path");
const { render, until, suite, ROOT } = require("./harness");

const TEMPLATE = path.join(ROOT, "src", "template.html");

module.exports = async function () {
  const t = suite("page structure");
  const page = render();
  const { window: w, document: d, log } = page;
  const q = (sel) => d.querySelectorAll(sel).length;

  await until(() => d.querySelector("#variants tbody tr"));

  t.section("no third-party weight");
  t.is("off-origin resource loads", log.resources.filter((u) => !u.startsWith("file:///")).length, 0);
  t.is("iframes", q("iframe"), 0);

  t.section("hero reads as a name, not a pitch");
  t.is("h1", d.querySelector(".hero h1").textContent.trim(), "xxHash");
  t.is("subtitle", d.querySelector(".hero .sub").textContent.trim(), "Hashing faster than RAM speed");
  t.is("no stats bar", q(".stats"), 0);
  t.is("star button", q("a.star-btn"), 1);
  // People do reach for XXH3 where they need a cryptographic hash. The word is
  // in the subtitle either way; the link is what makes it read as a warning.
  {
    const a = d.querySelector(".hero .tag a");
    t.is("'non-cryptographic' is a link", a && a.textContent.trim(), "non-cryptographic");
    t.is("pointing at the definition", a && a.getAttribute("href"),
      "https://en.wikipedia.org/wiki/Non-cryptographic_hash_function");
    t.ok("underlined, not colour alone",
      /underline/.test(w.getComputedStyle(a).textDecoration || w.getComputedStyle(a).textDecorationLine));
  }

  t.section("content carried over from index.html");
  t.is("variant rows", q("#variants tbody tr"), 4);
  t.is("default variant first", d.querySelector("#variants tbody td").textContent.trim(), "XXH3_64bits");
  t.is("implementations", q("#impl tbody tr:not(.grouphead)"), 60);
  t.is("shells/assembly heading", q("#impl .grouphead"), 1);
  t.is("used-by categories", q(".usedby h3"), 6);
  t.is("used-by entries", q(".usedby .grid a"), 53);
  t.is("trust strip logos", q(".trust img"), 10);
  t.is("comparison rows", q("details.compare tbody tr"), 19);
  t.ok("quality column dropped", !/quality/i.test(d.querySelector("details.compare").textContent));
  t.ok("no 'legacy' framing", !/legacy/i.test(d.body.textContent));
  t.is("images without alt", q(".usedby img:not([alt])") + q(".trust img:not([alt])"), 0);

  t.section("counts in the prose are generated, not typed");
  t.ok("implementations count", /60 ports and bindings/.test(d.querySelector("#other-languages .lede").textContent));
  t.ok("used-by count", /^53 projects/.test(d.querySelector("#references .lede").textContent.trim()));

  t.section("link policy: usable things come from the release branch");
  {
    const hrefs = [...d.querySelectorAll("a[href]")].map((a) => a.getAttribute("href"));
    const refs = hrefs
      .map((h) => /\/xxHash\/(?:blob|tree)\/([^/#?]+)/.exec(h))
      .filter(Boolean)
      .map((m) => m[1]);
    t.ok("some links do reach into the source", refs.length >= 3);
    t.is("all of them on one branch", [...new Set(refs)].join(","), "release");
    t.is("nothing points into dev", hrefs.filter((h) => /\/dev\//.test(h)).length, 0);
    t.ok("no branch pinned to a version number", !refs.some((r) => /^v?\d/.test(r)));
    // the counterpart: the project itself is not a released artifact
    t.ok("source link stays on the default branch",
      hrefs.includes("https://github.com/Cyan4973/xxHash"));
  }

  t.section("the top bar keeps what the old one offered");
  {
    const nav = (sel) => [...d.querySelectorAll("header.top nav a" + (sel || ""))]
      .map((a) => a.getAttribute("href"));
    t.ok("latest release", nav().includes("https://github.com/Cyan4973/xxHash/releases/latest"));
    t.ok("docs", nav().some((h) => h.startsWith("doc/")));
    t.ok("github", nav().includes("https://github.com/Cyan4973/xxHash"));
    // what survives once the wide-screen-only links drop off
    const small = nav(":not(.hide-sm)");
    t.is("still there on a phone", small.length, 4);
    t.ok("github among them", small.includes("https://github.com/Cyan4973/xxHash"));
  }

  t.section("the API docs links point at a page that is really there");
  {
    const docs = [...d.querySelectorAll('a[href^="doc/"]')].map((a) => a.getAttribute("href"));
    t.is("three of them", docs.length, 3);
    t.is("all naming one release", new Set(docs).size, 1);
    t.ok("no version typed into the template", !/v0\.8\.3/.test(fs.readFileSync(TEMPLATE, "utf8")));
    t.ok("the file exists", fs.existsSync(path.join(ROOT, docs[0])));
    // Independent of build.js's own comparison: numeric collation, not a copy
    // of the loop under test.
    const newest = fs.readdirSync(path.join(ROOT, "doc"))
      .filter((n) => /^v\d/.test(n))
      .sort((a, b) => a.localeCompare(b, undefined, { numeric: true }))
      .pop();
    t.is("the newest release under doc/", docs[0], `doc/${newest}/index.html`);
  }

  t.section("anchors the old page published still resolve");
  // README.md links to xxhash.com/#other-languages, and deep links to the other
  // three exist in the wild. Renaming a section must not break them.
  for (const id of ["summary", "benchmarks", "other-languages", "references"]) {
    t.ok("#" + id, d.getElementById(id));
  }
  const ids = new Set([...d.querySelectorAll("[id]")].map((e) => e.id));
  const dangling = [...d.querySelectorAll('a[href^="#"]')]
    .map((a) => a.getAttribute("href").slice(1))
    .filter((h) => h && !ids.has(h));
  t.is("dangling internal links", [...new Set(dangling)].join(",") || 0, 0);

  t.section("every referenced image exists");
  const missing = [...d.querySelectorAll("img[src]")]
    .map((i) => i.getAttribute("src"))
    .filter((s) => !fs.existsSync(path.join(ROOT, s)));
  t.is("missing files", missing.join(",") || 0, 0);

  t.section("platform selector actually hides charts");
  // asserting computed display, not the hidden attribute: [hidden] loses to any
  // author `display` rule unless it wins on order, which is easy to regress
  const visible = () => [...d.querySelectorAll(".chart")]
    .filter((c) => w.getComputedStyle(c).display !== "none").length;
  const tabs = [...d.querySelectorAll(".tab")];
  t.is("charts present", tabs.length, 3);
  t.is("one visible at rest", visible(), 1);
  t.is("tabs and charts share slugs",
    tabs.map((x) => x.dataset.plat).join(","),
    [...d.querySelectorAll(".chart")].map((c) => c.dataset.plat).join(","));
  tabs[2].click();
  t.is("still one visible after click", visible(), 1);
  t.is("clicked chart shown",
    w.getComputedStyle(d.querySelector('.chart[data-plat="' + tabs[2].dataset.plat + '"]')).display, "grid");
  t.is("first chart hidden",
    w.getComputedStyle(d.querySelector('.chart[data-plat="' + tabs[0].dataset.plat + '"]')).display, "none");
  t.is("bars scaled per platform",
    d.querySelector('.chart[data-plat="' + tabs[2].dataset.plat + '"] .bar').style.width, "100.0%");

  t.section("implementation filter");
  const filter = d.getElementById("filter");
  t.is("idle label", d.getElementById("implcount").textContent, "60 implementations");
  filter.value = "rust";
  filter.dispatchEvent(new w.Event("input"));
  t.is("'rust' matches", [...d.querySelectorAll("#impl tbody tr")].filter((r) => !r.hidden).length, 2);
  filter.value = "";
  filter.dispatchEvent(new w.Event("input"));
  t.is("cleared restores all", [...d.querySelectorAll("#impl tbody tr")].filter((r) => !r.hidden).length, 61);

  await page.close();
  return t.report();
};
