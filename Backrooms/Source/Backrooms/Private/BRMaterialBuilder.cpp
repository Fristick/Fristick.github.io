#include "BRMaterialBuilder.h"
#include "Backrooms.h"

#include "Engine/Texture.h"
#include "Materials/Material.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
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
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSceneDepth.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
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
		/** Sorties d'un VectorParameter : 0 = RGB (float3) ... 5 = RGBA (float4) */
		static constexpr int32 VectorRGBA = 5;

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
		FPin CameraVector() { return FPin{ New<UMaterialExpressionCameraVectorWS>(), 0 }; }
		FPin PixelDepth() { return FPin{ New<UMaterialExpressionPixelDepth>(), 0 }; }
		FPin TwoSidedSign() { return FPin{ New<UMaterialExpressionTwoSidedSign>(), 0 }; }

		/** Texture passee telle quelle a un noeud Custom (echantillonnee dans le HLSL : Nom, NomSampler) */
		FPin TexObj(const TCHAR* Name, UTexture* Default)
		{
			UMaterialExpressionTextureObjectParameter* E = New<UMaterialExpressionTextureObjectParameter>();
			E->ParameterName = FName(Name);
			E->Texture = Default;
			E->SamplerType = SAMPLERTYPE_Color;
			return FPin{ E, 0 };
		}

		/** Image de la scene derriere un materiau translucide, decalee de Offset (fraction de l'ecran) si fourni */
		FPin SceneColor(const FPin* Offset = nullptr)
		{
			UMaterialExpressionSceneColor* E = New<UMaterialExpressionSceneColor>();
			E->InputMode = EMaterialSceneAttributeInputMode::OffsetFraction;
			if (Offset)
			{
				Link(E->Input, *Offset);
			}
			return FPin{ E, 0 };
		}

		/** Profondeur de la scene opaque (cm), decalee de Offset si fourni */
		FPin SceneDepth(const FPin* Offset = nullptr)
		{
			UMaterialExpressionSceneDepth* E = New<UMaterialExpressionSceneDepth>();
			E->InputMode = EMaterialSceneAttributeInputMode::OffsetFraction;
			if (Offset)
			{
				Link(E->Input, *Offset);
			}
			return FPin{ E, 0 };
		}

		/** Vecteur du monde exprime dans l'espace de la camera (X a droite, Y en haut) */
		FPin WorldToView(const FPin& In)
		{
			UMaterialExpressionTransform* E = New<UMaterialExpressionTransform>();
			E->TransformSourceType = TRANSFORMSOURCE_World;
			E->TransformType = TRANSFORM_View;
			Link(E->Input, In);
			return FPin{ E, 0 };
		}

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

	/** Texture par defaut de la simulation de l'eau (remplacee en jeu par celle de UBRWaterSim) */
	UTexture* BlackTexture()
	{
		UTexture* Black = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/Black.Black"));
		return Black ? Black : EngineTexture(false);
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

		// v4.3 : anti-repetition des sols et plafonds (AntiTile) : un 2e echantillon, tourne de 37 degres et a une autre
		// echelle, se melange au premier selon un bruit a grande echelle (13 m) ; les motifs ne s'alignent plus
		UTexture* NoiseLF = LoadTexture(TEXT("T_NoiseLF"));
		if (!NoiseLF)
		{
			NoiseLF = Grime;
		}
		const FPin AntiTile = G.Scalar(TEXT("AntiTile"), 0.f);
		const FPin UvZ2 = G.Add(G.Mul(G.Append(G.Dot(UvZ, G.C2(0.8f, -0.6f)), G.Dot(UvZ, G.C2(0.6f, 0.8f))), G.Const(0.77f)), G.C2(0.37f, 0.61f));
		const FPin XY = G.Mask(WP, TEXT("rg"));
		const FPin MixNoise = G.Tex(TEXT("NoiseTex"), NoiseLF, false, G.Div(XY, G.Const(1300.f)), 3);
		const FPin MixZ = G.Mul(G.Sat(G.Div(G.Sub(MixNoise, G.Const(0.45f)), G.Const(0.1f))), AntiTile);

		auto Tri = [&](const TCHAR* Name, UTexture* Tex, bool bNormal)
		{
			const FPin Sx = G.Tex(Name, Tex, bNormal, UvX);
			const FPin Sy = G.Tex(Name, Tex, bNormal, UvY);
			const FPin Sz1 = G.Tex(Name, Tex, bNormal, UvZ);
			FPin Sz2 = G.Tex(Name, Tex, bNormal, UvZ2);
			if (bNormal)
			{
				// Pente du 2e echantillon ramenee dans le repere des UV d'origine (rotation inverse)
				const FPin Nxy = G.Mask(Sz2, TEXT("rg"));
				Sz2 = G.Append(G.Append(G.Dot(Nxy, G.C2(0.8f, 0.6f)), G.Dot(Nxy, G.C2(-0.6f, 0.8f))), G.Mask(Sz2, TEXT("b")));
			}
			const FPin Sz = G.Lerp(Sz1, Sz2, MixZ);
			return G.Add(G.Add(G.Mul(Sx, Wx), G.Mul(Sy, Wy)), G.Mul(Sz, Wz));
		};

		FPin Col = Tri(TEXT("BaseTex"), BaseDefault, false);

		// v4.3 : variation de teinte a grande echelle (sols), et taches d'humidite dessinees a l'echelle du monde (Stains)
		const FPin MacroUv = G.Div(G.Append(G.Dot(XY, G.C2(0.6f, 0.8f)), G.Dot(XY, G.C2(-0.8f, 0.6f))), G.Const(2300.f));
		const FPin Macro = G.Tex(TEXT("NoiseTex"), NoiseLF, false, MacroUv, 2);
		Col = G.Mul(Col, G.Add(G.Const(1.f), G.Mul(G.Mul(G.Sub(Macro, G.Const(0.5f)), G.Const(0.45f)), G.Mul(AntiTile, Wz))));
		const FPin StainUv = G.Add(G.Div(G.Append(G.Dot(XY, G.C2(0.92f, -0.39f)), G.Dot(XY, G.C2(0.39f, 0.92f))), G.Scalar(TEXT("StainScale"), 1600.f)), G.C2(0.21f, 0.53f));
		const FPin StainNoise = G.Tex(TEXT("NoiseTex"), NoiseLF, false, StainUv, 1);
		const FPin Stain = G.Mul(G.Sat(G.Div(G.Sub(StainNoise, G.Const(0.7f)), G.Const(0.12f))), G.Mul(G.Scalar(TEXT("Stains"), 0.f), Wz));
		Col = G.Mul(Col, G.Lerp(G.C3(1.f, 1.f, 1.f), G.C3(0.7f, 0.7f, 0.56f), Stain));

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

		// Caustiques (reflets de l'eau sur le carrelage), deformees par les vagues simulees autour du joueur
		TArray<TPair<FName, FPin>> CausticIn;
		CausticIn.Add(TPair<FName, FPin>(TEXT("WP"), WP));
		CausticIn.Add(TPair<FName, FPin>(TEXT("Wz"), Wz));
		CausticIn.Add(TPair<FName, FPin>(TEXT("T"), G.Time()));
		CausticIn.Add(TPair<FName, FPin>(TEXT("Amount"), G.Scalar(TEXT("Caustics"), 0.f)));
		CausticIn.Add(TPair<FName, FPin>(TEXT("CausTex"), G.TexObj(TEXT("CausticsTex"), Caustics)));
		CausticIn.Add(TPair<FName, FPin>(TEXT("SimTex"), G.TexObj(TEXT("WaterSim"), BlackTexture())));
		CausticIn.Add(TPair<FName, FPin>(TEXT("SimWin"), FPin{ G.Vector(TEXT("WaterSimWindow"), FLinearColor(0.f, 0.f, 1000.f, 0.f)).Expr, FGraph::VectorRGBA }));
		Col = G.Mul(Col, G.Custom(TEXT("BRCaustics"), BRMaterialBuilder::CausticsHLSL(), CMOT_Float1, CausticIn));

		const FPin Base = G.Mul(Col, G.Vector(TEXT("Tint"), FLinearColor::White));

		// v4.1 : flaques et sol mouille (reflets ray traces : rugosite quasi nulle, surface plane, ronds de gouttes)
		TArray<TPair<FName, FPin>> PuddleIn;
		PuddleIn.Add(TPair<FName, FPin>(TEXT("WP"), WP));
		PuddleIn.Add(TPair<FName, FPin>(TEXT("N"), G.VertexNormal()));
		PuddleIn.Add(TPair<FName, FPin>(TEXT("T"), G.Time()));
		PuddleIn.Add(TPair<FName, FPin>(TEXT("Amount"), G.Scalar(TEXT("Puddles"), 0.f)));
		PuddleIn.Add(TPair<FName, FPin>(TEXT("Wet"), G.Scalar(TEXT("Wetness"), 0.f)));
		PuddleIn.Add(TPair<FName, FPin>(TEXT("Tex"), G.TexObj(TEXT("PuddleTex"), Grime)));
		const FPin Pud = G.Custom(TEXT("BRPuddles"), BRMaterialBuilder::PuddlesHLSL(), CMOT_Float4, PuddleIn);
		const FPin Puddle = G.Mask(Pud, TEXT("r"));
		const FPin Wet = G.Mask(Pud, TEXT("g"));
		const FPin Ripple = G.Mask(Pud, TEXT("ba"));
		FPin Shaded = G.Mul(Base, G.Lerp(G.Const(1.f), G.Const(0.55f), Wet));
		Shaded = G.Mul(Shaded, G.Lerp(G.Const(1.f), G.Const(0.7f), Puddle));
		FGraph::Link(Out->BaseColor, Shaded);

		const FPin Rv = G.Tex(TEXT("GrimeTex"), Grime, false, G.Mul(GUv, G.Const(3.1f)), 3);
		FPin Rough = G.Sat(G.Add(G.Scalar(TEXT("Roughness"), 0.85f), G.Mul(G.Sub(Rv, G.Const(0.5f)), G.Const(0.3f))));
		Rough = G.Lerp(Rough, G.Mul(Rough, G.Const(0.35f)), Wet);
		Rough = G.Lerp(Rough, G.Const(0.02f), Puddle);
		FGraph::Link(Out->Roughness, Rough);
		FGraph::Link(Out->Metallic, G.Mul(G.Scalar(TEXT("Metallic"), 0.f), G.Sub(G.Const(1.f), Puddle)));
		FGraph::Link(Out->EmissiveColor, G.Add(G.Mul(Base, G.Scalar(TEXT("SelfIllum"), 0.f)), G.Vector(TEXT("Emissive"), FLinearColor::Black)));

		const FPin Nrm = G.Lerp(G.C3(0.f, 0.f, 1.f), Tri(TEXT("NormalTex"), NormalDefault, true), G.Scalar(TEXT("NormalStrength"), 1.f));
		FGraph::Link(Out->Normal, G.Lerp(Nrm, G.Append(Ripple, G.Const(1.f)), Puddle));
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

	/** Eau translucide : meme graphe que build_water_surface_material() en Python */
	void BuildWaterTranslucent(FGraph& G, UMaterialEditorOnlyData* Out)
	{
		const FPin WP = G.WorldPos();
		const FPin XY = G.Mask(WP, TEXT("rg"));
		const FPin T = G.Time();

		// Houle et clapot de fond (BRMaterialBuilder::WaterSurfaceHLSL)
		TArray<TPair<FName, FPin>> SurfIn;
		SurfIn.Add(TPair<FName, FPin>(TEXT("P"), XY));
		SurfIn.Add(TPair<FName, FPin>(TEXT("T"), T));
		SurfIn.Add(TPair<FName, FPin>(TEXT("Amp"), G.Scalar(TEXT("WaveAmplitude"), 1.f)));
		SurfIn.Add(TPair<FName, FPin>(TEXT("Chop"), G.Scalar(TEXT("WaveChop"), 1.f)));
		const FPin Surface = G.Custom(TEXT("BRWaterSurface"), BRMaterialBuilder::WaterSurfaceHLSL(), CMOT_Float3, SurfIn);
		FGraph::Link(Out->WorldPositionOffset, G.Append(G.C2(0.f, 0.f), G.Mask(Surface, TEXT("b"))));

		// Vagues simulees autour du joueur (sillage, ronds dans l'eau)
		TArray<TPair<FName, FPin>> SimIn;
		SimIn.Add(TPair<FName, FPin>(TEXT("P"), XY));
		SimIn.Add(TPair<FName, FPin>(TEXT("Win"), FPin{ G.Vector(TEXT("WaterSimWindow"), FLinearColor(0.f, 0.f, 1000.f, 0.f)).Expr, FGraph::VectorRGBA }));
		SimIn.Add(TPair<FName, FPin>(TEXT("SimTex"), G.TexObj(TEXT("WaterSim"), BlackTexture())));
		const FPin Sim = G.Custom(TEXT("BRWaterSim"), BRMaterialBuilder::WaterSimHLSL(), CMOT_Float2, SimIn);

		// Rides de detail
		UTexture* NTex = EngineTexture(true);
		const FPin Scale = G.Scalar(TEXT("TexScale"), 300.f);
		const FPin UvA = G.Add(G.Div(XY, Scale), G.Mul(T, G.C2(0.012f, 0.008f)));
		const FPin UvB = G.Add(G.Mul(G.Div(XY, Scale), G.C2(-1.6f, 1.6f)), G.Mul(T, G.C2(-0.01f, 0.014f)));
		const FPin Na = G.Tex(TEXT("NormalTex"), NTex, true, UvA);
		const FPin Nb = G.Tex(TEXT("NormalTex"), NTex, true, UvB);
		const FPin Detail = G.Mul(G.Mask(G.Add(Na, Nb), TEXT("rg")), G.Scalar(TEXT("NormalStrength"), 0.1f));
		const FPin NormalXY = G.Sub(G.Sub(Detail, G.Mask(Surface, TEXT("rg"))), Sim);
		FGraph::Link(Out->Normal, G.Append(NormalXY, G.Const(1.f)));

		// Refraction : l'image du fond (couleur et profondeur de la scene) est decalee selon la pente de la surface
		const FPin PixD = G.PixelDepth();
		const FPin D0 = G.SceneDepth();
		TArray<TPair<FName, FPin>> RefIn;
		RefIn.Add(TPair<FName, FPin>(TEXT("VN"), G.WorldToView(G.Append(NormalXY, G.Const(0.f)))));
		RefIn.Add(TPair<FName, FPin>(TEXT("PixD"), PixD));
		RefIn.Add(TPair<FName, FPin>(TEXT("D0"), D0));
		RefIn.Add(TPair<FName, FPin>(TEXT("Strength"), G.Scalar(TEXT("RefractionStrength"), 1.f)));
		const FPin Offset = G.Custom(TEXT("BRWaterRefract"), BRMaterialBuilder::WaterRefractHLSL(), CMOT_Float2, RefIn);

		// Absorption par centimetre d'eau traversee (la teinte est la couleur qui passe le mieux)
		const FPin Tint = G.Vector(TEXT("Tint"), FLinearColor(0.24f, 0.7f, 0.72f));
		const FPin Absorb = G.Mul(G.Add(G.Mul(G.Sub(G.C3(1.f, 1.f, 1.f), Tint), G.Scalar(TEXT("Absorption"), 1.f)), G.C3(0.02f, 0.02f, 0.02f)), G.Const(0.01f));
		TArray<TPair<FName, FPin>> ShadeIn;
		ShadeIn.Add(TPair<FName, FPin>(TEXT("S"), NormalXY));
		ShadeIn.Add(TPair<FName, FPin>(TEXT("V"), G.CameraVector()));
		ShadeIn.Add(TPair<FName, FPin>(TEXT("Side"), G.TwoSidedSign()));
		ShadeIn.Add(TPair<FName, FPin>(TEXT("PixD"), PixD));
		ShadeIn.Add(TPair<FName, FPin>(TEXT("D0"), D0));
		ShadeIn.Add(TPair<FName, FPin>(TEXT("D1"), G.SceneDepth(&Offset)));
		ShadeIn.Add(TPair<FName, FPin>(TEXT("C0"), G.SceneColor()));
		ShadeIn.Add(TPair<FName, FPin>(TEXT("C1"), G.SceneColor(&Offset)));
		ShadeIn.Add(TPair<FName, FPin>(TEXT("Absorb"), Absorb));
		const FPin Shade = G.Custom(TEXT("BRWaterShade"), BRMaterialBuilder::WaterShadeHLSL(), CMOT_Float4, ShadeIn);

		// Lumiere transmise en emission ; voile de l'eau profonde eclaire par la scene ; reflets speculaires
		FGraph::Link(Out->EmissiveColor, G.Mask(Shade, TEXT("rgb")));
		FGraph::Link(Out->BaseColor, G.Mul(G.Mul(Tint, G.Scalar(TEXT("Scattering"), 0.2f)), G.Mask(Shade, TEXT("a"))));
		FGraph::Link(Out->Specular, G.Const(0.35f));
		FGraph::Link(Out->Roughness, G.Scalar(TEXT("Roughness"), 0.03f));
		FGraph::Link(Out->Opacity, G.Const(1.f));
	}
}
#endif

