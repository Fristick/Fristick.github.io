// Test automatique du jeu : lancer le jeu avec -BRAutoTest (eventuellement -BRAutoTestLevels=0,37).
// Il parcourt chaque niveau, mesure les images par seconde, fait apparaitre les entites du niveau, teste la coupure
// de courant, la lampe, la vision nocturne, la nage, l'inventaire, la 3e personne, la mort et le reveil, prend des
// captures (Saved/AutoTest/*.png) et ecrit un rapport (Saved/AutoTest/Rapport.txt) avec les avertissements et
// erreurs du journal. Le jeu se ferme a la fin.
// v4.6 : salle de fosses du Niveau 0 (point de vue, mesures par profil, Lumen logiciel, lampe, IA, chute et reveil) :
// incluse quand le Niveau 0 est teste ; -BRAutoTestPits : ce scenario seul. Graine : -BRSeed=<n>, sinon celle de demonstration.
// Test multijoueur : -BRNetTest sur un hote (carte ouverte avec "?listen") et sur un client qui le rejoint
// (rapports dans Saved/NetTest_Hote et Saved/NetTest_Client).
// v4.8 : -BRAutoTestV48 : combinaison (materiaux par section), profils graphiques appliques a ce qui est deja charge, profil
// RTX fluide (mesures), 22 langues (traductions, formats, coupure des lignes, preference), notes des anciennes sauvegardes,
// partie d'un format plus recent, echecs d'ecriture. Inclus dans -BRAutoTest ; -BRNetTest y ajoute degats decides par le
// serveur, reveil premature refuse et langue differente sur chaque machine.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BRTypes.h"
#include "BRAutoTest.generated.h"

class ABRWorld;
class ABRCharacter;
class ABRPlayerController;
class ABREntity;
class FBRLogCapture;

UCLASS()
class BACKROOMS_API ABRAutoTest : public AActor
{
	GENERATED_BODY()

public:
	ABRAutoTest();

	/** -BRAutoTest (ou -BRNetTest) sur la ligne de commande */
	static bool IsRequested();
	/** -BRNetTest : test multijoueur (un hote et un client lances sur la meme machine) */
	static bool IsNetTestRequested();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	/** Une etape du plan : une action (false = condition pas encore remplie, on reessaie), puis une attente */
	struct FStep
	{
		FString Name;
		TFunction<bool()> Action;
		float Wait = 0.f;
	};

	/** Mesures d'un niveau, pour le rapport */
	struct FLevelReport
	{
		int32 Level = 0;
		FString Title;
		float AvgFPS = 0.f;
		float WorstMs = 0.f;
		int32 Chunks = 0;
		int32 Lights = 0;
		int32 ShadowLights = 0;
		int32 Pickups = 0;
		int32 Exits = 0;
		int32 Entities = 0;
		// v4.5 : fluidite (percentiles des temps d'image), memoire, construction des chunks
		float P50Ms = 0.f;
		float P95Ms = 0.f;
		float P99Ms = 0.f;
		float Low1FPS = 0.f;
		float GpuMs = 0.f;
		float GameMs = 0.f;
		float RenderMs = 0.f;
		float RamMB = 0.f;
		float TexMB = 0.f;
		float ChunkMaxMs = 0.f;
		float ChunkAvgMs = 0.f;
		/** v4.8 : images ou la generation a depasse son budget, pire demontage d'un chunk, chargements synchrones en jeu */
		int32 FramesOverBudget = 0;
		float MaxTeardownMs = 0.f;
		int32 SyncLoads = 0;
		/** v4.6 : scene mesuree dans le niveau ("vue" : point de depart ; "fosses_Qualite"... : salle de fosses par profil) */
		FString Scene = TEXT("vue");
		TArray<FString> Notes;
		int32 FirstLogLine = 0;
		int32 LastLogLine = 0;
	};

	void BuildPlan(const TArray<int32>& Levels);
	void BuildNetPlan();
	/** Multijoueur : niveau, graine, joueurs, chunks, entites et position du personnage (dans le rapport) */
	void NoteNetState(const TCHAR* When);
	void Add(const FString& Name, float Wait, TFunction<bool()> Action);
	void AddLevelSteps(int32 Level);
	void AddEntityShot(int32 Level, EBREntityKind Kind);
	/** v4.6 : scenario de la salle de fosses */
	void AddPitSteps();
	/** v4.7 : non-regression (mort et reanimation, sauvegardes et reprise, attaque en trois temps, streaming, graines) */
	void AddRegressionSteps();
	/** v4.7 : charge un niveau avec une graine choisie (meme calcul que -BRSeed) et attend qu'il soit pret */
	void AddSeedLoad(int32 Level, uint32 UserSeed, const FString& Title);
	/** Lignes du journal deja recopiees (debut du rapport d'une scene) */
	int32 LogLineCount() const;
	/** v4.7 : multijoueur : mort d'un coequipier (blessure), reanimation par l'hote, etats sur les deux machines */
	void AddNetDeathSteps(bool bClient);
	/** v4.8 : combinaison, profils, RTX fluide, langues, notes, sauvegardes (BRAutoTestV48.cpp) */
	void AddV48Steps();
	/** v4.8 : multijoueur : langue propre a chaque machine, coup decide par le serveur (sans double application), reveil
	 *  premature refuse par le serveur, reanimation */
	void AddNetV48Steps(bool bClient);
	/** v4.8 : une ligne du journal capture (avertissements, erreurs) depuis From contient Needle */
	bool LogContains(int32 From, const TCHAR* Needle) const;
	/** Mesure des temps d'image : debut, puis fin (moyenne, percentiles, memoire, chunks, mode de rendu dans le rapport R) */
	void StartMeasure();
	void EndMeasure(FLevelReport& R);
	/** Charge un niveau et attend qu'il soit pret (titre efface, exposition stabilisee) */
	void AddLoad(int32 Level, float Settle, const FString& Title = FString());

