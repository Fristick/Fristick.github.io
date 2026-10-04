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
		Row_Volume,
		Row_Voice,
		Row_Brightness,
		Row_WindowMode,
		Row_RenderScale,
		Row_VSync,
		Row_MaxFPS,
		Row_Quality,
		Row_HardwareRT,
		Row_RTHitLighting,
		Row_AreaLights,
		Row_VolumetricFog,
		Row_FilmGrain,
		Row_VHSEffect,
		Row_Count
	};

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
	WriteActiveSave(); // fermeture du jeu ou de l'editeur en pleine partie
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

	if (bPendingNewSave)
	{
		bPendingNewSave = false;
		StartNewSave();
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
	WriteActiveSave();
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
	// Sans partie choisie (tests, console), seul le Niveau 0 est propose
	return ActiveSave ? ActiveSave->IsExplored(LevelNumber) : LevelNumber == 0;
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
}

void ABRPlayerController::SelectSave(int32 Slot)
{
	UBRSaveGame* S = GetSaveInSlot(Slot);
	if (!S)
	{
		return;
	}
	ActiveSave = S;
	BRSaves::ActiveSlot() = Slot;
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
	if (!ActiveSave || BRSaves::ActiveSlot() == INDEX_NONE || bInMenu || !IsLocalController() || GetNetMode() == NM_Client)
	{
		return;
	}
	const bool bNew = !ActiveSave->IsExplored(LevelNumber);
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
	}
}

void ABRPlayerController::WriteActiveSave()
{
	if (!ActiveSave || BRSaves::ActiveSlot() == INDEX_NONE || bInMenu || !IsLocalController() || GetNetMode() == NM_Client)
	{
		return;
	}
	const ABRCharacter* C = GetBRCharacter();
	if (C && !C->IsDead())
	{
		C->WriteToSave(ActiveSave);
	}
	if (const ABRWorld* W = ABRWorld::Get(this))
	{
		ActiveSave->Discovered = W->GetDiscoveredList();
		if (W->IsLevelReady() && !W->IsTransitioning())
		{
			ActiveSave->CurrentLevel = W->GetLevelNumber();
			ActiveSave->MarkExplored(W->GetLevelNumber());
		}
	}
	if (BRSaves::Write(BRSaves::ActiveSlot(), ActiveSave))
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
	WriteActiveSave();
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
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->RequestTransition(TargetLevel);
	}
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
	if (Target != W->GetLevelNumber())
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
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		bInMenu = false;
		ShowAddressBox(false);
		UpdateInputMode();
		W->RequestTransition(Number);
	}
}

void ABRPlayerController::BRGod()
{
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->bGodMode = !C->bGodMode;
		ABRHUD::Notify(this, C->bGodMode ? TEXT("Mode invincible : ON") : TEXT("Mode invincible : OFF"), 2.f);
	}
}

