// v4.13 : l'histoire dans le monde. L'hote tient l'etat narratif (BRStoryLogic), le replique (un client qui arrive le
// recoit) et le sauvegarde ; il valide chaque transition contre le fil. Le Level 0 n'a plus d'enigme : une porte de
// service, posee une fois dans le mur d'enceinte au bout d'un chemin garanti sans fosse, mene au Niveau 1. Reveil,
// marques de craie de L. et aide d'orientation apres une longue errance. Aucun Tick : transitions, minuteries, OnRep.
#include "BRWorld.h"
#include "BRStoryLogic.h"
#include "BRContentLogic.h"
#include "BRCharacter.h"
#include "BRInteractables.h"
#include "BRPlayerController.h"
#include "BRHUD.h"
#include "BRAssets.h"
#include "BRLevels.h"
#include "BRLoc.h"
#include "BRMission.h"
#include "Backrooms.h"

#include "Algo/Reverse.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	/** Disponibilite d'un niveau dans cette version (BRContent), pour BRStory */
	bool StoryAvailable(int Level, void*)
	{
		return BRLevels::Exists(Level);
	}

	BRStory::FState ToState(const FBRNetStory& N)
	{
		BRStory::FState S;
		S.Reached = N.Reached;
		S.Done = N.Done;
		S.Events = N.Events;
		S.ChapterEnds = N.ChapterEnds;
		S.Resume = N.Resume;
		S.Parked = N.Parked;
		S.IntroSeed = N.IntroSeed;
		S.DoorX = N.DoorX;
		S.DoorY = N.DoorY;
		S.DoorDir = N.DoorDir;
		S.DoorTier = N.DoorTier;
		S.DoorVersion = N.DoorVersion;
		return S;
	}

	void CopyState(FBRNetStory& N, const BRStory::FState& S)
	{
		N.Reached = S.Reached;
		N.Done = S.Done;
		N.Events = S.Events;
		N.ChapterEnds = S.ChapterEnds;
		N.Resume = S.Resume;
		N.Parked = S.Parked;
		N.IntroSeed = S.IntroSeed;
		N.DoorX = S.DoorX;
		N.DoorY = S.DoorY;
		N.DoorDir = S.DoorDir;
		N.DoorTier = static_cast<uint8>(FMath::Clamp(S.DoorTier, 0, 255));
		N.DoorVersion = static_cast<uint8>(FMath::Clamp(S.DoorVersion, 0, 255));
	}

	/** Face de la porte (BRStory::FState::DoorDir) : 0 +X, 1 -X, 2 +Y, 3 -Y */
	FIntPoint ServiceDoorDirOf(int32 Idx)
	{
		const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		return Idx >= 0 && Idx < 4 ? Dirs[Idx] : FIntPoint::ZeroValue;
	}

	/** Duree du reveil (entrees bloquees) et delai avant l'aide d'orientation du Level 0 */
	constexpr float IntroSeconds = 2.8f;
	constexpr float DraftHintDelay = 420.f;
}

// =====================================================================================================================
// Etat narratif
// =====================================================================================================================

EBRSessionMode ABRWorld::HostSessionMode() const
{
	const ABRPlayerController* PC = LocalPC();
	return PC ? PC->GetSessionMode() : EBRSessionMode::Free;
}

EBRSessionMode ABRWorld::GetSessionMode() const
{
	return HasAuthority() ? HostSessionMode() : static_cast<EBRSessionMode>(NetStory.Mode);
}

BRStory::FState ABRWorld::GetStory() const
{
	return ToState(NetStory);
}

