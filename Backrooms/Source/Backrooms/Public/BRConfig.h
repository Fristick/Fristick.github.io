// Reglages du joueur (parametres, touches, derniere adresse IP) dans un fichier dedie :
// Saved/Config/<plateforme>/BackroomsPlayer.ini.
// Ils etaient dans GameUserSettings.ini, que le moteur efface en entier des que la version de ce fichier ne lui
// convient pas (nouvelle version d'Unreal, reglages enregistres depuis l'editeur puis jeu lance en "Standalone"...).
#pragma once

#include "CoreMinimal.h"

class FConfigFile;

namespace BRConfig
{
	/** Le fichier, charge a la premiere utilisation (reprend une fois les anciens reglages de GameUserSettings.ini) */
	BACKROOMS_API FConfigFile& Get();

	/** Ecrit le fichier sur le disque */
	BACKROOMS_API void Save();

	BACKROOMS_API const FString& Path();
}
