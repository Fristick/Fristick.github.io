// v4.12 : banc hors moteur des transactions de l'equipe (inventaire, ramassages, soins).
//
// Il compile BRTxnLogic.cpp, le code exact que le jeu utilise (ABRCharacter, ABRWorld), et verifie :
//   A. les regles de l'inventaire : meme comportement que l'ajout d'avant la v4.12, place annoncee = place reelle,
//      cas du faux "inventaire plein" (lampe, frontale, gilet, piles partielles) ;
//   B. la place reservee pendant une demande de ramassage (deplacements quelconques : l'objet accepte rentre toujours) ;
//   C. la reserve "mis de cote" ;
//   D. les decisions sur les reponses de l'hote (soin, ramassage, coup), dont le constat 4.6 (100 -> 35) ;
//   E. des scenarios de bout en bout avec un hote et deux a quatre joueurs : objet dispute, inventaire plein, derniere
//      case occupee pendant l'attente, mort apres l'acceptation, reponse apres un changement de niveau, reponse repetee,
//      deconnexion, soins. Apres chaque scenario : objet du monde, inventaires, stock de soin de l'hote et compteurs.
//
// Le modele d'hote reprend l'ordre des operations de ABRWorld::ServerTryCollect et ABRCharacter (memes decisions,
// memes registres) ; il ne remplace pas un vrai essai reseau (prepare dans -BRNetTest).
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Transactions/test_transactions.cpp
//       Source/Backrooms/Private/BRTxnLogic.cpp -o test_transactions && ./test_transactions
#include "BRTxnLogic.h"

#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace BRTxn;

namespace
{
	int GFailures = 0;
	int GChecks = 0;

	void Check(bool bOk, const char* What)
	{
		++GChecks;
		if (!bOk)
		{
			++GFailures;
			std::printf("  ECHEC : %s\n", What);
		}
	}

	bool SameInv(const FInv& A, const FInv& B)
	{
		auto Eq = [](const FSlot& X, const FSlot& Y) { return (X.IsEmpty() && Y.IsEmpty()) || (X.Item == Y.Item && X.Count == Y.Count); };
		for (int I = 0; I < NumPockets; ++I) if (!Eq(A.Pockets[I], B.Pockets[I])) return false;
		for (int I = 0; I < NumStorage; ++I) if (!Eq(A.Storage[I], B.Storage[I])) return false;
		for (int I = 0; I < NumEquip; ++I) if (!Eq(A.Equip[I], B.Equip[I])) return false;
		return true;
	}

	// ---------------------------------------------------------------------- reference : l'ajout et la place d'avant
	// Copies de ABRCharacter::AddItem et ::RoomFor de la base v4.11 (d9c47f9), pour comparer
	int BaseAdd(FInv& Inv, uint8_t Item, int Count)
	{
		const FRule& R = Rule(Item);
		const int MaxStack = R.MaxStack > 1 ? R.MaxStack : 1;
		int Left = Count;
		FSlot* G[2] = { Inv.Pockets, Inv.Storage };
		const int N[2] = { NumPockets, NumStorage };
		for (int g = 0; g < 2; ++g)
			for (int i = 0; i < N[g]; ++i)
				if (Left > 0 && G[g][i].Item == Item && G[g][i].Count < MaxStack)
				{
					const int Add = Left < MaxStack - G[g][i].Count ? Left : MaxStack - G[g][i].Count;
					G[g][i].Count += Add;
					Left -= Add;
				}
		if (Left > 0 && R.EquipMask != 0)
			for (int i = 0; i < NumEquip && Left > 0; ++i)
				if (Inv.Equip[i].IsEmpty() && CanEquipIn(Item, i))
				{
					Inv.Equip[i].Item = Item;
					Inv.Equip[i].Count = 1;
					--Left;
				}
		const int First = R.bConsumable ? 0 : 1;
		for (int k = 0; k < 2; ++k)
		{
			const int g = k == 0 ? First : 1 - First;
			for (int i = 0; i < N[g]; ++i)
				if (Left > 0 && G[g][i].IsEmpty())
				{
					const int Add = Left < MaxStack ? Left : MaxStack;
					G[g][i].Item = Item;
					G[g][i].Count = Add;
					Left -= Add;
				}
		}
		return Left;
	}

	int BaseRoomFor(const FInv& Inv, uint8_t Item)
	{
		const int MaxStack = Rule(Item).MaxStack > 1 ? Rule(Item).MaxStack : 1;
		int Room = 0;
		for (const FSlot& S : Inv.Pockets) Room += S.IsEmpty() ? MaxStack : (S.Item == Item ? (MaxStack - S.Count > 0 ? MaxStack - S.Count : 0) : 0);
		for (const FSlot& S : Inv.Storage) Room += S.IsEmpty() ? MaxStack : (S.Item == Item ? (MaxStack - S.Count > 0 ? MaxStack - S.Count : 0) : 0);
		return Room;
	}

	// Inventaire au hasard (objets valides a leur place)
	FInv RandomInv(std::mt19937& Rng, int FillPercent)
	{
		FInv Inv;
		std::uniform_int_distribution<int> Pct(0, 99);
		const uint8_t Bag[] = { ItemAlmondWater, ItemBandage, ItemBattery, ItemEnergyBar, ItemVHSTape, ItemFlashlight, ItemCamcorder, ItemHeadlamp, ItemVest };
		std::uniform_int_distribution<int> Pick(0, static_cast<int>(sizeof(Bag)) - 1);
		auto Fill = [&](FSlot& S)
		{
			if (Pct(Rng) < FillPercent)
			{
				S.Item = Bag[Pick(Rng)];
				const int M = Rule(S.Item).MaxStack > 1 ? Rule(S.Item).MaxStack : 1;
				S.Count = std::uniform_int_distribution<int>(1, M)(Rng);
			}
		};
		for (FSlot& S : Inv.Pockets) Fill(S);
		for (FSlot& S : Inv.Storage) Fill(S);
		for (int E = 0; E < NumEquip; ++E)
		{
			if (Pct(Rng) < FillPercent)
			{
				for (uint8_t It : { ItemFlashlight, ItemHeadlamp, ItemVest })
				{
					if (CanEquipIn(It, E))
					{
						Inv.Equip[E].Item = It;
						Inv.Equip[E].Count = 1;
						break;
					}
				}
			}
		}
		return Inv;
	}

