// Hallucinations : ce que la folie montre a UN joueur (silhouette sans visage au coin de l'oeil,
// sourire qui flotte dans le noir). Acteurs locaux, jamais repliques : le coequipier, lui, ne voit rien.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BRHallucinationTypes.h"
#include "BRPhenomena.generated.h"

class ABRCharacter;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class BACKROOMS_API ABRHallucination : public AActor
{
	GENERATED_BODY()

public:
	ABRHallucination();

	void Init(EBRHallucination InForm, ABRCharacter* InViewer);
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Parts;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;

private:
	void Vanish(bool bWhisper);

	TWeakObjectPtr<ABRCharacter> Viewer;
	EBRHallucination Form = EBRHallucination::Shadow;
	float Life = 0.f;
	float MaxLife = 8.f;
	float Stare = 0.f;
};
