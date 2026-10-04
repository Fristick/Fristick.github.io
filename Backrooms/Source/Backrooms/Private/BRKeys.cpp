#include "BRKeys.h"
#include "BRConfig.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* KeysSection = TEXT("/Script/Backrooms.BRKeys");

	struct FActionDef
	{
		const TCHAR* Id;      // nom dans GameUserSettings.ini
		const TCHAR* Label;   // libelle affiche
	};

	const FActionDef& Def(EBRAction Action)
	{
		static const FActionDef Defs[] = {
			{ TEXT("MoveForward"), TEXT("AVANCER") },
			{ TEXT("MoveBackward"), TEXT("RECULER") },
			{ TEXT("MoveLeft"), TEXT("ALLER \u00c0 GAUCHE") },
			{ TEXT("MoveRight"), TEXT("ALLER \u00c0 DROITE") },
			{ TEXT("Jump"), TEXT("SAUTER / REMONTER (NAGE)") },
			{ TEXT("Sprint"), TEXT("COURIR / NAGER VITE") },
			{ TEXT("Crouch"), TEXT("S'ACCROUPIR / PLONGER") },
			{ TEXT("Interact"), TEXT("INTERAGIR / RAMASSER") },
			{ TEXT("Flashlight"), TEXT("LAMPE") },
			{ TEXT("NightVision"), TEXT("VISION NOCTURNE") },
			{ TEXT("Inventory"), TEXT("INVENTAIRE") },
			{ TEXT("Pocket1"), TEXT("POCHE 1") },
			{ TEXT("Pocket2"), TEXT("POCHE 2") },
			{ TEXT("Pocket3"), TEXT("POCHE 3") },
			{ TEXT("Pocket4"), TEXT("POCHE 4") },
			{ TEXT("Drink"), TEXT("BOIRE DE L'EAU D'AMANDE") },
			{ TEXT("Bandage"), TEXT("METTRE UN BANDAGE") },
			{ TEXT("Battery"), TEXT("CHANGER LES PILES") },
			{ TEXT("ThirdPerson"), TEXT("VUE \u00c0 LA 3E PERSONNE") },
			{ TEXT("Pause"), TEXT("PAUSE") },
			{ TEXT("PushToTalk"), TEXT("PARLER (CHAT VOCAL)") },
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
		return Def(Action).Label;
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
		return Key.GetDisplayName(false).ToString().ToUpper();
	}

	FString Primary(EBRAction Action)
	{
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
}