	FInv FullBags(uint8_t With = ItemBattery)
	{
		FInv Inv;
		for (FSlot& S : Inv.Pockets) { S.Item = With; S.Count = Rule(With).MaxStack; }
		for (FSlot& S : Inv.Storage) { S.Item = With; S.Count = Rule(With).MaxStack; }
		return Inv;
	}

	// ------------------------------------------------------------------------------------------------ A. inventaire
	void TestInventoryRules()
	{
		std::printf("A. Regles de l'inventaire\n");
		std::mt19937 Rng(4120);
		int Same = 0, Total = 0, CapOk = 0, FalseFullBase = 0, FalseFullNew = 0;
		for (int T = 0; T < 4000; ++T)
		{
			const FInv Inv = RandomInv(Rng, 30 + (T % 70));
			for (uint8_t It = ItemAlmondWater; It <= ItemVest; ++It)
			{
				// A1 : meme resultat que l'ajout de la base
				for (int C = 1; C <= 5; ++C)
				{
					FInv A = Inv, B = Inv;
					const int La = Add(A, It, C);
					const int Lb = BaseAdd(B, It, C);
					Same += (La == Lb && SameInv(A, B)) ? 1 : 0;
					++Total;
				}
				// A2 : la place annoncee est la place reelle (exemplaire par exemplaire)
				const int Cap = Capacity(Inv, It);
				FInv Seq = Inv;
				int Placed = 0;
				while (Placed < Cap && Add(Seq, It, 1) == 0) ++Placed;
				const bool bNextFails = Cap >= 255 || Add(Seq, It, 1) == 1;
				CapOk += (Placed == Cap && bNextFails) ? 1 : 0;
				// A4 : faux "plein" de la base (place 0 alors que l'ajout reussit)
				FInv Probe = Inv;
				const bool bFits = Add(Probe, It, 1) == 0;
				FalseFullBase += (BaseRoomFor(Inv, It) == 0 && bFits) ? 1 : 0;
				FalseFullNew += (Capacity(Inv, It) == 0 && bFits) ? 1 : 0;
			}
		}
		std::printf("  ajout identique a celui de la base : %d/%d\n", Same, Total);
		std::printf("  place annoncee = exemplaires reellement ranges : %d/%d\n", CapOk, 4000 * 9);
		std::printf("  faux \"inventaire plein\" (place 0, ajout possible) : base %d, v4.12 %d\n", FalseFullBase, FalseFullNew);
		Check(Same == Total, "l'ajout v4.12 change le comportement de l'ajout de la base");
		Check(CapOk == 4000 * 9, "la place annoncee differe des exemplaires ranges");
		Check(FalseFullBase > 0 && FalseFullNew == 0, "faux inventaire plein");

		// A3 : cas du constat 4.3 et voisins
		struct FCase { const char* Name; FInv Inv; uint8_t Item; int Expected; int ExpectSlot; };
		std::vector<FCase> Cases;
		{
			FInv I = FullBags(); I.Equip[SlotBelt] = { ItemFlashlight, 1 };
			Cases.push_back({ "sacs pleins, lampe a la ceinture, main libre : lampe", I, ItemFlashlight, 1, SlotHand });
		}
		{
			FInv I = FullBags(); I.Equip[SlotHand] = { ItemFlashlight, 1 };
			Cases.push_back({ "sacs pleins, lampe en main, ceinture libre : lampe", I, ItemFlashlight, 1, SlotBelt });
		}
		{
			FInv I = FullBags(); I.Equip[SlotHand] = { ItemFlashlight, 1 }; I.Equip[SlotBelt] = { ItemFlashlight, 1 };
			Cases.push_back({ "sacs pleins, main et ceinture prises : lampe", I, ItemFlashlight, 0, -1 });
		}
		{
			FInv I = FullBags();
			Cases.push_back({ "sacs pleins, tete libre : frontale", I, ItemHeadlamp, 1, SlotHead });
		}
		{
			FInv I = FullBags(); I.Equip[SlotChest] = { ItemVest, 1 };
			Cases.push_back({ "sacs pleins, torse pris : gilet", I, ItemVest, 0, -1 });
		}
		{
			FInv I = FullBags(); I.Pockets[0] = { ItemBandage, 2 };
			Cases.push_back({ "sacs pleins, pile de 2 bandages : bandage", I, ItemBandage, 2, -1 });
		}
		{
			FInv I = FullBags(ItemVHSTape);
			Cases.push_back({ "sacs pleins de cassettes (12/12) : piles", I, ItemBattery, 0, -1 });
		}
		{
			FInv I;
			Cases.push_back({ "inventaire vide : bandage (24 cases x 4)", I, ItemBandage, 96, -1 });
			Cases.push_back({ "inventaire vide : lampe (2 emplacements + 24 cases)", I, ItemFlashlight, 26, SlotHand });
		}
		for (const FCase& C : Cases)
		{
			const int Cap = Capacity(C.Inv, C.Item);
			FInv After = C.Inv;
			const int Left = Add(After, C.Item, 1);
			const bool bSlotOk = C.ExpectSlot < 0 || (After.Equip[C.ExpectSlot].Item == C.Item);
			const bool bOk = Cap == C.Expected && (C.Expected == 0 ? Left == 1 : (Left == 0 && bSlotOk));
			std::printf("  %-58s place %3d (attendu %3d) %s\n", C.Name, Cap, C.Expected, bOk ? "OK" : "ECHEC");
			Check(bOk, C.Name);
		}
	}

	// ------------------------------------------------------------------------------------- B. place reservee
	// Un deplacement au hasard, comme le glisser-deposer et le double-clic (desequiper) de l'inventaire
	bool RandomMove(std::mt19937& Rng, FInv& Inv, bool bReserve, uint8_t Reserved, int& Refused)
	{
		FInv After = Inv;
		std::uniform_int_distribution<int> G(0, 2), Kind(0, 5);
		bool bDone = false;
		if (Kind(Rng) == 0)
		{
			bDone = Unequip(After, std::uniform_int_distribution<int>(0, NumEquip - 1)(Rng));
		}
		else
		{
			const EGroup From = static_cast<EGroup>(G(Rng)), To = static_cast<EGroup>(G(Rng));
			const int MaxF = From == EGroup::Pockets ? NumPockets : (From == EGroup::Storage ? NumStorage : NumEquip);
			const int MaxT = To == EGroup::Pockets ? NumPockets : (To == EGroup::Storage ? NumStorage : NumEquip);
			const EMove R = Move(After, From, std::uniform_int_distribution<int>(0, MaxF - 1)(Rng), To, std::uniform_int_distribution<int>(0, MaxT - 1)(Rng));
			bDone = R == EMove::Moved || R == EMove::Stacked;
		}
		if (!bDone)
		{
			return false;
		}
		if (bReserve && !KeepsRoom(After, Reserved))
		{
			++Refused;
			return false;
		}
		Inv = After;
		return true;
	}

