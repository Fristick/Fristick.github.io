// Reproduction hors moteur de constats du prompt v4.12 sur la base v4.11 (commit d9c47f9).
//
// Chaque fonction ci-dessous est une copie de la base, ramenee a du C++ standard (TArray -> std::vector, FMath ->
// std::min/max) sans changer sa logique. Les references de lignes sont celles de d9c47f9. Ce banc ne compile pas le
// jeu : il execute le calcul tel qu'il est ecrit pour montrer le defaut.
//
//   g++ -std=c++17 -O1 -Wall -Wextra -o repro_base Docs/v412/repro_base_v411.cpp && ./repro_base
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
	// ---------------------------------------------------------------------------------- Objets (BRTypes.h, BRItems.cpp)
	enum class EItem : uint8_t { None, AlmondWater, Bandage, Battery, EnergyBar, VHSTape, Flashlight, Camcorder, Headlamp, Vest, Note, Count };
	enum class ESlot : uint8_t { Head, Chest, Hand, Belt, Count, None };
	struct FInfo { int MaxStack; ESlot Slot; bool bConsumable; };
	FInfo Info(EItem I)
	{
		switch (I)
		{
		case EItem::AlmondWater: return { 4, ESlot::None, true };
		case EItem::Bandage: return { 4, ESlot::None, true };
		case EItem::Battery: return { 6, ESlot::None, true };
		case EItem::EnergyBar: return { 4, ESlot::None, true };
		case EItem::VHSTape: return { 12, ESlot::None, false };
		case EItem::Flashlight: return { 1, ESlot::Hand, false };
		case EItem::Camcorder: return { 1, ESlot::None, false };
		case EItem::Headlamp: return { 1, ESlot::Head, false };
		case EItem::Vest: return { 1, ESlot::Chest, false };
		default: return { 1, ESlot::None, false };
		}
	}
	bool CanEquipIn(EItem Item, ESlot Slot) // BRItems.cpp:68
	{
		if (Item == EItem::None) return true;
		if (Item == EItem::Flashlight) return Slot == ESlot::Hand || Slot == ESlot::Belt;
		return Info(Item).Slot == Slot;
	}
	struct FSlot
	{
		EItem Item = EItem::None;
		int Count = 0;
		bool IsEmpty() const { return Item == EItem::None || Count <= 0; }
	};
	struct FChar
	{
		std::vector<FSlot> Pockets = std::vector<FSlot>(4);
		std::vector<FSlot> Storage = std::vector<FSlot>(20);
		std::vector<FSlot> Equipment = std::vector<FSlot>(4);

		// BRCharacter.cpp:1442 (base)
		int RoomFor(EItem Item) const
		{
			const int MaxStack = std::max(1, Info(Item).MaxStack);
			int Room = 0;
			for (const std::vector<FSlot>* Arr : { &Pockets, &Storage })
			{
				for (const FSlot& S : *Arr)
				{
					if (S.IsEmpty()) Room += MaxStack;
					else if (S.Item == Item) Room += std::max(0, MaxStack - S.Count);
				}
			}
			return Room;
		}

		// BRCharacter.cpp:338 (base)
		int AddItem(EItem Item, int Count)
		{
			const FInfo I = Info(Item);
			const int MaxStack = std::max(1, I.MaxStack);
			int Left = Count;
			for (std::vector<FSlot>* Arr : { &Pockets, &Storage })
			{
				for (FSlot& S : *Arr)
				{
					if (Left > 0 && S.Item == Item && S.Count < MaxStack)
					{
						const int Add = std::min(Left, MaxStack - S.Count);
						S.Count += Add;
						Left -= Add;
					}
				}
			}
			if (Left > 0 && I.Slot != ESlot::None)
			{
				for (int i = 0; i < static_cast<int>(Equipment.size()) && Left > 0; ++i)
				{
					if (Equipment[i].IsEmpty() && CanEquipIn(Item, static_cast<ESlot>(i)))
					{
						Equipment[i] = FSlot{ Item, 1 };
						--Left;
					}
				}
			}
			std::vector<std::vector<FSlot>*> Order = I.bConsumable ? std::vector<std::vector<FSlot>*>{ &Pockets, &Storage } : std::vector<std::vector<FSlot>*>{ &Storage, &Pockets };
			for (std::vector<FSlot>* Arr : Order)
			{
				for (FSlot& S : *Arr)
				{
					if (Left > 0 && S.IsEmpty())
					{
						const int Add = std::min(Left, MaxStack);
						S = FSlot{ Item, Add };
						Left -= Add;
					}
				}
			}
			return Left;
		}

		// BRCharacter.cpp:2161 (base) : partie objet
		bool ReceivePickup(EItem Item) { return AddItem(Item, 1) == 0; }

		int Count(EItem Item) const
		{
			int N = 0;
			for (const std::vector<FSlot>* Arr : { &Pockets, &Storage, &Equipment })
				for (const FSlot& S : *Arr) N += S.Item == Item ? S.Count : 0;
			return N;
		}
	};

	void FillAllBut(FChar& C, int FreeStorage)
	{
		// Poches et sac remplis de piles pleines (6), sauf FreeStorage cases du sac
		for (FSlot& S : C.Pockets) S = FSlot{ EItem::Battery, 6 };
		for (int i = 0; i < static_cast<int>(C.Storage.size()); ++i)
			C.Storage[i] = i < static_cast<int>(C.Storage.size()) - FreeStorage ? FSlot{ EItem::Battery, 6 } : FSlot{};
	}

	// ------------------------------------------------------------------------------------------- Soin (base 1282)
	struct FHeal
	{
		float Health = 100.f;
		float LastSentHealth = 100.f;
		uint16_t AckHealSerial = 3;
		float LastServerHealth = -1.f;
		int CurrentLevelSerial = 8;
		int Bandages = 1;

		// Branche acceptee de ClientHealResult_Implementation, copie fidele (RemoveItem ramene a un compteur)
		void OnAccepted(uint16_t Serial, float NewHealth, int LevelSerial)
		{
			const bool bStale = LevelSerial != CurrentLevelSerial;
			if (Bandages > 0) --Bandages;
			const bool bNewer = static_cast<int16_t>(Serial - AckHealSerial) > 0;
			if (bNewer)
			{
				AckHealSerial = Serial;
				LastServerHealth = NewHealth;
			}
			if (bStale)
			{
				if (bNewer)
				{
					Health = std::clamp(NewHealth, 0.f, 100.f);
					LastSentHealth = Health;
				}
				return;
			}
			Health = std::clamp(NewHealth, 0.f, 100.f); // ApplyHealAccepted
		}
	};
}

