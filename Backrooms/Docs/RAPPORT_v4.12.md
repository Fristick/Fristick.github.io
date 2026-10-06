# Rapport v4.12 : stabilité, effets physiques des missions, publication progressive

Travail fait le 6 octobre 2026 sur la branche `backrooms`.

| | |
|---|---|
| **Commit de base** | `d9c47f95` (v4.11, 7/7), vérifié avant toute modification ([audit](AUDIT_v4.12.md)) |
| **Commits v4.12** | Voir la liste ci-dessous. Les sept premiers portent « /8 » ; la série est passée à neuf commits quand les tests du jeu ont été séparés de la livraison. |
| **Documents** | [Audit](AUDIT_v4.12.md) · [Gameplay : règles finales et effets physiques](GAMEPLAY_v4.12.md) · [Plan de publication](PLAN_PUBLICATION.md) · journaux réels dans [`Docs/v412/`](v412/) |

Commits v4.12 :

- `c6f1bab` : audit, reproduction ;
- `5319229` : transactions ;
- `5c8e5fa` : départ par échelle ;
- `881f80b` : lumière, Tick, placement ;
- `981eddd` : publication progressive ;
- `9df61e5` : effets physiques ;
- `543e3c2` : carnet, retours, langues ;
- `c3a6639` (8/9) : tests du jeu, journaux ;
- **9/9 (ce rapport, gameplay, README)** : commit final, le dernier de la branche (`git log -1`).

## 1. Où, comment, et ce qui n'a pas pu être fait

**Machine.** Conteneur Linux x86_64, 4 cœurs, **sans GPU**.

- **Unreal Engine absent** : ni éditeur, ni UHT/UBT/UAT, ni Xcode.
- Outils disponibles : g++ 13.3, clang 18.1, Python 3.11.
- Aucune machine Windows, Linux de bureau ou Mac ; aucune RTX 4080 ni Ryzen 7 7800X3D.
- La documentation de `dev.epicgames.com`, `partner.steamgames.com` et des guides NVIDIA est refusée par la politique
  réseau (403). **Rien n'a été contourné.**

Le **P0 du prompt** n'a donc pas pu être fait ici :

- compiler Editor et Shipping avec UHT, et en compilation regroupée ;
- cuire et lancer un paquet ;
- exécuter `-BRAutoTest -BRAutoTestStrict`, les suites V47 à V411, `-BRAutoTestV412` et `-BRNetTest`.

Ce qui suit distingue strictement ce qui a été exécuté de ce qui est préparé.

**Exécuté ici** (journaux réels dans [`Docs/v412/`](v412/), commandes au § 10) :

- reproduction des constats 4.1, 4.2, 4.3 et 4.6 sur des copies fidèles du code de la base : **4 sur 4 reproduits** ;
- cinq bancs C++ hors moteur, sur le code même du jeu (logique pure, partagée par le jeu et le banc) :
  - transactions : 51 vérifications ;
  - départ de groupe : 30 ;
  - lumière de gameplay : 6 ;
  - contenu par version : 67 ;
  - mécanismes physiques : 49 ;
  - tout est OK ;
- banc des missions : 3000 graines × 12 niveaux, OK ;
- tests des outils de paquet : 62 tests, 61 réussis, 1 sauté (propre à un hôte non Linux) ;
- langues :
  - extraction : 1118 clés ;
  - compilation : 0 traduction refusée, 22 langues à 1118/1118 ;
  - relecture ciblée gauche/droite, identifiants et unités : 0 problème ;
- empreintes des modèles protégés : OK ; aucun fichier de `RawAssets/` modifié depuis la base ;
- contrôle syntaxique du code Unreal (clang, en-têtes simplifiés), base et v4.12. Les mêmes 10 fichiers butent sur des
  API absentes des en-têtes simplifiés, avec les mêmes nombres d'erreurs. Les 5 nouveaux fichiers passent sans
  remarque. Aucun masquage de nom ; aucun nouveau conflit de compilation regroupée.

