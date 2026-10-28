// Variables utilisables dans les textes configurables :
// {user} mention · {username} pseudo affiché · {tag} nom d'utilisateur · {server} nom du serveur
// {memberCount} nombre de membres · plus toute variable passée dans vars (ex. {level}, {boostCount}, {boostTier}).
export const PLACEHOLDER_HELP = '{user} {username} {tag} {server} {memberCount}';

export function fill(template, { member, user, guild, ...vars } = {}) {
  const u = user ?? member?.user;
  const g = guild ?? member?.guild;
  const values = {
    user: u ? `<@${u.id}>` : '',
    username: member?.displayName ?? u?.displayName ?? u?.username ?? '',
    tag: u?.username ?? '',
    server: g?.name ?? '',
    memberCount: g?.memberCount ?? '',
    ...vars,
  };
  return String(template ?? '').replace(/\{(\w+)\}/g, (match, key) => (key in values ? String(values[key]) : match));
}
