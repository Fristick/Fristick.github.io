# Gameplay v4.17 : l'image change, les règles non

Les règles de la v4.16 ([`GAMEPLAY_v4.16.md`](GAMEPLAY_v4.16.md)) restent valables. La v4.17 est une passe graphique et
d'interface : elle n'ajoute aucun système de jeu, ne change ni la lumière de jeu, ni les niveaux et leur ordre, ni le
réseau (protocole 416). Ce document dit seulement ce que le joueur voit ou fait autrement. Tests, mesures et limites :
[`RAPPORT_v4.17.md`](RAPPORT_v4.17.md). Les appuis de manette et de clavier ont été injectés par les tests : aucune vraie
manette n'a été essayée.

## 1. Réglages d'image

- **VHS désactivé** : l'image est vraiment nette. Plus de frange colorée de base, plus de saleté ni de reflets d'objectif.
  Le mode caméscope (VHS activé) ne change pas.
- **GRAIN DU CAPTEUR** (nouvelle ligne de PARAMÈTRES) : désactivé, plus aucun grain d'image, même quand la santé mentale
  baisse, sous les parasites ou en vision nocturne.
- L'aide aux parasites du Level 6 reste un signal de jeu, indépendant de ces deux réglages.

## 2. Ce que l'on voit autrement

- **Level 0** :
  - papier peint moins clair et moins contrasté, joints des lés visibles, cadres des plafonniers en tôle peinte ;
  - diffuseurs lisibles (plus d'écrêtage) ; haut des murs et coins plus sombres ; arêtes chanfreinées des plinthes et des
    chambranles ;
  - en **Performance** (Level 0 et Poolrooms), les dalles éclairent vers le bas comme dans les autres profils : plus de
    halo blanc au plafond. Les autres niveaux gardent l'éclairage de la v4.16 dans ce profil.
- **Poolrooms** : plus de reflets d'eau au plafond ni sur les trottoirs secs (seulement sous et près de l'eau) ; plafond
  moins clair ; voile plus léger.
- **Niveaux 1 et 2** : moins de flaques brillantes.
- **Combinaison** (joueur et coéquipiers) : plis et coutures en relief, PVC brillant sous une lumière froide, visière en
  verre sombre.
- **Créatures** :
  - la Bacteria pose ses pieds et ses griffes au sol, avance par petits bonds et replie ses bras près des murs ;
  - les marcheurs gardent leur pied d'appui au sol et piétinent en tournant.
- **Corps du joueur** (3e personne, coéquipiers) : cadence tirée de la jambe, pieds posés au sol, bas du corps en léger
  retard dans les virages.

Les mécanismes, les indices et la lumière qui décide des créatures et de la santé mentale ne dépendent toujours ni du
profil, ni du ray tracing, ni de FLASHS.

## 3. Inventaire (Tab par défaut)

- **Lisibilité** :
  - textes plus grands (corps de 12 points à 1080 lignes, plancher de 7, de 8 en chinois, japonais, coréen, arabe et persan) ;
  - cases, légendes et silhouette lisibles ; INSPECTER et l'infobulle opaques ;
  - icônes sombres (caméscope, lampe, frontale, piles, cassette) cernées d'un contour clair ;
  - « VIDE » et les étapes encore inconnues du carnet sans transparence.
- **Barre rapide en jeu** : la touche de chaque poche s'affiche en pastille sombre, au-dessus de l'icône.
- **Manette** :
  - **PERSONNAGE** : Y prend l'objet de la case choisie, puis le pose ; A le pose aussi ; B annule. Le pied d'INSPECTER
    n'annonce plus Y ;
  - **PARAMÈTRES** : la croix choisit une ligne (haut, bas) et règle sa valeur (gauche, droite) ; A l'active ; LT et RT
    changent de catégorie ;
  - **CARNET et JOURNAL** : la croix gauche ou droite choisit une colonne (annoncé dans le pied), la croix haut ou bas
    fait défiler d'une ligne, le stick droit fait défiler ;
  - LB et RB changent toujours d'onglet ; les commandes marchent aussi dans l'inventaire ouvert depuis la pause.
- **Clavier seul** (nouveau) :
  - les flèches choisissent une case dans PERSONNAGE et une ligne dans PARAMÈTRES, et font défiler ailleurs ; le curseur
    s'affiche ;
  - **Entrée** utilise, équipe ou pose, et active une ligne de réglage ;
  - la touche **INTERAGIR** (E par défaut) inspecte ; la touche **LAMPE** (F par défaut) prend puis pose un objet ;
  - le pied de PERSONNAGE l'indique ; un mouvement de la souris rend la main à la souris.
- Toujours **aucune barre de vie** : endurance et santé mentale dans PERSONNAGE seulement.

## 4. Ce qui ne change pas

- Les 12 niveaux et leur ordre (`0 → 1 → 37 → 2 → 3 → 4 → 5 → 6 → 8 → 9 → 10 → 11 → dernier quai`) ; premier chapitre
  public 0, 1, 37, 2.
- Level 0 sans objectifs ; porte de service tirée et sauvegardée ; fosses réelles.
- Level 6 : noir et scanner LiDAR.
- Coopération jusqu'à 4, 22 langues, commandes reconfigurables, trois modes d'affichage, sauvegardes.
- Lumière de jeu (rayon de 620 cm, loi et seuils) : la portée **visible** des plafonniers du Level 0 passe à 560 cm,
  la règle non.
- Le réglage FLASHS : les néons qui clignotent restent plus rapides en « normaux » qu'en « atténués » (3 clignements par
  seconde au plus) ou « aucun » (rapport § 8).