void ABRWorld::EnsureStoryLoaded()
{
	if (bStoryLoaded || !HasAuthority())
	{
		return;
	}
	// Premiere construction de la carte chez l'hote (partie en ligne) : sa partie n'est appliquee qu'une fois son
	// personnage pret ; d'ici la, la sauvegarde active fait foi (meme principe que la campagne)
	const ABRPlayerController* PC = LocalPC();
	const UBRSaveGame* Save = PC ? PC->GetActiveSave() : nullptr;
	if (!Save)
	{
		return;
	}
	bStoryLoaded = true;
	const BRStory::FState S = Save->GetStory();
	SeenStoryEvents = S.Events; // deja vus dans cette partie : rien n'est rejoue
	CopyState(NetStory, S);
}

void ABRWorld::SetStory(const BRStory::FState& S, EBRSessionMode Mode, bool bLevelChanging)
{
	if (!HasAuthority())
	{
		return;
	}
	bStoryLoaded = true;
	NetStory.Mode = static_cast<uint8>(Mode);
	SeenStoryEvents = S.Events; // evenements deja vus dans cette partie : seuls les nouveaux auront un effet
	UE_LOG(LogBackrooms, Log, TEXT("[Story] Partie appliquee : mode %d, atteint %d, etapes 0x%x, evenements 0x%x, fins 0x%x, reprise %d, gare %d"),
		static_cast<int32>(Mode), S.Reached, S.Done, S.Events, S.ChapterEnds, S.Resume, S.Parked);
	if (bLevelReady && !bLevelChanging && !IsTransitioning())
	{
		CopyState(NetStory, S);
		OnStoryLevelLoaded();
		return;
	}
	PublishStory(S);
}

void ABRWorld::PublishStory(const BRStory::FState& S)
{
	if (!HasAuthority())
	{
		return;
	}
	CopyState(NetStory, S);
	NetStory.Mode = static_cast<uint8>(HostSessionMode());
	++NetStory.Rev;
	ForceNetUpdate();
	ApplyStoryLocal(false);
}

void ABRWorld::MarkStoryEvent(uint32 Event)
{
	if (!HasAuthority() || !IsStorySession() || (NetStory.Events & Event) == Event)
	{
		return;
	}
	BRStory::FState S = GetStory();
	S.Events |= Event;
	UE_LOG(LogBackrooms, Log, TEXT("[Story] Evenement 0x%x (Niveau %d)"), Event, GetLevelNumber());
	PublishStory(S);
}

void ABRWorld::OnRep_Story()
{
	// Client : le premier etat recu decrit ce qui s'est passe avant l'arrivee (pas de reveil ni d'annonce rejoues)
	const bool bInitial = !bStorySeen;
	bStorySeen = true;
	// La porte de l'hote fait foi : la meme graine et la meme version donnent la meme porte ; un ecart est signale
	if (bLevelReady && bServiceDoor && NetStory.IntroSeed == Seed && NetStory.DoorDir >= 0
		&& (ServiceDoorCell != FIntPoint(NetStory.DoorX, NetStory.DoorY) || GetServiceDoorDir() != ServiceDoorDirOf(NetStory.DoorDir)))
	{
		UE_LOG(LogBackrooms, Warning, TEXT("[Story] Porte de service : l'hote l'a en %d,%d (face %d), ce client en %d,%d"), NetStory.DoorX, NetStory.DoorY,
			NetStory.DoorDir, ServiceDoorCell.X, ServiceDoorCell.Y);
	}
	ApplyStoryLocal(bInitial);
}

