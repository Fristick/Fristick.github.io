#include "BRPlayerController.h"
#include "BRMissionLogic.h"
#include "BRLoc.h"
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
#include "BRDisplay.h"
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
		Row_Language,
		Row_Sensitivity,
		Row_InvertY,
		Row_FOV,
		Row_HeadBob,
		Row_CameraShake,
		Row_Flashes,
		Row_MotionBlur,
		Row_Volume,
		Row_EffectsVolume,  // v4.11 : effets et ambiances
		Row_VoiceVolume,    // v4.11 : voix des coequipiers
		Row_Subtitles,      // v4.11 : sous-titres des sons utiles
		Row_Voice,
		Row_Brightness,
		Row_WindowMode,
		Row_Resolution,     // v4.9 : resolution de la fenetre ou de la sortie (separee de l'echelle de rendu)
		Row_RenderScale,
		Row_VSync,
		Row_MaxFPS,
		Row_Profile,
		Row_Quality,
		Row_HardwareRT,
		Row_RTHitLighting,
		Row_CreatureReflections, // v4.9 : creatures dans les reflets ray traces
		Row_RTShadows,
		Row_AreaLights,
		Row_FullCreatures,
		Row_VolumetricFog,
		Row_FilmGrain,
		Row_VHSEffect,
		// v4.9 : interface d'exploration
		Row_UiScale,
		Row_HudOpacity,
		Row_Crosshair,
		Row_QuickBar,
		Row_Objectives,
		Row_DevMode,
		Row_Count
	};

	FString ProfileNames(int32 Index)
	{
		const FString Items[] = { BR_STR(NSLOCTEXT("BR", "Menu.Performance", "PERFORMANCE")), BR_STR(NSLOCTEXT("BR", "Menu.Qualite", "QUALIT\u00c9")), BR_STR(NSLOCTEXT("BR", "Menu.Cinematique", "CIN\u00c9MATIQUE")), BR_STR(NSLOCTEXT("BR", "Menu.Personnalise", "PERSONNALIS\u00c9")), BR_STR(NSLOCTEXT("BR", "Menu.RtxFluide", "RTX FLUIDE")) };
		return Items[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(Items)) - 1)];
	}
	/** v4.8 : ordre des profils proposes (le profil PERSONNALISE ne se choisit pas : il vient d'un reglage modifie) */
	const int32 ProfileCycle[] = { 0, 1, 4, 2 };
	FString QualityNames(int32 Index)
	{
		const FString Items[] = { BR_STR(NSLOCTEXT("BR", "Menu.Bas", "BAS")), BR_STR(NSLOCTEXT("BR", "Menu.Moyen", "MOYEN")), BR_STR(NSLOCTEXT("BR", "Menu.Eleve", "\u00c9LEV\u00c9")), BR_STR(NSLOCTEXT("BR", "Menu.Epique", "\u00c9PIQUE")), BR_STR(NSLOCTEXT("BR", "Menu.Cinematique", "CIN\u00c9MATIQUE")) };
		return Items[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(Items)) - 1)];
	}
	FString VoiceNames(int32 Index)
	{
		const FString Items[] = { BR_STR(NSLOCTEXT("BR", "Menu.VoixOuverte", "VOIX OUVERTE")), BR_STR(NSLOCTEXT("BR", "Menu.AppuyerParler", "APPUYER POUR PARLER")), BR_STR(NSLOCTEXT("BR", "Menu.MicroCoupe", "MICRO COUP\u00c9")) };
		return Items[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(Items)) - 1)];
	}
	FString WindowNames(int32 Index)
	{
		const FString Items[] = { BR_STR(NSLOCTEXT("BR", "Menu.PleinEcran", "PLEIN \u00c9CRAN")), BR_STR(NSLOCTEXT("BR", "Menu.FenetreSansBordure", "FEN\u00caTR\u00c9 SANS BORDURE")), BR_STR(NSLOCTEXT("BR", "Menu.Fenetre", "FEN\u00caTR\u00c9")) };
		return Items[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(Items)) - 1)];
	}
	/** v4.9 : trois choix : contextuel (bref), toujours, masque */
	FString ShowNames(int32 Index)
	{
		const FString Items[] = { BR_STR(NSLOCTEXT("BR", "Menu.ShowContextual", "BRI\u00c8VEMENT")), BR_STR(NSLOCTEXT("BR", "Menu.ShowAlways", "TOUJOURS")), BR_STR(NSLOCTEXT("BR", "Menu.ShowHidden", "MASQU\u00c9S")) };
		return Items[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(Items)) - 1)];
	}
	FString CrosshairNames(int32 Index)
	{
		const FString Items[] = { BR_STR(NSLOCTEXT("BR", "Menu.CrosshairDot", "POINT")), BR_STR(NSLOCTEXT("BR", "Menu.CrosshairFocus", "SUR LES OBJETS")), BR_STR(NSLOCTEXT("BR", "Menu.Aucun", "AUCUN")) };
		return Items[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(Items)) - 1)];
	}
	/** v4.9 : relance en hit lighting des impacts sans cache de surfaces : variable propre a certaines versions du moteur,
	 *  verifiee a l'execution (aucune supposition sur la version installee) */
	const TCHAR* const RetraceHitLightingCVar = TEXT("r.Lumen.Reflections.HardwareRayTracing.Retrace.HitLighting");
	bool RetraceSupported()
	{
		return IConsoleManager::Get().FindConsoleVariable(RetraceHitLightingCVar) != nullptr;
	}
	const int32 FPSSteps[] = { 0, 30, 60, 90, 120, 144, 165, 240 };
	const int32 NumFPSSteps = UE_ARRAY_COUNT(FPSSteps);

	FString OnOff(bool b)
	{
		return b ? FString(BR_STR(NSLOCTEXT("BR", "Menu.Active", "ACTIV\u00c9"))) : FString(BR_STR(NSLOCTEXT("BR", "Menu.Desactive", "D\u00c9SACTIV\u00c9")));
	}

	// Erreurs reseau : la connexion echoue ou se perd, le moteur recharge la carte et le menu les affiche
	FString GNetMessage;
	bool GNetHooks = false;

	// v4.12 : refus explicite de l'hote (version ou contenu different), lu dans la langue de ce joueur
	FString DescribeJoinRefusal(const FString& Error)
	{
		bool bVersion = false;
		int32 HostNet = 0, HostChannel = 0, HostLot = 0;
		if (!BRLevels::ParseJoinRefusal(Error, bVersion, HostNet, HostChannel, HostLot))
		{
			return FString();
		}
		if (bVersion)
		{
			return BRLoc::Fmt(NSLOCTEXT("BR", "Menu.JoinRefusedVersion", "Impossible de rejoindre : l'h\u00f4te a la version {Host} du jeu, vous avez la version {Local}. Installez la m\u00eame version des deux c\u00f4t\u00e9s."),
				{ { TEXT("Host"), BRLoc::Arg(BRLevels::VersionLabel(HostNet)) }, { TEXT("Local"), BRLoc::Arg(BRLevels::VersionLabel(BRContent::NetVersion)) } });
		}
		const BRContent::EChannel Local = BRLevels::Channel();
		return BRLoc::Fmt(NSLOCTEXT("BR", "Menu.JoinRefusedContent", "Impossible de rejoindre : l'h\u00f4te joue avec la {HostChannel} (contenu jusqu'au lot {HostLot}), vous avez la {LocalChannel} (lot {LocalLot}). Les deux joueurs doivent avoir le m\u00eame contenu."),
			{ { TEXT("HostChannel"), BRLoc::Arg(BRLevels::ChannelName(static_cast<BRContent::EChannel>(HostChannel)).ToString()) }, { TEXT("HostLot"), BRLoc::Int(HostLot) },
				{ TEXT("LocalChannel"), BRLoc::Arg(BRLevels::ChannelName(Local).ToString()) }, { TEXT("LocalLot"), BRLoc::Int(BRContent::CurrentLot(Local)) } });
	}

	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
	{
		const FString Refusal = DescribeJoinRefusal(Error);
		if (!Refusal.IsEmpty())
		{
			GNetMessage = Refusal;
			return;
		}
		switch (Type)
		{
		case ENetworkFailure::OutdatedClient:
		case ENetworkFailure::OutdatedServer:
			// v4.12 : le moteur compare la version du projet (ProjectVersion) avant meme la demande d'entree ; il ne dit pas
			// laquelle des deux est la plus recente
			GNetMessage = BRLoc::Fmt(NSLOCTEXT("BR", "Menu.JoinOtherVersion", "Impossible de rejoindre : l'h\u00f4te a une autre version du jeu (vous avez la version {Local}). Installez la m\u00eame version des deux c\u00f4t\u00e9s."),
				{ { TEXT("Local"), BRLoc::Arg(BRLevels::VersionLabel(BRContent::NetVersion)) } });
			break;
		case ENetworkFailure::PendingConnectionFailure:
			if (Error.Contains(TEXT("Server full")))
			{
				GNetMessage = BR_STR(NSLOCTEXT("BR", "Menu.JoinServerFull", "Impossible de rejoindre : la partie est compl\u00e8te (4 joueurs)."));
				break;
			}
			GNetMessage = BR_STR(NSLOCTEXT("BR", "Menu.ImpossibleRejoindrePartieVerifiezAdresse", "Impossible de rejoindre la partie : v\u00e9rifiez l'adresse, le port 7777 (UDP) et le pare-feu de l'h\u00f4te."));
			break;
		case ENetworkFailure::ConnectionLost:
		case ENetworkFailure::ConnectionTimeout:
			GNetMessage = BR_STR(NSLOCTEXT("BR", "Menu.ConnexionPerdueHote", "Connexion perdue avec l'h\u00f4te."));
			break;
		case ENetworkFailure::NetDriverListenFailure:
			GNetMessage = BR_STR(NSLOCTEXT("BR", "Menu.ImpossibleHebergerPort7777Peut", "Impossible d'h\u00e9berger : le port 7777 est peut-\u00eatre d\u00e9j\u00e0 utilis\u00e9."));
			break;
		default:
			GNetMessage = BR_STR(NSLOCTEXT("BR", "Menu.ErreurReseau", "Erreur r\u00e9seau : ")) + Error;
			break;
		}
	}

	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
	{
		GNetMessage = BR_STR(NSLOCTEXT("BR", "Menu.ImpossibleRejoindrePartie", "Impossible de rejoindre la partie : ")) + Error;
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
	// v4.9 : affichage : UGameUserSettings seule source, retour a la configuration confirmee si le jeu s'est arrete avant
	BRDisplay::Startup();
	ApplySettings();
	if (BRDisplay::TakeRestoredAtStartup())
	{
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Display.RestoredAtStartup", "Affichage r\u00e9tabli : le changement pr\u00e9c\u00e9dent n'avait pas \u00e9t\u00e9 confirm\u00e9.")), 5.f, FLinearColor(1.f, 0.85f, 0.5f));
	}

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
	// v4.9 : confirmation de l'affichage (retour automatique a l'expiration) et changements faits hors du jeu
	if (IsLocalController() && BRDisplay::Tick())
	{
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Display.Reverted", "Affichage r\u00e9tabli (pas de confirmation).")), 4.f, FLinearColor(1.f, 0.85f, 0.5f));
	}
	// v4.10 : ouverture et fermeture du dialogue (conserver, retablir, expiration) : curseur et focus suivent ; a la fermeture,
	// l'etat d'avant revient (inventaire, menu ou jeu). Les touches tenues ne reprennent pas leur effet toutes seules.
	if (IsLocalController() && IsModalDialogOpen() != bModalWasOpen)
	{
		bModalWasOpen = IsModalDialogOpen();
		bTalkKeyHeld = false;
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->SetSprinting(false);
		}
		FlushPressedKeys();
		UpdateInputMode();
	}
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
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Menu.HebergezPartieVotrePcFait", "Vous h\u00e9bergez la partie : votre PC fait tourner le monde et les entit\u00e9s pour tout le groupe. Gardez le jeu ouvert jusqu'\u00e0 la fin.")),
				9.f, FLinearColor(1.f, 0.85f, 0.4f));
		}
		else
		{
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Menu.ConnectePartieHoteRestezGroupes", "Connect\u00e9 \u00e0 la partie de l'h\u00f4te. Restez group\u00e9s : on s'entend mieux de pr\u00e8s.")), 7.f,
				FLinearColor(0.75f, 1.f, 0.75f));
		}
		if (FBRSettings::Get().VoiceMode == 1)
		{
			ABRHUD::Notify(this, BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Menu.ChatVocalProximiteMaintenezPushtotalk", "Chat vocal de proximit\u00e9 : maintenez {PushToTalk} pour parler."))), 7.f, FLinearColor(0.75f, 0.9f, 1.f));
		}
	}
}

