#include "BRDisplay.h"
#include "Backrooms.h"
#include "BRConfig.h"
#include "BRTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/GenericWindowDefinition.h"
#include "HAL/PlatformTime.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/SWindow.h"

namespace
{
	/** Section de BackroomsPlayer.ini : derniere configuration confirmee, changement en attente, derniere taille fenetree */
	const TCHAR* const DisplaySection = TEXT("/Script/Backrooms.BRDisplay");
	/** Ancienne valeur (v3.4 a v4.8) : BackroomsPlayer.ini, section des reglages */
	const TCHAR* const LegacySection = TEXT("/Script/Backrooms.BRSettings");

	struct FDisplayState
	{
		bool bPending = false;
		double Deadline = 0.0;
		BRDisplay::FMode Confirmed;
		FIntPoint LastWindowed = FIntPoint::ZeroValue;
		double NextSync = 0.0;
		FIntPoint ResizeSeen = FIntPoint::ZeroValue;
		double ResizeSince = 0.0;
		bool bRestoredAtStartup = false;
	};

	FDisplayState& State()
	{
		static FDisplayState S;
		return S;
	}

	UGameUserSettings* Settings()
	{
		return GEngine ? GEngine->GetGameUserSettings() : nullptr;
	}

	EWindowMode::Type ToEngine(int32 Window)
	{
		return Window == 0 ? EWindowMode::Fullscreen : (Window == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
	}

	int32 FromEngine(EWindowMode::Type Mode)
	{
		return Mode == EWindowMode::Fullscreen ? 0 : (Mode == EWindowMode::WindowedFullscreen ? 1 : 2);
	}

	FIntPoint Desktop()
	{
		const UGameUserSettings* G = Settings();
		const FIntPoint D = G ? G->GetDesktopResolution() : FIntPoint::ZeroValue;
		return (D.X > 0 && D.Y > 0) ? D : FIntPoint(1920, 1080);
	}

	bool FitsDesktop(const FIntPoint& Size)
	{
		const FIntPoint D = Desktop();
		return Size.X >= 640 && Size.Y >= 360 && Size.X <= D.X && Size.Y <= D.Y;
	}

	/** Taille fenetree utilisable : celle demandee si elle tient sur le bureau, sinon la plus grande des tailles usuelles
	 *  qui laisse de la place a la barre des taches et aux bordures (90 % du bureau) */
	FIntPoint FitWindowed(const FIntPoint& Want)
	{
		const FIntPoint D = Desktop();
		if (Want.X >= 640 && Want.Y >= 360 && Want.X <= D.X * 0.95f && Want.Y <= D.Y * 0.95f)
		{
			return Want;
		}
		static const FIntPoint Usual[] = { { 3200, 1800 }, { 2560, 1440 }, { 1920, 1080 }, { 1600, 900 }, { 1366, 768 }, { 1280, 720 }, { 1024, 576 } };
		for (const FIntPoint& U : Usual)
		{
			if (U.X <= D.X * 0.9f && U.Y <= D.Y * 0.9f)
			{
				return U;
			}
		}
		return FIntPoint(FMath::Max(640, FMath::RoundToInt(D.X * 0.8f)), FMath::Max(360, FMath::RoundToInt(D.Y * 0.8f)));
	}

	/** Resolution qui convient au mode */
	FIntPoint ResolutionFor(int32 Window, const FIntPoint& Want)
	{
		if (Window == 1)
		{
			return Desktop(); // sans bordures : toujours l'ecran entier
		}
		if (Window == 2)
		{
			const FIntPoint Last = State().LastWindowed;
			return FitWindowed(Want.X > 0 ? Want : (Last.X > 0 ? Last : FIntPoint(1600, 900)));
		}
		return Want.X > 0 ? Want : Desktop();
	}

	void SaveConfirmed(const BRDisplay::FMode& M)
	{
		FConfigFile& Cfg = BRConfig::Get();
		Cfg.SetInt64(DisplaySection, TEXT("ConfirmedWindow"), M.Window);
		Cfg.SetInt64(DisplaySection, TEXT("ConfirmedX"), M.Resolution.X);
		Cfg.SetInt64(DisplaySection, TEXT("ConfirmedY"), M.Resolution.Y);
		const FIntPoint Last = State().LastWindowed;
		Cfg.SetInt64(DisplaySection, TEXT("LastWindowedX"), Last.X);
		Cfg.SetInt64(DisplaySection, TEXT("LastWindowedY"), Last.Y);
		BRConfig::Save();
	}

	void SetPendingMarker(bool bPending)
	{
		BRConfig::Get().SetBool(DisplaySection, TEXT("Pending"), bPending);
		BRConfig::Save();
	}

	/** Applique le mode a la fenetre sans l'ecrire dans GameUserSettings.ini */
	void ApplyToEngine(const BRDisplay::FMode& M)
	{
		UGameUserSettings* G = Settings();
		if (!G)
		{
			return;
		}
		G->SetFullscreenMode(ToEngine(M.Window));
		G->SetScreenResolution(M.Resolution);
		G->ApplyResolutionSettings(false);
		FBRSettings::Get().WindowMode = M.Window;
		// La fenetre change a la fin de l'image : le suivi des changements exterieurs attend un peu
		State().NextSync = FPlatformTime::Seconds() + 2.0;
	}

	/** Le mode en vigueur devient la configuration confirmee (GameUserSettings.ini et BackroomsPlayer.ini) */
	void Commit()
	{
		UGameUserSettings* G = Settings();
		if (!G)
		{
			return;
		}
		G->ConfirmVideoMode();
		G->SaveSettings();
		FDisplayState& St = State();
		St.Confirmed = BRDisplay::Current();
		if (St.Confirmed.Window == 2)
		{
			St.LastWindowed = St.Confirmed.Resolution;
		}
		FBRSettings::Get().WindowMode = St.Confirmed.Window;
		SaveConfirmed(St.Confirmed);
	}
}

namespace BRDisplay
{
	bool CanChange()
	{
		// Dans l'editeur (PIE), la fenetre est celle de l'editeur : on n'y touche pas
		return !GIsEditor && Settings() != nullptr;
	}

