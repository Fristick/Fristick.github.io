// Salons vocaux temporaires : rejoindre le salon « créateur » ouvre un salon personnel, supprimé dès qu'il se vide.
// Le propriétaire le gère avec les boutons du panneau posté dans le chat du salon vocal (customId « vc:… »).
import { setTimeout as sleep } from 'node:timers/promises';
import {
  ActionRowBuilder,
  ButtonBuilder,
  ButtonStyle,
  ChannelType,
  Events,
  LabelBuilder,
  MessageFlags,
  ModalBuilder,
  OverwriteType,
  PermissionFlagsBits,
  PermissionsBitField,
  TextInputBuilder,
  TextInputStyle,
  UserSelectMenuBuilder,
  escapeMarkdown,
} from 'discord.js';
import { COLORS } from '../config.mjs';
import { db, getConfig } from '../db.mjs';
import { onComponent, onModal } from '../router.mjs';
import { baseEmbed, brandFooter, replyEphemeral, replyError, warnEmbed } from '../utils/embeds.mjs';
import { listen, log } from '../utils/log.mjs';
import { fill } from '../utils/placeholders.mjs';
import { missingPerms } from '../utils/permissions.mjs';

const P = PermissionFlagsBits;
const CREATE_COOLDOWN_MS = 10_000;
const RENAME_WINDOW_MS = 10 * 60_000;
const RENAMES_PER_WINDOW = 2; // limite Discord : 2 renommages par 10 min et par salon
const EDIT_TIMEOUT_MS = 8_000;
const KEEP_INSIDE_MAX = 25;
const OWNER_PERMS = ['ViewChannel', 'Connect', 'Speak', 'Stream', 'MoveMembers'];
const BOT_PERMS = [
  'ViewChannel',
  'Connect',
  'ManageChannels',
  'MoveMembers',
  'SendMessages',
  'EmbedLinks',
  'ReadMessageHistory',
];
const TIMEOUT = Symbol('timeout');

const q = {
  insert: db.prepare(
    'INSERT OR REPLACE INTO temp_voices (channel_id, guild_id, owner_id, panel_message_id, created_at) ' +
      'VALUES (?, ?, ?, NULL, ?)',
  ),
  get: db.prepare('SELECT * FROM temp_voices WHERE channel_id = ?'),
  all: db.prepare('SELECT * FROM temp_voices'),
  remove: db.prepare('DELETE FROM temp_voices WHERE channel_id = ?'),
  // Comparer-échanger : deux Claim simultanés ne peuvent pas réussir tous les deux.
  setOwner: db.prepare('UPDATE temp_voices SET owner_id = ? WHERE channel_id = ? AND owner_id = ?'),
  setPanel: db.prepare('UPDATE temp_voices SET panel_message_id = ? WHERE channel_id = ?'),
};

const cooldowns = new Map(); // `${guildId}:${userId}` → dernière création
const renames = new Map(); // channelId → horodatages des renommages récents

// ─── Outils ──────────────────────────────────────────────────────────────────

// Coupe sans casser un emoji (paire de substitution).
function cut(text, max) {
  let out = '';
  for (const ch of text) {
    if (out.length + ch.length > max) break;
    out += ch;
  }
  return out;
}

const permList = (names) => names.map((n) => `**${n.replace('Manage Roles', 'Manage Permissions')}**`).join(', ');
const inChannel = (channel, userId) => channel.guild.voiceStates.cache.get(userId)?.channelId === channel.id;
// Membres humains présents (un membre absent du cache compte comme humain, par prudence).
const humans = (channel) =>
  channel.guild.voiceStates.cache.filter((s) => s.channelId === channel.id && !s.member?.user.bot).size;

// Permissions que le bot peut accorder dans une dérogation (Manage Roles en salon : administrateurs seulement).
function grantable(guild) {
  const me = guild.members.me;
  return me.permissions.has(P.Administrator) ? PermissionsBitField.All : me.permissions.bitfield & ~P.ManageRoles;
}

const bits = (names, mask) => names.reduce((acc, n) => acc | P[n], 0n) & mask;
const options = (names, mask, value) => Object.fromEntries(names.filter((n) => P[n] & mask).map((n) => [n, value]));

