#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRPlayerController.h"
#include "BRWaterSim.h"
#include "BRWorld.h"

#include "Components/LocalLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "DynamicRHI.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "RHI.h"
#include "RenderTimer.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

/** Recopie les avertissements et erreurs du journal pendant le test */
class FBRLogCapture : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
		if (Level > ELogVerbosity::Warning || !V)
		{
			return;
		}
		// Bruit connu du binaire de l'editeur lance en mode jeu, sans rapport avec le jeu
		static const FName Ignored[] = { TEXT("LogEditorDataStorageUI"), TEXT("LogEditorDataStorage") };
		for (const FName& I : Ignored)
		{
			if (Category == I)
			{
				return;
			}
		}
		FScopeLock Lock(&Mutex);
		Lines.Add(FString::Printf(TEXT("%s [%s] %s"), Level <= ELogVerbosity::Error ? TEXT("ERREUR") : TEXT("Avert."), *Category.ToString(), V));
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }

	int32 Num()
	{
		FScopeLock Lock(&Mutex);
		return Lines.Num();
	}

	TArray<FString> Range(int32 From, int32 To)
	{
		FScopeLock Lock(&Mutex);
		TArray<FString> Out;
		for (int32 i = FMath::Max(0, From); i < FMath::Min(To, Lines.Num()); ++i)
		{
			Out.AddUnique(Lines[i]);
		}
		return Out;
	}

private:
	FCriticalSection Mutex;
	TArray<FString> Lines;
};

ABRAutoTest::ABRAutoTest()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetActorEnableCollision(false);
}

bool ABRAutoTest::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("BRAutoTest")) || IsNetTestRequested();
}

bool ABRAutoTest::IsNetTestRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("BRNetTest"));
}

void ABRAutoTest::BeginPlay()
{
	Super::BeginPlay();
	StartTime = FPlatformTime::Seconds();
	bNetTest = IsNetTestRequested();
	// Test multijoueur : l'hote et le client ecrivent chacun dans leur dossier
	const FString Sub = !bNetTest ? FString(TEXT("AutoTest")) : (GetNetMode() == NM_Client ? FString(TEXT("NetTest_Client")) : FString(TEXT("NetTest_Hote")));
	OutDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), Sub));
	IFileManager::Get().DeleteDirectory(*OutDir, false, true);
	IFileManager::Get().MakeDirectory(*OutDir, true);

	Capture = MakeShared<FBRLogCapture>();
	if (GLog)
	{
		GLog->AddOutputDevice(Capture.Get());
	}

	if (bNetTest)
	{
		BuildNetPlan();
		UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] Test multijoueur (%s) : %d etapes. Rapport : %s"), GetNetMode() == NM_Client ? TEXT("client") : TEXT("hote"),
			Plan.Num(), *OutDir);
		return;
	}

	// Niveaux a tester : tous, ou ceux de -BRAutoTestLevels=0,37
	TArray<int32> Levels;
	FString List;
	if (FParse::Value(FCommandLine::Get(), TEXT("BRAutoTestLevels="), List, false))
	{
		TArray<FString> Parts;
		List.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& P : Parts)
		{
			const int32 N = FCString::Atoi(*P);
			if (BRLevels::Exists(N))
			{
				Levels.AddUnique(N);
			}
		}
	}
	if (Levels.Num() == 0)
	{
		for (const FBRLevelDef& D : BRLevels::All())
		{
			Levels.Add(D.Number);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("BRAutoTestV47")))
	{
		// v4.7 : les verifications de non-regression seules
		AddRegressionSteps();
		Add(TEXT("Fin"), 0.f, [this]()
		{
			Finish();
			return true;
		});
		UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] Non-regression v4.7 : %d etapes. Rapport : %s"), Plan.Num(), *OutDir);
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("BRAutoTestPits")))
	{
		// v4.6 : la salle de fosses seule
		AddPitSteps();
		Add(TEXT("Fin"), 0.f, [this]()
		{
			Finish();
			return true;
		});
		UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] Salle de fosses : %d etapes. Captures et rapport : %s"), Plan.Num(), *OutDir);
		return;
	}
	BuildPlan(Levels);
	UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] Debut : %d etapes, %d niveaux. Captures et rapport : %s"), Plan.Num(), Levels.Num(), *OutDir);
}

void ABRAutoTest::EndPlay(const EEndPlayReason::Type Reason)
{
	// Multijoueur : l'hote a ferme la partie avant la fin (le client revient au menu) : on conclut quand meme
	if (bNetTest && !bFinished && Reason != EEndPlayReason::Quit)
	{
		Note(TEXT("la partie s'est terminee avant la fin du test (connexion perdue ?)"), true);
		Finish();
	}
	if (Capture.IsValid() && GLog)
	{
		GLog->RemoveOutputDevice(Capture.Get());
	}
	Super::EndPlay(Reason);
}

int32 ABRAutoTest::LogLineCount() const
{
	return Capture.IsValid() ? Capture->Num() : 0;
}

ABRWorld* ABRAutoTest::GetBRWorld() const
{
	return ABRWorld::Get(this);
}

ABRCharacter* ABRAutoTest::GetPlayer() const
{
	return Cast<ABRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

ABRPlayerController* ABRAutoTest::GetPC() const
{
	return Cast<ABRPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
}

ABRAutoTest::FLevelReport& ABRAutoTest::Report()
{
	if (Reports.Num() == 0)
	{
		Reports.AddDefaulted();
		Reports.Last().Title = TEXT("(demarrage)");
	}
	return Reports.Last();
}

void ABRAutoTest::Note(const FString& Text, bool bProblem)
{
	Report().Notes.Add((bProblem ? TEXT("PROBLEME : ") : TEXT("")) + Text);
	if (bProblem)
	{
		Problems.Add(FString::Printf(TEXT("Niveau %d : %s"), Report().Level, *Text));
	}
	UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] %s%s"), bProblem ? TEXT("PROBLEME : ") : TEXT(""), *Text);
}

void ABRAutoTest::Shot(const FString& Name)
{
	FScreenshotRequest::RequestScreenshot(FPaths::Combine(OutDir, Name + TEXT(".png")), true, false);
}

void ABRAutoTest::Add(const FString& Name, float Wait, TFunction<bool()> Action)
{
	FStep S;
	S.Name = Name;
	S.Wait = Wait;
	S.Action = MoveTemp(Action);
	Plan.Add(MoveTemp(S));
}

// =====================================================================================================================
// Plan
// =====================================================================================================================

void ABRAutoTest::AddLoad(int32 Level, float Settle, const FString& Title)
{
	Add(FString::Printf(TEXT("Niveau %d : chargement"), Level), 0.f, [this, Level, Title]()
	{
		ABRPlayerController* PC = GetPC();
		ABRWorld* W = GetBRWorld();
		if (!PC || !W || !GetPlayer() || W->IsTransitioning())
		{
			return false;
		}
		// Le rapport du niveau commence ici : les avertissements du chargement lui sont attribues
		FLevelReport R;
		R.Level = Level;
		R.Title = Title.IsEmpty() ? BRLevels::Get(Level).Title.ToString() : Title;
		R.FirstLogLine = Capture.IsValid() ? Capture->Num() : 0;
		Reports.Add(R);
		PC->BRLevel(Level); // ferme aussi le menu principal
		return true;
	});
	Add(FString::Printf(TEXT("Niveau %d : attente"), Level), Settle, [this, Level]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != Level)
		{
			return false;
		}
		C->bGodMode = true;
		// Captures comparables d'un niveau a l'autre : sans la desaturation due a la sante mentale qui baisse
		C->Sanity = 100.f;
		C->Health = 100.f;
		return true;
	});
}

