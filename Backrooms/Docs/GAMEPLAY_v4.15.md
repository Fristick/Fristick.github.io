# Gameplay v4.15 : le parcours linéaire

Les règles de la v4.13 et de la v4.14 ([`GAMEPLAY_v4.13.md`](GAMEPLAY_v4.13.md), [`GAMEPLAY_v4.14.md`](GAMEPLAY_v4.14.md))
restent valables : Level 0 sans énigme, porte de service tirée par partie, fil `0 → 1 → Poolrooms → 2 → 3 → …`, Level 6
sans lumière au scanner, REJOUER sans écriture de la campagne. Ce document décrit seulement ce que la v4.15 change pour
le joueur. Tout a été joué dans Unreal 5.8 par les tests du jeu (`-BRAutoTestV415`, `-BRAutoTestV415Fix`, suites
précédentes, test réseau) : résultats dans [`RAPPORT_v4.15.md`](RAPPORT_v4.15.md). Les
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

## 5. Correctifs (2e lot)

- **Bruits** : seuls les murs étouffent un bruit (portée réduite à 55 %). Avant, la capsule du joueur qui faisait le
  bruit, ou celle d'une créature grande qui écoutait (Wretch), coupait toujours la ligne : un Wretch attiré par un
  mécanisme en vue directe ne l'entendait qu'à la moitié de la portée prévue (manivelle 5,5 m au lieu de 10 m), et une
  impulsion LiDAR ne s'entendait qu'à 9,9 m au lieu de 18 m quand le Smiler n'a pas le joueur en vue. Un joueur ou une
  créature entre le bruit et l'auditeur ne l'étouffe plus.
- **Rejoindre une partie** : le personnage d'un joueur qui rejoint reste immobile tant que son niveau se prépare
  (aucun sol chez lui) ; l'hôte ne déduit plus de chute d'un joueur en chargement, ni de sa position d'avant le
  placement. Avant, un joueur qui rejoignait une partie au Level 0 tombait dans le vide pendant le chargement et
  l'hôte le tuait d'une chute juste après son arrivée. De même, un joueur réveillé au point de départ après une chute
  dans une fosse n'est plus tué une seconde fois : l'hôte attend son premier mouvement parti de sa nouvelle place.
  Partie seule : inchangé.
