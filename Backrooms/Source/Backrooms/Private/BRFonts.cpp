#include "BRFonts.h"
#include "Backrooms.h"

#include "Engine/Font.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
	/** Une sous-police : fichiers Regular / Bold, langues ou elle prime ("" : toutes), plages de caracteres */
	struct FScriptFont
	{
		const TCHAR* Regular;
		const TCHAR* Bold;
		const TCHAR* Cultures;
		TArray<FInt32Range> Ranges;
	};

	FInt32Range Span(int32 First, int32 Last)
	{
		return FInt32Range(FInt32Range::BoundsType::Inclusive(First), FInt32Range::BoundsType::Inclusive(Last));
	}

	/** Plages sans un caractere : il passe a la sous-police suivante (ideogramme absent d'une police propre a une langue) */
	TArray<FInt32Range> Without(const TArray<FInt32Range>& Ranges, int32 Codepoint)
	{
		TArray<FInt32Range> Out;
		for (const FInt32Range& R : Ranges)
		{
			const int32 Lo = R.GetLowerBoundValue();
			const int32 Hi = R.GetUpperBoundValue();
			if (Codepoint < Lo || Codepoint > Hi)
			{
				Out.Add(R);
				continue;
			}
			if (Codepoint > Lo)
			{
				Out.Add(Span(Lo, Codepoint - 1));
			}
			if (Codepoint < Hi)
			{
				Out.Add(Span(Codepoint + 1, Hi));
			}
		}
		return Out;
	}

	const TArray<FScriptFont>& ScriptFonts()
	{
		// Ideogrammes, ponctuation et formes pleine chasse communs aux langues CJK
		const TArray<FInt32Range> Han = { Span(0x2E80, 0x2FDF), Span(0x3000, 0x303F), Span(0x31C0, 0x31EF), Span(0x3200, 0x33FF), Span(0x3400, 0x4DBF),
			Span(0x4E00, 0x9FFF), Span(0xF900, 0xFAFF), Span(0xFE30, 0xFE4F), Span(0xFF00, 0xFFEF), Span(0x20000, 0x2FA1F) };
		const TArray<FInt32Range> Kana = { Span(0x3040, 0x30FF), Span(0x31F0, 0x31FF), Span(0xFF65, 0xFF9F) };
		const TArray<FInt32Range> Hangul = { Span(0x1100, 0x11FF), Span(0x3130, 0x318F), Span(0xA960, 0xA97F), Span(0xAC00, 0xD7AF), Span(0xD7B0, 0xD7FF) };
		const TArray<FInt32Range> Bopomofo = { Span(0x3100, 0x312F), Span(0x31A0, 0x31BF) };
		const TArray<FInt32Range> Arabic = { Span(0x0600, 0x06FF), Span(0x0750, 0x077F), Span(0x0870, 0x08FF), Span(0xFB50, 0xFDFF), Span(0xFE70, 0xFEFF) };
		auto Join = [](std::initializer_list<const TArray<FInt32Range>*> Parts)
		{
			TArray<FInt32Range> Out;
			for (const TArray<FInt32Range>* P : Parts)
			{
				Out.Append(*P);
			}
			return Out;
		};
		// \u7B80 (premier caractere de \u7B80\u4F53\u4E2D\u6587, nom natif du chinois simplifie) manque aux polices TC et JP : il
		// passe a la police SC. Tools/Localization/build_fonts.py signale tout autre caractere des noms natifs absent.
		const TArray<FInt32Range> HanNoSimplified = Without(Han, 0x7B80);
		static const TArray<FScriptFont> List = {
			// Propres a une langue : les ideogrammes y prennent la forme locale (le coreen garde les ideogrammes de la police SC,
			// rares dans l'interface ; sa police couvre le hangeul)
			{ TEXT("NotoSansJP-Regular.ttf"), TEXT("NotoSansJP-Bold.ttf"), TEXT("ja"), Join({ &HanNoSimplified, &Kana }) },
			{ TEXT("NotoSansTC-Regular.ttf"), TEXT("NotoSansTC-Bold.ttf"), TEXT("zh-Hant;zh-TW;zh-HK;zh-MO"), Join({ &HanNoSimplified, &Bopomofo }) },
			// Pour toutes les langues
			{ TEXT("NotoSansSC-Regular.ttf"), TEXT("NotoSansSC-Bold.ttf"), TEXT(""), Join({ &Han, &Bopomofo }) },
			{ TEXT("NotoSansJP-Regular.ttf"), TEXT("NotoSansJP-Bold.ttf"), TEXT(""), Kana },
			{ TEXT("NotoSansKR-Regular.ttf"), TEXT("NotoSansKR-Bold.ttf"), TEXT(""), Hangul },
			{ TEXT("NotoSansArabic-Regular.ttf"), TEXT("NotoSansArabic-Bold.ttf"), TEXT(""), Arabic },
		};
		return List;
	}

	FString FontPath(const TCHAR* File)
	{
		return FPaths::ProjectContentDir() / TEXT("Fonts") / File;
	}

	bool FontExists(const TCHAR* File)
	{
		// IFileManager : voit aussi les fichiers du paquet (.pak / IoStore)
		return IFileManager::Get().FileSize(*FontPath(File)) > 0;
	}
}

