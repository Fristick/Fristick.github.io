#include "BRLevels.h"

namespace
{
	FLinearColor C(float R, float G, float B) { return FLinearColor(R, G, B, 1.f); }

	FBRSurface S(const TCHAR* Tex, const FLinearColor& Tint, float Scale, float Rough = 0.85f, float Grime = 0.35f)
	{
		return FBRSurface(FName(Tex), Tint, Scale, Rough, Grime);
	}

	FBREntitySpawn E(EBREntityKind K, float W)
	{
		FBREntitySpawn Out;
		Out.Kind = K;
		Out.Weight = W;
		return Out;
	}

	FBRExitDef X(int32 Target, EBRExitStyle Style, float Chance)
	{
		FBRExitDef Out;
		Out.Target = Target;
		Out.Style = Style;
		Out.ChancePerChunk = Chance;
		return Out;
	}

	// ---------------------------------------------------------------- Niveau 0
	FBRLevelDef Level0()
	{
		FBRLevelDef D;
		D.Number = 0;
		D.Title = TEXT("Threshold");
		D.Nickname = TEXT("Le Seuil - \"Yellow Hell\"");
		D.Description = TEXT("Un labyrinthe infini de salles de bureau vides : papier peint jaune moisi, moquette humide, ")
			TEXT("n\u00e9ons qui bourdonnent sans fin. Les salles se ressemblent toutes. Certains murs semblent... faux.");
		D.SurvivalClass = 1;
		D.ClassText = TEXT("Classe 1 : S\u00fbr - Stable - Entit\u00e9s quasi absentes");
		D.Layout = EBRLayout::Rooms;
		D.CellSize = 350.f; D.WallHeight = 290.f; D.WallThickness = 20.f;
		D.WallLineChance = 0.55f; D.SegmentLength = 4; D.DoorChance = 0.3f; D.DoorWidth = 150.f;
		D.PillarChance = 0.04f; D.PillarSize = 45.f; D.OpenZoneChance = 0.15f;
		D.Floor = S(TEXT("T_L0_Carpet"), C(1, 1, 1), 220.f, 0.95f, 0.3f);
		D.Wall = S(TEXT("T_L0_Wallpaper"), C(1, 1, 1), 120.f, 0.8f, 0.3f);
		D.Ceiling = S(TEXT("T_L0_Ceiling"), C(1, 1, 1), 120.f, 0.9f, 0.15f);
		D.Trim = S(TEXT("T_L0_Wallpaper"), C(0.55f, 0.48f, 0.32f), 100.f, 0.6f, 0.2f);
		D.Pillar = D.Wall;
		D.Fixture = EBRFixture::Panel;
		D.LightChance = 0.38f; D.BrokenChance = 0.06f; D.FlickerChance = 0.07f;
		D.LightLumens = 2400.f; D.LightColor = C(1.f, 0.96f, 0.84f); D.LightRadius = 620.f;
		D.DarkZoneChance = 0.06f; D.ShadowChance = 0.15f;
		D.FogDensity = 0.08f; D.FogColor = C(0.32f, 0.29f, 0.16f);
		D.SceneTint = C(1.f, 0.98f, 0.9f); D.Saturation = 0.95f; D.Contrast = 1.05f;
		D.MinEV = 3.f; D.MaxEV = 9.f; D.ExposureBias = 0.3f; D.Vignette = 0.5f; D.Grain = 0.3f;
		D.AmbientSound = TEXT("S_Amb_L0"); D.AmbientVolume = 0.5f;
		D.HumSound = TEXT("S_Hum"); D.HumVolume = 0.55f;
		D.Step = EBRStep::Carpet;
		D.SanityDrain = 0.08f;
		D.Entities = { E(EBREntityKind::Smiler, 0.6f), E(EBREntityKind::Bacteria, 0.4f) };
		D.MaxEntities = 1; D.SpawnInterval = 100.f;
		D.AlmondWaterChance = 0.3f; D.BatteryChance = 0.15f; D.NoteChance = 0.25f;
		D.BandageChance = 0.12f; D.EnergyBarChance = 0.08f; D.GearChance = 0.03f;
		D.Exits = { X(1, EBRExitStyle::NoclipWall, 0.18f), X(37, EBRExitStyle::NoclipFloor, 0.04f) };
		// Comme dans Escape Together : cassettes VHS + enregistrement pendant une coupure pour stabiliser la sortie
		D.bRequireObjectives = true; D.VHSRequired = 6; D.VHSChance = 0.22f;
		D.bBlackouts = true; D.BlackoutFirst = 75.f; D.BlackoutMinInterval = 120.f; D.BlackoutMaxInterval = 220.f;
		D.WallDetailChance = 0.1f;
		D.Wall.FloorGrime = 0.6f;
		D.Pillar.FloorGrime = 0.6f;
		D.bPhenomena = true;
		D.Notes = {
			TEXT("Si tu lis ceci, tu as \"noclipp\u00e9\" hors de la r\u00e9alit\u00e9. Ne panique pas. Les murs ne bougent pas : c'est toi qui te perds."),
			TEXT("Le bourdonnement ne s'arr\u00eate jamais. J'ai compte plus de six cents salles. Toujours la m\u00eame moquette humide. Toujours la m\u00eame odeur."),
			TEXT("Certains murs ont l'air FAUX, comme une image qui se brouille. Touche-les. C'est comme \u00e7a que j'ai quitt\u00e9 cet endroit."),
			TEXT("L'eau d'amande calme l'esprit. Garde toujours une bouteille sur toi. {Drink} pour boire."),
			TEXT("Si les lumi\u00e8res sont mortes dans une zone, n'y entre pas. Quelque chose y sourit dans le noir."),
			TEXT("Jour 3 (je crois). J'ai entendu des pas derri\u00e8re moi. Quand je me suis retourn\u00e9, il n'y avait que le bourdonnement."),
			TEXT("Les sorties ne tiennent pas. Il faut r\u00e9cup\u00e9rer les six cassettes et filmer le noir pendant une coupure. Apr\u00e8s, les murs c\u00e8dent."),
			TEXT("Quand les n\u00e9ons s'\u00e9teignent, sors le cam\u00e9scope et appuie sur {NightVision}. La grande chose maigre fait du bruit quand elle approche. Ne cours pas vers elle.")
		};
		return D;
	}

