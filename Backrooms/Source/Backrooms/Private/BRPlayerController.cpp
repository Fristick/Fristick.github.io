#include "BRPlayerController.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRWorld.h"
#include "BRLevels.h"
#include "BRHUD.h"
#include "BREntity.h"
#include "BRKeys.h"
#include "BRConfig.h"
#include "BRAutoTest.h"
#include "BRSave.h"
#include "BRInteractables.h"
#include "EngineUtils.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/Engine.h"
#include "AudioDevice.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DynamicRHI.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "RenderUtils.h"
#include "RHI.h"
#include "UnrealClient.h"
#include "Misc/ConfigCacheIni.h"
#include "Net/VoiceConfig.h"
#include "BRAssets.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "SocketSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"

namespace
{
	const TCHAR* SettingsSection = TEXT("/Script/Backrooms.BRSettings");

	// Ordre d'affichage (deux colonnes) : controles, son, affichage, puis graphismes
	enum ESettingRow
	{
		Row_Sensitivity,
		Row_InvertY,
		Row_FOV,
		Row_HeadBob,
		Row_CameraShake,
		Row_Flashes,
		Row_MotionBlur,
		Row_Volume,
		Row_Voice,
		Row_Brightness,
		Row_WindowMode,
		Row_RenderScale,
		Row_VSync,
		Row_MaxFPS,
		Row_Profile,
		Row_Quality,
		Row_HardwareRT,
		Row_RTHitLighting,
		Row_RTShadows,
		Row_AreaLights,
		Row_FullCreatures,
		Row_VolumetricFog,
		Row_FilmGrain,
		Row_VHSEffect,
		Row_DevMode,
		Row_Count
	};

	const TCHAR* ProfileNames[] = { TEXT("PERFORMANCE"), TEXT("QUALIT\u00c9"), TEXT("CIN\u00c9MATIQUE"), TEXT("PERSONNALIS\u00c9"), TEXT("RTX FLUIDE") };
	/** v4.8 : ordre des profils proposes (le profil PERSONNALISE ne se choisit pas : il vient d'un reglage modifie) */
	const int32 ProfileCycle[] = { 0, 1, 4, 2 };
	const TCHAR* QualityNames[] = { TEXT("BAS"), TEXT("MOYEN"), TEXT("\u00c9LEV\u00c9"), TEXT("\u00c9PIQUE"), TEXT("CIN\u00c9MATIQUE") };
	const TCHAR* VoiceNames[] = { TEXT("VOIX OUVERTE"), TEXT("APPUYER POUR PARLER"), TEXT("MICRO COUP\u00c9") };
	const TCHAR* WindowNames[] = { TEXT("PLEIN \u00c9CRAN"), TEXT("FEN\u00caTR\u00c9 SANS BORDURE"), TEXT("FEN\u00caTR\u00c9") };
	const int32 FPSSteps[] = { 0, 30, 60, 90, 120, 144, 165, 240 };
	const int32 NumFPSSteps = UE_ARRAY_COUNT(FPSSteps);

	FString OnOff(bool b)
	{
		return b ? FString(TEXT("ACTIV\u00c9")) : FString(TEXT("D\u00c9SACTIV\u00c9"));
	}

	// Erreurs reseau : la connexion echoue ou se perd, le moteur recharge la carte et le menu les affiche
	FString GNetMessage;
	bool GNetHooks = false;

	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
	{
		switch (Type)
		{
		case ENetworkFailure::PendingConnectionFailure:
			GNetMessage = TEXT("Impossible de rejoindre la partie : v\u00e9rifiez l'adresse, le port 7777 (UDP) et le pare-feu de l'h\u00f4te.");
			break;
		case ENetworkFailure::ConnectionLost:
		case ENetworkFailure::ConnectionTimeout:
			GNetMessage = TEXT("Connexion perdue avec l'h\u00f4te.");
			break;
		case ENetworkFailure::NetDriverListenFailure:
			GNetMessage = TEXT("Impossible d'h\u00e9berger : le port 7777 est peut-\u00eatre d\u00e9j\u00e0 utilis\u00e9.");
			break;
		default:
			GNetMessage = TEXT("Erreur r\u00e9seau : ") + Error;
			break;
		}
	}

	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
	{
		GNetMessage = TEXT("Impossible de rejoindre la partie : ") + Error;
	}

	FString FindLocalAddress()
	{
		if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			bool bCanBindAll = false;
			const TSharedRef<FInternetAddr> Addr = Sockets->GetLocalHostAddr(*GLog, bCanBindAll);
			return Addr->ToString(false);
		}
		return FString();
	}
}

ABRPlayerController::ABRPlayerController()
{
	bShowMouseCursor = false;
}

void ABRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// En reseau on arrive directement dans la partie ; seul, le menu principal s'affiche
	bInMenu = !IsNetGame();
	if (!IsLocalController())
	{
		return; // copie serveur du controleur d'un autre joueur
	}
	if (!GNetHooks && GEngine)
	{
		GNetHooks = true;
		GEngine->OnNetworkFailure().AddStatic(&HandleNetworkFailure);
		GEngine->OnTravelFailure().AddStatic(&HandleTravelFailure);
	}
	if (bInMenu && !GNetMessage.IsEmpty())
	{
		MenuStatus = GNetMessage;
		GNetMessage.Empty();
	}
	BRConfig::Get().GetString(SettingsSection, TEXT("LastAddress"), JoinAddress);

	EnsureInput();
	AddMappingToPlayer();
	UpdateInputMode();

	LoadSettings();
	ApplySettings();

	if (const ABRWorld* W = ABRWorld::Get(this))
	{
		MenuIndex = BRLevels::IndexOf(W->StartLevel);
	}

	// Sauvegardes : liste des parties (sous-titre du menu), et pour l'hote d'une partie en ligne, la partie choisie
	// dans le menu suit le rechargement de la carte (elle est appliquee quand son personnage est pret)
	if (bInMenu)
	{
		RefreshSaves();
	}
	else if (HasAuthority() && BRSaves::ActiveSlot() != INDEX_NONE)
	{
		ActiveSave = BRSaves::Load(BRSaves::ActiveSlot());
		ResolvePendingDeath(ActiveSave);
		bApplySaveOnSpawn = ActiveSave != nullptr;
		if (!ActiveSave)
		{
			BRSaves::ActiveSlot() = INDEX_NONE;
		}
	}

	// Test multijoueur (-BRNetTest) cote client : l'hote lance le sien depuis le mode de jeu
	if (GetNetMode() == NM_Client && ABRAutoTest::IsNetTestRequested() && GetWorld())
	{
		bool bRunning = false;
		for (TActorIterator<ABRAutoTest> It(GetWorld()); It; ++It)
		{
			bRunning = true;
		}
		if (!bRunning)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<ABRAutoTest>(ABRAutoTest::StaticClass(), FTransform::Identity, Params);
		}
	}
}

void ABRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	WriteActiveSave(true); // fermeture du jeu ou de l'editeur en pleine partie : on attend la fin de l'ecriture
	ShowAddressBox(false);
	ShowNameBox(false);
	if (bTransmitting)
	{
		bTransmitting = false;
		StopTalking();
	}
	Super::EndPlay(EndPlayReason);
}

bool ABRPlayerController::IsNetGame() const
{
	return GetNetMode() != NM_Standalone;
}

UInputAction* ABRPlayerController::MakeAction(const TCHAR* Name, EInputActionValueType Type, bool bWhenPaused)
{
	UInputAction* A = NewObject<UInputAction>(this, FName(Name));
	A->ValueType = Type;
	A->bTriggerWhenPaused = bWhenPaused;
	return A;
}

void ABRPlayerController::MapKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate)
{
	if (!Context || !Action || !Key.IsValid())
	{
		return;
	}
	FEnhancedActionKeyMapping& M = Context->MapKey(Action, Key);
	if (bSwizzle)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		M.Modifiers.Add(Swizzle);
	}
	if (bNegate)
	{
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(this);
		M.Modifiers.Add(Negate);
	}
}

void ABRPlayerController::EnsureInput()
{
	if (MoveAction)
	{
		return;
	}
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	LookPadAction = MakeAction(TEXT("IA_LookPad"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	CrouchAction = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	FlashAction = MakeAction(TEXT("IA_Flashlight"), EInputActionValueType::Boolean);
	InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	DrinkAction = MakeAction(TEXT("IA_Drink"), EInputActionValueType::Boolean);
	ReloadAction = MakeAction(TEXT("IA_Reload"), EInputActionValueType::Boolean);
	JournalAction = MakeAction(TEXT("IA_Inventory"), EInputActionValueType::Boolean, true);
	PauseAction = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean, true);
	QuitAction = MakeAction(TEXT("IA_Quit"), EInputActionValueType::Boolean, true);
	MenuPrevAction = MakeAction(TEXT("IA_MenuPrev"), EInputActionValueType::Boolean);
	MenuNextAction = MakeAction(TEXT("IA_MenuNext"), EInputActionValueType::Boolean);
	MenuConfirmAction = MakeAction(TEXT("IA_MenuConfirm"), EInputActionValueType::Boolean);
	NightVisionAction = MakeAction(TEXT("IA_NightVision"), EInputActionValueType::Boolean);
	BandageAction = MakeAction(TEXT("IA_Bandage"), EInputActionValueType::Boolean);
	ViewAction = MakeAction(TEXT("IA_View"), EInputActionValueType::Boolean);
	MenuUpAction = MakeAction(TEXT("IA_MenuUp"), EInputActionValueType::Boolean);
	TalkAction = MakeAction(TEXT("IA_Talk"), EInputActionValueType::Boolean, true);
	MenuDownAction = MakeAction(TEXT("IA_MenuDown"), EInputActionValueType::Boolean);
	MenuDeleteAction = MakeAction(TEXT("IA_MenuDelete"), EInputActionValueType::Boolean);
	PocketActions.Reset();
	for (int32 i = 0; i < 4; ++i)
	{
		PocketActions.Add(MakeAction(*FString::Printf(TEXT("IA_Pocket%d"), i + 1), EInputActionValueType::Boolean));
	}
	BRKeys::Load();
	RebuildMappings();
}

UInputAction* ABRPlayerController::ActionFor(int32 BRAction) const
{
	switch (static_cast<EBRAction>(BRAction))
	{
	case EBRAction::Jump:
		return JumpAction;
	case EBRAction::Sprint:
		return SprintAction;
	case EBRAction::Crouch:
		return CrouchAction;
	case EBRAction::Interact:
		return InteractAction;
	case EBRAction::Flashlight:
		return FlashAction;
	case EBRAction::NightVision:
		return NightVisionAction;
	case EBRAction::Inventory:
		return JournalAction;
	case EBRAction::Pocket1:
		return PocketActions.IsValidIndex(0) ? PocketActions[0].Get() : nullptr;
	case EBRAction::Pocket2:
		return PocketActions.IsValidIndex(1) ? PocketActions[1].Get() : nullptr;
	case EBRAction::Pocket3:
		return PocketActions.IsValidIndex(2) ? PocketActions[2].Get() : nullptr;
	case EBRAction::Pocket4:
		return PocketActions.IsValidIndex(3) ? PocketActions[3].Get() : nullptr;
	case EBRAction::Drink:
		return DrinkAction;
	case EBRAction::Bandage:
		return BandageAction;
	case EBRAction::Battery:
		return ReloadAction;
	case EBRAction::ThirdPerson:
		return ViewAction;
	case EBRAction::Pause:
		return PauseAction;
	case EBRAction::PushToTalk:
		return TalkAction;
	default:
		return nullptr;
	}
}

void ABRPlayerController::RebuildMappings()
{
	if (!MoveAction)
	{
		return;
	}
	MappingRevision = BRKeys::Revision();
	UInputMappingContext* Ctx = NewObject<UInputMappingContext>(this);

	// Touches configurables
	for (int32 A = 0; A < BRKeys::NumActions(); ++A)
	{
		const EBRAction Act = static_cast<EBRAction>(A);
		for (int32 Slot = 0; Slot < BRKeys::SlotsPerAction; ++Slot)
		{
			const FKey K = BRKeys::GetKey(Act, Slot);
			switch (Act)
			{
			case EBRAction::MoveForward:
				MapKey(Ctx, MoveAction, K, true, false);
				break;
			case EBRAction::MoveBackward:
				MapKey(Ctx, MoveAction, K, true, true);
				break;
			case EBRAction::MoveRight:
				MapKey(Ctx, MoveAction, K, false, false);
				break;
			case EBRAction::MoveLeft:
				MapKey(Ctx, MoveAction, K, false, true);
				break;
			default:
				MapKey(Ctx, ActionFor(A), K);
				break;
			}
		}
	}

	// Souris, manette et menu titre (fixes)
	MapKey(Ctx, MoveAction, EKeys::Gamepad_Left2D);
	MapKey(Ctx, LookAction, EKeys::Mouse2D);
	MapKey(Ctx, LookPadAction, EKeys::Gamepad_Right2D);
	MapKey(Ctx, JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	MapKey(Ctx, SprintAction, EKeys::Gamepad_LeftThumbstick);
	MapKey(Ctx, CrouchAction, EKeys::Gamepad_FaceButton_Right);
	MapKey(Ctx, FlashAction, EKeys::Gamepad_FaceButton_Top);
	MapKey(Ctx, InteractAction, EKeys::Gamepad_FaceButton_Left);
	MapKey(Ctx, DrinkAction, EKeys::Gamepad_LeftShoulder);
	MapKey(Ctx, ReloadAction, EKeys::Gamepad_RightShoulder);
	MapKey(Ctx, JournalAction, EKeys::Gamepad_Special_Left);
	MapKey(Ctx, NightVisionAction, EKeys::Gamepad_DPad_Up);
	MapKey(Ctx, BandageAction, EKeys::Gamepad_DPad_Down);
	MapKey(Ctx, ViewAction, EKeys::Gamepad_RightThumbstick);
	MapKey(Ctx, PauseAction, EKeys::Gamepad_Special_Right);
	MapKey(Ctx, QuitAction, EKeys::End);
	MapKey(Ctx, MenuPrevAction, EKeys::Left);
	MapKey(Ctx, MenuPrevAction, EKeys::A);
	MapKey(Ctx, MenuPrevAction, EKeys::Q);
	MapKey(Ctx, MenuPrevAction, EKeys::Gamepad_DPad_Left);
	MapKey(Ctx, MenuNextAction, EKeys::Right);
	MapKey(Ctx, MenuNextAction, EKeys::D);
	MapKey(Ctx, MenuNextAction, EKeys::Gamepad_DPad_Right);
	MapKey(Ctx, MenuUpAction, EKeys::Up);
	MapKey(Ctx, MenuUpAction, EKeys::W);
	MapKey(Ctx, MenuUpAction, EKeys::Z);
	MapKey(Ctx, MenuUpAction, EKeys::Gamepad_DPad_Up);
	MapKey(Ctx, MenuDownAction, EKeys::Down);
	MapKey(Ctx, MenuDownAction, EKeys::S);
	MapKey(Ctx, MenuDownAction, EKeys::Gamepad_DPad_Down);
	MapKey(Ctx, MenuConfirmAction, EKeys::Enter);
	MapKey(Ctx, MenuConfirmAction, EKeys::SpaceBar);
	MapKey(Ctx, MenuConfirmAction, EKeys::Gamepad_FaceButton_Bottom);
	MapKey(Ctx, MenuDeleteAction, EKeys::Delete);
	MapKey(Ctx, MenuDeleteAction, EKeys::Gamepad_FaceButton_Top);

	// Remplace l'ancien contexte
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (Mapping && bMappingAdded)
			{
				Sub->RemoveMappingContext(Mapping);
			}
			Sub->AddMappingContext(Ctx, 0);
			bMappingAdded = true;
		}
	}
	Mapping = Ctx;
}

