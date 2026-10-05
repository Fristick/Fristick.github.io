// Le joueur (explorateur en combinaison hazmat) : vue a la premiere personne, inventaire,
// equipement (camescope, lampe torche, frontale, gilet), endurance, sante mentale.
// Multijoueur : chacun simule son propre personnage (deplacements acceptes par le serveur) ; les autres
// voient son corps, sa lampe et ce qu'il tient. Les attaques des entites (serveur) lui sont renvoyees.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BRTypes.h"
#include "BRRig.h"
#include "BRCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USpotLightComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UBRSaveGame;
class ABRExit;
class UPoseableMeshComponent;
class ABREntity;

UCLASS()
class BACKROOMS_API ABRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABRCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	/** Reception d'un saut : gerbe d'eau si on atterrit dans l'eau */
	virtual void Landed(const FHitResult& Hit) override;

	// ---- Entrees (appelees par le PlayerController) ----
	void InputMove(const FVector2D& Value);
	void InputLook(const FVector2D& DeltaDegrees);
	void InputJump(bool bPressed);
	void SetSprinting(bool bInSprint);
	void ToggleCrouch();
	void ToggleFlashlight();
	void ToggleNightVision();
	/** Vue a la premiere / troisieme personne (le corps en combinaison devient visible) */
	void ToggleThirdPerson();
	void Interact();
	/** Utilise l'objet de la poche 0..3 (touches 1-4) */
	void UsePocket(int32 Index);
	/** Utilise le premier objet de ce type (touches B = eau, R = piles) */
	void QuickUse(EBRItem Item);
	void SetInputLocked(bool bLocked) { bInputLocked = bLocked; }
	/** Touche Interagir maintenue (relever un coequipier a terre) */
	void SetInteractHeld(bool bHeld) { bInteractHeld = bHeld; }
	/** Reanimation d'un coequipier en cours (0..1) */
	float GetReviveProgress() const { return ReviveProgress; }
	/** Serveur : ce joueur, a terre, est releve par By */
	void ReviveBy(ABRCharacter* By);
	bool IsInputLocked() const { return bInputLocked; }

	/** v4.3 : echelle (sortie du Niveau 0...) : avancer / reculer pour monter / descendre, Saut ou Interagir pour lacher ;
	 *  en haut, dans le conduit au-dessus de la trappe, on noclippe vers le niveau suivant */
	void StartClimb(ABRExit* Ladder);
	void StopClimb();
	bool IsClimbing() const { return bClimbing; }

	/** v4.4 : jumpscare propre a chaque entite. Source : l'entite qui frappe (son modele se jette sur la camera) ;
	 *  sans Source (test du mode developpeur), elle apparait devant soi. bLethal : la mort attend la fin */
	void PlayJumpscare(EBREntityKind Kind, ABREntity* Source, bool bLethal);
	/** Jumpscare en cours (HUD) : type d'entite (-1 : aucun), temps ecoule, duree, instant ou elle atteint le visage */
	int32 GetScareKind() const { return ScareKind; }
	float GetScareTime() const { return ScareTime; }
	float GetScareDuration() const { return ScareDur; }
	float GetScareImpact() const { return ScareImpact; }

	/** v4.4, mode developpeur : vol libre a travers les murs (regard pour diriger, Saut pour monter, Course pour accelerer) */
	void SetDevFly(bool bFly);
	bool IsDevFlying() const { return bDevFly; }

	// ---- Etat ----
	float Health = 100.f;
	float Sanity = 100.f;
	float Stamina = 100.f;
	float Battery = 100.f;
	int32 NotesRead = 0;
	bool bGodMode = false;
	TArray<FString> ReadNotes;

	// ---- Inventaire ----
	static constexpr int32 NumPockets = 4;
	static constexpr int32 NumStorage = 20;
	TArray<FBRItemSlot> Pockets;
	TArray<FBRItemSlot> Storage;
	TArray<FBRItemSlot> Equipment;

	FBRItemSlot* GetSlot(EBRSlotGroup Group, int32 Index);
	int32 CountItem(EBRItem Item) const;
	/** Ajoute un objet (poches puis sac). Retourne le nombre NON ajoute (0 = tout est rentre). */
	int32 AddItem(EBRItem Item, int32 Count = 1);
	/** Deplace / empile / echange le contenu de deux cases (glisser-deposer) */
	bool MoveItem(EBRSlotGroup FromGroup, int32 FromIndex, EBRSlotGroup ToGroup, int32 ToIndex);
	/** Double-clic : consomme, equipe ou desequipe */
	void UseSlot(EBRSlotGroup Group, int32 Index);
	EBRItem GetEquipped(EBREquipSlot Slot) const;
	/** Camescope sur soi (poches, sac) : il ne se tient plus en main (v4.2), il filme et donne la vision nocturne */
	bool HasCamcorder() const { return CountItem(EBRItem::Camcorder) > 0; }
	bool HasLightSource() const;
	bool IsNightVision() const { return bNightVision; }
	void ResetInventory();

	bool IsDead() const { return bDead; }
	bool IsThirdPerson() const { return bThirdPerson; }
	bool IsSwimming() const { return bSwimming; }
	bool IsUnderwater() const { return bUnderwater; }
	/** Dans une cachette (placard, ou accroupi dans un trou du mur) : les entites ne le voient ni ne l'entendent */
	bool IsHidden() const { return bHidden; }
	/** Oxygene (0..100), consomme sous l'eau */
	float GetBreath() const { return Breath; }
	/** Profondeur d'eau aux pieds (cm) */
	float GetWaterDepth() const { return WaterDepth; }
	bool IsSprinting() const;
	bool IsFlashlightOn() const;
	float GetNoiseRadius() const;
	FVector GetEyeLocation() const;
	FVector GetViewDirection() const;
	/** Rotation de visee : celle du controleur, ou la visee repliquee pour le pion d'un autre joueur */
	FRotator GetAimRotation() const;
	const FString& GetKilledBy() const { return KilledBy; }
	float GetDeathTime() const { return DeathTime; }
	float GetDamageFlash() const { return DamageFlash; }
	float GetChaseLevel() const { return ChaseLevel; }
	/** Tendances (points / s) pour les fleches de la biometrie */
	float GetSanityTrend() const { return SanityTrend; }
	float GetHealthTrend() const { return HealthTrend; }
	float GetStaminaTrend() const { return StaminaTrend; }

	/** Attaque d'une entite (degats de sante et de sante mentale) */
	void ReceiveAttack(float Damage, float SanityDamage, AActor* Source, const FString& SourceName);
	/** Pression mentale continue (entite proche, obscurite...) en points par seconde */
	void AddSanityPressure(float PointsPerSecond) { SanityPressure += PointsPerSecond; }
	/** Une entite poursuit le joueur (musique de poursuite) */
	void NotifyChase(float Intensity) { ChaseTarget = FMath::Max(ChaseTarget, Intensity); }

	/** Ramassage d'un objet. false si l'inventaire est plein */
	bool ReceivePickup(EBRItem Item, const FString& Note);
	void OnEnteredLevel(const FBRLevelDef& Def);
	void ResetStats();

	// ---- Sauvegardes (v4.1)
	/** Ecrit l'inventaire, la sante et les notes lues dans la sauvegarde */
	void WriteToSave(UBRSaveGame* Save) const;
	/** Reprend l'etat enregistre (sans etat enregistre : equipement de depart, comme une nouvelle partie) */
	void ReadFromSave(const UBRSaveGame* Save);

	// ---- HUD ----
	/** Son d'interface (inventaire, clic) */
	void PlayUISound(FName Sound, float Volume = 0.6f) { PlaySound2D(Sound, Volume); }
	/** Flou de profondeur derriere le menu titre et la pause (0 = net, 1 = arriere-plan tres flou) */
	void ApplyMenuBlur(float Amount);
	const FString& GetFocusPrompt() const { return FocusPrompt; }
	bool IsReadingNote() const { return bReadingNote; }
	const FString& GetOpenNote() const { return OpenNote; }
	void CloseNote() { bReadingNote = false; }

