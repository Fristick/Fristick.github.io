// v4.15 : aides du test reseau (-BRNetTest) partagees par les etapes de plusieurs versions : etapes signalees par le client
// a l'hote (barrieres) et deplacement instantane d'un client que le serveur recoit vraiment.
//   - Barrieres : le client signale l'etape atteinte (ABRPlayerController::ServerCheat 4, accepte par l'hote seulement si
//     AreCheatsAllowed() chez lui : jamais en Shipping) ; l'hote lit la copie de son controleur (NetTestMark). L'hote ne
//     lance plus un changement de niveau pendant que le client finit l'etape precedente, et ne ferme plus la partie avant
//     que le client ait fini son plan.
//   - Deplacement d'un client : la position d'un client part au serveur avec ses mouvements. Un mouvement en attente, parti
//     de l'ancienne position, pouvait etre fusionne avec le suivant et ramener le personnage a son point de depart (la
//     demande "a 9 m" etait faite depuis l'indice). Les mouvements en attente sont envoyes avant le deplacement, et le
//     test verifie sur plusieurs images que le personnage est bien reste a sa nouvelle place.
#pragma once

#include "CoreMinimal.h"
#include "BRCharacter.h"
#include "BRPlayerController.h"
#include "BRWorld.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace BRTestNet
{
	/** Demande "a 9 m" terminee chez le client (v4.12) */
	constexpr int32 MarkV412Refused = 4122;
	/** Client arrive a la premiere etape v4.15, avant le changement de niveau lance par l'hote */
	constexpr int32 MarkV415Start = 4151;
	/** Client arrive au bout de son plan (rapport ecrit juste apres) */
	constexpr int32 MarkFinished = 9999;

	/** Client : signale a l'hote l'etape atteinte */
	inline void SendMark(ABRPlayerController* PC, int32 Mark)
	{
		if (PC)
		{
			PC->NetTestMark = Mark;
			PC->ServerCheat(4, Mark);
		}
	}

	/** Hote : personnage du premier client (nullptr s'il n'y en a pas) */
	inline ABRCharacter* RemoteMate(const ABRWorld* W)
	{
		TArray<ABRCharacter*> Players;
		if (W)
		{
			W->GetPlayers(Players);
		}
		for (ABRCharacter* P : Players)
		{
			if (P && !P->IsLocallyControlled())
			{
				return P;
			}
		}
		return nullptr;
	}

	/** Hote : derniere etape signalee par le client (0 : aucune, ou plus de client) */
	inline int32 RemoteMark(const ABRWorld* W)
	{
		const ABRCharacter* M = RemoteMate(W);
		const ABRPlayerController* PC = M ? Cast<ABRPlayerController>(M->GetController()) : nullptr;
		return PC ? PC->NetTestMark : 0;
	}

	/** Client : deplacement instantane recu tel quel par le serveur (mouvements en attente envoyes avant, vitesse nulle),
	 *  regard vers Face si donne */
	inline void TeleportForServer(ABRCharacter* C, const FVector& Stand, const FVector* Face = nullptr)
	{
		if (!C)
		{
			return;
		}
		UCharacterMovementComponent* M = C->GetCharacterMovement();
		if (M)
		{
			M->FlushServerMoves();
		}
		C->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
		if (M)
		{
			M->StopMovementImmediately();
		}
		if (Face)
		{
			if (AController* Ctrl = C->GetController())
			{
				Ctrl->SetControlRotation((*Face - C->GetEyeLocation()).Rotation());
			}
		}
	}

	/** Le personnage est a sa place (a Tol cm pres, a l'horizontale) */
	inline bool IsAt(const ABRCharacter* C, const FVector& Stand, float Tol = 50.f)
	{
		return C && FVector::Dist2D(C->GetActorLocation(), Stand) < Tol;
	}

	/** Suivi d'un deplacement instantane d'un client : replace le personnage s'il n'est plus a sa place, et dit quand il y
	 *  est reste Hold secondes de suite (le serveur a alors recu des mouvements partis de la nouvelle place) */
	struct FPlacement
	{
		// Objet global (une instance par fichier de test) : pas de dependance a l'initialisation d'un autre module
		FVector Stand = FVector(0.0, 0.0, 0.0);
		FVector Face = FVector(0.0, 0.0, 0.0);
		bool bFace = false;
		double StableSince = 0.0;
		int32 Reapplied = 0;

		void Start(ABRCharacter* C, const FVector& InStand, const FVector* InFace, double Now)
		{
			Stand = InStand;
			bFace = InFace != nullptr;
			Face = InFace ? *InFace : FVector::ZeroVector;
			Reapplied = 0;
			TeleportForServer(C, Stand, InFace);
			StableSince = Now;
		}

		/** true : en place depuis Hold secondes */
		bool Update(ABRCharacter* C, double Now, double Hold = 0.6)
		{
			if (!IsAt(C, Stand))
			{
				++Reapplied;
				TeleportForServer(C, Stand, bFace ? &Face : nullptr);
				StableSince = Now;
				return false;
			}
			return Now - StableSince >= Hold;
		}
	};
}
