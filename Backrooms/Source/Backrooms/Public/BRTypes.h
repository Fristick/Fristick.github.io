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

/** v4.11 : emplacement d'un mecanisme : sur une face de mur (bWall) ou sur un poteau au milieu de la cellule (exterieur) */
struct FBRMissionSpot
{
	FVector Pos = FVector::ZeroVector;
	/** Orientation : +X local vers le joueur (sortie du mur) */
	float Yaw = 0.f;
	FIntPoint Cell = FIntPoint::ZeroValue;
	bool bWall = true;
	bool bValid = false;
	/** v4.12 : module physique construit par ce mecanisme (BRMech::Module : bassins, sas, passerelle ; 0 : aucun) */
	uint8 Module = 0;
};

/** v4.11 : reponse de l'hote a une demande de ramassage (transaction unique : un seul gagnant par objet) */
enum class EBRPickupResult : uint8
{
	Accepted,
	/** Deja attribue (un coequipier l'a pris, ou cette demande a deja ete servie) */
	AlreadyTaken,
	/** Trop loin de l'objet (distance 3D vue par l'hote) */
	TooFar,
	/** Un mur ou un plancher entre le joueur et l'objet */
	NotVisible,
	/** Joueur a terre ou mort */
	Dead,
	/** Joueur encore en chargement du niveau */
	Loading,
	/** Demande faite dans un autre niveau (ou pendant un changement de niveau) */
	StaleLevel,
	/** Objet inconnu de l'hote (identifiant ou type different) */
	Unknown,
	/** Plus de place pour cet objet */
	Full,
	/** Une demande est deja en attente */
	Busy,
	/** v4.12 : demande faite avant un reveil confirme par l'hote (inventaire d'une autre vie) : rien n'est attribue */
	StaleLife
};

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

/** v4.7 : cause de la mort d'un joueur : decide si un coequipier peut le relever, le delai avant le reveil et les messages.
 *  Fixee explicitement par le code qui tue (jamais deduite de la position) et repliquee par le serveur */
enum class EBRDeathCause : uint8
{
	None,
	Injury,     // frappe d'une entite (ou jumpscare mortel)
	Drowning,   // apnee epuisee
	Fall,       // chute dans une fosse (Niveau 0)
	Madness     // sante mentale a zero
};

namespace BRDeath
{
	/** Un coequipier peut relever un joueur mort de cette cause (le corps est atteignable) */
	FORCEINLINE bool CanRevive(EBRDeathCause C)
	{
		return C == EBRDeathCause::Injury || C == EBRDeathCause::Drowning || C == EBRDeathCause::Madness;
	}
	/** Cooperation : temps laisse aux coequipiers pour relever le joueur (s) ; 0 : reveil rapide au point de depart */
	FORCEINLINE float ReviveWindow(EBRDeathCause C)
	{
		switch (C)
		{
		case EBRDeathCause::Injury:
		case EBRDeathCause::Madness:
			return 30.f;
		case EBRDeathCause::Drowning:
			return 20.f; // il faut le sortir de l'eau vite
		default:
			return 0.f;
		}
	}
	/** v4.8 : identifiant stable de la cause (journal, tests) */
	FORCEINLINE const TCHAR* CauseId(EBRDeathCause C)
	{
		switch (C)
		{
		case EBRDeathCause::Injury: return TEXT("Injury");
		case EBRDeathCause::Drowning: return TEXT("Drowning");
		case EBRDeathCause::Fall: return TEXT("Fall");
		case EBRDeathCause::Madness: return TEXT("Madness");
		default: return TEXT("None");
		}
	}
	/** Message vu par les coequipiers. v4.8 : motif localise, compose par chaque machine dans sa langue ({Name} : le joueur) */
	FORCEINLINE FText TeammateMessage(EBRDeathCause C)
	{
		switch (C)
		{
		case EBRDeathCause::Drowning:
			return NSLOCTEXT("BR", "Death.Teammate.Drowning", "{Name} se noie : remontez-le !");
		case EBRDeathCause::Fall:
			return NSLOCTEXT("BR", "Death.Teammate.Fall", "{Name} est tomb\u00e9 dans une fosse.");
		case EBRDeathCause::Madness:
			return NSLOCTEXT("BR", "Death.Teammate.Madness", "{Name} a perdu la raison : il est \u00e0 terre.");
		default:
			return NSLOCTEXT("BR", "Death.Teammate.Injury", "{Name} est \u00e0 terre.");
		}
	}
}

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
	float Puddles = 0.f;      // v4.1 : part du sol couverte de flaques (reflets ray traces), 0..1
	float Wetness = 0.f;      // v4.1 : sol mouille (plus sombre, plus brillant), 0..1
	float AntiTile = 0.f;     // v4.3 : sols/plafonds, 2e echantillon tourne melange a grande echelle (casse la repetition)
	float Stains = 0.f;       // v4.3 : taches d'humidite a l'echelle du monde (sols), 0..1
	float WaterLine = -1.f;   // v4.5 : hauteur de l'eau (cm) : bande mouillee juste au-dessus (murs, piliers, rebords) ; < 0 : aucune
	float RoughDetail = 1.f;  // v4.7 : part de la carte de rugosite <Texture>_R appliquee (peinture sur beton : faible)
	float WallVariation = 0.f; // v4.7 : murs, teinte et aureoles d'humidite a l'echelle du monde (casse la repetition)

	FBRSurface() {}
	FBRSurface(FName InTex, const FLinearColor& InTint, float InScale, float InRough = 0.85f, float InGrime = 0.35f)
		: Texture(InTex), Tint(InTint), Scale(InScale), Roughness(InRough), Grime(InGrime) {}

	FString Key() const
	{
		return FString::Printf(TEXT("%s|%.3f,%.3f,%.3f|%.0f|%.2f|%.2f|%.2f|%.2f|%.2f,%.2f,%.2f|%.2f|%.2f|%.2f|%.2f|%.2f|%.2f|%.0f|%.2f|%.2f"), *Texture.ToString(),
			Tint.R, Tint.G, Tint.B, Scale, Roughness, Metallic, Grime, SelfIllum, Emissive.R, Emissive.G, Emissive.B, Caustics, FloorGrime, Puddles, Wetness,
			AntiTile, Stains, WaterLine, RoughDetail, WallVariation);
	}
};

