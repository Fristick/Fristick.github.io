#include "BRWorld.h"
#include "Backrooms.h"
#include "BRLevels.h"
#include "BRAssets.h"
#include "BRChunk.h"
#include "BREntity.h"
#include "BRCharacter.h"
#include "BRHUD.h"
#include "BRPlayerController.h"
#include "BRKeys.h"
#include "BRInteractables.h"
#include "BRPhenomena.h"
#include "BRWaterSim.h"

#include "Algo/Reverse.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

namespace
{
	FORCEINLINE int32 Floor32(double V)
	{
		return static_cast<int32>(FMath::FloorToDouble(V));
	}

	TWeakObjectPtr<ABRWorld> GWorldInstance;
}

ABRWorld::ABRWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	// Multijoueur : le serveur choisit le niveau et sa graine, chaque joueur construit les memes salles chez lui
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;

	AmbientAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientAudio"));
	AmbientAudio->SetupAttachment(Root);
	AmbientAudio->bAutoActivate = false;

	HumAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("HumAudio"));
	HumAudio->SetupAttachment(Root);
	HumAudio->bAutoActivate = false;
}

ABRWorld* ABRWorld::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (GWorldInstance.IsValid() && GWorldInstance->GetWorld() == World)
	{
		return GWorldInstance.Get();
	}
	if (World)
	{
		for (TActorIterator<ABRWorld> It(World); It; ++It)
		{
			GWorldInstance = *It;
			return *It;
		}
	}
	return nullptr;
}

void ABRWorld::BeginPlay()
{
	Super::BeginPlay();
	GWorldInstance = this;

	if (!HasAuthority())
	{
		// Client : le niveau et sa graine viennent du serveur
		Fade = 1.f;
		SyncNetLevel();
		return;
	}

	int32 CmdLevel = StartLevel;
	if (FParse::Value(FCommandLine::Get(), TEXT("BRLevel="), CmdLevel) && BRLevels::Exists(CmdLevel))
	{
		StartLevel = CmdLevel;
	}

	LoadLevelNow(StartLevel);
	TransState = ETrans::FadingIn;
	TransTimer = 0.f;
	Fade = 1.f;
}

const FBRLevelDef& ABRWorld::Def() const
{
	return Current ? *Current : BRLevels::Get(0);
}

int32 ABRWorld::GetLevelNumber() const
{
	return Def().Number;
}

ABRCharacter* ABRWorld::GetPlayer() const
{
	return Cast<ABRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void ABRWorld::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRWorld, NetLevel);
	DOREPLIFETIME(ABRWorld, NetBlackout);
	DOREPLIFETIME(ABRWorld, NetCollected);
	DOREPLIFETIME(ABRWorld, VHSFound);
	DOREPLIFETIME(ABRWorld, bBlackoutRecorded);
	DOREPLIFETIME(ABRWorld, bEntityRecorded);
}

bool ABRWorld::IsNetGame() const
{
	return GetNetMode() != NM_Standalone;
}

ABRPlayerController* ABRWorld::LocalPC() const
{
	return Cast<ABRPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
}

void ABRWorld::GetPlayers(TArray<ABRCharacter*>& Out) const
{
	Out.Reset();
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<ABRCharacter> It(W); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed())
			{
				Out.Add(*It);
			}
		}
	}
}

ABRCharacter* ABRWorld::RandomLivingPlayer() const
{
	TArray<ABRCharacter*> All;
	GetPlayers(All);
	All.RemoveAll([](const ABRCharacter* C) { return C->IsDead(); });
	return All.Num() > 0 ? All[FMath::RandRange(0, All.Num() - 1)] : nullptr;
}

bool ABRWorld::IsHiddenFromPlayers(const FVector& Loc) const
{
	TArray<ABRCharacter*> All;
	GetPlayers(All);
	for (const ABRCharacter* C : All)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRSpawnHidden), false, C);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, C->GetEyeLocation(), Loc, ECC_Visibility, Q))
		{
			return false;
		}
	}
	return true;
}

void ABRWorld::RegisterEntity(ABREntity* Entity)
{
	if (IsValid(Entity))
	{
		Entities.AddUnique(Entity);
	}
}

// =====================================================================================
// Niveaux & transitions
// =====================================================================================

void ABRWorld::RequestTransition(int32 TargetLevel, bool bFromDeath)
{
	if (!HasAuthority())
	{
		// Client : c'est le serveur qui emmene tout le groupe
		if (ABRPlayerController* PC = LocalPC())
		{
			PC->ServerRequestTransition(TargetLevel);
		}
		return;
	}
	if (TransState == ETrans::FadingOut)
	{
		return;
	}
	if (TargetLevel < 0)
	{
		const TArray<FBRLevelDef>& All = BRLevels::All();
		TArray<int32> Choices;
		for (const FBRLevelDef& L : All)
		{
			if (L.Number != GetLevelNumber())
			{
				Choices.Add(L.Number);
			}
		}
		TargetLevel = Choices.Num() > 0 ? Choices[FMath::RandRange(0, Choices.Num() - 1)] : 0;
	}
	if (!BRLevels::Exists(TargetLevel))
	{
		TargetLevel = 0;
	}
	MulticastTransition(TargetLevel, bFromDeath);
}

void ABRWorld::MulticastTransition_Implementation(int32 TargetLevel, bool bFromDeath)
{
	BeginTransition(TargetLevel, bFromDeath);
}

void ABRWorld::BeginTransition(int32 TargetLevel, bool bFromDeath)
{
	if (TransState == ETrans::FadingOut)
	{
		return;
	}
	PendingLevel = TargetLevel;
	bPendingDeath = bFromDeath;
	TransState = ETrans::FadingOut;
	TransTimer = 0.f;
	DeathTimer = -1.f;

	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(bFromDeath ? FName(TEXT("S_Whisper")) : FName(TEXT("S_Noclip"))))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.9f);
		}
	}
	if (ABRCharacter* P = GetPlayer())
	{
		P->SetInputLocked(true);
	}
}

void ABRWorld::HandlePlayerDeath()
{
	// Seul : retour au Niveau 0. En equipe : a terre, un coequipier a 30 s pour nous relever,
	// sinon on se reveille au point de depart du niveau en cours
	DeathTimer = IsNetGame() ? (HasLivingTeammate() ? 30.f : 6.f) : 4.5f;
}

bool ABRWorld::HasLivingTeammate() const
{
	const ABRCharacter* Me = GetPlayer();
	TArray<ABRCharacter*> All;
	GetPlayers(All);
	for (const ABRCharacter* C : All)
	{
		if (C != Me && !C->IsDead())
		{
			return true;
		}
	}
	return false;
}

void ABRWorld::RespawnLocalPlayer()
{
	ABRCharacter* P = GetPlayer();
	if (!P || !bLevelReady)
	{
		return;
	}
	P->ResetStats();
	PlacePlayer();
	P->SetInputLocked(true);
	TransState = ETrans::FadingIn;
	TransTimer = 0.f;
	Fade = 1.f;
	ABRHUD::Notify(this, TEXT("Vous vous r\u00e9veillez au point de d\u00e9part. Votre \u00e9quipement est rest\u00e9 l\u00e0 o\u00f9 vous \u00eates tomb\u00e9."), 6.f,
		FLinearColor(1.f, 0.85f, 0.6f));
}

void ABRWorld::ClearLevel()
{
	for (TPair<FIntPoint, TObjectPtr<ABRChunk>>& Pair : Chunks)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	Chunks.Empty();

	TArray<TObjectPtr<ABREntity>> Copy = Entities;
	Entities.Empty();
	BlackoutEntities.Reset();
	RedSources.Reset();
	PatrolSpawnTimer = 0.f;
	if (!HasAuthority())
	{
		// Les entites repliquees sont detruites par le serveur : on garde celles qui existent encore
		for (ABREntity* E : Copy)
		{
			if (IsValid(E) && !E->IsActorBeingDestroyed())
			{
				Entities.Add(E);
			}
		}
		return;
	}
	for (ABREntity* E : Copy)
	{
		if (IsValid(E))
		{
			E->Destroy();
		}
	}
}