// =====================================================================================================================
// Chat vocal de proximite (VOIP du moteur : la voix de chacun sort de son personnage, etouffee par les murs)
// =====================================================================================================================

void ABRPlayerController::OnTalkStarted(const FInputActionValue& Value)
{
	bTalkKeyHeld = CaptureAction == INDEX_NONE && !IsModalDialogOpen();
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
	ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Menu.TouchesDefautRetablies", "Touches par d\u00e9faut r\u00e9tablies.")), 2.5f);
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
			ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.KRetireeRemoved", "{K} retir\u00e9e de : {Removed}"), { { TEXT("K"), BRLoc::Arg(BRKeys::KeyName(K)) }, { TEXT("Removed"), BRLoc::Arg(Removed) } }), 3.f,
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

bool ABRPlayerController::IsModalDialogOpen() const
{
	// v4.11 : l'ecran de fin de campagne est aussi un dialogue (curseur, actions de jeu suspendues)
	const ABRWorld* W = ABRWorld::Get(this);
	return BRDisplay::IsPending() || (W && W->IsEndingShown());
}

void ABRPlayerController::OnEndingShown()
{
	// La fin est enregistree tout de suite (campagne, fins vues)
	WriteActiveSave();
	UpdateInputMode();
}

void ABRPlayerController::CloseEnding(bool bContinue)
{
	ABRWorld* W = ABRWorld::Get(this);
	if (W)
	{
		W->CloseEnding(bContinue);
	}
	UpdateInputMode();
	if (!bContinue)
	{
		ReturnToMainMenu();
	}
}

bool ABRPlayerController::CanPlay() const
{
	// v4.10 : la confirmation de l'affichage bloque aussi les actions de jeu (avant : seulement ses propres touches)
	return !bInMenu && !bPauseMenu && !bInventory && CaptureAction == INDEX_NONE && !IsModalDialogOpen();
}

void ABRPlayerController::UpdateInputMode()
{
	// Curseur visible dans le menu principal, l'inventaire, le menu pause et les dialogues (boutons cliquables)
	const bool bCursor = bInventory || bPauseMenu || bInMenu || IsModalDialogOpen();
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
	// v4.9 : touches purgees a l'ouverture et a la fermeture : une touche restee enfoncee (sprint, deplacement, action)
	// ne reprend pas son effet toute seule ; il faut l'appuyer de nouveau
	FlushPressedKeys();
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
	if (IsModalDialogOpen())
	{
		return; // v4.10 : Tab ne ferme plus l'inventaire sous la confirmation de l'affichage
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
	// v4.9 : en partie, l'inventaire s'ouvre toujours sur l'onglet Personnage (objets et les deux jauges), pas sur
	// l'onglet de la visite precedente
	SetInventoryOpen(!bInventory, bInventory ? INDEX_NONE : 0);
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
	if (BRDisplay::IsPending())
	{
		BRDisplay::Revert(); // v4.9 : Echap pendant la confirmation de l'affichage : retour immediat
		return;
	}
	if (const ABRWorld* W = ABRWorld::Get(this); W && W->IsPreparing())
	{
		// v4.10 : preparation d'un niveau trop longue : retour au menu principal (jamais d'ecran noir sans issue)
		if (W->GetPrepareTime() >= ABRWorld::PrepareMenuDelay)
		{
			ReturnToMainMenu();
		}
		return;
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
	if ((bPauseMenu || bInMenu) && CaptureAction == INDEX_NONE && !IsModalDialogOpen())
	{
		QuitToDesktop();
	}
}

void ABRPlayerController::OnMenuPrev(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory && !IsModalDialogOpen())
	{
		if (MenuPage == EBRMenuPage::Language)
		{
			// Deux colonnes de langues : gauche / droite change de colonne
			const int32 Half = (BRLoc::Languages().Num() + 1) / 2;
			SetMenuCursor(MenuCursor >= Half && MenuCursor < BRLoc::Languages().Num() ? MenuCursor - Half : MenuCursor);
			return;
		}
		MenuShiftLevel(-1);
	}
}

void ABRPlayerController::OnMenuNext(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory && !IsModalDialogOpen())
	{
		if (MenuPage == EBRMenuPage::Language)
		{
			const int32 Half = (BRLoc::Languages().Num() + 1) / 2;
			SetMenuCursor(MenuCursor < Half ? FMath::Min(MenuCursor + Half, BRLoc::Languages().Num() - 1) : MenuCursor);
			return;
		}
		MenuShiftLevel(1);
	}
}

void ABRPlayerController::OnMenuUp(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory && !IsModalDialogOpen())
	{
		SetMenuCursor((MenuCursor + GetMenuItemCount() - 1) % FMath::Max(1, GetMenuItemCount()));
	}
}

void ABRPlayerController::OnMenuDown(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory && !IsModalDialogOpen())
	{
		SetMenuCursor((MenuCursor + 1) % FMath::Max(1, GetMenuItemCount()));
	}
}

void ABRPlayerController::OnMenuConfirm(const FInputActionValue& Value)
{
	if (BRDisplay::IsPending())
	{
		BRDisplay::Confirm(); // v4.9 : Entree pendant la confirmation de l'affichage : conserver
		return;
	}
	if (bInMenu && !bInventory)
	{
		MenuActivate(MenuCursor);
	}
}

