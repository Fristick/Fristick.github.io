// v4.13 : scanner LiDAR du Level 6 (local a chaque joueur). Les points ne sont pas repliques : chacun voit ses propres
// releves. La mission (mire calibree, cages relevees) passe par les actions ordinaires, verifiees par l'hote.
//
// Rendu : un composant d'instances par categorie et par groupe d'age (BRLidarLogic.h : FRing). Aucun acteur ni aucune
// lumiere par point, aucune ombre, pas d'eclairage indirect (Lumen) ni de lancer de rayons materiel : seul le materiau
// emissif du groupe change (attenuation commune). Les rayons d'une impulsion sont repartis sur plusieurs images.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BRLidarLogic.h"
#include "BRLidar.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UAudioComponent;
class ABRCharacter;
class ABRMissionDevice;
class ABREntity;

UCLASS()
class BACKROOMS_API ABRLidar : public AActor
{
	GENERATED_BODY()

public:
	ABRLidar();

	/** Joueur local qui tient le scanner */
	void Init(ABRCharacter* InOwner);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Impulsion large ; false pendant la recharge */
	bool Pulse();
	/** Scan focalise (maintenu) */
	void SetFocus(bool bOn);
	bool IsFocusing() const { return bFocus; }
	/** Recharge de l'impulsion (0 prete .. 1 vient d'etre tiree) */
	float GetPulseCharge() const { return Cooldown / BRLidar::PulseCooldown; }
	int32 GetPointCount() const { return Ring.Total(); }
	/** Mecanisme vise par le scan focalise (releve en cours), ou nul */
	ABRMissionDevice* GetFocusDevice() const { return FocusDevice.Get(); }
	float GetFocusHold() const { return FocusHold; }

	/** Reglages (enregistres dans la configuration du joueur) */
	static BRLidar::FSettings& Settings();
	static void LoadSettings();
	static void SaveSettings();

	/** Releves de plaques de portes faits par ce joueur (rang de la porte -> silhouette, code), pour le carnet */
	const TMap<int32, FIntPoint>& GetScannedPlates() const { return ScannedPlates; }

	/** Mesures (autotest, profilage) : rayons traces et points ajoutes depuis la derniere remise a zero, temps passe */
	static int64 StatRays;
	static int64 StatPoints;
	static double StatSeconds;
	static void ResetStats()
	{
		StatRays = 0;
		StatPoints = 0;
		StatSeconds = 0.0;
	}

private:
	TWeakObjectPtr<ABRCharacter> Holder;

	/** [Categorie * AgeBuckets + Groupe] */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Ism;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;
	/** Le glyphe de la categorie est le plan du moteur (repli) : son axe normal est Z au lieu de X */
	bool bPlaneFallback[BRLidar::NumCategories] = {};
	/** Scanner tenu en main (vue a la premiere personne) */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandMesh;
	UPROPERTY()
	TObjectPtr<UAudioComponent> FocusAudio;

	BRLidar::FRing Ring;
	float Cooldown = 0.f;
	/** Impulsion en cours : prochain rayon (-1 aucune), repere fige au moment du tir */
	int32 PulseNext = -1;
	FVector PulseOrigin = FVector::ZeroVector;
	FQuat PulseRot = FQuat::Identity;
	float PulsePhase = 0.f;
	uint32 Frame = 0;

	bool bFocus = false;
	TWeakObjectPtr<ABRMissionDevice> FocusDevice;
	float FocusHold = 0.f;
	float FocusSend = 0.f;
	TWeakObjectPtr<ABREntity> FocusEntity;
	float EntityHold = 0.f;
	int32 GlyphCursor = 0;
	/** Repere au sol deja montre dans le groupe courant (un chevron par repere et par groupe) */
	TArray<int32> MarkShownAt;
	int32 BucketSerial = 0;

	/** Points en attente de cette image, par categorie */
	TArray<FTransform> Pending[BRLidar::NumCategories];
	TMap<int32, FIntPoint> ScannedPlates;

	void BuildComponents();
	void TraceRay(const FVector& From, const FVector& Dir, bool bFocused, bool bCenter);
	void AddPoint(uint8 Category, const FVector& At, const FVector& Normal, float Size, const FVector* Along = nullptr);
	void AddCageGlyphs(ABRMissionDevice* Dev, const FVector& Toward, int32 Count);
	void AddPlateGlyphs(ABRMissionDevice* Dev, int32 Count);
	void Flush();
	void UpdateFades();
	void UpdateFocusTargets(float Dt);
};