	// ---------------------------------------------------------------- Niveau 1
	FBRLevelDef Level1()
	{
		FBRLevelDef D;
		D.Number = 1;
		D.Title = TEXT("Habitable Zone");
		D.Nickname = TEXT("Zone habitable");
		D.Description = TEXT("Un entrep\u00f4t de b\u00e9ton immense et brumeux, ponctu\u00e9 de piliers et de flaques. Des caisses de ")
			TEXT("ravitaillement apparaissent parfois. Quand les n\u00e9ons s'\u00e9teignent, quelque chose se met \u00e0 r\u00f4der.");
		D.SurvivalClass = 1;
		D.ClassText = TEXT("Classe 1 : S\u00fbr - Stable - Peu d'entit\u00e9s");
		D.Layout = EBRLayout::Rooms;
		D.CellSize = 600.f; D.WallHeight = 560.f; D.WallThickness = 40.f;
		D.WallLineChance = 0.28f; D.SegmentLength = 6; D.DoorChance = 0.45f; D.DoorWidth = 260.f;
		D.PillarChance = 0.55f; D.PillarSize = 80.f; D.OpenZoneChance = 0.3f;
		D.bTrim = false; D.bLintels = true;
		D.Floor = S(TEXT("T_ConcreteFloor"), C(1, 1, 1), 400.f, 0.55f, 0.35f);
		D.Wall = S(TEXT("T_Concrete"), C(0.95f, 0.95f, 0.95f), 350.f, 0.85f, 0.4f);
		D.Ceiling = S(TEXT("T_Concrete"), C(0.6f, 0.6f, 0.6f), 400.f, 0.9f, 0.3f);
		D.Pillar = S(TEXT("T_Concrete"), C(0.85f, 0.85f, 0.85f), 300.f, 0.85f, 0.5f);
		D.Fixture = EBRFixture::Tube;
		D.LightChance = 0.35f; D.BrokenChance = 0.12f; D.FlickerChance = 0.15f;
		D.LightLumens = 4500.f; D.LightColor = C(0.88f, 0.94f, 1.f); D.LightRadius = 1100.f;
		D.DarkZoneChance = 0.12f; D.ShadowChance = 0.2f;
		D.FogDensity = 0.16f; D.FogColor = C(0.22f, 0.24f, 0.27f);
		D.SceneTint = C(0.95f, 0.98f, 1.f); D.Saturation = 0.85f; D.Contrast = 1.08f;
		D.MinEV = 3.f; D.MaxEV = 9.f;
		D.AmbientSound = TEXT("S_Amb_Industrial"); D.AmbientVolume = 0.6f;
		D.HumSound = TEXT("S_Hum"); D.HumVolume = 0.3f;
		D.Step = EBRStep::Hard;
		D.SanityDrain = 0.1f;
		D.Entities = { E(EBREntityKind::Smiler, 1.f), E(EBREntityKind::Faceling, 1.f), E(EBREntityKind::Hound, 0.4f) };
		D.MaxEntities = 2; D.SpawnInterval = 60.f;
		D.AlmondWaterChance = 0.4f; D.BatteryChance = 0.3f; D.NoteChance = 0.2f;
		D.Exits = { X(2, EBRExitStyle::Door, 0.18f), X(4, EBRExitStyle::Elevator, 0.05f) };
		D.Props = EBRProps::Warehouse; D.PropDensity = 0.12f;
		D.Notes = {
			TEXT("Les caisses contiennent parfois de l'eau d'amande et des piles. Fouille-les. Ici, on peut presque survivre."),
			TEXT("Quand les tubes au plafond clignotent, eloigne-toi. Quand ils s'\u00e9teignent, cours."),
			TEXT("Les portes de service m\u00e8nent plus bas. Plus bas, c'est pire. Mais c'est peut-etre la seule sortie."),
			TEXT("Les Facelings ne sont pas m\u00e9chants. La plupart. Ne les regarde pas trop longtemps.")
		};
		D.bBlackouts = true; D.BlackoutFirst = 120.f;
		D.Wall.FloorGrime = 0.5f; D.Pillar.FloorGrime = 0.5f;
		D.BandageChance = 0.15f; D.EnergyBarChance = 0.12f; D.GearChance = 0.04f; D.WallDetailChance = 0.05f;
		return D;
	}

