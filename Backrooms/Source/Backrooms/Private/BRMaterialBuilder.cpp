#include "BRMaterialBuilder.h"
#include "Backrooms.h"

#include "Engine/Texture.h"
#include "Materials/Material.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#endif

#if WITH_EDITOR
namespace
{
	/** Sortie d'un noeud (TextureSample : 0 = RGB, 1 = R, 2 = G, 3 = B, 4 = A) */
	struct FPin
	{
		UMaterialExpression* Expr = nullptr;
		int32 Out = 0;
	};

	/** Petit constructeur de graphe : meme vocabulaire que la classe Graph du script Python */
	class FGraph
	{
	public:
		explicit FGraph(UMaterial* InMat) : Mat(InMat) {}

		template <typename T>
		T* New()
		{
			T* E = NewObject<T>(Mat);
			E->Material = Mat;
			Mat->GetExpressionCollection().AddExpression(E);
			return E;
		}

		static void Link(FExpressionInput& In, const FPin& P)
		{
			if (P.Expr)
			{
				In.Connect(P.Out, P.Expr);
			}
		}

		FPin Scalar(const TCHAR* Name, float Def)
		{
			UMaterialExpressionScalarParameter* E = New<UMaterialExpressionScalarParameter>();
			E->ParameterName = FName(Name);
			E->DefaultValue = Def;
			return FPin{ E, 0 };
		}

		FPin Vector(const TCHAR* Name, const FLinearColor& Def)
		{
			UMaterialExpressionVectorParameter* E = New<UMaterialExpressionVectorParameter>();
			E->ParameterName = FName(Name);
			E->DefaultValue = Def;
			return FPin{ E, 0 };
		}

		FPin Const(float V)
		{
			UMaterialExpressionConstant* E = New<UMaterialExpressionConstant>();
			E->R = V;
			return FPin{ E, 0 };
		}

		FPin C2(float R, float G)
		{
			UMaterialExpressionConstant2Vector* E = New<UMaterialExpressionConstant2Vector>();
			E->R = R;
			E->G = G;
			return FPin{ E, 0 };
		}

		FPin C3(float R, float G, float B)
		{
			UMaterialExpressionConstant3Vector* E = New<UMaterialExpressionConstant3Vector>();
			E->Constant = FLinearColor(R, G, B, 1.f);
			return FPin{ E, 0 };
		}

		FPin WorldPos() { return FPin{ New<UMaterialExpressionWorldPosition>(), 0 }; }
		FPin VertexNormal() { return FPin{ New<UMaterialExpressionVertexNormalWS>(), 0 }; }
		FPin Time() { return FPin{ New<UMaterialExpressionTime>(), 0 }; }
		FPin UV0() { return FPin{ New<UMaterialExpressionTextureCoordinate>(), 0 }; }

		FPin Tex(const TCHAR* Name, UTexture* Default, bool bNormal, const FPin& UV, int32 Out = 0)
		{
			UMaterialExpressionTextureSampleParameter2D* E = New<UMaterialExpressionTextureSampleParameter2D>();
			E->ParameterName = FName(Name);
			E->Texture = Default;
			E->SamplerType = bNormal ? SAMPLERTYPE_Normal : SAMPLERTYPE_Color;
			Link(E->Coordinates, UV);
			return FPin{ E, Out };
		}

		FPin Mask(const FPin& In, const TCHAR* Channels)
		{
			UMaterialExpressionComponentMask* E = New<UMaterialExpressionComponentMask>();
			const FString C(Channels);
			E->R = C.Contains(TEXT("r")) ? 1 : 0;
			E->G = C.Contains(TEXT("g")) ? 1 : 0;
			E->B = C.Contains(TEXT("b")) ? 1 : 0;
			E->A = C.Contains(TEXT("a")) ? 1 : 0;
			Link(E->Input, In);
			return FPin{ E, 0 };
		}

		template <typename T>
		FPin Bin(const FPin& InA, const FPin& InB)
		{
			T* E = New<T>();
			Link(E->A, InA);
			Link(E->B, InB);
			return FPin{ E, 0 };
		}

