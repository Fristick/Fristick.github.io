// /rank [user] : carte de rang (image) d'un membre.
import { AttachmentBuilder, SlashCommandBuilder } from 'discord.js';
import { COLORS } from '../config.mjs';
import { getStats, rankRoleFor } from '../features/levels.mjs';
import { renderRankCard } from '../features/rankCard.mjs';
import { cleanName, formatDuration, formatNumber } from '../utils/canvas.mjs';
import { baseEmbed, replyError } from '../utils/embeds.mjs';
import { log } from '../utils/log.mjs';

export const data = new SlashCommandBuilder()
  .setName('rank')
  .setDescription('Show your rank card, or the rank card of another member')
  .addUserOption((o) => o.setName('user').setDescription('Member to look up'));

export async function execute(interaction) {
  const user = interaction.options.getUser('user') ?? interaction.user;
  if (user.bot) return replyError(interaction, 'Bots do not earn XP, so they have no rank.');

  await interaction.deferReply();
  const member = await interaction.guild.members.fetch(user.id).catch(() => null);
  const stats = getStats(interaction.guildId, user.id);
  const displayName = member?.displayName ?? user.displayName;
  const rankRole = rankRoleFor(interaction.guild, stats.level);

  try {
    const png = await renderRankCard({
      username: cleanName(displayName, user.username),
      avatarURL: (member ?? user).displayAvatarURL({ extension: 'png', size: 256 }),
      level: stats.level,
      rank: stats.rank,
      xpIntoLevel: stats.xpIntoLevel,
      xpForNext: stats.xpForNext,
      totalXp: stats.xp,
      messages: stats.messages,
      voiceSeconds: stats.voiceSeconds,
      rankName: rankRole?.name ?? null,
      rankColor: rankRole?.hexColor ?? null,
    });
    await interaction.editReply({ files: [new AttachmentBuilder(png, { name: 'rank.png' })] });
  } catch (error) {
    // Carte impossible à générer : mêmes infos en texte.
    log.error('Rank card', error);
    const embed = baseEmbed(COLORS.gold)
      .setAuthor({ name: displayName, iconURL: (member ?? user).displayAvatarURL({ size: 64 }) })
      .addFields(
        { name: 'Rank', value: `#${formatNumber(stats.rank)}${rankRole ? ` · ${rankRole}` : ''}`, inline: true },
        { name: 'Level', value: formatNumber(stats.level), inline: true },
        {
          name: 'XP',
          value: `${formatNumber(stats.xpIntoLevel)} / ${formatNumber(stats.xpForNext)}` +
            ` (${formatNumber(stats.xp)} total)`,
          inline: true,
        },
        { name: 'Messages', value: formatNumber(stats.messages), inline: true },
        { name: 'Voice time', value: formatDuration(stats.voiceSeconds), inline: true },
      );
    await interaction.editReply({ embeds: [embed] });
  }
}
