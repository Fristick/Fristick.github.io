// Système de niveaux : XP par message (avec délai) et par minute en vocal, annonces de passage de niveau,
// rôles récompenses. L'XP total est stocké, le niveau en est déduit (niveau L → L+1 : 5·L² + 50·L + 100 XP).
import { EmbedBuilder, Events, PermissionFlagsBits } from 'discord.js';
import { BRAND, COLORS, DEFAULT_RANKS, XP } from '../config.mjs';
import { db, getConfig, updateConfig } from '../db.mjs';
import { listen, log } from '../utils/log.mjs';
import { canManageRole, missingPerms, sendTo } from '../utils/permissions.mjs';
import { fill } from '../utils/placeholders.mjs';

const VOICE_TICK_MS = 60_000;

// --- Formules ---

export const xpForLevel = (level) => 5 * level ** 2 + 50 * level + 100;

// XP total nécessaire pour atteindre un niveau (somme fermée de xpForLevel(0 … level-1)).
export function totalXpForLevel(level) {
  const l = Math.max(0, Math.floor(Number(level) || 0));
  if (l === 0) return 0;
  return (5 * (l - 1) * l * (2 * l - 1)) / 6 + 25 * l * (l - 1) + 100 * l;
}

export function levelFromXp(totalXp) {
  let rest = Math.max(0, Math.floor(Number(totalXp) || 0));
  let level = 0;
  while (rest >= xpForLevel(level)) {
    rest -= xpForLevel(level);
    level += 1;
  }
  return { level, xpIntoLevel: rest, xpForNext: xpForLevel(level) };
}

// --- Base de données ---

const selectRow = db.prepare(
  'SELECT xp, messages, voice_seconds, last_message_at FROM levels WHERE guild_id = ? AND user_id = ?',
);
const countAbove = db.prepare('SELECT COUNT(*) AS n FROM levels WHERE guild_id = ? AND xp > ?');
const upsertXp = db.prepare(
  `INSERT INTO levels (guild_id, user_id, xp) VALUES (?, ?, ?)
   ON CONFLICT(guild_id, user_id) DO UPDATE SET xp = excluded.xp`,
);
const bumpMessages = db.prepare(
  `INSERT INTO levels (guild_id, user_id, messages) VALUES (?, ?, 1)
   ON CONFLICT(guild_id, user_id) DO UPDATE SET messages = messages + 1`,
);
const markRewarded = db.prepare('UPDATE levels SET last_message_at = ? WHERE guild_id = ? AND user_id = ?');
const addVoiceTime = db.prepare(
  `INSERT INTO levels (guild_id, user_id, voice_seconds) VALUES (?, ?, ?)
   ON CONFLICT(guild_id, user_id) DO UPDATE SET voice_seconds = voice_seconds + excluded.voice_seconds`,
);
const deleteUser = db.prepare('DELETE FROM levels WHERE guild_id = ? AND user_id = ?');
const deleteGuild = db.prepare('DELETE FROM levels WHERE guild_id = ?');
const selectRanked = db.prepare('SELECT user_id, xp FROM levels WHERE guild_id = ? AND xp > 0');

export function getStats(guildId, userId) {
  const row = selectRow.get(guildId, userId);
  const xp = Number(row?.xp ?? 0);
  return {
    xp,
    ...levelFromXp(xp),
    messages: Number(row?.messages ?? 0),
    voiceSeconds: Number(row?.voice_seconds ?? 0),
    rank: 1 + Number(countAbove.get(guildId, xp).n),
  };
}

const multiplierOf = (cfg) => {
  const m = Number(cfg.levels.multiplier);
  return Number.isFinite(m) && m >= 0 ? m : 1;
};

// --- Annonces et récompenses ---

// Récompenses valides triées par niveau croissant.
export function rewardsOf(cfg) {
  return (Array.isArray(cfg.levels.rewards) ? cfg.levels.rewards : [])
    .filter((r) => r?.roleId && Number.isFinite(Number(r.level)))
    .map((r) => ({ level: Number(r.level), roleId: String(r.roleId) }))
    .sort((a, b) => a.level - b.level);
}

// Grade d'un niveau : rôle récompense du plus haut palier atteint qui existe encore, sinon null.
export function rankRoleFor(guild, level, cfg = getConfig(guild.id)) {
  const reached = rewardsOf(cfg).filter((r) => r.level <= level);
  for (let i = reached.length - 1; i >= 0; i--) {
    const role = guild.roles.cache.get(reached[i].roleId);
    if (role) return role;
  }
  return null;
}

