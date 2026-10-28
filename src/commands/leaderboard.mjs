// /leaderboard : classement XP du serveur, 10 par page, boutons ◀ ▶ réservés à l'auteur de la commande.
import { ActionRowBuilder, ButtonBuilder, ButtonStyle, SlashCommandBuilder } from 'discord.js';
import { COLORS } from '../config.mjs';
import { db } from '../db.mjs';
import { getStats, levelFromXp } from '../features/levels.mjs';
import { onComponent } from '../router.mjs';
import { formatNumber } from '../utils/canvas.mjs';
import { baseEmbed, replyError } from '../utils/embeds.mjs';

const PAGE_SIZE = 10;
const MEDALS = ['🥇', '🥈', '🥉'];

const countRanked = db.prepare('SELECT COUNT(*) AS n FROM levels WHERE guild_id = ? AND xp > 0');
// Ex æquo : même rang (comme getStats).
const selectPage = db.prepare(
  `SELECT user_id, xp, RANK() OVER (ORDER BY xp DESC) AS position
   FROM levels WHERE guild_id = ? AND xp > 0
   ORDER BY xp DESC, user_id LIMIT ? OFFSET ?`,
);

export const data = new SlashCommandBuilder().setName('leaderboard').setDescription('Show the server XP leaderboard');

function buildPage(guild, invokerId, requestedPage) {
  const total = Number(countRanked.get(guild.id).n);
  if (!total) {
    return {
      embeds: [
        baseEmbed(COLORS.gold)
          .setTitle('🏆 Leaderboard')
          .setDescription('Nobody has earned XP yet. Start chatting or hop in a voice channel to claim the top spot!'),
      ],
      components: [],
    };
  }

  const pages = Math.ceil(total / PAGE_SIZE);
  const page = Math.min(Math.max(0, requestedPage), pages - 1);
  const rows = selectPage.all(guild.id, PAGE_SIZE, page * PAGE_SIZE);
  const lines = rows.map((row) => {
    const position = Number(row.position);
    const prefix = MEDALS[position - 1] ?? `**#${position}**`;
    const level = levelFromXp(row.xp).level;
    return `${prefix} <@${row.user_id}> — Level ${formatNumber(level)} · ${formatNumber(row.xp)} XP`;
  });

  const me = getStats(guild.id, invokerId);
  const you = me.xp > 0 ? `You are #${formatNumber(me.rank)} · Level ${formatNumber(me.level)}` : 'You have no XP yet';
  const embed = baseEmbed(COLORS.gold)
    .setTitle(`🏆 Leaderboard — ${guild.name}`.slice(0, 256))
    .setDescription(lines.join('\n'))
    .setFooter({ text: `${you} · Page ${page + 1}/${pages}` });
  const icon = guild.iconURL({ size: 128 });
  if (icon) embed.setThumbnail(icon);

  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder()
      .setCustomId(`lb:page:${page - 1}:${invokerId}`)
      .setEmoji('◀️')
      .setStyle(ButtonStyle.Secondary)
      .setDisabled(page === 0),
    new ButtonBuilder()
      .setCustomId(`lb:page:${page + 1}:${invokerId}`)
      .setEmoji('▶️')
      .setStyle(ButtonStyle.Secondary)
      .setDisabled(page >= pages - 1),
  );
  return { embeds: [embed], components: pages > 1 ? [row] : [] };
}

export async function execute(interaction) {
  // Les mentions dans un embed ne notifient personne.
  await interaction.reply(buildPage(interaction.guild, interaction.user.id, 0));
}

onComponent('lb', async (interaction, [action, page, invokerId]) => {
  if (action !== 'page' || !interaction.isButton()) return;
  if (interaction.user.id !== invokerId) {
    return replyError(interaction, 'Only the member who used this command can change pages. Try `/leaderboard`!');
  }
  await interaction.update(buildPage(interaction.guild, invokerId, Number(page) || 0));
});
