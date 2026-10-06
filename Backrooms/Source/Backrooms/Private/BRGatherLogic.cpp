// v4.12 : rassemblement d'un depart de groupe et attente au sommet d'une echelle (voir BRGatherLogic.h). C++ pur.
#include "BRGatherLogic.h"

namespace BRGather
{
	namespace GatherImpl
	{
		float Abs(float V) { return V < 0.f ? -V : V; }

		float Dist2DSq(const FVec& A, const FVec& B)
		{
			const float DX = A.X - B.X;
			const float DY = A.Y - B.Y;
			return DX * DX + DY * DY;
		}

		float Dist3DSq(const FVec& A, const FVec& B)
		{
			const float DZ = A.Z - B.Z;
			return Dist2DSq(A, B) + DZ * DZ;
		}

		FVec Ahead(const FExitShape& S, float Dist)
		{
			FVec P = S.Foot;
			P.X += S.Forward.X * Dist;
			P.Y += S.Forward.Y * Dist;
			return P;
		}

		/** Meme etage que le pied : centre du joueur a moins de Height de celui d'un joueur debout au pied */
		bool SameFloor(const FExitShape& S, const FVec& Pos)
		{
			return Abs(Pos.Z - (S.Foot.Z + CapsuleHalf)) < Height;
		}
	}

	FVec GatherPoint(const FExitShape& Shape)
	{
		switch (Shape.Style)
		{
		case EStyle::Ladder: return GatherImpl::Ahead(Shape, 100.f);
		case EStyle::Barn: return GatherImpl::Ahead(Shape, 520.f);
		default: return Shape.Foot;
		}
	}

	FVec ViewPoint(const FExitShape& Shape)
	{
		FVec P = GatherPoint(Shape);
		P.Z += ViewHeight;
		return P;
	}

	bool OnLadder(const FExitShape& Shape, const FVec& Pos)
	{
		return Shape.Style == EStyle::Ladder && GatherImpl::Dist2DSq(Pos, Shape.Anchor) < LadderColumn * LadderColumn && Pos.Z > Shape.Foot.Z + 40.f
			&& Pos.Z < Shape.TopZ + 120.f;
	}

	bool InFloorZone(const FExitShape& Shape, const FVec& Pos)
	{
		return GatherImpl::Dist2DSq(Pos, GatherPoint(Shape)) < Radius * Radius && GatherImpl::SameFloor(Shape, Pos);
	}

	EGather Classify(const FExitShape& Shape, const FVec& Pos)
	{
		if (OnLadder(Shape, Pos))
		{
			return EGather::Ladder;
		}
		return InFloorZone(Shape, Pos) ? EGather::FloorNeedsView : EGather::No;
	}

	EGather CanRequest(const FExitShape& Shape, const FVec& Pos)
	{
		switch (Shape.Style)
		{
		case EStyle::Ladder:
			if (OnLadder(Shape, Pos))
			{
				return EGather::Ladder;
			}
			return (GatherImpl::Dist2DSq(Pos, Shape.Anchor) < 260.f * 260.f && GatherImpl::SameFloor(Shape, Pos)) ? EGather::FloorNeedsView : EGather::No;
		case EStyle::Barn:
			return InFloorZone(Shape, Pos) ? EGather::FloorNeedsView : EGather::No;
		default:
			return GatherImpl::Dist3DSq(Pos, Shape.Foot) < DoorReach * DoorReach ? EGather::FloorNeedsView : EGather::No;
		}
	}

	FClimbWait::FOut FClimbWait::Update(float Dt, float Z, float TopZ, float UpInput, uint8_t Phase, uint16_t DepartureId, bool bForThisExit, bool bInitiator)
	{
		FOut Out;
		if (!bForThisExit)
		{
			Phase = 0;
			DepartureId = 0;
		}
		const bool bAtTop = Z >= TopZ - 1.f;
		switch (State)
		{
		case EState::Climbing:
			if (bAtTop)
			{
				// Une seule demande par arrivee au sommet
				Out.bSendRequest = true;
				State = EState::Waiting;
				SinceRequest = 0.f;
				SeenDeparture = 0;
				bCancelSent = false;
			}
			break;
		case EState::Waiting:
			SinceRequest += Dt;
			if (Phase == 1 && DepartureId != 0)
			{
				SeenDeparture = DepartureId;
			}
			if (Phase == 2 && DepartureId != 0 && (SeenDeparture == 0 || SeenDeparture == DepartureId))
			{
				State = EState::Leaving;
			}
			else if ((Phase == 3 && SeenDeparture != 0 && DepartureId == SeenDeparture) || (Phase != 1 && SeenDeparture == 0 && SinceRequest > RequestTimeout))
			{
				State = EState::Stopped; // annule, expire ou jamais vu : plus de demande tant qu'on ne redescend pas
			}
			else if (UpInput < 0.f && Phase == 1 && bInitiator && !bCancelSent)
			{
				// L'initiateur redescend pendant le rassemblement : il annule (une fois)
				Out.bSendCancel = true;
				bCancelSent = true;
			}
			break;
		default:
			break;
		}
		// Redescendu d'un metre : la prochaine arrivee au sommet redemandera
		if ((State == EState::Waiting || State == EState::Stopped) && Z < TopZ - RearmDrop)
		{
			State = EState::Climbing;
		}
		Out.bClampTop = State != EState::Leaving && bAtTop;
		return Out;
	}

	void FClimbWait::OnRefused()
	{
		if (State == EState::Waiting)
		{
			State = EState::Stopped;
		}
	}

	void FClimbWait::Reset()
	{
		State = EState::Climbing;
		SinceRequest = 0.f;
		SeenDeparture = 0;
		bCancelSent = false;
	}
}