void ABRAutoTest::BuildPlan(const TArray<int32>& Levels)
{
	for (const int32 Level : Levels)
	{
		AddLevelSteps(Level);
	}
	if (Levels.Contains(0))
	{
		AddPitSteps(); // v4.6
	}
	AddRegressionSteps(); // v4.7

	// Galerie : toutes les entites dans le bureau eclaire du Niveau 4
	AddLoad(4, 8.f, TEXT("Galerie des entites"));
	for (int32 K = 0; K < static_cast<int32>(EBREntityKind::Count); ++K)
	{
		AddEntityShot(4, static_cast<EBREntityKind>(K));
	}

	// Interface : inventaire, 3e personne
	Add(TEXT("Inventaire"), 1.f, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetInventoryOpen(true);
		}
		return true;
	});
	Add(TEXT("Capture inventaire"), 0.5f, [this]()
	{
		Shot(TEXT("UI_inventaire"));
		return true;
	});
	Add(TEXT("Fermer l'inventaire"), 0.5f, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetInventoryOpen(false);
		}
		return true;
	});
	Add(TEXT("3e personne"), 2.f, [this]()
	{
		if (ABRCharacter* C = GetPlayer())
		{
			C->ToggleThirdPerson();
		}
		return true;
	});
	Add(TEXT("Capture 3e personne"), 0.5f, [this]()
	{
		Shot(TEXT("UI_3e_personne"));
		if (ABRCharacter* C = GetPlayer())
		{
			if (!C->IsThirdPerson())
			{
				Note(TEXT("la vue a la 3e personne ne s'active pas"), true);
			}
		}
		return true;
	});
	Add(TEXT("Retour 1re personne"), 1.f, [this]()
	{
		if (ABRCharacter* C = GetPlayer())
		{
			if (C->IsThirdPerson())
			{
				C->ToggleThirdPerson();
			}
		}
		return true;
	});

	// Mort et reveil (solo : retour au Niveau 0 avec l'equipement de depart)
	Add(TEXT("Mort"), 2.5f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return false;
		}
		C->bGodMode = false;
		C->ReceiveAttack(1000.f, 0.f, nullptr);
		if (!C->IsDead())
		{
			Note(TEXT("le personnage ne meurt pas apres 1000 points de degats"), true);
		}
		return true;
	});
	Add(TEXT("Capture mort"), 0.f, [this]()
	{
		Shot(TEXT("UI_mort"));
		return true;
	});
	Add(TEXT("Reveil"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (StepTime > 25.f)
		{
			Note(TEXT("pas de reveil au Niveau 0 apres la mort (25 s)"), true);
			return true;
		}
		if (W->IsTransitioning() || C->IsDead() || W->GetLevelNumber() != 0)
		{
			return false;
		}
		Note(FString::Printf(TEXT("reveil au Niveau 0 apres %.1f s, sante %.0f, %d objets de depart"), StepTime, C->Health,
			C->CountItem(EBRItem::AlmondWater) + C->CountItem(EBRItem::Bandage) + C->CountItem(EBRItem::Battery)));
		return true;
	});

	Add(TEXT("Fin"), 0.f, [this]()
	{
		Finish();
		return true;
	});
}

// =====================================================================================================================
// Test multijoueur (-BRNetTest) : un hote ("?listen") et un client sur la meme machine
// =====================================================================================================================

void ABRAutoTest::NoteNetState(const TCHAR* When)
{
	ABRWorld* W = GetBRWorld();
	ABRCharacter* C = GetPlayer();
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const TCHAR* NetRole = GetNetMode() == NM_Client ? TEXT("client") : TEXT("hote");
	if (!W || !C)
	{
		Note(FString::Printf(TEXT("%s : monde ou personnage absent"), When), true);
		return;
	}
	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	Note(FString::Printf(TEXT("%s (%s) : niveau %d, graine %u, %d joueur(s), %d chunks, %d entites, position (%.0f, %.0f, %.0f), au sol : %s"), When, NetRole,
		W->GetLevelNumber(), W->GetSeed(), GS ? GS->PlayerArray.Num() : 0, W->GetChunkCount(), W->GetEntities().Num(), C->GetActorLocation().X,
		C->GetActorLocation().Y, C->GetActorLocation().Z, (Move && Move->IsMovingOnGround()) ? TEXT("oui") : TEXT("non")));
	if (C->GetActorLocation().Z < -500.f)
	{
		Note(TEXT("le personnage est tombe sous le sol"), true);
	}
	if (W->HasPits())
	{
		// v4.6 : salles de fosses vues par cette machine (l'hote et le client doivent avoir exactement les memes)
		TArray<FIntRect> Rooms;
		W->GetPitRooms(Rooms, FIntPoint(0, 0), 4);
		FString List;
		for (const FIntRect& R : Rooms)
		{
			int32 H = 0;
			for (int32 X = R.Min.X; X < R.Max.X - 1; ++X)
			{
				for (int32 Y = R.Min.Y; Y < R.Max.Y - 1; ++Y)
				{
					H = H * 2 + (W->HasPitAtCorner(X, Y) ? 1 : 0);
				}
			}
			List += FString::Printf(TEXT(" %d,%d->%d,%d motif %d"), R.Min.X, R.Min.Y, R.Max.X - 1, R.Max.Y - 1, H);
		}
		Note(FString::Printf(TEXT("  salles de fosses :%s"), *List));
	}
	if (W->Def().bWater)
	{
		const UBRWaterSim* Sim = W->GetWaterSim();
		Note(Sim ? FString::Printf(TEXT("  eau simulee autour du joueur (vague max %.2f cm)"), Sim->GetPeak()) : FString(TEXT("  pas de simulation de l'eau")), !Sim);
	}
	// Ou cette machine voit les coequipiers (l'hote et le client doivent etre d'accord)
	TArray<ABRCharacter*> Players;
	W->GetPlayers(Players);
	for (const ABRCharacter* Other : Players)
	{
		if (Other && Other != C)
		{
			Note(FString::Printf(TEXT("  coequipier vu en (%.0f, %.0f, %.0f)"), Other->GetActorLocation().X, Other->GetActorLocation().Y,
				Other->GetActorLocation().Z));
		}
	}
}