void ABRPlayerController::AddMappingToPlayer()
{
	if (bMappingAdded || !Mapping)
	{
		return;
	}
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Sub->AddMappingContext(Mapping, 0);
			bMappingAdded = true;
		}
	}
}

void ABRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	EnsureInput();
	AddMappingToPlayer();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		UE_LOG(LogBackrooms, Error, TEXT("Enhanced Input non actif : verifiez Config/DefaultInput.ini"));
		return;
	}
	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABRPlayerController::OnMove);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABRPlayerController::OnLook);
	EIC->BindAction(LookPadAction, ETriggerEvent::Triggered, this, &ABRPlayerController::OnLookPad);
	EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ABRPlayerController::OnJumpStarted);
	EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABRPlayerController::OnJumpCompleted);
	EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ABRPlayerController::OnSprintStarted);
	EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ABRPlayerController::OnSprintCompleted);
	EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABRPlayerController::OnCrouch);
	EIC->BindAction(FlashAction, ETriggerEvent::Started, this, &ABRPlayerController::OnFlash);
	EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ABRPlayerController::OnInteract);
	EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &ABRPlayerController::OnInteractCompleted);
	EIC->BindAction(TalkAction, ETriggerEvent::Started, this, &ABRPlayerController::OnTalkStarted);
	EIC->BindAction(TalkAction, ETriggerEvent::Completed, this, &ABRPlayerController::OnTalkCompleted);
	EIC->BindAction(DrinkAction, ETriggerEvent::Started, this, &ABRPlayerController::OnDrink);
	EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &ABRPlayerController::OnReload);
	EIC->BindAction(JournalAction, ETriggerEvent::Started, this, &ABRPlayerController::OnInventory);
	EIC->BindAction(NightVisionAction, ETriggerEvent::Started, this, &ABRPlayerController::OnNightVision);
	EIC->BindAction(BandageAction, ETriggerEvent::Started, this, &ABRPlayerController::OnBandage);
	EIC->BindAction(ViewAction, ETriggerEvent::Started, this, &ABRPlayerController::OnView);
	EIC->BindAction(PocketActions[0], ETriggerEvent::Started, this, &ABRPlayerController::OnPocket1);
	EIC->BindAction(PocketActions[1], ETriggerEvent::Started, this, &ABRPlayerController::OnPocket2);
	EIC->BindAction(PocketActions[2], ETriggerEvent::Started, this, &ABRPlayerController::OnPocket3);
	EIC->BindAction(PocketActions[3], ETriggerEvent::Started, this, &ABRPlayerController::OnPocket4);
	EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &ABRPlayerController::OnPause);
	EIC->BindAction(QuitAction, ETriggerEvent::Started, this, &ABRPlayerController::OnQuit);
	EIC->BindAction(MenuPrevAction, ETriggerEvent::Started, this, &ABRPlayerController::OnMenuPrev);
	EIC->BindAction(MenuNextAction, ETriggerEvent::Started, this, &ABRPlayerController::OnMenuNext);
	EIC->BindAction(MenuConfirmAction, ETriggerEvent::Started, this, &ABRPlayerController::OnMenuConfirm);
	EIC->BindAction(MenuUpAction, ETriggerEvent::Started, this, &ABRPlayerController::OnMenuUp);
	EIC->BindAction(MenuDownAction, ETriggerEvent::Started, this, &ABRPlayerController::OnMenuDown);
	EIC->BindAction(MenuDeleteAction, ETriggerEvent::Started, this, &ABRPlayerController::OnMenuDelete);
}

void ABRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (MappingRevision != BRKeys::Revision())
	{
		RebuildMappings();
	}
	PollKeyCapture();
	UpdateVoice(DeltaTime);
	UpdateMenuAmbience(DeltaTime);
	DevHelpTime = FMath::Max(0.f, DevHelpTime - DeltaTime);
	if (IsDevMode() && IsLocalController() && !bInMenu)
	{
		UpdateDevKeys();
	}
	if (bPendingPitTeleport && IsLocalController())
	{
		// v4.6 : BRPits a lance le Niveau 0 (graine de demonstration) : placement des que le niveau est construit
		const ABRWorld* PW = ABRWorld::Get(this);
		if (PW && PW->IsLevelReady() && !PW->IsTransitioning() && PW->GetLevelNumber() == 0 && PW->GetLevelTime() > 0.3f)
		{
			bPendingPitTeleport = false;
			BRPits();
		}
	}

	if (bPendingNewSave)
	{
		bPendingNewSave = false;
		StartNewSave();
	}

	// v4.7 : manette ou clavier-souris ? (les aides des premieres minutes montrent les bons boutons)
	if (IsLocalController())
	{
		const float Pad = FMath::Abs(GetInputAnalogKeyState(EKeys::Gamepad_LeftX)) + FMath::Abs(GetInputAnalogKeyState(EKeys::Gamepad_LeftY))
			+ FMath::Abs(GetInputAnalogKeyState(EKeys::Gamepad_RightX)) + FMath::Abs(GetInputAnalogKeyState(EKeys::Gamepad_RightY));
		if (Pad > 0.4f || IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom) || IsInputKeyDown(EKeys::Gamepad_FaceButton_Left))
		{
			bPadActive = true;
		}
		float MouseDX = 0.f;
		float MouseDY = 0.f;
		GetInputMouseDelta(MouseDX, MouseDY);
		if (FMath::Abs(MouseDX) + FMath::Abs(MouseDY) > 1.f)
		{
			bPadActive = false;
		}
	}

	// Sauvegarde automatique de la partie en cours : a chaque niveau, puis toutes les minutes
	if (IsLocalController())
	{
		TimeSinceSave += DeltaTime;
		ABRWorld* SaveWorld = ABRWorld::Get(this);
		if (bApplySaveOnSpawn && GetPawn() && SaveWorld && SaveWorld->IsLevelReady())
		{
			bApplySaveOnSpawn = false;
			ApplyActiveSave();
			OnLevelLoaded(SaveWorld->GetLevelNumber());
		}
		if (ActiveSave && !bInMenu && GetNetMode() != NM_Client)
		{
			if (!(bPauseMenu && !IsNetGame()))
			{
				ActiveSave->PlayTime += DeltaTime;
			}
			// v4.7 : point de reprise : le dernier endroit sur ou l'on se tenait (au sol, loin du vide et de l'eau profonde)
			SafeSpotTimer -= DeltaTime;
			if (SafeSpotTimer <= 0.f)
			{
				SafeSpotTimer = 0.5f;
				const ABRCharacter* SpotC = GetBRCharacter();
				if (SaveWorld && SaveWorld->IsSafeSaveSpot(SpotC))
				{
					bHasSafeSpot = true;
					SafeSpot = SpotC->GetActorLocation();
					SafeYaw = static_cast<float>(GetControlRotation().Yaw);
				}
			}
			// v4.7 : une ecriture de fond a echoue (disque plein, droits) : on le dit.
			// v4.8 : chaque echec est garde (emplacement, numero de demande) jusqu'a ce qu'il soit montre puis acquitte ; une
			// reussite suivante ne l'efface plus en silence
			ShowSaveFailures();
			if (PendingSaveDelay > 0.f)
			{
				PendingSaveDelay -= DeltaTime;
				if (PendingSaveDelay <= 0.f)
				{
					WriteActiveSave();
				}
			}
			AutoSaveTimer -= DeltaTime;
			if (AutoSaveTimer <= 0.f)
			{
				AutoSaveTimer = 60.f;
				WriteActiveSave();
			}
		}
	}

	// Arrivee dans une partie en ligne : rappel du role de l'hote et du chat vocal
	if (!bNetIntroShown && IsNetGame() && GetHUD() && GetPawn())
	{
		bNetIntroShown = true;
		if (HasAuthority())
		{
			ABRHUD::Notify(this, TEXT("Vous h\u00e9bergez la partie : votre PC fait tourner le monde et les entit\u00e9s pour tout le groupe. Gardez le jeu ouvert jusqu'\u00e0 la fin."),
				9.f, FLinearColor(1.f, 0.85f, 0.4f));
		}
		else
		{
			ABRHUD::Notify(this, TEXT("Connect\u00e9 \u00e0 la partie de l'h\u00f4te. Restez group\u00e9s : on s'entend mieux de pr\u00e8s."), 7.f,
				FLinearColor(0.75f, 1.f, 0.75f));
		}
		if (FBRSettings::Get().VoiceMode == 1)
		{
			ABRHUD::Notify(this, BRKeys::Expand(TEXT("Chat vocal de proximit\u00e9 : maintenez {PushToTalk} pour parler.")), 7.f, FLinearColor(0.75f, 0.9f, 1.f));
		}
	}
}

// =====================================================================================================================
// Chat vocal de proximite (VOIP du moteur : la voix de chacun sort de son personnage, etouffee par les murs)
// =====================================================================================================================

void ABRPlayerController::OnTalkStarted(const FInputActionValue& Value)
{
	bTalkKeyHeld = CaptureAction == INDEX_NONE;
}

void ABRPlayerController::OnTalkCompleted(const FInputActionValue& Value)
{
	bTalkKeyHeld = false;
}

void ABRPlayerController::UpdateVoice(float DeltaTime)
{
	const bool bNet = IsNetGame();
	const int32 Mode = FBRSettings::Get().VoiceMode;
	const bool bWant = bNet && !bInMenu && (Mode == 0 || (Mode == 1 && bTalkKeyHeld));
	if (bWant != bTransmitting)
	{
		bTransmitting = bWant;
		if (bWant)
		{
			StartTalking();
		}
		else
		{
			StopTalking();
		}
	}
	if (!bNet)
	{
		return;
	}
	TalkerTimer -= DeltaTime;
	if (TalkerTimer > 0.f)
	{
		return;
	}
	TalkerTimer = 0.5f;
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	UBRAssets* A = UBRAssets::Get(this);
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS || PS == PlayerState)
		{
			continue;
		}
		TWeakObjectPtr<UVOIPTalker>& Slot = Talkers.FindOrAdd(PS);
		if (!Slot.IsValid())
		{
			Slot = UVOIPTalker::CreateTalkerForPlayer(PS);
		}
		if (UVOIPTalker* Talker = Slot.Get())
		{
			APawn* SpeakerPawn = PS->GetPawn();
			Talker->Settings.ComponentToAttachTo = SpeakerPawn ? SpeakerPawn->GetRootComponent() : nullptr;
			Talker->Settings.AttenuationSettings = A ? A->VoiceAttenuation() : nullptr;
		}
	}
}

float ABRPlayerController::GetTalkLevel(const APlayerState* Speaker) const
{
	const TWeakObjectPtr<UVOIPTalker>* Slot = Talkers.Find(const_cast<APlayerState*>(Speaker));
	UVOIPTalker* Talker = Slot ? Slot->Get() : nullptr;
	return Talker ? Talker->GetVoiceLevel() : 0.f;
}

// =====================================================================================================================
// Reaffectation des touches
// =====================================================================================================================

void ABRPlayerController::BeginKeyCapture(int32 Action, int32 Slot)
{
	if (Action < 0 || Action >= BRKeys::NumActions() || Slot < 0 || Slot >= BRKeys::SlotsPerAction)
	{
		return;
	}
	CaptureAction = Action;
	CaptureSlot = Slot;
	CaptureStart = FPlatformTime::Seconds();
}

void ABRPlayerController::CancelKeyCapture()
{
	CaptureAction = INDEX_NONE;
}

void ABRPlayerController::ClearKey(int32 Action, int32 Slot)
{
	BRKeys::SetKey(static_cast<EBRAction>(Action), Slot, FKey());
	BRKeys::Save();
	CancelKeyCapture();
}

void ABRPlayerController::ResetKeys()
{
	BRKeys::ResetDefaults();
	BRKeys::Save();
	CancelKeyCapture();
	ABRHUD::Notify(this, TEXT("Touches par d\u00e9faut r\u00e9tablies."), 2.5f);
}

void ABRPlayerController::PollKeyCapture()
{
	if (CaptureAction == INDEX_NONE)
	{
		return;
	}
	// On ignore le clic qui a lance la capture
	if (FPlatformTime::Seconds() - CaptureStart < 0.15)
	{
		return;
	}
	static TArray<FKey> AllKeys;
	if (AllKeys.Num() == 0)
	{
		EKeys::GetAllKeys(AllKeys);
	}
	for (const FKey& K : AllKeys)
	{
		if (!WasInputKeyJustPressed(K))
		{
			continue;
		}
		if (K == EKeys::Escape)
		{
			CancelKeyCapture();
			return;
		}
		if (K == EKeys::BackSpace || K == EKeys::Delete)
		{
			ClearKey(CaptureAction, CaptureSlot);
			return;
		}
		if (!BRKeys::IsBindable(K))
		{
			continue;
		}
		FString Removed;
		BRKeys::SetKey(static_cast<EBRAction>(CaptureAction), CaptureSlot, K, &Removed);
		BRKeys::Save();
		if (!Removed.IsEmpty())
		{
			ABRHUD::Notify(this, FString::Printf(TEXT("%s retir\u00e9e de : %s"), *BRKeys::KeyName(K), *Removed), 3.f,
				FLinearColor(1.f, 0.8f, 0.4f));
		}
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->PlayUISound(TEXT("S_UIClick"));
		}
		CancelKeyCapture();
		return;
	}
}

