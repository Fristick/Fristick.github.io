// v4.12 : regles de l'inventaire et des transactions de l'equipe, sans dependance au moteur.
//
// Une seule implementation sert a verifier la place (demande de ramassage), a reserver cette place pendant l'attente
// de l'hote, et a ranger l'objet (ABRCharacter::AddItem) : avant la v4.12, deux algorithmes differents (RoomFor et
// AddItem) se contredisaient (faux "inventaire plein" pour une lampe, objet accepte puis perdu si l'inventaire changeait
// pendant l'attente). Les decisions sur les reponses de l'hote (soin, ramassage, coup) sont aussi ici, pour etre
// verifiees hors moteur par Tools/Transactions/test_transactions.cpp, avec les memes fonctions que le jeu.
//
// Regles :
//   - Un objet accepte par l'hote finit dans l'inventaire, ou dans la reserve "mis de cote" (rangee automatiquement des
//     qu'une place se libere, gardee dans la sauvegarde) ; jamais perdu parce que la confirmation arrive tard.
//   - Pendant une demande de ramassage, une place reste reservee : un deplacement qui la prendrait est refuse.
//   - Une reponse appartient a une "epoque" de l'inventaire : le nombre de reveils (equipement de depart) confirmes par
//     l'hote. Un objet accepte avant une mort suivie d'un reveil part avec l'inventaire de cette vie, comme le reste ;
//     un soin accepte avant le reveil ne consomme jamais un objet de la nouvelle vie.
//   - La sante officielle porte une revision commune aux coups, aux soins, aux morts, releves et reveils : seule une
//     revision plus recente que la derniere recue l'ecrit. Une reponse d'un ancien niveau ou d'une ancienne vie ne
//     rejoue aucun effet.
#pragma once

#include <cstdint>

namespace BRTxn
{
	constexpr int NumPockets = 4;
	constexpr int NumStorage = 20;
	constexpr int NumEquip = 4;
	/** Reserve "mis de cote" : objets acceptes sans place au moment de la reponse */
	constexpr int MaxRecovered = 8;

	/** Memes valeurs que EBRItem */
	enum EItemId : uint8_t
	{
		ItemNone,
		ItemAlmondWater,
		ItemBandage,
		ItemBattery,
		ItemEnergyBar,
		ItemVHSTape,
		ItemFlashlight,
		ItemCamcorder,
		ItemHeadlamp,
		ItemVest,
		ItemNote,
		ItemCount
	};

	/** Memes valeurs que EBREquipSlot */
	enum ESlotId : uint8_t
	{
		SlotHead,
		SlotChest,
		SlotHand,
		SlotBelt
	};

	/** Memes valeurs que EBRSlotGroup */
	enum class EGroup : uint8_t
	{
		Pockets,
		Storage,
		Equipment
	};

	/** Regles d'un objet : source unique (BRItems les reprend) */
	struct FRule
	{
		int32_t MaxStack = 1;
		/** Emplacements d'equipement acceptes (bit = ESlotId) */
		uint8_t EquipMask = 0;
		bool bConsumable = false;
	};

	const FRule& Rule(uint8_t Item);
	bool CanEquipIn(uint8_t Item, int Slot);
	/** Premier emplacement prefere d'un objet equipable (ordre des emplacements), -1 sinon */
	int PreferredSlot(uint8_t Item);

	struct FSlot
	{
		uint8_t Item = ItemNone;
		int32_t Count = 0;
		bool IsEmpty() const { return Item == ItemNone || Count <= 0; }
		void Clear()
		{
			Item = ItemNone;
			Count = 0;
		}
	};

	struct FInv
	{
		FSlot Pockets[NumPockets];
		FSlot Storage[NumStorage];
		FSlot Equip[NumEquip];

		FSlot* Get(EGroup Group, int Index);
		const FSlot* Get(EGroup Group, int Index) const;
	};

	/** Range Count exemplaires (piles existantes, equipement libre compatible, cases vides) ; retour : reste non range.
	 *  bOutEquipChanged : un emplacement d'equipement a ete rempli */
	int Add(FInv& Inv, uint8_t Item, int Count, bool* bOutEquipChanged = nullptr);
	/** Exemplaires que Add rangerait (meme algorithme, sur une copie), plafonne a 255 */
	int Capacity(const FInv& Inv, uint8_t Item);
	int Count(const FInv& Inv, uint8_t Item);
	/** Retire jusqu'a Count exemplaires (poches, sac, equipement) ; retour : nombre retire */
	int Remove(FInv& Inv, uint8_t Item, int Count);

	enum class EMove : uint8_t
	{
		/** Objets deplaces ou echanges */
		Moved,
		/** Ajoutes a une pile */
		Stacked,
		/** Rien a faire (meme case, case vide, indice invalide) */
		Invalid,
		/** L'objet ne va pas dans cet emplacement d'equipement */
		CannotEquip,
		/** L'objet de destination ne peut pas revenir dans l'emplacement d'origine */
		CannotSwapBack,
		/** Un seul exemplaire par emplacement, et il est occupe */
		EquipBusy
	};
	/** Deplacement d'une case a une autre (memes regles que le glisser-deposer de l'inventaire) */
	EMove Move(FInv& Inv, EGroup From, int FromIndex, EGroup To, int ToIndex, bool* bOutEquipChanged = nullptr);
	/** Desequipe vers la premiere case libre (poches puis sac) ; false : inventaire plein */
	bool Unequip(FInv& Inv, int Slot);
	/** Apres une operation : au moins Need exemplaires de Reserved trouvent encore leur place */
	bool KeepsRoom(const FInv& After, uint8_t Reserved, int Need = 1);

