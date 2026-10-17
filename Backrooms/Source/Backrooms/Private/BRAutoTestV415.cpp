// v4.15 : etapes du test automatique du parcours lineaire (-BRAutoTest -BRAutoTestV415 ; reseau : -BRNetTest).
// Chaque etape verifie une regle de la v4.15 dans le jeu reel, sur des parties de test (prefixe dedie, jamais une partie
// du joueur) :
//   - menu : SOLO -> VOS PARTIES -> NOUVELLE PARTIE -> COMMENCER (aucun choix de parcours) cree une partie au format 6,
//     parcours lineaire, graine de parcours non nulle ; le Level 0 garde sa graine propre ; pastille LINEAIRE sur la carte ;
//   - CONTINUER d'une partie lineaire arretee au Niveau 2 : graine du niveau = BRStory::LevelSeedFor, drapeau du niveau,
//     aucune sortie tiree par les chunks, une seule sortie (celle de la mission, vers le Niveau 3) ;
//   - mort en solo : le niveau recommence de zero (meme graine, nouveau chargement, mission neuve, equipement de depart,
//     reprise inchangee, mort comptee) ; jeu ferme pendant la mort : par le chemin du menu (VOS PARTIES, choix de la
//     partie, CONTINUER), le niveau est recharge avec sa graine, mission neuve ;
//   - partie classique, meme graine : sorties des chunks presentes, mort -> Level 0 (regle inchangee) ; jeu ferme
//     pendant une mort classique : CONTINUER ramene au Level 0 ;
//   - marche reelle : mission resolue a pied, sortie unique prise, Niveau 3 avec sa graine ; version publiee : fin du
//     chapitre ;
//   - REJOUER et mode developpeur (REJOUER d'un niveau inexplore) dans une partie lineaire : niveau classique, partie
//     intacte ; migration 5 -> 6 (copie _Format5, parcours classique) ; protocole 415.
// Reseau : l'hote impose le parcours lineaire (SetLinearTestOverride, hors version finale) ; drapeau, graine et sorties
// identiques chez chacun ; le client tombe et se reveille dans le niveau (regle de la cooperation).
// Prepare, non execute dans l'environnement de developpement (pas d'Unreal) : une verification qui ne peut pas etre
// faite est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "BRAutoTestNet.h"
#include "BRAutoTestWalk.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRChunk.h"
#include "BRContentLogic.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRMission.h"
#include "BRMissionSolver.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRStoryLogic.h"
#include "BRWorld.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

namespace BRTestV415
{
	const TCHAR* const SavePrefix = TEXT("BR_AutoTestV415_");
	/** Parties de test (la NOUVELLE PARTIE du menu prend le premier emplacement libre) */
	constexpr int32 StorySlot = 1;
	constexpr int32 ClassicSlot = 2;
	constexpr int32 MigrationSlot = 3;
	/** Graine du parcours des parties de test ; graine choisie du test reseau (Niveau 2) */
	constexpr uint32 TestRunSeed = 41500u;
	constexpr uint32 NetUserSeed = 4150u;

	/** Etat partage par les etapes */
	int32 NewGameSlot = INDEX_NONE;
	uint32 LinearSeed2 = 0;
	int32 SerialMark = 0;
	int32 DeathsMark = 0;
	TArray<uint8> FreshMission;
	TArray<uint8> DeathFileBytes;
	FString DurableMark;
	EBRMenuPage MenuPageMark = EBRMenuPage::Main;
	bool bInMenuMark = false;
	bool bReplayMark = false;
	bool bHostFlowMark = false;
	int32 MenuIndexMark = 0;
	/** Niveau pas encore explore de la partie lineaire, visite en mode developpeur (REJOUER d'un niveau inexplore) */
	int32 DevLevel = 3;
	float ClockMark = 0.f;
	int32 StepPhase = 0;

	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	void DeleteV415Saves()
	{
		for (int32 SaveIdx = 0; SaveIdx < BRSaves::MaxSlots; ++SaveIdx)
		{
			for (const FString& FileName : { BRSaves::SlotName(SaveIdx), BRSaves::BackupSlotName(SaveIdx), BRSaves::UnreadableSlotName(SaveIdx), BRSaves::RecoverySlotName(SaveIdx) })
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(FileName), false, true, true);
			}
			for (int32 V = 1; V < UBRSaveGame::CurrentVersion; ++V)
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(BRSaves::LegacySlotName(SaveIdx, V)), false, true, true);
			}
		}
	}

	/** Partie d'histoire de test arretee a l'arrivee au Niveau 2 (Level 0, Niveau 1 et Poolrooms accomplis), sans session */
	UBRSaveGame* MakeStorySave(const TCHAR* Title, bool bLinear)
	{
		UBRSaveGame* G = NewObject<UBRSaveGame>();
		G->SaveName = Title;
		G->Created = FDateTime::Now();
		for (const int32 Lv : { 0, 1, 37, 2 })
		{
			G->MarkExplored(Lv);
		}
		G->CurrentLevel = 2;
		BRStory::FState St;
		BRStory::SetDone(St, 0);
		BRStory::SetDone(St, 1);
		BRStory::SetDone(St, BRStory::Poolrooms);
		St.Reached = BRStory::IndexOf(2);
		St.Resume = 2;
		St.Events = BRStory::Event::IntroWake | BRStory::Event::DoorOpened;
		if (bLinear)
		{
			St.Flags = BRStory::Flag::Linear;
			St.RunSeed = TestRunSeed;
		}
		G->SetStory(St);
		return G;
	}

	/** Sorties presentes dans le monde : tirees par les chunks (proprietaire : le chunk), posees par la mission
	 *  (proprietaire : le monde), dont celles vers Forward ; cibles et cellules pour le rapport */
	struct FExitTally
	{
		int32 Chunk = 0;
		int32 Mission = 0;
		int32 MissionForward = 0;
		FString Where;
	};

	FExitTally TallyExits(UWorld* InWorld, const ABRWorld* W, int32 Forward)
	{
		FExitTally Tally;
		if (!InWorld || !W)
		{
			return Tally;
		}
		for (TActorIterator<ABRExit> It(InWorld); It; ++It)
		{
			const FIntPoint Cell = W->WorldToCell(It->GetActorLocation());
			if (Cast<ABRChunk>(It->GetOwner()))
			{
				++Tally.Chunk;
				Tally.Where += FString::Printf(TEXT(" chunk -> %d (%d, %d)"), It->Target, Cell.X, Cell.Y);
			}
			else if (It->GetOwner() == W)
			{
				++Tally.Mission;
				Tally.MissionForward += It->Target == Forward ? 1 : 0;
				Tally.Where += FString::Printf(TEXT(" mission -> %d (%d, %d)"), It->Target, Cell.X, Cell.Y);
			}
		}
		return Tally;
	}

	/** Streaming au repos : niveau pret, aucun chunk en preparation */
	bool Settled(const ABRWorld* W)
	{
		return W && W->IsLevelReady() && !W->IsTransitioning() && W->GetPreparingChunkCount() == 0;
	}

	/** Champs durables d'une partie (ce que REJOUER ne doit pas changer), parcours compris */
	FString DurableOf(const UBRSaveGame* G)
	{
		if (!G)
		{
			return TEXT("(partie illisible)");
		}
		const BRStory::FState St = G->GetStory();
		return FString::Printf(TEXT("objets %d sante %.1f morts %d temps %.0f niveau %d explores %d campagne %d/%d/%d mort en attente %d session %d/%d/%u "
			"histoire %d/%x/%x/%d/%d parcours %u/%u"), G->Items.Num(), G->Health, G->Deaths, G->PlayTime, G->CurrentLevel, G->Explored.Num(), G->RouteBits,
			G->OptionalFound, G->Endings, G->bPendingDeath ? 1 : 0, G->Session.bValid ? 1 : 0, G->Session.Level, G->Session.Seed, St.Reached, St.Done, St.Events,
			St.Resume, St.Parked, St.Flags, St.RunSeed);
	}

	/** Solution de la mission (solveur aux seules informations visibles) appliquee par les regles de l'hote, au plus Max
	 *  actions ; bUntilChange : arret des que l'etat de la mission a change. Retour : actions appliquees (-1 : aucune
	 *  solution) */
	int32 ApplySolution(ABRWorld* W, int32 Max, bool bUntilChange)
	{
		if (!W || !W->IsMissionActive())
		{
			return -1;
		}
		BRMission::FState Work = W->GetMissionState();
		BRMission::FSolver Solver(W->GetMissionPlan(), Work, W->GetMissionCampaign());
		Solver.bPlayerActions = true;
		Solver.bRecord = true;
		if (!Solver.Solve(0) || Solver.bBad || Solver.bOverflow)
		{
			return -1;
		}
		const TArray<uint8> Start = W->GetMissionBlob();
		int32 Applied = 0;
		for (int32 K = 0; K < Solver.NumSteps && Applied < Max; ++K)
		{
			W->DebugMissionAct(Solver.Steps[K].Device, Solver.Steps[K].Action);
			++Applied;
			if (bUntilChange && W->GetMissionBlob() != Start)
			{
				break;
			}
		}
		return Applied;
	}
}

