#include "BRWorld.h"
#include "BRSave.h"
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
#include "HAL/PlatformTime.h"
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

	// v4.7 : l'hote reprend une partie en ligne : la carte vient d'etre rechargee, la session attend son niveau
	const FBRSessionState& Resume = BRSaves::PendingResume();
	LoadLevelNow(StartLevel, (Resume.bValid && Resume.Level == StartLevel) ? Resume.Seed : 0u);
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

void ABRWorld::RequestTransition(int32 TargetLevel, bool bFromDeath, uint32 InSeed)
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
	PendingSeed = InSeed;
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

void ABRWorld::HandlePlayerDeath(EBRDeathCause Cause)
{
	// Seul : retour au Niveau 0. En equipe : a terre, un coequipier peut nous relever pendant un delai
	// propre a la cause (blessure 30 s, noyade 20 s), sinon on se reveille au point de depart du niveau.
	// v4.7 : la cause est explicite. Au fond d'une fosse, personne ne peut nous relever.
	DeathTimer = IsNetGame()
		? ((HasLivingTeammate() && BRDeath::CanRevive(Cause)) ? BRDeath::ReviveWindow(Cause) : 6.f)
		: 4.5f;
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
	// v4.7 : le joueur n'est pas encore place dans ce niveau. Sans cela, la construction synchrone ci-dessous se faisait
	// autour de sa position dans le niveau precedent, et le sol du point d'arrivee venait quelques images plus tard.
	bPlayerPlaced = false;
	Current = &BRLevels::Get(LevelNumber);
	Seed = InSeed != 0 ? InSeed : (static_cast<uint32>(FMath::Rand()) * 2654435761u ^ static_cast<uint32>(LevelNumber * 7919 + 17));
	// v4.5 : -BRSeed=<n> : meme disposition a chaque lancement (captures avant / apres comparables, tests automatiques)
	uint32 FixedSeed = 0;
	if (InSeed == 0 && FParse::Value(FCommandLine::Get(), TEXT("BRSeed="), FixedSeed) && FixedSeed != 0)
	{
		Seed = SeedFromUser(FixedSeed, LevelNumber);
	}
	Seed = Seed != 0 ? Seed : 1u;
	// v4.6 : salles de fosses et cellules atteignables du niveau fini (avant la construction des chunks)
	PrepareLevelLayout();
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
		// v4.7 : reprise d'une partie : meme graine, donc meme disposition. Objectifs et objets ramasses sont remis
		// avant la construction des chunks : un objet deja ramasse n'est jamais cree. Le point de reprise est applique
		// au placement du joueur, une fois le sol et les collisions construits.
		bResumed = false;
		bHasResumeSpot = false;
		FBRSessionState& Resume = BRSaves::PendingResume();
		if (Resume.bValid)
		{
			if (Resume.Level == Current->Number && Resume.Seed == Seed)
			{
				VHSFound = Resume.VHSFound;
				bBlackoutRecorded = Resume.bBlackoutRecorded;
				bEntityRecorded = Resume.bEntityRecorded;
				for (const uint64 Id : Resume.Collected)
				{
					Collected.Add(Id);
					NetCollected.AddUnique(Id);
				}
				bHasResumeSpot = Resume.bHasSpot;
				ResumeSpot = Resume.Spot;
				ResumeYaw = Resume.Yaw;
				bResumed = true;
				UE_LOG(LogBackrooms, Log, TEXT("Reprise : Niveau %d, graine %u, %d cassette(s), %d objet(s) deja ramasse(s), point de reprise %s"),
					Current->Number, Seed, VHSFound, Resume.Collected.Num(), bHasResumeSpot ? *ResumeSpot.ToString() : TEXT("aucun"));
			}
			else
			{
				UE_LOG(LogBackrooms, Warning, TEXT("Reprise ignoree : session du Niveau %d (graine %u), niveau charge %d (graine %u)"), Resume.Level,
					Resume.Seed, Current->Number, Seed);
			}
			Resume = FBRSessionState();
		}
		// v4.7 : nouveau niveau, tout le groupe repart debout (etat tenu par le serveur, chaque machine suit)
		TArray<ABRCharacter*> All;
		GetPlayers(All);
		for (ABRCharacter* C : All)
		{
			if (C && C->GetDeathState().bDead)
			{
				C->ServerApplyDeathState(false, EBRDeathCause::None, 3);
			}
		}
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
	// Sauvegarde de la partie en cours : ce niveau est desormais explore
	if (ABRPlayerController* PC = LocalPC())
	{
		PC->OnLevelLoaded(Current->Number);
	}
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
	// Diffusion vers l'avant : halos autour des lampes quand on les regarde, rayons dans la poussiere
	Fog->SetVolumetricFogScatteringDistribution(0.35f);
	{
		// Albedo teinte par la couleur du brouillard (eclaircie : c'est la lumiere des lampes qui colore le volume)
		const float M = FMath::Max3(D.FogColor.R, D.FogColor.G, D.FogColor.B);
		const FLinearColor Tint = M > 0.01f ? D.FogColor / M : FLinearColor::White;
		const FLinearColor Albedo = FLinearColor::LerpUsingHSV(FLinearColor::White, Tint, 0.35f);
		Fog->SetVolumetricFogAlbedo(Albedo.ToFColor(true));
	}

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

	// Etalonnage : ombres et hautes lumieres legerement teintees (split toning)
	S.bOverride_ColorGainShadows = true;
	S.ColorGainShadows = FVector4(D.ShadowTint.R, D.ShadowTint.G, D.ShadowTint.B, 1.f);
	S.bOverride_ColorGainHighlights = true;
	S.ColorGainHighlights = FVector4(D.HighlightTint.R, D.HighlightTint.G, D.HighlightTint.B, 1.f);
	// Exposition locale : les neons ne brulent plus l'image, les recoins sombres gardent du detail
	S.bOverride_LocalExposureHighlightContrastScale = true;
	S.LocalExposureHighlightContrastScale = 0.8f;
	S.bOverride_LocalExposureShadowContrastScale = true;
	S.LocalExposureShadowContrastScale = 0.9f;
	S.bOverride_LocalExposureDetailStrength = true;
	S.LocalExposureDetailStrength = 1.12f;

	// v4.7 : salles de fosses : sans brouillard volumetrique, le brouillard ordinaire voilait le fond d'environ 11 %
	// de sa couleur ; ce qui est sous le sol s'assombrit avec la profondeur (M_BR_PitShade). Le haut des parois,
	// eclaire par la salle, reste visible ; le fond redevient noir. Seulement dans un niveau a fosses.
	S.WeightedBlendables.Array.RemoveAll([this](const FWeightedBlendable& B) { return PitShadeMID && B.Object == PitShadeMID; });
	if (HasPits() && A)
	{
		if (!PitShadeMID)
		{
			PitShadeMID = A->NewPitShade(this);
		}
		if (PitShadeMID)
		{
			PitShadeMID->SetScalarParameterValue(TEXT("PitFloorZ"), 0.f);
			PitShadeMID->SetScalarParameterValue(TEXT("PitShadeStart"), D.PitLipThickness + 30.f);
			PitShadeMID->SetScalarParameterValue(TEXT("PitShadeRange"), FMath::Min(600.f, D.PitDepth * 0.5f));
			PitShadeMID->SetScalarParameterValue(TEXT("PitShadeFloor"), 0.04f);
			PitShadeMID->SetScalarParameterValue(TEXT("PitShadeAmount"), 1.f);
			S.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, PitShadeMID));
		}
	}

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
	if (IsOverPit(Loc, 60.f))
	{
		// v4.6 : ne peut pas arriver (pas de salle de fosses dans les chunks du depart) : garde-fou
		UE_LOG(LogBackrooms, Error, TEXT("Point d'apparition au-dessus d'une fosse (%s) : repli au centre du depart"), *Loc.ToString());
		Loc = CellCenter(FIntPoint(0, 0), Half + 5.f);
	}
	Loc.Z += FloorZAt(Loc); // estrade du point de depart (Niveau 37)
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
	// v4.7 : reprise d'une partie (joueur de l'hote) : au point sauvegarde s'il est encore sur, sinon au depart
	bool bResumePlaced = false;
	if (bHasResumeSpot && HasAuthority() && P->IsLocallyControlled())
	{
		bHasResumeSpot = false;
		const float Radius = P->GetCapsuleComponent() ? P->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
		FVector Safe;
		if (FindSafeResumeSpot(ResumeSpot, Half, Radius, Safe))
		{
			P->SetActorLocation(Safe, false, nullptr, ETeleportType::TeleportPhysics);
			Yaw = ResumeYaw;
			bResumePlaced = true;
			UE_LOG(LogBackrooms, Log, TEXT("Reprise : joueur replace en %s"), *Safe.ToString());
		}
		else
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Reprise : point %s invalide (mur, fosse, sol absent) : point de depart"), *ResumeSpot.ToString());
		}
	}
	if (!bServerPlaced && !bResumePlaced)
	{
		const int32 Slot = IsNetGame() ? PlayerSlot(P->GetPlayerState()) : 0;
		const FVector Loc = SpawnSpot(Slot, Half);
		P->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogBackrooms, Log, TEXT("Joueur place au point de depart (place %d) : %s"), Slot, *Loc.ToString());
	}
	if (AController* C = P->GetController())
	{
		C->SetControlRotation(FRotator(0.f, Yaw, 0.f));
		if (ABRPlayerController* PC = Cast<ABRPlayerController>(C))
		{
			PC->ResetMenuDrift();
		}
	}
	P->OnEnteredLevel(Def());
	bPlayerPlaced = true;
}

