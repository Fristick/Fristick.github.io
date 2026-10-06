// Le "monde" : grille procedurale infinie, streaming de chunks, population d'entites,
// environnement (brouillard, ciel, post-process, ambiance sonore) et transitions entre niveaux.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BRTypes.h"
#include "BRMissionLogic.h"
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
class UReverbEffect;
class ABRPlayerController;
class APlayerState;
class UBRWaterSim;
class ABRMissionDevice;
class ABRExit;

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

/** v4.11 : etat de la mission du niveau, tenu par l'hote. Un client qui rejoint recoit cet instantane avec le niveau
 *  (meme acteur, meme paquet), puis chaque changement. Le plan se reconstruit chez chacun d'apres le niveau, la graine,
 *  la version de generation et la variante de secours. */
USTRUCT()
struct FBRNetMission
{
	GENERATED_BODY()

	/** Version de generation : 0 aucune, 1 ancien mode (sessions d'avant la v4.11 : cassettes du Niveau 0, sorties
	 *  libres), 2 missions v4.11 (BRMission::GenVersion) */
	UPROPERTY()
	uint8 Gen = 0;

	/** Variante de secours deterministe (graine invalide ou placement impossible) */
	UPROPERTY()
	bool bFallback = false;

	/** Niveau (FBRNetLevel::Serial) auquel cet etat appartient */
	UPROPERTY()
	int32 Serial = 0;

	/** Revision, augmentee a chaque changement */
	UPROPERTY()
	uint16 Rev = 0;

	/** Etat serialise (BRMission::Serialize) */
	UPROPERTY()
	TArray<uint8> State;

	/** Dernier changement : mecanisme et retour (son et lueur chez tous) */
	UPROPERTY()
	uint8 LastDevice = 255;

	UPROPERTY()
	uint8 LastFeedback = 0;
};

/** v4.11 : donnees de campagne (fragments de route, objectifs facultatifs, fins vues), tenues par l'hote */
USTRUCT()
struct FBRNetCampaign
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 RouteBits = 0;

	UPROPERTY()
	uint16 OptionalFound = 0;

	/** Bits : 1 fin principale vue, 2 variante vue */
	UPROPERTY()
	uint8 Endings = 0;
};

/** v4.11 : sortie de groupe. Toucher une sortie ne transporte plus tout le monde : l'hote verifie la mission et le
 *  rassemblement (joueurs vivants et charges a moins de 8 m en 3D de la sortie, meme etage, sans mur entre), annonce le
 *  depart, puis emmene le groupe ; un joueur a terre est emmene, un joueur en chargement n'est pas attendu. */
USTRUCT()
struct FBRNetDeparture
{
	GENERATED_BODY()

	/** 0 aucun, 1 rassemblement, 2 depart, 3 annule */
	UPROPERTY()
	uint8 Phase = 0;

	/** Annulation : 1 temps ecoule, 2 l'initiateur s'est eloigne, 3 annule par un joueur, 4 plus personne debout */
	UPROPERTY()
	uint8 Reason = 0;

	/** Numero du depart (une demande repetee ou perimee ne relance rien) */
	UPROPERTY()
	uint16 Id = 0;

	UPROPERTY()
	int32 Serial = 0;

	UPROPERTY()
	int32 Target = 0;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Temps du serveur (AGameStateBase::GetServerWorldTimeSeconds) a l'annulation automatique */
	UPROPERTY()
	float Deadline = 0.f;

	UPROPERTY()
	uint8 Ready = 0;

	UPROPERTY()
	uint8 Needed = 0;

	/** Joueurs a terre emmenes */
	UPROPERTY()
	uint8 Carried = 0;

	/** Nom de l'initiateur */
	UPROPERTY()
	FString By;
};

