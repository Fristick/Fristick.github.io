// Tickets : panneau avec menu de catégories, un salon privé par ticket, prise en charge par le staff,
// fermeture avec transcript HTML envoyé dans le salon de logs et en MP à l'auteur.
import {
  ActionRowBuilder,
  AttachmentBuilder,
  ButtonBuilder,
  ButtonStyle,
  ChannelType,
  EmbedBuilder,
  Events,
  LabelBuilder,
  MessageFlags,
  ModalBuilder,
  OverwriteType,
  PermissionFlagsBits,
  RESTJSONErrorCodes,
  StringSelectMenuBuilder,
  StringSelectMenuOptionBuilder,
  TextInputBuilder,
  TextInputStyle,
} from 'discord.js';
import { setTimeout as sleep } from 'node:timers/promises';
import { BRAND, COLORS, DEFAULT_CONFIG, TICKET_CATEGORIES } from '../config.mjs';
import { db, getConfig, updateConfig } from '../db.mjs';
import { onComponent, onModal } from '../router.mjs';
import {
  baseEmbed,
  errorEmbed,
  infoEmbed,
  replyEphemeral,
  replyError,
  replySuccess,
  warnEmbed,
} from '../utils/embeds.mjs';
import { listen, log } from '../utils/log.mjs';
import { missingPerms, sendTo } from '../utils/permissions.mjs';
import { fill } from '../utils/placeholders.mjs';
import { buildTranscript, formatDuration, padNumber } from './transcript.mjs';

const P = PermissionFlagsBits;
const CLOSE_DELAY_MS = 5000;
const PENDING_TTL_MS = 2 * 60_000; // ligne réservée sans salon : création en cours (ou plantage si plus vieille)
const MAX_CHANNELS_PER_CATEGORY = 50;

// Accès donné à l'auteur (et via /ticket add) ; le staff peut en plus gérer les messages.
export const TICKET_MEMBER_PERMS = [P.ViewChannel, P.SendMessages, P.ReadMessageHistory, P.AttachFiles, P.EmbedLinks];
const STAFF_PERMS = [...TICKET_MEMBER_PERMS, P.ManageMessages];
const BOT_PERMS = [...TICKET_MEMBER_PERMS, P.ManageChannels, P.ManageMessages];
// Le bot ne peut accorder dans un salon que des permissions qu'il possède lui-même.
const CREATE_PERMS = [...new Set([P.ManageChannels, P.ManageRoles, ...BOT_PERMS])];

const stmt = {
  byChannel: db.prepare('SELECT * FROM tickets WHERE channel_id = ?'),
  openByUser: db.prepare("SELECT * FROM tickets WHERE guild_id = ? AND user_id = ? AND status = 'open'"),
  allOpen: db.prepare("SELECT * FROM tickets WHERE status = 'open'"),
  nextNumber: db.prepare('SELECT COALESCE(MAX(number), 0) + 1 AS next FROM tickets WHERE guild_id = ?'),
  insert: db.prepare(
    `INSERT INTO tickets (guild_id, number, user_id, category, subject, details, status, created_at)
     VALUES (?, ?, ?, ?, ?, ?, 'open', ?)`,
  ),
  setChannel: db.prepare('UPDATE tickets SET channel_id = ? WHERE id = ?'),
  remove: db.prepare('DELETE FROM tickets WHERE id = ?'),
  claim: db.prepare('UPDATE tickets SET claimed_by = ? WHERE id = ? AND claimed_by IS NULL'),
  close: db.prepare(
    "UPDATE tickets SET status = 'closed', closed_at = ?, closed_by = ? WHERE id = ? AND status = 'open'",
  ),
  closeByChannel: db.prepare(
    "UPDATE tickets SET status = 'closed', closed_at = ? WHERE channel_id = ? AND status = 'open'",
  ),
};

const categoryOf = (id) => TICKET_CATEGORIES.find((c) => c.id === id) ?? null;
const categoryName = (id) => {
  const category = categoryOf(id);
  return category ? `${category.emoji} ${category.label}` : String(id);
};

export const getTicketByChannel = (channelId) => (channelId ? (stmt.byChannel.get(channelId) ?? null) : null);

