// Construction des humanoides articules (pieces de maillages + pivots aux articulations),
// partagee par les entites et par le corps du joueur (combinaison hazmat).
#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UPrimitiveComponent;
class UMaterialInstanceDynamic;

/**
 * Proportions d'un humanoide : positions absolues des articulations (cm, repere de l'acteur : +X avant,
 * +Y a droite, Z = 0 aux pieds). Index 0 = cote gauche (-Y), 1 = cote droit (+Y).
 */
struct FBRHumanoidSpec
{
	FVector Torso = FVector(0.f, 0.f, 92.f);
	FVector Head = FVector(0.f, 0.f, 151.f);
	FVector Shoulder[2] = { FVector(0.f, -19.f, 145.f), FVector(0.f, 19.f, 145.f) };
	FVector Elbow[2] = { FVector(0.f, -19.f, 115.f), FVector(0.f, 19.f, 115.f) };
	FVector Hip[2] = { FVector(0.f, -10.f, 92.f), FVector(0.f, 10.f, 92.f) };
	FVector Knee[2] = { FVector(0.f, -10.f, 46.f), FVector(0.f, 10.f, 46.f) };
	/** Pieces distinctes a gauche et a droite (*_UpperArmL / *_UpperArmR...) */
	bool bSidedMeshes = false;
	// Pose de repos et amplitude de la marche
	float ArmPitch = 4.f;
	float ElbowPitch = 10.f;
	float ArmRoll = 4.f;
	float ArmAmp = 22.f;
	float LegAmp = 28.f;

	/** Humanoide "procedural" (pieces generees par Tools/Blender/generate_models.py) */
	static FBRHumanoidSpec Simple(float HipZ, float ShoulderZ, float ShoulderW, float HipW, float Hunch, float UpperArmLen, float ThighLen, float Neck);
	/** Combinaison hazmat fournie (Tools/Blender/import_user_models.py, RawAssets/Meshes/user_models.json) */
	static FBRHumanoidSpec Hazmat();
};

/** Pivots crees pour un humanoide (index 0 = gauche, 1 = droite) */
struct FBRHumanoidParts
{
	USceneComponent* Torso = nullptr;
	USceneComponent* Head = nullptr;
	USceneComponent* UpperArm[2] = { nullptr, nullptr };
	USceneComponent* LowerArm[2] = { nullptr, nullptr };
	USceneComponent* Thigh[2] = { nullptr, nullptr };
	USceneComponent* Shin[2] = { nullptr, nullptr };
	TArray<UPrimitiveComponent*> Meshes;
};

namespace BRRig
{
	/**
	 * Ajoute une piece : un pivot (USceneComponent) place sur l'articulation + le maillage (ou une boite de repli).
	 * Les composants crees sont ajoutes a OutComponents, a conserver dans un UPROPERTY de l'acteur.
	 */
	USceneComponent* AddPart(AActor* Owner, USceneComponent* Parent, FName MeshName, const FVector& Joint, const FVector& FallbackSize,
		float FallbackDrop, const TMap<FString, FLinearColor>* Tints, TArray<TObjectPtr<USceneComponent>>& OutComponents,
		UPrimitiveComponent** OutMesh = nullptr, bool bCastShadow = true, bool bUniqueGlow = false, float GlowScale = 1.f,
		TArray<UMaterialInstanceDynamic*>* OutGlow = nullptr);

	FBRHumanoidParts BuildHumanoid(AActor* Owner, USceneComponent* Root, const TCHAR* Prefix, const FBRHumanoidSpec& Spec,
		const TMap<FString, FLinearColor>* Tints, TArray<TObjectPtr<USceneComponent>>& OutComponents, bool bCastShadow = true);

	/** true si les pieces de la vraie combinaison hazmat sont importees */
	bool HasHazmat(const UObject* WorldContext);
}