	void TestReservation()
	{
		std::printf("B. Place reservee pendant l'attente de l'hote\n");
		std::mt19937 Rng(4121);
		int Scenarios = 0, LostWithout = 0, LostWith = 0, Refused = 0, Moves = 0;
		for (int T = 0; T < 20000; ++T)
		{
			FInv Inv = RandomInv(Rng, 70 + (T % 30));
			const uint8_t Item = static_cast<uint8_t>(std::uniform_int_distribution<int>(ItemAlmondWater, ItemVest)(Rng));
			if (Item == ItemCamcorder || Capacity(Inv, Item) < 1)
			{
				continue; // la demande n'est pas envoyee sans place
			}
			++Scenarios;
			FInv With = Inv, Without = Inv;
			const int N = std::uniform_int_distribution<int>(1, 12)(Rng);
			std::mt19937 RngA(T), RngB(T);
			int Dummy = 0;
			for (int K = 0; K < N; ++K)
			{
				Moves += RandomMove(RngA, With, true, Item, Refused) ? 1 : 0;
				RandomMove(RngB, Without, false, Item, Dummy);
			}
			LostWith += Add(With, Item, 1) > 0 ? 1 : 0;
			LostWithout += Add(Without, Item, 1) > 0 ? 1 : 0;
		}
		std::printf("  %d demandes, deplacements au hasard pendant l'attente : %d faits, %d refuses (place reservee)\n", Scenarios, Moves, Refused);
		std::printf("  objet accepte sans place a la reponse : sans reserve %d, avec reserve %d\n", LostWithout, LostWith);
		Check(LostWithout > 0, "le banc ne reproduit pas le risque sans reserve");
		Check(LostWith == 0, "un objet accepte ne trouve pas sa place malgre la reserve");

		// Le scenario du constat 4.2 : une case libre, bandage demande, lampe de la ceinture rangee dans cette case
		FInv Inv = FullBags();
		Inv.Storage[NumStorage - 1].Clear();
		Inv.Equip[SlotBelt] = { ItemFlashlight, 1 };
		FInv After = Inv;
		const EMove R = Move(After, EGroup::Equipment, SlotBelt, EGroup::Storage, NumStorage - 1);
		const bool bRefused = R == EMove::Moved && !KeepsRoom(After, ItemBandage);
		std::printf("  constat 4.2 : place du bandage %d, lampe rangee dans la derniere case -> %s\n", Capacity(Inv, ItemBandage), bRefused ? "deplacement refuse" : "ACCEPTE");
		Check(bRefused, "le deplacement qui prend la place reservee n'est pas refuse");
	}

	// ----------------------------------------------------------------------------------------- C. mis de cote
	void TestRecovery()
	{
		std::printf("C. Reserve \"mis de cote\"\n");
		FInv Inv = FullBags();
		FRecovery Rec;
		const int Left = Add(Inv, ItemBandage, 1);
		const int NotKept = Rec.Put(ItemBandage, Left);
		Check(Left == 1 && NotKept == 0 && Rec.Count(ItemBandage) == 1, "objet sans place non mis de cote");
		Check(Rec.Stow(Inv) == 0, "rangement sans place");
		Inv.Pockets[2].Clear(); // on utilise des piles : une case se libere
		const int Stowed = Rec.Stow(Inv);
		std::printf("  bandage sans place : mis de cote %d, range apres une case liberee : %d, reste %d\n", Left - NotKept, Stowed, Rec.NumItems());
		Check(Stowed == 1 && Rec.NumItems() == 0 && Count(Inv, ItemBandage) == 1, "objet mis de cote non range");
		// Lampe mise de cote : rangee dans la main des qu'elle se libere
		FInv I2 = FullBags();
		I2.Equip[SlotHand] = { ItemFlashlight, 1 };
		I2.Equip[SlotBelt] = { ItemFlashlight, 1 };
		FRecovery R2;
		R2.Put(ItemFlashlight, Add(I2, ItemFlashlight, 1));
		I2.Equip[SlotHand].Clear();
		bool bEquip = false;
		Check(R2.Stow(I2, &bEquip) == 1 && bEquip && I2.Equip[SlotHand].Item == ItemFlashlight, "lampe mise de cote non equipee");
		// Piles : la reserve empile
		FRecovery R3;
		R3.Put(ItemBattery, 4);
		R3.Put(ItemBattery, 5);
		Check(R3.Count(ItemBattery) == 9 && R3.Slots[0].Count == 6 && R3.Slots[1].Count == 3, "reserve : empilement");
	}