void ABRWorld::RestoreJournal(const TArray<int32>& InDiscovered, const TArray<int32>& InVisited)
{
	Discovered.Empty();
	for (const int32 K : InDiscovered)
	{
		Discovered.Add(K);
	}
	Visited.Reset();
	if (Current)
	{
		Visited.Add(Current->Number);
	}
	for (const int32 L : InVisited)
	{
		Visited.AddUnique(L);
	}
}

TArray<uint64> ABRWorld::GetCollectedList() const
{
	TArray<uint64> Out = Collected.Array();
	for (const uint64 Id : NetCollected)
	{
		Out.AddUnique(Id);
	}
	return Out;
}

bool ABRWorld::IsSafeSaveSpot(const ABRCharacter* P) const
{
	if (!P || P->IsDead() || !bLevelReady || IsTransitioning() || P->IsClimbing() || P->IsSwimming())
	{
		return false;
	}
	const UCharacterMovementComponent* Move = P->GetCharacterMovement();
	if (!Move || !Move->IsMovingOnGround())
	{
		return false;
	}
	const FVector L = P->GetActorLocation();
	if (HasPits() && IsOverPit(L, 90.f))
	{
		return false;
	}
	const FIntPoint Cell = WorldToCell(L);
	return IsCellInBounds(Cell.X, Cell.Y) && IsWalkable(Cell) && (!HasPits() || IsReachable(Cell));
}