namespace BRMaterialBuilder
{
	const FString& WaterSurfaceHLSL()
	{
		// Entrees : P (XY monde, cm), T (temps, s), Amp (houle), Chop (clapot).
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
			"return acc;\n");
		return Code;
	}

	const FString& WaterSimHLSL()
	{
		// Entrees : P (XY monde, cm), Win (centre X, centre Y, cote en cm, intensite), SimTex (pentes de UBRWaterSim).
		static const FString Code = TEXT(
			"// Vagues simulees autour du joueur : pentes de la surface, estompees au bord de la zone simulee\n"
			"float sz = max(Win.z, 1.0);\n"
			"float2 d = abs(P - Win.xy) / (0.5 * sz);\n"
			"float fade = Win.w * saturate((1.0 - max(d.x, d.y)) / 0.15);\n"
			"return Texture2DSampleLevel(SimTex, SimTexSampler, P / sz, 0.0).rg * fade;\n");
		return Code;
	}

	const FString& WaterRefractHLSL()
	{
		// Entrees : VN (inclinaison de la surface dans l'espace camera), PixD / D0 (profondeurs, cm), Strength.
		// Sortie : decalage de l'image du fond, en fraction de l'ecran.
		static const FString Code = TEXT(
			"// Plus l'eau est epaisse sous ce point, plus l'image du fond est deplacee par la pente de la surface\n"
			"float thick = clamp(D0 - PixD, 0.0, 250.0);\n"
			"return float2(VN.x, -VN.y) * (Strength * 0.12 * thick / max(PixD, 20.0));\n");
		return Code;
	}

	const FString& WaterShadeHLSL()
	{
		// Entrees : S (XY de la normale), V (vers la camera), Side (+1 dessus, -1 dessous), PixD (cm),
		// D0 / C0 (profondeur / couleur de la scene juste derriere), D1 / C1 (idem, image refractee), Absorb (1/cm).
		// Sortie : float4(lumiere qui traverse l'eau, voile de l'eau 0..1).
		static const FString Code = TEXT(
			"float3 N = normalize(float3(S, 1.0));\n"
			"float ndv = saturate(abs(dot(N, normalize(V))));\n"
			"// Image refractee, sauf si elle tombe sur un objet place devant l'eau\n"
			"bool ok = D1 > PixD + 2.0;\n"
			"float3 C = ok ? C1 : C0;\n"
			"float thick = max((ok ? D1 : D0) - PixD, 0.0);\n"
			"if (Side >= 0.0)\n"
			"{\n"
			"  // Vue de dessus : Fresnel de Schlick (eau : F0 = 0,02), absorption selon l'epaisseur traversee\n"
			"  float F = 0.02 + 0.98 * pow(1.0 - ndv, 5.0);\n"
			"  float3 Tr = exp(-thick * Absorb);\n"
			"  return float4(C * Tr * (1.0 - F), 1.0 - dot(Tr, float3(0.3333, 0.3334, 0.3333)));\n"
			"}\n"
			"// Vue de dessous : au-dela de ~49 degres, reflexion totale (on voit l'eau elle-meme)\n"
			"float s2 = 1.7689 * (1.0 - ndv * ndv);\n"
			"float F = s2 >= 1.0 ? 1.0 : 0.02 + 0.98 * pow(1.0 - sqrt(1.0 - s2), 5.0);\n"
			"return float4(C * (1.0 - F), F);\n");
		return Code;
	}

	const FString& PuddlesHLSL()
	{
		// Entrees : WP, N (normale du sommet), T, Amount (flaques), Wet (humidite), Tex (bruit).
		// Sortie : float4(flaque, sol mouille, pente XY des ronds de gouttes)
		static const FString Code = TEXT(
			"// Flaques et sol mouille (v4.1). Entrees : WP (position monde, cm), N (normale du sommet), T (temps, s),\n"
			"// Amount (part du sol couverte de flaques, 0..1), Wet (humidite generale, 0..1), Tex (bruit : T_Grime).\n"
			"// Sortie : float4(flaque 0..1, sol mouille 0..1, pente XY des ronds de gouttes dans les flaques).\n"
			"if (Amount <= 0.0 && Wet <= 0.0) return float4(0.0, 0.0, 0.0, 0.0);\n"
			"float up = saturate((N.z - 0.6) * 4.0);\n"
			"float2 p = WP.xy;\n"
			"// T_Grime est importee en sRGB : on revient aux valeurs du fichier (seuils calibres dessus)\n"
			"float n1 = pow(Texture2DSample(Tex, TexSampler, p / 1150.0).r, 0.4545);\n"
			"float n2 = pow(Texture2DSample(Tex, TexSampler, p / 460.0 + 0.37).g, 0.4545);\n"
			"float n = n1 * 0.82 + n2 * 0.18;\n"
			"// Plus Amount est grand, plus le seuil baisse : 0,2 -> ~8 % du sol, 0,55 -> ~26 %, 1 -> ~57 % (bruit de T_Grime)\n"
			"float th = lerp(0.94, 0.76, saturate(Amount));\n"
			"float puddle = Amount > 0.0 ? saturate((n - th) / 0.01) * up : 0.0;\n"
			"float wet = saturate(saturate((n - th + 0.035) / 0.035) * 0.9 * saturate(Amount * 4.0) + Wet) * up;\n"
			"wet = max(wet, puddle);\n"
			"// Gouttes qui tombent du plafond : un rond qui s'elargit par case de 70 cm, a un rythme propre a chaque case\n"
			"float2 ripple = float2(0.0, 0.0);\n"
			"if (puddle > 0.001)\n"
			"{\n"
			"  float2 cell = floor(p / 70.0);\n"
			"  for (int i = -1; i <= 1; i++)\n"
			"  {\n"
			"    for (int j = -1; j <= 1; j++)\n"
			"    {\n"
			"      float2 c = cell + float2(i, j);\n"
			"      float h = frac(sin(dot(c, float2(12.9898, 78.233))) * 43758.5453);\n"
			"      float h2 = frac(h * 91.7);\n"
			"      float2 center = (c + float2(h, h2)) * 70.0;\n"
			"      float period = 1.8 + h2 * 2.6;\n"
			"      float age = frac(T / period + h) * period;\n"
			"      float2 d = p - center;\n"
			"      float r = length(d);\n"
			"      float ring = exp(-pow((r - age * 34.0) / 3.5, 2.0)) * exp(-age * 1.7);\n"
			"      ripple += (d / max(r, 0.01)) * ring * 0.45;\n"
			"    }\n"
			"  }\n"
			"}\n"
			"return float4(puddle, wet, ripple * puddle);\n");
		return Code;
	}

	const FString& CausticsHLSL()
	{
		// Entrees : WP (position monde), Wz (poids des faces horizontales), T (temps), Amount (reglage de la surface),
		// CausTex (T_Caustics), SimTex / SimWin (vagues simulees). Sortie : multiplicateur de la couleur.
		static const FString Code = TEXT(
			"// Reseau lumineux qui derive lentement (deux echelles, legerement ondulees) ; les vagues simulees autour\n"
			"// du joueur le deforment d'autant plus que l'eau est profonde au-dessus du carrelage\n"
			"float sz = max(SimWin.z, 1.0);\n"
			"float2 sd = abs(WP.xy - SimWin.xy) / (0.5 * sz);\n"
			"float sf = SimWin.w * saturate((1.0 - max(sd.x, sd.y)) / 0.15);\n"
			"float2 slope = Texture2DSampleLevel(SimTex, SimTexSampler, WP.xy / sz, 0.0).rg * sf;\n"
			"float depth = max(45.0 - WP.z, 0.0);\n"
			"float2 uv = lerp(float2(WP.x + WP.y, WP.z), WP.xy, Wz) + slope * (20.0 + depth) * 2.0;\n"
			"float2 wob = float2(sin(uv.y * 0.019 + T * 0.7) + sin(uv.x * 0.013 - T * 0.45), cos(uv.x * 0.017 + T * 0.6) + cos(uv.y * 0.011 - T * 0.5));\n"
			"float k1 = Texture2DSample(CausTex, CausTexSampler, uv / 260.0 + T * float2(0.011, 0.006) + wob * 0.018).r;\n"
			"float k2 = Texture2DSample(CausTex, CausTexSampler, uv * float2(-1.0, 1.0) / 430.0 + T * float2(-0.007, 0.01) - wob * 0.012).r;\n"
			"float k = saturate(k1 + 0.45 * k2);\n"
			"// Sous l'eau : partout ; au-dessus : reflets plus faibles qui s'eteignent 2 m au-dessus de la surface\n"
			"float above = WP.z - 45.0;\n"
			"float fade = lerp(above > 0.0 ? 0.75 * saturate(1.0 - above / 200.0) : saturate(1.0 + above / 300.0), 1.0, Wz);\n"
			"return 1.0 + Amount * fade * (k * 1.5 - 0.27);\n");
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
		const TCHAR* Names[] = { TEXT("M_BR_World_Runtime"), TEXT("M_BR_Mesh_Runtime"), TEXT("M_BR_Skin_Runtime"), TEXT("M_BR_WaterSurface_Runtime") };
		UMaterial* M = NewObject<UMaterial>(Outer ? Outer : GetTransientPackage(), FName(Names[static_cast<int32>(Which)]), RF_Transient);
		M->MaterialDomain = MD_Surface;
		M->BlendMode = BLEND_Opaque;
		M->SetUsageByFlag(MATUSAGE_InstancedStaticMeshes, true);
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
		case EBRMasterMaterial::WaterSurface:
			M->BlendMode = BLEND_Translucent;
			M->TwoSided = true;
			M->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
			BuildWaterTranslucent(G, Out);
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
