// v4.16 (vis) : regles geometriques des decalques d'usure, en C++ pur. ABRChunk::BuildDecals et ABRChunk::CeilingStainAt les
// appliquent ; le banc Tools/Visual/test_decals.cpp les verifie avec les dimensions des niveaux qui en ont (Levels 0, 1 et 2)
// et des tirages BRHash recopies : face du mur, plinthe, chambranle, plafond, taches au sol sous le mur, bornes par chunk,
// distance utile, variete d'un chunk a l'autre. Les tirages (0..1) sont ceux de BuildDecals, memes sels et meme ordre qu'en
// v4.15 : seules les regles marquees v4.16 changent une place.
//
// Reperes : "le long" (Along) est mesure depuis le milieu de la face du mur de la cellule ; Z depuis le sol de la cellule.
// Decalque projete : demi-tailles (profondeur de projection, demi-hauteur de l'image, demi-largeur de l'image).
#pragma once

#include <cmath>

namespace BRDecal
{
	// ---------------------------------------------------------------------------------------------------- bornes v4.16
	/** Decalques projetes (un composant chacun) par chunk, au plus. v4.16 (relecture) : au-dela, les tirages gardes sont
	 *  repartis sur tout le chunk (KeepUnderCap), dans l'ordre des cellules (le meme chez l'hote et chez les clients) ; avant,
	 *  les premiers tires seulement (un chunk plein restait nu dans ses dernieres colonnes). Ordre de grandeur : 15 a 25 par
	 *  chunk du Level 0 */
	constexpr int MaxProjectedPerChunk = 40;
	/** Taches au sol (instances d'un seul lot) par chunk, au plus (meme repartition) */
	constexpr int MaxFloorPatchesPerChunk = 24;
	/** Distance utile (cm) : un decalque s'efface entre 2/3 de cette distance et elle. Le moteur rapporte l'effacement a la
	 *  taille a l'ecran, donc a la largeur interne de l'image et au champ : v4.16 (relecture) FadeScreenSize est calcule pour la
	 *  vue locale (largeur interne, champ horizontal du reglage), puis recalcule quand elle change (ABRWorld) : 35 m a toute
	 *  definition. Avant la relecture : calcule pour 1920 px seulement (19 m a 1024 px, 70 m a 3840 px). Avant la v4.16 :
	 *  FadeScreenSize 0,01 (jusqu'a 116 m pour une aureole a 1920 px) */
	constexpr float UsefulDistance = 3500.f;
	/** Vue de reference (sans vue locale : serveur dedie, aucun ecran) */
	constexpr float ReferenceWidth = 1920.f;
	constexpr float ReferenceHFovDeg = 88.f;

	/** v4.16 (relecture) : sur Count tirages ranges dans l'ordre des cellules, le tirage Index est-il garde sous la borne Max ?
	 *  Exactement min(Count, Max) tirages gardes, repartis regulierement (jamais plus de ceil(Count / Max) - 1 ecartes de
	 *  suite) ; le meme resultat partout (entiers seulement) */
	inline bool KeepUnderCap(int Index, int Count, int Max)
	{
		if (Count <= Max)
		{
			return true;
		}
		if (Max <= 0 || Index < 0 || Index >= Count)
		{
			return false;
		}
		const long long Next = static_cast<long long>(Index + 1) * Max / Count;
		const long long Here = static_cast<long long>(Index) * Max / Count;
		return Next > Here;
	}
	/** Lot des taches au sol : distance d'affichage (cm) ; avant 4500 */
	constexpr float FloorPatchCull = 3500.f;

	// ---------------------------------------------------------------------------------------------------- geometrie (cm)
	/** Marge laissee aux poteaux de chaque bout d'une face de mur (cm) */
	constexpr float PostMargin = 10.f;
	/** Chambranle des portes (SM_L0_DoorCasing, ABRChunk::AddDoorway) : largeur (cm) ; une eraflure de jambage commence 3 cm
	 *  apres lui */
	constexpr float CasingWidth = 9.f;
	constexpr float JambStart = 12.f;
	/** Fond d'une eraflure au-dessus du sol (cm) : jamais sur le sol */
	constexpr float ScuffFloorClear = 6.f;
	/** Marge transparente du haut de l'aureole dans l'atlas (Tools/generate_textures.py : 48 px de 1024, puis fondu de 4 %) :
	 *  la part de l'image au-dessus du plafond ne se voit pas */
	constexpr float WaterStainTopMargin = 48.f / 1024.f;
	/** Taches au sol : hauteur au-dessus du sol (cm), lacet maximal (degres), marge sous le mur (cm) */
	constexpr float PatchLift = 0.6f;
	constexpr float PatchMaxJitterDeg = 8.f;
	constexpr float PatchUnderWallMargin = 5.f;

