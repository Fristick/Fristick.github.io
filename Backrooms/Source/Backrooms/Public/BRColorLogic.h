// v4.15 (realisme) : couleur des lampes vue par la camera : temperature de couleur (lieu de Planck) et balance des blancs
// du moteur. C++ pur : le banc Tools/Light/test_color.cpp verifie le modele et les reglages de BRLevels.cpp sans le moteur.
//
// Copie fidele des formules d'Unreal 5.8 :
//   - FColorSpace::MakeFromColorTemperature (Core, ColorSpace.cpp) : couleur d'une lampe de T kelvins (lieu de Planck,
//     approximation de Krystek), luminance 1, primaires BT.709 (espace de travail sRGB du projet) ;
//   - WhiteBalance (Shaders/Private/TonemapCommon.ush, mode "temperature") : le blanc de reference est pris sur le lieu de
//     Planck SOUS 4000 K et sur le lieu du jour (illuminant D) A PARTIR DE 4000 K, decale le long de l'isotherme par la
//     teinte, puis adapte vers D65 (Bradford). Une lampe (Planck) vue avec une balance >= 4000 K vire donc au rose ou a
//     la peche, sauf teinte qui le compense.
// Rendu seulement : rien ici ne touche la generation, le reseau ni la lumiere de gameplay.
#pragma once

#include <cmath>

namespace BRColor
{
	/** Chromaticite (x, y) du lieu de Planck (Krystek 1985, comme le moteur ; 1000 a 15000 K) */
	inline void PlanckianXY(double T, double& X, double& Y)
	{
		const double U = (0.860117757 + 1.54118254e-4 * T + 1.28641212e-7 * T * T) / (1.0 + 8.42420235e-4 * T + 7.08145163e-7 * T * T);
		const double V = (0.317398726 + 4.22806245e-5 * T + 4.20481691e-8 * T * T) / (1.0 - 2.89741816e-5 * T + 1.61456053e-7 * T * T);
		const double Den = 2.0 * U - 8.0 * V + 4.0;
		X = 3.0 * U / Den;
		Y = 2.0 * V / Den;
	}

	/** Chromaticite (x, y) de l'illuminant D (lieu du jour, 4000 a 25000 K ; 6500 donne D65), comme le moteur */
	inline void DaylightXY(double T, double& X, double& Y)
	{
		T *= 1.4388 / 1.438;
		const double Inv = 1.0 / T;
		X = T <= 7000.0 ? 0.244063 + (0.09911e3 + (2.9678e6 - 4.6070e9 * Inv) * Inv) * Inv
			: 0.237040 + (0.24748e3 + (1.9018e6 - 2.0064e9 * Inv) * Inv) * Inv;
		Y = -3.0 * X * X + 2.87 * X - 0.275;
	}

	/** Point (x, y) decale de Tint le long de l'isotherme de T (PlanckianIsothermal du moteur) */
	inline void IsothermalXY(double T, double Tint, double& X, double& Y)
	{
		double U = (0.860117757 + 1.54118254e-4 * T + 1.28641212e-7 * T * T) / (1.0 + 8.42420235e-4 * T + 7.08145163e-7 * T * T);
		double V = (0.317398726 + 4.22806245e-5 * T + 4.20481691e-8 * T * T) / (1.0 - 2.89741816e-5 * T + 1.61456053e-7 * T * T);
		const double UdBase = 1.41213984e6 + 1189.62 * T + T * T;
		const double VdBase = 6.19363586e6 - 179.456 * T + T * T;
		const double Ud = (-1.13758118e9 - 1.91615621e6 * T - 1.53177 * T * T) / (UdBase * UdBase);
		const double Vd = (1.97471536e9 - 705674.0 * T - 308.607 * T * T) / (VdBase * VdBase);
		const double Len = std::sqrt(Ud * Ud + Vd * Vd);
		const double Un = Len > 0.0 ? Ud / Len : 0.0;
		const double Vn = Len > 0.0 ? Vd / Len : 0.0;
		U += Vn * Tint * 0.05;
		V += -Un * Tint * 0.05;
		const double Den = 2.0 * U - 8.0 * V + 4.0;
		X = 3.0 * U / Den;
		Y = 2.0 * V / Den;
	}

	/** sRGB lineaire (BT.709, D65) <-> XYZ */
	inline void RgbToXyz(const double In[3], double Out[3])
	{
		Out[0] = 0.4124564 * In[0] + 0.3575761 * In[1] + 0.1804375 * In[2];
		Out[1] = 0.2126729 * In[0] + 0.7151522 * In[1] + 0.0721750 * In[2];
		Out[2] = 0.0193339 * In[0] + 0.1191920 * In[1] + 0.9503041 * In[2];
	}

