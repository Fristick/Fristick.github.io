#include "BRKeys.h"
#include "GameFramework/PlayerController.h"
#include "BRConfig.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* KeysSection = TEXT("/Script/Backrooms.BRKeys");

	struct FActionDef
	{
		const TCHAR* Id;      // nom dans GameUserSettings.ini
		FText Label;          // libelle affiche (v4.8 : localise)
	};

	const FActionDef& Def(EBRAction Action)
	{
		static const FActionDef Defs[] = {
			{ TEXT("MoveForward"), NSLOCTEXT("BR", "Key.Action.MoveForward", "AVANCER") },
			{ TEXT("MoveBackward"), NSLOCTEXT("BR", "Key.Action.MoveBackward", "RECULER") },
			{ TEXT("MoveLeft"), NSLOCTEXT("BR", "Key.Action.MoveLeft", "ALLER \u00c0 GAUCHE") },
			{ TEXT("MoveRight"), NSLOCTEXT("BR", "Key.Action.MoveRight", "ALLER \u00c0 DROITE") },
			{ TEXT("Jump"), NSLOCTEXT("BR", "Key.Action.Jump", "SAUTER / REMONTER (NAGE)") },
			{ TEXT("Sprint"), NSLOCTEXT("BR", "Key.Action.Sprint", "COURIR / NAGER VITE") },
			{ TEXT("Crouch"), NSLOCTEXT("BR", "Key.Action.Crouch", "S'ACCROUPIR / PLONGER") },
			{ TEXT("Interact"), NSLOCTEXT("BR", "Key.Action.Interact", "INTERAGIR / RAMASSER") },
			{ TEXT("Flashlight"), NSLOCTEXT("BR", "Key.Action.Flashlight", "LAMPE") },
			{ TEXT("NightVision"), NSLOCTEXT("BR", "Key.Action.NightVision", "VISION NOCTURNE") },
			{ TEXT("Inventory"), NSLOCTEXT("BR", "Key.Action.Inventory", "INVENTAIRE") },
			{ TEXT("Pocket1"), NSLOCTEXT("BR", "Key.Action.Pocket1", "POCHE 1") },
			{ TEXT("Pocket2"), NSLOCTEXT("BR", "Key.Action.Pocket2", "POCHE 2") },
			{ TEXT("Pocket3"), NSLOCTEXT("BR", "Key.Action.Pocket3", "POCHE 3") },
			{ TEXT("Pocket4"), NSLOCTEXT("BR", "Key.Action.Pocket4", "POCHE 4") },
			{ TEXT("Drink"), NSLOCTEXT("BR", "Key.Action.Drink", "BOIRE DE L'EAU D'AMANDE") },
			{ TEXT("Bandage"), NSLOCTEXT("BR", "Key.Action.Bandage", "METTRE UN BANDAGE") },
			{ TEXT("Battery"), NSLOCTEXT("BR", "Key.Action.Battery", "CHANGER LES PILES") },
			{ TEXT("ThirdPerson"), NSLOCTEXT("BR", "Key.Action.ThirdPerson", "VUE \u00c0 LA 3E PERSONNE") },
			{ TEXT("Pause"), NSLOCTEXT("BR", "Key.Action.Pause", "PAUSE") },
			{ TEXT("PushToTalk"), NSLOCTEXT("BR", "Key.Action.PushToTalk", "PARLER (CHAT VOCAL)") },
			{ TEXT("LidarPulse"), NSLOCTEXT("BR", "Key.Action.LidarPulse", "SCANNER : IMPULSION") },
			{ TEXT("LidarFocus"), NSLOCTEXT("BR", "Key.Action.LidarFocus", "SCANNER : SCAN FOCALIS\u00c9 (MAINTENIR)") },
		};
		static_assert(UE_ARRAY_COUNT(Defs) == static_cast<int32>(EBRAction::Count), "Une ligne par action");
		return Defs[FMath::Clamp(static_cast<int32>(Action), 0, static_cast<int32>(EBRAction::Count) - 1)];
	}

	TArray<FKey>& Table()
	{
		static TArray<FKey> Keys;
		if (Keys.Num() != static_cast<int32>(EBRAction::Count) * BRKeys::SlotsPerAction)
		{
			Keys.SetNum(static_cast<int32>(EBRAction::Count) * BRKeys::SlotsPerAction);
		}
		return Keys;
	}

	int32 GRevision = 1;
	bool GInitialized = false;
	/** v4.16 : la manette a servi en dernier (invites de ses boutons) */
	bool GPadPrompts = false;
	bool GKeyCursor = false;

	void SetDefault(EBRAction A, const FKey& K0, const FKey& K1 = FKey(), const FKey& K2 = FKey())
	{
		const int32 Base = static_cast<int32>(A) * BRKeys::SlotsPerAction;
		Table()[Base] = K0;
		Table()[Base + 1] = K1;
		Table()[Base + 2] = K2;
	}

	void EnsureInit()
	{
		if (!GInitialized)
		{
			GInitialized = true;
			BRKeys::ResetDefaults();
		}
	}
}

