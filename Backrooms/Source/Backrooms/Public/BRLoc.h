// v4.8 : langues du jeu.
// Tous les textes visibles sont des FText ecrits NSLOCTEXT("BR", "<Cle stable>", "<Texte francais source>") a l'endroit ou
// ils servent : le rassemblement d'Unreal (GatherText) les retrouve, Content/Localization/Game/<culture>/Game.po porte les
// traductions et Game.locres (compile) les fournit au jeu. Le francais est la langue source.
// BR_CACHED garde le FText du site d'appel : la recherche dans la table n'est faite qu'une fois, et le texte suit
// quand meme le changement de langue en jeu (FText se reconstruit quand la revision des textes change).
#pragma once

#include "CoreMinimal.h"

/** FText mis en cache a ce site d'appel (constante de localisation : NSLOCTEXT, INVTEXT) */
#define BR_CACHED(...) ([]() -> const FText& { static const FText BRCachedText = __VA_ARGS__; return BRCachedText; }())
/** Meme chose, en FString (dessin du HUD) */
#define BR_STR(...) BR_CACHED(__VA_ARGS__).ToString()

namespace BRLoc
{
	/** Une langue proposee */
	struct FLanguage
	{
		/** Code de culture Unreal (dossier Content/Localization/Game/<Code>) */
		const TCHAR* Code;
		/** Nom de la langue dans cette langue (affiche tel quel, jamais traduit) */
		const TCHAR* NativeName;
		/** Ecriture de droite a gauche (arabe, persan) */
		bool bRightToLeft;
		/** Ecriture sans espaces entre les mots (chinois, japonais) : coupure des lignes entre les caracteres */
		bool bNoSpaces;
	};

	/** Les 22 langues, dans l'ordre du menu (francais, la langue source, en premier) */
	BACKROOMS_API const TArray<FLanguage>& Languages();
	/** Indice de la langue courante dans Languages() (INDEX_NONE si la culture courante n'en fait pas partie) */
	BACKROOMS_API int32 CurrentIndex();
	BACKROOMS_API const FLanguage& Current();
	/** Langue du systeme si le jeu la propose (fr-CA -> fr, pt-PT, es-MX -> es-419, zh-TW -> zh-Hant...), sinon l'anglais */
	BACKROOMS_API FString DetectSystemLanguage();
	/** Change la langue du jeu tout de suite (textes, formats des nombres et des dates) ; false si refusee */
	BACKROOMS_API bool SetLanguage(const FString& Code);
	/** Langue choisie par le joueur et enregistree ("" : celle du systeme) */
	BACKROOMS_API FString& Preference();
	/** Ecriture de droite a gauche pour la langue courante */
	BACKROOMS_API bool IsRightToLeft();
	/** Langue sans espaces entre les mots (coupure des lignes entre caracteres) */
	BACKROOMS_API bool UsesNoSpaces();
	/** Cles sans traduction dans la langue courante (le texte francais est affiche) : recensees au changement de langue
	 *  (journal LogBackrooms, mode developpeur, tests) ; jamais de texte vide */
	BACKROOMS_API int32 CountMissing(TArray<FString>* OutKeys = nullptr);
	/** Le texte d'une cle (namespace BR) dans la langue courante, si elle est traduite ; nullptr sinon */
	BACKROOMS_API bool HasTranslation(const TCHAR* Key);
}
