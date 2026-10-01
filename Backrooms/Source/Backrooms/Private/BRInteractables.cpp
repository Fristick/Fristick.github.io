#include "BRInteractables.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRWorld.h"
#include "BRLevels.h"
#include "BRCharacter.h"
#include "BRItems.h"
#include "BRHUD.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

// =====================================================================================================================
// Objets a ramasser
// =====================================================================================================================

ABRPickup::ABRPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(28.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABRPickup::Init(EBRItem InItem, uint64 InId, const FString& InNote)
{
	Item = InItem;
	Id = InId;
	NoteText = InNote;

	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	const FBRItemInfo& Info = BRItems::Get(Item);

	// Taille de la boite de repli (si les modeles Blender ne sont pas importes) et echelle d'affichage
	FVector FallbackSize(8.f, 8.f, 24.f);
	FLinearColor FallbackColor(0.9f, 0.85f, 0.7f);
	float Scale = 1.2f;
	switch (Item)
	{
	case EBRItem::AlmondWater:
		Scale = 1.25f;
		break;
	case EBRItem::Battery:
		FallbackSize = FVector(10.f, 10.f, 5.f);
		FallbackColor = FLinearColor(0.1f, 0.1f, 0.1f);
		Scale = 1.4f;
		break;
	case EBRItem::Note:
		FallbackSize = FVector(21.f, 30.f, 0.5f);
		FallbackColor = FLinearColor(0.9f, 0.88f, 0.8f);
		Scale = 1.f;
		break;
	case EBRItem::Bandage:
		FallbackSize = FVector(12.f, 6.f, 6.f);
		FallbackColor = FLinearColor(0.92f, 0.9f, 0.85f);
		Scale = 1.35f;
		break;
	case EBRItem::EnergyBar:
		FallbackSize = FVector(13.f, 4.f, 1.6f);
		FallbackColor = FLinearColor(0.8f, 0.25f, 0.1f);
		Scale = 1.4f;
		break;
	case EBRItem::VHSTape:
		FallbackSize = FVector(19.f, 10.f, 2.5f);
		FallbackColor = FLinearColor(0.03f, 0.03f, 0.03f);
		Scale = 1.3f;
		break;
	case EBRItem::Flashlight:
		FallbackSize = FVector(25.f, 5.f, 5.f);
		FallbackColor = FLinearColor(0.08f, 0.08f, 0.08f);
		Scale = 1.15f;
		break;
	case EBRItem::Camcorder:
		FallbackSize = FVector(22.f, 9.f, 11.f);
		FallbackColor = FLinearColor(0.05f, 0.05f, 0.05f);
		Scale = 1.1f;
		break;
	case EBRItem::Headlamp:
		FallbackSize = FVector(18.f, 17.f, 5.f);
		FallbackColor = FLinearColor(0.2f, 0.2f, 0.22f);
		Scale = 1.2f;
		break;
	case EBRItem::Vest:
		FallbackSize = FVector(20.f, 40.f, 50.f);
		FallbackColor = FLinearColor(0.3f, 0.32f, 0.25f);
		Scale = 1.f;
		break;
	default:
		break;
	}

	if (UStaticMesh* M = A->Mesh(Info.Mesh))
	{
		Mesh->SetStaticMesh(M);
		Mesh->SetRelativeScale3D(FVector(Scale));
		// Pose le modele sur le sol quelle que soit la position de son pivot
		const FBox B = M->GetBoundingBox();
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, -B.Min.Z * Scale - 1.f));
		if (Item == EBRItem::Vest)
		{
			// Gilet couche sur le sol
			Mesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
			Mesh->SetRelativeLocation(FVector(0.f, 0.f, B.Max.X * Scale - 1.f));
		}
		A->ApplySlots(Mesh);
	}
	else if (A->Cube())
	{
		Mesh->SetStaticMesh(A->Cube());
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, FallbackSize.Z * 0.5f - 1.f));
		Mesh->SetRelativeScale3D(FallbackSize / 100.f);
		FBRSurface S(TEXT("T_Grime"), FallbackColor, 100.f);
		Mesh->SetMaterial(0, A->Surface(S));
	}
	Collision->SetSphereRadius(Item == EBRItem::Vest ? 40.f : 28.f);

	// Les cassettes VHS (objectif) brillent tres legerement pour qu'on puisse les reperer
	if (Item == EBRItem::VHSTape)
	{
		Glint = NewObject<UPointLightComponent>(this);
		Glint->SetupAttachment(Collision);
		Glint->SetRelativeLocation(FVector(0.f, 0.f, 18.f));
		Glint->SetIntensityUnits(ELightUnits::Lumens);
		Glint->SetIntensity(6.f);
		Glint->SetLightColor(FLinearColor(0.75f, 0.85f, 1.f));
		Glint->SetAttenuationRadius(110.f);
		Glint->SetCastShadows(false);
		Glint->RegisterComponent();
		SetActorTickEnabled(true);
		Time = FMath::FRandRange(0.f, 5.f);
	}
}

void ABRPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	if (Glint)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 2.2f);
		Glint->SetIntensity(2.f + 8.f * Pulse * Pulse);
	}
}

FString ABRPickup::GetPrompt() const
{
	if (Item == EBRItem::Note)
	{
		return TEXT("[E] Lire la note");
	}
	return FString::Printf(TEXT("[E] Ramasser : %s"), *BRItems::Get(Item).Name);
}

void ABRPickup::Collect(ABRCharacter* By)
{
	if (!By || IsActorBeingDestroyed())
	{
		return;
	}
	if (!By->ReceivePickup(Item, NoteText))
	{
		return; // inventaire plein : l'objet reste au sol
	}
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->MarkCollected(Id);
	}
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		FName SoundName = TEXT("S_Pickup");
		if (Item == EBRItem::Battery)
		{
			SoundName = TEXT("S_Battery");
		}
		else if (Item == EBRItem::Note || Item == EBRItem::VHSTape)
		{
			SoundName = TEXT("S_ItemMove");
		}
		if (USoundBase* S = A->Sound(SoundName))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.8f);
		}
	}
	Destroy();
}

// =====================================================================================================================
// Sorties
// =====================================================================================================================

ABRExit::ABRExit()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(Root);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Audio = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
	Audio->SetupAttachment(Root);
	Audio->bAutoActivate = false;
}

bool ABRExit::IsInteractable() const
{
	return Style != EBRExitStyle::NoclipWall && Style != EBRExitStyle::NoclipFloor;
}

