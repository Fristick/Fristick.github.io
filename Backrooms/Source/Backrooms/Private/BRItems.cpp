#include "BRItems.h"
#include "BRTxnLogic.h"

namespace
{
	FBRItemInfo Make(const FText& Name, const FText& Short, const FText& Desc, const TCHAR* Mesh, const TCHAR* Icon, int32 Stack,
		EBREquipSlot Slot, bool bConsumable, const FText& Verb)
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
		L[static_cast<int32>(EBRItem::None)] = Make(NSLOCTEXT("BR", "Item.Vide", "Vide"), FText::GetEmpty(), FText::GetEmpty(), TEXT(""), TEXT(""), 0, EBREquipSlot::None, false, FText::GetEmpty());
		L[static_cast<int32>(EBRItem::AlmondWater)] = Make(NSLOCTEXT("BR", "Item.EauAmande", "Eau d'amande"), NSLOCTEXT("BR", "Item.EauAmande2", "EAU D'AMANDE"),
			NSLOCTEXT("BR", "Item.BouteilleEauAmandeLegerementSucree", "Une bouteille d'eau d'amande, l\u00e9g\u00e8rement sucr\u00e9e. Apaise l'esprit : +40 sant\u00e9 mentale, +10 sant\u00e9."),
			TEXT("SM_AlmondWater"), TEXT("I_AlmondWater"), 4, EBREquipSlot::None, true, NSLOCTEXT("BR", "Item.Boire", "Boire"));
		L[static_cast<int32>(EBRItem::Bandage)] = Make(NSLOCTEXT("BR", "Item.Bandages", "Bandages"), NSLOCTEXT("BR", "Item.Bandages2", "BANDAGES"),
			NSLOCTEXT("BR", "Item.RouleauGazeSterileSoigneBlessures", "Un rouleau de gaze st\u00e9rile. Soigne les blessures : +35 sant\u00e9."),
			TEXT("SM_Bandage"), TEXT("I_Bandage"), 4, EBREquipSlot::None, true, NSLOCTEXT("BR", "Item.Utiliser", "Utiliser"));
		L[static_cast<int32>(EBRItem::Battery)] = Make(NSLOCTEXT("BR", "Item.Piles", "Piles"), NSLOCTEXT("BR", "Item.Piles2", "PILES"),
			NSLOCTEXT("BR", "Item.DeuxGrossesPilesRechargentLampe", "Deux grosses piles. Rechargent la lampe torche, la lampe frontale et le cam\u00e9scope."),
			TEXT("SM_Battery"), TEXT("I_Battery"), 6, EBREquipSlot::None, true, NSLOCTEXT("BR", "Item.Recharger", "Recharger"));
		L[static_cast<int32>(EBRItem::EnergyBar)] = Make(NSLOCTEXT("BR", "Item.BarreEnergetique", "Barre \u00e9nerg\u00e9tique"), NSLOCTEXT("BR", "Item.Barre", "BARRE"),
			NSLOCTEXT("BR", "Item.BarreCerealesPerimeeDepuis1992", "Une barre aux c\u00e9r\u00e9ales p\u00e9rim\u00e9e depuis 1992. Endurance au maximum et r\u00e9cup\u00e9ration acc\u00e9l\u00e9r\u00e9e pendant 30 s."),
			TEXT("SM_EnergyBar"), TEXT("I_EnergyBar"), 4, EBREquipSlot::None, true, NSLOCTEXT("BR", "Item.Manger", "Manger"));
		L[static_cast<int32>(EBRItem::VHSTape)] = Make(NSLOCTEXT("BR", "Item.CassetteVhs", "Cassette VHS"), NSLOCTEXT("BR", "Item.CassetteVhs2", "CASSETTE VHS"),
			NSLOCTEXT("BR", "Item.CassetteEtiqueteeThresholdSystemsEnregis", "Une cassette \u00e9tiquet\u00e9e \"THRESHOLD SYSTEMS - ENREGISTREMENT\". Il faut toutes les r\u00e9cup\u00e9rer pour stabiliser la sortie du niveau."),
			TEXT("SM_VHSTape"), TEXT("I_VHSTape"), 12, EBREquipSlot::None, false, FText::GetEmpty());
		L[static_cast<int32>(EBRItem::Flashlight)] = Make(NSLOCTEXT("BR", "Item.LampeTorche", "Lampe torche"), NSLOCTEXT("BR", "Item.Lampe", "LAMPE"),
			NSLOCTEXT("BR", "Item.LampeTorcheRobusteEquiperMain", "Une lampe torche robuste. \u00c0 \u00e9quiper dans la MAIN ou \u00e0 la CEINTURE. {Flashlight} pour l'allumer."),
			TEXT("SM_Flashlight"), TEXT("I_Flashlight"), 1, EBREquipSlot::Hand, false, NSLOCTEXT("BR", "Item.Equiper", "\u00c9quiper"));
		L[static_cast<int32>(EBRItem::Camcorder)] = Make(NSLOCTEXT("BR", "Item.Camescope", "Cam\u00e9scope"), NSLOCTEXT("BR", "Item.Camescope2", "CAM\u00c9SCOPE"),
			NSLOCTEXT("BR", "Item.CamescopeVhsAnnees90Accroche", "Un cam\u00e9scope VHS des ann\u00e9es 90, accroch\u00e9 \u00e0 votre sac. Tant que vous l'avez sur vous, il filme ce que vous regardez (t\u00e2ches d'enregistrement). Vision nocturne avec {NightVision}."),
			TEXT("SM_Camcorder"), TEXT("I_Camcorder"), 1, EBREquipSlot::None, false, FText::GetEmpty());
		L[static_cast<int32>(EBRItem::Headlamp)] = Make(NSLOCTEXT("BR", "Item.LampeFrontale", "Lampe frontale"), NSLOCTEXT("BR", "Item.Frontale", "FRONTALE"),
			NSLOCTEXT("BR", "Item.LampeFrontaleLargeFaisceauFaible", "Une lampe frontale \u00e0 large faisceau, plus faible qu'une lampe torche. \u00c0 \u00e9quiper sur la T\u00caTE."),
			TEXT("SM_Headlamp"), TEXT("I_Headlamp"), 1, EBREquipSlot::Head, false, NSLOCTEXT("BR", "Item.Equiper", "\u00c9quiper"));
		L[static_cast<int32>(EBRItem::Vest)] = Make(NSLOCTEXT("BR", "Item.GiletProtection", "Gilet de protection"), NSLOCTEXT("BR", "Item.Gilet", "GILET"),
			NSLOCTEXT("BR", "Item.GiletRenforceThresholdSystemsReduit", "Un gilet renforc\u00e9 de Threshold Systems. R\u00e9duit les d\u00e9g\u00e2ts de 30 %. \u00c0 \u00e9quiper sur le TORSE."),
			TEXT("SM_Vest"), TEXT("I_Vest"), 1, EBREquipSlot::Chest, false, NSLOCTEXT("BR", "Item.Equiper", "\u00c9quiper"));
		L[static_cast<int32>(EBRItem::Note)] = Make(NSLOCTEXT("BR", "Item.Note", "Note"), NSLOCTEXT("BR", "Item.Note2", "NOTE"), NSLOCTEXT("BR", "Item.NoteLaisseeVagabond", "Une note laiss\u00e9e par un vagabond."),
			TEXT("SM_Note"), TEXT("I_Note"), 1, EBREquipSlot::None, false, NSLOCTEXT("BR", "Item.Lire", "Lire"));
		// v4.12 : piles, emplacements et consommables viennent des regles partagees (BRTxn::Rule) : l'inventaire, la
		// verification de place et le banc hors moteur lisent la meme table
		for (int32 I = 0; I < L.Num(); ++I)
		{
			const BRTxn::FRule& R = BRTxn::Rule(static_cast<uint8>(I));
			L[I].MaxStack = R.MaxStack;
			L[I].bConsumable = R.bConsumable;
			const int32 Preferred = BRTxn::PreferredSlot(static_cast<uint8>(I));
			L[I].Slot = Preferred >= 0 ? static_cast<EBREquipSlot>(Preferred) : EBREquipSlot::None;
		}
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
		// v4.12 : regle partagee (lampe torche : main ou ceinture ; frontale : tete ; gilet : torse)
		return BRTxn::CanEquipIn(static_cast<uint8>(Item), static_cast<int32>(Slot));
	}

	FText SlotName(EBREquipSlot Slot)
	{
		switch (Slot)
		{
		case EBREquipSlot::Head:
			return NSLOCTEXT("BR", "Item.Tete", "T\u00caTE");
		case EBREquipSlot::Chest:
			return NSLOCTEXT("BR", "Item.Torse", "TORSE");
		case EBREquipSlot::Hand:
			return NSLOCTEXT("BR", "Item.Main", "MAIN");
		case EBREquipSlot::Belt:
			return NSLOCTEXT("BR", "Item.Ceinture", "CEINTURE");
		default:
			return FText::GetEmpty();
		}
	}
}