/** Objectif affiche dans l'inventaire (colonne OBJECTIFS) et dans le coin de l'ecran */
struct FBRObjective
{
	FString Text;
	int32 Progress = 0;
	int32 Goal = 1;
	float Partial = 0.f;     // progression de l'etape en cours (enregistrement...)
	bool bRequired = false;  // necessaire pour quitter le niveau
	bool bHideCount = false; // v4.11 : etape sans compteur (affichee sans "0/1")
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
	/** v4.8 : reveil refuse par le serveur : le joueur reste a terre le temps restant */
	void SetDeathTimer(float Seconds) { DeathTimer = Seconds; }
	/** A terre : abandonner et se reveiller tout de suite au point de depart */
	void GiveUpDowned() { DeathTimer = FMath::Min(DeathTimer, 0.3f); }
	/** Un autre joueur est encore debout (il peut nous relever) */
	bool HasLivingTeammate() const;
	/** Client : une entite repliquee rejoint la liste (lampes rouges, camescope, sante mentale) */
	void RegisterEntity(ABREntity* Entity);
	/** v4.11 : serveur : transaction de ramassage. Verifie le niveau (numero de la demande), l'etat du joueur (vivant,
	 *  charge), l'objet (existe, meme type, encore disponible), la distance 3D depuis les yeux du joueur et la ligne de vue,
	 *  la place annoncee ; puis, en une seule operation : objet attribue et retire pour tous, stock de soin credite,
	 *  cassette comptee (ancien mode du Niveau 0) ou document facultatif note. OutItem : objet attribue */
	EBRPickupResult ServerTryCollect(ABRCharacter* By, uint64 Id, int32 LevelSerial, uint8 ExpectedItem, uint8 Room, EBRItem& OutItem);
	/** v4.11 : distance maximale (cm, 3D, depuis les yeux) acceptee par l'hote pour un ramassage : portee de visee (260)
	 *  plus la marge du reseau */
	static constexpr float PickupReachServer = 420.f;
	/** v4.11 : numero du niveau charge sur cette machine (FBRNetLevel::Serial) : une demande ou une reponse d'un autre
	 *  niveau est perimee */
	int32 GetLevelSerial() const { return LoadedSerial; }
	/** Serveur : un enregistrement est valide (0 = coupure, 1 = entite) */
	void ServerCompleteObjective(uint8 Which);
	/** v4.11 : serveur : enregistrement annonce par un client, valide seulement si les conditions sont vraies chez l'hote :
	 *  coupure noire depuis au moins 4,5 s (5 s demandees, marge du reseau), ou entite a moins de 26 m du joueur, visible */
	void ServerValidateRecording(uint8 Which, ABRCharacter* By);
	/** Console : valide tous les objectifs (serveur) */
	void DebugCompleteObjectives();

	// ------------------------------------------------------------ v4.11 : missions de niveau
	/** La mission v4.11 du niveau est en place (sinon : ancien mode ou niveau sans mission) */
	bool IsMissionActive() const;
	/** Ancien mode des objectifs (cassettes VHS du Niveau 0) : session commencee avant la v4.11, jusqu'a sa sortie */
	bool IsLegacyObjectives() const;
	uint8 GetMissionGen() const { return MissionGen; }
	bool IsMissionFallback() const { return MissionPlan.bFallback; }
	const BRMission::FPlan& GetMissionPlan() const { return MissionPlan; }
	const BRMission::FState& GetMissionState() const { return MissionState; }
	BRMission::FCampaign GetMissionCampaign() const;
	void GetMissionEval(BRMission::FEval& Out) const;
	/** Cellule reservee a un mecanisme ou a une sortie de mission (les chunks n'y posent rien) */
	bool IsMissionCell(int32 X, int32 Y) const { return MissionCells.Contains(FIntPoint(X, Y)); }
	/** Mecanisme d'apres son indice (nullptr si absent) */
	ABRMissionDevice* FindMissionDevice(int32 Index) const;
	const TArray<TObjectPtr<ABRMissionDevice>>& GetMissionDevices() const { return MissionDevices; }
	/** Revision de l'etat recu (HUD) */
	uint16 GetMissionRev() const { return NetMission.Rev; }
	/** Serveur : action d'un joueur sur un mecanisme. Verifie le niveau, l'etat du joueur, la distance 3D, la ligne de
	 *  vue, la cadence des actions maintenues (une unite par 0,4 s au plus) et le rearmement apres une erreur ; applique
	 *  l'action (une seule attribution par objet d'equipe) et publie l'etat. Retour : BRMission::EFeedback ou
	 *  BRMissionText::Cooldown/TooFar/NotNow/Taken */
	uint8 ServerMissionAct(ABRCharacter* By, int32 Device, uint8 Action, int32 LevelSerial, uint8& OutRelated, uint8& OutCount);
	/** Distance 3D maximale (cm) acceptee par l'hote entre les yeux du joueur et un mecanisme */
	static constexpr float MissionReachServer = 340.f;
	/** Une sortie vers Target peut etre prise maintenant (mission du niveau resolue, ou sortie de retour). OutReason :
	 *  message dans la langue du joueur */
	bool CanUseExit(int32 Target, FString& OutReason) const;
	/** Etat de mission a sauvegarder (vide hors mission v4.11) */
	TArray<uint8> GetMissionBlob() const;
	/** Campagne : donnees de la sauvegarde de l'hote (chargement d'une partie) */
	void SetCampaign(uint8 RouteBits, uint16 OptionalFound, uint8 Endings);
	const FBRNetCampaign& GetCampaign() const { return NetCampaign; }
	/** Cassette VHS (document facultatif) acceptee par l'hote dans une nouvelle partie */
	void OnMissionLoreFound();
	/** Console : resout la mission (serveur) */
	void DebugCompleteMission();
	/** Mecanismes : affichage selon les chunks construits et animation des seuls mecanismes proches */
	void UpdateMissionDevices(float Dt);

