// v4.17 (correction, QA visuelle § 1.4, prompt § 4.6) : carnet vraiment long (-BRAutoTest -BRAutoTestV417Carnet).
// Le cas « carnet long » du banc v4.16 visuel (Level 1) ne debordait jamais : meme avec tous les indices lus, en allemand et
// a la taille d'interface 1,25, le carnet du Level 1 tient dans sa zone a 1280 x 720. Ce banc ouvre le carnet de plusieurs
// niveaux a mission (graine 4242), tous leurs indices et observations lus (etat de l'essai seulement, refait par le chargement
// suivant), aide et rappel de l'etape en cours affiches, en allemand, a la taille 1,25, dans la fenetre du lancement
// (1280 x 720 conseille). Pour chaque niveau : texte cache ou non, place restante dans chaque colonne, puis un appui de la
// croix bas (ABRHUD::PadScrollLines(1)) et ce qui a vraiment defile ; captures avant et apres l'appui.
// Un niveau dont le carnet tient dans sa zone est note tel quel ; le cas n'est NON VERIFIE que si aucun ne deborde.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRHUD.h"
#include "BRLoc.h"
#include "BRMissionLogic.h"
#include "BRPlayerController.h"
#include "BRWorld.h"

namespace BRTestV417Carnet
{
	FString SavedLang;
	int32 Phase = 0;
	float PressLeft = 0.f;
	float PressRight = 0.f;
	float RoomLeft = 0.f;
	int32 Overflowing = 0;
	int32 Scrolled = 0;
}