**Préparé, non exécuté** (aucune de ces étapes n'est comptée comme réussie) :

- compilation Editor et Shipping, cook, paquets Windows, Linux et macOS ;
- tous les tests du jeu : `-BRAutoTestV412` (nouveau, § 9), V47 à V411, fosses, `-BRNetTest`, `-BRSmokeTest` ;
- captures, mesures d'images, Unreal Insights, profilage GPU ;
- essais coop à quatre processus, latence et pertes simulées ;
- signature et notarisation Apple, essais Linux natifs ;
- Steam : aucun envoi ; `SetLive` vide, prévisualisation active.

**Catégories utilisées.**

- *Défaut reproduit* : observé ici sur la base.
- *Constat de source* : lu dans le code, reproduction en moteur impossible ici.
- *Corrigé, vérifié hors moteur* : banc exécuté ici sur le code du jeu.
- *Corrigé, test préparé* : code écrit, test du jeu écrit, rien de lancé dans Unreal.

## 2. Bugs prioritaires (§ 4)

| Point | Catégorie | Correction | Vérification |
|---|---|---|---|
| **4.1 Départ par une échelle** (P1) | **reproduit** : 5 échelles à conduit sur 8, grimpeur jamais « prêt » (écarts de 280 à 450 cm), une demande par image | `BRGatherLogic` (C++ pur). Une sortie a une forme : pied, direction, conduit et sommet d'une échelle. Un joueur est rassemblé **sur l'échelle** (colonne de 70 cm autour de la prise, à toute hauteur de la montée) ou **au même étage**, à 8 m et à vue du point de rassemblement. La tolérance verticale reste d'un étage : un joueur derrière un plafond n'est jamais compté. Demande et rassemblement visent le même point. Au sommet : une demande par arrivée, attente stable, plus de montée au-delà. L'initiateur annule en redescendant (nouvel appel au serveur) ; un refus ou une expiration (3 s) ne relance pas de demande ; redescendre d'un mètre réarme. Le départ répliqué porte la forme de la sortie et l'initiateur. | **corrigé, vérifié hors moteur** : `banc_depart.txt`, 30 OK (les 8 échelles, 2 et 4 joueurs, autre étage, attente, annulation, refus). **Test préparé** : `-BRAutoTestV412` (une demande au sommet, solo) ; `-BRNetTest` (hôte au sommet, compté, coéquipier qui arrive à pied, départ commun) |
| **4.2 Objet accepté puis perdu** (P1) | **reproduit** (capacité annoncée 4, lampe rangée pendant l'attente, bandage perdu) ; époque de vie : constat de source | `BRTxnLogic` (C++ pur), une seule règle pour vérifier, réserver et ranger. **Place réservée** pendant l'attente : un déplacement qui la prendrait est refusé. Un objet accepté finit toujours rangé, ou **mis de côté** (8 emplacements, sauvegardés, rangés dès qu'une place se libère). Accusé de réception : un joueur qui part avant rend l'objet au monde (nouvel identifiant, effets compensés, répliqué et sauvegardé). **Époque de l'inventaire** : une réponse d'une vie terminée ne touche pas la nouvelle. L'idempotence de la v4.11 est gardée (registre de l'hôte, 32 réponses mémorisées). | **corrigé, vérifié hors moteur** : `banc_transactions.txt`, 51 OK. 16 960 demandes avec déplacements au hasard : 548 objets acceptés sans place sans réservation, 0 avec. Le modèle hôte/joueurs couvre 2 et 4 joueurs, mort, transition, répétition, déconnexion, et 600 parties au hasard sans objet perdu ni doublé. **Test préparé** : `-BRAutoTestV412` (mis de côté, rangé, sauvegardé) |
| **4.3 Faux « inventaire plein »** (P2) | **reproduit** | La même fonction de place que l'ajout : piles, puis équipement libre compatible, puis cases. Plus deux algorithmes. | **corrigé, vérifié hors moteur** : ajout identique à la base sur 180 000 cas ; place annoncée = exemplaires rangés sur 36 000 ; faux « plein » : base 33, v4.12 0 ; neuf cas nommés (lampe main/ceinture, frontale, gilet, piles, vide). **Test préparé** : `-BRAutoTestV412` |
| **4.4 Lumières de mission** (P1) | constat de source | **Lumière de gameplay** commune (`BRLightLogic`, `ABRWorld::LightLevelAt`) : plafonniers plus sources de mission allumées, enregistrées sur chaque machine d'après l'état répliqué. Balises 9 m / 2 ; sortie de secours et porche 7 m / 1,6. Les sources sont autonomes (une coupure ne les éteint pas) ; même loi que les plafonniers, en 3D ; un mur arrête l'apport (une trace par source à portée, peu de sources). Smiler hors poursuite dissipé au-dessus de 0,5 (sous-titre) ; apparitions dans l'ombre et santé mentale suivent la même valeur. L'aide du Niveau 6 donne la portée réelle (« à moins de 4 m »), calculée avec la loi du jeu. | **corrigé, vérifié hors moteur** : `banc_lumiere.txt`, 6 OK (portée annoncée, au-delà de 9 m, somme de deux balises, coupure). **Test préparé** : `-BRAutoTestV412` (Niveau 6 : sources et lumière avant et après, pendant une coupure). Non couvert par un test : source derrière un mur, client tardif (à vérifier en jeu) |
| **4.5 Animation dans un chunk masqué** (P2) | constat de source ; coût non mesuré | Mécanisme masqué : Tick coupé. Une action reçue pendant le masquage ne relance pas le Tick. À la réapparition, le mécanisme reprend sa position sans rejouer l'animation. Les sorties masquées n'ont pas de Tick non plus. Compteurs de Ticks et de temps des mécanismes (`ABRMissionDevice::TickCount`, `TickSeconds`) pour les mesures. | **corrigé, test préparé** : `-BRAutoTestV412` (tous les mécanismes du Niveau 6 masqués : 0 Tick en 3 s). **Gain non mesuré** (aucun profilage possible ici ; les pointes RTX ne lui sont pas attribuées) |
| **4.6 Soin périmé** (P2) | **reproduit** (100 → 35) | **Révision de santé** commune aux coups, soins, morts, relevés et réveils (avant : un compteur pour les coups, un autre pour les soins, aucun pour le réveil). La quantité et la santé sont réconciliées séparément : l'objet n'est retiré que dans la même vie ; la santé n'est écrite que si aucun état plus récent n'a été reçu ; aucun effet n'est rejoué hors du niveau courant. Un envoi de santé du joueur parti avant un changement officiel est ignoré par l'hôte. | **corrigé, vérifié hors moteur** : scénario du constat (santé 100 gardée, objet neuf gardé), soin puis transition, soin puis mort et réveil, coup et soin rapprochés dans les deux ordres, renvoi. **Test préparé** : `-BRAutoTestV412` (envoi périmé ignoré par l'hôte) ; `-BRNetTest` v4.11 (soin périmé) |
| **4.7 Placement, restauration** (P2) | constat de source ; aucune graine défaillante trouvée | Trois placements : normal, variante de secours, **module de secours** près du départ (toujours possible). Plus de mission abandonnée ni de sorties ouvertes en silence. Le palier retenu est répliqué : les clients placent pareil. **Version de placement** (`PlaceVersion` 2) dans la sauvegarde. État non restaurable : la sauvegarde est **copiée avant réécriture** et les joueurs sont prévenus. Mécanismes replacés : progression gardée, avis. | **corrigé, test préparé** : `-BRAutoTestV412` (état d'une autre version : avis, copie identique ; placement d'une autre version : avis, indice lu gardé ; palier relevé pour chaque mission chargée). Banc des missions inchangé (3000 graines). Le placement dans le décor réel n'est pas essayé hors moteur |
| **4.8 Carnet** (P2) | constat de source | Chaque colonne a sa position et son défilement. Gauche : étapes, objets, aide, avec le bouton AIDE fixé en bas, un en-tête d'aide et un défilement automatique jusqu'à l'indice demandé. Droite : le message « aucune observation » part du haut de sa colonne. Rien n'est coupé, la taille du texte n'est pas réduite. | **corrigé, test préparé** : `-BRAutoTestV412` (défilement borné, aide de niveau 2 amenée à l'écran, captures en allemand et en arabe). Captures 720p, 1080p et écran large : à faire à la main |

## 3. Effets physiques des missions (§ 5)

Règles et cotes détaillées : [`GAMEPLAY_v4.12.md`](GAMEPLAY_v4.12.md), § 3.

| Point | Réalisation | Vérification |
|---|---|---|
| **Passerelle du Niveau 8** | Module construit dans le repère du mécanisme :<br>• palier de l'échelle (1,4 m) ;<br>• vraie interruption de 2,2 m ;<br>• appui de 0,9 m avec deux marches de 30 cm ;<br>• tablier portant (collision), animé par les treuils, relié au portique par deux câbles, mains courantes.<br>Sol, collisions, animation, chemin des entités (`ABRWorld::MissionNavDetour`, utilisé par `ABREntity::FollowPathTo`) et validation de sortie suivent le même état. Chute de 1,4 m au plus (aucune mort) ; contournement au sol des deux côtés. | **vérifié hors moteur** (`banc_mecanismes.txt`, E et F) : continuité palier/tablier/appui, pente 9°, palier inaccessible sans tablier, 500 graines des treuils, détours des entités. **Test préparé** : tablier levé → palier hors d'atteinte en marchant et sautant ; treuils faits en marchant ; tablier posé ; traversée à pied et échelle grimpée |
| **Bassins des Poolrooms** | **Eau locale** : volumes enregistrés par les mécanismes d'après l'état de mission. Nage, profondeur, immersion de la caméra, apnée, corps qui flotte et contrôle de noyade de l'hôte suivent la surface du volume (sinon l'eau du niveau).<br>Deux bassins identifiables (A haut, B bas) avec règles graduées (colonne et flotteur), ligne d'eau mobile, jet A → B, vidange, débordement, roues à aubes, bruit d'eau.<br>**Sas du passage sec** : déversoir, dallage, murets et garde-corps ; vidange visible ; passage réellement sec, sans plongée. Aucune entité. | **vérifié hors moteur** (`banc_mecanismes.txt`, A à D) : 3000 graines, 108 000 positions de vannes ; marque lue = marque de la mission ; débordement exactement quand la mission le signale ; résolution → passage ouvert 3000/3000 ; sas plein 1 m (sous la nage), vide sous le dallage, vidange 5,2 s ; marches, murets, entrée, échelle sous le plafond. **Test préparé** : surfaces = graduations avant et après ; eau du personnage = surface rendue ; sas vide ; traversée à pied sans nager ; aucune entité |
| **Autres niveaux** | Niveau 1 : voyant d'appel de l'ascenseur alimenté, sonnerie et sous-titre.<br>Niveau 2 : vapeur en voiles translucides animés (plus de sphères opaques).<br>Niveaux 6 et 9 : sources réelles et lumière de gameplay.<br>Toutes les sorties gardées : voyant rouge ou vert. | **test préparé** (captures du départ et de la sortie de chaque niveau du lot 1, voyants comptés) |
| **Non faits** | Niveaux 3 (défaut électrique localisable), 4 (archives), 5 (chaudière, serrures), 10 (moulin relié à sa commande) : réalisation inchangée. Ces niveaux ne sont pas publiés au lancement ; leur finition est rattachée à leur lot dans le [plan](PLAN_PUBLICATION.md). | — |
| **Correction trouvée en route** | Une échelle de mission posée sur un trottoir de 68 cm (Poolrooms) visait un sommet au-dessus du plafond (424 cm, tête à 512 > 450) : montée bloquée. La hauteur est maintenant mesurée depuis le pied, pour les missions et les chunks. | **reproduit puis vérifié hors moteur** (`banc_mecanismes.txt`, C) |

**Accès physique (§ 5, § 8).** Le test qui plaçait le joueur devant chaque mécanisme (v4.11) est complété par un test
**en déplacement réel** (`AddWalkMissionSteps`, `AddWalkExitSteps`). Le personnage n'est jamais déplacé directement :

- un chemin est calculé sur la grille avec l'A* des entités ;
- il est suivi avec les seules entrées du joueur : avancer, sauter, pas de côté, nouveau chemin si bloqué ;
- chaque mécanisme doit être visé par le trace d'interaction depuis la place atteinte, puis actionné ;
- la sortie est prise par la touche d'interaction ; on grimpe aux échelles en avançant ;
- l'arrivée est vérifiée.

Un déplacement de plus de 3 m en une image est compté comme une faute. Couverture :

- Niveaux 0, 1, 2 et 37 dans la version publiée simulée ;
- Niveau 8 ;
- Niveau 6, sans sa sortie.

**Préparé, non exécuté** : c'est le test le plus important à lancer, il dira si une mission publiée est bloquée par le
décor.

## 4. Direction visuelle, modèles et interface (§ 6)

**Fait.**

- **HUD.** Sous l'invite, une pastille discrète : « En attente de l'hôte… » (« Toujours sans réponse » après 5 s),
  « Accepté : … », « Refusé : raison ». Elle couvre ramassages, mécanismes, soins et départs. Aucune jauge n'est
  ajoutée : endurance et santé mentale restent dans l'inventaire.
- **Carnet** : voir 4.8.
- **Voyants des sorties gardées** : rouge ou vert.
- **Vapeur du Niveau 2** : voiles translucides.
- **Mécanismes des bassins et de la passerelle** : modules dédiés (épaisseurs, câbles, règles graduées, garde-corps).
  Ils sont construits en primitives, comme le reste des mécanismes.
- **Langues** : 49 nouveaux textes v4.12, traduits automatiquement dans les 21 autres langues et **marqués à relire**.
  Relecture ciblée outillée (`Tools/Localization/loc_review.py`, sur les 1118 textes de toutes les langues) :
  - côté gauche/droite : 147 vérifications ;
  - identifiants de vannes et d'appareils (A à E, V1 à V9), nombres : 2709 ;
  - unités : 231 ;
  - 20 nombres écrits en toutes lettres relus à la main ;
  - **0 problème**.

  Ce contrôle ne juge ni le style ni le sens général : **aucune traduction n'a été relue par une personne**.
- **Terminologie harmonisée** avec les textes existants (balise, Smiler en alphabet latin, crochets pleine chasse en
  chinois et japonais, accords en russe et en ukrainien).

**Non fait.**

- Poses, transitions, contact au sol et signaux des créatures ; combinaison hazmat en première et troisième personne,
  slots, visière, mains. Tout cela demande le moteur et des captures. Les modèles protégés n'ont pas été touchés.
- Matériaux et LOD des mécanismes revus en détail : nécessite l'éditeur.
- Scène jouable du dernier quai (facultative dans le prompt) : non faite. Le Niveau 11 n'est pas publié au lancement.
- Limite connue : la flèche « → » manque dans la police arabe pour d'anciens textes de développement (antérieurs à la
  v4.12) ; elle s'affiche en carré dans ces textes.

