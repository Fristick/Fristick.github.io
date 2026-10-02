#include "BRCharacter.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRItems.h"
#include "BRKeys.h"
#include "BRWorld.h"
#include "BRHUD.h"
#include "BRInteractables.h"
#include "BRPlayerController.h"
#include "BREntity.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
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
	constexpr float SwimSpeed = 190.f;
	constexpr float SwimSprintSpeed = 290.f;
	constexpr float BreathSeconds = 18.f;    // temps d'apnee
	constexpr float ThirdPersonArm = 240.f;
	const FVector ThirdPersonOffset(0.f, 45.f, 18.f);

	// Position de la source lumineuse selon l'emplacement de la lampe
	const FVector HandLightPos(32.f, 16.f, -14.f);
	const FVector BeltLightPos(12.f, 14.f, -52.f);
	const FVector HeadLightPos(6.f, 0.f, 9.f);
	/** A la 3e personne la lampe est avancee devant la combinaison (sinon le corps la masquerait) */
	const FVector ThirdPersonLightPush(32.f, 0.f, 0.f);
	constexpr float InteractReach = 260.f;
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
	// Multijoueur : chaque joueur simule ses propres deplacements (nage, mantle, vitesse variable dans l'eau)
	// et le serveur reprend sa position telle quelle, sans corrections qui feraient sauter l'image
	Move->bIgnoreClientMovementErrorChecksAndCorrection = true;
	Move->bServerAcceptClientAuthoritativePosition = true;

	// La perche suit la rotation de visee ; a la 1re personne sa longueur est nulle
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, StandEyeZ));
	CameraBoom->TargetArmLength = 0.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->ProbeSize = 14.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
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

	UnderwaterAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("UnderwaterAudio"));
	UnderwaterAudio->SetupAttachment(RootComponent);
	UnderwaterAudio->bAutoActivate = false;

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
		SetupLoopAudio(UnderwaterAudio, TEXT("S_Underwater"));
	}
	BuildBody();
	OnEquipmentChanged();
	UpdateViewMode();
}

void ABRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ABRCharacter, NetFlags, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ABRCharacter, NetHand, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ABRCharacter, NetLamp, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ABRCharacter, bDead, COND_SkipOwner);
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
	ApplyLamp(LampSlot());

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

	SetHeldVisual(InHand);
	UpdateViewMode();
}

uint8 ABRCharacter::LampSlot() const
{
	if (GetEquipped(EBREquipSlot::Hand) == EBRItem::Flashlight)
	{
		return 0;
	}
	return GetEquipped(EBREquipSlot::Belt) == EBRItem::Flashlight ? 1 : 2;
}

void ABRCharacter::ApplyLamp(uint8 Lamp)
{
	if (!Flashlight || Lamp == AppliedLamp)
	{
		return;
	}
	AppliedLamp = Lamp;
	const FVector Pos[3] = { HandLightPos, BeltLightPos, HeadLightPos };
	const float Inner[3] = { 13.f, 15.f, 22.f };
	const float Outer[3] = { 30.f, 34.f, 45.f };
	const int32 I = FMath::Min<int32>(Lamp, 2);
	FlashBase = Pos[I];
	Flashlight->SetRelativeLocation(FlashBase);
	Flashlight->SetInnerConeAngle(Inner[I]);
	Flashlight->SetOuterConeAngle(Outer[I]);
}

