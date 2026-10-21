# Mesures de performance : v417f_avant v417f_avant2 -> v417f_apres_perfo

**avant (v417f_avant v417f_avant2)** : 112 mesure(s), version(s) v4.16
- machine : NVIDIA GeForce RTX 5050 Laptop GPU, pilote 610.88 ; 13th Gen Intel(R) Core(TM) i7-13620H
- resolutions (interne -> fenetre) : 1093x614 -> 1366x768, 1366x768 -> 1366x768, 915x515 -> 1366x768
- commit(s) : inconnu

**apres (v417f_apres_perfo)** : 28 mesure(s), version(s) v4.17
- machine : NVIDIA GeForce RTX 5050 Laptop GPU, pilote 610.88 ; 13th Gen Intel(R) Core(TM) i7-13620H
- resolutions (interne -> fenetre) : 915x515 -> 1366x768
- commit(s) : 207404650dc3 (modifie)

ATTENTION : resolutions differentes entre avant et apres.
Seulement avant : fosses Qualite, fosses RTXFluide, fosses Cinematique, bassins Qualite, bassins RTXFluide, bassins Cinematique, marche0 Qualite, marche0 RTXFluide, marche0 Cinematique, marche1 Qualite, marche1 RTXFluide, marche1 Cinematique, lidar6 Qualite, lidar6 RTXFluide, lidar6 Cinematique, entites Qualite, entites RTXFluide, entites Cinematique, entites0 Qualite, entites0 RTXFluide, entites0 Cinematique
ATTENTION : apres (v417f_apres_perfo) : au moins une execution vient d'un arbre modifie (non commite).

## Temps d'image (avant -> apres, ecart)

| Scene | Profil | n avant / apres | Mediane (ms) | Ecart | 95 % (ms) | Pire 99 % (ms) | > 33 ms pour 1000 img | > 50 ms pour 1000 img | GPU (ms) | Jeu (ms) | Rendu (ms) | VRAM (Mo) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| fosses | Performance | 4 / 4 | 11.85 -> 17.15 | +5.30 (+44.8 %) | +6.43 (+41.6 %) | +2.73 (+12.3 %) | 0.0 -> 0.0 | 0.0 -> 0.0 | -0.35 (-8.1 %) | +0.12 (+2.5 %) | +5.34 (+45.2 %) | -38 (-1.5 %) |
| bassins | Performance | 4 / 4 | 12.53 -> 17.91 | +5.38 (+43.0 %) | +6.81 (+42.4 %) | +5.57 (+27.3 %) | 0.0 -> 0.0 | 0.0 -> 0.0 | -0.55 (-9.7 %) | +0.19 (+3.1 %) | +5.34 (+42.6 %) | -40 (-1.6 %) |
| marche0 | Performance | 4 / 4 | 12.16 -> 17.60 | +5.44 (+44.7 %) | +7.10 (+45.0 %) | +5.49 (+26.6 %) | 0.0 -> 0.2 | 0.0 -> 0.0 | -0.32 (-7.5 %) | +0.14 (+2.7 %) | +5.44 (+44.7 %) | -38 (-1.5 %) |
| marche1 | Performance | 4 / 4 | 11.49 -> 16.62 | +5.13 (+44.7 %) | +7.14 (+48.6 %) | +3.81 (+17.6 %) | 0.1 -> 0.6 | 0.1 -> 0.4 | -0.05 (-1.1 %) | +0.35 (+7.1 %) | +5.16 (+45.0 %) | -25 (-1.0 %) |
| lidar6 | Performance | 4 / 4 | 10.13 -> 14.79 | +4.65 (+45.9 %) | +5.42 (+41.2 %) | +6.05 (+38.3 %) | 0.0 -> 0.0 | 0.0 -> 0.0 | +0.03 (+0.7 %) | +0.47 (+9.0 %) | +4.70 (+46.5 %) | -25 (-1.0 %) |
| entites | Performance | 4 / 4 | 12.72 -> 18.77 | +6.05 (+47.6 %) | +7.84 (+47.9 %) | +8.01 (+42.5 %) | 0.0 -> 0.2 | 0.0 -> 0.0 | -0.40 (-8.2 %) | +0.42 (+7.4 %) | +6.06 (+47.7 %) | -9 (-0.4 %) |
| entites0 | Performance | 4 / 4 | 11.83 -> 17.51 | +5.69 (+48.1 %) | +7.50 (+49.8 %) | +7.51 (+41.1 %) | 0.6 -> 0.9 | 0.0 -> 0.2 | -0.39 (-8.9 %) | +0.32 (+6.4 %) | +5.71 (+48.4 %) | -27 (-1.1 %) |

## Streaming (avant -> apres)