bool ABRWorld::FindSafeResumeSpot(const FVector& Wanted, float Half, float Radius, FVector& Out) const
{
	UWorld* World = GetWorld();
	const FIntPoint Cell = WorldToCell(Wanted);
	if (!World || !IsCellInBounds(Cell.X, Cell.Y) || !IsWalkable(Cell) || !IsChunkLoaded(CellToChunk(Cell)))
	{
		return false;
	}
	if (HasPits() && (IsOverPit(Wanted, Radius + 40.f) || !IsReachable(Cell)))
	{
		return false;
	}
	// Le sol doit exister vraiment (chunk construit, collisions comprises) juste sous le point
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRResumeSpot), false);
	if (const ABRCharacter* P = GetPlayer())
	{
		Q.AddIgnoredActor(P);
	}
	FHitResult Hit;
	const FVector From(Wanted.X, Wanted.Y, Wanted.Z + 60.f);
	const FVector To(Wanted.X, Wanted.Y, Wanted.Z - Half - 250.f);
	if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q) || Hit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}
	const FVector Loc = Hit.ImpactPoint + FVector(0.f, 0.f, Half + 3.f);
	// Capsule degagee (pas dans un mur ni un meuble apparu depuis)
	if (World->OverlapBlockingTestByChannel(Loc, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(Radius, Half), Q))
	{
		return false;
	}
	Out = Loc;
	return true;
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
				const uint32 ForcedSeed = PendingSeed;
				PendingSeed = 0;
				LoadLevelNow(PendingLevel, ForcedSeed);
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
			UpdatePitFalls();
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
	// v4.7 : reprise : le sol autour du point de reprise est construit avant d'y poser le joueur
	Centers.Add((P && bPlayerPlaced) ? P->GetActorLocation() : (bHasResumeSpot ? ResumeSpot : CellCenter(FIntPoint(0, 0))));
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
				if (!Chunks.Contains(C) && !Wanted.Contains(C) && IsChunkInBounds(C) && ChunkDist(C) <= D.ViewDistance + ChunkWorld * 0.75f)
				{
					Wanted.Add(C);
				}
			}
		}
	}
	Wanted.Sort([&](const FIntPoint& A, const FIntPoint& B) { return ChunkDist(A) < ChunkDist(B); });

	// v4.5 : au plus 2 chunks par image, et pas de second chunk si le premier a deja pris plus de 5 ms (saccades)
	int32 Budget = bSynchronous ? MAX_int32 : 2;
	const double BuildStart = FPlatformTime::Seconds();
	for (const FIntPoint& C : Wanted)
	{
		if (Budget-- <= 0 || (!bSynchronous && (FPlatformTime::Seconds() - BuildStart) * 1000.0 > 5.0))
		{
			break;
		}
		const double T0 = FPlatformTime::Seconds();
		SpawnChunk(C);
		const float Ms = static_cast<float>((FPlatformTime::Seconds() - T0) * 1000.0);
		LastChunkBuildMs = Ms;
		MaxChunkBuildMs = FMath::Max(MaxChunkBuildMs, Ms);
		++ChunksBuilt;
		ChunkBuildMsTotal += Ms;
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
		else if (bAuth && D.BlackoutSmilers > 0)
		{
			// Tant que dure le noir, d'autres Smilers surgissent (parfois sous les yeux du joueur)
			BlackoutSpawnTimer -= Dt;
			if (BlackoutSpawnTimer <= 0.f)
			{
				BlackoutSpawnTimer = FMath::FRandRange(6.f, 9.f);
				int32 Alive = 0;
				for (const TWeakObjectPtr<ABREntity>& Weak : BlackoutEntities)
				{
					Alive += Weak.IsValid() ? 1 : 0;
				}
				if (Alive < D.BlackoutSmilers + 3)
				{
					if (const ABRCharacter* P = RandomLivingPlayer())
					{
						SpawnBlackoutSmiler(P, FMath::FRand() < 0.5f);
					}
				}
			}
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
				ABRHUD::Notify(this, TEXT("Filmez pendant la coupure : gardez le cam\u00e9scope sur vous et regardez autour de vous."), 5.f, FLinearColor(1.f, 0.85f, 0.4f));
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

bool ABRWorld::IsChunkInBounds(const FIntPoint& Chunk) const
{
	const int32 B = Def().BoundsChunks;
	return B <= 0 || (Chunk.X >= -B && Chunk.X < B && Chunk.Y >= -B && Chunk.Y < B);
}

bool ABRWorld::IsCellInBounds(int32 X, int32 Y) const
{
	return Def().BoundsChunks <= 0 || IsChunkInBounds(CellToChunk(FIntPoint(X, Y)));
}

namespace
{
	/** Chunks du point de depart (cellules -1..1) : ni sortie, ni cassette garantie */
	bool IsSpawnChunk(const ABRWorld* W, const FIntPoint& Chunk)
	{
		const FIntPoint Lo = W->CellToChunk(FIntPoint(-1, -1));
		const FIntPoint Hi = W->CellToChunk(FIntPoint(1, 1));
		return Chunk.X >= Lo.X && Chunk.X <= Hi.X && Chunk.Y >= Lo.Y && Chunk.Y <= Hi.Y;
	}
}

int32 ABRWorld::BoundedChunkCount(bool bAvoidSpawn) const
{
	const int32 B = Def().BoundsChunks;
	int32 Count = 0;
	for (int32 X = -B; X < B; ++X)
	{
		for (int32 Y = -B; Y < B; ++Y)
		{
			Count += (bAvoidSpawn && IsSpawnChunk(this, FIntPoint(X, Y))) ? 0 : 1;
		}
	}
	return Count;
}

bool ABRWorld::IsChunkPicked(const FIntPoint& Chunk, int32 Salt, int32 Count, bool bAvoidSpawn) const
{
	const int32 B = Def().BoundsChunks;
	if (B <= 0 || Count <= 0 || !IsChunkInBounds(Chunk) || (bAvoidSpawn && IsSpawnChunk(this, Chunk)))
	{
		return false;
	}
	// Rang du chunk parmi ceux de l'enceinte, trie par hachage : les Count premiers sont tires
	const uint32 Mine = BRHash::Hash(Chunk.X, Chunk.Y, Salt, Seed);
	int32 Before = 0;
	for (int32 X = -B; X < B; ++X)
	{
		for (int32 Y = -B; Y < B; ++Y)
		{
			const FIntPoint C(X, Y);
			if (C == Chunk || (bAvoidSpawn && IsSpawnChunk(this, C)))
			{
				continue;
			}
			const uint32 K = BRHash::Hash(X, Y, Salt, Seed);
			if (K < Mine || (K == Mine && (X < Chunk.X || (X == Chunk.X && Y < Chunk.Y))))
			{
				++Before;
			}
		}
	}
	return Before < Count;
}

// ---------------------------------------------------------------------------------------------------------------------
// v4.6 : salles de fosses ("Hole Variation" du Niveau 0)
//
// Une salle carree de K x K cellules, inscrite dans un chunk avec une galerie d'au moins une cellule tout autour, percee
// d'une fosse carree a chaque coin interieur de cellule (sauf quelques-uns, tires) : les fosses forment une grille
// (K-1) x (K-1) et les passages qui les separent passent par le centre des cellules, la ou la grille A* fait marcher les
// entites. Tout se deduit de la graine et des coordonnees : chaque joueur construit les memes fosses, quel que soit
// l'ordre dans lequel ses chunks se chargent.
// ---------------------------------------------------------------------------------------------------------------------

uint32 ABRWorld::SeedFromUser(uint32 N, int32 Level)
{
	return BRHash::Mix(N ^ static_cast<uint32>(Level * 7919 + 17));
}

bool ABRWorld::HasPits() const
{
	const FBRLevelDef& D = Def();
	return D.PitRoomChance > 0.f && D.PoolChance <= 0.f && (D.Layout == EBRLayout::Rooms || D.Layout == EBRLayout::Maze);
}

bool ABRWorld::ComputePitRoom(const FIntPoint& Chunk, FIntRect& OutRoom) const
{
	const FBRLevelDef& D = Def();
	if (!HasPits() || !IsChunkInBounds(Chunk) || IsSpawnChunk(this, Chunk))
	{
		return false;
	}
	bool bPicked = false;
	if (D.BoundsChunks > 0)
	{
		const int32 Count = FMath::Max(D.PitRoomsMin, FMath::RoundToInt(D.PitRoomChance * static_cast<float>(BoundedChunkCount(true))));
		bPicked = IsChunkPicked(Chunk, 1950, Count, true);
	}
	else
	{
		bPicked = BRHash::Rand(Chunk.X, Chunk.Y, 1950, Seed) < D.PitRoomChance;
	}
	if (!bPicked)
	{
		return false;
	}
	const int32 N = D.ChunkCells;
	const int32 K = FMath::Clamp(D.PitRoomCells, 3, N - 2);
	// Marge de chaque cote : une cellule au moins (la galerie), le reste tire
	const int32 Free = N - K - 2;
	const int32 OX = 1 + (Free > 0 ? static_cast<int32>(BRHash::Hash(Chunk.X, Chunk.Y, 1951, Seed) % static_cast<uint32>(Free + 1)) : 0);
	const int32 OY = 1 + (Free > 0 ? static_cast<int32>(BRHash::Hash(Chunk.X, Chunk.Y, 1952, Seed) % static_cast<uint32>(Free + 1)) : 0);
	OutRoom.Min = FIntPoint(Chunk.X * N + OX, Chunk.Y * N + OY);
	OutRoom.Max = OutRoom.Min + FIntPoint(K, K);
	return true;
}

bool ABRWorld::GetPitRoom(const FIntPoint& Chunk, FIntRect& OutRoom) const
{
	if (!Current || !HasPits())
	{
		return false;
	}
	if (Def().BoundsChunks > 0)
	{
		// Niveau fini : liste calculee au chargement (le tirage parmi tous les chunks coute trop cher pour chaque arete)
		if (const FIntRect* R = BoundedPitRooms.Find(Chunk))
		{
			OutRoom = *R;
			return true;
		}
		return false;
	}
	return ComputePitRoom(Chunk, OutRoom);
}

bool ABRWorld::IsPitRoomCell(int32 X, int32 Y) const
{
	FIntRect R;
	return GetPitRoom(CellToChunk(FIntPoint(X, Y)), R) && X >= R.Min.X && X < R.Max.X && Y >= R.Min.Y && Y < R.Max.Y;
}

bool ABRWorld::HasPitAtCorner(int32 X, int32 Y) const
{
	FIntRect R;
	// Coin interieur : les quatre cellules qui le touchent sont dans la salle
	if (!GetPitRoom(CellToChunk(FIntPoint(X, Y)), R) || X < R.Min.X || X + 1 >= R.Max.X || Y < R.Min.Y || Y + 1 >= R.Max.Y)
	{
		return false;
	}
	return BRHash::Rand(X, Y, 1953, Seed) < Def().PitHoleChance;
}

float ABRWorld::GetPitHoleSize() const
{
	const FBRLevelDef& D = Def();
	const float Passage = FMath::Max(120.f, D.PitPassage);
	return FMath::Clamp(D.PitHoleSize, 40.f, D.CellSize - Passage);
}

bool ABRWorld::IsOverPit(const FVector& P, float Margin) const
{
	if (!Current || !HasPits())
	{
		return false;
	}
	const float S = CellSize();
	const float Half = GetPitHoleSize() * 0.5f;
	// Coin de la grille le plus proche (les fosses sont centrees sur les coins ; la marge reste sous la demi-cellule)
	const int32 GX = FMath::RoundToInt(static_cast<float>(P.X) / S);
	const int32 GY = FMath::RoundToInt(static_cast<float>(P.Y) / S);
	const float R = FMath::Min(Half + FMath::Max(0.f, Margin), S * 0.5f);
	if (FMath::Abs(static_cast<float>(P.X) - GX * S) >= R || FMath::Abs(static_cast<float>(P.Y) - GY * S) >= R)
	{
		return false;
	}
	return HasPitAtCorner(GX - 1, GY - 1);
}

bool ABRWorld::SegmentCrossesPit(const FVector& A, const FVector& B, float Margin) const
{
	if (!Current || !HasPits())
	{
		return false;
	}
	const float S = CellSize();
	const float R = FMath::Min(GetPitHoleSize() * 0.5f + FMath::Max(0.f, Margin), S * 0.5f);
	const FVector2D P0(A.X, A.Y);
	const FVector2D D(B.X - A.X, B.Y - A.Y);
	// Coins dont la fosse elargie peut toucher la boite englobante du segment
	const int32 GX0 = FMath::FloorToInt((FMath::Min(A.X, B.X) - R) / S);
	const int32 GX1 = FMath::CeilToInt((FMath::Max(A.X, B.X) + R) / S);
	const int32 GY0 = FMath::FloorToInt((FMath::Min(A.Y, B.Y) - R) / S);
	const int32 GY1 = FMath::CeilToInt((FMath::Max(A.Y, B.Y) + R) / S);
	for (int32 GX = GX0; GX <= GX1; ++GX)
	{
		for (int32 GY = GY0; GY <= GY1; ++GY)
		{
			if (!HasPitAtCorner(GX - 1, GY - 1))
			{
				continue;
			}
			// Segment contre carre (methode des dalles)
			const FVector2D Lo(GX * S - R, GY * S - R);
			const FVector2D Hi(GX * S + R, GY * S + R);
			float T0 = 0.f;
			float T1 = 1.f;
			bool bHit = true;
			for (int32 Axis = 0; Axis < 2 && bHit; ++Axis)
			{
				const float O = Axis == 0 ? static_cast<float>(P0.X) : static_cast<float>(P0.Y);
				const float Dir = Axis == 0 ? static_cast<float>(D.X) : static_cast<float>(D.Y);
				const float L = Axis == 0 ? static_cast<float>(Lo.X) : static_cast<float>(Lo.Y);
				const float H = Axis == 0 ? static_cast<float>(Hi.X) : static_cast<float>(Hi.Y);
				if (FMath::Abs(Dir) < 1e-4f)
				{
					bHit = O > L && O < H;
					continue;
				}
				float TA = (L - O) / Dir;
				float TB = (H - O) / Dir;
				if (TA > TB)
				{
					Swap(TA, TB);
				}
				T0 = FMath::Max(T0, TA);
				T1 = FMath::Min(T1, TB);
				bHit = T0 < T1;
			}
			if (bHit)
			{
				return true;
			}
		}
	}
	return false;
}

EBREdge ABRWorld::PitEdge(int32 X, int32 Y, bool bEast, bool& bOut) const
{
	bOut = false;
	if (!Current || !HasPits())
	{
		return EBREdge::Open;
	}
	const FIntPoint A(X, Y);
	const FIntPoint B = bEast ? FIntPoint(X + 1, Y) : FIntPoint(X, Y + 1);
	const FIntPoint Chunk = CellToChunk(A);
	FIntRect R;
	if (Chunk != CellToChunk(B) || !GetPitRoom(Chunk, R))
	{
		return EBREdge::Open; // bord du chunk : regle ordinaire
	}
	bOut = true;
	auto In = [&R](const FIntPoint& C) { return C.X >= R.Min.X && C.X < R.Max.X && C.Y >= R.Min.Y && C.Y < R.Max.Y; };
	const bool bInA = In(A);
	const bool bInB = In(B);
	if (bInA == bInB)
	{
		return EBREdge::Open; // dans la salle, ou dans la galerie qui la contourne
	}
	// Pourtour de la salle : un mur, perce de PitDoorsPerSide portes par cote (positions tirees)
	const int32 K = R.Max.X - R.Min.X;
	int32 Side = 0;
	int32 Along = 0;
	if (bEast)
	{
		Side = bInA ? 0 : 1; // 0 : cote +X, 1 : cote -X
		Along = Y - R.Min.Y;
	}
	else
	{
		Side = bInA ? 2 : 3; // 2 : cote +Y, 3 : cote -Y
		Along = X - R.Min.X;
	}
	for (int32 d = 0; d < FMath::Max(1, Def().PitDoorsPerSide); ++d)
	{
		const int32 Pos = static_cast<int32>(BRHash::Hash(R.Min.X, R.Min.Y, 1960 + Side * 8 + d, Seed) % static_cast<uint32>(FMath::Max(1, K)));
		if (Pos == Along)
		{
			return EBREdge::Door;
		}
	}
	return EBREdge::Wall;
}

void ABRWorld::GetPitRooms(TArray<FIntRect>& Out, const FIntPoint& AroundChunk, int32 Radius) const
{
	Out.Reset();
	if (!Current || !HasPits())
	{
		return;
	}
	if (Def().BoundsChunks > 0)
	{
		BoundedPitRooms.GenerateValueArray(Out);
		return;
	}
	for (int32 DX = -Radius; DX <= Radius; ++DX)
	{
		for (int32 DY = -Radius; DY <= Radius; ++DY)
		{
			FIntRect R;
			if (ComputePitRoom(AroundChunk + FIntPoint(DX, DY), R))
			{
				Out.Add(R);
			}
		}
	}
}

bool ABRWorld::FindPitRoomView(const FVector& From, FVector& OutLoc, FRotator& OutRot) const
{
	TArray<FIntRect> Rooms;
	GetPitRooms(Rooms, CellToChunk(WorldToCell(From)), 6);
	const float S = CellSize();
	float Best = TNumericLimits<float>::Max();
	for (const FIntRect& R : Rooms)
	{
		// Les quatre coins de la salle : on se place au fond de la cellule d'angle, regard en diagonale sur les fosses
		for (int32 c = 0; c < 4; ++c)
		{
			const bool bHiX = (c & 1) != 0;
			const bool bHiY = (c & 2) != 0;
			const FIntPoint Cell(bHiX ? R.Max.X - 1 : R.Min.X, bHiY ? R.Max.Y - 1 : R.Min.Y);
			const FVector Out(bHiX ? 1.f : -1.f, bHiY ? 1.f : -1.f, 0.f);
			const FVector Loc = CellCenter(Cell, 0.f) + Out * (S * 0.5f - 95.f);
			const float Dist = static_cast<float>(FVector::DistSquared2D(Loc, From));
			if (Dist < Best)
			{
				Best = Dist;
				OutLoc = Loc;
				OutRot = FRotator(-24.f, FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Out.Y), static_cast<float>(-Out.X))), 0.f);
			}
		}
	}
	return Best < TNumericLimits<float>::Max();
}

