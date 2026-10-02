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
		Smiler.Advice = TEXT("Ne braquez JAMAIS votre lampe sur eux : ils chargent. Eteignez la lumi\u00e8re, ne courez pas et reculez lentement.");
		Smiler.HalfHeight = 50.f; Smiler.Radius = 45.f; Smiler.bFlying = true; Smiler.HoverHeight = 165.f; Smiler.bNeedsDark = true;
		Smiler.WalkSpeed = 110.f; Smiler.ChaseSpeed = 700.f; Smiler.SightRange = 1800.f; Smiler.AttackRange = 110.f;
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
		Faceling.Advice = TEXT("Restez poli et gardez vos distances. Dans les champs, certains se tapissent dans les bl\u00e9s et attrapent ceux qui passent trop pr\u00e8s.");
		Faceling.HalfHeight = 92.f; Faceling.Radius = 30.f; Faceling.WalkSpeed = 115.f; Faceling.ChaseSpeed = 300.f;
		Faceling.SightRange = 1500.f; Faceling.AttackRange = 110.f; Faceling.Damage = 15.f; Faceling.SanityDamage = 5.f;
		Faceling.AttackCooldown = 1.8f; Faceling.Aura = 0.2f; Faceling.AuraRadius = 450.f;
		Faceling.Voice = TEXT("S_Faceling"); Faceling.VoiceInterval = 14.f;

		FBREntityInfo& Skin = L[static_cast<int32>(EBREntityKind::SkinStealer)];
		Skin.Name = TEXT("Skin-Stealers");
		Skin.Number = TEXT("Entit\u00e9 10");
		Skin.Description = TEXT("Une cr\u00e9ature longiligne qui porte la peau de ses victimes et imite leurs voix pour attirer ")
			TEXT("les vagabonds. Au repos, ce n'est qu'une masse de chair translucide ; \u00e0 votre approche, elle prend l'apparence d'un ")
			TEXT("explorateur en combinaison. Extr\u00eamement dangereuse.");
		Skin.Advice = TEXT("M\u00e9fiez-vous des explorateurs qui s'approchent sans rien dire. Ne r\u00e9pondez jamais \u00e0 une voix famili\u00e8re : fuyez.");
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
		Party.Advice = TEXT("Ne soutenez pas leur regard plus de quelques secondes. Pendant les coupures de courant, ils partent \u00e0 la chasse : cachez-vous.");
		Party.HalfHeight = 92.f; Party.Radius = 30.f; Party.WalkSpeed = 70.f; Party.ChaseSpeed = 520.f; Party.SightRange = 3000.f;
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

		FBREntityInfo& Bact = L[static_cast<int32>(EBREntityKind::Bacteria)];
		Bact.Name = TEXT("Bacteria");
		Bact.Number = TEXT("Entit\u00e9 du Niveau 0");
		Bact.Description = TEXT("Une silhouette humano\u00efde d\u00e9mesur\u00e9e, faite de fils torsad\u00e9s comme un squelette de c\u00e2bles. ")
			TEXT("Elle erre dans le Niveau 0 en se tordant et imite des coups frapp\u00e9s aux murs pour attirer les vagabonds.");
		Bact.Advice = TEXT("Si vous entendez frapper, \u00e9loignez-vous. D\u00e8s qu'elle vous voit, cassez la ligne de vue : portes, virages, recoins.");
		Bact.HalfHeight = 108.f; Bact.Radius = 30.f; Bact.WalkSpeed = 140.f; Bact.ChaseSpeed = 490.f; Bact.SightRange = 2600.f;
		Bact.AttackRange = 130.f; Bact.Damage = 60.f; Bact.SanityDamage = 20.f; Bact.AttackCooldown = 1.4f; Bact.Aura = 0.5f;
		Bact.AuraRadius = 900.f; Bact.Voice = TEXT("S_Bacteria"); Bact.VoiceInterval = 7.f; Bact.VoiceFalloff = 3200.f;
		return L;
	}();
	const int32 Index = FMath::Clamp(static_cast<int32>(InKind), 0, Infos.Num() - 1);
	return Infos[Index];
}

FBRHumanoidSpec ABREntity::SpecFor(EBREntityKind InKind)
{
	FBRHumanoidSpec S;
	switch (InKind)
	{
	case EBREntityKind::SkinStealer:
		S = FBRHumanoidSpec::Simple(115.f, 185.f, 22.f, 11.f, 10.f, 45.f, 58.f, 10.f);
		S.ArmPitch = 8.f; S.ElbowPitch = 14.f; S.ArmRoll = 6.f;
		break;
	case EBREntityKind::Wretch:
		S = FBRHumanoidSpec::Simple(82.f, 122.f, 17.f, 9.f, 30.f, 33.f, 41.f, 5.f);
		S.ArmPitch = 18.f; S.ElbowPitch = 22.f; S.ArmAmp = 12.f; S.LegAmp = 20.f;
		break;
	case EBREntityKind::Partygoer:
		S = FBRHumanoidSpec::Simple(85.f, 138.f, 22.f, 11.f, 0.f, 28.f, 43.f, 4.f);
		S.ArmRoll = 8.f; S.ArmAmp = 26.f;
		break;
	case EBREntityKind::Bacteria:
		S = FBRHumanoidSpec::Simple(140.f, 215.f, 19.f, 8.f, 10.f, 62.f, 70.f, 16.f);
		S.ArmPitch = 6.f; S.ElbowPitch = 12.f; S.ArmRoll = 7.f; S.ArmAmp = 26.f; S.LegAmp = 30.f;
		break;
	default: // Faceling
		S = FBRHumanoidSpec::Simple(92.f, 145.f, 19.f, 10.f, 0.f, 30.f, 46.f, 6.f);
		break;
	}
	return S;
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

void ABREntity::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ABREntity, NetKind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ABREntity, NetScale, COND_InitialOnly);
	DOREPLIFETIME(ABREntity, NetState);
	DOREPLIFETIME(ABREntity, Target);
	DOREPLIFETIME(ABREntity, NetChase);
	DOREPLIFETIME(ABREntity, bNetVanish);
}

ABRCharacter* ABREntity::GetChaseTarget() const
{
	return NetChase > 0 ? Target.Get() : nullptr;
}

void ABREntity::SetVisualScale(float Scale)
{
	SetActorScale3D(FVector(Scale));
	NetScale = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Scale * 100.f), 1, 255));
}

