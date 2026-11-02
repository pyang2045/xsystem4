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

#include <errno.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system4.h"
#include "system4/ain.h"
#include "system4/buffer.h"
#include "system4/file.h"
#include "system4/string.h"
#include "system4/utfsjis.h"

#include "gfx/font.h"
#include "translation.h"
#include "xsystem4.h"

char *translation_font_path = NULL;

/*
 * Assigned codes: every SJIS row outside JIS X 0208, which is all the game
 * text uses. F0..F9 are the CP932 user-defined area, 87 / ED, EE / FA..FC
 * the NEC and IBM extensions, 85, 86, EB, EC and EF unassigned. The .fnl has
 * no glyphs in any of them.
 */
static const uint8_t slot_rows[] = {
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
	0x85, 0x86, 0xEB, 0xEC, 0xEF, 0x87, 0xED, 0xEE, 0xFA, 0xFB, 0xFC
};
#define NR_SLOT_ROWS (sizeof(slot_rows) / sizeof(slot_rows[0]))
#define SLOTS_PER_ROW 188
#define MAX_SLOTS (NR_SLOT_ROWS * SLOTS_PER_ROW)

static uint32_t *slots;  // slot index -> Unicode
static int nr_slots;
static int slot_row_of_lead[256]; // lead byte -> row + 1, 0 = not a slot row
static bool translation_loaded;

static uint16_t slot_to_sjis(int k)
{
	unsigned t = k % SLOTS_PER_ROW;
	uint8_t trail = t < 0x3F ? 0x40 + t : 0x41 + t; // skips 0x7F
	return (slot_rows[k / SLOTS_PER_ROW] << 8) | trail;
}

bool translation_is_loaded(void)
{
	return translation_loaded;
}

uint32_t translation_sjis_to_unicode(uint32_t code)
{
	if (code & TEXT_CODE_UNICODE)
		return code & ~TEXT_CODE_UNICODE;
	if (!nr_slots || code > 0xffff)
		return 0;
	int row = slot_row_of_lead[code >> 8];
	if (!row)
		return 0;
	uint8_t trail = code & 0xFF;
	if (trail < 0x40 || trail == 0x7F || trail > 0xFC)
		return 0;
	int k = (row - 1) * SLOTS_PER_ROW + (trail < 0x7F ? trail - 0x40 : trail - 0x41);
	return k < nr_slots ? slots[k] : 0;
}

// Decodes one UTF-8 character; returns its length (invalid bytes: 1, U+FFFD).
static int utf8_decode(const uint8_t *s, uint32_t *out)
{
	if (s[0] < 0x80) {
		*out = s[0];
		return 1;
	}
	int n = s[0] >= 0xF0 ? 4 : s[0] >= 0xE0 ? 3 : s[0] >= 0xC0 ? 2 : 0;
	if (!n)
		goto invalid;
	uint32_t c = s[0] & (0x7F >> n);
	for (int i = 1; i < n; i++) {
		if ((s[i] & 0xC0) != 0x80)
			goto invalid;
		c = (c << 6) | (s[i] & 0x3F);
	}
	*out = c;
	return n;
invalid:
	*out = 0xFFFD;
	return 1;
}

// Chinese punctuation whose Japanese counterpart the .fnl draws.
static uint32_t prefer_sjis_form(uint32_t c)
{
	switch (c) {
	case 0x2014: return 0x2015; // — EM DASH -> ― HORIZONTAL BAR (815C)
	case 0x00B7: return 0x30FB; // · MIDDLE DOT -> ・ (8145)
	default:     return c;
	}
}

/*
 * The SJIS code of c as the .fnl knows it: JIS X 0208 rows (lead 81..84,
 * 88..9F, E0..EA) or half-width katakana. CP932's NEC and IBM extensions
 * are left to the TrueType font. Returns false if c has no such code.
 */
static bool unicode_to_sjis(uint32_t c, uint8_t out[2], int *len)
{
	static iconv_t cd = (iconv_t)-1;
	if (cd == (iconv_t)-1 && (cd = iconv_open("CP932", "UTF-32LE")) == (iconv_t)-1)
		ERROR("iconv_open(CP932): %s", strerror(errno));
	uint32_t in = c;
	char *inp = (char*)&in;
	size_t inleft = 4;
	char buf[4];
	char *outp = buf;
	size_t outleft = sizeof(buf);
	iconv(cd, NULL, NULL, NULL, NULL);
	if (iconv(cd, &inp, &inleft, &outp, &outleft) == (size_t)-1 || inleft)
		return false;
	size_t n = sizeof(buf) - outleft;
	uint8_t b0 = buf[0];
	if (n == 1 && b0 >= 0xA1 && b0 <= 0xDF) {
		out[0] = b0;
		*len = 1;
		return true;
	}
	if (n != 2)
		return false;
	if (!((b0 >= 0x81 && b0 <= 0x84) || (b0 >= 0x88 && b0 <= 0x9F) || (b0 >= 0xE0 && b0 <= 0xEA)))
		return false;
	out[0] = b0;
	out[1] = buf[1];
	*len = 2;
	return true;
}

