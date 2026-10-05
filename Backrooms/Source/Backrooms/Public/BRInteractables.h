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

/** Objet d'inventaire pose au sol (eau d'amande, piles, cassettes VHS, notes, equipement...) */
UCLASS()
class BACKROOMS_API ABRPickup : public AActor
{
	GENERATED_BODY()

public:
	ABRPickup();

	void Init(EBRItem InItem, uint64 InId, const FString& InNote);
	virtual void Tick(float DeltaSeconds) override;
	FString GetPrompt() const;
	void Collect(ABRCharacter* By);

	EBRItem Item = EBRItem::AlmondWater;
	uint64 Id = 0;
	/** v4.8 : identifiant de la note (Note.L0.3) : chaque joueur la lit dans sa langue */
	FString NoteText;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Petite lueur pour reperer les cassettes VHS dans le noir */
	UPROPERTY()
	TObjectPtr<UPointLightComponent> Glint;

	float Time = 0.f;
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

	/** v4.3 : echelle sur laquelle on grimpe vraiment ; dans le conduit au-dessus de la trappe du plafond, on noclippe.
	 *  CeilingZ : hauteur du plafond au-dessus du pied de l'echelle ; ShaftHeight : conduit au-dessus (0 : aucun) */
	void InitLadder(float CeilingZ, float ShaftHeight);
	bool IsClimbable() const { return ClimbTopZ > 0.f; }
	/** Ou se tient le grimpeur (centre de sa capsule, en XY) */
	FVector GetClimbAnchor() const;
	/** Hauteur (monde) du centre de la capsule a partir de laquelle on noclippe */
	float GetClimbTopZ() const { return ClimbTopZ; }
	/** Sommet atteint : noclip vers le niveau cible */
	void FinishClimb(ABRCharacter* By);

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

	/** Segments d'echelle supplementaires, dechirure lumineuse en haut du conduit */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> LadderParts;

	/** Quitte le niveau (transition vers Target) */
	void Leave();

	bool bUsed = false;
	float Time = 0.f;
	float LastDenied = -100.f;
	float ClimbTopZ = 0.f;
};
