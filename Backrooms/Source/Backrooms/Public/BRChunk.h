// Un "chunk" : un carre de NxN cellules dont la geometrie est construite en instances.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BRTypes.h"
#include "BRChunk.generated.h"

class ABRWorld;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ULocalLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UAudioComponent;
struct FBRLightInfo;

/** Lampe animee par le chunk : clignotement, et virage au rouge pres de l'entite du Niveau 0 */
USTRUCT()
struct FBRFlicker
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ULocalLightComponent> Light = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Glow;

	FLinearColor GlowColor = FLinearColor::White;
	FLinearColor LightColor = FLinearColor::White;
	float BaseIntensity = 1000.f;
	float Timer = 0.f;
	float Phase = 0.f;
	bool bOn = true;
	/** false : lampe stable (individuelle seulement pour pouvoir rougir) */
	bool bFlickers = true;
	float Red = 0.f;
	float AppliedMod = -1.f;
	float AppliedRed = -1.f;
};

UCLASS()
class BACKROOMS_API ABRChunk : public AActor
{
	GENERATED_BODY()

public:
	ABRChunk();

	void Build(ABRWorld* InWorld, const FIntPoint& InCoord);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Alimentation electrique 0..1 (coupures de courant) */
	void SetPower(float InPower);
	/** Reapplique le materiau de l'eau (changement du reglage "Rendu de l'eau") */
	void RefreshWater();
	/** Cachettes de ce chunk */
	bool IsInHidingSpot(const FVector& Location, bool bCrouched) const;
	bool FindHidingSpotNear(const FVector& Location, float Radius, bool& bOutNeedsCrouch) const;

	FIntPoint Coord;

protected:
	/** Lot d'instances (une meme maille + un meme materiau) en attente de creation */
	struct FBatch
	{
		UStaticMesh* Mesh = nullptr;
		UMaterialInterface* Material = nullptr; // nullptr = materiaux par nom de slot
		bool bCollision = true;
		bool bShadow = true;
		bool bHidden = false;
		float GlowScale = 1.f;
		float CullDistance = 0.f;
		bool bPowered = false;   // emissif eteint pendant les coupures
		int8 Water = -1;         // 0 = eau du niveau, 1 = flaque calme (materiau change par RefreshWater)
		TArray<FTransform> Transforms;
	};

	/** Boite (cube moteur) texturee par projection monde */
	void AddBox(const FBRSurface& S, const FVector& Center, const FVector& Size, bool bCollision = true, float Yaw = 0.f);
	/** Modele Blender (ou boite de repli) */
	void AddProp(FName MeshName, const FTransform& T, bool bCollision, const FVector& FallbackSize, const FBRSurface* FallbackSurface = nullptr,
		float CullDistance = 0.f, bool bShadow = true);
	void AddLight(int32 X, int32 Y, const FBRLightInfo& L);
	/** Segment de mur sur une ligne de la grille (A..B le long de la ligne, ZLo..ZHi en hauteur) */
	void AddWallSegment(bool bAlongY, float Fixed, float A, float B, float ZLo, float ZHi, bool bWithTrim, bool bWithPipes);
	void AddDoorway(bool bAlongY, float Fixed, float Mid);
	void AddFaceProp(int32 X, int32 Y, const FIntPoint& Dir, FName Mesh, float Along, float Z, const FVector& FallbackSize, bool bCollision);
	/** Plan d'eau du niveau, ou flaque calme (bCalm : presque pas de vagues) */
	void AddWaterPlane(const FVector& Center, const FVector2D& Size, bool bCalm = false);
	/** Prises electriques, grilles d'aeration le long d'un mur */
	void AddWallDetails(bool bAlongY, float Fixed, float A, float B);
	void BuildCellProps(int32 X, int32 Y);
	/** Sol cellule par cellule avec bassins profonds (Niveau 37) */
	void BuildPools();
	/** Verriere inclinee sur un mur + lumiere du jour qui inonde la piece (Niveau 37) */
	void BuildSkylight();
	/** Placards et trous dans le mur ou se cacher (Niveau 0) */
	void BuildHidingSpots();
	void BuildPickupsAndExits();
	void FinishBatches();

	FBatch& GetBatch(const FString& Key, UStaticMesh* Mesh, UMaterialInterface* Mat, bool bCollision, bool bShadow, float Cull);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Instances;

	UPROPERTY()
	TArray<TObjectPtr<UActorComponent>> Extra;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Spawned;

	UPROPERTY()
	TArray<FBRFlicker> Flickers;

	/** Lumieres coupees pendant les coupures de courant (et leur intensite nominale) */
	UPROPERTY()
	TArray<TObjectPtr<ULocalLightComponent>> PoweredLights;

	TArray<float> PoweredBase;

	struct FHidingSpot
	{
		FBox Box;
		bool bCrouch = false;
	};
	TArray<FHidingSpot> HidingSpots;
	TSet<FIntPoint> HidingCells;

	/** Instances d'eau (et si c'est une flaque calme) */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> WaterISMs;
	TArray<bool> WaterCalm;
	float Power = 1.f;

	TMap<FString, FBatch> Batches;
	TWeakObjectPtr<ABRWorld> World;
	int32 LightCount = 0;
};
