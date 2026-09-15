#!/usr/bin/env node
//
// Generates the homepage from `src/template.html` + `src/data.md`.
//
//   node build.js          # writes preview.html
//
// Everything that is a *list* lives in data.md as an ordinary markdown pipe
// table, so adding a language or a logo stays a one-line edit. Everything that
// is *layout* lives in the template. The script only knows pipe tables — there
// is no markdown library involved, and nothing is fetched at build time.
//
// Re-run it after editing either file, and commit the generated HTML: GitHub
// Pages serves that file directly, so there is no CI step to keep alive.

const fs = require("fs");
const path = require("path");

const ROOT = __dirname;
const OUT = path.join(ROOT, "preview.html");

/* ------------------------------ markdown bits ------------------------------ */

const escapeHtml = (s) =>
  s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");

// The only inline syntax used in the data tables.
const inline = (s) =>
  escapeHtml(s)
    .replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2">$1</a>')
    .replace(/__([^_]+)__/g, "<strong>$1</strong>")
    .replace(/_([^_]+)_/g, "<em>$1</em>")
    .replace(/`([^`]+)`/g, "<code>$1</code>");

// Plain text of a cell, with the markup stripped rather than rendered.
const plain = (s) => s.replace(/\[([^\]]+)\]\([^)]+\)/g, "$1").replace(/[_`*]/g, "").trim();

function parseTable(lines) {
  const rows = lines
    .filter((l) => l.trim().startsWith("|"))
    .map((l) => l.trim().replace(/^\|/, "").replace(/\|$/, "").split("|").map((c) => c.trim()));
  if (!rows.length) return [];
  // drop the header and the |---|---| separator
  return rows.slice(1).filter((r) => !/^:?-{2,}:?$/.test(r[0]));
}

/* --------------------------------- data.md --------------------------------- */
// Blocks are `## name`; inside `## used-by`, each `### Title` is one category.

function readData(file) {
  const blocks = {};
  let current = null;
  for (const line of fs.readFileSync(file, "utf8").split("\n")) {
    const h2 = /^##\s+(?!#)(.+)$/.exec(line);
    if (h2) {
      current = h2[1].trim();
      blocks[current] = [];
      continue;
    }
    if (current) blocks[current].push(line);
  }
  return blocks;
}

function splitCategories(lines) {
  const cats = [];
  let cur = null;
  for (const line of lines) {
    const h3 = /^###\s+(.+)$/.exec(line);
    if (h3) {
      cur = { title: h3[1].trim(), lines: [] };
      cats.push(cur);
      continue;
    }
    if (cur) cur.lines.push(line);
  }
  return cats;
}

/* -------------------------------- renderers -------------------------------- */

// Platform | Variant | Bandwidth | Kind
// Bars are scaled against the fastest entry of each platform, computed here so
// that the widths can never drift away from the numbers.
function renderBandwidth(rows) {
  const platforms = [];
  for (const [platform, variant, bandwidth, kind] of rows) {
    let p = platforms.find((x) => x.name === platform);
    if (!p) platforms.push((p = { name: platform, rows: [] }));
    p.rows.push({ variant, bandwidth, ref: (kind || "").toLowerCase() === "reference" });
  }

  const num = (s) => parseFloat(s);
  const tabs = platforms
    .map(
      (p, i) =>
        `      <button class="tab" role="tab" data-plat="${slug(p.name)}"` +
        ` aria-selected="${i === 0}">${escapeHtml(p.name)}</button>`
    )
    .join("\n");

  const charts = platforms
    .map((p) => {
      const max = Math.max(...p.rows.map((r) => num(r.bandwidth)));
      const bars = p.rows
        .map((r) => {
          const w = ((num(r.bandwidth) / max) * 100).toFixed(1);
          const ref = r.ref ? " ref" : "";
          return (
            `      <div class="name${ref}">${inline(r.variant)}</div>` +
            `<div class="track${ref}"><div class="bar" style="width:${w}%"></div></div>` +
            `<div class="val${ref}">${escapeHtml(r.bandwidth)}</div>`
          );
        })
        .join("\n");
      const hidden = p === platforms[0] ? "" : " hidden";
      return `    <div class="chart" data-plat="${slug(p.name)}"${hidden}>\n${bars}\n    </div>`;
    })
    .join("\n\n");

  return { tabs, charts };
}

const slug = (s) => s.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");

// Variant | Output | Use when
function renderVariants(rows) {
  return rows
    .map(
      ([variant, output, when]) =>
        `          <tr><td>${inline(variant)}</td><td>${inline(output)}</td>` +
        `<td>${inline(when)}</td></tr>`
    )
    .join("\n");
}