void ABRWorld::LoadLevelNow(int32 LevelNumber, uint32 InSeed)
{
	// Premier niveau d'un client qui rejoint : le serveur a deja fait apparaitre son personnage a sa place
	const bool bFirstClientLoad = !HasAuthority() && !bLevelReady;
	ClearLevel();
	Current = &BRLevels::Get(LevelNumber);
	Seed = InSeed != 0 ? InSeed : (static_cast<uint32>(FMath::Rand()) * 2654435761u ^ static_cast<uint32>(LevelNumber * 7919 + 17));
	Seed = Seed != 0 ? Seed : 1u;
	bLevelReady = true;
	Collected.Empty();
	LevelTime = 0.f;
	TitleTime = 7.f;
	SpawnTimer = 25.f;
	PhenomenaTimer = FMath::FRandRange(30.f, 60.f);
	Visited.AddUnique(Current->Number);

	// Coupures & objectifs (v2)
	BlackoutPhase = EBlackout::None;
	BlackoutTimer = Current->BlackoutFirst * FMath::FRandRange(0.85f, 1.15f);
	Power = 1.f;
	AppliedPower = -1.f;
	if (HasAuthority())
	{
		// Le serveur publie le nouveau niveau : les clients le construisent avec la meme graine
		NetLevel.Level = Current->Number;
		NetLevel.Seed = Seed;
		++NetLevel.Serial;
		LoadedSerial = NetLevel.Serial;
		NetBlackout = 0;
		NetCollected.Reset();
		VHSFound = 0;
		bBlackoutRecorded = false;
		bEntityRecorded = false;
	}
	PrevVHSFound = VHSFound;
	bPrevBlackoutRecorded = bBlackoutRecorded;
	bPrevEntityRecorded = bEntityRecorded;
	bObjectiveSent[0] = bObjectiveSent[1] = false;
	bObjectivesAnnounced = false;
	BlackoutRecordTime = 0.f;
	EntityRecordTime = 0.f;
	RecordProgress = 0.f;
	RecordLabel.Empty();
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		A->SetGlowScale(1.f);
	}
	// Modeles importes a la mauvaise echelle (invisibles) : on le dit clairement, une seule fois
	if (!bMeshesChecked)
	{
		bMeshesChecked = true;
		if (UBRAssets* A = UBRAssets::Get(this))
		{
			const FString Problem = A->CheckImportedMeshes();
			if (!Problem.IsEmpty())
			{
				UE_LOG(LogBackrooms, Error, TEXT("%s"), *Problem);
				ABRHUD::Notify(this, Problem, 30.f, FLinearColor(1.f, 0.35f, 0.3f));
			}
		}
	}

	// Eau calme dans le nouveau niveau ; ses murs seront relus par la simulation
	if (WaterSim)
	{
		WaterSim->Reset();
	}
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		A->SetWaterSim(nullptr, FLinearColor(0.f, 0.f, 1000.f, 0.f));
	}

	UE_LOG(LogBackrooms, Log, TEXT("Chargement du Niveau %d - %s (graine %u)"), Current->Number, *Current->Title, Seed);

	ApplyEnvironment();
	UpdateStreaming(true);
	PlacePlayer(bFirstClientLoad);
	// Client arrive pendant une coupure : on reprend l'etat du serveur
	if (!HasAuthority() && NetBlackout != 0)
	{
		EnterBlackoutPhase(NetBlackout, true);
	}
}

// =====================================================================================
// Multijoueur : suivre le serveur
// =====================================================================================

bool ABRWorld::SyncNetLevel()
{
	if (NetLevel.Serial == 0 || NetLevel.Serial == LoadedSerial)
	{
		return bLevelReady;
	}
	if (!bLevelReady)
	{
		// Arrivee dans la partie : on construit tout de suite le niveau du groupe
		LoadNetLevel();
		TransState = ETrans::FadingIn;
		TransTimer = 0.f;
		Fade = 1.f;
		return true;
	}
	if (TransState != ETrans::FadingOut)
	{
		BeginTransition(NetLevel.Level, false); // l'annonce a ete manquee : on suit quand meme
	}
	return true;
}

void ABRWorld::LoadNetLevel()
{
	LoadedSerial = NetLevel.Serial;
	LoadLevelNow(NetLevel.Level, NetLevel.Seed);
}

void ABRWorld::OnRep_NetLevel()
{
	if (HasActorBegunPlay())
	{
		SyncNetLevel();
	}
}

void ABRWorld::OnRep_Blackout()
{
	if (bLevelReady && NetBlackout != static_cast<uint8>(BlackoutPhase))
	{
		EnterBlackoutPhase(NetBlackout);
	}
}

void ABRWorld::OnRep_Collected()
{
	for (const uint64 Id : NetCollected)
	{
		if (!Collected.Contains(Id))
		{
			Collected.Add(Id);
			DestroyPickup(Id);
		}
	}
}

void ABRWorld::OnRep_Objectives()
{
	if (bLevelReady)
	{
		if (VHSFound > PrevVHSFound)
		{
			AnnounceVHS();
		}
		if (bBlackoutRecorded && !bPrevBlackoutRecorded)
		{
			CompleteTask(TEXT("FILMER PENDANT UNE COUPURE"));
		}
		if (bEntityRecorded && !bPrevEntityRecorded)
		{
			CompleteTask(TEXT("FILMER UNE ENTIT\u00c9"));
		}
	}
	PrevVHSFound = VHSFound;
	bPrevBlackoutRecorded = bBlackoutRecorded;
	bPrevEntityRecorded = bEntityRecorded;
}

void ABRWorld::MarkCollected(uint64 Id)
{
	Collected.Add(Id);
	if (HasAuthority())
	{
		NetCollected.AddUnique(Id);
	}
	else if (ABRPlayerController* PC = LocalPC())
	{
		PC->ServerMarkCollected(Id);
	}
}

void ABRWorld::ServerCollected(uint64 Id)
{
	if (!HasAuthority())
	{
		return;
	}
	Collected.Add(Id);
	NetCollected.AddUnique(Id);
	DestroyPickup(Id);
}

void ABRWorld::DestroyPickup(uint64 Id)
{
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<ABRPickup> It(W); It; ++It)
		{
			if (It->Id == Id && !It->IsActorBeingDestroyed())
			{
				It->Destroy();
			}
		}
	}
}

void ABRWorld::CompleteObjective(uint8 Which)
{
	if (HasAuthority())
	{
		ServerCompleteObjective(Which);
		return;
	}
	bool& bSent = bObjectiveSent[Which == 0 ? 0 : 1];
	if (!bSent)
	{
		bSent = true;
		if (ABRPlayerController* PC = LocalPC())
		{
			PC->ServerCompleteObjective(Which);
		}
	}
}

void ABRWorld::ServerCompleteObjective(uint8 Which)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Which == 0 && !bBlackoutRecorded)
	{
		bBlackoutRecorded = bPrevBlackoutRecorded = true;
		CompleteTask(TEXT("FILMER PENDANT UNE COUPURE"));
	}
	else if (Which == 1 && !bEntityRecorded)
	{
		bEntityRecorded = bPrevEntityRecorded = true;
		CompleteTask(TEXT("FILMER UNE ENTIT\u00c9"));
	}
}

void ABRWorld::DebugCompleteObjectives()
{
	if (!HasAuthority())
	{
		return;
	}
	const int32 Missing = FMath::Max(0, Def().VHSRequired - VHSFound);
	for (int32 i = 0; i < Missing; ++i)
	{
		OnVHSCollected();
	}
	if (Def().bBlackouts)
	{
		ServerCompleteObjective(0);
	}
	ServerCompleteObjective(1);
}

void ABRWorld::ApplyEnvironment()
{
	const FBRLevelDef& D = Def();
	UBRAssets* A = UBRAssets::Get(this);

	// --- Brouillard ---
	Fog->SetFogDensity(D.FogDensity);
	Fog->SetFogHeightFalloff(D.FogFalloff);
	Fog->SetFogInscatteringColor(D.FogColor);
	Fog->SetStartDistance(D.FogStart);
	UnderwaterBlend = 0.f;
	Fog->SetFogMaxOpacity(1.f);
	Fog->SetVolumetricFog(D.bVolumetricFog);

	// --- Ciel ---
	const bool bSky = D.bOutdoor && D.Sky != EBRSky::None;
	SkyAtmosphere->SetVisibility(bSky);
	Sun->SetVisibility(bSky);
	SkyLight->SetVisibility(bSky);
	if (bSky)
	{
		Sun->SetIntensity(D.SunLux);
		Sun->SetLightColor(D.SunColor);
		Sun->SetAtmosphereSunLight(true);
		Sun->SetCastShadows(true);
		Sun->SetWorldRotation(FRotator(D.SunPitch, 35.f, 0.f));
		SkyLight->SetIntensity(D.Sky == EBRSky::Night ? 0.6f : 1.f);
		SkyLight->RecaptureSky();
	}

	// --- Post-process du niveau ---
	FPostProcessSettings& S = PostProcess->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	S.bOverride_AutoExposureMinBrightness = true;
	S.AutoExposureMinBrightness = D.MinEV;
	S.bOverride_AutoExposureMaxBrightness = true;
	S.AutoExposureMaxBrightness = D.MaxEV;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = D.ExposureBias;
	S.bOverride_AutoExposureSpeedUp = true;
	S.AutoExposureSpeedUp = 2.f;
	S.bOverride_AutoExposureSpeedDown = true;
	S.AutoExposureSpeedDown = 1.f;
	S.bOverride_SceneColorTint = true;
	S.SceneColorTint = D.SceneTint;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(D.Saturation, D.Saturation, D.Saturation, 1.f);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(D.Contrast, D.Contrast, D.Contrast, 1.f);
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = D.Vignette;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = D.Grain;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = D.Bloom;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;

	// --- Audio ---
	AmbientAudio->Stop();
	HumAudio->Stop();
	if (A)
	{
		AmbientAudio->SetSound(A->Sound(D.AmbientSound));
		AmbientAudio->SetVolumeMultiplier(D.AmbientVolume);
		if (AmbientAudio->Sound)
		{
			AmbientAudio->Play();
		}
		HumAudio->SetSound(D.HumVolume > 0.f ? A->Sound(D.HumSound) : nullptr);
		HumAudio->SetVolumeMultiplier(0.01f);
		if (HumAudio->Sound)
		{
			HumAudio->Play();
		}
	}
}

int32 ABRWorld::PlayerSlot(const APlayerState* PS) const
{
	// L'ordre de PlayerArray differe d'une machine a l'autre (chez un client, son propre etat arrive souvent en premier) :
	// on classe par identifiant, attribue par le serveur dans l'ordre d'arrivee
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!PS || !GS)
	{
		return 0;
	}
	int32 Slot = 0;
	for (const APlayerState* Other : GS->PlayerArray)
	{
		Slot += (Other && Other != PS && Other->GetPlayerId() < PS->GetPlayerId()) ? 1 : 0;
	}
	return Slot;
}