/*
 * Translated text draws its ideographs with the TrueType font even where
 * the .fnl has the character: the two fonts differ in weight and shape, and
 * the .fnl draws Japanese forms, so a line mixing them looks patchy. Kana
 * and punctuation stay in the .fnl (the layout looks at 「, 、 and such).
 */
static bool is_ideograph(uint32_t c)
{
	return (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF)
		|| (c >= 0xF900 && c <= 0xFAFF) || (c >= 0x20000 && c <= 0x3134F);
}

uint32_t translation_unicode_to_fnl_code(uint32_t c)
{
	uint8_t b[2];
	int len;
	if (c < 0x80)
		return c;
	/* No CP932 code. The .fnl draws hearts at the codes of the male and female
	 * signs in the faces of the sizes that need them, as the game's own ♂ and ♀
	 * do (see fnl_font_size_by_scale). */
	if (c == 0x2764 || c == 0x2665) // ❤ ♥
		return 0x8189;
	if (c == 0x2661) // ♡
		return 0x818A;
	if (is_ideograph(c) || !unicode_to_sjis(c, b, &len))
		return TEXT_CODE_UNICODE | c;
	return len == 1 ? b[0] : (b[0] << 8) | b[1];
}

static int find_slot(uint32_t c)
{
	int lo = 0, hi = nr_slots - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (slots[mid] == c)
			return mid;
		if (slots[mid] < c)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return -1;
}

static char *encode_message(const char *utf8)
{
	struct buffer out;
	buffer_init(&out, NULL, 0);
	const uint8_t *s = (const uint8_t*)utf8;
	while (*s) {
		uint32_t c;
		s += utf8_decode(s, &c);
		if (c < 0x80) {
			buffer_write_int8(&out, c);
			continue;
		}
		c = prefer_sjis_form(c);
		int k = find_slot(c);
		if (k < 0) {
			uint8_t b[2];
			int len;
			if (!unicode_to_sjis(c, b, &len))
				ERROR("translation: U+%04X has no code", c); // collected in the first pass
			buffer_write_bytes(&out, b, len);
			continue;
		}
		uint16_t code = slot_to_sjis(k);
		buffer_write_int8(&out, code >> 8);
		buffer_write_int8(&out, code & 0xFF);
	}
	buffer_write_int8(&out, 0);
	return (char*)out.buf;
}

/*
 * .po parsing
 */

struct po_entry {
	int no;
	char *msgid;
	char *msgstr;
};

struct po_state {
	char *ctxt, *id, *str;
	char **field;
	bool fuzzy;
	bool plural;
};

static void po_append(char **field, const char *line, const char *path, int lineno)
{
	const char *p = strchr(line, '"');
	const char *end = p ? strrchr(line, '"') : NULL;
	if (!p || end == p)
		ERROR("%s:%d: expected a quoted string", path, lineno);
	size_t old = *field ? strlen(*field) : 0;
	char *s = xrealloc(*field, old + (end - p) + 1);
	char *d = s + old;
	for (p++; p < end; p++) {
		if (*p != '\\' || p + 1 >= end) {
			*d++ = *p;
			continue;
		}
		switch (*++p) {
		case 'n':  *d++ = '\n'; break;
		case 't':  *d++ = '\t'; break;
		case 'r':  *d++ = '\r'; break;
		case '"':  *d++ = '"'; break;
		case '\\': *d++ = '\\'; break;
		default:   *d++ = '\\'; *d++ = *p; break;
		}
	}
	*d = '\0';
	*field = s;
}

static void po_finish(struct po_state *st, struct po_entry **entries, int *nr, int *fuzzy)
{
	if (st->ctxt && st->id && *st->id && st->str && *st->str) {
		char *end;
		long no = strtol(st->ctxt, &end, 10);
		if (*st->ctxt && !*end && no >= 0) {
			if (st->fuzzy) {
				(*fuzzy)++;
			} else {
				*entries = xrealloc_array(*entries, *nr, *nr + 1, sizeof(struct po_entry));
				(*entries)[(*nr)++] = (struct po_entry) { no, st->id, st->str };
				st->id = st->str = NULL;
			}
		}
	}
	free(st->ctxt);
	free(st->id);
	free(st->str);
	*st = (struct po_state) {0};
}

