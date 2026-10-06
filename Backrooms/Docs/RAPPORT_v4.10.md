# Rapport v4.10 : fiabilité, fluidité, finition

Travail fait le 6 octobre 2026 sur la branche `backrooms`, à partir de `1526b1c` (v4.9, 4/4). Cette version corrige
avant d'ajouter : aucune fonction de jeu nouvelle.

## 1. Où, comment, et ce qui n'a pas pu être fait

**Machine** : Windows 10 Pro 19045, AMD Ryzen 7 7800X3D, NVIDIA GeForce RTX 4080 (16 Go), 31 Go de mémoire, deux
écrans (principal 2560x1440 à 96 DPI). **Moteur** : Unreal Engine 5.8.3 (`++UE5+Release-5.8-CL-58210709`).

**Exécuté ici** :

- compilation `BackroomsEditor Win64 Development` et `Backrooms Win64 Shipping` (sans erreur) ;
- paquet Windows Shipping construit, contrôlé par `check_package.py` (release) et lancé (§ 6.1) ;
- jeu lancé seul (`UnrealEditor.exe … -game`, fenêtre 1920x1080 sauf mention contraire), graine fixe `-BRSeed=4242` ;
- toutes les suites automatiques, avant (v4.9) et après (v4.10), avec les mêmes réglages ;
- test réseau à deux joueurs sur ce PC (`-BRNetTest`) ;
- tests unitaires des outils de livraison (Python) ;
- rendu Vulkan (celui de Linux) et DirectX 11 essayés sous Windows (§ 6).

**Préparé, non exécuté** (aucune machine ou aucun accès) : paquets Linux et macOS, signature et notarisation Apple,
envoi SteamPipe (interdit par la consigne, même en test), Overlay Steam. Ces points sont marqués « préparé » partout
dans ce rapport ; aucun n'est compté comme réussi.

**Catégories** utilisées dans les tableaux :

| Catégorie | Sens |
|---|---|
| **défaut reproduit** | observé en lançant la v4.9 sur cette machine (référence : `Saved/Baseline_v49/`) |
| **constat de source** | relevé en lisant le code ; pas de reproduction possible ou nécessaire |
| **risque à vérifier** | possible, non observé ; à vérifier sur une autre machine ou plateforme |
| **corrigé et testé** | modification compilée **et** vérifiée par un test qui échoue sur la v4.9 (ou par une mesure) |
| **préparé seulement** | écrit, non exécuté |

Une vérification qui n'a pas pu être faite est notée « NON VÉRIFIÉ » dans les rapports des tests ; elle n'est jamais
comptée comme une réussite. Le code de sortie des tests le reflète (§ 4).

## 2. Avant / après : suites automatiques

Mêmes commandes, même graine (4242), même fenêtre (1920x1080), même machine. Colonne « v4.9 » : code de `1526b1c`
avec les seules corrections de compilation pour Unreal 5.8 (rapports dans `Saved/Baseline_v49/`). Colonne « v4.10 » :
**passe finale sur la version livrée** (`Saved/Final_v410/` et `Saved/Final_v410_Net/`) : V47, V48, V49, V410 et
fosses à 0 problème et 0 vérification non faite, codes de sortie 0 ; réseau à 0 problème des deux côtés (codes 0 et
0). Les passes intermédiaires (`After_v410`, `Run2` à `Run4`, `Net_v410` à `Net3_v410`, `Full_v410`, `Check_v410`,
`Ecran2_v410`) sont gardées : elles montrent les défauts trouvés en chemin.

| Suite | v4.9 | v4.10 |
|---|---|---|
| `-BRAutoTestV47` (non-régression) | **plantage** après 17 s (code 3, aucun rapport) : assertion d'Unreal sur la sauvegarde abîmée du test | 0 problème (130 s, code 0) |
| `-BRAutoTestV48` | 1 problème : « 1954 images sans sol sur 4241 » (course RTX fluide) | 0 problème (code 0). Le défaut venait du test (§ 3, T1) |
| `-BRAutoTestV49` | « aucun problème », **faux succès** : catalogue de préchargement vide (0 ensemble), tests de préchargement passés sans rien vérifier | 0 problème ; catalogue réellement vérifié (44 ensembles, puis 36 après le classement des textures de style, § 3.1) |
| `-BRAutoTestPits` | 2 problèmes : « aucune fosse au coin de la cellule », « toujours vivant après 7 s » | 0 problème : mort par chute en 1,6 s, cause « chute », non relevable. Le défaut venait du test (§ 3, T2) |
| `-BRAutoTestV410` | — | 0 problème (114 s, code 0), dont le test sur deux écrans |
| `-BRNetTest` (hôte + client) | non lancé en v4.9 | 0 problème des deux côtés (codes 0 et 0) |
| `-BRAutoTest` (complet) | non relancé en v4.9 | voir § 5.1 |
| Tests Python (`Tools/Build/tests`) | — | 28 tests, 28 réussis |
| Paquet Windows Shipping | **ne compilait pas** (`GetLocResID`, constat de source ; reproduit sur le code v4.10 avant correction) | construit, PAQUET VÉRIFIÉ ET LANCÉ (§ 6.1) |
| Journal : « Could not find Glyph … last resort font » | dans chaque suite (texte en carrés) | aucun |
| Code de sortie avec des problèmes | toujours 0 | 1 (vérifié : V48 et réseau en cours de correction ont rendu 1) |

