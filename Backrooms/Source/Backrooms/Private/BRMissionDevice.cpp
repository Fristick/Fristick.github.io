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
#include "BRLightLogic.h"
#include "HAL/PlatformTime.h"

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
	Module = Spot.Module;
	SetActorLocationAndRotation(Spot.Pos, FRotator(0.f, Spot.Yaw, 0.f));
	for (int32 I = 0; I < Plan.NumDevices; ++I)
	{
		if (Plan.Devices[I].Role == BRM::R_Sluice && Plan.Devices[I].Label == 1)
		{
			SluiceBIndex = I;
		}
	}

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
			// v4.12 : la lampe du porche eclaire vraiment une fois allumee
			GateLight = NewObject<UPointLightComponent>(this);
			GateLight->SetupAttachment(Root);
			GateLight->SetRelativeLocation(FVector(40.f, 0.f, Z + 52.f));
			GateLight->SetIntensityUnits(ELightUnits::Lumens);
			GateLight->SetIntensity(0.f);
			GateLight->SetLightColor(FLinearColor(1.f, 0.78f, 0.45f));
			GateLight->SetAttenuationRadius(BRLight::LitGateRadius);
			GateLight->SetCastShadows(false);
			GateLight->RegisterComponent();
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
		// v4.12 : sas du passage sec (Poolrooms) et passerelle (Niveau 8) : un vrai module, pas un volet
		if (Module == BRMech::Module::PoolLock || Module == BRMech::Module::Bridge)
		{
			if (Module == BRMech::Module::PoolLock)
			{
				BuildPoolLock();
			}
			else
			{
				BuildBridge();
			}
			bAnimated = true;
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BoxExtent = FVector(1.f, 1.f, 1.f);
			break;
		}
		switch (Device.Role)
		{
		case BRM::R_ReserveLights:
		case BRM::R_Passage:
		case BRM::R_LightsExit:
			GlowMID = A->NewGlow(this, Device.Role == BRM::R_Passage ? FLinearColor(0.6f, 0.5f, 1.f) : FLinearColor(1.f, 0.95f, 0.8f), 0.f);
			AddBoxPart(FVector(2.f, 0.f, 200.f), FVector(4.f, 90.f, 10.f), GlowMID);
			AddBoxPart(FVector(1.f, 0.f, 200.f), FVector(2.f, 100.f, 16.f), DarkMetal);
			if (Device.Role == BRM::R_LightsExit)
			{
				// v4.12 : la sortie "eclairee" eclaire vraiment (et compte pour les regles : BRLight)
				GateLight = NewObject<UPointLightComponent>(this);
				GateLight->SetupAttachment(Root);
				GateLight->SetRelativeLocation(FVector(30.f, 0.f, 190.f));
				GateLight->SetIntensityUnits(ELightUnits::Lumens);
				GateLight->SetIntensity(0.f);
				GateLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
				GateLight->SetAttenuationRadius(BRLight::LitGateRadius);
				GateLight->SetCastShadows(false);
				GateLight->RegisterComponent();
			}
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
				// v4.12 : vapeur en voiles translucides (eau tres diffusante, presque sans absorption) qui ondulent devant la
				// porte, au lieu de spheres opaques ; elles s'eteignent quand les vannes sont bien reglees. Une fuite sort
				// d'une conduite au-dessus de la porte
				const FBRSurface SteamS(TEXT("T_WaterNormal"), FLinearColor(0.86f, 0.88f, 0.9f), 180.f, 0.3f, 0.f);
				UMaterialInterface* Steam = A->WaterMaterial(SteamS, 0.04f, 1.f, 0.7f, 0.9f);
				AddPart(A->Cylinder(), FVector(18.f, 0.f, H + 14.f), FRotator(0.f, 0.f, 90.f), FVector(14.f, 14.f, W + 20.f), DarkMetal);
				for (int32 I = 0; I < 6; ++I)
				{
					const float Y = -W * 0.4f + I * W * 0.16f;
					UStaticMeshComponent* Veil = AddPart(A->Plane(), FVector(26.f + (I % 2) * 14.f, Y, H * 0.55f), FRotator(0.f, I % 2 ? 70.f : -70.f, 90.f),
						FVector(70.f, 110.f + (I % 3) * 25.f, 100.f), Steam);
					if (Veil)
					{
						Veil->SetTranslucentSortPriority(1);
						Flows.Add(Veil);
					}
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
	if (Module == BRMech::Module::PoolTanks)
	{
		// v4.12 : la vanne A porte les deux bassins (derriere elle), leurs regles graduees et leurs conduites
		BuildPoolTanks();
		bAnimated = true;
	}
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
	if (!bShow)
	{
		// v4.12 : zone dechargee : plus aucune animation (neon, balise, vapeur) ; l'etat logique de la mission reste
		SetActorTickEnabled(false);
	}
	else
	{
		// Reapparition : la bonne position tout de suite, sans rejouer le mouvement (le monde rallume le Tick des
		// mecanismes animes proches)
		ShownPos = TargetPos;
		Tick(0.f);
		SetActorTickEnabled(false);
	}
	Box->SetCollisionEnabled(bShow && Device.Kind != BRM::EKind::Gate ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	SetModuleCollision(bShow); // v4.12 : sol, parois et tablier du module n'existent que si la zone est construite
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
	if (!bAnimate || !bShown)
	{
		ShownPos = TargetPos; // v4.12 : masque, pas d'animation : il apparaitra deja en place
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
	if (GateLight)
	{
		GateLight->SetIntensity(GateValue ? 1600.f : 0.f);
	}
	UpdateGameplayLights();
	if (Module != BRMech::Module::None)
	{
		ApplyModuleState(Plan, State, Eval, bAnimate);
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
	// v4.12 : un retour recu pendant que la zone est dechargee ne relance pas le Tick (le son, lui, reste)
	if (bShown)
	{
		SetActorTickEnabled(true);
	}
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

int64 ABRMissionDevice::TickCount = 0;
double ABRMissionDevice::TickSeconds = 0.0;

void ABRMissionDevice::UpdateGameplayLights()
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W)
	{
		return;
	}
	if (BeaconLight)
	{
		W->SetGameplayLight(this, 0, BeaconLight->GetComponentLocation(), BRLight::BeaconRadius, BRLight::BeaconIntensity, StateValue >= Device.Positions);
	}
	if (GateLight)
	{
		W->SetGameplayLight(this, 1, GateLight->GetComponentLocation(), BRLight::LitGateRadius, BRLight::LitGateIntensity, GateValue != 0);
	}
}

void ABRMissionDevice::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Mesure : seuls les vrais Ticks du moteur comptent (Tick(0) applique une position sans animation)
	const double TickStart = FPlatformTime::Seconds();
	TickCount += DeltaSeconds > 0.f ? 1 : 0;
	Time += DeltaSeconds;
	// v4.12 : un mecanisme masque ne garde jamais son Tick (avant : bKeep = bAnimated, meme zone dechargee)
	bool bKeep = bAnimated && bShown;
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
			if (Module != BRMech::Module::None)
			{
				break; // v4.12 : anime par UpdateModule (meme etat que l'eau, le sol et les collisions)
			}
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
	}
	if (Device.Role == BRM::R_SteamDoor && Flows.Num() > 0)
	{
		// v4.12 : voiles de vapeur : ils ondulent et montent, plus minces a mesure que les vannes approchent du bon reglage
		const float Density = 1.f - ShownPos;
		for (int32 I = 0; I < Flows.Num(); ++I)
		{
			UStaticMeshComponent* Veil = Flows[I];
			if (!Veil)
			{
				continue;
			}
			Veil->SetVisibility(Density > 0.05f);
			const float Phase = Time * (0.9f + I * 0.17f) + I * 1.3f;
			Veil->SetRelativeScale3D(FVector(0.7f * (0.4f + 0.6f * Density), (1.1f + (I % 3) * 0.25f) * (0.5f + 0.5f * Density) * (1.f + 0.12f * FMath::Sin(Phase)), 1.f));
			Veil->SetRelativeRotation(FRotator(0.f, (I % 2 ? 70.f : -70.f) + 8.f * FMath::Sin(Phase * 0.7f), 90.f));
		}
	}
	if (Module != BRMech::Module::None && UpdateModule(DeltaSeconds))
	{
		bKeep = true;
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
	if (DeltaSeconds > 0.f)
	{
		TickSeconds += FPlatformTime::Seconds() - TickStart;
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

// =====================================================================================================================
// v4.12 : modules physiques (BRMech) : bassins et sas des Poolrooms, passerelle du Niveau 8.
// L'etat vient de la mission (repliquee et sauvegardee) ; l'eau est enregistree dans le monde (ABRWorld::WaterAt), le
// sol et les parois sont de vraies collisions, coupees quand la zone est masquee.
// =====================================================================================================================

UStaticMeshComponent* ABRMissionDevice::AddSolid(const FVector& Min, const FVector& Max, UMaterialInterface* Mat, bool bBlockSight, USceneComponent* Parent)
{
	UStaticMeshComponent* C = AddBoxPart((Min + Max) * 0.5f, Max - Min, Mat, Parent);
	if (C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetCollisionResponseToAllChannels(ECR_Block);
		// Murets, garde-corps et tablier ne cachent rien : la vue (rassemblement, perception) passe au travers
		C->SetCollisionResponseToChannel(ECC_Visibility, bBlockSight ? ECR_Block : ECR_Ignore);
		C->SetCastShadow(true);
		ModuleSolids.Add(C);
	}
	return C;
}

void ABRMissionDevice::PlaceRod(UStaticMeshComponent* Rod, const FVector& From, const FVector& To, float Thick)
{
	if (!Rod)
	{
		return;
	}
	const FVector D = To - From;
	const float Len = static_cast<float>(D.Size());
	Rod->SetRelativeLocation((From + To) * 0.5f);
	Rod->SetRelativeRotation(Len > 0.1f ? FRotationMatrix::MakeFromX(D / Len).Rotator() : FRotator::ZeroRotator);
	Rod->SetRelativeScale3D(FVector(FMath::Max(Len, 0.1f), Thick, Thick) / 100.f);
}

UStaticMeshComponent* ABRMissionDevice::AddRod(const FVector& From, const FVector& To, float Thick, UMaterialInterface* Mat, USceneComponent* Parent)
{
	UStaticMeshComponent* Rod = AddBoxPart(FVector::ZeroVector, FVector(1.f, Thick, Thick), Mat, Parent);
	PlaceRod(Rod, From, To, Thick);
	return Rod;
}

BRMech::FFrame ABRMissionDevice::ModuleFrame() const
{
	BRMech::FFrame F;
	const FVector L = GetActorLocation();
	F.X = static_cast<float>(L.X);
	F.Y = static_cast<float>(L.Y);
	F.Z = static_cast<float>(L.Z);
	F.Yaw = static_cast<float>(GetActorRotation().Yaw);
	return F;
}

void ABRMissionDevice::SetModuleCollision(bool bOn)
{
	for (UStaticMeshComponent* C : ModuleSolids)
	{
		if (C)
		{
			C->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
	}
}

void ABRMissionDevice::BuildPoolTanks()
{
	UBRAssets* A = UBRAssets::Get(this);
	ABRWorld* W = ABRWorld::Get(this);
	if (!A || !W)
	{
		return;
	}
	namespace P = BRMech::Pool;
	const FBRLevelDef& D = W->Def();
	const float LW = D.WaterHeight;
	const float Z0 = static_cast<float>(GetActorLocation().Z);
	auto Zl = [Z0](float Abs) { return Abs - Z0; };
	const float Ba = P::BottomA(LW), Bb = P::BottomB(LW), Ra = P::RimA(LW), Rb = P::RimB(LW);
	const float X0 = -P::TankDepth, X1 = 0.f, Wl = P::Wall;
	UMaterialInterface* Tile = A->Surface(D.Wall);
	UMaterialInterface* Coping = A->Surface(FBRSurface(TEXT("T_Concrete"), FLinearColor(1.25f, 1.25f, 1.22f), 120.f, 0.45f, 0.1f));
	UMaterialInterface* DarkMetal = A->Surface(MetalSurf(FLinearColor(0.14f, 0.15f, 0.16f), 0.55f));
	UMaterialInterface* Steel = A->Surface(MetalSurf(FLinearColor(0.55f, 0.57f, 0.6f)));
	UMaterialInterface* Enamel = A->Surface(FBRSurface(TEXT("T_Paper"), FLinearColor(0.95f, 0.95f, 0.9f), 40.f, 0.35f, 0.05f));
	UMaterialInterface* InkMat = A->Surface(FBRSurface(TEXT("T_Grime"), FLinearColor(0.04f, 0.04f, 0.05f), 50.f, 0.7f, 0.1f));
	UMaterialInterface* FloatMat = A->Surface(FBRSurface(TEXT("T_Grime"), FLinearColor(0.95f, 0.35f, 0.05f), 50.f, 0.5f, 0.1f));
	const FBRSurface WaterS = W->GetWaterSurface();
	UMaterialInterface* Water = A->WaterMaterial(WaterS, D.WaterAbsorption, D.WaterScattering, 0.15f, 0.3f);

	// Corps : fonds pleins (A plus haut que B), cloison commune, parois
	AddSolid(FVector(X0, P::AY0 - Wl, Zl(-2.f)), FVector(X1, P::AY1, Zl(Ba)), Tile, true);
	AddSolid(FVector(X0, P::AY1, Zl(-2.f)), FVector(X1, P::BY0, Zl(Ra)), Tile, true);
	AddSolid(FVector(X0, P::BY0, Zl(-2.f)), FVector(X1, P::BY1 + Wl, Zl(Bb)), Tile, true);
	AddSolid(FVector(X0, P::AY0 - Wl, Zl(Ba)), FVector(X1, P::AY0, Zl(Ra)), Tile, true);
	AddSolid(FVector(X1 - Wl, P::AY0, Zl(Ba)), FVector(X1, P::AY1, Zl(Ra)), Tile, true);
	AddSolid(FVector(X0, P::AY0, Zl(Ba)), FVector(X0 + Wl, P::AY1, Zl(Ra)), Tile, true);
	AddSolid(FVector(X0, P::BY1, Zl(Bb)), FVector(X1, P::BY1 + Wl, Zl(Rb)), Tile, true);
	AddSolid(FVector(X1 - Wl, P::BY0, Zl(Bb)), FVector(X1, P::BY1, Zl(Rb)), Tile, true);
	AddSolid(FVector(X0, P::BY0, Zl(Bb)), FVector(X0 + Wl, P::BY1, Zl(Rb)), Tile, true);
	// Margelles (le haut des parois, plus clair et un peu plus large)
	AddBoxPart(FVector((X0 + X1) * 0.5f, (P::AY0 - Wl + P::BY0) * 0.5f, Zl(Ra) + 2.5f), FVector(P::TankDepth + 4.f, P::BY0 - P::AY0 + Wl + 4.f, 5.f), Coping);
	AddBoxPart(FVector((X0 + X1) * 0.5f, (P::BY0 + P::BY1 + Wl) * 0.5f, Zl(Rb) + 2.5f), FVector(P::TankDepth + 4.f, P::BY1 + Wl - P::BY0 + 4.f, 5.f), Coping);

	// Doublures interieures : leur ligne d'eau suit la surface (bande mouillee qui monte et descend)
	struct FTank
	{
		float Y0, Y1, Bottom, Rim;
	};
	const FTank Tanks[2] = { { P::AY0, P::AY1, Ba, Ra }, { P::BY0, P::BY1, Bb, Rb } };
	for (int32 T = 0; T < 2; ++T)
	{
		const FTank& K = Tanks[T];
		FBRSurface Lining = D.Wall;
		Lining.WaterLine = K.Bottom;
		UMaterialInstanceDynamic* MID = A->NewSurface(Lining, this);
		WaterLineMIDs.Add(MID);
		const float XI0 = X0 + Wl, XI1 = X1 - Wl;
		AddBoxPart(FVector(XI0 + 0.5f, (K.Y0 + K.Y1) * 0.5f, Zl((K.Bottom + K.Rim) * 0.5f)), FVector(1.f, K.Y1 - K.Y0, K.Rim - K.Bottom), MID);
		AddBoxPart(FVector(XI1 - 0.5f, (K.Y0 + K.Y1) * 0.5f, Zl((K.Bottom + K.Rim) * 0.5f)), FVector(1.f, K.Y1 - K.Y0, K.Rim - K.Bottom), MID);
		AddBoxPart(FVector((XI0 + XI1) * 0.5f, K.Y0 + 0.5f, Zl((K.Bottom + K.Rim) * 0.5f)), FVector(XI1 - XI0, 1.f, K.Rim - K.Bottom), MID);
		AddBoxPart(FVector((XI0 + XI1) * 0.5f, K.Y1 - 0.5f, Zl((K.Bottom + K.Rim) * 0.5f)), FVector(XI1 - XI0, 1.f, K.Rim - K.Bottom), MID);
		AddBoxPart(FVector((XI0 + XI1) * 0.5f, (K.Y0 + K.Y1) * 0.5f, Zl(K.Bottom) + 0.5f), FVector(XI1 - XI0, K.Y1 - K.Y0, 1.f), MID);
		// Surface d'eau (placee par UpdateModule)
		UStaticMeshComponent* Plane = AddPart(A->Plane(), FVector((XI0 + XI1) * 0.5f, (K.Y0 + K.Y1) * 0.5f, Zl(K.Bottom + 20.f)), FRotator::ZeroRotator,
			FVector(XI1 - XI0, K.Y1 - K.Y0, 100.f), Water);
		WaterPlanes.Add(Plane);
		// Regle graduee sur l'avant : plaque emaillee, marques et chiffres, colonne d'eau et flotteur (lisibles du sol)
		const int32 Marks = T == 0 ? P::MarksA : P::MarksB;
		const float GY = T == 0 ? -78.f : 118.f;
		const float S0 = T == 0 ? P::SurfaceA(0, LW) : P::SurfaceB(0, LW);
		const float SN = T == 0 ? P::SurfaceA(Marks, LW) : P::SurfaceB(Marks, LW);
		AddBoxPart(FVector(1.5f, GY, Zl((S0 + SN) * 0.5f)), FVector(2.f, 22.f, SN - S0 + 18.f), Enamel);
		for (int32 M = 0; M <= Marks; ++M)
		{
			const float Z = T == 0 ? P::SurfaceA(M, LW) : P::SurfaceB(M, LW);
			AddBoxPart(FVector(2.8f, GY + 2.f, Zl(Z)), FVector(0.8f, 8.f, 1.2f), InkMat);
			AddLabel(FString::FromInt(M), FVector(3.2f, GY + 9.f, Zl(Z)), 5.5f, FColor(20, 20, 24));
		}
		UStaticMeshComponent* Column = AddBoxPart(FVector(3.f, GY - 6.f, Zl(S0)), FVector(1.5f, 4.f, 1.f), Water);
		GaugeColumns.Add(Column);
		GaugeFloats.Add(AddBoxPart(FVector(3.6f, GY - 6.f, Zl(S0)), FVector(2.f, 7.f, 2.5f), FloatMat));
		AddLabel(T == 0 ? TEXT("A") : TEXT("B"), FVector(1.5f, GY, Zl(K.Rim) - 14.f), 22.f, FColor(30, 60, 70));
	}

	// Vanne A : dans la cloison, au fond du bassin A ; elle se deverse dans B par une bouche (jet visible)
	AddPart(A->Cylinder(), FVector(-60.f, P::BY0 + 4.f, Zl(Ba + 6.f)), FRotator(0.f, 0.f, 90.f), FVector(12.f, 12.f, 10.f), DarkMetal);
	Flows.Add(AddBoxPart(FVector(-60.f, P::BY0 + 11.f, Zl(Ba)), FVector(16.f, 3.f, 1.f), Water));
	// Vanne B : vidange vers le canal (conduite coudee devant le bassin, bouche au-dessus d'une grille)
	const float DY = 96.f;
	AddRod(FVector(-2.f, DY, Zl(Bb + 10.f)), FVector(30.f, DY, Zl(Bb + 10.f)), 9.f, Steel);
	AddRod(FVector(30.f, DY, Zl(Bb + 14.f)), FVector(30.f, DY, Zl(Bb - 14.f)), 9.f, Steel);
	AddBoxPart(FVector(30.f, DY, 1.f), FVector(32.f, 32.f, 2.f), DarkMetal);
	for (int32 I = 0; I < 4; ++I)
	{
		AddBoxPart(FVector(30.f, DY - 12.f + I * 8.f, 2.2f), FVector(30.f, 2.f, 0.6f), InkMat);
	}
	Flows.Add(AddBoxPart(FVector(30.f, DY, Zl(Bb - 14.f) * 0.5f), FVector(5.f, 7.f, 1.f), Water));
	// Debordement de B (regle depassee) : nappe sur l'avant du bassin
	Flows.Add(AddBoxPart(FVector(1.2f, (P::BY0 + P::BY1) * 0.5f, Zl(Rb) * 0.5f), FVector(1.f, P::BY1 - P::BY0 - 20.f, Zl(Rb)), Water));
	// Regards des indicateurs de courant (roue a aubes) : sous la vanne A, et sur la vidange
	const FVector Windows[2] = { FVector(1.f, 0.f, WorkZ - 52.f), FVector(1.f, DY, Zl(Bb + 30.f)) };
	for (const FVector& Wd : Windows)
	{
		AddPart(A->Cylinder(), Wd, FRotator(90.f, 0.f, 0.f), FVector(20.f, 20.f, 2.f), DarkMetal);
		USceneComponent* Pd = NewObject<USceneComponent>(this);
		Pd->SetupAttachment(Root);
		Pd->SetRelativeLocation(Wd + FVector(1.5f, 0.f, 0.f));
		Pd->RegisterComponent();
		Paddles.Add(Pd);
		AddBoxPart(FVector::ZeroVector, FVector(1.f, 15.f, 3.f), Steel, Pd);
		AddBoxPart(FVector::ZeroVector, FVector(1.f, 3.f, 15.f), Steel, Pd);
	}
	// Tige de la vanne A dans la cloison
	AddPart(A->Cylinder(), FVector(3.f, 0.f, WorkZ), FRotator(90.f, 0.f, 0.f), FVector(7.f, 7.f, 6.f), DarkMetal);
	for (UStaticMeshComponent* F : Flows)
	{
		if (F)
		{
			F->SetVisibility(false);
		}
	}
}

void ABRMissionDevice::BuildPoolLock()
{
	UBRAssets* A = UBRAssets::Get(this);
	ABRWorld* W = ABRWorld::Get(this);
	if (!A || !W || !Moving)
	{
		return;
	}
	namespace P = BRMech::Pool;
	const FBRLevelDef& D = W->Def();
	const float LW = D.WaterHeight;
	const float Z0 = static_cast<float>(GetActorLocation().Z);
	auto Zl = [Z0](float Abs) { return Abs - Z0; };
	const float L = P::ChannelLength, HW = P::ChannelHalfWidth, CW = P::ChannelWall;
	const float Slab = P::SlabTop(LW, D.DeckHeight);
	const float StepTop = P::EntryStepTop(LW, D.DeckHeight);
	UMaterialInterface* FloorMat = A->Surface(D.Floor);
	UMaterialInterface* Coping = A->Surface(FBRSurface(TEXT("T_Concrete"), FLinearColor(1.25f, 1.25f, 1.22f), 120.f, 0.45f, 0.1f));
	UMaterialInterface* DarkMetal = A->Surface(MetalSurf(FLinearColor(0.14f, 0.15f, 0.16f), 0.55f));
	UMaterialInterface* Steel = A->Surface(MetalSurf(FLinearColor(0.55f, 0.57f, 0.6f)));
	UMaterialInterface* Painted = A->Surface(MetalSurf(FLinearColor(0.25f, 0.42f, 0.4f), 0.5f));
	UMaterialInterface* Hazard = A->Surface(FBRSurface(TEXT("T_Hazard"), FLinearColor(1.f, 1.f, 1.f), 40.f, 0.6f, 0.3f));
	const FBRSurface WaterS = W->GetWaterSurface();
	UMaterialInterface* Water = A->WaterMaterial(WaterS, D.WaterAbsorption, D.WaterScattering, 0.15f, 0.3f);

	// Dallage du sas (au ras des trottoirs, au-dessus de l'eau des canaux) et marche d'entree
	AddSolid(FVector(-L, -(HW + CW), Zl(-2.f)), FVector(0.f, HW + CW, Zl(Slab)), FloorMat, true);
	if (Z0 < StepTop - 1.f)
	{
		AddSolid(FVector(0.f, -HW, Zl(-2.f)), FVector(P::EntryStepDepth, HW, Zl(StepTop)), FloorMat, true);
	}
	// Murets carreles (ligne d'eau mobile), margelles, garde-corps : on ne passe que par l'entree
	FBRSurface Lining = D.Wall;
	Lining.WaterLine = Slab;
	UMaterialInstanceDynamic* MID = A->NewSurface(Lining, this);
	WaterLineMIDs.Add(MID);
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float YIn = Side * HW, YOut = Side * (HW + CW);
		AddSolid(FVector(-L, FMath::Min(YIn, YOut), Zl(Slab)), FVector(0.f, FMath::Max(YIn, YOut), Zl(Slab + P::ChannelWallHeight)), MID, false);
		AddBoxPart(FVector(-L * 0.5f, Side * (HW + CW * 0.5f), Zl(Slab + P::ChannelWallHeight) + 2.f), FVector(L, CW + 4.f, 4.f), Coping);
		const int32 Posts = FMath::FloorToInt((L - 16.f) / 56.f) + 1;
		for (int32 I = 0; I < Posts; ++I)
		{
			const float X = -L + 10.f + I * (L - 16.f) / FMath::Max(1, Posts - 1);
			AddSolid(FVector(X - 2.5f, Side * (HW + CW * 0.5f) - 2.5f, Zl(Slab + P::ChannelWallHeight + 4.f)),
				FVector(X + 2.5f, Side * (HW + CW * 0.5f) + 2.5f, Zl(Slab + P::RailHeight)), Steel, false);
		}
		AddSolid(FVector(-L, Side * (HW + CW * 0.5f) - 2.f, Zl(Slab + P::RailHeight) - 4.f), FVector(0.f, Side * (HW + CW * 0.5f) + 2.f, Zl(Slab + P::RailHeight)), Steel, false);
		AddSolid(FVector(-L, Side * (HW + CW * 0.5f) - 1.5f, Zl(Slab + 162.f) - 3.f), FVector(0.f, Side * (HW + CW * 0.5f) + 1.5f, Zl(Slab + 162.f)), Steel, false);
		// Glissieres du deversoir
		AddSolid(FVector(-13.f, Side > 0 ? HW - 3.f : -(HW + CW), Zl(Slab)), FVector(-1.f, Side > 0 ? HW + CW : -(HW - 3.f), Zl(Slab + 200.f)), DarkMetal, false);
	}
	// Traverse au-dessus de l'entree (on passe dessous) et son moteur
	AddBoxPart(FVector(-7.f, 0.f, Zl(Slab + 204.f)), FVector(14.f, 2.f * (HW + CW), 10.f), DarkMetal);
	AddBoxPart(FVector(-7.f, HW * 0.5f, Zl(Slab + 216.f)), FVector(22.f, 30.f, 16.f), Painted);
	// Deversoir : un panneau qui descend dans sa fente juste devant l'eau qui baisse
	Moving->SetRelativeLocation(FVector(-7.f, 0.f, Zl(Slab)));
	AddSolid(FVector(-4.f, -HW, -P::WeirMax), FVector(4.f, HW, 0.f), Painted, true, Moving);
	AddBoxPart(FVector(0.f, 0.f, -3.f), FVector(9.f, 2.f * HW, 6.f), Hazard, Moving);
	// Bonde au fond du sas, contre le mur de l'echelle
	AddBoxPart(FVector(-L + 35.f, 0.f, Zl(Slab) + 0.6f), FVector(40.f, 70.f, 1.2f), DarkMetal);
	for (int32 I = 0; I < 6; ++I)
	{
		AddBoxPart(FVector(-L + 35.f, -30.f + I * 12.f, Zl(Slab) + 1.4f), FVector(38.f, 2.f, 0.6f), Steel);
	}
	// Eau du sas et nappe qui passe par-dessus le deversoir pendant la vidange
	WaterPlanes.Add(AddPart(A->Plane(), FVector((-L - 8.f) * 0.5f, 0.f, Zl(P::ChannelFull(Slab))), FRotator::ZeroRotator, FVector(L - 8.f, 2.f * HW, 100.f), Water));
	Flows.Add(AddBoxPart(FVector(3.f, 0.f, 0.f), FVector(3.f, 2.f * HW * 0.9f, 1.f), Water));
	Flows[0]->SetVisibility(false);
}

void ABRMissionDevice::BuildBridge()
{
	UBRAssets* A = UBRAssets::Get(this);
	ABRWorld* W = ABRWorld::Get(this);
	if (!A || !W || !Moving)
	{
		return;
	}
	namespace BB = BRMech::Bridge;
	const FBRLevelDef& D = W->Def();
	const float Z0 = static_cast<float>(GetActorLocation().Z);
	UMaterialInterface* Rock = A->Surface(D.Wall);
	UMaterialInterface* DarkMetal = A->Surface(MetalSurf(FLinearColor(0.14f, 0.15f, 0.16f), 0.55f));
	UMaterialInterface* Steel = A->Surface(MetalSurf(FLinearColor(0.5f, 0.5f, 0.52f), 0.5f));
	UMaterialInterface* Wood = A->Surface(FBRSurface(TEXT("T_Grime"), FLinearColor(0.42f, 0.3f, 0.2f), 80.f, 0.8f, 0.4f));
	UMaterialInterface* Hazard = A->Surface(FBRSurface(TEXT("T_Hazard"), FLinearColor(1.f, 1.f, 1.f), 40.f, 0.6f, 0.3f));
	UMaterialInterface* CableMat = A->Surface(MetalSurf(FLinearColor(0.08f, 0.08f, 0.08f), 0.7f));

	// Palier de l'echelle, interruption, appui d'arrivee et ses marches (on contourne le module au sol, des deux cotes)
	AddSolid(FVector(-BB::FarDepth, -BB::HalfWidth, -2.f), FVector(0.f, BB::HalfWidth, BB::FarTop), Rock, true);
	AddSolid(FVector(BB::Gap, -BB::HalfWidth, -2.f), FVector(BB::Gap + BB::NearDepth, BB::HalfWidth, BB::NearTop), Rock, true);
	for (int32 I = 0; I < BB::Steps; ++I)
	{
		const float SX = BB::Gap + BB::NearDepth + I * BB::StepDepth;
		const float Top = BB::NearTop * (BB::Steps - I) / (BB::Steps + 1.f);
		AddSolid(FVector(SX, -BB::HalfWidth, -2.f), FVector(SX + BB::StepDepth, BB::HalfWidth, Top), Rock, true);
	}
	// Charniere, plaque d'appui, bords signales
	AddBoxPart(FVector(-3.f, 0.f, BB::FarTop - 3.f), FVector(6.f, 2.f * BB::DeckHalfWidth + 20.f, 6.f), DarkMetal);
	AddBoxPart(FVector(BB::Gap + (BB::Bearing + 10.f) * 0.5f, 0.f, BB::NearTop + 0.5f), FVector(BB::Bearing + 10.f, 2.f * BB::DeckHalfWidth, 1.f), Steel);
	AddBoxPart(FVector(-6.f, 0.f, BB::FarTop + 0.4f), FVector(10.f, 2.f * BB::HalfWidth, 0.8f), Hazard);
	AddBoxPart(FVector(BB::Gap + 5.f, 0.f, BB::NearTop + 0.4f), FVector(10.f, 2.f * BB::HalfWidth, 0.8f), Hazard);
	// Tablier : charniere en haut de l'arete du palier ; vraie surface portante qui suit son animation
	const float Len = BB::DeckLength();
	Moving->SetRelativeLocation(FVector(0.f, 0.f, BB::HingeZ()));
	AddSolid(FVector(0.f, -BB::DeckHalfWidth, -BB::DeckThick * 0.5f), FVector(Len, BB::DeckHalfWidth, BB::DeckThick * 0.5f), Wood, false, Moving);
	for (int32 I = 0; I < 9; ++I)
	{
		AddBoxPart(FVector(14.f + I * (Len - 28.f) / 8.f, 0.f, BB::DeckThick * 0.5f + 0.6f), FVector(6.f, 2.f * BB::DeckHalfWidth - 6.f, 1.2f), Steel, Moving);
	}
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		AddBoxPart(FVector(Len * 0.5f, Side * (BB::DeckHalfWidth - 3.f), BB::DeckThick * 0.5f + 5.f), FVector(Len, 6.f, 10.f), DarkMetal, Moving);
		AddBoxPart(FVector(Len - 8.f, Side * (BB::DeckHalfWidth - 6.f), BB::DeckThick * 0.5f + 12.f), FVector(8.f, 4.f, 8.f), Steel, Moving);
	}
	// Portique sur le palier : poteaux, traverse, poulies ; cables vers le bout du tablier (places a chaque image)
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float PY = Side * (BB::DeckHalfWidth + 15.f);
		AddSolid(FVector(-35.f, PY - 5.f, BB::FarTop), FVector(-25.f, PY + 5.f, BB::FarTop + 230.f), DarkMetal, false);
		AddPart(A->Cylinder(), FVector(-30.f, Side * (BB::DeckHalfWidth - 6.f), BB::FarTop + 222.f), FRotator(0.f, 0.f, 90.f), FVector(16.f, 16.f, 6.f), Steel);
		Cables.Add(AddRod(FVector(-30.f, Side * (BB::DeckHalfWidth - 6.f), BB::FarTop + 214.f), FVector(Len, Side * (BB::DeckHalfWidth - 6.f), BB::HingeZ() + 12.f), 2.f, CableMat));
		// Poteau de main courante sur l'appui ; corde tendue une fois le tablier pose
		AddSolid(FVector(BB::Gap + 22.f, Side * (BB::DeckHalfWidth + 6.f) - 3.f, BB::NearTop), FVector(BB::Gap + 28.f, Side * (BB::DeckHalfWidth + 6.f) + 3.f, BB::NearTop + 100.f), DarkMetal, false);
		Ropes.Add(AddRod(FVector(-30.f, PY, BB::FarTop + 100.f), FVector(BB::Gap + 25.f, Side * (BB::DeckHalfWidth + 6.f), BB::NearTop + 100.f), 2.5f, CableMat));
	}
	AddBoxPart(FVector(-30.f, 0.f, BB::FarTop + 234.f), FVector(10.f, 2.f * (BB::DeckHalfWidth + 20.f), 8.f), DarkMetal);
	// Le cable des treuils : du portique vers la paroi, puis le long du plafond vers les galeries (lien visible avec les
	// treuils de la mission)
	const float Ceil = D.WallHeight - Z0 - 18.f;
	AddRod(FVector(-30.f, 0.f, BB::FarTop + 238.f), FVector(-BB::FarDepth + 8.f, 0.f, Ceil), 2.5f, CableMat);
	AddRod(FVector(-BB::FarDepth + 8.f, 0.f, Ceil), FVector(BB::TotalLength + 260.f, 0.f, Ceil), 2.5f, CableMat);
	for (int32 I = 0; I < 6; ++I)
	{
		const float X = -BB::FarDepth + 40.f + I * (BB::TotalLength + 200.f) / 5.f;
		AddBoxPart(FVector(X, 0.f, Ceil + 8.f), FVector(3.f, 3.f, 16.f), DarkMetal);
	}
	for (UStaticMeshComponent* R : Ropes)
	{
		R->SetVisibility(false);
	}
}

void ABRMissionDevice::ApplyModuleState(const BRM::FPlan& Plan, const BRM::FState& State, const BRM::FEval& Eval, bool bAnimate)
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W)
	{
		return;
	}
	const bool bSnap = !bAnimate || !bShown;
	const BRMech::FFrame F = ModuleFrame();
	const FBRLevelDef& D = W->Def();
	namespace P = BRMech::Pool;
	switch (Module)
	{
	case BRMech::Module::PoolTanks:
	{
		int32 LA = 0, LB = 0;
		BRM::PoolLevels(Plan, State, LA, LB);
		const float X0 = -P::TankDepth + P::Wall, X1 = -P::Wall;
		W->SetLocalWater(this, 0, BRMech::MakeBox(F, X0, P::AY0, X1, P::AY1, P::BottomA(D.WaterHeight), P::RimA(D.WaterHeight), 0.f), P::SurfaceA(LA, D.WaterHeight),
			P::TankSpeed, bSnap);
		W->SetLocalWater(this, 1, BRMech::MakeBox(F, X0, P::BY0, X1, P::BY1, P::BottomB(D.WaterHeight), P::RimB(D.WaterHeight), 0.f), P::SurfaceB(LB, D.WaterHeight),
			P::TankSpeed, bSnap);
		bOverflow = SluiceBIndex >= 0 && Eval.WarningDevice == SluiceBIndex;
		break;
	}
	case BRMech::Module::PoolLock:
	{
		const float Slab = P::SlabTop(D.WaterHeight, D.DeckHeight);
		// Meme etat que la sortie : la mission resolue ouvre le passage, le sas se vide et le deversoir descend
		const bool bOpen = Eval.bSolved || GateValue >= 200;
		W->SetLocalWater(this, 0, BRMech::MakeBox(F, -P::ChannelLength, -P::ChannelHalfWidth, -8.f, P::ChannelHalfWidth, Slab, Slab + P::ChannelWallHeight, 0.f),
			bOpen ? P::ChannelDry(Slab) : P::ChannelFull(Slab), P::DrainSpeed, bSnap);
		break;
	}
	case BRMech::Module::Bridge:
		ModuleTarget = GateValue / 255.f;
		if (bSnap)
		{
			ModuleProgress = ModuleTarget;
		}
		W->SetBridgeNav(F, ModuleProgress >= 0.99f && GateValue >= 255);
		break;
	default:
		break;
	}
	if (!bSnap)
	{
		SetActorTickEnabled(true);
	}
}

