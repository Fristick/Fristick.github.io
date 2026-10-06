// v4.11 : mecanismes de mission. Chaque role a un aspect lisible (plaques, cables, jauges, poignees, etiquettes, lampe
// d'etat) construit a partir des formes de base et des modeles existants ; aucune lumiere dynamique par mecanisme sauf
// les balises du Niveau 6 (seul repere dans le noir). L'etat affiche vient de l'etat replique par l'hote.
#include "BRMission.h"
#include "BRHUD.h"
#include "BRAssets.h"
#include "BRWorld.h"
#include "BRCharacter.h"
#include "BRKeys.h"
#include "BRLoc.h"
#include "Backrooms.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace BRM = BRMission;

namespace
{
	FBRSurface MetalSurf(const FLinearColor& Tint, float Rough = 0.45f)
	{
		FBRSurface S(TEXT("T_MetalPanel"), Tint, 60.f, Rough, 0.3f);
		S.Metallic = 0.6f;
		return S;
	}

	/** Hauteur de travail des commandes murales (cm au-dessus du sol) */
	constexpr float WorkZ = 120.f;
}

ABRMissionDevice::ABRMissionDevice()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(Root);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(Root);
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UStaticMeshComponent* ABRMissionDevice::AddPart(UStaticMesh* InMesh, const FVector& Loc, const FRotator& Rot, const FVector& Size, UMaterialInterface* Mat,
	USceneComponent* Parent)
{
	if (!InMesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent ? Parent : Root.Get());
	C->SetStaticMesh(InMesh);
	C->SetRelativeLocation(Loc);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(Size / 100.f);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	if (Mat)
	{
		C->SetMaterial(0, Mat);
	}
	C->RegisterComponent();
	Parts.Add(C);
	return C;
}

UStaticMeshComponent* ABRMissionDevice::AddBoxPart(const FVector& Loc, const FVector& Size, UMaterialInterface* Mat, USceneComponent* Parent, const FRotator& Rot)
{
	UBRAssets* A = UBRAssets::Get(this);
	return A ? AddPart(A->Cube(), Loc, Rot, Size, Mat, Parent) : nullptr;
}

void ABRMissionDevice::AddSymbol(uint8 Symbol, const FVector& Center, float Size, UMaterialInterface* Mat, USceneComponent* Parent)
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	const float T = FMath::Max(1.5f, Size * 0.14f);
	const float D = 1.f;
	switch (Symbol)
	{
	case 0: // triangle : trois barres d'un sommet a l'autre (le roulis R tourne l'axe Y vers -Z : R = -angle)
	{
		const float R = Size * 0.55f;
		FVector2D V[3];
		for (int32 I = 0; I < 3; ++I)
		{
			const float Ang = FMath::DegreesToRadians(90.f + I * 120.f);
			V[I] = FVector2D(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R);
		}
		for (int32 I = 0; I < 3; ++I)
		{
			const FVector2D P0 = V[I];
			const FVector2D P1 = V[(I + 1) % 3];
			const FVector2D Mid = (P0 + P1) * 0.5f;
			const float Len = static_cast<float>((P1 - P0).Size());
			const float Theta = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(P1.Y - P0.Y), static_cast<float>(P1.X - P0.X)));
			AddBoxPart(Center + FVector(0.f, Mid.X, Mid.Y), FVector(D, Len + T, T), Mat, Parent, FRotator(0.f, 0.f, -Theta));
		}
		break;
	}
	case 1: // cercle (disque tourne face au joueur)
		AddPart(A->Cylinder(), Center, FRotator(90.f, 0.f, 0.f), FVector(Size, Size, D), Mat, Parent);
		break;
	case 2: // carre
		AddBoxPart(Center, FVector(D, Size * 0.8f, Size * 0.8f), Mat, Parent);
		break;
	case 3: // losange
		AddBoxPart(Center, FVector(D, Size * 0.62f, Size * 0.62f), Mat, Parent, FRotator(0.f, 0.f, 45.f));
		break;
	case 4: // croix
		AddBoxPart(Center, FVector(D, Size, T), Mat, Parent);
		AddBoxPart(Center, FVector(D, T, Size), Mat, Parent);
		break;
	default: // etoile (six branches)
		for (int32 I = 0; I < 3; ++I)
		{
			AddBoxPart(Center, FVector(D, Size, T), Mat, Parent, FRotator(0.f, 0.f, I * 60.f));
		}
		break;
	}
}

void ABRMissionDevice::AddLabel(const FString& Text, const FVector& Loc, float Size, const FColor& Color, USceneComponent* Parent)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetupAttachment(Parent ? Parent : Root.Get());
	T->SetRelativeLocation(Loc);
	// Le texte regarde +X (vers le joueur)
	T->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Color);
	T->SetText(FText::AsCultureInvariant(Text));
	T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	T->SetCastShadow(false);
	T->RegisterComponent();
	Labels.Add(T);
}

void ABRMissionDevice::BuildWallPlate(float W, float H, UMaterialInterface* Mat)
{
	AddBoxPart(FVector(1.5f, 0.f, WorkZ + 20.f), FVector(3.f, W, H), Mat);
	// Vis d'angle et cable vers le plafond : une plaque fixee, pas un objet pose
	UBRAssets* A = UBRAssets::Get(this);
	UMaterialInterface* Dark = A ? A->Surface(MetalSurf(FLinearColor(0.12f, 0.12f, 0.13f), 0.6f)) : nullptr;
	for (int32 I = 0; I < 4; ++I)
	{
		const float SY = (I & 1) ? 1.f : -1.f;
		const float SZ = (I & 2) ? 1.f : -1.f;
		AddBoxPart(FVector(3.4f, SY * (W * 0.5f - 3.f), WorkZ + 20.f + SZ * (H * 0.5f - 3.f)), FVector(1.f, 2.f, 2.f), Dark);
	}
}

