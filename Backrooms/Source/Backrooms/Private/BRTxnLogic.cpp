// v4.12 : regles de l'inventaire et des transactions (voir BRTxnLogic.h). C++ pur, sans moteur ni bibliotheque standard.
#include "BRTxnLogic.h"

namespace BRTxn
{
	namespace TxnImpl
	{
		constexpr uint8_t Bit(int Slot) { return static_cast<uint8_t>(1u << Slot); }

		// Memes valeurs qu'avant la v4.12 (BRItems.cpp) : piles, emplacements, consommables
		const FRule Rules[ItemCount] = {
			{ 0, 0, false },                                // ItemNone
			{ 4, 0, true },                                 // ItemAlmondWater
			{ 4, 0, true },                                 // ItemBandage
			{ 6, 0, true },                                 // ItemBattery
			{ 4, 0, true },                                 // ItemEnergyBar
			{ 12, 0, false },                               // ItemVHSTape
			{ 1, Bit(SlotHand) | Bit(SlotBelt), false },   // ItemFlashlight : main ou ceinture
			{ 1, 0, false },                                // ItemCamcorder : reste dans le sac
			{ 1, Bit(SlotHead), false },                    // ItemHeadlamp
			{ 1, Bit(SlotChest), false },                   // ItemVest
			{ 1, 0, false },                                // ItemNote
		};

		int MaxStackOf(uint8_t Item)
		{
			const int M = Rule(Item).MaxStack;
			return M > 1 ? M : 1;
		}

		void SwapSlots(FSlot& A, FSlot& B)
		{
			const FSlot T = A;
			A = B;
			B = T;
		}
	}

	const FRule& Rule(uint8_t Item)
	{
		return TxnImpl::Rules[Item < ItemCount ? Item : static_cast<uint8_t>(ItemNone)];
	}

	bool CanEquipIn(uint8_t Item, int Slot)
	{
		if (Item == ItemNone)
		{
			return true;
		}
		if (Slot < 0 || Slot >= NumEquip)
		{
			return false;
		}
		return (Rule(Item).EquipMask & TxnImpl::Bit(Slot)) != 0;
	}

	int PreferredSlot(uint8_t Item)
	{
		for (int S = 0; S < NumEquip; ++S)
		{
			if (Item != ItemNone && CanEquipIn(Item, S))
			{
				return S;
			}
		}
		return -1;
	}

	FSlot* FInv::Get(EGroup Group, int Index)
	{
		switch (Group)
		{
		case EGroup::Pockets: return Index >= 0 && Index < NumPockets ? &Pockets[Index] : nullptr;
		case EGroup::Storage: return Index >= 0 && Index < NumStorage ? &Storage[Index] : nullptr;
		case EGroup::Equipment: return Index >= 0 && Index < NumEquip ? &Equip[Index] : nullptr;
		default: return nullptr;
		}
	}

	const FSlot* FInv::Get(EGroup Group, int Index) const
	{
		return const_cast<FInv*>(this)->Get(Group, Index);
	}

	int Add(FInv& Inv, uint8_t Item, int Count, bool* bOutEquipChanged)
	{
		if (bOutEquipChanged)
		{
			*bOutEquipChanged = false;
		}
		if (Item == ItemNone || Item >= ItemCount || Count <= 0)
		{
			return Count > 0 ? Count : 0;
		}
		const FRule& R = Rule(Item);
		const int MaxStack = TxnImpl::MaxStackOf(Item);
		int Left = Count;
		// 1) Completer les piles existantes (poches, puis sac)
		FSlot* Groups[2] = { Inv.Pockets, Inv.Storage };
		const int Sizes[2] = { NumPockets, NumStorage };
		for (int G = 0; G < 2; ++G)
		{
			for (int I = 0; I < Sizes[G] && Left > 0; ++I)
			{
				FSlot& S = Groups[G][I];
				if (S.Item == Item && S.Count > 0 && S.Count < MaxStack)
				{
					const int Put = Left < MaxStack - S.Count ? Left : MaxStack - S.Count;
					S.Count += Put;
					Left -= Put;
				}
			}
		}
		// 2) Emplacement d'equipement libre et compatible (une lampe va directement dans la main)
		if (R.EquipMask != 0)
		{
			for (int Slot = 0; Slot < NumEquip && Left > 0; ++Slot)
			{
				if (Inv.Equip[Slot].IsEmpty() && CanEquipIn(Item, Slot))
				{
					Inv.Equip[Slot].Item = Item;
					Inv.Equip[Slot].Count = 1;
					--Left;
					if (bOutEquipChanged)
					{
						*bOutEquipChanged = true;
					}
				}
			}
		}
		// 3) Cases vides : poches d'abord pour les consommables, sac d'abord pour le reste
		const int First = R.bConsumable ? 0 : 1;
		for (int K = 0; K < 2; ++K)
		{
			const int G = K == 0 ? First : 1 - First;
			for (int I = 0; I < Sizes[G] && Left > 0; ++I)
			{
				FSlot& S = Groups[G][I];
				if (S.IsEmpty())
				{
					const int Put = Left < MaxStack ? Left : MaxStack;
					S.Item = Item;
					S.Count = Put;
					Left -= Put;
				}
			}
		}
		return Left;
	}

