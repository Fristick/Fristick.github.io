#include "BRPlayerController.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRWorld.h"
#include "BRLevels.h"
#include "BRHUD.h"
#include "BREntity.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* SettingsSection = TEXT("/Script/Backrooms.BRSettings");

	enum ESettingRow
	{
		Row_Sensitivity,
		Row_InvertY,
		Row_FOV,
		Row_Quality,
		Row_HardwareRT,
		Row_RTHitLighting,
		Row_AreaLights,
		Row_VolumetricFog,
		Row_FilmGrain,
		Row_Count
	};

	const TCHAR* QualityNames[] = { TEXT("BAS"), TEXT("MOYEN"), TEXT("\u00c9LEV\u00c9"), TEXT("\u00c9PIQUE"), TEXT("CIN\u00c9MATIQUE") };

	FString OnOff(bool b)
	{
		return b ? FString(TEXT("ACTIV\u00c9")) : FString(TEXT("D\u00c9SACTIV\u00c9"));
	}
}

ABRPlayerController::ABRPlayerController()
{
	bShowMouseCursor = false;
}

void ABRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureInput();
	AddMappingToPlayer();

	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;

	LoadSettings();
	ApplySettings();

	if (const ABRWorld* W = ABRWorld::Get(this))
	{
		MenuIndex = BRLevels::IndexOf(W->StartLevel);
	}
}

UInputAction* ABRPlayerController::MakeAction(const TCHAR* Name, EInputActionValueType Type, bool bWhenPaused)
{
	UInputAction* A = NewObject<UInputAction>(this, FName(Name));
	A->ValueType = Type;
	A->bTriggerWhenPaused = bWhenPaused;
	return A;
}

void ABRPlayerController::MapKey(UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate)
{
	FEnhancedActionKeyMapping& M = Mapping->MapKey(Action, Key);
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
	if (Mapping)
	{
		return;
	}
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Backrooms"));

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
	JournalAction = MakeAction(TEXT("IA_Journal"), EInputActionValueType::Boolean);
	PauseAction = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean, true);
	QuitAction = MakeAction(TEXT("IA_Quit"), EInputActionValueType::Boolean, true);
	MenuPrevAction = MakeAction(TEXT("IA_MenuPrev"), EInputActionValueType::Boolean);
	MenuNextAction = MakeAction(TEXT("IA_MenuNext"), EInputActionValueType::Boolean);
	MenuConfirmAction = MakeAction(TEXT("IA_MenuConfirm"), EInputActionValueType::Boolean);
	NightVisionAction = MakeAction(TEXT("IA_NightVision"), EInputActionValueType::Boolean);
	BandageAction = MakeAction(TEXT("IA_Bandage"), EInputActionValueType::Boolean);
	PocketActions.Reset();
	for (int32 i = 0; i < 4; ++i)
	{
		PocketActions.Add(MakeAction(*FString::Printf(TEXT("IA_Pocket%d"), i + 1), EInputActionValueType::Boolean));
	}

	// Deplacement : ZQSD / WASD (on mappe les deux dispositions de clavier)
	MapKey(MoveAction, EKeys::W, true, false);
	MapKey(MoveAction, EKeys::Z, true, false);
	MapKey(MoveAction, EKeys::S, true, true);
	MapKey(MoveAction, EKeys::D, false, false);
	MapKey(MoveAction, EKeys::A, false, true);
	MapKey(MoveAction, EKeys::Q, false, true);
	MapKey(MoveAction, EKeys::Up, true, false);
	MapKey(MoveAction, EKeys::Down, true, true);
	MapKey(MoveAction, EKeys::Gamepad_Left2D);

	MapKey(LookAction, EKeys::Mouse2D);
	MapKey(LookPadAction, EKeys::Gamepad_Right2D);

	MapKey(JumpAction, EKeys::SpaceBar);
	MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	MapKey(SprintAction, EKeys::LeftShift);
	MapKey(SprintAction, EKeys::Gamepad_LeftThumbstick);
	MapKey(CrouchAction, EKeys::LeftControl);
	MapKey(CrouchAction, EKeys::C);
	MapKey(CrouchAction, EKeys::Gamepad_FaceButton_Right);
	MapKey(FlashAction, EKeys::F);
	MapKey(FlashAction, EKeys::Gamepad_FaceButton_Top);
	MapKey(InteractAction, EKeys::E);
	MapKey(InteractAction, EKeys::Gamepad_FaceButton_Left);
	MapKey(DrinkAction, EKeys::B);
	MapKey(DrinkAction, EKeys::Gamepad_LeftShoulder);
	MapKey(ReloadAction, EKeys::R);
	MapKey(ReloadAction, EKeys::Gamepad_RightShoulder);
	MapKey(JournalAction, EKeys::Tab);
	MapKey(JournalAction, EKeys::I);
	MapKey(JournalAction, EKeys::Gamepad_Special_Left);
	MapKey(NightVisionAction, EKeys::N);
	MapKey(NightVisionAction, EKeys::Gamepad_DPad_Up);
	MapKey(BandageAction, EKeys::H);
	MapKey(BandageAction, EKeys::Gamepad_DPad_Down);
	// Poches 1-4 : rangee des chiffres en QWERTY et en AZERTY (& e " ')
	const FKey PocketKeys[4][3] = {
		{ EKeys::One, EKeys::Ampersand, EKeys::NumPadOne },
		{ EKeys::Two, EKeys::E_AccentAigu, EKeys::NumPadTwo },
		{ EKeys::Three, EKeys::Quote, EKeys::NumPadThree },
		{ EKeys::Four, EKeys::Apostrophe, EKeys::NumPadFour },
	};
	for (int32 i = 0; i < 4; ++i)
	{
		for (const FKey& K : PocketKeys[i])
		{
			MapKey(PocketActions[i], K);
		}
	}
	MapKey(PauseAction, EKeys::P);
	MapKey(PauseAction, EKeys::Escape);
	MapKey(PauseAction, EKeys::Gamepad_Special_Right);
	MapKey(QuitAction, EKeys::End);

	MapKey(MenuPrevAction, EKeys::Left);
	MapKey(MenuPrevAction, EKeys::A);
	MapKey(MenuPrevAction, EKeys::Q);
	MapKey(MenuPrevAction, EKeys::Gamepad_DPad_Left);
	MapKey(MenuNextAction, EKeys::Right);
	MapKey(MenuNextAction, EKeys::D);
	MapKey(MenuNextAction, EKeys::Gamepad_DPad_Right);
	MapKey(MenuConfirmAction, EKeys::Enter);
	MapKey(MenuConfirmAction, EKeys::SpaceBar);
	MapKey(MenuConfirmAction, EKeys::Gamepad_FaceButton_Bottom);
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
	EIC->BindAction(DrinkAction, ETriggerEvent::Started, this, &ABRPlayerController::OnDrink);
	EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &ABRPlayerController::OnReload);
	EIC->BindAction(JournalAction, ETriggerEvent::Started, this, &ABRPlayerController::OnInventory);
	EIC->BindAction(NightVisionAction, ETriggerEvent::Started, this, &ABRPlayerController::OnNightVision);
	EIC->BindAction(BandageAction, ETriggerEvent::Started, this, &ABRPlayerController::OnBandage);
	EIC->BindAction(PocketActions[0], ETriggerEvent::Started, this, &ABRPlayerController::OnPocket1);
	EIC->BindAction(PocketActions[1], ETriggerEvent::Started, this, &ABRPlayerController::OnPocket2);
	EIC->BindAction(PocketActions[2], ETriggerEvent::Started, this, &ABRPlayerController::OnPocket3);
	EIC->BindAction(PocketActions[3], ETriggerEvent::Started, this, &ABRPlayerController::OnPocket4);
	EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &ABRPlayerController::OnPause);
	EIC->BindAction(QuitAction, ETriggerEvent::Started, this, &ABRPlayerController::OnQuit);
	EIC->BindAction(MenuPrevAction, ETriggerEvent::Started, this, &ABRPlayerController::OnMenuPrev);
	EIC->BindAction(MenuNextAction, ETriggerEvent::Started, this, &ABRPlayerController::OnMenuNext);
	EIC->BindAction(MenuConfirmAction, ETriggerEvent::Started, this, &ABRPlayerController::OnMenuConfirm);
}

