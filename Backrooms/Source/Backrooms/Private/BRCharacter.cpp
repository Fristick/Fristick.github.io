#include "BRCharacter.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRWorld.h"
#include "BRHUD.h"
#include "BRInteractables.h"
#include "BRPlayerController.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float WalkSpeed = 260.f;
	constexpr float SprintSpeed = 470.f;
	constexpr float CrouchSpeed = 140.f;
	constexpr float StandEyeZ = 74.f;
	constexpr float CrouchEyeZ = 42.f;
	constexpr float FlashCandelas = 600.f;
	constexpr float BatteryDrain = 0.42f; // % par seconde (~4 min)
}

ABRCharacter::ABRCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->JumpZVelocity = 360.f;
	Move->AirControl = 0.15f;
	Move->BrakingDecelerationWalking = 1800.f;
	Move->GroundFriction = 7.f;
	Move->MaxStepHeight = 35.f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->bCanWalkOffLedgesWhenCrouching = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, StandEyeZ));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(88.f);

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(Camera);
	Flashlight->SetRelativeLocation(FVector(25.f, 14.f, -14.f));
	Flashlight->SetIntensityUnits(ELightUnits::Candelas);
	Flashlight->SetIntensity(FlashCandelas);
	Flashlight->SetInnerConeAngle(13.f);
	Flashlight->SetOuterConeAngle(30.f);
	Flashlight->SetAttenuationRadius(2400.f);
	Flashlight->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
	Flashlight->SetCastShadows(true);
	Flashlight->SetSourceRadius(2.f);
	Flashlight->SetVolumetricScatteringIntensity(0.6f);
	Flashlight->SetVisibility(false);

	FlashlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightMesh"));
	FlashlightMesh->SetupAttachment(Camera);
	FlashlightMesh->SetRelativeLocation(FVector(30.f, 16.f, -19.f));
	FlashlightMesh->SetRelativeRotation(FRotator(2.f, -4.f, 0.f));
	FlashlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlashlightMesh->SetCastShadow(false);

	HeartAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("HeartAudio"));
	HeartAudio->SetupAttachment(RootComponent);
	HeartAudio->bAutoActivate = false;

	BreathAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BreathAudio"));
	BreathAudio->SetupAttachment(RootComponent);
	BreathAudio->bAutoActivate = false;

	ChaseAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ChaseAudio"));
	ChaseAudio->SetupAttachment(RootComponent);
	ChaseAudio->bAutoActivate = false;
}

void ABRCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Immobile tant que le monde ne nous a pas place sur un sol
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		if (W->IsChunkLoaded(FIntPoint(0, 0)))
		{
			bInputLocked = W->IsTransitioning();
		}
	}

	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (UStaticMesh* M = A->Mesh(TEXT("SM_Flashlight")))
		{
			FlashlightMesh->SetStaticMesh(M);
			A->ApplySlots(FlashlightMesh, nullptr, true, nullptr, 0.05f);
		}
		else if (A->Cylinder())
		{
			FlashlightMesh->SetStaticMesh(A->Cylinder());
			FlashlightMesh->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.16f));
			FlashlightMesh->SetRelativeRotation(FRotator(-88.f, 0.f, 0.f));
			FlashlightMesh->SetMaterial(0, A->SlotMaterial(TEXT("DarkMetal")));
		}
		SetupLoopAudio(HeartAudio, TEXT("S_Heartbeat"));
		SetupLoopAudio(BreathAudio, TEXT("S_Breath"));
		SetupLoopAudio(ChaseAudio, TEXT("S_Chase"));
	}
	FlashlightMesh->SetVisibility(false);
}

void ABRCharacter::SetupLoopAudio(UAudioComponent* Comp, FName SoundName)
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!Comp || !A)
	{
		return;
	}
	if (USoundBase* S = A->Sound(SoundName))
	{
		Comp->SetSound(S);
		Comp->SetVolumeMultiplier(0.001f);
		Comp->Play();
	}
}