	// ----------------------------------------------------------------------------------------- D. decisions
	void TestDecisions()
	{
		std::printf("D. Decisions sur les reponses de l'hote\n");
		// Constat 4.6 : sante 100 apres un reveil (epoque 1, revision 10), reponse de soin d'avant (epoque 0, revision 9, 35)
		{
			FHealIn In;
			In.bAccepted = true;
			In.bStaleLevel = true;
			In.RespEpoch = 0;
			In.CurEpoch = 1;
			In.RespRev = 9;
			In.AckRev = 10;
			const FHealOut Out = DecideHeal(In);
			float Health = 100.f;
			if (Out.bSetHealth) Health = 35.f;
			std::printf("  constat 4.6 : sante 100 apres un reveil, soin d'avant (35) -> sante %.0f, objet retire : %s\n", Health, Out.bRemoveItem ? "oui" : "non");
			Check(Health == 100.f && !Out.bRemoveItem && !Out.bEffects, "soin perime : etat recent ecrase ou objet neuf consomme");
		}
		// Meme vie, changement de niveau pendant l'attente, aucun etat plus recent : la quantite et la sante officielle
		// sont reconciliees, sans effet rejoue
		{
			FHealIn In;
			In.bAccepted = true;
			In.bWasPending = true;
			In.bStaleLevel = true;
			In.RespEpoch = In.CurEpoch = 0;
			In.RespRev = 12;
			In.AckRev = 11;
			const FHealOut Out = DecideHeal(In);
			Check(Out.bRemoveItem && Out.bSetHealth && !Out.bEffects && Out.bItemEffect, "soin apres changement de niveau : quantite et sante");
		}
		// Un coup recu apres le soin (revision plus recente) : le soin ne l'ecrase pas
		{
			FHealIn In;
			In.bAccepted = true;
			In.bWasPending = true;
			In.RespEpoch = In.CurEpoch = 0;
			In.RespRev = 20;
			In.AckRev = 21;
			const FHealOut Out = DecideHeal(In);
			Check(Out.bRemoveItem && !Out.bSetHealth && Out.bEffects, "soin plus ancien qu'un coup recu");
		}
		// Repetition, refus
		{
			FHealIn In;
			In.bAccepted = true;
			In.bRepeated = true;
			Check(DecideHeal(In).bIgnore, "soin repete");
			FHealIn Ref;
			Ref.bWasPending = true;
			Check(DecideHeal(Ref).bNotifyRefusal, "refus de la demande en cours");
			Ref.bStaleLevel = true;
			Check(!DecideHeal(Ref).bNotifyRefusal, "refus d'un ancien niveau annonce");
		}
		// Ramassage
		{
			FPickupIn In;
			In.bAccepted = true;
			In.bWasPending = true;
			FPickupOut Out = DecidePickup(In);
			Check(Out.bStore && Out.bAck && Out.bEffects, "ramassage accepte");
			In.bStaleLevel = true;
			Out = DecidePickup(In);
			Check(Out.bStore && Out.bAck && !Out.bEffects, "ramassage accepte, ancien niveau");
			In.RespEpoch = 0;
			In.CurEpoch = 1;
			Out = DecidePickup(In);
			Check(!Out.bStore && Out.bLostWithLife && Out.bAck, "ramassage accepte avant un reveil");
			In.bRepeated = true;
			Check(DecidePickup(In).bIgnore, "ramassage repete");
		}
		// Coups : meme vie et revision plus recente, en tenant compte du bouclage des numeros
		Check(ShouldApplyHealth(2, 2, 5, 4) && !ShouldApplyHealth(2, 3, 5, 4) && !ShouldApplyHealth(2, 2, 4, 4) && ShouldApplyHealth(0, 0, 1, 65535),
			"revision de la sante");
		// Hote : ordre des refus
		FServerPickupIn S;
		Check(DecideServerPickup(S) == EPickup::Accepted, "hote : acceptation");
		S.Room = 0;
		Check(DecideServerPickup(S) == EPickup::Full, "hote : plein");
		S.Room = 1;
		S.Epoch = 1;
		Check(DecideServerPickup(S) == EPickup::StaleLife, "hote : autre vie");
		S.bCollected = true;
		S.Epoch = 0;
		Check(DecideServerPickup(S) == EPickup::AlreadyTaken, "hote : deja pris");
		S.bLevelMatches = false;
		Check(DecideServerPickup(S) == EPickup::StaleLevel, "hote : autre niveau");
		std::printf("  decisions : %s\n", GFailures == 0 ? "OK" : "ECHECS");
	}

	// ----------------------------------------------------------------------------- E. hote et joueurs (modele)
	struct FMsg
	{
		enum EKind { PickupResult, HealResult, WakeAck, Hit } Kind;
		uint64_t PickupId = 0;
		uint16_t RequestId = 0;
		int32_t LevelSerial = 0;
		uint8_t Item = 0;
		uint8_t Result = 0;
		uint8_t Epoch = 0;
		uint16_t Rev = 0;
		float Health = 0.f;
	};

	struct FHost;

	struct FClient
	{
		int Index = 0;
		FInv Inv;
		FRecovery Rec;
		uint8_t KnownWake = 0;
		uint8_t PendingWake = 0; // signalements de reveil sans reponse (ABRCharacter::PendingWakeReports)
		uint16_t AckRev = 0;
		float Health = 100.f;
		uint16_t Applied[32] = {};
		int AppliedNext = 0;
		uint16_t NextReq = 0;
		uint16_t PendingPickupReq = 0;
		uint8_t PendingItem = 0;
		uint16_t PendingHealReq = 0;
		std::deque<FMsg> Inbox;
		bool bConnected = true;
		int Effects = 0;
		int Lost = 0;
		int Stored = 0;
		int Recovered = 0;
		std::vector<uint16_t> AcksToSend;

		uint8_t Epoch() const { return static_cast<uint8_t>(KnownWake + PendingWake); }
		bool WasApplied(uint16_t R) const { for (uint16_t A : Applied) if (A == R && R != 0) return true; return false; }
		void Remember(uint16_t R) { Applied[AppliedNext] = R; AppliedNext = (AppliedNext + 1) % 32; }
		int Heals() const { return Count(Inv, ItemBandage) + Count(Inv, ItemAlmondWater) + Rec.Count(ItemBandage) + Rec.Count(ItemAlmondWater); }
		void StartingGear()
		{
			Inv = FInv();
			Rec = FRecovery();
			Inv.Pockets[0] = { ItemAlmondWater, 1 };
			Inv.Pockets[1] = { ItemBandage, 1 };
			Inv.Pockets[2] = { ItemBattery, 1 };
			Inv.Storage[0] = { ItemCamcorder, 1 };
			Inv.Equip[SlotBelt] = { ItemFlashlight, 1 };
		}
	};

	struct FWorldPickup
	{
		uint8_t Item;
		float X, Y, Z;
	};

	struct FHost
	{
		int32_t LevelSerial = 1;
		std::map<uint64_t, FWorldPickup> Pickups;
		std::set<uint64_t> Collected;
		int Lore = 0;
		struct FPlayer
		{
			uint8_t Wake = 0;
			int Stock[2] = { 1, 1 }; // eau, bandage
			uint16_t Rev = 0;
			float Health = 100.f;
			bool bDead = false;
			FLedger Ledger;
			std::map<uint16_t, FMsg> History;
		};
		std::vector<FPlayer> Players;
		std::vector<FClient>* Clients = nullptr;
		uint64_t NextReturnId = 0xF000;

		void Send(int P, const FMsg& M)
		{
			FClient& C = (*Clients)[P];
			if (C.bConnected)
			{
				C.Inbox.push_back(M);
			}
		}

