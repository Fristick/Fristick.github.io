# Rapport v4.16 : lumière de jeu alignée sur l'image, scanner sur les supports mobiles, coopération et paquet

Travail fait du 17 au 20 octobre 2026 sur la branche `backrooms-v4.16`, partie de `backrooms-v4.15` (`aada32b`, laissée
intacte ; aucune branche distante suivie).

| | |
|---|---|
| **Base** | `aada32b` (v4.15, 17 octobre 2026, 18:39:55 à Paris), base auditée par le prompt v4.16 |
| **Tête** | dernier code `b5d8c4c` (20 octobre, 08:15) ; puis `4bc75ad` et le commit de ce rapport (documents seulement). Dernier changement du comportement du jeu : `a04a3bb` (01:51) ; ensuite `0ebb8f7` (une fonction de lecture pour le test du chapitre), `6d39fe8` (documents), `b5d8c4c` (marche des tests et un outil Python) |
| **Passages finaux** | `6d39fe8` : suites du jeu `final416`, test complet rejoué `final416b`, test réseau `fnet_*`, contrôles hors moteur ; `b5d8c4c` : `final416c` (v412, v415fix, chapitre en un lancement et en trois phases, test complet : 0 problème) ; `4bc75ad` (documents seulement) : réseau `fnet_v416lidar`, mesures sur deux graines `seeds_cine_mode1` / `seeds_cine_mode2` et `seeds_all_final` (§ 4, § 6.2 bis) ; paquet Shipping : manifeste `b5d8c4c` (§ 5) |
| **Journaux** | [`Docs/v416/`](v416/) : [`correctifs_lumiere.txt`](v416/correctifs_lumiere.txt), [`banc_lumiere.txt`](v416/banc_lumiere.txt), [`correctifs_net.txt`](v416/correctifs_net.txt), [`correctifs_ship.txt`](v416/correctifs_ship.txt), [`perf_mesures.txt`](v416/perf_mesures.txt) ; résultats (hors dépôt) : `Saved/TestRuns/autotest/<étiquette>/`, `Saved/TestRuns/perf/`, `Saved/TestRuns/v416/final/` ; relectures : `Saved/TestRuns/v416/reviews/*.json` |
| **Versions** | jeu `v4.16`, `ProjectVersion=4.16.0.0`, protocole réseau **416** (un client 4.15 est refusé) ; sauvegarde au format 6, inchangé |
| **Demande** | prompt v4.16 (« fiabiliser et finaliser la branche v4.15 »), § 3 à 11 |

## 1. Résumé

**Ce qui change** (aucun nouveau système de jeu) :