bool ABRWorld::CanEnterLevel(int32 Level, FString* OutReason) const
{
	if (!BRLevels::Exists(Level))
	{
		if (OutReason)
		{
			*OutReason = BR_STR(NSLOCTEXT("BR", "Story.Unavailable", "Ce niveau n'existe pas encore dans cette version."));
		}
		return false;
	}
	const EBRSessionMode Mode = GetSessionMode();
	if (Mode == EBRSessionMode::Free)
	{
		return true;
	}
	if (Mode == EBRSessionMode::Replay)
	{
		// Rejouer : tout niveau deja decouvert dans la partie (meme au-dela des Poolrooms d'une ancienne partie), rien
		// d'autre ; l'histoire n'avance pas
		const ABRPlayerController* PC = LocalPC();
		if (PC && PC->GetActiveSave() && PC->GetActiveSave()->IsExplored(Level))
		{
			return true;
		}
	}
	switch (BRStory::CanEnter(GetStory(), Level))
	{
	case BRStory::EAccess::Allowed:
		return true;
	case BRStory::EAccess::PoolroomsRequired:
		if (OutReason)
		{
			*OutReason = BR_STR(NSLOCTEXT("BR", "Story.PoolroomsRequired", "L'équipe de L. est passée par les Poolrooms : la suite du chemin n'est pas encore ouverte."));
		}
		return false;
	default:
		if (OutReason)
		{
			*OutReason = Mode == EBRSessionMode::Replay
				? BR_STR(NSLOCTEXT("BR", "Story.ReplayBeyond", "Rejouer : ce passage mène au-delà de votre progression. Reprenez l'histoire avec CONTINUER."))
				: BR_STR(NSLOCTEXT("BR", "Story.NotReached", "Ce niveau n'est pas encore atteint dans l'histoire."));
		}
		return false;
	}
}

void ABRWorld::GetExplorationChoices(TArray<int32>& Out) const
{
	Out.Reset();
	for (const FBRLevelDef& L : BRLevels::All())
	{
		if (L.Number != GetLevelNumber() && CanEnterLevel(L.Number))
		{
			Out.Add(L.Number);
		}
	}
}

int32 ABRWorld::StoryDestination(int32 Target) const
{
	if (!IsStorySession() || Target != BRStory::NextOf(GetLevelNumber()))
	{
		return Target;
	}
	const int32 Dest = BRStory::ProgressTarget(GetStory(), GetLevelNumber(), &StoryAvailable, nullptr);
	return Dest >= 0 ? Dest : Target;
}

int32 ABRWorld::StoryOnLeave(int32 Target, uint32& OutSeed)
{
	OutSeed = 0;
	if (!HasAuthority() || !IsStorySession())
	{
		return Target;
	}
	const int32 From = GetLevelNumber();
	if (Target != BRStory::NextOf(From))
	{
		return Target; // sortie de retour : l'histoire ne bouge pas (l'arrivee met a jour la reprise)
	}
	BRStory::FState S = GetStory();
	const int32 Dest = BRStory::ProgressTarget(S, From, &StoryAvailable, nullptr);
	const BRContent::FExitResolution Res = BRLevels::ResolveExit(From, Dest == BRStory::Ending ? Target : Dest);
	if (Res.Kind == BRContent::EExit::ChapterEnd || Dest == BRStory::Ending || Res.Kind == BRContent::EExit::Ending)
	{
		// Fin du contenu disponible (ou dernier quai) : etape accomplie, fin de chapitre ; la suite reprendra au niveau
		// suivant du fil quand il sera publie
		const int32 Lot = BRContent::CurrentLot(BRLevels::Channel());
		BRStory::OnChapterEnd(S, From, Lot);
		if (From == 2)
		{
			S.Events |= BRStory::Event::StationSign;
		}
		UE_LOG(LogBackrooms, Log, TEXT("[Story] Fin de chapitre en quittant le Niveau %d (lot %d) : reprise au Niveau %d"), From, Lot, S.Resume);
		PublishStory(S);
		return Target;
	}
	if (Res.Kind != BRContent::EExit::Go)
	{
		return Target;
	}
	const int32 To = Res.Target >= 0 ? Res.Target : Dest;
	const bool bParked = From == BRStory::Poolrooms && S.Parked >= 0 && To == S.Parked;
	BRStory::OnProgress(S, From, To);
	UE_LOG(LogBackrooms, Log, TEXT("[Story] Etape accomplie : Niveau %d -> Niveau %d%s"), From, To, bParked ? TEXT(" (progression gardee d'une ancienne partie)") : TEXT(""));
	PublishStory(S);
	if (bParked)
	{
		if (ABRPlayerController* PC = LocalPC())
		{
			OutSeed = PC->TakeParkedSession(To);
		}
	}
	return To;
}