void ABRAutoTest::BuildNetPlan()
{
	const bool bClient = GetNetMode() == NM_Client;
	{
		FLevelReport R;
		R.Title = bClient ? TEXT("Multijoueur (client)") : TEXT("Multijoueur (hote)");
		Reports.Add(R);
	}
	// v4.7 : conditions reseau degradees, sur chaque machine : -BRNetLag=150 (ms) -BRNetLoss=5 (% de paquets perdus)
	int32 Lag = 0;
	int32 Loss = 0;
	FParse::Value(FCommandLine::Get(), TEXT("BRNetLag="), Lag);
	FParse::Value(FCommandLine::Get(), TEXT("BRNetLoss="), Loss);
	if (Lag > 0 || Loss > 0)
	{
		Add(TEXT("Reseau degrade"), 0.f, [this, Lag, Loss]()
		{
			if (GEngine && GetWorld())
			{
				GEngine->Exec(GetWorld(), *FString::Printf(TEXT("Net PktLag=%d"), Lag));
				GEngine->Exec(GetWorld(), *FString::Printf(TEXT("Net PktLoss=%d"), Loss));
			}
			Note(FString::Printf(TEXT("reseau simule : latence %d ms, pertes %d %%"), Lag, Loss));
			return true;
		});
	}

	Add(TEXT("Attente de la partie"), 8.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		if (!W || !PC || !GetPlayer() || !W->IsLevelReady() || W->IsTransitioning() || !PC->IsNetGame())
		{
			return false;
		}
		// L'hote attend son coequipier (le client est lance juste apres lui)
		if (!bClient && (!GS || GS->PlayerArray.Num() < 2) && StepTime < 50.f)
		{
			return false;
		}
		GetPlayer()->bGodMode = true;
		return true;
	});
	Add(TEXT("Etat initial"), 0.5f, [this]()
	{
		NoteNetState(TEXT("debut"));
		if (const ABRWorld* W = GetBRWorld())
		{
			NetEntitiesSeen = FMath::Max(NetEntitiesSeen, W->GetEntities().Num());
		}
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		if (!GS || GS->PlayerArray.Num() < 2)
		{
			Note(TEXT("le second joueur n'a pas rejoint la partie"), true);
		}
		Shot(TEXT("Net_debut"));
		return true;
	});

	// L'hote fait apparaitre un Hound devant le client : le client doit le voir (entite simulee par le serveur)
	Add(TEXT("Entite pres du coequipier"), 4.f, [this, bClient]()
	{
		if (bClient)
		{
			return true;
		}
		ABRWorld* W = GetBRWorld();
		TArray<ABRCharacter*> Players;
		if (W)
		{
			W->GetPlayers(Players);
		}
		for (ABRCharacter* Other : Players)
		{
			if (Other && !Other->IsLocallyControlled())
			{
				const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Hound);
				const FVector Spot = Other->GetActorLocation() + Other->GetActorForwardVector() * 400.f;
				const FIntPoint Cell = W->WorldToCell(Spot);
				const FVector Loc = W->IsWalkable(Cell) ? FVector(Spot.X, Spot.Y, Info.HalfHeight + 5.f)
					: W->CellCenter(W->WorldToCell(Other->GetActorLocation()), Info.HalfHeight + 5.f);
				if (ABREntity* E = W->SpawnEntity(EBREntityKind::Hound, Loc))
				{
					E->CustomTimeDilation = 0.05f;
					Note(TEXT("Hound place devant le client"));
				}
				return true;
			}
		}
		Note(TEXT("aucun client trouve pour y placer une entite"), true);
		return true;
	});
	Add(TEXT("Etat avec entite"), 0.5f, [this, bClient]()
	{
		NoteNetState(TEXT("apres apparition"));
		ABRWorld* W = GetBRWorld();
		if (W && W->GetLevelNumber() == 0)
		{
			NetEntitiesSeen = FMath::Max(NetEntitiesSeen, W->GetEntities().Num());
		}
		if (bClient && NetEntitiesSeen == 0)
		{
			Note(TEXT("le client ne voit aucune entite (replication)"), true);
		}
		Shot(TEXT("Net_entite"));
		return true;
	});

	// v4.7 : mort et reanimation en reseau (cause, etat tenu par le serveur, reveil vu des deux cotes)
	AddNetDeathSteps(bClient);

	// L'hote emmene le groupe au Niveau 37 : le client doit suivre avec la meme graine
	Add(TEXT("Changement de niveau"), 0.f, [this, bClient]()
	{
		if (!bClient)
		{
			if (ABRPlayerController* PC = GetPC())
			{
				PC->BRLevel(37);
			}
		}
		return true;
	});
	Add(TEXT("Attente du Niveau 37"), 7.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (StepTime > 45.f)
		{
			Note(TEXT("le groupe n'est pas arrive au Niveau 37 en 45 s"), true);
			return true;
		}
		return W && !W->IsTransitioning() && W->IsLevelReady() && W->GetLevelNumber() == 37;
	});
	Add(TEXT("Etat Niveau 37"), 0.5f, [this]()
	{
		NoteNetState(TEXT("Niveau 37"));
		Shot(TEXT("Net_niveau37"));
		return true;
	});
	// Le client part le premier ; l'hote attend un peu avant de fermer la partie
	Add(TEXT("Fin"), bClient ? 6.f : 12.f, []()
	{
		return true;
	});
	Add(TEXT("Fermeture"), 0.f, [this]()
	{
		Finish();
		return true;
	});
}

void ABRAutoTest::StartMeasure()
{
	bMeasuring = true;
	Frames = 0;
	MeasureTime = 0.f;
	Worst = 0.f;
	GpuMs = GameMs = RenderMs = 0.0;
	FrameMs.Reset();
}

void ABRAutoTest::EndMeasure(FLevelReport& R)
{
	bMeasuring = false;
	R.AvgFPS = MeasureTime > 0.f ? Frames / MeasureTime : 0.f;
	R.WorstMs = Worst * 1000.f;
	CollectStats(R);
	if (R.AvgFPS > 0.f && R.AvgFPS < 30.f)
	{
		Note(FString::Printf(TEXT("images par seconde faibles : %.0f"), R.AvgFPS), true);
	}
	if (Frames > 0)
	{
		Note(FString::Printf(TEXT("temps par image : GPU %.1f ms, jeu %.1f ms, rendu %.1f ms"), GpuMs / Frames, GameMs / Frames, RenderMs / Frames));
		R.GpuMs = static_cast<float>(GpuMs / Frames);
		R.GameMs = static_cast<float>(GameMs / Frames);
		R.RenderMs = static_cast<float>(RenderMs / Frames);
	}
	// v4.5 : fluidite (percentiles), 1 % le plus lent, memoire, construction des chunks, mode de rendu reel
	if (FrameMs.Num() > 0)
	{
		TArray<float> Sorted = FrameMs;
		Sorted.Sort();
		auto Pct = [&Sorted](float P) { return Sorted[FMath::Clamp(FMath::FloorToInt(P * (Sorted.Num() - 1)), 0, Sorted.Num() - 1)]; };
		R.P50Ms = Pct(0.5f);
		R.P95Ms = Pct(0.95f);
		R.P99Ms = Pct(0.99f);
		const int32 NLow = FMath::Max(1, Sorted.Num() / 100);
		float SumLow = 0.f;
		for (int32 i = Sorted.Num() - NLow; i < Sorted.Num(); ++i)
		{
			SumLow += Sorted[i];
		}
		R.Low1FPS = SumLow > 0.f ? 1000.f / (SumLow / NLow) : 0.f;
		Note(FString::Printf(TEXT("fluidite : mediane %.1f ms, 95 %% %.1f ms, 99 %% %.1f ms, 1 %% le plus lent %.0f img/s"), R.P50Ms, R.P95Ms, R.P99Ms,
			R.Low1FPS));
	}
	R.RamMB = static_cast<float>(FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0));
	FTextureMemoryStats TexStats;
	RHIGetTextureMemoryStats(TexStats);
	R.TexMB = static_cast<float>((TexStats.StreamingMemorySize + TexStats.NonStreamingMemorySize) / (1024.0 * 1024.0));
	Note(FString::Printf(TEXT("memoire : processus %.0f Mo, textures %.0f Mo (memoire video dediee %.0f Mo)"), R.RamMB, R.TexMB,
		TexStats.DedicatedVideoMemory / (1024.0 * 1024.0)));
	if (ABRWorld* W = GetBRWorld())
	{
		R.ChunkMaxMs = W->MaxChunkBuildMs;
		R.ChunkAvgMs = W->ChunksBuilt > 0 ? W->ChunkBuildMsTotal / W->ChunksBuilt : 0.f;
		Note(FString::Printf(TEXT("construction des chunks : %d termines, travail moyen %.1f ms par chunk, pire image %.1f ms (budget %.1f ms)"),
			W->ChunksBuilt, R.ChunkAvgMs, R.ChunkMaxMs, W->ChunkStepBudgetMs));
		if (W->ChunksBuilt > 0)
		{
			// v4.7 : temps par etape (moyenne par chunk) et constructions forcees sous un joueur
			const float N = static_cast<float>(W->ChunksBuilt);
			Note(FString::Printf(TEXT("  etapes : planification %.2f ms (pire %.1f), collisions %.2f, visuels %.2f, lumieres %.2f, objets %.2f ; forcees : %d"),
				W->ChunkPlanMsTotal / N, W->MaxChunkPlanMs, W->ChunkCollisionMsTotal / N, W->ChunkVisualMsTotal / N, W->ChunkLightMsTotal / N,
				W->ChunkActorMsTotal / N, W->ForcedChunkBuilds));
		}
		W->ResetChunkStats();
	}
	if (ABRPlayerController* PC = GetPC())
	{
		Note(PC->GetRenderModeText());
	}
	// -BRAutoTestGPU : detail du temps passe par le processeur graphique (dans Saved/Logs/Backrooms.log)
	if (FParse::Param(FCommandLine::Get(), TEXT("BRAutoTestGPU")) && GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
		GEngine->Exec(GetWorld(), TEXT("stat dumpframe -ms=0.4"));
	}
}

