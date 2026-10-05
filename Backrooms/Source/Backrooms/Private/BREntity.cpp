#include "BREntity.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRWorld.h"

#include "AIController.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
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
		// Un peu moins rapide qu'un sprint (470) : on peut la semer en cassant la ligne de vue, pas en restant sur place
		Bact.HalfHeight = 108.f; Bact.Radius = 30.f; Bact.WalkSpeed = 140.f; Bact.ChaseSpeed = 440.f; Bact.SightRange = 2600.f;
		Bact.AttackRange = 130.f; Bact.Damage = 60.f; Bact.SanityDamage = 20.f; Bact.AttackCooldown = 1.4f; Bact.Aura = 0.5f;
		Bact.AuraRadius = 900.f; Bact.Voice = TEXT("S_Bacteria"); Bact.VoiceInterval = 0.f; Bact.VoiceFalloff = 3200.f; // v4.4 : ses cris en continu (enregistrement fourni)

		// v4.7 : attaque en trois temps et demarche, espece par espece.
		// Preparation, fenetre d'impact, recuperation (s) ; allonge de la fente (cm) ; vitesse gardee pendant la
		// preparation ; acceleration, freinage (cm/s2) ; rotation (deg/s)
		auto Tune = [&L](EBREntityKind K, float Windup, float Impact, float Recover, float Lunge, float Move, float Accel, float Brake, float Turn)
		{
			FBREntityInfo& E = L[static_cast<int32>(K)];
			E.WindupTime = Windup;
			E.ImpactWindow = Impact;
			E.RecoveryTime = Recover;
			E.LungeReach = Lunge;
			E.WindupMove = Move;
			E.Acceleration = Accel;
			E.Braking = Brake;
			E.TurnRate = Turn;
		};
		// Smiler : coup mortel, mais le sourire s'ouvre avant ; on peut encore reculer dans la lumiere
		Tune(EBREntityKind::Smiler, 0.40f, 0.15f, 0.45f, 30.f, 0.15f, 2000.f, 2200.f, 400.f);
		// Hound : se tasse, puis bondit loin ; foulees puissantes, virages larges
		Tune(EBREntityKind::Hound, 0.30f, 0.18f, 0.55f, 70.f, 0.f, 2600.f, 1400.f, 240.f);
		Tune(EBREntityKind::Faceling, 0.50f, 0.15f, 0.60f, 15.f, 0.3f, 700.f, 1200.f, 200.f);
		// Skin-Stealer : trompeur, la preparation est courte, il continue d'avancer pendant qu'il arme
		Tune(EBREntityKind::SkinStealer, 0.30f, 0.16f, 0.65f, 45.f, 0.4f, 900.f, 1800.f, 300.f);
		Tune(EBREntityKind::Deathmoth, 0.45f, 0.15f, 0.60f, 30.f, 0.3f, 1400.f, 900.f, 260.f);
		// Wretch : epuise, geste lent et long a recuperer
		Tune(EBREntityKind::Wretch, 0.65f, 0.18f, 0.90f, 10.f, 0.2f, 420.f, 700.f, 110.f);
		Tune(EBREntityKind::Partygoer, 0.45f, 0.15f, 0.50f, 35.f, 0.2f, 1800.f, 1500.f, 360.f);
		// Clump : lourd, s'ecrase en avant sur ses bras d'appui
		Tune(EBREntityKind::Clump, 0.60f, 0.20f, 0.85f, 25.f, 0.1f, 500.f, 2600.f, 80.f);
		// Bacteria : saccadee, arrets et departs secs, demi-tours brusques
		Tune(EBREntityKind::Bacteria, 0.32f, 0.15f, 0.50f, 45.f, 0.1f, 3200.f, 5000.f, 540.f);
		return L;
	}();
	const int32 Index = FMath::Clamp(static_cast<int32>(InKind), 0, Infos.Num() - 1);
	return Infos[Index];
}

