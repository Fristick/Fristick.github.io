// v4.4 : jumpscares. Quand une entite frappe le joueur, son modele se jette sur la camera ; chaque entite a sa facon
// d'arriver (ruee, saut depuis le sol, a-coups, apparition soudaine...), son son, ses effets d'image et son decor a
// l'ecran (dessine par ABRHUD::DrawJumpscare).
#include "BRCharacter.h"
#include "Backrooms.h"
#include "BREntity.h"
#include "BRWorld.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Controller.h"

namespace
{
	struct FScareStyle
	{
		float Dur;          // duree totale (s)
		float Delay;        // attente avant la ruee (silence du Partygoer, approche lente du Faceling)
		float Rush;         // duree de la ruee jusqu'au visage
		float Final;        // distance finale devant la camera (cm)
		float Pitch;        // la camera leve (+) ou baisse (-) les yeux
		FVector From;       // decalage du point de depart (repere camera : avant, droite, haut)
		float Shake;        // tremblement de la camera (degres)
		float Roll;         // roulis de la camera (degres)
		float FOV;          // variation du champ de vision (degres)
		float Spin;         // rotation du modele sur lui-meme (degres / s)
		float Tilt;         // tete penchee (degres)
		float Steps;        // > 0 : avance par a-coups (nombre de sauts)
		float Dark;         // la scene s'eteint autour d'elle (0..1)
		FLinearColor Tint;  // couleur de l'eclair a l'impact
		float FaceUp;       // hauteur du visage dans le modele (cm au-dessus des pieds)
		float FaceFwd;      // avancee du visage (cm)
		const TCHAR* Sound;
	};

	const FScareStyle& ScareStyle(EBREntityKind Kind)
	{
		// Ordre de EBREntityKind : Smiler, Hound, Faceling, SkinStealer, Deathmoth, Wretch, Partygoer, Clump, Bacteria
		static const FScareStyle Styles[] = {
			// Smiler : tout s'eteint, le sourire fonce depuis le noir et remplit l'ecran
			{ 1.8f, 0.05f, 0.32f, 34.f, 0.f, FVector::ZeroVector, 2.5f, 0.f, -12.f, 0.f, 0.f, 0.f, 1.f, FLinearColor(1.f, 1.f, 1.f), 62.f, 22.f, TEXT("S_Scare_Smiler") },
			// Hound : bondit depuis le sol, la camera est projetee, griffures rouges
			{ 1.5f, 0.f, 0.22f, 40.f, -8.f, FVector(0.f, 0.f, -90.f), 7.f, 9.f, 8.f, 0.f, 0.f, 0.f, 0.f, FLinearColor(1.f, 0.12f, 0.06f), 72.f, 48.f, TEXT("S_Scare_Hound") },
			// Faceling : s'approche lentement, tete penchee, sans visage... puis la neige d'une television
			{ 2.3f, 0.1f, 1.15f, 36.f, 0.f, FVector::ZeroVector, 0.6f, 0.f, -20.f, 0.f, 24.f, 0.f, 0.f, FLinearColor(0.75f, 0.75f, 0.75f), 165.f, 12.f, TEXT("S_Scare_Faceling") },
			// Skin-Stealer : la camera est arrachee vers lui, il surgit de cote, la scene vire a la chair
			{ 1.7f, 0.f, 0.2f, 38.f, 6.f, FVector(0.f, 140.f, 0.f), 4.f, -18.f, 4.f, 0.f, -10.f, 0.f, 0.f, FLinearColor(0.85f, 0.18f, 0.12f), 200.f, 20.f, TEXT("S_Scare_SkinStealer") },
			// Deathmoth : un essaim traverse l'ecran, le papillon s'ecrase sur le visage
			{ 1.8f, 0.f, 0.5f, 32.f, 4.f, FVector(0.f, -60.f, 60.f), 3.f, 5.f, -6.f, 0.f, 0.f, 0.f, 0.f, FLinearColor(0.65f, 0.48f, 0.25f), 35.f, 26.f, TEXT("S_Scare_Moth") },
			// Wretch : avance par a-coups entre des images noires
			{ 1.9f, 0.f, 0.75f, 38.f, -3.f, FVector::ZeroVector, 1.5f, 6.f, -8.f, 0.f, 12.f, 5.f, 0.f, FLinearColor(0.55f, 0.6f, 0.5f), 116.f, 30.f, TEXT("S_Scare_Wretch") },
			// Partygoer : silence... puis il est la, d'un coup, avec ses confettis et son sourire =)
			{ 2.4f, 0.5f, 0.04f, 48.f, 0.f, FVector::ZeroVector, 0.6f, 0.f, -16.f, 0.f, 0.f, 0.f, 0.f, FLinearColor(1.f, 0.85f, 0.15f), 172.f, 12.f, TEXT("S_Scare_Partygoer") },
			// Clump : il se jette sur vous, ses bras libres tendus, la bouche de lamproie ouverte devant la camera
			{ 1.7f, 0.f, 0.42f, 46.f, -6.f, FVector(0.f, 0.f, -40.f), 6.f, 0.f, 6.f, 0.f, 0.f, 0.f, 0.f, FLinearColor(0.55f, 0.04f, 0.04f), 52.f, 36.f, TEXT("S_Scare_Clump") },
			// Bacteria : elle vous domine ; la camera leve les yeux, sa tete descend vers vous, l'image se brouille
			{ 2.0f, 0.15f, 0.45f, 36.f, 32.f, FVector(0.f, 0.f, 160.f), 3.f, 0.f, -10.f, 0.f, 0.f, 0.f, 0.f, FLinearColor(0.9f, 0.9f, 0.95f), 205.f, 14.f, TEXT("S_Scare_Bacteria") },
		};
		static_assert(UE_ARRAY_COUNT(Styles) == static_cast<int32>(EBREntityKind::Count), "Un jumpscare par entite");
		return Styles[FMath::Clamp(static_cast<int32>(Kind), 0, static_cast<int32>(EBREntityKind::Count) - 1)];
	}

