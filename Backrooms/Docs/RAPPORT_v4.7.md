# Rapport v4.7 : finition et fiabilité

Travail fait le 5 octobre 2026 sur la branche `backrooms`, à partir de la v4.6 (`be26444`). L'état des lieux est dans
[`AUDIT_v4.7.md`](AUDIT_v4.7.md).

## 1. Ce qui n'a pas pu être fait ici

**Unreal Engine n'est toujours pas disponible.** Le conteneur est sous Linux, avec 4 cœurs et 15 Go de mémoire, sans
carte graphique et sans moteur.

Rien n'a donc été :

- compilé ;
- importé ;
- lancé en PIE, en Standalone ou en version empaquetée ;
- capturé dans le moteur ;
- mesuré en images par seconde.

**Aucun résultat de ce type n'est donné dans ce rapport.** La section 4 liste chaque validation restante, avec la
commande à lancer.

Les contrôles faits ici :

- syntaxe C++ avec des en-têtes Unreal simplifiés ;
- modèles mathématiques en Python (fenêtres d'esquive, appui des pieds, voile de brouillard) ;
- génération rejouée en Python ;
- cartes de rugosité régénérées et comparées.

Ces contrôles ne valident ni la physique, ni le réseau, ni le rendu.

## 2. Ce qui a changé

### 2.1 Mort et coopération (`BRCharacter`, `BRWorld`, `BRPlayerController`, `BRHUD`)

**Le défaut corrigé.** En v4.6, `OnRep_Dead` déduisait la chute dans une fosse de la hauteur (`Z < -100`). Une mort
dans un bassin des Poolrooms (2,60 m de profondeur) était donc annoncée comme une chute, et ne pouvait pas être
relevée.

**Cause de mort explicite.** `EBRDeathCause` distingue la blessure, la noyade, la chute et la folie (santé mentale à
zéro).

| Cause | Réanimation | Délai | Message aux coéquipiers |
|---|---|---|---|
| Blessure | oui | 30 s | « X est à terre. » |
| Noyade | oui (on sort le corps de l'eau) | 20 s | « X se noie : remontez-le ! » |
| Folie | oui | 30 s | « X a perdu la raison : il est à terre. » |
| Chute dans une fosse | non | réveil au point de départ après 6 s | « X est tombé dans une fosse. » |

Le corps d'un noyé remonte à la surface : un coéquipier peut l'atteindre. L'écran « à terre » et les étiquettes de nom
suivent la cause. **Aucun message de fosse n'apparaît pour une mort dans l'eau.**

**Un état tenu par le serveur, le même partout.**

- `FBRDeathState` est répliqué à toutes les machines : mort, cause, relevable, événement (mort, relevé, réveil) et
  numéro de séquence.
- Le joueur concerné réagit tout de suite sur sa machine, puis le serveur vérifie et publie :
  - une chute annoncée hors d'une fosse devient une blessure ;
  - une noyade annoncée hors de l'eau devient une blessure ;
  - une blessure sans coup reçu dans les 6 s est signalée dans le journal.
- **Chute d'un joueur distant** : le serveur publie la mort lui-même, puis prévient le client. Si ce message se perd,
  l'état répliqué suffit, et le client exécute la chute à sa réception.
- **Réanimation** : vérifiée par le serveur (le coéquipier est mort et relevable, à moins de 4,5 m), puis publiée à
  tous.
- **Réveil** : annoncé par le client, accepté s'il est mort pour le serveur.
- **Changement de niveau** : tout le monde repart debout, par une décision du serveur.

Cas limites, analysés sans test réseau réel :

- La réplication n'envoie que le dernier état. Si une mort et une réanimation tombent entre deux envois, les autres
  joueurs ne voient que le résultat (vivant). L'état reste juste, seul le message manque.
- Si le réveil local (fin du délai) croise une réanimation en route, le joueur se réveille au départ. L'état reste
  cohérent (vivant).

**Commandes de test contrôlées par le serveur.**

- `ServerCheat` (coupure, objectifs, entité) est refusé si le serveur n'autorise pas les commandes de test :
  `AreCheatsAllowed()`, toujours faux dans une version Shipping.
- `ServerRequestTransition` n'est accepté que :
  - si le joueur est vivant, près d'une sortie ouverte (6 m) qui mène à ce niveau ;
  - ou si les commandes de test sont permises.
- Les commandes console de test (`BRLevel`, `BRGod`, `BRSpawn`, `BRGiveAll`, `BRBlackout`, `BRObjectives`, `BRPits`,
  `BRSeed`) passent par la même règle.
- Le mode invincible d'un client ne vaut, pour le serveur, que si celui-ci autorise les commandes de test.
- `IsDevMode()` est toujours faux en Shipping, quel que soit le réglage `DevMode` du fichier ini.

### 2.2 « Reprendre » devient une vraie reprise (`BRSave`, `BRPlayerController`, `BRWorld`)

**Format 2 de la sauvegarde.** Il garde la session en cours :

- niveau et graine ;
- cassettes trouvées ;
- coupure filmée, entité filmée ;
- objets déjà ramassés ;
- dernier point sûr où le joueur se tenait.

Ce point sûr est relevé deux fois par seconde : au sol, hors de l'eau profonde, pas au-dessus d'une fosse, dans une
cellule atteignable.

**Reprise.**

- Même graine, donc même disposition.
- Les objectifs et les objets ramassés sont remis **avant la construction des chunks** : un objet déjà ramassé n'est
  jamais créé.
- Le sol autour du point de reprise est construit d'abord.
- Le joueur n'y est posé qu'après un rayon qui touche vraiment le sol et un test de capsule dégagée. Si le point n'est
  plus valide (mur, fosse, sol absent), il est posé au point de départ.
- L'hôte d'une partie en ligne reprend aussi sa session : elle est gardée à travers le rechargement de la carte.

**Mort et fermeture du jeu ne se confondent plus.**

- Fermer le jeu à terre, ou pendant le fondu de la mort, ne l'annule plus. La mort est écrite tout de suite dans la
  sauvegarde (`bPendingDeath`). Au chargement suivant : équipement de départ, niveau neuf.
- Avant la v4.7, l'inventaire d'avant la mort était gardé.
- La règle voulue est conservée : seul, une mort ramène au Niveau 0 avec une **nouvelle graine**.
- La carte de la partie dit ce que donnera « Reprendre » : « à l'identique » ou « nouvelle disposition ».

**Anciennes parties, fichiers abîmés.**

- **Format 1 migré en mémoire.** Inventaire, journal, niveaux explorés, temps de jeu et morts sont gardés. Une copie
  intacte du fichier est faite une fois (`BR_Partie_<n>_Format1.sav`).
  - Piège évité : Unreal n'écrit pas les propriétés égales à leur valeur par défaut. La valeur par défaut de `Version`
    reste donc 1, et l'écriture y met 2. Un fichier v4.6, où `Version` est absent, est bien reconnu comme format 1.
- **Copie de secours.** Le fichier principal puis une copie (`_Secours`) sont écrits à chaque fois, par fichier
  temporaire et renommage. Si le principal est illisible, la partie reprend depuis la copie, et le joueur en est
  prévenu.
- **Rien de lisible.** Le fichier est mis de côté (`_Illisible`), pas effacé, et l'emplacement redevient libre. Un
  message dit où le fichier se trouve. Avant, l'emplacement restait occupé, sans explication.

**Écritures.**

- L'instantané est pris sur le thread du jeu : l'état est cohérent.
- Le fichier est écrit sur un thread de fond, par une seule file, dans l'ordre des demandes. Un instantané plus récent
  remplace celui qui attend encore.
- La fermeture du jeu, le retour au menu et la fin de partie attendent la fin de l'écriture.
- Un échec d'écriture est signalé une fois à l'écran.

**Corrigé au passage.** Au changement de niveau, la construction immédiate se faisait autour de l'ancienne position du
joueur. Le sol du point d'arrivée n'arrivait que quelques images plus tard.

### 2.3 Attaques et mouvement des entités (`BREntity`)

**Le défaut corrigé.** En v4.6, les dégâts étaient appliqués dans la même image que l'ordre de frappe, avant le
geste.

**Attaque en trois temps.**

1. **Préparation.**
   - Le geste armé se forme, et le cri est joué chez tous les joueurs proches.
   - L'entité se tourne vers sa proie et ralentit, selon l'espèce (le Hound s'arrête net, le Skin-Stealer continue
     d'avancer).
   - Elle abandonne si la proie s'éloigne au-delà de trois fois sa portée.
2. **Impact.**
   - La frappe est envoyée à tous, de façon fiable.
   - Le coup touche **une seule fois**, entre 0,05 s et 0,05 s + la fenêtre de l'espèce, si quatre conditions sont
     réunies : la proie est à portée (avec la fente des espèces qui bondissent), à hauteur, devant l'entité, et
     visible (ligne de vue, cachette).
3. **Récupération.** Lente, puis délai avant la prochaine attaque.

| Espèce | Préparation | Fenêtre | Récupération | Fente |
|---|---|---|---|---|
| Smiler | 0,40 s | 0,15 s | 0,45 s | 30 cm |
| Hound | 0,30 s | 0,18 s | 0,55 s | 70 cm |
| Faceling | 0,50 s | 0,15 s | 0,60 s | 15 cm |
| Skin-Stealer | 0,30 s | 0,16 s | 0,65 s | 45 cm |
| Deathmoth | 0,45 s | 0,15 s | 0,60 s | 30 cm |
| Wretch | 0,65 s | 0,18 s | 0,90 s | 10 cm |
| Partygoer | 0,45 s | 0,15 s | 0,50 s | 35 cm |
| Clump | 0,60 s | 0,20 s | 0,85 s | 25 cm |
| Bacteria | 0,32 s | 0,15 s | 0,50 s | 45 cm |

`Tools/attack_timing.py` (`Docs/v47/fenetres_esquive.txt`) calcule, avec un modèle cinématique simple, ce que le
joueur peut éviter.

- Chaque attaque peut être esquivée par un joueur **déjà en fuite** au sprint, ou qui **se met à couvert**.
- Un joueur **arrêté qui se met à courir au cri**, à 25 cm de plus que la portée, s'échappe : Smiler, Faceling,
  Deathmoth, Wretch, Partygoer, Clump.
- Les mêmes conditions ne suffisent pas contre les espèces qui bondissent vite (Hound, Skin-Stealer, Bacteria) : il
  faut couper la ligne de vue.

**Ce modèle ignore les accélérations et la latence.** En réseau, la position d'un client vue par le serveur a un
retard : une esquive au tout dernier moment peut ne pas compter (section 7).

**Démarches distinctes.** Chaque espèce a sa démarche : accélération, freinage, rotation, et un rythme de vitesse.

| Espèce | Démarche |
|---|---|
| Bacteria | saccadée : élans courts, arrêts nets (freinage à 5 000 cm/s²), demi-tours brusques |
| Hound | quadrupède : vitesse qui pulse au rythme du galop, virages larges |
| Wretch | épuisé : avance par à-coups, trébuche toutes les 3 à 6 s (corps qui plonge) |
| Skin-Stealer | trompeur : allure tranquille, puis poussée brutale (×1,4) quand la proie est à moins de 6,5 m |
| Clump | lourd : avance quand son poids retombe sur une main, presque à l'arrêt entre deux appuis |

**Appui de chaque pied.**

- Le réglage v4.6 décalait le corps pour poser le pied le plus bas, en supposant un sol plat.
- Maintenant, un rayon sous chaque pied (20 fois par seconde) mesure le sol réel : trottoir, marche, bord de bassin.
  - Le bassin descend vers le sol le plus bas.
  - Chaque jambe plie le genou pour poser le pied sur un sol plus haut (IK à deux segments).
  - Les pattes du Hound se replient.
- La formule est vérifiée par `Tools/check_leg_ik.py` : 648 cas, le pied monte exactement de la hauteur demandée,
  sans dériver.
- Le sens des pivots est celui de la pose armée, qui fonctionne déjà. Il reste à confirmer à l'écran.

**Smiler.**

- L'émission est plafonnée, et atténuée de près (jusqu'à 50 % à 1,2 m) : le visage ne devient plus une tache blanche
  qui cache les yeux et les dents.
- Elle est plus vive pendant la préparation.

**Wretch et Clump.** Des cartes de rugosité donnent des bouches et des plaies luisantes, des dents mi-brillantes et un
grain de peau variable (§ 2.4). Les silhouettes sont inchangées.

### 2.4 Matières et niveaux 0, 1 et 37

**Cartes de rugosité.** `Tools/generate_roughness.py` en produit dix, tirées des textures livrées, qui ne sont pas
modifiées. Planche : `Docs/v47/rugosite_cartes.jpg`.

| Texture | Ce que la carte distingue | Taille |
|---|---|---|
| Papier peint du Niveau 0 | encre plus lisse, auréoles humides satinées | 1024 |
| Moquette | fibres, zones humides un peu lustrées | 1024 |
| Plafond du Niveau 0 | ossature métallique (0,4) contre dalles de fibre (0,92) | 1024 |
| Béton de sol | huile et eau lisses, pores rugueux, usure | 1024 |
| Béton | grain, pores | 1024 |
| Panneaux métalliques | rouille rugueuse, rivets polis, joints sales | 512 |
| Carrelage des Poolrooms | émail brillant (0,10), joints poreux (0,80) | 512 |
| Plâtre | coups de taloche | 1024 |
| Peau du Wretch, peau du Clump | bouches et plaies luisantes, dents, grain | 1024 |

**Format des cartes.**

- Les cartes sont **relatives** : 0,5 vaut la rugosité nominale de la surface, propre à chaque niveau. La même carte
  sert donc partout où la texture apparaît, et les autres niveaux en profitent.
- Matériaux C++ et Python (`MATERIAL_VERSION 6`) : paramètres `RoughTex` et `RoughContrast`. Un seul échantillon,
  projeté sur l'axe dominant de la face.
- Import linéaire (sRGB désactivé).
- À l'exécution, la carte n'est branchée que si l'échantillonneur du matériau parent est linéaire.

**Parking.**

- Peintures satinées (rugosité 0,42), qui ne prennent presque pas le grain du béton (`RoughDetail`).
- Sol mouillé et flaques : inchangés.
- Le béton nu, les taches d'huile et le métal ont chacun leur réponse.

**Murs sans répétition visible.** Le papier peint se répète tous les 1,2 m. Une variation de teinte et des auréoles
d'humidité, dessinées à l'échelle du monde (17 m), cassent cette répétition sans toucher au motif : Niveau 0 et
parking (`WallVariation`).

**Fosses : le fond redevient noir.**

- Sans brouillard volumétrique, le brouillard ordinaire ajoutait sa couleur au fond d'un puits de 14 m.
- Estimation (`Tools/pit_fog_estimate.py`, `Docs/v47/voile_fosses.txt`) : 12 % de voile, un fond gris-jaune à
  55/255 au lieu du noir.
- Le post-traitement `M_BR_PitShade`, actif seulement au Niveau 0, assombrit ce qui est sous le sol, progressivement de
  0,9 m à 6,9 m de profondeur. Le fond tombe à environ 5/255, et le haut des parois garde sa lumière.
- **Ce sont des estimations, pas des mesures.**

### 2.5 Construction des chunks étalée (`BRChunk`, `BRWorld`)

**Le défaut corrigé.** En v4.6, le budget de 5 ms était vérifié entre deux chunks. Un chunk riche (une vingtaine de
lumières, des dizaines de lots d'instances) se construisait d'un bloc.

**Construction en deux temps.**

1. **`BeginBuild` planifie tout** sans créer de composant : grille, lots d'instances, lumières à créer.
2. **`StepBuild` crée ensuite les composants**, dans l'ordre :
   1. collisions (sol, murs) ;
   2. visuels ;
   3. lumières, une à une, avec leur luminaire ;
   4. objets (ramassables, sorties).

**Budget et garanties.**

- `StepBuild` travaille dans un budget par image : 4 ms, réglable par `-BRChunkBudget=`.
- Le sol sous chaque joueur, et autour, est terminé sans attendre : jamais de trou de collision sous les pieds. Ces
  constructions forcées sont comptées.
- `IsChunkLoaded` signifie maintenant « chunk prêt » : rien n'apparaît et aucune entité ne passe dans un chunk en
  préparation.
- L'hôte et les clients construisent la même chose, seul le rythme diffère.

**Préparation hors du thread principal : non retenue.** La planification crée des matériaux (des `UObject`) et charge
des maillages, deux opérations réservées au thread du jeu.

**Mesures préparées.** Le test automatique note :

- le pire temps de construction par image ;
- le temps moyen par étape ;
- la planification la plus longue ;
- les constructions forcées.

**Autres leviers.**

- `-BRPitShadows=half|none` mesure le coût des néons à ombres des salles de fosses (jusqu'à 9 par salle). Par défaut,
  toutes les ombres restent : sans elles, la lumière traverserait la dalle.
- Les entités ne projettent plus d'ombre au-delà de 30 m (rétablie en deçà de 26 m, jamais pendant un jumpscare). De
  près, rien ne change.

### 2.6 Peur, lisibilité, confort

**Directeur de tension** (serveur, répliqué). Les phases s'enchaînent ainsi :

1. calme ;
2. malaise ;
3. détection (une entité a remarqué quelqu'un, ou il fait noir pendant une coupure) ;
4. poursuite ;
5. **répit de 25 à 40 s**.

Les règles :

- Pas de nouvelle entité ni de phénomène pendant une détection, une poursuite ou le répit.
- Les coupures attendent la fin du répit.
- Au calme, une rencontre n'est possible qu'après 20 s.
- La phase s'affiche en mode développeur.

**Sons qui disent où est la menace.**

- Derrière un mur, le son est étouffé et plus faible (occlusion).
- De loin, il perd ses aigus et se noie dans la réverbération.
- Les sorties restent nettes : c'est un signal fiable.
- Réverbération propre au lieu :
  - courte et étouffée au Niveau 0 (moquette) ;
  - longue sur le béton ;
  - très longue dans les Poolrooms ;
  - presque nulle dehors.
- Les entités ont des **pas synchronisés sur leur foulée**, spatialisés, propres à chaque espèce :
  - Hound léger ;
  - Clump lourd ;
  - Wretch traînant ;
  - Skin-Stealer qui marche comme un explorateur.

**Jumpscares selon la situation.** Complets dans trois cas :

- pour un coup mortel ;
- pour la première frappe d'une espèce dans le niveau ;
- pour une frappe hors du champ de vision.

Les autres coups gardent la secousse, le son et la teinte. Le jumpscare ne devient pas une routine qui cache le modèle.

**Réglages de confort.** Les mécaniques ne changent pas.

- **Tremblements de la caméra** (0 à 100 %) : coups reçus et jumpscares. Le roulis est réduit aussi.
- **Flashs et clignotements** (normaux, atténués, aucun) :
  - éclairs des jumpscares ;
  - écran de mort ;
  - images noires du Wretch ;
  - neige du Faceling ;
  - néons qui clignotent (à « aucun », le néon défaillant ne s'éteint plus) ;
  - vacillement des coupures (à « aucun », un fondu régulier) ;
  - bandes du noclip.
- **Flou de mouvement**, désactivé par défaut.

**Premières minutes.**

- Quatre aides espacées, de 4 s à 52 s : déplacements, inventaire et interaction, caméscope et vision nocturne, se
  cacher.
- Elles affichent les touches du clavier ou les boutons de la manette, selon l'entrée utilisée en dernier.
- Elles sont montrées à un joueur qui a moins de 15 minutes de jeu, une fois par lancement.
- La première ronde de la Bacteria passe de 50 s à 110 s : elle ne chevauche plus la première coupure (75 s).

### 2.7 Test automatique (`BRAutoTest`, `BRAutoTestV47`)

`-BRAutoTestV47` lance seul le scénario de non-régression. Il est aussi inclus dans `-BRAutoTest`. Les sauvegardes du
test ont leur propre préfixe (`BR_AutoTest_`) : les parties du joueur ne sont pas touchées.

**Sauvegardes.**

- Migration d'un vrai fichier au format 1 : objets, notes, journal, niveaux, temps, morts, copie d'origine.
- Fichier illisible mis de côté, emplacement libéré, message.
- Copie de secours.
- 30 écritures de fond, la dernière relue.

**Reprise fidèle.** Au Niveau 0, graine 4605 :

1. ramasser une cassette, écrire, fermer, reprendre ;
2. vérifier : même graine, cassettes, cassette absente, moins de 2 m du point de reprise.

**Mort par blessure.**

- Vérifier : cause, relevable, mort notée dans la sauvegarde, session invalidée.
- Puis le réveil : Niveau 0, nouvelle graine.

**Noyade** au fond d'un bassin des Poolrooms. Vérifier :

- la cause noyade ;
- l'absence d'annonce de fosse ;
- que le joueur est relevable.

**Chute dans une fosse.** Le scénario v4.6 vérifie maintenant aussi la cause « chute », tenue par le serveur, non
relevable.

**Attaque.** Un Hound devant le joueur :

- **esquive** : s'éloigner pendant la préparation, aucun coup ;
- **coup reçu** : un seul coup, dans la vraie fenêtre (entre préparation + 0,04 s et préparation + 0,05 s + fenêtre +
  0,08 s).

**Streaming.** Sprint de 25 s au Niveau 1. Compter :

- les images sans sol sous le joueur ;
- les chunks en préparation ;
- les constructions forcées.

Mesure des images et des étapes.

**Graines 1, 9, 132, 191 et 4605** du Niveau 0 : sorties et cassettes chargées atteignables sans traverser de salle
de fosses.

**Réseau** (`-BRNetTest`).

- `-BRNetLag=<ms>` et `-BRNetLoss=<%>` simulent une latence et des pertes, sur chaque machine.
- L'hôte met le client à terre. Vérifier, des deux côtés : la cause et l'état relevable, puis la réanimation par
  l'hôte (événement « relevé »).

## 3. Vérifié ici, et comment

| Contrôle | Moyen | Résultat |
|---|---|---|
| Syntaxe C++ | `clang++ -std=c++20 -fsyntax-only`, en-têtes Unreal simplifiés, tous les fichiers sources | 19 fichiers sur 22 sans erreur ni avertissement, dont `BRAutoTest.cpp` et `BRMaterialBuilder.cpp`, jusqu'ici non couverts. Les 3 autres (`BRConfig`, `BRGameMode`, `BRWaterSim`, inchangés en v4.7) butent sur des lacunes des en-têtes simplifiés (`Docs/v47/verification_syntaxe.txt`). |
| Variables masquées, caractères non ASCII | script maison sur les sources | aucun problème |
| Rugosité | `Tools/generate_roughness.py --check` | 10 cartes identiques au script (écart 0/255) |
| Appui des pieds | `Tools/check_leg_ik.py` | 648 cas, écart maximal 0,0000 cm |
| Fenêtres d'esquive | `Tools/attack_timing.py` | tableau du § 2.3 (modèle, pas une mesure) |
| Voile au fond des fosses | `Tools/pit_fog_estimate.py` | 55/255 sans, environ 5/255 avec le post-traitement (estimation) |
| Génération du Niveau 0 (inchangée) | `Tools/verify_pitfalls.py --seeds 1-300` | 300 / 300 OK, 12 000 chemins d'entités, aucun au-dessus du vide (`Docs/v47/verification_300_graines.txt`) |
| Ressources protégées | `Tools/protect_assets.py check` | OK : aucun fichier protégé modifié ; les cartes `_R` sont de nouveaux fichiers dérivés |
| Script d'import | `python3 -m py_compile` | OK |

**Ce que ces contrôles ne valident pas :**

- la compilation par Unreal Header Tool et MSVC ;
- l'API exacte d'Unreal 5.8 (signatures, propriétés renommées) ;
- la compilation des matériaux et des shaders ;
- le comportement physique et réseau ;
- le rendu ;
- les performances.

## 4. Validations restantes (PC Windows, Unreal 5.8, carte compatible DirectX 12)

Les commandes supposent `UE=C:\Program Files\Epic Games\UE_5.8` et le projet dans `C:\Backrooms`.

### 4.1 Compilation et empaquetage

1. **Compiler** :
   ```bat
   "%UE%\Engine\Build\BatchFiles\Build.bat" BackroomsEditor Win64 Development -Project="C:\Backrooms\Backrooms.uproject" -WaitMutex
   ```
   Corriger les erreurs réellement rencontrées. Les points les plus exposés :
   - `UReverbEffect` (noms des propriétés) ;
   - `ISaveGameSystem`, non utilisé : les fichiers sont lus et écrits directement ;
   - `FWeightedBlendable` ;
   - `UMaterialExpressionSceneTexture` ;
   - `FMaterialParameterInfo` ;
   - `LineTraceSingleByObjectType` ;
   - `GetInputMouseDelta` ;
   - `Async(EAsyncExecution::ThreadPool, ...)` ;
   - les propriétés `LPF`, `ReverbSend` et `Occlusion` de `FSoundAttenuationSettings`.
2. **Copie propre.** Supprimer `Content/Backrooms/` et `Saved/`, ouvrir l'éditeur, laisser l'import Python se faire
   (`MATERIAL_VERSION 6`).
   - Vérifier que les dix `T_*_R` sont en sRGB désactivé.
   - Vérifier que `M_BR_World`, `M_BR_Mesh`, `M_BR_Skin`, `M_BR_WaterSurface` et `M_BR_PitShade` compilent sans
     erreur d'échantillonneur.
   - Le journal ne doit pas afficher « date d'une ancienne version ».
3. **Shaders.** Laisser compiler, puis contrôler le journal (`LogMaterial`, `LogShaderCompilers`).
4. **PIE**, puis **Standalone**.
5. **Version Windows empaquetée** :
   ```bat
   "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="C:\Backrooms\Backrooms.uproject" -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="C:\Backrooms\Build"
   ```
   Dans la version Shipping, vérifier :
   - que les touches de développement et les commandes de test sont refusées ;
   - que `IsDevMode` est faux.

### 4.2 Tests automatiques (captures et mesures dans Unreal)

- **Toujours les mêmes conditions** : `-BRSeed=4605`, 1920 × 1080, profil Qualité. Fermer les autres programmes.
- **Non-régression v4.7** :
  ```bat
  UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -BRAutoTest -BRAutoTestV47
  ```
  `Saved/AutoTest/Rapport.txt` ne doit lister aucun « PROBLEME ».
- **Test complet**, avec les captures des 12 niveaux et des 9 entités, et la salle de fosses :
  `-BRAutoTest -BRSeed=4605`, puis `-BRAutoTestPits -BRSeed=9` et `-BRAutoTestGPU`.
- **Coût des ombres des salles de fosses** : refaire `-BRAutoTestPits` avec `-BRPitShadows=half`, puis
  `-BRPitShadows=none`. Comparer les lignes `fosses_*` de `Mesures.csv`.
- **Matériaux de secours** construits en C++ : `-BRRuntimeMaterials`.

### 4.3 Réseau

**À 2 joueurs, sans dégradation**, lancer les deux commandes de la section 9 du README.

**Avec latence et pertes** : même chose, en ajoutant `-BRNetLag=150 -BRNetLoss=5` aux deux lignes.

**À 4 joueurs** :

1. lancer l'hôte, puis trois clients avec `-BRNetTest` ;
2. vérifier à la main :
   - attaque ;
   - noyade (Poolrooms) ;
   - chute (Niveau 0, graine 9) ;
   - réanimation ;
   - déconnexion d'un joueur à terre ;
   - changement de niveau pendant qu'un joueur est à terre.

### 4.4 À vérifier à l'œil

- **Reflets.**
  - Entités dans les reflets : flaques du parking, eau des Poolrooms.
  - En Qualité, les reflets s'éclairent par le cache de surfaces de Lumen. Ce cache ne couvre pas les maillages à
    squelette : les entités peuvent y paraître sombres.
  - En Cinématique (éclairage des reflets par les rayons), elles doivent être justes.
  - Si le défaut se confirme en Qualité, le réglage « Éclairage des reflets » corrige le problème. Il coûte cher.
- **Surface Cache** : vue `r.Lumen.Visualize.SurfaceCache`, fuites de lumière, bruit, traînées. Comparer avec Lumen
  logiciel.
- **Entités** :
  - appuis des pieds en mouvement, sur les trottoirs des Poolrooms et les marches ;
  - démarches ;
  - préparation lisible ;
  - émission du Smiler de près et de loin.
- **Fosses** : fond noir sans brouillard volumétrique, haut des parois toujours lisible.
- **Interfaces** : au clavier, à la souris et à la manette, en 16:9, 21:9 et 16:10.
- **Réglages** : flashs à « aucun » et tremblements à 0 %.

### 4.5 Performances

**Objectif : 60 images par seconde en Qualité.**

- Machine cible : Ryzen 5 5600X, GeForce RTX 3070, 16 Go.
- Sortie en 1920 × 1080, avec une résolution interne TSR à 80 % (1536 × 864).
- **Cet objectif n'est pas atteint tant qu'il n'est pas mesuré.**

À rapporter, depuis `Mesures.csv` et `Rapport.txt` :

- les temps CPU et GPU ;
- le 1 % le plus lent ;
- le pire temps de construction des chunks par image (sprint du Niveau 1) ;
- les constructions forcées.

### 4.6 Session de jeu continue

Au moins 30 minutes dans le moteur, du Niveau 0 vers le parking puis les Poolrooms. Noter :

- le rythme (répits, coupures, rondes) ;
- le confort ;
- la progression ;
- les premières minutes d'un nouveau joueur ;
- une fermeture et une reprise au milieu.

## 5. Mesures

**Aucune mesure dans le moteur.** Les seuls chiffres de ce rapport viennent des modèles et scripts de la section 3.

## 6. Fichiers modifiés ou ajoutés

| Fichier | Rôle |
|---|---|
| `Source/Backrooms/Public/BRTypes.h` | `EBRDeathCause` et règles, `FBRLightInfo` (déplacé), `RoughDetail`, `WallVariation`, réglages de confort |
| `BRCharacter.*` | état de mort répliqué, RPC vérifiées, cause, secousses réglables, jumpscares selon la situation, flou |
| `BRWorld.*` | délai de réanimation par cause, reprise de session, point de reprise sûr, chunks étalés, directeur de tension, réverbération, post-traitement des fosses, aides des premières minutes |
| `BRPlayerController.*` | commandes de test contrôlées, sauvegarde v2 et reprise, mort en attente, réglages de confort, manette |
| `BRSave.*` | format 2, migration, copie de secours, fichier illisible, écritures de fond ordonnées |
| `BREntity.*` | attaque en trois temps, démarches, appui de chaque pied, pas, ombres au loin, émission du Smiler |
| `BRChunk.*` | construction en étapes, peintures du parking, néons et coupures selon le réglage des flashs |
| `BRAssets.*`, `BRMaterialBuilder.*` | carte de rugosité, variation des murs, `M_BR_PitShade`, sons étouffés et réverbérés |
| `BRHUD.cpp`, `BRJumpscare.cpp`, `BRInteractables.cpp`, `BRLevels.cpp` | écran à terre selon la cause, flashs réglables, sorties nettes, réglages des niveaux |
| `BRAutoTest.*`, `BRAutoTestV47.cpp` | non-régression v4.7, réseau dégradé, mort et réanimation en réseau |
| `Content/Python/backrooms_setup.py` | `MATERIAL_VERSION 6`, import des `_R`, mêmes graphes que le C++, `M_BR_PitShade` |
| `RawAssets/Textures/*_R.png` | 10 cartes de rugosité (nouvelles ; les textures livrées sont intactes) |
| `Tools/generate_roughness.py`, `check_leg_ik.py`, `attack_timing.py`, `pit_fog_estimate.py` | production et contrôles hors moteur |
| `Docs/AUDIT_v4.7.md`, `Docs/RAPPORT_v4.7.md`, `Docs/v47/*` | audit, rapport, planche de rugosité, sorties des contrôles |

## 7. Limites connues

- **Rien n'est validé dans le moteur** (section 4).
- **Réseau.**
  - Les positions restent envoyées par les clients (choix v3.3). Le serveur vérifie les causes de mort et l'accès aux
    sorties avec ces positions : un client modifié peut encore mentir sur sa position.
  - Une esquive au tout dernier moment peut être annulée par la latence.
- **Sauvegarde.**
  - En partie en ligne, seul l'hôte retrouve sa position à la reprise. Les invités apparaissent au point de départ.
  - La phase de coupure en cours et les entités présentes ne sont pas sauvegardées : la reprise se fait lumières
    allumées.
- **Reprise.** Une partie v4.6 reprend avec une disposition neuve : aucune graine n'avait été enregistrée.
- **Salles de fosses.** Pas de variantes : la géométrie et les passages v4.6 sont gardés, pour ne pas fragiliser la
  génération vérifiée sur 300 graines.
- **Entités dans les reflets en Qualité** : limite connue du cache de surfaces de Lumen pour les maillages à
  squelette (§ 4.4).
- **Appui des pieds.** Il suppose une jambe presque tendue au moment de l'appui. Le sens des pivots reste à confirmer
  à l'écran.
- **Construction des chunks.** La planification d'un chunk reste d'un bloc. Le test mesure sa durée la plus longue.
