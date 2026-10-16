// v4.15 (correctifs) : -BRAutoTest -BRAutoTestV415Fix : les seules etapes de non-regression des correctifs v4.15 des places
// des mecanismes et des tests de marche (Saved/TestRuns/analysis : diag_v413_3, diag_v413_4, diag_interchange_verified),
// pour une verification courte :
//   - Niveaux 8 et 9, graine 4111 (niveaux 643828852 et 774538619) : rochers et maison hors des places des mecanismes ;
//     chaque mecanisme a une place debout a portee de la visee et selon la regle de l'hote, essai a blanc depuis elle ;
//   - Level 6, graines 4136, 4126, 6 et 208 : salle de l'aile, aucun avertissement, postes et cages atteignables, poste 1
//     tourne depuis sa place ;
//   - Poolrooms, graine 1217121623 (niveau 125584919) : mission a pied, vanne B depuis l'eau des canaux, sortie par le sas,
//     sans nager.
// Les memes etapes font partie de -BRAutoTestV411 (AddMissionSteps : places des mecanismes de chaque niveau), de
// -BRAutoTestV412 (Poolrooms) et de -BRAutoTestV413 (Level 6), donc du test complet : cette suite n'y est pas ajoutee une
// seconde fois. Une verification qui ne peut pas etre faite est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"

void ABRAutoTest::AddV415FixSteps()
{
	Add(TEXT("v4.15 correctifs : preparation"), 0.f, [this]()
	{
		FLevelReport R;
		R.Title = TEXT("v4.15 (correctifs) : places des mecanismes (Niveaux 6, 8, 9), Poolrooms a pied sans nager");
		R.Scene = TEXT("v4.15");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		return true;
	});
	// Niveaux 8 et 9 : la graine de -BRAutoTestV411 (rochers du Niveau 8, perron de la maison du Niveau 9)
	for (const int32 PlaceLevel : { 8, 9 })
	{
		AddSeedLoad(PlaceLevel, 4111, FString::Printf(TEXT("v4.15 correctifs : Niveau %d (graine 4111)"), PlaceLevel));
		AddMissionPlaceSweep(PlaceLevel, false);
	}
	// Level 6 : aile, postes et cages (BRAutoTestV413Lidar.cpp)
	AddV415LidarPlaceSteps();
	// Poolrooms : vanne B et sas a pied (BRAutoTestV412.cpp)
	AddV415PoolroomsSteps();
}
