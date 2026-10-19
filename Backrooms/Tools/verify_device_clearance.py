"""
v4.15 : banc hors moteur des places des mecanismes de mission : le point d'interaction d'un mecanisme (centre de sa boite
visee, ABRMissionDevice::GetInteractPoint) n'est dans aucune collision que le jeu pose autour de lui, et une place debout
a portee de l'hote (340 cm des yeux) le voit (ligne de vue de ABRWorld::ServerMissionAct, qui n'ignore que le joueur et le
mecanisme).

Quatre cas, chacun avec la regle d'avant (v4.14) et celle du jeu (v4.15) :
  A. Level 6 : poste de confinement et sa cage sur la meme face (groupes 10 a 13). v4.14 : poste a la place 0 (lateral 0),
     cage a la place 1 (+58 cm) : le point du poste est DANS le volume bloquant de la cage -> ligne de vue toujours coupee
     (actions refusees). v4.15 (BRMech::Clearance::PickSlot, miroir ici) : la cage d'abord, au bout de sa face ou elle
     tient (correctifs : cage a +-58, poste a -+58, comme diag_v413_4), le poste a cote, hors de son volume. Jugee comme les
     tests du jeu (v4.15 correctifs) : place de BRTestV412::FindMissionStand (cercles et portees lus dans le code), marche
     vers elle (rayons d'arrivee lus dans le code), regle de l'hote (ligne de vue) et visee reelle du client
     (ABRCharacter::UpdateFocus : balayage de 7 cm qui prend la cage si elle est frolee) depuis CHAQUE point d'arrivee
     possible, puis la marche de reprise ; murs voisins presents ou non aux deux bouts de la face, fond de la cellule ou
     non. La premiere disposition v4.15 (cage au centre, poste a +-116 : 29 cm du mur voisin) est montree pour information.
  B. Level 8 : rochers des parois et des cellules (ABRChunk : boite de collision = bornes de SM_Rock a l'echelle du rocher).
     Grille des grottes rejouee (BRHash, ABRWorld::IsSolid), chaque face de paroi d'une cellule libre comme place d'un treuil
     ou d'une plaque, a chaque decalage lateral du placement. v4.14 : tous les rochers ; v4.15 : pas de rocher dont la boite
     entre dans la zone libre du mecanisme (BRMech::Clearance::KeepOut, miroir ici).
  C. Level 9 : maisons (boite de collision de SM_House). v4.14 : bornes completes (jusqu'a 5,2 m, perron compris) ;
     v4.15 : corps jusqu'a GHouseBodyFront et perron bas (BRChunk.cpp). Faces des lots de maisons vues de la route.
  D. Level 8 : sorties de la mission (echelle contre une paroi, ABRWorld::PlaceMission : FaceSpot au decalage 0). v4.14 : la
     capsule qui monte a l'echelle (ABRExit::GetClimbAnchor) et son approche sont dans un rocher ; v4.15 : pas de rocher dans
     la zone libre de la sortie (BRMech::Clearance::ExitKeepOut, miroir ici).
  E. v4.15 (correctifs 3) : Niveau 10 : groupe du moulin (tableau des granges, orientation, frein) sur les poteaux d'une
     cellule de champ (ABRWorld::PlaceMission, RoomSlots/PostSpot, miroir ici). Regle d'avant : un lacet tire par poteau ;
     depuis la place devant le moulin ou le frein, la visee du client (ABRCharacter::UpdateFocus : balayage de 7 cm sur les
     boites visees) prend le tableau (graine 1697132605 : journal du jeu "PAS VISE par le client"). Regle du jeu : poteaux
     d'une cellule en rang (meme lacet) et couloirs de visee degages (BRMech::Clearance::CutsFocusLane dans PickSlot,
     miroir ici) : aucune visee prise ; le couloir detecte chacune des combinaisons fautives de la regle d'avant.
  F. v4.15 (correctifs 4) : Level 8 : echelles tirees par les chunks (partie classique ; ABRChunk::PlanExits, branche murale,
     miroir ici). Regle d'avant : rochers des parois et des cellules sans zone libre autour d'elles : le point vise par le
     depart de groupe (BRGather::ViewPoint, 1 m devant le pied, 90 cm), la colonne du grimpeur (ABRExit::GetClimbAnchor) ou
     l'approche sont dans la boite d'un rocher (graine 233637237 : BRExit_42, journal final2/v413). Regle du jeu : pas de
     rocher dans la zone libre (BRMech::Clearance::ExitKeepOut) des sorties tirees par les 3 x 3 chunks autour
     (ABRWorld::IsChunkExitKeepOut).
  G. v4.16 : Niveau 37 : les deux vannes des bassins (ABRWorld::PlaceMission, module des Poolrooms : vanne B a
     BRMech::Pool::WheelLateralB de la vanne A, meme face, meme lacet ; boites visees de BRMech::Clearance::Of, R_Sluice ;
     volume des bassins). Regle d'avant (v4.12 a v4.16) : B a 58 cm, boites visees chevauchees de 6 cm ; graine 3053711763
     (chapitre, phase 1 : "mecanisme 2 (Vanne du bassin A) atteint mais PAS VISE", vise : vanne B, apres une marche de reprise
     de 0 m) : place a 45 degres de l'avant de A du cote de B (cercle de 160 cm), arrivee de la marche plus pres de B, la
     sphere de la visee du client touche la boite de B avant celle de A. Regle du jeu : miroir de
     BRMech::Clearance::FanSteals (places de biais des tests jusqu'a 45 degres, sur chaque cercle, et toutes leurs arrivees ;
     sols de la vanne B et du joueur decales : canal, marche, trottoir) : aucune visee prise ; et l'avant du bassin B (volants
     et graduations, regard de la vidange, regles) sans recouvrement. Les places plus de biais (67,5 degres) sont montrees
     pour information, ainsi que les voisins a 58 cm du placement des autres niveaux (inchange).
  H. v4.16 (visee) : choix de la visee du client entre voisins (ABRCharacter::PickAimedDevice, miroir de
     BRMech::Clearance::PickAim). Regle d'avant (v4.12 a c1c2bde) : le premier objet touche par la sphere ; juge sur le defaut
     d'origine (vannes du Niveau 37 a 58 cm, memes places, arrivees et sols que le cas G) : visee prise, ECHOUE. Regle du jeu :
     le balayage traverse les boites visees jusqu'au premier volume, le point vise le plus pres de l'axe du regard l'emporte, un
     voisin seulement si l'hote l'accepte : aucune visee prise, a 58 cm comme a 98 cm, et jamais un mecanisme cache (mur,
     volume, point vise derriere une dalle, hors de portee de l'hote : scenes du banc Tools/Mechanisms, section K, qui juge
     aussi les voisins de chaque niveau avec le code du jeu).
En B et C, la place debout est celle des tests du jeu (BRTestV412::FindMissionStand : cercles et directions lus dans
BRAutoTestV412.cpp, capsule de 36 cm, yeux a leur hauteur, portee de la visee du client puis regle de l'hote ; v4.15
correctifs 4 : puis la visee du client prend le mecanisme, BRTestV412::FocusVerdict : balayage, puis tolerance d'UpdateFocus si
le decor l'arrete).

Les constantes sont lues dans les sources (BRMechLogic.h/.cpp, BRMissionWorld.cpp, BRChunk.cpp, BRLevels.cpp, BRWorld.h,
BRCharacter.cpp, BRInteractables.cpp, BRAutoTestV412.cpp, BRAutoTestWalk.h) et les bornes des modeles dans
RawAssets/Meshes/SM_Rock.fbx et SM_House.fbx (comme l'importeur : X = X, Y = Z du fichier, Z = Y du fichier). Le banc verifie
que l'ancienne regle ECHOUE (le defaut est reproduit), que la regle du jeu PASSE et qu'elle est bien dans les sources (sinon
ECHEC) ; il ne remplace pas la verification dans le decor construit (-BRAutoTestV411 : "v4.15 mission N : places des
mecanismes" ; -BRAutoTestV413 : postes et cages du Level 6).

Usage : python Tools/verify_device_clearance.py [--seeds 120] [--rules both|old|new]
Code de sortie : 0 si la regle v4.15 passe partout ET la regle v4.14 echoue la ou les journaux du jeu l'ont vue echouer ET la regle v4.15
est dans les sources (cas A, D, E et F ; v4.16 : cas G, regle d'avant = vanne B a 58 cm ; cas H, regle d'avant = visee au premier
contact). Sortie : Docs/v415/correctifs_places.txt.
"""
import argparse
import math
import os
import re
import struct
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
SRC = os.path.join(ROOT, "Source", "Backrooms")
sys.path.insert(0, HERE)
import verify_pitfalls as vp  # noqa: E402

M32 = 0xFFFFFFFF


def read(rel):
    with open(os.path.join(SRC, rel), encoding="utf-8") as f:
        return f.read()


def symbol(K, text):
    """Valeur d'un litteral (110.f) ou d'une constante deja lue (BRMech::Clearance::StandRange)"""
    m = re.fullmatch(r"BRMech::Clearance::(\w+)", text)
    if m:
        key = "Clearance." + m.group(1)
        if key not in K:
            raise SystemExit("constante %s non lue (BRMechLogic.h)" % text)
        return K[key]
    return float(text.rstrip("f"))


def num(pattern, text, what):
    m = re.search(pattern, text, re.S)
    if not m:
        raise SystemExit("constante introuvable (%s) : %s" % (what, pattern))
    return [float(g) for g in m.groups()]