int32 ABREntity::StatWindups = 0;
int32 ABREntity::StatHits = 0;
int32 ABREntity::StatMisses = 0;
float ABREntity::StatLastHitDelay = -1.f;

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
	// v4.7 : demarche propre a l'espece (departs, arrets, virages)
	M->MaxAcceleration = I.Acceleration;
	M->BrakingDecelerationWalking = I.Braking;
	M->RotationRate = FRotator(0.f, I.TurnRate, 0.f);
	if (I.bFlying)
	{
		M->GravityScale = 0.f;
		M->SetMovementMode(MOVE_Flying);
	}
	else if (const ABRWorld* PW = World.Get())
	{
		// v4.6 : niveau a fosses : le deplacement refuse de quitter le sol (dernier garde-fou apres le chemin et le pilotage)
		M->bCanWalkOffLedges = !PW->HasPits();
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
		FLinearColor Base = FLinearColor::White;
		G->GetVectorParameterValue(FHashedMaterialParameterInfo(FName(TEXT("Emissive"))), Base);
		GlowMIDs.Add(G);
		GlowBase.Add(Base);
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

FBRHumanoidParts ABREntity::BuildHumanoid(const TCHAR* Prefix, const FBRHumanoidSpec& Spec, const TMap<FString, FLinearColor>* Tints, USceneComponent* Parent,
	const TCHAR* SkinName)
{
	USceneComponent* Root = Parent ? Parent : Visual.Get();
	UBRAssets* A = UBRAssets::Get(this);
	USkeletalMesh* SkinMesh = (A && SkinName) ? A->SkeletalMesh(SkinName) : nullptr;
	FBRHumanoidParts P;
	bool bSkinned = false;
	if (SkinMesh)
	{
		// v4.5 : modele fourni d'un seul tenant (geometrie et poids d'origine), anime par les memes pivots
		FBRSkinDriver Driver;
		P = BRRig::BuildSkinnedHumanoid(this, Root, SkinMesh, Tints, PartComponents, Driver, Kind != EBREntityKind::Smiler);
		bSkinned = P.Head && P.UpperArm[0] && P.UpperArm[1] && P.Thigh[0] && P.Thigh[1];
		if (bSkinned)
		{
			SkinDrivers.Add(Driver);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Backrooms : %s n'a pas les os attendus (Head, LeftArm, LeftUpLeg...) : pieces rigides utilisees"), SkinName);
			for (UPrimitiveComponent* M : P.Meshes)
			{
				if (M)
				{
					M->DestroyComponent();
				}
			}
			P = FBRHumanoidParts();
		}
	}
	if (!bSkinned)
	{
		P = BRRig::BuildHumanoid(this, Root, Prefix, Spec, Tints, PartComponents, Kind != EBREntityKind::Smiler);
	}
	HeadPivot = P.Head;
	if (P.Thigh[0])
	{
		LegLength = FMath::Max(30.f, static_cast<float>(P.Thigh[0]->GetRelativeLocation().Z));
	}
	if (bSkinned && P.Torso)
	{
		TorsoPivots.Add(P.Torso);
	}
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
		const TCHAR* BalloonMesh = BRRig::HasMesh(this, TEXT("SM_PartygoerET_Balloon")) ? TEXT("SM_PartygoerET_Balloon") : TEXT("SM_Partygoer_Balloon");
		Balloon = AddPart(BalloonMesh, P.LowerArm[1], FVector(2.f, 0.f, -LowerLen), FVector::ZeroVector, 0.f, nullptr);
		if (Balloon)
		{
			if (bSkinned && P.Meshes.Num() > 0 && P.Meshes[0])
			{
				// La ficelle est tenue par l'os de la main (il suit le poignet, le coude et l'epaule)
				Balloon->AttachToComponent(P.Meshes[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("RightHand"));
			}
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
	USceneComponent* WingL = AddPart(TEXT("SM_DeathmothET_WingL"), Body, FVector(-1.7f, -8.f, -4.88f), FVector::ZeroVector, 0.f, nullptr);
	USceneComponent* WingR = AddPart(TEXT("SM_DeathmothET_WingR"), Body, FVector(-0.79f, 8.f, -5.48f), FVector::ZeroVector, 0.f, nullptr);
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

bool ABREntity::BuildHoundModel()
{
	// v4.5 : Hound fourni d'un seul tenant (SK_Hound : geometrie, pelage et poids d'origine), memes pattes que ci-dessous.
	// v4.6 : hors profil Cinematique, derive allege SK_HoundLite (meme corps, meme squelette ; 45 % des meches de
	// cheveux, deux fois plus larges : 88 % des triangles de l'original etaient des cheveux ; 89 000 sommets au lieu de
	// 175 000) ; repli sur l'original s'il manque
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		USkeletalMesh* SkinMesh = FBRSettings::Get().GraphicsProfile != 2 ? A->SkeletalMesh(TEXT("SK_HoundLite")) : nullptr;
		if (!SkinMesh)
		{
			SkinMesh = A->SkeletalMesh(TEXT("SK_Hound"));
		}
		if (SkinMesh)
		{
			FBRSkinDriver Driver;
			Driver.Skin = BRRig::AddSkin(this, Visual, SkinMesh, nullptr, PartComponents, true);
			HeadPivot = Driver.AddPivot(this, Visual, TEXT("Head"), PartComponents);
			struct FSkinLeg
			{
				const TCHAR* Upper;
				const TCHAR* Lower;
				bool bFront;
				float Phase;
			};
			const FSkinLeg Legs[4] = { { TEXT("FrontUpperL"), TEXT("FrontLowerL"), true, PI }, { TEXT("FrontUpperR"), TEXT("FrontLowerR"), true, 0.f },
				{ TEXT("BackUpperL"), TEXT("BackLowerL"), false, 0.f }, { TEXT("BackUpperR"), TEXT("BackLowerR"), false, PI } };
			int32 Found = 0;
			for (const FSkinLeg& Leg : Legs)
			{
				USceneComponent* Up = Driver.AddPivot(this, Visual, Leg.Upper, PartComponents);
				USceneComponent* Low = Up ? Driver.AddPivot(this, Up, Leg.Lower, PartComponents) : nullptr;
				AddLimb(Up, ELimb::HoundUpper, Leg.Phase, 24.f, Leg.bFront ? -1.f : 1.f, FRotator::ZeroRotator);
				AddLimb(Low, ELimb::HoundLower, Leg.Phase, 22.f, Leg.bFront ? -1.f : 1.f, FRotator::ZeroRotator);
				Found += (Up && Low) ? 1 : 0;
			}
			if (Found == 4 && HeadPivot.IsValid())
			{
				SkinDrivers.Add(Driver);
				LegLength = 67.f; // hanche avant -> sol (foulee liee a la distance parcourue)
				return true;
			}
			UE_LOG(LogTemp, Warning, TEXT("Backrooms : SK_Hound n'a pas les os attendus (Head, FrontUpperL...) : pieces rigides utilisees"));
			if (Driver.Skin.IsValid())
			{
				Driver.Skin->DestroyComponent();
			}
			Limbs.Reset();
			HeadPivot = nullptr;
		}
	}
	// Hound fourni (Tools/Blender/import_user_models.py) : corps, tete et huit segments de pattes, deja en pose
	if (!BRRig::HasMesh(this, TEXT("SM_HoundET_Body")))
	{
		return false;
	}
	LegLength = 67.f;
	AddPart(TEXT("SM_HoundET_Body"), Visual, FVector(42.26f, 0.17f, 71.49f), FVector::ZeroVector, 0.f, nullptr);
	HeadPivot = AddPart(TEXT("SM_HoundET_Head"), Visual, FVector(45.1f, 0.17f, 78.14f), FVector::ZeroVector, 0.f, nullptr);
	struct FLeg
	{
		const TCHAR* Upper;
		const TCHAR* Lower;
		FVector Hip;
		FVector Knee;
		bool bFront;
		float Phase;
	};
	// Articulations : RawAssets/Meshes/user_models.json ("Hound")
	const FLeg Legs[4] = {
		{ TEXT("SM_HoundET_FrontUpperL"), TEXT("SM_HoundET_FrontLowerL"), FVector(36.72f, -17.87f, 67.15f), FVector(28.29f, -20.81f, 43.08f), true, PI },
		{ TEXT("SM_HoundET_FrontUpperR"), TEXT("SM_HoundET_FrontLowerR"), FVector(36.72f, 18.75f, 67.15f), FVector(28.29f, 21.69f, 43.08f), true, 0.f },
		{ TEXT("SM_HoundET_BackUpperL"), TEXT("SM_HoundET_BackLowerL"), FVector(-36.62f, -10.8f, 58.53f), FVector(-35.41f, -12.25f, 39.f), false, 0.f },
		{ TEXT("SM_HoundET_BackUpperR"), TEXT("SM_HoundET_BackLowerR"), FVector(-36.83f, 9.92f, 59.26f), FVector(-35.34f, 12.66f, 39.f), false, PI },
	};
	for (const FLeg& Leg : Legs)
	{
		USceneComponent* Up = AddPart(Leg.Upper, Visual, Leg.Hip, FVector::ZeroVector, 0.f, nullptr);
		USceneComponent* Low = AddPart(Leg.Lower, Up, Leg.Knee - Leg.Hip, FVector::ZeroVector, 0.f, nullptr);
		AddLimb(Up, ELimb::HoundUpper, Leg.Phase, 24.f, Leg.bFront ? -1.f : 1.f, FRotator::ZeroRotator);
		AddLimb(Low, ELimb::HoundLower, Leg.Phase, 22.f, Leg.bFront ? -1.f : 1.f, FRotator::ZeroRotator);
	}
	return true;
}

bool ABREntity::BuildClumpSkin()
{
	// v4.5 : Clump reconstruit (Tools/Blender/build_creatures.py) : 14 bras complets fondus dans la masse, chacun avec
	// ses os (bras, avant-bras, main) ; les mains d'appui sont posees au sol par IK, les autres bras se tordent
	UBRAssets* A = UBRAssets::Get(this);
	USkeletalMesh* ClumpMesh = A ? A->SkeletalMesh(TEXT("SK_Clump")) : nullptr;
	if (!ClumpMesh)
	{
		return false;
	}
	FBRSkinDriver Driver;
	Driver.Skin = BRRig::AddSkin(this, Visual, ClumpMesh, nullptr, PartComponents, true);
	USceneComponent* Core = Driver.AddPivot(this, Visual, TEXT("Core"), PartComponents);
	auto RestOf = [&Driver](FName Bone)
	{
		for (const FBRSkinDriver::FLink& L : Driver.Links)
		{
			if (L.Bone == Bone)
			{
				return L.RestPos;
			}
		}
		return FVector::ZeroVector;
	};
	for (int32 k = 0; Core && k < 32; ++k)
	{
		const FName U(*FString::Printf(TEXT("Arm%d_Upper"), k));
		USceneComponent* Up = Driver.AddPivot(this, Core, U, PartComponents);
		if (!Up)
		{
			break;
		}
		FClumpArm Arm;
		Arm.Upper = Up;
		Arm.Fore = Driver.AddPivot(this, Up, *FString::Printf(TEXT("Arm%d_Fore"), k), PartComponents);
		Arm.Hand = Arm.Fore.IsValid() ? Driver.AddPivot(this, Arm.Fore.Get(), *FString::Printf(TEXT("Arm%d_Hand"), k), PartComponents) : nullptr;
		Arm.Shoulder = RestOf(U);
		Arm.Elbow = RestOf(*FString::Printf(TEXT("Arm%d_Fore"), k));
		Arm.Wrist = RestOf(*FString::Printf(TEXT("Arm%d_Hand"), k));
		Arm.bGround = Arm.Wrist.Z < 12.f;
		Arm.Phase = k * 1.7f;
		if (Arm.Fore.IsValid() && Arm.Hand.IsValid())
		{
			ClumpArms.Add(Arm);
		}
	}
	if (!Core || ClumpArms.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Backrooms : SK_Clump n'a pas les os attendus (Core, Arm0_Upper...) : pieces rigides utilisees"));
		if (Driver.Skin.IsValid())
		{
			Driver.Skin->DestroyComponent();
		}
		ClumpArms.Reset();
		return false;
	}
	HeadPivot = nullptr; // la masse ne tourne pas sur elle-meme : les bras libres se tendent vers la proie
	SkinDrivers.Add(Driver);
	return true;
}

void ABREntity::AnimateClumpSkin(float Dt)
{
	UPoseableMeshComponent* Skin = SkinDrivers.Num() > 0 ? SkinDrivers[0].Skin.Get() : nullptr;
	if (!Skin || !Visual)
	{
		return;
	}
	const FVector Vel = GetVelocity();
	const float Speed = static_cast<float>(Vel.Size2D());
	const float Move = FMath::Clamp(Speed / 250.f, 0.f, 1.f);
	const float Strike = StrikeCurve();
	// La masse : affaissee par son poids, elle tangue et avance par a-coups a chaque main qui se pose
	Lurch = FMath::Max(0.f, Lurch - Dt * 3.5f);
	const float Breath = FMath::Sin(Life * 2.1f);
	Visual->SetRelativeLocation(VisualBase + FVector(Lurch * 7.f + Strike * 28.f - WindupAnim * 8.f, 0.f,
		-Lurch * 5.f - WindupAnim * 9.f + Breath * 1.5f - Move * 4.f));
	Visual->SetRelativeRotation(FRotator(-Lurch * 4.f - Strike * 10.f + WindupAnim * 6.f, 0.f, FMath::Sin(Life * 1.3f) * 3.f * (1.f + Move)));

	const FTransform ST = Skin->GetComponentTransform();
	const FQuat SQ = ST.GetRotation();
	const float Scale = static_cast<float>(ST.GetScale3D().X);
	const FVector Lead = FVector(Vel.X, Vel.Y, 0.f) * 0.28f;
	int32 Swinging = 0;
	for (const FClumpArm& Arm : ClumpArms)
	{
		Swinging += (Arm.bGround && Arm.Swing >= 0.f) ? 1 : 0;
	}
	// Jumpscare : le modele est tenu devant la camera ; tous les bras se tendent vers le joueur (pas d'appui au sol)
	const APawn* Prey = bScareOverride ? UGameplayStatics::GetPlayerPawn(this, 0) : static_cast<const APawn*>(Target.Get());
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ClumpHand), false, this);

	for (FClumpArm& Arm : ClumpArms)
	{
		USceneComponent* Up = Arm.Upper.Get();
		USceneComponent* Fo = Arm.Fore.Get();
		USceneComponent* Ha = Arm.Hand.Get();
		if (!Up || !Fo || !Ha)
		{
			continue;
		}
		const FVector RestD1 = (Arm.Elbow - Arm.Shoulder).GetSafeNormal();
		const FVector RestD2 = (Arm.Wrist - Arm.Elbow).GetSafeNormal();
		const float L1 = static_cast<float>((Arm.Elbow - Arm.Shoulder).Size()) * Scale;
		const float L2 = static_cast<float>((Arm.Wrist - Arm.Elbow).Size()) * Scale;
		const FVector S = Up->GetComponentLocation();

		if (bScareOverride)
		{
			Arm.bPlanted = false; // elles se reposeront au sol apres le jumpscare
			Arm.Swing = -1.f;
		}
		if (Arm.bGround && !bScareOverride)
		{
			// Main d'appui : plantee au sol (IK a deux segments), elle ne glisse pas ; quand le corps l'a depassee,
			// elle se leve et se repose plus loin (deux mains au plus en l'air)
			FVector Home = ST.TransformPosition(Arm.Wrist) + Lead;
			auto Ground = [this, &Q](FVector P)
			{
				FHitResult Hit;
				if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0.f, 0.f, 60.f), P - FVector(0.f, 0.f, 90.f), ECC_Visibility, Q))
				{
					P.Z = Hit.ImpactPoint.Z + 4.f;
				}
				else
				{
					// v4.6 : rien sous la main (bord d'une fosse) : elle reste en l'air, au niveau du corps, sans s'y poser
					P.Z = GetActorLocation().Z - MyInfo().HalfHeight + 30.f;
				}
				return P;
			};
			if (!Arm.bPlanted)
			{
				Arm.Planted = Ground(Home);
				Arm.bPlanted = true;
			}
			const float Step = (Speed > 150.f ? 34.f : 24.f) * Scale;
			if (Arm.Swing < 0.f && FVector::Dist2D(Arm.Planted, Home) > Step && Swinging < 2)
			{
				Arm.SwingFrom = Arm.Planted;
				Arm.Swing = 0.f;
				++Swinging;
			}
			FVector Hand = Arm.Planted;
			if (Arm.Swing >= 0.f)
			{
				Arm.Swing += Dt / (Speed > 150.f ? 0.17f : 0.26f);
				const FVector To = Ground(Home);
				const float T = FMath::Clamp(Arm.Swing, 0.f, 1.f);
				Hand = FMath::Lerp(Arm.SwingFrom, To, T * T * (3.f - 2.f * T)) + FVector(0.f, 0.f, FMath::Sin(T * PI) * 12.f * Scale);
				if (Arm.Swing >= 1.f)
				{
					Arm.Planted = To;
					Arm.Swing = -1.f;
					Lurch = FMath::Min(1.f, Lurch + 0.45f); // le poids retombe sur la main
				}
			}
			// IK : coude dans le plan qui contient l'epaule, la main et la direction "vers l'exterieur et le haut"
			FVector ToHand = Hand - S;
			float D = static_cast<float>(ToHand.Size());
			D = FMath::Clamp(D, FMath::Abs(L1 - L2) + 1.f, (L1 + L2) * 0.999f);
			const FVector Dir = ToHand.GetSafeNormal();
			const FVector Out = (S - Visual->GetComponentLocation()).GetSafeNormal2D();
			FVector Pole = (Out + FVector(0.f, 0.f, 1.2f)).GetSafeNormal();
			Pole = (Pole - Dir * FVector::DotProduct(Pole, Dir)).GetSafeNormal();
			const float CosA = FMath::Clamp((L1 * L1 + D * D - L2 * L2) / (2.f * L1 * D), -1.f, 1.f);
			const FVector Elbow = S + (Dir * CosA + Pole * FMath::Sqrt(FMath::Max(0.f, 1.f - CosA * CosA))) * L1;
			const FVector EndPos = S + Dir * D;
			const FQuat Q1 = FQuat::FindBetweenNormals(RestD1, SQ.UnrotateVector((Elbow - S).GetSafeNormal()));
			const FQuat Q2 = FQuat::FindBetweenNormals(RestD2, SQ.UnrotateVector((EndPos - Elbow).GetSafeNormal()));
			Up->SetWorldRotation(SQ * Q1);
			Fo->SetWorldRotation(SQ * Q2);
			// main a plat (doigts qui se crispent pendant le pas)
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
			Ha->SetWorldRotation(FQuat(Side, Arm.Swing >= 0.f ? -0.5f * FMath::Sin(Arm.Swing * PI) : 0.f) * SQ);
		}
		else
		{
			// Bras libre : il se tord lentement ; en poursuite, il se tend vers la proie ; il frappe avec la masse
			const float T = Life * (0.9f + 0.15f * FMath::Fmod(Arm.Phase, 3.f)) + Arm.Phase;
			const FVector Ax1 = FVector::CrossProduct(RestD1, FVector::UpVector).GetSafeNormal();
			const FVector Ax2 = FVector::CrossProduct(RestD1, Ax1).GetSafeNormal();
			const float Agit = 1.f + Move + LeanAnim * 0.8f;
			FQuat Q1 = FQuat(Ax1, FMath::Sin(T) * 0.35f * Agit) * FQuat(Ax2, FMath::Cos(T * 0.7f + 1.1f) * 0.3f * Agit);
			if (Prey && (State == EState::Chase || Strike > 0.f || bScareOverride))
			{
				const FVector ToPrey = SQ.UnrotateVector((Prey->GetActorLocation() - S).GetSafeNormal());
				const FQuat Aim = FQuat::FindBetweenNormals(RestD1, ToPrey);
				Q1 = FQuat::Slerp(Q1, Aim * FQuat(Ax1, FMath::Sin(T * 2.f) * 0.15f), FMath::Clamp(0.35f * LeanAnim + 0.6f * WindupAnim + Strike, 0.f, 0.9f));
			}
			const FQuat Q2 = FQuat(Ax1, 0.5f + 0.35f * FMath::Sin(T * 1.3f + 0.7f) - Strike * 0.45f + WindupAnim * 0.3f);
			Up->SetRelativeRotation(Q1);
			Fo->SetRelativeRotation(Q2);
			Ha->SetRelativeRotation(FQuat(Ax1, 0.4f * FMath::Sin(T * 2.3f) + WindupAnim * 0.6f));
		}
	}
}