## 3. Défauts et corrections

### 3.1 Défauts demandés (P0 à P2)

| N° | Défaut | Catégorie | Correction | Fichiers | Test et résultat |
|---|---|---|---|---|---|
| P0 | **Clé de préchargement du Smiler** : le test cherchait `entity:Smiler`, le catalogue rangeait le Smiler sous `entity:0`. | constat de source, **et** défaut reproduit (catalogue vide : rien n'était vérifié) | Une seule fonction (`UBRAssets::EntitySetKey`) donne la clé au catalogue, au préchargement et aux tests. Le catalogue est construit après un balayage synchrone de l'Asset Registry dans le jeu non empaqueté (`ScanPathsSynchronous`), sans quoi il était vide. | `BRAssets.*`, `BRAutoTestV49.cpp` | V49 et V410 : « clé du Smiler entity:Smiler (3 ressources) », 36 ensembles au catalogue (44 avant le classement des textures de style dans l'ensemble commun), aucun ensemble demandé vide. **Corrigé et testé.** |
| P1 | **`check_package.py` acceptait des paquets invalides** (extensions seules, contenu jamais lu, outil absent ou en échec ignoré). | constat de source | Réécrit : formats lus (PE, ELF, Mach-O, `.pak`, `.utoc`/`.ucas`), contenu listé par UnrealPak avec code de sortie vérifié, cinq vérifications OK / ÉCHEC / NON VÉRIFIÉ, modes `release` et `structure`, codes 0 / 1 / 2 / 3, rapport JSON, lancement réel (`--launch`). Ressources essentielles listées dans `essential_packages.txt`. | `Tools/Build/check_package.py`, `Tools/Build/essential_packages.txt` | 15 tests unitaires : paquet complet (Windows, Linux), dossier vide ou absent, `.ucas` absent, outil absent (code 2), outil en échec, carte absente, langue absente, ressource essentielle absente, extension sans format, `.pak` sans signature, système inconnu, mode structure, lancement impossible hors hôte (code 2). **Corrigé et testé** ; sur un vrai paquet : § 6. |
| P1 | **`make_steam_vdf.py` acceptait un dossier vide** et n'exigeait pas `check_package`. | constat de source | Réécrit : identifiants vérifiés (nombres > 0, un depot par système, distincts de l'AppID), `check_package --mode release` exigé (code 0), commit du manifeste = commit courant, refus des restes d'une construction précédente (fichiers absents des listes de BuildCookRun, conteneurs plus anciens), écriture dans un dossier temporaire puis mise en place d'un bloc ; un seul refus et rien n'est écrit. Aperçu par défaut, SetLive vide, aucun envoi. | `Tools/Steam/make_steam_vdf.py` | 13 tests unitaires : écriture (Preview 1, SetLive vide), dossier vide, paquet absent, un paquet invalide sur deux (anciens fichiers intacts), contenu non vérifié, système inconnu, identifiants invalides ou répétés, autre commit, reste d'une construction précédente, conteneur ancien, manifeste absent, arbre modifié (F15), aucun appel à steamcmd. **Corrigé et testé.** Aucun envoi fait. |
| P1 | **Caches `Loaded` et `MatCache`** : les ressources et matériaux des niveaux quittés restaient en mémoire. | constat de source, **puis défaut reproduit** par le nouveau test (textures du Skin-Stealer, du Wretch et `T_MetalPanel` encore en mémoire après un aller-retour au Niveau 3) | `UBRAssets::TrimCaches` à chaque chargement de niveau : ressources et matériaux de surface, d'eau **et des modèles** (`S|<slot>`, oubliés au premier essai) dont l'ensemble n'est plus voulu. Les textures des styles de modèles (rouille, bois…) servent partout : rangées dans l'ensemble commun. | `BRAssets.*`, `BRWorld.cpp` | V410 « mémoire » : Niveau 3 (38 ressources propres) chargé, retour, ramasse-miettes : « ressources propres encore en mémoire : aucune ». **Corrigé et testé.** |
| P1 | **Préchargement asynchrone jamais attendu** : le niveau se construisait pendant que ses ressources chargeaient (chargement bloquant au premier usage). | constat de source | Nouvel état `Preparing` : les ressources indispensables du niveau suivant (`EssentialSetsForLevel`) sont demandées au début du fondu ; s'il le faut, un écran sobre montre l'avancement ; après 10 s, Pause / Échap ramène au menu ; à 25 s, le niveau s'ouvre avec ce qui est prêt (journal). Une demande remplacée n'est jamais finalisée. Le client qui rejoint une partie prépare aussi avant de construire. | `BRWorld.*`, `BRAssets.*`, `BRHUD.cpp`, `BRPlayerController.cpp` | V410 : transitions 0 → 37 → 0 « chargements synchrones pendant la construction : 0 » ; préparation bloquée (simulée) : écran tenu 11 s, retour proposé, entrée à 27 s ; demande remplacée jamais prête. Capture `V410_preparation_longue.png`. **Corrigé et testé.** |
| P1 | **Confirmation de l'affichage non modale** : Tab fermait l'inventaire, le personnage bougeait, les menus réagissaient sous le dialogue. | constat de source | `IsModalDialogOpen()` bloque `CanPlay`, Tab, menus, suppression, parole ; curseur et focus suivent l'ouverture et la fermeture ; touches tenues purgées. Le monde continue (un coup reçu s'applique). | `BRPlayerController.*` | V410 « affichage » : Tab ignoré, personnage immobile, curseur visible, RÉTABLIR rend l'inventaire, coup reçu pendant le dialogue (100 → 90), expiration en 15 s. **Corrigé et testé.** |
| P1 | **Soins non synchronisés en coopération** : le client consommait l'objet et se soignait lui-même ; deux appuis rapides ou un soin pendant un coup donnaient une santé fausse. | constat de source | Transaction confirmée par le serveur : demande numérotée (`ServerRequestHeal`), refus motivés (trop tôt, à terre, objet inconnu de la partie), objet consommé seulement à l'acceptation, stock déclaré au serveur à l'arrivée, numéros de soin dans la synchronisation de santé (un envoi ancien ne l'efface plus). | `BRCharacter.*`, `BRPlayerController.cpp`, `BRGameMode.cpp` | Réseau : deux soins dans la même image → 1 bandage, 55 → 90, santé officielle 90 reçue en 44 ms ; soin au milieu d'une série de coups → 1 bandage, santé locale 94,0 = santé officielle 94,0 ; serveur : 2 acceptés, 0 refusé. **Corrigé et testé.** |
| P2 | **Plusieurs écrans et DPI** : le mode sans bordures et les tailles de fenêtre utilisaient toujours l'écran principal ; **un essai d'affichage renvoyait la fenêtre sur l'écran principal** (Unreal recentre sur l'écran enregistré, vide par défaut). | constat de source, **puis défaut reproduit** (fenêtre mise sur l'écran 2, essai 1024x576 → écran 1, RÉTABLIR → écran 1) | `BRDisplay::ActiveMonitor()` : écran qui contient la fenêtre (taille, zone utile sans barre des tâches, DPI, identifiant). Les fenêtres proposées tiennent dans la zone utile. Avant chaque changement, l'écran de la fenêtre est donné à Unreal (`SetDisplayProperties`) ; il est aussi enregistré pour le lancement suivant. La taille est vérifiée une fois sur l'écran où la fenêtre s'ouvre (une fenêtre 1920x1080 ne tient pas dans la zone utile 1920x1040 d'un écran 1080p). | `BRDisplay.*` | V410, deux écrans (2560x1440 principal, 1920x1080 à gauche) : chaque écran reconnu ; fenêtre déplacée sur l'écran 2 → reconnue (zone utile 1920x1040) ; essai → reste sur l'écran 2 ; RÉTABLIR → reste sur l'écran 2, en 1600x900 (tient dans la zone utile) ; affichage du joueur rétabli et confirmé sur l'écran 1. Lancement suivant après un changement d'affichage fait sur l'écran 2 : la fenêtre s'ouvre sur l'écran 2 (« écran de la fenêtre au lancement : 2 sur 2 »). **Corrigé et testé.** DPI différents d'un écran à l'autre : **non vérifié** (deux écrans à 96 DPI). |
| P2 | **Validation du rendu et des plateformes**. | risque à vérifier | Rendu Vulkan (Linux) et DirectX 11 (repli SM5) lancés sous Windows avec le test de lancement (§ 6). Configurations Linux et macOS inchangées (RT matériel coupé). | — | § 6. Linux et macOS natifs : **préparé seulement**. |

### 3.2 Défauts trouvés en route

| N° | Défaut | Catégorie | Correction | Test et résultat |
|---|---|---|---|---|
| F0 | **Le jeu ne pouvait pas être empaqueté** : `BRLoc::HasTranslation` appelle `FTextLocalizationManager::GetLocResID`, qui n'existe qu'avec les données de l'éditeur (`WITH_EDITORONLY_DATA`). L'éditeur compilait, la cible du jeu (Shipping) non. | **défaut reproduit** (BuildCookRun Win64 Shipping : `error C2039 : 'GetLocResID' n'est pas membre`). Le même appel figure dans la v4.9 : **constat de source** pour elle. | Éditeur : `GetLocResID` ; jeu : `FindDisplayString`. Une clé présente dans la table de la langue courante est comptée traduite (le jeu empaqueté ne garde pas l'origine des textes). | Cible `Backrooms Win64 Shipping` compilée ; paquet : § 6.1. **Corrigé et testé.** Leçon : compiler aussi la cible du jeu, pas seulement l'éditeur. |
| F1 | **Tout le texte en carrés** en jeu lancé seul : la police de l'interface copiait le champ vide de la police du moteur (en 5.8 son contenu vient du style Slate). | **défaut reproduit** (captures v4.9, journal « last resort font ») | `BRFonts::WithScripts` copie la police effective (`GetCompositeFont`). | V410 : 0 caractère sans glyphe dans 6 écritures ; « police de dernier recours : non » sur tout le test ; capture `V410_inventaire.png`. **Corrigé et testé.** |
| F2 | **Espaces fines** (« 1 920 » en français, heure en anglais) absentes de la police : dernier recours. | **défaut reproduit** (journal : U+202F) | Remplacées par l'espace insécable avant mesure et dessin (`ABRHUD::FontSafe`). | V410 : nombres et heures de la langue courante sans glyphe manquant. **Corrigé et testé.** |
| F3 | **Sauvegarde abîmée → arrêt du jeu** (assertion d'Unreal : nom de plus de 1024 caractères). | **défaut reproduit** (V47 v4.9 : code 3, aucun rapport) | Lecture bornée (taille des chaînes), en-tête vérifié à la main, noms et objets lus sans assertion, contrôle d'intégrité (CRC32) en fin de fichier ; un fichier abîmé est mis de côté. Les fichiers v4.9 (sans contrôle) restent lisibles. | V410 : intégrité, relecture, fichier v4.9, octet changé, fichier coupé, octets au hasard, données forgées : 7/7, jeu en marche. V47 : 0 problème. **Corrigé et testé.** |
| F4 | **Code de sortie toujours 0**, même avec des problèmes (sous Windows, une sortie normale ignore le code demandé). | **défaut reproduit** | Code 1 si problème (2 avec `-BRAutoTestStrict` si des vérifications manquent) ; sous Windows, le processus se termine avec ce code une fois le moteur arrêté. `Rapport.json` écrit. | V48 (avant correction du test) : code 1 ; réseau en échec : 1 et 1 ; succès : 0. **Corrigé et testé.** |
| F5 | **Notifications empilées sur l'inventaire** : elles couvraient les onglets et les titres des panneaux. | **défaut reproduit** (capture `V410_inventaire.png` du premier passage) | Inventaire ouvert : la plus récente seulement, sur une ligne, dans la bande du haut, avec « +N » pour les autres. | V410 : pastille de 30 à 66 px, trait du haut à 92 px (1920x1080) ; de 20 à 44 px, trait à 61 px (1280x720). Capture `V410_inventaire_notifications.png`. **Corrigé et testé.** |
| F6 | **Aide des paramètres qui déborde** sur le mode de rendu et le pied de page (langues aux mots longs). | constat de source | Zone fixe de 3 lignes au-dessus du mode de rendu ; un texte plus long défile (molette ou seul) ; lignes des réglages placées au-dessus ; mode de rendu ramené à la largeur du panneau. | V410 : aide la plus longue (allemand) : 2 lignes, dans la zone, en 1920x1080 et en 1280x720 ; lignes au-dessus de l'aide. Le défilement n'a pas servi (aucune aide n'atteint 4 lignes à cette largeur) : **non vérifié en défilement**. Capture `V410_parametres_aide.png`. |
| F7 | **Alerte d'air qui clignote** même avec les flashs réduits (`DrawContextCues`). | constat de source | Flashs atténués : pulsation lente et faible ; aucun flash : message fixe. | V410 : sans flashs, intensité 0,90 → 0,90 (fixe) ; flashs normaux 0,60 → 1,00. **Corrigé et testé.** |
| F8 | **Effets de blessure, de folie et d'asphyxie** non atténués par le réglage des flashs. | constat de source | Aberration, grain, teinte rouge des coups, folie, asphyxie et glitch réduits (facteur 0,35 sans flashs) ; la teinte rouge d'un coup disparaît sans flashs (la vignette reste). Les mécaniques ne changent pas. | Lecture du code et compilation ; effet à l'écran : **non mesuré** (pas de capture comparée). |
| F9 | **Variables de console cherchées six fois par image** pour le texte « Mode réel » (avertissement du moteur à 500 appels). | **défaut reproduit** (journal) | Variables gardées après la première recherche. | Avertissements absents du journal V410. **Corrigé et testé.** |
| F10 | **Tests réseau fragiles** : l'hôte frappait après un délai fixe, alors que le client, plus long à arriver (préparation v4.10), était encore invincible ; le serveur lit cette invincibilité dans l'état envoyé par le client. | **défaut reproduit** (réseau : « coéquipier à terre NON ») | L'hôte attend l'état réel du client (invincibilité retirée, niveau prêt, vivant). | Réseau : 0 problème. **Corrigé et testé.** |
| F11 | **Client « en préparation »** : entités et serveur l'ignoraient-ils pendant son chargement ? | constat de source | Indicateur répliqué (`bLevelLoading`), posé à l'apparition et pendant chaque préparation, levé à la fin du fondu ; les entités ne visent pas un joueur en préparation ; délai de secours 90 s. | Réseau : « client signalé en préparation au serveur : oui, plus en préparation à l'arrivée : oui ». **Corrigé et testé.** |
| F12 | **Coupure forcée repoussée** : la commande de coupure (développeur, tests) était repoussée de 8 s pendant une poursuite (règle v4.7). | **défaut reproduit** (suite complète : Niveaux 2 et 3) | Une coupure forcée passe outre ; les coupures naturelles gardent la règle. | Suite complète, Niveaux 2 et 3 : 0 problème. **Corrigé et testé.** |
| F13 | **Chemin `-BRSmokeOut` coupé au premier espace**, JSON écrit avec BOM. | **défaut reproduit** (fichier parasite `…\IA\Nouveau`) | Lecture du chemin espaces compris ; JSON sans BOM ; `check_package.py` accepte les deux. | Rapport écrit au bon endroit. **Corrigé et testé.** |
| F14 | **Version affichée « v4.5 »** dans le pied du menu, écrite dans le texte traduit des 22 langues. | **défaut reproduit** (captures) | Version tirée de `BR_GAME_VERSION` (« v4.10 »), hors du texte traduit ; les 22 traductions reprises sans le numéro. Le rapport des tests indique aussi la version du jeu. | Capture `Saved/RHI_v410/Vulkan/Complet/Lancement.png`. **Corrigé et testé.** |
| F15 | **Paquet construit avec des modifications non validées** présenté comme issu de son commit. | constat de source | Le manifeste indique « arbre : propre / modifié » (`build_windows.ps1`, `common.sh`) ; `make_steam_vdf.py` refuse un arbre modifié (`--allow-dirty` pour un essai local). | Test unitaire (28e). **Corrigé et testé.** |
| F16 | **Captures des tests sur un écran à coordonnées négatives** : Unreal 5.8 s'arrête (assertion D3D12, copie hors de l'image) quand une capture avec interface est demandée et que la fenêtre est à gauche de l'écran principal. | **défaut reproduit** (plantage du test, code 3) ; défaut du moteur, dans l'outil de capture | Les tests capturent la vue seule dans ce cas. Le joueur n'a pas de commande de capture dans le jeu. | V410 sur l'écran 2 : sans plantage. **Contourné et testé** (moteur non modifié). |
| C1 | **Premier niveau du lancement** construit sans préparation : une trentaine de ressources se chargent une par une (0,4 à 0,8 ms chacune), derrière l'écran de chargement du moteur. | constat (journal) | Non corrigé : le joueur ne le voit pas (rien n'est encore affiché). Les changements de niveau en jeu, eux, sont préparés. | — |
| T1 | **Test V48 hors du niveau** : la course de 94 m en ligne droite sortait du Niveau 0, fini depuis la v4.3 (4x4 chunks) ; la moitié des images étaient comptées « sans sol ». | **défaut reproduit** (test faux, pas le jeu) | Allers-retours entre les murs d'enceinte. | V48 : 0 image sans sol. |
| T2 | **Test de chute** : l'étape IA laisse le joueur dans un coin de la salle, sans fosse à ses quatre coins. | **défaut reproduit** (test faux) | Fosse la plus proche, jusqu'à 5 cellules. | Fosses : mort par chute, non relevable. |

## 4. Tests et codes de sortie

- `-BRAutoTest` (complet), `-BRAutoTestV47`, `V48`, `V49`, `V410`, `Pits`, `-BRSmokeTest`, `-BRAutoTestSoak=<min>`,
  `-BRNetTest` : commandes dans le README (§ 9).
- Code de sortie : 0 aucun problème ; 1 problème ; 2 vérification non faite avec `-BRAutoTestStrict`.
- `Saved/AutoTest/Rapport.json` : `etat`, `duree_s`, `jeu`, `moteur`, `rhi`, `ligne_de_commande`, `problemes`, `non_verifies`,
  `scenes`.
- Outils : `python -m unittest discover -s Tools/Build/tests -v` (28 tests).

## 5. Mesures

### 5.1 Suite complète

`-BRAutoTest` complet (12 niveaux, fosses, V47, V48, V49, V410, galerie), 1920x1080 fenêtré, graine 4242, profil du
joueur de cette machine (qualité Cinématique : hit lighting, ombres RT de la lampe, rendu à 100 %). Durée : 824 s.
Rapport : `Saved/Full_v410/`.

- **Résultat** : 2 problèmes au premier passage (« la coupure de courant ne s'est pas déclenchée », Niveaux 2 et 3).
  - **Cause** : depuis la v4.7, une coupure ne commence ni pendant une poursuite ni pendant le répit qui suit. Le test
    fait apparaître des créatures juste avant, et la coupure *forcée* (commande de développement, tests) était
    repoussée de 8 s.
  - **Correction** : une coupure forcée passe outre cette règle (`bBlackoutForced`) ; les coupures naturelles la
    gardent.
  - **Vérification** : la suite complète sur les Niveaux 2 et 3, avec V47, V48, V49, V410 et la galerie, ne signale
    plus aucun problème (499 s, code 0, `Saved/Check_v410/`).
  - **Catégorie** : défaut reproduit, **corrigé et testé**. Son origine dans la v4.9 n'est pas établie : la suite
    complète v4.9 n'a pas été relancée.
- **Chargements synchrones en jeu** : 0 dans les 19 scènes mesurées ; aucune attente de shaders à l'arrivée.
- **Police de dernier recours** : jamais (0 ligne « last resort » dans le rapport).
- **Images par seconde en profil Cinématique** (hit lighting, 100 %), point de départ de chaque niveau :

| Niveau | img/s | 95 % (ms) | 99 % (ms) | GPU (ms) |
|---|---|---|---|---|
| 0 Threshold | 50 | 21,1 | 21,5 | 16,3 |
| 1 Habitable Zone | 49 | 21,3 | 21,8 | 16,6 |
| 2 Abandoned Utility Halls | 43 | 24,6 | 24,9 | 19,4 |
| 3 Electrical Station | 50 | 21,2 | 21,5 | 16,4 |
| 4 Abandoned Office | 43 | 24,7 | 25,2 | 19,4 |
| 5 Terror Hotel | 56 | 18,9 | 19,4 | 14,3 |
| 6 Lights Out | 65 | 16,6 | 17,0 | 12,0 |
| 8 Cave System | 55 | 19,3 | 19,8 | 14,6 |
| 9 The Suburbs | 64 | 16,6 | 17,4 | 12,1 |
| 10 Field of Wheat | 64 | 16,9 | 17,4 | 11,9 |
| 11 The Endless City | 61 | 17,4 | 17,9 | 12,8 |
| 37 Sublimity (Poolrooms) | 40 | 26,8 | 27,1 | 21,0 |

  Le profil Cinématique ne vise pas 60 images/s : c'est le profil le plus coûteux. La cible de 60 images/s concerne
  **RTX FLUIDE** (§ 5.2). Dans la même suite, RTX fluide en 1080p donne : course au Niveau 0, 212 images/s, 95 % des
  images en 5,7 ms, 99 % en 7,2 ms, 0 image sans sol sur 4251 ; Poolrooms, 148 images/s, 99 % en 9,0 ms.

### 5.2 RTX fluide en 1440p

Suite V48 en **2560x1440 plein écran** (`-fullscreen -ResX=2560 -ResY=1440`), profil RTX FLUIDE (RT matériel, Lumen
matériel, reflets par le cache de surfaces, rendu interne 67 % = 1715x965, TSR jusqu'à 2560x1440). Aucun autre jeu
lancé (Discord et Steam ouverts en fond). Rapport : `Saved/Mesure1440_v410/V48/`.

| Scène | img/s | médiane | 95 % | 99 % | 1 % le plus lent | pire image | GPU | images sans sol |
|---|---|---|---|---|---|---|---|---|
| Course au Niveau 0 (sprint, 20 s) | 145 | 6,5 ms | **9,4 ms** | **11,7 ms** | 77 img/s | 20,2 ms | 4,3 ms | 0 sur 2908 |
| Poolrooms (Niveau 37) | 93 | 10,1 ms | **14,4 ms** | **15,0 ms** | 42 img/s | 35,7 ms | 6,6 ms | — |

- **Objectif « 60 images/s fluides »** (95 % des images sous 16,7 ms) : **atteint** dans les deux scènes, sur cette
  machine (RTX 4080).
- **Réserve** : dans les Poolrooms, moins de 1 % des images sont lentes (jusqu'à 35,7 ms), alors que le 99e centile
  n'est que de 15,0 ms. Ce sont quelques images isolées. Elles ne viennent ni des chunks (pire image de construction
  0,1 ms), ni d'un chargement synchrone (0), ni des lumières (0 recréée). Leur cause n'est **pas identifiée** (à
  profiler avec `-BRAutoTestGPU` ou Unreal Insights). **Non corrigé.**
- Pour comparer, la même suite en 1920x1080 fenêtré (§ 5.1) donne 212 et 148 images/s.
- Une autre carte graphique n'a pas été mesurée : **risque à vérifier** (une carte RTX de milieu de gamme en 1440p).

### 5.3 Session longue

`-BRAutoTest -BRAutoTestSoak=25`, 1920x1080, profil Cinématique : allers-retours entre le Niveau 0 et les 11 autres
niveaux, avec une promenade de 12 s à chaque arrivée. Détail par tour : `Saved/Soak_v410/Complet/SessionLongue.csv`.

- **87 tours en 25 min**, 0 problème, code 0.
- **Mémoire du processus au Niveau 0** : 4153 Mo au 4e tour, 4329 Mo au plus haut (**+4 %**). Elle reste entre 4265
  et 4330 Mo pendant les dix dernières minutes : pas de croissance continue.
- **Textures** : 2700 à 2730 Mo (2815 Mo au plus haut, en arrivant dans les Poolrooms).
- **Objets** : 54 000 à 62 000 selon le niveau, sans dérive.
- **Caches du jeu** : 93 → 120 ressources et 55 → 118 matériaux, stables après environ 6 minutes, ce qui correspond
  à l'ensemble des niveaux visités au moins une fois.
- **Chargements synchrones pendant les constructions** : 0.
- **Un tour (62)** : en marchant au hasard au Niveau 10, le joueur a pris une sortie vers le Niveau 0. C'est le
  comportement du jeu, pas un défaut.
- **Réserve** : une seule session de 25 minutes, sur une seule machine. Des parties de plusieurs heures, en
  coopération, ne sont pas mesurées.

## 6. Plateformes

| Plateforme | Ce qui a été fait | Résultat | Catégorie |
|---|---|---|---|
| Windows, DirectX 12 SM6 (rendu par défaut) | toutes les suites, réseau, 1440p, session longue | § 2, § 5 | **exécuté** |
| Windows, Vulkan (`-vulkan`) | test de lancement (`-BRSmokeTest`), premier lancement avec compilation des shaders Vulkan | 0 problème, code 0. « Vulkan SM5 · RT matériel indisponible · Lumen logiciel » ; carte, 16 chunks, sol, 22 langues, 0 glyphe manquant ; menu et pied de page corrects (`Saved/RHI_v410/Vulkan/`) | **exécuté**. Approche le rendu Linux, sans le remplacer : sous Windows, Vulkan tourne en SM5, et Linux vise SM6 |
| Windows, DirectX 11 (`-dx11`, repli SM5) | test de lancement | 0 problème, code 0. « D3D11 SM5 · Lumen logiciel » (`Saved/RHI_v410/DX11/`) | **exécuté** |
| Paquet Windows (BuildCookRun, Shipping) | construit, contrôlé, lancé | PAQUET VÉRIFIÉ ET LANCÉ (§ 6.1) | **exécuté** |
| Linux x86_64 (Vulkan) | scripts et configuration inchangés (RT matériel coupé) | aucune chaîne de compilation Linux ni plateforme Linux installée sur ce PC | **préparé seulement** |
| macOS (Metal, universel) | scripts inchangés | pas de Mac | **préparé seulement** |
| Steam | `make_steam_vdf.py` réécrit et testé sur des paquets fabriqués ; **aucun envoi** | § 3.1 | **préparé seulement** |

**Steam et connexion directe (correction d'une affirmation de la v4.9).** `AUDIT_v4.9.md` et `RAPPORT_v4.9.md`
écrivaient qu'activer Steamworks « romprait la connexion directe ». **C'est faux en général.** Le résultat dépend des
pilotes réseau déclarés : le pilote IP peut rester celui des adresses IP, le pilote Steam (ou SteamSockets) servant
aux invitations. Il dépend aussi du chat vocal choisi. Les deux documents sont corrigés (texte barré, renvoi ici).
Rien de cela n'est vérifié : Steamworks n'est pas activé, et aucune fonction Steam (succès, salons, invitations)
n'est annoncée.

**Défaut trouvé pendant ces essais** : `-BRSmokeOut=<chemin>` s'arrêtait au premier espace (« Nouveau dossier »). Le
rapport du test de lancement partait dans un fichier `…\IA\Nouveau`, et `check_package.py --launch` aurait conclu
« le jeu s'est arrêté sans rapport ». Le chemin est maintenant lu avec ses espaces, avec ou sans guillemets
(`ABRAutoTest::ParsePathOption`). Vérifié : rapport écrit au bon endroit, aucun fichier parasite. Le JSON est aussi
écrit **sans BOM** (Python refusait le fichier avec BOM) ; `check_package.py` accepte les deux. **Corrigé et testé.**

### 6.1 Paquet Windows

`Tools\Build\build_windows.ps1 -UERoot "F:\Epic Games\UE_5.8" -Config Shipping` (BuildCookRun : compilation, cuisson,
conteneurs IoStore, archive dans `Build/Windows/Shipping/`), puis `check_package.py --ue … --launch`. Rapport :
`Saved/Paquet_v410/check_package.txt` et `.json`.

1. **Premier essai : échec de compilation** de la cible du jeu (défaut F0, `GetLocResID` réservé à l'éditeur). Le jeu
   ne pouvait pas être empaqueté. Corrigé.
2. **Deuxième essai** : paquet construit (cuisson en 2 min ; 159 Mo d'exécutable, 1 `.pak`, 2 `.utoc`), mais
   `check_package.py` le refuse. Deux erreurs venaient de l'outil, pas du paquet :
   - **liste IoStore** : en Unreal 5.8, la commande est `-ListContainer=<.utoc>`. La forme `-List=` n'existe que
     derrière la sous-commande `IoStore`, et UnrealPak affichait son mode d'emploi. De plus, UnrealPak change de
     dossier de travail : les chemins doivent être absolus ;
   - **dépendances** : `UIAutomationCore.dll` et `tbs.dll` sont fournies par Windows. Elles sont ajoutées à la liste,
     et sur un hôte Windows une DLL présente dans `System32` est reconnue.

   C'est la vérification sur un vrai paquet qui a révélé ces erreurs ; les tests sur paquets fabriqués ne pouvaient
   pas les voir.
3. **Résultat** : **PAQUET VÉRIFIÉ ET LANCÉ** (code 0) :
   - structure OK : lanceur et binaire PE x64, en-têtes `.pak` et `.utoc`, `.ucas` présents ;
   - contenu OK : 4604 fichiers dans le `.pak`, 1485 entrées IoStore ; polices, 22 `Game.locres`, `Game.locmeta`,
     carte et 34 ressources essentielles présentes ;
   - architecture x86_64 ;
   - dépendances : 41 bibliothèques importées, toutes présentes ou fournies par Windows ;
   - **lancement** : le paquet Shipping lancé avec `-BRAutoTest -BRSmokeTest` rend le code 0, avec 0 problème et
     0 vérification non faite.
4. **Contre-épreuve sur ce paquet** :
   - copie privée de son `.ucas` → `check_package.py` : ÉCHEC, code 1 ;
   - `make_steam_vdf.py` → REFUS, rien d'écrit : le paquet vient d'un arbre non validé (`arbre : modifie` dans le
     manifeste) ;
   - avec `--allow-dirty` et des identifiants factices, un aperçu est écrit dans un dossier d'essai : Preview 1,
     SetLive vide, aucun envoi. La détection des restes accepte cette archive neuve. Unreal 5.8 archive le jeu
     directement dans le dossier demandé : `make_steam_vdf.py` l'accepte maintenant.

## 7. Passe visuelle

Captures produites par les tests, en 1920x1080, profil Cinématique sauf mention. Chaque ligne dit ce qui a été
regardé et ce qui a été constaté. **Aucun modèle protégé ni aucune texture n'a été modifié** : cette passe vérifie le
rendu, elle ne refait pas l'art.

| Sujet | Captures | Constat |
|---|---|---|
| Texte et polices (22 langues) | `V410_inventaire.png`, `V48_langue_ar.png`, `V48_langue_ja.png`, `V48_langue_ru.png`, `V48_langue_zh-Hans.png`, `V48_menu_de.png` | **v4.9 : texte en carrés partout** (défaut F1). v4.10 : latin, cyrillique, arabe mis en forme, CJK et coréen lisibles. |
| Inventaire et notifications | `V410_inventaire_notifications.png` | une ligne dans la bande du haut, « +3 » ; panneaux dégagés (défaut F5). |
| Paramètres, aide | `V410_parametres_aide.png` (allemand) | aide dans sa zone, mode de rendu sur une ligne. |
| Préparation d'un niveau | `V410_preparation_longue.png` | écran noir sobre, avancement, retour au menu proposé après 10 s. |
| Combinaison hazmat | `V48_combinaison_L00.png`, `V48_combinaison_L37.png`, `UI_3e_personne.png` | texturée (jaune, bandes, masque) au Niveau 0, dans les Poolrooms et en 3e personne ; 3 sections, 0 matériau incorrect (V48). |
| Créatures (galerie au Niveau 4) | `L04_E_*.png` (9 créatures) | les 9 modèles présents, texturés, éclairés ; ballon du Partygoer présent. Le Smiler disparaît dans la lumière avant sa photo (comportement voulu, noté par le test). |
| Créatures dans leurs niveaux | `L0x_E_*.png` | Skin-Stealer au Niveau 3 photographié de très près et de haut (cadrage du test, pas un défaut de modèle). |
| Décors | `L00_vue.png` … `L11_vue.png`, `L37_vue.png` | rien d'anormal (pas de matériau par défaut, pas de trou). Niveau 11 : les rues entre les tours sont très sombres en hit lighting ; à revoir avec un artiste (ambiance voulue ou exposition trop basse : **non tranché**). Niveau 10 : ciel uni. |
| Poolrooms | `L37_vue.png`, `L37_sous_eau.png`, `L37_sillage.png`, `V48_RTX_fluide_L37.png` | eau, reflets, caustiques et sillage corrects ; aucune entité. |
| Créatures dans les reflets | — (pas de capture dédiée) | vérifié par les variables de rendu (V49 « reflets : créatures » : maillages à squelette dans la scène ray tracée conformes au réglage). Le menu est honnête : RAYONS si le moteur le permet, sinon « ÉCRAN (MOTEUR) » ; UE 5.8.3 n'a pas la variable de reprise des rayons. Qu'une créature hors champ apparaisse dans un reflet en hit lighting : **constat de source, non vérifié à l'image**. |
| Fosses | `L00_fosses_*.png` | vue, lampe dans une fosse, IA, chute : corrects. |

## 8. Contraintes respectées

| Contrainte | État |
|---|---|
| Aucune barre de vie ; endurance et santé mentale seulement dans l'inventaire | inchangé (V49 : 0 jauge en exploration, 2 dans l'onglet Personnage) |
| Modèles protégés (Bacteria, Hound, Skin-Stealer, Faceling PS1, ballon du Partygoer, Deathmoth, hazmat, décors des bureaux et des Poolrooms) | aucun fichier de modèle ni de texture modifié ; seul leur classement de préchargement change |
| 12 niveaux, 9 entités, fosses, objectifs, déterminisme (graine), coop à 4, Poolrooms sans entité | inchangés ; fosses, graines et Poolrooms vérifiés par V47, Pits, V48, V410 |
| Compatibilité des sauvegardes | fichiers v4.9 lus (V410) ; format inchangé, contrôle d'intégrité ajouté en fin de fichier |
| Caméscope dans le sac, VHS facultatif | inchangés |
| Connexion IP directe, langue propre à chaque joueur | inchangées (réseau v4.8 : langue par machine) |
| Aucun envoi ni lancement public Steam | aucun : `make_steam_vdf.py` n'appelle jamais steamcmd (test), aperçu par défaut, SetLive vide |
| Aucune fonction Steam annoncée sans implémentation | aucune ; l'affirmation fausse de la v4.9 sur la connexion directe est corrigée (§ 6) |

## 9. Ce qui reste à vérifier

| Point | Pourquoi ce n'est pas fait | Comment le faire |
|---|---|---|
| Paquets Linux et macOS, installation, lancement | ni machine Linux (pas de chaîne `LINUX_MULTIARCH_ROOT` ni de plateforme Linux installée), ni Mac | `Tools/Build/build_linux.sh`, `build_mac.sh`, puis `check_package.py --launch` sur l'hôte du même système |
| Signature et notarisation Apple | pas de Mac ni de certificat | `build_mac.sh` avec `MAC_SIGN_IDENTITY` et `NOTARY_PROFILE` |
| Envoi SteamPipe, Overlay | interdit par la consigne (aucun envoi, même en test) | `make_steam_vdf.py` (aperçu), puis `upload_steam.sh` à la main |
| Coop entre systèmes différents (Windows ↔ Linux ↔ macOS) | une seule machine | `-BRNetTest` sur deux machines |
| Écrans de DPI différents | les deux écrans de ce PC sont à 96 DPI | `-BRAutoTestV410` avec un écran à 150 % |
| Écran retenu sans changement d'affichage | Unreal n'enregistre l'écran qu'au premier changement d'affichage : une fenêtre seulement déplacée à la souris rouvre sur l'écran principal | à décider (enregistrer aussi l'écran à la fermeture du jeu) |
| Défilement de l'aide des paramètres | aucune aide n'atteint 4 lignes, en 1920x1080 comme en 1280x720 | même test avec une taille d'interface de 1,3 dans une petite fenêtre |
| Effets atténués (blessure, folie, asphyxie) à l'écran | pas de capture comparée | captures avec `Flashes` à 0, 1 et 2 |
| Relecture des traductions | produites automatiquement | entrées « fuzzy » des `.po` (Docs/LOCALISATION_COUVERTURE.md) |