// Envoi dans un salon reçu en paramètre (texte, fil ou chat d'un vocal) si le bot en a le droit ; sinon null.
async function sendIn(channel, payload) {
  if (!channel?.isTextBased?.() || !channel.guild) return null;
  const send = channel.isThread() ? PermissionFlagsBits.SendMessagesInThreads : PermissionFlagsBits.SendMessages;
  const needed = [PermissionFlagsBits.ViewChannel, send, PermissionFlagsBits.EmbedLinks];
  if (missingPerms(channel, needed).length) return null;
  return channel.send(payload).catch(() => null);
}

async function announceLevelUp(member, oldLevel, level, channel, cfg) {
  // Palier franchi : on annonce la promotion (le plus haut grade passé pendant cette montée).
  const crossed = rewardsOf(cfg).filter((r) => r.level > oldLevel && r.level <= level);
  const promotion = crossed.length ? member.guild.roles.cache.get(crossed.at(-1).roleId) : null;
  const text = fill(cfg.levels.message, { member, level });
  const embed = new EmbedBuilder()
    .setColor(promotion?.colors?.primaryColor || COLORS.gold)
    .setDescription(promotion ? `${text}\n\n🎖️ **Promotion!** You are now <@&${promotion.id}>.` : text)
    .setThumbnail(member.displayAvatarURL({ extension: 'png', size: 128 }));
  const payload = { embeds: [embed], allowedMentions: { parse: [] } };
  // Salon des niveaux configuré ; s'il a disparu, on se rabat sur le salon de l'activité.
  const sent = cfg.levels.channelId ? await sendTo(member.guild, cfg.levels.channelId, payload) : null;
  if (!sent && channel) await sendIn(channel, payload);
}

/**
 * Donne les rôles récompenses du niveau atteint. stackRewards : tous les paliers atteints ; sinon seul le plus haut
 * (les autres rôles récompenses sont retirés). prune : retire aussi les rôles de paliers au-dessus du niveau
 * (utile après une baisse d'XP par un admin). Ne lève jamais d'erreur.
 */
export async function syncRewardRoles(member, level, { prune = false } = {}) {
  try {
    const { guild } = member;
    const cfg = getConfig(guild.id);
    const rewards = rewardsOf(cfg);
    if (!rewards.length) return;

    const reached = rewards.filter((r) => r.level <= level);
    const keep = new Set(
      cfg.levels.stackRewards ? reached.map((r) => r.roleId) : reached.slice(-1).map((r) => r.roleId),
    );
    const removable = new Set(
      rewards.filter((r) => !cfg.levels.stackRewards || prune || r.level <= level).map((r) => r.roleId),
    );

    const has = (id) => member.roles.cache.has(id);
    const toAdd = [...keep].filter((id) => !has(id));
    const toRemove = [...removable].filter((id) => !keep.has(id) && has(id));
    for (const [ids, action] of [
      [toAdd, 'add'],
      [toRemove, 'remove'],
    ]) {
      for (const id of ids) {
        const role = guild.roles.cache.get(id);
        if (!canManageRole(guild, role)) continue;
        await member.roles[action](role, `Level ${level} reward`).catch((error) => {
          log.warn(`Could not ${action} reward role ${role.name} for ${member.user.tag}: ${error.message}`);
        });
      }
    }
  } catch (error) {
    log.warn(`Reward roles sync failed for ${member?.id}: ${error.message}`);
  }
}

// --- Modifications d'XP ---

/**
 * Ajoute (ou retire si négatif) de l'XP. Passage de niveau : annonce (salon des niveaux, sinon `channel`)
 * puis synchronisation des rôles récompenses. announce: false = pas d'annonce.
 */
export async function addXp(member, amount, { channel, announce = true } = {}) {
  const { guild } = member;
  const oldXp = Number(selectRow.get(guild.id, member.id)?.xp ?? 0);
  const xp = Math.max(0, Math.round(oldXp + (Number(amount) || 0)));
  upsertXp.run(guild.id, member.id, xp);

  const oldLevel = levelFromXp(oldXp).level;
  const level = levelFromXp(xp).level;
  // Rôles d'abord : quand l'annonce de promotion paraît, le membre a déjà son nouveau grade.
  if (level !== oldLevel) await syncRewardRoles(member, level);
  if (level > oldLevel && announce) {
    await announceLevelUp(member, oldLevel, level, channel, getConfig(guild.id)).catch((error) => {
      log.warn(`Level-up announcement failed in ${guild.name}: ${error.message}`);
    });
  }
  return { xp, oldLevel, level, leveledUp: level > oldLevel };
}

