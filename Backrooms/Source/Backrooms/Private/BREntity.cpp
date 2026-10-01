#include "BREntity.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRWorld.h"

#include "AIController.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

// =====================================================================================================================
// Fiches des entites (descriptions inspirees du Backrooms Wiki)
// =====================================================================================================================

const FBREntityInfo& ABREntity::Info(EBREntityKind InKind)
{
	static const TArray<FBREntityInfo> Infos = []()
	{
		TArray<FBREntityInfo> L;
		L.SetNum(static_cast<int32>(EBREntityKind::Count));

		FBREntityInfo& Smiler = L[static_cast<int32>(EBREntityKind::Smiler)];
		Smiler.Name = TEXT("Smilers");
		Smiler.Number = TEXT("Entit\u00e9 3");
		Smiler.Description = TEXT("Une silhouette invisible dans l'obscurit\u00e9, dont on ne distingue que deux yeux et un sourire ")
			TEXT("lumineux. On les trouve dans les zones o\u00f9 la lumi\u00e8re est morte.");
		Smiler.Advice = TEXT("Ne braquez pas votre lampe sur eux et ne courez pas. Eteignez la lumi\u00e8re et reculez lentement.");
		Smiler.HalfHeight = 50.f; Smiler.Radius = 45.f; Smiler.bFlying = true; Smiler.HoverHeight = 165.f; Smiler.bNeedsDark = true;
		Smiler.WalkSpeed = 110.f; Smiler.ChaseSpeed = 640.f; Smiler.SightRange = 1800.f; Smiler.AttackRange = 110.f;
		Smiler.Damage = 100.f; Smiler.SanityDamage = 30.f; Smiler.AttackCooldown = 1.f; Smiler.Aura = 0.8f; Smiler.AuraRadius = 900.f;
		Smiler.Voice = TEXT("S_Smiler"); Smiler.VoiceInterval = 12.f; Smiler.VoiceFalloff = 2000.f;

		FBREntityInfo& Hound = L[static_cast<int32>(EBREntityKind::Hound)];
		Hound.Name = TEXT("Hounds");
		Hound.Number = TEXT("Entit\u00e9 8");
		Hound.Description = TEXT("Des humano\u00efdes p\u00e2les et d\u00e9charn\u00e9s qui se d\u00e9placent \u00e0 quatre pattes, le visage ")
			TEXT("cach\u00e9 sous de longs cheveux noirs. Ils chassent en sentant la peur.");
		Hound.Advice = TEXT("Ne fuyez jamais en courant devant un Hound. Faites-lui face, regardez-le et reculez calmement.");
		Hound.HalfHeight = 45.f; Hound.Radius = 38.f; Hound.WalkSpeed = 180.f; Hound.ChaseSpeed = 465.f; Hound.SightRange = 2200.f;
		Hound.AttackRange = 130.f; Hound.Damage = 30.f; Hound.SanityDamage = 10.f; Hound.AttackCooldown = 1.3f; Hound.Aura = 0.3f;
		Hound.Voice = TEXT("S_Hound"); Hound.VoiceInterval = 7.f;

		FBREntityInfo& Faceling = L[static_cast<int32>(EBREntityKind::Faceling)];
		Faceling.Name = TEXT("Facelings");
		Faceling.Number = TEXT("Entit\u00e9 9");
		Faceling.Description = TEXT("Des \u00eatres d'apparence humaine, v\u00eatus normalement, mais sans le moindre visage. ")
			TEXT("La plupart errent sans but et ignorent les vagabonds.");
		Faceling.Advice = TEXT("Restez poli et gardez vos distances. Certains adultes deviennent agressifs si on les approche.");
		Faceling.HalfHeight = 92.f; Faceling.Radius = 30.f; Faceling.WalkSpeed = 115.f; Faceling.ChaseSpeed = 300.f;
		Faceling.SightRange = 1500.f; Faceling.AttackRange = 110.f; Faceling.Damage = 15.f; Faceling.SanityDamage = 5.f;
		Faceling.AttackCooldown = 1.8f; Faceling.Aura = 0.2f; Faceling.AuraRadius = 450.f;
		Faceling.Voice = TEXT("S_Faceling"); Faceling.VoiceInterval = 14.f;

		FBREntityInfo& Skin = L[static_cast<int32>(EBREntityKind::SkinStealer)];
		Skin.Name = TEXT("Skin-Stealers");
		Skin.Number = TEXT("Entit\u00e9 10");
		Skin.Description = TEXT("Une cr\u00e9ature longiligne qui porte la peau de ses victimes et imite leurs voix pour attirer ")
			TEXT("les vagabonds. Extr\u00eamement dangereuse.");
		Skin.Advice = TEXT("Ne r\u00e9pondez jamais \u00e0 une voix famili\u00e8re. Fuyez d\u00e8s que vous l'apercevez.");
		Skin.HalfHeight = 112.f; Skin.Radius = 32.f; Skin.WalkSpeed = 160.f; Skin.ChaseSpeed = 440.f; Skin.SightRange = 2400.f;
		Skin.AttackRange = 140.f; Skin.Damage = 45.f; Skin.SanityDamage = 15.f; Skin.AttackCooldown = 1.5f; Skin.Aura = 0.5f;
		Skin.Voice = TEXT("S_SkinStealer"); Skin.VoiceInterval = 9.f; Skin.VoiceFalloff = 3000.f;

		FBREntityInfo& Moth = L[static_cast<int32>(EBREntityKind::Deathmoth)];
		Moth.Name = TEXT("Deathmoths");
		Moth.Number = TEXT("Entit\u00e9 4");
		Moth.Description = TEXT("Des papillons de nuit g\u00e9ants dont la piq\u00fbre est toxique. Ils sont irr\u00e9sistiblement ")
			TEXT("attir\u00e9s par la lumi\u00e8re.");
		Moth.Advice = TEXT("Eteignez votre lampe quand vous entendez des battements d'ailes. Sans lumi\u00e8re, ils se d\u00e9sint\u00e9ressent de vous.");
		Moth.HalfHeight = 30.f; Moth.Radius = 40.f; Moth.bFlying = true; Moth.HoverHeight = 210.f; Moth.WalkSpeed = 220.f;
		Moth.ChaseSpeed = 380.f; Moth.SightRange = 2000.f; Moth.AttackRange = 130.f; Moth.Damage = 12.f; Moth.SanityDamage = 8.f;
		Moth.AttackCooldown = 2.f; Moth.Aura = 0.2f;
		Moth.Voice = TEXT("S_Moth"); Moth.VoiceInterval = 0.f; Moth.VoiceFalloff = 1500.f;

		FBREntityInfo& Wretch = L[static_cast<int32>(EBREntityKind::Wretch)];
		Wretch.Name = TEXT("Wretches");
		Wretch.Number = TEXT("Entit\u00e9 15");
		Wretch.Description = TEXT("D'anciens vagabonds d\u00e9g\u00e9n\u00e9r\u00e9s, d\u00e9charn\u00e9s et vo\u00fbt\u00e9s, priv\u00e9s d'eau ")
			TEXT("d'amande trop longtemps. Lents, mais agressifs.");
		Wretch.Advice = TEXT("Ils sont lents : ne les laissez pas vous acculer, et gardez toujours de l'eau d'amande.");
		Wretch.HalfHeight = 80.f; Wretch.Radius = 30.f; Wretch.WalkSpeed = 90.f; Wretch.ChaseSpeed = 240.f; Wretch.SightRange = 1100.f;
		Wretch.AttackRange = 120.f; Wretch.Damage = 20.f; Wretch.SanityDamage = 15.f; Wretch.AttackCooldown = 2.f; Wretch.Aura = 0.4f;
		Wretch.Voice = TEXT("S_Wretch"); Wretch.VoiceInterval = 8.f;

		FBREntityInfo& Party = L[static_cast<int32>(EBREntityKind::Partygoer)];
		Party.Name = TEXT("Partygoers");
		Party.Number = TEXT("Entit\u00e9 67");
		Party.Description = TEXT("=) Des silhouettes jaunes au sourire dessin\u00e9, qui vous invitent \u00e0 \"faire la f\u00eate\". ")
			TEXT("Personne n'est jamais revenu d'une de leurs f\u00eates.");
		Party.Advice = TEXT("Ils ne bougent que lorsque vous ne les regardez pas. Ne les quittez JAMAIS des yeux en reculant.");
		Party.HalfHeight = 92.f; Party.Radius = 30.f; Party.WalkSpeed = 0.f; Party.ChaseSpeed = 700.f; Party.SightRange = 3000.f;
		Party.AttackRange = 120.f; Party.Damage = 100.f; Party.SanityDamage = 25.f; Party.AttackCooldown = 1.f; Party.Aura = 0.6f;
		Party.Voice = TEXT("S_Partygoer"); Party.VoiceInterval = 9.f;

		FBREntityInfo& Clump = L[static_cast<int32>(EBREntityKind::Clump)];
		Clump.Name = TEXT("Clump");
		Clump.Number = TEXT("Entit\u00e9 5");
		Clump.Description = TEXT("Une masse de chair h\u00e9riss\u00e9e de bras et de jambes fusionn\u00e9s, qui roule et rampe vers ses proies.");
		Clump.Advice = TEXT("Lent dans les virages : utilisez les couloirs pour le semer.");
		Clump.HalfHeight = 60.f; Clump.Radius = 60.f; Clump.WalkSpeed = 100.f; Clump.ChaseSpeed = 300.f; Clump.SightRange = 1300.f;
		Clump.AttackRange = 150.f; Clump.Damage = 40.f; Clump.SanityDamage = 12.f; Clump.AttackCooldown = 1.8f; Clump.Aura = 0.5f;
		Clump.Voice = TEXT("S_Clump"); Clump.VoiceInterval = 6.f;
		return L;
	}();
	const int32 Index = FMath::Clamp(static_cast<int32>(InKind), 0, Infos.Num() - 1);
	return Infos[Index];
}

