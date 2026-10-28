// Panneau /setup : un seul message éphémère pour configurer tous les modules (accueil, auto-setup, vue par module).
// Les vues sont des fonctions pures (guild, cfg, notice) → { embeds, components } ; les handlers modifient la config
// puis réaffichent la vue dans le même message (interaction.update).
import {
  ActionRowBuilder,
  ButtonBuilder,
  ButtonStyle,
  ChannelSelectMenuBuilder,
  ChannelType,
  MessageFlags,
  ModalBuilder,
  OverwriteType,
  PermissionFlagsBits as P,
  RoleSelectMenuBuilder,
  StringSelectMenuBuilder,
  TextInputBuilder,
  TextInputStyle,
} from 'discord.js';
import { BRAND, COLORS, DEFAULT_CONFIG, DEFAULT_RANKS, HEX, TICKET_CATEGORIES } from '../config.mjs';
import { getConfig, updateConfig } from '../db.mjs';
import { onComponent, onModal } from '../router.mjs';
import { baseEmbed, brandFooter, replyError } from '../utils/embeds.mjs';
import { log } from '../utils/log.mjs';
import { canManageRole, missingPerms, sendTo } from '../utils/permissions.mjs';
import { PLACEHOLDER_HELP } from '../utils/placeholders.mjs';
import { sendBoostMessage } from './boosts.mjs';
import { buildRankSystemEmbed, createRankRoles } from './levels.mjs';
import { postRulesPanel } from './rules.mjs';
import { postTicketPanel } from './tickets.mjs';
import { postVerificationPanel, restrictEveryone } from './verification.mjs';
import { sendWelcome } from './welcome.mjs';

// --- Constantes ---

const TEXT = [ChannelType.GuildText, ChannelType.GuildAnnouncement];
const VOICE = [ChannelType.GuildVoice];
const CATEGORY = [ChannelType.GuildCategory];
const IGNORABLE = [...TEXT, ChannelType.GuildVoice, ChannelType.GuildStageVoice, ChannelType.GuildForum, ...CATEGORY];
const SEND = [P.ViewChannel, P.SendMessages, P.EmbedLinks];
// Les panneaux (vérification, tickets) ne concernent aucun membre : seules les variables du serveur s'appliquent.
const PANEL_VARS = '{server} {memberCount}';
const BOT_PERMS = [
  P.ManageRoles,
  P.ManageChannels,
  P.MoveMembers,
  P.ViewAuditLog,
  P.SendMessages,
  P.EmbedLinks,
  P.AttachFiles,
  P.ReadMessageHistory,
  P.ManageMessages,
];

const MODULES = [
  { id: 'rules', label: 'Rules', emoji: '📜', description: 'Server rules: Discord ToS, respect, no hate, no spam…' },
  { id: 'welcome', label: 'Welcome', emoji: '👋', description: 'Welcome message, image, DM and auto-role' },
  { id: 'verify', label: 'Verification', emoji: '✅', description: 'Verify button that gives the verified role' },
  { id: 'levels', label: 'Levels', emoji: '📈', description: 'XP from messages and voice, level-ups, role rewards' },
  { id: 'tickets', label: 'Tickets', emoji: '🎫', description: 'Support ticket panel, category and staff roles' },
  { id: 'logs', label: 'Logs', emoji: '🗂️', description: 'Join/leave and role change logs' },
  { id: 'voice', label: 'Create your voice', emoji: '🔊', description: 'Temporary voice channels created on demand' },
  { id: 'boosts', label: 'Boosts', emoji: '💎', description: 'Thank-you message for server boosts' },
];

// --- Petits utilitaires d'affichage ---

const clip = (text, max) => {
  const s = String(text ?? '');
  return s.length > max ? `${s.slice(0, max - 1)}…` : s;
};
const join = (parts, max = 4096) => clip(parts.filter(Boolean).join('\n\n'), max);
const field = (name, value, inline = false) => ({ name, value: clip(value || '—', 1024), inline });
const bullets = (items) => items.map((item) => `• ${item}`).join('\n');
const plural = (n, word) => `${n} ${word}${n === 1 ? '' : 's'}`;
const codes = (list) =>
  list
    .split(' ')
    .map((p) => `\`${p}\``)
    .join(' ');

const channelOf = (guild, id) => (id && guild.channels.cache.get(id)) || null;
const roleOf = (guild, id) => (id && guild.roles.cache.get(id)) || null;
const channelText = (guild, id, empty = 'Not set') => {
  if (!id) return empty;
  return channelOf(guild, id) ? `<#${id}>` : '⚠️ deleted channel';
};
const roleText = (guild, id, empty = 'Not set') => {
  if (!id) return empty;
  return roleOf(guild, id) ? `<@&${id}>` : '⚠️ deleted role';
};
const onOff = (on) => (on ? '🟢 On' : '⚫ Off');
const statusText = (on) => (on ? '🟢 Enabled' : '🔴 Disabled');
const botRoleName = (guild) => {
  const me = guild.members.me;
  return me?.roles?.botRole?.name ?? me?.roles?.highest?.name ?? 'my role';
};

// Récompenses triées par niveau, une seule par niveau.
function sortedRewards(levels) {
  const byLevel = new Map();
  for (const r of Array.isArray(levels.rewards) ? levels.rewards : []) {
    if (Number.isInteger(r?.level) && r.roleId) byLevel.set(r.level, r);
  }
  return [...byLevel.values()].sort((a, b) => a.level - b.level);
}

const RANKS_TEXT = DEFAULT_RANKS.map((r) => `${r.name} (${r.level})`).join(' → ');

// Résumé de createRankRoles pour une ligne d'état.
function ranksNotice(report) {
  const parts = [];
  if (report.created.length) parts.push(`🆕 Created ${report.created.join(' ')}`);
  if (report.reused.length) parts.push(`♻️ Reused ${report.reused.join(' ')}`);
  if (report.created.length || report.reused.length) {
    parts.push(
      '🎖️ Ranks saved as level rewards (promotion mode: members keep only their highest rank)' +
        (report.synced ? ` — ${plural(report.synced, 'member')} updated.` : '.'),
    );
  }
  if (report.created.length && !report.placed) {
    parts.push('⚠️ Move the rank roles above the verified role in Server Settings → Roles so their colours show.');
  }
  return [...parts, ...report.errors.map((e) => `❌ ${e}`)].join('\n');
}

// --- Avertissements (salons supprimés, permissions, hiérarchie des rôles) ---

function channelWarning(guild, id, what, perms = SEND) {
  if (!id) return null;
  const channel = channelOf(guild, id);
  if (!channel) return `The ${what} was deleted — pick another one.`;
  const missing = missingPerms(channel, perms);
  return missing.length ? `I'm missing **${missing.join(', ')}** in <#${id}>.` : null;
}

function roleWarning(guild, id, what) {
  if (!id) return null;
  const role = roleOf(guild, id);
  if (!role) return `The ${what} was deleted — pick another one.`;
  if (canManageRole(guild, role)) return null;
  if (role.managed || role.id === guild.id) {
    return `<@&${id}> is managed by Discord or an integration — pick another role.`;
  }
  if (!guild.members.me?.permissions.has(P.ManageRoles)) return `I need **Manage Roles** to give <@&${id}>.`;
  return (
    `I can't manage the role <@&${id}> — move my role **${botRoleName(guild)}** above it ` +
    '(Server Settings → Roles).'
  );
}

function guildWarning(guild, flags, why) {
  const missing = missingPerms(guild, flags);
  return missing.length ? `I need **${missing.join(', ')}** ${why}.` : null;
}

