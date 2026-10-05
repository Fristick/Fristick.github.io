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
	const TArray<FBRNote>& CommonNotes();
	/** v4.8 : note d'apres son identifiant (tous niveaux) ; nullptr si inconnue */
	const FBRNote* FindNote(FName Id);
	/** v4.8 : identifiant d'une note d'apres son texte francais (sauvegardes v4.1-v4.7, qui gardaient le texte) */
	FName NoteIdFromLegacyText(const FString& FrenchText);
	/** v4.8 : texte d'une entree du journal : note connue (dans la langue courante), sinon le texte enregistre tel quel */
	FText NoteText(const FString& JournalEntry);
}