FVector ABRWorld::SpawnSpot(int32 Slot, float Half) const
{
	FVector Loc = CellCenter(FIntPoint(0, 0), Half + 5.f);
	if (Slot > 0)
	{
		const float Ang = FMath::DegreesToRadians(90.f + 60.f * static_cast<float>(Slot - 1));
		Loc += FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * FMath::Min(85.f, CellSize() * 0.3f);
	}
	return Loc;
}

void ABRWorld::PlacePlayer(bool bKeepServerSpot)
{
	ABRCharacter* P = GetPlayer();
	if (!P)
	{
		bPlayerPlaced = false;
		return;
	}

	// Choisit une direction de depart degagee
	float Yaw = 0.f;
	const FIntPoint Start(0, 0);
	const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };
	const float Yaws[4] = { 0.f, 90.f, 180.f, 270.f };
	for (int32 i = 0; i < 4; ++i)
	{
		if (CanStep(Start, Start + Dirs[i]) && CanStep(Start + Dirs[i], Start + Dirs[i] * 2))
		{
			Yaw = Yaws[i];
			break;
		}
	}

	const float Half = P->GetCapsuleComponent() ? P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	// (a l'arrivee d'un client, l'etat des autres joueurs n'est pas toujours encore recu : sa place calculee ici
	// serait fausse, alors que le serveur l'a deja fait apparaitre a la bonne)
	const bool bServerPlaced = bKeepServerSpot && FVector::Dist2D(P->GetActorLocation(), CellCenter(FIntPoint(0, 0))) < CellSize();
	if (!bServerPlaced)
	{
		const int32 Slot = IsNetGame() ? PlayerSlot(P->GetPlayerState()) : 0;
		const FVector Loc = SpawnSpot(Slot, Half);
		P->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogBackrooms, Log, TEXT("Joueur place au point de depart (place %d) : %s"), Slot, *Loc.ToString());
	}
	if (AController* C = P->GetController())
	{
		C->SetControlRotation(FRotator(0.f, Yaw, 0.f));
	}
	P->OnEnteredLevel(Def());
	bPlayerPlaced = true;
}

void ABRWorld::Discover(EBREntityKind Kind)
{
	const int32 K = static_cast<int32>(Kind);
	if (Discovered.Contains(K))
	{
		return;
	}
	Discovered.Add(K);
	const FBREntityInfo& Info = ABREntity::Info(Kind);
	ABRHUD::Notify(this, FString::Printf(TEXT("Nouvelle entr\u00e9e du journal : %s - %s  %s"), *Info.Number, *Info.Name, *BRKeys::Tag(EBRAction::Inventory)),
		6.f, FLinearColor(1.f, 0.4f, 0.35f));
}

// =====================================================================================
// Tick
// =====================================================================================

void ABRWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	if (!HasAuthority() && !SyncNetLevel())
	{
		Fade = 1.f; // en attente du niveau du serveur
		return;
	}
	LevelTime += Dt;
	TitleTime = FMath::Max(0.f, TitleTime - Dt);

	if (!bPlayerPlaced)
	{
		// Client qui arrive dans la partie : son personnage apparait a la place que le serveur lui a donnee
		PlacePlayer(!HasAuthority());
	}

	switch (TransState)
	{
	case ETrans::FadingOut:
		TransTimer += Dt;
		Fade = FMath::Clamp(TransTimer / 1.3f, 0.f, 1.f);
		Glitch = Fade;
		if (TransTimer >= 1.45f)
		{
			if (HasAuthority())
			{
				LoadLevelNow(PendingLevel);
			}
			else if (NetLevel.Serial != LoadedSerial)
			{
				LoadNetLevel();
			}
			else if (TransTimer < 12.f)
			{
				break; // le serveur n'a pas encore choisi le niveau : on reste dans le noir
			}
			ABRCharacter* P = GetPlayer();
			if (P && (bPendingDeath || (IsNetGame() && P->IsDead())))
			{
				P->ResetStats();
			}
			TransState = ETrans::FadingIn;
			TransTimer = 0.f;
		}
		break;
	case ETrans::FadingIn:
		TransTimer += Dt;
		Fade = 1.f - FMath::Clamp(TransTimer / 1.6f, 0.f, 1.f);
		Glitch = Fade * 0.6f;
		if (TransTimer >= 1.6f)
		{
			TransState = ETrans::None;
			Fade = 0.f;
			Glitch = 0.f;
			if (ABRCharacter* P = GetPlayer())
			{
				P->SetInputLocked(false);
			}
		}
		break;
	default:
		break;
	}

	if (DeathTimer > 6.f && IsNetGame() && !HasLivingTeammate())
	{
		DeathTimer = 6.f; // plus personne pour nous relever
	}
	if (DeathTimer > 0.f)
	{
		DeathTimer -= Dt;
		if (DeathTimer <= 0.f)
		{
			DeathTimer = -1.f;
			if (IsNetGame())
			{
				RespawnLocalPlayer();
			}
			else
			{
				RequestTransition(0, true);
			}
		}
	}

	StreamTimer -= Dt;
	if (StreamTimer <= 0.f)
	{
		StreamTimer = 0.1f;
		UpdateStreaming(false);
	}
	UpdateWaterSim(Dt);

	// Le menu titre s'affiche par-dessus le niveau : pas de coupure ni d'annonce tant qu'on n'a pas commence
	bool bMenu = false;
	if (const ABRCharacter* P = GetPlayer())
	{
		if (const ABRPlayerController* PC = Cast<ABRPlayerController>(P->GetController()))
		{
			bMenu = PC->IsInMenu();
		}
	}
	if (bMenu)
	{
		LevelTime = 0.f;
	}

	if (TransState == ETrans::None)
	{
		if (HasAuthority())
		{
			UpdatePopulation(Dt);
		}
		UpdatePatrol(Dt);
		UpdatePhenomena(Dt);
		if (!bMenu)
		{
			UpdateBlackout(Dt);
		}
	}
	UpdateAudio(Dt);

	// Annonce des objectifs au debut d'un niveau qui en exige
	if (!bObjectivesAnnounced && !bMenu && TransState == ETrans::None && LevelTime > 8.f && Def().bRequireObjectives)
	{
		bObjectivesAnnounced = true;
		ABRHUD::Notify(this, FString::Printf(TEXT("OBJECTIFS : trouver %d cassettes VHS et filmer pendant une coupure de courant pour stabiliser la sortie.  %s"),
			Def().VHSRequired, *BRKeys::Tag(EBRAction::Inventory)), 8.f, FLinearColor(1.f, 0.85f, 0.4f));
	}
}

void ABRWorld::UpdateStreaming(bool bSynchronous)
{
	const FBRLevelDef& D = Def();
	const ABRCharacter* P = GetPlayer();
	const float ChunkWorld = D.ChunkCells * D.CellSize;
	const int32 R = FMath::CeilToInt(D.ViewDistance / ChunkWorld);

	// Le serveur garde le sol sous les pieds de chaque joueur (collisions, IA des entites)
	TArray<FVector> Centers;
	Centers.Add((P && bPlayerPlaced) ? P->GetActorLocation() : CellCenter(FIntPoint(0, 0)));
	if (HasAuthority() && IsNetGame())
	{
		TArray<ABRCharacter*> All;
		GetPlayers(All);
		for (const ABRCharacter* C : All)
		{
			if (C != P)
			{
				Centers.Add(C->GetActorLocation());
			}
		}
	}

	auto ChunkDist = [&](const FIntPoint& C)
	{
		float Best = 1e20f;
		for (const FVector& Center : Centers)
		{
			const FVector CC((C.X + 0.5f) * ChunkWorld, (C.Y + 0.5f) * ChunkWorld, Center.Z);
			Best = FMath::Min(Best, static_cast<float>(FVector::Dist2D(CC, Center)));
		}
		return Best;
	};

	TArray<FIntPoint> Wanted;
	for (const FVector& Center : Centers)
	{
		const FIntPoint PC = CellToChunk(WorldToCell(Center));
		for (int32 DX = -R; DX <= R; ++DX)
		{
			for (int32 DY = -R; DY <= R; ++DY)
			{
				const FIntPoint C(PC.X + DX, PC.Y + DY);
				if (!Chunks.Contains(C) && !Wanted.Contains(C) && ChunkDist(C) <= D.ViewDistance + ChunkWorld * 0.75f)
				{
					Wanted.Add(C);
				}
			}
		}
	}
	Wanted.Sort([&](const FIntPoint& A, const FIntPoint& B) { return ChunkDist(A) < ChunkDist(B); });

	int32 Budget = bSynchronous ? MAX_int32 : 2;
	for (const FIntPoint& C : Wanted)
	{
		if (Budget-- <= 0)
		{
			break;
		}
		SpawnChunk(C);
	}

	TArray<FIntPoint> ToRemove;
	for (const TPair<FIntPoint, TObjectPtr<ABRChunk>>& Pair : Chunks)
	{
		if (ChunkDist(Pair.Key) > D.ViewDistance + ChunkWorld * 1.6f)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FIntPoint& K : ToRemove)
	{
		if (ABRChunk* C = Chunks.FindRef(K))
		{
			C->Destroy();
		}
		Chunks.Remove(K);
	}
}