// Fixe l'XP total (commandes admin) : pas d'annonce, rôles récompenses resynchronisés (y compris retraits).
export async function setXp(member, totalXp) {
  const { guild } = member;
  const oldLevel = levelFromXp(selectRow.get(guild.id, member.id)?.xp ?? 0).level;
  const xp = Math.max(0, Math.round(Number(totalXp) || 0));
  upsertXp.run(guild.id, member.id, xp);
  const level = levelFromXp(xp).level;
  await syncRewardRoles(member, level, { prune: true });
  return { xp, oldLevel, level };
}

// Supprime les données d'un membre, ou de tout le serveur si userId est null. Renvoie le nombre de lignes effacées.
export function resetXp(guildId, userId = null) {
  const result = userId ? deleteUser.run(guildId, userId) : deleteGuild.run(guildId);
  return Number(result.changes);
}

// Resynchronise les grades de tous les membres qui ont de l'XP (après la création des grades, par exemple).
export async function syncAllRewardRoles(guild) {
  let synced = 0;
  for (const row of selectRanked.all(guild.id)) {
    const member =
      guild.members.cache.get(row.user_id) ?? (await guild.members.fetch(row.user_id).catch(() => null));
    if (!member) continue;
    await syncRewardRoles(member, levelFromXp(row.xp).level, { prune: true });
    synced += 1;
  }
  return synced;
}

// --- Grades ---

const normalize = (name) => name.toLowerCase().replace(/[^\p{L}\p{N}]+/gu, '');

/**
 * Crée les grades de DEFAULT_RANKS (rôle existant du même nom réutilisé), les enregistre comme récompenses de
 * niveau en mode promotion (un seul grade gardé), place les nouveaux rôles juste au-dessus du rôle vérifié pour
 * que leur couleur s'affiche, puis donne leur grade aux membres qui ont déjà de l'XP. Ne lève jamais d'erreur.
 */
export async function createRankRoles(guild) {
  const report = { created: [], reused: [], errors: [], synced: 0, placed: false };
  try {
    const me = guild.members.me;
    if (!me?.permissions.has(PermissionFlagsBits.ManageRoles)) {
      report.errors.push('I need the **Manage Roles** permission to create the ranks.');
      return report;
    }
    const ranks = [];
    const fresh = [];
    // Du plus haut au plus bas : chaque rôle créé arrive tout en bas, le plus haut finit donc au-dessus.
    for (const rank of [...DEFAULT_RANKS].reverse()) {
      let role = guild.roles.cache.find((r) => !r.managed && normalize(r.name) === normalize(rank.name));
      if (role) report.reused.push(`<@&${role.id}>`);
      else {
        role = await guild.roles
          .create({ name: rank.name, colors: { primaryColor: rank.color }, reason: `${BRAND} level ranks` })
          .catch((error) => {
            report.errors.push(`Couldn't create **${rank.name}**: ${error.message}`);
            return null;
          });
        if (role) {
          report.created.push(`<@&${role.id}>`);
          fresh.push(role);
        }
      }
      if (role) ranks.push({ level: rank.level, roleId: role.id });
    }

    // Au-dessus du rôle vérifié / rôle automatique, sinon la couleur verte de « Verified » l'emporterait.
    // Chaque rôle placé juste au-dessus de l'ancre, du plus haut au plus bas : l'ordre final est respecté.
    const cfg = getConfig(guild.id);
    const anchor = [cfg.verification.roleId, cfg.welcome.autoRoleId]
      .map((id) => id && guild.roles.cache.get(id))
      .filter((role) => role && role.position < me.roles.highest.position)
      .sort((a, b) => b.position - a.position)[0];
    if (anchor && fresh.length) {
      try {
        for (const role of fresh) await role.setPosition(guild.roles.cache.get(anchor.id).position);
        report.placed = true;
      } catch (error) {
        report.errors.push(`Couldn't move the ranks above <@&${anchor.id}>: ${error.message}`);
      }
    }

    const levels = new Set(ranks.map((r) => r.level));
    updateConfig(guild.id, (c) => {
      const others = rewardsOf(c).filter((r) => !levels.has(r.level));
      c.levels.rewards = [...others, ...ranks].sort((a, b) => a.level - b.level);
      c.levels.stackRewards = false;
    });
    report.synced = await syncAllRewardRoles(guild);
  } catch (error) {
    log.error('createRankRoles', error);
    report.errors.push(`Unexpected error: ${error.message}`);
  }
  return report;
}

