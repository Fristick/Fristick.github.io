// Types partages : definitions de niveaux, surfaces, entites, hachage deterministe.
#pragma once

#include "CoreMinimal.h"

/** Algorithme de generation de la grille d'un niveau */
enum class EBRLayout : uint8
{
	Rooms,    // salles irregulieres (Niveau 0, 1, 4, 6, 37)
	Maze,     // labyrinthe de couloirs (Niveau 2, 3)
	Hotel,    // couloirs reguliers bordes de chambres (Niveau 5)
	Caves,    // grottes (Niveau 8)
	Open,     // espace ouvert (Niveau 10)
	Suburbs,  // quartier pavillonnaire (Niveau 9)
	City      // ville (Niveau 11)
};

enum class EBRFixture : uint8 { None, Panel, Tube, Bulb, Sconce, SkyPanel, StreetLamp };
enum class EBRSky : uint8 { None, Overcast, Night, Day };
enum class EBRStep : uint8 { Carpet, Hard, Water, Grass };
enum class EBRProps : uint8 { None, Warehouse, Pipes, Electrical, Office, Hotel, Caves, Field, Suburbs, City };
enum class EBREdge : uint8 { Open, Wall, Door };

/** Objets d'inventaire */
enum class EBRItem : uint8
{
	None,
	AlmondWater,  // eau d'amande : sante mentale
	Bandage,      // bandage : sante
	Battery,      // piles : lampe / camescope
	EnergyBar,    // barre energetique : endurance
	VHSTape,      // cassette VHS (objectif)
	Flashlight,   // lampe torche (main ou ceinture)
	Camcorder,    // camescope (main) : REC + vision nocturne
	Headlamp,     // lampe frontale (tete)
	Vest,         // gilet de protection (torse)
	Note,         // note (lue immediatement, va dans le journal)
	Count
};

/** Emplacements d'equipement (panneau de droite de l'inventaire) */
enum class EBREquipSlot : uint8 { Head, Chest, Hand, Belt, Count, None };

/** Une case d'inventaire */
struct FBRItemSlot
{
	EBRItem Item = EBRItem::None;
	int32 Count = 0;
	bool IsEmpty() const { return Item == EBRItem::None || Count <= 0; }
	void Clear() { Item = EBRItem::None; Count = 0; }
};

/** Groupe de cases (poches, sac, equipement) */
enum class EBRSlotGroup : uint8 { Pockets, Storage, Equipment };

enum class EBREntityKind : uint8
{
	Smiler,
	Hound,
	Faceling,
	SkinStealer,
	Deathmoth,
	Wretch,
	Partygoer,
	Clump,
	Bacteria,
	Count
};

enum class EBRExitStyle : uint8
{
	NoclipWall,   // pan de mur qui "glitche" : il suffit de le toucher
	NoclipFloor,  // zone du sol qui glitche
	Door,         // porte de secours (E pour ouvrir)
	HotelDoor,
	Elevator,
	Ladder,
	Barn,
	HouseDoor,
	BuildingDoor
};

/** Apparence d'une surface (texture projetee dans l'espace monde) */
struct FBRSurface
{
	FName Texture = NAME_None;
	FLinearColor Tint = FLinearColor::White;
	float Scale = 200.f;      // taille d'une repetition de texture en cm
	float Roughness = 0.85f;
	float Metallic = 0.f;
	float Grime = 0.35f;      // intensite de la salete a grande echelle
	float SelfIllum = 0.f;    // fausse lumiere ambiante
	FLinearColor Emissive = FLinearColor::Black;
	float Caustics = 0.f;     // reflets d'eau animes (Poolrooms)
	float FloorGrime = 0.f;   // salete au pied des murs

	FBRSurface() {}
	FBRSurface(FName InTex, const FLinearColor& InTint, float InScale, float InRough = 0.85f, float InGrime = 0.35f)
		: Texture(InTex), Tint(InTint), Scale(InScale), Roughness(InRough), Grime(InGrime) {}

