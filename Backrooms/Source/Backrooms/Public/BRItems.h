// Catalogue des objets d'inventaire.
#pragma once

#include "CoreMinimal.h"
#include "BRTypes.h"

struct FBRItemInfo
{
	// v4.8 : textes localises
	FText Name;            // nom affiche
	FText Short;           // libelle court au-dessus de l'icone
	FText Description;
	FName Mesh;            // modele 3D au sol
	FName Icon;            // icone d'inventaire (RawAssets/Icons)
	int32 MaxStack = 1;
	EBREquipSlot Slot = EBREquipSlot::None;
	bool bConsumable = false;
	FText UseVerb;         // "Boire", "Utiliser"...
};

namespace BRItems
{
	const FBRItemInfo& Get(EBRItem Item);
	bool CanEquipIn(EBRItem Item, EBREquipSlot Slot);
	FText SlotName(EBREquipSlot Slot);
}