void ABRCharacter::SetHeldVisual(EBRItem InHand)
{
	// Objet tenu par le corps (visible a la 3e personne et par les autres joueurs)
	if (HeldMesh && InHand != HeldVisual)
	{
		HeldVisual = InHand;
		UBRAssets* A = UBRAssets::Get(this);
		UStaticMesh* M = nullptr;
		if (A && InHand == EBRItem::Flashlight)
		{
			M = A->Mesh(TEXT("SM_Flashlight"));
		}
		else if (A && InHand == EBRItem::Camcorder)
		{
			M = A->Mesh(TEXT("SM_Camcorder"));
		}
		HeldMesh->SetStaticMesh(M);
		if (A && M)
		{
			A->ApplySlots(HeldMesh);
		}
		UpdateViewMode();
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
	if (bSwimming)
	{
		// Nage : on avance dans la direction du regard (regarder vers le bas = plonger)
		const FRotator View = GetViewRotation();
		const FVector Fwd = View.Vector();
		const FVector Right = FRotator(0.f, View.Yaw, 0.f).RotateVector(FVector(0.f, 1.f, 0.f));
		AddMovementInput(Fwd, Value.Y);
		AddMovementInput(Right, Value.X);
		SwimInput = Fwd * Value.Y + Right * Value.X;
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
	bJumpHeld = bPressed;
	if (bInputLocked || bDead)
	{
		return;
	}
	if (bSwimming)
	{
		// Remonter a la surface, ou se hisser hors du bassin
		bDiving = false;
		if (bPressed && IsNearPoolEdge())
		{
			ClimbOutOfWater();
		}
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
	if (bSwimming)
	{
		bDiving = !bDiving; // plonger / arreter de plonger
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
		ABRHUD::Notify(this, BRKeys::Expand(TEXT("Aucune lampe \u00e9quip\u00e9e (inventaire : {Inventory}).")), 2.5f, FLinearColor(1.f, 0.8f, 0.4f));
		return;
	}
	if (!bFlashlightOn && Battery <= 0.f)
	{
		ABRHUD::Notify(this, CountItem(EBRItem::Battery) > 0 ? BRKeys::Expand(TEXT("Piles vides : {Battery} pour les changer")) : FString(TEXT("Piles vides... il faut en trouver.")),
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

bool ABRCharacter::IsFlashlightOn() const
{
	if (!IsLocallyControlled())
	{
		return (NetFlags & 1) != 0 && !bDead;
	}
	return bFlashlightOn && Battery > 0.f;
}

bool ABRCharacter::IsSprinting() const
{
	if (!IsLocallyControlled())
	{
		return (NetFlags & 2) != 0;
	}
	if (bSwimming)
	{
		return bWantsSprint && !bExhausted && GetVelocity().Size() > SwimSpeed * 0.8f;
	}
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
	if (bRemoteView)
	{
		return GetActorLocation() + FVector(0.f, 0.f, bIsCrouched ? CrouchEyeZ : StandEyeZ);
	}
	// Les yeux du personnage (et non la camera, qui recule a la 3e personne)
	if (!bThirdPerson && Camera)
	{
		return Camera->GetComponentLocation();
	}
	return GetActorLocation() + FVector(0.f, 0.f, CamZ);
}

FVector ABRCharacter::GetViewDirection() const
{
	// Rotation de visee du controleur : toujours a jour (la camera n'est orientee qu'au moment du rendu)
	return GetAimRotation().Vector();
}

FRotator ABRCharacter::GetAimRotation() const
{
	if (Controller)
	{
		return Controller->GetControlRotation(); // joueur local, ou copie serveur d'un client (rotation envoyee avec ses mouvements)
	}
	return GetBaseAimRotation(); // pion d'un autre joueur : lacet de l'acteur + tangage replique
}

void ABRCharacter::ReceiveAttack(float Damage, float SanityDamage, AActor* Source, const FString& SourceName)
{
	if (!IsLocallyControlled())
	{
		// Les entites vivent sur le serveur : le coup est transmis au joueur concerne
		if (HasAuthority() && !bDead)
		{
			ClientReceiveAttack(Damage, SanityDamage, Source, SourceName);
		}
		return;
	}
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
	bSwimming = false;
	bDiving = false;
	bMantling = false;
	GetCharacterMovement()->DisableMovement();
	PlaySound2D(TEXT("S_Death"), 1.f);
	if (!HasAuthority())
	{
		ServerSetDead(true);
	}
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->HandlePlayerDeath();
	}
}

void ABRCharacter::ClientReceiveAttack_Implementation(float Damage, float SanityDamage, AActor* Source, const FString& SourceName)
{
	ReceiveAttack(Damage, SanityDamage, Source, SourceName);
}

void ABRCharacter::ServerSetState_Implementation(uint8 Flags, uint8 Hand, uint8 Lamp)
{
	NetFlags = Flags;
	NetHand = Hand;
	NetLamp = Lamp;
}

void ABRCharacter::ServerSetDead_Implementation(bool bInDead)
{
	const bool bWas = bDead;
	bDead = bInDead;
	if (bDead != bWas)
	{
		OnRep_Dead();
	}
}

void ABRCharacter::OnRep_Dead()
{
	if (IsLocallyControlled() || !bDead)
	{
		return;
	}
	// Un coequipier tombe : cri a sa position et message
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Death")))
		{
			UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), 0.8f, 1.f, 0.f, A->Attenuation(3500.f));
		}
	}
	const APlayerState* PS = GetPlayerState();
	ABRHUD::Notify(this, FString::Printf(TEXT("%s est \u00e0 terre."), PS ? *PS->GetPlayerName() : TEXT("Un explorateur")), 4.f,
		FLinearColor(1.f, 0.45f, 0.4f));
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
	Breath = 100.f;
	bSwimming = bDiving = bUnderwater = bMantling = false;
	DeathBlend = 0.f;
	KilledBy.Empty();
	KillerActor.Reset();
	ResetInventory();
	OnEquipmentChanged();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (bIsCrouched)
	{
		UnCrouch();
	}
	if (!HasAuthority())
	{
		ServerSetDead(false);
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
		ABRHUD::Notify(this, BRKeys::Expand(TEXT("Inventaire plein ! {Inventory} pour faire de la place.")), 2.5f, FLinearColor(1.f, 0.6f, 0.5f));
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
	bSwimming = bDiving = bUnderwater = bMantling = false;
	Breath = 100.f;
	WaterDepth = 0.f;
	bSwimHint = false;
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
		ABRHUD::Notify(this, BRKeys::Expand(TEXT("Il fait noir comme dans un four. {Flashlight} lampe  -  {NightVision} vision nocturne")), 5.f, FLinearColor(1.f, 0.85f, 0.6f));
	}
}

void ABRCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	// Atterrir dans l'eau : gerbe et grosse onde
	if (WaterDepth > 5.f && !bSwimming && IsLocallyControlled())
	{
		const float Impact = FMath::Abs(static_cast<float>(GetVelocity().Z));
		if (SplashCooldown <= 0.f)
		{
			PlaySound2D(TEXT("S_Splash"), FMath::Clamp(0.3f + Impact / 1200.f, 0.3f, 0.8f));
			SplashCooldown = 0.5f;
		}
		if (ABRWorld* W = ABRWorld::Get(this))
		{
			W->AddWaterRipple(GetActorLocation(), FMath::Clamp(1.2f + Impact / 400.f, 1.2f, 3.f));
		}
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

	if (!IsLocallyControlled())
	{
		TickRemote(Dt);
		return;
	}
	if (bRemoteView)
	{
		bRemoteView = false; // client : le controleur local vient d'arriver
		HeldVisual = EBRItem::Count;
		AppliedLamp = 255;
		OnEquipmentChanged();
	}

	UpdateEntityEffects();
	UpdateStats(Dt);
	UpdateWater(Dt);
	UpdateHiding();
	UpdateCamera(Dt);
	UpdateFlashlight(Dt);
	UpdateFocus();
	UpdateAudio(Dt);
	UpdatePostProcess(Dt);
	AnimateBody(Dt);
	SwimInput = FVector::ZeroVector;

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

	SyncNetState();
	SanityPressure = 0.f;
	ChaseTarget = 0.f;
}

void ABRCharacter::SyncNetState()
{
	if (GetNetMode() == NM_Standalone)
	{
		return;
	}
	const uint8 Flags = (IsFlashlightOn() ? 1 : 0) | (IsSprinting() ? 2 : 0) | (bSwimming ? 4 : 0);
	const uint8 Hand = static_cast<uint8>(GetEquipped(EBREquipSlot::Hand));
	const uint8 Lamp = LampSlot();
	if (Flags == NetFlags && Hand == NetHand && Lamp == NetLamp)
	{
		return;
	}
	NetFlags = Flags;
	NetHand = Hand;
	NetLamp = Lamp;
	if (!HasAuthority())
	{
		ServerSetState(Flags, Hand, Lamp);
	}
}

void ABRCharacter::UpdateEntityEffects()
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W || bDead || !GetWorld())
	{
		return;
	}
	const FVector Eye = GetEyeLocation();
	const FVector Dir = GetViewDirection();
	for (ABREntity* E : W->GetEntities())
	{
		if (!IsValid(E) || E->IsVanishing())
		{
			continue;
		}
		// Musique de poursuite : l'entite traque CE joueur
		if (E->GetChaseTarget() == this)
		{
			NotifyChase(E->GetChaseIntensity());
		}
		if (bHidden)
		{
			continue; // cache : ni vu, ni oppresse
		}
		const FBREntityInfo& I = E->MyInfo();
		const FVector Target = E->GetActorLocation();
		const float Dist = static_cast<float>(FVector::Dist(Target, GetActorLocation()));
		const bool bAura = Dist < I.AuraRadius && !E->IsLurking();
		const bool bJournal = Dist < 2500.f && !W->IsDiscovered(E->Kind);
		if (!bAura && !bJournal)
		{
			continue;
		}
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BREntityAura), false, this);
		Q.AddIgnoredActor(E);
		if (GetWorld()->LineTraceTestByChannel(Target + FVector(0.f, 0.f, I.HalfHeight * 0.5f), Eye, ECC_Visibility, Q))
		{
			continue;
		}
		if (bAura)
		{
			AddSanityPressure(I.Aura);
		}
		// La fiche du journal s'ouvre quand on la regarde bien en face (pas une masse informe)
		if (bJournal && FVector::DotProduct((Target - Eye).GetSafeNormal(), Dir) > 0.82f && E->IsRecognizable())
		{
			W->Discover(E->Kind);
		}
	}
}