## 5. Performances (§ 7)

**Aucune mesure n'a été faite** : aucun GPU, aucun moteur. Ce rapport ne donne aucun chiffre d'images par seconde.

Les chiffres du rapport v4.10 (145 img/s au Niveau 0, 93 dans les Poolrooms, RTX fluide 1440p) restent ceux de ce
rapport, mesurés sur une autre version.

Préparé pour les mesures :

- les Ticks et le temps des mécanismes sont comptés (`ABRMissionDevice::TickCount`, `TickSeconds`) ;
- `-BRAutoTest` relève médiane, p95, p99, maximum, temps GPU, jeu et rendu, mémoire, chunks (`Mesures.csv`), avec
  `-BRSeed` pour des trajets identiques ;
- `-BRAutoTestSoak=<minutes>` fait la session longue.

À faire sur la machine de référence, dans l'ordre :

1. même paquet, mêmes graines (`-BRSeed=4242`), 1440p, profils RTX fluide, Qualité et Cinématique, Niveaux 0, 37, 10
   et 11 ;
2. Unreal Insights sur les pointes (chunks, chargements, PSO, scène RT, ombres, eau, nuages, GC) ;
3. session de 20 à 30 minutes ;
4. coop à quatre.

Le gain du Tick coupé (4.5) n'est pas annoncé tant qu'il n'est pas mesuré.