	// ------------------------------------------------------------ v4.11 : sortie de groupe et fin
	/** Un joueur prend une sortie : seul, depart immediat ; en ligne, demande de depart de groupe a l'hote */
	void RequestDeparture(ABRCharacter* By, int32 Target, const FVector& ExitLocation);
	/** Serveur : demande d'un joueur ; false (+ raison : 1 sortie fermee, 2 trop loin, 3 autre depart, 4 niveau perime,
	 *  5 a terre ou en chargement) si refusee */
	bool ServerStartDeparture(ABRCharacter* By, int32 Target, int32 LevelSerial, uint8& OutReason);
	/** Serveur : un joueur annule le depart en cours */
	void ServerCancelDeparture(ABRCharacter* By);
	const FBRNetDeparture& GetDeparture() const { return NetDeparture; }
	/** Secondes restantes avant l'annulation automatique du depart */
	float GetDepartureRemaining() const;
	/** Rayon (cm) du rassemblement, ecart vertical maximal (cm) */
	static constexpr float GatherRadius = 800.f;
	static constexpr float GatherHeight = 260.f;
	/** Fin de campagne affichee (Niveau 11) et sa variante */
	bool IsEndingShown() const { return bEndingShown; }
	bool IsEndingVariant() const { return bEndingVariant; }
	/** Fin : continuer l'exploration (l'hote emmene le groupe vers un niveau au hasard) ou revenir au menu */
	void CloseEnding(bool bContinue);

	// ------------------------------------------------------------ Niveaux
	/** Lance une transition (fondu + effet "noclip") vers un niveau. -1 = niveau aleatoire.
	 *  InSeed (serveur) : graine imposee au niveau (0 : nouvelle graine, ou celle de -BRSeed) */
	void RequestTransition(int32 TargetLevel, bool bFromDeath = false, uint32 InSeed = 0);
	/** Charge immediatement un niveau (sans fondu). Graine 0 = nouvelle graine aleatoire */
	void LoadLevelNow(int32 LevelNumber, uint32 InSeed = 0);
	const FBRLevelDef& Def() const;
	int32 GetLevelNumber() const;
	bool IsTransitioning() const { return TransState != ETrans::None; }
	/** v4.10 : ecran de preparation : les ressources indispensables du niveau suivant finissent de se charger */
	bool IsPreparing() const { return TransState == ETrans::Preparing; }
	/** v4.10 : avancement (0..1) et duree de la preparation en cours ; niveau prepare */
	float GetPrepareProgress() const { return PrepProgress; }
	float GetPrepareTime() const { return PrepTime; }
	int32 GetPreparingLevel() const { return PendingLevel; }
	/** v4.10 : delai apres lequel le retour au menu est propose, et delai maximal (on entre alors avec ce qui est pret) */
	static constexpr float PrepareMenuDelay = 10.f;
	static constexpr float PrepareMaxSeconds = 25.f;
	/** v4.10 (tests) : numero de la demande de preparation en cours */
	uint32 GetPrepareSerial() const { return PrepSerial; }
	/** Opacite du fondu au noir (0..1) */
	float GetFade() const { return Fade; }
	/** Intensite de l'effet glitch pendant une transition */
	float GetGlitch() const { return Glitch; }
	/** Temps restant d'affichage du titre du niveau */
	float GetTitleTime() const { return TitleTime; }
	void ReplayTitle() { TitleTime = 7.f; }
	float GetLevelTime() const { return LevelTime; }
	/** Appele par le personnage a sa mort. bNoRevive (chute dans une fosse) : personne ne peut le relever */
	void HandlePlayerDeath(EBRDeathCause Cause);

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
	/** A* sur la grille (v4.6 : les cellules d'une salle de fosses coutent plus cher : on passe par la galerie si possible) */
	bool FindPath(const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutPath, int32 MaxNodes = 1500) const;