// =====================================================================================================================
// Entrees
// =====================================================================================================================

void ABRCharacter::InputMove(const FVector2D& Value)
{
	if (bInputLocked || bDead)
	{
		return;
	}
	AddMovementInput(GetActorForwardVector(), Value.Y);
	AddMovementInput(GetActorRightVector(), Value.X);
}

void ABRCharacter::InputLook(const FVector2D& DeltaDegrees)
{
	if (bInputLocked || bDead || !Controller)
	{
		return;
	}
	FRotator R = Controller->GetControlRotation();
	R.Yaw += DeltaDegrees.X;
	R.Pitch = FMath::ClampAngle(static_cast<float>(R.Pitch + DeltaDegrees.Y), -85.f, 85.f);
	R.Roll = 0.f;
	Controller->SetControlRotation(R);
	LookLag += DeltaDegrees;
}

void ABRCharacter::InputJump(bool bPressed)
{
	if (bInputLocked || bDead)
	{
		return;
	}
	if (bPressed)
	{
		if (bIsCrouched)
		{
			UnCrouch();
		}
		else if (Stamina > 10.f && CanJump())
		{
			Stamina -= 8.f;
			Jump();
		}
	}
	else
	{
		StopJumping();
	}
}

void ABRCharacter::SetSprinting(bool bInSprint)
{
	bWantsSprint = bInSprint;
	if (bInSprint && bIsCrouched)
	{
		UnCrouch();
	}
}

void ABRCharacter::ToggleCrouch()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ABRCharacter::ToggleFlashlight()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	if (!bFlashlightOn && Battery <= 0.f)
	{
		ABRHUD::Notify(this, Batteries > 0 ? TEXT("Lampe vide : [R] pour changer les piles") : TEXT("Lampe vide... il faut trouver des piles."),
			3.f, FLinearColor(1.f, 0.8f, 0.4f));
		return;
	}
	bFlashlightOn = !bFlashlightOn;
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Flashlight")))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.7f);
		}
	}
}

void ABRCharacter::Interact()
{
	if (bDead)
	{
		return;
	}
	if (bReadingNote)
	{
		bReadingNote = false;
		return;
	}
	if (bInputLocked)
	{
		return;
	}
	AActor* Target = FocusActor.Get();
	if (ABRPickup* P = Cast<ABRPickup>(Target))
	{
		P->Collect(this);
	}
	else if (ABRExit* E = Cast<ABRExit>(Target))
	{
		E->Use(this);
	}
}

void ABRCharacter::DrinkAlmondWater()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	if (AlmondWater <= 0)
	{
		ABRHUD::Notify(this, TEXT("Plus d'eau d'amande."), 2.5f, FLinearColor(1.f, 0.7f, 0.5f));
		return;
	}
	if (Sanity >= 99.f && Health >= 99.f)
	{
		ABRHUD::Notify(this, TEXT("Vous n'en avez pas besoin pour l'instant."), 2.5f);
		return;
	}
	--AlmondWater;
	Sanity = FMath::Min(100.f, Sanity + 40.f);
	Health = FMath::Min(100.f, Health + 15.f);
	ABRHUD::Notify(this, TEXT("Vous buvez de l'eau d'amande. Votre esprit s'\u00e9claircit."), 3.f, FLinearColor(0.85f, 0.95f, 1.f));
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Drink")))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.9f);
		}
	}
}

void ABRCharacter::ReplaceBattery()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	if (Batteries <= 0)
	{
		ABRHUD::Notify(this, TEXT("Aucune pile de rechange."), 2.5f, FLinearColor(1.f, 0.7f, 0.5f));
		return;
	}
	if (Battery > 90.f)
	{
		ABRHUD::Notify(this, TEXT("Les piles sont encore pleines."), 2.f);
		return;
	}
	--Batteries;
	Battery = 100.f;
	ABRHUD::Notify(this, TEXT("Piles remplac\u00e9es."), 2.f);
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Battery")))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.8f);
		}
	}
}