void ABREntity::BeginPlay()
{
	Super::BeginPlay();
	World = ABRWorld::Get(this);
	if (HasAuthority())
	{
		NetKind = static_cast<uint8>(Kind);
	}
	else
	{
		// Client : l'espece et la taille arrivent avec l'acteur ; il rejoint la liste du monde
		Kind = static_cast<EBREntityKind>(NetKind);
		if (NetScale != 100)
		{
			SetActorScale3D(FVector(NetScale / 100.f));
		}
		if (ABRWorld* W = World.Get())
		{
			W->RegisterEntity(this);
			bRegistered = true;
		}
	}
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
	TwitchTimer = FMath::FRandRange(0.2f, 1.f);

	switch (Kind)
	{
	case EBREntityKind::Smiler:
	case EBREntityKind::Partygoer:
		SetOrientToMovement(false);
		SetState(EState::Idle);
		break;
	case EBREntityKind::SkinStealer:
		// Commence sous sa forme de masse de chair
		Morph = MorphTarget = 0.f;
		SetState(EState::Idle);
		break;
	case EBREntityKind::Faceling:
	{
		const ABRWorld* W = World.Get();
		if (W && W->Def().Layout == EBRLayout::Open)
		{
			// Niveau 10 : tapi dans les bles
			bHostileVariant = true;
			HideCrouch = 1.f;
			SetState(EState::Hide);
		}
		break;
	}
	default:
		break;
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
	if (!HasAuthority())
	{
		State = static_cast<EState>(NetState); // l'etat du serveur prime sur l'etat initial
		if (bNetVanish)
		{
			StartVanish();
		}
	}
	BuildVisual();
	UpdateMorph(0.f);
}

void ABREntity::OnRep_State()
{
	const EState NewState = static_cast<EState>(NetState);
	if (NewState == State || !HasActorBegunPlay())
	{
		State = NewState;
		return;
	}
	State = NewState;
	StateTime = 0.f;
	OnStateEntered();
}

void ABREntity::OnRep_Vanish()
{
	if (bNetVanish && HasActorBegunPlay())
	{
		StartVanish();
	}
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
// Modele articule
// =====================================================================================================================

USceneComponent* ABREntity::AddPart(FName MeshName, USceneComponent* Parent, const FVector& Joint, const FVector& FallbackSize,
	float FallbackDrop, const TMap<FString, FLinearColor>* Tints, bool bUniqueGlow, float GlowScale)
{
	TArray<UMaterialInstanceDynamic*> Glows;
	USceneComponent* Pivot = BRRig::AddPart(this, Parent ? Parent : Visual.Get(), MeshName, Joint, FallbackSize, FallbackDrop, Tints, PartComponents,
		nullptr, Kind != EBREntityKind::Smiler, bUniqueGlow, GlowScale, &Glows);
	for (UMaterialInstanceDynamic* G : Glows)
	{
		GlowMIDs.Add(G);
	}
	return Pivot;
}

void ABREntity::AddLimb(USceneComponent* Pivot, ELimb Type, float Phase, float Amp, float Sign, const FRotator& Base)
{
	if (!Pivot)
	{
		return;
	}
	FLimb L;
	L.Pivot = Pivot;
	L.Type = Type;
	L.Phase = Phase;
	L.Amp = Amp;
	L.Sign = Sign;
	L.Base = Base;
	Pivot->SetRelativeRotation(Base);
	Limbs.Add(L);
}

FBRHumanoidParts ABREntity::BuildHumanoid(const TCHAR* Prefix, const FBRHumanoidSpec& Spec, const TMap<FString, FLinearColor>* Tints, USceneComponent* Parent)
{
	FBRHumanoidParts P = BRRig::BuildHumanoid(this, Parent ? Parent : Visual.Get(), Prefix, Spec, Tints, PartComponents, Kind != EBREntityKind::Smiler);
	HeadPivot = P.Head;
	for (int32 i = 0; i < 2; ++i)
	{
		const float Sgn = i == 0 ? -1.f : 1.f;
		// Bras en opposition de phase avec la jambe du meme cote ; roulis negatif = vers l'exterieur pour +Y
		const float LegPhase = i == 1 ? 0.f : PI;
		AddLimb(P.UpperArm[i], ELimb::UpperArm, LegPhase + PI, Spec.ArmAmp, Sgn, FRotator(Spec.ArmPitch, 0.f, -Sgn * Spec.ArmRoll));
		AddLimb(P.LowerArm[i], ELimb::LowerArm, LegPhase + PI, Spec.ArmAmp, Sgn, FRotator(Spec.ElbowPitch, 0.f, 0.f));
		AddLimb(P.Thigh[i], ELimb::Thigh, LegPhase, Spec.LegAmp, Sgn, FRotator::ZeroRotator);
		AddLimb(P.Shin[i], ELimb::Shin, LegPhase, Spec.LegAmp * 1.4f, Sgn, FRotator::ZeroRotator);
	}
	// Partygoer : ballon rouge dans la main droite
	if (Kind == EBREntityKind::Partygoer && P.LowerArm[1])
	{
		const float LowerLen = static_cast<float>((Spec.Elbow[1] - Spec.Shoulder[1]).Size()) * 1.4f;
		Balloon = AddPart(TEXT("SM_Partygoer_Balloon"), P.LowerArm[1], FVector(2.f, 0.f, -LowerLen), FVector::ZeroVector, 0.f, nullptr);
		if (Balloon)
		{
			Balloon->SetUsingAbsoluteRotation(true);
		}
	}
	return P;
}

bool ABREntity::BuildBacteriaModel()
{
	// Modele fourni (Tools/Blender/import_user_models.py) : corps, tete et deux longs bras en fils
	UBRAssets* A = UBRAssets::Get(this);
	if (!A || !A->Mesh(TEXT("SM_BacteriaET_Body")))
	{
		return false;
	}
	AddPart(TEXT("SM_BacteriaET_Body"), Visual, FVector::ZeroVector, FVector::ZeroVector, 0.f, nullptr);
	HeadPivot = AddPart(TEXT("SM_BacteriaET_Head"), Visual, FVector(3.32f, -4.97f, 233.29f), FVector::ZeroVector, 0.f, nullptr);
	USceneComponent* ArmL = AddPart(TEXT("SM_BacteriaET_ArmL"), Visual, FVector(0.87f, -4.67f, 238.28f), FVector::ZeroVector, 0.f, nullptr);
	USceneComponent* ArmR = AddPart(TEXT("SM_BacteriaET_ArmR"), Visual, FVector(-0.81f, 11.32f, 231.41f), FVector::ZeroVector, 0.f, nullptr);
	// Bras ramenes un peu vers le corps pour passer les portes ; ils servent de bequilles en marchant
	AddLimb(ArmL, ELimb::UpperArm, 0.f, 16.f, -1.f, FRotator(0.f, 0.f, -6.f));
	AddLimb(ArmR, ELimb::UpperArm, PI, 16.f, 1.f, FRotator(0.f, 0.f, 6.f));
	return true;
}

bool ABREntity::BuildMothModel()
{
	// Papillon de nuit scanne (modele fourni) : corps + deux ailes articulees a leur racine
	UBRAssets* A = UBRAssets::Get(this);
	if (!A || !A->Mesh(TEXT("SM_DeathmothET_Body")))
	{
		return false;
	}
	const float Z = MyInfo().HalfHeight;
	USceneComponent* Body = AddPart(TEXT("SM_DeathmothET_Body"), Visual, FVector(0.f, 0.f, Z), FVector::ZeroVector, 0.f, nullptr);
	USceneComponent* WingL = AddPart(TEXT("SM_DeathmothET_WingL"), Body, FVector(2.53f, -8.f, -5.53f), FVector::ZeroVector, 0.f, nullptr);
	USceneComponent* WingR = AddPart(TEXT("SM_DeathmothET_WingR"), Body, FVector(1.73f, 8.f, -4.66f), FVector::ZeroVector, 0.f, nullptr);
	AddLimb(WingL, ELimb::Wing, 0.f, 45.f, -1.f, FRotator::ZeroRotator);
	AddLimb(WingR, ELimb::Wing, 0.f, 45.f, 1.f, FRotator::ZeroRotator);
	return true;
}

void ABREntity::BuildHound(const TMap<FString, FLinearColor>* Tints)
{
	// Proportions identiques a HOUND dans generate_models.py
	const float Z = 70.f;
	AddPart(TEXT("SM_Hound_Body"), Visual, FVector(0.f, 0.f, Z), FVector(90.f, 30.f, 25.f), 0.f, Tints);
	HeadPivot = AddPart(TEXT("SM_Hound_Head"), Visual, FVector(42.f, 0.f, Z + 9.f), FVector(28.f, 18.f, 20.f), 0.f, Tints);
	struct FLeg
	{
		FVector Joint;
		bool bFront;
		float Phase;
	};
	const FLeg Legs[4] = {
		{ FVector(34.f, 15.f, Z), true, 0.f },
		{ FVector(34.f, -15.f, Z), true, PI },
		{ FVector(-32.f, 14.f, Z), false, PI },
		{ FVector(-32.f, -14.f, Z), false, 0.f },
	};
	for (const FLeg& Leg : Legs)
	{
		const float Side = Leg.Joint.Y > 0.f ? 1.f : -1.f;
		USceneComponent* Up = AddPart(TEXT("SM_Hound_UpperLeg"), Visual, Leg.Joint, FVector(10.f, 10.f, 36.f), -18.f, Tints);
		USceneComponent* Low = AddPart(TEXT("SM_Hound_LowerLeg"), Up, FVector(0.f, 0.f, -36.f), FVector(8.f, 8.f, 40.f), -20.f, Tints);
		// Pattes avant pliees vers l'arriere, pattes arriere vers l'avant, legerement ecartees
		AddLimb(Up, ELimb::HoundUpper, Leg.Phase, 22.f, Leg.bFront ? -1.f : 1.f, FRotator(Leg.bFront ? 25.f : -30.f, 0.f, -Side * 12.f));
		AddLimb(Low, ELimb::HoundLower, Leg.Phase, 20.f, Leg.bFront ? -1.f : 1.f, FRotator(Leg.bFront ? -45.f : 40.f, 0.f, 0.f));
	}
}

void ABREntity::BuildVisual()
{
	TMap<FString, FLinearColor> Tints;
	switch (Kind)
	{
	case EBREntityKind::Smiler:
	{
		AddPart(TEXT("SM_Smiler"), Visual, FVector(0.f, 0.f, MyInfo().HalfHeight), FVector(70.f, 70.f, 90.f), 0.f, nullptr, true, 0.5f);
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
		Tints.Add(TEXT("Skin"), FLinearColor(0.55f, 0.53f, 0.5f));
		BuildHound(&Tints);
		break;
	case EBREntityKind::Faceling:
	{
		const FLinearColor Shirts[5] = { FLinearColor(0.45f, 0.47f, 0.5f), FLinearColor(0.35f, 0.25f, 0.2f), FLinearColor(0.2f, 0.3f, 0.45f),
			FLinearColor(0.5f, 0.45f, 0.35f), FLinearColor(0.3f, 0.35f, 0.3f) };
		Tints.Add(TEXT("Cloth"), Shirts[FMath::RandRange(0, 4)]);
		Tints.Add(TEXT("Skin"), FLinearColor(0.82f, 0.72f, 0.64f));
		BuildHumanoid(TEXT("SM_Faceling"), SpecFor(Kind), &Tints, Visual);
		break;
	}
	case EBREntityKind::SkinStealer:
	{
		Tints.Add(TEXT("Flesh"), FLinearColor(0.72f, 0.48f, 0.42f));
		BodyForm = NewObject<USceneComponent>(this);
		BodyForm->SetupAttachment(Visual);
		BodyForm->RegisterComponent();
		BuildHumanoid(TEXT("SM_SkinStealer"), SpecFor(Kind), &Tints, BodyForm);
		TrueHead = HeadPivot;
		MassForm = AddPart(TEXT("SM_SkinStealer_Mass"), Visual, FVector::ZeroVector, FVector(110.f, 100.f, 90.f), 45.f, &Tints);
		// Deguisement : la vraie combinaison hazmat (modele fourni), sinon la combinaison procedurale
		DisguiseForm = NewObject<USceneComponent>(this);
		DisguiseForm->SetupAttachment(Visual);
		DisguiseForm->RegisterComponent();
		if (BRRig::HasHazmat(this))
		{
			BuildHumanoid(TEXT("SM_Hazmat"), FBRHumanoidSpec::Hazmat(), nullptr, DisguiseForm);
			DisguiseHead = HeadPivot;
		}
		else
		{
			AddPart(TEXT("SM_Hazmat"), DisguiseForm, FVector::ZeroVector, FVector(40.f, 50.f, 180.f), 90.f, nullptr);
		}
		HeadPivot = TrueHead;
		break;
	}
	case EBREntityKind::Wretch:
		Tints.Add(TEXT("Skin"), FLinearColor(0.42f, 0.45f, 0.36f));
		BuildHumanoid(TEXT("SM_Wretch"), SpecFor(Kind), &Tints, Visual);
		break;
	case EBREntityKind::Partygoer:
		BuildHumanoid(TEXT("SM_Partygoer"), SpecFor(Kind), nullptr, Visual);
		break;
	case EBREntityKind::Bacteria:
		if (!BuildBacteriaModel())
		{
			BuildHumanoid(TEXT("SM_Bacteria"), SpecFor(Kind), nullptr, Visual);
		}
		break;
	case EBREntityKind::Deathmoth:
	{
		if (BuildMothModel())
		{
			break;
		}
		const float Z = MyInfo().HalfHeight;
		AddPart(TEXT("SM_Deathmoth_Body"), Visual, FVector(0.f, 0.f, Z), FVector(60.f, 20.f, 20.f), 0.f, nullptr);
		// L'aile s'etend d'un cote : on detecte lequel pour la mirer de l'autre
		float WingSide = -1.f;
		if (UBRAssets* A = UBRAssets::Get(this))
		{
			if (UStaticMesh* Wing = A->Mesh(TEXT("SM_Deathmoth_Wing")))
			{
				WingSide = Wing->GetBoundingBox().GetCenter().Y >= 0.f ? 1.f : -1.f;
			}
		}
		for (int32 k = 0; k < 2; ++k)
		{
			const float Out = (k == 0) ? 1.f : -1.f; // cote desire (+Y / -Y)
			USceneComponent* WingPivot = AddPart(TEXT("SM_Deathmoth_Wing"), Visual, FVector(0.f, Out * 8.f, Z + 4.f), FVector(60.f, 80.f, 2.f), 0.f, nullptr);
			if (WingPivot)
			{
				WingPivot->SetRelativeScale3D(FVector(1.f, Out * WingSide, 1.f));
			}
			AddLimb(WingPivot, ELimb::Wing, 0.f, 50.f, Out, FRotator::ZeroRotator);
		}
		break;
	}
	case EBREntityKind::Clump:
		AddPart(TEXT("SM_Clump"), Visual, FVector(0.f, 0.f, MyInfo().HalfHeight), FVector(110.f, 110.f, 100.f), 0.f, nullptr);
		if (Visual)
		{
			Visual->SetRelativeScale3D(FVector(1.1f));
		}
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
		const float Sc = FMath::Max(0.01f, 1.f - Vanish / 0.6f);
		Visual->SetRelativeScale3D(FVector(Sc, Sc, Sc));
		if (GlowLight)
		{
			GlowLight->SetIntensity(60.f * Sc);
		}
		if (Vanish > 0.6f && HasAuthority())
		{
			Destroy();
		}
		return;
	}

	bWantsMove = false;
	if (HasAuthority())
	{
		Think(Dt);
	}
	else
	{
		if (!bRegistered)
		{
			if (ABRWorld* W = ABRWorld::Get(this))
			{
				World = W;
				W->RegisterEntity(this);
				bRegistered = true;
			}
		}
		UpdateClientState(Dt);
	}
	Animate(Dt);
	UpdateHead(Dt);
	UpdateMorph(Dt);

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
			const bool bSilent = State == EState::Frozen || State == EState::Hide || (Kind == EBREntityKind::SkinStealer && Morph < 0.5f);
			if (!bSilent)
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
	if (HasAuthority())
	{
		NetState = static_cast<uint8>(NewState);
	}
	if (NewState == EState::Chase)
	{
		LostSight = 0.f;
		if (const ABRCharacter* P = Target.Get())
		{
			LastKnown = P->GetActorLocation();
		}
	}
	OnStateEntered();
}

void ABREntity::OnStateEntered()
{
	if (State != EState::Chase)
	{
		return;
	}
	// Chacun entend l'alerte si la poursuite demarre pres de lui
	const APawn* Local = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Local && FVector::Dist(Local->GetActorLocation(), GetActorLocation()) < 2500.f)
	{
		PlaySound2D(TEXT("S_Alert"), 0.55f);
	}
	PlayVoice(1.f);
}

void ABREntity::UpdateClientState(float Dt)
{
	// L'IA tourne sur le serveur : on deduit la pose de l'etat replique
	const ABRCharacter* P = Target.Get();
	const float Dist = P ? static_cast<float>(FVector::Dist(P->GetActorLocation(), GetActorLocation())) : 1e9f;
	const bool bReaching = State == EState::Chase && Dist < 700.f
		&& (Kind == EBREntityKind::Bacteria || Kind == EBREntityKind::SkinStealer || Kind == EBREntityKind::Wretch || Kind == EBREntityKind::Faceling);
	Reach = FMath::FInterpTo(Reach, bReaching ? 1.f : 0.f, Dt, 4.f);
	if (Kind == EBREntityKind::Faceling)
	{
		const bool bHide = State == EState::Hide;
		HideCrouch = FMath::FInterpTo(HideCrouch, bHide ? 1.f : 0.f, Dt, bHide ? 3.f : 8.f);
	}
	if (Kind == EBREntityKind::SkinStealer)
	{
		MorphTarget = State == EState::Idle ? 0.f : 1.f;
	}
}

ABRCharacter* ABREntity::PickTarget(ABRWorld* W) const
{
	TArray<ABRCharacter*> Players;
	W->GetPlayers(Players);
	ABRCharacter* Best = nullptr;
	float BestScore = 1e30f;
	for (ABRCharacter* C : Players)
	{
		if (C->IsDead())
		{
			continue;
		}
		float Score = static_cast<float>(FVector::DistSquared(C->GetActorLocation(), GetActorLocation()));
		if (C->IsHidden())
		{
			Score *= 4.f; // un joueur cache est delaisse pour un autre
		}
		if (C == Target.Get())
		{
			Score *= 0.6f; // garde sa proie tant qu'une autre n'est pas bien plus proche
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = C;
		}
	}
	return Best;
}

void ABREntity::SetOrientToMovement(bool bOrient)
{
	if (UCharacterMovementComponent* M = GetCharacterMovement())
	{
		M->bOrientRotationToMovement = bOrient;
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

void ABREntity::PlaySound2D(FName Sound, float Volume)
{
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(Sound))
		{
			UGameplayStatics::PlaySound2D(this, S, Volume);
		}
	}
}

void ABREntity::StartVanish()
{
	if (Vanish < 0.f)
	{
		if (HasAuthority())
		{
			bNetVanish = true;
		}
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
	if (!P || !GetWorld() || bTargetHidden)
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

void ABREntity::MoveTowards(const FVector& Dest, float Speed)
{
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->MaxWalkSpeed = Speed;
	M->MaxFlySpeed = Speed;
	FVector Dir = Dest - GetActorLocation();
	const FBREntityInfo& I = MyInfo();
	if (I.bFlying)
	{
		Dir.Z = (I.HoverHeight + FMath::Sin(Life * 1.3f) * 25.f) - GetActorLocation().Z;
	}
	else
	{
		Dir.Z = 0.f;
	}
	if (Dir.SizeSquared() < 100.f || Speed <= 0.f)
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
	(void)Dt;
}

void ABREntity::FacePlayer(float Dt)
{
	const ABRCharacter* P = Target.Get();
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
	ABRCharacter* P = W ? PickTarget(W) : nullptr;
	Target = P;
	ChaseOut = 0.f;
	NetChase = 0;
	const FBREntityInfo& I = MyInfo();
	if (!W || !P || P->IsDead())
	{
		Reach = FMath::FInterpTo(Reach, 0.f, Dt, 3.f);
		if (Kind != EBREntityKind::Smiler && Kind != EBREntityKind::Partygoer && State != EState::Hide)
		{
			Wander(I.WalkSpeed, Dt);
		}
		return;
	}

	FSense S;
	S.Dist = static_cast<float>(FVector::Dist(P->GetActorLocation(), GetActorLocation()));
	// Joueur cache (placard, trou dans le mur) : ni vu ni entendu, sauf si on l'a vu s'y glisser juste devant soi
	bTargetHidden = P->IsHidden() && !(State == EState::Chase && S.Dist < 300.f);
	S.bLOS = S.Dist < I.SightRange && HasLineOfSight(P);
	S.bLookedAt = S.bLOS && IsLookedAtBy(P, 0.82f);
	S.bHeard = !bTargetHidden && P->GetNoiseRadius() >= S.Dist;
	if (S.bLOS)
	{
		LastKnown = P->GetActorLocation();
	}

	// (journal, aura et musique de poursuite : calcules chez chaque joueur, voir ABRCharacter::UpdateEntityEffects)

	switch (Kind)
	{
	case EBREntityKind::Smiler:
		ThinkSmiler(W, P, S, Dt);
		break;
	case EBREntityKind::Hound:
		ThinkHound(P, S, Dt);
		break;
	case EBREntityKind::Faceling:
		ThinkFaceling(P, S, Dt);
		break;
	case EBREntityKind::SkinStealer:
		ThinkSkinStealer(P, S, Dt);
		break;
	case EBREntityKind::Deathmoth:
		ThinkDeathmoth(P, S, Dt);
		break;
	case EBREntityKind::Wretch:
		ThinkSimpleHunter(P, S, Dt, 0.6f, 5.f);
		break;
	case EBREntityKind::Clump:
		ThinkSimpleHunter(P, S, Dt, 0.7f, 6.f);
		break;
	case EBREntityKind::Partygoer:
		ThinkPartygoer(W, P, S, Dt);
		break;
	case EBREntityKind::Bacteria:
		ThinkBacteria(W, P, S, Dt);
		break;
	default:
		break;
	}

	// Bras tendus vers le joueur pendant une poursuite
	const bool bReaching = State == EState::Chase && S.Dist < 700.f
		&& (Kind == EBREntityKind::Bacteria || Kind == EBREntityKind::SkinStealer || Kind == EBREntityKind::Wretch || Kind == EBREntityKind::Faceling);
	Reach = FMath::FInterpTo(Reach, bReaching ? 1.f : 0.f, Dt, 4.f);
	NetChase = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(ChaseOut * 255.f), 0, 255));
}

// ------------------------------------------------------------------ Smilers
void ABREntity::ThinkSmiler(ABRWorld* W, ABRCharacter* P, const FSense& S, float Dt)
{
	const FBREntityInfo& I = MyInfo();
	const FVector PL = P->GetActorLocation();
	FacePlayer(Dt);
	const float Light = W->LightLevelAt(GetActorLocation());
	if (Light > 0.5f && State != EState::Chase)
	{
		StartVanish(); // la lumiere les fait fuir
		return;
	}

	// Comme dans Escape Together : un faisceau braque sur lui le fait charger
	const bool bBeamed = P->IsFlashlightOn() && S.bLOS && S.Dist < 1600.f && IsLookedAtBy(P, 0.97f);
	BeamTime = bBeamed ? BeamTime + Dt : FMath::Max(0.f, BeamTime - Dt * 0.5f);
	if (P->IsSprinting() && S.Dist < 1200.f && (S.bLOS || S.bHeard))
	{
		Agitation += Dt * 0.7f;
	}
	else
	{
		Agitation = FMath::Max(0.f, Agitation - Dt * 0.25f);
	}

	if (State == EState::Chase)
	{
		NotifyChase(1.f);
		FollowPathTo(bTargetHidden ? LastKnown : PL, I.ChaseSpeed, Dt);
		TryAttack(P, S.Dist);
		LostSight = S.bLOS ? 0.f : LostSight + Dt;
		const bool bCalm = !P->IsFlashlightOn() && !P->IsSprinting() && StateTime > 3.f && S.Dist > 500.f;
		if (StateTime > 6.f || LostSight > 2.5f || bCalm)
		{
			StartVanish();
		}
		return;
	}

	if (BeamTime > 0.35f || Agitation >= 1.f)
	{
		SetState(EState::Chase);
	}
	else if (S.bLOS && S.Dist > 300.f && S.Dist < 1800.f && !S.bLookedAt)
	{
		// S'approche sans bruit quand on ne le regarde pas, en restant dans l'ombre
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

// ------------------------------------------------------------------ Hounds
void ABREntity::ThinkHound(ABRCharacter* P, const FSense& S, float Dt)
{
	const FBREntityInfo& I = MyInfo();
	const FVector PL = P->GetActorLocation();
	const bool bFacing = S.bLOS && IsLookedAtBy(P, 0.9f);
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
		NotifyChase(1.f);
		FollowPathTo(PL, I.ChaseSpeed, Dt);
		TryAttack(P, S.Dist);
		if (bFacing && !P->IsSprinting() && S.Dist < 500.f)
		{
			Intimidation += Dt * 0.5f;
			if (Intimidation > 2.f)
			{
				SetState(EState::Retreat);
			}
		}
		LostSight = (S.bLOS || S.bHeard) ? 0.f : LostSight + Dt;
		if (LostSight > 6.f)
		{
			SetState(EState::Wander);
		}
		break;
	}
	case EState::Stalk:
	{
		if (S.Dist > 700.f)
		{
			SetOrientToMovement(true);
			FollowPathTo(PL, I.WalkSpeed * 1.25f, Dt);
		}
		else
		{
			SetOrientToMovement(false);
			FacePlayer(Dt);
		}
		if ((P->IsSprinting() && (S.bHeard || S.bLOS)) || (!bFacing && S.Dist < 650.f && StateTime > 2.5f))
		{
			SetOrientToMovement(true);
			SetState(EState::Chase);
		}
		else if (bFacing && S.Dist < 750.f)
		{
			Intimidation += Dt;
			if (Intimidation > 3.f)
			{
				SetOrientToMovement(true);
				SetState(EState::Retreat);
			}
		}
		else if (!S.bLOS && !S.bHeard && StateTime > 10.f)
		{
			SetOrientToMovement(true);
			SetState(EState::Wander);
		}
		break;
	}
	default:
		Wander(I.WalkSpeed, Dt);
		if ((S.bLOS && S.Dist < 1500.f) || S.bHeard)
		{
			SetState(EState::Stalk);
		}
		break;
	}
}

// ------------------------------------------------------------------ Facelings
void ABREntity::ThinkFaceling(ABRCharacter* P, const FSense& S, float Dt)
{
	const FBREntityInfo& I = MyInfo();
	const FVector PL = P->GetActorLocation();
	switch (State)
	{
	case EState::Hide:
	{
		// Tapi dans les bles : se jette sur le joueur s'il passe trop pres
		HideCrouch = FMath::FInterpTo(HideCrouch, 1.f, Dt, 3.f);
		MoveTowards(GetActorLocation(), 0.f);
		if (S.Dist < 260.f && S.bLOS)
		{
			HideCrouch = 0.f;
			PlayVoice(1.f);
			SetState(EState::Chase);
		}
		break;
	}
	case EState::Chase:
	{
		HideCrouch = FMath::FInterpTo(HideCrouch, 0.f, Dt, 8.f);
		NotifyChase(0.6f);
		FollowPathTo(PL, I.ChaseSpeed, Dt);
		TryAttack(P, S.Dist);
		LostSight = S.bLOS ? 0.f : LostSight + Dt;
		const bool bGiveUp = LostSight > 6.f || (HideCrouch < 0.01f && World.IsValid() && World->Def().Layout == EBRLayout::Open && StateTime > 7.f);
		if (bGiveUp)
		{
			SetState(World.IsValid() && World->Def().Layout == EBRLayout::Open ? EState::Hide : EState::Wander);
		}
		break;
	}
	case EState::Retreat:
	{
		const FVector Away = GetActorLocation() + (GetActorLocation() - PL).GetSafeNormal2D() * 500.f;
		FollowPathTo(Away, I.WalkSpeed * 1.4f, Dt);
		if (StateTime > 4.f)
		{
			SetState(EState::Wander);
		}
		break;
	}
	default:
		if (S.bLOS && S.Dist < 350.f)
		{
			SetOrientToMovement(false);
			FacePlayer(Dt);
			if (bHostileVariant && S.Dist < 300.f)
			{
				SetOrientToMovement(true);
				SetState(EState::Chase);
			}
			else if (S.Dist < 220.f)
			{
				SetOrientToMovement(true);
				SetState(EState::Retreat);
			}
		}
		else
		{
			SetOrientToMovement(true);
			Wander(I.WalkSpeed, Dt);
		}
		break;
	}
}

// ------------------------------------------------------------------ Skin-Stealers
void ABREntity::ThinkSkinStealer(ABRCharacter* P, const FSense& S, float Dt)
{
	const FBREntityInfo& I = MyInfo();
	const FVector PL = P->GetActorLocation();
	switch (State)
	{
	case EState::Idle:
		// Masse de chair immobile ; prend forme humaine quand une proie approche
		MorphTarget = 0.f;
		MoveTowards(GetActorLocation(), 0.f);
		if ((S.bLOS && S.Dist < 1800.f) || S.bHeard)
		{
			MorphTarget = 1.f;
			SetState(EState::Lure);
		}
		break;
	case EState::Lure:
	{
		// Imite un explorateur en combinaison : marche calmement vers le joueur
		MorphTarget = 1.f;
		if (Morph < 0.95f)
		{
			MoveTowards(GetActorLocation(), 0.f);
			break;
		}
		FollowPathTo(PL, I.WalkSpeed * 0.8f, Dt);
		EyeContact = S.bLookedAt ? EyeContact + Dt : FMath::Max(0.f, EyeContact - Dt * 0.5f);
		if (S.Dist < 500.f || EyeContact > 2.5f)
		{
			SetState(EState::Chase);
		}
		LostSight = (S.bLOS || S.bHeard) ? 0.f : LostSight + Dt;
		if (LostSight > 10.f)
		{
			SetState(EState::Idle);
		}
		break;
	}
	case EState::Chase:
		MorphTarget = 1.f;
		NotifyChase(1.f);
		FollowPathTo(S.bLOS ? PL : LastKnown, I.ChaseSpeed, Dt);
		TryAttack(P, S.Dist);
		LostSight = (S.bLOS || S.bHeard) ? 0.f : LostSight + Dt;
		if (LostSight > 8.f)
		{
			SetState(EState::Lure);
		}
		break;
	default:
		SetState(EState::Idle);
		break;
	}
}

// ------------------------------------------------------------------ Deathmoths
void ABREntity::ThinkDeathmoth(ABRCharacter* P, const FSense& S, float Dt)
{
	const FBREntityInfo& I = MyInfo();
	const bool bAttracted = P->IsFlashlightOn() && S.bLOS && S.Dist < 2000.f;
	if (State == EState::Chase)
	{
		NotifyChase(0.5f);
		const FVector Wobble(FMath::Sin(Life * 2.1f) * 120.f, FMath::Cos(Life * 1.7f) * 120.f, 0.f);
		FollowPathTo(P->GetActorLocation() + Wobble, I.ChaseSpeed, Dt);
		TryAttack(P, S.Dist);
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
}

// ------------------------------------------------------------------ Wretches, Clump
void ABREntity::ThinkSimpleHunter(ABRCharacter* P, const FSense& S, float Dt, float ChaseIntensity, float GiveUpTime)
{
	const FBREntityInfo& I = MyInfo();
	if (State == EState::Chase)
	{
		NotifyChase(ChaseIntensity);
		FollowPathTo(S.bLOS ? P->GetActorLocation() : LastKnown, I.ChaseSpeed, Dt);
		TryAttack(P, S.Dist);
		LostSight = S.bLOS ? 0.f : LostSight + Dt;
		if (LostSight > GiveUpTime)
		{
			SetState(EState::Wander);
		}
	}
	else
	{
		Wander(I.WalkSpeed, Dt);
		if (S.bLOS && S.Dist < I.SightRange)
		{
			SetState(EState::Chase);
		}
	}
}

// ------------------------------------------------------------------ Partygoers
void ABREntity::ThinkPartygoer(ABRWorld* W, ABRCharacter* P, const FSense& S, float Dt)
{
	const FBREntityInfo& I = MyInfo();
	const FVector PL = P->GetActorLocation();
	if (State == EState::Chase)
	{
		SetOrientToMovement(true);
		NotifyChase(0.9f);
		FollowPathTo(S.bLOS ? PL : LastKnown, I.ChaseSpeed * (W->IsBlackout() ? 1.1f : 1.f), Dt);
		TryAttack(P, S.Dist);
		LostSight = S.bLOS ? 0.f : LostSight + Dt;
		if (LostSight > 7.f && !W->IsBlackout())
		{
			EyeContact = 0.f;
			bWarned = false;
			SetState(EState::Idle);
		}
		return;
	}

	// Reste immobile a sourire... et compte combien de temps on soutient son regard
	EyeContact = (S.bLOS && IsLookedAtBy(P, 0.9f) && S.Dist < 2200.f) ? EyeContact + Dt : FMath::Max(0.f, EyeContact - Dt * 0.4f);
	if (EyeContact > 1.5f && !bWarned)
	{
		bWarned = true;
		PlayVoice(1.f); // "=)"
	}
	const bool bHunt = EyeContact > 3.f || (W->IsBlackout() && S.Dist < 3000.f) || (S.bLOS && S.Dist < 300.f);
	if (bHunt)
	{
		SetState(EState::Chase);
		return;
	}
	if (S.bLOS)
	{
		SetOrientToMovement(false);
		FacePlayer(Dt);
		MoveTowards(GetActorLocation(), 0.f);
	}
	else
	{
		SetOrientToMovement(true);
		Wander(I.WalkSpeed, Dt);
	}
}

// ------------------------------------------------------------------ Bacteria
void ABREntity::ThinkBacteria(ABRWorld* W, ABRCharacter* P, const FSense& S, float Dt)
{
	const FBREntityInfo& I = MyInfo();
	const FVector PL = P->GetActorLocation();
	const float Sight = I.SightRange * (W->IsBlackout() ? 0.6f : 1.f);

	// Elle ne repere pas le joueur d'un coup : la suspicion monte tant qu'elle le voit (vite s'il est proche,
	// s'il court, s'il l'eclaire ; lentement s'il est dans son dos). Le joueur a le temps de la voir et de se cacher.
	float Rate = 0.f;
	if (S.bLOS && S.Dist < Sight)
	{
		const float Near = 1.f - FMath::Clamp(S.Dist / Sight, 0.f, 1.f);
		Rate = 0.12f + 1.2f * Near * Near + (P->IsSprinting() ? 0.8f : 0.f) + ((P->IsFlashlightOn() && S.bLookedAt) ? 0.4f : 0.f);
		const FVector ToPlayer = (PL - GetActorLocation()).GetSafeNormal2D();
		if (FVector::DotProduct(GetActorForwardVector(), ToPlayer) < 0.f)
		{
			Rate *= 0.35f;
		}
		if (S.Dist < 350.f)
		{
			Rate = 10.f;
		}
	}
	if (S.bHeard && S.Dist < 1200.f)
	{
		Rate += 0.9f;
	}
	Suspicion = Rate > 0.f ? FMath::Min(1.f, Suspicion + Rate * Dt) : FMath::Max(0.f, Suspicion - 0.15f * Dt);

	switch (State)
	{
	case EState::Chase:
	{
		NotifyChase(1.f);
		// Poursuite implacable tant qu'elle voit le joueur, puis fouille sa derniere position connue
		FollowPathTo(S.bLOS ? PL : LastKnown, I.ChaseSpeed, Dt);
		TryAttack(P, S.Dist);
		LostSight = S.bLOS ? 0.f : LostSight + Dt;
		if (LostSight > 5.f)
		{
			SetState(EState::Stalk);
		}
		break;
	}
	case EState::Stalk:
		// Fouille les environs de la derniere position connue
		FollowPathTo(LastKnown, I.WalkSpeed * 1.4f, Dt);
		if (Suspicion >= 1.f)
		{
			SetState(EState::Chase);
		}
		else if (StateTime > 8.f || FVector::Dist2D(LastKnown, GetActorLocation()) < 120.f)
		{
			Suspicion = 0.3f;
			bHasPatrolGoal = false;
			SetState(EState::Wander);
		}
		break;
	default:
		if (Suspicion >= 1.f)
		{
			SetState(EState::Chase);
			break;
		}
		if (Suspicion > 0.45f)
		{
			// "Elle m'a vu ?" : elle s'arrete et tourne la tete vers le joueur
			MoveTowards(GetActorLocation(), 0.f);
			FacePlayer(Dt);
			break;
		}
		// Rondes autour du joueur
		PatrolTime += Dt;
		if (!bHasPatrolGoal || PatrolTime > 25.f || FVector::Dist2D(PatrolGoal, GetActorLocation()) < 150.f)
		{
			PickPatrolGoal(W, P);
		}
		if (bHasPatrolGoal)
		{
			FollowPathTo(PatrolGoal, I.WalkSpeed, Dt);
		}
		else
		{
			Wander(I.WalkSpeed, Dt);
		}
		break;
	}
}

void ABREntity::PickPatrolGoal(ABRWorld* W, const ABRCharacter* P)
{
	PatrolTime = 0.f;
	bHasPatrolGoal = false;
	if (!W || !P || !GetWorld())
	{
		return;
	}
	++PatrolLeg;
	const FVector PL = P->GetActorLocation();
	auto Usable = [W](const FIntPoint& C)
	{
		return W->IsWalkable(C) && W->IsChunkLoaded(W->CellToChunk(C)) && !W->IsPoolCell(C.X, C.Y);
	};

	// Un passage sur trois traverse le champ de vision du joueur, a 9-13 m, dans une salle qu'il voit
	if (PatrolLeg % 3 == 0)
	{
		const FVector Fwd = P->GetViewDirection().GetSafeNormal2D();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRPatrolView), false, P);
		Q.AddIgnoredActor(this);
		for (int32 Try = 0; Try < 12; ++Try)
		{
			const FVector Dir = FRotator(0.f, FMath::FRandRange(-35.f, 35.f), 0.f).RotateVector(Fwd);
			const FIntPoint C = W->WorldToCell(PL + Dir * FMath::FRandRange(900.f, 1300.f));
			if (!Usable(C))
			{
				continue;
			}
			const FVector Spot = W->CellCenter(C, PL.Z);
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, P->GetEyeLocation(), Spot + FVector(0.f, 0.f, 60.f), ECC_Visibility, Q))
			{
				PatrolGoal = Spot;
				bHasPatrolGoal = true;
				return;
			}
		}
	}

	// Sinon : un point d'une ronde de 10 a 16 m autour du joueur, en tournant toujours dans le meme sens
	if (PatrolLeg == 1)
	{
		const FVector From = GetActorLocation() - PL;
		PatrolAngle = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(From.Y), static_cast<float>(From.X)));
		PatrolDir = FMath::FRand() < 0.5f ? -1.f : 1.f;
	}
	PatrolAngle += FMath::FRandRange(35.f, 70.f) * PatrolDir;
	for (int32 Try = 0; Try < 12; ++Try)
	{
		const float Ang = FMath::DegreesToRadians(PatrolAngle + FMath::FRandRange(-20.f, 20.f));
		const FIntPoint C = W->WorldToCell(PL + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * FMath::FRandRange(1000.f, 1600.f));
		if (Usable(C))
		{
			PatrolGoal = W->CellCenter(C, PL.Z);
			bHasPatrolGoal = true;
			return;
		}
	}
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
		// Le sourire s'illumine davantage quand il est sur le point de charger
		const float Rage = State == EState::Chase ? 1.6f : 1.f + FMath::Clamp(BeamTime * 2.f, 0.f, 0.6f);
		const float G = bBlink ? 0.f : (0.85f + 0.15f * FMath::Sin(Life * 13.f)) * Rage;
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
		const float Sc = 1.1f * (1.f + 0.05f * FMath::Sin(Life * 5.f));
		Visual->SetRelativeScale3D(FVector(Sc, Sc, 1.21f / Sc));
		return;
	}
	case EBREntityKind::Deathmoth:
		Visual->SetRelativeLocation(VisualBase + FVector(0.f, 0.f, FMath::Sin(Life * 3.f) * 8.f));
		AnimateLimbs(Dt, Gait);
		return;
	default:
		break;
	}

	if (!bFrozen)
	{
		AnimateLimbs(Dt, Gait);
	}

	// Rebond de la marche, accroupissement (Faceling), palpitation de la masse (Skin-Stealer)
	const float Bob = FMath::Abs(FMath::Sin(AnimTime)) * 3.f * Gait;
	Visual->SetRelativeLocation(VisualBase + FVector(0.f, 0.f, Bob - HideCrouch * 70.f));
	if (Kind == EBREntityKind::Faceling)
	{
		Visual->SetRelativeRotation(FRotator(-HideCrouch * 25.f, 0.f, 0.f));
	}
	if (MassForm)
	{
		const float Pulse = 1.f + 0.06f * FMath::Sin(Life * 3.1f) + 0.03f * FMath::Sin(Life * 7.3f);
		const float M = FMath::Max(0.01f, 1.f - Morph);
		MassForm->SetRelativeScale3D(FVector(Pulse * M, Pulse * M, M / Pulse));
	}
	if (Balloon)
	{
		// Le ballon flotte et se balance doucement
		Balloon->SetWorldRotation(FRotator(FMath::Sin(Life * 0.9f) * 5.f, GetActorRotation().Yaw, FMath::Cos(Life * 1.3f) * 5.f));
	}
}