namespace BRFonts
{
	const TArray<FString>& ExpectedFiles()
	{
		static const TArray<FString> Files = []()
		{
			TArray<FString> Out;
			for (const FScriptFont& F : ScriptFonts())
			{
				Out.AddUnique(F.Regular);
				Out.AddUnique(F.Bold);
			}
			return Out;
		}();
		return Files;
	}

	TArray<FString> MissingFiles()
	{
		TArray<FString> Missing;
		for (const FString& F : ExpectedFiles())
		{
			if (!FontExists(*F))
			{
				Missing.Add(F);
			}
		}
		return Missing;
	}

	UFont* WithScripts(UFont* Base)
	{
		if (!Base || Base->FontCacheType != EFontCacheType::Runtime)
		{
			return Base; // police "hors ligne" (atlas precalcule) : pas de sous-polices possibles
		}
		static TMap<TWeakObjectPtr<UFont>, TWeakObjectPtr<UFont>> Made;
		if (const TWeakObjectPtr<UFont>* Found = Made.Find(Base))
		{
			if (UFont* Existing = Found->Get())
			{
				return Existing;
			}
		}
		// v4.10 : on copie la police *effective*. En 5.8, la police moyenne du moteur (DefaultRegularFont) est une coquille
		// dont le contenu vient du style Slate (RuntimeFontSource = CoreStyleDefault) : son champ CompositeFont est vide.
		// Copier ce champ donnait une police sans aucun caractere latin : tout le texte en carres (police de dernier recours).
		const FCompositeFont* Source = Base->GetCompositeFont();
		if (!Source || Source->DefaultTypeface.Fonts.Num() == 0)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Police de l'interface : %s n'a pas de police par defaut ; sous-polices d'ecriture non ajoutees"), *Base->GetName());
			return Base;
		}
		UFont* Font = NewObject<UFont>(GetTransientPackage(), NAME_None, RF_Transient);
		Font->FontCacheType = EFontCacheType::Runtime;
		Font->RuntimeFontSource = ERuntimeFontSource::Asset;
		FCompositeFont& Composite = Font->GetMutableInternalCompositeFont();
		Composite = *Source;
		Font->LegacyFontSize = Base->LegacyFontSize;
		Font->LegacyFontName = Base->LegacyFontName;
		int32 Added = 0;
		for (const FScriptFont& S : ScriptFonts())
		{
			if (!FontExists(S.Regular))
			{
				continue;
			}
			FCompositeSubFont Sub;
			Sub.Typeface.AppendFont(TEXT("Regular"), FontPath(S.Regular), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			if (FontExists(S.Bold))
			{
				Sub.Typeface.AppendFont(TEXT("Bold"), FontPath(S.Bold), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			}
			Sub.CharacterRanges = S.Ranges;
			Sub.Cultures = S.Cultures;
			Composite.SubTypefaces.Add(Sub);
			++Added;
		}
		const TArray<FString> Missing = MissingFiles();
		if (Missing.Num() > 0)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Polices : %d fichier(s) absent(s) de Content/Fonts (%s) : chinois, japonais, coreen, arabe ou persan en carres. Paquet : DirectoriesToAlwaysStageAsUFS=Fonts."),
				Missing.Num(), *FString::Join(Missing, TEXT(", ")));
		}
		UE_LOG(LogBackrooms, Log, TEXT("Police de l'interface : %s + %d sous-police(s) d'ecriture"), *Base->GetName(), Added);
		Font->AddToRoot(); // gardee jusqu'a la fin du jeu, comme les polices du moteur
		Made.Add(Base, Font);
		return Font;
	}
}