## 6. Coopération et sauvegardes : scénarios (§ 8)

| Scénario | Couverture v4.12 |
|---|---|
| Deux puis quatre processus, deux clients sur le même ramassage | **Hors moteur, exécuté** : modèle hôte et joueurs, 2 et 4 joueurs, une attribution, stocks cohérents. **Préparé** : `-BRNetTest` v4.11 « ramassage disputé » (hôte + 1 client). **Quatre processus : non automatisés** (`-BRNetTest` lance un hôte et un client), à faire à la main |
| Client qui rejoint une mission à moitié terminée | **Préparé** : `-BRNetTest` v4.11 (génération, empreinte du plan, révision, état). v4.12 : le palier de placement est répliqué. Monde physique (eau, tablier) du client : application de l'état reçu sans animation, non testée en jeu |
| Déconnexion du joueur qui a actionné un mécanisme ou pris une clé | Objets d'équipe et progrès tenus par l'hôte (v4.11). v4.12 : un objet attribué sans accusé de réception revient au monde (hors moteur : E7). Départ : l'initiateur parti annule (règle v4.11). **Non testé en jeu** |
| Inventaire modifié, mort ou transition pendant une réponse | **Hors moteur, exécuté** : E3, E3b, E4, E5, E8, E9, E12. **Préparé** : `-BRAutoTestV412` (mis de côté), `-BRNetTest` v4.11 (soin périmé) |
| Manivelle tenue par un joueur, un autre intervient | Cadence validée par l'hôte (v4.11). **Non couvert** par un test à deux joueurs |
| Porte, passerelle ou eau qui change pendant une reconnexion | L'état est appliqué sans animation à l'arrivée ou à la réapparition : surfaces posées tout de suite, tablier à sa position. **Non testé en jeu** |
| Chunk déchargé puis reconstruit en pleine mission | **Préparé** : Tick coupé, puis réapparition (`-BRAutoTestV412`, Niveau 6). Lumières : sources tenues par le monde, pas par le chunk |
| Sauvegarde v4.11 avant/après objet pris, porte ouverte ou bassin modifié | État de mission sauvegardé et restauré (v4.11) ; v4.12 : avis et copie si non restaurable, avis si placement changé (**préparé**). Bassins et sas : surface recalculée depuis l'état, pas sauvegardée à part |
| Ancienne session au format 1 à 3 | Inchangé depuis la v4.11 (**préparé** : `-BRAutoTestV411`, ancienne partie) |
| Latence, pertes simulées, requêtes répétées | **Hors moteur** : réponses répétées et renvoyées (E6, E11). En jeu : retour « en attente / sans réponse » (**préparé**). Latence et pertes réelles (`NetEmulation`) : **non préparées** |

