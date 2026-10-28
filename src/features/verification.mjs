// Vérification : panneau avec bouton qui donne le rôle « vérifié », et verrouillage du serveur pour les non-vérifiés.
import {
  ActionRowBuilder,
  ButtonBuilder,
  ButtonStyle,
  MessageFlags,
  OverwriteType,
  PermissionFlagsBits,
  PermissionsBitField,
} from 'discord.js';
import { BRAND, COLORS } from '../config.mjs';
import { getConfig, updateConfig } from '../db.mjs';
import { onComponent } from '../router.mjs';
import {
  baseEmbed,
  brandFooter,
  infoEmbed,
  parseColor,
  replyEphemeral,
  replyError,
  replySuccess,
} from '../utils/embeds.mjs';
import { log } from '../utils/log.mjs';
import { fill } from '../utils/placeholders.mjs';
import { canManageRole, missingPerms, sendTo } from '../utils/permissions.mjs';

const { Administrator, EmbedLinks, ManageRoles, ReadMessageHistory, SendMessages, ViewChannel } = PermissionFlagsBits;
// Autres façons d'écrire dans le salon de vérification, refusées à @everyone en plus de SendMessages.
const VERIFY_EXTRA_DENY = {
  AddReactions: PermissionFlagsBits.AddReactions,
  CreatePublicThreads: PermissionFlagsBits.CreatePublicThreads,
  CreatePrivateThreads: PermissionFlagsBits.CreatePrivateThreads,
  SendMessagesInThreads: PermissionFlagsBits.SendMessagesInThreads,
};
const REASON = 'Verification setup';

async function fetchChannel(guild, id) {
  if (!id) return null;
  return guild.channels.cache.get(id) ?? (await guild.channels.fetch(id).catch(() => null));
}

async function fetchRole(guild, id) {
  if (!id) return null;
  return guild.roles.cache.get(id) ?? (await guild.roles.fetch(id).catch(() => null));
}

const bold = (names) => names.map((n) => `**${n}**`).join(', ');

function panelPayload(guild, cfg, rules) {
  // Règlement activé : vérifier = l'accepter.
  const rulesLine =
    rules?.enabled && rules.channelId && guild.channels.cache.has(rules.channelId)
      ? `

📜 By verifying, you confirm that you have read and accept the rules in <#${rules.channelId}>.`
      : '';
  const description = fill(cfg.message, { guild }) || 'Click the button below to verify yourself.';
  const embed = baseEmbed(parseColor(cfg.color, COLORS.green))
    .setTitle(fill(cfg.title, { guild }).slice(0, 256) || 'Verification')
    .setDescription(`${description.slice(0, 4096 - rulesLine.length)}${rulesLine}`);
  brandFooter(embed, BRAND, guild.iconURL() ?? undefined);
  const button = new ButtonBuilder()
    .setCustomId('verify')
    .setStyle(ButtonStyle.Success)
    .setEmoji('✅')
    .setLabel(String(cfg.buttonLabel || 'Verify').slice(0, 80));
  return { embeds: [embed], components: [new ActionRowBuilder().addComponents(button)] };
}

// Publie le panneau, ou modifie celui déjà posté s'il existe encore. Renvoie le message ou null.
export async function postVerificationPanel(guild) {
  try {
    const { verification: cfg, rules } = getConfig(guild.id);
    const channel = await fetchChannel(guild, cfg.channelId);
    if (!channel?.isTextBased()) return null;
    const missing = missingPerms(channel, [ViewChannel, SendMessages, EmbedLinks]);
    if (missing.length) {
      log.warn(`Verification panel: missing ${missing.join(', ')} in #${channel.name} (${guild.name})`);
      return null;
    }
    const payload = panelPayload(guild, cfg, rules);
    if (cfg.panelMessageId) {
      const old = await channel.messages.fetch(cfg.panelMessageId).catch(() => null);
      const edited = old?.editable ? await old.edit(payload).catch(() => null) : null;
      if (edited) return edited;
    }
    const message = await channel.send(payload).catch(() => null);
    if (message) {
      updateConfig(guild.id, (c) => {
        c.verification.panelMessageId = message.id;
      });
    }
    return message;
  } catch (error) {
    log.error('Verification panel', error);
    return null;
  }
}