/** Lumiere d'une cellule (v4.7 : ici pour la construction etalee des chunks, qui les met en attente) */
struct FBRLightInfo
{
	bool bHas = false;
	bool bBroken = false;
	bool bFlicker = false;
	bool bShadow = false;
	FVector Offset = FVector::ZeroVector; // decalage dans la cellule (local au centre)
	float Yaw = 0.f;
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

/** v4.8 : note trouvable. Id : identifiant stable, enregistre dans le journal de la sauvegarde (le texte suit la langue) */
struct FBRNote
{
	FName Id;
	FText Text;
};

/** Description complete d'un niveau */
struct FBRLevelDef
{
	int32 Number = 0;
	// v4.8 : textes localises (NSLOCTEXT, cle stable "Level.<n>...")
	FText Title;            // titre du wiki
	FText Nickname;         // surnom
	FText Description;      // description courte
	int32 SurvivalClass = 1;
	FText ClassText;

	// --- Grille ---
	EBRLayout Layout = EBRLayout::Rooms;
	float CellSize = 350.f;
	float WallHeight = 290.f;
	/** Epaisseur des murs (v4.3 : trois fois plus epais qu'avant dans tous les niveaux) */
	float WallThickness = 60.f;
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
	/** Brouillard volumetrique : halos et rayons sous les lampes (desactivable dans les parametres) */
	bool bVolumetricFog = true;
	/** Part de la lumiere des lampes diffusee dans le brouillard volumetrique */
	float VolumetricScatter = 0.6f;
	/** Etalonnage "cinema" : teinte des ombres et des hautes lumieres (gain multiplie par la teinte) */
	FLinearColor ShadowTint = FLinearColor::White;
	FLinearColor HighlightTint = FLinearColor::White;
	EBRSky Sky = EBRSky::None;
	float SunLux = 6.f;
	FLinearColor SunColor = FLinearColor(1.f, 0.97f, 0.92f);
	float SunPitch = -50.f;
	/** v4.11 : intensite de la lumiere du ciel (1 = defaut) ; nuages volumetriques (ciel non uniforme, profils Qualite et
	 *  Cinematique) */
	float SkyLightScale = 1.f;
	bool bClouds = false;
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
	/** v4.8 : notes par identifiant stable (journal des sauvegardes) et texte localise */
	TArray<FBRNote> Notes;
	EBRProps Props = EBRProps::None;
	float PropDensity = 0.1f;
	bool bWater = false;
	float WaterHeight = 45.f;
	FBRSurface Water;
	/** Proprietes optiques de l'eau : absorption (plus elle est basse, plus l'eau est limpide) et voile laiteux
	 *  de l'eau profonde (diffusion) */
	float WaterAbsorption = 1.2f;
	float WaterScattering = 0.15f;
	/** Mouvement de fond de l'eau : houle (deplace la surface) et clapot (vaguelettes qui plient les reflets).
	 *  Les vagues des joueurs et des entites s'y ajoutent (UBRWaterSim) */
	float WaterWaves = 1.f;
	float WaterChop = 1.f;
	/** Bassins profonds (Niveau 37) : proportion de blocs 2x2 creuses et profondeur sous le sol */
	float PoolChance = 0.f;
	float PoolDepth = 260.f;
	/** Grandes verrieres inclinees a la jonction mur / plafond (par chunk) */
	float SkylightChance = 0.f;
	// --- v3.8 : Poolrooms d'apres la scene fournie ---
	/** Trottoirs carreles le long des murs, au-dessus de l'eau (DeckHeight 0 = aucun) : hauteur, largeur, et marche
	 *  immergee le long du bord (DeckStep 0 = aucune) pour remonter du canal sans sauter */
	float DeckHeight = 0.f;
	float DeckWidth = 130.f;
	float DeckStep = 0.f;
	float DeckStepWidth = 35.f;
	/** Proportion des murs bordes d'un trottoir (une porte en a des deux cotes, ou d'aucun) */
	float DeckChance = 0.8f;
	/** Portes en arche (plein cintre) au lieu d'un linteau droit ; sommet de l'arche en cm (0 : 60 cm sous le plafond) */
	bool bArches = false;
	float ArchApex = 0.f;
	/** v4.6 : chambranles (jambages et traverse en saillie, surface Trim) autour des portes a linteau droit */
	bool bDoorCasings = false;
	/** Corniche a 45 degres entre les murs et le plafond (cote en cm, 0 = aucune) */
	float CoveSize = 0.f;
	/** Piliers alignes en grille reguliere dans les grandes salles ouvertes */
	bool bPillarGrid = false;
	// --- v3.8 : Niveau 4 d'apres la scene fournie ---
	/** Zones de bureaux cloisonnes (rangees de petits bureaux et allees) : proportion des zones de 12 x 12 cellules */
	float CubicleZoneChance = 0.f;
	// --- v4.1 : Niveau 1 en parking souterrain ---
	/** Places et fleches peintes au sol, bandes jaunes et noires au pied des piliers, poutres au plafond */
	bool bGarage = false;
	/** Couleur de la bande peinte le long des murs du parking */
	FLinearColor GarageStripe = FLinearColor(0.12f, 0.42f, 0.55f);

