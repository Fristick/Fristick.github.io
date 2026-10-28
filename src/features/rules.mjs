// Règlement du serveur : un embed posté par le bot (modifié sur place s'il est déjà posté), avec des boutons vers
// les Conditions d'utilisation et les Règles de la communauté de Discord.
import { ActionRowBuilder, ButtonBuilder, ButtonStyle, PermissionFlagsBits } from 'discord.js';
import { BRAND, COLORS, DISCORD_GUIDELINES_URL, DISCORD_TERMS_URL, SITE_URL } from '../config.mjs';
import { getConfig, updateConfig } from '../db.mjs';
import { baseEmbed, brandFooter, parseColor } from '../utils/embeds.mjs';
import { log } from '../utils/log.mjs';
import { fill } from '../utils/placeholders.mjs';
import { missingPerms } from '../utils/permissions.mjs';

const { EmbedLinks, SendMessages, ViewChannel } = PermissionFlagsBits;

const link = (label, emoji, url) =>
  new ButtonBuilder().setStyle(ButtonStyle.Link).setLabel(label).setEmoji(emoji).setURL(url);

// Message du règlement pour une config complète (pure : testable sans Discord).
export function buildRulesPayload(guild, cfg) {
  const ticketsId = cfg.tickets?.panelChannelId;
  const tickets = ticketsId && guild.channels?.cache?.has(ticketsId) ? `<#${ticketsId}>` : 'our support channel';
  const embed = baseEmbed(parseColor(cfg.rules.color, COLORS.blue))
    .setTitle(fill(cfg.rules.title, { guild, tickets }).slice(0, 256) || 'Server Rules')
    .setDescription(fill(cfg.rules.message, { guild, tickets }).slice(0, 4096) || 'Be respectful.');
  const icon = guild.iconURL?.({ size: 128 });
  if (icon) embed.setThumbnail(icon);
  brandFooter(embed, `${BRAND} · Last updated`, icon ?? undefined);
  const buttons = new ActionRowBuilder().addComponents(
    link('Discord Terms', '📄', DISCORD_TERMS_URL),
    link('Community Guidelines', '🛡️', DISCORD_GUIDELINES_URL),
    link('Website', '🌐', SITE_URL),
  );
  return { embeds: [embed], components: [buttons] };
}

// Publie le règlement, ou modifie celui déjà posté s'il existe encore. Renvoie le message ou null.
export async function postRulesPanel(guild) {
  try {
    const cfg = getConfig(guild.id);
    const id = cfg.rules.channelId;
    const channel = id ? (guild.channels.cache.get(id) ?? (await guild.channels.fetch(id).catch(() => null))) : null;
    if (!channel?.isTextBased()) return null;
    const missing = missingPerms(channel, [ViewChannel, SendMessages, EmbedLinks]);
    if (missing.length) {
      log.warn(`Rules: missing ${missing.join(', ')} in #${channel.name} (${guild.name})`);
      return null;
    }
    const payload = buildRulesPayload(guild, cfg);
    if (cfg.rules.panelMessageId) {
      const old = await channel.messages.fetch(cfg.rules.panelMessageId).catch(() => null);
      const edited = old?.editable ? await old.edit(payload).catch(() => null) : null;
      if (edited) return edited;
    }
    const message = await channel.send(payload).catch(() => null);
    if (message) {
      updateConfig(guild.id, (c) => {
        c.rules.panelMessageId = message.id;
      });
    }
    return message;
  } catch (error) {
    log.error('postRulesPanel', error);
    return null;
  }
}
