#include "BRHUD.h"
#include "BRFonts.h"
#include "BRLoc.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRDisplay.h"
#include "BREntity.h"
#include "BRItems.h"
#include "BRKeys.h"
#include "BRLevels.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRWorld.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineFontServices.h"
#include "EngineUtils.h"
#include "Fonts/FontCache.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Internationalization/BreakIterator.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

namespace
{
	// Palette de l'interface (jaune "Escape Together" sur fond sombre translucide)
	const FLinearColor Yellow(1.f, 0.82f, 0.22f, 1.f);
	const FLinearColor YellowDim(0.95f, 0.78f, 0.25f, 0.45f);
	const FLinearColor Ink(0.93f, 0.91f, 0.84f, 1.f);
	const FLinearColor InkDim(0.75f, 0.72f, 0.62f, 0.8f);
	const FLinearColor PanelBg(0.035f, 0.03f, 0.012f, 0.72f);
	const FLinearColor SlotBg(0.f, 0.f, 0.f, 0.5f);
	const FLinearColor Danger(0.95f, 0.3f, 0.22f, 1.f);
	const FLinearColor Done(0.55f, 0.88f, 0.5f, 0.9f);

	/** v4.9 : charge des piles, information des objets lumineux et des piles (plus de jauge PILES a l'ecran) */
	FString ChargeLine(const ABRCharacter* C, EBRItem Item)
	{
		if (!C || !(Item == EBRItem::Flashlight || Item == EBRItem::Headlamp || Item == EBRItem::Camcorder || Item == EBRItem::Battery))
		{
			return FString();
		}
		const int32 Pct = FMath::RoundToInt(FMath::Clamp(C->Battery, 0.f, 100.f));
		return Item == EBRItem::Battery
			? BRLoc::Fmt(NSLOCTEXT("BR", "HUD.BatterySpare", "Charge actuelle de la lampe et du cam\u00e9scope : {Pct} %. Une pile neuve la remet \u00e0 100 %."), { { TEXT("Pct"), BRLoc::Int(Pct) } })
			: BRLoc::Fmt(NSLOCTEXT("BR", "HUD.BatteryCharge", "Charge des piles : {Pct} %"), { { TEXT("Pct"), BRLoc::Int(Pct) } });
	}

	enum EButtonId
	{
		Btn_TabCharacter = 100,
		Btn_TabJournal = 101,
		Btn_TabSettings = 102,
		Btn_TabKeys = 103,
		Btn_InspectUse = 200,
		Btn_InspectClose = 201,
		Btn_SettingBase = 300,   // 300 + ligne * 2 (+0 = moins, +1 = plus)
		Btn_SettingRow = 600,    // 600 + ligne (clic sur la ligne entiere)
		Btn_KeysReset = 900,
		Btn_PauseResume = 950,
		Btn_PauseSettings = 951,
		Btn_PauseKeys = 952,
		Btn_PauseQuit = 953,
		Btn_PauseMainMenu = 954,
		Btn_Menu = 960,          // 960 + element de la page du menu principal
		Btn_MenuLevelPrev = 980,
		Btn_MenuLevelNext = 981,
		Btn_MenuCard = 990,      // 990..996 : cartes du carrousel des niveaux (993 = carte centrale)
		Btn_Language = 1400,     // 1400 + element de la page Langue (22 langues + RETOUR)
		Btn_SaveDelete = 1500,   // 1500 + emplacement : corbeille d'une partie
		Btn_SettingCat = 1600,   // v4.9 : 1600 + categorie de l'onglet Parametres (jeu, video, interface, graphismes)
		Btn_VideoKeep = 1700,    // v4.9 : confirmation de l'affichage
		Btn_VideoRevert = 1701,
		Btn_KeySlot = 1000       // 1000 + action * 3 + case
	};

	FLinearColor ClassColor(int32 Class)
	{
		switch (Class)
		{
		case 0:
		case 1:
			return FLinearColor(0.55f, 0.95f, 0.55f);
		case 2:
			return FLinearColor(1.f, 0.9f, 0.4f);
		case 3:
			return FLinearColor(1.f, 0.6f, 0.25f);
		default:
			return FLinearColor(1.f, 0.3f, 0.25f);
		}
	}

	FString Timecode(float Seconds)
	{
		const int32 T = FMath::FloorToInt(Seconds);
		return FString::Printf(TEXT("%02d:%02d:%02d"), T / 3600, (T / 60) % 60, T % 60);
	}

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A *= A;
		return C;
	}

	float EaseOut(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return 1.f - (1.f - T) * (1.f - T) * (1.f - T);
	}

	float Smooth(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return A + (B - A) * T;
	}

	/**
	 * Police "objet" de l'interface. Le Canvas n'affiche un texte que si sa FSlateFontInfo porte une UFont
	 * (FCanvasTextItem::HasValidText teste Font) : la police Slate par defaut seule (v4.0, v4.1) ne s'affichait pas.
	 * On prend la police Roboto du moteur (police "runtime" composite de preference).
	 */
	UFont* UiFontObject()
	{
		static TWeakObjectPtr<UFont> Cached;
		if (UFont* F = Cached.Get())
		{
			return F;
		}
		UFont* Best = nullptr;
		if (GEngine)
		{
			for (UFont* F : { GEngine->GetMediumFont(), GEngine->GetLargeFont(), GEngine->GetSmallFont() })
			{
				if (F && F->FontCacheType == EFontCacheType::Runtime)
				{
					Best = F;
					break;
				}
				Best = Best ? Best : F;
			}
		}
		// v4.8 : la police du moteur plus les ecritures qu'elle n'a pas (arabe, persan, chinois, japonais, coreen)
		Best = BRFonts::WithScripts(Best);
		Cached = Best;
		return Best;
	}

	/** Graisse demandee si la police la contient, sinon la plus proche ; NAME_None : premiere police de la famille */
	FName PickTypeface(const FCompositeFont* Composite, int32 Weight)
	{
		static const TCHAR* const Wanted[4][2] = {
			{ TEXT("Light"), TEXT("Regular") },
			{ TEXT("Regular"), nullptr },
			{ TEXT("Bold"), nullptr },
			{ TEXT("Black"), TEXT("Bold") } };
		if (!Composite)
		{
			return FName(Wanted[Weight][0]);
		}
		for (const TCHAR* Name : Wanted[Weight])
		{
			if (!Name)
			{
				break;
			}
			const FName Face(Name);
			for (const FTypefaceEntry& Entry : Composite->DefaultTypeface.Fonts)
			{
				if (Entry.Name == Face)
				{
					return Face;
				}
			}
		}
		return NAME_None;
	}

	/** Police de l'interface v4 : Roboto, graisse Light / Regular / Bold / Black */
	FSlateFontInfo UiFontInfo(float Size, int32 Weight, float U)
	{
		// Tailles entieres : chaque taille occupe sa place dans l'atlas des polices
		const float Pt = FMath::Max(6.f, FMath::RoundToFloat(Size * U));
		const int32 Wt = FMath::Clamp(Weight, 0, 3);
		UFont* Font = UiFontObject();
		if (!Font)
		{
			static const FName Faces[] = { FName(TEXT("Light")), FName(TEXT("Regular")), FName(TEXT("Bold")), FName(TEXT("Black")) };
			return FCoreStyle::GetDefaultFontStyle(Faces[Wt], Pt);
		}
		static TWeakObjectPtr<UFont> ResolvedFor;
		static FName Faces[4];
		if (ResolvedFor.Get() != Font)
		{
			ResolvedFor = Font;
			const FSlateFontInfo Probe(Font, Pt);
			for (int32 i = 0; i < 4; ++i)
			{
				Faces[i] = PickTypeface(Probe.GetCompositeFont(), i);
			}
		}
		return FSlateFontInfo(Font, Pt, Faces[Wt]);
	}

	// ---- v4.8 : ecritures (mise en forme, graphemes, coupure des lignes)

	bool IsJoiningScript(uint32 C)
	{
		// Hebreu, arabe, syriaque, thaana, persan, formes de presentation ; marques de direction et liants invisibles
		return (C >= 0x0590 && C <= 0x08FF) || (C >= 0xFB1D && C <= 0xFDFF) || (C >= 0xFE70 && C <= 0xFEFF) || (C >= 0x200C && C <= 0x200F)
			|| (C >= 0x202A && C <= 0x202E) || (C >= 0x2066 && C <= 0x2069);
	}

	/** Texte a mettre en forme avant le dessin : lettres liees de l'arabe et du persan, ordre de droite a gauche.
	 *  Le Canvas dessine sinon caractere par caractere, de gauche a droite (lettres isolees et mot a l'envers). */
	bool NeedsShaping(const FString& S)
	{
		for (int32 i = 0; i < S.Len(); ++i)
		{
			if (IsJoiningScript(static_cast<uint32>(S[i])))
			{
				return true;
			}
		}
		return false;
	}

	/** Fins des graphemes de S (lettre + accents combines, paire de substitution, emoji compose : jamais separes) */
	TArray<int32> GraphemeEnds(const FString& S)
	{
		TArray<int32> Ends;
		bool bSimple = true;
		for (int32 i = 0; i < S.Len() && bSimple; ++i)
		{
			const uint32 C = static_cast<uint32>(S[i]);
			bSimple = !((C >= 0x0300 && C <= 0x036F) || (C >= 0x1AB0 && C <= 0x1AFF) || (C >= 0x1DC0 && C <= 0x1DFF) || (C >= 0x20D0 && C <= 0x20FF)
				|| (C >= 0xFE00 && C <= 0xFE2F) || (C >= 0xD800 && C <= 0xDFFF) || (C >= 0x1100 && C <= 0x11FF) || C == 0x200D || IsJoiningScript(C));
		}
		if (bSimple)
		{
			// Cas courant (latin, cyrillique, ideogrammes, kana, hangeul precompose) : un caractere = un grapheme
			Ends.Reserve(S.Len());
			for (int32 i = 1; i <= S.Len(); ++i)
			{
				Ends.Add(i);
			}
			return Ends;
		}
		const TSharedRef<IBreakIterator> It = FBreakIterator::CreateCharacterBoundaryIterator();
		It->SetString(FString(S));
		for (int32 B = It->MoveToNext(); B != INDEX_NONE; B = It->MoveToNext())
		{
			Ends.Add(B);
		}
		if (Ends.Num() == 0 || Ends.Last() != S.Len())
		{
			Ends.Add(S.Len());
		}
		return Ends;
	}

	/** Points ou une ligne peut finir (regles Unicode UAX #14, ICU : apres les espaces et les traits d'union, entre deux
	 *  ideogrammes, jamais devant une ponctuation fermante) ; le dernier est la fin du texte */
	TArray<int32> LineBreaks(const FString& S)
	{
		TArray<int32> Out;
		const TSharedRef<IBreakIterator> It = FBreakIterator::CreateLineBreakIterator();
		It->SetString(FString(S));
		for (int32 B = It->MoveToNext(); B != INDEX_NONE; B = It->MoveToNext())
		{
			Out.Add(B);
		}
		if (Out.Num() == 0 || Out.Last() != S.Len())
		{
			Out.Add(S.Len());
		}
		return Out;
	}

	/** Coupe S en lignes d'au plus MaxWidth (Measure : largeur d'un texte en pixels) */
	TArray<FString> WrapText(const FString& S, float MaxWidth, TFunctionRef<float(const FString&)> Measure)
	{
		TArray<FString> Lines;
		TArray<FString> Paragraphs;
		S.ParseIntoArray(Paragraphs, TEXT("\n"), false);
		for (const FString& P : Paragraphs)
		{
			if (P.TrimStartAndEnd().IsEmpty())
			{
				Lines.Add(FString());
				continue;
			}
			const TArray<int32> Breaks = LineBreaks(P);
			int32 Start = 0;
			int32 Fit = INDEX_NONE; // dernier point de coupure qui tient sur la ligne en cours
			int32 k = 0;
			while (k < Breaks.Num())
			{
				const int32 B = Breaks[k];
				if (B <= Start)
				{
					++k;
					continue;
				}
				if (Measure(P.Mid(Start, B - Start).TrimEnd()) <= MaxWidth)
				{
					Fit = B;
					++k;
					continue;
				}
				if (Fit != INDEX_NONE)
				{
					// La ligne finit au dernier point qui tenait ; le meme segment est repris sur la ligne suivante
					Lines.Add(P.Mid(Start, Fit - Start).TrimEnd());
					Start = Fit;
					Fit = INDEX_NONE;
					continue;
				}
				// Un segment seul plus large que la ligne (mot tres long, adresse) : coupe entre deux graphemes
				const FString Segment = P.Mid(Start, B - Start);
				const TArray<int32> Ends = GraphemeEnds(Segment);
				int32 Cut = Ends.Num() > 0 ? Ends[0] : Segment.Len();
				for (int32 g = 1; g < Ends.Num() && Measure(Segment.Left(Ends[g])) <= MaxWidth; ++g)
				{
					Cut = Ends[g];
				}
				if (Cut >= Segment.Len())
				{
					// Le segment ne depasse qu'a cause de ses espaces de fin : il tient
					Fit = B;
					++k;
					continue;
				}
				Lines.Add(Segment.Left(Cut));
				Start += Cut;
			}
			if (Start < P.Len())
			{
				const FString Rest = P.Mid(Start).TrimEnd();
				if (!Rest.IsEmpty())
				{
					Lines.Add(Rest);
				}
			}
		}
		return Lines;
	}

	/** Culture courante : les caches de texte mis en forme ou coupe en dependent (sous-polices propres a une langue) */
	bool CultureChanged(FString& Seen)
	{
		const FString Now = FInternationalization::Get().GetCurrentLanguage()->GetName();
		if (Now != Seen)
		{
			Seen = Now;
			return true;
		}
		return false;
	}

	/** Texte mis en forme (HarfBuzz, ordre bidirectionnel), garde pour les images suivantes */
	TOptional<FShapedGlyphSequenceRef> ShapeText(const FString& S, const FSlateFontInfo& Font)
	{
		static TMap<FString, FShapedGlyphSequenceRef> Cache;
		static FString Culture;
		if (CultureChanged(Culture) || Cache.Num() > 512)
		{
			Cache.Reset();
		}
		const TSharedPtr<FSlateFontCache> FontCache = FEngineFontServices::IsInitialized() ? FEngineFontServices::Get().GetFontCache() : nullptr;
		if (!FontCache.IsValid())
		{
			return TOptional<FShapedGlyphSequenceRef>();
		}
		const bool bRtl = BRLoc::IsRightToLeft();
		const FString Key = FString::Printf(TEXT("%d|%s|%d|%s"), FMath::RoundToInt(static_cast<float>(Font.Size)), *Font.TypefaceFontName.ToString(), bRtl ? 1 : 0, *S);
		if (const FShapedGlyphSequenceRef* Found = Cache.Find(Key))
		{
			if (!(*Found)->IsDirty())
			{
				return *Found;
			}
		}
		const FShapedGlyphSequenceRef Shaped = FontCache->ShapeBidirectionalText(S, Font, 1.f,
			bRtl ? TextBiDi::ETextDirection::RightToLeft : TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::Auto);
		Cache.Add(Key, Shaped);
		return Shaped;
	}

	/** Taille, en points de l'interface, d'un texte des anciennes primitives (police du moteur a l'echelle Scale) */
	float LegacySize(UFont* Font, float Scale, float U)
	{
		const int32 Pt = Font && Font->LegacyFontSize > 0 ? Font->LegacyFontSize : 24;
		return Pt * Scale / FMath::Max(U, 0.01f);
	}

	/** "Classe 1 : Sur - Stable" -> "CLASSE 1" (deux-points latin ou pleine chasse ; majuscules selon la langue : I turc...) */
	FString ClassShort(const FString& ClassText)
	{
		int32 Colon = INDEX_NONE;
		if (!ClassText.FindChar(TEXT(':'), Colon))
		{
			ClassText.FindChar(TEXT('\xFF1A'), Colon);
		}
		const FString Head = Colon != INDEX_NONE ? ClassText.Left(Colon).TrimEnd() : ClassText;
		return FText::AsCultureInvariant(Head).ToUpper().ToString();
	}

	/** Phase du directeur de tension (mode developpeur) */
	FText TensionLabel(ABRWorld::ETension T)
	{
		switch (T)
		{
		case ABRWorld::ETension::Unease:
			return NSLOCTEXT("BR", "HUD.Tension.Unease", "malaise");
		case ABRWorld::ETension::Detection:
			return NSLOCTEXT("BR", "HUD.Tension.Detection", "d\u00e9tection");
		case ABRWorld::ETension::Chase:
			return NSLOCTEXT("BR", "HUD.Tension.Chase", "poursuite");
		case ABRWorld::ETension::Recovery:
			return NSLOCTEXT("BR", "HUD.Tension.Recovery", "r\u00e9pit");
		default:
			return NSLOCTEXT("BR", "HUD.Tension.Calm", "calme");
		}
	}

	/** Carte du carrousel a dessiner : niveau et ecart (en cartes) avec le centre */
	struct FCardDraw
	{
		int32 Index;
		float Off;
	};

	/** Astuces du menu titre ({Action} : touche configuree) */
	const TArray<FText>& MenuTips()
	{
		static const TArray<FText> Tips = {
			NSLOCTEXT("BR", "HUD.SmilersBraquezJamaisVotreLampe", "Smilers : ne braquez JAMAIS votre lampe sur eux. \u00c9teignez-la, ne courez pas et reculez lentement."),
			NSLOCTEXT("BR", "HUD.HoundsFuyezCourantFaitesLeur", "Hounds : ne fuyez pas en courant. Faites-leur face, regardez-les et reculez calmement."),
			NSLOCTEXT("BR", "HUD.PartygoersSoutenezLeurRegardPendant", "Partygoers : ne soutenez pas leur regard. Pendant les coupures de courant, cachez-vous."),
			NSLOCTEXT("BR", "HUD.EauAmandeApaiseEspritBuvez", "L'eau d'amande apaise l'esprit : buvez-en ({Drink}) quand votre sant\u00e9 mentale baisse."),
			NSLOCTEXT("BR", "HUD.Niveau0RamassezCassettesVhs", "Au Niveau 0, ramassez les cassettes VHS et filmez une coupure de courant pour ouvrir la sortie."),
			NSLOCTEXT("BR", "HUD.MultijoueurJoueurPcPuissantDoit", "En multijoueur, c'est le joueur qui a le PC le plus puissant qui doit h\u00e9berger la partie."),
			NSLOCTEXT("BR", "HUD.InventoryOuvreInventaireGlissezObjets", "{Inventory} ouvre l'inventaire : glissez les objets, double-cliquez pour les utiliser."),
			NSLOCTEXT("BR", "HUD.PlacardsTrousMursFoisCache", "Placards, trous dans les murs : une fois cach\u00e9, les entit\u00e9s ne vous voient plus."),
			NSLOCTEXT("BR", "HUD.DeathmothsEteignezVotreLampeEntendez", "Deathmoths : \u00e9teignez votre lampe d\u00e8s que vous entendez des battements d'ailes."),
			NSLOCTEXT("BR", "HUD.EntendezFrapperNiveau0Eloignez", "Si vous entendez frapper au Niveau 0, \u00e9loignez-vous : la Bacteria n'est pas loin."),
			NSLOCTEXT("BR", "HUD.ToutesTouchesChangentParametresTouches", "Toutes les touches se changent dans Param\u00e8tres > Touches (jusqu'\u00e0 3 par action)."),
			NSLOCTEXT("BR", "HUD.CoopCoequipierTerreReleveMaintenez", "Coop : un co\u00e9quipier \u00e0 terre se rel\u00e8ve si vous maintenez {Interact} pr\u00e8s de lui.")
		};
		return Tips;
	}

}

FString ABRHUD::ControlsLine(int32 Line) const
{
	using namespace BRKeys;
	if (Line == 0)
	{
		return BRLoc::Fmt(NSLOCTEXT("BR", "HUD.MoveforwardMoveleftMovebackwardMoveright", "{MoveForward}{MoveLeft}{MoveBackward}{MoveRight}  se d\u00e9placer     {Sprint}  courir     {Crouch}  s'accroupir     {Jump}  sauter     {Interact}  interagir     {ThirdPerson}  vue 3e personne"), { { TEXT("MoveForward"), BRLoc::Arg(Primary(EBRAction::MoveForward)) }, { TEXT("MoveLeft"), BRLoc::Arg(Primary(EBRAction::MoveLeft)) }, { TEXT("MoveBackward"), BRLoc::Arg(Primary(EBRAction::MoveBackward)) }, { TEXT("MoveRight"), BRLoc::Arg(Primary(EBRAction::MoveRight)) }, { TEXT("Sprint"), BRLoc::Arg(Primary(EBRAction::Sprint)) }, { TEXT("Crouch"), BRLoc::Arg(Primary(EBRAction::Crouch)) }, { TEXT("Jump"), BRLoc::Arg(Primary(EBRAction::Jump)) }, { TEXT("Interact"), BRLoc::Arg(Primary(EBRAction::Interact)) }, { TEXT("ThirdPerson"), BRLoc::Arg(Primary(EBRAction::ThirdPerson)) } });
	}
	return BRLoc::Fmt(NSLOCTEXT("BR", "HUD.FlashlightLampeNightvisionVisionNocturne", "{Flashlight}  lampe     {NightVision}  vision nocturne     {Pocket1}-{Pocket4}  poches     {Drink}  eau d'amande     {Bandage}  bandage     {Battery}  piles     {Inventory}  inventaire     {Pause}  pause"), { { TEXT("Flashlight"), BRLoc::Arg(Primary(EBRAction::Flashlight)) }, { TEXT("NightVision"), BRLoc::Arg(Primary(EBRAction::NightVision)) }, { TEXT("Pocket1"), BRLoc::Arg(Primary(EBRAction::Pocket1)) }, { TEXT("Pocket4"), BRLoc::Arg(Primary(EBRAction::Pocket4)) }, { TEXT("Drink"), BRLoc::Arg(Primary(EBRAction::Drink)) }, { TEXT("Bandage"), BRLoc::Arg(Primary(EBRAction::Bandage)) }, { TEXT("Battery"), BRLoc::Arg(Primary(EBRAction::Battery)) }, { TEXT("Inventory"), BRLoc::Arg(Primary(EBRAction::Inventory)) }, { TEXT("Pause"), BRLoc::Arg(Primary(EBRAction::Pause)) } });
}

float ABRHUD::Ui() const
{
	// v4.9 : taille d'interface choisie (0,8 a 1,25), plafonnee pour que les ecrans prevus pour 1080 lignes virtuelles
	// tiennent toujours (au moins 820 lignes virtuelles). Le dessin et les zones cliquables utilisent la meme echelle.
	const float Scale = FMath::Clamp(FBRSettings::Get().UiScale, 0.8f, 1.25f);
	if (!Canvas)
	{
		return Scale;
	}
	const float Base = FMath::Max(0.5f, Canvas->ClipY / 1080.f);
	return FMath::Min(Base * Scale, FMath::Max(0.5f, Canvas->ClipY / 820.f));
}

float ABRHUD::HudAlpha() const
{
	return FMath::Clamp(FBRSettings::Get().HudOpacity, 0.4f, 1.f);
}

float ABRHUD::ScrollArea(float& Scroll, float X, float Y, float W, float H, float ContentH)
{
	// v4.9 : molette au-dessus de la zone ; position bornee au contenu ; reperes discrets s'il reste du texte cache
	const float U = Ui();
	const float MaxOffset = FMath::Max(0.f, ContentH - H);
	if (PlayerOwner && Hover(X, Y, W, H))
	{
		if (PlayerOwner->WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			Scroll += 60.f;
		}
		if (PlayerOwner->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			Scroll -= 60.f;
		}
	}
	Scroll = FMath::Clamp(Scroll, 0.f, MaxOffset / FMath::Max(U, 0.01f));
	const float Offset = Scroll * U;
	const float AX = X + W - 14.f * U;
	if (Offset > 1.f)
	{
		DrawLine(AX - 6.f * U, Y + 8.f * U, AX, Y + 2.f * U, YellowDim, 2.f * U);
		DrawLine(AX, Y + 2.f * U, AX + 6.f * U, Y + 8.f * U, YellowDim, 2.f * U);
	}
	if (Offset < MaxOffset - 1.f)
	{
		DrawLine(AX - 6.f * U, Y + H - 8.f * U, AX, Y + H - 2.f * U, YellowDim, 2.f * U);
		DrawLine(AX, Y + H - 2.f * U, AX + 6.f * U, Y + H - 8.f * U, YellowDim, 2.f * U);
	}
	return -Offset;
}

void ABRHUD::Notify(const UObject* WorldContext, const FString& Text, float Duration, FLinearColor Color)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	if (ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr)
	{
		H->AddMessage(Text, Duration, Color);
	}
}

void ABRHUD::AddMessage(const FString& Text, float Duration, const FLinearColor& Color)
{
	for (FMsg& M : Messages)
	{
		if (M.Text == Text)
		{
			M.Age = 0.f;
			return;
		}
	}
	FMsg M;
	M.Text = Text;
	M.Duration = Duration;
	M.Color = Color;
	Messages.Add(M);
	if (Messages.Num() > 5)
	{
		Messages.RemoveAt(0);
	}
}

// =====================================================================================================================
// Primitives
// =====================================================================================================================

void ABRHUD::Txt(const FString& S, float X, float Y, const FLinearColor& C, float Scale, UFont* Font, bool bCenter, bool bShadow)
{
	// v4.8 : meme taille qu'avant (police du moteur x Scale), mais avec la police de l'interface : ecritures non latines,
	// arabe mis en forme. L'ancienne police n'avait ni ideogrammes ni lettres arabes.
	TextF(S, X, Y, C, LegacySize(Font ? Font : GEngine->GetMediumFont(), Scale, Ui()), EUiWeight::Regular, bCenter ? EUiAlign::Center : EUiAlign::Left, bShadow);
}

void ABRHUD::TxtRight(const FString& S, float RightX, float Y, const FLinearColor& C, float Scale, UFont* Font)
{
	TextF(S, RightX, Y, C, LegacySize(Font ? Font : GEngine->GetMediumFont(), Scale, Ui()), EUiWeight::Regular, EUiAlign::Right, true);
}

float ABRHUD::TextW(const FString& S, UFont* Font, float Scale)
{
	return TextSize(S, LegacySize(Font ? Font : GEngine->GetMediumFont(), Scale, Ui()), EUiWeight::Regular).X;
}

void ABRHUD::TxtLine(const FString& S, float X, float Y, float W, const FLinearColor& C, float Scale, UFont* Font)
{
	// Ligne d'un paragraphe coupe par Wrap : alignee a droite du bloc dans une langue ecrite de droite a gauche
	const bool bRtl = BRLoc::IsRightToLeft();
	TextF(S, bRtl ? X + W : X, Y, C, LegacySize(Font ? Font : GEngine->GetMediumFont(), Scale, Ui()), EUiWeight::Regular, bRtl ? EUiAlign::Right : EUiAlign::Left, false);
}

TArray<FString> ABRHUD::Wrap(const FString& S, float MaxWidth, UFont* Font, float Scale)
{
	return WrapF(S, MaxWidth, LegacySize(Font ? Font : GEngine->GetMediumFont(), Scale, Ui()), EUiWeight::Regular);
}

void ABRHUD::Bar(float X, float Y, float W, float H, float Fill, const FLinearColor& C, const FString& Label)
{
	const float U = Ui();
	TextF(Label, X, Y - 20.f * U, FLinearColor(1.f, 1.f, 1.f, 0.78f), 9.5f, EUiWeight::Bold);
	const float R = H * 0.5f;
	RoundRect(X, Y, W, H, R, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	const float F = FMath::Clamp(Fill, 0.f, 1.f);
	if (F > 0.f)
	{
		RoundRect(X, Y, FMath::Max(H, W * F), H, R, C);
	}
}

void ABRHUD::Frame(float X, float Y, float W, float H, const FLinearColor& C, float Thickness)
{
	DrawRect(C, X, Y, W, Thickness);
	DrawRect(C, X, Y + H - Thickness, W, Thickness);
	DrawRect(C, X, Y + Thickness, Thickness, H - 2.f * Thickness);
	DrawRect(C, X + W - Thickness, Y + Thickness, Thickness, H - 2.f * Thickness);
}

void ABRHUD::Panel(float X, float Y, float W, float H, const FString& Title)
{
	const float U = Ui();
	RoundRect(X, Y, W, H, 14.f * U, PanelBg);
	RoundRect(X, Y, W, H, 14.f * U, FLinearColor(0.95f, 0.78f, 0.25f, 0.22f), true);
	if (!Title.IsEmpty())
	{
		TextSpaced(Title, X + 20.f * U, Y + 13.f * U, Yellow, 11.5f, EUiWeight::Bold, 2.5f * U);
		DrawRect(FLinearColor(0.95f, 0.78f, 0.25f, 0.25f), X + 18.f * U, Y + 42.f * U, W - 36.f * U, FMath::Max(1.f, U));
	}
}

void ABRHUD::VLabel(const FString& S, float X, float Y, const FLinearColor& C, float Scale)
{
	const float U = Ui();
	const float Size = 11.3f * Scale / FMath::Max(U, 0.01f);
	if (NeedsShaping(S))
	{
		// Arabe, persan : lettres liees, le mot ne s'empile pas. Il est tourne d'un quart de tour (lu de bas en haut)
		const FVector2f TS = TextSize(S, Size, EUiWeight::Bold);
		const FVector Pivot(X, Y + TS.X, 0.f);
		Canvas->Canvas->PushRelativeTransform(FTranslationMatrix(-Pivot) * FRotationMatrix(FRotator(0.f, -90.f, 0.f)) * FTranslationMatrix(Pivot));
		TextF(S, Pivot.X, Pivot.Y - TS.Y * 0.5f, C, Size, EUiWeight::Bold, EUiAlign::Left, false);
		Canvas->Canvas->PopTransform();
		return;
	}
	// Libelle vertical : graphemes empiles (ideogrammes et kana : l'ecriture verticale habituelle)
	const TArray<int32> Ends = GraphemeEnds(S);
	float LY = Y;
	int32 Prev = 0;
	for (const int32 End : Ends)
	{
		TextF(S.Mid(Prev, End - Prev), X, LY, C, Size, EUiWeight::Bold, EUiAlign::Center, false);
		LY += 16.f * Scale;
		Prev = End;
	}
}

void ABRHUD::TrendBox(float X, float Y, float Size, float Trend)
{
	const float U = Ui();
	Frame(X, Y, Size, Size, YellowDim, 1.f * U);
	const float CX = X + Size * 0.5f;
	const float CY = Y + Size * 0.5f;
	const float A = Size * 0.28f;
	if (FMath::Abs(Trend) < 0.05f)
	{
		DrawRect(InkDim, CX - A, CY - 1.f * U, A * 2.f, 2.f * U);
		return;
	}
	const bool bUp = Trend > 0.f;
	const FLinearColor C = bUp ? Done : Danger;
	const float Dir = bUp ? -1.f : 1.f;
	// fleche : tige + pointe
	DrawLine(CX, CY - Dir * A, CX, CY + Dir * A, C, 2.f * U);
	DrawLine(CX, CY + Dir * A, CX - A * 0.7f, CY + Dir * A * 0.3f, C, 2.f * U);
	DrawLine(CX, CY + Dir * A, CX + A * 0.7f, CY + Dir * A * 0.3f, C, 2.f * U);
}

void ABRHUD::Icon(UTexture* Tex, float X, float Y, float W, float H, const FLinearColor& Tint)
{
	if (Tex)
	{
		DrawTexture(Tex, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, Tint, BLEND_Translucent);
	}
}

void ABRHUD::Scanlines(float Alpha)
{
	const float U = Ui();
	const float Step = FMath::Max(3.f, 4.f * U);
	const float Offset = FMath::Fmod(Clock * 20.f * U, Step);
	for (float Y = Offset; Y < Canvas->ClipY; Y += Step)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, Alpha), 0.f, Y, Canvas->ClipX, FMath::Max(1.f, U));
	}
}

bool ABRHUD::Hover(float X, float Y, float W, float H) const
{
	return MouseX >= X && MouseX <= X + W && MouseY >= Y && MouseY <= Y + H;
}

void ABRHUD::AddButton(int32 Id, float X, float Y, float W, float H)
{
	FButton B;
	B.Id = Id;
	B.X = X;
	B.Y = Y;
	B.W = W;
	B.H = H;
	Buttons.Add(B);
}

int32 ABRHUD::ButtonAt(float X, float Y) const
{
	// Les derniers boutons ajoutes sont au-dessus
	for (int32 i = Buttons.Num() - 1; i >= 0; --i)
	{
		const FButton& B = Buttons[i];
		if (X >= B.X && X <= B.X + B.W && Y >= B.Y && Y <= B.Y + B.H)
		{
			return B.Id;
		}
	}
	return INDEX_NONE;
}

ABRHUD::FSlotBox* ABRHUD::FindSlot(float X, float Y)
{
	for (FSlotBox& B : Slots)
	{
		if (X >= B.X && X <= B.X + B.S && Y >= B.Y && Y <= B.Y + B.S)
		{
			return &B;
		}
	}
	return nullptr;
}

UTexture* ABRHUD::ItemIcon(EBRItem Item)
{
	UBRAssets* A = UBRAssets::Get(this);
	return A ? A->Icon(BRItems::Get(Item).Icon) : nullptr;
}

// =====================================================================================================================
// Primitives v4 : texte net (polices Slate), formes arrondies, degrades, halos
// =====================================================================================================================

void ABRHUD::TextF(const FString& S, float X, float Y, const FLinearColor& C, float Size, EUiWeight Weight, EUiAlign Align, bool bShadow)
{
	if (S.IsEmpty() || C.A <= 0.004f || !Canvas)
	{
		return;
	}
	const FSlateFontInfo Font = UiFontInfo(Size, static_cast<int32>(Weight), Ui());
	const float Off = FMath::Max(1.f, FMath::RoundToFloat(1.5f * Ui()));
	if (NeedsShaping(S))
	{
		// v4.8 : arabe, persan : texte mis en forme (lettres liees, droite a gauche) puis dessine tel quel
		const TOptional<FShapedGlyphSequenceRef> Shaped = ShapeText(S, Font);
		if (Shaped.IsSet())
		{
			if (Align != EUiAlign::Left)
			{
				const float W = static_cast<float>(Shaped.GetValue()->GetMeasuredWidth());
				X -= Align == EUiAlign::Center ? W * 0.5f : W;
			}
			FCanvasShapedTextItem Item(FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y)), Shaped.GetValue(), C);
			if (bShadow)
			{
				Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.6f * C.A), FVector2D(Off, Off));
			}
			Canvas->DrawItem(Item);
			return;
		}
	}
	if (Align != EUiAlign::Left)
	{
		const float W = TextSize(S, Size, Weight).X;
		X -= Align == EUiAlign::Center ? W * 0.5f : W;
	}
	FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y)), FText::FromString(S), Font, C);
	if (bShadow)
	{
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.6f * C.A), FVector2D(Off, Off));
	}
	Canvas->DrawItem(Item);
}

