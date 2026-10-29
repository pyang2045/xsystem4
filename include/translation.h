/* Copyright (C) 2026
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#ifndef XSYSTEM4_TRANSLATION_H
#define XSYSTEM4_TRANSLATION_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Message translations from a gettext .po file (--locale).
 *
 * An SJIS game keeps running in SJIS: each translated message (MSG0) is
 * re-encoded to SJIS. Characters Shift-JIS lacks (simplified Chinese 们, 这,
 * 吧, ...) get a code in rows the game never uses (F0..F9, then 85, 86, EB,
 * EC, EF); the text renderer maps those codes back to Unicode and draws them
 * with a TrueType font. The game scripts see ordinary 2-byte SJIS text, so
 * string functions, line breaking, the backlog and saves need no changes.
 *
 * .po entries: msgctxt is the message number, msgid the original text (an
 * entry whose msgid no longer matches the .ain is skipped), msgstr the
 * translation (empty = untranslated). "#, fuzzy" entries are skipped.
 */

struct ain;
struct font;

// Writes every non-empty message of the .ain as a .pot template.
bool translation_write_pot(struct ain *ain, const char *path);

// Loads the .po file and replaces the translated messages of the .ain.
// Call before the VM runs. Errors abort; mismatches are warned about.
void translation_apply(struct ain *ain, const char *po_path);

// The Unicode character behind an assigned SJIS code, or 0.
uint32_t translation_sjis_to_unicode(uint16_t sjis_code);

// TrueType font for the assigned codes (--locale-font, else a system CJK
// font, else the mincho font). NULL when no translation is loaded.
struct font *translation_font(void);

extern char *translation_font_path;

#endif /* XSYSTEM4_TRANSLATION_H */