bool ABRWorld::IsSafelyReachable(const FIntPoint& C) const
{
	return Def().BoundsChunks <= 0 || SafeReach.Num() == 0 || SafeReach.Contains(C);
}

bool ABRWorld::IsReachable(const FIntPoint& C) const
{
	return Def().BoundsChunks <= 0 || FullReach.Num() == 0 || FullReach.Contains(C);
}

void ABRWorld::PrepareLevelLayout()
{
	BoundedPitRooms.Reset();
	SafeReach.Reset();
	FullReach.Reset();
	ForcedDoors.Reset();
	const FBRLevelDef& D = Def();
	if (D.BoundsChunks <= 0)
	{
		return;
	}
	// 1. Salles de fosses du niveau fini (tirage global, le meme chez tous les joueurs)
	if (HasPits())
	{
		for (int32 X = -D.BoundsChunks; X < D.BoundsChunks; ++X)
		{
			for (int32 Y = -D.BoundsChunks; Y < D.BoundsChunks; ++Y)
			{
				FIntRect R;
				if (ComputePitRoom(FIntPoint(X, Y), R))
				{
					BoundedPitRooms.Add(FIntPoint(X, Y), R);
				}
			}
		}
	}
	// 2. Toute cellule hors des salles de fosses doit etre atteignable depuis le depart SANS les traverser (objectifs et
	//    sorties n'iront que la : il y a toujours un chemin de contournement). Les murs tires au hasard enfermaient
	//    parfois le depart (21 cellules sur 1024 avec -BRSeed=1 en v4.5) : tant qu'une zone reste isolee, on perce une porte
	//    dans le mur qui la separe de la zone atteinte (le mur candidat de plus petit hachage : le meme chez tous)
	const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	TArray<FIntPoint> Queue;
	auto Grow = [&](bool bAvoidPits, TSet<FIntPoint>& Out)
	{
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const FIntPoint Cur = Queue[Head];
			for (const FIntPoint& Dir : Dirs)
			{
				const FIntPoint N = Cur + Dir;
				if (Out.Contains(N) || !IsCellInBounds(N.X, N.Y) || !CanStep(Cur, N) || (bAvoidPits && IsPitRoomCell(N.X, N.Y)))
				{
					continue;
				}
				Out.Add(N);
				Queue.Add(N);
			}
		}
		Queue.Reset();
	};
	SafeReach.Add(FIntPoint(0, 0));
	Queue.Add(FIntPoint(0, 0));
	Grow(true, SafeReach);
	for (int32 Guard = 0; Guard < 512; ++Guard)
	{
		bool bFound = false;
		uint32 BestH = 0;
		FIntPoint BestKey(0, 0);
		FIntPoint BestCell(0, 0);
		for (const FIntPoint& C : SafeReach)
		{
			for (const FIntPoint& Dir : Dirs)
			{
				const FIntPoint N = C + Dir;
				if (!IsCellInBounds(N.X, N.Y) || SafeReach.Contains(N) || IsPitRoomCell(N.X, N.Y))
				{
					continue;
				}
				const bool bEast = Dir.Y == 0;
				const FIntPoint A = (Dir.X > 0 || Dir.Y > 0) ? C : N;
				const FIntPoint Key(A.X * 2 + (bEast ? 1 : 0), A.Y);
				const uint32 H = BRHash::Hash(Key.X, Key.Y, 1990, Seed);
				if (!bFound || H < BestH || (H == BestH && (Key.X < BestKey.X || (Key.X == BestKey.X && Key.Y < BestKey.Y))))
				{
					bFound = true;
					BestH = H;
					BestKey = Key;
					BestCell = N;
				}
			}
		}
		if (!bFound)
		{
			break;
		}
		ForcedDoors.Add(BestKey);
		SafeReach.Add(BestCell);
		Queue.Add(BestCell);
		Grow(true, SafeReach);
	}
	// Puis en passant par les passages entre les fosses : les salles elles-memes doivent etre accessibles
	FullReach.Add(FIntPoint(0, 0));
	Queue.Add(FIntPoint(0, 0));
	Grow(false, FullReach);
	int32 Total = 0;
	for (int32 X = -D.BoundsChunks * D.ChunkCells; X < D.BoundsChunks * D.ChunkCells; ++X)
	{
		for (int32 Y = -D.BoundsChunks * D.ChunkCells; Y < D.BoundsChunks * D.ChunkCells; ++Y)
		{
			Total += IsWalkable(FIntPoint(X, Y)) ? 1 : 0;
		}
	}
	FString Rooms;
	for (const TPair<FIntPoint, FIntRect>& P : BoundedPitRooms)
	{
		const bool bOk = FullReach.Contains(P.Value.Min);
		Rooms += FString::Printf(TEXT(" [cellules %d,%d a %d,%d%s]"), P.Value.Min.X, P.Value.Min.Y, P.Value.Max.X - 1, P.Value.Max.Y - 1,
			bOk ? TEXT("") : TEXT(" INACCESSIBLE"));
		if (!bOk)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Salle de fosses inaccessible depuis le depart (chunk %d,%d)"), P.Key.X, P.Key.Y);
		}
	}
	UE_LOG(LogBackrooms, Log, TEXT("Disposition : %d cellules, %d atteignables sans les fosses, %d avec ; %d porte(s) percee(s) pour relier les zones isolees ; %d salle(s) de fosses%s"),
		Total, SafeReach.Num(), FullReach.Num(), ForcedDoors.Num(), BoundedPitRooms.Num(), *Rooms);
}

