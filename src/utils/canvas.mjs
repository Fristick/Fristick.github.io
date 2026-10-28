// Outils de dessin partagés par les cartes (rang, bienvenue) : police, avatars, fond, textes.
// Le serveur n'a aucune police système : tout texte utilise Bricolage (une seule graisse, 800).
import { GlobalFonts, loadImage } from '@napi-rs/canvas';
import { fileURLToPath } from 'node:url';
import { HEX } from '../config.mjs';
import { log } from './log.mjs';

export const FONT = 'Bricolage';
const FONT_PATH = fileURLToPath(new URL('../../assets/fonts/bricolage-grotesque.woff2', import.meta.url));

if (!GlobalFonts.registerFromPath(FONT_PATH, FONT)) log.warn(`Could not load the card font (${FONT_PATH}).`);

// Chaîne CSS pour ctx.font.
export const font = (size) => `800 ${Math.round(size)}px ${FONT}`;

// Caractères présents dans la police (table cmap du fichier woff2) : latin de base, Latin-1 et un peu de ponctuation.
const DRAWABLE =
  /[ -~ -£¥-¬®-ÿĂıŒœˆ˚˜–—‘-‚“-„•…′″‹›⁄€™↑↓−]/u;
// Lettres latines sans décomposition Unicode : on garde une lettre proche plutôt que de la perdre.
const TRANSLIT = { ł: 'l', Ł: 'L', đ: 'd', Đ: 'D', ħ: 'h', Ħ: 'H', ŀ: 'l', Ŀ: 'L', ŧ: 't', Ŧ: 'T', ſ: 's' };

function strip(name) {
  let out = '';
  for (const char of String(name ?? '').normalize('NFC')) {
    if (DRAWABLE.test(char)) out += char;
    else if (TRANSLIT[char]) out += TRANSLIT[char];
    // Lettres stylisées (𝓐, Ａ, Ⓐ…) ou accents absents de la police : on garde la lettre de base.
    else for (const part of char.normalize('NFKD')) if (DRAWABLE.test(part)) out += part;
  }
  return out.replace(/\s+/g, ' ').trim();
}

// Nom affichable par la police : premier candidat qui garde au moins une lettre ou un chiffre, sinon 'Member'.
export function cleanName(...candidates) {
  for (const candidate of candidates) {
    const text = strip(candidate);
    if (/[\p{L}\p{N}]/u.test(text)) return text;
  }
  return 'Member';
}

const numberFormat = new Intl.NumberFormat('en-US');
export const formatNumber = (value) => numberFormat.format(Math.max(0, Math.floor(Number(value) || 0)));

// 12 000 s → « 3h 20m » ; moins d'une heure → « 20m ».
export function formatDuration(seconds) {
  const minutes = Math.floor(Math.max(0, Number(seconds) || 0) / 60);
  const hours = Math.floor(minutes / 60);
  return hours ? `${formatNumber(hours)}h ${minutes % 60}m` : `${minutes}m`;
}

// Avatar téléchargé (délai max 5 s) ; null si l'URL est absente ou en échec.
export async function loadAvatar(url, timeoutMs = 5000) {
  if (!url) return null;
  try {
    const res = await fetch(url, { signal: AbortSignal.timeout(timeoutMs) });
    if (!res.ok) return null;
    return await loadImage(Buffer.from(await res.arrayBuffer()));
  } catch {
    return null;
  }
}

// Chemin d'un rectangle arrondi (à remplir, tracer ou utiliser comme masque).
export function roundRect(ctx, x, y, width, height, radius) {
  const r = Math.max(0, Math.min(radius, width / 2, height / 2));
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + width, y, x + width, y + height, r);
  ctx.arcTo(x + width, y + height, x, y + height, r);
  ctx.arcTo(x, y + height, x, y, r);
  ctx.arcTo(x, y, x + width, y, r);
  ctx.closePath();
}