	/** Reserve "mis de cote" */
	struct FRecovery
	{
		FSlot Slots[MaxRecovered];

		int NumItems() const;
		/** Ajoute (empile si possible) ; retour : reste qui ne tient pas (reserve pleine) */
		int Put(uint8_t Item, int Count);
		/** Range dans l'inventaire tout ce qui y trouve place ; retour : exemplaires ranges */
		int Stow(FInv& Inv, bool* bOutEquipChanged = nullptr);
		int Count(uint8_t Item) const;
	};

	/** Numeros sur 16 bits qui bouclent : A est-il plus recent que B ? */
	inline bool IsNewer(uint16_t A, uint16_t B)
	{
		return static_cast<int16_t>(static_cast<uint16_t>(A - B)) > 0;
	}

	/** La sante d'une reponse de l'hote s'ecrit-elle ? (meme vie, revision plus recente) */
	bool ShouldApplyHealth(uint8_t RespEpoch, uint8_t CurEpoch, uint16_t RespRev, uint16_t AckRev);

	// ---------------------------------------------------------------------------------------------- Reponse de soin
	struct FHealIn
	{
		bool bAccepted = false;
		/** Reponse deja appliquee (renvoi de l'hote) */
		bool bRepeated = false;
		/** C'est la demande en attente de ce joueur */
		bool bWasPending = false;
		/** Demande faite dans un autre niveau que le niveau courant */
		bool bStaleLevel = false;
		uint8_t RespEpoch = 0;
		uint8_t CurEpoch = 0;
		uint16_t RespRev = 0;
		uint16_t AckRev = 0;
	};
	struct FHealOut
	{
		bool bIgnore = false;
		/** Retirer l'objet (une fois) */
		bool bRemoveItem = false;
		/** Ecrire la sante officielle et la revision */
		bool bSetHealth = false;
		/** Son et message du soin */
		bool bEffects = false;
		/** Effet propre a l'objet tenu par le joueur (sante mentale de l'eau d'amande) */
		bool bItemEffect = false;
		/** Message de refus */
		bool bNotifyRefusal = false;
	};
	FHealOut DecideHeal(const FHealIn& In);

	// ---------------------------------------------------------------------------------------- Reponse de ramassage
	struct FPickupIn
	{
		bool bAccepted = false;
		bool bRepeated = false;
		bool bWasPending = false;
		bool bStaleLevel = false;
		uint8_t RespEpoch = 0;
		uint8_t CurEpoch = 0;
	};
	struct FPickupOut
	{
		bool bIgnore = false;
		/** Ranger l'objet (inventaire, sinon reserve "mis de cote") */
		bool bStore = false;
		/** Accepte pour une vie terminee depuis (reveil) : parti avec cet inventaire */
		bool bLostWithLife = false;
		/** Son et message du ramassage */
		bool bEffects = false;
		bool bNotifyRefusal = false;
		/** Accuse de reception a envoyer a l'hote (objet attribue et traite) */
		bool bAck = false;
	};
	FPickupOut DecidePickup(const FPickupIn& In);

	// --------------------------------------------------------------------------------------- Ramassage, cote hote
	/** Memes valeurs que EBRPickupResult */
	enum class EPickup : uint8_t
	{
		Accepted,
		AlreadyTaken,
		TooFar,
		NotVisible,
		Dead,
		Loading,
		StaleLevel,
		Unknown,
		Full,
		Busy,
		/** v4.12 : demande faite avant un reveil confirme par l'hote (autre inventaire) */
		StaleLife
	};
	/** Ce que l'hote a mesure (distance, ligne de vue...) : la decision est la meme partout */
	struct FServerPickupIn
	{
		bool bLevelMatches = true;
		bool bLevelReady = true;
		bool bDead = false;
		bool bLoading = false;
		bool bCollected = false;
		bool bExists = true;
		bool bTypeMatches = true;
		bool bInReach = true;
		bool bVisible = true;
		int Room = 1;
		uint8_t Epoch = 0;
		uint8_t ServerEpoch = 0;
	};
	EPickup DecideServerPickup(const FServerPickupIn& In);

	/** Registre de l'hote : objets attribues, en attente de l'accuse de reception du joueur. Si le joueur se deconnecte
	 *  avant, l'objet revient dans le monde (et ses effets sont compenses) au lieu de disparaitre */
	struct FLedgerEntry
	{
		uint16_t RequestId = 0;
		uint64_t PickupId = 0;
		uint8_t Item = ItemNone;
		uint8_t Epoch = 0;
		int32_t LevelSerial = 0;
		float X = 0.f;
		float Y = 0.f;
		float Z = 0.f;
		bool bAwaitingAck = false;
	};
	class FLedger
	{
	public:
		static constexpr int Size = 32;
		FLedgerEntry Entries[Size];
		int Next = 0;

		void Record(const FLedgerEntry& Entry);
		/** Accuse de reception : true si une attribution attendait */
		bool Ack(uint16_t RequestId);
		/** Attributions sans accuse dans ce niveau ; retour : nombre ecrit dans Out */
		int Unacked(int32_t LevelSerial, FLedgerEntry* Out, int Max) const;
	};
}
