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
class UMaterialInstanceDynamic;
class ABRPlayerController;
class APlayerState;
class UBRWaterSim;

/** Niveau en cours, choisi par le serveur et recopie chez les clients (multijoueur) */
USTRUCT()
struct FBRNetLevel
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Level = 0;

	/** Graine de generation : tous les joueurs construisent exactement les memes salles */
	UPROPERTY()
	uint32 Seed = 0;

	/** Incremente a chaque chargement (0 = aucun niveau encore) */
	UPROPERTY()
	int32 Serial = 0;
};

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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------ Multijoueur
	/** Partie en reseau (hote ou client) */
	bool IsNetGame() const;
	/** Le niveau est construit (un client attend celui du serveur) */
	bool IsLevelReady() const { return bLevelReady; }
	/** Tous les joueurs de la partie (le sien et ceux des autres) */
	void GetPlayers(TArray<ABRCharacter*>& Out) const;
	/** Multijoueur : apres une mort, on se reveille au point de depart du niveau */
	void RespawnLocalPlayer();
	/** Place d'un joueur autour du point de depart (0 = l'hote) : rang de son identifiant, le meme chez tous */
	int32 PlayerSlot(const APlayerState* PS) const;
	/** Point d'apparition de cette place pour une capsule de demi-hauteur Half (au centre, puis en cercle) */
	FVector SpawnSpot(int32 Slot, float Half) const;
	/** Multijoueur : un coequipier vient de nous relever */
	void CancelPlayerDeath() { DeathTimer = -1.f; }
	/** A terre : secondes avant le reveil au point de depart (-1 sinon) */
	float GetDeathTimer() const { return DeathTimer; }
	/** A terre : abandonner et se reveiller tout de suite au point de depart */
	void GiveUpDowned() { DeathTimer = FMath::Min(DeathTimer, 0.3f); }
	/** Un autre joueur est encore debout (il peut nous relever) */
	bool HasLivingTeammate() const;
	/** Client : une entite repliquee rejoint la liste (lampes rouges, camescope, sante mentale) */
	void RegisterEntity(ABREntity* Entity);
	/** Serveur : un client a ramasse un objet (il disparait chez tout le monde) */
	void ServerCollected(uint64 Id);
	/** Serveur : un client a termine un enregistrement (0 = coupure, 1 = entite) */
	void ServerCompleteObjective(uint8 Which);
	/** Console : valide tous les objectifs (serveur) */
	void DebugCompleteObjectives();

	// ------------------------------------------------------------ Niveaux
	/** Lance une transition (fondu + effet "noclip") vers un niveau. -1 = niveau aleatoire */
	void RequestTransition(int32 TargetLevel, bool bFromDeath = false);
	/** Charge immediatement un niveau (sans fondu). Graine 0 = nouvelle graine aleatoire */
	void LoadLevelNow(int32 LevelNumber, uint32 InSeed = 0);
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
	/** v4.3 : niveau fini (BoundsChunks > 0) : ce chunk / cette cellule est dans l'enceinte (toujours vrai sinon) */
	bool IsChunkInBounds(const FIntPoint& Chunk) const;
	bool IsCellInBounds(int32 X, int32 Y) const;
	/** Niveau fini : nombre de chunks de l'enceinte (sans les 4 du point de depart si bAvoidSpawn) */
	int32 BoundedChunkCount(bool bAvoidSpawn) const;
	/** Niveau fini : ce chunk fait partie des Count chunks tires pour Salt (le meme tirage chez tous les joueurs) */
	bool IsChunkPicked(const FIntPoint& Chunk, int32 Salt, int32 Count, bool bAvoidSpawn) const;
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
	/** Hauteur du sol sous un point (0, le fond d'un bassin, ou le dessus d'un trottoir) */
	float FloorZAt(const FVector& P) const;
	/** Trottoir le long du cote Side de la cellule (0 : +X, 1 : -X, 2 : +Y, 3 : -Y), Niveau 37 */
	bool HasDeck(int32 X, int32 Y, int32 Side) const;
	/** Hauteur d'un trottoir (ou de sa marche immergee) sous un point, 0 ailleurs */
	float DeckZAt(const FVector& P) const;
	/** Bureau cloisonne (Niveau 4) ; OutDoorSide = cote de sa porte (0..3, comme HasDeck) */
	bool IsCubicle(int32 X, int32 Y, int32* OutDoorSide = nullptr) const;
	/** Camera sous l'eau (0..1) : brouillard turquoise dense */
	void SetUnderwater(float Blend);
	/** Surface (texture, teinte) de l'eau du niveau */
	FBRSurface GetWaterSurface() const;
	/** Positions des entites qui font virer les lampes au rouge (Niveau 0) */
	const TArray<FVector>& GetRedLightSources() const { return RedSources; }
	float GetRedLightRadius() const { return Def().RedLightRadius; }
	/** Le point est-il dans une cachette ? Les trous dans le mur demandent d'etre accroupi */
	bool IsInHidingSpot(const FVector& Location, bool bCrouched) const;
	/** Une cachette a moins de Radius cm (indication a l'ecran) */
	bool FindHidingSpotNear(const FVector& Location, float Radius, bool& bOutNeedsCrouch) const;
	/** Rond dans l'eau (pas, plongeon, sortie de l'eau). Strength ~ creux en cm */
	void AddWaterRipple(const FVector& Location, float Strength);
	/** Vagues simulees autour du joueur (nullptr hors des niveaux inondes) */
	UBRWaterSim* GetWaterSim() const { return WaterSim; }
	/** Estimation de l'eclairage (0 = noir, 1 = bien eclaire) */
	float LightLevelAt(const FVector& P) const;
	/** A* sur la grille */
	bool FindPath(const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutPath, int32 MaxNodes = 1500) const;
	bool IsChunkLoaded(const FIntPoint& Chunk) const { return Chunks.Contains(Chunk); }
	int32 GetChunkCount() const { return Chunks.Num(); }
	uint32 GetSeed() const { return Seed; }

	// ------------------------------------------------------------ Etat
	bool IsCollected(uint64 Id) const { return Collected.Contains(Id) || NetCollected.Contains(Id); }
	void MarkCollected(uint64 Id);
	void Discover(EBREntityKind Kind);
	bool IsDiscovered(EBREntityKind Kind) const { return Discovered.Contains(static_cast<int32>(Kind)); }
	void UnregisterEntity(ABREntity* Entity);
	ABREntity* SpawnEntity(EBREntityKind Kind, const FVector& Location);
	const TArray<int32>& GetVisitedLevels() const { return Visited; }
	/** Sauvegardes : entites deja rencontrees */
	TArray<int32> GetDiscoveredList() const { return Discovered.Array(); }
	/** Sauvegardes : reprend le journal d'une partie (entites rencontrees, niveaux visites), sans annonce */
	void RestoreJournal(const TArray<int32>& InDiscovered, const TArray<int32>& InVisited);
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

	/** Niveau de depart choisi dans le menu */
	int32 StartLevel = 0;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	float UnderwaterBlend = 0.f;

	/** Vagues a la surface de l'eau autour du joueur (sillage des joueurs et des entites, plongeons, gouttes) */
	UPROPERTY()
	TObjectPtr<UBRWaterSim> WaterSim;

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

	// ---- Etat partage (serveur -> clients)
	UPROPERTY(ReplicatedUsing = OnRep_NetLevel)
	FBRNetLevel NetLevel;

	UPROPERTY(ReplicatedUsing = OnRep_Blackout)
	uint8 NetBlackout = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Collected)
	TArray<uint64> NetCollected;

	UPROPERTY(ReplicatedUsing = OnRep_Objectives)
	int32 VHSFound = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Objectives)
	bool bBlackoutRecorded = false;

	UPROPERTY(ReplicatedUsing = OnRep_Objectives)
	bool bEntityRecorded = false;

	UFUNCTION()
	void OnRep_NetLevel();

	UFUNCTION()
	void OnRep_Blackout();

	UFUNCTION()
	void OnRep_Collected();

	UFUNCTION()
	void OnRep_Objectives();

	/** Tout le monde plonge dans le noir en meme temps ; le niveau suit quand le serveur l'a choisi */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastTransition(int32 TargetLevel, bool bFromDeath);

