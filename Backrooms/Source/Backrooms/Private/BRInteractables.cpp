#include "BRInteractables.h"
#include "BRMissionLogic.h"
#include "BRLoc.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRWorld.h"
#include "BRLevels.h"
#include "BRCharacter.h"
#include "BRItems.h"
#include "BRHUD.h"
#include "BRKeys.h"

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
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.ReadNote", "{Key} Lire la note"), { { TEXT("Key"), BRLoc::Arg(BRKeys::Tag(EBRAction::Interact)) } });
	}
	if (bPending)
	{
		return BR_STR(NSLOCTEXT("BR", "Interact.PickupPending", "Ramassage en cours\u2026 (r\u00e9ponse de l'h\u00f4te)"));
	}
	return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.InteractRamasserItem", "{Interact} Ramasser : {Item}"), { { TEXT("Interact"), BRLoc::Arg(BRKeys::Tag(EBRAction::Interact)) }, { TEXT("Item"), BRLoc::Arg(BRItems::Get(Item).Name) } });
}

void ABRPickup::Collect(ABRCharacter* By)
{
	if (!By || IsActorBeingDestroyed())
	{
		return;
	}
	if (Item == EBRItem::Note)
	{
		// v4.11 : une note n'est pas un objet consommable : chaque joueur la lit (et l'ajoute a son journal), elle reste en
		// place pour les autres. Rien n'est demande a l'hote.
		By->ReceivePickup(Item, NoteText);
		if (UBRAssets* A = UBRAssets::Get(this))
		{
			if (USoundBase* S = A->Sound(TEXT("S_ItemMove")))
			{
				UGameplayStatics::PlaySound2D(this, S, 0.6f);
			}
		}
		return;
	}
	// v4.11 : objet unique : transaction confirmee par l'hote (un seul gagnant, aucun objet accorde a l'avance)
	By->RequestPickup(this);
}

