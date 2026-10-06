// v4.7 : etapes de non-regression du test automatique (-BRAutoTest, ou seules avec -BRAutoTestV47).
// Chaque etape verifie un defaut reellement corrige en v4.7 :
//   - cause de mort explicite (blessure, noyade dans un bassin de 2,60 m, chute) et regles de reanimation ;
//   - sauvegarde : migration du format 1, fichier illisible, copie de secours, ecritures de fond dans l'ordre,
//     reprise fidele (graine, cassette ramassee, point de reprise), mort en attente, nouvelle graine apres une mort ;
//   - attaque en trois temps : coup esquive en s'eloignant pendant la preparation, coup recu une seule fois ;
//   - streaming pendant un sprint (aucune image sans sol sous les pieds, constructions forcees comptees) ;
//   - sorties et cassettes atteignables sur plusieurs graines du Niveau 0.
// Les fichiers de sauvegarde du test ont leur propre prefixe (BR_AutoTest_) : les parties du joueur ne sont pas touchees.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRWorld.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

namespace
{
	const TCHAR* const TestPrefix = TEXT("BR_AutoTest_");

	const TCHAR* CauseName(EBRDeathCause C)
	{
		switch (C)
		{
		case EBRDeathCause::Injury:
			return TEXT("blessure");
		case EBRDeathCause::Drowning:
			return TEXT("noyade");
		case EBRDeathCause::Fall:
			return TEXT("chute");
		case EBRDeathCause::Madness:
			return TEXT("folie");
		default:
			return TEXT("aucune");
		}
	}

	void DeleteTestFiles()
	{
		for (int32 Slot = 0; Slot < BRSaves::MaxSlots; ++Slot)
		{
			for (const FString& Name : { BRSaves::SlotName(Slot), BRSaves::BackupSlotName(Slot), BRSaves::UnreadableSlotName(Slot),
				BRSaves::UnreadableSlotName(Slot) + TEXT("_Secours"), BRSaves::LegacySlotName(Slot, 1) })
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(Name), false, true, true);
			}
		}
	}
}

void ABRAutoTest::AddSeedLoad(int32 Level, uint32 UserSeed, const FString& Title)
{
	const uint32 LevelSeed = ABRWorld::SeedFromUser(UserSeed, Level);
	Add(FString::Printf(TEXT("%s : chargement"), *Title), 0.f, [this, Level, LevelSeed, Title]()
	{
		ABRPlayerController* PC = GetPC();
		ABRWorld* W = GetBRWorld();
		if (!PC || !W || !GetPlayer() || W->IsTransitioning())
		{
			return false;
		}
		FLevelReport R;
		R.Level = Level;
		R.Title = Title;
		R.Scene = TEXT("v4.7");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		W->RequestTransition(Level, false, LevelSeed);
		PC->BRLevel(Level); // ferme le menu principal s'il est ouvert (la demande avec graine passe la premiere)
		return true;
	});
	Add(FString::Printf(TEXT("%s : attente"), *Title), 2.f, [this, Level, LevelSeed]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != Level || W->GetSeed() != LevelSeed || C->IsDead())
		{
			return false;
		}
		C->bGodMode = true;
		C->Health = 100.f;
		C->Sanity = 100.f;
		return true;
	});
}

