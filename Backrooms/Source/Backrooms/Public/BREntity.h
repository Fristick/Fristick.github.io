// Entites du Backrooms Wiki : modeles Blender articules (bras, avant-bras, cuisses, tibias, tete),
// animation procedurale, IA a etats (inspiree de Backrooms : Escape Together) et chemins sur la grille.
// Multijoueur : l'IA ne tourne que sur le serveur (proie = joueur le plus proche) ; les clients recoivent
// l'espece, l'etat, la cible et l'intensite de la poursuite, et animent le modele a partir de la.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BRTypes.h"
#include "BRRig.h"
#include "BREntity.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class ABRCharacter;
class ABRWorld;

/** Fiche descriptive + parametres de comportement d'une espece */
struct FBREntityInfo
{
	FString Name;
	FString Number;
	FString Description;
	FString Advice;
	float HalfHeight = 90.f;
	float Radius = 30.f;
	bool bFlying = false;
	float HoverHeight = 150.f;
	bool bNeedsDark = false;
	float WalkSpeed = 150.f;
	float ChaseSpeed = 400.f;
	float SightRange = 2000.f;
	float AttackRange = 120.f;
	float Damage = 25.f;
	float SanityDamage = 10.f;
	float AttackCooldown = 1.5f;
	float Aura = 0.3f;          // perte de sante mentale / s a proximite
	float AuraRadius = 700.f;
	FName Voice = NAME_None;
	float VoiceInterval = 8.f;
	float VoiceFalloff = 2500.f;
};

UCLASS()
class BACKROOMS_API ABREntity : public ACharacter
{
	GENERATED_BODY()

public:
	ABREntity();

	static const FBREntityInfo& Info(EBREntityKind InKind);
	const FBREntityInfo& MyInfo() const { return Info(Kind); }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** A definir avant FinishSpawning */
	EBREntityKind Kind = EBREntityKind::Smiler;

	/** L'entite poursuit le joueur */
	bool IsHunting() const { return State == EState::Chase; }
	/** Fait disparaitre l'entite (fin d'une coupure de courant pour les Smilers) */
	void Dismiss() { StartVanish(); }
	/** Bacteria : 0..1, a quel point elle a repere le joueur (1 = poursuite) */
	float GetSuspicion() const { return Suspicion; }
	bool IsVanishing() const { return Vanish >= 0.f; }
	/** Tapie (Faceling dans les bles) : pas d'aura */
	bool IsLurking() const { return State == EState::Hide; }
	/** Assez visible pour ouvrir sa fiche du journal (pas un Skin-Stealer deguise ou en masse informe) */
	bool IsRecognizable() const { return Kind != EBREntityKind::SkinStealer || (Morph > 0.5f && !bDisguised); }
	/** Joueur poursuivi (nullptr si elle ne traque personne) et intensite de la poursuite (musique) */
	ABRCharacter* GetChaseTarget() const;
	float GetChaseIntensity() const { return NetChase / 255.f; }
	/** Taille de l'acteur (aussi chez les clients) */
	void SetVisualScale(float Scale);

	/** v4.4, jumpscare (chez le joueur qui le subit) : le modele est place a WorldTM, devant la camera ; l'IA se fige.
	 *  nullptr : fin, le modele reprend sa place */
	void SetScareTransform(const FTransform* WorldTM);
	bool IsScaring() const { return bScareOverride; }
	USceneComponent* GetVisual() const { return Visual; }

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Visual;

	/** Squelette articule (le Skin-Stealer peut le cacher sous sa forme de masse) */
	UPROPERTY()
	TObjectPtr<USceneComponent> BodyForm;

	/** Forme "au repos" du Skin-Stealer */
	UPROPERTY()
	TObjectPtr<USceneComponent> MassForm;

	/** Deguisement du Skin-Stealer : un explorateur en combinaison hazmat */
	UPROPERTY()
	TObjectPtr<USceneComponent> DisguiseForm;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> PartComponents;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> Voice;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;

	/** Emission de base de chaque materiau de GlowMIDs (yeux, dents, bords plus faibles) */
	TArray<FLinearColor> GlowBase;

	/** Ballon rouge du Partygoer (reste vertical quoi que fasse le bras) */
	UPROPERTY()
	TObjectPtr<USceneComponent> Balloon;

	// ---- Replique vers les clients
	UPROPERTY(Replicated)
	uint8 NetKind = 0;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	uint8 NetState = 0;

	/** Joueur observe / poursuivi */
	UPROPERTY(Replicated)
	TObjectPtr<ABRCharacter> Target;