// Nom de salon valide : minuscules, a-z 0-9 et tirets, 100 caractères max.
export function sanitizeChannelName(name) {
  return String(name ?? '')
    .normalize('NFKD')
    .replace(/\p{M}/gu, '') // accents retirés après décomposition NFKD
    .toLowerCase()
    .replace(/[^a-z0-9-]+/g, '-')
    .replace(/-{2,}/g, '-')
    .replace(/^-+/, '')
    .slice(0, 100)
    .replace(/-+$/, '');
}

const roleIdsOf = (member) => (member?.roles?.cache ? [...member.roles.cache.keys()] : (member?.roles ?? []));
const displayNameOf = (interaction) =>
  interaction.member?.displayName ?? interaction.member?.nick ?? interaction.user.displayName;

// Staff : un des rôles staff configurés, ou la permission Gérer les salons dans le salon de l'interaction.
export function isStaff(interaction, cfg = getConfig(interaction.guildId).tickets) {
  const roles = roleIdsOf(interaction.member);
  // @everyone (id = id du serveur) n'est jamais un rôle staff, même s'il a été enregistré par erreur.
  if (cfg.staffRoleIds?.some((id) => id !== interaction.guildId && roles.includes(id))) return true;
  return Boolean(interaction.memberPermissions?.has(P.ManageChannels));
}

const canClose = (interaction, ticket) => ticket.user_id === interaction.user.id || isStaff(interaction);

// Salon d'un serveur : undefined si introuvable pour une autre raison qu'une suppression (erreur réseau…).
async function fetchChannel(guild, channelId) {
  const cached = guild.channels.cache.get(channelId);
  if (cached) return cached;
  try {
    return await guild.channels.fetch(channelId);
  } catch (error) {
    return error.code === RESTJSONErrorCodes.UnknownChannel ? null : undefined;
  }
}

// Ticket ouvert d'un membre ; les lignes dont le salon a disparu sont fermées au passage.
async function findOpenTicket(guild, userId) {
  for (const ticket of stmt.openByUser.all(guild.id, userId)) {
    if (!ticket.channel_id) {
      if (Date.now() - ticket.created_at < PENDING_TTL_MS) return ticket;
    } else if ((await fetchChannel(guild, ticket.channel_id)) !== null) {
      return ticket;
    }
    stmt.close.run(Date.now(), null, ticket.id);
  }
  return null;
}

const existingTicketText = (ticket) =>
  ticket.channel_id
    ? `You already have an open ticket: <#${ticket.channel_id}>`
    : 'Your ticket is already being created, please wait a moment.';

function configProblem(guild, cfg) {
  if (!cfg.enabled || !cfg.categoryId) return "The ticket system isn't configured yet.";
  if (guild.channels.cache.get(cfg.categoryId)?.type !== ChannelType.GuildCategory) {
    return 'The ticket category no longer exists. Please tell an administrator.';
  }
  return null;
}

// ---------- Panneau ----------

function panelPayload(guild, cfg) {
  const defaults = DEFAULT_CONFIG.tickets;
  const title = fill(cfg.title, { guild }).slice(0, 256);
  const description = fill(cfg.message, { guild }).slice(0, 4096) || fill(defaults.message, { guild });
  const embed = baseEmbed(cfg.color).setDescription(description).setFooter({ text: BRAND });
  if (title) embed.setTitle(title);
  const menu = new StringSelectMenuBuilder()
    .setCustomId('ticket:open')
    .setPlaceholder('Select a category…')
    .addOptions(
      TICKET_CATEGORIES.map((category) =>
        new StringSelectMenuOptionBuilder()
          .setLabel(category.label)
          .setValue(category.id)
          .setDescription(category.description)
          .setEmoji(category.emoji),
      ),
    );
  return { embeds: [embed], components: [new ActionRowBuilder().addComponents(menu)] };
}

