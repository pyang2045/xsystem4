#!/bin/sh
# Print a meson cross file for xsystem4 on iOS, using the libraries from build_deps.sh.
#
#   ios/cross.sh PLATFORM > FILE        PLATFORM: sim-x86_64, sim-arm64, device
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
deps="$(cd "${XSYS4_IOS_DEPS:-$here/../ios-deps}" 2>/dev/null && pwd)/$1" || deps="ios-deps/$1"
MIN=17.0
case $1 in
sim-x86_64) sdk=iphonesimulator arch=x86_64 fam=x86_64 sub=ios-simulator min=-mios-simulator-version-min=$MIN ;;
sim-arm64) sdk=iphonesimulator arch=arm64 fam=aarch64 sub=ios-simulator min=-mios-simulator-version-min=$MIN ;;
device) sdk=iphoneos arch=arm64 fam=aarch64 sub=ios min=-miphoneos-version-min=$MIN ;;
*) sed -n '2,4p' "$0" >&2; exit 1 ;;
esac
[ -d "$deps/lib/pkgconfig" ] || { echo "no $deps: run ios/build_deps.sh $1" >&2; exit 1; }
sysroot="$(xcrun --sdk $sdk --show-sdk-path)"
flags="'-arch', '$arch', '-isysroot', '$sysroot', '$min'"
cat <<EOT
[binaries]
c = ['$(xcrun --sdk $sdk -f clang)']
objc = ['$(xcrun --sdk $sdk -f clang)']
ar = '$(xcrun --sdk $sdk -f ar)'
strip = '$(xcrun --sdk $sdk -f strip)'
pkg-config = 'pkg-config'

[built-in options]
c_args = [$flags]
c_link_args = [$flags]
objc_args = [$flags]
objc_link_args = [$flags]

[properties]
pkg_config_libdir = '$deps/lib/pkgconfig'

[host_machine]
system = 'darwin'
subsystem = '$sub'
kernel = 'xnu'
cpu_family = '$fam'
cpu = '$arch'
endian = 'little'
EOT
