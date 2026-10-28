// /xp : gestion de l'XP par les admins (ajout, retrait, valeur, niveau, remise à zéro).
import {
  ActionRowBuilder,
  ButtonBuilder,
  ButtonStyle,
  MessageFlags,
  PermissionFlagsBits,
  SlashCommandBuilder,
} from 'discord.js';
import { getConfig } from '../db.mjs';
import { getStats, resetXp, setXp, syncRewardRoles, totalXpForLevel } from '../features/levels.mjs';
import { onComponent } from '../router.mjs';
import { formatNumber } from '../utils/canvas.mjs';
import { infoEmbed, replyError, replySuccess, successEmbed, warnEmbed } from '../utils/embeds.mjs';
import { log } from '../utils/log.mjs';

const MAX_XP = 1_000_000_000;
const MAX_LEVEL = 500;

const userOption = (o) => o.setName('user').setDescription('Member').setRequired(true);
const amountOption = (description, min) => (o) =>
  o.setName('amount').setDescription(description).setRequired(true).setMinValue(min).setMaxValue(MAX_XP);

export const data = new SlashCommandBuilder()
  .setName('xp')
  .setDescription('Manage member XP')
  .setDefaultMemberPermissions(PermissionFlagsBits.ManageGuild)
  .addSubcommand((s) =>
    s
      .setName('add')
      .setDescription('Give XP to a member')
      .addUserOption(userOption)
      .addIntegerOption(amountOption('XP to add', 1)),
  )
  .addSubcommand((s) =>
    s
      .setName('remove')
      .setDescription('Remove XP from a member')
      .addUserOption(userOption)
      .addIntegerOption(amountOption('XP to remove', 1)),
  )
  .addSubcommand((s) =>
    s
      .setName('set')
      .setDescription("Set a member's total XP")
      .addUserOption(userOption)
      .addIntegerOption(amountOption('New total XP', 0)),
  )
  .addSubcommand((s) =>
    s
      .setName('setlevel')
      .setDescription("Set a member's level (XP set to the start of that level)")
      .addUserOption(userOption)
      .addIntegerOption((o) =>
        o.setName('level').setDescription('New level').setRequired(true).setMinValue(0).setMaxValue(MAX_LEVEL),
      ),
  )
  .addSubcommand((s) =>
    s
      .setName('reset')
      .setDescription('Reset the XP of a member, or of the whole server')
      .addUserOption((o) => o.setName('user').setDescription('Member to reset (leave empty for the whole server)')),
  );

const levelChange = (before, after) =>
  after === before ? `level **${after}**` : `level **${after}** (was ${before})`;

// Après une remise à zéro du serveur : retire les rôles récompenses aux membres en cache qui en ont un.
async function stripRewardRoles(guild) {
  const ids = new Set((getConfig(guild.id).levels.rewards ?? []).map((r) => r?.roleId).filter(Boolean));
  if (!ids.size) return;
  for (const member of guild.members.cache.values()) {
    if (member.roles.cache.some((role) => ids.has(role.id))) await syncRewardRoles(member, 0, { prune: true });
  }
}

export async function execute(interaction) {
  const sub = interaction.options.getSubcommand();
  const user = interaction.options.getUser('user');

  if (sub === 'reset' && !user) {
    const row = new ActionRowBuilder().addComponents(
      new ButtonBuilder().setCustomId('xp:resetall:confirm').setLabel('Reset everything').setStyle(ButtonStyle.Danger),
      new ButtonBuilder().setCustomId('xp:resetall:cancel').setLabel('Cancel').setStyle(ButtonStyle.Secondary),
    );
    return interaction.reply({
      embeds: [
        warnEmbed(
          '**Reset the XP of the whole server?**\nEvery level, message count and voice time will be erased. ' +
            'This cannot be undone.',
        ),
      ],
      components: [row],
      flags: MessageFlags.Ephemeral,
    });
  }

  if (user.bot) return replyError(interaction, 'Bots do not earn XP.');
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const member = await interaction.guild.members.fetch(user.id).catch(() => null);

  if (sub === 'reset') {
    const removed = resetXp(interaction.guildId, user.id);
    if (member) await syncRewardRoles(member, 0, { prune: true });
    return replySuccess(interaction, removed ? `The XP of ${user} has been reset.` : `${user} had no XP to reset.`);
  }

  if (!member) return replyError(interaction, `${user} is not a member of this server.`);
  const before = getStats(interaction.guildId, user.id);
  let target;
  if (sub === 'add') target = before.xp + interaction.options.getInteger('amount');
  else if (sub === 'remove') target = before.xp - interaction.options.getInteger('amount');
  else if (sub === 'set') target = interaction.options.getInteger('amount');
  else target = totalXpForLevel(interaction.options.getInteger('level'));

  const { xp, level } = await setXp(member, Math.min(MAX_XP, Math.max(0, target)));
  const summary = `${member} now has **${formatNumber(xp)} XP**, ${levelChange(before.level, level)}.`;
  const lead = {
    add: `Added **${formatNumber(xp - before.xp)} XP**.`,
    remove: `Removed **${formatNumber(before.xp - xp)} XP**.`,
    set: 'XP updated.',
    setlevel: `Level set to **${level}**.`,
  }[sub];
  return replySuccess(interaction, `${lead}\n${summary}`);
}

onComponent('xp', async (interaction, [action, choice]) => {
  if (action !== 'resetall' || !interaction.isButton()) return;
  if (!interaction.memberPermissions?.has(PermissionFlagsBits.ManageGuild)) {
    return replyError(interaction, 'You need the **Manage Server** permission to do this.');
  }
  if (choice !== 'confirm') {
    return interaction.update({ embeds: [infoEmbed('Reset cancelled. No XP was removed.')], components: [] });
  }

  const count = resetXp(interaction.guildId);
  const hasRewards = (getConfig(interaction.guildId).levels.rewards ?? []).length > 0;
  await interaction.update({
    embeds: [
      successEmbed(
        `The XP of the whole server has been reset (${formatNumber(count)} member${count === 1 ? '' : 's'}).` +
          (hasRewards ? '\nReward roles are being removed in the background.' : ''),
      ),
    ],
    components: [],
  });
  stripRewardRoles(interaction.guild).catch((error) => log.warn(`Reward roles cleanup failed: ${error.message}`));
});
