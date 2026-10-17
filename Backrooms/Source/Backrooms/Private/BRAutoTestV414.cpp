// v4.14 : etapes du test automatique propres a la v4.14 (-BRAutoTest -BRAutoTestV414 ; reseau : -BRNetTest).
// Chaque etape verifie une correction de la v4.14 dans le jeu reel :
//   - REJOUER isole : une partie de campagne de test (emplacement copie, prefixe de test, jamais une partie du joueur) ;
//     en REJOUER : consommation, ramassage, blessure, ecritures (minuterie, pause), mort et reveil dans le niveau rejoue ;
//     le FICHIER relu par BRSaves::Load garde equipement, sante, morts, temps de jeu, niveau, session, campagne et
//     histoire a l'identique (seul le journal peut grandir) ; meme chose en mode developpeur ;
//   - migration du format 4, decouvertes [0, 1, 37, 4] arretees au Niveau 4 : traversee des Poolrooms NON creditee
//     (ambigue), session du Niveau 4 garee intacte, reprise aux Poolrooms ; la session garee reste en place tant que sa
//     reprise n'a pas eu lieu ;
//   - scanner : rechargement 6 -> 6 avec une autre graine (identite du chargement, points, plaques relevees, focus
//     effaces) ; focus arrete par l'inventaire et la pause (et le maintien sur un Smiler remis a zero) ; rayons par
//     seconde a 30, 60, 144 et 240 images/s plafonnees (t.MaxFPS) : debit, refus, remplacements, inscriptions ;
//     HUD contextuel (recharge, cone, rappel des touches) ;
//   - kit du Level 0 : etat du kit (complet, ou raison du refus), modules poses dans les chunks, passages franchissables
//     par la capsule du joueur, murs bloquants ; captures a hauteur d'oeil de la meme graine avec et sans le kit.
// Reseau : porte du client = porte de l'hote (cellule, face), meme apres un etat recu en retard.
// Prepare, non execute dans l'environnement de developpement (pas d'Unreal) : une verification qui ne peut pas etre
// faite est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRChunk.h"
#include "BRHUD.h"
#include "BRLevels.h"
#include "BRLidar.h"
#include "BRLidarLogic.h"
#include "BRMission.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRStoryLogic.h"
#include "BRWorld.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

namespace BRTestV414
{
	const TCHAR* const SavePrefix = TEXT("BR_AutoTestV414_");
	constexpr int32 CampaignSlot = 1;
	constexpr int32 MigrationSlot = 2;
	/** Graines : Level 1 rejoue, Level 6 (deux chargements), Level 0 du kit (9 : graine de la scene du kit) */
	constexpr uint32 Lidar6SeedA = 4136u;
	constexpr uint32 Lidar6SeedB = 4142u;
	constexpr uint32 KitSeed = 9u;
	/** Cadences du test du scanner (images/s plafonnees) */
	const int32 FpsCaps[] = { 30, 60, 144, 240 };

	/** Empreinte des champs durables d'une partie (tout ce que REJOUER et le mode developpeur ne doivent pas changer) */
	FString Durable(const UBRSaveGame* S)
	{
		if (!S)
		{
			return TEXT("(partie illisible)");
		}
		FString Items;
		for (const FBRSavedItem& I : S->Items)
		{
			Items += FString::Printf(TEXT("%d:%d:%d:%d,"), I.Group, I.Index, I.Item, I.Count);
		}
		const BRStory::FState St = S->GetStory();
		return FString::Printf(TEXT("objets[%s] sante %.1f mental %.1f piles %.1f morts %d temps %.0f niveau %d explores %d notes %d campagne %d/%d/%d "
			"mort en attente %d session %d/%d/%u/%d octets gare %d/%d histoire %d/%x/%x/%d/%d"),
			*Items, S->Health, S->Sanity, S->Battery, S->Deaths, S->PlayTime, S->CurrentLevel, S->Explored.Num(), S->Notes.Num(), S->RouteBits, S->OptionalFound,
			S->Endings, S->bPendingDeath ? 1 : 0, S->Session.bValid ? 1 : 0, S->Session.Level, S->Session.Seed, S->Session.Mission.Num(),
			S->ParkedSession.bValid ? 1 : 0, S->ParkedSession.Level, St.Reached, St.Done, St.Events, St.Resume, St.Parked);
	}

