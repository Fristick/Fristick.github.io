// Routage des boutons, menus et fenêtres (modals). Un customId s'écrit « espace:action:arg1:arg2 » ;
// chaque module réserve son espace (setup, verify, ticket, vc, lb…) et reçoit (interaction, [action, ...args]).
import { replyError } from './utils/embeds.mjs';
import { log } from './utils/log.mjs';

const components = new Map();
const modals = new Map();

export function onComponent(namespace, handler) {
  components.set(namespace, handler);
}

export function onModal(namespace, handler) {
  modals.set(namespace, handler);
}

export async function routeInteraction(interaction) {
  const [namespace, ...args] = interaction.customId.split(':');
  const handler = (interaction.isModalSubmit() ? modals : components).get(namespace);
  if (!handler) return; // ex. boutons d'un collecteur local, gérés ailleurs
  try {
    await handler(interaction, args);
  } catch (error) {
    log.error(`Interaction ${interaction.customId}`, error);
    await replyError(interaction, 'Something went wrong. Please try again.').catch(() => {});
  }
}