// Attend une modification de salon sans rester bloqué derrière une limite de débit Discord.
// Renvoie null (succès), l'erreur, ou TIMEOUT (la requête partira plus tard, quand Discord le permettra).
async function settle(promise) {
  const abort = new AbortController();
  const timer = sleep(EDIT_TIMEOUT_MS, TIMEOUT, { signal: abort.signal }).catch(() => null);
  const result = await Promise.race([promise.then(() => null, (error) => error), timer]);
  abort.abort();
  return result;
}

// Renvoie l'horodatage auquel un nouveau renommage sera possible, ou 0 s'il l'est déjà.
export function nextRenameAt(channelId, now = Date.now()) {
  const recent = (renames.get(channelId) ?? []).filter((t) => now - t < RENAME_WINDOW_MS);
  if (recent.length) renames.set(channelId, recent);
  else renames.delete(channelId);
  return recent.length >= RENAMES_PER_WINDOW ? recent[recent.length - RENAMES_PER_WINDOW] + RENAME_WINDOW_MS : 0;
}

export function recordRename(channelId, now = Date.now()) {
  renames.set(channelId, [...(renames.get(channelId) ?? []), now]);
}

function forgetRename(channelId) {
  const list = renames.get(channelId);
  list?.pop();
  if (list && !list.length) renames.delete(channelId);
}

async function deleteTemp(channel, reason = 'Temporary voice channel is empty') {
  renames.delete(channel.id);
  try {
    await channel.delete(reason);
    q.remove.run(channel.id);
  } catch (error) {
    if (error.code === 10003) q.remove.run(channel.id); // déjà supprimé
    else log.warn(`Could not delete temporary voice channel #${channel.name}: ${error.message}`);
  }
}

// ─── Panneau ─────────────────────────────────────────────────────────────────

function panelPayload(ownerId) {
  const embed = baseEmbed(COLORS.blue)
    .setTitle('🎧 Voice channel controls')
    .setDescription(
      [
        `Owner: <@${ownerId}>`,
        '',
        'Only the owner can use these buttons. If the owner leaves, anyone still inside can press **🙋 Claim**.',
        '',
        '✏️ **Rename** the channel · 👥 set a user **Limit** (0 = unlimited)',
        '🔒 **Lock**: nobody new can join · 👁️ **Hide**: the channel disappears for others',
        '👢 **Kick** someone out · 👑 **Transfer** ownership',
      ].join('\n'),
    );
  brandFooter(embed);
  const button = (action, emoji, label, style = ButtonStyle.Secondary) =>
    new ButtonBuilder().setCustomId(`vc:${action}`).setEmoji(emoji).setLabel(label).setStyle(style);
  return {
    embeds: [embed],
    components: [
      new ActionRowBuilder().addComponents(
        button('rename', '✏️', 'Rename'),
        button('limit', '👥', 'Limit'),
        button('lock', '🔒', 'Lock / Unlock'),
        button('hide', '👁️', 'Hide / Show'),
      ),
      new ActionRowBuilder().addComponents(
        button('kick', '👢', 'Kick'),
        button('transfer', '👑', 'Transfer'),
        button('claim', '🙋', 'Claim', ButtonStyle.Primary),
      ),
    ],
    allowedMentions: { parse: [] },
  };
}

async function postPanel(channel, ownerId) {
  const message = await channel.send(panelPayload(ownerId)).catch((error) => {
    log.warn(`Could not post the voice panel in #${channel.name}: ${error.message}`);
    return null;
  });
  if (message) q.setPanel.run(message.id, channel.id);
}

async function refreshPanel(channel, panelMessageId, ownerId) {
  if (!panelMessageId) return;
  const message = await channel.messages.fetch(panelMessageId).catch(() => null);
  await message?.edit(panelPayload(ownerId)).catch(() => null);
}

// ─── Création / suppression ──────────────────────────────────────────────────

// Message dans le chat du salon créateur (seul moyen de prévenir le membre, il n'y a pas d'interaction).
async function notify(creator, member, text) {
  log.warn(`Temp voice (${creator.guild.name}): ${text}`);
  await creator.send({ content: `<@${member.id}> ${text}`, allowedMentions: { users: [member.id] } }).catch(() => {});
}