	ABRWorld* GetBRWorld() const;
	ABRCharacter* GetPlayer() const;
	ABRPlayerController* GetPC() const;
	FLevelReport& Report();
	void Note(const FString& Text, bool bProblem = false);
	void Shot(const FString& Name);
	/** Un point degage, visible, a Dist cm devant le joueur (ou autour s'il n'y a pas de place) */
	bool FindSpotInFront(float Dist, float Z, FVector& Out) const;
	void CollectStats(FLevelReport& R) const;
	void WriteReport();
	void Finish();

	// v4.7 : etat des etapes de non-regression
	uint64 TestPickupId = 0;
	uint32 TestSeed = 0;
	int32 TestVHS = 0;
	FVector TestSpot = FVector::ZeroVector;
	FVector TestStart = FVector::ZeroVector;
	float TestHealth = 100.f;
	int32 TestHits = 0;
	int32 TestMisses = 0;
	int32 TestWindups = 0;
	TWeakObjectPtr<ABREntity> TestEntity;
	bool bSprintStarted = false;
	float SprintT0 = 0.f;
	int32 SprintHoles = 0;
	int32 SprintFrames = 0;
	int32 SprintMaxPreparing = 0;
	/** v4.7 : test reseau : coequipier mis a terre, temps de la demande */
	TWeakObjectPtr<ABRCharacter> NetMate;

	// v4.8 : etat des etapes v4.8
	/** Langue courante et preference du joueur avant le test (retablies a la fin) */
	FString TestLanguage;
	FString TestLanguagePref;
	bool bLanguageSaved = false;
	/** Le menu principal etait ouvert avant les captures de la page Langue */
	bool bWasInMenu = false;
	/** Premiere ligne du journal a examiner (refus attendu) */
	int32 TestFirstLog = 0;
	/** Reseau : sante du coequipier vue par le serveur juste apres le coup ; reveil premature demande */
	float TestServerHealth = 0.f;
	bool bRespawnRequested = false;
	/** Contenu d'un fichier avant une ecriture refusee (il doit rester identique) */
	TArray<uint8> TestBytes;

	TArray<FStep> Plan;
	int32 StepIndex = 0;
	float StepTime = 0.f;
	float WaitLeft = 0.f;
	bool bActionDone = false;
	bool bFinished = false;
	bool bNetTest = false;

	// Mesure des images par seconde
	bool bMeasuring = false;
	int32 Frames = 0;
	float MeasureTime = 0.f;
	float Worst = 0.f;
	/** Temps moyens par image pendant la mesure (ms) : processeur graphique, thread du jeu, thread de rendu */
	double GpuMs = 0.0;
	double GameMs = 0.0;
	double RenderMs = 0.0;
	/** v4.5 : chaque temps d'image de la mesure (percentiles, 1 % le plus lent) */
	TArray<float> FrameMs;

	TArray<FLevelReport> Reports;
	TArray<FString> Problems;
	TWeakObjectPtr<ABREntity> LastEntity;
	/** Marche automatique (test du sillage dans l'eau) */
	float WalkTime = 0.f;
	/** Multijoueur : plus grand nombre d'entites vues au Niveau 0 (le client peut suivre l'hote au Niveau 37 avant sa verification) */
	int32 NetEntitiesSeen = 0;
	float WalkYaw = 0.f;
	FVector WalkStart = FVector::ZeroVector;
	FString OutDir;
	double StartTime = 0.0;
	TSharedPtr<FBRLogCapture> Capture;

	// v4.6 : salle de fosses
	/** Reglages du joueur avant le scenario (les profils sont essayes puis on revient a ceux-ci) */
	FBRSettings SavedSettings;
	bool bSettingsSaved = false;
	/** Entite qui traverse la salle : point le plus bas, images passees au-dessus d'une fosse, plus courte distance au joueur */
	TWeakObjectPtr<ABREntity> PitEntity;
	bool bPitTrack = false;
	float PitMinZ = 0.f;
	int32 PitOverFrames = 0;
	float PitMinDist = 0.f;
	float PitStartDist = 0.f;
	/** Chute : instant ou le joueur commence a marcher vers la fosse */
	double FallStart = 0.0;
};