	// ---------------------------------------------------------------- Niveau 2
	FBRLevelDef Level2()
	{
		FBRLevelDef D;
		D.Number = 2;
		D.Title = TEXT("Abandoned Utility Halls");
		D.Nickname = TEXT("Couloirs techniques abandonn\u00e9s - \"Pipe Dreams\"");
		D.Description = TEXT("Un r\u00e9seau sans fin de tunnels de maintenance \u00e9troits, en b\u00e9ton sale, couverts de tuyaux. ")
			TEXT("La chaleur y est \u00e9touffante et l'\u00e9clairage rare.");
		D.SurvivalClass = 2;
		D.ClassText = TEXT("Classe 2 : Instable - Entit\u00e9s pr\u00e9sentes");
		D.Layout = EBRLayout::Maze;
		D.CellSize = 260.f; D.WallHeight = 280.f; D.WallThickness = 30.f;
		D.LoopChance = 0.15f; D.OpenZoneChance = 0.06f;
		D.bTrim = false; D.bLintels = false;
		D.Floor = S(TEXT("T_ConcreteFloor"), C(0.8f, 0.76f, 0.7f), 250.f, 0.7f, 0.5f);
		D.Wall = S(TEXT("T_Concrete"), C(0.72f, 0.68f, 0.6f), 250.f, 0.9f, 0.55f);
		D.Ceiling = S(TEXT("T_ConcreteDark"), C(1, 1, 1), 250.f, 0.9f, 0.4f);
		D.Fixture = EBRFixture::Bulb;
		D.LightChance = 0.22f; D.BrokenChance = 0.2f; D.FlickerChance = 0.15f;
		D.LightLumens = 1300.f; D.LightColor = C(1.f, 0.7f, 0.45f); D.LightRadius = 560.f;
		D.DarkZoneChance = 0.15f; D.ShadowChance = 0.2f;
		D.FogDensity = 0.12f; D.FogColor = C(0.18f, 0.13f, 0.08f);
		D.SceneTint = C(1.f, 0.93f, 0.85f); D.Saturation = 0.85f; D.Contrast = 1.12f;
		D.MinEV = 2.5f; D.MaxEV = 9.f;
		D.AmbientSound = TEXT("S_Amb_Industrial"); D.AmbientVolume = 0.7f;
		D.HumSound = TEXT("S_Hum"); D.HumVolume = 0.15f;
		D.Step = EBRStep::Hard;
		D.SanityDrain = 0.14f;
		D.Entities = { E(EBREntityKind::Wretch, 1.f), E(EBREntityKind::Hound, 0.7f), E(EBREntityKind::Clump, 0.4f), E(EBREntityKind::Smiler, 0.5f) };
		D.MaxEntities = 3; D.SpawnInterval = 50.f;
		D.AlmondWaterChance = 0.25f; D.BatteryChance = 0.25f; D.NoteChance = 0.2f;
		D.Exits = { X(3, EBRExitStyle::Door, 0.15f), X(1, EBRExitStyle::Ladder, 0.05f) };
		D.Props = EBRProps::Pipes; D.PropDensity = 0.6f;
		D.Notes = {
			TEXT("Il fait si chaud. Les tuyaux sifflent. Ne touche pas ceux qui fument."),
			TEXT("Les Wretches \u00e9taient des gens comme nous. Ils sont lents. Ne les laisse pas s'approcher."),
			TEXT("Une porte de secours plus loin. On dit qu'elle m\u00e8ne \u00e0 la Station \u00e9lectrique. Pr\u00e9pare-toi.")
		};
		D.bBlackouts = true; D.BlackoutFirst = 100.f;
		D.Wall.FloorGrime = 0.5f;
		D.BandageChance = 0.12f; D.EnergyBarChance = 0.1f; D.GearChance = 0.03f;
		return D;
	}

	// ---------------------------------------------------------------- Niveau 3
	FBRLevelDef Level3()
	{
		FBRLevelDef D;
		D.Number = 3;
		D.Title = TEXT("Electrical Station");
		D.Nickname = TEXT("Station \u00e9lectrique");
		D.Description = TEXT("Un d\u00e9dale assourdissant de briques, de grilles et de machines \u00e9lectriques. ")
			TEXT("L'un des niveaux les plus hostiles connus : les entit\u00e9s y sont nombreuses.");
		D.SurvivalClass = 4;
		D.ClassText = TEXT("Classe 4 : Dangereux - Infest\u00e9 d'entit\u00e9s");
		D.Layout = EBRLayout::Maze;
		D.CellSize = 300.f; D.WallHeight = 320.f; D.WallThickness = 30.f;
		D.LoopChance = 0.2f; D.OpenZoneChance = 0.1f;
		D.bTrim = false; D.bLintels = false;
		D.Floor = S(TEXT("T_MetalPanel"), C(0.6f, 0.6f, 0.6f), 200.f, 0.6f, 0.5f);
		D.Floor.Metallic = 0.5f;
		D.Wall = S(TEXT("T_Brick"), C(0.9f, 0.85f, 0.8f), 220.f, 0.9f, 0.5f);
		D.Ceiling = S(TEXT("T_ConcreteDark"), C(1, 1, 1), 250.f, 0.9f, 0.4f);
		D.Fixture = EBRFixture::Bulb;
		D.LightChance = 0.3f; D.BrokenChance = 0.15f; D.FlickerChance = 0.25f;
		D.LightLumens = 1500.f; D.LightColor = C(1.f, 0.85f, 0.6f); D.LightRadius = 620.f;
		D.DarkZoneChance = 0.15f; D.ShadowChance = 0.2f;
		D.FogDensity = 0.1f; D.FogColor = C(0.14f, 0.12f, 0.1f);
		D.SceneTint = C(1.f, 0.95f, 0.88f); D.Saturation = 0.8f; D.Contrast = 1.15f;
		D.MinEV = 2.5f; D.MaxEV = 9.f;
		D.AmbientSound = TEXT("S_Amb_Machinery"); D.AmbientVolume = 0.75f;
		D.HumSound = TEXT("S_Hum"); D.HumVolume = 0.25f;
		D.Step = EBRStep::Hard;
		D.SanityDrain = 0.2f;
		D.Entities = { E(EBREntityKind::Hound, 1.f), E(EBREntityKind::SkinStealer, 0.6f), E(EBREntityKind::Smiler, 0.6f),
			E(EBREntityKind::Deathmoth, 0.5f), E(EBREntityKind::Wretch, 0.6f) };
		D.MaxEntities = 4; D.SpawnInterval = 35.f;
		D.AlmondWaterChance = 0.25f; D.BatteryChance = 0.3f; D.NoteChance = 0.2f;
		D.Exits = { X(4, EBRExitStyle::Elevator, 0.15f), X(2, EBRExitStyle::Door, 0.05f) };
		D.Props = EBRProps::Electrical; D.PropDensity = 0.35f;
		D.Notes = {
			TEXT("LES HOUNDS SENTENT LA PEUR. Ne cours pas devant eux. Regarde-les dans les yeux et recule."),
			TEXT("Si quelqu'un t'appelle par ton nom dans ces couloirs, ce n'est pas un humain. Ne r\u00e9ponds pas."),
			TEXT("Les ascenseurs fonctionnent encore. Ils m\u00e8nent \u00e0 un bureau. Un endroit calme. Trouve-les.")
		};
		D.bBlackouts = true; D.BlackoutFirst = 90.f;
		D.Wall.FloorGrime = 0.45f;
		D.BandageChance = 0.15f; D.EnergyBarChance = 0.1f; D.GearChance = 0.04f;
		return D;
	}

