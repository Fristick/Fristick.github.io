# v4.17 : revue finale de la livraison (agent « Reality Checker »)

| | |
|---|---|
| **Rôle** | Agent installé « Reality Checker » (`~/.claude/agents/testing-reality-checker.md`, lu en entier, empreinte `748334839b21`), lancé comme sous-agent générique. Statut par défaut : NEEDS WORK. Les commandes Playwright et Laravel de la définition ne s'appliquent pas à un jeu Unreal et n'ont pas été lancées. |
| **Base** | Branche `backrooms-v4.17`, tête `21bf02b`, arbre propre avant et après la revue. Copie « avant » : `Backrooms_v417_avant` (`COMMIT_AVANT.txt` = `a9ccf1c`). |
| **Fait** | Lecture du prompt v4.17, de `RAPPORT_v4.17.md`, `VISUELS_v4.17.md`, `AGENTS_v4.17.md`, `GAMEPLAY_v4.17.md` et des 11 rapports d'agents. `git log`, `git diff`, `git show` et `git merge-base` en lecture seule. Lecture des fiches JSON, des rapports de tests, des comparaisons de performance et des journaux des deux paquets. Mesures refaites en Python (Pillow, numpy) avec des scripts jetables du dossier temporaire de la session, et avec les outils du dépôt `Tools/Visual/v417_criteria.py` et `v417_motion.py` (sortie à l'écran seulement). |
| **Non fait** | Pas d'Unreal, pas de Blender, pas de compilation, pas de banc, aucune commande `git` qui écrit. Ce fichier est le seul écrit dans le dépôt, et il n'est pas commité. |
| **Images vues** (en entier, puis en recadrages ×1,4 à ×3) | `paires/Qualite/v417_{l0_salle,p_vue,l0_combinaison_neutre_face,l1_vue,l0_fosses,l0_inventaire_personnage_fr,l0_hud_jeu}_avant_apres.jpg` ; `paires/Performance/v417_{l0_salle,l1_vue}_avant_apres.jpg` ; `details/pieds_l0_3p_cote_010-032.jpg`, `details/icones_contour.png` ; `reseau/coequipier_vu_par_{client,hote}.jpg` ; `interface/carnet_niveau3_de_1280x720_tous_indices.jpg`. Images brutes : `apres7`/`avant7` `Qualite/v417_video_l0_bacteria_{010,040,070,076,080,100,130,149}.png` (planche, puis pattes en gros plan aux images 040 et 100) ; `avant8`/`apres8` `v417_video_l0_3p_cote_{015,030,060,085}.png` ; `v417_l0_combinaison_neutre_face`, `v417_l0_plinthe` (avant et après, recadrés) ; `v417_l0_retour_{15,40}` en Qualité et en Performance (`avant7`, `apres7`, `apres9`). |

Étiquettes de cette revue : **[mesuré]** = recalculé par moi sur les fichiers cités ; **[vu]** = regardé à l'image ; **[fichier]** = lu dans un rapport, une fiche ou un journal ; **[code]** = lu dans les sources.

---

## 1. Statut et note

**Statut : NEEDS WORK.** Note honnête : **C+ (environ 60/100)**. Environ la moitié des exigences du prompt est réellement livrée et prouvée.

Ce qui est solide :

- le banc visuel figé, avec une fiche par capture ;
- l'image nette (VHS et grain coupés) ;
- la correction des diffuseurs et du plafond délavé du Performance au Level 0 ;
- les caustiques des Poolrooms ;
- l'accessibilité de l'inventaire ;
- le protocole de performance alterné.

Les preuves sont réelles : les planches correspondent aux captures brutes, et les chiffres principaux se recalculent.

Ce qui empêche READY :

1. **Le paquet annoncé « Shipping du commit final `2074046`, vérifié et traversé » ne contient pas le code de `2074046`.** L'exécutable et les conteneurs IoStore testés sont ceux du paquet de `e612818` (§ 3).
2. **Aucun test Unreal n'a tourné sur le code final.** Le § 7 du rapport dit à tort que la dernière modification du code est `ee3365e`.
3. Plusieurs affirmations chiffrées sont inexactes ou optimistes. Certaines étiquettes [correction validée] ne sont pas méritées (§ 5 et § 6).
4. De grands pans du prompt ne sont pas faits :
   - variantes déterministes ;
   - 152 paires ;
   - damier des fosses ;
   - matières des Niveaux 1 et 2 ;
   - coût séparé de l'eau ;
   - plusieurs cas d'animation et de réseau.

   Le rapport en signale la plupart.

---

## 2. Mesures refaites

