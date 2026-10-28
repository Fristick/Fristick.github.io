// Transcript d'un ticket : un seul fichier HTML autonome (CSS inline) aux couleurs de renard-studio.fr.
// renderTranscriptHtml() est pure (objets simples) ; buildTranscript() récupère les messages Discord et l'appelle.
import { MessageType } from 'discord.js';
import { BRAND, HEX, SITE_URL, TICKET_CATEGORIES } from '../config.mjs';

export const MAX_TRANSCRIPT_MESSAGES = 5000;
const GROUP_WINDOW_MS = 7 * 60_000; // messages regroupés sous le même en-tête, comme dans Discord
const MARK = '\u0000'; // délimite les fragments HTML mis de côté pendant le rendu markdown

const ESCAPES = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };
const UNESCAPES = Object.fromEntries(Object.entries(ESCAPES).map(([char, entity]) => [entity, char]));

export const escapeHtml = (value) => String(value ?? '').replace(/[&<>"']/g, (char) => ESCAPES[char]);
const unescapeHtml = (value) => value.replace(/&(?:amp|lt|gt|quot|#39);/g, (entity) => UNESCAPES[entity]);

export const padNumber = (number) => String(number ?? 0).padStart(4, '0');

// Seuls les liens http(s) sont gardés (pas de javascript:, data:…).
function safeUrl(url) {
  try {
    const parsed = new URL(String(url ?? ''));
    return parsed.protocol === 'http:' || parsed.protocol === 'https:' ? parsed.href : '';
  } catch {
    return '';
  }
}

const DATE_FORMAT = new Intl.DateTimeFormat('en-GB', {
  day: '2-digit',
  month: 'short',
  year: 'numeric',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
  hourCycle: 'h23',
  timeZone: 'UTC',
});

const TIME_FORMAT = new Intl.DateTimeFormat('en-GB', {
  hour: '2-digit',
  minute: '2-digit',
  hourCycle: 'h23',
  timeZone: 'UTC',
});

const validDate = (ms) => {
  const date = new Date(ms == null ? NaN : Number(ms));
  return Number.isNaN(date.getTime()) ? null : date;
};

export function formatDate(ms) {
  const date = validDate(ms);
  return date ? `${DATE_FORMAT.format(date)} UTC` : '—';
}

export function formatDuration(ms) {
  const total = Math.max(0, Math.floor(Number(ms) / 1000) || 0);
  if (total < 60) return `${total}s`;
  const parts = [
    [Math.floor(total / 86_400), 'd'],
    [Math.floor((total % 86_400) / 3600), 'h'],
    [Math.floor((total % 3600) / 60), 'm'],
  ];
  return parts.filter(([value]) => value > 0).map(([value, unit]) => `${value}${unit}`).join(' ');
}

function formatSize(bytes) {
  if (!Number.isFinite(bytes)) return '';
  const units = ['B', 'KB', 'MB', 'GB'];
  let value = bytes;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }
  return `${unit ? value.toFixed(1) : value} ${units[unit]}`;
}

const hexColor = (value) => {
  if (Number.isInteger(value) && value > 0) return `#${value.toString(16).padStart(6, '0')}`;
  return typeof value === 'string' && /^#[0-9a-f]{6}$/i.test(value) && value !== '#000000' ? value : null;
};

const categoryLabel = (id) => {
  const category = TICKET_CATEGORIES.find((c) => c.id === id);
  return category ? `${category.emoji} ${category.label}` : String(id ?? 'Unknown');
};

// ---------- Markdown Discord (sous-ensemble) ----------

// Règles en ligne : la plus proche gagne, son contenu est rendu récursivement (HTML toujours bien imbriqué).
const INLINE_RULES = [
  [/\*\*\*([\s\S]+?)\*\*\*/, (inner) => `<strong><em>${inner}</em></strong>`],
  [/\*\*([\s\S]+?)\*\*/, (inner) => `<strong>${inner}</strong>`],
  [/__([\s\S]+?)__/, (inner) => `<u>${inner}</u>`],
  [/\*([^*\s](?:[^*]*?[^*\s])?)\*/, (inner) => `<em>${inner}</em>`],
  [/\b_([^_]+?)_\b/, (inner) => `<em>${inner}</em>`],
  [/~~([\s\S]+?)~~/, (inner) => `<s>${inner}</s>`],
  [/\|\|([\s\S]+?)\|\|/, (inner) => `<span class="spoiler">${inner}</span>`],
];

function renderInline(text, depth = 0) {
  if (depth > 8) return text;
  let best = null;
  for (const [regex, wrap] of INLINE_RULES) {
    const match = regex.exec(text);
    if (match && (!best || match.index < best.match.index)) best = { match, wrap };
  }
  if (!best) return text;
  const { match, wrap } = best;
  const before = text.slice(0, match.index);
  const after = text.slice(match.index + match[0].length);
  return before + wrap(renderInline(match[1], depth + 1)) + renderInline(after, depth);
}

// Lignes : titres (#, ##, ###), sous-texte (-#), citations (>) ; le reste est séparé par des <br>.
function renderLines(text) {
  const blocks = [];
  let quote = [];
  const flushQuote = () => {
    if (quote.length) blocks.push({ block: true, html: `<blockquote>${quote.join('<br>')}</blockquote>` });
    quote = [];
  };
  for (const line of text.split('\n')) {
    const quoted = line.match(/^&gt; (.*)$/) ?? (line === '&gt;' ? ['', ''] : null);
    if (quoted) {
      quote.push(renderInline(quoted[1]));
      continue;
    }
    flushQuote();
    const heading = line.match(/^(#{1,3}) (.+)$/);
    const subtext = line.match(/^-# (.+)$/);
    if (heading) {
      blocks.push({ block: true, html: `<div class="h${heading[1].length}">${renderInline(heading[2])}</div>` });
    } else if (subtext) blocks.push({ block: true, html: `<div class="subtext">${renderInline(subtext[1])}</div>` });
    else blocks.push({ block: false, html: renderInline(line) });
  }
  flushQuote();
  return blocks
    .map((item, i) => (i > 0 && !item.block && !blocks[i - 1].block ? `<br>${item.html}` : item.html))
    .join('');
}

const URL_CHARS = '(?:[^\\s&\\u0000]|&amp;)+';
const BARE_URL = new RegExp(`&lt;(https?:\\/\\/${URL_CHARS})&gt;|(https?:\\/\\/${URL_CHARS})`, 'g');
const MASKED_LINK = new RegExp(`\\[([^\\]\\n]{1,256})\\]\\((?:&lt;)?(https?:\\/\\/${URL_CHARS}?)(?:&gt;)?\\)`, 'g');

// Échappe TOUT le texte puis applique le markdown ; mentions = { users, roles, channels } (id → nom).
export function renderMarkdown(text, mentions = {}) {
  const stash = [];
  const keep = (html) => `${MARK}${stash.push(html) - 1}${MARK}`;
  const link = (url, label) => {
    const href = safeUrl(unescapeHtml(url));
    return href ? keep(`<a href="${escapeHtml(href)}" target="_blank" rel="noopener noreferrer">${label}</a>`) : null;
  };

  let out = String(text ?? '').replaceAll(MARK, '');
  out = out.replace(/```(?:([\w+#.-]{1,20})\n)?([\s\S]*?)```/g, (_, lang, code) =>
    keep(`<pre class="codeblock"><code>${escapeHtml(code.replace(/^\n+|\n+$/g, ''))}</code></pre>`),
  );
  out = out.replace(/``([^`]+?)``|`([^`\n]+?)`/g, (_, a, b) => keep(`<code>${escapeHtml(a ?? b)}</code>`));
  out = escapeHtml(out);

  const name = (map, id, fallback) => escapeHtml(map?.[id] ?? fallback);
  out = out
    .replace(/&lt;@!?(\d{15,25})&gt;/g, (_, id) =>
      keep(`<span class="mention">@${name(mentions.users, id, 'unknown-user')}</span>`),
    )
    .replace(/&lt;@&amp;(\d{15,25})&gt;/g, (_, id) =>
      keep(`<span class="mention">@${name(mentions.roles, id, 'deleted-role')}</span>`),
    )
    .replace(/&lt;#(\d{15,25})&gt;/g, (_, id) =>
      keep(`<span class="mention">#${name(mentions.channels, id, 'deleted-channel')}</span>`),
    )
    .replace(/&lt;(a?):(\w{2,32}):(\d{15,25})&gt;/g, (_, animated, emoji, id) =>
      keep(
        `<img class="emoji" src="https://cdn.discordapp.com/emojis/${id}.${animated ? 'gif' : 'png'}" ` +
          `alt=":${emoji}:" title=":${emoji}:">`,
      ),
    )
    .replace(/&lt;t:(-?\d{1,13})(?::[tTdDfFR])?&gt;/g, (_, seconds) =>
      keep(`<span class="timestamp">${escapeHtml(formatDate(Number(seconds) * 1000))}</span>`),
    )
    .replace(/&lt;\/([\w -]{1,32}):\d{15,25}&gt;/g, (_, command) => keep(`<span class="mention">/${command}</span>`))
    .replace(MASKED_LINK, (match, label, url) => link(url, label) ?? match)
    .replace(BARE_URL, (match, wrapped, bare) => {
      if (wrapped) return link(wrapped, wrapped) ?? match;
      const trail = bare.match(/[.,;:!?)\]]+$/)?.[0] ?? '';
      const url = trail ? bare.slice(0, -trail.length) : bare;
      return (link(url, url) ?? url) + trail;
    });

  out = renderLines(out);
  // Les fragments peuvent en contenir d'autres (ex. code dans un lien masqué).
  for (let i = 0; i < 5 && out.includes(MARK); i += 1) {
    out = out.replace(/\u0000(\d+)\u0000/g, (_, index) => stash[Number(index)] ?? '');
  }
  return out;
}

// ---------- Rendu HTML ----------

const CSS = `
*{box-sizing:border-box}
html,body{margin:0;background:${HEX.bg};color:${HEX.text}}
body{font:15px/1.45 "Segoe UI",system-ui,-apple-system,Roboto,"Helvetica Neue",Arial,sans-serif}
a{color:${HEX.blueText};text-decoration:none}
a:hover{text-decoration:underline}
.wrap{max-width:1000px;margin:0 auto;padding:24px 16px 48px}
.header{background:linear-gradient(135deg,${HEX.surface},${HEX.bg2});border:1px solid ${HEX.line};
  border-radius:14px;padding:20px 22px;margin-bottom:18px}
.brand{display:flex;align-items:center;gap:12px;margin-bottom:16px}
.guild-icon{width:48px;height:48px;border-radius:50%;background:${HEX.blue};display:flex;align-items:center;
  justify-content:center;font-weight:700;font-size:20px;overflow:hidden;flex:none}
.guild-icon img{width:100%;height:100%;object-fit:cover}
.guild{font-weight:700;font-size:17px}
.kicker{color:${HEX.muted};font-size:12px;text-transform:uppercase;letter-spacing:.08em}
h1{margin:0 0 16px;font-size:24px;line-height:1.2}
.meta{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:12px 20px;margin:0}
.meta dt{color:${HEX.muted};font-size:11px;text-transform:uppercase;letter-spacing:.07em}
.meta dd{margin:3px 0 0;overflow-wrap:anywhere}
.box{margin-top:16px;padding:12px 14px;background:${HEX.bg};border-left:3px solid ${HEX.blue};border-radius:6px}
.box .label{color:${HEX.muted};font-size:11px;text-transform:uppercase;letter-spacing:.07em;margin-bottom:4px}
.notice{margin-top:16px;color:${HEX.gold};font-size:13px}
.messages{background:${HEX.surface};border:1px solid ${HEX.line};border-radius:14px;padding:6px 0 12px}
.empty{color:${HEX.muted};text-align:center;padding:28px}
.msg{display:flex;gap:14px;padding:3px 18px}
.msg.first{margin-top:12px;padding-top:4px}
.msg:hover{background:${HEX.surface2}}
.avatar{width:40px;height:40px;border-radius:50%;flex:none;background:${HEX.surface2};object-fit:cover}
.gutter{width:40px;flex:none;color:transparent;font-size:10px;text-align:right;padding-top:4px;white-space:nowrap}
.msg:hover .gutter{color:${HEX.muted}}
.body{min-width:0;flex:1}
.head{display:flex;align-items:baseline;gap:8px;flex-wrap:wrap}
.name{font-weight:600;color:${HEX.text}}
.tag{background:${HEX.blue};color:#fff;font-size:10px;font-weight:700;padding:1px 5px;border-radius:4px;
  letter-spacing:.04em;align-self:center}
time,.edited{color:${HEX.muted};font-size:12px}
.reply{font-size:13px;color:${HEX.muted};margin-bottom:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.reply b{color:${HEX.text};font-weight:600}
.content{overflow-wrap:anywhere}
.system{color:${HEX.muted};font-style:italic}
code{background:${HEX.bg};border:1px solid ${HEX.line};border-radius:4px;padding:1px 4px;
  font:13px/1.4 Consolas,"Courier New",monospace}
pre.codeblock{background:${HEX.bg};border:1px solid ${HEX.line};border-radius:6px;padding:10px 12px;margin:6px 0;
  white-space:pre-wrap;overflow-wrap:anywhere}
pre.codeblock code{background:none;border:0;padding:0}
blockquote{margin:4px 0;padding:0 0 0 12px;border-left:4px solid ${HEX.line}}
.h1{font-size:22px;font-weight:700;margin:6px 0 2px}
.h2{font-size:19px;font-weight:700;margin:6px 0 2px}
.h3{font-size:16px;font-weight:700;margin:6px 0 2px}
.subtext{color:${HEX.muted};font-size:12px}
.mention{background:rgba(42,91,255,.22);color:${HEX.blueText};border-radius:3px;padding:0 2px;font-weight:500}
.spoiler{background:${HEX.line};color:transparent;border-radius:3px}
.spoiler:hover{background:rgba(31,49,112,.5);color:inherit}
.emoji{width:22px;height:22px;vertical-align:-5px;object-fit:contain}
.embed{display:flex;gap:12px;margin-top:6px;max-width:520px;background:${HEX.bg2};border-left:4px solid var(--c);
  border-radius:4px;padding:10px 14px 12px}
.embed-main{min-width:0;flex:1}
.embed-author{font-size:13px;font-weight:600}
.embed-title{font-weight:700;margin-top:4px}
.embed-desc{font-size:14px;margin-top:4px}
.fields{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px 12px;margin-top:8px}
.field{grid-column:1/-1}
.field.inline{grid-column:auto}
.field-name{font-size:13px;font-weight:700}
.field-value{font-size:14px}
.embed-thumb{width:80px;height:80px;object-fit:contain;border-radius:4px;flex:none}
.embed-image{display:block;max-width:100%;border-radius:4px;margin-top:10px}
.embed-footer{font-size:12px;color:${HEX.muted};margin-top:8px}
.attachments{display:flex;flex-direction:column;align-items:flex-start;gap:6px;margin-top:6px}
.attachment-img{display:block;max-width:min(400px,100%);max-height:300px;border-radius:6px}
.file{display:inline-flex;gap:10px;align-items:center;background:${HEX.bg2};border:1px solid ${HEX.line};
  border-radius:8px;padding:10px 12px;max-width:100%}
.file small{color:${HEX.muted}}
.sticker{display:inline-block;margin-top:6px;padding:6px 10px;border:1px dashed ${HEX.line};border-radius:8px;
  color:${HEX.muted};font-size:13px}
footer{color:${HEX.muted};font-size:12px;text-align:center;margin-top:18px}
@media (max-width:600px){.fields{grid-template-columns:1fr}.field.inline{grid-column:1/-1}.msg{padding:3px 10px}}
`;

const IMAGE_EXT = /\.(png|jpe?g|gif|webp|avif|bmp)$/i;

// Lien http(s) autour d'un fragment déjà rendu, sinon le fragment seul.
const linkTo = (url, html) => {
  const href = safeUrl(url);
  return href ? `<a href="${escapeHtml(href)}" target="_blank" rel="noopener noreferrer">${html}</a>` : html;
};

function renderEmbed(embed, mentions) {
  const color = hexColor(embed.color) ?? HEX.line;
  const parts = [];
  if (embed.author?.name) {
    parts.push(`<div class="embed-author">${linkTo(embed.author.url, escapeHtml(embed.author.name))}</div>`);
  }
  if (embed.title) {
    parts.push(`<div class="embed-title">${linkTo(embed.url, renderMarkdown(embed.title, mentions))}</div>`);
  }
  if (embed.description) parts.push(`<div class="embed-desc">${renderMarkdown(embed.description, mentions)}</div>`);
  if (embed.fields?.length) {
    const fields = embed.fields.map(
      (field) =>
        `<div class="field${field.inline ? ' inline' : ''}">` +
        `<div class="field-name">${renderMarkdown(field.name, mentions)}</div>` +
        `<div class="field-value">${renderMarkdown(field.value, mentions)}</div></div>`,
    );
    parts.push(`<div class="fields">${fields.join('')}</div>`);
  }
  const image = safeUrl(embed.image);
  if (image) parts.push(`<img class="embed-image" src="${escapeHtml(image)}" alt="" loading="lazy">`);
  const footer = [embed.footer, embed.timestamp ? formatDate(embed.timestamp) : null].filter(Boolean).join(' • ');
  if (footer) parts.push(`<div class="embed-footer">${escapeHtml(footer)}</div>`);
  const thumb = safeUrl(embed.thumbnail);
  const thumbHtml = thumb ? `<img class="embed-thumb" src="${escapeHtml(thumb)}" alt="" loading="lazy">` : '';
  return `<div class="embed" style="--c:${color}"><div class="embed-main">${parts.join('')}</div>${thumbHtml}</div>`;
}

function renderAttachment(file) {
  const url = safeUrl(file.url);
  const name = escapeHtml(file.name || 'file');
  if (!url) return `<div class="file">📎 ${name}</div>`;
  const href = escapeHtml(url);
  const isImage = String(file.contentType ?? '').startsWith('image/') || IMAGE_EXT.test(file.name ?? '');
  if (isImage) {
    return (
      `<a href="${href}" target="_blank" rel="noopener noreferrer">` +
      `<img class="attachment-img" src="${href}" alt="${name}" loading="lazy"></a>`
    );
  }
  const size = formatSize(file.size);
  return (
    `<div class="file">📎 <a href="${href}" target="_blank" rel="noopener noreferrer">${name}</a>` +
    `${size ? ` <small>${escapeHtml(size)}</small>` : ''}</div>`
  );
}

function renderMessage(message, previous, byId, mentions) {
  const author = message.author ?? {};
  const grouped =
    previous &&
    !message.replyTo &&
    !message.system &&
    !previous.system &&
    previous.author?.id === author.id &&
    message.timestamp - previous.timestamp < GROUP_WINDOW_MS;

  const date = validDate(message.timestamp);
  const body = [];
  if (message.replyTo) {
    const target = byId.get(message.replyTo);
    const text = String(target?.content || (target?.attachments?.length ? '📎 Attachment' : '…'));
    const snippet = text.replace(/\s+/g, ' ').slice(0, 120);
    body.push(
      target
        ? `<div class="reply">↪ Replying to <b>${escapeHtml(target.author?.name)}</b> ${escapeHtml(snippet)}</div>`
        : '<div class="reply">↪ Replying to a deleted message</div>',
    );
  }
  if (!grouped) {
    const color = hexColor(author.color);
    body.push(
      '<div class="head">' +
        `<span class="name"${color ? ` style="color:${color}"` : ''} title="${escapeHtml(author.username ?? '')}">` +
        `${escapeHtml(author.name ?? 'Unknown')}</span>` +
        `${author.bot ? '<span class="tag">BOT</span>' : ''}` +
        `<time datetime="${date?.toISOString() ?? ''}">${escapeHtml(formatDate(message.timestamp))}</time>` +
        '</div>',
    );
  }
  if (message.content) {
    const edited = message.editedTimestamp
      ? ` <span class="edited" title="${escapeHtml(formatDate(message.editedTimestamp))}">(edited)</span>`
      : '';
    const classes = message.system ? 'content system' : 'content';
    body.push(`<div class="${classes}">${renderMarkdown(message.content, mentions)}${edited}</div>`);
  } else if (message.system) {
    body.push(`<div class="content system">System message: ${escapeHtml(message.systemType ?? 'unknown')}</div>`);
  }
  for (const embed of message.embeds ?? []) body.push(renderEmbed(embed, mentions));
  if (message.attachments?.length) {
    body.push(`<div class="attachments">${message.attachments.map(renderAttachment).join('')}</div>`);
  }
  for (const sticker of message.stickers ?? []) {
    body.push(`<div class="sticker">Sticker: ${escapeHtml(sticker.name ?? 'unknown')}</div>`);
  }

  const avatarUrl = safeUrl(author.avatarUrl);
  let side = '<div class="avatar"></div>';
  if (grouped) side = `<div class="gutter">${date ? TIME_FORMAT.format(date) : ''}</div>`;
  else if (avatarUrl) side = `<img class="avatar" src="${escapeHtml(avatarUrl)}" alt="" loading="lazy">`;
  const attrs = `class="msg${grouped ? '' : ' first'}" id="m-${escapeHtml(message.id ?? '')}"`;
  return `<div ${attrs}>${side}<div class="body">${body.join('')}</div></div>`;
}

const personLabel = (person) => {
  if (!person) return '—';
  const name = escapeHtml(person.name ?? person.id);
  if (!person.username || person.username === person.name) return name;
  return `${name} <small>(@${escapeHtml(person.username)})</small>`;
};

// meta : { guildName, guildIconUrl, channelName, number, category, subject, details, openedBy, claimedBy, closedBy,
//          openedAt, closedAt, generatedAt, truncated, mentions } ; personne = { id, name, username }.
// messages : objets simples (voir serializeMessage), du plus ancien au plus récent.
export function renderTranscriptHtml(meta = {}, messages = []) {
  const mentions = meta.mentions ?? {};
  const number = padNumber(meta.number);
  const category = meta.category ?? 'Ticket';
  const guildName = meta.guildName ?? BRAND;
  const icon = safeUrl(meta.guildIconUrl);
  const initial = escapeHtml([...guildName.trim()][0]?.toUpperCase() ?? '?');
  const duration = meta.openedAt && meta.closedAt ? formatDuration(meta.closedAt - meta.openedAt) : '—';

  const metaItems = [
    ['Subject', escapeHtml(meta.subject || '—')],
    ['Category', escapeHtml(category)],
    ['Channel', meta.channelName ? `#${escapeHtml(meta.channelName)}` : '—'],
    ['Opened by', personLabel(meta.openedBy)],
    ['Claimed by', meta.claimedBy ? personLabel(meta.claimedBy) : 'Unclaimed'],
    ['Closed by', personLabel(meta.closedBy)],
    ['Opened', escapeHtml(formatDate(meta.openedAt))],
    ['Closed', escapeHtml(formatDate(meta.closedAt))],
    ['Duration', escapeHtml(duration)],
    ['Messages', String(messages.length)],
  ];

  const byId = new Map(messages.map((m) => [m.id, m]));
  const rendered = messages.map((message, i) => renderMessage(message, messages[i - 1], byId, mentions));
  const details = meta.details
    ? `<div class="box"><div class="label">Details</div>${renderMarkdown(meta.details, mentions)}</div>`
    : '';
  const notice = meta.truncated
    ? `<div class="notice">⚠️ Only the latest ${MAX_TRANSCRIPT_MESSAGES} messages are included.</div>`
    : '';
  const site = `<a href="${escapeHtml(SITE_URL)}">${escapeHtml(new URL(SITE_URL).host)}</a>`;
  const generated = escapeHtml(formatDate(meta.generatedAt ?? Date.now()));

  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="dark">
<meta name="referrer" content="no-referrer">
<title>Ticket #${number} · ${escapeHtml(guildName)}</title>
<style>${CSS}</style>
</head>
<body>
<div class="wrap">
<header class="header">
<div class="brand">
<div class="guild-icon">${icon ? `<img src="${escapeHtml(icon)}" alt="">` : initial}</div>
<div><div class="guild">${escapeHtml(guildName)}</div><div class="kicker">Ticket transcript</div></div>
</div>
<h1>🎫 Ticket #${number} — ${escapeHtml(category)}</h1>
<dl class="meta">
${metaItems.map(([label, value]) => `<div><dt>${label}</dt><dd>${value}</dd></div>`).join('\n')}
</dl>
${details}
${notice}
</header>
<main class="messages">
${rendered.length ? rendered.join('\n') : '<div class="empty">No messages.</div>'}
</main>
<footer>Generated on ${generated} by ${escapeHtml(BRAND)} · ${site}</footer>
</div>
</body>
</html>
`;
}

// ---------- Récupération Discord ----------

// Tous les messages du salon, 100 par 100 (paramètre before), du plus ancien au plus récent.
export async function fetchAllMessages(channel, max = MAX_TRANSCRIPT_MESSAGES) {
  const messages = [];
  let before;
  let truncated = false;
  while (messages.length < max) {
    const limit = Math.min(100, max - messages.length);
    // Une erreur remonte : mieux vaut pas de transcript qu'un transcript incomplet sans le dire (tickets.mjs garde
    // alors le salon au lieu de le supprimer).
    const batch = await channel.messages.fetch({ limit, before, cache: false });
    if (!batch?.size) break;
    messages.push(...batch.values());
    before = [...batch.keys()].reduce((a, b) => (BigInt(a) < BigInt(b) ? a : b));
    if (batch.size < limit) break;
    if (messages.length >= max) truncated = true;
  }
  messages.sort((a, b) => (BigInt(a.id) < BigInt(b.id) ? -1 : 1));
  return { messages, truncated };
}

// Message discord.js → objet simple pour renderTranscriptHtml.
export function serializeMessage(message) {
  const member = message.member;
  const author = message.author;
  return {
    id: message.id,
    timestamp: message.createdTimestamp,
    editedTimestamp: message.editedTimestamp ?? null,
    author: {
      id: author?.id ?? null,
      name: member?.displayName ?? author?.displayName ?? author?.username ?? 'Unknown',
      username: author?.username ?? null,
      avatarUrl: (member ?? author)?.displayAvatarURL?.({ size: 64 }) ?? null,
      bot: Boolean(author?.bot),
      color: member?.displayHexColor ?? null,
    },
    content: message.content ?? '',
    system: Boolean(message.system),
    systemType: message.system ? (MessageType[message.type] ?? String(message.type)) : null,
    replyTo: message.reference?.messageId && message.reference.channelId === message.channelId
      ? message.reference.messageId
      : null,
    embeds: message.embeds.map((embed) => ({
      title: embed.title ?? null,
      url: embed.url ?? null,
      description: embed.description ?? null,
      color: embed.color ?? null,
      author: embed.author ? { name: embed.author.name, url: embed.author.url ?? null } : null,
      fields: embed.fields.map((field) => ({ name: field.name, value: field.value, inline: Boolean(field.inline) })),
      footer: embed.footer?.text ?? null,
      image: embed.image?.url ?? null,
      thumbnail: embed.thumbnail?.url ?? null,
      timestamp: embed.timestamp ? Date.parse(embed.timestamp) : null,
    })),
    attachments: [...message.attachments.values()].map((file) => ({
      name: file.name,
      url: file.url,
      contentType: file.contentType ?? null,
      size: file.size,
    })),
    stickers: [...message.stickers.values()].map((sticker) => ({ name: sticker.name })),
  };
}

// Noms des mentions présentes dans les textes (mentions fournies par Discord, puis caches du serveur).
function collectMentions(guild, messages) {
  const mentions = { users: {}, roles: {}, channels: {} };
  const texts = [];
  for (const message of messages) {
    for (const user of message.mentions.users.values()) {
      mentions.users[user.id] = message.mentions.members?.get(user.id)?.displayName ?? user.displayName;
    }
    for (const role of message.mentions.roles.values()) mentions.roles[role.id] = role.name;
    texts.push(message.content ?? '');
    for (const embed of message.embeds) {
      texts.push(embed.title ?? '', embed.description ?? '', ...embed.fields.flatMap((f) => [f.name, f.value]));
    }
  }
  const all = texts.join('\n');
  for (const [, id] of all.matchAll(/<@!?(\d{15,25})>/g)) {
    mentions.users[id] ??=
      guild.members.cache.get(id)?.displayName ?? guild.client.users.cache.get(id)?.displayName ?? undefined;
  }
  for (const [, id] of all.matchAll(/<@&(\d{15,25})>/g)) mentions.roles[id] ??= guild.roles.cache.get(id)?.name;
  for (const [, id] of all.matchAll(/<#(\d{15,25})>/g)) mentions.channels[id] ??= guild.channels.cache.get(id)?.name;
  return mentions;
}

async function resolvePerson(guild, userId) {
  if (!userId) return null;
  const member = guild.members.cache.get(userId) ?? (await guild.members.fetch(userId).catch(() => null));
  const user = member?.user ?? (await guild.client.users.fetch(userId).catch(() => null));
  return { id: userId, name: member?.displayName ?? user?.displayName ?? userId, username: user?.username ?? null };
}

// Transcript complet d'un ticket (ligne de la table tickets) → { buffer, messageCount }.
export async function buildTranscript(channel, ticket) {
  const { messages, truncated } = await fetchAllMessages(channel);
  const guild = channel.guild;
  const [openedBy, claimedBy, closedBy] = await Promise.all(
    [ticket.user_id, ticket.claimed_by, ticket.closed_by].map((id) => resolvePerson(guild, id)),
  );
  const meta = {
    guildName: guild.name,
    guildIconUrl: guild.iconURL({ size: 128 }),
    channelName: channel.name,
    number: ticket.number,
    category: categoryLabel(ticket.category),
    subject: ticket.subject,
    details: ticket.details,
    openedBy,
    claimedBy,
    closedBy,
    openedAt: ticket.created_at,
    closedAt: ticket.closed_at ?? Date.now(),
    generatedAt: Date.now(),
    truncated,
    mentions: collectMentions(guild, messages),
  };
  const html = renderTranscriptHtml(meta, messages.map(serializeMessage));
  return { buffer: Buffer.from(html, 'utf8'), messageCount: messages.length };
}
