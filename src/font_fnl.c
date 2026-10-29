/* Copyright (C) 2019 Nunuhara Cabbage <nunuhara@haniwa.technology>
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

#include <math.h>

#include "system4.h"
#include "system4/fnl.h"
#include "system4/hashtable.h"
#include "system4/string.h"
#include "system4/utfsjis.h"

#include "gfx/gfx.h"
#include "gfx/font.h"
#include "translation.h"
#include "xsystem4.h"

struct fnl_bitmap_glyph {
	unsigned width;
	unsigned height;
	unsigned advance;
	uint8_t *pixels;
};

struct fnl_bitmap_size {
	struct fnl_font_face *face;
	unsigned nr_glyphs;
	struct fnl_bitmap_glyph **glyphs;
};

struct fnl_font_size {
	struct font_size super;
	struct fnl_bitmap_size *bitmap_size;
	unsigned denominator;
};

struct font_fnl {
	struct font super;
	struct fnl_font *font;
	unsigned nr_bitmap_sizes;
	struct fnl_bitmap_size *bitmap_sizes;
	unsigned nr_sizes;
	struct fnl_font_size *sizes;
};

static struct font_size *fnl_font_get_size(struct font *_font, float size)
{
	struct font_fnl *font = (struct font_fnl*)_font;

	float min_diff = 9999;
	struct fnl_font_size *closest = &font->sizes[0];
	for (unsigned i = 0; i < font->nr_sizes; i++) {
		float diff = fabsf(font->sizes[i].super.size - size);
		if (diff < min_diff) {
			min_diff = diff;
			closest = &font->sizes[i];
		}
	}
	return &closest->super;
}

static struct font_size *fnl_font_get_size_round_down(struct font *_font, float size)
{
	struct font_fnl *font = (struct font_fnl*)_font;

	float min_diff = 9999;
	struct fnl_font_size *closest = &font->sizes[0];
	for (unsigned i = 0; i < font->nr_sizes; i++) {
		if (font->sizes[i].super.size > size)
			continue;
		float diff = fabsf(font->sizes[i].super.size - size);
		if (diff < min_diff) {
			min_diff = diff;
			closest = &font->sizes[i];
		}
	}
	return &closest->super;
}

static float fnl_font_get_actual_size(struct font *font, float size)
{
	return fnl_font_get_size(font, size)->size;
}

static float fnl_font_get_actual_size_round_down(struct font *font, float size)
{
	return fnl_font_get_size_round_down(font, size)->size;
}

static struct fnl_bitmap_glyph *fnl_get_bitmap_glyph(struct fnl_bitmap_size *bitmap, unsigned index)
{
	if (bitmap->glyphs[index])
		return bitmap->glyphs[index];

	struct fnl_glyph *glyph = &bitmap->face->glyphs[index];
	uint8_t *data = fnl_glyph_data(bitmap->face->font->fnl, glyph);

	const int glyph_w = fnl_glyph_stride(glyph) * 8;
	const int glyph_h = bitmap->face->height;

	struct fnl_bitmap_glyph *out = xmalloc(sizeof(struct fnl_bitmap_glyph));
	out->width = glyph_w;
	out->height = glyph_h;
	out->advance = glyph->width;
	out->pixels = xcalloc(glyph_w, glyph_h);

	// expand 1-bit bitmap to 8-bit
	for (unsigned row = 0; row < glyph_h; row++) {
		uint8_t *dst = out->pixels + ((glyph_h - 1) - row) * glyph_w;
		for (unsigned col = 0; col < glyph->width; col++) {
			unsigned i = row * glyph_w + col;
			bool on = data[i/8] & (1 << (7 - i % 8));
			dst[col] = on ? 255 : 0;
		}
	}

	free(data);
	bitmap->glyphs[index] = out;
	return out;
}

#define GLYPH_BORDER_SIZE 4

static float fnl_font_size_char(struct font_size *_size, uint32_t code);

/*
 * A translated message's characters outside Shift-JIS (--locale) come from
 * the translation's TrueType font. The .fnl size is the line height; its
 * full-width characters are narrower (30.67 at size 40 in 母爛漫), so the
 * TrueType font gets the width of 亜 as its size and advance, and is
 * centred in the line: .fnl glyphs fill the line height, FreeType glyph
 * blocks start at the top of the em.
 */
static float fnl_translation_em(struct fnl_font_size *size)
{
	float em = fnl_font_size_char(&size->super, 0x889F); // 亜
	return em > 0 ? em : size->super.size;
}

static struct font_size *fnl_translation_size(struct fnl_font_size *size, struct font *font)
{
	return font->get_size(font, fnl_translation_em(size));
}

static bool fnl_translation_glyph(struct fnl_font_size *size, struct glyph *glyph,
		uint32_t unicode, enum font_weight weight)
{
	struct font *font = translation_font();
	if (!font)
		return false;
	struct font_size *fs = fnl_translation_size(size, font);
	if (!font->get_glyph(fs, glyph, unicode, weight))
		return false;
	glyph->rect.y -= lroundf((size->super.size - fnl_translation_em(size)) / 2);
	return true;
}

