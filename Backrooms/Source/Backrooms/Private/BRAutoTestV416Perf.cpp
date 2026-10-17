// v4.16 (perf) : mesures de performance du jeu (-BRAutoTest -BRAutoTestPerf ; -BRAutoTestV416Perf est accepte aussi).
// Premiere partie de la section 7 : des scenes fixes et reproductibles, mesurees de la meme facon a chaque version, sans
// aucune optimisation. Les resultats d'un lancement se comparent a ceux d'un autre (Tools/Tests/run_perf.ps1 lance N
// executions x graines x profils et archive chacune ; Tools/Tests/perf_summary.py les agrege et compare deux etiquettes).
//
// Scenes (graines fixes) :
//   fosses  : Level 0, graine 9 (la scene des fosses, -BRSeed=9 de -BRAutoTestPits), point de vue de la commande BRPits ;
//   bassins : Poolrooms (Niveau 37), graine du niveau 178662963 (celle de la mesure RTX fluide de V48), point de depart ;
//   marche0 : Level 0, graine 9 : marche a la vitesse de course (4,7 m/s) sur un chemin de grille fixe (A* du monde, cellules
//             praticables, ni bassin ni salle de fosses au but) du depart vers le coin le plus eloigne, aller et retour ;
//   marche1 : Niveau 1 (niveau infini), graine 4242 : meme marche vers une cellule a 40 cellules : chunks construits et demontes ;
//   lidar6  : Level 6, graine 4136 : scan focalise maintenu et impulsions des qu'elles sont pretes, rotation de 30 degres/s,
//             regard a -8 degres, apres 6 s de pre-remplissage (points deja en place : regime etabli).
//   -BRPerfScenes=fosses,bassins,marche0,marche1,lidar6 : celles-la seules ; -BRPerfSeed=<n> : graine utilisateur n (meme
//   calcul que -BRSeed) pour toutes les scenes (plusieurs graines : un lancement par graine).
// Profils : Performance, Qualite, RTX fluide, Cinematique, appliques AVANT le chargement de chaque scene (lumieres creees du
//   bon type, meme etat de depart, comme le banc visuel) ; -BRProfile=<0|1|2|4> : ce profil seul. Reglages d'apparence fixes
//   (valeurs par defaut : champ de vision, VHS, grain, luminosite, flou), synchro verticale coupee, images non limitees ;
//   tout est retabli a la fin (BackroomsPlayer.ini n'est pas ecrit ; ApplySettings reecrit GameUserSettings.ini, synchro et
//   limite d'images, avec les valeurs d'origine au bilan : un arret force en cours de suite les laisserait coupees).
// Pour chaque scene x profil :
//   1. chargement (duree, attente des shaders derriere l'ecran noir de l'arrivee) ;
//   2. prechauffage : au moins 8 s (-BRPerfWarm=<s> ; titre du niveau efface, exposition posee), aucun shader ni PSO en
//      compilation depuis 1 s (shaders de l'editeur, precompilation du cache de PSO et precache des PSO a l'execution :
//      PerfShaderJobs), aucun chunk en preparation, temps d'image stabilises (BRPerf::IsSettled : trois fenetres de 30
//      images, medianes a 3 % ou 0,3 ms, dispersion faible, aucune saccade) ; 45 s au plus, sinon la mesure est faite et
//      marquee instable (NON VERIFIE) ;
//   3. fenetre de mesure fixe : 10 s (marches 20 s, LiDAR 12 s ; -BRPerfSeconds=<s> pour toutes) : temps de chaque image,
//      threads du jeu, du rendu et du RHI, processeur graphique ; memoire video (budget et usage du pilote, textures,
//      tampons), RAM ; chunks construits et demontes (ms), composants ajoutes aux chunks, chargements synchrones, PSO crees
//      a la demande et saccades de creation de PSO ; LiDAR : rayons, visees, points, instances, inscriptions et materiaux par
//      seconde, temps des traces et de l'envoi des instances ;
//   4. hors fenetre : 3 releves ProfileGPU (-BRPerfGpu=<k>, 0 : aucun) lus dans le journal (lignes compactes,
//      r.ProfileGPU.TableFormatting 0, fenetre du profileur coupee, evenements sans dessin affiches : la construction des
//      structures du ray tracing n'a ni dessin ni dispatch), ramenes a des postes (BRPerfLogic.h) ; mediane des releves.
//      File graphique dans Perf.csv ; files de calcul asynchrone a part (PerfPasses.csv, type poste_async : elles se
//      recouvrent avec la file graphique, leurs temps ne s'ajoutent pas a l'image). La marche et le scan continuent pendant
//      les releves.
// Fichiers (Saved/AutoTest) : Perf.csv (une ligne par scene x profil, toujours avec fenetre, resolution interne, profil,
//   ray tracing, MegaLights, hit lighting, carte, pilote, processeur), PerfPasses.csv (postes et passes du processeur
//   graphique), PerfGPU/<scene>_<profil>_<k>.txt (releves bruts), PerfFrames/<scene>_<profil>.csv (temps de chaque image),
//   plus une ligne perf_<scene>_<profil> dans Mesures.csv et Rapport.txt. Etiquettes : -BRPerfTag=<texte>, -BRPerfRun=<n>.
// Cout : pendant la fenetre, rien n'est ecrit ni journalise ; par image, la lecture de compteurs du moteur et le nombre de
//   composants des chunks (quelques dizaines d'acteurs) ; memoire lue toutes les 0,5 s. Les releves ProfileGPU et le
//   decompte des lumieres et des composants du monde sont faits apres la fenetre.
// Un prealable absent (niveau non charge, pas de salle de fosses, pas de chemin, scanner absent, ProfileGPU sans resultat,
// ray tracing materiel absent) est note NON VERIFIE, jamais compte comme une mesure valide. Aucun seuil d'images par
// seconde n'est juge ici : ce sont des mesures, pas des verifications.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRChunk.h"
#include "BREntity.h"
#include "BRLevels.h"
#include "BRLidar.h"
#include "BRPerfLogic.h"
#include "BRPlayerController.h"
#include "BRWorld.h"

#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "PipelineStateCache.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "RHIStats.h"
#include "RenderTimer.h"
#include "UnrealClient.h"
#include "UObject/ObjectKey.h"

namespace BRTestPerf
{
	enum class EPerfKind : uint8
	{
		Pits,
		Spawn,
		Walk,
		Lidar
	};

	struct FPerfScene
	{
		const TCHAR* Id;
		int32 Level;
		/** Graine utilisateur (meme calcul que -BRSeed) ; 0 : LevelSeed est la graine du niveau elle-meme */
		uint32 UserSeed;
		uint32 LevelSeed;
		EPerfKind Kind;
		/** Duree de la fenetre de mesure (s) */
		float Seconds;
	};

	const FPerfScene PerfScenes[] = {
		{ TEXT("fosses"), 0, 9u, 0u, EPerfKind::Pits, 10.f },
		{ TEXT("bassins"), 37, 0u, 178662963u, EPerfKind::Spawn, 10.f },
		{ TEXT("marche0"), 0, 9u, 0u, EPerfKind::Walk, 20.f },
		{ TEXT("marche1"), 1, 4242u, 0u, EPerfKind::Walk, 20.f },
		{ TEXT("lidar6"), 6, 4136u, 0u, EPerfKind::Lidar, 12.f },
	};

	struct FPerfProfile
	{
		int32 Index;
		const TCHAR* Name;
	};
	const FPerfProfile PerfProfiles[] = { { 0, TEXT("Performance") }, { 1, TEXT("Qualite") }, { 4, TEXT("RTXFluide") }, { 2, TEXT("Cinematique") } };

	/** Version des colonnes de Perf.csv et PerfPasses.csv (Tools/Tests/perf_summary.py) */
	constexpr int32 PerfFormat = 1;
	constexpr float PerfLoadTimeout = 120.f;
	constexpr float PerfWarmTimeout = 45.f;
	constexpr float PerfShaderQuiet = 1.f;
	constexpr float PerfLidarPrefill = 6.f;
	constexpr float PerfLidarPrefillTimeout = 30.f;
	constexpr float PerfLidarYawRate = 30.f;
	constexpr float PerfLidarPitch = -8.f;
	/** Vitesse de course du personnage (ABRCharacter, SprintSpeed) ; regard 1,5 m devant sur le chemin */
	constexpr float PerfWalkSpeed = 470.f;
	constexpr float PerfWalkLookAhead = 150.f;
	/** Marche du Niveau 1 (infini) : but a 40 cellules ; chemin d'au moins 12 cellules */
	constexpr int32 PerfWalkFarCells = 40;
	constexpr int32 PerfWalkMinCells = 12;
	constexpr float PerfMemoryEvery = 0.5f;
	constexpr float PerfGpuTimeout = 5.f;
	constexpr float PerfGpuQuiet = 0.3f;
	/** Profondeur des chemins de PerfPasses.csv (<root>/Scene/... : les passes de Lumen ou de MegaLights sont a 3 ou 4) */
	constexpr int32 PerfGpuMaxDepth = 5;
	constexpr double PerfGpuMinMs = 0.05;
	/** Graine fixe des tirages du jeu (FMath::FRand) au debut du prechauffage et de la fenetre */
	constexpr int32 PerfRandSeed = 416416;

	struct FPerfOptions
	{
		TArray<int32> SceneIndices;
		TArray<FPerfProfile> Profiles;
		float Seconds = 0.f;
		float WarmSeconds = 8.f;
		int32 GpuCaptures = 3;
		uint32 Seed = 0;
		int32 Run = 0;
		FString Tag;
		bool bFrames = true;
	};
	FPerfOptions PerfOptions;

