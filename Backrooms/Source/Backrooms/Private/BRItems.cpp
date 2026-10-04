#include "BRItems.h"

namespace
{
	FBRItemInfo Make(const TCHAR* Name, const TCHAR* Short, const TCHAR* Desc, const TCHAR* Mesh, const TCHAR* Icon, int32 Stack,
		EBREquipSlot Slot, bool bConsumable, const TCHAR* Verb)
	{
		FBRItemInfo I;
		I.Name = Name;
		I.Short = Short;
		I.Description = Desc;
		I.Mesh = FName(Mesh);
		I.Icon = FName(Icon);
		I.MaxStack = Stack;
		I.Slot = Slot;
		I.bConsumable = bConsumable;
		I.UseVerb = Verb;
		return I;
	}

	TArray<FBRItemInfo> BuildItems()
	{
		TArray<FBRItemInfo> L;
		L.SetNum(static_cast<int32>(EBRItem::Count));
		L[static_cast<int32>(EBRItem::None)] = Make(TEXT("Vide"), TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, EBREquipSlot::None, false, TEXT(""));
		L[static_cast<int32>(EBRItem::AlmondWater)] = Make(TEXT("Eau d'amande"), TEXT("EAU D'AMANDE"),
			TEXT("Une bouteille d'eau d'amande, l\u00e9g\u00e8rement sucr\u00e9e. Apaise l'esprit : +40 sant\u00e9 mentale, +10 sant\u00e9."),
			TEXT("SM_AlmondWater"), TEXT("I_AlmondWater"), 4, EBREquipSlot::None, true, TEXT("Boire"));
		L[static_cast<int32>(EBRItem::Bandage)] = Make(TEXT("Bandages"), TEXT("BANDAGES"),
			TEXT("Un rouleau de gaze st\u00e9rile. Soigne les blessures : +35 sant\u00e9."),
			TEXT("SM_Bandage"), TEXT("I_Bandage"), 4, EBREquipSlot::None, true, TEXT("Utiliser"));
		L[static_cast<int32>(EBRItem::Battery)] = Make(TEXT("Piles"), TEXT("PILES"),
			TEXT("Deux grosses piles. Rechargent la lampe torche, la lampe frontale et le cam\u00e9scope."),
			TEXT("SM_Battery"), TEXT("I_Battery"), 6, EBREquipSlot::None, true, TEXT("Recharger"));
		L[static_cast<int32>(EBRItem::EnergyBar)] = Make(TEXT("Barre \u00e9nerg\u00e9tique"), TEXT("BARRE"),
			TEXT("Une barre aux c\u00e9r\u00e9ales p\u00e9rim\u00e9e depuis 1992. Endurance au maximum et r\u00e9cup\u00e9ration acc\u00e9l\u00e9r\u00e9e pendant 30 s."),
			TEXT("SM_EnergyBar"), TEXT("I_EnergyBar"), 4, EBREquipSlot::None, true, TEXT("Manger"));
		L[static_cast<int32>(EBRItem::VHSTape)] = Make(TEXT("Cassette VHS"), TEXT("CASSETTE VHS"),
			TEXT("Une cassette \u00e9tiquet\u00e9e \"THRESHOLD SYSTEMS - ENREGISTREMENT\". Il faut toutes les r\u00e9cup\u00e9rer pour stabiliser la sortie du niveau."),
			TEXT("SM_VHSTape"), TEXT("I_VHSTape"), 12, EBREquipSlot::None, false, TEXT(""));
		L[static_cast<int32>(EBRItem::Flashlight)] = Make(TEXT("Lampe torche"), TEXT("LAMPE"),
			TEXT("Une lampe torche robuste. \u00c0 \u00e9quiper dans la MAIN ou \u00e0 la CEINTURE. {Flashlight} pour l'allumer."),
			TEXT("SM_Flashlight"), TEXT("I_Flashlight"), 1, EBREquipSlot::Hand, false, TEXT("\u00c9quiper"));
		L[static_cast<int32>(EBRItem::Camcorder)] = Make(TEXT("Cam\u00e9scope"), TEXT("CAM\u00c9SCOPE"),
			TEXT("Un cam\u00e9scope VHS des ann\u00e9es 90, accroch\u00e9 \u00e0 votre sac. Tant que vous l'avez sur vous, il filme ce que vous regardez (t\u00e2ches d'enregistrement). Vision nocturne avec {NightVision}."),
			TEXT("SM_Camcorder"), TEXT("I_Camcorder"), 1, EBREquipSlot::None, false, TEXT(""));
		L[static_cast<int32>(EBRItem::Headlamp)] = Make(TEXT("Lampe frontale"), TEXT("FRONTALE"),
			TEXT("Une lampe frontale \u00e0 large faisceau, plus faible qu'une lampe torche. \u00c0 \u00e9quiper sur la T\u00caTE."),
			TEXT("SM_Headlamp"), TEXT("I_Headlamp"), 1, EBREquipSlot::Head, false, TEXT("\u00c9quiper"));
		L[static_cast<int32>(EBRItem::Vest)] = Make(TEXT("Gilet de protection"), TEXT("GILET"),
			TEXT("Un gilet renforc\u00e9 de Threshold Systems. R\u00e9duit les d\u00e9g\u00e2ts de 30 %. \u00c0 \u00e9quiper sur le TORSE."),
			TEXT("SM_Vest"), TEXT("I_Vest"), 1, EBREquipSlot::Chest, false, TEXT("\u00c9quiper"));
		L[static_cast<int32>(EBRItem::Note)] = Make(TEXT("Note"), TEXT("NOTE"), TEXT("Une note laiss\u00e9e par un vagabond."),
			TEXT("SM_Note"), TEXT("I_Note"), 1, EBREquipSlot::None, false, TEXT("Lire"));
		return L;
	}
}

namespace BRItems
{
	const FBRItemInfo& Get(EBRItem Item)
	{
		static const TArray<FBRItemInfo> Items = BuildItems();
		const int32 Index = FMath::Clamp(static_cast<int32>(Item), 0, Items.Num() - 1);
		return Items[Index];
	}

	bool CanEquipIn(EBRItem Item, EBREquipSlot Slot)
	{
		if (Item == EBRItem::None)
		{
			return true;
		}
		if (Item == EBRItem::Flashlight)
		{
			return Slot == EBREquipSlot::Hand || Slot == EBREquipSlot::Belt;
		}
		return Get(Item).Slot == Slot;
	}

	FString SlotName(EBREquipSlot Slot)
	{
		switch (Slot)
		{
		case EBREquipSlot::Head:
			return TEXT("T\u00caTE");
		case EBREquipSlot::Chest:
			return TEXT("TORSE");
		case EBREquipSlot::Hand:
			return TEXT("MAIN");
		case EBREquipSlot::Belt:
			return TEXT("CEINTURE");
		default:
			return FString();
		}
	}
}
