#include "BRAssets.h"
#include "Backrooms.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/MeshComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Misc/PackageName.h"

namespace
{
	const TCHAR* MeshFolder = TEXT("/Game/Backrooms/Meshes");
	const TCHAR* TexFolder = TEXT("/Game/Backrooms/Textures");
	const TCHAR* SoundFolder = TEXT("/Game/Backrooms/Sounds");
	const TCHAR* MatFolder = TEXT("/Game/Backrooms/Materials");

	struct FSlotStyle
	{
		const TCHAR* Key;
		const TCHAR* Tex;
		FLinearColor Color;
		float Rough;
		float Metal;
		float TexScale;
	};

	// Style de chaque nom de materiau utilise par les modeles Blender (generate_models.py)
	const TArray<FSlotStyle>& SlotStyles()
	{
		static TArray<FSlotStyle> Styles = []()
		{
			TArray<FSlotStyle> S = {
				{ TEXT("Frame"), TEXT("T_Grime"), FLinearColor(0.85f, 0.85f, 0.82f), 0.4f, 0.3f, 1.f },
				{ TEXT("Trim"), TEXT("T_Grime"), FLinearColor(0.88f, 0.88f, 0.86f), 0.5f, 0.f, 1.f },
				{ TEXT("Metal"), TEXT("T_Grime"), FLinearColor(0.5f, 0.5f, 0.52f), 0.45f, 0.85f, 1.f },
				{ TEXT("DarkMetal"), TEXT("T_Grime"), FLinearColor(0.09f, 0.09f, 0.1f), 0.5f, 0.6f, 1.f },
				{ TEXT("Chrome"), TEXT("T_Grime"), FLinearColor(0.85f, 0.85f, 0.87f), 0.15f, 1.f, 1.f },
				{ TEXT("Rust"), TEXT("T_MetalPanel"), FLinearColor(0.9f, 0.7f, 0.6f), 0.8f, 0.3f, 1.f },
				{ TEXT("Wood"), TEXT("T_Wood"), FLinearColor(1.f, 1.f, 1.f), 0.6f, 0.f, 1.f },
				{ TEXT("Laminate"), TEXT("T_OfficeWall"), FLinearColor(0.85f, 0.8f, 0.7f), 0.55f, 0.f, 1.f },
				{ TEXT("Plastic"), TEXT("T_Grime"), FLinearColor(0.82f, 0.82f, 0.8f), 0.45f, 0.f, 1.f },
				{ TEXT("BlackPlastic"), TEXT("T_Grime"), FLinearColor(0.04f, 0.04f, 0.04f), 0.5f, 0.f, 1.f },
				{ TEXT("Fabric"), TEXT("T_OfficeCarpet"), FLinearColor(0.9f, 0.9f, 0.95f), 0.95f, 0.f, 2.f },
				{ TEXT("Cardboard"), TEXT("T_Grime"), FLinearColor(0.55f, 0.4f, 0.24f), 0.9f, 0.f, 1.f },
				{ TEXT("Tape"), TEXT("T_Grime"), FLinearColor(0.7f, 0.62f, 0.45f), 0.4f, 0.f, 1.f },
				{ TEXT("Paper"), TEXT("T_Paper"), FLinearColor(1.f, 1.f, 1.f), 0.9f, 0.f, 4.f },
				{ TEXT("Bottle"), TEXT("T_Grime"), FLinearColor(0.85f, 0.88f, 0.9f), 0.15f, 0.f, 1.f },
				{ TEXT("Label"), TEXT("T_Paper"), FLinearColor(0.95f, 0.75f, 0.5f), 0.6f, 0.f, 8.f },
				{ TEXT("Cap"), TEXT("T_Grime"), FLinearColor(0.15f, 0.3f, 0.7f), 0.4f, 0.f, 1.f },
				{ TEXT("Battery"), TEXT("T_Grime"), FLinearColor(0.06f, 0.06f, 0.06f), 0.35f, 0.2f, 1.f },
				{ TEXT("Copper"), TEXT("T_Grime"), FLinearColor(0.85f, 0.5f, 0.25f), 0.3f, 1.f, 1.f },
				{ TEXT("Rubber"), TEXT("T_Grime"), FLinearColor(0.05f, 0.05f, 0.05f), 0.85f, 0.f, 1.f },
				{ TEXT("Sign"), TEXT("T_Grime"), FLinearColor(0.9f, 0.75f, 0.1f), 0.5f, 0.f, 1.f },
				{ TEXT("Glass"), TEXT("T_Grime"), FLinearColor(0.35f, 0.5f, 0.65f), 0.05f, 0.3f, 1.f },
				{ TEXT("Brass"), TEXT("T_Grime"), FLinearColor(0.85f, 0.65f, 0.3f), 0.3f, 1.f, 1.f },
				{ TEXT("Barn"), TEXT("T_Wood"), FLinearColor(1.6f, 0.45f, 0.35f), 0.85f, 0.f, 0.5f },
				{ TEXT("Roof"), TEXT("T_Asphalt"), FLinearColor(1.f, 1.f, 1.f), 0.9f, 0.f, 0.5f },
				{ TEXT("Siding"), TEXT("T_Siding"), FLinearColor(1.f, 1.f, 1.f), 0.7f, 0.f, 0.5f },
				{ TEXT("Door"), TEXT("T_Wood"), FLinearColor(1.f, 1.f, 1.f), 0.6f, 0.f, 1.f },
				{ TEXT("Window"), TEXT("T_Grime"), FLinearColor(0.04f, 0.05f, 0.07f), 0.05f, 0.5f, 1.f },
				{ TEXT("Brick"), TEXT("T_Brick"), FLinearColor(1.f, 1.f, 1.f), 0.9f, 0.f, 0.5f },
				{ TEXT("Concrete"), TEXT("T_Concrete"), FLinearColor(1.f, 1.f, 1.f), 0.9f, 0.f, 0.5f },
				{ TEXT("Ceramic"), TEXT("T_Grime"), FLinearColor(0.9f, 0.9f, 0.85f), 0.3f, 0.f, 1.f },
				{ TEXT("Stalk"), TEXT("T_Grime"), FLinearColor(0.78f, 0.64f, 0.32f), 0.8f, 0.f, 1.f },
				{ TEXT("Grain"), TEXT("T_Grime"), FLinearColor(0.85f, 0.66f, 0.26f), 0.7f, 0.f, 1.f },
				{ TEXT("Rock"), TEXT("T_Rock"), FLinearColor(1.f, 1.f, 1.f), 0.9f, 0.f, 0.6f },
				{ TEXT("Body"), TEXT("T_Grime"), FLinearColor(0.008f, 0.008f, 0.008f), 0.95f, 0.f, 1.f },
				{ TEXT("Skin"), TEXT("T_Skin"), FLinearColor(0.8f, 0.72f, 0.66f), 0.6f, 0.f, 2.f },
				{ TEXT("Hair"), TEXT("T_Grime"), FLinearColor(0.01f, 0.01f, 0.01f), 0.5f, 0.f, 1.f },
				{ TEXT("Cloth"), TEXT("T_OfficeCarpet"), FLinearColor(0.55f, 0.57f, 0.6f), 0.95f, 0.f, 2.f },
				{ TEXT("Pants"), TEXT("T_OfficeCarpet"), FLinearColor(0.3f, 0.35f, 0.45f), 0.95f, 0.f, 2.f },
				{ TEXT("Dark"), TEXT("T_Grime"), FLinearColor(0.005f, 0.005f, 0.005f), 1.f, 0.f, 1.f },
				{ TEXT("Flesh"), TEXT("T_Skin"), FLinearColor(0.75f, 0.5f, 0.46f), 0.5f, 0.f, 2.f },
				{ TEXT("Fur"), TEXT("T_Skin"), FLinearColor(0.42f, 0.33f, 0.24f), 0.95f, 0.f, 3.f },
				{ TEXT("Wing"), TEXT("T_Skin"), FLinearColor(0.5f, 0.43f, 0.36f), 0.9f, 0.f, 2.f },
				{ TEXT("Pattern"), TEXT("T_Grime"), FLinearColor(0.12f, 0.08f, 0.06f), 0.9f, 0.f, 1.f },
				{ TEXT("Eye"), TEXT("T_Grime"), FLinearColor(0.01f, 0.01f, 0.01f), 0.1f, 0.f, 1.f },
				{ TEXT("Party"), TEXT("T_Grime"), FLinearColor(1.f, 0.86f, 0.1f), 0.5f, 0.f, 1.f },
				{ TEXT("Face"), TEXT("T_Grime"), FLinearColor(0.01f, 0.01f, 0.01f), 0.6f, 0.f, 1.f },
				{ TEXT("Shade"), TEXT("T_Paper"), FLinearColor(1.f, 0.85f, 0.6f), 0.9f, 0.f, 2.f },
				{ TEXT("Teeth"), TEXT("T_Grime"), FLinearColor(0.9f, 0.88f, 0.8f), 0.4f, 0.f, 1.f },
			};
			// Les cles les plus longues d'abord ("DarkMetal" avant "Metal")
			S.Sort([](const FSlotStyle& A, const FSlotStyle& B) { return FCString::Strlen(A.Key) > FCString::Strlen(B.Key); });
			return S;
		}();
		return Styles;
	}