		FPin Mul(const FPin& InA, const FPin& InB) { return Bin<UMaterialExpressionMultiply>(InA, InB); }
		FPin Div(const FPin& InA, const FPin& InB) { return Bin<UMaterialExpressionDivide>(InA, InB); }
		FPin Add(const FPin& InA, const FPin& InB) { return Bin<UMaterialExpressionAdd>(InA, InB); }
		FPin Sub(const FPin& InA, const FPin& InB) { return Bin<UMaterialExpressionSubtract>(InA, InB); }
		FPin Min(const FPin& InA, const FPin& InB) { return Bin<UMaterialExpressionMin>(InA, InB); }
		FPin Dot(const FPin& InA, const FPin& InB) { return Bin<UMaterialExpressionDotProduct>(InA, InB); }
		FPin Append(const FPin& InA, const FPin& InB) { return Bin<UMaterialExpressionAppendVector>(InA, InB); }

		FPin Abs(const FPin& In)
		{
			UMaterialExpressionAbs* E = New<UMaterialExpressionAbs>();
			Link(E->Input, In);
			return FPin{ E, 0 };
		}

		FPin Sat(const FPin& In)
		{
			UMaterialExpressionSaturate* E = New<UMaterialExpressionSaturate>();
			Link(E->Input, In);
			return FPin{ E, 0 };
		}

		FPin Sine(const FPin& In)
		{
			UMaterialExpressionSine* E = New<UMaterialExpressionSine>();
			E->Period = 6.283185f; // sin(x) et non sin(2 pi x)
			Link(E->Input, In);
			return FPin{ E, 0 };
		}

		/** Noeud HLSL "Custom" : Inputs = (nom, entree) */
		FPin Custom(const TCHAR* Description, const FString& Code, ECustomMaterialOutputType OutType, const TArray<TPair<FName, FPin>>& Inputs)
		{
			UMaterialExpressionCustom* E = New<UMaterialExpressionCustom>();
			E->Description = Description;
			E->Code = Code;
			E->OutputType = OutType;
			E->Inputs.Reset();
			for (const TPair<FName, FPin>& In : Inputs)
			{
				FCustomInput CI;
				CI.InputName = In.Key;
				E->Inputs.Add(CI);
			}
			for (int32 i = 0; i < Inputs.Num(); ++i)
			{
				Link(E->Inputs[i].Input, Inputs[i].Value);
			}
			return FPin{ E, 0 };
		}

		FPin Lerp(const FPin& InA, const FPin& InB, const FPin& Alpha)
		{
			UMaterialExpressionLinearInterpolate* E = New<UMaterialExpressionLinearInterpolate>();
			Link(E->A, InA);
			Link(E->B, InB);
			Link(E->Alpha, Alpha);
			return FPin{ E, 0 };
		}

		UMaterial* Mat;
	};

