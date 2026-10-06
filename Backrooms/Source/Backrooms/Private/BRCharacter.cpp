#include "BRCharacter.h"
#include "BRMission.h"
#include "BRLoc.h"
#include "BRLevels.h"
#include "Backrooms.h"
#include "EngineUtils.h"
#include "BRAssets.h"
#include "BRItems.h"
#include "BRKeys.h"
#include "BRWorld.h"
#include "BRHUD.h"
#include "BRInteractables.h"
#include "BRPlayerController.h"
#include "BREntity.h"
#include "BRSave.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
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
	// v4.6 : au bord d'une fosse, on tombe des que le centre du corps depasse le bord de ~18 cm (par defaut, la capsule
	// tenait en equilibre sur l'arete jusqu'a presque tout son rayon)
	Move->PerchRadiusThreshold = 16.f;
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
	InfraredLight->SetIntensity(1800.f);
	InfraredLight->SetAttenuationRadius(2000.f);
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
	SetFlashlightRayTracedShadows(FBRSettings::Get().bRTShadows && ABRPlayerController::IsHardwareRayTracingAvailable());

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
	// v4.7 : l'etat de mort officiel va a toutes les machines, proprietaire compris (mort constatee par le serveur,
	// reanimation par un coequipier)
	DOREPLIFETIME(ABRCharacter, DeathState);
	DOREPLIFETIME(ABRCharacter, bLevelLoading);
}

void ABRCharacter::SetLevelLoading(bool bLoading)
{
	// Toujours transmis par le joueur local : la valeur du serveur peut differer (joueur marque en chargement a son apparition)
	bLevelLoading = bLoading;
	LevelLoadingSince = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	if (!HasAuthority() && IsLocallyControlled())
	{
		ServerSetLevelLoading(bLoading);
	}
}

bool ABRCharacter::IsLevelLoading() const
{
	return bLevelLoading && (!GetWorld() || GetWorld()->GetRealTimeSeconds() - LevelLoadingSince < 90.f);
}

void ABRCharacter::ServerSetLevelLoading_Implementation(bool bLoading)
{
	bLevelLoading = bLoading;
	LevelLoadingSince = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
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
	// Main libre (v4.2) : le camescope reste dans le sac, il filme tant qu'on l'a sur soi ; la lampe est a la ceinture
	Storage[0] = FBRItemSlot{ EBRItem::Camcorder, 1 };
	Equipment[static_cast<int32>(EBREquipSlot::Belt)] = FBRItemSlot{ EBRItem::Flashlight, 1 };
	// v4.12 : les objets mis de cote font partie de cet inventaire (un reveil les emporte aussi)
	Recovered.Reset();
}

BRTxn::FInv ABRCharacter::ToTxn() const
{
	static_assert(BRTxn::NumPockets == NumPockets && BRTxn::NumStorage == NumStorage && BRTxn::NumEquip == static_cast<int32>(EBREquipSlot::Count),
		"BRTxn : memes tailles d'inventaire que le personnage");
	static_assert(static_cast<int32>(EBRItem::Count) == BRTxn::ItemCount, "BRTxn : memes objets que EBRItem");
	BRTxn::FInv Inv;
	auto Copy = [](const TArray<FBRItemSlot>& From, BRTxn::FSlot* To, int32 N)
	{
		for (int32 I = 0; I < N; ++I)
		{
			if (From.IsValidIndex(I) && !From[I].IsEmpty())
			{
				To[I].Item = static_cast<uint8>(From[I].Item);
				To[I].Count = From[I].Count;
			}
		}
	};
	Copy(Pockets, Inv.Pockets, BRTxn::NumPockets);
	Copy(Storage, Inv.Storage, BRTxn::NumStorage);
	Copy(Equipment, Inv.Equip, BRTxn::NumEquip);
	return Inv;
}

void ABRCharacter::FromTxn(const BRTxn::FInv& Inv)
{
	auto Copy = [](const BRTxn::FSlot* From, TArray<FBRItemSlot>& To, int32 N)
	{
		To.SetNum(N);
		for (int32 I = 0; I < N; ++I)
		{
			To[I] = From[I].IsEmpty() ? FBRItemSlot() : FBRItemSlot{ static_cast<EBRItem>(From[I].Item), From[I].Count };
		}
	};
	Copy(Inv.Pockets, Pockets, BRTxn::NumPockets);
	Copy(Inv.Storage, Storage, BRTxn::NumStorage);
	Copy(Inv.Equip, Equipment, BRTxn::NumEquip);
}

uint8 ABRCharacter::GetInvEpoch() const
{
	// Serveur (hote, solo) : ses propres reveils. Client : reveils confirmes, plus le reveil local pas encore confirme
	// (l'inventaire est deja celui de la nouvelle vie)
	if (HasAuthority())
	{
		return DeathState.WakeCount;
	}
	return static_cast<uint8>(KnownWakeCount + PendingWakeReports);
}

int32 ABRCharacter::CountRecovered() const
{
	int32 N = 0;
	for (const FBRItemSlot& S : Recovered)
	{
		N += S.IsEmpty() ? 0 : S.Count;
	}
	return N;
}

int32 ABRCharacter::StowRecovered(bool bNotify)
{
	if (Recovered.Num() == 0)
	{
		return 0;
	}
	BRTxn::FRecovery R;
	for (int32 I = 0; I < Recovered.Num() && I < BRTxn::MaxRecovered; ++I)
	{
		R.Slots[I].Item = static_cast<uint8>(Recovered[I].Item);
		R.Slots[I].Count = Recovered[I].Count;
	}
	BRTxn::FInv Inv = ToTxn();
	bool bEquip = false;
	const int32 Stowed = R.Stow(Inv, &bEquip);
	if (Stowed <= 0)
	{
		return 0;
	}
	FromTxn(Inv);
	Recovered.Reset();
	for (const BRTxn::FSlot& S : R.Slots)
	{
		if (!S.IsEmpty())
		{
			Recovered.Add(FBRItemSlot{ static_cast<EBRItem>(S.Item), S.Count });
		}
	}
	if (bEquip)
	{
		OnEquipmentChanged();
	}
	if (bNotify && IsLocallyControlled())
	{
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.RecoveredStowed", "Objet mis de c\u00f4t\u00e9 rang\u00e9 dans l'inventaire.")), 2.5f, FLinearColor(0.75f, 1.f, 0.75f));
		PlaySound2D(TEXT("S_ItemMove"), 0.6f);
	}
	return Stowed;
}

bool ABRCharacter::CheckReservation(const BRTxn::FInv& After)
{
	// v4.12 : une place reste reservee a l'objet demande tant que la reponse de l'hote n'est pas arrivee (au plus 10 s :
	// ensuite, un objet accepte sans place irait dans la reserve "mis de cote")
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (PendingPickup.RequestId == 0 || Now - PendingPickup.Since > 10.f || BRTxn::KeepsRoom(After, static_cast<uint8>(PendingPickup.Item)))
	{
		return true;
	}
	++ReservedMovesRefused;
	ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Player.PickupReserved", "Place r\u00e9serv\u00e9e : {Item} est en cours de ramassage."),
		{ { TEXT("Item"), BRLoc::Arg(BRItems::Get(PendingPickup.Item).Name) } }), 2.5f, FLinearColor(1.f, 0.8f, 0.5f));
	return false;
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
	// v4.12 : les objets mis de cote comptent (l'hote les compte aussi)
	int32 N = BRTxn::Count(ToTxn(), static_cast<uint8>(Item));
	for (const FBRItemSlot& S : Recovered)
	{
		N += (S.Item == Item) ? S.Count : 0;
	}
	return N;
}

int32 ABRCharacter::RemoveItem(EBRItem Item, int32 Count)
{
	BRTxn::FInv Inv = ToTxn();
	int32 Removed = BRTxn::Remove(Inv, static_cast<uint8>(Item), Count);
	FromTxn(Inv);
	for (FBRItemSlot& S : Recovered)
	{
		while (Removed < Count && S.Item == Item && S.Count > 0)
		{
			S.Count -= 1;
			++Removed;
		}
	}
	Recovered.RemoveAll([](const FBRItemSlot& S) { return S.IsEmpty(); });
	return Removed;
}

int32 ABRCharacter::GetServerHealStock(EBRItem Item) const
{
	const int32* N = ServerHealStock.Find(static_cast<uint8>(Item));
	return bServerHealStockKnown ? (N ? *N : 0) : -1;
}

void ABRCharacter::CreditHealItem(EBRItem Item, int32 Count)
{
	if (HasAuthority() && (Item == EBRItem::AlmondWater || Item == EBRItem::Bandage))
	{
		ServerHealStock.FindOrAdd(static_cast<uint8>(Item)) += Count;
	}
}

int32 ABRCharacter::AddItem(EBRItem Item, int32 Count)
{
	// v4.12 : l'algorithme partage (piles, equipement libre compatible, cases vides) : le meme que la verification de place
	BRTxn::FInv Inv = ToTxn();
	bool bEquip = false;
	const int32 Left = BRTxn::Add(Inv, static_cast<uint8>(Item), Count, &bEquip);
	FromTxn(Inv);
	if (bEquip)
	{
		OnEquipmentChanged();
	}
	return Left;
}

bool ABRCharacter::StoreItem(EBRItem Item)
{
	return AddItem(Item, 1) == 0;
}

bool ABRCharacter::MoveItem(EBRSlotGroup FromGroup, int32 FromIndex, EBRSlotGroup ToGroup, int32 ToIndex)
{
	// v4.12 : regles partagees (BRTxn::Move), appliquees sur une copie : un deplacement qui prendrait la place reservee a
	// l'objet en cours de ramassage est refuse avant de toucher l'inventaire
	BRTxn::FInv Inv = ToTxn();
	const BRTxn::FSlot* FromSlot = Inv.Get(static_cast<BRTxn::EGroup>(FromGroup), FromIndex);
	const EBRItem Moving = FromSlot ? static_cast<EBRItem>(FromSlot->Item) : EBRItem::None;
	bool bEquip = false;
	const BRTxn::EMove Result = BRTxn::Move(Inv, static_cast<BRTxn::EGroup>(FromGroup), FromIndex, static_cast<BRTxn::EGroup>(ToGroup), ToIndex, &bEquip);
	if (Result == BRTxn::EMove::CannotEquip)
	{
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Player.ItemPeutAllerEmplacementToindex", "{Item} ne peut pas aller dans l'emplacement {ToIndex}."), { { TEXT("Item"), BRLoc::Arg(BRItems::Get(Moving).Name) }, { TEXT("ToIndex"), BRLoc::Arg(BRItems::SlotName(static_cast<EBREquipSlot>(ToIndex))) } }), 2.5f, FLinearColor(1.f, 0.7f, 0.5f));
		return false;
	}
	if (Result != BRTxn::EMove::Moved && Result != BRTxn::EMove::Stacked)
	{
		return false;
	}
	if (!CheckReservation(Inv))
	{
		return false;
	}
	FromTxn(Inv);
	PlaySound2D(TEXT("S_ItemMove"), 0.6f);
	if (bEquip)
	{
		OnEquipmentChanged();
	}
	StowRecovered(); // une pile completee peut liberer une case
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
			StowRecovered(); // une place s'est peut-etre liberee
		}
		return;
	}
	if (Info.Slot == EBREquipSlot::None)
	{
		return;
	}
	if (Group == EBRSlotGroup::Equipment)
	{
		// Desequiper : vers la premiere case libre (regles partagees ; la place reservee a un ramassage est gardee)
		BRTxn::FInv Inv = ToTxn();
		if (!BRTxn::Unequip(Inv, Index))
		{
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.InventairePlein", "Inventaire plein.")), 2.f, FLinearColor(1.f, 0.7f, 0.5f));
			return;
		}
		if (!CheckReservation(Inv))
		{
			return;
		}
		FromTxn(Inv);
		PlaySound2D(TEXT("S_ItemMove"), 0.6f);
		OnEquipmentChanged();
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
	ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Player.OutOfItem", "{Item} : il n'en reste plus."), { { TEXT("Item"), BRLoc::Arg(BRItems::Get(Item).Name) } }), 2.f, FLinearColor(1.f, 0.7f, 0.5f));
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
	case EBRItem::Bandage:
	{
		if (Item == EBRItem::AlmondWater && Sanity >= 99.f && Health >= 99.f)
		{
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.AvezBesoinInstant", "Vous n'en avez pas besoin pour l'instant.")), 2.f);
			return false;
		}
		if (Item == EBRItem::Bandage && Health >= 99.f)
		{
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.EtesBlesse", "Vous n'\u00eates pas bless\u00e9.")), 2.f);
			return false;
		}
		// v4.10 : meme delai que le serveur ; un refus ne consomme rien
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		if (Now - LocalLastHealTime < HealCooldown)
		{
			NotifyHealRefused(1);
			return false;
		}
		if (!HasAuthority())
		{
			// v4.10 : partie en reseau : l'objet n'est consomme et la sante changee qu'a la reponse du serveur
			// (ClientHealResult). Une seule demande a la fois : un second appui pendant l'attente ne part pas.
			if (PendingHealRequest != 0 && Now - PendingHealSince < 3.f)
			{
				NotifyHealRefused(4);
				return false;
			}
			NextHealRequest = static_cast<uint16>(NextHealRequest % 65535 + 1);
			PendingHealRequest = NextHealRequest;
			PendingHealSince = Now;
			LocalLastHealTime = Now;
			const ABRWorld* HW = ABRWorld::Get(this);
			PendingHealLevel = HW ? HW->GetLevelSerial() : 0;
			ServerRequestHeal(static_cast<uint8>(Item), PendingHealRequest, PendingHealLevel, GetInvEpoch());
			return false; // consomme a l'acceptation
		}
		// Hote ou partie solo : la sante officielle est la sienne
		if (bDead || DeathState.bDead || bServerDying)
		{
			NotifyHealRefused(2);
			return false;
		}
		LocalLastHealTime = Now;
		ServerLastHealTime = Now;
		++HealthRev;
		AckHealthRev = HealthRev;
		ApplyHealAccepted(Item, FMath::Min(100.f, Health + (Item == EBRItem::Bandage ? 35.f : 10.f)));
		return true; // consomme par l'appelant
	}
	case EBRItem::Battery:
		if (Battery > 90.f)
		{
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.PilesSontEncorePleines", "Les piles sont encore pleines.")), 2.f);
			return false;
		}
		Battery = 100.f;
		PlaySound2D(TEXT("S_Battery"), 0.8f);
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.PilesRemplacees", "Piles remplac\u00e9es.")), 2.f);
		return true;
	case EBRItem::EnergyBar:
		Stamina = 100.f;
		bExhausted = false;
		EnergyBoost = 30.f;
		PlaySound2D(TEXT("S_Eat"), 0.9f);
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.RegainEnergie", "Un regain d'\u00e9nergie !")), 2.f, FLinearColor(1.f, 0.9f, 0.6f));
		return true;
	default:
		return false;
	}
}