// =====================================================================================================================
// Etat
// =====================================================================================================================

bool ABRCharacter::IsSprinting() const
{
	return bWantsSprint && !bExhausted && !bIsCrouched && GetVelocity().Size2D() > WalkSpeed * 0.8f;
}

float ABRCharacter::GetNoiseRadius() const
{
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	if (Speed < 20.f)
	{
		return 80.f;
	}
	if (bIsCrouched)
	{
		return 180.f;
	}
	return IsSprinting() ? 1900.f : 700.f;
}

FVector ABRCharacter::GetEyeLocation() const
{
	return Camera ? Camera->GetComponentLocation() : GetActorLocation();
}

FVector ABRCharacter::GetViewDirection() const
{
	return Camera ? Camera->GetForwardVector() : GetActorForwardVector();
}

void ABRCharacter::ReceiveAttack(float Damage, float SanityDamage, AActor* Source, const FString& SourceName)
{
	if (bDead || bGodMode)
	{
		return;
	}
	Health -= Damage;
	Sanity = FMath::Max(0.f, Sanity - SanityDamage);
	DamageFlash = 1.f;
	LastDamageTime = TimeAlive;
	if (Controller)
	{
		FRotator R = Controller->GetControlRotation();
		R.Pitch += FMath::FRandRange(2.f, 5.f);
		R.Yaw += FMath::FRandRange(-4.f, 4.f);
		Controller->SetControlRotation(R);
	}
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Hurt")))
		{
			UGameplayStatics::PlaySound2D(this, S, 1.f);
		}
	}
	if (Health <= 0.f)
	{
		Die(SourceName, Source);
	}
}

void ABRCharacter::Die(const FString& By, AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	Health = 0.f;
	DeathTime = 0.f;
	KilledBy = By;
	KillerActor = Killer;
	bReadingNote = false;
	GetCharacterMovement()->DisableMovement();
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Death")))
		{
			UGameplayStatics::PlaySound2D(this, S, 1.f);
		}
	}
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->HandlePlayerDeath();
	}
}

void ABRCharacter::ResetStats()
{
	Health = 100.f;
	Sanity = 100.f;
	Stamina = 100.f;
	Battery = 100.f;
	AlmondWater = 1;
	Batteries = 1;
	bDead = false;
	bExhausted = false;
	bFlashlightOn = false;
	DamageFlash = 0.f;
	ChaseLevel = ChaseTarget = 0.f;
	KilledBy.Empty();
	KillerActor.Reset();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (bIsCrouched)
	{
		UnCrouch();
	}
}

void ABRCharacter::ReceivePickup(EBRPickupType Type, const FString& Note)
{
	switch (Type)
	{
	case EBRPickupType::AlmondWater:
		++AlmondWater;
		ABRHUD::Notify(this, FString::Printf(TEXT("+1 eau d'amande  (%d)   [B] boire"), AlmondWater), 3.f, FLinearColor(0.9f, 0.85f, 0.7f));
		break;
	case EBRPickupType::Battery:
		++Batteries;
		ABRHUD::Notify(this, FString::Printf(TEXT("+1 jeu de piles  (%d)   [R] recharger"), Batteries), 3.f, FLinearColor(0.9f, 0.9f, 0.6f));
		break;
	case EBRPickupType::Note:
		++NotesRead;
		OpenNote = Note.IsEmpty() ? FString(TEXT("(La note est illisible, l'encre a coul\u00e9.)")) : Note;
		bReadingNote = true;
		break;
	}
}

void ABRCharacter::OnEnteredLevel(const FBRLevelDef& Def)
{
	StepType = Def.Step;
	ChaseLevel = ChaseTarget = 0.f;
	bReadingNote = false;
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		if (!bDead)
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}
	if (Def.Fixture == EBRFixture::None && !Def.bOutdoor && !bFlashlightOn)
	{
		ABRHUD::Notify(this, TEXT("Il fait noir comme dans un four. [F] Lampe torche"), 5.f, FLinearColor(1.f, 0.85f, 0.6f));
	}
}

void ABRCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CamZ += ScaledHalfHeightAdjust;
}

void ABRCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CamZ -= ScaledHalfHeightAdjust;
}

// =====================================================================================================================
// Tick
// =====================================================================================================================

void ABRCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	TimeAlive += Dt;

	UpdateStats(Dt);
	UpdateCamera(Dt);
	UpdateFlashlight(Dt);
	UpdateFocus();
	UpdateAudio(Dt);
	UpdatePostProcess(Dt);

	SanityPressure = 0.f;
	ChaseTarget = 0.f;
}

void ABRCharacter::UpdateStats(float Dt)
{
	if (bDead)
	{
		DeathTime += Dt;
		// La camera se tourne vers le tueur
		if (AActor* K = KillerActor.Get())
		{
			if (Controller)
			{
				const FVector To = (K->GetActorLocation() + FVector(0.f, 0.f, 40.f)) - GetEyeLocation();
				const FRotator Want = To.Rotation();
				Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Want, Dt, 12.f));
			}
		}
		return;
	}

	// Rien ne se passe tant que le menu titre est ouvert
	if (const ABRPlayerController* PC = Cast<ABRPlayerController>(Controller))
	{
		if (PC->IsInMenu())
		{
			return;
		}
	}

	// Endurance
	const bool bSprint = IsSprinting();
	GetCharacterMovement()->MaxWalkSpeed = (bWantsSprint && !bExhausted) ? SprintSpeed : WalkSpeed;
	if (bSprint)
	{
		Stamina = FMath::Max(0.f, Stamina - 17.f * Dt);
		SprintTime += Dt;
		if (Stamina <= 0.f)
		{
			bExhausted = true;
		}
	}
	else
	{
		SprintTime = FMath::Max(0.f, SprintTime - Dt);
		Stamina = FMath::Min(100.f, Stamina + (GetVelocity().Size2D() < 20.f ? 16.f : 10.f) * Dt);
		if (bExhausted && Stamina > 35.f)
		{
			bExhausted = false;
		}
	}

	// Sante mentale
	const ABRWorld* W = ABRWorld::Get(this);
	if (W && !W->IsTransitioning())
	{
		const FBRLevelDef& D = W->Def();
		float Drain = D.SanityDrain + SanityPressure;
		const float Light = W->LightLevelAt(GetActorLocation());
		if (Light < 0.12f && !D.bOutdoor)
		{
			Drain += IsFlashlightOn() ? 0.06f : 0.22f;
		}
		Drain += ChaseLevel * 0.4f;
		Sanity = FMath::Clamp(Sanity - Drain * Dt, 0.f, 100.f);
	}

	// Sante : la folie blesse, le calme soigne
	if (Sanity <= 0.f)
	{
		Health -= 1.5f * Dt;
		if (Health <= 0.f)
		{
			Die(TEXT("la folie"), nullptr);
			return;
		}
	}
	else if (TimeAlive - LastDamageTime > 8.f && Sanity > 30.f)
	{
		Health = FMath::Min(100.f, Health + 1.f * Dt);
	}

	DamageFlash = FMath::Max(0.f, DamageFlash - Dt * 1.5f);
	ChaseLevel = FMath::FInterpTo(ChaseLevel, ChaseTarget, Dt, ChaseTarget > ChaseLevel ? 3.f : 0.4f);
}