	FPerfOptions ReadPerfOptions()
	{
		FPerfOptions Out;
		const TCHAR* Cmd = FCommandLine::Get();
		FString SceneList;
		if (FParse::Value(Cmd, TEXT("BRPerfScenes="), SceneList, false))
		{
			TArray<FString> Parts;
			SceneList.ParseIntoArray(Parts, TEXT(","), true);
			for (const FString& Part : Parts)
			{
				for (int32 I = 0; I < static_cast<int32>(UE_ARRAY_COUNT(PerfScenes)); ++I)
				{
					if (Part.TrimStartAndEnd().Equals(PerfScenes[I].Id, ESearchCase::IgnoreCase))
					{
						Out.SceneIndices.AddUnique(I);
					}
				}
			}
		}
		if (Out.SceneIndices.Num() == 0)
		{
			for (int32 I = 0; I < static_cast<int32>(UE_ARRAY_COUNT(PerfScenes)); ++I)
			{
				Out.SceneIndices.Add(I);
			}
		}
		int32 Forced = -1;
		if (FParse::Value(Cmd, TEXT("BRProfile="), Forced))
		{
			for (const FPerfProfile& P : PerfProfiles)
			{
				if (P.Index == Forced)
				{
					Out.Profiles.Add(P);
				}
			}
		}
		if (Out.Profiles.Num() == 0)
		{
			for (const FPerfProfile& P : PerfProfiles)
			{
				Out.Profiles.Add(P);
			}
		}
		float Seconds = 0.f;
		if (FParse::Value(Cmd, TEXT("BRPerfSeconds="), Seconds) && Seconds > 0.f)
		{
			Out.Seconds = FMath::Clamp(Seconds, 2.f, 120.f);
		}
		float Warm = 0.f;
		if (FParse::Value(Cmd, TEXT("BRPerfWarm="), Warm) && Warm > 0.f)
		{
			Out.WarmSeconds = FMath::Clamp(Warm, 2.f, 40.f);
		}
		int32 Gpu = -1;
		if (FParse::Value(Cmd, TEXT("BRPerfGpu="), Gpu) && Gpu >= 0)
		{
			Out.GpuCaptures = FMath::Clamp(Gpu, 0, 9);
		}
		uint32 Seed = 0;
		if (FParse::Value(Cmd, TEXT("BRPerfSeed="), Seed))
		{
			Out.Seed = Seed;
		}
		FParse::Value(Cmd, TEXT("BRPerfRun="), Out.Run);
		FString Tag;
		if (FParse::Value(Cmd, TEXT("BRPerfTag="), Tag))
		{
			for (const TCHAR Ch : Tag)
			{
				Out.Tag.AppendChar(FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-') || Ch == TEXT('.') ? Ch : TEXT('_'));
			}
		}
		int32 FramesFlag = 1;
		if (FParse::Value(Cmd, TEXT("BRPerfFrames="), FramesFlag))
		{
			Out.bFrames = FramesFlag != 0;
		}
		return Out;
	}

	/** Shaders et PSO en compilation : shaders de l'editeur et precompilation du cache de PSO (ABRWorld::ShadersInFlight),
	 *  plus les PSO precaches a l'execution (materiaux d'un chunk qui arrive) */
	int32 PerfShaderJobs()
	{
		return ABRWorld::ShadersInFlight() + static_cast<int32>(PipelineStateCache::NumActivePrecacheRequests());
	}

	/** PSO crees a la demande du rendu et creations trop longues (saccades), depuis le lancement */
	struct FPerfPsoCount
	{
		int64 Created = 0;
		int64 Hitches = 0;
	};
	FPerfPsoCount PerfPsoNow()
	{
		const FPSORuntimeCreationStats PsoStats = PipelineStateCache::GetPSORuntimeCreationStats();
		FPerfPsoCount Out;
		Out.Created = PsoStats.TotalPSOCreations;
		Out.Hitches = static_cast<int64>(PsoStats.GraphicsPSOHitches) + static_cast<int64>(PsoStats.ComputePSOHitches);
		return Out;
	}

	/** Un releve ProfileGPU, ramene a des postes et a des chemins de passes */
	struct FPerfGpuCapture
	{
		double FrameMs = 0.0;
		double RootMs = 0.0;
		double WaitMs = 0.0;
		double OtherQueuesMs = 0.0;
		double CatMs[BRPerf::NumCategories] = {};
		/** Files de calcul asynchrone et de copie (recouvrent la file graphique) */
		double AsyncCatMs[BRPerf::NumCategories] = {};
		TArray<FString> PathOrder;
		TMap<FString, double> PathMs;
		TMap<FString, int32> PathDepth;
	};

	/** Etat de la mesure en cours (une scene x un profil a la fois) */
	struct FPerfState
	{
		int32 SceneIndex = 0;
		FPerfProfile Profile = { 1, TEXT("Qualite") };
		uint32 UserSeed = 0;
		uint32 LevelSeed = 0;
		bool bValid = true;
		FString Why;
		TArray<FString> RowNotes;
		// Chargement
		double LoadStart = 0.0;
		float LoadSeconds = 0.f;
		float ShaderHoldMax = 0.f;
		// Pose et mouvements
		FVector PoseLoc = FVector::ZeroVector;
		FRotator PoseRot = FRotator::ZeroRotator;
		TArray<FVector> Path;
		TArray<float> PathCum;
		float PathLength = 0.f;
		float WalkYawLast = 0.f;
		double ScanStart = 0.0;
		// Prechauffage
		double WarmStart = 0.0;
		TArray<float> WarmFrames;
		double ShaderQuietSince = -1.0;
		int32 WarmShaderMax = 0;
		bool bStable = false;
		float WarmSeconds = 0.f;
		double PrefillStart = 0.0;
		// Fenetre
		float Window = 10.f;
		double T0 = 0.0;
		double Duration = 0.0;
		TArray<float> FrameTimes;
		TArray<float> GameTimes;
		TArray<float> RenderTimes;
		TArray<float> RhiTimes;
		TArray<float> GpuTimes;
		int32 ShaderFrames = 0;
		int32 ShaderMax = 0;
		int32 EntitiesMax = 0;
		int32 InactiveFrames = 0;
		double NextMemory = 0.0;
		double VramUsedMax = 0.0;
		double RamMax = 0.0;
		TMap<FObjectKey, int32> ChunkComponents;
		int64 ComponentsAdded = 0;
		int32 ChunksAppeared = 0;
		int32 SyncBase = 0;
		int32 SyncNamesBase = 0;
		FPerfPsoCount PsoBase;
		FPerfPsoCount PsoWindow;
		int32 LidarPeak = 0;
		FVector WalkFrom = FVector::ZeroVector;
		double WalkMeters = 0.0;
		// Releves a la fin de la fenetre
		int32 ChunksBuilt = 0;
		float ChunkBuildMs = 0.f;
		float ChunkWorstFrameMs = 0.f;
		int32 FramesOverBudget = 0;
		int32 ForcedBuilds = 0;
		int32 ChunksTornDown = 0;
		float TeardownMs = 0.f;
		float TeardownWorstMs = 0.f;
		int32 SyncLoads = 0;
		FString SyncNames;
		FBRLidarStats Lidar;
		int32 LidarEnd = 0;
		FRHIMemoryStats Memory;
		FTextureMemoryStats Textures;
		double BufferBytes = 0.0;
		double UniformBytes = 0.0;
		double RamEnd = 0.0;
		int32 ChunksEnd = 0;
		int32 PreparingEnd = 0;
		bool bForegroundEnd = true;
		// Releves ProfileGPU
		int32 GpuPhase = 0;
		int32 GpuWaitFrames = 0;
		double GpuIssued = 0.0;
		TArray<FPerfGpuCapture> GpuCaptures;
		int32 GpuFailed = 0;
	};
	FPerfState PerfState;

	/** Fichiers ecrits (lignes) */
	TArray<FString> PerfHeader;
	TArray<FString> PerfLines;
	TArray<FString> PassLines;
	int32 PerfMeasured = 0;
	int32 PerfUnstable = 0;
	int32 PerfInvalid = 0;
	int32 PerfGpuDumps = 0;
	double PerfSuiteStart = 0.0;

	/** Lignes du journal de la categorie LogRHI (releve ProfileGPU) ; jamais libere : peut rester inscrit jusqu'a l'arret */
	class FPerfRhiLog final : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type /*Verbosity*/, const FName& Category) override
		{
			static const FName RhiCategory(TEXT("LogRHI"));
			if (!V || Category != RhiCategory)
			{
				return;
			}
			FScopeLock Lock(&Mutex);
			if (Lines.Num() < 50000)
			{
				Lines.Add(V);
			}
			LastLineTime = FPlatformTime::Seconds();
		}
		virtual bool CanBeUsedOnAnyThread() const override
		{
			return true;
		}
		void Reset()
		{
			FScopeLock Lock(&Mutex);
			Lines.Reset();
			LastLineTime = 0.0;
		}
		void Take(TArray<FString>& Out, double& OutLast)
		{
			FScopeLock Lock(&Mutex);
			Out = Lines;
			OutLast = LastLineTime;
		}

