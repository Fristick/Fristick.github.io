// Carte de bienvenue : avatar centré, « WELCOME », pseudo, numéro de membre et nom du serveur.
import { createCanvas } from '@napi-rs/canvas';
import { BRAND, HEX } from '../config.mjs';
import {
  cleanName,
  drawAvatar,
  drawCardBackground,
  finishCard,
  fitText,
  font,
  formatNumber,
  loadAvatar,
  roundRect,
} from '../utils/canvas.mjs';

const WIDTH = 1024;
const HEIGHT = 450;
const RADIUS = 32;
const AVATAR = { cx: WIDTH / 2, cy: 166, radius: 86 };

/**
 * Carte PNG de bienvenue.
 * @returns {Promise<Buffer>}
 */
export async function renderWelcomeCard({ username, avatarURL, serverName, memberCount }) {
  const canvas = createCanvas(WIDTH, HEIGHT);
  const ctx = canvas.getContext('2d');
  const name = cleanName(username);
  const avatar = await loadAvatar(avatarURL);

  drawCardBackground(ctx, WIDTH, HEIGHT, { radius: RADIUS, focus: { x: AVATAR.cx, y: AVATAR.cy } });
  drawAvatar(ctx, { image: avatar, name, ...AVATAR, ring: 7 });

  ctx.textAlign = 'center';

  // Nom du serveur en petit, encadré de deux traits rouges.
  const server = cleanName(serverName, BRAND).toUpperCase();
  ctx.save();
  ctx.letterSpacing = '4px';
  const label = fitText(ctx, server, 560, 15, 12);
  // L'espacement s'ajoute aussi après la dernière lettre : on le retire pour centrer.
  const labelWidth = ctx.measureText(label.text).width - 4;
  ctx.fillStyle = HEX.blueText;
  ctx.fillText(label.text, WIDTH / 2 + 2, 48);
  ctx.restore();
  ctx.fillStyle = HEX.red;
  for (const x of [WIDTH / 2 - labelWidth / 2 - 40, WIDTH / 2 + labelWidth / 2 + 16]) {
    roundRect(ctx, x, 42, 24, 5, 2.5);
    ctx.fill();
  }

  ctx.save();
  ctx.letterSpacing = '8px';
  ctx.font = font(66);
  ctx.fillStyle = HEX.gold;
  ctx.shadowColor = 'rgba(255, 209, 102, 0.25)';
  ctx.shadowBlur = 24;
  ctx.fillText('WELCOME', WIDTH / 2 + 4, 330);
  ctx.restore();

  const fitted = fitText(ctx, name, WIDTH - 160, 40, 24);
  ctx.fillStyle = HEX.text;
  ctx.fillText(fitted.text, WIDTH / 2, 381);

  ctx.font = font(21);
  ctx.fillStyle = HEX.muted;
  ctx.fillText(`Member #${formatNumber(memberCount)}`, WIDTH / 2, 418);

  finishCard(ctx, WIDTH, HEIGHT, RADIUS);
  return canvas.encode('png');
}