ABRCharacter* ABRPlayerController::GetBRCharacter() const
{
	return Cast<ABRCharacter>(GetPawn());
}

bool ABRPlayerController::CanPlay() const
{
	return !bInMenu && !bPauseMenu && !bInventory && CaptureAction == INDEX_NONE;
}

void ABRPlayerController::UpdateInputMode()
{
	// Curseur visible dans le menu principal, l'inventaire et le menu pause (boutons cliquables)
	const bool bCursor = bInventory || bPauseMenu || bInMenu;
	bShowMouseCursor = bCursor;
	if (bCursor)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		SetInputMode(Mode);
	}
	else
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
	}
}

// =====================================================================================================================

void ABRPlayerController::OnMove(const FInputActionValue& Value)
{
	if (!CanPlay())
	{
		return;
	}
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->InputMove(Value.Get<FVector2D>());
	}
}

void ABRPlayerController::OnLook(const FInputActionValue& Value)
{
	if (!CanPlay())
	{
		return;
	}
	if (ABRCharacter* C = GetBRCharacter())
	{
		const FBRSettings& Set = FBRSettings::Get();
		const FVector2D V = Value.Get<FVector2D>();
		const float K = 2.5f * Set.Sensitivity;
		C->InputLook(FVector2D(V.X * K, V.Y * K * (Set.bInvertY ? -1.f : 1.f)));
	}
}

void ABRPlayerController::OnLookPad(const FInputActionValue& Value)
{
	if (!CanPlay())
	{
		return;
	}
	if (ABRCharacter* C = GetBRCharacter())
	{
		const FBRSettings& Set = FBRSettings::Get();
		const FVector2D V = Value.Get<FVector2D>();
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const float K = 140.f * Dt * Set.Sensitivity;
		C->InputLook(FVector2D(V.X * K, V.Y * K * (Set.bInvertY ? -1.f : 1.f)));
	}
}

void ABRPlayerController::OnJumpStarted(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->InputJump(true);
		}
	}
}

void ABRPlayerController::OnJumpCompleted(const FInputActionValue& Value)
{
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->InputJump(false);
	}
}

void ABRPlayerController::OnSprintStarted(const FInputActionValue& Value)
{
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->SetSprinting(CanPlay());
	}
}

void ABRPlayerController::OnSprintCompleted(const FInputActionValue& Value)
{
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->SetSprinting(false);
	}
}

void ABRPlayerController::OnCrouch(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->ToggleCrouch();
		}
	}
}

void ABRPlayerController::OnFlash(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->ToggleFlashlight();
		}
	}
}

void ABRPlayerController::OnInteract(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->Interact();
			C->SetInteractHeld(true); // maintenir : relever un coequipier
		}
	}
}

void ABRPlayerController::OnInteractCompleted(const FInputActionValue& Value)
{
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->SetInteractHeld(false);
	}
}

void ABRPlayerController::OnDrink(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->QuickUse(EBRItem::AlmondWater);
		}
	}
}

void ABRPlayerController::OnReload(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->QuickUse(EBRItem::Battery);
		}
	}
}

void ABRPlayerController::SetInventoryOpen(bool bOpen, int32 Tab)
{
	if (Tab != INDEX_NONE)
	{
		RequestedTab = Tab;
	}
	if (bOpen == bInventory)
	{
		return;
	}
	bInventory = bOpen;
	CancelKeyCapture();
	if (bInMenu)
	{
		ShowAddressBox(!bOpen && MenuPage == EBRMenuPage::Join); // le champ IP ne doit pas recouvrir les parametres
	}
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->SetSprinting(false);
		C->PlayUISound(TEXT("S_Inventory"));
	}
	// Curseur visible pour glisser-deposer les objets ; le monde continue de vivre (comme dans Escape Together)
	UpdateInputMode();
	if (bOpen)
	{
		int32 SX = 0;
		int32 SY = 0;
		GetViewportSize(SX, SY);
		SetMouseLocation(SX / 2, SY / 2);
	}
}

int32 ABRPlayerController::ConsumeRequestedTab()
{
	const int32 T = RequestedTab;
	RequestedTab = INDEX_NONE;
	return T;
}

void ABRPlayerController::TogglePause()
{
	if (bInMenu)
	{
		return;
	}
	bPauseMenu = !bPauseMenu;
	if (!bPauseMenu)
	{
		bInventory = false;
		CancelKeyCapture();
	}
	else
	{
		WriteActiveSave();
	}
	// En multijoueur le monde continue de tourner pour les autres
	if (!IsNetGame())
	{
		SetPause(bPauseMenu);
	}
	UpdateInputMode();
}

void ABRPlayerController::QuitToDesktop()
{
	WriteActiveSave(true);
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ABRPlayerController::OnInventory(const FInputActionValue& Value)
{
	if (CaptureAction != INDEX_NONE)
	{
		return; // la touche est en train d'etre reaffectee
	}
	if (bInMenu || bPauseMenu)
	{
		// Depuis le menu titre ou la pause : acces direct aux parametres et aux touches
		SetInventoryOpen(!bInventory, bInventory ? INDEX_NONE : 2);
		return;
	}
	if (ABRCharacter* C = GetBRCharacter())
	{
		if (C->IsReadingNote())
		{
			C->CloseNote();
		}
	}
	SetInventoryOpen(!bInventory);
}

void ABRPlayerController::OnNightVision(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->ToggleNightVision();
		}
	}
}

void ABRPlayerController::OnBandage(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->QuickUse(EBRItem::Bandage);
		}
	}
}

void ABRPlayerController::UsePocket(int32 Index)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->UsePocket(Index);
		}
	}
}

void ABRPlayerController::OnPocket1(const FInputActionValue& Value)
{
	UsePocket(0);
}

void ABRPlayerController::OnPocket2(const FInputActionValue& Value)
{
	UsePocket(1);
}

void ABRPlayerController::OnPocket3(const FInputActionValue& Value)
{
	UsePocket(2);
}

void ABRPlayerController::OnPocket4(const FInputActionValue& Value)
{
	UsePocket(3);
}

void ABRPlayerController::OnPause(const FInputActionValue& Value)
{
	if (CaptureAction != INDEX_NONE)
	{
		return; // capture en cours : la touche est pour la reaffectation (Echap l'annule dans PollKeyCapture)
	}
	if (bInventory)
	{
		SetInventoryOpen(false); // Echap ferme d'abord l'inventaire
		return;
	}
	if (bInMenu)
	{
		MenuBack();
		return;
	}
	TogglePause();
}

void ABRPlayerController::OnView(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->ToggleThirdPerson();
		}
	}
}

void ABRPlayerController::OnQuit(const FInputActionValue& Value)
{
	if ((bPauseMenu || bInMenu) && CaptureAction == INDEX_NONE)
	{
		QuitToDesktop();
	}
}

void ABRPlayerController::OnMenuPrev(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory)
	{
		MenuShiftLevel(-1);
	}
}

void ABRPlayerController::OnMenuNext(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory)
	{
		MenuShiftLevel(1);
	}
}

void ABRPlayerController::OnMenuUp(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory)
	{
		SetMenuCursor((MenuCursor + GetMenuItemCount() - 1) % FMath::Max(1, GetMenuItemCount()));
	}
}

void ABRPlayerController::OnMenuDown(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory)
	{
		SetMenuCursor((MenuCursor + 1) % FMath::Max(1, GetMenuItemCount()));
	}
}

void ABRPlayerController::OnMenuConfirm(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory)
	{
		MenuActivate(MenuCursor);
	}
}

void ABRPlayerController::OnMenuDelete(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory && MenuPage == EBRMenuPage::Saves && !bConfirmDelete)
	{
		const int32 Slot = GetMenuSaveSlot(MenuCursor);
		if (Slot >= 0)
		{
			RequestDeleteSave(Slot);
		}
	}
}

// =====================================================================================================================
// Menu principal
// =====================================================================================================================

int32 ABRPlayerController::GetMenuItemCount() const
{
	switch (MenuPage)
	{
	case EBRMenuPage::Main:
		return 4;
	case EBRMenuPage::Multi:
		return 3;
	case EBRMenuPage::Saves:
		return bConfirmDelete ? 2 : SaveOrder.Num() + (CanCreateSave() ? 1 : 0) + 1;
	default:
		return 2;
	}
}

int32 ABRPlayerController::GetMenuSaveSlot(int32 Item) const
{
	if (Item >= 0 && Item < SaveOrder.Num())
	{
		return SaveOrder[Item];
	}
	return (CanCreateSave() && Item == SaveOrder.Num()) ? MenuItemNew : MenuItemBack;
}

bool ABRPlayerController::IsLevelUnlocked(int32 LevelNumber) const
{
	// Mode developpeur : tout est jouable. Sans partie choisie (tests, console), seul le Niveau 0 est propose
	if (IsDevMode())
	{
		return true;
	}
	return ActiveSave ? ActiveSave->IsExplored(LevelNumber) : LevelNumber == 0;
}

bool ABRPlayerController::IsNewPlayer() const
{
	return !ActiveSave || ActiveSave->PlayTime < 900.f;
}

bool ABRPlayerController::IsDevMode() const
{
#if UE_BUILD_SHIPPING
	return false; // v4.7 : le reglage DevMode du fichier ini n'ouvre rien dans une version publiee
#else
	return FBRSettings::Get().bDevMode;
#endif
}

bool ABRPlayerController::AreCheatsAllowed()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FBRSettings::Get().bDevMode || ABRAutoTest::IsRequested();
#endif
}

bool ABRPlayerController::CheatGate()
{
	// v4.7 : commandes de test de la console. Le serveur verifie de son cote (ServerCheat, ServerRequestTransition).
	if (AreCheatsAllowed())
	{
		return true;
	}
	ABRHUD::Notify(this, TEXT("Commande de test d\u00e9sactiv\u00e9e (mode d\u00e9veloppeur requis, absent des versions publi\u00e9es)."), 3.f);
	return false;
}

void ABRPlayerController::DevJumpLevel(int32 Delta)
{
	ABRWorld* W = ABRWorld::Get(this);
	const TArray<FBRLevelDef>& All = BRLevels::All();
	if (!W || All.Num() == 0)
	{
		return;
	}
	int32 Cur = 0;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		Cur = All[i].Number == W->GetLevelNumber() ? i : Cur;
	}
	const FBRLevelDef& Next = All[(Cur + Delta + All.Num()) % All.Num()];
	bDevSession = true;
	ABRHUD::Notify(this, FString::Printf(TEXT("MODE D\u00c9V : Niveau %d - %s"), Next.Number, *Next.Title), 3.f, FLinearColor(0.6f, 0.9f, 1.f));
	W->RequestTransition(Next.Number);
}

void ABRPlayerController::UpdateDevKeys()
{
	ABRWorld* W = ABRWorld::Get(this);
	ABRCharacter* C = GetBRCharacter();
	if (!W || !W->IsLevelReady() || W->IsTransitioning() || IsPaused() || bInventory)
	{
		return;
	}
	// (F1 a F5, F8 et F9 sont pris par Unreal hors version finale : modes d'affichage, ejection, capture)
	bool bUsed = true;
	if (WasInputKeyJustPressed(EKeys::PageUp))
	{
		DevJumpLevel(1);
	}
	else if (WasInputKeyJustPressed(EKeys::PageDown))
	{
		DevJumpLevel(-1);
	}
	else if (WasInputKeyJustPressed(EKeys::Home))
	{
		// Meme niveau, nouvelle graine : une autre disposition
		bDevSession = true;
		W->RequestTransition(W->GetLevelNumber());
	}
	else if (WasInputKeyJustPressed(EKeys::End))
	{
		BRObjectives();
		ABRHUD::Notify(this, TEXT("MODE D\u00c9V : objectifs remplis"), 2.f, FLinearColor(0.6f, 0.9f, 1.f));
	}
	else if (WasInputKeyJustPressed(EKeys::Insert))
	{
		BRBlackout();
	}
	else if (WasInputKeyJustPressed(EKeys::Delete))
	{
		BRPits();
	}
	else if (WasInputKeyJustPressed(EKeys::F6) && C)
	{
		C->SetDevFly(!C->IsDevFlying());
		ABRHUD::Notify(this, C->IsDevFlying() ? TEXT("MODE D\u00c9V : vol libre (\u00e0 travers les murs, Espace pour monter, Maj pour acc\u00e9l\u00e9rer)")
			: TEXT("MODE D\u00c9V : vol libre coup\u00e9"), 3.f, FLinearColor(0.6f, 0.9f, 1.f));
	}
	else if (WasInputKeyJustPressed(EKeys::F7))
	{
		BRGod();
	}
	else if (WasInputKeyJustPressed(EKeys::F10) && C)
	{
		const EBREntityKind Kind = static_cast<EBREntityKind>(DevScareKind % static_cast<int32>(EBREntityKind::Count));
		++DevScareKind;
		C->PlayJumpscare(Kind, nullptr, false);
	}
	else
	{
		bUsed = false;
	}
	if (bUsed)
	{
		DevHelpTime = FMath::Max(DevHelpTime, 6.f);
	}
}

