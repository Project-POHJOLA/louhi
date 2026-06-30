/**
 * generate-icons.mjs — generates MIL-STD-2525C SVG icons for all affiliations
 *
 * Reads icons.xml, extracts base SIDC patterns, expands to all 10 standard
 * identities (Pending/Unknown/AssumedFriend/Friend/Neutral/Suspect/Hostile/
 * Joker/Faker/None), renders with milsymbol.js v3, converts <text> to <path>
 * via opentype.js for font-independent rendering.
 *
 * Usage: node generate-icons.mjs
 *
 * Output: assets/icons/map/2525/<sidc-lowercase>.svg
 *
 * Copyright Stefan Gofferje
 * Licensed under GPL v3 or higher
 */

import ms from 'milsymbol';
import opentype from 'opentype.js';
import { DOMParser, XMLSerializer } from '@xmldom/xmldom';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// ── paths ──────────────────────────────────────────────────────────────────
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const INPUT_XML    = path.join(__dirname, 'icons.xml');
const FONT_REGULAR = path.join(__dirname, 'LiberationSans-Regular.ttf');
const FONT_BOLD    = path.join(__dirname, 'LiberationSans-Bold.ttf');
const OUTPUT_DIR   = path.resolve(__dirname, '..', '2525');

// Ensure output dir exists
fs.mkdirSync(OUTPUT_DIR, { recursive: true });

// ── affiliation map ────────────────────────────────────────────────────────
// 10 standard identities from MIL-STD-2525C.
// Uppercase codes for milsymbol; lowercase for filenames.
const AFFILIATIONS = [
  { code: 'P', lower: 'p', name: 'Pending'         },
  { code: 'U', lower: 'u', name: 'Unknown'          },
  { code: 'A', lower: 'a', name: 'Assumed Friend'   },
  { code: 'F', lower: 'f', name: 'Friend'           },
  { code: 'N', lower: 'n', name: 'Neutral'          },
  { code: 'S', lower: 's', name: 'Suspect'          },
  { code: 'H', lower: 'h', name: 'Hostile'          },
  { code: 'J', lower: 'j', name: 'Joker'            },
  { code: 'K', lower: 'k', name: 'Faker'            },
  { code: 'O', lower: 'o', name: 'None'             },
];

// ── load fonts ─────────────────────────────────────────────────────────────
const fontRegular = opentype.loadSync(FONT_REGULAR);
const fontBold    = opentype.loadSync(FONT_BOLD);

// ── parse icons.xml ────────────────────────────────────────────────────────
function parseIconsXml(xmlPath) {
  const raw = fs.readFileSync(xmlPath, 'utf-8');
  const doc = new DOMParser().parseFromString(raw, 'text/xml');
  const iconNodes = doc.getElementsByTagName('icon');
  const seen  = new Set();
  const sidcs = [];

  for (let i = 0; i < iconNodes.length; i++) {
    const node = iconNodes[i];
    const filePathEl = node.getElementsByTagName('filePath')[0];
    if (!filePathEl) continue;
    const fpText = (filePathEl.textContent || '').trim();
    if (!fpText) continue;                             // skip empty entries
    const match = fpText.match(/Data\/2525_Icons\/(s[a-z-]{14})\.png/);
    if (!match) continue;
    if (seen.has(match[1])) continue;
    seen.add(match[1]);
    sidcs.push(match[1]);
  }
  return sidcs;
}
function toBasePattern(sidcLower) {
  if (sidcLower.length !== 15) return null;
  return 'S' + '{affil}' + sidcLower.substring(2).toUpperCase();
}