void ABRPlayerController::BRSpawn(int32 Kind)
{
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
	case Row_Quality:
		return TEXT("QUALIT\u00c9 GRAPHIQUE");
	case Row_HardwareRT:
		return TEXT("RAY TRACING MAT\u00c9RIEL (RTX)");
	case Row_RTHitLighting:
		return TEXT("REFLETS RAY TRAC\u00c9S HAUTE QUALIT\u00c9");
	case Row_AreaLights:
		return TEXT("N\u00c9ONS EN LUMI\u00c8RES SURFACIQUES");
	case Row_VolumetricFog:
		return TEXT("BROUILLARD VOLUM\u00c9TRIQUE");
	case Row_FilmGrain:
		return TEXT("GRAIN DE L'IMAGE");
	case Row_VHSEffect:
		return TEXT("EFFET VHS");
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
	case Row_Volume:
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.MasterVolume * 100.f));
	case Row_Voice:
		return VoiceNames[FMath::Clamp(S.VoiceMode, 0, 2)];
	case Row_Brightness:
		return FString::Printf(TEXT("%+.1f"), S.Brightness);
	case Row_WindowMode:
		return WindowNames[FMath::Clamp(S.WindowMode, 0, 2)];
	case Row_RenderScale:
		return FString::Printf(TEXT("%d %%"), S.RenderScale);
	case Row_VSync:
		return OnOff(S.bVSync);
	case Row_MaxFPS:
		return S.MaxFPS <= 0 ? FString(TEXT("ILLIMIT\u00c9")) : FString::Printf(TEXT("%d"), S.MaxFPS);
	case Row_Quality:
		return QualityNames[FMath::Clamp(S.Quality, 0, 4)];
	case Row_HardwareRT:
		return OnOff(S.bHardwareRT);
	case Row_RTHitLighting:
		return OnOff(S.bRTHitLighting);
	case Row_AreaLights:
		return OnOff(S.bAreaLights);
	case Row_VolumetricFog:
		return OnOff(S.bVolumetricFog);
	case Row_FilmGrain:
		return OnOff(S.bFilmGrain);
	case Row_VHSEffect:
		return OnOff(S.bVHSEffect);
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
	case Row_Quality:
		return TEXT("Ombres, Lumen, textures, anti-cr\u00e9nelage (scalability).");
	case Row_HardwareRT:
		return TEXT("Lumen en ray tracing mat\u00e9riel : reflets et lumi\u00e8re indirecte bien plus pr\u00e9cis (carte RTX / RX 6000+ requise).");
	case Row_RTHitLighting:
		return TEXT("Reflets \u00e9clair\u00e9s par les rayons eux-m\u00eames (eau, flaques, carrelage). Toujours actif en qualit\u00e9 \u00c9pique et Cin\u00e9matique ; ici, forc\u00e9 dans toutes les qualit\u00e9s. Co\u00fbteux.");
	case Row_AreaLights:
		return TEXT("Ombres douces des n\u00e9ons. S'applique aux zones charg\u00e9es ensuite.");
	case Row_VolumetricFog:
		return TEXT("Halos de lumi\u00e8re dans l'air humide.");
	case Row_VHSEffect:
		return TEXT("Lignes de balayage, l\u00e9g\u00e8re aberration et salet\u00e9 d'objectif. D\u00e9sactiv\u00e9 : image nette.");
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
	case Row_Quality:
		S.Quality = (S.Quality + Dir + 5) % 5;
		break;
	case Row_HardwareRT:
		S.bHardwareRT = !S.bHardwareRT;
		break;
	case Row_RTHitLighting:
		S.bRTHitLighting = !S.bRTHitLighting;
		break;
	case Row_AreaLights:
		S.bAreaLights = !S.bAreaLights;
		break;
	case Row_VolumetricFog:
		S.bVolumetricFog = !S.bVolumetricFog;
		break;
	case Row_FilmGrain:
		S.bFilmGrain = !S.bFilmGrain;
		break;
	case Row_VHSEffect:
		S.bVHSEffect = !S.bVHSEffect;
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
	Cfg.GetInt(SettingsSection, TEXT("Quality"), S.Quality);
	Cfg.GetBool(SettingsSection, TEXT("HardwareRT"), S.bHardwareRT);
	Cfg.GetBool(SettingsSection, TEXT("RTHitLighting"), S.bRTHitLighting);
	Cfg.GetBool(SettingsSection, TEXT("AreaLights"), S.bAreaLights);
	Cfg.GetBool(SettingsSection, TEXT("VolumetricFog"), S.bVolumetricFog);
	Cfg.GetBool(SettingsSection, TEXT("FilmGrain"), S.bFilmGrain);
	Cfg.GetBool(SettingsSection, TEXT("VHSEffect"), S.bVHSEffect);
	Cfg.GetFloat(SettingsSection, TEXT("MasterVolume"), S.MasterVolume);
	Cfg.GetInt(SettingsSection, TEXT("VoiceMode"), S.VoiceMode);
	Cfg.GetFloat(SettingsSection, TEXT("Brightness"), S.Brightness);
	Cfg.GetInt(SettingsSection, TEXT("WindowMode"), S.WindowMode);
	Cfg.GetInt(SettingsSection, TEXT("RenderScale"), S.RenderScale);
	Cfg.GetBool(SettingsSection, TEXT("VSync"), S.bVSync);
	Cfg.GetInt(SettingsSection, TEXT("MaxFPS"), S.MaxFPS);
	Cfg.GetBool(SettingsSection, TEXT("HeadBob"), S.bHeadBob);
	S.MasterVolume = FMath::Clamp(S.MasterVolume, 0.f, 1.f);
	S.VoiceMode = FMath::Clamp(S.VoiceMode, 0, 2);
	S.Brightness = FMath::Clamp(S.Brightness, -1.5f, 1.5f);
	S.WindowMode = FMath::Clamp(S.WindowMode, 0, 2);
	S.RenderScale = FMath::Clamp(S.RenderScale, 50, 100);
	S.MaxFPS = FMath::Clamp(S.MaxFPS, 0, 1000);
	S.Sensitivity = FMath::Clamp(S.Sensitivity, 0.1f, 5.f);
	S.FOV = FMath::Clamp(S.FOV, 70.f, 110.f);
	S.Quality = FMath::Clamp(S.Quality, 0, 4);
}