ABRCharacter* ABRPlayerController::GetBRCharacter() const
{
	return Cast<ABRCharacter>(GetPawn());
}

bool ABRPlayerController::CanPlay() const
{
	return !bInMenu && !bPauseMenu && !bInventory;
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
		}
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

void ABRPlayerController::SetInventoryOpen(bool bOpen)
{
	if (bOpen && (bInMenu || bPauseMenu))
	{
		return;
	}
	if (bOpen == bInventory)
	{
		return;
	}
	bInventory = bOpen;
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->SetSprinting(false);
		C->PlayUISound(TEXT("S_Inventory"));
	}
	// Curseur visible pour glisser-deposer les objets ; le monde continue de vivre (comme dans Escape Together)
	bShowMouseCursor = bOpen;
	if (bOpen)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		SetInputMode(Mode);
		int32 SX = 0;
		int32 SY = 0;
		GetViewportSize(SX, SY);
		SetMouseLocation(SX / 2, SY / 2);
	}
	else
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
	}
}

void ABRPlayerController::OnInventory(const FInputActionValue& Value)
{
	if (bInMenu || bPauseMenu)
	{
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
	if (bInMenu)
	{
		return;
	}
	if (bInventory)
	{
		SetInventoryOpen(false); // Echap ferme d'abord l'inventaire
		return;
	}
	bPauseMenu = !bPauseMenu;
	SetPause(bPauseMenu);
}

void ABRPlayerController::OnQuit(const FInputActionValue& Value)
{
	if (bPauseMenu || bInMenu)
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
}

void ABRPlayerController::OnMenuPrev(const FInputActionValue& Value)
{
	if (bInMenu)
	{
		const int32 Num = BRLevels::All().Num();
		MenuIndex = (MenuIndex + Num - 1) % Num;
	}
}

void ABRPlayerController::OnMenuNext(const FInputActionValue& Value)
{
	if (bInMenu)
	{
		MenuIndex = (MenuIndex + 1) % BRLevels::All().Num();
	}
}

void ABRPlayerController::OnMenuConfirm(const FInputActionValue& Value)
{
	if (!bInMenu)
	{
		return;
	}
	ABRWorld* W = ABRWorld::Get(this);
	if (!W || W->IsTransitioning())
	{
		return;
	}
	bInMenu = false;
	const int32 Target = BRLevels::All()[MenuIndex].Number;
	if (Target != W->GetLevelNumber())
	{
		W->RequestTransition(Target);
	}
	else
	{
		W->ReplayTitle();
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
	if (ABRWorld* W = ABRWorld::Get(this))
	{
		W->ForceBlackout();
	}
}

void ABRPlayerController::BRObjectives()
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W)
	{
		return;
	}
	const int32 Missing = FMath::Max(0, W->Def().VHSRequired - W->GetVHSFound());
	for (int32 i = 0; i < Missing; ++i)
	{
		W->OnVHSCollected();
	}
	W->DebugCompleteRecording();
}

