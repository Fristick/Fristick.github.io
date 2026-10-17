# Rapport v4.15 : parcours linéaire, image réaliste, et premier passage complet des tests dans Unreal

Travail fait du 16 au 17 octobre 2026 sur la branche `backrooms-v4.15` (partie de `backrooms`, `3f65cc9`, laissée
intacte).

| | |
|---|---|
| **Base** | `3f65cc9` (v4.14), jouée pour la première fois dans Unreal : [audit](AUDIT_v4.15.md) |
| **Documents** | [Audit](AUDIT_v4.15.md) · [Gameplay v4.15](GAMEPLAY_v4.15.md) · [Histoire et progression v4.15](HISTOIRE_PROGRESSION_v4.15.md) · journaux réels dans [`Docs/v415/`](v415/) |
| **Demandes** | tester tout (tests en attente et déjà faits) sur ce PC ; mettre à jour ce qui le demande ; ajouter un gameplay linéaire ; rendre l'image plus réaliste (exemple : « Backrooms: Escape Together ») ; pousser une fois les tests finis |
| **Décisions prises avec toi** | mort en parcours linéaire : le niveau recommence à zéro ; toute nouvelle partie est linéaire (les anciennes restent classiques) ; éclairage de type Escape Together |

## 1. Où et comment

Machine : Windows 11, i7-13620H, 16 Go, **RTX 5050 Laptop** (écran sur l'Intel UHD), Unreal Engine **5.8.3**, Visual
Studio 2026 (MSVC 14.50.35739), clang-cl 22.1.3 pour les bancs. Détails et résultats de la base : [audit](AUDIT_v4.15.md).

Tout ce qui suit a été **exécuté** sur cette machine :

- compilation éditeur et jeu (Development), **0 avertissement du projet** ; compilation sans regroupement (`-DisableUnity`)
  par les équipes de correction ;
- installation du contenu (`backrooms_setup.run(force=True)`, version 13, matériaux 8) : **0 problème** de validation,
  kit du Level 0 complet ;
- **10 bancs C++ hors moteur** compilés avec clang-cl (`/W4 /WX -Wshadow`) et MSVC (`/W4`), même sortie, 0 échec ;
- outils Python : 5000 graines de portes, 300 graines de fosses, scanner (500 graines), dégagement des mécanismes,
  langues, modèles protégés, paquets ;
- **14 suites de test du jeu** et le **test réseau à deux joueurs** (hôte et client sur ce PC), plusieurs fois.

Méthode : chaque échec a été diagnostiqué (code, journaux, captures, sondes hors moteur) puis vérifié par une seconde
analyse chargée de le réfuter ; les corrections ont été faites sur des branches séparées, relues, puis fusionnées et
rejouées dans Unreal.

## 2. Résultats des tests du jeu

| Suite | Base v4.14 | **v4.15 finale** |
|---|---|---|
| démarrage | réussi | **réussi** |
| V415 (parcours linéaire) | — | **réussi, 0 problème** |
| V415Fix (places des mécanismes, échelle du Niveau 8) | — | **0 problème** (1 non vérifié : pas de couloir sombre pour le Smiler sur cette graine) |
| banc visuel (`-BRAutoTestVisual`, 93 captures) | — | **réussi** |
| V414 | incomplet (3 non vérifiés) | incomplet (les mêmes 3) |
| V413 (histoire, à pied du Level 0 au Niveau 3) | échec, 11 problèmes | **réussi, 0 problème** |
| V412 (accès par déplacement réel) | échec, 8 | **réussi, 0 problème** |
| V411 (missions de tous les niveaux) | échec, 6 | **0 problème** (1 non vérifié : volume général lisible seulement sur le fil audio) |
| V410 | incomplet (2) | incomplet (les mêmes 2) |
| V49, V47 | réussi | **réussi** |
| V48 (performances, RTX fluide) | échec, 2 | **réussi** (Poolrooms à la limite, voir § 6) |
| fosses | échec, 1 (img/s) | échec, 1 (img/s : 22, limite de la machine) |
| complet (`-BRSeed=4242`) | échec, 29 | **1** (img/s des fosses) ; 6 non vérifiés propres à la machine |
| réseau, hôte / client | échec, 5 / 6 (après la fusion du linéaire) | **0 / 0** (3 et 2 non vérifiés : arrivée tardive, 4 joueurs) |

Le test réseau couvre maintenant aussi le départ de groupe par une porte murale et par l'échelle du Niveau 8 (2/2
rassemblés, arrivée au Niveau 9 des deux côtés), l'arrivée d'un joueur sans chute, et des décalques identiques chez
l'hôte et le client (16 chunks comparés, 0 différence).

## 3. Parcours linéaire

