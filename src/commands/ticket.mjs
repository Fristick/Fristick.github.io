// /ticket : gestion du ticket courant (ajout/retrait d'un membre, renommage, fermeture).
import { MessageFlags, OverwriteType, PermissionFlagsBits, PermissionsBitField, SlashCommandBuilder } from 'discord.js';
import { setTimeout as sleep } from 'node:timers/promises';
import {
  getTicketByChannel,
  isStaff,
  requestClose,
  sanitizeChannelName,
  TICKET_MEMBER_PERMS,
} from '../features/tickets.mjs';
import { errorEmbed, replyError, successEmbed, warnEmbed } from '../utils/embeds.mjs';
import { missingPerms } from '../utils/permissions.mjs';

const P = PermissionFlagsBits;
const RENAME_WAIT_MS = 8000; // au-delà, Discord limite les renommages (2 toutes les 10 min)

// { ViewChannel: true, SendMessages: true, … } pour permissionOverwrites.edit.
const MEMBER_OVERWRITE = Object.fromEntries(
  new PermissionsBitField(TICKET_MEMBER_PERMS).toArray().map((name) => [name, true]),
);

export const data = new SlashCommandBuilder()
  .setName('ticket')
  .setDescription('Manage the current ticket')
  .addSubcommand((sub) =>
    sub
      .setName('add')
      .setDescription('Give a member access to this ticket')
      .addUserOption((option) => option.setName('user').setDescription('Member to add').setRequired(true)),
  )
  .addSubcommand((sub) =>
    sub
      .setName('remove')
      .setDescription("Remove a member's access to this ticket")
      .addUserOption((option) => option.setName('user').setDescription('Member to remove').setRequired(true)),
  )
  .addSubcommand((sub) =>
    sub
      .setName('rename')
      .setDescription('Rename this ticket channel')
      .addStringOption((option) =>
        option.setName('name').setDescription('New channel name').setRequired(true).setMaxLength(100),
      ),
  )
  .addSubcommand((sub) => sub.setName('close').setDescription('Close this ticket'));

export async function execute(interaction) {
  const sub = interaction.options.getSubcommand();
  const ticket = getTicketByChannel(interaction.channelId);
  if (!ticket || ticket.status !== 'open') {
    return replyError(interaction, 'This command only works inside an open ticket channel.');
  }
  if (sub === 'close') return requestClose(interaction);
  if (!isStaff(interaction)) return replyError(interaction, 'Only staff members can use this command.');

  const channel = interaction.channel;
  if (!channel) return replyError(interaction, "I can't access this channel.");
  if (sub === 'rename') return rename(interaction, channel);

  const missing = missingPerms(channel, [P.ViewChannel, P.ManageRoles]);
  if (missing.length) {
    return replyError(interaction, `I'm missing these permissions in this channel: **${missing.join(', ')}**.`);
  }
  if (sub === 'add') return add(interaction, channel);
  if (sub === 'remove') return remove(interaction, channel);
}

async function add(interaction, channel) {
  const user = interaction.options.getUser('user', true);
  if (!interaction.options.getMember('user')) {
    return replyError(interaction, `<@${user.id}> isn't a member of this server.`);
  }
  if (user.id === interaction.client.user.id) return replyError(interaction, 'I already have access to this ticket.');
  try {
    await channel.permissionOverwrites.edit(user.id, MEMBER_OVERWRITE, {
      type: OverwriteType.Member,
      reason: `Added to the ticket by ${interaction.user.username}`,
    });
  } catch (error) {
    return replyError(interaction, `I couldn't add <@${user.id}>: ${error.message}`);
  }
  return interaction.reply({
    content: `<@${user.id}>`,
    embeds: [successEmbed(`<@${user.id}> has been added to the ticket by <@${interaction.user.id}>.`)],
    allowedMentions: { users: [user.id] },
  });
}

async function remove(interaction, channel) {
  const user = interaction.options.getUser('user', true);
  if (user.id === interaction.client.user.id) return replyError(interaction, "I can't remove myself from the ticket.");
  if (!channel.permissionOverwrites.cache.has(user.id)) {
    return replyError(
      interaction,
      `<@${user.id}> has no personal access to this ticket (they may still see it through one of their roles).`,
    );
  }
  try {
    await channel.permissionOverwrites.delete(user.id, `Removed from the ticket by ${interaction.user.username}`);
  } catch (error) {
    return replyError(interaction, `I couldn't remove <@${user.id}>: ${error.message}`);
  }
  return interaction.reply({
    embeds: [successEmbed(`<@${user.id}> has been removed from the ticket by <@${interaction.user.id}>.`)],
    allowedMentions: { parse: [] },
  });
}

async function rename(interaction, channel) {
  const name = sanitizeChannelName(interaction.options.getString('name', true));
  if (!name) return replyError(interaction, 'Please use letters, numbers or dashes in the name.');
  if (name === channel.name) return replyError(interaction, `This ticket is already named **${name}**.`);
  const missing = missingPerms(channel, [P.ViewChannel, P.ManageChannels]);
  if (missing.length) {
    return replyError(interaction, `I'm missing these permissions in this channel: **${missing.join(', ')}**.`);
  }

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  // Un renommage limité par Discord attendrait jusqu'à 10 min : on n'attend pas plus de quelques secondes.
  const result = await Promise.race([
    channel.setName(name, `Renamed by ${interaction.user.username}`).then(
      () => 'done',
      (error) => error,
    ),
    sleep(RENAME_WAIT_MS, 'pending', { ref: false }),
  ]);
  if (result === 'done') return interaction.editReply({ embeds: [successEmbed(`Ticket renamed to **${name}**.`)] });
  if (result === 'pending') {
    return interaction.editReply({
      embeds: [
        warnEmbed(
          'Discord only allows 2 channel renames every 10 minutes. ' +
            `The name **${name}** will be applied automatically as soon as possible.`,
        ),
      ],
    });
  }
  return interaction.editReply({ embeds: [errorEmbed(`I couldn't rename the channel: ${result.message}`)] });
}