	inline void XyzToRgb(const double In[3], double Out[3])
	{
		Out[0] = 3.2404542 * In[0] - 1.5371385 * In[1] - 0.4985314 * In[2];
		Out[1] = -0.9692660 * In[0] + 1.8760108 * In[1] + 0.0415560 * In[2];
		Out[2] = 0.0556434 * In[0] - 0.2040259 * In[1] + 1.0572252 * In[2];
	}

	/** Couleur (sRGB lineaire, luminance 1, negatifs ramenes a 0) d'une lampe de Kelvin K, comme le moteur */
	inline void LampRgb(float Kelvin, float Out[3])
	{
		const double T = Kelvin < 1000.f ? 1000.0 : (Kelvin > 15000.f ? 15000.0 : static_cast<double>(Kelvin));
		double X = 0.0;
		double Y = 0.0;
		PlanckianXY(T, X, Y);
		const double Xyz[3] = { X / Y, 1.0, (1.0 - X - Y) / Y };
		double Rgb[3];
		XyzToRgb(Xyz, Rgb);
		for (int I = 0; I < 3; ++I)
		{
			Out[I] = Rgb[I] > 0.0 ? static_cast<float>(Rgb[I]) : 0.f;
		}
	}

	/** Balance des blancs de la camera (WhiteTemp, WhiteTint du post-process, mode temperature) appliquee a une couleur
	 *  sRGB lineaire, comme le shader du moteur */
	inline void WhiteBalance(const float In[3], float WhiteTemp, float WhiteTint, float Out[3])
	{
		const double T = WhiteTemp;
		double PX = 0.0;
		double PY = 0.0;
		PlanckianXY(T, PX, PY);
		double SX = PX;
		double SY = PY;
		if (T >= 4000.0)
		{
			DaylightXY(T, SX, SY);
		}
		double IX = 0.0;
		double IY = 0.0;
		IsothermalXY(T, WhiteTint, IX, IY);
		SX += IX - PX;
		SY += IY - PY;
		// Bradford : reponses des cones du blanc source et de D65
		const double M[3][3] = { { 0.8951, 0.2664, -0.1614 }, { -0.7502, 1.7135, 0.0367 }, { 0.0389, -0.0685, 1.0296 } };
		const double Mi[3][3] = { { 0.9869929, -0.1470543, 0.1599627 }, { 0.4323053, 0.5183603, 0.0492912 }, { -0.0085287, 0.0400428, 0.9684867 } };
		const double Src[3] = { SX / SY, 1.0, (1.0 - SX - SY) / SY };
		const double DX = 0.31270;
		const double DY = 0.32900;
		const double Dst[3] = { DX / DY, 1.0, (1.0 - DX - DY) / DY };
		double SrcCone[3];
		double DstCone[3];
		for (int I = 0; I < 3; ++I)
		{
			SrcCone[I] = M[I][0] * Src[0] + M[I][1] * Src[1] + M[I][2] * Src[2];
			DstCone[I] = M[I][0] * Dst[0] + M[I][1] * Dst[1] + M[I][2] * Dst[2];
		}
		const double Rgb[3] = { In[0], In[1], In[2] };
		double Xyz[3];
		RgbToXyz(Rgb, Xyz);
		double Cone[3];
		for (int I = 0; I < 3; ++I)
		{
			Cone[I] = (M[I][0] * Xyz[0] + M[I][1] * Xyz[1] + M[I][2] * Xyz[2]) * (SrcCone[I] != 0.0 ? DstCone[I] / SrcCone[I] : 1.0);
		}
		double Adapted[3];
		for (int I = 0; I < 3; ++I)
		{
			Adapted[I] = Mi[I][0] * Cone[0] + Mi[I][1] * Cone[1] + Mi[I][2] * Cone[2];
		}
		double Res[3];
		XyzToRgb(Adapted, Res);
		for (int I = 0; I < 3; ++I)
		{
			Out[I] = static_cast<float>(Res[I]);
		}
	}

	/** Couleur apparente a l'ecran d'une lampe : temperature (Kelvin > 0 ; sinon blanc), multipliee par Tint (LightColor),
	 *  vue a travers la balance des blancs, ramenee a un maximum de 1 */
	inline void ApparentLamp(float Kelvin, const float Tint[3], float WhiteTemp, float WhiteTint, float Out[3])
	{
		float Lamp[3] = { 1.f, 1.f, 1.f };
		if (Kelvin > 0.f)
		{
			LampRgb(Kelvin, Lamp);
		}
		const float Lit[3] = { Lamp[0] * Tint[0], Lamp[1] * Tint[1], Lamp[2] * Tint[2] };
		float Seen[3];
		WhiteBalance(Lit, WhiteTemp, WhiteTint, Seen);
		float Peak = 0.f;
		for (int I = 0; I < 3; ++I)
		{
			Seen[I] = Seen[I] > 0.f ? Seen[I] : 0.f;
			Peak = Seen[I] > Peak ? Seen[I] : Peak;
		}
		for (int I = 0; I < 3; ++I)
		{
			Out[I] = Peak > 0.001f ? Seen[I] / Peak : Tint[I];
		}
	}