void ABREntity::AnimateLimbs(float Dt, float Gait)
{
	// Spasmes (Bacteria, Wretch)
	TwitchTimer -= Dt;
	if (TwitchTimer <= 0.f)
	{
		const bool bTwitchy = Kind == EBREntityKind::Bacteria || Kind == EBREntityKind::Wretch;
		TwitchTimer = bTwitchy ? FMath::FRandRange(0.08f, 0.7f) : 1.f;
		Twitch = bTwitchy ? FRotator(FMath::FRandRange(-12.f, 12.f), FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-25.f, 25.f)) : FRotator::ZeroRotator;
	}

	for (const FLimb& L : Limbs)
	{
		USceneComponent* C = L.Pivot.Get();
		if (!C)
		{
			continue;
		}
		const float Swing = FMath::Sin(AnimTime + L.Phase);
		const float Lift = FMath::Max(0.f, FMath::Cos(AnimTime + L.Phase));
		FRotator R = L.Base;
		switch (L.Type)
		{
		case ELimb::UpperArm:
			R.Pitch += L.Amp * Swing * Gait * (1.f - Reach) + Reach * 75.f;
			R.Roll += Reach * L.Sign * 8.f;
			if (Kind == EBREntityKind::Bacteria || Kind == EBREntityKind::Wretch)
			{
				R.Pitch += Twitch.Pitch * 0.4f * L.Sign + FMath::Sin(Life * 17.f + L.Phase) * 2.5f;
			}
			break;
		case ELimb::LowerArm:
			R.Pitch += (10.f * FMath::Max(0.f, Swing)) * Gait + Reach * 10.f;
			break;
		case ELimb::Thigh:
			R.Pitch += L.Amp * Swing * Gait;
			break;
		case ELimb::Shin:
			R.Pitch -= L.Amp * Lift * Gait; // le genou plie vers l'arriere
			break;
		case ELimb::HoundUpper:
			R.Pitch += L.Amp * Swing * Gait;
			break;
		case ELimb::HoundLower:
			R.Pitch += L.Sign * L.Amp * Lift * Gait;
			break;
		case ELimb::Wing:
		{
			const float Flap = FMath::Sin(Life * 20.f) * L.Amp;
			R = FRotator(0.f, 0.f, -Flap * L.Sign);
			break;
		}
		}
		C->SetRelativeRotation(R);
	}
}