// =====================================================================================================================

ABREntity::ABREntity()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* M = GetCharacterMovement();
	M->bOrientRotationToMovement = true;
	M->RotationRate = FRotator(0.f, 300.f, 0.f);
	M->BrakingDecelerationWalking = 900.f;
	M->BrakingDecelerationFlying = 700.f;
	M->MaxStepHeight = 35.f;

	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(GetCapsuleComponent());

	Voice = CreateDefaultSubobject<UAudioComponent>(TEXT("Voice"));
	Voice->SetupAttachment(GetCapsuleComponent());
	Voice->bAutoActivate = false;
}

void ABREntity::BeginPlay()
{
	Super::BeginPlay();
	World = ABRWorld::Get(this);
	const FBREntityInfo& I = MyInfo();

	GetCapsuleComponent()->SetCapsuleSize(I.Radius, I.HalfHeight);
	VisualBase = FVector(0.f, 0.f, -I.HalfHeight);
	Visual->SetRelativeLocation(VisualBase);

	UCharacterMovementComponent* M = GetCharacterMovement();
	M->MaxWalkSpeed = I.WalkSpeed;
	M->MaxFlySpeed = I.WalkSpeed;
	if (I.bFlying)
	{
		M->GravityScale = 0.f;
		M->SetMovementMode(MOVE_Flying);
	}

	bHostileVariant = (Kind != EBREntityKind::Faceling) || FMath::FRand() < 0.15f;
	VoiceTimer = FMath::FRandRange(2.f, 6.f);
	if (Kind == EBREntityKind::Smiler || Kind == EBREntityKind::Partygoer)
	{
		M->bOrientRotationToMovement = false;
		SetState(EState::Idle);
	}

	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(I.Voice))
		{
			Voice->SetSound(S);
			Voice->AttenuationSettings = A->Attenuation(I.VoiceFalloff);
			if (I.VoiceInterval <= 0.f)
			{
				Voice->SetVolumeMultiplier(0.8f);
				Voice->Play();
			}
		}
	}
	BuildVisual();
}