// Publie (ou met à jour) le panneau dans panelChannelId. Renvoie le message, ou null si impossible.
export async function postTicketPanel(guild) {
  const cfg = getConfig(guild.id).tickets;
  if (!cfg.panelChannelId) return null;
  const channel = await fetchChannel(guild, cfg.panelChannelId);
  if (!channel?.isTextBased()) return null;
  const missing = missingPerms(channel, [P.ViewChannel, P.SendMessages, P.EmbedLinks]);
  if (missing.length) {
    log.warn(`Ticket panel in #${channel.name} (${guild.name}): missing ${missing.join(', ')}`);
    return null;
  }
  const payload = panelPayload(guild, cfg);
  if (cfg.panelMessageId) {
    const existing = await channel.messages.fetch(cfg.panelMessageId).catch(() => null);
    if (existing?.editable) {
      const edited = await existing.edit(payload).catch(() => null);
      if (edited) return edited;
    }
  }
  const message = await channel.send(payload).catch((error) => {
    log.warn(`Ticket panel in #${channel.name} (${guild.name}): ${error.message}`);
    return null;
  });
  if (message) {
    updateConfig(guild.id, (config) => {
      config.tickets.panelMessageId = message.id;
    });
  }
  return message;
}

// ---------- Ouverture ----------

function ticketModal(category) {
  const subject = new TextInputBuilder()
    .setCustomId('subject')
    .setStyle(TextInputStyle.Short)
    .setRequired(true)
    .setMaxLength(100)
    .setPlaceholder('Briefly describe your request');
  const details = new TextInputBuilder()
    .setCustomId('details')
    .setStyle(TextInputStyle.Paragraph)
    .setRequired(false)
    .setMaxLength(1000)
    .setPlaceholder('Anything that could help us (optional)');
  return new ModalBuilder()
    .setCustomId(`ticket:create:${category.id}`)
    .setTitle(`${category.emoji} ${category.label}`.slice(0, 45))
    .addLabelComponents(
      new LabelBuilder().setLabel('Subject').setTextInputComponent(subject),
      new LabelBuilder().setLabel('Details').setTextInputComponent(details),
    );
}

async function handleOpen(interaction) {
  // En cas d'erreur, on renvoie le panneau tel quel pour vider le choix du menu (sinon la même catégorie ne
  // déclenche plus rien), puis l'erreur part en message éphémère.
  const fail = async (text) => {
    if (interaction.message) {
      await interaction.update({ components: interaction.message.components }).catch(() => {});
    }
    return replyError(interaction, text);
  };
  const category = categoryOf(interaction.values?.[0]);
  if (!category) return fail('Unknown ticket category.');
  const cfg = getConfig(interaction.guildId).tickets;
  const problem = configProblem(interaction.guild, cfg);
  if (problem) return fail(problem);
  const existing = await findOpenTicket(interaction.guild, interaction.user.id);
  if (existing) return fail(existingTicketText(existing));
  return interaction.showModal(ticketModal(category));
}

export function ticketButtons(claimerName = null) {
  const claim = new ButtonBuilder().setCustomId('ticket:claim').setEmoji('🙋');
  if (claimerName) {
    claim
      .setLabel(`Claimed by ${claimerName}`.slice(0, 80))
      .setStyle(ButtonStyle.Success)
      .setDisabled(true);
  } else {
    claim.setLabel('Claim').setStyle(ButtonStyle.Primary);
  }
  const close = new ButtonBuilder()
    .setCustomId('ticket:close')
    .setLabel('Close')
    .setEmoji('🔒')
    .setStyle(ButtonStyle.Danger);
  return new ActionRowBuilder().addComponents(claim, close);
}

async function acknowledgeModal(interaction) {
  // Formulaire venu du panneau : on « met à jour » le panneau à l'identique pour réinitialiser le menu.
  if (interaction.isFromMessage()) {
    const ok = await interaction
      .update({ components: interaction.message.components })
      .then(() => true)
      .catch(() => false);
    if (ok) return;
  }
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
}