void ABRWorld::SpawnChunk(const FIntPoint& Coord)
{
	const FBRLevelDef& D = Def();
	const float ChunkWorld = D.ChunkCells * D.CellSize;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform T(FVector(Coord.X * ChunkWorld, Coord.Y * ChunkWorld, 0.f));
	ABRChunk* Chunk = GetWorld()->SpawnActor<ABRChunk>(ABRChunk::StaticClass(), T, Params);
	if (Chunk)
	{
		Chunk->Build(this, Coord);
		Chunks.Add(Coord, Chunk);
		if (Power < 0.999f)
		{
			Chunk->SetPower(Power);
		}
	}
}

// =====================================================================================
// Entites
// =====================================================================================

ABREntity* ABRWorld::SpawnEntity(EBREntityKind Kind, const FVector& Location)
{
	const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Location);
	ABREntity* E = GetWorld()->SpawnActorDeferred<ABREntity>(ABREntity::StaticClass(), T, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!E)
	{
		return nullptr;
	}
	E->Kind = Kind;
	E->FinishSpawning(T);
	Entities.Add(E);
	return E;
}

void ABRWorld::UnregisterEntity(ABREntity* Entity)
{
	Entities.Remove(Entity);
}

void ABRWorld::UpdatePopulation(float Dt)
{
	const FBRLevelDef& D = Def();
	TArray<ABRCharacter*> Players;
	GetPlayers(Players);
	ABRCharacter* P = RandomLivingPlayer();
	if (Players.Num() == 0)
	{
		return;
	}
	const float ChunkWorld = D.ChunkCells * D.CellSize;

	// Disparition des entites trop eloignees de tous les joueurs (ou dont le sol n'est plus charge)
	for (int32 i = Entities.Num() - 1; i >= 0; --i)
	{
		ABREntity* E = Entities[i];
		if (!IsValid(E))
		{
			Entities.RemoveAt(i);
			continue;
		}
		bool bFar = true;
		for (const ABRCharacter* C : Players)
		{
			bFar = bFar && FVector::Dist2D(E->GetActorLocation(), C->GetActorLocation()) > D.ViewDistance * 0.95f;
		}
		const bool bNoFloor = !IsChunkLoaded(CellToChunk(WorldToCell(E->GetActorLocation())));
		if (bFar || bNoFloor)
		{
			Entities.RemoveAt(i);
			E->Destroy();
		}
	}

	int32 Regular = 0;
	for (const ABREntity* E : Entities)
	{
		const bool bPatrol = D.bPatrolEntity && E && E->Kind == D.PatrolKind;
		const bool bBlackout = BlackoutEntities.ContainsByPredicate([E](const TWeakObjectPtr<ABREntity>& B) { return B.Get() == E; });
		Regular += (bPatrol || bBlackout) ? 0 : 1;
	}
	// Un peu plus de monde quand on est plusieurs
	const int32 MaxRegular = D.MaxEntities + (D.MaxEntities > 0 ? FMath::Min(Players.Num() - 1, 3) : 0);
	if (D.MaxEntities <= 0 || D.Entities.Num() == 0 || Regular >= MaxRegular || !P)
	{
		return;
	}
	if (const ABRPlayerController* PC = LocalPC())
	{
		if (PC->IsInMenu())
		{
			return;
		}
	}
	const FVector PL = P->GetActorLocation();
	SpawnTimer -= Dt;
	if (SpawnTimer > 0.f)
	{
		return;
	}
	SpawnTimer = D.SpawnInterval * FMath::FRandRange(0.6f, 1.4f) * (IsBlackout() ? 0.5f : 1.f);

	// Choix pondere de l'espece
	float Total = 0.f;
	for (const FBREntitySpawn& S : D.Entities)
	{
		Total += S.Weight;
	}
	float Pick = FMath::FRandRange(0.f, Total);
	EBREntityKind Kind = D.Entities[0].Kind;
	for (const FBREntitySpawn& S : D.Entities)
	{
		Pick -= S.Weight;
		if (Pick <= 0.f)
		{
			Kind = S.Kind;
			break;
		}
	}

	const FBREntityInfo& Info = ABREntity::Info(Kind);
	const float MaxDist = FMath::Min(4200.f, D.ViewDistance - ChunkWorld * 0.5f);
	const FVector Eye = P->GetEyeLocation();
	for (int32 Attempt = 0; Attempt < 30; ++Attempt)
	{
		const float Ang = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = FMath::FRandRange(1600.f, FMath::Max(1700.f, MaxDist));
		const FVector Cand = PL + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Dist;
		const FIntPoint Cell = WorldToCell(Cand);
		if (!IsWalkable(Cell) || IsSpawnArea(Cell.X, Cell.Y) || !IsChunkLoaded(CellToChunk(Cell)))
		{
			continue;
		}
		const float Z = Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f;
		const FVector Loc = CellCenter(Cell, Z);
		if (Info.bNeedsDark && LightLevelAt(Loc) > 0.12f)
		{
			continue;
		}
		// Pas d'apparition sous les yeux d'un joueur
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRSpawnLOS), false, P);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Loc, ECC_Visibility, Q);
		if ((!bBlocked && Dist < 3000.f) || (Players.Num() > 1 && !IsHiddenFromPlayers(Loc)))
		{
			continue;
		}
		SpawnEntity(Kind, Loc);
		// Les papillons de la mort se deplacent en essaim
		if (Kind == EBREntityKind::Deathmoth)
		{
			for (int32 k = 0; k < 2 && Entities.Num() < MaxRegular + 2; ++k)
			{
				const FVector Off(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-40.f, 40.f));
				if (ABREntity* Moth = SpawnEntity(Kind, Loc + Off))
				{
					Moth->SetVisualScale(0.75f);
				}
			}
		}
		return;
	}
}

void ABRWorld::UpdatePhenomena(float Dt)
{
	const FBRLevelDef& D = Def();
	ABRCharacter* P = GetPlayer();
	UBRAssets* A = UBRAssets::Get(this);
	if (!P || !A || P->IsDead())
	{
		return;
	}

	PhenomenaTimer -= Dt;
	const bool bInsane = P->Sanity < 35.f;
	const bool bTroubled = P->Sanity < 50.f;
	if (PhenomenaTimer > 0.f || (!D.bPhenomena && !bTroubled))
	{
		return;
	}
	PhenomenaTimer = FMath::FRandRange(35.f, 90.f) * (bInsane ? 0.45f : (bTroubled ? 0.7f : 1.f));

	// Hallucinations : plus la sante mentale baisse, plus on "voit" des choses
	if (bTroubled && FMath::FRand() < (bInsane ? 0.7f : 0.45f) && SpawnHallucination(P))
	{
		return;
	}

	const float Ang = FMath::FRandRange(0.f, 2.f * PI);
	const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
	const int32 Roll = FMath::RandRange(0, 2);
	FName SoundName = TEXT("S_DistantSteps");
	float Dist = 1400.f;
	if (bInsane && Roll == 0)
	{
		SoundName = TEXT("S_Whisper");
		Dist = 300.f;
	}
	else if (Roll == 1)
	{
		SoundName = TEXT("S_Flicker");
		Dist = 700.f;
	}
	if (USoundBase* S = A->Sound(SoundName))
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, P->GetActorLocation() + Dir * Dist + FVector(0, 0, 100.f), 1.f, 1.f, 0.f,
			A->Attenuation(4000.f));
	}
}

bool ABRWorld::SpawnHallucination(ABRCharacter* P)
{
	UWorld* W = GetWorld();
	if (!W || !P || !bLevelReady || IsTransitioning())
	{
		return false;
	}
	const FVector Eye = P->GetEyeLocation();
	const FVector Fwd = P->GetViewDirection().GetSafeNormal2D();
	const bool bSmile = FMath::FRand() < 0.35f;
	for (int32 Try = 0; Try < 16; ++Try)
	{
		// Silhouette : au bord du champ de vision (on tourne la tete... plus rien). Sourire : droit devant, dans le noir
		const float Side = FMath::FRand() < 0.5f ? 1.f : -1.f;
		const float Ang = bSmile ? FMath::FRandRange(-18.f, 18.f) : Side * FMath::FRandRange(32.f, 48.f);
		const FVector Dir = FRotator(0.f, Ang, 0.f).RotateVector(Fwd);
		const float Dist = bSmile ? FMath::FRandRange(1200.f, 2200.f) : FMath::FRandRange(900.f, 1700.f);
		const FIntPoint Cell = WorldToCell(P->GetActorLocation() + Dir * Dist);
		if (!IsWalkable(Cell) || IsPoolCell(Cell.X, Cell.Y) || !IsChunkLoaded(CellToChunk(Cell)))
		{
			continue;
		}
		const FVector Loc = CellCenter(Cell, 0.f);
		if (bSmile && LightLevelAt(Loc) > 0.3f)
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRHallucination), false, P);
		if (W->LineTraceSingleByChannel(Hit, Eye, Loc + FVector(0.f, 0.f, 120.f), ECC_Visibility, Q))
		{
			continue; // il faut pouvoir l'apercevoir
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ABRHallucination* H = W->SpawnActor<ABRHallucination>(ABRHallucination::StaticClass(), FTransform(Loc), Params))
		{
			H->Init(bSmile ? EBRHallucination::Smile : EBRHallucination::Shadow, P);
			return true;
		}
	}
	return false;
}

