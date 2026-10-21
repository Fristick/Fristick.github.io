// Entrees (Enhanced Input cree entierement en C++), menu principal (solo / multijoueur / parametres), pause,
// inventaire (TAB), parametres, commandes console et demandes envoyees au serveur en multijoueur.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "BRTypes.h"
#include "BRPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ABRCharacter;
class SWidget;
class SEditableTextBox;
class UVOIPTalker;
class APlayerState;
class UAudioComponent;
class UBRSaveGame;

/** Pages du menu principal (Solo : choix du niveau parmi ceux deja explores de la partie choisie) */
enum class EBRMenuPage : uint8 { Main, Solo, Multi, Join, Saves, NewSave, Language };

UCLASS()
class BACKROOMS_API ABRPlayerController : public APlayerController
{
	GENERATED_BODY()
	/** v4.7 : le test automatique choisit la partie active et force les ecritures */
	friend class ABRAutoTest;

public:
	ABRPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	/** v4.16 : une repetition (IE_Repeat) d'une touche que l'entree du joueur ne tient pas pour enfoncee (touches purgees :
	 *  perte du focus de la fenetre, inventaire, dialogue, changement de touches) est ignoree. Unreal 5.8 la changeait en nouvel
	 *  appui (Input.AutoReconcilePressedEventsOnFirstRepeat) : au retour d'Alt+Tab, une touche toujours tenue relancait seule le
	 *  scan, la course ou l'interaction. Il faut maintenant la relacher puis l'appuyer de nouveau */
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

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

	// ---- Sauvegardes (v4.1)
	/** Partie choisie (en cours de jeu, ou selectionnee dans le menu) ; nullptr : aucune */
	UBRSaveGame* GetActiveSave() const { return ActiveSave; }
	/** Sauvegarde d'un emplacement (relue a l'ouverture de la page PARTIES) */
	UBRSaveGame* GetSaveInSlot(int32 Slot) const { return SaveSlots.IsValidIndex(Slot) ? SaveSlots[Slot].Get() : nullptr; }
	/** Page PARTIES : emplacement de l'element (sauvegarde), MenuItemNew, MenuItemBack */
	int32 GetMenuSaveSlot(int32 Item) const;
	static constexpr int32 MenuItemNew = -2;
	static constexpr int32 MenuItemBack = -3;
	bool CanCreateSave() const { return SaveOrder.Num() < 6; }
	/** Emplacements occupes, du plus recemment joue au plus ancien */
	const TArray<int32>& GetSaveOrder() const { return SaveOrder; }
	/** Seuls les niveaux deja explores dans la partie choisie peuvent etre choisis */
	bool IsLevelUnlocked(int32 LevelNumber) const;
	/** v4.4 : mode developpeur actif (v4.7 : toujours faux dans une version Shipping) */
	bool IsDevMode() const;
	/** v4.7 : les commandes de test sont permises sur cette machine (jamais en Shipping ; sinon mode developpeur de
	 *  l'hote ou test automatique). Sur le serveur, c'est le reglage de l'hote qui decide pour tous les clients. */
	static bool AreCheatsAllowed();
	/** Secondes restantes d'affichage de l'aide du mode developpeur */
	float GetDevHelpTime() const { return DevHelpTime; }
	/** Niveau visite en mode developpeur, pas ajoute a la partie */
	bool IsDevSession() const { return bDevSession; }
	/** v4.13 : nature de la session de cette manette : histoire (partie choisie, CONTINUER), rejouer (un niveau decouvert,
	 *  sans toucher a l'histoire) ou libre (sans partie, mode developpeur, tests). L'hote decide pour le groupe */
	EBRSessionMode GetSessionMode() const;
	/** v4.14 : cette session ecrit-elle la partie de campagne (histoire seulement ; REJOUER et le mode developpeur jouent
	 *  sur un etat temporaire, voir BRStory::WritePolicyFor) */
	bool WritesCampaign() const { return GetSessionMode() == EBRSessionMode::Story; }
	/** v4.13 : niveau ou CONTINUER reprend l'histoire de la partie choisie (BRStory::ResumePoint) */
	int32 GetContinueLevel() const;
	/** v4.13 : apres les Poolrooms, session gardee d'une ancienne partie pour ce niveau : elle devient la session a
	 *  reprendre (graine renvoyee ; 0 : aucune). v4.14 : elle reste dans la partie jusqu'a ConfirmParkedRestored */
	uint32 TakeParkedSession(int32 Level);
	/** v4.14 : la partie garde-t-elle une session garee pour ce niveau */
	bool HasParkedSession(int32 Level) const;
	/** v4.14 : le monde a repris la session garee de ce niveau (ou il n'y avait rien a reprendre) : elle est liberee */
	void ConfirmParkedRestored(int32 Level);
	/** Confirmation de suppression d'une partie (page PARTIES) */
	bool IsConfirmingDelete() const { return bConfirmDelete; }
	int32 GetDeleteSlot() const { return DeleteSlot; }
	/** Demande la suppression d'une partie (touche Suppr ou corbeille) */
	void RequestDeleteSave(int32 Slot);
	/** Le choix de partie et de niveau sert a heberger une partie en ligne (et non a jouer seul) */
	bool IsHostFlow() const { return bHostFlow; }
	/** v4.7 : la manette a servi en dernier (les aides affichent ses boutons) */
	bool IsUsingGamepad() const { return bPadActive; }
	/** v4.7 : joueur qui debute (moins de 15 minutes de jeu dans la partie, ou sans partie) : aides des premieres minutes */
	bool IsNewPlayer() const;
	/** Secondes depuis la derniere sauvegarde automatique (icone a l'ecran) */
	float GetTimeSinceSave() const { return TimeSinceSave; }
	/** Appele par le monde a chaque niveau charge : il devient explore dans la partie en cours */
	void OnLevelLoaded(int32 LevelNumber);
	/** Le personnage local vient de mourir (compteur de la sauvegarde) */
	void NotifyPlayerDeath();
	/** Ecrit la partie en cours (etat du joueur, niveau, journal ; v4.7 : session a reprendre).
	 *  v4.7 : ecriture sur un thread de fond ; bBlocking attend la fin (fermeture du jeu, retour au menu) */
	void WriteActiveSave(bool bBlocking = false);
	bool IsNetGame() const;

