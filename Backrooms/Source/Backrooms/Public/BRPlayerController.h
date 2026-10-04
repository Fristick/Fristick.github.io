// Entrees (Enhanced Input cree entierement en C++), menu principal (solo / multijoueur / parametres), pause,
// inventaire (TAB), parametres, commandes console et demandes envoyees au serveur en multijoueur.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "BRPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ABRCharacter;
class SWidget;
class SEditableTextBox;
class UVOIPTalker;
class APlayerState;
class UAudioComponent;

/** Pages du menu principal */
enum class EBRMenuPage : uint8 { Main, Solo, Multi, Join };

UCLASS()
class BACKROOMS_API ABRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABRPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// ---- Menu principal : SOLO / MULTIJOUEUR / PARAMETRES / QUITTER
	EBRMenuPage GetMenuPage() const { return MenuPage; }
	int32 GetMenuCursor() const { return MenuCursor; }
	int32 GetMenuItemCount() const;
	FString GetMenuItemLabel(int32 Item) const;
	void SetMenuCursor(int32 Item);
	/** Clic ou ENTREE sur un element de la page */
	void MenuActivate(int32 Item);
	void MenuBack();
	/** Niveau de depart (pages Solo et Multijoueur) */
	void MenuShiftLevel(int32 Direction);
	/** Message sous le menu (connexion en cours, erreur reseau...) */
	const FString& GetMenuStatus() const { return MenuStatus; }
	/** Adresse IP de cette machine (a donner aux amis) */
	const FString& GetLocalAddress() const { return LocalAddress; }
	/** Quitte la partie (et la session reseau) pour revenir au menu principal */
	void ReturnToMainMenu();
	bool IsNetGame() const;

	// ---- Chat vocal de proximite
	/** Le micro transmet (voix ouverte, ou touche de parole enfoncee) */
	bool IsTransmittingVoice() const { return bTransmitting; }
	/** Niveau de la voix d'un coequipier (0 = silencieux) : icone au-dessus de sa tete */
	float GetTalkLevel(const APlayerState* Speaker) const;

	// ---- Multijoueur : demandes envoyees au serveur (le monde est tenu par l'hote)
	UFUNCTION(Server, Reliable)
	void ServerRequestTransition(int32 TargetLevel);

	UFUNCTION(Server, Reliable)
	void ServerMarkCollected(uint64 Id);

	UFUNCTION(Server, Reliable)
	void ServerVHSCollected();

	UFUNCTION(Server, Reliable)
	void ServerCompleteObjective(uint8 Which);

	/** Commandes console d'un client executees par l'hote (1 coupure, 2 objectifs, 3 entite) */
	UFUNCTION(Server, Reliable)
	void ServerCheat(uint8 Command, int32 Value);

	bool IsInMenu() const { return bInMenu; }
	/** Le joueur vient d'etre place : la camera du menu repart de sa nouvelle orientation */
	void ResetMenuDrift() { bMenuDriftInit = false; }
	int32 GetMenuIndex() const { return MenuIndex; }
	bool IsInventoryOpen() const { return bInventory; }
	bool IsPauseMenuOpen() const { return bPauseMenu; }
	void SetInventoryOpen(bool bOpen, int32 Tab = INDEX_NONE);
	/** Onglet demande a l'ouverture de l'inventaire (lu une fois par le HUD) */
	int32 ConsumeRequestedTab();
	void TogglePause();
	void QuitToDesktop();

	// ---- Touches (onglet TOUCHES) ----
	void BeginKeyCapture(int32 Action, int32 Slot);
	void CancelKeyCapture();
	bool IsCapturingKey() const { return CaptureAction != INDEX_NONE; }
	int32 GetCaptureAction() const { return CaptureAction; }
	int32 GetCaptureSlot() const { return CaptureSlot; }
	void ClearKey(int32 Action, int32 Slot);
	void ResetKeys();
	/** Reconstruit le contexte Enhanced Input a partir des touches configurees */
	void RebuildMappings();

	// ---- Parametres (onglet PARAMETRES de l'inventaire) ----
	int32 GetSettingsCount() const;
	FString GetSettingLabel(int32 Index) const;
	FString GetSettingValue(int32 Index) const;
	FString GetSettingHint(int32 Index) const;
	void AdjustSetting(int32 Index, int32 Direction);
	void LoadSettings();
	void SaveSettings() const;
	void ApplySettings();

	// ---- Commandes console (touche ` ou \u00b2) ----
	/** Aller a un niveau : BRLevel 37 */
	UFUNCTION(Exec)
	void BRLevel(int32 Number);

	/** Invincibilite */
	UFUNCTION(Exec)
	void BRGod();

	/** Faire apparaitre une entite devant soi : BRSpawn 0..8 */
	UFUNCTION(Exec)
	void BRSpawn(int32 Kind);

	/** Sensibilite de la souris : BRSensitivity 1.5 */
	UFUNCTION(Exec)
	void BRSensitivity(float Value);

	UFUNCTION(Exec)
	void BRInvertY();

	/** Remplit l'inventaire (eau, bandages, piles, barres, equipement) et soigne */
	UFUNCTION(Exec)
	void BRGiveAll();

	/** Declenche une coupure de courant */
	UFUNCTION(Exec)
	void BRBlackout();

	/** Valide les objectifs du niveau (cassettes VHS, enregistrement) */
	UFUNCTION(Exec)
	void BRObjectives();