float ABRHUD::TextSpaced(const FString& S, float X, float Y, const FLinearColor& C, float Size, EUiWeight Weight, float Spacing, EUiAlign Align)
{
	if (S.IsEmpty())
	{
		return 0.f;
	}
	if (NeedsShaping(S))
	{
		// v4.8 : arabe, persan : l'espacement separerait des lettres liees ; le titre est dessine d'un bloc
		const float Total = TextSize(S, Size, Weight).X;
		TextF(S, X, Y, C, Size, Weight, Align, false);
		return Total;
	}
	// Espacement entre graphemes : une lettre et ses accents combines (ou une paire de substitution) restent ensemble
	const TArray<int32> Ends = GraphemeEnds(S);
	TArray<float> Widths;
	Widths.Reserve(Ends.Num());
	float Total = 0.f;
	int32 Prev = 0;
	for (int32 i = 0; i < Ends.Num(); ++i)
	{
		const float CW = TextSize(S.Mid(Prev, Ends[i] - Prev), Size, Weight).X;
		Widths.Add(CW);
		Total += CW + (i + 1 < Ends.Num() ? Spacing : 0.f);
		Prev = Ends[i];
	}
	if (Align != EUiAlign::Left)
	{
		X -= Align == EUiAlign::Center ? Total * 0.5f : Total;
	}
	Prev = 0;
	for (int32 i = 0; i < Ends.Num(); ++i)
	{
		TextF(S.Mid(Prev, Ends[i] - Prev), X, Y, C, Size, Weight, EUiAlign::Left, false);
		X += Widths[i] + Spacing;
		Prev = Ends[i];
	}
	return Total;
}

FVector2f ABRHUD::TextSize(const FString& S, float Size, EUiWeight Weight) const
{
	const FSlateFontInfo Font = UiFontInfo(Size, static_cast<int32>(Weight), Ui());
	// GetFontMeasure() renvoie un TSharedPtr depuis Unreal 5.8
	const TSharedPtr<FSlateFontMeasure> Measure = FEngineFontServices::IsInitialized() ? FEngineFontServices::Get().GetFontMeasure() : nullptr;
	if (Measure.IsValid())
	{
		const float LineH = static_cast<float>(Measure->GetMaxCharacterHeight(Font));
		if (S.IsEmpty())
		{
			return FVector2f(0.f, LineH);
		}
		if (NeedsShaping(S))
		{
			// Largeur du texte mis en forme (les lettres liees de l'arabe n'ont pas la largeur des lettres isolees)
			const TOptional<FShapedGlyphSequenceRef> Shaped = ShapeText(S, Font);
			if (Shaped.IsSet())
			{
				return FVector2f(static_cast<float>(Shaped.GetValue()->GetMeasuredWidth()), FMath::Max(static_cast<float>(Shaped.GetValue()->GetMaxTextHeight()), LineH));
			}
		}
		const FVector2D M(Measure->Measure(S, Font));
		return FVector2f(static_cast<float>(M.X), FMath::Max(static_cast<float>(M.Y), LineH));
	}
	const float Px = Font.Size * 4.f / 3.f;
	return FVector2f(S.Len() * Px * 0.52f, Px * 1.2f);
}

TArray<FString> ABRHUD::WrapF(const FString& S, float MaxWidth, float Size, EUiWeight Weight) const
{
	// Les memes textes sont coupes a chaque image : le resultat est garde (cle : largeur, taille en pixels, graisse, texte)
	static TMap<FString, TArray<FString>> Cache;
	static FString Culture;
	if (CultureChanged(Culture) || Cache.Num() > 256)
	{
		Cache.Reset();
	}
	const FString Key = FString::Printf(TEXT("%d|%d|%d|%s"), FMath::RoundToInt(MaxWidth), FMath::RoundToInt(Size * Ui() * 10.f), static_cast<int32>(Weight), *S);
	if (const TArray<FString>* Found = Cache.Find(Key))
	{
		return *Found;
	}
	TArray<FString> Lines = WrapText(S, MaxWidth, [this, Size, Weight](const FString& T) { return TextSize(T, Size, Weight).X; });
	Cache.Add(Key, Lines);
	return Lines;
}

float ABRHUD::FitSize(const FString& S, float MaxWidth, float Size, EUiWeight Weight, float MinScale) const
{
	// v4.8 : les libelles traduits sont plus ou moins longs que le francais : la taille baisse (jusqu'a MinScale) pour tenir
	const float W = TextSize(S, Size, Weight).X;
	if (W <= MaxWidth || W <= 0.f)
	{
		return Size;
	}
	return Size * FMath::Max(MinScale, MaxWidth / W);
}

void ABRHUD::TextFit(const FString& S, float X, float Y, float MaxWidth, const FLinearColor& C, float Size, EUiWeight Weight, EUiAlign Align, bool bShadow)
{
	const float Fitted = FitSize(S, MaxWidth, Size, Weight);
	// Hauteur gardee : le texte reduit reste centre sur la ligne prevue
	const float DY = (TextSize(S, Size, Weight).Y - TextSize(S, Fitted, Weight).Y) * 0.5f;
	TextF(Ellipsize(S, MaxWidth, Fitted, Weight), X, Y + DY, C, Fitted, Weight, Align, bShadow);
}

void ABRHUD::DrawParagraph(const TArray<FString>& Lines, float X, float Y, float W, float LineH, const FLinearColor& C, float Size, EUiWeight Weight)
{
	const bool bRtl = BRLoc::IsRightToLeft();
	for (const FString& L : Lines)
	{
		TextF(L, bRtl ? X + W : X, Y, C, Size, Weight, bRtl ? EUiAlign::Right : EUiAlign::Left, false);
		Y += LineH;
	}
}

FString ABRHUD::Ellipsize(const FString& S, float MaxWidth, float Size, EUiWeight Weight) const
{
	if (TextSize(S, Size, Weight).X <= MaxWidth)
	{
		return S;
	}
	// Coupe entre deux graphemes (jamais au milieu d'une lettre accentuee composee ou d'une paire de substitution)
	const TArray<int32> Ends = GraphemeEnds(S);
	for (int32 g = Ends.Num() - 2; g >= 0; --g)
	{
		const FString Cut = S.Left(Ends[g]).TrimEnd() + TEXT("\u2026");
		if (TextSize(Cut, Size, Weight).X <= MaxWidth)
		{
			return Cut;
		}
	}
	return TEXT("\u2026");
}

UTexture* ABRHUD::UiTex(const TCHAR* Name)
{
	UBRAssets* A = UBRAssets::Get(this);
	return A ? A->Icon(FName(Name)) : nullptr;
}

void ABRHUD::RoundRect(float X, float Y, float W, float H, float R, const FLinearColor& C, bool bOutline)
{
	if (C.A <= 0.003f || W < 1.f || H < 1.f)
	{
		return;
	}
	X = FMath::RoundToFloat(X);
	Y = FMath::RoundToFloat(Y);
	W = FMath::RoundToFloat(W);
	H = FMath::RoundToFloat(H);
	R = FMath::Min(FMath::RoundToFloat(R), FMath::FloorToFloat(FMath::Min(W, H) * 0.5f));
	UTexture* T = UiTex(bOutline ? TEXT("UI_RoundLine") : TEXT("UI_Round"));
	if (!T || R < 2.f)
	{
		if (bOutline)
		{
			Frame(X, Y, W, H, C, FMath::Max(1.f, FMath::RoundToFloat(Ui())));
		}
		else
		{
			DrawRect(C, X, Y, W, H);
		}
		return;
	}
	// 9 tranches : coins de R pixels (16 texels sur 64), bords etires, centre plein
	const float E = 0.25f;
	const float MW = W - 2.f * R;
	const float MH = H - 2.f * R;
	auto Piece = [&](float PX, float PY, float PW, float PH, float U0, float V0, float UL, float VL)
	{
		if (PW > 0.f && PH > 0.f)
		{
			DrawTexture(T, PX, PY, PW, PH, U0, V0, UL, VL, C, BLEND_Translucent);
		}
	};
	Piece(X, Y, R, R, 0.f, 0.f, E, E);
	Piece(X + W - R, Y, R, R, 1.f - E, 0.f, E, E);
	Piece(X, Y + H - R, R, R, 0.f, 1.f - E, E, E);
	Piece(X + W - R, Y + H - R, R, R, 1.f - E, 1.f - E, E, E);
	Piece(X + R, Y, MW, R, E, 0.f, 1.f - 2.f * E, E);
	Piece(X + R, Y + H - R, MW, R, E, 1.f - E, 1.f - 2.f * E, E);
	Piece(X, Y + R, R, MH, 0.f, E, E, 1.f - 2.f * E);
	Piece(X + W - R, Y + R, R, MH, 1.f - E, E, E, 1.f - 2.f * E);
	if (!bOutline)
	{
		Piece(X + R, Y + R, MW, MH, E, E, 1.f - 2.f * E, 1.f - 2.f * E);
	}
}

void ABRHUD::Gradient(float X, float Y, float W, float H, const FLinearColor& C, int32 Dir)
{
	if (C.A <= 0.003f || W < 1.f || H < 1.f)
	{
		return;
	}
	const bool bHoriz = Dir < 2;
	if (UTexture* T = UiTex(bHoriz ? TEXT("UI_GradH") : TEXT("UI_GradV")))
	{
		// Une demi-texel de marge : le filtrage ne melange pas les deux extremites
		const float In = 0.5f / 256.f;
		const bool bFlip = Dir == 1 || Dir == 3;
		const float A0 = bFlip ? 1.f - In : In;
		const float AL = (bFlip ? -1.f : 1.f) * (1.f - 2.f * In);
		if (bHoriz)
		{
			DrawTexture(T, X, Y, W, H, A0, 0.25f, AL, 0.5f, C, BLEND_Translucent);
		}
		else
		{
			DrawTexture(T, X, Y, W, H, 0.25f, A0, 0.5f, AL, C, BLEND_Translucent);
		}
		return;
	}
	// Repli sans texture : bandes successives
	const int32 N = 24;
	for (int32 i = 0; i < N; ++i)
	{
		const float T0 = static_cast<float>(i) / N;
		const float K = 1.f - (T0 + 0.5f / N);
		const float Fade = K * K * (3.f - 2.f * K);
		const int32 Step = (Dir == 0 || Dir == 2) ? i : N - 1 - i;
		const FLinearColor BandC = WithAlpha(C, Fade);
		if (bHoriz)
		{
			DrawRect(BandC, X + W * Step / N, Y, W / N + 1.f, H);
		}
		else
		{
			DrawRect(BandC, X, Y + H * Step / N, W, H / N + 1.f);
		}
	}
}

void ABRHUD::Glow(float CX, float CY, float RX, float RY, const FLinearColor& C)
{
	if (C.A <= 0.003f)
	{
		return;
	}
	if (UTexture* T = UiTex(TEXT("UI_Radial")))
	{
		DrawTexture(T, CX - RX, CY - RY, RX * 2.f, RY * 2.f, 0.f, 0.f, 1.f, 1.f, C, BLEND_Translucent);
	}
}

float ABRHUD::KeyCap(float X, float Y, const FString& Key, const FString& Label, float Alpha, bool bDraw)
{
	const float U = Ui();
	const FVector2f KS = TextSize(Key, 10.5f, EUiWeight::Bold);
	const float KH = 26.f * U;
	const float KW = FMath::Max(KH, KS.X + 18.f * U);
	float W = KW;
	FVector2f LS = FVector2f::ZeroVector;
	if (!Label.IsEmpty())
	{
		LS = TextSize(Label, 11.5f, EUiWeight::Regular);
		W += 9.f * U + LS.X;
	}
	if (bDraw)
	{
		RoundRect(X, Y, KW, KH, 6.f * U, FLinearColor(1.f, 0.93f, 0.75f, 0.12f * Alpha));
		RoundRect(X, Y, KW, KH, 6.f * U, FLinearColor(1.f, 0.9f, 0.6f, 0.38f * Alpha), true);
		DrawRect(FLinearColor(1.f, 0.9f, 0.6f, 0.22f * Alpha), X + 5.f * U, Y + KH - 2.f * U, KW - 10.f * U, FMath::Max(1.f, U));
		TextF(Key, X + KW * 0.5f, Y + (KH - KS.Y) * 0.5f, WithAlpha(Ink, Alpha), 10.5f, EUiWeight::Bold, EUiAlign::Center, false);
		if (!Label.IsEmpty())
		{
			TextF(Label, X + KW + 9.f * U, Y + (KH - LS.Y) * 0.5f, WithAlpha(InkDim, Alpha), 11.5f, EUiWeight::Regular, EUiAlign::Left, false);
		}
	}
	return W;
}

void ABRHUD::KeyHints(float X, float Y, const TArray<TPair<FString, FString>>& Hints, float Alpha, bool bCenter)
{
	const float Gap = 28.f * Ui();
	if (bCenter)
	{
		float Total = 0.f;
		for (const TPair<FString, FString>& H : Hints)
		{
			Total += KeyCap(0.f, 0.f, H.Key, H.Value, Alpha, false) + Gap;
		}
		X -= (Total - Gap) * 0.5f;
	}
	for (const TPair<FString, FString>& H : Hints)
	{
		X += KeyCap(X, Y, H.Key, H.Value, Alpha) + Gap;
	}
}

// =====================================================================================================================
// Boucle principale
// =====================================================================================================================

void ABRHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const float Dt = LastTime > 0.0 ? FMath::Min(static_cast<float>(Now - LastTime), 0.1f) : 0.f;
	LastTime = Now;
	Clock += Dt;
	UiDt = Dt;

	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	ABRCharacter* C = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	ABRWorld* W = ABRWorld::Get(this);
	// v4.9 (tests) : jauges de statut dessinees pendant cette image
	LastFrameStatusGauges = StatusGaugesDrawn;
	StatusGaugesDrawn = 0;

	// Animations d'ouverture du menu titre et de la pause
	const bool bMenuNow = PC && PC->IsInMenu();
	if (bMenuNow && !bWasInMenu)
	{
		MenuIntro = 0.f;
		LastMenuPage = -1;
		Carousel = -1000.f;
		TipClock = FMath::FRand() * 90.f; // une astuce au hasard a chaque lancement
	}
	bWasInMenu = bMenuNow;
	const bool bPauseNow = PC && PC->IsPauseMenuOpen();
	if (bPauseNow && !bWasPaused)
	{
		PauseTime = 0.f;
		for (float& S : PauseSel)
		{
			S = 0.f;
		}
	}
	bWasPaused = bPauseNow;

	if (PC && PC->IsInMenu())
	{
		if (W && W->GetFade() > 0.001f)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, W->GetFade()), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		}
		DrawMenu();
		if (PC->IsInventoryOpen())
		{
			// Parametres et touches depuis le menu titre. v4.9 : sans personnage (celui qui est derriere le menu n'est pas
			// celui de la partie : ni ses objets, ni ses jauges)
			DrawInventory(PC, nullptr, W);
		}
		DrawMessages(Dt);
		DrawVideoConfirm(PC);
		return;
	}

	if (PC && C && C->IsDead() && PC->IsInventoryOpen())
	{
		PC->SetInventoryOpen(false);
	}
	// v4.9 : changement de niveau : l'inventaire se ferme (aucun glisser-deposer sur un etat qui change)
	if (PC && W && W->IsTransitioning() && PC->IsInventoryOpen())
	{
		PC->SetInventoryOpen(false);
	}
	const bool bInv = PC && PC->IsInventoryOpen();
	if (!bInv && bWasInventoryOpen)
	{
		Dragging = FSlotRef();
		Inspecting = FSlotRef();
		HoverSlot = FSlotRef();
	}
	bWasInventoryOpen = bInv;

	if (C && !bInv)
	{
		DrawTeammates(C);
		DrawVoiceIndicator(PC);
	}
	if (!bInv)
	{
		DrawTitleCard(); // bandes noires et titre du niveau, sous les informations du HUD
	}
	if (C && !C->IsDead() && !bInv)
	{
		DrawRecording(C, W);
		if (!W || W->GetTitleTime() < 0.5f)
		{
			DrawCrosshair(C);
		}
		DrawContextCues(C); // v4.9 : aucune jauge en exploration
		DrawQuickBar(C);
		DrawObjectiveTracker(W);
	}
	if (PC && PC->IsDevMode() && !bInv)
	{
		DrawDevOverlay(PC, C, W);
	}
	if (C && C->IsReadingNote() && !bInv)
	{
		DrawNote(C);
	}
	if (C && C->IsDead())
	{
		DrawDeath(C);
	}
	if (C && C->GetScareKind() >= 0)
	{
		DrawJumpscare(C);
	}
	if (bInv)
	{
		DrawInventory(PC, C, W);
	}
	DrawMessages(Dt);
	if (PC && PC->GetActiveSave() && PC->GetTimeSinceSave() < 3.f && !bInv)
	{
		DrawSaveIndicator(PC->GetTimeSinceSave());
	}
	if (W)
	{
		if (W->GetGlitch() > 0.01f)
		{
			DrawGlitch(W->GetGlitch());
		}
		if (W->GetFade() > 0.001f)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, W->GetFade()), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		}
		// v4.8 : ecran noir tenu le temps de preparer les shaders (une fois par modele et par session)
		if (W->GetShaderHold() > 0.6f)
		{
			const float SA = FMath::Clamp((W->GetShaderHold() - 0.6f) / 0.5f, 0.f, 1.f);
			TextF(BR_STR(NSLOCTEXT("BR", "Loading.Shaders", "Pr\u00e9paration des shaders\u2026")), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.86f,
				FLinearColor(0.9f, 0.85f, 0.6f, 0.75f * SA), 12.f, EUiWeight::Regular, EUiAlign::Center);
		}
	}
	if (PC && PC->IsPauseMenuOpen() && !bInv)
	{
		DrawPause(PC);
	}
	DrawVideoConfirm(PC);
}

// =====================================================================================================================
// Menu titre (v4.0) : logo, cartes animees, carrousel des niveaux, astuces
// =====================================================================================================================

void ABRHUD::DrawContentWarning(float Y)
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	if (!A->HasContent())
	{
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.TexturesIndisponiblesOuvrezProjetEditeur", "Textures indisponibles : ouvrez le projet dans l'\u00e9diteur (plugin Python actif) pour importer les ressources.")), CX, Y,
			Danger, 11.5f, EUiWeight::Regular, EUiAlign::Center);
		Y += 20.f * U;
	}
	else if (A->IsUsingRuntimeContent())
	{
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.ModeSecoursTexturesLuesRawassets", "Mode secours : textures lues dans RawAssets/. Relancez l'import (Output Log > Python : import backrooms_setup; backrooms_setup.run(True))")),
			CX, Y, FLinearColor(1.f, 0.8f, 0.35f, 0.85f), 11.f, EUiWeight::Regular, EUiAlign::Center);
		Y += 20.f * U;
	}
	if (!A->Sound(TEXT("S_Hum")))
	{
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.SonsNonImportesJeuSera", "Sons non import\u00e9s : le jeu sera silencieux tant que l'import Python n'aura pas \u00e9t\u00e9 fait.")), CX, Y,
			FLinearColor(1.f, 0.8f, 0.35f, 0.85f), 11.f, EUiWeight::Regular, EUiAlign::Center);
	}
}

void ABRHUD::DrawLogo(float X, float Y, float W, float A, bool bFlicker)
{
	if (A <= 0.003f)
	{
		return;
	}
	UTexture* T = UiTex(TEXT("UI_Logo"));
	const float Aspect = (T && T->GetSurfaceWidth() > 0.f) ? T->GetSurfaceHeight() / T->GetSurfaceWidth() : 413.f / 1809.f;
	const float H = W * Aspect;
	float F = 1.f;
	if (bFlicker)
	{
		// Toutes les 6,5 s, le neon du logo gresille une fraction de seconde
		const float Ph = FMath::Fmod(Clock, 6.5f);
		if (Ph > 5.85f && FMath::Sin(Clock * 57.f) > -0.2f)
		{
			F = 0.45f + 0.25f * FMath::Abs(FMath::Sin(Clock * 23.f));
		}
	}
	Glow(X + W * 0.5f, Y + H * 0.55f, W * 0.66f, H * 1.35f, FLinearColor(1.f, 0.76f, 0.22f, 0.12f * A * F));
	if (T)
	{
		DrawTexture(T, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.f, 1.f, 1.f, A * (0.35f + 0.65f * F)), BLEND_Translucent);
	}
	else
	{
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.TheBackrooms", "THE BACKROOMS")), X + W * 0.03f, Y + H * 0.2f, WithAlpha(Yellow, A * F), W / Ui() * 0.075f, EUiWeight::Black);
	}
}

void ABRHUD::DrawMenuBackdrop(bool bCentered, float A)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float U = Ui();
	// Le niveau reste visible derriere le menu (camera qui derive, flou de profondeur) : on l'assombrit vers le texte
	DrawRect(FLinearColor(0.02f, 0.017f, 0.008f, (bCentered ? 0.5f : 0.2f) * A), 0.f, 0.f, W, H);
	if (!bCentered)
	{
		Gradient(0.f, 0.f, W * 0.68f, H, FLinearColor(0.012f, 0.01f, 0.004f, 0.93f * A), 0);
	}
	Gradient(0.f, H * 0.55f, W, H * 0.45f, FLinearColor(0.008f, 0.007f, 0.003f, 0.85f * A), 3);
	Gradient(0.f, 0.f, W, H * 0.24f, FLinearColor(0.f, 0.f, 0.f, 0.5f * A), 2);
	// Lumiere chaude des neons, en haut a droite
	Glow(W * 0.8f, H * 0.12f, W * 0.42f, H * 0.5f, FLinearColor(1.f, 0.8f, 0.35f, 0.05f * A));
	if (FBRSettings::Get().bVHSEffect)
	{
		Scanlines(0.022f * A);
		// Bande de "tracking" qui descend l'ecran de temps en temps
		const float Cycle = FMath::Fmod(Clock, 9.f);
		if (Cycle < 0.8f)
		{
			const float BY = H * (Cycle / 0.8f);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.035f * A), 0.f, BY, W, FMath::Max(1.f, 2.f * U));
			DrawRect(FLinearColor(1.f, 0.95f, 0.85f, 0.018f * A), 0.f, BY + 5.f * U, W, 16.f * U);
		}
	}
}

void ABRHUD::DrawCard(float X, float Y, float W, float H, float Sel, const FString& Label, const FString& Sub, const TCHAR* IconName, float Alpha, bool bDanger)
{
	if (Alpha <= 0.003f)
	{
		return;
	}
	const float U = Ui();
	const float S = Smooth(Sel);
	const FLinearColor Accent = bDanger ? FLinearColor(0.93f, 0.33f, 0.25f, 1.f) : Yellow;
	const FLinearColor DarkInk(0.07f, 0.055f, 0.02f, 1.f);
	X += S * 14.f * U;
	const float R = 14.f * U;

	// Halo, ombre portee, fond (jaune quand la carte est selectionnee), lisere, reflet
	Glow(X + W * 0.42f, Y + H * 0.5f, W * 0.75f, H * 1.5f, WithAlpha(Accent, 0.16f * S * Alpha));
	RoundRect(X, Y + 4.f * U, W, H, R, FLinearColor(0.f, 0.f, 0.f, 0.3f * Alpha));
	RoundRect(X, Y, W, H, R, WithAlpha(Mix(FLinearColor(0.05f, 0.045f, 0.03f, 0.78f), FLinearColor(Accent.R, Accent.G, Accent.B, 0.97f), S), Alpha));
	RoundRect(X, Y, W, H, R, FLinearColor(1.f, 0.88f, 0.5f, 0.13f * (1.f - S) * Alpha), true);
	Gradient(X + R, Y + 1.f * U, W - 2.f * R, H * 0.45f, FLinearColor(1.f, 1.f, 1.f, (0.03f + 0.07f * S) * Alpha), 2);

	// Icone dans un cercle
	const float D = H - 30.f * U;
	const float CircX = X + 16.f * U;
	const float CircY = Y + (H - D) * 0.5f;
	RoundRect(CircX, CircY, D, D, D * 0.5f, WithAlpha(Mix(FLinearColor(Accent.R, Accent.G, Accent.B, 0.13f), FLinearColor(0.07f, 0.055f, 0.02f, 0.92f), S), Alpha));
	if (UTexture* T = UiTex(IconName))
	{
		DrawTexture(T, CircX + D * 0.22f, CircY + D * 0.22f, D * 0.56f, D * 0.56f, 0.f, 0.f, 1.f, 1.f, WithAlpha(Accent, Alpha), BLEND_Translucent);
	}

	// Libelle et sous-titre
	const float TX = CircX + D + 20.f * U;
	const FVector2f LS = TextSize(Label, 19.f, EUiWeight::Bold);
	const FVector2f SS = Sub.IsEmpty() ? FVector2f::ZeroVector : TextSize(Sub, 11.5f, EUiWeight::Regular);
	const float Block = LS.Y + (Sub.IsEmpty() ? 0.f : SS.Y - 4.f * U);
	const float TY = Y + (H - Block) * 0.5f;
	TextFit(Label, TX, TY, X + W - TX - 60.f * U, WithAlpha(Mix(Ink, DarkInk, S), Alpha), 19.f, EUiWeight::Bold, EUiAlign::Left, S < 0.5f);
	if (!Sub.IsEmpty())
	{
		TextF(Ellipsize(Sub, X + W - TX - 60.f * U, 11.5f, EUiWeight::Regular), TX, TY + LS.Y - 4.f * U,
			WithAlpha(Mix(InkDim, FLinearColor(0.14f, 0.11f, 0.04f, 0.9f), S), Alpha), 11.5f, EUiWeight::Regular, EUiAlign::Left, false);
	}

	// Chevron (glisse vers la droite a la selection)
	if (UTexture* T = UiTex(TEXT("UI_IconArrow")))
	{
		const float AS = 22.f * U;
		DrawTexture(T, X + W - AS - 22.f * U - (1.f - S) * 8.f * U, Y + (H - AS) * 0.5f, AS, AS, 0.f, 0.f, 1.f, 1.f,
			WithAlpha(Mix(FLinearColor(1.f, 0.9f, 0.6f, 0.22f), DarkInk, S), Alpha), BLEND_Translucent);
	}
}

void ABRHUD::MenuCard(int32 Item, float X, float Y, float W, float H, const FString& Sub, const TCHAR* IconName, bool bInteractive, float Appear)
{
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	const float Ap = FMath::Clamp(Appear, 0.f, 1.f);
	const bool bSel = PC && PC->GetMenuCursor() == Item && Ap > 0.5f;
	float& S = MenuSel[FMath::Clamp(Item, 0, static_cast<int32>(UE_ARRAY_COUNT(MenuSel)) - 1)];
	S = FMath::FInterpTo(S, bSel ? 1.f : 0.f, UiDt, 14.f);
	const bool bDanger = PC && PC->GetMenuPage() == EBRMenuPage::Main && Item == 4; // QUITTER
	DrawCard(X - (1.f - Ap) * 40.f * Ui(), Y, W, H, S, PC ? PC->GetMenuItemLabel(Item) : FString(), Sub, IconName, Ap, bDanger);
	if (bInteractive && Ap > 0.5f)
	{
		AddButton(Btn_Menu + Item, X, Y, W, H);
	}
}

void ABRHUD::MenuPill(int32 Item, float X, float Y, float W, float H, const TCHAR* IconName, bool bPrimary, bool bInteractive, float Alpha, bool bDanger,
	bool bDisabled)
{
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	if (!PC || Alpha <= 0.003f)
	{
		return;
	}
	const float U = Ui();
	float& Sel = MenuSel[FMath::Clamp(Item, 0, static_cast<int32>(UE_ARRAY_COUNT(MenuSel)) - 1)];
	Sel = FMath::FInterpTo(Sel, PC->GetMenuCursor() == Item ? 1.f : 0.f, UiDt, 14.f);
	const float S = Smooth(Sel);
	const FLinearColor DarkInk(0.07f, 0.055f, 0.02f, 1.f);
	// Couleur d'accent : jaune, rouge (suppression) ou gris (bouton inactif : niveau verrouille)
	const FLinearColor Accent = bDisabled ? FLinearColor(0.42f, 0.4f, 0.36f, 1.f) : (bDanger ? FLinearColor(0.9f, 0.3f, 0.24f, 1.f) : Yellow);
	const FLinearColor Fill = bPrimary ? Mix(FLinearColor(Accent.R * 0.86f, Accent.G * 0.83f, Accent.B * 0.75f, 0.92f), FLinearColor(Accent.R, Accent.G, Accent.B, 1.f), S)
									   : Mix(FLinearColor(0.05f, 0.045f, 0.03f, 0.82f), FLinearColor(Accent.R, Accent.G, Accent.B, 0.97f), S);
	const bool bDarkText = (bPrimary || S > 0.5f) && !bDisabled;
	if (!bDisabled)
	{
		Glow(X + W * 0.5f, Y + H * 0.5f, W * 0.8f, H * 1.7f, WithAlpha(Accent, (bPrimary ? 0.08f + 0.12f * S : 0.14f * S) * Alpha));
	}
	RoundRect(X, Y + 4.f * U, W, H, H * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.3f * Alpha));
	RoundRect(X, Y, W, H, H * 0.5f, WithAlpha(Fill, Alpha));
	Gradient(X + H * 0.5f, Y + 1.f * U, W - H, H * 0.45f, FLinearColor(1.f, 1.f, 1.f, 0.08f * Alpha), 2);
	if (bPrimary)
	{
		RoundRect(X - 4.f * U, Y - 4.f * U, W + 8.f * U, H + 8.f * U, H * 0.5f + 4.f * U, FLinearColor(Accent.R, Accent.G, Accent.B, 0.6f * S * Alpha), true);
	}
	else
	{
		RoundRect(X, Y, W, H, H * 0.5f, FLinearColor(1.f, 0.88f, 0.5f, 0.2f * (1.f - S) * Alpha), true);
	}
	const FString Label = PC->GetMenuItemLabel(Item);
	const float IS = 18.f * U;
	// v4.8 : libelle ajuste a la largeur du bouton (traductions plus longues)
	const float LabelSize = FitSize(Label, W - IS - 12.f * U - 32.f * U, 14.5f, EUiWeight::Bold);
	const FVector2f LS = TextSize(Label, LabelSize, EUiWeight::Bold);
	const float Total = IS + 12.f * U + LS.X;
	const float TX = X + (W - Total) * 0.5f;
	const FLinearColor TextC = bDisabled ? FLinearColor(0.85f, 0.82f, 0.75f, 0.8f) : (bDarkText ? DarkInk : Ink);
	if (UTexture* T = UiTex(IconName))
	{
		DrawTexture(T, TX, Y + (H - IS) * 0.5f, IS, IS, 0.f, 0.f, 1.f, 1.f, WithAlpha(bDisabled ? TextC : (bDarkText ? DarkInk : Accent), Alpha), BLEND_Translucent);
	}
	TextF(Ellipsize(Label, W - IS - 12.f * U - 32.f * U, LabelSize, EUiWeight::Bold), TX + IS + 12.f * U, Y + (H - LS.Y) * 0.5f, WithAlpha(TextC, Alpha), LabelSize, EUiWeight::Bold,
		EUiAlign::Left, false);
	if (bInteractive)
	{
		AddButton(Btn_Menu + Item, X, Y, W, H);
	}
}

void ABRHUD::DrawTips(float X, float Y, float W, float A)
{
	if (A <= 0.003f)
	{
		return;
	}
	const float U = Ui();
	const int32 N = MenuTips().Num();
	const float Period = 7.5f;
	const int32 Index = FMath::FloorToInt(TipClock / Period) % N;
	const float Ph = FMath::Fmod(TipClock, Period);
	const float TA = A * FMath::Clamp(Ph / 0.45f, 0.f, 1.f) * FMath::Clamp((Period - Ph) / 0.45f, 0.f, 1.f);
	const float H = 100.f * U;
	RoundRect(X, Y, W, H, 14.f * U, FLinearColor(0.035f, 0.03f, 0.018f, 0.62f * A));
	RoundRect(X, Y, W, H, 14.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.1f * A), true);
	if (UTexture* T = UiTex(TEXT("UI_IconTip")))
	{
		DrawTexture(T, X + 20.f * U, Y + 16.f * U, 22.f * U, 22.f * U, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, 0.95f * A), BLEND_Translucent);
	}
	TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.Astuce", "ASTUCE")), X + 52.f * U, Y + 18.f * U, WithAlpha(Yellow, 0.9f * A), 10.f, EUiWeight::Bold, 3.f * U);
	TextF(FString::Printf(TEXT("%d / %d"), Index + 1, N), X + W - 20.f * U, Y + 18.f * U, WithAlpha(InkDim, 0.6f * A), 10.f, EUiWeight::Regular,
		EUiAlign::Right, false);
	const TArray<FString> Lines = WrapF(BRKeys::Expand(MenuTips()[Index].ToString()), W - 72.f * U, 13.f, EUiWeight::Regular);
	DrawParagraph(TArray<FString>(Lines.GetData(), FMath::Min(Lines.Num(), 2)), X + 52.f * U, Y + 44.f * U, W - 72.f * U, 21.f * U, WithAlpha(Ink, 0.92f * TA), 13.f,
		EUiWeight::Regular);
	// Temps restant avant l'astuce suivante
	DrawRect(FLinearColor(1.f, 0.82f, 0.22f, 0.3f * A), X + 20.f * U, Y + H - 9.f * U, (W - 40.f * U) * (Ph / Period), FMath::Max(1.f, 2.f * U));
}

void ABRHUD::DrawMenuFooter(ABRPlayerController* PC, float A)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.V45InspireBackroomsWiki", "v4.5   \u00b7   Inspir\u00e9 du Backrooms Wiki (CC BY-SA 3.0)   \u00b7   \u00a9 1992 THRESHOLD SYSTEMS")), W - 100.f * U, H - 34.f * U,
		WithAlpha(InkDim, 0.55f * A), 9.5f, EUiWeight::Light, EUiAlign::Right, false);
	// Message de connexion / d'erreur reseau : pastille en haut au centre
	if (!PC->GetMenuStatus().IsEmpty())
	{
		const FString& St = PC->GetMenuStatus();
		const FVector2f SS = TextSize(St, 13.f, EUiWeight::Regular);
		const float PH = 40.f * U;
		const float PW = SS.X + 60.f * U;
		const float PX = (W - PW) * 0.5f;
		const float PY = 40.f * U;
		RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(0.05f, 0.04f, 0.02f, 0.88f * A));
		RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(1.f, 0.7f, 0.35f, 0.35f * A), true);
		const float D = 10.f * U;
		RoundRect(PX + 20.f * U, PY + (PH - D) * 0.5f, D, D, D * 0.5f, FLinearColor(1.f, 0.7f, 0.35f, (0.6f + 0.4f * FMath::Sin(Clock * 5.f)) * A));
		TextF(St, PX + 40.f * U, PY + (PH - SS.Y) * 0.5f, FLinearColor(1.f, 0.88f, 0.7f, A), 13.f, EUiWeight::Regular, EUiAlign::Left, false);
	}
	DrawContentWarning(10.f * U);
}

void ABRHUD::DrawMenu()
{
	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	if (!PC)
	{
		return;
	}
	const bool bInteractive = !PC->IsInventoryOpen();
	PlayerOwner->GetMousePosition(MouseX, MouseY);
	Buttons.Reset();

	MenuIntro += UiDt;
	TipClock += UiDt;
	const int32 Page = static_cast<int32>(PC->GetMenuPage());
	if (Page != LastMenuPage)
	{
		LastMenuPage = Page;
		MenuPageTime = 0.f;
		for (float& S : MenuSel)
		{
			S = 0.f;
		}
	}
	MenuPageTime += UiDt;

	const float A = EaseOut(MenuIntro / 1.4f);
	const EBRMenuPage P = PC->GetMenuPage();
	DrawMenuBackdrop(P == EBRMenuPage::Solo || P == EBRMenuPage::Join || P == EBRMenuPage::NewSave, A);
	switch (P)
	{
	case EBRMenuPage::Main:
		DrawMenuMain(PC, bInteractive);
		break;
	case EBRMenuPage::Solo:
		DrawMenuSolo(PC, bInteractive);
		break;
	case EBRMenuPage::Multi:
		DrawMenuMulti(PC, bInteractive);
		break;
	case EBRMenuPage::Join:
		DrawMenuJoin(PC, bInteractive);
		break;
	case EBRMenuPage::Saves:
		DrawMenuSaves(PC, bInteractive);
		break;
	case EBRMenuPage::NewSave:
		DrawMenuNewSave(PC, bInteractive);
		break;
	case EBRMenuPage::Language:
		DrawMenuLanguage(PC, bInteractive);
		break;
	}
	DrawMenuFooter(PC, A);
	if (bInteractive)
	{
		HandleMenuMouse(PC);
	}
	// Ouverture du jeu : fondu depuis le noir
	if (MenuIntro < 1.6f)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f - EaseOut(MenuIntro / 1.6f)), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}
}