void ABRCharacter::OnEquipmentChanged()
{
	const EBRItem InHand = GetEquipped(EBREquipSlot::Hand);
	if (!HasCamcorder() && bNightVision)
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
		HandMesh->SetStaticMesh(M);
		HandMesh->SetRelativeLocation(Pos);
		HandMesh->SetRelativeRotation(Rot);
		HandMesh->SetVisibility(M != nullptr);
		if (A && M)
		{
			TArray<UMaterialInstanceDynamic*> Glows;
			A->ApplySlots(HandMesh, nullptr, true, &Glows, 0.05f);
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
	if (bInputLocked || bDead || ScareKind >= 0)
	{
		return;
	}
	if (bClimbing)
	{
		ClimbInput = FMath::Clamp(static_cast<float>(Value.Y), -1.f, 1.f);
		return;
	}
	if (bDevFly)
	{
		// Vol libre (mode developpeur) : on avance dans la direction du regard
		const FRotator View = GetViewRotation();
		AddMovementInput(View.Vector(), Value.Y);
		AddMovementInput(FRotator(0.f, View.Yaw, 0.f).RotateVector(FVector(0.f, 1.f, 0.f)), Value.X);
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
	if (bInputLocked || bDead || !Controller || ScareKind >= 0)
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
	if (bDead && bPressed)
	{
		// A terre en cooperation : abandonner, se reveiller tout de suite au point de depart
		if (ABRWorld* W = ABRWorld::Get(this))
		{
			if (W->IsNetGame())
			{
				W->GiveUpDowned();
			}
		}
		return;
	}
	if (bInputLocked || bDead)
	{
		return;
	}
	if (bClimbing)
	{
		if (bPressed)
		{
			StopClimb(); // lacher l'echelle
		}
		return;
	}
	if (bDevFly)
	{
		return; // monter : voir Tick (touche maintenue)
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
	if (bInputLocked || bDead || bClimbing)
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

void ABRCharacter::SetFlashlightRayTracedShadows(bool bEnable)
{
	if (!Flashlight)
	{
		return;
	}
	const ECastRayTracedShadow::Type Want = bEnable ? ECastRayTracedShadow::Enabled : ECastRayTracedShadow::Disabled;
	if (Flashlight->CastRaytracedShadow != Want)
	{
		Flashlight->CastRaytracedShadow = Want;
		Flashlight->MarkRenderStateDirty();
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
		ABRHUD::Notify(this, BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Player.AucuneLampeEquipeeInventaireInventory", "Aucune lampe \u00e9quip\u00e9e (inventaire : {Inventory})."))), 2.5f, FLinearColor(1.f, 0.8f, 0.4f));
		return;
	}
	if (!bFlashlightOn && Battery <= 0.f)
	{
		ABRHUD::Notify(this, CountItem(EBRItem::Battery) > 0 ? BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Player.PilesVidesBatteryChanger", "Piles vides : {Battery} pour les changer"))) : FString(BR_STR(NSLOCTEXT("BR", "Player.PilesVidesFautTrouver", "Piles vides... il faut en trouver."))),
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
	if (!HasCamcorder())
	{
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.FautAvoirCamescopeVisionNocturne", "Il faut avoir le cam\u00e9scope sur vous pour la vision nocturne.")), 2.5f, FLinearColor(1.f, 0.8f, 0.4f));
		return;
	}
	if (!bNightVision && Battery <= 0.f)
	{
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.BatterieCamescopeVide", "Batterie du cam\u00e9scope vide.")), 2.5f, FLinearColor(1.f, 0.8f, 0.4f));
		return;
	}
	bNightVision = !bNightVision;
	UpdateViewMode();
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
	if (MissionDoc >= 0)
	{
		MissionDoc = -1; // v4.11 : ranger le document de mission
		return;
	}
	if (bInputLocked)
	{
		return;
	}
	if (bClimbing)
	{
		StopClimb(); // lacher l'echelle
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
	else if (ABRMissionDevice* D = Cast<ABRMissionDevice>(Target))
	{
		// v4.11 : observation et manivelle se maintiennent ; le reste agit tout de suite
		if (D->IsHoldAction())
		{
			HoldDevice = D;
			HoldTimer = 0.f;
			RequestMissionAction(D->GetIndex(), static_cast<uint8>(BRMission::EAction::Hold));
		}
		else
		{
			RequestMissionAction(D->GetIndex(), static_cast<uint8>(BRMission::EAction::Use));
		}
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

void ABRCharacter::ReceiveAttack(float Damage, float SanityDamage, AActor* Source)
{
	// v4.8 : le serveur decide seul : il retire la sante, decide si le coup tue, puis envoie l'effet au joueur touche.
	// Avant : le coup etait transmis au client, qui appliquait lui-meme les degats (le serveur ignorait sa sante).
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	if (DeathState.bDead || bServerDying || (IsLocallyControlled() && (bDead || bScareLethal)))
	{
		return;
	}
	ServerLastHitTime = GetWorld()->GetTimeSeconds();
	const bool bGod = IsLocallyControlled() ? bGodMode : ((NetFlags & 16) != 0 && ABRPlayerController::AreCheatsAllowed());
	// Gilet : connu du proprietaire (equipement), et du serveur par l'etat replique (bit 32)
	const bool bVest = IsLocallyControlled() ? GetEquipped(EBREquipSlot::Chest) == EBRItem::Vest : (NetFlags & 32) != 0;
	const float Applied = bGod ? 0.f : Damage * (Damage >= 100.f ? 1.f : (bVest ? 0.7f : 1.f));
	Health = FMath::Max(0.f, Health - Applied);
	++HealthRev; // v4.12 : revision commune (coups, soins, morts, reveils)
	const ABREntity* Attacker = Cast<ABREntity>(Source);
	const int8 Kind = Attacker ? static_cast<int8>(Attacker->Kind) : int8(-1);
	const bool bLethal = !bGod && Health <= 0.f;
	if (bLethal)
	{
		// Mort officielle a la fin du jumpscare (le joueur la signale) ; delai de secours si son message se perd
		bServerDying = true;
		ServerDyingTimer = 6.f;
		ServerDyingKiller = Kind;
	}
	if (IsLocallyControlled())
	{
		AckHealthRev = HealthRev;
		ApplyHitFeedback(Applied, SanityDamage, Source, Kind, Health, bLethal);
	}
	else
	{
		ClientHitFeedback(Applied, SanityDamage, Source, Kind, Health, HealthRev, DeathState.WakeCount, bLethal);
	}
}

void ABRCharacter::ClientHitFeedback_Implementation(float Damage, float SanityDamage, AActor* Source, int8 SourceKind, float NewHealth, uint16 Rev,
	uint8 Epoch, bool bLethal)
{
	// v4.12 : un coup d'une vie terminee, ou plus ancien qu'un etat deja recu (reveil, releve), ne touche rien : ni la sante,
	// ni un jumpscare ou une mort a contretemps
	if (!BRTxn::ShouldApplyHealth(Epoch, GetInvEpoch(), Rev, AckHealthRev))
	{
		++IgnoredStaleHealth;
		return;
	}
	AckHealthRev = Rev;
	LastServerHealth = NewHealth;
	ApplyHitFeedback(Damage, SanityDamage, Source, SourceKind, NewHealth, bLethal);
}

void ABRCharacter::ApplyHitFeedback(float Damage, float SanityDamage, AActor* Source, int8 SourceKind, float NewHealth, bool bLethal)
{
	if (bDead || bScareLethal)
	{
		return;
	}
	// Sante decidee par le serveur, appliquee telle quelle (le client ne retire rien lui-meme : pas de double application)
	Health = NewHealth;
	LastSentHealth = NewHealth;
	Sanity = FMath::Max(0.f, Sanity - SanityDamage);
	if (Damage <= 0.f && !bLethal)
	{
		return; // invincible (mode developpeur)
	}
	DamageFlash = 1.f;
	LastDamageTime = TimeAlive;
	if (Controller)
	{
		// v4.7 : secousse du coup recu, reglable (TREMBLEMENTS DE LA CAMERA)
		const float Shake = FBRSettings::Get().CameraShake;
		FRotator R = Controller->GetControlRotation();
		R.Pitch += FMath::FRandRange(2.f, 5.f) * Shake;
		R.Yaw += FMath::FRandRange(-4.f, 4.f) * Shake;
		Controller->SetControlRotation(R);
	}
	PlaySound2D(TEXT("S_Hurt"), 1.f);
	// v4.4 : jumpscare de l'entite qui frappe ; un coup mortel attend la fin du jumpscare.
	// v4.7 : selon la situation : toujours pour un coup mortel ; sinon seulement la premiere fois que cette espece
	// frappe dans le niveau, ou quand elle frappe hors du champ de vision (surprise reelle). Les autres coups gardent la
	// secousse, le son et la teinte : le jumpscare ne devient pas une routine qui cache le jeu.
	ABREntity* Attacker = Cast<ABREntity>(Source);
	if (Attacker)
	{
		const FVector ToAttacker = (Attacker->GetActorLocation() - GetEyeLocation()).GetSafeNormal();
		const bool bUnseen = FVector::DotProduct(GetViewDirection(), ToAttacker) < 0.35f;
		const bool bFirst = !ScaredKinds.Contains(static_cast<int32>(Attacker->Kind));
		if (ScareKind >= 0 || bLethal || (ScareCooldown <= 0.f && (bFirst || bUnseen)))
		{
			ScaredKinds.Add(static_cast<int32>(Attacker->Kind));
			PlayJumpscare(Attacker->Kind, Attacker, bLethal);
		}
	}
	if (bLethal)
	{
		if (ScareKind >= 0)
		{
			bScareLethal = true;
			ScareKillerKind = SourceKind;
			ScareKiller = Source;
		}
		else
		{
			DieOf(EBRDeathCause::Injury, SourceKind, Source);
		}
	}
}

void ABRCharacter::TickServerVitals(float Dt)
{
	if (!bServerDying)
	{
		return;
	}
	if (DeathState.bDead)
	{
		bServerDying = false;
		return;
	}
	ServerDyingTimer -= Dt;
	if (ServerDyingTimer <= 0.f)
	{
		// Le joueur n'a pas signale sa mort (message perdu, client modifie) : le serveur l'applique
		bServerDying = false;
		UE_LOG(LogBackrooms, Warning, TEXT("%s : coup mortel sans mort signalee : mort appliquee par le serveur"), *GetName());
		ServerApplyDeathState(true, EBRDeathCause::Injury, 1, FString(), ServerDyingKiller);
	}
}

bool ABRCharacter::ServerSyncVitals_Validate(float InHealth, uint16 AckRev)
{
	return FMath::IsFinite(InHealth) && InHealth >= -1.f && InHealth <= 101.f;
}

void ABRCharacter::ServerSyncVitals_Implementation(float InHealth, uint16 AckRev)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float Since = FMath::Clamp(Now - ServerLastVitalsTime, 0.f, 5.f);
	ServerLastVitalsTime = Now;
	// Un envoi parti avant le dernier coup, soin, mort ou reveil l'effacerait : ignore (le suivant portera la bonne revision)
	if (AckRev != HealthRev || DeathState.bDead || bServerDying)
	{
		return;
	}
	const float Wanted = FMath::Clamp(InHealth, 0.f, 100.f);
	// Baisse (noyade, folie) : acceptee. Hausse : recuperation lente seulement (1 point/s) ; les soins d'objets passent par
	// ServerRequestHeal
	Health = Wanted <= Health ? Wanted : FMath::Min(Wanted, Health + 1.2f * Since + 0.5f);
}

bool ABRCharacter::ServerRequestHeal_Validate(uint8 Item, uint16 RequestId, int32 LevelSerial, uint8 Epoch)
{
	return Item < static_cast<uint8>(EBRItem::Count) && RequestId != 0;
}

void ABRCharacter::ServerRequestHeal_Implementation(uint8 Item, uint16 RequestId, int32 LevelSerial, uint8 Epoch)
{
	// v4.11 : demande deja traitee (renvoi, meme ancienne) : la meme reponse, rien n'est applique une seconde fois
	for (const FBRTxnRecord& R : ServerHealHistory)
	{
		if (R.RequestId == RequestId && R.RequestId != 0)
		{
			ClientHealResult(R.Item, RequestId, R.Result == 0, R.Health, R.Rev, R.Result, static_cast<int32>(R.Key), R.Epoch);
			return;
		}
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const EBRItem What = static_cast<EBRItem>(Item);
	const float Amount = What == EBRItem::Bandage ? 35.f : (What == EBRItem::AlmondWater ? 10.f : 0.f);
	const ABRWorld* W = ABRWorld::Get(this);
	uint8 Reason = 0;
	if (Amount <= 0.f)
	{
		Reason = 5; // pas un objet de soin
	}
	else if (DeathState.bDead || bServerDying)
	{
		Reason = 2; // a terre, ou coup mortel en cours
	}
	else if (W && LevelSerial != W->GetLevelSerial())
	{
		Reason = 7; // demande faite dans un autre niveau : refusee, rien n'est consomme
	}
	else if (Epoch != DeathState.WakeCount)
	{
		Reason = 8; // v4.12 : demande faite avant un reveil (autre inventaire) : refusee, rien n'est consomme
	}
	else if (Now - ServerLastHealTime < HealCooldown - HealServerTolerance)
	{
		Reason = 1; // trop tot apres le soin precedent
	}
	else if (!bServerHealStockKnown)
	{
		Reason = 6; // v4.11 : inventaire pas encore declare a l'hote : refuse (avant : accepte sans verification)
	}
	else if (GetServerHealStock(What) <= 0)
	{
		Reason = 3; // objet que l'hote ne lui connait pas (deja utilise, jamais ramasse)
	}
	FBRTxnRecord& Rec = ServerHealHistory[ServerHealHistoryNext];
	ServerHealHistoryNext = (ServerHealHistoryNext + 1) % TxnHistory;
	Rec = FBRTxnRecord();
	Rec.RequestId = RequestId;
	Rec.Item = Item;
	Rec.Key = static_cast<uint64>(static_cast<uint32>(LevelSerial));
	Rec.Epoch = DeathState.WakeCount;
	if (Reason != 0)
	{
		Rec.Result = Reason;
		Rec.Health = Health;
		Rec.Rev = HealthRev;
		ServerLastHealRequest = RequestId;
		bServerLastHealAccepted = false;
		ServerLastHealReason = Reason;
		++ServerHealsRefused;
		ClientHealResult(Item, RequestId, false, Health, HealthRev, Reason, LevelSerial, Rec.Epoch);
		return;
	}
	// Une seule operation : sante officielle, revision de la sante, stock tenu par l'hote
	ServerLastHealTime = Now;
	Health = FMath::Min(100.f, Health + Amount);
	++HealthRev;
	ServerHealStock.FindOrAdd(Item) -= 1;
	Rec.Result = 0;
	Rec.Health = Health;
	Rec.Rev = HealthRev;
	ServerLastHealRequest = RequestId;
	bServerLastHealAccepted = true;
	ServerLastHealReason = 0;
	++ServerHealsAccepted;
	ClientHealResult(Item, RequestId, true, Health, HealthRev, 0, LevelSerial, Rec.Epoch);
}

bool ABRCharacter::WasApplied(const uint16 (&Applied)[TxnHistory], uint16 RequestId)
{
	for (const uint16 Id : Applied)
	{
		if (Id == RequestId && Id != 0)
		{
			return true;
		}
	}
	return false;
}

void ABRCharacter::RememberApplied(uint16 (&Applied)[TxnHistory], int32& Next, uint16 RequestId)
{
	Applied[Next] = RequestId;
	Next = (Next + 1) % TxnHistory;
}

void ABRCharacter::ClientHealResult_Implementation(uint8 Item, uint16 RequestId, bool bAccepted, float NewHealth, uint16 Rev, uint8 Reason, int32 LevelSerial,
	uint8 Epoch)
{
	// v4.12 : la quantite et la sante sont reconciliees separement (BRTxn::DecideHeal) :
	//   - une reponse deja recue n'est jamais appliquee deux fois (v4.11) ;
	//   - l'objet n'est retire que si le soin a ete accepte pour l'inventaire de cette vie (un reveil depuis : l'objet est
	//     parti avec l'ancien inventaire, aucun objet neuf n'est pris) ;
	//   - la sante n'est ecrite que si sa revision est plus recente que le dernier etat recu (coup, soin, mort, releve,
	//     reveil) : une reponse en retard n'ecrase jamais un etat plus recent (avant : 100 -> 35 dans un nouveau niveau) ;
	//   - aucun effet n'est rejoue pour une reponse d'un niveau precedent.
	const bool bRepeated = WasApplied(AppliedHealResults, RequestId);
	if (!bRepeated)
	{
		RememberApplied(AppliedHealResults, AppliedHealNext, RequestId);
	}
	const bool bWasPending = RequestId == PendingHealRequest;
	if (bWasPending)
	{
		PendingHealRequest = 0;
	}
	const ABRWorld* W = ABRWorld::Get(this);
	BRTxn::FHealIn In;
	In.bAccepted = bAccepted;
	In.bRepeated = bRepeated;
	In.bWasPending = bWasPending;
	In.bStaleLevel = W && LevelSerial != W->GetLevelSerial();
	In.RespEpoch = Epoch;
	In.CurEpoch = GetInvEpoch();
	In.RespRev = Rev;
	In.AckRev = AckHealthRev;
	const BRTxn::FHealOut Out = BRTxn::DecideHeal(In);
	if (Out.bIgnore)
	{
		++IgnoredRepeatedResults;
		return;
	}
	if (Out.bNotifyRefusal)
	{
		NotifyHealRefused(Reason);
	}
	if (!bAccepted)
	{
		return;
	}
	const EBRItem What = static_cast<EBRItem>(Item);
	if (Out.bRemoveItem && RemoveItem(What, 1) == 0)
	{
		UE_LOG(LogBackrooms, Warning, TEXT("Soin accepte par l'hote sans %s dans l'inventaire (demande %u)"), *BRItems::Get(What).Name.ToString(), RequestId);
	}
	if (Out.bSetHealth)
	{
		AckHealthRev = Rev;
		LastServerHealth = NewHealth;
		Health = FMath::Clamp(NewHealth, 0.f, 100.f);
		LastSentHealth = Health;
	}
	else
	{
		++IgnoredStaleHealth;
	}
	if (In.bStaleLevel || In.RespEpoch != In.CurEpoch)
	{
		++StaleResultsReconciled;
	}
	ApplyHealEffects(What, Out.bItemEffect, Out.bEffects);
}

void ABRCharacter::ApplyHealAccepted(EBRItem Item, float NewHealth)
{
	Health = FMath::Clamp(NewHealth, 0.f, 100.f);
	LastSentHealth = Health;
	ApplyHealEffects(Item, true, true);
}

void ABRCharacter::ApplyHealEffects(EBRItem Item, bool bItemEffect, bool bEffects)
{
	if (Item == EBRItem::AlmondWater)
	{
		if (bItemEffect)
		{
			Sanity = FMath::Min(100.f, Sanity + 40.f); // la sante mentale reste tenue par le joueur
		}
		if (bEffects)
		{
			PlaySound2D(TEXT("S_Drink"), 0.9f);
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.BuvezEauAmandeVotreEsprit", "Vous buvez de l'eau d'amande. Votre esprit s'\u00e9claircit.")), 3.f, FLinearColor(0.85f, 0.95f, 1.f));
		}
	}
	else if (bEffects)
	{
		PlaySound2D(TEXT("S_Bandage"), 0.9f);
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.BandezVosBlessures", "Vous bandez vos blessures.")), 2.5f, FLinearColor(0.9f, 0.95f, 0.9f));
	}
}

void ABRCharacter::NotifyHealRefused(uint8 Reason)
{
	FText Msg;
	switch (Reason)
	{
	case 1:
		Msg = NSLOCTEXT("BR", "Player.HealTooSoon", "Pas si vite : attendez un instant avant de vous soigner de nouveau.");
		break;
	case 2:
		Msg = NSLOCTEXT("BR", "Player.HealDown", "Impossible de vous soigner maintenant.");
		break;
	case 3:
		Msg = NSLOCTEXT("BR", "Player.HealNotOwned", "Soin refus\u00e9 par l'h\u00f4te : objet inconnu de la partie. Rien n'a \u00e9t\u00e9 utilis\u00e9.");
		break;
	case 4:
		Msg = NSLOCTEXT("BR", "Player.HealPending", "Soin en cours\u2026");
		break;
	case 6:
		Msg = NSLOCTEXT("BR", "Player.HealStockUnknown", "Soin refus\u00e9 : l'h\u00f4te n'a pas encore re\u00e7u votre inventaire. R\u00e9essayez dans un instant.");
		break;
	case 7:
		Msg = NSLOCTEXT("BR", "Player.HealStale", "Soin annul\u00e9 par le changement de niveau. Rien n'a \u00e9t\u00e9 utilis\u00e9.");
		break;
	case 8:
		Msg = NSLOCTEXT("BR", "Player.HealStaleLife", "Soin annul\u00e9 : vous vous \u00eates r\u00e9veill\u00e9 entre-temps. Rien n'a \u00e9t\u00e9 utilis\u00e9.");
		break;
	default:
		Msg = NSLOCTEXT("BR", "Player.HealRefused", "Soin refus\u00e9. Rien n'a \u00e9t\u00e9 utilis\u00e9.");
		break;
	}
	ABRHUD::Notify(this, Msg.ToString(), 2.f, FLinearColor(1.f, 0.75f, 0.55f));
}

void ABRCharacter::ServerDeclareHealStock_Implementation(uint8 Water, uint8 Bandages)
{
	// v4.11 : une seule declaration par session : l'inventaire avec lequel le joueur arrive (depart ou sauvegarde). Une
	// declaration repetee (chaque niveau en v4.10) ou en retard est ignoree : elle ne peut plus reintroduire des objets
	// deja consommes. Ensuite, seuls les ramassages acceptes et les soins acceptes par l'hote changent ce stock, et un
	// reveil apres une mort le ramene a l'equipement de depart. Plafond : ce que l'inventaire peut contenir.
	if (bServerHealStockKnown)
	{
		UE_LOG(LogBackrooms, Verbose, TEXT("%s : declaration du stock de soin ignoree (deja connue de l'hote)"), *GetName());
		return;
	}
	auto Cap = [](EBRItem Item)
	{
		return (NumPockets + NumStorage) * FMath::Max(1, BRItems::Get(Item).MaxStack);
	};
	ServerHealStock.FindOrAdd(static_cast<uint8>(EBRItem::AlmondWater)) = FMath::Min<int32>(Water, Cap(EBRItem::AlmondWater));
	ServerHealStock.FindOrAdd(static_cast<uint8>(EBRItem::Bandage)) = FMath::Min<int32>(Bandages, Cap(EBRItem::Bandage));
	bServerHealStockKnown = true;
}

bool ABRCharacter::ServerRequestPickup_Validate(uint64 PickupId, uint16 RequestId, int32 LevelSerial, uint8 ExpectedItem, uint8 Room, uint8 Epoch)
{
	return RequestId != 0 && ExpectedItem < static_cast<uint8>(EBRItem::Count);
}

void ABRCharacter::ServerRequestPickup_Implementation(uint64 PickupId, uint16 RequestId, int32 LevelSerial, uint8 ExpectedItem, uint8 Room, uint8 Epoch)
{
	// Demande deja servie (renvoi) : la meme reponse, sans second effet
	for (const FBRTxnRecord& R : ServerPickupHistory)
	{
		if (R.RequestId == RequestId && R.RequestId != 0 && R.Key == PickupId)
		{
			ClientPickupResult(PickupId, RequestId, LevelSerial, R.Item, R.Result, R.Epoch);
			return;
		}
	}
	ABRWorld* W = ABRWorld::Get(this);
	EBRItem Given = EBRItem::None;
	FVector Where = FVector::ZeroVector;
	const EBRPickupResult Result = W ? W->ServerTryCollect(this, PickupId, LevelSerial, ExpectedItem, Room, Epoch, Given, &Where) : EBRPickupResult::Unknown;
	FBRTxnRecord& Rec = ServerPickupHistory[ServerPickupHistoryNext];
	ServerPickupHistoryNext = (ServerPickupHistoryNext + 1) % TxnHistory;
	Rec = FBRTxnRecord();
	Rec.RequestId = RequestId;
	Rec.Key = PickupId;
	Rec.Item = static_cast<uint8>(Result == EBRPickupResult::Accepted ? Given : static_cast<EBRItem>(ExpectedItem));
	Rec.Result = static_cast<uint8>(Result);
	Rec.Epoch = DeathState.WakeCount;
	if (Result == EBRPickupResult::Accepted)
	{
		++ServerPickupsAccepted;
		// v4.12 : attribue, en attente de l'accuse de reception du joueur (rendu au monde s'il part avant)
		BRTxn::FLedgerEntry E;
		E.RequestId = RequestId;
		E.PickupId = PickupId;
		E.Item = Rec.Item;
		E.Epoch = Rec.Epoch;
		E.LevelSerial = LevelSerial;
		E.X = static_cast<float>(Where.X);
		E.Y = static_cast<float>(Where.Y);
		E.Z = static_cast<float>(Where.Z);
		E.bAwaitingAck = true;
		PickupLedger.Record(E);
	}
	else
	{
		++ServerPickupsRefused;
	}
	ClientPickupResult(PickupId, RequestId, LevelSerial, Rec.Item, Rec.Result, Rec.Epoch);
}

void ABRCharacter::ClientPickupResult_Implementation(uint64 PickupId, uint16 RequestId, int32 LevelSerial, uint8 Item, uint8 Result, uint8 Epoch)
{
	HandlePickupResult(PickupId, RequestId, LevelSerial, static_cast<EBRItem>(Item), static_cast<EBRPickupResult>(Result), Epoch);
}

void ABRCharacter::ServerAckPickup_Implementation(uint16 RequestId)
{
	PickupLedger.Ack(RequestId);
}

int32 ABRCharacter::CountUnackedPickups(int32 LevelSerial) const
{
	BRTxn::FLedgerEntry Out[BRTxn::FLedger::Size];
	return PickupLedger.Unacked(LevelSerial, Out, BRTxn::FLedger::Size);
}

void ABRCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// v4.12 : deconnexion d'un joueur (son pion est detruit) : un objet que l'hote lui a attribue sans accuse de reception
	// revient dans le monde, a sa place, ses effets compenses (il n'a jamais atteint son inventaire)
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed && !IsLocallyControlled())
	{
		if (ABRWorld* W = ABRWorld::Get(this))
		{
			BRTxn::FLedgerEntry Out[BRTxn::FLedger::Size];
			const int32 N = PickupLedger.Unacked(W->GetLevelSerial(), Out, BRTxn::FLedger::Size);
			for (int32 I = 0; I < N; ++I)
			{
				W->ServerReturnPickup(Out[I].PickupId, static_cast<EBRItem>(Out[I].Item), FVector(Out[I].X, Out[I].Y, Out[I].Z));
				PickupLedger.Ack(Out[I].RequestId);
			}
			ServerReturnedPickups += N;
		}
	}
	Super::EndPlay(EndPlayReason);
}

int32 ABRCharacter::RoomFor(EBRItem Item) const
{
	// v4.12 : la regle de l'ajout (piles, equipement libre compatible, cases vides), sur une copie
	return BRTxn::Capacity(ToTxn(), static_cast<uint8>(Item));
}

void ABRCharacter::RequestPickup(ABRPickup* Pickup)
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!Pickup || !W || bDead)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (PendingPickup.RequestId != 0 && Now - PendingPickup.Since < 4.f)
	{
		return; // une demande a la fois ; l'objet en attente reste estompe
	}
	const int32 Room = RoomFor(Pickup->Item);
	if (Room <= 0)
	{
		NotifyPickupRefused(EBRPickupResult::Full);
		return;
	}
	NextPickupRequest = static_cast<uint16>(NextPickupRequest % 65535 + 1);
	const uint16 RequestId = NextPickupRequest;
	const int32 Serial = W->GetLevelSerial();
	const uint8 Epoch = GetInvEpoch();
	PendingPickup.Id = Pickup->Id;
	PendingPickup.RequestId = RequestId;
	PendingPickup.Item = Pickup->Item;
	PendingPickup.LevelSerial = Serial;
	PendingPickup.Since = Now;
	if (HasAuthority())
	{
		// Hote ou partie solo : la meme transaction, appliquee tout de suite (pas d'accuse : rien ne circule)
		EBRItem Given = EBRItem::None;
		const EBRPickupResult Result = W->ServerTryCollect(this, Pickup->Id, Serial, static_cast<uint8>(Pickup->Item), static_cast<uint8>(FMath::Min(Room, 255)),
			Epoch, Given, nullptr);
		if (Result == EBRPickupResult::Accepted)
		{
			++ServerPickupsAccepted;
		}
		else
		{
			++ServerPickupsRefused;
		}
		HandlePickupResult(Pickup->Id, RequestId, Serial, Result == EBRPickupResult::Accepted ? Given : Pickup->Item, Result, DeathState.WakeCount);
		return;
	}
	Pickup->SetPending(true);
	ServerRequestPickup(Pickup->Id, RequestId, Serial, static_cast<uint8>(Pickup->Item), static_cast<uint8>(FMath::Min(Room, 255)), Epoch);
}

void ABRCharacter::HandlePickupResult(uint64 PickupId, uint16 RequestId, int32 LevelSerial, EBRItem Item, EBRPickupResult Result, uint8 Epoch)
{
	// v4.12 : decision partagee (BRTxn::DecidePickup) : une reponse repetee ne donne jamais un second objet ; un objet
	// attribue est range (inventaire, sinon reserve "mis de cote") et accuse aupres de l'hote ; attribue a une vie
	// terminee depuis (reveil), il est parti avec cet inventaire
	const bool bRepeated = WasApplied(AppliedPickupResults, RequestId);
	if (!bRepeated)
	{
		RememberApplied(AppliedPickupResults, AppliedPickupNext, RequestId);
	}
	const bool bMine = PendingPickup.RequestId == RequestId;
	if (bMine)
	{
		PendingPickup = FBRPendingPickup();
	}
	ABRWorld* W = ABRWorld::Get(this);
	const bool bStale = W && LevelSerial != W->GetLevelSerial();
	if (W && !bStale && Result != EBRPickupResult::Accepted)
	{
		for (TActorIterator<ABRPickup> It(GetWorld()); It; ++It)
		{
			if (It->Id == PickupId)
			{
				It->SetPending(false);
			}
		}
	}
	BRTxn::FPickupIn In;
	In.bAccepted = Result == EBRPickupResult::Accepted;
	In.bRepeated = bRepeated;
	In.bWasPending = bMine;
	In.bStaleLevel = bStale;
	In.RespEpoch = Epoch;
	In.CurEpoch = GetInvEpoch();
	const BRTxn::FPickupOut Out = BRTxn::DecidePickup(In);
	if (Out.bIgnore)
	{
		++IgnoredRepeatedResults;
		return;
	}
	if (Out.bAck && !HasAuthority())
	{
		ServerAckPickup(RequestId);
	}
	if (Out.bNotifyRefusal)
	{
		NotifyPickupRefused(Result);
	}
	if (!In.bAccepted)
	{
		return;
	}
	if (Out.bLostWithLife)
	{
		++LostWithLifeResults;
		UE_LOG(LogBackrooms, Log, TEXT("Ramassage %u accepte avant un reveil : parti avec l'inventaire de cette vie (%s)"), RequestId, *BRItems::Get(Item).Name.ToString());
		return;
	}
	if (bStale)
	{
		// Accepte dans le niveau precedent (reponse arrivee apres le changement) : l'objet est bien a ce joueur, il est
		// range sans rien rejouer dans le nouveau niveau
		++StaleResultsReconciled;
	}
	StorePickup(Item, Out.bEffects);
}

void ABRCharacter::StorePickup(EBRItem Item, bool bEffects)
{
	const int32 Left = AddItem(Item, 1);
	if (Left > 0)
	{
		// v4.12 : pas de place au moment de la reponse (inventaire change entre-temps, reponse tardive) : mis de cote,
		// jamais perdu. Il se range tout seul des qu'une place se libere
		BRTxn::FRecovery R;
		for (int32 I = 0; I < Recovered.Num() && I < BRTxn::MaxRecovered; ++I)
		{
			R.Slots[I].Item = static_cast<uint8>(Recovered[I].Item);
			R.Slots[I].Count = Recovered[I].Count;
		}
		const int32 NotKept = R.Put(static_cast<uint8>(Item), Left);
		Recovered.Reset();
		for (const BRTxn::FSlot& S : R.Slots)
		{
			if (!S.IsEmpty())
			{
				Recovered.Add(FBRItemSlot{ static_cast<EBRItem>(S.Item), S.Count });
			}
		}
		++RecoveredPickups;
		if (NotKept > 0)
		{
			UE_LOG(LogBackrooms, Error, TEXT("Reserve \"mis de cote\" pleine : %d %s non gardes"), NotKept, *BRItems::Get(Item).Name.ToString());
		}
		if (IsLocallyControlled())
		{
			ABRHUD::Notify(this, BRKeys::Expand(BRLoc::Fmt(NSLOCTEXT("BR", "Player.PickupSetAside", "{Item} : pas de place, mis de côté. Faites de la place ({Inventory}) : il se rangera tout seul."),
				{ { TEXT("Item"), BRLoc::Arg(BRItems::Get(Item).Name) } })), 4.f, FLinearColor(1.f, 0.85f, 0.5f));
		}
		return;
	}
	if (!bEffects || !IsLocallyControlled())
	{
		return;
	}
	const FBRItemInfo& Info = BRItems::Get(Item);
	if (Item == EBRItem::VHSTape)
	{
		// v4.11 : la cassette est comptee par l'hote dans la transaction de ramassage (plus de message separe)
		const ABRWorld* W = ABRWorld::Get(this);
		if (W && !(W->Def().bRequireObjectives && W->IsLegacyObjectives()))
		{
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.LoreTape", "Cassette VHS : un document facultatif, ajouté au journal.")), 3.f, FLinearColor(0.9f, 0.88f, 0.75f));
		}
	}
	else
	{
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Player.PickedUpCount", "+1 {Item}  ({Count})"), { { TEXT("Item"), BRLoc::Arg(Info.Name) }, { TEXT("Count"), BRLoc::Int(CountItem(Item)) } }), 2.5f, FLinearColor(0.9f, 0.88f, 0.75f));
	}
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		const FName SoundName = Item == EBRItem::Battery ? FName(TEXT("S_Battery")) : (Item == EBRItem::VHSTape ? FName(TEXT("S_ItemMove")) : FName(TEXT("S_Pickup")));
		if (USoundBase* S = A->Sound(SoundName))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.8f);
		}
	}
}

void ABRCharacter::NotifyPickupRefused(EBRPickupResult Result)
{
	FText Msg;
	switch (Result)
	{
	case EBRPickupResult::AlreadyTaken:
		Msg = NSLOCTEXT("BR", "Player.PickupTaken", "Trop tard : un co\u00e9quipier l'a d\u00e9j\u00e0 pris.");
		break;
	case EBRPickupResult::TooFar:
	case EBRPickupResult::NotVisible:
		Msg = NSLOCTEXT("BR", "Player.PickupTooFar", "Hors de port\u00e9e : approchez-vous de l'objet.");
		break;
	case EBRPickupResult::Full:
		ABRHUD::Notify(this, BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Player.InventairePleinInventoryFairePlace", "Inventaire plein ! {Inventory} pour faire de la place."))), 2.5f,
			FLinearColor(1.f, 0.6f, 0.5f));
		return;
	case EBRPickupResult::Dead:
	case EBRPickupResult::Loading:
	case EBRPickupResult::StaleLevel:
		Msg = NSLOCTEXT("BR", "Player.PickupNotNow", "Impossible de ramasser maintenant.");
		break;
	case EBRPickupResult::StaleLife:
		Msg = NSLOCTEXT("BR", "Player.PickupStaleLife", "Ramassage annul\u00e9 : vous vous \u00eates r\u00e9veill\u00e9 entre-temps. R\u00e9essayez.");
		break;
	default:
		Msg = NSLOCTEXT("BR", "Player.PickupRefused", "Ramassage refus\u00e9 par l'h\u00f4te.");
		break;
	}
	ABRHUD::Notify(this, Msg.ToString(), 2.5f, FLinearColor(1.f, 0.7f, 0.5f));
}

void ABRCharacter::DieOf(EBRDeathCause Cause, int8 InKiller, AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	StopClimb();
	SetDevFly(false);
	bDead = true;
	LocalCause = Cause;
	Health = 0.f;
	DeathTime = 0.f;
	KillerKind = InKiller;
	KillerActor = Killer;
	bReadingNote = false;
	bNightVision = false;
	bSwimming = false;
	bDiving = false;
	bMantling = false;
	UpdateViewMode();
	ABRWorld* W = ABRWorld::Get(this);
	if (Cause == EBRDeathCause::Drowning && W && W->Def().bWater)
	{
		// v4.7 : le corps d'un noye remonte et flotte a la surface : un coequipier peut l'atteindre et le relever
		FVector L = GetActorLocation();
		const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
		L.Z = FMath::Max(static_cast<float>(L.Z), W->Def().WaterHeight - Half * 0.6f);
		SetActorLocation(L, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (Cause != EBRDeathCause::Fall)
	{
		GetCharacterMovement()->DisableMovement();
	}
	// (v4.6 : chute dans une fosse : le corps continue de tomber dans le noir jusqu'au fond)
	PlaySound2D(TEXT("S_Death"), 1.f);
	if (ABRPlayerController* OwnerPC = Cast<ABRPlayerController>(Controller))
	{
		OwnerPC->NotifyPlayerDeath();
	}
	// v4.7 : l'etat officiel est celui du serveur (hote ou solo : tout de suite ; client : apres verification)
	if (HasAuthority())
	{
		bServerDying = false;
		ServerApplyDeathState(true, Cause, 1, FString(), InKiller);
	}
	else
	{
		ServerReportDeath(static_cast<uint8>(Cause));
	}
	if (W)
	{
		W->HandlePlayerDeath(Cause);
	}
}

void ABRCharacter::ServerApplyDeathState(bool bInDead, EBRDeathCause Cause, uint8 Event, const FString& By, int8 InKiller)
{
	if (!HasAuthority())
	{
		return;
	}
	DeathState.Killer = bInDead ? InKiller : int8(-1);
	if (bInDead)
	{
		// v4.8 : le delai de reanimation ne s'applique que si un coequipier vivant pouvait venir (regle du client)
		bServerTeammateAtDeath = false;
		for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->DeathState.bDead)
			{
				bServerTeammateAtDeath = true;
				break;
			}
		}
		Health = 0.f;
	}
	else
	{
		bServerDying = false;
		Health = Event == 2 ? 35.f : 100.f; // releve : un peu de sante ; reveille : sante pleine
		if (Event == 3 && bServerHealStockKnown)
		{
			// v4.11 : reveil apres une mort : le joueur repart avec l'equipement de depart (ResetInventory), le stock tenu
			// par l'hote aussi (une eau d'amande, un bandage)
			ServerHealStock.FindOrAdd(static_cast<uint8>(EBRItem::AlmondWater)) = 1;
			ServerHealStock.FindOrAdd(static_cast<uint8>(EBRItem::Bandage)) = 1;
		}
	}
	DeathState.bDead = bInDead;
	DeathState.Cause = bInDead ? static_cast<uint8>(Cause) : 0;
	DeathState.bRevivable = bInDead && BRDeath::CanRevive(Cause);
	DeathState.Event = Event;
	DeathState.By = By;
	DeathState.Serial = static_cast<uint8>(DeathState.Serial + 1);
	// v4.12 : la sante officielle change (0, 35 ou 100) : nouvelle revision, portee par l'etat replique. Un reveil au point
	// de depart (inventaire de depart) ouvre une nouvelle epoque de l'inventaire
	++HealthRev;
	DeathState.HealthRev = HealthRev;
	if (!bInDead && Event == 3)
	{
		DeathState.WakeCount = static_cast<uint8>(DeathState.WakeCount + 1);
	}
	if (IsLocallyControlled())
	{
		AckHealthRev = HealthRev;
	}
	if (bInDead && GetWorld())
	{
		ServerDeathTime = GetWorld()->GetTimeSeconds();
	}
	UE_LOG(LogBackrooms, Log, TEXT("Etat de mort (serveur) : %s %s, cause %d, evenement %d"), *GetName(), bInDead ? TEXT("mort") : TEXT("vivant"),
		static_cast<int32>(Cause), static_cast<int32>(Event));
	OnRep_DeathState(); // l'hote applique aussi (pions de ses invites, et le sien)
	ForceNetUpdate();
}

void ABRCharacter::ServerReportDeath_Implementation(uint8 InCause)
{
	if (DeathState.bDead)
	{
		return; // deja mort pour le serveur (chute constatee par lui, double envoi)
	}
	EBRDeathCause Cause = InCause <= static_cast<uint8>(EBRDeathCause::Madness) ? static_cast<EBRDeathCause>(InCause) : EBRDeathCause::Injury;
	const ABRWorld* W = ABRWorld::Get(this);
	const FVector L = GetActorLocation();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	// Verifications du serveur : une cause invraisemblable redevient une blessure (relevable)
	switch (Cause)
	{
	case EBRDeathCause::Fall:
		if (!W || !W->HasPits() || L.Z > -W->Def().PitKillDepth * 0.5f || !W->IsOverPit(L, 80.f))
		{
			UE_LOG(LogBackrooms, Warning, TEXT("%s : chute annoncee hors d'une fosse (%s) : notee comme blessure"), *GetName(), *L.ToString());
			Cause = EBRDeathCause::Injury;
		}
		break;
	case EBRDeathCause::Drowning:
		if (!W || !W->Def().bWater || L.Z > W->Def().WaterHeight + 60.f)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("%s : noyade annoncee hors de l'eau (%s) : notee comme blessure"), *GetName(), *L.ToString());
			Cause = EBRDeathCause::Injury;
		}
		break;
	case EBRDeathCause::Injury:
		if (!bServerDying && Now - ServerLastHitTime > 6.f)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("%s : mort par blessure sans frappe recente vue par le serveur"), *GetName());
		}
		break;
	case EBRDeathCause::Madness:
		break;
	default:
		Cause = EBRDeathCause::Injury;
		break;
	}
	const int8 Killer = (Cause == EBRDeathCause::Injury && bServerDying) ? ServerDyingKiller : int8(-1);
	bServerDying = false;
	ServerApplyDeathState(true, Cause, 1, FString(), Killer);
}

