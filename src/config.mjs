// Constantes du bot : identité visuelle (reprise du site renard-studio.fr), réglages d'XP, catégories de tickets
// et configuration par défaut de chaque module. Tout ce qui s'affiche dans Discord est en anglais.

export const BRAND = 'Renard Studio';
export const SITE_URL = 'https://renard-studio.fr';

// Couleurs du site (src/assets/css du site) : entiers pour les embeds, chaînes pour le canvas.
export const COLORS = {
  blue: 0x2a5bff,
  red: 0xe3191f,
  gold: 0xffd166,
  green: 0x5ee69a,
  muted: 0xb9c3ea,
  bg: 0x010717,
  surface: 0x08133a,
};

export const HEX = {
  blue: '#2a5bff',
  blueText: '#9db2ff',
  red: '#e3191f',
  redText: '#ff6b70',
  gold: '#ffd166',
  green: '#5ee69a',
  text: '#f3f5ff',
  muted: '#b9c3ea',
  line: '#1f3170',
  bg: '#010717',
  bg2: '#040d29',
  surface: '#08133a',
  surface2: '#0c1a4a',
};

// Gain d'XP. Niveau suivant : 5·L² + 50·L + 100 XP (formule type MEE6), voir features/levels.mjs.
export const XP = {
  messageMin: 15,
  messageMax: 25,
  messageCooldownMs: 60_000,
  voicePerMinute: 10,
};

// Grades gagnés avec les niveaux (du plus bas au plus haut). /setup → Levels → Rewards → « Create ranks » crée les
// rôles manquants et les enregistre comme récompenses ; un membre ne garde que son grade le plus haut.
// Couleurs relevées sur la liste de rôles fournie par Renard Studio.
export const DEFAULT_RANKS = [
  { level: 5, name: 'Active Member', color: '#fee75c' },
  { level: 10, name: '🎮 Regular', color: '#3498db' },
  { level: 20, name: '⚔️ Veteran', color: '#9b59b6' },
  { level: 30, name: '💎 Diamond', color: '#00ffff' },
  { level: 40, name: '🔥 Elite', color: '#e74c3c' },
  { level: 50, name: '👑 Legend', color: '#ffd700' },
];

export const TICKET_CATEGORIES = [
  { id: 'support', label: 'Support', emoji: '🛠️', description: 'General help or questions' },
  { id: 'bug', label: 'Bug Report', emoji: '🐛', description: 'Report a bug in one of our games' },
  { id: 'partnership', label: 'Partnership', emoji: '🤝', description: 'Collaboration or partnership request' },
  { id: 'other', label: 'Other', emoji: '❓', description: 'Anything else' },
];

export const DISCORD_TERMS_URL = 'https://discord.com/terms';
export const DISCORD_GUIDELINES_URL = 'https://discord.com/guidelines';

// Règlement par défaut (modifiable dans /setup → Rules ; vider le champ revient à ce texte).
// {tickets} = salon d'ouverture des tickets. Moins de 4000 caractères : limite d'un champ de fenêtre Discord.
export const DEFAULT_RULES = `Welcome to **{server}**! 🦊
To keep this community friendly and safe, everyone must follow these rules. By staying on the server, you agree \
to them.

**1 · Follow Discord's rules**
Respect the [Discord Terms of Service](${DISCORD_TERMS_URL}) and the \
[Community Guidelines](${DISCORD_GUIDELINES_URL}). You must be at least 13 years old (or the minimum age in your \
country).

**2 · Be respectful**
Treat everyone with kindness. No harassment, insults, bullying, threats or personal attacks. Debates are fine, \
toxicity isn't.

**3 · No racism, hate or discrimination**
Zero tolerance for racism, sexism, homophobia, transphobia or any discrimination based on origin, religion, gender, \
sexual orientation, disability or age — including slurs, "jokes", memes and symbols.

**4 · No NSFW or shocking content**
No sexual, gory, violent or disturbing content anywhere: messages, images, links, usernames, avatars or statuses.

**5 · No spam**
No flooding, walls of emojis, excessive caps or mass mentions. Don't ping staff or members without a good reason.

**6 · No advertising**
No self-promotion, server invites or DM advertising without the staff's permission.

**7 · Stay safe**
Never share personal information, yours or anyone else's. No scams, phishing links, malware, cheats or exploits.

**8 · No impersonation**
Don't pretend to be a staff member, a member of the {server} team or anyone else.

**9 · Respect voice channels**
No screaming, soundboard spam or voice changers to annoy others, and never record anyone without their consent.

**10 · Use the right channels**
Keep each channel on topic and follow its description.

**11 · Listen to the staff**
Staff decisions are final. A problem with a member or a sanction? Open a ticket in {tickets} instead of arguing \
in public.

⚠️ Breaking these rules can lead to a warning, a mute, a kick or a ban depending on how serious it is. \
The staff may also act on behaviour that isn't listed here.`;

// Configuration par défaut d'un serveur. Les textes acceptent les variables de utils/placeholders.mjs.
// Un module dont le salon vaut null est considéré comme non configuré.
export const DEFAULT_CONFIG = {
  rules: {
    enabled: false,
    channelId: null,
    title: '📜 Server Rules',
    message: DEFAULT_RULES,
    color: HEX.blue,
    panelMessageId: null,
  },
  welcome: {
    enabled: false,
    channelId: null,
    title: 'Welcome to {server}!',
    message: 'Hey {user}, welcome to **{server}**! 🦊\nYou are member **#{memberCount}**.',
    color: HEX.blue,
    image: true,
    dm: false,
    dmMessage: 'Welcome to **{server}**, {username}! Head to the verification channel to unlock the server.',
    autoRoleId: null,
  },
  verification: {
    enabled: false,
    channelId: null,
    roleId: null,
    title: '✅ Verification',
    message: 'Welcome to **{server}**!\nClick the button below to verify yourself and unlock the server.',
    color: HEX.green,
    buttonLabel: 'Verify',
    panelMessageId: null,
  },
  levels: {
    enabled: true,
    channelId: null, // null = annonce dans le salon où le membre a écrit
    message: 'GG {user}, you just reached **level {level}**! 🎉',
    ignoredChannelIds: [],
    stackRewards: true, // false = seul le rôle du plus haut palier atteint est gardé
    rewards: [], // [{ level: 5, roleId: '…' }]
    multiplier: 1,
  },
  tickets: {
    enabled: false,
    panelChannelId: null,
    categoryId: null,
    staffRoleIds: [],
    logChannelId: null,
    title: '🎫 Support',
    message: 'Need help? Pick a category in the menu below and a private channel will be opened with our team.',
    color: HEX.blue,
    panelMessageId: null,
  },
  logs: {
    memberChannelId: null, // arrivées / départs
    roleChannelId: null, // changements de rôles
  },
  tempVoice: {
    enabled: false,
    creatorChannelId: null,
    categoryId: null, // null = catégorie du salon « Create your voice »
    nameTemplate: "🔊 {username}'s channel",
    userLimit: 0,
  },
  boosts: {
    enabled: false,
    channelId: null,
    title: '💎 Thank you for boosting!',
    message: '{user} just boosted **{server}**! 🚀\nWe now have **{boostCount}** boosts (Level {boostTier}).',
    color: '#f47fff',
  },
};
