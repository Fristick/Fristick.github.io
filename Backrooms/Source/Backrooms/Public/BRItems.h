// Catalogue des objets d'inventaire.
#pragma once

#include "CoreMinimal.h"
#include "BRTypes.h"

struct FBRItemInfo
{
	FString Name;          // nom affiche (FR)
	FString Short;         // libelle court au-dessus de l'icone
	FString Description;
	FName Mesh;            // modele 3D au sol
	FName Icon;            // icone d'inventaire (RawAssets/Icons)
	int32 MaxStack = 1;
	EBREquipSlot Slot = EBREquipSlot::None;
	bool bConsumable = false;
	FString UseVerb;       // "Boire", "Utiliser"...
};

namespace BRItems
{
	const FBRItemInfo& Get(EBRItem Item);
	bool CanEquipIn(EBRItem Item, EBREquipSlot Slot);
	FString SlotName(EBREquipSlot Slot);
}
