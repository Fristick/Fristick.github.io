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
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

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
	MapKey(JournalAction, EKeys::Gamepad_Special_Left);
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
	EIC->BindAction(JournalAction, ETriggerEvent::Started, this, &ABRPlayerController::OnJournalStarted);
	EIC->BindAction(JournalAction, ETriggerEvent::Completed, this, &ABRPlayerController::OnJournalCompleted);
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
	return !bInMenu && !bPauseMenu;
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
		const FVector2D V = Value.Get<FVector2D>();
		const float K = 2.5f * MouseSensitivity;
		C->InputLook(FVector2D(V.X * K, V.Y * K * (bInvertY ? -1.f : 1.f)));
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
		const FVector2D V = Value.Get<FVector2D>();
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const float K = 140.f * Dt * MouseSensitivity;
		C->InputLook(FVector2D(V.X * K, V.Y * K * (bInvertY ? -1.f : 1.f)));
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
			C->DrinkAlmondWater();
		}
	}
}

void ABRPlayerController::OnReload(const FInputActionValue& Value)
{
	if (CanPlay())
	{
		if (ABRCharacter* C = GetBRCharacter())
		{
			C->ReplaceBattery();
		}
	}
}

void ABRPlayerController::OnJournalStarted(const FInputActionValue& Value)
{
	bJournal = !bInMenu;
}

void ABRPlayerController::OnJournalCompleted(const FInputActionValue& Value)
{
	bJournal = false;
}

void ABRPlayerController::OnPause(const FInputActionValue& Value)
{
	if (bInMenu)
	{
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
	MouseSensitivity = FMath::Clamp(Value, 0.05f, 10.f);
	ABRHUD::Notify(this, FString::Printf(TEXT("Sensibilit\u00e9 : %.2f"), MouseSensitivity), 2.f);
}

void ABRPlayerController::BRInvertY()
{
	bInvertY = !bInvertY;
	ABRHUD::Notify(this, bInvertY ? TEXT("Axe Y invers\u00e9") : TEXT("Axe Y normal"), 2.f);
}

void ABRPlayerController::BRGiveAll()
{
	if (ABRCharacter* C = GetBRCharacter())
	{
		C->AlmondWater += 5;
		C->Batteries += 5;
		C->Sanity = 100.f;
		C->Health = 100.f;
	}
}
