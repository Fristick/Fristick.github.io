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
// v4.16 : inscriptions gardees dans le repere de leur support (le battant d'une porte peut pivoter) et posees a sa pose
// actuelle ; boite visee et corps bloquant des portes du passage ignores par les rayons, le battant etant represente par
// ses propres boites (BRLidarLogic.h : DoorScanBox) : pas de faux mur de points dans une ouverture vide.
#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"
#include "BRLidarLogic.h"
#include "BRLidar.generated.h"

class UInstancedStaticMeshComponent;
class UPrimitiveComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UAudioComponent;
class ABRCharacter;
class ABRMissionDevice;
class ABREntity;
class ABRWorld;

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
	/** v4.16 : rayons arretes par un battant de porte (ses boites), passes d'inscription recommencees (support deplace),
	 *  rayons sur une inscription dont le support bougeait (non relevee) */
	int64 LeafHits = 0;
	int64 GlyphPoseRestarts = 0;
	int64 GlyphMovingSkips = 0;
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

	/** v4.16 (tests) : un point demande par ce scanner (position du glyphe, categorie, mecanisme touche ou inscrit, boite d'un
	 *  battant, composant touche, image) */
	struct FTestPoint
	{
		FVector At = FVector::ZeroVector;
		uint8 Category = 0;
		int32 Device = -1;
		bool bLeaf = false;
		TWeakObjectPtr<UPrimitiveComponent> Component;
		uint64 Frame = 0;
	};
	/** v4.16 (tests) : releve des points demandes, seulement quand un test le demande (MaxTestPoints au plus) */
	void SetTestRecord(bool bOn)
	{
		bTestRecord = bOn;
		TestPoints.Reset();
	}
	const TArray<FTestPoint>& GetTestPoints() const { return TestPoints; }
	static constexpr int32 MaxTestPoints = 200000;
	/** v4.16 (tests) : suivi du support d'une inscription (revision de pose, en mouvement) ; false si pas encore relevee */
	bool GetGlyphTrack(int32 DeviceIndex, uint32& OutRev, bool& bOutMoving) const;

	/** v4.16 : rayons du scanner (points, visee du faisceau maintenu et sa verification par l'hote) : la boite visee et le
	 *  corps bloquant des portes du passage sont ignores (Q) ; leurs battants arretent le rayon par leurs boites */
	static void AddScanIgnored(const ABRWorld* W, FCollisionQueryParams& Q);
	/** v4.16 : premier battant de porte du passage (affiche) touche par le rayon [From, From + Dir * MaxDist] (Dir unitaire) :
	 *  le mecanisme, la distance et la normale ; nul sinon */
	static ABRMissionDevice* TraceLeaves(const ABRWorld* W, const FVector& From, const FVector& Dir, float MaxDist, float& OutDist, FVector& OutNormal);

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

	/** v4.14 : forme des inscriptions, calculee une fois par cible et par chargement. v4.16 : dans le repere de son support
	 *  (battant de la porte, plaque murale, cage : ABRMissionDevice::GetLidarSupport), posee a la pose actuelle du support a
	 *  chaque emission (v4.15 : coordonnees du monde, redessinees a l'ancienne place apres l'ouverture de la porte) */
	struct FGlyphCache
	{
		TArray<FVector> Local;
		FVector LocalNormal = FVector::ForwardVector;
		bool bPlate = false;
		/** Pose du support suivie a chaque image : inscription non relevee pendant un mouvement, nouvelle passe ensuite */
		BRLidar::FSupportTrack Track;
	};
	TMap<int32, FGlyphCache> GlyphCache;

	/** v4.16 : battants des portes du passage vus a cette image (repere monde) ; leurs boites (BRLidar::DoorScanBox) sont les
	 *  surfaces du scanner, pas la boite visee ni le corps bloquant de la porte */
	struct FScanLeaf
	{
		BRLidar::FFrame3 Frame;
		TWeakObjectPtr<ABRMissionDevice> Device;
	};
	TArray<FScanLeaf> ScanLeaves;
	/** v4.16 : requete des rayons de l'image : le joueur, le scanner et les collisions d'interaction ignores */
	FCollisionQueryParams ScanQuery;
	/** v4.16 : requete, battants et supports des inscriptions, une fois par image avant les rayons */
	void PrepareScan(const FVector& Eye);
	/** v4.16 : point d'impact sur un mecanisme (decor du mecanisme ou battant) : losange, et cible a inscription au focus */
	void DeviceHit(ABRMissionDevice* D, const FVector& At, const FVector& N, bool bPulse, BRLidar::FHit& Out);

	/** v4.16 (tests) : releve, et contexte du point en cours (mecanisme, battant, composant) */
	bool bTestRecord = false;
	TArray<FTestPoint> TestPoints;
	int32 CtxDevice = -1;
	bool bCtxLeaf = false;
	TWeakObjectPtr<UPrimitiveComponent> CtxComponent;

	void BuildComponents();
	uint64 ComputeLoadKey() const;
	void AddPoint(uint8 Category, const FVector& At, const FVector& Normal, float Size, const FVector* Along = nullptr);
	FGlyphCache* GetGlyphCache(int32 DeviceIndex);
	void ApplyFrame(const BRLidar::FFrameResult& R);
	void UpdateFades(bool bForce);
	void UpdateAim(float Dt);
	/** Le joueur peut-il scanner (pas de menu, d'inventaire, de pause, de dialogue, d'entree verrouillee, de mort, de
	 *  transition) */
	bool CanScan() const;
};
