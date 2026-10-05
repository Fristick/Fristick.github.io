#include "BRRig.h"
#include "BRAssets.h"

#include "AnimationRuntime.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
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
	S.Torso = FVector(0.12f, 0.f, 98.9f);
	S.Head = FVector(-1.44f, 0.f, 144.25f);
	S.Shoulder[0] = FVector(0.05f, -27.51f, 135.1f);
	S.Shoulder[1] = FVector(-2.93f, 27.51f, 135.1f);
	S.Elbow[0] = FVector(0.05f, -32.81f, 110.16f);
	S.Elbow[1] = FVector(-2.93f, 32.8f, 110.18f);
	S.Hip[0] = FVector(-0.1f, -10.17f, 93.87f);
	S.Hip[1] = FVector(0.1f, 10.17f, 93.87f);
	S.Knee[0] = FVector(0.27f, -10.25f, 55.52f);
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
	FBRHumanoidSpec S = FromJoints(FVector(0.f, 0.3f, 103.95f), FVector(-0.24f, 0.55f, 164.33f), FVector(-4.63f, -11.64f, 154.64f),
		FVector(-5.12f, -18.43f, 106.28f), FVector(-4.63f, 11.74f, 154.64f), FVector(-5.12f, 18.54f, 106.28f), FVector(0.f, -7.25f, 100.79f),
		FVector(0.49f, -14.8f, 58.14f), FVector(0.f, 7.25f, 100.79f), FVector(0.49f, 15.53f, 58.14f));
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

// =====================================================================================================================
// v4.5 : maillages a squelette pilotes par des pivots
// =====================================================================================================================

USceneComponent* FBRSkinDriver::AddPivot(AActor* Owner, USceneComponent* Parent, FName Bone, TArray<TObjectPtr<USceneComponent>>& OutComponents)
{
	UPoseableMeshComponent* S = Skin.Get();
	USkinnedAsset* Asset = S ? S->GetSkinnedAsset() : nullptr;
	if (!Owner || !Parent || !Asset)
	{
		return nullptr;
	}
	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	const int32 Index = Ref.FindBoneIndex(Bone);
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}
	FLink L;
	L.Bone = Bone;
	L.BoneIndex = Index;
	const FTransform RestCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Index);
	L.RestRot = RestCS.GetRotation();
	L.RestPos = RestCS.GetLocation();

	// Position relative au parent : au repos, tous les pivots sont alignes sur le repere du maillage (rotation nulle)
	FVector Offset = L.RestPos;
	for (const FLink& Other : Links)
	{
		if (Other.Pivot.Get() == Parent)
		{
			Offset = L.RestPos - Other.RestPos;
			break;
		}
	}
	USceneComponent* Pivot = NewObject<USceneComponent>(Owner);
	Pivot->SetupAttachment(Parent);
	Pivot->SetRelativeLocation(Offset);
	Pivot->RegisterComponent();
	OutComponents.Add(Pivot);
	L.Pivot = Pivot;
	Links.Add(L);
	Links.Sort([](const FLink& A, const FLink& B) { return A.BoneIndex < B.BoneIndex; });
	return Pivot;
}

void FBRSkinDriver::Apply() const
{
	UPoseableMeshComponent* S = Skin.Get();
	if (!S || !S->GetSkinnedAsset())
	{
		return;
	}
	const FQuat SpaceInv = S->GetComponentQuat().Inverse();
	for (const FLink& L : Links)
	{
		const USceneComponent* P = L.Pivot.Get();
		if (!P)
		{
			continue;
		}
		// Rotation du pivot dans le repere du maillage (hierarchie des pivots comprise), appliquee a l'os au repos ;
		// la position de l'os vient de son parent, deja mis a jour (les liens sont tries parents d'abord)
		const FQuat Delta = SpaceInv * P->GetComponentQuat();
		FTransform T = S->GetBoneTransformByName(L.Bone, EBoneSpaces::ComponentSpace);
		T.SetRotation(Delta * L.RestRot);
		S->SetBoneTransformByName(L.Bone, T, EBoneSpaces::ComponentSpace);
	}
}