void ABREntity::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ABRWorld* W = World.Get())
	{
		W->UnregisterEntity(this);
	}
	Super::EndPlay(Reason);
}

// =====================================================================================================================
// Modele
// =====================================================================================================================

USceneComponent* ABREntity::AddPart(FName MeshName, const FVector& Joint, const FVector& Scale, const FVector& FallbackSize,
	float FallbackDrop, const TMap<FString, FLinearColor>* Tints, bool bUniqueGlow, float GlowScale)
{
	UBRAssets* A = UBRAssets::Get(this);
	USceneComponent* Pivot = NewObject<USceneComponent>(this);
	Pivot->SetupAttachment(Visual);
	Pivot->SetRelativeLocation(Joint);
	Pivot->RegisterComponent();
	PartComponents.Add(Pivot);

	UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
	MeshComp->SetupAttachment(Pivot);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCastShadow(Kind != EBREntityKind::Smiler);
	UStaticMesh* SM = A ? A->Mesh(MeshName) : nullptr;
	if (SM)
	{
		MeshComp->SetStaticMesh(SM);
		MeshComp->SetRelativeScale3D(Scale);
	}
	else if (A && A->Cube() && FallbackSize.X > 0.f)
	{
		MeshComp->SetStaticMesh(A->Cube());
		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, FallbackDrop));
		MeshComp->SetRelativeScale3D(FallbackSize / 100.f);
	}
	MeshComp->RegisterComponent();
	PartComponents.Add(MeshComp);

	if (A)
	{
		if (SM)
		{
			TArray<UMaterialInstanceDynamic*> Glows;
			A->ApplySlots(MeshComp, Tints, bUniqueGlow, &Glows, GlowScale);
			for (UMaterialInstanceDynamic* G : Glows)
			{
				GlowMIDs.Add(G);
			}
		}
		else
		{
			FLinearColor C(0.5f, 0.48f, 0.45f);
			if (Tints && Tints->Num() > 0)
			{
				C = Tints->CreateConstIterator()->Value;
			}
			MeshComp->SetMaterial(0, A->Surface(FBRSurface(TEXT("T_Skin"), C, 100.f)));
		}
	}
	return Pivot;
}

void ABREntity::BuildHumanoid(const TCHAR* Prefix, float Hip, float Shoulder, float ShoulderW, float HipW, float Hunch,
	const TMap<FString, FLinearColor>* Tints, float ArmLen, float LegLen)
{
	const FString P(Prefix);
	AddPart(FName(*(P + TEXT("_Torso"))), FVector(0.f, 0.f, Hip), FVector(1.f), FVector(30.f, 40.f, Shoulder - Hip + 40.f),
		(Shoulder - Hip + 40.f) * 0.5f, Tints);
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		USceneComponent* Arm = AddPart(FName(*(P + TEXT("_Arm"))), FVector(Hunch, Side * ShoulderW, Shoulder), FVector(1.f),
			FVector(10.f, 10.f, ArmLen), -ArmLen * 0.5f, Tints);
		USceneComponent* Leg = AddPart(FName(*(P + TEXT("_Leg"))), FVector(0.f, Side * HipW, Hip), FVector(1.f),
			FVector(13.f, 13.f, LegLen), -LegLen * 0.5f, Tints);
		FLimb LA;
		LA.Pivot = Arm;
		LA.Phase = Side > 0 ? 0.f : PI;
		LA.Amp = -22.f;
		Limbs.Add(LA);
		FLimb LL;
		LL.Pivot = Leg;
		LL.Phase = Side > 0 ? 0.f : PI;
		LL.Amp = 30.f;
		Limbs.Add(LL);
	}
}