void ABRPlayerController::OnMenuDelete(const FInputActionValue& Value)
{
	if (bInMenu && !bInventory && MenuPage == EBRMenuPage::Saves && !bConfirmDelete && !IsModalDialogOpen())
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
		return 5;
	case EBRMenuPage::Language:
		return BRLoc::Languages().Num() + 1;
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
	ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Menu.CommandeTestDesactiveeModeDeveloppeur", "Commande de test d\u00e9sactiv\u00e9e (mode d\u00e9veloppeur requis, absent des versions publi\u00e9es).")), 3.f);
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
	ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.ModeDevNiveauNumberTitle", "MODE D\u00c9V : Niveau {Number} - {Title}"), { { TEXT("Number"), BRLoc::Int(Next.Number) }, { TEXT("Title"), BRLoc::Arg(Next.Title) } }), 3.f, FLinearColor(0.6f, 0.9f, 1.f));
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
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Menu.ModeDevObjectifsRemplis", "MODE D\u00c9V : objectifs remplis")), 2.f, FLinearColor(0.6f, 0.9f, 1.f));
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
		ABRHUD::Notify(this, C->IsDevFlying() ? BR_STR(NSLOCTEXT("BR", "Menu.ModeDevVolLibreTravers", "MODE D\u00c9V : vol libre (\u00e0 travers les murs, Espace pour monter, Maj pour acc\u00e9l\u00e9rer)"))
			: BR_STR(NSLOCTEXT("BR", "Menu.ModeDevVolLibreCoupe", "MODE D\u00c9V : vol libre coup\u00e9")), 3.f, FLinearColor(0.6f, 0.9f, 1.f));
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
	const FString Main[] = { BR_STR(NSLOCTEXT("BR", "Menu.Solo", "SOLO")), BR_STR(NSLOCTEXT("BR", "Menu.Multijoueur", "MULTIJOUEUR")), BR_STR(NSLOCTEXT("BR", "Menu.Parametres", "PARAM\u00c8TRES")), BR_STR(NSLOCTEXT("BR", "Menu.Language", "LANGUE")), BR_STR(NSLOCTEXT("BR", "Menu.Quitter", "QUITTER")) };
	const FString Multi[] = { BR_STR(NSLOCTEXT("BR", "Menu.HebergerPartie", "H\u00c9BERGER UNE PARTIE")), BR_STR(NSLOCTEXT("BR", "Menu.RejoindrePartie", "REJOINDRE UNE PARTIE")), BR_STR(NSLOCTEXT("BR", "Menu.Retour", "RETOUR")) };
	switch (MenuPage)
	{
	case EBRMenuPage::Main:
		return Main[Item];
	case EBRMenuPage::Language:
		// Noms natifs (jamais traduits) : chacun reconnait sa langue
		return BRLoc::Languages().IsValidIndex(Item) ? FString(BRLoc::Languages()[Item].NativeName) : BR_STR(NSLOCTEXT("BR", "Menu.Retour", "RETOUR"));
	case EBRMenuPage::Multi:
		return Multi[Item];
	case EBRMenuPage::Join:
		return Item == 0 ? BR_STR(NSLOCTEXT("BR", "Menu.Connecter", "SE CONNECTER")) : BR_STR(NSLOCTEXT("BR", "Menu.Retour", "RETOUR"));
	case EBRMenuPage::NewSave:
		return Item == 0 ? (bHostFlow ? BR_STR(NSLOCTEXT("BR", "Menu.Heberger", "H\u00c9BERGER")) : BR_STR(NSLOCTEXT("BR", "Menu.Commencer", "COMMENCER"))) : BR_STR(NSLOCTEXT("BR", "Menu.Retour", "RETOUR"));
	case EBRMenuPage::Solo:
	{
		if (Item == 1)
		{
			return BR_STR(NSLOCTEXT("BR", "Menu.Retour", "RETOUR"));
		}
		const TArray<FBRLevelDef>& All = BRLevels::All();
		const int32 Level = All[FMath::Clamp(MenuIndex, 0, All.Num() - 1)].Number;
		if (!IsLevelUnlocked(Level))
		{
			return BR_STR(NSLOCTEXT("BR", "Menu.Verrouille", "VERROUILL\u00c9"));
		}
		return bHostFlow ? BR_STR(NSLOCTEXT("BR", "Menu.Heberger", "H\u00c9BERGER")) : BR_STR(NSLOCTEXT("BR", "Menu.Noclipper", "NOCLIPPER"));
	}
	case EBRMenuPage::Saves:
	{
		if (bConfirmDelete)
		{
			return Item == 0 ? BR_STR(NSLOCTEXT("BR", "Menu.OuiSupprimer", "OUI, SUPPRIMER")) : BR_STR(NSLOCTEXT("BR", "Menu.Annuler", "ANNULER"));
		}
		const int32 Slot = GetMenuSaveSlot(Item);
		if (Slot >= 0)
		{
			const UBRSaveGame* S = GetSaveInSlot(Slot);
			return S ? S->SaveName : FString();
		}
		return Slot == MenuItemNew ? BR_STR(NSLOCTEXT("BR", "Menu.NouvellePartie", "NOUVELLE PARTIE")) : BR_STR(NSLOCTEXT("BR", "Menu.Retour", "RETOUR"));
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
		NewSaveName = BRLoc::Fmt(NSLOCTEXT("BR", "Menu.PartieFree", "Partie {Free}"), { { TEXT("Free"), BRLoc::Int(Free == INDEX_NONE ? 1 : Free + 1) } });
	}
	if (Page == EBRMenuPage::Language)
	{
		MenuCursor = FMath::Max(0, BRLoc::CurrentIndex());
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
	case EBRMenuPage::Language:
		SetMenuPage(EBRMenuPage::Main);
		MenuCursor = 3;
		break;
	default:
		break;
	}
}

void ABRPlayerController::ChooseLanguage(int32 Index)
{
	if (!BRLoc::Languages().IsValidIndex(Index))
	{
		return;
	}
	const FString Code = BRLoc::Languages()[Index].Code;
	if (BRLoc::SetLanguage(Code))
	{
		BRLoc::SavePreference(Code);
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.LanguageChanged", "Langue : {Language}"),
			{ { TEXT("Language"), BRLoc::Arg(BRLoc::Languages()[Index].NativeName) } }), 3.f, FLinearColor(0.85f, 0.95f, 1.f));
	}
	else
	{
		PlayMenuSound(TEXT("S_UIDeny"), 0.6f);
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
			ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.NiveauNumberEncoreExploreCette", "Niveau {Number} : pas encore explor\u00e9 dans cette partie. Trouvez une sortie qui y m\u00e8ne."), { { TEXT("Number"), BRLoc::Int(D.Number) } }), 4.f,
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
		else if (Item == 3)
		{
			SetMenuPage(EBRMenuPage::Language);
		}
		else
		{
			QuitToDesktop();
		}
		break;
	case EBRMenuPage::Language:
		if (BRLoc::Languages().IsValidIndex(Item))
		{
			ChooseLanguage(Item);
		}
		else
		{
			SetMenuPage(EBRMenuPage::Main);
			MenuCursor = 3;
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
				ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.PartieNameSupprimee", "Partie \u00ab {Name} \u00bb supprim\u00e9e."), { { TEXT("Name"), BRLoc::Arg(Name) } }), 3.f, FLinearColor(1.f, 0.7f, 0.55f));
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
	// Message compose ici, dans la langue du joueur, d'apres la cause (le fil d'ecriture ne compose aucun texte)
	FText Cause;
	switch (F.Error)
	{
	case EBRSaveError::Replace:
		Cause = BRLoc::FmtText(NSLOCTEXT("BR", "Save.Error.Replace", "le fichier {File}.sav n'a pas pu \u00eatre remplac\u00e9 (ouvert par un autre programme ?)"),
			{ { TEXT("File"), BRLoc::Arg(F.File) } });
		break;
	case EBRSaveError::FutureFormat:
		Cause = BRLoc::FmtText(NSLOCTEXT("BR", "Save.Error.FutureFormat", "partie d'une version plus r\u00e9cente du jeu (format {Format}) : r\u00e9\u00e9criture refus\u00e9e, fichier pr\u00e9serv\u00e9"),
			{ { TEXT("Format"), BRLoc::Int(F.Version) } });
		break;
	default:
		Cause = BRLoc::FmtText(NSLOCTEXT("BR", "Save.Error.TempWrite", "\u00e9criture impossible de {File} (disque plein ou dossier prot\u00e9g\u00e9 ?)"),
			{ { TEXT("File"), BRLoc::Arg(F.File) } });
		break;
	}
	ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Save.Error.Notice", "\u00c9chec de la sauvegarde (emplacement {Slot}, demande n\u00b0 {Request}) : {Cause}. La partie continue ; nouvel essai \u00e0 la prochaine sauvegarde."),
		{ { TEXT("Slot"), BRLoc::Int(F.Slot + 1) }, { TEXT("Request"), BRLoc::Int(F.RequestId) }, { TEXT("Cause"), BRLoc::Arg(Cause) } }), 9.f, FLinearColor(1.f, 0.5f, 0.4f));
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
	ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Menu.DerniereSessionArreteePendantMort", "La derni\u00e8re session s'est arr\u00eat\u00e9e pendant une mort : vous repartez avec l'\u00e9quipement de d\u00e9part.")), 7.f,
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
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.CettePartieVientVersionRecente", "Cette partie vient d'une version plus r\u00e9cente du jeu (format {LoadedVersion}) : elle reste intacte et ne peut pas \u00eatre reprise ici."), { { TEXT("LoadedVersion"), BRLoc::Int(S->LoadedVersion) } }), 6.f, FLinearColor(1.f, 0.7f, 0.45f));
		PlayMenuSound(TEXT("S_UIDeny"), 0.5f);
		return;
	}
	ActiveSave = S;
	BRSaves::ActiveSlot() = Slot;
	ResolvePendingDeath(S);
	// On reprend la ou on s'etait arrete
	bool bMoved = false;
	const int32 Level = BRLevels::ResumeLevel(S->CurrentLevel, S->Explored, &bMoved);
	if (bMoved)
	{
		// v4.12 : partie d'une version de test (niveau pas encore publie ici) : le fichier est copie tel quel avant toute
		// ecriture, la partie garde ses niveaux, decouvertes et fins, et reprend au dernier niveau disponible explore
		const bool bCopied = BRSaves::PreserveBeforeRecovery(Slot);
		const FText Msg = bCopied
			? NSLOCTEXT("BR", "Menu.SaveLevelNotAvailableCopy", "Le Niveau {Level} de cette partie n'est pas encore disponible dans cette version : reprise au Niveau {Resume}. Vos niveaux, d\u00e9couvertes et fins sont conserv\u00e9s, et une copie de la partie est gard\u00e9e.")
			: NSLOCTEXT("BR", "Menu.SaveLevelNotAvailable", "Le Niveau {Level} de cette partie n'est pas encore disponible dans cette version : reprise au Niveau {Resume}. Vos niveaux, d\u00e9couvertes et fins sont conserv\u00e9s.");
		ABRHUD::Notify(this, BRLoc::Fmt(Msg, { { TEXT("Level"), BRLoc::Int(S->CurrentLevel) }, { TEXT("Resume"), BRLoc::Int(Level) } }), 8.f, FLinearColor(1.f, 0.82f, 0.5f));
		UE_LOG(LogBackrooms, Log, TEXT("[Content] Partie %d : niveau %d indisponible (canal %d), reprise au niveau %d, copie %s"), Slot, S->CurrentLevel,
			static_cast<int32>(BRLevels::Channel()), Level, bCopied ? TEXT("oui") : TEXT("non"));
	}
	SetMenuPage(EBRMenuPage::Solo);
	MenuIndex = FMath::Max(0, BRLevels::IndexOf(Level));
}

void ABRPlayerController::StartNewSave()
{
	const int32 Slot = BRSaves::FreeSlot();
	if (Slot == INDEX_NONE)
	{
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Menu.6EmplacementsSontOccupesSupprimez", "Les 6 emplacements sont occup\u00e9s : supprimez une partie (touche Suppr).")), 5.f, FLinearColor(1.f, 0.6f, 0.45f));
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
		Name = BRLoc::Fmt(NSLOCTEXT("BR", "Menu.PartieSlot", "Partie {Slot}"), { { TEXT("Slot"), BRLoc::Int(Slot + 1) } });
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
		// v4.11 : campagne de la partie (hote seulement : le monde l'ignore chez un client)
		W->SetCampaign(ActiveSave ? ActiveSave->RouteBits : 0, ActiveSave ? ActiveSave->OptionalFound : 0, ActiveSave ? ActiveSave->Endings : 0);
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
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.ModeDeveloppeurNiveauLevelnumberVisite", "MODE D\u00c9VELOPPEUR : Niveau {LevelNumber} visit\u00e9 sans l'ajouter \u00e0 la partie \u00ab {SaveName} \u00bb."), { { TEXT("LevelNumber"), BRLoc::Int(LevelNumber) }, { TEXT("SaveName"), BRLoc::Arg(ActiveSave->SaveName) } }), 6.f, FLinearColor(0.6f, 0.9f, 1.f));
		return;
	}
	ActiveSave->MarkExplored(LevelNumber);
	ActiveSave->CurrentLevel = LevelNumber;
	// Ecrit un peu plus tard : apres une mort, l'inventaire est remis a zero juste apres le chargement
	PendingSaveDelay = 2.f;
	if (bNew)
	{
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.NiveauLevelnumberAjouteVosNiveaux", "Niveau {LevelNumber} ajout\u00e9 \u00e0 vos niveaux explor\u00e9s ({Explored} / {All}) : vous pourrez y revenir depuis le menu."), { { TEXT("LevelNumber"), BRLoc::Int(LevelNumber) }, { TEXT("Explored"), BRLoc::Int(BRLevels::CountAvailable(ActiveSave->Explored)) }, { TEXT("All"), BRLoc::Int(BRLevels::All().Num()) } }), 7.f, FLinearColor(0.75f, 1.f, 0.75f));
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
		// v4.11 : campagne (fragments de route, objectifs facultatifs, fins vues), tenue par l'hote
		ActiveSave->RouteBits = W->GetCampaign().RouteBits;
		ActiveSave->OptionalFound = W->GetCampaign().OptionalFound;
		ActiveSave->Endings = W->GetCampaign().Endings;
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
				// v4.11 : version de generation (1 : session d'avant la v4.11 encore en cours, gardee jusqu'a sa sortie), etat
				// de la mission (mecanismes, objets d'equipe, sorties ouvertes) et documents facultatifs
				Session.GenVersion = W->GetMissionGen() != 0 ? W->GetMissionGen() : BRMission::GenVersion;
				Session.PlaceVersion = BRMission::PlaceVersion;
				Session.Mission = W->GetMissionBlob();
				Session.LoreFound = W->GetLoreFound();
				Session.bBlackoutRecorded = W->IsBlackoutRecorded();
				Session.bEntityRecorded = W->IsEntityRecorded();
				Session.Collected = W->GetCollectedList();
				Session.Returned = W->GetReturnedList();
				Session.bHasSpot = bHasSafeSpot;
				Session.Spot = SafeSpot;
				Session.Yaw = SafeYaw;
			}
		}
	}
	// v4.12 : mission non restauree a la reprise : la sauvegarde d'origine est copiee avant sa premiere reecriture
	if (ABRWorld* MW = ABRWorld::Get(this))
	{
		if (MW->ConsumeSaveBackupRequest())
		{
			BRSaves::PreserveBeforeRecovery(BRSaves::ActiveSlot());
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
	MenuStatus = BR_STR(NSLOCTEXT("BR", "Menu.CreationPartie", "Cr\u00e9ation de la partie..."));
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
		MenuStatus = BR_STR(NSLOCTEXT("BR", "Menu.EntrezAdresseIpHoteEx", "Entrez l'adresse IP de l'h\u00f4te (ex. 192.168.1.20)."));
		return;
	}
	BRConfig::Get().SetString(SettingsSection, TEXT("LastAddress"), *Address);
	BRConfig::Save();
	MenuStatus = BRLoc::Fmt(NSLOCTEXT("BR", "Menu.ConnexionAddress", "Connexion \u00e0 {Address}..."), { { TEXT("Address"), BRLoc::Arg(Address) } });
	// v4.12 : version et contenu de ce jeu, verifies par l'hote avant d'entrer dans la partie
	ClientTravel(Address + BRLevels::JoinOptions(), TRAVEL_Absolute);
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
					.HintText(FText::FromString(BR_STR(NSLOCTEXT("BR", "Menu.AdresseIpHoteEx192", "Adresse IP de l'h\u00f4te, ex. 192.168.1.20"))))
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
					.HintText(FText::FromString(BR_STR(NSLOCTEXT("BR", "Menu.NomPartie", "Nom de la partie"))))
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
	// v4.11 : les sorties passent par le depart de groupe (ABRCharacter::ServerRequestDeparture, distance 3D, ligne de
	// vue, mission, rassemblement). Ce chemin ne sert plus qu'au saut de niveau du mode developpeur, si l'hote autorise les
	// commandes de test : l'ancienne validation par proximite 2D a 6 m d'une sortie est retiree.
	if (!AreCheatsAllowed())
	{
		UE_LOG(LogBackrooms, Warning, TEXT("Changement de niveau refuse pour %s (cible %d) : passer par une sortie et le depart de groupe"),
			*GetNameSafe(PlayerState), TargetLevel);
		return;
	}
	W->RequestTransition(TargetLevel);
}

