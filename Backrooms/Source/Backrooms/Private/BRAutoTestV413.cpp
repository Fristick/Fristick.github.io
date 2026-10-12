// v4.13 : etapes du test automatique propres a la v4.13 (-BRAutoTest -BRAutoTestV413 ; reseau : -BRNetTest).
// Chaque etape verifie une regle de la v4.13 dans le jeu reel :
//   - Level 0 sans enigme : une seule sortie, la porte de service, posee dans le mur d'enceinte ; chemin du depart a la
//     porte sans fosse ni cachette, continu, marque a la craie ; sur plusieurs graines, le personnage y va A PIED (chemin
//     de grille suivi avec les seules entrees du joueur, comme la v4.12) et la porte est visee par le trace d'interaction ;
//   - histoire (partie de test, session d'histoire) : reveil au premier Level 0, aide d'orientation programmee, porte
//     fermee ; raccourcis refuses par l'hote (Niveaux 1, 37, 2 depuis le Level 0) ; porte ouverte a la touche
//     d'interaction, depart, arrivee au Niveau 1, etape accomplie et sauvegardee ; suite 1 -> Poolrooms -> 2 -> fin du
//     chapitre par les missions resolues en marchant (BRAutoTestV412) ; inscription de L. dans les Poolrooms ; mission
//     accomplie qui le reste au retour ; point de « Continuer » et niveaux d'exploration apres la fin du chapitre ;
//   - migration des anciennes parties par BRSaves::Load (vrais fichiers du format 4) : copie d'origine gardee a
//     l'identique, traversee des Poolrooms organisee (session garee intacte), traversee creditee seulement sur preuve,
//     session du Level 0 sans condition (graine et objets gardes), generation de mission des autres sessions inchangee.
// Reseau : porte fermee chez les deux, ouverte par l'hote pour le groupe (etat recu par le client, battant anime),
// rassemblement, coequipier qui arrive a pied, un seul depart, arrivee commune au Niveau 1.
// Une verification qui ne peut pas etre faite ici est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "BRAutoTestWalk.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRMission.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRStoryLogic.h"
#include "BRWorld.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