FBRHumanoidSpec FBRHumanoidSpec::WretchSK()
{
	// Articulations : celles du squelette (Tools/Blender/build_creatures.py, RawAssets/Skeletal/skeletal_models.json)
	FBRHumanoidSpec S = FromJoints(FVector(3.f, 0.f, 95.f), FVector(24.f, 0.f, 117.f), FVector(17.f, -16.5f, 114.f), FVector(18.f, -20.5f, 84.f),
		FVector(17.f, 16.5f, 114.f), FVector(18.f, 20.5f, 84.f), FVector(0.f, -8.5f, 82.f), FVector(8.f, -10.f, 47.f), FVector(0.f, 8.5f, 82.f),
		FVector(8.f, 10.f, 47.f));
	S.ArmAmp = 12.f;
	S.LegAmp = 22.f;
	return S;
}

namespace BRRig
{
	UPoseableMeshComponent* AddSkin(AActor* Owner, USceneComponent* Root, USkeletalMesh* Mesh, const TMap<FString, FLinearColor>* Tints,
		TArray<TObjectPtr<USceneComponent>>& OutComponents, bool bCastShadow)
	{
		if (!Owner || !Root || !Mesh)
		{
			return nullptr;
		}
		UPoseableMeshComponent* Skin = NewObject<UPoseableMeshComponent>(Owner);
		Skin->SetupAttachment(Root);
		Skin->SetSkinnedAssetAndUpdate(Mesh);
		Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skin->SetCastShadow(bCastShadow);
		Skin->RegisterComponent();
		OutComponents.Add(Skin);
		if (UBRAssets* A = UBRAssets::Get(Owner))
		{
			A->ApplySlots(Skin, Tints);
		}
		return Skin;
	}

	FBRHumanoidParts BuildSkinnedHumanoid(AActor* Owner, USceneComponent* Root, USkeletalMesh* Mesh, const TMap<FString, FLinearColor>* Tints,
		TArray<TObjectPtr<USceneComponent>>& OutComponents, FBRSkinDriver& OutDriver, bool bCastShadow)
	{
		FBRHumanoidParts P;
		UPoseableMeshComponent* Skin = AddSkin(Owner, Root, Mesh, Tints, OutComponents, bCastShadow);
		if (!Skin)
		{
			return P;
		}
		P.Meshes.Add(Skin);
		OutDriver.Skin = Skin;
		OutDriver.Links.Reset();
		P.Torso = OutDriver.AddPivot(Owner, Root, TEXT("Spine"), OutComponents);
		USceneComponent* Upper = P.Torso ? P.Torso : Root;
		P.Head = OutDriver.AddPivot(Owner, Upper, TEXT("Head"), OutComponents);
		const TCHAR* Arm[2] = { TEXT("LeftArm"), TEXT("RightArm") };
		const TCHAR* Fore[2] = { TEXT("LeftForeArm"), TEXT("RightForeArm") };
		const TCHAR* UpLeg[2] = { TEXT("LeftUpLeg"), TEXT("RightUpLeg") };
		const TCHAR* Leg[2] = { TEXT("LeftLeg"), TEXT("RightLeg") };
		for (int32 i = 0; i < 2; ++i)
		{
			P.UpperArm[i] = OutDriver.AddPivot(Owner, Upper, Arm[i], OutComponents);
			P.LowerArm[i] = P.UpperArm[i] ? OutDriver.AddPivot(Owner, P.UpperArm[i], Fore[i], OutComponents) : nullptr;
			P.Thigh[i] = OutDriver.AddPivot(Owner, Root, UpLeg[i], OutComponents);
			P.Shin[i] = P.Thigh[i] ? OutDriver.AddPivot(Owner, P.Thigh[i], Leg[i], OutComponents) : nullptr;
		}
		return P;
	}

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

	bool HasSkeletalMesh(const UObject* WorldContext, FName MeshName)
	{
		UBRAssets* A = UBRAssets::Get(WorldContext);
		return A && A->SkeletalMesh(MeshName) != nullptr;
	}

	bool HasHazmat(const UObject* WorldContext)
	{
		UBRAssets* A = UBRAssets::Get(WorldContext);
		return A && A->Mesh(TEXT("SM_Hazmat_Torso")) != nullptr;
	}
}