namespace BRKeys
{
	int32 NumActions()
	{
		return static_cast<int32>(EBRAction::Count);
	}

	FString ActionLabel(EBRAction Action)
	{
		return Def(Action).Label.ToString();
	}

	FKey GetKey(EBRAction Action, int32 Slot)
	{
		EnsureInit();
		const int32 I = static_cast<int32>(Action) * SlotsPerAction + Slot;
		return Table().IsValidIndex(I) && Slot >= 0 && Slot < SlotsPerAction ? Table()[I] : FKey();
	}

	void SetKey(EBRAction Action, int32 Slot, const FKey& Key, FString* OutRemovedFrom)
	{
		EnsureInit();
		if (Slot < 0 || Slot >= SlotsPerAction)
		{
			return;
		}
		TArray<FKey>& T = Table();
		const int32 Target = static_cast<int32>(Action) * SlotsPerAction + Slot;
		if (Key.IsValid())
		{
			for (int32 i = 0; i < T.Num(); ++i)
			{
				if (i != Target && T[i] == Key)
				{
					T[i] = FKey();
					if (OutRemovedFrom)
					{
						*OutRemovedFrom = ActionLabel(static_cast<EBRAction>(i / SlotsPerAction));
					}
				}
			}
		}
		T[Target] = Key;
		++GRevision;
	}

	void ResetDefaults()
	{
		GInitialized = true;
		SetDefault(EBRAction::MoveForward, EKeys::Z, EKeys::W, EKeys::Up);
		SetDefault(EBRAction::MoveBackward, EKeys::S, EKeys::Down);
		SetDefault(EBRAction::MoveLeft, EKeys::Q, EKeys::A, EKeys::Left);
		SetDefault(EBRAction::MoveRight, EKeys::D, EKeys::Right);
		SetDefault(EBRAction::Jump, EKeys::SpaceBar);
		SetDefault(EBRAction::Sprint, EKeys::LeftShift);
		SetDefault(EBRAction::Crouch, EKeys::LeftControl, EKeys::C);
		SetDefault(EBRAction::Interact, EKeys::E);
		SetDefault(EBRAction::Flashlight, EKeys::F);
		SetDefault(EBRAction::NightVision, EKeys::N);
		SetDefault(EBRAction::Inventory, EKeys::Tab, EKeys::I);
		SetDefault(EBRAction::Pocket1, EKeys::One, EKeys::Ampersand, EKeys::NumPadOne);
		SetDefault(EBRAction::Pocket2, EKeys::Two, EKeys::E_AccentAigu, EKeys::NumPadTwo);
		SetDefault(EBRAction::Pocket3, EKeys::Three, EKeys::Quote, EKeys::NumPadThree);
		SetDefault(EBRAction::Pocket4, EKeys::Four, EKeys::Apostrophe, EKeys::NumPadFour);
		SetDefault(EBRAction::Drink, EKeys::B);
		SetDefault(EBRAction::Bandage, EKeys::H);
		SetDefault(EBRAction::Battery, EKeys::R);
		SetDefault(EBRAction::ThirdPerson, EKeys::V);
		SetDefault(EBRAction::Pause, EKeys::P, EKeys::Escape);
		SetDefault(EBRAction::PushToTalk, EKeys::T);
		SetDefault(EBRAction::LidarPulse, EKeys::RightMouseButton, EKeys::G);
		SetDefault(EBRAction::LidarFocus, EKeys::X, EKeys::MiddleMouseButton);
		++GRevision;
	}