async function handleCreate(interaction, categoryId) {
  const category = categoryOf(categoryId);
  if (!category) return replyError(interaction, 'Unknown ticket category.');
  await acknowledgeModal(interaction);

  const { guild, user } = interaction;
  const cfg = getConfig(guild.id).tickets;
  const problem = configProblem(guild, cfg);
  if (problem) return replyError(interaction, problem);
  const parent = guild.channels.cache.get(cfg.categoryId);
  const missing = missingPerms(parent, CREATE_PERMS);
  if (missing.length) {
    return replyError(
      interaction,
      `I'm missing these permissions in the ticket category: **${missing.join(', ')}**. Please tell an administrator.`,
    );
  }
  if (parent.children.cache.size >= MAX_CHANNELS_PER_CATEGORY) {
    return replyError(interaction, 'The ticket category is full (50 channels max). Please try again later.');
  }
  const existing = await findOpenTicket(guild, user.id);
  if (existing) return replyError(interaction, existingTicketText(existing));

  const subject = interaction.fields.getTextInputValue('subject').trim().slice(0, 100) || 'No subject';
  const details = interaction.fields.getTextInputValue('details')?.trim().slice(0, 1000) || null;

  // Vérification + réservation du numéro sans await entre les deux : deux formulaires simultanés = un seul ticket.
  if (stmt.openByUser.get(guild.id, user.id)) return replyError(interaction, existingTicketText({}));
  const createdAt = Date.now();
  const { next: number } = stmt.nextNumber.get(guild.id);
  const inserted = stmt.insert.run(guild.id, number, user.id, category.id, subject, details, createdAt);
  const ticketId = Number(inserted.lastInsertRowid);
  const num = padNumber(number);

  const staffRoleIds = (cfg.staffRoleIds ?? []).filter((id) => id !== guild.id && guild.roles.cache.has(id));
  let channel;
  try {
    channel = await guild.channels.create({
      name: sanitizeChannelName(`ticket-${num}-${user.username}`) || `ticket-${num}`,
      type: ChannelType.GuildText,
      parent: parent.id,
      topic: `Ticket #${num} · ${category.label} · opened by ${user.username}`,
      permissionOverwrites: [
        { id: guild.roles.everyone.id, type: OverwriteType.Role, deny: [P.ViewChannel] },
        { id: user.id, type: OverwriteType.Member, allow: TICKET_MEMBER_PERMS },
        ...staffRoleIds.map((id) => ({ id, type: OverwriteType.Role, allow: STAFF_PERMS })),
        { id: interaction.client.user.id, type: OverwriteType.Member, allow: BOT_PERMS },
      ],
      reason: `Ticket #${num} opened by ${user.username}`,
    });
  } catch (error) {
    stmt.remove.run(ticketId);
    log.warn(`Ticket #${num} (${guild.name}): channel creation failed: ${error.message}`);
    return replyError(interaction, `I couldn't create your ticket channel: ${error.message}`);
  }
  stmt.setChannel.run(channel.id, ticketId);

  const embed = baseEmbed(cfg.color)
    .setTitle(`🎫 Ticket #${num} — ${category.label}`)
    .setDescription('A staff member will be with you shortly.')
    .addFields(
      { name: 'Subject', value: subject },
      { name: 'Details', value: details ?? '*No details provided.*' },
      { name: 'Opened by', value: `<@${user.id}>`, inline: true },
      { name: 'Created', value: `<t:${Math.floor(createdAt / 1000)}:R>`, inline: true },
    )
    .setFooter({ text: BRAND });
  await channel
    .send({
      content: [`<@${user.id}>`, ...staffRoleIds.map((id) => `<@&${id}>`)].join(' '),
      embeds: [embed],
      components: [ticketButtons()],
      allowedMentions: { users: [user.id], roles: staffRoleIds },
    })
    .catch((error) => log.warn(`Ticket #${num} (${guild.name}): opening message failed: ${error.message}`));

  log.info(`Ticket #${num} opened by ${user.username} in ${guild.name}`);
  return replySuccess(interaction, `Your ticket has been created: <#${channel.id}>`);
}

// ---------- Prise en charge ----------

async function handleClaim(interaction) {
  const ticket = getTicketByChannel(interaction.channelId);
  if (!ticket || ticket.status !== 'open') return replyError(interaction, 'This ticket is closed or no longer exists.');
  if (!isStaff(interaction)) return replyError(interaction, 'Only staff members can claim tickets.');
  const alreadyClaimed = (by) => replyError(interaction, `This ticket has already been claimed by <@${by}>.`);
  if (ticket.claimed_by) return alreadyClaimed(ticket.claimed_by);
  if (!stmt.claim.run(interaction.user.id, ticket.id).changes) {
    return alreadyClaimed(getTicketByChannel(interaction.channelId)?.claimed_by ?? 'someone');
  }
  await interaction.update({ components: [ticketButtons(displayNameOf(interaction))] });
  await interaction.followUp({
    embeds: [baseEmbed(COLORS.green).setDescription(`🙋 <@${interaction.user.id}> will handle this ticket.`)],
  });
}

