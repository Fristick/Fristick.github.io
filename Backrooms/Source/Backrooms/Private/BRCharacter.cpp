#include "BRCharacter.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRItems.h"
#include "BRWorld.h"
#include "BRHUD.h"
#include "BRInteractables.h"
#include "BRPlayerController.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float WalkSpeed = 260.f;
	constexpr float SprintSpeed = 470.f;
	constexpr float CrouchSpeed = 140.f;
	constexpr float StandEyeZ = 74.f;
	constexpr float CrouchEyeZ = 42.f;
	constexpr float FlashCandelas = 650.f;
	constexpr float BatteryDrain = 0.4f;      // % par seconde (~4 min)
	constexpr float NightVisionDrain = 0.3f;

	// Position de la source lumineuse selon l'emplacement de la lampe
	const FVector HandLightPos(32.f, 16.f, -14.f);
	const FVector BeltLightPos(12.f, 14.f, -52.f);
	const FVector HeadLightPos(6.f, 0.f, 9.f);
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
	Flashlight->SetRelativeLocation(BeltLightPos);
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

	HandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandMesh"));
	HandMesh->SetupAttachment(Camera);
	HandMesh->SetRelativeLocation(FVector(30.f, 16.f, -19.f));
	HandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandMesh->SetCastShadow(false);

	InfraredLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("InfraredLight"));
	InfraredLight->SetupAttachment(Camera);
	InfraredLight->SetRelativeLocation(FVector(40.f, 0.f, 0.f));
	InfraredLight->SetIntensityUnits(ELightUnits::Lumens);
	InfraredLight->SetIntensity(500.f);
	InfraredLight->SetAttenuationRadius(1400.f);
	InfraredLight->SetCastShadows(false);
	InfraredLight->SetVisibility(false);

	HeartAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("HeartAudio"));
	HeartAudio->SetupAttachment(RootComponent);
	HeartAudio->bAutoActivate = false;

	BreathAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BreathAudio"));
	BreathAudio->SetupAttachment(RootComponent);
	BreathAudio->bAutoActivate = false;

	ChaseAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ChaseAudio"));
	ChaseAudio->SetupAttachment(RootComponent);
	ChaseAudio->bAutoActivate = false;

	ResetInventory();
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
		SetupLoopAudio(HeartAudio, TEXT("S_Heartbeat"));
		SetupLoopAudio(BreathAudio, TEXT("S_Breath"));
		SetupLoopAudio(ChaseAudio, TEXT("S_Chase"));
	}
	OnEquipmentChanged();
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

void ABRCharacter::PlaySound2D(FName SoundName, float Volume)
{
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(SoundName))
		{
			UGameplayStatics::PlaySound2D(this, S, Volume);
		}
	}
}

// =====================================================================================================================
// Inventaire
// =====================================================================================================================

void ABRCharacter::ResetInventory()
{
	Pockets.SetNum(NumPockets);
	Storage.SetNum(NumStorage);
	Equipment.SetNum(static_cast<int32>(EBREquipSlot::Count));
	for (FBRItemSlot& S : Pockets)
	{
		S.Clear();
	}
	for (FBRItemSlot& S : Storage)
	{
		S.Clear();
	}
	for (FBRItemSlot& S : Equipment)
	{
		S.Clear();
	}
	Pockets[0] = FBRItemSlot{ EBRItem::AlmondWater, 1 };
	Pockets[1] = FBRItemSlot{ EBRItem::Bandage, 1 };
	Pockets[2] = FBRItemSlot{ EBRItem::Battery, 1 };
	Equipment[static_cast<int32>(EBREquipSlot::Hand)] = FBRItemSlot{ EBRItem::Camcorder, 1 };
	Equipment[static_cast<int32>(EBREquipSlot::Belt)] = FBRItemSlot{ EBRItem::Flashlight, 1 };
}

FBRItemSlot* ABRCharacter::GetSlot(EBRSlotGroup Group, int32 Index)
{
	TArray<FBRItemSlot>& Arr = Group == EBRSlotGroup::Pockets ? Pockets : (Group == EBRSlotGroup::Storage ? Storage : Equipment);
	return Arr.IsValidIndex(Index) ? &Arr[Index] : nullptr;
}