void ABRCharacter::UpdateCamera(float Dt)
{
	if (!Camera)
	{
		return;
	}
	const float TargetZ = bIsCrouched ? CrouchEyeZ : StandEyeZ;
	CamZ = FMath::FInterpTo(CamZ, TargetZ, Dt, 10.f);

	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const bool bGrounded = GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround();
	float BobZ = 0.f;
	float BobY = 0.f;
	if (bGrounded && Speed > 15.f && !bDead)
	{
		const float Rate = 8.f * (Speed / WalkSpeed);
		BobTime += Dt * FMath::Clamp(Rate, 4.f, 14.f);
		const float Amp = IsSprinting() ? 3.2f : (bIsCrouched ? 1.2f : 2.f);
		BobZ = FMath::Sin(BobTime) * Amp;
		BobY = FMath::Cos(BobTime * 0.5f) * Amp * 0.6f;
		const int32 Phase = FMath::FloorToInt(BobTime / PI);
		if (Phase != LastStepPhase)
		{
			LastStepPhase = Phase;
			PlayFootstep();
		}
	}

	// Mort : la camera tombe au sol
	float DeathDrop = 0.f;
	if (bDead)
	{
		DeathDrop = -FMath::Min(DeathTime * 1.5f, 1.f) * 60.f;
	}

	Camera->SetRelativeLocation(FVector(0.f, BobY, CamZ + BobZ + DeathDrop));

	// Roulis leger + vertige quand la sante mentale baisse
	const float Insanity = 1.f - Sanity / 100.f;
	const float Wobble = Insanity > 0.5f ? FMath::Sin(TimeAlive * 0.8f) * (Insanity - 0.5f) * 8.f : 0.f;
	Camera->SetRelativeRotation(FRotator(0.f, 0.f, Wobble + (bDead ? FMath::Min(DeathTime, 1.f) * 25.f : 0.f)));
	Camera->SetFieldOfView(88.f + (IsSprinting() ? 4.f : 0.f) + FMath::Sin(TimeAlive * 0.6f) * Insanity * Insanity * 6.f);

	// Lampe : leger retard sur les mouvements de camera
	LookLag = FMath::Vector2DInterpTo(LookLag, FVector2D::ZeroVector, Dt, 8.f);
	if (FlashlightMesh)
	{
		FlashlightMesh->SetRelativeLocation(FVector(30.f, 16.f - LookLag.X * 0.15f, -19.f + BobZ * 0.4f + LookLag.Y * 0.15f));
	}
}

void ABRCharacter::UpdateFlashlight(float Dt)
{
	if (!Flashlight)
	{
		return;
	}
	if (bFlashlightOn && Battery > 0.f && !bDead)
	{
		Battery = FMath::Max(0.f, Battery - BatteryDrain * Dt);
		float Mult = FMath::Lerp(0.35f, 1.f, FMath::Clamp(Battery / 40.f, 0.f, 1.f));
		if (Battery < 15.f)
		{
			// Piles faibles : la lampe vacille
			if (FMath::FRand() < 0.06f)
			{
				FlashFlicker = FMath::FRandRange(0.f, 0.5f);
			}
			FlashFlicker = FMath::FInterpTo(FlashFlicker, 1.f, Dt, 6.f);
			Mult *= FlashFlicker;
		}
		Flashlight->SetIntensity(FlashCandelas * Mult);
		Flashlight->SetVisibility(true);
		FlashlightMesh->SetVisibility(true);
		if (Battery <= 0.f)
		{
			bFlashlightOn = false;
			ABRHUD::Notify(this, TEXT("La lampe s'\u00e9teint. [R] changer les piles"), 3.f, FLinearColor(1.f, 0.8f, 0.4f));
		}
	}
	else
	{
		Flashlight->SetVisibility(false);
		FlashlightMesh->SetVisibility(bFlashlightOn && !bDead);
	}
}

void ABRCharacter::UpdateFocus()
{
	FocusPrompt.Empty();
	FocusActor.Reset();
	if (bDead || bInputLocked || !GetWorld())
	{
		return;
	}
	const FVector Start = GetEyeLocation();
	const FVector End = Start + GetViewDirection() * 230.f;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRFocus), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Q))
	{
		AActor* A = Hit.GetActor();
		if (const ABRPickup* P = Cast<ABRPickup>(A))
		{
			FocusActor = A;
			FocusPrompt = P->GetPrompt();
		}
		else if (const ABRExit* E = Cast<ABRExit>(A))
		{
			if (E->IsInteractable())
			{
				FocusActor = A;
				FocusPrompt = E->GetPrompt();
			}
		}
	}
}

