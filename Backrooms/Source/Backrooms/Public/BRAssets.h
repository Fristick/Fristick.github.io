// Chargement des ressources importees (/Game/Backrooms/...) avec repli sur les formes du moteur.
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
class UMeshComponent;

UCLASS()
class BACKROOMS_API UBRAssets : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBRAssets* Get(const UObject* WorldContext);

	/** /Game/Backrooms/Meshes/<Name> (nullptr si absent) */
	UStaticMesh* Mesh(FName Name);
	UStaticMesh* Cube();
	UStaticMesh* Sphere();
	UStaticMesh* Cylinder();
	UStaticMesh* Plane();

	UTexture* Texture(FName Name);
	USoundBase* Sound(FName Name);
	USoundAttenuation* Attenuation(float FalloffDistance);

	/** Materiau "projete dans l'espace monde" (murs, sols...) - mis en cache */
	UMaterialInterface* Surface(const FBRSurface& S);
	/** Eau (translucide) */
	UMaterialInterface* WaterMaterial(const FBRSurface& S);
	/** Materiau d'un slot de modele Blender, d'apres son nom (Glow, Metal, Skin...) */
	UMaterialInterface* SlotMaterial(const FString& SlotName, const FLinearColor* TintOverride = nullptr);
	/** Nouveau materiau emissif unique (pour les lumieres qui clignotent, les yeux...) */
	UMaterialInstanceDynamic* NewGlow(UObject* Outer, const FLinearColor& Color, float Strength);

	/**
	 * Applique a chaque slot du composant le materiau correspondant a son nom.
	 * TintOverrides : remplace la couleur de certains slots (ex: "Skin").
	 * bUniqueGlow : les slots "Glow*" recoivent un materiau unique, retourne dans OutGlow.
	 */
	void ApplySlots(UMeshComponent* Comp, const TMap<FString, FLinearColor>* TintOverrides = nullptr,
		bool bUniqueGlow = false, TArray<UMaterialInstanceDynamic*>* OutGlow = nullptr, float GlowScale = 1.f);

	/** Couleur emissive (deja multipliee) d'un slot Glow*, ou noir */
	static FLinearColor GlowColorForSlot(const FString& SlotName);

	/** true si le script d'import a ete execute (materiaux maitres presents) */
	bool HasContent();

private:
	UObject* LoadAsset(const TCHAR* Folder, FName Name, UClass* Class);
	UMaterialInterface* WorldParent();
	UMaterialInterface* MeshParent();
	UMaterialInterface* WaterParent();
	UMaterialInterface* FallbackParent();
	static FLinearColor TextureAverage(FName Texture);

	UPROPERTY()
	TMap<FName, TObjectPtr<UObject>> Loaded;

	TSet<FName> Missing;

	UPROPERTY()
	TMap<FString, TObjectPtr<UMaterialInterface>> MatCache;

	UPROPERTY()
	TMap<int32, TObjectPtr<USoundAttenuation>> AttCache;

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