// ---------- Fermeture ----------

// Demande de confirmation (bouton Close et /ticket close).
export async function requestClose(interaction) {
  const ticket = getTicketByChannel(interaction.channelId);
  if (!ticket || ticket.status !== 'open') return replyError(interaction, 'This ticket is already closed.');
  if (!canClose(interaction, ticket)) {
    return replyError(interaction, 'Only the ticket opener or a staff member can close this ticket.');
  }
  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder()
      .setCustomId('ticket:confirmclose')
      .setLabel('Close ticket')
      .setEmoji('🔒')
      .setStyle(ButtonStyle.Danger),
    new ButtonBuilder().setCustomId('ticket:cancelclose').setLabel('Cancel').setStyle(ButtonStyle.Secondary),
  );
  return replyEphemeral(interaction, {
    embeds: [warnEmbed('Close this ticket? A transcript will be saved and the channel will be deleted.')],
    components: [row],
  });
}

function summaryEmbed(guild, cfg, ticket, messageCount) {
  const num = padNumber(ticket.number);
  const time = (ms) => (ms ? `<t:${Math.floor(ms / 1000)}:f>` : 'Unknown');
  return baseEmbed(cfg.color)
    .setTitle(`🎫 Ticket #${num} closed`)
    .setDescription(ticket.subject ? `**Subject:** ${ticket.subject}` : null)
    .addFields(
      { name: 'Ticket', value: `#${num}`, inline: true },
      { name: 'Category', value: categoryName(ticket.category), inline: true },
      { name: 'Messages', value: messageCount == null ? 'Unknown' : String(messageCount), inline: true },
      { name: 'Opened by', value: `<@${ticket.user_id}>`, inline: true },
      { name: 'Claimed by', value: ticket.claimed_by ? `<@${ticket.claimed_by}>` : 'Unclaimed', inline: true },
      { name: 'Closed by', value: ticket.closed_by ? `<@${ticket.closed_by}>` : 'Unknown', inline: true },
      { name: 'Opened', value: time(ticket.created_at), inline: true },
      { name: 'Closed', value: time(ticket.closed_at), inline: true },
      { name: 'Duration', value: formatDuration(ticket.closed_at - ticket.created_at), inline: true },
    )
    .setFooter({ text: (guild.name === BRAND ? BRAND : `${BRAND} · ${guild.name}`).slice(0, 2048) })
    .setTimestamp(ticket.closed_at);
}