	FVector FacePoint(const ABREntity* E, const FScareStyle& St)
	{
		const USceneComponent* V = E ? E->GetVisual() : nullptr;
		return V ? V->GetComponentTransform().TransformPosition(FVector(St.FaceFwd, 0.f, St.FaceUp)) : FVector::ZeroVector;
	}
}

void ABRCharacter::PlayJumpscare(EBREntityKind Kind, ABREntity* Source, bool bLethal)
{
	if (!IsLocallyControlled() || bDead || !Camera)
	{
		return;
	}
	if (ScareKind >= 0)
	{
		bScareLethal = bScareLethal || bLethal; // deja en plein jumpscare : la mort attendra la fin
		return;
	}
	const FScareStyle& St = ScareStyle(Kind);
	StopClimb();
	ScareKind = static_cast<int32>(Kind);
	ScareTime = 0.f;
	ScareDur = St.Dur;
	ScareImpact = St.Delay + St.Rush;
	bScareLethal = bLethal;
	bScareSpawned = false;
	ScareTint = St.Tint;

	// Test (mode developpeur) : sans attaquant, l'entite apparait devant soi le temps du jumpscare
	ABRWorld* W = ABRWorld::Get(this);
	if (!Source && W && HasAuthority())
	{
		const FBREntityInfo& I = ABREntity::Info(Kind);
		FVector Loc = GetActorLocation() + GetViewDirection().GetSafeNormal2D() * 330.f;
		Loc.Z = GetActorLocation().Z - 88.f + (I.bFlying ? I.HoverHeight : I.HalfHeight + 5.f);
		Source = W->SpawnEntity(Kind, Loc);
		bScareSpawned = Source != nullptr;
	}
	ScareEntity = Source;
	if (Source && Source->GetVisual())
	{
		// Figee des maintenant (plus d'IA, pas de disparition) : son modele part de sa place actuelle
		const FTransform Here = Source->GetVisual()->GetComponentTransform();
		Source->SetScareTransform(&Here);
	}

	const FVector Eye = Camera->GetComponentLocation();
	FVector Face = Source ? FacePoint(Source, St) : Eye + GetViewDirection() * 300.f;
	if ((Face - Eye).SizeSquared() < 1.f)
	{
		Face = Eye + GetViewDirection() * 300.f;
	}
	FRotator View = (Face - Eye).Rotation();
	View.Pitch = FMath::Clamp(static_cast<float>(View.Pitch) + St.Pitch, -60.f, 70.f);
	View.Roll = 0.f;
	ScareView = View;
	ScareStart = Face + View.RotateVector(St.From);
	PlaySound2D(St.Sound, 1.f);
}