void ABRAutoTest::AddLevelSteps(int32 Level)
{
	const FBRLevelDef& D = BRLevels::Get(Level);
	AddLoad(Level, 8.f);
	Add(FString::Printf(TEXT("Niveau %d : mesure"), Level), 4.f, [this]()
	{
		StartMeasure();
		return true;
	});
	Add(FString::Printf(TEXT("Niveau %d : capture"), Level), 1.f, [this, Level]()
	{
		FLevelReport& R = Report();
		EndMeasure(R);
		if (R.Chunks == 0)
		{
			Note(TEXT("aucune salle chargee"), true);
		}
		Shot(FString::Printf(TEXT("L%02d_vue"), Level));
		return true;
	});
	Add(FString::Printf(TEXT("Niveau %d : demi-tour"), Level), 1.5f, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			FRotator R = PC->GetControlRotation();
			R.Yaw += 180.f;
			PC->SetControlRotation(R);
		}
		return true;
	});
	Add(FString::Printf(TEXT("Niveau %d : capture dos"), Level), 0.5f, [this, Level]()
	{
		Shot(FString::Printf(TEXT("L%02d_dos"), Level));
		return true;
	});

	// Entites du niveau (et celle qui fait des rondes)
	TArray<EBREntityKind> Kinds;
	for (const FBREntitySpawn& E : D.Entities)
	{
		Kinds.AddUnique(E.Kind);
	}
	if (D.bPatrolEntity)
	{
		Kinds.AddUnique(D.PatrolKind);
	}
	for (const EBREntityKind K : Kinds)
	{
		AddEntityShot(Level, K);
	}

	// Coupure de courant, lampe, vision nocturne
	const bool bCanBlackout = !D.bOutdoor && D.Fixture != EBRFixture::None;
	if (bCanBlackout && D.bBlackouts)
	{
		Add(FString::Printf(TEXT("Niveau %d : coupure"), Level), 4.5f, [this]()
		{
			if (ABRWorld* W = GetBRWorld())
			{
				W->ForceBlackout();
			}
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : capture coupure"), Level), 0.5f, [this, Level]()
		{
			ABRWorld* W = GetBRWorld();
			if (W && !W->IsBlackout())
			{
				Note(TEXT("la coupure de courant ne s'est pas declenchee"), true);
			}
			else if (W)
			{
				Note(FString::Printf(TEXT("coupure : alimentation %.2f, %d entites"), W->GetPower(), W->GetEntities().Num()));
			}
			Shot(FString::Printf(TEXT("L%02d_coupure"), Level));
			return true;
		});
	}
	if (!D.bOutdoor)
	{
		Add(FString::Printf(TEXT("Niveau %d : lampe"), Level), 1.f, [this]()
		{
			if (ABRCharacter* C = GetPlayer())
			{
				C->ToggleFlashlight();
				if (!C->IsFlashlightOn())
				{
					Note(TEXT("la lampe ne s'allume pas"), true);
				}
			}
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : capture lampe"), Level), 0.3f, [this, Level]()
		{
			Shot(FString::Printf(TEXT("L%02d_lampe"), Level));
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : vision nocturne"), Level), 1.f, [this]()
		{
			if (ABRCharacter* C = GetPlayer())
			{
				C->ToggleFlashlight();
				C->ToggleNightVision();
				if (!C->IsNightVision())
				{
					Note(TEXT("la vision nocturne ne s'active pas"), true);
				}
			}
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : capture vision nocturne"), Level), 0.4f, [this, Level]()
		{
			Shot(FString::Printf(TEXT("L%02d_nuit"), Level));
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : fin vision nocturne"), Level), 0.2f, [this]()
		{
			if (ABRCharacter* C = GetPlayer())
			{
				if (C->IsNightVision())
				{
					C->ToggleNightVision();
				}
			}
			return true;
		});
	}

	// Bassins : on pose le joueur au-dessus de l'eau profonde, il doit se mettre a nager
	if (D.PoolChance > 0.f)
	{
		Add(FString::Printf(TEXT("Niveau %d : nage"), Level), 3.f, [this]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			const FIntPoint Here = W->WorldToCell(C->GetActorLocation());
			for (int32 Ring = 1; Ring < 14; ++Ring)
			{
				for (int32 DX = -Ring; DX <= Ring; ++DX)
				{
					for (int32 DY = -Ring; DY <= Ring; ++DY)
					{
						const FIntPoint Cell(Here.X + DX, Here.Y + DY);
						if ((FMath::Abs(DX) == Ring || FMath::Abs(DY) == Ring) && W->IsPoolCell(Cell.X, Cell.Y)
							&& W->IsChunkLoaded(W->CellToChunk(Cell)))
						{
							C->SetActorLocation(W->CellCenter(Cell, W->Def().WaterHeight), false, nullptr, ETeleportType::TeleportPhysics);
							return true;
						}
					}
				}
			}
			Note(TEXT("aucun bassin profond trouve pres du point de depart"), true);
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : capture nage"), Level), 0.3f, [this, Level]()
		{
			if (ABRCharacter* C = GetPlayer())
			{
				Note(FString::Printf(TEXT("dans le bassin : nage %s, profondeur d'eau %.0f cm"), C->IsSwimming() ? TEXT("oui") : TEXT("NON"),
					C->GetWaterDepth()));
				if (!C->IsSwimming())
				{
					Note(TEXT("le personnage ne nage pas dans un bassin profond"), true);
				}
			}
			Shot(FString::Printf(TEXT("L%02d_nage"), Level));
			return true;
		});
		// Sous l'eau : la surface vue de dessous (reflexion totale sur les cotes)
		Add(FString::Printf(TEXT("Niveau %d : plongee"), Level), 0.5f, [this]()
		{
			if (ABRCharacter* C = GetPlayer())
			{
				C->SetActorLocation(C->GetActorLocation() - FVector(0.f, 0.f, 150.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (ABRPlayerController* PC = GetPC())
			{
				PC->SetControlRotation(FRotator(25.f, PC->GetControlRotation().Yaw, 0.f));
			}
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : capture sous l'eau"), Level), 0.3f, [this, Level]()
		{
			if (ABRCharacter* C = GetPlayer())
			{
				Note(FString::Printf(TEXT("plongee : camera sous l'eau %s"), C->IsUnderwater() ? TEXT("oui") : TEXT("NON")), !C->IsUnderwater());
			}
			Shot(FString::Printf(TEXT("L%02d_sous_eau"), Level));
			return true;
		});
	}

	// Sillage : le joueur traverse l'eau peu profonde, puis se retourne vers les vagues qu'il a laissees
	if (D.bWater)
	{
		Add(FString::Printf(TEXT("Niveau %d : sillage"), Level), 1.8f, [this]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRPlayerController* PC = GetPC();
			if (!W || !C || !PC)
			{
				return false;
			}
			const float Half = C->GetSimpleCollisionHalfHeight();
			C->SetActorLocation(W->SpawnSpot(0, Half), false, nullptr, ETeleportType::TeleportPhysics);
			C->GetMovementComponent()->StopMovementImmediately();
			// Une direction ou l'on peut avancer de deux cellules sans mur ni bassin profond
			const FIntPoint Cell = W->WorldToCell(C->GetActorLocation());
			static const FIntPoint Dirs[] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };
			WalkYaw = 0.f;
			for (const FIntPoint& Dir : Dirs)
			{
				const FIntPoint A = Cell + Dir;
				const FIntPoint B = A + Dir;
				if (W->CanStep(Cell, A) && W->CanStep(A, B) && !W->IsPoolCell(A.X, A.Y) && !W->IsPoolCell(B.X, B.Y))
				{
					WalkYaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Dir.Y), static_cast<float>(Dir.X)));
					break;
				}
			}
			// Vue a la 3e personne, un peu plongeante : le joueur et l'eau qu'il fend
			PC->SetControlRotation(FRotator(-22.f, WalkYaw, 0.f));
			if (!C->IsThirdPerson())
			{
				C->ToggleThirdPerson();
			}
			WalkStart = C->GetActorLocation();
			WalkTime = 2.8f;
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : capture sillage en marchant"), Level), 1.1f, [this, Level]()
		{
			Shot(FString::Printf(TEXT("L%02d_sillage_marche"), Level));
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : regarder le sillage"), Level), 0.5f, [this]()
		{
			WalkTime = 0.f;
			if (ABRCharacter* C = GetPlayer())
			{
				Note(FString::Printf(TEXT("sillage : %.0f cm parcourus dans l'eau"), FVector::Dist2D(WalkStart, C->GetActorLocation())));
				if (C->IsThirdPerson())
				{
					C->ToggleThirdPerson();
				}
			}
			if (ABRPlayerController* PC = GetPC())
			{
				PC->SetControlRotation(FRotator(-30.f, WalkYaw + 180.f, 0.f));
			}
			return true;
		});
		Add(FString::Printf(TEXT("Niveau %d : capture sillage"), Level), 0.3f, [this, Level]()
		{
			const ABRWorld* W = GetBRWorld();
			if (const UBRWaterSim* Sim = W ? W->GetWaterSim() : nullptr)
			{
				Note(FString::Printf(TEXT("eau simulee : plus haute vague %.2f cm, calcul %.2f ms par image"), Sim->GetPeak(), Sim->GetLastTickMs()),
					Sim->GetPeak() < 0.05f);
			}
			else
			{
				Note(TEXT("pas de simulation de l'eau"), true);
			}
			Shot(FString::Printf(TEXT("L%02d_sillage"), Level));
			return true;
		});
	}
}

// =====================================================================================================================
// v4.6 : salle de fosses du Niveau 0
// =====================================================================================================================

void ABRAutoTest::AddPitSteps()
{
	uint32 UserSeed = 0;
	if (!FParse::Value(FCommandLine::Get(), TEXT("BRSeed="), UserSeed) || UserSeed == 0)
	{
		UserSeed = ABRWorld::DemoSeed;
	}
	const uint32 LevelSeed = ABRWorld::SeedFromUser(UserSeed, 0);
	static const TCHAR* const PitProfileNames[] = { TEXT("Performance"), TEXT("Qualite"), TEXT("Cinematique") };

	Add(TEXT("Fosses : chargement"), 0.f, [this, LevelSeed, UserSeed]()
	{
		ABRPlayerController* PC = GetPC();
		ABRWorld* W = GetBRWorld();
		if (!PC || !W || !GetPlayer() || W->IsTransitioning())
		{
			return false;
		}
		FLevelReport R;
		R.Level = 0;
		R.Title = FString::Printf(TEXT("Salle de fosses (BRSeed=%u)"), UserSeed);
		R.Scene = TEXT("fosses");
		R.FirstLogLine = Capture.IsValid() ? Capture->Num() : 0;
		Reports.Add(R);
		if (!bSettingsSaved)
		{
			SavedSettings = FBRSettings::Get();
			bSettingsSaved = true;
		}
		// La transition avec la graine d'abord (une seconde demande pendant le fondu est ignoree), puis BRLevel pour fermer
		// le menu principal s'il est ouvert
		W->RequestTransition(0, false, LevelSeed);
		PC->BRLevel(0);
		return true;
	});
	Add(TEXT("Fosses : attente"), 6.f, [this, LevelSeed]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 0 || W->GetSeed() != LevelSeed)
		{
			return false;
		}
		C->bGodMode = true;
		C->Sanity = 100.f;
		C->Health = 100.f;
		return true;
	});
	Add(TEXT("Fosses : disposition"), 0.2f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		TArray<FIntRect> Rooms;
		W->GetPitRooms(Rooms, FIntPoint(0, 0), 4);
		// Meme presentation que Tools/verify_pitfalls.py : les deux doivent donner les memes salles pour la meme graine
		FString List;
		int32 Holes = 0;
		for (const FIntRect& R : Rooms)
		{
			int32 H = 0;
			for (int32 X = R.Min.X; X < R.Max.X - 1; ++X)
			{
				for (int32 Y = R.Min.Y; Y < R.Max.Y - 1; ++Y)
				{
					H += W->HasPitAtCorner(X, Y) ? 1 : 0;
				}
			}
			Holes += H;
			List += FString::Printf(TEXT(" %d,%d->%d,%d (%d fosses)"), R.Min.X, R.Min.Y, R.Max.X - 1, R.Max.Y - 1, H);
		}
		Note(FString::Printf(TEXT("graine du niveau %u : %d salle(s) de fosses :%s"), W->GetSeed(), Rooms.Num(), *List), Rooms.Num() == 0 || Holes == 0);
		const float Half = C->GetSimpleCollisionHalfHeight();
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			const FVector Spot = W->SpawnSpot(Slot, Half);
			if (W->IsOverPit(Spot, 60.f))
			{
				Note(FString::Printf(TEXT("point d'apparition %d au-dessus d'une fosse"), Slot), true);
			}
		}
		return true;
	});
	Add(TEXT("Fosses : point de vue"), 3.f, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->BRPits();
		}
		return true;
	});
	Add(TEXT("Fosses : capture"), 0.3f, [this]()
	{
		Shot(TEXT("L00_fosses_vue"));
		return true;
	});

	// Cout de la salle dans chaque profil (meme point de vue, meme graine)
	for (int32 P = 0; P < 3; ++P)
	{
		const FString Name = PitProfileNames[P];
		Add(FString::Printf(TEXT("Fosses : profil %s"), *Name), 3.f, [this, P, Name, UserSeed]()
		{
			ABRPlayerController::ApplyGraphicsProfile(P);
			if (ABRPlayerController* PC = GetPC())
			{
				PC->ApplySettings();
			}
			FLevelReport R;
			R.Level = 0;
			R.Title = FString::Printf(TEXT("Fosses - %s (BRSeed=%u)"), *Name, UserSeed);
			R.Scene = TEXT("fosses_") + Name;
			R.FirstLogLine = Capture.IsValid() ? Capture->Num() : 0;
			Reports.Add(R);
			return true;
		});
		Add(FString::Printf(TEXT("Fosses : mesure %s"), *Name), 4.f, [this]()
		{
			StartMeasure();
			return true;
		});
		Add(FString::Printf(TEXT("Fosses : capture %s"), *Name), 0.5f, [this, Name]()
		{
			EndMeasure(Report());
			Shot(TEXT("L00_fosses_") + Name);
			return true;
		});
	}
	// Lumen logiciel (profil Qualite sans ray tracing materiel) : ombres, exposition, fuites de lumiere dans les puits
	Add(TEXT("Fosses : Lumen logiciel"), 3.f, [this]()
	{
		ABRPlayerController::ApplyGraphicsProfile(1);
		FBRSettings::Get().bHardwareRT = false;
		FBRSettings::Get().GraphicsProfile = 3;
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
			Note(TEXT("Lumen logiciel : ") + PC->GetRenderModeText(true));
		}
		return true;
	});
	Add(TEXT("Fosses : capture Lumen logiciel"), 0.3f, [this]()
	{
		Shot(TEXT("L00_fosses_Lumen_logiciel"));
		ABRPlayerController::ApplyGraphicsProfile(1);
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		return true;
	});

	// Lampe torche plongee dans la fosse la plus proche (les parois doivent s'eclairer, le fond rester noir)
	Add(TEXT("Fosses : lampe"), 1.2f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!W || !C || !PC)
		{
			return false;
		}
		const float S = W->CellSize();
		const FIntPoint Here = W->WorldToCell(C->GetActorLocation());
		float Best = TNumericLimits<float>::Max();
		FVector Hole = FVector::ZeroVector;
		for (int32 DX = -3; DX <= 3; ++DX)
		{
			for (int32 DY = -3; DY <= 3; ++DY)
			{
				if (W->HasPitAtCorner(Here.X + DX, Here.Y + DY))
				{
					const FVector P((Here.X + DX + 1) * S, (Here.Y + DY + 1) * S, 0.f);
					const float D = static_cast<float>(FVector::Dist2D(P, C->GetActorLocation()));
					if (D < Best)
					{
						Best = D;
						Hole = P;
					}
				}
			}
		}
		if (Best == TNumericLimits<float>::Max())
		{
			Note(TEXT("aucune fosse pres du point de vue"), true);
			return true;
		}
		// Au croisement de passages le plus proche de cette fosse, regard plonge vers son centre
		const FVector Stand = W->CellCenter(W->WorldToCell(Hole - FVector(S * 0.5f, S * 0.5f, 0.f)), C->GetSimpleCollisionHalfHeight() + 5.f);
		C->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation((Hole - FVector(0.f, 0.f, 350.f) - C->GetEyeLocation()).Rotation());
		if (!C->IsFlashlightOn())
		{
			C->ToggleFlashlight();
		}
		return true;
	});
	Add(TEXT("Fosses : capture lampe"), 0.3f, [this]()
	{
		Shot(TEXT("L00_fosses_lampe"));
		if (ABRCharacter* C = GetPlayer())
		{
			if (C->IsFlashlightOn())
			{
				C->ToggleFlashlight();
			}
		}
		return true;
	});

	// IA : une Bacteria (entite terrestre) apparait a un coin de la salle et poursuit le joueur place au coin oppose
	Add(TEXT("Fosses : IA"), 0.2f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		TArray<FIntRect> Rooms;
		W->GetPitRooms(Rooms, W->CellToChunk(W->WorldToCell(C->GetActorLocation())), 4);
		if (Rooms.Num() == 0)
		{
			Note(TEXT("IA : pas de salle de fosses"), true);
			return true;
		}
		const FIntRect R = Rooms[0];
		const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Bacteria);
		C->bGodMode = true;
		C->SetActorLocation(W->CellCenter(FIntPoint(R.Max.X - 1, R.Max.Y - 1), C->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr,
			ETeleportType::TeleportPhysics);
		ABREntity* E = W->SpawnEntity(EBREntityKind::Bacteria, W->CellCenter(R.Min, Info.HalfHeight + 5.f));
		if (!E)
		{
			Note(TEXT("IA : echec de l'apparition de la Bacteria"), true);
			return true;
		}
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetControlRotation((E->GetActorLocation() - C->GetEyeLocation()).Rotation());
		}
		PitEntity = E;
		bPitTrack = true;
		PitMinZ = static_cast<float>(E->GetActorLocation().Z);
		PitOverFrames = 0;
		PitStartDist = static_cast<float>(FVector::Dist2D(E->GetActorLocation(), C->GetActorLocation()));
		PitMinDist = PitStartDist;
		return true;
	});
	Add(TEXT("Fosses : poursuite"), 14.f, []()
	{
		return true; // (suivi image par image dans Tick)
	});
	Add(TEXT("Fosses : bilan IA"), 0.2f, [this]()
	{
		bPitTrack = false;
		ABREntity* E = PitEntity.Get();
		Note(FString::Printf(TEXT("IA : Bacteria %s, point le plus bas Z = %.0f cm, %d image(s) au-dessus d'une fosse, distance au joueur %.0f -> %.0f cm"),
			E ? TEXT("toujours la") : TEXT("DISPARUE"), PitMinZ, PitOverFrames, PitStartDist, PitMinDist), !E || PitMinZ < -30.f || PitOverFrames > 0);
		if (PitMinDist > PitStartDist * 0.6f)
		{
			Note(TEXT("IA : la Bacteria s'est peu rapprochee (ne voit pas le joueur, ou bloquee au bord ?) : a regarder sur la capture"));
		}
		Shot(TEXT("L00_fosses_IA"));
		if (E)
		{
			E->Destroy();
		}
		PitEntity.Reset();
		return true;
	});

	// Chute : le joueur marche d'un croisement de passages vers le centre d'une fosse (en diagonale) ; le serveur constate
	// la chute sous le bord et le tue par le systeme existant ; le corps s'arrete au fond ; reveil hors des fosses
	Add(TEXT("Fosses : chute"), 0.1f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!W || !C || !PC)
		{
			return false;
		}
		const FIntPoint Here = W->WorldToCell(C->GetActorLocation());
		const float S = W->CellSize();
		for (int32 k = 0; k < 4; ++k)
		{
			const int32 DX = (k & 1) ? 0 : -1;
			const int32 DY = (k & 2) ? 0 : -1;
			if (W->HasPitAtCorner(Here.X + DX, Here.Y + DY))
			{
				const FVector Hole((Here.X + DX + 1) * S, (Here.Y + DY + 1) * S, 0.f);
				C->bGodMode = false;
				C->SetActorLocation(W->CellCenter(Here, C->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
				const FVector To = Hole - C->GetActorLocation();
				WalkYaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(To.Y), static_cast<float>(To.X)));
				PC->SetControlRotation(FRotator(-35.f, WalkYaw, 0.f));
				WalkTime = 4.f;
				FallStart = FPlatformTime::Seconds();
				return true;
			}
		}
		Note(TEXT("chute : aucune fosse au coin de la cellule du joueur"), true);
		return true;
	});
	Add(TEXT("Fosses : attente de la chute"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		const ABRWorld* W = GetBRWorld();
		if (!C || !W)
		{
			return false;
		}
		if (C->IsDead())
		{
			WalkTime = 0.f;
			// v4.7 : cause explicite tenue par le serveur, et personne ne peut relever un joueur au fond d'une fosse
			const FBRDeathState& DS = C->GetDeathState();
			const bool bFallOk = C->DiedInPit() && DS.bDead && DS.Cause == static_cast<uint8>(EBRDeathCause::Fall) && !DS.bRevivable;
			Note(FString::Printf(TEXT("chute : mort %.1f s apres le premier pas, a Z = %.0f cm, cause : %s, etat serveur : %s"), FPlatformTime::Seconds() - FallStart,
				C->GetActorLocation().Z, BRDeath::CauseId(C->GetDeathCause()), bFallOk ? TEXT("chute, non relevable (OK)") : TEXT("INCORRECT")), !bFallOk);
			return true;
		}
		if (StepTime > 7.f)
		{
			WalkTime = 0.f;
			Note(FString::Printf(TEXT("chute : toujours vivant apres 7 s (Z = %.0f cm)"), C->GetActorLocation().Z), true);
			return true;
		}
		return false;
	});
	Add(TEXT("Fosses : capture chute"), 1.5f, [this]()
	{
		Shot(TEXT("L00_fosses_chute"));
		return true;
	});
	Add(TEXT("Fosses : fond"), 0.f, [this]()
	{
		const ABRCharacter* C = GetPlayer();
		const ABRWorld* W = GetBRWorld();
		if (!C || !W)
		{
			return false;
		}
		const float Z = static_cast<float>(C->GetActorLocation().Z);
		Note(FString::Printf(TEXT("chute : corps a Z = %.0f cm (fond a %.0f cm)"), Z, -W->Def().PitDepth), Z < -(W->Def().PitDepth + 200.f));
		return true;
	});
	Add(TEXT("Fosses : reveil"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (StepTime > 25.f)
		{
			Note(TEXT("pas de reveil apres la chute (25 s)"), true);
			return true;
		}
		if (W->IsTransitioning() || C->IsDead() || !W->IsLevelReady())
		{
			return false;
		}
		const FVector L = C->GetActorLocation();
		Note(FString::Printf(TEXT("reveil au Niveau %d apres %.1f s, a (%.0f, %.0f, %.0f)"), W->GetLevelNumber(), StepTime, L.X, L.Y, L.Z),
			W->IsOverPit(L, 60.f) || L.Z < 0.f);
		// Reglages du joueur retablis
		if (bSettingsSaved)
		{
			FBRSettings::Get() = SavedSettings;
			bSettingsSaved = false;
			if (ABRPlayerController* PC = GetPC())
			{
				PC->ApplySettings();
			}
		}
		C->bGodMode = true;
		return true;
	});
}