# ------------------------------------------------------------------------------------------------- constantes du jeu
def read_constants():
    mech_h = read("Public/BRMechLogic.h")
    mech = read("Private/BRMechLogic.cpp")
    world = read("Private/BRMissionWorld.cpp")
    chunk = read("Private/BRChunk.cpp")
    world_h = read("Public/BRWorld.h")
    char = read("Private/BRCharacter.cpp")
    K = {}
    ns = lambda name: mech_h[mech_h.index("namespace %s" % name):]
    for name, keys in (("LidarCage", ("Gap", "Depth", "HalfWidth", "Height")),
                       ("Clearance", ("PlayerRadius", "Margin", "StandDepth", "StepHeight", "FocusSweep", "StandRange", "StandEye", "AimProbe")),
                       ("Device", ("WorkZ",))):
        body = ns(name)
        for k in keys:
            K[name + "." + k] = num(r"constexpr float %s = ([\d.]+)f;" % k, body, name + "." + k)[0]
    K["Clearance.StandGap"] = K["Clearance.PlayerRadius"] + K["Clearance.Margin"]
    Z = K["Device.WorkZ"]
    # Boites visees (BRMech::Clearance::Of) : poste, treuil, plaque murale, entailles (defaut), boitier de rue, marqueur
    K["post"] = num(r"Role == R_Beacon \|\| Role == R_LidarPost\)\s*\{\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);", mech, "poste")
    K["winch"] = num(r"F\.AimExtent = V3\(14\.f, 16\.f, 60\.f\);\s*\}\s*else\s*\{\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);", mech, "treuil")
    d = num(r"F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*switch \(Kind\)", mech, "boite par defaut")
    K["default"] = [d[0], d[1], Z, d[2], d[3], d[4]]
    p = num(r"else\s*\{\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z \+ ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*\}\s*break;\s*\}\s*case EKind::Observe", mech, "plaque")
    K["plate"] = [p[0], p[1], Z + p[2], p[3], p[4], p[5]]
    m = num(r"Role == R_HouseMarker\)\s*\{\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z \+ ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);", mech, "marqueur")
    K["marker"] = [m[0], m[1], Z + m[2], m[3], m[4], m[5]]
    sb = num(r"const float R = Role == R_MillDial \? [\d.]+f : ([\d.]+)f;\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z \+ ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, R \+ ([\d.]+)f, R \+ ([\d.]+)f\);", mech, "boitier")
    K["streetbox"] = [sb[1], sb[2], Z + sb[3], sb[4], sb[0] + sb[5], sb[0] + sb[6]]
    # Placement (BRMissionWorld.cpp) : ecart des places d'une face, nombre par face
    K["Spacing"] = num(r"const float Spacing = ([\d.]+)f;", world, "Spacing")[0]
    K["PerFaceMargin"] = num(r"FloorToInt\(\(S - ([\d.]+)f\) / Spacing\) \+ 1, 1, (\d+)\)", world, "PerFace")
    K["Reach"] = num(r"MissionReachServer = ([\d.]+)f;", world_h, "MissionReachServer")[0]
    K["Capsule"] = num(r"InitCapsuleSize\(([\d.]+)f, ([\d.]+)f\)", char, "capsule")
    K["EyeZ"] = num(r"constexpr float StandEyeZ = ([\d.]+)f;", char, "StandEyeZ")[0]
    # Rochers (ABRChunk::StepPlan, etape 5 ; BuildCellProps, grottes). v4.15 (correctifs 4) : tirages dans BRMech::Cave::WallRock et
    # CellRock (BRMechLogic.cpp), appeles par ABRChunk
    wr = num(r"Grid::Rand\(X \* 4 \+ K, Y, 931, Seed\) < ([\d.]+)f\)\)\s*\{.*?const float Sc = ([\d.]+)f \+ ([\d.]+)f \* Grid::Rand\(X, Y \* 4 \+ K, 932, Seed\);"
             r".*?\(Grid::Rand\(X, Y, 933 \+ K, Seed\) - 0\.5f\) \* S \* ([\d.]+)f\);\s*Out\.X = FaceX \+ \(-DY\) \* Along - DX \* ([\d.]+);.*?Out\.ScaleZ = Sc \* ([\d.]+)f;",
             mech, "rocher de paroi")
    K["wall_rock"] = dict(chance=wr[0], sc0=wr[1], sc1=wr[2], spread=wr[3], inset=wr[4], zs=wr[5])
    cr = num(r"Grid::Rand\(X, Y, 930, Seed\) < PropDensity \* ([\d.]+)f\)\)\s*\{.*?const float Sc = ([\d.]+)f \+ Grid::Rand\(X, Y, 931, Seed\) \* ([\d.]+)f;"
             r".*?\(Grid::Rand\(X, Y, 932, Seed\) - 0\.5f\) \* S \* ([\d.]+)f\);.*?Out\.Z = (-?[\d.]+);", mech, "rocher de cellule")
    K["cell_rock"] = dict(density=cr[0], sc0=cr[1], sc1=cr[2], spread=cr[3], z=cr[4])
    K["rock_keepout_in_game"] = ("BRMech::Cave::WallRock(X, Y, k, Seed, S, Rock)" in chunk and "BRMech::Cave::CellRock(X, Y, Seed, S, D.PropDensity, Rock)" in chunk
                                 and "RockInKeepOut(this, InWorld, Rock)" in chunk and "RockInKeepOut(this, W, Rock)" in chunk and "W->IsMissionKeepOut(R)" in chunk)
    # v4.15 (correctifs 4) : sorties tirees par les chunks : regle des rochers et tirage partage
    K["src_F"] = {
        "ABRChunk : rochers hors de la zone libre des sorties tirees par les chunks (ABRWorld::IsChunkExitKeepOut)": "W->IsMissionKeepOut(R) || W->IsChunkExitKeepOut(R)" in chunk,
        "ABRWorld::IsChunkExitKeepOut : sorties murales des 3 x 3 chunks, BRMech::Cave::InExitKeepOut": "PickChunkWallFace(Chunk, I, nullptr, Face)" in world
        and "BRMech::Cave::InExitKeepOut(Rock, Frames->GetData(), Frames->Num())" in world,
        "ABRChunk::PlanExits : meme tirage (ABRWorld::PickChunkWallFace)": "W->PickChunkWallFace(Coord, i, &HidingCells, Face)" in chunk and "W->IsChunkExitDrawn(Coord, i)" in chunk,
        "BRMech::Cave::InExitKeepOut : BRMech::Clearance::ExitKeepOut": "Clearance::Overlap(Clearance::ExitKeepOut(Exits[I]), Rock)" in mech,
    }
    # Maison (BRChunk.cpp)
    hb = re.search(r"constexpr float GHouseBodyFront = ([\d.]+)f;\s*constexpr float GHousePorchHalfY = ([\d.]+)f;\s*constexpr float GHousePorchTop = ([\d.]+)f;", chunk)
    K["house_body"] = [float(g) for g in hb.groups()] if hb else None
    # Niveaux (BRLevels.cpp)
    levels = open(vp.LEVELS_CPP, encoding="utf-8").read()
    for n in (6, 8, 9, 10, 37):
        body = levels[levels.index("FBRLevelDef Level%d()" % n):]
        body = body[:body.index("return D;")]
        vals = dict((k, float(v)) for k, v in re.findall(r"D\.(\w+)\s*=\s*(-?[\d.]+)f?\s*;", body))
        K["L%d" % n] = vals
    K["WallThicknessDefault"] = num(r"float WallThickness = ([\d.]+)f;", read("Public/BRTypes.h"), "WallThickness")[0]
    # v4.15 (correctifs 4) : echelle du Level 8 tiree par les chunks (BRLevels.cpp) et point vise par le depart (BRGatherLogic)
    l8 = levels[levels.index("FBRLevelDef Level8()"):]
    K["L8_ladder_chance"] = num(r"D\.Exits = \{ X\(9, EBRExitStyle::Ladder, ([\d.]+)f\) \};", l8[:l8.index("return D;")], "echelle du Level 8")[0]
    gather_h, gather = read("Public/BRGatherLogic.h"), read("Private/BRGatherLogic.cpp")
    K["view_height"] = num(r"constexpr float ViewHeight = ([\d.]+)f;", gather_h, "ViewHeight")[0]
    K["gather_ahead"] = num(r"case EStyle::Ladder: return GatherImpl::Ahead\(Shape, ([\d.]+)f\);", gather, "GatherPoint (echelle)")[0]
    K["ChunkCellsDefault"] = num(r"int32 ChunkCells = (\d+);", read("Public/BRTypes.h"), "ChunkCells")[0]
    # Places debout des tests du jeu (BRTestV412::CheckMissionStand / FindMissionStand) et marches vers elles
    walk = read("Private/BRAutoTestV412.cpp")
    walk_h = read("Private/BRAutoTestWalk.h")
    rings = re.search(r"for \(const float Dist : \{ ([^}]*) \}\)", walk)
    if not rings:
        raise SystemExit("cercles de BRTestV412::FindMissionStand introuvables (BRAutoTestV412.cpp)")
    # v4.15 (correctifs 3) : le premier cercle est BRMech::Clearance::StandRange (place ordinaire du couloir de visee)
    K["rings"] = [symbol(K, v.strip()) for v in rings.group(1).split(",")]
    t = num(r"for \(int32 Turn = 0; Turn < (\d+); \+\+Turn\)\s*\{\s*const FVector P = Aim \+ Fwd\.RotateAngleAxis\(Turn \* ([\d.]+)f", walk, "directions")
    K["turns"], K["turn_deg"] = int(t[0]), t[1]
    K["stand_cap"] = num(r"FCollisionShape::MakeCapsule\(([\d.]+)f, Half - ([\d.]+)f\)", walk, "capsule de la place")
    K["stand_lift"] = num(r"const FVector Stand\(P\.X, P\.Y, FloorZ \+ Half \+ ([\d.]+)f\);", walk, "capsule au-dessus du sol")[0]
    K["ClientFocusReach"] = num(r"constexpr float ClientFocusReach = ([\d.]+)f;", walk_h, "ClientFocusReach")[0]
    K["front_ahead"] = num(r"CheckMissionStand\(W, C, Dev, Aim \+ Fwd \* ([\d.]+)f, true, Goal\)", walk, "place devant le mecanisme")[0]
    K["walk_radius"] = num(r"Front == EStand::Ok \? ([\d.]+)f : ([\d.]+)f, true\)", walk, "rayons d'arrivee de la marche")
    K["retry"] = num(r"CheckMissionStand\(W, C, Dev, Aim \+ Fwd \* ([\d.]+)f, true, Closer\);.*?BeginWalk\(W, C, FVector\(Goal\.X, Goal\.Y, C->GetActorLocation\(\)\.Z\), "
                     r"([\d.]+)f, true\);", walk, "marche de reprise")
    # Visee du client (ABRCharacter::UpdateFocus) : balayage precis, portee, tolerance
    # v4.15 (correctifs 3) : rayon du balayage = BRMech::Clearance::FocusSweep (le couloir de visee du placement)
    fs = re.search(r"ECC_Visibility, FCollisionShape::MakeSphere\(([\w:.]+)\), Q\)", char)
    if not fs:
        raise SystemExit("balayage de la visee introuvable (BRCharacter.cpp)")
    K["focus_sweep"] = symbol(K, fs.group(1))
    K["focus_sweep_shared"] = fs.group(1) == "BRMech::Clearance::FocusSweep"
    K["InteractReach"] = num(r"constexpr float InteractReach = ([\d.]+)f;", char, "InteractReach")[0]
    K["tolerance"] = num(r"MakeSphere\(InteractReach \* ([\d.]+)f\)", char, "tolerance de la visee")[0]
    # v4.15 (correctifs 4, relecture) : alignement minimal de la tolerance (cosinus) ; le miroir des tests du jeu (BRAutoTestWalk.h)
    # doit avoir les memes valeurs
    K["focus_dot"] = num(r"MakeSphere\(InteractReach \* [\d.]+f\), Q\)\)\s*\{\s*AActor\* Best = nullptr;\s*float BestDot = ([\d.]+)f;", char,
                         "alignement de la tolerance")[0]
    K["tolerance_shared"] = ("constexpr float ClientFocusTolerance = %gf;" % K["tolerance"] in walk_h
                             and "constexpr float ClientFocusDot = %gf;" % K["focus_dot"] in walk_h)
    # Cage : boite visee (BRMech::Clearance::Of, R_LidarCage ; bloque la visee du client comme son volume)
    ca = num(r"case R_LidarCage:\s*F\.Aim = V3\(LidarCage::Gap \+ LidarCage::Depth \+ ([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*"
             r"F\.AimExtent = V3\(([\d.]+)f, LidarCage::HalfWidth, ([\d.]+)f\);", mech, "boite visee de la cage")
    K["cage_aim"] = [K["LidarCage.Gap"] + K["LidarCage.Depth"] + ca[0], ca[1], ca[2], ca[3], K["LidarCage.HalfWidth"], ca[4]]
    # Sorties : zone libre (BRMech::Clearance::ExitKeepOut) et capsule de l'echelle (ABRExit::GetClimbAnchor)
    K["exit_keepout"] = num(r"FRect ExitKeepOut\(const FFrame& F\)\s*\{\s*const float HY = LidarDoor::Width \* 0\.5f \+ StandGap;\s*"
                            r"return RectOf\(F, Box\(-([\d.]+)f, -HY, 0\.f, StandDepth \+ ([\d.]+)f, HY, 0\.f\)\);", mech, "ExitKeepOut")
    K["LidarDoor.Width"] = num(r"constexpr float Width = ([\d.]+)f;", ns("LidarDoor"), "LidarDoor.Width")[0]
    K["climb_anchor"] = num(r"FVector ABRExit::GetClimbAnchor\(\) const\s*\{[^}]*?GetActorForwardVector\(\) \* ([\d.]+)f;", read("Private/BRInteractables.cpp"),
                            "GetClimbAnchor")[0]
    # Regle du jeu dans les sources : le banc echoue si elle n'y est plus (le miroir passerait seul)
    place = world[world.index("bool ABRWorld::PlaceMission("):world.index("bool ABRWorld::PlaceMissionModule(")]
    module = world[world.index("bool ABRWorld::PlaceMissionModule("):world.index("void ABRWorld::SpawnMissionActors()")]
    pick = mech[mech.index("int PickSlot("):]
    K["src_A"] = {
        "ABRWorld::PlaceMission : volumes d'abord (StableSort), puis BRMech::Clearance::PickSlot": "Ordered.StableSort(" in place
        and "BRMech::Clearance::PickSlot(Print, Cand.GetData()" in place,
        "ABRWorld::PlaceMissionModule : PickSlot, curseur hors des places prises": "BRMech::Clearance::PickSlot(Print, Cand.GetData()" in module
        and "Taken[Next]" in module,
        "BRMech::Clearance::PickSlot : volume mural au bout de sa face": "FaceCenter(" in pick and "AbsF(T.Lateral) <= AbsF(Slots[Best].Lateral)" in pick,
        # v4.15 (correctifs 4, relecture) : la place des tests du jeu se juge avec la tolerance de la visee, comme ce miroir
        "BRTestV412::CheckMissionStand : visee du client par FocusVerdict, tolerance d'ABRCharacter::UpdateFocus": K["tolerance_shared"]
        and "const FFocus Focus = FocusVerdict(W, C, Eye, Dev);" in walk,
    }
    K["src_D"] = {"ABRWorld::SetupMission : zone libre des sorties de la mission": "MissionKeepOuts.Add(BRMech::Clearance::ExitKeepOut(F))" in world}
    # v4.15 (correctifs 3) : cas E : poteaux (PostSpot, RoomSlots), boites du moulin et du frein, regle des couloirs
    K["post_back"] = num(r"Spot\.Pos = CellCenter\(C, 0\.f\) \+ Side \* Lateral - Fwd \* \(S \* ([\d.]+)f\);", world, "recul des poteaux")[0]
    K["post_lat"] = num(r"Slots\.Add\(PostSpot\(C, Lateral\(K\) \* ([\d.]+)f, ", world, "ecart des poteaux")[0]
    md = num(r"const float R = Role == R_MillDial \? ([\d.]+)f : [\d.]+f;\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z \+ ([\d.]+)f\);\s*"
             r"F\.AimExtent = V3\(([\d.]+)f, R \+ ([\d.]+)f, R \+ ([\d.]+)f\);", mech, "moulin")
    K["mill"] = [md[1], md[2], Z + md[3], md[4], md[0] + md[5], md[0] + md[6]]
    bd = num(r"else\s*\{\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z \+ ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*\}\s*break;\s*case EKind::Gate:",
             mech, "bouton (frein)")
    K["button"] = [bd[0], bd[1], Z + bd[2], bd[3], bd[4], bd[5]]
    lane = mech[mech.index("bool CutsFocusLane("):mech.index("bool FocusClash(")]
    K["src_E"] = {
        "ABRWorld::PlaceMission : poteaux d'une cellule en rang (meme lacet)": "bPlacement2 ? 77u + static_cast<uint32>(K) : 77u" in place,
        "ABRWorld::PlaceMission : PickSlot avec les couloirs de visee": "FaceHalf, !bPlacement2)" in place,
        "BRMech::Clearance::PickSlot : couloirs (FocusClash), puis le choix d'avant faute de mieux": "bLanes && FocusClash(Me, Placed[J])" in pick and "bLanes = false;" in pick,
        "BRMech::Clearance::CutsFocusLane : place a StandRange, yeux a StandEye, FocusSweep + Margin": "StandRange" in lane and "StandEye" in lane
        and "FocusSweep + Margin" in lane,
        "ABRCharacter::UpdateFocus : balayage de BRMech::Clearance::FocusSweep": K["focus_sweep_shared"],
    }
    # v4.16 : cas G : vannes des Poolrooms (BRMech::Pool, boite visee R_Sluice, volume des bassins), places de biais
    # (BRMech::Clearance::Fan*) et pieces de l'avant des bassins (ABRMissionDevice::Init et BuildPoolTanks)
    pool = ns("Pool")
    for k in ("TankDepth", "WheelLateralB", "Wall", "DrainLateral", "GaugeLateralA", "GaugeLateralB"):
        K["Pool." + k] = num(r"constexpr float %s = (-?[\d.]+)f;" % k, pool, "Pool." + k)[0]
    K["Pool.AY"] = num(r"constexpr float AY0 = (-?[\d.]+)f, AY1 = (-?[\d.]+)f;", pool, "Pool.AY0/AY1")
    K["Pool.BY"] = num(r"constexpr float BY0 = (-?[\d.]+)f, BY1 = (-?[\d.]+)f;", pool, "Pool.BY0/BY1")
    sl = num(r"const float R = Role == R_Sluice \? ([\d.]+)f : [\d.]+f;\s*F\.AimExtent = V3\(([\d.]+)f, R \+ ([\d.]+)f, R \+ ([\d.]+)f\);", mech, "vanne (R_Sluice)")
    K["sluice"] = [K["default"][0], K["default"][1], K["default"][2], sl[1], sl[0] + sl[2], sl[0] + sl[3]]
    K["tank_top"] = num(r"F\.Solid = Box\(-Pool::TankDepth, Pool::AY0 - Pool::Wall, 0\.f, 0\.f, Pool::BY1 \+ Pool::Wall, ([\d.]+)f\);", mech, "volume des bassins")[0]
    clh = ns("Clearance")
    fr_ = re.search(r"constexpr float FanRings\[NumFanRings\] = \{ ([^}]*) \};", clh)
    if not fr_:
        raise SystemExit("BRMech::Clearance::FanRings introuvable (BRMechLogic.h)")
    K["fan_rings"] = [K["Clearance." + v.strip()] if re.fullmatch(r"[A-Za-z]\w*", v.strip()) else symbol(K, v.strip()) for v in fr_.group(1).split(",")]
    for k in ("FanStep", "FanArrival", "FanArrivalFront", "FanReach", "FanGrid"):
        K["Clearance." + k] = num(r"constexpr float %s = ([\d.]+)f;" % k, clh, "Clearance." + k)[0]
    K["Clearance.FanTurns"] = int(num(r"constexpr int FanTurns = (\d+);", clh, "Clearance.FanTurns")[0])
    dev = read("Private/BRMissionDevice.cpp")
    K["wheel_r"] = num(r"const float R = Device\.Role == BRM::R_Sluice \? ([\d.]+)f : [\d.]+f;", dev, "rayon du volant")[0]
    K["wheel_ring_half"] = num(r"AddBoxPart\(FVector\(0\.f, FMath::Cos\(Ang\) \* R, FMath::Sin\(Ang\) \* R\), FVector\(([\d.]+)f, R \* [\d.]+f, [\d.]+f\)", dev,
                               "anneau du volant")[0] * 0.5
    gl = num(r"AddLabel\(FString::FromInt\(I\), FVector\([\d.]+f, FMath::Sin\(Ang\) \* \(R \+ ([\d.]+)f\), Z \+ FMath::Cos\(Ang\) \* \(R \+ [\d.]+f\)\), ([\d.]+)f, LabelColor\);",
             dev, "graduations du volant")
    K["wheel_label_r"], K["wheel_label_size"] = gl
    K["window_d"] = num(r"AddPart\(A->Cylinder\(\), Wd, FRotator\(90\.f, 0\.f, 0\.f\), FVector\(([\d.]+)f, ", dev, "regards")[0]
    K["gauge_w"] = num(r"AddBoxPart\(FVector\(1\.5f, GY, Zl\(\(S0 \+ SN\) \* 0\.5f\)\), FVector\(2\.f, ([\d.]+)f, ", dev, "regle graduee")[0]
    gd = num(r"AddLabel\(FString::FromInt\(M\), FVector\(3\.2f, GY \+ ([\d.]+)f, Zl\(Z\)\), ([\d.]+)f, ", dev, "chiffres de la regle")
    K["gauge_digit"] = gd[0] + gd[1] * 0.3
    K["sluice_positions"] = int(num(r"Add\(P, EKind::Switch, R_Sluice, 0, (\d+), ", read("Private/BRMissionLogic.cpp"), "crans des vannes")[0])
    bench = open(os.path.join(ROOT, "Tools", "Mechanisms", "test_mechanisms.cpp"), encoding="utf-8").read()
    walk_r = K["walk_radius"]
    K["src_G"] = {
        "ABRWorld::PlaceMission : vanne B a BRMech::Pool::WheelLateralB de la vanne A": "B.Pos = A.Pos + Left * BRMech::Pool::WheelLateralB + Fwd * BRMech::Pool::TankDepth;" in world,
        "ABRMissionDevice::BuildPoolTanks : vidange a Pool::DrainLateral, regles a Pool::GaugeLateralA/B": "const float DY = P::DrainLateral;" in dev
        and "const float GY = T == 0 ? P::GaugeLateralA : P::GaugeLateralB;" in dev,
        "BRMech::Clearance::FanSteals, juge par le banc Tools/Mechanisms (section J)": "int FanSteals(const FPlaced& Me, const FPlaced& Other, float FloorLift, FVec3* OutFirst)" in mech
        and "CL::FanSteals(NA, NB, Lift) + CL::FanSteals(NB, NA, Lift)" in bench,
        "BRMech::Clearance::Fan* = cercles, pas, arrivees et portee des tests (FindMissionStand, marches, ClientFocusReach)":
        K["fan_rings"] == K["rings"] and K["Clearance.FanStep"] == K["turn_deg"] and walk_r[0] == K["Clearance.FanArrivalFront"]
        and walk_r[1] == K["Clearance.FanArrival"] and K["Clearance.FanReach"] == K["ClientFocusReach"],
    }
    # v4.16 (visee) : cas H : choix de la visee du client entre voisins (BRMech::Clearance::PickAim, ABRCharacter::PickAimedDevice)
    K["Clearance.HostReach"] = num(r"constexpr float HostReach = ([\d.]+)f;", clh, "Clearance.HostReach")[0]
    K["Clearance.MaxAimCandidates"] = int(num(r"constexpr int MaxAimCandidates = (\d+);", clh, "Clearance.MaxAimCandidates")[0])
    pick = char[char.index("AActor* ABRCharacter::PickAimedDevice("):] if "AActor* ABRCharacter::PickAimedDevice(" in char else ""
    pick = pick[:pick.index("\n}") + 2] if pick else ""
    K["src_H"] = {
        "ABRCharacter::UpdateFocus : etape 1 par ABRCharacter::PickAimedDevice": "Accept(PickAimedDevice(GetWorld(), this, Start, Eye, Dir, Len, InteractReach + 30.f, Hit))" in char,
        "ABRCharacter::PickAimedDevice : boites visees traversees (meme sphere), arret au premier objet opaque, au plus MaxAimCandidates":
        "D0->IsAimBox(First.GetComponent())" in pick and "Q.AddIgnoredComponent(Next.GetComponent());" in pick
        and "FCollisionShape::MakeSphere(BRMech::Clearance::FocusSweep), Q)" in pick and "I < BRMech::Clearance::MaxAimCandidates" in pick
        and "if (!D->IsAimBox(Next.GetComponent()))" in pick,
        "ABRCharacter::PickAimedDevice : point vise le plus pres de l'axe, un voisin seulement selon la regle de l'hote":
        "D->GetInteractPoint() - Start" in pick and "Cos > BestCos && W && W->CanReachMissionDevice(Eye, D, Self)" in pick,
        "BRTestV412::FocusVerdict : le meme choix (Dev compte toujours)":
        "ABRCharacter::PickAimedDevice(World, Self, Eye, Eye, Dir, ClientFocusSweep, ClientFocusSweep + 30.f, Hit, Dev)" in walk,
        "BRMech::Clearance::PickAim et FanStealsPicked, juges par le banc Tools/Mechanisms (section K)":
        "int PickAim(const FVec3& Eye, const FVec3& Look, const FAimShape* Shapes, int NumShapes, const FVec3* Centres, int NumDevices, EAimPick Rule)" in mech
        and "CL::FanStealsPicked(WA, WB, 0.f)" in bench and "CL::PickAim(From, Far, S, N, Centres, 2, Rule)" in bench,
        "BRMech::Clearance::HostReach = ABRWorld::MissionReachServer": K["Clearance.HostReach"] == K["Reach"]
        and "static_assert(BRMech::Clearance::HostReach == ABRWorld::MissionReachServer" in char,
    }
    return K