void ABREntity::UpdateHead(float Dt)
{
	USceneComponent* Head = HeadPivot.Get();
	if (!Head || !Visual)
	{
		return;
	}
	FRotator Want = FRotator::ZeroRotator;
	// La tete suit sa proie (ou, chez un client, le joueur local s'il n'y en a pas)
	const ABRCharacter* P = Target.Get() ? Target.Get() : Cast<ABRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const bool bInterested = State != EState::Wander && State != EState::Idle && State != EState::Hide;
	const float Dist = P ? static_cast<float>(FVector::Dist(P->GetActorLocation(), GetActorLocation())) : 1e9f;
	if (P && (bInterested || Dist < 1500.f) && State != EState::Frozen)
	{
		// La tete suit le joueur (dans les limites du cou)
		const FTransform& VT = Visual->GetComponentTransform();
		const FVector Local = VT.InverseTransformPosition(P->GetEyeLocation()) - Head->GetRelativeLocation();
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X)));
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Z), static_cast<float>(Local.Size2D())));
		Want = FRotator(FMath::Clamp(Pitch, -40.f, 35.f), FMath::Clamp(Yaw, -75.f, 75.f), 0.f);
	}
	if (Kind == EBREntityKind::Hound && State == EState::Stalk)
	{
		Want.Pitch -= 15.f; // tete basse, a l'affut
	}
	if (Kind == EBREntityKind::Bacteria || Kind == EBREntityKind::Wretch)
	{
		Want += Twitch * (Kind == EBREntityKind::Bacteria ? 1.f : 0.5f);
	}
	if (Kind == EBREntityKind::Partygoer && State != EState::Chase)
	{
		Want.Roll += FMath::Sin(Life * 0.7f) * 12.f; // tete penchee, "amicale"
	}
	const float Speed = (Kind == EBREntityKind::Bacteria) ? 14.f : 5.f;
	HeadRot = FMath::RInterpTo(HeadRot, Want, Dt, Speed);
	Head->SetRelativeRotation(HeadRot);
}

