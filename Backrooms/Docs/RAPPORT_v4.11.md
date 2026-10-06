# Rapport v4.11 : missions des douze niveaux, transactions coop, créatures lisibles

Travail fait le 6 octobre 2026 sur la branche `backrooms`.

| | |
|---|---|
| **Commit de base** | `3e1f55b` (v4.10, 3/3), vérifié avant toute modification ([audit](AUDIT_v4.11.md)) |
| **Commits v4.11** | `98c1407` (1/7 outils) · `c7b8d70` (2/7 transactions et missions) · `cb251c2` (3/7 créatures) · `a31ea23` (4/7 finition, traductions) · `489e5d7` (5/7 solveur partagé, tests, journaux) · `35238a4` (6/7 tests réseau v4.10 adaptés, audit, gameplay, propositions) · **7/7 (ce rapport, README) : commit final**, le dernier de la branche (`git log -1`) |
| **Documents** | [Audit](AUDIT_v4.11.md) · [Gameplay : règles des niveaux et des entités](GAMEPLAY_v4.11.md) · [Propositions « Level ! » et « Level Fun »](PROPOSITIONS_NIVEAUX_v4.11.md) · journaux dans [`Docs/v411/`](v411/) |

## 1. Où, comment, et ce qui n'a pas pu être fait

**Machine** : conteneur Linux x86_64, sans GPU. **Unreal Engine absent** (ni éditeur, ni UAT, ni Xcode). La documentation
d'Epic, de Steamworks et de NVIDIA est refusée par la politique réseau (403) : rien n'a été contourné.

**Exécuté ici** (journaux réels dans `Docs/v411/`) :

- banc hors moteur des missions (C++ pur, g++ 13) : 3000 graines × 12 niveaux, solveur aux seules informations visibles,
  actions au hasard, erreurs typiques, sérialisation, mode joueur rejoué ;
- reproductions des constats 2.3 à 2.5 sur la base et sur la v4.11 ; tests unitaires des outils de livraison (Linux) ;
- vérification syntaxique du code Unreal (clang 18 avec des en-têtes simplifiés), base et v4.11 ;
- localisation (`loc_build.py --check`), glyphes des polices CJK et arabe (fontTools) ;
- empreintes des modèles protégés (`protect_assets.py check`).

