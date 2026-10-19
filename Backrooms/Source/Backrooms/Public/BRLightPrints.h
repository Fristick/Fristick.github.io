// v4.16 (relecture) : empreintes attendues de la grille de lumiere (ABRWorld::GetLightGridFingerprint, cellules [-Half, Half[)
// pour quelques graines. Une seule table pour le banc hors moteur (Tools/Light/test_light.cpp, section D : il recalcule chaque
// empreinte avec son portage de la grille et echoue si elle differe) et pour -BRAutoTestV416Light (le jeu la recalcule) : un
// changement de la grille du jeu ou du portage fait echouer le banc et oblige a mettre les deux a jour. C++ pur.
#pragma once

namespace BRLightPrints
{
	struct FPrint
	{
		int Level;
		unsigned int UserSeed;
		unsigned int Print;
	};
	constexpr int Half = 12;
	constexpr FPrint Table[] = {
		{ 0, 9u, 0x33387e10u }, { 0, 4242u, 0xb142f19cu }, { 1, 4242u, 0xd0705669u }, { 2, 4242u, 0x94f0c4d7u },
		{ 4, 4242u, 0x8d3f54e3u }, { 5, 4242u, 0xb752f85fu }, { 8, 4242u, 0x9c633728u }, { 37, 4242u, 0x5b6e31f0u } };
	constexpr int Count = static_cast<int>(sizeof(Table) / sizeof(Table[0]));
}