	bool PlatformHasExclusiveFullscreen()
	{
#if PLATFORM_WINDOWS
		return true;
#else
		// macOS : le plein ecran du jeu est une fenetre sans bordures a la taille de l'ecran. Linux : selon le systeme de
		// fenetres (X11 ou Wayland) ; le mode reellement obtenu est lu sur la fenetre et affiche (Effective)
		return false;
#endif
	}

	FMode Current()
	{
		FMode M;
		if (const UGameUserSettings* G = Settings())
		{
			M.Window = FromEngine(G->GetFullscreenMode());
			M.Resolution = G->GetScreenResolution();
		}
		return M;
	}

	FMode Effective()
	{
		FMode M = Current();
		if (GEngine && GEngine->GameViewport)
		{
			const TSharedPtr<SWindow> Win = GEngine->GameViewport->GetWindow();
			if (Win.IsValid())
			{
				M.Window = FromEngine(Win->GetWindowMode());
				const FVector2D Client = Win->GetClientSizeInScreen();
				M.Resolution = FIntPoint(FMath::RoundToInt(Client.X), FMath::RoundToInt(Client.Y));
			}
		}
		return M;
	}

	TArray<FIntPoint> ResolutionsFor(int32 Window)
	{
		TArray<FIntPoint> List;
		if (Window == 1)
		{
			List.Add(Desktop());
			return List;
		}
		TArray<FIntPoint> Found;
		if (Window == 0)
		{
			UKismetSystemLibrary::GetSupportedFullscreenResolutions(Found);
		}
		else
		{
			UKismetSystemLibrary::GetConvenientWindowedResolutions(Found);
			if (State().LastWindowed.X > 0)
			{
				Found.Add(State().LastWindowed);
			}
		}
		for (const FIntPoint& R : Found)
		{
			const bool bOk = Window == 0 ? (R.X >= 640 && R.Y >= 360) : FitsDesktop(R);
			if (bOk && !List.Contains(R))
			{
				List.Add(R);
			}
		}
		if (List.Num() == 0)
		{
			List.Add(Window == 0 ? Desktop() : FitWindowed(FIntPoint(1600, 900)));
		}
		List.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y || (A.X * A.Y == B.X * B.Y && A.X < B.X); });
		return List;
	}

	void Startup()
	{
		FDisplayState& St = State();
		const FConfigFile& Cfg = BRConfig::Get();
		int32 X = 0;
		int32 Y = 0;
		Cfg.GetInt(DisplaySection, TEXT("LastWindowedX"), X);
		Cfg.GetInt(DisplaySection, TEXT("LastWindowedY"), Y);
		St.LastWindowed = FIntPoint(X, Y);
		if (!CanChange())
		{
			return;
		}
		UGameUserSettings* G = Settings();
		int32 Window = -1;
		const bool bHasConfirmed = Cfg.GetInt(DisplaySection, TEXT("ConfirmedWindow"), Window);
		if (!bHasConfirmed)
		{
			// Migration : le mode v3.4-v4.8 de BackroomsPlayer.ini est repris une fois, puis UGameUserSettings fait foi
			int32 Legacy = -1;
			if (Cfg.GetInt(LegacySection, TEXT("WindowMode"), Legacy) && Legacy >= 0 && Legacy <= 2 && FromEngine(G->GetFullscreenMode()) != Legacy)
			{
				ApplyToEngine({ Legacy, ResolutionFor(Legacy, G->GetScreenResolution()) });
				UE_LOG(LogBackrooms, Log, TEXT("Affichage : mode %d repris de BackroomsPlayer.ini (v4.8)"), Legacy);
			}
			Commit();
		}
		else
		{
			Cfg.GetInt(DisplaySection, TEXT("ConfirmedX"), X);
			Cfg.GetInt(DisplaySection, TEXT("ConfirmedY"), Y);
			St.Confirmed.Window = FMath::Clamp(Window, 0, 2);
			St.Confirmed.Resolution = FIntPoint(X, Y);
			bool bPendingAtExit = false;
			Cfg.GetBool(DisplaySection, TEXT("Pending"), bPendingAtExit);
			if (bPendingAtExit)
			{
				// Le jeu s'est arrete pendant une confirmation : retour a la derniere configuration confirmee
				ApplyToEngine({ St.Confirmed.Window, ResolutionFor(St.Confirmed.Window, St.Confirmed.Resolution) });
				Commit();
				St.bRestoredAtStartup = true;
				UE_LOG(LogBackrooms, Warning, TEXT("Affichage : arret avant confirmation, configuration confirmee retablie (%d, %dx%d)"), St.Confirmed.Window,
					St.Confirmed.Resolution.X, St.Confirmed.Resolution.Y);
			}
		}
		SetPendingMarker(false);
		// Ecran retire, autre ordinateur, fenetre plus grande que le bureau : taille ramenee a l'ecran actif
		const FMode Now = Current();
		const FIntPoint Fixed = ResolutionFor(Now.Window, Now.Resolution);
		if (Fixed != Now.Resolution)
		{
			ApplyToEngine({ Now.Window, Fixed });
			Commit();
			UE_LOG(LogBackrooms, Log, TEXT("Affichage : %dx%d ramene a %dx%d (ecran actif %dx%d)"), Now.Resolution.X, Now.Resolution.Y, Fixed.X, Fixed.Y, Desktop().X,
				Desktop().Y);
		}
		FBRSettings::Get().WindowMode = Current().Window;
	}

	bool Request(int32 Window, FIntPoint Resolution)
	{
		if (!CanChange())
		{
			return false;
		}
		FDisplayState& St = State();
		Window = FMath::Clamp(Window, 0, 2);
		if (!St.bPending)
		{
			// La configuration en vigueur est celle a laquelle on revient
			St.Confirmed = Current();
			SaveConfirmed(St.Confirmed);
		}
		const FMode Before = Current();
		if (Before.Window == 2 && FitsDesktop(Before.Resolution))
		{
			St.LastWindowed = Before.Resolution;
		}
		const FMode Want{ Window, ResolutionFor(Window, Resolution) };
		ApplyToEngine(Want);
		St.bPending = true;
		St.Deadline = FPlatformTime::Seconds() + ConfirmSeconds;
		SetPendingMarker(true);
		UE_LOG(LogBackrooms, Log, TEXT("Affichage : essai du mode %d en %dx%d (retour au mode %d en %dx%d sans confirmation)"), Want.Window, Want.Resolution.X,
			Want.Resolution.Y, St.Confirmed.Window, St.Confirmed.Resolution.X, St.Confirmed.Resolution.Y);
		return true;
	}

	void Confirm()
	{
		FDisplayState& St = State();
		if (!St.bPending)
		{
			return;
		}
		St.bPending = false;
		Commit();
		SetPendingMarker(false);
	}

	void Revert()
	{
		FDisplayState& St = State();
		if (!St.bPending)
		{
			return;
		}
		St.bPending = false;
		ApplyToEngine({ St.Confirmed.Window, ResolutionFor(St.Confirmed.Window, St.Confirmed.Resolution) });
		Commit();
		SetPendingMarker(false);
	}

	bool IsPending()
	{
		return State().bPending;
	}

	float SecondsLeft()
	{
		return State().bPending ? FMath::Max(0.f, static_cast<float>(State().Deadline - FPlatformTime::Seconds())) : 0.f;
	}

	bool TakeRestoredAtStartup()
	{
		const bool b = State().bRestoredAtStartup;
		State().bRestoredAtStartup = false;
		return b;
	}

	bool Tick()
	{
		if (!CanChange())
		{
			return false;
		}
		FDisplayState& St = State();
		const double Now = FPlatformTime::Seconds();
		if (St.bPending)
		{
			if (Now >= St.Deadline)
			{
				Revert();
				UE_LOG(LogBackrooms, Log, TEXT("Affichage : pas de confirmation en %.0f s, retour a la configuration precedente"), ConfirmSeconds);
				return true;
			}
			return false;
		}
		if (Now < St.NextSync)
		{
			return false;
		}
		St.NextSync = Now + 0.5;
		// Changements faits hors du jeu : Alt+Entree, redimensionnement a la souris, mode impose par le systeme
		const FMode Req = Current();
		const FMode Eff = Effective();
		if (Eff.Window != Req.Window)
		{
			// Plein ecran demande, sans bordures obtenu sur un systeme sans plein ecran exclusif : ce n'est pas un changement
			// du joueur ; le mode reel est affiche dans les parametres
			if (!(Req.Window == 0 && Eff.Window == 1 && !PlatformHasExclusiveFullscreen()))
			{
				if (UGameUserSettings* G = Settings())
				{
					G->SetFullscreenMode(ToEngine(Eff.Window));
					G->SetScreenResolution(Eff.Window == 2 ? Eff.Resolution : (Eff.Window == 1 ? Desktop() : Req.Resolution));
				}
				Commit();
				UE_LOG(LogBackrooms, Log, TEXT("Affichage : mode change hors du jeu (%d -> %d), enregistre"), Req.Window, Eff.Window);
			}
			return false;
		}
		if (Eff.Window == 2 && Eff.Resolution != Req.Resolution && Eff.Resolution.X >= 320 && Eff.Resolution.Y >= 200)
		{
			// Fenetre redimensionnee a la souris : enregistree quand sa taille ne change plus depuis une seconde
			if (Eff.Resolution != St.ResizeSeen)
			{
				St.ResizeSeen = Eff.Resolution;
				St.ResizeSince = Now;
			}
			else if (Now - St.ResizeSince > 1.0)
			{
				if (UGameUserSettings* G = Settings())
				{
					G->SetScreenResolution(Eff.Resolution);
				}
				Commit();
			}
		}
		return false;
	}
}
