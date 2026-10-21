# v4.17 — Performances : protocole avant / après, scène « entites », isolation des coûts

Rapport de l'agent **Performance Benchmarker** (définition lue : `~/.claude/agents/testing-performance-benchmarker.md`,
rôle adapté à Unreal ; lancé par l'orchestrateur comme sous-agent générique). Mission : prompt v4.17 § 5.

**Ce que j'ai fait réellement** : lecture du banc (`BRAutoTestV416Perf.cpp`), de `run_perf.ps1`, `perf_summary.py`, des
mesures v4.16 (`Docs/RAPPORT_v4.16.md` § 6 et les dossiers d'étiquettes), du code des entités, de la lumière de jeu et
des sources du moteur 5.8 installé (noms des variables vérifiés) ; **implémentation** de deux scènes dans
`Source/Backrooms/Private/BRAutoTestV416Perf.cpp` (seul fichier source modifié ; commité ensuite par l'orchestrateur
dans `4a9996a`) ; `python -m unittest Tools/Tests/test_perf_summary.py` relancé après la modification : **15 tests OK**
(colonnes et options lues par le jeu).
**Ce que je n'ai pas fait** : ni compilation, ni lancement d'Unreal, ni mesure (consigne de l'orchestrateur). Rien de ce
qui suit n'est un chiffre de la v4.17 ; les seuls chiffres cités viennent des mesures v4.16 déjà archivées.

---

## 1. Base v4.16 : ce qui existe et où

- **Emplacement réel des mesures** : `Saved/TestRuns/perf/` (étiquettes `base_v416`, `hl2_cine`, `seeds_cine_mode1`,
  `seeds_cine_mode2`, `seeds_all_final`, `x_*`, résumés `cmp_*.md`). Il n'y a **pas** de `Saved/PerfRuns/` : c'est la
  valeur par défaut de `-OutRoot` de `run_perf.ps1`, que la v4.16 a remplacée. Toutes les commandes ci-dessous passent
  donc `-OutRoot Saved\TestRuns\perf` pour garder avant / après à côté de la base.
- **Fenêtre** : les manifestes v4.16 montrent `-ResX=1920 -ResY=1080` (valeur par défaut du script) ; la fenêtre obtenue
  est **1366 × 768** (colonnes `fenetre_x/y` de `Perf.csv`) : le 1920 × 1080 ne tient pas sur l'écran 1536 × 960 et le
  moteur l'a réduit. Pour la v4.17 : **`-ResX 1366 -ResY 768` explicites**, puis contrôle de `fenetre_x/y` et
  `interne_x/y` dans le premier `Perf.csv` (le résumé avertit si les résolutions diffèrent).
- **Durées relevées** (manifestes et `Rapport.txt`) : lancement à un profil, 5 scènes : **2,9 min** (suite 150–153 s +
  ≈ 24 s de démarrage et d'arrêt de l'éditeur) ; lancement « tous profils » : **10,1–10,8 min** (suite 585–594 s) ;
  chargement d'une scène 3,1–3,7 s, préchauffage 8,0–8,7 s, attente des shaders à l'arrivée 0 à 0,3 s.
- **Bruit observé** : écart des médianes entre exécutions 1,7 à 4,8 % (graines et exécutions mélangées,
  `seeds_cine_mode2`) ; c'est le plancher en dessous duquel un écart n'est pas un résultat.
- **Matériel** : RTX 5050 Laptop (pilote 610.88), i7-13620H, 16 Go ; l'écran est branché sur l'Intel UHD (rendu par la
  carte NVIDIA, image recopiée vers l'iGPU en fenêtré). Hors Cinématique, `rendu_med_ms` ≈ médiane de l'image et le GPU
  est loin derrière (ex. fosses Performance : image 9,28 ms, thread de rendu 9,25, GPU 4,15) : **ce portable n'est pas
  limité par le GPU dans ces profils** ; hypothèse à tester : thread de rendu / RHI ou présentation via l'iGPU.

## 2. Protocole A — avant / après sur le portable (exécutable)

### 2.1 Conditions à fixer et à noter avant chaque série

Secteur branché, même mode d'alimentation Windows et même profil du constructeur ; pilote NVIDIA inchangé (colonne
`pilote` de `Perf.csv`) ; `UnrealEditor.exe` sur la carte NVIDIA ; mode d'affichage hybride inchangé (un passage en
GPU seul / MUX change les résultats) ; aucune autre instance d'Unreal (le script saute alors le lancement), **aucune
compilation, cuisson ni import en parallèle** (autres agents compris : ils volent le processeur et peuvent réécrire
`Binaries/`) ; ne pas toucher la fenêtre pendant la série (une perte de focus est notée « pas au premier plan ») ;
pilote NVIDIA sans limite d'images ni synchro forcées (le jeu coupe la synchro et la limite ; le résumé ne voit que
les variables du moteur, pas les réglages du pilote).