namespace BRTestV413
{
	const TCHAR* const SavePrefix = TEXT("BR_AutoTestV413_");
	constexpr int32 StorySlot = 1;
	constexpr int32 MigrationSlot = 2;
	/** Graines du Level 0 parcourues a pied (9 : graine de demonstration) */
	const uint32 DoorSeeds[] = { 9u, 1u, 77u, 4605u, 31337u };
	/** Graine de la partie d'histoire */
	constexpr uint32 StorySeed = 9u;
	/** Bilan des portes (rapport) */
	TArray<FString> DoorLines;
	/** Serie du niveau au moment d'une ouverture de porte (un seul depart attendu) */
	int32 SerialAtOpen = 0;
	/** Porte vue ouverte juste apres la touche d'interaction */
	bool bOpenSeen = false;

	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	void DeleteV413Saves()
	{
		for (int32 Slot = 0; Slot < BRSaves::MaxSlots; ++Slot)
		{
			for (const FString& Name : { BRSaves::SlotName(Slot), BRSaves::BackupSlotName(Slot), BRSaves::UnreadableSlotName(Slot), BRSaves::RecoverySlotName(Slot) })
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(Name), false, true, true);
			}
			for (int32 V = 1; V < UBRSaveGame::CurrentVersion; ++V)
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(BRSaves::LegacySlotName(Slot, V)), false, true, true);
			}
		}
	}

	ABRExit* FindServiceDoor(UWorld* World)
	{
		for (TActorIterator<ABRExit> It(World); It; ++It)
		{
			if (It->Style == EBRExitStyle::ServiceDoor && !It->IsHidden())
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Sorties du Level 0 autres que la porte de service (attendu : aucune) */
	int32 OtherExits(UWorld* World)
	{
		int32 N = 0;
		for (TActorIterator<ABRExit> It(World); It; ++It)
		{
			N += (It->Style != EBRExitStyle::ServiceDoor && !It->IsHidden()) ? 1 : 0;
		}
		return N;
	}

	void FaceAt(ABRCharacter* C, const FVector& Target)
	{
		if (AController* Ctrl = C->GetController())
		{
			Ctrl->SetControlRotation((Target - C->GetEyeLocation()).Rotation());
		}
	}

	FString StoryText(const BRStory::FState& S)
	{
		return FString::Printf(TEXT("atteint %d, etapes 0x%x, evenements 0x%x, fins 0x%x, reprise %d, gare %d"), S.Reached, S.Done, S.Events, S.ChapterEnds, S.Resume,
			S.Parked);
	}

	/** Place devant la porte (vers la piece), a hauteur de capsule */
	FVector DoorGoal(const ABRWorld* W, const ABRCharacter* C, float Dist)
	{
		const FVector Fwd = FRotator(0.f, W->GetServiceDoorYaw(), 0.f).Vector();
		const FVector Foot = W->GetServiceDoorLocation();
		return FVector(Foot.X + Fwd.X * Dist, Foot.Y + Fwd.Y * Dist, Foot.Z + C->GetSimpleCollisionHalfHeight() + 5.f);
	}

	/** Une ancienne partie (format 4, v4.11 / v4.12) ecrite sans passer par BRSaves : bytes ecrits renvoyes */
	TArray<uint8> WriteLegacy(int32 Slot, const TArray<int32>& Explored, int32 Current, uint32 Seed, int32 Gen, const TArray<uint8>& Mission)
	{
		UBRSaveGame* Old = NewObject<UBRSaveGame>();
		Old->Version = 4;
		Old->SaveName = TEXT("Partie v4.12");
		Old->Created = FDateTime::Now();
		Old->PlayTime = 5400.f;
		for (const int32 L : Explored)
		{
			Old->MarkExplored(L);
		}
		Old->CurrentLevel = Current;
		Old->Session.bValid = true;
		Old->Session.Level = Current;
		Old->Session.Seed = Seed;
		Old->Session.GenVersion = Gen;
		Old->Session.Mission = Mission;
		Old->Session.VHSFound = Current == 0 ? 2 : 0;
		TArray<uint8> Bytes;
		UGameplayStatics::SaveGameToMemory(Old, Bytes);
		FFileHelper::SaveArrayToFile(Bytes, *BRSaves::FilePath(BRSaves::SlotName(Slot)));
		return Bytes;
	}
}

// =====================================================================================================================
// Porte de service : chemin, marche reelle, visee ; bUse : la porte est prise (touche d'interaction) et l'arrivee au
// Niveau 1 est attendue
// =====================================================================================================================

void ABRAutoTest::AddServiceDoorWalkSteps(const FString& Tag, bool bUse)
{
	using namespace BRTestV413;
	Add(FString::Printf(TEXT("v4.13 %s : porte et chemin"), *Tag), 0.5f, [this, Tag]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		TestLevel = -1; // les etapes suivantes ne font rien tant que la marche n'a pas commence
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 0)
		{
			if (StepTime > 30.f)
			{
				Note(FString::Printf(TEXT("%s : Level 0 non pret apres 30 s"), *Tag), true);
				return true;
			}
			return false;
		}
		if (!W->HasServiceDoor())
		{
			Note(FString::Printf(TEXT("%s : AUCUNE porte de service"), *Tag), true);
			return true;
		}
		ABRExit* Door = FindServiceDoor(GetWorld());
		if (!Door && StepTime < 15.f)
		{
			return false; // chunk de la porte en construction
		}
		const TArray<FIntPoint>& Path = W->GetServicePath();
		int32 Pits = 0, Gaps = 0, Marks = 0;
		for (int32 I = 0; I < Path.Num(); ++I)
		{
			Pits += W->IsPitRoomCell(Path[I].X, Path[I].Y) ? 1 : 0;
			Gaps += (I > 0 && FMath::Abs(Path[I].X - Path[I - 1].X) + FMath::Abs(Path[I].Y - Path[I - 1].Y) != 1) ? 1 : 0;
			FIntPoint Next;
			Marks += W->IsChalkMarkCell(Path[I].X, Path[I].Y, Next) ? 1 : 0;
		}
		const bool bStart = Path.Num() > 0 && FMath::Abs(Path[0].X) <= 1 && FMath::Abs(Path[0].Y) <= 1;
		const bool bEnd = Path.Num() > 0 && Path.Last() == W->GetServiceDoorCell();
		const int32 Others = OtherExits(GetWorld());
		const bool bOk = Path.Num() >= 2 && Pits == 0 && Gaps == 0 && bStart && bEnd && Door && Others == 0 && !W->IsMissionActive();
		const FString Line = FString::Printf(TEXT("%s : porte en (%d, %d) vers (%d, %d), palier %d, chemin %d pas (%.0f m), marques de L. %d, fosses sur le chemin %d, "
			"discontinuites %d, depart %s, fin devant la porte %s, autres sorties %d, mission %s, porte construite %s"), *Tag, W->GetServiceDoorCell().X,
			W->GetServiceDoorCell().Y, W->GetServiceDoorDir().X, W->GetServiceDoorDir().Y, W->GetServiceDoorTier(), Path.Num() - 1, (Path.Num() - 1) * 3.5f, Marks, Pits, Gaps,
			Oui(bStart), Oui(bEnd), Others, W->IsMissionActive() ? TEXT("ACTIVE") : TEXT("aucune"), Oui(Door != nullptr));
		DoorLines.Add(Line);
		Note(Line, !bOk);
		if (!Door)
		{
			return true;
		}
		BRTestV412::BeginWalk(W, C, DoorGoal(W, C, 130.f), 70.f, false);
		TestLevel = 0;
		return true;
	});
	Add(FString::Printf(TEXT("v4.13 %s : marche jusqu'a la porte"), *Tag), 0.f, [this, Tag]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (TestLevel < 0 || !W || !C)
		{
			return true;
		}
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const bool bBusy = C->GetScareKind() >= 0 || C->IsInputLocked() || C->IsDead() || W->IsTransitioning();
		const BRTestV412::EWalk R = BRTestV412::StepWalk(W, C, Dt, 300.0, bBusy);
		if (R == BRTestV412::EWalk::Moving)
		{
			return false;
		}
		const FString Stats = BRTestV412::WalkStats();
		if (R == BRTestV412::EWalk::Stuck)
		{
			const FIntPoint Cell = W->WorldToCell(C->GetActorLocation());
			Note(FString::Printf(TEXT("%s : porte NON ATTEINTE a pied (cellule %d, %d ; chemin a la cellule %d du trace de L. : %s) : %s"), *Tag, Cell.X, Cell.Y,
				W->GetServicePath().IndexOfByKey(Cell), Oui(W->IsServicePathCell(Cell.X, Cell.Y)), *Stats), true);
			Shot(FString::Printf(TEXT("v413_%s_bloque"), *Tag.Replace(TEXT(" "), TEXT("_"))));
			TestLevel = -1;
			return true;
		}
		Note(FString::Printf(TEXT("%s : porte atteinte a pied : %s"), *Tag, *Stats), BRTestV412::Walk.Teleports > 0);
		TestTimer = FPlatformTime::Seconds();
		return true;
	});
	Add(FString::Printf(TEXT("v4.13 %s : porte visee"), *Tag), 0.f, [this, Tag, bUse]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRExit* Door = FindServiceDoor(GetWorld());
		if (TestLevel < 0 || !W || !C || !Door)
		{
			return true;
		}
		FaceAt(C, Door->GetActorLocation() + FVector(0.f, 0.f, 110.f));
		const double Now = FPlatformTime::Seconds();
		if (Now - TestTimer < 0.15)
		{
			return false;
		}
		if (C->FocusActor.Get() != Door)
		{
			if (Now - TestTimer < 2.0)
			{
				return false;
			}
			const AActor* Seen = C->FocusActor.Get();
			Note(FString::Printf(TEXT("%s : porte PAS VISEE par l'interaction (vise : %s)"), *Tag, Seen ? *Seen->GetName() : TEXT("rien")), true);
			TestLevel = -1;
			return true;
		}
		Note(FString::Printf(TEXT("%s : porte visee par l'interaction, invite \"%s\", fermee %s"), *Tag, *Door->GetPrompt(), Oui(!W->IsServiceDoorOpen())), W->IsServiceDoorOpen());
		Shot(FString::Printf(TEXT("v413_%s_porte"), *Tag.Replace(TEXT(" "), TEXT("_"))));
		if (bUse)
		{
			SerialAtOpen = W->GetLevelSerial();
			C->Interact();
			bOpenSeen = W->IsServiceDoorOpen() || Door->IsServiceDoorOpen();
			TestTimer = Now;
		}
		return true;
	});
	if (!bUse)
	{
		return;
	}
	Add(FString::Printf(TEXT("v4.13 %s : battant et depart"), *Tag), 0.f, [this, Tag]()
	{
		ABRWorld* W = GetBRWorld();
		if (TestLevel < 0 || !W)
		{
			return true;
		}
		const double Elapsed = FPlatformTime::Seconds() - TestTimer;
		ABRExit* Door = FindServiceDoor(GetWorld());
		if (Door && !W->IsTransitioning() && Elapsed < 1.0)
		{
			bOpenSeen |= W->IsServiceDoorOpen();
			return false;
		}
		const float Angle = Door ? Door->GetServiceDoorAngle() : -1.f;
		Note(FString::Printf(TEXT("%s : porte ouverte par l'hote %s, battant a %.0f degres apres 1 s (0 ferme, 95 ouvert)"), *Tag, Oui(bOpenSeen), Angle), !bOpenSeen);
		Shot(FString::Printf(TEXT("v413_%s_ouverture"), *Tag.Replace(TEXT(" "), TEXT("_"))));
		return true;
	});
	Add(FString::Printf(TEXT("v4.13 %s : arrivee au Niveau 1"), *Tag), 1.f, [this, Tag]()
	{
		ABRWorld* W = GetBRWorld();
		if (TestLevel < 0 || !W)
		{
			return true;
		}
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 1) && StepTime < 40.f)
		{
			return false;
		}
		const bool bArrived = W->GetLevelNumber() == 1 && W->IsLevelReady();
		const bool bOne = W->GetLevelSerial() == SerialAtOpen + 1;
		Note(FString::Printf(TEXT("%s : arrivee au Niveau 1 %s, un seul changement de niveau %s (serie %d -> %d)"), *Tag, Oui(bArrived), Oui(bOne), SerialAtOpen,
			W->GetLevelSerial()), !bArrived || !bOne);
		return true;
	});
}