void ABRCharacter::TickRemote(float Dt)
{
	// Pion d'un autre joueur (chez soi), ou copie serveur du pion d'un client
	if (!bRemoteView)
	{
		bRemoteView = true;
		bNightVision = false;
		if (InfraredLight)
		{
			InfraredLight->SetVisibility(false);
		}
		UpdateViewMode();
	}
	for (UAudioComponent* Loop : { HeartAudio.Get(), BreathAudio.Get(), ChaseAudio.Get(), UnderwaterAudio.Get() })
	{
		if (Loop && Loop->IsPlaying())
		{
			Loop->Stop();
		}
	}
	if (HasAuthority())
	{
		UpdateHiding(); // l'IA des entites (serveur) doit savoir s'il est cache
	}
	SetHeldVisual(static_cast<EBRItem>(NetHand));
	ApplyLamp(NetLamp);

	bSwimming = (NetFlags & 4) != 0;
	const ABRWorld* W = ABRWorld::Get(this);
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	WaterDepth = (W && W->IsLevelReady() && W->Def().bWater) ? FMath::Max(0.f, W->Def().WaterHeight - static_cast<float>(GetActorLocation().Z - Half)) : 0.f;

	// Sa lampe eclaire la ou il regarde
	const bool bLight = IsFlashlightOn();
	if (Flashlight)
	{
		Flashlight->SetVisibility(bLight);
		if (bLight)
		{
			const FRotator Aim = GetAimRotation();
			const FRotator YawOnly(0.f, Aim.Yaw, 0.f);
			Flashlight->SetWorldLocationAndRotation(GetEyeLocation() + YawOnly.RotateVector(FlashBase + ThirdPersonLightPush), Aim);
			Flashlight->SetIntensity(FlashCandelas * (NetLamp == 2 ? 0.55f : 1.f));
		}
	}

	// Ses pas (spatialises) et ses remous dans l'eau
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	RemoteStepTimer -= Dt;
	if (!bDead && Speed > 40.f && RemoteStepTimer <= 0.f && (bSwimming || (Move && Move->IsMovingOnGround())))
	{
		const bool bSprint = (NetFlags & 2) != 0;
		RemoteStepTimer = bSwimming ? 1.f : (bSprint ? 0.3f : (bIsCrouched ? 0.65f : 0.46f));
		if (UBRAssets* A = UBRAssets::Get(this))
		{
			if (USoundBase* S = A->Sound(bSwimming ? FName(TEXT("S_Swim")) : StepSoundName()))
			{
				const float Vol = bIsCrouched ? 0.25f : (bSprint ? 0.9f : 0.6f);
				UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation() - FVector(0.f, 0.f, Half), Vol, FMath::FRandRange(0.92f, 1.08f), 0.f,
					A->Attenuation(1800.f));
			}
		}
		if (ABRWorld* RW = ABRWorld::Get(this))
		{
			if (WaterDepth > 4.f)
			{
				RW->AddWaterRipple(GetActorLocation(), bSwimming ? 1.2f : 0.8f);
			}
		}
	}
	AnimateBody(Dt);
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
	// Dans l'eau on avance plus lentement (jusqu'a -40 % quand elle arrive a la taille)
	const float Wade = (!bSwimming && WaterDepth > 5.f) ? FMath::Lerp(1.f, 0.6f, FMath::Clamp(WaterDepth / 120.f, 0.f, 1.f)) : 1.f;
	const bool bFast = bWantsSprint && !bExhausted;
	GetCharacterMovement()->MaxWalkSpeed = (bFast ? SprintSpeed : WalkSpeed) * Wade;
	// L'eau freine les demarrages et prolonge les arrets (on la pousse, elle nous pousse)
	const float Drag = bSwimming ? 0.f : FMath::Clamp(WaterDepth / 100.f, 0.f, 1.f);
	GetCharacterMovement()->MaxAcceleration = FMath::Lerp(2048.f, 850.f, Drag);
	GetCharacterMovement()->BrakingDecelerationWalking = FMath::Lerp(1800.f, 650.f, Drag);
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed * Wade;
	GetCharacterMovement()->MaxFlySpeed = bFast ? SwimSprintSpeed : SwimSpeed;
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
	if (!Camera || !CameraBoom)
	{
		return;
	}
	const float TargetZ = (bIsCrouched && !bSwimming) ? CrouchEyeZ : StandEyeZ;
	CamZ = FMath::FInterpTo(CamZ, TargetZ, Dt, 10.f);

	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const bool bGrounded = GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround();
	float BobZ = 0.f;
	float BobY = 0.f;
	if (bSwimming && !bDead)
	{
		// Nage : on se balance doucement au rythme des brasses
		BobTime += Dt * (1.6f + 2.4f * FMath::Clamp(static_cast<float>(GetVelocity().Size()) / SwimSpeed, 0.f, 1.f));
		BobZ = FMath::Sin(BobTime) * 3.f;
		BobY = FMath::Cos(BobTime * 0.5f) * 1.5f;
	}
	else if (bGrounded && Speed > 15.f && !bDead)
	{
		const float Rate = 8.f * (Speed / WalkSpeed) * FMath::Lerp(1.f, 0.8f, FMath::Clamp(WaterDepth / 100.f, 0.f, 1.f));
		BobTime += Dt * FMath::Clamp(Rate, 4.f, 14.f);
		const float Amp = (IsSprinting() ? 3.2f : (bIsCrouched ? 1.2f : 2.f)) * (1.f + FMath::Clamp(WaterDepth / 100.f, 0.f, 1.f) * 0.4f);
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

	// Perche : a la 1re personne elle est a hauteur des yeux (longueur nulle), a la 3e elle recule derriere l'epaule
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CamZ + DeathDrop + (bThirdPerson ? -8.f : BobZ)));
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, bThirdPerson ? ThirdPersonArm : 0.f, Dt, 9.f);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, bThirdPerson ? ThirdPersonOffset : FVector::ZeroVector, Dt, 9.f);
	Camera->SetRelativeLocation(FVector(0.f, bThirdPerson ? 0.f : BobY, 0.f));

	// Roulis leger + vertige quand la sante mentale baisse
	const float Insanity = 1.f - Sanity / 100.f;
	const float Wobble = Insanity > 0.5f ? FMath::Sin(TimeAlive * 0.8f) * (Insanity - 0.5f) * 8.f : 0.f;
	const float SwimRoll = bSwimming ? FMath::Sin(BobTime * 0.5f) * 2.f : 0.f;
	Camera->SetRelativeRotation(FRotator(0.f, 0.f, Wobble + SwimRoll + (bDead ? FMath::Min(DeathTime, 1.f) * 25.f : 0.f)));
	Camera->SetFieldOfView(FBRSettings::Get().FOV + (IsSprinting() ? 4.f : 0.f) + FMath::Sin(TimeAlive * 0.6f) * Insanity * Insanity * 6.f);

	// Lampe : a la 3e personne elle reste sur le personnage (et non sur la camera qui recule)
	if (Flashlight)
	{
		if (bThirdPerson)
		{
			const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, CamZ);
			const FRotator YawOnly(0.f, GetViewRotation().Yaw, 0.f);
			Flashlight->SetWorldLocation(Eye + YawOnly.RotateVector(FlashBase + ThirdPersonLightPush));
		}
		else
		{
			Flashlight->SetRelativeLocation(FlashBase);
		}
	}

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
			ABRHUD::Notify(this, BRKeys::Expand(TEXT("Batterie vide : vision nocturne coup\u00e9e. {Battery} changer les piles")), 3.f, FLinearColor(1.f, 0.8f, 0.4f));
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
			ABRHUD::Notify(this, BRKeys::Expand(TEXT("La lampe s'\u00e9teint. {Battery} changer les piles")), 3.f, FLinearColor(1.f, 0.8f, 0.4f));
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
	const FVector Eye = GetEyeLocation();
	const FVector Dir = GetViewDirection();
	auto Accept = [this](AActor* A) -> bool
	{
		if (const ABRPickup* P = Cast<ABRPickup>(A))
		{
			FocusActor = A;
			FocusPrompt = P->GetPrompt();
			return true;
		}
		if (const ABRExit* E = Cast<ABRExit>(A))
		{
			if (E->IsInteractable())
			{
				FocusActor = A;
				FocusPrompt = E->GetPrompt();
				return true;
			}
		}
		return false;
	};

	// 1) Visee precise : petite sphere lancee dans l'axe du regard (depuis la camera a la 3e personne)
	FVector Start = Eye;
	float Len = InteractReach;
	if (bThirdPerson && Camera)
	{
		Start = Camera->GetComponentLocation();
		Len = static_cast<float>(FVector::Dist(Start, Eye)) + InteractReach;
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRFocus), false, this);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, Start + Dir * Len, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(7.f), Q)
		&& FVector::Dist(Hit.ImpactPoint, Eye) < InteractReach + 30.f && Accept(Hit.GetActor()))
	{
		return;
	}

	// 2) Tolerance : l'objet le plus proche de l'axe du regard, a portee et visible (pas de mur entre les deux)
	TArray<FOverlapResult> Overlaps;
	if (GetWorld()->OverlapMultiByChannel(Overlaps, Eye, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(InteractReach * 0.8f), Q))
	{
		AActor* Best = nullptr;
		float BestDot = 0.86f;
		for (const FOverlapResult& O : Overlaps)
		{
			AActor* A = O.GetActor();
			const UPrimitiveComponent* Comp = O.GetComponent();
			const ABRExit* E = Cast<ABRExit>(A);
			if (!A || !Comp || (!Cast<ABRPickup>(A) && !(E && E->IsInteractable())))
			{
				continue;
			}
			const FVector Target = Comp->Bounds.Origin;
			const float Dot = static_cast<float>(FVector::DotProduct((Target - Eye).GetSafeNormal(), Dir));
			if (Dot <= BestDot)
			{
				continue;
			}
			FCollisionQueryParams LosQ(SCENE_QUERY_STAT(BRFocusLos), false, this);
			LosQ.AddIgnoredActor(A);
			if (!GetWorld()->LineTraceTestByChannel(Eye, Target, ECC_WorldStatic, LosQ))
			{
				Best = A;
				BestDot = Dot;
			}
		}
		if (Best && Accept(Best))
		{
			return;
		}
	}

	// 3) Nage : se hisser hors du bassin
	if (bSwimming && IsNearPoolEdge())
	{
		FocusPrompt = BRKeys::Tag(EBRAction::Jump) + TEXT(" Sortir de l'eau");
		return;
	}

	// 4) Cachette toute proche
	bool bNeedsCrouch = false;
	const ABRWorld* HW = ABRWorld::Get(this);
	if (!bHidden && HW && HW->FindHidingSpotNear(GetActorLocation(), 110.f, bNeedsCrouch))
	{
		FocusPrompt = bNeedsCrouch ? BRKeys::Tag(EBRAction::Crouch) + TEXT(" S'accroupir et se glisser dans le trou pour se cacher")
			: FString(TEXT("Entrer dans le placard pour se cacher"));
	}
}