function buildOverwrites(guild, parent, ownerId) {
  const mask = grantable(guild);
  const map = new Map();
  for (const o of parent?.permissionOverwrites.cache.values() ?? []) {
    map.set(o.id, { id: o.id, type: o.type, allow: o.allow.bitfield & mask, deny: o.deny.bitfield & mask });
  }
  const grant = (id, names) => {
    const allow = bits(names, mask);
    const prev = map.get(id) ?? { id, type: OverwriteType.Member, allow: 0n, deny: 0n };
    map.set(id, { ...prev, allow: prev.allow | allow, deny: prev.deny & ~allow });
  };
  grant(ownerId, OWNER_PERMS);
  grant(guild.members.me.id, BOT_PERMS); // le bot doit voir le salon pour y déplacer le membre et poster le panneau
  return [...map.values()];
}

async function createFor(member, creator, cfg) {
  const { guild } = member;
  if (member.user.bot || !guild.members.me) return;
  const key = `${guild.id}:${member.id}`;
  const now = Date.now();
  for (const [k, t] of cooldowns) if (now - t >= CREATE_COOLDOWN_MS) cooldowns.delete(k);
  if (cooldowns.has(key)) return;
  cooldowns.set(key, now);

  const asCategory = (id) => {
    const channel = id ? guild.channels.cache.get(id) : null;
    return channel?.type === ChannelType.GuildCategory ? channel : null;
  };
  const parent = asCategory(cfg.categoryId) ?? asCategory(creator.parentId);
  const needed = [P.ViewChannel, P.ManageChannels, P.ManageRoles, P.MoveMembers, P.Connect];
  const missing = missingPerms(parent ?? guild, needed);
  if (missing.length) return notify(creator, member, `I can't create your channel: I'm missing ${permList(missing)}.`);

  const name = cut(fill(cfg.nameTemplate, { member }).trim() || `${member.displayName}'s channel`, 100);
  const userLimit = Math.min(99, Math.max(0, Math.trunc(Number(cfg.userLimit) || 0)));
  let channel;
  try {
    channel = await guild.channels.create({
      name,
      type: ChannelType.GuildVoice,
      parent: parent?.id,
      userLimit,
      permissionOverwrites: buildOverwrites(guild, parent, member.id),
      reason: `Temporary voice channel of ${member.user.tag}`,
    });
  } catch (error) {
    return notify(creator, member, `I couldn't create your channel: ${error.message}`);
  }

  // Ligne enregistrée avant le déplacement : un départ immédiat doit pouvoir supprimer le salon.
  q.insert.run(channel.id, guild.id, member.id, Date.now());
  const moved =
    member.voice.channelId === creator.id &&
    (await member.voice.setChannel(channel, 'Temporary voice channel').then(
      () => true,
      () => false,
    ));
  if (!moved) return deleteTemp(channel, 'Owner left before being moved');
  await postPanel(channel, member.id);
}

async function onVoiceUpdate(oldState, newState) {
  if (oldState.channelId === newState.channelId) return; // micro, caméra…
  const { guild } = newState;
  if (oldState.channelId && q.get.get(oldState.channelId)) {
    const channel = guild.channels.cache.get(oldState.channelId);
    if (!channel) q.remove.run(oldState.channelId);
    else if (!humans(channel)) await deleteTemp(channel);
  }
  if (!newState.channelId || !newState.member || !newState.channel) return;
  const cfg = getConfig(guild.id).tempVoice;
  if (cfg.enabled && cfg.creatorChannelId === newState.channelId) {
    await createFor(newState.member, newState.channel, cfg);
  }
}

// Au démarrage : on oublie les salons disparus et on supprime ceux restés vides pendant que le bot était éteint.
async function cleanupOnReady(client) {
  for (const row of q.all.all()) {
    const guild = client.guilds.cache.get(row.guild_id);
    if (!guild) {
      q.remove.run(row.channel_id);
      continue;
    }
    if (!guild.available) continue;
    const channel = guild.channels.cache.get(row.channel_id);
    if (!channel) q.remove.run(row.channel_id);
    else if (!humans(channel)) await deleteTemp(channel, 'Temporary voice channel left empty');
  }
}

