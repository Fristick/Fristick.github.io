#include "BRLoc.h"
#include "Backrooms.h"
#include "BRConfig.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformMisc.h"

namespace
{
	const TCHAR* LanguageSection = TEXT("/Script/Backrooms.BRSettings");

	/** Cles de l'espace de noms BR (genere par Tools/Localization/loc_build.py a partir des sources) */
	const TCHAR* const BRLocKeyList[] = {
#include "BRLocKeys.inl"
	};
}

namespace BRLoc
{
	FFormatArgumentValue Num(double V, int32 Digits)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(Digits);
		Options.SetMaximumFractionalDigits(Digits);
		return FFormatArgumentValue(FText::AsNumber(V, &Options));
	}

	FFormatArgumentValue Signed(double V, int32 Digits)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(Digits);
		Options.SetMaximumFractionalDigits(Digits);
		const FText Value = FText::AsNumber(V, &Options);
		return FFormatArgumentValue(V > 0.0 ? FText::AsCultureInvariant(TEXT("+") + Value.ToString()) : Value);
	}

	FFormatArgumentValue Pad(int64 V, int32 Width)
	{
		FString S = FString::Printf(TEXT("%lld"), V);
		while (S.Len() < Width)
		{
			S = TEXT("0") + S;
		}
		return FFormatArgumentValue(FText::AsCultureInvariant(S));
	}

	FText FmtText(const FText& Pattern, std::initializer_list<TPair<const TCHAR*, FFormatArgumentValue>> Args)
	{
		FFormatNamedArguments Named;
		for (const TPair<const TCHAR*, FFormatArgumentValue>& A : Args)
		{
			Named.Add(A.Key, A.Value);
		}
		return FText::Format(FTextFormat(Pattern), Named);
	}

	FString Fmt(const FText& Pattern, std::initializer_list<TPair<const TCHAR*, FFormatArgumentValue>> Args)
	{
		return FmtText(Pattern, Args).ToString();
	}

	const TArray<FLanguage>& Languages()
	{
		// Noms natifs : chaque langue s'affiche dans sa propre ecriture, quelle que soit la langue courante
		static const TArray<FLanguage> List = {
			{ TEXT("fr"), TEXT("Fran\u00e7ais"), false, false },
			{ TEXT("en"), TEXT("English"), false, false },
			{ TEXT("de"), TEXT("Deutsch"), false, false },
			{ TEXT("es-ES"), TEXT("Espa\u00f1ol (Espa\u00f1a)"), false, false },
			{ TEXT("pt-BR"), TEXT("Portugu\u00eas (Brasil)"), false, false },
			{ TEXT("ru"), TEXT("\u0420\u0443\u0441\u0441\u043a\u0438\u0439"), false, false },
			{ TEXT("it"), TEXT("Italiano"), false, false },
			{ TEXT("tr"), TEXT("T\u00fcrk\u00e7e"), false, false },
			{ TEXT("es-419"), TEXT("Espa\u00f1ol (Latinoam\u00e9rica)"), false, false },
			{ TEXT("pl"), TEXT("Polski"), false, false },
			{ TEXT("zh-Hans"), TEXT("\u7b80\u4f53\u4e2d\u6587"), false, true },
			{ TEXT("uk"), TEXT("\u0423\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430"), false, false },
			{ TEXT("ar"), TEXT("\u0627\u0644\u0639\u0631\u0628\u064a\u0629"), true, false },
			{ TEXT("ko"), TEXT("\ud55c\uad6d\uc5b4"), false, false },
			{ TEXT("fa"), TEXT("\u0641\u0627\u0631\u0633\u06cc"), true, false },
			{ TEXT("ja"), TEXT("\u65e5\u672c\u8a9e"), false, true },
			{ TEXT("hu"), TEXT("Magyar"), false, false },
			{ TEXT("cs"), TEXT("\u010ce\u0161tina"), false, false },
			{ TEXT("pt-PT"), TEXT("Portugu\u00eas (Portugal)"), false, false },
			{ TEXT("sv"), TEXT("Svenska"), false, false },
			{ TEXT("zh-Hant"), TEXT("\u7e41\u9ad4\u4e2d\u6587"), false, true },
			{ TEXT("id"), TEXT("Bahasa Indonesia"), false, false },
		};
		return List;
	}

	namespace
	{
		/** Langue proposee la plus proche d'un code de culture (fr-CA -> fr, es-MX -> es-419, zh-TW -> zh-Hant...) */
		FString BestMatch(const FString& InCulture)
		{
			const FString C = InCulture.Replace(TEXT("_"), TEXT("-"));
			for (const FLanguage& L : Languages())
			{
				if (C.Equals(L.Code, ESearchCase::IgnoreCase))
				{
					return L.Code;
				}
			}
			FString Lang = C;
			FString Rest;
			C.Split(TEXT("-"), &Lang, &Rest);
			Lang = Lang.ToLower();
			if (Lang == TEXT("zh"))
			{
				const FString R = Rest.ToUpper();
				return (R.Contains(TEXT("HANT")) || R.Contains(TEXT("TW")) || R.Contains(TEXT("HK")) || R.Contains(TEXT("MO"))) ? TEXT("zh-Hant") : TEXT("zh-Hans");
			}
			if (Lang == TEXT("es"))
			{
				return (Rest.IsEmpty() || Rest.Equals(TEXT("ES"), ESearchCase::IgnoreCase)) ? TEXT("es-ES") : TEXT("es-419");
			}
			if (Lang == TEXT("pt"))
			{
				return (Rest.IsEmpty() || Rest.Equals(TEXT("BR"), ESearchCase::IgnoreCase)) ? TEXT("pt-BR") : TEXT("pt-PT");
			}
			if (Lang == TEXT("in"))
			{
				Lang = TEXT("id"); // ancien code de l'indonesien
			}
			for (const FLanguage& L : Languages())
			{
				if (Lang.Equals(L.Code, ESearchCase::IgnoreCase))
				{
					return L.Code;
				}
			}
			return FString();
		}
	}

	int32 CurrentIndex()
	{
		const FString Name = FInternationalization::Get().GetCurrentLanguage()->GetName();
		const FString Code = BestMatch(Name);
		for (int32 i = 0; i < Languages().Num(); ++i)
		{
			if (Code == Languages()[i].Code)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	const FLanguage& Current()
	{
		const int32 I = CurrentIndex();
		return Languages()[I == INDEX_NONE ? 0 : I];
	}

	FString DetectSystemLanguage()
	{
		// Langue de l'interface du systeme, sinon ses reglages regionaux ; a defaut, l'anglais
		const FString Candidates[] = { FPlatformMisc::GetDefaultLanguage(), FPlatformMisc::GetDefaultLocale() };
		for (const FString& C : Candidates)
		{
			const FString Match = BestMatch(C);
			if (!Match.IsEmpty())
			{
				return Match;
			}
		}
		return TEXT("en");
	}

	FString& Preference()
	{
		static FString Pref;
		return Pref;
	}

	bool SetLanguage(const FString& Code)
	{
		const FString Match = BestMatch(Code);
		if (Match.IsEmpty())
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Langue %s non proposee par le jeu"), *Code);
			return false;
		}
		// Langue des textes, formats des nombres et des dates : tout change tout de suite (les FText se reconstruisent)
		if (!FInternationalization::Get().SetCurrentCulture(Match))
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Culture %s indisponible (donnees ICU absentes du paquet ?)"), *Match);
			return false;
		}
		TArray<FString> MissingKeys;
		const int32 Missing = CountMissing(&MissingKeys);
		UE_LOG(LogBackrooms, Log, TEXT("Langue : %s (%d cles sans traduction, texte francais affiche a leur place)"), *Match, Missing);
		for (int32 i = 0; i < MissingKeys.Num() && i < 20; ++i)
		{
			UE_LOG(LogBackrooms, Log, TEXT("  sans traduction (%s) : %s"), *Match, *MissingKeys[i]);
		}
		return true;
	}

	bool IsRightToLeft()
	{
		return Current().bRightToLeft;
	}

	bool UsesNoSpaces()
	{
		return Current().bNoSpaces;
	}

	bool HasTranslation(const TCHAR* Key)
	{
		// Une cle est traduite quand une ressource .locres de la langue courante l'a fournie
#if WITH_EDITORONLY_DATA
		// Unreal 5.8 : l'identifiant de la ressource est rendu par un parametre de sortie
		FString LocResId;
		return FTextLocalizationManager::Get().GetLocResID(FTextKey(TEXT("BR")), FTextKey(Key), LocResId) && !LocResId.IsEmpty();
#else
		// v4.10 : GetLocResID n'existe qu'avec les donnees de l'editeur : le jeu empaquete ne compilait pas (Shipping,
		// Development jeu). Sans elles, la table ne garde pas l'origine d'un texte : une cle presente dans la table de la
		// langue courante est comptee traduite
		return FTextLocalizationManager::Get().FindDisplayString(FTextKey(TEXT("BR")), FTextKey(Key)).IsValid();
#endif
	}

	int32 KeyCount()
	{
		return UE_ARRAY_COUNT(BRLocKeyList);
	}

	bool IsReviewed(const FString& Code)
	{
		if (Code == TEXT("fr"))
		{
			return true; // langue source
		}
		static TArray<FString> Reviewed;
		static bool bRead = false;
		if (!bRead && GConfig)
		{
			bRead = true;
			GConfig->GetArray(TEXT("/Script/Backrooms.BRLocalization"), TEXT("ReviewedCultures"), Reviewed, GGameIni);
		}
		return Reviewed.ContainsByPredicate([&Code](const FString& R) { return R.Equals(Code, ESearchCase::IgnoreCase); });
	}

	int32 CountMissing(TArray<FString>* OutKeys)
	{
		if (Current().Code == FString(TEXT("fr")))
		{
			return 0; // langue source
		}
		int32 Count = 0;
		for (const TCHAR* Key : BRLocKeyList)
		{
			if (!HasTranslation(Key))
			{
				++Count;
				if (OutKeys)
				{
					OutKeys->Add(Key);
				}
			}
		}
		return Count;
	}

	void ApplyStartupLanguage()
	{
		// Langue choisie et enregistree, sinon celle du systeme (si le jeu la propose), sinon l'anglais
		FString Saved;
		BRConfig::Get().GetString(LanguageSection, TEXT("Language"), Saved);
		Preference() = Saved;
		SetLanguage(Saved.IsEmpty() ? DetectSystemLanguage() : Saved);
	}

	void SavePreference(const FString& Code)
	{
		Preference() = Code;
		BRConfig::Get().SetString(LanguageSection, TEXT("Language"), *Code);
		BRConfig::Save();
	}
}
