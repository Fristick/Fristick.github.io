// Entites du Backrooms Wiki : modeles Blender articules (bras, avant-bras, cuisses, tibias, tete),
// animation procedurale, IA a etats (inspiree de Backrooms : Escape Together) et chemins sur la grille.
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

	/** L'entite poursuit le joueur */
	bool IsHunting() const { return State == EState::Chase; }

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Visual;

	/** Squelette articule (le Skin-Stealer peut le cacher sous sa forme de masse) */
	UPROPERTY()
	TObjectPtr<USceneComponent> BodyForm;

	/** Forme "au repos" du Skin-Stealer */
	UPROPERTY()
	TObjectPtr<USceneComponent> MassForm;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> PartComponents;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> Voice;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;

	/** Ballon rouge du Partygoer (reste vertical quoi que fasse le bras) */
	UPROPERTY()
	TObjectPtr<USceneComponent> Balloon;

private:
	enum class EState : uint8 { Idle, Wander, Stalk, Chase, Retreat, Frozen, Hide, Lure };
	enum class ELimb : uint8 { UpperArm, LowerArm, Thigh, Shin, HoundUpper, HoundLower, Wing };

	struct FLimb
	{
		TWeakObjectPtr<USceneComponent> Pivot;
		ELimb Type = ELimb::Thigh;
		float Phase = 0.f;
		float Amp = 30.f;
		float Sign = 1.f;
		FRotator Base = FRotator::ZeroRotator;
	};

	/** Proportions d'un humanoide (cm) - identiques a HUMANOIDS dans Tools/Blender/generate_models.py */
	struct FHumanoidSpec
	{
		float Hip = 92.f;
		float Shoulder = 145.f;
		float ShoulderW = 19.f;
		float HipW = 10.f;
		float Hunch = 0.f;
		float UpperArm = 30.f;
		float LowerArm = 42.f;
		float Thigh = 46.f;
		float Neck = 6.f;
	};

	/** Ce que l'entite percoit du joueur cette image */
	struct FSense
	{
		float Dist = 1e9f;
		bool bLOS = false;
		bool bLookedAt = false;
		bool bHeard = false;
	};

	// Construction du modele
	void BuildVisual();
	USceneComponent* AddPart(FName MeshName, USceneComponent* Parent, const FVector& Joint, const FVector& FallbackSize,
		float FallbackDrop, const TMap<FString, FLinearColor>* Tints, bool bUniqueGlow = false, float GlowScale = 1.f);
	void BuildHumanoid(const TCHAR* Prefix, const FHumanoidSpec& Spec, const TMap<FString, FLinearColor>* Tints, USceneComponent* Parent);
	void BuildHound(const TMap<FString, FLinearColor>* Tints);
	void AddLimb(USceneComponent* Pivot, ELimb Type, float Phase, float Amp, float Sign, const FRotator& Base);
	static FHumanoidSpec SpecFor(EBREntityKind InKind);

	// IA
	void Think(float Dt);
	void ThinkSmiler(ABRWorld* W, ABRCharacter* P, const FSense& S, float Dt);
	void ThinkHound(ABRCharacter* P, const FSense& S, float Dt);
	void ThinkFaceling(ABRCharacter* P, const FSense& S, float Dt);
	void ThinkSkinStealer(ABRCharacter* P, const FSense& S, float Dt);
	void ThinkPartygoer(ABRWorld* W, ABRCharacter* P, const FSense& S, float Dt);
	void ThinkBacteria(ABRWorld* W, ABRCharacter* P, const FSense& S, float Dt);
	void ThinkSimpleHunter(ABRCharacter* P, const FSense& S, float Dt, float ChaseIntensity, float GiveUpTime);
	void ThinkDeathmoth(ABRCharacter* P, const FSense& S, float Dt);

	void Animate(float Dt);
	void AnimateLimbs(float Dt, float Gait);
	void UpdateHead(float Dt);
	void UpdateMorph(float Dt);
	void SetState(EState NewState);
	void MoveTowards(const FVector& Target, float Speed);
	void FollowPathTo(const FVector& Goal, float Speed, float Dt);
	void Wander(float Speed, float Dt);
	void FacePlayer(float Dt);
	void SetOrientToMovement(bool bOrient);
	bool HasLineOfSight(const ABRCharacter* P) const;
	bool IsLookedAtBy(const ABRCharacter* P, float CosAngle) const;
	bool IsDirectPathClear(const FVector& Goal) const;
	void TryAttack(ABRCharacter* P, float Dist);
	void PlayVoice(float Volume = 1.f);
	void PlaySound2D(FName Sound, float Volume);
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
	float EyeContact = 0.f;
	float BeamTime = 0.f;
	float AnimTime = 0.f;
	float Life = 0.f;
	float Vanish = -1.f;
	float Morph = 1.f;          // Skin-Stealer : 0 = masse, 1 = forme humaine
	float MorphTarget = 1.f;
	float Reach = 0.f;          // bras tendus vers le joueur
	float HideCrouch = 0.f;         // Faceling cache dans les bles
	float TwitchTimer = 0.f;
	FRotator Twitch = FRotator::ZeroRotator;
	FRotator HeadRot = FRotator::ZeroRotator;
	FVector LastKnown = FVector::ZeroVector;
	bool bHostileVariant = true;
	bool bSeenOnce = false;
	bool bWantsMove = false;
	bool bWarned = false;
	TArray<FLimb> Limbs;
	TWeakObjectPtr<USceneComponent> HeadPivot;
	FVector VisualBase = FVector::ZeroVector;
	TWeakObjectPtr<ABRWorld> World;
};