	// ------------------------------------------------------------ v4.6 : salles de fosses ("Hole Variation")
	/** Le niveau a des salles de fosses */
	bool HasPits() const;
	/** Salle de fosses de ce chunk : rectangle de cellules [Min, Max[ ; false si le chunk n'en a pas.
	 *  Ne depend que de la graine et des coordonnees du chunk : la meme chez tous les joueurs, quel que soit l'ordre de chargement */
	bool GetPitRoom(const FIntPoint& Chunk, FIntRect& OutRoom) const;
	/** Cellule d'une salle de fosses : reservee (pas de murs interieurs, d'objets, de cachettes ni de sorties) */
	bool IsPitRoomCell(int32 X, int32 Y) const;
	/** Fosse percee au coin (+X, +Y) de la cellule (X, Y) */
	bool HasPitAtCorner(int32 X, int32 Y) const;
	/** Cote reel d'une fosse (cm) : PitHoleSize, reduit pour garder des passages d'au moins PitPassage (et 120 cm) */
	float GetPitHoleSize() const;
	/** Le point (XY) est au-dessus du vide d'une fosse ; Margin > 0 elargit la fosse (marge de securite des entites) */
	bool IsOverPit(const FVector& P, float Margin = 0.f) const;
	/** Le segment A-B (XY) passe au-dessus d'une fosse, pour un corps qui garde Margin cm du bord */
	bool SegmentCrossesPit(const FVector& A, const FVector& B, float Margin) const;
	/** Salles de fosses des chunks a moins de Radius chunks de AroundChunk (niveau fini : toutes) */
	void GetPitRooms(TArray<FIntRect>& Out, const FIntPoint& AroundChunk, int32 Radius) const;
	/** Point de vue sur la salle de fosses la plus proche (coin de la salle, regard en diagonale sur la grille de fosses) */
	bool FindPitRoomView(const FVector& From, FVector& OutLoc, FRotator& OutRot) const;
	/** Niveau fini : la cellule est atteignable depuis le depart sans entrer dans une salle de fosses (les objectifs et
	 *  les sorties n'y sont places que la : il y a toujours un chemin de contournement). Toujours vrai dans un niveau infini */
	bool IsSafelyReachable(const FIntPoint& C) const;
	/** Niveau fini : la cellule est atteignable depuis le depart, en passant au besoin par les passages entre les fosses */
	bool IsReachable(const FIntPoint& C) const;
	/** Graine d'un niveau a partir d'un nombre choisi par l'utilisateur (-BRSeed=<n>, commande BRSeed <n>) */
	static uint32 SeedFromUser(uint32 N, int32 Level);
	/** Graine de demonstration de la v4.6 (-BRSeed=9) : au Niveau 0, salle de 16 fosses a 14 cellules du depart */
	static constexpr uint32 DemoSeed = 9;
	/** v4.7 : chunk entierement construit (sol, murs, objets). Un chunk encore en preparation ne compte pas : les
	 *  entites n'y vont pas, rien n'y apparait */
	bool IsChunkLoaded(const FIntPoint& Chunk) const;
	/** v4.7 : le sol et les murs du chunk existent (collisions creees), meme si ses details sont encore en preparation */
	bool HasChunkFloor(const FIntPoint& Chunk) const;
	int32 GetChunkCount() const { return Chunks.Num(); }
	/** v4.7 : chunks en cours de construction (etalee sur plusieurs images) */
	int32 GetPreparingChunkCount() const;
	/** v4.5 : temps de construction des chunks (ms), pour le rapport du test automatique.
	 *  v4.7 : MaxChunkBuildMs = pire temps passe a construire des chunks pendant UNE image (saccade) ; ChunkBuildMsTotal
	 *  et ChunksBuilt : chunks termines ; temps par etape ; constructions forcees (chunk pas pret sous un joueur) */
	float LastChunkBuildMs = 0.f;
	float MaxChunkBuildMs = 0.f;
	float ChunkBuildMsTotal = 0.f;
	int32 ChunksBuilt = 0;
	float ChunkPlanMsTotal = 0.f;
	float ChunkCollisionMsTotal = 0.f;
	float ChunkVisualMsTotal = 0.f;
	float ChunkLightMsTotal = 0.f;
	float ChunkActorMsTotal = 0.f;
	float MaxChunkPlanMs = 0.f;
	int32 ForcedChunkBuilds = 0;
	/** Budget de construction des chunks par image (ms) ; -BRChunkBudget=<ms>.
	 *  v4.8 : budget de generation PARTAGE par image (planification, creation et demontage des chunks), -BRFrameBudget=<ms>
	 *  (ou -BRChunkBudget=) ; il baisse tout seul quand l'image depasse 16,7 ms et remonte quand il y a de la marge */
	float ChunkStepBudgetMs = 4.f;
	/** v4.8 : budget de l'image en cours (adapte) et plafond configure */
	float FrameBudgetMs = 4.f;
	/** v4.8 : statistiques de fluidite du streaming : pire image de generation, images au-dela du budget, chunks
	 *  demontes, collisions preparees a l'avance (position predite d'un joueur) */
	int32 FramesOverBudget = 0;
	int32 ChunksTornDown = 0;
	int32 PredictedCollisionBuilds = 0;
	float MaxTeardownMs = 0.f;
	/** v4.9 : changement de type des lumieres : lumieres recreees, pire temps par image (ms) */
	int32 LightsRecreated = 0;
	float MaxLightRefreshMs = 0.f;
	void ResetChunkStats()
	{
		LightsRecreated = 0; MaxLightRefreshMs = 0.f;
		MaxChunkBuildMs = 0.f; ChunkBuildMsTotal = 0.f; ChunksBuilt = 0; ChunkPlanMsTotal = 0.f; ChunkCollisionMsTotal = 0.f;
		ChunkVisualMsTotal = 0.f; ChunkLightMsTotal = 0.f; ChunkActorMsTotal = 0.f; MaxChunkPlanMs = 0.f; ForcedChunkBuilds = 0;
		FramesOverBudget = 0; ChunksTornDown = 0; PredictedCollisionBuilds = 0; MaxTeardownMs = 0.f;
	}
	/** v4.8 : une lumiere a cette position doit-elle projeter une ombre (distance au joueur local le plus proche, selon le
	 *  profil graphique) ? */
	bool ShouldCastLocalShadow(const FVector& LightPos) const;
	/** v4.8 : distance d'ombre des lumieres locales (cm) pour le profil courant */
	static float LocalShadowDistance();
	/** v4.8 : reglages graphiques changes : lumieres des chunks deja construits (type, ombres), detail des entites */
	void OnGraphicsSettingsChanged();
	/** v4.8 : lumieres projetant une ombre (dernier passage) */
	int32 GetShadowedLightCount() const { return ShadowedLights; }
	/** v4.8 : temps passe a attendre la compilation des shaders derriere l'ecran noir de l'arrivee (0 : aucune attente) */
	float GetShaderHold() const { return ShaderHold; }
	/** v4.8 : des shaders (PSO, ou materiaux dans l'editeur) sont encore en compilation */
	static int32 ShadersInFlight();
	uint32 GetSeed() const { return Seed; }