	void Load()
	{
		EnsureInit();
		const FConfigFile& Cfg = BRConfig::Get();
		for (int32 A = 0; A < NumActions(); ++A)
		{
			FString Line;
			if (!Cfg.GetString(KeysSection, Def(static_cast<EBRAction>(A)).Id, Line))
			{
				continue;
			}
			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT("|"), false);
			for (int32 Slot = 0; Slot < SlotsPerAction; ++Slot)
			{
				FKey K;
				if (Parts.IsValidIndex(Slot) && !Parts[Slot].IsEmpty() && Parts[Slot] != TEXT("None"))
				{
					K = FKey(FName(*Parts[Slot]));
					if (!K.IsValid() || !IsBindable(K))
					{
						K = FKey();
					}
				}
				Table()[A * SlotsPerAction + Slot] = K;
			}
		}
		++GRevision;
	}

	void Save()
	{
		EnsureInit();
		FConfigFile& Cfg = BRConfig::Get();
		for (int32 A = 0; A < NumActions(); ++A)
		{
			FString Line;
			for (int32 Slot = 0; Slot < SlotsPerAction; ++Slot)
			{
				const FKey& K = Table()[A * SlotsPerAction + Slot];
				Line += (Slot > 0 ? TEXT("|") : TEXT("")) + (K.IsValid() ? K.GetFName().ToString() : FString(TEXT("None")));
			}
			Cfg.SetString(KeysSection, Def(static_cast<EBRAction>(A)).Id, *Line);
		}
		BRConfig::Save();
	}

	FString KeyName(const FKey& Key)
	{
		if (!Key.IsValid())
		{
			return TEXT("-");
		}
		// v4.17 (accessibilite) : capitales selon la langue (FText::ToUpper, ICU) ; FString::ToUpper ne traite que l'ASCII
		// (avant : "BOTóN", "BOTãO")
		return Key.GetDisplayName(false).ToUpper().ToString();
	}

	FString Primary(EBRAction Action)
	{
		// v4.16 : manette : son bouton ; les quatre actions du deplacement nomment le stick gauche (Expand ne le repete pas
		// quand elles se suivent : "{MoveForward}{MoveLeft}{MoveBackward}{MoveRight}" donne "[STICK GAUCHE]", l'echelle garde
		// "[STICK GAUCHE] monter - [STICK GAUCHE] descendre")
		if (GPadPrompts)
		{
			const FKey Pad = GamepadKey(Action);
			if (Pad.IsValid())
			{
				return GamepadName(Pad);
			}
		}
		for (int32 Slot = 0; Slot < SlotsPerAction; ++Slot)
		{
			const FKey K = GetKey(Action, Slot);
			if (K.IsValid())
			{
				return KeyName(K);
			}
		}
		return TEXT("?");
	}

	FString Tag(EBRAction Action)
	{
		return FString::Printf(TEXT("[%s]"), *Primary(Action));
	}

	FString Expand(const FString& Text)
	{
		if (!Text.Contains(TEXT("{")))
		{
			return Text;
		}
		FString Out = Text;
		for (int32 A = 0; A < NumActions(); ++A)
		{
			const FString Token = FString::Printf(TEXT("{%s}"), Def(static_cast<EBRAction>(A)).Id);
			if (Out.Contains(Token))
			{
				Out = Out.Replace(*Token, *Tag(static_cast<EBRAction>(A)));
			}
		}
		if (GPadPrompts)
		{
			// v4.16 : manette : le stick gauche une seule fois pour les quatre directions qui se suivent
			const FString Stick = Tag(EBRAction::MoveForward);
			const FString Twice = Stick + Stick;
			while (Out.Contains(Twice))
			{
				Out.ReplaceInline(*Twice, *Stick);
			}
		}
		return Out;
	}

	bool IsBindable(const FKey& Key)
	{
		if (!Key.IsValid() || Key == EKeys::LeftMouseButton || Key == EKeys::AnyKey)
		{
			return false;
		}
		if (Key.IsGamepadKey() || Key.IsTouch() || Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D())
		{
			return false;
		}
		return true;
	}

	int32 Revision()
	{
		EnsureInit();
		return GRevision;
	}

	void SetGamepadPrompts(bool bPad)
	{
		GPadPrompts = bPad;
	}

	bool UsesGamepadPrompts()
	{
		return GPadPrompts;
	}

	void SetKeyboardCursor(bool bKeys)
	{
		GKeyCursor = bKeys;
	}

	bool UsesKeyboardCursor()
	{
		return GKeyCursor && !GPadPrompts;
	}

	bool IsActionKeyDown(const APlayerController* PC, EBRAction Action)
	{
		if (!PC)
		{
			return false;
		}
		for (int32 Slot = 0; Slot < SlotsPerAction; ++Slot)
		{
			const FKey Key = GetKey(Action, Slot);
			if (Key.IsValid() && !Key.IsGamepadKey() && PC->IsInputKeyDown(Key))
			{
				return true;
			}
		}
		return false;
	}

	FKey GamepadKey(EBRAction Action)
	{
		// v4.16 : meme table que les liaisons fixes de la manette (ABRPlayerController::RebuildMappings) ; le test
		// -BRAutoTestV416Visual compare les deux
		switch (Action)
		{
		case EBRAction::MoveForward:
		case EBRAction::MoveBackward:
		case EBRAction::MoveLeft:
		case EBRAction::MoveRight:
			return EKeys::Gamepad_Left2D;
		case EBRAction::Jump:
			return EKeys::Gamepad_FaceButton_Bottom;
		case EBRAction::Sprint:
			return EKeys::Gamepad_LeftThumbstick;
		case EBRAction::Crouch:
			return EKeys::Gamepad_FaceButton_Right;
		case EBRAction::Interact:
			return EKeys::Gamepad_FaceButton_Left;
		case EBRAction::Flashlight:
			return EKeys::Gamepad_FaceButton_Top;
		case EBRAction::NightVision:
			return EKeys::Gamepad_DPad_Up;
		case EBRAction::Inventory:
			return EKeys::Gamepad_Special_Left;
		case EBRAction::Drink:
			return EKeys::Gamepad_LeftShoulder;
		case EBRAction::Bandage:
			return EKeys::Gamepad_DPad_Down;
		case EBRAction::Battery:
			return EKeys::Gamepad_RightShoulder;
		case EBRAction::ThirdPerson:
			return EKeys::Gamepad_RightThumbstick;
		case EBRAction::Pause:
			return EKeys::Gamepad_Special_Right;
		case EBRAction::LidarPulse:
			return EKeys::Gamepad_LeftTrigger;
		case EBRAction::LidarFocus:
			return EKeys::Gamepad_RightTrigger;
		default:
			return FKey();
		}
	}

	FString GamepadName(const FKey& Key)
	{
		// Noms des boutons d'une manette au format Xbox (les plus repandus sur PC) ; mots traduits pour les boutons sans lettre
		// (fleches absentes de la police de l'interface : "CROIX HAUT" plutot qu'une fleche)
		if (Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			return TEXT("A");
		}
		if (Key == EKeys::Gamepad_FaceButton_Right)
		{
			return TEXT("B");
		}
		if (Key == EKeys::Gamepad_FaceButton_Left)
		{
			return TEXT("X");
		}
		if (Key == EKeys::Gamepad_FaceButton_Top)
		{
			return TEXT("Y");
		}
		if (Key == EKeys::Gamepad_LeftShoulder)
		{
			return TEXT("LB");
		}
		if (Key == EKeys::Gamepad_RightShoulder)
		{
			return TEXT("RB");
		}
		if (Key == EKeys::Gamepad_LeftTrigger)
		{
			return TEXT("LT");
		}
		if (Key == EKeys::Gamepad_RightTrigger)
		{
			return TEXT("RT");
		}
		if (Key == EKeys::Gamepad_LeftThumbstick)
		{
			return TEXT("LS");
		}
		if (Key == EKeys::Gamepad_RightThumbstick)
		{
			return TEXT("RS");
		}
		if (Key == EKeys::Gamepad_Special_Left)
		{
			return NSLOCTEXT("BR", "Key.Pad.View", "VUE").ToString();
		}
		if (Key == EKeys::Gamepad_Special_Right)
		{
			return NSLOCTEXT("BR", "Key.Pad.Menu", "MENU").ToString();
		}
		if (Key == EKeys::Gamepad_DPad_Up)
		{
			return NSLOCTEXT("BR", "Key.Pad.DpadUp", "CROIX HAUT").ToString();
		}
		if (Key == EKeys::Gamepad_DPad_Down)
		{
			return NSLOCTEXT("BR", "Key.Pad.DpadDown", "CROIX BAS").ToString();
		}
		if (Key == EKeys::Gamepad_DPad_Left)
		{
			return NSLOCTEXT("BR", "Key.Pad.DpadLeft", "CROIX GAUCHE").ToString();
		}
		if (Key == EKeys::Gamepad_DPad_Right)
		{
			return NSLOCTEXT("BR", "Key.Pad.DpadRight", "CROIX DROITE").ToString();
		}
		if (Key == EKeys::Gamepad_Left2D)
		{
			return NSLOCTEXT("BR", "Key.Pad.LeftStick", "STICK GAUCHE").ToString();
		}
		return KeyName(Key);
	}

	FString GamepadDpadName()
	{
		// v4.16 (relecture) : la croix entiere (curseur des cases de l'inventaire, cartes de la pause)
		return NSLOCTEXT("BR", "Key.Pad.Dpad", "CROIX").ToString();
	}
}
