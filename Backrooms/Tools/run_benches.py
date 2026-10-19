"""v4.16 : les bancs C++ hors moteur, compiles avec chaque compilateur disponible (regles strictes) puis executes.

Un succes sous un seul compilateur ne couvre pas les autres (v4.16 : test_departure passait sous clang-cl et MSVC mais
echouait sous GCC -Wshadow). Ce script compile chaque banc avec :
  - gcc     : g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow
  - clang   : clang++ (memes options que g++)
  - clang-cl: clang-cl /std:c++17 /O2 /EHsc /utf-8 /W4 /WX -Wshadow -Wno-c++98-compat (Windows)
  - msvc    : cl /std:c++17 /O2 /EHsc /utf-8 /W4 /WX /permissive- (Windows ; environnement vcvars64 trouve par vswhere)
execute chaque binaire depuis Backrooms/ et compare les sorties entre compilateurs (meme resultat attendu).

  python Tools/run_benches.py                       tous les compilateurs trouves, tous les bancs
  python Tools/run_benches.py --compilers gcc,msvc  seulement ceux-la (absent : erreur)
  python Tools/run_benches.py --bench departure     un banc
  python Tools/run_benches.py --out <dossier>       binaires et journaux (defaut : Saved/Benches)
Code de sortie : 0 si chaque banc compile sans avertissement et reussit avec chaque compilateur, et si les sorties
concordent ; 1 sinon ; 2 si un compilateur demande est absent.
"""
import argparse
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
P = "Source/Backrooms/Private/"
BENCHES = [
    ("story", ["Tools/Story/test_story.cpp", P + "BRStoryLogic.cpp", P + "BRContentLogic.cpp"]),
    ("lidar", ["Tools/Lidar/test_lidar.cpp", P + "BRLidarLogic.cpp"]),
    ("content", ["Tools/Content/test_content.cpp", P + "BRContentLogic.cpp", P + "BRMissionLogic.cpp", P + "BRStoryLogic.cpp"]),
    ("departure", ["Tools/Departure/test_departure.cpp", P + "BRGatherLogic.cpp", P + "BRMechLogic.cpp", P + "BRMissionLogic.cpp"]),
    ("light", ["Tools/Light/test_light.cpp"]),
    ("color", ["Tools/Light/test_color.cpp"]),
    ("fluo", ["Tools/Light/test_fluo.cpp"]),
    ("mechanisms", ["Tools/Mechanisms/test_mechanisms.cpp", P + "BRMechLogic.cpp", P + "BRMissionLogic.cpp"]),
    ("missions", ["Tools/Missions/test_mission_logic.cpp", P + "BRMissionLogic.cpp", P + "BRMissionSolver.cpp"]),
    ("transactions", ["Tools/Transactions/test_transactions.cpp", P + "BRTxnLogic.cpp"]),
    ("perf", ["Tools/Perf/test_perf.cpp"]),  # v4.16 (perf) : logique des mesures de performance (BRPerfLogic.h)
]
GNU_FLAGS = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-Wshadow", "-I", "Source/Backrooms/Public"]
CLANG_CL_FLAGS = ["/nologo", "/std:c++17", "/O2", "/EHsc", "/utf-8", "/W4", "/WX", "-Wshadow", "-Wno-c++98-compat",
                  "/I", "Source\\Backrooms\\Public"]
MSVC_FLAGS = ["/nologo", "/std:c++17", "/O2", "/EHsc", "/utf-8", "/W4", "/WX", "/permissive-", "/I", "Source\\Backrooms\\Public"]