void ABRWorld::UpdatePitFalls()
{
	if (!HasAuthority() || !Current || !HasPits() || !bLevelReady)
	{
		return;
	}
	const FBRLevelDef& D = Def();
	TArray<ABRCharacter*> Players;
	GetPlayers(Players);
	for (ABRCharacter* P : Players)
	{
		if (!IsValid(P) || P->IsDead() || P->IsDevFlying())
		{
			continue;
		}
		// Position telle que le serveur la connait (celle que le client lui envoie) : sous le bord d'une fosse, ou plus bas
		// que tout fond possible
		const FVector L = P->GetActorLocation();
		const bool bInPit = L.Z < -D.PitKillDepth && IsOverPit(L, 60.f);
		const bool bLost = L.Z < -(D.PitDepth + 600.f);
		if (bInPit || bLost)
		{
			P->NotifyFellIntoPit();
		}
	}
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
	if (IsPoolCell(C.X, C.Y))
	{
		return -Def().PoolDepth;
	}
	if (IsOverPit(P))
	{
		return -Def().PitDepth; // v4.6 : fond de la fosse
	}
	return Def().DeckHeight > 0.f ? DeckZAt(P) : 0.f;
}

bool ABRWorld::HasDeck(int32 X, int32 Y, int32 Side) const
{
	const FBRLevelDef& D = Def();
	if (D.DeckHeight <= 0.f || Side < 0 || Side > 3 || IsSolid(X, Y) || IsPoolCell(X, Y))
	{
		return false;
	}
	const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	const FIntPoint Dir = Dirs[Side];
	const int32 NX = X + Dir.X;
	const int32 NY = Y + Dir.Y;
	const bool bAlongY = Dir.X != 0; // le mur s'etend le long de Y
	const EBREdge E = bAlongY ? EdgeE(Dir.X > 0 ? X : NX, Y) : EdgeN(X, Dir.Y > 0 ? Y : NY);
	if (E == EBREdge::Open && !IsSolid(NX, NY))
	{
		return false;
	}
	if (E == EBREdge::Door)
	{
		// Une porte : trottoir des deux cotes (le seuil fait pont) ou d'aucun, jamais un quai qui tombe dans l'eau
		if (IsPoolCell(NX, NY) || IsSolid(NX, NY))
		{
			return false;
		}
		const int32 KX = bAlongY ? FMath::Min(X, NX) : X;
		const int32 KY = bAlongY ? Y : FMath::Min(Y, NY);
		return BRHash::Rand(KX, KY, bAlongY ? 1803 : 1804, Seed) < D.DeckChance;
	}
	// Mur : decide par troncons de 3 cellules de chaque cote de la ligne, pour de longs trottoirs continus
	const int32 Line = bAlongY ? (Dir.X > 0 ? X + 1 : X) : (Dir.Y > 0 ? Y + 1 : Y);
	const int32 SideOfLine = (bAlongY ? Dir.X : Dir.Y) > 0 ? 0 : 1;
	const int32 Run = BRHash::FloorDiv(bAlongY ? Y : X, 3);
	return BRHash::Rand(Line * 2 + SideOfLine, Run, bAlongY ? 1801 : 1802, Seed) < D.DeckChance;
}