void ABRHUD::DrawMenuMain(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float X0 = FMath::Max(60.f * U, Canvas->ClipX * 0.0625f);
	const float In = EaseOut((MenuIntro - 0.2f) / 1.f);

	// Logo (les lettres commencent a 3 % de l'image) et accroche
	const float LogoW = 660.f * U;
	const float LogoY = 92.f * U;
	DrawLogo(X0 - LogoW * 0.03f - (1.f - In) * 50.f * U, LogoY, LogoW, In, true);
	const float TY = LogoY + LogoW * 413.f / 1809.f * 0.86f + 18.f * U;
	DrawRect(WithAlpha(Yellow, In), X0, TY + 11.f * U, 30.f * U, FMath::Max(1.f, 3.f * U));
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.NoclippezHorsRealiteMauvaisEndroit", "Si vous noclippez hors de la r\u00e9alit\u00e9 au mauvais endroit\u2026")), X0 + 44.f * U, TY, WithAlpha(Ink, 0.88f * In), 14.5f, EUiWeight::Light);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.AtterrissezBackrooms", "\u2026vous atterrissez dans les Backrooms.")), X0 + 44.f * U, TY + 25.f * U, WithAlpha(Yellow, 0.95f * In), 14.5f, EUiWeight::Regular);

	// Cartes : SOLO / MULTIJOUEUR / PARAMETRES / LANGUE / QUITTER (entree en cascade)
	FString SoloSub = BR_STR(NSLOCTEXT("BR", "HUD.NouvellePartiePartirSeulInconnu", "Nouvelle partie : partir seul dans l'inconnu"));
	if (PC->GetSaveOrder().Num() > 0)
	{
		if (const UBRSaveGame* Last = PC->GetSaveInSlot(PC->GetSaveOrder()[0]))
		{
			SoloSub = BRLoc::Fmt(NSLOCTEXT("BR", "HUD.ReprendreSavenameNiveauCurrentlevelNouve", "Reprendre \u00ab {SaveName} \u00bb (Niveau {CurrentLevel}) ou nouvelle partie"), { { TEXT("SaveName"), BRLoc::Arg(Last->SaveName) }, { TEXT("CurrentLevel"), BRLoc::Int(Last->CurrentLevel) } });
		}
	}
	// Sous-titre de LANGUE : la langue courante, dans sa propre ecriture
	const FString LanguageSub = BRLoc::Fmt(NSLOCTEXT("BR", "HUD.LanguageCurrent", "Actuelle : {Language}  \u00b7  22 langues"),
		{ { TEXT("Language"), BRLoc::Arg(BRLoc::Current().NativeName) } });
	const FString Subs[] = {
		SoloSub,
		BR_STR(NSLOCTEXT("BR", "HUD.Jusqu4ExplorateursMeilleurPc", "Jusqu'\u00e0 4 explorateurs \u00b7 le meilleur PC h\u00e9berge")),
		BR_STR(NSLOCTEXT("BR", "HUD.GraphismesTouchesChatVocal", "Graphismes, son, touches, chat vocal")),
		LanguageSub,
		BR_STR(NSLOCTEXT("BR", "HUD.RevenirRealiteExiste", "Revenir \u00e0 la r\u00e9alit\u00e9\u2026 si elle existe")),
	};
	const TCHAR* Icons[] = { TEXT("UI_IconSolo"), TEXT("UI_IconMulti"), TEXT("UI_IconSettings"), TEXT("UI_IconLanguage"), TEXT("UI_IconQuit") };
	const float CW = 540.f * U;
	const float CH = 78.f * U;
	const float Gap = 12.f * U;
	float Y = 336.f * U;
	const float Base = FMath::Min(MenuPageTime, MenuIntro - 0.45f);
	for (int32 i = 0; i < PC->GetMenuItemCount() && i < 5; ++i)
	{
		MenuCard(i, X0, Y, CW, CH, Subs[i], Icons[i], bInteractive, EaseOut((Base - 0.07f * i) / 0.45f));
		Y += CH + Gap;
	}
	DrawTips(X0, Y + 12.f * U, CW, EaseOut((Base - 0.4f) / 0.5f));

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("\u2191 \u2193"), BR_STR(NSLOCTEXT("BR", "HUD.Choisir", "Choisir")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Entree", "ENTR\u00c9E")), BR_STR(NSLOCTEXT("BR", "HUD.Valider", "Valider")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Fin", "FIN")), BR_STR(NSLOCTEXT("BR", "HUD.Quitter", "Quitter")));
	KeyHints(X0, Canvas->ClipY - 72.f * U, Hints, In, false);
}

void ABRHUD::DrawMenuLanguage(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const float In = EaseOut(MenuPageTime / 0.45f);
	const TArray<BRLoc::FLanguage>& Langs = BRLoc::Languages();
	const int32 Current = BRLoc::CurrentIndex();
	const int32 Num = Langs.Num();
	static const FString SystemCode = BRLoc::DetectSystemLanguage();
	const FLinearColor DarkInk(0.07f, 0.055f, 0.02f, 1.f);

	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	const FString Title = BR_STR(NSLOCTEXT("BR", "HUD.LanguageTitle", "LANGUE"));
	TextF(Title, X0, 124.f * U, WithAlpha(Ink, In), 32.f, EUiWeight::Black);
	// Le mot anglais a cote du titre : la page se retrouve meme dans une langue qu'on ne lit pas
	if (Current == INDEX_NONE || FString(Langs[Current].Code) != TEXT("en"))
	{
		TextF(TEXT("Language"), X0 + TextSize(Title, 32.f, EUiWeight::Black).X + 18.f * U, 140.f * U, WithAlpha(InkDim, 0.7f * In), 15.f, EUiWeight::Light);
	}
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.LanguageSubtitle", "Textes, nombres et dates  \u00b7  s'applique tout de suite  \u00b7  en coop, chacun garde la sienne")), X0, 178.f * U,
		WithAlpha(InkDim, In), 13.f, EUiWeight::Light);

	// Deux colonnes de langues (noms natifs, jamais traduits), la langue actuelle cochee
	const int32 Half = (Num + 1) / 2;
	const float CW = 330.f * U;
	const float RH = 46.f * U;
	const float RG = 8.f * U;
	const float Y0 = 228.f * U;
	const int32 LastSel = static_cast<int32>(UE_ARRAY_COUNT(MenuSel)) - 1;
	for (int32 i = 0; i < Num; ++i)
	{
		const BRLoc::FLanguage& L = Langs[i];
		const int32 Col = i / Half;
		const int32 Row = i % Half;
		const float Ap = EaseOut((MenuPageTime - 0.025f * Row - 0.06f * Col) / 0.4f);
		if (Ap <= 0.003f)
		{
			continue;
		}
		float& Sel = MenuSel[FMath::Clamp(i, 0, LastSel)];
		Sel = FMath::FInterpTo(Sel, PC->GetMenuCursor() == i ? 1.f : 0.f, UiDt, 14.f);
		const float S = Smooth(Sel);
		const bool bCur = i == Current;
		const float RX = X0 + Col * (CW + 16.f * U) - (1.f - Ap) * 30.f * U + S * 8.f * U;
		const float RY = Y0 + Row * (RH + RG);
		RoundRect(RX, RY + 3.f * U, CW, RH, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.25f * Ap));
		RoundRect(RX, RY, CW, RH, 12.f * U, WithAlpha(Mix(FLinearColor(0.05f, 0.045f, 0.03f, 0.78f), FLinearColor(Yellow.R, Yellow.G, Yellow.B, 0.97f), S), Ap));
		RoundRect(RX, RY, CW, RH, 12.f * U, bCur ? WithAlpha(Yellow, (0.7f - 0.5f * S) * Ap) : FLinearColor(1.f, 0.88f, 0.5f, 0.12f * (1.f - S) * Ap), true);
		// Code de culture (pastille), nom natif, langue du systeme, coche
		const FString Code = FString(L.Code).ToUpper();
		const float BW = 70.f * U;
		const float BH = 24.f * U;
		RoundRect(RX + 12.f * U, RY + (RH - BH) * 0.5f, BW, BH, BH * 0.5f, WithAlpha(Mix(FLinearColor(1.f, 0.88f, 0.5f, 0.1f), FLinearColor(0.07f, 0.055f, 0.02f, 0.18f), S), Ap));
		const FVector2f CS = TextSize(Code, 9.5f, EUiWeight::Bold);
		TextF(Code, RX + 12.f * U + BW * 0.5f, RY + (RH - CS.Y) * 0.5f, WithAlpha(Mix(InkDim, DarkInk, S), Ap), 9.5f, EUiWeight::Bold, EUiAlign::Center, false);
		const EUiWeight NW = bCur ? EUiWeight::Bold : EUiWeight::Regular;
		const FVector2f NS = TextSize(L.NativeName, 15.f, NW);
		float RightX = RX + CW - 14.f * U;
		if (bCur)
		{
			const float IS = 20.f * U;
			if (UTexture* T = UiTex(TEXT("UI_IconCheck")))
			{
				DrawTexture(T, RightX - IS, RY + (RH - IS) * 0.5f, IS, IS, 0.f, 0.f, 1.f, 1.f, WithAlpha(S > 0.5f ? DarkInk : Yellow, Ap), BLEND_Translucent);
			}
			RightX -= IS + 10.f * U;
		}
		if (SystemCode == L.Code)
		{
			const FString Tag = BR_STR(NSLOCTEXT("BR", "HUD.LanguageSystem", "SYST\u00c8ME"));
			const FVector2f TS = TextSize(Tag, 8.5f, EUiWeight::Bold);
			TextF(Tag, RightX, RY + (RH - TS.Y) * 0.5f, WithAlpha(Mix(InkDim, DarkInk, S), 0.8f * Ap), 8.5f, EUiWeight::Bold, EUiAlign::Right, false);
			RightX -= TS.X + 10.f * U;
		}
		const float NX = RX + 12.f * U + BW + 14.f * U;
		TextF(Ellipsize(L.NativeName, RightX - NX, 15.f, NW), NX, RY + (RH - NS.Y) * 0.5f, WithAlpha(Mix(Ink, DarkInk, S), Ap), 15.f, NW, EUiAlign::Left, S < 0.5f);
		if (bInteractive && Ap > 0.5f)
		{
			AddButton(Btn_Language + i, RX, RY, CW, RH);
		}
	}

	// RETOUR
	const float BY = Y0 + Half * (RH + RG) + 10.f * U;
	const float BackW = 220.f * U;
	const float BackH = 52.f * U;
	const float BackA = EaseOut((MenuPageTime - 0.3f) / 0.4f);
	MenuPill(Num, X0, BY, BackW, BackH, TEXT("UI_IconBack"), false, false, BackA);
	if (bInteractive && BackA > 0.5f)
	{
		AddButton(Btn_Language + Num, X0, BY, BackW, BackH);
	}

	// Colonne de droite : etat honnete de la langue actuelle (couverture, relecture, voix)
	const float PX = FMath::Max(X0 + 2.f * CW + 72.f * U, W - 56.f * U - 600.f * U);
	const float PW = FMath::Max(300.f * U, FMath::Min(600.f * U, W - PX - 56.f * U));
	const float A = EaseOut((MenuPageTime - 0.15f) / 0.5f);
	if (A <= 0.003f || Current == INDEX_NONE)
	{
		return;
	}
	const BRLoc::FLanguage& Cur = Langs[Current];
	const bool bSource = FString(Cur.Code) == TEXT("fr");
	if (CoverageFor != Current)
	{
		// Recense une fois par langue (environ mille recherches dans la table des textes)
		CoverageFor = Current;
		CoverageMissing = BRLoc::CountMissing();
	}
	const int32 Total = BRLoc::KeyCount();
	const int32 Translated = FMath::Max(0, Total - CoverageMissing);
	const bool bReviewed = BRLoc::IsReviewed(Cur.Code);

	TArray<FString> Paras;
	if (bSource)
	{
		Paras.Add(BR_STR(NSLOCTEXT("BR", "HUD.LanguageSourceNote", "Langue d'origine du jeu : tous les textes sont \u00e9crits en fran\u00e7ais, puis traduits.")));
	}
	else if (bReviewed)
	{
		Paras.Add(BR_STR(NSLOCTEXT("BR", "HUD.LanguageReviewedNote", "Traduction relue par une personne qui parle cette langue.")));
	}
	else
	{
		Paras.Add(BR_STR(NSLOCTEXT("BR", "HUD.LanguageMachineNote", "Traduction produite automatiquement, pas encore relue par une personne qui parle cette langue : des tournures peuvent sonner faux.")));
	}
	if (!bSource && CoverageMissing > 0)
	{
		Paras.Add(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.LanguageMissingNote", "{Count}|plural(one=Un texte n'est pas encore traduit : il s'affiche en fran\u00e7ais.,other={Count} textes ne sont pas encore traduits : ils s'affichent en fran\u00e7ais.)"),
			{ { TEXT("Count"), BRLoc::Int(CoverageMissing) } }));
	}
	Paras.Add(BR_STR(NSLOCTEXT("BR", "HUD.LanguageAudioNote", "Voix et sons : pas de doublage. Les bruits et les voix sont les m\u00eames dans toutes les langues ; seuls les textes \u00e0 l'\u00e9cran changent.")));
	static const int32 MissingFonts = BRFonts::MissingFiles().Num();
	if (MissingFonts > 0)
	{
		Paras.Add(BR_STR(NSLOCTEXT("BR", "HUD.LanguageFontsMissing", "Des polices manquent dans le jeu install\u00e9 (dossier Content/Fonts) : le chinois, le japonais, le cor\u00e9en, l'arabe ou le persan peuvent s'afficher en carr\u00e9s.")));
	}
	if (Cur.bRightToLeft)
	{
		Paras.Add(BR_STR(NSLOCTEXT("BR", "HUD.LanguageRtlNote", "\u00c9criture de droite \u00e0 gauche : les paragraphes sont align\u00e9s \u00e0 droite.")));
	}
	TArray<TArray<FString>> Wrapped;
	float BodyH = 0.f;
	for (const FString& P : Paras)
	{
		Wrapped.Add(WrapF(P, PW - 48.f * U, 12.5f, EUiWeight::Regular));
		BodyH += Wrapped.Last().Num() * 20.f * U + 10.f * U;
	}
	const float BoxH = 150.f * U + BodyH;
	float PY = Y0;
	RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(0.035f, 0.03f, 0.018f, 0.72f * A));
	RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.14f * A), true);
	TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.LanguageCurrentHeader", "LANGUE ACTUELLE")), PX + 24.f * U, PY + 20.f * U, WithAlpha(Yellow, A), 10.f, EUiWeight::Bold, 3.f * U);
	TextF(Cur.NativeName, PX + 24.f * U, PY + 42.f * U, WithAlpha(Ink, A), 24.f, EUiWeight::Bold);
	// Couverture : textes traduits / textes du jeu
	const FString Cov = bSource ? BR_STR(NSLOCTEXT("BR", "HUD.LanguageSourceCoverage", "Langue source"))
		: BRLoc::Fmt(NSLOCTEXT("BR", "HUD.LanguageCoverage", "{Done} / {Total} textes traduits"), { { TEXT("Done"), BRLoc::Int(Translated) }, { TEXT("Total"), BRLoc::Int(Total) } });
	TextF(Cov, PX + 24.f * U, PY + 86.f * U, WithAlpha(InkDim, A), 12.f, EUiWeight::Regular);
	const float BarW = PW - 48.f * U;
	const float Frac = bSource || Total <= 0 ? 1.f : static_cast<float>(Translated) / static_cast<float>(Total);
	RoundRect(PX + 24.f * U, PY + 112.f * U, BarW, 6.f * U, 3.f * U, FLinearColor(1.f, 1.f, 1.f, 0.1f * A));
	RoundRect(PX + 24.f * U, PY + 112.f * U, FMath::Max(6.f * U, BarW * Frac), 6.f * U, 3.f * U, WithAlpha(bReviewed ? Done : Yellow, A));
	float LY = PY + 134.f * U;
	for (const TArray<FString>& Lines : Wrapped)
	{
		DrawParagraph(Lines, PX + 24.f * U, LY, PW - 48.f * U, 20.f * U, WithAlpha(Ink, 0.88f * A), 12.5f, EUiWeight::Regular);
		LY += Lines.Num() * 20.f * U + 10.f * U;
	}
	// Langue sous le curseur : ce qu'ENTREE fera
	const int32 Cursor = PC->GetMenuCursor();
	if (Langs.IsValidIndex(Cursor) && Cursor != Current)
	{
		TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.LanguageSwitchTo", "ENTR\u00c9E : passer en {Language}"), { { TEXT("Language"), BRLoc::Arg(Langs[Cursor].NativeName) } }),
			PX + 24.f * U, PY + BoxH + 18.f * U, WithAlpha(Yellow, 0.9f * A), 12.5f, EUiWeight::Bold);
	}

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("\u2191 \u2193 \u2190 \u2192"), BR_STR(NSLOCTEXT("BR", "HUD.Choisir", "Choisir")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Entree", "ENTR\u00c9E")), BR_STR(NSLOCTEXT("BR", "HUD.Valider", "Valider")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Echap", "\u00c9CHAP")), BR_STR(NSLOCTEXT("BR", "HUD.Retour", "Retour")));
	KeyHints(X0, H - 72.f * U, Hints, In, false);
}

void ABRHUD::DrawLevelScene(const FBRLevelDef& D, float X, float Y, float W, float H, float Scale, float Alpha)
{
	// Apercu "en coupe" du niveau avec ses vraies textures : plafond et ses lampes, mur, plinthe, sol, eau, brouillard
	UBRAssets* A = UBRAssets::Get(this);
	const float U = Ui();
	const float Tile = 92.f * U * Scale;
	const bool bOpen = D.bOutdoor || !D.bCeiling;
	const bool bDark = D.Fixture == EBRFixture::None && !bOpen;
	const float Lum = bDark ? 0.16f : 1.f;
	const FLinearColor Light(D.LightColor.R, D.LightColor.G, D.LightColor.B, 1.f);
	auto Surface = [&](const FBRSurface& Surf, float SX, float SY, float SW, float SH, float Rep, float Bright)
	{
		if (SW < 1.f || SH < 1.f)
		{
			return;
		}
		FLinearColor C = Surf.Tint * Light * (Bright * Lum);
		C.A = Alpha;
		UTexture* T = (A && !Surf.Texture.IsNone()) ? A->Texture(Surf.Texture) : nullptr;
		if (T)
		{
			DrawTexture(T, SX, SY, SW, SH, 0.f, 0.f, SW / Rep, SH / Rep, C, BLEND_Translucent);
		}
		else
		{
			DrawRect(FLinearColor(C.R * 0.5f, C.G * 0.48f, C.B * 0.4f, Alpha), SX, SY, SW, SH);
		}
	};

	DrawRect(FLinearColor(0.015f, 0.015f, 0.015f, Alpha), X, Y, W, H);
	float WallTop = Y;
	float WallBot = Y + H * 0.74f;
	if (bOpen)
	{
		FLinearColor SkyTop(0.f, 0.f, 0.f, 1.f);
		FLinearColor SkyLow(0.03f, 0.03f, 0.03f, 1.f);
		switch (D.Sky)
		{
		case EBRSky::Night:
			SkyTop = FLinearColor(0.005f, 0.01f, 0.03f, 1.f);
			SkyLow = FLinearColor(0.05f, 0.06f, 0.11f, 1.f);
			break;
		case EBRSky::Overcast:
			SkyTop = FLinearColor(0.38f, 0.41f, 0.46f, 1.f);
			SkyLow = FLinearColor(0.7f, 0.72f, 0.74f, 1.f);
			break;
		case EBRSky::Day:
			SkyTop = FLinearColor(0.22f, 0.42f, 0.78f, 1.f);
			SkyLow = FLinearColor(0.72f, 0.8f, 0.9f, 1.f);
			break;
		default:
			break;
		}
		DrawRect(WithAlpha(SkyLow, Alpha), X, Y, W, H * 0.62f);
		Gradient(X, Y, W, H * 0.62f, WithAlpha(SkyTop, Alpha), 2);
		WallTop = Y + H * 0.36f;
		WallBot = Y + H * 0.62f;
	}
	else
	{
		const float CeilH = H * 0.16f;
		Surface(D.Ceiling, X, Y, W, CeilH, Tile, 0.72f);
		WallTop = Y + CeilH;
	}
	if (!D.Wall.Texture.IsNone())
	{
		Surface(D.Wall, X, WallTop, W, WallBot - WallTop, Tile, 0.95f);
	}
	if (D.bTrim && !bOpen)
	{
		DrawRect(FLinearColor(D.Trim.Tint.R * 0.3f * Lum, D.Trim.Tint.G * 0.28f * Lum, D.Trim.Tint.B * 0.24f * Lum, Alpha), X, WallBot - 4.f * U * Scale, W, 4.f * U * Scale);
	}
	Surface(D.Floor, X, WallBot, W, Y + H - WallBot, Tile * 0.8f, 0.7f);
	if (D.bWater)
	{
		const float WY = WallBot - H * 0.07f;
		DrawRect(FLinearColor(0.3f, 0.68f, 0.7f, 0.5f * Alpha), X, WY, W, Y + H - WY);
		DrawRect(FLinearColor(0.85f, 1.f, 1.f, 0.4f * Alpha), X, WY, W, FMath::Max(1.f, 2.f * U * Scale));
		Gradient(X, WY, W, (Y + H - WY) * 0.6f, FLinearColor(0.75f, 0.95f, 1.f, 0.18f * Alpha), 2);
	}
	// v4.1 : sol du parking (places peintes) et flaques qui refletent les lampes
	const FBRSurface& Ground = D.Road.Puddles > D.Floor.Puddles ? D.Road : D.Floor;
	if (D.bGarage)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float LX = X + W * (0.12f + 0.25f * i);
			DrawLine(LX, Y + H, LX + W * 0.07f, WallBot + 2.f * U * Scale, FLinearColor(0.9f, 0.9f, 0.85f, 0.55f * Alpha), FMath::Max(1.f, 1.5f * U * Scale));
		}
	}
	if (Ground.Puddles > 0.f)
	{
		const float FloorH = Y + H - WallBot;
		for (int32 i = 0; i < 3; ++i)
		{
			const float PX = X + W * (0.2f + 0.3f * i);
			const float PY = WallBot + FloorH * (0.35f + 0.2f * (i % 2));
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.25f * Alpha), PX - W * 0.11f, PY - FloorH * 0.06f, W * 0.22f, FloorH * 0.12f);
			Glow(PX, PY, W * 0.1f, FloorH * 0.09f, WithAlpha(Light, 0.45f * Alpha));
		}
	}
	// Lampes : plafonniers (rangee de trois) ou lampadaires a l'horizon
	if (D.Fixture != EBRFixture::None)
	{
		if (!bOpen)
		{
			const float CeilH = WallTop - Y;
			for (int32 i = 0; i < 3; ++i)
			{
				const float LX = X + W * (0.2f + 0.3f * i);
				Glow(LX, WallTop, W * 0.26f, H * 0.42f, WithAlpha(Light, 0.26f * Alpha));
				DrawRect(WithAlpha(Mix(Light, FLinearColor::White, 0.6f), Alpha), LX - W * 0.07f, Y + CeilH * 0.38f, W * 0.14f, FMath::Max(2.f, 5.f * U * Scale));
			}
		}
		else
		{
			for (int32 i = 0; i < 2; ++i)
			{
				const float LX = X + W * (0.28f + 0.46f * i);
				DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, Alpha), LX - 1.5f * U * Scale, WallTop - H * 0.1f, 3.f * U * Scale, WallBot - WallTop + H * 0.1f);
				Glow(LX, WallTop - H * 0.1f, W * 0.18f, H * 0.2f, WithAlpha(Light, 0.55f * Alpha));
			}
		}
	}
	// Brouillard du niveau puis vignettage
	DrawRect(FLinearColor(D.FogColor.R, D.FogColor.G, D.FogColor.B, FMath::Clamp(D.FogDensity * 2.2f, 0.04f, 0.32f) * Alpha), X, Y, W, H);
	Gradient(X, Y, W * 0.3f, H, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), 0);
	Gradient(X + W * 0.7f, Y, W * 0.3f, H, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), 1);
	Gradient(X, Y + H * 0.55f, W, H * 0.45f, FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), 3);
	Frame(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, 0.06f * Alpha), FMath::Max(1.f, U));
}

void ABRHUD::DrawLevelCard(int32 Index, float CX, float Top, float Scale, float Alpha, float Sel, bool bLocked, bool bCurrent)
{
	const TArray<FBRLevelDef>& All = BRLevels::All();
	if (!All.IsValidIndex(Index) || Alpha <= 0.003f)
	{
		return;
	}
	const FBRLevelDef& D = All[Index];
	const float U = Ui();
	const float S = Scale;
	const float W = 294.f * U * S;
	const float H = 392.f * U * S;
	const float X = CX - W * 0.5f;
	const float Y = Top + (392.f * U - H) * 0.5f;
	const float R = 16.f * U * S;
	const FLinearColor Accent = bLocked ? FLinearColor(0.55f, 0.52f, 0.46f, 1.f) : Yellow;

	Glow(CX, Y + H * 0.5f, W * 0.95f, H * 0.75f, FLinearColor(Accent.R, Accent.G * 0.95f, Accent.B, (bLocked ? 0.08f : 0.16f) * Sel * Alpha));
	RoundRect(X + 4.f * U * S, Y + 10.f * U * S, W, H, R, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha));
	RoundRect(X, Y, W, H, R, bLocked ? FLinearColor(0.035f, 0.033f, 0.03f, 0.97f * Alpha) : FLinearColor(0.06f, 0.052f, 0.035f, 0.97f * Alpha));

	const float Pad = 10.f * U * S;
	const float PW = W - 2.f * Pad;
	const float PH = H * 0.52f;
	const float PX = X + Pad;
	const float PY = Y + Pad;
	if (bLocked)
	{
		// Niveau pas encore explore : ecran neigeux, cadenas
		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, Alpha), PX, PY, PW, PH);
		const float Step = FMath::Max(2.f, 3.f * U * S);
		for (float LY = PY; LY < PY + PH; LY += Step)
		{
			const float N = FMath::Frac(FMath::Sin((LY - PY) * 12.9898f + Index * 78.233f + FMath::FloorToFloat(Clock * 12.f) * 3.7f) * 43758.5453f);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, (0.015f + 0.05f * N) * Alpha), PX, LY, PW, FMath::Max(1.f, U));
		}
		Gradient(PX, PY, PW, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), 2);
		Gradient(PX, PY + PH * 0.5f, PW, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), 3);
		const float LD = 64.f * U * S;
		RoundRect(CX - LD * 0.5f, PY + (PH - LD) * 0.5f, LD, LD, LD * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha));
		RoundRect(CX - LD * 0.5f, PY + (PH - LD) * 0.5f, LD, LD, LD * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.18f * Alpha), true);
		if (UTexture* T = UiTex(TEXT("UI_IconLock")))
		{
			DrawTexture(T, CX - LD * 0.25f, PY + (PH - LD) * 0.5f + LD * 0.22f, LD * 0.5f, LD * 0.5f, 0.f, 0.f, 1.f, 1.f, FLinearColor(0.85f, 0.82f, 0.75f, 0.85f * Alpha),
				BLEND_Translucent);
		}
		Frame(PX, PY, PW, PH, FLinearColor(1.f, 1.f, 1.f, 0.06f * Alpha), FMath::Max(1.f, U));
	}
	else
	{
		DrawLevelScene(D, PX, PY, PW, PH, S, Alpha);
	}
	if (bCurrent)
	{
		// La ou la partie s'est arretee
		const FString Badge = BR_STR(NSLOCTEXT("BR", "HUD.DernierePosition", "DERNI\u00c8RE POSITION"));
		const FVector2f BS = TextSize(Badge, 8.5f * S, EUiWeight::Bold);
		const float BH = 20.f * U * S;
		const float BW = BS.X + 22.f * U * S;
		RoundRect(CX - BW * 0.5f, PY + 8.f * U * S, BW, BH, BH * 0.5f, WithAlpha(Yellow, 0.95f * Alpha));
		TextF(Badge, CX, PY + 8.f * U * S + (BH - BS.Y) * 0.5f, FLinearColor(0.07f, 0.055f, 0.02f, Alpha), 8.5f * S, EUiWeight::Bold, EUiAlign::Center, false);
	}

	const FLinearColor Dim = bLocked ? FLinearColor(0.55f, 0.53f, 0.48f, 0.8f) : InkDim;
	float TY = PY + PH + 12.f * U * S;
	TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.Niveau", "NIVEAU")), CX, TY, WithAlpha(Dim, Alpha), 9.5f * S, EUiWeight::Light, 4.f * U * S, EUiAlign::Center);
	TY += 15.f * U * S;
	const FLinearColor NumC = bLocked ? FLinearColor(0.5f, 0.48f, 0.43f, 1.f) : Mix(Ink, Yellow, Sel);
	TextF(FString::FromInt(D.Number), CX, TY, WithAlpha(NumC, Alpha), 40.f * S, EUiWeight::Black, EUiAlign::Center);
	TY += TextSize(TEXT("0"), 40.f * S, EUiWeight::Black).Y - 6.f * U * S;
	if (bLocked)
	{
		TextF(TEXT("? ? ?"), CX, TY, WithAlpha(Dim, Alpha), 14.f * S, EUiWeight::Bold, EUiAlign::Center, false);
		TY += 21.f * U * S;
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.NonExplore", "Non explor\u00e9")), CX, TY, WithAlpha(Dim, Alpha), 11.f * S, EUiWeight::Regular, EUiAlign::Center, false);
	}
	else
	{
		TextF(Ellipsize(D.Title.ToString(), PW, 14.f * S, EUiWeight::Bold), CX, TY, WithAlpha(Ink, Alpha), 14.f * S, EUiWeight::Bold, EUiAlign::Center, false);
		TY += 21.f * U * S;
		TextF(Ellipsize(D.Nickname.ToString(), PW, 11.f * S, EUiWeight::Regular), CX, TY, WithAlpha(InkDim, Alpha), 11.f * S, EUiWeight::Regular, EUiAlign::Center, false);
	}

	// Classe de survie (inconnue tant que le niveau n'est pas explore)
	const FString Cls = bLocked ? FString(BR_STR(NSLOCTEXT("BR", "HUD.Verrouille", "VERROUILL\u00c9"))) : ClassShort(D.ClassText.ToString());
	const FLinearColor CC = bLocked ? FLinearColor(0.6f, 0.57f, 0.52f, 1.f) : ClassColor(D.SurvivalClass);
	const FVector2f CS = TextSize(Cls, 9.5f * S, EUiWeight::Bold);
	const float PillH = 22.f * U * S;
	const float PillW = CS.X + 34.f * U * S;
	const float PillY = Y + H - PillH - 14.f * U * S;
	RoundRect(CX - PillW * 0.5f, PillY, PillW, PillH, PillH * 0.5f, FLinearColor(CC.R, CC.G, CC.B, 0.16f * Alpha));
	const float Dot = 7.f * U * S;
	RoundRect(CX - PillW * 0.5f + 11.f * U * S, PillY + (PillH - Dot) * 0.5f, Dot, Dot, Dot * 0.5f, WithAlpha(CC, Alpha));
	TextF(Cls, CX + 6.f * U * S, PillY + (PillH - CS.Y) * 0.5f, WithAlpha(CC, Alpha), 9.5f * S, EUiWeight::Bold, EUiAlign::Center, false);

	if (Sel > 0.01f)
	{
		RoundRect(X - 3.f * U, Y - 3.f * U, W + 6.f * U, H + 6.f * U, R + 3.f * U, WithAlpha(Accent, Sel * Alpha), true);
	}
	else
	{
		RoundRect(X, Y, W, H, R, FLinearColor(1.f, 0.9f, 0.6f, 0.1f * Alpha), true);
	}
}