	// ---- Chat vocal de proximite
	/** Le micro transmet (voix ouverte, ou touche de parole enfoncee) */
	bool IsTransmittingVoice() const { return bTransmitting; }
	/** Niveau de la voix d'un coequipier (0 = silencieux) : icone au-dessus de sa tete */
	float GetTalkLevel(const APlayerState* Speaker) const;

	// ---- Multijoueur : demandes envoyees au serveur (le monde est tenu par l'hote)
	UFUNCTION(Server, Reliable)
	void ServerRequestTransition(int32 TargetLevel);

	/** Enregistrement termine (0 : pendant une coupure, 1 : une entite). v4.11 : l'hote verifie les conditions (coupure
	 *  noire depuis assez longtemps, entite visible et proche du joueur) avant de valider */
	UFUNCTION(Server, Reliable)
	void ServerCompleteObjective(uint8 Which);

	/** Commandes console d'un client executees par l'hote (1 coupure, 2 objectifs, 3 entite ; v4.15 : 4 etape du test
	 *  reseau atteinte par ce client, voir NetTestMark).
	 *  v4.7 : refusees par le serveur si AreCheatsAllowed() est faux chez lui. */
	UFUNCTION(Server, Reliable)
	void ServerCheat(uint8 Command, int32 Value);
	/** v4.15 (test reseau -BRNetTest, developpement) : derniere etape du plan signalee par ce client (copie de son controleur
	 *  chez l'hote) : l'hote s'en sert de barriere (changement de niveau, fin du test) */
	int32 NetTestMark = 0;
	/** v4.16 (tests) : repetitions de touches purgees ignorees (voir InputKey) */
	int32 IgnoredStaleRepeats = 0;