	// ------------------------------------------------------------ Etat
	bool IsCollected(uint64 Id) const { return Collected.Contains(Id) || NetCollected.Contains(Id); }
	void Discover(EBREntityKind Kind);
	bool IsDiscovered(EBREntityKind Kind) const { return Discovered.Contains(static_cast<int32>(Kind)); }
	void UnregisterEntity(ABREntity* Entity);
	ABREntity* SpawnEntity(EBREntityKind Kind, const FVector& Location);
	const TArray<int32>& GetVisitedLevels() const { return Visited; }
	/** Sauvegardes : entites deja rencontrees */
	TArray<int32> GetDiscoveredList() const { return Discovered.Array(); }
	/** v4.7 : objets deja ramasses dans ce niveau (session sauvegardee) */
	TArray<uint64> GetCollectedList() const;
	/** v4.7 : ce niveau reprend une session sauvegardee (meme graine, objectifs et objets ramasses restaures) */
	bool IsResumedSession() const { return bResumed; }
	/** v4.7 : objectifs du niveau (sauvegarde de la session) */
	bool IsBlackoutRecorded() const { return bBlackoutRecorded; }
	bool IsEntityRecorded() const { return bEntityRecorded; }
	/** v4.7 : un point ou poser le joueur a la reprise : cellule libre, atteignable, pas au-dessus d'une fosse, sol touche
	 *  par un rayon (les chunks autour doivent etre construits) et capsule degagee. Out : centre de la capsule */
	bool FindSafeResumeSpot(const FVector& Wanted, float Half, float Radius, FVector& Out) const;
	/** v4.7 : point ou le joueur peut etre sauvegarde (au sol, pas au-dessus d'une fosse, pas dans l'eau profonde) */
	bool IsSafeSaveSpot(const ABRCharacter* P) const;
	/** Sauvegardes : reprend le journal d'une partie (entites rencontrees, niveaux visites), sans annonce */
	void RestoreJournal(const TArray<int32>& InDiscovered, const TArray<int32>& InVisited);
	const TArray<TObjectPtr<ABREntity>>& GetEntities() const { return Entities; }