	// ---------------------------------------------------------------- Niveau 4
	FBRLevelDef Level4()
	{
		FBRLevelDef D;
		D.Number = 4;
		D.Title = TEXT("Abandoned Office");
		D.Nickname = TEXT("Bureau abandonn\u00e9");
		D.Description = TEXT("Un open-space vide, propre et bien \u00e9clair\u00e9. Bureaux, chaises, fontaines \u00e0 eau d'amande. ")
			TEXT("Un rare moment de r\u00e9pit... si l'on ignore les fen\u00eatres qui donnent sur le n\u00e9ant.");
		D.SurvivalClass = 1;
		D.ClassText = TEXT("Classe 1 : S\u00fbr - Stable - Peu d'entit\u00e9s");
		D.Layout = EBRLayout::Rooms;
		D.CellSize = 400.f; D.WallHeight = 300.f; D.WallThickness = 15.f;
		D.WallLineChance = 0.45f; D.SegmentLength = 3; D.DoorChance = 0.35f; D.DoorWidth = 120.f;
		D.PillarChance = 0.02f; D.OpenZoneChance = 0.3f;
		D.Floor = S(TEXT("T_OfficeCarpet"), C(1, 1, 1), 200.f, 0.95f, 0.2f);
		D.Wall = S(TEXT("T_OfficeWall"), C(1, 1, 1), 200.f, 0.8f, 0.15f);
		D.Ceiling = S(TEXT("T_L0_Ceiling"), C(1.f, 1.f, 1.03f), 120.f, 0.9f, 0.05f);
		D.Trim = S(TEXT("T_OfficeWall"), C(0.35f, 0.35f, 0.37f), 100.f, 0.5f, 0.1f);
		D.Pillar = D.Wall;
		D.Fixture = EBRFixture::Panel;
		D.LightChance = 0.55f; D.BrokenChance = 0.02f; D.FlickerChance = 0.02f;
		D.LightLumens = 3000.f; D.LightColor = C(0.95f, 0.97f, 1.f); D.LightRadius = 700.f;
		D.ShadowChance = 0.12f;
		D.FogDensity = 0.04f; D.FogColor = C(0.3f, 0.3f, 0.32f);
		D.MinEV = 3.f; D.MaxEV = 9.f;
		D.AmbientSound = TEXT("S_Amb_L0"); D.AmbientVolume = 0.35f;
		D.HumSound = TEXT("S_Hum"); D.HumVolume = 0.25f;
		D.Step = EBRStep::Carpet;
		D.SanityDrain = 0.03f;
		D.Entities = { E(EBREntityKind::Faceling, 1.f), E(EBREntityKind::Partygoer, 0.15f) };
		D.MaxEntities = 1; D.SpawnInterval = 120.f;
		D.AlmondWaterChance = 0.5f; D.BatteryChance = 0.3f; D.NoteChance = 0.2f;
		D.Exits = { X(5, EBRExitStyle::Door, 0.15f), X(1, EBRExitStyle::Elevator, 0.05f) };
		D.Props = EBRProps::Office; D.PropDensity = 0.35f;
		D.Notes = {
			TEXT("Les fontaines distribuent de l'eau d'amande. Remplis tes bouteilles ici."),
			TEXT("La cage d'escalier descend vers un h\u00f4tel. Ne t'y attarde pas."),
			TEXT("Si tu vois quelqu'un de jaune qui sourit, NE LE QUITTE PAS DES YEUX.")
		};
		D.Wall.FloorGrime = 0.3f; D.WallDetailChance = 0.12f;
		D.BandageChance = 0.12f; D.EnergyBarChance = 0.15f; D.GearChance = 0.04f;
		return D;
	}