void ABREntity::BuildVisual()
{
	TMap<FString, FLinearColor> Tints;
	switch (Kind)
	{
	case EBREntityKind::Smiler:
	{
		AddPart(TEXT("SM_Smiler"), FVector(0.f, 0.f, MyInfo().HalfHeight), FVector(1.f), FVector(70.f, 70.f, 90.f), 0.f, nullptr, true, 0.5f);
		GlowLight = NewObject<UPointLightComponent>(this);
		GlowLight->SetupAttachment(Visual);
		GlowLight->SetRelativeLocation(FVector(70.f, 0.f, MyInfo().HalfHeight));
		GlowLight->SetIntensityUnits(ELightUnits::Lumens);
		GlowLight->SetIntensity(60.f);
		GlowLight->SetAttenuationRadius(260.f);
		GlowLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
		GlowLight->SetCastShadows(false);
		GlowLight->RegisterComponent();
		break;
	}
	case EBREntityKind::Hound:
	{
		Tints.Add(TEXT("Skin"), FLinearColor(0.55f, 0.53f, 0.5f));
		const float Z = 62.f;
		AddPart(TEXT("SM_Hound_Body"), FVector(0.f, 0.f, Z), FVector(1.f), FVector(90.f, 30.f, 25.f), 0.f, &Tints);
		const FVector Joints[4] = { FVector(33.f, 16.f, Z), FVector(33.f, -16.f, Z), FVector(-30.f, 14.f, Z), FVector(-30.f, -14.f, Z) };
		const float Phases[4] = { 0.f, PI, PI, 0.f };
		for (int32 i = 0; i < 4; ++i)
		{
			FLimb L;
			L.Pivot = AddPart(TEXT("SM_Hound_Leg"), Joints[i], FVector(1.f), FVector(10.f, 10.f, Z), -Z * 0.5f, &Tints);
			L.Phase = Phases[i];
			L.Amp = 28.f;
			Limbs.Add(L);
		}
		break;
	}
	case EBREntityKind::Faceling:
	{
		const FLinearColor Shirts[5] = { FLinearColor(0.45f, 0.47f, 0.5f), FLinearColor(0.35f, 0.25f, 0.2f), FLinearColor(0.2f, 0.3f, 0.45f),
			FLinearColor(0.5f, 0.45f, 0.35f), FLinearColor(0.3f, 0.35f, 0.3f) };
		Tints.Add(TEXT("Cloth"), Shirts[FMath::RandRange(0, 4)]);
		Tints.Add(TEXT("Skin"), FLinearColor(0.82f, 0.72f, 0.64f));
		BuildHumanoid(TEXT("SM_Faceling"), 92.f, 145.f, 20.f, 10.f, 0.f, &Tints, 68.f, 92.f);
		break;
	}
	case EBREntityKind::SkinStealer:
		Tints.Add(TEXT("Flesh"), FLinearColor(0.72f, 0.48f, 0.42f));
		BuildHumanoid(TEXT("SM_SkinStealer"), 112.f, 180.f, 23.f, 11.f, 12.f, &Tints, 100.f, 112.f);
		break;
	case EBREntityKind::Wretch:
		Tints.Add(TEXT("Skin"), FLinearColor(0.42f, 0.45f, 0.36f));
		BuildHumanoid(TEXT("SM_Wretch"), 80.f, 122.f, 18.f, 9.f, 28.f, &Tints, 78.f, 80.f);
		break;
	case EBREntityKind::Partygoer:
		BuildHumanoid(TEXT("SM_Partygoer"), 88.f, 142.f, 22.f, 11.f, 0.f, nullptr, 66.f, 88.f);
		break;
	case EBREntityKind::Deathmoth:
	{
		const float Z = MyInfo().HalfHeight;
		AddPart(TEXT("SM_Deathmoth_Body"), FVector(0.f, 0.f, Z), FVector(1.f), FVector(60.f, 20.f, 20.f), 0.f, nullptr);
		// L'aile s'etend d'un cote : on detecte lequel pour la mirer de l'autre
		float Side = -1.f;
		if (UBRAssets* A = UBRAssets::Get(this))
		{
			if (UStaticMesh* Wing = A->Mesh(TEXT("SM_Deathmoth_Wing")))
			{
				Side = Wing->GetBoundingBox().GetCenter().Y >= 0.f ? 1.f : -1.f;
			}
		}
		for (int32 k = 0; k < 2; ++k)
		{
			const float Out = (k == 0) ? 1.f : -1.f; // cote desire (+Y / -Y)
			const float Mirror = Out * Side;
			FLimb L;
			L.Pivot = AddPart(TEXT("SM_Deathmoth_Wing"), FVector(0.f, Out * 8.f, Z + 4.f), FVector(1.f, Mirror, 1.f),
				FVector(60.f, 80.f, 2.f), 0.f, nullptr);
			L.bRoll = true;
			L.Sign = Out;
			L.Amp = 50.f;
			Limbs.Add(L);
		}
		break;
	}
	case EBREntityKind::Clump:
		AddPart(TEXT("SM_Clump"), FVector(0.f, 0.f, MyInfo().HalfHeight), FVector(1.1f), FVector(110.f, 110.f, 100.f), 0.f, nullptr);
		break;
	default:
		break;
	}
}

// =====================================================================================================================
// Tick
// =====================================================================================================================