static struct po_entry *po_read(const char *path, int *nr_out, int *fuzzy_out)
{
	size_t size;
	char *data = file_read(path, &size);
	if (!data)
		ERROR("Failed to read '%s': %s", path, strerror(errno));
	data = xrealloc(data, size + 1);
	data[size] = '\0';

	struct po_entry *entries = NULL;
	int nr = 0, fuzzy = 0;
	struct po_state st = {0};
	int lineno = 0;
	char *line = data;
	if (!strncmp(line, "\xEF\xBB\xBF", 3))
		line += 3;
	while (line) {
		char *next = strchr(line, '\n');
		if (next)
			*next++ = '\0';
		lineno++;
		size_t len = strlen(line);
		if (len && line[len-1] == '\r')
			line[--len] = '\0';
		while (*line == ' ' || *line == '\t')
			line++;

		if (!*line) {
			po_finish(&st, &entries, &nr, &fuzzy);
		} else if (line[0] == '#') {
			// a comment after the strings starts the next entry
			if (st.id || st.str)
				po_finish(&st, &entries, &nr, &fuzzy);
			if (line[1] == ',' && strstr(line, "fuzzy"))
				st.fuzzy = true;
		} else if (line[0] == '"') {
			if (!st.field)
				ERROR("%s:%d: string outside an entry", path, lineno);
			if (!st.plural)
				po_append(st.field, line, path, lineno);
		} else if (!strncmp(line, "msgctxt", 7)) {
			if (st.id || st.str)
				po_finish(&st, &entries, &nr, &fuzzy);
			st.field = &st.ctxt;
			po_append(st.field, line, path, lineno);
		} else if (!strncmp(line, "msgid_plural", 12)) {
			st.plural = true;
		} else if (!strncmp(line, "msgid", 5)) {
			if (st.id || st.str)
				po_finish(&st, &entries, &nr, &fuzzy);
			st.field = &st.id;
			st.plural = false;
			po_append(st.field, line, path, lineno);
		} else if (!strncmp(line, "msgstr[0]", 9) || !strncmp(line, "msgstr", 6)) {
			if (line[6] == '[' && line[7] != '0') {
				st.plural = true;
				continue;
			}
			st.field = &st.str;
			st.plural = false;
			po_append(st.field, line, path, lineno);
		} else {
			ERROR("%s:%d: can't parse line", path, lineno);
		}
		line = next;
	}
	po_finish(&st, &entries, &nr, &fuzzy);
	free(data);
	*nr_out = nr;
	*fuzzy_out = fuzzy;
	return entries;
}

void translation_apply(struct ain *ain, const char *po_path)
{
	int nr, fuzzy;
	if (game_charset_is_gbk()) {
		WARNING("%s: .po translations don't support GBK games (ignored)", po_path);
		return;
	}
	struct po_entry *entries = po_read(po_path, &nr, &fuzzy);

	// first pass: check each entry against the .ain, collect the characters
	// SJIS lacks and the ideographs SJIS has
	uint8_t *needed = xcalloc(0x110000 / 8, 1);
	uint8_t *ideographs = xcalloc(0x110000 / 8, 1);
	int nr_needed = 0, nr_ideographs = 0, bad_no = 0, changed = 0;
	for (int i = 0; i < nr; i++) {
		struct po_entry *e = &entries[i];
		if (e->no >= ain->nr_messages || !ain->messages[e->no]) {
			if (bad_no++ < 5)
				WARNING("%s: message %d is not in the .ain", po_path, e->no);
			e->no = -1;
			continue;
		}
		char *orig = game_to_utf8(ain->messages[e->no]->text, ain->messages[e->no]->size);
		bool same = !strcmp(orig, e->msgid);
		free(orig);
		if (!same) {
			if (changed++ < 5)
				WARNING("%s: message %d: msgid differs from the .ain (skipped)", po_path, e->no);
			e->no = -1;
			continue;
		}
		if (ain_is_utf8)
			continue;
		const uint8_t *s = (const uint8_t*)e->msgstr;
		while (*s) {
			uint32_t c;
			s += utf8_decode(s, &c);
			if (c < 0x80 || c > 0x10FFFF)
				continue;
			c = prefer_sjis_form(c);
			uint8_t b[2];
			int len;
			if ((needed[c/8] | ideographs[c/8]) & (1 << (c%8)))
				continue;
			if (!unicode_to_sjis(c, b, &len)) {
				needed[c/8] |= 1 << (c%8);
				nr_needed++;
			} else if (is_ideograph(c)) {
				ideographs[c/8] |= 1 << (c%8);
				nr_ideographs++;
			}
		}
	}
	if (nr_needed > (int)MAX_SLOTS)
		ERROR("%s: %d characters outside Shift-JIS; at most %d fit",
		      po_path, nr_needed, (int)MAX_SLOTS);
	if (nr_needed + nr_ideographs <= (int)MAX_SLOTS) {
		for (int i = 0; i < 0x110000 / 8; i++)
			needed[i] |= ideographs[i];
		nr_needed += nr_ideographs;
	} else {
		WARNING("%s: %d + %d characters do not fit in %d codes; ideographs in"
			" Shift-JIS keep the game font", po_path, nr_needed, nr_ideographs,
			(int)MAX_SLOTS);
	}
	free(ideographs);

	// assign codes in Unicode order, so a .po edit moves few of them
	slots = xcalloc(nr_needed ? nr_needed : 1, sizeof(uint32_t));
	for (uint32_t c = 0; c < 0x110000; c++) {
		if (needed[c/8] & (1 << (c%8)))
			slots[nr_slots++] = c;
	}
	free(needed);
	for (unsigned i = 0; i < NR_SLOT_ROWS; i++)
		slot_row_of_lead[slot_rows[i]] = i + 1;

	// second pass: replace the messages
	int applied = 0;
	for (int i = 0; i < nr; i++) {
		struct po_entry *e = &entries[i];
		if (e->no >= 0) {
			// A UTF-8 game's strings need no codes: the engine draws any character.
			char *text = ain_is_utf8 ? xstrdup(e->msgstr) : encode_message(e->msgstr);
			free_string(ain->messages[e->no]);
			ain->messages[e->no] = make_string(text, strlen(text));
			free(text);
			applied++;
		}
		free(e->msgid);
		free(e->msgstr);
	}
	free(entries);
	translation_loaded = true;

	NOTICE("Translation: %d of %d messages from '%s' (%d characters in the translation font)",
	       applied, ain->nr_messages, po_path, nr_slots);
	if (bad_no || changed || fuzzy)
		WARNING("Translation: skipped %d unknown, %d changed, %d fuzzy entries",
			bad_no, changed, fuzzy);
}