async function handleVerify(interaction) {
  if (!interaction.inCachedGuild()) return;
  const { guild, member } = interaction;
  const cfg = getConfig(guild.id);
  const role = cfg.verification.enabled ? await fetchRole(guild, cfg.verification.roleId) : null;
  if (!role) return replyError(interaction, "Verification isn't configured yet.");
  if (!canManageRole(guild, role)) {
    return replyError(
      interaction,
      `I can't give you the ${role} role. Please ask a staff member to move my role above it ` +
        '(and to give me the **Manage Roles** permission).',
    );
  }
  if (member.roles.cache.has(role.id)) {
    return replyEphemeral(interaction, { embeds: [infoEmbed("You're already verified.")] });
  }

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  try {
    await member.roles.add(role, 'Verification button');
  } catch (error) {
    log.warn(`Verification: could not give ${role.name} to ${member.user.tag}: ${error.message}`);
    return replyError(interaction, "I couldn't give you the role. Please contact a staff member.");
  }
  await replySuccess(interaction, `You're verified! Welcome to **${guild.name}** 🦊`);
  await sendTo(guild, cfg.logs.memberChannelId, {
    embeds: [baseEmbed(COLORS.green).setDescription(`✅ <@${member.id}> verified`).setTimestamp()],
  });
}