	const FSlotStyle* FindSlotStyle(const FString& Slot)
	{
		for (const FSlotStyle& S : SlotStyles())
		{
			if (Slot.Equals(S.Key, ESearchCase::IgnoreCase))
			{
				return &S;
			}
		}
		for (const FSlotStyle& S : SlotStyles())
		{
			if (Slot.Contains(S.Key, ESearchCase::IgnoreCase))
			{
				return &S;
			}
		}
		return nullptr;
	}
}

UBRAssets* UBRAssets::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UBRAssets>() : nullptr;
}

UObject* UBRAssets::LoadAsset(const TCHAR* Folder, FName Name, UClass* Class)
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (TObjectPtr<UObject>* Found = Loaded.Find(Name))
	{
		return Found->Get();
	}
	if (Missing.Contains(Name))
	{
		return nullptr;
	}

	const FString N = Name.ToString();
	const FString PackageName = FString(Folder) + TEXT("/") + N;
	UObject* Obj = nullptr;
	if (FPackageName::DoesPackageExist(PackageName))
	{
		Obj = StaticLoadObject(Class, nullptr, *(PackageName + TEXT(".") + N), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	if (Obj)
	{
		Loaded.Add(Name, Obj);
	}
	else
	{
		Missing.Add(Name);
		UE_LOG(LogBackrooms, Verbose, TEXT("Ressource absente : %s (repli utilise)"), *PackageName);
	}
	return Obj;
}

UStaticMesh* UBRAssets::Mesh(FName Name)
{
	return Cast<UStaticMesh>(LoadAsset(MeshFolder, Name, UStaticMesh::StaticClass()));
}

UStaticMesh* UBRAssets::Cube()
{
	if (!CubeMesh)
	{
		CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	return CubeMesh;
}

UStaticMesh* UBRAssets::Sphere()
{
	if (!SphereMesh)
	{
		SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}
	return SphereMesh;
}

UStaticMesh* UBRAssets::Cylinder()
{
	if (!CylinderMesh)
	{
		CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}
	return CylinderMesh;
}

UStaticMesh* UBRAssets::Plane()
{
	if (!PlaneMesh)
	{
		PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}
	return PlaneMesh;
}

UTexture* UBRAssets::Texture(FName Name)
{
	return Cast<UTexture>(LoadAsset(TexFolder, Name, UTexture::StaticClass()));
}

USoundBase* UBRAssets::Sound(FName Name)
{
	return Cast<USoundBase>(LoadAsset(SoundFolder, Name, USoundBase::StaticClass()));
}

USoundAttenuation* UBRAssets::Attenuation(float FalloffDistance)
{
	const int32 Key = FMath::RoundToInt(FalloffDistance / 100.f);
	if (TObjectPtr<USoundAttenuation>* Found = AttCache.Find(Key))
	{
		return Found->Get();
	}
	USoundAttenuation* Att = NewObject<USoundAttenuation>(this);
	Att->Attenuation.bAttenuate = true;
	Att->Attenuation.bSpatialize = true;
	Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Att->Attenuation.AttenuationShapeExtents = FVector(FMath::Max(50.f, FalloffDistance * 0.08f), 0.f, 0.f);
	Att->Attenuation.FalloffDistance = FalloffDistance;
	Att->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	AttCache.Add(Key, Att);
	return Att;
}

UMaterialInterface* UBRAssets::WorldParent()
{
	return Cast<UMaterialInterface>(LoadAsset(MatFolder, TEXT("M_BR_World"), UMaterialInterface::StaticClass()));
}

UMaterialInterface* UBRAssets::MeshParent()
{
	return Cast<UMaterialInterface>(LoadAsset(MatFolder, TEXT("M_BR_Mesh"), UMaterialInterface::StaticClass()));
}

UMaterialInterface* UBRAssets::WaterParent()
{
	return Cast<UMaterialInterface>(LoadAsset(MatFolder, TEXT("M_BR_Water"), UMaterialInterface::StaticClass()));
}

UMaterialInterface* UBRAssets::FallbackParent()
{
	if (!FallbackMat)
	{
		FallbackMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	return FallbackMat;
}

bool UBRAssets::HasContent()
{
	return WorldParent() != nullptr;
}

FLinearColor UBRAssets::TextureAverage(FName Texture)
{
	static const TMap<FName, FLinearColor> Avg = {
		{ TEXT("T_L0_Wallpaper"), FLinearColor(0.78f, 0.7f, 0.42f) },
		{ TEXT("T_L0_Carpet"), FLinearColor(0.5f, 0.42f, 0.25f) },
		{ TEXT("T_L0_Ceiling"), FLinearColor(0.85f, 0.83f, 0.75f) },
		{ TEXT("T_Concrete"), FLinearColor(0.5f, 0.5f, 0.48f) },
		{ TEXT("T_ConcreteFloor"), FLinearColor(0.38f, 0.37f, 0.35f) },
		{ TEXT("T_ConcreteDark"), FLinearColor(0.2f, 0.2f, 0.21f) },
		{ TEXT("T_Brick"), FLinearColor(0.32f, 0.2f, 0.15f) },
		{ TEXT("T_MetalPanel"), FLinearColor(0.33f, 0.35f, 0.3f) },
		{ TEXT("T_OfficeCarpet"), FLinearColor(0.3f, 0.34f, 0.4f) },
		{ TEXT("T_OfficeWall"), FLinearColor(0.8f, 0.79f, 0.74f) },
		{ TEXT("T_HotelCarpet"), FLinearColor(0.4f, 0.12f, 0.08f) },
		{ TEXT("T_HotelWallpaper"), FLinearColor(0.5f, 0.49f, 0.35f) },
		{ TEXT("T_Wood"), FLinearColor(0.28f, 0.16f, 0.08f) },
		{ TEXT("T_PoolTile"), FLinearColor(0.88f, 0.91f, 0.92f) },
		{ TEXT("T_Rock"), FLinearColor(0.33f, 0.29f, 0.25f) },
		{ TEXT("T_Dirt"), FLinearColor(0.36f, 0.28f, 0.19f) },
		{ TEXT("T_Asphalt"), FLinearColor(0.17f, 0.17f, 0.18f) },
		{ TEXT("T_Grass"), FLinearColor(0.2f, 0.3f, 0.11f) },
		{ TEXT("T_Facade"), FLinearColor(0.45f, 0.44f, 0.43f) },
		{ TEXT("T_Siding"), FLinearColor(0.68f, 0.72f, 0.74f) },
		{ TEXT("T_Glitch"), FLinearColor(0.5f, 0.4f, 0.6f) },
		{ TEXT("T_Paper"), FLinearColor(0.88f, 0.85f, 0.74f) },
		{ TEXT("T_WaterNormal"), FLinearColor(0.3f, 0.6f, 0.7f) },
	};
	if (const FLinearColor* Found = Avg.Find(Texture))
	{
		return *Found;
	}
	return FLinearColor(0.8f, 0.8f, 0.8f);
}

UMaterialInterface* UBRAssets::Surface(const FBRSurface& S)
{
	const FString Key = TEXT("W|") + S.Key();
	if (TObjectPtr<UMaterialInterface>* Found = MatCache.Find(Key))
	{
		return Found->Get();
	}

	UMaterialInterface* Parent = WorldParent();
	const bool bCustom = Parent != nullptr;
	if (!Parent)
	{
		Parent = FallbackParent();
	}
	if (!Parent)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	if (bCustom)
	{
		if (UTexture* Tex = Texture(S.Texture))
		{
			MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
		}
		MID->SetVectorParameterValue(TEXT("Tint"), S.Tint);
		MID->SetScalarParameterValue(TEXT("TexScale"), S.Scale);
		MID->SetScalarParameterValue(TEXT("Roughness"), S.Roughness);
		MID->SetScalarParameterValue(TEXT("Metallic"), S.Metallic);
		MID->SetScalarParameterValue(TEXT("Grime"), S.Grime);
		MID->SetScalarParameterValue(TEXT("SelfIllum"), S.SelfIllum);
		MID->SetVectorParameterValue(TEXT("Emissive"), S.Emissive);
	}
	else
	{
		MID->SetVectorParameterValue(TEXT("Color"), TextureAverage(S.Texture) * S.Tint);
	}
	MatCache.Add(Key, MID);
	return MID;
}

UMaterialInterface* UBRAssets::WaterMaterial(const FBRSurface& S)
{
	const FString Key = TEXT("Water|") + S.Key();
	if (TObjectPtr<UMaterialInterface>* Found = MatCache.Find(Key))
	{
		return Found->Get();
	}
	UMaterialInterface* Parent = WaterParent();
	const bool bCustom = Parent != nullptr;
	if (!Parent)
	{
		Parent = FallbackParent();
	}
	if (!Parent)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	if (bCustom)
	{
		MID->SetVectorParameterValue(TEXT("Tint"), S.Tint);
		MID->SetScalarParameterValue(TEXT("TexScale"), S.Scale);
		MID->SetScalarParameterValue(TEXT("Roughness"), S.Roughness);
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.35f);
	}
	else
	{
		MID->SetVectorParameterValue(TEXT("Color"), S.Tint);
	}
	MatCache.Add(Key, MID);
	return MID;
}

FLinearColor UBRAssets::GlowColorForSlot(const FString& SlotName)
{
	if (!SlotName.Contains(TEXT("Glow"), ESearchCase::IgnoreCase))
	{
		return FLinearColor::Black;
	}
	if (SlotName.Contains(TEXT("GlowWindow"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(1.f, 0.75f, 0.42f) * 12.f;
	}
	if (SlotName.Contains(TEXT("GlowGreen"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.1f, 1.f, 0.25f) * 40.f;
	}
	if (SlotName.Contains(TEXT("GlowRed"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(1.f, 0.08f, 0.04f) * 40.f;
	}
	if (SlotName.Contains(TEXT("GlowWarm"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(1.f, 0.7f, 0.38f) * 90.f;
	}
	return FLinearColor(1.f, 0.96f, 0.86f) * 120.f;
}

UMaterialInstanceDynamic* UBRAssets::NewGlow(UObject* Outer, const FLinearColor& Color, float Strength)
{
	UMaterialInterface* Parent = MeshParent();
	const bool bCustom = Parent != nullptr;
	if (!Parent)
	{
		Parent = FallbackParent();
	}
	if (!Parent)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Outer ? Outer : this);
	if (bCustom)
	{
		if (UTexture* Tex = Texture(TEXT("T_Grime")))
		{
			MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
		}
		MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Color.R, Color.G, Color.B, 1.f));
		MID->SetScalarParameterValue(TEXT("Roughness"), 0.4f);
		MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		MID->SetVectorParameterValue(TEXT("Emissive"), Color * Strength);
	}
	else
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return MID;
}

UMaterialInterface* UBRAssets::SlotMaterial(const FString& SlotName, const FLinearColor* TintOverride)
{
	const FString Key = FString::Printf(TEXT("S|%s|%s"), *SlotName, TintOverride ? *TintOverride->ToString() : TEXT("-"));
	if (TObjectPtr<UMaterialInterface>* Found = MatCache.Find(Key))
	{
		return Found->Get();
	}

	const FLinearColor Glow = GlowColorForSlot(SlotName);
	UMaterialInterface* Result = nullptr;
	if (!Glow.IsAlmostBlack())
	{
		const float Strength = FMath::Max3(Glow.R, Glow.G, Glow.B);
		Result = NewGlow(this, Glow / FMath::Max(Strength, 0.001f), Strength);
	}
	else
	{
		const FSlotStyle* Style = FindSlotStyle(SlotName);
		const FLinearColor Color = TintOverride ? *TintOverride : (Style ? Style->Color : FLinearColor(0.6f, 0.6f, 0.6f));
		UMaterialInterface* Parent = MeshParent();
		const bool bCustom = Parent != nullptr;
		if (!Parent)
		{
			Parent = FallbackParent();
		}
		if (Parent)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
			if (bCustom)
			{
				if (UTexture* Tex = Texture(Style ? FName(Style->Tex) : FName(TEXT("T_Grime"))))
				{
					MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
				}
				MID->SetVectorParameterValue(TEXT("Tint"), Color);
				MID->SetScalarParameterValue(TEXT("TexScale"), Style ? Style->TexScale : 1.f);
				MID->SetScalarParameterValue(TEXT("Roughness"), Style ? Style->Rough : 0.7f);
				MID->SetScalarParameterValue(TEXT("Metallic"), Style ? Style->Metal : 0.f);
				MID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor::Black);
			}
			else
			{
				FLinearColor C = Color;
				if (Style && FCString::Strcmp(Style->Tex, TEXT("T_Grime")) != 0)
				{
					C = C * TextureAverage(FName(Style->Tex));
				}
				MID->SetVectorParameterValue(TEXT("Color"), C);
			}
			Result = MID;
		}
	}

	if (Result)
	{
		MatCache.Add(Key, Result);
	}
	return Result;
}

void UBRAssets::ApplySlots(UMeshComponent* Comp, const TMap<FString, FLinearColor>* TintOverrides, bool bUniqueGlow,
	TArray<UMaterialInstanceDynamic*>* OutGlow, float GlowScale)
{
	if (!Comp)
	{
		return;
	}
	const TArray<FName> Names = Comp->GetMaterialSlotNames();
	const int32 Num = Comp->GetNumMaterials();
	for (int32 i = 0; i < Num; ++i)
	{
		const FString Slot = Names.IsValidIndex(i) ? Names[i].ToString() : FString(TEXT("Body"));
		const FLinearColor Glow = GlowColorForSlot(Slot);
		UMaterialInterface* Mat = nullptr;

		if (!Glow.IsAlmostBlack() && (bUniqueGlow || !FMath::IsNearlyEqual(GlowScale, 1.f)))
		{
			const float Strength = FMath::Max3(Glow.R, Glow.G, Glow.B);
			UMaterialInstanceDynamic* MID = NewGlow(Comp, Glow / FMath::Max(Strength, 0.001f), Strength * GlowScale);
			if (OutGlow && MID)
			{
				OutGlow->Add(MID);
			}
			Mat = MID;
		}
		else
		{
			const FLinearColor* Override = nullptr;
			if (TintOverrides)
			{
				for (const TPair<FString, FLinearColor>& Pair : *TintOverrides)
				{
					if (Slot.Contains(Pair.Key, ESearchCase::IgnoreCase))
					{
						Override = &Pair.Value;
						break;
					}
				}
			}
			Mat = SlotMaterial(Slot, Override);
		}

		if (Mat)
		{
			Comp->SetMaterial(i, Mat);
		}
	}
}