struct font *translation_font(void)
{
	static struct font *font;
	static bool tried;
	if (font || tried || (!translation_loaded && !ain_is_utf8))
		return font;
	tried = true;

	static const char *system_fonts[] = {
		"/System/Library/Fonts/Hiragino Sans GB.ttc",
		"/System/Library/Fonts/STHeiti Light.ttc",
		"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
	};
	if (translation_font_path) {
		if ((font = ft_font_load(translation_font_path)))
			return font;
		WARNING("Failed to load --locale-font '%s'", translation_font_path);
	}
	for (size_t i = 0; i < sizeof(system_fonts) / sizeof(*system_fonts); i++) {
		if (file_exists(system_fonts[i]) && (font = ft_font_load(system_fonts[i]))) {
			NOTICE("Translation font: %s", system_fonts[i]);
			return font;
		}
	}
	extern struct font *font_ttf[2];
	WARNING("No CJK font found for the translation; using the mincho font");
	return font = font_ttf[FONT_MINCHO];
}

/*
 * .pot template
 */

static void pot_write_string(FILE *f, const char *s)
{
	fputc('"', f);
	for (; *s; s++) {
		switch (*s) {
		case '"':  fputs("\\\"", f); break;
		case '\\': fputs("\\\\", f); break;
		case '\t': fputs("\\t", f); break;
		case '\r': fputs("\\r", f); break;
		case '\n': fputs("\\n", f); break;
		default:   fputc(*s, f); break;
		}
	}
	fputs("\"\n", f);
}

bool translation_write_pot(struct ain *ain, const char *path)
{
	FILE *f = file_open_utf8(path, "wb");
	if (!f) {
		WARNING("Failed to open '%s': %s", path, strerror(errno));
		return false;
	}
	fputs("# Message templates for the --locale option of xsystem4.\n"
	      "# msgctxt is the message number in the .ain; keep msgctxt and msgid as they are.\n"
	      "msgid \"\"\n"
	      "msgstr \"\"\n"
	      "\"Content-Type: text/plain; charset=UTF-8\\n\"\n\n", f);
	int n = 0;
	for (int i = 0; i < ain->nr_messages; i++) {
		struct string *m = ain->messages[i];
		if (!m || !m->size)
			continue;
		char *u = game_to_utf8(m->text, m->size);
		fprintf(f, "msgctxt \"%d\"\nmsgid ", i);
		pot_write_string(f, u);
		fputs("msgstr \"\"\n\n", f);
		free(u);
		n++;
	}
	fclose(f);
	NOTICE("Wrote %d messages to '%s'", n, path);
	return true;
}
