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

// Loads a table of a Chinese patch (65536 or fewer little-endian GBK codes, by
// SJIS code): the text stays SJIS for the VM, and its double-byte characters draw
// with translation_font(). Errors abort.
void translation_load_sjis_table(const char *path);
bool translation_has_sjis_table(void);

// The Unicode character behind an assigned SJIS code or a TEXT_CODE_UNICODE
// code, or 0.
uint32_t translation_sjis_to_unicode(uint32_t sjis_code);

/* A character of a UTF-8 game (ain_is_utf8) for the .fnl: its SJIS code
 * where the .fnl draws it (kana, punctuation, the JIS X 0208 rows, but not
 * the ideographs, as translated text), else TEXT_CODE_UNICODE | c, which
 * the .fnl draws with translation_font(). */
#define TEXT_CODE_UNICODE 0x80000000u
uint32_t translation_unicode_to_fnl_code(uint32_t c);

// TrueType font for the assigned codes (--locale-font, else a system CJK
// font, else the mincho font). NULL when no translation is loaded and the
// game is not a UTF-8 one.
struct font *translation_font(void);

extern char *translation_font_path;

#endif /* XSYSTEM4_TRANSLATION_H */
