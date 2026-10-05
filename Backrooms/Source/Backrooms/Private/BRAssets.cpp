#include "BRAssets.h"
#include "Backrooms.h"
#include "BRMaterialBuilder.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Components/SkinnedMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/MeshComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
	const TCHAR* MeshFolder = TEXT("/Game/Backrooms/Meshes");
	const TCHAR* TexFolder = TEXT("/Game/Backrooms/Textures");
	const TCHAR* UIFolder = TEXT("/Game/Backrooms/UI");
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
		/** v4.5 : normal map propre (modele cuit) ; sinon T_Skin_N pour la peau */
		const TCHAR* Normal = nullptr;
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
				// v2 : entites articulees, objets d'inventaire, combinaison
				{ TEXT("Wire"), TEXT("T_Grime"), FLinearColor(0.07f, 0.065f, 0.06f), 0.45f, 0.8f, 1.f },
				{ TEXT("Mouth"), TEXT("T_Grime"), FLinearColor(0.08f, 0.01f, 0.01f), 0.3f, 0.f, 1.f },
				{ TEXT("Visor"), TEXT("T_Grime"), FLinearColor(0.02f, 0.025f, 0.03f), 0.05f, 0.6f, 1.f },
				{ TEXT("Hazmat"), TEXT("T_Grime"), FLinearColor(0.75f, 0.6f, 0.08f), 0.6f, 0.f, 2.f },
				{ TEXT("Claw"), TEXT("T_Grime"), FLinearColor(0.12f, 0.1f, 0.08f), 0.35f, 0.f, 1.f },
				{ TEXT("Sucker"), TEXT("T_Grime"), FLinearColor(0.95f, 0.7f, 0.6f), 0.5f, 0.f, 1.f },
				{ TEXT("Shoe"), TEXT("T_Grime"), FLinearColor(0.08f, 0.06f, 0.05f), 0.7f, 0.f, 1.f },
				{ TEXT("FleshGlass"), TEXT("T_Skin"), FLinearColor(0.85f, 0.55f, 0.5f), 0.25f, 0.f, 2.f },
				{ TEXT("Vein"), TEXT("T_Skin"), FLinearColor(0.35f, 0.05f, 0.06f), 0.4f, 0.f, 2.f },
				{ TEXT("Balloon"), TEXT("T_Grime"), FLinearColor(0.8f, 0.04f, 0.03f), 0.22f, 0.f, 1.f },
				{ TEXT("String"), TEXT("T_Grime"), FLinearColor(0.9f, 0.9f, 0.9f), 0.8f, 0.f, 1.f },
				{ TEXT("Lens"), TEXT("T_Grime"), FLinearColor(0.02f, 0.02f, 0.025f), 0.05f, 0.5f, 1.f },
				{ TEXT("Strap"), TEXT("T_OfficeCarpet"), FLinearColor(0.08f, 0.08f, 0.09f), 0.9f, 0.f, 3.f },
				{ TEXT("Reel"), TEXT("T_Grime"), FLinearColor(0.85f, 0.85f, 0.85f), 0.4f, 0.f, 1.f },
				{ TEXT("Gauze"), TEXT("T_Paper"), FLinearColor(0.95f, 0.93f, 0.88f), 0.95f, 0.f, 6.f },
				{ TEXT("Wrapper"), TEXT("T_Grime"), FLinearColor(0.75f, 0.2f, 0.08f), 0.35f, 0.4f, 1.f },
				{ TEXT("Vest"), TEXT("T_OfficeCarpet"), FLinearColor(0.22f, 0.25f, 0.18f), 0.9f, 0.f, 3.f },
				{ TEXT("Reflective"), TEXT("T_Grime"), FLinearColor(0.85f, 0.85f, 0.8f), 0.2f, 0.6f, 1.f },
				{ TEXT("Glove"), TEXT("T_Skin"), FLinearColor(0.7f, 0.55f, 0.08f), 0.5f, 0.f, 3.f },
				// v3 : modeles fournis (textures UV d'origine)
				{ TEXT("HazmatSuit"), TEXT("T_Hazmat_Suit"), FLinearColor(1.f, 1.f, 1.f), 0.62f, 0.f, 1.f },
				{ TEXT("HazmatMask"), TEXT("T_Hazmat_Mask"), FLinearColor(1.f, 1.f, 1.f), 0.45f, 0.1f, 1.f },
				{ TEXT("HazmatGlass"), TEXT("T_Grime"), FLinearColor(0.015f, 0.02f, 0.025f), 0.04f, 0.7f, 1.f },
				{ TEXT("MothTex"), TEXT("T_Deathmoth"), FLinearColor(1.f, 1.f, 1.f), 0.78f, 0.f, 1.f },
				{ TEXT("BacteriaSkin"), TEXT("T_Grime"), FLinearColor(0.006f, 0.006f, 0.008f), 0.22f, 0.f, 1.f },
				// v3.5 : Skin-Stealer, Faceling, Partygoer, Hound fournis ; Smiler et Clump refaits d'apres les images
				{ TEXT("SkinStealerFlesh"), TEXT("T_SkinStealer_Flesh"), FLinearColor(1.f, 1.f, 1.f), 0.3f, 0.f, 1.f },
				{ TEXT("StealerClaw"), TEXT("T_SkinStealer_Claw"), FLinearColor(1.f, 1.f, 1.f), 0.35f, 0.f, 1.f },
				{ TEXT("StealerEye"), TEXT("T_SkinStealer_Eye"), FLinearColor(1.f, 1.f, 1.f), 0.08f, 0.f, 1.f },
				{ TEXT("FacelingTex"), TEXT("T_Faceling"), FLinearColor(1.f, 1.f, 1.f), 0.8f, 0.f, 1.f },
				{ TEXT("PartygoerTex"), TEXT("T_Partygoer"), FLinearColor(1.f, 1.f, 1.f), 0.5f, 0.f, 1.f },
				{ TEXT("HoundSkin"), TEXT("T_Hound"), FLinearColor(1.f, 1.f, 1.f), 0.7f, 0.f, 1.f },
				{ TEXT("HoundHair"), TEXT("T_Grime"), FLinearColor(0.012f, 0.011f, 0.011f), 0.35f, 0.f, 1.f },
				{ TEXT("HoundFace"), TEXT("T_Skin"), FLinearColor(0.42f, 0.32f, 0.28f), 0.6f, 0.f, 2.f },
				{ TEXT("HoundTongue"), TEXT("T_Skin"), FLinearColor(0.75f, 0.28f, 0.35f), 0.3f, 0.f, 2.f },
				{ TEXT("ClumpFlesh"), TEXT("T_Skin"), FLinearColor(0.86f, 0.7f, 0.6f), 0.45f, 0.f, 2.f },
				{ TEXT("ClumpMouth"), TEXT("T_Grime"), FLinearColor(0.12f, 0.015f, 0.015f), 0.25f, 0.f, 1.f },
				{ TEXT("ClumpTeeth"), TEXT("T_Grime"), FLinearColor(0.72f, 0.58f, 0.32f), 0.4f, 0.f, 1.f },
				// v4.5 : Wretch et Clump reconstruits (Tools/Blender/build_creatures.py) : couleur et relief cuits
				{ TEXT("WretchSkin"), TEXT("T_Wretch"), FLinearColor(1.f, 1.f, 1.f), 0.55f, 0.f, 1.f, TEXT("T_Wretch_N") },
				{ TEXT("WretchTeeth"), TEXT("T_Grime"), FLinearColor(0.95f, 0.86f, 0.62f), 0.38f, 0.f, 1.f },
				{ TEXT("WretchEye"), TEXT("T_Grime"), FLinearColor(0.7f, 0.72f, 0.66f), 0.12f, 0.f, 1.f },
				{ TEXT("ClumpSkin"), TEXT("T_Clump"), FLinearColor(1.f, 1.f, 1.f), 0.45f, 0.f, 1.f, TEXT("T_Clump_N") },
				// v3.8 : poste de travail du Niveau 4 (scene fournie) ; T_Grime vaut ~0,59 en lineaire
				{ TEXT("DeskTop"), TEXT("T_Grime"), FLinearColor(1.05f, 1.f, 0.7f), 0.45f, 0.f, 1.f },
				{ TEXT("DeskChrome"), TEXT("T_Grime"), FLinearColor(1.2f, 1.2f, 1.22f), 0.22f, 1.f, 1.f },
				{ TEXT("DeskDark"), TEXT("T_Grime"), FLinearColor(0.04f, 0.04f, 0.04f), 0.4f, 0.f, 1.f },
				{ TEXT("PCBeige"), TEXT("T_Grime"), FLinearColor(1.1f, 0.92f, 0.62f), 0.55f, 0.f, 1.f },
				{ TEXT("PCScreen"), TEXT("T_Grime"), FLinearColor(0.015f, 0.02f, 0.02f), 0.06f, 0.f, 1.f },
				{ TEXT("PCDark"), TEXT("T_Grime"), FLinearColor(0.035f, 0.035f, 0.035f), 0.5f, 0.f, 1.f },
				{ TEXT("PCGrey"), TEXT("T_Grime"), FLinearColor(0.17f, 0.17f, 0.17f), 0.5f, 0.f, 1.f },
				{ TEXT("PCLight"), TEXT("T_Grime"), FLinearColor(0.8f, 0.8f, 0.8f), 0.5f, 0.f, 1.f },
				{ TEXT("ChairLeather"), TEXT("T_Grime"), FLinearColor(0.045f, 0.045f, 0.045f), 0.42f, 0.f, 1.f },
				{ TEXT("ChairBase"), TEXT("T_Grime"), FLinearColor(0.07f, 0.07f, 0.075f), 0.3f, 0.6f, 1.f },
				{ TEXT("CoolerBody"), TEXT("T_Grime"), FLinearColor(1.05f, 0.88f, 0.55f), 0.5f, 0.f, 1.f },
				{ TEXT("CoolerBottle"), TEXT("T_Grime"), FLinearColor(1.3f, 1.45f, 1.6f), 0.12f, 0.f, 1.f },
				{ TEXT("TapBlue"), TEXT("T_Grime"), FLinearColor(0.08f, 0.3f, 1.3f), 0.3f, 0.f, 1.f },
				{ TEXT("TapRed"), TEXT("T_Grime"), FLinearColor(1.3f, 0.08f, 0.06f), 0.3f, 0.f, 1.f },
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
	const FName Key(*(FString(Folder) + TEXT("/") + Name.ToString()));
	if (TObjectPtr<UObject>* Found = Loaded.Find(Key))
	{
		return Found->Get();
	}
	if (Missing.Contains(Key))
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
		Loaded.Add(Key, Obj);
	}
	else
	{
		Missing.Add(Key);
		UE_LOG(LogBackrooms, Verbose, TEXT("Ressource absente : %s (repli utilise)"), *PackageName);
	}
	return Obj;
}