void ABREntity::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	Life += Dt;
	StateTime += Dt;
	AttackTimer -= Dt;

	if (Vanish >= 0.f)
	{
		Vanish += Dt;
		const float S = FMath::Max(0.01f, 1.f - Vanish / 0.6f);
		Visual->SetRelativeScale3D(FVector(S, S, S));
		if (GlowLight)
		{
			GlowLight->SetIntensity(60.f * S);
		}
		if (Vanish > 0.6f)
		{
			Destroy();
		}
		return;
	}

	bWantsMove = false;
	Think(Dt);
	Animate(Dt);

	// Detection de blocage
	const float Speed = static_cast<float>(GetVelocity().Size());
	if (bWantsMove && Speed < 15.f)
	{
		StuckTimer += Dt;
		if (StuckTimer > 1.5f)
		{
			StuckTimer = 0.f;
			Path.Reset();
			RepathTimer = 0.f;
		}
	}
	else
	{
		StuckTimer = 0.f;
	}

	// Voix
	const FBREntityInfo& I = MyInfo();
	if (I.VoiceInterval > 0.f)
	{
		VoiceTimer -= Dt * (State == EState::Chase ? 2.f : 1.f);
		if (VoiceTimer <= 0.f)
		{
			VoiceTimer = I.VoiceInterval * FMath::FRandRange(0.7f, 1.3f);
			if (State != EState::Frozen)
			{
				PlayVoice();
			}
		}
	}
}

void ABREntity::SetState(EState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	StateTime = 0.f;
	Path.Reset();
	RepathTimer = 0.f;
	if (NewState == EState::Chase)
	{
		LostSight = 0.f;
		const ABRCharacter* P = Cast<ABRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		if (P && FVector::Dist(P->GetActorLocation(), GetActorLocation()) < 2500.f)
		{
			if (UBRAssets* A = UBRAssets::Get(this))
			{
				if (USoundBase* S = A->Sound(TEXT("S_Alert")))
				{
					UGameplayStatics::PlaySound2D(this, S, 0.55f);
				}
			}
		}
		PlayVoice(1.f);
	}
}

void ABREntity::PlayVoice(float Volume)
{
	if (Voice && Voice->Sound && MyInfo().VoiceInterval > 0.f)
	{
		Voice->SetVolumeMultiplier(Volume);
		Voice->SetPitchMultiplier(FMath::FRandRange(0.9f, 1.1f));
		Voice->Play();
	}
}

void ABREntity::StartVanish()
{
	if (Vanish < 0.f)
	{
		Vanish = 0.f;
		GetCharacterMovement()->StopMovementImmediately();
		SetActorEnableCollision(false);
	}
}

// =====================================================================================================================
// Perception & deplacement
// =====================================================================================================================

bool ABREntity::HasLineOfSight(const ABRCharacter* P) const
{
	if (!P || !GetWorld())
	{
		return false;
	}
	const FVector From = GetActorLocation() + FVector(0.f, 0.f, MyInfo().HalfHeight * 0.5f);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BREntityLOS), false, this);
	Q.AddIgnoredActor(P);
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, P->GetEyeLocation(), ECC_Visibility, Q);
}

bool ABREntity::IsLookedAtBy(const ABRCharacter* P, float CosAngle) const
{
	if (!P)
	{
		return false;
	}
	const FVector Dir = (GetActorLocation() - P->GetEyeLocation()).GetSafeNormal();
	return FVector::DotProduct(P->GetViewDirection(), Dir) > CosAngle;
}

bool ABREntity::IsDirectPathClear(const FVector& Goal) const
{
	if (!GetWorld())
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BREntityPath), false, this);
	const FVector Start = GetActorLocation();
	FVector End = Goal;
	End.Z = Start.Z;
	const float R = MyInfo().Radius * 0.8f;
	return !GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(R), Q);
}

void ABREntity::MoveTowards(const FVector& Target, float Speed)
{
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->MaxWalkSpeed = Speed;
	M->MaxFlySpeed = Speed;
	FVector Dir = Target - GetActorLocation();
	const FBREntityInfo& I = MyInfo();
	if (I.bFlying)
	{
		Dir.Z = (I.HoverHeight + FMath::Sin(Life * 1.3f) * 25.f) - GetActorLocation().Z;
	}
	else
	{
		Dir.Z = 0.f;
	}
	if (Dir.SizeSquared() < 100.f)
	{
		return;
	}
	bWantsMove = true;
	AddMovementInput(Dir.GetSafeNormal(), 1.f);
}

void ABREntity::FollowPathTo(const FVector& Goal, float Speed, float Dt)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		MoveTowards(Goal, Speed);
		return;
	}
	const FIntPoint MyCell = W->WorldToCell(GetActorLocation());
	const FIntPoint GoalCell = W->WorldToCell(Goal);
	const float Dist = static_cast<float>(FVector::Dist2D(Goal, GetActorLocation()));
	if (MyCell == GoalCell || (Dist < W->CellSize() * 2.5f && IsDirectPathClear(Goal)))
	{
		Path.Reset();
		MoveTowards(Goal, Speed);
		return;
	}
	RepathTimer -= Dt;
	if (RepathTimer <= 0.f || GoalCell != PathGoal || PathIndex >= Path.Num())
	{
		RepathTimer = 0.7f;
		PathGoal = GoalCell;
		if (!W->FindPath(MyCell, GoalCell, Path, 1500))
		{
			Path.Reset();
			MoveTowards(Goal, Speed);
			return;
		}
		PathIndex = Path.Num() > 1 ? 1 : 0;
	}
	if (Path.IsValidIndex(PathIndex))
	{
		const FVector Wp = W->CellCenter(Path[PathIndex], GetActorLocation().Z);
		if (FVector::Dist2D(Wp, GetActorLocation()) < 70.f)
		{
			++PathIndex;
		}
		MoveTowards(Wp, Speed);
	}
}

