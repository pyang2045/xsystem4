#!/bin/sh
# Fails on version or game checks outside the files allowed to have them.
# See docs/game-profiles.md.
cd "$(dirname "$0")/.." || exit 2
status=0

versions=$(grep -rnE 'ain->version|AIN_VERSION_' src \
	| grep -vE '^src/(profile|vm|page|heap|savedata|resume|ffi)\.c:')
if [ -n "$versions" ]; then
	echo "Version checks outside src/profile.c (use a named behaviour):"
	echo "$versions"
	status=1
fi

names=$(grep -rnE 'strcmp\((config\.)?game_name' src | grep -vE '^src/hacks\.c:')
if [ -n "$names" ]; then
	echo "Game name checks outside src/hacks.c:"
	echo "$names"
	status=1
fi

exit $status
