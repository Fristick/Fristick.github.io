// Mode de jeu : cree le monde procedural et definit les classes du joueur.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BRGameMode.generated.h"

UCLASS()
class BACKROOMS_API ABRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABRGameMode();

	virtual void StartPlay() override;
};