void ABREntity::Wander(float Speed, float Dt)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	if (PathIndex >= Path.Num())
	{
		// Petite marche aleatoire sur la grille
		Path.Reset();
		FIntPoint C = W->WorldToCell(GetActorLocation());
		FIntPoint Prev = C;
		Path.Add(C);
		const int32 Steps = FMath::RandRange(4, 9);
		const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		for (int32 s = 0; s < Steps; ++s)
		{
			const int32 Start = FMath::RandRange(0, 3);
			bool bMoved = false;
			for (int32 k = 0; k < 4; ++k)
			{
				const FIntPoint N = C + Dirs[(Start + k) % 4];
				if (N != Prev && W->CanStep(C, N) && W->IsChunkLoaded(W->CellToChunk(N)))
				{
					Prev = C;
					C = N;
					Path.Add(C);
					bMoved = true;
					break;
				}
			}
			if (!bMoved)
			{
				break;
			}
		}
		PathIndex = Path.Num() > 1 ? 1 : 0;
		if (Path.Num() <= 1)
		{
			return;
		}
	}
	const FVector Wp = W->CellCenter(Path[PathIndex], GetActorLocation().Z);
	if (FVector::Dist2D(Wp, GetActorLocation()) < 60.f)
	{
		++PathIndex;
	}
	MoveTowards(Wp, Speed);
}

void ABREntity::FacePlayer(float Dt)
{
	const ABRCharacter* P = Cast<ABRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P)
	{
		return;
	}
	FVector To = P->GetActorLocation() - GetActorLocation();
	To.Z = 0.f;
	if (!To.IsNearlyZero())
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), To.Rotation(), Dt, 5.f));
	}
}

void ABREntity::TryAttack(ABRCharacter* P, float Dist)
{
	if (!P || P->IsDead() || AttackTimer > 0.f)
	{
		return;
	}
	const FBREntityInfo& I = MyInfo();
	if (Dist <= I.AttackRange + 40.f && HasLineOfSight(P))
	{
		AttackTimer = I.AttackCooldown;
		PlayVoice(1.f);
		P->ReceiveAttack(I.Damage, I.SanityDamage, this, I.Name);
	}
}

// =====================================================================================================================
// Comportements
// =====================================================================================================================

