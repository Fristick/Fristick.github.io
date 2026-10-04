// Mode de jeu : cree le monde procedural et definit les classes du joueur.
// Multijoueur : l'hote ouvre la carte avec "?listen?BRLevel=N" ; le monde (et son niveau) est replique aux clients.
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

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	/** La carte n'a pas de PlayerStart : chaque joueur apparait autour du point de depart du niveau */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

private:
	/** Niveau de depart demande par l'hote (option BRLevel) */
	int32 StartLevelOption = INDEX_NONE;
};