	// --- v4.6 : salles de fosses ("Hole Variation" du Niveau 0) ---
	/** Salle carree de PitRoomCells x PitRoomCells cellules, inscrite dans un chunk (une galerie d'au moins une cellule tout
	 *  autour la contourne), percee d'une grille de fosses carrees aux coins interieurs des cellules : les passages passent
	 *  par le centre des cellules, la ou la grille A* fait marcher les entites. 0 : aucune salle.
	 *  Niveau fini : nombre garanti = max(PitRoomsMin, arrondi(PitRoomChance x chunks hors depart)) ; infini : tirage par chunk */
	float PitRoomChance = 0.f;
	int32 PitRoomsMin = 1;
	int32 PitRoomCells = 5;
	/** Cote d'une fosse (cm) et largeur minimale des passages entre deux fosses : le cote est reduit si
	 *  PitHoleSize + PitPassage depasse la taille d'une cellule (les passages gardent au moins 120 cm) */
	float PitHoleSize = 200.f;
	float PitPassage = 150.f;
	/** Part des coins interieurs perces (les autres restent pleins : la grille garde des "trous" manquants) */
	float PitHoleChance = 1.f;
	/** Profondeur du puits jusqu'au fond (cm) ; chute mortelle sous PitKillDepth (centre du joueur) ; epaisseur de la dalle */
	float PitDepth = 1400.f;
	float PitKillDepth = 450.f;
	float PitLipThickness = 30.f;
	/** Ouvertures dans les murs de la salle, par cote */
	int32 PitDoorsPerSide = 1;