void ABREntity::Think(float Dt)
{
	ABRWorld* W = World.Get();
	ABRCharacter* P = Cast<ABRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FBREntityInfo& I = MyInfo();
	if (!W || !P || P->IsDead())
	{
		if (Kind != EBREntityKind::Smiler && Kind != EBREntityKind::Partygoer)
		{
			Wander(I.WalkSpeed, Dt);
		}
		return;
	}

	const FVector PL = P->GetActorLocation();
	const float Dist = static_cast<float>(FVector::Dist(PL, GetActorLocation()));
	const bool bLOS = Dist < I.SightRange && HasLineOfSight(P);
	const bool bSeesPlayer = bLOS;
	const bool bLookedAt = bLOS && IsLookedAtBy(P, 0.82f);
	const bool bHeard = P->GetNoiseRadius() >= Dist;

	if (bLookedAt && Dist < 2500.f)
	{
		W->Discover(Kind);
		bSeenOnce = true;
	}
	if (bLOS && Dist < I.AuraRadius)
	{
		P->AddSanityPressure(I.Aura);
	}

	switch (Kind)
	{
	// ------------------------------------------------------------------ Smilers
	case EBREntityKind::Smiler:
	{
		FacePlayer(Dt);
		const float Light = W->LightLevelAt(GetActorLocation());
		if (Light > 0.5f && State != EState::Chase)
		{
			StartVanish(); // la lumiere les fait fuir
			return;
		}
		const bool bBeamed = P->IsFlashlightOn() && bLOS && Dist < 1600.f && IsLookedAtBy(P, 0.97f);
		if (bBeamed)
		{
			Agitation += Dt * 0.9f;
		}
		else if (P->IsSprinting() && Dist < 1200.f && (bLOS || bHeard))
		{
			Agitation += Dt * 0.7f;
		}
		else
		{
			Agitation = FMath::Max(0.f, Agitation - Dt * 0.25f);
		}

		if (State == EState::Chase)
		{
			P->NotifyChase(1.f);
			FollowPathTo(PL, I.ChaseSpeed, Dt);
			TryAttack(P, Dist);
			const bool bGiveUp = StateTime > 9.f || (!bLOS && StateTime > 4.f) || (!P->IsFlashlightOn() && !P->IsSprinting() && StateTime > 3.f && Dist > 500.f);
			if (bGiveUp)
			{
				StartVanish();
			}
		}
		else
		{
			if (Agitation >= 1.f)
			{
				SetState(EState::Chase);
			}
			else if (bLOS && Dist > 300.f && Dist < 1800.f && !bLookedAt)
			{
				// S'approche sans bruit quand on ne le regarde pas
				const FVector Next = GetActorLocation() + (PL - GetActorLocation()).GetSafeNormal() * 150.f;
				if (W->LightLevelAt(Next) < 0.2f)
				{
					FollowPathTo(PL, I.WalkSpeed, Dt);
				}
			}
			else
			{
				MoveTowards(GetActorLocation(), 0.f);
			}
		}
		break;
	}
	// ------------------------------------------------------------------ Hounds
	case EBREntityKind::Hound:
	{
		const bool bFacing = bLOS && IsLookedAtBy(P, 0.9f);
		switch (State)
		{
		case EState::Retreat:
		{
			const FVector Away = GetActorLocation() + (GetActorLocation() - PL).GetSafeNormal2D() * 600.f;
			FollowPathTo(Away, I.WalkSpeed * 1.6f, Dt);
			if (StateTime > 4.f)
			{
				Intimidation = 0.f;
				SetState(EState::Wander);
			}
			break;
		}
		case EState::Chase:
		{
			P->NotifyChase(1.f);
			FollowPathTo(PL, I.ChaseSpeed, Dt);
			TryAttack(P, Dist);
			if (bFacing && !P->IsSprinting() && Dist < 500.f)
			{
				Intimidation += Dt * 0.5f;
				if (Intimidation > 2.f)
				{
					SetState(EState::Retreat);
				}
			}
			LostSight = (bLOS || bHeard) ? 0.f : LostSight + Dt;
			if (LostSight > 6.f)
			{
				SetState(EState::Wander);
			}
			break;
		}
		case EState::Stalk:
		{
			if (Dist > 700.f)
			{
				FollowPathTo(PL, I.WalkSpeed * 1.25f, Dt);
			}
			else
			{
				GetCharacterMovement()->bOrientRotationToMovement = false;
				FacePlayer(Dt);
			}
			if ((P->IsSprinting() && (bHeard || bLOS)) || (!bFacing && Dist < 650.f && StateTime > 2.5f))
			{
				GetCharacterMovement()->bOrientRotationToMovement = true;
				SetState(EState::Chase);
			}
			else if (bFacing && Dist < 750.f)
			{
				Intimidation += Dt;
				if (Intimidation > 3.f)
				{
					GetCharacterMovement()->bOrientRotationToMovement = true;
					SetState(EState::Retreat);
				}
			}
			else if (!bLOS && !bHeard && StateTime > 10.f)
			{
				GetCharacterMovement()->bOrientRotationToMovement = true;
				SetState(EState::Wander);
			}
			break;
		}
		default:
			Wander(I.WalkSpeed, Dt);
			if ((bLOS && Dist < 1500.f) || bHeard)
			{
				SetState(EState::Stalk);
			}
			break;
		}
		break;
	}
	// ------------------------------------------------------------------ Facelings
	case EBREntityKind::Faceling:
	{
		if (State == EState::Chase)
		{
			P->NotifyChase(0.6f);
			FollowPathTo(PL, I.ChaseSpeed, Dt);
			TryAttack(P, Dist);
			LostSight = bLOS ? 0.f : LostSight + Dt;
			if (LostSight > 6.f)
			{
				SetState(EState::Wander);
			}
		}
		else if (State == EState::Retreat)
		{
			const FVector Away = GetActorLocation() + (GetActorLocation() - PL).GetSafeNormal2D() * 500.f;
			FollowPathTo(Away, I.WalkSpeed * 1.4f, Dt);
			if (StateTime > 4.f)
			{
				SetState(EState::Wander);
			}
		}
		else if (bLOS && Dist < 350.f)
		{
			GetCharacterMovement()->bOrientRotationToMovement = false;
			FacePlayer(Dt);
			if (bHostileVariant && Dist < 300.f)
			{
				GetCharacterMovement()->bOrientRotationToMovement = true;
				SetState(EState::Chase);
			}
			else if (Dist < 220.f)
			{
				GetCharacterMovement()->bOrientRotationToMovement = true;
				SetState(EState::Retreat);
			}
		}
		else
		{
			GetCharacterMovement()->bOrientRotationToMovement = true;
			Wander(I.WalkSpeed, Dt);
		}
		break;
	}
	// ------------------------------------------------------------------ Skin-Stealers
	case EBREntityKind::SkinStealer:
	{
		if (State == EState::Chase)
		{
			P->NotifyChase(1.f);
			FollowPathTo(PL, I.ChaseSpeed, Dt);
			TryAttack(P, Dist);
			LostSight = (bLOS || bHeard) ? 0.f : LostSight + Dt;
			if (LostSight > 8.f)
			{
				SetState(EState::Wander);
			}
		}
		else if (State == EState::Stalk)
		{
			FollowPathTo(PL, I.WalkSpeed * 1.5f, Dt);
			if (bLOS)
			{
				SetState(EState::Chase);
			}
			else if (StateTime > 12.f)
			{
				SetState(EState::Wander);
			}
		}
		else
		{
			Wander(I.WalkSpeed, Dt);
			if (bLOS)
			{
				SetState(EState::Chase);
			}
			else if (bHeard)
			{
				SetState(EState::Stalk);
			}
		}
		break;
	}
	// ------------------------------------------------------------------ Deathmoths
	case EBREntityKind::Deathmoth:
	{
		const bool bAttracted = P->IsFlashlightOn() && bLOS && Dist < 2000.f;
		if (State == EState::Chase)
		{
			P->NotifyChase(0.5f);
			const FVector Wobble(FMath::Sin(Life * 2.1f) * 120.f, FMath::Cos(Life * 1.7f) * 120.f, 0.f);
			FollowPathTo(PL + Wobble, I.ChaseSpeed, Dt);
			TryAttack(P, Dist);
			LostSight = P->IsFlashlightOn() ? 0.f : LostSight + Dt;
			if (LostSight > 2.5f)
			{
				SetState(EState::Wander);
			}
		}
		else
		{
			Wander(I.WalkSpeed, Dt);
			if (bAttracted)
			{
				SetState(EState::Chase);
			}
		}
		break;
	}
	// ------------------------------------------------------------------ Wretches
	case EBREntityKind::Wretch:
	{
		if (State == EState::Chase)
		{
			P->NotifyChase(0.6f);
			FollowPathTo(PL, I.ChaseSpeed, Dt);
			TryAttack(P, Dist);
			LostSight = bLOS ? 0.f : LostSight + Dt;
			if (LostSight > 5.f)
			{
				SetState(EState::Wander);
			}
		}
		else
		{
			Wander(I.WalkSpeed, Dt);
			if (bLOS && Dist < I.SightRange)
			{
				SetState(EState::Chase);
			}
		}
		break;
	}
	// ------------------------------------------------------------------ Partygoers
	case EBREntityKind::Partygoer:
	{
		const bool bObserved = bLOS && IsLookedAtBy(P, 0.72f);
		if (bObserved)
		{
			// Fige comme une statue tant qu'on le regarde
			GetCharacterMovement()->StopMovementImmediately();
			if (State != EState::Frozen)
			{
				SetState(EState::Frozen);
			}
		}
		else if ((bSeenOnce || Dist < 1200.f) && Dist < 3000.f)
		{
			if (State != EState::Chase)
			{
				State = EState::Chase;
				StateTime = 0.f;
			}
			P->NotifyChase(0.7f);
			FollowPathTo(PL, I.ChaseSpeed, Dt);
			FacePlayer(Dt);
			TryAttack(P, Dist);
		}
		else
		{
			FacePlayer(Dt);
		}
		break;
	}
	// ------------------------------------------------------------------ Clump
	case EBREntityKind::Clump:
	{
		if (State == EState::Chase)
		{
			P->NotifyChase(0.7f);
			FollowPathTo(PL, I.ChaseSpeed, Dt);
			TryAttack(P, Dist);
			LostSight = bLOS ? 0.f : LostSight + Dt;
			if (LostSight > 6.f)
			{
				SetState(EState::Wander);
			}
		}
		else
		{
			Wander(I.WalkSpeed, Dt);
			if (bLOS && Dist < I.SightRange)
			{
				SetState(EState::Chase);
			}
		}
		break;
	}
	default:
		break;
	}
	(void)bSeesPlayer;
}