void ABREntity::UpdateMorph(float Dt)
{
	if (Kind != EBREntityKind::SkinStealer || !BodyForm)
	{
		return;
	}
	const float Prev = Morph;
	Morph = Dt > 0.f ? FMath::FInterpConstantTo(Morph, MorphTarget, Dt, 1.25f) : MorphTarget;
	if (Morph > 0.f && Prev <= 0.f && MorphTarget > 0.5f)
	{
		PlayVoice(0.8f); // bruit de chair qui se dechire
	}
	// Deguise en explorateur pendant l'approche, vraie forme pendant la chasse
	const bool bWantDisguise = State == EState::Lure && DisguiseForm != nullptr;
	if (bWantDisguise != bDisguised)
	{
		if (bDisguised && Morph > 0.5f)
		{
			Morph = 0.3f; // la combinaison se dechire : la vraie forme jaillit
			PlaySound2D(TEXT("S_Alert"), 0.6f);
			PlayVoice(1.f);
		}
		bDisguised = bWantDisguise;
		HeadPivot = (bDisguised && DisguiseHead.IsValid()) ? DisguiseHead : TrueHead;
	}
	// Le corps "pousse" hors de la masse
	const float M = FMath::Max(0.01f, Morph);
	USceneComponent* Shown = bDisguised ? DisguiseForm.Get() : BodyForm.Get();
	USceneComponent* Other = bDisguised ? BodyForm.Get() : DisguiseForm.Get();
	if (Shown)
	{
		Shown->SetRelativeScale3D(FVector(0.5f + 0.5f * M, 0.5f + 0.5f * M, M));
		Shown->SetVisibility(Morph > 0.02f, true);
	}
	if (Other)
	{
		Other->SetVisibility(false, true);
	}
	if (MassForm)
	{
		MassForm->SetVisibility(Morph < 0.98f, true);
	}
}