EBRItem ABRCharacter::GetEquipped(EBREquipSlot Slot) const
{
	const int32 I = static_cast<int32>(Slot);
	return Equipment.IsValidIndex(I) && !Equipment[I].IsEmpty() ? Equipment[I].Item : EBRItem::None;
}

bool ABRCharacter::HasLightSource() const
{
	return GetEquipped(EBREquipSlot::Hand) == EBRItem::Flashlight || GetEquipped(EBREquipSlot::Belt) == EBRItem::Flashlight
		|| GetEquipped(EBREquipSlot::Head) == EBRItem::Headlamp;
}

int32 ABRCharacter::CountItem(EBRItem Item) const
{
	int32 N = 0;
	for (const FBRItemSlot& S : Pockets)
	{
		N += (S.Item == Item) ? S.Count : 0;
	}
	for (const FBRItemSlot& S : Storage)
	{
		N += (S.Item == Item) ? S.Count : 0;
	}
	for (const FBRItemSlot& S : Equipment)
	{
		N += (S.Item == Item) ? S.Count : 0;
	}
	return N;
}

int32 ABRCharacter::AddItem(EBRItem Item, int32 Count)
{
	const FBRItemInfo& Info = BRItems::Get(Item);
	const int32 MaxStack = FMath::Max(1, Info.MaxStack);
	int32 Left = Count;
	// 1) completer les piles existantes
	for (TArray<FBRItemSlot>* Arr : { &Pockets, &Storage })
	{
		for (FBRItemSlot& S : *Arr)
		{
			if (Left > 0 && S.Item == Item && S.Count < MaxStack)
			{
				const int32 Add = FMath::Min(Left, MaxStack - S.Count);
				S.Count += Add;
				Left -= Add;
			}
		}
	}
	// 2) equipement libre (une lampe va directement dans un emplacement vide)
	if (Left > 0 && Info.Slot != EBREquipSlot::None)
	{
		for (int32 i = 0; i < Equipment.Num() && Left > 0; ++i)
		{
			if (Equipment[i].IsEmpty() && BRItems::CanEquipIn(Item, static_cast<EBREquipSlot>(i)))
			{
				Equipment[i] = FBRItemSlot{ Item, 1 };
				--Left;
				OnEquipmentChanged();
			}
		}
	}
	// 3) cases vides : poches d'abord pour les consommables, sac pour le reste
	TArray<TArray<FBRItemSlot>*> Order;
	if (Info.bConsumable)
	{
		Order = { &Pockets, &Storage };
	}
	else
	{
		Order = { &Storage, &Pockets };
	}
	for (TArray<FBRItemSlot>* Arr : Order)
	{
		for (FBRItemSlot& S : *Arr)
		{
			if (Left > 0 && S.IsEmpty())
			{
				const int32 Add = FMath::Min(Left, MaxStack);
				S = FBRItemSlot{ Item, Add };
				Left -= Add;
			}
		}
	}
	return Left;
}

bool ABRCharacter::StoreItem(EBRItem Item)
{
	return AddItem(Item, 1) == 0;
}