// ─── Boutons, menus, fenêtres ────────────────────────────────────────────────

const confirm = (interaction, text) =>
  replyEphemeral(interaction, { content: '', embeds: [baseEmbed(COLORS.green).setDescription(text)], components: [] });

async function lacks(interaction, channel, flags) {
  const missing = missingPerms(channel, flags);
  if (!missing.length) return false;
  await replyError(interaction, `I'm missing ${permList(missing)} in this channel.`);
  return true;
}

// Le panneau vit dans le chat du salon vocal : interaction.channelId est le salon temporaire.
async function resolveTemp(interaction) {
  if (!interaction.inCachedGuild()) return null;
  const row = q.get.get(interaction.channelId);
  const channel = row ? interaction.guild.channels.cache.get(interaction.channelId) : null;
  if (!channel) {
    await replyError(interaction, "This isn't an active temporary voice channel anymore.");
    return null;
  }
  return { row, channel };
}

function showRename(interaction, { channel }) {
  const input = new TextInputBuilder()
    .setCustomId('name')
    .setStyle(TextInputStyle.Short)
    .setMinLength(1)
    .setMaxLength(100)
    .setRequired(true)
    .setValue(cut(channel.name, 100));
  const modal = new ModalBuilder()
    .setCustomId('vc:renamemodal')
    .setTitle('Rename your channel')
    .addLabelComponents(new LabelBuilder().setLabel('New name').setTextInputComponent(input));
  return interaction.showModal(modal);
}

function showLimit(interaction, { channel }) {
  const input = new TextInputBuilder()
    .setCustomId('limit')
    .setStyle(TextInputStyle.Short)
    .setMinLength(1)
    .setMaxLength(2)
    .setRequired(true)
    .setPlaceholder('0 = unlimited')
    .setValue(String(channel.userLimit ?? 0));
  const modal = new ModalBuilder()
    .setCustomId('vc:limitmodal')
    .setTitle('User limit')
    .addLabelComponents(
      new LabelBuilder()
        .setLabel('Max members')
        .setDescription('A number from 0 to 99 (0 = unlimited).')
        .setTextInputComponent(input),
    );
  return interaction.showModal(modal);
}

async function rename(interaction, { channel }) {
  const name = cut(interaction.fields.getTextInputValue('name').trim(), 100);
  if (!name) return replyError(interaction, "The name can't be empty.");
  const wait = nextRenameAt(channel.id);
  if (wait) return replyError(interaction, `You can rename again <t:${Math.ceil(wait / 1000)}:R>.`);
  if (await lacks(interaction, channel, [P.ManageChannels])) return;
  recordRename(channel.id);
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const result = await settle(channel.setName(name, `Renamed by ${interaction.user.tag}`));
  if (result === TIMEOUT) {
    const text = 'Discord is slowing down renames for this channel: the new name will apply in a few minutes.';
    return replyEphemeral(interaction, { embeds: [warnEmbed(text)] });
  }
  if (result) {
    forgetRename(channel.id);
    return replyError(interaction, `I couldn't rename the channel: ${result.message}`);
  }
  return confirm(interaction, `✏️ Channel renamed to **${escapeMarkdown(name)}**.`);
}

async function setLimit(interaction, { channel }) {
  const raw = interaction.fields.getTextInputValue('limit').trim();
  if (!/^\d{1,2}$/.test(raw)) return replyError(interaction, 'Enter a number between 0 and 99 (0 = unlimited).');
  const limit = Number(raw);
  if (await lacks(interaction, channel, [P.ManageChannels])) return;
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const result = await settle(channel.setUserLimit(limit, `Limit set by ${interaction.user.tag}`));
  if (result === TIMEOUT) {
    return replyEphemeral(interaction, {
      embeds: [warnEmbed('Discord is rate-limiting this channel: the new limit will apply in a few minutes.')],
    });
  }
  if (result) return replyError(interaction, `I couldn't change the limit: ${result.message}`);
  return confirm(interaction, limit ? `👥 User limit set to **${limit}**.` : '👥 User limit removed.');
}