	// ---------------------------------------------------------------- Niveau 5
	FBRLevelDef Level5()
	{
		FBRLevelDef D;
		D.Number = 5;
		D.Title = TEXT("Terror Hotel");
		D.Nickname = TEXT("L'h\u00f4tel de la terreur");
		D.Description = TEXT("Un h\u00f4tel des ann\u00e9es 1920 qui ne finit jamais : couloirs feutr\u00e9s, moquette rouge, portes ")
			TEXT("num\u00e9rot\u00e9es qui ne s'ouvrent pas. Une musique lointaine. L'impression d'\u00eatre observ\u00e9.");
		D.SurvivalClass = 2;
		D.ClassText = TEXT("Classe 2 : Instable - Entit\u00e9s pr\u00e9sentes");
		D.Layout = EBRLayout::Hotel;
		D.CellSize = 300.f; D.WallHeight = 320.f; D.Spacing = 5; D.OpenZoneChance = 0.08f;
		D.Floor = S(TEXT("T_HotelCarpet"), C(1, 1, 1), 150.f, 0.95f, 0.25f);
		D.Wall = S(TEXT("T_HotelWallpaper"), C(1, 1, 1), 120.f, 0.8f, 0.3f);
		D.Solid = D.Wall;
		D.Ceiling = S(TEXT("T_OfficeWall"), C(0.85f, 0.78f, 0.66f), 200.f, 0.9f, 0.3f);
		D.Trim = S(TEXT("T_Wood"), C(1, 1, 1), 120.f, 0.5f, 0.1f);
		D.Pillar = D.Trim;
		D.Fixture = EBRFixture::Sconce;
		D.LightChance = 0.5f; D.BrokenChance = 0.15f; D.FlickerChance = 0.12f;
		D.LightLumens = 900.f; D.LightColor = C(1.f, 0.7f, 0.4f); D.LightRadius = 520.f;
		D.DarkZoneChance = 0.1f; D.ShadowChance = 0.2f;
		D.FogDensity = 0.06f; D.FogColor = C(0.2f, 0.12f, 0.08f);
		D.SceneTint = C(1.f, 0.92f, 0.85f); D.Saturation = 0.9f; D.Contrast = 1.1f;
		D.MinEV = 2.5f; D.MaxEV = 9.f;
		D.AmbientSound = TEXT("S_Amb_Hotel"); D.AmbientVolume = 0.6f;
		D.Step = EBRStep::Carpet;
		D.SanityDrain = 0.12f;
		D.Entities = { E(EBREntityKind::SkinStealer, 0.6f), E(EBREntityKind::Partygoer, 0.5f), E(EBREntityKind::Faceling, 0.6f) };
		D.MaxEntities = 2; D.SpawnInterval = 60.f;
		D.AlmondWaterChance = 0.3f; D.BatteryChance = 0.25f; D.NoteChance = 0.2f;
		D.Exits = { X(6, EBRExitStyle::HotelDoor, 0.12f), X(4, EBRExitStyle::Door, 0.04f) };
		D.Props = EBRProps::Hotel; D.PropDensity = 0.5f;
		D.Notes = {
			TEXT("Chambre 1508... ou \u00e9tait-ce 5108 ? Les num\u00e9ros changent quand on ne regarde pas."),
			TEXT("La chaufferie est au bout du couloir. Apr\u00e8s elle, il n'y a plus de lumi\u00e8re du tout."),
			TEXT("Quelqu'un frappe aux portes la nuit. N'ouvre jamais.")
		};
		D.bBlackouts = true; D.BlackoutFirst = 110.f;
		D.Wall.FloorGrime = 0.4f; D.WallDetailChance = 0.06f;
		D.BandageChance = 0.12f; D.EnergyBarChance = 0.1f; D.GearChance = 0.03f;
		return D;
	}

	// ---------------------------------------------------------------- Niveau 6
	FBRLevelDef Level6()
	{
		FBRLevelDef D;
		D.Number = 6;
		D.Title = TEXT("Lights Out");
		D.Nickname = TEXT("Lumi\u00e8res \u00e9teintes");
		D.Description = TEXT("Une obscurit\u00e9 totale. Aucune source de lumi\u00e8re ne fonctionne, sauf celle que l'on apporte. ")
			TEXT("Des bruits \u00e9tranges, des murmures... et des sourires dans le noir.");
		D.SurvivalClass = 4;
		D.ClassText = TEXT("Classe 4 : Dangereux - Obscurit\u00e9 totale");
		D.Layout = EBRLayout::Rooms;
		D.CellSize = 350.f; D.WallHeight = 290.f;
		D.WallLineChance = 0.6f; D.SegmentLength = 4; D.DoorChance = 0.3f;
		D.Floor = S(TEXT("T_ConcreteDark"), C(1, 1, 1), 250.f, 0.8f, 0.3f);
		D.Wall = S(TEXT("T_ConcreteDark"), C(0.8f, 0.8f, 0.8f), 250.f, 0.9f, 0.3f);
		D.Ceiling = D.Wall;
		D.Trim = D.Wall;
		D.Pillar = D.Wall;
		D.Fixture = EBRFixture::None;
		D.FogDensity = 0.02f; D.FogColor = C(0, 0, 0);
		D.Saturation = 0.7f; D.Contrast = 1.1f;
		D.MinEV = 3.5f; D.MaxEV = 9.f;
		D.AmbientSound = TEXT("S_Amb_Dark"); D.AmbientVolume = 0.8f;
		D.Step = EBRStep::Hard;
		D.SanityDrain = 0.3f;
		D.Entities = { E(EBREntityKind::Smiler, 1.f) };
		D.MaxEntities = 3; D.SpawnInterval = 25.f;
		D.AlmondWaterChance = 0.25f; D.BatteryChance = 0.45f; D.NoteChance = 0.15f;
		D.Exits = { X(8, EBRExitStyle::Ladder, 0.12f) };
		D.bPhenomena = true;
		D.Notes = {
			TEXT("Garde ta lampe \u00e9teinte quand tu vois deux yeux. Recule. Lentement."),
			TEXT("Les piles sont plus pr\u00e9cieuses que l'eau ici. {Battery} pour en changer."),
			TEXT("Il y a des \u00e9chelles qui descendent. En bas, \u00e7a sent la terre mouill\u00e9e.")
		};
		return D;
	}