FString ABRPlayerController::GetMenuItemLabel(int32 Item) const
{
	if (Item < 0 || Item >= GetMenuItemCount())
	{
		return FString();
	}
	static const TCHAR* Main[] = { TEXT("SOLO"), TEXT("MULTIJOUEUR"), TEXT("PARAM\u00c8TRES"), TEXT("QUITTER") };
	static const TCHAR* Multi[] = { TEXT("H\u00c9BERGER UNE PARTIE"), TEXT("REJOINDRE UNE PARTIE"), TEXT("RETOUR") };
	switch (MenuPage)
	{
	case EBRMenuPage::Main:
		return Main[Item];
	case EBRMenuPage::Multi:
		return Multi[Item];
	case EBRMenuPage::Join:
		return Item == 0 ? TEXT("SE CONNECTER") : TEXT("RETOUR");
	case EBRMenuPage::NewSave:
		return Item == 0 ? (bHostFlow ? TEXT("H\u00c9BERGER") : TEXT("COMMENCER")) : TEXT("RETOUR");
	case EBRMenuPage::Solo:
	{
		if (Item == 1)
		{
			return TEXT("RETOUR");
		}
		const TArray<FBRLevelDef>& All = BRLevels::All();
		const int32 Level = All[FMath::Clamp(MenuIndex, 0, All.Num() - 1)].Number;
		if (!IsLevelUnlocked(Level))
		{
			return TEXT("VERROUILL\u00c9");
		}
		return bHostFlow ? TEXT("H\u00c9BERGER") : TEXT("NOCLIPPER");
	}
	case EBRMenuPage::Saves:
	{
		if (bConfirmDelete)
		{
			return Item == 0 ? TEXT("OUI, SUPPRIMER") : TEXT("ANNULER");
		}
		const int32 Slot = GetMenuSaveSlot(Item);
		if (Slot >= 0)
		{
			const UBRSaveGame* S = GetSaveInSlot(Slot);
			return S ? S->SaveName : FString();
		}
		return Slot == MenuItemNew ? TEXT("NOUVELLE PARTIE") : TEXT("RETOUR");
	}
	}
	return FString();
}

void ABRPlayerController::SetMenuCursor(int32 Item)
{
	if (Item != MenuCursor && Item >= 0 && Item < GetMenuItemCount())
	{
		MenuCursor = Item;
		PlayMenuSound(TEXT("S_UIHover"), 0.35f);
	}
}

void ABRPlayerController::PlayMenuSound(FName Sound, float Volume)
{
	UBRAssets* A = UBRAssets::Get(this);
	USoundBase* S = A ? A->Sound(Sound) : nullptr;
	if (!S && A)
	{
		S = A->Sound(TEXT("S_UIClick"));
	}
	if (S)
	{
		UGameplayStatics::PlaySound2D(this, S, Volume);
	}
}

void ABRPlayerController::UpdateMenuAmbience(float DeltaTime)
{
	if (!IsLocalController())
	{
		return;
	}
	// Musique : seulement sur l'ecran titre ; fondu de sortie au lancement de la partie
	if (bInMenu && !MenuMusic)
	{
		UBRAssets* A = UBRAssets::Get(this);
		if (USoundBase* Theme = A ? A->Sound(TEXT("S_MenuTheme")) : nullptr)
		{
			MenuMusic = UGameplayStatics::SpawnSound2D(this, Theme, 0.55f);
			if (MenuMusic)
			{
				MenuMusic->FadeIn(2.5f, 1.f);
			}
		}
	}
	else if (!bInMenu && MenuMusic)
	{
		MenuMusic->FadeOut(2.f, 0.f); // le composant se detruit a la fin du fondu
		MenuMusic = nullptr;
	}

	// Camera du menu titre : le personnage regarde lentement autour de lui (le niveau vit derriere le menu)
	if (bInMenu && GetPawn())
	{
		if (!bMenuDriftInit)
		{
			bMenuDriftInit = true;
			MenuBaseYaw = GetControlRotation().Yaw;
			MenuBasePitch = -4.f;
		}
		MenuDrift += DeltaTime;
		const float Yaw = MenuBaseYaw + 26.f * FMath::Sin(MenuDrift * 0.045f);
		const float Pitch = MenuBasePitch + 2.5f * FMath::Sin(MenuDrift * 0.11f + 0.7f);
		SetControlRotation(FRotator(Pitch, Yaw, 0.f));
	}
	else
	{
		bMenuDriftInit = false;
	}

	// Flou de profondeur derriere le menu titre et la pause (ce controleur continue de tourner pendant la pause)
	const float Want = (bInMenu || bPauseMenu) ? 1.f : 0.f;
	if (MenuBlur != Want || Want > 0.f)
	{
		MenuBlur = FMath::FInterpConstantTo(MenuBlur, Want, FMath::Min(DeltaTime, 0.1f), 2.5f);
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->ApplyMenuBlur(MenuBlur);
		}
	}
}

void ABRPlayerController::SetMenuPage(EBRMenuPage Page)
{
	MenuPage = Page;
	MenuCursor = 0;
	bConfirmDelete = false;
	if (Page == EBRMenuPage::Multi && LocalAddress.IsEmpty())
	{
		LocalAddress = FindLocalAddress();
	}
	if (Page == EBRMenuPage::Saves)
	{
		RefreshSaves();
		// Curseur sur la partie choisie juste avant (retour depuis le choix du niveau)
		const int32 Idx = SaveOrder.IndexOfByKey(BRSaves::ActiveSlot());
		MenuCursor = Idx != INDEX_NONE ? Idx : 0;
	}
	if (Page == EBRMenuPage::NewSave)
	{
		const int32 Free = BRSaves::FreeSlot();
		NewSaveName = FString::Printf(TEXT("Partie %d"), Free == INDEX_NONE ? 1 : Free + 1);
	}
	ShowAddressBox(Page == EBRMenuPage::Join);
	ShowNameBox(Page == EBRMenuPage::NewSave);
}

void ABRPlayerController::MenuShiftLevel(int32 Direction)
{
	if (MenuPage != EBRMenuPage::Solo)
	{
		return;
	}
	const int32 Num = BRLevels::All().Num();
	MenuIndex = (MenuIndex + (Direction >= 0 ? 1 : Num - 1)) % Num;
	PlayMenuSound(TEXT("S_UIHover"), 0.45f);
}

void ABRPlayerController::MenuBack()
{
	if (bConfirmDelete)
	{
		bConfirmDelete = false;
		const int32 Idx = SaveOrder.IndexOfByKey(DeleteSlot);
		MenuCursor = Idx != INDEX_NONE ? Idx : 0;
		return;
	}
	switch (MenuPage)
	{
	case EBRMenuPage::Join:
		SetMenuPage(EBRMenuPage::Multi);
		break;
	case EBRMenuPage::Solo:
	case EBRMenuPage::NewSave:
		SetMenuPage(EBRMenuPage::Saves);
		break;
	case EBRMenuPage::Saves:
		SetMenuPage(bHostFlow ? EBRMenuPage::Multi : EBRMenuPage::Main);
		break;
	case EBRMenuPage::Multi:
		SetMenuPage(EBRMenuPage::Main);
		break;
	default:
		break;
	}
}

void ABRPlayerController::MenuActivate(int32 Item)
{
	if (!bInMenu || Item < 0 || Item >= GetMenuItemCount())
	{
		return;
	}
	MenuCursor = Item;
	// Niveau pas encore explore dans cette partie : on ne peut pas y aller depuis le menu
	if (MenuPage == EBRMenuPage::Solo && Item == 0)
	{
		const TArray<FBRLevelDef>& All = BRLevels::All();
		const FBRLevelDef& D = All[FMath::Clamp(MenuIndex, 0, All.Num() - 1)];
		if (!IsLevelUnlocked(D.Number))
		{
			PlayMenuSound(TEXT("S_UIDeny"), 0.6f);
			ABRHUD::Notify(this, FString::Printf(TEXT("Niveau %d : pas encore explor\u00e9 dans cette partie. Trouvez une sortie qui y m\u00e8ne."), D.Number), 4.f,
				FLinearColor(1.f, 0.6f, 0.45f));
			return;
		}
	}
	PlayMenuSound(TEXT("S_UIConfirm"), 0.6f);
	switch (MenuPage)
	{
	case EBRMenuPage::Main:
		if (Item == 0)
		{
			bHostFlow = false;
			SetMenuPage(EBRMenuPage::Saves);
		}
		else if (Item == 1)
		{
			SetMenuPage(EBRMenuPage::Multi);
		}
		else if (Item == 2)
		{
			SetInventoryOpen(true, 2); // onglet PARAMETRES
		}
		else
		{
			QuitToDesktop();
		}
		break;
	case EBRMenuPage::Saves:
		if (bConfirmDelete)
		{
			if (Item == 0)
			{
				const UBRSaveGame* Gone = GetSaveInSlot(DeleteSlot);
				const FString Name = Gone ? Gone->SaveName : FString();
				if (ActiveSave && ActiveSave.Get() == Gone)
				{
					ActiveSave = nullptr;
				}
				BRSaves::Delete(DeleteSlot);
				bConfirmDelete = false;
				RefreshSaves();
				MenuCursor = 0;
				ABRHUD::Notify(this, FString::Printf(TEXT("Partie \u00ab %s \u00bb supprim\u00e9e."), *Name), 3.f, FLinearColor(1.f, 0.7f, 0.55f));
			}
			else
			{
				MenuBack();
			}
			break;
		}
		{
			const int32 Slot = GetMenuSaveSlot(Item);
			if (Slot >= 0)
			{
				SelectSave(Slot);
			}
			else if (Slot == MenuItemNew)
			{
				SetMenuPage(EBRMenuPage::NewSave);
			}
			else
			{
				MenuBack();
			}
		}
		break;
	case EBRMenuPage::NewSave:
		if (Item == 0)
		{
			StartNewSave();
		}
		else
		{
			MenuBack();
		}
		break;
	case EBRMenuPage::Solo:
		if (Item == 0)
		{
			if (bHostFlow)
			{
				HostGame();
			}
			else
			{
				StartSolo();
			}
		}
		else
		{
			MenuBack();
		}
		break;
	case EBRMenuPage::Multi:
		if (Item == 0)
		{
			bHostFlow = true; // l'hote choisit une de ses parties, puis un niveau deja explore
			SetMenuPage(EBRMenuPage::Saves);
		}
		else if (Item == 1)
		{
			SetMenuPage(EBRMenuPage::Join);
		}
		else
		{
			MenuBack();
		}
		break;
	case EBRMenuPage::Join:
		if (Item == 0)
		{
			JoinGame();
		}
		else
		{
			MenuBack();
		}
		break;
	}
}

// =====================================================================================================================
// Sauvegardes
// =====================================================================================================================

void ABRPlayerController::RefreshSaves()
{
	SaveSlots.SetNum(BRSaves::MaxSlots);
	SaveOrder.Reset();
	for (int32 i = 0; i < BRSaves::MaxSlots; ++i)
	{
		SaveSlots[i] = BRSaves::Load(i);
		if (SaveSlots[i])
		{
			SaveOrder.Add(i);
		}
	}
	SaveOrder.Sort([this](int32 L, int32 R) { return SaveSlots[L]->LastPlayed > SaveSlots[R]->LastPlayed; });
	ShowSaveLoadMessages();
}

void ABRPlayerController::ShowSaveFailures()
{
	const TArray<FWriteFailure> Failed = BRSaves::PendingFailures();
	if (Failed.Num() == 0)
	{
		return;
	}
	uint32 Last = 0;
	for (const FWriteFailure& F : Failed)
	{
		Last = FMath::Max(Last, F.RequestId);
		UE_LOG(LogBackrooms, Warning, TEXT("Sauvegarde : echec de la demande %u (emplacement %d) : %s"), F.RequestId, F.Slot + 1, *F.Reason);
	}
	const FWriteFailure& F = Failed.Last();
	ABRHUD::Notify(this, FString::Printf(TEXT("\u00c9chec de la sauvegarde (emplacement %d, demande n\u00b0 %u) : %s. La partie continue ; nouvel essai \u00e0 la prochaine sauvegarde."),
		F.Slot + 1, F.RequestId, *F.Reason), 9.f, FLinearColor(1.f, 0.5f, 0.4f));
	BRSaves::AcknowledgeFailures(Last);
}

void ABRPlayerController::ShowSaveLoadMessages()
{
	for (const FString& Msg : BRSaves::TakeLoadMessages())
	{
		ABRHUD::Notify(this, Msg, 10.f, FLinearColor(1.f, 0.75f, 0.45f));
	}
}

void ABRPlayerController::ResolvePendingDeath(UBRSaveGame* Save)
{
	if (!Save || !Save->bPendingDeath)
	{
		return;
	}
	// Le jeu a ete ferme pendant une mort (a terre, ou pendant le fondu) : elle va a son terme, comme en jeu.
	// L'equipement est perdu, le niveau sera neuf ; le journal, les niveaux explores et le temps de jeu restent.
	Save->bPendingDeath = false;
	Save->bHasPlayer = false;
	Save->Items.Reset();
	Save->Health = 100.f;
	Save->Sanity = 100.f;
	Save->Battery = 100.f;
	Save->Session = FBRSessionState();
	Save->CurrentLevel = Save->IsExplored(Save->PendingDeathLevel) ? Save->PendingDeathLevel : 0;
	ABRHUD::Notify(this, TEXT("La derni\u00e8re session s'est arr\u00eat\u00e9e pendant une mort : vous repartez avec l'\u00e9quipement de d\u00e9part."), 7.f,
		FLinearColor(1.f, 0.7f, 0.5f));
}

void ABRPlayerController::SelectSave(int32 Slot)
{
	UBRSaveGame* S = GetSaveInSlot(Slot);
	if (!S)
	{
		return;
	}
	if (S->bFutureFormat)
	{
		// v4.8 : partie d'une version plus recente : lecture seule, jamais reprise ni reecrite par ce jeu
		ABRHUD::Notify(this, FString::Printf(TEXT("Cette partie vient d'une version plus r\u00e9cente du jeu (format %d) : elle reste intacte et ne peut pas \u00eatre reprise ici."),
			S->LoadedVersion), 6.f, FLinearColor(1.f, 0.7f, 0.45f));
		PlayMenuSound(TEXT("S_UIDeny"), 0.5f);
		return;
	}
	ActiveSave = S;
	BRSaves::ActiveSlot() = Slot;
	ResolvePendingDeath(S);
	// On reprend la ou on s'etait arrete
	const int32 Level = S->IsExplored(S->CurrentLevel) ? S->CurrentLevel : 0;
	SetMenuPage(EBRMenuPage::Solo);
	MenuIndex = FMath::Max(0, BRLevels::IndexOf(Level));
}

