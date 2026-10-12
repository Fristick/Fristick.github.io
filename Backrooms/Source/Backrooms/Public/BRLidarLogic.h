// v4.13 : Level 6 sans lumiere : logique pure de l'aile d'essai et du scanner LiDAR, sans dependance au moteur.
//
// Le jeu (ABRWorld, ABRLidar, ABREntity) et le banc Tools/Lidar/test_lidar.cpp utilisent ces fonctions et ces cotes.
//
// L'aile d'essai (« aile LiDAR ») est une salle fermee de 4 x 4 cellules, posee d'apres la graine seule dans un chunk
// a deux chunks du depart (meme aile chez tous les joueurs, a chaque chargement). Reperes locaux : U = profondeur depuis
// le cote d'entree (0 .. 3), V = rang lateral (0 .. 3).
//   - Cote d'entree (tourne vers le depart) : quatre ouvertures, une par cellule, fermees par les quatre portes du
//     passage (mecanismes de la mission). Les trois autres cotes sont des murs pleins.
//   - Rangees U = 0 .. 2 : zone de danger. Trous reels dans le sol aux angles interieurs des dalles, aux limites U = 1 et
//     U = 2 (six trous de PitHoleSize). Les passages entre deux trous gardent au moins PitPassage : on traverse a pied,
//     sans jamais sauter, en voyant les trous au scanner.
//   - Rangee U = 3 : quatre alcoves separees par des murs, chacune ouverte sur la rangee 2. L'echelle de sortie (vers le
//     Niveau 8) est dans l'une d'elles, avec l'emetteur de parasites ; les trois autres sont vides.
//
// Scanner : impulsion large (rayons repartis sur plusieurs images, temps de recharge) et scan focalise maintenu (cone
// etroit). Les points ont une duree de vie bornee, une capacite bornee (anneau), et une categorie (couleur ET forme).
#pragma once

#include <cstdint>

namespace BRLidar
{
	/** Taille de l'aile (cellules) */
	constexpr int WingCells = 4;
	/** Anneau de chunks ou l'aile est posee (distance de Tchebychev au chunk du depart) */
	constexpr int WingRing = 2;

	struct FWing
	{
		bool bValid = false;
		int ChunkX = 0, ChunkY = 0;
		/** Rectangle de l'aile (cellules), [Min, Min + WingCells) */
		int MinX = 0, MinY = 0;
		/** Normale du cote d'entree (de l'aile vers la galerie qui la longe), un axe */
		int OutX = -1, OutY = 0;
		/** Alcove de l'echelle (rang lateral V de la rangee U = 3) */
		int LadderSlot = 0;
	};

	/** Hachage de la grille (identique a BRHash::Hash du jeu) */
	uint32_t Hash3(int32_t A, int32_t B, int32_t C, uint32_t Seed);

	/** Aile d'une graine de niveau. ChunkCells : cellules par chunk (>= WingCells + 2) */
	FWing PickWing(uint32_t LevelSeed, int ChunkCells);
	bool Contains(const FWing& W, int X, int Y);
	/** Cellule (X, Y) -> repere local (U profondeur depuis l'entree, V lateral) ; false hors de l'aile */
	bool ToLocal(const FWing& W, int X, int Y, int& OutU, int& OutV);
	void ToCell(const FWing& W, int U, int V, int& OutX, int& OutY);

	/** Aretes de l'aile (memes valeurs que EBREdge du jeu) */
	enum EEdge : uint8_t { Open = 0, Wall = 1, Door = 2 };
	/** Arete entre deux cellules voisines du chunk de l'aile, dont l'une au moins est dans l'aile ; Open si aucune */
	EEdge EdgeBetween(const FWing& W, int AX, int AY, int BX, int BY);
	/** Trou au coin superieur (X + 1, Y + 1) de la cellule (X, Y), comme ABRWorld::HasPitAtCorner */
	bool HoleAtCorner(const FWing& W, int X, int Y);
	/** Couloir garanti du depart a l'entree de l'aile : chemin en L de la cellule (0, 0) a la galerie devant la porte
	 *  V = 1 (d'abord l'axe lateral, puis l'axe d'entree : il n'entre jamais dans l'aile). Un mur tire sur ce chemin
	 *  recoit une ouverture (porte) : l'aile est toujours atteignable a pied, par un chemin de longueur bornee */
	bool CorridorEdge(const FWing& W, int AX, int AY, int BX, int BY);
	/** Cellule de la galerie devant la porte V = 1 (fin du couloir, salle de la mission) */
	void CorridorEnd(const FWing& W, int& OutX, int& OutY);