// --- Embed de présentation (panneau /setup) ---

// guild fourni : la liste des grades (rôles récompenses) est ajoutée sous la présentation.
export function buildRankSystemEmbed(guild = null) {
  const embed = new EmbedBuilder()
    .setColor(COLORS.gold)
    .setTitle('🏆 Rank System')
    .setDescription(
      [
        '**Level up through active participation!**',
        '',
        '📈 Earn XP through messages and voice time',
        '🎖️ Rise through the ranks',
        '🥇 Unlock exclusive rewards',
        '📊 Check out the leaderboard',
        '',
        '*The more active you are, the faster you level up!*',
      ].join('\n'),
    )
    .setFooter({ text: 'Level System' })
    .setTimestamp();
  const ranks = guild ? rewardsOf(getConfig(guild.id)).filter((r) => guild.roles.cache.has(r.roleId)) : [];
  if (ranks.length) {
    const lines = ranks.reverse().map((r) => `<@&${r.roleId}> — Level **${r.level}**`);
    embed.addFields({ name: '🎖️ Ranks', value: lines.join('\n').slice(0, 1024) });
  }
  return embed;
}

// --- Événements ---

const isIgnored = (cfg, channel) => {
  const ignored = Array.isArray(cfg.levels.ignoredChannelIds) ? cfg.levels.ignoredChannelIds : [];
  return Boolean(channel) && (ignored.includes(channel.id) || (channel.parentId && ignored.includes(channel.parentId)));
};

async function onMessage(message) {
  if (!message.inGuild() || message.author.bot || message.webhookId || message.system) return;
  const cfg = getConfig(message.guildId);
  if (!cfg.levels.enabled || isIgnored(cfg, message.channel)) return;

  const { guildId } = message;
  const userId = message.author.id;
  const now = Date.now();
  const lastRewarded = Number(selectRow.get(guildId, userId)?.last_message_at ?? 0);
  bumpMessages.run(guildId, userId);
  if (now - lastRewarded < XP.messageCooldownMs) return;
  markRewarded.run(now, guildId, userId);

  const member = message.member ?? (await message.guild.members.fetch(userId).catch(() => null));
  if (!member) return;
  const base = XP.messageMin + Math.floor(Math.random() * (XP.messageMax - XP.messageMin + 1));
  const amount = Math.round(base * multiplierOf(cfg));
  if (amount > 0) await addXp(member, amount, { channel: message.channel });
}

// Toutes les minutes : XP aux membres actifs en vocal (au moins 2 humains, ni muets ni sourds, hors salon AFK).
async function voiceTick(client) {
  for (const guild of client.guilds.cache.values()) {
    try {
      const cfg = getConfig(guild.id);
      if (!cfg.levels.enabled) continue;
      const amount = Math.round(XP.voicePerMinute * multiplierOf(cfg));
      for (const channel of guild.channels.cache.values()) {
        if (!channel.isVoiceBased() || channel.id === guild.afkChannelId || isIgnored(cfg, channel)) continue;
        const humans = channel.members.filter((m) => !m.user.bot);
        if (humans.size < 2) continue;
        for (const member of humans.values()) {
          if (member.voice.mute || member.voice.deaf) continue;
          addVoiceTime.run(guild.id, member.id, VOICE_TICK_MS / 1000);
          if (amount > 0) await addXp(member, amount, { channel });
        }
      }
    } catch (error) {
      log.error(`Voice XP in ${guild.name}`, error);
    }
  }
}

let voiceTimer = null;

export function register(client) {
  listen(client, Events.MessageCreate, onMessage);
  listen(client, Events.ClientReady, () => {
    if (voiceTimer) return;
    let running = false;
    voiceTimer = setInterval(async () => {
      if (running) return; // un tour lent (beaucoup de rôles à donner) ne se chevauche pas avec le suivant
      running = true;
      try {
        await voiceTick(client);
      } catch (error) {
        log.error('Voice XP', error);
      } finally {
        running = false;
      }
    }, VOICE_TICK_MS);
    voiceTimer.unref();
  });
}
