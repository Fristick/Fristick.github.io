// Le "monde" : grille procedurale infinie, streaming de chunks, population d'entites,
// environnement (brouillard, ciel, post-process, ambiance sonore) et transitions entre niveaux.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BRTypes.h"
#include "BRWorld.generated.h"

class ABRChunk;
class ABREntity;
class ABRCharacter;
class UExponentialHeightFogComponent;
class USkyAtmosphereComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class UPostProcessComponent;
class UAudioComponent;

/** Lumiere d'une cellule */
struct FBRLightInfo
{
	bool bHas = false;
	bool bBroken = false;
	bool bFlicker = false;
	bool bShadow = false;
	FVector Offset = FVector::ZeroVector; // decalage dans la cellule (local au centre)
	float Yaw = 0.f;
};

/** Objectif affiche dans l'inventaire (colonne OBJECTIFS) et dans le coin de l'ecran */
struct FBRObjective
{
	FString Text;
	int32 Progress = 0;
	int32 Goal = 1;
	float Partial = 0.f;     // progression de l'etape en cours (enregistrement...)
	bool bRequired = false;  // necessaire pour quitter le niveau
	bool IsDone() const { return Progress >= Goal; }
};

UCLASS()
class BACKROOMS_API ABRWorld : public AActor
{
	GENERATED_BODY()

public:
	ABRWorld();

	static ABRWorld* Get(const UObject* WorldContext);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------ Niveaux
	/** Lance une transition (fondu + effet "noclip") vers un niveau. -1 = niveau aleatoire */
	void RequestTransition(int32 TargetLevel, bool bFromDeath = false);
	/** Charge immediatement un niveau (sans fondu) */
	void LoadLevelNow(int32 LevelNumber);
	const FBRLevelDef& Def() const;
	int32 GetLevelNumber() const;
	bool IsTransitioning() const { return TransState != ETrans::None; }
	/** Opacite du fondu au noir (0..1) */
	float GetFade() const { return Fade; }
	/** Intensite de l'effet glitch pendant une transition */
	float GetGlitch() const { return Glitch; }
	/** Temps restant d'affichage du titre du niveau */
	float GetTitleTime() const { return TitleTime; }
	void ReplayTitle() { TitleTime = 7.f; }
	float GetLevelTime() const { return LevelTime; }
	/** Appele par le personnage a sa mort */
	void HandlePlayerDeath();

	// ------------------------------------------------------------ Grille
	float CellSize() const;
	FIntPoint WorldToCell(const FVector& P) const;
	FVector CellCenter(const FIntPoint& C, float Z = 0.f) const;
	FIntPoint CellToChunk(const FIntPoint& C) const;
	bool IsSpawnArea(int32 X, int32 Y) const;
	bool IsSolid(int32 X, int32 Y) const;
	/** Arete entre (X,Y) et (X+1,Y) */
	EBREdge EdgeE(int32 X, int32 Y) const;
	/** Arete entre (X,Y) et (X,Y+1) */
	EBREdge EdgeN(int32 X, int32 Y) const;
	bool CanStep(const FIntPoint& From, const FIntPoint& To) const;
	bool IsWalkable(const FIntPoint& C) const { return !IsSolid(C.X, C.Y); }
	bool IsDarkZone(int32 X, int32 Y) const;
	FBRLightInfo CellLight(int32 X, int32 Y) const;
	/** Pilier au coin (+X,+Y) de la cellule */
	bool HasPillar(int32 X, int32 Y) const;
	/** Hauteur d'un batiment (Niveau 11), en cm */
	float BuildingHeight(int32 BlockX, int32 BlockY) const;
	/** Maison presente sur ce lot (Niveau 9) */
	bool HasHouse(int32 LotX, int32 LotY) const;
	/** Bassin profond (Niveau 37) : le sol de la cellule est a -PoolDepth */
	bool IsPoolCell(int32 X, int32 Y) const;
	/** Hauteur du sol sous un point (0, ou le fond d'un bassin) */
	float FloorZAt(const FVector& P) const;
	/** Camera sous l'eau (0..1) : brouillard turquoise dense */
	void SetUnderwater(float Blend);
	/** Estimation de l'eclairage (0 = noir, 1 = bien eclaire) */
	float LightLevelAt(const FVector& P) const;
	/** A* sur la grille */
	bool FindPath(const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutPath, int32 MaxNodes = 1500) const;
	bool IsChunkLoaded(const FIntPoint& Chunk) const { return Chunks.Contains(Chunk); }
	uint32 GetSeed() const { return Seed; }