bool ABRMissionDevice::UpdateModule(float Dt)
{
	ABRWorld* W = ABRWorld::Get(this);
	UBRAssets* A = UBRAssets::Get(this);
	if (!W)
	{
		return false;
	}
	const FBRLevelDef& D = W->Def();
	const float Z0 = static_cast<float>(GetActorLocation().Z);
	namespace P = BRMech::Pool;
	FlowSoundCooldown = FMath::Max(0.f, FlowSoundCooldown - Dt);
	auto FlowSound = [&](bool bFlowing, float Volume)
	{
		if (bFlowing && !bFlowSound && FlowSoundCooldown <= 0.f && A && Dt > 0.f)
		{
			if (USoundBase* S = A->Sound(TEXT("S_Splash")))
			{
				UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), Volume);
			}
			FlowSoundCooldown = 1.2f;
		}
		bFlowSound = bFlowing;
	};
	bool bMoving = false;
	switch (Module)
	{
	case BRMech::Module::PoolTanks:
	{
		float RateA = 0.f, RateB = 0.f;
		const float SA = W->GetLocalWaterSurface(this, 0, &RateA);
		const float SB = W->GetLocalWaterSurface(this, 1, &RateB);
		if (SA < -1.0e5f || SB < -1.0e5f || WaterPlanes.Num() < 2 || GaugeColumns.Num() < 2 || Flows.Num() < 3)
		{
			return false;
		}
		const float Surf[2] = { SA, SB };
		const float S0[2] = { P::SurfaceA(0, D.WaterHeight), P::SurfaceB(0, D.WaterHeight) };
		for (int32 T = 0; T < 2; ++T)
		{
			FVector PL = WaterPlanes[T]->GetRelativeLocation();
			PL.Z = Surf[T] - Z0;
			WaterPlanes[T]->SetRelativeLocation(PL);
			if (WaterLineMIDs.IsValidIndex(T) && WaterLineMIDs[T])
			{
				WaterLineMIDs[T]->SetScalarParameterValue(TEXT("WaterLine"), Surf[T]);
			}
			// Colonne de la regle : du bas de la regle a la surface ; flotteur a la surface
			const float Bottom = S0[T] - 8.f;
			const float H = FMath::Max(1.f, Surf[T] - Bottom);
			FVector CL = GaugeColumns[T]->GetRelativeLocation();
			CL.Z = Bottom + H * 0.5f - Z0;
			GaugeColumns[T]->SetRelativeLocation(CL);
			GaugeColumns[T]->SetRelativeScale3D(FVector(1.5f, 4.f, H) / 100.f);
			FVector FL = GaugeFloats[T]->GetRelativeLocation();
			FL.Z = Surf[T] - Z0;
			GaugeFloats[T]->SetRelativeLocation(FL);
		}
		// Transfert A -> B (positif) et vidange de B vers le canal (positif), d'apres les variations des surfaces
		const float Transfer = -RateA;
		const float Drain = Transfer - RateB;
		const float SpoutZ = P::BottomA(D.WaterHeight) + 6.f;
		const float FallH = FMath::Max(1.f, SpoutZ - SB);
		Flows[0]->SetVisibility(Transfer > 1.f);
		Flows[0]->SetRelativeLocation(FVector(-60.f, P::BY0 + 11.f, SpoutZ - FallH * 0.5f - Z0));
		Flows[0]->SetRelativeScale3D(FVector(FMath::Clamp(Transfer, 4.f, 16.f), 3.f, FallH) / 100.f);
		const float MouthZ = P::BottomB(D.WaterHeight) - 14.f;
		const float DrainH = FMath::Max(1.f, MouthZ - Z0);
		Flows[1]->SetVisibility(Drain > 1.f);
		Flows[1]->SetRelativeLocation(FVector(30.f, 96.f, DrainH * 0.5f));
		Flows[1]->SetRelativeScale3D(FVector(5.f, FMath::Clamp(Drain * 0.6f, 3.f, 9.f), DrainH) / 100.f);
		Flows[2]->SetVisibility(bOverflow);
		if (Paddles.Num() >= 2)
		{
			Paddles[0]->AddRelativeRotation(FRotator(0.f, 0.f, Transfer * Dt * 25.f));
			Paddles[1]->AddRelativeRotation(FRotator(0.f, 0.f, Drain * Dt * 25.f));
		}
		FlowSound(FMath::Abs(Transfer) > 1.f || FMath::Abs(Drain) > 1.f, 0.35f);
		bMoving = FMath::Abs(RateA) > 0.01f || FMath::Abs(RateB) > 0.01f || bOverflow;
		break;
	}
	case BRMech::Module::PoolLock:
	{
		float Rate = 0.f;
		const float S = W->GetLocalWaterSurface(this, 0, &Rate);
		if (S < -1.0e5f || WaterPlanes.Num() < 1 || Flows.Num() < 1)
		{
			return false;
		}
		const float Slab = P::SlabTop(D.WaterHeight, D.DeckHeight);
		const float Weir = P::WeirHeight(S, Slab);
		Moving->SetRelativeLocation(FVector(-7.f, 0.f, Slab + Weir - Z0));
		FVector PL = WaterPlanes[0]->GetRelativeLocation();
		PL.Z = S - Z0;
		WaterPlanes[0]->SetRelativeLocation(PL);
		WaterPlanes[0]->SetVisibility(S > Slab + 0.5f);
		if (WaterLineMIDs.Num() > 0 && WaterLineMIDs[0])
		{
			WaterLineMIDs[0]->SetScalarParameterValue(TEXT("WaterLine"), FMath::Max(S, Slab));
		}
		// Pendant la vidange, l'eau passe par-dessus le deversoir et tombe devant l'entree
		const bool bSpill = Rate < -1.f && Weir > 1.f;
		const float Top = Slab + Weir - Z0;
		Flows[0]->SetVisibility(bSpill);
		Flows[0]->SetRelativeLocation(FVector(3.f, 0.f, Top * 0.5f));
		Flows[0]->SetRelativeScale3D(FVector(3.f, 2.f * P::ChannelHalfWidth * 0.9f, FMath::Max(1.f, Top)) / 100.f);
		FlowSound(bSpill, 0.8f);
		bMoving = FMath::Abs(Rate) > 0.01f;
		break;
	}
	case BRMech::Module::Bridge:
	{
		namespace BB = BRMech::Bridge;
		const float Before = ModuleProgress;
		ModuleProgress = Dt > 0.f ? BRMech::Approach(ModuleProgress, ModuleTarget, BB::Speed, Dt) : ModuleProgress;
		Moving->SetRelativeRotation(FRotator(BB::DeckPitch(ModuleProgress), 0.f, 0.f));
		float TX = 0.f, TZ = 0.f;
		BB::DeckTip(ModuleProgress, TX, TZ);
		const float R = FMath::DegreesToRadians(BB::DeckPitch(ModuleProgress));
		// Anneau du cable : au bout du tablier, sur sa face superieure
		const FVector Eye(TX - FMath::Sin(R) * 12.f - FMath::Cos(R) * 8.f, 0.f, TZ + FMath::Cos(R) * 12.f - FMath::Sin(R) * 8.f);
		for (int32 I = 0; I < Cables.Num(); ++I)
		{
			const float Side = I == 0 ? -1.f : 1.f;
			PlaceRod(Cables[I], FVector(-30.f, Side * (BB::DeckHalfWidth - 6.f), BB::FarTop + 214.f), FVector(Eye.X, Side * (BB::DeckHalfWidth - 6.f), Eye.Z), 2.f);
		}
		for (UStaticMeshComponent* Rope : Ropes)
		{
			Rope->SetVisibility(ModuleProgress >= 0.98f);
		}
		W->SetBridgeNav(ModuleFrame(), ModuleProgress >= 0.99f && GateValue >= 255);
		const bool bMove = !FMath::IsNearlyEqual(Before, ModuleProgress);
		if (bMove && !bFlowSound && A && FlowSoundCooldown <= 0.f && Dt > 0.f)
		{
			if (USoundBase* S = A->Sound(TEXT("S_M_Crank")))
			{
				UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation() + FVector(0.f, 0.f, BB::FarTop), 0.7f);
			}
			FlowSoundCooldown = 1.5f;
		}
		bFlowSound = bMove;
		bMoving = bMove || !FMath::IsNearlyEqual(ModuleProgress, ModuleTarget);
		break;
	}
	default:
		break;
	}
	return bMoving;
}