protected:
	/** Perche de camera : longueur 0 a la premiere personne, recul a la troisieme */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpotLightComponent> Flashlight;

	/** Objet tenu en main (vue a la premiere personne) */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> HandMesh;

	/** Eclairage infrarouge de la vision nocturne */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> InfraredLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> HeartAudio;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> BreathAudio;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> ChaseAudio;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> HandGlow;

	/** Corps du joueur : la combinaison hazmat fournie, articulee (ombre a la 1re personne, visible a la 3e) */
	UPROPERTY()
	TObjectPtr<USceneComponent> BodyRoot;

	UPROPERTY()
	TObjectPtr<USceneComponent> BodyFeet;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> BodyComponents;

	/** Objet tenu, visible a la 3e personne */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HeldMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> UnderwaterAudio;

	// ---- Multijoueur : ce que les autres joueurs voient de ce personnage
	/** bit 0 lampe allumee, bit 1 course, bit 2 nage */
	UPROPERTY(Replicated)
	uint8 NetFlags = 0;

	/** Objet tenu en main (EBRItem) */
	UPROPERTY(Replicated)
	uint8 NetHand = 0;

	/** Lampe : 0 en main, 1 a la ceinture, 2 frontale */
	UPROPERTY(Replicated)
	uint8 NetLamp = 1;

	UFUNCTION(Server, Reliable)
	void ServerSetState(uint8 Flags, uint8 Hand, uint8 Lamp);

	UFUNCTION(Server, Reliable)
	void ServerSetDead(bool bInDead);

	/** Une entite (simulee par le serveur) frappe ce joueur : les degats sont appliques chez lui */
	UFUNCTION(Client, Reliable)
	void ClientReceiveAttack(float Damage, float SanityDamage, AActor* Source, const FString& SourceName);

	UFUNCTION()
	void OnRep_Dead();

	/** Relever un coequipier a terre (verifie par le serveur) */
	UFUNCTION(Server, Reliable)
	void ServerRevive(ABRCharacter* Mate);

	UFUNCTION(Client, Reliable)
	void ClientRevived(const FString& ByName);

