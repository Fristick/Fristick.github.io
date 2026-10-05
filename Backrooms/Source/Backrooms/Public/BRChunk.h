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

	/** Construction complete, tout de suite (chargement d'un niveau, point de depart, tests) */
	void Build(ABRWorld* InWorld, const FIntPoint& InCoord);

	/** v4.7 : construction etalee sur plusieurs images. StepBuild cree les composants dans l'ordre : collisions (sol,
	 *  murs), visuels, lumieres, objets (ramassables, sorties), jusqu'a epuiser BudgetMs. true quand le chunk est pret.
	 *  Chaque appel fait au moins un pas : la construction avance toujours.
	 *  v4.8 : BeginBuild ne fait plus qu'initialiser : la planification (grille, lots d'instances, lumieres a creer) est
	 *  elle aussi faite par StepBuild, colonne par colonne de cellules, dans le meme budget. Les lots d'instances sont
	 *  crees par sous-lots (curseur), et les objets un par un. */
	void BeginBuild(ABRWorld* InWorld, const FIntPoint& InCoord);
	/** bStopAtCollision : s'arrete des que le sol et les murs existent (construction urgente sous un joueur) */
	bool StepBuild(double BudgetMs, bool bStopAtCollision = false);
	/** v4.8 : la planification est terminee (lots et lumieres connus) */
	bool IsPlanned() const { return bPlanned; }

	/** v4.8 : demontage etale : objets, puis composants, quelques-uns par image dans BudgetMs ; true quand il ne reste
	 *  plus rien (le chunk peut etre detruit sans saccade). Un chunk en demontage n'est plus construit. */
	bool StepTeardown(double BudgetMs);
	bool IsTearingDown() const { return bTearingDown; }
	/** Le demontage a commence a detruire des composants (le chunk ne peut plus etre repris tel quel) */
	bool HasTeardownStarted() const { return TeardownStep > 0; }
	void BeginTeardown() { bTearingDown = true; SetActorTickEnabled(false); }
	/** Reprise d'un chunk en attente de demontage (le joueur est revenu avant qu'il ne commence) */
	void CancelTeardown() { bTearingDown = false; SetActorTickEnabled(Flickers.Num() > 0); }

	/** v4.8 : le type des lumieres (neons en lumieres surfaciques ou ponctuelles) suit le reglage courant : recree les
	 *  lumieres dont le type a change. Retourne le nombre de lumieres recreees.
	 *  v4.9 : au plus BudgetMs (ms) par appel, au moins une lumiere : a rappeler tant que LightTypesMatch est faux */
	int32 RefreshLightTypes(bool bArea, double BudgetMs = 1.0e9);
	/** Toutes les lumieres de ce chunk sont-elles deja du type demande ? */
	bool LightTypesMatch(bool bArea) const;
	/** v4.8 : ombres des lumieres selon la distance aux joueurs locaux (au-dela de ShadowDistance, plus d'ombre ; marge de
	 *  10 % pour ne pas basculer sans cesse). Retourne le nombre de lumieres qui projettent une ombre. */
	int32 UpdateShadowLOD(const TArray<FVector>& Viewers, float ShadowDistance);
	/** Tous les composants et objets sont crees */
	bool IsReady() const { return bReady; }
	/** Sol et murs (collisions) crees : on peut marcher dessus */
	bool HasCollision() const { return bCollisionReady; }

	/** v4.7 : temps passe dans chaque etape (ms) et nombre de pas */
	struct FBuildStats
	{
		float PlanMs = 0.f;
		float CollisionMs = 0.f;
		float VisualMs = 0.f;
		float LightMs = 0.f;
		float ActorMs = 0.f;
		int32 Steps = 0;
	};
	const FBuildStats& GetBuildStats() const { return Stats; }

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
		/** v4.8 : hors des structures ray tracees (eau translucide : noire dans les reflets d'autres surfaces) */
		bool bNoRayTracing = false;
		TArray<FTransform> Transforms;
		/** v4.8 : instances deja creees (sous-lots) */
		int32 Cursor = 0;
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
	/** v4.7 : construction etalee */
	struct FPendingLight
	{
		int32 X = 0;
		int32 Y = 0;
		FBRLightInfo L;
	};
	TArray<FPendingLight> PendingLights;
	bool bDeferLights = false;
	int32 BuildPhase = 0;
	bool bReady = false;
	bool bCollisionReady = false;
	FBuildStats Stats;
	/** v4.8 : cree un sous-lot du lot Key (au plus MaxInstances instances, dans un composant a lui) et avance son curseur ;
	 *  le lot est retire de Batches quand tout est cree. Retourne le nombre d'instances creees. */
	int32 CreateBatchStep(const FString& Key, int32 MaxInstances);
	/** Taille du prochain sous-lot d'apres le cout moyen mesure par instance et le budget restant */
	static int32 SubBatchSize(bool bCollision, double RemainingMs);
	/** v4.8 : cout moyen mesure d'une instance (ms), avec ou sans collision, partage par tous les chunks */
	static float CostPerInstanceMs[2];

	/** v4.8 : planification etalee : une unite (une colonne de cellules, ou une etape) ; true quand tout est planifie */
	bool StepPlan();
	int32 PlanStage = 0;
	int32 PlanCursor = 0;
	bool bPlanned = false;
	/** v4.8 : objets et sorties crees un par un : indice du prochain tirage d'objet, puis de la prochaine sortie */
	int32 ActorCursor = 0;
	/** Un tirage d'objet (Index dans la table des tirages) ; false si l'indice depasse la table */
	bool SpawnPickupRoll(int32 Index);
	static int32 NumPickupRolls();
	void SpawnPlannedExit(int32 Index);
	/** v4.8 : demontage */
	bool bTearingDown = false;
	int32 TeardownStep = 0;

	/** v4.8 : chaque lumiere creee, pour pouvoir la recreer d'un autre type ou couper son ombre au loin */
	struct FLightRecord
	{
		FVector Pos = FVector::ZeroVector;
		float Yaw = 0.f;
		EBRFixture Fixture = EBRFixture::None;
		float SourceLength = 0.f;
		bool bShadow = false;
		/** L'ombre est actuellement projetee (selon la distance aux joueurs) */
		bool bShadowOn = false;
		bool bArea = false;
		/** Intensite nominale appliquee (lumens, deja reduits pour une lumiere surfacique) */
		float Lumens = 0.f;
		int32 FlickerIndex = INDEX_NONE;
		int32 PoweredIndex = INDEX_NONE;
		TWeakObjectPtr<ULocalLightComponent> Comp;
	};
	TArray<FLightRecord> LightRecords;
	/** Cree le composant de lumiere d'un enregistrement (surfacique ou ponctuel), l'enregistre et le range dans Extra */
	ULocalLightComponent* CreateLightComponent(FLightRecord& R, bool bArea);
	static bool IsAreaFixture(EBRFixture Fixture);
	/** Prochain lot a creer : avec collision d'abord si bCollisionFirst ; vide s'il n'y en a plus de ce type */
	FString NextBatchKey(bool bCollision) const;
	TWeakObjectPtr<ABRWorld> World;
	int32 LightCount = 0;
};