	// --- v4.3 : Niveau 0 plus petit ---
	/** > 0 : niveau fini de 2 x BoundsChunks chunks de cote autour du point de depart, ferme par des murs d'enceinte.
	 *  Les cassettes VHS et les sorties y sont tirees parmi les chunks (nombre garanti) au lieu d'un tirage par chunk */
	int32 BoundsChunks = 0;

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
	/** v4.5 : profil graphique : 0 Performance, 1 Qualite, 2 Cinematique, 3 Personnalise (un reglage modifie a la main),
	 *  v4.8 : 4 RTX fluide (Lumen materiel, cache de surfaces, TSR a 67 %, ombres des neons limitees a 25 m) */
	int32 GraphicsProfile = 1;
	/** v4.8 : modeles complets des entites (Hound d'origine, 175 000 sommets, au lieu du derive allege) : reglage effectif,
	 *  donne par le profil (Cinematique) ou choisi a la main (Personnalise) ; s'applique aussi aux entites presentes */
	bool bFullCreatures = false;
	int32 Quality = 3;          // 0 Bas .. 4 Cinematique
	bool bHardwareRT = true;    // Lumen en ray tracing materiel (RTX)
	bool bRTHitLighting = false;
	/** v4.5 : ombres ray tracees pour la lampe torche (les plafonniers gardent les ombres virtuelles) */
	bool bRTShadows = false;
	bool bAreaLights = true;    // neons en lumieres surfaciques
	bool bVolumetricFog = true;
	bool bFilmGrain = true;
	/** Effet camescope / VHS : viseur REC, cadres, lignes de balayage, aberration, grain, salete d'objectif */
	bool bVHSEffect = true;
	/** Volume general (0..1) */
	float MasterVolume = 1.f;
	/** v4.11 : volume des effets et ambiances (entites, mecanismes, pas, interface), 0..1 */
	float EffectsVolume = 1.f;
	/** v4.11 : volume des voix des coequipiers, 0,25..1 (le chat vocal se coupe dans CHAT VOCAL) */
	float VoiceVolume = 1.f;
	/** v4.11 : sous-titres des sons utiles (entites proches, mecanismes, coupures) */
	bool bSubtitles = true;
	/** Chat vocal de proximite : 0 voix ouverte, 1 appuyer pour parler, 2 desactive */
	int32 VoiceMode = 1;
	/** Luminosite : decalage d'exposition (IL) */
	float Brightness = 0.f;
	/** Affichage : 0 plein ecran, 1 plein ecran fenetre, 2 fenetre.
	 *  v4.9 : copie de UGameUserSettings (seule source du mode et de la resolution, GameUserSettings.ini) : n'est plus
	 *  enregistre dans BackroomsPlayer.ini ; une ancienne valeur y est reprise une fois (migration) */
	int32 WindowMode = 1;
	/** Resolution de rendu (%), completee par l'upscaling TSR */
	int32 RenderScale = 100;
	bool bVSync = false;
	/** Images par seconde maximum (0 = illimite) */
	int32 MaxFPS = 0;
	/** Balancement de la camera pendant la marche */
	bool bHeadBob = true;
	/** v4.7 : confort. Tremblements de la camera (coups recus, jumpscares) : 0..1 */
	float CameraShake = 1.f;
	/** v4.7 : flashs (eclairs des jumpscares, image de la mort, neons qui clignotent) : 0 normaux, 1 attenues, 2 aucun */
	int32 Flashes = 0;
	/** v4.7 : flou de mouvement (desactive par defaut) */
	bool bMotionBlur = false;
	// ---- v4.9 : interface d'exploration (aucune jauge de statut hors de l'inventaire, quel que soit le reglage)
	/** Taille de l'interface (0,8 a 1,3 ; 1 = taille de reference en 1080p) */
	float UiScale = 1.f;
	/** Opacite des informations d'exploration (objets rapides, objectifs, reticule, consignes) : 0,4 a 1 */
	float HudOpacity = 1.f;
	/** Reticule : 0 point, 1 seulement sur un objet utilisable, 2 aucun (les consignes d'interaction restent) */
	int32 CrosshairMode = 0;
	/** Objets rapides : 0 brievement apres un changement, 1 toujours, 2 masques (toujours dans l'inventaire) */
	int32 QuickBarMode = 0;
	/** Objectifs : 0 brievement (arrivee, progres), 1 toujours, 2 masques (toujours dans l'inventaire, onglet Personnage) */
	int32 ObjectivesMode = 0;
	/** v4.9 : creatures dans les reflets ray traces sans hit lighting : 0 traces d'ecran seules (une creature hors champ
	 *  disparait du reflet), 1 relance en hit lighting des seuls impacts sans cache de surfaces (maillages a squelette),
	 *  si le moteur le permet (variable verifiee a l'execution) */
	int32 CreatureReflections = 1;
	/** Facteur des flashs et clignotements (1, 0,35 ou 0) ; les mecaniques ne changent pas */
	float FlashScale() const { return Flashes <= 0 ? 1.f : (Flashes == 1 ? 0.35f : 0.f); }
	/** v4.4 : mode developpeur (actif par defaut hors version finale) : tous les niveaux jouables depuis le choix des
	 *  niveaux, raccourcis en jeu (changer de niveau, voler a travers les murs, invincible, jumpscares...) */
	bool bDevMode = UE_BUILD_SHIPPING == 0;

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