void ABRPickup::SetPending(bool bInPending)
{
	bPending = bInPending;
	if (Mesh)
	{
		// En attente de la reponse de l'hote : l'objet s'estompe un peu (aucun objet n'est encore donne)
		Mesh->SetScalarParameterValueOnMaterials(TEXT("Opacity"), bPending ? 0.5f : 1.f);
		Mesh->SetRenderCustomDepth(bPending);
	}
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
		GlitchMID = A->NewSurface(G, this);
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

	// v4.12 : passage condamne dans cette version : planches en croix et lueur rouge (la porte reste un repere du decor)
	if (IsSealed() && IsInteractable() && Style != EBRExitStyle::Barn)
	{
		const FBRSurface Plank(TEXT("T_Concrete"), FLinearColor(0.32f, 0.22f, 0.12f), 120.f, 0.9f, 0.6f);
		UMaterialInterface* PlankMat = A->NewSurface(Plank, this);
		const float H = Style == EBRExitStyle::Ladder ? 180.f : 110.f;
		for (int32 I = 0; I < 2; ++I)
		{
			UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(this);
			P->SetupAttachment(Root);
			P->SetStaticMesh(A->Cube());
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->SetRelativeLocation(FVector(14.f, 0.f, H));
			P->SetRelativeRotation(FRotator(0.f, 0.f, I == 0 ? 35.f : -35.f));
			P->SetRelativeScale3D(FVector(0.05f, 1.3f, 0.12f));
			P->SetMaterial(0, PlankMat);
			P->RegisterComponent();
			LadderParts.Add(P);
		}
		if (Light)
		{
			Light->SetLightColor(FLinearColor(1.f, 0.25f, 0.15f));
			Light->SetIntensity(90.f);
		}
	}

	// Un bourdonnement discret aide a trouver les sorties a l'oreille
	if (USoundBase* Hum = A->Sound(TEXT("S_ExitHum")))
	{
		Audio->SetSound(Hum);
		// v4.7 : signal fiable d'une sortie : pas etouffe par les murs (on peut s'orienter a l'oreille)
		Audio->AttenuationSettings = A->Attenuation(IsInteractable() ? 900.f : 1400.f, false);
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
	if (!IsInteractable() && !IsSealed())
	{
		if (ABRCharacter* C = Cast<ABRCharacter>(OtherActor))
		{
			Use(C);
		}
	}
}

bool ABRExit::IsSealed() const
{
	const ABRWorld* W = ABRWorld::Get(this);
	return W && BRLevels::ResolveExit(W->GetLevelNumber(), Target).Kind == BRContent::EExit::Sealed;
}

FString ABRExit::DestinationLabel() const
{
	const ABRWorld* W = ABRWorld::Get(this);
	const BRContent::FExitResolution R = BRLevels::ResolveExit(W ? W->GetLevelNumber() : 0, Target);
	switch (R.Kind)
	{
	case BRContent::EExit::Ending:
		return BR_STR(NSLOCTEXT("BR", "Interact.LastPlatform", "dernier quai"));
	case BRContent::EExit::ChapterEnd:
		return BR_STR(NSLOCTEXT("BR", "Interact.ChapterEnd", "fin du contenu disponible"));
	case BRContent::EExit::Go:
		return R.Target >= 0 ? BRLoc::Fmt(NSLOCTEXT("BR", "Interact.NiveauTarget", "Niveau {Target} ?"), { { TEXT("Target"), BRLoc::Int(R.Target) } }) : FString(TEXT("???"));
	default:
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.NiveauTarget", "Niveau {Target} ?"), { { TEXT("Target"), BRLoc::Int(Target) } });
	}
}

FString ABRExit::GetPrompt() const
{
	// v4.12 : la destination suit la disponibilite des niveaux de cette version (redirection, fin du contenu disponible)
	const FString Dest = DestinationLabel();
	const FString Key = BRKeys::Tag(EBRAction::Interact);
	if (IsSealed())
	{
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.ExitSealed", "Passage condamn\u00e9 ({Dest}) : pas encore accessible dans cette version"), { { TEXT("Dest"), BRLoc::Arg(Dest) } });
	}
	// v4.11 : sortie gardee par la mission : on le dit dans l'invite
	if (const ABRWorld* W = ABRWorld::Get(this))
	{
		FString Reason;
		if (!W->CanUseExit(Target, Reason))
		{
			return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.ExitLocked", "Sortie verrouill\u00e9e ({Dest}) : mission du niveau en cours"), { { TEXT("Dest"), BRLoc::Arg(Dest) } });
		}
	}
	switch (Style)
	{
	case EBRExitStyle::Door:
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.KeyOuvrirPorteSecoursDest", "{Key} Ouvrir la porte de secours  ({Dest})"), { { TEXT("Key"), BRLoc::Arg(Key) }, { TEXT("Dest"), BRLoc::Arg(Dest) } });
	case EBRExitStyle::HotelDoor:
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.KeyOuvrirPorteChaufferieDest", "{Key} Ouvrir la porte \"CHAUFFERIE\"  ({Dest})"), { { TEXT("Key"), BRLoc::Arg(Key) }, { TEXT("Dest"), BRLoc::Arg(Dest) } });
	case EBRExitStyle::Elevator:
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.KeyPrendreAscenseurDest", "{Key} Prendre l'ascenseur  ({Dest})"), { { TEXT("Key"), BRLoc::Arg(Key) }, { TEXT("Dest"), BRLoc::Arg(Dest) } });
	case EBRExitStyle::Ladder:
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.KeyMonterEchelleDest", "{Key} Monter \u00e0 l'\u00e9chelle  ({Dest})"), { { TEXT("Key"), BRLoc::Arg(Key) }, { TEXT("Dest"), BRLoc::Arg(Dest) } });
	case EBRExitStyle::Barn:
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.KeyEntrerGrangeDest", "{Key} Entrer dans la grange  ({Dest})"), { { TEXT("Key"), BRLoc::Arg(Key) }, { TEXT("Dest"), BRLoc::Arg(Dest) } });
	case EBRExitStyle::HouseDoor:
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.KeyPousserPorteEntrouverteDest", "{Key} Pousser la porte entrouverte  ({Dest})"), { { TEXT("Key"), BRLoc::Arg(Key) }, { TEXT("Dest"), BRLoc::Arg(Dest) } });
	case EBRExitStyle::BuildingDoor:
		return BRLoc::Fmt(NSLOCTEXT("BR", "Interact.KeyEntrerImmeubleDest", "{Key} Entrer dans l'immeuble  ({Dest})"), { { TEXT("Key"), BRLoc::Arg(Key) }, { TEXT("Dest"), BRLoc::Arg(Dest) } });
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
	// Multijoueur : chez un client seul son propre personnage compte (le serveur voit passer tout le monde)
	if (!W->HasAuthority() && !By->IsLocallyControlled())
	{
		return;
	}
	FString Reason;
	// v4.12 : passage condamne (niveau pas encore disponible) : annonce, rien d'autre
	if (IsSealed())
	{
		if (By->IsLocallyControlled())
		{
			const float NowS = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
			if (NowS - LastDenied > 2.f)
			{
				LastDenied = NowS;
				ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Interact.ExitSealedUse", "Ce passage ne m\u00e8ne nulle part pour l'instant : le niveau suivant arrivera dans une prochaine mise \u00e0 jour.")), 4.f,
					FLinearColor(1.f, 0.75f, 0.45f));
			}
		}
		return;
	}
	// v4.11 : la sortie suit la mission du niveau (sorties de retour toujours ouvertes)
	if (!W->CanUseExit(Target, Reason))
	{
		if (!By->IsLocallyControlled())
		{
			return; // le message est pour le joueur qui a touche la sortie, sur son ecran
		}
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
	if (IsClimbable())
	{
		// Echelle : on y grimpe (chez le joueur qui la prend) ; le noclip a lieu en haut (FinishClimb)
		if (By->IsLocallyControlled() && !By->IsClimbing())
		{
			By->StartClimb(this);
		}
		return;
	}
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
	Leave(By);
}