	FString Key() const
	{
		return FString::Printf(TEXT("%s|%.3f,%.3f,%.3f|%.0f|%.2f|%.2f|%.2f|%.2f|%.2f,%.2f,%.2f|%.2f|%.2f"), *Texture.ToString(),
			Tint.R, Tint.G, Tint.B, Scale, Roughness, Metallic, Grime, SelfIllum, Emissive.R, Emissive.G, Emissive.B, Caustics, FloorGrime);
	}
};

struct FBREntitySpawn
{
	EBREntityKind Kind = EBREntityKind::Smiler;
	float Weight = 1.f;
};

struct FBRExitDef
{
	int32 Target = 1;            // numero du niveau cible (-1 = aleatoire)
	EBRExitStyle Style = EBRExitStyle::Door;
	float ChancePerChunk = 0.2f;
};

/** Description complete d'un niveau */
struct FBRLevelDef
{
	int32 Number = 0;
	FString Title;          // titre du wiki
	FString Nickname;       // surnom (FR)
	FString Description;    // description courte (FR)
	int32 SurvivalClass = 1;
	FString ClassText;

	// --- Grille ---
	EBRLayout Layout = EBRLayout::Rooms;
	float CellSize = 350.f;
	float WallHeight = 290.f;
	float WallThickness = 20.f;
	int32 ChunkCells = 8;
	float ViewDistance = 5000.f;
	float WallLineChance = 0.55f;
	int32 SegmentLength = 4;
	float DoorChance = 0.3f;
	float DoorWidth = 140.f;
	float PillarChance = 0.04f;
	float PillarSize = 50.f;
	float SolidChance = 0.f;
	float LoopChance = 0.12f;
	float OpenZoneChance = 0.15f;
	int32 Spacing = 6;
	bool bCeiling = true;
	bool bTrim = true;
	bool bLintels = true;

	FBRSurface Floor, Wall, Ceiling, Trim, Pillar, Solid, Road;

	// --- Eclairage ---
	EBRFixture Fixture = EBRFixture::Panel;
	float LightChance = 0.45f;
	float BrokenChance = 0.05f;
	float FlickerChance = 0.06f;
	float LightLumens = 2600.f;
	FLinearColor LightColor = FLinearColor(1.f, 0.95f, 0.82f);
	float LightRadius = 650.f;
	float DarkZoneChance = 0.f;
	float ShadowChance = 0.25f;

	// --- Atmosphere ---
	float FogDensity = 0.01f;
	FLinearColor FogColor = FLinearColor(0.3f, 0.28f, 0.2f);
	float FogStart = 0.f;
	float FogFalloff = 0.02f;
	bool bVolumetricFog = false;
	EBRSky Sky = EBRSky::None;
	float SunLux = 6.f;
	FLinearColor SunColor = FLinearColor(1.f, 0.97f, 0.92f);
	float SunPitch = -50.f;
	FLinearColor SceneTint = FLinearColor::White;
	float Saturation = 1.f;
	float Contrast = 1.f;
	float ExposureBias = 0.f;
	float MinEV = 2.f;
	float MaxEV = 10.f;
	float Vignette = 0.45f;
	float Grain = 0.25f;
	float Bloom = 0.6f;

	// --- Audio ---
	FName AmbientSound = NAME_None;
	float AmbientVolume = 0.6f;
	FName HumSound = NAME_None;
	float HumVolume = 0.f;
	EBRStep Step = EBRStep::Carpet;

	// --- Gameplay ---
	float SanityDrain = 0.12f;      // points / seconde
	TArray<FBREntitySpawn> Entities;
	int32 MaxEntities = 0;
	float SpawnInterval = 45.f;
	float AlmondWaterChance = 0.3f; // par chunk
	float BatteryChance = 0.2f;
	float NoteChance = 0.15f;
	TArray<FBRExitDef> Exits;
	TArray<FString> Notes;
	EBRProps Props = EBRProps::None;
	float PropDensity = 0.1f;
	bool bWater = false;
	float WaterHeight = 45.f;
	FBRSurface Water;
	/** Proprietes optiques de l'eau (Single Layer Water) : plus elles sont basses, plus l'eau est limpide */
	float WaterAbsorption = 1.2f;
	float WaterScattering = 0.15f;
	/** Mouvement de l'eau : houle (deplace la surface) et clapot (vaguelettes qui plient les reflets) */
	float WaterWaves = 1.f;
	float WaterChop = 1.f;
	/** Bassins profonds (Niveau 37) : proportion de blocs 2x2 creuses et profondeur sous le sol */
	float PoolChance = 0.f;
	float PoolDepth = 260.f;
	/** Grandes verrieres inclinees a la jonction mur / plafond (par chunk) */
	float SkylightChance = 0.f;