// État de chaque module : configuré ? activé ? résumé d'une ligne + avertissements.
const CHECKS = {
  rules(guild, { rules: c }) {
    return {
      configured: Boolean(c.channelId),
      enabled: c.enabled,
      summary: `${channelText(guild, c.channelId)}${c.panelMessageId ? '' : ' · not posted yet'}`,
      warnings: [channelWarning(guild, c.channelId, 'rules channel')].filter(Boolean),
    };
  },
  welcome(guild, { welcome: c }) {
    return {
      configured: Boolean(c.channelId),
      enabled: c.enabled,
      summary: channelText(guild, c.channelId),
      warnings: [
        channelWarning(guild, c.channelId, 'welcome channel', c.image ? [...SEND, P.AttachFiles] : SEND),
        roleWarning(guild, c.autoRoleId, 'auto-role'),
      ].filter(Boolean),
    };
  },
  verify(guild, { verification: c }) {
    return {
      configured: Boolean(c.channelId),
      enabled: c.enabled,
      summary: `${channelText(guild, c.channelId)} · role ${roleText(guild, c.roleId)}`,
      warnings: [
        channelWarning(guild, c.channelId, 'verification channel'),
        c.roleId ? roleWarning(guild, c.roleId, 'verified role') : 'No verified role selected yet.',
      ].filter(Boolean),
    };
  },
  levels(guild, { levels: c }) {
    const rewards = sortedRewards(c);
    return {
      configured: true,
      enabled: c.enabled,
      summary: `level-ups in ${c.channelId ? channelText(guild, c.channelId) : 'the same channel'} · ${plural(
        rewards.length,
        'reward',
      )}`,
      warnings: [
        channelWarning(guild, c.channelId, 'level-up channel'),
        ...rewards.map((r) => roleWarning(guild, r.roleId, `reward role for level ${r.level}`)),
      ].filter(Boolean),
    };
  },
  tickets(guild, { tickets: c }) {
    const category = channelOf(guild, c.categoryId);
    const staff = Array.isArray(c.staffRoleIds) ? c.staffRoleIds : [];
    const goneStaff = staff.filter((id) => !roleOf(guild, id)).length;
    const categoryMissing = category ? missingPerms(category, [P.ViewChannel, P.ManageChannels]) : [];
    return {
      configured: Boolean(c.panelChannelId),
      enabled: c.enabled,
      summary: `panel ${channelText(guild, c.panelChannelId)} · ${plural(staff.length, 'staff role')}`,
      warnings: [
        channelWarning(guild, c.panelChannelId, 'ticket panel channel'),
        c.categoryId && !category ? 'The tickets category was deleted — pick another one.' : null,
        c.panelChannelId && !c.categoryId ? "No tickets category selected — tickets can't be opened yet." : null,
        categoryMissing.length
          ? `I'm missing **${categoryMissing.join(', ')}** in the **${category.name}** category.`
          : null,
        c.panelChannelId ? guildWarning(guild, [P.ManageChannels, P.ManageRoles], 'to open ticket channels') : null,
        channelWarning(guild, c.logChannelId, 'transcript channel', [...SEND, P.AttachFiles]),
        goneStaff ? `${plural(goneStaff, 'staff role')} were deleted — update the staff roles.` : null,
        c.panelChannelId && !staff.length ? 'No staff role selected — only admins will see the tickets.' : null,
      ].filter(Boolean),
    };
  },
  logs(guild, { logs: c }) {
    const on = Boolean(c.memberChannelId || c.roleChannelId);
    return {
      configured: on,
      enabled: on,
      summary: `joins/leaves ${channelText(guild, c.memberChannelId, 'off')} · roles ${channelText(
        guild,
        c.roleChannelId,
        'off',
      )}`,
      warnings: [
        channelWarning(guild, c.memberChannelId, 'join/leave log channel'),
        channelWarning(guild, c.roleChannelId, 'role log channel'),
        c.roleChannelId ? guildWarning(guild, [P.ViewAuditLog], 'to show who changed a role') : null,
      ].filter(Boolean),
    };
  },
  voice(guild, { tempVoice: c }) {
    return {
      configured: Boolean(c.creatorChannelId),
      enabled: c.enabled,
      summary: channelText(guild, c.creatorChannelId),
      warnings: [
        c.creatorChannelId && !channelOf(guild, c.creatorChannelId)
          ? 'The "Create your voice" channel was deleted — pick another one.'
          : null,
        c.categoryId && !channelOf(guild, c.categoryId) ? 'The category for new channels was deleted.' : null,
        c.creatorChannelId
          ? guildWarning(guild, [P.ManageChannels, P.MoveMembers], 'to create voice channels and move members')
          : null,
      ].filter(Boolean),
    };
  },
  boosts(guild, { boosts: c }) {
    return {
      configured: Boolean(c.channelId),
      enabled: c.enabled,
      summary: channelText(guild, c.channelId),
      warnings: [channelWarning(guild, c.channelId, 'boost channel')].filter(Boolean),
    };
  },
};

// --- Briques de composants ---

const row = (...components) => new ActionRowBuilder().addComponents(components);

function button(customId, label, { style = ButtonStyle.Secondary, emoji, disabled = false } = {}) {
  const b = new ButtonBuilder().setCustomId(customId).setLabel(label).setStyle(style).setDisabled(disabled);
  return emoji ? b.setEmoji(emoji) : b;
}
const backButton = (customId = 'setup:home', label = 'Back') => button(customId, label, { emoji: '⬅️' });
const toggleButton = (customId, enabled) =>
  button(customId, enabled ? 'Disable' : 'Enable', {
    style: enabled ? ButtonStyle.Danger : ButtonStyle.Success,
    emoji: enabled ? '⏸️' : '▶️',
  });
const switchButton = (customId, label, on) =>
  button(customId, `${label}: ${on ? 'On' : 'Off'}`, { style: on ? ButtonStyle.Primary : ButtonStyle.Secondary });

// Menu de salons avec la valeur actuelle présélectionnée (seulement si le salon existe encore et a le bon type).
function channelSelect(guild, customId, placeholder, { types = TEXT, ids = [], min = 1, max = 1 } = {}) {
  const menu = new ChannelSelectMenuBuilder()
    .setCustomId(customId)
    .setPlaceholder(placeholder)
    .setChannelTypes(types)
    .setMinValues(min)
    .setMaxValues(max);
  const defaults = ids.filter((id) => types.includes(channelOf(guild, id)?.type)).slice(0, max);
  if (defaults.length) menu.setDefaultChannels(defaults);
  return row(menu);
}

function roleSelect(guild, customId, placeholder, { ids = [], min = 1, max = 1 } = {}) {
  const menu = new RoleSelectMenuBuilder()
    .setCustomId(customId)
    .setPlaceholder(placeholder)
    .setMinValues(min)
    .setMaxValues(max);
  const defaults = ids.filter((id) => roleOf(guild, id) && id !== guild.id).slice(0, max);
  if (defaults.length) menu.setDefaultRoles(defaults);
  return row(menu);
}

const panelEmbed = (title, color = COLORS.blue) => brandFooter(baseEmbed(color).setTitle(title), `${BRAND} · Setup`);
const moduleEmbed = (title, intro, warnings, notice) =>
  panelEmbed(title).setDescription(join([intro, warnings.map((w) => `⚠️ ${w}`).join('\n'), notice]));

// --- Vues ---

export function homeView(guild, cfg, notice) {
  const lines = MODULES.map((m) => {
    const s = CHECKS[m.id](guild, cfg);
    const name = `**${m.label}**`;
    if (!s.configured) return `❌ ${name} — not configured`;
    if (!s.enabled) return `❌ ${name} — disabled · ${s.summary}`;
    if (s.warnings.length) return `⚠️ ${name} — ${s.summary} · ${plural(s.warnings.length, 'issue')} to fix`;
    return `✅ ${name} — ${s.summary}`;
  });
  const missing = missingPerms(guild, BOT_PERMS);
  const embed = panelEmbed(`⚙️ ${BRAND} — Setup`).setDescription(
    join([
      'Configure every module from this panel: pick a module in the menu, or click **⚡ Auto Setup** to create ' +
        'the recommended channels and roles in one go.',
      lines.join('\n'),
      missing.length
        ? `⚠️ **Bot permissions** — I'm missing **${missing.join(', ')}**. ` +
          'Give these permissions to my role in Server Settings → Roles.'
        : null,
      notice,
    ]),
  );
  const select = new StringSelectMenuBuilder()
    .setCustomId('setup:module')
    .setPlaceholder('Configure a module…')
    .addOptions(MODULES.map((m) => ({ label: m.label, value: m.id, description: m.description, emoji: m.emoji })));
  return {
    embeds: [embed],
    components: [
      row(
        button('setup:auto', 'Auto Setup', { style: ButtonStyle.Success, emoji: '⚡' }),
        button('setup:refresh', 'Refresh', { emoji: '🔄' }),
      ),
      row(select),
    ],
  };
}

export function rulesView(guild, cfg, notice) {
  const c = cfg.rules;
  const posted =
    c.panelMessageId && c.channelId && channelOf(guild, c.channelId)
      ? `[Open](https://discord.com/channels/${guild.id}/${c.channelId}/${c.panelMessageId})`
      : 'Not posted';
  const embed = moduleEmbed(
    '📜 Rules',
    'The server rules, posted by the bot with buttons to the Discord Terms of Service and Community Guidelines. ' +
      'The default text covers Discord’s rules, respect, no racism or discrimination, NSFW, spam, advertising, ' +
      'safety and voice channels. **Edit rules** to change it (an empty text restores the default), then ' +
      '**Post / update** — the posted message is edited in place. When enabled, the verification panel reminds ' +
      'members that verifying means accepting the rules.',
    CHECKS.rules(guild, cfg).warnings,
    notice,
  ).addFields(
    field('Status', statusText(c.enabled), true),
    field('Channel', channelText(guild, c.channelId), true),
    field('Message', posted, true),
    field('Title', c.title),
    field('Rules (preview)', c.message),
    field('Placeholders', codes('{server} {memberCount} {tickets}')),
  );
  return {
    embeds: [embed],
    components: [
      channelSelect(guild, 'setup:rules:channel', 'Rules channel…', { ids: [c.channelId] }),
      row(
        toggleButton('setup:rules:toggle', c.enabled),
        button('setup:rules:edit', 'Edit rules', { emoji: '✏️' }),
        button('setup:rules:post', 'Post / update', { style: ButtonStyle.Primary, emoji: '📤' }),
        backButton(),
      ),
    ],
  };
}

