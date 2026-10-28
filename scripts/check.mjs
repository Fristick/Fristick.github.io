// `npm run check` : vérifie le bot hors ligne, sans jeton ni connexion à Discord.
//  - syntaxe de chaque fichier ; chargement de chaque module et des fonctions attendues entre modules ;
//  - slash commands valides pour Discord (toJSON) ;
//  - formule d'XP, base SQLite en mémoire, variables des messages, transcript HTML (échappement) ;
//  - rendu de la carte de rang et de l'image de bienvenue en PNG (dossier affiché à la fin).
// Option --images <dossier> : où écrire les PNG (par défaut un dossier temporaire).
import { spawnSync } from 'node:child_process';
import { readdir, mkdir, writeFile } from 'node:fs/promises';
import { EventEmitter } from 'node:events';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

process.env.DB_PATH = ':memory:'; // avant tout import de src/db.mjs
process.removeAllListeners('warning');
process.on('warning', (w) => w.name === 'ExperimentalWarning' || console.warn(w));

const ROOT = fileURLToPath(new URL('..', import.meta.url));
const imagesArg = process.argv.indexOf('--images');
const IMAGES = imagesArg > 0 ? path.resolve(process.argv[imagesArg + 1]) : path.join(os.tmpdir(), 'renard-bot-check');

let failures = 0;
const ok = (msg) => console.log(`  ✔ ${msg}`);
const fail = (msg, error) => {
  failures++;
  console.log(`  ✘ ${msg}${error ? ` — ${error.message ?? error}` : ''}`);
};
async function step(title, fn) {
  console.log(`\n${title}`);
  try {
    await fn();
  } catch (error) {
    fail('erreur inattendue', error);
    if (process.env.DEBUG) console.error(error);
  }
}
const load = (rel) => import(pathToFileURL(path.join(ROOT, rel)).href);
function assert(condition, message) {
  if (condition) ok(message);
  else fail(message);
}

async function* jsFiles(dir) {
  for (const d of await readdir(dir, { withFileTypes: true })) {
    const abs = path.join(dir, d.name);
    if (d.isDirectory()) yield* jsFiles(abs);
    else if (/\.(m?js)$/.test(d.name)) yield abs;
  }
}

await step('Syntaxe', async () => {
  const files = [path.join(ROOT, 'index.js')];
  for (const dir of ['src', 'scripts']) for await (const f of jsFiles(path.join(ROOT, dir))) files.push(f);
  for (const file of files) {
    const res = spawnSync(process.execPath, ['--check', file], { encoding: 'utf8' });
    if (res.status !== 0) fail(path.relative(ROOT, file), new Error(res.stderr.split('\n').slice(0, 4).join(' ')));
  }
  ok(`${files.length} fichiers analysés`);
});

const COMMANDS = ['setup', 'rank', 'leaderboard', 'xp', 'ticket', 'help'];
await step('Slash commands', async () => {
  for (const name of COMMANDS) {
    try {
      const mod = await load(`src/commands/${name}.mjs`);
      const json = mod.data.toJSON();
      if (typeof mod.execute !== 'function') throw new Error('execute() manquant');
      if (json.name !== name) throw new Error(`nom « ${json.name} »`);
      const size = JSON.stringify(json).length;
      ok(`/${name} (${json.options?.length ?? 0} options, ${size} o)`);
    } catch (error) {
      fail(`/${name}`, error);
    }
  }
});