void ABRWorld::UpdateAudio(float Dt)
{
	const FBRLevelDef& D = Def();
	if (AmbientAudio->Sound && !AmbientAudio->IsPlaying())
	{
		AmbientAudio->Play();
	}
	if (HumAudio->Sound)
	{
		if (!HumAudio->IsPlaying())
		{
			HumAudio->Play();
		}
		const ABRCharacter* P = GetPlayer();
		const float Light = P ? LightLevelAt(P->GetActorLocation()) : 0.f;
		HumAudio->SetVolumeMultiplier(FMath::Max(0.01f, D.HumVolume * FMath::Clamp(Light, 0.f, 1.f) * (1.f - Fade)));
	}
	AmbientAudio->SetVolumeMultiplier(FMath::Max(0.01f, D.AmbientVolume * (1.f - Fade * 0.8f)));
}

// =====================================================================================
// Coupures de courant (v2)
// =====================================================================================

void ABRWorld::ForceBlackout()
{
	if (HasAuthority() && BlackoutPhase == EBlackout::None && !Def().bOutdoor && Def().Fixture != EBRFixture::None)
	{
		BlackoutTimer = 0.f;
		BlackoutPhase = EBlackout::None;
		// Le prochain UpdateBlackout declenchera la coupure
		UpdateBlackout(0.f);
	}
}

void ABRWorld::UpdateBlackout(float Dt)
{
	const FBRLevelDef& D = Def();
	const bool bAuth = HasAuthority();
	const bool bAllowed = D.bBlackouts || BlackoutPhase != EBlackout::None || (bAuth && BlackoutTimer <= 0.f);
	if (!bAllowed || D.Fixture == EBRFixture::None || D.bOutdoor)
	{
		if (Power < 1.f)
		{
			Power = 1.f;
			ApplyPower(false);
		}
		return;
	}

	// Seul le serveur fait avancer les phases ; un client anime le vacillement en attendant la suivante
	BlackoutTimer -= Dt;
	switch (BlackoutPhase)
	{
	case EBlackout::None:
		if (bAuth && BlackoutTimer <= 0.f)
		{
			EnterBlackoutPhase(static_cast<uint8>(EBlackout::Failing));
		}
		break;
	case EBlackout::Failing:
	case EBlackout::Restoring:
	{
		const bool bFailing = BlackoutPhase == EBlackout::Failing;
		if (BlackoutTimer > 0.f)
		{
			// Les neons vacillent de plus en plus (ou de moins en moins) avant de lacher
			const float Total = bFailing ? 1.8f : 1.4f;
			const float T = FMath::Clamp(BlackoutTimer / Total, 0.f, 1.f);
			const float OnChance = bFailing ? T * 0.8f : 1.f - T * 0.8f;
			PowerFlickerTimer -= Dt;
			if (PowerFlickerTimer <= 0.f)
			{
				PowerFlickerTimer = FMath::FRandRange(0.04f, 0.16f);
				Power = FMath::FRand() < OnChance ? FMath::FRandRange(0.6f, 1.f) : FMath::FRandRange(0.f, 0.08f);
			}
		}
		else if (bAuth)
		{
			EnterBlackoutPhase(static_cast<uint8>(bFailing ? EBlackout::Dark : EBlackout::None));
		}
		else
		{
			Power = bFailing ? 0.f : 1.f;
		}
		break;
	}
	case EBlackout::Dark:
		Power = 0.f;
		if (bAuth && BlackoutTimer <= 0.f)
		{
			EnterBlackoutPhase(static_cast<uint8>(EBlackout::Restoring));
		}
		break;
	}
	ApplyPower(false);
}

void ABRWorld::EnterBlackoutPhase(uint8 Phase, bool bSilent)
{
	const FBRLevelDef& D = Def();
	const bool bAuth = HasAuthority();
	UBRAssets* A = UBRAssets::Get(this);
	auto Play = [this, A, bSilent](const TCHAR* Name, float Volume)
	{
		if (A && !bSilent)
		{
			if (USoundBase* S = A->Sound(FName(Name)))
			{
				UGameplayStatics::PlaySound2D(this, S, Volume);
			}
		}
	};

	BlackoutPhase = static_cast<EBlackout>(FMath::Min<uint8>(Phase, static_cast<uint8>(EBlackout::Restoring)));
	PowerFlickerTimer = 0.f;
	if (bAuth)
	{
		NetBlackout = static_cast<uint8>(BlackoutPhase);
	}
	switch (BlackoutPhase)
	{
	case EBlackout::Failing:
		BlackoutTimer = 1.8f;
		Play(TEXT("S_Blackout"), 1.f);
		break;
	case EBlackout::Dark:
		BlackoutTimer = bAuth ? FMath::FRandRange(24.f, 40.f) : 0.f;
		Power = 0.f;
		if (bAuth)
		{
			SpawnBlackoutEntities();
		}
		if (!bSilent)
		{
			ABRHUD::Notify(this, TEXT("COUPURE DE COURANT"), 4.f, FLinearColor(1.f, 0.3f, 0.25f));
			if (D.bRequireObjectives && !bBlackoutRecorded)
			{
				ABRHUD::Notify(this, TEXT("Filmez pendant la coupure : cam\u00e9scope en MAIN."), 5.f, FLinearColor(1.f, 0.85f, 0.4f));
			}
		}
		break;
	case EBlackout::Restoring:
		BlackoutTimer = 1.4f;
		Play(TEXT("S_PowerUp"), 0.9f);
		if (bAuth)
		{
			DismissBlackoutEntities();
		}
		break;
	default:
		Power = 1.f;
		if (bAuth)
		{
			BlackoutTimer = FMath::FRandRange(D.BlackoutMinInterval, FMath::Max(D.BlackoutMinInterval, D.BlackoutMaxInterval));
		}
		break;
	}
	ApplyPower(false);
}

void ABRWorld::ApplyPower(bool bForce)
{
	if (!bForce && FMath::Abs(Power - AppliedPower) < 0.01f)
	{
		return;
	}
	AppliedPower = Power;
	for (TPair<FIntPoint, TObjectPtr<ABRChunk>>& Pair : Chunks)
	{
		if (Pair.Value)
		{
			Pair.Value->SetPower(Power);
		}
	}
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		A->SetGlowScale(Power);
	}
}

// =====================================================================================
// Objectifs (v2)
// =====================================================================================

void ABRWorld::GetObjectives(TArray<FBRObjective>& Out) const
{
	Out.Reset();
	const FBRLevelDef& D = Def();
	if (D.bRequireObjectives)
	{
		FBRObjective Vhs;
		Vhs.Text = TEXT("TROUVER LES CASSETTES VHS");
		Vhs.Progress = FMath::Min(VHSFound, D.VHSRequired);
		Vhs.Goal = D.VHSRequired;
		Vhs.bRequired = true;
		Out.Add(Vhs);
	}
	if (D.bBlackouts)
	{
		FBRObjective Rec;
		Rec.Text = TEXT("FILMER PENDANT UNE COUPURE");
		Rec.Progress = bBlackoutRecorded ? 1 : 0;
		Rec.Partial = bBlackoutRecorded ? 1.f : FMath::Clamp(BlackoutRecordTime / 5.f, 0.f, 1.f);
		Rec.bRequired = D.bRequireObjectives;
		Out.Add(Rec);
	}
	if (D.Entities.Num() > 0 && D.MaxEntities > 0)
	{
		FBRObjective Ent;
		Ent.Text = TEXT("FILMER UNE ENTIT\u00c9");
		Ent.Progress = bEntityRecorded ? 1 : 0;
		Ent.Partial = bEntityRecorded ? 1.f : FMath::Clamp(EntityRecordTime / 3.f, 0.f, 1.f);
		Out.Add(Ent);
	}
	FBRObjective Exit;
	Exit.Text = D.bRequireObjectives ? TEXT("STABILISER ET PRENDRE LA SORTIE") : TEXT("TROUVER UNE SORTIE");
	Exit.Progress = 0;
	Out.Add(Exit);
}

bool ABRWorld::AreObjectivesComplete() const
{
	const FBRLevelDef& D = Def();
	if (!D.bRequireObjectives)
	{
		return true;
	}
	return VHSFound >= D.VHSRequired && (bBlackoutRecorded || !D.bBlackouts);
}

bool ABRWorld::CanLeaveLevel(FString& OutReason) const
{
	if (AreObjectivesComplete())
	{
		return true;
	}
	const FBRLevelDef& D = Def();
	OutReason = FString::Printf(TEXT("La sortie est instable... Cassettes VHS %d/%d"), FMath::Min(VHSFound, D.VHSRequired), D.VHSRequired);
	if (D.bBlackouts)
	{
		OutReason += FString::Printf(TEXT(", filmer pendant une coupure %d/1"), bBlackoutRecorded ? 1 : 0);
	}
	OutReason += TEXT("  ") + BRKeys::Tag(EBRAction::Inventory);
	return false;
}

void ABRWorld::CompleteTask(const FString& Text)
{
	ABRHUD::Notify(this, FString::Printf(TEXT("T\u00c2CHE ACCOMPLIE : %s"), *Text), 5.f, FLinearColor(0.55f, 1.f, 0.55f));
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Objective")))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.8f);
		}
	}
	if (Def().bRequireObjectives && AreObjectivesComplete())
	{
		ABRHUD::Notify(this, TEXT("Les sorties se sont stabilis\u00e9es. Trouvez un passage (noclip) pour quitter le Niveau."), 7.f,
			FLinearColor(1.f, 0.9f, 0.5f));
	}
}

void ABRWorld::OnVHSCollected()
{
	if (!HasAuthority())
	{
		// Les cassettes trouvees par chacun comptent pour tout le groupe
		if (ABRPlayerController* PC = LocalPC())
		{
			PC->ServerVHSCollected();
		}
		return;
	}
	++VHSFound;
	PrevVHSFound = VHSFound;
	AnnounceVHS();
}