	UTexture* EngineTexture(bool bNormal)
	{
		return LoadObject<UTexture>(nullptr, bNormal ? TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal")
			: TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	}

	void BuildWorld(FGraph& G, UMaterialEditorOnlyData* Out, TFunctionRef<UTexture*(FName)> LoadTexture)
	{
		UTexture* Grime = LoadTexture(TEXT("T_Grime"));
		UTexture* Caustics = LoadTexture(TEXT("T_Caustics"));
		UTexture* BaseDefault = EngineTexture(false);
		UTexture* NormalDefault = EngineTexture(true);
		if (!Grime)
		{
			Grime = BaseDefault;
		}
		if (!Caustics)
		{
			Caustics = BaseDefault;
		}

		const FPin WP = G.WorldPos();
		const FPin D = G.Div(WP, G.Scalar(TEXT("TexScale"), 200.f));
		const FPin Flip = G.C2(1.f, -1.f);
		const FPin UvX = G.Mul(G.Mask(D, TEXT("gb")), Flip);
		const FPin UvY = G.Mul(G.Mask(D, TEXT("rb")), Flip);
		const FPin UvZ = G.Mask(D, TEXT("rg"));

		const FPin A = G.Abs(G.VertexNormal());
		const FPin A2 = G.Mul(A, A);
		const FPin W4 = G.Mul(A2, A2);
		const FPin Wn = G.Div(W4, G.Dot(W4, G.C3(1.f, 1.f, 1.f)));
		const FPin Wx = G.Mask(Wn, TEXT("r"));
		const FPin Wy = G.Mask(Wn, TEXT("g"));
		const FPin Wz = G.Mask(Wn, TEXT("b"));

		auto Tri = [&](const TCHAR* Name, UTexture* Tex, bool bNormal)
		{
			const FPin Sx = G.Tex(Name, Tex, bNormal, UvX);
			const FPin Sy = G.Tex(Name, Tex, bNormal, UvY);
			const FPin Sz = G.Tex(Name, Tex, bNormal, UvZ);
			return G.Add(G.Add(G.Mul(Sx, Wx), G.Mul(Sy, Wy)), G.Mul(Sz, Wz));
		};

		FPin Col = Tri(TEXT("BaseTex"), BaseDefault, false);

		// Salete a grande echelle
		const FPin GUv = G.Div(G.Append(G.Dot(WP, G.C3(0.7f, 0.3f, 0.f)), G.Dot(WP, G.C3(0.f, 0.5f, 1.f))), G.Scalar(TEXT("GrimeScale"), 900.f));
		const FPin Gs = G.Tex(TEXT("GrimeTex"), Grime, false, GUv, 1);
		Col = G.Mul(Col, G.Lerp(G.Const(1.f), Gs, G.Scalar(TEXT("Grime"), 0.35f)));

		// Salete au pied des murs
		const FPin Z = G.Mask(WP, TEXT("b"));
		const FPin Wall = G.Sat(G.Sub(G.Const(1.f), G.Mul(G.Mask(A, TEXT("b")), G.Const(1.5f))));
		const FPin H = G.Sat(G.Sub(G.Const(1.f), G.Div(Z, G.Const(70.f))));
		const FPin FUv = G.Div(G.Append(G.Dot(WP, G.C3(1.f, 1.f, 0.f)), G.Mul(Z, G.Const(2.f))), G.Const(180.f));
		const FPin Fn = G.Tex(TEXT("GrimeTex"), Grime, false, FUv, 2);
		const FPin Fg = G.Mul(G.Mul(G.Scalar(TEXT("FloorGrime"), 0.f), Wall), G.Mul(H, H));
		Col = G.Mul(Col, G.Sub(G.Const(1.f), G.Mul(Fg, G.Add(G.Const(0.45f), G.Mul(Fn, G.Const(0.55f))))));

		// Caustiques
		const FPin T = G.Time();
		const FPin CUv = G.Lerp(G.Append(G.Dot(WP, G.C3(1.f, 1.f, 0.f)), Z), G.Mask(WP, TEXT("rg")), Wz);
		const FPin K1 = G.Tex(TEXT("CausticsTex"), Caustics, false, G.Add(G.Div(CUv, G.Const(260.f)), G.Mul(T, G.C2(0.031f, 0.017f))), 1);
		const FPin K2 = G.Tex(TEXT("CausticsTex"), Caustics, false,
			G.Add(G.Mul(CUv, G.C2(-1.f / 330.f, 1.f / 330.f)), G.Mul(T, G.C2(-0.022f, 0.026f))), 1);
		const FPin K = G.Min(K1, K2);
		const FPin Fade = G.Lerp(G.Sat(G.Sub(G.Const(1.f), G.Div(G.Abs(G.Sub(Z, G.Const(45.f))), G.Const(260.f)))), G.Const(1.f), Wz);
		const FPin Cm = G.Add(G.Const(1.f), G.Mul(G.Mul(G.Scalar(TEXT("Caustics"), 0.f), Fade), G.Sub(G.Mul(K, G.Const(2.2f)), G.Const(0.35f))));
		Col = G.Mul(Col, Cm);

		const FPin Base = G.Mul(Col, G.Vector(TEXT("Tint"), FLinearColor::White));
		FGraph::Link(Out->BaseColor, Base);

		const FPin Rv = G.Tex(TEXT("GrimeTex"), Grime, false, G.Mul(GUv, G.Const(3.1f)), 3);
		FGraph::Link(Out->Roughness, G.Sat(G.Add(G.Scalar(TEXT("Roughness"), 0.85f), G.Mul(G.Sub(Rv, G.Const(0.5f)), G.Const(0.3f)))));
		FGraph::Link(Out->Metallic, G.Scalar(TEXT("Metallic"), 0.f));
		FGraph::Link(Out->EmissiveColor, G.Add(G.Mul(Base, G.Scalar(TEXT("SelfIllum"), 0.f)), G.Vector(TEXT("Emissive"), FLinearColor::Black)));

		const FPin Nrm = Tri(TEXT("NormalTex"), NormalDefault, true);
		FGraph::Link(Out->Normal, G.Lerp(G.C3(0.f, 0.f, 1.f), Nrm, G.Scalar(TEXT("NormalStrength"), 1.f)));
	}

	void BuildMesh(FGraph& G, UMaterialEditorOnlyData* Out, bool bSkin, TFunctionRef<UTexture*(FName)> LoadTexture)
	{
		UTexture* Def = LoadTexture(bSkin ? FName(TEXT("T_Skin")) : FName(TEXT("T_Grime")));
		if (!Def)
		{
			Def = EngineTexture(false);
		}
		const FPin Uv = G.Mul(G.UV0(), G.Scalar(TEXT("TexScale"), 1.f));
		const FPin Base = G.Mul(G.Tex(TEXT("BaseTex"), Def, false, Uv), G.Vector(TEXT("Tint"), FLinearColor::White));
		FGraph::Link(Out->BaseColor, Base);
		FGraph::Link(Out->Roughness, G.Scalar(TEXT("Roughness"), bSkin ? 0.6f : 0.85f));
		FGraph::Link(Out->Metallic, G.Scalar(TEXT("Metallic"), 0.f));
		FGraph::Link(Out->EmissiveColor, G.Add(G.Mul(Base, G.Scalar(TEXT("SelfIllum"), 0.f)), G.Vector(TEXT("Emissive"), FLinearColor::Black)));
		if (bSkin)
		{
			const FPin N = G.Tex(TEXT("NormalTex"), EngineTexture(true), true, Uv);
			FGraph::Link(Out->Normal, G.Lerp(G.C3(0.f, 0.f, 1.f), N, G.Scalar(TEXT("NormalStrength"), 0.8f)));
			FGraph::Link(Out->SubsurfaceColor, G.Mul(Base, G.Vector(TEXT("SubsurfaceColor"), FLinearColor(1.f, 0.35f, 0.25f))));
			FGraph::Link(Out->Opacity, G.Scalar(TEXT("Subsurface"), 0.6f));
		}
	}

	void BuildWater(FGraph& G, UMaterialEditorOnlyData* Out)
	{
		const FPin WP = G.WorldPos();
		const FPin XY = G.Mask(WP, TEXT("rg"));
		const FPin T = G.Time();

		// Surface : houle (deplace la surface), clapot (normales) et ondes circulaires autour du joueur.
		// Meme code HLSL que BR_WATER_SURFACE_HLSL dans Content/Python/backrooms_setup.py.
		TArray<TPair<FName, FPin>> Inputs;
		Inputs.Add(TPair<FName, FPin>(TEXT("P"), XY));
		Inputs.Add(TPair<FName, FPin>(TEXT("T"), T));
		Inputs.Add(TPair<FName, FPin>(TEXT("Amp"), G.Scalar(TEXT("WaveAmplitude"), 1.f)));
		Inputs.Add(TPair<FName, FPin>(TEXT("Chop"), G.Scalar(TEXT("WaveChop"), 1.f)));
		for (int32 i = 0; i < BRMaterialBuilder::NumRipples; ++i)
		{
			const FString Name = FString::Printf(TEXT("Ripple%d"), i);
			Inputs.Add(TPair<FName, FPin>(FName(*FString::Printf(TEXT("R%d"), i)), G.Vector(*Name, FLinearColor(0.f, 0.f, 0.f, 0.f))));
		}
		const FPin Surface = G.Custom(TEXT("BRWaterSurface"), BRMaterialBuilder::WaterSurfaceHLSL(), CMOT_Float3, Inputs);
		const FPin Height = G.Mask(Surface, TEXT("b"));
		const FPin Slope = G.Mask(Surface, TEXT("rg"));
		FGraph::Link(Out->WorldPositionOffset, G.Append(G.C2(0.f, 0.f), Height));

		// Rides de detail
		UTexture* NTex = EngineTexture(true);
		const FPin Scale = G.Scalar(TEXT("TexScale"), 300.f);
		const FPin UvA = G.Add(G.Div(XY, Scale), G.Mul(T, G.C2(0.012f, 0.008f)));
		const FPin UvB = G.Add(G.Mul(G.Div(XY, Scale), G.C2(-1.6f, 1.6f)), G.Mul(T, G.C2(-0.01f, 0.014f)));
		const FPin Na = G.Tex(TEXT("NormalTex"), NTex, true, UvA);
		const FPin Nb = G.Tex(TEXT("NormalTex"), NTex, true, UvB);
		const FPin Detail = G.Mul(G.Mask(G.Add(Na, Nb), TEXT("rg")), G.Scalar(TEXT("NormalStrength"), 0.35f));
		FGraph::Link(Out->Normal, G.Append(G.Sub(Detail, Slope), G.Const(1.f)));

		FGraph::Link(Out->BaseColor, G.C3(0.f, 0.f, 0.f));
		FGraph::Link(Out->Roughness, G.Scalar(TEXT("Roughness"), 0.04f));
		FGraph::Link(Out->Specular, G.Const(0.5f));

		// Proprietes optiques (coefficients par metre convertis en 1/cm)
		const FPin Tint = G.Vector(TEXT("Tint"), FLinearColor(0.35f, 0.75f, 0.8f));
		const FPin Absorb = G.Mul(G.Add(G.Mul(G.Sub(G.C3(1.f, 1.f, 1.f), Tint), G.Scalar(TEXT("Absorption"), 1.2f)), G.C3(0.02f, 0.02f, 0.02f)), G.Const(0.01f));
		const FPin Scatter = G.Mul(G.Mul(Tint, G.Scalar(TEXT("Scattering"), 0.15f)), G.Const(0.01f));
		UMaterialExpressionSingleLayerWaterMaterialOutput* WaterOut = G.New<UMaterialExpressionSingleLayerWaterMaterialOutput>();
		FGraph::Link(WaterOut->ScatteringCoefficients, Scatter);
		FGraph::Link(WaterOut->AbsorptionCoefficients, Absorb);
		FGraph::Link(WaterOut->PhaseG, G.Const(0.1f));
		FGraph::Link(WaterOut->ColorScaleBehindWater, G.Const(1.f));
	}
}
#endif

namespace BRMaterialBuilder
{
	const FString& WaterSurfaceHLSL()
	{
		// Entrees : P (XY monde, cm), T (temps, s), Amp (houle), Chop (clapot), R0..R7 (ondes : x, y, rayon, amplitude).
		// Sortie : float3(pente X, pente Y, hauteur de la houle).
		static const FString Code = TEXT(
			"float3 acc = float3(0.0, 0.0, 0.0);\n"
			"float2 dir; float k; float ph;\n"
			"// Houle lente : deplace la surface (la grille d'eau a un sommet par metre)\n"
			"dir = float2(0.8, 0.6); k = 6.2831853 / 620.0; ph = dot(P, dir) * k + T * 0.8;\n"
			"acc += float3(cos(ph) * 1.3 * k * dir, sin(ph) * 1.3) * Amp;\n"
			"dir = float2(-0.6, 0.8); k = 6.2831853 / 470.0; ph = dot(P, dir) * k + T * 1.05;\n"
			"acc += float3(cos(ph) * 0.8 * k * dir, sin(ph) * 0.8) * Amp;\n"
			"// Clapot : vagues courtes qui ne font que plier les reflets\n"
			"float2 sl = float2(0.0, 0.0);\n"
			"dir = float2(0.8, 0.6); k = 6.2831853 / 340.0; sl += cos(dot(P, dir) * k + T * 1.1) * 0.9 * k * dir;\n"
			"dir = float2(-0.5, 0.866); k = 6.2831853 / 210.0; sl += cos(dot(P, dir) * k + T * 1.6) * 0.55 * k * dir;\n"
			"dir = float2(0.2, -0.98); k = 6.2831853 / 130.0; sl += cos(dot(P, dir) * k + T * 2.3) * 0.3 * k * dir;\n"
			"dir = float2(-0.94, -0.34); k = 6.2831853 / 75.0; sl += cos(dot(P, dir) * k + T * 3.1) * 0.16 * k * dir;\n"
			"dir = float2(0.57, -0.82); k = 6.2831853 / 46.0; sl += cos(dot(P, dir) * k + T * 4.2) * 0.08 * k * dir;\n"
			"acc.xy += sl * Amp * Chop;\n"
			"// Ondes circulaires (pas du joueur, nage, gouttes) : front gaussien qui s'eloigne en s'elargissant\n"
			"float4 R[8] = { R0, R1, R2, R3, R4, R5, R6, R7 };\n"
			"[unroll] for (int i = 0; i < 8; i++)\n"
			"{\n"
			"  float4 r = R[i];\n"
			"  if (r.w > 0.0005)\n"
			"  {\n"
			"    float2 d2 = P - r.xy;\n"
			"    float d = max(length(d2), 0.5);\n"
			"    float x = d - r.z;\n"
			"    float w = 18.0 + 0.15 * r.z;\n"
			"    float kk = 6.2831853 / 22.0;\n"
			"    float env = exp(-(x * x) / (w * w));\n"
			"    float dh = r.w * env * (kk * cos(kk * x) - 2.0 * x / (w * w) * sin(kk * x));\n"
			"    acc.xy += dh * d2 / d;\n"
			"  }\n"
			"}\n"
			"return acc;\n");
		return Code;
	}

