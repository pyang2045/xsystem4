#!/bin/sh
# Fails when the iOS project and meson build different engine sources: the
# project is generated (ios/README.md), and it doesn't notice a new file.
cd "$(dirname "$0")/.." || exit 2
meson=$(grep -oE "'[A-Za-z0-9_/.-]+\.(c|m)'" src/meson.build | tr -d "'" | sort -u)
xcode=$(grep -oE 'path = \.\./src/[A-Za-z0-9_/.-]+\.(c|m);' ios/xsystem4.xcodeproj/project.pbxproj \
	| sed -e 's|path = \.\./src/||' -e 's/;$//' | sort -u)
# Left out of iOS on purpose: the debugger, and ffmpeg for movies.
missing=$(echo "$meson" | grep -vE '^(debug|debug_info|debugger_[a-z]+|movie_ffmpeg)\.c$' \
	| while read -r f; do echo "$xcode" | grep -qx "$f" || echo "$f"; done)
if [ -n "$missing" ]; then
	echo "In src/meson.build but not in ios/xsystem4.xcodeproj (see ios/README.md):"
	echo "$missing"
	exit 1
fi
