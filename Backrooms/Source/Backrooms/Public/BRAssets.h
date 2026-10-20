// Chargement des ressources importees (/Game/Backrooms/...) avec deux niveaux de secours :
//  1. dans l'editeur, si l'import Python n'a pas ete fait : textures lues directement dans RawAssets/
//     et materiaux maitres construits en C++ (BRMaterialBuilder) -> le jeu reste texture ;
//  2. sinon, formes et materiau de base du moteur (couleurs unies).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BRTypes.h"
#include "BRAssets.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;
class USoundAttenuation;
class UTexture;
class UTexture2D;
class UMeshComponent;
enum class EBRMasterMaterial : uint8;

UCLASS()
class BACKROOMS_API UBRAssets : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBRAssets* Get(const UObject* WorldContext);

	/** /Game/Backrooms/Meshes/<Name> (nullptr si absent) */
	UStaticMesh* Mesh(FName Name);
	/** v4.4 : maillage a squelette (combinaison du joueur SK_Hazmat), nullptr s'il n'est pas importe */
	class USkeletalMesh* SkeletalMesh(FName Name);
	/** v4.14 : module du kit Blender du Level 0, /Game/Backrooms/Level0Kit/Meshes/<Name> (backrooms_setup.import_level0_kit) */
	UStaticMesh* KitMesh(FName Name);
	/** v4.14 : le generateur pose-t-il les modules du kit ? Il faut le kit voulu (-BRNoLevel0Kit ou BRLevel0Kit 0 l'ecartent)
	 *  et complet : chaque module requis importe, avec ses collisions UCX quand il en faut, et uniquement des materiaux du
	 *  kit. Verifie une fois ; la raison d'un refus est dans le journal et dans GetLevel0KitStatus */
	bool UseLevel0Kit();
	const FString& GetLevel0KitStatus();
	static void SetLevel0KitWanted(bool bOn);
	static bool IsLevel0KitWanted();
	/** v4.14 : axe Y des modules non retourne par l'import (lu sur SM_L0_Wall_Corner, voir UseLevel0Kit) */
	bool IsKitMirrorY() const { return bKitMirrorY; }
	/** v4.15 : module de sol (SM_L0_Floor_350) ou de plafond (SM_L0_Ceiling_350) du kit, s'il est importe et a la bonne
	 *  taille (350 x 350 cm centre sur la cellule, dessus du sol a 0, dessous du plafond a 290 cm) ; verifie une fois.
	 *  nullptr : le chunk garde sa grande dalle */
	UStaticMesh* KitCellSlab(bool bCeiling);
	UStaticMesh* Cube();
	UStaticMesh* Sphere();
	UStaticMesh* Cylinder();
	UStaticMesh* Plane();

	/** /Game/Backrooms/Textures/<Name>, sinon RawAssets/Textures/<Name>.jpg|png (editeur) */
	UTexture* Texture(FName Name);
	/** Icone d'inventaire : /Game/Backrooms/UI/<Name>, sinon RawAssets/Icons/<Name>.png */
	UTexture* Icon(FName Name);
	/** v4.15 : profil photometrique IES /Game/Backrooms/Lights/<Name> (faisceau de la lampe), nullptr s'il n'est pas importe */
	class UTextureLightProfile* LightProfile(FName Name);
	USoundBase* Sound(FName Name);
	/** Attenuation spatialisee. v4.7 : bOcclude : etouffee derriere les murs (entites, bruits du monde) ; les sorties
	 *  et les objectifs passent false (signal fiable) ; de loin, le son perd ses aigus et gagne de la reverberation */
	USoundAttenuation* Attenuation(float FalloffDistance, bool bOcclude = true);
	/** Voix des coequipiers : 3D, portee ~26 m, etouffee derriere les murs */
	USoundAttenuation* VoiceAttenuation();

	/** Materiau "projete dans l'espace monde" (murs, sols...) - mis en cache */
	UMaterialInterface* Surface(const FBRSurface& S);
	/** Meme materiau, mais unique (non partage) : ses parametres peuvent etre animes (mur qui glitche...) */
	UMaterialInstanceDynamic* NewSurface(const FBRSurface& S, UObject* Outer);
	/** Eau translucide : refraction, absorption selon la profondeur, houle (Waves) et clapot (Chop) de fond */
	UMaterialInterface* WaterMaterial(const FBRSurface& S, float Absorption = 1.2f, float Scattering = 0.15f, float Waves = 1.f, float Chop = 1.f);
	/** Vagues simulees autour du joueur (UBRWaterSim) : donnees a l'eau et aux caustiques du carrelage */
	void SetWaterSim(UTexture* SimTexture, const FLinearColor& Window);
	/** Materiau d'un slot de modele Blender, d'apres son nom (Glow, Metal, Skin...) */
	UMaterialInterface* SlotMaterial(const FString& SlotName, const FLinearColor* TintOverride = nullptr);
	/** Nouveau materiau emissif unique (pour les lumieres qui clignotent, les yeux...) */
	UMaterialInstanceDynamic* NewGlow(UObject* Outer, const FLinearColor& Color, float Strength);

	/**
	 * Applique a chaque slot du composant le materiau correspondant a son nom.
	 * TintOverrides : remplace la couleur de certains slots (ex: "Skin").
	 * bUniqueGlow : les slots "Glow*" recoivent un materiau unique, retourne dans OutGlow.
	 * bPowered : les slots "Glow" (neons) s'eteignent pendant les coupures de courant (SetGlowScale).
	 * LightTemperatureK : v4.15 : > 0, ces slots alimentes prennent la couleur d'une lampe de cette temperature
	 * (TemperatureGlow), comme les diffuseurs individuels et la lumiere qu'ils projettent.
	 */
	void ApplySlots(UMeshComponent* Comp, const TMap<FString, FLinearColor>* TintOverrides = nullptr,
		bool bUniqueGlow = false, TArray<UMaterialInstanceDynamic*>* OutGlow = nullptr, float GlowScale = 1.f, bool bPowered = false,
		float LightTemperatureK = 0.f);

	/**
	 * v4.8 : affectation explicite des slots d'un modele (combinaison du joueur) : nom de slot -> style (SlotStyles).
	 * Le nom est compare exactement, puis par inclusion (suffixe ajoute par l'import) ; jamais par indice. Un slot
	 * inattendu recoit le materiau d'erreur (magenta hors Shipping) et est signale (GetMaterialProblems).
	 */
	void ApplySlotMap(UMeshComponent* Comp, const TArray<TPair<FString, FString>>& SlotStylesByName, const FString& Label);
	/** v4.8 : une ligne par section (LOD, section, slot, materiau final, parent, texture, usage, secours) ; les sections
	 *  sans usage "squelette" ou en materiau de secours sont aussi signalees comme problemes */
	TArray<FString> DescribeSections(UMeshComponent* Comp, const FString& Label);
	/** v4.8 : materiau visible des erreurs d'affectation (magenta en developpement, gris neutre en Shipping) */
	UMaterialInterface* ErrorMaterial();
	/** v4.8 : problemes de materiaux constates (usage absent, slot inattendu, secours) : journal, mode developpeur, tests */
	static void ReportMaterialProblem(const FString& Problem);
	static const TArray<FString>& GetMaterialProblems();

	/** Intensite des neons alimentes par le secteur (0 = coupure de courant) */
	void SetGlowScale(float Scale);

	/** Couleur emissive (deja multipliee) d'un slot Glow*, ou noir */
	static FLinearColor GlowColorForSlot(const FString& SlotName);
	/** v4.15 : couleur emissive d'un diffuseur eclaire par une lampe de Kelvin K : teinte de la temperature (celle de la
	 *  lumiere, FLinearColor::MakeFromColorTemperature), meme luminance que Glow (meme eclat a l'ecran) */
	static FLinearColor TemperatureGlow(const FLinearColor& Glow, float Kelvin);

	/** true si des materiaux texturees sont disponibles (import Python ou construction a la volee) */
	bool HasContent();
	/** true si le jeu tourne sur les ressources de secours (import Python absent ou incomplet) */
	bool IsUsingRuntimeContent();
	/**
	 * Verifie la taille des modeles importes (un importeur qui ignore l'echelle du fichier les rend 100 fois trop
	 * petits : invisibles). Retourne un message d'erreur, vide si tout va bien.
	 */
	FString CheckImportedMeshes();

	/** v4.5 : un modele fourni (ou son derive) manque et une forme de secours le remplace. Signale une fois dans le
	 *  journal (avertissement) et liste en mode developpeur : un modele fourni n'est jamais remplace en silence. */
	static void ReportFallback(const FString& Model);
	/** v4.8 : prechargement asynchrone lance des le menu (plus de chargement synchrone au premier besoin). v4.9 : par
	 *  ensembles (commun au menu, puis niveau et voisins : PreloadForLevel) au lieu de tout /Game/Backrooms ; -BRPreloadAll
	 *  pour l'ancien comportement. */
	static void StartPreload();
	/** v4.11 : volume des effets et ambiances (multiplie le volume d'origine de chaque son charge par UBRAssets::Sound) */
	static void SetEffectsGain(float Gain);
	/** v4.9 : ensembles du niveau Level (textures de ses surfaces, creatures qu'il peut faire apparaitre) et des niveaux
	 *  ou menent ses sorties ; les ensembles devenus inutiles sont relaches. L'ensemble commun reste. */
	static void PreloadForLevel(int32 Level);
	/** v4.9 : ensembles en memoire, nombre de ressources, duree et memoire de chaque chargement (rapport du test) */
	static TArray<FString> PreloadReport();
	/** v4.9 (tests) : noms des ensembles en memoire ("common", "entity:Hound", "tex:T_...") et nombre d'ensembles au catalogue */
	static TArray<FString> PreloadedSetNames();
	static int32 PreloadCatalogCount();
	/** Prechargement termine (ou impossible) */
	static bool IsPreloadDone();
	/** v4.10 : cles des ensembles. Seule convention : le catalogue, les niveaux et les tests les construisent ici
	 *  ("entity:Smiler", "tex:T_PoolTile", "props:Office", "fixture:StreetLamp") */
	static FString EntitySetKey(EBREntityKind Kind);
	static FString SurfaceSetKey(FName Texture);
	/** v4.10 : ensembles indispensables au niveau Level (commun, ses surfaces, ses creatures, ses decors propres) et
	 *  anticipation facultative (niveaux ou menent ses sorties) */
	static TArray<FString> EssentialSetsForLevel(int32 Level);
	static TArray<FString> NeighborSetsForLevel(int32 Level);
	/** v4.10 : ensemble auquel appartient une ressource (nom sans chemin), d'apres ses usages ("common" par defaut) */
	static FString SetKeyForAsset(const FString& AssetName, bool bTexture);
	/** v4.10 : ressources d'un ensemble au catalogue (tests : un ensemble vide n'est pas une reussite) */
	static int32 CatalogSetSize(const FString& Key);
	/** v4.10 : ressources de cet ensemble encore en memoire (objets retenus, et pas seulement handles) */
	static int32 ResidentCount(const FString& Key);
	/** v4.10 : prepare les ensembles indispensables du niveau Level en arriere-plan, avant leur premiere utilisation.
	 *  Serial identifie la demande : une demande remplacee n'est jamais consideree comme prete. */
	static void PrepareLevel(int32 Level, uint32 Serial);
	/** v4.10 : la demande Serial est prete (ressources en memoire). OutProgress : 0..1 ; OutMissing : ensembles encore en
	 *  cours. Faux pour une demande remplacee. */
	static bool IsLevelPrepared(uint32 Serial, float* OutProgress = nullptr, TArray<FString>* OutMissing = nullptr);
	/** v4.10 : retire des caches (Loaded, MatCache) les entrees des ensembles inutiles au niveau Level. Les objets encore
	 *  utilises par un acteur restent en memoire tant qu'ils le sont ; les autres partent au ramasse-miettes suivant. */
	void TrimCaches(int32 Level);
	/** v4.10 : mesures : entrees des caches (ressources, materiaux) */
	int32 GetLoadedCount() const { return Loaded.Num(); }
	int32 GetMaterialCacheCount() const { return MatCache.Num(); }
	/** v4.8 : chargements synchrones survenus en jeu (nombre, pire duree, premiers noms) : rapport et mode developpeur */
	static int32 SyncLoadsInGame;
	static float MaxSyncLoadMs;
	static TArray<FString> SyncLoadNames;
	/** Compter les chargements synchrones (en jeu, hors menus et transitions) */
	static bool bCountSyncLoads;
	/** v4.10 : chargements synchrones pendant la construction d'un niveau (premieres utilisations non preparees) */
	static bool bCountBuildLoads;
	static int32 SyncLoadsInBuild;
	static TArray<FString> SyncLoadBuildNames;
	/** v4.10 (tests) : preparation volontairement bloquee jusqu'a cette heure (FPlatformTime::Seconds) : ecran de
	 *  preparation, retour au menu propose, delai maximal */
	static double TestPrepareStallUntil;
	/** v4.8 : textures lues dans RawAssets (secours de l'editeur ; jamais dans un jeu empaquete) */
	static int32 RawTextureLoads;
	/** v4.7 : post-traitement des salles de fosses (assombrit ce qui est sous le sol) ; nullptr si indisponible */
	UMaterialInstanceDynamic* NewPitShade(UObject* Outer);
	/** v4.15 : decalque d'usure (MD_BR_Decal, case Kind de l'atlas T_Decal_Atlas), partage et mis en cache ; nullptr sans
	 *  materiau maitre (jeu cuisine sans MD_BR_Decal) : le chunk ne pose alors aucun decalque */
	UMaterialInterface* DecalMaterial(EBRDecal Kind);
	static const TArray<FString>& GetFallbacks();

