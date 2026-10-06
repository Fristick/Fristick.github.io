#include "BRGameMode.h"
#include "Backrooms.h"
#include "BRAutoTest.h"
#include "BRCharacter.h"
#include "BRHUD.h"
#include "BRPlayerController.h"
#include "BRWorld.h"

#include "BRLevels.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

ABRGameMode::ABRGameMode()
{
	DefaultPawnClass = ABRCharacter::StaticClass();
	PlayerControllerClass = ABRPlayerController::StaticClass();
	HUDClass = ABRHUD::StaticClass();
}

void ABRGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (UGameplayStatics::HasOption(Options, TEXT("BRLevel")))
	{
		const int32 Level = UGameplayStatics::GetIntOption(Options, TEXT("BRLevel"), 0);
		StartLevelOption = BRLevels::Exists(Level) ? Level : 0;
	}
}

void ABRGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return; // partie complete (4 joueurs) ou refus du moteur
	}
	ErrorMessage = BRLevels::CheckJoinOptions(Options);
	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogBackrooms, Warning, TEXT("Connexion refusee (%s) : %s"), *Address, *ErrorMessage);
	}
}

APawn* ABRGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// Sans PlayerStart, le moteur faisait apparaitre tout le monde a l'origine : le premier joueur l'occupait, et le
	// suivant (un ami qui rejoint la partie) restait sans personnage ("SpawnActor failed because of collision").
	// Chacun a sa place autour du point de depart, comme dans ABRWorld::PlacePlayer.
	UWorld* World = GetWorld();
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!World || !PawnClass)
	{
		return nullptr;
	}
	const ABRWorld* BRWorld = ABRWorld::Get(this);
	const ACharacter* CDO = Cast<ACharacter>(PawnClass->GetDefaultObject());
	const float Half = (CDO && CDO->GetCapsuleComponent()) ? CDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 90.f;
	const FVector Loc = BRWorld ? BRWorld->SpawnSpot(BRWorld->PlayerSlot(NewPlayer ? NewPlayer->PlayerState.Get() : nullptr), Half)
		: FVector(175.f, 175.f, Half + 5.f);
	FActorSpawnParameters Params;
	Params.Instigator = GetInstigator();
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* Pawn = World->SpawnActor<APawn>(PawnClass, FTransform(SpawnTransform.Rotator(), Loc), Params);
	// v4.10 : un joueur qui rejoint la partie prepare puis construit le niveau chez lui : les entites l'ignorent jusqu'a ce
	// qu'il signale etre pret (ABRWorld::SetLocalLoading)
	if (ABRCharacter* C = Cast<ABRCharacter>(Pawn))
	{
		if (NewPlayer && !NewPlayer->IsLocalController())
		{
			C->SetLevelLoading(true);
		}
	}
	return Pawn;
}

void ABRGameMode::StartPlay()
{
	// Le monde est cree avant le BeginPlay des acteurs : le sol existe des la premiere image
	UWorld* World = GetWorld();
	bool bExists = false;
	for (TActorIterator<ABRWorld> It(World); It; ++It)
	{
		bExists = true;
		break;
	}
	if (!bExists && World)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABRWorld* BRWorld = World->SpawnActor<ABRWorld>(ABRWorld::StaticClass(), FTransform::Identity, Params);
		if (BRWorld && StartLevelOption != INDEX_NONE)
		{
			BRWorld->StartLevel = StartLevelOption; // lu dans son BeginPlay, qui suit StartPlay
		}
	}
	// Test automatique (-BRAutoTest) : seulement en solo ; test multijoueur (-BRNetTest) : chez l'hote
	// (le client lance le sien depuis son controleur, voir ABRPlayerController::BeginPlay)
	const bool bSolo = World && World->GetNetMode() == NM_Standalone;
	if (World && ABRAutoTest::IsRequested() && (ABRAutoTest::IsNetTestRequested() ? !bSolo : bSolo))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ABRAutoTest>(ABRAutoTest::StaticClass(), FTransform::Identity, Params);
	}
	Super::StartPlay();
}