protected:
	/** Musique du menu titre (fondu a l'ouverture et au lancement de la partie) */
	UPROPERTY()
	TObjectPtr<UAudioComponent> MenuMusic;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;
	UPROPERTY()
	TObjectPtr<UInputAction> LookPadAction;
	UPROPERTY()
	TObjectPtr<UInputAction> JumpAction;
	UPROPERTY()
	TObjectPtr<UInputAction> SprintAction;
	UPROPERTY()
	TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY()
	TObjectPtr<UInputAction> FlashAction;
	UPROPERTY()
	TObjectPtr<UInputAction> InteractAction;
	UPROPERTY()
	TObjectPtr<UInputAction> DrinkAction;
	UPROPERTY()
	TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY()
	TObjectPtr<UInputAction> JournalAction;
	UPROPERTY()
	TObjectPtr<UInputAction> PauseAction;
	UPROPERTY()
	TObjectPtr<UInputAction> QuitAction;
	UPROPERTY()
	TObjectPtr<UInputAction> MenuPrevAction;
	UPROPERTY()
	TObjectPtr<UInputAction> MenuNextAction;
	UPROPERTY()
	TObjectPtr<UInputAction> MenuConfirmAction;
	UPROPERTY()
	TObjectPtr<UInputAction> NightVisionAction;
	UPROPERTY()
	TObjectPtr<UInputAction> BandageAction;
	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> PocketActions;
	UPROPERTY()
	TObjectPtr<UInputAction> ViewAction;
	UPROPERTY()
	TObjectPtr<UInputAction> MenuUpAction;
	UPROPERTY()
	TObjectPtr<UInputAction> MenuDownAction;
	UPROPERTY()
	TObjectPtr<UInputAction> TalkAction;

private:
	void EnsureInput();
	void AddMappingToPlayer();
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType Type, bool bWhenPaused = false);
	void MapKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bSwizzle = false, bool bNegate = false);
	UInputAction* ActionFor(int32 BRAction) const;
	void UpdateInputMode();
	void PollKeyCapture();
	ABRCharacter* GetBRCharacter() const;
	bool CanPlay() const;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnLookPad(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);
	void OnSprintStarted(const FInputActionValue& Value);
	void OnSprintCompleted(const FInputActionValue& Value);
	void OnCrouch(const FInputActionValue& Value);
	void OnFlash(const FInputActionValue& Value);
	void OnInteract(const FInputActionValue& Value);
	void OnDrink(const FInputActionValue& Value);
	void OnReload(const FInputActionValue& Value);
	void OnInventory(const FInputActionValue& Value);
	void OnNightVision(const FInputActionValue& Value);
	void OnBandage(const FInputActionValue& Value);
	void OnPocket1(const FInputActionValue& Value);
	void OnPocket2(const FInputActionValue& Value);
	void OnPocket3(const FInputActionValue& Value);
	void OnPocket4(const FInputActionValue& Value);
	void OnView(const FInputActionValue& Value);
	void UsePocket(int32 Index);
	void OnPause(const FInputActionValue& Value);
	void OnQuit(const FInputActionValue& Value);
	void OnMenuPrev(const FInputActionValue& Value);
	void OnMenuNext(const FInputActionValue& Value);
	void OnMenuConfirm(const FInputActionValue& Value);
	void OnMenuUp(const FInputActionValue& Value);
	void OnInteractCompleted(const FInputActionValue& Value);
	void OnTalkStarted(const FInputActionValue& Value);
	void OnTalkCompleted(const FInputActionValue& Value);
	void UpdateVoice(float DeltaTime);
	void OnMenuDown(const FInputActionValue& Value);
	void SetMenuPage(EBRMenuPage Page);
	void StartSolo();
	void HostGame();
	void JoinGame();
	void SpawnInFront(int32 Kind);
	/** Champ de saisie de l'adresse IP (Slate : respecte la disposition du clavier, AZERTY compris) */
	void ShowAddressBox(bool bShow);
	/** Menu titre et pause : musique, camera qui derive lentement, flou de profondeur sur le niveau */
	void UpdateMenuAmbience(float DeltaTime);
	/** Son d'interface (fonctionne aussi sans personnage) ; repli sur S_UIClick si le son n'est pas importe */
	void PlayMenuSound(FName Sound, float Volume);

	bool bInMenu = true;
	bool bInventory = false;
	bool bPauseMenu = false;
	bool bMappingAdded = false;
	int32 MenuIndex = 0;
	int32 RequestedTab = INDEX_NONE;
	int32 CaptureAction = INDEX_NONE;
	int32 CaptureSlot = 0;
	int32 MappingRevision = -1;
	double CaptureStart = 0.0;

	EBRMenuPage MenuPage = EBRMenuPage::Main;
	int32 MenuCursor = 0;
	FString MenuStatus;
	FString LocalAddress;
	FString JoinAddress;
	TSharedPtr<SWidget> AddressWidget;
	TSharedPtr<SEditableTextBox> AddressBox;

	float MenuBlur = 0.f;
	float MenuDrift = 0.f;
	float MenuBaseYaw = 0.f;
	float MenuBasePitch = 0.f;
	bool bMenuDriftInit = false;

	bool bTalkKeyHeld = false;
	bool bNetIntroShown = false;
	bool bTransmitting = false;
	float TalkerTimer = 0.f;
	/** Une source de voix par coequipier, attachee a son personnage */
	TMap<TWeakObjectPtr<APlayerState>, TWeakObjectPtr<UVOIPTalker>> Talkers;
};
