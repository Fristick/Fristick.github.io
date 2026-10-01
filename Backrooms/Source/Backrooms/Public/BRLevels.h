// Catalogue des niveaux (inspires de https://backrooms-wiki.wikidot.com/normal-levels-i)
#pragma once

#include "CoreMinimal.h"
#include "BRTypes.h"

namespace BRLevels
{
	/** Tous les niveaux jouables, dans l'ordre du menu */
	const TArray<FBRLevelDef>& All();

	/** Niveau par numero (retourne le Niveau 0 s'il n'existe pas) */
	const FBRLevelDef& Get(int32 Number);

	bool Exists(int32 Number);

	/** Index dans All() (0 si introuvable) */
	int32 IndexOf(int32 Number);

	/** Notes generiques trouvables partout */
	const TArray<FString>& CommonNotes();
}
