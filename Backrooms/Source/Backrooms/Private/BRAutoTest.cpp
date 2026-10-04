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
		R.Title = Title.IsEmpty() ? BRLevels::Get(Level).Title : Title;
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
		C->ReceiveAttack(1000.f, 0.f, nullptr, TEXT("le test automatique"));
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

void ABRAutoTest::AddLevelSteps(int32 Level)
{
	const FBRLevelDef& D = BRLevels::Get(Level);
	AddLoad(Level, 8.f);
	Add(FString::Printf(TEXT("Niveau %d : mesure"), Level), 4.f, [this]()
	{
		bMeasuring = true;
		Frames = 0;
		MeasureTime = 0.f;
		Worst = 0.f;
		GpuMs = GameMs = RenderMs = 0.0;
		return true;
	});
	Add(FString::Printf(TEXT("Niveau %d : capture"), Level), 1.f, [this, Level]()
	{
		bMeasuring = false;
		FLevelReport& R = Report();
		R.AvgFPS = MeasureTime > 0.f ? Frames / MeasureTime : 0.f;
		R.WorstMs = Worst * 1000.f;
		CollectStats(R);
		if (R.Chunks == 0)
		{
			Note(TEXT("aucune salle chargee"), true);
		}
		if (R.AvgFPS > 0.f && R.AvgFPS < 30.f)
		{
			Note(FString::Printf(TEXT("images par seconde faibles : %.0f"), R.AvgFPS), true);
		}
		if (Frames > 0)
		{
			Note(FString::Printf(TEXT("temps par image : GPU %.1f ms, jeu %.1f ms, rendu %.1f ms"), GpuMs / Frames, GameMs / Frames, RenderMs / Frames));
		}
		// -BRAutoTestGPU : detail du temps passe par le processeur graphique (dans Saved/Logs/Backrooms.log)
		if (FParse::Param(FCommandLine::Get(), TEXT("BRAutoTestGPU")) && GEngine)
		{
			GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
			GEngine->Exec(GetWorld(), TEXT("stat dumpframe -ms=0.4"));
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

void ABRAutoTest::AddEntityShot(int32 Level, EBREntityKind Kind)
{
	const FString Name = ABREntity::Info(Kind).Name.Replace(TEXT("-"), TEXT(""));
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
	Add(FString::Printf(TEXT("Niveau %d : capture %s"), Level, *Name), 0.4f, [this, Level, Name, Kind]()
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
		GameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
		RenderMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
	}
	if (!Plan.IsValidIndex(StepIndex))
	{
		Finish();
		return;
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
	L.Add(TEXT(""));
	L.Add(Problems.Num() == 0 ? FString(TEXT("RESULTAT : aucun probleme detecte.")) : FString::Printf(TEXT("RESULTAT : %d probleme(s)"), Problems.Num()));
	for (const FString& P : Problems)
	{
		L.Add(TEXT("  - ") + P);
	}
	L.Add(TEXT(""));
	L.Add(TEXT("Niveau | Titre                     | img/s | pire (ms) | chunks | lumieres (ombres) | objets | sorties | entites"));
	for (const FLevelReport& R : Reports)
	{
		if (R.Chunks == 0 && R.AvgFPS <= 0.f)
		{
			continue;
		}
		L.Add(FString::Printf(TEXT("%6d | %-25s | %5.0f | %9.1f | %6d | %8d (%d) | %6d | %7d | %7d"), R.Level, *R.Title.Left(25), R.AvgFPS, R.WorstMs,
			R.Chunks, R.Lights, R.ShadowLights, R.Pickups, R.Exits, R.Entities));
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