		// ABRCharacter::ServerRequestPickup_Implementation + ABRWorld::ServerTryCollect
		void RequestPickup(int P, uint64_t Id, uint16_t Req, int32_t Serial, uint8_t Expected, int Room, uint8_t Epoch)
		{
			FPlayer& Pl = Players[P];
			const auto H = Pl.History.find(Req);
			if (H != Pl.History.end())
			{
				Send(P, H->second);
				return;
			}
			FServerPickupIn In;
			In.bLevelMatches = Serial == LevelSerial;
			In.bDead = Pl.bDead;
			In.Epoch = Epoch;
			In.ServerEpoch = Pl.Wake;
			In.bCollected = Collected.count(Id) != 0;
			const auto It = Pickups.find(Id);
			In.bExists = It != Pickups.end();
			In.bTypeMatches = In.bExists && It->second.Item == Expected;
			In.Room = Room;
			const EPickup R = DecideServerPickup(In);
			FMsg M;
			M.Kind = FMsg::PickupResult;
			M.PickupId = Id;
			M.RequestId = Req;
			M.LevelSerial = Serial;
			M.Item = Expected;
			M.Result = static_cast<uint8_t>(R);
			M.Epoch = Pl.Wake;
			if (R == EPickup::Accepted)
			{
				const FWorldPickup W = It->second;
				Collected.insert(Id);
				Pickups.erase(It);
				if (W.Item == ItemAlmondWater) ++Pl.Stock[0];
				if (W.Item == ItemBandage) ++Pl.Stock[1];
				if (W.Item == ItemVHSTape) ++Lore;
				FLedgerEntry E;
				E.RequestId = Req;
				E.PickupId = Id;
				E.Item = W.Item;
				E.Epoch = Pl.Wake;
				E.LevelSerial = Serial;
				E.X = W.X;
				E.Y = W.Y;
				E.Z = W.Z;
				E.bAwaitingAck = true;
				Pl.Ledger.Record(E);
			}
			Pl.History[Req] = M;
			Send(P, M);
		}

		void Ack(int P, uint16_t Req) { Players[P].Ledger.Ack(Req); }

		// ABRCharacter::ServerRequestHeal_Implementation (sans le delai entre deux soins)
		void RequestHeal(int P, uint8_t Item, uint16_t Req, int32_t Serial, uint8_t Epoch)
		{
			FPlayer& Pl = Players[P];
			const auto H = Pl.History.find(static_cast<uint16_t>(Req | 0x8000));
			if (H != Pl.History.end())
			{
				Send(P, H->second);
				return;
			}
			uint8_t Reason = 0;
			const int K = Item == ItemAlmondWater ? 0 : 1;
			if (Pl.bDead) Reason = 2;
			else if (Serial != LevelSerial) Reason = 7;
			else if (Epoch != Pl.Wake) Reason = 8;
			else if (Pl.Stock[K] <= 0) Reason = 3;
			FMsg M;
			M.Kind = FMsg::HealResult;
			M.RequestId = Req;
			M.LevelSerial = Serial;
			M.Item = Item;
			M.Result = Reason;
			M.Epoch = Pl.Wake;
			if (Reason == 0)
			{
				Pl.Health = Pl.Health + 35.f > 100.f ? 100.f : Pl.Health + 35.f;
				++Pl.Rev;
				--Pl.Stock[K];
			}
			M.Rev = Pl.Rev;
			M.Health = Pl.Health;
			Pl.History[static_cast<uint16_t>(Req | 0x8000)] = M;
			Send(P, M);
		}

		void Hit(int P, float Damage)
		{
			FPlayer& Pl = Players[P];
			Pl.Health = Pl.Health - Damage < 0.f ? 0.f : Pl.Health - Damage;
			++Pl.Rev;
			FMsg M;
			M.Kind = FMsg::Hit;
			M.Rev = Pl.Rev;
			M.Epoch = Pl.Wake;
			M.Health = Pl.Health;
			Send(P, M);
		}

		// Mort puis reveil au point de depart (ServerApplyDeathState, evenement 3)
		void Die(int P) { Players[P].bDead = true; ++Players[P].Rev; }
		void Wake(int P)
		{
			FPlayer& Pl = Players[P];
			Pl.bDead = false;
			Pl.Health = 100.f;
			++Pl.Rev;
			++Pl.Wake;
			Pl.Stock[0] = Pl.Stock[1] = 1;
			FMsg M;
			M.Kind = FMsg::WakeAck;
			M.Epoch = Pl.Wake;
			M.Rev = Pl.Rev;
			Send(P, M);
		}

		// ABRCharacter::EndPlay + ABRWorld::ServerReturnPickup
		void Disconnect(int P)
		{
			FLedgerEntry Out[FLedger::Size];
			const int N = Players[P].Ledger.Unacked(LevelSerial, Out, FLedger::Size);
			for (int I = 0; I < N; ++I)
			{
				const uint64_t NewId = NextReturnId++;
				Pickups[NewId] = { Out[I].Item, Out[I].X, Out[I].Y, Out[I].Z };
				if (Out[I].Item == ItemVHSTape && Lore > 0) --Lore;
				Players[P].Ledger.Ack(Out[I].RequestId);
			}
		}
	};

	struct FSim
	{
		FHost Host;
		std::vector<FClient> Clients;

		explicit FSim(int N)
		{
			Clients.resize(N);
			Host.Players.resize(N);
			Host.Clients = &Clients;
			for (int I = 0; I < N; ++I)
			{
				Clients[I].Index = I;
				Clients[I].StartingGear();
			}
		}

		// ABRCharacter::RequestPickup
		bool Pickup(int P, uint64_t Id, uint8_t Item)
		{
			FClient& C = Clients[P];
			const int Room = Capacity(C.Inv, Item);
			if (Room <= 0)
			{
				return false;
			}
			C.NextReq = static_cast<uint16_t>(C.NextReq % 0x7FFF + 1);
			C.PendingPickupReq = C.NextReq;
			C.PendingItem = Item;
			Host.RequestPickup(P, Id, C.NextReq, Host.LevelSerial, Item, Room, C.Epoch());
			return true;
		}

