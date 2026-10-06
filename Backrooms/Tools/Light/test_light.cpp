// v4.12 : banc hors moteur de la lumiere de gameplay (constat 4.4).
//
// Il lit les constantes et la loi d'apport de BRLightLogic.h (celles du jeu) et verifie ce qui est annonce aux joueurs :
// la portee a laquelle une balise allumee dissipe un Smiler hors poursuite (indice du carnet, Niveau 6), celle ou plus
// aucune entite de l'ombre n'apparait, la sortie eclairee, deux balises proches, l'independance vis-a-vis des
// coupures (les sources de mission ne sont pas multipliees par la puissance des neons). L'occlusion par les murs et la
// replication sont verifiees en jeu (-BRAutoTestV412, prepare).
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Light/test_light.cpp -o test_light
#include "BRLightLogic.h"

#include <cmath>
#include <cstdio>

namespace
{
	int GChecks = 0, GFailures = 0;
	void Line(const char* What, bool bOk)
	{
		++GChecks;
		GFailures += bOk ? 0 : 1;
		std::printf("  %-80s %s\n", What, bOk ? "OK" : "ECHEC");
	}

	// Portee (cm) au-dela de laquelle l'apport d'une source passe sous un seuil
	float Reach(float Radius, float Intensity, float Threshold)
	{
		return Intensity <= Threshold ? 0.f : Radius * (1.f - std::sqrt(Threshold / Intensity));
	}

	// Recherche numerique de la meme portee avec la loi du jeu (aucune formule recopiee)
	float ReachByScan(float Radius, float Intensity, float Threshold)
	{
		float Last = 0.f;
		for (float D = 0.f; D <= Radius; D += 0.5f)
		{
			if (BRLight::Contribution(D, Radius, Intensity) > Threshold)
			{
				Last = D;
			}
		}
		return Last;
	}
}

int main()
{
	using namespace BRLight;
	std::printf("# Banc de la lumiere de gameplay v4.12 (BRLightLogic.h)\n\n");
	const float Vanish = ReachByScan(BeaconRadius, BeaconIntensity, SmilerVanish);
	const float NoSpawn = ReachByScan(BeaconRadius, BeaconIntensity, NeedsDark);
	const float Gate = ReachByScan(LitGateRadius, LitGateIntensity, SmilerVanish);
	std::printf("  balise : Smiler hors poursuite dissipe a moins de %.1f m ; aucune entite de l'ombre a moins de %.1f m\n", Vanish / 100.f, NoSpawn / 100.f);
	std::printf("  sortie de secours eclairee / porche : Smiler dissipe a moins de %.1f m\n", Gate / 100.f);
	Line("formule de l'indice (R x (1 - racine(seuil / I))) = recherche avec la loi du jeu", std::fabs(Reach(BeaconRadius, BeaconIntensity, SmilerVanish) - Vanish) < 1.f);
	const int Announced = static_cast<int>(std::floor(Reach(BeaconRadius, BeaconIntensity, SmilerVanish) / 100.f));
	char Buf[160];
	std::snprintf(Buf, sizeof(Buf), "indice du carnet : \"a moins de %d m\" ; a %d m l'effet a toujours lieu (apport %.2f > %.2f)", Announced, Announced,
		Contribution(Announced * 100.f, BeaconRadius, BeaconIntensity), SmilerVanish);
	Line(Buf, Contribution(Announced * 100.f, BeaconRadius, BeaconIntensity) > SmilerVanish);
	Line("au-dela de la portee visuelle (9 m), aucun apport", Contribution(BeaconRadius, BeaconRadius, BeaconIntensity) == 0.f && Contribution(1200.f, BeaconRadius, BeaconIntensity) == 0.f);
	Line("apport decroissant avec la distance", Contribution(100.f, BeaconRadius, BeaconIntensity) > Contribution(300.f, BeaconRadius, BeaconIntensity));
	// Deux balises a 8 m l'une de l'autre : le milieu (4 m de chacune) est eclaire au-dessus du seuil
	const float Mid = 2.f * Contribution(400.f, BeaconRadius, BeaconIntensity);
	std::snprintf(Buf, sizeof(Buf), "deux balises a 8 m : milieu a %.2f (somme des apports)", Mid);
	Line(Buf, Mid > SmilerVanish);
	// Coupure : les plafonniers sont multiplies par la puissance (0 en coupure noire), pas les sources de mission
	const float Fixtures = 0.8f, Power = 0.f;
	const float DuringBlackout = Fixtures * Power + Contribution(200.f, BeaconRadius, BeaconIntensity);
	Line("coupure noire : la balise eclaire toujours (source autonome)", DuringBlackout > SmilerVanish);
	std::printf("\nVerifications : %d, echecs : %d\nRESULTAT : %s\n", GChecks, GFailures, GFailures == 0 ? "OK" : "ECHEC");
	return GFailures == 0 ? 0 : 1;
}