private:
	enum class ETrans : uint8 { None, FadingOut, FadingIn };
	enum class EBlackout : uint8 { None, Failing, Dark, Restoring };

	void ClearLevel();
	void BeginTransition(int32 TargetLevel, bool bFromDeath);
	/** Client : suit le niveau du serveur. false tant qu'aucun niveau n'est construit */
	bool SyncNetLevel();
	void LoadNetLevel();
	void EnterBlackoutPhase(uint8 Phase, bool bSilent = false);
	void DestroyPickup(uint64 Id);
	void AnnounceVHS();
	void CompleteObjective(uint8 Which);
	ABRPlayerController* LocalPC() const;
	/** Un joueur vivant au hasard (point d'ancrage des apparitions) */
	ABRCharacter* RandomLivingPlayer() const;
	void ApplyEnvironment();
	void UpdateStreaming(bool bSynchronous);
	void SpawnChunk(const FIntPoint& Coord);
	/** Place le joueur local au point de depart. bKeepServerSpot : garde la place deja donnee par le serveur */
	void PlacePlayer(bool bKeepServerSpot = false);
	void UpdatePopulation(float Dt);
	void UpdateAudio(float Dt);
	void UpdatePhenomena(float Dt);
	/** Sante mentale basse : une silhouette ou un sourire que seul ce joueur voit */
	bool SpawnHallucination(ABRCharacter* P);
	ABRCharacter* GetPlayer() const;
	float ZoneDensity(int32 X, int32 Y) const;
	/** Motif des bureaux cloisonnes : -1 hors zone, 0 allee, 1 bureau ouvert vers -Y, 2 bureau ouvert vers +Y */
	int32 CubicleRole(int32 X, int32 Y) const;
	/** Arete dans une zone de bureaux (bOut = false : la regle ordinaire s'applique) */
	EBREdge CubicleEdge(int32 X, int32 Y, bool bEast, bool& bOut) const;
	bool MazeOpen(int32 X, int32 Y, bool bEast) const;
	void UpdateBlackout(float Dt);
	void ApplyPower(bool bForce);
	void CompleteTask(const FString& Text);
	void UpdateWaterSim(float Dt);
	/** Niveau 0 : l'entite qui fait des rondes est toujours la (elle reapparait si elle s'eloigne trop) */
	void UpdatePatrol(float Dt);
	/** Cherche ou faire apparaitre une entite entre MinDist et MaxDist du joueur Anchor (hors de la vue de tous si bAvoidSight) */
	bool FindSpawnSpot(EBREntityKind Kind, const ABRCharacter* Anchor, float MinDist, float MaxDist, bool bAvoidSight, FVector& Out) const;
	/** Aucun joueur ne voit ce point */
	bool IsHiddenFromPlayers(const FVector& Loc) const;
	void SpawnBlackoutEntities();
	/** Un Smiler de plus pendant la coupure ; bInView : il surgit dans le champ de vision du joueur */
	bool SpawnBlackoutSmiler(const ABRCharacter* P, bool bInView);
	/** Point dans le noir que le joueur voit (ligne de vue degagee), devant lui, entre 5 et 14 m */
	bool FindBlackoutSpot(const ABRCharacter* P, FVector& Out) const;
	void DismissBlackoutEntities();
	/** Coupure en cours : prochain Smiler qui surgit */
	float BlackoutSpawnTimer = 0.f;
	TArray<FVector> RedSources;
	TArray<TWeakObjectPtr<ABREntity>> BlackoutEntities;
	float PatrolSpawnTimer = 0.f;

	float DripTimer = 1.f;
	bool bMeshesChecked = false;

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
	bool bLevelReady = false;
	int32 LoadedSerial = 0;
	int32 PrevVHSFound = 0;
	bool bPrevBlackoutRecorded = false;
	bool bPrevEntityRecorded = false;
	bool bObjectiveSent[2] = { false, false };
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
	bool bObjectivesAnnounced = false;
	float BlackoutRecordTime = 0.f;
	float EntityRecordTime = 0.f;
	float RecordProgress = 0.f;
	FString RecordLabel;
};