	int Capacity(const FInv& Inv, uint8_t Item)
	{
		FInv Copy = Inv;
		constexpr int Probe = 255;
		return Probe - Add(Copy, Item, Probe);
	}

	int Count(const FInv& Inv, uint8_t Item)
	{
		int N = 0;
		for (const FSlot& S : Inv.Pockets)
		{
			N += S.Item == Item ? S.Count : 0;
		}
		for (const FSlot& S : Inv.Storage)
		{
			N += S.Item == Item ? S.Count : 0;
		}
		for (const FSlot& S : Inv.Equip)
		{
			N += S.Item == Item ? S.Count : 0;
		}
		return N;
	}

	int Remove(FInv& Inv, uint8_t Item, int Count)
	{
		int Removed = 0;
		FSlot* Groups[3] = { Inv.Pockets, Inv.Storage, Inv.Equip };
		const int Sizes[3] = { NumPockets, NumStorage, NumEquip };
		for (int G = 0; G < 3; ++G)
		{
			for (int I = 0; I < Sizes[G]; ++I)
			{
				FSlot& S = Groups[G][I];
				while (Removed < Count && S.Item == Item && S.Count > 0)
				{
					--S.Count;
					++Removed;
					if (S.Count <= 0)
					{
						S.Clear();
					}
				}
			}
		}
		return Removed;
	}

	EMove Move(FInv& Inv, EGroup FromGroup, int FromIndex, EGroup ToGroup, int ToIndex, bool* bOutEquipChanged)
	{
		if (bOutEquipChanged)
		{
			*bOutEquipChanged = false;
		}
		FSlot* From = Inv.Get(FromGroup, FromIndex);
		FSlot* To = Inv.Get(ToGroup, ToIndex);
		if (!From || !To || From == To || From->IsEmpty())
		{
			return EMove::Invalid;
		}
		// Contraintes d'equipement
		if (ToGroup == EGroup::Equipment && !CanEquipIn(From->Item, ToIndex))
		{
			return EMove::CannotEquip;
		}
		if (FromGroup == EGroup::Equipment && !To->IsEmpty() && !CanEquipIn(To->Item, FromIndex))
		{
			return EMove::CannotSwapBack;
		}
		// Empilement
		const int MaxStack = TxnImpl::MaxStackOf(From->Item);
		if (To->Item == From->Item && MaxStack > 1 && ToGroup != EGroup::Equipment)
		{
			const int Put = From->Count < MaxStack - To->Count ? From->Count : MaxStack - To->Count;
			if (Put > 0)
			{
				To->Count += Put;
				From->Count -= Put;
				if (From->Count <= 0)
				{
					From->Clear();
				}
				return EMove::Stacked;
			}
		}
		// Une case d'equipement ne contient qu'un objet
		if (ToGroup == EGroup::Equipment && From->Count > 1)
		{
			if (!To->IsEmpty())
			{
				return EMove::EquipBusy;
			}
			To->Item = From->Item;
			To->Count = 1;
			From->Count -= 1;
		}
		else
		{
			TxnImpl::SwapSlots(*From, *To);
		}
		if (bOutEquipChanged)
		{
			*bOutEquipChanged = FromGroup == EGroup::Equipment || ToGroup == EGroup::Equipment;
		}
		return EMove::Moved;
	}

	bool Unequip(FInv& Inv, int Slot)
	{
		if (Slot < 0 || Slot >= NumEquip || Inv.Equip[Slot].IsEmpty())
		{
			return false;
		}
		for (FSlot& Free : Inv.Pockets)
		{
			if (Free.IsEmpty())
			{
				TxnImpl::SwapSlots(Free, Inv.Equip[Slot]);
				return true;
			}
		}
		for (FSlot& Free : Inv.Storage)
		{
			if (Free.IsEmpty())
			{
				TxnImpl::SwapSlots(Free, Inv.Equip[Slot]);
				return true;
			}
		}
		return false;
	}

	bool KeepsRoom(const FInv& After, uint8_t Reserved, int Need)
	{
		return Reserved == ItemNone || Capacity(After, Reserved) >= Need;
	}

	int FRecovery::NumItems() const
	{
		int N = 0;
		for (const FSlot& S : Slots)
		{
			N += S.IsEmpty() ? 0 : S.Count;
		}
		return N;
	}

	int FRecovery::Count(uint8_t Item) const
	{
		int N = 0;
		for (const FSlot& S : Slots)
		{
			N += S.Item == Item ? S.Count : 0;
		}
		return N;
	}