float ABRCharacter::MinRespawnDelay() const
{
	// Meme regle que ABRWorld::HandlePlayerDeath cote client (delai de reanimation si un coequipier vivant pouvait venir,
	// sinon 6 s), moins une marge pour la latence et l'ecart des horloges
	const EBRDeathCause Cause = static_cast<EBRDeathCause>(DeathState.Cause);
	const float Rule = (DeathState.bRevivable && bServerTeammateAtDeath) ? BRDeath::ReviveWindow(Cause) : 6.f;
	return FMath::Max(1.5f, Rule - 1.5f);
}

bool ABRCharacter::ServerCanStillRevive() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const EBRDeathCause Cause = static_cast<EBRDeathCause>(DeathState.Cause);
	// Un peu de marge apres la fin du delai : le geste de reanimation a commence avant
	return CanBeRevived() && Now - ServerDeathTime <= BRDeath::ReviveWindow(Cause) + 1.f;
}

void ABRCharacter::ClientRespawnDenied_Implementation(float Remaining)
{
	// v4.12 : ce signalement ne cree pas de nouvelle epoque : le serveur nous voit encore a terre
	PendingWakeReports = PendingWakeReports > 0 ? static_cast<uint8>(PendingWakeReports - 1) : 0;
	// Le serveur nous voit encore a terre : on y retourne pour le temps restant (meme cause, meme entite)
	const EBRDeathCause Cause = static_cast<EBRDeathCause>(DeathState.Cause);
	if (!bDead)
	{
		DieOf(Cause == EBRDeathCause::None ? EBRDeathCause::Injury : Cause, DeathState.Killer, nullptr);
	}
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->SetDeathTimer(FMath::Max(Remaining, 0.5f) + 0.25f);
	}
}