void ABRAutoTest::AddEntityShot(int32 Level, EBREntityKind Kind)
{
	const FString* Source = FTextInspector::GetSourceString(ABREntity::Info(Kind).Name);
	const FString Name = (Source ? *Source : FString::FromInt(static_cast<int32>(Kind))).Replace(TEXT("-"), TEXT(""));
	Add(FString::Printf(TEXT("Niveau %d : entite %s"), Level, *Name), 1.2f, [this, Kind, Name]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const FBREntityInfo& Info = ABREntity::Info(Kind);
		FVector Spot;
		if (!FindSpotInFront(450.f, Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f, Spot))
		{
			Note(FString::Printf(TEXT("pas de place pour faire apparaitre : %s"), *Name), true);
			return true;
		}
		ABREntity* E = W->SpawnEntity(Kind, Spot);
		if (!E)
		{
			Note(FString::Printf(TEXT("echec de l'apparition : %s"), *Name), true);
			return true;
		}
		// Face au joueur, et le joueur face a elle
		FVector ToPlayer = C->GetActorLocation() - Spot;
		ToPlayer.Z = 0.f;
		E->SetActorRotation(ToPlayer.Rotation());
		if (ABRPlayerController* PC = GetPC())
		{
			const FVector ToEntity = (Spot + FVector(0.f, 0.f, Info.bFlying ? 0.f : Info.HalfHeight * 0.4f)) - C->GetEyeLocation();
			PC->SetControlRotation(ToEntity.Rotation());
		}
		// Presque figee (l'IA tourne, mais au ralenti) : elle reste dans le cadre le temps de la capture
		E->CustomTimeDilation = 0.02f;
		LastEntity = E;
		return true;
	});
	Add(FString::Printf(TEXT("Niveau %d : capture %s"), Level, *Name), 0.4f, [this, Name, Kind]()
	{
		ABREntity* E = LastEntity.Get();
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (E && C && PC)
		{
			const FBREntityInfo& Info = ABREntity::Info(Kind);
			const FVector Aim = E->GetActorLocation() + FVector(0.f, 0.f, Info.bFlying ? 0.f : Info.HalfHeight * 0.3f);
			PC->SetControlRotation((Aim - C->GetEyeLocation()).Rotation());
			if (E->IsVanishing())
			{
				Note(FString::Printf(TEXT("%s : disparue avant la capture (lumiere trop forte ?)"), *Name));
			}
		}
		else
		{
			Note(FString::Printf(TEXT("%s : detruite avant la capture"), *Name));
		}
		return true;
	});
	Add(FString::Printf(TEXT("Niveau %d : photo %s"), Level, *Name), 0.4f, [this, Level, Name]()
	{
		Shot(FString::Printf(TEXT("L%02d_E_%s"), Level, *Name));
		return true;
	});
	Add(FString::Printf(TEXT("Niveau %d : retrait %s"), Level, *Name), 0.2f, [this]()
	{
		if (ABREntity* E = LastEntity.Get())
		{
			E->Destroy();
		}
		LastEntity.Reset();
		return true;
	});
}

