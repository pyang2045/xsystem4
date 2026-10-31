#!/bin/sh
# Assemble an iPad app from an iOS meson build of xsystem4, without Xcode's IDE.
#
#   ios/make_app.sh CONFIG BUILD_DIR OUT_DIR [SIGN_IDENTITY PROFILE]
#
# CONFIG is an app config (see ios/app.py; ios/xsystem4.json is the plain app). BUILD_DIR is a
# meson build made with ios/cross.sh. Writes OUT_DIR/NAME.app: engine, shaders/metal, fonts/,
# COPYING.xsystem4, the config's resources and its asset catalog (icon, compiled by actool);
# the game is not included (it goes in the app's Documents). Without SIGN_IDENTITY the app
# is ad-hoc signed: enough for the Simulator, or for a sideloading tool to re-sign. For a
# device, pass "Apple Development" and the profile from ios/provision.sh.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
[ $# -ge 3 ] || { sed -n '2,12p' "$0"; exit 1; }
cfg="$1"; bld="$(cd "$2" && pwd)"; out="$3"; ident="${4:--}"; profile="${5:-}"
app_py() { python3 -I "$here/app.py" "$@"; }
name="$(app_py get "$cfg" name)"
src="$(cd "$here/.." && pwd)"
python3 -I -c 'import json,sys; m=json.load(open(sys.argv[1]))["host"]; print(m.get("subsystem",""))' "$bld/meson-info/intro-machines.json" | grep -q simulator && plat=iPhoneSimulator || plat=iPhoneOS
rev="$(git -C "$src" rev-parse --short HEAD)"
app="$out/$name.app"
rm -rf "$app"; mkdir -p "$app/shaders"
cp "$bld/src/xsystem4" "$app/$name"
strip -S "$app/$name"  # debug info names build directories
cp -R "$src/shaders/metal" "$app/shaders/"; rm -f "$app/shaders/metal/glsl2msl.py"
cp -R "$src/fonts" "$app/"
cp "$src/COPYING" "$app/COPYING.xsystem4"
app_py resources "$cfg" | while IFS="$(printf '\t')" read -r dest file; do
	mkdir -p "$app/$(dirname "$dest")"
	cp "$file" "$app/$dest"
done
app_py plist "$cfg" "$rev" "$plat" > "$app/Info.plist"
assets="$(app_py get "$cfg" assets)"
if [ -n "$assets" ]; then
	[ $plat = iPhoneOS ] && sdk=iphoneos || sdk=iphonesimulator
	icon="$(mktemp)"
	xcrun actool --compile "$app" --platform $sdk --minimum-deployment-target 17.0 --target-device ipad \
		--app-icon AppIcon --output-partial-info-plist "$icon" --errors --warnings "$assets" >/dev/null
	/usr/libexec/PlistBuddy -c "Merge $icon" "$app/Info.plist" >/dev/null
	rm -f "$icon"
fi
plutil -lint "$app/Info.plist" >/dev/null
if [ -n "$profile" ]; then
	cp "$profile" "$app/embedded.mobileprovision"
	ent="$(mktemp)"; trap 'rm -f "$ent" "$ent.p"' EXIT
	security cms -D -i "$profile" > "$ent.p"
	plutil -extract Entitlements xml1 -o "$ent" "$ent.p"
	codesign -f -s "$ident" --entitlements "$ent" "$app"
else
	codesign -f -s "$ident" "$app" >/dev/null 2>&1 || codesign -f -s "$ident" "$app"
fi
echo "$app ($plat, $(app_py get "$cfg" version), xsystem4 $rev)"