	/** v4.15 : couleur (LightColor) a donner a une lampe portee (lampe torche, frontale) pour qu'elle garde a l'ecran la
	 *  couleur Seen qu'elle avait avec une balance des blancs neutre (6500 K). WhiteBalance est lineaire (une matrice 3x3,
	 *  tiree ici des trois primaires) : on l'inverse. Negatifs ramenes a 0, maximum 1 ; Gain (facultatif) : facteur de la
	 *  couleur vue obtenue (Seen x Gain), pour juger la perte de luminosite */
	inline void UnbalancedLamp(const float Seen[3], float WhiteTemp, float WhiteTint, float Out[3], float* Gain = nullptr)
	{
		double Mx[3][3];
		for (int J = 0; J < 3; ++J)
		{
			float Unit[3] = { 0.f, 0.f, 0.f };
			Unit[J] = 1.f;
			float Col[3];
			WhiteBalance(Unit, WhiteTemp, WhiteTint, Col);
			for (int I = 0; I < 3; ++I)
			{
				Mx[I][J] = Col[I];
			}
		}
		const double C00 = Mx[1][1] * Mx[2][2] - Mx[1][2] * Mx[2][1];
		const double C01 = Mx[1][2] * Mx[2][0] - Mx[1][0] * Mx[2][2];
		const double C02 = Mx[1][0] * Mx[2][1] - Mx[1][1] * Mx[2][0];
		const double Det = Mx[0][0] * C00 + Mx[0][1] * C01 + Mx[0][2] * C02;
		if (std::fabs(Det) < 1e-9)
		{
			for (int I = 0; I < 3; ++I)
			{
				Out[I] = Seen[I];
			}
			if (Gain)
			{
				*Gain = 1.f;
			}
			return;
		}
		// Inverse = adjointe / determinant
		const double Inv[3][3] = {
			{ C00 / Det, (Mx[0][2] * Mx[2][1] - Mx[0][1] * Mx[2][2]) / Det, (Mx[0][1] * Mx[1][2] - Mx[0][2] * Mx[1][1]) / Det },
			{ C01 / Det, (Mx[0][0] * Mx[2][2] - Mx[0][2] * Mx[2][0]) / Det, (Mx[0][2] * Mx[1][0] - Mx[0][0] * Mx[1][2]) / Det },
			{ C02 / Det, (Mx[0][1] * Mx[2][0] - Mx[0][0] * Mx[2][1]) / Det, (Mx[0][0] * Mx[1][1] - Mx[0][1] * Mx[1][0]) / Det } };
		double Lamp[3];
		double Peak = 0.0;
		for (int I = 0; I < 3; ++I)
		{
			Lamp[I] = Inv[I][0] * Seen[0] + Inv[I][1] * Seen[1] + Inv[I][2] * Seen[2];
			Lamp[I] = Lamp[I] > 0.0 ? Lamp[I] : 0.0;
			Peak = Lamp[I] > Peak ? Lamp[I] : Peak;
		}
		for (int I = 0; I < 3; ++I)
		{
			Out[I] = Peak > 1e-6 ? static_cast<float>(Lamp[I] / Peak) : Seen[I];
		}
		if (Gain)
		{
			*Gain = Peak > 1e-6 ? static_cast<float>(1.0 / Peak) : 1.f;
		}
	}

	/** Teinte (degres, 0..360) et saturation (0..1) d'une couleur, comme un selecteur TSV */
	inline void HueSat(const float C[3], float& Hue, float& Sat)
	{
		const float Mx = C[0] > C[1] ? (C[0] > C[2] ? C[0] : C[2]) : (C[1] > C[2] ? C[1] : C[2]);
		const float Mn = C[0] < C[1] ? (C[0] < C[2] ? C[0] : C[2]) : (C[1] < C[2] ? C[1] : C[2]);
		const float Delta = Mx - Mn;
		Sat = Mx > 0.f ? Delta / Mx : 0.f;
		Hue = 0.f;
		if (Delta <= 0.f)
		{
			return;
		}
		if (Mx == C[0])
		{
			Hue = 60.f * std::fmod((C[1] - C[2]) / Delta, 6.f);
		}
		else if (Mx == C[1])
		{
			Hue = 60.f * ((C[2] - C[0]) / Delta + 2.f);
		}
		else
		{
			Hue = 60.f * ((C[0] - C[1]) / Delta + 4.f);
		}
		if (Hue < 0.f)
		{
			Hue += 360.f;
		}
	}
}