void ABRCharacter::UpdateHiding()
{
	const bool bWas = bHidden;
	const ABRWorld* W = ABRWorld::Get(this);
	bHidden = !bDead && W && W->IsInHidingSpot(GetActorLocation(), bIsCrouched);
	if (bHidden && !bWas && IsLocallyControlled())
	{
		PlaySound2D(TEXT("S_ItemMove"), 0.35f);
		if (!bHideHint)
		{
			bHideHint = true;
			ABRHUD::Notify(this, TEXT("Vous \u00eates cach\u00e9 : les entit\u00e9s ne vous voient plus. Restez immobile et attendez qu'elles s'\u00e9loignent."),
				5.f, FLinearColor(0.75f, 0.9f, 1.f));
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
	if (USoundBase* S = A->Sound(StepSoundName()))
	{
		const float Vol = bIsCrouched ? 0.18f : (IsSprinting() ? 0.75f : 0.42f);
		UGameplayStatics::PlaySound2D(this, S, Vol, FMath::FRandRange(0.92f, 1.08f));
	}
}

FName ABRCharacter::StepSoundName() const
{
	const ABRWorld* W = ABRWorld::Get(this);
	const EBRStep Type = (W && W->IsLevelReady()) ? W->Def().Step : StepType;
	const TCHAR* Kind = TEXT("Carpet");
	switch (Type)
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
	return FName(*FString::Printf(TEXT("S_Step_%s_%d"), Kind, FMath::RandRange(1, 4)));
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
	if (UnderwaterAudio && UnderwaterAudio->Sound)
	{
		if (!UnderwaterAudio->IsPlaying())
		{
			UnderwaterAudio->Play();
		}
		UnderwaterAudio->SetVolumeMultiplier(FMath::Max(0.001f, UnderBlend * 0.9f));
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

	// Sous l'eau (camera immergee) : teinte turquoise, brouillard dense, bords flous ; manque d'air : vision qui se resserre
	const bool bCamUnder = Camera->GetComponentLocation().Z < WaterZ - 1.f;
	UnderBlend = FMath::FInterpTo(UnderBlend, bCamUnder ? 1.f : 0.f, Dt, 10.f);
	if (ABRWorld* FogWorld = ABRWorld::Get(this))
	{
		FogWorld->SetUnderwater(UnderBlend);
	}
	const float Choke = (!bDead && Breath < 35.f) ? (35.f - Breath) / 35.f : 0.f;

	FPostProcessSettings& S = Camera->PostProcessSettings;
	Camera->PostProcessBlendWeight = 1.f;

	// Effet camescope desactive : ni aberration de l'objectif, ni grain, ni salete, vignettage leger
	const bool bVHS = Set.bVHSEffect;
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = (bVHS ? 0.4f : 0.f) + Insanity * Insanity * 4.f + Glitch * 8.f + DamageFlash * 3.f + (bNV ? 1.5f : 0.f) + UnderBlend * 1.5f + Choke * 2.f;

	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = ((Set.bFilmGrain && bVHS) ? (D ? D->Grain : 0.25f) : 0.f) + Insanity * 0.5f + Glitch * 0.8f + (bNV ? 0.7f : 0.f);

	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = (D ? D->Vignette : 0.45f) * (bVHS ? 1.f : 0.4f) + (bHidden ? 0.45f : 0.f) + Insanity * 0.5f + DamageFlash * 0.6f + Dead * 0.8f + (bNV ? 0.5f : 0.f) + UnderBlend * 0.6f
		+ Choke * 0.9f;

	float Sat = (D ? D->Saturation : 1.f) * FMath::Lerp(1.f, 0.45f, FMath::Max3(Insanity * Insanity, Dead, Choke * 0.6f));
	if (bNV)
	{
		Sat = 0.f;
	}
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);

	FLinearColor Tint = D ? D->SceneTint : FLinearColor::White;
	Tint = FMath::Lerp(Tint, FLinearColor(0.45f, 0.9f, 0.82f), UnderBlend * 0.8f);
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
	S.bOverride_BloomIntensity = bNV || UnderBlend > 0.01f;
	S.BloomIntensity = bNV ? 2.f : FMath::Lerp(D ? D->Bloom : 0.6f, 2.2f, UnderBlend);

	// Salete sur l'objectif du camescope (visible dans les halos des neons)
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (UTexture* Dirt = A->Texture(TEXT("T_LensDirt")))
		{
			S.bOverride_BloomDirtMask = true;
			S.BloomDirtMask = Dirt;
			S.bOverride_BloomDirtMaskIntensity = true;
			S.BloomDirtMaskIntensity = !bVHS ? 0.f : (HasCamcorderInHand() ? 8.f : 3.f);
		}
	}

	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;
}

// =====================================================================================================================
// Corps (combinaison hazmat articulee) et vue a la 3e personne
// =====================================================================================================================

void ABRCharacter::BuildBody()
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (bHasBody || !Capsule || !UBRAssets::Get(this))
	{
		return;
	}
	bHasBody = true;
	BodyRoot = NewObject<USceneComponent>(this, TEXT("BodyRoot"));
	// Sur le maillage (vide) du Character : le moteur y applique le lissage des positions recues du reseau
	BodyRoot->SetupAttachment(GetMesh() ? static_cast<USceneComponent*>(GetMesh()) : static_cast<USceneComponent*>(Capsule));
	BodyRoot->RegisterComponent();
	BodyFeet = NewObject<USceneComponent>(this, TEXT("BodyFeet"));
	BodyFeet->SetupAttachment(BodyRoot);
	BodyFeet->SetRelativeLocation(FVector(0.f, 0.f, -Capsule->GetUnscaledCapsuleHalfHeight()));
	BodyFeet->RegisterComponent();

	// Pieces de la combinaison fournie (Tools/Blender/import_user_models.py) ; a defaut, des boites jaunes
	TMap<FString, FLinearColor> Fallback;
	Fallback.Add(TEXT("FallbackHazmat"), FLinearColor(0.75f, 0.6f, 0.08f));
	Body = BRRig::BuildHumanoid(this, BodyFeet, TEXT("SM_Hazmat"), FBRHumanoidSpec::Hazmat(), &Fallback, BodyComponents, true);

	// La tete et les bras suivent le buste (penche en avant quand on court / s'accroupit)
	const FAttachmentTransformRules Keep(EAttachmentRule::KeepWorld, false);
	if (Body.Torso)
	{
		for (USceneComponent* Part : { Body.Head, Body.UpperArm[0], Body.UpperArm[1] })
		{
			if (Part)
			{
				Part->AttachToComponent(Body.Torso, Keep);
			}
		}
	}

	// Objet tenu dans la main droite (visible a la 3e personne)
	HeldMesh = NewObject<UStaticMeshComponent>(this, TEXT("HeldMesh"));
	HeldMesh->SetupAttachment(Body.LowerArm[1] ? Body.LowerArm[1] : BodyFeet.Get());
	HeldMesh->SetRelativeLocation(FVector(3.f, 0.f, -29.f));
	HeldMesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	HeldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldMesh->RegisterComponent();
	HeldVisual = EBRItem::Count;
}

