// Contrôles de permissions du bot, pour prévenir au lieu d'échouer en silence.
import { PermissionFlagsBits, PermissionsBitField } from 'discord.js';

// Nom lisible d'une permission : ManageRoles → « Manage Roles ».
export const permName = (flag) =>
  (new PermissionsBitField(flag).toArray()[0] ?? String(flag)).replace(/([a-z])([A-Z])/g, '$1 $2');

// Permissions manquantes au bot dans un salon (ou sur le serveur si channel est une Guild).
export function missingPerms(target, flags) {
  const me = target.guild ? target.guild.members.me : target.members.me;
  if (!me) return flags.map(permName);
  const perms = target.guild ? target.permissionsFor(me) : me.permissions;
  return flags.filter((flag) => !perms?.has(flag)).map(permName);
}

// Le bot peut-il donner/retirer ce rôle ? (Manage Roles, rôle sous le sien, pas un rôle d'intégration)
export function canManageRole(guild, role) {
  const me = guild.members.me;
  return Boolean(
    role &&
      me?.permissions.has(PermissionFlagsBits.ManageRoles) &&
      !role.managed &&
      role.id !== guild.id &&
      role.position < me.roles.highest.position,
  );
}

// Envoi sûr dans un salon configuré : null si le salon n'existe plus ou si le bot ne peut pas y écrire.
export async function sendTo(guild, channelId, payload) {
  if (!channelId) return null;
  const channel = guild.channels.cache.get(channelId) ?? (await guild.channels.fetch(channelId).catch(() => null));
  if (!channel?.isTextBased()) return null;
  if (missingPerms(channel, [PermissionFlagsBits.ViewChannel, PermissionFlagsBits.SendMessages]).length) return null;
  return channel.send(payload).catch(() => null);
}