void ABRCharacter::UpdateJumpscare(float Dt)
{
	ScareCooldown = FMath::Max(0.f, ScareCooldown - Dt);
	if (ScareKind < 0)
	{
		ScareFOV = FMath::FInterpTo(ScareFOV, 0.f, Dt, 4.f);
		ScareFringe = FMath::FInterpTo(ScareFringe, 0.f, Dt, 3.f);
		ScareDark = FMath::FInterpTo(ScareDark, 0.f, Dt, 3.f);
		ScareFlash = FMath::FInterpTo(ScareFlash, 0.f, Dt, 5.f);
		return;
	}
	const FScareStyle& St = ScareStyle(static_cast<EBREntityKind>(ScareKind));
	ScareTime += Dt;
	const float T = ScareTime;
	float Rush = FMath::Clamp((T - St.Delay) / FMath::Max(St.Rush, 0.01f), 0.f, 1.f);
	if (St.Steps > 0.f)
	{
		Rush = FMath::FloorToFloat(Rush * St.Steps) / St.Steps; // a-coups
	}
	const float Ease = Rush * Rush * (3.f - 2.f * Rush);
	const bool bArrived = T >= ScareImpact;
	const float SinceImpact = FMath::Max(0.f, T - ScareImpact);

	// Camera : tournee vers l'entite, secouee, penchee ; le joueur ne la controle plus
	// v4.5 : tremblement reduit de 40 % : le cadrage reste sur le visage de l'entite (son geste porte la peur)
	const float ShakeAmp = 0.6f * St.Shake * (bArrived ? FMath::Max(0.35f, FMath::Exp(-SinceImpact * 1.5f)) : 0.35f * Ease);
	FRotator V = ScareView;
	V.Pitch += (FMath::Sin(T * 41.f) + 0.5f * FMath::Sin(T * 67.f)) * ShakeAmp;
	V.Yaw += (FMath::Cos(T * 37.f) + 0.5f * FMath::Sin(T * 59.f)) * ShakeAmp;
	V.Roll = St.Roll * FMath::Min(1.f, T / 0.25f) + FMath::Sin(T * 23.f) * ShakeAmp * 0.5f;
	if (Controller)
	{
		Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), V, Dt, 24.f));
	}

	// Effets d'image : champ de vision, aberration, assombrissement, eclair colore a l'impact
	ScareFOV = St.FOV * Ease;
	ScareFringe = 0.3f + 1.2f * Ease + (bArrived ? 2.5f * FMath::Exp(-SinceImpact * 4.f) : 0.f);
	ScareDark = St.Dark * FMath::Min(1.f, T / 0.15f);
	ScareFlash = bArrived ? FMath::Exp(-SinceImpact * 5.f) : 0.f;

	// Le modele se jette sur la camera
	if (ABREntity* E = ScareEntity.Get())
	{
		const FVector Eye = Camera->GetComponentLocation();
		const FVector Final = Eye + ScareView.Vector() * St.Final;
		FVector FacePos = FMath::Lerp(ScareStart, Final, Ease);
		if (bArrived)
		{
			FacePos += FVector(FMath::Sin(T * 53.f), FMath::Cos(T * 47.f), FMath::Sin(T * 61.f)) * 1.5f;
		}
		FRotator Look = (Eye - FacePos).Rotation();
		Look.Pitch = FMath::Clamp(static_cast<float>(Look.Pitch), -40.f, 40.f);
		Look.Roll = St.Tilt * Ease + St.Spin * T;
		const FVector Offset = Look.RotateVector(FVector(St.FaceFwd, 0.f, St.FaceUp));
		const FTransform TM(Look, FacePos - Offset, FVector(1.f));
		E->SetScareTransform(&TM);
	}

	if (ScareTime >= ScareDur)
	{
		EndJumpscare();
	}
}

void ABRCharacter::EndJumpscare()
{
	if (ABREntity* E = ScareEntity.Get())
	{
		E->SetScareTransform(nullptr);
		if (bScareSpawned)
		{
			E->Dismiss();
		}
	}
	ScareEntity.Reset();
	ScareKind = -1;
	ScareCooldown = 5.f;
	if (Controller)
	{
		FRotator R = Controller->GetControlRotation();
		R.Roll = 0.f;
		Controller->SetControlRotation(R);
	}
	if (bScareLethal && !bDead)
	{
		bScareLethal = false;
		Die(ScareKillerName, ScareKiller.Get());
	}
}