void ABRExit::Leave(ABRCharacter* By)
{
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		// v4.11 : seul, on part tout de suite ; en ligne, l'hote rassemble le groupe avant le depart (ABRWorld)
		if (!W->IsNetGame())
		{
			bUsed = true;
		}
		W->RequestDeparture(By, Target, GetActorLocation());
	}
}

void ABRExit::FinishClimb(ABRCharacter* By)
{
	ABRWorld* W = ABRWorld::Get(this);
	if (bUsed || !By || !W || W->IsTransitioning())
	{
		return;
	}
	FString Reason;
	if (!W->CanUseExit(Target, Reason))
	{
		// v4.12 : on reste au sommet (plus de chute de l'echelle) ; redescendre puis remonter redemande
		ABRHUD::Notify(this, Reason, 4.f, FLinearColor(1.f, 0.55f, 0.35f));
		By->OnClimbExitRefused();
		return;
	}
	Leave(By);
}

FVector ABRExit::GetClimbAnchor() const
{
	// Montants a 14 cm du mur, capsule de 34 cm de rayon
	return GetActorLocation() + GetActorForwardVector() * 52.f;
}

void ABRExit::InitLadder(float CeilingZ, float ShaftHeight)
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!A || !Mesh)
	{
		return;
	}
	// Echelle jusqu'en haut du conduit : segments de 3,2 m legerement resserres
	const float Total = CeilingZ + FMath::Max(0.f, ShaftHeight);
	const int32 Count = FMath::Max(1, FMath::CeilToInt(Total / 320.f - 0.05f));
	const float Seg = Total / Count;
	const bool bModel = A->Mesh(TEXT("SM_Ladder")) != nullptr;
	for (int32 i = 0; i < Count; ++i)
	{
		UStaticMeshComponent* Part = Mesh;
		if (i > 0)
		{
			Part = NewObject<UStaticMeshComponent>(this);
			Part->SetupAttachment(Root);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetStaticMesh(Mesh->GetStaticMesh());
			for (int32 m = 0; m < Mesh->GetNumMaterials(); ++m)
			{
				Part->SetMaterial(m, Mesh->GetMaterial(m));
			}
			Part->RegisterComponent();
			LadderParts.Add(Part);
		}
		if (bModel)
		{
			Part->SetRelativeLocation(FVector(0.f, 0.f, i * Seg));
			Part->SetRelativeScale3D(FVector(1.f, 1.f, Seg / 320.f));
		}
		else
		{
			Part->SetRelativeLocation(FVector(5.f, 0.f, i * Seg + Seg * 0.5f));
			Part->SetRelativeScale3D(FVector(0.1f, 0.45f, Seg / 100.f));
		}
	}
	// Noclip au milieu du conduit ; sans conduit, quand la tete touche le plafond
	constexpr float CapsuleHalf = 88.f;
	ClimbTopZ = static_cast<float>(GetActorLocation().Z) + (ShaftHeight > 0.f ? CeilingZ + ShaftHeight * 0.5f : CeilingZ - CapsuleHalf - 6.f);
	// On attrape l'echelle a n'importe quelle hauteur de la piece
	Box->SetRelativeLocation(FVector(20.f, 0.f, CeilingZ * 0.5f));
	Box->SetBoxExtent(FVector(22.f, 32.f, CeilingZ * 0.5f));
	if (ShaftHeight <= 0.f || !A->Cube())
	{
		return;
	}
	// En haut du conduit, la realite se dechire : une plaque qui "glitche" et sa lueur violette, visible par la trappe
	UStaticMeshComponent* Tear = NewObject<UStaticMeshComponent>(this);
	Tear->SetupAttachment(Root);
	Tear->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tear->SetStaticMesh(A->Cube());
	Tear->SetRelativeLocation(FVector(55.f, 0.f, CeilingZ + ShaftHeight - 8.f));
	Tear->SetRelativeScale3D(FVector(1.f, 1.15f, 0.02f));
	FBRSurface G(TEXT("T_Glitch"), FLinearColor(0.9f, 0.9f, 0.9f), 60.f, 0.5f, 0.f);
	G.Emissive = FLinearColor(0.6f, 0.5f, 0.8f);
	GlitchMID = A->NewSurface(G, this);
	Tear->SetMaterial(0, GlitchMID);
	Tear->RegisterComponent();
	LadderParts.Add(Tear);
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(55.f, 0.f, CeilingZ + ShaftHeight - 40.f));
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(260.f);
	Light->SetLightColor(FLinearColor(0.55f, 0.45f, 1.f));
	Light->SetAttenuationRadius(ShaftHeight + 260.f);
	Light->RegisterComponent();
	SetActorTickEnabled(true);
}