| Point | Source | Le document dit | Je trouve | Verdict |
|---|---|---|---|---|
| Critères du Level 0 et des Poolrooms, passage 7 | `v417_criteria.py --before avant7 --after apres7` [mesuré] | tableau RAPPORT § 4.1 et § 4.2 | valeurs identiques à 0,01 près | exact |
| Performance final | `--after apres9 --profiles Performance` [mesuré] | `l0_salle` 0,8 %, `l0_dos` 6,0 % | 0,8 % et 6,0 % ; `l0_coupure_lampe` 99 → 119 ; `p_dos` 2,07 → 2,12 | exact, mais `l0_dos` reste au-dessus de la cible de 2 % |
| Stabilité du banc | `avant6` contre `avant7`, Qualité, 41 poses [mesuré] | PSNR médian 42,7 dB, 26 poses sur 41 au-dessus de 40 dB | 42,7 dB, 26 sur 41 | exact |
| Stabilité des critères | `v417_criteria.py --before avant6 --after avant7` [mesuré] | « égaux à 0,2 point près », sauf la lampe en Qualité | diffuseur de `l0_dos` en Performance : 48,1 contre 51,2 (**3,1 points**) ; `l0_salle` en Cinématique : 81,0 contre 81,5 ; pixel maximal de `l0_coupure_lampe` en Cinématique : **213 contre 125** | inexact |
| Banc figé | toutes les fiches des poses de `avant7`, `apres7` et `apres9`, 3 profils [mesuré] | 41 sur 41 au calme, frayeur nulle, une seule entité | 41 sur 41 dans les 7 exécutions ; 1 entité dans une seule pose (`l0_entite_bacteria`) | exact |
| Aberration sans VHS | `v417_l0_salle.png`, décalage R/B par corrélation de phase dans les quatre coins [mesuré] | 0,2-0,8 px → 0 | avant 0,43 à 0,76 px ; après 0,00 (Qualité et Performance) ; poses `_vhs` inchangées (±0,05 px) | exact, **[correction validée] méritée** |
| Papier peint | L\*a\*b\* médian des zones de `v417_l0_salle`, Qualité [mesuré] | « moins clair et moins vert » | mur gauche L\* 68,3 → 65,9, a\* −4,3 → −1,5 ; moquette L\* 60,0 → 61,8 | vrai, mais faible à l'écran ; la moquette qui s'éclaircit appuie l'[hypothèse] de l'exposition automatique |
| Planches | moitiés des planches de `l0_salle` (Qualité) et `l1_vue` (Performance) contre les PNG bruts réduits [mesuré] | « rien n'est retouché » | écart moyen de 1,7 à 2,5 sur 255 (JPEG) | exact : aucune capture inventée |
| Animation | `v417_motion.py` sur `apres7`, `apres8` et `avant8` [mesuré] | VISUELS § 5 | Bacteria : glissement médian 0, p95 3,6 à 6,8 cm, maximum 26,7 cm, dérive 15,1 à 29,1 cm, bond de 59,7°. Joueur : 34-41 → 15,6-15,8 cm par appui. **Os du pied pendant l'« appui » entre 6,6 et 18,4 cm ; pied en l'air à 3,6 cm** | chiffres exacts ; interprétation discutable (§ 5) |
| Contour des icônes | `details/icones_contour.png`, médiane du contour contre le fond [mesuré] | « environ 10:1 » | environ 8,7:1 | acceptable |
| Sol du Niveau 1 | `l1_vue` et `l1_dos`, part des pixels du sol au-dessus de 0,6 [mesuré] | « moins de reflets brillants » | 5,8 → 5,5 % et 5,9 → 4,9 % ; Niveau 2 inchangé (`l2_vue` PSNR 35,9 dB) | faible effet au Niveau 1, aucun au Niveau 2 |
| Rallumage | `v417_l0_retour_{15,40}` avec les fiches, et `BRAutoTestV417Visual.cpp:847` [code], [mesuré] | « rallumage à 15 et 40 s », « comparable » | `ReturnAt = {1.5f, 4.f}` : 1,5 s et 4 s. Alimentation à 1,0 dans toutes les fiches. À 1,5 s, Qualité L\* 54,4 → 46,6 et Cinématique 50,2 → 39,0, contre un état stable d'environ 52-54. Même écart dans `avant6`/`apres6` et `avant8`/`apres8` | **changement réel non signalé** : la v4.17 revient plus lentement à la lumière après une coupure |
| Performances | `v417f_deux_tours.md`, `v417f_controle.md`, `v417f_cine100.md` [fichier] | tableaux du RAPPORT § 5 | médianes, écarts et postes GPU identiques | exact, avec les omissions du § 6 |
| Résolution interne | `Perf.csv` (`interne_x/y`) et fiches du banc visuel [fichier] | « Performance et Qualité : 1093 × 614 » | Performance **915 × 515 (67 %)** ; Qualité 1093 × 614 (80 %) ; RTX fluide 915 × 515 ; Cinématique 1366 × 768 | inexact |
| Agents | `sha256sum` des 11 définitions [mesuré] | tableau AGENTS § 1 | 11 empreintes sur 11 identiques ; 243 fichiers | exact |
| Linux | `Engine/Platforms` du moteur installé [fichier] | pas de `Linux` | Android, IOS, VisionOS, Windows ; `LINUX_MULTIARCH_ROOT` vide | exact |

---

## 3. Constat bloquant : le paquet « de `2074046` » contient l'exécutable de `e612818`

Preuves [fichier] :

- **Échecs de copie.** `Saved/TestRuns/v417/paquet_2074046/package.txt`, étape d'archivage : **13 copies échouent définitivement**, après 10 tentatives chacune (« being used by another process »). Les fichiers concernés :
  - `Backrooms/Binaries/Win64/Backrooms-Win64-Shipping.exe` ;
  - `Backrooms/Content/Paks/Backrooms-Windows.ucas` et `.utoc`, `global.ucas` et `.utoc` ;
  - `D3D12Core.dll`, `DirectML.dll`, `dbghelp.dll`, `GFSDK_Aftermath_Lib.x64.dll`, `libogg_64.dll`, `libvorbis_64.dll`, `libvorbisfile_64.dll` et `onnxruntime.dll`.