UTexture2D* UBRAssets::LoadRawTexture(const TCHAR* SubFolder, FName Name, bool bLinear, bool bMips)
{
	const FString Dir = FPaths::Combine(FPaths::ProjectDir(), TEXT("RawAssets"), SubFolder);
	const TCHAR* Exts[] = { TEXT(".jpg"), TEXT(".png"), TEXT(".jpeg"), TEXT(".tga") };
	FString File;
	for (const TCHAR* Ext : Exts)
	{
		const FString Candidate = FPaths::Combine(Dir, Name.ToString() + Ext);
		if (FPaths::FileExists(Candidate))
		{
			File = Candidate;
			break;
		}
	}
	if (File.IsEmpty())
	{
		return nullptr;
	}

	FImage Source;
	if (!FImageUtils::LoadImage(*File, Source))
	{
		UE_LOG(LogBackrooms, Warning, TEXT("Image illisible : %s"), *File);
		return nullptr;
	}
	// Conversion en BGRA8 sans changer l'espace gamma : les octets d'une normal map restent intacts
	FImage Img;
	Source.CopyTo(Img, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	const int32 W = Img.SizeX;
	const int32 H = Img.SizeY;
	if (W <= 0 || H <= 0)
	{
		return nullptr;
	}

	UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), Name));
	if (!Tex || !Tex->GetPlatformData() || Tex->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}
	Tex->SRGB = !bLinear;
	Tex->AddressX = TA_Wrap;
	Tex->AddressY = TA_Wrap;
	Tex->LODGroup = bMips ? TEXTUREGROUP_World : TEXTUREGROUP_UI;
	Tex->NeverStream = true;

	FTexturePlatformData* PD = Tex->GetPlatformData();
	TArray<FColor> Level;
	Level.SetNumUninitialized(W * H);
	FMemory::Memcpy(Level.GetData(), Img.RawData.GetData(), static_cast<SIZE_T>(W) * H * 4);
	{
		FTexture2DMipMap& Mip0 = PD->Mips[0];
		void* Dest = Mip0.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Dest, Level.GetData(), static_cast<SIZE_T>(W) * H * 4);
		Mip0.BulkData.Unlock();
	}

	// Chaine de mipmaps (filtre boite) : evite le scintillement des sols et murs au loin
	int32 PW = W;
	int32 PH = H;
	while (bMips && (PW > 1 || PH > 1))
	{
		const int32 NW = FMath::Max(1, PW / 2);
		const int32 NH = FMath::Max(1, PH / 2);
		TArray<FColor> Next;
		Next.SetNumUninitialized(NW * NH);
		for (int32 Y = 0; Y < NH; ++Y)
		{
			const int32 Y0 = FMath::Min(Y * 2, PH - 1);
			const int32 Y1 = FMath::Min(Y * 2 + 1, PH - 1);
			for (int32 X = 0; X < NW; ++X)
			{
				const int32 X0 = FMath::Min(X * 2, PW - 1);
				const int32 X1 = FMath::Min(X * 2 + 1, PW - 1);
				const FColor& C00 = Level[Y0 * PW + X0];
				const FColor& C01 = Level[Y0 * PW + X1];
				const FColor& C10 = Level[Y1 * PW + X0];
				const FColor& C11 = Level[Y1 * PW + X1];
				Next[Y * NW + X] = FColor(
					static_cast<uint8>((C00.R + C01.R + C10.R + C11.R + 2) / 4),
					static_cast<uint8>((C00.G + C01.G + C10.G + C11.G + 2) / 4),
					static_cast<uint8>((C00.B + C01.B + C10.B + C11.B + 2) / 4),
					static_cast<uint8>((C00.A + C01.A + C10.A + C11.A + 2) / 4));
			}
		}
		FTexture2DMipMap* Mip = new FTexture2DMipMap();
		Mip->SizeX = NW;
		Mip->SizeY = NH;
		Mip->SizeZ = 1;
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		void* Dest = Mip->BulkData.Realloc(static_cast<int64>(NW) * NH * 4);
		FMemory::Memcpy(Dest, Next.GetData(), static_cast<SIZE_T>(NW) * NH * 4);
		Mip->BulkData.Unlock();
		PD->Mips.Add(Mip);
		Level = MoveTemp(Next);
		PW = NW;
		PH = NH;
	}
	Tex->UpdateResource();
	bRuntimeContent = true;
	UE_LOG(LogBackrooms, Log, TEXT("Texture chargee depuis RawAssets : %s"), *File);
	return Tex;
}