// =====================================================================================================================
// Execution
// =====================================================================================================================

void ABRAutoTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished)
	{
		return;
	}
	const float RealDt = static_cast<float>(FApp::GetDeltaTime());
	if (bMeasuring && RealDt > 0.f)
	{
		++Frames;
		MeasureTime += RealDt;
		Worst = FMath::Max(Worst, RealDt);
		GpuMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
		FrameMs.Add(RealDt * 1000.f);
		GameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
		RenderMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
	}
	if (!Plan.IsValidIndex(StepIndex))
	{
		Finish();
		return;
	}
	if (bPitTrack)
	{
		// v4.6 : l'entite qui traverse la salle de fosses ne doit jamais passer au-dessus du vide
		const ABREntity* E = PitEntity.Get();
		const ABRWorld* W = GetBRWorld();
		const ABRCharacter* C = GetPlayer();
		if (E && W && C)
		{
			const FVector EL = E->GetActorLocation();
			PitMinZ = FMath::Min(PitMinZ, static_cast<float>(EL.Z));
			PitOverFrames += W->IsOverPit(EL) ? 1 : 0;
			PitMinDist = FMath::Min(PitMinDist, static_cast<float>(FVector::Dist2D(EL, C->GetActorLocation())));
		}
	}
	if (WalkTime > 0.f)
	{
		WalkTime -= RealDt;
		if (ABRCharacter* C = GetPlayer())
		{
			C->AddMovementInput(FRotator(0.f, WalkYaw, 0.f).Vector(), 1.f);
		}
	}
	StepTime += RealDt;
	FStep& S = Plan[StepIndex];
	if (!bActionDone)
	{
		if (S.Action())
		{
			bActionDone = true;
			WaitLeft = S.Wait;
		}
		else if (StepTime > 60.f)
		{
			Note(FString::Printf(TEXT("etape bloquee plus de 60 s : %s"), *S.Name), true);
			bActionDone = true;
			WaitLeft = 0.f;
		}
		return;
	}
	WaitLeft -= RealDt;
	if (WaitLeft > 0.f)
	{
		return;
	}
	if (Reports.Num() > 0 && Capture.IsValid())
	{
		Reports.Last().LastLogLine = Capture->Num();
	}
	++StepIndex;
	StepTime = 0.f;
	bActionDone = false;
}