- **Durées.** L'archivage a pris 2235 s, contre 0,35 s pour le paquet de `e612818`. L'outil a quand même écrit « BUILD SUCCESSFUL ».
- **Dates des fichiers dans `Build/Windows/Shipping`** :
  - `Backrooms-Win64-Shipping.exe` : **21/10 à 07:59:47**, 169 331 200 octets. C'est l'exécutable du paquet de `e612818`, construit de 07:58 à 08:11. Le nouvel exécutable de `Saved/StagedBuilds` date de 12:37:25 et fait 169 331 712 octets.
  - `.ucas` et `.utoc` : **08:03:25**. Seul `Backrooms-Windows.pak` est neuf (12:43:18).
- **Manifeste.** `build_manifest.txt` est écrit par `build_windows.ps1` après l'archivage (13:20). Il annonce `2074046` sans contrôler ce qui a été copié. `check_package.py` ne compare ni dates ni empreintes avec `StagedBuilds`. Le jeu annonce « v4.17 » dans les deux versions.

Conclusion :

- Le premier chapitre a été traversé avec l'exécutable et le contenu cuisiné de `e612818`, plus un `.pak` neuf.
- **La traversée du premier chapitre dans le paquet Shipping du commit final (prompt § 7) n'est pas faite.**
- Le dernier paquet propre et traversé est celui de `e612818` : 0 copie échouée, archivage en 0,35 s.

Le processus du jeu oublié, que le rapport identifie au § 5, a donc aussi verrouillé l'archive. Le rapport l'accuse seulement d'avoir « chargé la machine » (l. 389).

À faire :

1. Arrêter tout `Backrooms-Win64-Shipping.exe`.
2. Reconstruire le paquet.
3. Comparer les empreintes de l'exécutable et des `.ucas`/`.utoc` entre `Build/` et `Saved/StagedBuilds/`.
4. Relancer `check_package.py -Chapter`.
5. Faire échouer `build_windows.ps1` dès qu'une ligne « Failed to copy » apparaît sans nouvelle tentative.

---

## 4. Exigences du prompt

