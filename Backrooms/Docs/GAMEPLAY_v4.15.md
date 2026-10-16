# Gameplay v4.15 : le parcours linéaire

Les règles de la v4.13 et de la v4.14 ([`GAMEPLAY_v4.13.md`](GAMEPLAY_v4.13.md), [`GAMEPLAY_v4.14.md`](GAMEPLAY_v4.14.md))
restent valables : Level 0 sans énigme, porte de service tirée par partie, fil `0 → 1 → Poolrooms → 2 → 3 → …`, Level 6
sans lumière au scanner, REJOUER sans écriture de la campagne. Ce document décrit seulement ce que la v4.15 change pour
le joueur. Rien de ceci n'a encore été joué dans Unreal : les tests du jeu sont préparés (`-BRAutoTestV415`). Les
règles détaillées (R-L1 à R-L6) : [`HISTOIRE_PROGRESSION_v4.15.md`](HISTOIRE_PROGRESSION_v4.15.md).

## 1. Toute nouvelle partie suit le parcours linéaire

- **NOUVELLE PARTIE** crée toujours une partie au **parcours linéaire** ; il n'y a pas de choix à faire (la page de
  création garde ses deux éléments : COMMENCER, RETOUR). La carte de la partie, page VOS PARTIES, porte la pastille
  **LINÉAIRE**.
- Les parties commencées avant la v4.15 (et toute partie migrée d'un format ancien) restent au **parcours classique** :
  rien ne change pour elles.
- REJOUER un niveau découvert, le mode développeur et une partie rejointe en invité d'une partie classique jouent
  toujours selon les règles classiques. Une mort en REJOUER relance le niveau rejoué (règle de la v4.14) ; l'écran de
  mort l'annonce désormais par « Le niveau recommence. ». Le texte de l'écran de mort suit toujours la règle appliquée.

## 2. Ce qui change dans une partie linéaire

| | Parcours linéaire | Parcours classique (inchangé) |
|---|---|---|
| Ordre des niveaux | le fil de l'histoire, `0 → 1 → Poolrooms → 2 → 3 → … → 11 → dernier quai` | le même fil |
| Disposition d'un niveau | **fixe pour la partie** : chaque niveau a sa graine, tirée une fois à la nouvelle partie (le Level 0 garde la sienne, comme avant) | neuve à chaque arrivée (Level 0 : celle de la partie) |
| Sorties | **une seule** : celle que garde la mission du niveau (porte, échelle, grange…) ; aucune sortie de retour, aucune autre sortie vers l'avant | sorties multiples, retours possibles |
| Mort en solo | **le niveau recommence de zéro** : même disposition, mission à refaire, équipement de départ ; texte « Le niveau recommence. » | retour au Level 0 |
| Mort en coopération | à terre, un coéquipier peut relever ; sinon réveil au départ du niveau avec l'équipement de départ (comme avant) | identique |
| Jeu fermé pendant une mort | CONTINUER recharge **le même niveau**, avec sa disposition, mission neuve, équipement de départ | CONTINUER ramène au Level 0, équipement de départ (corrigé en v4.15 : avant, CONTINUER reprenait au niveau de la mort) |

- La reprise d'une session (CONTINUER après une fermeture normale) garde comme avant la disposition, la mission en
  cours, les objets ramassés et le dernier point sûr.
- Le nombre de morts est compté comme avant.
- Sécurité : si la mission d'un niveau n'a pas pu poser sa sortie (cas qui ne devrait pas arriver), la sortie de
  progression tirée par le niveau reste ; seules les sorties de retour disparaissent. Une partie n'est jamais bloquée.

## 3. Coopération

- La partie de l'hôte décide : si elle est linéaire, chaque niveau est linéaire pour tout le groupe (le drapeau arrive
  avec le niveau, comme sa graine) ; chacun voit la même et unique sortie.
- La mort en coopération ne change pas : fenêtre de réanimation, puis réveil au point de départ du niveau. Le message
  ne prétend plus que l'équipement est resté là où l'on est tombé : on repart avec l'équipement de départ.
- Protocole réseau **415** : une partie 4.14 et une 4.15 ne se mélangent pas.

## 4. Sauvegardes

- Format **6** : deux champs de plus (parcours, graine du parcours). Une sauvegarde d'un format plus ancien est migrée
  en parcours classique, et une copie intacte est gardée (`BR_Partie_<n>_Format5.sav` pour une partie de la v4.14).
- Une partie de la v4.15 ouverte par une version plus ancienne est en lecture seule (règle des formats plus récents).