void ABRWorld::AnnounceVHS()
{
	const FBRLevelDef& D = Def();
	if (!D.bRequireObjectives)
	{
		ABRHUD::Notify(this, FString::Printf(TEXT("+1 Cassette VHS  (%d)"), VHSFound), 3.f, FLinearColor(0.9f, 0.88f, 0.75f));
		return;
	}
	if (VHSFound == D.VHSRequired)
	{
		CompleteTask(FString::Printf(TEXT("CASSETTES VHS %d/%d"), D.VHSRequired, D.VHSRequired));
	}
	else if (VHSFound < D.VHSRequired)
	{
		ABRHUD::Notify(this, FString::Printf(TEXT("Cassette VHS  %d/%d"), VHSFound, D.VHSRequired), 3.f, FLinearColor(1.f, 0.85f, 0.4f));
	}
}

ABREntity* ABRWorld::FindVisibleEntity(const FVector& Eye, const FVector& Dir, float MaxDist, float MinDot) const
{
	const UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	ABREntity* Best = nullptr;
	float BestDot = MinDot;
	for (ABREntity* E : Entities)
	{
		if (!IsValid(E))
		{
			continue;
		}
		const FVector Target = E->GetActorLocation();
		const FVector To = Target - Eye;
		const float Dist = static_cast<float>(To.Size());
		if (Dist > MaxDist || Dist < 1.f)
		{
			continue;
		}
		const float Dot = static_cast<float>(FVector::DotProduct(To / Dist, Dir));
		if (Dot < BestDot)
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRRecordLOS), false);
		Q.AddIgnoredActor(E);
		if (const ABRCharacter* P = GetPlayer())
		{
			Q.AddIgnoredActor(P);
		}
		if (W->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Q))
		{
			continue;
		}
		Best = E;
		BestDot = Dot;
	}
	return Best;
}

void ABRWorld::NotifyRecording(float Dt, const FVector& Eye, const FVector& Dir)
{
	const FBRLevelDef& D = Def();
	RecordLabel.Empty();
	RecordProgress = 0.f;
	if (IsTransitioning())
	{
		return;
	}

	// Filmer pendant une coupure
	if (D.bBlackouts && !bBlackoutRecorded && !bObjectiveSent[0] && BlackoutPhase == EBlackout::Dark)
	{
		BlackoutRecordTime += Dt;
		RecordLabel = TEXT("COUPURE DE COURANT");
		RecordProgress = FMath::Clamp(BlackoutRecordTime / 5.f, 0.f, 1.f);
		if (BlackoutRecordTime >= 5.f)
		{
			CompleteObjective(0);
		}
		return;
	}

	// Filmer une entite
	if (!bEntityRecorded && !bObjectiveSent[1])
	{
		if (ABREntity* E = FindVisibleEntity(Eye, Dir, 2600.f, 0.9f))
		{
			EntityRecordTime += Dt;
			RecordLabel = ABREntity::Info(E->Kind).Name.ToUpper();
			RecordProgress = FMath::Clamp(EntityRecordTime / 3.f, 0.f, 1.f);
			if (EntityRecordTime >= 3.f)
			{
				Discover(E->Kind);
				CompleteObjective(1);
			}
		}
	}
}

// =====================================================================================
// Grille procedurale
// =====================================================================================

float ABRWorld::CellSize() const
{
	return Def().CellSize;
}

FIntPoint ABRWorld::WorldToCell(const FVector& P) const
{
	const double S = CellSize();
	return FIntPoint(Floor32(P.X / S), Floor32(P.Y / S));
}

FVector ABRWorld::CellCenter(const FIntPoint& C, float Z) const
{
	const float S = CellSize();
	return FVector((C.X + 0.5f) * S, (C.Y + 0.5f) * S, Z);
}

FIntPoint ABRWorld::CellToChunk(const FIntPoint& C) const
{
	const int32 N = Def().ChunkCells;
	return FIntPoint(BRHash::FloorDiv(C.X, N), BRHash::FloorDiv(C.Y, N));
}

bool ABRWorld::IsSpawnArea(int32 X, int32 Y) const
{
	return FMath::Abs(X) <= 1 && FMath::Abs(Y) <= 1;
}

bool ABRWorld::IsPoolCell(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	if (!D.bWater || D.PoolChance <= 0.f || IsSolid(X, Y) || (FMath::Abs(X) <= 2 && FMath::Abs(Y) <= 2))
	{
		return false;
	}
	// Bassins par blocs de 2x2 cellules : de vraies piscines, pas des trous isoles
	return BRHash::Rand(BRHash::FloorDiv(X, 2), BRHash::FloorDiv(Y, 2), 1700, Seed) < D.PoolChance;
}

float ABRWorld::FloorZAt(const FVector& P) const
{
	const FIntPoint C = WorldToCell(P);
	return IsPoolCell(C.X, C.Y) ? -Def().PoolDepth : 0.f;
}

bool ABRWorld::FindSpawnSpot(EBREntityKind Kind, const ABRCharacter* Anchor, float MinDist, float MaxDist, bool bAvoidSight, FVector& Out) const
{
	const ABRCharacter* P = Anchor;
	if (!P || !GetWorld())
	{
		return false;
	}
	const FBREntityInfo& Info = ABREntity::Info(Kind);
	const FVector PL = P->GetActorLocation();
	const FVector Eye = P->GetEyeLocation();
	for (int32 Attempt = 0; Attempt < 40; ++Attempt)
	{
		const float Ang = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = FMath::FRandRange(MinDist, MaxDist);
		const FIntPoint Cell = WorldToCell(PL + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Dist);
		if (!IsWalkable(Cell) || IsSpawnArea(Cell.X, Cell.Y) || IsPoolCell(Cell.X, Cell.Y) || !IsChunkLoaded(CellToChunk(Cell)))
		{
			continue;
		}
		const FVector Loc = CellCenter(Cell, Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f);
		if (Info.bNeedsDark && LightLevelAt(Loc) > 0.12f)
		{
			continue;
		}
		if (bAvoidSight)
		{
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BRSpawnSpot), false, P);
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Loc, ECC_Visibility, Q) || (IsNetGame() && !IsHiddenFromPlayers(Loc)))
			{
				continue;
			}
		}
		Out = Loc;
		return true;
	}
	return false;
}

void ABRWorld::UpdatePatrol(float Dt)
{
	const FBRLevelDef& D = Def();
	RedSources.Reset();
	if (!D.bPatrolEntity)
	{
		return;
	}
	bool bPresent = false;
	for (const ABREntity* E : Entities)
	{
		if (IsValid(E) && E->Kind == D.PatrolKind)
		{
			bPresent = true;
			if (D.RedLightRadius > 0.f)
			{
				RedSources.Add(E->GetActorLocation());
			}
		}
	}
	// Le reste (faire revenir l'entite) est decide par le serveur
	ABRCharacter* P = HasAuthority() ? RandomLivingPlayer() : nullptr;
	if (bPresent || !P || IsTransitioning() || LevelTime < D.PatrolDelay)
	{
		return;
	}
	if (const ABRPlayerController* PC = LocalPC())
	{
		if (PC->IsInMenu())
		{
			return;
		}
	}
	PatrolSpawnTimer -= Dt;
	if (PatrolSpawnTimer > 0.f)
	{
		return;
	}
	PatrolSpawnTimer = 4.f;
	FVector Loc;
	if (FindSpawnSpot(D.PatrolKind, P, 1500.f, 2600.f, true, Loc))
	{
		SpawnEntity(D.PatrolKind, Loc);
		PatrolSpawnTimer = 15.f;
	}
}

void ABRWorld::SpawnBlackoutEntities()
{
	const FBRLevelDef& D = Def();
	TArray<ABRCharacter*> Players;
	GetPlayers(Players);
	Players.RemoveAll([](const ABRCharacter* C) { return C->IsDead(); });
	if (D.BlackoutSmilers <= 0 || Players.Num() == 0)
	{
		return;
	}
	// Dans le noir, a portee de vue : on distingue leurs yeux et leur sourire (reparti entre les joueurs)
	const int32 Count = D.BlackoutSmilers + FMath::Min(Players.Num() - 1, 2);
	for (int32 i = 0; i < Count; ++i)
	{
		FVector Loc;
		if (FindSpawnSpot(EBREntityKind::Smiler, Players[i % Players.Num()], 900.f, 1900.f, false, Loc))
		{
			if (ABREntity* E = SpawnEntity(EBREntityKind::Smiler, Loc))
			{
				BlackoutEntities.Add(E);
			}
		}
	}
}

void ABRWorld::DismissBlackoutEntities()
{
	for (const TWeakObjectPtr<ABREntity>& Weak : BlackoutEntities)
	{
		if (ABREntity* E = Weak.Get())
		{
			E->Dismiss();
		}
	}
	BlackoutEntities.Reset();
}

bool ABRWorld::IsInHidingSpot(const FVector& Location, bool bCrouched) const
{
	const TObjectPtr<ABRChunk>* C = Chunks.Find(CellToChunk(WorldToCell(Location)));
	return C && *C && (*C)->IsInHidingSpot(Location, bCrouched);
}

bool ABRWorld::FindHidingSpotNear(const FVector& Location, float Radius, bool& bOutNeedsCrouch) const
{
	const TObjectPtr<ABRChunk>* C = Chunks.Find(CellToChunk(WorldToCell(Location)));
	return C && *C && (*C)->FindHidingSpotNear(Location, Radius, bOutNeedsCrouch);
}