void ABRCharacter::UpdateViewMode()
{
	// 1re personne : le corps est entierement masque (sinon il couperait le faisceau de la lampe portee a la ceinture)
	const bool bShow = ShowsBody();
	for (UPrimitiveComponent* P : Body.Meshes)
	{
		if (P)
		{
			P->SetVisibility(bShow);
			P->SetCastShadow(bShow);
		}
	}
	if (HeldMesh)
	{
		HeldMesh->SetVisibility(bShow && HeldMesh->GetStaticMesh() != nullptr);
		HeldMesh->SetCastShadow(bShow);
	}
	if (HandMesh)
	{
		HandMesh->SetVisibility(!bShow && HandMesh->GetStaticMesh() != nullptr);
	}
	if (CameraBoom)
	{
		// La camera recule derriere l'epaule mais ne traverse pas les murs
		CameraBoom->bDoCollisionTest = bShow;
	}
}

void ABRCharacter::ToggleThirdPerson()
{
	if (bDead || bInputLocked)
	{
		return;
	}
	bThirdPerson = !bThirdPerson;
	UpdateViewMode();
	ABRHUD::Notify(this, bThirdPerson ? FString(TEXT("Vue \u00e0 la 3e personne")) : FString(TEXT("Vue \u00e0 la 1re personne")), 1.5f,
		FLinearColor(0.85f, 0.9f, 1.f));
}

