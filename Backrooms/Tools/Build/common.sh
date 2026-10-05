#!/usr/bin/env bash
# v4.9 : fonctions communes aux scripts de construction Linux et macOS.
set -euo pipefail

BR_TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BR_PROJECT_DIR="$(cd "$BR_TOOLS/../.." && pwd)"
BR_PROJECT="$BR_PROJECT_DIR/Backrooms.uproject"
# shellcheck disable=SC1091
source "$BR_TOOLS/versions.env"

br_usage_engine() {
	echo "Chemin du moteur manquant : UE_ROOT=/chemin/vers/UE_5.8 $0 [Shipping|Development]" >&2
	exit 2
}

# Version installee (Engine/Build/Build.version) comparee a UE_VERSION
br_check_engine() {
	[ -n "${UE_ROOT:-}" ] || br_usage_engine
	local ver="$UE_ROOT/Engine/Build/Build.version"
	[ -f "$ver" ] || { echo "Moteur introuvable : $ver" >&2; exit 2; }
	BR_UE_FOUND=$(python3 -c "import json,sys;d=json.load(open(sys.argv[1]));print('%d.%d.%d'%(d['MajorVersion'],d['MinorVersion'],d['PatchVersion']))" "$ver")
	if [ "$BR_UE_FOUND" != "$UE_VERSION" ] && [ "${BR_ALLOW_OTHER_UE:-0}" != "1" ]; then
		echo "Moteur $BR_UE_FOUND installe, $UE_VERSION attendu (Tools/Build/versions.env). BR_ALLOW_OTHER_UE=1 pour continuer." >&2
		exit 3
	fi
	echo "Moteur : $BR_UE_FOUND ($UE_ROOT)"
}

# Manifeste de la construction : versions, commit, date, plateforme (reproductibilite)
br_write_manifest() {
	local out="$1" platform="$2" config="$3" toolchain="$4"
	mkdir -p "$out"
	{
		echo "projet : Backrooms"
		echo "commit : $(git -C "$BR_PROJECT_DIR" rev-parse HEAD 2>/dev/null || echo inconnu)"
		echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"
		echo "plateforme : $platform"
		echo "configuration : $config"
		echo "moteur : $BR_UE_FOUND"
		echo "chaine de compilation : $toolchain"
		echo "hote : $(uname -srm)"
	} > "$out/build_manifest.txt"
}