void ABRHUD::DrawMenuSolo(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const TArray<FBRLevelDef>& All = BRLevels::All();
	const int32 Num = All.Num();
	if (Num == 0)
	{
		return;
	}
	const int32 Sel = FMath::Clamp(PC->GetMenuIndex(), 0, Num - 1);
	const float In = EaseOut(MenuPageTime / 0.45f);
	const UBRSaveGame* Save = PC->GetActiveSave();
	int32 ExploredCount = 0;
	for (const FBRLevelDef& L : All)
	{
		ExploredCount += PC->IsLevelUnlocked(L.Number) ? 1 : 0;
	}

	// En-tete : partie choisie et progression
	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	TextSpaced(PC->IsHostFlow() ? BR_STR(NSLOCTEXT("BR", "HUD.HebergerChoisissezNiveau", "H\u00c9BERGER : CHOISISSEZ UN NIVEAU")) : BR_STR(NSLOCTEXT("BR", "HUD.ChoisissezNiveau", "CHOISISSEZ UN NIVEAU")), CX, 100.f * U, WithAlpha(Yellow, In), 13.f, EUiWeight::Bold,
		5.f * U, EUiAlign::Center);
	const FString Progress = BRLoc::Fmt(NSLOCTEXT("BR", "HUD.SavenameExploredcountValueNiveauxExplore", "{SaveName}  \u00b7  {ExploredCount} / {Value} niveaux explor\u00e9s"), { { TEXT("SaveName"), BRLoc::Arg(Save ? *FString::Printf(TEXT("\u00ab %s \u00bb"), *Save->SaveName) : BR_STR(NSLOCTEXT("BR", "HUD.SansPartie", "Sans partie"))) }, { TEXT("ExploredCount"), BRLoc::Int(ExploredCount) }, { TEXT("Value"), BRLoc::Int(Num) } });
	TextF(Progress, CX, 126.f * U, WithAlpha(Ink, 0.9f * In), 12.5f, EUiWeight::Regular, EUiAlign::Center);
	{
		const float BW = 320.f * U;
		const float BY = 152.f * U;
		RoundRect(CX - BW * 0.5f, BY, BW, 4.f * U, 2.f * U, FLinearColor(1.f, 1.f, 1.f, 0.12f * In));
		RoundRect(CX - BW * 0.5f, BY, FMath::Max(4.f * U, BW * ExploredCount / Num), 4.f * U, 2.f * U, WithAlpha(Yellow, In));
	}

	// Carrousel : sa position suit la selection par le plus court chemin (la liste boucle)
	if (Carousel < -500.f)
	{
		Carousel = static_cast<float>(Sel);
	}
	float Delta = static_cast<float>(Sel) - Carousel;
	Delta -= Num * FMath::RoundToFloat(Delta / Num);
	Carousel += Delta * FMath::Min(1.f, UiDt * 11.f);
	Carousel = FMath::Fmod(Carousel + Num, static_cast<float>(Num));

	const float Top = 182.f * U;
	const float Spacing = 336.f * U;
	TArray<FCardDraw> Cards;
	const int32 Center = FMath::RoundToInt(Carousel);
	for (int32 k = -3; k <= 3; ++k)
	{
		const float Off = static_cast<float>(Center + k) - Carousel;
		if (FMath::Abs(Off) <= 2.6f)
		{
			Cards.Add({ ((Center + k) % Num + Num) % Num, Off });
		}
	}
	// Les plus eloignees d'abord : la carte centrale passe devant
	Cards.Sort([](const FCardDraw& L, const FCardDraw& R) { return FMath::Abs(L.Off) > FMath::Abs(R.Off); });
	for (const FCardDraw& Cd : Cards)
	{
		const float AOff = FMath::Abs(Cd.Off);
		const float Scale = 1.f - 0.15f * FMath::Min(AOff, 2.f);
		const float Alpha = In * FMath::Clamp(1.f - 0.36f * AOff, 0.f, 1.f) * FMath::Clamp((2.6f - AOff) / 0.4f, 0.f, 1.f);
		const float CardCX = CX + Cd.Off * Spacing * (1.f - 0.06f * AOff);
		const int32 LevelNum = All[Cd.Index].Number;
		DrawLevelCard(Cd.Index, CardCX, Top, Scale, Alpha, FMath::Clamp(1.f - AOff * 2.f, 0.f, 1.f), !PC->IsLevelUnlocked(LevelNum),
			Save && Save->CurrentLevel == LevelNum);
		if (bInteractive && AOff < 2.2f)
		{
			// Ecart avec la selection (la liste boucle) : un clic sur une carte voisine la selectionne
			int32 Rel = Cd.Index - Sel;
			Rel -= Num * FMath::RoundToInt(static_cast<float>(Rel) / Num);
			if (FMath::Abs(Rel) <= 3)
			{
				const float CW = 294.f * U * Scale;
				const float CH = 392.f * U * Scale;
				AddButton(Btn_MenuCard + 3 + Rel, CardCX - CW * 0.5f, Top + (392.f * U - CH) * 0.5f, CW, CH);
			}
		}
	}

	// Pagination : fleches et points (pleins : explores, creux : verrouilles)
	const float PagerY = Top + 392.f * U + 30.f * U;
	const float Dot = 8.f * U;
	const float DotGap = 9.f * U;
	const float Wide = 16.f * U;
	const float DotsW = Num * Dot + (Num - 1) * DotGap + Wide;
	float DX = CX - DotsW * 0.5f;
	for (int32 i = 0; i < Num; ++i)
	{
		const bool bOpen = PC->IsLevelUnlocked(All[i].Number);
		const float DW = i == Sel ? Dot + Wide : Dot;
		const FLinearColor DC = i == Sel ? (bOpen ? WithAlpha(Yellow, In) : FLinearColor(0.6f, 0.57f, 0.52f, In))
										 : FLinearColor(1.f, 1.f, 1.f, (bOpen ? 0.5f : 0.14f) * In);
		RoundRect(DX, PagerY - Dot * 0.5f, DW, Dot, Dot * 0.5f, DC);
		DX += DW + DotGap;
	}
	const float AD = 38.f * U;
	for (int32 k = 0; k < 2; ++k)
	{
		const float AX = k == 0 ? CX - DotsW * 0.5f - 26.f * U - AD : CX + DotsW * 0.5f + 26.f * U;
		const float AY = PagerY - AD * 0.5f;
		const bool bHov = bInteractive && Hover(AX, AY, AD, AD);
		RoundRect(AX, AY, AD, AD, AD * 0.5f, bHov ? WithAlpha(Yellow, 0.95f * In) : FLinearColor(0.05f, 0.045f, 0.03f, 0.8f * In));
		RoundRect(AX, AY, AD, AD, AD * 0.5f, FLinearColor(1.f, 0.88f, 0.5f, 0.25f * In), true);
		if (UTexture* T = UiTex(TEXT("UI_IconArrow")))
		{
			const float IS = AD * 0.46f;
			DrawTexture(T, AX + (AD - IS) * 0.5f, AY + (AD - IS) * 0.5f, IS, IS, k == 0 ? 1.f : 0.f, 0.f, k == 0 ? -1.f : 1.f, 1.f,
				bHov ? FLinearColor(0.07f, 0.055f, 0.02f, In) : WithAlpha(Yellow, In), BLEND_Translucent);
		}
		if (bInteractive)
		{
			AddButton(k == 0 ? Btn_MenuLevelPrev : Btn_MenuLevelNext, AX, AY, AD, AD);
		}
	}

	// Details du niveau choisi
	const FBRLevelDef& D = All[Sel];
	const bool bLocked = !PC->IsLevelUnlocked(D.Number);
	float Y = PagerY + 34.f * U;
	if (bLocked)
	{
		TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.NiveauNonExplore", "NIVEAU NON EXPLOR\u00c9")), CX, Y, FLinearColor(0.75f, 0.72f, 0.65f, In), 12.f, EUiWeight::Bold, 3.f * U, EUiAlign::Center);
		Y += 30.f * U;
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.AvezEncoreAtteintNiveauCette", "Vous n'avez pas encore atteint ce niveau dans cette partie.")), CX, Y, WithAlpha(Ink, 0.85f * In), 13.5f, EUiWeight::Regular, EUiAlign::Center);
		Y += 22.f * U;
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.TrouvezJeuSortieMeneDeviendra", "Trouvez en jeu une sortie qui y m\u00e8ne : il deviendra s\u00e9lectionnable ici.")), CX, Y, WithAlpha(InkDim, In), 13.f, EUiWeight::Light,
			EUiAlign::Center);
		Y += 22.f * U;
	}
	else
	{
		TextF(D.ClassText.ToString(), CX, Y, WithAlpha(ClassColor(D.SurvivalClass), In), 12.5f, EUiWeight::Bold, EUiAlign::Center);
		Y += 28.f * U;
		const TArray<FString> Lines = WrapF(D.Description.ToString(), FMath::Min(880.f * U, W - 160.f * U), 13.5f, EUiWeight::Regular);
		for (int32 i = 0; i < Lines.Num() && i < 3; ++i)
		{
			TextF(Lines[i], CX, Y, WithAlpha(Ink, 0.9f * In), 13.5f, EUiWeight::Regular, EUiAlign::Center);
			Y += 22.f * U;
		}
		Y += 6.f * U;
		if (D.bRequireObjectives)
		{
			TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.ObjectifsVhsrequiredCassettesVhsFilmer", "Objectifs : {VHSRequired} cassettes VHS + filmer pendant une coupure de courant"), { { TEXT("VHSRequired"), BRLoc::Int(D.VHSRequired) } }), CX, Y,
				WithAlpha(Yellow, In), 12.f, EUiWeight::Regular, EUiAlign::Center);
			Y += 21.f * U;
		}
		TArray<FString> Names;
		for (const FBREntitySpawn& E : D.Entities)
		{
			Names.AddUnique(ABREntity::Info(E.Kind).Name.ToString());
		}
		if (D.bPatrolEntity)
		{
			Names.AddUnique(ABREntity::Info(D.PatrolKind).Name.ToString());
		}
		if (D.BlackoutSmilers > 0)
		{
			Names.AddUnique(ABREntity::Info(EBREntityKind::Smiler).Name.ToString());
		}
		TextF(Names.Num() > 0 ? BRLoc::Fmt(NSLOCTEXT("BR", "HUD.EntitiesList", "Entit\u00e9s : {List}"), { { TEXT("List"), BRLoc::Arg(FString::Join(Names, TEXT("  \u00b7  "))) } }) : FString(BR_STR(NSLOCTEXT("BR", "HUD.AucuneEntiteSignalee", "Aucune entit\u00e9 signal\u00e9e"))), CX, Y,
			WithAlpha(InkDim, In), 12.f, EUiWeight::Regular, EUiAlign::Center);
		Y += 21.f * U;
		// Sorties connues : un niveau deja explore est nomme, les autres restent un mystere
		TArray<FString> Exits;
		for (const FBRExitDef& X : D.Exits)
		{
			Exits.AddUnique(X.Target < 0 ? FString(BR_STR(NSLOCTEXT("BR", "HUD.Aleatoire", "al\u00e9atoire"))) : (PC->IsLevelUnlocked(X.Target) ? BRLoc::Fmt(NSLOCTEXT("BR", "HUD.NiveauTarget", "Niveau {Target}"), { { TEXT("Target"), BRLoc::Int(X.Target) } })
				: FString(TEXT("???"))));
		}
		if (Exits.Num() > 0)
		{
			TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.ExitsList", "Sorties : {List}"), { { TEXT("List"), BRLoc::Arg(FString::Join(Exits, TEXT("  \u00b7  "))) } }), CX, Y, WithAlpha(InkDim, 0.85f * In), 12.f, EUiWeight::Regular, EUiAlign::Center);
			Y += 21.f * U;
		}
	}

	// NOCLIPPER (ou HEBERGER) / RETOUR
	const float BH = 60.f * U;
	const float PW0 = 300.f * U;
	const float PW1 = 210.f * U;
	const float BG = 18.f * U;
	const float BX = CX - (PW0 + PW1 + BG) * 0.5f;
	const float BY = FMath::Max(836.f * U, Y + 18.f * U);
	MenuPill(0, BX, BY, PW0, BH, bLocked ? TEXT("UI_IconLock") : TEXT("UI_IconPlay"), true, bInteractive, In, false, bLocked);
	MenuPill(1, BX + PW0 + BG, BY, PW1, BH, TEXT("UI_IconBack"), false, bInteractive, In);

	TextF(ControlsLine(0), CX, H - 134.f * U, WithAlpha(InkDim, 0.75f * In), 10.5f, EUiWeight::Regular, EUiAlign::Center, false);
	TextF(ControlsLine(1), CX, H - 112.f * U, WithAlpha(InkDim, 0.75f * In), 10.5f, EUiWeight::Regular, EUiAlign::Center, false);
	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("\u2190 \u2192"), BR_STR(NSLOCTEXT("BR", "HUD.Niveau3", "Niveau")));
	Hints.Emplace(TEXT("\u2191 \u2193"), BR_STR(NSLOCTEXT("BR", "HUD.Choisir", "Choisir")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Entree", "ENTR\u00c9E")), BR_STR(NSLOCTEXT("BR", "HUD.Valider", "Valider")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Echap", "\u00c9CHAP")), BR_STR(NSLOCTEXT("BR", "HUD.Parties", "Parties")));
	KeyHints(CX, H - 76.f * U, Hints, In, true);
}

void ABRHUD::DrawMenuMulti(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const float In = EaseOut(MenuPageTime / 0.45f);

	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.Multijoueur", "MULTIJOUEUR")), X0, 124.f * U, WithAlpha(Ink, In), 32.f, EUiWeight::Black);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.CooperationJusqu4ExplorateursChat", "Coop\u00e9ration jusqu'\u00e0 4 explorateurs  \u00b7  chat vocal de proximit\u00e9")), X0, 178.f * U, WithAlpha(InkDim, In), 13.f, EUiWeight::Light);

	const FString Subs[] = {
		BR_STR(NSLOCTEXT("BR", "HUD.CreerPartiePcPort7777", "Cr\u00e9er la partie sur ce PC (port 7777)")),
		BR_STR(NSLOCTEXT("BR", "HUD.EntrerAdresseIpHote", "Entrer l'adresse IP de l'h\u00f4te")),
		BR_STR(NSLOCTEXT("BR", "HUD.RevenirMenuPrincipal", "Revenir au menu principal")),
	};
	const TCHAR* Icons[] = { TEXT("UI_IconHost"), TEXT("UI_IconJoin"), TEXT("UI_IconBack") };
	const float CW = 540.f * U;
	const float CH = 90.f * U;
	float Y = 250.f * U;
	for (int32 i = 0; i < PC->GetMenuItemCount() && i < 3; ++i)
	{
		MenuCard(i, X0, Y, CW, CH, Subs[i], Icons[i], bInteractive, EaseOut((MenuPageTime - 0.07f * i) / 0.45f));
		Y += CH + 14.f * U;
	}

	// Colonne de droite : qui doit heberger, niveau de depart, adresse IP
	const float PX = FMath::Max(X0 + CW + 70.f * U, W - 56.f * U - 600.f * U);
	const float PW = FMath::Max(320.f * U, FMath::Min(600.f * U, W - PX - 56.f * U));
	const float A = EaseOut((MenuPageTime - 0.15f) / 0.5f);
	float PY = 250.f * U;
	{
		const TArray<FString> Lines = WrapF(BR_STR(NSLOCTEXT("BR", "HUD.JoueurOrdinateurPuissantMeilleureConnexi", "Le joueur qui a l'ordinateur le plus puissant (et la meilleure connexion). Son PC fait tourner le monde, les entit\u00e9s et leurs d\u00e9placements pour tout le groupe ; les autres le rejoignent avec son adresse IP.")),
			PW - 96.f * U, 12.5f, EUiWeight::Regular);
		const float BoxH = 54.f * U + Lines.Num() * 20.f * U + 14.f * U;
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(0.17f, 0.13f, 0.02f, 0.75f * A));
		RoundRect(PX, PY, PW, BoxH, 14.f * U, WithAlpha(Yellow, 0.35f * A), true);
		const float D = 44.f * U;
		RoundRect(PX + 20.f * U, PY + 18.f * U, D, D, D * 0.5f, WithAlpha(Yellow, 0.16f * A));
		if (UTexture* T = UiTex(TEXT("UI_IconHost")))
		{
			DrawTexture(T, PX + 20.f * U + D * 0.2f, PY + 18.f * U + D * 0.2f, D * 0.6f, D * 0.6f, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, A), BLEND_Translucent);
		}
		TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.DoitHeberger", "QUI DOIT H\u00c9BERGER ?")), PX + 80.f * U, PY + 22.f * U, WithAlpha(Yellow, A), 11.f, EUiWeight::Bold, 2.5f * U);
		DrawParagraph(Lines, PX + 80.f * U, PY + 48.f * U, PW - 96.f * U, 20.f * U, WithAlpha(Ink, 0.92f * A), 12.5f, EUiWeight::Regular);
		PY += BoxH + 16.f * U;
	}
	{
		// Partie hebergee : choisie a l'etape suivante, parmi vos sauvegardes
		const TArray<FString> Lines = WrapF(BR_STR(NSLOCTEXT("BR", "HUD.ApresHebergerChoisissezVosParties", "Apr\u00e8s H\u00c9BERGER, choisissez une de vos parties puis un niveau d\u00e9j\u00e0 explor\u00e9. Les niveaux que le groupe d\u00e9couvre s'ajoutent \u00e0 votre partie ; vos amis jouent dans la v\u00f4tre.")),
			PW - 96.f * U, 12.f, EUiWeight::Regular);
		const float BoxH = 54.f * U + Lines.Num() * 19.f * U + 12.f * U;
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.78f * A));
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.12f * A), true);
		const float D = 44.f * U;
		RoundRect(PX + 20.f * U, PY + 18.f * U, D, D, D * 0.5f, WithAlpha(Yellow, 0.12f * A));
		if (UTexture* T = UiTex(TEXT("UI_IconSave")))
		{
			DrawTexture(T, PX + 20.f * U + D * 0.25f, PY + 18.f * U + D * 0.25f, D * 0.5f, D * 0.5f, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, A), BLEND_Translucent);
		}
		TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.VotrePartie", "VOTRE PARTIE")), PX + 80.f * U, PY + 22.f * U, WithAlpha(InkDim, A), 10.f, EUiWeight::Bold, 3.f * U);
		DrawParagraph(Lines, PX + 80.f * U, PY + 46.f * U, PW - 96.f * U, 19.f * U, WithAlpha(Ink, 0.9f * A), 12.f, EUiWeight::Regular);
		PY += BoxH + 16.f * U;
	}
	{
		// Adresse IP a donner aux amis
		const FString Ip = PC->GetLocalAddress().IsEmpty() ? FString(BR_STR(NSLOCTEXT("BR", "HUD.Inconnue", "inconnue"))) : PC->GetLocalAddress();
		const TArray<FString> Help = WrapF(BR_STR(NSLOCTEXT("BR", "HUD.MemeReseauLanDonnezCette", "M\u00eame r\u00e9seau (LAN) : donnez cette adresse \u00e0 vos amis. Par Internet : redirigez le port UDP 7777 vers ce PC sur la box, ou utilisez un r\u00e9seau virtuel (Radmin VPN, ZeroTier, Tailscale\u2026) et son adresse. Chat vocal de proximit\u00e9 : Param\u00e8tres.")),
			PW - 44.f * U, 11.5f, EUiWeight::Regular);
		const float BoxH = 100.f * U + Help.Num() * 18.f * U + 12.f * U;
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.78f * A));
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.12f * A), true);
		TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.VotreAdresseIp", "VOTRE ADRESSE IP")), PX + 22.f * U, PY + 18.f * U, WithAlpha(InkDim, A), 10.f, EUiWeight::Bold, 3.f * U);
		const FVector2f IS = TextSize(Ip, 26.f, EUiWeight::Black);
		TextF(Ip, PX + 22.f * U, PY + 38.f * U, WithAlpha(Yellow, A), 26.f, EUiWeight::Black);
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.Port7777Udp", "port 7777 (UDP)")), PX + 34.f * U + IS.X, PY + 38.f * U + IS.Y - 30.f * U, WithAlpha(InkDim, A), 12.f, EUiWeight::Light, EUiAlign::Left, false);
		DrawParagraph(Help, PX + 22.f * U, PY + 100.f * U, PW - 44.f * U, 18.f * U, WithAlpha(InkDim, 0.95f * A), 11.5f, EUiWeight::Regular);
	}

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("\u2191 \u2193"), BR_STR(NSLOCTEXT("BR", "HUD.Choisir", "Choisir")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Entree", "ENTR\u00c9E")), BR_STR(NSLOCTEXT("BR", "HUD.Valider", "Valider")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Echap", "\u00c9CHAP")), BR_STR(NSLOCTEXT("BR", "HUD.Retour", "Retour")));
	KeyHints(X0, H - 72.f * U, Hints, In, false);
}

void ABRHUD::DrawMenuJoin(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const float In = EaseOut(MenuPageTime / 0.4f);

	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	// Carte centrale autour du champ de saisie (widget Slate de 520 x 52, centre a l'ecran)
	const float CW = 700.f * U;
	const float CH = 384.f * U;
	const float CardX = CX - CW * 0.5f;
	const float CardY = CY - 196.f * U + (1.f - In) * 24.f * U;
	Glow(CX, CY, CW * 0.8f, CH * 0.9f, FLinearColor(1.f, 0.8f, 0.3f, 0.06f * In));
	RoundRect(CardX, CardY + 6.f * U, CW, CH, 22.f * U, FLinearColor(0.f, 0.f, 0.f, 0.35f * In));
	RoundRect(CardX, CardY, CW, CH, 22.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.92f * In));
	RoundRect(CardX, CardY, CW, CH, 22.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.16f * In), true);
	// Badge rond a cheval sur le bord superieur
	const float BD = 64.f * U;
	RoundRect(CX - BD * 0.5f, CardY - BD * 0.5f, BD, BD, BD * 0.5f, WithAlpha(Yellow, In));
	if (UTexture* T = UiTex(TEXT("UI_IconJoin")))
	{
		DrawTexture(T, CX - BD * 0.3f, CardY - BD * 0.3f, BD * 0.6f, BD * 0.6f, 0.f, 0.f, 1.f, 1.f, FLinearColor(0.07f, 0.055f, 0.02f, In), BLEND_Translucent);
	}
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.RejoindrePartie", "REJOINDRE UNE PARTIE")), CX, CardY + 46.f * U, WithAlpha(Ink, In), 24.f, EUiWeight::Black, EUiAlign::Center);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.EntrezAdresseIpHoteVoit", "Entrez l'adresse IP de l'h\u00f4te (il la voit dans son menu MULTIJOUEUR)")), CX, CardY + 92.f * U, WithAlpha(InkDim, In), 12.5f,
		EUiWeight::Light, EUiAlign::Center);
	const float FW = 548.f * U;
	const float FH = 68.f * U;
	RoundRect(CX - FW * 0.5f, CY - FH * 0.5f, FW, FH, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.45f * In));
	RoundRect(CX - FW * 0.5f, CY - FH * 0.5f, FW, FH, 12.f * U, FLinearColor(1.f, 0.84f, 0.3f, (0.5f + 0.25f * FMath::Sin(Clock * 3.f)) * In), true);

	const float BH = 56.f * U;
	const float PW0 = 260.f * U;
	const float PW1 = 180.f * U;
	const float BG = 16.f * U;
	const float BX = CX - (PW0 + PW1 + BG) * 0.5f;
	MenuPill(0, BX, CY + 66.f * U, PW0, BH, TEXT("UI_IconPlay"), true, bInteractive, In);
	MenuPill(1, BX + PW0 + BG, CY + 66.f * U, PW1, BH, TEXT("UI_IconBack"), false, bInteractive, In);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.Exemples192168120", "Exemples :  192.168.1.20   \u00b7   26.45.120.7:7777")), CX, CardY + CH - 40.f * U, WithAlpha(InkDim, 0.85f * In), 11.5f,
		EUiWeight::Regular, EUiAlign::Center, false);

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Entree", "ENTR\u00c9E")), BR_STR(NSLOCTEXT("BR", "HUD.Connecter", "Se connecter")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Echap", "\u00c9CHAP")), BR_STR(NSLOCTEXT("BR", "HUD.Retour", "Retour")));
	KeyHints(CX, H - 76.f * U, Hints, In, true);
}

void ABRHUD::DrawSaveCard(int32 Item, const UBRSaveGame* Save, float X, float Y, float W, float H, bool bInteractive, float Appear)
{
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	if (!PC || !Save)
	{
		return;
	}
	const float U = Ui();
	const float Ap = FMath::Clamp(Appear, 0.f, 1.f);
	float& Sel = MenuSel[FMath::Clamp(Item, 0, static_cast<int32>(UE_ARRAY_COUNT(MenuSel)) - 1)];
	Sel = FMath::FInterpTo(Sel, PC->GetMenuCursor() == Item && !PC->IsConfirmingDelete() ? 1.f : 0.f, UiDt, 14.f);
	const float S = Smooth(Sel);
	const FLinearColor DarkInk(0.07f, 0.055f, 0.02f, 1.f);
	const float CX0 = X - (1.f - Ap) * 40.f * U + S * 14.f * U;
	const float R = 14.f * U;
	const TArray<FBRLevelDef>& All = BRLevels::All();
	const int32 LevelIdx = FMath::Max(0, BRLevels::IndexOf(Save->CurrentLevel));
	const FBRLevelDef& D = All[FMath::Clamp(LevelIdx, 0, All.Num() - 1)];

	Glow(CX0 + W * 0.42f, Y + H * 0.5f, W * 0.75f, H * 1.5f, WithAlpha(Yellow, 0.16f * S * Ap));
	RoundRect(CX0, Y + 4.f * U, W, H, R, FLinearColor(0.f, 0.f, 0.f, 0.3f * Ap));
	RoundRect(CX0, Y, W, H, R, WithAlpha(Mix(FLinearColor(0.05f, 0.045f, 0.03f, 0.8f), FLinearColor(1.f, 0.82f, 0.22f, 0.97f), S), Ap));
	RoundRect(CX0, Y, W, H, R, FLinearColor(1.f, 0.88f, 0.5f, 0.13f * (1.f - S) * Ap), true);
	Gradient(CX0 + R, Y + 1.f * U, W - 2.f * R, H * 0.45f, FLinearColor(1.f, 1.f, 1.f, (0.03f + 0.07f * S) * Ap), 2);

	// Pastille : numero du dernier niveau atteint
	const float DD = H - 28.f * U;
	const float DX = CX0 + 16.f * U;
	const float DY = Y + (H - DD) * 0.5f - 2.f * U;
	RoundRect(DX, DY, DD, DD, DD * 0.5f, WithAlpha(Mix(FLinearColor(1.f, 0.82f, 0.22f, 0.13f), FLinearColor(0.07f, 0.055f, 0.02f, 0.92f), S), Ap));
	const FString Num = FString::FromInt(Save->CurrentLevel);
	const FVector2f NS = TextSize(Num, 17.f, EUiWeight::Black);
	TextF(Num, DX + DD * 0.5f, DY + (DD - NS.Y) * 0.5f, WithAlpha(Yellow, Ap), 17.f, EUiWeight::Black, EUiAlign::Center, false);

	// Nom, dernier niveau ; a droite : progression, temps de jeu, date
	const float TX = DX + DD + 18.f * U;
	const FLinearColor LabelC = Mix(Ink, DarkInk, S);
	const FLinearColor SubC = Mix(InkDim, FLinearColor(0.14f, 0.11f, 0.04f, 0.9f), S);
	const FVector2f LS = TextSize(Save->SaveName, 18.f, EUiWeight::Bold);
	const float TY = Y + (H - LS.Y - 16.f * U) * 0.5f - 2.f * U;
	const float RightW = 170.f * U;
	TextF(Ellipsize(Save->SaveName, W - (TX - CX0) - RightW - 20.f * U, 18.f, EUiWeight::Bold), TX, TY, WithAlpha(LabelC, Ap), 18.f, EUiWeight::Bold,
		EUiAlign::Left, S < 0.5f);
	TextF(Ellipsize(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.NiveauNumberTitle", "Niveau {Number}  \u00b7  {Title}"), { { TEXT("Number"), BRLoc::Int(D.Number) }, { TEXT("Title"), BRLoc::Arg(D.Title) } }), W - (TX - CX0) - RightW - 20.f * U, 11.5f, EUiWeight::Regular), TX,
		TY + LS.Y - 3.f * U, WithAlpha(SubC, Ap), 11.5f, EUiWeight::Regular, EUiAlign::Left, false);
	const float RX = CX0 + W - 22.f * U;
	TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.ExploredAllNiveaux", "{Explored} / {All} niveaux"), { { TEXT("Explored"), BRLoc::Int(Save->Explored.Num()) }, { TEXT("All"), BRLoc::Int(All.Num()) } }), RX, TY + 2.f * U, WithAlpha(Mix(Yellow, DarkInk, S), Ap), 11.f,
		EUiWeight::Bold, EUiAlign::Right, false);
	TextF(BRSaves::FormatPlayTime(Save->PlayTime) + TEXT("  \u00b7  ") + BRSaves::FormatDate(Save->LastPlayed), RX, TY + LS.Y - 3.f * U, WithAlpha(SubC, Ap),
		10.f, EUiWeight::Regular, EUiAlign::Right, false);

	// Barre de progression (niveaux explores) au bas de la carte
	const float BarX = CX0 + R;
	const float BarW = W - 2.f * R;
	const float BarY = Y + H - 9.f * U;
	RoundRect(BarX, BarY, BarW, 3.f * U, 1.5f * U, FLinearColor(S > 0.5f ? 0.f : 1.f, S > 0.5f ? 0.f : 1.f, S > 0.5f ? 0.f : 1.f, 0.12f * Ap));
	RoundRect(BarX, BarY, FMath::Max(3.f * U, BarW * Save->Explored.Num() / FMath::Max(1, All.Num())), 3.f * U, 1.5f * U,
		WithAlpha(Mix(Yellow, DarkInk, S), 0.9f * Ap));
	if (bInteractive && Ap > 0.5f)
	{
		AddButton(Btn_Menu + Item, X, Y, W, H);
	}
}

void ABRHUD::DrawLevelGrid(const UBRSaveGame* Save, float X, float Y, float W, float A)
{
	// Les 12 niveaux en vignettes : apercu des niveaux explores, cadenas pour les autres
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	const TArray<FBRLevelDef>& All = BRLevels::All();
	const float U = Ui();
	const int32 Cols = 4;
	const float Gap = 10.f * U;
	const float TW = (W - (Cols - 1) * Gap) / Cols;
	const float TH = TW * 0.62f;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		const FBRLevelDef& D = All[i];
		const float TX = X + (i % Cols) * (TW + Gap);
		const float TY = Y + (i / Cols) * (TH + Gap);
		const bool bOpen = Save ? Save->IsExplored(D.Number) : (PC && PC->IsLevelUnlocked(D.Number));
		if (bOpen)
		{
			DrawLevelScene(D, TX, TY, TW, TH, 0.45f, A);
		}
		else
		{
			DrawRect(FLinearColor(0.03f, 0.03f, 0.028f, 0.9f * A), TX, TY, TW, TH);
			if (UTexture* T = UiTex(TEXT("UI_IconLock")))
			{
				const float LS = TH * 0.34f;
				DrawTexture(T, TX + (TW - LS) * 0.5f, TY + (TH - LS) * 0.5f - 4.f * U, LS, LS, 0.f, 0.f, 1.f, 1.f, FLinearColor(0.6f, 0.57f, 0.52f, 0.6f * A),
					BLEND_Translucent);
			}
		}
		const FString Num = FString::FromInt(D.Number);
		const FVector2f NS = TextSize(Num, 10.f, EUiWeight::Black);
		RoundRect(TX + 5.f * U, TY + TH - NS.Y - 7.f * U, NS.X + 12.f * U, NS.Y + 2.f * U, 6.f * U, FLinearColor(0.f, 0.f, 0.f, 0.6f * A));
		TextF(Num, TX + 11.f * U, TY + TH - NS.Y - 6.f * U, bOpen ? WithAlpha(Yellow, A) : FLinearColor(0.6f, 0.57f, 0.52f, A), 10.f, EUiWeight::Black, EUiAlign::Left,
			false);
		const bool bHere = Save && Save->CurrentLevel == D.Number;
		Frame(TX, TY, TW, TH, bHere ? WithAlpha(Yellow, 0.95f * A) : FLinearColor(1.f, 1.f, 1.f, 0.08f * A), FMath::Max(1.f, (bHere ? 2.f : 1.f) * U));
	}
}

