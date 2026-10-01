// Le joueur : vue a la premiere personne, lampe torche, endurance, sante mentale.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BRTypes.h"
#include "BRCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UAudioComponent;

UCLASS()
class BACKROOMS_API ABRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABRCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// ---- Entrees (appelees par le PlayerController) ----
	void InputMove(const FVector2D& Value);
	void InputLook(const FVector2D& DeltaDegrees);
	void InputJump(bool bPressed);
	void SetSprinting(bool bInSprint);
	void ToggleCrouch();
	void ToggleFlashlight();
	void Interact();
	void DrinkAlmondWater();
	void ReplaceBattery();
	void SetInputLocked(bool bLocked) { bInputLocked = bLocked; }
	bool IsInputLocked() const { return bInputLocked; }

	// ---- Etat ----
	float Health = 100.f;
	float Sanity = 100.f;
	float Stamina = 100.f;
	float Battery = 100.f;
	int32 AlmondWater = 1;
	int32 Batteries = 1;
	int32 NotesRead = 0;
	bool bGodMode = false;

	bool IsDead() const { return bDead; }
	bool IsSprinting() const;
	bool IsFlashlightOn() const { return bFlashlightOn && Battery > 0.f; }
	float GetNoiseRadius() const;
	FVector GetEyeLocation() const;
	FVector GetViewDirection() const;
	const FString& GetKilledBy() const { return KilledBy; }
	float GetDeathTime() const { return DeathTime; }
	float GetDamageFlash() const { return DamageFlash; }
	float GetChaseLevel() const { return ChaseLevel; }

	/** Attaque d'une entite (degats de sante et de sante mentale) */
	void ReceiveAttack(float Damage, float SanityDamage, AActor* Source, const FString& SourceName);
	/** Pression mentale continue (entite proche, obscurite...) en points par seconde */
	void AddSanityPressure(float PointsPerSecond) { SanityPressure += PointsPerSecond; }
	/** Une entite poursuit le joueur (musique de poursuite) */
	void NotifyChase(float Intensity) { ChaseTarget = FMath::Max(ChaseTarget, Intensity); }

	void ReceivePickup(EBRPickupType Type, const FString& Note);
	void OnEnteredLevel(const FBRLevelDef& Def);
	void ResetStats();

	// ---- HUD ----
	const FString& GetFocusPrompt() const { return FocusPrompt; }
	bool IsReadingNote() const { return bReadingNote; }
	const FString& GetOpenNote() const { return OpenNote; }
	void CloseNote() { bReadingNote = false; }

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpotLightComponent> Flashlight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FlashlightMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> HeartAudio;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> BreathAudio;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> ChaseAudio;

private:
	void UpdateStats(float Dt);
	void UpdateCamera(float Dt);
	void UpdateFocus();
	void UpdateFlashlight(float Dt);
	void UpdateAudio(float Dt);
	void UpdatePostProcess(float Dt);
	void PlayFootstep();
	void Die(const FString& By, AActor* Killer);
	void SetupLoopAudio(UAudioComponent* Comp, FName Sound);

	bool bDead = false;
	bool bInputLocked = true;
	bool bFlashlightOn = false;
	bool bWantsSprint = false;
	bool bExhausted = false;
	bool bReadingNote = false;
	float CamZ = 74.f;
	float BobTime = 0.f;
	int32 LastStepPhase = 0;
	float DamageFlash = 0.f;
	float LastDamageTime = -100.f;
	float DeathTime = 0.f;
	float SanityPressure = 0.f;
	float ChaseLevel = 0.f;
	float ChaseTarget = 0.f;
	float FlashFlicker = 1.f;
	float SprintTime = 0.f;
	float TimeAlive = 0.f;
	FVector2D LookLag = FVector2D::ZeroVector;
	EBRStep StepType = EBRStep::Carpet;
	FString FocusPrompt;
	TWeakObjectPtr<AActor> FocusActor;
	FString OpenNote;
	FString KilledBy;
	TWeakObjectPtr<AActor> KillerActor;
};