	// ------------------------------------------------------------ v4.7 : directeur de tension
	/** Rythme de la partie : calme (exploration), malaise (signes inquietants), detection (une entite a remarque
	 *  quelqu'un), poursuite, retour au calme (repit garanti). Decide par le serveur, replique a tous */
	enum class ETension : uint8 { Calm, Unease, Detection, Chase, Recovery };
	ETension GetTension() const { return static_cast<ETension>(NetTension); }
	/** Secondes passees dans la phase actuelle */
	float GetTensionTime() const { return TensionTime; }
	static const TCHAR* TensionName(ETension T);
	/** Une nouvelle rencontre (entite qui apparait) est permise maintenant */
	bool AllowsNewEncounter() const;
	/** Phenomenes (bruits lointains, hallucinations) permis maintenant */
	bool AllowsPhenomena() const;
	/** v4.11 : budget de menace. Score : 1 par entite presente, +1 si elle poursuit, +2 pendant une coupure noire, +1 si un
	 *  joueur est dans une salle de fosses. Au-dela du budget (4, 5 a plus de deux joueurs), aucune nouvelle rencontre :
	 *  pas de coupure + Bacteria + plusieurs chasseurs + fosse au meme instant */
	int32 ThreatScore() const;
	int32 ThreatBudget() const;
	/** v4.11 : bruit du monde (mecanisme actionne, disjonction, porte) : les entites qui l'entendent viennent voir */
	void ReportNoise(const FVector& Location, float Radius);
	/** Bruit recent (moins de 4 s) entendu depuis From ; false sinon */
	bool FindRecentNoise(const FVector& From, FVector& OutLocation) const;
	/** v4.11 : une machine alimentee (relais, disjoncteur en marche, generateur, treuil, balise) a moins de Radius */
	bool IsNearActiveMachine(const FVector& Location, float Radius) const;

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
	/** v4.11 : documents facultatifs trouves dans ce niveau (cassettes VHS des nouvelles parties du Niveau 0) */
	int32 GetLoreFound() const { return LoreFound; }
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

	/** v4.7 : reverberation du lieu (moquette etouffee, beton, carrelage des Poolrooms) */
	UPROPERTY()
	TObjectPtr<UReverbEffect> LevelReverb;

	/** v4.7 : post-traitement des salles de fosses (le fond reste noir malgre le brouillard ordinaire) */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PitShadeMID;

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

	UPROPERTY(Replicated)
	uint8 NetTension = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Objectives)
	int32 VHSFound = 0;

	/** v4.11 : documents facultatifs (cassettes VHS des nouvelles parties) */
	UPROPERTY(Replicated)
	int32 LoreFound = 0;

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

	// ---- v4.11 : missions, campagne, depart de groupe
	UPROPERTY(ReplicatedUsing = OnRep_Mission)
	FBRNetMission NetMission;

	UPROPERTY(Replicated)
	FBRNetCampaign NetCampaign;

	UPROPERTY(ReplicatedUsing = OnRep_Departure)
	FBRNetDeparture NetDeparture;

	UFUNCTION()
	void OnRep_Mission();

	UFUNCTION()
	void OnRep_Departure();

	/** Fin de la campagne : ecran de fin chez tous */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastEnding(bool bVariant);

	UPROPERTY()
	TArray<TObjectPtr<ABRMissionDevice>> MissionDevices;

	/** Sorties garanties pres de la salle de mission (niveaux infinis) */
	UPROPERTY()
	TArray<TObjectPtr<ABRExit>> MissionExits;

	/** Tout le monde plonge dans le noir en meme temps ; le niveau suit quand le serveur l'a choisi */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastTransition(int32 TargetLevel, bool bFromDeath);