	bool IsInMenu() const { return bInMenu; }
	/** Le joueur vient d'etre place : la camera du menu repart de sa nouvelle orientation */
	void ResetMenuDrift() { bMenuDriftInit = false; }
	int32 GetMenuIndex() const { return MenuIndex; }
	bool IsInventoryOpen() const { return bInventory; }
	/** v4.14 : actions de jeu permises (ni menu, ni pause, ni inventaire, ni dialogue, ni saisie de touche) */
	bool CanPlayActions() const { return CanPlay(); }
	bool IsPauseMenuOpen() const { return bPauseMenu; }
	/** v4.16 (relecture) : cartes de la pause a la manette (croix, A) ou aux fleches (Entree) : carte du curseur (0 reprendre,
	 *  1 parametres, 2 touches, 3 menu principal, 4 quitter le jeu), montree une fois la manette ou une fleche utilisee ; la
	 *  souris la deplace. Avant : souris seulement */
	static constexpr int32 PauseItemCount = 5;
	int32 GetPauseCursor() const { return PauseCursor; }
	void SetPauseCursor(int32 Item);
	bool IsPauseCursorShown() const;
	void ActivatePauseItem(int32 Item);
	void SetInventoryOpen(bool bOpen, int32 Tab = INDEX_NONE);
	/** Onglet demande a l'ouverture de l'inventaire (lu une fois par le HUD) */
	int32 ConsumeRequestedTab();
	void TogglePause();
	void QuitToDesktop();
	/** v4.11 : fin de la campagne affichee (sauvegarde, curseur) ; choix du joueur : continuer l'exploration ou menu */
	void OnEndingShown();
	void CloseEnding(bool bContinue);

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
	/** v4.9 : categorie d'une ligne des parametres : 0 jeu, 1 video, 2 interface, 3 graphismes */
	int32 GetSettingCategory(int32 Index) const;
	/** v4.9 (tests) : ligne des parametres d'apres son nom interne ("HardwareRT", "UiScale"...), INDEX_NONE si inconnue */
	static int32 FindSettingRow(const FString& Name);
	/** v4.9 : pourquoi le ray tracing materiel est indisponible, selon la plateforme et le RHI demarre (texte localise) */
	static FString RayTracingUnavailableReason();
	/** v4.9 : mode d'affichage et resolution en vigueur (texte de la confirmation) */
	FString GetDisplaySummary() const;
	FString GetSettingLabel(int32 Index) const;
	FString GetSettingValue(int32 Index) const;
	FString GetSettingHint(int32 Index) const;
	/** v4.5 : mode de rendu reellement actif (RHI, ray tracing, Lumen, ombres, resolution interne) */
	FString GetRenderModeText(bool bShort = false) const;
	/** v4.15 : mode de rendu du rapport du test automatique : GetRenderModeText et l'etat de MegaLights */
	FString GetRenderModeReport() const;
	/** true si le ray tracing materiel est actif (decide au demarrage : DirectX 12, carte compatible, r.RayTracing) */
	static bool IsHardwareRayTracingAvailable();
	/** v4.15 : les maillages a squelette sont-ils visibles dans les reflets ray traces ? Seulement s'ils y sont eclaires par
	 *  les rayons (hit lighting, relance des impacts) : sinon ils y seraient noirs (pas de cache de surfaces) ; ils restent
	 *  dans la scene ray tracee pour les ombres (MegaLights) */
	static bool SkinnedMeshesInRayTracedReflections();
	/** v4.15 : applique cette regle aux maillages a squelette deja crees de ce monde ; retourne le nombre de changements */
	static int32 RefreshSkinnedReflections(UWorld* InWorld);
	/** v4.15 : applique cette regle a un maillage a squelette avant son enregistrement */
	static void ApplySkinnedReflections(class USkinnedMeshComponent* Skin);
	/** Profils : 0 Performance, 1 Qualite, 2 Cinematique */
	static void ApplyGraphicsProfile(int32 Profile);
	/** v4.8 : choisit la langue (indice dans BRLoc::Languages()), l'applique tout de suite et l'enregistre ; propre a
	 *  cette machine (en coop, chacun la sienne : seuls des identifiants et des valeurs circulent) */
	void ChooseLanguage(int32 Index);
	/** v4.8 : resolution calculee avant l'agrandissement TSR (fenetre de jeu x pourcentage de rendu) */
	static FIntPoint InternalResolution();
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