UStaticMesh* UBRAssets::Mesh(FName Name)
{
	return Cast<UStaticMesh>(LoadAsset(MeshFolder, Name, UStaticMesh::StaticClass()));
}

namespace
{
	TArray<FString>& FallbackList()
	{
		static TArray<FString> List;
		return List;
	}
}

void UBRAssets::ReportFallback(const FString& Model)
{
	TArray<FString>& L = FallbackList();
	if (!L.Contains(Model))
	{
		L.Add(Model);
		UE_LOG(LogTemp, Warning, TEXT("Backrooms : modele fourni absent, forme de secours utilisee : %s (relancer backrooms_setup.run() ; ")
			TEXT("fichiers dans RawAssets/Meshes et RawAssets/Skeletal, voir Tools/protect_assets.py check)"), *Model);
	}
}

const TArray<FString>& UBRAssets::GetFallbacks()
{
	return FallbackList();
}

USkeletalMesh* UBRAssets::SkeletalMesh(FName Name)
{
	return Cast<USkeletalMesh>(LoadAsset(MeshFolder, Name, USkeletalMesh::StaticClass()));
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
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (UTexture* T = Cast<UTexture>(LoadAsset(TexFolder, Name, UTexture::StaticClass())))
	{
		return T;
	}
	// Secours : lecture directe du fichier (normal maps en lineaire)
	const FName RawKey(*(TEXT("Raw/") + Name.ToString()));
	if (TObjectPtr<UObject>* Found = Loaded.Find(RawKey))
	{
		return Cast<UTexture>(Found->Get());
	}
	if (Missing.Contains(RawKey))
	{
		return nullptr;
	}
	const FString N = Name.ToString();
	// Normal maps et textures de donnees (bruits du materiau du monde) : valeurs lineaires
	// v4.7 : cartes de rugosite (<Texture>_R) : donnees, lineaires
	const bool bLinear = N.EndsWith(TEXT("_N")) || N.EndsWith(TEXT("_R")) || N.Contains(TEXT("Normal")) || N == TEXT("T_NoiseLF");
	UTexture2D* Raw = LoadRawTexture(TEXT("Textures"), Name, bLinear, true);
	if (Raw)
	{
		Loaded.Add(RawKey, Raw);
	}
	else
	{
		Missing.Add(RawKey);
	}
	return Raw;
}

UTexture* UBRAssets::Icon(FName Name)
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (UTexture* T = Cast<UTexture>(LoadAsset(UIFolder, Name, UTexture::StaticClass())))
	{
		return T;
	}
	const FName RawKey(*(TEXT("RawIcon/") + Name.ToString()));
	if (TObjectPtr<UObject>* Found = Loaded.Find(RawKey))
	{
		return Cast<UTexture>(Found->Get());
	}
	if (Missing.Contains(RawKey))
	{
		return nullptr;
	}
	UTexture2D* Raw = LoadRawTexture(TEXT("Icons"), Name, false, false);
	if (Raw)
	{
		Loaded.Add(RawKey, Raw);
	}
	else
	{
		Missing.Add(RawKey);
	}
	return Raw;
}

USoundBase* UBRAssets::Sound(FName Name)
{
	return Cast<USoundBase>(LoadAsset(SoundFolder, Name, USoundBase::StaticClass()));
}

USoundAttenuation* UBRAssets::VoiceAttenuation()
{
	if (!VoiceAtt)
	{
		VoiceAtt = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& S = VoiceAtt->Attenuation;
		S.bAttenuate = true;
		S.bSpatialize = true;
		S.AttenuationShape = EAttenuationShape::Sphere;
		S.AttenuationShapeExtents = FVector(250.f, 0.f, 0.f);
		S.FalloffDistance = 2400.f;
		S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		// Derriere un mur la voix est etouffee (filtre passe-bas) et plus faible
		S.bEnableOcclusion = true;
		S.OcclusionTraceChannel = ECC_Visibility;
		S.OcclusionLowPassFilterFrequency = 900.f;
		S.OcclusionVolumeAttenuation = 0.45f;
		S.OcclusionInterpolationTime = 0.25f;
	}
	return VoiceAtt;
}