// Fonctions dont dépendent les autres modules (/setup notamment).
const CONTRACT = {
  'features/welcome.mjs': ['register', 'sendWelcome'],
  'features/verification.mjs': ['register', 'postVerificationPanel', 'restrictEveryone'],
  'features/levels.mjs': [
    'register', 'xpForLevel', 'levelFromXp', 'totalXpForLevel', 'getStats', 'addXp', 'setXp', 'resetXp',
    'syncRewardRoles', 'buildRankSystemEmbed', 'rewardsOf', 'rankRoleFor', 'syncAllRewardRoles', 'createRankRoles',
  ],
  'features/tickets.mjs': ['register', 'postTicketPanel'],
  'features/transcript.mjs': ['buildTranscript', 'renderTranscriptHtml'],
  'features/logs.mjs': ['register'],
  'features/tempVoice.mjs': ['register'],
  'features/boosts.mjs': ['register', 'sendBoostMessage'],
  'features/setupPanel.mjs': ['register', 'showSetupPanel'],
  'features/rules.mjs': ['postRulesPanel', 'buildRulesPayload'],
  'features/rankCard.mjs': ['renderRankCard'],
  'features/welcomeCard.mjs': ['renderWelcomeCard'],
};
const mods = {};
await step('Modules et fonctions attendues', async () => {
  const fakeClient = new EventEmitter();
  for (const [file, names] of Object.entries(CONTRACT)) {
    try {
      const mod = (mods[file] = await load(`src/${file}`));
      const missing = names.filter((n) => typeof mod[n] !== 'function');
      if (missing.length) throw new Error(`manque ${missing.join(', ')}`);
      mod.register?.(fakeClient);
      ok(file);
    } catch (error) {
      fail(file, error);
    }
  }
  ok(`${fakeClient.eventNames().length} événements écoutés : ${fakeClient.eventNames().join(', ')}`);
});

await step('Formule d’XP', async () => {
  const lv = mods['features/levels.mjs'];
  if (!lv) return fail('levels.mjs non chargé');
  const firstLevels = [0, 1, 2].map(lv.xpForLevel).join('/');
  assert(firstLevels === '100/155/220', `xpForLevel 0/1/2 = 100/155/220 (${firstLevels})`);
  const a = lv.levelFromXp(0);
  assert(a.level === 0 && a.xpIntoLevel === 0 && a.xpForNext === 100, 'levelFromXp(0) → niveau 0, 0/100');
  const b = lv.levelFromXp(254);
  assert(b.level === 1 && b.xpIntoLevel === 154 && b.xpForNext === 155, 'levelFromXp(254) → niveau 1, 154/155');
  const c = lv.levelFromXp(255);
  assert(c.level === 2 && c.xpIntoLevel === 0, 'levelFromXp(255) → niveau 2');
  let coherent = true;
  for (let l = 0; l <= 200; l++) {
    if (lv.levelFromXp(lv.totalXpForLevel(l)).level !== l) coherent = false;
  }
  assert(coherent, 'totalXpForLevel et levelFromXp cohérents jusqu’au niveau 200');
});

await step('Base de données', async () => {
  const { getConfig, updateConfig } = await load('src/db.mjs');
  const cfg = getConfig('1');
  assert(cfg.levels.enabled === true && cfg.welcome.channelId === null, 'valeurs par défaut');
  updateConfig('1', (c) => {
    c.welcome.channelId = '42';
    c.levels.rewards.push({ level: 5, roleId: '7' });
  });
  const again = getConfig('1');
  assert(again.welcome.channelId === '42' && again.levels.rewards.length === 1, 'enregistrement et relecture');
  assert(again.welcome.title === cfg.welcome.title, 'les autres réglages gardent leur valeur par défaut');
  assert(getConfig('2').levels.rewards.length === 0, 'les serveurs ne partagent pas leurs réglages');
});

await step('Variables des messages', async () => {
  const { fill } = await load('src/utils/placeholders.mjs');
  const guild = { name: 'Renard Studio', memberCount: 128 };
  const member = { displayName: 'Foxy', guild, user: { id: '99', username: 'foxy' } };
  const out = fill('{user} {username} {tag} {server} #{memberCount} lvl {level} {unknown}', { member, level: 3 });
  assert(out === '<@99> Foxy foxy Renard Studio #128 lvl 3 {unknown}', `« ${out} »`);
});