void ABRCharacter::ServerReportRespawn_Implementation()
{
	if (!DeathState.bDead)
	{
		ClientWakeAck(DeathState.WakeCount); // v4.12 : deja vivant pour le serveur : rien a confirmer, l'attente cesse
		return;
	}
	// v4.8 : regles reelles selon la cause (avant : simple avertissement sous 2 s). Trop tot : refuse, le joueur reste a
	// terre le temps restant
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float Elapsed = Now - ServerDeathTime;
	const float MinDelay = MinRespawnDelay();
	if (Elapsed < MinDelay)
	{
		UE_LOG(LogBackrooms, Warning, TEXT("%s : reveil refuse %.1f s apres la mort (minimum %.1f s pour cette cause)"), *GetName(), Elapsed, MinDelay);
		ClientRespawnDenied(MinDelay - Elapsed);
		return;
	}
	ServerApplyDeathState(false, EBRDeathCause::None, 3);
	ClientWakeAck(DeathState.WakeCount);
}

void ABRCharacter::ClientWakeAck_Implementation(uint8 WakeCount)
{
	// v4.12 : reponse au plus ancien signalement en attente (une par signalement, dans l'ordre) ; l'epoque de
	// l'inventaire suit les reveils confirmes par le serveur
	if (static_cast<int8>(static_cast<uint8>(WakeCount - KnownWakeCount)) >= 0)
	{
		KnownWakeCount = WakeCount;
	}
	PendingWakeReports = PendingWakeReports > 0 ? static_cast<uint8>(PendingWakeReports - 1) : 0;
}