	private:
		FCriticalSection Mutex;
		TArray<FString> Lines;
		double LastLineTime = 0.0;
	};
	FPerfRhiLog* PerfRhiLog = nullptr;
	bool bPerfRhiLogAttached = false;

	void AttachRhiLog()
	{
		if (!PerfRhiLog)
		{
			PerfRhiLog = new FPerfRhiLog();
		}
		if (!bPerfRhiLogAttached && GLog)
		{
			GLog->AddOutputDevice(PerfRhiLog);
			bPerfRhiLogAttached = true;
		}
		PerfRhiLog->Reset();
	}

	void DetachRhiLog()
	{
		if (bPerfRhiLogAttached && GLog && PerfRhiLog)
		{
			GLog->RemoveOutputDevice(PerfRhiLog);
		}
		bPerfRhiLogAttached = false;
	}

	/** Variables du profileur changees pendant la suite (retablies au bilan) */
	struct FPerfSavedCVar
	{
		FString Name;
		FString Value;
	};
	TArray<FPerfSavedCVar> PerfSavedCVars;

	bool SetPerfCVar(const TCHAR* Name, const TCHAR* Value)
	{
		IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
		if (!Var)
		{
			return false;
		}
		PerfSavedCVars.Add({ FString(Name), Var->GetString() });
		Var->Set(Value, ECVF_SetByConsole);
		return true;
	}

	void RestorePerfCVars()
	{
		for (int32 I = PerfSavedCVars.Num() - 1; I >= 0; --I)
		{
			if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(*PerfSavedCVars[I].Name))
			{
				Var->Set(*PerfSavedCVars[I].Value, ECVF_SetByConsole);
			}
		}
		PerfSavedCVars.Reset();
	}

	float CVarValue(const TCHAR* Name, float Default)
	{
		const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
		return Var ? Var->GetFloat() : Default;
	}

	double ToMB(double Bytes)
	{
		return Bytes / (1024.0 * 1024.0);
	}

	FIntPoint ViewportSize()
	{
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			return GEngine->GameViewport->Viewport->GetSizeXY();
		}
		return FIntPoint(0, 0);
	}

	/** Profil puis reglages fixes (valeurs par defaut de l'apparence, pas de synchro ni de limite d'images) */
	void ApplyPerfProfile(ABRPlayerController* PC, int32 ProfileIndex)
	{
		ABRPlayerController::ApplyGraphicsProfile(ProfileIndex);
		FBRSettings& Live = FBRSettings::Get();
		const FBRSettings Stock = FBRSettings();
		Live.FOV = Stock.FOV;
		Live.bVHSEffect = Stock.bVHSEffect;
		Live.bFilmGrain = Stock.bFilmGrain;
		Live.Brightness = Stock.Brightness;
		Live.bMotionBlur = Stock.bMotionBlur;
		Live.Flashes = Stock.Flashes;
		Live.bHeadBob = Stock.bHeadBob;
		Live.CameraShake = Stock.CameraShake;
		Live.UiScale = Stock.UiScale;
		Live.HudOpacity = Stock.HudOpacity;
		Live.CrosshairMode = Stock.CrosshairMode;
		Live.QuickBarMode = Stock.QuickBarMode;
		Live.ObjectivesMode = Stock.ObjectivesMode;
		Live.bSubtitles = Stock.bSubtitles;
		Live.bVSync = false;
		Live.MaxFPS = 0;
		if (PC)
		{
			PC->ApplySettings();
		}
	}

	void PlacePlayer(ABRCharacter* C, ABRPlayerController* PC, const FVector& Loc, const FRotator& Rot)
	{
		C->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
		PC->SetControlRotation(Rot);
	}

	/** Chemin de la marche : du depart vers le coin le plus eloigne (niveau fini) ou une cellule a 40 cellules (niveau infini),
	 *  ordre fixe des buts ; but praticable, ni bassin ni salle de fosses (cherche par anneaux, ordre fixe) */
	bool BuildWalkPath(const ABRWorld* W, const FVector& From, float FeetOffset, TArray<FVector>& OutPoints)
	{
		OutPoints.Reset();
		const FBRLevelDef& D = W->Def();
		const FIntPoint Start = W->WorldToCell(From);
		TArray<FIntPoint> Wanted;
		if (D.BoundsChunks > 0)
		{
			const int32 Edge = D.BoundsChunks * D.ChunkCells - 2;
			TArray<FIntPoint> Corners = { FIntPoint(Edge, Edge), FIntPoint(-Edge, Edge), FIntPoint(-Edge, -Edge), FIntPoint(Edge, -Edge) };
			Corners.StableSort([&Start](const FIntPoint& A, const FIntPoint& B)
			{
				return (A - Start).SizeSquared() > (B - Start).SizeSquared();
			});
			Wanted = Corners;
		}
		else
		{
			Wanted = { Start + FIntPoint(PerfWalkFarCells, 0), Start + FIntPoint(0, PerfWalkFarCells), Start + FIntPoint(-PerfWalkFarCells, 0),
				Start + FIntPoint(0, -PerfWalkFarCells) };
		}
		for (const FIntPoint& Want : Wanted)
		{
			FIntPoint Goal(0, 0);
			bool bGoal = false;
			for (int32 Ring = 0; Ring <= 5 && !bGoal; ++Ring)
			{
				for (int32 DX = -Ring; DX <= Ring && !bGoal; ++DX)
				{
					for (int32 DY = -Ring; DY <= Ring && !bGoal; ++DY)
					{
						const FIntPoint Cell(Want.X + DX, Want.Y + DY);
						if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) == Ring && W->IsWalkable(Cell) && !W->IsPoolCell(Cell.X, Cell.Y)
							&& !W->IsPitRoomCell(Cell.X, Cell.Y))
						{
							Goal = Cell;
							bGoal = true;
						}
					}
				}
			}
			TArray<FIntPoint> Cells;
			if (!bGoal || !W->FindPath(Start, Goal, Cells, 20000) || Cells.Num() < PerfWalkMinCells)
			{
				continue;
			}
			for (const FIntPoint& Cell : Cells)
			{
				const FVector Center = W->CellCenter(Cell);
				OutPoints.Add(FVector(Center.X, Center.Y, W->FloorZAt(Center) + FeetOffset));
			}
			return true;
		}
		return false;
	}

	FVector PathLerp(const FPerfState& S, float Dist)
	{
		const int32 Num = S.Path.Num();
		if (Num == 0)
		{
			return FVector::ZeroVector;
		}
		if (Dist <= 0.f || Num == 1)
		{
			return S.Path[0];
		}
		if (Dist >= S.PathLength)
		{
			return S.Path.Last();
		}
		int32 Lo = 0;
		int32 Hi = Num - 1;
		while (Hi - Lo > 1)
		{
			const int32 MidIndex = (Lo + Hi) / 2;
			if (S.PathCum[MidIndex] <= Dist)
			{
				Lo = MidIndex;
			}
			else
			{
				Hi = MidIndex;
			}
		}
		const float Seg = FMath::Max(1.f, S.PathCum[Hi] - S.PathCum[Lo]);
		return FMath::Lerp(S.Path[Lo], S.Path[Hi], FMath::Clamp((Dist - S.PathCum[Lo]) / Seg, 0.f, 1.f));
	}

	/** Position de la marche apres Distance cm (aller-retour sur le chemin) et lacet du regard (1,5 m devant) */
	FVector WalkAt(FPerfState& S, float Distance, float& OutYaw)
	{
		const float Len = FMath::Max(1.f, S.PathLength);
		float U = FMath::Fmod(FMath::Max(0.f, Distance), 2.f * Len);
		const bool bBack = U > Len;
		if (bBack)
		{
			U = 2.f * Len - U;
		}
		const FVector Here = PathLerp(S, U);
		const FVector Ahead = PathLerp(S, FMath::Clamp(U + (bBack ? -PerfWalkLookAhead : PerfWalkLookAhead), 0.f, Len));
		FVector Dir = Ahead - Here;
		Dir.Z = 0.f;
		if (!Dir.IsNearlyZero(1.f))
		{
			S.WalkYawLast = static_cast<float>(Dir.Rotation().Yaw);
		}
		OutYaw = S.WalkYawLast;
		return Here;
	}

	/** Le joueur du scanner : focus maintenu, impulsion des qu'elle est prete, rotation reguliere */
	void DriveLidar(FPerfState& S, ABRCharacter* C, ABRPlayerController* PC)
	{
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!L || !PC)
		{
			return;
		}
		const double Now = FPlatformTime::Seconds();
		if (S.ScanStart <= 0.0)
		{
			S.ScanStart = Now;
		}
		const float Yaw = static_cast<float>(S.PoseRot.Yaw + PerfLidarYawRate * (Now - S.ScanStart));
		PC->SetControlRotation(FRotator(PerfLidarPitch, FRotator::NormalizeAxis(Yaw), 0.f));
		L->SetFocus(true);
		L->Pulse();
	}

	/** Composants des chunks : ajoutes depuis le debut de la fenetre (nouveaux chunks compris) */
	void CountChunkComponents(FPerfState& S, UWorld* InWorld, bool bBaseline)
	{
		if (!InWorld)
		{
			return;
		}
		for (TActorIterator<ABRChunk> It(InWorld); It; ++It)
		{
			const int32 Count = It->GetComponents().Num();
			const FObjectKey Key(*It);
			int32* Known = S.ChunkComponents.Find(Key);
			if (!Known)
			{
				S.ChunkComponents.Add(Key, Count);
				if (!bBaseline)
				{
					S.ComponentsAdded += Count;
					++S.ChunksAppeared;
				}
				continue;
			}
			if (Count > *Known)
			{
				S.ComponentsAdded += bBaseline ? 0 : Count - *Known;
				*Known = Count;
			}
		}
	}

	void SampleMemory(FPerfState& S)
	{
		FRHIMemoryStats Mem;
		RHIGetMemoryStats(Mem);
		S.VramUsedMax = FMath::Max(S.VramUsedMax, static_cast<double>(Mem.UsedLocal));
		S.RamMax = FMath::Max(S.RamMax, static_cast<double>(FPlatformMemory::GetStats().UsedPhysical));
	}

	/** Une ligne de tableur : noms et valeurs dans le meme ordre (l'en-tete vient de la premiere ligne) */
	struct FPerfCsv
	{
		TArray<FString> Names;
		TArray<FString> Values;
		void Text(const TCHAR* Name, const FString& Value)
		{
			Names.Add(Name);
			Values.Add(Value.Replace(TEXT(";"), TEXT(",")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")));
		}
		void Int(const TCHAR* Name, int64 Value)
		{
			Text(Name, FString::Printf(TEXT("%lld"), Value));
		}
		void Num(const TCHAR* Name, double Value, int32 Decimals = 2)
		{
			switch (Decimals)
			{
			case 0:
				Text(Name, FString::Printf(TEXT("%.0f"), Value));
				break;
			case 1:
				Text(Name, FString::Printf(TEXT("%.1f"), Value));
				break;
			case 3:
				Text(Name, FString::Printf(TEXT("%.3f"), Value));
				break;
			default:
				Text(Name, FString::Printf(TEXT("%.2f"), Value));
				break;
			}
		}
		void Empty(const TCHAR* Name)
		{
			Text(Name, FString());
		}
	};

	const FString PassHeader = TEXT("format;jeu;tag;execution;scene;niveau;graine_niveau;profil;type;nom;profondeur;ms_med;ms_min;ms_max;releves;gpu_image_ms_med");

	void SavePerfFiles(const FString& Dir)
	{
		TArray<FString> Out;
		if (PerfHeader.Num() > 0)
		{
			Out.Add(FString::Join(PerfHeader, TEXT(";")));
		}
		Out.Append(PerfLines);
		FFileHelper::SaveStringArrayToFile(Out, *FPaths::Combine(Dir, TEXT("Perf.csv")), FFileHelper::EEncodingOptions::ForceUTF8);
		TArray<FString> Passes;
		Passes.Add(PassHeader);
		Passes.Append(PassLines);
		FFileHelper::SaveStringArrayToFile(Passes, *FPaths::Combine(Dir, TEXT("PerfPasses.csv")), FFileHelper::EEncodingOptions::ForceUTF8);
	}

	/** Lit un releve ProfileGPU (lignes LogRHI) ; faux s'il n'y a aucun evenement de la file graphique. v4.16 (perf,
	 *  relecture) : les files de calcul asynchrone et de copie sont ramenees a des postes a part (AsyncCatMs) et leurs
	 *  chemins prefixes par "async:" ; "%d Other Children" est rattache a son parent (BRPerf::ParseGpuLine) */
	bool ParseGpuCapture(const TArray<FString>& Lines, FPerfGpuCapture& Out)
	{
		TArray<BRPerf::FGpuNode> Nodes;
		TArray<BRPerf::FGpuNode> AsyncNodes;
		TArray<FString> Stack;
		BRPerf::EQueueLine Queue = BRPerf::EQueueLine::None;
		TArray<TCHAR> NameBuffer;
		NameBuffer.SetNumZeroed(512);
		for (const FString& Text : Lines)
		{
			const TCHAR* Raw = *Text;
			const int32 RawLen = Text.Len();
			BRPerf::FGpuLine Event;
			if (BRPerf::ParseGpuLine(Raw, RawLen, Event))
			{
				if (Queue == BRPerf::EQueueLine::None)
				{
					continue;
				}
				const bool bGraphics = Queue == BRPerf::EQueueLine::Graphics;
				const TCHAR* Name = Raw + Event.NameStart;
				BRPerf::FGpuNode Node;
				Node.Depth = Event.Depth;
				Node.Ms = Event.Ms;
				Node.Cat = BRPerf::CategoryOf(Name, Event.NameLen);
				(bGraphics ? Nodes : AsyncNodes).Add(Node);
				if (Event.Depth == 0)
				{
					if (bGraphics)
					{
						Out.WaitMs += Event.WaitMs;
					}
					else
					{
						Out.OtherQueuesMs += Event.Ms;
					}
				}
				// Chemin stable (chiffres -> '#') jusqu'a la profondeur PerfGpuMaxDepth ; freres de meme chemin additionnes
				const int32 NameLen = BRPerf::NormalizeName(Name, Event.NameLen, NameBuffer.GetData(), NameBuffer.Num());
				const int32 Depth = FMath::Clamp(Event.Depth, 0, BRPerf::MaxTreeDepth - 1);
				Stack.SetNum(Depth + 1);
				Stack[Depth] = FString::ConstructFromPtrSize(NameBuffer.GetData(), NameLen);
				if (Depth <= PerfGpuMaxDepth)
				{
					const FString Key = (bGraphics ? FString() : FString(TEXT("async:"))) + FString::Join(Stack, TEXT("/"));
					if (double* Known = Out.PathMs.Find(Key))
					{
						*Known += Event.Ms;
					}
					else
					{
						Out.PathMs.Add(Key, Event.Ms);
						Out.PathDepth.Add(Key, Depth);
						Out.PathOrder.Add(Key);
					}
				}
				continue;
			}
			const BRPerf::EQueueLine Header = BRPerf::ParseQueueLine(Raw, RawLen);
			if (Header != BRPerf::EQueueLine::None)
			{
				Queue = Header;
				Stack.Reset();
				continue;
			}
			double FrameMs = 0.0;
			if (Queue == BRPerf::EQueueLine::Graphics && BRPerf::ParseFrameTimeLine(Raw, RawLen, FrameMs))
			{
				Out.FrameMs = FrameMs;
			}
		}
		if (Nodes.Num() == 0)
		{
			return false;
		}
		Out.RootMs = BRPerf::Attribute(Nodes.GetData(), Nodes.Num(), Out.CatMs);
		BRPerf::Attribute(AsyncNodes.GetData(), AsyncNodes.Num(), Out.AsyncCatMs);
		if (Out.FrameMs <= 0.0)
		{
			Out.FrameMs = Out.RootMs + Out.WaitMs;
		}
		return true;
	}

	double MedianOfCaptures(const TArray<FPerfGpuCapture>& Caps, TFunctionRef<double(const FPerfGpuCapture&)> Get, double& OutMin, double& OutMax)
	{
		TArray<double> Values;
		for (const FPerfGpuCapture& Cap : Caps)
		{
			Values.Add(Get(Cap));
		}
		OutMin = Values.Num() > 0 ? Values[0] : 0.0;
		OutMax = OutMin;
		for (const double V : Values)
		{
			OutMin = FMath::Min(OutMin, V);
			OutMax = FMath::Max(OutMax, V);
		}
		return BRPerf::MedianInPlace(Values.GetData(), Values.Num());
	}
}

void ABRAutoTest::AddPerfSteps()
{
	BRTestPerf::PerfOptions = BRTestPerf::ReadPerfOptions();
	const BRTestPerf::FPerfOptions Opt = BRTestPerf::PerfOptions;

	Add(TEXT("v4.16 perf : debut"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC || !GetBRWorld())
		{
			return false;
		}
		if (!bSettingsSaved)
		{
			SavedSettings = FBRSettings::Get();
			bSettingsSaved = true;
		}
		BRTestPerf::PerfHeader.Reset();
		BRTestPerf::PerfLines.Reset();
		BRTestPerf::PassLines.Reset();
		BRTestPerf::PerfMeasured = 0;
		BRTestPerf::PerfUnstable = 0;
		BRTestPerf::PerfInvalid = 0;
		BRTestPerf::PerfGpuDumps = 0;
		BRTestPerf::PerfSuiteStart = FPlatformTime::Seconds();
		FLevelReport R;
		R.Title = TEXT("v4.16 perf : machine et reglages");
		R.Scene = TEXT("perf");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		const BRTestPerf::FPerfOptions& O = BRTestPerf::PerfOptions;
		TArray<FString> SceneNames;
		for (const int32 I : O.SceneIndices)
		{
			SceneNames.Add(BRTestPerf::PerfScenes[I].Id);
		}
		TArray<FString> ProfileNames;
		for (const BRTestPerf::FPerfProfile& P : O.Profiles)
		{
			ProfileNames.Add(P.Name);
		}
		Note(FString::Printf(TEXT("perf : scenes %s ; profils %s ; fenetre %s ; prechauffage d'au moins %.0f s ; %d releve(s) ProfileGPU par mesure ; graine %s ; etiquette \"%s\", execution %d"),
			*FString::Join(SceneNames, TEXT(", ")), *FString::Join(ProfileNames, TEXT(", ")),
			O.Seconds > 0.f ? *FString::Printf(TEXT("%.0f s (-BRPerfSeconds)"), O.Seconds) : TEXT("10 s (marches 20 s, LiDAR 12 s)"), O.WarmSeconds, O.GpuCaptures,
			O.Seed ? *FString::Printf(TEXT("utilisateur %u (-BRPerfSeed)"), O.Seed) : TEXT("de reference de chaque scene"), *O.Tag, O.Run));
		Note(FString::Printf(TEXT("perf : carte %s, pilote %s (%s, %s) ; processeur %s (%d coeurs logiques), %.0f Go ; RHI %s, thread RHI %s ; ray tracing materiel %s"),
			*GRHIAdapterName.TrimStartAndEnd(), *GRHIAdapterUserDriverVersion, *GRHIAdapterInternalDriverVersion, *GRHIAdapterDriverDate,
			*FPlatformMisc::GetCPUBrand().TrimStartAndEnd(), FPlatformMisc::NumberOfCoresIncludingHyperthreads(),
			FPlatformMemory::GetConstants().TotalPhysical / (1024.0 * 1024.0 * 1024.0), *FApp::GetGraphicsRHI(), IsRHIThreadRunning() ? TEXT("oui") : TEXT("non"),
			ABRPlayerController::IsHardwareRayTracingAvailable() ? TEXT("disponible") : TEXT("INDISPONIBLE")));
		if (!ABRPlayerController::IsHardwareRayTracingAvailable())
		{
			Skip(TEXT("perf : ray tracing materiel indisponible : les profils Qualite, RTX fluide et Cinematique sont mesures sans lui (Lumen logiciel)"));
		}
		// Releve compact du profileur, sans sa fenetre (r.ProfileGPU.ShowUI ouvrirait le visualiseur hors de l'editeur).
		// v4.16 (perf, relecture) : ShowLeafEvents 1 (valeur du moteur) : a 0, un evenement sans dessin ni dispatch est cache
		// (HasWork) meme s'il occupe le processeur graphique (construction des structures du ray tracing, copies) et son
		// temps partait dans "N Other Children" de son parent
		static const TCHAR* const ProfilerVars[][2] = { { TEXT("r.ProfileGPU.ShowUI"), TEXT("0") }, { TEXT("r.ProfileGPU.TableFormatting"), TEXT("0") },
			{ TEXT("r.ProfileGPU.ShowLeafEvents"), TEXT("1") }, { TEXT("r.ProfileGPU.ThresholdPercent"), TEXT("0.1") }, { TEXT("r.ProfileGPU.Sort"), TEXT("0") },
			{ TEXT("r.ProfileGPU.Root"), TEXT("*") }, { TEXT("r.ProfileGPU.ShowHeader"), TEXT("1") } };
		int32 MissingVars = 0;
		for (const auto& Pair : ProfilerVars)
		{
			MissingVars += BRTestPerf::SetPerfCVar(Pair[0], Pair[1]) ? 0 : 1;
		}
		if (MissingVars > 0 && BRTestPerf::PerfOptions.GpuCaptures > 0)
		{
			BRTestPerf::PerfOptions.GpuCaptures = 0;
			Skip(TEXT("perf : variables r.ProfileGPU.* absentes (profileur du processeur graphique indisponible dans ce binaire) : aucune decomposition par passe"));
		}
		const FString Dir = OutDir;
		IFileManager::Get().MakeDirectory(*FPaths::Combine(Dir, TEXT("PerfGPU")), true);
		IFileManager::Get().MakeDirectory(*FPaths::Combine(Dir, TEXT("PerfFrames")), true);
		return true;
	});

	for (const BRTestPerf::FPerfProfile& Profile : Opt.Profiles)
	{
		for (const int32 SceneIndex : Opt.SceneIndices)
		{
			const BRTestPerf::FPerfScene& Scene = BRTestPerf::PerfScenes[SceneIndex];
			if (!BRLevels::Exists(Scene.Level))
			{
				Add(FString::Printf(TEXT("v4.16 perf : %s absente"), Scene.Id), 0.f, [this, SceneIndex]()
				{
					const BRTestPerf::FPerfScene& Missing = BRTestPerf::PerfScenes[SceneIndex];
					Skip(FString::Printf(TEXT("perf %s : Niveau %d absent de ce canal de contenu : scene non mesuree"), Missing.Id, Missing.Level));
					++BRTestPerf::PerfInvalid;
					return true;
				});
				continue;
			}
			const FString Key = FString::Printf(TEXT("%s %s"), Scene.Id, Profile.Name);
			const BRTestPerf::FPerfProfile ThisProfile = Profile;

			// 1. Profil applique avant le chargement (lumieres creees du bon type, meme etat de depart)
			Add(FString::Printf(TEXT("v4.16 perf : %s : profil"), *Key), 0.5f, [this, SceneIndex, ThisProfile]()
			{
				ABRPlayerController* PC = GetPC();
				ABRWorld* W = GetBRWorld();
				if (!PC || !W || !GetPlayer() || W->IsTransitioning())
				{
					return false;
				}
				const BRTestPerf::FPerfScene& Sc = BRTestPerf::PerfScenes[SceneIndex];
				BRTestPerf::FPerfState& S = BRTestPerf::PerfState;
				S = BRTestPerf::FPerfState();
				S.SceneIndex = SceneIndex;
				S.Profile = ThisProfile;
				const uint32 UserSeed = BRTestPerf::PerfOptions.Seed ? BRTestPerf::PerfOptions.Seed : Sc.UserSeed;
				S.UserSeed = UserSeed;
				S.LevelSeed = UserSeed ? ABRWorld::SeedFromUser(UserSeed, Sc.Level) : Sc.LevelSeed;
				S.Window = BRTestPerf::PerfOptions.Seconds > 0.f ? BRTestPerf::PerfOptions.Seconds : Sc.Seconds;
				BRTestPerf::ApplyPerfProfile(PC, ThisProfile.Index);
				W->bPerfQuiet = true;
				FLevelReport R;
				R.Level = Sc.Level;
				R.Title = FString::Printf(TEXT("perf %s %s"), Sc.Id, ThisProfile.Name);
				R.Scene = FString::Printf(TEXT("perf_%s_%s"), Sc.Id, ThisProfile.Name);
				R.FirstLogLine = LogLineCount();
				Reports.Add(R);
				return true;
			});

			// 2. Chargement (duree et attente des shaders a l'arrivee)
			Add(FString::Printf(TEXT("v4.16 perf : %s : chargement"), *Key), 0.f, [this]()
			{
				ABRPlayerController* PC = GetPC();
				ABRWorld* W = GetBRWorld();
				if (!PC || !W || !GetPlayer() || W->IsTransitioning())
				{
					return false;
				}
				BRTestPerf::FPerfState& S = BRTestPerf::PerfState;
				const BRTestPerf::FPerfScene& Sc = BRTestPerf::PerfScenes[S.SceneIndex];
				W->bPerfQuiet = true;
				W->RequestTransition(Sc.Level, false, S.LevelSeed);
				PC->BRLevel(Sc.Level); // ferme le menu principal s'il est ouvert (la demande avec graine passe la premiere)
				S.LoadStart = FPlatformTime::Seconds();
				return true;
			});
			Add(FString::Printf(TEXT("v4.16 perf : %s : attente du niveau"), *Key), 0.f, [this]()
			{
				BRTestPerf::FPerfState& S = BRTestPerf::PerfState;
				const BRTestPerf::FPerfScene& Sc = BRTestPerf::PerfScenes[S.SceneIndex];
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				if (W)
				{
					S.ShaderHoldMax = FMath::Max(S.ShaderHoldMax, W->GetShaderHold());
					W->bPerfQuiet = true;
				}
				const bool bReady = W && C && !W->IsTransitioning() && W->IsLevelReady() && W->GetLevelNumber() == Sc.Level && W->GetSeed() == S.LevelSeed && !C->IsDead();
				if (!bReady)
				{
					if (StepTime > BRTestPerf::PerfLoadTimeout)
					{
						S.bValid = false;
						S.Why = TEXT("niveau non charge");
						Skip(FString::Printf(TEXT("perf %s %s : Niveau %d (graine du niveau %u) non charge apres %.0f s : scene non mesuree"), Sc.Id, S.Profile.Name, Sc.Level,
							S.LevelSeed, BRTestPerf::PerfLoadTimeout));
						return true;
					}
					return false;
				}
				S.LoadSeconds = static_cast<float>(FPlatformTime::Seconds() - S.LoadStart);
				C->bGodMode = true;
				C->Health = 100.f;
				C->Sanity = 100.f;
				return true;
			});
			LimitLastStep(BRTestPerf::PerfLoadTimeout + 10.f);

			// 3. Pose de la scene
			Add(FString::Printf(TEXT("v4.16 perf : %s : pose"), *Key), 0.f, [this]()
			{
				BRTestPerf::FPerfState& S = BRTestPerf::PerfState;
				if (!S.bValid)
				{
					return true;
				}
				const BRTestPerf::FPerfScene& Sc = BRTestPerf::PerfScenes[S.SceneIndex];
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				if (!W || !C || !PC)
				{
					return false;
				}
				if (C->IsThirdPerson())
				{
					C->ToggleThirdPerson();
				}
				if (C->IsNightVision())
				{
					C->ToggleNightVision();
				}
				if (C->IsFlashlightOn())
				{
					C->ToggleFlashlight();
				}
				const float Half = C->GetSimpleCollisionHalfHeight();
				FVector Loc = C->GetActorLocation();
				FRotator Rot(0.f, PC->GetControlRotation().Yaw, 0.f);
				auto Invalidate = [this, &S, &Sc](const FString& Why)
				{
					S.bValid = false;
					S.Why = Why;
					Skip(FString::Printf(TEXT("perf %s %s : %s : scene non mesuree"), Sc.Id, S.Profile.Name, *Why));
				};
				switch (Sc.Kind)
				{
				case BRTestPerf::EPerfKind::Pits:
				{
					// Meme point de vue que la commande BRPits (et les captures L00_fosses_* et vis_0_*_fosses)
					FRotator View = FRotator::ZeroRotator;
					if (!W->FindPitRoomView(C->GetActorLocation(), Loc, View))
					{
						Invalidate(FString::Printf(TEXT("aucune salle de fosses (graine du niveau %u)"), S.LevelSeed));
						return true;
					}
					Loc.Z = Half + 5.f;
					Rot = View;
					break;
				}
				case BRTestPerf::EPerfKind::Walk:
				{
					if (!BRTestPerf::BuildWalkPath(W, C->GetActorLocation(), Half + 5.f, S.Path))
					{
						Invalidate(TEXT("aucun chemin de grille d'au moins 12 cellules vers un coin ou une cellule lointaine"));
						return true;
					}
					S.PathCum.SetNum(S.Path.Num());
					S.PathCum[0] = 0.f;
					for (int32 I = 1; I < S.Path.Num(); ++I)
					{
						S.PathCum[I] = S.PathCum[I - 1] + static_cast<float>(FVector::Dist2D(S.Path[I - 1], S.Path[I]));
					}
					S.PathLength = S.PathCum.Last();
					S.WalkYawLast = Rot.Yaw;
					float Yaw = 0.f;
					Loc = BRTestPerf::WalkAt(S, 0.f, Yaw);
					Rot = FRotator(0.f, Yaw, 0.f);
					S.RowNotes.Add(FString::Printf(TEXT("chemin de %d cellules (%.0f m) de (%d, %d) a (%d, %d)"), S.Path.Num(), S.PathLength / 100.f,
						W->WorldToCell(S.Path[0]).X, W->WorldToCell(S.Path[0]).Y, W->WorldToCell(S.Path.Last()).X, W->WorldToCell(S.Path.Last()).Y));
					break;
				}
				case BRTestPerf::EPerfKind::Lidar:
				{
					if (!C->GetLidar())
					{
						if (StepTime < 5.f)
						{
							return false;
						}
						Invalidate(TEXT("scanner absent"));
						return true;
					}
					Rot = FRotator(BRTestPerf::PerfLidarPitch, Rot.Yaw, 0.f);
					break;
				}
				default:
					break;
				}
				BRTestPerf::PlacePlayer(C, PC, Loc, Rot);
				C->Sanity = 100.f;
				C->Health = 100.f;
				S.PoseLoc = Loc;
				S.PoseRot = Rot;
				return true;
			});
			LimitLastStep(30.f);

			// 4. Prechauffage : ni shader en compilation, ni chunk en preparation, temps d'image stabilises
			Add(FString::Printf(TEXT("v4.16 perf : %s : prechauffage"), *Key), 0.f, [this]()
			{
				BRTestPerf::FPerfState& S = BRTestPerf::PerfState;
				ABRWorld* W = GetBRWorld();
				if (!S.bValid || !W)
				{
					return true;
				}
				const double Now = FPlatformTime::Seconds();
				if (S.WarmStart <= 0.0)
				{
					S.WarmStart = Now;
					S.WarmFrames.Reset();
					S.WarmFrames.Reserve(512);
					FMath::RandInit(BRTestPerf::PerfRandSeed);
					FMath::SRandInit(BRTestPerf::PerfRandSeed);
					return false;
				}
				S.WarmFrames.Add(static_cast<float>(FApp::GetDeltaTime() * 1000.0));
				if (S.WarmFrames.Num() > 480)
				{
					S.WarmFrames.RemoveAt(0, 240, EAllowShrinking::No);
				}
				const int32 Pending = BRTestPerf::PerfShaderJobs();
				S.WarmShaderMax = FMath::Max(S.WarmShaderMax, Pending);
				if (Pending > 0 || W->GetPreparingChunkCount() > 0)
				{
					S.ShaderQuietSince = -1.0;
				}
				else if (S.ShaderQuietSince < 0.0)
				{
					S.ShaderQuietSince = Now;
				}
				const bool bQuiet = S.ShaderQuietSince > 0.0 && Now - S.ShaderQuietSince >= BRTestPerf::PerfShaderQuiet;
				const bool bLongEnough = Now - S.WarmStart >= BRTestPerf::PerfOptions.WarmSeconds;
				bool bSettled = false;
				if (bQuiet && bLongEnough)
				{
					const BRPerf::FSettleRule Rule;
					TArray<float> Scratch;
					Scratch.SetNumUninitialized(BRPerf::SettleSpan(Rule));
					bSettled = BRPerf::IsSettled(S.WarmFrames.GetData(), S.WarmFrames.Num(), Rule, Scratch.GetData());
				}
				if (!bSettled && Now - S.WarmStart < BRTestPerf::PerfWarmTimeout)
				{
					return false;
				}
				S.bStable = bSettled;
				S.WarmSeconds = static_cast<float>(Now - S.WarmStart);
				return true;
			});
			LimitLastStep(BRTestPerf::PerfWarmTimeout + 15.f);

			// 4 bis. LiDAR : pre-remplissage (points deja en place, materiaux des glyphes compiles) avant la fenetre
			if (Scene.Kind == BRTestPerf::EPerfKind::Lidar)
			{
				Add(FString::Printf(TEXT("v4.16 perf : %s : pre-remplissage du scanner"), *Key), 0.f, [this]()
				{
					BRTestPerf::FPerfState& S = BRTestPerf::PerfState;
					if (!S.bValid)
					{
						return true;
					}
					const double Now = FPlatformTime::Seconds();
					if (S.PrefillStart <= 0.0)
					{
						S.PrefillStart = Now;
						S.ShaderQuietSince = -1.0;
					}
					BRTestPerf::DriveLidar(S, GetPlayer(), GetPC());
					if (BRTestPerf::PerfShaderJobs() > 0)
					{
						S.ShaderQuietSince = -1.0;
					}
					else if (S.ShaderQuietSince < 0.0)
					{
						S.ShaderQuietSince = Now;
					}
					const bool bQuiet = S.ShaderQuietSince > 0.0 && Now - S.ShaderQuietSince >= BRTestPerf::PerfShaderQuiet;
					if (Now - S.PrefillStart >= BRTestPerf::PerfLidarPrefill && bQuiet)
					{
						return true;
					}
					if (Now - S.PrefillStart >= BRTestPerf::PerfLidarPrefillTimeout)
					{
						S.RowNotes.Add(TEXT("shaders encore en compilation apres 30 s de pre-remplissage"));
						return true;
					}
					return false;
				});
				LimitLastStep(BRTestPerf::PerfLidarPrefillTimeout + 10.f);
			}

			// 5. Fenetre de mesure
			Add(FString::Printf(TEXT("v4.16 perf : %s : mesure"), *Key), 0.f, [this]()
			{
				BRTestPerf::FPerfState& S = BRTestPerf::PerfState;
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				if (!S.bValid)
				{
					return true;
				}
				if (!W || !C || !PC)
				{
					return false;
				}
				const BRTestPerf::FPerfScene& Sc = BRTestPerf::PerfScenes[S.SceneIndex];
				const double Now = FPlatformTime::Seconds();
				if (S.T0 <= 0.0)
				{
					// Debut : compteurs remis a zero ; l'image en cours appartient encore a l'etape precedente
					S.T0 = Now;
					const int32 Expected = FMath::Clamp(FMath::CeilToInt(S.Window * 300.f), 256, 65536);
					S.FrameTimes.Reset(Expected);
					S.GameTimes.Reset(Expected);
					S.RenderTimes.Reset(Expected);
					S.RhiTimes.Reset(Expected);
					S.GpuTimes.Reset(Expected);
					W->ResetChunkStats();
					ABRLidar::ResetStats();
					S.SyncBase = UBRAssets::SyncLoadsInGame;
					S.SyncNamesBase = UBRAssets::SyncLoadNames.Num();
					S.PsoBase = BRTestPerf::PerfPsoNow();
					S.ChunkComponents.Reset();
					BRTestPerf::CountChunkComponents(S, GetWorld(), true);
					S.NextMemory = Now;
					S.WalkFrom = C->GetActorLocation();
					C->Sanity = 100.f;
					FMath::RandInit(BRTestPerf::PerfRandSeed);
					FMath::SRandInit(BRTestPerf::PerfRandSeed);
					return false;
				}
				// L'image qui vient de finir
				S.FrameTimes.Add(static_cast<float>(FApp::GetDeltaTime() * 1000.0));
				S.GameTimes.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GGameThreadTime)));
				S.RenderTimes.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GRenderThreadTime)));
				S.RhiTimes.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GRHIThreadTime)));
				S.GpuTimes.Add(static_cast<float>(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0))));
				const int32 Pending = BRTestPerf::PerfShaderJobs();
				S.ShaderFrames += Pending > 0 ? 1 : 0;
				S.ShaderMax = FMath::Max(S.ShaderMax, Pending);
				S.EntitiesMax = FMath::Max(S.EntitiesMax, W->GetEntities().Num());
				BRTestPerf::CountChunkComponents(S, GetWorld(), false);
				if (Sc.Kind == BRTestPerf::EPerfKind::Lidar)
				{
					if (const ABRLidar* L = C->GetLidar())
					{
						S.LidarPeak = FMath::Max(S.LidarPeak, L->GetPointCount());
					}
				}
				if (Now >= S.NextMemory)
				{
					BRTestPerf::SampleMemory(S);
					S.NextMemory = Now + BRTestPerf::PerfMemoryEvery;
					S.InactiveFrames += FPlatformApplicationMisc::IsThisApplicationForeground() ? 0 : 1;
				}
				const double Elapsed = Now - S.T0;
				if (Elapsed >= S.Window)
				{
					// Fin : releves de la fenetre (les etapes suivantes ne les changent plus)
					S.Duration = Elapsed;
					S.ChunksBuilt = W->ChunksBuilt;
					S.ChunkBuildMs = W->ChunkBuildMsTotal;
					S.ChunkWorstFrameMs = W->MaxChunkBuildMs;
					S.FramesOverBudget = W->FramesOverBudget;
					S.ForcedBuilds = W->ForcedChunkBuilds;
					S.ChunksTornDown = W->ChunksTornDown;
					S.TeardownMs = W->TeardownMsTotal;
					S.TeardownWorstMs = W->MaxTeardownMs;
					S.SyncLoads = UBRAssets::SyncLoadsInGame - S.SyncBase;
					TArray<FString> NewNames;
					for (int32 I = S.SyncNamesBase; I < UBRAssets::SyncLoadNames.Num(); ++I)
					{
						NewNames.Add(UBRAssets::SyncLoadNames[I]);
					}
					S.SyncNames = FString::Join(NewNames, TEXT(", "));
					const BRTestPerf::FPerfPsoCount PsoEnd = BRTestPerf::PerfPsoNow();
					S.PsoWindow.Created = FMath::Max<int64>(0, PsoEnd.Created - S.PsoBase.Created);
					S.PsoWindow.Hitches = FMath::Max<int64>(0, PsoEnd.Hitches - S.PsoBase.Hitches);
					S.Lidar = ABRLidar::Stats;
					if (const ABRLidar* L = C->GetLidar())
					{
						S.LidarEnd = L->GetPointCount();
					}
					RHIGetMemoryStats(S.Memory);
					RHIGetTextureMemoryStats(S.Textures);
					S.BufferBytes = static_cast<double>(GRHIGlobals.BufferMemorySize);
					S.UniformBytes = static_cast<double>(GRHIGlobals.UniformBufferMemorySize);
					S.RamEnd = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical);
					BRTestPerf::SampleMemory(S);
					S.ChunksEnd = W->GetChunkCount();
					S.PreparingEnd = W->GetPreparingChunkCount();
					S.bForegroundEnd = FPlatformApplicationMisc::IsThisApplicationForeground();
					S.WalkMeters = FVector::Dist2D(S.WalkFrom, C->GetActorLocation()) / 100.0;
					if (Sc.Kind == BRTestPerf::EPerfKind::Walk)
					{
						S.WalkMeters = BRTestPerf::PerfWalkSpeed * Elapsed / 100.0;
					}
					return true;
				}
				// Mouvements de la scene, fonction du temps seul (memes positions a chaque execution)
				if (Sc.Kind == BRTestPerf::EPerfKind::Walk)
				{
					float Yaw = 0.f;
					const FVector At = BRTestPerf::WalkAt(S, static_cast<float>(BRTestPerf::PerfWalkSpeed * Elapsed), Yaw);
					C->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
					PC->SetControlRotation(FRotator(0.f, Yaw, 0.f));
				}
				else if (Sc.Kind == BRTestPerf::EPerfKind::Lidar)
				{
					BRTestPerf::DriveLidar(S, C, PC);
				}
				return false;
			});
			LimitLastStep(Scene.Seconds + (Opt.Seconds > 0.f ? Opt.Seconds : 0.f) + 60.f);

			// 6. Releves ProfileGPU, hors fenetre (le releve d'une image n'est pas compte dans la mesure)
			if (Opt.GpuCaptures > 0)
			{
				Add(FString::Printf(TEXT("v4.16 perf : %s : releves ProfileGPU"), *Key), 0.f, [this]()
				{
					BRTestPerf::FPerfState& S = BRTestPerf::PerfState;
					const int32 Wanted = BRTestPerf::PerfOptions.GpuCaptures;
					if (!S.bValid || Wanted <= 0 || !GEngine)
					{
						return true;
					}
					const BRTestPerf::FPerfScene& Sc = BRTestPerf::PerfScenes[S.SceneIndex];
					const double Now = FPlatformTime::Seconds();
					if (Sc.Kind == BRTestPerf::EPerfKind::Lidar)
					{
						BRTestPerf::DriveLidar(S, GetPlayer(), GetPC());
					}
					else if (Sc.Kind == BRTestPerf::EPerfKind::Walk && S.T0 > 0.0)
					{
						// v4.16 (perf, relecture) : la marche continue pendant les releves (meme loi du temps que la fenetre) : le
						// releve est celui d'une image de marche (streaming compris), pas d'un arret au bout du chemin
						if (ABRCharacter* C = GetPlayer())
						{
							float Yaw = 0.f;
							const FVector At = BRTestPerf::WalkAt(S, static_cast<float>(BRTestPerf::PerfWalkSpeed * (Now - S.T0)), Yaw);
							C->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
							if (ABRPlayerController* PC = GetPC())
							{
								PC->SetControlRotation(FRotator(0.f, Yaw, 0.f));
							}
						}
					}
					switch (S.GpuPhase)
					{
					case 0:
						BRTestPerf::AttachRhiLog();
						GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
						S.GpuIssued = Now;
						S.GpuPhase = 1;
						return false;
					case 1:
					{
						TArray<FString> Lines;
						double LastLine = 0.0;
						BRTestPerf::PerfRhiLog->Take(Lines, LastLine);
						bool bHeader = false;
						for (const FString& Line : Lines)
						{
							bHeader = bHeader || BRPerf::ParseQueueLine(*Line, Line.Len()) == BRPerf::EQueueLine::Graphics;
						}
						const bool bDone = bHeader && LastLine > 0.0 && Now - LastLine >= BRTestPerf::PerfGpuQuiet;
						if (!bDone && Now - S.GpuIssued < BRTestPerf::PerfGpuTimeout)
						{
							return false;
						}
						// Releve complet (en-tete de la file graphique puis 0,3 s sans nouvelle ligne), ou delai passe avec l'en-tete
						// recu (d'autres lignes LogRHI continuent d'arriver) : lu tel quel
						BRTestPerf::FPerfGpuCapture Cap;
						if (bHeader && BRTestPerf::ParseGpuCapture(Lines, Cap))
						{
							S.GpuCaptures.Add(MoveTemp(Cap));
							const FString Dump = FPaths::Combine(OutDir, TEXT("PerfGPU"), FString::Printf(TEXT("%s_%s_%d.txt"), Sc.Id, S.Profile.Name, S.GpuCaptures.Num()));
							FFileHelper::SaveStringArrayToFile(Lines, *Dump, FFileHelper::EEncodingOptions::ForceUTF8);
							++BRTestPerf::PerfGpuDumps;
						}
						else
						{
							++S.GpuFailed;
						}
						BRTestPerf::PerfRhiLog->Reset();
						S.GpuWaitFrames = 0;
						S.GpuPhase = 2;
						return false;
					}
					default:
						// Quelques images entre deux releves
						if (++S.GpuWaitFrames < 5)
						{
							return false;
						}
						if (S.GpuCaptures.Num() + S.GpuFailed < Wanted)
						{
							S.GpuPhase = 0;
							return false;
						}
						BRTestPerf::DetachRhiLog();
						return true;
					}
				});
				LimitLastStep(BRTestPerf::PerfGpuTimeout * (Opt.GpuCaptures + 1) + 20.f);
			}

			// 7. Bilan : ligne de Perf.csv, postes de PerfPasses.csv, temps des images, rapport
			Add(FString::Printf(TEXT("v4.16 perf : %s : bilan"), *Key), 0.f, [this]()
			{
				BRTestPerf::FPerfState& S = BRTestPerf::PerfState;
				const BRTestPerf::FPerfScene& Sc = BRTestPerf::PerfScenes[S.SceneIndex];
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				BRTestPerf::DetachRhiLog();
				if (C)
				{
					if (ABRLidar* L = C->GetLidar())
					{
						L->SetFocus(false);
					}
				}
				const FString Who = FString::Printf(TEXT("perf %s %s"), Sc.Id, S.Profile.Name);
				if (!S.bValid)
				{
					++BRTestPerf::PerfInvalid;
					Note(FString::Printf(TEXT("%s : non mesuree (%s)"), *Who, *S.Why));
					return true;
				}
				if (S.FrameTimes.Num() == 0 || !W)
				{
					++BRTestPerf::PerfInvalid;
					Note(FString::Printf(TEXT("%s : aucune image mesuree"), *Who), true);
					return true;
				}
				const FString Tag = BRTestPerf::PerfOptions.Tag;
				const int32 Run = BRTestPerf::PerfOptions.Run;

				// Temps de chaque image (avant les tris des resumes)
				if (BRTestPerf::PerfOptions.bFrames)
				{
					TArray<FString> FrameLines;
					FrameLines.Reserve(S.FrameTimes.Num() + 1);
					FrameLines.Add(TEXT("image;temps_ms;jeu_ms;rendu_ms;rhi_ms;gpu_ms"));
					for (int32 I = 0; I < S.FrameTimes.Num(); ++I)
					{
						FrameLines.Add(FString::Printf(TEXT("%d;%.3f;%.3f;%.3f;%.3f;%.3f"), I, S.FrameTimes[I], S.GameTimes[I], S.RenderTimes[I], S.RhiTimes[I], S.GpuTimes[I]));
					}
					FFileHelper::SaveStringArrayToFile(FrameLines, *FPaths::Combine(OutDir, TEXT("PerfFrames"), FString::Printf(TEXT("%s_%s.csv"), Sc.Id, S.Profile.Name)),
						FFileHelper::EEncodingOptions::ForceUTF8);
				}
				BRPerf::FSummary SumFrame = BRPerf::Summarize(S.FrameTimes.GetData(), S.FrameTimes.Num());
				BRPerf::FSummary SumGame = BRPerf::Summarize(S.GameTimes.GetData(), S.GameTimes.Num());
				BRPerf::FSummary SumRender = BRPerf::Summarize(S.RenderTimes.GetData(), S.RenderTimes.Num());
				BRPerf::FSummary SumRhi = BRPerf::Summarize(S.RhiTimes.GetData(), S.RhiTimes.Num());
				BRPerf::FSummary SumGpu = BRPerf::Summarize(S.GpuTimes.GetData(), S.GpuTimes.Num());
				const double FrameSeconds = SumFrame.Sum / 1000.0;
				const double Fps = FrameSeconds > 0.0 ? SumFrame.Frames / FrameSeconds : 0.0;

				// Decompte apres la fenetre : lumieres (CollectStats), composants enregistres dans le monde
				FLevelReport& R = Report();
				CollectStats(R);
				int64 WorldComponents = 0;
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					for (const UActorComponent* Comp : It->GetComponents())
					{
						WorldComponents += (Comp && Comp->IsRegistered()) ? 1 : 0;
					}
				}

				// Mode de rendu reel
				const FBRSettings& Live = FBRSettings::Get();
				const bool bRtActive = Live.bHardwareRT && ABRPlayerController::IsHardwareRayTracingAvailable();
				const bool bLumenHw = ABRPlayerController::IsHardwareRayTracingAvailable() && BRTestPerf::CVarValue(TEXT("r.Lumen.HardwareRayTracing"), 0.f) > 0.5f;
				const bool bHit = bLumenHw && BRTestPerf::CVarValue(TEXT("r.Lumen.HardwareRayTracing.LightingMode"), 0.f) > 0.5f;
				const FIntPoint Window = BRTestPerf::ViewportSize();
				const FIntPoint Internal = ABRPlayerController::InternalResolution();
				const int32 Aa = FMath::RoundToInt(BRTestPerf::CVarValue(TEXT("r.AntiAliasingMethod"), 4.f));
				const TCHAR* Upscaler = Aa == 4 ? TEXT("TSR") : (Aa == 2 ? TEXT("TAA") : (Aa == 1 ? TEXT("FXAA") : TEXT("aucun")));
				const bool bLidarScene = Sc.Kind == BRTestPerf::EPerfKind::Lidar;
				const double Secs = FMath::Max(0.001, S.Duration);
				if (!S.bStable)
				{
					S.RowNotes.Add(TEXT("prechauffage INSTABLE"));
				}
				if (S.InactiveFrames > 0 || !S.bForegroundEnd)
				{
					S.RowNotes.Add(TEXT("fenetre du jeu pas au premier plan"));
				}
				if (S.GpuFailed > 0)
				{
					S.RowNotes.Add(FString::Printf(TEXT("%d releve(s) ProfileGPU sans resultat"), S.GpuFailed));
				}

				BRTestPerf::FPerfCsv Row;
				Row.Int(TEXT("format"), BRTestPerf::PerfFormat);
				Row.Text(TEXT("jeu"), BR_GAME_VERSION);
				Row.Text(TEXT("tag"), Tag);
				Row.Int(TEXT("execution"), Run);
				Row.Text(TEXT("scene"), Sc.Id);
				Row.Int(TEXT("niveau"), Sc.Level);
				Row.Int(TEXT("graine_utilisateur"), S.UserSeed);
				Row.Int(TEXT("graine_niveau"), S.LevelSeed);
				Row.Text(TEXT("profil"), S.Profile.Name);
				Row.Int(TEXT("profil_index"), S.Profile.Index);
				Row.Int(TEXT("qualite"), Live.Quality);
				Row.Int(TEXT("rt_demande"), Live.bHardwareRT ? 1 : 0);
				Row.Int(TEXT("rt_actif"), bRtActive ? 1 : 0);
				Row.Int(TEXT("lumen_materiel"), bLumenHw ? 1 : 0);
				Row.Int(TEXT("hit_lighting"), bHit ? 1 : 0);
				Row.Int(TEXT("megalights"), ABRWorld::IsMegaLightsActive() ? 1 : 0);
				Row.Int(TEXT("megalights_echantillons"), FMath::RoundToInt(BRTestPerf::CVarValue(TEXT("r.MegaLights.NumSamplesPerPixel"), 0.f)));
				Row.Int(TEXT("ombres_virtuelles"), BRTestPerf::CVarValue(TEXT("r.Shadow.Virtual.Enable"), 0.f) > 0.5f ? 1 : 0);
				Row.Int(TEXT("ombres_rt_lampe"), (Live.bRTShadows && ABRPlayerController::IsHardwareRayTracingAvailable()) ? 1 : 0);
				Row.Int(TEXT("brouillard_volumetrique"), BRTestPerf::CVarValue(TEXT("r.VolumetricFog"), 0.f) > 0.5f ? 1 : 0);
				Row.Int(TEXT("fenetre_x"), Window.X);
				Row.Int(TEXT("fenetre_y"), Window.Y);
				Row.Int(TEXT("interne_x"), Internal.X);
				Row.Int(TEXT("interne_y"), Internal.Y);
				Row.Int(TEXT("pourcentage_rendu"), FMath::RoundToInt(BRTestPerf::CVarValue(TEXT("r.ScreenPercentage"), 100.f)));
				Row.Text(TEXT("agrandissement"), Upscaler);
				Row.Int(TEXT("resolution_dynamique"), BRTestPerf::CVarValue(TEXT("r.DynamicRes.OperationMode"), 0.f) > 0.5f ? 1 : 0);
				Row.Int(TEXT("vsync"), BRTestPerf::CVarValue(TEXT("r.VSync"), 0.f) > 0.5f ? 1 : 0);
				Row.Int(TEXT("max_fps"), FMath::RoundToInt(BRTestPerf::CVarValue(TEXT("t.MaxFPS"), 0.f)));
				Row.Int(TEXT("premier_plan"), S.bForegroundEnd ? 1 : 0);
				Row.Text(TEXT("gpu"), GRHIAdapterName.TrimStartAndEnd());
				Row.Text(TEXT("pilote"), GRHIAdapterUserDriverVersion);
				Row.Text(TEXT("pilote_interne"), GRHIAdapterInternalDriverVersion);
				Row.Text(TEXT("pilote_date"), GRHIAdapterDriverDate);
				Row.Text(TEXT("cpu"), FPlatformMisc::GetCPUBrand().TrimStartAndEnd());
				Row.Int(TEXT("coeurs"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
				Row.Num(TEXT("ram_totale_go"), FPlatformMemory::GetConstants().TotalPhysical / (1024.0 * 1024.0 * 1024.0), 1);
				Row.Text(TEXT("rhi"), FApp::GetGraphicsRHI());
				Row.Int(TEXT("thread_rhi"), IsRHIThreadRunning() ? 1 : 0);
				Row.Num(TEXT("chargement_s"), S.LoadSeconds, 2);
				Row.Num(TEXT("attente_shaders_arrivee_s"), S.ShaderHoldMax, 2);
				Row.Num(TEXT("prechauffe_s"), S.WarmSeconds, 2);
				Row.Int(TEXT("prechauffe_stable"), S.bStable ? 1 : 0);
				Row.Int(TEXT("shaders_max_prechauffe"), S.WarmShaderMax);
				Row.Num(TEXT("duree_s"), S.Duration, 3);
				Row.Int(TEXT("images"), SumFrame.Frames);
				Row.Num(TEXT("img_s"), Fps, 1);
				Row.Num(TEXT("moyenne_ms"), SumFrame.Mean, 3);
				Row.Num(TEXT("mediane_ms"), SumFrame.Median, 3);
				Row.Num(TEXT("p95_ms"), SumFrame.P95, 3);
				Row.Num(TEXT("p99_ms"), SumFrame.P99, 3);
				Row.Num(TEXT("pire_ms"), SumFrame.Worst, 3);
				Row.Num(TEXT("bas_1pct_img_s"), SumFrame.Low1Mean > 0.f ? 1000.0 / SumFrame.Low1Mean : 0.0, 1);
				Row.Int(TEXT("longues_33"), SumFrame.Long[0]);
				Row.Int(TEXT("longues_50"), SumFrame.Long[1]);
				Row.Int(TEXT("longues_100"), SumFrame.Long[2]);
				Row.Num(TEXT("jeu_moy_ms"), SumGame.Mean, 3);
				Row.Num(TEXT("jeu_med_ms"), SumGame.Median, 3);
				Row.Num(TEXT("jeu_p95_ms"), SumGame.P95, 3);
				Row.Num(TEXT("rendu_moy_ms"), SumRender.Mean, 3);
				Row.Num(TEXT("rendu_med_ms"), SumRender.Median, 3);
				Row.Num(TEXT("rendu_p95_ms"), SumRender.P95, 3);
				Row.Num(TEXT("rhi_moy_ms"), SumRhi.Mean, 3);
				Row.Num(TEXT("rhi_med_ms"), SumRhi.Median, 3);
				Row.Num(TEXT("rhi_p95_ms"), SumRhi.P95, 3);
				Row.Num(TEXT("gpu_moy_ms"), SumGpu.Mean, 3);
				Row.Num(TEXT("gpu_med_ms"), SumGpu.Median, 3);
				Row.Num(TEXT("gpu_p95_ms"), SumGpu.P95, 3);
				Row.Int(TEXT("images_shaders"), S.ShaderFrames);
				Row.Int(TEXT("shaders_max"), S.ShaderMax);
				Row.Int(TEXT("pso_crees"), S.PsoWindow.Created);
				Row.Int(TEXT("pso_saccades"), S.PsoWindow.Hitches);
				Row.Num(TEXT("vram_utilisee_mo"), BRTestPerf::ToMB(static_cast<double>(S.Memory.UsedLocal)), 0);
				Row.Num(TEXT("vram_max_mo"), BRTestPerf::ToMB(S.VramUsedMax), 0);
				Row.Num(TEXT("vram_budget_mo"), BRTestPerf::ToMB(static_cast<double>(S.Memory.BudgetLocal)), 0);
				Row.Num(TEXT("vram_retrogradee_mo"), BRTestPerf::ToMB(static_cast<double>(S.Memory.DemotedLocal)), 0);
				Row.Num(TEXT("vram_dediee_mo"), BRTestPerf::ToMB(static_cast<double>(FMath::Max<int64>(0, S.Textures.DedicatedVideoMemory))), 0);
				Row.Num(TEXT("textures_mo"), BRTestPerf::ToMB(static_cast<double>(S.Textures.StreamingMemorySize + S.Textures.NonStreamingMemorySize)), 0);
				Row.Num(TEXT("pool_textures_mo"), BRTestPerf::ToMB(static_cast<double>(S.Textures.TexturePoolSize)), 0);
				Row.Num(TEXT("tampons_mo"), BRTestPerf::ToMB(S.BufferBytes), 0);
				Row.Num(TEXT("tampons_uniformes_mo"), BRTestPerf::ToMB(S.UniformBytes), 1);
				Row.Num(TEXT("ram_mo"), BRTestPerf::ToMB(S.RamEnd), 0);
				Row.Num(TEXT("ram_max_mo"), BRTestPerf::ToMB(S.RamMax), 0);
				Row.Int(TEXT("chunks"), S.ChunksEnd);
				Row.Int(TEXT("chunks_en_preparation"), S.PreparingEnd);
				Row.Int(TEXT("chunks_construits"), S.ChunksBuilt);
				Row.Num(TEXT("construction_ms"), S.ChunkBuildMs, 2);
				Row.Num(TEXT("construction_pire_image_ms"), S.ChunkWorstFrameMs, 2);
				Row.Int(TEXT("images_hors_budget"), S.FramesOverBudget);
				Row.Int(TEXT("constructions_forcees"), S.ForcedBuilds);
				Row.Int(TEXT("chunks_demontes"), S.ChunksTornDown);
				Row.Num(TEXT("demontage_ms"), S.TeardownMs, 2);
				Row.Num(TEXT("demontage_pire_ms"), S.TeardownWorstMs, 2);
				Row.Int(TEXT("composants_ajoutes"), S.ComponentsAdded);
				Row.Num(TEXT("composants_par_s"), static_cast<double>(S.ComponentsAdded) / Secs, 1);
				Row.Int(TEXT("chunks_apparus"), S.ChunksAppeared);
				Row.Int(TEXT("composants_monde"), WorldComponents);
				Row.Int(TEXT("chargements_sync"), S.SyncLoads);
				Row.Text(TEXT("chargements_sync_noms"), S.SyncNames);
				Row.Int(TEXT("lumieres"), R.Lights);
				Row.Int(TEXT("lumieres_ombres"), R.ShadowLights);
				Row.Int(TEXT("ombres_locales"), W->GetShadowedLightCount());
				Row.Int(TEXT("entites_max"), S.EntitiesMax);
				Row.Num(TEXT("distance_m"), S.WalkMeters, 1);
				if (bLidarScene)
				{
					const FBRLidarStats& Li = S.Lidar;
					const double LiFrames = Li.Frames > 0 ? static_cast<double>(Li.Frames) : 1.0;
					Row.Num(TEXT("lidar_rayons_s"), static_cast<double>(Li.Rays) / Secs, 0);
					Row.Num(TEXT("lidar_visees_s"), static_cast<double>(Li.AimTraces) / Secs, 0);
					Row.Num(TEXT("lidar_points_s"), static_cast<double>(Li.Accepted) / Secs, 0);
					Row.Num(TEXT("lidar_remplaces_s"), static_cast<double>(Li.Replaced) / Secs, 0);
					Row.Num(TEXT("lidar_expires_s"), static_cast<double>(Li.Expired) / Secs, 0);
					Row.Int(TEXT("lidar_refuses"), Li.Rejected);
					Row.Int(TEXT("lidar_instances_fin"), S.LidarEnd);
					Row.Int(TEXT("lidar_instances_max"), S.LidarPeak);
					Row.Num(TEXT("lidar_inscriptions_s"), static_cast<double>(Li.GlyphPoints) / Secs, 0);
					Row.Num(TEXT("lidar_materiaux_s"), static_cast<double>(Li.MidUpdates) / Secs, 1);
					Row.Num(TEXT("lidar_cpu_ms"), Li.TotalSeconds * 1000.0 / LiFrames, 3);
					Row.Num(TEXT("lidar_traces_ms"), Li.TraceSeconds * 1000.0 / LiFrames, 3);
					Row.Num(TEXT("lidar_envoi_ms"), Li.UploadSeconds * 1000.0 / LiFrames, 3);
				}
				else
				{
					for (const TCHAR* Name : { TEXT("lidar_rayons_s"), TEXT("lidar_visees_s"), TEXT("lidar_points_s"), TEXT("lidar_remplaces_s"), TEXT("lidar_expires_s"),
						TEXT("lidar_refuses"), TEXT("lidar_instances_fin"), TEXT("lidar_instances_max"), TEXT("lidar_inscriptions_s"), TEXT("lidar_materiaux_s"),
						TEXT("lidar_cpu_ms"), TEXT("lidar_traces_ms"), TEXT("lidar_envoi_ms") })
					{
						Row.Empty(Name);
					}
				}
				// Postes du processeur graphique (mediane des releves)
				Row.Int(TEXT("gpu_releves"), S.GpuCaptures.Num());
				double Lo = 0.0;
				double Hi = 0.0;
				const bool bGpu = S.GpuCaptures.Num() > 0;
				const double GpuFrame = bGpu ? BRTestPerf::MedianOfCaptures(S.GpuCaptures, [](const BRTestPerf::FPerfGpuCapture& Cap) { return Cap.FrameMs; }, Lo, Hi) : 0.0;
				TArray<FString> PostText;
				auto CapCell = [&Row, bGpu](const TCHAR* Name, double Value)
				{
					if (bGpu)
					{
						Row.Num(Name, Value, 3);
					}
					else
					{
						Row.Empty(Name);
					}
				};
				CapCell(TEXT("gpu_image_ms"), GpuFrame);
				CapCell(TEXT("gpu_racine_ms"), bGpu ? BRTestPerf::MedianOfCaptures(S.GpuCaptures, [](const BRTestPerf::FPerfGpuCapture& Cap) { return Cap.RootMs; }, Lo, Hi) : 0.0);
				CapCell(TEXT("gpu_attente_ms"), bGpu ? BRTestPerf::MedianOfCaptures(S.GpuCaptures, [](const BRTestPerf::FPerfGpuCapture& Cap) { return Cap.WaitMs; }, Lo, Hi) : 0.0);
				CapCell(TEXT("gpu_autres_files_ms"), bGpu ? BRTestPerf::MedianOfCaptures(S.GpuCaptures, [](const BRTestPerf::FPerfGpuCapture& Cap) { return Cap.OtherQueuesMs; }, Lo, Hi) : 0.0);
				const FString PassPrefix = FString::Printf(TEXT("%d;%s;%s;%d;%s;%d;%u;%s"), BRTestPerf::PerfFormat, BR_GAME_VERSION, *Tag, Run, Sc.Id, Sc.Level, S.LevelSeed, S.Profile.Name);
				for (int32 Cat = 0; Cat < BRPerf::NumCategories; ++Cat)
				{
					const double Med = bGpu ? BRTestPerf::MedianOfCaptures(S.GpuCaptures, [Cat](const BRTestPerf::FPerfGpuCapture& Cap) { return Cap.CatMs[Cat]; }, Lo, Hi) : 0.0;
					CapCell(*FString::Printf(TEXT("gpu_%s_ms"), ANSI_TO_TCHAR(BRPerf::CategoryName(Cat))), Med);
					if (bGpu)
					{
						BRTestPerf::PassLines.Add(FString::Printf(TEXT("%s;poste;%s;-1;%.3f;%.3f;%.3f;%d;%.3f"), *PassPrefix, ANSI_TO_TCHAR(BRPerf::CategoryName(Cat)), Med, Lo, Hi,
							S.GpuCaptures.Num(), GpuFrame));
						if (Med >= 0.05)
						{
							PostText.Add(FString::Printf(TEXT("%s %.2f"), ANSI_TO_TCHAR(BRPerf::CategoryName(Cat)), Med));
						}
					}
				}
				TArray<FString> AsyncText;
				if (bGpu)
				{
					// v4.16 (perf, relecture) : files de calcul asynchrone (en parallele de la file graphique : a part)
					for (int32 Cat = 0; Cat < BRPerf::NumCategories; ++Cat)
					{
						const double Med = BRTestPerf::MedianOfCaptures(S.GpuCaptures, [Cat](const BRTestPerf::FPerfGpuCapture& Cap) { return Cap.AsyncCatMs[Cat]; }, Lo, Hi);
						BRTestPerf::PassLines.Add(FString::Printf(TEXT("%s;poste_async;%s;-1;%.3f;%.3f;%.3f;%d;%.3f"), *PassPrefix, ANSI_TO_TCHAR(BRPerf::CategoryName(Cat)), Med, Lo,
							Hi, S.GpuCaptures.Num(), GpuFrame));
						if (Med >= 0.05)
						{
							AsyncText.Add(FString::Printf(TEXT("%s %.2f"), ANSI_TO_TCHAR(BRPerf::CategoryName(Cat)), Med));
						}
					}
				}
				if (bGpu)
				{
					// Passes (chemins jusqu'a la profondeur PerfGpuMaxDepth ; "async:" : autres files), dans l'ordre du premier
					// releve puis des suivants ; absente d'un releve : 0
					TArray<FString> Order;
					for (const BRTestPerf::FPerfGpuCapture& Cap : S.GpuCaptures)
					{
						for (const FString& PathKey : Cap.PathOrder)
						{
							Order.AddUnique(PathKey);
						}
					}
					for (const FString& PathKey : Order)
					{
						int32 Depth = 0;
						for (const BRTestPerf::FPerfGpuCapture& Cap : S.GpuCaptures)
						{
							if (const int32* Found = Cap.PathDepth.Find(PathKey))
							{
								Depth = *Found;
								break;
							}
						}
						const double Med = BRTestPerf::MedianOfCaptures(S.GpuCaptures, [&PathKey](const BRTestPerf::FPerfGpuCapture& Cap) { return Cap.PathMs.FindRef(PathKey); }, Lo, Hi);
						if (Hi < BRTestPerf::PerfGpuMinMs)
						{
							continue;
						}
						BRTestPerf::PassLines.Add(FString::Printf(TEXT("%s;passe;%s;%d;%.3f;%.3f;%.3f;%d;%.3f"), *PassPrefix, *PathKey.Replace(TEXT(";"), TEXT(",")), Depth, Med, Lo, Hi,
							S.GpuCaptures.Num(), GpuFrame));
					}
				}
				Row.Num(TEXT("pose_x"), S.PoseLoc.X, 0);
				Row.Num(TEXT("pose_y"), S.PoseLoc.Y, 0);
				Row.Num(TEXT("pose_z"), S.PoseLoc.Z, 0);
				Row.Num(TEXT("pose_yaw"), S.PoseRot.Yaw, 1);
				Row.Num(TEXT("pose_pitch"), S.PoseRot.Pitch, 1);
				Row.Text(TEXT("notes"), FString::Join(S.RowNotes, TEXT(" | ")));
				if (BRTestPerf::PerfHeader.Num() == 0)
				{
					BRTestPerf::PerfHeader = Row.Names;
				}
				BRTestPerf::PerfLines.Add(FString::Join(Row.Values, TEXT(";")));
				BRTestPerf::SavePerfFiles(OutDir);
				++BRTestPerf::PerfMeasured;
				BRTestPerf::PerfUnstable += S.bStable ? 0 : 1;

				// Ligne du rapport (Mesures.csv, Rapport.txt)
				R.AvgFPS = static_cast<float>(Fps);
				R.WorstMs = SumFrame.Worst;
				R.P50Ms = SumFrame.Median;
				R.P95Ms = SumFrame.P95;
				R.P99Ms = SumFrame.P99;
				R.Low1FPS = SumFrame.Low1Mean > 0.f ? 1000.f / SumFrame.Low1Mean : 0.f;
				R.GpuMs = SumGpu.Mean;
				R.GameMs = SumGame.Mean;
				R.RenderMs = SumRender.Mean;
				R.RamMB = static_cast<float>(BRTestPerf::ToMB(S.RamEnd));
				R.TexMB = static_cast<float>(BRTestPerf::ToMB(static_cast<double>(S.Textures.StreamingMemorySize + S.Textures.NonStreamingMemorySize)));
				R.Chunks = S.ChunksEnd;
				R.ChunkMaxMs = S.ChunkWorstFrameMs;
				R.ChunkAvgMs = S.ChunksBuilt > 0 ? S.ChunkBuildMs / S.ChunksBuilt : 0.f;
				R.FramesOverBudget = S.FramesOverBudget;
				R.MaxTeardownMs = S.TeardownWorstMs;
				R.SyncLoads = S.SyncLoads;
				Note(FString::Printf(TEXT("%s (Niveau %d, graine du niveau %u) : %.1f img/s ; mediane %.2f ms, 95 %% %.2f, 99 %% %.2f, pire %.1f ; longues images > 33 / 50 / 100 ms : %d / %d / %d sur %d en %.1f s"),
					*Who, Sc.Level, S.LevelSeed, Fps, SumFrame.Median, SumFrame.P95, SumFrame.P99, SumFrame.Worst, SumFrame.Long[0], SumFrame.Long[1], SumFrame.Long[2],
					SumFrame.Frames, S.Duration));
				Note(FString::Printf(TEXT("%s : mediane des threads : jeu %.2f ms, rendu %.2f, RHI %.2f, processeur graphique %.2f (95 %% : %.2f) ; %d image(s) avec des shaders en compilation"),
					*Who, SumGame.Median, SumRender.Median, SumRhi.Median, SumGpu.Median, SumGpu.P95, S.ShaderFrames));
				Note(FString::Printf(TEXT("%s : memoire video %.0f / %.0f Mo (pic %.0f, retrogradee %.0f), textures %.0f Mo, tampons %.0f Mo ; RAM %.0f Mo (pic %.0f)"), *Who,
					BRTestPerf::ToMB(static_cast<double>(S.Memory.UsedLocal)), BRTestPerf::ToMB(static_cast<double>(S.Memory.BudgetLocal)), BRTestPerf::ToMB(S.VramUsedMax),
					BRTestPerf::ToMB(static_cast<double>(S.Memory.DemotedLocal)), R.TexMB, BRTestPerf::ToMB(S.BufferBytes), BRTestPerf::ToMB(S.RamEnd), BRTestPerf::ToMB(S.RamMax)));
				Note(FString::Printf(TEXT("%s : rendu %dx%d -> %dx%d (%s), RT %s, Lumen %s, hit lighting %s, MegaLights %s ; chargement %.1f s (shaders a l'arrivee %.1f s), prechauffage %.1f s (%s)"),
					*Who, Internal.X, Internal.Y, Window.X, Window.Y, Upscaler, bRtActive ? TEXT("actif") : TEXT("non"), bLumenHw ? TEXT("materiel") : TEXT("logiciel"),
					bHit ? TEXT("oui") : TEXT("non"), ABRWorld::IsMegaLightsActive() ? TEXT("actif") : TEXT("non"), S.LoadSeconds, S.ShaderHoldMax, S.WarmSeconds,
					S.bStable ? TEXT("stable") : TEXT("INSTABLE")));
				Note(FString::Printf(TEXT("%s : streaming : %d chunk(s) construit(s) (%.1f ms, pire image %.1f ms, %d au-dela du budget, %d force(s)), %d demonte(s) (%.1f ms, pire %.1f), %lld composant(s) ajoute(s) aux chunks, %d chargement(s) synchrone(s)%s%s ; %d composants enregistres dans le monde"),
					*Who, S.ChunksBuilt, S.ChunkBuildMs, S.ChunkWorstFrameMs, S.FramesOverBudget, S.ForcedBuilds, S.ChunksTornDown, S.TeardownMs, S.TeardownWorstMs, S.ComponentsAdded,
					S.SyncLoads, S.SyncNames.IsEmpty() ? TEXT("") : TEXT(" : "), *S.SyncNames, static_cast<int32>(WorldComponents)));
				Note(FString::Printf(TEXT("%s : PSO crees a la demande pendant la fenetre : %lld (creations trop longues : %lld) ; %d image(s) avec shaders ou PSO en preparation (au plus %d)"),
					*Who, S.PsoWindow.Created, S.PsoWindow.Hitches, S.ShaderFrames, S.ShaderMax));
				if (bLidarScene)
				{
					const FBRLidarStats& Li = S.Lidar;
					Note(FString::Printf(TEXT("%s : scanner : %.0f rayons/s, %.0f visees/s, %.0f points/s acceptes (%lld refuse(s)), %d instances a la fin (pic %d), %.0f points d'inscription/s, %.1f materiaux mis a jour/s, %.3f ms/image (traces %.3f, envoi des instances %.3f)"),
						*Who, Li.Rays / Secs, Li.AimTraces / Secs, Li.Accepted / Secs, Li.Rejected, S.LidarEnd, S.LidarPeak, Li.GlyphPoints / Secs, Li.MidUpdates / Secs,
						Li.Frames > 0 ? Li.TotalSeconds * 1000.0 / Li.Frames : 0.0, Li.Frames > 0 ? Li.TraceSeconds * 1000.0 / Li.Frames : 0.0,
						Li.Frames > 0 ? Li.UploadSeconds * 1000.0 / Li.Frames : 0.0));
				}
				if (bGpu)
				{
					Note(FString::Printf(TEXT("%s : processeur graphique (ProfileGPU, mediane de %d releve(s)) : image %.2f ms ; %s ; files asynchrones : %s"), *Who, S.GpuCaptures.Num(),
						GpuFrame, *FString::Join(PostText, TEXT(", ")), AsyncText.Num() > 0 ? *FString::Join(AsyncText, TEXT(", ")) : TEXT("aucune")));
				}
				else if (BRTestPerf::PerfOptions.GpuCaptures > 0)
				{
					Skip(FString::Printf(TEXT("%s : decomposition du processeur graphique non obtenue (%d releve(s) ProfileGPU sans resultat)"), *Who, S.GpuFailed));
				}
				if (!S.bStable)
				{
					Skip(FString::Printf(TEXT("%s : temps d'image non stabilises apres %.0f s de prechauffage : mesure gardee, marquee instable"), *Who, S.WarmSeconds));
				}
				if (PC)
				{
					Note(Who + TEXT(" : ") + PC->GetRenderModeReport());
				}
				for (const FString& Extra : S.RowNotes)
				{
					Note(Who + TEXT(" : ") + Extra);
				}
				return true;
			});
		}
	}

	Add(TEXT("v4.16 perf : bilan"), 0.f, [this]()
	{
		BRTestPerf::DetachRhiLog();
		BRTestPerf::RestorePerfCVars();
		if (ABRWorld* W = GetBRWorld())
		{
			W->bPerfQuiet = false;
		}
		if (bSettingsSaved)
		{
			FBRSettings::Get() = SavedSettings;
			bSettingsSaved = false;
			if (ABRPlayerController* PC = GetPC())
			{
				PC->ApplySettings();
			}
		}
		BRTestPerf::SavePerfFiles(OutDir);
		FLevelReport R;
		R.Title = TEXT("v4.16 perf : bilan");
		R.Scene = TEXT("perf");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		Note(FString::Printf(TEXT("perf : %d mesure(s) ecrite(s) dans Perf.csv (%d instable(s)), %d non faite(s) ; %d ligne(s) dans PerfPasses.csv, %d releve(s) ProfileGPU dans PerfGPU/ ; %.0f s"),
			BRTestPerf::PerfMeasured, BRTestPerf::PerfUnstable, BRTestPerf::PerfInvalid, BRTestPerf::PassLines.Num(), BRTestPerf::PerfGpuDumps,
			FPlatformTime::Seconds() - BRTestPerf::PerfSuiteStart));
		if (BRTestPerf::PerfMeasured == 0)
		{
			Note(TEXT("perf : aucune mesure faite"), true);
		}
		return true;
	});
}