float ABRWorld::DeckZAt(const FVector& P) const
{
	const FBRLevelDef& D = Def();
	if (D.DeckHeight <= 0.f)
	{
		return 0.f;
	}
	const FIntPoint C = WorldToCell(P);
	if (IsSolid(C.X, C.Y) || IsPoolCell(C.X, C.Y))
	{
		return 0.f;
	}
	const float S = D.CellSize;
	const float LX = static_cast<float>(P.X) - C.X * S;
	const float LY = static_cast<float>(P.Y) - C.Y * S;
	const float StepW = D.DeckStep > 0.f ? D.DeckStepWidth : 0.f;
	// Point de depart : une estrade seche au milieu de l'eau, bordee d'une marche
	if (C == FIntPoint(0, 0))
	{
		const float Edge = FMath::Min(FMath::Min(LX, S - LX), FMath::Min(LY, S - LY));
		return Edge < StepW ? D.DeckStep : D.DeckHeight;
	}
	const float Reach = D.WallThickness * 0.5f + D.DeckWidth;
	const float Dist[4] = { S - LX, LX, S - LY, LY };
	float Z = 0.f;
	for (int32 k = 0; k < 4; ++k)
	{
		if (Dist[k] >= Reach + StepW || !HasDeck(C.X, C.Y, k))
		{
			continue;
		}
		Z = FMath::Max(Z, Dist[k] < Reach ? D.DeckHeight : D.DeckStep);
	}
	return Z;
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
		if (!Info.bFlying && IsPitRoomCell(Cell.X, Cell.Y))
		{
			continue; // v4.6 : une entite terrestre n'apparait pas entre les fosses
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
	// v4.3 : le premier de chaque joueur surgit dans son champ de vision (ses yeux et son sourire s'allument dans le noir),
	// les autres tout autour, puis d'autres arrivent tant que dure la coupure (UpdateBlackout)
	const int32 Count = D.BlackoutSmilers + FMath::Min(Players.Num() - 1, 2);
	for (int32 i = 0; i < Count; ++i)
	{
		SpawnBlackoutSmiler(Players[i % Players.Num()], i < Players.Num());
	}
	BlackoutSpawnTimer = FMath::FRandRange(6.f, 9.f);
}

bool ABRWorld::SpawnBlackoutSmiler(const ABRCharacter* P, bool bInView)
{
	FVector Loc;
	const bool bFound = (bInView && FindBlackoutSpot(P, Loc)) || FindSpawnSpot(EBREntityKind::Smiler, P, 700.f, 1700.f, false, Loc);
	if (!bFound)
	{
		return false;
	}
	ABREntity* E = SpawnEntity(EBREntityKind::Smiler, Loc);
	if (E)
	{
		BlackoutEntities.Add(E);
	}
	return E != nullptr;
}

bool ABRWorld::FindBlackoutSpot(const ABRCharacter* P, FVector& Out) const
{
	if (!P || !GetWorld())
	{
		return false;
	}
	const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Smiler);
	const FVector Eye = P->GetEyeLocation();
	const FVector View = P->GetViewDirection().GetSafeNormal2D();
	const float ViewYaw = FMath::Atan2(static_cast<float>(View.Y), static_cast<float>(View.X));
	for (int32 Attempt = 0; Attempt < 48; ++Attempt)
	{
		const float Ang = ViewYaw + FMath::FRandRange(-0.9f, 0.9f);
		const float Dist = FMath::FRandRange(500.f, 1400.f);
		const FIntPoint Cell = WorldToCell(P->GetActorLocation() + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Dist);
		if (!IsWalkable(Cell) || IsSpawnArea(Cell.X, Cell.Y) || IsPoolCell(Cell.X, Cell.Y) || !IsChunkLoaded(CellToChunk(Cell)))
		{
			continue;
		}
		const FVector Loc = CellCenter(Cell, Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f);
		if (FVector::Dist2D(Loc, Eye) < 450.f)
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRBlackoutSpot), false, P);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Loc, ECC_Visibility, Q))
		{
			continue; // un mur cache ce point
		}
		Out = Loc;
		return true;
	}
	return false;
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

