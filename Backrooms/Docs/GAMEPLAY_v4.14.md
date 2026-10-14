# Gameplay v4.14 : ce qui change pour le joueur

Les règles de la v4.13 ([`GAMEPLAY_v4.13.md`](GAMEPLAY_v4.13.md)) restent valables : Level 0 sans énigme, porte de
service tirée par partie, fil `0 → 1 → Poolrooms → 2`, Level 6 sans lumière au scanner. Ce document décrit seulement
ce que la v4.14 change. Rien de ceci n'a encore été joué dans Unreal (voir le [rapport](RAPPORT_v4.14.md)).

## 1. Parties : REJOUER ne touche plus à la campagne

| Mode | Ce qui est écrit dans la partie |
|---|---|
| **CONTINUER** / nouvelle partie (session d'histoire) | tout : équipement, santé, morts, temps de jeu, niveau, session, campagne, histoire, journal |
| **REJOUER** un niveau découvert | **seulement le journal** : un document ou une entité découverts en rejeu restent au carnet (union, rien n'est retiré) |
| Mode développeur | rien |

- En REJOUER, on peut consommer, ramasser, se blesser, mourir, finir le niveau, mettre en pause, revenir au menu ou
  quitter : en revenant à CONTINUER, l'équipement, la santé, la session et l'avancement sont ceux d'avant le rejeu.
- **Mort en REJOUER, en solo** : le niveau rejoué recommence (avant : retour au Level 0 de la campagne).

## 2. Anciennes parties et Poolrooms

- Une ancienne partie n'est créditée de la traversée des Poolrooms que si l'ordre de ses découvertes le **prouve** :
  le Niveau 4 découvert après les Poolrooms, sans qu'aucun autre accès au Niveau 4 (Niveaux 1, 3, 5, 11) ait été
  découvert avant lui, et dans un format de sauvegarde où la console n'ajoutait pas les niveaux visités.
- Sinon (doute ou absence), la partie reprend aux Poolrooms ; la progression d'après est **garée**, jamais perdue.
- La progression garée reste dans la sauvegarde jusqu'à ce que sa reprise ait vraiment réussi. Si son niveau n'est pas
  encore publié (version publique), le menu le dit : « elle reprendra quand ce niveau sera disponible » ; elle reprend
  d'elle-même à la mise à jour qui l'ajoute.

## 3. Scanner LiDAR (Level 6)

### 3.1 Débit identique à toute cadence

- Scan focalisé : **1110 rayons par seconde**, à 30 comme à 240 images/s (48 au plus par image). Une saccade ne
  concentre pas les rayons : elle les étale.
- Impulsion : 1400 rayons tracés en **0,2 s** (260 au plus par image), recharge de 1,6 s inchangée.

### 3.2 Une réserve qui ne refuse plus

- Chaque type de point a sa propre réserve : décor 15 000, dangers 3000, mécanismes 1500, menaces 600, repères 400,
  inscriptions 3500.
- Une réserve pleine **remplace ses points les plus anciens** : un nouveau relevé apparaît toujours ; scanner le décor
  pendant des minutes ne peut pas empêcher de voir un trou ou un Smiler.
- La persistance réglée (4 à 30 s) est exacte, même après une longue image.

### 3.3 Plaques et cages

- Une inscription (silhouette et code d'une porte, cage) se dessine **une fois par passage**, quel que soit le nombre de
  rayons qui la touchent ; plusieurs inscriptions se partagent un budget par image.
- Une plaque est notée au carnet quand son inscription est **complète** (une fois), plus à chaque rayon.

### 3.4 Un scanner par chargement

Recharger le Level 6 (autre graine, reprise, autre plan) efface les points, les plaques relevées, la cible et le scan
en cours.

### 3.5 Actions maintenues et Smilers

- Le scan focalisé (et son son) et le sprint s'arrêtent à l'ouverture de l'inventaire, à la pause, quand le curseur
  apparaît, quand on change une touche et quand la fenêtre perd le focus (Alt+Tab) ; le scan s'arrête aussi à la mort,
  pendant une transition et dans un dialogue. Il faut appuyer de nouveau pour reprendre (aucune touche « collée »).
- Repousser un Smiler demande de tenir le faisceau sur **le même** Smiler ; changer de cible remet le compte à zéro.
  L'hôte vérifie la cible et la durée.
- Repoussé, le Smiler recule vers **un point fixe** (jamais dans un mur ni au bord d'un trou), y attend, puis revient.

### 3.6 HUD du scanner

| Élément | Visible |
|---|---|
| Cône du scan focalisé | pendant le scan (et 0,35 s après) |
| Barre de recharge | pendant la recharge, puis un bref « prêt » (0,6 s) |
| Relevé d'une mire ou d'une cage | pendant le relevé |
| Rappel des touches | jusqu'au calibrage (tutoriel), puis 12 s à chaque arrivée au Level 6, puis effacé |
| Aide des parasites | dès que les parasites sont perceptibles ; **jamais masquée** |

**Réglage « Scanner : aide visuelle des parasites »** (Paramètres › Interface) : *standard* ; *grande* (1,5 fois plus
grande, fond opaque, ignore l'opacité du HUD) ; *grande + bords* (en plus, des chevrons au bord de l'écran du côté des
parasites, un à quatre selon l'intensité ; des deux côtés quand ils sont derrière). L'intensité se lit au nombre de
cases et de chevrons, pas seulement à la couleur.

## 4. Level 0

- Avec le kit Blender importé, les murs, passages, poteaux, piliers et le mur percé de la porte de service du Level 0
  sont ceux du kit (papier peint neuf ou jauni, plinthes, chambranles). Sans le kit, les murs habituels : le jeu est le
  même, seule l'apparence change. La disposition, la porte tirée par partie et les fosses ne changent pas.
- En coopération, un client dont la porte différerait de celle de l'hôte (état reçu après la construction) reprend la
  porte de l'hôte et reconstruit le Level 0.

## 5. Outils de développement (console, mode développeur)

| Commande | Effet |
|---|---|
| `BRLevel0Kit` | état du kit (complet, ou ce qui manque) |
| `BRLevel0Kit 0` / `1` | murs générés / modules du kit, reconstruits sur cette machine (comparaison à la même graine) |
| `BRLidarStats` / `BRLidarStats 1` | compteurs du scanner (rayons, acceptés, remplacés, expirés, refusés, inscriptions, temps) ; `1` les remet à zéro |
| `-BRNoLevel0Kit` (ligne de commande) | lance sans le kit |