bool ABRCharacter::MoveItem(EBRSlotGroup FromGroup, int32 FromIndex, EBRSlotGroup ToGroup, int32 ToIndex)
{
	FBRItemSlot* From = GetSlot(FromGroup, FromIndex);
	FBRItemSlot* To = GetSlot(ToGroup, ToIndex);
	if (!From || !To || From == To || From->IsEmpty())
	{
		return false;
	}
	// Contraintes d'equipement
	if (ToGroup == EBRSlotGroup::Equipment && !BRItems::CanEquipIn(From->Item, static_cast<EBREquipSlot>(ToIndex)))
	{
		ABRHUD::Notify(this, FString::Printf(TEXT("%s ne peut pas aller dans l'emplacement %s."), *BRItems::Get(From->Item).Name,
			*BRItems::SlotName(static_cast<EBREquipSlot>(ToIndex))), 2.5f, FLinearColor(1.f, 0.7f, 0.5f));
		return false;
	}
	if (FromGroup == EBRSlotGroup::Equipment && !To->IsEmpty() && !BRItems::CanEquipIn(To->Item, static_cast<EBREquipSlot>(FromIndex)))
	{
		return false;
	}
	// Empilement
	const int32 MaxStack = FMath::Max(1, BRItems::Get(From->Item).MaxStack);
	if (To->Item == From->Item && MaxStack > 1 && ToGroup != EBRSlotGroup::Equipment)
	{
		const int32 Add = FMath::Min(From->Count, MaxStack - To->Count);
		if (Add > 0)
		{
			To->Count += Add;
			From->Count -= Add;
			if (From->Count <= 0)
			{
				From->Clear();
			}
			PlaySound2D(TEXT("S_ItemMove"), 0.6f);
			return true;
		}
	}
	// Une case d'equipement ne contient qu'un objet
	if (ToGroup == EBRSlotGroup::Equipment && From->Count > 1)
	{
		if (!To->IsEmpty())
		{
			return false;
		}
		*To = FBRItemSlot{ From->Item, 1 };
		From->Count -= 1;
	}
	else
	{
		Swap(*From, *To);
	}
	PlaySound2D(TEXT("S_ItemMove"), 0.6f);
	if (FromGroup == EBRSlotGroup::Equipment || ToGroup == EBRSlotGroup::Equipment)
	{
		OnEquipmentChanged();
	}
	return true;
}

void ABRCharacter::UseSlot(EBRSlotGroup Group, int32 Index)
{
	FBRItemSlot* S = GetSlot(Group, Index);
	if (!S || S->IsEmpty() || bDead)
	{
		return;
	}
	const EBRItem Item = S->Item;
	const FBRItemInfo& Info = BRItems::Get(Item);
	if (Info.bConsumable)
	{
		if (UseItemEffect(Item))
		{
			S->Count -= 1;
			if (S->Count <= 0)
			{
				S->Clear();
			}
		}
		return;
	}
	if (Info.Slot == EBREquipSlot::None)
	{
		return;
	}
	if (Group == EBRSlotGroup::Equipment)
	{
		// Desequiper : vers la premiere case libre
		for (TArray<FBRItemSlot>* Arr : { &Pockets, &Storage })
		{
			for (FBRItemSlot& Free : *Arr)
			{
				if (Free.IsEmpty())
				{
					Swap(Free, *S);
					PlaySound2D(TEXT("S_ItemMove"), 0.6f);
					OnEquipmentChanged();
					return;
				}
			}
		}
		ABRHUD::Notify(this, TEXT("Inventaire plein."), 2.f, FLinearColor(1.f, 0.7f, 0.5f));
		return;
	}
	// Equiper : la lampe torche va dans la main (ou la ceinture si la main est prise)
	EBREquipSlot Target = Info.Slot;
	if (Item == EBRItem::Flashlight && GetEquipped(EBREquipSlot::Hand) != EBRItem::None && GetEquipped(EBREquipSlot::Belt) == EBRItem::None)
	{
		Target = EBREquipSlot::Belt;
	}
	MoveItem(Group, Index, EBRSlotGroup::Equipment, static_cast<int32>(Target));
}

void ABRCharacter::QuickUse(EBRItem Item)
{
	if (bInputLocked || bDead)
	{
		return;
	}
	for (int32 i = 0; i < Pockets.Num(); ++i)
	{
		if (Pockets[i].Item == Item && !Pockets[i].IsEmpty())
		{
			UseSlot(EBRSlotGroup::Pockets, i);
			return;
		}
	}
	for (int32 i = 0; i < Storage.Num(); ++i)
	{
		if (Storage[i].Item == Item && !Storage[i].IsEmpty())
		{
			UseSlot(EBRSlotGroup::Storage, i);
			return;
		}
	}
	ABRHUD::Notify(this, FString::Printf(TEXT("Plus de %s."), *BRItems::Get(Item).Name.ToLower()), 2.f, FLinearColor(1.f, 0.7f, 0.5f));
}