void ABRAutoTest::AddV415Steps()
{
	using namespace BRTestV415;

	Add(TEXT("v4.15 : preparation"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		FLevelReport R;
		R.Title = TEXT("v4.15 : parcours lineaire (menu, graines, sortie unique, mort, migration, REJOUER, protocole)");
		R.Scene = TEXT("v4.15");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		TestOrigSlot = BRSaves::ActiveSlot();
		TestOrigSave = PC->ActiveSave;
		bTestDevSession = PC->bDevSession;
		bInMenuMark = PC->bInMenu;
		MenuPageMark = PC->MenuPage;
		bReplayMark = BRSaves::ReplaySession();
		bHostFlowMark = PC->bHostFlow;
		MenuIndexMark = PC->MenuIndex;
		BRSaves::SetTestPrefix(SavePrefix);
		DeleteV415Saves();
		BRLevels::SetChannelOverride(static_cast<int32>(BRContent::EChannel::All));
		W->SetLinearTestOverride(-1);
		StepPhase = 0;
		TestMissionPhase = 0;
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 1 Menu
	Add(TEXT("v4.15 menu : NOUVELLE PARTIE"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		// Le vrai menu : VOS PARTIES -> NOUVELLE PARTIE -> COMMENCER. Aucun choix de parcours : toute nouvelle partie est
		// lineaire
		PC->SetInventoryOpen(false);
		PC->bHostFlow = false;
		PC->bInMenu = true;
		PC->SetMenuPage(EBRMenuPage::Saves);
		PC->UpdateInputMode();
		int32 NewItem = INDEX_NONE;
		for (int32 Item = 0; Item < PC->GetMenuItemCount(); ++Item)
		{
			if (PC->GetMenuSaveSlot(Item) == ABRPlayerController::MenuItemNew)
			{
				NewItem = Item;
			}
		}
		if (NewItem == INDEX_NONE)
		{
			Note(TEXT("menu : NOUVELLE PARTIE absente de la page des parties"), true);
			return true;
		}
		PC->MenuActivate(NewItem);
		const bool bPage = PC->MenuPage == EBRMenuPage::NewSave;
		const int32 Items = PC->GetMenuItemCount();
		const FString First = PC->GetMenuItemLabel(0);
		SerialMark = W->GetLevelSerial();
		NewGameSlot = BRSaves::FreeSlot();
		PC->MenuActivate(0); // COMMENCER : ABRPlayerController::StartNewSave
		Note(FString::Printf(TEXT("menu : page NOUVELLE PARTIE %s, %d element(s) (attendu 2 : \"%s\" puis RETOUR, aucun choix de parcours), emplacement %d"), Oui(bPage), Items,
			*First, NewGameSlot + 1), !bPage || Items != 2 || NewGameSlot == INDEX_NONE);
		return true;
	});
	Add(TEXT("v4.15 menu : partie lineaire au Level 0"), 1.2f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == SerialMark || W->GetLevelNumber() != 0)
		{
			if (StepTime > 80.f)
			{
				Note(TEXT("menu : la nouvelle partie n'a pas commence au Level 0 en 80 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		BRSaves::Flush();
		const UBRSaveGame* L = NewGameSlot != INDEX_NONE ? BRSaves::Load(NewGameSlot) : nullptr;
		const BRStory::FState St = W->GetStory();
		const bool bFile = L && L->LoadedVersion == UBRSaveGame::CurrentVersion && L->Version == UBRSaveGame::CurrentVersion && (L->StoryFlags & BRStory::Flag::Linear) != 0
			&& L->StoryRunSeed != 0;
		const bool bWorld = W->IsStorySession() && W->IsLinearLevel() && W->GetNetLevelFlags() == 1 && St.IntroSeed == W->GetSeed() && BRStory::LevelSeedFor(St, 0) == 0
			&& BRSaves::ActiveSlot() == NewGameSlot;
		Note(FString::Printf(TEXT("NOUVELLE PARTIE : fichier relu au format %d (format du jeu %d), parcours lineaire %s, graine du parcours %u ; Level 0 : session d'histoire %s, "
			"niveau lineaire %s (drapeaux %d), graine %u, graine du Level 0 de la partie %s"), L ? L->LoadedVersion : -1, UBRSaveGame::CurrentVersion,
			Oui(L && (L->StoryFlags & BRStory::Flag::Linear) != 0), L ? L->StoryRunSeed : 0u, Oui(W->IsStorySession()), Oui(W->IsLinearLevel()),
			static_cast<int32>(W->GetNetLevelFlags()), W->GetSeed(), Oui(St.IntroSeed == W->GetSeed())), !bFile || !bWorld);
		// Carte de la partie sur la page VOS PARTIES : pastille LINEAIRE
		PC->bInMenu = true;
		PC->SetMenuPage(EBRMenuPage::Saves);
		PC->UpdateInputMode();
		const UBRSaveGame* Card = NewGameSlot != INDEX_NONE ? PC->GetSaveInSlot(NewGameSlot) : nullptr;
		Note(FString::Printf(TEXT("page VOS PARTIES : carte de la nouvelle partie %s, pastille LINEAIRE %s"), Oui(Card != nullptr),
			Oui(Card && BRStory::IsLinear(Card->GetStory()))), !Card || !BRStory::IsLinear(Card->GetStory()));
		return true;
	});
	LimitLastStep(90.f);
	Add(TEXT("v4.15 menu : capture de la carte"), 0.5f, [this]()
	{
		Shot(TEXT("v415_menu_parcours"));
		return true;
	});
	Add(TEXT("v4.15 menu : retour au jeu"), 0.5f, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->bInMenu = false;
			PC->SetMenuPage(EBRMenuPage::Main);
			PC->UpdateInputMode();
		}
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 2-3 Niveau 2 lineaire
	Add(TEXT("v4.15 Niveau 2 : CONTINUER d'une partie lineaire"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		const bool bWritten = BRSaves::Write(StorySlot, MakeStorySave(TEXT("Parcours lineaire v4.15"), true));
		BRSaves::Flush();
		SwapTestSave(PC, BRSaves::Load(StorySlot), StorySlot); // v4.15 (correctifs 3) : sans ecriture differee heritee
		BRSaves::ReplaySession() = false;
		PC->bDevSession = false;
		PC->bInMenu = true;
		SerialMark = W->GetLevelSerial();
		LinearSeed2 = PC->ActiveSave ? BRStory::LevelSeedFor(PC->ActiveSave->GetStory(), 2) : 0u;
		PC->StartSolo(false); // CONTINUER
		Note(FString::Printf(TEXT("partie lineaire de test ecrite %s (emplacement %d) : Level 0, Niveau 1 et Poolrooms accomplis, reprise au Niveau 2, aucune session ; "
			"graine attendue du Niveau 2 : %u"), Oui(bWritten), StorySlot + 1, LinearSeed2), !bWritten || LinearSeed2 == 0);
		return true;
	});
	Add(TEXT("v4.15 Niveau 2 : graine du niveau et drapeau"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == SerialMark || W->GetLevelNumber() != 2)
		{
			if (StepTime > 80.f)
			{
				Note(TEXT("CONTINUER : Niveau 2 non charge en 80 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		FreshMission = W->GetMissionBlob();
		const bool bOk = W->GetSeed() == LinearSeed2 && W->IsLinearLevel() && W->GetNetLevelFlags() == 1 && W->IsMissionActive() && W->HasGuaranteedForwardExit()
			&& W->IsStorySession();
		Note(FString::Printf(TEXT("CONTINUER : Niveau 2, graine %u (attendue %u, BRStory::LevelSeedFor), niveau lineaire %s (drapeaux %d), mission %s, sortie garantie vers "
			"l'avant %s"), W->GetSeed(), LinearSeed2, Oui(W->IsLinearLevel()), static_cast<int32>(W->GetNetLevelFlags()), Oui(W->IsMissionActive()),
			Oui(W->HasGuaranteedForwardExit())), !bOk);
		return true;
	});
	LimitLastStep(90.f);
	Add(TEXT("v4.15 Niveau 2 : une seule sortie"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		// Tous les chunks de la vue construits (streaming au repos) ; sinon rien n'est compte : un anneau a moitie construit
		// peut n'avoir encore aucune sortie, ce ne serait pas une preuve
		const bool bSettled = Settled(W);
		if ((!bSettled || StepTime < 4.f) && StepTime < 45.f)
		{
			return false;
		}
		if (!bSettled)
		{
			Note(FString::Printf(TEXT("parcours lineaire, Niveau 2 : streaming pas au repos en 45 s (%d chunk(s) en preparation) : sorties non comptees"),
				W->GetPreparingChunkCount()), true);
			return true;
		}
		const FExitTally Tally = TallyExits(GetWorld(), W, BRStory::NextOf(2));
		const bool bOk = Tally.Chunk == 0 && Tally.Mission == 1 && Tally.MissionForward == 1;
		Note(FString::Printf(TEXT("parcours lineaire, Niveau 2 (%d chunks charges) : %d sortie(s) tiree(s) par les chunks (attendu 0), %d sortie(s) de la mission dont %d vers "
			"le Niveau %d (attendu 1) :%s"), W->GetChunkCount(), Tally.Chunk, Tally.Mission, Tally.MissionForward, BRStory::NextOf(2), *Tally.Where), !bOk);
		Shot(TEXT("v415_sortie_unique"));
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 5 Mort : le niveau recommence
	Add(TEXT("v4.15 mort : mission avancee, objets, mort"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady())
		{
			return StepTime > 20.f;
		}
		const int32 Applied = ApplySolution(W, 40, true);
		const bool bProgress = W->GetMissionBlob() != FreshMission;
		C->AddItem(EBRItem::Battery, 2);
		const int32 Batteries = C->CountItem(EBRItem::Battery);
		SerialMark = W->GetLevelSerial();
		DeathsMark = PC->ActiveSave ? PC->ActiveSave->Deaths : -1;
		Note(FString::Printf(TEXT("avant la mort : %d action(s) de la solution appliquee(s) par les regles de l'hote, etat de la mission change %s ; piles %d (depart : 1) ; "
			"morts %d"), Applied, Oui(bProgress), Batteries, DeathsMark), !bProgress || Batteries < 2);
		C->bGodMode = false;
		C->ReceiveAttack(500.f, 0.f, nullptr);
		return true;
	});
	Add(TEXT("v4.15 mort : ecran et partie pendant la mort"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (!C->IsDead())
		{
			if (StepTime > 6.f)
			{
				Note(TEXT("mort : le joueur n'est pas tombe (coup de 500)"), true);
				return true;
			}
			return false;
		}
		if (C->GetDeathTime() < 3.3f && !W->IsTransitioning())
		{
			return false; // texte du reveil affiche (apres 2,2 s), avant le fondu (4,5 s)
		}
		const BRStory::EDeathRule Rule = W->GetDeathRule(); // la regle appliquee par le monde et le texte du HUD
		BRSaves::Flush();
		const UBRSaveGame* L = BRSaves::Load(StorySlot);
		const bool bOk = Rule == BRStory::EDeathRule::RestartLevel && L && L->bPendingDeath && L->PendingDeathLevel == 2 && !L->Session.bValid && L->Deaths == DeathsMark + 1;
		Note(FString::Printf(TEXT("mort en solo, parcours lineaire : regle %d (attendu %d, le niveau recommence) ; partie : mort en attente %s, niveau de reveil %d (attendu 2), "
			"session effacee %s, morts %d -> %d"), static_cast<int32>(Rule), static_cast<int32>(BRStory::EDeathRule::RestartLevel), Oui(L && L->bPendingDeath),
			L ? L->PendingDeathLevel : -1, Oui(L && !L->Session.bValid), DeathsMark, L ? L->Deaths : -1), !bOk);
		Shot(TEXT("v415_mort_niveau_recommence"));
		return true;
	});
	Add(TEXT("v4.15 mort : le niveau recommence"), 3.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || C->IsDead() || W->GetLevelSerial() == SerialMark)
		{
			if (StepTime > 45.f)
			{
				Note(TEXT("mort : aucun reveil 45 s apres la mort"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		const BRStory::FState St = W->GetStory();
		const bool bLevel = W->GetLevelNumber() == 2 && W->GetSeed() == LinearSeed2 && W->GetLevelSerial() == SerialMark + 1 && W->IsLinearLevel();
		const bool bMission = W->IsMissionActive() && W->GetMissionBlob() == FreshMission;
		const bool bKit = C->CountItem(EBRItem::Battery) == 1 && C->CountItem(EBRItem::AlmondWater) == 1 && C->CountItem(EBRItem::Bandage) == 1;
		const bool bStory = St.Resume == 2 && !BRStory::IsDone(St, 2) && BRStory::IsLinear(St);
		Note(FString::Printf(TEXT("reveil : Niveau %d, graine %u (attendue %u, la meme), chargement %d -> %d (nouveau), mission neuve %s, equipement de depart %s (piles %d), "
			"reprise %d, Niveau 2 accompli %s"), W->GetLevelNumber(), W->GetSeed(), LinearSeed2, SerialMark, W->GetLevelSerial(), Oui(bMission), Oui(bKit),
			C->CountItem(EBRItem::Battery), St.Resume, BRStory::IsDone(St, 2) ? TEXT("OUI") : TEXT("non")), !bLevel || !bMission || !bKit || !bStory);
		Shot(TEXT("v415_mort_reveil"));
		return true;
	});
	Add(TEXT("v4.15 mort : partie relue apres le reveil"), 0.f, [this]()
	{
		BRSaves::Flush();
		const UBRSaveGame* L = BRSaves::Load(StorySlot);
		const bool bOk = L && !L->bPendingDeath && L->Deaths == DeathsMark + 1 && L->StoryResume == 2 && (L->StoryFlags & BRStory::Flag::Linear) != 0
			&& L->StoryRunSeed == TestRunSeed;
		Note(FString::Printf(TEXT("partie apres le reveil : mort resolue %s, morts %d (attendu %d), reprise %d, parcours lineaire %s, graine du parcours %u"),
			Oui(L && !L->bPendingDeath), L ? L->Deaths : -1, DeathsMark + 1, L ? L->StoryResume : -1, Oui(L && (L->StoryFlags & BRStory::Flag::Linear) != 0),
			L ? L->StoryRunSeed : 0u), !bOk);
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 6 Jeu ferme pendant la mort
	Add(TEXT("v4.15 mort : jeu ferme pendant la mort"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady())
		{
			if (StepTime > 20.f)
			{
				StepPhase = 0;
				Note(TEXT("jeu ferme pendant la mort : niveau pas pret (ou reveil deja commence) avant l'ecriture a terre"), true);
				return true;
			}
			return false;
		}
		if (StepPhase == 0)
		{
			SerialMark = W->GetLevelSerial();
			C->bGodMode = false;
			C->ReceiveAttack(500.f, 0.f, nullptr);
			StepPhase = 1;
			return false;
		}
		if (!C->IsDead())
		{
			if (StepTime > 6.f)
			{
				StepPhase = 0;
				Note(TEXT("jeu ferme pendant la mort : le joueur n'est pas tombe"), true);
				return true;
			}
			return false;
		}
		StepPhase = 0;
		// Fermeture du jeu a terre : la meme ecriture bloquante que la sortie du jeu ; le fichier est garde tel quel
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		DeathFileBytes.Reset();
		FFileHelper::LoadFileToArray(DeathFileBytes, *BRSaves::FilePath(BRSaves::SlotName(StorySlot)));
		const UBRSaveGame* L = BRSaves::Load(StorySlot);
		const bool bOk = L && L->bPendingDeath && L->PendingDeathLevel == 2 && !L->Session.bValid && DeathFileBytes.Num() > 0;
		Note(FString::Printf(TEXT("fichier ecrit a terre : mort en attente %s, niveau de reveil %d (attendu 2), session effacee %s"), Oui(L && L->bPendingDeath),
			L ? L->PendingDeathLevel : -1, Oui(L && !L->Session.bValid)), !bOk);
		return true;
	});
	Add(TEXT("v4.15 mort : CONTINUER apres la fermeture"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C)
		{
			return false;
		}
		// Le jeu n'est pas vraiment ferme : la mort va a son terme ; puis le fichier ecrit a terre est remis tel quel (le jeu
		// aurait ete ferme a ce moment) et la partie est reprise par CONTINUER
		if (W->IsTransitioning() || !W->IsLevelReady() || C->IsDead() || W->GetLevelSerial() == SerialMark)
		{
			if (StepTime > 45.f)
			{
				Note(TEXT("jeu ferme pendant la mort : aucun reveil en 45 s"), true);
				return true;
			}
			return false;
		}
		if (StepPhase == 0)
		{
			StepPhase = 1;
			ClockMark = StepTime;
			return false;
		}
		if (StepTime - ClockMark < 3.5f)
		{
			return false; // l'ecriture d'apres le reveil (2 s) est passee
		}
		StepPhase = 0;
		C->bGodMode = true;
		BRSaves::Flush();
		const bool bRestored = DeathFileBytes.Num() > 0 && FFileHelper::SaveArrayToFile(DeathFileBytes, *BRSaves::FilePath(BRSaves::SlotName(StorySlot)));
		// Le chemin du joueur au lancement du jeu : VOS PARTIES (parties relues du disque), choix de la partie (SelectSave
		// resout la mort en attente), puis CONTINUER. Le monde montre deja le Niveau 2 avec sa graine : seule la mort
		// resolue doit en forcer le rechargement (mission neuve)
		PC->bInMenu = true;
		PC->SetMenuPage(EBRMenuPage::Saves);
		const UBRSaveGame* Card = PC->GetSaveInSlot(StorySlot);
		const bool bPending = Card && Card->bPendingDeath && Card->PendingDeathLevel == 2;
		PC->SelectSave(StorySlot);
		const int32 Cont = PC->GetContinueLevel();
		const bool bResolved = PC->ActiveSave && PC->ActiveSave.Get() == Card && !PC->ActiveSave->bPendingDeath && Cont == 2;
		const bool bSameWorld = W->GetLevelNumber() == 2 && W->GetSeed() == LinearSeed2;
		SerialMark = W->GetLevelSerial();
		PC->StartSolo(false); // CONTINUER
		Note(FString::Printf(TEXT("relance : fichier d'avant la fermeture remis %s, mort en attente au Niveau 2 relue par la page des parties %s ; partie choisie : mort resolue %s, "
			"CONTINUER vers le Niveau %d (attendu 2) ; le monde montrait deja ce niveau et sa graine %s"), Oui(bRestored), Oui(bPending), Oui(bResolved), Cont, Oui(bSameWorld)),
			!bRestored || !bPending || !bResolved);
		return true;
	});
	LimitLastStep(70.f);
	Add(TEXT("v4.15 mort : Niveau 2 recharge avec sa graine"), 3.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == SerialMark)
		{
			if (StepTime > 80.f)
			{
				Note(TEXT("CONTINUER apres la fermeture : aucun niveau charge en 80 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		const bool bOk = W->GetLevelNumber() == 2 && W->GetSeed() == LinearSeed2 && W->IsLinearLevel() && !C->IsDead() && C->CountItem(EBRItem::Battery) == 1 && PC->ActiveSave
			&& !PC->ActiveSave->bPendingDeath && W->GetMissionBlob() == FreshMission;
		Note(FString::Printf(TEXT("CONTINUER apres la fermeture a terre : Niveau %d, graine %u (attendue %u), niveau lineaire %s, debout %s, equipement de depart %s, "
			"mission neuve %s, mort resolue %s"), W->GetLevelNumber(), W->GetSeed(), LinearSeed2, Oui(W->IsLinearLevel()), Oui(!C->IsDead()),
			Oui(C->CountItem(EBRItem::Battery) == 1), Oui(W->GetMissionBlob() == FreshMission), Oui(PC->ActiveSave && !PC->ActiveSave->bPendingDeath)), !bOk);
		return true;
	});
	LimitLastStep(90.f);

	// ---------------------------------------------------------------------------------------------- 4 et 8 Partie classique
	Add(TEXT("v4.15 classique : meme graine"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady())
		{
			return StepTime > 20.f;
		}
		const bool bWritten = BRSaves::Write(ClassicSlot, MakeStorySave(TEXT("Parcours classique v4.15"), false));
		BRSaves::Flush();
		SwapTestSave(PC, BRSaves::Load(ClassicSlot), ClassicSlot); // v4.15 (correctifs 3) : sans ecriture differee heritee
		BRSaves::ReplaySession() = false;
		PC->bInMenu = false;
		PC->bDevSession = false;
		W->SetStory(PC->ActiveSave ? PC->ActiveSave->GetStory() : BRStory::FState(), EBRSessionMode::Story, true);
		SerialMark = W->GetLevelSerial();
		W->RequestTransition(2, false, LinearSeed2);
		Note(FString::Printf(TEXT("partie classique de test ecrite %s (emplacement %d), meme etat, Niveau 2 charge avec la meme graine %u"), Oui(bWritten), ClassicSlot + 1,
			LinearSeed2), !bWritten);
		return true;
	});
	Add(TEXT("v4.15 classique : sorties des chunks"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		const bool bThere = Settled(W) && W->GetLevelSerial() != SerialMark && W->GetLevelNumber() == 2;
		if ((!bThere || StepTime < 6.f) && StepTime < 50.f)
		{
			return false;
		}
		if (!bThere)
		{
			Note(TEXT("partie classique : Niveau 2 non charge en 50 s"), true);
			return true;
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		const FExitTally Tally = TallyExits(GetWorld(), W, BRStory::NextOf(2));
		const bool bClassic = !W->IsLinearLevel() && W->GetNetLevelFlags() == 0 && W->GetSeed() == LinearSeed2;
		Note(FString::Printf(TEXT("parcours classique, meme graine %u : niveau lineaire %s (drapeaux %d) ; %d sortie(s) tiree(s) par les chunks, %d de la mission :%s"),
			W->GetSeed(), W->IsLinearLevel() ? TEXT("OUI") : TEXT("non"), static_cast<int32>(W->GetNetLevelFlags()), Tally.Chunk, Tally.Mission, *Tally.Where), !bClassic);
		if (Tally.Chunk == 0)
		{
			Skip(TEXT("sorties classiques : aucune dans le rayon charge pour cette graine (contraste avec le parcours lineaire non observe)"));
		}
		Shot(TEXT("v415_sorties_classiques"));
		return true;
	});
	Add(TEXT("v4.15 classique : mort au Niveau 2"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady())
		{
			return StepTime > 20.f;
		}
		SerialMark = W->GetLevelSerial();
		C->bGodMode = false;
		C->ReceiveAttack(500.f, 0.f, nullptr);
		return true;
	});
	Add(TEXT("v4.15 classique : retour au Level 0"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if ((W->IsTransitioning() || !W->IsLevelReady() || C->IsDead() || W->GetLevelSerial() == SerialMark) && StepTime < 45.f)
		{
			return false;
		}
		C->bGodMode = true;
		const bool bOk = W->GetLevelNumber() == 0 && !C->IsDead() && W->GetLevelSerial() != SerialMark;
		Note(FString::Printf(TEXT("partie classique, mort en solo au Niveau 2 : reveil au Niveau %d (attendu 0, regle inchangee)"), W->GetLevelNumber()), !bOk);
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 8 bis Classique, jeu ferme a terre
	// Partie classique fermee pendant une mort en solo : la mort va a son terme comme en jeu, CONTINUER ramene au Level 0
	// (avant la v4.15, CONTINUER suivait la reprise de l'histoire, restee au niveau de la mort)
	Add(TEXT("v4.15 classique ferme a terre : CONTINUER au Niveau 2"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		const bool bWritten = BRSaves::Write(ClassicSlot, MakeStorySave(TEXT("Parcours classique v4.15"), false));
		BRSaves::Flush();
		SwapTestSave(PC, BRSaves::Load(ClassicSlot), ClassicSlot); // v4.15 (correctifs 3) : sans ecriture differee heritee
		BRSaves::ReplaySession() = false;
		PC->bDevSession = false;
		PC->bInMenu = true;
		SerialMark = W->GetLevelSerial();
		StepPhase = 0;
		PC->StartSolo(false); // CONTINUER : reprise de l'histoire au Niveau 2
		Note(FString::Printf(TEXT("partie classique de test reecrite %s : reprise au Niveau 2, CONTINUER"), Oui(bWritten)), !bWritten);
		return true;
	});
	Add(TEXT("v4.15 classique ferme a terre : mort et ecriture"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (StepPhase == 0)
		{
			if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == SerialMark || W->GetLevelNumber() != 2)
			{
				if (StepTime > 80.f)
				{
					Note(TEXT("partie classique : Niveau 2 non charge par CONTINUER en 80 s"), true);
					return true;
				}
				return false;
			}
			SerialMark = W->GetLevelSerial();
			C->bGodMode = false;
			C->ReceiveAttack(500.f, 0.f, nullptr);
			StepPhase = 1;
			ClockMark = StepTime;
			return false;
		}
		if (!W || !PC || !C || !C->IsDead())
		{
			if (StepTime - ClockMark > 6.f)
			{
				StepPhase = 0;
				Note(TEXT("partie classique : le joueur n'est pas tombe"), true);
				return true;
			}
			return false;
		}
		StepPhase = 0;
		// Fermeture du jeu a terre (meme ecriture bloquante que la sortie du jeu) ; le fichier est garde tel quel
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		DeathFileBytes.Reset();
		FFileHelper::LoadFileToArray(DeathFileBytes, *BRSaves::FilePath(BRSaves::SlotName(ClassicSlot)));
		const UBRSaveGame* L = BRSaves::Load(ClassicSlot);
		const bool bOk = L && L->bPendingDeath && L->PendingDeathLevel == 0 && L->StoryResume == 2 && !L->Session.bValid && DeathFileBytes.Num() > 0;
		Note(FString::Printf(TEXT("partie classique, fichier ecrit a terre : mort en attente %s, niveau de reveil %d (attendu 0), reprise de l'histoire encore %d, session effacee %s"),
			Oui(L && L->bPendingDeath), L ? L->PendingDeathLevel : -1, L ? L->StoryResume : -1, Oui(L && !L->Session.bValid)), !bOk);
		return true;
	});
	LimitLastStep(90.f);
	Add(TEXT("v4.15 classique ferme a terre : CONTINUER apres la fermeture"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C)
		{
			return false;
		}
		// La mort va a son terme dans ce jeu (Level 0), puis le fichier ecrit a terre est remis tel quel et la partie est
		// reprise par le chemin du menu
		if (W->IsTransitioning() || !W->IsLevelReady() || C->IsDead() || W->GetLevelSerial() == SerialMark)
		{
			if (StepTime > 45.f)
			{
				Note(TEXT("partie classique fermee a terre : aucun reveil en 45 s"), true);
				return true;
			}
			return false;
		}
		if (StepPhase == 0)
		{
			StepPhase = 1;
			ClockMark = StepTime;
			return false;
		}
		if (StepTime - ClockMark < 3.5f)
		{
			return false; // l'ecriture d'apres le reveil (2 s) est passee
		}
		StepPhase = 0;
		C->bGodMode = true;
		BRSaves::Flush();
		const bool bRestored = DeathFileBytes.Num() > 0 && FFileHelper::SaveArrayToFile(DeathFileBytes, *BRSaves::FilePath(BRSaves::SlotName(ClassicSlot)));
		PC->bInMenu = true;
		PC->SetMenuPage(EBRMenuPage::Saves);
		const UBRSaveGame* Card = PC->GetSaveInSlot(ClassicSlot);
		const bool bPending = Card && Card->bPendingDeath && Card->PendingDeathLevel == 0 && Card->StoryResume == 2;
		PC->SelectSave(ClassicSlot);
		const int32 Cont = PC->GetContinueLevel();
		const bool bResolved = PC->ActiveSave && PC->ActiveSave.Get() == Card && !PC->ActiveSave->bPendingDeath && PC->ActiveSave->GetStory().Resume == 0 && Cont == 0;
		SerialMark = W->GetLevelSerial();
		PC->StartSolo(false); // CONTINUER
		Note(FString::Printf(TEXT("partie classique relancee : fichier d'avant la fermeture remis %s, mort en attente relue %s ; partie choisie : mort resolue %s, CONTINUER vers "
			"le Niveau %d (attendu 0, la regle de la mort en solo)"), Oui(bRestored), Oui(bPending), Oui(bResolved), Cont), !bRestored || !bPending || !bResolved);
		return true;
	});
	LimitLastStep(70.f);
	Add(TEXT("v4.15 classique ferme a terre : reprise au Level 0"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == SerialMark)
		{
			if (StepTime > 80.f)
			{
				Note(TEXT("partie classique fermee a terre : aucun niveau charge par CONTINUER en 80 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		const bool bOk = W->GetLevelNumber() == 0 && !C->IsDead() && !W->IsLinearLevel() && W->IsStorySession() && C->CountItem(EBRItem::Battery) == 1;
		Note(FString::Printf(TEXT("partie classique fermee a terre, CONTINUER : Niveau %d (attendu 0), niveau classique %s, session d'histoire %s, equipement de depart %s"),
			W->GetLevelNumber(), Oui(!W->IsLinearLevel()), Oui(W->IsStorySession()), Oui(C->CountItem(EBRItem::Battery) == 1)), !bOk);
		return true;
	});
	LimitLastStep(90.f);

	// ---------------------------------------------------------------------------------------------- 7 Marche et sortie unique
	Add(TEXT("v4.15 marche : retour au parcours lineaire"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		BRSaves::Flush();
		SwapTestSave(PC, BRSaves::Load(StorySlot), StorySlot); // v4.15 (correctifs 3) : sans ecriture differee heritee
		BRSaves::ReplaySession() = false;
		PC->bDevSession = false;
		PC->bInMenu = true;
		SerialMark = W->GetLevelSerial();
		PC->StartSolo(false);
		return true;
	});
	Add(TEXT("v4.15 marche : Niveau 2 du parcours"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == SerialMark || W->GetLevelNumber() != 2)
		{
			if (StepTime > 80.f)
			{
				Note(TEXT("marche : Niveau 2 du parcours non charge en 80 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		TestMissionPhase = 0;
		Note(FString::Printf(TEXT("marche : Niveau 2, graine %u (attendue %u), niveau lineaire %s"), W->GetSeed(), LinearSeed2, Oui(W->IsLinearLevel())),
			W->GetSeed() != LinearSeed2 || !W->IsLinearLevel());
		return true;
	});
	LimitLastStep(90.f);
	AddWalkMissionSteps(2, TestRunSeed, false);
	AddWalkExitSteps(2);
	Add(TEXT("v4.15 marche : Niveau 3, graine du parcours"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		// v4.15 (correctifs 4, relecture) : sortie du Niveau 2 NON VERIFIEE a pied plus haut (mission pas faite a pied, entite seule
		// sur le passage) : l'arrivee n'est pas jugee (avant : PROBLEME "Niveau 3 non atteint" en cascade)
		const FString NotVerified = BRTestV412::ExitNotVerified(2);
		if (!NotVerified.IsEmpty())
		{
			Skip(FString::Printf(TEXT("marche : %s : Niveau 3 par la sortie unique non verifie"), *NotVerified));
			return true;
		}
		const bool bThere = Settled(W) && W->GetLevelNumber() == 3;
		if ((!bThere || StepTime < 4.f) && StepTime < 45.f)
		{
			return false;
		}
		if (!bThere)
		{
			Note(TEXT("marche : Niveau 3 non atteint par la sortie unique du Niveau 2"), true);
			return true;
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		const BRStory::FState St = W->GetStory();
		const uint32 Want = BRStory::LevelSeedFor(St, 3);
		const FExitTally Tally = TallyExits(GetWorld(), W, BRStory::NextOf(3));
		const bool bOk = BRStory::IsDone(St, 2) && St.Resume == 3 && Want != 0 && W->GetSeed() == Want && W->IsLinearLevel() && Tally.Chunk == 0 && Tally.MissionForward == 1;
		Note(FString::Printf(TEXT("Niveau 3 par la sortie unique : Niveau 2 accompli %s, reprise %d, graine %u (attendue %u), niveau lineaire %s ; %d sortie(s) des chunks "
			"(attendu 0), %d vers le Niveau %d :%s"), Oui(BRStory::IsDone(St, 2)), St.Resume, W->GetSeed(), Want, Oui(W->IsLinearLevel()), Tally.Chunk, Tally.MissionForward,
			BRStory::NextOf(3), *Tally.Where), !bOk);
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 7 bis Version publiee
	Add(TEXT("v4.15 version publiee : Niveau 2 du parcours"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		BRLevels::SetChannelOverride(static_cast<int32>(BRContent::EChannel::Public));
		const bool bWritten = BRSaves::Write(StorySlot, MakeStorySave(TEXT("Parcours lineaire v4.15 (version publiee)"), true));
		BRSaves::Flush();
		SwapTestSave(PC, BRSaves::Load(StorySlot), StorySlot); // v4.15 (correctifs 3) : sans ecriture differee heritee
		BRSaves::ReplaySession() = false;
		PC->bDevSession = false;
		PC->bInMenu = true;
		SerialMark = W->GetLevelSerial();
		TestMissionPhase = 0;
		PC->StartSolo(false);
		Note(FString::Printf(TEXT("version publiee : partie lineaire reecrite %s, CONTINUER vers le Niveau 2"), Oui(bWritten)), !bWritten);
		return true;
	});
	Add(TEXT("v4.15 version publiee : mission resolue par les regles de l'hote"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		const bool bThere = Settled(W) && W->GetLevelSerial() != SerialMark && W->GetLevelNumber() == 2;
		if ((!bThere || StepTime < 3.f) && StepTime < 80.f)
		{
			return false;
		}
		if (!bThere)
		{
			Note(TEXT("version publiee : Niveau 2 non charge en 80 s"), true);
			return true;
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		const int32 Applied = ApplySolution(W, BRMission::MaxSolveSteps, false);
		BRMission::FEval E;
		W->GetMissionEval(E);
		const FExitTally Tally = TallyExits(GetWorld(), W, BRStory::NextOf(2));
		const bool bOk = E.bSolved && W->GetSeed() == LinearSeed2 && W->IsLinearLevel() && Tally.Chunk == 0 && Tally.MissionForward == 1;
		Note(FString::Printf(TEXT("version publiee, Niveau 2 : graine %u (attendue %u, la meme dans tous les canaux), %d action(s) appliquee(s), mission resolue %s ; "
			"%d sortie(s) des chunks, %d de la mission vers le Niveau %d"), W->GetSeed(), LinearSeed2, Applied, Oui(E.bSolved), Tally.Chunk, Tally.MissionForward,
			BRStory::NextOf(2)), !bOk);
		return true;
	});
	LimitLastStep(90.f);
	AddWalkExitSteps(2);
	Add(TEXT("v4.15 version publiee : fin du chapitre"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC)
		{
			return false;
		}
		const BRStory::FState St = W->GetStory();
		const int32 Lot = BRContent::CurrentLot(BRLevels::Channel());
		const int32 Cont = PC->GetContinueLevel();
		const bool bOk = BRStory::IsDone(St, 2) && (St.ChapterEnds & (1u << Lot)) != 0 && BRStory::IsLinear(St) && Cont == 2;
		Note(FString::Printf(TEXT("version publiee : fin du chapitre %d enregistree %s, Niveau 2 accompli %s, parcours lineaire garde %s, CONTINUER au Niveau %d (attendu 2)"), Lot,
			Oui((St.ChapterEnds & (1u << Lot)) != 0), Oui(BRStory::IsDone(St, 2)), Oui(BRStory::IsLinear(St)), Cont), !bOk);
		BRLevels::SetChannelOverride(static_cast<int32>(BRContent::EChannel::All));
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 10 REJOUER
	Add(TEXT("v4.15 REJOUER dans une partie lineaire"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		DurableMark = DurableOf(BRSaves::Load(StorySlot));
		SwapTestSave(PC, BRSaves::Load(StorySlot), StorySlot); // v4.15 (correctifs 3) : sans ecriture differee heritee
		PC->bDevSession = false;
		PC->bInMenu = true;
		PC->MenuIndex = FMath::Max(0, BRLevels::IndexOf(1));
		SerialMark = W->GetLevelSerial();
		PC->StartSolo(true); // REJOUER le Niveau 1
		return true;
	});
	Add(TEXT("v4.15 REJOUER : niveau classique, partie intacte"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == SerialMark || W->GetLevelNumber() != 1)
		{
			if (StepTime > 80.f)
			{
				Note(TEXT("REJOUER : Niveau 1 non charge en 80 s"), true);
				return true;
			}
			return false;
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		const bool bReplay = W->GetSessionMode() == EBRSessionMode::Replay && !PC->WritesCampaign();
		const bool bClassic = !W->IsLinearLevel() && W->GetNetLevelFlags() == 0;
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		const FString After = DurableOf(BRSaves::Load(StorySlot));
		Note(FString::Printf(TEXT("REJOUER dans une partie lineaire : session de rejeu %s, niveau classique %s (drapeaux %d) ; partie du fichier inchangee %s%s"), Oui(bReplay),
			Oui(bClassic), static_cast<int32>(W->GetNetLevelFlags()), Oui(After == DurableMark), After == DurableMark ? TEXT("") : *(TEXT(" ; maintenant : ") + After)),
			!bReplay || !bClassic || After != DurableMark);
		return true;
	});
	LimitLastStep(90.f);

	// ---------------------------------------------------------------------------------------------- 10 bis Mode developpeur
	// REJOUER d'un niveau pas encore explore (le chemin du menu vers le mode developpeur, StartSolo) depuis la partie
	// lineaire : session libre, niveau classique (ni drapeau ni graine du parcours), rien d'ecrit
	Add(TEXT("v4.15 mode developpeur dans une partie lineaire"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		BRSaves::Flush();
		SwapTestSave(PC, BRSaves::Load(StorySlot), StorySlot); // v4.15 (correctifs 3) : sans ecriture differee heritee
		DevLevel = INDEX_NONE;
		for (const int32 Lv : { 3, 4, 5, 6, 7 })
		{
			if (DevLevel == INDEX_NONE && BRLevels::Exists(Lv) && PC->ActiveSave && !PC->ActiveSave->IsExplored(Lv))
			{
				DevLevel = Lv;
			}
		}
		if (DevLevel == INDEX_NONE)
		{
			Note(TEXT("mode developpeur : aucun niveau inexplore disponible dans la partie lineaire de test"), true);
			return true;
		}
		PC->bDevSession = false;
		PC->bInMenu = true;
		PC->MenuIndex = FMath::Max(0, BRLevels::IndexOf(DevLevel));
		SerialMark = W->GetLevelSerial();
		PC->StartSolo(true); // REJOUER d'un niveau inexplore : mode developpeur
		Note(FString::Printf(TEXT("mode developpeur : REJOUER du Niveau %d (inexplore) depuis la partie lineaire, session du mode developpeur %s"), DevLevel,
			Oui(PC->bDevSession)), !PC->bDevSession);
		return true;
	});
	Add(TEXT("v4.15 mode developpeur : niveau classique, partie intacte"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (DevLevel == INDEX_NONE)
		{
			return true;
		}
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == SerialMark || W->GetLevelNumber() != DevLevel)
		{
			if (StepTime > 80.f)
			{
				Note(FString::Printf(TEXT("mode developpeur : Niveau %d non charge en 80 s"), DevLevel), true);
				return true;
			}
			return false;
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		const uint32 Derived = PC->ActiveSave ? BRStory::LevelSeedFor(PC->ActiveSave->GetStory(), DevLevel) : 0u;
		const bool bDev = PC->bDevSession && W->GetSessionMode() == EBRSessionMode::Free && !PC->WritesCampaign();
		const bool bClassic = !W->IsLinearLevel() && W->GetNetLevelFlags() == 0 && Derived != 0 && W->GetSeed() != Derived
			&& W->GetDeathRule() == BRStory::EDeathRule::BackToStart;
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		const FString After = DurableOf(BRSaves::Load(StorySlot));
		Note(FString::Printf(TEXT("mode developpeur dans une partie lineaire : session libre %s ; niveau classique %s (drapeaux %d, graine %u, graine du parcours %u non utilisee, "
			"mort : regle %d) ; partie du fichier inchangee %s%s"), Oui(bDev), Oui(bClassic), static_cast<int32>(W->GetNetLevelFlags()), W->GetSeed(), Derived,
			static_cast<int32>(W->GetDeathRule()), Oui(After == DurableMark), After == DurableMark ? TEXT("") : *(TEXT(" ; maintenant : ") + After)),
			!bDev || !bClassic || After != DurableMark);
		return true;
	});
	LimitLastStep(90.f);

	// ---------------------------------------------------------------------------------------------- 9 Migration 5 -> 6
	Add(TEXT("v4.15 migration : format 5 -> 6"), 0.f, [this]()
	{
		// Partie de la v4.14 (format 5) ecrite sans BRSaves. Les champs du parcours n'existaient pas : s'ils portaient une
		// valeur, la migration doit quand meme rendre une partie classique
		UBRSaveGame* Old = NewObject<UBRSaveGame>();
		Old->Version = 5;
		Old->SaveName = TEXT("Partie v4.14 (format 5)");
		Old->Created = FDateTime::Now();
		for (const int32 Lv : { 0, 1, 37, 2 })
		{
			Old->MarkExplored(Lv);
		}
		Old->CurrentLevel = 2;
		BRStory::FState St;
		BRStory::SetDone(St, 0);
		BRStory::SetDone(St, 1);
		BRStory::SetDone(St, BRStory::Poolrooms);
		St.Reached = BRStory::IndexOf(2);
		St.Resume = 2;
		St.IntroSeed = 4157u;
		St.DoorX = 7;
		St.DoorY = -3;
		St.DoorDir = 2;
		St.DoorTier = 1;
		St.DoorVersion = BRStory::DoorPlaceVersion;
		St.Flags = BRStory::Flag::Linear;
		St.RunSeed = 77u;
		Old->SetStory(St);
		TArray<uint8> OldBytes;
		UGameplayStatics::SaveGameToMemory(Old, OldBytes);
		FFileHelper::SaveArrayToFile(OldBytes, *BRSaves::FilePath(BRSaves::SlotName(MigrationSlot)));
		const UBRSaveGame* L = BRSaves::Load(MigrationSlot);
		TArray<uint8> LegacyCopy;
		const bool bCopy = FFileHelper::LoadFileToArray(LegacyCopy, *BRSaves::FilePath(BRSaves::LegacySlotName(MigrationSlot, 5))) && LegacyCopy == OldBytes;
		const BRStory::FState M = L ? L->GetStory() : BRStory::FState();
		const bool bKept = M.Reached == St.Reached && M.Done == St.Done && M.Resume == 2 && M.IntroSeed == 4157u && M.DoorX == 7 && M.DoorY == -3 && M.DoorDir == 2
			&& M.DoorTier == 1 && L && L->Explored.Num() == 4;
		const bool bOk = L && bCopy && L->LoadedVersion == 5 && L->Version == UBRSaveGame::CurrentVersion && M.Flags == 0 && M.RunSeed == 0 && bKept;
		Note(FString::Printf(TEXT("format 5 -> %d : copie d'origine (_Format5) identique %s ; parcours classique %s (drapeaux %u, graine %u) ; etapes, reprise, Level 0 et porte "
			"gardes %s"), UBRSaveGame::CurrentVersion, Oui(bCopy), Oui(M.Flags == 0 && M.RunSeed == 0), M.Flags, M.RunSeed, Oui(bKept)), !bOk);
		return true;
	});

	// ---------------------------------------------------------------------------------------------- 11 Protocole
	Add(TEXT("v4.15 protocole"), 0.f, [this]()
	{
		const FString Options = BRLevels::JoinOptions();
		const FString Label = BRLevels::VersionLabel(BRContent::NetVersion);
		const bool bOk = BRContent::NetVersion == 415 && Options.Contains(TEXT("?BRNet=415")) && Label == TEXT("4.15") && FString(BR_GAME_VERSION) == TEXT("v4.15");
		Note(FString::Printf(TEXT("protocole %d, options d'entree \"%s\", version affichee %s, jeu %s"), BRContent::NetVersion, *Options, *Label, BR_GAME_VERSION), !bOk);
		return true;
	});

	Add(TEXT("v4.15 : retour a l'etat d'avant le test"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (PC)
		{
			PC->bDevSession = bTestDevSession;
			SwapTestSave(PC, TestOrigSave, TestOrigSlot); // v4.15 (correctifs 3) : sans ecriture differee heritee
			PC->bInMenu = bInMenuMark;
			PC->bHostFlow = bHostFlowMark;
			PC->bPendingDeathResolved = false;
		}
		TestOrigSave = nullptr;
		BRSaves::ActiveSlot() = TestOrigSlot;
		BRSaves::ReplaySession() = bReplayMark;
		BRLevels::SetChannelOverride(-1);
		if (W)
		{
			W->SetLinearTestOverride(-1);
			W->SetStory(BRStory::FState(), EBRSessionMode::Free);
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		StepPhase = 0;
		TestMissionPhase = 0;
		DeleteV415Saves();
		BRSaves::SetTestPrefix(FString());
		// La page du menu est remise apres les parties de test et leur prefixe : la page VOS PARTIES relit alors les parties
		// du joueur, pas celles du test qui viennent d'etre effacees
		if (PC)
		{
			PC->SetMenuPage(MenuPageMark);
			PC->MenuIndex = MenuIndexMark;
			PC->UpdateInputMode();
		}
		Note(TEXT("v4.15 : partie active, mode, menu (page, niveau choisi, hebergement), canal de contenu et parties de test retablis"));
		return true;
	});
}

// =====================================================================================================================
// Etapes v4.15 (reseau) : parcours lineaire impose par l'hote, meme niveau chez chacun, mort du client dans le niveau
// =====================================================================================================================

void ABRAutoTest::AddNetV415Steps(bool bClient)
{
	using namespace BRTestV415;
	// v4.15 : barriere : l'hote ne lance le changement de niveau qu'une fois le client arrive a cette etape (il signale son
	// arrivee, BRTestNet). Avant, le client, plus lent, y entrait apres le debut de la transition : il relevait le numero du
	// Niveau 2 deja en chargement et ne voyait jamais "un autre chargement" (ses verifications v4.15 ne se faisaient pas)
	Add(TEXT("v4.15 reseau : parcours lineaire impose par l'hote"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		if (bClient)
		{
			SerialMark = W->GetLevelSerial();
			StepPhase = 0;
			BRTestNet::SendMark(GetPC(), BRTestNet::MarkV415Start);
			return true;
		}
		const int32 Mark = BRTestNet::RemoteMark(W);
		if (Mark != BRTestNet::MarkV415Start && Mark != BRTestNet::MarkFinished && BRTestNet::RemoteMate(W))
		{
			if (StepTime < 90.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("hote : le client n'est pas arrive a l'etape v4.15 en 90 s (derniere etape signalee %d) : niveau change quand meme"), Mark), true);
		}
		SerialMark = W->GetLevelSerial();
		StepPhase = 0;
		W->SetLinearTestOverride(1);
		W->RequestTransition(2, false, ABRWorld::SeedFromUser(NetUserSeed, 2));
		return true;
	});
	LimitLastStep(100.f);
	Add(TEXT("v4.15 reseau : Niveau 2 lineaire chez chacun"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		const bool bThere = Settled(W) && W->GetLevelNumber() == 2 && W->GetLevelSerial() != SerialMark;
		if ((!bThere || StepTime < 6.f) && StepTime < 80.f)
		{
			return false;
		}
		const TCHAR* Who = bClient ? TEXT("client") : TEXT("hote");
		if (!bThere)
		{
			Note(FString::Printf(TEXT("%s : Niveau 2 du parcours lineaire non charge en 80 s"), Who), true);
			return true;
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		const FExitTally Tally = TallyExits(GetWorld(), W, BRStory::NextOf(2));
		const uint32 Want = ABRWorld::SeedFromUser(NetUserSeed, 2);
		const bool bOk = W->GetNetLevelFlags() == 1 && W->IsLinearLevel() && W->GetSeed() == Want && Tally.Chunk == 0 && Tally.Mission == 1 && Tally.MissionForward == 1;
		// La cellule de la sortie de la mission figure dans les deux rapports : elles doivent etre identiques
		Note(FString::Printf(TEXT("%s : Niveau 2, graine %u (attendue %u), drapeaux %d, niveau lineaire %s ; %d sortie(s) des chunks (attendu 0), %d de la mission (attendu 1) :%s"),
			Who, W->GetSeed(), Want, static_cast<int32>(W->GetNetLevelFlags()), Oui(W->IsLinearLevel()), Tally.Chunk, Tally.Mission, *Tally.Where), !bOk);
		SerialMark = W->GetLevelSerial();
		return true;
	});
	LimitLastStep(90.f);
	// Le client tombe (coup de l'hote, comme une entite) et se reveille dans le niveau, parcours lineaire ou non
	Add(TEXT("v4.15 reseau : mort du client"), 0.f, [this, bClient]()
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
				if (((Other->NetFlags & 16) != 0 || Other->IsLevelLoading()) && StepTime < 30.f)
				{
					return false;
				}
				if ((Other->NetFlags & 16) != 0 || Other->IsLevelLoading())
				{
					// v4.15 : client pas pret : un coup ne lui ferait rien (invincible) et la suite noterait a tort "pas vu a
					// terre" : un seul probleme, la verification du reveil est sautee
					Note(FString::Printf(TEXT("hote : client pas pret apres 30 s (%s) : mort et reveil du client non verifies"),
						Other->IsLevelLoading() ? TEXT("encore en chargement") : TEXT("toujours invincible")), true);
					NetMate.Reset();
					return true;
				}
				Other->bGodMode = false;
				Other->ReceiveAttack(500.f, 0.f, nullptr);
				NetMate = Other;
				return true;
			}
		}
		NetMate.Reset();
		Note(TEXT("v4.15 reseau : aucun client a mettre a terre"), true);
		return true;
	});
	Add(TEXT("v4.15 reseau : reveil dans le niveau"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (bClient)
		{
			ABRCharacter* Me = GetPlayer();
			if (!Me)
			{
				return false;
			}
			if (StepPhase == 0)
			{
				if (!Me->IsDead())
				{
					if (StepTime > 15.f)
					{
						Note(TEXT("client : pas tombe apres le coup de l'hote"), true);
						Me->bGodMode = true;
						return true;
					}
					return false;
				}
				StepPhase = 1;
				return false;
			}
			if ((Me->IsDead() || W->IsTransitioning()) && StepTime < 55.f)
			{
				return false;
			}
			StepPhase = 0;
			const bool bOk = !Me->IsDead() && W->GetLevelNumber() == 2 && W->GetLevelSerial() == SerialMark && W->IsLinearLevel();
			Note(FString::Printf(TEXT("client : a terre puis reveille dans le niveau %s (Niveau %d, meme chargement %s, apres %.0f s) : regle de la cooperation"), Oui(!Me->IsDead()),
				W->GetLevelNumber(), Oui(W->GetLevelSerial() == SerialMark), StepTime), !bOk);
			Me->bGodMode = true;
			return true;
		}
		const ABRCharacter* Mate = NetMate.Get();
		if (!Mate)
		{
			return true;
		}
		const FBRDeathState& DS = Mate->GetDeathState();
		if (StepPhase == 0)
		{
			if (!DS.bDead)
			{
				if (StepTime > 15.f)
				{
					Note(TEXT("hote : le client n'est pas vu a terre"), true);
					return true;
				}
				return false;
			}
			StepPhase = 1;
			return false;
		}
		if (DS.bDead && StepTime < 55.f)
		{
			return false;
		}
		StepPhase = 0;
		const bool bOk = !DS.bDead && W->GetLevelNumber() == 2 && W->GetLevelSerial() == SerialMark;
		Note(FString::Printf(TEXT("hote : client reveille %s, groupe toujours au Niveau 2 sans rechargement %s (%.0f s)"), Oui(!DS.bDead), Oui(W->GetLevelSerial() == SerialMark),
			StepTime), !bOk);
		if (ABRCharacter* M = NetMate.Get())
		{
			M->bGodMode = true;
		}
		return true;
	});
	LimitLastStep(70.f);
	Add(TEXT("v4.15 reseau : retour a la regle normale"), 0.f, [this, bClient]()
	{
		if (!bClient)
		{
			if (ABRWorld* W = GetBRWorld())
			{
				W->SetLinearTestOverride(-1);
			}
		}
		return true;
	});
}
