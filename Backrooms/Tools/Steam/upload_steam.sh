#!/usr/bin/env bash
# v4.9 : envoi SteamPipe avec steamcmd (prepare, non execute : aucun acces Steamworks pendant la preparation).
#   STEAM_BUILD_USER=<compte de build> Tools/Steam/upload_steam.sh
# Utilise Build/SteamPipe/app_build.vdf ecrit par make_steam_vdf.py (apercu par defaut : rien n'est envoye).
# Le mot de passe et le code Steam Guard sont demandes par steamcmd ; ils ne sont jamais ecrits dans le depot.
# Aucune branche n'est publiee : SetLive est vide, la mise en ligne se fait dans Steamworks.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VDF="$ROOT/Build/SteamPipe/app_build.vdf"
[ -f "$VDF" ] || { echo "Lancer d'abord Tools/Steam/make_steam_vdf.py" >&2; exit 2; }
[ -n "${STEAM_BUILD_USER:-}" ] || { echo "STEAM_BUILD_USER non defini" >&2; exit 2; }
STEAMCMD="${STEAMCMD:-steamcmd}"
"$STEAMCMD" +login "$STEAM_BUILD_USER" +run_app_build "$VDF" +quit