| § | Exigence | Verdict | Preuve | Le rapport le dit-il ? |
|---|---|---|---|---|
| 1 | Découvrir et inspecter les agents installés, sans en inventer | **Fait** | 243 définitions, 11 empreintes vérifiées | oui |
| 1 | Un agent réel par rôle, avec une mission délimitée | **Fait, par émulation** | rapports avec en-tête rôle, mission et images vues ; l'outil Agent de la session n'exposait pas les agents installés | oui (AGENTS § 2) |
| 1 | Lots en parallèle, fichiers partagés sous un responsable | **Fait** | arbres `wt/{anim,jeu,suit,ui}`, branches `v417-*` | oui |
| 1 | Relecture distincte de chaque lot important | **Partiel** | voir le détail sous ce tableau | **non** |
| 1 | `AGENTS_v4.17.md` | **Partiel** | § 6 « À remplir » ; délégation 14 « à venir » | — |
| 4.1 | `M_BR_Gear`, cartes dérivées, graphes C++ et Python, version des matériaux, cuisson | **Fait dans l'éditeur** | `test_material_sync`, `v48` strict ; non prouvé dans un paquet de `2074046` (§ 3) | en partie |
| 4.1 | Jugement à l'image : lumière neutre et néon, hôte et client, proche et loin, course, accroupi, lampe, coupure, reprise, profil | **Partiel** | neutre et néon [vu] : reflets sur les plis, petit reflet de la visière, gain modeste. Hôte et client : une pose fixe à 2,3 m, VHS et grain actifs (fiches), bras en A chez le client. Le reste n'est pas fait | la liste du « non vérifié » oublie la course, la coupure et la reprise |
| 4.2 | Chanfreins ciblés, joints fermés, collisions et pivots | **Partiel** | liseré visible sur la plinthe [vu, ×3] ; la porte et les luminaires du kit ne sont pas posés en jeu | oui |
| 4.2 | Papier peint, lés, plinthes, joints du plafond, lumière rasante | **Fait sur images fixes** | `l0_rasante`, `l0_plinthe`, `l0_raccord_*` ; ligne de marée non traitée | oui |
| 4.2 | Variantes déterministes et parcimonieuses | **Non fait** | — | oui |
| 4.2 | Décalques : répétition et transitions d'effacement inspectées | **Non fait** | `v416vis` : surfaces des décalques au bord de la zone construite NON VÉRIFIÉES (Niveaux 1 et 2) | non |
| 4.3 | Mode 2 gardé ; diffuseurs, hautes lumières et coins | **Partiel** | diffuseurs corrigés, sauf `l0_dos` en Performance (6,0 %) ; murs plus clairs que la moquette (1,17 à 1,84) | en partie : il surévalue les diffuseurs |
| 4.3 | Adaptation en mouvement : néon puis mur, porte, fosse, coupure et rallumage, lampe, combinaison proche | **Largement non fait** | une vidéo (`l0_marche`). Le rallumage est capturé à 1,5 et 4 s, avec une image après plus sombre de 7 à 13 L\*, non signalé | en partie, avec l'erreur « 15 et 40 s » |
| 4.3 | 152 paires : identifier et traiter les occultants importants | **Non fait (analyse seule)** | `eclairage.md` partie C | oui, mais le rapport l'exclut d'une « passe image seulement » que le prompt n'impose pas |
| 4.3 | Règle de survie indépendante du profil | **Fait sur `ee3365e`** | `v416light`, `test_light` ; rien relancé dans Unreal après `2074046` | non précisé |
| 4.4 | Caustiques liées à l'eau | **Fait** | `p_vue` [vu] : plus de filaments sur les trottoirs ni au plafond | oui |
| 4.4 | Clarté, voile, plafond, reflets des plafonniers, ligne humide, traversée de la surface, marches immergées | **Non tenu ou non fait** | p1 0,051 pour une cible ≤ 0,01 ; `p_dos` 1,73 ; aucune vidéo de traversée | oui |
| 4.4 | Eau, translucide, reflets et brouillard mesurés séparément | **Non fait** | — | oui |
| 4.4 | Vannes et passages secs à quatre joueurs | **Non vérifié** | test réseau avec 1 client seulement | non |
| 4.5 | Limites inspectées : glissement, demi-tour, escalier, porte, objet tenu, coude, continuité | **Partiel** | glissement et demi-tour mesurés ; le reste n'est pas fait | oui |
| 4.5 | Animations améliorées | **Partiel** | critères R.1 et R.2 non tenus : dérive 15-29 cm, bond de 59,7°, 15,7 cm par appui. Vidéo de la Bacteria : corps hors cadre, seules les pattes sont visibles [vu] | oui, mais avec des étiquettes trop fortes |
| 4.5 | LOD, pelage, mise à jour RT, reflets hors champ | **Non fait** | — | pour les reflets seulement |
| 4.6 | Image nette, grain coupé, aides et traductions | **Fait, validé** | ma mesure : 0,43-0,76 px → 0,00 | oui |
| 4.6 | Pas de barre de vie ; jauges seulement dans l'inventaire | **Fait** | `v416vis` : 0 jauge hors de PERSONNAGE ; HUD [vu] | oui |
| 4.6 | Inventaire soigné à la souris, à la manette et au clavier | **Largement fait** (appuis injectés) | planche de l'inventaire [vu] ; icônes à environ 8,7:1 | oui |
| 4.6 | Carnet qui défile vraiment ; arabe de droite à gauche ; libellés C11 | **Non fait** | aucun carnet ne déborde ; C9 et C11 non faits | **contradictoire** (§ 3.6 contre § 1 et § 7.1) |
| 4.6 | HUD discret | **Non tenu (préexistant)** | `l0_hud_jeu` [vu] : 5 notifications empilées en haut et 2 sous-titres | oui |
| 5 | Avant/après, deux graines, plusieurs exécutions, scène avec entités | **Fait sur `e612818`** | `v417f_deux_tours.md` | oui |
| 5 | Commit final mesuré | **Partiel** | Performance seulement, graine 0, 2 exécutions, **arbre modifié** (`modifie=true`) | l'arbre modifié n'est pas signalé |
| 5 | RTX 4080 en 2560 × 1440 | **Non vérifié** | protocole dans `performances.md` § 3 | oui |
| 5 | Maximum RT et 100 % de rendu | **Fait sur le portable** | `v417f_cine100.md` | oui |
| 5 | p95 et p99, images > 33, 50 et 100 ms, VRAM, CPU et GPU, chunks, chargement à part | **Partiel** | p99 jusqu'à +15 % non commenté ; temps de chargement et de shaders non rapportés ; erreurs mineures (§ 6) | en partie |
| 5 | Isolation par poste (eau, brouillard, RT, scanner) | **Partiel** | postes GPU relevés ; pas d'essai A/B ; Unreal Insights absent de l'installation | oui |
| 6 | Ordre des niveaux, chapitre public, Level 0, Pitfalls, sauvegardes | **Préservé sur `ee3365e`** | `chapitre`, `verify_pitfalls`, `v416ship` | oui, mais rien n'a tourné sur `2074046` |
| 6 | Level 6 complet, aide aux parasites | **Partiel** | `v416lidar` réussi ; `v416vis` : « scanner : aucune place à 8-14 m des parasites » NON VÉRIFIÉ, deux fois | **non** |
| 6 | Coopération jusqu'à 4 | **Non revérifié** | `net417` : 1 client | en partie |
| 6 | Trois modes d'affichage | **Partiel** | éditeur : fenêtré et sans bordure ; plein écran exclusif jamais essayé ; paquet non vérifié | oui |
| 6 | Linux et macOS | **Non fait** | moteur sans plateforme Linux ; pas de Mac | oui, honnêtement |
| 7 | Captures comparables avec métadonnées | **Fait** | 41 poses × 3 profils et leurs fiches | oui |
| 7 | Vidéos de mouvement | **Partiel** | 6 WebP ; Bacteria vue par les pattes ; pieds coupés après l'image 55 ; néons figés | en partie |
| 7 | Scènes minimales 1 à 6 | **Partiel** | manquent : traversée de l'eau, objet tenu, reflets, Niveau 2 en marche, aide aux parasites | oui (VISUELS § 6) |
| 7 | Preuves accessibles au propriétaire | **Partiel** | planches et vidéos versionnées (38 Mo) ; mesures de performance, rapports de tests et journaux des paquets seulement sous `Saved/`, ignoré par Git ; rien n'est poussé | non |
| 7 | Paquet Shipping du commit final, chapitre traversé | **Non fait** | § 3 | **affirmation fausse** |
| 7 | Modes d'affichage, perte de focus et réglages de rendu persistants dans le paquet | **Non fait** | — | oui pour les modes et le focus ; les réglages persistants ne sont jamais mentionnés |
| 7 | Tests exécutés après la dernière correction | **Mal décrit** | RAPPORT l. 427 | **faux** |
| 7 | Étiquettes et commit de chaque résultat | **Partiel** | [défaut reproduit] n'est employé nulle part, sauf dans la légende ; VISUELS sans étiquettes | — |

