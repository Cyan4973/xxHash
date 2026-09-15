// Structure and content of the generated page, plus the two interactive bits
// that are pure DOM: the platform selector and the implementation filter.

const { render, until, suite } = require("./harness");

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
  const fs = require("fs"), path = require("path"), { ROOT } = require("./harness");
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