bool ABREntity::BuildClumpModel()
{
	// Clump (d'apres l'image fournie) : une masse de chair a bouche de lamproie et huit faisceaux de bras qui se tordent
	if (!BRRig::HasMesh(this, TEXT("SM_ClumpET_Core")))
	{
		return false;
	}
	USceneComponent* Core = AddPart(TEXT("SM_ClumpET_Core"), Visual, FVector(0.f, 0.f, MyInfo().HalfHeight), FVector::ZeroVector, 0.f, nullptr);
	HeadPivot = Core;
	// Racines des faisceaux par rapport au centre de la masse : RawAssets/Meshes/user_models.json ("Clump")
	const FVector Roots[8] = { FVector(13.23f, -20.71f, 22.31f), FVector(11.09f, 24.04f, 20.45f), FVector(-17.33f, -15.f, 20.84f),
		FVector(-20.51f, 19.24f, 13.85f), FVector(15.86f, -30.78f, -9.84f), FVector(10.78f, 16.3f, -27.09f), FVector(-12.12f, -18.48f, -23.14f),
		FVector(-28.11f, 12.37f, -13.75f) };
	for (int32 k = 0; k < 8; ++k)
	{
		USceneComponent* Arm = AddPart(*FString::Printf(TEXT("SM_ClumpET_Arm%d"), k), Core, Roots[k], FVector::ZeroVector, 0.f, nullptr);
		AddLimb(Arm, ELimb::Tendril, k * 0.9f, 10.f + static_cast<float>(k % 3) * 4.f, (k % 2) ? 1.f : -1.f, FRotator::ZeroRotator);
	}
	return true;
}