bool ABRAutoTest::FindSpotInFront(float Dist, float Z, FVector& Out) const
{
	const ABRWorld* W = GetBRWorld();
	const ABRCharacter* C = GetPlayer();
	if (!W || !C || !GetWorld())
	{
		return false;
	}
	const FVector Eye = C->GetEyeLocation();
	const float BaseYaw = C->GetAimRotation().Yaw;
	const float Yaws[] = { 0.f, 30.f, -30.f, 60.f, -60.f, 90.f, -90.f, 135.f, -135.f, 180.f };
	const float Dists[] = { Dist, Dist * 0.75f, Dist * 0.5f };
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRAutoTestSpot), false, C);
	for (const float D : Dists)
	{
		for (const float Y : Yaws)
		{
			const FVector Dir = FRotator(0.f, BaseYaw + Y, 0.f).Vector();
			const FIntPoint Cell = W->WorldToCell(C->GetActorLocation() + Dir * D);
			if (!W->IsWalkable(Cell) || W->IsPoolCell(Cell.X, Cell.Y) || !W->IsChunkLoaded(W->CellToChunk(Cell)))
			{
				continue;
			}
			const FVector P = W->CellCenter(Cell, Z);
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, P, ECC_Visibility, Q))
			{
				Out = P;
				return true;
			}
		}
	}
	return false;
}