// v4.11 : ServerMarkCollected et ServerVHSCollected sont retires : un client ne peut plus marquer un objet ramasse ni
// compter une cassette lui-meme. Les ramassages passent par ABRCharacter::ServerRequestPickup (transaction de l'hote).

void ABRPlayerController::ServerCompleteObjective_Implementation(uint8 Which)
{
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->ServerValidateRecording(Which, GetBRCharacter());
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
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.ReprisePartieSavenameNiveauTarget", "Reprise de la partie \u00ab {SaveName} \u00bb : Niveau {Target}, l\u00e0 o\u00f9 vous l'aviez laiss\u00e9."), { { TEXT("SaveName"), BRLoc::Arg(ActiveSave->SaveName) }, { TEXT("Target"), BRLoc::Int(Target) } }), 5.f, FLinearColor(0.75f, 0.95f, 1.f));
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
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.ModeDevSalleFossesNiveau", "MODE D\u00c9V : salle de fosses (Niveau {LevelNumber}, graine {Seed}). Attention au bord."), { { TEXT("LevelNumber"), BRLoc::Int(W->GetLevelNumber()) }, { TEXT("Seed"), BRLoc::Int(W->GetSeed()) } }), 4.f, FLinearColor(0.6f, 0.9f, 1.f));
		return;
	}
	if (!HasAuthority())
	{
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Menu.SalleFossesNiveauHotePeut", "Pas de salle de fosses dans ce niveau (l'h\u00f4te peut charger le Niveau 0 : BRPits).")), 4.f);
		return;
	}
	// Pas de salle ici : Niveau 0 avec la graine de demonstration, puis placement au bord des fosses
	bDevSession = true;
	bInMenu = false;
	ShowAddressBox(false);
	UpdateInputMode();
	bPendingPitTeleport = true;
	ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.ModeDevNiveau0Graine", "MODE D\u00c9V : Niveau 0, graine de d\u00e9monstration {DemoSeed}"), { { TEXT("DemoSeed"), BRLoc::Int(ABRWorld::DemoSeed) } }), 3.f,
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
		ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Menu.SeedHostOnly", "BRSeed : r\u00e9serv\u00e9 \u00e0 l'h\u00f4te de la partie.")), 3.f);
		return;
	}
	bDevSession = true;
	const int32 Level = W->GetLevelNumber();
	ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.NiveauLevelGraineNumber", "Niveau {Level}, graine {Number}"), { { TEXT("Level"), BRLoc::Int(Level) }, { TEXT("Number"), BRLoc::Int(Number) } }), 3.f, FLinearColor(0.6f, 0.9f, 1.f));
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
		ABRHUD::Notify(this, C->bGodMode ? BR_STR(NSLOCTEXT("BR", "Menu.ModeInvincible", "Mode invincible : ON")) : BR_STR(NSLOCTEXT("BR", "Menu.ModeInvincibleOff", "Mode invincible : OFF")), 2.f);
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
	ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Menu.SensibiliteSensitivity", "Sensibilit\u00e9 : {Sensitivity}"), { { TEXT("Sensitivity"), BRLoc::Num(FBRSettings::Get().Sensitivity, 2) } }), 2.f);
}