void ABREntity::BuildVisual()
{
	TMap<FString, FLinearColor> Tints;
	switch (Kind)
	{
	case EBREntityKind::Smiler:
	{
		// Le Smiler de l'image fournie (SM_SmilerET), sinon l'ancien modele procedural
		const TCHAR* SmilerMesh = BRRig::HasMesh(this, TEXT("SM_SmilerET")) ? TEXT("SM_SmilerET") : TEXT("SM_Smiler");
		AddPart(SmilerMesh, Visual, FVector(0.f, 0.f, MyInfo().HalfHeight), FVector(70.f, 70.f, 90.f), 0.f, nullptr, true, 0.5f);
		GlowLight = NewObject<UPointLightComponent>(this);
		GlowLight->SetupAttachment(Visual);
		// Faible lueur des dents et des yeux sur ce qui l'entoure (la masse d'ombre, elle, ne renvoie presque rien)
		GlowLight->SetRelativeLocation(FVector(55.f, 0.f, MyInfo().HalfHeight - 5.f));
		GlowLight->SetIntensityUnits(ELightUnits::Lumens);
		GlowLight->SetIntensity(35.f);
		GlowLight->SetAttenuationRadius(260.f);
		GlowLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
		GlowLight->SetCastShadows(false);
		GlowLight->RegisterComponent();
		break;
	}
	case EBREntityKind::Hound:
		if (!BuildHoundModel())
		{
			UBRAssets::ReportFallback(TEXT("Hound (SK_Hound, SM_HoundET_*)"));
			Tints.Add(TEXT("Skin"), FLinearColor(0.55f, 0.53f, 0.5f));
			BuildHound(&Tints);
		}
		break;
	case EBREntityKind::Faceling:
	{
		const FLinearColor Shirts[5] = { FLinearColor(0.45f, 0.47f, 0.5f), FLinearColor(0.35f, 0.25f, 0.2f), FLinearColor(0.2f, 0.3f, 0.45f),
			FLinearColor(0.5f, 0.45f, 0.35f), FLinearColor(0.3f, 0.35f, 0.3f) };
		if (BRRig::HasMesh(this, TEXT("SM_FacelingET_Torso")) || BRRig::HasSkeletalMesh(this, TEXT("SK_Faceling")))
		{
			// Faceling fourni (style PS1) : texture d'origine, legerement assombrie au hasard pour varier les silhouettes
			const float Shade = FMath::FRandRange(0.75f, 1.f);
			Tints.Add(TEXT("Faceling"), FLinearColor(Shade, Shade, Shade));
			BuildHumanoid(TEXT("SM_FacelingET"), FBRHumanoidSpec::FacelingET(), &Tints, Visual, TEXT("SK_Faceling"));
			break;
		}
		UBRAssets::ReportFallback(TEXT("Faceling (SK_Faceling, SM_FacelingET_*)"));
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
		if (BRRig::HasMesh(this, TEXT("SM_SkinStealerET_Torso")) || BRRig::HasSkeletalMesh(this, TEXT("SK_SkinStealer")))
		{
			// Skin-Stealer fourni : chair a vif, griffes demesurees ; sa masse au repos prend la meme teinte rouge
			BuildHumanoid(TEXT("SM_SkinStealerET"), FBRHumanoidSpec::SkinStealerET(), nullptr, BodyForm, TEXT("SK_SkinStealer"));
			Tints.Add(TEXT("Flesh"), FLinearColor(0.5f, 0.14f, 0.12f));
		}
		else
		{
			UBRAssets::ReportFallback(TEXT("Skin-Stealer (SK_SkinStealer, SM_SkinStealerET_*)"));
			BuildHumanoid(TEXT("SM_SkinStealer"), SpecFor(Kind), &Tints, BodyForm);
		}
		TrueHead = HeadPivot;
		MassForm = AddPart(TEXT("SM_SkinStealer_Mass"), Visual, FVector::ZeroVector, FVector(110.f, 100.f, 90.f), 45.f, &Tints);
		// Deguisement : la vraie combinaison hazmat (modele fourni), sinon la combinaison procedurale
		DisguiseForm = NewObject<USceneComponent>(this);
		DisguiseForm->SetupAttachment(Visual);
		DisguiseForm->RegisterComponent();
		if (BRRig::HasHazmat(this) || BRRig::HasSkeletalMesh(this, TEXT("SK_Hazmat")))
		{
			BuildHumanoid(TEXT("SM_Hazmat"), FBRHumanoidSpec::Hazmat(), nullptr, DisguiseForm, TEXT("SK_Hazmat"));
			DisguiseHead = HeadPivot;
		}
		else
		{
			UBRAssets::ReportFallback(TEXT("Combinaison hazmat (SK_Hazmat, SM_Hazmat_*)"));
			AddPart(TEXT("SM_Hazmat"), DisguiseForm, FVector::ZeroVector, FVector(40.f, 50.f, 180.f), 90.f, nullptr);
		}
		HeadPivot = TrueHead;
		break;
	}
	case EBREntityKind::Wretch:
		if (BRRig::HasSkeletalMesh(this, TEXT("SK_Wretch")))
		{
			// v4.5 : Wretch reconstruit d'un seul tenant (peau cuite, cotes et vertebres sous la peau, machoire pendante)
			BuildHumanoid(TEXT("SM_Wretch"), FBRHumanoidSpec::WretchSK(), nullptr, Visual, TEXT("SK_Wretch"));
			break;
		}
		Tints.Add(TEXT("Skin"), FLinearColor(0.42f, 0.45f, 0.36f));
		BuildHumanoid(TEXT("SM_Wretch"), SpecFor(Kind), &Tints, Visual);
		break;
	case EBREntityKind::Partygoer:
		if (BRRig::HasMesh(this, TEXT("SM_PartygoerET_Torso")) || BRRig::HasSkeletalMesh(this, TEXT("SK_Partygoer")))
		{
			BuildHumanoid(TEXT("SM_PartygoerET"), FBRHumanoidSpec::PartygoerET(), nullptr, Visual, TEXT("SK_Partygoer"));
		}
		else
		{
			UBRAssets::ReportFallback(TEXT("Partygoer (SK_Partygoer, SM_PartygoerET_*)"));
			BuildHumanoid(TEXT("SM_Partygoer"), SpecFor(Kind), nullptr, Visual);
		}
		break;
	case EBREntityKind::Bacteria:
		if (!BuildBacteriaModel())
		{
			UBRAssets::ReportFallback(TEXT("Bacteria (SM_BacteriaET_*)"));
			BuildHumanoid(TEXT("SM_Bacteria"), SpecFor(Kind), nullptr, Visual);
		}
		break;
	case EBREntityKind::Deathmoth:
	{
		if (BuildMothModel())
		{
			break;
		}
		UBRAssets::ReportFallback(TEXT("Deathmoth (SM_DeathmothET_*)"));
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
		if (BuildClumpSkin())
		{
			break;
		}
		if (!BuildClumpModel())
		{
			AddPart(TEXT("SM_Clump"), Visual, FVector(0.f, 0.f, MyInfo().HalfHeight), FVector(110.f, 110.f, 100.f), 0.f, nullptr);
		}
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

void ABREntity::SetScareTransform(const FTransform* WorldTM)
{
	if (!Visual)
	{
		return;
	}
	if (WorldTM)
	{
		if (!bScareOverride)
		{
			ScareSavedRel = Visual->GetRelativeTransform();
			bScareOverride = true;
		}
		ScareTM = *WorldTM;
		Visual->SetWorldTransform(ScareTM);
	}
	else if (bScareOverride)
	{
		bScareOverride = false;
		Visual->SetRelativeTransform(ScareSavedRel);
	}
}

void ABREntity::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	Life += Dt;
	StateTime += Dt;
	AttackTimer -= Dt;

	if (bScareOverride)
	{
		// Jumpscare : l'entite se fige, son modele est tenu devant la camera du joueur, bras lances vers elle
		UpdateStatePose(Dt);
		Animate(Dt);
		UpdateHead(Dt);
		Visual->SetWorldTransform(ScareTM);
		SkinAccum = 1.f;
		ApplySkins(Dt);
		return;
	}
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
		// v4.6 : garde-fou : une entite tombee dans une fosse (ou sous le monde) disparait
		if (const ABRWorld* PW = World.Get())
		{
			if (PW->HasPits() && GetActorLocation().Z < -PW->Def().PitKillDepth)
			{
				UE_LOG(LogBackrooms, Warning, TEXT("%s est tombee dans une fosse (%s) : retiree"), *MyInfo().Name, *GetActorLocation().ToString());
				Destroy();
				return;
			}
		}
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
	UpdateStatePose(Dt);
	Animate(Dt);
	UpdateHead(Dt);
	UpdateMorph(Dt);
	ApplySkins(Dt);
	UpdateDistanceLOD(Dt);
	UpdateFootsteps();

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

void ABREntity::UpdateFootsteps()
{
	const FBREntityInfo& I = MyInfo();
	if (I.bFlying || Kind == EBREntityKind::Smiler || Vanish >= 0.f || bScareOverride)
	{
		return;
	}
	// Une foulee = un demi-cycle d'AnimTime (le pied d'appui change)
	const int32 Index = FMath::FloorToInt(AnimTime / PI);
	if (Index == LastStepIndex)
	{
		return;
	}
	LastStepIndex = Index;
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const APawn* Listener = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Speed < 40.f || !Listener || FVector::DistSquared(Listener->GetActorLocation(), GetActorLocation()) > FMath::Square(2600.f))
	{
		return;
	}
	float Pitch = 1.f;
	float Volume = 0.45f;
	switch (Kind)
	{
	case EBREntityKind::Hound:
		Pitch = 1.3f; Volume = 0.3f; // pattes legeres et rapides
		break;
	case EBREntityKind::Clump:
		Pitch = 0.55f; Volume = 0.9f; // masse qui retombe sur ses mains
		break;
	case EBREntityKind::Wretch:
		Pitch = 0.82f; Volume = 0.38f; // pieds traines
		break;
	case EBREntityKind::SkinStealer:
		Pitch = 1.f; Volume = 0.55f; // des pas d'explorateur : trompeur
		break;
	case EBREntityKind::Bacteria:
		Pitch = FMath::FRandRange(0.65f, 0.85f); Volume = 0.5f; // a-coups irreguliers
		break;
	default:
		break;
	}
	const ABRWorld* W = World.Get();
	const EBRStep Surface = W ? W->Def().Step : EBRStep::Hard;
	const TCHAR* Prefix = Surface == EBRStep::Carpet ? TEXT("S_Step_Carpet_") : (Surface == EBRStep::Water ? TEXT("S_Step_Water_")
		: (Surface == EBRStep::Grass ? TEXT("S_Step_Grass_") : TEXT("S_Step_Hard_")));
	UBRAssets* A = UBRAssets::Get(this);
	USoundBase* S = A ? A->Sound(FName(*FString::Printf(TEXT("%s%d"), Prefix, 1 + (Index & 3)))) : nullptr;
	if (S)
	{
		const float Run = FMath::Clamp(Speed / FMath::Max(I.ChaseSpeed, 1.f), 0.3f, 1.2f);
		UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation() - FVector(0.f, 0.f, I.HalfHeight), Volume * (0.6f + 0.5f * Run),
			Pitch * FMath::FRandRange(0.94f, 1.06f), 0.f, A->Attenuation(1800.f));
	}
}

void ABREntity::UpdateDistanceLOD(float Dt)
{
	LodTimer -= Dt;
	if (LodTimer > 0.f)
	{
		return;
	}
	LodTimer = 0.5f;
	if (ShadowCasters.Num() == 0)
	{
		TArray<UPrimitiveComponent*> Prims;
		GetComponents<UPrimitiveComponent>(Prims);
		for (UPrimitiveComponent* P : Prims)
		{
			if (P && P != GetCapsuleComponent())
			{
				ShadowCasters.Add(TPair<TWeakObjectPtr<UPrimitiveComponent>, bool>(P, P->CastShadow != 0));
			}
		}
	}
	const APawn* Local = UGameplayStatics::GetPlayerPawn(this, 0);
	const float Dist = Local ? static_cast<float>(FVector::Dist(Local->GetActorLocation(), GetActorLocation())) : 0.f;
	// Hysteresis : coupe au-dela de 30 m, retablit en deca de 26 m (pas de clignotement a la limite) ; jamais pendant
	// un jumpscare
	const bool bWantOff = !bScareOverride && (bFarShadowsOff ? Dist > 2600.f : Dist > 3000.f);
	if (bWantOff == bFarShadowsOff)
	{
		return;
	}
	bFarShadowsOff = bWantOff;
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, bool>& Pair : ShadowCasters)
	{
		if (UPrimitiveComponent* P = Pair.Key.Get())
		{
			P->SetCastShadow(Pair.Value && !bFarShadowsOff);
		}
	}
}