## 7. Plateformes et Steam (§ 9)

Rien n'a été construit ni lancé sur Windows, Linux ou macOS dans cet environnement.

Les scripts de construction prennent maintenant un **canal de contenu** :

- `-Content internal` ou `BR_CONTENT_CHANNEL=internal` ;
- dossier `<Config>-internal` ;
- manifeste « contenu : » ;
- compilation propre si le canal change.

Tests de ces scripts : exécutés (62 tests, 61 réussis, 1 sauté).

`make_steam_vdf.py --content` refuse un paquet d'un autre canal ou sans canal. `SetLive` reste vide et la
prévisualisation active. La procédure Steamworks des branches n'a pas pu être relue (403).

La détection des capacités graphiques et les profils par plateforme sont inchangés depuis la v4.9 et la v4.10.

## 8. Publication progressive (§ 10)

Détail : [`PLAN_PUBLICATION.md`](PLAN_PUBLICATION.md).

**Configuration centrale** (`BRContentLogic`, C++ pur) : étape de chaque niveau (développé, test interne, publié), lots,
redirections du lot en cours.

**Lancement : chapitre 1, « Le Seuil »** : Niveaux 0, 1, 2, 37.

**Canal d'une version.** Celui d'une version Shipping est fixé à la compilation. En développement :
`-BRContent=public|internal|all`.