void ABRExit::Init(int32 InTarget, EBRExitStyle InStyle)
{
	Target = InTarget;
	Style = InStyle;
	UBRAssets* A = UBRAssets::Get(this);
	ABRWorld* W = ABRWorld::Get(this);
	if (!A || !W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();

	auto SetupInteractBox = [this](const FVector& Center, const FVector& Extent)
	{
		Box->SetRelativeLocation(Center);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	};
	auto SetupMesh = [this, A](FName Name, const FVector& FallbackSize, const FLinearColor& FallbackColor)
	{
		if (UStaticMesh* M = A->Mesh(Name))
		{
			Mesh->SetStaticMesh(M);
			A->ApplySlots(Mesh);
		}
		else if (A->Cube())
		{
			Mesh->SetStaticMesh(A->Cube());
			Mesh->SetRelativeLocation(FVector(FallbackSize.X * 0.5f, 0.f, FallbackSize.Z * 0.5f));
			Mesh->SetRelativeScale3D(FallbackSize / 100.f);
			Mesh->SetMaterial(0, A->Surface(FBRSurface(TEXT("T_Grime"), FallbackColor, 100.f)));
		}
	};

	switch (Style)
	{
	case EBRExitStyle::NoclipWall:
	case EBRExitStyle::NoclipFloor:
	{
		const bool bWall = Style == EBRExitStyle::NoclipWall;
		const float W2 = FMath::Min(D.CellSize - D.WallThickness * 2.f - 30.f, 220.f);
		const FVector Size = bWall ? FVector(3.f, W2, FMath::Min(D.WallHeight - 30.f, 240.f)) : FVector(220.f, 220.f, 2.f);
		Mesh->SetStaticMesh(A->Cube());
		Mesh->SetRelativeLocation(bWall ? FVector(1.f, 0.f, Size.Z * 0.5f + 5.f) : FVector(0.f, 0.f, 1.f));
		Mesh->SetRelativeScale3D(Size / 100.f);
		FBRSurface G(TEXT("T_Glitch"), FLinearColor(0.9f, 0.9f, 0.9f), 60.f, 0.5f, 0.f);
		G.Emissive = FLinearColor(0.6f, 0.5f, 0.8f);
		GlitchMID = UMaterialInstanceDynamic::Create(A->Surface(G), this);
		Mesh->SetMaterial(0, GlitchMID);
		// Zone de declenchement : il suffit de toucher
		Box->SetRelativeLocation(bWall ? FVector(25.f, 0.f, Size.Z * 0.5f) : FVector(0.f, 0.f, 60.f));
		Box->SetBoxExtent(bWall ? FVector(30.f, W2 * 0.5f, Size.Z * 0.5f) : FVector(100.f, 100.f, 60.f));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Box->SetGenerateOverlapEvents(true);
		SetActorTickEnabled(true);
		break;
	}
	case EBRExitStyle::Door:
		SetupMesh(TEXT("SM_ExitDoor"), FVector(6.f, 95.f, 210.f), FLinearColor(0.3f, 0.35f, 0.3f));
		SetupInteractBox(FVector(10.f, 0.f, 105.f), FVector(15.f, 50.f, 105.f));
		break;
	case EBRExitStyle::HotelDoor:
		SetupMesh(TEXT("SM_HotelDoor"), FVector(6.f, 92.f, 215.f), FLinearColor(0.3f, 0.18f, 0.1f));
		SetupInteractBox(FVector(10.f, 0.f, 105.f), FVector(15.f, 48.f, 105.f));
		break;
	case EBRExitStyle::Elevator:
		SetupMesh(TEXT("SM_ElevatorDoor"), FVector(8.f, 130.f, 225.f), FLinearColor(0.7f, 0.7f, 0.72f));
		SetupInteractBox(FVector(12.f, 0.f, 112.f), FVector(15.f, 70.f, 112.f));
		break;
	case EBRExitStyle::Ladder:
		SetupMesh(TEXT("SM_Ladder"), FVector(10.f, 45.f, 320.f), FLinearColor(0.4f, 0.4f, 0.42f));
		SetupInteractBox(FVector(15.f, 0.f, 150.f), FVector(20.f, 30.f, 150.f));
		break;
	case EBRExitStyle::Barn:
		SetupMesh(TEXT("SM_Barn"), FVector(1000.f, 800.f, 600.f), FLinearColor(0.5f, 0.12f, 0.08f));
		if (!A->Mesh(TEXT("SM_Barn")))
		{
			Mesh->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
		}
		SetupInteractBox(FVector(510.f, 0.f, 170.f), FVector(20.f, 160.f, 170.f));
		Body->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
		Body->SetBoxExtent(FVector(500.f, 400.f, 300.f));
		Body->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		break;
	case EBRExitStyle::HouseDoor:
		SetupInteractBox(FVector(5.f, 0.f, 135.f), FVector(20.f, 55.f, 110.f));
		break;
	case EBRExitStyle::BuildingDoor:
		SetupMesh(TEXT("SM_ExitDoor"), FVector(6.f, 95.f, 210.f), FLinearColor(0.3f, 0.35f, 0.3f));
		SetupInteractBox(FVector(10.f, 0.f, 105.f), FVector(15.f, 50.f, 105.f));
		break;
	}

	// Petite lueur pour reperer les portes (sauf dans le noir total du Niveau 6)
	if (Style == EBRExitStyle::Door || Style == EBRExitStyle::Elevator || Style == EBRExitStyle::HouseDoor || Style == EBRExitStyle::BuildingDoor)
	{
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(FVector(60.f, 0.f, 240.f));
		Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->SetIntensity(Style == EBRExitStyle::HouseDoor ? 400.f : 150.f);
		Light->SetLightColor(Style == EBRExitStyle::HouseDoor ? FLinearColor(1.f, 0.75f, 0.45f) : FLinearColor(0.3f, 1.f, 0.4f));
		Light->SetAttenuationRadius(350.f);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
	}

	// Un bourdonnement discret aide a trouver les sorties a l'oreille
	if (USoundBase* Hum = A->Sound(TEXT("S_ExitHum")))
	{
		Audio->SetSound(Hum);
		Audio->AttenuationSettings = A->Attenuation(IsInteractable() ? 900.f : 1400.f);
		Audio->SetVolumeMultiplier(IsInteractable() ? 0.25f : 0.6f);
		Audio->SetRelativeLocation(FVector(30.f, 0.f, 120.f));
		Audio->Play(FMath::FRandRange(0.f, 2.f));
	}
}

void ABRExit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	if (GlitchMID)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 7.f) * FMath::Sin(Time * 2.3f);
		const bool bJump = FMath::FRand() < 0.08f;
		GlitchMID->SetScalarParameterValue(TEXT("TexScale"), bJump ? FMath::FRandRange(20.f, 140.f) : 60.f + 10.f * FMath::Sin(Time * 3.f));
		GlitchMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.5f, 0.4f, 0.9f) * (0.6f + 2.5f * Pulse * Pulse));
		GlitchMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f + 0.5f * Pulse, 0.3f, 0.8f));
	}
	if (Mesh && Style == EBRExitStyle::NoclipWall && FMath::FRand() < 0.04f)
	{
		Mesh->SetRelativeLocation(FVector(1.f + FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-4.f, 4.f), Mesh->GetRelativeLocation().Z));
	}
}

