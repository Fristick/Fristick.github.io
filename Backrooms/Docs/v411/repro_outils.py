"""Reproduction des constats 2.4 et 2.5 sur un check_package.py donne (v4.10 ou v4.11).
Usage : python repro_tools.py <check_package.py> <dossier des tests>"""
import contextlib
import importlib.util
import io
import json
import os
import shutil
import struct
import sys
import tempfile

mod_path, tests_dir = sys.argv[1], sys.argv[2]
spec = importlib.util.spec_from_file_location("cp", mod_path)
cp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cp)
sys.path.insert(0, tests_dir)
import test_packaging as tp  # noqa: E402

tp.check_package = cp


def run(argv):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        code = cp.main(argv)
    return code, [l.strip() for l in buf.getvalue().splitlines() if "RESULTAT" in l or l.strip().startswith("lancement")]


# 2.5 : PE de 68 octets (MZ, pointeur a 64, PE\0\0)
d = bytearray(68)
d[0:2] = b"MZ"
struct.pack_into("<I", d, 0x3C, 64)
d[64:68] = b"PE\0\0"
tmp = tempfile.mkdtemp()
f = os.path.join(tmp, "x.exe")
with open(f, "wb") as fh:
    fh.write(bytes(d))
try:
    print("2.5 pe_info(68 octets) ->", cp.pe_info(f))
except Exception as e:  # noqa: BLE001
    print("2.5 pe_info(68 octets) -> exception %s: %s" % (type(e).__name__, e))
if hasattr(cp, "binary_info"):
    print("2.5 binary_info(pe_info, 68 octets) ->", cp.binary_info(cp.pe_info, f))

# 2.4 : lancement simule par le Backrooms.sh du paquet (hote Linux), rapport au schema du jeu
CASES = [
    ("--launch, code 1, etat echec", ["--launch"], 1, {"etat": "echec", "jeu": "v", "problemes": ["x"], "non_verifies": []}),
    ("--require-launch, code 0, etat incomplet", ["--require-launch"], 0, {"etat": "incomplet", "jeu": "v", "problemes": [], "non_verifies": ["y"]}),
    ("--require-launch, code 0, rapport {}", ["--require-launch"], 0, {}),
]
SCRIPT = """#!/bin/sh
for a in "$@"; do case $a in -BRSmokeOut=*) out=${a#-BRSmokeOut=};; esac; done
python3 - "$out" "$*" <<'EOF'
import json, sys
rep = json.loads(%r)
if rep:
    rep["ligne_de_commande"] = sys.argv[2]
open(sys.argv[1], "w").write(json.dumps(rep))
EOF
exit %d
"""
for label, flags, game_code, rep in CASES:
    t = tempfile.mkdtemp()
    fx = tp.Fixture(t)
    archive = fx.make("Linux")
    launcher = os.path.join(archive, "Linux", "Backrooms.sh")
    with open(launcher, "w") as fh:
        fh.write(SCRIPT % (json.dumps(rep), game_code))
    os.chmod(launcher, 0o755)
    code, lines = run(["--platform", "Linux", "--dir", archive, "--project", fx.project, "--unrealpak", fx.tool] + flags)
    print("2.4 %s -> code %s | %s" % (label, code, " / ".join(lines)))
    shutil.rmtree(t, ignore_errors=True)