int32 ABRWorld::CubicleRole(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	constexpr int32 Zone = 12;
	if (D.CubicleZoneChance <= 0.f || D.Layout != EBRLayout::Rooms
		|| BRHash::Rand(BRHash::FloorDiv(X, Zone), BRHash::FloorDiv(Y, Zone), 1901, Seed) >= D.CubicleZoneChance)
	{
		return -1;
	}
	// Rangees de bureaux dos a dos (portes vers les allees), une allee toutes les 3 rangees, une transversale toutes les 6 colonnes
	const int32 LX = BRHash::PosMod(X, Zone);
	const int32 LY = BRHash::PosMod(Y, Zone);
	if (LX % 6 == 0 || LY % 3 == 0)
	{
		return 0;
	}
	return LY % 3 == 1 ? 1 : 2;
}

EBREdge ABRWorld::CubicleEdge(int32 X, int32 Y, bool bEast, bool& bOut) const
{
	const int32 A = CubicleRole(X, Y);
	const int32 B = bEast ? CubicleRole(X + 1, Y) : CubicleRole(X, Y + 1);
	bOut = A >= 0 || B >= 0;
	if (!bOut)
	{
		return EBREdge::Open;
	}
	if (bEast)
	{
		// Bureaux voisins dans une rangee : cloison ; une allee : passage
		return (A == 0 || B == 0) ? EBREdge::Open : EBREdge::Wall;
	}
	if (A == 2 || B == 1)
	{
		return EBREdge::Door; // porte du bureau sur l'allee
	}
	if (A == 1 && B == 2)
	{
		return EBREdge::Wall; // bureaux dos a dos
	}
	return (A == 0 || B == 0) ? EBREdge::Open : EBREdge::Wall;
}