	// ------------------------------------------------------------ Etat
	bool IsCollected(uint64 Id) const { return Collected.Contains(Id); }
	void MarkCollected(uint64 Id) { Collected.Add(Id); }
	void Discover(EBREntityKind Kind);
	bool IsDiscovered(EBREntityKind Kind) const { return Discovered.Contains(static_cast<int32>(Kind)); }
	void UnregisterEntity(ABREntity* Entity);
	ABREntity* SpawnEntity(EBREntityKind Kind, const FVector& Location);
	const TArray<int32>& GetVisitedLevels() const { return Visited; }
	const TArray<TObjectPtr<ABREntity>>& GetEntities() const { return Entities; }

	// ------------------------------------------------------------ v2 : coupures de courant
	/** Coupure de courant en cours (les neons sont eteints ou en train de lacher) */
	bool IsBlackout() const { return BlackoutPhase == EBlackout::Failing || BlackoutPhase == EBlackout::Dark; }
	/** Alimentation electrique 0..1 (vacille pendant les coupures) */
	float GetPower() const { return Power; }
	/** Declenche une coupure tout de suite (console : BRBlackout) */
	void ForceBlackout();

	// ------------------------------------------------------------ v2 : objectifs
	void GetObjectives(TArray<FBRObjective>& Out) const;
	/** false (+ raison) si les sorties sont encore instables */
	bool CanLeaveLevel(FString& OutReason) const;
	bool AreObjectivesComplete() const;
	void OnVHSCollected();
	/** Appele chaque image tant que le joueur tient le camescope */
	void NotifyRecording(float Dt, const FVector& Eye, const FVector& Dir);
	/** Entite visible dans un cone (ligne de vue verifiee) */
	ABREntity* FindVisibleEntity(const FVector& Eye, const FVector& Dir, float MaxDist, float MinDot) const;
	/** Ce que le camescope est en train de filmer (HUD) */
	const FString& GetRecordLabel() const { return RecordLabel; }
	float GetRecordProgress() const { return RecordProgress; }
	int32 GetVHSFound() const { return VHSFound; }
	/** Console : valide les taches d'enregistrement */
	void DebugCompleteRecording();

	/** Niveau de depart choisi dans le menu */
	int32 StartLevel = 0;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	float UnderwaterBlend = 0.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> AmbientAudio;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> HumAudio;

	UPROPERTY()
	TMap<FIntPoint, TObjectPtr<ABRChunk>> Chunks;

	UPROPERTY()
	TArray<TObjectPtr<ABREntity>> Entities;

private:
	enum class ETrans : uint8 { None, FadingOut, FadingIn };
	enum class EBlackout : uint8 { None, Failing, Dark, Restoring };

	void ClearLevel();
	void ApplyEnvironment();
	void UpdateStreaming(bool bSynchronous);
	void SpawnChunk(const FIntPoint& Coord);
	void PlacePlayer();
	void UpdatePopulation(float Dt);
	void UpdateAudio(float Dt);
	void UpdatePhenomena(float Dt);
	ABRCharacter* GetPlayer() const;
	float ZoneDensity(int32 X, int32 Y) const;
	bool MazeOpen(int32 X, int32 Y, bool bEast) const;
	void UpdateBlackout(float Dt);
	void ApplyPower(bool bForce);
	void CompleteTask(const FString& Text);

	const FBRLevelDef* Current = nullptr;
	uint32 Seed = 1337;
	ETrans TransState = ETrans::None;
	float TransTimer = 0.f;
	int32 PendingLevel = 0;
	bool bPendingDeath = false;
	float Fade = 1.f;
	float Glitch = 0.f;
	float TitleTime = 0.f;
	float LevelTime = 0.f;
	float StreamTimer = 0.f;
	float SpawnTimer = 20.f;
	float PhenomenaTimer = 40.f;
	float DeathTimer = -1.f;
	bool bPlayerPlaced = false;
	TSet<uint64> Collected;
	TSet<int32> Discovered;
	TArray<int32> Visited;

	// v2 : coupures
	EBlackout BlackoutPhase = EBlackout::None;
	float BlackoutTimer = 90.f;
	float PowerFlickerTimer = 0.f;
	float Power = 1.f;
	float AppliedPower = -1.f;

	// v2 : objectifs
	int32 VHSFound = 0;
	bool bBlackoutRecorded = false;
	bool bEntityRecorded = false;
	bool bObjectivesAnnounced = false;
	float BlackoutRecordTime = 0.f;
	float EntityRecordTime = 0.f;
	float RecordProgress = 0.f;
	FString RecordLabel;
};