USoundAttenuation* UBRAssets::Attenuation(float FalloffDistance, bool bOcclude)
{
	const int32 Key = FMath::RoundToInt(FalloffDistance / 100.f) * 2 + (bOcclude ? 1 : 0);
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
	// v4.7 : ou est la menace ? Derriere un mur, le son est etouffe et plus faible (il ne vient pas de la ou on le croit) ;
	// de loin, il perd ses aigus et se noie dans la reverberation du lieu ; de pres, il est sec et net
	Att->Attenuation.bEnableOcclusion = bOcclude;
	Att->Attenuation.OcclusionTraceChannel = ECC_Visibility;
	Att->Attenuation.OcclusionLowPassFilterFrequency = 1100.f;
	Att->Attenuation.OcclusionVolumeAttenuation = 0.5f;
	Att->Attenuation.OcclusionInterpolationTime = 0.2f;
	Att->Attenuation.bAttenuateWithLPF = true;
	Att->Attenuation.LPFRadiusMin = FalloffDistance * 0.25f;
	Att->Attenuation.LPFRadiusMax = FalloffDistance;
	Att->Attenuation.LPFFrequencyAtMin = 20000.f;
	Att->Attenuation.LPFFrequencyAtMax = 2800.f;
	Att->Attenuation.bEnableReverbSend = true;
	Att->Attenuation.ReverbWetLevelMin = 0.25f;
	Att->Attenuation.ReverbWetLevelMax = 0.9f;
	Att->Attenuation.ReverbDistanceMin = FalloffDistance * 0.1f;
	Att->Attenuation.ReverbDistanceMax = FalloffDistance;
	AttCache.Add(Key, Att);
	return Att;
}

// ---------------------------------------------------------------------------------------------------------------------
// Materiaux maitres
// ---------------------------------------------------------------------------------------------------------------------

UMaterialInterface* UBRAssets::Parent(EParent Which)
{
	const int32 Index = static_cast<int32>(Which);
	const int32 Count = static_cast<int32>(EParent::Count);
	if (Parents.Num() != Count)
	{
		Parents.SetNum(Count);
		ParentResolved.Init(false, Count);
		ParentCustom.Init(false, Count);
	}
	if (ParentResolved[Index])
	{
		return Parents[Index];
	}
	ParentResolved[Index] = true;

	const TCHAR* AssetNames[] = { TEXT("M_BR_World"), TEXT("M_BR_Mesh"), TEXT("M_BR_Skin"), TEXT("M_BR_WaterSurface"), TEXT("M_BR_PitShade") };
	// Parametre propre a la version attendue de chaque materiau : une version plus ancienne (sans ce parametre) est ignoree
	// (v4.7 : "RoughContrast", la carte de rugosite ; la peau et les modeles l'ont aussi)
	const TCHAR* V2Params[] = { TEXT("RoughContrast"), TEXT("RoughContrast"), TEXT("RoughContrast"), TEXT("WaterSim"), TEXT("PitShadeAmount") };
	static_assert(UE_ARRAY_COUNT(AssetNames) == static_cast<int32>(EParent::Count), "Un materiau maitre par EParent");
	// -BRRuntimeMaterials : ignore les materiaux importes (pour tester ceux construits en C++)
	static const bool bForceRuntime = FParse::Param(FCommandLine::Get(), TEXT("BRRuntimeMaterials"));
	UMaterialInterface* M = bForceRuntime ? nullptr
		: Cast<UMaterialInterface>(LoadAsset(MatFolder, FName(AssetNames[Index]), UMaterialInterface::StaticClass()));
	if (M)
	{
		const FHashedMaterialParameterInfo Info{ FName(V2Params[Index]) };
		float ScalarValue = 0.f;
		UTexture* TexValue = nullptr;
		const bool bV2 = M->GetScalarParameterValue(Info, ScalarValue) || M->GetTextureParameterValue(Info, TexValue);
		if (!bV2)
		{
			UE_LOG(LogBackrooms, Warning, TEXT("%s date d'une ancienne version : relancez l'import (backrooms_setup.run(force=True))."),
				AssetNames[Index]);
			// Sans constructeur de materiaux (jeu empaquete), une ancienne version vaut mieux que des couleurs unies
			if (BRMaterialBuilder::IsAvailable())
			{
				M = nullptr;
			}
		}
	}

	// v4.8 : usages declares (instances, maillages a squelette, Nanite). Un materiau importe avant la v4.8 n'a que l'usage
	// "instances" : la combinaison et les entites a squelette recevaient le materiau par defaut hors de l'editeur
	static const EBRMasterMaterial UsageKinds[] = { EBRMasterMaterial::World, EBRMasterMaterial::Mesh, EBRMasterMaterial::Skin,
		EBRMasterMaterial::WaterSurface, EBRMasterMaterial::PitShade };
	if (M && !EnsureUsages(M, UsageKinds[Index], AssetNames[Index]) && BRMaterialBuilder::IsAvailable())
	{
		M = nullptr; // jeu autonome non cuisine : le materiau construit a la volee declare tous ses usages
	}

	if (!M && BRMaterialBuilder::IsAvailable())
	{
		static const EBRMasterMaterial Kinds[] = { EBRMasterMaterial::World, EBRMasterMaterial::Mesh, EBRMasterMaterial::Skin,
			EBRMasterMaterial::WaterSurface, EBRMasterMaterial::PitShade };
		M = BRMaterialBuilder::Build(Kinds[Index], this, [this](FName TexName) { return Texture(TexName); });
		if (M)
		{
			bRuntimeContent = true;
		}
	}
	// La peau retombe sur le materiau des modeles
	if (!M && Which == EParent::Skin)
	{
		M = Parent(EParent::Mesh);
		ParentCustom[Index] = ParentCustom[static_cast<int32>(EParent::Mesh)];
		Parents[Index] = M;
		return M;
	}
	Parents[Index] = M;
	ParentCustom[Index] = M != nullptr;
	return M;
}

namespace
{
	TArray<FString>& MaterialProblemList()
	{
		static TArray<FString> List;
		return List;
	}
}

void UBRAssets::ReportMaterialProblem(const FString& Problem)
{
	TArray<FString>& L = MaterialProblemList();
	if (!L.Contains(Problem))
	{
		L.Add(Problem);
		UE_LOG(LogBackrooms, Error, TEXT("Materiaux : %s"), *Problem);
	}
}

const TArray<FString>& UBRAssets::GetMaterialProblems()
{
	return MaterialProblemList();
}