	// ---------------------------------------------------------------- Niveau 8
	FBRLevelDef Level8()
	{
		FBRLevelDef D;
		D.Number = 8;
		D.Title = TEXT("Cave System");
		D.Nickname = TEXT("Le r\u00e9seau de grottes");
		D.Description = TEXT("Un enchev\u00eatrement de grottes humides, \u00e9clair\u00e9es \u00e7\u00e0 et l\u00e0 par de vieilles lampes de mine. ")
			TEXT("Des Deathmoths nichent au plafond, attir\u00e9es par la moindre lumi\u00e8re.");
		D.SurvivalClass = 4;
		D.ClassText = TEXT("Classe 4 : Dangereux - Entit\u00e9s nombreuses");
		D.Layout = EBRLayout::Caves;
		D.CellSize = 400.f; D.WallHeight = 450.f; D.SolidChance = 0.38f;
		D.bTrim = false; D.bLintels = false;
		D.Floor = S(TEXT("T_Rock"), C(0.7f, 0.68f, 0.65f), 300.f, 0.9f, 0.5f);
		D.Solid = S(TEXT("T_Rock"), C(1, 1, 1), 300.f, 0.9f, 0.5f);
		D.Wall = D.Solid;
		D.Ceiling = S(TEXT("T_Rock"), C(0.5f, 0.5f, 0.5f), 300.f, 0.9f, 0.5f);
		D.Fixture = EBRFixture::Bulb;
		D.LightChance = 0.12f; D.BrokenChance = 0.3f; D.FlickerChance = 0.2f;
		D.LightLumens = 1600.f; D.LightColor = C(1.f, 0.75f, 0.5f); D.LightRadius = 800.f;
		D.DarkZoneChance = 0.2f; D.ShadowChance = 0.25f;
		D.FogDensity = 0.1f; D.FogColor = C(0.08f, 0.08f, 0.08f);
		D.Saturation = 0.85f;
		D.MinEV = 2.5f; D.MaxEV = 9.f;
		D.AmbientSound = TEXT("S_Amb_Cave"); D.AmbientVolume = 0.7f;
		D.Step = EBRStep::Hard;
		D.SanityDrain = 0.18f;
		D.Entities = { E(EBREntityKind::Deathmoth, 1.f), E(EBREntityKind::Clump, 0.6f), E(EBREntityKind::Hound, 0.5f) };
		D.MaxEntities = 3; D.SpawnInterval = 40.f;
		D.AlmondWaterChance = 0.25f; D.BatteryChance = 0.35f; D.NoteChance = 0.15f;
		D.Exits = { X(9, EBRExitStyle::Ladder, 0.12f) };
		D.Props = EBRProps::Caves; D.PropDensity = 0.3f;
		D.Notes = {
			TEXT("Les papillons. \u00c9normes. Ils viennent vers la lumi\u00e8re. \u00c9teins ta lampe quand tu entends les ailes."),
			TEXT("Une \u00e9chelle remonte vers la surface. Il fait nuit l\u00e0-haut. Toujours nuit.")
		};
		return D;
	}

	// ---------------------------------------------------------------- Niveau 9
	FBRLevelDef Level9()
	{
		FBRLevelDef D;
		D.Number = 9;
		D.Title = TEXT("The Suburbs");
		D.Nickname = TEXT("La banlieue");
		D.Description = TEXT("Un quartier r\u00e9sidentiel sans fin, plong\u00e9 dans une nuit \u00e9ternelle. Les lampadaires gr\u00e9sillent, ")
			TEXT("les maisons sont vides. Ne restez pas dans la rue.");
		D.SurvivalClass = 4;
		D.ClassText = TEXT("Classe 4 : Dangereux la nuit");
		D.Layout = EBRLayout::Suburbs;
		D.CellSize = 500.f; D.Spacing = 5; D.ViewDistance = 8000.f;
		D.bCeiling = false; D.bTrim = false; D.bOutdoor = true;
		D.Floor = S(TEXT("T_Grass"), C(1, 1, 1), 300.f, 0.95f, 0.4f);
		D.Road = S(TEXT("T_Asphalt"), C(1, 1, 1), 400.f, 0.75f, 0.3f);
		D.Wall = S(TEXT("T_Siding"), C(1, 1, 1), 200.f);
		D.Fixture = EBRFixture::StreetLamp;
		D.LightChance = 0.6f; D.BrokenChance = 0.2f; D.FlickerChance = 0.1f;
		D.LightLumens = 6000.f; D.LightColor = C(1.f, 0.72f, 0.42f); D.LightRadius = 1500.f;
		D.ShadowChance = 0.3f;
		D.Sky = EBRSky::Night; D.SunLux = 0.15f; D.SunColor = C(0.6f, 0.7f, 1.f); D.SunPitch = -35.f;
		D.FogDensity = 0.05f; D.FogColor = C(0.02f, 0.03f, 0.06f);
		D.Saturation = 0.8f;
		D.MinEV = 0.f; D.MaxEV = 8.f;
		D.AmbientSound = TEXT("S_Amb_Night"); D.AmbientVolume = 0.6f;
		D.Step = EBRStep::Grass;
		D.SanityDrain = 0.15f;
		D.Entities = { E(EBREntityKind::SkinStealer, 0.8f), E(EBREntityKind::Hound, 0.8f), E(EBREntityKind::Faceling, 0.4f) };
		D.MaxEntities = 3; D.SpawnInterval = 45.f;
		D.AlmondWaterChance = 0.25f; D.BatteryChance = 0.3f; D.NoteChance = 0.15f;
		D.Exits = { X(10, EBRExitStyle::HouseDoor, 0.12f) };
		D.Props = EBRProps::Suburbs;
		D.Notes = {
			TEXT("Certaines maisons ont la porte entrouverte. Derri\u00e8re l'une d'elles, j'ai vu un champ de bl\u00e9 en plein jour."),
			TEXT("Les lampadaires ne te prot\u00e8gent pas. Ils te rendent juste plus visible.")
		};
		return D;
	}

