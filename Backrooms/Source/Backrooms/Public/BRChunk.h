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
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UAudioComponent;
struct FBRLightInfo;

/** Lumiere qui clignote (animee par le chunk) */
USTRUCT()
struct FBRFlicker
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Glow;

	FLinearColor GlowColor = FLinearColor::White;
	float BaseIntensity = 1000.f;
	float Timer = 0.f;
	float Phase = 0.f;
	bool bOn = true;
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
	void AddWaterPlane(const FVector& Center, const FVector2D& Size);
	void BuildCellProps(int32 X, int32 Y);
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

	TMap<FString, FBatch> Batches;
	TWeakObjectPtr<ABRWorld> World;
	int32 LightCount = 0;
};