void ABRCharacter::OnRep_DeathState()
{
	if (IsLocallyControlled() && !HasAuthority())
	{
		// v4.12 : revision de la sante (l'etat replique peut arriver avant ou apres les RPC). Les reveils, eux, ne sont
		// comptes que par les reponses aux signalements (ClientWakeAck, ClientRespawnDenied), une par signalement
		if (BRTxn::IsNewer(DeathState.HealthRev, AckHealthRev))
		{
			AckHealthRev = DeathState.HealthRev;
		}
	}
	if (DeathState.Serial == HandledDeathSerial)
	{
		return;
	}
	HandledDeathSerial = DeathState.Serial;
	const EBRDeathCause Cause = static_cast<EBRDeathCause>(DeathState.Cause);
	if (IsLocallyControlled())
	{
		// Proprietaire : seuls les evenements decides ailleurs comptent (sa propre mort et son reveil sont deja faits)
		if (DeathState.Event == 1 && DeathState.bDead && !bDead)
		{
			if (Cause == EBRDeathCause::Fall)
			{
				FallDeath(); // chute constatee par le serveur, arrivee avant (ou sans) la RPC
			}
			else if (ScareKind >= 0 && bScareLethal)
			{
				// v4.8 : coup mortel decide par le serveur : la mort viendra a la fin du jumpscare (EndJumpscare)
			}
			else
			{
				DieOf(Cause, DeathState.Killer, nullptr);
			}
		}
		else if (DeathState.Event == 1 && DeathState.bDead && bDead && Cause != LocalCause)
		{
			// Le serveur n'a pas retenu la cause annoncee (chute hors fosse, noyade hors de l'eau) : on suit son verdict
			LocalCause = Cause;
			if (ABRWorld* W = ABRWorld::Get(this))
			{
				W->HandlePlayerDeath(Cause);
			}
		}
		else if (DeathState.Event == 2 && !DeathState.bDead && bDead)
		{
			Revived(DeathState.By);
		}
		return;
	}
	// Autres machines : le corps d'un coequipier tombe, se releve ou se reveille ailleurs
	const bool bWas = bDead;
	bDead = DeathState.bDead;
	LocalCause = bDead ? Cause : EBRDeathCause::None;
	if (bDead && !bWas)
	{
		if (UBRAssets* A = UBRAssets::Get(this))
		{
			if (USoundBase* S = A->Sound(TEXT("S_Death")))
			{
				UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), 0.8f, 1.f, 0.f, A->Attenuation(3500.f));
			}
		}
		const APlayerState* PS = GetPlayerState();
		const FString Name = PS ? PS->GetPlayerName() : FString(BR_STR(NSLOCTEXT("BR", "Player.Explorateur", "Un explorateur")));
		ABRHUD::Notify(this, BRLoc::Fmt(BRDeath::TeammateMessage(Cause), { { TEXT("Name"), BRLoc::Arg(Name) } }), 4.f, FLinearColor(1.f, 0.45f, 0.4f));
	}
	else if (!bDead && bWas && DeathState.Event == 2)
	{
		const APlayerState* PS = GetPlayerState();
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Player.PlayernameEteReleveBy", "{PlayerName} a \u00e9t\u00e9 relev\u00e9 par {By}."), { { TEXT("PlayerName"), BRLoc::Arg(PS ? *PS->GetPlayerName() : BR_STR(NSLOCTEXT("BR", "Player.Explorateur", "Un explorateur"))) }, { TEXT("By"), BRLoc::Arg(DeathState.By) } }), 3.f,
			FLinearColor(0.6f, 1.f, 0.6f));
	}
}

void ABRCharacter::NotifyFellIntoPit()
{
	// Serveur : appele par ABRWorld::UpdatePitFalls quand la position du joueur est sous le bord d'une fosse
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (!HasAuthority() || bDead || Now - LastFallNotify < 2.f)
	{
		return;
	}
	LastFallNotify = Now;
	UE_LOG(LogBackrooms, Log, TEXT("Chute dans une fosse : %s a %s"), *GetName(), *GetActorLocation().ToString());
	// v4.7 : le serveur decide : etat de mort officiel tout de suite (replique a tous), puis la RPC donne au joueur
	// l'effet immediat. Mode developpeur invincible (autorise par l'hote) : seulement la remontee au bord
	const bool bGod = (IsLocallyControlled() ? bGodMode : (NetFlags & 16) != 0) && ABRPlayerController::AreCheatsAllowed();
	if (!bGod)
	{
		ServerApplyDeathState(true, EBRDeathCause::Fall, 1);
	}
	if (IsLocallyControlled())
	{
		FallDeath();
	}
	else
	{
		ClientFellIntoPit();
	}
}

void ABRCharacter::ClientFellIntoPit_Implementation()
{
	FallDeath();
}

void ABRCharacter::FallDeath()
{
	if (bDead)
	{
		return;
	}
	if (bGodMode)
	{
		// Mode developpeur invincible : on remonte sur le croisement de passages le plus proche (centre de la cellule)
		if (const ABRWorld* W = ABRWorld::Get(this))
		{
			const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
			SetActorLocation(W->CellCenter(W->WorldToCell(GetActorLocation()), Half + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
			GetCharacterMovement()->StopMovementImmediately();
		}
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.ModeDevChuteAnnuleeInvincible", "MODE D\u00c9V : chute annul\u00e9e (invincible)")), 3.f, FLinearColor(0.6f, 0.9f, 1.f));
		return;
	}
	Health = 0.f;
	DieOf(EBRDeathCause::Fall, -1, nullptr);
}

ABRCharacter* ABRCharacter::FindDownedTeammate() const
{
	const ABRWorld* W = ABRWorld::Get(this);
	if (!W || !W->IsNetGame() || bDead || !GetWorld())
	{
		return nullptr;
	}
	TArray<ABRCharacter*> Players;
	W->GetPlayers(Players);
	const FVector Eye = GetEyeLocation();
	const FVector Dir = GetViewDirection();
	ABRCharacter* Best = nullptr;
	float BestDot = 0.72f;
	for (ABRCharacter* Mate : Players)
	{
		if (Mate == this || !Mate->CanBeRevived())
		{
			continue; // (v4.7 : etat du serveur : une chute dans une fosse ne se releve pas, une noyade si)
		}
		const FVector BodyPos = Mate->GetActorLocation() - FVector(0.f, 0.f, 50.f); // etendu au sol
		if (FVector::Dist(BodyPos, GetActorLocation()) > 260.f)
		{
			continue;
		}
		const float Dot = static_cast<float>(FVector::DotProduct((BodyPos - Eye).GetSafeNormal(), Dir));
		if (Dot <= BestDot)
		{
			continue;
		}
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRReviveLos), false, this);
		Q.AddIgnoredActor(Mate);
		if (GetWorld()->LineTraceTestByChannel(Eye, BodyPos, ECC_WorldStatic, Q))
		{
			continue;
		}
		Best = Mate;
		BestDot = Dot;
	}
	return Best;
}

void ABRCharacter::UpdateRevive(float Dt)
{
	ABRCharacter* Mate = Cast<ABRCharacter>(FocusActor.Get());
	if (!bInteractHeld || !Mate || !Mate->IsDead() || bDead || bInputLocked)
	{
		ReviveProgress = 0.f;
		return;
	}
	if (ReviveProgress <= 0.f)
	{
		PlaySound2D(TEXT("S_Bandage"), 0.7f);
	}
	ReviveProgress += Dt / 3.5f;
	if (ReviveProgress >= 1.f)
	{
		ReviveProgress = 0.f;
		bInteractHeld = false;
		if (HasAuthority())
		{
			Mate->ReviveBy(this);
		}
		else
		{
			ServerRevive(Mate);
		}
	}
}

void ABRCharacter::ServerRevive_Implementation(ABRCharacter* Mate)
{
	// v4.7 : verifie par le serveur : coequipier mort d'une cause relevable, sauveteur vivant et a portee
	if (Mate && Mate != this && Mate->ServerCanStillRevive() && !DeathState.bDead && FVector::Dist(Mate->GetActorLocation(), GetActorLocation()) < 450.f)
	{
		Mate->ReviveBy(this);
	}
	else
	{
		UE_LOG(LogBackrooms, Warning, TEXT("Reanimation refusee par le serveur (%s)"), Mate ? *Mate->GetName() : TEXT("personne"));
	}
}

void ABRCharacter::ReviveBy(ABRCharacter* By)
{
	if (!HasAuthority() || !ServerCanStillRevive())
	{
		return;
	}
	const APlayerState* PS = By ? By->GetPlayerState() : nullptr;
	const FString Name = PS ? PS->GetPlayerName() : FString(BR_STR(NSLOCTEXT("BR", "Player.Coequipier", "un co\u00e9quipier")));
	// v4.7 : etat officiel (replique) ; le proprietaire se releve en le recevant (OnRep_DeathState)
	ServerApplyDeathState(false, EBRDeathCause::None, 2, Name);
}

void ABRCharacter::Revived(const FString& ByName)
{
	if (!bDead || LocalCause == EBRDeathCause::Fall)
	{
		return;
	}
	// Releve avec un peu de sante : l'inventaire est conserve
	bDead = false;
	Health = 35.f;
	Sanity = FMath::Max(Sanity, 35.f);
	Stamina = FMath::Max(Stamina, 40.f);
	DamageFlash = 0.f;
	DeathTime = 0.f;
	LastDamageTime = TimeAlive;
	KillerKind = -1;
	KillerActor.Reset();
	LocalCause = EBRDeathCause::None;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->CancelPlayerDeath();
	}
	PlaySound2D(TEXT("S_Gasp"), 0.85f);
	ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Player.BynameReleve", "{ByName} vous a relev\u00e9 !"), { { TEXT("ByName"), BRLoc::Arg(ByName) } }), 4.f, FLinearColor(0.6f, 1.f, 0.6f));
}

void ABRCharacter::ServerSetState_Implementation(uint8 Flags, uint8 Hand, uint8 Lamp)
{
	NetFlags = Flags;
	NetHand = Hand;
	NetLamp = Lamp;
}