void ABRWorld::OnStoryLevelLoaded()
{
	GetWorldTimerManager().ClearTimer(DraftHintTimer);
	GetWorldTimerManager().ClearTimer(InscriptionTimer);
	if (GetLevelNumber() == BRStory::Poolrooms)
	{
		// Chaque machine, a chaque arrivee : l'information de route ne depend d'aucune collecte
		GetWorldTimerManager().SetTimer(InscriptionTimer, this, &ABRWorld::ShowPoolInscription, 9.f, false);
	}
	if (!HasAuthority())
	{
		return;
	}
	EnsureStoryLoaded();
	BRStory::FState S = GetStory();
	const int32 Level = GetLevelNumber();
	if (HostSessionMode() == EBRSessionMode::Story)
	{
		if (Level == 0 && HasServiceDoor())
		{
			// Level 0 de la partie : sa graine (tiree a la nouvelle partie) et la porte retenue, sauvegardees avec elle.
			// Une graine imposee pour un essai (autre que celle de la partie) ne remplace rien
			if (S.IntroSeed == 0)
			{
				S.IntroSeed = Seed;
				UE_LOG(LogBackrooms, Log, TEXT("[Story] Level 0 de la partie : graine %u"), Seed);
			}
			if (S.IntroSeed == Seed)
			{
				const FIntPoint Dir = GetServiceDoorDir();
				S.DoorX = ServiceDoorCell.X;
				S.DoorY = ServiceDoorCell.Y;
				S.DoorDir = Dir.X > 0 ? 0 : (Dir.X < 0 ? 1 : (Dir.Y > 0 ? 2 : 3));
				S.DoorTier = ServiceDoorTier;
				S.DoorVersion = BRStory::DoorPlaceVersion;
			}
		}
		BRStory::OnArrive(S, Level);
		if (Level == 0 && !BRStory::IsDone(S, 0) && (S.Events & BRStory::Event::IntroWake) == 0)
		{
			S.Events |= BRStory::Event::IntroWake; // reveil : une fois par partie, au premier Level 0
		}
	}
	if (Level == 0 && HasServiceDoor() && HostSessionMode() == EBRSessionMode::Story && !IsServiceDoorOpen())
	{
		// Aide d'orientation apres une longue errance (chaque visite du Level 0, tant que la porte n'est pas ouverte)
		GetWorldTimerManager().SetTimer(DraftHintTimer, this, &ABRWorld::OnDraftHint, DraftHintDelay, false);
	}
	PublishStory(S);
}

// =====================================================================================================================
// Effets locaux (chaque machine)
// =====================================================================================================================

void ABRWorld::ApplyStoryLocal(bool bInitial)
{
	if (ABRExit* Door = ServiceDoorActor.Get())
	{
		Door->SetServiceDoorOpen(IsServiceDoorOpen(), !bInitial && NetStory.DoorSerial != SeenDoorSerial);
	}
	SeenDoorSerial = NetStory.DoorSerial;
	const uint32 New = NetStory.Events & ~SeenStoryEvents;
	SeenStoryEvents = NetStory.Events;
	const bool bHintNow = NetStory.HintSerial != 0 && NetStory.HintSerial == LoadedSerial;
	if (bInitial || !bLevelReady)
	{
		return;
	}
	if ((New & BRStory::Event::IntroWake) != 0 && GetLevelNumber() == 0)
	{
		PlayIntroWake();
	}
	if (bHintNow && HintShownSerial != LoadedSerial)
	{
		HintShownSerial = LoadedSerial; // l'aide de ce niveau est montree une fois
		ShowDraftHint();
	}
}

