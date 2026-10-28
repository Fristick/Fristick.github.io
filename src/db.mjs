// Base SQLite (node:sqlite, intégré à Node 22) : réglages par serveur, XP, tickets, salons vocaux temporaires.
// Le fichier est dans DATA_DIR (par défaut ./data) : à sauvegarder avec les backups Pterodactyl.
import { DatabaseSync } from 'node:sqlite';
import { mkdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { DEFAULT_CONFIG } from './config.mjs';

const ROOT = fileURLToPath(new URL('..', import.meta.url));
export const DATA_DIR = path.resolve(ROOT, process.env.DATA_DIR || 'data');

function open() {
  if (process.env.DB_PATH === ':memory:') return new DatabaseSync(':memory:');
  mkdirSync(DATA_DIR, { recursive: true });
  return new DatabaseSync(path.join(DATA_DIR, 'bot.db'));
}

export const db = open();

db.exec(`
  PRAGMA journal_mode = WAL;
  PRAGMA busy_timeout = 5000;

  CREATE TABLE IF NOT EXISTS guild_config (
    guild_id TEXT PRIMARY KEY,
    data     TEXT NOT NULL
  );

  -- XP total par membre ; le niveau en est déduit (features/levels.mjs).
  CREATE TABLE IF NOT EXISTS levels (
    guild_id        TEXT NOT NULL,
    user_id         TEXT NOT NULL,
    xp              INTEGER NOT NULL DEFAULT 0,
    messages        INTEGER NOT NULL DEFAULT 0,
    voice_seconds   INTEGER NOT NULL DEFAULT 0,
    last_message_at INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (guild_id, user_id)
  );
  CREATE INDEX IF NOT EXISTS levels_rank ON levels (guild_id, xp DESC);

  CREATE TABLE IF NOT EXISTS tickets (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    guild_id   TEXT NOT NULL,
    number     INTEGER NOT NULL,
    channel_id TEXT UNIQUE,
    user_id    TEXT NOT NULL,
    category   TEXT NOT NULL,
    subject    TEXT,
    details    TEXT,
    claimed_by TEXT,
    status     TEXT NOT NULL DEFAULT 'open', -- open | closed
    created_at INTEGER NOT NULL,
    closed_at  INTEGER,
    closed_by  TEXT
  );
  CREATE INDEX IF NOT EXISTS tickets_user ON tickets (guild_id, user_id, status);

  CREATE TABLE IF NOT EXISTS temp_voices (
    channel_id       TEXT PRIMARY KEY,
    guild_id         TEXT NOT NULL,
    owner_id         TEXT NOT NULL,
    panel_message_id TEXT,
    created_at       INTEGER NOT NULL
  );
`);

const selectConfig = db.prepare('SELECT data FROM guild_config WHERE guild_id = ?');
const upsertConfig = db.prepare(
  'INSERT INTO guild_config (guild_id, data) VALUES (?, ?) ON CONFLICT(guild_id) DO UPDATE SET data = excluded.data',
);

// Réglages d'un serveur, complétés par les valeurs par défaut (un module ajouté plus tard reçoit ses défauts).
export function getConfig(guildId) {
  const row = selectConfig.get(guildId);
  const stored = row ? JSON.parse(row.data) : {};
  const config = structuredClone(DEFAULT_CONFIG);
  for (const [key, value] of Object.entries(stored)) {
    config[key] = config[key] && typeof value === 'object' && !Array.isArray(value) ? { ...config[key], ...value } : value;
  }
  return config;
}

// Modifie les réglages : updateConfig(id, (cfg) => { cfg.welcome.channelId = '…'; }). Renvoie la config enregistrée.
export function updateConfig(guildId, mutate) {
  const config = getConfig(guildId);
  mutate(config);
  upsertConfig.run(guildId, JSON.stringify(config));
  return config;
}

export function closeDb() {
  try {
    db.close();
  } catch {}
}