void ABRCharacter::AnimateBody(float Dt)
{
	// Le corps n'est visible (et anime) qu'a la 3e personne
	if (!bHasBody || !BodyRoot || !BodyFeet || !Body.Torso || !ShowsBody())
	{
		return;
	}
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const FBRHumanoidSpec Spec = FBRHumanoidSpec::Hazmat();
	const FVector Vel = GetVelocity();
	const float Speed2D = static_cast<float>(Vel.Size2D());
	const float FwdSpeed = static_cast<float>(FVector::DotProduct(Vel, GetActorForwardVector()));
	const bool bGrounded = Move && Move->IsMovingOnGround();

	CrouchBlend = FMath::FInterpTo(CrouchBlend, (bIsCrouched && !bSwimming) ? 1.f : 0.f, Dt, 10.f);
	SwimBlend = FMath::FInterpTo(SwimBlend, bSwimming ? 1.f : 0.f, Dt, 5.f);
	AirBlend = FMath::FInterpTo(AirBlend, (!bGrounded && !bSwimming && !bDead) ? 1.f : 0.f, Dt, 6.f);
	DeathBlend = FMath::FInterpTo(DeathBlend, bDead ? 1.f : 0.f, Dt, 3.f);
	const float SwimMove = bSwimming ? FMath::Clamp(static_cast<float>(Vel.Size()) / SwimSpeed, 0.f, 1.f) : 0.f;
	const float Gait = bSwimming ? 0.f : FMath::Clamp(Speed2D / WalkSpeed, 0.f, 1.6f) * (1.f - 0.4f * CrouchBlend);
	const float Wade = bSwimming ? 0.f : FMath::Clamp((WaterDepth - 15.f) / 60.f, 0.f, 1.f);

	// Phase du cycle : un pas complet (2 enjambees) ~ 2*PI*45 cm ; a reculons le cycle s'inverse
	if (bSwimming)
	{
		BodyAnim += Dt * (2.2f + 2.6f * SwimMove);
	}
	else if (Speed2D > 10.f)
	{
		BodyAnim += Dt * (Speed2D / 45.f) * (FwdSpeed < -10.f ? -1.f : 1.f);
	}
	BodyAnim = FMath::Fmod(BodyAnim, 2.f * PI * 64.f);

	FRotator Thigh[2], Shin[2], Upper[2], Lower[2];
	for (int32 i = 0; i < 2; ++i)
	{
		const float Ph = BodyAnim + (i == 0 ? 0.f : PI);
		const float Out = i == 0 ? 1.f : -1.f; // roulis qui ecarte le membre du corps

		// ---- A pied : marche / course, accroupi, en l'air, dans l'eau jusqu'aux genoux ----
		float TP = Spec.LegAmp * Gait * FMath::Sin(Ph) + 80.f * CrouchBlend + 22.f * AirBlend;
		float SP = -Spec.LegAmp * 1.3f * Gait * FMath::Max(0.f, FMath::Cos(Ph)) - 115.f * CrouchBlend - 38.f * AirBlend;
		float UP = -Spec.ArmAmp * Gait * FMath::Sin(Ph) + 18.f * CrouchBlend + 10.f * Wade;
		float UR = Out * (16.f * AirBlend + 20.f * Wade);
		float LP = 8.f + 14.f * FMath::Min(Gait, 1.f) + 25.f * CrouchBlend;

		// ---- Nage : sur place (godille + pedalage) ou brasse ----
		if (SwimBlend > 0.01f)
		{
			const float T = BodyAnim;
			// Sur place
			const float TreadUP = 35.f + 15.f * FMath::Sin(2.f * T);
			const float TreadUR = Out * (50.f + 18.f * FMath::Sin(2.f * T));
			const float TreadLP = 40.f;
			const float TreadTP = 28.f * FMath::Sin(1.6f * T + (i == 0 ? 0.f : PI)) + 10.f;
			const float TreadSP = -35.f * FMath::Max(0.f, FMath::Cos(1.6f * T + (i == 0 ? 0.f : PI))) - 15.f;
			// Brasse : bras tendus vers l'avant qui s'ecartent, jambes en grenouille
			const float Pull = FMath::Max(0.f, FMath::Sin(T));
			const float Kick = FMath::Max(0.f, FMath::Sin(T + PI));
			const float StrokeUP = 165.f - 60.f * Pull;
			const float StrokeUR = Out * (15.f + 55.f * Pull);
			const float StrokeLP = 15.f + 55.f * Pull;
			const float StrokeTP = 10.f + 40.f * Kick;
			const float StrokeSP = -15.f - 75.f * Kick;
			const float SUP = FMath::Lerp(TreadUP, StrokeUP, SwimMove);
			const float SUR = FMath::Lerp(TreadUR, StrokeUR, SwimMove);
			const float SLP = FMath::Lerp(TreadLP, StrokeLP, SwimMove);
			const float STP = FMath::Lerp(TreadTP, StrokeTP, SwimMove);
			const float SSP = FMath::Lerp(TreadSP, StrokeSP, SwimMove);
			UP = FMath::Lerp(UP, SUP, SwimBlend);
			UR = FMath::Lerp(UR, SUR, SwimBlend);
			LP = FMath::Lerp(LP, SLP, SwimBlend);
			TP = FMath::Lerp(TP, STP, SwimBlend);
			SP = FMath::Lerp(SP, SSP, SwimBlend);
		}
		Thigh[i] = FRotator(TP, 0.f, Out * 4.f * SwimBlend * SwimMove);
		Shin[i] = FRotator(SP, 0.f, 0.f);
		Upper[i] = FRotator(UP, 0.f, UR);
		Lower[i] = FRotator(LP, 0.f, 0.f);
	}

	// Objet tenu : avant-bras droit a l'horizontale, qui suit la visee
	const float ViewPitch = FMath::Clamp(static_cast<float>(FRotator::NormalizeAxis(GetAimRotation().Pitch)), -60.f, 60.f);
	const float TorsoPitch = -20.f * CrouchBlend - 7.f * (IsSprinting() ? 1.f : 0.f) * (1.f - SwimBlend);
	if (HeldMesh && HeldMesh->GetStaticMesh())
	{
		const float Hold = 1.f - SwimBlend;
		Upper[1] = FMath::Lerp(Upper[1], FRotator(28.f + ViewPitch * 0.8f - TorsoPitch, 0.f, -6.f), Hold);
		Lower[1] = FMath::Lerp(Lower[1], FRotator(62.f, 0.f, 0.f), Hold);
	}

	for (int32 i = 0; i < 2; ++i)
	{
		if (Body.Thigh[i]) { Body.Thigh[i]->SetRelativeRotation(Thigh[i]); }
		if (Body.Shin[i]) { Body.Shin[i]->SetRelativeRotation(Shin[i]); }
		if (Body.UpperArm[i]) { Body.UpperArm[i]->SetRelativeRotation(Upper[i]); }
		if (Body.LowerArm[i]) { Body.LowerArm[i]->SetRelativeRotation(Lower[i]); }
	}
	Body.Torso->SetRelativeRotation(FRotator(TorsoPitch, 0.f, 0.f));

	// Corps entier : penche a l'horizontale pendant la brasse, s'effondre a la mort
	const float SwimPitch = -72.f * SwimMove * SwimBlend;
	if (Body.Head)
	{
		// La tete regarde ou vise le joueur (et se redresse quand on nage a plat ventre)
		Body.Head->SetRelativeRotation(FRotator(ViewPitch * 0.6f - TorsoPitch - SwimPitch * 0.7f, 0.f, 0.f));
	}
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	BodyRoot->SetRelativeRotation(FRotator(SwimPitch, 0.f, 80.f * DeathBlend));
	BodyRoot->SetRelativeLocation(FVector(0.f, 0.f, 35.f * SwimMove * SwimBlend - 60.f * DeathBlend));
	// Pieds au sol : la capsule raccourcit en position accroupie, et la pose plie les jambes (~42 cm)
	// (le maillage du Character remonte de la difference de hauteur de capsule quand on s'accroupit : on la retire)
	const float MeshLift = (GetMesh() && BodyRoot->GetAttachParent() == GetMesh()) ? static_cast<float>(GetBaseTranslationOffset().Z) : 0.f;
	BodyFeet->SetRelativeLocation(FVector(0.f, 0.f, -Half - 42.f * CrouchBlend + 8.f * AirBlend - MeshLift));
}