	/** v4.6 : se placer au bord de la salle de fosses la plus proche ; sans salle dans ce niveau, charge le Niveau 0
	 *  avec la graine de demonstration puis s'y place (touche Suppr en mode developpeur) */
	UFUNCTION(Exec)
	void BRPits();

	/** v4.6 : recharge le niveau avec une graine choisie (meme calcul que -BRSeed=<n>) : BRSeed 4605 (hote seulement) */
	UFUNCTION(Exec)
	void BRSeed(int32 Number);

	/** v4.13 : porte de service du Level 0 : graine du niveau et de la partie, porte retenue (palier, sauvegardee ou
	 *  non), emplacements possibles par bande ; apercu 30 s des emplacements (vert : bande visee, jaune : bande elargie,
	 *  gris : secours seulement, rouge : porte retenue). Outil de developpement */
	UFUNCTION(Exec)
	void BRDoor();

	/** v4.14 : kit Blender du Level 0 : BRLevel0Kit (etat), BRLevel0Kit 0 (murs generes, boites), BRLevel0Kit 1 (modules du
	 *  kit) ; reconstruit les chunks de cette machine pour comparer. Outil de developpement */
	UFUNCTION(Exec)
	void BRLevel0Kit(int32 Mode = -1);

	/** v4.14 : compteurs du scanner LiDAR (rayons, points acceptes, remplaces, expires, refuses, inscriptions, temps) ;
	 *  BRLidarStats 1 les remet a zero */
	UFUNCTION(Exec)
	void BRLidarStats(int32 Reset = 0);

private:
	/** v4.7 : vrai si les commandes de test sont permises ici, sinon message et faux */
	bool CheatGate();

	/** Mode developpeur : raccourcis en jeu (Page prec./suiv., Debut, Fin, Inser, Suppr, F6, F7, F10) */
	void UpdateDevKeys();
	/** Niveau precedent / suivant dans l'ordre du wiki */
	void DevJumpLevel(int32 Delta);
	/** Les niveaux visites en mode developpeur (sans les avoir explores) ne sont pas ajoutes a la partie */
	bool bDevSession = false;
	/** v4.6 : BRPits a charge le Niveau 0 : placement au bord des fosses des que le niveau est pret */
	bool bPendingPitTeleport = false;
	float DevHelpTime = 0.f;
	int32 DevScareKind = 0;

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
	UPROPERTY()
	TObjectPtr<UInputAction> MenuDeleteAction;
	/** v4.13 : scanner LiDAR du Level 6 */
	UPROPERTY()
	TObjectPtr<UInputAction> LidarPulseAction;
	UPROPERTY()
	TObjectPtr<UInputAction> LidarFocusAction;

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
	/** v4.14 : arrete les actions maintenues (scan focalise, sprint) : interface ouverte, pause, saisie ou changement de
	 *  touche, perte de focus de la fenetre */
	void StopHeldActions();
	/** v4.10 : un dialogue modal est ouvert (confirmation de l'affichage) : actions de jeu, onglets et menus bloques ; le
	 *  monde continue (ni pause, ni invulnerabilite) */
	bool IsModalDialogOpen() const;

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
	void OnLidarPulse(const FInputActionValue& Value);
	void OnLidarFocusStarted(const FInputActionValue& Value);
	/** v4.17 (interface, C6) : LT / RT dans PARAMETRES (inventaire ouvert) : categorie precedente / suivante ; false sinon */
	bool PadSettingsTrigger();
	void OnLidarFocusCompleted(const FInputActionValue& Value);
	void UpdateVoice(float DeltaTime);
	void OnMenuDown(const FInputActionValue& Value);
	void SetMenuPage(EBRMenuPage Page);
	/** Seul : CONTINUER (histoire) ou REJOUER (bReplay : le niveau choisi dans le carrousel) */
	void StartSolo(bool bReplay = false);
	/** Heberger : meme choix, la carte est rechargee en serveur */
	void HostGame(bool bReplay = false);
	void JoinGame();
	void SpawnInFront(int32 Kind);
	/** Champ de saisie de l'adresse IP (Slate : respecte la disposition du clavier, AZERTY compris) */
	void ShowAddressBox(bool bShow);
	/** Menu titre et pause : musique, camera qui derive lentement, flou de profondeur sur le niveau */
	void UpdateMenuAmbience(float DeltaTime);
	/** Relit les emplacements de sauvegarde (page PARTIES) */
	void RefreshSaves();
	/** Choisit une partie existante : page du choix des niveaux, sur le dernier niveau atteint */
	void SelectSave(int32 Slot);
	/** Cree une partie et la lance au Niveau 0 */
	void StartNewSave();
	/** Applique la partie choisie au personnage et au journal (debut de partie) */
	/** bLevelChanging : un nouveau niveau va etre charge (l'histoire s'appliquera a son arrivee, pas au niveau actuel) */
	void ApplyActiveSave(bool bLevelChanging = false);
	/** v4.7 : partie fermee pendant une mort : la mort va a son terme (equipement de depart, niveau neuf) */
	void ResolvePendingDeath(UBRSaveGame* Save);
	/** v4.7 : messages du chargement des sauvegardes (copie de secours, fichier illisible) */
	/** v4.8 : echecs d'ecriture non acquittes : montres au joueur (emplacement, demande, raison), puis acquittes */
	void ShowSaveFailures();
	void ShowSaveLoadMessages();
	/** Champ du nom d'une nouvelle partie (Slate, comme le champ de l'adresse IP) */
	void ShowNameBox(bool bShow);
	void OnMenuDelete(const FInputActionValue& Value);
	/** Son d'interface (fonctionne aussi sans personnage) ; repli sur S_UIClick si le son n'est pas importe */
	void PlayMenuSound(FName Sound, float Volume);

