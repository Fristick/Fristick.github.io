#include "BRPhenomena.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRRig.h"
#include "BRWorld.h"

#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

ABRHallucination::ABRHallucination()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SetActorEnableCollision(false);
}

void ABRHallucination::Init(EBRHallucination InForm, ABRCharacter* InViewer)
{
	Form = InForm;
	Viewer = InViewer;
	MaxLife = FMath::FRandRange(6.f, 11.f);

	if (Form == EBRHallucination::Shadow)
	{
		// Une silhouette sans visage (modele du Faceling) d'un noir d'encre, un peu trop grande, un peu voutee
		TMap<FString, FLinearColor> Tints;
		const FLinearColor InkBlack(0.004f, 0.004f, 0.005f);
		Tints.Add(TEXT("Faceling"), InkBlack);
		Tints.Add(TEXT("Cloth"), InkBlack);
		Tints.Add(TEXT("Skin"), InkBlack);
		if (BRRig::HasMesh(this, TEXT("SM_FacelingET_Torso")))
		{
			BRRig::BuildHumanoid(this, Root, TEXT("SM_FacelingET"), FBRHumanoidSpec::FacelingET(), &Tints, Parts, false);
		}
		else
		{
			FBRHumanoidSpec Spec = FBRHumanoidSpec::Simple(100.f, 160.f, 20.f, 10.f, 8.f, 36.f, 52.f, 7.f);
			Spec.ArmPitch = 2.f;
			Spec.ArmRoll = 3.f;
			BRRig::BuildHumanoid(this, Root, TEXT("SM_Faceling"), Spec, &Tints, Parts, false);
		}
	}
	else
	{
		TArray<UMaterialInstanceDynamic*> Glows;
		const TCHAR* SmilerMesh = BRRig::HasMesh(this, TEXT("SM_SmilerET")) ? TEXT("SM_SmilerET") : TEXT("SM_Smiler");
		BRRig::AddPart(this, Root, SmilerMesh, FVector(0.f, 0.f, 140.f), FVector(70.f, 70.f, 90.f), 0.f, nullptr, Parts, nullptr, false,
			true, 0.5f, &Glows);
		for (UMaterialInstanceDynamic* G : Glows)
		{
			GlowMIDs.Add(G);
		}
		Glow = NewObject<UPointLightComponent>(this);
		Glow->SetupAttachment(Root);
		Glow->SetRelativeLocation(FVector(60.f, 0.f, 140.f));
		Glow->SetIntensityUnits(ELightUnits::Lumens);
		Glow->SetIntensity(40.f);
		Glow->SetAttenuationRadius(220.f);
		Glow->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
		Glow->SetCastShadows(false);
		Glow->RegisterComponent();
	}

	// Rien ne la touche : ni la visee du joueur, ni le regard des entites
	TArray<UPrimitiveComponent*> Prims;
	GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* P : Prims)
	{
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABRHallucination::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Life += DeltaSeconds;
	ABRCharacter* V = Viewer.Get();
	const ABRWorld* W = ABRWorld::Get(this);
	if (!V || V->IsDead() || Life > MaxLife || !W || W->IsTransitioning())
	{
		Vanish(false);
		return;
	}

	// Elle fixe le joueur
	const FVector Eye = V->GetEyeLocation();
	FVector Face = Eye - GetActorLocation();
	Face.Z = 0.f;
	if (!Face.IsNearlyZero())
	{
		SetActorRotation(Face.Rotation());
	}

	const FVector Center = GetActorLocation() + FVector(0.f, 0.f, 120.f);
	const float Dist = static_cast<float>(FVector::Dist(Center, Eye));
	if (Dist < 650.f)
	{
		Vanish(true); // on s'approche : il n'y a jamais rien eu
		return;
	}
	const float Dot = static_cast<float>(FVector::DotProduct((Center - Eye).GetSafeNormal(), V->GetViewDirection()));

	if (Form == EBRHallucination::Smile)
	{
		// Le sourire palpite ; la lampe ou un regard soutenu le dissipent
		const float G = 0.75f + 0.25f * FMath::Sin(Life * 9.f);
		for (UMaterialInstanceDynamic* M : GlowMIDs)
		{
			if (M)
			{
				M->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(1.f, 0.96f, 0.86f) * 55.f * G);
			}
		}
		if (Glow)
		{
			Glow->SetIntensity(40.f * G);
		}
		if (V->IsFlashlightOn() && Dot > 0.95f)
		{
			Vanish(true);
			return;
		}
	}

	// Regarde en face : la silhouette s'efface aussitot, le sourire apres un instant
	if (Dot > 0.975f)
	{
		Stare += DeltaSeconds;
		if (Stare > (Form == EBRHallucination::Shadow ? 0.12f : 0.9f))
		{
			Vanish(Form == EBRHallucination::Smile);
		}
	}
	else
	{
		Stare = 0.f;
	}
}

void ABRHallucination::Vanish(bool bWhisper)
{
	if (IsActorBeingDestroyed())
	{
		return;
	}
	if (bWhisper)
	{
		if (UBRAssets* A = UBRAssets::Get(this))
		{
			if (USoundBase* S = A->Sound(TEXT("S_Whisper")))
			{
				UGameplayStatics::PlaySound2D(this, S, 0.35f, FMath::FRandRange(0.85f, 1.05f));
			}
		}
	}
	Destroy();
}