void ABRPlayerController::StartNewSave()
{
	const int32 Slot = BRSaves::FreeSlot();
	if (Slot == INDEX_NONE)
	{
		ABRHUD::Notify(this, TEXT("Les 6 emplacements sont occup\u00e9s : supprimez une partie (touche Suppr)."), 5.f, FLinearColor(1.f, 0.6f, 0.45f));
		return;
	}
	UBRSaveGame* S = Cast<UBRSaveGame>(UGameplayStatics::CreateSaveGameObject(UBRSaveGame::StaticClass()));
	if (!S)
	{
		return;
	}
	FString Name = NewSaveName.TrimStartAndEnd();
	if (Name.IsEmpty())
	{
		Name = FString::Printf(TEXT("Partie %d"), Slot + 1);
	}
	S->SaveName = Name.Left(28);
	S->Created = FDateTime::Now();
	// Une nouvelle partie commence toujours au Niveau 0
	S->CurrentLevel = 0;
	S->MarkExplored(0);
	BRSaves::Write(Slot, S);
	BRSaves::ActiveSlot() = Slot;
	ActiveSave = S;
	MenuIndex = FMath::Max(0, BRLevels::IndexOf(0));
	ShowNameBox(false);
	if (bHostFlow)
	{
		HostGame();
	}
	else
	{
		StartSolo();
	}
}

void ABRPlayerController::ApplyActiveSave()
{
	ResolvePendingDeath(ActiveSave);
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->ReadFromSave(ActiveSave);
	}
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->RestoreJournal(ActiveSave ? ActiveSave->Discovered : TArray<int32>(), ActiveSave ? ActiveSave->Explored : TArray<int32>());
	}
	AutoSaveTimer = 60.f;
}

void ABRPlayerController::OnLevelLoaded(int32 LevelNumber)
{
	// v4.7 : le point de reprise appartient au niveau : il sera releve des que le joueur se tient au sol
	bHasSafeSpot = false;
	SafeSpotTimer = 0.f;
	if (IsDevMode() && !bInMenu)
	{
		DevHelpTime = 10.f;
	}
	if (!ActiveSave || BRSaves::ActiveSlot() == INDEX_NONE || bInMenu || !IsLocalController() || GetNetMode() == NM_Client)
	{
		return;
	}
	const bool bNew = !ActiveSave->IsExplored(LevelNumber);
	if (bNew && bDevSession)
	{
		ABRHUD::Notify(this, FString::Printf(TEXT("MODE D\u00c9VELOPPEUR : Niveau %d visit\u00e9 sans l'ajouter \u00e0 la partie \u00ab %s \u00bb."),
			LevelNumber, *ActiveSave->SaveName), 6.f, FLinearColor(0.6f, 0.9f, 1.f));
		return;
	}
	ActiveSave->MarkExplored(LevelNumber);
	ActiveSave->CurrentLevel = LevelNumber;
	// Ecrit un peu plus tard : apres une mort, l'inventaire est remis a zero juste apres le chargement
	PendingSaveDelay = 2.f;
	if (bNew)
	{
		ABRHUD::Notify(this, FString::Printf(TEXT("Niveau %d ajout\u00e9 \u00e0 vos niveaux explor\u00e9s (%d / %d) : vous pourrez y revenir depuis le menu."),
			LevelNumber, ActiveSave->Explored.Num(), BRLevels::All().Num()), 7.f, FLinearColor(0.75f, 1.f, 0.75f));
	}
}

void ABRPlayerController::NotifyPlayerDeath()
{
	if (ActiveSave && IsLocalController())
	{
		++ActiveSave->Deaths;
		// v4.7 : ecrit tout de suite : fermer le jeu a terre ne doit pas annuler la mort
		WriteActiveSave();
	}
}

void ABRPlayerController::WriteActiveSave(bool bBlocking)
{
	if (!ActiveSave || BRSaves::ActiveSlot() == INDEX_NONE || bInMenu || !IsLocalController() || GetNetMode() == NM_Client)
	{
		if (bBlocking)
		{
			BRSaves::Flush();
		}
		return;
	}
	const ABRCharacter* C = GetBRCharacter();
	const ABRWorld* W = ABRWorld::Get(this);
	const bool bDeadNow = C && C->IsDead();
	if (bDeadNow)
	{
		// v4.7 : mort en cours : l'inventaire d'avant la mort n'est pas reecrit, et la mort est notee pour le prochain
		// chargement (seul : retour au Niveau 0 ; en equipe : reveil dans le niveau en cours)
		ActiveSave->bPendingDeath = true;
		ActiveSave->PendingDeathLevel = (W && IsNetGame()) ? W->GetLevelNumber() : 0;
		ActiveSave->Session.bValid = false;
	}
	else if (C)
	{
		C->WriteToSave(ActiveSave);
		ActiveSave->bPendingDeath = false;
	}
	if (W)
	{
		ActiveSave->Discovered = W->GetDiscoveredList();
		if (W->IsLevelReady() && !W->IsTransitioning() && (!bDevSession || ActiveSave->IsExplored(W->GetLevelNumber())))
		{
			ActiveSave->CurrentLevel = W->GetLevelNumber();
			ActiveSave->MarkExplored(W->GetLevelNumber());
			// v4.7 : session a reprendre : meme disposition, objectifs, objets ramasses, dernier point sur
			if (!bDeadNow && !bDevSession && C)
			{
				FBRSessionState& Session = ActiveSave->Session;
				Session.bValid = true;
				Session.Level = W->GetLevelNumber();
				Session.Seed = W->GetSeed();
				Session.VHSFound = W->GetVHSFound();
				Session.bBlackoutRecorded = W->IsBlackoutRecorded();
				Session.bEntityRecorded = W->IsEntityRecorded();
				Session.Collected = W->GetCollectedList();
				Session.bHasSpot = bHasSafeSpot;
				Session.Spot = SafeSpot;
				Session.Yaw = SafeYaw;
			}
		}
	}
	const bool bQueued = bBlocking ? BRSaves::Write(BRSaves::ActiveSlot(), ActiveSave) : BRSaves::WriteAsync(BRSaves::ActiveSlot(), ActiveSave);
	if (bQueued)
	{
		TimeSinceSave = 0.f;
	}
}

void ABRPlayerController::RequestDeleteSave(int32 Slot)
{
	if (MenuPage != EBRMenuPage::Saves || !GetSaveInSlot(Slot))
	{
		return;
	}
	bConfirmDelete = true;
	DeleteSlot = Slot;
	MenuCursor = 1; // ANNULER par defaut
	PlayMenuSound(TEXT("S_UIDeny"), 0.5f);
}

void ABRPlayerController::HostGame()
{
	// La carte est rechargee en serveur "listen" : les amis peuvent rejoindre sur le port 7777
	const int32 Level = BRLevels::All()[FMath::Clamp(MenuIndex, 0, BRLevels::All().Num() - 1)].Number;
	MenuStatus = TEXT("Cr\u00e9ation de la partie...");
	// v4.7 : l'hote reprend sa session (la carte est rechargee : le monde la retrouve dans BRSaves::PendingResume)
	BRSaves::PendingResume() = FBRSessionState();
	if (ActiveSave && ActiveSave->Session.bValid && ActiveSave->Session.Level == Level && ActiveSave->Session.Seed != 0)
	{
		BRSaves::PendingResume() = ActiveSave->Session;
	}
	BRSaves::Flush();
	const FString Map = UGameplayStatics::GetCurrentLevelName(this, true);
	UGameplayStatics::OpenLevel(this, FName(*Map), true, FString::Printf(TEXT("listen?BRLevel=%d"), Level));
}

void ABRPlayerController::JoinGame()
{
	// Invite : la progression appartient a la partie de l'hote
	BRSaves::ActiveSlot() = INDEX_NONE;
	ActiveSave = nullptr;
	const FString Address = JoinAddress.TrimStartAndEnd();
	if (Address.IsEmpty())
	{
		MenuStatus = TEXT("Entrez l'adresse IP de l'h\u00f4te (ex. 192.168.1.20).");
		return;
	}
	BRConfig::Get().SetString(SettingsSection, TEXT("LastAddress"), *Address);
	BRConfig::Save();
	MenuStatus = FString::Printf(TEXT("Connexion \u00e0 %s..."), *Address);
	ClientTravel(Address, TRAVEL_Absolute);
}

void ABRPlayerController::ReturnToMainMenu()
{
	WriteActiveSave(true);
	// Recharger la carte hors ligne : on quitte la session (l'hote ferme la partie pour tout le monde)
	const FString Map = UGameplayStatics::GetCurrentLevelName(this, true);
	UGameplayStatics::OpenLevel(this, FName(*Map), true);
}

void ABRPlayerController::ShowAddressBox(bool bShow)
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (bShow && !AddressWidget.IsValid() && Viewport && FSlateApplication::IsInitialized())
	{
		TWeakObjectPtr<ABRPlayerController> WeakThis(this);
		AddressWidget = SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(520.f)
				.HeightOverride(52.f)
				[
					SAssignNew(AddressBox, SEditableTextBox)
					.Text(FText::FromString(JoinAddress))
					.HintText(FText::FromString(TEXT("Adresse IP de l'h\u00f4te, ex. 192.168.1.20")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 22))
					.SelectAllTextWhenFocused(true)
					.OnTextChanged_Lambda([WeakThis](const FText& NewText)
					{
						if (ABRPlayerController* Self = WeakThis.Get())
						{
							Self->JoinAddress = NewText.ToString();
						}
					})
					.OnTextCommitted_Lambda([WeakThis](const FText& NewText, ETextCommit::Type CommitType)
					{
						ABRPlayerController* Self = WeakThis.Get();
						if (Self && Self->MenuPage == EBRMenuPage::Join)
						{
							Self->JoinAddress = NewText.ToString();
							if (CommitType == ETextCommit::OnEnter)
							{
								Self->JoinGame();
							}
						}
					})
				]
			];
		Viewport->AddViewportWidgetContent(AddressWidget.ToSharedRef(), 50);
		FSlateApplication::Get().SetKeyboardFocus(AddressBox);
	}
	else if (!bShow && AddressWidget.IsValid())
	{
		if (Viewport)
		{
			Viewport->RemoveViewportWidgetContent(AddressWidget.ToSharedRef());
		}
		AddressWidget.Reset();
		AddressBox.Reset();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
}

void ABRPlayerController::ShowNameBox(bool bShow)
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (bShow && !NameWidget.IsValid() && Viewport && FSlateApplication::IsInitialized())
	{
		TWeakObjectPtr<ABRPlayerController> WeakThis(this);
		NameWidget = SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(520.f)
				.HeightOverride(52.f)
				[
					SAssignNew(NameBox, SEditableTextBox)
					.Text(FText::FromString(NewSaveName))
					.HintText(FText::FromString(TEXT("Nom de la partie")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 22))
					.SelectAllTextWhenFocused(true)
					.OnTextChanged_Lambda([WeakThis](const FText& NewText)
					{
						if (ABRPlayerController* Self = WeakThis.Get())
						{
							Self->NewSaveName = NewText.ToString();
						}
					})
					.OnTextCommitted_Lambda([WeakThis](const FText& NewText, ETextCommit::Type CommitType)
					{
						ABRPlayerController* Self = WeakThis.Get();
						if (Self && Self->MenuPage == EBRMenuPage::NewSave)
						{
							Self->NewSaveName = NewText.ToString();
							if (CommitType == ETextCommit::OnEnter)
							{
								Self->bPendingNewSave = true; // lance a l'image suivante (le champ se ferme)
							}
						}
					})
				]
			];
		Viewport->AddViewportWidgetContent(NameWidget.ToSharedRef(), 50);
		FSlateApplication::Get().SetKeyboardFocus(NameBox);
	}
	else if (!bShow && NameWidget.IsValid())
	{
		if (Viewport)
		{
			Viewport->RemoveViewportWidgetContent(NameWidget.ToSharedRef());
		}
		NameWidget.Reset();
		NameBox.Reset();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
}

// =====================================================================================================================
// Multijoueur : demandes des clients executees par le serveur
// =====================================================================================================================

void ABRPlayerController::ServerRequestTransition_Implementation(int32 TargetLevel)
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W)
	{
		return;
	}
	// v4.7 : un client ne fait changer le groupe de niveau qu'en prenant une vraie sortie, ouverte, a cote de lui.
	// Le saut de niveau du mode developpeur passe seulement si l'hote autorise les commandes de test.
	if (!AreCheatsAllowed())
	{
		const ABRCharacter* C = Cast<ABRCharacter>(GetPawn());
		FString Reason;
		bool bNearExit = false;
		if (C && !C->IsDead() && W->CanLeaveLevel(Reason))
		{
			for (TActorIterator<ABRExit> It(GetWorld()); It; ++It)
			{
				if (It->Target == TargetLevel && FVector::DistSquared2D(It->GetActorLocation(), C->GetActorLocation()) < FMath::Square(600.f))
				{
					bNearExit = true;
					break;
				}
			}
		}
		if (!bNearExit)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Changement de niveau refuse pour %s (cible %d) : aucune sortie ouverte a proximite"),
				*GetNameSafe(PlayerState), TargetLevel);
			return;
		}
	}
	W->RequestTransition(TargetLevel);
}

void ABRPlayerController::ServerMarkCollected_Implementation(uint64 Id)
{
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->ServerCollected(Id);
	}
}

void ABRPlayerController::ServerVHSCollected_Implementation()
{
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->OnVHSCollected();
	}
}

void ABRPlayerController::ServerCompleteObjective_Implementation(uint8 Which)
{
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->ServerCompleteObjective(Which);
	}
}

void ABRPlayerController::ServerCheat_Implementation(uint8 Command, int32 Value)
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W)
	{
		return;
	}
	// v4.7 : decide par le serveur (jamais en Shipping), pas par le client qui envoie la commande
	if (!AreCheatsAllowed())
	{
		UE_LOG(LogBackrooms, Warning, TEXT("Commande de test %d refusee pour %s"), Command, *GetNameSafe(PlayerState));
		return;
	}
	switch (Command)
	{
	case 1:
		W->ForceBlackout();
		break;
	case 2:
		W->DebugCompleteObjectives();
		break;
	case 3:
		SpawnInFront(Value);
		break;
	default:
		break;
	}
}