void ABRMissionDevice::Init(int32 InIndex, const BRM::FPlan& Plan, const FBRMissionSpot& Spot)
{
	Index = InIndex;
	Device = Plan.Devices[InIndex];
	Level = Plan.Level;
	Cell = Spot.Cell;
	bWall = Spot.bWall;
	SetActorLocationAndRotation(Spot.Pos, FRotator(0.f, Spot.Yaw, 0.f));

	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	UMaterialInterface* Metal = A->Surface(MetalSurf(FLinearColor(0.55f, 0.57f, 0.6f)));
	UMaterialInterface* DarkMetal = A->Surface(MetalSurf(FLinearColor(0.14f, 0.15f, 0.16f), 0.55f));
	UMaterialInterface* Painted = A->Surface(MetalSurf(FLinearColor(0.32f, 0.38f, 0.3f), 0.6f));
	UMaterialInterface* Paper = A->Surface(FBRSurface(TEXT("T_Paper"), FLinearColor(0.92f, 0.9f, 0.82f), 40.f, 0.9f, 0.2f));
	UMaterialInterface* Wood = A->Surface(FBRSurface(TEXT("T_Grime"), FLinearColor(0.42f, 0.3f, 0.2f), 80.f, 0.8f, 0.4f));
	UMaterialInterface* InkMat = A->Surface(FBRSurface(TEXT("T_Grime"), FLinearColor(0.05f, 0.05f, 0.06f), 50.f, 0.7f, 0.1f));
	UMaterialInterface* RedPaint = A->Surface(FBRSurface(TEXT("T_Grime"), FLinearColor(0.45f, 0.05f, 0.04f), 50.f, 0.6f, 0.2f));
	UMaterialInterface* Brass = A->Surface(MetalSurf(FLinearColor(0.75f, 0.58f, 0.3f), 0.35f));
	UMaterialInterface* Hazard = A->Surface(FBRSurface(TEXT("T_Hazard"), FLinearColor(1.f, 1.f, 1.f), 40.f, 0.6f, 0.3f));
	LampMID = A->NewGlow(this, FLinearColor(1.f, 0.15f, 0.1f), 0.f);

	// Exterieur : un poteau porte le mecanisme
	if (!bWall && Device.Kind != BRM::EKind::Gate)
	{
		AddBoxPart(FVector(-6.f, 0.f, 80.f), FVector(10.f, 10.f, 160.f), Device.Role == BRM::R_FenceMark || Device.Role == BRM::R_BarnBoard ? Wood : DarkMetal);
	}

	const FColor LabelColor(235, 230, 210);
	const float Z = WorkZ;
	FVector BoxCenter(8.f, 0.f, Z);
	FVector BoxExtent(10.f, 22.f, 25.f);
	USceneComponent* Pivot = nullptr;
	auto MakePivot = [this](const FVector& Loc)
	{
		USceneComponent* P = NewObject<USceneComponent>(this);
		P->SetupAttachment(Root);
		P->SetRelativeLocation(Loc);
		P->RegisterComponent();
		Moving = P;
		return P;
	};

	switch (Device.Kind)
	{
	case BRM::EKind::Clue:
	{
		const bool bMetalPlate = Device.Role == BRM::R_Schematic || Device.Role == BRM::R_PressurePlate || Device.Role == BRM::R_LoadBoard || Device.Role == BRM::R_StartPlate
			|| Device.Role == BRM::R_CityBoard || Device.Role == BRM::R_LevelMarks;
		const bool bNotebook = Device.Role == BRM::R_ReserveNote || Device.Role == BRM::R_DiveLog || Device.Role == BRM::R_Register || Device.Role == BRM::R_Archive;
		if (Device.Role == BRM::R_PassageMarks)
		{
			// Entailles dans la roche
			for (int32 I = 0; I < 5; ++I)
			{
				AddBoxPart(FVector(1.f, -18.f + I * 9.f, Z + 10.f + (I % 2) * 6.f), FVector(2.f, 2.f, 26.f), InkMat, nullptr, FRotator(0.f, 0.f, -20.f + I * 10.f));
			}
		}
		else if (bNotebook)
		{
			// Carnet ou registre pose sur une tablette
			AddBoxPart(FVector(14.f, 0.f, Z - 8.f), FVector(28.f, 46.f, 3.f), Wood);
			AddBoxPart(FVector(14.f, 0.f, Z - 5.f), FVector(20.f, 28.f, 3.f), Device.Role == BRM::R_Register ? RedPaint : Paper);
			BoxCenter = FVector(14.f, 0.f, Z - 4.f);
			BoxExtent = FVector(14.f, 18.f, 8.f);
		}
		else
		{
			BuildWallPlate(bMetalPlate ? 56.f : 42.f, bMetalPlate ? 40.f : 54.f, bMetalPlate ? Metal : (Device.Role == BRM::R_BarnBoard ? Wood : Paper));
			// Lignes de texte et schema (illisibles de loin : il faut le lire de pres)
			for (int32 I = 0; I < 5; ++I)
			{
				AddBoxPart(FVector(3.6f, -4.f + (I % 2) * 4.f, Z + 34.f - I * 7.f), FVector(0.5f, bMetalPlate ? 40.f : 30.f, 1.2f), InkMat);
			}
			if (Device.Role == BRM::R_Schematic || Device.Role == BRM::R_LoadBoard)
			{
				AddBoxPart(FVector(3.7f, 18.f, Z + 10.f), FVector(0.5f, 1.2f, 22.f), Hazard);
			}
			BoxCenter = FVector(5.f, 0.f, Z + 20.f);
			BoxExtent = FVector(6.f, 28.f, 28.f);
		}
		break;
	}
	case BRM::EKind::Observe:
		switch (Device.Role)
		{
		case BRM::R_Anomaly:
		{
			// Neon au plafond qui clignote par series ; son motif est peint au mur, a hauteur des yeux
			GlowMID = A->NewGlow(this, FLinearColor(1.f, 0.96f, 0.82f), 6.f);
			const float H = 230.f;
			if (UStaticMesh* Panel = A->Mesh(TEXT("SM_LightPanel")))
			{
				UStaticMeshComponent* C = AddPart(Panel, FVector(40.f, 0.f, H), FRotator::ZeroRotator, FVector(100.f, 100.f, 100.f), nullptr);
				if (C)
				{
					for (int32 S = 0; S < C->GetNumMaterials(); ++S)
					{
						C->SetMaterial(S, GlowMID);
					}
				}
			}
			else
			{
				AddBoxPart(FVector(40.f, 0.f, H), FVector(60.f, 120.f, 4.f), GlowMID);
			}
			AddSymbol(Device.Label, FVector(1.5f, 0.f, 150.f), 34.f, RedPaint);
			// Traces sur le mur : coulures sous le neon
			for (int32 I = 0; I < 3; ++I)
			{
				AddBoxPart(FVector(1.f, -20.f + I * 18.f, 190.f - I * 6.f), FVector(1.f, 3.f, 40.f + I * 10.f), InkMat);
			}
			Blinks = Device.Info[0];
			bAnimated = true;
			BoxCenter = FVector(20.f, 0.f, 170.f);
			BoxExtent = FVector(25.f, 40.f, 70.f);
			break;
		}
		case BRM::R_Gauge:
		{
			// Manometre rond au bout d'une conduite qui sort du mur
			AddPart(A->Cylinder(), FVector(6.f, 0.f, Z + 60.f), FRotator::ZeroRotator, FVector(7.f, 7.f, 120.f), DarkMetal);
			AddPart(A->Cylinder(), FVector(8.f, 0.f, Z), FRotator(90.f, 0.f, 0.f), FVector(30.f, 30.f, 8.f), Brass);
			AddPart(A->Cylinder(), FVector(12.5f, 0.f, Z), FRotator(90.f, 0.f, 0.f), FVector(26.f, 26.f, 1.f), Paper);
			for (int32 I = 0; I <= 4; ++I)
			{
				const float Ang = FMath::DegreesToRadians(-135.f + I * 67.5f);
				AddBoxPart(FVector(13.2f, FMath::Sin(Ang) * 10.5f, Z + FMath::Cos(Ang) * 10.5f), FVector(0.5f, 1.f, 3.f), InkMat, nullptr, FRotator(0.f, 0.f, FMath::RadiansToDegrees(Ang)));
			}
			Pivot = MakePivot(FVector(13.6f, 0.f, Z));
			MovingMesh = AddBoxPart(FVector(0.f, 0.f, 5.f), FVector(0.6f, 1.2f, 10.f), RedPaint, Pivot);
			AddLabel(FString::Printf(TEXT("M%d"), Device.Label + 1), FVector(13.8f, 0.f, Z - 22.f), 8.f, LabelColor);
			BoxCenter = FVector(10.f, 0.f, Z);
			BoxExtent = FVector(8.f, 18.f, 18.f);
			break;
		}
		case BRM::R_JunctionBox:
		{
			if (UStaticMesh* EBox = A->Mesh(TEXT("SM_ElectricBox")))
			{
				AddPart(EBox, FVector(0.f, 0.f, Z), FRotator::ZeroRotator, FVector(100.f, 100.f, 100.f), nullptr);
			}
			else
			{
				AddBoxPart(FVector(10.f, 0.f, Z + 10.f), FVector(20.f, 40.f, 55.f), Painted);
			}
			AddLabel(BRMissionText::Letter(Device.Label), FVector(22.f, 0.f, Z + 30.f), 14.f, LabelColor);
			if (Device.Info[0])
			{
				// Secteur en defaut : traces de brule et crepitement (signe visible avant meme de l'examiner)
				AddBoxPart(FVector(21.f, 6.f, Z + 4.f), FVector(1.f, 22.f, 26.f), InkMat);
				GlowMID = A->NewGlow(this, FLinearColor(0.6f, 0.75f, 1.f), 0.f);
				AddBoxPart(FVector(22.f, 10.f, Z - 6.f), FVector(1.f, 3.f, 3.f), GlowMID);
				bAnimated = true;
			}
			BoxCenter = FVector(12.f, 0.f, Z + 10.f);
			BoxExtent = FVector(12.f, 24.f, 32.f);
			break;
		}
		case BRM::R_FenceMark:
		{
			AddBoxPart(FVector(0.f, 0.f, 70.f), FVector(8.f, 120.f, 14.f), Wood);
			AddBoxPart(FVector(0.f, 0.f, 110.f), FVector(8.f, 120.f, 14.f), Wood);
			AddSymbol(Device.Info[0], FVector(4.5f, 20.f, 90.f), 26.f, RedPaint);
			BoxCenter = FVector(4.f, 0.f, 90.f);
			BoxExtent = FVector(8.f, 60.f, 30.f);
			break;
		}
		default: // BRM::R_Current : fleche flottante au bord du trottoir
		{
			AddPart(A->Cylinder(), FVector(40.f, 0.f, -8.f), FRotator::ZeroRotator, FVector(30.f, 30.f, 10.f), Hazard);
			Pivot = MakePivot(FVector(40.f, 0.f, 0.f));
			AddBoxPart(FVector(0.f, 0.f, 0.f), FVector(4.f, 40.f, 4.f), RedPaint, Pivot);
			AddBoxPart(FVector(0.f, 18.f, 0.f), FVector(4.f, 14.f, 4.f), RedPaint, Pivot, FRotator(0.f, 35.f, 0.f));
			AddBoxPart(FVector(0.f, 18.f, 0.f), FVector(4.f, 14.f, 4.f), RedPaint, Pivot, FRotator(0.f, -35.f, 0.f));
			AddLabel(TEXT("A  >  B"), FVector(2.f, 0.f, 100.f), 14.f, LabelColor);
			bAnimated = true;
			BoxCenter = FVector(30.f, 0.f, 40.f);
			BoxExtent = FVector(30.f, 30.f, 60.f);
			break;
		}
		}
		break;
	case BRM::EKind::Switch:
	{
		const bool bToggle = Device.Role == BRM::R_Breaker || Device.Role == BRM::R_Relay || Device.Role == BRM::R_RouteLever;
		const bool bWheel = Device.Role == BRM::R_Valve || Device.Role == BRM::R_Sluice || Device.Role == BRM::R_BoilerDial;
		if (bToggle)
		{
			const bool bBig = Device.Role == BRM::R_RouteLever || Device.Role == BRM::R_Relay;
			AddBoxPart(FVector(6.f, 0.f, Z), FVector(12.f, bBig ? 26.f : 16.f, bBig ? 40.f : 28.f), Painted);
			Pivot = MakePivot(FVector(12.f, 0.f, Z));
			MovingMesh = AddBoxPart(FVector(bBig ? 14.f : 8.f, 0.f, 0.f), FVector(bBig ? 28.f : 16.f, 4.f, 4.f), Device.Role == BRM::R_RouteLever ? Hazard : DarkMetal, Pivot);
			AddBoxPart(FVector(bBig ? 28.f : 16.f, 0.f, 0.f), FVector(6.f, 7.f, 7.f), RedPaint, Pivot);
			const FString Lbl = Device.Role == BRM::R_RouteLever ? FString(TEXT("1  |  37")) : (Device.Role == BRM::R_Relay ? FString::FromInt(Device.Label + 1) : BRMissionText::Letter(Device.Label));
			AddLabel(Lbl, FVector(12.5f, 0.f, Z + (bBig ? 26.f : 19.f)), bBig ? 10.f : 8.f, LabelColor);
			AddBoxPart(FVector(12.5f, bBig ? 10.f : 6.f, Z - (bBig ? 14.f : 10.f)), FVector(1.f, 3.f, 3.f), LampMID);
		}
		else if (bWheel)
		{
			AddPart(A->Cylinder(), FVector(6.f, 0.f, Z), FRotator(90.f, 0.f, 0.f), FVector(10.f, 10.f, 12.f), DarkMetal);
			Pivot = MakePivot(FVector(14.f, 0.f, Z));
			const float R = Device.Role == BRM::R_Sluice ? 26.f : 18.f;
			for (int32 I = 0; I < 3; ++I)
			{
				AddBoxPart(FVector(0.f, 0.f, 0.f), FVector(2.f, R * 2.f, 3.f), RedPaint, Pivot, FRotator(0.f, 0.f, I * 60.f));
			}
			for (int32 I = 0; I < 12; ++I)
			{
				const float Ang = FMath::DegreesToRadians(I * 30.f);
				AddBoxPart(FVector(0.f, FMath::Cos(Ang) * R, FMath::Sin(Ang) * R), FVector(3.f, R * 0.55f, 3.f), RedPaint, Pivot, FRotator(0.f, 0.f, -(I * 30.f + 90.f)));
			}
			// Graduations de l'ouverture
			for (int32 I = 0; I < Device.Positions; ++I)
			{
				const float Ang = FMath::DegreesToRadians(-120.f + I * 240.f / FMath::Max(1, Device.Positions - 1));
				AddLabel(FString::FromInt(I), FVector(3.f, FMath::Sin(Ang) * (R + 9.f), Z + FMath::Cos(Ang) * (R + 9.f)), 6.f, LabelColor);
			}
			const FString Lbl = Device.Role == BRM::R_Valve ? FString::Printf(TEXT("V%d"), Device.Label + 1)
				: (Device.Role == BRM::R_Sluice ? BRMissionText::Letter(Device.Label) : FString(TEXT("P")));
			AddLabel(Lbl, FVector(3.f, 0.f, Z - R - 18.f), 10.f, LabelColor);
			BoxExtent = FVector(12.f, R + 6.f, R + 6.f);
		}
		else
		{
			// Cadran, molette, boitier : plaque, bouton tournant et graduations
			const bool bMill = Device.Role == BRM::R_MillDial;
			const float R = bMill ? 24.f : 12.f;
			BuildWallPlate(R * 2.f + 26.f, R * 2.f + 30.f, Device.Role == BRM::R_StreetBox ? Painted : Metal);
			Pivot = MakePivot(FVector(5.f, 0.f, Z + 20.f));
			AddPart(A->Cylinder(), FVector(2.f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), FVector(R * 1.2f, R * 1.2f, 4.f), DarkMetal, Pivot);
			MovingMesh = AddBoxPart(FVector(4.5f, 0.f, R * 0.4f), FVector(1.f, 2.f, R * 0.8f), RedPaint, Pivot);
			for (int32 I = 0; I < Device.Positions; ++I)
			{
				const float Ang = FMath::DegreesToRadians(-150.f + I * 300.f / FMath::Max(1, Device.Positions - 1));
				const FString Tick = bMill ? FString::Printf(TEXT("%d"), I * 45) : FString::FromInt(I);
				AddLabel(Tick, FVector(4.f, FMath::Sin(Ang) * (R + 7.f), Z + 20.f + FMath::Cos(Ang) * (R + 7.f)), bMill ? 5.f : 5.5f, LabelColor);
			}
			if (Device.Role == BRM::R_Dial)
			{
				AddSymbol(Device.Label, FVector(4.f, 0.f, Z + 20.f - R - 10.f), 9.f, RedPaint);
			}
			else if (Device.Role == BRM::R_CodeDial || Device.Role == BRM::R_DestDial)
			{
				AddLabel(FString::FromInt(Device.Label + 1), FVector(4.f, 0.f, Z + 20.f - R - 10.f), 7.f, LabelColor);
			}
			BoxCenter = FVector(6.f, 0.f, Z + 20.f);
			BoxExtent = FVector(8.f, R + 12.f, R + 14.f);
		}
		break;
	}
	case BRM::EKind::Socket:
		if (Device.Role == BRM::R_Lock)
		{
			// Moraillon et cadenas portant un symbole
			AddBoxPart(FVector(3.f, 0.f, Z), FVector(6.f, 30.f, 8.f), DarkMetal);
			Pivot = MakePivot(FVector(10.f, 0.f, Z));
			AddPart(A->Cylinder(), FVector(0.f, 0.f, 6.f), FRotator::ZeroRotator, FVector(10.f, 10.f, 10.f), Metal, Pivot);
			AddBoxPart(FVector(0.f, 0.f, -6.f), FVector(6.f, 14.f, 14.f), Brass);
			AddSymbol(Device.Label, FVector(13.5f, 0.f, Z - 6.f), 8.f, InkMat);
			BoxExtent = FVector(10.f, 16.f, 16.f);
		}
		else
		{
			// Porte-fusible : socle, contacts, fusible visible une fois pose
			AddBoxPart(FVector(4.f, 0.f, Z + 30.f), FVector(8.f, 12.f, 20.f), Painted);
			AddBoxPart(FVector(9.f, 0.f, Z + 38.f), FVector(2.f, 6.f, 2.f), Brass);
			AddBoxPart(FVector(9.f, 0.f, Z + 22.f), FVector(2.f, 6.f, 2.f), Brass);
			ItemMesh = AddPart(A->Cylinder(), FVector(10.f, 0.f, Z + 30.f), FRotator::ZeroRotator, FVector(4.f, 4.f, 16.f), Brass);
			AddLabel(BRMissionText::Letter(Device.Label), FVector(8.5f, 0.f, Z + 46.f), 7.f, LabelColor);
			BoxCenter = FVector(8.f, 0.f, Z + 30.f);
			BoxExtent = FVector(8.f, 10.f, 14.f);
		}
		break;
	case BRM::EKind::Item:
		if (Device.Role == BRM::R_Key)
		{
			// Crochet au mur, cle et etiquette de chambre
			AddBoxPart(FVector(2.f, 0.f, Z + 20.f), FVector(4.f, 10.f, 14.f), Wood);
			AddBoxPart(FVector(5.f, 0.f, Z + 24.f), FVector(6.f, 1.f, 1.f), Brass);
			ItemMesh = AddBoxPart(FVector(7.f, 0.f, Z + 14.f), FVector(1.f, 2.f, 9.f), Brass);
			AddLabel(FString::FromInt(100 + Device.Label), FVector(4.3f, 0.f, Z + 30.f), 4.5f, FColor(40, 30, 20));
			BoxCenter = FVector(5.f, 0.f, Z + 20.f);
			BoxExtent = FVector(6.f, 8.f, 12.f);
		}
		else
		{
			// Fusible pose sur une caisse ou une etagere
			AddBoxPart(FVector(18.f, 0.f, 30.f), FVector(36.f, 46.f, 60.f), Wood);
			ItemMesh = AddPart(A->Cylinder(), FVector(18.f, 0.f, 64.f), FRotator(90.f, 0.f, 0.f), FVector(4.f, 4.f, 16.f), Brass);
			AddLabel(BRMissionText::Letter(Device.Label), FVector(36.5f, 0.f, 44.f), 12.f, LabelColor);
			BoxCenter = FVector(18.f, 0.f, 64.f);
			BoxExtent = FVector(14.f, 14.f, 10.f);
		}
		break;
	case BRM::EKind::Crank:
		if (Device.Role == BRM::R_Beacon)
		{
			// Balise : poteau, lanterne, symbole en relief (lisible au toucher et a la lampe), manivelle
			AddBoxPart(FVector(14.f, 0.f, 75.f), FVector(10.f, 10.f, 150.f), DarkMetal);
			GlowMID = A->NewGlow(this, FLinearColor(1.f, 0.7f, 0.3f), 0.f);
			AddPart(A->Sphere(), FVector(14.f, 0.f, 160.f), FRotator::ZeroRotator, FVector(18.f, 18.f, 22.f), GlowMID);
			AddSymbol(Device.Label, FVector(19.5f, 0.f, 120.f), 14.f, Brass);
			Pivot = MakePivot(FVector(20.f, 0.f, 90.f));
			MovingMesh = AddBoxPart(FVector(2.f, 0.f, 6.f), FVector(3.f, 2.f, 14.f), Metal, Pivot);
			AddBoxPart(FVector(6.f, 0.f, 12.f), FVector(8.f, 3.f, 3.f), RedPaint, Pivot);
			BeaconLight = NewObject<UPointLightComponent>(this);
			BeaconLight->SetupAttachment(Root);
			BeaconLight->SetRelativeLocation(FVector(30.f, 0.f, 160.f));
			BeaconLight->SetIntensityUnits(ELightUnits::Lumens);
			BeaconLight->SetIntensity(0.f);
			BeaconLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.35f));
			BeaconLight->SetAttenuationRadius(900.f);
			BeaconLight->SetCastShadows(false);
			BeaconLight->RegisterComponent();
			bAnimated = true;
			BoxCenter = FVector(18.f, 0.f, 110.f);
			BoxExtent = FVector(14.f, 16.f, 60.f);
		}
		else
		{
			// Treuil ou generateur : tambour, cable, manivelle
			const bool bGen = Device.Role == BRM::R_Generator;
			AddBoxPart(FVector(18.f, 0.f, 30.f), FVector(36.f, bGen ? 70.f : 50.f, 60.f), bGen ? Painted : DarkMetal);
			AddPart(A->Cylinder(), FVector(18.f, 0.f, 70.f), FRotator(0.f, 0.f, 90.f), FVector(30.f, 30.f, bGen ? 60.f : 44.f), bGen ? DarkMetal : Wood);
			Pivot = MakePivot(FVector(38.f, 0.f, 70.f));
			MovingMesh = AddBoxPart(FVector(0.f, 0.f, 12.f), FVector(3.f, 3.f, 24.f), Metal, Pivot);
			AddBoxPart(FVector(5.f, 0.f, 22.f), FVector(10.f, 3.f, 3.f), RedPaint, Pivot);
			AddLabel(bGen ? FString(TEXT("G")) : BRMissionText::Letter(Device.Label), FVector(36.5f, 0.f, 40.f), 14.f, LabelColor);
			AddBoxPart(FVector(36.5f, 18.f, 50.f), FVector(1.f, 4.f, 4.f), LampMID);
			BoxCenter = FVector(26.f, 0.f, 60.f);
			BoxExtent = FVector(18.f, 30.f, 34.f);
		}
		break;
	case BRM::EKind::Button:
		if (Device.Role == BRM::R_HouseMarker)
		{
			// Plaque de facade : porche (symbole), fenetres (carres), lampe du porche
			BuildWallPlate(60.f, 44.f, Wood);
			AddSymbol(Device.Label, FVector(3.8f, -16.f, Z + 24.f), 14.f, InkMat);
			for (int32 I = 0; I < Device.Info[0]; ++I)
			{
				AddBoxPart(FVector(3.8f, 2.f + (I % 3) * 9.f, Z + 30.f - (I / 3) * 10.f), FVector(0.6f, 6.f, 6.f), Paper);
			}
			GlowMID = A->NewGlow(this, FLinearColor(1.f, 0.78f, 0.45f), 0.f);
			AddPart(A->Sphere(), FVector(8.f, 0.f, Z + 52.f), FRotator::ZeroRotator, FVector(9.f, 9.f, 9.f), GlowMID);
			BoxCenter = FVector(5.f, 0.f, Z + 24.f);
			BoxExtent = FVector(6.f, 32.f, 26.f);
		}
		else
		{
			BuildWallPlate(30.f, 34.f, Device.Role == BRM::R_MillBrake ? Wood : Painted);
			AddPart(A->Cylinder(), FVector(6.f, 0.f, Z + 20.f), FRotator(90.f, 0.f, 0.f), FVector(12.f, 12.f, 6.f), RedPaint);
			AddBoxPart(FVector(4.f, 0.f, Z + 33.f), FVector(1.f, 8.f, 3.f), LampMID);
			BoxCenter = FVector(6.f, 0.f, Z + 20.f);
			BoxExtent = FVector(8.f, 16.f, 18.f);
		}
		break;
	case BRM::EKind::Gate:
	{
		// Element visible de la mission : volet, grille, passerelle, porte, eclairage. Les portes et passages devant une
		// sortie de mission bloquent le passage tant qu'ils sont fermes.
		const bool bBig = Device.Role == BRM::R_BarnDoor;
		const float W = bBig ? 360.f : 170.f;
		const float H = bBig ? 320.f : 250.f;
		Pivot = MakePivot(FVector(0.f, 0.f, 0.f));
		switch (Device.Role)
		{
		case BRM::R_ReserveLights:
		case BRM::R_Passage:
		case BRM::R_LightsExit:
			GlowMID = A->NewGlow(this, Device.Role == BRM::R_Passage ? FLinearColor(0.6f, 0.5f, 1.f) : FLinearColor(1.f, 0.95f, 0.8f), 0.f);
			AddBoxPart(FVector(2.f, 0.f, 200.f), FVector(4.f, 90.f, 10.f), GlowMID);
			AddBoxPart(FVector(1.f, 0.f, 200.f), FVector(2.f, 100.f, 16.f), DarkMetal);
			break;
		case BRM::R_Bridge:
			// Passerelle levee (verticale) qui descend en pont
			MovingMesh = AddBoxPart(FVector(0.f, 0.f, H * 0.5f), FVector(10.f, W, H), Wood, Pivot);
			AddBoxPart(FVector(6.f, 0.f, H * 0.5f), FVector(2.f, W * 0.9f, 6.f), Metal, Pivot);
			Pivot->SetRelativeLocation(FVector(30.f, 0.f, 0.f));
			Blocker->SetRelativeLocation(FVector(30.f, 0.f, H * 0.5f));
			Blocker->SetBoxExtent(FVector(15.f, W * 0.5f, H * 0.5f));
			bBlocking = true;
			break;
		default:
			// Volet, grille ou porte qui se leve ou coulisse
			MovingMesh = AddBoxPart(FVector(0.f, 0.f, H * 0.5f), FVector(8.f, W, H), Device.Role == BRM::R_HotelAccess || Device.Role == BRM::R_StationGate ? Metal : Painted, Pivot);
			for (int32 I = 0; I < 6; ++I)
			{
				AddBoxPart(FVector(5.f, -W * 0.5f + (I + 0.5f) * W / 6.f, H * 0.5f), FVector(2.f, 4.f, H * 0.95f), DarkMetal, Pivot);
			}
			AddBoxPart(FVector(5.f, 0.f, H - 12.f), FVector(2.f, W, 10.f), Hazard, Pivot);
			Blocker->SetRelativeLocation(FVector(0.f, 0.f, H * 0.5f));
			Blocker->SetBoxExtent(FVector(15.f, W * 0.5f, H * 0.5f));
			bBlocking = true;
			if (Device.Role == BRM::R_SteamDoor)
			{
				GlowMID = A->NewGlow(this, FLinearColor(0.9f, 0.92f, 0.95f), 0.4f);
				for (int32 I = 0; I < 4; ++I)
				{
					AddPart(A->Sphere(), FVector(30.f + I * 8.f, -40.f + I * 26.f, 40.f + I * 30.f), FRotator::ZeroRotator, FVector(50.f, 50.f, 50.f), GlowMID, Pivot);
				}
				bAnimated = true;
			}
			break;
		}
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoxExtent = FVector(1.f, 1.f, 1.f);
		break;
	}
	}

	Box->SetRelativeLocation(BoxCenter);
	Box->SetBoxExtent(BoxExtent);
}