**Entrées qui passent par ces règles.**

- Menu et cartes de sauvegarde.
- Invites et usage des sorties, noclip des chunks.
- Sortie gardée par chaque mission, destinations au hasard.
- Départs de groupe, transitions validées par l'hôte.

**Fin du contenu disponible** au Niveau 2 : écran dédié, fin enregistrée dans la partie.

**Sauvegardes** : copie avant récupération, reprise au dernier niveau disponible exploré, rien n'est retiré.

**Réseau.**

- `?BRNet=412?BRContent=<signature>`, vérifiés dans `PreLogin` ;
- refus expliqué dans la langue du joueur, avec le « chapitre » de chacun ;
- `ProjectVersion=4.12.0.0`.

Vérifications :

- **hors moteur** : banc de contenu, 67 OK ;
- **préparé** : `-BRAutoTestV412` dans la version publiée simulée (menu, sorties, règle de connexion, reprise d'une
  partie v4.11, lot 1 parcouru à pied) ;
- **à faire à la main** : un vrai client d'un autre canal.

## 9. Tests du jeu ajoutés (préparés, non exécutés)

`-BRAutoTestV412` (seul) ou dans `-BRAutoTest` ; fichier `Source/Backrooms/Private/BRAutoTestV412.cpp`. Les sauvegardes
du test sont `BR_AutoTestV412_*`. L'inventaire, la langue, la partie active et le canal sont rétablis à la fin.

```bat
UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -BRAutoTest -BRAutoTestV412 -BRAutoTestStrict
```

| Étape | Vérifie |
|---|---|
| contenu : version publiée | 4 niveaux disponibles ; redirections 37 → 1, 1 → 2 ; Niveau 2 → fin du contenu ; refus de connexion lus ; reprise d'une partie v4.11 au Niveau 5 → Niveau 1 ; capture du menu |
| inventaire, santé | une seule règle de place ; objet accepté sans place mis de côté, rangé, sauvegardé ; envoi de santé périmé ignoré |
| Poolrooms (graine 4120) | surfaces = graduations, sas plein, aucune entité ; mission **en marchant** ; surfaces après, eau du personnage = surface rendue, sas vide ; sortie à pied par le sas (sans nager, profondeur maximale), échelle → Niveau 1 |
| Niveaux 0, 1, 2 (graine 4120) | mission en marchant, sortie à pied, arrivée (Niveau 1 ou 37 ; Niveau 2 ; fin du contenu) ; une seule demande au sommet d'une échelle |
| Niveau 8 (graine 4128) | tablier levé : palier hors d'atteinte malgré les sauts ; treuils en marchant ; tablier posé, navigation des entités ; traversée à pied, échelle → Niveau 9 |
| Niveau 6 (graine 4126) | balises : sources de gameplay et lumière avant/après, pendant une coupure ; mécanismes masqués : 0 Tick |
| reprise | état d'une autre version : avis 2, copie identique de la sauvegarde ; placement d'une autre version : avis 3, indice lu gardé |
| carnet | défilement borné, aide amenée à l'écran, captures en allemand et en arabe |
| bilan | placement retenu pour chaque mission (module de secours compté), sorties prises à pied (5 attendues) |