void ABRAutoTest::AddRegressionSteps()
{
	// ------------------------------------------------------------------------------------------------ Sauvegardes
	Add(TEXT("v4.7 sauvegarde : preparation"), 0.f, [this]()
	{
		FLevelReport R;
		R.Title = TEXT("v4.7 : sauvegardes (fichiers BR_AutoTest_*)");
		R.Scene = TEXT("v4.7");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		BRSaves::SetTestPrefix(TestPrefix);
		DeleteTestFiles();
		BRSaves::TakeLoadMessages();
		return true;
	});
	Add(TEXT("v4.7 sauvegarde : migration du format 1"), 0.f, [this]()
	{
		// Un fichier v4.6 : ecrit sans passer par BRSaves (Version reste 1, donc absente du fichier comme en v4.6)
		UBRSaveGame* Old = NewObject<UBRSaveGame>();
		Old->SaveName = TEXT("Partie v4.6");
		Old->CurrentLevel = 1;
		Old->Explored = { 0, 1 };
		Old->bHasPlayer = true;
		FBRSavedItem Item;
		Item.Group = 0;
		Item.Index = 0;
		Item.Item = static_cast<uint8>(EBRItem::AlmondWater);
		Item.Count = 2;
		Old->Items.Add(Item);
		Old->Notes.Add(TEXT("note lue"));
		Old->Discovered.Add(2);
		Old->PlayTime = 1234.f;
		Old->Deaths = 3;
		TArray<uint8> Bytes;
		UGameplayStatics::SaveGameToMemory(Old, Bytes);
		FFileHelper::SaveArrayToFile(Bytes, *BRSaves::FilePath(BRSaves::SlotName(0)));
		const UBRSaveGame* L = BRSaves::Load(0);
		const bool bOk = L && L->LoadedVersion == 1 && L->Version == UBRSaveGame::CurrentVersion && L->Items.Num() == 1 && L->Items[0].Count == 2
			&& L->Notes.Num() == 1 && L->Discovered.Num() == 1 && L->Explored.Num() == 2 && L->CurrentLevel == 1 && FMath::IsNearlyEqual(L->PlayTime, 1234.f)
			&& L->Deaths == 3 && !L->Session.bValid && IFileManager::Get().FileExists(*BRSaves::FilePath(BRSaves::LegacySlotName(0, 1)));
		Note(FString::Printf(TEXT("migration du format 1 : %s (format lu %d, objets %d, notes %d, journal %d, niveaux %d, copie d'origine %s)"),
			bOk ? TEXT("OK") : TEXT("ECHEC"), L ? L->LoadedVersion : -1, L ? L->Items.Num() : -1, L ? L->Notes.Num() : -1, L ? L->Discovered.Num() : -1,
			L ? L->Explored.Num() : -1, IFileManager::Get().FileExists(*BRSaves::FilePath(BRSaves::LegacySlotName(0, 1))) ? TEXT("gardee") : TEXT("absente")), !bOk);
		return true;
	});
	Add(TEXT("v4.7 sauvegarde : fichier illisible"), 0.f, [this]()
	{
		TArray<uint8> Garbage;
		Garbage.Init(0x5A, 300);
		FFileHelper::SaveArrayToFile(Garbage, *BRSaves::FilePath(BRSaves::SlotName(1)));
		const UBRSaveGame* L = BRSaves::Load(1);
		const bool bAside = IFileManager::Get().FileExists(*BRSaves::FilePath(BRSaves::UnreadableSlotName(1)));
		const bool bFree = !IFileManager::Get().FileExists(*BRSaves::FilePath(BRSaves::SlotName(1)));
		const TArray<FString> Msgs = BRSaves::TakeLoadMessages();
		const bool bOk = !L && bAside && bFree && Msgs.Num() > 0;
		Note(FString::Printf(TEXT("fichier illisible : %s (mis de cote : %s, emplacement libre : %s, message : %s)"), bOk ? TEXT("OK") : TEXT("ECHEC"),
			bAside ? TEXT("oui") : TEXT("non"), bFree ? TEXT("oui") : TEXT("non"), Msgs.Num() > 0 ? *Msgs[0] : TEXT("aucun")), !bOk);
		return true;
	});
	Add(TEXT("v4.7 sauvegarde : copie de secours"), 0.f, [this]()
	{
		UBRSaveGame* S = NewObject<UBRSaveGame>();
		S->SaveName = TEXT("Secours");
		S->PlayTime = 77.f;
		BRSaves::Write(2, S);
		TArray<uint8> Broken;
		Broken.Init(0x11, 120);
		FFileHelper::SaveArrayToFile(Broken, *BRSaves::FilePath(BRSaves::SlotName(2)));
		const UBRSaveGame* L = BRSaves::Load(2);
		BRSaves::TakeLoadMessages();
		const bool bOk = L && L->bRecovered && L->SaveName == TEXT("Secours") && FMath::IsNearlyEqual(L->PlayTime, 77.f);
		Note(FString::Printf(TEXT("copie de secours : %s"), bOk ? TEXT("OK (fichier principal abime, partie reprise depuis la copie)") : TEXT("ECHEC")), !bOk);
		return true;
	});
	Add(TEXT("v4.7 sauvegarde : ecritures de fond ordonnees"), 0.f, [this]()
	{
		UBRSaveGame* S = NewObject<UBRSaveGame>();
		S->SaveName = TEXT("Ordre");
		for (int32 i = 0; i < 30; ++i)
		{
			S->PlayTime = static_cast<float>(i);
			BRSaves::WriteAsync(3, S);
		}
		BRSaves::Flush();
		const UBRSaveGame* L = BRSaves::Load(3);
		const bool bOk = L && FMath::IsNearlyEqual(L->PlayTime, 29.f) && BRSaves::LastWriteSucceeded();
		Note(FString::Printf(TEXT("30 ecritures de fond : %s (temps de jeu relu %.0f, attendu 29)"), bOk ? TEXT("OK") : TEXT("ECHEC"), L ? L->PlayTime : -1.f),
			!bOk);
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Reprise fidele
	AddSeedLoad(0, 4605, TEXT("v4.7 reprise : Niveau 0, graine 4605"));
	Add(TEXT("v4.7 reprise : partie de test"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return false;
		}
		UBRSaveGame* S = NewObject<UBRSaveGame>(PC);
		S->SaveName = TEXT("Reprise");
		S->Created = FDateTime::Now();
		S->MarkExplored(0);
		BRSaves::Write(4, S);
		BRSaves::ActiveSlot() = 4;
		PC->ActiveSave = S;
		PC->bDevSession = false;
		return true;
	});
	Add(TEXT("v4.7 reprise : ramasser une cassette"), 1.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		ABRPickup* Best = nullptr;
		float BestD = TNumericLimits<float>::Max();
		for (TActorIterator<ABRPickup> It(GetWorld()); It; ++It)
		{
			const float D = static_cast<float>(FVector::DistSquared(It->GetActorLocation(), C->GetActorLocation()));
			if (It->Item == EBRItem::VHSTape && D < BestD)
			{
				Best = *It;
				BestD = D;
			}
		}
		if (!Best)
		{
			Note(TEXT("reprise : aucune cassette chargee pres du depart (graine 4605)"), true);
			return true;
		}
		TestPickupId = Best->Id;
		// v4.11 : l'hote verifie la distance et la ligne de vue : le joueur se place d'abord a cote de la cassette
		C->SetActorLocation(Best->GetActorLocation() + FVector(60.f, 0.f, C->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		Best->Collect(C);
		// Le joueur se tient ensuite a une cellule du depart (point sur), le temps que le point de reprise soit releve
		const FVector Spot = W->CellCenter(FIntPoint(1, 0), C->GetSimpleCollisionHalfHeight() + 5.f);
		C->SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	});
	Add(TEXT("v4.7 reprise : ecriture"), 0.5f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!PC || !W || !C)
		{
			return false;
		}
		PC->WriteActiveSave(true);
		TestSeed = W->GetSeed();
		TestVHS = W->GetVHSFound();
		TestSpot = PC->bHasSafeSpot ? PC->SafeSpot : FVector::ZeroVector;
		const UBRSaveGame* L = BRSaves::Load(4);
		const bool bOk = L && L->Session.bValid && L->Session.Seed == TestSeed && L->Session.VHSFound == TestVHS && L->Session.Collected.Contains(TestPickupId)
			&& L->Session.bHasSpot;
		Note(FString::Printf(TEXT("session ecrite : %s (graine %u, cassettes %d, objets ramasses %d, point de reprise %s)"), bOk ? TEXT("OK") : TEXT("ECHEC"),
			L ? L->Session.Seed : 0u, L ? L->Session.VHSFound : -1, L ? L->Session.Collected.Num() : -1,
			(L && L->Session.bHasSpot) ? *L->Session.Spot.ToString() : TEXT("aucun")), !bOk);
		return true;
	});
	Add(TEXT("v4.7 reprise : fermeture puis reprise"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		const UBRSaveGame* L = BRSaves::Load(4);
		if (!W || !C || !L)
		{
			return false;
		}
		// Le joueur est ailleurs, puis la session relue du disque est reprise comme au lancement du jeu
		C->SetActorLocation(W->CellCenter(FIntPoint(0, 0), C->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		BRSaves::PendingResume() = L->Session;
		W->RequestTransition(L->Session.Level, false, L->Session.Seed);
		return true;
	});
	Add(TEXT("v4.7 reprise : verification"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady())
		{
			return false;
		}
		bool bPickupBack = false;
		for (TActorIterator<ABRPickup> It(GetWorld()); It; ++It)
		{
			bPickupBack = bPickupBack || It->Id == TestPickupId;
		}
		const float Off = static_cast<float>(FVector::Dist2D(C->GetActorLocation(), TestSpot));
		const bool bOk = W->IsResumedSession() && W->GetSeed() == TestSeed && W->GetVHSFound() == TestVHS && !bPickupBack && Off < 200.f;
		Note(FString::Printf(TEXT("reprise : %s (meme graine %s, cassettes %d/%d, cassette ramassee %s, ecart au point de reprise %.0f cm)"),
			bOk ? TEXT("OK") : TEXT("ECHEC"), W->GetSeed() == TestSeed ? TEXT("oui") : TEXT("non"), W->GetVHSFound(), TestVHS,
			bPickupBack ? TEXT("REAPPARUE") : TEXT("absente"), Off), !bOk);
		Shot(TEXT("v47_reprise"));
		return true;
	});
	Add(TEXT("v4.7 reprise : mort pendant la partie"), 0.2f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return false;
		}
		C->bGodMode = false;
		C->ReceiveAttack(500.f, 0.f, nullptr);
		BRSaves::Flush();
		const UBRSaveGame* L = BRSaves::Load(4);
		const FBRDeathState& DS = C->GetDeathState();
		const bool bOk = C->IsDead() && C->GetDeathCause() == EBRDeathCause::Injury && DS.bDead && DS.Cause == static_cast<uint8>(EBRDeathCause::Injury)
			&& DS.bRevivable && L && L->bPendingDeath && !L->Session.bValid;
		Note(FString::Printf(TEXT("mort par blessure : %s (cause %s, relevable %s, mort notee dans la sauvegarde %s, session invalidee %s)"),
			bOk ? TEXT("OK") : TEXT("ECHEC"), CauseName(C->GetDeathCause()), DS.bRevivable ? TEXT("oui") : TEXT("non"),
			(L && L->bPendingDeath) ? TEXT("oui") : TEXT("non"), (L && !L->Session.bValid) ? TEXT("oui") : TEXT("non")), !bOk);
		return true;
	});
	Add(TEXT("v4.7 reprise : reveil apres la mort"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (StepTime > 25.f)
		{
			Note(TEXT("pas de reveil 25 s apres la mort"), true);
			return true;
		}
		if (W->IsTransitioning() || C->IsDead() || !W->IsLevelReady())
		{
			return false;
		}
		// Regle voulue : seul, une mort ramene au Niveau 0 avec une NOUVELLE graine (la reprise ne s'applique pas)
		const bool bOk = W->GetLevelNumber() == 0 && W->GetSeed() != TestSeed && !W->IsResumedSession() && !C->GetDeathState().bDead;
		Note(FString::Printf(TEXT("apres la mort : %s (Niveau %d, nouvelle graine %s, etat serveur vivant %s)"), bOk ? TEXT("OK") : TEXT("ECHEC"),
			W->GetLevelNumber(), W->GetSeed() != TestSeed ? TEXT("oui") : TEXT("non"), C->GetDeathState().bDead ? TEXT("non") : TEXT("oui")), !bOk);
		C->bGodMode = true;
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ActiveSave = nullptr;
		}
		BRSaves::ActiveSlot() = INDEX_NONE;
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Noyade
	AddSeedLoad(37, 4605, TEXT("v4.7 noyade : Poolrooms, graine 4605"));
	Add(TEXT("v4.7 noyade : au fond d'un bassin"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const FIntPoint Here = W->WorldToCell(C->GetActorLocation());
		for (int32 R = 1; R < 24; ++R)
		{
			for (int32 DX = -R; DX <= R; ++DX)
			{
				for (int32 DY = -R; DY <= R; ++DY)
				{
					const FIntPoint Cell(Here.X + DX, Here.Y + DY);
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R || !W->IsPoolCell(Cell.X, Cell.Y) || !W->IsChunkLoaded(W->CellToChunk(Cell)))
					{
						continue;
					}
					C->bGodMode = false;
					C->SetActorLocation(W->CellCenter(Cell, -W->Def().PoolDepth + 110.f), false, nullptr, ETeleportType::TeleportPhysics);
					C->Breath = 2.f;
					return true;
				}
			}
		}
		Note(TEXT("noyade : aucun bassin profond charge pres du depart"), true);
		return true;
	});
	Add(TEXT("v4.7 noyade : attente"), 1.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return false;
		}
		if (StepTime > 20.f)
		{
			Note(FString::Printf(TEXT("noyade : toujours vivant apres 20 s (Z = %.0f cm)"), C->GetActorLocation().Z), true);
			return true;
		}
		if (!C->IsDead())
		{
			C->Breath = FMath::Min(C->Breath, 2.f);
			return false;
		}
		// Le defaut v4.6 : sous Z = -100, toute mort etait annoncee comme une chute dans une fosse, et non relevable
		const FBRDeathState& DS = C->GetDeathState();
		const bool bOk = C->GetDeathCause() == EBRDeathCause::Drowning && !C->DiedInPit() && DS.bRevivable;
		Note(FString::Printf(TEXT("noyade : %s (cause %s, a Z = %.0f cm, annoncee comme une chute : %s, relevable %s)"), bOk ? TEXT("OK") : TEXT("ECHEC"),
			CauseName(C->GetDeathCause()), C->GetActorLocation().Z, C->DiedInPit() ? TEXT("OUI") : TEXT("non"), DS.bRevivable ? TEXT("oui") : TEXT("non")), !bOk);
		Shot(TEXT("v47_noyade"));
		return true;
	});
	Add(TEXT("v4.7 noyade : reveil"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (W->IsTransitioning() || C->IsDead() || !W->IsLevelReady())
		{
			return StepTime > 25.f;
		}
		C->bGodMode = true;
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Attaque en trois temps
	AddSeedLoad(4, 4605, TEXT("v4.7 attaque : Niveau 4"));
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bDodge = Pass == 0;
		const FString Label = bDodge ? TEXT("esquive") : TEXT("coup recu");
		Add(FString::Printf(TEXT("v4.7 attaque (%s) : Hound devant le joueur"), *Label), 0.f, [this, bDodge]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			FVector Spot;
			const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Hound);
			if (!FindSpotInFront(170.f, Info.HalfHeight + 5.f, Spot))
			{
				Note(TEXT("attaque : pas de place devant le joueur"), true);
				return true;
			}
			C->bGodMode = false;
			C->Health = 100.f;
			C->ScareCooldown = bDodge ? 0.f : 99.f; // le second passage ne rejoue pas le jumpscare de la premiere rencontre
			TestHealth = C->Health;
			TestHits = ABREntity::StatHits;
			TestMisses = ABREntity::StatMisses;
			TestWindups = ABREntity::StatWindups;
			TestEntity = W->SpawnEntity(EBREntityKind::Hound, Spot);
			TestStart = C->GetActorLocation();
			return true;
		});
		Add(FString::Printf(TEXT("v4.7 attaque (%s) : preparation"), *Label), 0.f, [this, bDodge]()
		{
			ABREntity* E = TestEntity.Get();
			ABRCharacter* C = GetPlayer();
			ABRWorld* W = GetBRWorld();
			if (!E || !C || !W)
			{
				Note(TEXT("attaque : le Hound a disparu"), true);
				return true;
			}
			if (StepTime > 12.f)
			{
				Note(TEXT("attaque : aucune preparation en 12 s (le Hound ne voit pas le joueur ?)"), true);
				return true;
			}
			if (E->GetAttackPhase() != ABREntity::EAttackPhase::Windup)
			{
				return false;
			}
			if (bDodge)
			{
				// Pendant la preparation, le joueur s'eloigne de 9 m (hors d'atteinte, meme avec la fente)
				const FVector Away = (C->GetActorLocation() - E->GetActorLocation()).GetSafeNormal2D();
				FVector Dest = C->GetActorLocation() + Away * 900.f;
				if (!W->IsWalkable(W->WorldToCell(Dest)))
				{
					Dest = C->GetActorLocation() - Away * 900.f;
				}
				Dest.Z = C->GetActorLocation().Z;
				C->SetActorLocation(Dest, false, nullptr, ETeleportType::TeleportPhysics);
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.7 attaque (%s) : bilan"), *Label), 1.6f, [this, bDodge]()
		{
			ABRCharacter* C = GetPlayer();
			ABREntity* E = TestEntity.Get();
			if (!C)
			{
				return false;
			}
			if (E && E->GetAttackPhase() != ABREntity::EAttackPhase::Recover && E->GetAttackPhase() != ABREntity::EAttackPhase::None && StepTime < 4.f)
			{
				return false;
			}
			const int32 Hits = ABREntity::StatHits - TestHits;
			const int32 Misses = ABREntity::StatMisses - TestMisses;
			const FBREntityInfo& I = ABREntity::Info(EBREntityKind::Hound);
			bool bOk = false;
			if (bDodge)
			{
				bOk = Hits == 0 && Misses >= 1 && FMath::IsNearlyEqual(C->Health, TestHealth);
				Note(FString::Printf(TEXT("attaque esquivee : %s (coups %d, rates %d, sante %.0f -> %.0f)"), bOk ? TEXT("OK") : TEXT("ECHEC"), Hits, Misses, TestHealth,
					C->Health), !bOk);
			}
			else
			{
				const float Delay = ABREntity::StatLastHitDelay;
				const float Lo = I.WindupTime + 0.04f;
				const float Hi = I.WindupTime + 0.05f + I.ImpactWindow + 0.08f;
				bOk = Hits == 1 && C->Health < TestHealth && Delay >= Lo && Delay <= Hi;
				Note(FString::Printf(TEXT("attaque recue : %s (coups %d, sante %.0f -> %.0f, coup %.2f s apres le debut de la preparation, fenetre %.2f a %.2f s)"),
					bOk ? TEXT("OK") : TEXT("ECHEC"), Hits, TestHealth, C->Health, Delay, Lo, Hi), !bOk);
			}
			if (E)
			{
				E->Destroy();
			}
			TestEntity.Reset();
			C->bGodMode = true;
			C->Health = 100.f;
			return true;
		});
	}

	// ------------------------------------------------------------------------------------------------ Sprint et streaming
	AddSeedLoad(1, 4605, TEXT("v4.7 streaming : sprint au Niveau 1"));
	Add(TEXT("v4.7 streaming : sprint de 25 s"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (!bSprintStarted)
		{
			bSprintStarted = true;
			SprintT0 = StepTime;
			W->ResetChunkStats();
			SprintHoles = 0;
			SprintFrames = 0;
			SprintMaxPreparing = 0;
			TestStart = C->GetActorLocation();
			StartMeasure();
		}
		const float T = StepTime - SprintT0;
		// 470 cm/s en ligne droite (vitesse de sprint), a travers les murs : seul le sol sous les pieds compte ici
		FVector P = TestStart + FVector(470.f * T, 0.f, 0.f);
		P.Z = C->GetSimpleCollisionHalfHeight() + 5.f;
		C->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
		const FIntPoint Chunk = W->CellToChunk(W->WorldToCell(P));
		++SprintFrames;
		SprintHoles += W->HasChunkFloor(Chunk) ? 0 : 1;
		SprintMaxPreparing = FMath::Max(SprintMaxPreparing, W->GetPreparingChunkCount());
		if (T < 25.f)
		{
			return false;
		}
		bSprintStarted = false;
		Note(FString::Printf(TEXT("sprint de %.0f m : %d image(s) sans sol sous le joueur sur %d, %d chunk(s) en preparation au plus, %d construction(s) forcee(s)"),
			470.f * 25.f / 100.f, SprintHoles, SprintFrames, SprintMaxPreparing, W->ForcedChunkBuilds), SprintHoles > 0);
		EndMeasure(Report());
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Graines du Niveau 0
	static const uint32 Seeds[] = { 1, 9, 132, 191, 4605 };
	for (const uint32 UserSeed : Seeds)
	{
		AddSeedLoad(0, UserSeed, FString::Printf(TEXT("v4.7 accessibilite : Niveau 0, graine %u"), UserSeed));
		Add(FString::Printf(TEXT("v4.7 accessibilite : graine %u"), UserSeed), 0.f, [this, UserSeed]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			int32 Exits = 0;
			int32 BadExits = 0;
			int32 Tapes = 0;
			int32 BadTapes = 0;
			for (TActorIterator<ABRExit> It(GetWorld()); It; ++It)
			{
				++Exits;
				BadExits += W->IsSafelyReachable(W->WorldToCell(It->GetActorLocation())) ? 0 : 1;
			}
			for (TActorIterator<ABRPickup> It(GetWorld()); It; ++It)
			{
				if (It->Item == EBRItem::VHSTape)
				{
					++Tapes;
					BadTapes += W->IsSafelyReachable(W->WorldToCell(It->GetActorLocation())) ? 0 : 1;
				}
			}
			// (seuls les chunks charges autour du depart sont comptes ; la simulation Tools/verify_pitfalls.py couvre le niveau entier)
			const bool bOk = BadExits == 0 && BadTapes == 0;
			Note(FString::Printf(TEXT("graine %u : %d sortie(s), %d cassette(s) chargees ; hors d'atteinte sans passer par une fosse : %d sortie(s), %d cassette(s) %s"),
				UserSeed, Exits, Tapes, BadExits, BadTapes, bOk ? TEXT("OK") : TEXT("ECHEC")), !bOk);
			return true;
		});
	}

	Add(TEXT("v4.7 : nettoyage"), 0.f, [this]()
	{
		DeleteTestFiles();
		BRSaves::SetTestPrefix(FString());
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		return true;
	});
}

void ABRAutoTest::AddNetDeathSteps(bool bClient)
{
	// L'hote met le client a terre (coup mortel transmis a son personnage, comme une entite) : le client meurt chez lui,
	// annonce la cause, le serveur la verifie et la publie. Puis l'hote le releve.
	Add(TEXT("Reseau : coequipier a terre"), 0.f, [this, bClient]()
	{
		if (bClient)
		{
			// La sante du client est geree chez lui : il quitte le mode invincible du test pour pouvoir tomber
			if (ABRCharacter* Me = GetPlayer())
			{
				Me->bGodMode = false;
			}
			return true;
		}
		if (StepTime < 1.f)
		{
			return false;
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
				// v4.10 : l'hote attend que le client ait vraiment quitte le mode invincible (etat qu'il envoie : NetFlags) et
				// fini de preparer son niveau. Avant : 3 s fixes, trop court quand le client arrive plus tard que l'hote
				if (((Other->NetFlags & 16) != 0 || Other->IsLevelLoading()) && StepTime < 30.f)
				{
					return false;
				}
				Other->bGodMode = false;
				Other->ReceiveAttack(500.f, 0.f, nullptr);
				NetMate = Other;
				return true;
			}
		}
		Note(TEXT("reseau : aucun client a mettre a terre"), true);
		return true;
	});
	Add(TEXT("Reseau : mort vue par le serveur"), 0.5f, [this, bClient]()
	{
		if (bClient)
		{
			// Le client constate sa propre mort (et la cause) de son cote
			const ABRCharacter* Me = GetPlayer();
			if (!Me)
			{
				return false;
			}
			if (!Me->IsDead() && StepTime < 10.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("client : a terre %s, cause %s"), Me->IsDead() ? TEXT("oui") : TEXT("NON"), CauseName(Me->GetDeathCause())),
				!Me->IsDead() || Me->GetDeathCause() != EBRDeathCause::Injury);
			return true;
		}
		const ABRCharacter* Mate = NetMate.Get();
		if (!Mate)
		{
			return true;
		}
		const FBRDeathState& DS = Mate->GetDeathState();
		if (!DS.bDead && StepTime < 10.f)
		{
			return false;
		}
		const bool bOk = DS.bDead && DS.Cause == static_cast<uint8>(EBRDeathCause::Injury) && DS.bRevivable;
		Note(FString::Printf(TEXT("hote : coequipier a terre %s, cause %s, relevable %s, apres %.1f s"), DS.bDead ? TEXT("oui") : TEXT("NON"),
			CauseName(static_cast<EBRDeathCause>(DS.Cause)), DS.bRevivable ? TEXT("oui") : TEXT("non"), StepTime), !bOk);
		return true;
	});
	Add(TEXT("Reseau : reanimation"), 0.f, [this, bClient]()
	{
		if (bClient)
		{
			return true;
		}
		ABRCharacter* Me = GetPlayer();
		ABRCharacter* Mate = NetMate.Get();
		if (!Me || !Mate)
		{
			return true;
		}
		// L'hote se place a cote de son coequipier puis le releve (meme verification que la touche d'interaction)
		Me->SetActorLocation(Mate->GetActorLocation() + FVector(120.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		Me->ServerRevive(Mate);
		return true;
	});
	Add(TEXT("Reseau : releve"), 1.f, [this, bClient]()
	{
		if (bClient)
		{
			const ABRCharacter* Me = GetPlayer();
			if (!Me)
			{
				return false;
			}
			if (Me->IsDead() && StepTime < 12.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("client : releve %s (%.1f s)"), Me->IsDead() ? TEXT("NON") : TEXT("oui"), StepTime), Me->IsDead());
			if (ABRCharacter* Self = GetPlayer())
			{
				Self->bGodMode = true;
			}
			return true;
		}
		const ABRCharacter* Mate = NetMate.Get();
		if (!Mate)
		{
			return true;
		}
		const FBRDeathState& DS = Mate->GetDeathState();
		if (DS.bDead && StepTime < 10.f)
		{
			return false;
		}
		Note(FString::Printf(TEXT("hote : coequipier releve %s (evenement %d)"), DS.bDead ? TEXT("NON") : TEXT("oui"), DS.Event), DS.bDead || DS.Event != 2);
		if (ABRCharacter* M = NetMate.Get())
		{
			M->bGodMode = true;
		}
		return true;
	});
}
