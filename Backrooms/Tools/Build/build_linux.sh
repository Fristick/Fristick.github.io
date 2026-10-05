#!/usr/bin/env bash
# v4.9 : construction, cuisson et paquet Linux x86_64 natif (Vulkan).
#   UE_ROOT=/opt/UnrealEngine-5.8 Tools/Build/build_linux.sh [Shipping|Development]
# Sur une machine Linux avec Unreal 5.8 (moteur installe ou compile, avec la chaine clang fournie par Epic).
# Depuis Windows, la construction croisee passe par build_windows.ps1 -Platform Linux (LINUX_MULTIARCH_ROOT).
# Resultat : Build/Linux/<Config>/Linux/ (Backrooms.sh, binaires, paks) et build_manifest.txt.
source "$(dirname "$0")/common.sh"
CONFIG="${1:-Shipping}"
br_check_engine
OUT="$BR_PROJECT_DIR/Build/Linux/$CONFIG"
TOOLCHAIN="$( (clang --version 2>/dev/null || echo 'clang non trouve dans le PATH (chaine du moteur)') | head -1)"
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$BR_PROJECT" -noP4 -utf8output \
	-platform=Linux -clientconfig="$CONFIG" -build -cook -stage -pak -archive -archivedirectory="$OUT"
br_write_manifest "$OUT" "Linux x86_64 (Vulkan)" "$CONFIG" "$TOOLCHAIN"
python3 "$BR_TOOLS/check_package.py" --platform Linux --dir "$OUT" --ue "$UE_ROOT"
