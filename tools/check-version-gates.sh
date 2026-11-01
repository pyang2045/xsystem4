#!/bin/sh
# Fails on version or game checks outside the files allowed to have them.
# See docs/game-profiles.md.
cd "$(dirname "$0")/.." || exit 2
status=0

# The version of another format (a save, a flash, a model) is not the game's.
other_formats='^src/(swf|apeg|3d/[a-z_]+|dungeon/[a-z_]+|hll/(ReignEngine|Toushin3Loader|MsgSkip)|hll/system)\.c:|^include/(pl_mpeg|profile)\.h:|game_version'
versions=$(grep -rnE '(->|\.)version\b|AIN_VERSION_' src include \
	| grep -vE "^src/(profile|vm|page|heap|savedata|resume|ffi)\.c:" \
	| grep -vE "$other_formats")
if [ -n "$versions" ]; then
	echo "Version checks outside src/profile.c (use a named behaviour):"
	echo "$versions"
	status=1
fi

# The files of rule 3 follow a format; their checks are counted, so API
# behaviour doesn't grow there unseen. Change a number only with a format reason.
for allowed in vm:67 page:33 heap:2 savedata:4 resume:1 ffi:1; do
	file=src/${allowed%:*}.c
	expected=${allowed#*:}
	count=$(grep -cE 'ain->version|AIN_VERSION_' "$file")
	if [ "$count" != "$expected" ]; then
		echo "$file has $count version checks, expected $expected:"
		echo "  API behaviour goes in src/profile.c; a format's check goes in this list."
		status=1
	fi
done

names=$(grep -rnE '(str(n|case)?cmp|strstr)\([^;]*(game_name|boot_name)' src include | grep -vE '^src/hacks\.c:')
if [ -n "$names" ]; then
	echo "Game name checks outside src/hacks.c:"
	echo "$names"
	status=1
fi

exit $status