	// ---------------------------------------------------------------- Niveau 10
	FBRLevelDef Level10()
	{
		FBRLevelDef D;
		D.Number = 10;
		D.Title = TEXT("Field of Wheat");
		D.Nickname = TEXT("Le champ de bl\u00e9");
		D.Description = TEXT("Un champ de bl\u00e9 infini sous un ciel couvert. Le vent, le bruissement des \u00e9pis, quelques ")
			TEXT("granges isol\u00e9es. Un endroit \u00e9trangement paisible.");
		D.SurvivalClass = 1;
		D.ClassText = TEXT("Classe 1 : S\u00fbr - Stable");
		D.Layout = EBRLayout::Open;
		D.CellSize = 400.f; D.ViewDistance = 9000.f;
		D.bCeiling = false; D.bTrim = false; D.bOutdoor = true;
		D.Floor = S(TEXT("T_Dirt"), C(1, 1, 1), 300.f, 0.95f, 0.3f);
		D.Fixture = EBRFixture::None;
		D.Sky = EBRSky::Overcast; D.SunLux = 4.f; D.SunColor = C(0.95f, 0.95f, 1.f); D.SunPitch = -40.f;
		D.FogDensity = 0.035f; D.FogColor = C(0.55f, 0.58f, 0.62f);
		D.Saturation = 0.9f;
		D.MinEV = 1.f; D.MaxEV = 12.f;
		D.AmbientSound = TEXT("S_Amb_Wind"); D.AmbientVolume = 0.6f;
		D.Step = EBRStep::Grass;
		D.SanityDrain = 0.02f;
		D.Entities = { E(EBREntityKind::Faceling, 1.f) };
		D.MaxEntities = 1; D.SpawnInterval = 120.f;
		D.AlmondWaterChance = 0.2f; D.BatteryChance = 0.15f; D.NoteChance = 0.1f;
		D.Exits = { X(11, EBRExitStyle::Barn, 0.1f) };
		D.Props = EBRProps::Field; D.PropDensity = 1.f;
		D.Notes = {
			TEXT("Les granges ont toujours une porte. L'une d'elles s'ouvre sur une ville."),
			TEXT("Pour la premiere fois depuis des semaines, j'ai dormi.")
		};
		return D;
	}

	// ---------------------------------------------------------------- Niveau 11
	FBRLevelDef Level11()
	{
		FBRLevelDef D;
		D.Number = 11;
		D.Title = TEXT("The Endless City");
		D.Nickname = TEXT("La ville sans fin");
		D.Description = TEXT("Une m\u00e9tropole infinie de gratte-ciel et d'avenues d\u00e9sertes, baign\u00e9e de soleil. ")
			TEXT("Des Facelings y vivent paisiblement. Ses immeubles m\u00e8nent un peu partout.");
		D.SurvivalClass = 1;
		D.ClassText = TEXT("Classe 1 : S\u00fbr - Stable - Habit\u00e9");
		D.Layout = EBRLayout::City;
		D.CellSize = 600.f; D.Spacing = 4; D.ViewDistance = 9000.f;
		D.bCeiling = false; D.bTrim = false; D.bOutdoor = true;
		D.Floor = S(TEXT("T_Asphalt"), C(1, 1, 1), 400.f, 0.75f, 0.3f);
		D.Road = S(TEXT("T_Concrete"), C(0.9f, 0.9f, 0.9f), 300.f, 0.85f, 0.3f);
		D.Solid = S(TEXT("T_Facade"), C(1, 1, 1), 800.f, 0.6f, 0.2f);
		D.Wall = D.Solid;
		D.Fixture = EBRFixture::StreetLamp;
		D.LightChance = 0.3f; D.LightLumens = 3000.f; D.LightColor = C(1.f, 0.85f, 0.6f); D.LightRadius = 1200.f;
		D.ShadowChance = 0.f;
		D.Sky = EBRSky::Day; D.SunLux = 8.f; D.SunPitch = -55.f;
		D.FogDensity = 0.025f; D.FogColor = C(0.55f, 0.6f, 0.7f);
		D.MinEV = 1.f; D.MaxEV = 12.f;
		D.AmbientSound = TEXT("S_Amb_City"); D.AmbientVolume = 0.6f;
		D.Step = EBRStep::Hard;
		D.SanityDrain = 0.f;
		D.Entities = { E(EBREntityKind::Faceling, 1.f) };
		D.MaxEntities = 4; D.SpawnInterval = 30.f;
		D.AlmondWaterChance = 0.3f; D.BatteryChance = 0.2f; D.NoteChance = 0.15f;
		D.Exits = { X(-1, EBRExitStyle::BuildingDoor, 0.15f) };
		D.Props = EBRProps::City;
		D.Notes = {
			TEXT("Les portes des immeubles m\u00e8nent n'importe o\u00f9. Vraiment n'importe o\u00f9."),
			TEXT("Les Facelings d'ici sont gentils. Ils ne parlent pas, mais ils ne font pas de mal.")
		};
		return D;
	}