void ABREntity::ApplySkins(float Dt)
{
	if (SkinDrivers.Num() == 0)
	{
		return;
	}
	// LOD d'animation : chaque image si l'entite est proche et a l'ecran, 20 fois par seconde au-dela de 25 m,
	// 5 fois par seconde hors de vue (ses os restent a jour pour l'ombre et les reflets ray traces)
	const APawn* Local = UGameplayStatics::GetPlayerPawn(this, 0);
	const float Dist = Local ? static_cast<float>(FVector::Dist(Local->GetActorLocation(), GetActorLocation())) : 0.f;
	const UPrimitiveComponent* First = SkinDrivers[0].Skin.Get();
	const bool bSeen = !First || First->WasRecentlyRendered(0.25f);
	const float Period = !bSeen ? 0.2f : (Dist > 2500.f ? 0.05f : 0.f);
	SkinAccum += Dt;
	if (SkinAccum < Period)
	{
		return;
	}
	SkinAccum = 0.f;
	for (const FBRSkinDriver& D : SkinDrivers)
	{
		D.Apply();
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

void ABREntity::MulticastStrike_Implementation()
{
	StrikeTime = 0.f;
	WindupClock = -1.f;
	// (la pose armee se relache pendant la montee de la frappe : WindupAnim * (1 - Strike) dans AnimateLimbs)
}

void ABREntity::MulticastVoiceCue_Implementation(float Volume)
{
	PlayVoice(Volume);
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
	// v4.6 : le vide d'une fosse n'arrete pas un balayage : une ligne droite au-dessus d'une fosse n'est pas un chemin
	const ABRWorld* W = World.Get();
	if (W && !MyInfo().bFlying && W->SegmentCrossesPit(Start, End, MyInfo().Radius + 10.f))
	{
		return false;
	}
	return !GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(R), Q);
}

void ABREntity::MoveTowards(const FVector& Dest, float Speed)
{
	UCharacterMovementComponent* M = GetCharacterMovement();
	// v4.7 : demarche de l'espece et phase d'attaque (preparation lente, fente, recuperation)
	Speed *= GaitScale * AttackMoveScale();
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
	Dir = Dir.GetSafeNormal();
	const ABRWorld* W = World.Get();
	if (!I.bFlying && W && W->HasPits())
	{
		// v4.6 : navigation locale au bord des fosses : si le pas suivant mene au-dessus du vide, on glisse le long du bord
		// (composante X ou Y qui reste sur le passage), sinon on s'arrete en attendant le prochain chemin
		const FVector Loc = GetActorLocation();
		const float Look = I.Radius + 35.f;
		const float Keep = I.Radius * 0.6f;
		if (W->IsOverPit(Loc + Dir * Look, Keep))
		{
			FVector Best = FVector::ZeroVector;
			float BestDot = 0.f;
			const FVector Alts[2] = { FVector(Dir.X, 0.f, 0.f), FVector(0.f, Dir.Y, 0.f) };
			for (const FVector& Alt : Alts)
			{
				const FVector A = Alt.GetSafeNormal();
				const float Dot = static_cast<float>(FVector::DotProduct(A, Dir));
				if (!A.IsNearlyZero() && Dot > BestDot && !W->IsOverPit(Loc + A * Look, Keep))
				{
					Best = A;
					BestDot = Dot;
				}
			}
			if (Best.IsNearlyZero())
			{
				return;
			}
			Dir = Best;
		}
	}
	bWantsMove = true;
	AddMovementInput(Dir, 1.f);
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
	// v4.6 : dans une salle de fosses, on suit les passages (centres des cellules) et on ne coupe jamais au-dessus du vide
	const bool bPitZone = !MyInfo().bFlying && (W->IsPitRoomCell(MyCell.X, MyCell.Y) || W->IsPitRoomCell(GoalCell.X, GoalCell.Y));
	const float Accept = bPitZone ? 28.f : 70.f;
	if (MyCell == GoalCell || (Dist < W->CellSize() * 2.5f && IsDirectPathClear(Goal)))
	{
		Path.Reset();
		if (bPitZone && W->SegmentCrossesPit(GetActorLocation(), Goal, MyInfo().Radius + 10.f))
		{
			// Meme cellule mais d'un bras du passage a l'autre : par le croisement (centre de la cellule)
			MoveTowards(W->CellCenter(MyCell, GetActorLocation().Z), Speed);
			return;
		}
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
		if (FVector::Dist2D(Wp, GetActorLocation()) < Accept)
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
	const FIntPoint Here = W->WorldToCell(GetActorLocation());
	const bool bPitZone = !MyInfo().bFlying && W->IsPitRoomCell(Here.X, Here.Y);
	if (FVector::Dist2D(Wp, GetActorLocation()) < (bPitZone ? 28.f : 60.f))
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
	// v4.7 : ne frappe plus dans la meme image : lance la preparation. Le coup n'est porte que pendant la fenetre
	// d'impact, si la proie est encore a portee et visible (UpdateAttack)
	if (!P || P->IsDead() || AttackTimer > 0.f || AttackPhase != EAttackPhase::None)
	{
		return;
	}
	const FBREntityInfo& I = MyInfo();
	if (Dist <= I.AttackRange + 40.f && HasLineOfSight(P))
	{
		AttackPhase = EAttackPhase::Windup;
		AttackPhaseTime = 0.f;
		AttackStartTime = Life;
		bAttackLanded = false;
		AttackVictim = P;
		++StatWindups;
		MulticastAttackWindup(I.WindupTime);
		MulticastVoiceCue(1.f); // le cri annonce le coup, chez tous les joueurs proches
	}
}

void ABREntity::MulticastAttackWindup_Implementation(float Duration)
{
	WindupClock = 0.f;
	WindupDuration = FMath::Max(0.05f, Duration);
}

float ABREntity::AttackMoveScale() const
{
	const FBREntityInfo& I = MyInfo();
	switch (AttackPhase)
	{
	case EAttackPhase::Windup:
		return I.WindupMove;
	case EAttackPhase::Impact:
		// Fente : les especes qui bondissent gagnent de la vitesse, les autres ralentissent en frappant
		return I.LungeReach >= 40.f ? 1.6f : 0.5f;
	case EAttackPhase::Recover:
		return 0.25f;
	default:
		return 1.f;
	}
}

void ABREntity::UpdateAttack(float Dt)
{
	if (AttackPhase == EAttackPhase::None)
	{
		return;
	}
	const FBREntityInfo& I = MyInfo();
	ABRCharacter* V = AttackVictim.Get();
	AttackPhaseTime += Dt;
	// Face a la proie pendant la preparation (lisible), sans pivoter pendant l'impact et la recuperation (poids)
	if (V && AttackPhase == EAttackPhase::Windup)
	{
		FVector To = V->GetActorLocation() - GetActorLocation();
		To.Z = 0.f;
		if (!To.IsNearlyZero())
		{
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), To.Rotation(), Dt, I.TurnRate * 1.5f));
		}
	}
	switch (AttackPhase)
	{
	case EAttackPhase::Windup:
		// Proie hors d'atteinte (elle a fui loin, elle est morte, disparue) : l'entite abandonne son geste
		if (!V || V->IsDead() || FVector::Dist2D(V->GetActorLocation(), GetActorLocation()) > (I.AttackRange + I.LungeReach) * 3.f)
		{
			AttackPhase = EAttackPhase::Recover;
			AttackPhaseTime = 0.f;
			++StatMisses;
			break;
		}
		if (AttackPhaseTime >= I.WindupTime)
		{
			AttackPhase = EAttackPhase::Impact;
			AttackPhaseTime = 0.f;
			MulticastStrike();
		}
		break;
	case EAttackPhase::Impact:
	{
		// Le bras atteint son allonge 0,05 s apres le debut du geste (StrikeCurve) : la fenetre commence la
		if (!bAttackLanded && AttackPhaseTime >= 0.05f && V && !V->IsDead())
		{
			FVector To = V->GetActorLocation() - GetActorLocation();
			const float Dist = static_cast<float>(To.Size2D());
			To.Z = 0.f;
			const float Facing = To.IsNearlyZero() ? 1.f : static_cast<float>(FVector::DotProduct(To.GetSafeNormal(), GetActorForwardVector()));
			const bool bInReach = Dist <= I.AttackRange + 30.f + I.LungeReach;
			const bool bVertical = FMath::Abs(V->GetActorLocation().Z - GetActorLocation().Z) < I.HalfHeight + 120.f;
			if (bInReach && bVertical && Facing > 0.f && HasLineOfSight(V))
			{
				bAttackLanded = true;
				++StatHits;
				StatLastHitDelay = Life - AttackStartTime;
				V->ReceiveAttack(I.Damage, I.SanityDamage, this, I.Name);
			}
		}
		if (AttackPhaseTime >= 0.05f + I.ImpactWindow)
		{
			if (!bAttackLanded)
			{
				++StatMisses; // esquive : hors de portee, derriere un obstacle ou cache
			}
			AttackPhase = EAttackPhase::Recover;
			AttackPhaseTime = 0.f;
		}
		break;
	}
	case EAttackPhase::Recover:
		if (AttackPhaseTime >= I.RecoveryTime)
		{
			AttackPhase = EAttackPhase::None;
			AttackVictim.Reset();
			// Le delai entre deux attaques compte a partir de la fin du geste (au moins 0,2 s)
			AttackTimer = FMath::Max(0.2f, I.AttackCooldown - I.WindupTime - I.ImpactWindow - I.RecoveryTime);
		}
		break;
	default:
		break;
	}
}