void ABRWorld::PlayIntroWake()
{
	if (IntroPlayedSerial == LoadedSerial)
	{
		return;
	}
	IntroPlayedSerial = LoadedSerial;
	if (TransState != ETrans::None)
	{
		bIntroPending = true; // le reveil commence quand le fondu d'arrivee se termine
		return;
	}
	StartPendingIntro();
}

void ABRWorld::StartPendingIntro()
{
	bIntroPending = false;
	ABRCharacter* P = GetPlayer();
	if (!P || P->IsDead() || GetLevelNumber() != 0)
	{
		return;
	}
	IntroStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	P->SetInputLocked(true);
	if (APlayerController* PC = Cast<APlayerController>(P->GetController()))
	{
		// On se reveille au sol, le regard sur la moquette ; on se releve en regardant autour de soi
		const FRotator R = PC->GetControlRotation();
		PC->SetControlRotation(FRotator(-42.f, R.Yaw, 0.f));
	}
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Gasp")))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.55f);
		}
	}
	ABRHUD::Caption(this, BR_STR(NSLOCTEXT("BR", "Story.IntroWake", "[Le bourdonnement des néons. La moquette est humide sous vos mains.]")), FVector::ZeroVector, 4.5f);
	GetWorldTimerManager().SetTimer(IntroUnlockTimer, this, &ABRWorld::EndIntroWake, IntroSeconds, false);
}

void ABRWorld::EndIntroWake()
{
	ABRCharacter* P = GetPlayer();
	if (P && !IsTransitioning())
	{
		P->SetInputLocked(false);
	}
	ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Story.IntroAir", "Quelque part, un courant d'air froid passe. L'air ne devrait pas bouger ici.")), 6.f,
		FLinearColor(0.8f, 0.9f, 1.f));
}

float ABRWorld::GetIntroElapsed() const
{
	if (IntroStartTime < 0.0 || !GetWorld())
	{
		return -1.f;
	}
	return static_cast<float>(GetWorld()->GetTimeSeconds() - IntroStartTime);
}

void ABRWorld::OnDraftHint()
{
	if (!HasAuthority() || GetLevelNumber() != 0 || IsServiceDoorOpen() || !IsStorySession() || !bServiceDoor)
	{
		return;
	}
	NetStory.HintSerial = LoadedSerial;
	BRStory::FState S = GetStory();
	S.Events |= BRStory::Event::DraftHint;
	UE_LOG(LogBackrooms, Log, TEXT("[Story] Aide d'orientation du Level 0 (porte en %d,%d)"), ServiceDoorCell.X, ServiceDoorCell.Y);
	PublishStory(S);
}

void ABRWorld::ShowDraftHint()
{
	if (ABRExit* Door = ServiceDoorActor.Get())
	{
		Door->BoostDraft();
	}
	const ABRCharacter* P = GetPlayer();
	if (!P || !bServiceDoor)
	{
		return;
	}
	// Direction de l'air froid par rapport au regard du joueur (pas de boussole dans les Backrooms)
	const FVector To = GetServiceDoorLocation() - P->GetActorLocation();
	const float Yaw = P->GetController() ? static_cast<float>(P->GetController()->GetControlRotation().Yaw) : static_cast<float>(P->GetActorRotation().Yaw);
	const float Rel = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(To.Y), static_cast<float>(To.X))) - Yaw);
	FText Dir;
	if (FMath::Abs(Rel) <= 45.f)
	{
		Dir = NSLOCTEXT("BR", "Story.DraftAhead", "devant vous");
	}
	else if (FMath::Abs(Rel) >= 135.f)
	{
		Dir = NSLOCTEXT("BR", "Story.DraftBehind", "derrière vous");
	}
	else if (Rel > 0.f)
	{
		Dir = NSLOCTEXT("BR", "Story.DraftRight", "sur votre droite");
	}
	else
	{
		Dir = NSLOCTEXT("BR", "Story.DraftLeft", "sur votre gauche");
	}
	ABRHUD::Caption(this, BRLoc::Fmt(NSLOCTEXT("BR", "Story.DraftHint", "[Un courant d'air froid, {Dir}. Il vient d'une porte.]"), { { TEXT("Dir"), BRLoc::Arg(Dir) } }),
		GetServiceDoorLocation(), 7.f);
	ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Story.DraftHintNote", "Suivez l'air froid et les marques de craie de L. (un cercle barré et un L).")), 7.f, FLinearColor(0.8f, 0.9f, 1.f));
}

