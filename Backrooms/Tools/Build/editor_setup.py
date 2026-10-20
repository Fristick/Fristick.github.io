"""v4.17 : import des ressources sans surveillance dans l'editeur, puis fermeture (reprend Saved/TestRuns/setup_content.py
de la v4.16, hors depot).

Lance par Tools/Build/run_editor_setup.ps1 :
  UnrealEditor.exe Backrooms.uproject -ExecutePythonScript=<ce fichier> -unattended -nosplash -log -abslog=<journal>
Variables d'environnement :
  BR_SETUP_FORCE=1   backrooms_setup.run(force=True) (tout est reimporte : textures, modeles, materiaux, kit du Level 0)
  BR_SETUP_KIT=1     import_level0_kit(force=True) apres run() (FBX du kit regeneres)
  BR_SETUP_OUT=<f>   fichier du resultat (defaut : Saved/BackroomsSetup_Result.txt)
Resultat : RESULT=OK|PROBLEMS|EXCEPTION, la liste des problemes de backrooms_setup.validate(), la duree.
"""
import os
import time
import traceback

import unreal

OUT = os.environ.get("BR_SETUP_OUT") or os.path.join(unreal.Paths.project_saved_dir(), "BackroomsSetup_Result.txt")


def _write(text):
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)


start = time.time()
try:
    import backrooms_setup
    backrooms_setup.run(force=os.environ.get("BR_SETUP_FORCE") == "1")
    if os.environ.get("BR_SETUP_KIT") == "1":
        backrooms_setup.import_level0_kit(force=True)
    problems = backrooms_setup.validate(verbose=True)
    _write("RESULT=%s\nproblems=%r\nmaterial_version=%d\nseconds=%.0f\n" % (
        "OK" if not problems else "PROBLEMS", problems, backrooms_setup.MATERIAL_VERSION, time.time() - start))
    unreal.log("[BRSETUP] fini, problemes=%r" % (problems,))
except Exception:
    _write("RESULT=EXCEPTION\n" + traceback.format_exc())
    unreal.log_error("[BRSETUP] " + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
