// Point d'entrée pour les hébergeurs qui lancent « index.js » (ex. egg Node.js de Pterodactyl).

// node:sqlite est stable en pratique mais Node 22 affiche encore un avertissement « experimental » : on le masque.
process.removeAllListeners('warning');
process.on('warning', (warning) => {
  if (warning.name === 'ExperimentalWarning' && /SQLite/i.test(warning.message)) return;
  console.warn(warning);
});

// Variables d'environnement depuis .env (absent = on garde celles du système, ex. variables Pterodactyl).
try {
  process.loadEnvFile();
} catch {}

await import('./src/index.mjs');