void ABRExit::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (!IsInteractable())
	{
		if (ABRCharacter* C = Cast<ABRCharacter>(OtherActor))
		{
			Use(C);
		}
	}
}

FString ABRExit::GetPrompt() const
{
	const FString Dest = (Target >= 0 && BRLevels::Exists(Target)) ? FString::Printf(TEXT("Niveau %d ?"), Target) : FString(TEXT("???"));
	switch (Style)
	{
	case EBRExitStyle::Door:
		return FString::Printf(TEXT("[E] Ouvrir la porte de secours  (%s)"), *Dest);
	case EBRExitStyle::HotelDoor:
		return FString::Printf(TEXT("[E] Ouvrir la porte \"CHAUFFERIE\"  (%s)"), *Dest);
	case EBRExitStyle::Elevator:
		return FString::Printf(TEXT("[E] Prendre l'ascenseur  (%s)"), *Dest);
	case EBRExitStyle::Ladder:
		return FString::Printf(TEXT("[E] Emprunter l'\u00e9chelle  (%s)"), *Dest);
	case EBRExitStyle::Barn:
		return FString::Printf(TEXT("[E] Entrer dans la grange  (%s)"), *Dest);
	case EBRExitStyle::HouseDoor:
		return FString::Printf(TEXT("[E] Pousser la porte entrouverte  (%s)"), *Dest);
	case EBRExitStyle::BuildingDoor:
		return FString::Printf(TEXT("[E] Entrer dans l'immeuble  (%s)"), *Dest);
	default:
		return FString();
	}
}

void ABRExit::Use(ABRCharacter* By)
{
	ABRWorld* W = ABRWorld::Get(this);
	if (bUsed || !By || !W || W->IsTransitioning() || By->IsDead())
	{
		return;
	}
	FString Reason;
	if (!W->CanLeaveLevel(Reason))
	{
		// Niveau 0 : la sortie reste instable tant que les objectifs ne sont pas remplis
		const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
		if (IsInteractable() || Now - LastDenied > 4.f)
		{
			LastDenied = Now;
			ABRHUD::Notify(this, Reason, 4.f, FLinearColor(1.f, 0.55f, 0.35f));
			if (UBRAssets* A = UBRAssets::Get(this))
			{
				if (USoundBase* S = A->Sound(TEXT("S_Flicker")))
				{
					UGameplayStatics::PlaySound2D(this, S, 0.6f);
				}
			}
		}
		return;
	}
	bUsed = true;
	if (IsInteractable())
	{
		if (UBRAssets* A = UBRAssets::Get(this))
		{
			if (USoundBase* S = A->Sound(TEXT("S_Door")))
			{
				UGameplayStatics::PlaySound2D(this, S, 0.9f);
			}
		}
	}
	W->RequestTransition(Target);
}