void ABRCharacter::ResetStats()
{
	const bool bWasDead = bDead || DeathState.bDead;
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
	KillerKind = -1;
	KillerActor.Reset();
	LocalCause = EBRDeathCause::None;
	ResetInventory();
	OnEquipmentChanged();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (bIsCrouched)
	{
		UnCrouch();
	}
	// v4.7 : reveil (au point de depart, ou nouveau niveau) : le serveur le note vivant
	if (bWasDead)
	{
		if (HasAuthority())
		{
			if (DeathState.bDead)
			{
				ServerApplyDeathState(false, EBRDeathCause::None, 3);
			}
		}
		else
		{
			// v4.12 : l'inventaire est deja celui de la nouvelle vie ; l'epoque suit des la demande, en attendant la
			// confirmation (ou le refus) du serveur
			PendingWakeReports = static_cast<uint8>(PendingWakeReports + 1);
			ServerReportRespawn();
		}
	}
}

void ABRCharacter::WriteToSave(UBRSaveGame* Save) const
{
	if (!Save)
	{
		return;
	}
	Save->bHasPlayer = true;
	Save->Items.Reset();
	auto AddGroup = [Save](EBRSlotGroup Group, const TArray<FBRItemSlot>& Slots)
	{
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (!Slots[i].IsEmpty())
			{
				FBRSavedItem It;
				It.Group = static_cast<uint8>(Group);
				It.Index = i;
				It.Item = static_cast<uint8>(Slots[i].Item);
				It.Count = Slots[i].Count;
				Save->Items.Add(It);
			}
		}
	};
	AddGroup(EBRSlotGroup::Pockets, Pockets);
	AddGroup(EBRSlotGroup::Storage, Storage);
	AddGroup(EBRSlotGroup::Equipment, Equipment);
	// v4.12 : objets mis de cote (acceptes par l'hote, pas encore ranges) : gardes avec l'inventaire
	Save->Recovered.Reset();
	for (const FBRItemSlot& S : Recovered)
	{
		if (!S.IsEmpty())
		{
			FBRSavedItem It;
			It.Item = static_cast<uint8>(S.Item);
			It.Count = S.Count;
			Save->Recovered.Add(It);
		}
	}
	Save->Health = Health;
	Save->Sanity = Sanity;
	Save->Battery = Battery;
	Save->Notes = ReadNotes;
}

void ABRCharacter::ReadFromSave(const UBRSaveGame* Save)
{
	ResetStats(); // equipement de depart, etat normal
	if (!Save || !Save->bHasPlayer)
	{
		return;
	}
	for (TArray<FBRItemSlot>* Slots : { &Pockets, &Storage, &Equipment })
	{
		for (FBRItemSlot& S : *Slots)
		{
			S.Clear();
		}
	}
	for (const FBRSavedItem& It : Save->Items)
	{
		if (It.Item == 0 || It.Item >= static_cast<uint8>(EBRItem::Count) || It.Count <= 0 || It.Group > 2)
		{
			continue;
		}
		if (FBRItemSlot* S = GetSlot(static_cast<EBRSlotGroup>(It.Group), It.Index))
		{
			S->Item = static_cast<EBRItem>(It.Item);
			S->Count = It.Count;
		}
	}
	// v4.12 : objets mis de cote (une sauvegarde plus ancienne n'en a pas)
	Recovered.Reset();
	for (const FBRSavedItem& It : Save->Recovered)
	{
		if (It.Item != 0 && It.Item < static_cast<uint8>(EBRItem::Count) && It.Count > 0 && Recovered.Num() < BRTxn::MaxRecovered)
		{
			Recovered.Add(FBRItemSlot{ static_cast<EBRItem>(It.Item), It.Count });
		}
	}
	// Anciennes sauvegardes (camescope en main) : un objet qui ne peut plus etre equipe la retourne dans le sac
	for (int32 i = 0; i < Equipment.Num(); ++i)
	{
		if (!Equipment[i].IsEmpty() && !BRItems::CanEquipIn(Equipment[i].Item, static_cast<EBREquipSlot>(i)))
		{
			const FBRItemSlot Moved = Equipment[i];
			Equipment[i].Clear();
			AddItem(Moved.Item, Moved.Count);
		}
	}
	// On ne reprend jamais une partie a l'agonie
	Health = FMath::Clamp(Save->Health, 25.f, 100.f);
	Sanity = FMath::Clamp(Save->Sanity, 25.f, 100.f);
	Battery = FMath::Clamp(Save->Battery, 0.f, 100.f);
	ReadNotes = Save->Notes; // identifiants (migres depuis le texte francais des anciennes sauvegardes par BRSaves::Load)
	NotesRead = ReadNotes.Num();
	OnEquipmentChanged();
}

bool ABRCharacter::ReceivePickup(EBRItem Item, const FString& Note)
{
	if (Item == EBRItem::Note)
	{
		++NotesRead;
		// v4.8 : identifiant de la note (son texte est compose a l'affichage, dans la langue du joueur)
		OpenNote = Note;
		if (!Note.IsEmpty())
		{
			ReadNotes.AddUnique(Note);
		}
		bReadingNote = true;
		return true;
	}
	// v4.12 : un objet attribue n'est jamais refuse : inventaire, sinon reserve "mis de cote"
	StorePickup(Item, true);
	return true;
}