// Lock (Connect) / Hide (ViewChannel) : bascule le refus pour @everyone ; propriétaire et présents gardent l'accès.
async function toggle(interaction, { channel, row }, perm) {
  if (await lacks(interaction, channel, [P.ManageRoles])) return;
  const { guild } = interaction;
  const everyone = guild.roles.everyone;
  const enable = !channel.permissionOverwrites.cache.get(everyone.id)?.deny.has(P[perm]);
  const reason = `Temporary voice channel (${interaction.user.tag})`;
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  try {
    if (enable) {
      const inside = guild.voiceStates.cache
        .filter((s) => s.channelId === channel.id && s.id !== row.owner_id && s.id !== guild.members.me.id)
        .first(KEEP_INSIDE_MAX)
        .map((s) => s.id);
      for (const id of [row.owner_id, ...inside]) {
        await channel.permissionOverwrites.edit(id, { [perm]: true }, { type: OverwriteType.Member, reason });
      }
    }
    await channel.permissionOverwrites.edit(everyone, { [perm]: enable ? false : null }, { reason });
  } catch (error) {
    return replyError(interaction, `I couldn't change the channel permissions: ${error.message}`);
  }
  const texts = {
    Connect: ['🔒 Channel locked: nobody new can join.', '🔓 Channel unlocked: everyone can join again.'],
    ViewChannel: [
      '🙈 Channel hidden: only you and the people already inside can see it.',
      '👁️ Channel visible to everyone again.',
    ],
  };
  return confirm(interaction, texts[perm][enable ? 0 : 1]);
}

async function askUser(interaction, { channel }, action, placeholder) {
  const others = channel.guild.voiceStates.cache.filter(
    (s) => s.channelId === channel.id && s.id !== interaction.user.id,
  );
  if (!others.size) return replyError(interaction, 'There is nobody else in your channel.');
  const menu = new UserSelectMenuBuilder()
    .setCustomId(`vc:${action}`)
    .setPlaceholder(placeholder)
    .setMinValues(1)
    .setMaxValues(1);
  return replyEphemeral(interaction, { components: [new ActionRowBuilder().addComponents(menu)] });
}

async function kick(interaction, { channel }) {
  const targetId = interaction.values[0];
  if (targetId === interaction.user.id) return replyError(interaction, "You can't kick yourself.");
  if (!inChannel(channel, targetId)) return replyError(interaction, `<@${targetId}> isn't in your channel.`);
  if (await lacks(interaction, channel, [P.MoveMembers])) return;
  await interaction.deferUpdate();
  try {
    const target = await interaction.guild.members.fetch(targetId);
    await target.voice.disconnect(`Kicked from the temporary channel by ${interaction.user.tag}`);
  } catch (error) {
    return replyError(interaction, `I couldn't disconnect <@${targetId}>: ${error.message}`);
  }
  // Sans sa dérogation (Lock/Hide), il ne peut plus revenir dans un salon verrouillé.
  if (targetId !== interaction.guild.members.me.id && channel.permissionOverwrites.cache.has(targetId)) {
    await channel.permissionOverwrites.delete(targetId).catch(() => null);
  }
  return confirm(interaction, `👢 <@${targetId}> was disconnected.`);
}

// Change de propriétaire : base de données, dérogations, panneau. Renvoie un avertissement éventuel,
// ou null si le propriétaire a changé entre-temps (autre Claim/Transfer plus rapide).
async function transferOwnership(channel, row, newOwnerId, by) {
  if (q.setOwner.run(newOwnerId, channel.id, row.owner_id).changes === 0) return null;
  let warning = '';
  if (missingPerms(channel, [P.ManageRoles]).length) {
    warning = '\n⚠️ I lack **Manage Permissions** here, so the channel permissions were not updated.';
  } else {
    const mask = grantable(channel.guild);
    const reason = `Temporary voice channel ownership (${by})`;
    try {
      await channel.permissionOverwrites.edit(newOwnerId, options(OWNER_PERMS, mask, true), {
        type: OverwriteType.Member,
        reason,
      });
      if (channel.permissionOverwrites.cache.has(row.owner_id) && P.MoveMembers & mask) {
        await channel.permissionOverwrites.edit(
          row.owner_id,
          { MoveMembers: null },
          { type: OverwriteType.Member, reason },
        );
      }
    } catch (error) {
      warning = `\n⚠️ I couldn't update the channel permissions: ${error.message}`;
    }
  }
  await refreshPanel(channel, row.panel_message_id, newOwnerId);
  await channel
    .send({ content: `👑 <@${newOwnerId}> is now the owner of this channel.`, allowedMentions: { users: [newOwnerId] } })
    .catch(() => null);
  return warning;
}

