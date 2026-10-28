# Bot Discord de Renard Studio

Bot du serveur Discord de [Renard Studio](https://renard-studio.fr). Tout ce qu'il affiche dans Discord est en anglais.

| Module | Ce qu'il fait |
| --- | --- |
| **Rules** | Règlement posté par le bot : règles de Discord (CGU, âge minimum), respect, pas de racisme ni de discrimination, pas de NSFW, spam, pub, arnaques, usurpation, vocaux. Boutons vers les règles de Discord. Texte modifiable |
| **Welcome** | Annonce de bienvenue avec image (avatar, pseudo, numéro de membre), bouton vers le site. Options : DM de bienvenue, rôle donné à l'arrivée |
| **Verification** | Panneau avec un bouton **Verify** qui donne le rôle `Verified`. Option *Restrict @everyone* : le serveur n'est visible qu'une fois vérifié |
| **Levels** | XP par message (15–25, une fois par minute) et en vocal (10/min, à au moins 2, sans être muet/sourd ni AFK). Annonce des niveaux, grades (Active Member → 👑 Legend) avec message de promotion, `/rank` (carte image avec le grade), `/leaderboard`, embed « Rank System » avec la liste des grades |
| **Tickets** | Menu Support / Bug Report / Partnership / Other → salon privé, boutons Claim et Close, transcript HTML dans les logs et en DM |
| **Logs** | Arrivées (alerte si compte de moins de 7 jours), départs (rôles, temps passé), ajouts/retraits de rôles avec leur auteur, rôles créés/supprimés |
| **Create your voice** | Rejoindre `➕ Create your voice` crée un vocal à ton nom, supprimé quand il est vide. Panneau de contrôle : renommer, limite, verrouiller, cacher, expulser, transférer, récupérer |
| **Boosts** | Message de remerciement à chaque boost (même un 2ᵉ boost du même membre), fins de boost dans les logs |
| **/setup** | Une seule commande pour tout configurer, avec un bouton **Auto Setup** qui crée salons, catégories et rôle |

## 1. Créer le bot sur Discord

1. [Portail des développeurs Discord](https://discord.com/developers/applications) → **New Application** → nom
   « Renard Studio » (ou autre), avatar.
2. Onglet **Bot** :
   - **Reset Token** → copiez le jeton (il ne s'affiche qu'une fois ; ne le partagez jamais) ;
   - **Privileged Gateway Intents** : activez **Server Members Intent** et **Message Content Intent**, puis
     **Save Changes**. Sans eux, le bot s'arrête au démarrage avec l'erreur `Used disallowed intents`.
3. Invitez le bot : remplacez `APPLICATION_ID` par l'**Application ID** (onglet *General Information*) :

   ```
   https://discord.com/oauth2/authorize?client_id=APPLICATION_ID&scope=bot+applications.commands&permissions=563328197192912
   ```

   Permissions incluses : Manage Roles, Manage Channels, View Audit Log, View Channels, Send Messages, Manage
   Messages, Embed Links, Attach Files, Read Message History, Add Reactions, Use External Emojis, Connect, Move
   Members, Create Public/Private Threads, Send Messages in Threads, Send Polls. Le bot ne peut interdire aux
   membres que ce qu'il a lui-même le droit de faire : les permissions de fils et de sondages lui servent à bloquer
   les fils et les sondages dans les salons en lecture seule.
4. **Paramètres du serveur → Rôles** : glissez le rôle du bot **au-dessus** de `Verified` et des rôles récompenses.
   Un bot ne peut donner que les rôles placés sous le sien. `/setup` vous prévient si ce n'est pas le cas.

## 2. Lancer en local (pour tester)

Il faut Node.js 22.13 ou plus récent.

```bash
npm install
```

Copiez `.env.example` en `.env` et collez le jeton après `DISCORD_TOKEN=`, puis :

```bash
npm start
```

La console affiche `Logged in as …`. Les commandes apparaissent tout de suite sur chaque serveur où est le bot.

Si PowerShell répond « l'exécution de scripts est désactivée sur ce système », utilisez `npm.cmd` à la place de
`npm` (`npm.cmd install`, `npm.cmd run check`…), ou lancez le bot avec `node index.js`.

`npm run check` vérifie le bot sans se connecter à Discord : syntaxe, commandes, formule d'XP, base de données,
transcript et rendu des images (le dossier des PNG est indiqué à la fin).

## 3. Premier réglage : `/setup`

`/setup` est réservé aux **administrateurs** (il crée des rôles et des salons, et choisit les rôles que le bot
distribue) ; seul vous voyez le panneau.

1. **⚡ Auto Setup** crée ce qui manque, sans toucher aux salons existants :
   - le rôle `Verified` ;
   - les 6 grades de niveau, si aucune récompense de niveau n'existe encore (voir plus bas) ;
   - 📌 INFORMATION : `#rules`, `#welcome`, `#verify`, `#boosts` ;
   - 📈 LEVELS : `#level-ups` ;
   - 🎫 SUPPORT : `#open-a-ticket`, plus la catégorie 🎫 TICKETS ;
   - 🔊 VOICE : `➕ Create your voice` ;
   - 🔒 STAFF (cachée) : `#member-logs`, `#role-logs`, `#ticket-logs`.

   Il poste aussi le règlement, les panneaux de vérification et de tickets, et l'embed « Rank System ».
   Sur un serveur déjà configuré, relancer **Auto Setup** ne crée que ce qui manque (par exemple `#rules` et les
   grades).
2. **Tickets** : choisissez le rôle staff qui verra et traitera les tickets.
3. **Verification → Restrict @everyone** (facultatif) : les non-vérifiés ne voient plus que `#verify` et `#rules`.
   Le rôle `Verified` reçoit les permissions qu'avait @everyone. Quand le module Rules est activé, le panneau de
   vérification rappelle que vérifier = accepter le règlement.
4. **Rules → Edit rules** pour adapter le règlement (vider le champ remet le texte par défaut), puis
   **Post / update** : le message déjà posté est modifié sur place.
5. **Levels → Rewards → 🎖️ Create ranks** crée (ou réutilise, s'ils existent déjà sous ce nom) les grades et les
   enregistre comme récompenses :

   | Niveau | Grade | Couleur |
   | --- | --- | --- |
   | 5 | Active Member | `#fee75c` |
   | 10 | 🎮 Regular | `#3498db` |
   | 20 | ⚔️ Veteran | `#9b59b6` |
   | 30 | 💎 Diamond | `#00ffff` |
   | 40 | 🔥 Elite | `#e74c3c` |
   | 50 | 👑 Legend | `#ffd700` |

   Un membre ne garde que son grade le plus haut (*Stack rewards* : Off) : il est promu d'un grade au suivant, avec
   un message « Promotion! » à chaque palier. Les grades sont placés juste au-dessus de `Verified` pour que leur
   couleur s'affiche, et les membres qui ont déjà de l'XP reçoivent leur grade tout de suite. Pour changer un palier :
   retirez-le dans **Rewards**, puis **➕ Add reward** au niveau voulu. Les niveaux par défaut sont dans
   `src/config.mjs` (`DEFAULT_RANKS`).
6. Chaque module se règle ensuite depuis le menu **Configure a module** : salons, rôles, textes (**Edit message**),
   **Send test**.

Variables utilisables dans les textes : `{user}` (mention), `{username}`, `{tag}`, `{server}`, `{memberCount}`. En
plus : `{level}` pour les niveaux, `{boostCount}` et `{boostTier}` pour les boosts, `{tickets}` (salon des tickets)
pour le règlement.

## Commandes

| Commande | Qui | Rôle |
| --- | --- | --- |
| `/rank [user]` | Tous | Carte de niveau |
| `/leaderboard` | Tous | Classement |
| `/help` | Tous | Aide |
| `/setup` | Administrateur | Panneau de configuration |
| `/xp add · remove · set · setlevel · reset` | Gérer le serveur | Modifier l'XP d'un membre, ou tout remettre à zéro |
| `/ticket add · remove · rename · close` | Staff (dans un ticket) | Gérer un ticket |

## 4. Mise en ligne sur Pterodactyl

1. Générez l'archive :
   ```bash
   npm run zip
   ```
   Elle contient le code, l'egg et un `.env` vide.

   Options :
   - `npm run zip -- --avec-jeton` reprend le jeton de votre `.env` local (ne partagez pas ce zip) ;
   - `npm run zip -- --avec-donnees` inclut `data/` (réglages `/setup`, XP, tickets).
2. Créez un serveur avec l'egg **`deploy/pterodactyl-egg.json`** (Admin > Nests > Import Egg), image **Node.js 22**.
   L'egg du site convient aussi. Variables : **User Uploaded Files = 1**, **Main file = `index.js`**. 512 Mo de RAM
   suffisent. Aucun port n'est utilisé.
3. Gestionnaire de fichiers : envoyez `bot-renard-pterodactyl.zip`, puis **Unarchive**. `index.js` et `package.json`
   doivent être à la racine. Supprimez ensuite le zip.
4. Ouvrez `.env` et collez le jeton après `DISCORD_TOKEN=` (sauf si vous avez utilisé `--avec-jeton`).
5. **Start**. Au premier démarrage, `npm install` télécharge discord.js et le moteur d'images pour Linux. Puis la
   console affiche `Logged in as …` et le serveur passe au vert.

Les données (réglages, XP, tickets) sont dans `data/bot.db` : sauvegardez-les avec les backups de Pterodactyl.

### Mettre à jour le bot déjà en ligne

```bash
npm run zip -- --mise-a-jour
```

Envoyez `bot-renard-maj.zip`, **Unarchive** en écrasant, puis redémarrez. Ni `data/` ni `.env` ne sont touchés.

## Dépannage

| Problème | Solution |
| --- | --- |
| `Used disallowed intents` au démarrage | Activez les 2 intents (étape 1.2) |
| `DISCORD_TOKEN is missing` | Le `.env` est absent ou vide |
| `An invalid token was provided` | Jeton recopié incomplet, ou régénéré depuis : refaites **Reset Token** |
| Les commandes n'apparaissent pas | Bot invité sans `applications.commands` : réinvitez-le avec le lien de l'étape 1.3 |
| Le rôle n'est pas donné | Rôle du bot sous le rôle à donner (étape 1.4) |
| « You can rename again … » sur un vocal | Discord limite à 2 renommages par salon toutes les 10 minutes |
