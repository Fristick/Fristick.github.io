// v4.9 : modes d'affichage : plein ecran, plein ecran fenetre (sans bordures), fenetre.
// UGameUserSettings est la seule source du mode et de la resolution (GameUserSettings.ini) : BackroomsPlayer.ini ne les
// enregistre plus (une ancienne valeur y est reprise une fois). Un changement de mode ou de resolution est applique tout
// de suite mais n'est ecrit qu'une fois confirme : sans confirmation dans les 15 s, retour a la derniere configuration
// confirmee ; si le jeu s'arrete avant (plantage, fermeture), le lancement suivant y revient aussi.
// L'echelle de rendu (RENDU, r.ScreenPercentage) est un reglage a part : elle ne change ni la fenetre ni la sortie.
#pragma once

#include "CoreMinimal.h"

namespace BRDisplay
{
	/** Fenetre : 0 plein ecran, 1 plein ecran fenetre (sans bordures, taille de l'ecran), 2 fenetre */
	struct FMode
	{
		int32 Window = 1;
		/** Taille de la fenetre (fenetre) ou de la sortie (plein ecran) ; ecran entier en sans bordures */
		FIntPoint Resolution = FIntPoint::ZeroValue;
		bool operator==(const FMode& O) const { return Window == O.Window && Resolution == O.Resolution; }
	};

	/** Delai de confirmation (s) */
	constexpr float ConfirmSeconds = 15.f;

	/** Mode demande (UGameUserSettings) */
	BACKROOMS_API FMode Current();
	/** Mode reellement obtenu (fenetre du jeu) : peut differer du mode demande (plein ecran traduit en sans bordures par
	 *  le systeme de fenetres de macOS, par exemple) */
	BACKROOMS_API FMode Effective();
	/** Resolutions proposees pour ce mode sur l'ecran actif (plein ecran : modes de l'ecran ; sans bordures : l'ecran ;
	 *  fenetre : tailles qui tiennent sur le bureau, la derniere taille fenetree comprise) */
	BACKROOMS_API TArray<FIntPoint> ResolutionsFor(int32 Window);
	/** Le plein ecran exclusif existe sur cette plateforme (sinon le systeme en fait un plein ecran sans bordures) */
	BACKROOMS_API bool PlatformHasExclusiveFullscreen();

	/** Au lancement : reprise d'une ancienne valeur, retour a la configuration confirmee si le jeu s'est arrete avant une
	 *  confirmation, taille fenetree ramenee sur le bureau (ecran retire, autre ordinateur) */
	BACKROOMS_API void Startup();
	/** Applique un mode (Resolution nulle : celle qui convient au mode) et attend la confirmation ; false si impossible
	 *  (editeur : la fenetre est celle de l'editeur) */
	BACKROOMS_API bool Request(int32 Window, FIntPoint Resolution);
	BACKROOMS_API void Confirm();
	BACKROOMS_API void Revert();
	BACKROOMS_API bool IsPending();
	BACKROOMS_API float SecondsLeft();
	/** A appeler a chaque image : retour automatique a l'expiration (true ce tour-la), puis suivi des changements faits
	 *  hors du jeu (Alt+Entree, redimensionnement a la souris, mode impose par le systeme) */
	BACKROOMS_API bool Tick();
	/** Le jeu peut changer la fenetre (faux dans l'editeur) */
	BACKROOMS_API bool CanChange();
	/** Message a montrer une fois : configuration retablie au lancement */
	BACKROOMS_API bool TakeRestoredAtStartup();

	/** v4.10 : ecran qui contient la fenetre du jeu (centre de la fenetre ; ecran principal sans fenetre) : taille en pixels
	 *  physiques, zone utile (sans la barre des taches), DPI, rang et nombre d'ecrans. Avant : toujours l'ecran principal
	 *  (GetDesktopResolution), meme quand le jeu etait sur un autre ecran */
	struct FMonitor
	{
		FIntPoint Size = FIntPoint::ZeroValue;
		FIntPoint Origin = FIntPoint::ZeroValue;
		FIntPoint WorkSize = FIntPoint::ZeroValue;
		int32 DPI = 0;
		int32 Index = 0;
		int32 Count = 0;
		/** Identifiant de l'ecran pour Unreal (UGameUserSettings::SetDisplayProperties) */
		FString ID;
		bool bPrimary = true;
		/** Faux : informations des ecrans indisponibles (serveur, editeur sans Slate) ; valeurs de repli */
		bool bKnown = false;
	};
	BACKROOMS_API FMonitor ActiveMonitor();
	/** v4.10 (tests) : ecran qui contient ce point du bureau (pixels physiques) */
	BACKROOMS_API FMonitor MonitorAt(const FIntPoint& Point);
}