		// ABRCharacter::HandlePickupResult / ClientHealResult / ClientHitFeedback / ClientWakeAck
		void Deliver(int P, int32_t ClientLevelSerial)
		{
			FClient& C = Clients[P];
			while (!C.Inbox.empty())
			{
				const FMsg M = C.Inbox.front();
				C.Inbox.pop_front();
				if (M.Kind == FMsg::PickupResult)
				{
					const bool bRep = C.WasApplied(M.RequestId);
					if (!bRep) C.Remember(M.RequestId);
					const bool bMine = C.PendingPickupReq == M.RequestId;
					if (bMine) { C.PendingPickupReq = 0; C.PendingItem = 0; }
					FPickupIn In;
					In.bAccepted = M.Result == static_cast<uint8_t>(EPickup::Accepted);
					In.bRepeated = bRep;
					In.bWasPending = bMine;
					In.bStaleLevel = M.LevelSerial != ClientLevelSerial;
					In.RespEpoch = M.Epoch;
					In.CurEpoch = C.Epoch();
					const FPickupOut Out = DecidePickup(In);
					if (Out.bIgnore) continue;
					if (Out.bAck) Host.Ack(P, M.RequestId);
					if (Out.bLostWithLife) { ++C.Lost; continue; }
					if (Out.bStore)
					{
						const int Left = Add(C.Inv, M.Item, 1);
						if (Left > 0) { C.Rec.Put(M.Item, Left); ++C.Recovered; }
						++C.Stored;
						C.Effects += Out.bEffects ? 1 : 0;
					}
				}
				else if (M.Kind == FMsg::HealResult)
				{
					const bool bRep = C.WasApplied(static_cast<uint16_t>(M.RequestId | 0x8000));
					if (!bRep) C.Remember(static_cast<uint16_t>(M.RequestId | 0x8000));
					FHealIn In;
					In.bAccepted = M.Result == 0;
					In.bRepeated = bRep;
					In.bWasPending = C.PendingHealReq == M.RequestId;
					In.bStaleLevel = M.LevelSerial != ClientLevelSerial;
					In.RespEpoch = M.Epoch;
					In.CurEpoch = C.Epoch();
					In.RespRev = M.Rev;
					In.AckRev = C.AckRev;
					if (In.bWasPending) C.PendingHealReq = 0;
					const FHealOut Out = DecideHeal(In);
					if (Out.bIgnore || !In.bAccepted) continue;
					if (Out.bRemoveItem) Remove(C.Inv, M.Item, 1);
					if (Out.bSetHealth) { C.AckRev = M.Rev; C.Health = M.Health; }
					C.Effects += Out.bEffects ? 1 : 0;
				}
				else if (M.Kind == FMsg::Hit)
				{
					if (ShouldApplyHealth(M.Epoch, C.Epoch(), M.Rev, C.AckRev)) { C.AckRev = M.Rev; C.Health = M.Health; }
				}
				else
				{
					if (static_cast<int8_t>(static_cast<uint8_t>(M.Epoch - C.KnownWake)) >= 0) C.KnownWake = M.Epoch;
					C.PendingWake = C.PendingWake > 0 ? static_cast<uint8_t>(C.PendingWake - 1) : 0;
					if (IsNewer(M.Rev, C.AckRev)) C.AckRev = M.Rev;
				}
			}
		}

		// Reveil local du client (ResetStats) : inventaire de depart, sante 100, epoque en attente de confirmation
		void LocalWake(int P)
		{
			Clients[P].StartingGear();
			Clients[P].Health = 100.f;
			++Clients[P].PendingWake;
		}

		// Invariants : chaque objet attribue est dans le monde, dans un inventaire (rangé ou mis de côté), ou parti avec
		// une vie terminee ; le stock de l'hote suit l'inventaire du joueur (meme vie)
		bool StockMatches(int P) const
		{
			const FClient& C = Clients[P];
			return Host.Players[P].Stock[0] == Count(C.Inv, ItemAlmondWater) + C.Rec.Count(ItemAlmondWater)
				&& Host.Players[P].Stock[1] == Count(C.Inv, ItemBandage) + C.Rec.Count(ItemBandage);
		}
	};

	void Scenario(const char* Name, bool bOk)
	{
		std::printf("  %-74s %s\n", Name, bOk ? "OK" : "ECHEC");
		Check(bOk, Name);
	}