# ------------------------------------------------------------------------------------------- bornes des modeles (FBX)
def _props(d, o, n):
    out = []
    for _ in range(n):
        t = chr(d[o])
        o += 1
        if t == "Y":
            out.append(struct.unpack_from("<h", d, o)[0])
            o += 2
        elif t == "C":
            out.append(d[o] != 0)
            o += 1
        elif t in "IF":
            out.append(struct.unpack_from("<" + ("i" if t == "I" else "f"), d, o)[0])
            o += 4
        elif t in "DL":
            out.append(struct.unpack_from("<" + ("d" if t == "D" else "q"), d, o)[0])
            o += 8
        elif t in "fdlib":
            ln, enc, cl = struct.unpack_from("<III", d, o)
            o += 12
            raw = d[o:o + cl]
            o += cl
            if enc == 1:
                raw = zlib.decompress(raw)
            fmt = {"f": "f", "d": "d", "l": "q", "i": "i", "b": "B"}[t]
            out.append(list(struct.unpack("<%d%s" % (ln, fmt), raw[:ln * struct.calcsize(fmt)])))
        elif t in "SR":
            ln = struct.unpack_from("<I", d, o)[0]
            o += 4
            out.append(d[o:o + ln])
            o += ln
        else:
            raise ValueError("FBX : type %r" % t)
    return out, o


def _nodes(d, o, end, ver):
    nodes = []
    while o < end:
        if ver >= 7500:
            eo, np_, pl = struct.unpack_from("<QQQ", d, o)
            o += 24
        else:
            eo, np_, pl = struct.unpack_from("<III", d, o)
            o += 12
        nl = d[o]
        o += 1
        if eo == 0:
            break
        name = d[o:o + nl].decode("latin-1")
        o += nl
        props, _ = _props(d, o, np_)
        o2 = o + pl
        nodes.append((name, props, _nodes(d, o2, eo, ver) if o2 < eo else []))
        o = eo
    return nodes


def fbx_bounds(path):
    """Bornes des sommets dans Unreal (X = X, Y = Z, Z = Y du fichier ; fichier Y haut, unite 1 cm, noeud identite)"""
    d = open(path, "rb").read()
    assert d[:20] == b"Kaydara FBX Binary  ", path
    ver = struct.unpack_from("<I", d, 23)[0]
    nodes = _nodes(d, 27, len(d), ver)
    objs = next(n for n in nodes if n[0] == "Objects")[2]
    xs, ys, zs = [], [], []
    for g in objs:
        if g[0] != "Geometry":
            continue
        for c in g[2]:
            if c[0] == "Vertices":
                v = c[1][0]
                xs += v[0::3]
                ys += v[1::3]
                zs += v[2::3]
    return (min(xs), min(zs), min(ys)), (max(xs), max(zs), max(ys))


# ------------------------------------------------------------------------------------------------------- geometrie
def rot(yaw):
    r = math.radians(yaw)
    return math.cos(r), math.sin(r)


def to_world(fr, lx, ly):
    """FFrame::ToWorld (lacet arrondi au quart de tour) : Y local = (-FY, FX)"""
    fx, fy = rot(fr[3])
    fx, fy = round(fx), round(fy)
    return fr[0] + fx * lx - fy * ly, fr[1] + fy * lx + fx * ly


def to_local(fr, wx, wy):
    fx, fy = rot(fr[3])
    fx, fy = round(fx), round(fy)
    dx, dy = wx - fr[0], wy - fr[1]
    return dx * fx + dy * fy, -dx * fy + dy * fx


def rect_overlap(a, b):
    """BRMech::Clearance::Overlap : rectangles orientes (cx, cy, hx, hy, yaw), separation des axes"""
    axes = []
    for r in (a, b):
        c, s = rot(r[4])
        axes.append(((c, s), (-s, c)))
    dx, dy = b[0] - a[0], b[1] - a[1]
    for n in (axes[0][0], axes[0][1], axes[1][0], axes[1][1]):
        dist = abs(dx * n[0] + dy * n[1])
        pa = a[2] * abs(axes[0][0][0] * n[0] + axes[0][0][1] * n[1]) + a[3] * abs(axes[0][1][0] * n[0] + axes[0][1][1] * n[1])
        pb = b[2] * abs(axes[1][0][0] * n[0] + axes[1][0][1] * n[1]) + b[3] * abs(axes[1][1][0] * n[0] + axes[1][1][1] * n[1])
        if dist > pa + pb:
            return False
    return True


def rect_of(fr, x0, y0, x1, y1):
    cx, cy = to_world(fr, (x0 + x1) * 0.5, (y0 + y1) * 0.5)
    return (cx, cy, abs(x1 - x0) * 0.5, abs(y1 - y0) * 0.5, round(fr[3] / 90.0) * 90.0)


def keep_out(K, fr, aim):
    """BRMech::Clearance::KeepOut (mecanisme sans volume)"""
    hy = abs(aim[1]) + aim[4] + K["Clearance.StandGap"]
    return rect_of(fr, -12.0, -hy, aim[0] + max(aim[3], K["Clearance.StandGap"]) + K["Clearance.StandDepth"], hy)


class OBox:
    """Boite orientee (lacet quelconque) : centre, demi-tailles, lacet, hauteur"""

    def __init__(self, cx, cy, hx, hy, yaw, z0, z1):
        self.cx, self.cy, self.hx, self.hy, self.yaw, self.z0, self.z1 = cx, cy, hx, hy, yaw, z0, z1
        self.c, self.s = rot(yaw)

    def local(self, x, y):
        dx, dy = x - self.cx, y - self.cy
        return dx * self.c + dy * self.s, -dx * self.s + dy * self.c

    def contains(self, p, grow=0.0):
        lx, ly = self.local(p[0], p[1])
        return abs(lx) <= self.hx + grow and abs(ly) <= self.hy + grow and self.z0 - grow <= p[2] <= self.z1 + grow

    def dist2d(self, x, y):
        lx, ly = self.local(x, y)
        ex, ey = max(abs(lx) - self.hx, 0.0), max(abs(ly) - self.hy, 0.0)
        return math.hypot(ex, ey)

    def hits(self, a, b):
        """Segment a -> b (3D) contre la boite (slabs dans le repere de la boite)"""
        la = self.local(a[0], a[1]) + (a[2],)
        lb = self.local(b[0], b[1]) + (b[2],)
        lo = (-self.hx, -self.hy, self.z0)
        hi = (self.hx, self.hy, self.z1)
        t0, t1 = 0.0, 1.0
        for i in range(3):
            dd = lb[i] - la[i]
            if abs(dd) < 1e-9:
                if la[i] < lo[i] or la[i] > hi[i]:
                    return False
                continue
            u0, u1 = (lo[i] - la[i]) / dd, (hi[i] - la[i]) / dd
            if u0 > u1:
                u0, u1 = u1, u0
            t0, t1 = max(t0, u0), min(t1, u1)
            if t0 > t1:
                return False
        return True

    def rect(self):
        return (self.cx, self.cy, self.hx, self.hy, self.yaw)

    def seg_dist(self, a, b):
        """v4.15 (correctifs 4) : distance 3D d'un segment a la boite (dans son repere)"""
        la = self.local(a[0], a[1]) + (a[2],)
        lb = self.local(b[0], b[1]) + (b[2],)
        return seg_box_dist(la, lb, (-self.hx, -self.hy, self.z0, self.hx, self.hy, self.z1))


def prop_box(bounds, px, py, pz, yaw, sx, sy, sz):
    """Boite de collision de ABRChunk::AddProp : bornes du modele a l'echelle, tournees, centre transforme"""
    (x0, y0, z0), (x1, y1, z1) = bounds
    c, s = rot(yaw)
    bcx, bcy = (x0 + x1) * 0.5 * sx, (y0 + y1) * 0.5 * sy
    return OBox(px + bcx * c - bcy * s, py + bcx * s + bcy * c, (x1 - x0) * 0.5 * sx, (y1 - y0) * 0.5 * sy, yaw, pz + z0 * sz, pz + z1 * sz)


def laterals(K, S):
    per = int(max(1, min(int(K["PerFaceMargin"][1]), math.floor((S - K["PerFaceMargin"][0]) / K["Spacing"]) + 1)))
    out = []
    for k in range(per):
        step = (k + 1) // 2
        out.append((1.0 if k % 2 == 1 else -1.0) * step * K["Spacing"])
    return out


def face_frame(S, c, d, lat, inset):
    """FaceSpot : cellule c, face d, decalage lateral (Side = (-d.y, d.x)), mecanisme tourne vers la cellule"""
    cx, cy = (c[0] + 0.5) * S, (c[1] + 0.5) * S
    sx, sy = -d[1], d[0]
    yaw = math.degrees(math.atan2(-d[1], -d[0]))
    return (cx + d[0] * (S * 0.5 - inset) + sx * lat, cy + d[1] * (S * 0.5 - inset) + sy * lat, 0.0, yaw)


def aim_point(fr, aim):
    x, y = to_world(fr, aim[0], aim[1])
    return (x, y, fr[2] + aim[2])


def forward(fr):
    c, s = rot(fr[3])
    return round(c), round(s)


def find_mission_stand(K, fr, aim, boxes, solid_cells=None, S=0.0):
    """v4.15 (correctifs) : miroir de BRTestV412::FindMissionStand (StandNear des tests du jeu lui delegue) : cercles et
    directions lus dans BRAutoTestV412.cpp (110, 160, 210, puis 75 et 240 cm ; 16 directions depuis l'avant du mecanisme, vers
    son axe droit), et pour chaque place CheckMissionStand : sol (plat ici), capsule de 36 cm (hauteur Half - 4, posee 5 cm
    au-dessus du sol) libre des boites et des cellules pleines, yeux du joueur debout (centre + 74 cm), point a portee de la
    visee du client (ClientFocusReach, 250 cm) puis regle de l'hote (340 cm, ligne de vue WorldStatic : boites, cellules
    pleines). Avant : cercles de 120 a 300 cm, 8 directions, yeux jusqu'a 340 cm (l'ancien StandNear)"""
    ip = aim_point(fr, aim)
    f = forward(fr)
    half = K["Capsule"][1]
    cap_r, cap_shrink = K["stand_cap"]
    cap_lo = fr[2] + K["stand_lift"] + cap_shrink
    cap_hi = fr[2] + K["stand_lift"] + 2.0 * half - cap_shrink
    eye_z = fr[2] + half + K["EyeZ"]

    def near_solid(x, y, r):
        if not solid_cells:
            return False
        cx, cy = math.floor(x / S), math.floor(y / S)
        for X in range(cx - 1, cx + 2):
            for Y in range(cy - 1, cy + 2):
                if solid_cells(X, Y):
                    dx = max(X * S - x, 0.0, x - (X + 1) * S)
                    dy = max(Y * S - y, 0.0, y - (Y + 1) * S)
                    if math.hypot(dx, dy) < r:
                        return True
        return False

    def los_solid(a, b):
        if not solid_cells:
            return False
        n = max(2, int(math.dist(a, b) / 10.0))
        for i in range(1, n):
            t = i / n
            x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            if solid_cells(math.floor(x / S), math.floor(y / S)):
                return True
        return False

    for dist in K["rings"]:
        for turn in range(K["turns"]):
            ang = math.radians(turn * K["turn_deg"])
            dx, dy = f[0] * math.cos(ang) - f[1] * math.sin(ang), f[0] * math.sin(ang) + f[1] * math.cos(ang)
            sx, sy = ip[0] + dx * dist, ip[1] + dy * dist
            if near_solid(sx, sy, cap_r):
                continue
            if any(b.z1 > cap_lo and b.z0 < cap_hi and b.dist2d(sx, sy) < cap_r for b in boxes):
                continue
            eye = (sx, sy, eye_z)
            if math.dist(eye, ip) > K["ClientFocusReach"]:
                continue
            if math.dist(eye, ip) > K["Reach"] or any(b.hits(eye, ip) for b in boxes) or los_solid(eye, ip):
                continue
            # v4.15 (correctifs 4) : la visee du client depuis ces yeux prend le mecanisme (FocusVerdict : balayage, puis tolerance)
            if not focus_first(K, eye, fr, aim, boxes, solid_cells, S):
                continue
            return True
    return False


def focus_first(K, eye, fr, aim, boxes, solid_cells=None, S=0.0):
    """v4.15 (correctifs 4) : miroir de la visee du client des tests du jeu (ABRCharacter::UpdateFocus) : la sphere de FocusSweep
    lancee des yeux vers le point vise touche la boite visee du mecanisme avant toute boite du decor (rocher, maison) et toute
    cellule pleine (contact avec la boite visee : entree du segment dans la boite grossie du rayon). v4.15 (correctifs 4,
    relecture) : sinon la tolerance (BRTestV412::FocusVerdict, second temps d'UpdateFocus). Avant : le decor touche en premier
    refusait la place"""
    r = K["focus_sweep"]
    ip = aim_point(fr, aim)
    c, e = world_aim_box(fr, aim)
    if focus_sweep_first(eye, ip, c, e, r, boxes, solid_cells, S):
        return True
    return focus_tolerance(K, eye, ip, c, e, boxes, solid_cells, S)


