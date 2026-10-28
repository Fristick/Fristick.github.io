// Journal console horodaté (visible dans la console Pterodactyl).
const stamp = () => new Date().toISOString().replace('T', ' ').slice(0, 19);

export const log = {
  info: (...args) => console.log(`[${stamp()}]`, ...args),
  warn: (...args) => console.warn(`[${stamp()}] WARN`, ...args),
  error: (...args) => console.error(`[${stamp()}] ERROR`, ...args),
};

// Abonne un module à un événement du client ; une erreur est journalisée au lieu de faire planter le bot.
export function listen(client, event, handler) {
  client.on(event, async (...args) => {
    try {
      await handler(...args);
    } catch (error) {
      log.error(`Event ${event}`, error);
    }
  });
}
