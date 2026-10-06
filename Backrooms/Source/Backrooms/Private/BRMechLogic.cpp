// v4.12 : effets physiques des missions (voir BRMechLogic.h). C++ pur.
#include "BRMechLogic.h"

#include <cmath>

namespace BRMech
{
	namespace MechImpl
	{
		constexpr float Pi = 3.14159265358979f;
		float Rad(float Deg)
		{
			return Deg * Pi / 180.f;
		}
		float Clamp(float V, float Lo, float Hi)
		{
			return V < Lo ? Lo : (V > Hi ? Hi : V);
		}
		/** Lacet arrondi au quart de tour (les modules sont alignes sur la grille) */
		void Axes(float Yaw, float& FX, float& FY)
		{
			const float R = Rad(Yaw);
			FX = std::round(std::cos(R));
			FY = std::round(std::sin(R));
		}
	}

	void FFrame::ToWorld(float LX, float LY, float& OutX, float& OutY) const
	{
		float FX = 0.f, FY = 0.f;
		MechImpl::Axes(Yaw, FX, FY);
		// Y local = cote (-FY, FX)
		OutX = X + FX * LX - FY * LY;
		OutY = Y + FY * LX + FX * LY;
	}

	void FFrame::ToLocal(float WX, float WY, float& OutLX, float& OutLY) const
	{
		float FX = 0.f, FY = 0.f;
		MechImpl::Axes(Yaw, FX, FY);
		const float DX = WX - X;
		const float DY = WY - Y;
		OutLX = DX * FX + DY * FY;
		OutLY = -DX * FY + DY * FX;
	}

	FWaterBox MakeBox(const FFrame& F, float LX0, float LY0, float LX1, float LY1, float FloorZ, float RimZ, float Surface)
	{
		float AX = 0.f, AY = 0.f, BX = 0.f, BY = 0.f;
		F.ToWorld(LX0, LY0, AX, AY);
		F.ToWorld(LX1, LY1, BX, BY);
		FWaterBox B;
		B.MinX = AX < BX ? AX : BX;
		B.MaxX = AX < BX ? BX : AX;
		B.MinY = AY < BY ? AY : BY;
		B.MaxY = AY < BY ? BY : AY;
		B.FloorZ = FloorZ;
		B.RimZ = RimZ;
		B.Surface = Surface;
		return B;
	}

	FWaterQuery WaterAt(const FWaterBox* Boxes, int Num, float X, float Y, float Z, bool bLevelWater, float LevelSurface, float LevelFloor)
	{
		FWaterQuery Q;
		for (int I = 0; I < Num; ++I)
		{
			const FWaterBox& B = Boxes[I];
			if (X < B.MinX || X > B.MaxX || Y < B.MinY || Y > B.MaxY || Z < B.FloorZ - 60.f || Z > B.RimZ + 250.f)
			{
				continue;
			}
			Q.bLocal = true;
			Q.Box = I;
			Q.Floor = B.FloorZ;
			Q.bWater = B.Surface > B.FloorZ + 0.5f;
			Q.Surface = Q.bWater ? B.Surface : -1.0e6f;
			return Q;
		}
		Q.Floor = LevelFloor;
		Q.bWater = bLevelWater && LevelSurface > LevelFloor;
		Q.Surface = bLevelWater ? LevelSurface : -1.0e6f;
		return Q;
	}

	float Approach(float Cur, float Target, float Speed, float Dt)
	{
		const float Step = Speed * (Dt > 0.f ? Dt : 0.f);
		if (Cur < Target)
		{
			return Cur + Step >= Target ? Target : Cur + Step;
		}
		return Cur - Step <= Target ? Target : Cur - Step;
	}

	namespace Pool
	{
		float BottomA(float LevelWater)
		{
			return LevelWater + 55.f;
		}
		float BottomB(float LevelWater)
		{
			return LevelWater + 15.f;
		}
		float RimA(float LevelWater)
		{
			return BottomA(LevelWater) + MarkZero + MarksA * MarkStep + 22.f;
		}
		float RimB(float LevelWater)
		{
			return BottomB(LevelWater) + MarkZero + MarksB * MarkStep + 12.f;
		}
		float SurfaceA(int Mark, float LevelWater)
		{
			const int M = Mark < 0 ? 0 : (Mark > MarksA ? MarksA : Mark);
			return BottomA(LevelWater) + MarkZero + M * MarkStep;
		}
		float SurfaceB(int Mark, float LevelWater)
		{
			const int M = Mark < 0 ? 0 : (Mark > MarksB ? MarksB : Mark);
			return BottomB(LevelWater) + MarkZero + M * MarkStep;
		}
		int MarkOfA(float Surface, float LevelWater)
		{
			return static_cast<int>(std::lround((Surface - BottomA(LevelWater) - MarkZero) / MarkStep));
		}
		int MarkOfB(float Surface, float LevelWater)
		{
			return static_cast<int>(std::lround((Surface - BottomB(LevelWater) - MarkZero) / MarkStep));
		}
		float SlabTop(float LevelWater, float DeckHeight)
		{
			return (DeckHeight > LevelWater ? DeckHeight : LevelWater) + 2.f;
		}
		float EntryStepTop(float LevelWater, float DeckHeight)
		{
			return SlabTop(LevelWater, DeckHeight) * 0.5f;
		}
		float ChannelFull(float Slab)
		{
			return Slab + 100.f;
		}
		float ChannelDry(float Slab)
		{
			return Slab - 30.f;
		}
		float WeirHeight(float Surface, float Slab)
		{
			return MechImpl::Clamp(Surface - Slab + 12.f, 0.f, WeirMax);
		}
	}