void ABRPlayerController::SaveSettings() const
{
	FConfigFile& Cfg = BRConfig::Get();
	const FBRSettings& S = FBRSettings::Get();
	Cfg.SetFloat(SettingsSection, TEXT("Sensitivity"), S.Sensitivity);
	Cfg.SetBool(SettingsSection, TEXT("InvertY"), S.bInvertY);
	Cfg.SetFloat(SettingsSection, TEXT("FOV"), S.FOV);
	Cfg.SetInt64(SettingsSection, TEXT("Quality"), S.Quality);
	Cfg.SetBool(SettingsSection, TEXT("HardwareRT"), S.bHardwareRT);
	Cfg.SetBool(SettingsSection, TEXT("RTHitLighting"), S.bRTHitLighting);
	Cfg.SetBool(SettingsSection, TEXT("AreaLights"), S.bAreaLights);
	Cfg.SetBool(SettingsSection, TEXT("VolumetricFog"), S.bVolumetricFog);
	Cfg.SetBool(SettingsSection, TEXT("FilmGrain"), S.bFilmGrain);
	Cfg.SetBool(SettingsSection, TEXT("VHSEffect"), S.bVHSEffect);
	Cfg.SetFloat(SettingsSection, TEXT("MasterVolume"), S.MasterVolume);
	Cfg.SetInt64(SettingsSection, TEXT("VoiceMode"), S.VoiceMode);
	Cfg.SetFloat(SettingsSection, TEXT("Brightness"), S.Brightness);
	Cfg.SetInt64(SettingsSection, TEXT("WindowMode"), S.WindowMode);
	Cfg.SetInt64(SettingsSection, TEXT("RenderScale"), S.RenderScale);
	Cfg.SetBool(SettingsSection, TEXT("VSync"), S.bVSync);
	Cfg.SetInt64(SettingsSection, TEXT("MaxFPS"), S.MaxFPS);
	Cfg.SetBool(SettingsSection, TEXT("HeadBob"), S.bHeadBob);
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
	// Liquides "RTX" : en qualite Epique et au-dela, les reflets (flaques, eau, carrelage) sont eclaires par les rayons
	// eux-memes (hit lighting) et non par le cache de surfaces de Lumen ; l'option les force dans toutes les qualites
	const bool bHitLighting = S.bHardwareRT && (S.bRTHitLighting || S.Quality >= 3);
	Cmd(FString::Printf(TEXT("r.Lumen.HardwareRayTracing.LightingMode %d"), bHitLighting ? 1 : 0));
	// Les flaques mouillees (rugosite 0,1 a 0,3) restent tracees, pas seulement les miroirs
	Cmd(FString::Printf(TEXT("r.Lumen.Reflections.MaxRoughnessToTrace %.2f"), S.Quality >= 3 ? 0.5f : 0.4f));
	Cmd(FString::Printf(TEXT("r.Lumen.Reflections.HardwareRayTracing.Translucent.Refraction %d"), S.bHardwareRT ? 1 : 0));
	Cmd(FString::Printf(TEXT("r.VolumetricFog %d"), S.bVolumetricFog ? 1 : 0));
	Cmd(FString::Printf(TEXT("r.ScreenPercentage %d"), FMath::Clamp(S.RenderScale, 50, 100)));
	// Image plus nette (filtre de nettete du tonemapper) a partir de la qualite Elevee
	Cmd(FString::Printf(TEXT("r.Tonemapper.Sharpen %.2f"), S.Quality >= 2 ? 0.5f : 0.25f));

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
}