bool UBRAssets::EnsureUsages(UMaterialInterface* M, EBRMasterMaterial Kind, const TCHAR* AssetName)
{
	UMaterial* Base = M ? M->GetMaterial() : nullptr;
	if (!Base)
	{
		return false;
	}
	TArray<EMaterialUsage> Absent;
	for (const EMaterialUsage Usage : BRMaterialBuilder::RequiredUsages(Kind))
	{
		if (!Base->GetUsageByFlag(Usage))
		{
			Absent.Add(Usage);
		}
	}
	if (Absent.Num() == 0)
	{
		return true;
	}
	FString Names;
	for (const EMaterialUsage Usage : Absent)
	{
		Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(BRMaterialBuilder::UsageName(Usage));
	}
#if WITH_EDITOR
	if (GIsEditor)
	{
		// Editeur (PIE) : l'usage est ajoute en memoire et le materiau recompile ; il reste a l'enregistrer
		for (const EMaterialUsage Usage : Absent)
		{
			bool bNeedsRecompile = false;
			Base->SetMaterialUsage(bNeedsRecompile, Usage);
		}
		UE_LOG(LogBackrooms, Warning, TEXT("%s : usage(s) %s ajoute(s) en memoire. Pour l'enregistrer : backrooms_setup.repair() ")
			TEXT("(Fenetre > Journal de sortie > Python)."), AssetName, *Names);
		return true;
	}
#endif
	ReportMaterialProblem(FString::Printf(TEXT("%s n'a pas l'usage %s : materiau par defaut sur les modeles concernes. ")
		TEXT("Reparer dans l'editeur : import backrooms_setup; backrooms_setup.repair(), puis recuire le jeu."), AssetName, *Names));
	return false;
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
	return Parent(EParent::World) != nullptr;
}

bool UBRAssets::IsUsingRuntimeContent()
{
	HasContent();
	return bRuntimeContent;
}

FLinearColor UBRAssets::TextureAverage(FName Texture)
{
	static const TMap<FName, FLinearColor> Avg = {
		{ TEXT("T_L0_Wallpaper"), FLinearColor(0.66f, 0.64f, 0.33f) },
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
		{ TEXT("T_PoolTile37"), FLinearColor(0.84f, 0.88f, 0.84f) },
		{ TEXT("T_Plaster"), FLinearColor(0.84f, 0.85f, 0.84f) },
		{ TEXT("T_OfficeCarpetNavy"), FLinearColor(0.07f, 0.12f, 0.25f) },
		{ TEXT("T_OfficeCeiling"), FLinearColor(0.88f, 0.88f, 0.89f) },
		{ TEXT("T_Hazmat_Suit"), FLinearColor(0.75f, 0.72f, 0.49f) },
		{ TEXT("T_Hazmat_Mask"), FLinearColor(0.41f, 0.41f, 0.4f) },
		{ TEXT("T_Deathmoth"), FLinearColor(0.69f, 0.6f, 0.49f) },
		{ TEXT("T_Rock"), FLinearColor(0.33f, 0.29f, 0.25f) },
		{ TEXT("T_Dirt"), FLinearColor(0.36f, 0.28f, 0.19f) },
		{ TEXT("T_Asphalt"), FLinearColor(0.17f, 0.17f, 0.18f) },
		{ TEXT("T_Grass"), FLinearColor(0.2f, 0.3f, 0.11f) },
		{ TEXT("T_Facade"), FLinearColor(0.45f, 0.44f, 0.43f) },
		{ TEXT("T_Siding"), FLinearColor(0.68f, 0.72f, 0.74f) },
		{ TEXT("T_Glitch"), FLinearColor(0.5f, 0.4f, 0.6f) },
		{ TEXT("T_Paper"), FLinearColor(0.88f, 0.85f, 0.74f) },
		{ TEXT("T_Skin"), FLinearColor(0.75f, 0.62f, 0.55f) },
		{ TEXT("T_WaterNormal"), FLinearColor(0.3f, 0.6f, 0.7f) },
	};
	if (const FLinearColor* Found = Avg.Find(Texture))
	{
		return *Found;
	}
	return FLinearColor(0.8f, 0.8f, 0.8f);
}

UMaterialInstanceDynamic* UBRAssets::NewPitShade(UObject* Outer)
{
	UMaterialInterface* M = Parent(EParent::PitShade);
	return M ? UMaterialInstanceDynamic::Create(M, Outer ? Outer : this) : nullptr;
}

void UBRAssets::ApplyRoughMap(UMaterialInstanceDynamic* MID, FName BaseTexture, float Detail)
{
	if (!MID)
	{
		return;
	}
	UTexture* Map = BaseTexture.IsNone() ? nullptr : Texture(FName(*(BaseTexture.ToString() + TEXT("_R"))));
	// Le type d'echantillonneur est fige a la compilation du materiau parent : la carte n'est branchee que si sa valeur
	// par defaut est lineaire, comme la carte (sinon le rendu serait faux sans message clair)
	UTexture* Default = nullptr;
	const FHashedMaterialParameterInfo Info{ FName(TEXT("RoughTex")) };
	const bool bParam = MID->GetTextureParameterValue(Info, Default);
	const bool bUse = Map && !Map->SRGB && bParam && Default && !Default->SRGB && Detail > 0.f;
	if (bUse)
	{
		MID->SetTextureParameterValue(TEXT("RoughTex"), Map);
	}
	MID->SetScalarParameterValue(TEXT("RoughContrast"), bUse ? Detail : 0.f);
}

UMaterialInterface* UBRAssets::Surface(const FBRSurface& S)
{
	const FString Key = TEXT("W|") + S.Key();
	if (TObjectPtr<UMaterialInterface>* Found = MatCache.Find(Key))
	{
		return Found->Get();
	}
	UMaterialInstanceDynamic* MID = CreateSurface(S, this);
	if (MID)
	{
		MatCache.Add(Key, MID);
	}
	return MID;
}

UMaterialInstanceDynamic* UBRAssets::NewSurface(const FBRSurface& S, UObject* Outer)
{
	// Un MID ne peut pas avoir un autre MID pour parent : on repart du materiau maitre
	return CreateSurface(S, Outer ? Outer : this);
}

