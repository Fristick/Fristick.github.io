// Construction des humanoides articules (pieces de maillages + pivots aux articulations),
// partagee par les entites et par le corps du joueur (combinaison hazmat).
#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UPrimitiveComponent;
class UMaterialInstanceDynamic;
class UPoseableMeshComponent;
class USkeletalMesh;

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
	/** Modeles d'entites fournis (v3.5) : pieces SM_SkinStealerET_*, SM_FacelingET_*, SM_PartygoerET_* */
	static FBRHumanoidSpec SkinStealerET();
	static FBRHumanoidSpec FacelingET();
	static FBRHumanoidSpec PartygoerET();
	/** v4.5 : Wretch reconstruit (SK_Wretch) : posture voutee deja dans la pose de liaison, petits pas, bras ballants */
	static FBRHumanoidSpec WretchSK();
	/** Articulations en cm (repere Unreal), dans l'ordre de user_models.json ; pose deja naturelle (bras le long du corps) */
	static FBRHumanoidSpec FromJoints(const FVector& InTorso, const FVector& InHead, const FVector& ShoulderL, const FVector& ElbowL,
		const FVector& ShoulderR, const FVector& ElbowR, const FVector& HipL, const FVector& KneeL, const FVector& HipR, const FVector& KneeR);
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

/**
 * v4.5 : maillage a squelette (modele fourni, geometrie et poids d'origine) pilote par des pivots invisibles.
 * Les pivots sont places sur les os et animes exactement comme les pieces rigides (AnimateLimbs, tete, poses d'etat) ;
 * chaque image, la rotation de chaque pivot (repere du maillage) est recopiee sur son os : les articulations se plient
 * au lieu de se casser. Os principaux (Tools/Blender/build_entity_skeletal.py) : Spine, Head, LeftArm, LeftForeArm,
 * RightArm, RightForeArm, LeftUpLeg, LeftLeg, RightUpLeg, RightLeg ; Hound : Head, FrontUpperL, FrontLowerL...
 */
struct FBRSkinDriver
{
	struct FLink
	{
		FName Bone;
		int32 BoneIndex = INDEX_NONE;
		TWeakObjectPtr<USceneComponent> Pivot;
		/** Os au repos, repere du maillage */
		FQuat RestRot = FQuat::Identity;
		FVector RestPos = FVector::ZeroVector;
	};

	TWeakObjectPtr<UPoseableMeshComponent> Skin;
	/** Tries par indice d'os : un parent passe toujours avant ses enfants */
	TArray<FLink> Links;

	bool IsActive() const { return Skin.IsValid() && Links.Num() > 0; }
	/** Pivot place sur l'os (au repos), rattache a Parent (le parent du maillage ou un autre pivot du meme maillage).
	 *  nullptr si l'os n'existe pas dans ce squelette. */
	USceneComponent* AddPivot(AActor* Owner, USceneComponent* Parent, FName Bone, TArray<TObjectPtr<USceneComponent>>& OutComponents);
	/** Recopie la rotation des pivots sur les os (apres l'animation des pivots) */
	void Apply() const;
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

	/** v4.5 : maillage a squelette + pivots aux os principaux d'un humanoide (meme interface que BuildHumanoid).
	 *  Les pivots suivent la hierarchie des os : tete et bras sous le buste, avant-bras sous le bras, tibia sous la cuisse. */
	FBRHumanoidParts BuildSkinnedHumanoid(AActor* Owner, USceneComponent* Root, USkeletalMesh* Mesh, const TMap<FString, FLinearColor>* Tints,
		TArray<TObjectPtr<USceneComponent>>& OutComponents, FBRSkinDriver& OutDriver, bool bCastShadow = true);

	/** Maillage a squelette seul (materiaux du jeu appliques), rattache a Root */
	UPoseableMeshComponent* AddSkin(AActor* Owner, USceneComponent* Root, USkeletalMesh* Mesh, const TMap<FString, FLinearColor>* Tints,
		TArray<TObjectPtr<USceneComponent>>& OutComponents, bool bCastShadow = true);

	/** true si les pieces de la vraie combinaison hazmat sont importees */
	bool HasHazmat(const UObject* WorldContext);
	/** true si ce maillage est importe (modeles fournis optionnels) */
	bool HasMesh(const UObject* WorldContext, FName MeshName);
	/** v4.5 : true si ce maillage a squelette est importe */
	bool HasSkeletalMesh(const UObject* WorldContext, FName MeshName);
}