def focus_sweep_first(eye, ip, c, e, r, boxes, solid_cells, S):
    """Premier temps : le balayage atteint la boite visee (grossie du rayon) sans froler le decor"""
    t0, t1 = 0.0, 1.0
    for i in range(3):
        d = ip[i] - eye[i]
        lo, hi = c[i] - e[i] - r, c[i] + e[i] + r
        if abs(d) < 1e-9:
            if eye[i] < lo or eye[i] > hi:
                return False
            continue
        u0, u1 = (lo - eye[i]) / d, (hi - eye[i]) / d
        if u0 > u1:
            u0, u1 = u1, u0
        t0, t1 = max(t0, u0), min(t1, u1)
        if t0 > t1:
            return False
    stop = tuple(eye[k] + (ip[k] - eye[k]) * t0 for k in range(3))
    if any(b.seg_dist(eye, stop) < r for b in boxes):
        return False
    if solid_cells:
        n = max(2, int(math.dist(eye, stop) / 5.0))
        for i in range(n + 1):
            x, y = eye[0] + (stop[0] - eye[0]) * i / n, eye[1] + (stop[1] - eye[1]) * i / n
            cx, cy = math.floor(x / S), math.floor(y / S)
            for X in range(cx - 1, cx + 2):
                for Y in range(cy - 1, cy + 2):
                    if solid_cells(X, Y) and math.hypot(max(X * S - x, 0.0, x - (X + 1) * S), max(Y * S - y, 0.0, y - (Y + 1) * S)) < r:
                        return False
    return True


def focus_tolerance(K, eye, ip, c, e, boxes, solid_cells, S):
    """v4.15 (correctifs 4, relecture) : second temps d'UpdateFocus (miroir de BRTestV412::FocusVerdict) pour le seul mecanisme
    vise (pas de voisin dans les cas B et C) : sphere de InteractReach * tolerance autour des yeux qui touche sa boite visee,
    centre de la boite dans l'axe du regard (cosinus > focus_dot), ligne WorldStatic libre jusqu'au centre (boites du decor,
    cellules pleines)"""
    near = math.sqrt(sum(max(abs(eye[k] - c[k]) - e[k], 0.0) ** 2 for k in range(3)))
    if near > K["InteractReach"] * K["tolerance"]:
        return False
    look = tuple(ip[k] - eye[k] for k in range(3))
    to_c = tuple(c[k] - eye[k] for k in range(3))
    nl, nc = math.sqrt(sum(v * v for v in look)), math.sqrt(sum(v * v for v in to_c))
    if nl < 1e-6 or nc < 1e-6 or sum(look[k] * to_c[k] for k in range(3)) / (nl * nc) <= K["focus_dot"]:
        return False
    if any(b.hits(eye, c) for b in boxes):
        return False
    if solid_cells:
        n = max(2, int(math.dist(eye, c) / 10.0))
        for i in range(1, n):
            x, y = eye[0] + (c[0] - eye[0]) * i / n, eye[1] + (c[1] - eye[1]) * i / n
            if solid_cells(math.floor(x / S), math.floor(y / S)):
                return False
    return True


# ------------------------------------------------------------------------------------------------ A. Level 6
def pick_slot_mirror(K, prints, frames_lat, face_half, end_of_face=True, lanes=False):
    """BRMech::Clearance::PickSlot (miroir) pour un groupe : les membres a volume d'abord (tri stable, comme StableSort),
    chacun a la premiere place libre sans gene avec ceux deja poses ; un volume mural tient dans la face. v4.15 (correctifs,
    end_of_face) : un volume prend ensuite, sur cette face, la place libre de plus grand |lateral| ou il tient (la premiere a
    egalite). frames_lat : (repere, lateral le long de l'axe droit du mecanisme), une seule face ici"""
    order = sorted(range(len(prints)), key=lambda i: 0 if prints[i]["solid"] else 1)
    taken = [False] * len(frames_lat)
    placed = []
    out = [None] * len(prints)

    def usable(pr, j, with_lanes):
        fr, lat = frames_lat[j]
        if taken[j]:
            return False
        if pr["solid"] and abs(lat) + max(abs(pr["solid"][1]), abs(pr["solid"][4])) > face_half + 0.01:
            return False
        return not any(clash(K, (fr, pr), q) or (with_lanes and focus_clash(K, (fr, pr), q)) for q in placed)

    def pick(pr, with_lanes):
        for j in range(len(frames_lat)):
            if not usable(pr, j, with_lanes):
                continue
            best = j
            if pr["solid"] and end_of_face:
                for k in range(j + 1, len(frames_lat)):
                    if abs(frames_lat[k][1]) > abs(frames_lat[best][1]) + 0.01 and usable(pr, k, with_lanes):
                        best = k
            return best
        return None

    for i in order:
        pr = prints[i]
        # v4.15 (correctifs 3) : couloirs de visee degages d'abord, sinon le choix d'avant (BRMech::Clearance::PickSlot)
        best = pick(pr, True) if lanes else None
        if best is None:
            best = pick(pr, False)
        if best is None:
            continue
        taken[best] = True
        placed.append((frames_lat[best][0], pr))
        out[i] = best
    return out


def seg_box_dist(a, b, box):
    """Distance 3D minimale d'un segment a une boite alignee (x0, y0, z0, x1, y1, z1), echantillonnee tous les 2 cm"""
    n = max(2, int(math.dist(a, b) / 2.0))
    best = 1e9
    for i in range(n + 1):
        t = i / n
        p = [a[k] + (b[k] - a[k]) * t for k in range(3)]
        dx = max(box[0] - p[0], 0.0, p[0] - box[3])
        dy = max(box[1] - p[1], 0.0, p[1] - box[4])
        dz = max(box[2] - p[2], 0.0, p[2] - box[5])
        best = min(best, math.sqrt(dx * dx + dy * dy + dz * dz))
    return best


def seg_hits_box(a, b, box):
    """Segment contre boite alignee (slabs)"""
    t0, t1 = 0.0, 1.0
    for i in range(3):
        d = b[i] - a[i]
        lo, hi = box[i], box[i + 3]
        if abs(d) < 1e-9:
            if a[i] < lo or a[i] > hi:
                return False
            continue
        u0, u1 = (lo - a[i]) / d, (hi - a[i]) / d
        if u0 > u1:
            u0, u1 = u1, u0
        t0, t1 = max(t0, u0), min(t1, u1)
        if t0 > t1:
            return False
    return True


class Level6Face:
    """Une face du Level 6 dans le repere du mecanisme : u depuis le mur vers la salle, v le long de l'axe droit (Lateral du
    placement). Mur du fond de la face en u = 0 ; murs voisins (bouts de la face) en v = +-face_half s'ils existent ; mur d'en
    face (fond de la cellule) en u = S - T s'il existe. Cage (volume bloquant, toutes les collisions ; boite visee, visee du
    client) et poste (point d'interaction et boite visee) aux decalages donnes"""

    def __init__(self, K, face_half, cage_v, post_v, wall_plus, wall_minus, far):
        self.K = K
        self.fh = face_half
        g, dep, hw, h = K["LidarCage.Gap"], K["LidarCage.Depth"], K["LidarCage.HalfWidth"], K["LidarCage.Height"]
        self.cage = (g, cage_v - hw, 0.0, g + dep, cage_v + hw, h)
        ca = K["cage_aim"]
        self.cage_box = (ca[0] - ca[3], cage_v + ca[1] - ca[4], ca[2] - ca[5], ca[0] + ca[3], cage_v + ca[1] + ca[4], ca[2] + ca[5])
        pa = K["post"]
        self.aim = (pa[0], post_v + pa[1], pa[2])
        self.post_box = (pa[0] - pa[3], post_v + pa[1] - pa[4], pa[2] - pa[5], pa[0] + pa[3], post_v + pa[1] + pa[4], pa[2] + pa[5])
        self.wp, self.wm, self.far = wall_plus, wall_minus, far
        self.cap = K["stand_cap"][0]
        self.eye_z = K["Capsule"][1] + K["EyeZ"]

    def wall_dist(self, u, v):
        d = [u]
        if self.wp:
            d.append(self.fh - v)
        if self.wm:
            d.append(v + self.fh)
        if self.far:
            d.append(self.far - u)
        return min(d)

    def capsule_free(self, u, v):
        if self.wall_dist(u, v) < self.cap:
            return False
        dx = max(self.cage[0] - u, 0.0, u - self.cage[3])
        dy = max(self.cage[1] - v, 0.0, v - self.cage[4])
        return math.hypot(dx, dy) >= self.cap

    def host(self, u, v):
        """ABRWorld::CanReachMissionDevice : yeux a portee, ligne de vue qui n'ignore que le joueur et le poste (la cage bloque)"""
        eye = (u, v, self.eye_z)
        return math.dist(eye, self.aim) <= self.K["Reach"] and not seg_hits_box(eye, self.aim, self.cage)

    def check(self, u, v):
        """BRTestV412::CheckMissionStand (sol plat, pas d'eau au Level 6) ; v4.15 (correctifs 4) : puis la visee du client prend le
        poste (v4.15 correctifs 4, relecture : BRTestV412::FocusVerdict, miroir client() : la cage frolee la prend, un mur frole
        laisse jouer la tolerance ; avant : tout contact refusait la place, sans la tolerance)"""
        if not self.capsule_free(u, v):
            return "capsule"
        if math.dist((u, v, self.eye_z), self.aim) > self.K["ClientFocusReach"]:
            return "loin"
        if not self.host(u, v):
            return "vue"
        if not self.client(u, v)[0]:
            return "visee"
        return "ok"

    def client(self, u, v):
        """ABRCharacter::UpdateFocus, regard sur le point du poste : balayage de 7 cm jusqu'a la boite du poste (la cage frolee
        la prend : visee prise), un mur frole fait echouer le balayage -> tolerance (boite a 208 cm, ligne de vue). Rend
        (vise, ecart a la cage)"""
        K = self.K
        r = K["focus_sweep"]
        eye = (u, v, self.eye_z)
        g = tuple(self.post_box[k] - r for k in range(3)) + tuple(self.post_box[k] + r for k in range(3, 6))
        n = max(2, int(math.dist(eye, self.aim) / 2.0))
        stop = 1.0
        for i in range(n + 1):
            t = i / n
            p = [eye[k] + (self.aim[k] - eye[k]) * t for k in range(3)]
            if g[0] <= p[0] <= g[3] and g[1] <= p[1] <= g[4] and g[2] <= p[2] <= g[5]:
                stop = t
                break
        end = tuple(eye[k] + (self.aim[k] - eye[k]) * stop for k in range(3))
        gap = min(seg_box_dist(eye, end, self.cage), seg_box_dist(eye, end, self.cage_box))
        if gap < r:
            return False, gap
        side = min(self.wall_dist(eye[0] + (end[0] - eye[0]) * t / 10.0, eye[1] + (end[1] - eye[1]) * t / 10.0) for t in range(11))
        if side < r or math.dist(eye, end) > K["InteractReach"]:
            near = math.dist(eye, (min(max(eye[0], self.post_box[0]), self.post_box[3]), min(max(eye[1], self.post_box[1]), self.post_box[4]),
                                   min(max(eye[2], self.post_box[2]), self.post_box[5])))
            return near <= K["InteractReach"] * K["tolerance"] and self.host(u, v), gap
        return True, gap

    def find(self):
        """BRTestV412::FindMissionStand (cercles et directions du code)"""
        K = self.K
        for dist in K["rings"]:
            for turn in range(K["turns"]):
                a = math.radians(turn * K["turn_deg"])
                u, v = self.aim[0] + math.cos(a) * dist, self.aim[1] + math.sin(a) * dist
                if self.check(u, v) == "ok":
                    return (u, v), dist, turn
        return None, None, None

    def disc(self, c, radius):
        """Points d'arrivee possibles d'une marche (centre de la capsule libre a moins de radius du but) : nombre, vue de
        l'hote coupee, visee prise par la cage (ou ratee), ecart minimal du balayage a la cage"""
        n = bad_h = bad_c = 0
        gap_min = 1e9
        u = c[0] - radius
        while u <= c[0] + radius:
            v = c[1] - radius
            while v <= c[1] + radius:
                if math.hypot(u - c[0], v - c[1]) <= radius and self.capsule_free(u, v):
                    n += 1
                    if not self.host(u, v):
                        bad_h += 1
                    else:
                        ok, gap = self.client(u, v)
                        bad_c += 0 if ok else 1
                        gap_min = min(gap_min, gap)
                v += 5.0
            u += 5.0
        return n, bad_h, bad_c, gap_min


def level6_walk(K, face):
    """Marche des tests du jeu (AddWalkMissionSteps) vers le poste : la place devant lui (110 cm) si elle est bonne, rayon
    d'arrivee 70 cm, sinon la place de FindMissionStand, rayon 35 cm ; puis, si la visee rate, la marche de reprise (but a
    60 cm si c'est une place, sinon le premier but ; rayon 35). Rend un dict de mesures et le verdict"""
    a = face.aim
    out = {"inside": seg_box_dist(a, a, face.cage) <= K["Clearance.Margin"]}
    front = (a[0] + K["front_ahead"], a[1])
    out["front"] = face.check(*front)
    st, dist, turn = face.find()
    out["stand"] = (st, dist, turn)
    if out["front"] == "ok":
        goal, radius = front, K["walk_radius"][0]
    else:
        goal, radius = st, K["walk_radius"][1]
    out["goal"], out["radius"] = goal, radius
    out["ok"] = False
    if out["inside"] or goal is None:
        return out
    out["d1"] = face.disc(goal, radius)
    closer = (a[0] + K["retry"][0], a[1])
    goal2 = closer if face.check(*closer) == "ok" else goal
    out["goal2"] = goal2
    out["d2"] = face.disc(goal2, K["retry"][1])
    d1, d2 = out["d1"], out["d2"]
    out["ok"] = d1[0] > 0 and d1[1] == 0 and (d1[2] == 0 or (d2[0] > 0 and d2[1] == 0 and d2[2] == 0))
    return out


def solid_box(fr, sol):
    x0, y0, z0, x1, y1, z1 = sol
    r = rect_of(fr, x0, y0, x1, y1)
    return OBox(r[0], r[1], r[2], r[3], r[4], fr[2] + z0, fr[2] + z1)


def hinders(K, me, other):
    fr, pr = me
    ofr, opr = other
    if not opr["solid"]:
        return False
    box = solid_box(ofr, opr["solid"])
    ip = aim_point(fr, pr["aim"])
    if box.contains(ip, K["Clearance.Margin"]):
        return True
    if opr["solid"][5] - opr["solid"][2] <= K["Clearance.StepHeight"]:
        return False
    sx, sy = to_world(fr, pr["aim"][0] + K["Clearance.StandGap"], pr["aim"][1])
    return box.dist2d(sx, sy) < K["Clearance.PlayerRadius"]


def clash(K, a, b):
    if hinders(K, a, b) or hinders(K, b, a):
        return True
    if a[1]["solid"] and b[1]["solid"]:
        return rect_overlap(solid_box(a[0], a[1]["solid"]).rect(), solid_box(b[0], b[1]["solid"]).rect())
    return False


def seg_aabb(a, b, c, e):
    """Distance d'un segment a une boite alignee (centre c, demi-tailles e) : recherche ternaire (fonction convexe)"""
    def at(t):
        p = [a[k] + (b[k] - a[k]) * t for k in range(3)]
        return math.sqrt(sum(max(abs(p[k] - c[k]) - e[k], 0.0) ** 2 for k in range(3)))
    lo, hi = 0.0, 1.0
    for _ in range(60):
        m1, m2 = lo + (hi - lo) / 3.0, hi - (hi - lo) / 3.0
        if at(m1) <= at(m2):
            hi = m2
        else:
            lo = m1
    return min(at(0.0), at(1.0), at((lo + hi) * 0.5))