// Image recadrée en cercle (mode « cover »).
export function drawCircleImage(ctx, image, cx, cy, radius) {
  const scale = Math.max((radius * 2) / image.width, (radius * 2) / image.height);
  const width = image.width * scale;
  const height = image.height * scale;
  ctx.save();
  ctx.beginPath();
  ctx.arc(cx, cy, radius, 0, Math.PI * 2);
  ctx.closePath();
  ctx.clip();
  ctx.drawImage(image, cx - width / 2, cy - height / 2, width, height);
  ctx.restore();
}

// Plus grande taille (startSize → minSize) qui tient dans maxWidth ; sinon texte tronqué avec « … ».
// Laisse ctx.font réglé sur la taille retenue.
export function fitText(ctx, text, maxWidth, startSize, minSize = startSize) {
  let size = startSize;
  ctx.font = font(size);
  while (size > minSize && ctx.measureText(text).width > maxWidth) {
    size -= 1;
    ctx.font = font(size);
  }
  if (ctx.measureText(text).width <= maxWidth) return { text, size };
  let cut = text;
  while (cut.length > 1 && ctx.measureText(`${cut}…`).width > maxWidth) cut = cut.slice(0, -1);
  return { text: `${cut.trimEnd()}…`, size };
}

// Texte multicolore sur une ligne : segments [{ text, size, color, gap? }], alignés à gauche, à droite ou au centre.
// Renvoie la largeur totale.
export function drawSegments(ctx, segments, x, y, align = 'left') {
  const widths = segments.map((s) => {
    ctx.font = font(s.size);
    return ctx.measureText(s.text).width + (s.gap ?? 0);
  });
  const total = widths.reduce((a, b) => a + b, 0);
  let cursor = align === 'right' ? x - total : align === 'center' ? x - total / 2 : x;
  ctx.save();
  ctx.textAlign = 'left';
  ctx.textBaseline = 'alphabetic';
  segments.forEach((s, i) => {
    ctx.font = font(s.size);
    ctx.fillStyle = s.color;
    ctx.fillText(s.text, cursor, y);
    cursor += widths[i];
  });
  ctx.restore();
  return total;
}

export const measureSegments = (ctx, segments) =>
  segments.reduce((sum, s) => {
    ctx.font = font(s.size);
    return sum + ctx.measureText(s.text).width + (s.gap ?? 0);
  }, 0);

function glow(ctx, x, y, radius, color, alpha) {
  const gradient = ctx.createRadialGradient(x, y, 0, x, y, radius);
  gradient.addColorStop(0, withAlpha(color, alpha));
  gradient.addColorStop(1, withAlpha(color, 0));
  ctx.fillStyle = gradient;
  ctx.fillRect(x - radius, y - radius, radius * 2, radius * 2);
}

// '#2a5bff' + 0.4 → 'rgba(42, 91, 255, 0.4)'.
export function withAlpha(hex, alpha) {
  const n = parseInt(hex.slice(1), 16);
  return `rgba(${(n >> 16) & 255}, ${(n >> 8) & 255}, ${n & 255}, ${alpha})`;
}