- **lumière de gameplay** : un plafonnier ne compte plus à travers les murs (grille partagée, sans trace physique : même
  valeur chez l'hôte et les clients, quel que soit le profil graphique) ; pendant une coupure, les règles suivent le fondu
  régulier de la phase répliquée, et non plus le vacillement local ; les sources de mission ne sont plus coupées par les
  capsules ; l'inventaire affiche l'état de lumière qui compte. Effet : Levels 0 à 3 nettement plus sombres pour les
  règles, seuils inchangés (décision du propriétaire, § 9) ;
- **scanner LiDAR** : inscriptions gardées dans le repère de leur support (battant, plaque, cage) ; boîtes de visée
  ignorées par les rayons ; plus de faux mur de points dans une ouverture vide ;
- **échelles** : la reprise verticale près du pied essaie d'abord la marche ; lâcher en descente sur un obstacle bas ;
- **coopération** : réveils refusés, mécanismes loin de l'hôte, actions maintenues sous la pause, répétition du clavier,
  objets rendus invisibles, personnage reçu après le fondu, abandon à terre d'un client ;
- **paquet** : REJOUER garde les documents lus ; une version publiée n'accepte que le test de démarrage et la traversée
  du chapitre, traversée jouée dans le paquet lui-même ;
- **finition** : manette dans l'inventaire, le carnet et la pause ; décalques bornés ; banc visuel v4.16 ;
- **performances** : instrumentation (`-BRAutoTestPerf`), mesures, et **une** optimisation : en Cinématique, les impacts
  éclairés de Lumen ne servent plus qu'aux reflets (fosses : 34,70 → 25,05 ms de médiane sur la RTX 5050 Laptop, § 6) ;
- **défauts du jeu trouvés en jeu après la fusion** (§ 2.9) : porte de service ouverte qui coupait la vue, les sons et
  le scanner ; vanne des Poolrooms qui volait la visée de sa voisine, puis visée entre mécanismes voisins sur tous les
  niveaux ; pause et inventaire de la pause sans effet à la manette ; Bacteria enfoncée dans le sol.

**Ce qui est vérifié** :

- dans Unreal, sur `6d39fe8` : 23 lancements solo (`final416`), **0 problème** partout sauf le test complet (2
  problèmes : la marche du test, coincée au Niveau 2 par un Smiler contre un bout de mur, a été classée « décor » :
  défaut du **test**, corrigé par `b5d8c4c`) ; le test complet rejoué sur le même commit (`final416b`) : **0 problème**.
  Les NON VERIFIE restants viennent du matériel ou de la machine (un seul écran, 144 et 240 img/s non atteintes), d'une
  limite d'Unreal 5.8 (volume général), d'une vérification à faire à la main (perte du focus, plein écran exclusif) ou
  d'un cas absent de la graine (§ 3) ;
- test réseau v4.16 sur ce PC : 1 à 3 clients, arrivées tardives, reconnexion, latence et pertes simulées, plan
  ordinaire : **0 problème** dans les 9 configurations ; le premier lancement à quatre joueurs est tombé faute de mémoire
  du PC (environnement), rejoué : 0 problème (§ 4) ;
- paquet Windows Shipping construit, contrôlé, lancé, et **premier chapitre traversé dans le paquet**
  (`0 → 1 → 37 → 2 → fin du chapitre`, trois lancements séparés par de vraies fermetures) (§ 5) ;
- hors moteur : 12 bancs × 3 compilateurs (GCC, clang-cl, MSVC), langues, outils de paquet, contrôles de génération ;
- mesures : base v4.16 (3 exécutions, 5 scènes, 4 profils), puis Cinématique après correction (3 exécutions) (§ 6).

**Ce qui ne l'est pas** (§ 8) : la machine de référence (RTX 4080 + Ryzen 7 7800X3D) ; Linux (bloqué : plateforme
absente du moteur installé) et macOS ; une vraie manette, un second PC, quatre joueurs physiques, un second écran ; le
plein écran exclusif ; la vue d'un coéquipier chez un client ; la montée d'une échelle depuis le canal par une personne
(l'essai automatique, entrées injectées, a réussi) ; l'équilibrage de la lumière en partie réelle.

**Dernier passage** (`final416c`, `b5d8c4c`) : v412, chapitre en un lancement et en trois phases et test complet
réussis, 0 problème ; v415fix 0 problème, 1 non vérifié (couloir sombre du Smiler). Dans la traversée du chapitre, un
Smiler collé au joueur au Level 2 a été écarté par le test (correction T9) et la mission résolue. Réseau du scanner
relancé sur `4bc75ad` : 0 problème. Gain du mode 2 de Lumen confirmé sur deux graines par scène (§ 6.2 bis).

**Machine des essais** : Windows 11 Famille 10.0.26200, Intel Core i7-13620H, 16 Go, **NVIDIA GeForce RTX 5050 Laptop
GPU** (pilote 610.88 ; écran sur l'Intel UHD, 1536 × 960), Unreal Engine 5.8.3 (CL 58210709), Visual Studio 2026
(MSVC 14.50), clang-cl 22.1.3, GCC 16.2.0 (MinGW-W64). Suites solo en fenêtre de 1366 × 768, profil Qualité (rendu
80 %, 1093 × 614) sauf indication ; test réseau en 1280 × 720. **Ce n'est pas la machine de référence du propriétaire
(RTX 4080 + Ryzen 7 7800X3D) : aucune mesure n'existe sur cette machine de référence**, et aucun chiffre de ce rapport
ne prédit ses performances.

Vocabulaire (prompt § 11) : **défaut reproduit** (observé dans Unreal, ou par un banc qui rejoue la règle du jeu),
**risque trouvé dans le code** (lecture, pas observé), **correction validée** (en jeu ou hors moteur : c'est précisé),
**hypothèse à tester**. Une étape non exécutée, un chunk absent ou une échelle introuvable reste NON VERIFIE, jamais une
réussite.

| Lot | Commits | Prompt |
|---|---|---|
| bancs sous GCC | `f99e8a6` | § 6 |
| scanner (lidar) | `b4aa3a6`, `91bab0d`, `6c56831`, fusion `822d39f`, puis `2e78cbe`, `425eb01` | § 4 |
| lumière | `7a265be`, `e1e5f43`, fusion `bb13003` | § 3.1, 3.2 |
| échelles (ladder) | `25657f3`, `a42e80f`, fusion `86830c5` | § 5 |
| performances (perf) | `43d2cb1`, `e1592a7`, fusion `0b21a4a` | § 7 |
| coopération (net) | `a84a176`, `2c35050`, fusion `4b19bc3` | § 8 |
| paquet (ship) | `79d767c`, `70c6ddb`, fusion `625245d` | § 8, 11.4, 11.6 |
| finition (vis) | `e70b731`, `3835547`, `efdd00d`, fusion `0d169a2` | § 9 |
| après la fusion : abandon à terre, version 4.16 | `600dc94` | § 8 |
| après la fusion : défauts trouvés en jeu (jeu et tests) | `b1b19d3`, `a165ed6`, `fefaf20`, `45c6562`, `b41facc`, `a7397b2`, `2a3ca94`, `c1c2bde`, `a04a3bb`, `0ebb8f7`, `b5d8c4c` | § 2.9 |
| après la fusion : performances | `013541a`, `c04ee2b` | § 7 |
| documents (contradictions v4.15) | `6d39fe8` | § 11 |

Méthode : chaque lot sur sa branche et son arbre de travail, écrit sans lancer Unreal, relu par une seconde analyse
(constats : `Saved/TestRuns/v416/reviews/*.json`), corrigé (« corrections de relecture »), puis fusionné le 19 octobre
vers 20:15. Les suites du jeu ont ensuite tourné dans Unreal ; chaque défaut trouvé a été corrigé dans un commit qui
donne sa cause, l'avant, l'après et le test (§ 2.9), puis tout a été relancé sur `6d39fe8`.

## 2. Corrections, point par point

### 2.1 Plafonniers comptés à travers les murs (prompt § 3.1)

**Défaut reproduit** au dernier passage de la v4.15 (banc visuel `autotest/final4/visual`, Level 0, graine 9, trois
profils) : `vis_0_<profil>_mur_smiler` : lumière de jeu 0,72 à l'endroit du Smiler, dont 0,35 d'un plafonnier derrière le
mur ; le Smiler se dissipait dans un coin que MegaLights rend noir. **Correction validée en jeu** (`final416`,
`6d39fe8`) et hors moteur.

- **Cause** : `ABRWorld::LightLevelAt` additionnait les plafonniers des cellules voisines par distance à plat, sans mur.
  Deux causes de plus, **trouvées dans le code** :
  - l'alimentation vue par les règles était `GetPower()`, le vacillement de l'image tiré au hasard chez chaque joueur et
    dépendant de son réglage FLASHS : pendant les 1,8 s de défaillance et les 1,4 s de retour, une même zone pouvait être
    éclairée chez l'un et noire chez l'autre ;
  - un plafonnier que le chunk ne pose pas (ancre à moins de 70 cm du conduit d'une échelle de sortie) comptait quand
    même : de la lumière sans lampe au pied des échelles des Niveaux 2, 8 et 37, en partie classique.
- **Avant** : lumière de jeu derrière un mur ; pendant une coupure, lumière différente selon la machine ; lumière sans
  lampe sous une trappe d'échelle.
- **Après** (`BRWorldLight.cpp`, `BRLightLogic.h`, `BRWorldLightTypes.h`, `BRLightPrints.h`) :
  - un plafonnier compte seulement si le segment de son point d'émission au point visé ne traverse pas la géométrie bâtie
    d'après la grille partagée : murs épais, jambages, linteau (225 cm) ou arche des passages, sommets, piliers,
    cellules pleines, plafond, sol (sauf fosse, bassin ou cuve ouverte), porte de mission fermée sur une ouverture de la
    grille ;
  - aucune trace physique : graine et grille seules, mêmes valeurs chez l'hôte et chez les clients, quels que soient le
    profil, Lumen, MegaLights, FLASHS ou la carte graphique ;
  - alimentation vue par les règles : `BRLight::PhasePower`, le fondu régulier de la phase répliquée ;
  - plafonnier sous la trappe d'une échelle de sortie : ne compte plus (trappe prédite avec les tirages du chunk) ;
  - loi d'apport et seuils inchangés : (1 − d/R)² à plat, × 0,6 si le néon clignote ; apparition sous 0,12, dissipation
    du Smiler au-dessus de 0,50, Deathmoth 0,55, Bacteria 0,15, santé mentale 0,12 ;
  - coût borné : cellules voisines seulement, grille en cache par chunk, plafonniers évalués au nœud de 10 cm le plus
    proche (au plus 8,7 cm du point) et mis en cache par nœud ; caches vidés au changement de niveau, de graine, de
    chargement, de disposition de la mission ou d'une porte de mission ;
  - lisible : l'inventaire affiche sous la santé mentale « dans le noir », « pénombre » ou « éclairé » (`BRLight::Band`,
    sans couleur ; 3 textes, 21 traductions à relire).
- **Tests** :
  - banc `Tools/Light/test_light.cpp` : 46 vérifications à la fusion ([`banc_lumiere.txt`](v416/banc_lumiere.txt)),
    banc réussi sous GCC, clang-cl et MSVC au passage final (`6d39fe8`) ;
  - en jeu, `-BRAutoTest -BRAutoTestV416Light -BRAutoTestStrict` (`final416/v416light`, `6d39fe8`) : **0 problème,
    2 non vérifiés** (Niveau 9 hors du banc ; aucune vraie porte de mission sur la grille d'un niveau à plafonniers) :
    - empreintes de la grille **égales au banc** pour Level 0 graines 9 (`33387e10`) et 4242 (`b142f19c`), Niveaux 1
      (`d0705669`), 2 (`94f0c4d7`), 4 (`8d3f54e3`), 5 (`b752f85f`), 8 (`9c633728`) et 37 (`5b6e31f0`) ; Niveau 9
      (`04082cdd`) relevée, non comparée ;
    - plafonniers de jeu tous à la place d'une lumière construite (ex. Level 0, graine 9 : 147 / 147) ; trappes :
      Niveau 8 en partie classique (graine du niveau 233637237), échelle à la trappe prédite 4 / 4, un plafonnier sous la
      trappe, sans lumière construite, retiré par la règle (jusqu'à 0,726) ; Niveau 37 (115083815) : 1 / 1, retiré
      (jusqu'à 0,850) ;
    - accord de la grille et de la trace physique : **6494 / 6646 paires (97,7 %)** ; la grille seule ne cache jamais
      (0 %, borne 3 %) ; la physique seule cache 4 paires (Level 0, graine 4242), 88 (Niveau 4 : 90,2 % d'accord),
      18 (Niveau 8) et 42 (Niveau 9) : composants instanciés des chunks que la grille ne porte pas (le rapport les nomme,
      ex. `BRChunk_95.InstancedStaticMeshComponent_7`) ; là, la règle compte une lumière que la géométrie arrête
      (**hypothèse à tester** : effet sur le jeu non mesuré) ;
    - mur (Level 0, graine 9) : lumière de jeu 0,218 (règle d'avant 0,727) ; apport du plafonnier derrière le mur 0,348 :
      ne compte plus ; sans mur : 0,352, compte ; passage : axe éclairé, derrière le jambage caché ;
    - Smiler du coin sombre : 0,108 (règle d'avant 0,560, au-dessus du seuil de 0,50) : il reste ; Smiler témoin dans la
      lumière (1,500) : il se dissipe ;
    - arête fermée comme une porte de mission (crochet de test) : ouvert 1,500, fermé 1,242, rouvert 1,500, caches vidés ;
    - coupure : lumière de jeu = plafonniers visibles × alimentation des règles à chaque image (104 images de défaillance,
      64 de retour), alimentation monotone ; le vacillement local diffère des règles sur 78 et 54 images, sans les
      toucher ; noir : 0,000 ; courant revenu : 0 / 28 points différents ;
    - profils Performance, Qualité, RTX fluide, Cinématique, réglages coupés, Level 0 rechargé : **0 / 28 points
      différents** ;
    - coût : 0,07 trace par créature et par image (créatures immobiles), 0,13 (libres) ; un appel à `LightLevelAt` :
      3,99 µs cache vide, 0,122 µs cache plein ;
    - inventaire : lumière 0,469 → « pénombre » (capture `v416_inventaire_lumiere`) ;
  - banc visuel v4.15, `-BRAutoTestVisual` (`final416/visual`) : `vis_0_<profil>_mur_smiler` : **0,22** (v4.15 : 0,72),
    Smiler présent (v4.15 : il se dissipait), profils Performance, Qualité, RTX fluide ;
  - réseau (`fnet_classic`) : empreintes de la grille identiques chez l'hôte et le client pour les 8 couples niveau /
    graine du niveau rencontrés (journaux `[Lumiere]`).
- **Relecture** (`reviews/light.json`, 11 constats ; corrigés dans `e1e5f43`) : refonte de `BRWorld.h` laissée à moitié
  (achevée) ; plafonniers sous les trappes comptés (corrigé) ; cache clé sur la position exacte (nœuds de 10 cm) ; traces
  des sources de mission non vidées sans plafonniers (révision vérifiée à chaque appel) ; accord grille / physique trop
  lâche (la grille seule cache au plus 3 %) ; Smiler du coin sombre qui pouvait réussir sans reproduire le défaut
  (point choisi là où la règle d'avant le dissipait, témoin, Smiler absent NON VERIFIE) ; empreintes jamais comparées
  hors moteur (table partagée `BRLightPrints.h`) ; chemins absents (crochet de test, NON VERIFIE explicites) ; fichiers
  partagés trop touchés (réduits). Non retenu : la réévaluation par entité à la cadence de l'IA (avec le cache par nœud,
  une créature qui bouge reprend le résultat).
- **Limites** (non vérifiées) : le rebond (Lumen) n'est pas compté ; les trottoirs du 37, les objets posés et les battants
  hors de la grille ne cachent pas les plafonniers ; la maison du Niveau 9 et la forme de l'arche ne sont jugées que par
  l'accord grille / physique ; chez un client, le fondu d'une coupure part avec la latence ; un client qui reçoit une
  variante de mission après avoir construit ses chunks garde leurs sorties (écart antérieur, non corrigé).

### 2.2 Lumières de mission et capsules (prompt § 3.2)

**Risque trouvé dans le code, plus large que prévu, puis défaut reproduit en jeu** (le filtre d'avant est rejoué par le
test) ; **correction validée en jeu**.

- **Cause** : la trace d'une source de mission (canal `ECC_WorldStatic`, seul l'émetteur ignoré) s'arrête sur une capsule
  (profil Pawn). Elle finissait au centre de la cible : la capsule de la cible elle-même l'arrêtait, **toujours**. Jusqu'à
  la v4.15, une source de mission ne comptait donc jamais pour un joueur ou une créature ; seulement pour les apparitions
  (point sans cible).
- **Après** : `FCollisionResponseParams` qui ignore `ECC_Pawn` (comme `FindRecentNoise`), cible ignorée
  (`LightLevelAt(P, Target)`) ; murs des chunks et battants fermés arrêtent toujours la lumière ; traces en cache 0,2 s par
  10 cm, vidées quand une source bouge, s'éteint ou que la mission change.
- **Effet sur le jeu** : la seule source de mission du contenu actuel est le porche allumé du Niveau 9 (7 m, intensité
  1,6) : un joueur à moins de 5,1 m n'est plus « dans le noir » pour la santé mentale (v4.15 : jamais). Le Level 6 n'a
  plus de source de mission depuis la v4.13 : aucun effet sur son équilibrage.
- **Tests** (`final416/v416light`) : rien entre la source et le joueur (528 cm) : apport **0,341** (loi 0,341) ; filtre
  d'avant **0,000**, trace arrêtée par la capsule du joueur lui-même (défaut d'avant reproduit) ; capsule d'un coéquipier
  sur le rayon : 0,341 ; créature (Skin-Stealer) : 0,341 ; porte physique fermée : 0,000 (obstacle `V416DoorBlocker`) ;
  porte ouverte : 0,341 ; source éteinte : 0,000 ; derrière un mur : 0,000 (obstacle du chunk). Banc `test_light` :
  portée du porche, 5,1 m.
- **Relecture** : étapes coéquipier et créature qui pouvaient réussir sans capsule sur le rayon (trace de diagnostic qui
  garde les capsules ; premier obstacle exigé) ; cause sous-estimée dans la documentation (capsule de la cible).

### 2.3 Scanner LiDAR : repère local et boîtes d'interaction (prompt § 4)

**Défaut reproduit au banc** (le modèle v4.15 redonne les 100,5 cm), **correction validée en jeu** (`final416/v416lidar`,
`6d39fe8`, 0 problème, 0 non vérifié).

- **Cause** : `ABRLidar::GetGlyphCache` gardait les points des inscriptions en coordonnées du monde, calculés une fois par
  chargement, alors que le battant de la porte du passage pivote jusqu'à 95° : le centre de la plaque se déplace de
  100,5 cm. Deuxième cause : la boîte de visée de la porte (16 × 140 × 200 cm) bloque le canal du scanner et reste en place
  après l'ouverture ; porte fermée, les impacts étaient dessinés jusqu'à 17 cm devant le vrai battant.
- **Avant** : un nouveau scan après l'ouverture redessinait la plaque dans le vide, à l'ancienne place ; faux mur de
  points dans l'ouverture vide (modèle v4.15 : 91 % des rayons arrêtés par la boîte de visée).
- **Après** : forme gardée dans le repère de son support (battant, plaque murale, cage), posée à la pose actuelle du
  support à chaque émission (position et normale) ; rien n'est relevé pendant le mouvement, une nouvelle pose recommence
  la passe ; déduplication et budget de la v4.14 inchangés ; boîte de visée et corps bloquant de la porte ignorés par les
  rayons, la visée du faisceau et le recul d'un Smiler vérifié par l'hôte ; le battant est représenté par deux boîtes à sa
  pose de l'image ; boîte de visée des cages ignorée aussi (`425eb01`). Protocole, sauvegarde et textes inchangés.
- **Tests** :
  - banc `Tools/Lidar/test_lidar.cpp` : 20 241 vérifications à la fusion, réussi sous les 3 compilateurs au passage final ;
  - en jeu (`final416/v416lidar`, Level 6, graine 4136, bonne porte : mécanisme 12) : porte fermée : 82 inscriptions,
    toutes sur la plaque actuelle (écart au plan 0,60 cm au plus), 0 impact sur la boîte de visée ou le corps bloquant ;
    ouverture pendant le scan (1,24 s jusqu'à 95°) : 1343 impacts sur le battant dont 1327 pendant le mouvement,
    **0 inscription pendant le mouvement**, 0 à l'ancienne place ; centre de la plaque **déplacé de 100,5 cm** (prévu :
    100,5) ; nouveau scan, trois rescans (galerie, face, biais), après le rechargement du chunk et après la reprise :
    **82 inscriptions sur la plaque actuelle, 0 à l'ancienne place** ; ouverture vide : 0 point dans l'ouverture, 2105
    au-delà ; reprise : nouvelle identité du scanner, 0 point ; autre plan (graine 4126) : identité changée, 0 point ;
    bornes : 1 scanner, 1389 points sur 24 000 au plus, pic de 116 points par image (borne 492), 0 refusé ; temps du
    scanner 0,081 ms par image (processeur ; le coût de rendu des instances n'est pas un poste mesuré) ;
  - passages antérieurs : `v416a` (`822d39f`, échec d'un défaut du test, corrigé par `2e78cbe`), `v416b` (`2e78cbe`) et
    `v416c` (`425eb01`) réussis ; réseau avec `-BRAutoTestV416Lidar` : `net_v416lidar` (`822d39f`) : 0 problème chez
    l'hôte et le client, même plan `20b45d7c`, 82 / 82 sur la plaque actuelle des deux côtés ; **relancé sur `4bc75ad`**
    (`fnet_v416lidar`, code du jeu de `b5d8c4c`) : 0 problème chez l'hôte et le client (le client tardif en mission
    LiDAR, `fnet_lidar`, aussi : § 4).
- **Après la fusion** : la boîte d'interaction de la porte de service ouverte coupait aussi le scanner (défaut J5,
  § 2.9).
- **Relecture** (`reviews/lidar.json`, 3 constats, corrigés par `2e78cbe` et `425eb01`). La boîte des cages n'a pas
  d'étape dédiée en jeu. **Risque** : forme du battant analytique (poignée et charnières non représentées).

### 2.4 Échelles (prompt § 5)

**Risques trouvés dans le code ; correction validée en jeu avec des entrées injectées** (`final416/v416ladder`,
`6d39fe8`, 0 problème, 0 non vérifié) et par le modèle 2D du banc.

- **Ce qui avait tourné en v4.15** (dernier passage `final4`, code de `03a7a3b`) : prise depuis le canal NON VERIFIEE
  (aucune place libre devant l'échelle essayée) ; réseau : aucune échelle dans les chunks construits, NON VERIFIE.
- **Défaut 1** (`ABRCharacter::UpdateClimb`, toutes les échelles) : la reprise verticale des correctifs 5 levait le
  grimpeur dès qu'il touchait une paroi non praticable avec les pieds sous le pied de l'échelle plus une marche. Avant
  (modèle) : un mur haut, une caisse, un coéquipier ou un plafond bas le levaient jusqu'à 37 cm sur un sol plat, 105 cm
  depuis le canal. Après : `BRGather::ClimbAfterHit` : reprise seulement si l'essai de la marche est libre.
- **Défaut 2** (descente) : sur un obstacle à une marche au plus au-dessus du pied, le grimpeur restait accroché à
  16–20 cm. Après : `BRGather::ClimbReleaseOnDescent` : il lâche l'échelle et se tient sur l'obstacle.
- **Tests** :
  - banc `Tools/Departure/test_departure.cpp`, section 6 (30, 60, 120 et 144 images/s) : 68 vérifications ;
  - en jeu, `-BRAutoTestV416Ladder` (graines **du niveau** 2303284074 et 115083815, parcours classique imposé) :
    - chunk (2, −1) à 2303284074 : échelle à la place de la règle du jeu (11750, −1560, 0), ancienne place sans échelle ;
    - caisse de 20 cm : accroche atteinte (0,34 s), 0 image en collision sur 19 ; caisse de 50 cm : arrêté au sol,
      aucune hausse, 0 / 306 ;
    - prise depuis le canal (échelle `BRExit_8` sur son trottoir, 115083815) : « avancer tout de suite » : montée
      au-dessus du trottoir en 0,55 s, puis debout sur le trottoir (70,2 cm) ; « avancer 0,5 s après » : 1,07 s ;
      montée à 200 cm puis descente : lâchée après 1,45 s, debout sur le trottoir, 0 / 284 ;
    - mur haut (400 cm) : hausse 0,0 cm (v4.15 correctifs 5 : jusqu'à 103 cm), côté du mur gardé ; plafond bas : 0,0 cm,
      tête 17,8 cm sous le plafond ; caisse de 20 cm sous l'accroche en descente : lâchée après 0,96 s, debout sur la
      caisse, 0 / 71 ;
    - montée complète : une demande de départ au sommet, arrivée au Niveau 2, 0 / 292 ;
    - Niveau 8 (`BRExit_42`) et Level 6 (alcôve de l'aile) : montée, descente jusqu'au lâcher, lâcher en hauteur par
      Saut : au sol en 0,51 et 0,42 s, jamais en l'air sans échelle ;
  - `-BRAutoTestV415Fix` (`final416/v415fix`) : prise depuis le canal jugée sur `BRExit_61` (115083815) : montée
    au-dessus du trottoir ; **0 problème, 1 non vérifié** (marche avec un Smiler : aucun couloir droit et sombre parmi
    4 points de passage) ; v4.15 : 2 non vérifiés ;
  - réseau (`fnet_classic`, plan ordinaire, étapes v4.16 des échelles toujours à la fin) : Niveau 37 à 115083815, une
    échelle des chunks construite (`BRExit_42`, place du banc), hôte au sommet : une demande, 1 / 2 puis 2 / 2, départ et
    arrivée au Niveau 2 chez l'hôte et le client ; à 2303284074, aucune échelle vers le Niveau 2 dans les chunks
    construits au départ : la ligne renvoie au jugement fait à 115083815 ;
  - régressions : Levels 6 et 8 et départ de groupe dans `v413`, `v412` et le test complet : réussis (§ 3) ;
  - traversée manuelle depuis le canal (montée, descente, rassemblement) par une personne : **non faite**.
- **Relecture** (`reviews/ladder.json`, 4 constats ; `a42e80f`). Après la fusion, défaut du test `fefaf20` (§ 2.9, T3).
- **Risques** : les normales de contact du moteur peuvent différer du modèle 2D sur d'autres échelles ; règle plus
  stricte : un obstacle de 36 à 45 cm sous l'accroche n'est plus franchi.

### 2.5 Banc des départs sous GCC strict (prompt § 6)

- **Défaut reproduit** (GCC 16.2, `-std=c++17 -Wall -Wextra -Werror -Wshadow`) : `test_departure.cpp:705` « declaration of
  'Wall' / 'Door' shadows a global declaration » (constantes `Pool37::Wall`, `Pool37::Door` et valeurs de
  `enum class EEdge`). clang-cl et MSVC ne le signalaient pas.
- **Après** (`f99e8a6`) : constantes renommées `WallThickness` et `DoorChance` ; aucune option affaiblie ; la commande
  stricte d'origine passe (54 vérifications à ce commit ; 68 aujourd'hui).
- `Tools/run_benches.py` compile chaque banc avec chaque compilateur trouvé (`-Werror` ou `/WX`), l'exécute depuis
  `Backrooms/` et compare les sorties. Sous Windows, GCC est lié en statique (sinon un `libstdc++-6.dll` d'un autre
  logiciel du PATH faisait planter `std::regex`).
- **Tests** : passage final (`6d39fe8`) : **RESULTAT : OK (12 banc(s) x 3 compilateur(s))** ; de même sur `b5d8c4c`
  (message du commit).

### 2.6 Performances (prompt § 7)

**Instrumentation** (`43d2cb1`, `e1592a7`), **mesures** et **une optimisation** (`013541a`) ; résultats au § 6.

- `-BRAutoTest -BRAutoTestPerf` (`BRAutoTestV416Perf.cpp`) : cinq scènes à graine fixe (fosses du Level 0 ; Poolrooms,
  graine du niveau 178662963 ; marche au Level 0 ; marche au Niveau 1, chunks construits et démontés ; scan maintenu au
  Level 6) × quatre profils (ou `-BRProfile=<n>`) ; préchauffage qui attend shaders, PSO et chunks (sinon mesure instable,
  NON VERIFIE) ; fenêtre fixe : médiane, 95 %, 99 %, pire image, longues images > 33,3 / 50 / 100 ms, threads, GPU, VRAM,
  streaming, chargements synchrones, PSO, compteurs du scanner ; trois relevés ProfileGPU ramenés à des postes ;
  réglages de rendu toujours notés.
- `Tools/Tests/run_perf.ps1` (N exécutions × graines × profils sous une étiquette, `commit.txt` par exécution) et
  `Tools/Tests/perf_summary.py` (médiane des médianes, écart, pire 99 %, longues images, postes ; avant / après ; avertit
  d'un arbre modifié, de réglages ou de résolutions différents).
- `-BRPerfCmds="r.X 1;r.Y 2"` (`c04ee2b`, hors version publiée) : commandes de la console appliquées après le profil,
  pour essayer un réglage sans recompiler (`run_perf.ps1 -PerfCmds`).
- `-BRHitLightingMode=1` (`013541a`, hors version publiée) : rend l'ancien mode de Lumen en Cinématique, pour comparer.
- **Tests** : banc `Tools/Perf/test_perf.cpp` (32 vérifications à la fusion, réussi au passage final) ;
  `test_perf_summary.py` : 15 tests OK (passage final).
- **Relecture** (`reviews/perf.json`, 6 constats, corrigés dans `e1592a7`) : baisse de qualité silencieuse non signalée,
  scène comptée valide sans son scénario, marche sans vitesse, « Other Children » mal compté, graines mélangées,
  étiquette réutilisée.

### 2.7 Coopération, sauvegardes et paquet (prompt § 8)

**Coopération** (lot net, [`correctifs_net.txt`](v416/correctifs_net.txt)). Sept **risques trouvés dans le code**,
**corrections validées en jeu** par le test réseau v4.16 sur ce PC (§ 4), sauf le défaut 7 dont le chemin n'a pas été
emprunté :

1. **Réveil refusé loin des coéquipiers** (`ABRWorld::HasLivingTeammate`). Au-delà de 150 m, un coéquipier vivant
   n'existait pas chez le client : réveil à 6 s refusé par l'hôte, retour à terre au point de départ. Après : le
   personnage d'un joueur est répliqué à tous les joueurs à toute distance (`ABRCharacter::IsNetRelevantFor`). En jeu
   (`fnet_4j_b`) : client à 230 m, « personnages des coéquipiers toujours répliqués ici 3/3 », délai à terre 30,0 s,
   aide d'un coéquipier affichée, réveil non refusé.
2. **Réveil refusé quand le dernier coéquipier debout tombe après** (`ABRCharacter::MinRespawnDelay`). Après : l'hôte
   retient le premier instant où personne n'est debout. En jeu : client lointain tombé après tous les autres : délai à
   terre 6,0 s, réveil non refusé.
3. **Mécanismes manipulés par un client loin de l'hôte** (`ABRWorld::UpdateMissionDevices`). Après : animé s'il est à
   moins de 35 m de n'importe quel joueur. En jeu : hôte à 53 m du sas et 50 m de la passerelle : « mission résolue par
   l'action du client », sas vide, tablier posé, navigation des entités par le tablier.
4. **Interaction maintenue sous le menu pause** (`ABRPlayerController::StopHeldActions`). En jeu : interaction et scan
   arrêtés à la pause, pas de reprise sans nouvel appui.
5. **Répétition du clavier après une purge des touches** (`ABRPlayerController::InputKey` ; Unreal 5.8 change la
   première répétition d'une touche purgée en nouvel appui). En jeu : après l'inventaire, « scan repris seul non » ; après
   la fenêtre réduite par le test, « répétition(s) ignorée(s) 3/3 ».
6. **Objets rendus au monde invisibles pour un client qui rejoint ou se reconnecte** (`ABRWorld::LoadLevelNow`). En
   jeu (`fnet_reco`) : « objets rendus au monde visibles 1/1 ».
7. **Personnage reçu après la fin du fondu d'arrivée** (`ABRWorld::Tick`) : entrées déverrouillées et fin du chargement
   signalée. **Chemin non emprunté** : les 5 rapports qui le notent disent « personnage reçu pendant l'arrivée ».

**Sauvegardes et paquet** (lot ship, [`correctifs_ship.txt`](v416/correctifs_ship.txt)) :

- **D1. REJOUER : une note lue en rejeu disparaissait du carnet** (risque trouvé dans le code). Cause : la branche du
  rejeu de `ABRPlayerController::WriteActiveSave` ne complétait que les entités du journal. Après : la note s'ajoute
  (union, jamais de retrait), le rejeu n'écrit toujours rien d'autre. **Validée en jeu** (`final416/v416ship`, réussi) :
  dégâts et eau bue en rejeu, mort en rejeu (fichier inchangé), fermeture : « champs durables identiques à la campagne
  oui, journal = union oui » ; campagne reprise avec son équipement ; partie au format 5 migrée en mémoire, copie
  `_Format5` identique. Banc de l'histoire, section I.
- **D2. Plans de test joignables dans un paquet Shipping** (risque trouvé dans le code). Après : seuls `-BRAutoTest
  -BRSmokeTest` et `-BRAutoTest -BRAutoTestChapter` sont acceptés (`bPackagePlan`, tout autre plan refusé avant sa
  première étape, code 1). Le paquet a exécuté ces deux plans (§ 5) ; **le refus d'un autre plan n'a pas été essayé**.
- **D3. Parties du joueur lues au lancement d'un essai du paquet** (risque trouvé dans le code). Après : préfixes de test
  dès le lancement (`BR_AutoTestChapitre_`, `BR_AutoTestLancement_`). En jeu : « parties du joueur ni lues ni écrites »,
  nettoyage à la fin (0 fichier restant).
- **N1. Traversée du premier chapitre dans le paquet** (`-BRAutoTestChapter`, `check_package.py --chapter`,
  `build_windows.ps1 -Chapter`) : **exécutée** (§ 3 et § 5).
- **R1** (risque trouvé, non corrigé) : l'écriture de l'arrivée, 2 s après le chargement, peut tomber pendant le fondu ;
  un jeu tué dans la minute suivante reprend le niveau neuf. **R2** : un paquet Shipping n'accepte plus `-BRNetTest`.
- **Relectures** : `reviews/net.json` (8 constats, corrigés dans `2c35050` ou `600dc94`) ; `reviews/ship.json` (4,
  `70c6ddb`).

### 2.8 Graphismes, HUD, manette et décalques (prompt § 9)

Aucun modèle protégé touché (`protect_assets.py check` OK) ; aucune créature remodelée ; la Bacteria est corrigée dans
le code de pose, pas dans l'asset (§ 2.9, J4). Banc `-BRAutoTest -BRAutoTestV416Visual -BRAutoTestStrict`
(`final416/v416vis`, `6d39fe8`) : **0 problème, 7 non vérifiés**.

- **Décalques** (`BRDecalLogic.h`, `ABRChunk::BuildDecals`) : distance d'effacement 35 m au plus à toute définition (avant :
  jusqu'à 116 m) ; au plus 40 décalques projetés et 24 taches au sol par chunk ; seuil humide jamais au-dessus d'un
  bassin ; éraflures retournées une sur deux. En jeu : Level 0 (graines 9 et 4242), Level 1 et Level 2 (graine 4242) :
  0 décalque sans surface, 0 au-dessus d'un passage, 0 tache au sol flottante ; graine 4242 du Level 0 : 2 tirages écartés
  par la borne dans un chunk (fonctionnement voulu) ; distance utile de 11,8 à 35,0 m. Banc `test_decals.cpp` :
  38 vérifications.
- **Manette** : invites des boutons quand elle a servi en dernier ; tout bouton compte pour la détecter ; inventaire et
  carnet parcourables ; onglet PERSONNAGE (curseur à la croix, A utiliser ou équiper, X inspecter, B fermer) ; pause
  (cartes à la croix, A, B ; plus de « [FIN] Quitter » à la manette). En jeu (appuis injectés) : 18 actions nommées à la
  manette, 0 sans la liaison annoncée ; pause : bouton A montré 6 fois sur 6, touche FIN jamais ; RB, croix, LB, Y, X, A,
  B dans l'inventaire ; dans la pause : croix bas (carte 1), A (onglet 2), RB (onglet 3), LB (onglet 2), B (retour à la
  pause, inventaire fermé). Deux de ces commandes ne marchaient pas pendant la pause (J2, J3, § 2.9).
- **HUD** : 462 images, 18 345 textes (1280 × 720, 1920 × 1080, 2560 × 1080 ; 5 onglets, la pause et le HUD du jeu ;
  22 langues) : 0 hors de l'image, 0 au-delà de la largeur, 0 hors du panneau, 0 chevauchement ; jauges dans PERSONNAGE
  seulement (66 rendus), aucune ailleurs (396 rendus), aucune barre de vie ; vraies fenêtres 1280 × 720, 1520 × 855 et
  1520 × 641 obtenues ; ligne MODE D'AFFICHAGE présente ; fenêtre d'origine rétablie.
- **Entités et kit** : matériaux du joueur et des neuf entités : 0 vide, 0 au matériau par défaut ; Bacteria à +2,1 cm
  du sol (borne 3 cm) ; plinthes : 0 trou ; joints de plafond ouverts : 0.
- **Non vérifiés** (7) : défilement du carnet à la croix (colonne plus courte que sa zone) ; plein écran exclusif
  (essayé seulement avec `-BRV416PleinEcran`, qui change la définition de l'écran) ; décalques au bord de la zone
  construite (Level 1 : 3 décalques et 4 points de taches ; Level 2 : 1) ; scanner : aucune place à 8–14 m des parasites
  (deux étapes) ; vue d'un vrai coéquipier (banc solo).
- **Captures** à juger à l'œil (seules preuves des poses, de l'appui des pieds et des raccords) : `v416_entite_*`,
  `v416_kit_*`, `v416_hud_*`, `v416_manette_*`, `v416_lidar_*` dans `final416/v416vis` ; ce rapport n'en contient pas
  d'examen à l'œil.

### 2.9 Défauts trouvés pendant la fusion et les essais en jeu

Chaque défaut a son commit, dont le message donne la cause, l'avant, l'après et les tests. « Avant » cite le lancement
qui l'a montré ; « Test » le lancement final.

**Défauts du jeu**

| | Défaut | Cause | Avant | Après | Test | Commit |
|---|---|---|---|---|---|---|
| J1 | Abandon à terre d'un client refusé (antérieur à la v4.16 ; la branche net avait d'abord réservé l'abandon à l'hôte) | le réveil qui suit l'abandon passait par `ServerReportRespawn` ; l'hôte le prenait pour un réveil trop tôt (fenêtre de 28,5 s) | **risque trouvé dans le code** (relecture du lot net) : client retombé à terre au point de départ, avec l'équipement de départ | message `ABRCharacter::ServerReportGiveUp`, accepté à tout moment ; tout joueur à terre peut abandonner (règle d'avant), invite rendue à tous ; version 4.16, protocole 416 | `fnet_base`, `fnet_4j_b`, `fnet_lag` : réveil 0,3 s après l'appui, debout 2,5 s après, « abandon à terre accepté oui » chez l'hôte | `600dc94` |
| J2 | Pause en solo sans effet à la croix et à A | la pause arrête le jeu ; `IA_MenuUp`, `IA_MenuDown`, `IA_MenuConfirm` n'étaient pas actives pendant la pause | `v416m1` (`600dc94`), journal : « pause, croix bas : carte 0 (attendu 1) », « pause, A : onglet 0 (attendu 2) » | ces actions sont actives pendant la pause (`bTriggerWhenPaused`) | `final416/v416vis` : carte 1, onglet 2 | `b1b19d3` |
| J3 | Dans l'inventaire ouvert depuis la pause : B, LB / RB, Y, X et croix gauche / droite sans effet | mêmes actions (`IA_Crouch`, `IA_Drink`, `IA_Reload`, `IA_Flashlight`, `IA_Interact`, `IA_MenuPrev` / `Next`) inactives pendant la pause | `v416m2` (`b1b19d3`) : « pause, B : retour à la pause : inventaire ouvert, pause ouverte » | actives pendant la pause ; hors pause, rien ne change (leurs gestionnaires passent par `CanPlay`) | `final416/v416vis` : RB onglet 3, LB onglet 2, B : inventaire fermé, pause ouverte | `45c6562` |
| J4 | Bacteria enfoncée dans le sol | le roulis des bras abaisse les griffes (3,3 cm sous la capsule au repos) ; tassement à l'arrêt et au sursaut jusqu'à 9 cm (pièces rigides sans jambes) | `v416m2` : bas des volumes visibles à −3,4 cm du sol (borne 3 cm) | `ABREntity::KeepRigidPartsAboveGround` : corps et bras relevés du manque de leurs points d'appui, à chaque image (code, pas l'asset) | `final416/v416vis` : +2,1 cm ; simulation hors moteur des poses : 0,2 cm sous la capsule au plus | `45c6562` |
| J5 | Porte de service ouverte : la boîte d'interaction restait dans l'ouverture et coupait la vue des créatures, le nom des coéquipiers, l'occultation des voix et des sons, le scanner et les apparitions à l'abri des regards (toutes les machines) | la boîte `Box` de `ABRExit` bloque le canal Visibility | `net416_porte` : client tardif « vue par l'ouverture libre NON (Box) » | ouverte, la boîte répond « chevauchement » au canal Visibility ; la visée la trouve par sa tolérance ; fermée, elle bloque ; aucun RPC ni champ répliqué | `fnet_porte` : « vue par l'ouverture libre oui », « porte ouverte visée par l'interaction à 1.2 m : oui » | `2a3ca94` |
| J6 | Poolrooms : la vanne B volait la visée de la vanne A | vanne B à 58 cm de A sur la même face : boîtes de visée chevauchées de 6 cm ; de biais (au-delà de 54°), la sphère de visée touche B d'abord | `v416chap/chapitre1` (`45c6562`), Niveau 37, graine 3053711763 : « Vanne du bassin A atteint mais PAS VISE », mission non résolue | vanne B à 98 cm (34 cm entre les boîtes), vidange entre les volants ; même place pour tous (protocole 416 pas encore distribué), sauvegarde inchangée | hors moteur : `verify_device_clearance` cas G (règle d'avant : 8 arrivées sur 149 volées ; v4.16 : 0) ; en jeu : chapitre (phases 0 à 3) et paquet réussis, **sur d'autres graines** que 3053711763 | `c1c2bde` |
| J7 | Visée entre mécanismes voisins, tous niveaux | `ABRCharacter::UpdateFocus` prenait le premier objet touché par la sphère de 7 cm | **reproduit hors moteur** : banc des mécanismes, section K : 21 paires de voisins à 58 cm (48 graines) ; règle d'avant : 16 paires et **571 points** d'où le voisin prend la visée (Niveau 37, vanne B à 58 cm : 1054) | `ABRCharacter::PickAimedDevice` : parmi les mécanismes touchés, le point visé le plus près de l'axe du regard, un voisin seulement si l'hôte l'accepte ; les tests appellent la même fonction | banc : **0 point** ; `verify_device_clearance` cas H ; en jeu, pas d'étape dédiée : `v412` (accès à pied), chapitre et paquet réussis | `a04a3bb` |
| J8 | Cinématique : Lumen éclairait aux impacts aussi l'éclairage indirect | le jeu demandait `r.Lumen.HardwareRayTracing.LightingMode 1` pour des reflets éclairés ; dans Unreal 5.8, le mode 1 éclaire aussi les sondes d'écran | `base_v416`, Cinématique : Lumen sur la file asynchrone 28,2 ms par image dans les fosses, 31,0 dans les bassins | mode 2 (reflets seulement), `BRPerfHitLightingMode` | `hl2_cine` (§ 6) ; image changée : § 6 | `013541a` |

**Défauts des tests** (aucun effet sur le jeu)

| | Défaut | Avant | Après | Test | Commit |
|---|---|---|---|---|---|
| T1 | Banc visuel : fenêtre remise dans l'image même de la capture | `v416m1/v416vis` : assertion D3D12 d'Unreal, pas de rapport | retour de la fenêtre 1 s après, dans une étape à part | `final416/v416vis` terminé | `b1b19d3` |
| T2 | Banc de lumière : chunks en démontage comptés comme construits | `v416m1/v416light` : « échelle à la trappe prédite 3 / 4 » (PROBLEME) | seuls les chunks prêts et pas en démontage ; trappe visée sous un plafonnier de la grille | `final416/v416light` : 4 / 4, plafonnier sous la trappe exercé | `a165ed6` |
| T3 | Banc des échelles : demi-hauteur du joueur lue à 0 cm pendant le vol du développeur | `v416m1/v416ladder` : 9 NON VERIFIE, puis départ du Level 6 pris pour une échelle disparue (PROBLEME) | capsule du composant ; départ pendant le cas jugé | `final416/v416ladder` réussi | `fefaf20` |
| T4 | Banc des décalques : surfaces bâties par des chunks non construits, borne comptée comme défaut | `v416m2/v416vis` : décalques sans surface (3 au Level 1, 1 au Level 2), 3 points de taches au sol flottants (Level 1), « 2 tirages écartés par la borne » (Level 0) | supports au bord de la zone construite NON VERIFIE ; borne : fonctionnement voulu | `final416/v416vis` : 0 problème | `45c6562` |
| T5 | Test réseau : abandon lu dans les avertissements du journal ; mort officielle jugée dans l'image du coup | `net416_base`, `net416_4j`, `net416_lag` (`45c6562`) : « abandon à terre accepté NON », « mort officielle NON » | compteur `ServerGiveUpsAccepted` ; attente jusqu'à 3 s | `fnet_*` : oui, oui | `b41facc` |
| T6 | Test réseau : actions restantes comptées depuis l'état initial ; départ relevé trop tard ; instant commun au multiple de 10 s précédent | `net416_base` : « sas : 9 actions restantes (attendu 1) », « passerelle : 4 » ; `net416_4j` : « 0 départ annoncé (attendu 1) » | dernière action du solveur sur l'état reçu ; départ relevé dès les demandes ; multiple le plus proche | `fnet_base`, `fnet_4j_b`, `fnet_lag` | `a7397b2` (copie identique `c813020` hors de la branche) |
| T7 | Test réseau : échelles du Niveau 37 jugées sur le chargement d'avant | `net416_classic` : « 0 demande en tout », départ ÉCHEC | chargement retenu à l'arrivée | `fnet_classic` : départ et arrivée au Niveau 2 | `2a3ca94` |
| T8 | Traversée du chapitre : sortie garantie cherchée parmi les sorties visibles seulement | `v416chap2` : après une reprise aux Poolrooms, « sortie ouverte trouvée NON » (graines 3101365319, 3884783009) | `ABRWorld::GetMissionExit` (lecture seule) ; l'étape part vers la sortie de la mission même cachée | `final416/chapitre`, `chapitre2`, paquet : sortie prise après chaque reprise ; les rapports ne disent pas si elle était encore cachée (chemin de la correction non confirmé) | `0ebb8f7` |
| T9 | Marche des tests : blocage par une entité classé « décor » | `final416/full` : Niveau 2, vanne V1 « NON ATTEINT à pied », Smiler à 107 cm (contact des capsules), puis « accès par déplacement réel 4 sur 5 » | `BRTestV412::WalkLikeCharacter` : la capsule du classement glisse le long du décor comme le personnage ; entité seule si l'avance sans entité est libre ; Smiler écarté | hors moteur : `Tools/verify_walk_block.py` (6000 dispositions : règle d'avant « décor » 6000, v4.16 « entité seule » 6000 ; porte, cage et mur restent du décor) ; en jeu : `final416c` | `b5d8c4c` |

**Environnement** : le premier lancement à quatre joueurs (`fnet_4j`) s'est arrêté chez l'hôte sur « Out of video
memory » d'Unreal, alors que la mémoire vidéo utilisée était de 2284 Mo sur 7142 et la mémoire physique du PC de
15 991 Mo sur 16 011 (20 Mo libres) : quatre instances de l'éditeur sur un PC de 16 Go. Rejoué (`fnet_4j_b`) : 0
problème.

## 3. Résultats en jeu

Commandes (depuis `Backrooms/`, une suite par lancement, aucun autre Unreal ouvert) :

```
# Solo (rapport dans Saved/AutoTest) :
#   UnrealEditor.exe "<Backrooms>\Backrooms.uproject" -game -windowed -ResX=1366 -ResY=768 -unattended -nosplash -log <drapeaux>
#   ou Saved\TestRuns\run_autotests.ps1 -Only <suite> -Tag <etiquette>   (lanceur hors depot)
# Reseau v4.16 : powershell -ExecutionPolicy Bypass -File Tools\Tests\run_nettest.ps1 <options> -Tag <etiquette>
# Mesures :      powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Tag <etiquette> -Runs 3
# Paquet :       powershell -ExecutionPolicy Bypass -File Tools\Build\build_windows.ps1 -UERoot "C:\Program Files\Epic Games\UE_5.8" -Chapter -ChapterOut <dossier>
```

Passage final `final416` : commit `6d39fe8`, 20 octobre de 02:10 (début) à 05:10 ; fenêtre 1366 × 768, profil Qualité
(rendu 80 %). La colonne v4.15 rappelle le dernier passage de la base (`autotest/final4`, code de `aada32b`).

| Suite | Drapeaux | v4.15 (`final4`) | v4.16 (`final416`, `6d39fe8`) | Non vérifiés : raisons |
|---|---|---|---|---|
| `chapitre` | `-BRAutoTest -BRAutoTestChapter -BRAutoTestStrict` (un lancement) | — | réussi, 0 / 0 | — |
| `chapitre1` | `... -BRChapterPhase=1` | — | réussi, 0 / 0 | — |
| `chapitre2` | `... -BRChapterPhase=2` | — | réussi, 0 / 0 | — |
| `chapitre3` | `... -BRChapterPhase=3` | — | réussi, 0 / 0 | — |
| `v416light` | `-BRAutoTest -BRAutoTestV416Light -BRAutoTestStrict` | — | 0 problème, 2 non vérifiés | Niveau 9 hors du banc ; aucune vraie porte de mission sur la grille d'un niveau à plafonniers |
| `v416ladder` | `... -BRAutoTestV416Ladder ...` | — | réussi, 0 / 0 | — |
| `v416vis` | `... -BRAutoTestV416Visual ...` | — | 0 problème, 7 non vérifiés | § 2.8 |
| `v416ship` | `... -BRAutoTestV416Ship ...` | — | réussi, 0 / 0 | — |
| `v416lidar` | `... -BRAutoTestV416Lidar ...` | — | réussi, 0 / 0 | — |
| `smoke` | `-BRAutoTest -BRSmokeTest -BRAutoTestStrict` | réussi | réussi | — |
| `v415` | `... -BRAutoTestV415 ...` | réussi | réussi (protocole 416, version affichée 4.16) | — |
| `v415fix` | `... -BRAutoTestV415Fix ...` | 0 problème, 2 non vérifiés | 0 problème, 1 non vérifié | Niveau 2 : aucun couloir droit et sombre pour la marche avec un Smiler |
| `v414` | `... -BRAutoTestV414 ...` | 0 problème, 3 non vérifiés | 0 problème, 3 non vérifiés | perte du focus à vérifier à la main ; 144 et 240 img/s non atteintes sur cette machine |
| `v413` | `... -BRAutoTestV413 ...` | réussi | réussi | — |
| `v412` | `... -BRAutoTestV412 ...` | réussi | réussi | — |
| `v411` | `... -BRAutoTestV411 ...` | 0 problème, 1 non vérifié | 0 problème, 1 non vérifié | volume général lisible seulement sur le fil audio (Unreal 5.8) |
| `v410` | `... -BRAutoTestV410 ... -BRSeed=4242` | 0 problème, 2 non vérifiés | 0 problème, 2 non vérifiés | un seul écran : fenêtre sur un autre écran, choix de l'écran |
| `v49`, `v48`, `v47` | `-BRAutoTestV49`, `V48`, `V47 -BRSeed=4242` | réussis | réussis | — |
| `pits` | `-BRAutoTest -BRAutoTestPits -BRSeed=9` | échec, 1 (22 img/s en Cinématique, fenêtre 1536 × 960) | réussi (38 img/s en Cinématique, fenêtre 1366 × 768 : tailles différentes, § 6) | — |
| `visual` | `-BRAutoTest -BRAutoTestVisual` | réussi | réussi | — |
| `full` | `-BRAutoTest -BRAutoTestStrict -BRSeed=4242` | échec, 1 (img/s des fosses), 6 non vérifiés | **échec, 2 problèmes**, 7 non vérifiés : marche du test bloquée par un Smiler au Niveau 2 et sa conséquence (défaut du test T9) | les 6 de `final416b` ; Niveau 2 : sortie non testée (mission non résolue à pied) |
| `final416b/full` | test complet rejoué, même commit `6d39fe8` | — | **0 problème**, 6 non vérifiés ; accès par déplacement réel 5 niveaux sur 5 | un seul écran (2) ; volume général (fil audio) ; perte du focus ; 144 et 240 img/s non atteintes |
| `final416c` : `v412`, `v415fix`, `chapitre`, `chapitre1` à `3`, `full` | mêmes drapeaux, commit `b5d8c4c` | — | **v412, chapitre (un lancement), chapitre1, chapitre2, chapitre3 : réussis, 0 problème, 0 non vérifié** ; `full` : 0 problème, 6 non vérifiés ; `v415fix` : 0 problème, 1 non vérifié. Chapitre : Niveau 2, vanne V1 atteinte, « 1 entité(s) écartée(s) par le test (Smilers …) », mission résolue | `full` : les 6 de `final416b` ; `v415fix` : aucun couloir droit et sombre pour la marche avec un Smiler |

Dans le paquet Shipping (`b5d8c4c`, § 5) : test de démarrage réussi ; chapitre en trois phases : 0 problème, 0 non
vérifié chacune.

Ce que montrent les suites du chapitre (`final416/chapitre`, un lancement) : NOUVELLE PARTIE par le vrai menu, porte de
service atteinte à pied (92,2 m en 36 s) et ouverte, arrivée au Niveau 1, partie relue du disque à chaque arrivée,
fermeture aux Poolrooms résolues et reprise (même graine, même état de mission), Niveau 2, fin du chapitre écrite par le
jeu et relue, CONTINUER après la fin, parties de test effacées. Graines du parcours tirées à chaque lancement (phase 0 :
4272009673 ; phases 1 à 3 : 1861319209) : le rapport donne `-BRChapterSeed` et `-BRChapterIntro` pour rejouer une
disposition.

**Contrôles hors moteur** du passage final (`Saved/TestRuns/v416/final/`, commit `6d39fe8`) :

| Contrôle | Résultat |
|---|---|
| `python Tools/run_benches.py` | **OK (12 bancs × 3 compilateurs)** : GCC 16.2.0, clang-cl 22.1.3, MSVC ; contenu 70, couleur 24, néons 20, mécanismes 86 vérifications (les autres bancs ne donnent pas leur compte dans ce résumé) |
| langues | `loc_extract.py --check` à jour ; `loc_build.py` : 1215 clés traduites dans chaque langue ; `loc_review.py` : 0 problème |
| `protect_assets.py check` | OK (`Tools/SourceModels` absent : originaux non vérifiés) |
| outils de paquet | 83 tests, OK (1 sauté : cas Linux sur un hôte Windows) |
| `test_perf_summary.py` | 15 tests, OK |
| `verify_pitfalls.py --doors 1-500` | 500 graines, 0 problème |
| `verify_lidar.py --seeds 1-200` | OK (0 échec) |
| `verify_device_clearance.py` | OK (A à H ; G : vannes à 58 cm, H : visée au premier contact, échouent avec la règle d'avant) |
| `verify_level0_kit.py` | 40 graines sans défaut |

Sur `b5d8c4c` (message du commit et `build_editor_walk.txt`) : `BackroomsEditor Win64 Development` réussi, 0
avertissement ; `run_benches` 12 × 3 OK ; `verify_device_clearance`, `verify_pitfalls`, `verify_walk_block` : OK. La
cible Shipping a été compilée sans avertissement du compilateur par la construction du paquet (§ 5). Ces contrôles ne
valident ni les collisions, ni le rendu, ni le réseau du moteur.

## 4. Réseau

Toutes les machines sur ce PC (`Tools\Tests\run_nettest.ps1`), commit `6d39fe8`, 20 octobre de 05:14 à 07:06
(`fnet_4j_b` : 08:14), fenêtres de 1280 × 720, Level 0 à la graine 9. Code du lanceur : 0 tout réussi, 1 un problème,
2 seulement des non vérifiés, 3 un rapport manquant.

| Étiquette | Options | Machines | Résultat | Non vérifiés : raisons | Avant (passages intermédiaires) |
|---|---|---|---|---|---|
| `fnet_base` | `-Clients 1` | hôte, client | 0 problème (code 2) | hôte 4 : concurrence (ramassage, départ, sas) impossible avec moins de 2 clients ; second écran, deuxième machine, quatre joueurs physiques ; client 1 : perte de focus (la fenêtre réduite reste active pour Slate) | `net416_base` (`45c6562`) : 2 + 2 problèmes, défauts des tests T5, T6 |
| `fnet_4j` | `-Clients 3 -NullRhiClients` | hôte, 3 clients (2 sans rendu) | **code 3** : hôte arrêté par Unreal (mémoire du PC pleine, § 2.9), Client3 jamais entré, Client1 et 2 : partie terminée | environnement, pas un résultat du jeu | `net416_4j` : 3 + 2 + 1 problèmes (T5, T6) |
| `fnet_4j_b` | même chose, rejoué | hôte, 3 clients | **0 problème** (code 2) ; 3 clients réussis | hôte 1 : matériel absent (second écran, deuxième machine, quatre joueurs physiques) | |
| `fnet_porte` | `-Clients 1 -LateJoin porte` | hôte, client, tardif | **réussi** (code 0) | — | `net416_porte` : tardif, « vue par l'ouverture libre NON (Box) » : **défaut du jeu J5** |
| `fnet_lidar` | `-Clients 1 -LateJoin lidar` | hôte, client, tardif | réussi | — | |
| `fnet_passage` | `-Clients 1 -LateJoin passage` | hôte, client, tardif | réussi | — | |
| `fnet_reco` | `-Clients 1 -LateJoin reconnexion` | hôte, client | réussi | — | |
| `fnet_lag` | `-Clients 1 -Lag 150 -Jitter 40 -Loss 3` | hôte, client | 0 problème (code 2) ; client réussi | hôte 4 : comme `fnet_base` | `net416_lag` : 2 + 2 problèmes (T5, T6) |
| `fnet_reco_lag` | `-Clients 1 -LateJoin reconnexion -Lag 150 -Jitter 40 -Loss 3` | hôte, client | réussi | — | |
| `fnet_classic` | `-Classic` (plan `-BRNetTest` ordinaire, étapes v4.16 des échelles et empreintes de la lumière) | hôte, client | 0 / 0 problème (code 2) | hôte 3 : client tardif (porte, Level 6) et quatre joueurs non reproductibles dans ce plan (couverts par le plan v4.16) ; état de la porte reçu après la construction du niveau (à provoquer sous forte latence) ; client 2 : les deux sur la porte | `net416_classic` : 2 + 2 problèmes (T7) ; v4.15 `net_final4` : 0 / 0, 4 et 3 non vérifiés |

Lignes relevées (`fnet_4j_b` sauf mention) :

- ramassage simultané à 4 joueurs : 1 attribution, objet retiré pour tous ; « 2 demande(s) de clients reçues au même
  instant du serveur (attendu 3) » : la ligne n'est pas comptée comme problème, mais une des trois demandes n'est pas
  arrivée au même instant que les autres ; départ simultané : 3 demandes au même instant, 1 départ annoncé, joueurs attendus 4, un chargement ;
- sas loin de l'hôte : même action demandée par 2 clients au même instant du serveur : retours 11 et 11, état de la
  mission celui des deux demandes appliquées l'une après l'autre ; mission résolue par l'action du client ;
- morts et réveils (C1 à C3) : § 2.7 ; abandon d'un client : § 2.9, J1 ; équipe à terre puis réveils : en chargement chez
  l'hôte non, position d'avant le placement non, chutes constatées depuis 0 ; chute d'un client dans une fosse : mort vue,
  réveil hors des fosses, aucune seconde chute ;
- « coup mortel sur le client lointain (à 230 m), mort officielle NON » à l'étape C1 : ligne d'information de l'image du
  coup, non jugée (l'étape C3 attend jusqu'à 3 s : « mort officielle oui ») ;
- inventaire, pause, touche ajoutée pendant l'appui, fenêtre réduite par le test : scan, son, course et interaction
  arrêtés, aucune reprise sans nouvel appui ;
- entre machines : début, gagnant du ramassage, niveau du départ, sas, passerelle : identiques sur les 4 machines ;
- clients tardifs : Level 6, même plan (`992d0eb0`) et même état de mission, rien ne recommence (`fnet_lidar`) ;
  porte de service construite, ouverte, mur percé, vue et passage libres (`fnet_porte`) ;
- latence simulée appliquée et relue : « latence 150 ms, gigue ±40 ms, pertes 3 % » chez l'hôte et le client
  (`fnet_lag`, `fnet_reco_lag`).

Non couvert : deux PC, réseau réel, quatre joueurs physiques, second écran (§ 8).

## 5. Paquet Shipping et plateformes

**Windows 64 bits, paquet Shipping** (`Saved/TestRuns/v416/final/package.txt`, rapports des phases dans
`chapitre_paquet/`) : construit le 20 octobre entre 08:15 et 08:18 par `build_windows.ps1 -Chapter` (BuildCookRun
176 s, « BUILD SUCCESSFUL », compilation Shipping sans avertissement du compilateur), contenu public, dans
`Build/Windows/Shipping`. Manifeste `build_manifest.txt` : commit **`b5d8c4c`**, « arbre : modifié » ; le seul fichier
suivi modifié est `Config/DefaultEngine.ini` (section `AndroidFileServerRuntimeSettings` ajoutée à 08:16, pendant la
construction, non commitée ; `bIncludeInShipping=False`). Le code du jeu de `b5d8c4c` est celui de `6d39fe8` (`b5d8c4c` ne change que la marche des
tests et un outil Python).

| Vérification | Résultat |
|---|---|
| structure | OK : `Backrooms-Win64-Shipping.exe` (161 Mo), 1 `.pak`, 2 `.utoc` |
| contenu | OK : 4604 fichiers dans les `.pak`, 1693 entrées IoStore |
| architecture, dépendances | OK : x86_64 ; 41 bibliothèques importées, 13 chargées à la demande |
| lancement | OK : code 0, réussi, 0 problème, 0 non vérifié |
| chapitre, phase 1 | code 0 : `0 → 1 → 37`, fermeture aux Poolrooms (graine du parcours 3494430059, Level 0 14197508 ; 1280 × 720) |
| chapitre, phase 2 | code 0 : reprise après redémarrage, Niveau 2, fin du chapitre atteinte et enregistrée |
| chapitre, phase 3 | code 0 : partie après un nouveau redémarrage, CONTINUER au Niveau 2, nettoyage |
| **résultat** | **« RESULTAT : PAQUET VERIFIE ET LANCE, CHAPITRE TRAVERSE »**, code 0 |

Non essayé dans le paquet : le refus d'un autre plan de test (§ 2.7, D2) ; l'effet du lancement sur le fichier de
réglages du joueur.

| Plateforme | État |
|---|---|
| Windows 64 bits | paquet Shipping construit, contrôlé, lancé, premier chapitre traversé (ci-dessus) |
| Linux | **bloqué** : l'installation du moteur n'a pas `Engine/Platforms/Linux` (dossier `Engine/Platforms` : Android, IOS, VisionOS, Windows). Le propriétaire doit ajouter la plateforme cible Linux dans l'Epic Games Launcher (UE 5.8 > Options > Plateformes cibles > Linux). Chaîne identifiée dans `Engine/Config/Linux/Linux_SDK.json` : `v26_clang-20.1.8-rockylinux8`, **non installée** (`LINUX_MULTIARCH_ROOT` vide). WSL non installé (changement du système laissé au propriétaire). Rien n'a été construit ni lancé. |
| macOS | **impossible sur cette machine** (pas de Mac) : rien n'a été construit ni lancé |
| Steam | `Tools/Steam/make_steam_vdf.py` et `upload_steam.sh` présents ; **rien n'a été envoyé, rien n'a été publié** |

Aucune compatibilité Linux ou macOS n'est annoncée : la présence de scripts et de fichiers de configuration ne la prouve
pas.

## 6. Performance

Toutes les mesures : RTX 5050 Laptop (pilote 610.88), i7-13620H, fenêtre **1366 × 768**, `-BRAutoTestPerf`, graines
de référence (une par scène), mesures instables ou sans scénario exclues (aucune ici). **Aucune mesure sur la machine
de référence (RTX 4080 + 7800X3D).** Les chiffres de la v4.15 (fosses à 21–22 img/s en Cinématique) venaient d'une
fenêtre de 1536 × 960 : ils ne se comparent pas à ceux-ci. Plusieurs graines : le profil Cinématique (le seul que le
changement de rendu touche) est mesuré sur deux graines par scène, ancien et nouveau mode, sur le même binaire (§ 6.2 bis) ;
les autres profils sur une seconde graine (`seeds_all_final`, 1 exécution, pour mémoire).

### 6.1 Base v4.16, profils sans changement (`perf/base_v416`, 3 exécutions)

Commit `2a3ca94` avec un arbre modifié (noté par `perf_summary.py`). Le changement suivant du rendu (`013541a`) ne
touche que les reflets éclairés aux impacts (profil Cinématique) : ces lignes valent pour la tête. Longues images :
nombre pour 1000 images au-delà de 33 / 50 / 100 ms.

| Scène | Profil (rendu interne) | Médiane (ms) | 95 % | Pire 99 % | > 33 / 50 / 100 ms ‰ | GPU (ms) | VRAM (Mo, pic) |
|---|---|---|---|---|---|---|---|
| fosses (Level 0) | Performance (915 × 515) | 9,28 | 10,84 | 15,85 | 0 / 0 / 0 | 4,16 | 2578 (2636) |
| fosses | Qualité (1093 × 614) | 10,64 | 13,05 | 16,59 | 0 / 0 / 0 | 8,71 | 2513 (2517) |
| fosses | RTX fluide (915 × 515) | 9,78 | 12,20 | 14,19 | 0 / 0 / 0 | 7,19 | 2513 (2514) |
| bassins (Poolrooms) | Performance | 10,64 | 13,04 | 17,87 | 0 / 0 / 0 | 5,75 | 2581 (2638) |
| bassins | Qualité | 12,61 | 16,17 | 18,12 | 0 / 0 / 0 | 10,65 | 2548 (2550) |
| bassins | RTX fluide | 11,52 | 14,71 | 21,34 | 0 / 0 / 0 | 8,68 | 2520 (2521) |
| marche0 (Level 0) | Performance | 9,33 | 11,71 | 16,41 | 0 / 0 / 0 | 3,99 | 2577 (2638) |
| marche0 | Qualité | 10,66 | 13,48 | 15,88 | 0 / 0 / 0 | 8,40 | 2536 (2538) |
| marche0 | RTX fluide | 10,43 | 13,01 | 14,96 | 0,4 / 0 / 0 | 6,99 | 2510 (2511) |
| marche1 (Niveau 1) | Performance | 8,90 | 11,65 | 14,22 | 0,2 / 0,2 / 0 | 4,13 | 2578 (2638) |
| marche1 | Qualité | 10,65 | 13,99 | 16,65 | 0 / 0 / 0 | 8,67 | 2570 (2571) |
| marche1 | RTX fluide | 10,30 | 13,14 | 15,71 | 0 / 0 / 0 | 7,13 | 2511 (2513) |
| lidar6 (Level 6) | Performance | 7,93 | 10,20 | 11,73 | 0 / 0 / 0 | 3,60 | 2563 (2623) |
| lidar6 | Qualité | 9,22 | 11,26 | 13,09 | 0 / 0 / 0 | 7,01 | 2641 (2641) |
| lidar6 | RTX fluide | 8,91 | 11,29 | 13,56 | 0 / 0 / 0 | 5,82 | 2614 (2616) |

### 6.2 Cinématique avant / après le mode 2 de Lumen (`base_v416` → `hl2_cine`, 3 exécutions chacune)

Rendu interne 1366 × 768 (100 %) des deux côtés ; `hl2_cine` mesuré sur `2a3ca94` avec la modification devenue
`013541a` (arbre modifié).

| Scène | Médiane (ms) | 95 % | Pire 99 % | > 33 ms ‰ | > 50 ms ‰ | GPU (ms) | VRAM (Mo) | Lumen, file asynchrone (ms) |
|---|---|---|---|---|---|---|---|---|
| fosses | 34,70 → **25,05** | 37,78 → 27,57 | 41,14 → 29,17 | 791,0 → 0,0 | 0 → 0 | 33,59 → 23,91 | 3106 → 2864 | 28,22 → 18,28 |
| bassins | 39,57 → **31,70** | 43,20 → 34,83 | 45,81 → 37,30 | 988,1 → 140,0 | 5,3 → 0 | 38,10 → 30,26 | 3268 → 3041 | 30,98 → 23,35 |
| marche0 | 31,66 → **24,84** | 36,14 → 27,67 | 40,20 → 30,41 | 252,1 → 3,7 | 0,5 → 0 | 30,60 → 23,63 | 3092 → 2896 | 24,40 → 17,88 |
| marche1 | 30,42 → **25,50** | 35,52 → 29,21 | 39,13 → 32,33 | 178,2 → 3,0 | 2,0 → 0,4 | 29,03 → 24,39 | 3124 → 2897 | 23,57 → 19,43 |
| lidar6 | 22,93 → **19,15** | 25,50 → 21,61 | 27,24 → 23,59 | 0 → 0 | 0 → 0 | 21,88 → 18,06 | 3059 → 2865 | 16,59 → 12,87 |

Fosses : 28,8 → 39,8 img/s ; aucune image au-delà de 100 ms avant comme après. Dans le test `pits` du passage final
(`6d39fe8`, autre test, même fenêtre) : Cinématique 38 img/s, médiane 25,9 ms.

### 6.2 bis Deux graines par scène, même binaire (`seeds_cine_mode1` → `seeds_cine_mode2`, commit `4bc75ad`)

Ancien mode rendu par `-BRPerfCmds="r.Lumen.HardwareRayTracing.LightingMode 1"` (appliqué après le profil), nouveau mode
par défaut ; graines `0` et `4242` du banc, 2 exécutions par graine et par mode, arbre propre ; résumé
`perf/cmp_seeds_cine_parseed.md` (`perf_summary.py --by-seed`).

| Scène | Graine du niveau | Médiane (ms) | 95 % | Pire 99 % | > 33 ms ‰ | GPU (ms) |
|---|---|---|---|---|---|---|
| fosses | 1896875771 | 34,02 → **24,77** (−27,2 %) | −9,08 | −8,69 | 619,3 → 0,0 | −8,91 |
| fosses | 2271519799 | 33,37 → **24,01** (−28,0 %) | −9,59 | −10,54 | 512,5 → 1,2 | −9,11 |
| bassins | 178662963 | 38,54 → **31,06** (−19,4 %) | −7,37 | −8,14 | 942,4 → 172,4 | −7,18 |
| bassins | 2772684265 | 40,28 → **31,61** (−21,5 %) | −8,00 | −6,96 | 994,0 → 247,6 | −8,47 |
| marche0 | 1896875771 | 31,22 → **24,79** (−20,6 %) | −8,00 | −8,90 | 251,4 → 1,2 | −6,22 |
| marche0 | 2271519799 | 33,01 → **25,31** (−23,3 %) | −10,75 | −12,75 | 461,5 → 0,0 | −7,40 |
| marche1 | 814216900 (les deux graines du banc donnent ce niveau) | 29,94 → **25,45** (−15,0 %) | −6,28 | −6,81 | 141,8 → 2,2 | −4,32 |
| lidar6 | 229355065 | 21,82 → **19,15** (−12,2 %) | −3,43 | −3,40 | 0 → 0 | −2,68 |
| lidar6 | 2702236446 | 21,61 → **18,97** (−12,2 %) | −3,36 | −5,54 | 2,7 → 0 | −2,71 |

Le gain ne dépend pas de la graine (écart entre graines de la même scène : au plus 2 points). Les bassins restent la
scène la plus lourde en Cinématique (≈ 31 ms, 32 img/s sur cette machine).

### 6.3 Coût dominant (relevés ProfileGPU de `base_v416`)

- **Performance** : GPU de 3,6 à 5,8 ms pour une médiane de 7,9 à 10,6 ms ; postes principaux : ombres virtuelles (1,05
  ms dans les fosses, 1,34 dans les bassins), post-traitement (0,76), Lumen logiciel sur la file asynchrone (1,6 à
  2,4 ms).
- **Qualité** : GPU de 7,0 à 10,7 ms ; MegaLights 1,8 à 2,4 ms sur la file graphique (0 au Level 6, sans lampes), Lumen
  matériel 4,2 à 7,3 ms sur la file asynchrone ; Poolrooms : surface de l'eau 0,84 + 1,11 ms.
- **RTX fluide** : GPU de 5,8 à 8,7 ms ; MegaLights 1,2 à 1,7 ms, Lumen asynchrone 3,4 à 5,7 ms.
- **Cinématique** : Lumen sur la file asynchrone 16,6 à 31,0 ms (dont `DiffuseIndirectAndAO` 25 ms dans les fosses,
  d'après `013541a`), MegaLights 5,8 à 8,2 ms (hors Level 6), brouillard 1,6 à 2,3 ms, Lumen graphique 9,48 ms dans les
  bassins. Après le mode 2 : Lumen asynchrone 12,9 à 23,4 ms, MegaLights 5,6 à 8,0 ms (poste suivant), brouillard 1,6 à
  2,2 ms.
- Hors Cinématique, le GPU reste sous la médiane de l'image, et le relevé « rendu » du thread de rendu vaut à peu près
  cette médiane (ex. fosses en Performance : 9,25 ms pour 9,28) : ce qui limite ces profils sur cette machine n'est pas
  décomposé par ces relevés (travail du thread de rendu ou attente).

### 6.4 Streaming et scanner

- **Streaming** (marche de 93,9 m) : Niveau 1 (graine du niveau 814216900) : 6 à 7 chunks construits (112,9 à 269,6 ms
  de travail en tout, le plus en Cinématique ; 225,2 après le mode 2), 4 démontés (6,6 à 12,3 ms), 18 à 22 composants
  par seconde ; Level 0 : 0 construit, 5 démontés (11,3 à 18,7 ms). Partout : 0 chargement synchrone, 0 saccade de PSO ;
  colonne « sols sur la position prédite » à 0. Chargement 3,1 à 3,7 s, préchauffage 8,0 à 8,7 s (notés à part).
- **Scanner** (Level 6, scan maintenu) : 2042 à 2043 rayons par seconde, pic de 14 747 à 14 953 instances, 80,8 à 163,5
  mises à jour de matériaux par seconde, 0 inscription par seconde (aucune plaque en vue dans cette scène) ; temps du
  scanner sur le processeur 0,208 ms par image (Performance) à 0,573 ms (Cinématique ; 0,523 après le mode 2). Le coût
  GPU des instances n'apparaît pas comme poste propre. Débit à 144 et 240 img/s : non jugé (cadences non atteintes).

### 6.5 Essais sans gain (non appliqués)

`-BRPerfCmds`, 2 exécutions par côté, témoin du même binaire, commit `c1c2bde` (arbre modifié) :

| Essai | Profil | Fosses (médiane) | Bassins (médiane) |
|---|---|---|---|
| `r.Lumen.Reflections.MaxRoughnessToTrace 0.4` (`x_rtx_ctrl` → `x_rtx_rough`) | RTX fluide | 9,60 → 9,83 ms (+2,4 %) | 10,98 → 10,76 ms (−2,0 %) |
| `r.Lumen.ScreenProbeGather.TracingOctahedronResolution 8` (`x_cine_ctrl` → `x_cine_oct8`) | Cinématique | 24,49 → 25,45 ms (+3,9 %) | 31,44 → 31,80 ms (+1,1 %) |

Dans le bruit ou plus lent : rien n'est appliqué. Ombres des fosses, eau des Poolrooms, étalement du streaming et coût
de rendu du scanner (ordre du prompt § 7) : **mesurés, pas optimisés**.

### 6.6 Cinématique : ce que change le mode 2, et comment revenir

- Le mode 2 éclaire aux impacts les **reflets seulement** ; l'éclairage indirect revient aux sondes d'écran ordinaires :
  l'image change. Comparaison des captures du banc visuel en Cinématique (`autotest/hlcmpc` : `visualc` en mode 2,
  `visualchl1` en mode 1 ; planches `cmp_lit.png` et `cmp_vis_0_Cinematique_*.png`), faite par l'orchestrateur sur les
  captures : la plupart des poses entre 35 et 54 dB de PSNR ; poses éclairées du Level 0 **13 à 23 % plus lumineuses en
  mode 2** ; poses de coupure non comparables (exposition automatique et places des Smilers différentes d'un lancement à
  l'autre). Seuil d'images par seconde du banc : 1 dépassement en mode 2 (Niveau 37, 30 img/s), 7 en mode 1 (24 à
  30 img/s). **Le jugement de l'image revient au propriétaire.**
- Pour mesurer l'ancien mode sans recompiler (hors version publiée) : `-BRHitLightingMode=1`.
- Pour revenir définitivement : fonction `BRPerfHitLightingMode()` de `Source/Backrooms/Private/BRPlayerController.cpp` :
  la version publiée ne lit que sa ligne `return 2;` (à passer à `return 1;`) ; hors version publiée, la valeur par
  défaut 2 de la même fonction.

## 7. Tests lancés après la dernière correction, sur un commit antérieur, restant à faire

**Après la dernière correction du jeu, sur `6d39fe8`** (le dernier changement du comportement du jeu est `a04a3bb` ;
`0ebb8f7` ajoute une fonction de lecture pour le test ; `6d39fe8` ne change que des documents) :

- les 23 lancements solo de `final416` (§ 3) et le test complet rejoué `final416b/full` ;
- le test réseau : `fnet_base`, `fnet_4j` (environnement), `fnet_4j_b`, `fnet_porte`, `fnet_lidar`, `fnet_passage`,
  `fnet_reco`, `fnet_lag`, `fnet_reco_lag`, `fnet_classic` (§ 4) ;
- hors moteur : bancs 12 × 3, langues, `protect_assets`, outils de paquet, `test_perf_summary`, `verify_pitfalls`,
  `verify_lidar`, `verify_device_clearance`, `verify_level0_kit` (§ 3).

**Sur `b5d8c4c`** (la tête ; seuls la marche des tests `BRAutoTestV412.cpp` et `BRAutoTestWalk.h`, et l'outil
`verify_walk_block.py`, changent) :

- compilation de l'éditeur (0 avertissement), `run_benches` 12 × 3, `verify_device_clearance`, `verify_pitfalls`,
  `verify_walk_block` (message du commit) ;
- le paquet Shipping et sa traversée du chapitre (manifeste `b5d8c4c`, § 5) ;
- `final416c` : v412, chapitre en un lancement, chapitre1, chapitre2, chapitre3 réussis (0 problème, 0 non vérifié) ;
  `full` 0 problème, 6 non vérifiés ; `v415fix` 0 problème, 1 non vérifié.

**Sur `4bc75ad`** (documents seulement ; code du jeu de `b5d8c4c`) :

- `fnet_v416lidar` : test réseau avec `-BRAutoTestV416Lidar` : 0 problème chez l'hôte et le client ;
- mesures `seeds_cine_mode1`, `seeds_cine_mode2` (Cinématique, deux graines, deux modes) et `seeds_all_final` (§ 6.2 bis).

**Sur un commit antérieur** :

- mesures de performance : `base_v416` et `hl2_cine` sur `2a3ca94` (arbre modifié ; `hl2_cine` porte la modification
  devenue `013541a`), essais `x_*` sur `c1c2bde` (arbre modifié). Après `2a3ca94`, seul `013541a` touche au rendu
  (compris dans `hl2_cine`) ; `c1c2bde` déplace une vanne des Poolrooms, la scène « bassins » n'a pas été remesurée
  ensuite ;
- comparaison des modes de Lumen `hlcmpc` : rapports du 20 octobre, 01:08 et 01:12, après `013541a` ;
- scanner : `v416a` (`822d39f`), `v416b` (`2e78cbe`), `v416c` (`425eb01`) en solo, remplacés par `final416/v416lidar` ;
  `net_v416lidar` (`822d39f`), test réseau avec `-BRAutoTestV416Lidar`, relancé depuis sur `4bc75ad` ;
- passages intermédiaires qui ont montré les défauts du § 2.9 : `v416m1` (`600dc94`), `v416m2` (`b1b19d3`), `v416m3`,
  `v416m4`, `v416chap` (binaires de `45c6562`), `v416chap2`, `net416_*` (`45c6562`), `net416b_*` : preuves de l'« avant »,
  pas des validations ;
- pour mémoire, base v4.15 (`final4`, `net_final4`, code de `aada32b`) : référence d'avant.

**Restant à faire** :

- une optimisation au-delà du mode de Lumen : en RTX fluide aucun poste ne domine aux Poolrooms (≈ 90 img/s ici), le
  streaming et le scanner restent dans leur budget, et les deux essais du § 6.5 n'ont rien gagné ; rien n'est mesuré à
  1440p ni sur la machine de référence ;
- le jugement à l'œil des captures du banc visuel et des planches `hlcmpc` ;
- le refus d'un autre plan de test par le paquet Shipping, à la main ;
- la traversée manuelle d'une échelle depuis le canal, par une personne ;
- les essais sur le matériel absent et la machine de référence (§ 8) ; Linux après installation de la plateforme ;
- la décision d'équilibrage de la lumière (§ 9) ;
- [`GAMEPLAY_v4.16.md`](GAMEPLAY_v4.16.md) décrit les changements pour le joueur (écrit après ce rapport, à partir de
  lui).

## 8. Ce qui n'est pas vérifié

- **Machine de référence** (RTX 4080 + Ryzen 7 7800X3D) : aucune mesure ; aucun résultat du portable ne la prédit.
- **Linux, macOS** : rien construit ni lancé (§ 5). **Steam** : rien envoyé.
- **Vraie manette** : tous les appuis sont injectés par les tests ; aucune manette physique essayée. À la manette,
  déplacer un objet d'une case à l'autre (glisser-déposer) n'a pas d'équivalent décrit dans les commits ni essayé.
- **Matériel absent** : second PC (réseau réel ; tout le test réseau tourne sur ce PC), **quatre joueurs physiques** (les
  quatre instances tournent sur ce PC, deux sans rendu), **second écran** (un seul écran branché : NON VERIFIE de `v410`
  et du test complet), ultrawide réel (rendus hors écran en 2560 × 1080 ; vraie fenêtre la plus large : 1520 × 641).
- **Plein écran exclusif** : non essayé (seulement avec `-BRV416PleinEcran`).
- **Vue d'un coéquipier chez un client** : le banc visuel ne montre que la troisième personne locale.
- **Perte de focus** : NON VERIFIE chez le client de `fnet_base` (la fenêtre réduite reste active pour Slate) et dans
  `v414` ; vérifiée chez l'hôte de `fnet_4j_b`.
- **Montée d'une échelle depuis le canal vers le trottoir par une personne** (montée, descente, rassemblement) : l'essai
  automatique avec entrées injectées a réussi (§ 2.4) ; pas d'essai humain.
- **Lumière** : rebond non compté ; objets, trottoirs du 37, battants et autres composants hors de la grille (au test, la
  physique seule cache 152 paires sur 6646, § 2.1) ; plafonnier derrière une vraie porte de mission (crochet de test seulement) ; Niveau 9 hors
  du banc ; équilibrage en partie réelle (§ 9).
- **Scanner** : coût de rendu des instances sur le GPU ; boîte de visée des cages (pas d'étape dédiée) ; débit à 144 et
  240 img/s.
- **Coopération** : chemin du défaut 7 (personnage reçu après le fondu) jamais emprunté ; latence et pertes réelles
  (simulées par le moteur seulement).
- **Visée** : le cas exact de la graine 3053711763 (vanne B) n'a été rejoué que hors moteur ; la visée entre voisins
  (`a04a3bb`) n'a pas d'étape dédiée en jeu. **Chapitre** : le chemin « sortie encore cachée » de `0ebb8f7` n'est pas
  confirmé par les rapports.
- **Paquet** : refus des autres plans ; effet du lancement du paquet sur le fichier de réglages du joueur ; R1 (écriture
  de l'arrivée pendant le fondu).
- **Finition** : poses, appui des pieds et raccords (seulement par les captures, non examinées ici) ; image de la
  Cinématique en mode 2 (§ 6.6).
- **Traductions** : les nouveaux textes (21 langues) sont générés et marqués à relire ; `protect_assets` ne vérifie pas
  les originaux (`Tools/SourceModels` absent).

## 9. Équilibrage de la lumière (décision du propriétaire)

La correction du § 2.1 ne fait qu'enlever de la lumière (jamais plus qu'avant) : les Levels 0 à 3 deviennent nettement
plus sombres pour les règles.

**Hors moteur** ([`banc_lumiere.txt`](v416/banc_lumiere.txt), section C : grille du jeu rejouée sur 300 graines du
Level 0 et 60 graines des autres niveaux ; courant plein, plafonniers seuls ; un point au hasard par cellule praticable,
à 40 cm des murs, aux hauteurs du joueur, du Smiler et du Deathmoth) :

| Niveau | Positions changées | Apparition possible (cellules) | Smiler dissipé | Noir (santé) | Deathmoth non attiré | Bacteria ralentie | Santé mentale, 100 points sans lampe |
|---|---|---|---|---|---|---|---|
| 0 | 73,4 % | 14,4 → 29,0 % | 45,9 → 36,7 % | 16,0 → 31,3 % | 42,6 → 34,4 % | 19,1 → 34,7 % | 14,5 → 11,2 min |
| 1 | 60,4 % | 22,1 → 32,5 % | 38,6 → 31,7 % | 23,4 → 33,5 % | 35,4 → 29,5 % | 26,9 → 37,0 % | 11,0 → 9,6 min |
| 2 | 68,8 % | 34,0 → 56,8 % | 27,7 → 18,4 % | 34,1 → 57,0 % | 24,8 → 17,0 % | 38,0 → 58,1 % | 7,7 → 6,3 min |
| 3 | 73,2 % | 24,7 → 43,4 % | 37,5 → 25,5 % | 24,3 → 44,2 % | 33,9 → 23,3 % | 27,4 → 46,5 % | 6,6 → 5,6 min |
| 4 | 93,1 % | 0,0 → 3,5 % | 96,8 → 81,2 % | 0,1 → 6,0 % | 95,8 → 75,7 % | 0,1 → 7,2 % | 55,4 → 38,5 min |
| 5 | 8,4 % | 54,8 → 58,0 % | 8,2 → 7,3 % | 64,3 → 66,1 % | 6,4 → 5,4 % | 68,1 → 69,5 % | 6,4 → 6,3 min |
| 8 | 6,3 % | 73,3 → 73,7 % | 8,4 → 8,3 % | 72,7 → 73,6 % | 7,7 → 7,7 % | 75,5 → 76,2 % | 4,9 → 4,9 min |
| 37 | 92,9 % | 0,2 → 3,4 % | 94,7 → 80,1 % | 0,2 → 3,9 % | 93,1 → 77,5 % | 0,3 → 4,9 % | remonte (pas d'entités) |

**En jeu** (`final416/v416light`, une graine par niveau, positions du joueur sur une grille, courant plein, règle
d'avant → v4.16) :

| Niveau (graine) | Positions | Lumière changée | Joueur dans le noir | Smiler qui se dissipe |
|---|---|---|---|---|
| 0 (`-BRSeed=9`) | 289 | 81,3 % | 18,7 → 37,0 % | 44,6 → 34,6 % |
| 0 (`-BRSeed=4242`) | 289 | 84,8 % | 4,8 → 23,5 % | 56,4 → 41,9 % |
| 1 (4242) | 288 | 67,7 % | 26,4 → 33,3 % | 44,4 → 35,1 % |
| 2 (4242) | 289 | 82,7 % | 23,9 → 45,7 % | 35,6 → 25,6 % |
| 4 (4242) | 289 | 90,7 % | 0,7 → 5,5 % | 96,9 → 84,8 % |
| 5 (4242) | 104 | 13,5 % | 55,8 → 57,7 % | 15,4 → 11,5 % |
| 8 (4242) | 226 | 11,5 % | 69,0 → 69,0 % | 13,7 → 13,3 % |
| 9 (4242) | 119 | 26,9 % | 30,3 → 31,9 % | 35,3 → 33,6 % |
| 37 (4242) | 244 | 93,4 % | 0,8 → 6,6 % | 92,2 → 76,2 % |

- Lecture : « apparition possible » est une part de cellules, pas un nombre d'apparitions (minuteries et budget de menace
  inchangés) ; « Smiler dissipé » : positions où un Smiler hors poursuite se dissipe ; « noir » : positions où la santé
  mentale baisse plus vite ; « Deathmoth non attiré » : positions éclairées où il n'est pas attiré par la lampe ;
  « Bacteria ralentie » : positions où elle repère deux fois moins vite un joueur sans lampe.
- Les deux mesures vont dans le même sens : au Level 0, la part des positions dans le noir double environ ; au Level 2,
  elle passe de 23,9 à 45,7 % en jeu (banc : 34,1 → 57,0 %) ; les Levels 5 et 8 changent peu.
- **Seuils gardés. Décision du propriétaire** : si c'est trop dur, baisser `NeedsDark` (seuil d'apparition, 0,12) ou
  compter une part des plafonniers cachés comme lumière de rebond. Aucune de ces options n'est appliquée ; aucune partie
  réelle n'a jugé la difficulté.

## 10. Contradictions corrigées

Le prompt (§ 11) demande de corriger les documents qui disaient encore « préparés » des tests exécutés en v4.15. Preuves :
[`RAPPORT_v4.15.md`](RAPPORT_v4.15.md) § 1 et 2, sommaires et rapports du dernier passage
`Saved/TestRuns/autotest/final4/summary.txt` (17 octobre, 16:07 à 18:26 : `v415`, `v413`, `v412` réussis ; `v414`
0 problème et 3 non vérifiés ; `v411` 0 et 1 ; `v415fix` 0 et 2) et `net_final4/{hote,client}/Rapport.txt` (0 problème,
4 et 3 non vérifiés). Ce passage a tourné sur le code de `03a7a3b`, le dernier commit des sources avant `aada32b` (qui ne
change que le rapport et le README) : ses rapports contiennent des lignes qui n'existent qu'à partir de `03a7a3b`
(« prise depuis le canal NON VERIFIEE : aucune place libre dans le canal devant le trottoir de l'echelle », « pose par le
tirage non jugee »). Corrections faites dans `6d39fe8`.

1. `Docs/HISTOIRE_PROGRESSION_v4.15.md`, en tête.
   - Avant : « les étapes du test du jeu `-BRAutoTestV415` sont préparées, à exécuter dans Unreal. »
   - Après : « les étapes du test du jeu `-BRAutoTestV415` ont été exécutées dans Unreal 5.8.3 (réussi, 0 problème :
     RAPPORT_v4.15.md § 2). »
   - Preuve : `final4/v415` (et `final` à `final3`) : `etat=reussi problemes=0 non_verifies=0`.
2. `Docs/HISTOIRE_PROGRESSION_v4.15.md`, § 3.
   - Avant : « Test du jeu `-BRAutoTestV415` (préparé) : ... ; `-BRNetTest` : drapeau, graine et sortie identiques ... »
   - Après : « Test du jeu `-BRAutoTestV415` (exécuté : réussi, 0 problème) : ... ; `-BRNetTest` (exécuté : 0 problème
     chez l'hôte et le client) : ... »
   - Preuve : `final4/v415` ; `net_final4` : « Niveau 2, graine 1167934721 (attendue 1167934721), drapeaux 1, niveau
     lineaire oui ; 0 sortie(s) des chunks (attendu 0), 1 de la mission » chez l'hôte et le client, « a terre puis
     reveille dans le niveau oui » chez le client.
3. `Docs/RAPPORT_v4.15.md`, § 6 (Poolrooms).
   - Avant : « reste ouvert : la vérification en jeu (étapes ajoutées, non lancées). »
   - Après : « reste ouvert : la vérification en jeu (étapes lancées au dernier passage, `final4` et `net_final4`, mais
     NON VÉRIFIÉES : aucune place libre dans le canal devant l'échelle du chunk (-1, -1) ; aucune échelle dans les chunks
     construits du test réseau). »
   - Preuve : `final4/v415fix/Rapport.txt`, section « v4.15 correctifs 5 : Niveau 37 » ; `net_final4/hote/Rapport.txt` :
     « Niveau 37 : NON VERIFIE, aucune echelle vers le Niveau 2 dans les chunks construits ».
4. `Docs/RAPPORT_v4.15.md`, § 2, ligne V415Fix.
   - Avant : « **0 problème** (1 non vérifié : pas de couloir sombre pour le Smiler sur cette graine) »
   - Après : « **0 problème** (2 non vérifiés : pas de couloir sombre pour le Smiler sur cette graine ; prise depuis le
     canal des Poolrooms, aucune place libre dans le canal) »
   - Preuve : `final4/summary.txt` : `v415fix ... etat=incomplet problemes=0 non_verifies=2` (le chiffre d'avant était
     celui de `final3`, antérieur aux correctifs 5).
5. `Docs/RAPPORT_v4.15.md`, § 2, ligne réseau.
   - Avant : « **0 / 0** (3 et 2 non vérifiés : arrivée tardive, 4 joueurs) »
   - Après : « **0 / 0** (4 et 3 non vérifiés : arrivée tardive, 4 joueurs, aucune échelle des Poolrooms dans les chunks
     construits) »
   - Preuve : `net_final4/hote/Rapport.txt` (4 NON VERIFIE) et `client/Rapport.txt` (3) ; « 3 et 2 » était le résultat de
     `net_final3`.
6. `Docs/GAMEPLAY_v4.15.md`, § 6 (Poolrooms).
   - Avant : « (à vérifier en jeu : étape ajoutée, pas encore lancée). »
   - Après : « (à vérifier en jeu : l'étape a tourné au dernier passage mais n'a pas pu se faire, faute de place libre
     dans le canal devant l'échelle essayée : non vérifié). »
   - Preuve : comme le point 3.
7. `Docs/PLAN_PUBLICATION.md`, § 4.
   - Avant : « (étape « contenu : version publiée », préparé, non exécuté) »
   - Après : « (étape « contenu : version publiée » ; exécutée en v4.15, réussie) »
   - Preuve : `final4/v412/Rapport.txt` : « connexion : meme contenu accepte oui ; ancien client refuse (version) oui ;
     autre protocole refuse oui ; autre contenu refuse et lu (chapitre 1 de l'hote) oui », suite réussie.
8. `Docs/PLAN_PUBLICATION.md`, § 5.
   - Avant : « placement réel essayé (`-BRAutoTestV411`/`V412`, préparé) ; »
   - Après : « placement réel essayé (`-BRAutoTestV411`/`V412`, exécutés en v4.15 : V411 0 problème et 1 non vérifié,
     V412 réussi) ; »
   - Preuve : `final4/summary.txt` (`v411 ... problemes=0 non_verifies=1`, `v412 ... etat=reussi`).
9. `Docs/PLAN_PUBLICATION.md`, § 6.
   - Avant : un seul bloc « **Préparé, non exécuté.** » avec « Constructions Shipping par canal », `-BRAutoTestV413`,
     `-BRAutoTestV412` et l'essai à la main d'un client incompatible.
   - Après : bloc « **Exécuté en v4.15** » (paquet Windows Shipping du canal public construit, cuit, contrôlé et lancé ;
     `-BRAutoTestV413` réussi, `-BRNetTest` 0 problème chez l'hôte et le client ; `-BRAutoTestV412` réussi), puis bloc
     « **Préparé, non exécuté.** » avec « Constructions Shipping des autres canaux et des autres systèmes (canal interne ;
     Linux, macOS) » et l'essai à la main d'un client incompatible (inchangé).
   - Preuve : `final4/summary.txt` ; `net_final4` (porte de service, départ de groupe unique, Level 6 même aile et même
     plan `20b45d7c`, calibrage de l'hôte reçu) ; `Build/Windows/Shipping/build_manifest.txt` de l'époque (commit
     `03a7a3b`, arbre noté modifié, contenu public, 17 octobre 16:37 UTC) ;
     `%LOCALAPPDATA%\Backrooms\Saved\AutoTest\Rapport.txt` (17 octobre 18:38, « Lancement du paquet », aucun problème) ;
     RAPPORT_v4.15 § 1.
10. `README.md`, § 9, `-BRAutoTestV411`.
    - Avant : « **Préparé, non exécuté** : écrit sans Unreal sous la main, à lancer en premier après la compilation. »
    - Après : « **Exécuté en v4.15** (Unreal 5.8.3) : 0 problème, 1 non vérifié (volume général lisible seulement sur le
      fil audio). »
    - Preuve : `final4/v411/Rapport.txt` : « NON VERIFIE : Niveau 4 : volumes : volume general de l'appareil lisible
      seulement sur le fil audio (Unreal 5.8) ».
11. `README.md`, § 9, `-BRAutoTestV412`.
    - Avant : « **Préparé, non exécuté.** » ; après : « **Exécuté en v4.15** : réussi, 0 problème. »
    - Preuve : `final3/v412` et `final4/v412` : `etat=reussi problemes=0`.
12. `README.md`, § 9, `-BRAutoTestV415`.
    - Avant : « **Préparé, à exécuter dans Unreal.** » ; après : « **Exécuté en v4.15** : réussi, 0 problème. »
    - Preuve : `final` à `final4`, `v415` : `etat=reussi problemes=0 non_verifies=0`.
13. `README.md`, § 9, `-BRAutoTestV414`.
    - Avant : « **Préparé, non exécuté.** » ; après : « **Exécuté en v4.15** : 0 problème, 3 non vérifiés (perte du focus,
      à vérifier à la main ; 144 et 240 images/s non atteintes sur cette machine). »
    - Preuve : `final4/v414/Rapport.txt` (trois NON VERIFIE : perte du focus, 144 et 240 images/s) ; AUDIT_v4.15 § 3.
14. `README.md`, § 9, `-BRAutoTestV413`.
    - Avant : « **Préparé, non exécuté.** » ; après : « **Exécuté en v4.15** : réussi, 0 problème. »
    - Preuve : `final3/v413` et `final4/v413` : `etat=reussi problemes=0`.
15. `README.md`, § 9, kit Blender du Level 0.
    - Avant : « Sortie et réglages d'import (préparés, non exécutés) »
    - Après : « Sortie et réglages d'import (import exécuté en v4.15 par `backrooms_setup.run(force=True)` : kit du
      Level 0 complet, 34 modules et 29 textures) »
    - Preuve : `Saved/BackroomsSetup_Level0Kit.txt` (17 octobre 14:07) : « Kit du Level 0 : OK (34 modules,
      29 textures) » ; `Saved/BackroomsSetup_Validation.txt` : « 0 probleme(s) » ; RAPPORT_v4.15 § 1.
16. `README.md`, § 10, tableau des plateformes, ligne Windows.
    - Avant : « préparé, non exécuté pour la v4.9 »
    - Après : « v4.15 : paquet Shipping du canal public construit, cuit, contrôlé et lancé (test de démarrage) ; v4.9 :
      préparé, non exécuté »
    - Preuve : `Build/Windows/Shipping/build_manifest.txt` et `%LOCALAPPDATA%\Backrooms\Saved\AutoTest\Rapport.txt`
      (point 9) ; RAPPORT_v4.15 § 1.

Laissés tels quels : les blocs de version du README (v4.9 à v4.14), `RAPPORT_v4.5` à `v4.14`, `AUDIT_v4.5` à `v4.14` et
`HISTOIRE_PROGRESSION_v4.13.md` décrivent l'état de leur version (tests préparés dans un conteneur sans Unreal) ; c'était
exact, et AUDIT_v4.15 dit qu'ils ont été lancés pour la première fois en v4.15. `Docs/v415/correctifs_5.txt` (« Le jeu
n'a pas ete lance ici ») est un journal de lot, exact pour son arbre de travail. La formule globale du README v4.15
(« toutes réussies hormis les images par seconde des salles de fosses ») n'est pas une mention « préparé » : elle n'est
pas modifiée, bien que plusieurs suites (V414, V410, V411, V415Fix, complet) finissent avec des non vérifiés.
