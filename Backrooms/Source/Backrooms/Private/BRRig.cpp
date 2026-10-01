#include "BRRig.h"
#include "BRAssets.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

FBRHumanoidSpec FBRHumanoidSpec::Simple(float HipZ, float ShoulderZ, float ShoulderW, float HipW, float Hunch, float UpperArmLen, float ThighLen, float Neck)
{
	FBRHumanoidSpec S;
	S.Torso = FVector(0.f, 0.f, HipZ);
	S.Head = FVector(Hunch * 1.05f, 0.f, ShoulderZ + Neck);
	for (int32 i = 0; i < 2; ++i)
	{
		const float Sgn = i == 0 ? -1.f : 1.f;
		S.Shoulder[i] = FVector(Hunch, Sgn * ShoulderW, ShoulderZ);
		S.Elbow[i] = S.Shoulder[i] + FVector(0.f, 0.f, -UpperArmLen);
		S.Hip[i] = FVector(0.f, Sgn * HipW, HipZ);
		S.Knee[i] = S.Hip[i] + FVector(0.f, 0.f, -ThighLen);
	}
	return S;
}

FBRHumanoidSpec FBRHumanoidSpec::Hazmat()
{
	// Valeurs produites par Tools/Blender/import_user_models.py (RawAssets/Meshes/user_models.json)
	FBRHumanoidSpec S;
	S.Torso = FVector(0.12f, 0.f, 98.89f);
	S.Head = FVector(-1.44f, 0.f, 144.23f);
	S.Shoulder[0] = FVector(0.05f, -27.51f, 135.09f);
	S.Shoulder[1] = FVector(-2.93f, 27.5f, 135.09f);
	S.Elbow[0] = FVector(0.05f, -32.81f, 110.15f);
	S.Elbow[1] = FVector(-2.93f, 32.8f, 110.17f);
	S.Hip[0] = FVector(-0.1f, -10.17f, 93.86f);
	S.Hip[1] = FVector(0.1f, 10.17f, 93.86f);
	S.Knee[0] = FVector(0.27f, -10.25f, 55.51f);
	S.Knee[1] = FVector(-0.14f, 10.25f, 55.5f);
	S.bSidedMeshes = true;
	S.ArmPitch = 0.f;
	S.ElbowPitch = 0.f;
	S.ArmRoll = 0.f;
	S.ArmAmp = 26.f;
	S.LegAmp = 30.f;
	return S;
}

namespace BRRig
{
	USceneComponent* AddPart(AActor* Owner, USceneComponent* Parent, FName MeshName, const FVector& Joint, const FVector& FallbackSize,
		float FallbackDrop, const TMap<FString, FLinearColor>* Tints, TArray<TObjectPtr<USceneComponent>>& OutComponents,
		UPrimitiveComponent** OutMesh, bool bCastShadow, bool bUniqueGlow, float GlowScale, TArray<UMaterialInstanceDynamic*>* OutGlow)
	{
		if (!Owner || !Parent)
		{
			return nullptr;
		}
		UBRAssets* A = UBRAssets::Get(Owner);
		USceneComponent* Pivot = NewObject<USceneComponent>(Owner);
		Pivot->SetupAttachment(Parent);
		Pivot->SetRelativeLocation(Joint);
		Pivot->RegisterComponent();
		OutComponents.Add(Pivot);

		UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(Owner);
		MeshComp->SetupAttachment(Pivot);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetCastShadow(bCastShadow);
		UStaticMesh* SM = A ? A->Mesh(MeshName) : nullptr;
		if (SM)
		{
			MeshComp->SetStaticMesh(SM);
		}
		else if (A && A->Cube() && FallbackSize.X > 0.f)
		{
			MeshComp->SetStaticMesh(A->Cube());
			MeshComp->SetRelativeLocation(FVector(0.f, 0.f, FallbackDrop));
			MeshComp->SetRelativeScale3D(FallbackSize / 100.f);
		}
		MeshComp->RegisterComponent();
		OutComponents.Add(MeshComp);
		if (OutMesh)
		{
			*OutMesh = MeshComp;
		}

		if (A)
		{
			if (SM)
			{
				A->ApplySlots(MeshComp, Tints, bUniqueGlow, OutGlow, GlowScale);
			}
			else
			{
				FLinearColor C(0.5f, 0.48f, 0.45f);
				if (Tints && Tints->Num() > 0)
				{
					C = Tints->CreateConstIterator()->Value;
				}
				MeshComp->SetMaterial(0, A->Surface(FBRSurface(TEXT("T_Skin"), C, 100.f)));
			}
		}
		return Pivot;
	}