void ABRPlayerController::BRInvertY()
{
	FBRSettings::Get().bInvertY = !FBRSettings::Get().bInvertY;
	SaveSettings();
	ABRHUD::Notify(this, FBRSettings::Get().bInvertY ? BR_STR(NSLOCTEXT("BR", "Menu.AxeInverse", "Axe Y invers\u00e9")) : BR_STR(NSLOCTEXT("BR", "Menu.AxeNormal", "Axe Y normal")), 2.f);
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

FString ABRPlayerController::GetDisplaySummary() const
{
	// v4.9 : mode et resolution a l'essai (confirmation de l'affichage)
	const BRDisplay::FMode M = BRDisplay::Current();
	return FString::Printf(TEXT("%s  \u00b7  %d \u00d7 %d"), *WindowNames(M.Window), M.Resolution.X, M.Resolution.Y);
}

int32 ABRPlayerController::FindSettingRow(const FString& Name)
{
	struct FNamedRow
	{
		const TCHAR* Name;
		int32 Row;
	};
	static const FNamedRow Rows[] = { { TEXT("WindowMode"), Row_WindowMode }, { TEXT("Resolution"), Row_Resolution }, { TEXT("RenderScale"), Row_RenderScale },
		{ TEXT("Profile"), Row_Profile }, { TEXT("HardwareRT"), Row_HardwareRT }, { TEXT("RTHitLighting"), Row_RTHitLighting },
		{ TEXT("CreatureReflections"), Row_CreatureReflections }, { TEXT("RTShadows"), Row_RTShadows }, { TEXT("AreaLights"), Row_AreaLights },
		{ TEXT("UiScale"), Row_UiScale }, { TEXT("HudOpacity"), Row_HudOpacity }, { TEXT("Crosshair"), Row_Crosshair }, { TEXT("QuickBar"), Row_QuickBar },
		{ TEXT("Objectives"), Row_Objectives }, { TEXT("EffectsVolume"), Row_EffectsVolume }, { TEXT("VoiceVolume"), Row_VoiceVolume },
		{ TEXT("Subtitles"), Row_Subtitles } };
	for (const FNamedRow& R : Rows)
	{
		if (Name == R.Name)
		{
			return R.Row;
		}
	}
	return INDEX_NONE;
}

int32 ABRPlayerController::GetSettingCategory(int32 Index) const
{
	switch (Index)
	{
	case Row_WindowMode:
	case Row_Resolution:
	case Row_RenderScale:
	case Row_VSync:
	case Row_MaxFPS:
	case Row_Brightness:
		return 1; // VIDEO
	case Row_UiScale:
	case Row_HudOpacity:
	case Row_Crosshair:
	case Row_QuickBar:
	case Row_Objectives:
		return 2; // INTERFACE
	case Row_Profile:
	case Row_Quality:
	case Row_HardwareRT:
	case Row_RTHitLighting:
	case Row_CreatureReflections:
	case Row_RTShadows:
	case Row_AreaLights:
	case Row_FullCreatures:
	case Row_VolumetricFog:
	case Row_FilmGrain:
	case Row_VHSEffect:
		return 3; // GRAPHISMES
	default:
		return 0; // JEU
	}
}

FString ABRPlayerController::RayTracingUnavailableReason()
{
	// v4.9 : selon la plateforme et les capacites reelles (RHI demarre), pas un nom de carte
	const FString RHIName = GDynamicRHI ? FString(GDynamicRHI->GetName()) : FString(TEXT("?"));
#if PLATFORM_MAC
	return BRLoc::Fmt(NSLOCTEXT("BR", "Menu.RtUnavailableMac", "Indisponible sur macOS dans cette version ({RHI}) : Lumen logiciel est utilis\u00e9. Le ray tracing mat\u00e9riel Metal d'Unreal est exp\u00e9rimental, limit\u00e9 \u00e0 certains Mac Apple Silicon, et n'est pas valid\u00e9 pour ce jeu."), { { TEXT("RHI"), BRLoc::Arg(RHIName) } });
#elif PLATFORM_LINUX
	return BRLoc::Fmt(NSLOCTEXT("BR", "Menu.RtUnavailableLinux", "Indisponible sur Linux dans cette version ({RHI}) : Lumen logiciel est utilis\u00e9. Le ray tracing Vulkan d'Unreal d\u00e9pend du pilote et n'est pas valid\u00e9 pour ce jeu."), { { TEXT("RHI"), BRLoc::Arg(RHIName) } });
#else
	return BRLoc::Fmt(NSLOCTEXT("BR", "Menu.RtUnavailableWindows", "Indisponible : le jeu a d\u00e9marr\u00e9 sans ray tracing mat\u00e9riel ({RHI}). Il faut DirectX 12 et une carte qui le prend en charge. Lumen logiciel est utilis\u00e9 ; un changement de carte ou de RHI demande un red\u00e9marrage."), { { TEXT("RHI"), BRLoc::Arg(RHIName) } });
#endif
}

FString ABRPlayerController::GetSettingLabel(int32 Index) const
{
	switch (Index)
	{
	case Row_Language:
		return BR_STR(NSLOCTEXT("BR", "Menu.LanguageRow", "LANGUE"));
	case Row_Sensitivity:
		return BR_STR(NSLOCTEXT("BR", "Menu.SensibiliteSouris", "SENSIBILIT\u00c9 DE LA SOURIS"));
	case Row_InvertY:
		return BR_STR(NSLOCTEXT("BR", "Menu.InverserAxeVertical", "INVERSER L'AXE VERTICAL"));
	case Row_FOV:
		return BR_STR(NSLOCTEXT("BR", "Menu.ChampVision", "CHAMP DE VISION"));
	case Row_HeadBob:
		return BR_STR(NSLOCTEXT("BR", "Menu.BalancementCamera", "BALANCEMENT DE LA CAM\u00c9RA"));
	case Row_CameraShake:
		return BR_STR(NSLOCTEXT("BR", "Menu.TremblementsCamera", "TREMBLEMENTS DE LA CAM\u00c9RA"));
	case Row_Flashes:
		return BR_STR(NSLOCTEXT("BR", "Menu.FlashsClignotements", "FLASHS ET CLIGNOTEMENTS"));
	case Row_MotionBlur:
		return BR_STR(NSLOCTEXT("BR", "Menu.FlouMouvement", "FLOU DE MOUVEMENT"));
	case Row_Volume:
		return BR_STR(NSLOCTEXT("BR", "Menu.VolumeGeneral", "VOLUME G\u00c9N\u00c9RAL"));
	case Row_EffectsVolume:
		return BR_STR(NSLOCTEXT("BR", "Menu.EffectsVolume", "VOLUME DES EFFETS ET AMBIANCES"));
	case Row_VoiceVolume:
		return BR_STR(NSLOCTEXT("BR", "Menu.VoiceVolume", "VOLUME DES VOIX"));
	case Row_Subtitles:
		return BR_STR(NSLOCTEXT("BR", "Menu.Subtitles", "SOUS-TITRES DES SONS"));
	case Row_Voice:
		return BR_STR(NSLOCTEXT("BR", "Menu.ChatVocalProximite", "CHAT VOCAL (PROXIMIT\u00c9)"));
	case Row_Brightness:
		return BR_STR(NSLOCTEXT("BR", "Menu.Luminosite", "LUMINOSIT\u00c9"));
	case Row_WindowMode:
		return BR_STR(NSLOCTEXT("BR", "Menu.ModeAffichage", "MODE D'AFFICHAGE"));
	case Row_Resolution:
		return BR_STR(NSLOCTEXT("BR", "Menu.ResolutionFenetre", "R\u00c9SOLUTION"));
	case Row_CreatureReflections:
		return BR_STR(NSLOCTEXT("BR", "Menu.CreatureReflections", "CR\u00c9ATURES DANS LES REFLETS"));
	case Row_UiScale:
		return BR_STR(NSLOCTEXT("BR", "Menu.UiScale", "TAILLE DE L'INTERFACE"));
	case Row_HudOpacity:
		return BR_STR(NSLOCTEXT("BR", "Menu.HudOpacity", "OPACIT\u00c9 DE L'INTERFACE"));
	case Row_Crosshair:
		return BR_STR(NSLOCTEXT("BR", "Menu.Crosshair", "R\u00c9TICULE"));
	case Row_QuickBar:
		return BR_STR(NSLOCTEXT("BR", "Menu.QuickBarRow", "OBJETS RAPIDES"));
	case Row_Objectives:
		return BR_STR(NSLOCTEXT("BR", "Menu.ObjectivesRow", "OBJECTIFS \u00c0 L'\u00c9CRAN"));
	case Row_RenderScale:
		return BR_STR(NSLOCTEXT("BR", "Menu.ResolutionRendu", "R\u00c9SOLUTION DE RENDU"));
	case Row_VSync:
		return BR_STR(NSLOCTEXT("BR", "Menu.SynchroVerticaleVSync", "SYNCHRO VERTICALE (V-SYNC)"));
	case Row_MaxFPS:
		return BR_STR(NSLOCTEXT("BR", "Menu.ImagesSecondeMax", "IMAGES PAR SECONDE MAX."));
	case Row_Profile:
		return BR_STR(NSLOCTEXT("BR", "Menu.ProfilGraphique", "PROFIL GRAPHIQUE"));
	case Row_Quality:
		return BR_STR(NSLOCTEXT("BR", "Menu.QualiteGraphique", "QUALIT\u00c9 GRAPHIQUE"));
	case Row_HardwareRT:
		return BR_STR(NSLOCTEXT("BR", "Menu.RayTracingMaterielRtx", "RAY TRACING MAT\u00c9RIEL (RTX)"));
	case Row_RTHitLighting:
		return BR_STR(NSLOCTEXT("BR", "Menu.RefletsRayTracesHauteQualite", "REFLETS RAY TRAC\u00c9S HAUTE QUALIT\u00c9"));
	case Row_RTShadows:
		return BR_STR(NSLOCTEXT("BR", "Menu.OmbresRayTraceesLampe", "OMBRES RAY TRAC\u00c9ES (LAMPE)"));
	case Row_AreaLights:
		return BR_STR(NSLOCTEXT("BR", "Menu.NeonsLumieresSurfaciques", "N\u00c9ONS EN LUMI\u00c8RES SURFACIQUES"));
	case Row_FullCreatures:
		return BR_STR(NSLOCTEXT("BR", "Menu.ModelesCompletsEntites", "MOD\u00c8LES COMPLETS DES ENTIT\u00c9S"));
	case Row_VolumetricFog:
		return BR_STR(NSLOCTEXT("BR", "Menu.VolumetricFog", "BROUILLARD VOLUM\u00c9TRIQUE"));
	case Row_FilmGrain:
		return BR_STR(NSLOCTEXT("BR", "Menu.GrainImage", "GRAIN DE L'IMAGE"));
	case Row_VHSEffect:
		return BR_STR(NSLOCTEXT("BR", "Menu.EffetVhs", "EFFET VHS"));
	case Row_DevMode:
		return BR_STR(NSLOCTEXT("BR", "Menu.ModeDeveloppeur", "MODE D\u00c9VELOPPEUR"));
	default:
		return FString();
	}
}

FString ABRPlayerController::GetSettingValue(int32 Index) const
{
	const FBRSettings& S = FBRSettings::Get();
	switch (Index)
	{
	case Row_Language:
		return BRLoc::Current().NativeName;
	case Row_Sensitivity:
		return FString::Printf(TEXT("%.2f"), S.Sensitivity);
	case Row_InvertY:
		return OnOff(S.bInvertY);
	case Row_FOV:
		return FString::Printf(TEXT("%d\u00b0"), FMath::RoundToInt(S.FOV));
	case Row_HeadBob:
		return OnOff(S.bHeadBob);
	case Row_CameraShake:
		return S.CameraShake <= 0.f ? FString(BR_STR(NSLOCTEXT("BR", "Menu.Aucun", "AUCUN"))) : FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.CameraShake * 100.f));
	case Row_Flashes:
		return S.Flashes <= 0 ? FString(BR_STR(NSLOCTEXT("BR", "Menu.Normaux", "NORMAUX"))) : (S.Flashes == 1 ? FString(BR_STR(NSLOCTEXT("BR", "Menu.Attenues", "ATT\u00c9NU\u00c9S"))) : FString(BR_STR(NSLOCTEXT("BR", "Menu.Aucun", "AUCUN"))));
	case Row_MotionBlur:
		return OnOff(S.bMotionBlur);
	case Row_Volume:
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.MasterVolume * 100.f));
	case Row_EffectsVolume:
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.EffectsVolume * 100.f));
	case Row_VoiceVolume:
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.VoiceVolume * 100.f));
	case Row_Subtitles:
		return OnOff(S.bSubtitles);
	case Row_Voice:
		return VoiceNames(FMath::Clamp(S.VoiceMode, 0, 2));
	case Row_Brightness:
		return FString::Printf(TEXT("%+.1f"), S.Brightness);
	case Row_WindowMode:
	{
		// v4.9 : mode demande ; s'il differe du mode reellement obtenu (systeme sans plein ecran exclusif), on le dit
		const BRDisplay::FMode Want = BRDisplay::Current();
		const BRDisplay::FMode Got = BRDisplay::Effective();
		return Got.Window != Want.Window ? BRLoc::Fmt(NSLOCTEXT("BR", "Menu.WindowModeEffective", "{Want} \u2192 {Got}"), { { TEXT("Want"), BRLoc::Arg(WindowNames(Want.Window)) }, { TEXT("Got"), BRLoc::Arg(WindowNames(Got.Window)) } })
			: WindowNames(Want.Window);
	}
	case Row_Resolution:
	{
		const BRDisplay::FMode M = BRDisplay::Current();
		const FString Res = FString::Printf(TEXT("%d \u00d7 %d"), M.Resolution.X, M.Resolution.Y);
		return M.Window == 1 ? BRLoc::Fmt(NSLOCTEXT("BR", "Menu.ResolutionScreen", "{Res} (\u00e9cran)"), { { TEXT("Res"), BRLoc::Arg(Res) } }) : Res;
	}
	case Row_CreatureReflections:
	{
		if (!IsHardwareRayTracingAvailable() || !S.bHardwareRT)
		{
			return BR_STR(NSLOCTEXT("BR", "Menu.ReflectScreenOnly", "\u00c9CRAN SEULEMENT"));
		}
		if (S.bRTHitLighting)
		{
			return BR_STR(NSLOCTEXT("BR", "Menu.ReflectFullHit", "COMPLETS (RAYONS)"));
		}
		if (S.CreatureReflections == 1 && !RetraceSupported())
		{
			return BR_STR(NSLOCTEXT("BR", "Menu.ReflectNotInEngine", "\u00c9CRAN (MOTEUR)"));
		}
		return S.CreatureReflections == 1 ? FString(BR_STR(NSLOCTEXT("BR", "Menu.ReflectRetrace", "RAYONS"))) : FString(BR_STR(NSLOCTEXT("BR", "Menu.ReflectScreenOnly", "\u00c9CRAN SEULEMENT")));
	}
	case Row_UiScale:
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.UiScale * 100.f));
	case Row_HudOpacity:
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.HudOpacity * 100.f));
	case Row_Crosshair:
		return CrosshairNames(S.CrosshairMode);
	case Row_QuickBar:
		return ShowNames(S.QuickBarMode);
	case Row_Objectives:
		return ShowNames(S.ObjectivesMode);
	case Row_RenderScale:
	{
		// v4.8 : resolution interne reelle (celle que TSR agrandit), pas seulement le pourcentage
		const FIntPoint In = InternalResolution();
		return In.X > 0 ? BRLoc::Fmt(NSLOCTEXT("BR", "Menu.RenderscaleX", "{RenderScale} %  ({X}\u00d7{Y})"), { { TEXT("RenderScale"), BRLoc::Int(S.RenderScale) }, { TEXT("X"), BRLoc::Int(In.X) }, { TEXT("Y"), BRLoc::Int(In.Y) } }) : FString::Printf(TEXT("%d %%"), S.RenderScale);
	}
	case Row_VSync:
		return OnOff(S.bVSync);
	case Row_MaxFPS:
		return S.MaxFPS <= 0 ? FString(BR_STR(NSLOCTEXT("BR", "Menu.Illimite", "ILLIMIT\u00c9"))) : FString::Printf(TEXT("%d"), S.MaxFPS);
	case Row_Profile:
		return ProfileNames(FMath::Clamp(S.GraphicsProfile, 0, 4));
	case Row_Quality:
		return QualityNames(FMath::Clamp(S.Quality, 0, 4));
	case Row_HardwareRT:
		return !IsHardwareRayTracingAvailable() ? FString(BR_STR(NSLOCTEXT("BR", "Menu.Indisponible", "INDISPONIBLE"))) : OnOff(S.bHardwareRT);
	case Row_RTHitLighting:
		return !IsHardwareRayTracingAvailable() ? FString(BR_STR(NSLOCTEXT("BR", "Menu.Indisponible", "INDISPONIBLE"))) : OnOff(S.bRTHitLighting);
	case Row_RTShadows:
		return !IsHardwareRayTracingAvailable() ? FString(BR_STR(NSLOCTEXT("BR", "Menu.Indisponible", "INDISPONIBLE"))) : OnOff(S.bRTShadows);
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
	case Row_Language:
		return BR_STR(NSLOCTEXT("BR", "Menu.LanguageHint", "Langue des textes, des nombres et des dates. S'applique tout de suite et reste enregistr\u00e9e. En coop, chaque joueur garde la sienne. Les voix et les sons ne changent pas (pas de doublage)."));
	case Row_HeadBob:
		return BR_STR(NSLOCTEXT("BR", "Menu.DesactivezMouvementCameraPendantMarche", "D\u00e9sactivez-le si le mouvement de la cam\u00e9ra pendant la marche vous incommode."));
	case Row_CameraShake:
		return BR_STR(NSLOCTEXT("BR", "Menu.SecoussesCameraQuandFrappePendant", "Secousses de la cam\u00e9ra quand on est frapp\u00e9 et pendant les jumpscares. Les coups et leurs d\u00e9g\u00e2ts ne changent pas."));
	case Row_Flashes:
		return BR_STR(NSLOCTEXT("BR", "Menu.EclairsJumpscaresImageMortNeons", "\u00c9clairs des jumpscares, image de la mort, n\u00e9ons qui clignotent. Att\u00e9nu\u00e9s ou supprim\u00e9s si les lumi\u00e8res vives qui clignotent vous g\u00eanent ; les coupures de courant restent annonc\u00e9es par le son."));
	case Row_MotionBlur:
		return BR_STR(NSLOCTEXT("BR", "Menu.FlouMouvementsRapidesCameraDesactive", "Flou des mouvements rapides de la cam\u00e9ra. D\u00e9sactiv\u00e9 par d\u00e9faut."));
	case Row_Volume:
		return BR_STR(NSLOCTEXT("BR", "Menu.VolumeToutJeuAmbianceEntites", "Volume de tout le jeu (ambiance, entit\u00e9s, voix des co\u00e9quipiers)."));
	case Row_EffectsVolume:
		return BR_STR(NSLOCTEXT("BR", "Menu.EffectsVolumeHint", "Entit\u00e9s, m\u00e9canismes, pas, ambiances et interface, sans changer les voix. Chaque son utile a aussi un signe visible ou un sous-titre."));
	case Row_VoiceVolume:
		return BR_STR(NSLOCTEXT("BR", "Menu.VoiceVolumeHint", "Voix des co\u00e9quipiers, sans changer les effets. Pour ne plus les entendre du tout : CHAT VOCAL. Aucune \u00e9nigme ne demande le micro."));
	case Row_Subtitles:
		return BR_STR(NSLOCTEXT("BR", "Menu.SubtitlesHint", "\u00c9crit en bas de l'\u00e9cran ce que l'on entend d'utile : entit\u00e9 proche et sa direction, m\u00e9canisme qui r\u00e9agit, coupure de courant."));
	case Row_Voice:
		return BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "Menu.EntendAutresJoueursPresLeur", "On entend les autres joueurs pr\u00e8s de leur personnage, \u00e9touff\u00e9s par les murs. Appuyer pour parler : touche {PushToTalk}. Voix ouverte : le micro transmet en permanence. Micro coup\u00e9 : vous entendez toujours les autres.")));
	case Row_Brightness:
		return BR_STR(NSLOCTEXT("BR", "Menu.RendImageClaireSombreZones", "Rend l'image plus claire ou plus sombre (les zones sans lumi\u00e8re restent noires)."));
	case Row_WindowMode:
	{
		// v4.9 : confirmation, retour automatique, et mode reellement obtenu selon la plateforme
		FString Hint = BR_STR(NSLOCTEXT("BR", "Menu.WindowModeHint", "Plein \u00e9cran, plein \u00e9cran fen\u00eatr\u00e9 (sans bordures, \u00e0 la taille de l'\u00e9cran, Alt+Tab instantan\u00e9) ou fen\u00eatre. Chaque changement est \u00e0 confirmer dans les 15 secondes, sinon l'affichage pr\u00e9c\u00e9dent revient. Sans effet dans l'\u00e9diteur."));
		if (!BRDisplay::PlatformHasExclusiveFullscreen())
		{
			Hint += TEXT(" ") + BR_STR(NSLOCTEXT("BR", "Menu.NoExclusiveFullscreen", "Sur ce syst\u00e8me, le plein \u00e9cran est une fen\u00eatre sans bordures \u00e0 la taille de l'\u00e9cran : le mode r\u00e9ellement obtenu est indiqu\u00e9 apr\u00e8s la fl\u00e8che."));
		}
		return Hint;
	}
	case Row_Resolution:
		return BR_STR(NSLOCTEXT("BR", "Menu.ResolutionHint", "Taille de la fen\u00eatre, ou d\u00e9finition de l'\u00e9cran en plein \u00e9cran. Le plein \u00e9cran fen\u00eatr\u00e9 prend toujours celle de l'\u00e9cran. \u00c0 confirmer dans les 15 secondes. L'\u00e9chelle de rendu (R\u00c9SOLUTION DE RENDU) est un r\u00e9glage \u00e0 part."));
	case Row_CreatureReflections:
		return BR_STR(NSLOCTEXT("BR", "Menu.CreatureReflectionsHint", "\u00c9CRAN SEULEMENT : les reflets montrent une cr\u00e9ature seulement si elle est \u00e0 l'\u00e9cran ; hors champ, elle n'y appara\u00eet pas. RAYONS : les cr\u00e9atures sont dans la sc\u00e8ne ray trac\u00e9e et \u00e9clair\u00e9es par les rayons l\u00e0 o\u00f9 le cache de surfaces de Lumen ne les couvre pas, y compris hors champ ; plus co\u00fbteux. Demande le ray tracing mat\u00e9riel et une version du moteur qui le permet (sinon : \u00c9CRAN (MOTEUR)). Les reflets HAUTE QUALIT\u00c9 les montrent toujours."));
	case Row_UiScale:
		return BR_STR(NSLOCTEXT("BR", "Menu.UiScaleHint", "Taille des textes et des panneaux, pour tous les \u00e9crans (de 1280\u00d7720 \u00e0 l'ultra large et aux \u00e9crans Retina)."));
	case Row_HudOpacity:
		return BR_STR(NSLOCTEXT("BR", "Menu.HudOpacityHint", "Opacit\u00e9 des informations affich\u00e9es pendant l'exploration (objets rapides, objectifs, r\u00e9ticule). Les messages importants restent lisibles."));
	case Row_Crosshair:
		return BR_STR(NSLOCTEXT("BR", "Menu.CrosshairHint", "Point au centre de l'\u00e9cran : toujours, seulement sur un objet utilisable, ou jamais. Les consignes d'interaction et la r\u00e9animation restent affich\u00e9es."));
	case Row_QuickBar:
		return BR_STR(NSLOCTEXT("BR", "Menu.QuickBarHint", "Poches 1 \u00e0 4 en bas de l'\u00e9cran : bri\u00e8vement apr\u00e8s un changement (ramassage, utilisation), toujours, ou masqu\u00e9es. Elles restent dans l'inventaire."));
	case Row_Objectives:
		return BR_STR(NSLOCTEXT("BR", "Menu.ObjectivesHint", "Objectifs en haut \u00e0 droite : bri\u00e8vement \u00e0 l'arriv\u00e9e et \u00e0 chaque progr\u00e8s, toujours, ou masqu\u00e9s. Ils restent dans l'inventaire (onglet Personnage)."));
	case Row_RenderScale:
		return BR_STR(NSLOCTEXT("BR", "Menu.Dessous100ImageCalculeePetite", "En dessous de 100 %, l'image est calcul\u00e9e plus petite puis agrandie par TSR : beaucoup plus fluide, l\u00e9g\u00e8rement plus floue."));
	case Row_VSync:
		return BR_STR(NSLOCTEXT("BR", "Menu.SupprimeDechiruresImageAjoutePeu", "Supprime les d\u00e9chirures d'image, ajoute un peu de latence."));
	case Row_MaxFPS:
		return BR_STR(NSLOCTEXT("BR", "Menu.LimiterImagesSecondeReduitChaleur", "Limiter les images par seconde r\u00e9duit la chaleur et le bruit de la carte graphique. Sans effet dans l'\u00e9diteur."));
	case Row_Profile:
		return BR_STR(NSLOCTEXT("BR", "Menu.PerformanceQualiteEleveeLumenLogiciel", "PERFORMANCE : qualit\u00e9 \u00c9lev\u00e9e, Lumen logiciel, rendu \u00e0 67 % (TSR). QUALIT\u00c9 : \u00c9pique, Lumen en ray tracing mat\u00e9riel (cache de surfaces), rendu \u00e0 80 %. RTX FLUIDE : ray tracing mat\u00e9riel, rendu \u00e0 67 % agrandi par TSR, ombres des n\u00e9ons jusqu'\u00e0 25 m, Hound all\u00e9g\u00e9 : vise 60 images/s stables avec une carte RTX. CIN\u00c9MATIQUE : reflets \u00e9clair\u00e9s par les rayons, ombres ray trac\u00e9es de la lampe, mod\u00e8les complets, rendu \u00e0 100 %. Modifier un r\u00e9glage ci-dessous passe en PERSONNALIS\u00c9. S'applique tout de suite, aussi aux zones d\u00e9j\u00e0 charg\u00e9es."));
	case Row_Quality:
		return BR_STR(NSLOCTEXT("BR", "Menu.OmbresLumenTexturesAntiCrenelage", "Ombres, Lumen, textures, anti-cr\u00e9nelage (scalability). S'applique tout de suite."));
	case Row_HardwareRT:
		return IsHardwareRayTracingAvailable()
			? FString(BR_STR(NSLOCTEXT("BR", "Menu.LumenRayTracingMaterielReflets", "Lumen en ray tracing mat\u00e9riel : reflets et lumi\u00e8re indirecte bien plus pr\u00e9cis (carte RTX / RX 6000+). S'applique tout de suite.")))
			: RayTracingUnavailableReason();
	case Row_RTHitLighting:
		return BR_STR(NSLOCTEXT("BR", "Menu.RefletsEclairesRayonsEuxMemes", "Reflets \u00e9clair\u00e9s par les rayons eux-m\u00eames (eau, flaques, carrelage, m\u00e9tal) au lieu du cache de surfaces de Lumen. Le plus co\u00fbteux des r\u00e9glages : seul le profil CIN\u00c9MATIQUE l'active."));
	case Row_RTShadows:
		return BR_STR(NSLOCTEXT("BR", "Menu.OmbresLampeTorcheRayTracees", "Ombres de la lampe torche ray trac\u00e9es (contact net, pas de recalcul des ombres virtuelles \u00e0 chaque mouvement de la lampe). Les plafonniers gardent les ombres virtuelles (VSM), moins ch\u00e8res pour des dizaines de lumi\u00e8res fixes."));
	case Row_AreaLights:
		return BR_STR(NSLOCTEXT("BR", "Menu.OmbresDoucesNeonsAppliqueTout", "Ombres douces des n\u00e9ons. S'applique tout de suite, zones d\u00e9j\u00e0 charg\u00e9es comprises (quelques lumi\u00e8res par image)."));
	case Row_FullCreatures:
		return BR_STR(NSLOCTEXT("BR", "Menu.HoundOrigine175000Sommets", "Hound d'origine (175 000 sommets, pelage complet) au lieu du d\u00e9riv\u00e9 all\u00e9g\u00e9. Tr\u00e8s co\u00fbteux en ray tracing. S'applique aussi aux entit\u00e9s pr\u00e9sentes."));
	case Row_VolumetricFog:
		return BR_STR(NSLOCTEXT("BR", "Menu.HalosLumiereAirHumide", "Halos de lumi\u00e8re dans l'air humide."));
	case Row_VHSEffect:
		return BR_STR(NSLOCTEXT("BR", "Menu.LignesBalayageLegereAberrationSalete", "Lignes de balayage, l\u00e9g\u00e8re aberration et salet\u00e9 d'objectif. D\u00e9sactiv\u00e9 : image nette."));
	case Row_DevMode:
		return BR_STR(NSLOCTEXT("BR", "Menu.TousNiveauxJouablesDepuisChoix", "Tous les niveaux jouables depuis le choix des niveaux (non ajout\u00e9s \u00e0 la partie). En jeu : Page pr\u00e9c. / suiv. niveau, D\u00e9but nouvelle disposition, Fin objectifs, Inser coupure, F6 vol libre, F7 invincible, F10 jumpscares."));
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
	case Row_Language:
	{
		const int32 Num = BRLoc::Languages().Num();
		ChooseLanguage((FMath::Max(0, BRLoc::CurrentIndex()) + Dir + Num) % Num);
		break;
	}
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
	case Row_EffectsVolume:
		S.EffectsVolume = FMath::Clamp(FMath::RoundToFloat((S.EffectsVolume + Dir * 0.05f) * 20.f) / 20.f, 0.f, 1.f);
		break;
	case Row_VoiceVolume:
		S.VoiceVolume = FMath::Clamp(FMath::RoundToFloat((S.VoiceVolume + Dir * 0.05f) * 20.f) / 20.f, 0.25f, 1.f);
		break;
	case Row_Subtitles:
		S.bSubtitles = !S.bSubtitles;
		break;
	case Row_Voice:
		S.VoiceMode = (S.VoiceMode + Dir + 3) % 3;
		break;
	case Row_Brightness:
		S.Brightness = FMath::Clamp(FMath::RoundToFloat((S.Brightness + Dir * 0.1f) * 10.f) / 10.f, -1.5f, 1.5f);
		break;
	case Row_WindowMode:
	{
		// v4.9 : applique tout de suite, a confirmer dans les 15 s (sinon retour) ; rien d'autre n'est reapplique
		const int32 Mode = (BRDisplay::Current().Window + Dir + 3) % 3;
		if (!BRDisplay::Request(Mode, FIntPoint::ZeroValue))
		{
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Display.EditorOnly", "Mode d'affichage : sans effet dans l'\u00e9diteur (lancez le jeu seul).")), 3.f);
		}
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->PlayUISound(TEXT("S_UIClick"));
		}
		return;
	}
	case Row_Resolution:
	{
		const BRDisplay::FMode M = BRDisplay::Current();
		const TArray<FIntPoint> List = BRDisplay::ResolutionsFor(M.Window);
		if (M.Window == 1 || List.Num() < 2)
		{
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Display.BorderlessFollowsScreen", "Le plein \u00e9cran fen\u00eatr\u00e9 prend la d\u00e9finition de l'\u00e9cran. Changez le mode pour choisir une r\u00e9solution.")), 3.f);
			return;
		}
		int32 Idx = List.IndexOfByKey(M.Resolution);
		if (Idx == INDEX_NONE)
		{
			// Resolution hors de la liste (fenetre redimensionnee) : on repart de la plus proche
			Idx = 0;
			for (int32 k = 0; k < List.Num(); ++k)
			{
				Idx = List[k].X * List[k].Y <= M.Resolution.X * M.Resolution.Y ? k : Idx;
			}
		}
		BRDisplay::Request(M.Window, List[(Idx + Dir + List.Num()) % List.Num()]);
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->PlayUISound(TEXT("S_UIClick"));
		}
		return;
	}
	case Row_CreatureReflections:
		if (!IsHardwareRayTracingAvailable() || S.bRTHitLighting)
		{
			return; // sans ray tracing : ecran seulement ; avec les reflets haute qualite : toujours complets
		}
		S.CreatureReflections = S.CreatureReflections == 1 ? 0 : 1;
		S.GraphicsProfile = 3;
		break;
	case Row_UiScale:
		S.UiScale = FMath::Clamp(FMath::RoundToFloat((S.UiScale + Dir * 0.05f) * 20.f) / 20.f, 0.8f, 1.25f);
		break;
	case Row_HudOpacity:
		S.HudOpacity = FMath::Clamp(FMath::RoundToFloat((S.HudOpacity + Dir * 0.1f) * 10.f) / 10.f, 0.4f, 1.f);
		break;
	case Row_Crosshair:
		S.CrosshairMode = (S.CrosshairMode + Dir + 3) % 3;
		break;
	case Row_QuickBar:
		S.QuickBarMode = (S.QuickBarMode + Dir + 3) % 3;
		break;
	case Row_Objectives:
		S.ObjectivesMode = (S.ObjectivesMode + Dir + 3) % 3;
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
	case Row_RTHitLighting:
	case Row_RTShadows:
		if (!IsHardwareRayTracingAvailable())
		{
			// v4.9 : option indisponible sur cette machine (plateforme, RHI, carte) : expliquee, pas basculee pour rien
			ABRHUD::Notify(this, RayTracingUnavailableReason(), 5.f, FLinearColor(1.f, 0.85f, 0.5f));
			return;
		}
		if (Index == Row_HardwareRT)
		{
			S.bHardwareRT = !S.bHardwareRT;
		}
		else if (Index == Row_RTHitLighting)
		{
			S.bRTHitLighting = !S.bRTHitLighting;
		}
		else
		{
			S.bRTShadows = !S.bRTShadows;
		}
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
	Cfg.GetFloat(SettingsSection, TEXT("EffectsVolume"), S.EffectsVolume);
	Cfg.GetFloat(SettingsSection, TEXT("VoiceVolume"), S.VoiceVolume);
	Cfg.GetBool(SettingsSection, TEXT("Subtitles"), S.bSubtitles);
	Cfg.GetInt(SettingsSection, TEXT("VoiceMode"), S.VoiceMode);
	Cfg.GetFloat(SettingsSection, TEXT("Brightness"), S.Brightness);
	// v4.9 : WindowMode n'est plus lu ici : UGameUserSettings fait foi (BRDisplay::Startup reprend l'ancienne valeur)
	Cfg.GetFloat(SettingsSection, TEXT("UiScale"), S.UiScale);
	Cfg.GetFloat(SettingsSection, TEXT("HudOpacity"), S.HudOpacity);
	Cfg.GetInt(SettingsSection, TEXT("Crosshair"), S.CrosshairMode);
	Cfg.GetInt(SettingsSection, TEXT("QuickBar"), S.QuickBarMode);
	Cfg.GetInt(SettingsSection, TEXT("Objectives"), S.ObjectivesMode);
	Cfg.GetInt(SettingsSection, TEXT("CreatureReflections"), S.CreatureReflections);
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
	S.EffectsVolume = FMath::Clamp(S.EffectsVolume, 0.f, 1.f);
	S.VoiceVolume = FMath::Clamp(S.VoiceVolume, 0.25f, 1.f);
	S.VoiceMode = FMath::Clamp(S.VoiceMode, 0, 2);
	S.Brightness = FMath::Clamp(S.Brightness, -1.5f, 1.5f);
	S.UiScale = FMath::Clamp(S.UiScale, 0.8f, 1.25f);
	S.HudOpacity = FMath::Clamp(S.HudOpacity, 0.4f, 1.f);
	S.CrosshairMode = FMath::Clamp(S.CrosshairMode, 0, 2);
	S.QuickBarMode = FMath::Clamp(S.QuickBarMode, 0, 2);
	S.ObjectivesMode = FMath::Clamp(S.ObjectivesMode, 0, 2);
	S.CreatureReflections = FMath::Clamp(S.CreatureReflections, 0, 1);
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
		S.bVolumetricFog = true; S.bFullCreatures = false; S.CreatureReflections = 0;
		break;
	case 2: // Cinematique
		S.Quality = 4; S.bHardwareRT = true; S.bRTHitLighting = true; S.bRTShadows = true; S.RenderScale = 100; S.bAreaLights = true;
		S.bVolumetricFog = true; S.bFullCreatures = true; S.CreatureReflections = 1;
		break;
	case 4: // v4.8 : RTX fluide : ray tracing materiel sans les deux reglages les plus chers (hit lighting, ombres RT de la
		// lampe), TSR depuis 67 % (1440p -> 2160p : 1707x960 en 1440p), ombres des neons jusqu'a 25 m, Hound allege
		S.Quality = 3; S.bHardwareRT = true; S.bRTHitLighting = false; S.bRTShadows = false; S.RenderScale = 67; S.bAreaLights = true;
		S.bVolumetricFog = true; S.bFullCreatures = false; S.CreatureReflections = 1;
		break;
	default: // Qualite
		S.Quality = 3; S.bHardwareRT = true; S.bRTHitLighting = false; S.bRTShadows = false; S.RenderScale = 80; S.bAreaLights = true;
		S.bVolumetricFog = true; S.bFullCreatures = false; S.CreatureReflections = 1;
		break;
	}
	S.GraphicsProfile = (Profile == 4 || (Profile >= 0 && Profile <= 2)) ? Profile : 1;
}

