#include "BRSave.h"
#include "Backrooms.h"

#include "Kismet/GameplayStatics.h"

namespace BRSaves
{
	FString SlotName(int32 Slot)
	{
		return FString::Printf(TEXT("BR_Partie_%d"), Slot + 1);
	}

	UBRSaveGame* Load(int32 Slot)
	{
		if (Slot < 0 || Slot >= MaxSlots || !UGameplayStatics::DoesSaveGameExist(SlotName(Slot), 0))
		{
			return nullptr;
		}
		UBRSaveGame* Save = Cast<UBRSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName(Slot), 0));
		if (!Save)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Sauvegarde illisible : %s"), *SlotName(Slot));
		}
		return Save;
	}

	bool Write(int32 Slot, UBRSaveGame* Save)
	{
		if (!Save || Slot < 0 || Slot >= MaxSlots)
		{
			return false;
		}
		Save->LastPlayed = FDateTime::Now();
		const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SlotName(Slot), 0);
		if (!bOk)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Echec de l'ecriture de la sauvegarde %s"), *SlotName(Slot));
		}
		return bOk;
	}

	void Delete(int32 Slot)
	{
		if (Slot >= 0 && Slot < MaxSlots && UGameplayStatics::DoesSaveGameExist(SlotName(Slot), 0))
		{
			UGameplayStatics::DeleteGameInSlot(SlotName(Slot), 0);
		}
		if (ActiveSlot() == Slot)
		{
			ActiveSlot() = INDEX_NONE;
		}
	}

	int32 FreeSlot()
	{
		for (int32 i = 0; i < MaxSlots; ++i)
		{
			if (!UGameplayStatics::DoesSaveGameExist(SlotName(i), 0))
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	int32& ActiveSlot()
	{
		static int32 Slot = INDEX_NONE;
		return Slot;
	}

	FString FormatPlayTime(float Seconds)
	{
		const int32 Minutes = FMath::FloorToInt(FMath::Max(0.f, Seconds) / 60.f);
		if (Minutes < 60)
		{
			return FString::Printf(TEXT("%d min"), Minutes);
		}
		return FString::Printf(TEXT("%d h %02d"), Minutes / 60, Minutes % 60);
	}

	FString FormatDate(const FDateTime& Date)
	{
		return FString::Printf(TEXT("%02d/%02d/%04d %02d:%02d"), Date.GetDay(), Date.GetMonth(), Date.GetYear(), Date.GetHour(), Date.GetMinute());
	}
}