	FBRHumanoidParts BuildHumanoid(AActor* Owner, USceneComponent* Root, const TCHAR* Prefix, const FBRHumanoidSpec& Spec,
		const TMap<FString, FLinearColor>* Tints, TArray<TObjectPtr<USceneComponent>>& OutComponents, bool bCastShadow)
	{
		FBRHumanoidParts P;
		UBRAssets* A = UBRAssets::Get(Owner);
		const FString Pre(Prefix);
		auto Name = [&](const TCHAR* Part, int32 Side) -> FName
		{
			if (Spec.bSidedMeshes)
			{
				const FName Sided(*(Pre + TEXT("_") + Part + (Side == 0 ? TEXT("L") : TEXT("R"))));
				if (!A || A->Mesh(Sided))
				{
					return Sided;
				}
			}
			return FName(*(Pre + TEXT("_") + Part));
		};
		auto Track = [&P](USceneComponent* Pivot, UPrimitiveComponent* Mesh)
		{
			if (Mesh)
			{
				P.Meshes.Add(Mesh);
			}
			return Pivot;
		};

		UPrimitiveComponent* M = nullptr;
		const float TorsoH = FMath::Max(10.f, static_cast<float>(Spec.Shoulder[0].Z - Spec.Torso.Z));
		const float Width = static_cast<float>(FMath::Abs(Spec.Shoulder[1].Y - Spec.Shoulder[0].Y));
		P.Torso = Track(AddPart(Owner, Root, FName(*(Pre + TEXT("_Torso"))), Spec.Torso, FVector(28.f, Width, TorsoH + 10.f), TorsoH * 0.5f, Tints,
			OutComponents, &M, bCastShadow), M);
		P.Head = Track(AddPart(Owner, Root, FName(*(Pre + TEXT("_Head"))), Spec.Head, FVector(22.f, 19.f, 26.f), 13.f, Tints, OutComponents, &M,
			bCastShadow), M);
		for (int32 i = 0; i < 2; ++i)
		{
			const float UpperLen = static_cast<float>((Spec.Elbow[i] - Spec.Shoulder[i]).Size());
			const float ThighLen = static_cast<float>((Spec.Knee[i] - Spec.Hip[i]).Size());
			const float ShinLen = static_cast<float>(Spec.Knee[i].Z);
			P.UpperArm[i] = Track(AddPart(Owner, Root, Name(TEXT("UpperArm"), i), Spec.Shoulder[i], FVector(9.f, 9.f, UpperLen), -UpperLen * 0.5f, Tints,
				OutComponents, &M, bCastShadow), M);
			P.LowerArm[i] = Track(AddPart(Owner, P.UpperArm[i], Name(TEXT("LowerArm"), i), Spec.Elbow[i] - Spec.Shoulder[i],
				FVector(8.f, 8.f, UpperLen * 1.3f), -UpperLen * 0.65f, Tints, OutComponents, &M, bCastShadow), M);
			P.Thigh[i] = Track(AddPart(Owner, Root, Name(TEXT("Thigh"), i), Spec.Hip[i], FVector(13.f, 13.f, ThighLen), -ThighLen * 0.5f, Tints,
				OutComponents, &M, bCastShadow), M);
			P.Shin[i] = Track(AddPart(Owner, P.Thigh[i], Name(TEXT("Shin"), i), Spec.Knee[i] - Spec.Hip[i], FVector(11.f, 11.f, ShinLen), -ShinLen * 0.5f,
				Tints, OutComponents, &M, bCastShadow), M);
		}
		return P;
	}

	bool HasHazmat(const UObject* WorldContext)
	{
		UBRAssets* A = UBRAssets::Get(WorldContext);
		return A && A->Mesh(TEXT("SM_Hazmat_Torso")) != nullptr;
	}
}