	bool IsAvailable()
	{
#if WITH_EDITOR
		return !FPlatformProperties::RequiresCookedData();
#else
		return false;
#endif
	}

	UMaterial* Build(EBRMasterMaterial Which, UObject* Outer, TFunctionRef<UTexture*(FName)> LoadTexture)
	{
#if WITH_EDITOR
		if (!IsAvailable())
		{
			return nullptr;
		}
		const TCHAR* Names[] = { TEXT("M_BR_World_Runtime"), TEXT("M_BR_Mesh_Runtime"), TEXT("M_BR_Skin_Runtime"), TEXT("M_BR_Water_Runtime") };
		UMaterial* M = NewObject<UMaterial>(Outer ? Outer : GetTransientPackage(), FName(Names[static_cast<int32>(Which)]), RF_Transient);
		M->MaterialDomain = MD_Surface;
		M->BlendMode = BLEND_Opaque;
		M->bUsedWithInstancedStaticMeshes = true;
		M->bTangentSpaceNormal = true;

		UMaterialEditorOnlyData* Out = M->GetEditorOnlyData();
		if (!Out)
		{
			return nullptr;
		}
		FGraph G(M);
		switch (Which)
		{
		case EBRMasterMaterial::World:
			BuildWorld(G, Out, LoadTexture);
			break;
		case EBRMasterMaterial::Mesh:
			BuildMesh(G, Out, false, LoadTexture);
			break;
		case EBRMasterMaterial::Skin:
			M->SetShadingModel(MSM_Subsurface);
			BuildMesh(G, Out, true, LoadTexture);
			break;
		case EBRMasterMaterial::Water:
			M->SetShadingModel(MSM_SingleLayerWater);
			BuildWater(G, Out);
			break;
		}

		// Compile les shaders (de facon asynchrone : le materiau par defaut est affiche quelques secondes)
		M->PreEditChange(nullptr);
		M->PostEditChange();
		UE_LOG(LogBackrooms, Log, TEXT("Materiau construit a la volee : %s"), *M->GetName());
		return M;
#else
		(void)Which;
		(void)Outer;
		(void)LoadTexture;
		return nullptr;
#endif
	}
}