void ABRCharacter::UsePocket(int32 Index)
{
	if (bInputLocked || bDead)
	{
		return;
	}
	UseSlot(EBRSlotGroup::Pockets, Index);
}

bool ABRCharacter::UseItemEffect(EBRItem Item)
{
	switch (Item)
	{
	case EBRItem::AlmondWater:
		if (Sanity >= 99.f && Health >= 99.f)
		{
			ABRHUD::Notify(this, TEXT("Vous n'en avez pas besoin pour l'instant."), 2.f);
			return false;
		}
		Sanity = FMath::Min(100.f, Sanity + 40.f);
		Health = FMath::Min(100.f, Health + 10.f);
		PlaySound2D(TEXT("S_Drink"), 0.9f);
		ABRHUD::Notify(this, TEXT("Vous buvez de l'eau d'amande. Votre esprit s'\u00e9claircit."), 3.f, FLinearColor(0.85f, 0.95f, 1.f));
		return true;
	case EBRItem::Bandage:
		if (Health >= 99.f)
		{
			ABRHUD::Notify(this, TEXT("Vous n'\u00eates pas bless\u00e9."), 2.f);
			return false;
		}
		Health = FMath::Min(100.f, Health + 35.f);
		PlaySound2D(TEXT("S_Bandage"), 0.9f);
		ABRHUD::Notify(this, TEXT("Vous bandez vos blessures."), 2.5f, FLinearColor(0.9f, 0.95f, 0.9f));
		return true;
	case EBRItem::Battery:
		if (Battery > 90.f)
		{
			ABRHUD::Notify(this, TEXT("Les piles sont encore pleines."), 2.f);
			return false;
		}
		Battery = 100.f;
		PlaySound2D(TEXT("S_Battery"), 0.8f);
		ABRHUD::Notify(this, TEXT("Piles remplac\u00e9es."), 2.f);
		return true;
	case EBRItem::EnergyBar:
		Stamina = 100.f;
		bExhausted = false;
		EnergyBoost = 30.f;
		PlaySound2D(TEXT("S_Eat"), 0.9f);
		ABRHUD::Notify(this, TEXT("Un regain d'\u00e9nergie !"), 2.f, FLinearColor(1.f, 0.9f, 0.6f));
		return true;
	default:
		return false;
	}
}

