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

	namespace Clearance
	{
		namespace
		{
			FVec3 V3(float X, float Y, float Z)
			{
				FVec3 V;
				V.X = X;
				V.Y = Y;
				V.Z = Z;
				return V;
			}
			FBox Box(float X0, float Y0, float Z0, float X1, float Y1, float Z1)
			{
				FBox B;
				B.X0 = X0;
				B.Y0 = Y0;
				B.Z0 = Z0;
				B.X1 = X1;
				B.Y1 = Y1;
				B.Z1 = Z1;
				return B;
			}
			float AbsF(float V)
			{
				return V < 0.f ? -V : V;
			}
			float MaxF(float A, float B)
			{
				return A > B ? A : B;
			}
			float MinF(float A, float B)
			{
				return A < B ? A : B;
			}
			/** Lacet arrondi au quart de tour, comme FFrame */
			float QuarterYaw(float Yaw)
			{
				return std::round(Yaw / 90.f) * 90.f;
			}
			/** Distance (plan) d'un point local a un rectangle local */
			float DistToRect(float LX, float LY, const FBox& B)
			{
				const float DX = MaxF(MaxF(B.X0 - LX, 0.f), LX - B.X1);
				const float DY = MaxF(MaxF(B.Y0 - LY, 0.f), LY - B.Y1);
				return std::sqrt(DX * DX + DY * DY);
			}
			/** v4.15 (correctifs 3) : distance d'un point a une boite alignee de centre C et de demi-tailles E (meme repere) */
			float DistToBox(const FVec3& P, const FVec3& C, const FVec3& E)
			{
				const float DX = MaxF(AbsF(P.X - C.X) - E.X, 0.f);
				const float DY = MaxF(AbsF(P.Y - C.Y) - E.Y, 0.f);
				const float DZ = MaxF(AbsF(P.Z - C.Z) - E.Z, 0.f);
				return std::sqrt(DX * DX + DY * DY + DZ * DZ);
			}
			/** v4.15 (correctifs 3) : distance d'un segment A-B a une boite alignee. Fonction convexe du parametre (distance a un
			 *  convexe le long d'une droite) : recherche ternaire a nombre de pas fixe, memes operations chez tous */
			float SegToBox(const FVec3& A, const FVec3& B, const FVec3& C, const FVec3& E)
			{
				auto At = [&](float T)
				{
					FVec3 P;
					P.X = A.X + (B.X - A.X) * T;
					P.Y = A.Y + (B.Y - A.Y) * T;
					P.Z = A.Z + (B.Z - A.Z) * T;
					return DistToBox(P, C, E);
				};
				float Lo = 0.f, Hi = 1.f;
				for (int I = 0; I < 48; ++I)
				{
					const float M1 = Lo + (Hi - Lo) / 3.f;
					const float M2 = Hi - (Hi - Lo) / 3.f;
					if (At(M1) <= At(M2))
					{
						Hi = M2;
					}
					else
					{
						Lo = M1;
					}
				}
				return MinF(MinF(At(0.f), At(1.f)), At((Lo + Hi) * 0.5f));
			}
			/** Me gene par le volume de Other : point vise dedans, ou place debout devant lui coupee */
			bool Hinders(const FPlaced& Me, const FPlaced& Other)
			{
				if (!Other.Print.bSolid)
				{
					return false;
				}
				const FVec3 P = AimPoint(Me);
				if (InSolid(Other, P.X, P.Y, P.Z, Margin))
				{
					return true;
				}
				const FBox& S = Other.Print.Solid;
				if (S.Z1 - S.Z0 <= StepHeight)
				{
					return false;
				}
				float SX = 0.f, SY = 0.f, LX = 0.f, LY = 0.f;
				Me.Frame.ToWorld(Me.Print.Aim.X + StandGap, Me.Print.Aim.Y, SX, SY);
				Other.Frame.ToLocal(SX, SY, LX, LY);
				return DistToRect(LX, LY, S) < PlayerRadius;
			}
		}

		FFootprint Of(BRMission::EKind Kind, uint8_t Role, uint8_t ModuleId, float WallThickness)
		{
			// Memes boites que ABRMissionDevice::Init (aspect de chaque role), en cm, repere du mecanisme
			using namespace BRMission;
			constexpr float Z = Device::WorkZ;
			FFootprint F;
			F.Aim = V3(8.f, 0.f, Z);
			F.AimExtent = V3(10.f, 22.f, 25.f);
			switch (Kind)
			{
			case EKind::Clue:
			{
				const bool bNotebook = Role == R_ReserveNote || Role == R_DiveLog || Role == R_Register || Role == R_Archive || Role == R_TeamLog;
				if (Role == R_PassageMarks)
				{
					break; // entailles dans la roche : boite par defaut
				}
				if (bNotebook)
				{
					F.Aim = V3(14.f, 0.f, Z - 4.f);
					F.AimExtent = V3(14.f, 18.f, 8.f);
				}
				else
				{
					F.Aim = V3(5.f, 0.f, Z + 20.f);
					F.AimExtent = V3(6.f, 28.f, 28.f);
				}
				break;
			}
			case EKind::Observe:
				switch (Role)
				{
				case R_Anomaly:
					F.Aim = V3(20.f, 0.f, 170.f);
					F.AimExtent = V3(25.f, 40.f, 70.f);
					break;
				case R_Gauge:
					F.Aim = V3(10.f, 0.f, Z);
					F.AimExtent = V3(8.f, 18.f, 18.f);
					break;
				case R_JunctionBox:
					F.Aim = V3(12.f, 0.f, Z + 10.f);
					F.AimExtent = V3(12.f, 24.f, 32.f);
					break;
				case R_FenceMark:
					F.Aim = V3(4.f, 0.f, 90.f);
					F.AimExtent = V3(8.f, 60.f, 30.f);
					break;
				case R_LidarTarget:
					F.Aim = V3(5.f, 0.f, Z + 20.f);
					F.AimExtent = V3(6.f, 34.f, 34.f);
					break;
				case R_LidarCage:
					F.Aim = V3(LidarCage::Gap + LidarCage::Depth + 4.f, 0.f, 120.f);
					F.AimExtent = V3(8.f, LidarCage::HalfWidth, 90.f);
					F.bSolid = true;
					F.Solid = Box(LidarCage::Gap, -LidarCage::HalfWidth, 0.f, LidarCage::Gap + LidarCage::Depth, LidarCage::HalfWidth, LidarCage::Height);
					break;
				default: // fleche du courant (Poolrooms)
					F.Aim = V3(30.f, 0.f, 40.f);
					F.AimExtent = V3(30.f, 30.f, 60.f);
					break;
				}
				break;
			case EKind::Switch:
			{
				const bool bToggle = Role == R_Breaker || Role == R_Relay || Role == R_RouteLever;
				const bool bWheel = Role == R_Valve || Role == R_Sluice || Role == R_BoilerDial;
				if (bToggle)
				{
					break; // boite par defaut
				}
				if (bWheel)
				{
					const float R = Role == R_Sluice ? 26.f : 18.f;
					F.AimExtent = V3(12.f, R + 6.f, R + 6.f);
					if (ModuleId == Module::PoolTanks)
					{
						// Vanne A des Poolrooms : les deux bassins derriere elle
						F.bSolid = true;
						F.Solid = Box(-Pool::TankDepth, Pool::AY0 - Pool::Wall, 0.f, 0.f, Pool::BY1 + Pool::Wall, 300.f);
					}
					break;
				}
				const float R = Role == R_MillDial ? 24.f : 12.f;
				F.Aim = V3(6.f, 0.f, Z + 20.f);
				F.AimExtent = V3(8.f, R + 12.f, R + 14.f);
				break;
			}
			case EKind::Socket:
				if (Role == R_Lock)
				{
					F.AimExtent = V3(10.f, 16.f, 16.f);
				}
				else
				{
					F.Aim = V3(8.f, 0.f, Z + 30.f);
					F.AimExtent = V3(8.f, 10.f, 14.f);
				}
				break;
			case EKind::Item:
				if (Role == R_Key)
				{
					F.Aim = V3(5.f, 0.f, Z + 20.f);
					F.AimExtent = V3(6.f, 8.f, 12.f);
				}
				else
				{
					F.Aim = V3(18.f, 0.f, 64.f);
					F.AimExtent = V3(14.f, 14.f, 10.f);
				}
				break;
			case EKind::Crank:
				if (Role == R_Beacon || Role == R_LidarPost)
				{
					F.Aim = V3(18.f, 0.f, 110.f);
					F.AimExtent = V3(14.f, 16.f, 60.f);
				}
				else
				{
					F.Aim = V3(26.f, 0.f, 60.f);
					F.AimExtent = V3(18.f, 30.f, 34.f);
				}
				break;
			case EKind::Button:
				if (Role == R_LidarDoor)
				{
					if (ModuleId == Module::LidarDoor)
					{
						F.Aim = V3(LidarDoor::LeafThick + 6.f, 0.f, 120.f);
						F.AimExtent = V3(8.f, LidarDoor::Width * 0.5f, 100.f);
						// Battant ferme dans l'ouverture (son propre point vise est dans son volume : l'hote ignore le
						// mecanisme vise)
						F.bSolid = true;
						F.Solid = Box(-(WallThickness * 0.5f + 2.f), -LidarDoor::Width * 0.5f, 0.f, WallThickness * 0.5f + 2.f, LidarDoor::Width * 0.5f, LidarDoor::Height);
					}
					else
					{
						F.Aim = V3(6.f, 0.f, Z + 20.f);
						F.AimExtent = V3(8.f, 30.f, 30.f);
					}
				}
				else if (Role == R_HouseMarker)
				{
					F.Aim = V3(5.f, 0.f, Z + 24.f);
					F.AimExtent = V3(6.f, 32.f, 26.f);
				}
				else
				{
					F.Aim = V3(6.f, 0.f, Z + 20.f);
					F.AimExtent = V3(8.f, 16.f, 18.f);
				}
				break;
			case EKind::Gate:
			{
				// Rien a viser ; les portes et passages fermes devant une sortie bloquent le passage. v4.15 (correctifs 3) : leur
				// boite visee n'a pas de collision (ABRMissionDevice::SetShown) : elle ne prend pas la visee d'un voisin
				F.AimExtent = V3(1.f, 1.f, 1.f);
				F.bAimable = false;
				if (ModuleId == Module::Bridge)
				{
					F.bSolid = true;
					F.Solid = Box(-Bridge::FarDepth, -Bridge::HalfWidth, 0.f, Bridge::Gap + Bridge::NearDepth + Bridge::Steps * Bridge::StepDepth, Bridge::HalfWidth,
						Bridge::FarTop + 230.f);
					break;
				}
				if (ModuleId == Module::PoolLock)
				{
					F.bSolid = true;
					F.Solid = Box(-Pool::ChannelLength, -(Pool::ChannelHalfWidth + Pool::ChannelWall), 0.f, Pool::EntryStepDepth, Pool::ChannelHalfWidth + Pool::ChannelWall, 300.f);
					break;
				}
				if (Role == R_LidarStatic || Role == R_ReserveLights || Role == R_Passage || Role == R_LightsExit)
				{
					break;
				}
				const bool bBig = Role == R_BarnDoor;
				const float W = bBig ? 360.f : 170.f;
				const float H = bBig ? 320.f : 250.f;
				const float X = Role == R_Bridge ? 30.f : 0.f;
				F.bSolid = true;
				F.Solid = Box(X - 15.f, -W * 0.5f, 0.f, X + 15.f, W * 0.5f, H);
				break;
			}
			}
			return F;
		}

		FVec3 AimPoint(const FPlaced& P)
		{
			FVec3 Out;
			P.Frame.ToWorld(P.Print.Aim.X, P.Print.Aim.Y, Out.X, Out.Y);
			Out.Z = P.Frame.Z + P.Print.Aim.Z;
			return Out;
		}

		bool InSolid(const FPlaced& P, float X, float Y, float Z, float Grow)
		{
			if (!P.Print.bSolid)
			{
				return false;
			}
			float LX = 0.f, LY = 0.f;
			P.Frame.ToLocal(X, Y, LX, LY);
			const float LZ = Z - P.Frame.Z;
			const FBox& S = P.Print.Solid;
			return LX >= S.X0 - Grow && LX <= S.X1 + Grow && LY >= S.Y0 - Grow && LY <= S.Y1 + Grow && LZ >= S.Z0 - Grow && LZ <= S.Z1 + Grow;
		}

		bool Clash(const FPlaced& A, const FPlaced& B)
		{
			if (Hinders(A, B) || Hinders(B, A))
			{
				return true;
			}
			if (!A.Print.bSolid || !B.Print.bSolid)
			{
				return false;
			}
			const float AZ0 = A.Frame.Z + A.Print.Solid.Z0, AZ1 = A.Frame.Z + A.Print.Solid.Z1;
			const float BZ0 = B.Frame.Z + B.Print.Solid.Z0, BZ1 = B.Frame.Z + B.Print.Solid.Z1;
			return AZ0 < BZ1 && BZ0 < AZ1 && Overlap(RectOf(A.Frame, A.Print.Solid), RectOf(B.Frame, B.Print.Solid));
		}

		bool CutsFocusLane(const FPlaced& Me, const FPlaced& Other)
		{
			if (!Me.Print.bAimable)
			{
				return false; // rien a viser (porte ou passage de la mission)
			}
			// Couloir de Me, exprime dans le repere de Other (lacets au quart de tour, comme RectOf) : du point vise vers les
			// yeux d'un joueur debout a la place ordinaire, et vers ce meme point a hauteur du point vise
			const FVec3 P = AimPoint(Me);
			float SX = 0.f, SY = 0.f;
			Me.Frame.ToWorld(Me.Print.Aim.X + StandRange, Me.Print.Aim.Y, SX, SY);
			FVec3 A, Eye, Level;
			Other.Frame.ToLocal(P.X, P.Y, A.X, A.Y);
			A.Z = P.Z - Other.Frame.Z;
			Other.Frame.ToLocal(SX, SY, Eye.X, Eye.Y);
			Eye.Z = Me.Frame.Z + StandEye - Other.Frame.Z;
			Level = Eye;
			Level.Z = A.Z;
			const float Limit = FocusSweep + Margin;
			auto Cut = [&](const FVec3& C, const FVec3& E) { return SegToBox(A, Eye, C, E) < Limit || SegToBox(A, Level, C, E) < Limit; };
			if (Other.Print.bAimable && Cut(Other.Print.Aim, Other.Print.AimExtent))
			{
				return true;
			}
			if (Other.Print.bSolid)
			{
				// Le volume d'un voisin (cage, porte fermee, module) coupe la vue de l'hote et la visee du client
				const FBox& S = Other.Print.Solid;
				return Cut(V3((S.X0 + S.X1) * 0.5f, (S.Y0 + S.Y1) * 0.5f, (S.Z0 + S.Z1) * 0.5f), V3(AbsF(S.X1 - S.X0) * 0.5f, AbsF(S.Y1 - S.Y0) * 0.5f, AbsF(S.Z1 - S.Z0) * 0.5f));
			}
			return false;
		}

		bool FocusClash(const FPlaced& A, const FPlaced& B)
		{
			return CutsFocusLane(A, B) || CutsFocusLane(B, A);
		}

		int BridgeParts(FBox* Out, int MaxParts)
		{
			// Memes boites que ABRMissionDevice::BuildBridge (AddSolid) ; tablier leve : vertical sur sa charniere (tangage 90 degres,
			// BRMech::Bridge::DeckPitch(0))
			namespace BB = BRMech::Bridge;
			static_assert(2 + BB::Steps + 4 + 1 <= MaxBridgeParts, "BRMech::Clearance::BridgeParts : MaxBridgeParts trop petit");
			FBox Parts[MaxBridgeParts];
			int N = 0;
			Parts[N++] = Box(-BB::FarDepth, -BB::HalfWidth, 0.f, 0.f, BB::HalfWidth, BB::FarTop);
			Parts[N++] = Box(BB::Gap, -BB::HalfWidth, 0.f, BB::Gap + BB::NearDepth, BB::HalfWidth, BB::NearTop);
			for (int I = 0; I < BB::Steps; ++I)
			{
				const float SX = BB::Gap + BB::NearDepth + static_cast<float>(I) * BB::StepDepth;
				const float Top = BB::NearTop * static_cast<float>(BB::Steps - I) / (static_cast<float>(BB::Steps) + 1.f);
				Parts[N++] = Box(SX, -BB::HalfWidth, 0.f, SX + BB::StepDepth, BB::HalfWidth, Top);
			}
			for (int Side = -1; Side <= 1; Side += 2)
			{
				// Poteau du portique sur le palier, poteau de main courante sur l'appui
				const float PY = static_cast<float>(Side) * (BB::DeckHalfWidth + 15.f);
				Parts[N++] = Box(-35.f, PY - 5.f, BB::FarTop, -25.f, PY + 5.f, BB::FarTop + 230.f);
				const float RY = static_cast<float>(Side) * (BB::DeckHalfWidth + 6.f);
				Parts[N++] = Box(BB::Gap + 22.f, RY - 3.f, BB::NearTop, BB::Gap + 28.f, RY + 3.f, BB::NearTop + 100.f);
			}
			Parts[N++] = Box(-BB::DeckThick * 0.5f, -BB::DeckHalfWidth, BB::HingeZ(), BB::DeckThick * 0.5f, BB::DeckHalfWidth, BB::HingeZ() + BB::DeckLength());
			const int Count = N < MaxParts ? N : MaxParts;
			for (int I = 0; I < Count && Out; ++I)
			{
				Out[I] = Parts[I];
			}
			return Count;
		}

		ELaneCut LaneCut(const FPlaced& Me, uint8_t MeModule, const FPlaced& Other, uint8_t OtherModule)
		{
			if (!CutsFocusLane(Me, Other))
			{
				return ELaneCut::Clear;
			}
			// v4.15 (correctifs 3) : la vanne B et les bassins de la vanne A sont poses ensemble par leur module (apres PickSlot)
			if (MeModule == Module::PoolWheelB && OtherModule == Module::PoolTanks)
			{
				return ELaneCut::PoolModule;
			}
			// v4.15 (correctifs 4) : porte de la mission (boite visee sans collision : elle ne prend jamais la visee) : seul son
			// volume coupe le couloir. Posee avant les groupes devant la sortie gardee, PickSlot ne la voit pas
			if (!Other.Print.bAimable && Other.Print.bSolid)
			{
				if (OtherModule == Module::None)
				{
					// Dalle d'une porte, d'une grille ou d'un portail : la place ordinaire est dedans (capsule comprise)
					float SX = 0.f, SY = 0.f;
					Me.Frame.ToWorld(Me.Print.Aim.X + StandRange, Me.Print.Aim.Y, SX, SY);
					if (InSolid(Other, SX, SY, Me.Frame.Z, PlayerRadius))
					{
						return ELaneCut::StandInGate;
					}
				}
				else if (OtherModule == Module::Bridge)
				{
					// Enveloppe de la passerelle : le couloir est juge contre ses vraies pieces
					FBox Parts[MaxBridgeParts];
					const int N = BridgeParts(Parts, MaxBridgeParts);
					FPlaced Part = Other;
					for (int I = 0; I < N; ++I)
					{
						Part.Print.Solid = Parts[I];
						if (CutsFocusLane(Me, Part))
						{
							return ELaneCut::Logged;
						}
					}
					return ELaneCut::BridgeEnvelope;
				}
			}
			return ELaneCut::Logged;
		}

		bool LaneCutLogged(const FPlaced& Me, uint8_t MeModule, const FPlaced& Other, uint8_t OtherModule)
		{
			return LaneCut(Me, MeModule, Other, OtherModule) == ELaneCut::Logged;
		}

		bool Overlap(const FRect& A, const FRect& B)
		{
			const float RA = MechImpl::Rad(A.Yaw), RB = MechImpl::Rad(B.Yaw);
			const float Ax[2][2] = { { std::cos(RA), std::sin(RA) }, { -std::sin(RA), std::cos(RA) } };
			const float Bx[2][2] = { { std::cos(RB), std::sin(RB) }, { -std::sin(RB), std::cos(RB) } };
			const float DX = B.CX - A.CX, DY = B.CY - A.CY;
			for (int I = 0; I < 4; ++I)
			{
				const float* N = I < 2 ? Ax[I] : Bx[I - 2];
				const float D = AbsF(DX * N[0] + DY * N[1]);
				const float PA = A.HX * AbsF(Ax[0][0] * N[0] + Ax[0][1] * N[1]) + A.HY * AbsF(Ax[1][0] * N[0] + Ax[1][1] * N[1]);
				const float PB = B.HX * AbsF(Bx[0][0] * N[0] + Bx[0][1] * N[1]) + B.HY * AbsF(Bx[1][0] * N[0] + Bx[1][1] * N[1]);
				if (D > PA + PB)
				{
					return false;
				}
			}
			return true;
		}

		FRect RectOf(const FFrame& F, const FBox& B)
		{
			FRect R;
			F.ToWorld((B.X0 + B.X1) * 0.5f, (B.Y0 + B.Y1) * 0.5f, R.CX, R.CY);
			R.HX = AbsF(B.X1 - B.X0) * 0.5f;
			R.HY = AbsF(B.Y1 - B.Y0) * 0.5f;
			R.Yaw = QuarterYaw(F.Yaw);
			return R;
		}

		FRect KeepOut(const FPlaced& P)
		{
			const FFootprint& F = P.Print;
			float X0 = -12.f;
			// Jusqu'a la capsule d'un joueur debout a StandDepth devant le point vise (et au-dela du mecanisme)
			float X1 = F.Aim.X + (F.AimExtent.X > StandGap ? F.AimExtent.X : StandGap) + StandDepth;
			float HY = AbsF(F.Aim.Y) + F.AimExtent.Y + StandGap;
			if (F.bSolid)
			{
				X0 = MinF(X0, F.Solid.X0 - StandGap);
				X1 = MaxF(X1, F.Solid.X1 + StandGap);
				HY = MaxF(HY, MaxF(AbsF(F.Solid.Y0), AbsF(F.Solid.Y1)) + StandGap);
			}
			return RectOf(P.Frame, Box(X0, -HY, 0.f, X1, HY, 0.f));
		}

		FRect ExitKeepOut(const FFrame& F)
		{
			const float HY = LidarDoor::Width * 0.5f + StandGap;
			return RectOf(F, Box(-12.f, -HY, 0.f, StandDepth + 40.f, HY, 0.f));
		}

		int PickSlot(const FFootprint& Print, const FSlot* Slots, int NumSlots, const FPlaced* Placed, int NumPlaced, float FaceHalf, bool bFocusLanes)
		{
			const float SolidHalf = Print.bSolid ? MaxF(AbsF(Print.Solid.Y0), AbsF(Print.Solid.Y1)) : 0.f;
			// v4.15 (correctifs 3) : premier passage avec les couloirs de visee, puis, s'il ne trouve rien, le choix d'avant
			bool bLanes = bFocusLanes;
			auto Usable = [&](int I)
			{
				const FSlot& S = Slots[I];
				if (!S.bOpen)
				{
					return false;
				}
				// Un volume mural tient dans sa face (il n'entre pas dans le mur voisin)
				if (Print.bSolid && S.bWall && AbsF(S.Lateral) + SolidHalf > FaceHalf + 0.01f)
				{
					return false;
				}
				FPlaced Me;
				Me.Frame = S.Frame;
				Me.Print = Print;
				for (int J = 0; J < NumPlaced; ++J)
				{
					if (Clash(Me, Placed[J]) || (bLanes && FocusClash(Me, Placed[J])))
					{
						return false;
					}
				}
				return true;
			};
			// Point de la face devant le centre de la cellule (decalage lateral retire) : meme point, meme face
			auto FaceCenter = [](const FSlot& S, float& OutX, float& OutY)
			{
				S.Frame.ToWorld(0.f, -S.Lateral, OutX, OutY);
			};
			auto Pick = [&]() -> int
			{
				for (int I = 0; I < NumSlots; ++I)
				{
					if (!Usable(I))
					{
						continue;
					}
					if (!Print.bSolid || !Slots[I].bWall)
					{
						return I;
					}
					// v4.15 (correctifs) : un volume mural se pose au bout de sa face ou il tient (le plus grand decalage lateral,
					// le premier dans l'ordre a egalite) : le reste de la face est d'un seul tenant. Au Level 6, la cage a +-58 et
					// son poste a -+58 (diag_v413_4) : sa place ordinaire, 110 cm devant lui, est a 87 cm du mur voisin. Avant :
					// cage au centre, poste a +-116, a 29 cm du mur voisin : place ordinaire coupee par ce mur, place de biais qui
					// frolait la cage (vue de l'hote ou visee du client prises par elle)
					float CX = 0.f, CY = 0.f;
					FaceCenter(Slots[I], CX, CY);
					const float Turn = QuarterYaw(Slots[I].Frame.Yaw);
					int Best = I;
					for (int J = I + 1; J < NumSlots; ++J)
					{
						const FSlot& T = Slots[J];
						if (!T.bWall || QuarterYaw(T.Frame.Yaw) != Turn || AbsF(T.Lateral) <= AbsF(Slots[Best].Lateral) + 0.01f)
						{
							continue;
						}
						float TX = 0.f, TY = 0.f;
						FaceCenter(T, TX, TY);
						if (AbsF(TX - CX) < 1.f && AbsF(TY - CY) < 1.f && Usable(J))
						{
							Best = J;
						}
					}
					return Best;
				}
				return -1;
			};
			const int First = Pick();
			if (First >= 0 || !bLanes)
			{
				return First;
			}
			// Aucun emplacement au couloir degage : le choix sans les couloirs (le placement n'echoue pas plus qu'avant ;
			// l'invariant du placement le journalise)
			bLanes = false;
			return Pick();
		}
	}
}