// =====================================================================================================================
// Parametres (sauvegardes dans Saved/Config/<plateforme>/GameUserSettings.ini)
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
		return TEXT("GRAIN DE CAM\u00c9SCOPE");
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
	default:
		return FString();
	}
}

FString ABRPlayerController::GetSettingHint(int32 Index) const
{
	switch (Index)
	{
	case Row_Quality:
		return TEXT("Ombres, Lumen, textures, anti-cr\u00e9nelage (scalability).");
	case Row_HardwareRT:
		return TEXT("Lumen en ray tracing mat\u00e9riel : reflets et lumi\u00e8re indirecte bien plus pr\u00e9cis (carte RTX / RX 6000+ requise).");
	case Row_RTHitLighting:
		return TEXT("Reflets calcul\u00e9s par lancer de rayons complet (eau, carrelage...). Tr\u00e8s co\u00fbteux.");
	case Row_AreaLights:
		return TEXT("Ombres douces des n\u00e9ons. S'applique aux zones charg\u00e9es ensuite.");
	case Row_VolumetricFog:
		return TEXT("Halos de lumi\u00e8re dans l'air humide.");
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
	if (!GConfig)
	{
		return;
	}
	FBRSettings& S = FBRSettings::Get();
	GConfig->GetFloat(SettingsSection, TEXT("Sensitivity"), S.Sensitivity, GGameUserSettingsIni);
	GConfig->GetBool(SettingsSection, TEXT("InvertY"), S.bInvertY, GGameUserSettingsIni);
	GConfig->GetFloat(SettingsSection, TEXT("FOV"), S.FOV, GGameUserSettingsIni);
	GConfig->GetInt(SettingsSection, TEXT("Quality"), S.Quality, GGameUserSettingsIni);
	GConfig->GetBool(SettingsSection, TEXT("HardwareRT"), S.bHardwareRT, GGameUserSettingsIni);
	GConfig->GetBool(SettingsSection, TEXT("RTHitLighting"), S.bRTHitLighting, GGameUserSettingsIni);
	GConfig->GetBool(SettingsSection, TEXT("AreaLights"), S.bAreaLights, GGameUserSettingsIni);
	GConfig->GetBool(SettingsSection, TEXT("VolumetricFog"), S.bVolumetricFog, GGameUserSettingsIni);
	GConfig->GetBool(SettingsSection, TEXT("FilmGrain"), S.bFilmGrain, GGameUserSettingsIni);
	S.Sensitivity = FMath::Clamp(S.Sensitivity, 0.1f, 5.f);
	S.FOV = FMath::Clamp(S.FOV, 70.f, 110.f);
	S.Quality = FMath::Clamp(S.Quality, 0, 4);
}

void ABRPlayerController::SaveSettings() const
{
	if (!GConfig)
	{
		return;
	}
	const FBRSettings& S = FBRSettings::Get();
	GConfig->SetFloat(SettingsSection, TEXT("Sensitivity"), S.Sensitivity, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("InvertY"), S.bInvertY, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("FOV"), S.FOV, GGameUserSettingsIni);
	GConfig->SetInt(SettingsSection, TEXT("Quality"), S.Quality, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("HardwareRT"), S.bHardwareRT, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("RTHitLighting"), S.bRTHitLighting, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("AreaLights"), S.bAreaLights, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("VolumetricFog"), S.bVolumetricFog, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("FilmGrain"), S.bFilmGrain, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
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
	Cmd(FString::Printf(TEXT("r.Lumen.HardwareRayTracing.LightingMode %d"), (S.bHardwareRT && S.bRTHitLighting) ? 1 : 0));
	Cmd(FString::Printf(TEXT("r.Lumen.Reflections.HardwareRayTracing.Translucent.Refraction %d"), S.bHardwareRT ? 1 : 0));
	Cmd(FString::Printf(TEXT("r.VolumetricFog %d"), S.bVolumetricFog ? 1 : 0));
}