// =====================================================================================================================
// Eau : marche ralentie, nage, plongee, apnee
// =====================================================================================================================

void ABRCharacter::StartSwimming()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bSwimming || !Move || bDead)
	{
		return;
	}
	bSwimming = true;
	bDiving = false;
	EdgePush = 0.f;
	if (bIsCrouched)
	{
		UnCrouch();
	}
	const float Impact = FMath::Abs(static_cast<float>(Move->Velocity.Z));
	Move->SetMovementMode(MOVE_Flying);
	Move->Velocity.Z *= 0.3f;
	Move->BrakingDecelerationFlying = 450.f;
	Move->MaxFlySpeed = SwimSpeed;
	if (SplashCooldown <= 0.f)
	{
		PlaySound2D(TEXT("S_Splash"), FMath::Clamp(0.35f + Impact / 900.f, 0.35f, 1.f));
		SplashCooldown = 0.8f;
	}
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->AddWaterRipple(GetActorLocation(), FMath::Clamp(1.5f + Impact / 300.f, 1.5f, 3.5f));
	}
	if (!bSwimHint)
	{
		bSwimHint = true;
		ABRHUD::Notify(this, BRKeys::Expand(TEXT("Vous nagez : le regard guide la nage.  {Jump} remonter / sortir au bord  -  {Crouch} plonger  -  {Sprint} nager vite")),
			6.f, FLinearColor(0.7f, 0.95f, 1.f));
	}
}

void ABRCharacter::StopSwimming()
{
	if (!bSwimming)
	{
		return;
	}
	bSwimming = false;
	bDiving = false;
	EdgePush = 0.f;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Move && !bDead && Move->MovementMode == MOVE_Flying)
	{
		Move->SetMovementMode(MOVE_Falling);
	}
}

bool ABRCharacter::IsNearPoolEdge() const
{
	const ABRWorld* W = ABRWorld::Get(this);
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!bSwimming || !W || !Capsule || !GetWorld())
	{
		return false;
	}
	const FVector L = GetActorLocation();
	// Il faut etre a la surface (la tete hors de l'eau)
	if (L.Z + CamZ < WaterZ - 25.f)
	{
		return false;
	}
	const FVector Fwd = FRotator(0.f, GetActorRotation().Yaw, 0.f).Vector();
	const float Reach = Capsule->GetScaledCapsuleRadius() + 45.f;
	const float EdgeZ = W->FloorZAt(L + Fwd * Reach);
	if (EdgeZ < W->FloorZAt(L) + 100.f)
	{
		return false;
	}
	// Pas de mur juste au-dessus du rebord
	const FVector Top(L.X, L.Y, EdgeZ + 60.f);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRPoolEdge), false, this);
	return !GetWorld()->LineTraceTestByChannel(Top, Top + Fwd * (Reach + 40.f), ECC_WorldStatic, Q);
}

void ABRCharacter::ClimbOutOfWater()
{
	const ABRWorld* W = ABRWorld::Get(this);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!bSwimming || !W || !Move)
	{
		return;
	}
	MantleDir = FRotator(0.f, GetActorRotation().Yaw, 0.f).Vector();
	MantleZ = W->FloorZAt(GetActorLocation() + MantleDir * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 45.f));
	StopSwimming();
	bMantling = true;
	MantleTime = 0.f;
	ClimbGrace = 1.5f;
	Move->SetMovementMode(MOVE_Flying);
	Stamina = FMath::Max(0.f, Stamina - 6.f);
	PlaySound2D(TEXT("S_Splash"), 0.45f);
	SplashCooldown = 1.f;
	if (ABRWorld* MW = ABRWorld::Get(this))
	{
		MW->AddWaterRipple(GetActorLocation(), 1.4f);
	}
}