void ABRAutoTest::AddV417CarnetSteps()
{
	auto HudOf = [this]() -> ABRHUD*
	{
		ABRPlayerController* PC = GetPC();
		return PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
	};
	Add(TEXT("v4.17 carnet long : debut"), 0.f, [this]()
	{
		if (!GetPC())
		{
			return false;
		}
		if (!bSettingsSaved)
		{
			SavedSettings = FBRSettings::Get();
			bSettingsSaved = true;
		}
		BRTestV417Carnet::SavedLang = FString(BRLoc::Current().Code);
		BRTestV417Carnet::Overflowing = 0;
		BRTestV417Carnet::Scrolled = 0;
		FBRSettings::Get().UiScale = 1.25f;
		FLevelReport R;
		R.Title = TEXT("v4.17 carnet long (tous les indices lus, allemand, taille 1,25)");
		R.Scene = TEXT("v417carnet");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		FIntPoint View(0, 0);
		GetPC()->GetViewportSize(View.X, View.Y);
		Note(FString::Printf(TEXT("carnet long : fenetre %d x %d, taille d'interface 1,25, allemand"), View.X, View.Y));
		return true;
	});
	static const int32 Levels[] = { 1, 2, 3, 4, 6, 9, 11 };
	for (const int32 Level : Levels)
	{
		AddSeedLoad(Level, 4242u, FString::Printf(TEXT("v4.17 carnet long : Niveau %d, graine 4242"), Level));
		Add(FString::Printf(TEXT("v4.17 carnet long : Niveau %d : indices lus, carnet ouvert"), Level), 1.2f, [this, HudOf]()
		{
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			ABRHUD* H = HudOf();
			if (!W || !PC || !H || W->IsTransitioning())
			{
				return false;
			}
			// Essai seulement : l'etat de la mission est ecrit ici sans passer par les appareils (le chargement suivant le refait)
			const BRMission::FPlan& MPlan = W->GetMissionPlan();
			BRMission::FState& State = const_cast<BRMission::FState&>(W->GetMissionState());
			for (int32 D = 0; D < MPlan.NumDevices; ++D)
			{
				if (MPlan.Devices[D].Kind == BRMission::EKind::Clue)
				{
					State.Dev[D] = 1;
				}
				else if (MPlan.Devices[D].Kind == BRMission::EKind::Observe)
				{
					State.Dev[D] = static_cast<uint8>(MPlan.Devices[D].Positions);
				}
			}
			BRLoc::SetLanguage(TEXT("de"));
			PC->SetInventoryOpen(true, 4);
			H->Tab = ABRHUD::ETab::Notebook;
			H->Messages.Reset();
			H->NotebookLeftScroll = 0.f;
			H->NotebookScroll = 0.f;
			if (H->NotebookHintStep >= 0)
			{
				H->NotebookHint.FindOrAdd(H->NotebookHintStep) = 2;
			}
			H->PadScrollFocus(0);
			PC->SetMouseLocation(5, 5);
			BRTestV417Carnet::Phase = 0;
			return true;
		});
		Add(FString::Printf(TEXT("v4.17 carnet long : Niveau %d : mesure et appui"), Level), 1.f, [this, HudOf, Level]()
		{
			ABRHUD* H = HudOf();
			if (!H)
			{
				Skip(FString::Printf(TEXT("carnet long, Niveau %d : HUD absent"), Level));
				return true;
			}
			if (BRTestV417Carnet::Phase == 0)
			{
				// Re-ouvert si le niveau a regle l'aide apres le premier dessin (l'etape en cours est connue a l'image d'avant)
				if (H->NotebookHintStep >= 0)
				{
					H->NotebookHint.FindOrAdd(H->NotebookHintStep) = 2;
				}
				BRTestV417Carnet::PressLeft = H->NotebookLeftScroll;
				BRTestV417Carnet::PressRight = H->NotebookScroll;
				BRTestV417Carnet::RoomLeft = H->LastNotebookLeftRoom;
				Shot(FString::Printf(TEXT("v417_carnet_long_niveau%d_avant_appui"), Level));
				if (!H->HadScrollRoom())
				{
					Note(FString::Printf(TEXT("carnet long, Niveau %d : tout tient dans les zones (aucun texte cache), observations %d"), Level, H->LastNotebookObservations));
					return true;
				}
				++BRTestV417Carnet::Overflowing;
				H->PadScrollLines(1);
				BRTestV417Carnet::Phase = 1;
				return false;
			}
			if (StepTime < 1.4f)
			{
				return false;
			}
			BRTestV417Carnet::Phase = 0;
			const float GotL = H->NotebookLeftScroll - BRTestV417Carnet::PressLeft;
			const float GotR = H->NotebookScroll - BRTestV417Carnet::PressRight;
			const bool bLeft = BRTestV417Carnet::RoomLeft >= 1.f;
			const float WantL = FMath::Min(ABRHUD::PadScrollLine, BRTestV417Carnet::RoomLeft);
			const bool bOk = bLeft ? FMath::Abs(GotL - WantL) <= 0.5f : (GotR >= 0.5f && GotR <= ABRHUD::PadScrollLine + 0.5f);
			BRTestV417Carnet::Scrolled += bOk ? 1 : 0;
			Note(FString::Printf(TEXT("carnet long, Niveau %d : texte cache, reste a gauche %.0f, observations %d ; un appui (croix bas) : gauche %+.1f, OBSERVATIONS %+.1f (attendu %s)"),
				Level, BRTestV417Carnet::RoomLeft, H->LastNotebookObservations, GotL, GotR,
				bLeft ? *FString::Printf(TEXT("gauche +%.0f"), WantL) : *FString::Printf(TEXT("OBSERVATIONS +1 a +%.0f"), ABRHUD::PadScrollLine)), !bOk);
			Shot(FString::Printf(TEXT("v417_carnet_long_niveau%d_apres_appui"), Level));
			return true;
		});
		Add(FString::Printf(TEXT("v4.17 carnet long : Niveau %d : fermeture"), Level), 0.3f, [this]()
		{
			if (ABRPlayerController* PC = GetPC())
			{
				PC->SetInventoryOpen(false);
			}
			return true;
		});
	}
	Add(TEXT("v4.17 carnet long : bilan"), 0.f, [this]()
	{
		if (BRTestV417Carnet::Overflowing == 0)
		{
			Skip(TEXT("carnet long : aucun carnet ne deborde de sa zone dans ces niveaux et cette fenetre : defilement d'un carnet long NON VERIFIE"));
		}
		else
		{
			Note(FString::Printf(TEXT("carnet long : %d niveau(x) avec du texte cache, %d defile(nt) d'une ligne a l'appui"), BRTestV417Carnet::Overflowing, BRTestV417Carnet::Scrolled),
				BRTestV417Carnet::Scrolled < BRTestV417Carnet::Overflowing);
		}
		if (bSettingsSaved)
		{
			FBRSettings::Get() = SavedSettings;
			bSettingsSaved = false;
			if (ABRPlayerController* PC = GetPC())
			{
				PC->ApplySettings();
			}
		}
		if (!BRTestV417Carnet::SavedLang.IsEmpty())
		{
			BRLoc::SetLanguage(BRTestV417Carnet::SavedLang);
		}
		return true;
	});
}