Lots sans relecture distincte (§ 1) :

- **Animation.** Le Code Reviewer a relu `1e67bb1..93c7fb6`. `git merge-base` montre que le lot animation (`f14dbf7`, `14b2a4c`, `f662494`, `1f3bc1e` : `BREntity.cpp`, `BRCharacter.cpp`) n'y est pas.
- **Dérivation de la combinaison.** `d2049ec`, `ebe559a` et `derive_hazmat_maps.py` sont hors du périmètre relu.
- **Corrections finales.** `3bb039b`, `ee3365e`, `2074046`, `88b9ff0` et `3fa7ef5` n'ont été relus par personne avant cette revue.
- **QA visuelle.** Elle portait sur `eb3cc8f`, avant les corrections d'éclairage.

---

## 5. Étiquettes : méritées ou non

| Affirmation (RAPPORT) | Étiquette | Verdict |
|---|---|---|
| Image nette, l. 200 | [correction validée] | **méritée** : remesurée |
| Banc figé, l. 72 | [correction validée] | **méritée** : 41 sur 41 dans 7 exécutions |
| « Les diffuseurs ne sont plus écrêtés dans aucun profil », l. 256 | [correction validée] | **en partie** : vrai en Qualité et en Cinématique, et pour `l0_salle` en Performance ; faux pour `l0_dos` en Performance (4,8 % puis 6,0 %, cible < 2 %) |
| Plafond délavé du Performance, l. 257 | [correction validée] | **méritée** : vu et mesuré (tiers haut 0,403 → 0,179) |
| Caustiques, l. 163 | [jugé à l'image] | **méritée** : vu |
| Plis de la combinaison, l. 108-111 | [jugé à l'image] | **méritée**, et « gain modeste » est juste. En revanche « gants et bottes distincts » l'était déjà avant |
| Chanfrein de la plinthe, l. 121 | [jugé à l'image] | **méritée** : vu à ×3 |
| « Coins et plafonds plus profonds, papier peint moins vert », l. 260 | [jugé à l'image] | **juste mais faible** en Qualité : tiers haut −8 %, a\* −4,3 → −1,5 |
| Bacteria : « pieds et griffes immobiles entre des bonds », l. 285 | [correction validée] | **non méritée** : la médiane est nulle, mais la dérive atteint 15 à 29 cm sur un appui et aucun critère R.1 sur l'appui n'est tenu. À requalifier en « [mesuré] amélioration partielle ». La vidéo ne montre que les pattes : « bras-béquilles » et « bras repliés près des murs » ne sont pas visibles |
| Joueur : « le pied d'appui touche le sol », l. 290 | [correction validée] | **non méritée** : la détection place l'appui entre 6,6 et 18,4 cm, et le pied en l'air descend plus bas (3,6 cm). Le contact n'est pas établi. À requalifier : « [mesuré] l'os du pied descend de 11,6 à 6,6 cm ; glissement mesuré divisé par 2,5, sous réserve de la détection d'appui » |
| « Vérifiée chez l'hôte et chez un client », l. 33 | aucune | à étiqueter [jugé à l'image], une pose fixe, VHS actif |
| Défauts de départ : diffuseurs brûlés, plafond délavé, aberration, icônes sous 3:1, stroboscope du Niveau 1 | aucune | ce sont des **[défaut reproduit]** : l'étiquette existe dans la légende mais n'est jamais employée |
| Exposition automatique qui compense, l. 263 | [hypothèse] | juste ; la moquette passe de L\* 60,0 à 61,8, ce qui va dans son sens |

---

## 6. Affirmations inexactes ou trop optimistes, et correction à faire

### Bloquantes

1. **Paquet de `2074046`**. Passages concernés :
   - `RAPPORT_v4.17.md` l. 57-58 (§ 1) ;
   - l. 382-406 (§ 6) ;
   - l. 420 (ligne Windows du tableau des plateformes).

   Correction : « Le paquet de `2074046` n'est pas valide. 13 fichiers n'ont pas été copiés dans `Build/Windows/Shipping`, dont l'exécutable et les `.ucas`/`.utoc`, verrouillés par le jeu resté ouvert. L'exécutable traversé date de 07:59 (`e612818`). Dernier paquet valide et traversé : `e612818`. » Puis reconstruire (§ 3).
2. **`RAPPORT` l. 427-430**. « Dernière modification du code du jeu : `ee3365e` » est faux : c'est `2074046` (`BRChunk.cpp`, `BRLevels.cpp`, `BRTypes.h`). Écrire que les tests Unreal du § 7.1 portent sur l'avant-dernière version du code. Les seuls contrôles sur `2074046` sont :
   - le banc visuel `apres9` (Performance) ;
   - le contrôle de performance, sur un arbre modifié ;
   - `run_benches_final.txt` (13:54, 12 bancs × 3 compilateurs réussis, sans commit dans le fichier).
3. **`RAPPORT` l. 463**. Un marqueur de travail `<!-- HORSMOTEUR -->` est resté : « Bancs C++ sur le commit final : » n'a pas de suite. Le remplacer par le résultat de `offline_v417/run_benches_final.txt`.

### Inexactes

4. **l. 27 et l. 256.** « Non écrêtés dans les trois profils » et « dans aucun profil » : ajouter que `l0_dos` reste à 6,0 % en Performance (cible < 2 %).
5. **l. 232-233.** Stabilité « à 0,2 point près » : en réalité jusqu'à 3,1 points (diffuseur de `l0_dos` en Performance). Le pixel maximal de la lampe varie de 125 à 213 en Cinématique : ce critère ne prouve rien.
6. **l. 299-300.** Corriger les résolutions internes : Performance 915 × 515 (67 %) ; Qualité 1093 × 614 (80 %) ; RTX fluide 915 × 515 (67 %).
7. **l. 335.** Le « 0 ailleurs » des images de plus de 50 ms est faux : `entites0` en Qualité et en RTX fluide est à 0,2 et 0,3 ‰, avant comme après ; `marche1` passe de 0,1-0,2 ‰ à 0.
8. **l. 337.** « Bassins en Cinématique : inchangée » est trop optimiste :
   - images au-dessus de 33 ms : 302,9 → 340,0 ‰ (+12 %), et 336,5 → 354,8 ‰ dans `cine100` ;
   - médiane : +1,2 à +1,3 % ;
   - même sens dans les deux comparaisons.

   Écrire « légère hausse, dans le bruit, à surveiller ».
9. **l. 332-334.** Le p99 n'est pas commenté : +15,0 % sur `marche0` en Qualité et +10,2 % sur `fosses` en RTX fluide. Le prompt demande de juger le p99.
10. **l. 343.** « ±4 Mo ailleurs » est faux pour la VRAM : `entites` donne −62 Mo en Qualité et −16 Mo en RTX fluide.
11. **l. 364-376.** Les manifestes du contrôle sur `2074046` portent `modifie=true`, et `v417f_controle.md` affiche un avertissement. Le dire, et préciser ce qui était modifié : documents en cours d'écriture, ou `DefaultEngine.ini` laissé par la cuisson.
12. **l. 469.** « `protect_assets.py check` : OK » cache que l'outil affiche « Tools/SourceModels absent : originaux non vérifiés ». C'est exactement le cas que le prompt (§ 4.5) demande de ne pas présenter comme une comparaison. Écrire : « OK pour les dérivés versionnés ; originaux absents, empreintes non comparées ».
13. **l. 440 et § 8.1.** Les 8 non vérifiés de `v416vis` ne sont pas tous du « carnet long ». Il y a aussi :
    - **l'aide aux parasites du Level 6**, deux fois ;
    - le plein écran exclusif ;
    - le défilement à la manette ;
    - les décalques au bord de la zone construite (Niveaux 1 et 2) ;
    - le coéquipier réel au Level 4.

    Les ajouter au § 8.1.
14. **l. 216-217.** « Mesure le défilement de la colonne qui déborde » contredit l. 49 et l. 448 (aucun carnet ne déborde). Réécrire : « le banc lit tous les indices ; aucun carnet des 7 niveaux ne déborde ; le défilement reste non vérifié ».
15. **l. 33, et `VISUELS_v4.17.md` l. 171-177.** Le coéquipier vu par le client a les bras en A, celui vu par l'hôte les a le long du corps [vu]. L'explication « pose de repos du coéquipier distant » ne tient pas : l'hôte voit lui aussi un coéquipier distant. À reproduire : défaut possible de la pose des personnages simulés chez le client. Préciser aussi que les deux captures ont le VHS et le grain actifs, et `commit : inconnu`.
16. **l. 285 et l. 290** : voir le § 5.
17. **l. 151-152.** Les 152 paires sont dites « hors de cette passe image seulement ». Le prompt (§ 4.3) demande de traiter les occultants importants : écrire « non fait ».
18. **l. 1 (titre) et § 3.** « Niveaux 1-2 » laisse croire à une passe sur ces niveaux. En fait, il y a :
    - moins de flaques au Niveau 1, effet faible ;
    - aucun changement visible au Niveau 2 ;
    - le retour à la lumière ponctuelle de la v4.16 en Performance.

    Béton, métal, câbles et conduites ne sont pas traités. Le béton bruité et marbré des murs et plafonds du Niveau 1 [vu] reste le défaut le plus visible de ces niveaux.
19. **l. 389.** Ajouter que le processus oublié a verrouillé les fichiers de l'archive, d'où les 2235 s d'archivage.
20. **`VISUELS_v4.17.md` l. 58.** Corriger « rallumage à 15 et 40 s » en **1,5 et 4 s après la remise du courant**. Remplacer « comparable » par : « après, l'image reste de 7 à 13 L\* sous son état stable à 1,5 s, alors que l'avant y est déjà ; même écart sur 3 passages ». Dire s'il s'agit d'une adaptation voulue.
21. **`VISUELS_v4.17.md` l. 50.** « 1,17 à 1,82 » : mettre 1,84 (pilier en Cinématique), comme le RAPPORT.
22. **`VISUELS_v4.17.md` l. 44.** « Identique au passage 7 » : mettre « à 1,2 point près » (`l0_dos` 4,8 → 6,0 % ; pixel de la lampe 99 → 119).
23. **`GAMEPLAY_v4.17.md` l. 26.** « Voile plus léger » : vrai en Qualité et en Cinématique (p1 −12 à −15 %). En Performance, le voile augmente (p1 +15 %).
24. **`GAMEPLAY_v4.17.md` l. 27.** « Niveaux 1 et 2 : moins de flaques brillantes » : un peu au Niveau 1 seulement.
25. **`GAMEPLAY_v4.17.md` l. 28.** « Plis et coutures en relief » : aucune couture n'est visible sur les captures (le RAPPORT l. 111 le dit). Retirer « coutures », ou l'étiqueter [constat de source].
26. **`AGENTS_v4.17.md`** :
    - l. 67, 136 et 140 : remplir le § 6 avec cette revue ;
    - § 5 : lister les lots non relus (§ 4 ci-dessus).

    Le RAPPORT l. 60-61 compte déjà la revue finale parmi les 14 délégations.
27. **Chemins privés.** Remplacer `C:\Users\<nom>\...` par `~/.claude/agents/` ou `%USERPROFILE%` dans :
    - `RAPPORT` l. 80 ;
    - `AGENTS` l. 10 et l. 34 ;
    - les en-têtes des rapports d'agents, et `performances.md` l. 64-101.

    Je n'ai trouvé ni jeton, ni `SecurityToken`, ni adresse électronique dans les documents et les fiches versionnées. `Config/` ne contient aucune section `AndroidFileServer`.
28. **Outil.** `Tools/Visual/v417_criteria.py` plante quand sa sortie est redirigée sous Windows : `UnicodeEncodeError` sur « ≈ » en cp1252. Forcer l'UTF-8 sur la sortie.

---

## 7. Ce que montrent les images

- **`Qualite/v417_l0_salle`.**
  - Changement réel mais discret : papier peint un peu moins vert, diffuseur moins brûlé, haut des murs à peine plus sombre.
  - Les murs restent nettement plus clairs que la moquette.
  - L'ambiance est très proche de l'avant.
- **`Performance/v417_l0_salle`.**
  - L'amélioration la plus nette de la livraison : le plafond blanc et délavé de l'avant disparaît ; les dalles éclairent vers le bas, avec des flaques de lumière.
  - En contrepartie, l'image est plus sombre et un peu terne (L\* 54,6 → 44,4).
- **`Performance/v417_l1_vue`.**
  - Identique à l'avant après `2074046`, comme voulu.
  - Les taches sombres à bord dur du sol et le béton bruité restent.
- **`Qualite/v417_p_vue`.**
  - Les caustiques du plafond et des trottoirs secs ont disparu : correct.
  - Le voile laiteux et le plafond clair restent.
- **`Qualite/v417_l0_combinaison_neutre_face`.**
  - Reflets brillants qui suivent les plis des bras et du torse, petit reflet sur la visière : réel, mais un aspect de film plastique uniforme.
  - Ni coutures, ni fermeture, ni renforts lisibles. Le fond devient presque noir.
- **`details/pieds_l0_3p_cote_010-032`.**
  - Après, les semelles sont plus basses et ont une ombre de contact ; avant, les pieds flottent.
  - Les vignettes de l'avant coupent souvent les pieds en haut : la comparaison est lisible mais imparfaite.
- **`details/icones_contour.png`.** Contour clair net, utile ; aucun défaut.
- **`reseau/coequipier_vu_par_client`.**
  - Matière et visière correctes.
  - Bras écartés en A, différents de la vue de l'hôte ; pieds hors du cadre.
- **Vidéo `l0_bacteria`** (`apres7`, images 010 à 149).
  - Caméra trop basse : le corps et la tête de la Bacteria sont hors du cadre, seules les pattes sont visibles.
  - Les griffes touchent la moquette avant comme après.
  - L'image d'après est plus sombre, ce qui rend la silhouette moins lisible à contre-jour.
- **`l0_hud_jeu`.**
  - Préexistant : cinq notifications empilées et deux sous-titres.
  - Au Level 0, « TÂCHE ACCOMPLIE : FILMER UNE ENTITÉ » (+2 sur la capture de l'inventaire). À confronter à la règle « Level 0 sans compteur de mission ». Ce n'est pas une régression de la v4.17.
- **`l0_fosses`.** Fonds plus noirs ; le damier régulier de trous carrés reste, contrairement à « aucune impression de damier plat » du prompt.

---

## 8. Ce qui manque pour une livraison complète

**Avant de pouvoir dire READY :**

1. Reconstruire le paquet Shipping de `2074046` sans processus du jeu ouvert. Vérifier les empreintes de l'exécutable et des `.ucas`/`.utoc` contre `StagedBuilds`, puis retraverser le chapitre. Corriger `build_windows.ps1` et `check_package.py` pour qu'ils échouent sur une copie manquée.
2. Relancer sur le code final : `smoke`, `v48`, `v416light`, `v416lidar`, `v416vis`, `chapitre` et le test réseau. Dans ces deux derniers, ajouter `-BRCommit`.
3. Corriger les passages du § 6 : rapport, index visuel, gameplay et registre ; retirer le marqueur `HORSMOTEUR`.
4. Rendre les preuves accessibles au propriétaire : copier sous `Docs/v417/` les comparaisons `v417f_*.md`, les `summary.txt` des tests et le résumé de `check_package`, ou pousser la branche. Aujourd'hui, seuls les médias sont versionnés.

**Exigences encore ouvertes du prompt, à faire ou à déclarer « non fait » en tête du rapport :**

5. Variantes déterministes du Level 0 ; traitement ciblé des occultants des 152 paires ; bord et profondeur des Pitfalls, sans damier.
6. Passe matière des Niveaux 1 et 2 : béton, métal, câbles et conduites ; masque des flaques en C++ et en Python.
7. Poolrooms :
   - voile ;
   - plafond ;
   - reflets des plafonniers ;
   - coût séparé de l'eau, du translucide, des reflets et du brouillard ;
   - vidéo de la caméra qui traverse la surface.
8. Animation :
   - verrou du pied du joueur au monde ;
   - démarrage et demi-tour de la Bacteria ;
   - escalier, porte, objet tenu, coude, enchaînements ;
   - LOD, pelage et RT ;
   - reflets hors champ ;
   - vidéo de la Bacteria cadrée sur tout le corps.
9. Combinaison en mouvement : course, accroupi, lampe, coupure et reprise, distance, avec VHS coupé. Vérifier la pose du coéquipier chez le client.
10. Aide aux parasites du Level 6 ; plein écran exclusif ; perte de focus ; réglages de rendu persistants dans le paquet ; coopération à 3 ou 4 instances (vannes et passages secs).
11. Carnet qui déborde vraiment, avec un contenu de test au besoin ; arabe et persan de droite à gauche (C9) ; libellés C11.
12. Relecture par un agent distinct du code d'animation, des scripts de dérivation de la combinaison et des corrections d'éclairage finales.
13. Décision du propriétaire sur la photosensibilité (§ 8.3 du rapport) avant toute sortie publique du chapitre : le Niveau 1 en fait partie.
14. Cible RTX 4080 : non vérifiée tant que la machine n'est pas disponible. Le protocole existe.

---

## 9. Résumé

1. Statut : NEEDS WORK — note C+ (environ 60/100) ; à peu près la moitié du prompt réellement livrée et prouvée.
2. Bloquant : le « paquet Shipping de 2074046 » n'est pas celui-là. 13 fichiers n'ont pas été copiés (exe, .ucas, .utoc), verrouillés par le jeu oublié ; l'exe testé date de 07:59 (e612818).
3. § 7 du rapport faux : la dernière modification du code est 2074046, pas ee3365e ; aucun test Unreal sur le code final ; un marqueur `<!-- HORSMOTEUR -->` reste dans le texte.
4. Confirmé par mes mesures : image nette (0,4-0,8 px → 0), diffuseurs 71-82 % → 0 en Q/C, plafond délavé du Performance corrigé, caustiques, banc figé 41/41, planches = captures brutes, tableaux de perf exacts, empreintes des agents.
5. Inexact : diffuseur de l0_dos en Performance à 6,0 % (> 2 %), résolution du Performance (915 × 515), stabilité « 0,2 point » (3,1), VRAM, > 50 ms, bassins > 33 ms (30 → 34 %), p99 jusqu'à +15 % non commenté.
6. Étiquettes trop fortes : [correction validée] pour la Bacteria (dérive 15-29 cm) et pour le pied du joueur (contact non établi) ; [défaut reproduit] jamais employé ; VISUELS sans étiquettes.
7. Non signalé : rallumage pris à 1,5/4 s (pas 15/40 s), après 7-13 L* plus sombre ; bras en A du coéquipier chez le client ; aide aux parasites non vérifiée ; protect_assets sans les originaux.
8. Non fait : variantes déterministes, 152 paires, damier des fosses, matières des Niveaux 1-2, coût de l'eau, traversée de surface, cas d'animation, LOD/pelage/RT, carnet qui défile, arabe de droite à gauche.
9. Non vérifié : 4080, modes d'affichage, focus et réglages persistants dans le paquet, plein écran exclusif, 4 joueurs ; Linux et macOS non faits, déclarés honnêtement.
10. Relectures manquantes : code d'animation, dérivation de la combinaison, corrections d'éclairage et d'accessibilité ; AGENTS § 6 vide.
11. Preuves : perf, tests et journaux du paquet seulement sous Saved/ (ignoré), rien de poussé ; chemins privés dans les documents, aucun jeton ni adresse.
12. Avant READY : reconstruire et retraverser le paquet de 2074046, relancer les tests Unreal sur le code final, corriger les passages listés au § 6.
