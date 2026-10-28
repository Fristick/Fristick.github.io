// /help : commandes et fonctionnalités du bot, avec un lien vers le site.
import { ActionRowBuilder, ButtonBuilder, ButtonStyle, MessageFlags, SlashCommandBuilder } from 'discord.js';
import { BRAND, COLORS, SITE_URL } from '../config.mjs';
import { baseEmbed, brandFooter } from '../utils/embeds.mjs';

export const data = new SlashCommandBuilder().setName('help').setDescription('List the bot commands and features');

export function buildHelpEmbed() {
  return brandFooter(
    baseEmbed(COLORS.blue)
      .setTitle(`🦊 ${BRAND} — Help`)
      .setDescription(`Everything the ${BRAND} bot can do on this server.`)
      .addFields(
        {
          name: '👤 Member commands',
          value: [
            '`/rank` — Your level card: level, XP, rank and progress (or another member’s)',
            '`/leaderboard` — The most active members of the server',
          ].join('\n'),
        },
        {
          name: '🛡️ Staff commands',
          value: [
            '`/setup` — Configure every module from one panel (Administrator)',
            '`/xp add` · `remove` · `set` · `setlevel` · `reset` — Manage member XP',
            '`/ticket add` · `remove` — Add or remove a member from a ticket',
            '`/ticket rename` · `close` — Rename or close a ticket',
          ].join('\n'),
        },
        {
          name: '✨ Features',
          value: [
            '📜 **Rules** — Read the server rules: they follow Discord’s Terms and Community Guidelines',
            '✅ **Verification** — Click the button in the verification channel to unlock the server',
            '📈 **Levels** — Earn XP by chatting and by spending time in voice channels; ' +
              'level-ups are announced and promote you through the ranks (Active Member → 👑 Legend)',
            '🎫 **Tickets** — Pick a category in the ticket panel to open a private channel with the staff',
            '🔊 **Create your voice** — Join the creator channel to get your own temporary voice channel',
            '💎 **Boosts** — Every server boost gets a thank-you message',
          ].join('\n'),
        },
      ),
  );
}

export async function execute(interaction) {
  const link = new ButtonBuilder().setStyle(ButtonStyle.Link).setURL(SITE_URL).setLabel('Website').setEmoji('🌐');
  await interaction.reply({
    embeds: [buildHelpEmbed()],
    components: [new ActionRowBuilder().addComponents(link)],
    flags: MessageFlags.Ephemeral,
  });
}
