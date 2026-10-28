// Journaux du serveur : arrivées et départs (memberChannelId),
// rôles des membres et rôles créés/supprimés (roleChannelId).
import { setTimeout as sleep } from 'node:timers/promises';
import { AuditLogEvent, Events, PermissionFlagsBits, escapeMarkdown } from 'discord.js';
import { COLORS } from '../config.mjs';
import { getConfig } from '../db.mjs';
import { baseEmbed } from '../utils/embeds.mjs';
import { listen } from '../utils/log.mjs';
import { sendTo } from '../utils/permissions.mjs';

const DAY = 86_400_000;
const NEW_ACCOUNT_MS = 7 * DAY;
const AUDIT_WINDOW_MS = 10_000;
const AUDIT_DELAY_MS = 1_000; // l'entrée du journal d'audit arrive parfois après l'événement

const unix = (ms) => Math.floor(ms / 1000);
const tagOf = (user) => escapeMarkdown(user?.tag ?? user?.username ?? 'Unknown user');

const UNITS = [
  ['year', 365 * DAY],
  ['month', 30 * DAY],
  ['day', DAY],
  ['hour', 3_600_000],
  ['minute', 60_000],
  ['second', 1_000],
];

// « 2 days, 3 hours » : les deux plus grandes unités non nulles.
export function formatDuration(ms) {
  let rest = Math.max(0, ms);
  const parts = [];
  for (const [name, size] of UNITS) {
    const n = Math.floor(rest / size);
    if (n > 0 || parts.length) {
      if (n > 0) parts.push(`${n} ${name}${n > 1 ? 's' : ''}`);
      rest -= n * size;
      if (parts.length === 2 || (parts.length && n === 0)) break;
    }
  }
  return parts.join(', ') || 'less than a second';
}

// Joint les éléments sans dépasser max caractères ; ce qui ne rentre pas devient « … +N more ».
export function joinWithin(items, max = 1024, sep = ' ') {
  const full = items.join(sep);
  if (full.length <= max) return full;
  let out = '';
  for (let i = 0; i < items.length; i++) {
    const next = out ? `${out}${sep}${items[i]}` : items[i];
    if (next.length > max - 20) return `${out}${sep}… +${items.length - i} more`;
    out = next;
  }
  return out;
}

// Auteur d'une action d'après le journal d'audit : id, null si introuvable, undefined si le bot n'y a pas accès.
async function findExecutor(guild, type, targetId, prefer = () => true) {
  if (!guild.members.me?.permissions.has(PermissionFlagsBits.ViewAuditLog)) return undefined;
  await sleep(AUDIT_DELAY_MS);
  const logs = await guild.fetchAuditLogs({ type, limit: 10 }).catch(() => null);
  if (!logs) return undefined;
  const recent = logs.entries.filter(
    (e) => e.targetId === targetId && Date.now() - e.createdTimestamp < AUDIT_WINDOW_MS + AUDIT_DELAY_MS,
  );
  const entry = recent.find(prefer) ?? recent.first();
  return entry?.executorId ?? null;
}

function memberEmbed(color, title, user) {
  const avatar = user.displayAvatarURL({ size: 256 });
  return baseEmbed(color)
    .setTitle(title)
    .setAuthor({ name: user.tag ?? user.username ?? user.id, iconURL: avatar })
    .setThumbnail(avatar)
    .setFooter({ text: `User ID: ${user.id}` })
    .setTimestamp();
}

async function onJoin(member) {
  const channelId = getConfig(member.guild.id).logs.memberChannelId;
  if (!channelId) return;
  const { user, guild } = member;
  const created = unix(user.createdTimestamp);
  const notes = [];
  if (Date.now() - user.createdTimestamp < NEW_ACCOUNT_MS) {
    notes.push('⚠️ **New account**: created less than 7 days ago.');
  }
  if (user.bot) notes.push('🤖 This is a bot.');
  const embed = memberEmbed(COLORS.green, '📥 Member joined', user).addFields(
    { name: 'User', value: `${user} (${tagOf(user)})`, inline: true },
    { name: 'ID', value: `\`${user.id}\``, inline: true },
    { name: 'Member count', value: String(guild.memberCount), inline: true },
    { name: 'Account created', value: `<t:${created}:F> (<t:${created}:R>)` },
  );
  if (notes.length) embed.setDescription(notes.join('\n'));
  await sendTo(guild, channelId, { embeds: [embed] });
}