// ── text-to-path conversion ────────────────────────────────────────────────
function svgTextToPath(svgString) {
  const doc = new DOMParser().parseFromString(svgString, 'image/svg+xml');
  const textElements = doc.getElementsByTagName('text');

  // Collect first (live node list mutates as we replace)
  const texts = [];
  for (let i = 0; i < textElements.length; i++) texts.push(textElements[i]);

  for (const el of texts) {
    const text     = el.textContent || '';
    if (!text.trim()) continue;                         // skip empty

    const x        = parseFloat(el.getAttribute('x') || '0');
    const y        = parseFloat(el.getAttribute('y') || '0');
    const fontSize = parseFloat(el.getAttribute('font-size') || '12');
    const fontWeight = (el.getAttribute('font-weight') || 'normal').toLowerCase();
    const anchor   = (el.getAttribute('text-anchor') || 'start').toLowerCase();
    const baseline = (el.getAttribute('dominant-baseline') || 'auto').toLowerCase();
    const fillAttr = el.getAttribute('fill') || '';
    const strokeAttr = el.getAttribute('stroke') || '';

    // Pick bold or regular font
    const font = (fontWeight === 'bold' || fontWeight === '700') ? fontBold : fontRegular;

    // ── compute baseline from dominant-baseline ──
    // dominant-baseline="middle": y is the centre of the em-box.
    // opentype.js getPath() expects y as the alphabetic baseline.
    const scale  = fontSize / font.unitsPerEm;
    const ascent  = font.ascender  * scale;
    const descent = font.descender * scale;             // negative (below baseline)
    // For "middle", centre of em = (ascent + descent) / 2 above baseline
    // Em-box height in px = ascent - descent
    let baselineY = y;
    if (baseline === 'middle' || baseline === 'central') {
      baselineY = y + (ascent + descent) / 2;
    } else if (baseline === 'text-after-edge' || baseline === 'alphabetic') {
      baselineY = y;
    }

    // ── compute x offset for text-anchor ──
    let textX = x;
    if (anchor !== 'start') {
      let width = 0;
      for (let c = 0; c < text.length; c++) {
        const glyph = font.charToGlyph(text[c]);
        width += glyph.advanceWidth * scale;
      }
      if (anchor === 'middle') textX = x - width / 2;
      else if (anchor === 'end')  textX = x - width;
    }

    // ── generate path ──
    const glyphPath = font.getPath(text, textX, baselineY, fontSize);
    const pathData  = glyphPath.toPathData();

    // ── build <path> element ──
    const ns = 'http://www.w3.org/2000/svg';
    const pathEl = doc.createElementNS(ns, 'path');
    pathEl.setAttribute('d', pathData);
    if (fillAttr    && fillAttr    !== 'none') pathEl.setAttribute('fill', fillAttr);
    if (strokeAttr  && strokeAttr  !== 'none') pathEl.setAttribute('stroke', strokeAttr);
    // stroke-width from original text (milsymbol uses stroke="none" on text, so skip)

    el.parentNode.replaceChild(pathEl, el);
  }

  return new XMLSerializer().serializeToString(doc);
}

// ── generate one SIDC → SVG ────────────────────────────────────────────────
function renderSidc(sidcUpper) {
  // milsymbol.js v3 — 2525C standard
  const sym = new ms.Symbol(sidcUpper, {
    size:     35,
    standard: '2525C',
  });
  const svg = sym.asSVG();
  // Convert text to paths
  return svgTextToPath(svg);
}

// ── format duration ────────────────────────────────────────────────────────
function elapsed(msElapsed) {
  if (msElapsed < 1000) return `${msElapsed}ms`;
  return `${(msElapsed / 1000).toFixed(1)}s`;
}

// ── main ───────────────────────────────────────────────────────────────────
function main() {
  console.log('Reading icons.xml …');
  const referenceSidcs = parseIconsXml(INPUT_XML);
  console.log(`  Found ${referenceSidcs.length} reference SIDCs`);

  // Deduplicate to unique base patterns (strip affiliation)
  const baseSet = new Set();
  for (const s of referenceSidcs) {
    const pat = toBasePattern(s);
    if (pat) baseSet.add(pat);
  }
  const basePatterns = [...baseSet].sort();
  console.log(`  ${basePatterns.length} unique base patterns`);

  const totalExpected = basePatterns.length * AFFILIATIONS.length;
  console.log(`  Expected output: ${totalExpected} icons (${basePatterns.length} patterns × ${AFFILIATIONS.length} affiliations)`);
  console.log('');

  let generated = 0;
  let errors = 0;
  const t0 = Date.now();

  for (const base of basePatterns) {
    for (const aff of AFFILIATIONS) {
      // Build full 15-char uppercase SIDC
      const sidcUpper = base.replace('{affil}', aff.code);
      const filename  = sidcUpper.toLowerCase() + '.svg';
      const outPath   = path.join(OUTPUT_DIR, filename);

      // Skip if already exists (resume-friendly)
      // if (fs.existsSync(outPath)) { generated++; continue; }

      try {
        const svg = renderSidc(sidcUpper);
        fs.writeFileSync(outPath, svg, 'utf-8');
        generated++;
      } catch (err) {
        console.error(`  ERROR ${sidcUpper}: ${err.message}`);
        errors++;
      }
    }

    // Progress
    const pct = ((generated + errors) / totalExpected * 100).toFixed(1);
    process.stdout.write(`\r  ${pct}%  (${generated} OK, ${errors} errors, ${elapsed(Date.now() - t0)})   `);
  }

  const totalTime = Date.now() - t0;
  console.log('\n');
  console.log(`Done: ${generated} icons generated, ${errors} errors in ${elapsed(totalTime)}`);
  console.log(`Output: ${OUTPUT_DIR}`);
}

main();