void ABREntity::UpdateGait(float Dt, const ABRCharacter* P)
{
	const FBREntityInfo& I = MyInfo();
	const bool bChasing = State == EState::Chase;
	GaitTimer -= Dt;
	GaitCooldown -= Dt;
	UCharacterMovementComponent* M = GetCharacterMovement();
	switch (Kind)
	{
	case EBREntityKind::Bacteria:
		// Saccades : elans courts puis arrets nets (plus frequents en poursuite)
		if (GaitTimer <= 0.f)
		{
			bGaitBurst = !bGaitBurst;
			GaitTimer = bGaitBurst ? FMath::FRandRange(bChasing ? 0.35f : 0.8f, bChasing ? 0.7f : 2.2f)
				: FMath::FRandRange(bChasing ? 0.10f : 0.25f, bChasing ? 0.25f : 0.7f);
		}
		GaitScale = bGaitBurst ? (bChasing ? 1.25f : 1.f) : 0.05f;
		break;
	case EBREntityKind::Hound:
		// Galop : la vitesse pulse au rythme des foulees
		GaitScale = 1.f + (bChasing ? 0.12f : 0.05f) * FMath::Sin(Life * 2.f * PI * (bChasing ? 1.7f : 1.1f));
		break;
	case EBREntityKind::Wretch:
		// Epuise : avance par a-coups, trebuche de temps en temps
		if (GaitTimer <= 0.f)
		{
			bGaitBurst = !bGaitBurst; // vrai : trebuche
			GaitTimer = bGaitBurst ? FMath::FRandRange(0.35f, 0.6f) : FMath::FRandRange(2.5f, 6.f);
			if (bGaitBurst)
			{
				Twitch = FRotator(FMath::FRandRange(8.f, 16.f), FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(-20.f, 20.f));
				TwitchTimer = GaitTimer;
			}
		}
		GaitScale = bGaitBurst ? 0.15f : 0.78f + 0.22f * FMath::Sin(Life * 1.9f);
		break;
	case EBREntityKind::SkinStealer:
	{
		// Trompeur : allure tranquille, puis brusque poussee quand la proie est proche
		const float Dist = P ? static_cast<float>(FVector::Dist2D(P->GetActorLocation(), GetActorLocation())) : 1e9f;
		if (bChasing && !bGaitBurst && GaitCooldown <= 0.f && Dist < 650.f && Dist > 180.f)
		{
			bGaitBurst = true;
			GaitTimer = 1.1f;
			GaitCooldown = 4.5f;
		}
		if (bGaitBurst && GaitTimer <= 0.f)
		{
			bGaitBurst = false;
		}
		GaitScale = bGaitBurst ? 1.4f : (bChasing ? 0.85f : 1.f);
		if (M)
		{
			M->MaxAcceleration = bGaitBurst ? 3200.f : I.Acceleration;
		}
		break;
	}
	case EBREntityKind::Clump:
		// Lourd : avance quand le poids retombe sur une main (Lurch), presque a l'arret entre deux appuis
		GaitScale = ClumpArms.Num() > 0 ? 0.45f + 0.75f * Lurch : 0.6f + 0.4f * FMath::Abs(FMath::Sin(Life * 2.2f));
		break;
	default:
		GaitScale = 1.f;
		break;
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
	// v4.7 : l'attaque en cours se deroule jusqu'au bout (meme si la proie change ou tombe), puis la demarche
	UpdateAttack(Dt);
	UpdateGait(Dt, P);
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
	else if (W->IsBlackout() && !S.bLOS && S.Dist > 500.f)
	{
		// v4.3 : pendant une coupure, ils rodent vers le joueur dans le noir et finissent par surgir a l'angle d'une porte
		FollowPathTo(PL, I.WalkSpeed, Dt);
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
	// s'il court, s'il l'eclaire ; lentement s'il est sur le cote, accroupi ou dans le noir ; a peine s'il est
	// dans son dos). A ~10 m, il faut ~3 s pour qu'elle passe a l'attaque : le temps de la voir et de se cacher.
	float Rate = 0.f;
	if (S.bLOS && S.Dist < Sight)
	{
		const float Near = 1.f - FMath::Clamp(S.Dist / Sight, 0.f, 1.f);
		const FVector ToPlayer = (PL - GetActorLocation()).GetSafeNormal2D();
		const float Facing = static_cast<float>(FVector::DotProduct(GetActorForwardVector(), ToPlayer));
		const float Cone = Facing > 0.5f ? 1.f : (Facing > 0.f ? 0.5f : 0.15f);
		Rate = (0.05f + 0.75f * Near * Near) * Cone;
		if (P->bIsCrouched)
		{
			Rate *= 0.6f;
		}
		if (!P->IsFlashlightOn() && W->LightLevelAt(PL) < 0.15f)
		{
			Rate *= 0.5f; // une silhouette dans le noir se distingue mal
		}
		Rate += (P->IsSprinting() ? 0.6f : 0.f) + ((P->IsFlashlightOn() && S.bLookedAt) ? 0.35f : 0.f);
		if (S.Dist < 300.f)
		{
			Rate = 10.f;
		}
	}
	if (S.bHeard && S.Dist < 1200.f)
	{
		Rate += 0.6f;
	}
	Suspicion = Rate > 0.f ? FMath::Min(1.f, Suspicion + Rate * Dt) : FMath::Max(0.f, Suspicion - 0.15f * Dt);
	// "Elle m'a vu ?" : un raclement de fils avertit le joueur (une fois, jusqu'a ce qu'elle se desinteresse)
	if (Suspicion > 0.45f && !bSuspicionCue && State != EState::Chase)
	{
		bSuspicionCue = true;
		MulticastVoiceCue(0.9f);
	}
	else if (Suspicion < 0.2f)
	{
		bSuspicionCue = false;
	}

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

void ABREntity::UpdateFootPlanting(float Dt)
{
	const FBREntityInfo& I = MyInfo();
	const APawn* Viewer = UGameplayStatics::GetPlayerPawn(this, 0);
	const float ViewDist = Viewer ? static_cast<float>(FVector::Dist(Viewer->GetActorLocation(), GetActorLocation())) : 0.f;
	// Loin, en vol, figee ou sans jambes mesurees : pas de rayons, les reglages reviennent doucement a zero
	const bool bActive = !I.bFlying && bFeetMeasured && FootLift.Num() == Limbs.Num() && ViewDist < 2500.f && !bScareOverride
		&& GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround();
	if (!bActive)
	{
		PelvisDrop = FMath::FInterpTo(PelvisDrop, 0.f, Dt, 6.f);
		for (FLimb& L : Limbs)
		{
			L.PlantPitch = FMath::FInterpTo(L.PlantPitch, 0.f, Dt, 6.f);
		}
		return;
	}
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : I.HalfHeight;
	const float BaseZ = static_cast<float>(GetActorLocation().Z) - Half;
	// Rayons au sol sous chaque pied, 20 fois par seconde (sol statique seulement : ni joueurs ni entites)
	FootTraceTimer -= Dt;
	if (FootTraceTimer <= 0.f && GetWorld())
	{
		FootTraceTimer = 0.05f;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BREntityFeet), false, this);
		const FCollisionObjectQueryParams Obj(ECC_WorldStatic);
		for (int32 i = 0; i < Limbs.Num(); ++i)
		{
			const FLimb& L = Limbs[i];
			const USceneComponent* C = L.Pivot.Get();
			if (!C || (L.Type != ELimb::Shin && L.Type != ELimb::HoundLower))
			{
				continue;
			}
			const FVector Foot = C->GetComponentTransform().TransformPosition(L.FootOffset);
			FHitResult Hit;
			const FVector From(Foot.X, Foot.Y, BaseZ + 45.f);
			const FVector To(Foot.X, Foot.Y, BaseZ - 45.f);
			// Pas de sol trouve (bord d'un bassin, d'une fosse) : on garde le niveau du corps, le pied ne plonge pas
			FootGround[i] = GetWorld()->LineTraceSingleByObjectType(Hit, From, To, Obj, Q) && Hit.ImpactNormal.Z > 0.6f
				? FMath::Clamp(static_cast<float>(Hit.ImpactPoint.Z) - BaseZ, -25.f, 35.f)
				: 0.f;
		}
	}
	// Le bassin descend vers le sol le plus bas sous un pied (marche descendante) ; les autres pieds plient le genou
	float Lowest = 0.f;
	for (int32 i = 0; i < Limbs.Num(); ++i)
	{
		if (Limbs[i].Type == ELimb::Shin || Limbs[i].Type == ELimb::HoundLower)
		{
			Lowest = FMath::Min(Lowest, FootGround[i]);
		}
	}
	PelvisDrop = FMath::FInterpTo(PelvisDrop, Lowest, Dt, 10.f);
	for (int32 i = 0; i < Limbs.Num(); ++i)
	{
		FLimb& L = Limbs[i];
		if (L.Type != ELimb::Shin && L.Type != ELimb::HoundLower)
		{
			continue;
		}
		FootLift[i] = FMath::FInterpTo(FootLift[i], FMath::Clamp(FootGround[i] - PelvisDrop, 0.f, 35.f), Dt, 12.f);
		const float Lift = FootLift[i];
		if (L.Type == ELimb::HoundLower)
		{
			// Patte : le bas de la patte se replie (meme sens que le lever du pas)
			L.PlantPitch = L.Sign * FMath::Clamp(FMath::RadiansToDegrees(Lift / FMath::Max(L.LowerLen, 10.f)) * 1.2f, 0.f, 40.f);
			continue;
		}
		// Jambe : IK a deux segments. Raccourcir la distance hanche -> pied de Lift : la cuisse avance, le genou plie
		const float A = FMath::Max(L.UpperLen, 5.f);
		const float B = FMath::Max(L.LowerLen, 5.f);
		const float D0 = A + B - 2.f;
		const float D1 = FMath::Max(D0 - Lift, FMath::Abs(A - B) + 5.f);
		auto HipAngle = [A, B](float D) { return FMath::Acos(FMath::Clamp((A * A + D * D - B * B) / (2.f * A * D), -1.f, 1.f)); };
		auto KneeBend = [A, B](float D) { return PI - FMath::Acos(FMath::Clamp((A * A + B * B - D * D) / (2.f * A * B), -1.f, 1.f)); };
		const float ThighDeg = FMath::RadiansToDegrees(HipAngle(D1) - HipAngle(D0));
		const float KneeDeg = FMath::RadiansToDegrees(KneeBend(D1) - KneeBend(D0));
		L.PlantPitch = -KneeDeg;
		if (i > 0 && Limbs[i - 1].Type == ELimb::Thigh)
		{
			Limbs[i - 1].PlantPitch = ThighDeg;
		}
	}
}

bool ABREntity::MeasureFeet()
{
	bFeetMeasured = true;
	const FBREntityInfo& I = MyInfo();
	const FTransform AT = GetActorTransform();
	bool bAny = false;
	for (FLimb& L : Limbs)
	{
		USceneComponent* C = L.Pivot.Get();
		if (!C || (L.Type != ELimb::Shin && L.Type != ELimb::HoundLower))
		{
			continue;
		}
		// Au repos, le pied est a l'aplomb du genou, au niveau du sol (bas de la capsule)
		const FVector Knee = AT.InverseTransformPosition(C->GetComponentLocation());
		const FVector FootWorld = AT.TransformPosition(FVector(Knee.X, Knee.Y, -I.HalfHeight));
		L.FootOffset = C->GetComponentTransform().InverseTransformPosition(FootWorld);
		// v4.7 : longueurs des deux segments (le segment du haut precede toujours celui du bas dans Limbs)
		L.LowerLen = static_cast<float>(FVector::Dist(C->GetComponentLocation(), FootWorld));
		const int32 Idx = static_cast<int32>(&L - Limbs.GetData());
		if (Idx > 0 && Limbs[Idx - 1].Pivot.IsValid())
		{
			L.UpperLen = static_cast<float>(FVector::Dist(Limbs[Idx - 1].Pivot->GetComponentLocation(), C->GetComponentLocation()));
		}
		bAny = true;
	}
	FootLift.SetNumZeroed(Limbs.Num());
	FootGround.SetNumZeroed(Limbs.Num());
	return bAny;
}

bool ABREntity::LowestFootHeight(float& OutHeight) const
{
	const FTransform AT = GetActorTransform();
	float MinZ = TNumericLimits<float>::Max();
	for (const FLimb& L : Limbs)
	{
		const USceneComponent* C = L.Pivot.Get();
		if (!C || (L.Type != ELimb::Shin && L.Type != ELimb::HoundLower))
		{
			continue;
		}
		const FVector Foot = AT.InverseTransformPosition(C->GetComponentTransform().TransformPosition(L.FootOffset));
		MinZ = FMath::Min(MinZ, static_cast<float>(Foot.Z));
	}
	if (MinZ == TNumericLimits<float>::Max())
	{
		return false;
	}
	OutHeight = MinZ + MyInfo().HalfHeight - AppliedGroundZ;
	return true;
}

void ABREntity::Animate(float Dt)
{
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const bool bFrozen = State == EState::Frozen;
	if (!bFeetMeasured)
	{
		MeasureFeet(); // avant la premiere pose : les pivots sont encore au repos
	}
	// v4.5 : amplitude du pas selon la vitesse (jusqu'a 1,5 x en poursuite), cadence deduite de la distance parcourue :
	// pendant l'appui, le pied recule exactement de ce dont le corps avance (2 L sin A par demi-cycle) : pas de glissement
	const float Gait = FMath::Clamp(Speed / 220.f, 0.f, 1.5f);
	if (!bFrozen)
	{
		float LegAmpDeg = 26.f;
		for (const FLimb& L : Limbs)
		{
			if (L.Type == ELimb::Thigh || L.Type == ELimb::HoundUpper)
			{
				LegAmpDeg = L.Amp;
				break;
			}
		}
		const float A = FMath::DegreesToRadians(LegAmpDeg * FMath::Max(Gait, 0.2f));
		const float Omega = Speed > 5.f ? (PI * Speed) / (2.f * LegLength * FMath::Max(FMath::Sin(A), 0.05f)) : 0.f;
		AnimTime += Dt * FMath::Clamp(Omega, 0.f, 16.f);
	}

	switch (Kind)
	{
	case EBREntityKind::Smiler:
	{
		Visual->SetRelativeLocation(VisualBase + FVector(0.f, 0.f, FMath::Sin(Life * 1.5f) * 6.f));
		const bool bBlink = FMath::Fmod(Life + 0.37f * static_cast<float>(GetUniqueID() % 7), 5.3f) < 0.12f;
		// Le sourire s'illumine davantage quand il est sur le point de charger
		const float Rage = (State == EState::Chase ? 1.6f : 1.f + FMath::Clamp(BeamTime * 2.f, 0.f, 0.6f)) + (WindupClock >= 0.f ? 0.35f : 0.f);
		// v4.7 : emission tenue : de pres (jumpscare, couloir), le visage ne doit pas devenir une tache blanche qui cache
		// les yeux et les dents ; de loin, il reste deux points et un trait dans le noir
		const APawn* Viewer = UGameplayStatics::GetPlayerPawn(this, 0);
		const float ViewDist = Viewer ? static_cast<float>(FVector::Dist(Viewer->GetActorLocation(), GetActorLocation())) : 1000.f;
		const float Exposure = static_cast<float>(FMath::GetMappedRangeValueClamped(FVector2D(120.0, 650.0), FVector2D(0.5, 1.0), static_cast<double>(ViewDist)));
		const float G = bBlink ? 0.f : FMath::Min((0.85f + 0.15f * FMath::Sin(Life * 13.f)) * Rage, 1.8f) * Exposure;
		// v4.5 : chaque surface garde son intensite (coeur des yeux, dents, bords et racines plus faibles) :
		// le visage a du relief au lieu d'un aplat blanc uniforme
		for (int32 i = 0; i < GlowMIDs.Num(); ++i)
		{
			if (UMaterialInstanceDynamic* M = GlowMIDs[i])
			{
				M->SetVectorParameterValue(TEXT("Emissive"), (GlowBase.IsValidIndex(i) ? GlowBase[i] : FLinearColor(10.f, 9.6f, 8.6f)) * G);
			}
		}
		if (GlowLight)
		{
			GlowLight->SetIntensity(40.f * G);
		}
		return;
	}
	case EBREntityKind::Clump:
	{
		if (ClumpArms.Num() > 0)
		{
			AnimateClumpSkin(Dt);
			return;
		}
		Visual->SetRelativeRotation(FRotator(FMath::Sin(Life * 2.3f) * 6.f, FMath::Sin(Life * 1.1f) * 12.f, FMath::Cos(Life * 2.9f) * 6.f));
		const float Sc = 1.1f * (1.f + 0.05f * FMath::Sin(Life * 5.f));
		Visual->SetRelativeScale3D(FVector(Sc, Sc, 1.21f / Sc));
		AnimateLimbs(Dt, Gait); // les faisceaux de bras se tordent (plus vite en poursuite)
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
	// v4.7 : chaque pied cherche son propre sol (le reglage v4.6 supposait un sol plat sous tout le corps)
	UpdateFootPlanting(Dt);

	// Accroupissement (Faceling), palpitation de la masse (Skin-Stealer)
	// v4.5 : armee (le corps se tasse), frappe (fente vers l'avant), sursaut de la detection
	// v4.6 : appui au sol : le corps descend ou remonte pour que le pied le plus bas (jambe d'appui, genou tendu) touche
	// le sol a chaque instant du pas : plus de pieds qui flottent quand les jambes sont ecartees, ni de pied sous le sol
	// quand les genoux se plient pour l'armee (remplace le rebond et le tassement fixes de la v4.5)
	const float Strike = StrikeCurve();
	float FootH = 0.f;
	float Crouch = WindupAnim * 7.f + AlertAnim * 3.f;
	float Bob = FMath::Abs(FMath::Sin(AnimTime)) * 3.f * FMath::Min(Gait, 1.f);
	if (LowestFootHeight(FootH))
	{
		GroundAdjust = FMath::FInterpTo(GroundAdjust, FMath::Clamp(-FootH, -30.f, 30.f), Dt, 20.f);
		Crouch = AlertAnim * 3.f;
		Bob = GroundAdjust;
	}
	AppliedGroundZ = Bob + PelvisDrop - HideCrouch * 70.f - Crouch;
	Visual->SetRelativeLocation(VisualBase + FVector(Strike * 16.f - WindupAnim * 4.f, 0.f, AppliedGroundZ));
	if (Kind == EBREntityKind::Faceling)
	{
		Visual->SetRelativeRotation(FRotator(-HideCrouch * 25.f, 0.f, 0.f));
	}
	else if (TorsoPivots.Num() == 0 && Kind != EBREntityKind::Deathmoth)
	{
		// pieces rigides et quadrupedes : tout le corps bascule (le buste rigide ne porte pas la tete ni les bras)
		Visual->SetRelativeRotation(FRotator(-LeanAnim * 6.f - WindupAnim * 6.f + Strike * 4.f, 0.f, 0.f));
	}
	for (const TWeakObjectPtr<USceneComponent>& T : TorsoPivots)
	{
		if (USceneComponent* C = T.Get())
		{
			// buste du maillage a squelette : penche en poursuite, ramasse a l'armee, projete a la frappe, redresse au sursaut
			const float Pitch = -LeanAnim * 16.f - WindupAnim * 10.f - Strike * 14.f + AlertAnim * 6.f;
			const float Roll = FMath::Sin(AnimTime) * 3.f * FMath::Min(Gait, 1.f);
			C->SetRelativeRotation(FRotator(Pitch, 0.f, Roll) + Twitch * (Kind == EBREntityKind::Wretch ? 0.25f : 0.f));
		}
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

void ABREntity::UpdateStatePose(float Dt)
{
	// Detection : passage du calme (rondes, attente, cachette) a la traque ou a la poursuite -> sursaut, tete braquee
	if (State != AnimState)
	{
		const bool bWasCalm = AnimState == EState::Idle || AnimState == EState::Wander || AnimState == EState::Hide;
		const bool bNowAlert = State == EState::Chase || State == EState::Stalk || State == EState::Lure;
		if (bWasCalm && bNowAlert)
		{
			AlertAnim = 1.f;
		}
		AnimState = State;
	}
	AlertAnim = FMath::Max(0.f, AlertAnim - Dt / 0.8f);
	LeanAnim = FMath::FInterpTo(LeanAnim, State == EState::Chase ? 1.f : (State == EState::Stalk ? 0.4f : 0.f), Dt, 3.f);
	// Armee : la proie est presque a portee pendant une poursuite (calcule chez chacun : aucun effet sur le jeu)
	const ABRCharacter* P = Target.Get();
	const float Dist = P ? static_cast<float>(FVector::Dist(P->GetActorLocation(), GetActorLocation())) : 1e9f;
	const bool bReady = State == EState::Chase && Dist < MyInfo().AttackRange * 1.9f && (StrikeTime < 0.f || StrikeTime > 0.9f);
	// v4.7 : vraie preparation decidee par le serveur : le geste arme se forme sur toute sa duree, plus marque que
	// la simple garde (bReady) pour que l'on sache qu'un coup arrive et quand
	if (WindupClock >= 0.f)
	{
		WindupClock += Dt;
		if (WindupClock > WindupDuration + 0.6f)
		{
			WindupClock = -1.f; // frappe jamais recue (entite retiree) : on relache
		}
	}
	if (WindupClock >= 0.f && StrikeTime < 0.f)
	{
		const float T = FMath::Clamp(WindupClock / (WindupDuration * 0.85f), 0.f, 1.f);
		WindupAnim = FMath::Max(WindupAnim, 1.25f * T * T * (3.f - 2.f * T));
	}
	else
	{
		WindupAnim = FMath::FInterpTo(WindupAnim, bReady ? 0.6f : 0.f, Dt, bReady ? 6.f : 3.f);
	}
	if (StrikeTime >= 0.f)
	{
		StrikeTime += Dt;
		if (StrikeTime > 1.2f)
		{
			StrikeTime = -1.f;
		}
	}
	// Jumpscare : la frappe est figee au moment ou le modele remplit l'ecran
	if (bScareOverride)
	{
		StrikeTime = FMath::Min(StrikeTime < 0.f ? 0.f : StrikeTime, 0.2f);
	}
}

float ABREntity::StrikeCurve() const
{
	if (StrikeTime < 0.f)
	{
		return 0.f;
	}
	if (StrikeTime < 0.12f)
	{
		const float T = StrikeTime / 0.12f;
		return T * T * (3.f - 2.f * T);
	}
	if (StrikeTime < 0.32f)
	{
		return 1.f;
	}
	const float T = FMath::Clamp((StrikeTime - 0.32f) / 0.58f, 0.f, 1.f);
	return 1.f - T * T * (3.f - 2.f * T);
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
		const float Strike = StrikeCurve();
		const float Ready = FMath::Max(WindupAnim, Strike);
		switch (L.Type)
		{
		case ELimb::UpperArm:
			R.Pitch += L.Amp * Swing * Gait * (1.f - Reach) * (1.f - Ready) + Reach * 75.f * (1.f - Ready);
			R.Roll += Reach * L.Sign * 8.f;
			// v4.5 : armee (bras ramenes en arriere et ecartes), frappe (balayage vers l'avant et le bas), sursaut
			R.Pitch += WindupAnim * (1.f - Strike) * -35.f + Strike * 105.f + AlertAnim * 14.f;
			R.Roll += L.Sign * (WindupAnim * 22.f + AlertAnim * 10.f) * (1.f - Strike);
			if (Kind == EBREntityKind::Bacteria || Kind == EBREntityKind::Wretch)
			{
				R.Pitch += Twitch.Pitch * 0.4f * L.Sign + FMath::Sin(Life * 17.f + L.Phase) * 2.5f;
			}
			break;
		case ELimb::LowerArm:
			R.Pitch += (10.f * FMath::Max(0.f, Swing)) * Gait + Reach * 10.f;
			R.Pitch += WindupAnim * (1.f - Strike) * 55.f - Strike * 10.f + AlertAnim * 20.f; // coude plie, puis bras detendu
			break;
		case ELimb::Thigh:
			R.Pitch += L.Amp * Swing * Gait + Ready * 14.f + L.PlantPitch;
			break;
		case ELimb::Shin:
			R.Pitch -= L.Amp * Lift * Gait + Ready * 26.f; // le genou plie vers l'arriere ; flechi pour l'appel
			R.Pitch += L.PlantPitch; // v4.7 : pied pose sur un sol plus haut (trottoir, marche)
			break;
		case ELimb::HoundUpper:
			// Hound : tasse sur ses pattes avant de bondir, puis pattes avant lancees vers la proie
			R.Pitch += L.Amp * Swing * Gait + L.Sign * (WindupAnim * 18.f - Strike * 30.f);
			break;
		case ELimb::HoundLower:
			R.Pitch += L.Sign * L.Amp * Lift * Gait - L.Sign * WindupAnim * 30.f + L.PlantPitch;
			break;
		case ELimb::Wing:
		{
			const float Flap = FMath::Sin(Life * 20.f) * L.Amp;
			R = FRotator(0.f, 0.f, -Flap * L.Sign);
			break;
		}
		case ELimb::Tendril:
		{
			// Ondulation lente et desynchronisee de chaque faisceau, plus violente quand la masse se deplace
			const float T = Life * (1.6f + 0.25f * L.Phase) + L.Phase;
			R.Pitch += FMath::Sin(T) * L.Amp * (0.7f + 0.6f * Gait);
			R.Yaw += FMath::Sin(T * 0.7f + 1.3f) * L.Amp * 0.6f;
			R.Roll += FMath::Cos(T * 1.3f) * L.Amp * L.Sign;
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
		// Repere du parent de la tete : Visual pour les pieces rigides, le buste pour un maillage a squelette
		const USceneComponent* HeadParent = Head->GetAttachParent() ? Head->GetAttachParent() : Visual.Get();
		const FTransform& VT = HeadParent->GetComponentTransform();
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
	const float Speed = ((Kind == EBREntityKind::Bacteria) ? 14.f : 5.f) * (1.f + 3.f * AlertAnim);
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
