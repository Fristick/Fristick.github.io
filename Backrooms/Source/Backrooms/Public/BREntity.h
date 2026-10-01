// Entites du Backrooms Wiki : modeles Blender en plusieurs pieces, animation procedurale,
// IA a etats et recherche de chemin sur la grille du niveau.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BRTypes.h"
#include "BREntity.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class ABRCharacter;
class ABRWorld;

/** Fiche descriptive + parametres de comportement d'une espece */
struct FBREntityInfo
{
	FString Name;
	FString Number;
	FString Description;
	FString Advice;
	float HalfHeight = 90.f;
	float Radius = 30.f;
	bool bFlying = false;
	float HoverHeight = 150.f;
	bool bNeedsDark = false;
	float WalkSpeed = 150.f;
	float ChaseSpeed = 400.f;
	float SightRange = 2000.f;
	float AttackRange = 120.f;
	float Damage = 25.f;
	float SanityDamage = 10.f;
	float AttackCooldown = 1.5f;
	float Aura = 0.3f;          // perte de sante mentale / s a proximite
	float AuraRadius = 700.f;
	FName Voice = NAME_None;
	float VoiceInterval = 8.f;
	float VoiceFalloff = 2500.f;
};

UCLASS()
class BACKROOMS_API ABREntity : public ACharacter
{
	GENERATED_BODY()

public:
	ABREntity();

	static const FBREntityInfo& Info(EBREntityKind InKind);
	const FBREntityInfo& MyInfo() const { return Info(Kind); }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** A definir avant FinishSpawning */
	EBREntityKind Kind = EBREntityKind::Smiler;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Visual;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> PartComponents;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> Voice;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;

private:
	enum class EState : uint8 { Idle, Wander, Stalk, Chase, Retreat, Frozen };

	struct FLimb
	{
		TWeakObjectPtr<USceneComponent> Pivot;
		float Phase = 0.f;
		float Amp = 30.f;
		bool bRoll = false;
		float Sign = 1.f;
	};

	// Construction du modele
	void BuildVisual();
	USceneComponent* AddPart(FName MeshName, const FVector& Joint, const FVector& Scale, const FVector& FallbackSize,
		float FallbackDrop, const TMap<FString, FLinearColor>* Tints, bool bUniqueGlow = false, float GlowScale = 1.f);
	void BuildHumanoid(const TCHAR* Prefix, float Hip, float Shoulder, float ShoulderW, float HipW, float Hunch,
		const TMap<FString, FLinearColor>* Tints, float ArmLen, float LegLen);

	// IA
	void Think(float Dt);
	void Animate(float Dt);
	void SetState(EState NewState);
	void MoveTowards(const FVector& Target, float Speed);
	void FollowPathTo(const FVector& Goal, float Speed, float Dt);
	void Wander(float Speed, float Dt);
	void FacePlayer(float Dt);
	bool HasLineOfSight(const ABRCharacter* P) const;
	bool IsLookedAtBy(const ABRCharacter* P, float CosAngle) const;
	bool IsDirectPathClear(const FVector& Goal) const;
	void TryAttack(ABRCharacter* P, float Dist);
	void PlayVoice(float Volume = 1.f);
	void StartVanish();

	EState State = EState::Wander;
	float StateTime = 0.f;
	float RepathTimer = 0.f;
	TArray<FIntPoint> Path;
	int32 PathIndex = 0;
	FIntPoint PathGoal = FIntPoint(MAX_int32, MAX_int32);
	float AttackTimer = 0.f;
	float VoiceTimer = 3.f;
	float StuckTimer = 0.f;
	float Agitation = 0.f;
	float Intimidation = 0.f;
	float LostSight = 0.f;
	float AnimTime = 0.f;
	float Life = 0.f;
	float Vanish = -1.f;
	bool bHostileVariant = true;
	bool bSeenOnce = false;
	bool bWantsMove = false;
	TArray<FLimb> Limbs;
	FVector VisualBase = FVector::ZeroVector;
	TWeakObjectPtr<ABRWorld> World;
};