	/** Un decalque projete sur une face de mur : decalage le long de la face, hauteur du centre, demi-tailles */
	struct FWallDecal
	{
		float Along = 0.f;
		float Z = 0.f;
		float HalfDepth = 0.f;
		float HalfHeight = 0.f;
		float HalfWidth = 0.f;
		/** v4.16 : image retournee (rotation d'un demi-tour autour de l'axe de projection) */
		bool bFlipped = false;
	};

	/** Longueur utile d'une face de mur (sans les poteaux) : Inset = 0 contre une cellule pleine, epaisseur / 2 sinon */
	inline float FaceSpan(float CellSize, float Inset)
	{
		return CellSize - 2.f * Inset - 2.f * PostMargin;
	}

	/** Aureole d'infiltration sous le plafond (sels 7411, 7412, 7413) : l'eau descend du plafond le long du mur */
	inline FWallDecal WaterStain(float R1, float R2, float R3, float Span, float WallHeight)
	{
		FWallDecal D;
		const float Wd = std::fmin(70.f + 70.f * R1, Span);
		const float Ht = std::fmin(50.f + 70.f * R2, WallHeight - 60.f);
		D.Along = (R3 - 0.5f) * std::fmax(0.f, Span - Wd);
		D.Z = WallHeight + 0.05f * Ht - Ht * 0.5f;
		D.HalfDepth = 6.f;
		D.HalfHeight = Ht * 0.5f;
		D.HalfWidth = Wd * 0.5f;
		return D;
	}

	/** Eraflures basses (sels 7414 a 7417 ; v4.16 : 7418, image retournee une fois sur deux) */
	inline FWallDecal WallScuff(float R4, float R5, float R6, float R7, float RFlip, float Span)
	{
		FWallDecal D;
		const float Wd = std::fmin(40.f + 50.f * R4, Span);
		const float Ht = 18.f + 14.f * R5;
		D.Along = (R6 - 0.5f) * std::fmax(0.f, Span - Wd);
		D.Z = ScuffFloorClear + Ht * 0.5f + 16.f * R7;
		D.HalfDepth = 5.f;
		D.HalfHeight = Ht * 0.5f;
		D.HalfWidth = Wd * 0.5f;
		D.bFlipped = RFlip < 0.5f;
		return D;
	}

	/** Longueur d'un jambage de porte (de l'ouverture au poteau), largeur de passage DoorWidth */
	inline float JambLength(float CellSize, float WallThickness, float DoorWidth)
	{
		return (CellSize - WallThickness - DoorWidth) * 0.5f;
	}

	/** Le jambage recoit-il une eraflure (assez long pour la marge du chambranle et celle du poteau) */
	inline bool JambTakesScuff(float Jamb)
	{
		return Jamb > 26.f;
	}

	/** Eraflures a cote d'un passage, sur le jambage (sels 7431 a 7435 ; v4.16 : 7436, image retournee une fois sur deux) */
	inline FWallDecal JambScuff(float R1, float R2, float R3, float R4, float R5, float RFlip, float DoorWidth, float Jamb)
	{
		FWallDecal D;
		const float Wd = std::fmin(25.f + 30.f * R1, Jamb - 16.f);
		const float Sgn = R2 < 0.5f ? -1.f : 1.f;
		D.Along = Sgn * (DoorWidth * 0.5f + JambStart + Wd * 0.5f + std::fmax(0.f, Jamb - 16.f - Wd) * R3);
		const float Ht = 15.f + 15.f * R4;
		D.Z = 5.f + Ht * 0.5f + 20.f * R5;
		D.HalfDepth = 5.f;
		D.HalfHeight = Ht * 0.5f;
		D.HalfWidth = Wd * 0.5f;
		D.bFlipped = RFlip < 0.5f;
		return D;
	}

	/** Tache au sol (plan du moteur de 100 x 100 cm) : centre (le long, vers la piece depuis la face), dimensions, lacet ajoute */
	struct FFloorPatch
	{
		float Along = 0.f;
		float Out = 0.f;
		float Length = 0.f;
		float Width = 0.f;
		float YawJitterDeg = 0.f;
		/** Part de la largeur passee sous le mur (cm) */
		float Under = 0.f;
	};

	/** Moquette humide au pied d'un mur (sels 7421 a 7424) : une partie passe sous le mur, sans ressortir de l'autre cote.
	 *  Behind : matiere derriere la face (epaisseur du mur, ou cellule pleine) */
	inline FFloorPatch WallFootPatch(float R1, float R2, float R3, float R4, float Span, float Behind)
	{
		FFloorPatch P;
		P.Length = std::fmin(100.f + 120.f * R1, Span + 20.f);
		P.Width = 60.f + 60.f * R2;
		P.Along = (R3 - 0.5f) * std::fmax(0.f, Span + 20.f - P.Length);
		const float MaxIn = std::fmax(0.f, Behind - 0.5f * P.Length * std::sin(PatchMaxJitterDeg * 3.14159265f / 180.f) - PatchUnderWallMargin);
		P.Under = std::fmin(0.5f * P.Width, MaxIn);
		P.Out = 0.5f * P.Width - P.Under;
		P.YawJitterDeg = (R4 - 0.5f) * 2.f * PatchMaxJitterDeg;
		return P;
	}

