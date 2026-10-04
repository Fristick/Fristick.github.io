#include "BRConfig.h"
#include "Backrooms.h"

#include "HAL/PlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace
{
	// Sections des versions precedentes (dans GameUserSettings.ini)
	const TCHAR* LegacySections[] = { TEXT("/Script/Backrooms.BRSettings"), TEXT("/Script/Backrooms.BRKeys") };
}

namespace BRConfig
{
	const FString& Path()
	{
		static const FString File = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::GeneratedConfigDir(), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()), TEXT("BackroomsPlayer.ini")));
		return File;
	}

	FConfigFile& Get()
	{
		static FConfigFile File;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			if (FPaths::FileExists(Path()))
			{
				File.Read(Path());
			}
			else if (GConfig)
			{
				// Premiere utilisation : on reprend les reglages et les touches enregistres par une version precedente
				bool bMigrated = false;
				for (const TCHAR* Section : LegacySections)
				{
					TArray<FString> Lines;
					if (!GConfig->GetSection(Section, Lines, GGameUserSettingsIni))
					{
						continue;
					}
					for (const FString& Line : Lines)
					{
						FString Key;
						FString Value;
						if (Line.Split(TEXT("="), &Key, &Value))
						{
							File.SetString(Section, *Key, *Value);
							bMigrated = true;
						}
					}
				}
				if (bMigrated)
				{
					UE_LOG(LogBackrooms, Log, TEXT("Reglages repris de GameUserSettings.ini vers %s"), *Path());
					Save();
				}
			}
		}
		return File;
	}

	void Save()
	{
		Get().Write(Path());
	}
}