await step('Règlement', async () => {
  const rules = mods['features/rules.mjs'];
  if (!rules) return fail('rules.mjs non chargé');
  const { DEFAULT_CONFIG, DEFAULT_RULES } = await load('src/config.mjs');
  assert(DEFAULT_RULES.length <= 4000, `texte par défaut ≤ 4000 caractères (${DEFAULT_RULES.length})`);
  const guild = { id: '1', name: 'Renard Studio', memberCount: 128, channels: { cache: new Map([['55', {}]]) } };
  const cfg = structuredClone(DEFAULT_CONFIG);
  cfg.tickets.panelChannelId = '55';
  const payload = rules.buildRulesPayload(guild, cfg);
  const embed = payload.embeds[0].toJSON();
  const rows = payload.components.map((r) => r.toJSON());
  assert(embed.description.length <= 4096, `embed ≤ 4096 caractères (${embed.description.length})`);
  assert(!/\{(server|tickets)\}/.test(embed.description), 'variables remplacées');
  const desc = embed.description;
  assert(desc.includes('<#55>') && desc.includes('**Renard Studio**'), 'salon des tickets et nom du serveur');
  assert(/racism/i.test(embed.description) && embed.description.includes('discord.com/guidelines'), 'contenu attendu');
  assert(rows[0].components.length === 3 && rows[0].components.every((b) => b.url), '3 boutons-liens');
  const bare = { ...guild, channels: { cache: new Map() } };
  const noTickets = rules.buildRulesPayload(bare, structuredClone(DEFAULT_CONFIG));
  assert(noTickets.embeds[0].toJSON().description.includes('our support channel'), 'sans tickets : texte de repli');
});

await step('Transcript HTML', async () => {
  const tr = mods['features/transcript.mjs'];
  if (!tr) return fail('transcript.mjs non chargé');
  const html = await tr.renderTranscriptHtml(
    {
      guildName: 'Renard Studio',
      number: 42,
      category: 'Support',
      subject: '<b>Help</b>',
      openedAt: Date.now() - 3_600_000,
      closedAt: Date.now(),
    },
    [
      {
        id: '1',
        timestamp: Date.now(),
        author: { id: '99', name: 'Foxy', username: 'foxy', avatarUrl: null, bot: false, color: null },
        content: 'Hello <script>alert(1)</script> **bold**',
        system: false,
        embeds: [],
        attachments: [],
        stickers: [],
      },
    ],
  );
  const text = String(html);
  assert(text.includes('<html') || text.includes('<!doctype') || text.includes('<!DOCTYPE'), 'document HTML');
  assert(!text.includes('<script>alert(1)</script>'), 'contenu échappé (pas de <script> injecté)');
});

await step('Images', async () => {
  await mkdir(IMAGES, { recursive: true });
  const rank = mods['features/rankCard.mjs'];
  const welcome = mods['features/welcomeCard.mjs'];
  if (rank) {
    const png = await rank.renderRankCard({
      username: 'Renard', avatarURL: null, level: 12, rank: 3, xpIntoLevel: 640, xpForNext: 1_420,
      totalXp: 15_230, messages: 1_234, voiceSeconds: 12_000,
    });
    assert(png?.subarray(1, 4).toString() === 'PNG', 'carte de rang');
    await writeFile(path.join(IMAGES, 'rank.png'), png);
  } else fail('rankCard.mjs non chargé');
  if (welcome) {
    const png = await welcome.renderWelcomeCard({
      username: 'Renard', avatarURL: null, serverName: 'Renard Studio', memberCount: 128,
    });
    assert(png?.subarray(1, 4).toString() === 'PNG', 'image de bienvenue');
    await writeFile(path.join(IMAGES, 'welcome.png'), png);
  } else fail('welcomeCard.mjs non chargé');
  console.log(`  → ${IMAGES}`);
});

console.log(failures ? `\n✘ ${failures} problème(s)` : '\n✔ Tout est bon');
process.exit(failures ? 1 : 0);