void ABRHUD::DrawMenuSaves(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const float In = EaseOut(MenuPageTime / 0.45f);
	const bool bConfirm = PC->IsConfirmingDelete();

	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	TextF(PC->IsHostFlow() ? BR_STR(NSLOCTEXT("BR", "HUD.HebergerPartie", "H\u00c9BERGER UNE PARTIE")) : BR_STR(NSLOCTEXT("BR", "HUD.VosParties", "VOS PARTIES")), X0, 124.f * U, WithAlpha(Ink, In), 32.f, EUiWeight::Black);
	TextF(PC->IsHostFlow() ? BR_STR(NSLOCTEXT("BR", "HUD.ChoisissezPartieGroupeVaJouer", "Choisissez la partie que le groupe va jouer : ses niveaux explor\u00e9s seront propos\u00e9s."))
						   : BR_STR(NSLOCTEXT("BR", "HUD.ReprenezPartieCommencezNouvelleNiveau", "Reprenez une partie, ou commencez-en une nouvelle (au Niveau 0).")),
		X0, 178.f * U, WithAlpha(InkDim, In), 13.f, EUiWeight::Light);

	// Liste : parties (de la plus recente a la plus ancienne), NOUVELLE PARTIE, RETOUR
	const float CW = 600.f * U;
	float Y = 236.f * U;
	// (pendant la confirmation d'une suppression, seules les parties restent affichees, inactives)
	const bool bListActive = bInteractive && !bConfirm;
	const int32 Count = bConfirm ? PC->GetSaveOrder().Num() : PC->GetMenuItemCount();
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Slot = PC->GetMenuSaveSlot(i);
		const float Appear = EaseOut((MenuPageTime - 0.06f * i) / 0.45f);
		if (Slot >= 0)
		{
			const float CH = 84.f * U;
			DrawSaveCard(i, PC->GetSaveInSlot(Slot), X0, Y, CW, CH, bListActive, Appear);
			// Corbeille a droite de la carte
			const float TD = 34.f * U;
			const float TX = X0 + CW + 30.f * U;
			const float TY = Y + (CH - TD) * 0.5f;
			const bool bHov = bListActive && Hover(TX, TY, TD, TD);
			const bool bShow = bHov || PC->GetMenuCursor() == i || Hover(X0, Y, CW, CH);
			RoundRect(TX, TY, TD, TD, TD * 0.5f, bHov ? FLinearColor(0.9f, 0.3f, 0.24f, 0.95f * Appear) : FLinearColor(0.05f, 0.045f, 0.03f, (bShow ? 0.8f : 0.4f) * Appear));
			if (UTexture* T = UiTex(TEXT("UI_IconTrash")))
			{
				DrawTexture(T, TX + TD * 0.27f, TY + TD * 0.25f, TD * 0.46f, TD * 0.46f, 0.f, 0.f, 1.f, 1.f,
					bHov ? FLinearColor(1.f, 1.f, 1.f, Appear) : FLinearColor(0.95f, 0.5f, 0.42f, (bShow ? 0.9f : 0.35f) * Appear), BLEND_Translucent);
			}
			if (bListActive)
			{
				AddButton(Btn_SaveDelete + Slot, TX, TY, TD, TD);
			}
			Y += CH + 10.f * U;
		}
		else
		{
			const float CH = 70.f * U;
			const bool bNew = Slot == ABRPlayerController::MenuItemNew;
			MenuCard(i, X0, Y, CW, CH, bNew ? BR_STR(NSLOCTEXT("BR", "HUD.CommenceNiveau0EquipementDepart", "Commence au Niveau 0, avec l'\u00e9quipement de d\u00e9part")) : (PC->IsHostFlow() ? BR_STR(NSLOCTEXT("BR", "HUD.Multijoueur2", "Multijoueur"))
				: BR_STR(NSLOCTEXT("BR", "HUD.MenuPrincipal", "Menu principal"))), bNew ? TEXT("UI_IconPlus") : TEXT("UI_IconBack"), bInteractive, Appear);
			Y += CH + 10.f * U;
		}
	}

	// Panneau de droite : details de la partie sous le curseur (ou explication d'une nouvelle partie)
	const float PX = FMath::Max(X0 + CW + 90.f * U, W - 56.f * U - 560.f * U);
	const float PW = FMath::Max(300.f * U, FMath::Min(560.f * U, W - PX - 56.f * U));
	const float A = EaseOut((MenuPageTime - 0.15f) / 0.5f);
	const int32 Focus = PC->GetMenuSaveSlot(PC->GetMenuCursor());
	const UBRSaveGame* Shown = (!bConfirm && Focus >= 0) ? PC->GetSaveInSlot(Focus) : nullptr;
	float PY = 236.f * U;
	if (Shown)
	{
		const TArray<FBRLevelDef>& All = BRLevels::All();
		const FBRLevelDef& D = All[FMath::Clamp(BRLevels::IndexOf(Shown->CurrentLevel), 0, All.Num() - 1)];
		const float TileW = (PW - 44.f * U - 30.f * U) / 4.f;
		const float GridH = 3.f * TileW * 0.62f + 20.f * U;
		const float BoxH = 200.f * U + GridH;
		RoundRect(PX, PY, PW, BoxH, 16.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.82f * A));
		RoundRect(PX, PY, PW, BoxH, 16.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.12f * A), true);
		TextF(Ellipsize(Shown->SaveName, PW - 44.f * U, 24.f, EUiWeight::Black), PX + 22.f * U, PY + 16.f * U, WithAlpha(Ink, A), 24.f, EUiWeight::Black);
		float LY = PY + 62.f * U;
		// v4.7 : ce que donnera "Reprendre" (meme disposition et objectifs, ou niveau neuf)
		const FBRSessionState& Ses = Shown->Session;
		const FString Resume = Shown->bFutureFormat
			? BRLoc::Fmt(NSLOCTEXT("BR", "HUD.VersionRecenteJeuFormatLoadedversion", "Version plus r\u00e9cente du jeu (format {LoadedVersion}) : partie conserv\u00e9e intacte, lecture seule"), { { TEXT("LoadedVersion"), BRLoc::Int(Shown->LoadedVersion) } })
			: Shown->bPendingDeath
			? FString(BR_STR(NSLOCTEXT("BR", "HUD.RepriseDerniereSessionArreteePendant", "Reprise : la derni\u00e8re session s'est arr\u00eat\u00e9e pendant une mort (\u00e9quipement de d\u00e9part, niveau neuf)")))
			: (Ses.bValid && Ses.Level == D.Number
				? BRLoc::Fmt(NSLOCTEXT("BR", "HUD.RepriseIdentiqueMemeDispositionVhsfound", "Reprise \u00e0 l'identique : m\u00eame disposition, {VHSFound} cassette(s), {Collected} objet(s) ramass\u00e9(s){HasSpot}"), { { TEXT("VHSFound"), BRLoc::Int(Ses.VHSFound) }, { TEXT("Collected"), BRLoc::Int(Ses.Collected.Num()) }, { TEXT("HasSpot"), BRLoc::Arg(Ses.bHasSpot ? BR_STR(NSLOCTEXT("BR", "HUD.DernierePosition2", ", derni\u00e8re position")) : BR_STR(NSLOCTEXT("BR", "HUD.PointDepart", ", point de d\u00e9part"))) } })
				: FString(BR_STR(NSLOCTEXT("BR", "HUD.RepriseNouvelleDispositionNiveauPartie", "Reprise : nouvelle disposition du niveau (partie d'une version ant\u00e9rieure ou apr\u00e8s une mort)"))));
		const FString Lines[] = {
			BRLoc::Fmt(NSLOCTEXT("BR", "HUD.DernierePositionNiveauNumberTitle", "Derni\u00e8re position : Niveau {Number}  \u00b7  {Title}"), { { TEXT("Number"), BRLoc::Int(D.Number) }, { TEXT("Title"), BRLoc::Arg(D.Title) } }),
			Resume,
			BRLoc::Fmt(NSLOCTEXT("BR", "HUD.TempsJeuPlaytimeMortsDeaths", "Temps de jeu : {PlayTime}  \u00b7  morts : {Deaths}  \u00b7  entit\u00e9s rencontr\u00e9es : {Discovered} / {Count}"), { { TEXT("PlayTime"), BRLoc::Arg(BRSaves::FormatPlayTime(Shown->PlayTime)) }, { TEXT("Deaths"), BRLoc::Int(Shown->Deaths) }, { TEXT("Discovered"), BRLoc::Int(Shown->Discovered.Num()) }, { TEXT("Count"), BRLoc::Int(static_cast<int32>(EBREntityKind::Count)) } }),
			BRLoc::Fmt(NSLOCTEXT("BR", "HUD.CreeeCreatedJoueeLastplayed", "Cr\u00e9\u00e9e le {Created}  \u00b7  jou\u00e9e le {LastPlayed}"), { { TEXT("Created"), BRLoc::Arg(BRSaves::FormatDay(Shown->Created)) }, { TEXT("LastPlayed"), BRLoc::Arg(BRSaves::FormatDate(Shown->LastPlayed)) } }),
		};
		for (const FString& L : Lines)
		{
			TextF(Ellipsize(L, PW - 44.f * U, 12.f, EUiWeight::Regular), PX + 22.f * U, LY, WithAlpha(InkDim, A), 12.f, EUiWeight::Regular, EUiAlign::Left, false);
			LY += 21.f * U;
		}
		LY += 14.f * U;
		TextSpaced(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.NiveauxExploresExploredAll", "NIVEAUX EXPLOR\u00c9S  {Explored} / {All}"), { { TEXT("Explored"), BRLoc::Int(Shown->Explored.Num()) }, { TEXT("All"), BRLoc::Int(All.Num()) } }), PX + 22.f * U, LY, WithAlpha(Yellow, A), 10.5f,
			EUiWeight::Bold, 2.5f * U);
		DrawLevelGrid(Shown, PX + 22.f * U, LY + 26.f * U, PW - 44.f * U, A);
	}
	else if (!bConfirm && Focus == ABRPlayerController::MenuItemNew)
	{
		const TArray<FString> Lines = WrapF(BR_STR(NSLOCTEXT("BR", "HUD.CommencezNiveau0EquipementDepart", "Vous commencez au Niveau 0, avec l'\u00e9quipement de d\u00e9part. Chaque niveau que vous d\u00e9couvrez en jeu devient s\u00e9lectionnable quand vous reprenez la partie. Sauvegarde automatique \u00e0 chaque niveau, puis toutes les minutes.")),
			PW - 44.f * U, 12.5f, EUiWeight::Regular);
		const float BoxH = 112.f * U + Lines.Num() * 20.f * U;
		RoundRect(PX, PY, PW, BoxH, 16.f * U, FLinearColor(0.17f, 0.13f, 0.02f, 0.75f * A));
		RoundRect(PX, PY, PW, BoxH, 16.f * U, WithAlpha(Yellow, 0.35f * A), true);
		const float D = 44.f * U;
		RoundRect(PX + 22.f * U, PY + 20.f * U, D, D, D * 0.5f, WithAlpha(Yellow, 0.16f * A));
		if (UTexture* T = UiTex(TEXT("UI_IconPlus")))
		{
			DrawTexture(T, PX + 22.f * U + D * 0.25f, PY + 20.f * U + D * 0.25f, D * 0.5f, D * 0.5f, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, A), BLEND_Translucent);
		}
		TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.NouvellePartie", "NOUVELLE PARTIE")), PX + 80.f * U, PY + 24.f * U, WithAlpha(Yellow, A), 11.f, EUiWeight::Bold, 2.5f * U);
		TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.EmplacementsLibresSaveorder6", "Emplacements libres : {SaveOrder} / 6"), { { TEXT("SaveOrder"), BRLoc::Int(6 - PC->GetSaveOrder().Num()) } }), PX + 80.f * U, PY + 44.f * U, WithAlpha(InkDim, A), 11.f,
			EUiWeight::Regular, EUiAlign::Left, false);
		DrawParagraph(Lines, PX + 22.f * U, PY + 84.f * U, PW - 44.f * U, 20.f * U, WithAlpha(Ink, 0.92f * A), 12.5f, EUiWeight::Regular);
	}

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("\u2191 \u2193"), BR_STR(NSLOCTEXT("BR", "HUD.Choisir", "Choisir")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Entree", "ENTR\u00c9E")), BR_STR(NSLOCTEXT("BR", "HUD.Ouvrir", "Ouvrir")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Suppr", "SUPPR")), BR_STR(NSLOCTEXT("BR", "HUD.Supprimer", "Supprimer")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Echap", "\u00c9CHAP")), BR_STR(NSLOCTEXT("BR", "HUD.Retour", "Retour")));
	KeyHints(X0, H - 72.f * U, Hints, In, false);

	// Confirmation de suppression
	if (bConfirm)
	{
		const UBRSaveGame* Gone = PC->GetSaveInSlot(PC->GetDeleteSlot());
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, W, H);
		const float CW2 = 640.f * U;
		const float CH2 = 280.f * U;
		const float CX = W * 0.5f;
		const float CardY = H * 0.5f - CH2 * 0.5f;
		Glow(CX, H * 0.5f, CW2 * 0.8f, CH2, FLinearColor(0.9f, 0.3f, 0.24f, 0.08f));
		RoundRect(CX - CW2 * 0.5f, CardY, CW2, CH2, 22.f * U, FLinearColor(0.06f, 0.04f, 0.035f, 0.97f));
		RoundRect(CX - CW2 * 0.5f, CardY, CW2, CH2, 22.f * U, FLinearColor(0.9f, 0.35f, 0.28f, 0.45f), true);
		const float BD = 60.f * U;
		RoundRect(CX - BD * 0.5f, CardY - BD * 0.5f, BD, BD, BD * 0.5f, FLinearColor(0.9f, 0.3f, 0.24f, 1.f));
		if (UTexture* T = UiTex(TEXT("UI_IconTrash")))
		{
			DrawTexture(T, CX - BD * 0.28f, CardY - BD * 0.28f, BD * 0.56f, BD * 0.56f, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, BLEND_Translucent);
		}
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.SupprimerPartie", "SUPPRIMER LA PARTIE ?")), CX, CardY + 46.f * U, Ink, 22.f, EUiWeight::Black, EUiAlign::Center);
		if (Gone)
		{
			TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.SavenameSeraEffaceeDefinitivement", "\u00ab {SaveName} \u00bb sera effac\u00e9e d\u00e9finitivement"), { { TEXT("SaveName"), BRLoc::Arg(Gone->SaveName) } }), CX, CardY + 92.f * U, InkDim, 13.f, EUiWeight::Regular,
				EUiAlign::Center);
			const int32 NumExp = Gone->Explored.Num();
			TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.LevelsExploredPlayTime", "{Count} {Count}|plural(one=niveau explor\u00e9,other=niveaux explor\u00e9s)  \u00b7  {PlayTime} de jeu"), { { TEXT("Count"), BRLoc::Int(NumExp) }, { TEXT("PlayTime"), BRLoc::Arg(BRSaves::FormatPlayTime(Gone->PlayTime)) } }), CX,
				CardY + 116.f * U, WithAlpha(InkDim, 0.8f), 12.f, EUiWeight::Light, EUiAlign::Center);
		}
		const float BH = 56.f * U;
		const float PW0 = 250.f * U;
		const float PW1 = 190.f * U;
		const float BX = CX - (PW0 + PW1 + 16.f * U) * 0.5f;
		MenuPill(0, BX, CardY + CH2 - BH - 34.f * U, PW0, BH, TEXT("UI_IconTrash"), true, bInteractive, 1.f, true);
		MenuPill(1, BX + PW0 + 16.f * U, CardY + CH2 - BH - 34.f * U, PW1, BH, TEXT("UI_IconBack"), false, bInteractive, 1.f);
	}
}

void ABRHUD::DrawMenuNewSave(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const float In = EaseOut(MenuPageTime / 0.4f);

	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	// Carte centrale autour du champ du nom (widget Slate de 520 x 52, centre a l'ecran)
	const float CW = 700.f * U;
	const float CH = 384.f * U;
	const float CardX = CX - CW * 0.5f;
	const float CardY = CY - 196.f * U + (1.f - In) * 24.f * U;
	Glow(CX, CY, CW * 0.8f, CH * 0.9f, FLinearColor(1.f, 0.8f, 0.3f, 0.06f * In));
	RoundRect(CardX, CardY + 6.f * U, CW, CH, 22.f * U, FLinearColor(0.f, 0.f, 0.f, 0.35f * In));
	RoundRect(CardX, CardY, CW, CH, 22.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.92f * In));
	RoundRect(CardX, CardY, CW, CH, 22.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.16f * In), true);
	const float BD = 64.f * U;
	RoundRect(CX - BD * 0.5f, CardY - BD * 0.5f, BD, BD, BD * 0.5f, WithAlpha(Yellow, In));
	if (UTexture* T = UiTex(TEXT("UI_IconSave")))
	{
		DrawTexture(T, CX - BD * 0.28f, CardY - BD * 0.28f, BD * 0.56f, BD * 0.56f, 0.f, 0.f, 1.f, 1.f, FLinearColor(0.07f, 0.055f, 0.02f, In), BLEND_Translucent);
	}
	TextF(PC->IsHostFlow() ? BR_STR(NSLOCTEXT("BR", "HUD.NouvellePartieLigne", "NOUVELLE PARTIE EN LIGNE")) : BR_STR(NSLOCTEXT("BR", "HUD.NouvellePartie", "NOUVELLE PARTIE")), CX, CardY + 46.f * U, WithAlpha(Ink, In), 24.f, EUiWeight::Black,
		EUiAlign::Center);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.DonnezLuiNomCommencerezNiveau", "Donnez-lui un nom. Vous commencerez au Niveau 0, avec l'\u00e9quipement de d\u00e9part.")), CX, CardY + 92.f * U, WithAlpha(InkDim, In), 12.5f,
		EUiWeight::Light, EUiAlign::Center);
	const float FW = 548.f * U;
	const float FH = 68.f * U;
	RoundRect(CX - FW * 0.5f, CY - FH * 0.5f, FW, FH, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.45f * In));
	RoundRect(CX - FW * 0.5f, CY - FH * 0.5f, FW, FH, 12.f * U, FLinearColor(1.f, 0.84f, 0.3f, (0.5f + 0.25f * FMath::Sin(Clock * 3.f)) * In), true);

	const float BH = 56.f * U;
	const float PW0 = 260.f * U;
	const float PW1 = 180.f * U;
	const float BG = 16.f * U;
	const float BX = CX - (PW0 + PW1 + BG) * 0.5f;
	MenuPill(0, BX, CY + 66.f * U, PW0, BH, TEXT("UI_IconPlay"), true, bInteractive, In);
	MenuPill(1, BX + PW0 + BG, CY + 66.f * U, PW1, BH, TEXT("UI_IconBack"), false, bInteractive, In);
	TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.EmplacementSaveorder6SauvegardeAutomatiq", "Emplacement {SaveOrder} / 6   \u00b7   sauvegarde automatique \u00e0 chaque niveau"), { { TEXT("SaveOrder"), BRLoc::Int(PC->GetSaveOrder().Num() + 1) } }), CX, CardY + CH - 40.f * U,
		WithAlpha(InkDim, 0.85f * In), 11.5f, EUiWeight::Regular, EUiAlign::Center, false);

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Entree", "ENTR\u00c9E")), BR_STR(NSLOCTEXT("BR", "HUD.Commencer", "Commencer")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Echap", "\u00c9CHAP")), BR_STR(NSLOCTEXT("BR", "HUD.Retour", "Retour")));
	KeyHints(CX, H - 76.f * U, Hints, In, true);
}

void ABRHUD::DrawSaveIndicator(float Since)
{
	// Petite pastille en bas a droite pendant une sauvegarde automatique
	const float A = FMath::Clamp(Since / 0.2f, 0.f, 1.f) * FMath::Clamp((2.6f - Since) / 0.6f, 0.f, 1.f);
	if (A <= 0.01f)
	{
		return;
	}
	const float U = Ui();
	const FString Label = BR_STR(NSLOCTEXT("BR", "HUD.Sauvegarde", "Sauvegarde"));
	const FVector2f LS = TextSize(Label, 10.5f, EUiWeight::Regular);
	const float PH = 30.f * U;
	const float PW = LS.X + 52.f * U;
	const float PX = Canvas->ClipX - PW - 50.f * U;
	const float PY = Canvas->ClipY - PH - 112.f * U;
	RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.45f * A));
	if (UTexture* T = UiTex(TEXT("UI_IconSave")))
	{
		const float IS = 15.f * U;
		const float Pulse = 0.6f + 0.4f * FMath::Sin(Since * 9.f);
		DrawTexture(T, PX + 13.f * U, PY + (PH - IS) * 0.5f, IS, IS, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, A * Pulse), BLEND_Translucent);
	}
	TextF(Label, PX + 36.f * U, PY + (PH - LS.Y) * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.85f * A), 10.5f, EUiWeight::Regular, EUiAlign::Left, false);
}

void ABRHUD::HandleMenuMouse(ABRPlayerController* PC)
{
	if (!PC || !PlayerOwner || BRDisplay::IsPending())
	{
		return;
	}
	// Le survol ne deplace la selection que si la souris bouge (sinon le clavier reprend la main)
	const bool bMoved = FMath::Abs(MouseX - LastMenuMouseX) > 1.f || FMath::Abs(MouseY - LastMenuMouseY) > 1.f;
	LastMenuMouseX = MouseX;
	LastMenuMouseY = MouseY;
	const int32 Id = ButtonAt(MouseX, MouseY);
	if (bMoved && Id >= Btn_Menu && Id < Btn_Menu + 8)
	{
		PC->SetMenuCursor(Id - Btn_Menu);
	}
	if (bMoved && Id >= Btn_Language && Id < Btn_Language + 32)
	{
		PC->SetMenuCursor(Id - Btn_Language);
	}
	if (!PlayerOwner->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		return;
	}
	if (Id >= Btn_Menu && Id < Btn_Menu + 8)
	{
		PC->MenuActivate(Id - Btn_Menu);
	}
	else if (Id == Btn_MenuLevelPrev || Id == Btn_MenuLevelNext)
	{
		PC->MenuShiftLevel(Id == Btn_MenuLevelPrev ? -1 : 1);
	}
	else if (Id >= Btn_SaveDelete && Id < Btn_SaveDelete + 16)
	{
		PC->RequestDeleteSave(Id - Btn_SaveDelete);
	}
	else if (Id >= Btn_Language && Id < Btn_Language + 32)
	{
		PC->MenuActivate(Id - Btn_Language);
	}
	else if (Id >= Btn_MenuCard && Id <= Btn_MenuCard + 6)
	{
		// Carte du carrousel : celle du centre lance la partie, une voisine devient la selection
		const int32 Rel = Id - Btn_MenuCard - 3;
		if (Rel == 0)
		{
			PC->MenuActivate(0);
		}
		for (int32 k = 0; k < FMath::Abs(Rel); ++k)
		{
			PC->MenuShiftLevel(Rel > 0 ? 1 : -1);
		}
	}
}

void ABRHUD::DrawTitleCard()
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W)
	{
		return;
	}
	const float T = W->GetTitleTime();
	if (T <= 0.f || W->IsTransitioning())
	{
		return;
	}
	// Carte de titre facon generique de film : bandes noires, numero du niveau, filet jaune qui s'etire
	const float E = 7.f - T; // temps ecoule depuis l'arrivee
	const float Bars = EaseOut(E / 0.8f) * FMath::Clamp(T / 0.9f, 0.f, 1.f);
	const float A = FMath::Clamp((E - 0.35f) / 1.f, 0.f, 1.f) * FMath::Clamp(T / 1.3f, 0.f, 1.f);
	const FBRLevelDef& D = W->Def();
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float H = Canvas->ClipY;
	const float BH = H * 0.085f * Bars;
	DrawRect(FLinearColor::Black, 0.f, 0.f, Canvas->ClipX, BH);
	DrawRect(FLinearColor::Black, 0.f, H - BH, Canvas->ClipX, BH);
	if (A <= 0.003f)
	{
		return;
	}
	const float Y0 = H * 0.29f;
	Glow(CX, Y0 + 150.f * U, 660.f * U, 320.f * U, FLinearColor(0.f, 0.f, 0.f, 0.65f * A));
	Glow(CX, Y0 + 90.f * U, 380.f * U, 160.f * U, FLinearColor(1.f, 0.78f, 0.25f, 0.07f * A));
	TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.Niveau", "NIVEAU")), CX, Y0, FLinearColor(Ink.R, Ink.G, Ink.B, 0.9f * A), 15.f, EUiWeight::Regular, 12.f * U, EUiAlign::Center);
	const FString Num = FString::FromInt(D.Number);
	TextF(Num, CX, Y0 + 20.f * U, FLinearColor(1.f, 1.f, 1.f, A), 88.f, EUiWeight::Black, EUiAlign::Center);
	const float LY = Y0 + 20.f * U + TextSize(Num, 88.f, EUiWeight::Black).Y + 2.f * U;
	const float L = 230.f * U * EaseOut((E - 0.6f) / 1.2f);
	const float Th = FMath::Max(1.f, 2.f * U);
	DrawRect(WithAlpha(Yellow, 0.85f * A), CX - 18.f * U - L, LY, L, Th);
	DrawRect(WithAlpha(Yellow, 0.85f * A), CX + 18.f * U, LY, L, Th);
	RoundRect(CX - 5.f * U, LY + Th * 0.5f - 5.f * U, 10.f * U, 10.f * U, 5.f * U, WithAlpha(Yellow, A));
	TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.QuotedTitle", "\u00ab {Title} \u00bb"), { { TEXT("Title"), BRLoc::Arg(D.Title) } }), CX, LY + 22.f * U, WithAlpha(Ink, A), 26.f, EUiWeight::Bold, EUiAlign::Center);
	TextF(D.Nickname.ToString(), CX, LY + 66.f * U, FLinearColor(Ink.R, Ink.G, Ink.B, 0.85f * A), 16.f, EUiWeight::Regular, EUiAlign::Center);
	const FLinearColor CC = ClassColor(D.SurvivalClass);
	const FVector2f CS = TextSize(D.ClassText.ToString(), 11.f, EUiWeight::Bold);
	const float PH = 28.f * U;
	const float PW = CS.X + 46.f * U;
	const float PY = LY + 108.f * U;
	RoundRect(CX - PW * 0.5f, PY, PW, PH, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.45f * A));
	RoundRect(CX - PW * 0.5f, PY, PW, PH, PH * 0.5f, FLinearColor(CC.R, CC.G, CC.B, 0.18f * A));
	const float Dot = 8.f * U;
	RoundRect(CX - PW * 0.5f + 14.f * U, PY + (PH - Dot) * 0.5f, Dot, Dot, Dot * 0.5f, WithAlpha(CC, A));
	TextF(D.ClassText.ToString(), CX + 8.f * U, PY + (PH - CS.Y) * 0.5f, WithAlpha(CC, A), 11.f, EUiWeight::Bold, EUiAlign::Center, false);
}

// =====================================================================================================================
// HUD de jeu
// =====================================================================================================================

void ABRHUD::DrawRecording(ABRCharacter* C, ABRWorld* W)
{
	// v4.2 : plus de viseur de camescope (REC, coins, point rouge) ; seulement la tache d'enregistrement en cours
	// et l'indicateur de vision nocturne
	if (!C->HasCamcorder())
	{
		return;
	}
	const float U = Ui();
	const float X = 40.f * U;
	const float Y = 34.f * U;
	if (W && !W->GetRecordLabel().IsEmpty())
	{
		const FString Label = BRLoc::Fmt(NSLOCTEXT("BR", "HUD.RecordingLabel", "ENREGISTREMENT : {Label}"), { { TEXT("Label"), BRLoc::Arg(W->GetRecordLabel()) } });
		const float PW = FMath::Max(260.f * U, TextSize(Label, 11.f, EUiWeight::Bold).X + 36.f * U);
		const float PH = 46.f * U;
		RoundRect(X, Y, PW, PH, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.45f));
		TextF(Label, X + 18.f * U, Y + 7.f * U, Yellow, 11.f, EUiWeight::Bold);
		const float BarW = PW - 36.f * U;
		const float BarH = 5.f * U;
		const float BarY = Y + PH - 13.f * U;
		RoundRect(X + 18.f * U, BarY, BarW, BarH, BarH * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.15f));
		const float Progress = FMath::Clamp(W->GetRecordProgress(), 0.f, 1.f);
		if (Progress > 0.f)
		{
			RoundRect(X + 18.f * U, BarY, FMath::Max(BarH, BarW * Progress), BarH, BarH * 0.5f, Yellow);
		}
	}
	if (C->IsNightVision())
	{
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.VisionNocturne", "VISION NOCTURNE")), Canvas->ClipX - 40.f * U, Y, FLinearColor(0.5f, 1.f, 0.5f, 0.92f), 11.f, EUiWeight::Bold, EUiAlign::Right);
		if (FBRSettings::Get().bVHSEffect)
		{
			Scanlines(0.05f);
		}
	}
}

void ABRHUD::DrawDevOverlay(ABRPlayerController* PC, ABRCharacter* C, ABRWorld* W)
{
	const float U = Ui();
	const FLinearColor Cyan(0.55f, 0.88f, 1.f, 0.95f);
	// Pastille permanente, en haut au centre : niveau courant et etat des aides
	FString Tag = W ? BRLoc::Fmt(NSLOCTEXT("BR", "HUD.DevNiveauLevelnumber", "DEV  \u00b7  NIVEAU {LevelNumber}"), { { TEXT("LevelNumber"), BRLoc::Int(W->GetLevelNumber()) } }) : FString(BR_STR(NSLOCTEXT("BR", "HUD.Dev", "DEV")));
	if (C && C->IsDevFlying())
	{
		Tag += BR_STR(NSLOCTEXT("BR", "HUD.Vol", "  \u00b7  VOL"));
	}
	if (C && C->bGodMode)
	{
		Tag += BR_STR(NSLOCTEXT("BR", "HUD.Invincible", "  \u00b7  INVINCIBLE"));
	}
	if (PC->IsDevSession())
	{
		Tag += BR_STR(NSLOCTEXT("BR", "HUD.HorsPartie", "  \u00b7  hors partie"));
	}
	Tag += TEXT("  \u00b7  ") + PC->GetRenderModeText(true);
	if (W)
	{
		// v4.7 : phase du directeur de tension (et depuis combien de temps)
		Tag += BRLoc::Fmt(NSLOCTEXT("BR", "HUD.TensionTensionTensiontime", "  \u00b7  tension : {Tension} {TensionTime} s"), { { TEXT("Tension"), BRLoc::Arg(TensionLabel(W->GetTension())) }, { TEXT("TensionTime"), BRLoc::Num(W->GetTensionTime(), 0) } });
	}
	const FVector2f TS = TextSize(Tag, 10.f, EUiWeight::Bold);
	const float PW = TS.X + 28.f * U;
	const float PH = 26.f * U;
	const float PX = (Canvas->ClipX - PW) * 0.5f;
	const float PY = 12.f * U;
	RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(0.02f, 0.06f, 0.09f, 0.7f));
	RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(0.55f, 0.88f, 1.f, 0.45f), true);
	TextF(Tag, PX + 14.f * U, PY + (PH - TS.Y) * 0.5f, Cyan, 10.f, EUiWeight::Bold);

	// v4.5 : modeles fournis remplaces par une forme de secours (import incomplet) : jamais en silence
	const TArray<FString>& Fallbacks = UBRAssets::GetFallbacks();
	if (Fallbacks.Num() > 0)
	{
		const FString Msg = BR_STR(NSLOCTEXT("BR", "HUD.ModelesSecoursImportIncomplet", "MOD\u00c8LES DE SECOURS (import incomplet) : ")) + FString::Join(Fallbacks, TEXT("  \u00b7  "));
		TextF(Msg, Canvas->ClipX * 0.5f, PY + PH + 8.f * U, FLinearColor(1.f, 0.45f, 0.35f, 0.95f), 9.f, EUiWeight::Bold, EUiAlign::Center);
	}
	// v4.8 : materiaux en erreur (usage absent, slot inattendu, materiau de secours) : le premier, et leur nombre
	const TArray<FString>& MatProblems = UBRAssets::GetMaterialProblems();
	if (MatProblems.Num() > 0)
	{
		const FString Msg = BRLoc::Fmt(NSLOCTEXT("BR", "HUD.MateriauxMatproblemsMatproblems2", "MAT\u00c9RIAUX ({MatProblems}) : {MatProblems2}"), { { TEXT("MatProblems"), BRLoc::Int(MatProblems.Num()) }, { TEXT("MatProblems2"), BRLoc::Arg(MatProblems[0]) } });
		TextF(Ellipsize(Msg, Canvas->ClipX * 0.9f, 9.f, EUiWeight::Bold), Canvas->ClipX * 0.5f, PY + PH + 24.f * U,
			FLinearColor(1.f, 0.35f, 0.85f, 0.95f), 9.f, EUiWeight::Bold, EUiAlign::Center);
	}

	// Aide des raccourcis : a l'arrivee dans un niveau et apres chaque raccourci
	const float T = PC->GetDevHelpTime();
	if (T <= 0.f)
	{
		return;
	}
	const float A = FMath::Clamp(T / 0.6f, 0.f, 1.f);
	const FString Keys[][2] = {
		{ BR_STR(NSLOCTEXT("BR", "HUD.PagePrecSuiv", "PAGE PR\u00c9C. / SUIV.")), BR_STR(NSLOCTEXT("BR", "HUD.NiveauSuivantPrecedent", "niveau suivant / pr\u00e9c\u00e9dent")) },
		{ BR_STR(NSLOCTEXT("BR", "HUD.Debut", "D\u00c9BUT")), BR_STR(NSLOCTEXT("BR", "HUD.NouvelleDispositionNiveau", "nouvelle disposition du niveau")) },
		{ BR_STR(NSLOCTEXT("BR", "HUD.F6", "F6")), BR_STR(NSLOCTEXT("BR", "HUD.VolLibreTraversMurs", "vol libre \u00e0 travers les murs")) },
		{ BR_STR(NSLOCTEXT("BR", "HUD.F7", "F7")), BR_STR(NSLOCTEXT("BR", "HUD.Invincible2", "invincible")) },
		{ BR_STR(NSLOCTEXT("BR", "HUD.F10", "F10")), BR_STR(NSLOCTEXT("BR", "HUD.JumpscareSuivantChaqueEntite", "jumpscare suivant (chaque entit\u00e9)")) },
		{ BR_STR(NSLOCTEXT("BR", "HUD.Fin", "FIN")), BR_STR(NSLOCTEXT("BR", "HUD.ObjectifsRemplis", "objectifs remplis")) },
		{ BR_STR(NSLOCTEXT("BR", "HUD.Inser", "INSER")), BR_STR(NSLOCTEXT("BR", "HUD.CoupureCourant", "coupure de courant")) },
		{ BR_STR(NSLOCTEXT("BR", "HUD.Suppr", "SUPPR")), BR_STR(NSLOCTEXT("BR", "HUD.SalleFossesNiveau0", "salle de fosses (Niveau 0)")) },
	};
	const int32 N = UE_ARRAY_COUNT(Keys);
	const float RowH = 24.f * U;
	const float BW = 360.f * U;
	const float BH = 44.f * U + N * RowH;
	const float BX = 40.f * U;
	const float BY = Canvas->ClipY * 0.5f - BH * 0.5f;
	RoundRect(BX, BY, BW, BH, 12.f * U, FLinearColor(0.01f, 0.03f, 0.05f, 0.72f * A));
	RoundRect(BX, BY, BW, BH, 12.f * U, FLinearColor(0.55f, 0.88f, 1.f, 0.35f * A), true);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.ModeDeveloppeur", "MODE D\u00c9VELOPPEUR")), BX + 16.f * U, BY + 12.f * U, WithAlpha(Cyan, A), 10.f, EUiWeight::Black);
	for (int32 i = 0; i < N; ++i)
	{
		const float Y = BY + 40.f * U + i * RowH;
		TextF(Keys[i][0], BX + 16.f * U, Y, WithAlpha(Yellow, A), 9.5f, EUiWeight::Bold);
		TextF(Keys[i][1], BX + 150.f * U, Y, FLinearColor(0.92f, 0.9f, 0.84f, 0.92f * A), 9.5f, EUiWeight::Regular);
	}
}

void ABRHUD::DrawJumpscare(ABRCharacter* C)
{
	const int32 K = C->GetScareKind();
	const float T = C->GetScareTime();
	const float Dur = FMath::Max(0.1f, C->GetScareDuration());
	const float Impact = C->GetScareImpact();
	const bool bHit = T >= Impact;
	const float Since = FMath::Max(0.f, T - Impact);
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float U = Ui();
	const float Out = FMath::Clamp((Dur - T) / 0.25f, 0.f, 1.f); // fondu de sortie
	auto Vignette = [&](const FLinearColor& Col, float Frac)
	{
		Gradient(0.f, 0.f, W * Frac, H, Col, 0);
		Gradient(W * (1.f - Frac), 0.f, W * Frac, H, Col, 1);
		Gradient(0.f, 0.f, W, H * Frac, Col, 2);
		Gradient(0.f, H * (1.f - Frac), W, H * Frac, Col, 3);
	};
	// v4.5 : la peur vient du modele et de son geste ; l'ecran ne fait que ponctuer (effets divises par deux environ)
	// Une image sombre a l'impact (sauf le Smiler, qui finit sur un eclair)
	// v4.7 : reglage FLASHS : l'image noire et les eclairs s'attenuent ou disparaissent ; le modele reste visible
	const float FlashK = FBRSettings::Get().FlashScale();
	if (K != 0 && bHit && Since < 0.035f)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f * FlashK), 0.f, 0.f, W, H);
	}
	switch (static_cast<EBREntityKind>(K))
	{
	case EBREntityKind::Smiler:
	{
		// Le noir se referme autour du sourire, puis un eclair blanc
		Vignette(FLinearColor(0.f, 0.f, 0.f, 0.75f * Out), 0.32f);
		const float Flash = FMath::Clamp((T - (Dur - 0.35f)) / 0.12f, 0.f, 1.f) * FMath::Clamp((Dur - T) / 0.23f, 0.f, 1.f);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.55f * Flash * FlashK), 0.f, 0.f, W, H);
		break;
	}
	case EBREntityKind::Hound:
	{
		// Trois griffures qui dechirent l'ecran, l'une apres l'autre, et un voile rouge
		if (bHit)
		{
			DrawRect(FLinearColor(0.6f, 0.f, 0.f, 0.18f * FMath::Exp(-Since * 3.f) * FlashK), 0.f, 0.f, W, H);
			for (int32 i = 0; i < 3; ++i)
			{
				const float Appear = Since - i * 0.06f;
				if (Appear <= 0.f)
				{
					continue;
				}
				const float Len = FMath::Min(1.f, Appear / 0.08f);
				const float A = 0.6f * FMath::Clamp(1.4f - Since * 0.9f, 0.f, 1.f) * Out;
				const float X0 = W * (0.66f + i * 0.07f);
				const float Y0 = H * (0.1f + i * 0.04f);
				const float X1 = X0 - W * 0.42f * Len;
				const float Y1 = Y0 + H * 0.78f * Len;
				DrawLine(X0, Y0, X1, Y1, FLinearColor(0.15f, 0.f, 0.f, A), 26.f * U);
				DrawLine(X0, Y0, X1, Y1, FLinearColor(0.75f, 0.05f, 0.03f, A), 12.f * U);
				DrawLine(X0, Y0, X1, Y1, FLinearColor(1.f, 0.6f, 0.5f, 0.6f * A), 3.f * U);
			}
		}
		break;
	}
	case EBREntityKind::Faceling:
	{
		// La neige d'une television qui hurle
		if (bHit && Since < 0.45f)
		{
			const float A = (Since < 0.3f ? 0.35f : 0.35f * (0.45f - Since) / 0.15f) * (0.25f + 0.75f * FlashK);
			const float Cell = FMath::Max(4.f, 7.f * U);
			for (float Y = 0.f; Y < H; Y += Cell)
			{
				for (float X = 0.f; X < W; X += Cell * 3.f)
				{
					const float G = FMath::FRand();
					DrawRect(FLinearColor(G, G, G, A), X, Y, Cell * 3.f, Cell);
				}
			}
			const float Bar = FMath::Fmod(T * 1.7f, 1.f) * H;
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.25f * A), 0.f, Bar, W, 40.f * U);
		}
		else
		{
			Vignette(FLinearColor(0.f, 0.f, 0.f, 0.6f * FMath::Min(1.f, T / Impact) * Out), 0.3f);
		}
		break;
	}
	case EBREntityKind::SkinStealer:
	{
		// La scene vire a la chair : vignette rouge qui bat
		const float Pulse = 0.55f + 0.45f * FMath::Sin(T * 14.f);
		Vignette(FLinearColor(0.45f, 0.02f, 0.01f, 0.45f * Pulse * Out), 0.28f);
		break;
	}
	case EBREntityKind::Deathmoth:
	{
		// Un essaim de papillons de nuit traverse l'ecran
		for (int32 i = 0; i < 16; ++i)
		{
			const float Seed = static_cast<float>(i);
			const float Start = FMath::Frac(Seed * 0.6180339f) * 0.6f;
			const float P = (T - Start) / (0.6f + FMath::Frac(Seed * 0.37f) * 0.6f);
			if (P <= 0.f || P >= 1.f)
			{
				continue;
			}
			const bool bFromLeft = (i % 2) == 0;
			const float X = bFromLeft ? -0.1f * W + P * 1.2f * W : 1.1f * W - P * 1.2f * W;
			const float Y = H * (0.1f + FMath::Frac(Seed * 0.731f) * 0.8f) + FMath::Sin(P * 9.f + Seed) * 60.f * U;
			const float S = (18.f + FMath::Frac(Seed * 0.913f) * 46.f) * U;
			const float Flap = 0.25f + 0.75f * FMath::Abs(FMath::Sin(T * 38.f + Seed));
			const FLinearColor MothC(0.08f, 0.06f, 0.04f, 0.92f * Out);
			RoundRect(X - S * Flap, Y - S * 0.35f, S * Flap, S * 0.7f, S * 0.3f, MothC);
			RoundRect(X, Y - S * 0.35f, S * Flap, S * 0.7f, S * 0.3f, MothC);
			DrawRect(FLinearColor(0.03f, 0.02f, 0.01f, 0.95f * Out), X - S * 0.08f, Y - S * 0.4f, S * 0.16f, S * 0.8f);
		}
		Vignette(FLinearColor(0.12f, 0.08f, 0.03f, 0.35f * Out), 0.22f);
		break;
	}
	case EBREntityKind::Wretch:
	{
		// Images noires entre chaque a-coup
		const float Phase = (T - 0.f) / 0.75f * 5.f;
		if (!bHit && FMath::Frac(Phase) < 0.16f && FlashK > 0.f)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, FlashK), 0.f, 0.f, W, H);
		}
		Vignette(FLinearColor(0.02f, 0.03f, 0.02f, 0.45f * Out), 0.26f);
		break;
	}
	case EBREntityKind::Partygoer:
	{
		if (!bHit)
		{
			// Le silence : l'image s'assombrit un instant
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f * T / FMath::Max(Impact, 0.01f)), 0.f, 0.f, W, H);
			break;
		}
		// Confettis qui jaillissent du centre et retombent
		static const FLinearColor Colors[] = { FLinearColor(1.f, 0.2f, 0.25f), FLinearColor(0.2f, 0.6f, 1.f), FLinearColor(1.f, 0.85f, 0.1f),
			FLinearColor(0.3f, 0.95f, 0.4f), FLinearColor(0.9f, 0.3f, 1.f) };
		for (int32 i = 0; i < 60; ++i)
		{
			const float Seed = static_cast<float>(i);
			const float Ang = FMath::Frac(Seed * 0.6180339f) * 2.f * PI;
			const float Speed = (500.f + FMath::Frac(Seed * 0.377f) * 1100.f) * U;
			const float X = W * 0.5f + FMath::Cos(Ang) * Speed * Since;
			const float Y = H * 0.45f + FMath::Sin(Ang) * Speed * Since + 900.f * U * Since * Since;
			const float S = (8.f + FMath::Frac(Seed * 0.913f) * 10.f) * U;
			const float Flip = FMath::Abs(FMath::Sin(Since * 12.f + Seed));
			FLinearColor Col = Colors[i % UE_ARRAY_COUNT(Colors)];
			Col.A = Out;
			DrawRect(Col, X, Y, S, S * (0.3f + 0.7f * Flip));
		}
		// (le sourire est celui du modele, tout pres : plus de "=)" geant a l'ecran)
		if (Since < 0.9f)
		{
			TextF(BR_STR(NSLOCTEXT("BR", "HUD.JoyeuxAnniversaire", "JOYEUX ANNIVERSAIRE")), W * 0.5f, H * 0.8f, FLinearColor(1.f, 1.f, 1.f, 0.6f * Out * (1.f - Since / 0.9f)), 16.f, EUiWeight::Bold,
				EUiAlign::Center);
		}
		break;
	}
	case EBREntityKind::Clump:
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(T * 18.f);
		Vignette(FLinearColor(0.25f, 0.f, 0.f, (0.3f + 0.2f * Pulse) * Out), 0.3f);
		break;
	}
	case EBREntityKind::Bacteria:
	{
		// L'image se brouille : bandes noires et blanches qui sautent
		const float Amount = FMath::Clamp(T / Dur * 1.4f, 0.f, 1.f);
		const int32 Bars = 3 + static_cast<int32>(Amount * 6.f);
		for (int32 i = 0; i < Bars; ++i)
		{
			const float Y = FMath::FRand() * H;
			const float BH = (2.f + FMath::FRand() * 26.f) * U;
			const bool bWhite = FMath::FRand() < 0.35f;
			DrawRect(bWhite ? FLinearColor(0.9f, 0.9f, 0.95f, 0.5f * Out) : FLinearColor(0.f, 0.f, 0.f, 0.85f * Out), 0.f, Y, W, BH);
		}
		Vignette(FLinearColor(0.f, 0.f, 0.f, 0.7f * Out), 0.3f);
		if (T > Dur - 0.18f)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, W, H);
		}
		break;
	}
	default:
		break;
	}
}