void ABRPlayerController::StartSolo()
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W || W->IsTransitioning())
	{
		return;
	}
	bInMenu = false;
	ShowAddressBox(false);
	ShowNameBox(false);
	UpdateInputMode();
	// Partie choisie : inventaire, sante, journal (nouvelle partie : equipement de depart)
	ApplyActiveSave();
	const int32 Target = BRLevels::All()[MenuIndex].Number;
	// Mode developpeur : un niveau pas encore explore se visite sans etre ajoute a la partie
	bDevSession = ActiveSave && !ActiveSave->IsExplored(Target);
	// v4.7 : reprise apres fermeture du jeu : meme graine, objectifs, objets ramasses et point de reprise.
	// (Apres une mort, la session a ete invalidee : le niveau sera neuf, comme le veut la regle.)
	const FBRSessionState* Session = ActiveSave ? &ActiveSave->Session : nullptr;
	if (Session && Session->bValid && Session->Level == Target && Session->Seed != 0 && !bDevSession)
	{
		BRSaves::PendingResume() = *Session;
		ABRHUD::Notify(this, FString::Printf(TEXT("Reprise de la partie \u00ab %s \u00bb : Niveau %d, l\u00e0 o\u00f9 vous l'aviez laiss\u00e9."),
			*ActiveSave->SaveName, Target), 5.f, FLinearColor(0.75f, 0.95f, 1.f));
		W->RequestTransition(Target, false, Session->Seed);
	}
	else if (Target != W->GetLevelNumber())
	{
		W->RequestTransition(Target);
	}
	else
	{
		W->ReplayTitle();
		OnLevelLoaded(Target);
	}
}

// =====================================================================================================================
// Commandes console
// =====================================================================================================================

void ABRPlayerController::BRLevel(int32 Number)
{
	if (!CheatGate())
	{
		return;
	}
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		bDevSession = true;
		bInMenu = false;
		ShowAddressBox(false);
		UpdateInputMode();
		W->RequestTransition(Number);
	}
}

void ABRPlayerController::BRPits()
{
	if (!CheatGate())
	{
		return;
	}
	ABRWorld* W = ABRWorld::Get(this);
	ABRCharacter* C = GetBRCharacter();
	if (!W || !C)
	{
		return;
	}
	FVector Loc;
	FRotator Rot;
	if (W->IsLevelReady() && !W->IsTransitioning() && W->FindPitRoomView(C->GetActorLocation(), Loc, Rot))
	{
		const float Half = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
		Loc.Z = Half + 5.f;
		C->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
		C->GetCharacterMovement()->StopMovementImmediately();
		SetControlRotation(Rot);
		bDevSession = true;
		ABRHUD::Notify(this, FString::Printf(TEXT("MODE D\u00c9V : salle de fosses (Niveau %d, graine %u). Attention au bord."), W->GetLevelNumber(),
			W->GetSeed()), 4.f, FLinearColor(0.6f, 0.9f, 1.f));
		return;
	}
	if (!HasAuthority())
	{
		ABRHUD::Notify(this, TEXT("Pas de salle de fosses dans ce niveau (l'h\u00f4te peut charger le Niveau 0 : BRPits)."), 4.f);
		return;
	}
	// Pas de salle ici : Niveau 0 avec la graine de demonstration, puis placement au bord des fosses
	bDevSession = true;
	bInMenu = false;
	ShowAddressBox(false);
	UpdateInputMode();
	bPendingPitTeleport = true;
	ABRHUD::Notify(this, FString::Printf(TEXT("MODE D\u00c9V : Niveau 0, graine de d\u00e9monstration %u"), ABRWorld::DemoSeed), 3.f,
		FLinearColor(0.6f, 0.9f, 1.f));
	W->RequestTransition(0, false, ABRWorld::SeedFromUser(ABRWorld::DemoSeed, 0));
}

void ABRPlayerController::BRSeed(int32 Number)
{
	if (!CheatGate())
	{
		return;
	}
	ABRWorld* W = ABRWorld::Get(this);
	if (!W)
	{
		return;
	}
	if (!HasAuthority())
	{
		ABRHUD::Notify(this, TEXT("BRSeed : r\u00e9serv\u00e9 \u00e0 l'h\u00f4te de la partie."), 3.f);
		return;
	}
	bDevSession = true;
	const int32 Level = W->GetLevelNumber();
	ABRHUD::Notify(this, FString::Printf(TEXT("Niveau %d, graine %d"), Level, Number), 3.f, FLinearColor(0.6f, 0.9f, 1.f));
	W->RequestTransition(Level, false, ABRWorld::SeedFromUser(static_cast<uint32>(Number), Level));
}

void ABRPlayerController::BRGod()
{
	if (!CheatGate())
	{
		return;
	}
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->bGodMode = !C->bGodMode;
		ABRHUD::Notify(this, C->bGodMode ? TEXT("Mode invincible : ON") : TEXT("Mode invincible : OFF"), 2.f);
	}
}

void ABRPlayerController::BRSpawn(int32 Kind)
{
	if (!CheatGate())
	{
		return;
	}
	if (!HasAuthority())
	{
		ServerCheat(3, Kind);
		return;
	}
	SpawnInFront(Kind);
}

void ABRPlayerController::SpawnInFront(int32 Kind)
{
	ABRWorld* W = ABRWorld::Get(this);
	ABRCharacter* C = GetBRCharacter();
	if (!W || !C)
	{
		return;
	}
	const int32 K = FMath::Clamp(Kind, 0, static_cast<int32>(EBREntityKind::Count) - 1);
	const FBREntityInfo& Info = ABREntity::Info(static_cast<EBREntityKind>(K));
	FVector Loc = C->GetActorLocation() + C->GetActorForwardVector() * 600.f;
	Loc.Z = Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 10.f;
	W->SpawnEntity(static_cast<EBREntityKind>(K), Loc);
}

void ABRPlayerController::BRSensitivity(float Value)
{
	FBRSettings::Get().Sensitivity = FMath::Clamp(Value, 0.05f, 10.f);
	SaveSettings();
	ABRHUD::Notify(this, FString::Printf(TEXT("Sensibilit\u00e9 : %.2f"), FBRSettings::Get().Sensitivity), 2.f);
}

void ABRPlayerController::BRInvertY()
{
	FBRSettings::Get().bInvertY = !FBRSettings::Get().bInvertY;
	SaveSettings();
	ABRHUD::Notify(this, FBRSettings::Get().bInvertY ? TEXT("Axe Y invers\u00e9") : TEXT("Axe Y normal"), 2.f);
}

void ABRPlayerController::BRGiveAll()
{
	if (!CheatGate())
	{
		return;
	}
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->AddItem(EBRItem::AlmondWater, 4);
		C->AddItem(EBRItem::Bandage, 4);
		C->AddItem(EBRItem::Battery, 6);
		C->AddItem(EBRItem::EnergyBar, 4);
		C->AddItem(EBRItem::Headlamp, 1);
		C->AddItem(EBRItem::Vest, 1);
		C->Sanity = 100.f;
		C->Health = 100.f;
		C->Battery = 100.f;
	}
}

void ABRPlayerController::BRBlackout()
{
	if (!CheatGate())
	{
		return;
	}
	if (!HasAuthority())
	{
		ServerCheat(1, 0);
	}
	else if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->ForceBlackout();
	}
}

void ABRPlayerController::BRObjectives()
{
	if (!CheatGate())
	{
		return;
	}
	if (!HasAuthority())
	{
		ServerCheat(2, 0);
	}
	else if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->DebugCompleteObjectives();
	}
}

// =====================================================================================================================
// Parametres (sauvegardes dans Saved/Config/<plateforme>/BackroomsPlayer.ini)
// =====================================================================================================================

int32 ABRPlayerController::GetSettingsCount() const
{
	return Row_Count;
}

FString ABRPlayerController::GetSettingLabel(int32 Index) const
{
	switch (Index)
	{
	case Row_Sensitivity:
		return TEXT("SENSIBILIT\u00c9 DE LA SOURIS");
	case Row_InvertY:
		return TEXT("INVERSER L'AXE VERTICAL");
	case Row_FOV:
		return TEXT("CHAMP DE VISION");
	case Row_HeadBob:
		return TEXT("BALANCEMENT DE LA CAM\u00c9RA");
	case Row_CameraShake:
		return TEXT("TREMBLEMENTS DE LA CAM\u00c9RA");
	case Row_Flashes:
		return TEXT("FLASHS ET CLIGNOTEMENTS");
	case Row_MotionBlur:
		return TEXT("FLOU DE MOUVEMENT");
	case Row_Volume:
		return TEXT("VOLUME G\u00c9N\u00c9RAL");
	case Row_Voice:
		return TEXT("CHAT VOCAL (PROXIMIT\u00c9)");
	case Row_Brightness:
		return TEXT("LUMINOSIT\u00c9");
	case Row_WindowMode:
		return TEXT("MODE D'AFFICHAGE");
	case Row_RenderScale:
		return TEXT("R\u00c9SOLUTION DE RENDU");
	case Row_VSync:
		return TEXT("SYNCHRO VERTICALE (V-SYNC)");
	case Row_MaxFPS:
		return TEXT("IMAGES PAR SECONDE MAX.");
	case Row_Profile:
		return TEXT("PROFIL GRAPHIQUE");
	case Row_Quality:
		return TEXT("QUALIT\u00c9 GRAPHIQUE");
	case Row_HardwareRT:
		return TEXT("RAY TRACING MAT\u00c9RIEL (RTX)");
	case Row_RTHitLighting:
		return TEXT("REFLETS RAY TRAC\u00c9S HAUTE QUALIT\u00c9");
	case Row_RTShadows:
		return TEXT("OMBRES RAY TRAC\u00c9ES (LAMPE)");
	case Row_AreaLights:
		return TEXT("N\u00c9ONS EN LUMI\u00c8RES SURFACIQUES");
	case Row_FullCreatures:
		return TEXT("MOD\u00c8LES COMPLETS DES ENTIT\u00c9S");
	case Row_VolumetricFog:
		return TEXT("BROUILLARD VOLUM\u00c9TRIQUE");
	case Row_FilmGrain:
		return TEXT("GRAIN DE L'IMAGE");
	case Row_VHSEffect:
		return TEXT("EFFET VHS");
	case Row_DevMode:
		return TEXT("MODE D\u00c9VELOPPEUR");
	default:
		return FString();
	}
}

FString ABRPlayerController::GetSettingValue(int32 Index) const
{
	const FBRSettings& S = FBRSettings::Get();
	switch (Index)
	{
	case Row_Sensitivity:
		return FString::Printf(TEXT("%.2f"), S.Sensitivity);
	case Row_InvertY:
		return OnOff(S.bInvertY);
	case Row_FOV:
		return FString::Printf(TEXT("%d\u00b0"), FMath::RoundToInt(S.FOV));
	case Row_HeadBob:
		return OnOff(S.bHeadBob);
	case Row_CameraShake:
		return S.CameraShake <= 0.f ? FString(TEXT("AUCUN")) : FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.CameraShake * 100.f));
	case Row_Flashes:
		return S.Flashes <= 0 ? FString(TEXT("NORMAUX")) : (S.Flashes == 1 ? FString(TEXT("ATT\u00c9NU\u00c9S")) : FString(TEXT("AUCUN")));
	case Row_MotionBlur:
		return OnOff(S.bMotionBlur);
	case Row_Volume:
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.MasterVolume * 100.f));
	case Row_Voice:
		return VoiceNames[FMath::Clamp(S.VoiceMode, 0, 2)];
	case Row_Brightness:
		return FString::Printf(TEXT("%+.1f"), S.Brightness);
	case Row_WindowMode:
		return WindowNames[FMath::Clamp(S.WindowMode, 0, 2)];
	case Row_RenderScale:
	{
		// v4.8 : resolution interne reelle (celle que TSR agrandit), pas seulement le pourcentage
		const FIntPoint In = InternalResolution();
		return In.X > 0 ? FString::Printf(TEXT("%d %%  (%d\u00d7%d)"), S.RenderScale, In.X, In.Y) : FString::Printf(TEXT("%d %%"), S.RenderScale);
	}
	case Row_VSync:
		return OnOff(S.bVSync);
	case Row_MaxFPS:
		return S.MaxFPS <= 0 ? FString(TEXT("ILLIMIT\u00c9")) : FString::Printf(TEXT("%d"), S.MaxFPS);
	case Row_Profile:
		return ProfileNames[FMath::Clamp(S.GraphicsProfile, 0, 4)];
	case Row_Quality:
		return QualityNames[FMath::Clamp(S.Quality, 0, 4)];
	case Row_HardwareRT:
		return !IsHardwareRayTracingAvailable() ? FString(TEXT("INDISPONIBLE")) : OnOff(S.bHardwareRT);
	case Row_RTHitLighting:
		return !IsHardwareRayTracingAvailable() ? FString(TEXT("INDISPONIBLE")) : OnOff(S.bRTHitLighting);
	case Row_RTShadows:
		return !IsHardwareRayTracingAvailable() ? FString(TEXT("INDISPONIBLE")) : OnOff(S.bRTShadows);
	case Row_AreaLights:
		return OnOff(S.bAreaLights);
	case Row_FullCreatures:
		return OnOff(S.bFullCreatures);
	case Row_VolumetricFog:
		return OnOff(S.bVolumetricFog);
	case Row_FilmGrain:
		return OnOff(S.bFilmGrain);
	case Row_VHSEffect:
		return OnOff(S.bVHSEffect);
	case Row_DevMode:
		return OnOff(S.bDevMode);
	default:
		return FString();
	}
}