void ABRCharacter::OnEquipmentChanged()
{
	const EBRItem InHand = GetEquipped(EBREquipSlot::Hand);
	if (!HasCamcorderInHand() && bNightVision)
	{
		bNightVision = false;
	}
	if (!HasLightSource())
	{
		bFlashlightOn = false;
	}

	// Position et forme du faisceau selon la lampe utilisee
	if (Flashlight)
	{
		if (InHand == EBRItem::Flashlight)
		{
			Flashlight->SetRelativeLocation(HandLightPos);
			Flashlight->SetInnerConeAngle(13.f);
			Flashlight->SetOuterConeAngle(30.f);
		}
		else if (GetEquipped(EBREquipSlot::Belt) == EBRItem::Flashlight)
		{
			Flashlight->SetRelativeLocation(BeltLightPos);
			Flashlight->SetInnerConeAngle(15.f);
			Flashlight->SetOuterConeAngle(34.f);
		}
		else
		{
			Flashlight->SetRelativeLocation(HeadLightPos);
			Flashlight->SetInnerConeAngle(22.f);
			Flashlight->SetOuterConeAngle(45.f);
		}
	}

	// Objet visible dans la main
	if (HandMesh && InHand != HandVisual)
	{
		HandVisual = InHand;
		HandGlow.Empty();
		UBRAssets* A = UBRAssets::Get(this);
		UStaticMesh* M = nullptr;
		FVector Pos(30.f, 16.f, -19.f);
		FRotator Rot(2.f, -4.f, 0.f);
		if (A && InHand == EBRItem::Flashlight)
		{
			M = A->Mesh(TEXT("SM_Flashlight_FP"));
			if (!M)
			{
				M = A->Mesh(TEXT("SM_Flashlight"));
			}
		}
		else if (A && InHand == EBRItem::Camcorder)
		{
			M = A->Mesh(TEXT("SM_Camcorder_FP"));
			if (!M)
			{
				M = A->Mesh(TEXT("SM_Camcorder"));
			}
			Pos = FVector(30.f, 18.f, -21.f);
			Rot = FRotator(4.f, -8.f, 0.f);
		}
		HandMesh->SetStaticMesh(M);
		HandMesh->SetRelativeLocation(Pos);
		HandMesh->SetRelativeRotation(Rot);
		HandMesh->SetVisibility(M != nullptr);
		if (A && M)
		{
			TArray<UMaterialInstanceDynamic*> Glows;
			A->ApplySlots(HandMesh, nullptr, true, &Glows, InHand == EBRItem::Camcorder ? 0.15f : 0.05f);
			for (UMaterialInstanceDynamic* G : Glows)
			{
				HandGlow.Add(G);
			}
		}
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
	if (!HasLightSource())
	{
		ABRHUD::Notify(this, TEXT("Aucune lampe \u00e9quip\u00e9e (inventaire : [TAB])."), 2.5f, FLinearColor(1.f, 0.8f, 0.4f));
		return;
	}
	if (!bFlashlightOn && Battery <= 0.f)
	{
		ABRHUD::Notify(this, CountItem(EBRItem::Battery) > 0 ? TEXT("Piles vides : [R] pour les changer") : TEXT("Piles vides... il faut en trouver."),
			3.f, FLinearColor(1.f, 0.8f, 0.4f));
		return;
	}
	bFlashlightOn = !bFlashlightOn;
	PlaySound2D(TEXT("S_Flashlight"), 0.7f);
}

void ABRCharacter::ToggleNightVision()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	if (!HasCamcorderInHand())
	{
		ABRHUD::Notify(this, TEXT("Il faut tenir le cam\u00e9scope en main pour la vision nocturne."), 2.5f, FLinearColor(1.f, 0.8f, 0.4f));
		return;
	}
	if (!bNightVision && Battery <= 0.f)
	{
		ABRHUD::Notify(this, TEXT("Batterie du cam\u00e9scope vide."), 2.5f, FLinearColor(1.f, 0.8f, 0.4f));
		return;
	}
	bNightVision = !bNightVision;
	PlaySound2D(bNightVision ? FName(TEXT("S_NightVision")) : FName(TEXT("S_RecBeep")), 0.7f);
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
	if (!GetCharacterMovement() || !GetCharacterMovement()->IsMovingOnGround())
	{
		return 1300.f; // saut / reception
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
	const float Armor = GetEquipped(EBREquipSlot::Chest) == EBRItem::Vest ? 0.7f : 1.f;
	Health -= Damage * (Damage >= 100.f ? 1.f : Armor);
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
	PlaySound2D(TEXT("S_Hurt"), 1.f);
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
	bNightVision = false;
	GetCharacterMovement()->DisableMovement();
	PlaySound2D(TEXT("S_Death"), 1.f);
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
	bDead = false;
	bExhausted = false;
	bFlashlightOn = false;
	bNightVision = false;
	EnergyBoost = 0.f;
	DamageFlash = 0.f;
	ChaseLevel = ChaseTarget = 0.f;
	KilledBy.Empty();
	KillerActor.Reset();
	ResetInventory();
	OnEquipmentChanged();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (bIsCrouched)
	{
		UnCrouch();
	}
}

bool ABRCharacter::ReceivePickup(EBRItem Item, const FString& Note)
{
	if (Item == EBRItem::Note)
	{
		++NotesRead;
		OpenNote = Note.IsEmpty() ? FString(TEXT("(La note est illisible, l'encre a coul\u00e9.)")) : Note;
		ReadNotes.AddUnique(OpenNote);
		bReadingNote = true;
		return true;
	}
	if (AddItem(Item, 1) > 0)
	{
		ABRHUD::Notify(this, TEXT("Inventaire plein ! [TAB] pour faire de la place."), 2.5f, FLinearColor(1.f, 0.6f, 0.5f));
		return false;
	}
	const FBRItemInfo& Info = BRItems::Get(Item);
	if (Item == EBRItem::VHSTape)
	{
		if (ABRWorld* W = ABRWorld::Get(this))
		{
			W->OnVHSCollected();
		}
	}
	else
	{
		ABRHUD::Notify(this, FString::Printf(TEXT("+1 %s  (%d)"), *Info.Name, CountItem(Item)), 2.5f, FLinearColor(0.9f, 0.88f, 0.75f));
	}
	return true;
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
		ABRHUD::Notify(this, TEXT("Il fait noir comme dans un four. [F] lampe  -  [N] vision nocturne"), 5.f, FLinearColor(1.f, 0.85f, 0.6f));
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

	// Taches d'enregistrement : il suffit de tenir le camescope
	if (HasCamcorderInHand() && !bDead && !bInputLocked)
	{
		if (ABRWorld* W = ABRWorld::Get(this))
		{
			W->NotifyRecording(Dt, GetEyeLocation(), GetViewDirection());
		}
	}

	// Tendances pour la biometrie
	if (Dt > 0.f)
	{
		SanityTrend = FMath::FInterpTo(SanityTrend, (Sanity - PrevSanity) / Dt, Dt, 2.f);
		HealthTrend = FMath::FInterpTo(HealthTrend, (Health - PrevHealth) / Dt, Dt, 2.f);
		StaminaTrend = FMath::FInterpTo(StaminaTrend, (Stamina - PrevStamina) / Dt, Dt, 2.f);
	}
	PrevSanity = Sanity;
	PrevHealth = Health;
	PrevStamina = Stamina;

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
	EnergyBoost = FMath::Max(0.f, EnergyBoost - Dt);
	const bool bSprint = IsSprinting();
	GetCharacterMovement()->MaxWalkSpeed = (bWantsSprint && !bExhausted) ? SprintSpeed : WalkSpeed;
	if (bSprint)
	{
		Stamina = FMath::Max(0.f, Stamina - (EnergyBoost > 0.f ? 10.f : 17.f) * Dt);
		SprintTime += Dt;
		if (Stamina <= 0.f)
		{
			bExhausted = true;
		}
	}
	else
	{
		SprintTime = FMath::Max(0.f, SprintTime - Dt);
		const float Regen = (GetVelocity().Size2D() < 20.f ? 16.f : 10.f) * (EnergyBoost > 0.f ? 2.f : 1.f);
		Stamina = FMath::Min(100.f, Stamina + Regen * Dt);
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
			Drain += (IsFlashlightOn() || bNightVision) ? 0.06f : 0.22f;
		}
		if (W->IsBlackout())
		{
			Drain += 0.15f;
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
	Camera->SetFieldOfView(FBRSettings::Get().FOV + (IsSprinting() ? 4.f : 0.f) + FMath::Sin(TimeAlive * 0.6f) * Insanity * Insanity * 6.f);

	// Objet en main : leger retard sur les mouvements de camera
	LookLag = FMath::Vector2DInterpTo(LookLag, FVector2D::ZeroVector, Dt, 8.f);
	if (HandMesh && HandMesh->IsVisible())
	{
		const bool bCam = HandVisual == EBRItem::Camcorder;
		const FVector Base = bCam ? FVector(30.f, 18.f, -21.f) : FVector(30.f, 16.f, -19.f);
		HandMesh->SetRelativeLocation(Base + FVector(0.f, -static_cast<float>(LookLag.X) * 0.15f, BobZ * 0.4f + static_cast<float>(LookLag.Y) * 0.15f));
	}
}

void ABRCharacter::UpdateFlashlight(float Dt)
{
	if (!Flashlight)
	{
		return;
	}
	const bool bHeadlamp = GetEquipped(EBREquipSlot::Hand) != EBRItem::Flashlight && GetEquipped(EBREquipSlot::Belt) != EBRItem::Flashlight;
	if (bNightVision && !bDead)
	{
		Battery = FMath::Max(0.f, Battery - NightVisionDrain * Dt);
		if (Battery <= 0.f)
		{
			bNightVision = false;
			ABRHUD::Notify(this, TEXT("Batterie vide : vision nocturne coup\u00e9e. [R] changer les piles"), 3.f, FLinearColor(1.f, 0.8f, 0.4f));
		}
	}
	if (InfraredLight)
	{
		InfraredLight->SetVisibility(bNightVision && !bDead);
	}

	float Glow = 0.f;
	if (bFlashlightOn && Battery > 0.f && !bDead && HasLightSource())
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
		Flashlight->SetIntensity(FlashCandelas * Mult * (bHeadlamp ? 0.55f : 1.f));
		Flashlight->SetVisibility(true);
		Glow = Mult;
		if (Battery <= 0.f)
		{
			bFlashlightOn = false;
			ABRHUD::Notify(this, TEXT("La lampe s'\u00e9teint. [R] changer les piles"), 3.f, FLinearColor(1.f, 0.8f, 0.4f));
		}
	}
	else
	{
		Flashlight->SetVisibility(false);
	}
	// Verre de la lampe tenue en main
	if (HandVisual == EBRItem::Flashlight)
	{
		for (UMaterialInstanceDynamic* G : HandGlow)
		{
			if (G)
			{
				G->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(1.f, 0.95f, 0.85f) * (5.f + 120.f * Glow));
			}
		}
	}
	else if (HandVisual == EBRItem::Camcorder)
	{
		const bool bBlink = FMath::Fmod(TimeAlive, 1.f) < 0.6f;
		for (UMaterialInstanceDynamic* G : HandGlow)
		{
			if (G)
			{
				G->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(1.f, 0.05f, 0.03f) * (bBlink ? 30.f : 2.f));
			}
		}
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
	const FBRSettings& Set = FBRSettings::Get();
	const float Insanity = 1.f - Sanity / 100.f;
	const float Glitch = W ? W->GetGlitch() : 0.f;
	const float Dead = bDead ? FMath::Min(DeathTime / 2.f, 1.f) : 0.f;
	const bool bNV = bNightVision && !bDead;

	FPostProcessSettings& S = Camera->PostProcessSettings;
	Camera->PostProcessBlendWeight = 1.f;

	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.4f + Insanity * Insanity * 4.f + Glitch * 8.f + DamageFlash * 3.f + (bNV ? 1.5f : 0.f);

	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = (Set.bFilmGrain ? (D ? D->Grain : 0.25f) : 0.f) + Insanity * 0.5f + Glitch * 0.8f + (bNV ? 0.7f : 0.f);

	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = (D ? D->Vignette : 0.45f) + Insanity * 0.5f + DamageFlash * 0.6f + Dead * 0.8f + (bNV ? 0.5f : 0.f);

	float Sat = (D ? D->Saturation : 1.f) * FMath::Lerp(1.f, 0.45f, FMath::Max(Insanity * Insanity, Dead));
	if (bNV)
	{
		Sat = 0.f;
	}
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);

	FLinearColor Tint = D ? D->SceneTint : FLinearColor::White;
	if (bNV)
	{
		Tint = FLinearColor(0.35f, 1.f, 0.45f);
	}
	const FLinearColor Hurt(1.f, 0.35f, 0.3f);
	S.bOverride_SceneColorTint = true;
	S.SceneColorTint = FMath::Lerp(Tint, Hurt, FMath::Clamp(DamageFlash * 0.6f + Dead * 0.5f, 0.f, 1.f));

	// Vision nocturne : amplification de lumiere
	S.bOverride_AutoExposureBias = bNV;
	S.AutoExposureBias = (D ? D->ExposureBias : 0.f) + 3.5f;
	S.bOverride_AutoExposureMinBrightness = bNV;
	S.AutoExposureMinBrightness = (D ? D->MinEV : 2.f) - 4.f;
	S.bOverride_BloomIntensity = bNV;
	S.BloomIntensity = 2.f;

	// Salete sur l'objectif du camescope (visible dans les halos des neons)
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (UTexture* Dirt = A->Texture(TEXT("T_LensDirt")))
		{
			S.bOverride_BloomDirtMask = true;
			S.BloomDirtMask = Dirt;
			S.bOverride_BloomDirtMaskIntensity = true;
			S.BloomDirtMaskIntensity = HasCamcorderInHand() ? 8.f : 3.f;
		}
	}

	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;
}