FBRSurface ABRWorld::GetWaterSurface() const
{
	FBRSurface S = Def().Water;
	if (S.Texture.IsNone())
	{
		S = FBRSurface(TEXT("T_WaterNormal"), FLinearColor(0.3f, 0.45f, 0.5f), 200.f, 0.05f, 0.f);
	}
	return S;
}

void ABRWorld::AddWaterRipple(const FVector& Location, float Strength)
{
	if (!Current || !Current->bWater || Strength <= 0.f || !WaterSim)
	{
		return;
	}
	WaterSim->AddImpulse(FVector2D(Location.X, Location.Y), 10.f + 7.f * Strength, 0.6f * Strength);
}

void ABRWorld::UpdateWaterSim(float Dt)
{
	if (!Current || !Current->bWater)
	{
		return;
	}
	const ABRCharacter* P = GetPlayer();
	if (!P)
	{
		return;
	}
	if (!WaterSim)
	{
		WaterSim = NewObject<UBRWaterSim>(this);
	}
	const float WaterZ = Current->WaterHeight;

	// Tout ce qui traverse la surface fend l'eau : les joueurs (le sien et ceux des autres) et les entites
	TArray<AActor*> Bodies;
	TArray<ABRCharacter*> Players;
	GetPlayers(Players);
	Bodies.Append(Players);
	for (ABREntity* E : Entities)
	{
		if (IsValid(E))
		{
			Bodies.Add(E);
		}
	}
	for (const AActor* Body : Bodies)
	{
		float Radius = 0.f;
		float HalfHeight = 0.f;
		Body->GetSimpleCollisionCylinder(Radius, HalfHeight);
		const FVector L = Body->GetActorLocation();
		const float Bottom = static_cast<float>(L.Z) - HalfHeight;
		const float Top = static_cast<float>(L.Z) + HalfHeight;
		if (Bottom > WaterZ - 2.f || Top < WaterZ - 40.f)
		{
			continue; // hors de l'eau, ou entierement dessous
		}
		// Un marcheur fend l'eau en avancant ; un nageur la brasse aussi sur place (battements reguliers)
		const bool bSwim = Bottom < WaterZ - 100.f;
		const FVector Vel = Body->GetVelocity();
		const float Speed = static_cast<float>(Vel.Size2D());
		const FVector2D Pos(L.X, L.Y);
		const float Size = FMath::Clamp(Radius, 15.f, 60.f) * (bSwim ? 1.f : 0.6f);
		if (Speed > 15.f)
		{
			WaterSim->AddMover(Pos, FVector2D(Vel.X, Vel.Y) / Speed, Size, 0.3f * FMath::Min(Speed, 700.f));
		}
		if (bSwim)
		{
			WaterSim->AddMover(Pos, FVector2D::ZeroVector, Size, 14.f * FMath::Sin(LevelTime * 5.5f));
		}
	}

	// Gouttes qui tombent du plafond autour du joueur : l'eau n'est jamais tout a fait immobile
	DripTimer -= Dt;
	if (DripTimer <= 0.f)
	{
		DripTimer = FMath::FRandRange(0.8f, 2.4f);
		const FVector L = P->GetActorLocation() + FVector(FMath::FRandRange(-700.f, 700.f), FMath::FRandRange(-700.f, 700.f), 0.f);
		WaterSim->AddImpulse(FVector2D(L.X, L.Y), 9.f, FMath::FRandRange(0.25f, 0.5f));
	}

	WaterSim->Tick(Dt, P->GetActorLocation(), this);
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		A->SetWaterSim(WaterSim->GetTexture(), WaterSim->GetWindow());
	}
}

void ABRWorld::SetUnderwater(float Blend)
{
	Blend = FMath::Clamp(Blend, 0.f, 1.f);
	if (!Fog || FMath::Abs(Blend - UnderwaterBlend) < 0.01f)
	{
		return;
	}
	UnderwaterBlend = Blend;
	const FBRLevelDef& D = Def();
	// Eau limpide : on voit loin, mais tout se noie dans le turquoise
	Fog->SetFogDensity(FMath::Lerp(D.FogDensity, 0.09f, Blend));
	Fog->SetFogHeightFalloff(FMath::Lerp(D.FogFalloff, 0.001f, Blend));
	Fog->SetFogInscatteringColor(FMath::Lerp(D.FogColor, FLinearColor(0.08f, 0.36f, 0.34f), Blend));
	Fog->SetStartDistance(FMath::Lerp(D.FogStart, 0.f, Blend));
}

float ABRWorld::ZoneDensity(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	const float R = BRHash::Rand(BRHash::FloorDiv(X, 6), BRHash::FloorDiv(Y, 6), 104, Seed);
	if (R < D.OpenZoneChance)
	{
		return 0.25f;
	}
	if (R > 0.85f)
	{
		return 1.35f;
	}
	return 1.f;
}

bool ABRWorld::MazeOpen(int32 X, int32 Y, bool bEast) const
{
	const FBRLevelDef& D = Def();
	const bool bCarveEast = BRHash::Rand(X, Y, 201, Seed) < 0.5f;
	if (bEast == bCarveEast)
	{
		return true;
	}
	if (BRHash::Rand(X, Y, bEast ? 202 : 203, Seed) < D.LoopChance)
	{
		return true;
	}
	// Petites salles ouvertes dans le labyrinthe
	const int32 ZX = BRHash::FloorDiv(X, 4);
	const int32 ZY = BRHash::FloorDiv(Y, 4);
	if (BRHash::Rand(ZX, ZY, 204, Seed) < D.OpenZoneChance)
	{
		return bEast ? (BRHash::PosMod(X, 4) != 3) : (BRHash::PosMod(Y, 4) != 3);
	}
	return false;
}

bool ABRWorld::HasHouse(int32 LotX, int32 LotY) const
{
	return BRHash::Rand(LotX, LotY, 801, Seed) < 0.85f;
}

float ABRWorld::BuildingHeight(int32 BlockX, int32 BlockY) const
{
	if (BRHash::Rand(BlockX, BlockY, 701, Seed) < 0.12f)
	{
		return 0.f; // place / parc
	}
	const float R = BRHash::Rand(BlockX, BlockY, 702, Seed);
	return 1500.f + R * R * 7000.f;
}

bool ABRWorld::IsSolid(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	switch (D.Layout)
	{
	case EBRLayout::Hotel:
	{
		if (IsSpawnArea(X, Y))
		{
			return false;
		}
		const bool bCorrX = BRHash::PosMod(X, D.Spacing) == 0 && BRHash::Rand(X, 0, 501, Seed) < 0.85f;
		const bool bCorrY = BRHash::PosMod(Y, D.Spacing) == 0 && BRHash::Rand(0, Y, 502, Seed) < 0.85f;
		if (bCorrX || bCorrY)
		{
			return false;
		}
		const int32 ZX = BRHash::FloorDiv(X, D.Spacing);
		const int32 ZY = BRHash::FloorDiv(Y, D.Spacing);
		return BRHash::Rand(ZX, ZY, 503, Seed) >= D.OpenZoneChance;
	}
	case EBRLayout::Caves:
	{
		if (FMath::Abs(X) <= 1 && FMath::Abs(Y) <= 1)
		{
			return false;
		}
		const float R = 0.55f * BRHash::Rand(X, Y, 601, Seed)
			+ 0.45f * BRHash::Rand(BRHash::FloorDiv(X, 3), BRHash::FloorDiv(Y, 3), 602, Seed);
		return R < D.SolidChance;
	}
	case EBRLayout::Suburbs:
	{
		const int32 Sp = D.Spacing;
		if (BRHash::PosMod(X, Sp) == 0 || BRHash::PosMod(Y, Sp) == 0)
		{
			return false;
		}
		const int32 LotX = BRHash::FloorDiv(X, Sp) * 2 + (BRHash::PosMod(X, Sp) - 1) / 2;
		const int32 LotY = BRHash::FloorDiv(Y, Sp) * 2 + (BRHash::PosMod(Y, Sp) - 1) / 2;
		return HasHouse(LotX, LotY);
	}
	case EBRLayout::City:
	{
		const int32 Sp = D.Spacing;
		if (BRHash::PosMod(X, Sp) == 0 || BRHash::PosMod(Y, Sp) == 0)
		{
			return false;
		}
		return BuildingHeight(BRHash::FloorDiv(X, Sp), BRHash::FloorDiv(Y, Sp)) > 0.f;
	}
	default:
		return false;
	}
}

EBREdge ABRWorld::EdgeE(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	if (IsSpawnArea(X, Y) && IsSpawnArea(X + 1, Y))
	{
		return EBREdge::Open;
	}
	if (D.Layout == EBRLayout::Rooms)
	{
		const int32 Seg = FMath::Max(1, D.SegmentLength);
		const int32 Off = static_cast<int32>(BRHash::Hash(X, 0, 101, Seed) % static_cast<uint32>(Seg));
		const int32 SegIdx = BRHash::FloorDiv(Y + Off, Seg);
		if (BRHash::Rand(X, SegIdx, 102, Seed) >= D.WallLineChance * ZoneDensity(X, Y))
		{
			return EBREdge::Open;
		}
		return BRHash::Rand(X, Y, 103, Seed) < D.DoorChance ? EBREdge::Door : EBREdge::Wall;
	}
	if (D.Layout == EBRLayout::Maze)
	{
		return MazeOpen(X, Y, true) ? EBREdge::Open : EBREdge::Wall;
	}
	return EBREdge::Open;
}