private:
	// ---- v4.11 : missions (BRMissionWorld.cpp)
	BRMission::FPlan MissionPlan;
	BRMission::FState MissionState;
	uint8 MissionGen = 0;
	/** Reprise : version et etat de mission de la session sauvegardee (consommes par SetupMission) */
	uint8 ResumeMissionGen = 0;
	TArray<uint8> ResumeMissionBlob;
	TSet<FIntPoint> MissionCells;
	/** Mecanisme -> emplacement ; sorties garanties (position, orientation, cible, style) */
	TArray<FBRMissionSpot> MissionSpots;
	struct FMissionExitSpot
	{
		FVector Pos = FVector::ZeroVector;
		float Yaw = 0.f;
		int32 Target = 0;
		EBRExitStyle Style = EBRExitStyle::Door;
		float Shaft = 0.f;
	};
	TArray<FMissionExitSpot> MissionExitSpots;
	/** Serveur : derniere unite maintenue acceptee (joueur, mecanisme) et rearmement apres une erreur */
	TMap<uint64, double> MissionHoldTimes;
	struct FNoiseEvent
	{
		FVector Location = FVector::ZeroVector;
		float Radius = 0.f;
		float Time = 0.f;
	};
	TArray<FNoiseEvent> Noises;
	TMap<int32, double> MissionCooldowns;
	float MissionDeviceTimer = 0.f;
	bool bMissionSolvedSeen = false;
	/** Construit le plan, place les mecanismes (cellules reservees avant la construction des chunks) et cree leurs acteurs.
	 *  Serveur : decide la version (reprise d'une session ancienne : ancien mode) et l'etat ; client : suit NetMission */
	void SetupMission();
	/** Place les mecanismes et les sorties garanties ; false si impossible (la variante de secours est alors essayee) */
	bool PlaceMission(const BRMission::FPlan& Plan, TArray<FBRMissionSpot>& OutSpots, TArray<FMissionExitSpot>& OutExits, TSet<FIntPoint>& OutCells) const;
	void SpawnMissionActors();
	void ClearMission();
	/** Serveur : publie l'etat (revision, dernier changement), donnees de campagne, journal */
	void PublishMission(int32 Device, uint8 Feedback, bool bAnimate = true);
	/** Etat change (hote ou client) : mecanismes, sons, messages, mission resolue */
	void OnMissionStateChanged(int32 Device, uint8 Feedback, bool bAnimate);
	void UpdateDeparture(float Dt);
	void CancelDeparture(uint8 Reason);
	/** Depart du groupe : transition vers Target, ou fin de la campagne */
	void LeaveForTarget(int32 Target);
	bool bEndingShown = false;
	bool bEndingVariant = false;
	/** La campagne de la sauvegarde de l'hote a ete lue (une fois par carte) */
	bool bCampaignLoaded = false;
	float DepartureReadyTime = 0.f;
	uint16 NextDepartureId = 0;
	TWeakObjectPtr<ABRCharacter> DepartureInitiator;
	uint8 SeenDeparturePhase = 0;
	uint16 SeenDepartureId = 0;

	/** v4.10 : Preparing : apres le fondu au noir, attente des ressources indispensables du niveau (ecran sobre, delai
	 *  maximal, retour au menu possible) avant sa construction */
	enum class ETrans : uint8 { None, FadingOut, Preparing, FadingIn };
	/** v4.10 : fin du fondu au noir (ou de la preparation) : construction du niveau prepare, puis arrivee */
	bool FinishFadeOut();
	/** v4.10 : signale (au serveur) que le joueur local est en chargement : les entites l'ignorent */
	void SetLocalLoading(bool bLoading);
	uint32 PrepSerial = 0;
	float PrepTime = 0.f;
	float PrepProgress = 0.f;
	double PrepStart = 0.0;
	/** Client qui rejoint la partie : preparation du niveau du groupe avant sa premiere construction */
	bool bJoinPreparing = false;
	/** Demarre la preparation du niveau Level (nouvelle demande : les precedentes ne seront jamais finalisees) */
	void StartPreparing(int32 Level);
	enum class EBlackout : uint8 { None, Failing, Dark, Restoring };

	void ClearLevel();
	/** v4.7 : aides des premieres minutes (deplacements, inventaire, camescope, se cacher), une fois par session */
	void UpdateFirstMinutes();
	int32 FirstMinutesStep = 0;
	/** v4.7 : directeur de tension (serveur) ; chez les clients, seul le temps passe dans la phase avance */
	void UpdateTension(float Dt);
	void SetTension(ETension NewTension);
	float TensionTime = 0.f;
	float TensionLength = 40.f;
	float TensionQuiet = 0.f;
	uint8 SeenTension = 0;
	/** v4.7 : fait avancer les chunks en preparation (budget par image), et termine sans attendre les collisions des
	 *  chunks sous les joueurs */
	void StepChunkBuilds();
	/** Un chunk vient d'etre termine : statistiques */
	void OnChunkReady(const ABRChunk* Chunk);
	/** Temps passe a construire des chunks pendant l'image en cours (ms) */
	float FrameChunkMs = 0.f;
	/** v4.8 : chunks sortis de la vue, demontes quelques composants par image */
	UPROPERTY()
	TArray<TObjectPtr<ABRChunk>> TearingDown;
	/** v4.8 : chunks dont les lumieres doivent changer de type (un chunk par image) */
	TArray<TWeakObjectPtr<ABRChunk>> LightRefreshQueue;
	float ShadowLODTimer = 0.f;
	int32 ShadowedLights = 0;
	/** v4.8 : arrivee dans un niveau : ecran noir tenu (8 s au plus) tant que des shaders se compilent ; modeles a
	 *  squelette montres une fois derriere le noir pour preparer leurs shaders (PSO, ray tracing) et leurs textures */
	float ShaderHold = 0.f;
	float PrewarmTime = -1.f;
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> PrewarmComps;
	void PrewarmModels();
	void EndPrewarm();
	/** v4.8 : positions des joueurs pour la generation : actuelle et predite (vitesse x anticipation) */
	void GetStreamingCenters(TArray<FVector>& OutNow, TArray<FVector>& OutPredicted) const;
	/** v4.8 : demonte les chunks en attente dans le budget restant de l'image */
	void StepTeardowns(double RemainingMs);
	/** v4.8 : ombres des lumieres selon la distance (toutes les 0,25 s) et changement de type des lumieres (budget) */
	void UpdateLightLOD(float Dt);
	void BeginTransition(int32 TargetLevel, bool bFromDeath);
	/** Client : suit le niveau du serveur. false tant qu'aucun niveau n'est construit */
	bool SyncNetLevel();
	void LoadNetLevel();
	void EnterBlackoutPhase(uint8 Phase, bool bSilent = false);
	void DestroyPickup(uint64 Id);
	void AnnounceVHS();
	/** v4.11 : serveur : effet d'un ramassage accepte sur le niveau (cassette du mode historique, document facultatif) */
	void OnServerPickupAccepted(EBRItem Item);
	void CompleteObjective(uint8 Which);
	ABRPlayerController* LocalPC() const;
	/** Un joueur vivant au hasard (point d'ancrage des apparitions) */
	ABRCharacter* RandomLivingPlayer() const;
	void ApplyEnvironment();
	void UpdateStreaming(bool bSynchronous);
	/** bNow : construction complete tout de suite (chargement d'un niveau) ; sinon etalee (StepChunkBuilds) */
	void SpawnChunk(const FIntPoint& Coord, bool bNow = false);
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
	/** v4.6 : salle de fosses de ce chunk, d'apres la graine seule (GetPitRoom lit le cache d'un niveau fini) */
	bool ComputePitRoom(const FIntPoint& Chunk, FIntRect& OutRoom) const;
	/** v4.6 : arete dans un chunk a salle de fosses : ouverte dans la salle et dans la galerie qui l'entoure, mur perce
	 *  de PitDoorsPerSide portes sur son pourtour (bOut = false : la regle ordinaire s'applique) */
	EBREdge PitEdge(int32 X, int32 Y, bool bEast, bool& bOut) const;
	/** v4.6 : niveau fini : salles de fosses et cellules atteignables (calcules au chargement, les memes chez tous) */
	void PrepareLevelLayout();
	/** v4.6 : serveur : un joueur passe sous le bord d'une fosse -> mort par le systeme existant (chez lui, via RPC) */
	void UpdatePitFalls();
	TMap<FIntPoint, FIntRect> BoundedPitRooms;
	TSet<FIntPoint> SafeReach;
	TSet<FIntPoint> FullReach;
	/** v4.6 : niveau fini : murs perces d'une porte pour relier au depart les zones que les murs tires enfermaient
	 *  (cle : (X * 2 + 1 pour une arete Est, Y)) */
	TSet<FIntPoint> ForcedDoors;
	/** Serveur : graine imposee a la prochaine transition (console BRSeed, mode developpeur) */
	uint32 PendingSeed = 0;
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
	/** v4.7 : reprise d'une session (point de reprise a appliquer au placement du joueur de l'hote) */
	bool bResumed = false;
	bool bHasResumeSpot = false;
	FVector ResumeSpot = FVector::ZeroVector;
	float ResumeYaw = 0.f;
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
	/** v4.11 : instant (temps du niveau) ou la coupure est devenue noire : validation des enregistrements */
	float DarkSince = -1.f;
	float BlackoutTimer = 90.f;
	/** v4.10 : coupure demandee (mode developpeur, tests) : commence meme pendant une poursuite ou le repit qui suit */
	bool bBlackoutForced = false;
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
