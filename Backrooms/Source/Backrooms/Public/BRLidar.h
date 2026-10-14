// v4.13 : scanner LiDAR du Level 6 (local a chaque joueur). Les points ne sont pas repliques : chacun voit ses propres
// releves. La mission (mire calibree, cages relevees) passe par les actions ordinaires, verifiees par l'hote.
//
// Rendu : un composant d'instances par categorie et par groupe d'age (BRLidarLogic.h : FPointStore). Aucun acteur ni
// aucune lumiere par point, aucune ombre, pas d'eclairage indirect (Lumen) ni de lancer de rayons materiel : seul le
// materiau emissif du groupe change (attenuation commune).
//
// v4.14 : chaque image passe par BRLidar::RunFrame (la meme logique que le banc Tools/Lidar/test_lidar.cpp) : rayons par
// seconde independants des images par seconde, reserve par categorie ou le plus ancien est remplace, expiration de tous
// les groupes echus, inscriptions (plaques, cages) dessinees une fois par cible et par periode sous un budget commun.
// Le scanner est lie a l'identite du chargement (niveau, graine, numero de chargement, empreinte du plan) : un autre
// chargement efface ses points, ses releves et ses temporisations.
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

/** v4.14 : mesures du scanner (autotest, commande BRLidarStats) ; ce sont des compteurs du jeu, pas une mesure GPU */
struct FBRLidarStats
{
	int64 Frames = 0;
	int64 Rays = 0;
	int64 AimTraces = 0;
	int64 Requested = 0;
	int64 Accepted = 0;
	int64 Replaced = 0;
	int64 Expired = 0;
	int64 Rejected = 0;
	int64 GlyphPoints = 0;
	int64 GlyphTouches = 0;
	int64 GlyphDistinct = 0;
	int64 MidUpdates = 0;
	int32 PeakFramePoints = 0;
	int32 PeakFrameRays = 0;
	double TraceSeconds = 0.0;
	double GlyphSeconds = 0.0;
	double UploadSeconds = 0.0;
	double TotalSeconds = 0.0;
	FString ToString() const;
};

UCLASS()
class BACKROOMS_API ABRLidar : public AActor, public BRLidar::IScanHost
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
	/** Scan focalise (maintenu). v4.14 : arrete aussi le son, le maintien sur une cible et les envois en cours */
	void SetFocus(bool bOn);
	bool IsFocusing() const { return bFocus; }
	/** Recharge de l'impulsion (0 prete .. 1 vient d'etre tiree) */
	float GetPulseCharge() const { return State.Cooldown / BRLidar::PulseCooldown; }
	int32 GetPointCount() const { return State.Store.Total(); }
	/** Mecanisme vise par le scan focalise (releve en cours), ou nul */
	ABRMissionDevice* GetFocusDevice() const { return FocusDevice.Get(); }
	float GetFocusHold() const { return FocusHold; }
	/** v4.14 : Smiler tenu dans le faisceau (meme cible, sans interruption) et duree de ce maintien */
	ABREntity* GetHoldEntity() const { return HoldEntity.Get(); }
	float GetEntityHold() const { return EntityHold; }
	/** v4.14 : identite du chargement auquel appartiennent les points et les releves */
	uint64 GetLoadKey() const { return LoadKey; }
	/** v4.14 : efface points, releves, cibles et temporisations (autre chargement, autre plan) */
	void ResetForLoad();

	/** Reglages (enregistres dans la configuration du joueur) */
	static BRLidar::FSettings& Settings();
	static void LoadSettings();
	static void SaveSettings();

	/** Releves de plaques de portes faits par ce joueur dans ce chargement (rang de la porte -> silhouette, code), pour le
	 *  carnet. v4.14 : une plaque n'est relevee qu'une fois son inscription completee */
	const TMap<int32, FIntPoint>& GetScannedPlates() const { return ScannedPlates; }

	/** Mesures depuis la derniere remise a zero (tous les scanners de la machine) */
	static FBRLidarStats Stats;
	static void ResetStats() { Stats = FBRLidarStats(); }
	/** Compatibilite des tests v4.13 : rayons, points acceptes, temps */
	static int64 StatRaysCompat() { return Stats.Rays; }

	// BRLidar::IScanHost
	virtual BRLidar::FHit TraceRay(bool bPulse, bool bCenter, float X, float Y, float Z) override;
	virtual int EmitGlyphs(int Target, int First, int Count) override;
	virtual int PendingPoints(int Cat) const override { return (Cat >= 0 && Cat < BRLidar::NumCategories) ? Pending[Cat].Num() : 0; }

private:
	TWeakObjectPtr<ABRCharacter> Holder;

	/** [Categorie * AgeBuckets + Groupe] */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Ism;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;
	/** Derniere attenuation envoyee a chaque materiau (mise a jour seulement quand elle change) */
	TArray<float> SentFade;
	float SentContrast = -1.f;
	/** Le glyphe de la categorie est le plan du moteur (repli) : son axe normal est Z au lieu de X */
	bool bPlaneFallback[BRLidar::NumCategories] = {};
	/** Scanner tenu en main (vue a la premiere personne) */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandMesh;
	UPROPERTY()
	TObjectPtr<UAudioComponent> FocusAudio;

	BRLidar::FScanState State;
	uint64 LoadKey = 0;
	/** Reperes de l'image : impulsion (fige au tir) et vue courante (focus) */
	FVector PulseOrigin = FVector::ZeroVector;
	FQuat PulseRot = FQuat::Identity;
	FVector ViewOrigin = FVector::ZeroVector;
	FQuat ViewRot = FQuat::Identity;

	bool bFocus = false;
	TWeakObjectPtr<ABRMissionDevice> FocusDevice;
	float FocusHold = 0.f;
	float FocusSend = 0.f;
	/** v4.14 : maintien du faisceau sur UN Smiler (remis a zero si la cible change ou se perd) */
	TWeakObjectPtr<ABREntity> HoldEntity;
	float EntityHold = 0.f;
	/** Repere au sol deja montre dans le groupe courant des reperes (un chevron par repere et par groupe) */
	TArray<int32> MarkShownAt;

	/** Points en attente de cette image, par categorie */
	TArray<FTransform> Pending[BRLidar::NumCategories];
	TMap<int32, FIntPoint> ScannedPlates;

	/** v4.14 : coordonnees fixes des inscriptions, calculees une fois par cible et par chargement */
	struct FGlyphCache
	{
		TArray<FVector> Points;
		FVector Normal = FVector::ForwardVector;
		bool bPlate = false;
	};
	TMap<int32, FGlyphCache> GlyphCache;

	void BuildComponents();
	uint64 ComputeLoadKey() const;
	void AddPoint(uint8 Category, const FVector& At, const FVector& Normal, float Size, const FVector* Along = nullptr);
	const FGlyphCache* GetGlyphCache(int32 DeviceIndex);
	void ApplyFrame(const BRLidar::FFrameResult& R);
	void UpdateFades(bool bForce);
	void UpdateAim(float Dt);
	/** Le joueur peut-il scanner (pas de menu, d'inventaire, de pause, de dialogue, d'entree verrouillee, de mort, de
	 *  transition) */
	bool CanScan() const;
};