Toute nouvelle partie suit le fil de l'histoire, chaque niveau avec une disposition fixe pour la partie et une seule
sortie (celle que garde la mission). En solo, une mort fait recommencer le niveau de zéro (même disposition). Les
anciennes parties, REJOUER et le mode développeur restent au parcours classique. Sauvegarde au format 6 (migration
avec copie), protocole réseau 415. Règles : [`HISTOIRE_PROGRESSION_v4.15.md`](HISTOIRE_PROGRESSION_v4.15.md) ;
pour le joueur : [`GAMEPLAY_v4.15.md`](GAMEPLAY_v4.15.md) § 1 à 4. Vérifié par `-BRAutoTestV415` (graines, sortie unique,
mort, fermeture pendant la mort, marche jusqu'au Niveau 3, fin de chapitre publiée, REJOUER, migration 5 → 6) et le
banc de l'histoire (section J).

## 4. Image réaliste

Lot A (éclairage) et lot B (surfaces), relus puis corrigés, détaillés dans [`GAMEPLAY_v4.15.md`](GAMEPLAY_v4.15.md) § 7 :
caméra physique et balance des blancs, ombres de toutes les lampes (MegaLights, nouvelle option), Lumen, néons
(amorçage, fin de vie, bourdonnement spatialisé), brouillard volumétrique par niveau, faisceau IES de la lampe, matériau
du monde (normales, cavités, tissu), textures du Level 0 refaites, décalques d'usure par chunk, sol et plafond du kit
cellule par cellule, profil graphique choisi au premier lancement.

Vérifié dans le jeu par le banc visuel (Levels 0, 1, 2, 6, 37 ; profils Performance, Qualité, RTX fluide) et par le
test réseau (décalques identiques). Le banc a révélé un défaut, corrigé : avec la balance des blancs à 3900 K, la
lampe torche paraissait bleu ciel et le papier peint vert sous son faisceau ; la couleur de la lampe est maintenant
compensée par niveau (banc [`banc_couleur.txt`](v415/banc_couleur.txt)).

Non fait : R10 (kit en Nanite, demande une reconstruction Blender), l'extrémité sombre d'un tube en fin de vie (reportée
à la v4.16).

## 5. Bugs corrigés

Douze bugs du jeu trouvés et corrigés (liste et portée : [audit](AUDIT_v4.15.md) § 4 ; pour le joueur :
[`GAMEPLAY_v4.15.md`](GAMEPLAY_v4.15.md) § 5 et 6), dont deux qui rendaient une mission impossible (Level 6 sur toutes
les graines ; Niveau 8 et 9 selon la graine) et trois en coopération (arrivée d'un joueur, départ devant une porte
murale, échelles du Niveau 8 dans la roche). Ainsi qu'une vingtaine de défauts des tests eux-mêmes. Journaux des
corrections : [`correctifs_places.txt`](v415/correctifs_places.txt), [`correctifs_bancs.txt`](v415/correctifs_bancs.txt),
[`correctifs_tests_jeu.txt`](v415/correctifs_tests_jeu.txt), [`correctifs_3.txt`](v415/correctifs_3.txt),
[`correctifs_4.txt`](v415/correctifs_4.txt).

## 6. Limites restantes

- **Poolrooms, échelles des chunks vers le Niveau 2** (parcours classique seulement) : la vérification ajoutée au
  4e lot signale que la capsule du grimpeur n'y tient pas sur certaines graines (ni visée ni montée). La sortie de la
  mission marche ; ces sorties secondaires sont à corriger (suite de la v4.15).
- **Images par seconde** sur ce portable : salles de fosses en Cinématique (21–22 img/s) ; Poolrooms en RTX fluide à la
  limite de l'objectif de 60 img/s (passé dans 2 lancements sur 4) ; débits du scanner à 144 et 240 img/s non jugés.
- **Lumière de gameplay** (`LightLevelAt`) : elle ne tient pas compte des murs (une lampe derrière un mur éclaire un
  Smiler). La corriger change le gameplay : décision à prendre ; une capture du banc visuel montre le cas
  (`vis_0_*_mur_smiler`).
- **À vérifier à la main** : perte du focus (Alt+Tab pendant le scan), second écran, 4 joueurs, client arrivé en cours
  de partie et sous forte latence.
- **Langues** : toutes les traductions (21 langues) restent marquées « à relire », comme avant.
- **Plateformes** : Linux et macOS non construits (pas de chaîne de compilation ici).

## 7. Commandes

```
# Depuis Backrooms/ (Windows, Unreal 5.8.3)
# Contenu : dans l'éditeur, import backrooms_setup; backrooms_setup.run(force=True)
# Tests du jeu (un lancement par suite ; rapports dans Saved/AutoTest) :
#   UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -unattended -BRAutoTest -BRAutoTestV415 -BRAutoTestStrict
#   ... -BRAutoTestV415Fix | -BRAutoTestV413 | -BRAutoTestV412 | -BRAutoTestV411 | -BRAutoTestVisual | -BRSmokeTest
#   ... -BRAutoTest -BRAutoTestStrict -BRSeed=4242            (test complet)
# Test réseau : hôte  "/Game/Backrooms/Maps/L_Backrooms?listen?BRLevel=0" -BRNetTest ; client  127.0.0.1 -BRNetTest
# Bancs hors moteur : voir l'en-tête de chaque Tools/*/test_*.cpp (clang-cl ou g++, /W4 -Wshadow)
```