// =====================================================================================================================
// Etapes v4.13 (solo)
// =====================================================================================================================

void ABRAutoTest::AddV413Steps()
{
	using namespace BRTestV413;
	DoorLines.Reset();

	// ---------------------------------------------------------------------------------------------- Porte : plusieurs graines
	for (const uint32 Seed : DoorSeeds)
	{
		AddSeedLoad(0, Seed, FString::Printf(TEXT("v4.13 Level 0, graine %u"), Seed));
		AddServiceDoorWalkSteps(FString::Printf(TEXT("graine %u"), Seed), false);
	}

	// ---------------------------------------------------------------------------------------------- Histoire
	Add(TEXT("v4.13 histoire : nouvelle partie"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		BRSaves::SetTestPrefix(SavePrefix);
		DeleteV413Saves();
		UBRSaveGame* S = NewObject<UBRSaveGame>();
		S->SaveName = TEXT("Histoire v4.13");
		S->Created = FDateTime::Now();
		S->CurrentLevel = 0;
		S->MarkExplored(0);
		S->SetStory(BRStory::FState());
		const bool bWritten = BRSaves::Write(StorySlot, S);
		TestOrigSlot = BRSaves::ActiveSlot();
		TestOrigSave = PC->ActiveSave;
		bTestDevSession = PC->bDevSession;
		BRSaves::ActiveSlot() = StorySlot;
		BRSaves::ReplaySession() = false;
		PC->ActiveSave = S;
		PC->bDevSession = false;
		W->SetStory(S->GetStory(), EBRSessionMode::Story);
		W->RequestTransition(0, false, ABRWorld::SeedFromUser(StorySeed, 0));
		Note(FString::Printf(TEXT("partie d'histoire de test ecrite %s (emplacement %d, format %d)"), Oui(bWritten), StorySlot, UBRSaveGame::CurrentVersion), !bWritten);
		return true;
	});
	Add(TEXT("v4.13 histoire : reveil au Level 0"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 0 || W->GetSeed() != ABRWorld::SeedFromUser(StorySeed, 0))
		{
			return StepTime > 40.f;
		}
		C->bGodMode = true;
		const float Hint = W->GetDraftHintRemaining();
		const bool bOk = W->IsStorySession() && W->HasStoryEvent(BRStory::Event::IntroWake) && W->GetIntroElapsed() >= 0.f && !W->IsServiceDoorOpen()
			&& Hint > 0.f && Hint <= 420.f && !W->IsMissionActive();
		Note(FString::Printf(TEXT("premier Level 0 : session d'histoire %s, reveil %s (depuis %.1f s), porte fermee %s, aide d'orientation dans %.0f s, mission %s ; %s"),
			Oui(W->IsStorySession()), Oui(W->HasStoryEvent(BRStory::Event::IntroWake)), W->GetIntroElapsed(), Oui(!W->IsServiceDoorOpen()), Hint,
			W->IsMissionActive() ? TEXT("ACTIVE") : TEXT("aucune"), *StoryText(W->GetStory())), !bOk);
		Shot(TEXT("v413_reveil"));
		return true;
	});
	Add(TEXT("v4.13 histoire : raccourcis refuses"), 4.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		// L'hote refuse toute transition que le fil n'a pas preparee : le Niveau 1 tant que la porte n'est pas prise, les
		// Poolrooms et le Niveau 2 a plus forte raison ; la demande directe ne lance rien
		int32 Allowed = 0;
		FString Reasons;
		for (const int32 T : { 1, BRStory::Poolrooms, 2 })
		{
			FString Why;
			const bool bIn = W->CanEnterLevel(T, &Why);
			Allowed += bIn ? 1 : 0;
			Reasons += FString::Printf(TEXT(" ; %d : %s"), T, bIn ? TEXT("ACCEPTE") : *Why);
		}
		TestCountA = W->GetLevelSerial();
		W->RequestTransition(BRStory::Poolrooms);
		Note(FString::Printf(TEXT("raccourcis depuis le Level 0 : %d accepte(s) sur 3%s"), Allowed, *Reasons), Allowed != 0);
		return true;
	});
	Add(TEXT("v4.13 histoire : demande directe sans effet"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		const bool bOk = !W->IsTransitioning() && W->GetLevelNumber() == 0 && W->GetLevelSerial() == TestCountA;
		Note(FString::Printf(TEXT("Poolrooms demandees directement : %s"), bOk ? TEXT("refuse, toujours au Level 0") : TEXT("TRANSITION LANCEE")), !bOk);
		return true;
	});
	AddServiceDoorWalkSteps(TEXT("histoire"), true);
	Add(TEXT("v4.13 histoire : Level 0 accompli"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->GetLevelNumber() != 1)
		{
			return true; // arrivee deja notee
		}
		const BRStory::FState S = W->GetStory();
		PC->WriteActiveSave(true);
		const UBRSaveGame* L = BRSaves::Load(StorySlot);
		const bool bState = BRStory::IsDone(S, 0) && S.Reached == BRStory::IndexOf(1) && S.Resume == 1 && (S.Events & BRStory::Event::DoorOpened) != 0;
		const bool bSaved = L && BRStory::IsDone(L->GetStory(), 0) && L->StoryResume == 1 && L->Version == UBRSaveGame::CurrentVersion;
		FString Why1, Why2, Why0;
		const bool bPool = W->CanEnterLevel(BRStory::Poolrooms, &Why1);
		const bool b2 = W->CanEnterLevel(2, &Why2);
		const bool bBack = W->CanEnterLevel(0, &Why0);
		Note(FString::Printf(TEXT("Niveau 1 : %s ; sauvegarde : Level 0 accompli %s, reprise %d ; Poolrooms %s (%s), Niveau 2 %s (%s), retour au Level 0 %s"), *StoryText(S),
			Oui(L && BRStory::IsDone(L->GetStory(), 0)), L ? L->StoryResume : -1, bPool ? TEXT("ACCEPTE") : TEXT("refuse"), *Why1, b2 ? TEXT("ACCEPTE") : TEXT("refuse"),
			*Why2, bBack ? TEXT("permis") : TEXT("REFUSE")), !bState || !bSaved || bPool || b2 || !bBack);
		return true;
	});
	// Niveau 1 -> Poolrooms : mission resolue en marchant, sortie prise a pied (v4.12)
	AddWalkMissionSteps(1, StorySeed, false);
	AddWalkExitSteps(1);
	Add(TEXT("v4.13 histoire : Poolrooms, inscription de L."), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != BRStory::Poolrooms)
		{
			if (StepTime > 30.f)
			{
				Note(TEXT("Poolrooms non atteintes depuis le Niveau 1 : suite de l'histoire non testee"), true);
				return true;
			}
			return false;
		}
		// L'inscription s'affiche 9 s apres l'arrivee (chaque machine) ; evenement vu une fois par partie
		if (!W->HasStoryEvent(BRStory::Event::PoolInscription) && StepTime < 14.f)
		{
			return false;
		}
		const BRStory::FState S = W->GetStory();
		FString Why;
		const bool b2 = W->CanEnterLevel(2, &Why);
		const bool bOk = BRStory::IsDone(S, 1) && W->HasStoryEvent(BRStory::Event::PoolInscription) && !b2;
		Note(FString::Printf(TEXT("Poolrooms : %s ; inscription de L. %s ; Niveau 2 avant la traversee %s (%s)"), *StoryText(S),
			Oui(W->HasStoryEvent(BRStory::Event::PoolInscription)), b2 ? TEXT("ACCEPTE") : TEXT("refuse"), *Why), !bOk);
		Shot(TEXT("v413_inscription_poolrooms"));
		TestCountA = W->GetLevelSerial();
		W->RequestTransition(1); // retour au Niveau 1 (deja atteint : permis)
		return true;
	});
	Add(TEXT("v4.13 histoire : mission accomplie au retour"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 1) && StepTime < 40.f)
		{
			return false;
		}
		BRMission::FEval E;
		W->GetMissionEval(E);
		FString Why;
		const bool bExit = W->CanUseExit(BRStory::Poolrooms, Why);
		const bool bOk = W->GetLevelNumber() == 1 && E.bSolved && bExit;
		Note(FString::Printf(TEXT("retour au Niveau 1 : mission deja resolue %s, sortie vers les Poolrooms ouverte %s%s"), Oui(E.bSolved), Oui(bExit),
			bExit ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Why)), !bOk);
		W->RequestTransition(BRStory::Poolrooms);
		return true;
	});
	Add(TEXT("v4.13 histoire : de nouveau aux Poolrooms"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != BRStory::Poolrooms) && StepTime < 40.f)
		{
			return false;
		}
		Note(FString::Printf(TEXT("Poolrooms rejointes depuis le Niveau 1 : %s"), Oui(W->GetLevelNumber() == BRStory::Poolrooms)), W->GetLevelNumber() != BRStory::Poolrooms);
		return true;
	});
	// Poolrooms -> Niveau 2 (traversee a pied du sas, v4.12)
	AddWalkMissionSteps(BRStory::Poolrooms, StorySeed, false);
	AddWalkExitSteps(BRStory::Poolrooms);
	Add(TEXT("v4.13 histoire : traversee des Poolrooms enregistree"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 2)
		{
			if (StepTime > 30.f)
			{
				Note(TEXT("Niveau 2 non atteint depuis les Poolrooms : fin du chapitre non testee"), true);
				return true;
			}
			return false;
		}
		const BRStory::FState S = W->GetStory();
		const bool bOk = BRStory::IsDone(S, BRStory::Poolrooms) && S.Reached == BRStory::IndexOf(2) && S.Resume == 2;
		Note(FString::Printf(TEXT("Niveau 2 : %s (Poolrooms accomplies %s)"), *StoryText(S), Oui(BRStory::IsDone(S, BRStory::Poolrooms))), !bOk);
		return true;
	});
	// Niveau 2 -> fin du chapitre 1 (fin du contenu disponible)
	AddWalkMissionSteps(2, StorySeed, false);
	AddWalkExitSteps(2);
	Add(TEXT("v4.13 histoire : fin du chapitre"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC)
		{
			return false;
		}
		const BRStory::FState S = W->GetStory();
		PC->WriteActiveSave(true);
		const UBRSaveGame* L = BRSaves::Load(StorySlot);
		const int32 Lot = BRContent::CurrentLot(BRLevels::Channel());
		const int32 Cont = PC->GetContinueLevel();
		TArray<int32> Choices;
		W->GetExplorationChoices(Choices);
		int32 Beyond = 0;
		FString List;
		for (const int32 Level : Choices)
		{
			Beyond += BRStory::CanEnter(S, Level) == BRStory::EAccess::Allowed ? 0 : 1;
			List += (List.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::FromInt(Level);
		}
		const bool bOk = BRStory::IsDone(S, 2) && (S.ChapterEnds & (1u << Lot)) != 0 && (S.Events & BRStory::Event::StationSign) != 0 && L && L->StoryChapterEnds == S.ChapterEnds
			&& BRLevels::Exists(Cont) && Beyond == 0 && Choices.Num() > 0;
		Note(FString::Printf(TEXT("fin du chapitre %d : %s ; sauvegarde a jour %s ; CONTINUER reprend au Niveau %d ; exploration apres la fin : %s (hors histoire : %d)"), Lot,
			*StoryText(S), Oui(L && L->StoryChapterEnds == S.ChapterEnds), Cont, *List, Beyond), !bOk);
		// Retour a la session de test d'avant (partie active, mode)
		BRSaves::ActiveSlot() = TestOrigSlot;
		PC->ActiveSave = TestOrigSave;
		PC->bDevSession = bTestDevSession;
		TestOrigSave = nullptr;
		DeleteV413Saves();
		BRSaves::SetTestPrefix(FString());
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Migration (vrais fichiers)
	Add(TEXT("v4.13 migration : partie allee au-dela des Poolrooms sans les traverser"), 0.f, [this]()
	{
		BRSaves::SetTestPrefix(SavePrefix);
		DeleteV413Saves();
		const uint32 Seed = ABRWorld::SeedFromUser(4133, 3);
		const TArray<uint8> Bytes = WriteLegacy(MigrationSlot, { 0, 1, 2, 3 }, 3, Seed, BRMission::GenVersion, { 7, 7, 7 });
		UBRSaveGame* L = BRSaves::Load(MigrationSlot);
		TArray<uint8> Copy;
		const bool bCopy = FFileHelper::LoadFileToArray(Copy, *BRSaves::FilePath(BRSaves::LegacySlotName(MigrationSlot, 4))) && Copy == Bytes;
		const BRStory::FState S = L ? L->GetStory() : BRStory::FState();
		const bool bOk = L && bCopy && L->LoadedVersion == 4 && S.Parked == 3 && S.Resume == BRStory::Poolrooms && L->CurrentLevel == BRStory::Poolrooms
			&& !BRStory::IsDone(S, BRStory::Poolrooms) && (S.Events & BRStory::Event::MigratedDetour) != 0 && L->ParkedSession.bValid && L->ParkedSession.Seed == Seed
			&& L->ParkedSession.GenVersion == BRMission::GenVersion && L->ParkedSession.Mission.Num() == 3 && !L->Session.bValid
			&& BRStory::CanEnter(S, 2) == BRStory::EAccess::PoolroomsRequired;
		Note(FString::Printf(TEXT("format 4 (0, 1, 2, 3 ; arret au Niveau 3) : copie d'origine identique %s ; %s ; reprise aux Poolrooms %s ; session du Niveau 3 garee "
			"intacte %s (graine, generation %d, mission %d octets) ; Niveau 2 ferme avant la traversee %s"), Oui(bCopy), *StoryText(S), Oui(L && L->CurrentLevel == BRStory::Poolrooms),
			Oui(L && L->ParkedSession.bValid && L->ParkedSession.Seed == Seed), L ? L->ParkedSession.GenVersion : -1, L ? L->ParkedSession.Mission.Num() : -1,
			Oui(BRStory::CanEnter(S, 2) == BRStory::EAccess::PoolroomsRequired)), !bOk);
		return true;
	});
	Add(TEXT("v4.13 migration : traversee prouvee, visite simple"), 0.f, [this]()
	{
		DeleteV413Saves();
		// Preuve : le Niveau 4 decouvert juste apres les Poolrooms (leur echelle menait au Niveau 4 avant la v4.13)
		const uint32 Seed = ABRWorld::SeedFromUser(4134, 4);
		WriteLegacy(MigrationSlot, { 0, 37, 4, 1 }, 4, Seed, BRMission::GenVersion, { 1, 2 });
		UBRSaveGame* A = BRSaves::Load(MigrationSlot);
		const BRStory::FState SA = A ? A->GetStory() : BRStory::FState();
		const bool bA = A && BRStory::IsDone(SA, BRStory::Poolrooms) && SA.Parked == -1 && SA.Resume == 4 && A->Session.bValid && A->Session.Seed == Seed
			&& A->Session.GenVersion == BRMission::GenVersion && A->Session.Mission.Num() == 2;
		DeleteV413Saves();
		// Simple visite : apres les Poolrooms, le Niveau 1 (atteignable aussi depuis le Level 0) ne prouve rien
		WriteLegacy(MigrationSlot, { 0, 37, 1, 2 }, 2, ABRWorld::SeedFromUser(4135, 2), BRMission::GenVersion, { 3 });
		UBRSaveGame* B = BRSaves::Load(MigrationSlot);
		const BRStory::FState SB = B ? B->GetStory() : BRStory::FState();
		const bool bB = B && !BRStory::IsDone(SB, BRStory::Poolrooms) && SB.Parked == 2 && SB.Resume == BRStory::Poolrooms && B->ParkedSession.bValid;
		Note(FString::Printf(TEXT("0, 37, 4, 1 (arret au Niveau 4) : traversee creditee %s, session gardee telle quelle (generation %d) %s ; %s"), Oui(A && BRStory::IsDone(SA, BRStory::Poolrooms)),
			A ? A->Session.GenVersion : -1, Oui(bA), *StoryText(SA)), !bA);
		Note(FString::Printf(TEXT("0, 37, 1, 2 (arret au Niveau 2) : traversee NON creditee %s, reprise aux Poolrooms %s, Niveau 2 gare %s ; %s"),
			Oui(B && !BRStory::IsDone(SB, BRStory::Poolrooms)), Oui(SB.Resume == BRStory::Poolrooms), Oui(SB.Parked == 2), *StoryText(SB)), !bB);
		return true;
	});
	Add(TEXT("v4.13 migration : session du Level 0"), 0.f, [this]()
	{
		DeleteV413Saves();
		const uint32 Seed = ABRWorld::SeedFromUser(4136, 0);
		WriteLegacy(MigrationSlot, { 0 }, 0, Seed, BRMission::GenVersion, { 9, 9, 9, 9 });
		UBRSaveGame* L = BRSaves::Load(MigrationSlot);
		const BRStory::FState S = L ? L->GetStory() : BRStory::FState();
		// Plus d'objectif au Level 0 : la session garde sa graine et ses objets, perd son etat de mission (la porte s'ouvre
		// sans condition) ; l'histoire commence au Level 0, le reveil n'est pas rejoue (partie deja commencee)
		const bool bOk = L && L->Session.bValid && L->Session.Level == 0 && L->Session.Seed == Seed && L->Session.Mission.Num() == 0 && L->Session.VHSFound == 2
			&& S.Done == 0 && S.Resume == 0 && (S.Events & BRStory::Event::IntroWake) != 0;
		Note(FString::Printf(TEXT("session du Level 0 (format 4) : graine gardee %s, documents gardes %d, etat de mission retire %s, generation %d ; %s"),
			Oui(L && L->Session.Seed == Seed), L ? L->Session.VHSFound : -1, Oui(L && L->Session.Mission.Num() == 0), L ? L->Session.GenVersion : -1, *StoryText(S)), !bOk);
		DeleteV413Saves();
		BRSaves::SetTestPrefix(FString());
		return true;
	});
	Add(TEXT("v4.13 bilan des portes"), 0.f, [this]()
	{
		Note(FString::Printf(TEXT("porte de service : %d graine(s) verifiee(s)"), DoorLines.Num()), DoorLines.Num() < static_cast<int32>(UE_ARRAY_COUNT(DoorSeeds)));
		return true;
	});
}

// =====================================================================================================================
// Etapes v4.13 (reseau) : porte de service ouverte par l'hote pour le groupe
// =====================================================================================================================

void ABRAutoTest::AddNetV413Steps(bool bClient)
{
	using namespace BRTestV413;
	Add(TEXT("v4.13 reseau : Level 0"), 1.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		const uint32 Seed = ABRWorld::SeedFromUser(StorySeed, 0);
		if (!bClient && TestMissionPhase != 41 && !W->IsTransitioning())
		{
			W->RequestTransition(0, false, Seed);
			TestMissionPhase = 41;
			return false;
		}
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 0 || W->GetSeed() != Seed) && StepTime < 60.f)
		{
			return false;
		}
		TestMissionPhase = 0;
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		return true;
	});
	Add(TEXT("v4.13 reseau : porte fermee"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->GetLevelNumber() != 0)
		{
			Skip(TEXT("porte de service en reseau : groupe absent du Level 0"));
			TestLevel = -1;
			return true;
		}
		ABRExit* Door = FindServiceDoor(GetWorld());
		if (!Door && StepTime < 20.f)
		{
			return false;
		}
		TestLevel = Door ? 0 : -1;
		const bool bClosed = Door && !W->IsServiceDoorOpen() && !Door->IsServiceDoorOpen() && Door->GetServiceDoorAngle() < 1.f;
		Note(FString::Printf(TEXT("%s : porte en (%d, %d) (a comparer entre les deux rapports), construite %s, fermee %s"), bClient ? TEXT("client") : TEXT("hote"),
			W->GetServiceDoorCell().X, W->GetServiceDoorCell().Y, Oui(Door != nullptr), Oui(bClosed)), !bClosed);
		return true;
	});
	Add(TEXT("v4.13 reseau : l'hote ouvre"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRExit* Door = FindServiceDoor(GetWorld());
		if (TestLevel < 0 || !W || !C || !Door)
		{
			return true;
		}
		if (bClient)
		{
			return true; // le client reste au depart, loin de la porte
		}
		// Placement direct devant la porte : cette etape porte sur l'etat partage, pas sur l'acces (verifie en solo)
		if (StepTime < 0.5f)
		{
			C->SetActorLocation(DoorGoal(W, C, 120.f), false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* M = C->GetCharacterMovement())
			{
				M->StopMovementImmediately();
			}
			return false;
		}
		FaceAt(C, Door->GetActorLocation() + FVector(0.f, 0.f, 110.f));
		if (C->FocusActor.Get() != Door)
		{
			if (StepTime < 3.f)
			{
				return false;
			}
			Note(TEXT("hote : porte de service pas visee par l'interaction"), true);
			TestLevel = -1;
			return true;
		}
		SerialAtOpen = W->GetLevelSerial();
		C->Interact();
		TestTimer = FPlatformTime::Seconds();
		return true;
	});
	Add(TEXT("v4.13 reseau : porte ouverte pour le groupe"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRExit* Door = FindServiceDoor(GetWorld());
		if (TestLevel < 0 || !W)
		{
			return true;
		}
		// Le client recoit l'etat de l'hote (porte ouverte, rassemblement) ; chacun anime son battant
		const FBRNetDeparture& Dep = W->GetDeparture();
		const bool bOpen = W->IsServiceDoorOpen() && Door && Door->IsServiceDoorOpen();
		if ((!bOpen || Dep.Phase != 1 || (Door && Door->GetServiceDoorAngle() < 60.f)) && StepTime < 20.f)
		{
			return false;
		}
		SerialAtOpen = bClient ? W->GetLevelSerial() : SerialAtOpen;
		const bool bOk = bOpen && Dep.Phase == 1 && Dep.Ready < Dep.Needed;
		Note(FString::Printf(TEXT("%s : porte ouverte %s, battant a %.0f degres, rassemblement %s (%d/%d)"), bClient ? TEXT("client") : TEXT("hote"), Oui(bOpen),
			Door ? Door->GetServiceDoorAngle() : -1.f, Oui(Dep.Phase == 1), Dep.Ready, Dep.Needed), !bOk);
		Shot(TEXT("v413_reseau_porte_ouverte"));
		Skip(TEXT("client arrive apres l'ouverture : non reproductible dans ce banc (le client se connecte au debut) ; l'etat lui parviendrait par la premiere "
			"replication (OnRep_Story) et la porte de son chunk le prendrait a sa construction (RegisterServiceDoor)"));
		return true;
	});
	Add(TEXT("v4.13 reseau : coequipier a pied"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (TestLevel < 0 || !W || !C)
		{
			return true;
		}
		if (!bClient)
		{
			return W->IsTransitioning() || W->GetLevelSerial() != SerialAtOpen || StepTime > 150.f;
		}
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		if (TestMissionPhase != 42)
		{
			const FVector Loc = W->GetDeparture().Location;
			BRTestV412::BeginWalk(W, C, FVector(Loc.X, Loc.Y, C->GetActorLocation().Z), 250.f, false);
			TestMissionPhase = 42;
			return false;
		}
		if (W->IsTransitioning() || W->GetLevelSerial() != SerialAtOpen)
		{
			TestMissionPhase = 0;
			return true;
		}
		const BRTestV412::EWalk R = BRTestV412::StepWalk(W, C, Dt, 140.0, C->GetScareKind() >= 0 || C->IsInputLocked());
		if ((R == BRTestV412::EWalk::Moving || R == BRTestV412::EWalk::Arrived) && StepTime < 150.f)
		{
			return false; // une fois rassemble, l'hote lance le depart
		}
		TestMissionPhase = 0;
		Note(FString::Printf(TEXT("client : point de rassemblement de la porte non atteint a pied (%s)"), *BRTestV412::WalkStats()), true);
		return true;
	});
	Add(TEXT("v4.13 reseau : depart par la porte"), 1.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (TestLevel < 0 || !W)
		{
			return true;
		}
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 1) && StepTime < 40.f)
		{
			return false;
		}
		const bool bOk = W->GetLevelNumber() == 1 && W->IsLevelReady() && W->GetLevelSerial() == SerialAtOpen + 1;
		Note(FString::Printf(TEXT("%s : depart de groupe par la porte de service vers le Niveau 1 %s (serie %d -> %d, un seul depart attendu)"), bClient ? TEXT("client") : TEXT("hote"),
			bOk ? TEXT("effectue") : TEXT("ECHEC"), SerialAtOpen, W->GetLevelSerial()), !bOk);
		return true;
	});
}