	// ---------------------------------------------------------------- Niveau 37
	FBRLevelDef Level37()
	{
		FBRLevelDef D;
		D.Number = 37;
		D.Title = TEXT("Sublimity");
		D.Nickname = TEXT("Les Poolrooms");
		D.Description = TEXT("Un d\u00e9dale de salles carrel\u00e9es de blanc, inond\u00e9es d'une eau ti\u00e8de et claire. ")
			TEXT("La lumi\u00e8re est douce, l'\u00e9cho infini. Un calme presque r\u00e9confortant... presque.");
		D.SurvivalClass = 1;
		D.ClassText = TEXT("Classe 1 : S\u00fbr - Stable - Aucune entit\u00e9");
		D.Layout = EBRLayout::Rooms;
		D.CellSize = 500.f; D.WallHeight = 450.f; D.WallThickness = 40.f;
		D.WallLineChance = 0.35f; D.SegmentLength = 3; D.DoorChance = 0.5f; D.DoorWidth = 220.f;
		D.PillarChance = 0.25f; D.PillarSize = 70.f; D.OpenZoneChance = 0.35f;
		D.bTrim = false; D.bLintels = true;
		// D'apres la capture de reference : petit carrelage blanc casse tres brillant, plafonniers ovales,
		// grandes verrieres, eau turquoise-verte limpide qui ondule
		D.Floor = S(TEXT("T_PoolTile"), C(0.97f, 0.97f, 0.94f), 100.f, 0.12f, 0.1f);
		D.Wall = S(TEXT("T_PoolTile"), C(0.95f, 0.95f, 0.91f), 100.f, 0.1f, 0.12f);
		D.Ceiling = S(TEXT("T_PoolTile"), C(0.96f, 0.96f, 0.93f), 100.f, 0.14f, 0.08f);
		D.Pillar = D.Wall;
		D.Fixture = EBRFixture::SkyPanel;
		D.LightChance = 0.6f; D.BrokenChance = 0.f; D.FlickerChance = 0.f;
		D.LightLumens = 4200.f; D.LightColor = C(1.f, 0.99f, 0.96f); D.LightRadius = 1000.f; D.ShadowChance = 0.15f;
		D.SkylightChance = 0.45f;
		D.bWater = true; D.WaterHeight = 45.f;
		D.Water = S(TEXT("T_WaterNormal"), C(0.22f, 0.68f, 0.64f), 300.f, 0.02f, 0.f);
		D.WaterAbsorption = 1.1f; D.WaterScattering = 0.2f;
		D.WaterWaves = 1.6f; D.WaterChop = 1.3f;
		D.PoolChance = 0.32f; D.PoolDepth = 260.f;
		D.FogDensity = 0.012f; D.FogColor = C(0.66f, 0.73f, 0.71f);
		D.SceneTint = C(0.97f, 1.f, 0.98f); D.Saturation = 0.95f; D.Bloom = 1.4f;
		D.MinEV = 3.f; D.MaxEV = 10.f;
		D.AmbientSound = TEXT("S_Amb_Pool"); D.AmbientVolume = 0.7f;
		D.Step = EBRStep::Water;
		D.SanityDrain = -0.03f;
		D.AlmondWaterChance = 0.2f; D.BatteryChance = 0.1f; D.NoteChance = 0.15f;
		D.Exits = { X(0, EBRExitStyle::NoclipFloor, 0.08f), X(4, EBRExitStyle::Ladder, 0.06f) };
		D.Notes = {
			TEXT("L'eau est ti\u00e8de. Elle n'a pas de fond, a certains endroits. N'y plonge pas."),
			TEXT("C'est beau ici. Trop beau. Je crois que je n'ai plus envie de partir. C'est \u00e7a, le pi\u00e8ge.")
		};
		// Reflets d'eau animes sur le carrelage
		D.Floor.Caustics = 1.f; D.Wall.Caustics = 1.f; D.Pillar.Caustics = 1.f; D.Ceiling.Caustics = 0.6f;
		return D;
	}

	TArray<FBRLevelDef> BuildAll()
	{
		TArray<FBRLevelDef> L;
		L.Add(Level0());
		L.Add(Level1());
		L.Add(Level2());
		L.Add(Level3());
		L.Add(Level4());
		L.Add(Level5());
		L.Add(Level6());
		L.Add(Level8());
		L.Add(Level9());
		L.Add(Level10());
		L.Add(Level11());
		L.Add(Level37());
		return L;
	}
}

namespace BRLevels
{
	const TArray<FBRLevelDef>& All()
	{
		static const TArray<FBRLevelDef> Levels = BuildAll();
		return Levels;
	}

	int32 IndexOf(int32 Number)
	{
		const TArray<FBRLevelDef>& L = All();
		for (int32 i = 0; i < L.Num(); ++i)
		{
			if (L[i].Number == Number)
			{
				return i;
			}
		}
		return 0;
	}

	bool Exists(int32 Number)
	{
		for (const FBRLevelDef& D : All())
		{
			if (D.Number == Number)
			{
				return true;
			}
		}
		return false;
	}

	const FBRLevelDef& Get(int32 Number)
	{
		return All()[IndexOf(Number)];
	}

	const TArray<FString>& CommonNotes()
	{
		static const TArray<FString> Notes = {
			TEXT("R\u00c8GLE N\u00b01 : ne cours pas si tu n'y es pas oblig\u00e9. Le bruit attire les choses."),
			TEXT("R\u00c8GLE N\u00b02 : bois de l'eau d'amande quand ta t\u00eate commence \u00e0 tourner. La folie tue aussi s\u00fbrement que les entites."),
			TEXT("R\u00c8GLE N\u00b03 : une lampe allum\u00e9e se voit de loin. Parfois, il vaut mieux rester dans le noir."),
			TEXT("Ils l'appellent le \"Front\" - la r\u00e9alit\u00e9 d'o\u00f9 nous venons. Je ne suis plus s\u00fbr qu'elle ait exist\u00e9."),
			TEXT("Note pour moi-m\u00eame : {Inventory} pour ouvrir le journal. Ne pas oublier ce que j'ai vu.")
		};
		return Notes;
	}
}
