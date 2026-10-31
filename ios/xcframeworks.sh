#!/bin/sh
# Package the libraries from build_deps.sh as XCFrameworks, for the Xcode project.
#
#   ios/xcframeworks.sh
#
# Needs ios-deps/{device,sim-x86_64,sim-arm64} (ios/build_deps.sh device sim-x86_64 sim-arm64).
# Writes ios-deps/xcframeworks/NAME.xcframework: the device library, and one Simulator library
# holding both architectures, each with the headers xsystem4 includes. A header that differs
# between the two Simulator architectures is kept twice, under #if defined(__x86_64__).
# Returns at once when this script and the three platforms are unchanged.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
top="$(cd "${XSYS4_IOS_DEPS:-$here/../ios-deps}" && pwd)"
out="$top/xcframeworks"
PLATS="device sim-x86_64 sim-arm64"
key="$(shasum -a 256 "$0" | cut -c1-16)"
for p in $PLATS; do
	[ -f "$top/$p/build_deps.stamp" ] || { echo "no $top/$p: run ios/build_deps.sh $PLATS" >&2; exit 1; }
	key="$key $(cat "$top/$p/build_deps.stamp")"
done
[ "$(cat "$out/stamp" 2>/dev/null)" != "$key" ] || { echo "xcframeworks up to date"; exit 0; }

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

headers() {  # name platform-dir dest: the headers xsystem4 includes from library NAME
	i="$2/include"; mkdir -p "$3"
	case $1 in
	SDL2) cp -R "$i/SDL2/." "$3" ;;                     # <SDL.h>
	freetype) cp -R "$i/freetype2/." "$3" ;;            # <ft2build.h>
	ffi) cp "$i/ffi.h" "$i/ffitarget.h" "$3" ;;
	cglm) cp -R "$i/cglm" "$3" ;;                        # <cglm/cglm.h>
	sndfile) cp "$i/sndfile.h" "$3" ;;
	turbojpeg) cp "$i/turbojpeg.h" "$3" ;;
	webp) mkdir -p "$3/webp"; cp "$i/webp/decode.h" "$i/webp/encode.h" "$i/webp/types.h" "$3/webp" ;;
	png16) cp "$i/png.h" "$i/pngconf.h" "$i/pnglibconf.h" "$3" ;;
	*) rmdir "$3"; return 1 ;;                           # library only
	esac
}

merge_sim_headers() {  # x86_64-dir arm64-dir dest
	(cd "$1" && find . -type f) | while read -r f; do
		mkdir -p "$3/$(dirname "$f")"
		if cmp -s "$1/$f" "$2/$f"; then
			cp "$1/$f" "$3/$f"
		else
			{ echo "#if defined(__x86_64__)"; cat "$1/$f"; echo "#else"; cat "$2/$f"; echo "#endif"; } > "$3/$f"
		fi
	done
}

rm -rf "$out"; mkdir -p "$out"
# name: library file in lib/
for spec in SDL2:libSDL2.a SDL2main:libSDL2main.a freetype:libfreetype.a ffi:libffi.a cglm:libcglm.a \
	sndfile:libsndfile.a FLAC:libFLAC.a vorbis:libvorbis.a vorbisenc:libvorbisenc.a ogg:libogg.a \
	opus:libopus.a turbojpeg:libturbojpeg.a webp:libwebp.a sharpyuv:libsharpyuv.a png16:libpng16.a; do
	name="${spec%%:*}"; lib="${spec#*:}"
	w="$tmp/$name"; mkdir -p "$w/device" "$w/sim"
	cp "$top/device/lib/$lib" "$w/device/"
	lipo -create "$top/sim-x86_64/lib/$lib" "$top/sim-arm64/lib/$lib" -output "$w/sim/$lib"
	set -- -library "$w/device/$lib"
	if headers "$name" "$top/device" "$w/device-h"; then
		headers "$name" "$top/sim-x86_64" "$w/x86-h"
		headers "$name" "$top/sim-arm64" "$w/arm-h"
		merge_sim_headers "$w/x86-h" "$w/arm-h" "$w/sim-h"
		set -- "$@" -headers "$w/device-h" -library "$w/sim/$lib" -headers "$w/sim-h"
	else
		set -- "$@" -library "$w/sim/$lib"
	fi
	xcodebuild -create-xcframework "$@" -output "$out/$name.xcframework" >/dev/null
	echo "xcframework: $name"
done
echo "$key" > "$out/stamp"