void ABRCharacter::OnEnteredLevel(const FBRLevelDef& Def)
{
	// v4.10 : objets de soin declares au serveur (base de la verification de possession des soins)
	if (!HasAuthority() && IsLocallyControlled())
	{
		ServerDeclareHealStock(static_cast<uint8>(FMath::Clamp(CountItem(EBRItem::AlmondWater), 0, 255)),
			static_cast<uint8>(FMath::Clamp(CountItem(EBRItem::Bandage), 0, 255)));
	}
	ScaredKinds.Reset();
	StopClimb();
	ClimbGlitch = 0.f;
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
	if (bDevFly)
	{
		// Vol libre garde d'un niveau a l'autre (mode developpeur)
		bDevFly = false;
		SetDevFly(true);
	}
	if (Def.Fixture == EBRFixture::None && !Def.bOutdoor && !bFlashlightOn)
	{
		ABRHUD::Notify(this, BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Player.FaitNoirCommeFourFlashlight", "Il fait noir comme dans un four. {Flashlight} lampe  -  {NightVision} vision nocturne"))), 5.f, FLinearColor(1.f, 0.85f, 0.6f));
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
	if (HasAuthority())
	{
		TickServerVitals(Dt); // v4.8 : coup mortel en attente de la fin du jumpscare
	}

	if (!IsLocallyControlled())
	{
		TickRemote(Dt);
		return;
	}
	// v4.8 : client : la sante (soins, noyade, folie, recuperation) est envoyee au serveur, qui decide des coups
	if (!HasAuthority() && !bDead)
	{
		VitalsSyncTimer -= Dt;
		if (VitalsSyncTimer <= 0.f || FMath::Abs(Health - LastSentHealth) > 8.f)
		{
			VitalsSyncTimer = 0.5f;
			LastSentHealth = Health;
			ServerSyncVitals(Health, AckHealthRev);
		}
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
	UpdateJumpscare(Dt);
	UpdateClimb(Dt);
	// v4.12 : objets mis de cote : ranges des qu'une place se libere (essai chaque seconde, en plus de chaque deplacement)
	if (Recovered.Num() > 0 && !bDead)
	{
		StowTimer -= Dt;
		if (StowTimer <= 0.f)
		{
			StowTimer = 1.f;
			StowRecovered(true);
		}
	}
	if (bDevFly)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->MaxFlySpeed = bWantsSprint ? 2600.f : 900.f;
		}
		if (bJumpHeld && !bInputLocked)
		{
			AddMovementInput(FVector::UpVector, 1.f);
		}
	}
	UpdateWater(Dt);
	UpdateHiding();
	UpdateCamera(Dt);
	UpdateFlashlight(Dt);
	UpdateFocus();
	UpdateRevive(Dt);
	UpdateMissionHold(Dt);
	UpdateAudio(Dt);
	UpdatePostProcess(Dt);
	AnimateBody(Dt);
	SwimInput = FVector::ZeroVector;

	// Taches d'enregistrement : il suffit d'avoir le camescope sur soi (il filme ce que l'on regarde)
	if (HasCamcorder() && !bDead && !bInputLocked)
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
	const uint8 Flags = (IsFlashlightOn() ? 1 : 0) | (IsSprinting() ? 2 : 0) | (bSwimming ? 4 : 0) | (bClimbing ? 8 : 0) | (bGodMode ? 16 : 0)
		| (GetEquipped(EBREquipSlot::Chest) == EBRItem::Vest ? 32 : 0);
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
			DieOf(EBRDeathCause::Madness, -1, nullptr);
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

	// Balancement desactivable (confort) : les pas restent rythmes par la marche
	if (!FBRSettings::Get().bHeadBob)
	{
		BobZ = 0.f;
		BobY = 0.f;
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
	Camera->SetFieldOfView(FBRSettings::Get().FOV + (IsSprinting() ? 4.f : 0.f) + FMath::Sin(TimeAlive * 0.6f) * Insanity * Insanity * 6.f + ScareFOV);

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
		const FVector Base(30.f, 16.f, -19.f);
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
	if (bNightVision && !HasCamcorder())
	{
		// Camescope pose ou perdu : plus de vision nocturne
		bNightVision = false;
		UpdateViewMode();
	}
	if (bNightVision && !bDead)
	{
		Battery = FMath::Max(0.f, Battery - NightVisionDrain * Dt);
		if (Battery <= 0.f)
		{
			bNightVision = false;
			UpdateViewMode();
			ABRHUD::Notify(this, BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Player.BatterieVideVisionNocturneCoupee", "Batterie vide : vision nocturne coup\u00e9e. {Battery} changer les piles"))), 3.f, FLinearColor(1.f, 0.8f, 0.4f));
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
		if (Battery < 15.f && !bLowBatteryWarned && IsLocallyControlled())
		{
			// v4.9 : la charge n'est plus une jauge : un seul avertissement quand elle devient faible (et l'inspection de la
			// lampe donne la charge exacte)
			bLowBatteryWarned = true;
			ABRHUD::Notify(this, BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Player.LowBattery", "Piles faibles : la lampe va s'\u00e9teindre. {Battery} changer les piles"))), 3.5f,
				FLinearColor(1.f, 0.82f, 0.45f));
		}
		if (Battery > 25.f)
		{
			bLowBatteryWarned = false;
		}
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
			ABRHUD::Notify(this, BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Player.LampeEteintBatteryChangerPiles", "La lampe s'\u00e9teint. {Battery} changer les piles"))), 3.f, FLinearColor(1.f, 0.8f, 0.4f));
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
}

void ABRCharacter::UpdateFocus()
{
	FocusPrompt.Empty();
	FocusActor.Reset();
	if (bDead || bInputLocked || !GetWorld())
	{
		return;
	}
	if (bClimbing)
	{
		FocusPrompt = BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Player.MoveforwardMonterMovebackwardDescendreJu", "{MoveForward} monter  -  {MoveBackward} descendre  -  {Jump} l\u00e2cher l'\u00e9chelle")));
		return;
	}
	// 0) Coequipier a terre : le relever (multijoueur)
	if (ABRCharacter* Mate = FindDownedTeammate())
	{
		FocusActor = Mate;
		const APlayerState* PS = Mate->GetPlayerState();
		FocusPrompt = BRLoc::Fmt(NSLOCTEXT("BR", "Player.MaintenirInteractReleverPlayername", "Maintenir {Interact} : relever {PlayerName}"), { { TEXT("Interact"), BRLoc::Arg(BRKeys::Tag(EBRAction::Interact)) }, { TEXT("PlayerName"), BRLoc::Arg(PS ? *PS->GetPlayerName() : BR_STR(NSLOCTEXT("BR", "Player.VotreCoequipier", "votre co\u00e9quipier"))) } });
		return;
	}

	const FVector Eye = GetEyeLocation();
	const FVector Dir = GetViewDirection();
	auto Accept = [this](AActor* A) -> bool
	{
		if (const ABRMissionDevice* D = Cast<ABRMissionDevice>(A))
		{
			if (D->IsInteractable())
			{
				FocusActor = A;
				FocusPrompt = D->GetPrompt();
				return !FocusPrompt.IsEmpty();
			}
			return false;
		}
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
			const ABRMissionDevice* MD = Cast<ABRMissionDevice>(A);
			if (!A || !Comp || (!Cast<ABRPickup>(A) && !(E && E->IsInteractable()) && !(MD && MD->IsInteractable())))
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
		FocusPrompt = BRKeys::Tag(EBRAction::Jump) + BR_STR(NSLOCTEXT("BR", "Player.SortirEau", " Sortir de l'eau"));
		return;
	}

	// 4) Cachette toute proche
	bool bNeedsCrouch = false;
	const ABRWorld* HW = ABRWorld::Get(this);
	if (!bHidden && HW && HW->FindHidingSpotNear(GetActorLocation(), 110.f, bNeedsCrouch))
	{
		FocusPrompt = bNeedsCrouch ? BRKeys::Tag(EBRAction::Crouch) + BR_STR(NSLOCTEXT("BR", "Player.AccroupirGlisserTrouCacher", " S'accroupir et se glisser dans le trou pour se cacher"))
			: FString(BR_STR(NSLOCTEXT("BR", "Player.EntrerPlacardCacher", "Entrer dans le placard pour se cacher")));
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
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Player.EtesCacheEntitesVoientRestez", "Vous \u00eates cach\u00e9 : les entit\u00e9s ne vous voient plus. Restez immobile et attendez qu'elles s'\u00e9loignent.")),
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
		Kind = WaterDepth > 2.f ? TEXT("Water") : TEXT("Hard"); // au sec sur un trottoir carrele
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

void ABRCharacter::ApplyMenuBlur(float Amount)
{
	if (!Camera)
	{
		return;
	}
	// Mise au point a 25 cm, grande ouverture : tout le decor devient un bokeh doux (DOF cinematographique d'UE5)
	FPostProcessSettings& S = Camera->PostProcessSettings;
	const bool bOn = Amount > 0.01f;
	S.bOverride_DepthOfFieldFocalDistance = bOn;
	S.DepthOfFieldFocalDistance = 25.f;
	S.bOverride_DepthOfFieldFstop = bOn;
	S.DepthOfFieldFstop = FMath::Lerp(16.f, 1.4f, FMath::Clamp(Amount, 0.f, 1.f));
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
	// v4.10 : flashs attenues ou aucun : aberration, grain, teinte rouge des coups, folie et asphyxie reduits (les
	// mecaniques ne changent pas : seuls les effets a l'ecran)
	const float Comfort = FMath::Lerp(0.35f, 1.f, Set.FlashScale());
	const float Insanity = (1.f - Sanity / 100.f) * Comfort;
	// Coup recu : sans flashs, l'ecran ne rougit plus d'un coup ; le bord s'assombrit seulement (vignette)
	const float HitFlash = DamageFlash * Set.FlashScale();
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
	const float Choke = ((!bDead && Breath < 35.f) ? (35.f - Breath) / 35.f : 0.f) * FMath::Lerp(0.6f, 1.f, Set.FlashScale());
	// v4.9 : plus de barre de vie : une blessure grave se voit a un leger voile (bords assombris, couleurs ternies), sans
	// effet plein ecran permanent ; le coeur s'entend deja (UpdateAudio). Attenue avec les flashs reduits (confort)
	const float Wound = (!bDead && Health < 35.f) ? (35.f - Health) / 35.f * FMath::Lerp(0.6f, 1.f, Set.FlashScale()) : 0.f;

	FPostProcessSettings& S = Camera->PostProcessSettings;
	Camera->PostProcessBlendWeight = 1.f;

	// Effet camescope desactive : ni aberration de l'objectif, ni grain, ni salete, vignettage leger
	const bool bVHS = Set.bVHSEffect;
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = (bVHS ? 0.4f : 0.f) + Insanity * Insanity * 4.f + Glitch * 8.f * Comfort + HitFlash * 3.f + (bNV ? 1.5f : 0.f) + UnderBlend * 1.5f + Choke * 2.f * Comfort + ClimbGlitch * 6.f * Comfort + ScareFringe * 1.5f;

	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = ((Set.bFilmGrain && bVHS) ? (D ? D->Grain : 0.25f) : 0.f) + Insanity * 0.5f + Glitch * 0.8f + (bNV ? 0.7f : 0.f);

	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = (D ? D->Vignette : 0.45f) * (bVHS ? 1.f : 0.4f) + (bHidden ? 0.45f : 0.f) + Insanity * 0.5f + DamageFlash * 0.6f * Comfort + Dead * 0.8f + (bNV ? 0.5f : 0.f) + UnderBlend * 0.6f
		+ Choke * 0.9f + ScareFringe * 0.25f + Wound * 0.3f;

	float Sat = (D ? D->Saturation : 1.f) * FMath::Lerp(1.f, 0.45f, FMath::Max(FMath::Max3(Insanity * Insanity, Dead, Choke * 0.6f), Wound * 0.35f));
	if (bNV)
	{
		Sat = 0.f;
	}
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);

	FLinearColor Tint = D ? D->SceneTint : FLinearColor::White;
	Tint = FMath::Lerp(Tint, FLinearColor(0.45f, 0.9f, 0.82f), UnderBlend * 0.8f);
	// Vision nocturne : image monochrome teintee en vert. La teinte de scene passe avant la desaturation (elle serait
	// effacee) : le vert est donne par le gain de l'etalonnage, applique apres la saturation
	S.bOverride_ColorGain = true;
	S.ColorGain = bNV ? FVector4(0.42f, 1.18f, 0.5f, 1.f) : FVector4(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Hurt(1.f, 0.35f, 0.3f);
	S.bOverride_SceneColorTint = true;
	S.SceneColorTint = FMath::Lerp(Tint, Hurt, FMath::Clamp(HitFlash * 0.6f + Dead * 0.5f, 0.f, 1.f));
	// Jumpscare : eclair de la couleur de l'entite a l'impact
	S.SceneColorTint = FMath::Lerp(S.SceneColorTint, ScareTint * 1.3f, FMath::Clamp(ScareFlash * 0.35f, 0.f, 0.45f));

	// Luminosite choisie par le joueur ; vision nocturne : amplification de lumiere
	// (vision nocturne : l'exposition peut descendre tres bas et s'adapte vite ; c'est surtout le projecteur
	// infrarouge qui eclaire, un gain trop fort brulait l'image des qu'un mur etait proche)
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = (D ? D->ExposureBias : 0.f) + Set.Brightness + (bNV ? 1.f : 0.f) - ScareDark * 3.f;
	S.bOverride_AutoExposureMinBrightness = bNV;
	S.AutoExposureMinBrightness = (D ? D->MinEV : 2.f) - 6.f;
	S.bOverride_AutoExposureSpeedUp = bNV;
	S.AutoExposureSpeedUp = 6.f;
	S.bOverride_AutoExposureSpeedDown = bNV;
	S.AutoExposureSpeedDown = 6.f;
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
			S.BloomDirtMaskIntensity = bVHS ? 3.f : 0.f;
		}
	}

	S.bOverride_MotionBlurAmount = true;
	// v4.7 : flou de mouvement au choix du joueur (desactive par defaut)
	S.MotionBlurAmount = Set.bMotionBlur ? 0.35f : 0.f;

	// v4.1 : liquides "RTX" : reflets Lumen plus fins, et reflets de premier plan sur l'eau translucide (Poolrooms)
	// (traces en ray tracing materiel quand la carte le permet)
	S.bOverride_LumenReflectionQuality = true;
	S.LumenReflectionQuality = Set.Quality >= 3 ? 2.f : 1.f;
	S.bOverride_LumenFrontLayerTranslucencyReflections = true;
	S.LumenFrontLayerTranslucencyReflections = Set.Quality >= 2;
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

	// v4.4 : combinaison a squelette (Tools/Blender/build_hazmat_skeletal.py), animee os par os, peau sans coutures
	UBRAssets* A = UBRAssets::Get(this);
	if (USkeletalMesh* Skin = A ? A->SkeletalMesh(TEXT("SK_Hazmat")) : nullptr)
	{
		BodySkin = NewObject<UPoseableMeshComponent>(this, TEXT("BodySkin"));
		BodySkin->SetupAttachment(BodyFeet);
		BodySkin->SetSkinnedAssetAndUpdate(Skin);
		BodySkin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BodySkin->SetCastShadow(true);
		BodySkin->RegisterComponent();
		// v4.8 : slots de la combinaison affectes explicitement par leur nom : combinaison -> T_Hazmat_Suit, masque ->
		// T_Hazmat_Mask, visiere -> son propre materiau. Un slot inattendu est signale (materiau d'erreur), il n'est plus
		// habille en silence par le style sombre generique "Body"
		static const TArray<TPair<FString, FString>> HazmatSlots = {
			{ TEXT("HazmatSuit"), TEXT("HazmatSuit") }, { TEXT("HazmatMask"), TEXT("HazmatMask") }, { TEXT("HazmatGlass"), TEXT("HazmatGlass") } };
		A->ApplySlotMap(BodySkin, HazmatSlots, TEXT("SK_Hazmat"));
		// Diagnostic par section (une fois par session) : maillage, slot, materiau final, parent, texture, usage, secours
		static bool bDescribed = false;
		if (!bDescribed)
		{
			bDescribed = true;
			for (const FString& Line : A->DescribeSections(BodySkin, TEXT("SK_Hazmat")))
			{
				UE_LOG(LogBackrooms, Log, TEXT("Combinaison : %s"), *Line);
			}
		}
		Body.Meshes.Add(BodySkin);
		Body.Torso = BodySkin;
		BodyComponents.Add(BodySkin);
	}
	else
	{
		// Pieces rigides de la combinaison fournie (Tools/Blender/import_user_models.py) ; a defaut, des boites jaunes
		TMap<FString, FLinearColor> Fallback;
		Fallback.Add(TEXT("FallbackHazmat"), FLinearColor(0.75f, 0.6f, 0.08f));
		if (!BRRig::HasHazmat(this))
		{
			UBRAssets::ReportFallback(TEXT("Combinaison hazmat du joueur (SK_Hazmat, SM_Hazmat_*)"));
		}
		Body = BRRig::BuildHumanoid(this, BodyFeet, TEXT("SM_Hazmat"), FBRHumanoidSpec::Hazmat(), &Fallback, BodyComponents, true);
	}

	// La tete et les bras suivent le buste (penche en avant quand on court / s'accroupit)
	const FAttachmentTransformRules Keep(EAttachmentRule::KeepWorld, false);
	if (Body.Torso && !BodySkin)
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
		// Vision nocturne : on regarde a travers le camescope (son modele, colle au projecteur infrarouge, eblouissait
		// l'image au point que l'exposition automatique assombrissait tout le reste)
		HandMesh->SetVisibility(!bShow && !bNightVision && HandMesh->GetStaticMesh() != nullptr);
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
	ABRHUD::Notify(this, bThirdPerson ? FString(BR_STR(NSLOCTEXT("BR", "Player.Vue3ePersonne", "Vue \u00e0 la 3e personne"))) : FString(BR_STR(NSLOCTEXT("BR", "Player.Vue1rePersonne", "Vue \u00e0 la 1re personne"))), 1.5f,
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
	const bool bClimbPose = bClimbing || (bRemoteView && (NetFlags & 8) != 0);
	ClimbBlend = FMath::FInterpTo(ClimbBlend, bClimbPose ? 1.f : 0.f, Dt, 8.f);
	AirBlend = FMath::FInterpTo(AirBlend, (!bGrounded && !bSwimming && !bDead && !bClimbPose && !bDevFly) ? 1.f : 0.f, Dt, 6.f);
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
		// ---- Echelle (v4.4) : une main au-dessus de l'autre, un pied par barreau (le rythme suit la hauteur) ----
		if (ClimbBlend > 0.01f)
		{
			const float CP = static_cast<float>(GetActorLocation().Z) / 30.f * PI + (i == 0 ? 0.f : PI);
			const float Reach = FMath::Max(0.f, FMath::Sin(CP));
			TP = FMath::Lerp(TP, 35.f + 45.f * Reach, ClimbBlend);
			SP = FMath::Lerp(SP, -45.f - 55.f * Reach, ClimbBlend);
			UP = FMath::Lerp(UP, 135.f + 30.f * Reach, ClimbBlend);
			UR = FMath::Lerp(UR, Out * 8.f, ClimbBlend);
			LP = FMath::Lerp(LP, 35.f - 25.f * Reach, ClimbBlend);
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
		const float Hold = (1.f - SwimBlend) * (1.f - ClimbBlend);
		Upper[1] = FMath::Lerp(Upper[1], FRotator(28.f + ViewPitch * 0.8f - TorsoPitch, 0.f, -6.f), Hold);
		Lower[1] = FMath::Lerp(Lower[1], FRotator(62.f, 0.f, 0.f), Hold);
	}

	// Corps entier : penche a l'horizontale pendant la brasse, s'effondre a la mort
	const float SwimPitch = -72.f * SwimMove * SwimBlend;
	// La tete regarde ou vise le joueur (et se redresse quand on nage a plat ventre ; vers le haut sur l'echelle)
	const FRotator HeadRot(FMath::Lerp(ViewPitch * 0.6f - TorsoPitch - SwimPitch * 0.7f, 25.f, ClimbBlend), 0.f, 0.f);
	if (BodySkin)
	{
		PoseSkin(Thigh, Shin, Upper, Lower, FRotator(TorsoPitch, 0.f, 0.f), HeadRot);
		if (HeldMesh && HeldMesh->GetStaticMesh())
		{
			// L'objet tenu suit la main droite, dans l'axe de l'avant-bras
			const FVector Hand = BodySkin->GetBoneTransformByName(TEXT("RightHand"), EBoneSpaces::WorldSpace).GetLocation();
			const FVector Elbow = BodySkin->GetBoneTransformByName(TEXT("RightForeArm"), EBoneSpaces::WorldSpace).GetLocation();
			const FVector Dir = (Hand - Elbow).GetSafeNormal();
			if (!Dir.IsNearlyZero())
			{
				HeldMesh->SetWorldLocationAndRotation(Hand + Dir * 6.f, FRotationMatrix::MakeFromX(Dir).Rotator());
			}
		}
	}
	else
	{
		for (int32 i = 0; i < 2; ++i)
		{
			if (Body.Thigh[i]) { Body.Thigh[i]->SetRelativeRotation(Thigh[i]); }
			if (Body.Shin[i]) { Body.Shin[i]->SetRelativeRotation(Shin[i]); }
			if (Body.UpperArm[i]) { Body.UpperArm[i]->SetRelativeRotation(Upper[i]); }
			if (Body.LowerArm[i]) { Body.LowerArm[i]->SetRelativeRotation(Lower[i]); }
		}
		Body.Torso->SetRelativeRotation(FRotator(TorsoPitch, 0.f, 0.f));
		if (Body.Head)
		{
			Body.Head->SetRelativeRotation(HeadRot);
		}
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

void ABRCharacter::PoseSkin(const FRotator* Thigh, const FRotator* Shin, const FRotator* Upper, const FRotator* Lower, const FRotator& Torso,
	const FRotator& Head)
{
	// Os du squelette Mixamo renommes (index 0 = gauche, 1 = droite, comme les pieces rigides)
	static const FName NSpine(TEXT("Spine"));
	static const FName NHead(TEXT("Head"));
	static const FName NArm[2] = { FName(TEXT("LeftArm")), FName(TEXT("RightArm")) };
	static const FName NFore[2] = { FName(TEXT("LeftForeArm")), FName(TEXT("RightForeArm")) };
	static const FName NUpLeg[2] = { FName(TEXT("LeftUpLeg")), FName(TEXT("RightUpLeg")) };
	static const FName NLeg[2] = { FName(TEXT("LeftLeg")), FName(TEXT("RightLeg")) };
	if (!BodySkin)
	{
		return;
	}
	if (SkinRest.Num() == 0)
	{
		// Pose de repos (bras le long du corps), une fois : les angles s'y ajoutent autour des articulations
		for (const FName& N : { NSpine, NHead, NArm[0], NArm[1], NFore[0], NFore[1], NUpLeg[0], NUpLeg[1], NLeg[0], NLeg[1] })
		{
			if (BodySkin->GetBoneIndex(N) != INDEX_NONE)
			{
				SkinRest.Add(N, BodySkin->GetBoneTransformByName(N, EBoneSpaces::ComponentSpace).GetRotation());
			}
		}
		if (SkinRest.Num() == 0)
		{
			return;
		}
	}
	auto Set = [this](const FName& Bone, const FQuat& Delta)
	{
		if (const FQuat* Rest = SkinRest.Find(Bone))
		{
			FTransform T = BodySkin->GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace);
			T.SetRotation(Delta * *Rest);
			BodySkin->SetBoneTransformByName(Bone, T, EBoneSpaces::ComponentSpace);
		}
	};
	// Du tronc vers les extremites : chaque os herite de la rotation de son parent (meme composition que les pivots)
	const FQuat QT = Torso.Quaternion();
	Set(NSpine, QT);
	Set(NHead, QT * Head.Quaternion());
	for (int32 i = 0; i < 2; ++i)
	{
		const FQuat QU = QT * Upper[i].Quaternion();
		Set(NArm[i], QU);
		Set(NFore[i], QU * Lower[i].Quaternion());
		const FQuat QTh = Thigh[i].Quaternion();
		Set(NUpLeg[i], QTh);
		Set(NLeg[i], QTh * Shin[i].Quaternion());
	}
}

void ABRCharacter::SetDevFly(bool bFly)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move || bFly == bDevFly || (bFly && bDead))
	{
		return;
	}
	if (bFly)
	{
		StopClimb();
		StopSwimming();
	}
	bDevFly = bFly;
	SetActorEnableCollision(!bFly);
	if (bFly)
	{
		Move->SetMovementMode(MOVE_Flying);
		Move->MaxFlySpeed = 900.f;
		Move->BrakingDecelerationFlying = 3000.f;
	}
	else if (!bDead)
	{
		Move->SetMovementMode(MOVE_Falling);
	}
}

void ABRCharacter::StartClimb(ABRExit* Ladder)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Ladder || !Move || bDead || bSwimming || bClimbing)
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	bClimbing = true;
	ClimbLadder = Ladder;
	ClimbInput = 0.f;
	ClimbStepAcc = 0.f;
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_Flying);
	// Face a l'echelle (le dos a la piece)
	if (Controller)
	{
		FRotator R = Controller->GetControlRotation();
		R.Yaw = Ladder->GetActorRotation().Yaw + 180.f;
		R.Pitch = FMath::Clamp(static_cast<float>(FRotator::NormalizeAxis(R.Pitch)), -10.f, 40.f);
		Controller->SetControlRotation(R);
	}
	PlaySound2D(TEXT("S_Step_Hard_1"), 0.5f);
}

void ABRCharacter::StopClimb()
{
	if (!bClimbing)
	{
		return;
	}
	bClimbing = false;
	ClimbLadder.Reset();
	ClimbInput = 0.f;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (!bSwimming)
		{
			Move->SetMovementMode(MOVE_Falling);
		}
	}
}

void ABRCharacter::UpdateClimb(float Dt)
{
	ClimbGlitch = FMath::FInterpTo(ClimbGlitch, 0.f, Dt, 1.5f);
	if (!bClimbing)
	{
		return;
	}
	ABRExit* L = ClimbLadder.Get();
	ABRWorld* W = ABRWorld::Get(this);
	if (!L || !W || bDead)
	{
		StopClimb();
		return;
	}
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	const FVector Pos = GetActorLocation();
	const float TopZ = L->GetClimbTopZ();
	// Dans le conduit, la realite se dechire de plus en plus jusqu'au noclip
	ClimbGlitch = FMath::Max(ClimbGlitch, FMath::Clamp(static_cast<float>(Pos.Z - (TopZ - 220.f)) / 220.f, 0.f, 1.f));
	float Up = ClimbInput;
	ClimbInput = 0.f;
	if (W->IsTransitioning())
	{
		Up = 0.5f; // on continue de monter pendant le noclip
	}
	else if (bInputLocked)
	{
		Up = 0.f;
	}
	if (Up < 0.f && Pos.Z - Half <= L->GetActorLocation().Z + 4.f)
	{
		StopClimb(); // les pieds touchent le sol
		return;
	}
	// Colle a l'echelle, monte et descend a 1,4 m/s
	const FVector Anchor = L->GetClimbAnchor();
	const float Pull = FMath::Min(1.f, Dt * 10.f);
	const FVector Delta((Anchor.X - Pos.X) * Pull, (Anchor.Y - Pos.Y) * Pull, Up * 140.f * Dt);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->Velocity = FVector::ZeroVector;
	}
	SetActorLocation(Pos + Delta, true);
	// Les barreaux sous les mains et les pieds
	ClimbStepAcc += FMath::Abs(Up) * 140.f * Dt;
	if (ClimbStepAcc > 30.f)
	{
		ClimbStepAcc = 0.f;
		static const TCHAR* const Rungs[] = { TEXT("S_Step_Hard_1"), TEXT("S_Step_Hard_2"), TEXT("S_Step_Hard_3"), TEXT("S_Step_Hard_4") };
		PlaySound2D(Rungs[FMath::RandRange(0, 3)], 0.35f);
	}
	if (GetActorLocation().Z >= TopZ && !W->IsTransitioning())
	{
		L->FinishClimb(this);
	}
}

void ABRCharacter::StartSwimming()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bSwimming || !Move || bDead || bClimbing || bDevFly)
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
		ABRHUD::Notify(this, BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Player.NagezRegardGuideNageJump", "Vous nagez : le regard guide la nage.  {Jump} remonter / sortir au bord  -  {Crouch} plonger  -  {Sprint} nager vite"))),
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
				DieOf(EBRDeathCause::Drowning, -1, nullptr);
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

	// Le sillage dans l'eau (marche, nage) est calcule par la simulation de l'eau du monde (ABRWorld::UpdateWaterSim)

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

FString ABRCharacter::GetOpenNote() const
{
	if (OpenNote.IsEmpty())
	{
		return BR_STR(NSLOCTEXT("BR", "Player.NoteIllisibleEncreCoule", "(La note est illisible, l'encre a coul\u00e9.)"));
	}
	return BRLevels::NoteText(OpenNote).ToString();
}

// =====================================================================================================================
// v4.11 : missions et sortie de groupe
// =====================================================================================================================

int32 ABRCharacter::GetMissionHoldDevice() const
{
	const ABRMissionDevice* D = HoldDevice.Get();
	return D ? D->GetIndex() : -1;
}

void ABRCharacter::RequestMissionAction(int32 Device, uint8 Action)
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W || bDead || !W->IsMissionActive() || Device < 0 || Device > 254)
	{
		return;
	}
	NextMissionRequest = static_cast<uint16>(NextMissionRequest % 65535 + 1);
	++MissionActionsSent;
	if (HasAuthority())
	{
		// Hote ou partie solo : la meme validation, appliquee tout de suite
		uint8 Related = 255, Count = 0;
		const uint8 Fb = W->ServerMissionAct(this, Device, Action, W->GetLevelSerial(), Related, Count);
		RememberApplied(AppliedMissionResults, AppliedMissionNext, NextMissionRequest);
		HandleMissionResult(Device, Fb, Related, Count);
		return;
	}
	ServerMissionInteract(static_cast<uint8>(Device), Action, NextMissionRequest, W->GetLevelSerial());
}