	/** Seuil humide sous un passage (sels 7441 a 7443), centre sur la ligne du mur */
	inline FFloorPatch ThresholdPatch(float R1, float R2, float R3, float DoorWidth, float WallThickness)
	{
		FFloorPatch P;
		P.Length = DoorWidth * (0.9f + 0.3f * R1);
		P.Width = WallThickness + 80.f + 80.f * R2;
		P.YawJitterDeg = (R3 - 0.5f) * 20.f;
		return P;
	}

	/** Tache d'eau au plafond pres d'un plafonnier (sels 7451 a 7453) : rayon, angle et ecart au diffuseur */
	struct FCeilingStain
	{
		float X = 0.f;
		float Y = 0.f;
		float Radius = 0.f;
		float SpinRad = 0.f;
	};

	/** Centre relatif au centre de la cellule ; (LightX, LightY) : decalage du plafonnier dans la cellule. Le centre reste a
	 *  Radius / 2 des faces des murs : la tache peut passer sous un mur de moitie au plus (la partie cachee par le mur), jamais
	 *  jusqu'a la cellule voisine tant que Radius / 2 < epaisseur du mur (banc) */
	inline FCeilingStain CeilingStain(float R1, float R2, float R3, float LightX, float LightY, float FixtureHalfLength, float CellSize, float WallThickness)
	{
		FCeilingStain St;
		St.Radius = 35.f + 30.f * R1;
		const float Ang = R2 * 2.f * 3.14159265f;
		const float Dist = FixtureHalfLength * 0.6f + St.Radius * 0.7f;
		const float Lim = std::fmax(0.f, CellSize * 0.5f - WallThickness * 0.5f - St.Radius * 0.5f);
		St.X = std::fmin(std::fmax(LightX + std::cos(Ang) * Dist, -Lim), Lim);
		St.Y = std::fmin(std::fmax(LightY + std::sin(Ang) * Dist, -Lim), Lim);
		St.SpinRad = R3 * 2.f * 3.14159265f;
		return St;
	}

	// ---------------------------------------------------------------------------------------------------- effacement v4.16
	/** Facteur de champ du moteur (DecalRenderingShared.cpp, CalculateDecalFadeAlpha) : (2 / P[0][0]) / largeur x 600 */
	inline float EngineFovFactor(float ViewWidth, float HFovDeg)
	{
		return 2.f * std::tan(HFovDeg * 0.5f * 3.14159265f / 180.f) / (ViewWidth > 1.f ? ViewWidth : 1.f) * 600.f;
	}

	/** FadeScreenSize d'avant la v4.16 (le meme pour tous) : gardee comme plancher, un petit decalque s'efface toujours aussi tot */
	constexpr float LegacyFadeScreenSize = 0.01f;

	/** UDecalComponent::FadeScreenSize d'un decalque de demi-taille maximale MaxHalfSize, pour une vue de ViewWidth px de large
	 *  (largeur interne) et de champ horizontal HFovDeg : invisible au plus tard a UsefulDistance (le moteur : visible tant que
	 *  demi-taille / distance > FadeScreenSize x facteur de champ ; fondu sur 1/3) ; un petit decalque s'efface plus tot, comme
	 *  avant (plancher LegacyFadeScreenSize). v4.16 (relecture) : la vue est un parametre (avant : 1920 px toujours) */
	inline float FadeScreenSizeFor(float MaxHalfSize, float ViewWidth, float HFovDeg)
	{
		const float Fss = MaxHalfSize / (UsefulDistance * EngineFovFactor(ViewWidth, HFovDeg));
		return Fss > LegacyFadeScreenSize ? Fss : LegacyFadeScreenSize;
	}
	inline float FadeScreenSizeFor(float MaxHalfSize)
	{
		return FadeScreenSizeFor(MaxHalfSize, ReferenceWidth, ReferenceHFovDeg);
	}

	/** v4.16 (relecture) : la vue a change assez pour recalculer l'effacement des decalques deja crees (3 % de la largeur, 1
	 *  degre de champ) */
	inline bool FadeViewChanged(float AppliedWidth, float AppliedHFovDeg, float ViewWidth, float HFovDeg)
	{
		return std::fabs(ViewWidth - AppliedWidth) > AppliedWidth * 0.03f || std::fabs(HFovDeg - AppliedHFovDeg) > 1.f;
	}

	/** Distance (cm) ou le decalque disparait pour une vue donnee (largeur interne, champ horizontal) */
	inline float FadeOutDistance(float MaxHalfSize, float FadeScreenSize, float ViewWidth, float HFovDeg)
	{
		const float K = FadeScreenSize * EngineFovFactor(ViewWidth, HFovDeg);
		return K > 0.f ? MaxHalfSize / K : 1.0e9f;
	}
}