FString ABRPlayerController::GetSettingHint(int32 Index) const
{
	switch (Index)
	{
	case Row_HeadBob:
		return TEXT("D\u00e9sactivez-le si le mouvement de la cam\u00e9ra pendant la marche vous incommode.");
	case Row_CameraShake:
		return TEXT("Secousses de la cam\u00e9ra quand on est frapp\u00e9 et pendant les jumpscares. Les coups et leurs d\u00e9g\u00e2ts ne changent pas.");
	case Row_Flashes:
		return TEXT("\u00c9clairs des jumpscares, image de la mort, n\u00e9ons qui clignotent. Att\u00e9nu\u00e9s ou supprim\u00e9s si les lumi\u00e8res vives qui clignotent vous g\u00eanent ; les coupures de courant restent annonc\u00e9es par le son.");
	case Row_MotionBlur:
		return TEXT("Flou des mouvements rapides de la cam\u00e9ra. D\u00e9sactiv\u00e9 par d\u00e9faut.");
	case Row_Volume:
		return TEXT("Volume de tout le jeu (ambiance, entit\u00e9s, voix des co\u00e9quipiers).");
	case Row_Voice:
		return BRKeys::Expand(TEXT("On entend les autres joueurs pr\u00e8s de leur personnage, \u00e9touff\u00e9s par les murs. Appuyer pour parler : touche {PushToTalk}. Voix ouverte : le micro transmet en permanence. Micro coup\u00e9 : vous entendez toujours les autres."));
	case Row_Brightness:
		return TEXT("Rend l'image plus claire ou plus sombre (les zones sans lumi\u00e8re restent noires).");
	case Row_WindowMode:
		return TEXT("Plein \u00e9cran exclusif, plein \u00e9cran fen\u00eatr\u00e9 (Alt+Tab instantan\u00e9) ou fen\u00eatre. Sans effet dans l'\u00e9diteur.");
	case Row_RenderScale:
		return TEXT("En dessous de 100 %, l'image est calcul\u00e9e plus petite puis agrandie par TSR : beaucoup plus fluide, l\u00e9g\u00e8rement plus floue.");
	case Row_VSync:
		return TEXT("Supprime les d\u00e9chirures d'image, ajoute un peu de latence.");
	case Row_MaxFPS:
		return TEXT("Limiter les images par seconde r\u00e9duit la chaleur et le bruit de la carte graphique. Sans effet dans l'\u00e9diteur.");
	case Row_Profile:
		return TEXT("PERFORMANCE : qualit\u00e9 \u00c9lev\u00e9e, Lumen logiciel, rendu \u00e0 67 % (TSR). QUALIT\u00c9 : \u00c9pique, Lumen en ray tracing ")
			TEXT("mat\u00e9riel (cache de surfaces), rendu \u00e0 80 %. RTX FLUIDE : ray tracing mat\u00e9riel, rendu \u00e0 67 % agrandi par TSR, ombres des ")
			TEXT("n\u00e9ons jusqu'\u00e0 25 m, Hound all\u00e9g\u00e9 : vise 60 images/s stables avec une carte RTX. CIN\u00c9MATIQUE : reflets \u00e9clair\u00e9s par les ")
			TEXT("rayons, ombres ray trac\u00e9es de la lampe, mod\u00e8les complets, rendu \u00e0 100 %. Modifier un r\u00e9glage ci-dessous passe en PERSONNALIS\u00c9. ")
			TEXT("S'applique tout de suite, aussi aux zones d\u00e9j\u00e0 charg\u00e9es.");
	case Row_Quality:
		return TEXT("Ombres, Lumen, textures, anti-cr\u00e9nelage (scalability). S'applique tout de suite.");
	case Row_HardwareRT:
		return IsHardwareRayTracingAvailable()
			? FString(TEXT("Lumen en ray tracing mat\u00e9riel : reflets et lumi\u00e8re indirecte bien plus pr\u00e9cis (carte RTX / RX 6000+). S'applique tout de suite."))
			: FString(TEXT("Indisponible : le jeu a d\u00e9marr\u00e9 sans ray tracing (DirectX 12 et carte compatible requis, r.RayTracing=True). ")
				TEXT("Lumen logiciel est utilis\u00e9. Changer de carte ou de RHI demande un red\u00e9marrage."));
	case Row_RTHitLighting:
		return TEXT("Reflets \u00e9clair\u00e9s par les rayons eux-m\u00eames (eau, flaques, carrelage, m\u00e9tal) au lieu du cache de surfaces de Lumen. ")
			TEXT("Le plus co\u00fbteux des r\u00e9glages : seul le profil CIN\u00c9MATIQUE l'active.");
	case Row_RTShadows:
		return TEXT("Ombres de la lampe torche ray trac\u00e9es (contact net, pas de recalcul des ombres virtuelles \u00e0 chaque mouvement de la lampe). ")
			TEXT("Les plafonniers gardent les ombres virtuelles (VSM), moins ch\u00e8res pour des dizaines de lumi\u00e8res fixes.");
	case Row_AreaLights:
		return TEXT("Ombres douces des n\u00e9ons. S'applique tout de suite, zones d\u00e9j\u00e0 charg\u00e9es comprises (quelques lumi\u00e8res par image).");
	case Row_FullCreatures:
		return TEXT("Hound d'origine (175 000 sommets, pelage complet) au lieu du d\u00e9riv\u00e9 all\u00e9g\u00e9. Tr\u00e8s co\u00fbteux en ray tracing. S'applique aussi aux entit\u00e9s pr\u00e9sentes.");
	case Row_VolumetricFog:
		return TEXT("Halos de lumi\u00e8re dans l'air humide.");
	case Row_VHSEffect:
		return TEXT("Lignes de balayage, l\u00e9g\u00e8re aberration et salet\u00e9 d'objectif. D\u00e9sactiv\u00e9 : image nette.");
	case Row_DevMode:
		return TEXT("Tous les niveaux jouables depuis le choix des niveaux (non ajout\u00e9s \u00e0 la partie). En jeu : Page pr\u00e9c. / suiv. niveau, ")
			TEXT("D\u00e9but nouvelle disposition, Fin objectifs, Inser coupure, F6 vol libre, F7 invincible, F10 jumpscares.");
	default:
		return FString();
	}
}

void ABRPlayerController::AdjustSetting(int32 Index, int32 Direction)
{
	FBRSettings& S = FBRSettings::Get();
	const int32 Dir = Direction >= 0 ? 1 : -1;
	switch (Index)
	{
	case Row_Sensitivity:
		S.Sensitivity = FMath::Clamp(FMath::RoundToFloat((S.Sensitivity + Dir * 0.1f) * 100.f) / 100.f, 0.1f, 5.f);
		break;
	case Row_InvertY:
		S.bInvertY = !S.bInvertY;
		break;
	case Row_FOV:
		S.FOV = FMath::Clamp(S.FOV + Dir * 2.f, 70.f, 110.f);
		break;
	case Row_HeadBob:
		S.bHeadBob = !S.bHeadBob;
		break;
	case Row_CameraShake:
		S.CameraShake = FMath::Clamp(FMath::RoundToFloat((S.CameraShake + Dir * 0.25f) * 4.f) / 4.f, 0.f, 1.f);
		break;
	case Row_Flashes:
		S.Flashes = (S.Flashes + Dir + 3) % 3;
		break;
	case Row_MotionBlur:
		S.bMotionBlur = !S.bMotionBlur;
		break;
	case Row_Volume:
		S.MasterVolume = FMath::Clamp(FMath::RoundToFloat((S.MasterVolume + Dir * 0.05f) * 20.f) / 20.f, 0.f, 1.f);
		break;
	case Row_Voice:
		S.VoiceMode = (S.VoiceMode + Dir + 3) % 3;
		break;
	case Row_Brightness:
		S.Brightness = FMath::Clamp(FMath::RoundToFloat((S.Brightness + Dir * 0.1f) * 10.f) / 10.f, -1.5f, 1.5f);
		break;
	case Row_WindowMode:
		S.WindowMode = (S.WindowMode + Dir + 3) % 3;
		break;
	case Row_RenderScale:
		S.RenderScale = FMath::Clamp(S.RenderScale + Dir * 5, 50, 100);
		S.GraphicsProfile = 3;
		break;
	case Row_VSync:
		S.bVSync = !S.bVSync;
		break;
	case Row_MaxFPS:
	{
		int32 Step = 0;
		for (int32 k = 0; k < NumFPSSteps; ++k)
		{
			Step = FPSSteps[k] == S.MaxFPS ? k : Step;
		}
		S.MaxFPS = FPSSteps[(Step + Dir + NumFPSSteps) % NumFPSSteps];
		break;
	}
	case Row_Profile:
	{
		// le profil PERSONNALISE ne se choisit pas ; depuis lui, on repart du profil Qualite
		int32 Pos = 1;
		for (int32 k = 0; k < UE_ARRAY_COUNT(ProfileCycle); ++k)
		{
			Pos = ProfileCycle[k] == S.GraphicsProfile ? k : Pos;
		}
		const int32 NumProfiles = UE_ARRAY_COUNT(ProfileCycle);
		S.GraphicsProfile = ProfileCycle[(Pos + Dir + NumProfiles) % NumProfiles];
		ApplyGraphicsProfile(S.GraphicsProfile);
		break;
	}
	case Row_Quality:
		S.Quality = (S.Quality + Dir + 5) % 5;
		S.GraphicsProfile = 3;
		break;
	case Row_HardwareRT:
		S.bHardwareRT = !S.bHardwareRT;
		S.GraphicsProfile = 3;
		break;
	case Row_RTHitLighting:
		S.bRTHitLighting = !S.bRTHitLighting;
		S.GraphicsProfile = 3;
		break;
	case Row_RTShadows:
		S.bRTShadows = !S.bRTShadows;
		S.GraphicsProfile = 3;
		break;
	case Row_AreaLights:
		S.bAreaLights = !S.bAreaLights;
		S.GraphicsProfile = 3;
		break;
	case Row_FullCreatures:
		S.bFullCreatures = !S.bFullCreatures;
		S.GraphicsProfile = 3;
		break;
	case Row_VolumetricFog:
		S.bVolumetricFog = !S.bVolumetricFog;
		S.GraphicsProfile = 3;
		break;
	case Row_FilmGrain:
		S.bFilmGrain = !S.bFilmGrain;
		break;
	case Row_VHSEffect:
		S.bVHSEffect = !S.bVHSEffect;
		break;
	case Row_DevMode:
		S.bDevMode = !S.bDevMode;
		break;
	default:
		return;
	}
	ApplySettings();
	SaveSettings();
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->PlayUISound(TEXT("S_UIClick"));
	}
}

void ABRPlayerController::LoadSettings()
{
	const FConfigFile& Cfg = BRConfig::Get();
	FBRSettings& S = FBRSettings::Get();
	Cfg.GetFloat(SettingsSection, TEXT("Sensitivity"), S.Sensitivity);
	Cfg.GetBool(SettingsSection, TEXT("InvertY"), S.bInvertY);
	Cfg.GetFloat(SettingsSection, TEXT("FOV"), S.FOV);
	// v4.5 : profil graphique ; des reglages d'une version precedente restent tels quels (profil PERSONNALISE)
	const bool bHadQuality = Cfg.GetInt(SettingsSection, TEXT("Quality"), S.Quality);
	if (!Cfg.GetInt(SettingsSection, TEXT("GraphicsProfile"), S.GraphicsProfile))
	{
		S.GraphicsProfile = bHadQuality ? 3 : 1;
	}
	Cfg.GetBool(SettingsSection, TEXT("RTShadows"), S.bRTShadows);
	Cfg.GetBool(SettingsSection, TEXT("HardwareRT"), S.bHardwareRT);
	Cfg.GetBool(SettingsSection, TEXT("RTHitLighting"), S.bRTHitLighting);
	Cfg.GetBool(SettingsSection, TEXT("AreaLights"), S.bAreaLights);
	Cfg.GetBool(SettingsSection, TEXT("FullCreatures"), S.bFullCreatures);
	Cfg.GetBool(SettingsSection, TEXT("VolumetricFog"), S.bVolumetricFog);
	Cfg.GetBool(SettingsSection, TEXT("FilmGrain"), S.bFilmGrain);
	Cfg.GetBool(SettingsSection, TEXT("VHSEffect"), S.bVHSEffect);
	Cfg.GetBool(SettingsSection, TEXT("DevMode"), S.bDevMode);
	Cfg.GetFloat(SettingsSection, TEXT("MasterVolume"), S.MasterVolume);
	Cfg.GetInt(SettingsSection, TEXT("VoiceMode"), S.VoiceMode);
	Cfg.GetFloat(SettingsSection, TEXT("Brightness"), S.Brightness);
	Cfg.GetInt(SettingsSection, TEXT("WindowMode"), S.WindowMode);
	Cfg.GetInt(SettingsSection, TEXT("RenderScale"), S.RenderScale);
	Cfg.GetBool(SettingsSection, TEXT("VSync"), S.bVSync);
	Cfg.GetInt(SettingsSection, TEXT("MaxFPS"), S.MaxFPS);
	Cfg.GetBool(SettingsSection, TEXT("HeadBob"), S.bHeadBob);
	Cfg.GetFloat(SettingsSection, TEXT("CameraShake"), S.CameraShake);
	Cfg.GetInt(SettingsSection, TEXT("Flashes"), S.Flashes);
	Cfg.GetBool(SettingsSection, TEXT("MotionBlur"), S.bMotionBlur);
	S.CameraShake = FMath::Clamp(S.CameraShake, 0.f, 1.f);
	S.Flashes = FMath::Clamp(S.Flashes, 0, 2);
	S.MasterVolume = FMath::Clamp(S.MasterVolume, 0.f, 1.f);
	S.VoiceMode = FMath::Clamp(S.VoiceMode, 0, 2);
	S.Brightness = FMath::Clamp(S.Brightness, -1.5f, 1.5f);
	S.WindowMode = FMath::Clamp(S.WindowMode, 0, 2);
	S.RenderScale = FMath::Clamp(S.RenderScale, 50, 100);
	S.MaxFPS = FMath::Clamp(S.MaxFPS, 0, 1000);
	S.Sensitivity = FMath::Clamp(S.Sensitivity, 0.1f, 5.f);
	S.FOV = FMath::Clamp(S.FOV, 70.f, 110.f);
	S.Quality = FMath::Clamp(S.Quality, 0, 4);
	S.GraphicsProfile = FMath::Clamp(S.GraphicsProfile, 0, 4);
	if (S.GraphicsProfile != 3)
	{
		ApplyGraphicsProfile(S.GraphicsProfile);
	}
}