int main()
{
	int Problems = 0;
	std::printf("# Reproduction sur des copies des fonctions de la base v4.11 (d9c47f9)\n\n");

	// 4.2 : capacite annoncee, puis case occupee pendant l'attente
	{
		FChar C;
		FillAllBut(C, 1);
		C.Equipment[static_cast<int>(ESlot::Belt)] = FSlot{ EItem::Flashlight, 1 };
		const int Room = C.RoomFor(EItem::Bandage);
		// Pendant l'attente : la lampe de la ceinture est rangee dans la derniere case (MoveItem le permet)
		C.Storage.back() = C.Equipment[static_cast<int>(ESlot::Belt)];
		C.Equipment[static_cast<int>(ESlot::Belt)] = FSlot{};
		const int Before = C.Count(EItem::Bandage);
		const bool bStored = C.ReceivePickup(EItem::Bandage);
		const int After = C.Count(EItem::Bandage);
		std::printf("4.2  capacite envoyee a l'hote : %d ; lampe rangee dans la derniere case pendant l'attente\n", Room);
		std::printf("     reponse acceptee : ReceivePickup -> %s, bandages %d -> %d (l'hote a deja retire l'objet du monde)\n",
			bStored ? "true" : "false", Before, After);
		std::printf("     HandlePickupResult (base 1555) ignore le retour : l'objet accepte est perdu -> %s\n\n", (!bStored && After == Before) ? "REPRODUIT" : "non reproduit");
		Problems += (!bStored && After == Before) ? 1 : 0;
	}
	// 4.3 : faux inventaire plein pour une lampe
	{
		FChar C;
		FillAllBut(C, 0);
		C.Equipment[static_cast<int>(ESlot::Belt)] = FSlot{ EItem::Flashlight, 1 };
		const int Room = C.RoomFor(EItem::Flashlight);
		FChar Copy = C;
		const int Left = Copy.AddItem(EItem::Flashlight, 1);
		std::printf("4.3  poches et sac pleins, lampe a la ceinture, main libre\n");
		std::printf("     RoomFor(Lampe) = %d (demande refusee : Full) ; AddItem(Lampe, 1) -> reste %d, main = %s -> %s\n\n", Room, Left,
			Copy.Equipment[static_cast<int>(ESlot::Hand)].Item == EItem::Flashlight ? "lampe" : "vide", (Room == 0 && Left == 0) ? "REPRODUIT" : "non reproduit");
		Problems += (Room == 0 && Left == 0) ? 1 : 0;
	}
	// 4.6 : reponse de soin d'un ancien niveau qui ecrit la sante courante
	{
		FHeal H;
		H.Health = 100.f;
		H.OnAccepted(4, 35.f, 7);
		std::printf("4.6  sante 100, reponse de soin acceptee d'un ancien niveau (serie 7, niveau courant 8), NewHealth 35, numero plus recent\n");
		std::printf("     sante apres la branche bStale : %.0f -> %s\n\n", H.Health, H.Health == 35.f ? "REPRODUIT (100 -> 35)" : "non reproduit");
		Problems += H.Health == 35.f ? 1 : 0;
	}
	// 4.1 : hauteur du grimpeur au sommet et critere du rassemblement
	{
		struct FLadder { int Level; float Ceiling; float Shaft; };
		// Echelles de chunk (BRChunk.cpp:2421 : conduit de 300 cm sous plafond) et sorties garanties (Shaft = 0)
		const FLadder Ladders[] = { { 0, 290.f, 300.f }, { 2, 280.f, 300.f }, { 6, 290.f, 300.f }, { 8, 450.f, 300.f }, { 37, 450.f, 300.f },
			{ 6, 290.f, 0.f }, { 8, 450.f, 0.f }, { 37, 450.f, 0.f } };
		std::printf("4.1  sommet de la montee (BRInteractables.cpp:576) et critere \"pret\" (BRMissionWorld.cpp:1210, 1287)\n");
		int Fails = 0;
		for (const FLadder& L : Ladders)
		{
			const float Top = L.Shaft > 0.f ? L.Ceiling + L.Shaft * 0.5f : L.Ceiling - 88.f - 6.f; // centre de la capsule
			const float Gather = 150.f;
			const float PZ = Top, LZ = Gather;
			const bool bClose = std::fabs(PZ - LZ) < 260.f + (LZ > PZ + 100.f ? 250.f : 0.f);
			std::printf("     Niveau %2d, plafond %3.0f, conduit %3.0f : grimpeur a %3.0f cm, ecart %3.0f cm -> %s\n", L.Level, L.Ceiling, L.Shaft, Top, PZ - LZ,
				bClose ? "compte pret" : "PAS PRET (attend, la demande repart a chaque image)");
			Fails += bClose ? 0 : 1;
		}
		std::printf("     %d cas sur %d : le grimpeur au sommet n'est pas compte -> %s\n\n", Fails, static_cast<int>(sizeof(Ladders) / sizeof(Ladders[0])),
			Fails > 0 ? "REPRODUIT" : "non reproduit");
		Problems += Fails > 0 ? 1 : 0;
	}
	std::printf("Constats reproduits : %d sur 4\n", Problems);
	return 0;
}