	bool bInMenu = true;
	bool bInventory = false;
	/** v4.14 : la fenetre du jeu avait le focus a l'image precedente */
	bool bWindowActive = true;
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
	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SEditableTextBox> NameBox;
	FString NewSaveName;
	bool bPendingNewSave = false;

	// Sauvegardes
	UPROPERTY()
	TObjectPtr<UBRSaveGame> ActiveSave;
	UPROPERTY()
	TArray<TObjectPtr<UBRSaveGame>> SaveSlots;
	/** Emplacements occupes, du plus recemment joue au plus ancien */
	TArray<int32> SaveOrder;
	bool bHostFlow = false;
	bool bConfirmDelete = false;
	int32 DeleteSlot = INDEX_NONE;
	/** Hote d'une partie en ligne : la sauvegarde est appliquee quand son personnage est pret */
	bool bApplySaveOnSpawn = false;
	/** v4.15 : une mort en attente vient d'etre resolue (ResolvePendingDeath, par SelectSave avant CONTINUER) ; lu puis
	 *  remis a zero par StartSolo (parcours lineaire : le niveau est toujours recharge) */
	bool bPendingDeathResolved = false;
	float AutoSaveTimer = 60.f;
	float PendingSaveDelay = -1.f;
	float TimeSinceSave = 100.f;
	/** v4.7 : dernier point sur ou le joueur se tenait (point de reprise de la session) */
	bool bHasSafeSpot = false;
	FVector SafeSpot = FVector::ZeroVector;
	float SafeYaw = 0.f;
	float SafeSpotTimer = 0.f;
	/** v4.7 : derniere entree venue de la manette */
	bool bPadActive = false;
	/** v4.16 (relecture) : curseur des cartes de la pause, montre depuis un appui de la croix ou d'une fleche (bPauseKeyNav) */
	int32 PauseCursor = 0;
	bool bPauseKeyNav = false;
	void MovePauseCursor(int32 Step);

	float MenuBlur = 0.f;
	float MenuDrift = 0.f;
	float MenuBaseYaw = 0.f;
	float MenuBasePitch = 0.f;
	bool bMenuDriftInit = false;

	bool bTalkKeyHeld = false;
	/** v4.10 : etat du dialogue modal a l'image precedente (curseur et focus a remettre a sa fermeture) */
	bool bModalWasOpen = false;
	bool bNetIntroShown = false;
	bool bTransmitting = false;
	float TalkerTimer = 0.f;
	/** Une source de voix par coequipier, attachee a son personnage */
	TMap<TWeakObjectPtr<APlayerState>, TWeakObjectPtr<UVOIPTalker>> Talkers;
};
