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

	/** Impulsion large : rayons, angle du cone (degres, horizontal x vertical), recharge (s) */
	constexpr int PulseRays = 1400;
	constexpr float PulseHalfYaw = 50.f;
	constexpr float PulseHalfPitch = 32.f;
	constexpr float PulseCooldown = 1.6f;
	/** v4.14 : une impulsion trace ses rayons en PulseSeconds, quelle que soit la cadence d'images, sans depasser
	 *  PulseMaxPerFrame rayons dans une image (une image lente etale l'impulsion au lieu de la concentrer) */
	constexpr float PulseSeconds = 0.2f;
	constexpr int PulseMaxPerFrame = 260;
	/** Scan focalise : demi-angle (degres). v4.14 : rayons par seconde (la cadence de la v4.13 a 30 images/s), plafond par
	 *  image ; un rayon sur FocusCenterEvery suit l'axe de la vue */
	constexpr float FocusHalfAngle = 7.f;
	constexpr float FocusRaysPerSecond = 1110.f;
	constexpr int FocusMaxPerFrame = 48;
	constexpr int FocusCenterEvery = 37;
	/** Portee (cm) */
	constexpr float Range = 2600.f;
	/** Points au plus (toutes categories : somme des CategoryCapacity) */
	constexpr int MaxPoints = 24000;
	/** Groupes d'age par categorie (anneau) : les points d'un groupe s'estompent ensemble */
	constexpr int AgeBuckets = 8;

	/** Direction du rayon I (0 .. N-1) d'une impulsion, dans le repere de la vue (X devant, Y droite, Z haut) :
	 *  spirale a angle d'or dans une ellipse (couverture uniforme, sans motif repete d'une impulsion a l'autre grace a
	 *  Phase) */
	void PulseDir(int I, int N, float Phase, float& OutX, float& OutY, float& OutZ);
	/** Direction du rayon I d'un scan focalise (disque uniforme dans le cone) */
	void FocusDir(int I, uint32_t Frame, float& OutX, float& OutY, float& OutZ);

	// --------------------------------------------------------------- v4.14 : reserve de points, budgets, inscriptions
	/** Capacite reservee a une categorie (somme = MaxPoints) : le decor ne peut pas consommer la reserve des dangers, des
	 *  menaces, des mecanismes, des reperes et des inscriptions */
	int CategoryCapacity(int Cat);

	/** Reserve de points : un anneau de AgeBuckets groupes par categorie, sur une horloge absolue.
	 *   - Le groupe courant recoit les points pendant BucketSeconds() = Persistence / (AgeBuckets - 1), ou jusqu'a sa part
	 *     de la capacite (CategoryCapacity / AgeBuckets) ; le groupe suivant prend alors le relais. Le groupe reutilise est
	 *     le plus ancien : ses points sont remplaces (Replaced), jamais un nouveau point refuse tant qu'un ajout tient dans
	 *     un groupe. La memoire et le travail restent bornes.
	 *   - Un groupe expire quand son point le plus recent a Persistence secondes : TOUS les groupes expires sont vides a
	 *     chaque Advance, quelle que soit la duree ecoulee (longue image, suspension, persistance changee). L'age d'un
	 *     groupe n'est jamais raccourci. */
	struct FPointStore
	{
		double Now = 0.0;
		float Persistence = 12.f;
		int Current[NumCategories] = {};
		int Count[NumCategories][AgeBuckets] = {};
		double Start[NumCategories][AgeBuckets] = {};
		double Last[NumCategories][AgeBuckets] = {};
		/** Changements de groupe de la categorie (un chevron par repere et par groupe) */
		int Serial[NumCategories] = {};
		/** Mesures : points demandes, acceptes, remplaces (groupe le plus ancien reutilise), expires, refuses */
		long long Requested = 0;
		long long Accepted = 0;
		long long Replaced = 0;
		long long Expired = 0;
		long long Rejected = 0;

		void Reset();
		float BucketSeconds() const;
		/** Avance l'horloge, vide tous les groupes expires, change de groupe courant si sa duree est ecoulee.
		 *  OutCleared[Cat] : masque des groupes vides (a vider aussi au rendu) */
		void Advance(float Dt, uint8_t OutCleared[NumCategories]);
		/** Ajoute N points a la categorie ; renvoie le nombre accepte (N, sauf si N depasse la part d'un groupe) et le groupe
		 *  qui les recoit ; OutCleared : groupes vides pour faire place */
		int Insert(int Cat, int N, int& OutBucket, uint8_t& OutCleared);
		int Total() const;
		int Total(int Cat) const;
		/** Attenuation du groupe (1 neuf .. 0 expire), d'apres l'age de son point le plus recent */
		float FadeOf(int Cat, int B) const;
	};

	/** Budget par seconde, independant des images par seconde, avec un plafond par image ; une image lente ne rattrape pas
	 *  au-dela du plafond (pas d'arriere accumule) */
	struct FRateBudget
	{
		float Rate = 0.f;
		int MaxPerFrame = 0;
		float Carry = 0.f;
		int Take(float Dt);
		void Reset() { Carry = 0.f; }
	};

	/** Inscriptions relevees au scan focalise : plaque d'une porte (silhouette reduite + code 7 x 7) ou silhouette d'une
	 *  cage sous tension. Chaque inscription est dessinee au plus une fois par cible et par periode utile
	 *  (GlyphRefreshSeconds), progressivement, sous un budget de points commun a toutes les cibles : de nombreux rayons
	 *  sur la meme plaque ne la repetent pas */
	constexpr int PlateFigurePoints = 64;
	constexpr int CageFigurePoints = 175;
	constexpr float GlyphPointsPerSecond = 900.f;
	constexpr int GlyphMaxPerFrame = 120;
	constexpr int MaxGlyphTargets = 16;
	/** Symboles des codes (BRMission::NumSymbols, meme ordre que BRMissionText::Symbol) en grilles 7 x 7 */
	constexpr int NumSymbols = 6;
	bool SymbolCell(int Symbol, int Row, int Col);
	int SymbolCellCount(int Symbol);
	/** Points d'une plaque : silhouette puis cases du code (indices 0 .. PlateGlyphPoints - 1) */
	int PlateGlyphPoints(int Symbol);
	/** Point K de la plaque : true si c'est une case du code (OutRow, OutCol), false si c'est le point OutFigure de la
	 *  silhouette (parmi FigurePoints points de figure) */
	bool PlateGlyphPoint(int Symbol, int K, int FigurePoints, int& OutFigure, int& OutRow, int& OutCol);
	/** Point K (0 .. CageFigurePoints - 1) d'une silhouette de cage : indice parmi FigurePoints */
	int CageGlyphFigure(int K, int FigurePoints);
	/** Une inscription completee est redessinee apres cette duree (avant de s'eteindre), si la cible est encore scannee */
	float GlyphRefreshSeconds(float Persistence);

	struct FGlyphEmit
	{
		int Target = -1;
		int First = 0;
		int Count = 0;
	};
	struct FGlyphPlanner
	{
		struct FTarget
		{
			int Id = -1;
			int Total = 0;
			int Cursor = 0;
			double DoneAt = -1e30;
			double TouchedAt = -1e30;
			bool bTouched = false;
		};
		FTarget Targets[MaxGlyphTargets];
		FRateBudget Budget{ GlyphPointsPerSecond, GlyphMaxPerFrame, 0.f };
		/** Mesures : demandes de cibles (un rayon = une demande), cibles distinctes par image (apres deduplication) */
		long long Touches = 0;
		long long DistinctTouches = 0;

		void Reset();
		/** La cible a ete touchee par un rayon focalise dans cette image (dedupliquee) */
		void Touch(int Id, int Total, double Now, float Persistence);
		/** Repartit le budget de l'image entre les cibles touchees ; renvoie le nombre d'entrees. OutCompleted : cibles dont
		 *  l'inscription vient d'etre completee (une fois par passe) */
		int Plan(float Dt, double Now, FGlyphEmit* Out, int MaxOut, int* OutCompleted, int& NumCompleted);
	};

	// ------------------------------------------------------------------- v4.14 : image du scanner (jeu et banc)
	struct FHit
	{
		/** Mecanisme a inscriptions touche (porte, cage sous tension) : identifiant, nombre de points ; -1 aucun */
		int GlyphTarget = -1;
		int GlyphTotal = 0;
	};
	/** Ce que le jeu (ABRLidar) ou le banc fournit : tracer un rayon et poser ses points en attente, poser des points
	 *  d'inscription, compter les points en attente de l'image */
	class IScanHost
	{
	public:
		virtual ~IScanHost() {}
		/** Rayon dans le repere de visee (X devant) ; bPulse : repere fige au tir ; bCenter : axe de la vue */
		virtual FHit TraceRay(bool bPulse, bool bCenter, float X, float Y, float Z) = 0;
		/** Pose les points [First, First + Count) de l'inscription de la cible ; renvoie le nombre pose */
		virtual int EmitGlyphs(int Target, int First, int Count) = 0;
		virtual int PendingPoints(int Cat) const = 0;
	};
	struct FFrameResult
	{
		int PulseRays = 0;
		int FocusRays = 0;
		int GlyphPoints = 0;
		int Requested[NumCategories] = {};
		int Accepted[NumCategories] = {};
		int Bucket[NumCategories] = {};
		uint8_t Cleared[NumCategories] = {};
		int Completed[MaxGlyphTargets] = {};
		int NumCompleted = 0;
	};
	struct FScanState
	{
		FPointStore Store;
		FRateBudget PulseBudget{ PulseRays / PulseSeconds, PulseMaxPerFrame, 0.f };
		FRateBudget FocusBudget{ FocusRaysPerSecond, FocusMaxPerFrame, 0.f };
		FGlyphPlanner Glyphs;
		/** Impulsion en cours : prochain rayon (-1 aucune) */
		int PulseNext = -1;
		float PulsePhase = 0.f;
		float Cooldown = 0.f;
		uint32_t Frame = 0;
		/** Rayons focalises traces depuis le debut (choix du rayon d'axe) */
		uint32_t FocusCount = 0;

		void Reset();
		/** Impulsion ; false pendant la recharge */
		bool StartPulse(float Phase);
	};
	/** Une image : horloge et expiration, rayons d'impulsion et de focus du budget, inscriptions dedupliquees, insertion
	 *  bornee des points en attente. Le jeu applique ensuite Out.Cleared puis ajoute Out.Accepted points par categorie dans
	 *  le groupe Out.Bucket */
	void RunFrame(FScanState& S, float Dt, float Persistence, bool bFocus, IScanHost& Host, FFrameResult& Out);

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