// Transcript → salon de logs + MP à l'auteur, puis suppression du salon (5 s après l'annonce).
async function finishClose(interaction, channel, ticket) {
  const { guild, client } = interaction;
  const cfg = getConfig(guild.id).tickets;
  const num = padNumber(ticket.number);
  const announcedAt = Date.now();
  await channel
    .send({
      embeds: [
        baseEmbed(COLORS.red).setDescription(
          `🔒 Ticket closed by <@${ticket.closed_by}>. This channel will be deleted in 5 seconds.`,
        ),
      ],
    })
    .catch(() => {});

  let transcript = null;
  try {
    transcript = await buildTranscript(channel, ticket);
  } catch (error) {
    log.error(`Transcript of ticket #${num} (${guild.name})`, error);
  }
  const summary = summaryEmbed(guild, cfg, ticket, transcript?.messageCount);
  const files = () =>
    transcript ? [new AttachmentBuilder(transcript.buffer, { name: `transcript-${num}.html` })] : [];

  if (cfg.logChannelId) {
    let logged = await sendTo(guild, cfg.logChannelId, { embeds: [summary], files: files() });
    if (!logged && transcript) {
      // Sans « Joindre des fichiers », on envoie au moins le résumé.
      const note = EmbedBuilder.from(summary).setFooter({
        text: 'Transcript not attached: missing Attach Files permission?',
      });
      logged = await sendTo(guild, cfg.logChannelId, { embeds: [note] });
    }
    if (!logged) log.warn(`Ticket #${num} (${guild.name}): could not post in the ticket log channel.`);
  }

  const opener = await client.users.fetch(ticket.user_id).catch(() => null);
  await opener
    ?.send({
      content: `Your ticket on **${guild.name}** has been closed. Here is a copy of the conversation.`,
      embeds: [summary],
      files: files(),
    })
    .catch(() => {}); // MP fermés

  // Sans transcript, supprimer le salon effacerait la conversation pour de bon : on le garde.
  if (!transcript) {
    await channel
      .send({
        embeds: [
          errorEmbed(
            "I couldn't save the transcript, so this channel was **not** deleted. " +
              'Save what you need, then delete it manually.',
          ),
        ],
      })
      .catch(() => {});
    log.warn(`Ticket #${num} (${guild.name}): transcript failed, channel kept.`);
    return;
  }

  const remaining = CLOSE_DELAY_MS - (Date.now() - announcedAt);
  if (remaining > 0) await sleep(remaining);
  await channel.delete(`Ticket #${num} closed by ${interaction.user.username}`).catch(async (error) => {
    log.warn(`Ticket #${num} (${guild.name}): could not delete the channel: ${error.message}`);
    await interaction
      .followUp({
        embeds: [
          errorEmbed("I couldn't delete this channel (missing **Manage Channels**?). You can delete it manually."),
        ],
        flags: MessageFlags.Ephemeral,
      })
      .catch(() => {});
  });
  log.info(`Ticket #${num} closed by ${interaction.user.username} in ${guild.name}`);
}

async function handleConfirmClose(interaction) {
  const fail = (text) => interaction.update({ embeds: [errorEmbed(text)], components: [] });
  const ticket = getTicketByChannel(interaction.channelId);
  if (!ticket || ticket.status !== 'open') return fail('This ticket is already closed.');
  if (!canClose(interaction, ticket)) return fail('Only the ticket opener or a staff member can close this ticket.');
  const closedAt = Date.now();
  // Garde contre la double fermeture : seule la première requête passe de open à closed.
  if (!stmt.close.run(closedAt, interaction.user.id, ticket.id).changes) {
    return fail('This ticket is already being closed.');
  }
  await interaction.update({ embeds: [infoEmbed('🔒 Closing the ticket…')], components: [] });

  const channel = interaction.channel ?? (await fetchChannel(interaction.guild, interaction.channelId));
  if (!channel) return;
  const closed = { ...ticket, status: 'closed', closed_at: closedAt, closed_by: interaction.user.id };
  await finishClose(interaction, channel, closed);
}

// ---------- Enregistrement ----------

// Tickets restés ouverts alors que leur salon a été supprimé pendant que le bot était hors ligne.
function reconcile(client) {
  const now = Date.now();
  for (const ticket of stmt.allOpen.all()) {
    const guild = client.guilds.cache.get(ticket.guild_id);
    if (!guild?.available) continue;
    const gone = ticket.channel_id
      ? !guild.channels.cache.has(ticket.channel_id)
      : now - ticket.created_at > PENDING_TTL_MS;
    if (gone) stmt.close.run(now, null, ticket.id);
  }
}

export function register(client) {
  onComponent('ticket', async (interaction, [action]) => {
    if (!interaction.inCachedGuild()) return;
    if (action === 'open' && interaction.isStringSelectMenu()) return handleOpen(interaction);
    if (action === 'claim') return handleClaim(interaction);
    if (action === 'close') return requestClose(interaction);
    if (action === 'confirmclose') return handleConfirmClose(interaction);
    if (action === 'cancelclose') {
      return interaction.update({ embeds: [infoEmbed('Ticket closure cancelled.')], components: [] });
    }
  });

  onModal('ticket', async (interaction, [action, categoryId]) => {
    if (!interaction.inCachedGuild()) return;
    if (action === 'create') return handleCreate(interaction, categoryId);
  });

  // Salon de ticket supprimé à la main : le ticket est marqué fermé.
  listen(client, Events.ChannelDelete, (channel) => {
    stmt.closeByChannel.run(Date.now(), channel.id);
  });

  listen(client, Events.ClientReady, () => reconcile(client));
}