void ABRWorld::ShowPoolInscription()
{
	if (GetLevelNumber() != BRStory::Poolrooms || IsTransitioning())
	{
		return;
	}
	ABRHUD::Caption(this, FString(TEXT("[")) + BRMissionText::PoolInscription() + TEXT("]"), FVector::ZeroVector, 10.f);
	MarkStoryEvent(BRStory::Event::PoolInscription);
}

float ABRWorld::GetDraftHintRemaining() const
{
	return GetWorldTimerManager().IsTimerActive(DraftHintTimer) ? GetWorldTimerManager().GetTimerRemaining(DraftHintTimer) : -1.f;
}

// =====================================================================================================================
// Porte de service du Level 0
// =====================================================================================================================

void ABRWorld::PlaceServiceDoor()
{
	bServiceDoor = false;
	ServiceDoorTier = 0;
	bServiceDoorRestored = false;
	ServiceDoorCands.Reset();
	ServicePath.Reset();
	ServicePathIndex.Reset();
	const FBRLevelDef& D = Def();
	bool bWanted = false;
	for (const FBRExitDef& X : D.Exits)
	{
		bWanted |= X.Style == EBRExitStyle::ServiceDoor;
	}
	if (!bWanted || D.BoundsChunks <= 0)
	{
		return;
	}
	// 1. Distances de marche depuis le depart, sans jamais entrer dans une salle de fosses (les portes forcees de
	//    PrepareLevelLayout relient deja toute cellule hors fosse au depart)
	const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	TMap<FIntPoint, int32> Dist;
	TMap<FIntPoint, FIntPoint> Parent;
	TArray<FIntPoint> Queue;
	Dist.Add(FIntPoint(0, 0), 0);
	Queue.Add(FIntPoint(0, 0));
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint Cur = Queue[Head];
		const int32 CurD = Dist.FindChecked(Cur);
		for (const FIntPoint& Dir : Dirs)
		{
			const FIntPoint N = Cur + Dir;
			if (Dist.Contains(N) || !IsCellInBounds(N.X, N.Y) || IsPitRoomCell(N.X, N.Y) || !CanStep(Cur, N))
			{
				continue;
			}
			Dist.Add(N, CurD + 1);
			Parent.Add(N, Cur);
			Queue.Add(N);
		}
	}
	// 2. Emplacements possibles : cellules contre le mur d'enceinte (pas un coin, pas le depart), atteintes sans fosse.
	//    Une porte decrit une cellule, une face (vers l'exterieur), l'orientation qui en decoule et la hauteur du module
	//    (ouverture de 1,30 x 2,25 m au pied du mur) ; le chemin qui y mene est reserve avant les meubles (BRChunk)
	constexpr int32 BandLo = 24;
	constexpr int32 BandHi = 40;
	constexpr int32 WideLo = 16;
	constexpr int32 WideHi = 56;
	constexpr int32 Aim = 32;
	ServiceDoorCands.Reset();
	bServiceDoorRestored = false;
	int32 MaxD = 0;
	for (const FIntPoint& C : Queue)
	{
		if (IsSpawnArea(C.X, C.Y) || IsMissionCell(C.X, C.Y))
		{
			continue;
		}
		int32 OutCount = 0;
		int32 OutIdx = 0;
		for (int32 K = 0; K < 4; ++K)
		{
			if (!IsCellInBounds(C.X + Dirs[K].X, C.Y + Dirs[K].Y))
			{
				++OutCount;
				OutIdx = K;
			}
		}
		if (OutCount != 1)
		{
			continue; // interieur, ou coin de l'enceinte
		}
		FBRDoorCandidate Cand;
		Cand.Cell = C;
		Cand.Dir = Dirs[OutIdx];
		Cand.Dist = Dist.FindChecked(C);
		Cand.Hash = BRHash::Hash(C.X * 4 + OutIdx, C.Y, 2101, Seed);
		Cand.Band = (Cand.Dist >= BandLo && Cand.Dist <= BandHi) ? 0 : ((Cand.Dist >= WideLo && Cand.Dist <= WideHi) ? 1 : 2);
		MaxD = FMath::Max(MaxD, Cand.Dist);
		ServiceDoorCands.Add(Cand);
	}
	if (ServiceDoorCands.Num() == 0)
	{
		UE_LOG(LogBackrooms, Error, TEXT("[Story] Level 0 (graine %u) : aucune cellule de bord atteignable pour la porte de service"), Seed);
		return;
	}
	auto DirIndex = [&](const FIntPoint& Dir)
	{
		for (int32 K = 0; K < 4; ++K)
		{
			if (Dirs[K] == Dir)
			{
				return K;
			}
		}
		return -1;
	};
	// 3. La porte deja retenue pour ce Level 0 dans la partie (sauvegarde, ou etat de l'hote chez un client) reste a sa
	//    place tant qu'elle est encore un emplacement valide : ni une mort, ni une reprise, ni un rechargement ne
	//    tirent une autre porte
	const FBRDoorCandidate* Best = nullptr;
	if (NetStory.IntroSeed == Seed && NetStory.DoorDir >= 0 && NetStory.DoorDir < 4)
	{
		for (const FBRDoorCandidate& Cand : ServiceDoorCands)
		{
			if (Cand.Cell == FIntPoint(NetStory.DoorX, NetStory.DoorY) && DirIndex(Cand.Dir) == NetStory.DoorDir)
			{
				Best = &Cand;
				ServiceDoorTier = NetStory.DoorTier;
				bServiceDoorRestored = true;
				break;
			}
		}
		if (!Best)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("[Story] Porte sauvegardee (%d,%d face %d) plus valide pour la graine %u : nouveau choix"), NetStory.DoorX, NetStory.DoorY,
				NetStory.DoorDir, Seed);
		}
	}
	// 4. Sinon : la bande de distance de marche visee (24 a 40 cellules, 84 a 140 m par le plus court chemin praticable) ;
	//    parmi ses emplacements, celui que designe la graine (plus petit hachage). Bande vide : bande elargie (16 a 56) ;
	//    vide aussi : secours deterministe, l'emplacement atteignable le plus proche de 32 cellules. Jamais de partie
	//    sans sortie
	for (uint8 Band = 0; !Best && Band < 2; ++Band)
	{
		for (const FBRDoorCandidate& Cand : ServiceDoorCands)
		{
			if (Cand.Band <= Band && (!Best || Cand.Hash < Best->Hash))
			{
				Best = &Cand;
			}
		}
		ServiceDoorTier = Band;
	}
	if (!Best)
	{
		ServiceDoorTier = 2;
		for (const FBRDoorCandidate& Cand : ServiceDoorCands)
		{
			const int32 Gap = FMath::Abs(Cand.Dist - Aim);
			const int32 BestGap = Best ? FMath::Abs(Best->Dist - Aim) : MAX_int32;
			if (!Best || Gap < BestGap || (Gap == BestGap && Cand.Hash < Best->Hash))
			{
				Best = &Cand;
			}
		}
	}
	bServiceDoor = true;
	ServiceDoorCell = Best->Cell;
	ServiceDoorDir = Best->Dir;
	const int32 BestDist = Best->Dist;
	// 4. Chemin du depart a la porte (marques de craie)
	FIntPoint Cur = ServiceDoorCell;
	ServicePath.Add(Cur);
	while (Cur != FIntPoint(0, 0))
	{
		Cur = Parent.FindChecked(Cur);
		ServicePath.Add(Cur);
	}
	Algo::Reverse(ServicePath);
	for (int32 I = 0; I < ServicePath.Num(); ++I)
	{
		ServicePathIndex.Add(ServicePath[I], I);
	}
	int32 InBand = 0;
	for (const FBRDoorCandidate& Cand : ServiceDoorCands)
	{
		InBand += Cand.Band == 0 ? 1 : 0;
	}
	UE_LOG(LogBackrooms, Log, TEXT("[Story] Porte de service du Level 0 (graine %u) : cellule %d,%d vers %d,%d, %d cellules de marche (plus long %d), palier %d, %d emplacement(s) dans la bande sur %d%s"),
		Seed, ServiceDoorCell.X, ServiceDoorCell.Y, ServiceDoorDir.X, ServiceDoorDir.Y, BestDist, MaxD, ServiceDoorTier, InBand, ServiceDoorCands.Num(),
		bServiceDoorRestored ? TEXT(", porte de la partie (sauvegardee)") : TEXT(""));
}