- **Départ de groupe devant une porte** (ascenseur, porte d'hôtel, porte de la mission) : la porte est posée sur la
  face du mur ; le point visé par la vue (demande et rassemblement) était exactement sur la surface du mur, et le
  départ pouvait être refusé devant la porte selon l'arrondi. Il est désormais 50 cm devant la porte, dans la pièce ;
  le point de rassemblement (zone de 8 m) et la portée de la porte (3,6 m) ne changent pas. En développement, une
  sortie dont le point visé serait pris dans un obstacle le signale dans le journal (vérifié quand la sortie devient
  visible et quand la mission l'ouvre ; pas tant que la porte de la mission la ferme).

## 6. Missions et sorties qui bloquaient (1er, 3e, 4e et 5e lots)

Trouvés en jouant les tests dans Unreal sur un nouveau PC (aucun de ces cas n'avait été joué avant). Le 5e lot ne touche
aucune mission : il corrige des sorties secondaires du parcours classique (échelles des chunks des Poolrooms). Le détail,
les bancs et les sorties réelles : [`Docs/v415/correctifs_places.txt`](v415/correctifs_places.txt),
[`correctifs_3.txt`](v415/correctifs_3.txt), [`correctifs_4.txt`](v415/correctifs_4.txt),
[`correctifs_5.txt`](v415/correctifs_5.txt).

- **Level 6, postes de confinement** : sur toutes les graines, la cage de confinement enfermait le point d'action de son
  poste ; la mission ne pouvait pas être finie. La cage et son poste sont maintenant aux deux bouts du mur (±58 cm), et
  la place devant le poste est libre.
- **Level 6, aile du scanner** : l'aile n'était atteinte que dans environ la moitié des graines (recherche limitée) ;
  elle l'est toujours. Les créatures ne cherchent plus leur chemin par une porte de l'aile restée fermée.
- **Niveau 8** : des mécanismes (environ la moitié des graines) et des échelles de sortie (57 %) étaient pris dans les
  rochers ; les rochers sont écartés des mécanismes et des sorties. L'échelle de sortie posée sur le palier de la
  passerelle ne se grimpait pas depuis le tablier incliné (depuis la v4.12) : le grimpeur glisse maintenant sur le sol
  praticable jusqu'au palier. Les échelles des chunks ne sont plus enfouies dans la roche de leur paroi (en coopération,
  le départ de groupe y était refusé).
- **Poolrooms, échelles vers le Niveau 2** (parcours classique, sorties secondaires ; celle de la mission marchait) :
  environ une sur cinq était posée au fond du canal contre la marche immergée du trottoir d'un mur voisin, où le
  grimpeur ne tenait pas. Elles sont maintenant posées là où il tient à plat, le plus souvent sur un trottoir ; environ
  2 % des zones qui en tiraient une n'en ont plus. Prise depuis plus bas (le canal sous un trottoir), la montée passe
  maintenant par-dessus la marche et le bord du trottoir au lieu de s'y arrêter (à vérifier en jeu : l'étape a tourné
  au dernier passage mais n'a pas pu se faire, faute de place libre dans le canal devant l'échelle essayée : non
  vérifié).
- **Niveau 9** : la maison ne recouvre plus le « Plan de maison » (porche).
- **Niveau 10, moulin** : les poteaux d'une même pièce avaient chacun leur orientation ; un mécanisme pouvait cacher son
  voisin à la visée (« Orientation du moulin » et « Frein du moulin »). Les poteaux d'une pièce sont maintenant en rang
  et le placement garde dégagée la ligne de visée de chaque mécanisme.
- **Graines** : chaque niveau tire sa disposition parmi 4 milliards de graines (32 bits) ; sous Windows il n'y en avait
  que 32 768.
- Ces changements de place valent pour les dispositions de la v4.15 (placement 3). Une mission de la v4.14 reprise
  annonce « mécanismes réaménagés » seulement si ses mécanismes ont changé de place (toujours au Level 6 ; au Niveau 10
  selon la graine) ; sinon elle garde sa progression.

## 7. Image plus réaliste (direction « Backrooms: Escape Together »)

- **Éclairage** : néons blanc chaud (3700 K) vus par une caméra réglée comme sous des tubes fluorescents (balance des
  blancs à 3900 K au Level 0) : un jaune plus naturel, moins saturé ; plage d'exposition resserrée (le noir d'une
  coupure reste noir, l'image ne « pompe » plus) ; adaptation de l'œil plus lente.
- **Néons** : amorçage qui clignote à l'allumage, tubes en fin de vie, rallumage décalé après une coupure, bourdonnement
  qui vient de chaque plafonnier. Avec « flashs réduits », aucun clignotement ne dépasse 3 par seconde.
- **Ombres de toutes les lampes** (nouvelle option, PARAMÈTRES > GRAPHISMES, MegaLights) : chaque plafonnier projette son
  ombre ; active dans QUALITÉ, RTX FLUIDE et CINÉMATIQUE, coupée dans PERFORMANCE, indisponible sans ray tracing
  matériel.
- **Lampe torche et frontale** : faisceau photométrique (profil IES, centre plus dense, halo doux) ; la lampe garde son
  blanc chaud quelle que soit la balance des blancs du niveau.
- **Brouillard** : brouillard volumétrique réglé par niveau (halos des lampes), air humide des salles de fosses et des
  zones sans lumière, air froid du sas de la porte de service.
- **Surfaces du Level 0** : papier peint en lés de 53 cm (joints, bords décollés), ligne de marée au-dessus de la
  plinthe, moquette bouclée au lustre de tissu avec des zones humides, dalles de plafond de 61 cm fissurées ; sol et
  plafond du kit posés cellule par cellule.
- **Usure** (Levels 0, 1 et 2) : auréoles sous le plafond, taches près des plafonniers (plus près des lampes mortes),
  moquette humide au pied des murs, éraflures. Tirées avec la graine : les mêmes chez tous les joueurs.
- **Premier lancement** : le profil graphique est choisi selon la carte graphique (annoncé à l'écran) ; il se change
  dans PARAMÈTRES > GRAPHISMES.
- **Inchangé** : la lumière de gameplay (ce que voient les créatures, `LightLevelAt`) ; elle ne tient pas encore compte
  des murs (décision de gameplay laissée ouverte).