def cuts_focus_lane(K, me, other):
    """BRMech::Clearance::CutsFocusLane (miroir) : du point vise de me vers les yeux a la place ordinaire (StandRange devant,
    StandEye au-dessus du sol) et vers ce point a hauteur du point vise ; boite visee de other (si elle arrete la visee) ou son
    volume a moins de FocusSweep + Margin. me, other : (repere, emprise) ; emprise : aim (6 valeurs), solid, aimable"""
    fr, pr = me
    ofr, opr = other
    if not pr.get("aimable", True):
        return False
    aim = pr["aim"]
    p = aim_point(fr, aim)
    sx, sy = to_world(fr, aim[0] + K["Clearance.StandRange"], aim[1])
    ax, ay = to_local(ofr, p[0], p[1])
    ex, ey = to_local(ofr, sx, sy)
    A = (ax, ay, p[2] - ofr[2])
    eye = (ex, ey, fr[2] + K["Clearance.StandEye"] - ofr[2])
    level = (ex, ey, A[2])
    lim = K["Clearance.FocusSweep"] + K["Clearance.Margin"]
    boxes = []
    if opr.get("aimable", True):
        oa = opr["aim"]
        boxes.append((oa[:3], oa[3:6]))
    if opr["solid"]:
        x0, y0, z0, x1, y1, z1 = opr["solid"]
        boxes.append((((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5), (abs(x1 - x0) * 0.5, abs(y1 - y0) * 0.5, abs(z1 - z0) * 0.5)))
    return any(seg_aabb(A, eye, c, e) < lim or seg_aabb(A, level, c, e) < lim for c, e in boxes)


def focus_clash(K, a, b):
    return cuts_focus_lane(K, a, b) or cuts_focus_lane(K, b, a)


def case_level6(K, rules):
    S = K["L6"].get("CellSize", 350.0)
    T = K["L6"].get("WallThickness", K["WallThicknessDefault"])
    face_half = S * 0.5 - T * 0.5
    lats = laterals(K, S)
    post = {"aim": K["post"], "solid": None}
    cage = {"aim": K["cage_aim"], "solid": (K["LidarCage.Gap"], -K["LidarCage.HalfWidth"], 0.0, K["LidarCage.Gap"] + K["LidarCage.Depth"], K["LidarCage.HalfWidth"],
                                           K["LidarCage.Height"])}
    # Repere de la face : mecanisme tourne vers +u (lacet 0), decalage le long de son axe droit. FaceSpot(C, Dir, Lateral(K))
    # decale le long de (-Dir.Y, Dir.X), l'oppose de l'axe droit : Lateral du placement (LateralOf) = -Lateral(K)
    slots = [((0.0, -lat, 0.0, 0.0), -lat) for lat in lats]
    print("A. Level 6 : poste et cage d'un groupe (cellule de %g cm, murs de %g, face utile +-%g ; places (axe droit) %s ; cage %g..%g cm du mur, +-%g, boite visee %s ; "
          "place devant a %g cm, marche : rayon %g (devant) ou %g (autour), reprise a %g cm, rayon %g ; cercles %s, %d directions ; visee du client : balayage de %g cm, "
          "portee %g, tolerance %g)"
          % (S, T, face_half, "/".join("%+g" % s[1] for s in slots), K["LidarCage.Gap"], K["LidarCage.Gap"] + K["LidarCage.Depth"], K["LidarCage.HalfWidth"],
             tuple(K["cage_aim"][:3]), K["front_ahead"], K["walk_radius"][0], K["walk_radius"][1], K["retry"][0], K["retry"][1], "/".join("%g" % r for r in K["rings"]),
             K["turns"], K["focus_sweep"], K["ClientFocusReach"], K["InteractReach"] * K["tolerance"]))
    variants = [(wp, wm, far) for wp in (False, True) for wm in (False, True) for far in (None, S - T)]
    plans = []
    for name in rules:
        if name == "old":
            plans.append(("old", "v4.14", [0, 1]))  # membres dans l'ordre du plan (poste 1+K puis cage 5+K), places 0 puis 1
        else:
            plans.append(("first", "v4.15 avant correctifs (pour information)", pick_slot_mirror(K, [post, cage], slots, face_half, end_of_face=False)))
            plans.append(("new", "v4.15", pick_slot_mirror(K, [post, cage], slots, face_half, end_of_face=True, lanes=True)))
    results = {}
    for key, label, where in plans:
        if None in where:
            print("  regle %s : aucune place (placement impossible) -> ECHOUE" % label)
            results[key] = False
            continue
        pv, cv = slots[where[0]][1], slots[where[1]][1]
        fits = abs(cv) + K["LidarCage.HalfWidth"] <= face_half
        n_ok = 0
        lines = []
        for wp, wm, far in variants:
            face = Level6Face(K, face_half, cv, pv, wp, wm, far)
            w = level6_walk(K, face)
            ok = w["ok"] and fits
            n_ok += 1 if ok else 0
            where_txt = "murs voisins %s/%s, fond %s" % ("oui" if wp else "non", "oui" if wm else "non", "oui" if far else "non")
            if w["inside"]:
                lines.append("    %s : point du poste DANS la cage -> ECHOUE" % where_txt)
                continue
            st = w["stand"]
            if w["goal"] is None:
                lines.append("    %s : place devant %s, aucune place (FindMissionStand) -> ECHOUE" % (where_txt, w["front"]))
                continue
            d1, d2 = w["d1"], w["d2"]
            lines.append("    %s : place devant (%g cm) %s ; premiere place %s (cercle %g, direction %d) ; marche vers (%.0f, %.0f) rayon %g : %d arrivees, vue de l'hote coupee %d, "
                         "visee prise ou ratee %d (balayage a %.0f cm de la cage au plus pres) ; reprise vers (%.0f, %.0f) rayon %g : %d/%d/%d -> %s"
                         % (where_txt, K["front_ahead"], "libre" if w["front"] == "ok" else w["front"], "(%.0f, %.0f)" % st[0] if st[0] else "aucune", st[1] or 0,
                            st[2] or 0, w["goal"][0], w["goal"][1], w["radius"], d1[0], d1[1], d1[2], d1[3], w["goal2"][0], w["goal2"][1], K["retry"][1], d2[0], d2[1], d2[2],
                            "PASSE" if ok else "ECHOUE"))
        ok_all = n_ok == len(variants)
        results[key] = ok_all
        print("  regle %s : poste a %+g, cage a %+g (axe droit) ; cage dans la face : %s ; %d/%d dispositions des murs reussies -> %s"
              % (label, pv, cv, "oui" if fits else "NON", n_ok, len(variants), "PASSE" if ok_all else "ECHOUE"))
        shown = lines if key != "old" else lines[:1]
        for ln in shown:
            print(ln)
    return results


# ------------------------------------------------------------------------------------------------ B. Level 8
class Caves:
    def __init__(self, K, seed):
        L = K["L8"]
        self.S = L["CellSize"]
        self.solid_chance = vp.F32(L["SolidChance"])
        self.density = L.get("PropDensity", 0.0)
        self.seed = seed
        self._solid = {}

    def solid(self, x, y):
        k = (x, y)
        v = self._solid.get(k)
        if v is None:
            if abs(x) <= 1 and abs(y) <= 1:
                v = False
            else:
                r = vp.F32(0.55) * vp.rand(x, y, 601, self.seed) + vp.F32(0.45) * vp.rand(vp.floordiv(x, 3), vp.floordiv(y, 3), 602, self.seed)
                v = bool(r < self.solid_chance)
            self._solid[k] = v
        return v


def rocks_near(K, W, c, bounds, radius=3, exclude=None):
    """Rochers que ABRChunk pose autour de la cellule c : parois des cellules pleines (etape 5) et centre des cellules
    libres voisines (BuildCellProps ; pas dans la cellule du mecanisme, reservee a la mission ; v4.15 correctifs 4 : exclude, la
    cellule sans accessoire, sinon c)"""
    S = W.S
    wr, cr = K["wall_rock"], K["cell_rock"]
    out = []
    for X in range(c[0] - radius, c[0] + radius + 1):
        for Y in range(c[1] - radius, c[1] + radius + 1):
            if W.solid(X, Y):
                for k, d in enumerate(vp.DIRS):
                    if W.solid(X + d[0], Y + d[1]):
                        continue
                    if not vp.rand(X * 4 + k, Y, 931, W.seed) < vp.F32(wr["chance"]):
                        continue
                    sc = wr["sc0"] + wr["sc1"] * float(vp.rand(X, Y * 4 + k, 932, W.seed))
                    fx, fy = (X + 0.5) * S + d[0] * S * 0.5, (Y + 0.5) * S + d[1] * S * 0.5
                    off = (float(vp.rand(X, Y, 933 + k, W.seed)) - 0.5) * S * wr["spread"]
                    px, py = fx - d[1] * off - d[0] * wr["inset"], fy + d[0] * off - d[1] * wr["inset"]
                    yaw = float(vp.rand(X, Y, 937 + k, W.seed)) * 360.0
                    out.append(prop_box(bounds, px, py, 0.0, yaw, sc, sc, sc * wr["zs"]))
            elif (X, Y) != (c if exclude is None else exclude) and not (abs(X) <= 1 and abs(Y) <= 1) and abs(X - c[0]) <= 2 and abs(Y - c[1]) <= 2:
                R = lambda salt: float(vp.rand(X, Y, salt, W.seed))
                if R(930) < W.density * cr["density"]:
                    sc = cr["sc0"] + R(931) * cr["sc1"]
                    px = (X + 0.5) * S + (R(932) - 0.5) * S * cr["spread"]
                    py = (Y + 0.5) * S + (R(933) - 0.5) * S * cr["spread"]
                    out.append(prop_box(bounds, px, py, cr["z"], R(934) * 360.0, sc, sc, sc))
    return out


def case_level8(K, rules, bounds, seeds):
    S = K["L8"]["CellSize"]
    lats = laterals(K, S)
    kinds = (("treuil", K["winch"]), ("entailles", K["default"]))
    print("B. Level 8 : rochers (SM_Rock %s..%s cm ; paroi : %.0f %% des faces, echelle %.1f a %.1f, z x%.1f ; cellule : densite %.2f x %.1f, echelle %.2f a %.2f) ; "
          "%d graines, places laterales %s"
          % (tuple(round(v, 1) for v in bounds[0]), tuple(round(v, 1) for v in bounds[1]), 100 * K["wall_rock"]["chance"], K["wall_rock"]["sc0"],
             K["wall_rock"]["sc0"] + K["wall_rock"]["sc1"], K["wall_rock"]["zs"], K["L8"].get("PropDensity", 0), K["cell_rock"]["density"],
             K["cell_rock"]["sc0"], K["cell_rock"]["sc0"] + K["cell_rock"]["sc1"], seeds, "/".join("%+g" % v for v in lats)))
    stats = {r: {k: [0, 0, 0] for k, _ in kinds} for r in rules}  # places, point dans un rocher, sans place debout
    # Place 6 d'une face de 400 cm (+174 cm, a 26 cm du coin) : jamais prise au Level 8 (un seul mecanisme de groupe, a la
    # premiere place libre de la salle ; les mecanismes seuls sont a 0) : comptee a part, pour information
    corner = {r: [0, 0, 0] for r in rules}
    removed = [0, 0]
    for si in range(seeds):
        seed = vp.mix((si + 1) * 2654435761 & M32) or 1
        W = Caves(K, seed)
        for cxi in range(-6, 7, 3):
            for cyi in range(-6, 7, 3):
                c = (cxi, cyi)
                if W.solid(*c) or (abs(c[0]) <= 1 and abs(c[1]) <= 1):
                    continue
                rocks = rocks_near(K, W, c, bounds)
                for d in vp.DIRS:
                    if not W.solid(c[0] + d[0], c[1] + d[1]):
                        continue
                    for lat in lats:
                        fr = face_frame(S, c, d, lat, 0.0)
                        for kname, aim in kinds:
                            ko = keep_out(K, fr, aim)
                            for rname in rules:
                                kept = rocks if rname == "old" else [b for b in rocks if not rect_overlap(ko, b.rect())]
                                if rname == "new" and kname == "treuil":
                                    removed[0] += len(rocks) - len(kept)
                                    removed[1] += len(rocks)
                                ip = aim_point(fr, aim)
                                st = stats[rname][kname] if abs(lat) < S * 0.5 - 40.0 else corner[rname]
                                st[0] += 1
                                buried = any(b.contains(ip) for b in kept)
                                st[1] += 1 if buried else 0
                                st[2] += 0 if (not buried and find_mission_stand(K, fr, aim, kept, W.solid, S)) else 1
    results = {}
    for rname in rules:
        ok = all(st[1] == 0 and st[2] == 0 for st in stats[rname].values())
        results[rname] = ok
        parts = ["%s : %d places, point dans un rocher %d (%.1f %%), sans place debout (FindMissionStand) %d (%.1f %%)"
                 % (k, st[0], st[1], 100.0 * st[1] / max(1, st[0]), st[2], 100.0 * st[2] / max(1, st[0])) for k, st in stats[rname].items()]
        print("  regle %s : %s -> %s" % ("v4.14" if rname == "old" else "v4.15", " ; ".join(parts), "PASSE" if ok else "ECHOUE"))
        c = corner[rname]
        print("    (pour information, place au coin %+g : %d cas, point dans un rocher %d, sans place debout %d)" % (lats[-1], c[0], c[1], c[2]))
    if "new" in rules:
        print("  regle v4.15 : rochers retires autour d'un treuil (zone libre) : %d sur %d voisins (%.1f %%) ; dans le jeu : %s"
              % (removed[0], removed[1], 100.0 * removed[0] / max(1, removed[1]), "oui (RockInKeepOut)" if K["rock_keepout_in_game"] else "NON"))
    return results


# ------------------------------------------------------------------------------------------------ C. Level 9
def case_level9(K, rules, bounds, seeds):
    L = K["L9"]
    S, sp = L["CellSize"], int(L["Spacing"])
    lats = laterals(K, S)
    kinds = (("plan de maison", K["plate"]), ("boitier de rue", K["streetbox"]), ("plaque de facade", K["marker"]))
    body = K["house_body"]
    print("C. Level 9 : maisons (SM_House %s..%s cm ; v4.15 : corps jusqu'a %s cm, perron +-%s cm, %s cm de haut) ; lots de %g cm ; %d graines"
          % (tuple(round(v, 1) for v in bounds[0]), tuple(round(v, 1) for v in bounds[1]), body[0] if body else "?", body[1] if body else "?", body[2] if body else "?",
             2 * S, seeds))
    (x0, y0, z0), (x1, y1, z1) = bounds

    def house_boxes(X, Y, yaw, rname):
        hx, hy = (X + 1) * S, (Y + 1) * S
        if rname == "old" or not body or x1 <= body[0]:
            return [prop_box(((x0, y0, z0), (x1, y1, z1)), hx, hy, 0.0, yaw, 1, 1, 1)]
        return [prop_box(((x0, y0, z0), (body[0], y1, z1)), hx, hy, 0.0, yaw, 1, 1, 1),
                prop_box(((body[0], -body[1], z0), (x1, body[1], body[2])), hx, hy, 0.0, yaw, 1, 1, 1)]

    stats = {r: {k: [0, 0, 0] for k, _ in kinds} for r in rules}
    for si in range(seeds):
        seed = vp.mix((si + 1) * 2246822519 & M32) or 1
        has = lambda lx, ly: bool(vp.rand(lx, ly, 801, seed) < vp.F32(0.85))

        def lot_solid(x, y):
            if vp.posmod(x, sp) == 0 or vp.posmod(y, sp) == 0:
                return False
            return has(vp.floordiv(x, sp) * 2 + (vp.posmod(x, sp) - 1) // 2, vp.floordiv(y, sp) * 2 + (vp.posmod(y, sp) - 1) // 2)

        houses = {}
        for X in range(-12, 13):
            for Y in range(-12, 13):
                LX, LY = vp.posmod(X, sp), vp.posmod(Y, sp)
                if LX in (1, 3) and LY in (1, 3):
                    lot = (vp.floordiv(X, sp) * 2 + (LX - 1) // 2, vp.floordiv(Y, sp) * 2 + (LY - 1) // 2)
                    if has(*lot):
                        houses[(X, Y)] = 180.0 if LX == 1 else 0.0
        for X in range(-9, 10):
            for Y in range(-9, 10):
                c = (X, Y)
                if lot_solid(*c):
                    continue
                for d in vp.DIRS:
                    if not lot_solid(c[0] + d[0], c[1] + d[1]):
                        continue
                    for lat in lats:
                        fr = face_frame(S, c, d, lat, 0.0)
                        for kname, aim in kinds:
                            ip = aim_point(fr, aim)
                            for rname in rules:
                                boxes = []
                                for (hxc, hyc), yaw in houses.items():
                                    if abs((hxc + 1) * S - ip[0]) < 1200 and abs((hyc + 1) * S - ip[1]) < 1200:
                                        boxes += house_boxes(hxc, hyc, yaw, rname)
                                st = stats[rname][kname]
                                st[0] += 1
                                buried = any(b.contains(ip) for b in boxes)
                                st[1] += 1 if buried else 0
                                st[2] += 0 if (not buried and find_mission_stand(K, fr, aim, boxes)) else 1
    results = {}
    for rname in rules:
        ok = all(st[1] == 0 and st[2] == 0 for st in stats[rname].values())
        results[rname] = ok
        parts = ["%s : %d places, point dans la maison %d (%.1f %%), sans place debout (FindMissionStand) %d" % (k, st[0], st[1], 100.0 * st[1] / max(1, st[0]), st[2])
                 for k, st in stats[rname].items()]
        print("  regle %s : %s -> %s" % ("v4.14" if rname == "old" else "v4.15", " ; ".join(parts), "PASSE" if ok else "ECHOUE"))
    return results


# ------------------------------------------------------------------------------------------- D. Level 8 : sorties
def case_level8_exits(K, rules, bounds, seeds):
    """Sortie de la mission contre une paroi (FaceSpot au decalage 0, comme ABRWorld::PlaceMission) : la capsule qui monte a
    l'echelle (ABRExit::GetClimbAnchor) et l'approche de la marche des tests jusqu'a 90 cm de plus ne touchent aucun rocher
    plus haut qu'une marche"""
    S = K["L8"]["CellSize"]
    back, extra = K["exit_keepout"]
    hy = K["LidarDoor.Width"] * 0.5 + K["Clearance.StandGap"]
    anchor = K["climb_anchor"]
    R = K["Capsule"][0]
    print("D. Level 8 : sorties de la mission (echelle : capsule de %g cm a %g cm du pied, approche jusqu'a %g cm) ; zone libre v4.15 de %g cm dans la paroi a %g cm "
          "devant le pied, +-%g cm ; %d graines" % (R, anchor, anchor + 90.0, back, K["Clearance.StandDepth"] + extra, hy, seeds))
    stats = {r: [0, 0] for r in rules}
    for si in range(seeds):
        seed = vp.mix((si + 1) * 2654435761 & M32) or 1
        W = Caves(K, seed)
        for cxi in range(-6, 7, 3):
            for cyi in range(-6, 7, 3):
                c = (cxi, cyi)
                if W.solid(*c) or (abs(c[0]) <= 1 and abs(c[1]) <= 1):
                    continue
                rocks = rocks_near(K, W, c, bounds)
                for d in vp.DIRS:
                    if not W.solid(c[0] + d[0], c[1] + d[1]):
                        continue
                    fr = face_frame(S, c, d, 0.0, 0.0)
                    ko = rect_of(fr, -back, -hy, K["Clearance.StandDepth"] + extra, hy)
                    f = forward(fr)
                    pts = [(fr[0] + f[0] * x, fr[1] + f[1] * x) for x in range(int(anchor), int(anchor + 90.0) + 1, 10)]
                    for rname in rules:
                        kept = rocks if rname == "old" else [b for b in rocks if not rect_overlap(ko, b.rect())]
                        tall = [b for b in kept if b.z1 - fr[2] > K["Clearance.StepHeight"]]
                        st = stats[rname]
                        st[0] += 1
                        st[1] += 1 if any(b.dist2d(px, py) < R for b in tall for (px, py) in pts) else 0
    results = {}
    for rname in rules:
        st = stats[rname]
        results[rname] = st[1] == 0
        print("  regle %s : %d sorties, echelle ou approche dans un rocher %d (%.1f %%) -> %s" % ("v4.14" if rname == "old" else "v4.15", st[0], st[1],
                                                                                         100.0 * st[1] / max(1, st[0]), "PASSE" if st[1] == 0 else "ECHOUE"))
    return results


# ------------------------------------------------------------------------------- F. Level 8 : echelles des chunks
def chunk_ladder(K, W, ch, seed, chance):
    """ABRChunk::PlanExits, branche murale (miroir : BRMech::Grid::PickWallFace), Level 8 (niveau ouvert, sortie 0, echelle) :
    24 essais tires ; grottes : seules les faces pleines (pas de mur fin) ; cellules de la mission non rejouees"""
    if ch == (0, 0) or not vp.rand(ch[0], ch[1], 1100, seed) < vp.F32(chance):
        return None
    S, N = W.S, int(K["L8"].get("ChunkCells", K["ChunkCellsDefault"]))
    x0, y0 = ch[0] * N, ch[1] * N
    for t in range(24):
        h = vp.bhash(ch[0], ch[1], 1500 + t, seed)
        cell = (x0 + h % N, y0 + (h >> 8) % N)
        d = vp.DIRS[(h >> 16) % 4]
        if W.solid(*cell) or (abs(cell[0]) <= 1 and abs(cell[1]) <= 1) or not W.solid(cell[0] + d[0], cell[1] + d[1]):
            continue
        return dict(cell=cell, frame=face_frame(S, cell, d, 0.0, 0.0), fwd=(-d[0], -d[1]))
    return None


def case_level8_chunk_exits(K, rules, bounds, seeds):
    S = K["L8"]["CellSize"]
    back, extra = K["exit_keepout"]
    hy = K["LidarDoor.Width"] * 0.5 + K["Clearance.StandGap"]
    anchor = K["climb_anchor"]
    R = K["Capsule"][0]
    chance = K["L8_ladder_chance"]
    print("F. Level 8 : echelles tirees par les chunks (%.0f %% par chunk, partie classique ; point vise par le depart a %g cm devant le pied et %g cm de haut, sonde de 10 cm ; "
          "capsule du grimpeur de %g cm a %g cm ; approche jusqu'a %g cm) ; zone libre v4.15 de %g cm dans la paroi a %g cm devant le pied, +-%g cm ; %d graines, chunks -4..4"
          % (100.0 * chance, K["gather_ahead"], K["view_height"], R, anchor, anchor + 90.0, back, K["Clearance.StandDepth"] + extra, hy, seeds))
    stats = {r: [0, 0, 0, 0, 0] for r in rules}  # echelles, point vise, colonne, approche, point vise seul (colonne libre)
    removed = [0, 0]
    for si in range(seeds):
        seed = vp.mix((si + 1) * 2654435761 & M32) or 1
        W = Caves(K, seed)
        ladders = []
        for cx in range(-5, 6):
            for cy in range(-5, 6):
                ex = chunk_ladder(K, W, (cx, cy), seed, chance)
                if ex:
                    ladders.append(ex)
        kos = [rect_of(ex["frame"], -back, -hy, K["Clearance.StandDepth"] + extra, hy) for ex in ladders]
        for ex in ladders:
            n = int(K["L8"].get("ChunkCells", K["ChunkCellsDefault"]))
            ch = (vp.floordiv(ex["cell"][0], n), vp.floordiv(ex["cell"][1], n))
            if abs(ch[0]) > 4 or abs(ch[1]) > 4:
                continue
            fr = ex["frame"]
            fx, fy = ex["fwd"]
            view = (fr[0] + fx * K["gather_ahead"], fr[1] + fy * K["gather_ahead"], fr[2] + K["view_height"])
            anc = (fr[0] + fx * anchor, fr[1] + fy * anchor)
            front = (math.floor((fr[0] + fx * 30.0) / S), math.floor((fr[1] + fy * 30.0) / S))
            rocks = [b for b in rocks_near(K, W, ex["cell"], bounds, exclude=front)]
            for rname in rules:
                kept = rocks if rname == "old" else [b for b in rocks if not any(rect_overlap(ko, b.rect()) for ko in kos)]
                if rname == "new":
                    removed[0] += len(rocks) - len(kept)
                    removed[1] += len(rocks)
                tall = [b for b in kept if b.z1 - fr[2] > K["Clearance.StepHeight"]]
                v = any(b.contains(view, 10.0) and box_point_dist(b, view) <= 10.0 for b in kept)
                col = any(b.dist2d(*anc) < R for b in tall)
                app = any(b.dist2d(fr[0] + fx * x, fr[1] + fy * x) < R for b in tall for x in range(int(anchor), int(anchor + 90.0) + 1, 10))
                st = stats[rname]
                st[0] += 1
                st[1] += 1 if v else 0
                st[2] += 1 if col else 0
                st[3] += 1 if app else 0
                st[4] += 1 if (v and not col) else 0
    results = {}
    for rname in rules:
        st = stats[rname]
        results[rname] = st[1] == 0 and st[2] == 0 and st[3] == 0
        print("  regle %s : %d echelles, point vise dans un rocher %d (%.1f %%), colonne du grimpeur prise %d (%.1f %%), approche prise %d (%.1f %%), point vise seul (colonne libre) %d "
              "-> %s" % ("v4.14" if rname == "old" else "v4.15", st[0], st[1], 100.0 * st[1] / max(1, st[0]), st[2], 100.0 * st[2] / max(1, st[0]), st[3],
                         100.0 * st[3] / max(1, st[0]), st[4], "PASSE" if results[rname] else "ECHOUE"))
    if "new" in rules:
        print("  regle v4.15 : rochers retires autour des echelles : %d sur %d voisins (%.1f %%) ; dans le jeu : %s"
              % (removed[0], removed[1], 100.0 * removed[0] / max(1, removed[1]), "oui (ABRWorld::IsChunkExitKeepOut)" if all(K["src_F"].values()) else "NON"))
    return results


def box_point_dist(b, p):
    lx, ly = b.local(p[0], p[1])
    ex, ey = max(abs(lx) - b.hx, 0.0), max(abs(ly) - b.hy, 0.0)
    ez = max(b.z0 - p[2], 0.0, p[2] - b.z1)
    return math.sqrt(ex * ex + ey * ey + ez * ez)


# ------------------------------------------------------------------------------------- E. Niveau 10 : poteaux
def bhash_mission(seed, salt):
    """BRMission::Hash"""
    return vp.mix(((seed * 0x9E3779B1) & M32) ^ vp.mix((salt + 0x632BE59B) & M32))


def post_yaw(seed, cell, salt):
    """PostSpot : lacet tire par cellule et par sel"""
    s = (seed ^ ((cell[0] * 73856093) & M32) ^ ((cell[1] * 19349663) & M32)) & M32
    return 90.0 * (bhash_mission(s, salt) % 4)


def post_frame(K, S, cell, k, yaw):
    """PostSpot (ABRWorld::PlaceMission) : poteau k de la cellule, decalage Lateral(k) x post_lat, recule de post_back x S"""
    f = forward((0.0, 0.0, 0.0, yaw))
    step = (k + 1) // 2
    lat = (1.0 if k % 2 == 1 else -1.0) * step * K["Spacing"] * K["post_lat"]
    cx, cy = (cell[0] + 0.5) * S, (cell[1] + 0.5) * S
    return (cx - f[1] * lat - f[0] * S * K["post_back"], cy + f[0] * lat - f[1] * S * K["post_back"], 0.0, yaw)


def world_aim_box(fr, aim):
    """Boite visee (alignee : lacet au quart de tour) dans le monde : centre et demi-tailles"""
    c = aim_point(fr, aim)
    turn = int(round(fr[3] / 90.0)) % 2
    e = (aim[4], aim[3], aim[5]) if turn else (aim[3], aim[4], aim[5])
    return c, e


def first_hit(K, eye, target, devs):
    """ABRCharacter::UpdateFocus, etape 1 : sphere de FocusSweep des yeux vers le point vise, portee InteractReach : premier
    mecanisme dont la boite visee est touchee (la tolerance, etape 2, ne joue que si rien n'est touche)"""
    d = math.dist(eye, target)
    u = tuple((target[k] - eye[k]) / d for k in range(3))
    reach = K["InteractReach"]
    end = tuple(eye[k] + u[k] * reach for k in range(3))
    r = K["focus_sweep"]
    best = None
    for i, (c, e) in enumerate(devs):
        if seg_aabb(eye, end, c, e) > r:
            continue
        lo, hi = 0.0, 1.0
        if seg_aabb(eye, eye, c, e) > r:
            # premier contact (la distance decroit jusqu'au plus pres) : dichotomie sur le debut du segment
            def at(t):
                p = tuple(eye[k] + (end[k] - eye[k]) * t for k in range(3))
                return math.sqrt(sum(max(abs(p[k] - c[k]) - e[k], 0.0) ** 2 for k in range(3)))
            a, b = 0.0, 1.0
            for _ in range(60):
                m1, m2 = a + (b - a) / 3.0, b - (b - a) / 3.0
                if at(m1) <= at(m2):
                    b = m2
                else:
                    a = m1
            hi = (a + b) * 0.5
            for _ in range(60):
                m = (lo + hi) * 0.5
                if at(m) <= r:
                    hi = m
                else:
                    lo = m
        t = hi if seg_aabb(eye, eye, c, e) > r else 0.0
        if best is None or t < best[0]:
            best = (t, i)
    return None if best is None else best[1]


def mill_layout(K, S, cell, yaws, order, prints):
    out = []
    for k in range(3):
        fr = post_frame(K, S, cell, k, yaws[k])
        out.append((fr, prints[order[k]]))
    return out


def stolen(K, layout):
    """Places devant chaque mecanisme (cercles de FindMissionStand dans l'axe, yeux a portee de la visee du client) d'ou la
    visee prend un autre mecanisme : liste de (mecanisme, distance, mecanisme vise)"""
    boxes = [world_aim_box(fr, pr["aim"]) for fr, pr in layout]
    bad = []
    for i, (fr, pr) in enumerate(layout):
        ip = aim_point(fr, pr["aim"])
        f = forward(fr)
        for dist in K["rings"]:
            eye = (ip[0] + f[0] * dist, ip[1] + f[1] * dist, fr[2] + K["Clearance.StandEye"])
            if math.dist(eye, ip) > K["ClientFocusReach"]:
                continue
            hit = first_hit(K, eye, ip, boxes)
            if hit is not None and hit != i:
                bad.append((i, dist, hit))
    return bad


def case_level10(K, rules):
    S = K["L10"]["CellSize"]
    names = ("tableau des granges", "orientation du moulin", "frein du moulin")
    prints = [{"aim": K["plate"], "solid": None}, {"aim": K["mill"], "solid": None}, {"aim": K["button"], "solid": None}]
    lat = K["Spacing"] * K["post_lat"]
    print("E. Niveau 10 : groupe du moulin sur les poteaux d'une cellule de champ (cellule de %g cm ; poteaux a 0/+%.1f/-%.1f cm, recules de %g cm ; boites visees : tableau %s, "
          "moulin %s, frein %s ; visee du client : balayage de %g cm, portee %g ; places dans l'axe a %s cm, yeux a %g cm ; couloir : place a %g cm, ecart %g cm)"
          % (S, lat, lat, S * K["post_back"], tuple(K["plate"]), tuple(K["mill"]), tuple(K["button"]), K["focus_sweep"], K["InteractReach"],
             "/".join("%g" % r for r in K["rings"]), K["Clearance.StandEye"], K["Clearance.StandRange"], K["Clearance.FocusSweep"] + K["Clearance.Margin"]))
    seed, cell = 1697132605, (-1, -14)
    results = {}
    old_bad = old_front = 0
    missed = false_alarm = 0
    for y0 in range(4):
        for y1 in range(4):
            for y2 in range(4):
                lay = mill_layout(K, S, cell, (90.0 * y0, 90.0 * y1, 90.0 * y2), (0, 1, 2), prints)
                bad = stolen(K, lay)
                flagged = any(focus_clash(K, lay[a], lay[b]) for a in range(3) for b in range(a + 1, 3))
                # Place ordinaire (premier cercle de FindMissionStand, celle du couloir) ; plus loin, la place passe derriere un
                # voisin d'en face (pour information)
                front = any(dist == K["rings"][0] for _, dist, _ in bad)
                old_bad += 1 if bad else 0
                old_front += 1 if front else 0
                missed += 1 if front and not flagged else 0
                false_alarm += 1 if flagged and not front else 0
    yaws_old = tuple(post_yaw(seed, cell, 77 + k) for k in range(3))
    lay = mill_layout(K, S, cell, yaws_old, (0, 1, 2), prints)
    seed_bad = stolen(K, lay)
    if "old" in rules:
        results["old"] = old_front == 0
        print("  regle d'avant (un lacet tire par poteau, sels 77, 78, 79) : %d combinaisons de lacets sur 64 ou la visee prend un autre mecanisme depuis la place ordinaire a %g cm "
              "(%d depuis une place dans l'axe, jusqu'a %g cm) -> %s" % (old_front, K["rings"][0], old_bad, max(K["rings"]), "PASSE" if old_front == 0 else "ECHOUE"))
        pts = ", ".join("%s lacet %g point (%.0f, %.0f, %.0f)" % ((names[i], fr[3]) + aim_point(fr, pr["aim"])) for i, (fr, pr) in enumerate(lay))
        print("    graine %u, cellule %s : %s ; visee prise : %s"
              % (seed, cell, pts, "; ".join("%s a %g cm -> %s" % (names[i], d, names[h]) for i, d, h in seed_bad) or "aucune"))
    if "new" in rules:
        row_bad = row_cut = 0
        for yaw in (0.0, 90.0, 180.0, 270.0):
            for order in ((0, 1, 2), (0, 2, 1), (1, 0, 2), (1, 2, 0), (2, 0, 1), (2, 1, 0)):
                rl = mill_layout(K, S, cell, (yaw, yaw, yaw), order, prints)
                row_bad += 1 if stolen(K, rl) else 0
                row_cut += sum(1 for a in range(3) for b in range(a + 1, 3) if focus_clash(K, rl[a], rl[b]))
        ok = row_bad == 0 and row_cut == 0 and missed == 0
        results["new"] = ok
        yaw = post_yaw(seed, cell, 77)
        rl = mill_layout(K, S, cell, (yaw, yaw, yaw), (0, 1, 2), prints)
        print("  regle du jeu (poteaux en rang, couloirs de visee) : rang, 4 lacets x 6 ordres des roles : visee prise %d (toutes les places dans l'axe), couloirs coupes %d ; "
              "couloir sur les 64 combinaisons d'avant : %d fautive(s) depuis la place ordinaire non detectee(s), %d alerte(s) sans visee prise de la place ordinaire -> %s"
              % (row_bad, row_cut, missed, false_alarm, "PASSE" if ok else "ECHOUE"))
        print("    graine %u, cellule %s, rang au lacet %g : %s ; visee prise : %s"
              % (seed, cell, yaw, ", ".join("%s point (%.0f, %.0f, %.0f)" % ((names[i],) + aim_point(fr, pr["aim"])) for i, (fr, pr) in enumerate(rl)),
                 "; ".join("%s -> %s" % (names[i], names[h]) for i, _, h in stolen(K, rl)) or "aucune"))
    return results


# ------------------------------------------------------------------------------------- G. Niveau 37 : vannes des bassins
def np_dist_box(P, c, e):
    d = np.maximum(np.abs(P - c) - e, 0.0)
    return np.sqrt((d * d).sum(axis=-1))


def np_sweep(E, T, c, e, r):
    """Miroir de SweepContact (BRMechLogic.cpp), vectorise : premier contact (parametre 0..1) de la sphere de rayon r lancee de E
    vers T (n x 3) avec la boite alignee (c, e) ; 2 sans contact. Plus pres par recherche ternaire (48 pas), premier passage sous
    r par dichotomie (40 pas)"""
    D = T - E
    n = len(E)

    def at(t):
        return np_dist_box(E + D * t[:, None], c, e)
    lo, hi = np.zeros(n), np.ones(n)
    for _ in range(48):
        m1 = lo + (hi - lo) / 3.0
        m2 = hi - (hi - lo) / 3.0
        sel = at(m1) <= at(m2)
        hi = np.where(sel, m2, hi)
        lo = np.where(sel, lo, m1)
    first = (lo + hi) * 0.5
    first = np.where(at(np.ones(n)) < at(first), 1.0, first)
    hit = at(first) <= r
    prev = np.zeros(n)
    for _ in range(40):
        m = (prev + first) * 0.5
        inside = at(m) <= r
        first = np.where(inside, m, first)
        prev = np.where(inside, prev, m)
    return np.where(at(np.zeros(n)) <= r, 0.0, np.where(hit, first, 2.0))


def np_seg_box(E, T, c, e):
    """Miroir de SegToBox (BRMechLogic.cpp), vectorise : distance du segment E-T a la boite alignee (c, e)"""
    D = T - E
    n = len(E)

    def at(t):
        return np_dist_box(E + D * t[:, None], c, e)
    lo, hi = np.zeros(n), np.ones(n)
    for _ in range(48):
        m1 = lo + (hi - lo) / 3.0
        m2 = hi - (hi - lo) / 3.0
        sel = at(m1) <= at(m2)
        hi = np.where(sel, m2, hi)
        lo = np.where(sel, lo, m1)
    return np.minimum(np.minimum(at(np.zeros(n)), at(np.ones(n))), at((lo + hi) * 0.5))


def fan_points(K, steps=None):
    """Points de BRMech::Clearance::FanSteals depuis le point vise de Me (repere de Me) : places des cercles FanRings aux pas
    -FanTurns..FanTurns (ou steps), et grille de FanGrid dans le disque d'arrivee (FanArrivalFront pour la place devant, au
    premier cercle). Rend [(x, y, cercle, pas)]"""
    g = K["Clearance.FanGrid"]
    turns = K["Clearance.FanTurns"]
    out = []
    for ri, ring in enumerate(K["fan_rings"]):
        for step in (steps if steps is not None else range(-turns, turns + 1)):
            a = math.radians(step * K["Clearance.FanStep"])
            sx, sy = math.cos(a) * ring, math.sin(a) * ring
            arr = K["Clearance.FanArrivalFront"] if (ri == 0 and step == 0) else K["Clearance.FanArrival"]
            cells = int(arr / g)
            for gx in range(-cells, cells + 1):
                for gy in range(-cells, cells + 1):
                    ox, oy = gx * g, gy * g
                    if ox * ox + oy * oy <= arr * arr:
                        out.append((sx + ox, sy + oy, ri, step))
    return out


def fan_steals(K, me_box, other_box, me_solid, other_solid, lift, pts):
    """Miroir de BRMech::Clearance::FanSteals. Boites (centre, demi-tailles) dans le repere de Me, Z depuis son sol ; volumes :
    idem ou None. Capsule (PlayerRadius + Margin) hors des volumes plus hauts qu'une marche, yeux (StandEye au-dessus du sol
    de Me + lift) a portee (FanReach), ligne de vue de l'hote libre du volume de Other ; visee prise si la sphere (FocusSweep)
    lancee vers le point vise de Me touche la boite de Other au plus tard avec celle de Me, avant tout volume. Rend le masque"""
    tgt = np.array(me_box[0], float)
    P = np.array([(tgt[0] + p[0], tgt[1] + p[1]) for p in pts], float)
    ok = np.ones(len(P), bool)
    cap = K["Clearance.PlayerRadius"] + K["Clearance.Margin"]
    for s in (me_solid, other_solid):
        if s is None or s[1][2] * 2.0 <= K["Clearance.StepHeight"]:
            continue
        dx = np.maximum(np.abs(P[:, 0] - s[0][0]) - s[1][0], 0.0)
        dy = np.maximum(np.abs(P[:, 1] - s[0][1]) - s[1][1], 0.0)
        ok &= np.hypot(dx, dy) >= cap
    E = np.column_stack([P, np.full(len(P), lift + K["Clearance.StandEye"])])
    T = np.repeat(tgt[None, :], len(P), 0)
    ok &= np.linalg.norm(E - T, axis=1) <= K["Clearance.FanReach"]
    if other_solid is not None:
        ok &= np_seg_box(E, T, np.array(other_solid[0], float), np.array(other_solid[1], float)) > 0.0
    r = K["Clearance.FocusSweep"]
    t_me = np_sweep(E, T, tgt, np.array(me_box[1], float), r)
    t_o = np_sweep(E, T, np.array(other_box[0], float), np.array(other_box[1], float), r)
    t_s = np.full(len(P), 2.0)
    for s in (me_solid, other_solid):
        if s is not None:
            t_s = np.minimum(t_s, np_sweep(E, T, np.array(s[0], float), np.array(s[1], float), r))
    return ok & (t_o <= t_me) & (t_o < t_s), E - T


def pool_pair(K, lat_b, floor_b):
    """Repere de la vanne A (X sortant de l'avant des bassins, Y vers B, Z depuis le sol de A) : boites visees de A et de B (au
    sol floor_b), volume des bassins (BRMech::Clearance::Of, PoolTanks)"""
    s = K["sluice"]
    a_box = ((s[0], s[1], s[2]), (s[3], s[4], s[5]))
    b_box = ((s[0], lat_b + s[1], floor_b + s[2]), (s[3], s[4], s[5]))
    y0, y1 = K["Pool.AY"][0] - K["Pool.Wall"], K["Pool.BY"][1] + K["Pool.Wall"]
    td, top = K["Pool.TankDepth"], K["tank_top"]
    tanks = ((-td * 0.5, (y0 + y1) * 0.5, top * 0.5), (td * 0.5, (y1 - y0) * 0.5, top * 0.5))
    return a_box, b_box, tanks


def moved(box, dy, dz):
    """Boite du repere de A vers celui de B (origine a (0, lat_b), sol floor_b ; meme lacet)"""
    return ((box[0][0], box[0][1] - dy, box[0][2] - dz), box[1])


def pool_steals(K, lat_b, floor_b, lift, pts, target):
    a_box, b_box, tanks = pool_pair(K, lat_b, floor_b)
    if target == "A":
        return fan_steals(K, a_box, b_box, tanks, None, lift, pts)
    return fan_steals(K, moved(b_box, lat_b, floor_b), moved(a_box, lat_b, floor_b), None, moved(tanks, lat_b, floor_b), lift, pts)


def pool_bands(K, lat_b, drain, gauge_a, gauge_b):
    """Bandes laterales (repere de A) des pieces de l'avant des bassins : volants et leurs graduations (ABRMissionDevice::Init,
    R_Sluice : anneau de rayon R, graduations a R + 9 sur l'arc -120..120 degres), regard de la vidange (BuildPoolTanks : il
    descend ou monte avec le sol de la vanne, il ne doit croiser aucun volant quelle que soit sa hauteur), regles graduees"""
    n = K["sluice_positions"]
    lab = max(abs(math.sin(math.radians(-120.0 + i * 240.0 / max(1, n - 1)))) for i in range(n)) * (K["wheel_r"] + K["wheel_label_r"])
    wheel = max(K["wheel_r"] + K["wheel_ring_half"], lab + K["wheel_label_size"] * 0.3)
    half_w = K["window_d"] * 0.5
    g = K["gauge_w"] * 0.5
    return [("volant A et ses graduations", -wheel, wheel), ("volant B et ses graduations", lat_b - wheel, lat_b + wheel),
            ("regard de la vidange", drain - half_w, drain + half_w), ("regle de A", gauge_a - g, gauge_a + max(g, K["gauge_digit"])),
            ("regle de B", gauge_b - g, gauge_b + max(g, K["gauge_digit"]))]


def case_poolrooms(K, rules):
    """v4.16 : cas G : vannes A et B des Poolrooms, visee de biais (miroir de BRMech::Clearance::FanSteals) et pieces de l'avant
    des bassins"""
    old_lat, old_drain, old_gauge_b = 58.0, 96.0, 118.0
    new_lat = K["Pool.WheelLateralB"]
    l37 = K["L37"]
    floors = (0.0, l37["DeckStep"], l37["DeckHeight"], -l37["DeckStep"], -l37["DeckHeight"])
    pts = fan_points(K)
    print("G. Niveau 37 : vannes des bassins (boite visee R_Sluice %s ; bassins %g cm, Y %g..%g, haut %g ; places de biais : cercles %s, +-%d pas de %g degres, "
          "arrivees a %g cm (%g devant), grille de %g, portee %g ; visee du client : sphere de %g cm ; sols de B et du joueur decales de %s cm)"
          % (tuple(K["sluice"]), K["Pool.TankDepth"], K["Pool.AY"][0] - K["Pool.Wall"], K["Pool.BY"][1] + K["Pool.Wall"], K["tank_top"],
             "/".join("%g" % r for r in K["fan_rings"]), K["Clearance.FanTurns"], K["Clearance.FanStep"], K["Clearance.FanArrival"], K["Clearance.FanArrivalFront"],
             K["Clearance.FanGrid"], K["Clearance.FanReach"], K["Clearance.FocusSweep"], "/".join("%+g" % f for f in floors)))
    # Place de la graine 3053711763 (rapport du chapitre, phase 1) : place autour de A (la place devant refusee, "capsule genee"),
    # premiere place bonne au cercle de 160 cm a 45 degres du cote de B (arrivee a 18 cm environ, yeux a 169 cm du point de A
    # d'apres le rapport et la capture) ; la marche de reprise (but a 60 cm refuse, "capsule genee") revient au meme but : 0 m
    seen = [p for p in fan_points(K, steps=(2,)) if p[2] == 1]
    results = {}
    for key, label, lat, drain, gauge_b in (("old", "regle d'avant (v4.12 a v4.16)", old_lat, old_drain, old_gauge_b),
                                            ("new", "regle du jeu (v4.16)", new_lat, K["Pool.DrainLateral"], K["Pool.GaugeLateralB"])):
        if key not in rules:
            continue
        total = 0
        first = None
        worst = []
        for fb in floors:
            for lift in floors:
                for target in ("A", "B"):
                    mask, rel = pool_steals(K, lat, fb, lift, pts, target)
                    c = int(mask.sum())
                    total += c
                    if c and first is None:
                        k = int(np.argmax(mask))
                        first = (target, fb, lift, pts[k][2], pts[k][3], tuple(rel[k]))
                    if c:
                        worst.append((target, fb, lift, c))
        mask, rel = pool_steals(K, lat, 0.0, 0.0, seen, "A")
        seen_n = int(mask.sum())
        seen_txt = "aucune"
        if seen_n:
            k = int(np.argmax(mask))
            seen_txt = "yeux a (%.0f, %.0f, %.0f) du point de A, a %.0f degres de son avant" % (rel[k][0], rel[k][1], rel[k][2], math.degrees(math.atan2(rel[k][1], rel[k][0])))
        bands = pool_bands(K, lat, drain, K["Pool.GaugeLateralA"], gauge_b)
        clash_b = ["%s / %s" % (bands[i][0], bands[j][0]) for i in range(len(bands)) for j in range(i + 1, len(bands))
                   if bands[i][1] < bands[j][2] and bands[j][1] < bands[i][2]]
        lo_y, hi_y = K["Pool.AY"][0] - K["Pool.Wall"], K["Pool.BY"][1] + K["Pool.Wall"]
        outside = [b[0] for b in bands if b[1] < lo_y or b[2] > hi_y]
        gap = max(0.0, lat - 2.0 * K["sluice"][4])
        ok = total == 0 and seen_n == 0 and not clash_b and not outside
        results[key] = ok
        print("  %s : vanne B a %g cm de A (boites visees a %g cm), vidange a %g, regle de B a %g : visee prise depuis %d point(s) des places de biais et de leurs "
              "arrivees (2 cibles, %d sols) ; place de la graine 3053711763 (cercle de 160 cm, 45 degres vers B) : %d arrivee(s) sur %d ou B prend la visee de A "
              "(%s) ; reprise : meme but, meme disque ; avant des bassins : %s%s -> %s"
              % (label, lat, gap, drain, gauge_b, total, len(floors) * len(floors), seen_n, len(seen), seen_txt,
                 ("pieces qui se recouvrent : " + ", ".join(clash_b)) if clash_b else "pieces sans recouvrement",
                 (" ; hors de l'avant des bassins : " + ", ".join(outside)) if outside else "", "PASSE" if ok else "ECHOUE"))
        if first:
            print("    premier point : cible %s, sol de B %+g, joueur %+g, cercle %g, pas %+d, yeux a (%.0f, %.0f, %.0f) du point vise ; sols ou la visee est prise : %s"
                  % (first[0], first[1], first[2], K["fan_rings"][first[3]], first[4], first[5][0], first[5][1], first[5][2],
                     ", ".join("%s B%+g J%+g : %d" % w for w in worst[:6]) + (" ..." if len(worst) > 6 else "")))
        # Pour information : places plus de biais (67,5 degres : apres l'avant et les 45 degres de FindMissionStand du meme cote)
        graze = fan_points(K, steps=(-3, 3))
        g_total = sum(int(pool_steals(K, lat, fb, lift, graze, t)[0].sum()) for fb in floors for lift in floors for t in ("A", "B"))
        print("    pour information : places a 67,5 degres et leurs arrivees : visee prise depuis %d point(s) (l'autre volant tout pres de la ligne du regard)" % g_total)
    return results


# ------------------------------------------------------------------- H. v4.16 (visee) : choix de la visee entre voisins
def np_pick_aim(K, E, look, shapes, centres, rule):
    """Miroir de BRMech::Clearance::PickAim (BRMechLogic.cpp ; le jeu : ABRCharacter::PickAimedDevice), vectorise sur les yeux E
    (n x 3) : sphere de FocusSweep lancee de E vers look (n x 3). shapes : [(centre, demi-tailles, proprietaire, boite visee)]
    (proprietaire -1 : decor ; a egalite de contact, l'ordre de la liste) ; centres : points vises des mecanismes. rule "first" :
    proprietaire du premier contact ; "nearest" (regle du jeu) : le balayage traverse les boites visees jusqu'au premier volume ou
    decor, au plus MaxAimCandidates objets ; le point vise le plus pres de l'axe du regard (cosinus) l'emporte, un autre que le
    premier touche seulement si l'hote l'accepte (portee HostReach, ligne de vue a Margin pres de tout volume sauf le sien).
    Rend le mecanisme pris par yeux, -1 si rien"""
    n = len(E)
    r = K["Clearance.FocusSweep"]
    T = np.stack([np_sweep(E, look, np.array(s[0], float), np.array(s[1], float), r) for s in shapes], axis=1)
    order = np.argsort(T, axis=1, kind="stable")
    hit = np.take_along_axis(T, order, axis=1) <= 1.0
    owners = np.array([s[2] for s in shapes])[order]
    aim = np.array([bool(s[3]) for s in shapes])[order]
    nd = len(centres)
    is_dev = hit & (owners >= 0) & (owners < nd)
    best = np.where(is_dev[:, 0], owners[:, 0], -1)
    if rule == "first":
        return best
    C = [np.broadcast_to(np.array(c, float), (n, 3)) for c in centres]
    L = look - E
    ll = np.linalg.norm(L, axis=1)

    def cosine(d):
        V = C[d] - E
        vl = np.linalg.norm(V, axis=1)
        ok = (ll > 0) & (vl > 0)
        return np.where(ok, (V * L).sum(axis=1) / np.where(ok, vl * ll, 1.0), -1.0)

    def host(d):
        ok = np.linalg.norm(C[d] - E, axis=1) <= K["Clearance.HostReach"]
        for s in shapes:
            if not s[3] and s[2] != d:
                ok &= np_seg_box(E, C[d], np.array(s[0], float), np.array(s[1], float)) >= K["Clearance.Margin"]
        return ok
    cos = np.stack([cosine(d) for d in range(nd)], axis=1)
    hosts = np.stack([host(d) for d in range(nd)], axis=1)
    rows = np.arange(n)
    going = is_dev[:, 0] & aim[:, 0]
    best_cos = np.where(best >= 0, cos[rows, np.maximum(best, 0)], -2.0)
    for k in range(1, min(len(shapes), K["Clearance.MaxAimCandidates"])):
        own = owners[:, k]
        going &= is_dev[:, k]
        oc = np.maximum(own, 0)
        take = going & (own != best) & (cos[rows, oc] > best_cos) & hosts[rows, oc]
        best = np.where(take, own, best)
        best_cos = np.where(take, cos[rows, oc], best_cos)
        going &= aim[:, k]
    return best


def fan_steals_picked(K, me_box, other_box, me_solid, other_solid, lift, pts):
    """Miroir de BRMech::Clearance::FanStealsPicked : les places, arrivees et filtres de fan_steals (capsule, portee, ligne de vue
    de l'hote libre du volume de Other), puis la regle du jeu (np_pick_aim, "nearest") : pieces de Other d'abord (a egalite de
    contact, Other est le premier touche) ; visee prise si elle rend Other. Rend le masque"""
    tgt = np.array(me_box[0], float)
    P = np.array([(tgt[0] + p[0], tgt[1] + p[1]) for p in pts], float)
    ok = np.ones(len(P), bool)
    cap = K["Clearance.PlayerRadius"] + K["Clearance.Margin"]
    for s in (me_solid, other_solid):
        if s is None or s[1][2] * 2.0 <= K["Clearance.StepHeight"]:
            continue
        dx = np.maximum(np.abs(P[:, 0] - s[0][0]) - s[1][0], 0.0)
        dy = np.maximum(np.abs(P[:, 1] - s[0][1]) - s[1][1], 0.0)
        ok &= np.hypot(dx, dy) >= cap
    E = np.column_stack([P, np.full(len(P), lift + K["Clearance.StandEye"])])
    T = np.repeat(tgt[None, :], len(P), 0)
    ok &= np.linalg.norm(E - T, axis=1) <= K["Clearance.FanReach"]
    if other_solid is not None:
        ok &= np_seg_box(E, T, np.array(other_solid[0], float), np.array(other_solid[1], float)) > 0.0
    shapes = [(other_box[0], other_box[1], 1, True)] + ([(other_solid[0], other_solid[1], 1, False)] if other_solid is not None else [])
    shapes += [(me_box[0], me_box[1], 0, True)] + ([(me_solid[0], me_solid[1], 0, False)] if me_solid is not None else [])
    picked = np_pick_aim(K, E, T, shapes, [tuple(tgt), tuple(other_box[0])], "nearest")
    return ok & (picked == 1), E - T


def pool_steals_by(K, fn, lat_b, floor_b, lift, pts, target):
    a_box, b_box, tanks = pool_pair(K, lat_b, floor_b)
    if target == "A":
        return fn(K, a_box, b_box, tanks, None, lift, pts)
    return fn(K, moved(b_box, lat_b, floor_b), moved(a_box, lat_b, floor_b), None, moved(tanks, lat_b, floor_b), lift, pts)


def hidden_scenes(K, rule):
    """Scenes du banc Tools/Mechanisms (section K, 4) rejouees par le miroir : regard selon -X depuis (200, 0, 120) ; boite visee
    de Me frolee (X -2 a 18, Y 6 a 26), celle de Other plus loin (X -50 a -30), son point vise dans l'axe. Rend (a decouvert,
    derriere un mur, derriere le volume du premier touche, point vise derriere une dalle, hors de portee de l'hote)"""
    me = ((8.0, 16.0, 120.0), (10.0, 10.0, 25.0), 0, True)
    other = ((-40.0, 0.0, 120.0), (10.0, 22.0, 25.0), 1, True)
    centres = [me[0], other[0]]
    slab_e = (1.0, 60.0, 100.0)
    far = np.array([[-200.0, 0.0, 120.0]])

    def pick(extra, eye):
        shapes = [me, other] + ([] if extra is None else [extra])
        return int(np_pick_aim(K, np.array([eye], float), far, shapes, centres, rule)[0])
    eye = (200.0, 0.0, 120.0)
    return (pick(None, eye), pick(((-10.0, 0.0, 120.0), slab_e, -1, False), eye), pick(((-10.0, 0.0, 120.0), slab_e, 0, False), eye),
            pick(((-34.0, 0.0, 120.0), slab_e, -1, False), eye), pick(None, (305.0, 0.0, 120.0)))


def case_aim_pick(K, rules):
    """v4.16 (visee) : cas H : choix de la visee du client entre voisins, juge sur le defaut d'origine (Niveau 37, vanne B a 58 cm
    de la vanne A, boites visees chevauchees de 6 cm : graine 3053711763, "PAS VISE") : regle d'avant (premier contact,
    fan_steals) ECHOUE ; regle du jeu (np_pick_aim, "nearest") : aucune visee prise sur tous les sols decales, au placement d'avant
    comme a celui du jeu (98 cm), et jamais un mecanisme cache (scenes de la section K du banc)"""
    l37 = K["L37"]
    floors = (0.0, l37["DeckStep"], l37["DeckHeight"], -l37["DeckStep"], -l37["DeckHeight"])
    pts = fan_points(K)
    seen = [p for p in fan_points(K, steps=(2,)) if p[2] == 1]
    print("H. v4.16 (visee) : choix de la visee du client entre voisins (ABRCharacter::PickAimedDevice, miroir de BRMech::Clearance::PickAim : boites visees "
          "traversees jusqu'au premier volume, au plus %d objets ; point vise le plus pres de l'axe du regard ; un voisin seulement si l'hote l'accepte : portee %g, "
          "ligne de vue a %g cm pres) ; places de biais et arrivees du cas G, vannes du Niveau 37"
          % (K["Clearance.MaxAimCandidates"], K["Clearance.HostReach"], K["Clearance.Margin"]))
    results = {}
    for key, label, fn, rule in (("old", "regle d'avant (premier contact, v4.12 a c1c2bde)", fan_steals, "first"),
                                 ("new", "regle du jeu (v4.16 : point vise le plus pres de l'axe)", fan_steals_picked, "nearest")):
        if key not in rules:
            continue
        counts = {}
        for lat in (58.0, K["Pool.WheelLateralB"]):
            counts[lat] = sum(int(pool_steals_by(K, fn, lat, fb, lift, pts, t)[0].sum()) for fb in floors for lift in floors for t in ("A", "B"))
        seen_n = int(pool_steals_by(K, fn, 58.0, 0.0, 0.0, seen, "A")[0].sum())
        sc = hidden_scenes(K, rule)
        hidden_ok = sc[1] != 1 and sc[2] != 1 and sc[3] != 1 and sc[4] != 1
        ok = counts[58.0] == 0 and seen_n == 0 and hidden_ok and (sc[0] == 1 if key == "new" else True)
        results[key] = ok
        print("  %s : vanne B a 58 cm (placement d'avant) : visee prise depuis %d point(s) (2 cibles, %d sols) ; place de la graine 3053711763 (cercle de 160 cm, "
              "45 degres vers B) : %d arrivee(s) sur %d ; vanne B a %g cm (placement du jeu) : %d ; scenes du banc (a decouvert, mur, volume du premier touche, "
              "dalle, hors de portee) : %s -> %s"
              % (label, counts[58.0], len(floors) * len(floors), seen_n, len(seen), K["Pool.WheelLateralB"], counts[K["Pool.WheelLateralB"]],
                 "/".join("%d" % v for v in sc), "PASSE" if ok else "ECHOUE"))
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=120, help="graines par niveau (grottes, banlieue)")
    ap.add_argument("--rules", default="both", choices=("both", "old", "new"))
    a = ap.parse_args()
    K = read_constants()
    rock = fbx_bounds(os.path.join(ROOT, "RawAssets", "Meshes", "SM_Rock.fbx"))
    house = fbx_bounds(os.path.join(ROOT, "RawAssets", "Meshes", "SM_House.fbx"))
    rules = ("old", "new") if a.rules == "both" else (a.rules,)
    print("Banc des places des mecanismes (v4.15, Tools/verify_device_clearance.py)")
    print("Constantes lues : capsule %g x %g, yeux a %g cm au-dessus du centre, portee de l'hote %g cm ; marge %g, place debout a %g cm, profondeur libre %g ; "
          "poste %s ; treuil %s ; plaque %s"
          % (K["Capsule"][0], K["Capsule"][1], K["EyeZ"], K["Reach"], K["Clearance.Margin"], K["Clearance.StandGap"], K["Clearance.StandDepth"],
             tuple(K["post"][:3]), tuple(K["winch"][:3]), tuple(K["plate"][:3])))
    if K["Capsule"][0] != K["Clearance.PlayerRadius"]:
        print("ECHEC : BRMech::Clearance::PlayerRadius (%g) differe de la capsule de ABRCharacter (%g)" % (K["Clearance.PlayerRadius"], K["Capsule"][0]))
        return 1
    # v4.15 (correctifs 3) : le couloir de visee du placement suit le personnage et les tests
    if K["Capsule"][1] + K["EyeZ"] != K["Clearance.StandEye"] or K["rings"][0] != K["Clearance.StandRange"] or K["focus_sweep"] != K["Clearance.FocusSweep"]:
        print("ECHEC : couloir de visee (BRMech::Clearance : StandEye %g, StandRange %g, FocusSweep %g) different du personnage (yeux %g) ou des tests (premier cercle %g, "
              "balayage %g)" % (K["Clearance.StandEye"], K["Clearance.StandRange"], K["Clearance.FocusSweep"], K["Capsule"][1] + K["EyeZ"], K["rings"][0], K["focus_sweep"]))
        return 1
    print()
    res = {"A": case_level6(K, rules)}
    print()
    res["B"] = case_level8(K, rules, rock, a.seeds)
    print()
    res["C"] = case_level9(K, rules, house, a.seeds)
    print()
    res["D"] = case_level8_exits(K, rules, rock, a.seeds)
    print()
    res["E"] = case_level10(K, rules)
    print()
    res["F"] = case_level8_chunk_exits(K, rules, rock, a.seeds)
    print()
    res["G"] = case_poolrooms(K, rules)
    print()
    res["H"] = case_aim_pick(K, rules)
    print()
    print("Regle du jeu dans les sources :")
    for what, ok in list(K["src_A"].items()) + list(K["src_D"].items()) + list(K["src_E"].items()) + list(K["src_F"].items()) + list(K["src_G"].items()) + list(K["src_H"].items()):
        print("  %s : %s" % (what, "oui" if ok else "NON"))
    print()
    bad = 0
    for case, r in res.items():
        if "old" in r and r["old"]:
            print("ECHEC : cas %s : l'ancienne regle passe (le defaut n'est plus reproduit par le banc)" % case)
            bad += 1
        if "new" in r and not r["new"]:
            print("ECHEC : cas %s : la regle du jeu echoue" % case)
            bad += 1
    for case, checks in (("A", K["src_A"]), ("D", K["src_D"]), ("E", K["src_E"]), ("F", K["src_F"]), ("G", K["src_G"]), ("H", K["src_H"])):
        if "new" in rules and not all(checks.values()):
            print("ECHEC : cas %s : la regle v4.15 n'est plus dans les sources (%s)" % (case, ", ".join(w for w, ok in checks.items() if not ok)))
            bad += 1
    if "new" in rules and not K["rock_keepout_in_game"]:
        print("ECHEC : BRChunk.cpp ne retire pas les rochers de la zone libre des mecanismes (ou ne tire pas les rochers par BRMech::Cave)")
        bad += 1
    if "new" in rules and not K["house_body"]:
        print("ECHEC : BRChunk.cpp n'a pas la boite du corps de la maison (GHouseBodyFront)")
        bad += 1
    word = lambda r, k: ("passe" if r[k] else "echoue") if k in r else "non jouee"
    for r in res.values():
        r.pop("first", None)  # premiere disposition v4.15 du cas A : pour information
    fmt = {"G": "%s avant (vanne B a 58 cm) %s / v4.16 %s", "H": "%s visee au premier contact %s / v4.16 (point vise le plus pres de l'axe) %s"}
    print("RESULTAT : %s (%s)" % ("OK" if bad == 0 else "ECHEC", ", ".join(fmt.get(c, "%s v4.14 %s / v4.15 %s") % (c, word(r, "old"), word(r, "new"))
                                                                     for c, r in res.items())))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