	// ---------------------------------------------------------------------------------------------------- Scanner
	/** Categories de points : couleur ET forme (lisibles sans distinguer les couleurs) */
	enum ECategory : uint8_t
	{
		/** Murs, sol, plafond : petits carres blancs bleutes */
		Env = 0,
		/** Bord d'un trou, fond d'une fosse : triangles orange */
		Danger = 1,
		/** Mecanisme de mission, objet : losanges verts */
		Device = 2,
		/** Entite : croix rouges */
		Entity = 3,
		/** Reperes de l'equipe de L. au sol : chevrons bleus (dans le sens du chemin) */
		Marker = 4,
		/** Silhouettes et inscriptions des cages et des portes : points ambre */
		Glyph = 5,
		NumCategories = 6
	};

	struct FSettings
	{
		/** Taille des points (multiplicateur, 0,5 .. 2) */
		float PointSize = 1.f;
		/** Contraste (intensite emissive, 0,5 .. 2) */
		float Contrast = 1.f;
		/** Persistance (secondes, 4 .. 30) */
		float Persistence = 12.f;
	};
	FSettings Clamp(const FSettings& In);

	/** Impulsion large : rayons, angle du cone (degres, horizontal x vertical), recharge (s), rayons traces par image */
	constexpr int PulseRays = 1400;
	constexpr float PulseHalfYaw = 50.f;
	constexpr float PulseHalfPitch = 32.f;
	constexpr float PulseCooldown = 1.6f;
	constexpr int PulseRaysPerFrame = 220;
	/** Scan focalise : demi-angle (degres), rayons par image pendant l'appui */
	constexpr float FocusHalfAngle = 7.f;
	constexpr int FocusRaysPerFrame = 36;
	/** Portee (cm) */
	constexpr float Range = 2600.f;
	/** Points au plus (tous les groupes), par categorie au plus */
	constexpr int MaxPoints = 24000;
	/** Groupes d'age (anneau) : les points d'un groupe s'estompent ensemble, le plus ancien est vide puis reutilise */
	constexpr int AgeBuckets = 8;

	/** Direction du rayon I (0 .. N-1) d'une impulsion, dans le repere de la vue (X devant, Y droite, Z haut) :
	 *  spirale a angle d'or dans une ellipse (couverture uniforme, sans motif repete d'une impulsion a l'autre grace a
	 *  Phase) */
	void PulseDir(int I, int N, float Phase, float& OutX, float& OutY, float& OutZ);
	/** Direction du rayon I d'un scan focalise (disque uniforme dans le cone) */
	void FocusDir(int I, uint32_t Frame, float& OutX, float& OutY, float& OutZ);

	/** Anneau de points par groupes d'age : ou ranger un nouveau point, quel groupe vider */
	struct FRing
	{
		/** Duree de vie d'un groupe : Persistence / AgeBuckets */
		float BucketSeconds = 1.5f;
		int Capacity = MaxPoints;
		int Current = 0;
		float CurrentAge = 0.f;
		int Count[AgeBuckets] = {};
		/** Avance le temps ; true si le groupe courant change (l'ancien plus vieux groupe, retourne dans OutCleared, est
		 *  vide et devient le groupe courant) */
		bool Advance(float Dt, int& OutCleared);
		/** Reserve N points dans le groupe courant ; retourne le nombre accepte (capacite bornee par groupe) */
		int Reserve(int N);
		int Total() const;
		/** Age (s) du groupe B (0 pour le courant) et son attenuation (1 neuf .. 0 eteint) */
		float AgeOf(int B) const;
		float FadeOf(int B, float Persistence) const;
	};

	// --------------------------------------------------------------------------------------------- Menace lisible
	/** Regle des Smilers dans le noir : une impulsion large s'entend (rayon, cm) et attire ; un Smiler pris dans le scan
	 *  focalise pendant RepelHold secondes recule (RepelDistance cm) et ne revient pas avant RepelLock secondes */
	constexpr float PulseNoiseRadius = 1800.f;
	constexpr float RepelHold = 0.6f;
	constexpr float RepelDistance = 700.f;
	constexpr float RepelLock = 6.f;

	/** Aide de direction (sans le son) : intensite 0..1 des parasites a Distance (cm) de l'emetteur */
	float StaticLevel(float Distance);
	/** Portee de l'emetteur de parasites (son et aide) */
	constexpr float StaticRange = 3200.f;
}