**`-BRNetTest`, v4.12** (au Niveau 37, hôte et client) :

- compatibilité : protocole et signature comparés entre les deux rapports ;
- pastille du client : « en attente » à l'envoi, « accepté » à la réponse, « refusé » avec la raison à 9 m ;
- départ par l'échelle du sas :
  - l'hôte grimpe et fait **une** demande en 4 s ;
  - il est compté comme rassemblé au sommet ;
  - le client arrive **à pied** ;
  - départ commun vers le Niveau 4 (version de développement).

## 10. Commandes et résultats (exécutés ici)

Depuis `Backrooms/`. Chaque journal donne la commande exacte, le compilateur, la date et le code de sortie.

| Commande | Résultat | Journal |
|---|---|---|
| `g++ … Docs/v412/repro_base_v411.cpp` (copies de la base) | 4 constats reproduits sur 4 | `v412/reproduction_base.txt` |
| `g++ … Tools/Transactions/test_transactions.cpp BRTxnLogic.cpp` | 51 vérifications, 0 échec | `v412/banc_transactions.txt` |
| `g++ … Tools/Departure/test_departure.cpp BRGatherLogic.cpp` | 30, 0 échec | `v412/banc_depart.txt` |
| `g++ … Tools/Light/test_light.cpp` | 6, 0 échec | `v412/banc_lumiere.txt` |
| `g++ … Tools/Content/test_content.cpp BRContentLogic.cpp BRMissionLogic.cpp` | 67, 0 échec | `v412/banc_contenu.txt` |
| `g++ … Tools/Mechanisms/test_mechanisms.cpp BRMechLogic.cpp BRMissionLogic.cpp` | 49, 0 échec | `v412/banc_mecanismes.txt` |
| `g++ … Tools/Missions/test_mission_logic.cpp …` puis `./test_missions` (3000) | RÉSULTAT OK, 36 000 graines | `v412/banc_missions.txt` |
| `python3 -m unittest Tools/Build/tests/test_packaging.py -v` | 62 tests, 61 OK, 1 sauté | `v412/outils.txt` |
| `python3 Tools/Localization/loc_extract.py` | 1118 clés ; catalogue source fr complété (1069 → 1118 entrées, voir la remarque) ; autres langues inchangées | `v412/outils.txt` |
| `python3 Tools/Localization/loc_build.py --check` | 22 langues à 1118/1118, 0 refusée | `v412/outils.txt` |
| `python3 Tools/Localization/loc_review.py` | 0 problème | `v412/relecture_langues.txt` |
| `python3 Tools/protect_assets.py check` | OK | `v412/outils.txt` |
| clang 18, contrôle syntaxique base et v4.12 | mêmes 10 fichiers et mêmes erreurs qu'à la base (API absentes des en-têtes simplifiés) ; 0 masquage | `v412/syntaxe.txt` |

**Remarque sur les outils de langue.** `loc_extract.py` réécrit toujours ses fichiers (pas de mode vérification). Lancé
pour ce rapport, il a montré qu'au commit 7/9 la liste des clés du jeu (`BRLocKeys.inl`) était restée à 1069 clés au
lieu de 1118 : le recensement en jeu des textes non traduits aurait ignoré les 49 nouvelles. Le catalogue source
français n'avait pas ses 49 nouvelles entrées non plus. Les deux sont régénérés au commit 8/9 ; les `.locres` reconstruits
sont identiques.

## 11. Limitations restantes

1. **Rien n'a été compilé ni joué dans Unreal.** Les erreurs propres à UHT, à UBT ou à la compilation regroupée réelle
   sont possibles malgré le contrôle syntaxique. Le premier essai à faire est la compilation, puis `-BRAutoTestV412`.
2. Le test en déplacement réel est écrit sans avoir vu le décor. Un échec signalera soit un vrai blocage (le but du
   test), soit une limite du suiveur de chemin : portes de mission fermées traversées par le chemin de grille, entités
   qui gênent. Le rapport du test dit lequel, avec cellule et captures.