bool ABRCharacter::ServerMissionInteract_Validate(uint8 Device, uint8 Action, uint16 RequestId, int32 LevelSerial)
{
	return Action <= static_cast<uint8>(BRMission::EAction::Hold) && Device < BRMission::MaxDevices;
}

void ABRCharacter::ServerMissionInteract_Implementation(uint8 Device, uint8 Action, uint16 RequestId, int32 LevelSerial)
{
	// Demande deja servie (renvoi) : la meme reponse, sans second effet
	for (const FBRTxnRecord& R : ServerMissionHistory)
	{
		if (R.RequestId == RequestId && R.RequestId != 0 && R.Key == Device)
		{
			ClientMissionResult(Device, RequestId, R.Item, R.Result, R.Count, LevelSerial);
			return;
		}
	}
	ABRWorld* W = ABRWorld::Get(this);
	uint8 Related = 255, Count = 0;
	const uint8 Fb = W ? W->ServerMissionAct(this, Device, Action, LevelSerial, Related, Count) : BRMissionText::NotNow;
	FBRTxnRecord& Rec = ServerMissionHistory[ServerMissionHistoryNext];
	ServerMissionHistoryNext = (ServerMissionHistoryNext + 1) % TxnHistory;
	Rec = FBRTxnRecord();
	Rec.RequestId = RequestId;
	Rec.Key = Device;
	Rec.Item = Fb;
	Rec.Result = Related;
	Rec.Count = Count;
	ClientMissionResult(Device, RequestId, Fb, Related, Count, LevelSerial);
}

void ABRCharacter::ClientMissionResult_Implementation(uint8 Device, uint16 RequestId, uint8 Feedback, uint8 Related, uint8 Count, int32 LevelSerial)
{
	const ABRWorld* W = ABRWorld::Get(this);
	// Reponse repetee, ou d'un niveau quitte depuis : ignoree (l'etat officiel vient de toute facon de la replication)
	if (WasApplied(AppliedMissionResults, RequestId) || !W || LevelSerial != W->GetLevelSerial())
	{
		++MissionResultsIgnored;
		return;
	}
	RememberApplied(AppliedMissionResults, AppliedMissionNext, RequestId);
	HandleMissionResult(Device, Feedback, Related, Count);
}

void ABRCharacter::HandleMissionResult(int32 Device, uint8 Feedback, uint8 Related, uint8 Count)
{
	++MissionResultsReceived;
	LastMissionFeedback = Feedback;
	const ABRWorld* W = ABRWorld::Get(this);
	if (!W || !IsLocallyControlled() || !W->IsMissionActive() || Device < 0 || Device >= W->GetMissionPlan().NumDevices)
	{
		return;
	}
	const BRMission::FPlan& Plan = W->GetMissionPlan();
	const BRMission::FDevice& D = Plan.Devices[Device];
	const BRMission::EFeedback F = static_cast<BRMission::EFeedback>(Feedback);
	const bool bDone = F == BRMission::EFeedback::Done || F == BRMission::EFeedback::AlreadyDone;
	// Fin d'une action maintenue (objectif atteint, refus, ordre non respecte)
	if (HoldDevice.IsValid() && HoldDevice->GetIndex() == Device && F != BRMission::EFeedback::Progress)
	{
		HoldDevice.Reset();
		HoldTimer = 0.f;
	}
	if (bDone && (D.Kind == BRMission::EKind::Clue || D.Kind == BRMission::EKind::Observe))
	{
		// Indice lu ou observation documentee : le document s'ouvre (et reste dans le carnet)
		MissionDoc = Device;
		MissionDocWhere = GetActorLocation();
		PlaySound2D(D.Kind == BRMission::EKind::Clue ? TEXT("S_ItemMove") : TEXT("S_RecBeep"), 0.5f);
		return;
	}
	if (F == BRMission::EFeedback::Progress || F == BRMission::EFeedback::None)
	{
		return;
	}
	if (F == BRMission::EFeedback::Done && (D.Kind == BRMission::EKind::Switch || D.Kind == BRMission::EKind::Crank))
	{
		return; // le mecanisme montre lui-meme sa nouvelle position
	}
	const FString Msg = BRMissionText::Feedback(Plan, Device, Feedback, Related, Count);
	if (Msg.IsEmpty())
	{
		return;
	}
	const bool bGood = F == BRMission::EFeedback::Done || F == BRMission::EFeedback::Returned;
	const bool bWarn = F == BRMission::EFeedback::Warning || F == BRMission::EFeedback::Tripped;
	ABRHUD::Notify(this, Msg, bWarn ? 5.f : 3.5f, bGood ? FLinearColor(0.55f, 1.f, 0.55f) : (bWarn ? FLinearColor(1.f, 0.55f, 0.3f) : FLinearColor(1.f, 0.8f, 0.5f)));
	if (!bGood && !bWarn)
	{
		PlaySound2D(TEXT("S_UIDeny"), 0.4f);
	}
}

void ABRCharacter::UpdateMissionHold(float Dt)
{
	// Document ouvert : il se range si l'on s'eloigne
	if (MissionDoc >= 0 && FVector::DistSquared(GetActorLocation(), MissionDocWhere) > FMath::Square(250.f))
	{
		MissionDoc = -1;
	}
	ABRMissionDevice* D = HoldDevice.Get();
	if (!D)
	{
		return;
	}
	if (!bInteractHeld || bDead || bInputLocked || FocusActor.Get() != D)
	{
		HoldDevice.Reset();
		HoldTimer = 0.f;
		return;
	}
	HoldTimer += Dt;
	if (HoldTimer >= 0.5f)
	{
		HoldTimer -= 0.5f;
		RequestMissionAction(D->GetIndex(), static_cast<uint8>(BRMission::EAction::Hold));
	}
}

void ABRCharacter::RequestDepartureFromServer(int32 Target)
{
	if (const ABRWorld* W = ABRWorld::Get(this))
	{
		ServerRequestDeparture(Target, W->GetLevelSerial());
	}
}

bool ABRCharacter::ServerRequestDeparture_Validate(int32 Target, int32 LevelSerial)
{
	return true;
}

void ABRCharacter::ServerRequestDeparture_Implementation(int32 Target, int32 LevelSerial)
{
	ABRWorld* W = ABRWorld::Get(this);
	uint8 Reason = 0;
	if (W && !W->ServerStartDeparture(this, Target, LevelSerial, Reason))
	{
		ClientDepartureRefused(Reason);
	}
}

void ABRCharacter::ClientDepartureRefused_Implementation(uint8 Reason)
{
	NotifyDepartureRefused(Reason);
}

void ABRCharacter::NotifyDepartureRefused(uint8 Reason)
{
	FString Msg;
	switch (Reason)
	{
	case 1:
		Msg = BR_STR(NSLOCTEXT("BR", "Mission.Depart.RefusedLocked", "Cette sortie est encore verrouill\u00e9e."));
		break;
	case 2:
		Msg = BR_STR(NSLOCTEXT("BR", "Mission.Depart.RefusedFar", "Approchez-vous de la sortie."));
		break;
	case 3:
		Msg = BR_STR(NSLOCTEXT("BR", "Mission.Depart.RefusedOther", "Un autre d\u00e9part est d\u00e9j\u00e0 pr\u00e9vu : rejoignez le groupe."));
		break;
	default:
		return; // demande d'un niveau quitte, ou joueur a terre : rien a dire
	}
	ABRHUD::Notify(this, Msg, 3.5f, FLinearColor(1.f, 0.75f, 0.4f));
}