void ABRCharacter::PlayFootstep()
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	const TCHAR* Kind = TEXT("Carpet");
	switch (StepType)
	{
	case EBRStep::Hard:
		Kind = TEXT("Hard");
		break;
	case EBRStep::Water:
		Kind = TEXT("Water");
		break;
	case EBRStep::Grass:
		Kind = TEXT("Grass");
		break;
	default:
		break;
	}
	const FName Name(*FString::Printf(TEXT("S_Step_%s_%d"), Kind, FMath::RandRange(1, 4)));
	if (USoundBase* S = A->Sound(Name))
	{
		const float Vol = bIsCrouched ? 0.18f : (IsSprinting() ? 0.75f : 0.42f);
		UGameplayStatics::PlaySound2D(this, S, Vol, FMath::FRandRange(0.92f, 1.08f));
	}
}

void ABRCharacter::UpdateAudio(float Dt)
{
	const float Fear = FMath::Clamp(FMath::Max3(1.f - Health / 100.f, ChaseLevel, (35.f - Sanity) / 35.f), 0.f, 1.f);
	if (HeartAudio && HeartAudio->Sound)
	{
		if (!HeartAudio->IsPlaying())
		{
			HeartAudio->Play();
		}
		HeartAudio->SetVolumeMultiplier(FMath::Max(0.001f, Fear * 0.9f));
		HeartAudio->SetPitchMultiplier(1.f + Fear * 0.35f);
	}
	if (BreathAudio && BreathAudio->Sound)
	{
		if (!BreathAudio->IsPlaying())
		{
			BreathAudio->Play();
		}
		const float Tired = FMath::Clamp((60.f - Stamina) / 60.f, 0.f, 1.f);
		BreathAudio->SetVolumeMultiplier(FMath::Max(0.001f, bDead ? 0.f : Tired * 0.7f));
	}
	if (ChaseAudio && ChaseAudio->Sound)
	{
		if (!ChaseAudio->IsPlaying())
		{
			ChaseAudio->Play();
		}
		ChaseAudio->SetVolumeMultiplier(FMath::Max(0.001f, bDead ? 0.f : ChaseLevel * 0.8f));
	}
}

void ABRCharacter::UpdatePostProcess(float Dt)
{
	if (!Camera)
	{
		return;
	}
	const ABRWorld* W = ABRWorld::Get(this);
	const FBRLevelDef* D = W ? &W->Def() : nullptr;
	const float Insanity = 1.f - Sanity / 100.f;
	const float Glitch = W ? W->GetGlitch() : 0.f;
	const float Dead = bDead ? FMath::Min(DeathTime / 2.f, 1.f) : 0.f;

	FPostProcessSettings& S = Camera->PostProcessSettings;
	Camera->PostProcessBlendWeight = 1.f;

	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.4f + Insanity * Insanity * 4.f + Glitch * 8.f + DamageFlash * 3.f;

	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = (D ? D->Grain : 0.25f) + Insanity * 0.5f + Glitch * 0.8f;

	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = (D ? D->Vignette : 0.45f) + Insanity * 0.5f + DamageFlash * 0.6f + Dead * 0.8f;

	const float Sat = (D ? D->Saturation : 1.f) * FMath::Lerp(1.f, 0.45f, FMath::Max(Insanity * Insanity, Dead));
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);

	const FLinearColor BaseTint = D ? D->SceneTint : FLinearColor::White;
	const FLinearColor Hurt(1.f, 0.35f, 0.3f);
	S.bOverride_SceneColorTint = true;
	S.SceneColorTint = FMath::Lerp(BaseTint, Hurt, FMath::Clamp(DamageFlash * 0.6f + Dead * 0.5f, 0.f, 1.f));

	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;
}
