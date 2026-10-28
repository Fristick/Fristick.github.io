// Accueil des nouveaux membres : rôle automatique, message de bienvenue avec carte, MP optionnel.
import { ActionRowBuilder, AttachmentBuilder, ButtonBuilder, ButtonStyle, Events } from 'discord.js';
import { BRAND, SITE_URL } from '../config.mjs';
import { getConfig } from '../db.mjs';
import { cleanName } from '../utils/canvas.mjs';
import { baseEmbed, brandFooter, parseColor } from '../utils/embeds.mjs';
import { listen, log } from '../utils/log.mjs';
import { canManageRole, sendTo } from '../utils/permissions.mjs';
import { fill } from '../utils/placeholders.mjs';
import { renderWelcomeCard } from './welcomeCard.mjs';

const websiteRow = () =>
  new ActionRowBuilder().addComponents(
    new ButtonBuilder().setStyle(ButtonStyle.Link).setLabel('Website').setEmoji('🌐').setURL(SITE_URL),
  );

/**
 * Message de bienvenue dans le salon configuré (aussi utilisé par le bouton « Send test » de /setup).
 * @returns {Promise<import('discord.js').Message|null>} null si le salon est absent ou inaccessible.
 */
export async function sendWelcome(member, { test = false } = {}) {
  const { guild } = member;
  const { welcome } = getConfig(guild.id);
  if (!welcome.channelId) return null;

  const embed = baseEmbed(parseColor(welcome.color));
  const title = fill(welcome.title, { member }).slice(0, 256);
  const description = fill(welcome.message, { member }).slice(0, 4096);
  if (title) embed.setTitle(title);
  if (description) embed.setDescription(description);
  brandFooter(embed, test ? `${BRAND} (test)` : BRAND, guild.iconURL({ size: 64 }) ?? undefined);

  const files = [];
  if (welcome.image) {
    try {
      const png = await renderWelcomeCard({
        username: cleanName(member.displayName, member.user.username),
        avatarURL: member.displayAvatarURL({ extension: 'png', size: 256 }),
        serverName: guild.name,
        memberCount: guild.memberCount,
      });
      files.push(new AttachmentBuilder(png, { name: 'welcome.png' }));
      embed.setImage('attachment://welcome.png');
    } catch (error) {
      log.warn(`Welcome card failed in ${guild.name}: ${error.message}`);
    }
  }

  return sendTo(guild, welcome.channelId, {
    content: `${member}`,
    embeds: [embed],
    files,
    components: [websiteRow()],
    allowedMentions: { users: [member.id] },
  });
}

async function giveAutoRole(member, roleId) {
  const role = member.guild.roles.cache.get(roleId);
  if (!canManageRole(member.guild, role)) {
    log.warn(`Auto role ${roleId} cannot be given in ${member.guild.name} (missing, managed or above the bot).`);
    return;
  }
  await member.roles.add(role, 'Welcome auto role').catch((error) => {
    log.warn(`Could not give the auto role to ${member.user.tag}: ${error.message}`);
  });
}

async function sendWelcomeDm(member, welcome) {
  const embed = baseEmbed(parseColor(welcome.color))
    .setAuthor({ name: member.guild.name, iconURL: member.guild.iconURL({ size: 64 }) ?? undefined })
    .setDescription(fill(welcome.dmMessage, { member }).slice(0, 4096) || `Welcome to **${member.guild.name}**!`);
  brandFooter(embed);
  // MP fermés : on ignore.
  await member.send({ embeds: [embed], components: [websiteRow()] }).catch(() => {});
}

export function register(client) {
  listen(client, Events.GuildMemberAdd, async (member) => {
    if (member.user.bot) return;
    const { welcome } = getConfig(member.guild.id);
    // Module désactivé dans /setup = ni message, ni rôle automatique, ni MP.
    if (!welcome.enabled) return;
    if (welcome.autoRoleId) await giveAutoRole(member, welcome.autoRoleId);
    if (welcome.channelId) await sendWelcome(member);
    if (welcome.dm) await sendWelcomeDm(member, welcome);
  });
}
