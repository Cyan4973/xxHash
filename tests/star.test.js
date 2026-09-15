// The star counter asks GitHub directly. Every way that can fail must leave a
// plain "Star" button rather than an empty count bubble.

const { render, until, suite } = require("./harness");

const API = "https://api.github.com/repos/Cyan4973/xxHash";
const ORIGIN = "https://xxhash.com/preview.html";   // localStorage needs a real origin
const HOUR = 60 * 60 * 1000;

const okResponse = () => Promise.resolve({ ok: true, json: () => Promise.resolve({ stargazers_count: 12345 }) });

async function settle(page) {
  await until(() => page.document.querySelector(".star-btn"));
  await until(() => page.document.querySelector(".star-count"), { timeout: 2500 });
}

module.exports = async function () {
  const t = suite("star counter");

  t.section("normal case");
  {
    const page = render({ origin: ORIGIN, fetch: okResponse });
    await settle(page);
    const { document: d, log } = page;
    t.is("count shown, grouped", d.querySelector(".star-count").textContent, "12,345");
    t.is("one API call", log.fetches.length, 1);
    t.is("endpoint", log.fetches[0].url, API);
    t.is("request carries no body", log.fetches.filter((f) => f.body).length, 0);
    t.is("links to stargazers", d.querySelector(".star-btn").getAttribute("href"),
      "https://github.com/Cyan4973/xxHash/stargazers");
    t.is("value cached", JSON.parse(page.window.localStorage.getItem("xxhash-stars")).n, 12345);
    await page.close();
  }

  t.section("cache");
  {
    const fresh = render({ origin: ORIGIN, fetch: okResponse, storage: { "xxhash-stars": { n: 999, at: Date.now() - HOUR } } });
    await settle(fresh);
    t.is("inside 6h: no API call", fresh.log.fetches.length, 0);
    t.is("inside 6h: cached value used", fresh.document.querySelector(".star-count").textContent, "999");
    await fresh.close();

    const stale = render({ origin: ORIGIN, fetch: okResponse, storage: { "xxhash-stars": { n: 999, at: Date.now() - 7 * HOUR } } });
    await settle(stale);
    t.is("past 6h: refetches", stale.log.fetches.length, 1);
    t.is("past 6h: new value shown", stale.document.querySelector(".star-count").textContent, "12,345");
    await stale.close();
  }

  t.section("failure modes never show an empty bubble");
  const failures = {
    "rate limited (403)": () => Promise.resolve({ ok: false, status: 403 }),
    "network error": () => Promise.reject(new Error("offline")),
    "unexpected payload": () => Promise.resolve({ ok: true, json: () => Promise.resolve({}) }),
  };
  for (const [label, impl] of Object.entries(failures)) {
    const page = render({ origin: ORIGIN, fetch: impl });
    await settle(page);
    t.is(label, page.document.querySelectorAll(".star-count").length, 0);
    t.is(label + ", button intact", page.document.querySelector(".star-face").textContent.trim(), "★ Star");
    await page.close();
  }

  {
    const noFetch = render({ origin: ORIGIN });   // browser without fetch()
    await settle(noFetch);
    t.is("no fetch() at all", noFetch.document.querySelectorAll(".star-count").length, 0);
    t.is("no fetch(), button intact", noFetch.document.querySelectorAll(".star-btn").length, 1);
    await noFetch.close();
  }

  t.section("an expired count beats no count");
  {
    const page = render({
      origin: ORIGIN,
      fetch: () => Promise.resolve({ ok: false, status: 403 }),
      storage: { "xxhash-stars": { n: 999, at: Date.now() - 7 * HOUR } },
    });
    await settle(page);
    t.is("stale value painted", page.document.querySelector(".star-count").textContent, "999");
    t.is("exactly one bubble", page.document.querySelectorAll(".star-count").length, 1);
    await page.close();
  }

  return t.report();
};