UMaterialInstanceDynamic* UBRAssets::CreateSurface(const FBRSurface& S, UObject* Outer)
{
	UMaterialInterface* ParentMat = Parent(EParent::World);
	const bool bCustom = ParentMat != nullptr;
	if (!ParentMat)
	{
		ParentMat = FallbackParent();
	}
	if (!ParentMat)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMat, Outer);
	if (bCustom)
	{
		if (UTexture* Tex = Texture(S.Texture))
		{
			MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
		}
		// Normal map associee (<Texture>_N) : relief du papier peint, de la moquette, des joints de carrelage...
		UTexture* Nrm = S.Texture.IsNone() ? nullptr : Texture(FName(*(S.Texture.ToString() + TEXT("_N"))));
		if (Nrm)
		{
			MID->SetTextureParameterValue(TEXT("NormalTex"), Nrm);
		}
		MID->SetScalarParameterValue(TEXT("NormalStrength"), Nrm ? 1.f : 0.f);
		ApplyRoughMap(MID, S.Texture, S.RoughDetail);
		if (UTexture* Grime = Texture(TEXT("T_Grime")))
		{
			MID->SetTextureParameterValue(TEXT("GrimeTex"), Grime);
			MID->SetTextureParameterValue(TEXT("PuddleTex"), Grime);
		}
		if (S.Caustics > 0.f)
		{
			if (UTexture* Caus = Texture(TEXT("T_Caustics")))
			{
				MID->SetTextureParameterValue(TEXT("CausticsTex"), Caus);
			}
			// Les vagues autour du joueur deforment les caustiques
			WaterSimMIDs.Add(MID);
			if (UTexture* Sim = WaterSimTexture.Get())
			{
				MID->SetTextureParameterValue(TEXT("WaterSim"), Sim);
				MID->SetVectorParameterValue(TEXT("WaterSimWindow"), WaterSimWindow);
			}
		}
		MID->SetVectorParameterValue(TEXT("Tint"), S.Tint);
		MID->SetScalarParameterValue(TEXT("TexScale"), S.Scale);
		MID->SetScalarParameterValue(TEXT("Roughness"), S.Roughness);
		MID->SetScalarParameterValue(TEXT("Metallic"), S.Metallic);
		MID->SetScalarParameterValue(TEXT("Grime"), S.Grime);
		MID->SetScalarParameterValue(TEXT("SelfIllum"), S.SelfIllum);
		MID->SetVectorParameterValue(TEXT("Emissive"), S.Emissive);
		MID->SetScalarParameterValue(TEXT("Caustics"), S.Caustics);
		MID->SetScalarParameterValue(TEXT("FloorGrime"), S.FloorGrime);
		MID->SetScalarParameterValue(TEXT("Puddles"), S.Puddles);
		MID->SetScalarParameterValue(TEXT("Wetness"), S.Wetness);
		MID->SetScalarParameterValue(TEXT("AntiTile"), S.AntiTile);
		MID->SetScalarParameterValue(TEXT("Stains"), S.Stains);
		MID->SetScalarParameterValue(TEXT("WaterLine"), S.WaterLine);
		MID->SetScalarParameterValue(TEXT("WallVariation"), S.WallVariation);
	}
	else
	{
		MID->SetVectorParameterValue(TEXT("Color"), TextureAverage(S.Texture) * S.Tint);
	}
	return MID;
}

UMaterialInterface* UBRAssets::WaterMaterial(const FBRSurface& S, float Absorption, float Scattering, float Waves, float Chop)
{
	const FString Key = FString::Printf(TEXT("Water|%.3f|%.3f|%.2f|%.2f|"), Absorption, Scattering, Waves, Chop) + S.Key();
	if (TObjectPtr<UMaterialInterface>* Found = MatCache.Find(Key))
	{
		return Found->Get();
	}
	UMaterialInterface* ParentMat = Parent(EParent::WaterSurface);
	const bool bCustom = ParentMat != nullptr;
	if (!ParentMat)
	{
		ParentMat = FallbackParent();
	}
	if (!ParentMat)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMat, this);
	if (bCustom)
	{
		if (UTexture* N = Texture(TEXT("T_WaterNormal")))
		{
			MID->SetTextureParameterValue(TEXT("NormalTex"), N);
		}
		MID->SetVectorParameterValue(TEXT("Tint"), S.Tint);
		MID->SetScalarParameterValue(TEXT("TexScale"), S.Scale);
		MID->SetScalarParameterValue(TEXT("Roughness"), FMath::Min(S.Roughness, 0.08f));
		MID->SetScalarParameterValue(TEXT("WaveAmplitude"), Waves);
		MID->SetScalarParameterValue(TEXT("WaveChop"), Chop);
		// Rides de detail : quasi absentes sur une eau calme, nettes sur une eau agitee
		MID->SetScalarParameterValue(TEXT("NormalStrength"), 0.03f + 0.14f * Chop);
		MID->SetScalarParameterValue(TEXT("Absorption"), Absorption);
		MID->SetScalarParameterValue(TEXT("Scattering"), Scattering);
		// Refraction un peu exageree : les vagues se voient a travers l'eau, meme peu profonde
		MID->SetScalarParameterValue(TEXT("RefractionStrength"), 1.8f);
		WaterSimMIDs.Add(MID);
		if (UTexture* Sim = WaterSimTexture.Get())
		{
			MID->SetTextureParameterValue(TEXT("WaterSim"), Sim);
			MID->SetVectorParameterValue(TEXT("WaterSimWindow"), WaterSimWindow);
		}
	}
	else
	{
		MID->SetVectorParameterValue(TEXT("Color"), S.Tint);
	}
	MatCache.Add(Key, MID);
	return MID;
}

void UBRAssets::SetWaterSim(UTexture* SimTexture, const FLinearColor& Window)
{
	const bool bNewTexture = WaterSimTexture.Get() != SimTexture;
	if (!bNewTexture && Window.Equals(WaterSimWindow, 0.01f))
	{
		return; // le contenu de la texture change, pas les parametres des materiaux
	}
	WaterSimTexture = SimTexture;
	WaterSimWindow = Window;
	for (int32 i = WaterSimMIDs.Num() - 1; i >= 0; --i)
	{
		UMaterialInstanceDynamic* MID = WaterSimMIDs[i].Get();
		if (!MID)
		{
			WaterSimMIDs.RemoveAtSwap(i);
			continue;
		}
		if (bNewTexture && SimTexture)
		{
			MID->SetTextureParameterValue(TEXT("WaterSim"), SimTexture);
		}
		MID->SetVectorParameterValue(TEXT("WaterSimWindow"), Window);
	}
}

FString UBRAssets::CheckImportedMeshes()
{
	// Modeles de reference et leur plus grande dimension attendue (cm)
	struct FRef
	{
		const TCHAR* Name;
		float Expected;
	};
	static const FRef Refs[] = { { TEXT("SM_LightPanel"), 122.f }, { TEXT("SM_WaterGrid"), 100.f }, { TEXT("SM_Crate"), 60.f } };
	for (const FRef& R : Refs)
	{
		UStaticMesh* M = Mesh(R.Name);
		if (!M)
		{
			continue;
		}
		const float Size = static_cast<float>(M->GetBoundingBox().GetSize().GetMax());
		if (Size < R.Expected * 0.2f || Size > R.Expected * 5.f)
		{
			return FString::Printf(TEXT("Mod\u00e8les 3D import\u00e9s \u00e0 la mauvaise \u00e9chelle (%s : %.1f cm au lieu de %.0f) : ils sont invisibles. ")
				TEXT("Relancez l'import : Fen\u00eatre > Journal de sortie > Python : import backrooms_setup; backrooms_setup.run(force=True)"),
				R.Name, Size, R.Expected);
		}
	}
	return FString();
}