async function transfer(interaction, { channel, row }) {
  const targetId = interaction.values[0];
  if (targetId === interaction.user.id) return replyError(interaction, 'You already own this channel.');
  if (!inChannel(channel, targetId)) return replyError(interaction, `<@${targetId}> must be in your channel.`);
  const target = await interaction.guild.members.fetch(targetId).catch(() => null);
  if (!target) return replyError(interaction, `<@${targetId}> isn't a member of this server.`);
  if (target.user.bot) return replyError(interaction, "A bot can't own a channel.");
  await interaction.deferUpdate();
  const warning = await transferOwnership(channel, row, targetId, interaction.user.tag);
  if (warning === null) return replyError(interaction, 'You no longer own this channel.');
  return confirm(interaction, `👑 <@${targetId}> is now the owner of this channel.${warning}`);
}

async function claim(interaction, { channel, row }) {
  if (row.owner_id === interaction.user.id) return replyError(interaction, 'You already own this channel.');
  if (!inChannel(channel, interaction.user.id)) {
    return replyError(interaction, 'Join this voice channel first to claim it.');
  }
  if (inChannel(channel, row.owner_id)) {
    return replyError(interaction, `<@${row.owner_id}> is still here: only they can transfer the channel.`);
  }
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const warning = await transferOwnership(channel, row, interaction.user.id, interaction.user.tag);
  if (warning === null) return replyError(interaction, 'Someone else claimed this channel first.');
  return confirm(interaction, `👑 You now own this channel.${warning}`);
}

const OWNER_ACTIONS = {
  rename: async (i, ctx) => {
    const wait = nextRenameAt(ctx.channel.id);
    if (wait) return replyError(i, `You can rename again <t:${Math.ceil(wait / 1000)}:R>.`);
    if (!(await lacks(i, ctx.channel, [P.ManageChannels]))) return showRename(i, ctx);
  },
  limit: async (i, ctx) => {
    if (!(await lacks(i, ctx.channel, [P.ManageChannels]))) return showLimit(i, ctx);
  },
  lock: (i, ctx) => toggle(i, ctx, 'Connect'),
  hide: (i, ctx) => toggle(i, ctx, 'ViewChannel'),
  kick: (i, ctx) => askUser(i, ctx, 'kicksel', 'Pick the member to disconnect'),
  transfer: (i, ctx) => askUser(i, ctx, 'transfersel', 'Pick the new owner'),
  kicksel: kick,
  transfersel: transfer,
  renamemodal: rename,
  limitmodal: setLimit,
};

async function handle(interaction, [action]) {
  if (action !== 'claim' && !OWNER_ACTIONS[action]) return;
  const ctx = await resolveTemp(interaction);
  if (!ctx) return;
  if (action === 'claim') return claim(interaction, ctx);
  if (ctx.row.owner_id !== interaction.user.id) {
    return replyError(interaction, `Only the channel owner (<@${ctx.row.owner_id}>) can do that.`);
  }
  return OWNER_ACTIONS[action](interaction, ctx);
}

export function register(client) {
  onComponent('vc', handle);
  onModal('vc', handle);
  listen(client, Events.VoiceStateUpdate, onVoiceUpdate);
  listen(client, Events.ClientReady, () => cleanupOnReady(client));
  listen(client, Events.ChannelDelete, (channel) => {
    q.remove.run(channel.id);
    renames.delete(channel.id);
  });
}