def find_vcvars():
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"), "Microsoft Visual Studio", "Installer",
                           "vswhere.exe")
    if not os.path.isfile(vswhere):
        return None
    try:
        path = subprocess.run([vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                               "-property", "installationPath"], capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    vcvars = os.path.join(path, "VC", "Auxiliary", "Build", "vcvars64.bat") if path else ""
    return vcvars if os.path.isfile(vcvars) else None


def find_clang_cl(vcvars):
    exe = shutil.which("clang-cl")
    if exe:
        return exe
    if vcvars:
        cand = os.path.join(os.path.dirname(vcvars), "..", "..", "Tools", "Llvm", "x64", "bin", "clang-cl.exe")
        if os.path.isfile(cand):
            return os.path.normpath(cand)
    return None


def find_gpp():
    exe = shutil.which("g++")
    if exe:
        return exe
    # Windows : WinLibs installe par winget (BrechtSanders.WinLibs.*) hors du PATH de la session
    base = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Microsoft", "WinGet", "Packages")
    if os.path.isdir(base):
        for d in sorted(os.listdir(base)):
            cand = os.path.join(base, d, "mingw64", "bin", "g++.exe")
            if d.startswith("BrechtSanders.WinLibs") and os.path.isfile(cand):
                return cand
    return None


def compilers():
    """{nom: (fonction de commande (sources, sortie) -> (argv, shell), description)}"""
    found = {}
    gpp = find_gpp()
    if gpp:
        # Windows (MinGW) : liaison statique. Sinon le binaire charge le premier libstdc++-6.dll du PATH (celui d'un autre
        # logiciel) : incompatible, il plantait des l'utilisation de std::regex (0xC0000005) sans que le banc soit en cause
        link = ["-static"] if os.name == "nt" else []
        found["gcc"] = (lambda srcs, exe, c=gpp: ([c] + GNU_FLAGS + srcs + ["-o", exe] + link, False), gpp)
    clangpp = shutil.which("clang++")
    if clangpp:
        found["clang"] = (lambda srcs, exe, c=clangpp: ([c] + GNU_FLAGS + srcs + ["-o", exe], False), clangpp)
    if os.name == "nt":
        vcvars = find_vcvars()
        clang_cl = find_clang_cl(vcvars)
        if clang_cl:
            found["clang-cl"] = (lambda srcs, exe, c=clang_cl: ([c] + CLANG_CL_FLAGS + srcs + ["/Fe" + exe, "/Fo" + os.path.dirname(exe) + "\\"],
                                                                False), clang_cl)
        if vcvars:
            def msvc(srcs, exe, v=vcvars):
                cmd = " ".join(["cl"] + MSVC_FLAGS + srcs + ['/Fe"%s"' % exe, '/Fo"%s\\\\"' % os.path.dirname(exe)])
                return ('call "%s" >nul && %s' % (v, cmd), True)
            found["msvc"] = (msvc, "cl (" + vcvars + ")")
    return found


def version(name, desc):
    try:
        if name in ("gcc", "clang"):
            return subprocess.run([desc, "--version"], capture_output=True, text=True).stdout.splitlines()[0]
        if name == "clang-cl":
            return subprocess.run([desc, "--version"], capture_output=True, text=True).stdout.splitlines()[0]
    except (OSError, IndexError):
        pass
    return desc


def main():
    ap = argparse.ArgumentParser(description="Bancs C++ hors moteur avec chaque compilateur")
    ap.add_argument("--compilers", default="")
    ap.add_argument("--bench", default="")
    ap.add_argument("--out", default=os.path.join(ROOT, "Saved", "Benches"))
    args = ap.parse_args()
    avail = compilers()
    wanted = [c for c in args.compilers.split(",") if c] if args.compilers else sorted(avail)
    missing = [c for c in wanted if c not in avail]
    if missing:
        print("Compilateur(s) absent(s) : %s (trouves : %s)" % (", ".join(missing), ", ".join(sorted(avail)) or "aucun"))
        return 2
    benches = [b for b in BENCHES if not args.bench or b[0] == args.bench]
    if not benches:
        print("Banc inconnu : %s" % args.bench)
        return 2
    print("Bancs hors moteur depuis %s" % ROOT)
    for c in wanted:
        print("  %-8s %s" % (c, version(c, avail[c][1])))
    failures = 0
    for name, srcs in benches:
        outputs = {}
        for c in wanted:
            d = os.path.join(args.out, c)
            os.makedirs(d, exist_ok=True)
            exe = os.path.join(d, "test_%s.exe" % name if os.name == "nt" else "test_%s" % name)
            argv, shell = avail[c][0]([s.replace("/", "\\") if c in ("clang-cl", "msvc") else s for s in srcs], exe)
            b = subprocess.run(argv, cwd=ROOT, shell=shell, capture_output=True, text=True, encoding="utf-8", errors="replace")
            log = (b.stdout or "") + (b.stderr or "")
            with open(os.path.join(d, "%s_build.txt" % name), "w", encoding="utf-8") as f:
                f.write(log)
            warn = sum(1 for line in log.splitlines() if "warning" in line.lower() and "warning(s)" not in line.lower())
            if b.returncode != 0 or warn:
                failures += 1
                first = next((line for line in log.splitlines() if "error" in line.lower() or "warning" in line.lower()), "")
                print("  %-13s %-8s COMPILATION ECHOUEE (code %d, %d avertissement(s)) %s" % (name, c, b.returncode, warn, first.strip()[:140]))
                continue
            r = subprocess.run([exe], cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
            out = r.stdout or ""
            with open(os.path.join(d, "%s_run.txt" % name), "w", encoding="utf-8") as f:
                f.write(out + (r.stderr or ""))
            outputs[c] = out
            last = out.strip().splitlines()[-1] if out.strip() else "(aucune sortie)"
            ok = r.returncode == 0
            failures += 0 if ok else 1
            print("  %-13s %-8s %s %s" % (name, c, "OK   " if ok else "ECHEC", last[:110]))
        if len(set(outputs.values())) > 1:
            failures += 1
            print("  %-13s sorties DIFFERENTES selon le compilateur (%s)" % (name, ", ".join(sorted(outputs))))
    print("RESULTAT : %s (%d banc(s) x %d compilateur(s))" % ("OK" if failures == 0 else "%d ECHEC(S)" % failures, len(benches), len(wanted)))
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
