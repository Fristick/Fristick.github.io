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
		TArray<FTransform> Transforms;
	};

	/** Boite (cube moteur) texturee par projection monde */
	void AddBox(const FBRSurface& S, const FVector& Center, const FVector& Size, bool bCollision = true, float Yaw = 0.f);
	/** Modele d'architecture (arche, corniche) habille d'une surface du niveau ; false s'il n'est pas importe */
	bool AddSurfaceMesh(FName MeshName, const FBRSurface& S, const FTransform& T, bool bCollision);
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
	/** Parking souterrain (Niveau 1) : poutres du plafond, marquages au sol, bandes au pied des piliers */
	void BuildGarage();
	/** Trait de peinture au sol (marquage du parking), legerement au-dessus du beton */
	void AddFloorPaint(const FVector& Center, float Length, float Width, float Yaw, bool bYellow);
	/** Sol cellule par cellule avec bassins profonds (Niveau 37) */
	void BuildPools();
	/** Trottoirs carreles le long des murs, marche immergee, estrade du point de depart (Niveau 37) */
	void BuildDecks();
	/** Poste de travail du Niveau 4 dos a un mur : bureau (ecran, tour, clavier), chaise, fontaine a eau.
	 *  ToWall : vers le mur derriere le bureau ; Back : le long du mur, cote fontaine */
	void AddWorkstation(int32 X, int32 Y, const FVector& WallFace, const FVector& ToWall, const FVector& Back, bool bCooler);
	/** Verriere inclinee sur un mur + lumiere du jour qui inonde la piece (Niveau 37) */
	void BuildSkylight();
	/** Placards et trous dans le mur ou se cacher (Niveau 0) */
	void BuildHidingSpots();
	/** v4.6 : sol du chunk perce par la salle de fosses (dalle epaisse decoupee en rectangles autour des ouvertures),
	 *  parois des puits par bandes de plus en plus sombres, fond qui arrete la chute */
	void BuildPitRoom(const FIntRect& Room);
	/** v4.6 : cellule libre tiree dans le chunk (ni cachette, ni bassin, ni salle de fosses ; niveau fini : atteignable
	 *  depuis le depart sans passer par une salle de fosses). Essais tires, puis balayage de tout le chunk dans un ordre tire */
	bool PickFreeCell(int32 Salt, FIntPoint& Out, bool bFullScan) const;
	/** v4.6 : la salle de fosses de ce chunk (bHasPitRoom) */
	bool bHasPitRoom = false;
	FIntRect PitRoom;
	void BuildPickupsAndExits();
	/** Sortie decidee avant la construction du plafond (une echelle le perce d'une trappe) */
	struct FPlannedExit
	{
		FVector Pos = FVector::ZeroVector;
		float Yaw = 0.f;
		int32 Target = 0;
		EBRExitStyle Style = EBRExitStyle::Door;
		/** Echelle : hauteur du conduit au-dessus de la trappe (0 : pas de trappe) */
		float Shaft = 0.f;
	};
	/** Choisit les sorties de ce chunk (tirage par chunk, ou nombre garanti dans un niveau fini) */
	void PlanExits();
	/** Plafond du chunk, perce d'une trappe au-dessus d'une echelle, et le conduit sombre qui monte au-dessus */
	void BuildCeiling();
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

	TArray<FPlannedExit> PlannedExits;
	/** Cellules devant une sortie : on n'y pose pas d'accessoires */
	TSet<FIntPoint> ExitCells;
	/** Trappe au plafond (une par chunk au plus) : rectangle XY et hauteur du conduit */
	bool bShaftHole = false;
	FVector2D ShaftMin = FVector2D::ZeroVector;
	FVector2D ShaftMax = FVector2D::ZeroVector;
	float ShaftHeight = 0.f;

	float Power = 1.f;

	TMap<FString, FBatch> Batches;
	TWeakObjectPtr<ABRWorld> World;
	int32 LightCount = 0;
};