private:
	/** v4.7 : carte de rugosite <Texture>_R (relative a Roughness) si elle existe et si le materiau parent l'attend en
	 *  lineaire ; sinon RoughContrast = 0 (rugosite v4.6) */
	void ApplyRoughMap(UMaterialInstanceDynamic* MID, FName BaseTexture, float Detail);

	/** v4.15 : WorldCloth : variante "tissu" du monde (moquettes, FBRSurface::Sheen > 0) ; Decal : decalques d'usure */
	/** v4.17 : Gear : equipement du joueur (M_BR_Gear) */
	enum class EParent : uint8 { World, Mesh, Skin, WaterSurface, PitShade, WorldCloth, Decal, Gear, Count };

	UObject* LoadAsset(const TCHAR* Folder, FName Name, UClass* Class);
	UTexture2D* LoadRawTexture(const TCHAR* SubFolder, FName Name, bool bLinear, bool bMips);
	UMaterialInterface* Parent(EParent Which);
	/** v4.8 : le materiau maitre declare-t-il ses usages ? Editeur : ajoutes en memoire ; sinon signale. */
	bool EnsureUsages(UMaterialInterface* M, EBRMasterMaterial Kind, const TCHAR* AssetName);
	UMaterialInterface* FallbackParent();
	/** Nouveau materiau de surface (parametres remplis), sans passer par le cache */
	UMaterialInstanceDynamic* CreateSurface(const FBRSurface& S, UObject* Outer);
	static FLinearColor TextureAverage(FName Texture);
	static bool IsSkinSlot(const FString& SlotName);

	UPROPERTY()
	TMap<FName, TObjectPtr<UObject>> Loaded;

	TSet<FName> Missing;
	/** v4.14 : etat du kit du Level 0 (-1 : pas encore verifie, 0 : incomplet, 1 : complet) et sa description */
	int32 KitState = -1;
	FString KitStatus;
	/** v4.14 : sens de l'axe Y des modules importes, lu sur la boite de SM_L0_Wall_Corner (plinthes du cote +Y d'Unreal
	 *  quand l'import retourne Y, comme prevu ; du cote -Y sinon). Sert a orienter les poteaux d'angle et en T */
	bool bKitMirrorY = false;
	/** v4.15 : etat des modules de sol et de plafond du kit (-1 : pas encore verifie, 0 : refuse, 1 : utilisable) */
	int32 KitSlabState[2] = { -1, -1 };

	UPROPERTY()
	TMap<FString, TObjectPtr<UMaterialInterface>> MatCache;

	UPROPERTY()
	TMap<int32, TObjectPtr<USoundAttenuation>> AttCache;

	UPROPERTY()
	TObjectPtr<USoundAttenuation> VoiceAtt;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Parents;

	TArray<bool> ParentResolved;
	TArray<bool> ParentCustom;
	bool bRuntimeContent = false;

	struct FPoweredGlow
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> MID;
		FLinearColor Base = FLinearColor::Black;
	};
	TArray<FPoweredGlow> PoweredGlows;
	float GlowScaleNow = 1.f;

	/** Materiaux qui lisent la simulation de l'eau (eau, carrelage a caustiques) */
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> WaterSimMIDs;
	TWeakObjectPtr<UTexture> WaterSimTexture;
	FLinearColor WaterSimWindow = FLinearColor(0.f, 0.f, 1000.f, 0.f);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMat;
};
