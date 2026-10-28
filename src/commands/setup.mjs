// /setup : ouvre le panneau de configuration de tous les modules (réservé aux administrateurs).
// Commande enregistrée par serveur (index.mjs) : elle n'existe donc pas en DM.
import { PermissionFlagsBits, SlashCommandBuilder } from 'discord.js';
import { showSetupPanel } from '../features/setupPanel.mjs';

export const data = new SlashCommandBuilder()
  .setName('setup')
  .setDescription('Configure the bot: welcome, verification, levels, tickets, logs, voice and boosts')
  .setDefaultMemberPermissions(PermissionFlagsBits.Administrator);

export async function execute(interaction) {
  await showSetupPanel(interaction);
}