async function onLeave(member) {
  const channelId = getConfig(member.guild.id).logs.memberChannelId;
  if (!channelId) return;
  const { user, guild } = member;
  const embed = memberEmbed(COLORS.red, '📤 Member left', user).addFields(
    { name: 'User', value: `${user} (${tagOf(user)})`, inline: true },
    { name: 'ID', value: `\`${user.id}\``, inline: true },
    { name: 'Member count', value: String(guild.memberCount), inline: true },
  );
  if (member.joinedTimestamp) {
    const joined = unix(member.joinedTimestamp);
    embed.addFields({
      name: 'Joined',
      value: `<t:${joined}:R> · stayed **${formatDuration(Date.now() - member.joinedTimestamp)}**`,
    });
  }
  if (!member.partial) {
    const roles = member.roles.cache
      .filter((r) => r.id !== guild.id)
      .sort((a, b) => b.position - a.position)
      .map((r) => `${r}`);
    embed.addFields({ name: `Roles (${roles.length})`, value: roles.length ? joinWithin(roles) : 'None' });
  }
  await sendTo(guild, channelId, { embeds: [embed] });
}

async function onMemberUpdate(oldMember, newMember) {
  if (oldMember.partial) return; // rôles d'avant inconnus
  const added = newMember.roles.cache.filter((r) => !oldMember.roles.cache.has(r.id));
  const removed = oldMember.roles.cache.filter((r) => !newMember.roles.cache.has(r.id));
  if (!added.size && !removed.size) return;
  const { guild, user } = newMember;
  const channelId = getConfig(guild.id).logs.roleChannelId;
  if (!channelId) return;

  const ids = new Set([...added.keys(), ...removed.keys()]);
  const byRoles = (e) =>
    e.changes.some((c) => ['$add', '$remove'].includes(c.key) && c.new?.some((r) => ids.has(r.id)));
  const executorId = await findExecutor(guild, AuditLogEvent.MemberRoleUpdate, newMember.id, byRoles);

  const embed = memberEmbed(COLORS.blue, '🎭 Roles updated', user).addFields({
    name: 'User',
    value: `${user} (${tagOf(user)})`,
    inline: true,
  });
  if (executorId) embed.addFields({ name: 'By', value: `<@${executorId}>`, inline: true });
  if (added.size) embed.addFields({ name: 'Added', value: joinWithin(added.map((r) => `+ ${r}`), 1024, '\n') });
  if (removed.size) embed.addFields({ name: 'Removed', value: joinWithin(removed.map((r) => `− ${r}`), 1024, '\n') });
  await sendTo(guild, channelId, { embeds: [embed] });
}

async function onRole(role, created) {
  const channelId = getConfig(role.guild.id).logs.roleChannelId;
  if (!channelId) return;
  const type = created ? AuditLogEvent.RoleCreate : AuditLogEvent.RoleDelete;
  const executorId = await findExecutor(role.guild, type, role.id);
  const name = escapeMarkdown(role.name || 'unnamed');
  const color = role.colors?.primaryColor ?? role.color ?? 0;
  const embed = baseEmbed(created ? COLORS.green : COLORS.red)
    .setTitle(created ? '➕ Role created' : '➖ Role deleted')
    .addFields(
      { name: 'Role', value: created ? `${role} (${name})` : name, inline: true },
      { name: 'Colour', value: color ? `\`${role.hexColor}\`` : 'Default', inline: true },
      { name: 'ID', value: `\`${role.id}\``, inline: true },
    )
    .setFooter({ text: `Role ID: ${role.id}` })
    .setTimestamp();
  if (executorId) embed.addFields({ name: 'By', value: `<@${executorId}>`, inline: true });
  if (role.managed) embed.setDescription('This role is managed by an integration (bot, boosts or subscriptions).');
  await sendTo(role.guild, channelId, { embeds: [embed] });
}

export function register(client) {
  listen(client, Events.GuildMemberAdd, onJoin);
  listen(client, Events.GuildMemberRemove, onLeave);
  listen(client, Events.GuildMemberUpdate, onMemberUpdate);
  listen(client, Events.GuildRoleCreate, (role) => onRole(role, true));
  listen(client, Events.GuildRoleDelete, (role) => onRole(role, false));
}