	FString Snapshot;

	void DeleteV414Saves()
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

	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	/** Mesures du scanner par cadence (rapport) */
	TArray<FString> FpsLines;
}

void ABRAutoTest::AddV414Steps()
{
	using namespace BRTestV414;

	// ---------------------------------------------------------------------------------------------- 2.1 REJOUER isole
	Add(TEXT("v4.14 rejeu : partie de campagne de test"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		BRSaves::SetTestPrefix(SavePrefix);
		DeleteV414Saves();
		// Campagne arretee au Niveau 2 apres les Poolrooms, avec equipement, blessures, une mort et du temps de jeu
		UBRSaveGame* S = NewObject<UBRSaveGame>();
		S->SaveName = TEXT("Campagne v4.14");
		S->Created = FDateTime::Now();
		for (const int32 L : { 0, 1, 37, 2 })
		{
			S->MarkExplored(L);
		}
		S->CurrentLevel = 2;
		S->bHasPlayer = true;
		FBRSavedItem Water;
		Water.Group = static_cast<uint8>(EBRSlotGroup::Pockets);
		Water.Index = 0;
		Water.Item = static_cast<uint8>(EBRItem::AlmondWater);
		Water.Count = 2;
		FBRSavedItem Bandage = Water;
		Bandage.Index = 1;
		Bandage.Item = static_cast<uint8>(EBRItem::Bandage);
		Bandage.Count = 1;
		S->Items = { Water, Bandage };
		S->Health = 80.f;
		S->Sanity = 70.f;
		S->Deaths = 1;
		S->PlayTime = 1234.f;
		S->Session.bValid = true;
		S->Session.Level = 2;
		S->Session.Seed = ABRWorld::SeedFromUser(4140u, 2);
		S->Session.GenVersion = BRMission::GenVersion;
		S->Session.Mission = { 4, 1, 4 };
		BRStory::FState St;
		BRStory::SetDone(St, 0);
		BRStory::SetDone(St, 1);
		BRStory::SetDone(St, 37);
		St.Reached = BRStory::IndexOf(2);
		St.Resume = 2;
		S->SetStory(St);
		const bool bWritten = BRSaves::Write(CampaignSlot, S);
		BRSaves::Flush();
		Snapshot = Durable(BRSaves::Load(CampaignSlot));
		TestOrigSlot = BRSaves::ActiveSlot();
		TestOrigSave = PC->ActiveSave;
		bTestDevSession = PC->bDevSession;
		// REJOUER le Niveau 1 depuis cette partie, par le meme chemin que le bouton du menu (StartSolo) : partie choisie,
		// niveau du carrousel, menu ferme. Avant : le test imitait le menu sans le fermer ; la session restait "libre"
		// (GetSessionMode : menu ouvert) et le test concluait a tort que REJOUER ne marchait pas
		SwapTestSave(PC, BRSaves::Load(CampaignSlot), CampaignSlot); // v4.15 (correctifs 3) : sans ecriture differee heritee
		PC->bDevSession = false;
		PC->bInMenu = true;
		PC->MenuIndex = BRLevels::IndexOf(1);
		PC->StartSolo(true);
		Note(FString::Printf(TEXT("partie de campagne de test ecrite %s (emplacement %d) : %s"), Oui(bWritten), CampaignSlot, *Snapshot), !bWritten);
		return true;
	});
	Add(TEXT("v4.14 rejeu : consommation, ramassage, blessure, ecritures"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 1)
		{
			return StepTime > 40.f;
		}
		const bool bReplay = PC->GetSessionMode() == EBRSessionMode::Replay && !PC->WritesCampaign();
		C->bGodMode = false;
		C->RemoveItem(EBRItem::AlmondWater, 1);
		C->AddItem(EBRItem::Battery, 1);
		C->Health = FMath::Max(10.f, C->Health - 35.f);
		// Ecritures ordinaires d'une session : minuterie, pause (ouverte puis fermee), ecriture bloquante
		PC->WriteActiveSave(false);
		PC->TogglePause();
		PC->TogglePause();
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		const FString Now = Durable(BRSaves::Load(CampaignSlot));
		Note(FString::Printf(TEXT("REJOUER : mode rejeu %s ; apres consommation, ramassage, blessure et ecritures, partie du fichier inchangee %s%s"), Oui(bReplay),
			Oui(Now == Snapshot), Now == Snapshot ? TEXT("") : *(TEXT(" ; maintenant : ") + Now)), !bReplay || Now != Snapshot);
		return true;
	});
	Add(TEXT("v4.14 rejeu : mort"), 0.2f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return false;
		}
		C->bGodMode = false;
		C->ReceiveAttack(500.f, 0.f, nullptr);
		BRSaves::Flush();
		const FString Now = Durable(BRSaves::Load(CampaignSlot));
		Note(FString::Printf(TEXT("REJOUER : mort %s ; partie du fichier inchangee (ni mort en attente, ni session effacee, ni mort comptee) %s%s"), Oui(C->IsDead()),
			Oui(Now == Snapshot), Now == Snapshot ? TEXT("") : *(TEXT(" ; maintenant : ") + Now)), !C->IsDead() || Now != Snapshot);
		return true;
	});
	Add(TEXT("v4.14 rejeu : reveil dans le niveau rejoue"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if ((W->IsTransitioning() || C->IsDead() || !W->IsLevelReady()) && StepTime < 25.f)
		{
			return false;
		}
		// Regle de la v4.14 : en REJOUER, une mort en solo relance le niveau rejoue (pas le Level 0 de la campagne)
		const bool bOk = !C->IsDead() && W->GetLevelNumber() == 1;
		Note(FString::Printf(TEXT("REJOUER : reveil au Niveau %d (attendu 1, le niveau rejoue)"), W->GetLevelNumber()), !bOk);
		C->bGodMode = true;
		return true;
	});
	Add(TEXT("v4.14 rejeu : fermeture et relecture du fichier"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC)
		{
			return false;
		}
		// Fermeture : la meme ecriture bloquante que le retour au menu et la sortie du jeu
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		const UBRSaveGame* L = BRSaves::Load(CampaignSlot);
		const FString Now = Durable(L);
		Note(FString::Printf(TEXT("REJOUER : apres la fermeture, la partie relue du disque est identique %s (equipement %d objets, session du Niveau %d, histoire reprise %d)%s"),
			Oui(Now == Snapshot), L ? L->Items.Num() : -1, L ? L->Session.Level : -1, L ? L->GetStory().Resume : -1,
			Now == Snapshot ? TEXT("") : *(TEXT(" ; maintenant : ") + Now)), Now != Snapshot);
		return true;
	});
	Add(TEXT("v4.14 rejeu : mode developpeur"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady())
		{
			return StepTime > 20.f;
		}
		// Mode developpeur sur un niveau deja decouvert (le Niveau 1 de la partie) : rien n'est ecrit
		BRSaves::ReplaySession() = false;
		PC->bDevSession = true;
		C->RemoveItem(EBRItem::Bandage, 1);
		C->Health = FMath::Max(10.f, C->Health - 20.f);
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		const FString Now = Durable(BRSaves::Load(CampaignSlot));
		Note(FString::Printf(TEXT("mode developpeur : session libre %s, partie du fichier inchangee %s%s"), Oui(PC->GetSessionMode() == EBRSessionMode::Free), Oui(Now == Snapshot),
			Now == Snapshot ? TEXT("") : *(TEXT(" ; maintenant : ") + Now)), Now != Snapshot);
		// Retour a l'etat d'avant le test (partie active, mode du joueur, session du monde hors histoire)
		PC->bDevSession = bTestDevSession;
		SwapTestSave(PC, TestOrigSave, TestOrigSlot);
		TestOrigSave = nullptr;
		BRSaves::ReplaySession() = false;
		W->SetStory(BRStory::FState(), EBRSessionMode::Free);
		DeleteV414Saves();
		BRSaves::SetTestPrefix(FString());
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 2.2 Migration [0, 1, 37, 4]
	Add(TEXT("v4.14 migration : [0, 1, 37, 4] au Niveau 4"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		BRSaves::SetTestPrefix(SavePrefix);
		DeleteV414Saves();
		// Ancienne partie (format 4) ecrite sans BRSaves, comme la v4.12 l'aurait laissee
		UBRSaveGame* Old = NewObject<UBRSaveGame>();
		Old->Version = 4;
		Old->SaveName = TEXT("Partie v4.12 (0, 1, 37, 4)");
		Old->Created = FDateTime::Now();
		for (const int32 L : { 0, 1, 37, 4 })
		{
			Old->MarkExplored(L);
		}
		const uint32 Seed = ABRWorld::SeedFromUser(4143u, 4);
		Old->CurrentLevel = 4;
		Old->Session.bValid = true;
		Old->Session.Level = 4;
		Old->Session.Seed = Seed;
		Old->Session.GenVersion = BRMission::GenVersion;
		Old->Session.Mission = { 5, 6 };
		TArray<uint8> Bytes;
		UGameplayStatics::SaveGameToMemory(Old, Bytes);
		FFileHelper::SaveArrayToFile(Bytes, *BRSaves::FilePath(BRSaves::SlotName(MigrationSlot)));
		UBRSaveGame* L = BRSaves::Load(MigrationSlot);
		const BRStory::FState S = L ? L->GetStory() : BRStory::FState();
		// Le Niveau 1 decouvert avant le Niveau 4 : l'ascenseur du Niveau 1 menait aussi au Niveau 4. L'ordre ne prouve pas
		// la traversee : rien n'est credite, la session du Niveau 4 est garee intacte, la reprise est aux Poolrooms
		const bool bOk = L && !BRStory::IsDone(S, BRStory::Poolrooms) && S.Parked == 4 && S.Resume == BRStory::Poolrooms && L->ParkedSession.bValid
			&& L->ParkedSession.Seed == Seed && L->ParkedSession.Mission.Num() == 2 && !L->Session.bValid;
		Note(FString::Printf(TEXT("[0, 1, 37, 4] : traversee des Poolrooms creditee %s (attendu : non, ordre ambigu) ; reprise %d ; Niveau 4 gare %s, session garee intacte %s"),
			(L && BRStory::IsDone(S, BRStory::Poolrooms)) ? TEXT("OUI") : TEXT("non"), S.Resume, Oui(S.Parked == 4),
			Oui(L && L->ParkedSession.bValid && L->ParkedSession.Seed == Seed)), !bOk);
		// La session garee n'est pas retiree tant que sa reprise n'a pas eu lieu (retour par le Level 0 compris)
		if (PC && L)
		{
			UBRSaveGame* Keep = PC->ActiveSave;
			PC->ActiveSave = L;
			const bool bParked = PC->HasParkedSession(4);
			BRStory::FState After = S;
			BRStory::OnProgress(After, 0, 1);
			const bool bStill = PC->HasParkedSession(4) && After.Parked == 4;
			Note(FString::Printf(TEXT("session garee disponible %s ; toujours garee apres un passage 0 -> 1 (Poolrooms pas encore traversees) %s ; voulue au Niveau 4 seulement "
				"apres la traversee %s"), Oui(bParked), Oui(bStill), Oui(!BRStory::WantsParked(After, 4))), !bParked || !bStill || BRStory::WantsParked(After, 4));
			PC->ActiveSave = Keep;
		}
		DeleteV414Saves();
		BRSaves::SetTestPrefix(FString());
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 2.3 a 2.7 Scanner
	AddSeedLoad(6, Lidar6SeedA, TEXT("v4.14 Level 6, premier chargement"));
	Add(TEXT("v4.14 scanner : releves du premier chargement"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!L)
		{
			if (StepTime < 5.f)
			{
				return false;
			}
			Note(TEXT("scanner absent : rechargement 6 -> 6 non teste"), true);
			return true;
		}
		if (TestMissionPhase != 141)
		{
			TestMissionPhase = 141;
			L->Pulse();
			return false;
		}
		if (StepTime < 0.8f)
		{
			return false;
		}
		TestMissionPhase = 0;
		TestCountA = L->GetPointCount();
		TestKey = L->GetLoadKey();
		Note(FString::Printf(TEXT("premier chargement : %d points, %d plaque(s) relevee(s), identite %016llx"), TestCountA, L->GetScannedPlates().Num(), TestKey), TestCountA <= 0);
		L->SetFocus(true);
		return true;
	});
	AddSeedLoad(6, Lidar6SeedB, TEXT("v4.14 Level 6, autre graine"));
	Add(TEXT("v4.14 scanner : rechargement 6 -> 6"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!L)
		{
			if (StepTime < 5.f)
			{
				return false;
			}
			Note(TEXT("scanner absent apres le rechargement"), true);
			return true;
		}
		// Aucun point, aucune plaque, aucun focus, aucune cible de l'ancien chargement ; nouvelle identite
		const bool bOk = L->GetLoadKey() != TestKey && L->GetPointCount() == 0 && L->GetScannedPlates().Num() == 0 && !L->IsFocusing() && !L->GetFocusDevice()
			&& !L->GetHoldEntity();
		Note(FString::Printf(TEXT("rechargement 6 -> 6 (autre graine) : identite changee %s, points %d (avant : %d), plaques %d, focus arrete %s, cible %s"),
			Oui(L->GetLoadKey() != TestKey), L->GetPointCount(), TestCountA, L->GetScannedPlates().Num(), Oui(!L->IsFocusing()),
			L->GetFocusDevice() || L->GetHoldEntity() ? TEXT("GARDEE") : TEXT("aucune")), !bOk);
		return true;
	});
	Add(TEXT("v4.14 scanner : focus arrete par l'inventaire et la pause"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!PC || !L)
		{
			Note(TEXT("scanner ou controleur absent : arret du focus non teste"), true);
			return true;
		}
		L->SetFocus(true);
		const bool bOnBefore = L->IsFocusing();
		PC->SetInventoryOpen(true);
		const bool bInv = !L->IsFocusing() && !L->GetHoldEntity() && L->GetEntityHold() <= 0.f;
		PC->SetInventoryOpen(false);
		L->SetFocus(true);
		PC->TogglePause();
		const bool bPause = !L->IsFocusing();
		PC->TogglePause();
		Note(FString::Printf(TEXT("focus en marche %s ; arrete a l'ouverture de l'inventaire %s (maintien sur un Smiler remis a zero %s) ; arrete a la pause %s"), Oui(bOnBefore),
			Oui(bInv), Oui(!L->GetHoldEntity()), Oui(bPause)), !bOnBefore || !bInv || !bPause);
		Skip(TEXT("perte du focus de la fenetre (FSlateApplication::IsActive) et touche relachee hors de la fenetre : a verifier a la main (Alt+Tab pendant le scan)"));
		return true;
	});
	// Debit du scanner a plusieurs cadences plafonnees : rayons par seconde, refus, remplacements, inscriptions
	for (const int32 Fps : FpsCaps)
	{
		Add(FString::Printf(TEXT("v4.14 scanner : focus a %d images/s"), Fps), 0.f, [this, Fps]()
		{
			ABRCharacter* C = GetPlayer();
			ABRLidar* L = C ? C->GetLidar() : nullptr;
			if (!L)
			{
				Note(TEXT("scanner absent : debit non mesure"), true);
				return true;
			}
			if (TestMissionPhase != 142)
			{
				TestMissionPhase = 142;
				GEngine->Exec(GetWorld(), TEXT("r.VSync 0"));
				GEngine->Exec(GetWorld(), *FString::Printf(TEXT("t.MaxFPS %d"), Fps));
				TestCountB = 0;
				return false;
			}
			if (StepTime < 1.f)
			{
				return false; // la cadence se stabilise
			}
			if (TestCountB == 0)
			{
				ABRLidar::ResetStats();
				TestCountB = 1;
				TestCountC = 0;
				TestT0 = StepTime;
			}
			L->SetFocus(true);
			++TestCountC;
			if (StepTime - TestT0 < 4.f)
			{
				return false;
			}
			L->SetFocus(false);
			TestMissionPhase = 0;
			const double Secs = StepTime - TestT0;
			const FBRLidarStats& S = ABRLidar::Stats;
			const double Fpsm = TestCountC / Secs;
			const double RaysPerSec = static_cast<double>(S.Rays) / Secs;
			// La cadence n'est pas toujours atteinte (machine, scene) : le debit est juge seulement si elle l'est
			const bool bReached = Fpsm >= Fps * 0.85;
			const bool bRate = !bReached || RaysPerSec >= BRLidar::FocusRaysPerSecond * 0.85;
			const FString Line = FString::Printf(TEXT("%d images/s visees, %.0f obtenues : %.0f rayons/s (attendu ~%.0f), points demandes %lld, acceptes %lld, remplaces %lld, "
				"expires %lld, refuses %lld, inscriptions %lld points ; pic %d points par image ; temps %.2f ms/image"), Fps, Fpsm, RaysPerSec, BRLidar::FocusRaysPerSecond,
				S.Requested, S.Accepted, S.Replaced, S.Expired, S.Rejected, S.GlyphPoints, S.PeakFramePoints, S.Frames > 0 ? S.TotalSeconds * 1000.0 / S.Frames : 0.0);
			FpsLines.Add(Line);
			Note(Line, !bRate || S.Rejected > 0);
			if (!bReached)
			{
				Skip(FString::Printf(TEXT("%d images/s non atteintes sur cette machine : debit non juge a cette cadence"), Fps));
			}
			return true;
		});
	}
	Add(TEXT("v4.14 scanner : cadence et HUD contextuel"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
		if (PC && TestMissionPhase != 143 && TestMissionPhase != 145)
		{
			TestMissionPhase = 145;
			PC->ApplySettings(); // cadence et synchro du joueur, une seule fois
		}
		if (!L || !H)
		{
			TestMissionPhase = 0;
			Note(TEXT("scanner ou HUD absent : HUD contextuel non teste"), true);
			return true;
		}
		if (TestMissionPhase != 143)
		{
			if (StepTime < 16.f)
			{
				return false; // rappel des touches d'arrivee (12 s) efface, recharge terminee
			}
			TestMissionPhase = 143;
			TestCountA = FMath::RoundToInt(H->LastLidarChargeAlpha * 100.f) + 1000 * FMath::RoundToInt(H->LastLidarConeAlpha * 100.f);
			TestCountB = FMath::RoundToInt(H->LastLidarHintAlpha * 100.f);
			Note(FString::Printf(TEXT("au repos (16 s sans action) : barre de recharge %.2f, cone %.2f, rappel %.2f ; recharge restante %.2f, focus %s"),
				H->LastLidarChargeAlpha, H->LastLidarConeAlpha, H->LastLidarHintAlpha, L->GetPulseCharge(), Oui(L->IsFocusing())));
			L->Pulse();
			TestT0 = StepTime;
			return false;
		}
		if (StepTime - TestT0 < 0.3f)
		{
			return false;
		}
		TestMissionPhase = 0;
		const bool bCalibrated = GetBRWorld() && GetBRWorld()->IsLidarCalibrated();
		const bool bIdleClean = TestCountA == 0 && (TestCountB == 0 || !bCalibrated);
		const bool bChargeShown = H->LastLidarChargeAlpha > 0.f;
		Note(FString::Printf(TEXT("exploration au repos : recharge et cone caches %s, rappel des touches %s (calibre : %s) ; pendant la recharge : barre affichee %s"),
			Oui(TestCountA == 0), TestCountB == 0 ? TEXT("efface") : TEXT("AFFICHE"), Oui(bCalibrated), Oui(bChargeShown)), !bIdleClean || !bChargeShown);
		Shot(TEXT("v414_lidar_hud_recharge"));
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 4 Kit du Level 0
	AddSeedLoad(0, KitSeed, TEXT("v4.14 Level 0, graine du kit"));
	Add(TEXT("v4.14 kit : etat et modules poses"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		UBRAssets* A = UBRAssets::Get(this);
		if (!W || !C || !A || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 0)
		{
			return StepTime > 40.f;
		}
		int32 KitInstances = 0;
		int32 KitComponents = 0;
		TSet<FString> Meshes;
		for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
		{
			TArray<UInstancedStaticMeshComponent*> Comps;
			It->GetComponents<UInstancedStaticMeshComponent>(Comps);
			for (const UInstancedStaticMeshComponent* I : Comps)
			{
				const UStaticMesh* M = I ? I->GetStaticMesh() : nullptr;
				if (M && M->GetPathName().Contains(TEXT("/Level0Kit/")))
				{
					++KitComponents;
					KitInstances += I->GetInstanceCount();
					Meshes.Add(M->GetName());
				}
			}
		}
		const bool bUse = A->UseLevel0Kit();
		// Un kit voulu mais incomplet est une ressource manquante : signalee, jamais silencieuse
		const bool bOk = bUse ? (KitInstances > 0) : !UBRAssets::IsLevel0KitWanted();
		Note(FString::Printf(TEXT("kit du Level 0 : %s ; %d composant(s), %d instance(s), %d module(s) differents dans les chunks"), *A->GetLevel0KitStatus(), KitComponents,
			KitInstances, Meshes.Num()), !bOk);
		return true;
	});
	Add(TEXT("v4.14 kit : passages et murs"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !C->GetCapsuleComponent())
		{
			return true;
		}
		// Autour du depart : chaque porte ordinaire laisse passer la capsule du joueur (sans seuil ni linteau trop bas),
		// chaque mur plein l'arrete (collisions UCX importees)
		const float S = W->Def().CellSize;
		const float T = W->Def().WallThickness;
		const float Radius = C->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FCollisionQueryParams Params(FName(TEXT("BRV414Kit")), false, C);
		const FIntPoint Me = W->WorldToCell(C->GetActorLocation());
		int32 Doors = 0, DoorsBlocked = 0, Walls = 0, WallsOpen = 0;
		FString Where;
		for (int32 X = Me.X - 6; X <= Me.X + 6; ++X)
		{
			for (int32 Y = Me.Y - 6; Y <= Me.Y + 6; ++Y)
			{
				for (int32 East = 0; East < 2; ++East)
				{
					const EBREdge E = East ? W->EdgeE(X, Y) : W->EdgeN(X, Y);
					if (E == EBREdge::Open || W->IsServiceDoorEdge(X, Y, East != 0))
					{
						continue;
					}
					const FVector Mid = East ? FVector((X + 1) * S, (Y + 0.5f) * S, 0.f) : FVector((X + 0.5f) * S, (Y + 1) * S, 0.f);
					const FVector N = East ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 1.f, 0.f);
					const FVector A0 = Mid - N * (T * 0.5f + Radius + 20.f);
					const FVector A1 = Mid + N * (T * 0.5f + Radius + 20.f);
					FHitResult Hit;
					if (E == EBREdge::Door)
					{
						++Doors;
						const FVector Z(0.f, 0.f, Half + 3.f);
						const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, A0 + Z, A1 + Z, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half), Params)
							&& Cast<ABRChunk>(Hit.GetActor()) != nullptr;
						if (bBlocked)
						{
							++DoorsBlocked;
							Where += FString::Printf(TEXT(" porte (%d, %d, %s)"), X, Y, East ? TEXT("E") : TEXT("N"));
						}
					}
					else
					{
						++Walls;
						const FVector Z(0.f, 0.f, 100.f);
						if (!GetWorld()->LineTraceSingleByChannel(Hit, A0 + Z, A1 + Z, ECC_Pawn, Params))
						{
							++WallsOpen;
							Where += FString::Printf(TEXT(" mur (%d, %d, %s)"), X, Y, East ? TEXT("E") : TEXT("N"));
						}
					}
				}
			}
		}
		Note(FString::Printf(TEXT("autour du depart : %d porte(s), %d bloquee(s) pour la capsule (%.0f x %.0f cm) ; %d mur(s), %d traversable(s)%s"), Doors, DoorsBlocked, Radius,
			Half * 2.f, Walls, WallsOpen, *Where), DoorsBlocked > 0 || WallsOpen > 0 || Doors == 0);
		return true;
	});
	Add(TEXT("v4.14 kit : capture avec le kit"), 1.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (AController* Ctrl = C ? C->GetController() : nullptr)
		{
			Ctrl->SetControlRotation(FRotator(0.f, 45.f, 0.f));
		}
		Shot(TEXT("v414_kit_graine9_avec"));
		if (ABRWorld* W = GetBRWorld())
		{
			UBRAssets::SetLevel0KitWanted(false);
			W->RebuildAllChunks();
		}
		return true;
	});
	Add(TEXT("v4.14 kit : capture sans le kit (meme graine, meme vue)"), 2.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (W && !W->IsLevelReady() && StepTime < 20.f)
		{
			return false;
		}
		Shot(TEXT("v414_kit_graine9_sans"));
		UBRAssets::SetLevel0KitWanted(true);
		if (W)
		{
			W->RebuildAllChunks();
		}
		Note(TEXT("captures a hauteur d'oeil de la graine 9 avec et sans le kit : v414_kit_graine9_avec / _sans (a comparer)"));
		return true;
	});
	Add(TEXT("v4.14 bilan du scanner"), 0.f, [this]()
	{
		Note(FString::Printf(TEXT("scanner : %d cadence(s) mesuree(s) sur %d"), FpsLines.Num(), static_cast<int32>(UE_ARRAY_COUNT(FpsCaps))),
			FpsLines.Num() < static_cast<int32>(UE_ARRAY_COUNT(FpsCaps)));
		return true;
	});
}

