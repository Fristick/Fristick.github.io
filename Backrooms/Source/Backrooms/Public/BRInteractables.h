// Objets interactifs : objets a ramasser et sorties vers d'autres niveaux.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BRTypes.h"
#include "BRInteractables.generated.h"

class USphereComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class ABRCharacter;

/** Eau d'amande, piles, notes */
UCLASS()
class BACKROOMS_API ABRPickup : public AActor
{
	GENERATED_BODY()

public:
	ABRPickup();

	void Init(EBRPickupType InType, uint64 InId, const FString& InNote);
	FString GetPrompt() const;
	void Collect(ABRCharacter* By);

	EBRPickupType Type = EBRPickupType::AlmondWater;
	uint64 Id = 0;
	FString NoteText;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};

/** Passage vers un autre niveau */
UCLASS()
class BACKROOMS_API ABRExit : public AActor
{
	GENERATED_BODY()

public:
	ABRExit();

	void Init(int32 InTarget, EBRExitStyle InStyle);
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	/** Necessite la touche E (portes...) ; sinon il suffit de toucher (noclip) */
	bool IsInteractable() const;
	FString GetPrompt() const;
	void Use(ABRCharacter* By);

	int32 Target = 1;
	EBRExitStyle Style = EBRExitStyle::Door;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> Audio;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GlitchMID;

	bool bUsed = false;
	float Time = 0.f;
};