void ABRHUD::DrawContextCues(ABRCharacter* C)
{
	// v4.9 : aucune jauge de statut pendant l'exploration (sante, endurance, sante mentale, oxygene, piles). La sante reste
	// une mecanique (coups, soins, armure, mort, reanimation) ; ses signes sont le coeur, le souffle, la teinte des coups
	// et la vignette. Seul le manque d'air sous l'eau a un message : il demande d'agir tout de suite (remonter).
	if (!C->IsUnderwater() || C->GetBreath() >= 45.f)
	{
		return;
	}
	const float Urgent = FMath::Clamp((45.f - C->GetBreath()) / 45.f, 0.f, 1.f);
	const float Pulse = 0.6f + 0.4f * FMath::Sin(Clock * (3.f + 5.f * Urgent));
	const FString Msg = C->GetBreath() < 20.f ? BR_STR(NSLOCTEXT("BR", "HUD.AirCritical", "PLUS D'AIR : REMONTEZ !"))
		: BR_STR(NSLOCTEXT("BR", "HUD.AirLow", "Manque d'air : remontez respirer"));
	TextF(Msg, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.7f, FLinearColor(0.78f, 0.93f, 1.f, (0.5f + 0.45f * Urgent) * Pulse), 13.f + 3.f * Urgent,
		EUiWeight::Bold, EUiAlign::Center);
}

void ABRHUD::DrawQuickBar(ABRCharacter* C)
{
	// v4.9 : objets rapides brievement apres un changement (ramassage, utilisation, deplacement), toujours, ou masques
	uint32 Sig = 2166136261u;
	for (int32 i = 0; i < ABRCharacter::NumPockets; ++i)
	{
		const FBRItemSlot* It = C->Pockets.IsValidIndex(i) ? &C->Pockets[i] : nullptr;
		Sig = (Sig ^ (It ? static_cast<uint32>(It->Item) * 131u + static_cast<uint32>(It->Count) : 0u)) * 16777619u;
	}
	if (Sig != QuickBarSig)
	{
		QuickBarSig = Sig;
		QuickBarShown = Clock;
	}
	const int32 Mode = FBRSettings::Get().QuickBarMode;
	if (Mode == 2)
	{
		return;
	}
	float Show = 1.f;
	if (Mode == 0)
	{
		const float Age = Clock - QuickBarShown;
		Show = Age < 3.f ? 1.f : FMath::Clamp(1.f - (Age - 3.f) / 0.8f, 0.f, 1.f);
		if (Show <= 0.f)
		{
			return;
		}
	}
	const float A = Show * HudAlpha();
	const float U = Ui();
	const float S = 58.f * U;
	const float G = 8.f * U;
	const float Total = S * ABRCharacter::NumPockets + G * (ABRCharacter::NumPockets - 1);
	const float X0 = (Canvas->ClipX - Total) * 0.5f;
	const float Y = Canvas->ClipY - S - 34.f * U;
	for (int32 i = 0; i < ABRCharacter::NumPockets; ++i)
	{
		const float X = X0 + i * (S + G);
		RoundRect(X, Y, S, S, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.38f * A));
		RoundRect(X, Y, S, S, 10.f * U, FLinearColor(0.95f, 0.78f, 0.25f, 0.24f * A), true);
		TextF(FString::FromInt(i + 1), X + 7.f * U, Y + 4.f * U, WithAlpha(InkDim, InkDim.A * A), 8.5f, EUiWeight::Bold);
		const FBRItemSlot* It = C->Pockets.IsValidIndex(i) ? &C->Pockets[i] : nullptr;
		if (It && !It->IsEmpty())
		{
			Icon(ItemIcon(It->Item), X + S * 0.14f, Y + S * 0.14f, S * 0.72f, S * 0.72f, FLinearColor(1.f, 1.f, 1.f, 0.92f * A));
			if (It->Count > 1)
			{
				TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.XIt", "x{It}"), { { TEXT("It"), BRLoc::Int(It->Count) } }), X + S - 6.f * U, Y + S - 20.f * U, WithAlpha(Ink, Ink.A * A), 9.5f,
					EUiWeight::Bold, EUiAlign::Right);
			}
		}
	}
}

void ABRHUD::DrawObjectiveTracker(ABRWorld* W)
{
	if (!W || W->GetTitleTime() > 0.f || W->IsTransitioning())
	{
		return;
	}
	TArray<FBRObjective> Objs;
	W->GetObjectives(Objs);
	// v4.9 : objectifs brievement a l'arrivee (fin du titre) et a chaque progres, toujours, ou masques (inventaire)
	uint32 Sig = 2166136261u;
	for (const FBRObjective& O : Objs)
	{
		Sig = (Sig ^ static_cast<uint32>(O.Progress * 7 + O.Goal * 131 + (O.IsDone() ? 1 : 0))) * 16777619u;
	}
	const uint32 LevelKey = static_cast<uint32>(W->GetLevelNumber()) * 2654435761u ^ W->GetSeed();
	if (LevelKey != ObjectiveLevelKey || Sig != ObjectiveSig)
	{
		ObjectiveLevelKey = LevelKey;
		ObjectiveSig = Sig;
		ObjectiveShown = Clock;
	}
	const int32 Mode = FBRSettings::Get().ObjectivesMode;
	if (Mode == 2)
	{
		return;
	}
	float Show = 1.f;
	if (Mode == 0)
	{
		const float Age = Clock - ObjectiveShown;
		Show = Age < 8.f ? 1.f : FMath::Clamp(1.f - (Age - 8.f) / 1.2f, 0.f, 1.f);
		if (Show <= 0.f)
		{
			return;
		}
	}
	const float A = Show * HudAlpha();
	const float U = Ui();
	const float RX = Canvas->ClipX - 58.f * U;
	float Y = 110.f * U;
	for (const FBRObjective& O : Objs)
	{
		if (!O.bRequired && O.IsDone())
		{
			continue;
		}
		FLinearColor Col = O.IsDone() ? Done : (O.bRequired ? WithAlpha(Yellow, 0.92f) : WithAlpha(InkDim, 0.8f));
		Col.A *= A;
		const FString Line = O.Goal > 0 ? FString::Printf(TEXT("%s  %d/%d"), *O.Text, O.Progress, O.Goal) : O.Text;
		const FVector2f LS = TextSize(Line, 11.5f, EUiWeight::Regular);
		TextF(Line, RX, Y, Col, 11.5f, O.IsDone() ? EUiWeight::Light : EUiWeight::Regular, EUiAlign::Right);
		const float D = 7.f * U;
		RoundRect(RX + 10.f * U, Y + (LS.Y - D) * 0.5f, D, D, D * 0.5f, Col);
		Y += 24.f * U;
	}
}

void ABRHUD::DrawCrosshair(ABRCharacter* C)
{
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const bool bFocus = !C->GetFocusPrompt().IsEmpty();
	const float S = (bFocus ? 7.f : 4.f) * U;
	const float A = HudAlpha();
	if (C->IsHidden())
	{
		const float Pulse = 0.65f + 0.2f * FMath::Sin(Clock * 2.f);
		TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.Cache", "CACH\u00c9")), CX, Canvas->ClipY - 152.f * U, FLinearColor(0.75f, 0.9f, 1.f, Pulse), 13.f, EUiWeight::Bold, 6.f * U, EUiAlign::Center);
	}
	// v4.9 : reticule selon le reglage : toujours, seulement sur un objet utilisable, ou jamais
	const int32 Mode = FBRSettings::Get().CrosshairMode;
	if (Mode == 0 || (Mode == 1 && bFocus))
	{
		RoundRect(CX - S * 0.5f - 1.f * U, CY - S * 0.5f - 1.f * U, S + 2.f * U, S + 2.f * U, S * 0.5f + 1.f * U, FLinearColor(0.f, 0.f, 0.f, (bFocus ? 0.35f : 0.2f) * A));
		RoundRect(CX - S * 0.5f, CY - S * 0.5f, S, S, S * 0.5f, FLinearColor(1.f, 1.f, 1.f, (bFocus ? 0.92f : 0.5f) * A));
	}
	// Consigne d'interaction et reanimation : toujours affichees, lisibles meme a faible opacite
	const float PA = FMath::Max(A, 0.75f);
	if (bFocus)
	{
		const FString P = C->GetFocusPrompt();
		const FVector2f PS = TextSize(P, 13.f, EUiWeight::Regular);
		const float PH = 32.f * U;
		const float PW = PS.X + 32.f * U;
		RoundRect(CX - PW * 0.5f, CY + 26.f * U, PW, PH, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.42f * PA));
		TextF(P, CX, CY + 26.f * U + (PH - PS.Y) * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.95f * PA), 13.f, EUiWeight::Regular, EUiAlign::Center, false);
	}
	if (C->GetReviveProgress() > 0.f)
	{
		// Progression d'un geste (relever un coequipier) : pas une jauge de statut
		Bar(CX - 130.f * U, CY + 84.f * U, 260.f * U, 8.f * U, C->GetReviveProgress(), FLinearColor(0.55f, 1.f, 0.55f, 0.9f), BR_STR(NSLOCTEXT("BR", "HUD.Reanimation", "R\u00c9ANIMATION")));
	}
}

void ABRHUD::DrawMessages(float Dt)
{
	const float U = Ui();
	for (int32 i = Messages.Num() - 1; i >= 0; --i)
	{
		Messages[i].Age += Dt;
		if (Messages[i].Age > Messages[i].Duration)
		{
			Messages.RemoveAt(i);
		}
	}
	// Notifications : pastilles sombres avec un point de la couleur du message
	float Y = Canvas->ClipY * 0.1f;
	const float MaxW = FMath::Min(1100.f * U, Canvas->ClipX - 120.f * U);
	const float LH = 21.f * U;
	for (const FMsg& M : Messages)
	{
		const float A = FMath::Clamp(M.Duration - M.Age, 0.f, 1.f) * FMath::Clamp(M.Age * 5.f, 0.f, 1.f);
		const TArray<FString> Lines = WrapF(M.Text, MaxW - 70.f * U, 13.5f, EUiWeight::Regular);
		float TW = 0.f;
		for (const FString& L : Lines)
		{
			TW = FMath::Max(TW, TextSize(L, 13.5f, EUiWeight::Regular).X);
		}
		const float PH = Lines.Num() * LH + 18.f * U;
		const float PW = TW + 62.f * U;
		const float PX = (Canvas->ClipX - PW) * 0.5f;
		const float PY = Y - (1.f - EaseOut(M.Age * 4.f)) * 10.f * U;
		RoundRect(PX, PY, PW, PH, FMath::Min(PH * 0.5f, 20.f * U), FLinearColor(0.04f, 0.035f, 0.02f, 0.82f * A));
		RoundRect(PX, PY, PW, PH, FMath::Min(PH * 0.5f, 20.f * U), FLinearColor(M.Color.R, M.Color.G, M.Color.B, 0.3f * A), true);
		const float Dot = 10.f * U;
		RoundRect(PX + 20.f * U, PY + 9.f * U + (LH - Dot) * 0.5f, Dot, Dot, Dot * 0.5f, FLinearColor(M.Color.R, M.Color.G, M.Color.B, A));
		const FLinearColor TC = Mix(FLinearColor(M.Color.R, M.Color.G, M.Color.B, 1.f), FLinearColor::White, 0.35f);
		for (int32 k = 0; k < Lines.Num(); ++k)
		{
			TextF(Lines[k], PX + 42.f * U, PY + 9.f * U + k * LH, WithAlpha(TC, A), 13.5f, EUiWeight::Regular, EUiAlign::Left, false);
		}
		Y += PH + 10.f * U;
	}
}

void ABRHUD::DrawNote(ABRCharacter* C)
{
	const float U = Ui();
	const float W = 760.f * U;
	const float H = 460.f * U;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = (Canvas->ClipY - H) * 0.5f;
	UFont* Medium = GEngine->GetMediumFont();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	DrawRect(FLinearColor(0.86f, 0.83f, 0.71f, 0.97f), X, Y, W, H);
	DrawRect(FLinearColor(0.75f, 0.68f, 0.5f, 0.6f), X, Y, W, 6.f * U);
	Txt(BR_STR(NSLOCTEXT("BR", "HUD.NoteFroissee", "Une note froiss\u00e9e...")), X + 40.f * U, Y + 30.f * U, FLinearColor(0.25f, 0.18f, 0.1f), 1.1f * U, Medium, false, false);
	float LY = Y + 90.f * U;
	for (const FString& L : Wrap(BRKeys::Expand(C->GetOpenNote()), W - 80.f * U, Medium, 1.05f * U))
	{
		TxtLine(L, X + 40.f * U, LY, W - 80.f * U, FLinearColor(0.12f, 0.1f, 0.25f), 1.05f * U, Medium);
		LY += 34.f * U;
	}
	Txt(BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "HUD.InteractRangerNoteResteJournal", "{Interact} Ranger la note  (elle reste dans le journal : {Inventory})"))), X + W * 0.5f, Y + H - 50.f * U, FLinearColor(0.3f, 0.25f, 0.2f), 0.9f * U,
		Medium, true, false);
}

void ABRHUD::DrawDeath(ABRCharacter* C)
{
	const float T = C->GetDeathTime();
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float H = Canvas->ClipY;
	DrawRect(FLinearColor(0.15f, 0.f, 0.f, FMath::Clamp(T / 2.5f, 0.f, 0.85f)), 0.f, 0.f, Canvas->ClipX, H);
	if (T < 0.25f)
	{
		// v4.7 : reglage FLASHS
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.6f * (1.f - T / 0.25f) * FBRSettings::Get().FlashScale()), 0.f, 0.f, Canvas->ClipX, H);
	}
	const float A = FMath::Clamp((T - 0.8f) / 1.f, 0.f, 1.f);
	const ABRPlayerController* OwnerPC = Cast<ABRPlayerController>(PlayerOwner);
	// v4.7 : textes selon la cause explicite de la mort (plus de deduction par la hauteur)
	const EBRDeathCause Cause = C->GetDeathCause();
	FString Detail;
	switch (Cause)
	{
	case EBRDeathCause::Drowning:
		Detail = BR_STR(NSLOCTEXT("BR", "HUD.AvezManqueAir", "Vous avez manqu\u00e9 d'air."));
		break;
	case EBRDeathCause::Fall:
		Detail = BR_STR(NSLOCTEXT("BR", "HUD.EtesTombeFondFosse", "Vous \u00eates tomb\u00e9 au fond d'une fosse."));
		break;
	case EBRDeathCause::Madness:
		Detail = BR_STR(NSLOCTEXT("BR", "HUD.VotreEspritLache", "Votre esprit a l\u00e2ch\u00e9."));
		break;
	default:
		if (C->GetKillerKind() >= 0)
		{
			Detail = BRLoc::Fmt(NSLOCTEXT("BR", "HUD.KilledByTeam", "Abattu par : {Entity}"), { { TEXT("Entity"), BRLoc::Arg(ABREntity::Info(static_cast<EBREntityKind>(C->GetKillerKind())).Name) } });
		}
		break;
	}
	if (OwnerPC && OwnerPC->IsNetGame())
	{
		// Cooperation : a terre, un coequipier peut nous relever (sauf au fond d'une fosse)
		const ABRWorld* W = ABRWorld::Get(this);
		const float Left = W ? FMath::Max(0.f, W->GetDeathTimer()) : 0.f;
		const bool bHelp = W && W->HasLivingTeammate() && BRDeath::CanRevive(Cause);
		const FString Title = Cause == EBRDeathCause::Drowning ? BR_STR(NSLOCTEXT("BR", "HUD.Noye", "NOY\u00c9")) : (Cause == EBRDeathCause::Fall ? BR_STR(NSLOCTEXT("BR", "HUD.Chute", "CHUTE")) : BR_STR(NSLOCTEXT("BR", "HUD.Terre", "\u00c0 TERRE")));
		Txt(Title, CX, H * 0.34f, FLinearColor(0.9f, 0.1f, 0.08f, A), 2.4f * U, GEngine->GetLargeFont(), true);
		if (!Detail.IsEmpty())
		{
			Txt(Detail, CX, H * 0.44f, FLinearColor(1.f, 0.8f, 0.8f, A), 1.1f * U, GEngine->GetMediumFont(), true);
		}
		if (bHelp)
		{
			const FString Hint = Cause == EBRDeathCause::Drowning
				? BRLoc::Fmt(NSLOCTEXT("BR", "HUD.CoequipierDoitSortirEauMaintient", "Un co\u00e9quipier doit vous sortir de l'eau : il maintient {Interact} pr\u00e8s de vous."), { { TEXT("Interact"), BRLoc::Arg(BRKeys::Tag(EBRAction::Interact)) } })
				: BRLoc::Fmt(NSLOCTEXT("BR", "HUD.CoequipierPeutReleverDoitMaintenir", "Un co\u00e9quipier peut vous relever : il doit maintenir {Interact} pr\u00e8s de vous."), { { TEXT("Interact"), BRLoc::Arg(BRKeys::Tag(EBRAction::Interact)) } });
			Txt(Hint, CX, H * 0.52f, FLinearColor(0.75f, 1.f, 0.75f, A), 1.f * U, GEngine->GetMediumFont(), true);
		}
		else if (Cause == EBRDeathCause::Fall)
		{
			Txt(BR_STR(NSLOCTEXT("BR", "HUD.PersonnePeutAtteindreEtes", "Personne ne peut vous atteindre l\u00e0 o\u00f9 vous \u00eates.")), CX, H * 0.52f, FLinearColor(0.85f, 0.85f, 0.85f, A), 1.f * U,
				GEngine->GetMediumFont(), true);
		}
		Txt(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.ReveilPointDepartNiveauValue", "R\u00e9veil au point de d\u00e9part du niveau dans {Value} s      {Jump}  abandonner"), { { TEXT("Value"), BRLoc::Int(FMath::CeilToInt(Left)) }, { TEXT("Jump"), BRLoc::Arg(BRKeys::Tag(EBRAction::Jump)) } }), CX, H * 0.58f, FLinearColor(0.9f, 0.85f, 0.6f, A), 1.f * U, GEngine->GetMediumFont(), true);
		return;
	}
	Txt(BR_STR(NSLOCTEXT("BR", "HUD.EtesMort", "VOUS \u00caTES MORT")), CX, H * 0.38f, FLinearColor(0.9f, 0.1f, 0.08f, A), 2.4f * U, GEngine->GetLargeFont(), true);
	if (!Detail.IsEmpty())
	{
		Txt(Cause == EBRDeathCause::Injury && C->GetKillerKind() >= 0
			? BRLoc::Fmt(NSLOCTEXT("BR", "HUD.KilledBy", "Tu\u00e9 par : {Entity}"), { { TEXT("Entity"), BRLoc::Arg(ABREntity::Info(static_cast<EBREntityKind>(C->GetKillerKind())).Name) } }) : Detail, CX, H * 0.48f, FLinearColor(1.f, 0.8f, 0.8f, A), 1.1f * U,
			GEngine->GetMediumFont(), true);
	}
	const float A2 = FMath::Clamp((T - 2.2f) / 1.f, 0.f, 1.f);
	Txt(BR_STR(NSLOCTEXT("BR", "HUD.ReveillezMoquetteHumideEncore", "Vous vous r\u00e9veillez... sur une moquette humide. Encore.")), CX, H * 0.56f, FLinearColor(0.9f, 0.85f, 0.6f, A2), 1.f * U,
		GEngine->GetMediumFont(), true);
}

void ABRHUD::DrawPause(ABRPlayerController* PC)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	Buttons.Reset();
	PauseTime += UiDt;
	const float In = EaseOut(PauseTime / 0.35f);
	const float X0 = FMath::Max(60.f * U, W * 0.0625f) - (1.f - In) * 30.f * U;

	DrawRect(FLinearColor(0.01f, 0.01f, 0.005f, 0.45f * In), 0.f, 0.f, W, H);
	Gradient(0.f, 0.f, W * 0.62f, H, FLinearColor(0.012f, 0.01f, 0.004f, 0.9f * In), 0);
	Gradient(0.f, H * 0.6f, W, H * 0.4f, FLinearColor(0.f, 0.f, 0.f, 0.7f * In), 3);
	Scanlines(0.03f * In);

	DrawLogo(X0 - 240.f * U * 0.03f, 70.f * U, 240.f * U, In, false);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.Pause", "PAUSE")), X0, 138.f * U, WithAlpha(Ink, In), 44.f, EUiWeight::Black);
	if (PC && PC->IsNetGame())
	{
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		const int32 Count = GS ? GS->PlayerArray.Num() : 1;
		TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.OnlinePlayersPause", "Partie en ligne  \u00b7  {Count} {Count}|plural(one=joueur,other=joueurs)  \u00b7  le jeu continue pendant la pause"), { { TEXT("Count"), BRLoc::Int(Count) } }),
			X0, 214.f * U, FLinearColor(0.7f, 0.95f, 0.7f, In), 13.f, EUiWeight::Light);
		DrawPlayerList();
	}
	else
	{
		TextF(BR_STR(NSLOCTEXT("BR", "HUD.TempsArreteInstant", "Le temps s'est arr\u00eat\u00e9\u2026 pour l'instant.")), X0, 214.f * U, WithAlpha(InkDim, In), 13.f, EUiWeight::Light);
	}
	if (const UBRSaveGame* Save = PC ? PC->GetActiveSave() : nullptr)
	{
		TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.PartieSavenamePlaytimeJeuSauvegarde", "Partie \u00ab {SaveName} \u00bb  \u00b7  {PlayTime} de jeu  \u00b7  sauvegarde automatique"), { { TEXT("SaveName"), BRLoc::Arg(Save->SaveName) }, { TEXT("PlayTime"), BRLoc::Arg(BRSaves::FormatPlayTime(Save->PlayTime)) } }), X0, 236.f * U, WithAlpha(Yellow, 0.8f * In), 11.f, EUiWeight::Regular);
	}

	// Cartes cliquables (meme style que le menu titre)
	const FString Labels[] = { BR_STR(NSLOCTEXT("BR", "HUD.Reprendre", "REPRENDRE")), BR_STR(NSLOCTEXT("BR", "HUD.Parametres", "PARAM\u00c8TRES")), BR_STR(NSLOCTEXT("BR", "HUD.Touches", "TOUCHES")), BR_STR(NSLOCTEXT("BR", "HUD.MenuPrincipal2", "MENU PRINCIPAL")), BR_STR(NSLOCTEXT("BR", "HUD.QuitterJeu", "QUITTER LE JEU")) };
	const FString Subs[] = {
		BR_STR(NSLOCTEXT("BR", "HUD.RetournerBackrooms", "Retourner dans les Backrooms")),
		BR_STR(NSLOCTEXT("BR", "HUD.GraphismesAffichageChatVocal", "Graphismes, son, affichage, chat vocal")),
		BR_STR(NSLOCTEXT("BR", "HUD.ChangerTouchesJusqu3Action", "Changer les touches (jusqu'\u00e0 3 par action)")),
		(PC && PC->IsNetGame()) ? BR_STR(NSLOCTEXT("BR", "HUD.QuitterPartieLigne", "Quitter la partie en ligne")) : BR_STR(NSLOCTEXT("BR", "HUD.QuitterPartieRevenirEcranTitre", "Quitter la partie, revenir \u00e0 l'\u00e9cran titre")),
		BR_STR(NSLOCTEXT("BR", "HUD.FermerJeu", "Fermer le jeu")),
	};
	const TCHAR* Icons[] = { TEXT("UI_IconPlay"), TEXT("UI_IconSettings"), TEXT("UI_IconKeys"), TEXT("UI_IconBack"), TEXT("UI_IconQuit") };
	const int32 Ids[] = { Btn_PauseResume, Btn_PauseSettings, Btn_PauseKeys, Btn_PauseMainMenu, Btn_PauseQuit };
	const float CW = 500.f * U;
	const float CH = 78.f * U;
	float Y = 262.f * U;
	for (int32 i = 0; i < 5; ++i)
	{
		const bool bHov = Hover(X0, Y, CW, CH);
		PauseSel[i] = FMath::FInterpTo(PauseSel[i], bHov ? 1.f : 0.f, UiDt, 14.f);
		DrawCard(X0, Y, CW, CH, PauseSel[i], Labels[i], Subs[i], Icons[i], In * EaseOut((PauseTime - 0.05f * i) / 0.3f), i == 4);
		AddButton(Ids[i], X0, Y, CW, CH);
		Y += CH + 12.f * U;
	}
	HandlePauseMouse(PC);

	TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.Commandes", "COMMANDES")), CX, H - 170.f * U, WithAlpha(Yellow, 0.8f * In), 10.f, EUiWeight::Bold, 3.f * U, EUiAlign::Center);
	TextF(ControlsLine(0), CX, H - 144.f * U, FLinearColor(0.85f, 0.83f, 0.76f, 0.9f * In), 11.5f, EUiWeight::Regular, EUiAlign::Center, false);
	TextF(ControlsLine(1), CX, H - 120.f * U, FLinearColor(0.85f, 0.83f, 0.76f, 0.9f * In), 11.5f, EUiWeight::Regular, EUiAlign::Center, false);
	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(BRKeys::Primary(EBRAction::Pause), BR_STR(NSLOCTEXT("BR", "HUD.Reprendre2", "Reprendre")));
	Hints.Emplace(BRKeys::Primary(EBRAction::Inventory), BR_STR(NSLOCTEXT("BR", "HUD.ParametresTouches", "Param\u00e8tres et touches")));
	Hints.Emplace(BR_STR(NSLOCTEXT("BR", "HUD.Fin", "FIN")), BR_STR(NSLOCTEXT("BR", "HUD.Quitter", "Quitter")));
	KeyHints(CX, H - 84.f * U, Hints, In, true);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.ConsoleTouche2Brlevel37", "Console (touche \u00b2) : BRLevel 37  |  BRGod  |  BRSpawn 0-8  |  BRGiveAll  |  BRBlackout  |  BRObjectives")), CX, H - 38.f * U,
		WithAlpha(InkDim, 0.5f * In), 9.5f, EUiWeight::Light, EUiAlign::Center, false);
}

void ABRHUD::HandlePauseMouse(ABRPlayerController* PC)
{
	if (!PC || !PlayerOwner || !PlayerOwner->WasInputKeyJustPressed(EKeys::LeftMouseButton) || BRDisplay::IsPending())
	{
		return;
	}
	switch (ButtonAt(MouseX, MouseY))
	{
	case Btn_PauseResume:
		PC->TogglePause();
		break;
	case Btn_PauseSettings:
		PC->SetInventoryOpen(true, static_cast<int32>(ETab::Settings));
		break;
	case Btn_PauseKeys:
		PC->SetInventoryOpen(true, static_cast<int32>(ETab::Keys));
		break;
	case Btn_PauseMainMenu:
		PC->ReturnToMainMenu();
		break;
	case Btn_PauseQuit:
		PC->QuitToDesktop();
		break;
	default:
		break;
	}
}

void ABRHUD::DrawKeysTab(ABRPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	const float U = Ui();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float W = FMath::Min(IW, 1240.f * U);
	const float X = IX + (IW - W) * 0.5f;
	Panel(X, IY, W, IH, BR_STR(NSLOCTEXT("BR", "HUD.Touches", "TOUCHES")));

	const int32 Count = BRKeys::NumActions();
	const float Top = IY + 56.f * U;
	const float RowH = FMath::Min(40.f * U, (IH - 140.f * U) / FMath::Max(1, Count));
	const float SlotW = 170.f * U;
	const float SlotGap = 12.f * U;
	const float SlotsX = X + W - 24.f * U - 3.f * SlotW - 2.f * SlotGap;
	// En-tetes de colonnes
	const FString Heads[] = { BR_STR(NSLOCTEXT("BR", "HUD.Touche1", "TOUCHE 1")), BR_STR(NSLOCTEXT("BR", "HUD.Touche2", "TOUCHE 2")), BR_STR(NSLOCTEXT("BR", "HUD.Touche3", "TOUCHE 3")) };
	for (int32 k = 0; k < 3; ++k)
	{
		Txt(Heads[k], SlotsX + k * (SlotW + SlotGap) + SlotW * 0.5f, Top - 4.f * U, InkDim, 0.65f * U, Small, true, false);
	}
	float Y = Top + 18.f * U;
	const bool bBlink = FMath::Fmod(Clock, 0.8f) < 0.5f;
	for (int32 A = 0; A < Count; ++A)
	{
		const EBRAction Act = static_cast<EBRAction>(A);
		const bool bRowHov = Hover(X + 10.f * U, Y, W - 20.f * U, RowH - 4.f * U);
		if (bRowHov)
		{
			DrawRect(FLinearColor(1.f, 0.85f, 0.3f, 0.05f), X + 10.f * U, Y, W - 20.f * U, RowH - 4.f * U);
		}
		Txt(BRKeys::ActionLabel(Act), X + 28.f * U, Y + (RowH - 4.f * U) * 0.5f - 10.f * U, bRowHov ? Yellow : Ink, 0.78f * U, Medium, false, false);
		for (int32 Slot = 0; Slot < BRKeys::SlotsPerAction; ++Slot)
		{
			const float SX = SlotsX + Slot * (SlotW + SlotGap);
			const float SY = Y + 2.f * U;
			const float SH = RowH - 8.f * U;
			const bool bCap = PC->IsCapturingKey() && PC->GetCaptureAction() == A && PC->GetCaptureSlot() == Slot;
			const bool bHov = Hover(SX, SY, SlotW, SH);
			DrawRect(bCap ? WithAlpha(Yellow, bBlink ? 0.85f : 0.45f) : FLinearColor(0.f, 0.f, 0.f, 0.5f), SX, SY, SlotW, SH);
			Frame(SX, SY, SlotW, SH, bHov || bCap ? Yellow : YellowDim, 1.f * U);
			const FKey K = BRKeys::GetKey(Act, Slot);
			const FString Label = bCap ? FString(BR_STR(NSLOCTEXT("BR", "HUD.Appuyez", "APPUYEZ..."))) : BRKeys::KeyName(K);
			const FLinearColor Col = bCap ? FLinearColor(0.05f, 0.04f, 0.01f) : (K.IsValid() ? Ink : WithAlpha(InkDim, 0.5f));
			float LS = 0.72f * U;
			const float LW = TextW(Label, Small, LS);
			if (LW > SlotW - 12.f * U)
			{
				LS *= (SlotW - 12.f * U) / LW;
			}
			Txt(Label, SX + SlotW * 0.5f, SY + SH * 0.5f - 9.f * U, Col, LS, Small, true, false);
			AddButton(Btn_KeySlot + A * BRKeys::SlotsPerAction + Slot, SX, SY, SlotW, SH);
		}
		DrawRect(FLinearColor(0.95f, 0.78f, 0.25f, 0.1f), X + 18.f * U, Y + RowH - 3.f * U, W - 36.f * U, 1.f * U);
		Y += RowH;
	}

	// Bouton de reinitialisation + aide
	const FString ResetLabel(BR_STR(NSLOCTEXT("BR", "HUD.TouchesDefaut", "TOUCHES PAR D\u00c9FAUT")));
	const float RW = TextW(ResetLabel, Medium, 0.8f * U) + 40.f * U;
	const float RH = 36.f * U;
	const float RX = X + 24.f * U;
	const float RY = IY + IH - RH - 16.f * U;
	const bool bHovR = Hover(RX, RY, RW, RH);
	DrawRect(bHovR ? WithAlpha(Yellow, 0.9f) : FLinearColor(0.f, 0.f, 0.f, 0.5f), RX, RY, RW, RH);
	Frame(RX, RY, RW, RH, Yellow, 1.f * U);
	Txt(ResetLabel, RX + RW * 0.5f, RY + 6.f * U, bHovR ? FLinearColor(0.05f, 0.04f, 0.01f) : Yellow, 0.8f * U, Medium, true, false);
	AddButton(Btn_KeysReset, RX, RY, RW, RH);
	const FString Help = PC->IsCapturingKey()
		? FString(BR_STR(NSLOCTEXT("BR", "HUD.AppuyezNouvelleToucheClavierBouton", "Appuyez sur la nouvelle touche (clavier ou bouton de souris).  \u00c9chap : annuler   -   Retour arri\u00e8re : effacer")))
		: FString(BR_STR(NSLOCTEXT("BR", "HUD.CliquezCasePuisAppuyezTouche", "Cliquez sur une case puis appuyez sur une touche. Clic droit : effacer. Les changements s'appliquent imm\u00e9diatement.")));
	Txt(Help, RX + RW + 24.f * U, RY + 8.f * U, PC->IsCapturingKey() ? Yellow : InkDim, 0.72f * U, Small, false, false);
}