// =====================================================================================================================
// Etapes v4.14 (reseau) : porte du Level 0 du client = porte de l'hote
// =====================================================================================================================

void ABRAutoTest::AddNetV414Steps(bool bClient)
{
	using namespace BRTestV414;
	Add(TEXT("v4.14 reseau : porte du Level 0"), 1.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!bClient && TestMissionPhase != 144)
		{
			if (W->IsTransitioning())
			{
				return false;
			}
			TestMissionPhase = 144;
			W->RequestTransition(0, false, ABRWorld::SeedFromUser(KitSeed, 0));
			return false;
		}
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 0 || !W->HasServiceDoor()) && StepTime < 60.f)
		{
			return false;
		}
		const FBRNetStory& St = W->GetNetStory();
		// La porte retenue n'est dans l'etat de la partie que pour le Level 0 de la partie (session histoire, graine de la
		// partie) ; ailleurs (session libre, graine imposee) chaque machine la tire de la meme graine
		const bool bPartyDoor = St.IntroSeed == W->GetSeed() && St.DoorDir >= 0 && St.DoorDir < 4;
		if (bClient && !bPartyDoor && W->GetSessionMode() != EBRSessionMode::Free && StepTime < 30.f)
		{
			return false; // etat de l'hote pas encore recu
		}
		TestMissionPhase = 0;
		static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		const TCHAR* Who = bClient ? TEXT("client") : TEXT("hote");
		const FIntPoint Cell = W->GetServiceDoorCell();
		const FIntPoint Dir = W->GetServiceDoorDir();
		if (bPartyDoor)
		{
			const FIntPoint HostCell(St.DoorX, St.DoorY);
			const FIntPoint HostDir = Dirs[St.DoorDir];
			const bool bSame = Cell == HostCell && Dir == HostDir;
			Note(FString::Printf(TEXT("%s : porte en (%d, %d) face (%d, %d) ; porte de la partie (%d, %d) face (%d, %d) : identique %s"), Who, Cell.X, Cell.Y, Dir.X, Dir.Y,
				HostCell.X, HostCell.Y, HostDir.X, HostDir.Y, Oui(bSame)), !bSame);
		}
		else
		{
			// Meme regle que ABRWorld::PlaceServiceDoor sans porte retenue : bande visee, sinon elargie ; plus petit hachage
			const FBRDoorCandidate* Best = nullptr;
			for (uint8 Band = 0; !Best && Band < 2; ++Band)
			{
				for (const FBRDoorCandidate& Cand : W->GetServiceDoorCandidates())
				{
					if (Cand.Band <= Band && (!Best || Cand.Hash < Best->Hash))
					{
						Best = &Cand;
					}
				}
			}
			const bool bSeedChoice = Best && Best->Cell == Cell && Best->Dir == Dir;
			Note(FString::Printf(TEXT("%s : porte en (%d, %d) face (%d, %d) ; pas de porte de partie pour cette graine (session %d, graine %u, partie %u) : choix de la graine "
				"%s"), Who, Cell.X, Cell.Y, Dir.X, Dir.Y, static_cast<int32>(W->GetSessionMode()), W->GetSeed(), St.IntroSeed, Best ? Oui(bSeedChoice) : TEXT("secours")),
				Best && !bSeedChoice);
		}
		Skip(TEXT("etat de la porte recu APRES la construction du niveau (client tardif sous latence) : ABRWorld::ReconcileServiceDoor reconstruit le Level 0 ; a provoquer "
			"avec -BRNetTest et Net PktLag eleve, journal \"porte de service du client reprise\""));
		return true;
	});
}
