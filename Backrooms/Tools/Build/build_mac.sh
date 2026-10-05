#!/usr/bin/env bash
# v4.9 : construction, cuisson et paquet macOS natif (Metal), binaire universel arm64 + x86_64.
#   UE_ROOT="/Users/Shared/Epic Games/UE_5.8" Tools/Build/build_mac.sh [Shipping|Development]
# Sur un Mac, avec Xcode compatible avec UE 5.8. Ensuite, si MAC_SIGN_IDENTITY est defini : signature avec le
# Hardened Runtime et les droits de mac/Backrooms.entitlements ; si NOTARY_PROFILE est defini (profil cree par
# "xcrun notarytool store-credentials") : notarisation Apple puis agrafage. Identifiants et certificats restent hors du
# depot (variables d'environnement, trousseau).
# Resultat : Build/Mac/<Config>/Mac/Backrooms.app et build_manifest.txt.
source "$(dirname "$0")/common.sh"
CONFIG="${1:-Shipping}"
br_check_engine
XCODE="$(xcodebuild -version 2>/dev/null | tr '\n' ' ' || echo 'Xcode introuvable')"
if [ -n "${XCODE_VERSION:-}" ] && ! echo "$XCODE" | grep -q "Xcode $XCODE_VERSION"; then
	echo "Attention : $XCODE, Xcode $XCODE_VERSION attendu (Tools/Build/versions.env)" >&2
fi
OUT="$BR_PROJECT_DIR/Build/Mac/$CONFIG"
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$BR_PROJECT" -noP4 -utf8output \
	-platform=Mac -clientconfig="$CONFIG" -specifiedarchitecture=arm64+x86_64 \
	-build -cook -stage -pak -archive -archivedirectory="$OUT"
br_write_manifest "$OUT" "macOS (Metal, arm64+x86_64)" "$CONFIG" "$XCODE"

APP="$(find "$OUT" -maxdepth 3 -name '*.app' -type d | head -1)"
[ -n "$APP" ] || { echo "Aucun .app dans $OUT" >&2; exit 4; }
echo "Paquet : $APP"
# Architectures reellement presentes (le binaire doit contenir arm64 et x86_64)
EXE="$APP/Contents/MacOS/$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$APP/Contents/Info.plist")"
ARCHS="$(lipo -archs "$EXE")"
echo "Architectures : $ARCHS" | tee -a "$OUT/build_manifest.txt"
case "$ARCHS" in *arm64*x86_64*|*x86_64*arm64*) ;; *) echo "Binaire non universel : $ARCHS" >&2; exit 5;; esac

# Chat vocal : description d'usage du micro (macOS refuse l'acces sans elle ; le jeu reste jouable sans micro)
PLIST="$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Delete :NSMicrophoneUsageDescription" "$PLIST" 2>/dev/null || true
/usr/libexec/PlistBuddy -c "Add :NSMicrophoneUsageDescription string 'The Backrooms uses the microphone for optional proximity voice chat with other players. / Le micro sert au chat vocal de proximite (facultatif).'" "$PLIST"

if [ -n "${MAC_SIGN_IDENTITY:-}" ]; then
	ENT="$BR_TOOLS/mac/Backrooms.entitlements"
	# Bibliotheques internes d'abord, puis le paquet (pas de --deep : chaque binaire est signe explicitement)
	find "$APP/Contents" \( -name '*.dylib' -o -name '*.so' \) -type f -print0 | while IFS= read -r -d '' LIB; do
		codesign --force --timestamp --options runtime --sign "$MAC_SIGN_IDENTITY" "$LIB"
	done
	find "$APP/Contents" -name '*.framework' -type d -prune -print0 | while IFS= read -r -d '' FW; do
		codesign --force --timestamp --options runtime --sign "$MAC_SIGN_IDENTITY" "$FW"
	done
	codesign --force --timestamp --options runtime --entitlements "$ENT" --sign "$MAC_SIGN_IDENTITY" "$APP"
	codesign --verify --strict --verbose=2 "$APP"
	if [ -n "${NOTARY_PROFILE:-}" ]; then
		ZIP="$OUT/Backrooms-notarize.zip"
		ditto -c -k --keepParent "$APP" "$ZIP"
		xcrun notarytool submit "$ZIP" --keychain-profile "$NOTARY_PROFILE" --wait
		xcrun stapler staple "$APP"
		spctl --assess --type execute --verbose=4 "$APP"
		rm -f "$ZIP"
	else
		echo "NOTARY_PROFILE non defini : paquet signe, non notarise (prepare, non execute)"
	fi
else
	echo "MAC_SIGN_IDENTITY non defini : paquet non signe (prepare, non execute)"
fi
python3 "$BR_TOOLS/check_package.py" --platform Mac --dir "$OUT" --ue "$UE_ROOT"
