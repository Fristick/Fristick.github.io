// v4.12 : lumiere de gameplay partagee : ce que les regles du jeu (Smilers, apparitions dans l'ombre, sante mentale)
// considerent comme eclaire. Avant la v4.12, seuls les plafonniers comptaient (ABRWorld::LightLevelAt renvoyait 0 dans
// un niveau sans plafonnier) : les balises du Niveau 6, pourtant de vraies lumieres, etaient du noir pour les regles.
//
// Les sources de mission actives (balises allumees, sortie de secours eclairee, porche allume) sont enregistrees aupres
// du monde sur chaque machine, d'apres l'etat replique des mecanismes ; elles sont autonomes (une coupure des neons ne
// les eteint pas). L'apport d'une source suit la meme loi que les plafonniers ; un mur entre la source et le point
// l'annule (trace par ABRWorld). C++ pur : le banc Tools/Light/test_light.cpp verifie les portees annoncees aux joueurs.
#pragma once

namespace BRLight
{
	/** Balise allumee du Niveau 6 (2200 lm, 9 m de portee visuelle) */
	constexpr float BeaconRadius = 900.f;
	constexpr float BeaconIntensity = 2.f;
	/** Sortie de secours eclairee (Niveau 6), porche allume (Niveau 9) */
	constexpr float LitGateRadius = 700.f;
	constexpr float LitGateIntensity = 1.6f;
	/** Seuils lus par les regles : au-dessus, un Smiler hors poursuite se dissipe (BREntity, ThinkSmiler) ; en dessous,
	 *  une entite de l'ombre peut apparaitre (BRWorld, apparitions) */
	constexpr float SmilerVanish = 0.5f;
	constexpr float NeedsDark = 0.12f;

	/** Apport d'une source a une distance (cm) : (1 - d/R)^2 x intensite, nul au-dela du rayon */
	inline float Contribution(float Dist, float Radius, float Intensity)
	{
		if (Radius <= 0.f || Dist >= Radius)
		{
			return 0.f;
		}
		const float T = 1.f - Dist / Radius;
		return T * T * Intensity;
	}
}