| Scene | Profil | Construction (ms) | Demontage (ms) | Composants/s | Chargements synchrones | Saccades de PSO |
|---|---|---|---|---|---|---|
| fosses | Performance | +0.0 (-) | +0.0 (-) | +0.0 (-) | 0 -> 0 | 0 -> 0 |
| bassins | Performance | +0.0 (-) | +0.0 (-) | +0.0 (-) | 0 -> 0 | 0 -> 0 |
| marche0 | Performance | +0.0 (-) | -1.0 (-9.1 %) | +0.0 (-) | 0 -> 0 | 0 -> 0 |
| marche1 | Performance | +68.9 (+43.0 %) | +3.5 (+38.3 %) | +2.5 (+13.8 %) | 0 -> 0 | 0 -> 0 |
| lidar6 | Performance | +0.0 (-) | +0.0 (-) | +0.0 (-) | 0 -> 0 | 0 -> 0 |
| entites | Performance | +0.0 (-) | +0.0 (-) | +0.0 (-) | 0 -> 0 | 0 -> 0 |
| entites0 | Performance | +0.0 (-) | +0.0 (-) | +0.0 (-) | 0 -> 0 | 0 -> 0 |

## Postes du processeur graphique (ms, avant -> apres)

| Scene | Profil | Image | megalights | lumen | scene_rt | ombres | brouillard | translucidite_eau | post | base | eclairage | effets_ecran | scene_gpu | interface | autre |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| fosses | Performance | +0.25 (+6.4 %) | +0.00 (-) | -0.01 (-3.8 %) | +0.00 (-) | -0.16 (-15.4 %) | +0.02 (+11.4 %) | -0.01 (-2.8 %) | +0.00 (+0.0 %) | +0.02 (+2.3 %) | -0.03 (-12.5 %) | -0.01 (-5.6 %) | +0.00 (-) | +0.00 (+0.0 %) | -0.03 (-6.2 %) |
| bassins | Performance | +0.34 (+6.0 %) | +0.00 (-) | -0.02 (-1.9 %) | +0.00 (-) | -0.20 (-16.3 %) | -0.01 (-6.9 %) | +0.00 (+0.0 %) | +0.00 (+0.0 %) | +0.02 (+3.0 %) | -0.14 (-25.2 %) | +0.02 (+28.6 %) | +0.00 (+0.0 %) | +0.00 (+0.0 %) | +0.04 (+5.3 %) |
| marche0 | Performance | +0.84 (+21.8 %) | +0.00 (-) | +0.05 (+12.2 %) | +0.00 (-) | -0.21 (-23.4 %) | +0.00 (+0.0 %) | +0.01 (+6.7 %) | +0.02 (+2.0 %) | +0.02 (+2.6 %) | +0.01 (+6.2 %) | +0.01 (+5.9 %) | +0.00 (-) | +0.00 (+0.0 %) | +0.05 (+9.2 %) |
| marche1 | Performance | +1.96 (+57.1 %) | +0.00 (-) | +0.07 (+12.6 %) | +0.00 (-) | +0.00 (+0.0 %) | +0.00 (+0.0 %) | +0.00 (+3.4 %) | +0.03 (+4.0 %) | +0.02 (+2.2 %) | +0.10 (+52.6 %) | +0.02 (+28.6 %) | +0.01 (-) | -0.01 (-25.0 %) | +0.04 (+7.3 %) |
| lidar6 | Performance | +1.66 (+44.7 %) | +0.00 (-) | -0.03 (-6.2 %) | +0.00 (-) | +0.00 (-) | -0.01 (-8.0 %) | +0.00 (-) | +0.01 (+0.6 %) | -0.02 (-2.3 %) | +0.00 (+0.0 %) | +0.01 (+12.5 %) | +0.00 (+0.0 %) | +0.00 (+14.3 %) | +0.05 (+7.6 %) |
| entites | Performance | +0.99 (+21.0 %) | +0.00 (-) | -0.03 (-6.9 %) | +0.00 (-) | -0.01 (-1.2 %) | +0.01 (+3.2 %) | +0.00 (+2.4 %) | +0.00 (+0.0 %) | +0.01 (+0.6 %) | -0.10 (-27.4 %) | -0.01 (-5.6 %) | +0.00 (+0.0 %) | +0.00 (+0.0 %) | +0.04 (+5.3 %) |
| entites0 | Performance | +0.04 (+0.8 %) | +0.00 (-) | -0.05 (-15.5 %) | +0.00 (-) | -0.04 (-5.7 %) | -0.01 (-2.9 %) | -0.01 (-2.4 %) | -0.01 (-0.6 %) | +0.02 (+2.2 %) | -0.11 (-28.2 %) | +0.01 (+5.9 %) | +0.00 (-) | +0.00 (+0.0 %) | +0.05 (+9.9 %) |