void ABRCharacter::UpdateWater(float Dt)
{
	SplashCooldown = FMath::Max(0.f, SplashCooldown - Dt);
	ClimbGrace = FMath::Max(0.f, ClimbGrace - Dt);
	ABRWorld* W = ABRWorld::Get(this);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!W || !Move || W->IsTransitioning() || !W->Def().bWater)
	{
		WaterZ = -1.0e6f;
		WaterDepth = 0.f;
		bUnderwater = false;
		bMantling = false;
		StopSwimming();
		Breath = FMath::Min(100.f, Breath + 30.f * Dt);
		return;
	}

	WaterZ = W->Def().WaterHeight;
	const FVector L = GetActorLocation();
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	const float FloorZ = W->FloorZAt(L);
	WaterDepth = FMath::Max(0.f, WaterZ - static_cast<float>(L.Z - Half));
	const bool bDeep = WaterZ - FloorZ > 120.f;
	const bool bWasUnder = bUnderwater;
	bUnderwater = !bDead && L.Z + CamZ < WaterZ - 2.f;

	// Se hisser hors de l'eau : on monte le long de la paroi, puis on avance sur le rebord
	if (bMantling)
	{
		MantleTime += Dt;
		const float Bottom = static_cast<float>(L.Z) - Half;
		const bool bAbove = Bottom >= MantleZ + 4.f;
		Move->Velocity = bAbove ? MantleDir * 240.f : MantleDir * 25.f + FVector(0.f, 0.f, 320.f);
		if ((bAbove && FloorZ >= MantleZ - 1.f) || MantleTime > 1.4f || bDead)
		{
			bMantling = false;
			if (!bDead)
			{
				Move->SetMovementMode(MOVE_Falling);
			}
		}
	}

	// Entree / sortie de la nage
	if (!bSwimming && !bMantling && bDeep && ClimbGrace <= 0.f && !bDead && L.Z < WaterZ - 20.f)
	{
		StartSwimming();
	}
	else if (bSwimming && (!bDeep || bDead))
	{
		StopSwimming();
	}

	// Apnee
	if (bUnderwater && !bGodMode)
	{
		Breath = FMath::Max(0.f, Breath - 100.f / BreathSeconds * Dt);
		if (Breath <= 0.f)
		{
			Health -= 14.f * Dt;
			DamageFlash = FMath::Max(DamageFlash, 0.35f);
			LastDamageTime = TimeAlive;
			if (Health <= 0.f)
			{
				Die(TEXT("la noyade"), nullptr);
				return;
			}
		}
	}
	else
	{
		if (bWasUnder && Breath < 60.f)
		{
			PlaySound2D(TEXT("S_Gasp"), FMath::Lerp(0.9f, 0.4f, Breath / 60.f));
		}
		Breath = FMath::Min(100.f, Breath + 32.f * Dt);
	}
	if (bWasUnder != bUnderwater && SplashCooldown <= 0.f)
	{
		PlaySound2D(TEXT("S_Splash"), 0.3f);
		SplashCooldown = 0.5f;
		W->AddWaterRipple(L, 1.6f);
	}

	// Ondes a la surface : on fend l'eau en marchant, on la brasse en nageant, on la remue meme immobile
	const FVector Vel = GetVelocity();
	const bool bTouchesSurface = bSwimming ? (L.Z + CamZ > WaterZ - 45.f) : (WaterDepth > 4.f && L.Z - Half < WaterZ);
	RippleTimer -= Dt;
	if (bTouchesSurface && !bDead && RippleTimer <= 0.f)
	{
		const float Moving = FMath::Clamp(static_cast<float>(Vel.Size2D()) / WalkSpeed, 0.f, 1.5f);
		if (Moving > 0.08f)
		{
			RippleTimer = FMath::Lerp(0.42f, 0.2f, FMath::Min(Moving, 1.f));
			W->AddWaterRipple(L + Vel.GetSafeNormal2D() * 25.f, (bSwimming ? 1.1f : 0.7f) + 0.5f * Moving);
		}
		else
		{
			RippleTimer = bSwimming ? 0.9f : 1.6f;
			W->AddWaterRipple(L, bSwimming ? 0.45f : 0.2f);
		}
	}

	if (!bSwimming)
	{
		return;
	}

	// ---- Flottaison : on remonte doucement a la surface, Saut = remonter, Accroupi = plonger ----
	const float FloatZ = WaterZ - StandEyeZ + 14.f; // les yeux juste au-dessus de l'eau
	FVector V = Move->Velocity;
	float Accel = 0.f;
	if (bJumpHeld)
	{
		Accel = 650.f;
	}
	else if (bDiving)
	{
		Accel = -560.f;
	}
	else
	{
		Accel = FMath::Clamp((FloatZ - static_cast<float>(L.Z)) * 3.f, -250.f, 260.f) - static_cast<float>(V.Z) * 1.2f;
	}
	V.Z += Accel * Dt;
	// On ne jaillit pas de l'eau en nageant : pour sortir, il faut se hisser sur un rebord
	if (L.Z > FloatZ && V.Z > 0.f)
	{
		V.Z = FMath::Min(static_cast<float>(V.Z), (FloatZ - static_cast<float>(L.Z)) * 6.f);
	}
	V.Z = FMath::Clamp(static_cast<float>(V.Z), -SwimSprintSpeed, SwimSprintSpeed);
	Move->Velocity = V;
	if (bDiving && L.Z - Half < FloorZ + 15.f)
	{
		bDiving = false; // touche le fond
	}

	// Bruit des brasses (l'endurance baisse comme a la course quand on nage vite : voir UpdateStats)
	if (V.Size() > 60.f)
	{
		StrokeTimer += Dt;
		const float Period = IsSprinting() ? 0.7f : 1.05f;
		if (StrokeTimer > Period)
		{
			StrokeTimer = 0.f;
			PlaySound2D(TEXT("S_Swim"), bUnderwater ? 0.25f : FMath::FRandRange(0.4f, 0.55f));
		}
	}
	else
	{
		StrokeTimer = 0.f;
	}

	// Pousser vers un rebord le franchit tout seul (ou touche Saut)
	const FVector Fwd = FRotator(0.f, GetActorRotation().Yaw, 0.f).Vector();
	if (FVector::DotProduct(SwimInput.GetSafeNormal2D(), Fwd) > 0.5f && IsNearPoolEdge())
	{
		EdgePush += Dt;
		if (EdgePush > 0.45f)
		{
			ClimbOutOfWater();
		}
	}
	else
	{
		EdgePush = 0.f;
	}
}