void ABRHUD::DrawGlitch(float Amount)
{
	// v4.7 : reglage FLASHS : moins de bandes colorees qui clignotent (noclip, transitions)
	Amount *= 0.2f + 0.8f * FBRSettings::Get().FlashScale();
	const int32 Count = FMath::RoundToInt(Amount * 16.f);
	for (int32 i = 0; i < Count; ++i)
	{
		const float Y = FMath::FRandRange(0.f, Canvas->ClipY);
		const float H = FMath::FRandRange(2.f, 30.f) * Ui();
		const float X = FMath::FRandRange(-200.f, Canvas->ClipX * 0.5f);
		const float W = FMath::FRandRange(Canvas->ClipX * 0.2f, Canvas->ClipX * 1.2f);
		const FLinearColor C(FMath::FRand(), FMath::FRand() * 0.6f, FMath::FRand(), Amount * 0.45f);
		DrawRect(C, X, Y, W, H);
	}
}

// =====================================================================================================================
// Inventaire (TAB)
// =====================================================================================================================

void ABRHUD::DrawInventory(ABRPlayerController* PC, ABRCharacter* C, ABRWorld* W)
{
	const float U = Ui();
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();

	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	Buttons.Reset();
	Slots.Reset();

	// Fond sombre teinte de jaune + lignes de balayage
	DrawRect(FLinearColor(0.025f, 0.022f, 0.006f, 0.84f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	Scanlines(0.07f);

	IW = FMath::Min(Canvas->ClipX - 120.f * U, 1760.f * U);
	IX = (Canvas->ClipX - IW) * 0.5f;
	IY = 165.f * U;
	IH = Canvas->ClipY - IY - 115.f * U;
	const float Gap = IW * 0.02f;
	ColW[0] = IW * 0.27f;
	ColW[1] = IW * 0.42f;
	ColW[2] = IW * 0.27f;
	ColX[0] = IX;
	ColX[1] = ColX[0] + ColW[0] + Gap;
	ColX[2] = ColX[1] + ColW[1] + Gap;

	// En-tete "MENU >" et onglets
	Txt(BR_STR(NSLOCTEXT("BR", "HUD.Menu", "MENU >")), IX, 44.f * U, Yellow, 1.25f * U, Large, false, false);
	DrawRect(YellowDim, IX, 92.f * U, IW, 1.f * U);
	if (PC)
	{
		const int32 Wanted = PC->ConsumeRequestedTab();
		if (Wanted >= 0 && Wanted <= 3)
		{
			Tab = static_cast<ETab>(Wanted);
		}
	}
	if (!C && (Tab == ETab::Character || Tab == ETab::Journal))
	{
		Tab = ETab::Settings;
	}
	const FString TabNames[] = { BR_STR(NSLOCTEXT("BR", "HUD.Personnage", "PERSONNAGE")), BR_STR(NSLOCTEXT("BR", "HUD.Journal", "JOURNAL")), BR_STR(NSLOCTEXT("BR", "HUD.Parametres", "PARAM\u00c8TRES")), BR_STR(NSLOCTEXT("BR", "HUD.Touches", "TOUCHES")) };
	float TX = IX;
	for (int32 i = 0; i < 4; ++i)
	{
		if (!C && i < 2)
		{
			continue; // v4.9 : menu titre : parametres et touches seulement
		}
		const bool bSel = static_cast<int32>(Tab) == i;
		const FString Label = bSel ? FString(TEXT("> ")) + TabNames[i] : FString(TabNames[i]);
		const float LW = TextW(Label, Medium, 0.95f * U);
		const bool bHov = Hover(TX - 6.f * U, 104.f * U, LW + 12.f * U, 34.f * U);
		TextF(Label, TX, 106.f * U, bSel ? Yellow : (bHov ? Ink : InkDim), LegacySize(Medium, 0.95f * U, U), EUiWeight::Regular, EUiAlign::Left, false);
		if (bSel)
		{
			DrawRect(Yellow, TX, 138.f * U, LW, 2.f * U);
		}
		AddButton(Btn_TabCharacter + i, TX - 6.f * U, 104.f * U, LW + 12.f * U, 34.f * U);
		TX += LW + 48.f * U;
	}
	if (W)
	{
		TxtRight(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.NiveauLevelnumberTitle", "NIVEAU {LevelNumber}  \u00ab {Title} \u00bb"), { { TEXT("LevelNumber"), BRLoc::Int(W->GetLevelNumber()) }, { TEXT("Title"), BRLoc::Arg(W->Def().Title) } }), IX + IW, 106.f * U, InkDim, 0.85f * U, Medium);
	}

	switch (Tab)
	{
	case ETab::Character:
		LayoutCharacterTab(C);
		DrawCharacterTab(C, W);
		break;
	case ETab::Journal:
		DrawJournalTab(C, W);
		break;
	case ETab::Settings:
		DrawSettingsTab(PC);
		break;
	case ETab::Keys:
		DrawKeysTab(PC);
		break;
	}

	if (Inspecting.IsValid())
	{
		DrawInspect(C);
	}
	HandleInventoryMouse(PC, C);

	// Pied de page
	const float FY = Canvas->ClipY - 70.f * U;
	DrawRect(YellowDim, IX, FY - 14.f * U, IW, 1.f * U);
	Txt(BR_STR(NSLOCTEXT("BR", "HUD.1992ThresholdSystems", "\u00a9 1992 THRESHOLD SYSTEMS")), IX, FY, InkDim, 0.8f * U, Small, false, false);
	if (Tab == ETab::Character)
	{
		TxtRight(BRKeys::Expand(BR_STR(NSLOCTEXT("BR", "HUD.GlisserDeplacerDoubleClicUtiliser", "[GLISSER]  D\u00c9PLACER     [DOUBLE-CLIC]  UTILISER / \u00c9QUIPER     [CLIC DROIT]  INSPECTER     {Inventory}  FERMER"))), IX + IW, FY,
			InkDim, 0.8f * U, Small);
	}
	else
	{
		TxtRight(Tab == ETab::Keys ? FString(BR_STR(NSLOCTEXT("BR", "HUD.ClicChangerClicDroitEffacer", "[CLIC]  CHANGER     [CLIC DROIT]  EFFACER     [\u00c9CHAP]  ANNULER / FERMER")))
			: FString(BR_STR(NSLOCTEXT("BR", "HUD.ClicChoisirEchapFermer", "[CLIC]  CHOISIR     [\u00c9CHAP]  FERMER"))), IX + IW, FY, InkDim, 0.8f * U, Small);
	}

	// Infobulle et objet en cours de deplacement (au-dessus de tout)
	if (Tab == ETab::Character && !Inspecting.IsValid())
	{
		DrawTooltip(C);
	}
	if (Dragging.IsValid() && C)
	{
		if (const FBRItemSlot* It = C->GetSlot(Dragging.Group, Dragging.Index))
		{
			if (!It->IsEmpty())
			{
				const float S = 84.f * U;
				Icon(ItemIcon(It->Item), MouseX - S * 0.5f, MouseY - S * 0.5f, S, S, FLinearColor(1.f, 1.f, 1.f, 0.85f));
			}
		}
	}
}

void ABRHUD::LayoutCharacterTab(ABRCharacter* C)
{
	const float U = Ui();
	const float G = 10.f * U;

	// Colonne du milieu : poches (4) + stockage (5 x 4)
	const float InnerX = ColX[1] + 52.f * U;
	const float InnerW = ColW[1] - 70.f * U;
	const float Avail = IH - 60.f * U - 34.f * U - 40.f * U - 3.f * G - 30.f * U;
	const float S = FMath::Min((InnerW - 4.f * G) / 5.f, Avail / 5.f);
	float Y = IY + 64.f * U;
	for (int32 i = 0; i < ABRCharacter::NumPockets; ++i)
	{
		FSlotBox B;
		B.Ref.Group = EBRSlotGroup::Pockets;
		B.Ref.Index = i;
		B.X = InnerX + i * (S + G);
		B.Y = Y;
		B.S = S;
		B.Caption = FString::FromInt(i + 1);
		Slots.Add(B);
	}
	Y += S + 40.f * U;
	for (int32 i = 0; i < ABRCharacter::NumStorage; ++i)
	{
		FSlotBox B;
		B.Ref.Group = EBRSlotGroup::Storage;
		B.Ref.Index = i;
		B.X = InnerX + (i % 5) * (S + G);
		B.Y = Y + (i / 5) * (S + G);
		B.S = S;
		Slots.Add(B);
	}

	// Colonne de droite : silhouette + 4 emplacements d'equipement
	const float EqS = FMath::Min(S * 0.9f, 104.f * U);
	const float Margin = 16.f * U;
	SilH = FMath::Min(IH - 90.f * U, (ColW[2] - 2.f * EqS - 4.f * Margin) * 2.f);
	SilW = SilH * 0.5f;
	SilX = ColX[2] + (ColW[2] - SilW) * 0.5f;
	SilY = IY + 60.f * U + (IH - 60.f * U - SilH) * 0.5f;
	const float LeftX = ColX[2] + Margin;
	const float RightX = ColX[2] + ColW[2] - EqS - Margin;
	struct FEq
	{
		EBREquipSlot Slot;
		float X;
		float Y;
	};
	const FEq Eqs[] = {
		{ EBREquipSlot::Head, LeftX, SilY + SilH * 0.02f },
		{ EBREquipSlot::Chest, RightX, SilY + SilH * 0.2f },
		{ EBREquipSlot::Hand, LeftX, SilY + SilH * 0.44f },
		{ EBREquipSlot::Belt, RightX, SilY + SilH * 0.5f },
	};
	for (const FEq& E : Eqs)
	{
		FSlotBox B;
		B.Ref.Group = EBRSlotGroup::Equipment;
		B.Ref.Index = static_cast<int32>(E.Slot);
		B.X = E.X;
		B.Y = E.Y;
		B.S = EqS;
		B.Caption = BRItems::SlotName(E.Slot).ToString();
		Slots.Add(B);
	}
	(void)C;
}

void ABRHUD::DrawSlot(ABRCharacter* C, const FSlotBox& Box)
{
	const float U = Ui();
	UFont* Small = GEngine->GetSmallFont();
	const FBRItemSlot* It = C ? C->GetSlot(Box.Ref.Group, Box.Ref.Index) : nullptr;
	const bool bHov = HoverSlot == Box.Ref && !Inspecting.IsValid();
	const bool bSrc = Dragging == Box.Ref;
	const bool bTarget = Dragging.IsValid() && bHov && !bSrc;
	const float X = Box.X;
	const float Y = Box.Y;
	const float S = Box.S;

	DrawRect(SlotBg, X, Y, S, S);
	DrawRect(FLinearColor(1.f, 0.85f, 0.3f, bHov ? 0.08f : 0.035f), X, Y, S, S * 0.45f);
	Frame(X, Y, S, S, bTarget ? Yellow : (bHov ? WithAlpha(Yellow, 0.85f) : YellowDim), (bHov ? 2.f : 1.f) * U);

	const bool bEmpty = !It || It->IsEmpty();
	if (Box.Ref.Group == EBRSlotGroup::Pockets)
	{
		Txt(Box.Caption, X + 5.f * U, Y + S - 20.f * U, InkDim, 0.7f * U, Small, false, false);
	}
	if (Box.Ref.Group == EBRSlotGroup::Equipment)
	{
		Txt(Box.Caption, X, Y - 22.f * U, Yellow, 0.75f * U, Small, false, false);
		if (bEmpty)
		{
			Txt(BR_STR(NSLOCTEXT("BR", "HUD.Vide", "VIDE")), X + S * 0.5f, Y + S * 0.5f - 9.f * U, WithAlpha(InkDim, 0.5f), 0.7f * U, Small, true, false);
		}
	}
	if (bEmpty)
	{
		return;
	}
	const FBRItemInfo& Info = BRItems::Get(It->Item);
	const float A = bSrc ? 0.3f : 1.f;
	// Nom court au-dessus de l'icone (reduit s'il est trop long)
	float LS = 0.62f * U;
	const float LW = TextW(Info.Short.ToString(), Small, LS);
	if (LW > S - 8.f * U)
	{
		LS *= (S - 8.f * U) / LW;
	}
	Txt(Info.Short.ToString(), X + S * 0.5f, Y + 4.f * U, WithAlpha(Ink, A), LS, Small, true, false);
	UTexture* Tex = ItemIcon(It->Item);
	if (Tex)
	{
		Icon(Tex, X + S * 0.16f, Y + S * 0.22f, S * 0.68f, S * 0.68f, FLinearColor(1.f, 1.f, 1.f, A));
	}
	else
	{
		DrawRect(WithAlpha(YellowDim, A), X + S * 0.3f, Y + S * 0.35f, S * 0.4f, S * 0.4f);
	}
	if (It->Count > 1)
	{
		TxtRight(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.XIt", "x{It}"), { { TEXT("It"), BRLoc::Int(It->Count) } }), X + S - 6.f * U, Y + S - 20.f * U, WithAlpha(Ink, A), 0.75f * U, Small);
	}
}

void ABRHUD::DrawCharacterTab(ABRCharacter* C, ABRWorld* W)
{
	const float U = Ui();
	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();

	// ---------------- Colonne gauche : OBJECTIFS + ETAT (v4.9 : exactement deux jauges, ENDURANCE et SANTE MENTALE)
	const float LX = ColX[0];
	const float LW = ColW[0];
	const float StatusH = FMath::Min(250.f * U, IH * 0.42f);
	const float ObjH = IH - StatusH - 18.f * U;
	Panel(LX, IY, LW, ObjH, BR_STR(NSLOCTEXT("BR", "HUD.Objectifs", "OBJECTIFS")));
	if (W)
	{
		TArray<FBRObjective> Objs;
		W->GetObjectives(Objs);
		const float Top = IY + 56.f * U;
		const float Bottom = IY + ObjH - 12.f * U;
		const float TextW0 = LW - 74.f * U;
		const float LineH = 20.f * U;
		const FString Footer = W->Def().bRequireObjectives ? (W->AreObjectivesComplete()
			? BR_STR(NSLOCTEXT("BR", "HUD.SortiesSontStablesTrouvezMur", "Les sorties sont stables : trouvez un mur qui gr\u00e9sille."))
			: BR_STR(NSLOCTEXT("BR", "HUD.ObjectifsRequisStabiliserSortiesNiveau", "Objectifs requis pour stabiliser les sorties du niveau."))) : FString();
		// v4.9 : texte coupe sur plusieurs lignes (langues aux mots longs) plutot que reduit ; la molette fait defiler
		auto Layout = [&](bool bDraw, float Offset) -> float
		{
			float Y = Top + Offset;
			auto Visible = [&](float LY, float H) { return bDraw && LY >= Top - 1.f && LY + H <= Bottom + 1.f; };
			for (const FBRObjective& O : Objs)
			{
				const FLinearColor Col = O.IsDone() ? Done : (O.bRequired ? Ink : InkDim);
				const FString Count = O.Goal > 0 ? FString::Printf(TEXT("%d/%d"), O.Progress, O.Goal) : FString();
				const TArray<FString> Lines = Wrap(O.Text, TextW0, Small, 0.78f * U);
				for (int32 k = 0; k < Lines.Num(); ++k)
				{
					if (Visible(Y, LineH))
					{
						if (k == 0)
						{
							DrawRect(O.bRequired ? Yellow : YellowDim, LX + 18.f * U, Y + 6.f * U, 6.f * U, 6.f * U);
							TxtRight(Count, LX + LW - 18.f * U, Y, Col, 0.8f * U, Small);
						}
						TxtLine(Lines[k], LX + 32.f * U, Y, TextW0, Col, 0.78f * U, Small);
					}
					Y += LineH;
				}
				if (!O.IsDone() && O.Partial > 0.01f)
				{
					if (Visible(Y, 6.f * U))
					{
						DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), LX + 32.f * U, Y, LW - 50.f * U, 4.f * U);
						DrawRect(Yellow, LX + 32.f * U, Y, (LW - 50.f * U) * O.Partial, 4.f * U);
					}
					Y += 10.f * U;
				}
				Y += 8.f * U;
			}
			if (!Footer.IsEmpty())
			{
				Y += 8.f * U;
				for (const FString& L : Wrap(Footer, LW - 40.f * U, Small, 0.72f * U))
				{
					if (Visible(Y, 18.f * U))
					{
						TxtLine(L, LX + 18.f * U, Y, LW - 40.f * U, W->AreObjectivesComplete() ? Done : InkDim, 0.72f * U, Small);
					}
					Y += 18.f * U;
				}
			}
			return Y - (Top + Offset);
		};
		const float ContentH = Layout(false, 0.f);
		Layout(true, ScrollArea(ObjectiveScroll, LX, Top, LW, Bottom - Top, ContentH));
	}
	DrawStatusPanel(C, LX, IY + ObjH + 18.f * U, LW, StatusH);

	// ---------------- Colonne du milieu : INVENTAIRE
	Panel(ColX[1], IY, ColW[1], IH, BR_STR(NSLOCTEXT("BR", "HUD.Inventaire", "INVENTAIRE")));
	const FSlotBox* FirstPocket = nullptr;
	const FSlotBox* FirstStorage = nullptr;
	for (const FSlotBox& B : Slots)
	{
		if (B.Ref.Group == EBRSlotGroup::Pockets && !FirstPocket)
		{
			FirstPocket = &B;
		}
		if (B.Ref.Group == EBRSlotGroup::Storage && !FirstStorage)
		{
			FirstStorage = &B;
		}
	}
	if (FirstPocket)
	{
		VLabel(BR_STR(NSLOCTEXT("BR", "HUD.Poches", "POCHES")), ColX[1] + 28.f * U, FirstPocket->Y, Yellow, 0.75f * U);
	}
	if (FirstStorage)
	{
		VLabel(BR_STR(NSLOCTEXT("BR", "HUD.Stockage", "STOCKAGE")), ColX[1] + 28.f * U, FirstStorage->Y, Yellow, 0.75f * U);
	}

	// ---------------- Colonne de droite : EQUIPEMENT
	Panel(ColX[2], IY, ColW[2], IH, BR_STR(NSLOCTEXT("BR", "HUD.Equipement", "\u00c9QUIPEMENT")));
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		Icon(A->Icon(TEXT("I_Silhouette")), SilX, SilY, SilW, SilH, FLinearColor(0.85f, 0.82f, 0.72f, 0.95f));
	}
	// Traits reliant chaque emplacement au corps
	for (const FSlotBox& B : Slots)
	{
		if (B.Ref.Group != EBRSlotGroup::Equipment)
		{
			continue;
		}
		FVector2D Body(SilX + SilW * 0.5f, SilY + SilH * 0.1f);
		switch (static_cast<EBREquipSlot>(B.Ref.Index))
		{
		case EBREquipSlot::Chest:
			Body = FVector2D(SilX + SilW * 0.55f, SilY + SilH * 0.3f);
			break;
		case EBREquipSlot::Hand:
			Body = FVector2D(SilX + SilW * 0.16f, SilY + SilH * 0.5f);
			break;
		case EBREquipSlot::Belt:
			Body = FVector2D(SilX + SilW * 0.6f, SilY + SilH * 0.5f);
			break;
		default:
			break;
		}
		const bool bLeft = B.X < SilX;
		const float SX = bLeft ? B.X + B.S : B.X;
		const float SY = B.Y + B.S * 0.5f;
		DrawLine(SX, SY, Body.X, Body.Y, YellowDim, 1.5f * U);
		DrawRect(Yellow, Body.X - 3.f * U, Body.Y - 3.f * U, 6.f * U, 6.f * U);
	}

	for (const FSlotBox& B : Slots)
	{
		DrawSlot(C, B);
	}
	(void)Medium;
}

void ABRHUD::DrawStatusPanel(ABRCharacter* C, float X, float Y, float W, float H)
{
	Panel(X, Y, W, H, BR_STR(NSLOCTEXT("BR", "HUD.StatusPanel", "\u00c9TAT")));
	if (!C)
	{
		return;
	}
	const float U = Ui();
	const float Stam = FMath::Clamp(C->Stamina, 0.f, 100.f);
	const float San = FMath::Clamp(C->Sanity, 0.f, 100.f);
	// Un mot d'etat en plus du pourcentage : lisible d'un coup d'oeil, sans dependre de la couleur
	const FString StamState = Stam > 60.f ? BR_STR(NSLOCTEXT("BR", "HUD.StaminaRested", "repos\u00e9"))
		: (Stam > 25.f ? BR_STR(NSLOCTEXT("BR", "HUD.StaminaWinded", "essouffl\u00e9")) : BR_STR(NSLOCTEXT("BR", "HUD.StaminaSpent", "\u00e0 bout de souffle")));
	const FString SanState = San > 65.f ? BR_STR(NSLOCTEXT("BR", "HUD.SanityStable", "stable"))
		: (San > 30.f ? BR_STR(NSLOCTEXT("BR", "HUD.SanityShaken", "nerveux")) : BR_STR(NSLOCTEXT("BR", "HUD.SanityBreaking", "au bord de la rupture")));
	const float Row = (H - 70.f * U) * 0.5f;
	DrawStatusGauge(0, BR_STR(NSLOCTEXT("BR", "HUD.Endurance", "ENDURANCE")), StamState, Stam, C->GetStaminaTrend(), X + 18.f * U, Y + 60.f * U, W - 36.f * U);
	DrawStatusGauge(1, BR_STR(NSLOCTEXT("BR", "HUD.SanteMentale", "SANT\u00c9 MENTALE")), SanState, San, C->GetSanityTrend(), X + 18.f * U, Y + 60.f * U + Row, W - 36.f * U);
}

void ABRHUD::DrawStatusGauge(int32 Kind, const FString& Label, const FString& State, float Value, float Trend, float X, float Y, float W)
{
	++StatusGaugesDrawn;
	const float U = Ui();
	const bool bRTL = BRLoc::IsRightToLeft();
	const float IconS = 24.f * U;
	const float IconX = bRTL ? X + W - IconS : X;
	const FLinearColor IconCol = WithAlpha(Ink, 0.9f);
	// Icone (forme distincte) : chevrons de course pour l'endurance, oeil pour la sante mentale
	if (Kind == 0)
	{
		for (int32 k = 0; k < 3; ++k)
		{
			const float CXk = IconX + 4.f * U + k * 7.f * U;
			DrawLine(CXk, Y + 4.f * U, CXk + 6.f * U, Y + IconS * 0.5f, IconCol, 2.f * U);
			DrawLine(CXk + 6.f * U, Y + IconS * 0.5f, CXk, Y + IconS - 4.f * U, IconCol, 2.f * U);
		}
	}
	else
	{
		const float EX = IconX + IconS * 0.5f;
		const float EY = Y + IconS * 0.5f;
		FVector2D Prev(EX - IconS * 0.48f, EY);
		for (int32 k = 1; k <= 16; ++k)
		{
			const float T = k / 16.f * 2.f * PI;
			const FVector2D P(EX - IconS * 0.48f * FMath::Cos(T), EY - IconS * 0.26f * FMath::Sin(T) * (FMath::Sin(T) > 0.f ? 1.f : 0.8f));
			DrawLine(Prev.X, Prev.Y, P.X, P.Y, IconCol, 1.6f * U);
			Prev = P;
		}
		RoundRect(EX - 3.5f * U, EY - 3.5f * U, 7.f * U, 7.f * U, 3.5f * U, IconCol);
	}
	// Libelle, etat, valeur
	const float TX = bRTL ? IconX - 10.f * U : X + IconS + 10.f * U;
	const EUiAlign Near = bRTL ? EUiAlign::Right : EUiAlign::Left;
	const EUiAlign Far = bRTL ? EUiAlign::Left : EUiAlign::Right;
	const float FarX = bRTL ? X : X + W;
	const FString Pct = FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Value));
	const float PctW = TextSize(Pct, 13.f, EUiWeight::Bold).X;
	TextFit(Label, TX, Y - 1.f * U, W - IconS - PctW - 60.f * U, Ink, 12.5f, EUiWeight::Bold, Near, false);
	TextF(Pct, FarX, Y - 1.f * U, Yellow, 13.f, EUiWeight::Bold, Far, false);
	TextFit(State, TX, Y + 19.f * U, W - IconS - 60.f * U, InkDim, 10.5f, EUiWeight::Regular, Near, false);
	const float Box = 20.f * U;
	TrendBox(bRTL ? X + PctW + 10.f * U : X + W - PctW - Box - 10.f * U, Y + 18.f * U, Box, Trend);
	// Barre : 10 segments (endurance) ou barre continue a reperes (sante mentale) ; en arabe et en persan, de droite a gauche
	const float BY = Y + 44.f * U;
	const float BH = 12.f * U;
	const float K = FMath::Clamp(Value / 100.f, 0.f, 1.f);
	if (Kind == 0)
	{
		const float Gap = 3.f * U;
		const float SegW = (W - 9.f * Gap) / 10.f;
		const FLinearColor Fill(0.93f, 0.9f, 0.8f, 0.95f);
		for (int32 k = 0; k < 10; ++k)
		{
			const int32 Slot = bRTL ? 9 - k : k;
			const float SX = X + Slot * (SegW + Gap);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), SX, BY, SegW, BH);
			const float Part = FMath::Clamp(K * 10.f - k, 0.f, 1.f);
			if (Part > 0.f)
			{
				DrawRect(Fill, bRTL ? SX + SegW * (1.f - Part) : SX, BY, SegW * Part, BH);
			}
		}
	}
	else
	{
		const FLinearColor Fill = FMath::Lerp(FLinearColor(0.82f, 0.28f, 0.18f, 0.95f), FLinearColor(0.95f, 0.78f, 0.3f, 0.95f), FMath::Clamp((K - 0.2f) / 0.5f, 0.f, 1.f));
		RoundRect(X, BY, W, BH, BH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		const float FW = FMath::Max(BH, W * K);
		if (K > 0.f)
		{
			RoundRect(bRTL ? X + W - FW : X, BY, FW, BH, BH * 0.5f, Fill);
		}
		for (int32 k = 1; k < 4; ++k)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X + W * k / 4.f, BY - 2.f * U, 2.f * U, BH + 4.f * U);
		}
	}
}

void ABRHUD::DrawTooltip(ABRCharacter* C)
{
	if (!C || !HoverSlot.IsValid() || Dragging.IsValid())
	{
		return;
	}
	const FBRItemSlot* It = C->GetSlot(HoverSlot.Group, HoverSlot.Index);
	if (!It || It->IsEmpty())
	{
		return;
	}
	const float U = Ui();
	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();
	const FBRItemInfo& Info = BRItems::Get(It->Item);
	const float W = 380.f * U;
	TArray<FString> Lines = Wrap(BRKeys::Expand(Info.Description.ToString()), W - 28.f * U, Small, 0.72f * U);
	const FString Charge = ChargeLine(C, It->Item);
	if (!Charge.IsEmpty())
	{
		Lines.Append(Wrap(Charge, W - 28.f * U, Small, 0.72f * U));
	}
	FString Hint;
	if (Info.bConsumable)
	{
		Hint = BRLoc::Fmt(NSLOCTEXT("BR", "HUD.DoubleClicUseverb", "[DOUBLE-CLIC]  {UseVerb}"), { { TEXT("UseVerb"), BRLoc::Arg(Info.UseVerb.ToUpper()) } });
	}
	else if (Info.Slot != EBREquipSlot::None)
	{
		Hint = HoverSlot.Group == EBRSlotGroup::Equipment ? BR_STR(NSLOCTEXT("BR", "HUD.DoubleClicRetirer", "[DOUBLE-CLIC]  RETIRER")) : BR_STR(NSLOCTEXT("BR", "HUD.DoubleClicEquiper", "[DOUBLE-CLIC]  \u00c9QUIPER"));
	}
	const float H = 52.f * U + Lines.Num() * 19.f * U + (Hint.IsEmpty() ? 0.f : 26.f * U);
	float X = MouseX + 20.f * U;
	float Y = MouseY + 20.f * U;
	X = FMath::Min(X, Canvas->ClipX - W - 10.f * U);
	Y = FMath::Min(Y, Canvas->ClipY - H - 10.f * U);
	DrawRect(FLinearColor(0.02f, 0.018f, 0.008f, 0.95f), X, Y, W, H);
	Frame(X, Y, W, H, YellowDim, 1.f * U);
	Txt(FString::Printf(TEXT("%s%s"), *Info.Name.ToUpper().ToString(), It->Count > 1 ? *BRLoc::Fmt(NSLOCTEXT("BR", "HUD.XIt2", "  x{It}"), { { TEXT("It"), BRLoc::Int(It->Count) } }) : TEXT("")), X + 14.f * U,
		Y + 10.f * U, Yellow, 0.85f * U, Medium, false, false);
	float LY = Y + 44.f * U;
	for (const FString& L : Lines)
	{
		TxtLine(L, X + 14.f * U, LY, W - 28.f * U, Ink, 0.72f * U, Small);
		LY += 19.f * U;
	}
	if (!Hint.IsEmpty())
	{
		Txt(Hint, X + 14.f * U, LY + 6.f * U, InkDim, 0.68f * U, Small, false, false);
	}
}