	int FRecovery::Put(uint8_t Item, int InCount)
	{
		if (Item == ItemNone || Item >= ItemCount || InCount <= 0)
		{
			return 0;
		}
		const int MaxStack = TxnImpl::MaxStackOf(Item);
		int Left = InCount;
		for (FSlot& S : Slots)
		{
			if (Left > 0 && S.Item == Item && S.Count > 0 && S.Count < MaxStack)
			{
				const int P = Left < MaxStack - S.Count ? Left : MaxStack - S.Count;
				S.Count += P;
				Left -= P;
			}
		}
		for (FSlot& S : Slots)
		{
			if (Left > 0 && S.IsEmpty())
			{
				const int P = Left < MaxStack ? Left : MaxStack;
				S.Item = Item;
				S.Count = P;
				Left -= P;
			}
		}
		return Left;
	}

	int FRecovery::Stow(FInv& Inv, bool* bOutEquipChanged)
	{
		if (bOutEquipChanged)
		{
			*bOutEquipChanged = false;
		}
		int Stowed = 0;
		for (FSlot& S : Slots)
		{
			if (S.IsEmpty())
			{
				continue;
			}
			bool bEquip = false;
			const int Left = Add(Inv, S.Item, S.Count, &bEquip);
			Stowed += S.Count - Left;
			S.Count = Left;
			if (S.Count <= 0)
			{
				S.Clear();
			}
			if (bEquip && bOutEquipChanged)
			{
				*bOutEquipChanged = true;
			}
		}
		return Stowed;
	}

	bool ShouldApplyHealth(uint8_t RespEpoch, uint8_t CurEpoch, uint16_t RespRev, uint16_t AckRev)
	{
		return RespEpoch == CurEpoch && IsNewer(RespRev, AckRev);
	}

	FHealOut DecideHeal(const FHealIn& In)
	{
		FHealOut Out;
		if (In.bRepeated)
		{
			Out.bIgnore = true;
			return Out;
		}
		const bool bSameLife = In.RespEpoch == In.CurEpoch;
		if (!In.bAccepted)
		{
			// Rien n'a ete consomme : le refus ne concerne que la demande en cours, dans ce niveau et cette vie
			Out.bNotifyRefusal = In.bWasPending && !In.bStaleLevel && bSameLife;
			return Out;
		}
		// Accepte : l'hote a consomme un objet de l'inventaire de cette vie-la. Une autre vie (reveil depuis) : l'objet
		// est parti avec cet inventaire, la nouvelle vie n'en perd aucun et sa sante n'est pas touchee
		Out.bRemoveItem = bSameLife;
		Out.bItemEffect = bSameLife;
		Out.bSetHealth = ShouldApplyHealth(In.RespEpoch, In.CurEpoch, In.RespRev, In.AckRev);
		// Aucun effet rejoue pour une reponse d'un niveau precedent
		Out.bEffects = bSameLife && !In.bStaleLevel;
		return Out;
	}

	FPickupOut DecidePickup(const FPickupIn& In)
	{
		FPickupOut Out;
		if (In.bRepeated)
		{
			Out.bIgnore = true;
			return Out;
		}
		if (!In.bAccepted)
		{
			Out.bNotifyRefusal = In.bWasPending && !In.bStaleLevel;
			return Out;
		}
		// Attribue par l'hote : l'accuse de reception part dans tous les cas (l'hote n'a plus a le rendre au monde)
		Out.bAck = true;
		if (In.RespEpoch != In.CurEpoch)
		{
			Out.bLostWithLife = true;
			return Out;
		}
		Out.bStore = true;
		Out.bEffects = !In.bStaleLevel;
		return Out;
	}

	EPickup DecideServerPickup(const FServerPickupIn& In)
	{
		if (!In.bLevelMatches || !In.bLevelReady)
		{
			return EPickup::StaleLevel;
		}
		if (In.bDead)
		{
			return EPickup::Dead;
		}
		if (In.bLoading)
		{
			return EPickup::Loading;
		}
		if (In.Epoch != In.ServerEpoch)
		{
			return EPickup::StaleLife;
		}
		if (In.bCollected)
		{
			return EPickup::AlreadyTaken;
		}
		if (!In.bExists || !In.bTypeMatches)
		{
			return EPickup::Unknown;
		}
		if (!In.bInReach)
		{
			return EPickup::TooFar;
		}
		if (!In.bVisible)
		{
			return EPickup::NotVisible;
		}
		if (In.Room <= 0)
		{
			return EPickup::Full;
		}
		return EPickup::Accepted;
	}

	void FLedger::Record(const FLedgerEntry& Entry)
	{
		Entries[Next] = Entry;
		Next = (Next + 1) % Size;
	}

	bool FLedger::Ack(uint16_t RequestId)
	{
		for (FLedgerEntry& E : Entries)
		{
			if (E.bAwaitingAck && E.RequestId == RequestId && RequestId != 0)
			{
				E.bAwaitingAck = false;
				return true;
			}
		}
		return false;
	}

	int FLedger::Unacked(int32_t LevelSerial, FLedgerEntry* Out, int Max) const
	{
		int N = 0;
		for (const FLedgerEntry& E : Entries)
		{
			if (E.bAwaitingAck && E.LevelSerial == LevelSerial && N < Max)
			{
				Out[N++] = E;
			}
		}
		return N;
	}
}