bool ABRPlayerController::IsHardwareRayTracingAvailable()
{
	// Le ray tracing ne s'active qu'au demarrage, selon les capacites reelles : RHI (DirectX 12, Vulkan, Metal), carte et
	// pilote, r.RayTracing de la configuration de la plateforme (Config/Linux, Config/Mac : desactive dans le profil de base)
	return IsRayTracingEnabled();
}

FString ABRPlayerController::GetRenderModeText(bool bShort) const
{
	// v4.10 : variables cherchees une fois (avant : six recherches par image, signalees par le moteur)
	struct FRenderCVars
	{
		TMap<FString, IConsoleVariable*> Found;
		float Get(const TCHAR* Name, float Default)
		{
			IConsoleVariable** V = Found.Find(Name);
			if (!V)
			{
				V = &Found.Add(Name, IConsoleManager::Get().FindConsoleVariable(Name));
			}
			return *V ? (*V)->GetFloat() : Default;
		}
	};
	static FRenderCVars CVars;
	auto CVarF = [](const TCHAR* Name, float Default)
	{
		return CVars.Get(Name, Default);
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
		return BRLoc::Fmt(NSLOCTEXT("BR", "Menu.RhinameHitXXX2", "{RHIName}  \u00b7  {Hit}  \u00b7  {X}x{Y} \u2192 {X2}x{Y2} {Upscaler}{DynRes}"), { { TEXT("RHIName"), BRLoc::Arg(RHIName) }, { TEXT("Hit"), BRLoc::Arg(bLumenHW ? (bHit ? BR_STR(NSLOCTEXT("BR", "Menu.RtHitLighting", "RT + HIT LIGHTING")) : BR_STR(NSLOCTEXT("BR", "Menu.LumenRt", "LUMEN RT"))) : BR_STR(NSLOCTEXT("BR", "Menu.LumenLogiciel", "LUMEN LOGICIEL"))) }, { TEXT("X"), BRLoc::Int(In.X) }, { TEXT("Y"), BRLoc::Int(In.Y) }, { TEXT("X2"), BRLoc::Int(VP.X) }, { TEXT("Y2"), BRLoc::Int(VP.Y) }, { TEXT("Upscaler"), BRLoc::Arg(Upscaler) }, { TEXT("DynRes"), BRLoc::Arg(bDynRes ? BR_STR(NSLOCTEXT("BR", "Menu.Dyn", " dyn.")) : TEXT("")) } });
	}
	return BRLoc::Fmt(NSLOCTEXT("BR", "Menu.ModeReelRhinameSm6Ray", "Mode r\u00e9el : {RHIName} {SM6}  \u00b7  ray tracing mat\u00e9riel {RTOn}  \u00b7  Lumen {LumenHW} (reflets : {Hit})  \u00b7  ombres : {VSM}{LampRT}  \u00b7  rendu {X}x{Y} \u2192 {X2}x{Y2} ({SP} %, {Upscaler}{DynRes})"), { { TEXT("RHIName"), BRLoc::Arg(RHIName) }, { TEXT("SM6"), BRLoc::Arg(bSM6 ? TEXT("SM6") : TEXT("SM5")) }, { TEXT("RTOn"), BRLoc::Arg(bRTOn ? BR_STR(NSLOCTEXT("BR", "Menu.Actif", "actif")) : BR_STR(NSLOCTEXT("BR", "Menu.Indisponible2", "indisponible"))) }, { TEXT("LumenHW"), BRLoc::Arg(bLumenHW ? BR_STR(NSLOCTEXT("BR", "Menu.Materiel", "mat\u00e9riel")) : BR_STR(NSLOCTEXT("BR", "Menu.Logiciel", "logiciel"))) }, { TEXT("Hit"), BRLoc::Arg(bHit ? BR_STR(NSLOCTEXT("BR", "Menu.EclairesRayons", "\u00e9clair\u00e9s par les rayons")) : BR_STR(NSLOCTEXT("BR", "Menu.CacheSurfaces", "cache de surfaces"))) }, { TEXT("VSM"), BRLoc::Arg(bVSM ? BR_STR(NSLOCTEXT("BR", "Menu.VirtuellesVsm", "virtuelles (VSM)")) : BR_STR(NSLOCTEXT("BR", "Menu.CartesClassiques", "cartes classiques"))) }, { TEXT("LampRT"), BRLoc::Arg(bLampRT ? BR_STR(NSLOCTEXT("BR", "Menu.LampeRayTracee", ", lampe ray trac\u00e9e")) : TEXT("")) }, { TEXT("X"), BRLoc::Int(In.X) }, { TEXT("Y"), BRLoc::Int(In.Y) }, { TEXT("X2"), BRLoc::Int(VP.X) }, { TEXT("Y2"), BRLoc::Int(VP.Y) }, { TEXT("SP"), BRLoc::Int(FMath::RoundToInt(SP)) }, { TEXT("Upscaler"), BRLoc::Arg(Upscaler) }, { TEXT("DynRes"), BRLoc::Arg(bDynRes ? BR_STR(NSLOCTEXT("BR", "Menu.ResolutionDynamique", ", r\u00e9solution dynamique")) : TEXT("")) } });
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
	Cfg.SetFloat(SettingsSection, TEXT("EffectsVolume"), S.EffectsVolume);
	Cfg.SetFloat(SettingsSection, TEXT("VoiceVolume"), S.VoiceVolume);
	Cfg.SetBool(SettingsSection, TEXT("Subtitles"), S.bSubtitles);
	Cfg.SetInt64(SettingsSection, TEXT("VoiceMode"), S.VoiceMode);
	Cfg.SetFloat(SettingsSection, TEXT("Brightness"), S.Brightness);
	// v4.9 : WindowMode n'est plus ecrit ici (UGameUserSettings, GameUserSettings.ini)
	Cfg.SetFloat(SettingsSection, TEXT("UiScale"), S.UiScale);
	Cfg.SetFloat(SettingsSection, TEXT("HudOpacity"), S.HudOpacity);
	Cfg.SetInt64(SettingsSection, TEXT("Crosshair"), S.CrosshairMode);
	Cfg.SetInt64(SettingsSection, TEXT("QuickBar"), S.QuickBarMode);
	Cfg.SetInt64(SettingsSection, TEXT("Objectives"), S.ObjectivesMode);
	Cfg.SetInt64(SettingsSection, TEXT("CreatureReflections"), S.CreatureReflections);
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
	// Le ray tracing materiel n'est utilise que si le jeu a demarre avec (RHI, carte, r.RayTracing). v4.9 : une preference
	// RT venue d'un autre ordinateur (BackroomsPlayer.ini copie) est gardee mais sans effet ici : Lumen logiciel
	const bool bRT = S.bHardwareRT && IsHardwareRayTracingAvailable();
	Cmd(FString::Printf(TEXT("r.Lumen.HardwareRayTracing %d"), bRT ? 1 : 0));
	// v4.5 : reflets eclaires par les rayons (hit lighting) seulement si demandes (profil Cinematique) : c'est le reglage le
	// plus couteux ; sinon le cache de surfaces de Lumen eclaire les reflets
	const bool bHitLighting = bRT && S.bRTHitLighting;
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
	Cmd(FString::Printf(TEXT("r.Lumen.Reflections.HardwareRayTracing.Translucent.Refraction %d"), bRT ? 1 : 0));
	Cmd(FString::Printf(TEXT("r.VolumetricFog %d"), S.bVolumetricFog ? 1 : 0));
	Cmd(FString::Printf(TEXT("r.ScreenPercentage %d"), FMath::Clamp(S.RenderScale, 50, 100)));
	// Image plus nette (filtre de nettete du tonemapper) a partir de la qualite Elevee
	Cmd(FString::Printf(TEXT("r.Tonemapper.Sharpen %.2f"), S.Quality >= 2 ? 0.5f : 0.25f));
	// v4.8 : sans hit lighting, le cache de surfaces de Lumen n'eclaire pas les maillages a squelette : dans les reflets
	// ray traces, les entites et la combinaison devenaient noires. Ils ne sont dans la scene ray tracee que si les
	// reflets sont eclaires par les rayons, ou pour les ombres ray tracees de la lampe ; sinon les reflets les prennent a
	// l'ecran (traces d'ecran de Lumen)
	// v4.9 : CREATURES DANS LES REFLETS = RAYONS : maillages a squelette gardes dans la scene ray tracee, et les impacts
	// sans cache de surfaces (ces maillages) relances en hit lighting, si la version du moteur a cette variable. Sans elle,
	// ou sur ECRAN SEULEMENT, le compromis v4.8 reste : traces d'ecran (une creature hors champ n'est pas dans le reflet)
	IConsoleVariable* Retrace = IConsoleManager::Get().FindConsoleVariable(RetraceHitLightingCVar);
	const bool bRetrace = bRT && !bHitLighting && S.CreatureReflections == 1 && Retrace != nullptr;
	if (Retrace)
	{
		Retrace->Set(bRetrace ? 1 : 0, ECVF_SetByGameSetting);
	}
	Cmd(FString::Printf(TEXT("r.RayTracing.Geometry.SkeletalMeshes %d"), (bHitLighting || (S.bRTShadows && bRT) || bRetrace) ? 1 : 0));
	// v4.8 : TSR : historique a 100 % (au lieu de 200 % en qualite Cinematique) hors profil Cinematique : a 1440p et
	// au-dela, c'est l'un des postes les plus chers du TSR, pour un gain de nettete faible
	Cmd(FString::Printf(TEXT("r.TSR.History.ScreenPercentage %d"), S.GraphicsProfile == 2 ? 200 : 100));

	// Volume general. v4.11 : effets et voix separes. Le volume principal de l'appareil porte general x voix (il agit aussi
	// sur le chat vocal) ; les sons du jeu sont multiplies par effets / voix (au plus 4, voix au moins 25 %) : chacun obtient
	// exactement general x son propre volume
	const float VoiceGain = FMath::Clamp(S.VoiceVolume, 0.25f, 1.f);
	FAudioDeviceHandle Audio = W->GetAudioDevice();
	if (Audio.IsValid())
	{
		Audio->SetTransientPrimaryVolume(FMath::Clamp(S.MasterVolume, 0.f, 1.f) * VoiceGain);
	}
	UBRAssets::SetEffectsGain(FMath::Clamp(S.EffectsVolume, 0.f, 1.f) / VoiceGain);

	// Synchro verticale, limite d'images : pas dans l'editeur (ils agiraient sur la fenetre de l'editeur).
	// v4.9 : le mode et la resolution ne sont plus appliques ici (un profil graphique ne change jamais la fenetre) : seulement
	// par BRDisplay, sur demande, avec confirmation
	UGameUserSettings* Display = GEngine->GetGameUserSettings();
	if (Display && !GIsEditor)
	{
		Display->SetVSyncEnabled(S.bVSync);
		Display->SetFrameRateLimit(static_cast<float>(FMath::Max(0, S.MaxFPS)));
		if (!BRDisplay::IsPending())
		{
			Display->SaveSettings(); // pendant une confirmation, rien n'est ecrit (le mode a l'essai ne doit pas survivre a un arret)
		}
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
