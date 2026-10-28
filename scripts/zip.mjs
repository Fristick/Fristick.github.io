// `npm run zip` : prépare bot-renard-pterodactyl.zip, prêt à envoyer sur un serveur Pterodactyl
// (egg Node.js : il lance `npm install` puis `index.js`).
//  - inclus : le code, la police, l'egg, le README, et un .env à compléter (DISCORD_TOKEN) ;
//  - exclus : node_modules (réinstallé sur le serveur, pour Linux), data/ (base SQLite), .env local, archives.
// Option --avec-jeton : le .env inclus reprend le jeton de votre .env local (ne partagez pas ce zip).
// Option --avec-donnees : inclut data/ (réglages /setup, XP, tickets) — utile pour déménager le bot.
// Option --mise-a-jour (ou --update) : le code seulement (ni data/ ni .env), pour mettre à jour un bot déjà en ligne
// sans écraser sa base ni son jeton → bot-renard-maj.zip.
import { readFile, readdir, writeFile, stat } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { deflateRawSync, crc32 } from 'node:zlib';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = fileURLToPath(new URL('..', import.meta.url));
const args = process.argv.slice(2);
const update = args.includes('--mise-a-jour') || args.includes('--update');
const withToken = !update && args.includes('--avec-jeton');
const withData = !update && args.includes('--avec-donnees');
const OUT = path.join(ROOT, update ? 'bot-renard-maj.zip' : 'bot-renard-pterodactyl.zip');

const EXCLUDED = [
  /^node_modules\//, /^\.git\//, /^\.claude\//, /^\.env$/, /^\.env\.(?!example$)/, /\.zip$/, /\.log$/,
  /(^|\/)(Thumbs\.db|\.DS_Store)$/, /^data\/.*-(wal|shm)$/,
  ...(withData ? [] : [/^data\//]),
];

async function* walk(dir, rel = '') {
  for (const d of await readdir(dir, { withFileTypes: true })) {
    const r = rel ? `${rel}/${d.name}` : d.name;
    if (d.isDirectory()) {
      if (!EXCLUDED.some((re) => re.test(`${r}/`))) yield* walk(path.join(dir, d.name), r);
    } else if (!EXCLUDED.some((re) => re.test(r))) {
      yield r;
    }
  }
}

// .env de production : celui de .env.example, avec le jeton local si --avec-jeton.
async function productionEnv() {
  let env = await readFile(path.join(ROOT, '.env.example'), 'utf8');
  if (withToken) {
    const local = existsSync(path.join(ROOT, '.env')) ? await readFile(path.join(ROOT, '.env'), 'utf8') : '';
    const token = local.match(/^DISCORD_TOKEN=(.*)$/m)?.[1]?.trim();
    if (!token) throw new Error('--avec-jeton : aucun DISCORD_TOKEN dans le .env local.');
    env = env.replace(/^DISCORD_TOKEN=.*$/m, `DISCORD_TOKEN=${token}`);
  }
  return Buffer.from(env.replace(/\r\n/g, '\n'));
}

// ---------- Écriture d'un fichier ZIP (format standard, sans dépendance) ----------
function dosDateTime(date) {
  const time = (date.getHours() << 11) | (date.getMinutes() << 5) | Math.floor(date.getSeconds() / 2);
  const day = ((date.getFullYear() - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate();
  return { time, day };
}

const entries = [];
const chunks = [];
let offset = 0;

function addFile(name, data, mtime) {
  const nameBuf = Buffer.from(name, 'utf8');
  const compressed = deflateRawSync(data, { level: 9 });
  const useDeflate = compressed.length < data.length;
  const body = useDeflate ? compressed : data;
  const crc = crc32(data);
  const { time, day } = dosDateTime(mtime);
  const local = Buffer.alloc(30);
  local.writeUInt32LE(0x04034b50, 0);
  local.writeUInt16LE(20, 4);
  local.writeUInt16LE(0x0800, 6); // noms en UTF-8
  local.writeUInt16LE(useDeflate ? 8 : 0, 8);
  local.writeUInt16LE(time, 10);
  local.writeUInt16LE(day, 12);
  local.writeUInt32LE(crc, 14);
  local.writeUInt32LE(body.length, 18);
  local.writeUInt32LE(data.length, 22);
  local.writeUInt16LE(nameBuf.length, 26);
  local.writeUInt16LE(0, 28);
  chunks.push(local, nameBuf, body);
  entries.push({ nameBuf, crc, size: data.length, csize: body.length, method: useDeflate ? 8 : 0, time, day, offset });
  offset += local.length + nameBuf.length + body.length;
}

function finish() {
  const start = offset;
  for (const e of entries) {
    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE((3 << 8) | 20, 4); // créé sous Unix (droits ci-dessous)
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(0x0800, 8);
    central.writeUInt16LE(e.method, 10);
    central.writeUInt16LE(e.time, 12);
    central.writeUInt16LE(e.day, 14);
    central.writeUInt32LE(e.crc, 16);
    central.writeUInt32LE(e.csize, 20);
    central.writeUInt32LE(e.size, 24);
    central.writeUInt16LE(e.nameBuf.length, 28);
    central.writeUInt32LE((0o100644 << 16) >>> 0, 38); // fichier ordinaire, rw-r--r--
    central.writeUInt32LE(e.offset, 42);
    chunks.push(central, e.nameBuf);
    offset += central.length + e.nameBuf.length;
  }
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(offset - start, 12);
  end.writeUInt32LE(start, 16);
  chunks.push(end);
  return Buffer.concat(chunks);
}

// ---------- Construction ----------
let count = 0;
for await (const rel of walk(ROOT)) {
  const abs = path.join(ROOT, rel);
  addFile(rel, await readFile(abs), (await stat(abs)).mtime);
  count++;
}
if (!update) {
  addFile('.env', await productionEnv(), new Date());
  count++;
}

const zip = finish();
await writeFile(OUT, zip);
console.log(`✔ ${path.basename(OUT)} : ${count} fichiers, ${(zip.length / 1024 / 1024).toFixed(1)} Mo`);
if (update) {
  console.log('  Mise à jour : le code seulement. Le dossier data/ et le .env du serveur ne sont pas touchés.');
} else {
  console.log(withToken
    ? '  ⚠ Le .env inclus contient votre jeton : ne partagez pas ce zip.'
    : '  Le .env inclus est vide : collez votre jeton (DISCORD_TOKEN) dans le gestionnaire de fichiers Pterodactyl.');
  console.log(withData
    ? '  Inclut data/ (réglages, XP, tickets).'
    : '  Sans data/ : le bot démarrera avec une base vide (lancez /setup sur le serveur).');
}
