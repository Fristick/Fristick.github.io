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
// v4.9 : -BRAutoTestV49 : aucune jauge en exploration et deux dans l'onglet Personnage, Tab (onglet a l'ouverture,
// capture d'une touche, touche reaffectee, touches purgees), reglages d'interface (taille, opacite, reticule, objets
// rapides, objectifs), lignes des parametres, ray tracing indisponible, modes d'affichage (essai, retour, confirmation,
// expiration ; jeu lance seul), cadence des creatures, lumieres changees sur plusieurs images, prechargement par
// ensembles. Inclus dans -BRAutoTest ; -BRNetTest y ajoute le nom du coequipier cache par un mur.
// v4.11 : -BRAutoTestV411 : missions des douze niveaux par de vraies interactions, sorties, depart, fin, ancienne partie,
// ramassages, creatures (choix de cible, budget de menace, bruit), carnet, sous-titres, volumes. Inclus dans -BRAutoTest ;
// -BRNetTest y ajoute l'etat de mission a la connexion, l'action d'un client, le ramassage dispute, le soin perime et le
// depart de groupe.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "BRTypes.h"
#include "BRAutoTest.generated.h"

class ABRWorld;
class ABRCharacter;
class ABRPlayerController;
class ABREntity;
class FBRLogCapture;
class UBRSaveGame;

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
	/** v4.9 : interface, commandes, affichage, rendu et prechargement (BRAutoTestV49.cpp) */
	void AddV49Steps();
	/** v4.10 : polices, sauvegardes abimees, prechargement et preparation des niveaux, confirmation de l'affichage,
	 *  confort, memoire (BRAutoTestV410.cpp) */
	void AddV410Steps();
	/** v4.10 : multijoueur : soins rapproches, soin au moment d'un coup, preparation du client pendant un changement de niveau */
	void AddNetV410Steps(bool bClient);
	/** v4.10 : test de lancement d'un paquet (-BRSmokeTest) : carte, polices, langues, niveau construit ; rapport JSON */
	void AddSmokeSteps();
	/** v4.10 : session longue (-BRAutoTestSoak=<minutes>) : allers-retours entre le Niveau 0 et les 11 autres niveaux,
	 *  promenade a chaque arrivee ; memoire, objets, caches et images par seconde releves (SessionLongue.csv) */
	void AddSoakSteps(float Minutes);
	/** v4.9 : multijoueur : nom du coequipier a vue, puis cache par un mur (repere seulement s'il est a terre) */
	void AddNetV49Steps(bool bClient);
	/** v4.11 : missions des douze niveaux resolues par de vraies interactions, sorties verrouillees puis ouvertes, depart,
	 *  fin, ancienne partie (format 3) reprise en ancien mode, transactions de ramassage, choix de cible des creatures,
	 *  budget de menace, bruit, carnet et aide progressive, sous-titres, volumes separes (BRAutoTestV411.cpp) */
	void AddV411Steps();
	/** v4.11 : une mission : chargement (graine fixe), sortie verrouillee, solution par le solveur aux seules informations
	 *  visibles (BRMissionSolver), rejouee action par action (placement devant le mecanisme, validation de l'hote), sortie
	 *  ouverte ; bDepart : quitte ensuite le niveau par la sortie ouverte */
	void AddMissionSteps(int32 Level, uint32 UserSeed, bool bDepart);
	/** v4.11 : multijoueur : etat de mission a la connexion, action d'un client validee par l'hote, ramassage dispute (un
	 *  seul gagnant), soin perime refuse sans rien consommer, depart de groupe (attente, rassemblement, depart commun) */
	void AddNetV411Steps(bool bClient);
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
	/** v4.10 : verification qui n'a pas pu etre faite (ressource absente, plateforme) : jamais comptee comme une reussite */
	void Skip(const FString& Text);
	void Shot(const FString& Name);
	/** Un point degage, visible, a Dist cm devant le joueur (ou autour s'il n'y a pas de place) */
	bool FindSpotInFront(float Dist, float Z, FVector& Out) const;
	void CollectStats(FLevelReport& R) const;
	void WriteReport();
	/** v4.10 : valeur d'une option de chemin (-Nom=...), espaces compris */
	static bool ParsePathOption(const TCHAR* Name, FString& Out);
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

	// v4.9 : etat des etapes v4.9
	/** Touches de l'inventaire avant le test (retablies) */
	TArray<FKey> TestKeys;
	/** Affichage confirme avant le test (fenetre, resolution) ; le jeu peut changer la fenetre (pas dans l'editeur) */
	int32 TestWindow = 1;
	FIntPoint TestResolution = FIntPoint::ZeroValue;
	bool bDisplayTestable = false;
	/** Ensembles precharges au premier passage au Niveau 0 */
	TArray<FString> TestSets;
	/** Lampe allumee avant le test des piles ; type des lumieres avant le test d'etalement */
	bool bTestFlashlight = false;
	bool bTestAreaLights = false;
	/** Orientation du joueur avant un demi-tour */
	FRotator TestRotation = FRotator::ZeroRotator;
	/** Poche modifiee pour montrer les objets rapides (retablie) */
	FBRItemSlot TestPocket;
	/** Reseau : une place derriere un mur a ete trouvee pres du coequipier */
	bool bMateBehindWall = false;

	// v4.11 : etat des etapes v4.11
	/** Solution enregistree (triplets mecanisme, action, retour attendu) et avancement du rejeu */
	TArray<uint8> TestMission;
	int32 TestMissionStep = 0;
	int32 TestMissionPhase = 0;
	int32 TestMissionDevice = -1;
	double TestMissionClock = 0.0;
	double TestMissionLastHold = 0.0;
	int32 TestMissionRetries = 0;
	/** Resultat par niveau (rapport) */
	TArray<FString> TestMissionLines;
	/** Entites creees par un test (detruites a la fin du test) */
	TArray<TWeakObjectPtr<ABREntity>> TestSpawned;
	/** Compteurs avant une etape (ramassages, soins, retours) */
	int32 TestCountA = 0;
	int32 TestCountB = 0;
	int32 TestCountC = 0;
	uint64 TestPickup = 0;
	FVector TestExitSpot = FVector::ZeroVector;
	/** Partie active et mode de session avant le test de l'ancienne partie (retablis ensuite) */
	int32 TestOrigSlot = INDEX_NONE;
	bool bTestDevSession = true;
	UPROPERTY()
	TObjectPtr<UBRSaveGame> TestOrigSave = nullptr;

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
	/** v4.10 : verifications non faites (rapport : "NON VERIFIE") */
	TArray<FString> Skipped;
	/** v4.10 : session longue : releves (minute, niveau, RAM, textures, objets, caches) */
	TArray<FString> SoakLines;
	double SoakEnd = 0.0;
	int32 SoakRound = 0;
	float SoakRamFirst = 0.f;
	float SoakRamMax = 0.f;
	/** v4.10 : ensembles du Niveau 37 absents du Niveau 0 (verifies apres le retour : plus en memoire) */
	TArray<FString> TestOnlySets;
	/** v4.10 : soin : objets et sante avant la demande */
	int32 TestBandages = 0;
	int32 TestHeals = 0;
	/** v4.10 : chronometre d'une etape (transition, preparation bloquee, session longue) ; ecran de preparation vu ; niveau vise */
	double TestTimer = 0.0;
	bool bTestPrepShown = false;
	int32 TestLevel = 0;
	/** v4.10 : ecran de la fenetre au debut des verifications v4.10 */
	int32 TestWindowMonitor = INDEX_NONE;
	/** v4.10 : affichage avant le deplacement sur un autre ecran (retabli et confirme a la fin, sur l'ecran d'origine) */
	int32 TestOrigWindow = 1;
	FIntPoint TestOrigResolution = FIntPoint::ZeroValue;
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