bool ABRWorld::IsCubicle(int32 X, int32 Y, int32* OutDoorSide) const
{
	const int32 R = CubicleRole(X, Y);
	if (R <= 0 || IsSpawnArea(X, Y))
	{
		return false;
	}
	if (OutDoorSide)
	{
		*OutDoorSide = R == 1 ? 3 : 2;
	}
	return true;
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
	if (D.BoundsChunks > 0)
	{
		// Niveau fini : mur d'enceinte entre l'interieur et l'exterieur, rien au-dela
		const bool bIn = IsCellInBounds(X, Y);
		if (bIn != IsCellInBounds(X + 1, Y))
		{
			return EBREdge::Wall;
		}
		if (!bIn)
		{
			return EBREdge::Open;
		}
	}
	if (IsSpawnArea(X, Y) && IsSpawnArea(X + 1, Y))
	{
		return EBREdge::Open;
	}
	bool bPit = false;
	const EBREdge PE = PitEdge(X, Y, true, bPit);
	if (bPit)
	{
		return PE;
	}
	if (ForcedDoors.Num() > 0 && ForcedDoors.Contains(FIntPoint(X * 2 + 1, Y)))
	{
		return EBREdge::Door; // v4.6 : zone isolee reliee au depart
	}
	if (D.Layout == EBRLayout::Rooms)
	{
		bool bCubicles = false;
		const EBREdge CE = CubicleEdge(X, Y, true, bCubicles);
		if (bCubicles)
		{
			return CE;
		}
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
	if (D.BoundsChunks > 0)
	{
		const bool bIn = IsCellInBounds(X, Y);
		if (bIn != IsCellInBounds(X, Y + 1))
		{
			return EBREdge::Wall;
		}
		if (!bIn)
		{
			return EBREdge::Open;
		}
	}
	if (IsSpawnArea(X, Y) && IsSpawnArea(X, Y + 1))
	{
		return EBREdge::Open;
	}
	bool bPit = false;
	const EBREdge PE = PitEdge(X, Y, false, bPit);
	if (bPit)
	{
		return PE;
	}
	if (ForcedDoors.Num() > 0 && ForcedDoors.Contains(FIntPoint(X * 2, Y)))
	{
		return EBREdge::Door; // v4.6 : zone isolee reliee au depart
	}
	if (D.Layout == EBRLayout::Rooms)
	{
		bool bCubicles = false;
		const EBREdge CE = CubicleEdge(X, Y, false, bCubicles);
		if (bCubicles)
		{
			return CE;
		}
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
	if (HasPits())
	{
		// v4.6 : pas de pilier dans une salle de fosses ni sur son pourtour (le coin (+X,+Y) touche une cellule de la salle)
		if (IsPitRoomCell(X, Y) || IsPitRoomCell(X + 1, Y) || IsPitRoomCell(X, Y + 1) || IsPitRoomCell(X + 1, Y + 1))
		{
			return false;
		}
	}
	const bool bOpenZone = ZoneDensity(X, Y) < 0.5f;
	if (D.bPillarGrid && bOpenZone)
	{
		// Grandes salles inondees : une colonnade reguliere, comme dans la scene de reference
		return BRHash::PosMod(X, 2) == 1 && BRHash::PosMod(Y, 2) == 1;
	}
	const float Mult = bOpenZone ? 2.5f : 1.f;
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
		FIntRect Room;
		if (HasPits() && GetPitRoom(CellToChunk(FIntPoint(X, Y)), Room) && X >= Room.Min.X && X < Room.Max.X && Y >= Room.Min.Y && Y < Room.Max.Y)
		{
			// v4.6 : salle de fosses : neons reguliers une cellule sur deux, au-dessus des croisements des passages (centres
			// des cellules), jamais au-dessus du vide ; ombres portees pour que les bords des fosses decoupent la lumiere et
			// qu'elle n'eclaire pas les parois a travers la dalle
			if (BRHash::PosMod(X - Room.Min.X, 2) != 0 || BRHash::PosMod(Y - Room.Min.Y, 2) != 0)
			{
				return L;
			}
			L.bHas = true;
			L.bBroken = BRHash::Rand(X, Y, 1955, Seed) < 0.12f;
			L.bFlicker = !L.bBroken && BRHash::Rand(X, Y, 1956, Seed) < 0.18f;
			L.bShadow = true;
			L.Offset = FVector::ZeroVector;
			L.Yaw = BRHash::PosMod(X + Y, 4) == 0 ? 90.f : 0.f;
			return L;
		}
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
	const bool bPits = HasPits();
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
			// v4.6 : traverser une salle de fosses coute plus cher : les entites prennent la galerie quand elle n'est pas
			// beaucoup plus longue (dans la salle, elles suivent les passages, au centre des cellules)
			const float NG = CurG + ((bPits && IsPitRoomCell(N.X, N.Y)) ? 3.f : 1.f);
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