	// --- v3.2 : Niveau 0 ---
	/** Une entite fait des rondes autour du joueur (elle passe regulierement dans son champ de vision) */
	bool bPatrolEntity = false;
	EBREntityKind PatrolKind = EBREntityKind::Bacteria;
	float PatrolDelay = 25.f;         // avant sa premiere apparition (s)
	/** > 0 : les lampes virent au rouge a moins de ce rayon (cm) de l'entite qui fait des rondes */
	float RedLightRadius = 0.f;
	/** Smilers qui apparaissent dans le noir a chaque coupure de courant */
	int32 BlackoutSmilers = 0;
	/** Cachettes (placards, trous dans le mur) par chunk : les entites ne voient pas un joueur cache */
	float HidingSpotChance = 0.f;
	bool bOutdoor = false;
	bool bPhenomena = false;  // bruits de pas lointains, etc.

	// --- v2 : objectifs, coupures de courant, objets ---
	bool bRequireObjectives = false; // les sorties ne fonctionnent qu'une fois les objectifs remplis
	int32 VHSRequired = 6;
	bool bBlackouts = false;
	float BlackoutFirst = 90.f;      // delai avant la premiere coupure
	float BlackoutMinInterval = 140.f;
	float BlackoutMaxInterval = 280.f;
	float VHSChance = 0.15f;         // par chunk
	float BandageChance = 0.12f;
	float EnergyBarChance = 0.1f;
	float GearChance = 0.03f;        // lampe, frontale, gilet
	float WallDetailChance = 0.f;    // prises, aerations
};

/** Reglages du joueur (onglet Parametres de l'inventaire), sauvegardes dans GameUserSettings.ini */
struct FBRSettings
{
	float Sensitivity = 1.f;
	bool bInvertY = false;
	float FOV = 88.f;
	int32 Quality = 3;          // 0 Bas .. 4 Cinematique
	bool bHardwareRT = true;    // Lumen en ray tracing materiel (RTX)
	bool bRTHitLighting = false;
	bool bAreaLights = true;    // neons en lumieres surfaciques
	bool bVolumetricFog = true;
	bool bFilmGrain = true;
	/** Effet camescope / VHS : viseur REC, cadres, lignes de balayage, aberration, grain, salete d'objectif */
	bool bVHSEffect = true;
	/** Eau translucide (toujours visible) au lieu de l'eau Single Layer Water (prochain chargement de niveau) */
	bool bTranslucentWater = false;

	static FBRSettings& Get()
	{
		static FBRSettings Settings;
		return Settings;
	}
};

/** Hachage entier deterministe (pas d'etat global : le meme monde a chaque graine) */
namespace BRHash
{
	FORCEINLINE uint32 Mix(uint32 X)
	{
		X ^= X >> 16; X *= 0x7feb352dU;
		X ^= X >> 15; X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}

	FORCEINLINE uint32 Hash(int32 A, int32 B, int32 C, uint32 Seed)
	{
		uint32 H = Mix(static_cast<uint32>(A) * 0x9E3779B1u ^ Seed);
		H = Mix(H ^ (static_cast<uint32>(B) * 0x85EBCA77u));
		H = Mix(H ^ (static_cast<uint32>(C) * 0xC2B2AE3Du));
		return H;
	}

	FORCEINLINE float Rand(int32 A, int32 B, int32 C, uint32 Seed)
	{
		return static_cast<float>(Hash(A, B, C, Seed) & 0xFFFFFFu) / 16777216.f;
	}

	FORCEINLINE int32 FloorDiv(int32 A, int32 B)
	{
		return (A >= 0) ? (A / B) : -((-A + B - 1) / B);
	}

	FORCEINLINE int32 PosMod(int32 A, int32 B)
	{
		const int32 M = A % B;
		return M < 0 ? M + B : M;
	}
}
