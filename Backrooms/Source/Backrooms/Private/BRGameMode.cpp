#include "BRGameMode.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRHUD.h"
#include "BRPlayerController.h"
#include "BRWorld.h"

#include "Engine/World.h"
#include "EngineUtils.h"

ABRGameMode::ABRGameMode()
{
	DefaultPawnClass = ABRCharacter::StaticClass();
	PlayerControllerClass = ABRPlayerController::StaticClass();
	HUDClass = ABRHUD::StaticClass();
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
		World->SpawnActor<ABRWorld>(ABRWorld::StaticClass(), FTransform::Identity, Params);
	}
	Super::StartPlay();
}