export function welcomeView(guild, cfg, notice) {
  const c = cfg.welcome;
  const embed = moduleEmbed(
    '👋 Welcome',
    'Greets every new member with an embed (and a welcome image) in the channel of your choice. ' +
      'It can also DM them and give them a role on join.',
    CHECKS.welcome(guild, cfg).warnings,
    notice,
  ).addFields(
    field('Status', statusText(c.enabled), true),
    field('Channel', channelText(guild, c.channelId), true),
    field('Auto-role', roleText(guild, c.autoRoleId, 'None'), true),
    field('Image', onOff(c.image), true),
    field('DM', onOff(c.dm), true),
    field('Colour', c.color, true),
    field('Title', c.title),
    field('Message', c.message),
    field('DM message', c.dmMessage),
    field('Placeholders', codes(PLACEHOLDER_HELP)),
  );
  return {
    embeds: [embed],
    components: [
      channelSelect(guild, 'setup:welcome:channel', 'Welcome channel…', { ids: [c.channelId] }),
      roleSelect(guild, 'setup:welcome:autorole', 'Auto-role on join (optional)…', { ids: [c.autoRoleId], min: 0 }),
      row(
        toggleButton('setup:welcome:toggle', c.enabled),
        switchButton('setup:welcome:image', 'Image', c.image),
        switchButton('setup:welcome:dm', 'DM', c.dm),
        button('setup:welcome:edit', 'Edit message', { emoji: '✏️' }),
        button('setup:welcome:test', 'Send test', { emoji: '🧪' }),
      ),
      row(backButton()),
    ],
  };
}

export function verifyView(guild, cfg, notice) {
  const c = cfg.verification;
  const panel =
    c.panelMessageId && c.channelId && channelOf(guild, c.channelId)
      ? `[Open](https://discord.com/channels/${guild.id}/${c.channelId}/${c.panelMessageId})`
      : 'Not posted';
  const embed = moduleEmbed(
    '✅ Verification',
    'New members click the button of the verification panel to get the verified role. ' +
      'Use **🔒 Restrict @everyone** to hide the rest of the server from unverified members.',
    CHECKS.verify(guild, cfg).warnings,
    notice,
  ).addFields(
    field('Status', statusText(c.enabled), true),
    field('Channel', channelText(guild, c.channelId), true),
    field('Verified role', roleText(guild, c.roleId), true),
    field('Button label', c.buttonLabel, true),
    field('Colour', c.color, true),
    field('Panel', panel, true),
    field('Title', c.title),
    field('Message', c.message),
    field('Placeholders', codes(PANEL_VARS)),
  );
  return {
    embeds: [embed],
    components: [
      channelSelect(guild, 'setup:verify:channel', 'Verification channel…', { ids: [c.channelId] }),
      roleSelect(guild, 'setup:verify:role', 'Verified role…', { ids: [c.roleId] }),
      row(
        toggleButton('setup:verify:toggle', c.enabled),
        button('setup:verify:edit', 'Edit message', { emoji: '✏️' }),
        button('setup:verify:post', 'Post panel', { style: ButtonStyle.Primary, emoji: '📤' }),
        button('setup:verify:restrict', 'Restrict @everyone', { style: ButtonStyle.Danger, emoji: '🔒' }),
        backButton(),
      ),
    ],
  };
}

export function restrictConfirmView(guild, cfg) {
  const c = cfg.verification;
  const embed = panelEmbed('🔒 Restrict @everyone', COLORS.gold).setDescription(
    join([
      'This hides the server from members who are not verified yet. Here is exactly what will change:',
      bullets([
        `${roleText(guild, c.roleId)} gets every permission @everyone has now (plus **View Channels**), ` +
          'so verified members keep the same access.',
        `${channelText(guild, c.channelId)}: @everyone can see it but not write there, and it is hidden from ` +
          'verified members.',
        '**@everyone** loses **View Channels** at server level: unverified members only see the verification ' +
          'channel (and the rules channel, if the Rules module is enabled).',
      ]),
      '⚠️ This applies to **everyone** without the verified role, staff included: anyone whose roles don\'t have ' +
        '**View Channels** themselves must click **Verify** (or get the role) to see the server again.',
      'Channels whose own permissions still let @everyone in stay visible — they are listed after the change. ' +
        'To undo: Server Settings → Roles → @everyone → turn **View Channels** back on.',
    ]),
  );
  return {
    embeds: [embed],
    components: [
      row(
        button('setup:verify:restrictrun', 'Confirm', { style: ButtonStyle.Danger, emoji: '🔒' }),
        button('setup:verify:view', 'Cancel'),
      ),
    ],
  };
}

export function restrictResultView(guild, { changes = [], errors = [] } = {}) {
  const embed = panelEmbed('🔒 Restrict @everyone', errors.length ? COLORS.gold : COLORS.green).addFields(
    field('Changes', changes.length ? bullets(changes) : errors.length ? 'No changes made.' : 'Already restricted.'),
  );
  if (errors.length) embed.addFields(field('Errors', bullets(errors)));
  return { embeds: [embed], components: [row(backButton('setup:verify:view', 'Back to Verification'))] };
}

export function levelsView(guild, cfg, notice) {
  const c = cfg.levels;
  const ignored = (Array.isArray(c.ignoredChannelIds) ? c.ignoredChannelIds : []).filter((id) => channelOf(guild, id));
  const rewards = sortedRewards(c);
  const embed = moduleEmbed(
    '📈 Levels',
    'Members earn XP by chatting and by spending time in voice channels. ' +
      'Each new level is announced and can give a reward role.',
    CHECKS.levels(guild, cfg).warnings,
    notice,
  ).addFields(
    field('Status', statusText(c.enabled), true),
    field('Level-up channel', c.channelId ? channelText(guild, c.channelId) : 'Same channel as the message', true),
    field('XP multiplier', `×${c.multiplier}`, true),
    field('Stack rewards', `${onOff(c.stackRewards)}\n${c.stackRewards ? 'Keep every reward' : 'Highest only'}`, true),
    field('Rewards', rewards.length ? plural(rewards.length, 'reward') : 'None', true),
    field('Ignored channels', ignored.length ? ignored.map((id) => `<#${id}>`).join(' ') : 'None', true),
    field('Level-up message', c.message),
    field('Placeholders', codes(`${PLACEHOLDER_HELP} {level}`)),
  );
  return {
    embeds: [embed],
    components: [
      channelSelect(guild, 'setup:levels:channel', 'Level-up channel (empty = same channel as the message)…', {
        ids: [c.channelId],
        min: 0,
      }),
      channelSelect(guild, 'setup:levels:ignored', 'Channels where no XP is earned…', {
        types: IGNORABLE,
        ids: ignored,
        min: 0,
        max: 25,
      }),
      row(
        toggleButton('setup:levels:toggle', c.enabled),
        switchButton('setup:levels:stack', 'Stack rewards', c.stackRewards),
        button('setup:levels:edit', 'Edit', { emoji: '✏️' }),
        button('setup:reward:view', 'Rewards', { emoji: '🏅' }),
        button('setup:levels:rankinfo', 'Post Rank System info', { style: ButtonStyle.Primary, emoji: '📤' }),
      ),
      row(backButton()),
    ],
  };
}

export function rewardsView(guild, cfg, notice) {
  const rewards = sortedRewards(cfg.levels);
  const list = rewards.map((r) => `Level **${r.level}** → ${roleText(guild, r.roleId)}`).join('\n');
  const warnings = rewards.map((r) => roleWarning(guild, r.roleId, `reward role for level ${r.level}`)).filter(Boolean);
  const embed = panelEmbed('🏅 Level rewards').setDescription(
    join([
      'Roles given automatically when members reach a level. ' +
        (cfg.levels.stackRewards
          ? '**Stack rewards** is On: members keep every reward role they reached.'
          : '**Stack rewards** is Off: members only keep the role of the highest level reached.'),
      list || 'No rewards yet — click **➕ Add reward**, or **🎖️ Create ranks** for the ready-made ranks.',
      `**🎖️ Create ranks** creates (or reuses) these roles with their colours and sets them as rewards: ` +
        `${RANKS_TEXT}. It turns **Stack rewards** off so members are promoted from one rank to the next.`,
      warnings.map((w) => `⚠️ ${w}`).join('\n'),
      notice,
    ]),
  );
  const components = [
    row(
      button('setup:reward:add', 'Add reward', { style: ButtonStyle.Success, emoji: '➕' }),
      button('setup:reward:ranks', 'Create ranks', { style: ButtonStyle.Primary, emoji: '🎖️' }),
      backButton('setup:levels:view', 'Back to Levels'),
    ),
  ];
  if (rewards.length) {
    const select = new StringSelectMenuBuilder()
      .setCustomId('setup:reward:remove')
      .setPlaceholder(rewards.length > 25 ? 'Remove a reward (first 25)…' : 'Remove a reward…')
      .addOptions(
        rewards.slice(0, 25).map((r) => ({
          label: clip(`Level ${r.level} → ${roleOf(guild, r.roleId)?.name ?? 'deleted role'}`, 100),
          value: String(r.level),
          emoji: '🗑️',
        })),
      );
    components.push(row(select));
  }
  return { embeds: [embed], components };
}