bool ABRMissionDevice::IsInteractable() const
{
	return bShown && Device.Kind != BRM::EKind::Gate;
}

bool ABRMissionDevice::IsRunning() const
{
	switch (Device.Kind)
	{
	case BRM::EKind::Switch:
		return (Device.Role == BRM::R_Relay || Device.Role == BRM::R_Breaker) && StateValue != 0;
	case BRM::EKind::Crank:
		return StateValue >= Device.Positions;
	default:
		return false;
	}
}

bool ABRMissionDevice::IsHoldAction() const
{
	return Device.Kind == BRM::EKind::Observe || Device.Kind == BRM::EKind::Crank;
}

FVector ABRMissionDevice::GetInteractPoint() const
{
	return Box ? Box->GetComponentLocation() : GetActorLocation();
}

void ABRMissionDevice::SetShown(bool bShow)
{
	if (bShown == bShow)
	{
		return;
	}
	bShown = bShow;
	SetActorHiddenInGame(!bShow);
	Box->SetCollisionEnabled(bShow && Device.Kind != BRM::EKind::Gate ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (!bShow)
	{
		Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else if (Device.Kind == BRM::EKind::Gate && bBlocking)
	{
		Blocker->SetCollisionEnabled(GateValue < 200 ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

void ABRMissionDevice::ApplyState(const BRM::FPlan& Plan, const BRM::FState& State, const BRM::FEval& Eval, bool bAnimate)
{
	if (Index < 0 || Index >= Plan.NumDevices)
	{
		return;
	}
	StateValue = State.Dev[Index];
	GateValue = Eval.Gate[Index];
	bSolved = Eval.bSolved;
	bWarning = Eval.WarningDevice == Index;
	bLocked = BRM::CanUse(Plan, State, BRM::FCampaign(), Index).Feedback == BRM::EFeedback::Locked;

	// Position cible de la piece mobile (0..1)
	switch (Device.Kind)
	{
	case BRM::EKind::Switch:
		TargetPos = Device.Positions > 1 ? static_cast<float>(StateValue) / (Device.Positions - 1) : 0.f;
		break;
	case BRM::EKind::Observe:
		TargetPos = Device.Role == BRM::R_Gauge ? static_cast<float>(BRM::GaugeReading(Plan, State, Index)) / 4.f : 0.f;
		break;
	case BRM::EKind::Crank:
		TargetPos = static_cast<float>(StateValue) / FMath::Max<int32>(1, Device.Positions);
		break;
	case BRM::EKind::Socket:
		TargetPos = StateValue ? 1.f : 0.f;
		break;
	case BRM::EKind::Gate:
		TargetPos = GateValue / 255.f;
		break;
	default:
		TargetPos = 0.f;
		break;
	}
	if (!bAnimate)
	{
		ShownPos = TargetPos;
	}
	else if (!FMath::IsNearlyEqual(ShownPos, TargetPos))
	{
		SetActorTickEnabled(true);
	}

	// Objet present / pose
	if (ItemMesh)
	{
		const bool bVisible = Device.Kind == BRM::EKind::Item ? StateValue == 0 : StateValue != 0;
		ItemMesh->SetVisibility(bVisible);
	}

	// Lampe d'etat
	if (LampMID)
	{
		FLinearColor Lamp = FLinearColor::Black;
		switch (Device.Kind)
		{
		case BRM::EKind::Switch:
			Lamp = (Device.Role == BRM::R_Breaker || Device.Role == BRM::R_Relay) && StateValue ? FLinearColor(0.2f, 1.f, 0.3f) * 4.f
				: (bWarning ? FLinearColor(1.f, 0.4f, 0.05f) * 5.f : FLinearColor(0.4f, 0.05f, 0.02f));
			break;
		case BRM::EKind::Crank:
			Lamp = StateValue >= Device.Positions ? FLinearColor(0.2f, 1.f, 0.3f) * 4.f : FLinearColor(0.4f, 0.25f, 0.02f) * (StateValue ? 3.f : 0.6f);
			break;
		case BRM::EKind::Button:
			Lamp = bSolved ? FLinearColor(0.2f, 1.f, 0.3f) * 4.f : (bLocked ? FLinearColor(0.3f, 0.02f, 0.02f) : FLinearColor(1.f, 0.15f, 0.05f) * 2.5f);
			break;
		default:
			break;
		}
		LampMID->SetVectorParameterValue(TEXT("Emissive"), Lamp);
	}
	if (GlowMID)
	{
		if (Device.Role == BRM::R_HouseMarker)
		{
			GlowMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(1.f, 0.78f, 0.45f) * (GateValue ? 8.f : 0.f));
		}
		else if (Device.Role == BRM::R_ReserveLights || Device.Role == BRM::R_LightsExit || Device.Role == BRM::R_Passage)
		{
			GlowMID->SetVectorParameterValue(TEXT("Emissive"), (Device.Role == BRM::R_Passage ? FLinearColor(0.6f, 0.5f, 1.f) : FLinearColor(1.f, 0.95f, 0.8f)) * (GateValue ? 10.f : 0.f));
		}
		else if (Device.Role == BRM::R_Beacon)
		{
			GlowMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(1.f, 0.7f, 0.3f) * (StateValue >= Device.Positions ? 12.f : 0.f));
		}
	}
	if (BeaconLight)
	{
		BeaconLight->SetIntensity(StateValue >= Device.Positions ? 2200.f : 0.f);
	}
	if (Device.Kind == BRM::EKind::Gate && bBlocking && bShown)
	{
		Blocker->SetCollisionEnabled(GateValue < 200 ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Blocker->SetCollisionResponseToAllChannels(ECR_Block);
	}
	if (!bAnimate)
	{
		Tick(0.f);
	}
}

void ABRMissionDevice::PlayFeedback(uint8 Feedback)
{
	FlashTime = Time;
	SetActorTickEnabled(true);
	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	const TCHAR* Name = nullptr;
	switch (static_cast<BRM::EFeedback>(Feedback))
	{
	case BRM::EFeedback::Warning:
		Name = Level == 5 ? TEXT("S_M_Steam") : (Level == 37 ? TEXT("S_Splash") : TEXT("S_M_Steam"));
		break;
	case BRM::EFeedback::Tripped:
		Name = TEXT("S_M_Trip");
		break;
	case BRM::EFeedback::Wrong:
	case BRM::EFeedback::Overload:
	case BRM::EFeedback::NoFuse:
	case BRM::EFeedback::Order:
	case BRM::EFeedback::Dead:
	case BRM::EFeedback::WrongItem:
	case BRM::EFeedback::Full:
		Name = TEXT("S_UIDeny");
		break;
	case BRM::EFeedback::Done:
	case BRM::EFeedback::Progress:
	case BRM::EFeedback::Returned:
		switch (Device.Kind)
		{
		case BRM::EKind::Switch:
			Name = (Device.Role == BRM::R_Valve || Device.Role == BRM::R_Sluice || Device.Role == BRM::R_BoilerDial) ? TEXT("S_M_Valve")
				: (Device.Role == BRM::R_Relay ? TEXT("S_M_Relay") : (Device.Role == BRM::R_MillDial ? TEXT("S_M_Mill")
				: ((Device.Role == BRM::R_Breaker || Device.Role == BRM::R_RouteLever) ? TEXT("S_M_Switch") : TEXT("S_M_Dial"))));
			break;
		case BRM::EKind::Socket:
			Name = Device.Role == BRM::R_Lock ? TEXT("S_M_Lock") : TEXT("S_M_Switch");
			break;
		case BRM::EKind::Item:
			Name = TEXT("S_Pickup");
			break;
		case BRM::EKind::Crank:
			Name = Device.Role == BRM::R_Beacon ? TEXT("S_M_Beacon") : TEXT("S_M_Crank");
			break;
		case BRM::EKind::Button:
			Name = TEXT("S_M_Switch");
			break;
		case BRM::EKind::Clue:
			Name = TEXT("S_ItemMove");
			break;
		case BRM::EKind::Observe:
			Name = static_cast<BRM::EFeedback>(Feedback) == BRM::EFeedback::Done ? TEXT("S_RecBeep") : nullptr;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	if (Name)
	{
		// v4.11 : equivalent ecrit du son (sous-titres), si le mecanisme est proche du joueur local
		const APawn* Local = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Local && FVector::DistSquared(Local->GetActorLocation(), GetActorLocation()) < FMath::Square(2000.f))
		{
			const FString N(Name);
			FString Text;
			if (N == TEXT("S_M_Switch")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Switch", "[D\u00e9clic d'un levier]"));
			else if (N == TEXT("S_M_Dial")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Dial", "[Crans d'un cadran]"));
			else if (N == TEXT("S_M_Valve")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Valve", "[Grincement d'une vanne]"));
			else if (N == TEXT("S_M_Crank")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Crank", "[Cliquetis d'une manivelle]"));
			else if (N == TEXT("S_M_Steam")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Steam", "[Sifflement de vapeur]"));
			else if (N == TEXT("S_M_Relay")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Relay", "[Claquement d'un relais]"));
			else if (N == TEXT("S_M_Trip")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Trip", "[Disjonction, arc \u00e9lectrique]"));
			else if (N == TEXT("S_M_Lock")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Lock", "[Une serrure c\u00e8de]"));
			else if (N == TEXT("S_M_Beacon")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Beacon", "[Timbre d'une balise]"));
			else if (N == TEXT("S_M_Mill")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Mill", "[Grincement du moulin]"));
			else if (N == TEXT("S_Splash")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Overflow", "[L'eau d\u00e9borde]"));
			else if (N == TEXT("S_UIDeny")) Text = BR_STR(NSLOCTEXT("BR", "Caption.Deny", "[Le m\u00e9canisme refuse]"));
			ABRHUD::Caption(this, Text, GetActorLocation());
		}
		if (USoundBase* S = A->Sound(Name))
		{
			// Signal fiable : pas etouffe par les murs (le texte et la lampe disent la meme chose)
			UGameplayStatics::PlaySoundAtLocation(this, S, GetInteractPoint(), 0.85f, 1.f, 0.f, A->Attenuation(1800.f, false));
		}
	}
}

void ABRMissionDevice::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	bool bKeep = bAnimated;
	if (!FMath::IsNearlyEqual(ShownPos, TargetPos, 0.002f))
	{
		ShownPos = FMath::FInterpTo(ShownPos, TargetPos, DeltaSeconds, 6.f);
		if (DeltaSeconds <= 0.f)
		{
			ShownPos = TargetPos;
		}
		bKeep = true;
	}
	if (Moving)
	{
		switch (Device.Kind)
		{
		case BRM::EKind::Switch:
			if (Device.Role == BRM::R_Breaker || Device.Role == BRM::R_Relay || Device.Role == BRM::R_RouteLever)
			{
				Moving->SetRelativeRotation(FRotator(-50.f + ShownPos * 100.f, 0.f, 0.f));
			}
			else if (Device.Role == BRM::R_Valve || Device.Role == BRM::R_Sluice || Device.Role == BRM::R_BoilerDial)
			{
				Moving->SetRelativeRotation(FRotator(0.f, 0.f, ShownPos * 240.f));
			}
			else
			{
				Moving->SetRelativeRotation(FRotator(0.f, 0.f, -150.f + ShownPos * 300.f));
			}
			break;
		case BRM::EKind::Observe:
			if (Device.Role == BRM::R_Gauge)
			{
				Moving->SetRelativeRotation(FRotator(0.f, 0.f, -135.f + ShownPos * 270.f + (bWarning ? FMath::Sin(Time * 40.f) * 4.f : 0.f)));
			}
			else
			{
				// Fleche du courant qui oscille sur l'eau
				Moving->SetRelativeLocation(FVector(40.f, FMath::Sin(Time * 1.3f) * 6.f, FMath::Sin(Time * 2.1f) * 2.f));
			}
			break;
		case BRM::EKind::Crank:
			Moving->SetRelativeRotation(FRotator(0.f, 0.f, ShownPos * 1080.f));
			break;
		case BRM::EKind::Socket:
			Moving->SetRelativeLocation(FVector(10.f, 0.f, WorkZ + ShownPos * 6.f));
			break;
		case BRM::EKind::Gate:
			if (Device.Role == BRM::R_Bridge)
			{
				Moving->SetRelativeRotation(FRotator(-ShownPos * 90.f, 0.f, 0.f));
			}
			else if (Device.Role == BRM::R_BarnDoor || Device.Role == BRM::R_HouseDoor)
			{
				Moving->SetRelativeLocation(FVector(0.f, ShownPos * (Device.Role == BRM::R_BarnDoor ? 340.f : 150.f), 0.f));
			}
			else
			{
				Moving->SetRelativeLocation(FVector(0.f, 0.f, ShownPos * 235.f));
			}
			break;
		default:
			break;
		}
	}
	if (GlowMID)
	{
		if (Device.Role == BRM::R_Anomaly)
		{
			// Serie de N eclats (0,35 s chacun), puis 2 s de pause : on peut compter sans rien d'autre que les yeux
			const float Period = Blinks * 0.7f + 2.f;
			const float T = FMath::Fmod(Time, Period);
			const bool bOff = T < Blinks * 0.7f && FMath::Fmod(T, 0.7f) < 0.35f;
			GlowMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(1.f, 0.96f, 0.82f) * (bOff ? 0.15f : 6.f));
		}
		else if (Device.Role == BRM::R_JunctionBox)
		{
			GlowMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.6f, 0.75f, 1.f) * (FMath::FRand() < 0.12f ? 30.f : 0.f));
		}
		else if (Device.Role == BRM::R_SteamDoor)
		{
			const float Density = 1.f - ShownPos;
			GlowMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.9f, 0.92f, 0.95f) * Density * (0.3f + 0.1f * FMath::Sin(Time * 3.f)));
			if (Moving)
			{
				for (USceneComponent* Child : Moving->GetAttachChildren())
				{
					if (UStaticMeshComponent* Puff = Cast<UStaticMeshComponent>(Child))
					{
						if (Puff->GetMaterial(0) == GlowMID)
						{
							Puff->SetVisibility(Density > 0.05f);
						}
					}
				}
			}
		}
	}
	// Eclat bref a chaque retour de l'hote
	if (LampMID && Time - FlashTime < 0.4f)
	{
		bKeep = true;
	}
	if (!bKeep)
	{
		SetActorTickEnabled(false);
	}
}

FString ABRMissionDevice::GetPrompt() const
{
	const ABRWorld* W = ABRWorld::Get(this);
	if (!W || !W->IsMissionActive() || Index < 0)
	{
		return FString();
	}
	const BRM::FPlan& Plan = W->GetMissionPlan();
	const BRM::FState& State = W->GetMissionState();
	const FString Name = BRMissionText::DeviceName(Plan, Index);
	const FString St = BRMissionText::DeviceState(Plan, State, Index);
	const FString K = BRKeys::Tag(EBRAction::Interact);
	const BRM::FResult Lock = BRM::CanUse(Plan, State, W->GetMissionCampaign(), Index);
	if (Lock.Feedback == BRM::EFeedback::Locked)
	{
		return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Prompt.Locked", "{Name}  -  {Why}"), { { TEXT("Name"), BRLoc::Arg(Name) },
			{ TEXT("Why"), BRLoc::Arg(BRMissionText::Feedback(Plan, Index, static_cast<uint8>(BRM::EFeedback::Locked), Lock.Related, 0)) } });
	}
	FString Verb;
	switch (Device.Kind)
	{
	case BRM::EKind::Clue:
		Verb = BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Read", "{Key} Lire"), { { TEXT("Key"), BRLoc::Arg(K) } });
		break;
	case BRM::EKind::Observe:
		Verb = Device.Role == BRM::R_Anomaly ? BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Record", "Maintenir {Key} : documenter (cam\u00e9scope du sac)"), { { TEXT("Key"), BRLoc::Arg(K) } })
			: BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Examine", "Maintenir {Key} : examiner"), { { TEXT("Key"), BRLoc::Arg(K) } });
		break;
	case BRM::EKind::Switch:
		Verb = (Device.Role == BRM::R_Breaker || Device.Role == BRM::R_Relay || Device.Role == BRM::R_RouteLever)
			? BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Toggle", "{Key} Basculer"), { { TEXT("Key"), BRLoc::Arg(K) } })
			: BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Turn", "{Key} Tourner d'un cran"), { { TEXT("Key"), BRLoc::Arg(K) } });
		break;
	case BRM::EKind::Socket:
		Verb = StateValue ? FString() : BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Insert", "{Key} Ins\u00e9rer"), { { TEXT("Key"), BRLoc::Arg(K) } });
		break;
	case BRM::EKind::Item:
		if (!StateValue)
		{
			Verb = BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Take", "{Key} Prendre (objet d'\u00e9quipe)"), { { TEXT("Key"), BRLoc::Arg(K) } });
		}
		else if (Device.Role == BRM::R_Key && BRM::Held(Plan, State, Device.Need) > 0)
		{
			Verb = BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Return", "{Key} Remettre au crochet"), { { TEXT("Key"), BRLoc::Arg(K) } });
		}
		break;
	case BRM::EKind::Crank:
		Verb = BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Crank", "Maintenir {Key} : actionner"), { { TEXT("Key"), BRLoc::Arg(K) } });
		break;
	case BRM::EKind::Button:
		Verb = (State.Solved & 1) ? FString() : BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Verb.Press", "{Key} Appuyer"), { { TEXT("Key"), BRLoc::Arg(K) } });
		break;
	default:
		break;
	}
	FString Line = St.IsEmpty() ? Name : BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Prompt.NameState", "{Name} ({State})"), { { TEXT("Name"), BRLoc::Arg(Name) }, { TEXT("State"), BRLoc::Arg(St) } });
	return Verb.IsEmpty() ? Line : BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Prompt.Full", "{Verb}  -  {Line}"), { { TEXT("Verb"), BRLoc::Arg(Verb) }, { TEXT("Line"), BRLoc::Arg(Line) } });
}