private:
	/** Pion d'un autre joueur : corps, lampe, pas, remous */
	void TickRemote(float Dt);
	/** Envoie au serveur ce que les autres doivent voir (lampe, course, nage, objet en main) */
	void SyncNetState();
	/** Entites proches : pression mentale, musique de poursuite, journal (calcule chez chaque joueur) */
	void UpdateEntityEffects();
	/** Coequipier a terre devant soi, a portee de main */
	ABRCharacter* FindDownedTeammate() const;
	void UpdateRevive(float Dt);
	void Revived(const FString& ByName);
	void ApplyLamp(uint8 Lamp);
	void SetHeldVisual(EBRItem InHand);
	uint8 LampSlot() const;
	bool ShowsBody() const { return bThirdPerson || bRemoteView; }
	FName StepSoundName() const;
	void UpdateStats(float Dt);
	void UpdateCamera(float Dt);
	void UpdateFocus();
	void UpdateFlashlight(float Dt);
	void UpdateAudio(float Dt);
	void UpdatePostProcess(float Dt);
	void PlayFootstep();
	void PlaySound2D(FName Sound, float Volume = 1.f);
	void Die(const FString& By, AActor* Killer);
	void SetupLoopAudio(UAudioComponent* Comp, FName Sound);
	void OnEquipmentChanged();
	void BuildBody();
	void AnimateBody(float Dt);
	void UpdateWater(float Dt);
	void UpdateClimb(float Dt);
	void UpdateHiding();
	void UpdateViewMode();
	void StartSwimming();
	void StopSwimming();
	void ClimbOutOfWater();
	bool IsNearPoolEdge() const;
	bool UseItemEffect(EBRItem Item);
	bool StoreItem(EBRItem Item);

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	bool bInputLocked = true;
	/** Echelle en cours d'ascension */
	TWeakObjectPtr<ABRExit> ClimbLadder;
	bool bClimbing = false;
	bool bDevFly = false;
	float ClimbInput = 0.f;
	float ClimbStepAcc = 0.f;
	/** Dechirure de la realite en haut du conduit (aberration chromatique), 0..1 */
	float ClimbGlitch = 0.f;

	// ---- v4.4 : jumpscare en cours (BRJumpscare.cpp) ----
	void UpdateJumpscare(float Dt);
	void EndJumpscare();
	TWeakObjectPtr<ABREntity> ScareEntity;
	TWeakObjectPtr<AActor> ScareKiller;
	FString ScareKillerName;
	int32 ScareKind = -1;
	float ScareTime = 0.f;
	float ScareDur = 0.f;
	float ScareImpact = 0.f;
	float ScareCooldown = 0.f;
	bool bScareLethal = false;
	bool bScareSpawned = false;
	FVector ScareStart = FVector::ZeroVector;
	FRotator ScareView = FRotator::ZeroRotator;
	float ScareFOV = 0.f;
	float ScareFringe = 0.f;
	float ScareDark = 0.f;
	float ScareFlash = 0.f;
	FLinearColor ScareTint = FLinearColor::White;
	/** Pion vu de l'exterieur (autre joueur) : corps visible, pas de camera ni d'interface */
	bool bRemoteView = false;
	float RemoteStepTimer = 0.f;
	bool bInteractHeld = false;
	float ReviveProgress = 0.f;
	uint8 AppliedLamp = 255;
	bool bFlashlightOn = false;
	bool bNightVision = false;
	bool bWantsSprint = false;
	bool bExhausted = false;
	bool bReadingNote = false;
	float CamZ = 74.f;
	float BobTime = 0.f;
	int32 LastStepPhase = 0;
	float DamageFlash = 0.f;
	float LastDamageTime = -100.f;
	float DeathTime = 0.f;
	float SanityPressure = 0.f;
	float ChaseLevel = 0.f;
	float ChaseTarget = 0.f;
	float FlashFlicker = 1.f;
	float SprintTime = 0.f;
	float TimeAlive = 0.f;
	float EnergyBoost = 0.f;
	float SanityTrend = 0.f;
	float HealthTrend = 0.f;
	float StaminaTrend = 0.f;
	float PrevSanity = 100.f;
	float PrevHealth = 100.f;
	float PrevStamina = 100.f;
	EBRItem HandVisual = EBRItem::Count;
	EBRItem HeldVisual = EBRItem::Count;
	FBRHumanoidParts Body;
	bool bHasBody = false;
	bool bThirdPerson = false;
	bool bSwimming = false;
	bool bUnderwater = false;
	bool bJumpHeld = false;
	bool bDiving = false;
	float Breath = 100.f;
	float WaterDepth = 0.f;
	float WaterZ = -1.0e6f;
	float BodyAnim = 0.f;
	/** v4.4 : combinaison a squelette (SK_Hazmat), animee os par os ; nullptr : pieces rigides (repli) */
	UPROPERTY()
	TObjectPtr<UPoseableMeshComponent> BodySkin;
	/** Rotations de repos (espace du composant) des os animes */
	TMap<FName, FQuat> SkinRest;
	float ClimbBlend = 0.f;
	/** Applique la pose (memes angles que les pieces rigides) aux os de la combinaison */
	void PoseSkin(const FRotator* Thigh, const FRotator* Shin, const FRotator* Upper, const FRotator* Lower, const FRotator& Torso,
		const FRotator& Head);
	float CrouchBlend = 0.f;
	float SwimBlend = 0.f;
	float StrokeTimer = 0.f;
	float EdgePush = 0.f;
	float SplashCooldown = 0.f;
	float ClimbGrace = 0.f;
	/** Se hisser sur le rebord d'un bassin : monter le long de la paroi, puis avancer */
	bool bMantling = false;
	float MantleTime = 0.f;
	float MantleZ = 0.f;
	FVector MantleDir = FVector::ForwardVector;
	float AirBlend = 0.f;
	float UnderBlend = 0.f;
	float DeathBlend = 0.f;
	bool bSwimHint = false;
	bool bHidden = false;
	bool bHideHint = false;
	FVector FlashBase = FVector::ZeroVector;
	FVector SwimInput = FVector::ZeroVector;
	FVector2D LookLag = FVector2D::ZeroVector;
	EBRStep StepType = EBRStep::Carpet;
	FString FocusPrompt;
	TWeakObjectPtr<AActor> FocusActor;
	FString OpenNote;
	FString KilledBy;
	TWeakObjectPtr<AActor> KillerActor;
};