EBREdge ABRWorld::EdgeN(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	if (IsSpawnArea(X, Y) && IsSpawnArea(X, Y + 1))
	{
		return EBREdge::Open;
	}
	if (D.Layout == EBRLayout::Rooms)
	{
		const int32 Seg = FMath::Max(1, D.SegmentLength);
		const int32 Off = static_cast<int32>(BRHash::Hash(0, Y, 111, Seed) % static_cast<uint32>(Seg));
		const int32 SegIdx = BRHash::FloorDiv(X + Off, Seg);
		if (BRHash::Rand(SegIdx, Y, 112, Seed) >= D.WallLineChance * ZoneDensity(X, Y))
		{
			return EBREdge::Open;
		}
		return BRHash::Rand(X, Y, 113, Seed) < D.DoorChance ? EBREdge::Door : EBREdge::Wall;
	}
	if (D.Layout == EBRLayout::Maze)
	{
		return MazeOpen(X, Y, false) ? EBREdge::Open : EBREdge::Wall;
	}
	return EBREdge::Open;
}

bool ABRWorld::CanStep(const FIntPoint& From, const FIntPoint& To) const
{
	if (IsSolid(To.X, To.Y) || IsSolid(From.X, From.Y))
	{
		return false;
	}
	const int32 DX = To.X - From.X;
	const int32 DY = To.Y - From.Y;
	if (DX == 1 && DY == 0)
	{
		return EdgeE(From.X, From.Y) != EBREdge::Wall;
	}
	if (DX == -1 && DY == 0)
	{
		return EdgeE(To.X, To.Y) != EBREdge::Wall;
	}
	if (DX == 0 && DY == 1)
	{
		return EdgeN(From.X, From.Y) != EBREdge::Wall;
	}
	if (DX == 0 && DY == -1)
	{
		return EdgeN(To.X, To.Y) != EBREdge::Wall;
	}
	return false;
}

bool ABRWorld::HasPillar(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	if (D.Layout != EBRLayout::Rooms || D.PillarChance <= 0.f)
	{
		return false;
	}
	const float Mult = ZoneDensity(X, Y) < 0.5f ? 2.5f : 1.f;
	return BRHash::Rand(X, Y, 301, Seed) < D.PillarChance * Mult;
}

bool ABRWorld::IsDarkZone(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	if (D.Fixture == EBRFixture::None)
	{
		return !D.bOutdoor;
	}
	if (FMath::Abs(X) <= 3 && FMath::Abs(Y) <= 3)
	{
		return false;
	}
	return BRHash::Rand(BRHash::FloorDiv(X, 5), BRHash::FloorDiv(Y, 5), 401, Seed) < D.DarkZoneChance;
}

FBRLightInfo ABRWorld::CellLight(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	FBRLightInfo L;
	if (D.Fixture == EBRFixture::None || IsSolid(X, Y))
	{
		return L;
	}
	const bool bSpawn = IsSpawnArea(X, Y);

	auto Finish = [&](FBRLightInfo& Out)
	{
		Out.bHas = true;
		Out.bBroken = !bSpawn && BRHash::Rand(X, Y, 411, Seed) < D.BrokenChance;
		Out.bFlicker = !Out.bBroken && BRHash::Rand(X, Y, 412, Seed) < D.FlickerChance;
		Out.bShadow = BRHash::Rand(X, Y, 413, Seed) < D.ShadowChance;
	};

	switch (D.Layout)
	{
	case EBRLayout::Hotel:
	{
		// Appliques murales : uniquement contre un mur de chambre
		if (BRHash::PosMod(X + Y, 2) != 0 || (!bSpawn && BRHash::Rand(X, Y, 410, Seed) >= D.LightChance))
		{
			return L;
		}
		const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		const int32 Start = static_cast<int32>(BRHash::Hash(X, Y, 417, Seed) % 4u);
		for (int32 k = 0; k < 4; ++k)
		{
			const FIntPoint Dir = Dirs[(Start + k) % 4];
			if (IsSolid(X + Dir.X, Y + Dir.Y))
			{
				Finish(L);
				const float Half = D.CellSize * 0.5f;
				L.Offset = FVector(Dir.X * Half, Dir.Y * Half, 0.f);
				L.Yaw = FMath::RadiansToDegrees(FMath::Atan2(-static_cast<float>(Dir.Y), -static_cast<float>(Dir.X)));
				return L;
			}
		}
		return L;
	}
	case EBRLayout::Suburbs:
	case EBRLayout::City:
	{
		// Lampadaires le long des rues, sur le bas-cote
		const int32 Sp = D.Spacing;
		const bool bRoadX = BRHash::PosMod(X, Sp) == 0;
		const bool bRoadY = BRHash::PosMod(Y, Sp) == 0;
		if (bRoadX == bRoadY)
		{
			return L; // pas sur une rue, ou intersection
		}
		const int32 Along = bRoadX ? Y : X;
		if (BRHash::PosMod(Along, 2) != 0 || (!bSpawn && BRHash::Rand(X, Y, 410, Seed) >= D.LightChance))
		{
			return L;
		}
		Finish(L);
		const float Side = (BRHash::PosMod(Along, 4) == 0 ? 1.f : -1.f) * D.CellSize * 0.42f;
		L.Offset = bRoadX ? FVector(Side, 0.f, 0.f) : FVector(0.f, Side, 0.f);
		L.Yaw = FMath::RadiansToDegrees(FMath::Atan2(-static_cast<float>(L.Offset.Y), -static_cast<float>(L.Offset.X)));
		return L;
	}
	case EBRLayout::Open:
		return L;
	default:
	{
		if (!bSpawn && IsDarkZone(X, Y))
		{
			return L;
		}
		if (!bSpawn && BRHash::Rand(X, Y, 410, Seed) >= D.LightChance)
		{
			return L;
		}
		Finish(L);
		const float J = D.CellSize * 0.12f;
		L.Offset = FVector((BRHash::Rand(X, Y, 414, Seed) - 0.5f) * 2.f * J, (BRHash::Rand(X, Y, 415, Seed) - 0.5f) * 2.f * J, 0.f);
		L.Yaw = BRHash::Rand(X, Y, 416, Seed) < 0.5f ? 0.f : 90.f;
		return L;
	}
	}
}

float ABRWorld::LightLevelAt(const FVector& P) const
{
	const FBRLevelDef& D = Def();
	if (D.bOutdoor && (D.Sky == EBRSky::Day || D.Sky == EBRSky::Overcast))
	{
		return 1.f;
	}
	if (D.Fixture == EBRFixture::None || Power < 0.05f)
	{
		return 0.f;
	}
	const FIntPoint C = WorldToCell(P);
	const int32 R = FMath::Clamp(FMath::CeilToInt(D.LightRadius / D.CellSize), 1, 4);
	float Acc = 0.f;
	for (int32 DX = -R; DX <= R; ++DX)
	{
		for (int32 DY = -R; DY <= R; ++DY)
		{
			const FBRLightInfo L = CellLight(C.X + DX, C.Y + DY);
			if (!L.bHas || L.bBroken)
			{
				continue;
			}
			const FVector LP = CellCenter(FIntPoint(C.X + DX, C.Y + DY)) + L.Offset;
			const float Dist = static_cast<float>(FVector::Dist2D(P, LP));
			if (Dist < D.LightRadius)
			{
				Acc += FMath::Square(1.f - Dist / D.LightRadius) * (L.bFlicker ? 0.6f : 1.f);
			}
		}
	}
	return FMath::Clamp(Acc * Power, 0.f, 1.5f);
}

bool ABRWorld::FindPath(const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutPath, int32 MaxNodes) const
{
	OutPath.Reset();
	if (From == To)
	{
		OutPath.Add(To);
		return true;
	}
	if (!IsWalkable(To))
	{
		return false;
	}

	struct FNode
	{
		FIntPoint P;
		float F;
	};
	auto Less = [](const FNode& A, const FNode& B) { return A.F < B.F; };
	auto Heur = [&To](const FIntPoint& P) { return static_cast<float>(FMath::Abs(P.X - To.X) + FMath::Abs(P.Y - To.Y)); };

	TArray<FNode> Open;
	TMap<FIntPoint, FIntPoint> Came;
	TMap<FIntPoint, float> G;
	Open.HeapPush(FNode{ From, Heur(From) }, Less);
	G.Add(From, 0.f);

	const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	int32 Expanded = 0;
	while (Open.Num() > 0 && Expanded < MaxNodes)
	{
		FNode Cur{ FIntPoint(0, 0), 0.f };
		Open.HeapPop(Cur, Less);
		if (Cur.P == To)
		{
			FIntPoint P = To;
			OutPath.Add(P);
			while (P != From)
			{
				P = Came.FindChecked(P);
				OutPath.Add(P);
			}
			Algo::Reverse(OutPath);
			return true;
		}
		++Expanded;
		const float CurG = G.FindRef(Cur.P);
		for (const FIntPoint& Dir : Dirs)
		{
			const FIntPoint N = Cur.P + Dir;
			if (!CanStep(Cur.P, N))
			{
				continue;
			}
			const float NG = CurG + 1.f;
			if (const float* Old = G.Find(N))
			{
				if (*Old <= NG)
				{
					continue;
				}
			}
			G.Add(N, NG);
			Came.Add(N, Cur.P);
			Open.HeapPush(FNode{ N, NG + Heur(N) }, Less);
		}
	}
	return false;
}