bool ABRWorld::IsServiceDoorEdge(int32 X, int32 Y, bool bEast) const
{
	if (!bServiceDoor)
	{
		return false;
	}
	if (ServiceDoorDir.X != 0)
	{
		return bEast && Y == ServiceDoorCell.Y && X == (ServiceDoorDir.X > 0 ? ServiceDoorCell.X : ServiceDoorCell.X - 1);
	}
	return !bEast && X == ServiceDoorCell.X && Y == (ServiceDoorDir.Y > 0 ? ServiceDoorCell.Y : ServiceDoorCell.Y - 1);
}

bool ABRWorld::IsChalkMarkCell(int32 X, int32 Y, FIntPoint& OutNext) const
{
	const int32* Idx = ServicePathIndex.Find(FIntPoint(X, Y));
	if (!Idx)
	{
		return false;
	}
	const int32 I = *Idx;
	// Une marque toutes les trois cellules (environ 10 m), ni au depart ni juste devant la porte
	if (I < 3 || I + 2 >= ServicePath.Num() || I % 3 != 0 || IsSpawnArea(X, Y))
	{
		return false;
	}
	OutNext = ServicePath[I + 1];
	return true;
}

FVector ABRWorld::GetServiceDoorLocation() const
{
	const FBRLevelDef& D = Def();
	return CellCenter(ServiceDoorCell, 0.f) + FVector(ServiceDoorDir.X, ServiceDoorDir.Y, 0.f) * (D.CellSize * 0.5f - D.WallThickness * 0.5f);
}

float ABRWorld::GetServiceDoorYaw() const
{
	return FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-ServiceDoorDir.Y), static_cast<float>(-ServiceDoorDir.X)));
}

void ABRWorld::ServerOpenServiceDoor(const ABRCharacter* By)
{
	if (!HasAuthority() || !bServiceDoor || IsServiceDoorOpen())
	{
		return;
	}
	NetStory.DoorSerial = LoadedSerial;
	GetWorldTimerManager().ClearTimer(DraftHintTimer);
	BRStory::FState S = GetStory();
	if (IsStorySession())
	{
		S.Events |= BRStory::Event::DoorOpened;
	}
	UE_LOG(LogBackrooms, Log, TEXT("[Story] Porte de service ouverte par %s"), By ? *By->GetName() : TEXT("?"));
	PublishStory(S);
}

void ABRWorld::RegisterServiceDoor(ABRExit* Door)
{
	ServiceDoorActor = Door;
	if (Door)
	{
		Door->SetServiceDoorOpen(IsServiceDoorOpen(), false);
		if (IsDraftHintGiven())
		{
			Door->BoostDraft();
		}
	}
}