	namespace Bridge
	{
		float HingeZ()
		{
			return FarTop - DeckThick * 0.5f;
		}
		float DeckLength()
		{
			const float DX = Gap + Bearing;
			const float DZ = (NearTop + DeckThick * 0.5f) - HingeZ();
			return std::sqrt(DX * DX + DZ * DZ);
		}
		float LoweredPitch()
		{
			const float DX = Gap + Bearing;
			const float DZ = (NearTop + DeckThick * 0.5f) - HingeZ();
			return std::atan2(DZ, DX) * 180.f / MechImpl::Pi;
		}
		float DeckPitch(float Progress)
		{
			const float P = MechImpl::Clamp(Progress, 0.f, 1.f);
			return 90.f + (LoweredPitch() - 90.f) * P;
		}
		void DeckTip(float Progress, float& OutX, float& OutZ)
		{
			const float R = MechImpl::Rad(DeckPitch(Progress));
			OutX = DeckLength() * std::cos(R);
			OutZ = HingeZ() + DeckLength() * std::sin(R);
		}
		float DeckTopAt(float X)
		{
			const float R = MechImpl::Rad(LoweredPitch());
			return HingeZ() + X * std::tan(R) + DeckThick * 0.5f / std::cos(R);
		}

		bool Detour(const FFrame& F, bool bDown, const FVec3& From, const FVec3& Goal, FVec3& OutWaypoint)
		{
			float FX = 0.f, FY = 0.f, GX = 0.f, GY = 0.f;
			F.ToLocal(From.X, From.Y, FX, FY);
			F.ToLocal(Goal.X, Goal.Y, GX, GY);
			const float FZ = From.Z - F.Z;
			const float GZ = Goal.Z - F.Z;
			const float Near1 = Gap + NearDepth + Steps * StepDepth;
			const auto Abs = [](float V) { return V < 0.f ? -V : V; };
			const auto Around = [&](float LX, float LY) { return LX > -FarDepth - 50.f && LX < Near1 + 400.f && Abs(LY) < HalfWidth + 400.f; };
			if (!Around(FX, FY) && !Around(GX, GY))
			{
				return false;
			}
			const auto OnLedge = [&](float LX, float LY, float LZ) { return LX >= -FarDepth && LX <= 5.f && Abs(LY) <= HalfWidth && LZ > FarTop - 10.f; };
			// Sur l'appui d'arrivee, ses marches ou le tablier (au-dessus du sol de la grotte)
			const auto OnUpper = [&](float LX, float LY, float LZ) { return LX > 0.f && LX <= Near1 && Abs(LY) <= HalfWidth && LZ > 55.f; };
			const auto Local = [&](float LX, float LY, float LZ)
			{
				FVec3 P;
				F.ToWorld(LX, LY, P.X, P.Y);
				P.Z = F.Z + LZ;
				return P;
			};
			const auto Dist2D = [](const FVec3& A, const FVec3& B) { return std::sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y)); };
			const bool bFromLedge = OnLedge(FX, FY, FZ);
			const bool bGoalLedge = OnLedge(GX, GY, GZ);
			const bool bFromUpper = OnUpper(FX, FY, FZ);
			const FVec3 StepsFoot = Local(Near1 + 45.f, 0.f, FZ);
			const FVec3 NearTopPt = Local(Gap + NearDepth * 0.5f, 0.f, NearTop + 60.f);
			const FVec3 LedgeMid = Local(-FarDepth * 0.5f, 0.f, FarTop + 60.f);
			if (bGoalLedge && !bFromLedge)
			{
				if (!bDown)
				{
					// Tablier leve : le palier est hors d'atteinte ; on attend au pied, dans l'interruption
					OutWaypoint = Local(40.f, MechImpl::Clamp(GY, -HalfWidth, HalfWidth), FZ);
					return true;
				}
				if (bFromUpper)
				{
					OutWaypoint = LedgeMid;
					return true;
				}
				OutWaypoint = Dist2D(From, StepsFoot) < 70.f ? NearTopPt : StepsFoot;
				return true;
			}
			if (bFromLedge && !bGoalLedge)
			{
				if (!bDown)
				{
					return false;
				}
				OutWaypoint = NearTopPt;
				return true;
			}
			if (bFromUpper && !OnUpper(GX, GY, GZ))
			{
				// Redescendre par les marches, pas par-dessus le bord de l'interruption
				OutWaypoint = StepsFoot;
				return Dist2D(From, StepsFoot) > 50.f;
			}
			return false;
		}
	}
}