### 2.2 Arbre propre, commits et contenu

1. **Le banc d'abord** : l'orchestrateur a commité ce fichier dans `4a9996a` (avec le banc visuel v4.17). Entre
   `c07eddb` et `a632a89`, seuls des fichiers de test changent, plus `BRHUD.cpp/.h` pour un mode de capture de test
   (`TestCaptureMode`, 0 par défaut : dessin du HUD inchangé dans le banc de performance). **`a632a89` peut donc servir
   d'« avant » (`B0`)** : code du jeu de la v4.16, banc v4.17. Tout commit qui touche au jeu, aux matériaux ou aux
   scripts d'import vient après la mesure de l'avant, ou l'avant se mesure dans une copie à `B0`. Recompiler l'éditeur
   à `B0` avant la première mesure (les `Binaries/` du 21 octobre 00:10 précèdent ou accompagnent le commit : rien ne
   garantit qu'ils contiennent la version finale du fichier).
2. **Le contenu importé n'est pas versionné** (`Content/Backrooms/`, ignoré par Git, régénéré depuis `RawAssets/` par
   `Content/Python/backrooms_setup.py` ; 504 Mo avec le reste de `Content/`) : le commit ne dit pas quels matériaux et
   maillages ont été mesurés. Au 21 octobre, aucun fichier de `Content/` n'est plus récent que le rapport v4.16 : le
   contenu est encore celui de la v4.16, mais `Tools/Blender/build_level0_kit.py` est déjà modifié dans l'arbre (lot
   modèles en cours). **Avant que les lots matériaux / import ne régénèrent le contenu**, faire la copie de l'avant :
   ```powershell
   git -C <depot> worktree add wt\avant <B0>
   robocopy Backrooms\Content\Backrooms wt\avant\Backrooms\Content\Backrooms /MIR
   & "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" BackroomsEditor Win64 Development "-Project=wt\avant\Backrooms\Backrooms.uproject" -WaitMutex
   ```
   (copier aussi `DerivedDataCache\` s'il existe dans le projet : sinon l'amorçage recompile tous les shaders), et
   écrire une empreinte du contenu à côté de chaque étiquette (avant comme après), par exemple
   `Get-ChildItem Content\Backrooms -Recurse -File | Get-FileHash | Export-Csv <dossier de l'étiquette>\contenu.csv`.
3. **Après** : commit final de la v4.17, contenu régénéré par les scripts de ce commit, éditeur recompilé **après** le
   dernier changement ; `git status` propre (le script écrit `commit.txt` avec `modifie=0|1` ; une étiquette `modifie=1`
   ne vaut pas mesure).
4. Une étiquette = un commit, un binaire, un contenu. Jamais `-Append` sur une étiquette après un nouveau commit
   (`perf_summary.py` avertit d'un mélange, mais l'étiquette est alors à refaire).

### 2.3 Chargement et shaders à part de l'état préchauffé

- **Amorçage** après chaque construction (avant et après), jeté des comparaisons : remplit le cache de shaders de
  l'éditeur et des PSO ; ses colonnes `chargement_s`, `attente_shaders_arrivee_s`, `prechauffe_s`,
  `shaders_max_prechauffe` sont les coûts « à froid » de l'éditeur. Si une scène dépasse 120 s de chargement (nouveaux
  matériaux), relancer l'amorçage avant la série.
- **Séries mesurées** : le banc ne commence la fenêtre qu'après le préchauffage (≥ 8 s, ni shader ni PSO en compilation
  depuis 1 s, aucun chunk en préparation, temps d'image stabilisés) ; dans la fenêtre, `images_shaders`, `pso_crees`,
  `pso_saccades` et `chargements_sync` doivent valoir 0. `chargement_s` et l'attente des shaders restent dans chaque ligne,
  hors fenêtre.
- **Joueur réel (paquet)** : le banc tourne dans l'éditeur `-game` ; la version Shipping refuse ce plan. Le premier
  lancement réel (cache de PSO du paquet, cache de shaders du pilote) se mesure sur un paquet **Development**
  (`build_windows.ps1 -Config Development`) lancé à la main avec `-BRAutoTest -BRAutoTestPerf ...`, après avoir vidé le
  cache du pilote ; `run_perf.ps1` ne sait pas lancer un paquet (résultats sous le `Saved` du paquet). NON VÉRIFIÉ.

### 2.4 Commandes exactes

Depuis `Backrooms` (PowerShell 5.1) ; `$Avant` et `$Apres` : chemins des deux
`Backrooms.uproject` ; un profil par lancement (`-Profiles 0,1,4,2`, VRAM de 8 Go), deux graines (`0` = graines de
référence de chaque scène, `4242`), trois exécutions, les cinq scènes v4.16 **puis** les deux scènes v4.17 en fin de
lancement (elles ne précèdent donc jamais une scène v4.16 dans le même processus) :

