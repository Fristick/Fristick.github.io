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

FBRHumanoidSpec FBRHumanoidSpec::FromJoints(const FVector& InTorso, const FVector& InHead, const FVector& ShoulderL, const FVector& ElbowL,
	const FVector& ShoulderR, const FVector& ElbowR, const FVector& HipL, const FVector& KneeL, const FVector& HipR, const FVector& KneeR)
{
	FBRHumanoidSpec S;
	S.Torso = InTorso;
	S.Head = InHead;
	S.Shoulder[0] = ShoulderL;
	S.Shoulder[1] = ShoulderR;
	S.Elbow[0] = ElbowL;
	S.Elbow[1] = ElbowR;
	S.Hip[0] = HipL;
	S.Hip[1] = HipR;
	S.Knee[0] = KneeL;
	S.Knee[1] = KneeR;
	S.bSidedMeshes = true;
	S.ArmPitch = 0.f;
	S.ElbowPitch = 0.f;
	S.ArmRoll = 0.f;
	return S;
}

// Valeurs produites par Tools/Blender/import_user_models.py (RawAssets/Meshes/user_models.json)
FBRHumanoidSpec FBRHumanoidSpec::SkinStealerET()
{
	FBRHumanoidSpec S = FromJoints(FVector(0.f, 0.3f, 103.81f), FVector(-0.24f, 0.55f, 164.1f), FVector(-4.62f, -11.62f, 154.41f),
		FVector(-5.11f, -18.4f, 106.14f), FVector(-4.62f, 11.73f, 154.41f), FVector(-5.11f, 18.51f, 106.14f), FVector(0.f, -7.24f, 100.65f),
		FVector(0.49f, -14.78f, 58.07f), FVector(0.f, 7.24f, 100.65f), FVector(0.49f, 15.51f, 58.07f));
	S.ArmAmp = 14.f; // ses bras demesures balancent peu : ils pendent jusqu'aux genoux
	S.LegAmp = 26.f;
	return S;
}

FBRHumanoidSpec FBRHumanoidSpec::FacelingET()
{
	FBRHumanoidSpec S = FromJoints(FVector(0.f, 0.f, 90.12f), FVector(-0.14f, 0.f, 153.49f), FVector(0.f, -13.89f, 145.36f),
		FVector(-3.89f, -19.27f, 114.68f), FVector(0.f, 13.89f, 145.36f), FVector(-3.89f, 19.27f, 114.68f), FVector(0.f, -7.85f, 93.17f),
		FVector(0.83f, -9.17f, 54.85f), FVector(0.f, 7.85f, 93.17f), FVector(0.83f, 9.17f, 54.85f));
	S.ArmAmp = 22.f;
	S.LegAmp = 28.f;
	return S;
}

FBRHumanoidSpec FBRHumanoidSpec::PartygoerET()
{
	FBRHumanoidSpec S = FromJoints(FVector(1.54f, 0.f, 109.58f), FVector(1.88f, 0.f, 162.74f), FVector(-3.04f, -19.57f, 154.62f),
		FVector(-1.82f, -24.73f, 125.16f), FVector(-3.04f, 19.57f, 154.62f), FVector(-1.81f, 24.74f, 125.11f), FVector(0.f, -8.41f, 103.98f),
		FVector(-2.7f, -14.9f, 59.58f), FVector(0.f, 8.41f, 103.98f), FVector(-2.7f, 14.9f, 59.58f));
	S.ArmAmp = 18.f;
	S.LegAmp = 26.f;
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

	bool HasMesh(const UObject* WorldContext, FName MeshName)
	{
		UBRAssets* A = UBRAssets::Get(WorldContext);
		return A && A->Mesh(MeshName) != nullptr;
	}

	bool HasHazmat(const UObject* WorldContext)
	{
		UBRAssets* A = UBRAssets::Get(WorldContext);
		return A && A->Mesh(TEXT("SM_Hazmat_Torso")) != nullptr;
	}
}