	void TestEndToEnd()
	{
		std::printf("E. Hote et joueurs (modele de bout en bout)\n");
		// E1 : deux, puis quatre joueurs sur le meme objet
		for (int N : { 2, 4 })
		{
			FSim S(N);
			S.Host.Pickups[100] = { ItemBandage, 0, 0, 0 };
			for (int P = 0; P < N; ++P) S.Pickup(P, 100, ItemBandage);
			int Got = 0;
			for (int P = 0; P < N; ++P)
			{
				S.Deliver(P, 1);
				Got += Count(S.Clients[P].Inv, ItemBandage) - 1;
			}
			bool bStock = true;
			for (int P = 0; P < N; ++P) bStock &= S.StockMatches(P);
			char Buf[96];
			std::snprintf(Buf, sizeof(Buf), "E1 %d joueurs sur le meme bandage : une attribution, objet retire, stocks", N);
			Scenario(Buf, Got == 1 && S.Host.Pickups.empty() && bStock);
		}
		// E2 : inventaire plein des la demande (aucune place : pas de demande, rien ne change)
		{
			FSim S(2);
			S.Clients[0].Inv = FullBags();
			S.Host.Players[0].Stock[0] = S.Host.Players[0].Stock[1] = 0;
			S.Host.Pickups[101] = { ItemBattery, 0, 0, 0 };
			const bool bSent = S.Pickup(0, 101, ItemBattery);
			Scenario("E2 inventaire plein des la demande : aucune demande, objet en place", !bSent && S.Host.Pickups.count(101) == 1);
		}
		// E3 : derniere case occupee pendant l'attente : le deplacement est refuse, l'objet rentre
		{
			FSim S(2);
			FClient& C = S.Clients[0];
			C.Inv = FullBags();
			C.Inv.Storage[NumStorage - 1].Clear();
			C.Inv.Equip[SlotBelt] = { ItemFlashlight, 1 };
			S.Host.Players[0].Stock[0] = S.Host.Players[0].Stock[1] = 0;
			S.Host.Pickups[102] = { ItemBandage, 0, 0, 0 };
			S.Pickup(0, 102, ItemBandage);
			FInv After = C.Inv;
			Move(After, EGroup::Equipment, SlotBelt, EGroup::Storage, NumStorage - 1);
			const bool bRefused = !KeepsRoom(After, C.PendingItem);
			if (!bRefused) C.Inv = After;
			S.Deliver(0, 1);
			Scenario("E3 derniere case prise pendant l'attente : refus, bandage range", bRefused && Count(C.Inv, ItemBandage) == 1 && C.Recovered == 0 && S.StockMatches(0));
		}
		// E3b : place prise quand meme (reserve expiree) : l'objet est mis de cote, puis range
		{
			FSim S(2);
			FClient& C = S.Clients[0];
			C.Inv = FullBags();
			C.Inv.Storage[NumStorage - 1].Clear();
			S.Host.Players[0].Stock[0] = S.Host.Players[0].Stock[1] = 0;
			S.Host.Pickups[103] = { ItemBandage, 0, 0, 0 };
			S.Pickup(0, 103, ItemBandage);
			C.Inv.Storage[NumStorage - 1] = { ItemVHSTape, 1 }; // la place disparait sans passer par la reserve
			S.Deliver(0, 1);
			const bool bAside = C.Rec.Count(ItemBandage) == 1 && S.StockMatches(0);
			C.Inv.Pockets[0].Clear();
			C.Rec.Stow(C.Inv);
			Scenario("E3b place perdue malgre tout : mis de cote, puis range (jamais perdu)", bAside && Count(C.Inv, ItemBandage) == 1 && C.Rec.NumItems() == 0);
		}
		// E4 : mort apres l'acceptation de l'hote, reponse arrivee apres le reveil
		{
			FSim S(2);
			S.Host.Pickups[104] = { ItemBandage, 0, 0, 0 };
			S.Pickup(0, 104, ItemBandage);
			// La reponse est en route ; le joueur meurt et se reveille (l'hote confirme apres)
			std::deque<FMsg> InFlight;
			InFlight.swap(S.Clients[0].Inbox);
			S.Host.Die(0);
			S.LocalWake(0);
			S.Host.Wake(0);
			std::deque<FMsg> WakeMsgs;
			WakeMsgs.swap(S.Clients[0].Inbox);
			S.Clients[0].Inbox = InFlight; // reponse du ramassage d'abord, confirmation du reveil ensuite
			for (const FMsg& M : WakeMsgs) S.Clients[0].Inbox.push_back(M);
			S.Deliver(0, 1);
			FLedgerEntry Out[FLedger::Size];
			const int Unacked = S.Host.Players[0].Ledger.Unacked(1, Out, FLedger::Size);
			Scenario("E4 mort apres acceptation : parti avec l'ancien inventaire, stocks egaux, accuse",
				S.Clients[0].Lost == 1 && Count(S.Clients[0].Inv, ItemBandage) == 1 && S.StockMatches(0) && Unacked == 0);
		}
		// E5 : reponse arrivee apres un changement de niveau
		{
			FSim S(2);
			S.Host.Pickups[105] = { ItemAlmondWater, 0, 0, 0 };
			S.Pickup(0, 105, ItemAlmondWater);
			S.Host.LevelSerial = 2;
			S.Deliver(0, 2);
			Scenario("E5 reponse apres changement de niveau : rangee, aucun effet rejoue, stock",
				Count(S.Clients[0].Inv, ItemAlmondWater) == 2 && S.Clients[0].Effects == 0 && S.StockMatches(0));
		}
		// E6 : reponse repetee (renvoi), demande renvoyee (deja traitee)
		{
			FSim S(2);
			S.Host.Pickups[106] = { ItemBandage, 0, 0, 0 };
			S.Pickup(0, 106, ItemBandage);
			S.Clients[0].Inbox.push_back(S.Clients[0].Inbox.front());
			S.Host.RequestPickup(0, 106, S.Clients[0].NextReq, 1, ItemBandage, 4, 0); // la meme demande, renvoyee
			S.Deliver(0, 1);
			Scenario("E6 reponse repetee et demande renvoyee : un seul bandage, stock", Count(S.Clients[0].Inv, ItemBandage) == 2 && S.StockMatches(0));
		}
		// E7 : deconnexion avant la reponse : l'objet revient dans le monde, la cassette est decomptee ; reprise par un autre
		{
			FSim S(3);
			S.Host.Pickups[107] = { ItemVHSTape, 120.f, 40.f, 0.f };
			S.Pickup(0, 107, ItemVHSTape);
			const int LoreAfterAccept = S.Host.Lore;
			S.Clients[0].bConnected = false;
			S.Clients[0].Inbox.clear();
			S.Host.Disconnect(0);
			const bool bBack = S.Host.Pickups.size() == 1 && S.Host.Pickups.begin()->second.Item == ItemVHSTape && S.Host.Lore == 0;
			const uint64_t NewId = S.Host.Pickups.begin()->first;
			S.Pickup(1, NewId, ItemVHSTape);
			S.Deliver(1, 1);
			Scenario("E7 deconnexion avant reponse : rendu au monde, decompte, repris par un autre",
				LoreAfterAccept == 1 && bBack && Count(S.Clients[1].Inv, ItemVHSTape) == 1 && S.Host.Lore == 1);
		}
		// E8 : soin pendant un changement de niveau : quantite une fois, sante officielle (aucun etat plus recent)
		{
			FSim S(2);
			S.Host.Players[0].Health = 40.f;
			S.Clients[0].Health = 40.f;
			S.Clients[0].PendingHealReq = 1;
			S.Host.RequestHeal(0, ItemBandage, 1, 1, 0);
			S.Host.LevelSerial = 2;
			S.Deliver(0, 2);
			Scenario("E8 soin et changement de niveau : bandage retire une fois, sante 75, sans effet",
				Count(S.Clients[0].Inv, ItemBandage) == 0 && S.Clients[0].Health == 75.f && S.Clients[0].Effects == 0 && S.StockMatches(0));
		}
		// E9 : soin accepte, mort et reveil avant la reponse : rien de la nouvelle vie n'est pris, sante 100
		{
			FSim S(2);
			S.Host.Players[0].Health = 40.f;
			S.Clients[0].Health = 40.f;
			S.Clients[0].PendingHealReq = 1;
			S.Host.RequestHeal(0, ItemBandage, 1, 1, 0);
			std::deque<FMsg> InFlight;
			InFlight.swap(S.Clients[0].Inbox);
			S.Host.Die(0);
			S.LocalWake(0);
			S.Host.Wake(0);
			std::deque<FMsg> W;
			W.swap(S.Clients[0].Inbox);
			S.Clients[0].Inbox = InFlight;
			for (const FMsg& M : W) S.Clients[0].Inbox.push_back(M);
			S.Deliver(0, 1);
			Scenario("E9 soin puis mort et reveil : bandage neuf garde, sante 100, stock",
				Count(S.Clients[0].Inv, ItemBandage) == 1 && S.Clients[0].Health == 100.f && S.StockMatches(0));
		}
		// E10 : soin et coup rapproches : la sante finale est celle de l'hote
		{
			FSim S(2);
			S.Host.Players[0].Health = 40.f;
			S.Clients[0].Health = 40.f;
			S.Clients[0].PendingHealReq = 1;
			S.Host.RequestHeal(0, ItemBandage, 1, 1, 0);
			S.Host.Hit(0, 20.f);
			S.Deliver(0, 1);
			const bool bInOrder = S.Clients[0].Health == S.Host.Players[0].Health;
			// Ordre inverse (robustesse) : le soin arrive apres le coup
			FSim T(2);
			T.Host.Players[0].Health = 40.f;
			T.Clients[0].Health = 40.f;
			T.Clients[0].PendingHealReq = 1;
			T.Host.RequestHeal(0, ItemBandage, 1, 1, 0);
			T.Host.Hit(0, 20.f);
			std::swap(T.Clients[0].Inbox[0], T.Clients[0].Inbox[1]);
			T.Deliver(0, 1);
			Scenario("E10 soin et coup rapproches (dans l'ordre, et inverses) : sante de l'hote",
				bInOrder && T.Clients[0].Health == T.Host.Players[0].Health && Count(T.Clients[0].Inv, ItemBandage) == 0);
		}
		// E11 : demande de soin renvoyee : stock decremente une seule fois
		{
			FSim S(2);
			S.Host.Players[0].Health = 40.f;
			S.Clients[0].PendingHealReq = 1;
			S.Host.RequestHeal(0, ItemBandage, 1, 1, 0);
			S.Host.RequestHeal(0, ItemBandage, 1, 1, 0);
			S.Deliver(0, 1);
			Scenario("E11 demande de soin renvoyee : un soin, un bandage", S.Host.Players[0].Stock[1] == 0 && Count(S.Clients[0].Inv, ItemBandage) == 0 && S.StockMatches(0));
		}
		// E12 : reveil local refuse ou deja vivant pour l'hote : l'epoque revient a celle de l'hote (pas de blocage)
		{
			FSim S(2);
			S.LocalWake(0);
			const uint8_t During = S.Clients[0].Epoch();
			FMsg Ack;
			Ack.Kind = FMsg::WakeAck;
			Ack.Epoch = S.Host.Players[0].Wake; // ClientWakeAck(deja vivant) ou refus
			S.Clients[0].Inbox.push_back(Ack);
			S.Deliver(0, 1);
			S.Host.Pickups[112] = { ItemBattery, 0, 0, 0 };
			S.Pickup(0, 112, ItemBattery);
			S.Deliver(0, 1);
			Scenario("E12 reveil sans confirmation (deja vivant) : epoque retablie, ramassage servi",
				During == 1 && S.Clients[0].Epoch() == 0 && Count(S.Clients[0].Inv, ItemBattery) == 2);
		}
		// E13 : sequences au hasard (4 joueurs, 40 objets, deplacements, morts, transitions, renvois) : invariants
		{
			std::mt19937 Rng(4122);
			int Runs = 0, Ok = 0;
			for (int Run = 0; Run < 600; ++Run)
			{
				FSim S(4);
				uint64_t Next = 1000;
				int Spawned = 0;
				for (int K = 0; K < 40; ++K)
				{
					const uint8_t It = static_cast<uint8_t>(std::uniform_int_distribution<int>(ItemAlmondWater, ItemVHSTape)(Rng));
					S.Host.Pickups[Next++] = { It, 0, 0, 0 };
					++Spawned;
				}
				for (int Step = 0; Step < 120; ++Step)
				{
					const int P = std::uniform_int_distribution<int>(0, 3)(Rng);
					const int A = std::uniform_int_distribution<int>(0, 9)(Rng);
					if (A <= 4 && !S.Host.Pickups.empty())
					{
						auto It = S.Host.Pickups.begin();
						std::advance(It, std::uniform_int_distribution<int>(0, static_cast<int>(S.Host.Pickups.size()) - 1)(Rng));
						if (S.Clients[P].PendingPickupReq == 0 && !S.Host.Players[P].bDead)
						{
							S.Pickup(P, It->first, It->second.Item);
						}
					}
					else if (A == 5)
					{
						int Refused = 0;
						RandomMove(Rng, S.Clients[P].Inv, S.Clients[P].PendingPickupReq != 0, S.Clients[P].PendingItem, Refused);
					}
					else if (A == 6 && std::uniform_int_distribution<int>(0, 5)(Rng) == 0 && !S.Host.Players[P].bDead)
					{
						S.Host.Die(P);
						S.LocalWake(P);
						S.Host.Wake(P);
					}
					else if (A == 7 && !S.Clients[P].Inbox.empty() && S.Clients[P].Inbox.front().Kind == FMsg::PickupResult)
					{
						// Renvoi d'une reponse de ramassage (l'hote renvoie une reponse de son historique) ; la confirmation
						// d'un reveil, elle, n'est jamais renvoyee (RPC fiable, une par signalement)
						S.Clients[P].Inbox.push_back(S.Clients[P].Inbox.front());
					}
					else
					{
						S.Deliver(P, S.Host.LevelSerial);
					}
				}
				for (int P = 0; P < 4; ++P) S.Deliver(P, S.Host.LevelSerial);
				// Conservation : objets du monde + ranges + mis de cote + partis avec une vie = objets poses
				int InHands = 0, LostLife = 0;
				bool bStocks = true;
				for (int P = 0; P < 4; ++P)
				{
					InHands += S.Clients[P].Stored;
					LostLife += S.Clients[P].Lost;
					bStocks &= S.StockMatches(P);
				}
				const int Total = static_cast<int>(S.Host.Pickups.size()) + InHands + LostLife;
				++Runs;
				Ok += (Total == Spawned && bStocks) ? 1 : 0;
			}
			char Buf[96];
			std::snprintf(Buf, sizeof(Buf), "E13 %d parties au hasard (4 joueurs) : aucun objet perdu ni double, stocks", Runs);
			Scenario(Buf, Ok == Runs);
		}
	}
}

int main()
{
	std::printf("# Banc des transactions v4.12 (BRTxnLogic.cpp)\n\n");
	TestInventoryRules();
	TestReservation();
	TestRecovery();
	TestDecisions();
	TestEndToEnd();
	std::printf("\nVerifications : %d, echecs : %d\nRESULTAT : %s\n", GChecks, GFailures, GFailures == 0 ? "OK" : "ECHEC");
	return GFailures == 0 ? 0 : 1;
}