static bool fnl_font_get_glyph(struct font_size *_size, struct glyph *glyph, uint32_t code, enum font_weight weight)
{
	struct fnl_font_size *size = (struct fnl_font_size*)_size;
	uint32_t unicode = translation_sjis_to_unicode(code);
	if (unicode)
		return fnl_translation_glyph(size, glyph, unicode, weight);
	unsigned index = fnl_char_to_index(code);
	if (index > size->bitmap_size->nr_glyphs)
		return false;
	if (!size->bitmap_size->face->glyphs[index].data_pos)
		return false;

	struct fnl_bitmap_glyph *fullsize = fnl_get_bitmap_glyph(size->bitmap_size, index);
	const unsigned block_width = fullsize->width / size->denominator;
	const unsigned block_height = fullsize->height / size->denominator;
	const unsigned width = block_width + GLYPH_BORDER_SIZE*2;
	const unsigned height = block_height + GLYPH_BORDER_SIZE*2;
	const unsigned off_x = GLYPH_BORDER_SIZE;
	const unsigned off_y = GLYPH_BORDER_SIZE;

	// Sample each pixel in size->denominator`-sized blocks and compute the average.
	// TODO: no need to sample every pixel; 4 should be fine?
	uint8_t *pixels = xcalloc(1, width * height);
	unsigned *acc = xcalloc(block_width * block_height, sizeof(unsigned));
	for (unsigned i = 0; i < block_width * block_height; i++) {
		unsigned dst_row = i / block_width;
		unsigned dst_col = i % block_width;
		for (unsigned r = 0; r < size->denominator; r++) {
			unsigned src_row = dst_row * size->denominator + r;
			for (unsigned c = 0; c < size->denominator; c++) {
				unsigned src_col = dst_col * size->denominator + c;
				acc[i] += fullsize->pixels[src_row*fullsize->width + src_col];
			}
		}
		uint8_t p = acc[i] / (size->denominator * size->denominator);
		pixels[(dst_row+off_y)*width + (dst_col+off_x)] = p;
	}

	gfx_init_texture_rmap(&glyph->t[weight], width, height, pixels);
	glyph->rect.x = off_x;
	glyph->rect.y = off_y;
	glyph->rect.w = block_width;
	glyph->rect.h = block_height;
	glyph->advance = fullsize->advance / (float)size->denominator;

	free(pixels);
	free(acc);
	return true;
}

static float fnl_font_size_char(struct font_size *_size, uint32_t code)
{
	struct fnl_font_size *size = (struct fnl_font_size*)_size;
	uint32_t unicode = translation_sjis_to_unicode(code);
	if (unicode && translation_font()) {
		struct font *font = translation_font();
		return font->size_char(fnl_translation_size(size, font), unicode);
	}
	unsigned index = fnl_char_to_index(code);
	if (index > size->bitmap_size->nr_glyphs)
		return 0.0;
	if (!size->bitmap_size->face->glyphs[index].data_pos)
		return 0.0;
	float r = (float)size->bitmap_size->face->glyphs[index].width / (float)size->denominator;
	if (code == '\t')
		r *= 2.88;
	return r;
}

static float fnl_font_size_char_kerning(struct font_size *size, uint32_t code,
		uint32_t code_next)
{
	return fnl_font_size_char(size, code);
}

struct font *fnl_font_load(struct fnl *lib, unsigned index)
{
	if (index >= lib->nr_fonts)
		return NULL;

	struct fnl_font *lib_font = &lib->fonts[index];
	struct font_fnl *font = xcalloc(1, sizeof(struct font_fnl));

	// initialize bitmap sizes
	font->nr_bitmap_sizes = lib_font->nr_faces;
	font->bitmap_sizes = xcalloc(font->nr_bitmap_sizes, sizeof(struct fnl_bitmap_size));
	for (unsigned i = 0; i < font->nr_bitmap_sizes; i++) {
		struct fnl_bitmap_size *bitsize = &font->bitmap_sizes[i];
		bitsize->face = &lib_font->faces[i];
		bitsize->nr_glyphs = bitsize->face->nr_glyphs;
		bitsize->glyphs = xcalloc(bitsize->nr_glyphs, sizeof(struct fnl_bitmap_glyph*));
	}

	// initialize render sizes (downscaled)
	font->nr_sizes = font->nr_bitmap_sizes * 12;
	font->sizes = xcalloc(font->nr_sizes, sizeof(struct fnl_font_size));
	for (unsigned i = 0; i < font->nr_bitmap_sizes; i++) {
		struct fnl_bitmap_size *bitsize = &font->bitmap_sizes[i];
		struct fnl_font_size *sizes = &font->sizes[i*12];
		for (unsigned i = 0; i < 12; i++) {
			sizes[i].bitmap_size = bitsize;
			sizes[i].super.size = bitsize->face->height / (float)(i+1);
			sizes[i].super.font = &font->super;
			sizes[i].denominator = i + 1;
		}
	}

	font->super.charmap = CHARMAP_SJIS;
	font->super.get_size = fnl_font_get_size;
	font->super.get_actual_size = fnl_font_get_actual_size;
	font->super.get_actual_size_round_down = fnl_font_get_actual_size_round_down;
	font->super.get_glyph = fnl_font_get_glyph;
	font->super.size_char = fnl_font_size_char;
	font->super.size_char_kerning = fnl_font_size_char_kerning;

	if (!game_rance7_mg)
		gfx_text_advance_edges = true;

	return &font->super;
}
