#include "BRGameMode.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRHUD.h"
#include "BRPlayerController.h"
#include "BRWorld.h"

#include "BRLevels.h"

#include "Engine/World.h"
#include "EngineUtils.h"
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
	Super::StartPlay();
}