**Préparé, non exécuté** (aucune de ces étapes n'est comptée comme réussie) :

- compilation `BackroomsEditor` et `Backrooms Shipping`, cook, paquets Windows, Linux et macOS ;
- tests du jeu : nouvelles étapes v4.11 et non-régression V47, V48, V49, V410, fosses, `-BRNetTest` à deux et quatre
  joueurs, `-BRSmokeTest` ;
- captures d'écran, images par seconde, mémoire, profilage des pointes des Poolrooms (Unreal Insights) ;
- signature et notarisation Apple, essais Linux natifs, Steam (aucun envoi, `SetLive` vide, comme demandé).

**Catégories** : *défaut reproduit* (observé ici sur la base) ; *constat de source* (lu dans le code, reproduction en
moteur impossible ici) ; *corrigé, vérifié hors moteur* (test exécuté ici) ; *corrigé, test préparé* (code écrit, test
du jeu écrit, rien de lancé dans Unreal).

## 2. Bugs du § 2

| Point | Catégorie | Correction | Vérification |
|---|---|---|---|
| **2.1 Ramassage coop** | constat de source (`BRInteractables.cpp:184` à la base) | Le client **demande** l'objet par son identifiant stable, avec un numéro de requête et le numéro du niveau. L'hôte vérifie : niveau courant et pas de transition, joueur vivant et chargé, objet encore disponible, existence et type, distance 3D depuis les yeux (pas à l'étage au-dessus), ligne de vue, place libre. Une seule opération attribue l'objet, le retire pour tous, crédite le stock de soin et fait avancer le niveau. Le client ne change son inventaire **qu'à l'acceptation**, une seule fois ; une réponse répétée ou d'un ancien niveau est ignorée. Les RPC `ServerVHSCollected` et `ServerMarkCollected` sont supprimés. Fusibles et clés des missions suivent la même règle (objets d'équipe, un seul preneur). | corrigé, test préparé : `-BRAutoTestV411` (deux demandes dans la même image, objet à 12 m) ; `-BRNetTest` (« ramassage disputé » : un seul gagnant) |
| **2.1 Stock de soins** | constat de source (`BRCharacter.cpp:1281`) | Une seule déclaration par session (celle de l'arrivée), plafonnée à ce que l'inventaire peut contenir ; une déclaration répétée ou en retard est ignorée. Un soin est **refusé** tant que le stock n'est pas connu (raison 6). Ensuite, seuls les ramassages et soins acceptés par l'hôte changent ce stock. | corrigé, test préparé ; le test réseau v4.10, qui redéclarait le stock, est adapté (l'hôte crédite les bandages ajoutés pour le test, comme des ramassages acceptés ; ses assertions sont inchangées) |
| **2.2 Réponses de soin** | constat de source (`BRCharacter.cpp:1219`) | Les réponses appliquées sont mémorisées (32 dernières) : une réponse déjà reçue n'est jamais réappliquée. Une réponse d'un autre niveau ne fait que réconcilier la quantité. La santé officielle n'est reprise que si ce soin est plus récent que le dernier coup ou soin reçu. Demande faite dans un autre niveau : refusée par l'hôte, rien n'est consommé (raison 7). | corrigé, test préparé : `-BRNetTest` (« soin périmé » ; et les deux tests v4.10 : soins rapprochés, soin pendant une série de coups) |
| **2.3 Fixture POSIX** | **défaut reproduit** : base 28 tests, 26 réussis, 1 échec, 1 sauté | La fixture pose les droits d'exécution sur POSIX ; un cas distinct vérifie le refus d'un paquet Linux sans droits. | **corrigé, vérifié** : 59 tests, 58 réussis, 1 sauté (propre à un hôte non Linux). Windows : préparé, non exécuté. Journaux `tests_outils*.txt` |
| **2.4 Lancement** | **défaut reproduit** (3 cas, journal `reproduction_outils.txt`) | Un lancement demandé qui échoue fait échouer la commande (code 1). Un lancement obligatoire incomplet rend « INSPECTION INCOMPLÈTE » (code 2). Le rapport est vérifié : schéma, état, problèmes, vérifications non faites, cohérence avec le code de sortie, identifiant de l'essai. `{}` ou un rapport d'un autre essai sont refusés. Le jeu est lancé avec `-BRAutoTestStrict`. | **corrigé, vérifié** : codes 1, 2, 1 au lieu de 0, 0, 0 |
| **2.5 Binaires tronqués** | **défaut reproduit** : `struct.error` sur un PE de 68 octets | Lectures bornées pour PE, ELF et Mach-O (offsets, longueurs, sections, commandes) ; un fichier invalide donne ÉCHEC avec son explication ; filet de sécurité autour de chaque vérification. | **corrigé, vérifié** : « lecture hors du fichier (offset 64, 24 octets, taille 68) » ; tests des trois formats tronqués |

## 3. Missions des douze niveaux

### 3.1 Système

- **Logique pure** (`BRMissionLogic.h/.cpp`, sans moteur) : plan par niveau (mécanismes, rôles, inscriptions, zones,
  étapes, paramètres tirés), état (un octet par mécanisme, mission résolue, erreurs), actions (lire, prendre, insérer,
  basculer, appuyer, maintenir), évaluation (étapes masquées, disponibles, en cours, faites ; ouverture des éléments
  visibles ; avertissement), sorties gardées. Identifiants stables : rôles, indices de mécanisme, retours.
- **Monde** (`BRMissionWorld.cpp`) : l'hôte tire le plan avec la graine du niveau et le place **avant** la construction des
  chunks, dans une zone finie atteignable (parcours en largeur limité à 6000 cellules ou 95 m, bandes de zones, salle de
  mission réservée, aucun objet requis derrière ce qu'il ouvre). Niveaux infinis : sortie garantie près de la salle de
  mission. Graine sans plan valide : variante de secours déterministe, avant de placer les joueurs.
- **Réseau** : l'état est tenu par l'hôte et répliqué (`FBRNetMission`) ; un client qui rejoint reçoit l'état courant
  avant les mises à jour suivantes. Chaque action est une demande numérotée, validée chez l'hôte : distance 3D, ligne de
  vue, joueur vivant et chargé, cadence des actions maintenues (une unité par 0,4 s), réarmement d'un bouton refusé.
  Le client n'annonce jamais « mission terminée ».
- **Chunks** : les mécanismes sont des acteurs du monde, pas du chunk ; un chunk déchargé les masque sans toucher à leur
  état. Pas de Tick permanent : seul un mécanisme animé proche d'un joueur s'anime.
- **Sauvegarde, format 4** : version de génération de la session, état de mission (lié au plan par une empreinte),
  documents trouvés ; campagne (fragments de route, objectifs facultatifs, fins vues). CRC, copie de secours, file
  d'écriture et refus des formats futurs inchangés.
- **Anciennes parties** : une session au format 1 à 3 garde son ancien mode (cassettes VHS et enregistrement du
  Niveau 0, sorties libres) **jusqu'à la sortie de son niveau** ; le niveau suivant a sa mission. Rien n'est réinitialisé.
- **Sortie de groupe** : une sortie ouverte lance un départ annoncé ; rassemblement à 8 m, même étage, à vue ; joueur à
  terre emmené ; joueur en chargement non attendu ; annulation (90 s, initiateur parti à plus de 25 m, plus personne
  debout). La validation 2D à 6 m est retirée (`ServerRequestTransition` réservé aux commandes de développement).
- **Fin** : le Niveau 11 mène au dernier quai (fin principale, variante avec trois objectifs facultatifs) ; la porte
  vers un niveau tiré au hasard reste une route annexe.

### 3.2 Les missions

Règles complètes, variantes et menaces : [`GAMEPLAY_v4.11.md`](GAMEPLAY_v4.11.md). En bref :

| Niveau | Mission | Changement visible |
|---|---|---|
| 0 | Néons anormaux observés (rythme, symbole), panneau de trois cadrans, levier d'aiguillage (Niveau 1 ou Poolrooms) | passage stabilisé, route choisie ouverte |
| 1 | Schéma, fusibles marqués, puissance limitée à deux circuits, appel de l'ascenseur ; réserve éclairée en option | courant rétabli, ascenseur, porte de service |
| 2 | Plaque des pressions, conduites des manomètres, vannes (fuite réversible) | vapeur dissipée devant la porte |
| 3 | Tableau de charge, boîte en défaut, relais des secteurs sains (disjonction réversible) | ascenseur alimenté |
| 4 | Planning + annuaire des archives, code à trois molettes | grille de l'hôtel levée |
| 5 | Registre, trousseau de trois clés, serrures, consigne de la chaudière | passage de la chaufferie |
| 6 | Plaque en relief, chaîne de balises (bruyantes, une leurre), alimentation de secours | sortie éclairée |
| 8 | Marques gravées, treuils dans l'ordre (deux grippés, progression gardée) | passerelle descendue |
| 9 | Trois plans de maisons, boîtier de rue, maison à deux traits | porche allumé, porte entrouverte |
| 10 | Marque de clôture, tableau des granges, orientation du moulin, frein | porte de la grange |
| 11 | Générateur, chiffres rapportés ou lus en ville, destination, confirmation | portique vers le dernier quai |
| 37 | Marques de niveau, sens du courant, deux vannes (débordement réversible) | passage sec découvert ; aucune entité |

### 3.3 Résultats du banc (exécuté)

`Docs/v411/banc_missions_3000.txt` (0,8 s, code de sortie 0) :

| Niveau | Résolues (solveur) | Résolues après actions au hasard | Variantes distinctes | Mode joueur rejoué | Actions (mode joueur, moy./max) |
|---|---|---|---|---|---|
| 0 | 3000/3000 | 3000/3000 | 1359 | 3000/3000 | 26,5 / 35 |
| 1 | 3000/3000 | 3000/3000 | 120 | 3000/3000 | 9,0 / 9 |
| 2 | 3000/3000 | 3000/3000 | 384 | 3000/3000 | 14,5 / 19 |
| 3 | 3000/3000 | 3000/3000 | 96 | 3000/3000 | 13,0 / 13 |
| 4 | 3000/3000 | 3000/3000 | 3000 | 3000/3000 | 16,5 / 30 |
| 5 | 3000/3000 | 3000/3000 | 3000 | 3000/3000 | 11,0 / 13 |
| 6 | 3000/3000 | 3000/3000 | 2536 | 3000/3000 | 11,0 / 11 |
| 8 | 3000/3000 | 3000/3000 | 120 | 3000/3000 | 13,0 / 13 |
| 9 | 3000/3000 | 3000/3000 | 288 | 3000/3000 | 6,5 / 8 |
| 10 | 3000/3000 | 3000/3000 | 2969 | 3000/3000 | 9,0 / 12 |
| 11 | 3000/3000 | 3000/3000 | 952 | 3000/3000 | 25,3 / 39 |
| 37 | 3000/3000 | 3000/3000 | 40 | 3000/3000 | 8,8 / 13 |

Le solveur n'utilise **que** ce que le joueur voit (inscriptions, indices lus, observations, jauges, retours), jamais
les paramètres tirés. Le « mode joueur » ne se sert que des actions du jeu (un appui passe à la position suivante) ; ses
actions enregistrées se rejouent à l'identique sur un état neuf. Après 0 à 80 actions au hasard (1 415 847 en tout),
la mission reste résoluble : aucune impasse. Sont aussi vérifiés : 4 935 erreurs typiques et leur retour, la
sérialisation (blob d'une autre graine, tronqué ou d'une autre version refusé), la variante de secours de chaque niveau,
les fragments de campagne au Niveau 11, un fusible attribué une seule fois.

**Ce que le banc ne prouve pas** : le placement réel dans le décor, l'accessibilité physique, le rendu des mécanismes et
le réseau. C'est le rôle des tests du jeu (§ 7.2), préparés, non exécutés.

## 4. Créatures

| Point | Changement (`BREntity.cpp`, `BRMissionWorld.cpp`) |
|---|---|
| Choix de la cible | Perception d'abord : vu (×1), entendu seulement (×3), ni l'un ni l'autre (×10), caché (×2), sur le carré de la distance ; cible perçue depuis moins de 6 s gardée ; changement après 2 s minimum et pour une cible au moins deux fois mieux placée. `ChooseTarget` est testable seul (7 cas dans `-BRAutoTestV411`). |
| Bruits | Atténués de près de moitié sans ligne de vue, localisés avec une erreur ; la voix des joueurs n'est pas un bruit. Les mécanismes font du bruit (manivelle 10 m, balise 18 m, relais 12 m, disjonction 16 m). |
| Wretch | Chasse nerveuse aux bruits, recherche courte ; après 9 s de course, pause haletante de 3,5 s ; un bruit récent ailleurs peut détourner sa recherche. Ne partage plus `ThinkSimpleHunter` avec le Clump. |
| Clump | Garde un passage autour de son poste (11 m), charge courte, retour au poste ; s'écarte des machines alimentées. |
| Smiler | Faisceau braqué : crispation répliquée (tremblement, sourire qui s'embrase, sifflement) dès 0,25 s, charge à 1,15 s. La règle ne dépend pas de la graine. |
| Hound | Grondement tête basse à la traque ; pas de morsure avant 1,5 s de poursuite. |
| Skin-Stealer | S'arrête net quand on lui fait signe à la lampe (indice observable, en plus de l'absence de lampe et de la démarche). |
| Faceling | Neutre sauf provocation ; la variante hostile prévient (se fige, crie) avant de charger. |
| Deathmoth | Détourné par l'éclairage du décor. |
| Budget de menace | Créatures (1, 2 en poursuite), coupure (2), joueur dans une salle de fosses (1) : au-delà de 4 (5 à plus de deux joueurs), aucune nouvelle rencontre. Pas d'apparition dans le champ de vision à moins de 25 m ni sur la route d'une sortie ouverte. |

Les modèles et squelettes ne changent pas (aucun fichier protégé touché ; `protect_assets.py check` : OK). Les signes
passent par les animations et matériaux existants (lueur, tremblement, posture).

## 5. Finition

- **Carnet** (onglet de l'inventaire) : étapes, objets de l'équipe, observations, objectif facultatif ; **aide
  progressive** (rien, indice, rappel des indices trouvés) ; documents lus recopiés.
- **Sous-titres** des sons utiles, avec la direction ; **volumes séparés** des effets et des voix (le volume principal de
  l'appareil porte général × voix, les sons du jeu effets ÷ voix : chacun obtient exactement général × son volume).
- **Niveau 10** : soleil bas et chaud, brouillard chaud, nuages volumétriques (profil Qualité et au-delà), ciel plus
  lumineux. **Niveau 11** : lampadaires plus nombreux (50 %, 5200 lm, 15 m), soleil plus haut, exposition relevée.
- **Mécanismes** : plaques, poignées, cadrans gradués, lampes d'état, étiquettes universelles, pièces mobiles ; onze
  sons générés (`Tools/generate_mission_sounds.py`, `RawAssets/Sounds/S_M_*.wav`), dans l'ensemble préchargé commun.
- **Traductions** : 337 nouveaux textes dans les 21 langues (1069/1069 partout), marqués à relire (traduction
  automatique, **non relue par un traducteur**). Libellés M1-M3 et V1-V3 identiques aux inscriptions des mécanismes dans
  toutes les langues. Glyphes vérifiés : aucun caractère CJK ou arabe sans glyphe.

## 6. Performances et plateformes

**Aucune mesure** : sans Unreal ni GPU ici, aucun chiffre d'images par seconde, de mémoire ou de temps d'image n'est
donné. Les pointes des Poolrooms signalées par le rapport v4.10 (35,7 ms) **n'ont pas été profilées**. Choix faits pour
limiter le coût : mécanismes sans Tick permanent, actions événementielles, état d'un octet par mécanisme, pièces en
maillages simples partagés, une seule lumière dynamique par balise du Niveau 6 (quatre au plus, sans ombre), sons dans
l'ensemble commun déjà préchargé, `/Game/Backrooms` toujours cuit.

**Mac** : `mac_sign_order.py` trouve tout le code imbriqué par son contenu et signe de l'intérieur vers l'extérieur ;
`build_mac.sh` l'utilise et vérifie en profondeur. **Préparé, non exécuté** (aucun Mac). Linux natif : préparé, non
exécuté. Steam : rien n'a changé (aperçu, `SetLive` vide, aucun envoi).

## 7. Tests

### 7.1 Exécutés ici

| Test | Commande | Résultat | Journal |
|---|---|---|---|
| Banc des missions | voir § 8 | OK, code 0 | `v411/banc_missions_3000.txt` |
| Outils de livraison | `python3 -m unittest discover -s Tools/Build/tests -v` | 59 tests : 58 OK, 1 sauté | `v411/tests_outils.txt` |
| Base v4.10, mêmes outils | idem sur `3e1f55b` | 28 tests : 1 échec, 1 sauté | `v411/tests_outils_base_v410.txt` |
| Constats 2.4 et 2.5 | `python3 Docs/v411/repro_outils.py <check_package.py> <tests>` | base : codes 0/0/0 et exception ; v4.11 : 1/2/1 et ÉCHEC expliqué | `v411/reproduction_outils.txt` |
| Syntaxe du code Unreal | clang 18 + en-têtes simplifiés | les 10 fichiers en échec le sont aussi sur la base (lacunes des en-têtes simplifiés) ; tous les fichiers v4.11 sans erreur ni avertissement ; variables masquées : 0 | `v411/verification_syntaxe.txt` |
| Localisation | `python3 Tools/Localization/loc_build.py --check` | 22 × 1069/1069, code 0 ; glyphes manquants : 0 | `v411/localisation.txt` |
| Modèles protégés | `python3 Tools/protect_assets.py check` | OK ; manifeste inchangé | `v411/ressources_protegees.txt` |

### 7.2 Préparés, non exécutés (tests du jeu)

`-BRAutoTestV411` (inclus dans `-BRAutoTest`), fichier `BRAutoTestV411.cpp` :

- **missions des douze niveaux** (graine 4111) : sortie de progression verrouillée avant ; solution du solveur partagé
  (`BRMissionSolver`, le même que le banc) rejouée **par de vraies demandes** : joueur placé devant chaque mécanisme
  (sol, capsule libre, ligne de vue), attente du chunk, demande d'interaction, validation de l'hôte, retour comparé à
  celui attendu ; sortie ouverte après ; départ (Niveau 2) ; fin de campagne (Niveau 11) ; aucune commande ne coche
  d'étape ;
- mécanisme demandé à 8 m : refusé, rien ne change ;
- ramassage : deux demandes dans la même image (un objet, une acceptation) ; objet à 12 m refusé ;
- **ancienne partie au format 3** : migrée, reprise en ancien mode (cassettes), carnet, niveau suivant avec mission,
  version de génération 2 écrite dans la sauvegarde ;
- carnet et aide (trois niveaux) ; sous-titres (direction, désactivation) ; volumes (appareil et sons du jeu) ;
- choix de cible (7 cas) ; bruit entendu à 5 m, pas à 50 m ; budget de menace.

`-BRNetTest` : état de mission à la connexion (empreinte du plan, révision, CRC de l'état, dans les deux rapports) ;
lecture d'un indice par le client, validée par l'hôte et répliquée ; ramassage disputé (une seule acceptation) ; soin
périmé refusé sans rien consommer ; départ de groupe (coéquipier attendu, puis départ commun).

**Non-régression** : V47, V48, V49, V410 et fosses restent dans `-BRAutoTest`. Deux adaptations au nouveau design,
sans retirer d'assertion : V47 place le joueur à côté de la cassette avant de la ramasser (l'hôte vérifie la
distance) ; V410 (réseau) fait créditer par l'hôte les bandages ajoutés pour le test (le stock ne se redéclare plus).

**Non couvert** même en préparation : quatre joueurs, départ d'un joueur au milieu d'une étape, inventaire plein et
mort pendant un ramassage disputé, déconnexion pendant un soin. Ces cas sont traités par le code (objets d'équipe, refus
« plein », joueur mort refusé, réponses périmées ignorées) mais sans test dédié.

## 8. Commandes

```
# Banc des missions (depuis Backrooms/)
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Missions/test_mission_logic.cpp \
    Source/Backrooms/Private/BRMissionLogic.cpp Source/Backrooms/Private/BRMissionSolver.cpp -o test_missions
./test_missions 3000

# Outils de livraison
python3 -m unittest discover -s Tools/Build/tests -v

# Localisation
python3 Tools/Localization/loc_build.py --check

# Tests du jeu (préparés) : Windows, depuis le dossier du projet
UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -BRAutoTest -BRAutoTestV411 -BRAutoTestStrict
UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -BRAutoTest -BRAutoTestStrict -BRSeed=4242
# Réseau : hôte (carte ouverte avec ?listen) et client, chacun avec -BRNetTest (README, § 9)
```

## 9. Contraintes respectées

| Contrainte | État |
|---|---|
| Aucune barre de vie ; endurance et santé mentale dans l'inventaire (Tab, reconfigurable) | inchangé |
| Neuf entités, douze niveaux (0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 37) | conservés ; migration explicite des sauvegardes |
| Modèles protégés | `protect_assets.py check` : OK ; aucun fichier de `RawAssets` modifié ou supprimé (ajouts : sons `S_M_*`) |
| Pitfalls du Niveau 0, zone de 112 m, déterminisme | inchangés ; les mécanismes évitent les fosses et les cellules réservées |
| Poolrooms sans entités | mission sans créature ; budget de menace sans effet |
| Caméscope dans le sac | les observations se font touche maintenue, sans le sortir |
| Solo et coop à quatre, IP directe, 22 langues, micro facultatif | aucune énigme ne demande deux joueurs ni le micro |
| Trois modes de fenêtre, Windows/Linux/macOS, aucun envoi Steam | inchangés |

## 10. Limites et ce qui reste à vérifier

1. **Rien n'a été compilé ni lancé dans Unreal.** Le code est vérifié par clang avec des en-têtes simplifiés, qui ne
   remplacent pas UHT ni le compilateur du moteur. Première étape : compiler Editor et Shipping, lancer
   `-BRAutoTest -BRAutoTestStrict`, puis `-BRNetTest`.
2. Placement des mécanismes dans le décor réel (accès, collisions, lisibilité), rendu des mécanismes, lisibilité des
   inscriptions à 720p : non vus.
3. Équilibrage : durée des missions, bruit des mécanismes, budget de menace, fenêtres de réaction des créatures.
4. Coop à quatre, départ d'un joueur au milieu d'une étape, client rejoignant une mission avancée : préparés en partie
   (§ 7.2), non exécutés.
5. Traductions automatiques non relues (337 textes × 21 langues).
6. Mesures : aucune ; pointes des Poolrooms non profilées.
7. Linux, macOS (signature, notarisation), Steam : préparés, non exécutés.

## 11. Fichiers modifiés depuis `3e1f55b`

**Nouveaux** : `Public/BRMission.h`, `Public/BRMissionLogic.h`, `Public/BRMissionSolver.h`,
`Private/BRMissionDevice.cpp`, `Private/BRMissionLogic.cpp`, `Private/BRMissionSolver.cpp`, `Private/BRMissionText.cpp`,
`Private/BRMissionWorld.cpp`, `Private/BRAutoTestV411.cpp` (sous `Source/Backrooms/`) ; `Tools/Missions/test_mission_logic.cpp`,
`Tools/generate_mission_sounds.py`, `Tools/Build/mac_sign_order.py` ; `RawAssets/Sounds/S_M_{Beacon,Crank,Dial,Gate,Lock,Mill,Relay,Steam,Switch,Trip,Valve}.wav` ;
`Docs/AUDIT_v4.11.md`, `Docs/RAPPORT_v4.11.md`, `Docs/GAMEPLAY_v4.11.md`, `Docs/PROPOSITIONS_NIVEAUX_v4.11.md`,
`Docs/v411/*` (journaux et script de reproduction).

**Modifiés** (`Source/Backrooms/`) : `BRAssets.h/.cpp`, `BRAutoTest.h/.cpp`, `BRAutoTestV47.cpp`, `BRAutoTestV410.cpp`,
`BRCharacter.h/.cpp`, `BRChunk.cpp`, `BREntity.h/.cpp`, `BRHUD.h/.cpp`, `BRInteractables.h/.cpp`, `BRLevels.cpp`,
`BRLocKeys.inl`, `BRPlayerController.h/.cpp`, `BRSave.h/.cpp`, `BRTypes.h`, `BRWorld.h/.cpp`. Outils :
`Tools/Build/build_mac.sh`, `Tools/Build/check_package.py`, `Tools/Build/tests/test_packaging.py`. Localisation :
`Content/Localization/Game/*/Game.po` et `Game.locres` (22 langues), `coverage.json`, `Docs/LOCALISATION_COUVERTURE.md`.
`README.md`.
