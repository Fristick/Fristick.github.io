// Démarrage du bot : client Discord, modules, slash commands (enregistrées serveur par serveur, effet immédiat).
import { ActivityType, Client, Events, GatewayIntentBits, Partials } from 'discord.js';
import { closeDb } from './db.mjs';
import { routeInteraction } from './router.mjs';
import { replyError } from './utils/embeds.mjs';
import { listen, log } from './utils/log.mjs';
import { SITE_URL } from './config.mjs';

import * as setup from './commands/setup.mjs';
import * as rank from './commands/rank.mjs';
import * as leaderboard from './commands/leaderboard.mjs';
import * as xp from './commands/xp.mjs';
import * as ticket from './commands/ticket.mjs';
import * as help from './commands/help.mjs';

import * as welcome from './features/welcome.mjs';
import * as verification from './features/verification.mjs';
import * as levels from './features/levels.mjs';
import * as tickets from './features/tickets.mjs';
import * as logs from './features/logs.mjs';
import * as tempVoice from './features/tempVoice.mjs';
import * as boosts from './features/boosts.mjs';
import * as setupPanel from './features/setupPanel.mjs';

const COMMANDS = new Map([setup, rank, leaderboard, xp, ticket, help].map((c) => [c.data.name, c]));
const FEATURES = [welcome, verification, levels, tickets, logs, tempVoice, boosts, setupPanel];

const token = process.env.DISCORD_TOKEN?.trim();
if (!token) {
  log.error('DISCORD_TOKEN is missing: put your bot token in the .env file (see README.md).');
  process.exit(1);
}

const client = new Client({
  intents: [
    GatewayIntentBits.Guilds,
    GatewayIntentBits.GuildMembers,
    GatewayIntentBits.GuildMessages,
    GatewayIntentBits.MessageContent,
    GatewayIntentBits.GuildVoiceStates,
  ],
  partials: [Partials.GuildMember, Partials.User],
  allowedMentions: { parse: ['users', 'roles'] },
});
client.setMaxListeners(40);

for (const feature of FEATURES) feature.register(client);

async function registerCommands(guild) {
  try {
    await guild.commands.set([...COMMANDS.values()].map((c) => c.data.toJSON()));
  } catch (error) {
    log.warn(`Could not register commands in ${guild.name}: ${error.message}`);
  }
}

listen(client, Events.ClientReady, async () => {
  client.user.setPresence({
    activities: [{ name: new URL(SITE_URL).host, type: ActivityType.Watching }],
    status: 'online',
  });
  for (const guild of client.guilds.cache.values()) {
    await registerCommands(guild);
    // Membres en cache : nécessaire pour comparer les rôles avant/après et lister les rôles au départ.
    await guild.members.fetch().catch(() => {});
  }
  log.info(`Logged in as ${client.user.tag} — ${client.guilds.cache.size} server(s)`);
});

listen(client, Events.GuildCreate, async (guild) => {
  await registerCommands(guild);
  await guild.members.fetch().catch(() => {});
  log.info(`Joined server ${guild.name}`);
});

listen(client, Events.InteractionCreate, async (interaction) => {
  if (interaction.isChatInputCommand() || interaction.isAutocomplete()) {
    const command = COMMANDS.get(interaction.commandName);
    if (!command) return;
    if (interaction.isAutocomplete()) return command.autocomplete?.(interaction);
    if (!interaction.inGuild()) return replyError(interaction, 'This command only works in a server.');
    try {
      await command.execute(interaction);
    } catch (error) {
      log.error(`/${interaction.commandName}`, error);
      await replyError(interaction, 'Something went wrong. Please try again.').catch(() => {});
    }
    return;
  }
  if (interaction.isMessageComponent() || interaction.isModalSubmit()) await routeInteraction(interaction);
});

client.on(Events.Error, (error) => log.error('Client', error));
process.on('unhandledRejection', (error) => log.error('Unhandled rejection', error));

async function shutdown() {
  log.info('Shutting down…');
  await client.destroy().catch(() => {});
  closeDb();
  process.exit(0);
}
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);

try {
  await client.login(token);
} catch (error) {
  // Les deux erreurs de configuration les plus courantes, expliquées au lieu d'une pile d'appels.
  if (/disallowed intents/i.test(error.message)) {
    log.error(
      'Discord refused the connection: enable "Server Members Intent" and "Message Content Intent" in the ' +
        'Developer Portal (your application > Bot > Privileged Gateway Intents), save, then restart.',
    );
  } else if (error.code === 'TokenInvalid' || /invalid token/i.test(error.message)) {
    log.error('The bot token is invalid: copy it again (Developer Portal > Bot > Reset Token) into .env.');
  } else {
    log.error('Could not connect to Discord', error);
  }
  closeDb();
  process.exit(1);
}