// =====================================================================================================================
// Animation procedurale
// =====================================================================================================================

void ABREntity::Animate(float Dt)
{
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const bool bFrozen = State == EState::Frozen;
	if (!bFrozen)
	{
		AnimTime += Dt * (0.6f + Speed / 100.f * 3.2f);
	}
	const float Gait = FMath::Clamp(Speed / 220.f, 0.f, 1.f);

	switch (Kind)
	{
	case EBREntityKind::Smiler:
	{
		Visual->SetRelativeLocation(VisualBase + FVector(0.f, 0.f, FMath::Sin(Life * 1.5f) * 6.f));
		const bool bBlink = FMath::Fmod(Life + 0.37f * static_cast<float>(GetUniqueID() % 7), 5.3f) < 0.12f;
		const float G = bBlink ? 0.f : (0.85f + 0.15f * FMath::Sin(Life * 13.f));
		for (UMaterialInstanceDynamic* M : GlowMIDs)
		{
			if (M)
			{
				M->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(1.f, 0.96f, 0.86f) * 60.f * G);
			}
		}
		if (GlowLight)
		{
			GlowLight->SetIntensity(60.f * G);
		}
		return;
	}
	case EBREntityKind::Clump:
	{
		Visual->SetRelativeRotation(FRotator(FMath::Sin(Life * 2.3f) * 6.f, FMath::Sin(Life * 1.1f) * 12.f, FMath::Cos(Life * 2.9f) * 6.f));
		const float S = 1.f + 0.05f * FMath::Sin(Life * 5.f);
		Visual->SetRelativeScale3D(FVector(S, S, 1.f / S));
		return;
	}
	case EBREntityKind::Deathmoth:
	{
		Visual->SetRelativeLocation(VisualBase + FVector(0.f, 0.f, FMath::Sin(Life * 3.f) * 8.f));
		for (const FLimb& L : Limbs)
		{
			if (USceneComponent* C = L.Pivot.Get())
			{
				const float Flap = FMath::Sin(Life * 20.f) * L.Amp;
				C->SetRelativeRotation(FRotator(0.f, 0.f, -Flap * L.Sign));
			}
		}
		return;
	}
	default:
		break;
	}

	// Humanoides et Hounds : balancement des membres
	if (bFrozen)
	{
		return;
	}
	for (const FLimb& L : Limbs)
	{
		if (USceneComponent* C = L.Pivot.Get())
		{
			float A = FMath::Sin(AnimTime + L.Phase) * L.Amp * Gait;
			if (Kind == EBREntityKind::Wretch)
			{
				A += FMath::Sin(Life * 17.f + L.Phase) * 3.f; // tremblements
			}
			C->SetRelativeRotation(FRotator(A, 0.f, 0.f));
		}
	}
	const float Bob = FMath::Abs(FMath::Sin(AnimTime)) * 3.f * Gait;
	Visual->SetRelativeLocation(VisualBase + FVector(0.f, 0.f, Bob));
}
