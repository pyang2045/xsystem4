#!/bin/sh
# Build xsystem4's libraries as static iOS libraries.
#
#   ios/build_deps.sh PLATFORM...      PLATFORM: sim-x86_64, sim-arm64, device
#
# Sources are downloaded (sha256-checked) into ios-deps/src; each platform installs into
# ios-deps/PLATFORM/{include,lib,lib/pkgconfig}. ios-deps is xsystem4/ios-deps (gitignored)
# unless $XSYS4_IOS_DEPS names another folder. zlib and iconv come from the SDK. iOS 17.0.
# A library is rebuilt when this script changes (versions, flags); a platform whose stamp
# matches the script is skipped at once, so the Xcode project runs this on every build.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
top="${XSYS4_IOS_DEPS:-$here/../ios-deps}"
[ $# -ge 1 ] || { sed -n '2,10p' "$0"; exit 1; }
MIN=17.0
mkdir -p "$top/src"; top="$(cd "$top" && pwd)"
self="$(shasum -a 256 "$0" | cut -c1-16)"  # build key: this script's contents
current=1
for plat in "$@"; do
	[ "$(cat "$top/$plat/build_deps.stamp" 2>/dev/null)" = "$self" ] || current=0
done
[ $current = 0 ] || { echo "ios-deps up to date: $*"; exit 0; }

fetch() {  # name url sha256
	f="$top/src/$(basename "$2")"
	[ -f "$f" ] || curl -fL# -o "$f" "$2"
	echo "$3  $f" | shasum -a 256 -c - >/dev/null || { echo "bad checksum: $f" >&2; exit 1; }
	d="$top/src/$1"
	[ "$(cat "$d/.tarball" 2>/dev/null)" = "$(basename "$f")" ] && return
	rm -rf "$d"; mkdir -p "$d"; tar -xf "$f" -C "$d" --strip-components 1
	basename "$f" > "$d/.tarball"
}
fetch SDL2 https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz 5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
fetch freetype https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz 0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289
fetch libffi https://github.com/libffi/libffi/releases/download/v3.5.2/libffi-3.5.2.tar.gz f3a3082a23b37c293a4fcd1053147b371f2ff91fa7ea1b2a52e335676bac82dc
fetch libjpeg-turbo https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.0/libjpeg-turbo-3.1.0.tar.gz 9564c72b1dfd1d6fe6274c5f95a8d989b59854575d4bbee44ade7bc17aa9bc93
fetch libpng https://download.sourceforge.net/libpng/libpng-1.6.47.tar.xz b213cb381fbb1175327bd708a77aab708a05adde7b471bc267bd15ac99893631
fetch libwebp https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.5.0.tar.gz 7d6fab70cf844bf6769077bd5d7a74893f8ffd4dfb42861745750c63c2a5c92c
fetch libogg https://downloads.xiph.org/releases/ogg/libogg-1.3.6.tar.xz 5c8253428e181840cd20d41f3ca16557a9cc04bad4a3d04cce84808677fa1061
fetch libvorbis https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b
fetch flac https://downloads.xiph.org/releases/flac/flac-1.4.3.tar.xz 6c58e69cd22348f441b861092b825e591d0b822e106de6eb0ee4d05d27205b70
fetch opus https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz 65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1
fetch libsndfile https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-1.2.2.tar.xz 3799ca9924d3125038880367bf1468e53a1b7e3686a934f098b7e1d286cdb80e
fetch cglm https://github.com/recp/cglm/archive/refs/tags/v0.9.7.tar.gz cc0862eee2885d2a2b4870eb29b97c6e9b9ebc7c66e8854e1fa8f3d18d9d8475

for plat in "$@"; do
	case $plat in
	sim-x86_64) sdk=iphonesimulator arch=x86_64 host=x86_64-apple-darwin minflag=-mios-simulator-version-min=$MIN ;;
	sim-arm64) sdk=iphonesimulator arch=arm64 host=aarch64-apple-darwin minflag=-mios-simulator-version-min=$MIN ;;
	device) sdk=iphoneos arch=arm64 host=aarch64-apple-darwin minflag=-miphoneos-version-min=$MIN ;;
	*) echo "unknown platform $plat" >&2; exit 1 ;;
	esac
	pfx="$top/$plat"; bld="$top/build-$plat"
	mkdir -p "$pfx" "$bld"
	sysroot="$(xcrun --sdk $sdk --show-sdk-path)"
	cflags="-arch $arch -isysroot $sysroot $minflag -O2"
	map="-ffile-prefix-map=$top/="  # __FILE__ in assert strings: no home path in the app
	export PKG_CONFIG_LIBDIR="$pfx/lib/pkgconfig" PKG_CONFIG_PATH=""
	cm() {  # name [cmake args]
		n=$1; shift
		[ "$(cat "$bld/$n.done" 2>/dev/null)" = "$self" ] && return
		rm -rf "$bld/$n"
		cmake -S "$top/src/$n" -B "$bld/$n" -G Ninja -DCMAKE_SYSTEM_NAME=iOS \
			-DCMAKE_OSX_SYSROOT=$sdk -DCMAKE_OSX_ARCHITECTURES=$arch -DCMAKE_SYSTEM_PROCESSOR=$arch \
			-DCMAKE_OSX_DEPLOYMENT_TARGET=$MIN -DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_INSTALL_PREFIX="$pfx" -DCMAKE_PREFIX_PATH="$pfx" \
			-DCMAKE_FIND_ROOT_PATH="$pfx" -DBUILD_SHARED_LIBS=OFF \
			-DCMAKE_C_FLAGS="$map" -DCMAKE_CXX_FLAGS="$map" \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 "$@" >"$bld/$n.log" 2>&1 \
			&& cmake --build "$bld/$n" >>"$bld/$n.log" 2>&1 \
			&& cmake --install "$bld/$n" >>"$bld/$n.log" 2>&1 \
			|| { echo "$plat $n failed: see $bld/$n.log" >&2; exit 1; }
		echo "$self" > "$bld/$n.done"; echo "$plat: $n"
	}
	ac() {  # name [configure args]
		n=$1; shift
		[ "$(cat "$bld/$n.done" 2>/dev/null)" = "$self" ] && return
		rm -rf "$bld/$n"; mkdir -p "$bld/$n"
		(cd "$bld/$n" && CC="$(xcrun --sdk $sdk -f clang)" CFLAGS="$cflags $map" LDFLAGS="$cflags" \
			"$top/src/$n/configure" --host=$host --prefix="$pfx" --disable-shared --enable-static "$@" \
			&& make -j8 && make install) >"$bld/$n.log" 2>&1 \
			|| { echo "$plat $n failed: see $bld/$n.log" >&2; exit 1; }
		echo "$self" > "$bld/$n.done"; echo "$plat: $n"
	}
	mkdir -p "$pfx/lib/pkgconfig"  # zlib is the SDK's
	printf 'Name: zlib\nDescription: zlib (iOS SDK)\nVersion: 1.2.12\nLibs: -lz\nCflags:\n' \
		>"$pfx/lib/pkgconfig/zlib.pc"
	cm SDL2 -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF
	ac libffi --disable-docs --disable-multi-os-directory
	cm freetype -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
	cm libjpeg-turbo -DENABLE_SHARED=OFF -DWITH_SIMD=OFF
	cm libpng -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF
	cm libwebp -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF \
		-DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF \
		-DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF
	cm libogg
	cm libvorbis
	cm flac -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF \
		-DINSTALL_MANPAGES=OFF -DBUILD_CXXLIBS=OFF -DWITH_STACK_PROTECTOR=OFF
	cm opus -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF
	cm libsndfile -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DENABLE_EXTERNAL_LIBS=ON \
		-DENABLE_MPEG=OFF -DENABLE_CPACK=OFF -DINSTALL_PKGCONFIG_MODULE=ON
	cm cglm -DCGLM_STATIC=ON -DCGLM_SHARED=OFF -DCGLM_USE_TEST=OFF
	# Everything is static: fold Libs.private/Requires.private into Libs/Requires, so that a
	# lookup without --static (e.g. libsys4's) still gets every library.
	python3 -I - "$pfx/lib/pkgconfig" <<'PY'
import glob, os, re, sys
for pc in glob.glob(os.path.join(sys.argv[1], '*.pc')):
    lines = open(pc).read().splitlines()
    fields = {}
    for l in lines:
        m = re.match(r'(Libs|Requires)(\.private)?:(.*)', l)
        if m: fields.setdefault(m.group(1), []).append(m.group(3).strip())
    out = [l for l in lines if not re.match(r'(Libs|Requires)(\.private)?:', l)]
    for k in ('Requires', 'Libs'):
        if k in fields: out.append(k + ': ' + ' '.join(v for v in fields[k] if v))
    open(pc, 'w').write('\n'.join(out) + '\n')
PY
	echo "$self" > "$pfx/build_deps.stamp"
	echo "$plat: done -> $pfx"
done