// Fond commun des cartes : dégradé marine, halos, anneaux et grille de points décoratifs, bordure fine.
// À appeler en premier : les coins restent transparents (masque arrondi conservé jusqu'à finishCard).
export function drawCardBackground(ctx, width, height, { radius = 28, focus = { x: width / 2, y: height / 2 } } = {}) {
  ctx.save();
  roundRect(ctx, 0, 0, width, height, radius);
  ctx.clip();

  const base = ctx.createLinearGradient(0, 0, width, height);
  base.addColorStop(0, HEX.bg);
  base.addColorStop(0.55, HEX.bg2);
  base.addColorStop(1, HEX.surface);
  ctx.fillStyle = base;
  ctx.fillRect(0, 0, width, height);

  glow(ctx, focus.x, focus.y, height * 1.05, HEX.blue, 0.3);
  glow(ctx, width - 40, -30, height * 1.1, HEX.surface2, 0.9);
  glow(ctx, width * 0.72, height + 40, height * 0.7, HEX.red, 0.07);

  // Anneaux concentriques en bas à droite.
  ctx.lineWidth = 1.5;
  for (const [i, r] of [130, 190, 250, 310].entries()) {
    ctx.strokeStyle = withAlpha(HEX.blueText, 0.09 - i * 0.015);
    ctx.beginPath();
    ctx.arc(width - 30, height + 50, r, 0, Math.PI * 2);
    ctx.stroke();
  }

  // Petite grille de points en haut à droite.
  ctx.fillStyle = withAlpha(HEX.blueText, 0.16);
  for (let row = 0; row < 3; row++) {
    for (let col = 0; col < 6; col++) {
      ctx.beginPath();
      ctx.arc(width - 40 - col * 13, 22 + row * 12, 1.5, 0, Math.PI * 2);
      ctx.fill();
    }
  }

  // Trait lumineux en haut.
  const sheen = ctx.createLinearGradient(0, 0, width, 0);
  sheen.addColorStop(0, withAlpha(HEX.blueText, 0));
  sheen.addColorStop(0.5, withAlpha(HEX.blueText, 0.35));
  sheen.addColorStop(1, withAlpha(HEX.blueText, 0));
  ctx.fillStyle = sheen;
  ctx.fillRect(radius, 0, width - radius * 2, 1.5);
}

// Ferme le masque ouvert par drawCardBackground et trace la bordure.
export function finishCard(ctx, width, height, radius = 28) {
  ctx.restore();
  ctx.lineWidth = 2;
  ctx.strokeStyle = HEX.line;
  roundRect(ctx, 1, 1, width - 2, height - 2, radius - 1);
  ctx.stroke();
}

// Avatar rond avec anneau dégradé bleu ; sans image : disque bleu et initiale du nom.
export function drawAvatar(ctx, { image, name, cx, cy, radius, ring = 6, gap = 4 }) {
  const outer = radius + gap + ring / 2;

  ctx.save();
  ctx.shadowColor = withAlpha(HEX.blue, 0.55);
  ctx.shadowBlur = 28;
  ctx.fillStyle = HEX.bg2;
  ctx.beginPath();
  ctx.arc(cx, cy, outer + ring / 2, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();

  const ringGradient = ctx.createLinearGradient(cx - outer, cy - outer, cx + outer, cy + outer);
  ringGradient.addColorStop(0, HEX.blueText);
  ringGradient.addColorStop(0.5, HEX.blue);
  ringGradient.addColorStop(1, HEX.blue);
  ctx.lineWidth = ring;
  ctx.strokeStyle = ringGradient;
  ctx.beginPath();
  ctx.arc(cx, cy, outer, 0, Math.PI * 2);
  ctx.stroke();

  if (image) {
    drawCircleImage(ctx, image, cx, cy, radius);
    return;
  }
  const fill = ctx.createLinearGradient(cx - radius, cy - radius, cx + radius, cy + radius);
  fill.addColorStop(0, HEX.blue);
  fill.addColorStop(1, '#1638b8');
  ctx.fillStyle = fill;
  ctx.beginPath();
  ctx.arc(cx, cy, radius, 0, Math.PI * 2);
  ctx.fill();

  const initial = (cleanName(name).match(/[\p{L}\p{N}]/u)?.[0] ?? 'M').toUpperCase();
  ctx.save();
  ctx.font = font(radius * 1.05);
  ctx.fillStyle = HEX.text;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  // Centrage optique : la hauteur des capitales est mesurée plutôt que supposée.
  const m = ctx.measureText(initial);
  const offset = (m.actualBoundingBoxAscent - m.actualBoundingBoxDescent) / 2;
  ctx.textBaseline = 'alphabetic';
  ctx.fillText(initial, cx, cy + offset);
  ctx.restore();
}