export function rewardRoleView(guild, cfg, level) {
  const current = sortedRewards(cfg.levels).find((r) => r.level === level);
  const embed = panelEmbed(`🏅 Reward for level ${level}`).setDescription(
    join([
      `Pick the role members get when they reach **level ${level}**.`,
      current ? `This replaces the current reward for this level (${roleText(guild, current.roleId)}).` : null,
    ]),
  );
  return {
    embeds: [embed],
    components: [
      roleSelect(guild, `setup:reward:set:${level}`, 'Role to give…'),
      row(backButton('setup:reward:view', 'Cancel')),
    ],
  };
}

export function ticketsView(guild, cfg, notice) {
  const c = cfg.tickets;
  const staff = (Array.isArray(c.staffRoleIds) ? c.staffRoleIds : []).filter((id) => roleOf(guild, id));
  const embed = moduleEmbed(
    '🎫 Tickets',
    'Members pick a category in the ticket panel and a private channel is opened with your staff. ' +
      'Transcripts are sent to the log channel when a ticket is closed.',
    CHECKS.tickets(guild, cfg).warnings,
    notice,
  ).addFields(
    field('Status', statusText(c.enabled), true),
    field('Panel channel', channelText(guild, c.panelChannelId), true),
    field('Tickets category', channelText(guild, c.categoryId), true),
    field('Staff roles', staff.length ? staff.map((id) => `<@&${id}>`).join(' ') : 'None', true),
    field('Transcript channel', channelText(guild, c.logChannelId, 'None'), true),
    field('Colour', c.color, true),
    field('Title', c.title),
    field('Message', c.message),
    field('Ticket types', TICKET_CATEGORIES.map((t) => `${t.emoji} ${t.label}`).join(' · ')),
    field('Placeholders', codes(PANEL_VARS)),
  );
  return {
    embeds: [embed],
    components: [
      channelSelect(guild, 'setup:tickets:panel', 'Channel of the ticket panel…', { ids: [c.panelChannelId] }),
      channelSelect(guild, 'setup:tickets:category', 'Category where tickets are opened…', {
        types: CATEGORY,
        ids: [c.categoryId],
      }),
      roleSelect(guild, 'setup:tickets:staff', 'Staff roles that can see tickets…', { ids: staff, min: 0, max: 10 }),
      channelSelect(guild, 'setup:tickets:log', 'Transcript / log channel (optional)…', {
        ids: [c.logChannelId],
        min: 0,
      }),
      row(
        toggleButton('setup:tickets:toggle', c.enabled),
        button('setup:tickets:edit', 'Edit message', { emoji: '✏️' }),
        button('setup:tickets:post', 'Post panel', { style: ButtonStyle.Primary, emoji: '📤' }),
        backButton(),
      ),
    ],
  };
}

export function logsView(guild, cfg, notice) {
  const c = cfg.logs;
  const embed = moduleEmbed(
    '📜 Logs',
    'Logs member arrivals and departures, and role changes, in the channels of your choice. ' +
      'Clear a menu to turn that log off.',
    CHECKS.logs(guild, cfg).warnings,
    notice,
  ).addFields(
    field('Joins / leaves', channelText(guild, c.memberChannelId, 'Off'), true),
    field('Role changes', channelText(guild, c.roleChannelId, 'Off'), true),
  );
  return {
    embeds: [embed],
    components: [
      channelSelect(guild, 'setup:logs:member', 'Join/leave log channel (optional)…', {
        ids: [c.memberChannelId],
        min: 0,
      }),
      channelSelect(guild, 'setup:logs:role', 'Role log channel (optional)…', { ids: [c.roleChannelId], min: 0 }),
      row(backButton()),
    ],
  };
}

export function voiceView(guild, cfg, notice) {
  const c = cfg.tempVoice;
  const embed = moduleEmbed(
    '🔊 Create your voice',
    'Members who join the creator channel get their own temporary voice channel, deleted when it is empty.',
    CHECKS.voice(guild, cfg).warnings,
    notice,
  ).addFields(
    field('Status', statusText(c.enabled), true),
    field('Creator channel', channelText(guild, c.creatorChannelId), true),
    field('Category', channelText(guild, c.categoryId, 'Same as the creator channel'), true),
    field('Name template', c.nameTemplate, true),
    field('Default user limit', Number(c.userLimit) > 0 ? String(c.userLimit) : 'Unlimited', true),
    field('Placeholders', codes('{username}')),
  );
  return {
    embeds: [embed],
    components: [
      channelSelect(guild, 'setup:voice:creator', '"Create your voice" channel…', {
        types: VOICE,
        ids: [c.creatorChannelId],
      }),
      channelSelect(guild, 'setup:voice:category', 'Category for the new channels (optional)…', {
        types: CATEGORY,
        ids: [c.categoryId],
        min: 0,
      }),
      row(
        toggleButton('setup:voice:toggle', c.enabled),
        button('setup:voice:edit', 'Edit', { emoji: '✏️' }),
        backButton(),
      ),
    ],
  };
}

export function boostsView(guild, cfg, notice) {
  const c = cfg.boosts;
  const embed = moduleEmbed(
    '💎 Boosts',
    'Thanks every member who boosts the server with a message in the channel of your choice.',
    CHECKS.boosts(guild, cfg).warnings,
    notice,
  ).addFields(
    field('Status', statusText(c.enabled), true),
    field('Channel', channelText(guild, c.channelId), true),
    field('Colour', c.color, true),
    field('Title', c.title),
    field('Message', c.message),
    field('Placeholders', codes(`${PLACEHOLDER_HELP} {boostCount} {boostTier}`)),
  );
  return {
    embeds: [embed],
    components: [
      channelSelect(guild, 'setup:boosts:channel', 'Boost channel…', { ids: [c.channelId] }),
      row(
        toggleButton('setup:boosts:toggle', c.enabled),
        button('setup:boosts:edit', 'Edit message', { emoji: '✏️' }),
        button('setup:boosts:test', 'Send test', { emoji: '🧪' }),
        backButton(),
      ),
    ],
  };
}

export const VIEWS = {
  home: homeView,
  rules: rulesView,
  welcome: welcomeView,
  verify: verifyView,
  levels: levelsView,
  rewards: rewardsView,
  tickets: ticketsView,
  logs: logsView,
  voice: voiceView,
  boosts: boostsView,
};

// --- Fenêtres d'édition (modals) ---

const COLOR_INPUT = { id: 'color', label: 'Colour (hex, e.g. #2a5bff)', kind: 'color', max: 7, required: false };
const TITLE_INPUT = { id: 'title', label: 'Title', max: 200, required: false, placeholder: 'Empty = default title' };
const messageInput = (label, vars, max = 2000) => ({
  id: 'message',
  label,
  max,
  paragraph: true,
  placeholder: clip(`Variables: ${vars}`, 100),
});

// key = clé de config du module ; id d'un champ = clé de config qu'il modifie.
const EDITORS = {
  rules: {
    key: 'rules',
    title: 'Server rules',
    inputs: [
      TITLE_INPUT,
      messageInput('Rules (empty = default rules)', '{server} {memberCount} {tickets}', 4000),
      COLOR_INPUT,
    ],
  },
  welcome: {
    key: 'welcome',
    title: 'Welcome message',
    inputs: [
      TITLE_INPUT,
      messageInput('Message', PLACEHOLDER_HELP),
      COLOR_INPUT,
      {
        id: 'dmMessage',
        label: 'DM message (sent when DM is on)',
        max: 2000,
        paragraph: true,
        required: false,
        placeholder: 'Empty = default DM message',
      },
    ],
  },
  verify: {
    key: 'verification',
    title: 'Verification panel',
    inputs: [
      TITLE_INPUT,
      messageInput('Message', PANEL_VARS),
      COLOR_INPUT,
      { id: 'buttonLabel', label: 'Button label', max: 80, required: false, placeholder: 'Empty = Verify' },
    ],
  },
  levels: {
    key: 'levels',
    title: 'Level settings',
    inputs: [
      messageInput('Level-up message', `${PLACEHOLDER_HELP} {level}`, 1000),
      { id: 'multiplier', label: 'XP multiplier (0.1 – 5)', kind: 'number', min: 0.1, max: 5, length: 5 },
    ],
  },
  tickets: {
    key: 'tickets',
    title: 'Ticket panel',
    inputs: [TITLE_INPUT, messageInput('Message', PANEL_VARS), COLOR_INPUT],
  },
  voice: {
    key: 'tempVoice',
    title: 'Create your voice',
    inputs: [
      {
        id: 'nameTemplate',
        label: 'Channel name ({username} = owner)',
        max: 100,
        placeholder: "🔊 {username}'s channel",
      },
      {
        id: 'userLimit',
        label: 'Default user limit (0 = unlimited, max 99)',
        kind: 'integer',
        min: 0,
        max: 99,
        length: 2,
      },
    ],
  },
  boosts: {
    key: 'boosts',
    title: 'Boost message',
    inputs: [TITLE_INPUT, messageInput('Message', `${PLACEHOLDER_HELP} {boostCount} {boostTier}`), COLOR_INPUT],
  },
};

