# Histoire et progression (v4.15) : le parcours linéaire

Ce document complète [`HISTOIRE_PROGRESSION_v4.13.md`](HISTOIRE_PROGRESSION_v4.13.md) : le fil narratif, l'accès de
l'hôte, les fins de chapitre et la migration décrits là restent valables. Il fixe les règles du **parcours linéaire**
(R-L1 à R-L6), telles que codées dans la v4.15. La logique pure (`BRStoryLogic`) est vérifiée hors moteur (banc de
l'histoire, section J, [`v415/banc_histoire.txt`](v415/banc_histoire.txt)) ; les étapes du test du jeu
`-BRAutoTestV415` sont préparées, à exécuter dans Unreal. Pour le joueur : [`GAMEPLAY_v4.15.md`](GAMEPLAY_v4.15.md).

## 1. État de la partie

Deux champs de plus dans l'état narratif (`BRStory::FState`), sauvegardés au **format 6** (`StoryFlags`,
`StoryRunSeed`) :

- `Flags` : bit `BRStory::Flag::Linear` (la partie suit le parcours linéaire) ;
- `RunSeed` : graine du parcours, tirée une fois à la nouvelle partie, jamais nulle.

Une sauvegarde d'un format plus ancien (5 et avant) est migrée avec `Flags = 0` et `RunSeed = 0` (parcours classique),
après une copie intacte du fichier d'origine (`BR_Partie_<n>_Format5.sav`). Une version 4.14 ouvre une partie au
format 6 en lecture seule (règle des formats plus récents).

## 2. Règles

**R-L1. Création.** NOUVELLE PARTIE crée toujours une partie linéaire (`Flags |= Linear`, `RunSeed` aléatoire non
nul). Il n'y a pas de choix : la page de création garde ses deux éléments. La carte de la partie porte la pastille
LINÉAIRE (`Menu.BadgeLineaire`). Les parties existantes et migrées restent classiques. REJOUER, le mode développeur
(dont REJOUER d'un niveau pas encore exploré) et la session libre ne sont jamais linéaires : le niveau est linéaire
seulement dans une session d'histoire de l'hôte (`HostSessionMode() == Story`) d'une partie linéaire.

**R-L2. Graine des niveaux.** Dans une partie linéaire, chaque niveau a une graine fixe :
`BRStory::LevelSeedFor(S, Level)`, un hachage de `RunSeed` et du niveau, jamais nul, le même sur toutes les machines.

- Le Level 0 garde `IntroSeed` (`LevelSeedFor` rend 0 pour lui).
- La graine n'est appliquée que si aucune n'est imposée (`InSeed == 0`) : la reprise d'une session garde la sienne.
- `-BRSeed` ne la remplace pas (même règle qu'`IntroSeed`).
- Seuls les chargements de l'hôte en session d'histoire l'appliquent ; les clients reçoivent la graine avec le niveau.

**R-L3. Sorties.** Dans un niveau linéaire (`FBRNetLevel::Flags`, bit 0), `ABRChunk::PlanExits` ne pose **aucune**
sortie tirée par les chunks : ni sortie de progression supplémentaire, ni sortie de retour. Restent : la sortie garantie
de la mission vers le niveau suivant, la porte de service du Level 0, l'échelle du Level 6 posée par la mission et la
sortie de fin du Niveau 11.

- **Sécurité** : sans sortie garantie vers l'avant (mission non posée, niveau sans mission, aucune sortie de la mission
  vers le niveau suivant), les sorties de progression gardées restent ; seules les sorties de retour disparaissent.
- La règle est `BRStory::ChunkExitAllowed(bLinear, bForwardGuaranteed, bTargetGuarded)` : classique, toujours vrai ;
  linéaire avec sortie garantie, faux ; linéaire sans elle, `bTargetGuarded`.
- `D.Exits` (`BRLevels.cpp`) n'est pas modifié : le banc de contenu le lit.

**R-L4. Mort.** `BRStory::DeathRuleFor(Mode, bLinear, bNet)`, lue par le monde (`ABRWorld::GetDeathRule`), décide
seule de la mort, du texte de l'écran de mort et du niveau de réveil écrit dans une mort en attente :

| | Solo | Réseau |
|---|---|---|
| Histoire, parcours linéaire | `RestartLevel` : le niveau recommence de zéro (même graine, nouveau chargement, mission neuve, équipement de départ, reprise inchangée, mort comptée) ; texte « Le niveau recommence. » | `RespawnInPlace` |
| Histoire, parcours classique | `BackToStart` : retour au Level 0 | `RespawnInPlace` |
| REJOUER | `RestartLevel` : le niveau rejoué recommence | `RespawnInPlace` |
| Mode libre, développeur | `BackToStart` | `RespawnInPlace` |

- `RestartLevel` en solo : `RequestTransition(niveau en cours, true)` sans graine ; `LoadLevelNow` applique la graine
  du niveau (R-L2), le chargement est neuf (`NetLevel.Serial` + 1).
- Jeu fermé pendant une mort : `PendingDeathLevel` est le niveau de la mort, sauf si la règle ramène au Level 0
  (`BackToStart` : 0). `ResolvePendingDeath` (au choix de la partie, ou au plus tard au début de `StartSolo`) donne
  l'équipement de départ et efface la session ; dans une partie classique morte en solo, il applique aussi l'arrivée au
  Level 0 (`BRStory::OnArrive(S, 0)`) : CONTINUER y ramène, comme une mort en jeu (avant la v4.15, CONTINUER reprenait
  au niveau de la mort). Dans une partie linéaire, CONTINUER recharge toujours le niveau de la mort avec sa graine, même
  si le décor du menu montre déjà ce niveau.

**R-L5. Inchangé.** L'ordre `0 → 1 → 37 → 2 → … → 11 → dernier quai`, `CanEnter`, les Poolrooms obligatoires, les fins
de chapitre par canal, le tirage et la sauvegarde de la porte de service, les fosses, le Level 6, les neuf entités,
« aucune action ne demande deux joueurs à la fois », les règles d'écriture de la partie (`WritePolicyFor`), aucune
narration à chaque image. Après la fin, `CloseEnding` mène toujours à l'exploration (`RequestTransition(-1)`).

**R-L6. Coopération.** La partie de l'hôte décide : le drapeau du niveau arrive avec lui (`FBRNetLevel::Flags`, comme
la graine) et chacun voit la même et unique sortie. La mort en réseau ne change pas (fenêtre de réanimation, puis réveil
au point de départ du niveau avec l'équipement de départ, message `World.ReveilDepartNiveau`). Protocole réseau 415.

## 3. Vérifications

- Banc de l'histoire, section J : graines (déterminisme, jamais nulles, distinctes), table complète de `DeathRuleFor`,
  table des 8 cas de `ChunkExitAllowed`, fil, accès et reprise identiques en linéaire, mort classique fermée à terre
  ramenée au Level 0, règles d'écriture, nouvelle partie, REJOUER et mode développeur, migration toujours classique.
- Test du jeu `-BRAutoTestV415` (préparé) : menu, graine et drapeau du Niveau 2, sortie unique, mort et jeu fermé
  pendant la mort (linéaire et classique), contraste classique, marche jusqu'au Niveau 3, version publiée, REJOUER,
  mode développeur, migration 5 → 6, protocole ; `-BRNetTest` : drapeau, graine et sortie identiques chez l'hôte et le
  client, mort du client dans le niveau.