void ABRPlayerController::ApplyGraphicsProfile(int32 Profile)
{
	// Choix par besoin : le hit lighting (le plus cher) et les ombres ray tracees de la lampe sont reserves au profil
	// Cinematique ; Qualite vise 60 images/s (objectif, a mesurer) avec Lumen en ray tracing materiel et TSR a 80 %
	FBRSettings& S = FBRSettings::Get();
	switch (Profile)
	{
	case 0: // Performance
		S.Quality = 2; S.bHardwareRT = false; S.bRTHitLighting = false; S.bRTShadows = false; S.RenderScale = 67; S.bAreaLights = false;
		S.bVolumetricFog = true; S.bFullCreatures = false;
		break;
	case 2: // Cinematique
		S.Quality = 4; S.bHardwareRT = true; S.bRTHitLighting = true; S.bRTShadows = true; S.RenderScale = 100; S.bAreaLights = true;
		S.bVolumetricFog = true; S.bFullCreatures = true;
		break;
	case 4: // v4.8 : RTX fluide : ray tracing materiel sans les deux reglages les plus chers (hit lighting, ombres RT de la
		// lampe), TSR depuis 67 % (1440p -> 2160p : 1707x960 en 1440p), ombres des neons jusqu'a 25 m, Hound allege
		S.Quality = 3; S.bHardwareRT = true; S.bRTHitLighting = false; S.bRTShadows = false; S.RenderScale = 67; S.bAreaLights = true;
		S.bVolumetricFog = true; S.bFullCreatures = false;
		break;
	default: // Qualite
		S.Quality = 3; S.bHardwareRT = true; S.bRTHitLighting = false; S.bRTShadows = false; S.RenderScale = 80; S.bAreaLights = true;
		S.bVolumetricFog = true; S.bFullCreatures = false;
		break;
	}
	S.GraphicsProfile = (Profile == 4 || (Profile >= 0 && Profile <= 2)) ? Profile : 1;
}

bool ABRPlayerController::IsHardwareRayTracingAvailable()
{
	// Le ray tracing ne s'active qu'au demarrage (RHI DirectX 12, carte compatible, r.RayTracing=True dans la config)
	return IsRayTracingEnabled();
}

FString ABRPlayerController::GetRenderModeText(bool bShort) const
{
	auto CVarF = [](const TCHAR* Name, float Default)
	{
		IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
		return V ? V->GetFloat() : Default;
	};
	const FString RHIName = GDynamicRHI ? FString(GDynamicRHI->GetName()) : FString(TEXT("?"));
	const bool bSM6 = GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM6;
	const bool bRTOn = IsHardwareRayTracingAvailable();
	const bool bLumenHW = bRTOn && CVarF(TEXT("r.Lumen.HardwareRayTracing"), 0.f) > 0.5f;
	const bool bHit = bLumenHW && CVarF(TEXT("r.Lumen.HardwareRayTracing.LightingMode"), 0.f) > 0.5f;
	const bool bVSM = CVarF(TEXT("r.Shadow.Virtual.Enable"), 0.f) > 0.5f;
	const bool bLampRT = bRTOn && FBRSettings::Get().bRTShadows;
	const float SP = FMath::Clamp(CVarF(TEXT("r.ScreenPercentage"), 100.f), 10.f, 200.f);
	FIntPoint VP(0, 0);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		VP = GEngine->GameViewport->Viewport->GetSizeXY();
	}
	const FIntPoint In(FMath::RoundToInt(VP.X * SP / 100.f), FMath::RoundToInt(VP.Y * SP / 100.f));
	// v4.8 : affichage honnete : methode d'agrandissement reelle et resolution dynamique eventuelle
	const int32 AA = FMath::RoundToInt(CVarF(TEXT("r.AntiAliasingMethod"), 4.f));
	const TCHAR* Upscaler = AA == 4 ? TEXT("TSR") : (AA == 2 ? TEXT("TAA") : (AA == 1 ? TEXT("FXAA") : TEXT("sans AA")));
	const bool bDynRes = CVarF(TEXT("r.DynamicRes.OperationMode"), 0.f) > 0.5f;
	if (bShort)
	{
		return FString::Printf(TEXT("%s  \u00b7  %s  \u00b7  %dx%d \u2192 %dx%d %s%s"), *RHIName,
			bLumenHW ? (bHit ? TEXT("RT + HIT LIGHTING") : TEXT("LUMEN RT")) : TEXT("LUMEN LOGICIEL"), In.X, In.Y, VP.X, VP.Y, Upscaler,
			bDynRes ? TEXT(" dyn.") : TEXT(""));
	}
	return FString::Printf(TEXT("Mode r\u00e9el : %s %s  \u00b7  ray tracing mat\u00e9riel %s  \u00b7  Lumen %s (reflets : %s)  \u00b7  ombres : %s%s  \u00b7  ")
		TEXT("rendu %dx%d \u2192 %dx%d (%d %%, %s%s)"),
		*RHIName, bSM6 ? TEXT("SM6") : TEXT("SM5"), bRTOn ? TEXT("actif") : TEXT("indisponible"), bLumenHW ? TEXT("mat\u00e9riel") : TEXT("logiciel"),
		bHit ? TEXT("\u00e9clair\u00e9s par les rayons") : TEXT("cache de surfaces"), bVSM ? TEXT("virtuelles (VSM)") : TEXT("cartes classiques"),
		bLampRT ? TEXT(", lampe ray trac\u00e9e") : TEXT(""), In.X, In.Y, VP.X, VP.Y, FMath::RoundToInt(SP), Upscaler,
		bDynRes ? TEXT(", r\u00e9solution dynamique") : TEXT(""));
}

void ABRPlayerController::SaveSettings() const
{
	FConfigFile& Cfg = BRConfig::Get();
	const FBRSettings& S = FBRSettings::Get();
	Cfg.SetFloat(SettingsSection, TEXT("Sensitivity"), S.Sensitivity);
	Cfg.SetBool(SettingsSection, TEXT("InvertY"), S.bInvertY);
	Cfg.SetFloat(SettingsSection, TEXT("FOV"), S.FOV);
	Cfg.SetInt64(SettingsSection, TEXT("Quality"), S.Quality);
	Cfg.SetInt64(SettingsSection, TEXT("GraphicsProfile"), S.GraphicsProfile);
	Cfg.SetBool(SettingsSection, TEXT("RTShadows"), S.bRTShadows);
	Cfg.SetBool(SettingsSection, TEXT("HardwareRT"), S.bHardwareRT);
	Cfg.SetBool(SettingsSection, TEXT("RTHitLighting"), S.bRTHitLighting);
	Cfg.SetBool(SettingsSection, TEXT("AreaLights"), S.bAreaLights);
	Cfg.SetBool(SettingsSection, TEXT("FullCreatures"), S.bFullCreatures);
	Cfg.SetBool(SettingsSection, TEXT("VolumetricFog"), S.bVolumetricFog);
	Cfg.SetBool(SettingsSection, TEXT("FilmGrain"), S.bFilmGrain);
	Cfg.SetBool(SettingsSection, TEXT("VHSEffect"), S.bVHSEffect);
	Cfg.SetBool(SettingsSection, TEXT("DevMode"), S.bDevMode);
	Cfg.SetFloat(SettingsSection, TEXT("MasterVolume"), S.MasterVolume);
	Cfg.SetInt64(SettingsSection, TEXT("VoiceMode"), S.VoiceMode);
	Cfg.SetFloat(SettingsSection, TEXT("Brightness"), S.Brightness);
	Cfg.SetInt64(SettingsSection, TEXT("WindowMode"), S.WindowMode);
	Cfg.SetInt64(SettingsSection, TEXT("RenderScale"), S.RenderScale);
	Cfg.SetBool(SettingsSection, TEXT("VSync"), S.bVSync);
	Cfg.SetInt64(SettingsSection, TEXT("MaxFPS"), S.MaxFPS);
	Cfg.SetBool(SettingsSection, TEXT("HeadBob"), S.bHeadBob);
	Cfg.SetFloat(SettingsSection, TEXT("CameraShake"), S.CameraShake);
	Cfg.SetInt64(SettingsSection, TEXT("Flashes"), S.Flashes);
	Cfg.SetBool(SettingsSection, TEXT("MotionBlur"), S.bMotionBlur);
	BRConfig::Save();
}

void ABRPlayerController::ApplySettings()
{
	const FBRSettings& S = FBRSettings::Get();
	UWorld* W = GetWorld();
	if (!GEngine || !W)
	{
		return;
	}
	auto Cmd = [W](const FString& Line)
	{
		GEngine->Exec(W, *Line);
	};
	Cmd(FString::Printf(TEXT("scalability %d"), FMath::Clamp(S.Quality, 0, 4)));
	// Le ray tracing materiel n'est utilise que si la carte le supporte (r.RayTracing=True dans DefaultEngine.ini)
	Cmd(FString::Printf(TEXT("r.Lumen.HardwareRayTracing %d"), S.bHardwareRT ? 1 : 0));
	// v4.5 : reflets eclaires par les rayons (hit lighting) seulement si demandes (profil Cinematique) : c'est le reglage le
	// plus couteux ; sinon le cache de surfaces de Lumen eclaire les reflets
	const bool bHitLighting = S.bHardwareRT && S.bRTHitLighting;
	Cmd(FString::Printf(TEXT("r.Lumen.HardwareRayTracing.LightingMode %d"), bHitLighting ? 1 : 0));
	// Reflets de premier plan de l'eau translucide (Poolrooms) a partir de la qualite Epique
	Cmd(FString::Printf(TEXT("r.Lumen.TranslucencyReflections.FrontLayer.Enable %d"), S.Quality >= 3 ? 1 : 0));
	// Ombres ray tracees : jamais globales (r.RayTracing.Shadows reste a 0), seulement la lampe torche, au cas par cas
	Cmd(TEXT("r.RayTracing.Shadows 0"));
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->SetFlashlightRayTracedShadows(S.bRTShadows && IsHardwareRayTracingAvailable());
	}
	// Les flaques mouillees (rugosite 0,1 a 0,3) restent tracees, pas seulement les miroirs
	Cmd(FString::Printf(TEXT("r.Lumen.Reflections.MaxRoughnessToTrace %.2f"), S.Quality >= 3 ? 0.5f : 0.4f));
	Cmd(FString::Printf(TEXT("r.Lumen.Reflections.HardwareRayTracing.Translucent.Refraction %d"), S.bHardwareRT ? 1 : 0));
	Cmd(FString::Printf(TEXT("r.VolumetricFog %d"), S.bVolumetricFog ? 1 : 0));
	Cmd(FString::Printf(TEXT("r.ScreenPercentage %d"), FMath::Clamp(S.RenderScale, 50, 100)));
	// Image plus nette (filtre de nettete du tonemapper) a partir de la qualite Elevee
	Cmd(FString::Printf(TEXT("r.Tonemapper.Sharpen %.2f"), S.Quality >= 2 ? 0.5f : 0.25f));
	// v4.8 : sans hit lighting, le cache de surfaces de Lumen n'eclaire pas les maillages a squelette : dans les reflets
	// ray traces, les entites et la combinaison devenaient noires. Ils ne sont dans la scene ray tracee que si les
	// reflets sont eclaires par les rayons, ou pour les ombres ray tracees de la lampe ; sinon les reflets les prennent a
	// l'ecran (traces d'ecran de Lumen)
	Cmd(FString::Printf(TEXT("r.RayTracing.Geometry.SkeletalMeshes %d"), (bHitLighting || S.bRTShadows) ? 1 : 0));
	// v4.8 : TSR : historique a 100 % (au lieu de 200 % en qualite Cinematique) hors profil Cinematique : a 1440p et
	// au-dela, c'est l'un des postes les plus chers du TSR, pour un gain de nettete faible
	Cmd(FString::Printf(TEXT("r.TSR.History.ScreenPercentage %d"), S.GraphicsProfile == 2 ? 200 : 100));

	// Volume general
	FAudioDeviceHandle Audio = W->GetAudioDevice();
	if (Audio.IsValid())
	{
		Audio->SetTransientPrimaryVolume(FMath::Clamp(S.MasterVolume, 0.f, 1.f));
	}

	// Fenetre, synchro verticale, limite d'images : pas dans l'editeur (ils agiraient sur la fenetre de l'editeur)
	UGameUserSettings* Display = GEngine->GetGameUserSettings();
	if (Display && !GIsEditor)
	{
		const EWindowMode::Type Mode = S.WindowMode == 0 ? EWindowMode::Fullscreen : (S.WindowMode == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
		bool bResolution = false;
		if (Display->GetFullscreenMode() != Mode)
		{
			Display->SetFullscreenMode(Mode);
			if (Mode != EWindowMode::Windowed)
			{
				Display->SetScreenResolution(Display->GetDesktopResolution());
			}
			bResolution = true;
		}
		Display->SetVSyncEnabled(S.bVSync);
		Display->SetFrameRateLimit(static_cast<float>(FMath::Max(0, S.MaxFPS)));
		if (bResolution)
		{
			Display->ApplyResolutionSettings(false);
		}
		Display->SaveSettings();
		Cmd(FString::Printf(TEXT("r.VSync %d"), S.bVSync ? 1 : 0));
		Cmd(FString::Printf(TEXT("t.MaxFPS %d"), FMath::Max(0, S.MaxFPS)));
	}
	// v4.8 : ce qui est deja construit suit les reglages (type des lumieres, ombres au loin, detail des entites)
	if (ABRWorld* BW = ABRWorld::Get(this))
	{
		BW->OnGraphicsSettingsChanged();
	}
}

FIntPoint ABRPlayerController::InternalResolution()
{
	// v4.8 : resolution reellement calculee avant TSR : pourcentage de rendu (r.ScreenPercentage) et, s'il est actif, le
	// pourcentage secondaire de la fenetre de jeu ; resolution dynamique signalee a part (GetRenderModeText)
	IConsoleVariable* SP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	const float Pct = FMath::Clamp(SP ? SP->GetFloat() : 100.f, 10.f, 200.f);
	FIntPoint VP(0, 0);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		VP = GEngine->GameViewport->Viewport->GetSizeXY();
	}
	return FIntPoint(FMath::RoundToInt(VP.X * Pct / 100.f), FMath::RoundToInt(VP.Y * Pct / 100.f));
}