function textInput({ id, label, max, paragraph = false, required = true, placeholder, length }, value) {
  const maxLength = length ?? max;
  const input = new TextInputBuilder()
    .setCustomId(id)
    .setLabel(label)
    .setStyle(paragraph ? TextInputStyle.Paragraph : TextInputStyle.Short)
    .setMaxLength(maxLength)
    .setRequired(required);
  if (placeholder) input.setPlaceholder(clip(placeholder, 100));
  const current = String(value ?? '');
  if (current) input.setValue(clip(current, maxLength));
  return input;
}

const modal = (customId, title, inputs) =>
  new ModalBuilder()
    .setCustomId(customId)
    .setTitle(clip(title, 45))
    .addComponents(inputs.map((input) => new ActionRowBuilder().addComponents(input)));

export function editorModal(section, cfg) {
  const editor = EDITORS[section];
  const values = cfg[editor.key];
  return modal(
    `setup:${section}:edit`,
    editor.title,
    editor.inputs.map((spec) => textInput(spec, values[spec.id])),
  );
}

export const rewardModal = () =>
  modal('setup:reward:add', 'Add a level reward', [
    textInput({ id: 'level', label: 'Level (1 – 500)', max: 3, placeholder: 'e.g. 10' }),
  ]);

// '#2A5BFF', '2a5bff' ou 'abc' → '#2a5bff' ; null si invalide.
function parseHex(raw) {
  const hex = raw.trim().replace(/^#/, '');
  if (/^[0-9a-f]{6}$/i.test(hex)) return `#${hex.toLowerCase()}`;
  if (/^[0-9a-f]{3}$/i.test(hex)) return `#${[...hex.toLowerCase()].map((ch) => ch + ch).join('')}`;
  return null;
}

function parseNumber(raw, { min, max, integer = false }) {
  const n = Number(raw.trim().replace(',', '.'));
  if (!raw.trim() || !Number.isFinite(n) || n < min || n > max || (integer && !Number.isInteger(n))) return null;
  return integer ? n : Math.round(n * 100) / 100;
}

// Enregistre une fenêtre d'édition ; une valeur invalide garde l'ancienne et produit un avertissement.
function saveEditor(interaction, section) {
  const editor = EDITORS[section];
  const warnings = [];
  const updates = {};
  const current = getConfig(interaction.guildId)[editor.key];
  for (const spec of editor.inputs) {
    const raw = interaction.fields.getTextInputValue(spec.id) ?? '';
    if (spec.kind === 'color') {
      if (!raw.trim()) continue;
      const hex = parseHex(raw);
      if (hex) updates[spec.id] = hex;
      else warnings.push(`⚠️ \`${clip(raw.trim(), 20)}\` isn't a hex colour — kept \`${current[spec.id]}\`.`);
    } else if (spec.kind === 'number' || spec.kind === 'integer') {
      if (!raw.trim()) continue;
      const n = parseNumber(raw, { min: spec.min, max: spec.max, integer: spec.kind === 'integer' });
      if (n === null) {
        warnings.push(
          `⚠️ ${spec.label.replace(/ \(.*$/, '')} must be ${spec.kind === 'integer' ? 'a whole number' : 'a number'} ` +
            `between ${spec.min} and ${spec.max} — kept \`${current[spec.id]}\`.`,
        );
      } else updates[spec.id] = n;
    } else {
      updates[spec.id] = raw.trim() || DEFAULT_CONFIG[editor.key][spec.id];
    }
  }
  if (section === 'voice' && updates.nameTemplate && !updates.nameTemplate.includes('{username}')) {
    warnings.push('⚠️ The name has no `{username}`: every created channel will have the same name.');
  }
  updateConfig(interaction.guildId, (c) => Object.assign(c[editor.key], updates));
  return ['✅ Settings saved.', ...warnings].join('\n');
}

// --- Auto Setup ---

// Structure créée par l'Auto Setup. access : droits du salon/de la catégorie (voir overwritesFor).
const LAYOUT = [
  {
    name: '📌 INFORMATION',
    access: 'readOnly',
    channels: [
      { key: 'rules', name: '📜・rules', access: 'rules', current: (c) => c.rules.channelId },
      { key: 'welcome', name: '👋・welcome', access: 'readOnly', current: (c) => c.welcome.channelId },
      { key: 'verify', name: '✅・verify', access: 'verify', current: (c) => c.verification.channelId },
      { key: 'boosts', name: '💎・boosts', access: 'readOnly', current: (c) => c.boosts.channelId },
    ],
  },
  {
    name: '📈 LEVELS',
    access: 'readOnly',
    channels: [{ key: 'levelUps', name: '📈・level-ups', access: 'readOnly', current: (c) => c.levels.channelId }],
  },
  {
    name: '🎫 SUPPORT',
    access: 'readOnly',
    channels: [
      { key: 'ticketPanel', name: '🎫・open-a-ticket', access: 'readOnly', current: (c) => c.tickets.panelChannelId },
    ],
  },
  { key: 'ticketCategory', name: '🎫 TICKETS', access: 'tickets', current: (c) => c.tickets.categoryId, channels: [] },
  {
    name: '🔊 VOICE',
    access: 'none',
    channels: [
      {
        key: 'voice',
        name: '➕ Create your voice',
        type: ChannelType.GuildVoice,
        access: 'none',
        current: (c) => c.tempVoice.creatorChannelId,
      },
    ],
  },
  {
    name: '🔒 STAFF',
    access: 'private',
    channels: [
      { key: 'memberLogs', name: 'member-logs', access: 'private', current: (c) => c.logs.memberChannelId },
      { key: 'roleLogs', name: 'role-logs', access: 'private', current: (c) => c.logs.roleChannelId },
      { key: 'ticketLogs', name: 'ticket-logs', access: 'private', current: (c) => c.tickets.logChannelId },
    ],
  },
];
const ACCESS_NOTE = { readOnly: 'read-only', private: 'staff only', tickets: 'private' };

// Ce qui existe déjà : salon configuré encore présent, sinon salon du même nom dans la catégorie du même nom.
export function planAutoSetup(guild, cfg) {
  const find = (name, type, parentId) =>
    guild.channels.cache.find(
      (ch) => ch.name === name && ch.type === type && (!parentId || ch.parentId === parentId),
    ) ?? null;
  const configured = (id, types) => {
    const channel = channelOf(guild, id);
    return channel && types.includes(channel.type) ? channel : null;
  };
  const role =
    roleOf(guild, cfg.verification.roleId) ??
    guild.roles.cache.find((r) => r.name === 'Verified' && !r.managed && r.id !== guild.id) ??
    null;
  const categories = LAYOUT.map((cat) => {
    const existing =
      (cat.current && configured(cat.current(cfg), CATEGORY)) || find(cat.name, ChannelType.GuildCategory);
    const channels = cat.channels.map((ch) => {
      const type = ch.type ?? ChannelType.GuildText;
      const found =
        configured(ch.current(cfg), type === ChannelType.GuildVoice ? VOICE : TEXT) ||
        (existing && find(ch.name, type, existing.id)) ||
        null;
      return { ...ch, type, existing: found };
    });
    const needed = Boolean(cat.key) || channels.some((ch) => !ch.existing);
    return { ...cat, existing, channels, create: !existing && needed };
  });
  return { role, categories };
}

export function autoConfirmView(guild, cfg) {
  const plan = planAutoSetup(guild, cfg);
  const create = [];
  const reuse = [];
  if (plan.role) reuse.push(`${roleText(guild, plan.role.id)} (verified role)`);
  else create.push('Role **Verified** (green)');
  for (const cat of plan.categories) {
    const fresh = cat.channels
      .filter((ch) => !ch.existing)
      .map((ch) => `\`${ch.type === ChannelType.GuildVoice ? '🔊 ' : '#'}${ch.name}\``);
    const note = ACCESS_NOTE[cat.access] ? ` (${ACCESS_NOTE[cat.access]})` : '';
    if (cat.create) create.push(`**${cat.name}**${note}${fresh.length ? ` — ${fresh.join(', ')}` : ''}`);
    else if (fresh.length) create.push(`In **${cat.existing.name}** — ${fresh.join(', ')}`);
    else if (cat.key) reuse.push(`**${cat.existing.name}** (tickets category)`);
    for (const ch of cat.channels) if (ch.existing) reuse.push(`<#${ch.existing.id}>`);
  }
  const missing = missingPerms(guild, [P.ManageChannels, P.ManageRoles]);
  const embed = panelEmbed('⚡ Auto Setup', COLORS.green).setDescription(
    join([
      'Creates the recommended structure. Existing channels and roles are **reused and never modified**.',
      create.length ? `**🆕 Will be created**\n${bullets(create)}` : '**🆕 Nothing to create** — everything exists.',
      reuse.length ? `**♻️ Already there (reused)**\n${bullets(reuse)}` : null,
      `**Then**\n${bullets([
        'Enables Rules, Welcome, Verification, Levels, Tickets, Logs, Create your voice and Boosts',
        'Posts the rules and the verification and ticket panels (or updates them if they are already posted)',
        `Creates the level ranks if no level reward exists yet: ${RANKS_TEXT}`,
        'Posts the Rank System info in a newly created level-ups channel',
      ])}`,
      missing.length ? `❌ I need **${missing.join(', ')}** to run the Auto Setup.` : null,
    ]),
  );
  return {
    embeds: [embed],
    components: [
      row(
        button('setup:auto:run', 'Confirm', { style: ButtonStyle.Success, emoji: '✅', disabled: missing.length > 0 }),
        button('setup:home', 'Cancel'),
      ),
    ],
  };
}

// Droits des salons créés. Le bot ne peut autoriser/refuser que des permissions qu'il possède : on filtre.
// Lecture seule = rien d'autorisé explicitement à @everyone (hors salon de vérification) : « Restrict @everyone »
// peut ainsi cacher ces salons aux non-vérifiés (verification.restrictEveryone signale les salons ouverts à @everyone).
function overwritesFor(guild, access, verifiedRole) {
  const me = guild.members.me;
  const own = (flags) => flags.filter((flag) => flag !== undefined && me?.permissions.has(flag));
  const bot = (extra = []) =>
    me
      ? [
          {
            id: me.id,
            type: OverwriteType.Member,
            allow: own([P.ViewChannel, P.SendMessages, P.EmbedLinks, P.AttachFiles, P.ReadMessageHistory, ...extra]),
          },
        ]
      : [];
  const readOnlyDeny = own([
    P.SendMessages,
    P.AddReactions,
    P.CreatePublicThreads,
    P.CreatePrivateThreads,
    P.SendMessagesInThreads,
    P.SendPolls,
  ]);
  const everyone = (allow, deny) => ({ id: guild.id, type: OverwriteType.Role, allow: own(allow), deny });
  switch (access) {
    case 'verify':
      return [
        everyone([P.ViewChannel, P.ReadMessageHistory], readOnlyDeny),
        ...(verifiedRole ? [{ id: verifiedRole.id, type: OverwriteType.Role, deny: own([P.ViewChannel]) }] : []),
        ...bot(),
      ];
    case 'rules':
      // Lisible par tous, y compris les non-vérifiés après « Restrict @everyone » : on lit le règlement avant.
      return [everyone([P.ViewChannel, P.ReadMessageHistory], readOnlyDeny), ...bot()];
    case 'readOnly':
      return [everyone([], readOnlyDeny), ...bot()];
    case 'private':
      return [everyone([], own([P.ViewChannel])), ...bot()];
    case 'tickets':
      return [everyone([], own([P.ViewChannel])), ...bot([P.ManageChannels])];
    default:
      return [];
  }
}

async function createChannel(guild, options, report) {
  try {
    const channel = await guild.channels.create({ ...options, reason: `${BRAND} auto setup` });
    report.created.push(
      channel.type === ChannelType.GuildCategory ? `**${channel.name}** category` : `<#${channel.id}>`,
    );
    return channel;
  } catch (error) {
    report.errors.push(`Couldn't create \`${options.name}\`: ${error.message}`);
    return null;
  }
}

// postVerificationPanel / postTicketPanel modifient le panneau déjà posté s'il existe : pas de doublon.
async function postPanel(guild, report, { label, channelId, post }) {
  const message = await Promise.resolve()
    .then(() => post(guild))
    .catch((error) => {
      log.warn(`Auto setup: ${label} in ${guild.name}: ${error.message}`);
      return null;
    });
  if (message) report.posted.push(`${label} in <#${message.channelId}>`);
  else report.errors.push(`Couldn't post the ${label} in <#${channelId}> — check my permissions there.`);
}

const autoRunning = new Set();

export async function runAutoSetup(guild) {
  const report = { created: [], reused: [], posted: [], errors: [], aborted: false, role: null };
  const missing = missingPerms(guild, [P.ManageChannels, P.ManageRoles]);
  if (missing.length) {
    report.aborted = true;
    report.errors.push(`I need **${missing.join(', ')}** to create channels and roles.`);
    return report;
  }
  const plan = planAutoSetup(guild, getConfig(guild.id));

  let role = plan.role;
  if (role) report.reused.push(`<@&${role.id}> (verified role)`);
  else {
    role = await guild.roles
      .create({ name: 'Verified', colors: { primaryColor: HEX.green }, reason: `${BRAND} auto setup` })
      .catch((error) => {
        report.errors.push(`Couldn't create the **Verified** role: ${error.message}`);
        return null;
      });
    if (role) report.created.push(`<@&${role.id}> role`);
  }
  report.role = role;

  const ids = {};
  const fresh = new Set();
  for (const cat of plan.categories) {
    let category = cat.existing;
    if (cat.create) {
      category = await createChannel(
        guild,
        {
          name: cat.name,
          type: ChannelType.GuildCategory,
          permissionOverwrites: overwritesFor(guild, cat.access, role),
        },
        report,
      );
    }
    if (cat.key && category) {
      ids[cat.key] = category.id;
      if (!cat.create) report.reused.push(`**${category.name}** category`);
    }
    for (const ch of cat.channels) {
      if (ch.existing) {
        ids[ch.key] = ch.existing.id;
        report.reused.push(`<#${ch.existing.id}>`);
        continue;
      }
      if (!category) {
        report.errors.push(`Skipped \`${ch.name}\` because its category couldn't be created.`);
        continue;
      }
      const channel = await createChannel(
        guild,
        {
          name: ch.name,
          type: ch.type,
          parent: category.id,
          permissionOverwrites: overwritesFor(guild, ch.access, role),
        },
        report,
      );
      if (channel) {
        ids[ch.key] = channel.id;
        fresh.add(ch.key);
      }
    }
  }

  const cfg = updateConfig(guild.id, (c) => {
    if (ids.rules) Object.assign(c.rules, { channelId: ids.rules, enabled: true });
    if (ids.welcome) Object.assign(c.welcome, { channelId: ids.welcome, enabled: true });
    if (ids.verify) c.verification.channelId = ids.verify;
    if (role) c.verification.roleId = role.id;
    if (ids.verify && role) c.verification.enabled = true;
    if (ids.levelUps) c.levels.channelId = ids.levelUps;
    c.levels.enabled = true;
    if (ids.ticketPanel) c.tickets.panelChannelId = ids.ticketPanel;
    if (ids.ticketCategory) c.tickets.categoryId = ids.ticketCategory;
    if (ids.ticketLogs) c.tickets.logChannelId = ids.ticketLogs;
    if (ids.ticketPanel && ids.ticketCategory) c.tickets.enabled = true;
    if (ids.memberLogs) c.logs.memberChannelId = ids.memberLogs;
    if (ids.roleLogs) c.logs.roleChannelId = ids.roleLogs;
    if (ids.voice) Object.assign(c.tempVoice, { creatorChannelId: ids.voice, enabled: true });
    if (ids.boosts) Object.assign(c.boosts, { channelId: ids.boosts, enabled: true });
  });

  // Grades de niveau, seulement si aucune récompense n'est encore configurée (après le rôle vérifié : ils se
  // placent au-dessus de lui).
  if (!cfg.levels.rewards?.length) {
    const ranks = await createRankRoles(guild);
    report.created.push(...ranks.created.map((r) => `${r} rank`));
    report.reused.push(...ranks.reused.map((r) => `${r} rank`));
    report.errors.push(...ranks.errors);
  }

  // Le règlement d'abord : le panneau de vérification y renvoie.
  if (cfg.rules.enabled) {
    await postPanel(guild, report, { label: 'rules', channelId: cfg.rules.channelId, post: postRulesPanel });
  }
  if (cfg.verification.enabled) {
    await postPanel(guild, report, {
      label: 'verification panel',
      channelId: cfg.verification.channelId,
      post: postVerificationPanel,
    });
  }
  if (cfg.tickets.enabled) {
    await postPanel(guild, report, {
      label: 'ticket panel',
      channelId: cfg.tickets.panelChannelId,
      post: postTicketPanel,
    });
  }
  if (fresh.has('levelUps')) {
    const message = await sendTo(guild, ids.levelUps, { embeds: [buildRankSystemEmbed(guild)] }).catch(() => null);
    if (message) report.posted.push(`Rank System info in <#${ids.levelUps}>`);
    else report.errors.push(`Couldn't post the Rank System info in <#${ids.levelUps}>.`);
  }
  return report;
}

export function autoResultView(guild, cfg, report) {
  const me = guild.members.me;
  const role = report.role;
  const title = report.aborted
    ? '❌ Auto Setup failed'
    : report.errors.length
      ? '⚠️ Auto Setup finished with errors'
      : '✅ Auto Setup complete';
  const color = report.aborted ? COLORS.red : report.errors.length ? COLORS.gold : COLORS.green;
  const embed = panelEmbed(title, color);
  if (report.created.length) embed.addFields(field('🆕 Created', bullets(report.created)));
  if (report.reused.length) embed.addFields(field('♻️ Reused (not modified)', bullets(report.reused)));
  if (report.posted.length) embed.addFields(field('📤 Posted / updated', bullets(report.posted)));
  if (report.errors.length) embed.addFields(field('❌ Errors', bullets(report.errors)));
  if (!report.aborted) {
    const steps = [];
    if (!cfg.tickets.staffRoleIds?.length) steps.push('Pick a staff role in **Tickets** so your team sees tickets.');
    steps.push('Use **🔒 Restrict @everyone** in **Verification** to hide the server from unverified members.');
    steps.push('Customize the messages with **✏️ Edit** in each module.');
    embed.addFields(field('👉 Next steps', bullets(steps)));
  }
  if (role && me && role.position >= me.roles.highest.position) {
    embed.addFields(
      field(
        '⚠️ Role hierarchy',
        `${roleText(guild, role.id)} is above my highest role, so I can't give it. ` +
          `Fix: Server Settings → Roles → drag **${botRoleName(guild)}** above **${role.name}**.`,
      ),
    );
  }
  return { embeds: [embed], components: [row(backButton())] };
}

// --- Handlers ---

async function guard(interaction) {
  if (!interaction.inGuild() || !interaction.guild) {
    await replyError(interaction, 'This panel only works in a server.');
    return false;
  }
  // Administrateur requis : le panneau choisit des rôles que le bot distribue, crée des rôles/salons et
  // modifie @everyone. « Gérer le serveur » seul permettrait à un modérateur de s'attribuer un rôle supérieur.
  if (!interaction.memberPermissions?.has(P.Administrator)) {
    await replyError(interaction, 'You need the **Administrator** permission to use the setup panel.');
    return false;
  }
  return true;
}

const render = (view, guild, notice) => VIEWS[view](guild, getConfig(guild.id), notice);

// Action lente (envoi d'un message) : on diffère, puis on réaffiche la vue avec le résultat.
async function deferred(interaction, view, work) {
  await interaction.deferUpdate();
  let notice;
  try {
    notice = await work();
  } catch (error) {
    log.error(`Setup ${interaction.customId}`, error);
    notice = `❌ Something went wrong: ${clip(error.message, 200)}`;
  }
  return interaction.editReply(render(view, interaction.guild, notice));
}

function toggle(guild, key, ready, hint) {
  const cfg = getConfig(guild.id);
  if (!cfg[key].enabled && !ready(cfg)) return `❌ ${hint}`;
  const next = updateConfig(guild.id, (c) => {
    c[key].enabled = !c[key].enabled;
  });
  return next[key].enabled ? '🟢 Module enabled.' : '🔴 Module disabled.';
}

function setChannel(guild, apply, id, label) {
  updateConfig(guild.id, (c) => apply(c, id ?? null));
  return id ? `✅ ${label} set to <#${id}>.` : `✅ ${label} cleared.`;
}

// Rôle choisi dans un menu : refusé s'il est géré par une intégration, accepté avec avertissement sinon.
function setRole(guild, apply, id, label) {
  const role = roleOf(guild, id);
  if (role && (role.managed || role.id === guild.id)) {
    return `❌ <@&${role.id}> can't be given by a bot — pick another role.`;
  }
  updateConfig(guild.id, (c) => apply(c, id ?? null));
  if (!id) return `✅ ${label} cleared.`;
  return canManageRole(guild, role)
    ? `✅ ${label} set to <@&${id}>.`
    : `⚠️ ${label} set to <@&${id}>, but I can't give it yet.`;
}

async function memberOf(interaction) {
  if (interaction.member?.guild) return interaction.member;
  return interaction.guild.members.fetch(interaction.user.id).catch(() => null);
}

const SECTIONS = {
  async rules(interaction, action, cfg) {
    const { guild } = interaction;
    let notice;
    switch (action) {
      case 'channel':
        notice = setChannel(guild, (c, id) => (c.rules.channelId = id), interaction.values[0], 'Rules channel');
        break;
      case 'toggle':
        notice = toggle(guild, 'rules', (c) => channelOf(guild, c.rules.channelId), 'Pick a rules channel first.');
        break;
      case 'edit':
        return interaction.showModal(editorModal('rules', cfg));
      case 'post':
        return deferred(interaction, 'rules', async () => {
          if (!channelOf(guild, cfg.rules.channelId)) return '❌ Pick a rules channel first.';
          const message = await postRulesPanel(guild);
          if (!message) {
            return `❌ I can't post the rules in <#${cfg.rules.channelId}> — I need to view the channel and send ` +
              'messages/embeds.';
          }
          // Le panneau de vérification déjà posté reprend le lien vers le règlement.
          if (cfg.verification.panelMessageId) await postVerificationPanel(guild).catch(() => null);
          return `📤 Rules posted in <#${message.channelId}>.`;
        });
    }
    return interaction.update(render('rules', guild, notice));
  },

  async welcome(interaction, action, cfg) {
    const { guild } = interaction;
    const value = interaction.values?.[0];
    let notice;
    switch (action) {
      case 'channel':
        notice = setChannel(guild, (c, id) => (c.welcome.channelId = id), value, 'Welcome channel');
        break;
      case 'autorole':
        notice = setRole(guild, (c, id) => (c.welcome.autoRoleId = id), value, 'Auto-role');
        break;
      case 'toggle':
        notice = toggle(guild, 'welcome', (c) => channelOf(guild, c.welcome.channelId), 'Pick a channel first.');
        break;
      case 'image':
      case 'dm':
        updateConfig(guild.id, (c) => {
          c.welcome[action] = !c.welcome[action];
        });
        break;
      case 'edit':
        return interaction.showModal(editorModal('welcome', cfg));
      case 'test':
        return deferred(interaction, 'welcome', async () => {
          if (!channelOf(guild, cfg.welcome.channelId)) return '❌ Pick a welcome channel first.';
          const member = await memberOf(interaction);
          const message = member && (await sendWelcome(member, { test: true }));
          return message
            ? `🧪 Test message sent in <#${message.channelId}>.`
            : `❌ I couldn't send the test message in <#${cfg.welcome.channelId}> — check my permissions there.`;
        });
    }
    return interaction.update(render('welcome', guild, notice));
  },

  async verify(interaction, action, cfg) {
    const { guild } = interaction;
    const c = cfg.verification;
    const value = interaction.values?.[0];
    let notice;
    switch (action) {
      case 'channel':
        notice = setChannel(guild, (x, id) => (x.verification.channelId = id), value, 'Verification channel');
        break;
      case 'role':
        notice = setRole(guild, (x, id) => (x.verification.roleId = id), value, 'Verified role');
        break;
      case 'toggle':
        notice = toggle(
          guild,
          'verification',
          (x) => channelOf(guild, x.verification.channelId) && roleOf(guild, x.verification.roleId),
          'Pick the verification channel and the verified role first.',
        );
        break;
      case 'edit':
        return interaction.showModal(editorModal('verify', cfg));
      case 'post':
        return deferred(interaction, 'verify', async () => {
          if (!channelOf(guild, c.channelId)) return '❌ Pick the verification channel first.';
          if (!roleOf(guild, c.roleId)) return '❌ Pick the verified role first.';
          const message = await postVerificationPanel(guild);
          return message
            ? `📤 Panel posted in <#${message.channelId}>.`
            : `❌ I can't post the panel in <#${c.channelId}> — I need to view the channel and send messages/embeds.`;
        });
      case 'restrict':
        if (!channelOf(guild, c.channelId) || !roleOf(guild, c.roleId)) {
          notice = '❌ Pick the verification channel and the verified role first.';
          break;
        }
        return interaction.update(restrictConfirmView(guild, cfg));
      case 'restrictrun': {
        await interaction.deferUpdate();
        let result;
        try {
          result = await restrictEveryone(guild);
        } catch (error) {
          log.error('Restrict @everyone', error);
          result = { changes: [], errors: [error.message] };
        }
        return interaction.editReply(restrictResultView(guild, result));
      }
    }
    return interaction.update(render('verify', guild, notice));
  },

  async levels(interaction, action, cfg) {
    const { guild } = interaction;
    let notice;
    switch (action) {
      case 'channel':
        notice = setChannel(guild, (c, id) => (c.levels.channelId = id), interaction.values[0], 'Level-up channel');
        if (!interaction.values[0]) notice += ' Level-ups are announced in the channel where the member talked.';
        break;
      case 'ignored':
        updateConfig(guild.id, (c) => {
          c.levels.ignoredChannelIds = [...interaction.values];
        });
        notice = `✅ ${plural(interaction.values.length, 'ignored channel')}.`;
        break;
      case 'toggle':
        notice = toggle(guild, 'levels', () => true);
        break;
      case 'stack':
        updateConfig(guild.id, (c) => {
          c.levels.stackRewards = !c.levels.stackRewards;
        });
        break;
      case 'edit':
        return interaction.showModal(editorModal('levels', cfg));
      case 'rankinfo':
        return deferred(interaction, 'levels', async () => {
          const targetId = channelOf(guild, cfg.levels.channelId) ? cfg.levels.channelId : interaction.channelId;
          const message = await sendTo(guild, targetId, { embeds: [buildRankSystemEmbed(guild)] });
          return message
            ? `📤 Rank System info posted in <#${targetId}>.`
            : `❌ I can't send messages in <#${targetId}>.`;
        });
    }
    return interaction.update(render('levels', guild, notice));
  },

  async reward(interaction, action, cfg, args) {
    const { guild } = interaction;
    let notice;
    switch (action) {
      case 'add':
        return interaction.showModal(rewardModal());
      case 'ranks':
        return deferred(interaction, 'rewards', async () => ranksNotice(await createRankRoles(guild)));
      case 'set': {
        const level = Number(args[0]);
        const roleId = interaction.values?.[0];
        const role = roleOf(guild, roleId);
        if (!Number.isInteger(level) || level < 1 || level > 500 || !role) {
          notice = '❌ Invalid reward.';
        } else if (role.managed || role.id === guild.id) {
          notice = `❌ <@&${role.id}> can't be given by a bot — pick another role.`;
        } else {
          updateConfig(guild.id, (c) => {
            c.levels.rewards = [...sortedRewards(c.levels).filter((r) => r.level !== level), { level, roleId }].sort(
              (a, b) => a.level - b.level,
            );
          });
          notice = canManageRole(guild, role)
            ? `✅ Level ${level} → <@&${roleId}> saved.`
            : `⚠️ Level ${level} → <@&${roleId}> saved, but I can't give this role yet.`;
        }
        break;
      }
      case 'remove': {
        const level = Number(interaction.values?.[0]);
        updateConfig(guild.id, (c) => {
          c.levels.rewards = sortedRewards(c.levels).filter((r) => r.level !== level);
        });
        notice = `🗑️ Level ${level} reward removed.`;
        break;
      }
    }
    return interaction.update(render('rewards', guild, notice));
  },

  async tickets(interaction, action, cfg) {
    const { guild } = interaction;
    const value = interaction.values?.[0];
    let notice;
    switch (action) {
      case 'panel':
        notice = setChannel(guild, (c, id) => (c.tickets.panelChannelId = id), value, 'Panel channel');
        break;
      case 'category':
        notice = setChannel(guild, (c, id) => (c.tickets.categoryId = id), value, 'Tickets category');
        break;
      case 'log':
        notice = setChannel(guild, (c, id) => (c.tickets.logChannelId = id), value, 'Transcript channel');
        break;
      case 'staff': {
        // @everyone ou un rôle d'intégration comme rôle staff donnerait les droits staff à tout le monde.
        const ids = interaction.values.filter((id) => id !== guild.id && !guild.roles.cache.get(id)?.managed);
        updateConfig(guild.id, (c) => {
          c.tickets.staffRoleIds = ids;
        });
        notice = `✅ ${plural(ids.length, 'staff role')} selected.`;
        if (ids.length < interaction.values.length) notice += ' @everyone and bot roles were ignored.';
        break;
      }
      case 'toggle':
        notice = toggle(
          guild,
          'tickets',
          (c) => channelOf(guild, c.tickets.panelChannelId) && channelOf(guild, c.tickets.categoryId),
          'Pick the panel channel and the tickets category first.',
        );
        break;
      case 'edit':
        return interaction.showModal(editorModal('tickets', cfg));
      case 'post':
        return deferred(interaction, 'tickets', async () => {
          if (!channelOf(guild, cfg.tickets.panelChannelId)) return '❌ Pick a panel channel first.';
          const message = await postTicketPanel(guild);
          return message
            ? `📤 Panel posted in <#${message.channelId}>.`
            : `❌ I can't post the panel in <#${cfg.tickets.panelChannelId}> — I need to view the channel and ` +
                'send messages/embeds.';
        });
    }
    return interaction.update(render('tickets', guild, notice));
  },

  async logs(interaction, action) {
    const { guild } = interaction;
    const value = interaction.values?.[0];
    let notice;
    if (action === 'member') {
      notice = setChannel(guild, (c, id) => (c.logs.memberChannelId = id), value, 'Join/leave log channel');
    } else if (action === 'role') {
      notice = setChannel(guild, (c, id) => (c.logs.roleChannelId = id), value, 'Role log channel');
    }
    return interaction.update(render('logs', guild, notice));
  },

  async voice(interaction, action, cfg) {
    const { guild } = interaction;
    const value = interaction.values?.[0];
    let notice;
    switch (action) {
      case 'creator':
        notice = setChannel(guild, (c, id) => (c.tempVoice.creatorChannelId = id), value, 'Creator channel');
        break;
      case 'category':
        notice = setChannel(guild, (c, id) => (c.tempVoice.categoryId = id), value, 'Category');
        break;
      case 'toggle':
        notice = toggle(
          guild,
          'tempVoice',
          (c) => channelOf(guild, c.tempVoice.creatorChannelId),
          'Pick the "Create your voice" channel first.',
        );
        break;
      case 'edit':
        return interaction.showModal(editorModal('voice', cfg));
    }
    return interaction.update(render('voice', guild, notice));
  },

  async boosts(interaction, action, cfg) {
    const { guild } = interaction;
    let notice;
    switch (action) {
      case 'channel':
        notice = setChannel(guild, (c, id) => (c.boosts.channelId = id), interaction.values[0], 'Boost channel');
        break;
      case 'toggle':
        notice = toggle(guild, 'boosts', (c) => channelOf(guild, c.boosts.channelId), 'Pick a boost channel first.');
        break;
      case 'edit':
        return interaction.showModal(editorModal('boosts', cfg));
      case 'test':
        return deferred(interaction, 'boosts', async () => {
          if (!channelOf(guild, cfg.boosts.channelId)) return '❌ Pick a boost channel first.';
          const member = await memberOf(interaction);
          const message = member && (await sendBoostMessage(member, { test: true }));
          return message
            ? `🧪 Test message sent in <#${message.channelId}>.`
            : `❌ I couldn't send the test message in <#${cfg.boosts.channelId}> — check my permissions there.`;
        });
    }
    return interaction.update(render('boosts', guild, notice));
  },
};

async function handleComponent(interaction, [section, action, ...args]) {
  if (!(await guard(interaction))) return;
  const { guild } = interaction;
  const cfg = getConfig(guild.id);
  switch (section) {
    case 'home':
    case 'refresh':
      return interaction.update(homeView(guild, cfg));
    case 'module':
      return interaction.update(render(VIEWS[interaction.values?.[0]] ? interaction.values[0] : 'home', guild));
    case 'auto': {
      if (action !== 'run') return interaction.update(autoConfirmView(guild, cfg));
      if (autoRunning.has(guild.id)) {
        return interaction.update(homeView(guild, cfg, '⏳ The Auto Setup is already running — wait a few seconds.'));
      }
      autoRunning.add(guild.id);
      try {
        await interaction.deferUpdate();
        const report = await runAutoSetup(guild).catch((error) => {
          log.error('Auto setup', error);
          return { created: [], reused: [], posted: [], errors: [`Unexpected error: ${error.message}`], aborted: true };
        });
        return await interaction.editReply(autoResultView(guild, getConfig(guild.id), report));
      } finally {
        autoRunning.delete(guild.id);
      }
    }
    default: {
      const handler = SECTIONS[section];
      if (handler) return handler(interaction, action, cfg, args);
    }
  }
}

async function handleModal(interaction, [section, action]) {
  if (!(await guard(interaction))) return;
  const { guild } = interaction;
  let view;
  if (section === 'reward' && action === 'add') {
    const level = parseNumber(interaction.fields.getTextInputValue('level') ?? '', { min: 1, max: 500, integer: true });
    view =
      level === null
        ? render('rewards', guild, '❌ The level must be a whole number between 1 and 500.')
        : rewardRoleView(guild, getConfig(guild.id), level);
  } else if (action === 'edit' && EDITORS[section]) {
    view = render(section, guild, saveEditor(interaction, section));
  } else return;
  if (interaction.isFromMessage()) return interaction.update(view);
  return interaction.reply({ ...view, flags: MessageFlags.Ephemeral });
}

// Réponse à /setup : l'accueil du panneau, en éphémère.
export async function showSetupPanel(interaction) {
  if (!(await guard(interaction))) return;
  const view = homeView(interaction.guild, getConfig(interaction.guild.id));
  return interaction.reply({ ...view, flags: MessageFlags.Ephemeral });
}

// client non utilisé : tout passe par le routeur (espace « setup »).
export function register(client) {
  void client;
  onComponent('setup', handleComponent);
  onModal('setup', handleModal);
}