void ABRAutoTest::CollectStats(FLevelReport& R) const
{
	UWorld* World = GetWorld();
	const ABRWorld* W = GetBRWorld();
	if (!World || !W)
	{
		return;
	}
	R.Chunks = W->GetChunkCount();
	for (TActorIterator<ABRPickup> It(World); It; ++It)
	{
		++R.Pickups;
	}
	for (TActorIterator<ABRExit> It(World); It; ++It)
	{
		++R.Exits;
	}
	R.Entities = W->GetEntities().Num();
	for (TObjectIterator<ULocalLightComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && It->IsRegistered() && It->IsVisible() && It->GetOwner() != GetPlayer())
		{
			++R.Lights;
			R.ShadowLights += It->CastShadows ? 1 : 0;
		}
	}
}

void ABRAutoTest::WriteReport()
{
	TArray<FString> L;
	L.Add(TEXT("THE BACKROOMS - RAPPORT DU TEST AUTOMATIQUE"));
	L.Add(FString::Printf(TEXT("Date : %s   Duree : %.0f s"), *FDateTime::Now().ToString(), FPlatformTime::Seconds() - StartTime));
	L.Add(FString::Printf(TEXT("Moteur : %s   RHI : %s"), FApp::GetBuildVersion(), *FApp::GetGraphicsRHI()));
	// v4.5 : machine, profil et mode de rendu reellement actif (a joindre a toute comparaison avant / apres)
	L.Add(FString::Printf(TEXT("Processeur : %s   Carte graphique : %s   Memoire : %.0f Go"), *FPlatformMisc::GetCPUBrand().TrimStartAndEnd(),
		*FPlatformMisc::GetPrimaryGPUBrand(), FPlatformMemory::GetConstants().TotalPhysical / (1024.0 * 1024.0 * 1024.0)));
	{
		const FBRSettings& S = FBRSettings::Get();
		static const TCHAR* Profiles[] = { TEXT("Performance"), TEXT("Qualite"), TEXT("Cinematique"), TEXT("Personnalise") };
		uint32 FixedSeed = 0;
		FParse::Value(FCommandLine::Get(), TEXT("BRSeed="), FixedSeed);
		L.Add(FString::Printf(TEXT("Profil : %s (qualite %d, RT %d, hit lighting %d, ombres RT lampe %d, rendu %d %%)   Graine fixe : %s"),
			Profiles[FMath::Clamp(S.GraphicsProfile, 0, 3)], S.Quality, S.bHardwareRT ? 1 : 0, S.bRTHitLighting ? 1 : 0, S.bRTShadows ? 1 : 0,
			S.RenderScale, FixedSeed ? *FString::Printf(TEXT("%u"), FixedSeed) : TEXT("non (-BRSeed=<n>)")));
	}
	if (ABRPlayerController* PC = GetPC())
	{
		L.Add(PC->GetRenderModeText());
	}
	L.Add(TEXT(""));
	L.Add(Problems.Num() == 0 ? FString(TEXT("RESULTAT : aucun probleme detecte.")) : FString::Printf(TEXT("RESULTAT : %d probleme(s)"), Problems.Num()));
	for (const FString& P : Problems)
	{
		L.Add(TEXT("  - ") + P);
	}
	L.Add(TEXT(""));
	L.Add(TEXT("Niveau | Titre                     | img/s | 1% bas | med (ms) | 99% (ms) | pire (ms) | GPU (ms) | jeu (ms) | rendu (ms) | chunks | chunk max (ms) | lumieres (ombres) | entites | RAM (Mo) | tex (Mo)"));
	for (const FLevelReport& R : Reports)
	{
		if (R.Chunks == 0 && R.AvgFPS <= 0.f)
		{
			continue;
		}
		L.Add(FString::Printf(TEXT("%6d | %-25s | %5.0f | %6.0f | %8.1f | %8.1f | %9.1f | %8.1f | %8.1f | %10.1f | %6d | %14.1f | %8d (%d) | %7d | %8.0f | %8.0f"),
			R.Level, *R.Title.Left(25), R.AvgFPS, R.Low1FPS, R.P50Ms, R.P99Ms, R.WorstMs, R.GpuMs, R.GameMs, R.RenderMs, R.Chunks, R.ChunkMaxMs, R.Lights,
			R.ShadowLights, R.Entities, R.RamMB, R.TexMB));
	}
	// Tableur : Saved/AutoTest/Mesures.csv (une ligne par niveau)
	{
		TArray<FString> Csv;
		Csv.Add(TEXT("niveau;img_s;bas_1pct;mediane_ms;p95_ms;p99_ms;pire_ms;gpu_ms;jeu_ms;rendu_ms;chunks;chunk_max_ms;chunk_moy_ms;lumieres;ombres;entites;ram_mo;tex_mo;scene"));
		for (const FLevelReport& R : Reports)
		{
			if (R.Chunks == 0 && R.AvgFPS <= 0.f)
			{
				continue;
			}
			Csv.Add(FString::Printf(TEXT("%d;%.1f;%.1f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%d;%.2f;%.2f;%d;%d;%d;%.0f;%.0f;%s"), R.Level, R.AvgFPS, R.Low1FPS, R.P50Ms,
				R.P95Ms, R.P99Ms, R.WorstMs, R.GpuMs, R.GameMs, R.RenderMs, R.Chunks, R.ChunkMaxMs, R.ChunkAvgMs, R.Lights, R.ShadowLights, R.Entities, R.RamMB,
				R.TexMB, *R.Scene));
		}
		FFileHelper::SaveStringArrayToFile(Csv, *FPaths::Combine(OutDir, TEXT("Mesures.csv")), FFileHelper::EEncodingOptions::ForceUTF8);
	}
	for (const FLevelReport& R : Reports)
	{
		L.Add(TEXT(""));
		L.Add(FString::Printf(TEXT("=== Niveau %d - %s"), R.Level, *R.Title));
		for (const FString& N : R.Notes)
		{
			L.Add(TEXT("  ") + N);
		}
		if (Capture.IsValid())
		{
			const TArray<FString> Logs = Capture->Range(R.FirstLogLine, R.LastLogLine);
			if (Logs.Num() > 0)
			{
				L.Add(FString::Printf(TEXT("  Journal (%d avertissements / erreurs uniques) :"), Logs.Num()));
				for (int32 i = 0; i < FMath::Min(Logs.Num(), 25); ++i)
				{
					L.Add(TEXT("    ") + Logs[i].Left(400));
				}
			}
		}
	}
	const FString Path = FPaths::Combine(OutDir, TEXT("Rapport.txt"));
	FFileHelper::SaveStringArrayToFile(L, *Path, FFileHelper::EEncodingOptions::ForceUTF8);
	UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] Rapport ecrit : %s (%d probleme(s))"), *Path, Problems.Num());
}

void ABRAutoTest::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	if (Reports.Num() > 0 && Capture.IsValid())
	{
		Reports.Last().LastLogLine = Capture->Num();
	}
	WriteReport();
	if (!FParse::Param(FCommandLine::Get(), TEXT("BRAutoTestStay")))
	{
		FPlatformMisc::RequestExit(false, TEXT("BRAutoTest"));
	}
}