	/** Intensite de la poursuite (0..255) */
	UPROPERTY(Replicated)
	uint8 NetChase = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Vanish)
	bool bNetVanish = false;

	UPROPERTY(Replicated)
	uint8 NetScale = 100;

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void OnRep_Vanish();

	/** Cri / bruit de l'entite entendu par tous les joueurs proches (signal d'alerte) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastVoiceCue(float Volume);

	/** v4.5 : l'entite vient de frapper (le serveur decide) : chacun joue le geste de frappe puis de recuperation */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastStrike();

private:
	enum class EState : uint8 { Idle, Wander, Stalk, Chase, Retreat, Frozen, Hide, Lure };
	enum class ELimb : uint8 { UpperArm, LowerArm, Thigh, Shin, HoundUpper, HoundLower, Wing, Tendril };

	struct FLimb
	{
		TWeakObjectPtr<USceneComponent> Pivot;
		ELimb Type = ELimb::Thigh;
		float Phase = 0.f;
		float Amp = 30.f;
		float Sign = 1.f;
		FRotator Base = FRotator::ZeroRotator;
	};

	/** Ce que l'entite percoit du joueur cette image */
	struct FSense
	{
		float Dist = 1e9f;
		bool bLOS = false;
		bool bLookedAt = false;
		bool bHeard = false;
	};

	// Construction du modele
	void BuildVisual();
	USceneComponent* AddPart(FName MeshName, USceneComponent* Parent, const FVector& Joint, const FVector& FallbackSize,
		float FallbackDrop, const TMap<FString, FLinearColor>* Tints, bool bUniqueGlow = false, float GlowScale = 1.f);
	/** Pieces rigides Prefix_* ; v4.5 : le maillage a squelette SkinName a la place s'il est importe */
	FBRHumanoidParts BuildHumanoid(const TCHAR* Prefix, const FBRHumanoidSpec& Spec, const TMap<FString, FLinearColor>* Tints, USceneComponent* Parent,
		const TCHAR* SkinName = nullptr);
	void BuildHound(const TMap<FString, FLinearColor>* Tints);
	bool BuildHoundModel();
	bool BuildClumpModel();
	bool BuildBacteriaModel();
	bool BuildMothModel();
	void AddLimb(USceneComponent* Pivot, ELimb Type, float Phase, float Amp, float Sign, const FRotator& Base);
	static FBRHumanoidSpec SpecFor(EBREntityKind InKind);

	// IA
	void Think(float Dt);
	/** Serveur : le joueur vivant le plus proche (un joueur cache compte moins, la proie actuelle compte plus) */
	ABRCharacter* PickTarget(ABRWorld* W) const;
	/** Client : pose deduite de l'etat replique (bras tendus, accroupi, forme du Skin-Stealer) */
	void UpdateClientState(float Dt);
	/** Sons d'entree dans un etat (alerte de poursuite), joues chez chacun */
	void OnStateEntered();
	void NotifyChase(float Intensity) { ChaseOut = FMath::Max(ChaseOut, Intensity); }
	void ThinkSmiler(ABRWorld* W, ABRCharacter* P, const FSense& S, float Dt);
	void ThinkHound(ABRCharacter* P, const FSense& S, float Dt);
	void ThinkFaceling(ABRCharacter* P, const FSense& S, float Dt);
	void ThinkSkinStealer(ABRCharacter* P, const FSense& S, float Dt);
	void ThinkPartygoer(ABRWorld* W, ABRCharacter* P, const FSense& S, float Dt);
	void ThinkBacteria(ABRWorld* W, ABRCharacter* P, const FSense& S, float Dt);
	void ThinkSimpleHunter(ABRCharacter* P, const FSense& S, float Dt, float ChaseIntensity, float GiveUpTime);
	void ThinkDeathmoth(ABRCharacter* P, const FSense& S, float Dt);

	void Animate(float Dt);
	void AnimateLimbs(float Dt, float Gait);
	/** v4.5 : poses d'etat (detection, poursuite, armee, frappe, recuperation), calculees chez chaque joueur */
	void UpdateStatePose(float Dt);
	/** Courbe de la frappe : 0 -> 1 en 0,12 s, tenue, puis retour (recuperation) jusqu'a 0,9 s */
	float StrikeCurve() const;
	/** v4.5 : Clump a squelette (SK_Clump) : bras d'appui poses au sol par IK, bras libres qui se tordent */
	bool BuildClumpSkin();
	void AnimateClumpSkin(float Dt);
	void UpdateHead(float Dt);
	void UpdateMorph(float Dt);
	void SetState(EState NewState);
	void MoveTowards(const FVector& Dest, float Speed);
	void FollowPathTo(const FVector& Goal, float Speed, float Dt);
	void Wander(float Speed, float Dt);
	void FacePlayer(float Dt);
	void SetOrientToMovement(bool bOrient);
	bool HasLineOfSight(const ABRCharacter* P) const;
	bool IsLookedAtBy(const ABRCharacter* P, float CosAngle) const;
	bool IsDirectPathClear(const FVector& Goal) const;
	void TryAttack(ABRCharacter* P, float Dist);
	void PlayVoice(float Volume = 1.f);
	void PlaySound2D(FName Sound, float Volume);
	void StartVanish();
	/** Rondes : prochain point de passage autour du joueur (parfois dans son champ de vision) */
	void PickPatrolGoal(ABRWorld* W, const ABRCharacter* P);

	EState State = EState::Wander;
	float StateTime = 0.f;
	float RepathTimer = 0.f;
	TArray<FIntPoint> Path;
	int32 PathIndex = 0;
	FIntPoint PathGoal = FIntPoint(MAX_int32, MAX_int32);
	float AttackTimer = 0.f;
	float VoiceTimer = 3.f;
	float StuckTimer = 0.f;
	float Agitation = 0.f;
	float Intimidation = 0.f;
	float LostSight = 0.f;
	float EyeContact = 0.f;
	float BeamTime = 0.f;
	float AnimTime = 0.f;
	float Life = 0.f;
	float Vanish = -1.f;
	float Morph = 1.f;          // Skin-Stealer : 0 = masse, 1 = forme humaine
	float MorphTarget = 1.f;
	float Reach = 0.f;          // bras tendus vers le joueur
	float HideCrouch = 0.f;         // Faceling cache dans les bles
	float TwitchTimer = 0.f;
	FRotator Twitch = FRotator::ZeroRotator;
	FRotator HeadRot = FRotator::ZeroRotator;
	FVector LastKnown = FVector::ZeroVector;
	FVector PatrolGoal = FVector::ZeroVector;
	bool bHasPatrolGoal = false;
	float PatrolAngle = 0.f;
	float PatrolDir = 1.f;
	float PatrolTime = 0.f;
	int32 PatrolLeg = 0;
	float Suspicion = 0.f;
	/** Bacteria : le signal sonore "elle vous a remarque" a deja ete joue */
	bool bSuspicionCue = false;
	/** Le joueur est cache (placard, trou) : pas de ligne de vue, pas de bruit */
	bool bTargetHidden = false;
	bool bHostileVariant = true;
	float ChaseOut = 0.f;
	bool bRegistered = false;
	bool bWantsMove = false;
	bool bWarned = false;
	TArray<FLimb> Limbs;
	/** v4.5 : maillages a squelette (corps, deguisement du Skin-Stealer) et leurs pivots */
	TArray<FBRSkinDriver> SkinDrivers;
	/** Bustes des maillages a squelette (la tete et les bras suivent : il se penche en poursuite) */
	TArray<TWeakObjectPtr<USceneComponent>> TorsoPivots;
	// Poses d'etat
	float AlertAnim = 0.f;
	float WindupAnim = 0.f;
	float StrikeTime = -1.f;
	float LeanAnim = 0.f;
	float Lurch = 0.f;
	/** Longueur de jambe (hanche -> sol, cm) : la foulee avance d'autant que le corps, sans glissement des pieds */
	float LegLength = 90.f;
	EState AnimState = EState::Wander;

	struct FClumpArm
	{
		TWeakObjectPtr<USceneComponent> Upper;
		TWeakObjectPtr<USceneComponent> Fore;
		TWeakObjectPtr<USceneComponent> Hand;
		FVector Shoulder = FVector::ZeroVector;   // repere du maillage, au repos
		FVector Elbow = FVector::ZeroVector;
		FVector Wrist = FVector::ZeroVector;
		bool bGround = false;
		bool bPlanted = false;
		FVector Planted = FVector::ZeroVector;   // monde
		FVector SwingFrom = FVector::ZeroVector;
		float Swing = -1.f;                      // 0..1 pendant un pas, -1 main posee
		float Phase = 0.f;
	};
	TArray<FClumpArm> ClumpArms;
	/** Animation des os espacee quand l'entite est loin ou hors de vue */
	float SkinAccum = 0.f;
	void ApplySkins(float Dt);
	TWeakObjectPtr<USceneComponent> HeadPivot;
	TWeakObjectPtr<USceneComponent> TrueHead;
	TWeakObjectPtr<USceneComponent> DisguiseHead;
	bool bDisguised = false;
	FVector VisualBase = FVector::ZeroVector;
	bool bScareOverride = false;
	FTransform ScareTM;
	FTransform ScareSavedRel;
	TWeakObjectPtr<ABRWorld> World;
};
