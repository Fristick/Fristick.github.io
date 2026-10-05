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
	UStaticMesh* Cube();
	UStaticMesh* Sphere();
	UStaticMesh* Cylinder();
	UStaticMesh* Plane();

	/** /Game/Backrooms/Textures/<Name>, sinon RawAssets/Textures/<Name>.jpg|png (editeur) */
	UTexture* Texture(FName Name);
	/** Icone d'inventaire : /Game/Backrooms/UI/<Name>, sinon RawAssets/Icons/<Name>.png */
	UTexture* Icon(FName Name);
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
	 */
	void ApplySlots(UMeshComponent* Comp, const TMap<FString, FLinearColor>* TintOverrides = nullptr,
		bool bUniqueGlow = false, TArray<UMaterialInstanceDynamic*>* OutGlow = nullptr, float GlowScale = 1.f, bool bPowered = false);

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
	/** v4.7 : post-traitement des salles de fosses (assombrit ce qui est sous le sol) ; nullptr si indisponible */
	UMaterialInstanceDynamic* NewPitShade(UObject* Outer);
	static const TArray<FString>& GetFallbacks();

private:
	/** v4.7 : carte de rugosite <Texture>_R (relative a Roughness) si elle existe et si le materiau parent l'attend en
	 *  lineaire ; sinon RoughContrast = 0 (rugosite v4.6) */
	void ApplyRoughMap(UMaterialInstanceDynamic* MID, FName BaseTexture, float Detail);

	enum class EParent : uint8 { World, Mesh, Skin, WaterSurface, PitShade, Count };

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
