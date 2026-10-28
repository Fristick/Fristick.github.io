// Boosts : un message de remerciement par boost (y compris les boosts répétés) et un journal des fins de boost.
import { Events, MessageType } from 'discord.js';
import { BRAND } from '../config.mjs';
import { getConfig } from '../db.mjs';
import { baseEmbed, brandFooter, parseColor } from '../utils/embeds.mjs';
import { listen, log } from '../utils/log.mjs';
import { fill } from '../utils/placeholders.mjs';
import { sendTo } from '../utils/permissions.mjs';

const BOOST_TYPES = new Set([
  MessageType.GuildBoost,
  MessageType.GuildBoostTier1,
  MessageType.GuildBoostTier2,
  MessageType.GuildBoostTier3,
]);
const BOOST_PINK = 0xf47fff;
const UNBOOST_COLOR = 0xb48ead;
const DEDUPE_MS = 2 * 60_000;

// Un premier boost arrive deux fois (message système + premiumSince du membre) : on ne remercie qu'une fois.
// Deux messages système distincts restent deux boosts distincts (boost répété).
const recent = new Map(); // `${guildId}:${userId}` → { at, source: 'message' | 'update' | 'paired' }

function isDuplicate(key, source, now = Date.now()) {
  for (const [k, v] of recent) if (now - v.at > DEDUPE_MS) recent.delete(k);
  const prev = recent.get(key);
  if (prev && (source === 'update' || prev.source === 'update')) {
    recent.set(key, { at: prev.at, source: 'paired' });
    return true;
  }
  recent.set(key, { at: now, source });
  return false;
}

// Message de remerciement dans le salon configuré. Renvoie le message ou null (désactivé, salon absent…).
export async function sendBoostMessage(member, { test = false } = {}) {
  try {
    const { guild } = member;
    const cfg = getConfig(guild.id).boosts;
    if (!cfg.channelId || (!cfg.enabled && !test)) return null;
    const vars = { member, boostCount: guild.premiumSubscriptionCount ?? 0, boostTier: guild.premiumTier ?? 0 };
    const embed = baseEmbed(parseColor(cfg.color, BOOST_PINK))
      .setTitle(fill(cfg.title, vars).slice(0, 256) || 'Thank you for boosting!')
      .setDescription(fill(cfg.message, vars).slice(0, 4096) || `<@${member.id}> just boosted the server!`)
      .setThumbnail(member.displayAvatarURL({ size: 256 }));
    brandFooter(embed, test ? `${BRAND} (test)` : BRAND, guild.iconURL() ?? undefined);
    return await sendTo(guild, cfg.channelId, {
      content: `<@${member.id}>`,
      embeds: [embed],
      allowedMentions: { users: [member.id] },
    });
  } catch (error) {
    log.error('Boost message', error);
    return null;
  }
}

async function onBoost(member, source) {
  if (isDuplicate(`${member.guild.id}:${member.id}`, source)) return;
  const cfg = getConfig(member.guild.id).boosts;
  if (!cfg.enabled || !cfg.channelId) return;
  await member.guild.fetch().catch(() => null); // nombre de boosts et niveau à jour
  await sendBoostMessage(member);
}

async function onUnboost(member) {
  const channelId = getConfig(member.guild.id).logs.memberChannelId;
  if (!channelId) return;
  const embed = baseEmbed(UNBOOST_COLOR)
    .setAuthor({ name: member.user.tag ?? member.user.username, iconURL: member.displayAvatarURL() })
    .setDescription(`💔 <@${member.id}> stopped boosting`)
    .setTimestamp();
  await sendTo(member.guild, channelId, { embeds: [embed] });
}

export function register(client) {
  listen(client, Events.MessageCreate, async (message) => {
    if (!message.inGuild() || !BOOST_TYPES.has(message.type)) return;
    const member = message.member ?? (await message.guild.members.fetch(message.author.id).catch(() => null));
    if (member) await onBoost(member, 'message');
  });

  listen(client, Events.GuildMemberUpdate, async (oldMember, newMember) => {
    if (oldMember.partial) return; // ancien premiumSince inconnu
    const was = oldMember.premiumSinceTimestamp;
    const now = newMember.premiumSinceTimestamp;
    if (!was && now) await onBoost(newMember, 'update');
    else if (was && !now) await onUnboost(newMember);
  });
}
