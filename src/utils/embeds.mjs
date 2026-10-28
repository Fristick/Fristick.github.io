// Embeds et réponses communs à tous les modules.
import { EmbedBuilder, MessageFlags } from 'discord.js';
import { BRAND, COLORS } from '../config.mjs';

// '#2a5bff', '2a5bff' ou un entier → entier ; valeur invalide → fallback.
export function parseColor(value, fallback = COLORS.blue) {
  if (typeof value === 'number' && Number.isInteger(value)) return value;
  const hex = String(value ?? '').trim().replace(/^#/, '');
  return /^[0-9a-f]{6}$/i.test(hex) ? parseInt(hex, 16) : fallback;
}

export function baseEmbed(color = COLORS.blue) {
  return new EmbedBuilder().setColor(parseColor(color));
}

export function brandFooter(embed, text = BRAND, iconURL) {
  return embed.setFooter(iconURL ? { text, iconURL } : { text }).setTimestamp();
}

export const successEmbed = (text) => baseEmbed(COLORS.green).setDescription(`✅ ${text}`);
export const errorEmbed = (text) => baseEmbed(COLORS.red).setDescription(`❌ ${text}`);
export const infoEmbed = (text) => baseEmbed(COLORS.blue).setDescription(text);
export const warnEmbed = (text) => baseEmbed(COLORS.gold).setDescription(`⚠️ ${text}`);

// Réponse visible par l'auteur seul, que l'interaction ait déjà été différée/répondue ou non.
export async function replyEphemeral(interaction, payload) {
  const data = typeof payload === 'string' ? { content: payload } : payload;
  if (interaction.deferred && !interaction.replied) return interaction.editReply(data);
  if (interaction.replied) return interaction.followUp({ ...data, flags: MessageFlags.Ephemeral });
  return interaction.reply({ ...data, flags: MessageFlags.Ephemeral });
}

export const replyError = (interaction, text) => replyEphemeral(interaction, { content: '', embeds: [errorEmbed(text)] });
export const replySuccess = (interaction, text) =>
  replyEphemeral(interaction, { content: '', embeds: [successEmbed(text)] });