```powershell
$Scenes = 'fosses,bassins,marche0,marche1,lidar6,entites0,entites'
$Out = 'Backrooms\Saved\TestRuns\perf'
# Amorcage (jete)
powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Project $Avant -Tag v417_avant_amorce -Runs 1 -Seeds 0 -Profiles 0,1,4,2 -Scenes $Scenes -ResX 1366 -ResY 768 -OutRoot $Out -NoSummary
# Avant : 3 executions x 2 graines x 4 profils
powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Project $Avant -Tag v417_avant -Runs 3 -Seeds 0,4242 -Profiles 0,1,4,2 -Scenes $Scenes -ResX 1366 -ResY 768 -OutRoot $Out
# Apres (meme commande, autre projet, autres etiquettes)
powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Project $Apres -Tag v417_apres_amorce -Runs 1 -Seeds 0 -Profiles 0,1,4,2 -Scenes $Scenes -ResX 1366 -ResY 768 -OutRoot $Out -NoSummary
powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Project $Apres -Tag v417_apres -Runs 3 -Seeds 0,4242 -Profiles 0,1,4,2 -Scenes $Scenes -ResX 1366 -ResY 768 -OutRoot $Out
# Comparaisons
python Tools\Tests\perf_summary.py --before $Out\v417_avant --after $Out\v417_apres --out $Out\cmp_v417.md
python Tools\Tests\perf_summary.py --by-seed --before $Out\v417_avant --after $Out\v417_apres --out $Out\cmp_v417_parseed.md
```

**Variante alternée (recommandée sur portable : chaleur, mises à jour, dérive)** : les deux binaires restent construits
côte à côte et l'on alterne une exécution de chaque, `-Append` à partir de la deuxième :

```powershell
foreach ($r in 1..3) {
  $more = @(); if ($r -gt 1) { $more = @('-Append') }
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Project $Avant -Tag v417_avant -Runs 1 -Seeds 0,4242 -Profiles 0,1,4,2 -Scenes $Scenes -ResX 1366 -ResY 768 -OutRoot $Out -NoSummary @more
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Project $Apres -Tag v417_apres -Runs 1 -Seeds 0,4242 -Profiles 0,1,4,2 -Scenes $Scenes -ResX 1366 -ResY 768 -OutRoot $Out -NoSummary @more
}
```