// Hash | Width | Bandwidth | Small data | Note
// A bolded hash name marks one of ours, and gets the highlight class.
function renderComparison(rows) {
  return rows
    .map(([hash, width, bw, small, note]) => {
      const mine = /^__/.test(hash) ? ' class="me"' : "";
      return (
        `          <tr${mine}><td>${inline(hash)}</td>` +
        `<td class="num">${inline(width)}</td>` +
        `<td class="num">${inline(bw)}</td>` +
        `<td class="num">${inline(small)}</td>` +
        `<td>${inline(note || "")}</td></tr>`
      );
    })
    .join("\n");
}

// Language | Author | URL
function renderImplementations(main, shells) {
  const row = ([lang, author, url]) =>
    `          <tr><td>${inline(lang)}</td><td>${escapeHtml(author)}</td>` +
    `<td class="host"><a href="${escapeHtml(url)}">` +
    `${escapeHtml(url.replace(/^https?:\/\//, "").replace(/\/$/, ""))}</a></td></tr>`;

  return [
    ...main.map(row),
    `          <tr class="grouphead"><td colspan="3">Shells and assembly</td></tr>`,
    ...shells.map(row),
  ].join("\n");
}

// Project | Logo | URL      — a leading * also puts the logo in the trust strip.
function renderUsedBy(categories) {
  const featured = [];
  const sections = categories
    .map((cat) => {
      const items = parseTable(cat.lines).map(([name, logo, url]) => {
        const star = name.startsWith("*");
        const label = star ? name.slice(1).trim() : name;
        const src = logo ? `images/logo50/${logo}` : "images/logo50/placeholder.png";
        if (star) featured.push({ label, src });
        return (
          `      <a href="${escapeHtml(url)}"><img src="${escapeHtml(src)}" alt="">` +
          `<span>${escapeHtml(label)}</span></a>`
        );
      });
      return `    <h3>${escapeHtml(cat.title)}</h3>\n    <div class="grid">\n${items.join("\n")}\n    </div>`;
    })
    .join("\n");

  const strip = featured
    .map(
      (f) =>
        `      <a href="#usedby" title="${escapeHtml(f.label)}">` +
        `<img src="${escapeHtml(f.src)}" alt="${escapeHtml(f.label)}"></a>`
    )
    .join("\n");

  const total = categories.reduce((n, c) => n + parseTable(c.lines).length, 0);
  return { sections, strip, featured: featured.length, total };
}

/* ---------------------------------- build ---------------------------------- */

const data = readData(path.join(ROOT, "src", "data.md"));
const need = (name) => {
  if (!data[name]) throw new Error(`src/data.md is missing the "## ${name}" block`);
  return parseTable(data[name]);
};

const bandwidth = renderBandwidth(need("bandwidth"));
const comparison = renderComparison(need("comparison"));
const implMain = need("implementations");
const implShell = need("shells");
const usedBy = renderUsedBy(splitCategories(data["used-by"] || []));

const fields = {
  variants: renderVariants(need("variants")),
  "bandwidth-tabs": bandwidth.tabs,
  "bandwidth-charts": bandwidth.charts,
  comparison: comparison,
  implementations: renderImplementations(implMain, implShell),
  "implementation-count": String(implMain.length + implShell.length),
  "used-by": usedBy.sections,
  "trust-strip": usedBy.strip,
  "used-by-count": String(usedBy.total),
  "used-by-remainder": String(usedBy.total - usedBy.featured),
};

let page = fs.readFileSync(path.join(ROOT, "src", "template.html"), "utf8");
for (const [key, value] of Object.entries(fields)) {
  const token = `{{${key}}}`;
  if (!page.includes(token)) throw new Error(`template.html never uses ${token}`);
  page = page.split(token).join(value);
}
const leftover = page.match(/\{\{[a-z-]+\}\}/g);
if (leftover) throw new Error(`template.html has unfilled placeholders: ${leftover.join(", ")}`);

fs.writeFileSync(OUT, page);

console.log(`wrote ${path.relative(ROOT, OUT)}  (${(fs.statSync(OUT).size / 1024).toFixed(1)} KB)`);
console.log(`  implementations : ${implMain.length} + ${implShell.length} shells`);
console.log(`  used by         : ${usedBy.total} projects, ${usedBy.featured} featured in the trust strip`);
console.log(`  comparison rows : ${need("comparison").length}`);
