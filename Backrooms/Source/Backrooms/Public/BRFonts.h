// v4.8 : polices de l'interface pour les 22 langues.
// La police du moteur (Roboto) couvre le latin, le grec et le cyrillique. Les autres ecritures viennent de polices Noto
// (licence SIL OFL 1.1, Content/Fonts/OFL.txt) ajoutees comme sous-polices d'une police composite creee au lancement :
// arabe et persan, chinois simplifie, chinois traditionnel, japonais, coreen. Les ideogrammes communs au chinois, au japonais
// et au coreen prennent la forme de la langue courante (sous-polices propres a une culture) ; hors de ces langues, le chinois
// simplifie sert de defaut, et les noms natifs des langues (page Langue) s'affichent toujours.
// Les fichiers sont lus depuis Content/Fonts (DefaultGame.ini les met dans le paquet : DirectoriesToAlwaysStageAsUFS).
#pragma once

#include "CoreMinimal.h"

class UFont;

namespace BRFonts
{
	/** Police composite de l'interface : Base (police du moteur) + sous-polices des autres ecritures (une par Base, gardee) */
	BACKROOMS_API UFont* WithScripts(UFont* Base);
	/** Fichiers de police attendus dans Content/Fonts et absents (paquet incomplet : texte en carres a la place des lettres) */
	BACKROOMS_API TArray<FString> MissingFiles();
	/** Fichiers attendus (nom dans Content/Fonts) */
	BACKROOMS_API const TArray<FString>& ExpectedFiles();
}