**Contrôle de reproductibilité** (sans code du jeu changé, l'avant doit retrouver la v4.16 dans le bruit) :
`perf_summary.py --before $Out\seeds_cine_mode2 --after $Out\v417_avant` pour la Cinématique (même mode 2, mêmes
graines) et `--before $Out\base_v416 --after $Out\v417_avant` pour les trois autres profils, graine 0, **hors bassins**
(vanne déplacée par `c1c2bde` après `base_v416`). Un écart hors bruit veut dire que la machine a changé, pas le code.

**Protocole court** (lot isolé, pour décider vite) : `-Runs 2 -Seeds 0,4242 -Profiles 4,2` et seulement les scènes
touchées par le lot (par exemple `-Scenes fosses,marche0` pour le Level 0, `bassins` pour l'eau, `entites0,entites` pour
les créatures).

### 2.5 Durée estimée

| Série | Lancements | Durée d'un lancement | Total |
|---|---|---|---|
| 5 scènes v4.16, un profil (mesuré) | — | 2,9 min | — |
| 7 scènes (5 + `entites0` + `entites`), un profil (estimé) | — | ≈ 4,1 min (≈ 35 s par scène v4.17 : fenêtre 20 s + chargement, préchauffage, 3 relevés ProfileGPU, mise en jeu 1 s) | — |
| Amorçage (avant ou après) | 4 | ≈ 4,1 min + compilation des shaders | ≈ 17 min + compilation |
| Avant ou après complet (3 × 2 × 4) | 24 | ≈ 4,1 min | **≈ 1 h 40** |
| Protocole complet (2 amorçages + avant + après) | 56 | | **≈ 3 h 50** hors compilations |
| Protocole court (2 × 2 × 2, 7 scènes) | 8 par côté | ≈ 4,1 min | ≈ 33 min par côté |
| Scènes `entites0,entites` seules | — | ≈ 1,6 min | — |

### 2.6 Ce qu'il faut regarder, et règles de décision

Lignes retenues seulement si `prechauffe_stable = 1`, `scenario_ok = 1`, `premier_plan = 1`, `vsync = 0`, `max_fps = 0`,
mêmes `interne_x/y`, `fenetre_x/y`, mêmes graines du niveau, un seul commit non modifié par étiquette (le résumé
affiche tout cela en « ATTENTION »).

- **Médiane** (médiane des médianes) et **écart des médianes** (bruit) ; **95 %** et **pire 99 %** ; **pire image** ;
  **longues images > 33,3 / 50 / 100 ms** pour 1000 images (saccades) ; `bas_1pct_img_s`. Le banc ne compte pas les
  images > 16,7 ms : pour la cible de 60 img/s, lire 95 % et 99 % contre 16,7 ms.
- **Qui limite** : `gpu_med_ms` (durée GPU de l'image, de bout en bout) contre `rendu_med_ms`, `jeu_med_ms`,
  `rhi_med_ms`. GPU ≈ médiane : limité par le GPU ; rendu ≈ médiane et GPU nettement plus bas : thread de rendu, RHI
  ou présentation ; jeu élevé : IA, animation, streaming.
- **VRAM** : `vram_utilisee_mo` et pic `vram_max_mo` contre `vram_budget_mo` (7142 Mo ici) ; `vram_retrogradee_mo` doit
  rester à 0 (mémoire renvoyée en RAM : saccades) ; `textures_mo`, `pool_textures_mo`.
- **Chunks** (marche1 surtout) : `construction_ms`, `construction_pire_image_ms`, `images_hors_budget`,
  `constructions_forcees`, `demontage_ms`, `demontage_pire_ms`, `composants_par_s`, `collisions_predites` ;
  `chargements_sync` = 0.
- **Scanner** (lidar6) : `lidar_cpu_ms`, `lidar_traces_ms`, `lidar_envoi_ms`, `lidar_instances_max`, matériaux par seconde.
- **Entités** : notes de la ligne `entites` (places, présence, poursuites, attaques) et écart **`entites` − `entites0`**.
- **Postes GPU** (`PerfPasses.csv`) : file graphique d'un côté, files asynchrones de l'autre (§ 5).

Règle : un **gain** (ou une régression) est retenu si l'écart de la médiane dépasse à la fois 2 % et le plus grand des
deux écarts des médianes, **avec le même signe sur chaque graine** (`--by-seed`) ; sinon « dans le bruit ». Une hausse
de 99 % ou de longues images > 50 ms (plus d'une pour 1000 images) est examinée même si la médiane baisse. Toute baisse
de qualité volontaire (pourcentage de rendu, MegaLights, brouillard...) est nommée dans l'étiquette et rapportée comme
telle, jamais comme un gain du code.

## 3. Protocole 2560 × 1440 sur RTX 4080 + Ryzen 7 7800X3D — **NON VÉRIFIÉ** (machine absente)

- Même commit, même contenu (copie du projet avec `Content/`), éditeur construit sur place ; noter pilote, état du
  « hardware-accelerated GPU scheduling », mode Jeu de Windows ; panneau NVIDIA : synchro « selon l'application »,
  aucune limite d'images.
- **Fenêtre** : `run_perf.ps1` impose `-windowed` ; un 2560 × 1440 fenêtré ne tient pas sur un écran 1440p (barre de
  titre) et serait réduit comme 1920 × 1080 l'a été sur le portable. Il faut un bureau plus grand (4K) **ou** un mode
  plein écran : proposition (non faite, fichier hors de mon périmètre) d'un paramètre `-WindowMode fullscreen` qui passe
  `-fullscreen` au lieu de `-windowed`. Contournement possible à essayer : `-PerfCmds "r.SetRes 2560x1440f"` (appliqué à
  chaque profil, avant le chargement). Dans tous les cas, `fenetre_x/y` de `Perf.csv` fait foi.
- **Résolution réellement calculée** (`interne_x/y`, arrondi du pourcentage de rendu) : Performance 67 % → 1715 × 965 ;
  Qualité 80 % → 2048 × 1152 ; RTX fluide 67 % → 1715 × 965 ; Cinématique 100 % → 2560 × 1440 (TSR partout).
- **Profil RT de la cible 60 img/s** : RTX fluide (Lumen matériel, MegaLights 2 échantillons, sans hit lighting, TSR
  depuis 1715 × 965). Critère proposé : médiane ≤ 15 ms, 95 % ≤ 16,7 ms, 99 % ≤ 20 ms, aucune image > 33,3 ms,
  `chargements_sync` = `pso_saccades` = 0, sur les 7 scènes et les deux graines. Qualité en second.
- Commandes :
  ```powershell
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Tag v417_4080_1440 -Runs 3 -Seeds 0,4242 -Profiles 4,1,0,2 -Scenes $Scenes -ResX 2560 -ResY 1440 -OutRoot $Out
  # 100 % de rendu (defaut initial du proprietaire), qualite volontairement differente : etiquettes a part
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Tag v417_4080_1440_rtx100 -Runs 3 -Seeds 0,4242 -Profiles 4 -Scenes $Scenes -ResX 2560 -ResY 1440 -OutRoot $Out -PerfCmds "r.ScreenPercentage 100"
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Tag v417_4080_1440_qual100 -Runs 3 -Seeds 0,4242 -Profiles 1 -Scenes $Scenes -ResX 2560 -ResY 1440 -OutRoot $Out -PerfCmds "r.ScreenPercentage 100"
  # Maximum RT / Cinematique : le profil Cinematique (mode 2) est deja dans la premiere etiquette ; l'ancien mode 1 (le plus cher)
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Tag v417_4080_1440_cine_mode1 -Runs 3 -Seeds 0,4242 -Profiles 2 -Scenes $Scenes -ResX 2560 -ResY 1440 -OutRoot $Out -PerfCmds "r.Lumen.HardwareRayTracing.LightingMode 1"
  ```
- Sur le 7800X3D, comparer `rendu_med_ms` / `jeu_med_ms` au GPU : à 1440p la 4080 peut laisser le thread de rendu
  limiter (comme le portable hors Cinématique). Le portable ne prédit rien de ces chiffres.
- Limite commune : la lampe est éteinte dans toutes les scènes ; les **ombres ray tracées de la lampe** (Cinématique)
  ne sont donc pas mesurées par le banc.

## 4. Scène « joueur et entités » — ce que j'ai écrit et pourquoi

### 4.1 Deux scènes, hors de la liste par défaut

| Id | Niveau, graine | Pose et regard | Entités | Fenêtre |
|---|---|---|---|---|
| `entites0` (témoin) | Level 0, graine utilisateur 9 (celle de `fosses` et `marche0`) | point d'arrivée ; regard sur l'axe de la grille le plus dégagé (trace de 30 m sur les quatre axes, en partant du plus proche du regard d'arrivée, égalité : le premier) ; tangage −4° ; **balayage ±35° en 10 s**, fonction du temps seul | aucune | 20 s |
| `entites` | idem | **même pose, même balayage** | Hound, Bacteria, Smiler, IA active | 20 s |

Sélection : `-BRPerfScenes=entites0,entites` (ou `run_perf.ps1 -Scenes ...`), `-BRPerfScenes=toutes` pour les sept.
**Pas dans la liste par défaut**, parce que : (1) un lancement sans `-BRPerfScenes` doit rester identique à
`base_v416` (mêmes scènes, même ordre, même durée) ; (2) dans un lancement « tous profils », l'ordre est profil par
profil : une scène ajoutée s'exécuterait entre deux profils (entités de Performance avant les fosses de Qualité) et
changerait l'état hérité par les scènes suivantes (mémoire vidéo des créatures, caches, chaleur) ; (3) la base v4.16
n'a pas ces scènes. Placées en fin de liste dans un lancement à un profil, elles ne touchent pas aux cinq autres.

### 4.2 Les entités

- **Hound** : maillage à squelette ; `SK_HoundLite` en Performance, Qualité et RTX fluide, `SK_Hound` complet en
  Cinématique (`FBRSettings::bFullCreatures` du profil) : c'est le « Hound allégé » et la créature à squelette mise à
  jour dans la scène ray tracée (`r.RayTracing.Geometry.SkeletalMeshes` actif avec MegaLights) ; à 7 m d'abord, dans
  l'axe ou à ±12°/±24°, puis 10,5 / 5,25 / 14 m.
- **Bacteria** : l'entité du Level 0 (pièces rigides articulées, cris continus, poursuite) ; 10,5 m d'abord, ±18° puis
  ±30°, ±8°, puis 14 / 7 / 17,5 m.
- **Smiler** : lueur et halo ; seulement à une place dont la lumière de jeu est au plus `BRLight::NeedsDark` (0,12),
  la règle d'apparition des entités de l'ombre (`ABRWorld::UpdatePopulation`) : ailleurs il se dissiperait
  (`BRLight::SmilerVanish`, hors poursuite) et la scène changerait en cours de fenêtre. Essais à 7 / 10,5 / 14 / 17,5 /
  5,25 m, de 0 à ±60°. Pas de place sombre en vue : pas de Smiler, noté dans la ligne.

Chaque place : cellule praticable, ni bassin, ni salle de fosses, chunk construit, pas au-dessus d'une fosse, à 3 m au
moins du joueur et 2,5 m des autres, **vue depuis l'œil du joueur** (trace de visibilité libre) ; ordre des essais fixe,
donc places identiques à chaque exécution d'une graine et d'un contenu donnés. Moins de deux entités posées : scène
non mesurée (NON VÉRIFIÉ, raison dans le rapport).

### 4.3 Déroulement (même ordre d'étapes que les autres scènes)

1. Profil, chargement, pose (inchangés) ; pour `entites`, les trois entités sont posées **presque figées**
   (`CustomTimeDilation` 0,02, comme les bancs visuels), au sol par une trace vers le bas (pas de chute de 5 cm), face au
   joueur ; leurs fiches du journal sont ouvertes tout de suite, pour que l'annonce passe pendant le préchauffage et
   jamais dans la fenêtre.
2. Préchauffage existant (shaders, PSO, ombres et structures du ray tracing des créatures prêts ; scène immobile, donc
   stabilisable).
3. Nouvelle étape « entités en jeu » : les entités du préchauffage sont **remplacées par des neuves aux mêmes places**,
   tirages du jeu sur la graine fixe du banc, IA à vitesse normale : **état de départ identique** d'une exécution à
   l'autre. La fenêtre commence au moins 1 s plus tard, shaders et PSO au repos depuis 0,25 s (au plus 8 s, sinon
   noté) : création des composants et des structures RT hors de la fenêtre.
4. Fenêtre de 20 s : regard balayé (les entités entrent et sortent du champ : cadence d'animation hors champ, ombres,
   reflets), IA libre (repérage, poursuite, intimidation du Hound quand on le regarde, attaques). **Joueur invincible**
   (`bGodMode`, déjà posé par le banc : un coup ne retire rien et ne lance pas de jumpscare), santé mentale et santé
   remises à 100 à chaque image (ni mort par folie, ni effets d'image qui dériveraient) ; `bPerfQuiet` reste actif : ni
   apparition, ni ronde, ni phénomène, ni coupure tirés par le jeu. Les relevés ProfileGPU suivent, balayage continué.
5. Bilan : ligne de `Perf.csv` (mêmes colonnes ; `entites_max` compte les créatures), puis entités retirées.

**Scénario** (`scenario_ok`) : celui des scènes fixes (joueur à moins de 1,5 m de sa pose, même niveau, vivant) plus,
pour `entites`, toutes les entités présentes (ni détruites ni en train de se dissiper) sur au moins 90 % des images et
au moins 1 m parcouru par elles en tout (IA active). **Notes de la ligne** (dans `Perf.csv` et `Rapport.txt`) : cellule
et regard de la pose ; espèce, cellule, distance, angle et lumière de jeu de chaque place ; part des images avec toutes
les entités, avec au moins une rendue, avec une poursuite ; distance parcourue ; préparations d'attaque, coups portés
(sans dégât) et esquives (compteurs `ABREntity::Stat*`).

Aucun changement de `BRAutoTest.h` n'est nécessaire (tout est dans `AddPerfSteps` et l'espace `BRTestPerf`). Aucune
colonne ajoutée : `perf_summary.py` lit les nouvelles lignes sans changement (elles sont classées après les cinq
autres ; ajout cosmétique possible : `"entites0", "entites"` dans `SCENE_ORDER`).

### 4.4 Usage et premier contrôle (à faire par l'orchestrateur après compilation)

```powershell
powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Tag v417_ent_essai -Runs 2 -Seeds 0 -Profiles 4 -Scenes entites0,entites -ResX 1366 -ResY 768 -OutRoot Saved\TestRuns\perf
```

À vérifier : compilation sans avertissement ; dans `Rapport.txt`, « 3 entite(s) posee(s) » (ou 2 avec la raison) et les
**mêmes cellules d'une exécution à l'autre** ; même note de pose pour `entites0` et `entites` ; `scenario_ok = 1` et
`prechauffe_stable = 1` ; écart des médianes de `entites` (attendu plus grand que celui des scènes fixes : à mesurer).
Une inspection à l'image par un agent qui voit (Smiler visible, créatures au sol, pas d'attaque bloquée contre un mur)
reste à faire.

### 4.5 Limites

- L'IA n'est pas déterministe image par image (pas de temps variable, chemins, perception) : seuls l'état de départ, les
  places et le regard le sont. D'où trois exécutions au moins et le contrôle de l'écart.
- Si la v4.17 change la lumière de jeu ou le décor du Level 0 (variantes, accessoires qui coupent la vue), les places
  peuvent changer entre avant et après : **comparer les notes** ; places différentes = autre scène, comparaison
  `entites` invalide (les cinq autres scènes ne sont pas concernées).
- Lampe éteinte (comme partout : un Smiler éclairé charge) : ombres RT de la lampe non mesurées. Pas de coéquipier
  (banc solo) : combinaisons répliquées et réseau hors scène. Santé mentale tenue à 100 : effets d'image de la folie
  hors mesure. « Au moins une rendue » suit `AActor::WasRecentlyRendered` (sémantique du moteur, indicative).
- Le modèle du Hound change avec le profil (allégé / complet) : par conception, pas un écart du code.
- Ni compilé ni lancé par moi ; API utilisées vérifiées dans les en-têtes 5.8 installés ou déjà employées par le projet
  (`AActor::WasRecentlyRendered`, `TConstArrayView`, `MakeArrayView`, `FCollisionResponseParams`, `UE_PI`,
  `FRotator::NormalizeAxis`, `ABRWorld::SpawnEntity`, `LightLevelAt`, `Discover`).

## 5. Isolation des coûts (eau, brouillard, ombres, RT des entités, scanner)

### 5.1 Méthode

- Une **bascule = une étiquette** `x_<poste>`, mesurée contre un **témoin** `x_<poste>_ctrl` du **même binaire**,
  alternés (`-Append`), 2 à 3 exécutions, un profil par lancement, seulement les scènes concernées — comme les essais
  `x_*` de la v4.16. `-PerfCmds` est appliqué après le profil, à chaque scène, et l'emporte sur la scalabilité.
- **Coût d'un poste = écart de `gpu_med_ms` et de la médiane de l'image** entre témoin et bascule. Les postes
  ProfileGPU servent à attribuer, pas à additionner : la file graphique se décompose en racine + attentes
  (`gpu_racine_ms` + `gpu_attente_ms` ≈ `gpu_image_ms`), les files asynchrones (`poste_async`, `gpu_autres_files_ms`) se
  recouvrent avec elle. Exemple v4.16, bassins RTX fluide : graphique 6,18 ms + attente 1,99 = 8,24 ms d'image GPU,
  file asynchrone 6,88 ms : les additionner donnerait 13,06 ms, faux.
- Pour attribuer un poste asynchrone, une étiquette d'**attribution** avec `-PerfCmds "r.RDG.AsyncCompute 0"` (ou
  `"r.Lumen.AsyncCompute 0"`) met tout sur la file graphique : les passes deviennent additives ; ses temps d'image ne
  sont pas des performances du jeu.
- Pas de `stat` dans `-PerfCmds` (bascule à chaque scène, dessin à l'écran) : `stat unit`, `stat gpu`, `stat rhi`,
  `stat scenerendering`, `stat game`, `stat anim` en séance manuelle. Unreal Insights : ligne de `commande.txt` +
  `-trace=cpu,gpu,frame,bookmark -statnamedevents -tracefile=<fichier>.utrace`, lancée à la main (surcoût : attribution
  seulement) ; `UnrealInsights.exe` n'est pas dans `Engine/Binaries/Win64` de l'installation : à construire ou à
  ouvrir depuis l'éditeur.

### 5.2 Bascules par poste (noms vérifiés dans les sources du moteur 5.8 installé)

| Poste | Scènes / profils | `-PerfCmds` | Ce que l'écart mesure | Remarques |
|---|---|---|---|---|
| Eau : reflets de la 1re couche translucide | bassins ; Qualité, RTX fluide, Cinématique | `r.Lumen.TranslucencyReflections.FrontLayer.Allow 0` | passe `FrontLayerTranslucency` (v4.16 : 0,68 ms asynchrone en RTX fluide, 2,75 ms en Cinématique) | `Allow` plutôt que `Enable` : la passe s'active aussi par le post-process (`FrontLayerTranslucency.cpp`). Image : reflets de l'eau par le cache de radiance |
| Eau : toutes les translucides | bassins ; tous | `ShowFlag.Translucency 0` | borne haute (eau + autres translucides) | image très différente : borne seulement |
| Eau : réfraction RT | bassins ; RT | — | — | **constat de source** : `r.Lumen.Reflections.HardwareRayTracing.Translucent.Refraction`, posée par `ApplySettings` (`BRPlayerController.cpp`), n'existe pas dans les sources 5.8 (seules `.EnableForProject`, en lecture seule, et `.MaxRefractionBounces`, 0 par défaut) : la commande du jeu est probablement sans effet. À confirmer dans la console du jeu |
| Eau : simulation (sillages, texture `WaterSim`) | bassins | aucune variable | temps du thread du jeu | Insights (`ABRWorld::UpdateWaterSim`) |
| Brouillard volumétrique | toutes ; tous | `r.VolumetricFog 0` | `ComputeVolumetricFog` (1,98 ms bassins Cinématique) | image : plus de halo |
| Volumes de brouillard locaux | fosses, marche0 | `r.LocalFogVolume.RenderIntoVolumetricFog 0`, puis `r.LocalFogVolume 0` | voxelisation, puis rendu des volumes | obscurité des fosses changée |
| Brouillard de hauteur | toutes | `r.Fog 0` | `ExponentialHeightFog` (0,06 ms) | |
| Lumière MegaLights dans le brouillard | RT | `r.MegaLights.Volume 0` | volume de MegaLights du brouillard | |
| Finesse de la grille | toutes | `r.VolumetricFog.GridPixelSize 16`, `r.VolumetricFog.GridSizeZ 64` | sensibilité du coût à la grille | relever d'abord la valeur du profil dans la console |
| Ombres virtuelles | Performance (VSM) ; marche0, entites | `r.Shadow.Virtual.Cache 0` | ce que le cache épargne (le coût monte) : sensibilité aux invalidations (créatures qui bougent) | |
| Ombres : borne | tous | `ShowFlag.DynamicShadows 0` | borne haute des ombres dynamiques | image sans ombres |
| Ombres des lampes (MegaLights) | Qualité, RTX fluide, Cinématique | `r.MegaLights.NumSamplesPerPixel 2` ; `r.MegaLights.DefaultShadowMethod 1` | coût par échantillon ; RT contre VSM | `r.MegaLights.Allowed 0` = borne seulement : le jeu règle ses ombres en supposant MegaLights ; le vrai mode du jeu est `-BRMegaLights=0`, que `run_perf.ps1` ne sait pas passer |
| RT des entités | `entites` (et `entites0`) ; Qualité, RTX fluide, Cinématique | `r.RayTracing.Geometry.SkeletalMeshes 0` | maillages à squelette dans la scène RT (mises à jour, traces MegaLights / Lumen) : poste `scene_rt` et médiane | image : plus d'ombre RT ni de reflet RT des créatures |
| Animation hors champ en RT | `entites` ; RT | `r.RayTracing.DynamicGeometryLastRenderTimeUpdateDistance 0` | **hypothèse** : à 50 m par défaut, le moteur marque « rendues » les géométries dynamiques de la scène RT ; le LOD d'animation du jeu (`ABREntity::ApplySkins`, `WasRecentlyRendered`) les animerait alors à pleine cadence hors champ | lire `jeu_med_ms` et `scene_rt` |
| Coût total des entités | `entites` − `entites0` | — | IA, animation, ombres, RT, lueurs, sons | même pose, même balayage |
| Scanner : processeur | lidar6 | — | `lidar_cpu_ms`, `lidar_traces_ms`, `lidar_envoi_ms` (déjà au banc) | |
| Scanner : instances sur le GPU | lidar6 | aucune bascule propre | — | `ShowFlag.InstancedStaticMeshes 0` cacherait aussi les murs des chunks (instances) : inutilisable. Proposition : une variable de console du banc qui cache les composants d'instances du scanner (le scanner continue sur le processeur) ; d'ici là, capture PIX / Nsight d'une image |

## 6. Constats annexes

- `run_perf.ps1` : pas de mode plein écran (§ 3), pas de passage d'options libres (`-BRMegaLights=0`,
  `-BRHitLightingMode=1`, `-BRPerfWarm=` ne passent pas ; seules les commandes de console de `-PerfCmds`), `-OutRoot`
  par défaut différent du dossier des mesures v4.16. Propositions, non faites.
- La variable de réfraction RT de l'eau (§ 5.2) : constat de source à vérifier en jeu avant tout réglage de l'eau.

## 7. Non vérifié

Compilation et premier lancement des scènes `entites0` / `entites` ; leur écart entre exécutions ; toute mesure v4.17 ;
la machine de référence (RTX 4080 + 7800X3D) et le 2560 × 1440 ; le paquet Development ; les durées estimées des
nouvelles scènes ; l'effet des bascules du § 5.2.

## 8. Résumé

1. Base v4.16 lue : mesures dans `Saved/TestRuns/perf/` (pas `Saved/PerfRuns/`), fenêtre réelle 1366 × 768 malgré `-ResX 1920`.
2. Avant = `a632a89` (jeu v4.16, banc v4.17 commité dans `4a9996a`) ; contenu non versionné à copier et empreinter avant les lots matériaux.
3. Commandes : 3 exécutions × graines 0 et 4242 × un profil par lancement, 7 scènes, `-ResX 1366 -ResY 768`, `-OutRoot Saved\TestRuns\perf`.
4. Durée : ≈ 4,1 min par lancement, ≈ 1 h 40 par côté, ≈ 3 h 50 en tout ; protocole court ≈ 33 min par côté.
5. Amorçage jeté après chaque construction ; dans la fenêtre : 0 shader, 0 PSO, 0 chargement synchrone.
6. Règle : écart retenu au-delà de 2 % et du bruit, même signe sur chaque graine ; queues (99 %, > 50 ms) examinées à part.
7. 4080 en 2560 × 1440 : NON VÉRIFIÉ ; plein écran à prévoir ; RTX fluide calcule 1715 × 965 ; 100 % et Cinématique mode 1 en étiquettes à part.
8. Scènes implémentées : `entites0` (témoin) et `entites` (Hound, Bacteria, Smiler dans le noir), Level 0 graine 9, regard balayé.
9. Entités presque figées au préchauffage, remplacées par des neuves 1 s avant la fenêtre ; joueur invincible ; scénario vérifié.
10. Hors liste par défaut (`-BRPerfScenes=entites0,entites`) pour ne pas changer les comparaisons avec `base_v416`.
11. Isolation : bascules par `-PerfCmds` contre un témoin du même binaire, écart de `gpu_med_ms`, jamais de somme des files asynchrones.
12. Constat : la variable de réfraction RT posée par le jeu n'existe pas dans les sources 5.8 ; rien compilé ni lancé par moi.