3. Niveaux 3, 4, 5 et 10 : effets physiques inchangés (non publiés au lancement).
4. Créatures, combinaison, matériaux des mécanismes : aucune finition visuelle vérifiable ici.
5. Traductions v4.11 et v4.12 : relecture ciblée outillée seulement, aucune relecture humaine.
6. Aucune mesure de performance ; aucun essai à quatre processus, avec latence ou pertes simulées ; aucun client
   incompatible réel.
7. Plateformes, signature Apple, Steam : préparés, non exécutés ; documentation Epic, Steamworks et NVIDIA inaccessible
   (403).
8. Scène jouable du dernier quai : non faite.

## 12. Fichiers modifiés depuis `d9c47f95`

**Nouveaux, logique pure** (compilée par le jeu et par les bancs) :

- `Public/BRTxnLogic.h`, `Private/BRTxnLogic.cpp` : transactions ;
- `Public/BRGatherLogic.h`, `Private/BRGatherLogic.cpp` : départ de groupe ;
- `Public/BRLightLogic.h` : lumière de gameplay ;
- `Public/BRContentLogic.h`, `Private/BRContentLogic.cpp` : contenu par version ;
- `Public/BRMechLogic.h`, `Private/BRMechLogic.cpp` : eau locale, passerelle.

**Jeu** :

- `BRCharacter` : inventaire, réservation, mis de côté, époque, révision de santé, échelle, eau locale, état des
  demandes ;
- `BRWorld`, `BRMissionWorld` : ramassages rendus, lumière de gameplay, eau locale, navigation de la passerelle,
  placement en trois paliers, avis, fin du contenu, destinations ;
- `BRMissionDevice` : modules bassins, sas et passerelle, Tick masqué, vapeur ;
- `BRMission.h` ;
- `BRInteractables` : sorties condamnées, voyants, ascenseur, échelle mesurée depuis le pied ;
- `BRChunk` (échelles) ;
- `BREntity` (détour, lumière) ;
- `BRHUD` (carnet, pastille d'état, sauvegardes) ;
- `BRPlayerController` (refus réseau, reprise d'un niveau indisponible) ;
- `BRGameMode` (PreLogin) ;
- `BRLevels` (disponibilité, connexion) ;
- `BRSave` (copie avant récupération, nouveaux champs) ;
- `BRItems`, `BRMissionText`, `BRMissionLogic.h` (version de placement), `BRTypes.h` ;
- `Backrooms.Build.cs` (canal compilé) ;
- `Config/DefaultGame.ini` (`ProjectVersion`).

**Tests du jeu** : `BRAutoTestV412.cpp` (nouveau), `BRAutoTest.cpp/.h`, `BRAutoTestV411.cpp` (une ligne).

**Bancs et outils** :

- `Tools/Transactions`, `Tools/Departure`, `Tools/Light`, `Tools/Content`, `Tools/Mechanisms` (nouveaux) ;
- `Tools/Localization/loc_review.py` (nouveau) ;
- `Tools/Build/*` (canal de contenu), `Tools/Build/tests/test_packaging.py` ;
- `Tools/Steam/make_steam_vdf.py`.

**Langues** : `Content/Localization/Game/*/Game.po` et `.locres` (22 langues), `BRLocKeys.inl`,
`Docs/LOCALISATION_COUVERTURE.md`.

**Documents** :

- `Docs/AUDIT_v4.12.md`, `Docs/RAPPORT_v4.12.md`, `Docs/GAMEPLAY_v4.12.md`, `Docs/PLAN_PUBLICATION.md` ;
- `Docs/v412/` (journaux, reproduction) ;
- `README.md`.

**Non modifiés** : `RawAssets/` (modèles protégés) et les originaux.

## 13. Essais restant à exécuter, dans l'ordre

1. Compiler `BackroomsEditor Win64 Development`, puis `Backrooms Shipping` (Windows, puis Linux et Mac), unités
   regroupées comprises ; corriger les erreurs réelles.
2. `-BRAutoTest -BRAutoTestV412 -BRAutoTestStrict` ; lire `Saved/AutoTest/Rapport.txt` et les captures `v412_*`.
3. `-BRAutoTest -BRAutoTestStrict` complet (non-régression V47 à V411, fosses).
4. `-BRNetTest` (hôte avec `?listen`, client sur la même machine), puis à la main :
   - quatre joueurs ;
   - un client d'un autre canal ;
   - latence et pertes (`NetEmulation`).
5. Version publiée réelle (`-Content public`) : menu, fin du contenu, refus d'un client de test interne.
6. Mesures (§ 5) et session longue.
7. Paquets natifs Linux et macOS, signature et notarisation ; préparation Steam, sans publication.