FLinearColor UBRAssets::GlowColorForSlot(const FString& SlotName)
{
	if (!SlotName.Contains(TEXT("Glow"), ESearchCase::IgnoreCase))
	{
		return FLinearColor::Black;
	}
	if (SlotName.Contains(TEXT("GlowSoft"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(1.f, 0.93f, 0.82f) * 10.f; // bord des yeux et racines des dents du Smiler
	}
	// v4.5 : Smiler calibre (x 0,5 en jeu) : yeux ~14, dents ~10 ; assez pour percer le noir sans devenir un panneau plat
	if (SlotName.Contains(TEXT("GlowEye"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(1.f, 0.97f, 0.9f) * 28.f;
	}
	if (SlotName.Contains(TEXT("GlowTooth"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(1.f, 0.95f, 0.86f) * 20.f;
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
	if (SlotName.Contains(TEXT("GlowAmber"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(1.f, 0.55f, 0.12f) * 30.f; // yeux du Hound sous ses cheveux
	}
	if (SlotName.Contains(TEXT("GlowWarm"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(1.f, 0.7f, 0.38f) * 90.f;
	}
	if (SlotName.Contains(TEXT("GlowSky"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.92f, 0.97f, 1.f) * 260.f; // verriere : jour eblouissant
	}
	if (SlotName.Contains(TEXT("GlowCool"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.95f, 0.98f, 1.f) * 160.f;
	}
	return FLinearColor(1.f, 0.96f, 0.86f) * 120.f;
}

bool UBRAssets::IsSkinSlot(const FString& SlotName)
{
	return SlotName.Contains(TEXT("Skin"), ESearchCase::IgnoreCase) || SlotName.Contains(TEXT("Flesh"), ESearchCase::IgnoreCase)
		|| SlotName.Contains(TEXT("Vein"), ESearchCase::IgnoreCase) || SlotName.Contains(TEXT("Party"), ESearchCase::IgnoreCase);
}

UMaterialInstanceDynamic* UBRAssets::NewGlow(UObject* Outer, const FLinearColor& Color, float Strength)
{
	UMaterialInterface* ParentMat = Parent(EParent::Mesh);
	const bool bCustom = ParentMat != nullptr;
	if (!ParentMat)
	{
		ParentMat = FallbackParent();
	}
	if (!ParentMat)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMat, Outer ? Outer : this);
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
		const bool bSkin = IsSkinSlot(SlotName);
		UMaterialInterface* ParentMat = Parent(bSkin ? EParent::Skin : EParent::Mesh);
		const bool bCustom = ParentMat != nullptr;
		if (!ParentMat)
		{
			ParentMat = FallbackParent();
		}
		if (ParentMat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMat, this);
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
				// v4.7 : carte de rugosite de la texture du modele (peaux du Wretch et du Clump, metal rouille)
				ApplyRoughMap(MID, Style ? FName(Style->Tex) : NAME_None, 1.f);
				MID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor::Black);
				if (bSkin)
				{
					if (UTexture* N = Texture((Style && Style->Normal) ? FName(Style->Normal) : FName(TEXT("T_Skin_N"))))
					{
						MID->SetTextureParameterValue(TEXT("NormalTex"), N);
					}
					// Chair translucide du Skin-Stealer : diffusion sous-cutanee plus forte
					const bool bGlass = SlotName.Contains(TEXT("Glass"), ESearchCase::IgnoreCase);
					MID->SetScalarParameterValue(TEXT("Subsurface"), bGlass ? 1.f : 0.6f);
					MID->SetVectorParameterValue(TEXT("SubsurfaceColor"), bGlass ? FLinearColor(1.f, 0.25f, 0.2f) : FLinearColor(1.f, 0.35f, 0.25f));
				}
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
	TArray<UMaterialInstanceDynamic*>* OutGlow, float GlowScale, bool bPowered)
{
	if (!Comp)
	{
		return;
	}
	const TArray<FName> Names = Comp->GetMaterialSlotNames();
	const int32 Num = Comp->GetNumMaterials();
	for (int32 i = 0; i < Num; ++i)
	{
		// v4.8 : un slot sans nom n'est plus deguise en "Body" (presque noir) : il est nomme par son indice et signale
		FString Slot = Names.IsValidIndex(i) ? Names[i].ToString() : FString();
		if (Slot.IsEmpty() || Slot == TEXT("None"))
		{
			Slot = FString::Printf(TEXT("Slot%d"), i);
			ReportMaterialProblem(FString::Printf(TEXT("%s : slot %d sans nom (materiau neutre)"), *GetNameSafe(Comp), i));
		}
		const FLinearColor Glow = GlowColorForSlot(Slot);
		UMaterialInterface* Mat = nullptr;
		// Les panneaux de sortie (vert), voyants (rouge) et fenetres ne dependent pas du secteur
		const bool bMains = bPowered && !Glow.IsAlmostBlack() && !Slot.Contains(TEXT("GlowGreen"), ESearchCase::IgnoreCase)
			&& !Slot.Contains(TEXT("GlowRed"), ESearchCase::IgnoreCase) && !Slot.Contains(TEXT("GlowWindow"), ESearchCase::IgnoreCase);

		if (bMains && !bUniqueGlow)
		{
			// Materiau partage par tous les neons d'un meme type, eteint pendant les coupures
			const FString Key = FString::Printf(TEXT("P|%s|%.2f"), *Slot, GlowScale);
			if (TObjectPtr<UMaterialInterface>* Found = MatCache.Find(Key))
			{
				Mat = Found->Get();
			}
			else
			{
				const float Strength = FMath::Max3(Glow.R, Glow.G, Glow.B);
				const FLinearColor Unit = Glow / FMath::Max(Strength, 0.001f);
				UMaterialInstanceDynamic* MID = NewGlow(this, Unit, Strength * GlowScale);
				if (MID)
				{
					FPoweredGlow PG;
					PG.MID = MID;
					PG.Base = Unit * Strength * GlowScale;
					PoweredGlows.Add(PG);
					MID->SetVectorParameterValue(TEXT("Emissive"), PG.Base * GlowScaleNow);
					MatCache.Add(Key, MID);
				}
				Mat = MID;
			}
		}
		else if (!Glow.IsAlmostBlack() && (bUniqueGlow || !FMath::IsNearlyEqual(GlowScale, 1.f)))
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

void UBRAssets::ApplySlotMap(UMeshComponent* Comp, const TArray<TPair<FString, FString>>& SlotStylesByName, const FString& Label)
{
	if (!Comp)
	{
		return;
	}
	const TArray<FName> Names = Comp->GetMaterialSlotNames();
	const int32 Num = Comp->GetNumMaterials();
	for (int32 i = 0; i < Num; ++i)
	{
		const FString Slot = Names.IsValidIndex(i) ? Names[i].ToString() : FString();
		// Correspondance explicite par nom (exact, sinon contenu : Interchange peut ajouter un suffixe) ; jamais par indice
		const FString* StyleKey = nullptr;
		for (const TPair<FString, FString>& Pair : SlotStylesByName)
		{
			if (Slot.Equals(Pair.Key, ESearchCase::IgnoreCase))
			{
				StyleKey = &Pair.Value;
				break;
			}
		}
		for (int32 k = 0; !StyleKey && k < SlotStylesByName.Num(); ++k)
		{
			if (!Slot.IsEmpty() && Slot.Contains(SlotStylesByName[k].Key, ESearchCase::IgnoreCase))
			{
				StyleKey = &SlotStylesByName[k].Value;
			}
		}
		if (!StyleKey)
		{
			FString Expected;
			for (const TPair<FString, FString>& Pair : SlotStylesByName)
			{
				Expected += (Expected.IsEmpty() ? TEXT("") : TEXT(", ")) + Pair.Key;
			}
			ReportMaterialProblem(FString::Printf(TEXT("%s : slot %d \"%s\" inattendu (attendus : %s) ; materiau d'erreur affiche"),
				*Label, i, *Slot, *Expected));
			Comp->SetMaterial(i, ErrorMaterial());
			continue;
		}
		if (UMaterialInterface* Mat = SlotMaterial(*StyleKey))
		{
			Comp->SetMaterial(i, Mat);
		}
	}
}

UMaterialInterface* UBRAssets::ErrorMaterial()
{
	const FString Key = TEXT("ERROR");
	if (TObjectPtr<UMaterialInterface>* Found = MatCache.Find(Key))
	{
		return Found->Get();
	}
	// Developpement : magenta franc (une erreur se voit) ; version Shipping : gris neutre (jamais noir)
#if UE_BUILD_SHIPPING
	const FLinearColor Color(0.45f, 0.45f, 0.45f);
#else
	const FLinearColor Color(1.f, 0.f, 0.85f);
#endif
	UMaterialInterface* ParentMat = Parent(EParent::Mesh);
	const bool bCustom = ParentMat != nullptr;
	ParentMat = ParentMat ? ParentMat : FallbackParent();
	UMaterialInstanceDynamic* MID = ParentMat ? UMaterialInstanceDynamic::Create(ParentMat, this) : nullptr;
	if (MID)
	{
		MID->SetVectorParameterValue(bCustom ? TEXT("Tint") : TEXT("Color"), Color);
		MatCache.Add(Key, MID);
	}
	return MID;
}

TArray<FString> UBRAssets::DescribeSections(UMeshComponent* Comp, const FString& Label)
{
	TArray<FString> Lines;
	if (!Comp)
	{
		return Lines;
	}
	const TArray<FName> Names = Comp->GetMaterialSlotNames();
	auto Describe = [&](int32 Lod, int32 Section, int32 MatIndex, bool bSkeletal)
	{
		UMaterialInterface* Mat = Comp->GetMaterial(MatIndex);
		const UMaterialInstance* Inst = Cast<UMaterialInstance>(Mat);
		const UMaterialInterface* ParentMat = Inst ? Inst->Parent.Get() : nullptr;
		const UMaterial* Base = Mat ? Mat->GetMaterial() : nullptr;
		UTexture* Tex = nullptr;
		if (Mat)
		{
			Mat->GetTextureParameterValue(FHashedMaterialParameterInfo(FName(TEXT("BaseTex"))), Tex);
		}
		const bool bUsage = Base && (!bSkeletal || Base->GetUsageByFlag(MATUSAGE_SkeletalMesh));
		const bool bFallback = !Mat || Mat == ErrorMaterial() || (ParentMat && ParentMat == FallbackMat) || !Base
			|| (Base && Base->GetName().StartsWith(TEXT("WorldGrid"))) || (Base && Base->GetName().StartsWith(TEXT("DefaultMaterial")));
		Lines.Add(FString::Printf(TEXT("%s LOD%d section %d -> slot %d \"%s\" : %s (parent %s, BaseTex %s, usage %s, secours %s)"),
			*Label, Lod, Section, MatIndex, Names.IsValidIndex(MatIndex) ? *Names[MatIndex].ToString() : TEXT("?"), *GetNameSafe(Mat),
			*GetNameSafe(ParentMat), *GetNameSafe(Tex), bSkeletal ? (bUsage ? TEXT("squelette OK") : TEXT("SQUELETTE ABSENT")) : TEXT("-"),
			bFallback ? TEXT("OUI") : TEXT("non")));
		if (!bUsage || bFallback)
		{
			ReportMaterialProblem(Lines.Last());
		}
	};
	const USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Comp);
	const USkeletalMesh* Skel = Skinned ? Cast<USkeletalMesh>(Skinned->GetSkinnedAsset()) : nullptr;
	const FSkeletalMeshRenderData* RD = Skel ? Skel->GetResourceForRendering() : nullptr;
	if (RD)
	{
		// Sections de chaque niveau de detail, avec la table de remplacement des materiaux par LOD (LODMaterialMap)
		for (int32 Lod = 0; Lod < RD->LODRenderData.Num(); ++Lod)
		{
			const FSkeletalMeshLODInfo* Info = Skel->GetLODInfo(Lod);
			const TArray<FSkelMeshRenderSection>& Sections = RD->LODRenderData[Lod].RenderSections;
			for (int32 Sec = 0; Sec < Sections.Num(); ++Sec)
			{
				int32 MatIndex = Sections[Sec].MaterialIndex;
				if (Info && Info->LODMaterialMap.IsValidIndex(Sec) && Info->LODMaterialMap[Sec] != INDEX_NONE)
				{
					MatIndex = Info->LODMaterialMap[Sec];
				}
				Describe(Lod, Sec, MatIndex, true);
			}
		}
	}
	else
	{
		for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
		{
			Describe(0, i, i, false);
		}
	}
	return Lines;
}

void UBRAssets::SetGlowScale(float Scale)
{
	GlowScaleNow = FMath::Max(0.f, Scale);
	for (int32 i = PoweredGlows.Num() - 1; i >= 0; --i)
	{
		UMaterialInstanceDynamic* MID = PoweredGlows[i].MID.Get();
		if (!MID)
		{
			PoweredGlows.RemoveAtSwap(i);
			continue;
		}
		MID->SetVectorParameterValue(TEXT("Emissive"), PoweredGlows[i].Base * GlowScaleNow);
	}
}