// Rend le serveur visible aux seuls membres vérifiés. Ne lève jamais d'erreur : tout est résumé dans changes / errors.
// Ordre choisi pour ne jamais enfermer tout le monde : rôle vérifié → salon de vérification → @everyone.
export async function restrictEveryone(guild) {
  const changes = [];
  const errors = [];
  try {
    const { verification: cfg, rules } = getConfig(guild.id);
    const me = guild.members.me ?? (await guild.members.fetchMe().catch(() => null));
    if (!me) {
      errors.push("I couldn't read my own permissions in this server.");
      return { changes, errors };
    }
    if (!me.permissions.has(ManageRoles)) {
      errors.push('I need the **Manage Roles** permission.');
      return { changes, errors };
    }
    const everyone = guild.roles.everyone;
    const verified = await fetchRole(guild, cfg.roleId);
    if (!verified) {
      errors.push('Set the Verified role first.');
      return { changes, errors };
    }
    if (!canManageRole(guild, verified)) {
      errors.push(`My role must be above ${verified} (and it can't be an integration role) so I can edit it.`);
      return { changes, errors };
    }
    const channel = await fetchChannel(guild, cfg.channelId);
    if (!channel?.permissionOverwrites) {
      errors.push('Set the verification channel first: unverified members need somewhere to verify.');
      return { changes, errors };
    }
    const missing = missingPerms(channel, [ViewChannel, ManageRoles]).map((n) =>
      n.replace('Manage Roles', 'Manage Permissions'),
    );
    if (missing.length) {
      errors.push(`I'm missing ${bold(missing)} in ${channel}.`);
      return { changes, errors };
    }
    if (everyone.permissions.has(Administrator)) {
      errors.push("@everyone has **Administrator**: remove it first, otherwise the server can't be hidden.");
      return { changes, errors };
    }
    // Sans View Channels sur son propre rôle, le bot perdrait lui aussi l'accès au serveur.
    const ownPerms = new PermissionsBitField(me.roles.cache.filter((r) => r.id !== guild.id).map((r) => r.permissions));
    if (!ownPerms.has(ViewChannel)) {
      errors.push(
        "My role doesn't have **View Channels** by itself: give it that permission first, " +
          "otherwise I'd lose access to the server too.",
      );
      return { changes, errors };
    }

    // 1. Le rôle vérifié reçoit ce que @everyone peut faire (seulement ce que le bot a lui-même le droit de donner).
    const botPerms = me.permissions.has(Administrator) ? PermissionsBitField.All : me.permissions.bitfield;
    const wanted = (everyone.permissions.bitfield | ViewChannel) & botPerms & ~Administrator;
    const next = verified.permissions.bitfield | wanted;
    if (next !== verified.permissions.bitfield) {
      try {
        await verified.setPermissions(next, REASON);
        changes.push(`${verified} now has every permission @everyone had.`);
      } catch (error) {
        errors.push(`I couldn't update the permissions of ${verified}: ${error.message}`);
        return { changes, errors };
      }
    }

    // 2. Salon de vérification : lisible par @everyone (sans écrire), caché aux vérifiés ; le bot garde la main.
    try {
      await channel.permissionOverwrites.edit(
        me.id,
        { ViewChannel: true, SendMessages: true, EmbedLinks: true, ReadMessageHistory: true },
        { reason: REASON, type: OverwriteType.Member },
      );
    } catch (error) {
      errors.push(`I couldn't give myself access to ${channel}: ${error.message}`);
    }
    // Discord n'autorise un bot à refuser que les permissions qu'il possède : on garde celles-là,
    // on signale les autres.
    const myPerms = channel.permissionsFor(me);
    const extraDeny = {};
    const cantDeny = [];
    for (const [name, flag] of Object.entries(VERIFY_EXTRA_DENY)) {
      if (myPerms?.has(flag)) extraDeny[name] = false;
      else cantDeny.push(name.replace(/([a-z])([A-Z])/g, '$1 $2'));
    }
    if (cantDeny.length) {
      errors.push(
        `I don't have ${cantDeny.join(', ')}, so I can't block them in ${channel}: unverified members may still ` +
          'react or use threads there. Re-invite me with the link from the README to fix it.',
      );
    }
    const denyBits = Object.keys(extraDeny).reduce((bits, name) => bits | VERIFY_EXTRA_DENY[name], SendMessages);
    const ow = channel.permissionOverwrites.cache.get(everyone.id);
    if (!ow?.allow.has(ViewChannel | ReadMessageHistory) || !ow.deny.has(denyBits)) {
      try {
        await channel.permissionOverwrites.edit(
          everyone,
          { ViewChannel: true, ReadMessageHistory: true, SendMessages: false, ...extraDeny },
          { reason: REASON },
        );
        changes.push(`@everyone can see ${channel} but can't write there.`);
      } catch (error) {
        errors.push(
          `I couldn't edit the @everyone permissions in ${channel}, so the server was not locked: ${error.message}`,
        );
        return { changes, errors };
      }
    }
    if (!channel.permissionOverwrites.cache.get(verified.id)?.deny.has(ViewChannel)) {
      try {
        await channel.permissionOverwrites.edit(verified, { ViewChannel: false }, { reason: REASON });
        changes.push(`${channel} is now hidden from ${verified} (verified members don't need it).`);
      } catch (error) {
        errors.push(`I couldn't hide ${channel} from ${verified}: ${error.message}`);
      }
    }

    // 2 bis. Salon du règlement : reste lisible par les non-vérifiés (on lit les règles avant de vérifier).
    const rulesChannel = rules.enabled ? await fetchChannel(guild, rules.channelId) : null;
    if (rulesChannel && rulesChannel.id !== channel.id) {
      const rulesOw = rulesChannel.permissionOverwrites.cache.get(everyone.id);
      if (!rulesOw?.allow.has(ViewChannel | ReadMessageHistory) || !rulesOw.deny.has(SendMessages)) {
        try {
          await rulesChannel.permissionOverwrites.edit(
            everyone,
            { ViewChannel: true, ReadMessageHistory: true, SendMessages: false, ...extraDeny },
            { reason: REASON },
          );
          changes.push(`@everyone can read the rules in ${rulesChannel} before verifying.`);
        } catch (error) {
          errors.push(`I couldn't keep ${rulesChannel} visible to unverified members: ${error.message}`);
        }
      }
    }

    // 3. @everyone ne voit plus le serveur.
    if (everyone.permissions.has(ViewChannel)) {
      try {
        await everyone.setPermissions(everyone.permissions.bitfield & ~ViewChannel, REASON);
        changes.push('@everyone can no longer see the server: only verified members can.');
      } catch (error) {
        errors.push(`I couldn't remove **View Channels** from @everyone: ${error.message}`);
      }
    }

    // Salons qui laissent encore entrer @everyone par leurs propres permissions.
    const leaks = guild.channels.cache.filter(
      (c) =>
        c.id !== channel.id &&
        c.id !== rulesChannel?.id &&
        c.permissionOverwrites?.cache.get(everyone.id)?.allow.has(ViewChannel),
    );
    if (leaks.size) {
      const more = leaks.size > 10 ? ` and ${leaks.size - 10} more` : '';
      errors.push(
        `These channels still let @everyone in through their own permissions: ${leaks.first(10).join(', ')}${more}.`,
      );
    }
    if (!changes.length && !errors.length) changes.push('Everything was already set up.');
  } catch (error) {
    log.error('restrictEveryone', error);
    errors.push(`Unexpected error: ${error.message}`);
  }
  return { changes, errors };
}

export function register() {
  onComponent('verify', handleVerify);
}
