// Carte de rang (/rank) : avatar, pseudo, rang, niveau, barre d'XP et statistiques, aux couleurs du site.
import { createCanvas } from '@napi-rs/canvas';
import { BRAND, HEX } from '../config.mjs';
import {
  cleanName,
  drawAvatar,
  drawCardBackground,
  drawSegments,
  finishCard,
  fitText,
  font,
  formatDuration,
  formatNumber,
  loadAvatar,
  measureSegments,
  roundRect,
  withAlpha,
} from '../utils/canvas.mjs';

const WIDTH = 934;
const HEIGHT = 282;
const RADIUS = 28;
const AVATAR = { cx: 146, cy: 141, radius: 90 };
const LEFT = 284; // début de la zone de texte
const RIGHT = WIDTH - 42;
const BAR = { y: 168, height: 34 };

function drawBar(ctx, progress) {
  const width = RIGHT - LEFT;
  const { y, height } = BAR;
  const r = height / 2;

  roundRect(ctx, LEFT, y, width, height, r);
  ctx.fillStyle = HEX.surface2;
  ctx.fill();
  ctx.lineWidth = 1.5;
  ctx.strokeStyle = HEX.line;
  ctx.stroke();

  if (progress <= 0) return;
  // Une barre entamée reste une pilule entière (largeur minimale = sa hauteur).
  const fillWidth = Math.max(height, width * Math.min(1, progress));
  const gradient = ctx.createLinearGradient(LEFT, 0, LEFT + fillWidth, 0);
  gradient.addColorStop(0, HEX.blue);
  gradient.addColorStop(1, HEX.blueText);

  ctx.save();
  ctx.shadowColor = withAlpha(HEX.blue, 0.65);
  ctx.shadowBlur = 18;
  roundRect(ctx, LEFT, y, fillWidth, height, r);
  ctx.fillStyle = gradient;
  ctx.fill();
  ctx.restore();

  // Reflet sur la moitié haute.
  ctx.save();
  roundRect(ctx, LEFT, y, fillWidth, height, r);
  ctx.clip();
  const sheen = ctx.createLinearGradient(0, y, 0, y + height);
  sheen.addColorStop(0, 'rgba(255, 255, 255, 0.22)');
  sheen.addColorStop(0.5, 'rgba(255, 255, 255, 0.04)');
  sheen.addColorStop(1, 'rgba(255, 255, 255, 0)');
  ctx.fillStyle = sheen;
  ctx.fillRect(LEFT, y, fillWidth, height);
  ctx.restore();

  // Pastille dorée au bout du remplissage.
  ctx.save();
  ctx.shadowColor = withAlpha(HEX.gold, 0.8);
  ctx.shadowBlur = 10;
  ctx.fillStyle = HEX.gold;
  ctx.beginPath();
  ctx.arc(LEFT + fillWidth - r, y + r, 6, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

/**
 * Carte PNG de rang d'un membre.
 * @returns {Promise<Buffer>}
 */
export async function renderRankCard({
  username,
  avatarURL,
  level = 0,
  rank = 1,
  xpIntoLevel = 0,
  xpForNext = 100,
  totalXp = 0,
  messages = 0,
  voiceSeconds = 0,
  rankName = null, // grade (rôle récompense) affiché à la place de la marque, dans sa couleur
  rankColor = null,
}) {
  const canvas = createCanvas(WIDTH, HEIGHT);
  const ctx = canvas.getContext('2d');
  const name = cleanName(username);
  const avatar = await loadAvatar(avatarURL);
  // Nom du grade sans emoji ; ignoré s'il ne reste rien de dessinable (cleanName renvoie alors « Member »).
  const cleanRank = rankName ? cleanName(rankName) : null;
  const rankLabel = cleanRank && (cleanRank !== 'Member' || /member/i.test(rankName)) ? cleanRank : null;
  const accent = rankLabel && /^#[0-9a-f]{6}$/i.test(rankColor ?? '') && rankColor !== '#000000' ? rankColor : null;

  drawCardBackground(ctx, WIDTH, HEIGHT, { radius: RADIUS, focus: { x: AVATAR.cx, y: AVATAR.cy } });
  drawAvatar(ctx, { image: avatar, name, ...AVATAR });

  // Ligne du haut : grade (ou marque) à gauche, rang et niveau à droite.
  ctx.fillStyle = accent ?? HEX.red;
  roundRect(ctx, LEFT, 64, 22, 6, 3);
  ctx.fill();
  ctx.save();
  ctx.letterSpacing = '3px';
  ctx.font = font(14);
  ctx.fillStyle = rankLabel ? (accent ?? HEX.text) : HEX.muted;
  ctx.fillText(fitText(ctx, (rankLabel ?? BRAND).toUpperCase(), 300, 14, 11).text, LEFT + 32, 72);
  ctx.restore();

  const stats = [
    { text: 'RANK', size: 17, color: HEX.muted, gap: 8 },
    { text: `#${formatNumber(rank)}`, size: 42, color: HEX.blueText, gap: 26 },
    { text: 'LEVEL', size: 17, color: HEX.muted, gap: 8 },
    { text: formatNumber(level), size: 42, color: HEX.gold },
  ];
  drawSegments(ctx, stats, RIGHT, 84, 'right');

  // Pseudo et XP du niveau, juste au-dessus de la barre.
  const xpLabel = [
    { text: formatNumber(xpIntoLevel), size: 22, color: HEX.text },
    { text: ` / ${formatNumber(xpForNext)} XP`, size: 22, color: HEX.muted },
  ];
  const xpWidth = measureSegments(ctx, xpLabel);
  drawSegments(ctx, xpLabel, RIGHT, 152, 'right');

  const fitted = fitText(ctx, name, RIGHT - LEFT - xpWidth - 28, 44, 26);
  ctx.fillStyle = HEX.text;
  ctx.fillText(fitted.text, LEFT, 152);

  drawBar(ctx, xpForNext > 0 ? xpIntoLevel / xpForNext : 0);

  // Statistiques sous la barre.
  const total = `${formatNumber(totalXp)} total XP`;
  ctx.font = font(18);
  const totalWidth = ctx.measureText(total).width;
  ctx.fillStyle = HEX.muted;
  ctx.textAlign = 'right';
  ctx.fillText(total, RIGHT, 238);
  ctx.textAlign = 'left';

  const line = `${formatNumber(messages)} messages  ·  ${formatDuration(voiceSeconds)} in voice`;
  const statsLine = fitText(ctx, line, RIGHT - LEFT - totalWidth - 24, 18, 14);
  ctx.fillStyle = HEX.muted;
  ctx.fillText(statsLine.text, LEFT, 238);

  finishCard(ctx, WIDTH, HEIGHT, RADIUS);
  return canvas.encode('png');
}