void ABRHUD::DrawInspect(ABRCharacter* C)
{
	const FBRItemSlot* It = C ? C->GetSlot(Inspecting.Group, Inspecting.Index) : nullptr;
	if (!It || It->IsEmpty())
	{
		Inspecting = FSlotRef();
		return;
	}
	const float U = Ui();
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const FBRItemInfo& Info = BRItems::Get(It->Item);
	const float W = 760.f * U;
	const float H = 340.f * U;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = (Canvas->ClipY - H) * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	Panel(X, Y, W, H, BR_STR(NSLOCTEXT("BR", "HUD.Inspecter", "INSPECTER")));
	const float IconS = 220.f * U;
	DrawRect(SlotBg, X + 24.f * U, Y + 66.f * U, IconS, IconS);
	Frame(X + 24.f * U, Y + 66.f * U, IconS, IconS, YellowDim, 1.f * U);
	// Petit balancement de l'objet inspecte
	const float Bob = FMath::Sin(Clock * 1.7f) * 4.f * U;
	Icon(ItemIcon(It->Item), X + 34.f * U, Y + 76.f * U + Bob, IconS - 20.f * U, IconS - 20.f * U, FLinearColor::White);

	const float TX = X + IconS + 50.f * U;
	const float TW = W - IconS - 74.f * U;
	Txt(Info.Name.ToUpper().ToString(), TX, Y + 64.f * U, Yellow, 1.05f * U, Large, false, false);
	FString Kind = Info.bConsumable ? BR_STR(NSLOCTEXT("BR", "HUD.Consommable", "CONSOMMABLE")) : BR_STR(NSLOCTEXT("BR", "HUD.Objet", "OBJET"));
	if (Info.Slot != EBREquipSlot::None)
	{
		Kind = BRLoc::Fmt(NSLOCTEXT("BR", "HUD.EquipmentSlot", "\u00c9QUIPEMENT - {Slot}"), { { TEXT("Slot"), BRLoc::Arg(BRItems::SlotName(Info.Slot)) } });
	}
	Txt(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.KindQuantiteIt", "{Kind}     QUANTIT\u00c9 : {It}"), { { TEXT("Kind"), BRLoc::Arg(Kind) }, { TEXT("It"), BRLoc::Int(It->Count) } }), TX, Y + 112.f * U, InkDim, 0.75f * U, Small, false, false);
	float LY = Y + 146.f * U;
	// v4.9 : charge des piles (lampe, frontale, camescope, piles) : information de l'objet
	const FString Charge = ChargeLine(C, It->Item);
	if (!Charge.IsEmpty())
	{
		for (const FString& L : Wrap(Charge, TW, Small, 0.78f * U))
		{
			TxtLine(L, TX, LY - 6.f * U, TW, Yellow, 0.78f * U, Small);
			LY += 20.f * U;
		}
		LY += 6.f * U;
	}
	for (const FString& L : Wrap(BRKeys::Expand(Info.Description.ToString()), TW, Medium, 0.8f * U))
	{
		TxtLine(L, TX, LY, TW, Ink, 0.8f * U, Medium);
		LY += 24.f * U;
	}

	// Boutons
	const float BH = 38.f * U;
	const float BY = Y + H - BH - 20.f * U;
	FString UseLabel;
	if (Info.bConsumable)
	{
		UseLabel = Info.UseVerb.ToUpper().ToString();
	}
	else if (Info.Slot != EBREquipSlot::None)
	{
		UseLabel = Inspecting.Group == EBRSlotGroup::Equipment ? BR_STR(NSLOCTEXT("BR", "HUD.Retirer", "RETIRER")) : BR_STR(NSLOCTEXT("BR", "HUD.Equiper", "\u00c9QUIPER"));
	}
	float BX = TX;
	if (!UseLabel.IsEmpty())
	{
		const float BW = TextW(UseLabel, Medium, 0.85f * U) + 40.f * U;
		const bool bHov = Hover(BX, BY, BW, BH);
		DrawRect(bHov ? WithAlpha(Yellow, 0.9f) : FLinearColor(0.f, 0.f, 0.f, 0.5f), BX, BY, BW, BH);
		Frame(BX, BY, BW, BH, Yellow, 1.f * U);
		Txt(UseLabel, BX + BW * 0.5f, BY + 6.f * U, bHov ? FLinearColor(0.05f, 0.04f, 0.01f) : Yellow, 0.85f * U, Medium, true, false);
		AddButton(Btn_InspectUse, BX, BY, BW, BH);
		BX += BW + 16.f * U;
	}
	const FString CloseLabel(BR_STR(NSLOCTEXT("BR", "HUD.Fermer", "FERMER")));
	const float CW = TextW(CloseLabel, Medium, 0.85f * U) + 40.f * U;
	const bool bHovC = Hover(BX, BY, CW, BH);
	DrawRect(bHovC ? WithAlpha(Ink, 0.85f) : FLinearColor(0.f, 0.f, 0.f, 0.5f), BX, BY, CW, BH);
	Frame(BX, BY, CW, BH, InkDim, 1.f * U);
	Txt(CloseLabel, BX + CW * 0.5f, BY + 6.f * U, bHovC ? FLinearColor(0.05f, 0.04f, 0.01f) : Ink, 0.85f * U, Medium, true, false);
	AddButton(Btn_InspectClose, BX, BY, CW, BH);
}

void ABRHUD::DrawJournalTab(ABRCharacter* C, ABRWorld* W)
{
	if (!W)
	{
		return;
	}
	const float U = Ui();
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const FBRLevelDef& D = W->Def();
	const float Gap = IW * 0.02f;
	const float LW = IW * 0.49f;
	const float RX = IX + LW + Gap;
	const float RW = IW - LW - Gap;

	// ---- Niveau actuel + notes
	Panel(IX, IY, LW, IH, BR_STR(NSLOCTEXT("BR", "HUD.JournalVagabond", "JOURNAL DU VAGABOND")));
	float X = IX + 20.f * U;
	float Y = IY + 60.f * U;
	const float ColWidth = LW - 40.f * U;
	Txt(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.NiveauNumberTitle2", "NIVEAU {Number} - \u00ab {Title} \u00bb"), { { TEXT("Number"), BRLoc::Int(D.Number) }, { TEXT("Title"), BRLoc::Arg(D.Title) } }), X, Y, Ink, 0.95f * U, Large, false, false);
	Y += 40.f * U;
	Txt(D.Nickname.ToString(), X, Y, InkDim, 0.85f * U, Medium, false, false);
	Y += 26.f * U;
	Txt(D.ClassText.ToString(), X, Y, ClassColor(D.SurvivalClass), 0.85f * U, Medium, false, false);
	Y += 32.f * U;
	for (const FString& L : Wrap(D.Description.ToString(), ColWidth, Medium, 0.8f * U))
	{
		TxtLine(L, X, Y, ColWidth, Ink, 0.8f * U, Medium);
		Y += 23.f * U;
	}
	Y += 12.f * U;
	FString Visited;
	for (int32 N : W->GetVisitedLevels())
	{
		Visited += Visited.IsEmpty() ? FString::FromInt(N) : FString(TEXT(", ")) + FString::FromInt(N);
	}
	Txt(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.LevelsVisited", "Niveaux visit\u00e9s : {List}"), { { TEXT("List"), BRLoc::Arg(Visited) } }), X, Y, FLinearColor(0.7f, 0.8f, 0.9f), 0.8f * U, Medium, false, false);
	Y += 26.f * U;
	Txt(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.TempsNiveauLeveltime", "Temps sur ce niveau : {LevelTime}"), { { TEXT("LevelTime"), BRLoc::Arg(Timecode(W->GetLevelTime())) } }), X, Y, FLinearColor(0.7f, 0.8f, 0.9f), 0.8f * U, Medium,
		false, false);
	Y += 26.f * U;
	for (const FString& L : Wrap(BR_STR(NSLOCTEXT("BR", "HUD.QuitterNiveauCherchezPassageMur", "Pour quitter un niveau : cherchez un passage (mur qui gr\u00e9sille, porte, \u00e9chelle...). Les sorties \u00e9mettent un bourdonnement \u00e9lectrique : \u00e9coutez.")), ColWidth, Small, 0.75f * U))
	{
		TxtLine(L, X, Y, ColWidth, Yellow, 0.75f * U, Small);
		Y += 20.f * U;
	}
	Y += 14.f * U;
	if (C)
	{
		Txt(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.NotesTrouveesReadnotes", "NOTES TROUV\u00c9ES ({ReadNotes})"), { { TEXT("ReadNotes"), BRLoc::Int(C->ReadNotes.Num()) } }), X, Y, Yellow, 0.85f * U, Medium, false, false);
		Y += 30.f * U;
		// v4.9 : toutes les notes, a la molette (avant : coupees en bas du panneau)
		TArray<TPair<FString, float>> NoteLines;
		float ContentH = 0.f;
		for (int32 i = C->ReadNotes.Num() - 1; i >= 0; --i)
		{
			const TArray<FString> Wrapped = Wrap(BRKeys::Expand(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.QuotedNote", "\u00ab {Note} \u00bb"),
				{ { TEXT("Note"), BRLoc::Arg(BRLevels::NoteText(C->ReadNotes[i])) } })), ColWidth - 20.f * U, Small, 0.7f * U);
			for (int32 k = 0; k < Wrapped.Num(); ++k)
			{
				const float After = k == Wrapped.Num() - 1 ? 8.f * U : 0.f;
				NoteLines.Add(TPair<FString, float>(Wrapped[k], After));
				ContentH += 18.f * U + After;
			}
		}
		const float Top = Y;
		const float Bottom = IY + IH - 16.f * U;
		float LY = Top + ScrollArea(JournalScroll[0], X, Top, ColWidth, Bottom - Top, ContentH);
		for (const TPair<FString, float>& L : NoteLines)
		{
			if (LY >= Top - 1.f && LY + 18.f * U <= Bottom + 1.f)
			{
				TxtLine(L.Key, X, LY, ColWidth - 20.f * U, InkDim, 0.7f * U, Small);
			}
			LY += 18.f * U + L.Value;
		}
	}

	// ---- Entites
	Panel(RX, IY, RW, IH, BR_STR(NSLOCTEXT("BR", "HUD.EntitesRencontrees", "ENTIT\u00c9S RENCONTR\u00c9ES")));
	X = RX + 20.f * U;
	Y = IY + 60.f * U;
	const float EW = RW - 40.f * U;
	bool bAny = false;
	{
		// v4.9 : fiches a la molette (avant : coupees en bas du panneau)
		struct FLine
		{
			FString Text;
			FLinearColor Color;
			bool bHeader;
			float After;
		};
		TArray<FLine> Lines;
		float ContentH = 0.f;
		for (int32 Kd = 0; Kd < static_cast<int32>(EBREntityKind::Count); ++Kd)
		{
			const EBREntityKind Kind = static_cast<EBREntityKind>(Kd);
			if (!W->IsDiscovered(Kind))
			{
				continue;
			}
			bAny = true;
			const FBREntityInfo& Info = ABREntity::Info(Kind);
			Lines.Add({ BRLoc::Fmt(NSLOCTEXT("BR", "HUD.EntityNumberName", "{Number} - {Name}"), { { TEXT("Number"), BRLoc::Arg(Info.Number) }, { TEXT("Name"), BRLoc::Arg(Info.Name) } }),
				FLinearColor(1.f, 0.6f, 0.5f), true, 0.f });
			ContentH += 28.f * U;
			for (const FString& L : Wrap(Info.Description.ToString(), EW - 20.f * U, Small, 0.72f * U))
			{
				Lines.Add({ L, Ink, false, 0.f });
				ContentH += 19.f * U;
			}
			const TArray<FString> Advice = Wrap(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.AdviceLine", "Conseil : {Advice}"), { { TEXT("Advice"), BRLoc::Arg(Info.Advice) } }), EW - 20.f * U, Small, 0.72f * U);
			for (int32 k = 0; k < Advice.Num(); ++k)
			{
				const float After = k == Advice.Num() - 1 ? 12.f * U : 0.f;
				Lines.Add({ Advice[k], Done, false, After });
				ContentH += 19.f * U + After;
			}
		}
		const float Top = Y;
		const float Bottom = IY + IH - 16.f * U;
		float LY = Top + ScrollArea(JournalScroll[1], RX, Top, RW, Bottom - Top, ContentH);
		for (const FLine& L : Lines)
		{
			const float LH = L.bHeader ? 28.f * U : 19.f * U;
			if (LY >= Top - 1.f && LY + LH <= Bottom + 1.f)
			{
				if (L.bHeader)
				{
					Txt(L.Text, X, LY, L.Color, 0.9f * U, Medium, false, false);
				}
				else
				{
					TxtLine(L.Text, X + 12.f * U, LY, EW - 20.f * U, L.Color, 0.72f * U, Small);
				}
			}
			LY += LH + L.After;
		}
	}
	if (!bAny)
	{
		Txt(BR_STR(NSLOCTEXT("BR", "HUD.AucuneInstantTresBienComme", "Aucune pour l'instant... et c'est tr\u00e8s bien comme \u00e7a.")), X, Y, InkDim, 0.8f * U, Medium, false, false);
		Y += 30.f * U;
		for (const FString& L : Wrap(BR_STR(NSLOCTEXT("BR", "HUD.AstuceFilmerEntiteCamescopePendant", "Astuce : filmer une entit\u00e9 au cam\u00e9scope pendant 3 secondes ajoute sa fiche au journal.")), EW, Small, 0.72f * U))
		{
			TxtLine(L, X, Y, EW, InkDim, 0.72f * U, Small);
			Y += 19.f * U;
		}
	}
}

void ABRHUD::DrawSettingsTab(ABRPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	const float U = Ui();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float PanelW = FMath::Min(IW, 1760.f * U);
	const float PanelX = IX + (IW - PanelW) * 0.5f;
	Panel(PanelX, IY, PanelW, IH, BR_STR(NSLOCTEXT("BR", "HUD.Parametres", "PARAM\u00c8TRES")));

	// v4.9 : quatre categories (JEU, VIDEO, INTERFACE, GRAPHISMES), puis leurs lignes sur une ou deux colonnes
	const FString CatNames[] = { BR_STR(NSLOCTEXT("BR", "HUD.SettingsGame", "JEU")), BR_STR(NSLOCTEXT("BR", "HUD.SettingsVideo", "VID\u00c9O")),
		BR_STR(NSLOCTEXT("BR", "HUD.SettingsInterface", "INTERFACE")), BR_STR(NSLOCTEXT("BR", "HUD.SettingsGraphics", "GRAPHISMES")) };
	SettingsCategory = FMath::Clamp(SettingsCategory, 0, 3);
	float CatX = PanelX + 28.f * U;
	const float CatY = IY + 52.f * U;
	for (int32 c = 0; c < 4; ++c)
	{
		const bool bSel = SettingsCategory == c;
		const float CW = TextSize(CatNames[c], 12.f, EUiWeight::Bold).X + 28.f * U;
		const bool bHovCat = Hover(CatX, CatY, CW, 32.f * U);
		RoundRect(CatX, CatY, CW, 32.f * U, 6.f * U, bSel ? WithAlpha(Yellow, 0.9f) : FLinearColor(0.f, 0.f, 0.f, bHovCat ? 0.6f : 0.4f));
		TextF(CatNames[c], CatX + CW * 0.5f, CatY + 6.f * U, bSel ? FLinearColor(0.05f, 0.04f, 0.01f) : (bHovCat ? Yellow : Ink), 12.f, EUiWeight::Bold, EUiAlign::Center, false);
		AddButton(Btn_SettingCat + c, CatX, CatY, CW, 32.f * U);
		CatX += CW + 12.f * U;
	}
	TArray<int32> Rows;
	for (int32 i = 0; i < PC->GetSettingsCount(); ++i)
	{
		if (PC->GetSettingCategory(i) == SettingsCategory)
		{
			Rows.Add(i);
		}
	}
	const int32 Count = Rows.Num();
	const int32 PerColumn = Count <= 8 ? Count : (Count + 1) / 2;
	const float ColGap = 30.f * U;
	const float W = PerColumn == Count ? FMath::Min(PanelW, 1100.f * U) : (PanelW - ColGap) * 0.5f;
	const float RowH = FMath::Min(56.f * U, (IH - 200.f * U) / FMath::Max(1, PerColumn));
	HoverSetting = INDEX_NONE;
	const float Btn = 34.f * U;
	const float ValueW = 250.f * U;
	for (int32 r = 0; r < Count; ++r)
	{
		const int32 i = Rows[r];
		const float X = PanelX + (r < PerColumn ? 0.f : W + ColGap);
		const float Y = IY + 100.f * U + (r % PerColumn) * RowH;
		const bool bHov = Hover(X + 10.f * U, Y, W - 20.f * U, RowH - 6.f * U);
		if (bHov)
		{
			HoverSetting = i;
			DrawRect(FLinearColor(1.f, 0.85f, 0.3f, 0.06f), X + 10.f * U, Y, W - 20.f * U, RowH - 6.f * U);
		}
		AddButton(Btn_SettingRow + i, X + 10.f * U, Y, W - 20.f * U - (ValueW + Btn * 2.f + 40.f * U), RowH - 6.f * U);
		const float TY = Y + (RowH - 6.f * U) * 0.5f - 11.f * U;
		// v4.8 : libelle ajuste a la place libre avant les fleches (langues aux mots longs)
		const float LabelW = W - 56.f * U - (ValueW + Btn * 2.f + 40.f * U);
		TextFit(PC->GetSettingLabel(i), X + 28.f * U, TY, LabelW, bHov ? Yellow : Ink, LegacySize(Medium, 0.85f * U, U), EUiWeight::Regular, EUiAlign::Left, false);

		// [<]  valeur  [>]
		const float PX = X + W - 28.f * U - Btn;
		const float MX = PX - ValueW - Btn;
		const float BY = Y + (RowH - 6.f * U - Btn) * 0.5f;
		const bool bHovM = Hover(MX, BY, Btn, Btn);
		const bool bHovP = Hover(PX, BY, Btn, Btn);
		Frame(MX, BY, Btn, Btn, bHovM ? Yellow : YellowDim, 1.f * U);
		Frame(PX, BY, Btn, Btn, bHovP ? Yellow : YellowDim, 1.f * U);
		Txt(TEXT("<"), MX + Btn * 0.5f, BY + 4.f * U, bHovM ? Yellow : Ink, 0.85f * U, Medium, true, false);
		Txt(TEXT(">"), PX + Btn * 0.5f, BY + 4.f * U, bHovP ? Yellow : Ink, 0.85f * U, Medium, true, false);
		TextFit(PC->GetSettingValue(i), MX + Btn + ValueW * 0.5f, TY, ValueW - 12.f * U, Yellow, LegacySize(Medium, 0.85f * U, U), EUiWeight::Regular, EUiAlign::Center, false);
		AddButton(Btn_SettingBase + i * 2, MX, BY, Btn, Btn);
		AddButton(Btn_SettingBase + i * 2 + 1, PX, BY, Btn, Btn);
		DrawRect(FLinearColor(0.95f, 0.78f, 0.25f, 0.12f), X + 18.f * U, Y + RowH - 4.f * U, W - 36.f * U, 1.f * U);
	}
	const float X = PanelX;
	const float W2 = PanelW;

	// Aide de la ligne survolee
	const FString Hint = HoverSetting != INDEX_NONE ? PC->GetSettingHint(HoverSetting) : FString();
	float HY = IY + IH - 84.f * U;
	for (const FString& L : Wrap(Hint.IsEmpty() ? FString(BR_STR(NSLOCTEXT("BR", "HUD.ReglagesSontSauvegardesAutomatiquementBa", "Les r\u00e9glages sont sauvegard\u00e9s automatiquement (BackroomsPlayer.ini)."))) : Hint,
		W2 - 56.f * U, Small, 0.75f * U))
	{
		TxtLine(L, X + 28.f * U, HY, W2 - 56.f * U, InkDim, 0.75f * U, Small);
		HY += 20.f * U;
	}
	// v4.5 : le mode de rendu reellement actif (et non celui demande) ; RHI et support du ray tracing : au demarrage
	Txt(PC->GetRenderModeText(), X + 28.f * U, IY + IH - 40.f * U, WithAlpha(Yellow, 0.85f), 0.66f * U, Small, false, false);
	Txt(BR_STR(NSLOCTEXT("BR", "HUD.ImmediatProfilQualiteRayTracing", "Imm\u00e9diat : profil, qualit\u00e9, ray tracing, reflets, ombres de la lampe, r\u00e9solution.  Au red\u00e9marrage : DirectX 12 / 11, support du ray tracing, cache de skinning (Config/DefaultEngine.ini).")), X + 28.f * U, IY + IH - 22.f * U, WithAlpha(InkDim, 0.7f), 0.62f * U, Small, false, false);
}

void ABRHUD::HandleInventoryMouse(ABRPlayerController* PC, ABRCharacter* C)
{
	if (!PlayerOwner || !PC || BRDisplay::IsPending())
	{
		return; // v4.9 : pendant la confirmation de l'affichage, seuls ses deux boutons repondent
	}
	const bool bPressed = PlayerOwner->WasInputKeyJustPressed(EKeys::LeftMouseButton);
	const bool bReleased = PlayerOwner->WasInputKeyJustReleased(EKeys::LeftMouseButton);
	const bool bHeld = PlayerOwner->IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bRight = PlayerOwner->WasInputKeyJustPressed(EKeys::RightMouseButton);

	FSlotBox* Under = (C && Tab == ETab::Character && !Inspecting.IsValid()) ? FindSlot(MouseX, MouseY) : nullptr;
	HoverSlot = Under ? Under->Ref : FSlotRef();

	// Clic droit sur une touche : effacer
	if (bRight && Tab == ETab::Keys)
	{
		const int32 RId = ButtonAt(MouseX, MouseY);
		if (RId >= Btn_KeySlot)
		{
			PC->ClearKey((RId - Btn_KeySlot) / BRKeys::SlotsPerAction, (RId - Btn_KeySlot) % BRKeys::SlotsPerAction);
		}
		return;
	}

	if (bPressed)
	{
		const int32 Id = ButtonAt(MouseX, MouseY);
		if (Id != INDEX_NONE)
		{
			if (Id >= Btn_SettingCat && Id < Btn_SettingCat + 4)
			{
				SettingsCategory = Id - Btn_SettingCat;
				HoverSetting = INDEX_NONE;
				PC->CancelKeyCapture();
			}
			else if (Id >= Btn_KeySlot && Id < Btn_Language)
			{
				PC->BeginKeyCapture((Id - Btn_KeySlot) / BRKeys::SlotsPerAction, (Id - Btn_KeySlot) % BRKeys::SlotsPerAction);
			}
			else if (Id == Btn_KeysReset)
			{
				PC->ResetKeys();
			}
			else if (Id >= Btn_TabCharacter && Id <= Btn_TabKeys)
			{
				const ETab NewTab = static_cast<ETab>(Id - Btn_TabCharacter);
				if (C || NewTab == ETab::Settings || NewTab == ETab::Keys)
				{
					Tab = NewTab;
				}
				Inspecting = FSlotRef();
				Dragging = FSlotRef();
				PC->CancelKeyCapture();
			}
			else if (Id == Btn_InspectUse && Inspecting.IsValid() && C)
			{
				C->UseSlot(Inspecting.Group, Inspecting.Index);
				const FBRItemSlot* After = C->GetSlot(Inspecting.Group, Inspecting.Index);
				if (!After || After->IsEmpty() || Inspecting.Group == EBRSlotGroup::Equipment || BRItems::Get(After->Item).Slot != EBREquipSlot::None)
				{
					Inspecting = FSlotRef();
				}
			}
			else if (Id == Btn_InspectClose)
			{
				Inspecting = FSlotRef();
			}
			else if (Id >= Btn_SettingRow && Id < Btn_KeysReset)
			{
				PC->AdjustSetting(Id - Btn_SettingRow, 1);
			}
			else if (Id >= Btn_SettingBase && Id < Btn_SettingRow)
			{
				const int32 Rel = Id - Btn_SettingBase;
				PC->AdjustSetting(Rel / 2, (Rel % 2) ? 1 : -1);
			}
			return;
		}
		if (Inspecting.IsValid())
		{
			Inspecting = FSlotRef(); // clic en dehors : ferme l'inspection
			return;
		}
		if (PC->IsCapturingKey())
		{
			return;
		}
		if (Under && C)
		{
			const double Now = FPlatformTime::Seconds();
			const FBRItemSlot* S = C->GetSlot(Under->Ref.Group, Under->Ref.Index);
			if (S && !S->IsEmpty())
			{
				if (LastClickSlot == Under->Ref && Now - LastClickTime < 0.35)
				{
					// Double-clic : utiliser / equiper / retirer
					C->UseSlot(Under->Ref.Group, Under->Ref.Index);
					LastClickTime = 0.0;
					Dragging = FSlotRef();
					return;
				}
				Dragging = Under->Ref;
			}
			LastClickSlot = Under->Ref;
			LastClickTime = Now;
		}
	}

	if (Dragging.IsValid() && (bReleased || !bHeld))
	{
		if (Under && C && !(Under->Ref == Dragging))
		{
			C->MoveItem(Dragging.Group, Dragging.Index, Under->Ref.Group, Under->Ref.Index);
		}
		Dragging = FSlotRef();
	}

	if (bRight && Under && C)
	{
		const FBRItemSlot* S = C->GetSlot(Under->Ref.Group, Under->Ref.Index);
		if (S && !S->IsEmpty())
		{
			Inspecting = Under->Ref;
			Dragging = FSlotRef();
			C->PlayUISound(TEXT("S_UIClick"));
		}
	}
}

// =====================================================================================================================
// Multijoueur
// =====================================================================================================================

void ABRHUD::DrawTeammates(ABRCharacter* C)
{
	UWorld* World = GetWorld();
	if (!World || !C || World->GetNetMode() == NM_Standalone)
	{
		return;
	}
	const float U = Ui();
	UFont* Medium = GEngine->GetMediumFont();
	// v4.9 : les noms ne traversent plus les murs ni les etages (ligne de vue depuis la camera). Exception : un coequipier
	// a terre et relevable garde un repere discret (sans nom) pour qu'on puisse aller le relever.
	FVector ViewLoc = C->GetActorLocation();
	FRotator ViewRot = FRotator::ZeroRotator;
	if (PlayerOwner)
	{
		PlayerOwner->GetPlayerViewPoint(ViewLoc, ViewRot);
	}
	for (TActorIterator<ABRCharacter> It(World); It; ++It)
	{
		ABRCharacter* Other = *It;
		if (!Other || Other == C)
		{
			continue;
		}
		const float Dist = static_cast<float>(FVector::Dist(Other->GetActorLocation(), C->GetActorLocation()));
		if (Dist > 5000.f)
		{
			continue;
		}
		const FVector Screen = Project(Other->GetActorLocation() + FVector(0.f, 0.f, 112.f));
		if (Screen.Z <= 0.f || Screen.X < 0.f || Screen.Y < 0.f || Screen.X > Canvas->ClipX || Screen.Y > Canvas->ClipY)
		{
			continue;
		}
		FCollisionQueryParams Query(FName(TEXT("BRTeammateName")), false);
		Query.AddIgnoredActor(C);
		Query.AddIgnoredActor(Other);
		const FVector Head = Other->GetActorLocation() + FVector(0.f, 0.f, Other->IsDead() ? 20.f : 70.f);
		const bool bLineOfSight = !World->LineTraceTestByChannel(ViewLoc, Head, ECC_Visibility, Query);
		if (!bLineOfSight)
		{
			if (Other->IsDead() && Other->CanBeRevived())
			{
				const float MA = FMath::Clamp(1.1f - Dist / 5000.f, 0.3f, 0.75f);
				const float SX = static_cast<float>(Screen.X);
				const float SY = static_cast<float>(Screen.Y) + 60.f * U;
				DrawRect(FLinearColor(1.f, 0.45f, 0.4f, MA), SX - 7.f * U, SY - 1.5f * U, 14.f * U, 3.f * U);
				DrawRect(FLinearColor(1.f, 0.45f, 0.4f, MA), SX - 1.5f * U, SY - 7.f * U, 3.f * U, 14.f * U);
				TextF(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.DistM", "{Dist} m"), { { TEXT("Dist"), BRLoc::Int(FMath::RoundToInt(Dist / 100.f)) } }), SX, SY + 10.f * U,
					WithAlpha(InkDim, MA), 9.f, EUiWeight::Regular, EUiAlign::Center);
			}
			continue;
		}
		const APlayerState* PS = Other->GetPlayerState();
		FString Name = PS ? PS->GetPlayerName() : FString(BR_STR(NSLOCTEXT("BR", "HUD.Explorateur", "Explorateur")));
		if (Name.Len() > 20)
		{
			Name = Name.Left(20);
		}
		const float A = FMath::Clamp(1.2f - Dist / 5000.f, 0.35f, 1.f);
		if (Other->IsDead())
		{
			// v4.7 : etat tenu par le serveur ; au fond d'une fosse, inutile d'aller le chercher
			Name += Other->CanBeRevived()
				? (Other->GetDeathCause() == EBRDeathCause::Drowning ? BR_STR(NSLOCTEXT("BR", "HUD.NoyeSortez", "  (noy\u00e9 : sortez-le)")) : BR_STR(NSLOCTEXT("BR", "HUD.Terre2", "  (\u00e0 terre)")))
				: BR_STR(NSLOCTEXT("BR", "HUD.HorsAtteinte", "  (hors d'atteinte)"));
		}
		const FLinearColor Col = Other->IsDead() ? FLinearColor(1.f, 0.45f, 0.4f, A) : FLinearColor(0.8f, 1.f, 0.8f, A);
		Txt(Name, static_cast<float>(Screen.X), static_cast<float>(Screen.Y) - 22.f * U, Col, 0.8f * U, Medium, true);
		// Il parle : petites barres qui bougent avec sa voix, a cote de son nom
		const ABRPlayerController* MyPC = Cast<ABRPlayerController>(PlayerOwner);
		const float Talk = MyPC ? FMath::Clamp(MyPC->GetTalkLevel(PS) * 6.f, 0.f, 1.f) : 0.f;
		if (Talk > 0.05f)
		{
			const float BX = static_cast<float>(Screen.X) + TextW(Name, Medium, 0.8f * U) * 0.5f + 10.f * U;
			for (int32 k = 0; k < 3; ++k)
			{
				const float BH = (4.f + 12.f * Talk * (0.6f + 0.4f * FMath::Sin(Clock * 18.f + k * 1.7f))) * U;
				DrawRect(FLinearColor(0.6f, 1.f, 0.6f, A), BX + k * 6.f * U, static_cast<float>(Screen.Y) - 6.f * U - BH, 3.f * U, BH);
			}
		}
		Txt(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.DistM", "{Dist} m"), { { TEXT("Dist"), BRLoc::Int(FMath::RoundToInt(Dist / 100.f)) } }), static_cast<float>(Screen.X), static_cast<float>(Screen.Y) - 2.f * U,
			WithAlpha(InkDim, A), 0.65f * U, Medium, true);
	}
}

void ABRHUD::DrawVideoConfirm(ABRPlayerController* PC)
{
	if (!BRDisplay::IsPending() || !PC)
	{
		return;
	}
	const float U = Ui();
	float MX = 0.f;
	float MY = 0.f;
	PC->GetMousePosition(MX, MY);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	const float W = FMath::Min(680.f * U, Canvas->ClipX - 40.f * U);
	const float H = 250.f * U;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = (Canvas->ClipY - H) * 0.5f;
	RoundRect(X, Y, W, H, 12.f * U, FLinearColor(0.03f, 0.027f, 0.012f, 0.97f));
	RoundRect(X, Y, W, H, 12.f * U, WithAlpha(Yellow, 0.7f), true);
	TextF(BR_STR(NSLOCTEXT("BR", "Display.KeepTitle", "CONSERVER CET AFFICHAGE ?")), X + W * 0.5f, Y + 26.f * U, Yellow, 17.f, EUiWeight::Bold, EUiAlign::Center, false);
	TextFit(PC->GetDisplaySummary(), X + W * 0.5f, Y + 72.f * U, W - 40.f * U, Ink, 13.f, EUiWeight::Regular, EUiAlign::Center, false);
	TextFit(BRLoc::Fmt(NSLOCTEXT("BR", "Display.Countdown", "Retour \u00e0 l'affichage pr\u00e9c\u00e9dent dans {Seconds} s."),
		{ { TEXT("Seconds"), BRLoc::Int(FMath::CeilToInt(BRDisplay::SecondsLeft())) } }), X + W * 0.5f, Y + 104.f * U, W - 40.f * U, InkDim, 12.f, EUiWeight::Regular, EUiAlign::Center, false);
	const FString Keep = BR_STR(NSLOCTEXT("BR", "Display.Keep", "CONSERVER  [ENTR\u00c9E]"));
	const FString Back = BR_STR(NSLOCTEXT("BR", "Display.Revert", "R\u00c9TABLIR  [\u00c9CHAP]"));
	const float BH = 44.f * U;
	const float BW = (W - 72.f * U) * 0.5f;
	const float BY = Y + H - BH - 26.f * U;
	const float KX = X + 24.f * U;
	const float RX = KX + BW + 24.f * U;
	const bool bHovK = MX >= KX && MX <= KX + BW && MY >= BY && MY <= BY + BH;
	const bool bHovR = MX >= RX && MX <= RX + BW && MY >= BY && MY <= BY + BH;
	RoundRect(KX, BY, BW, BH, 8.f * U, bHovK ? Yellow : WithAlpha(Yellow, 0.75f));
	TextFit(Keep, KX + BW * 0.5f, BY + 10.f * U, BW - 16.f * U, FLinearColor(0.05f, 0.04f, 0.01f), 13.f, EUiWeight::Bold, EUiAlign::Center, false);
	RoundRect(RX, BY, BW, BH, 8.f * U, FLinearColor(0.f, 0.f, 0.f, bHovR ? 0.75f : 0.5f));
	RoundRect(RX, BY, BW, BH, 8.f * U, bHovR ? Ink : InkDim, true);
	TextFit(Back, RX + BW * 0.5f, BY + 10.f * U, BW - 16.f * U, Ink, 13.f, EUiWeight::Bold, EUiAlign::Center, false);
	if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		if (bHovK)
		{
			BRDisplay::Confirm();
		}
		else if (bHovR)
		{
			BRDisplay::Revert();
		}
	}
}

void ABRHUD::DrawVoiceIndicator(ABRPlayerController* PC)
{
	if (!PC || !PC->IsNetGame())
	{
		return;
	}
	const float U = Ui();
	const float X = 50.f * U;
	const float Y = Canvas->ClipY - 236.f * U;
	UFont* Small = GEngine->GetSmallFont();
	const int32 Mode = FBRSettings::Get().VoiceMode;
	if (PC->IsTransmittingVoice())
	{
		const float Pulse = 0.7f + 0.3f * FMath::Sin(Clock * 6.f);
		DrawRect(FLinearColor(0.95f, 0.2f, 0.15f, Pulse), X, Y + 4.f * U, 10.f * U, 10.f * U);
		Txt(Mode == 0 ? BR_STR(NSLOCTEXT("BR", "HUD.MicroOuvert", "MICRO OUVERT")) : BR_STR(NSLOCTEXT("BR", "HUD.Parlez", "VOUS PARLEZ")), X + 18.f * U, Y, FLinearColor(1.f, 0.85f, 0.8f, 0.9f), 0.75f * U, Small, false);
	}
	else if (Mode == 1)
	{
		Txt(BRLoc::Fmt(NSLOCTEXT("BR", "HUD.PushToTalkHint", "{Key} parler"), { { TEXT("Key"), BRLoc::Arg(BRKeys::Tag(EBRAction::PushToTalk)) } }), X, Y, WithAlpha(InkDim, 0.7f), 0.7f * U, Small, false);
	}
	else if (Mode == 2)
	{
		Txt(BR_STR(NSLOCTEXT("BR", "HUD.MicroCoupe", "MICRO COUP\u00c9")), X, Y, WithAlpha(InkDim, 0.6f), 0.7f * U, Small, false);
	}
	// v4.9 : qui parle, sans position (les noms au-dessus des tetes ne traversent plus les murs)
	float TY = Y - 24.f * U;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ABRCharacter> It(World); It; ++It)
		{
			ABRCharacter* Other = *It;
			if (!Other || Other == PC->GetPawn())
			{
				continue;
			}
			const APlayerState* PS = Other->GetPlayerState();
			const float Talk = FMath::Clamp(PC->GetTalkLevel(PS) * 6.f, 0.f, 1.f);
			if (!PS || Talk < 0.05f)
			{
				continue;
			}
			for (int32 k = 0; k < 3; ++k)
			{
				const float BH = (3.f + 9.f * Talk * (0.6f + 0.4f * FMath::Sin(Clock * 18.f + k * 1.7f))) * U;
				DrawRect(FLinearColor(0.6f, 1.f, 0.6f, 0.85f), X + k * 5.f * U, TY + 14.f * U - BH, 3.f * U, BH);
			}
			TextF(PS->GetPlayerName().Left(20), X + 22.f * U, TY, FLinearColor(0.8f, 1.f, 0.8f, 0.85f), 10.f, EUiWeight::Regular, EUiAlign::Left);
			TY -= 20.f * U;
		}
	}
}

void ABRHUD::DrawPlayerList()
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	const float U = Ui();
	const float PW = 400.f * U;
	const float X = Canvas->ClipX - PW - 60.f * U;
	float Y = 262.f * U;
	const int32 N = GS->PlayerArray.Num();
	const float RowH = 40.f * U;
	const float BoxH = 58.f * U + N * RowH + 8.f * U;
	RoundRect(X, Y, PW, BoxH, 14.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.85f));
	RoundRect(X, Y, PW, BoxH, 14.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.14f), true);
	TextSpaced(BR_STR(NSLOCTEXT("BR", "HUD.Joueurs", "JOUEURS")), X + 22.f * U, Y + 20.f * U, Yellow, 10.5f, EUiWeight::Bold, 3.f * U);
	TextF(BR_STR(NSLOCTEXT("BR", "HUD.Latence", "LATENCE")), X + PW - 22.f * U, Y + 20.f * U, InkDim, 9.5f, EUiWeight::Regular, EUiAlign::Right, false);
	Y += 54.f * U;
	for (int32 i = 0; i < N; ++i)
	{
		const APlayerState* PS = GS->PlayerArray[i];
		if (!PS)
		{
			continue;
		}
		// Le premier joueur de la liste est l'hote (c'est lui qui a cree la partie)
		FString Name = PS->GetPlayerName();
		if (Name.Len() > 18)
		{
			Name = Name.Left(18);
		}
		const bool bMe = PlayerOwner && PS == PlayerOwner->PlayerState;
		if (bMe)
		{
			RoundRect(X + 10.f * U, Y - 4.f * U, PW - 20.f * U, RowH - 4.f * U, 10.f * U, FLinearColor(1.f, 0.82f, 0.22f, 0.08f));
		}
		const float D = 26.f * U;
		RoundRect(X + 20.f * U, Y + (RowH - 8.f * U - D) * 0.5f, D, D, D * 0.5f, bMe ? WithAlpha(Yellow, 0.9f) : FLinearColor(1.f, 1.f, 1.f, 0.14f));
		TextF(Name.Left(1), X + 20.f * U + D * 0.5f, Y + (RowH - 8.f * U - D) * 0.5f + 3.f * U, bMe ? FLinearColor(0.07f, 0.055f, 0.02f, 1.f) : Ink, 10.5f,
			EUiWeight::Bold, EUiAlign::Center, false);
		const FVector2f NS = TextSize(Name, 13.f, EUiWeight::Regular);
		const float NY = Y + (RowH - 8.f * U - NS.Y) * 0.5f;
		TextF(Name, X + 58.f * U, NY, bMe ? Yellow : Ink, 13.f, EUiWeight::Regular, EUiAlign::Left, false);
		if (i == 0)
		{
			const FVector2f HS = TextSize(BR_STR(NSLOCTEXT("BR", "HUD.Hote", "H\u00d4TE")), 8.5f, EUiWeight::Bold);
			const float HX = X + 66.f * U + NS.X;
			RoundRect(HX, NY + (NS.Y - 18.f * U) * 0.5f, HS.X + 16.f * U, 18.f * U, 9.f * U, FLinearColor(1.f, 0.82f, 0.22f, 0.2f));
			TextF(BR_STR(NSLOCTEXT("BR", "HUD.Hote", "H\u00d4TE")), HX + 8.f * U, NY + (NS.Y - HS.Y) * 0.5f, Yellow, 8.5f, EUiWeight::Bold, EUiAlign::Left, false);
		}
		const int32 Ping = FMath::RoundToInt(PS->GetPingInMilliseconds());
		const FLinearColor PingCol = Ping < 80 ? Done : (Ping < 160 ? Yellow : Danger);
		TextF(i == 0 ? FString(TEXT("\u2014")) : BRLoc::Fmt(NSLOCTEXT("BR", "HUD.PingMs", "{Ping} ms"), { { TEXT("Ping"), BRLoc::Int(Ping) } }), X + PW - 22.f * U, NY, PingCol, 12.f, EUiWeight::Bold,
			EUiAlign::Right, false);
		Y += RowH;
	}
}
