/* TextSurfaceManager — v14 HLL for text measurement.
 * Dohna Dohna uses this to measure font glyph widths for layout. */

#include <math.h>

#include "system4/ain.h"
#include "system4/string.h"
#include "gfx/font.h"
#include "hll.h"
#include "xsystem4.h"

/* GetFontWidth(Text, &Width, Type, Size, R, G, B, BoldWeight, EdgeWeight, EdgeR, EdgeG, EdgeB) -> bool
 * Measures the pixel width of the given text string with the specified font properties.
 *
 * The CN (GBK) build without an .fnl returns the cell its renderer draws
 * (0x69fb30 -> 0x69c7a0; see gfx_text_cn_gdi): the width of the first
 * character plus 2e, where the edge e also counts the 太さ; an empty text is
 * 2e. CASFont@GetFontWidth passes one character at a time.
 *
 * Other games keep the byte-level heuristic (half-width for ASCII, full-width
 * for multi-byte). */
static bool TextSurfaceManager_GetFontWidth(struct string *text, int *width,
		int type, int size, int r, int g, int b,
		float bold_weight, float edge_weight,
		int edge_r, int edge_g, int edge_b)
{
	if (gfx_text_cn_gdi()) {
		int w = text && text->size ? gfx_text_cn_width(size, text->text[0]) : 0;
		if (width)
			*width = w + 2 * gfx_text_cn_edge(size, bold_weight, edge_weight);
		return true;
	}
	if (!text || size <= 0 || text->size == 0) {
		if (width) *width = 0;
		return true;
	}

	float w = 0.0f;
	const unsigned char *p = (const unsigned char *)text->text;
	while (*p) {
		if (*p <= 0x7f) {
			w += (float)size / 2.0f;
			p++;
		} else if (*p >= 0x81 && *p <= 0xFE && *(p+1)) {
			w += (float)size;
			p += 2;
		} else {
			w += (float)size / 2.0f;
			p++;
		}
	}
	if (width)
		*width = (int)w;
	return true;
}

/*
 * ain v7 (妻みぐい３): the font is set with SetFontType and the other
 * setters, and GetFontWidth(string Text, ref int Width) measures with it.
 * The message draws each character as a text parts of its own and places
 * the next one by this width, so it is measured with the font the parts
 * draw with: the advances the text parts add up (gfx_size_text), without
 * the character spacing, which the script adds.
 */
static struct text_style v7_style = {
	.face = 0,
	.size = 16,
	.color = { 255, 255, 255, 255 },
	.edge_color = { 0, 0, 0, 255 },
	.scale_x = 1.0f,
	.space_scale_x = 1.0f,
};

static void TextSurfaceManager_SetFontType(int type)
{
	v7_style.face = type;
	v7_style.font_size = NULL;
}

static void TextSurfaceManager_SetFontSize(int size)
{
	v7_style.size = size;
	v7_style.font_size = NULL;
}

static void TextSurfaceManager_SetFontColor(int r, int g, int b)
{
	v7_style.color = (SDL_Color){ r, g, b, 255 };
}

static void TextSurfaceManager_SetFontBoldWeight(float bold_weight)
{
	v7_style.weight = bold_weight * 1000;
	v7_style.bold_weight = bold_weight;
}

static void TextSurfaceManager_SetFontEdgeWeight(float edge_weight)
{
	text_style_set_edge_width(&v7_style, edge_weight);
}

static void TextSurfaceManager_SetFontEdgeColor(int r, int g, int b)
{
	v7_style.edge_color = (SDL_Color){ r, g, b, 255 };
}

static bool TextSurfaceManager_v7_GetFontWidth(struct string *text, int *width)
{
	int w = text && text->size ? (int)ceilf(gfx_size_text(&v7_style, text->text)) : 0;
	if (width)
		*width = w;
	return true;
}

extern struct static_library lib_TextSurfaceManager;
static void TextSurfaceManager_PreLink(void)
{
	int libno = ain_get_library(ain, "TextSurfaceManager");
	if (libno < 0)
		return;
	int fno = ain_get_library_function(ain, libno, "GetFontWidth");
	if (fno >= 0 && ain->libraries[libno].functions[fno].nr_arguments == 2)
		static_library_replace(&lib_TextSurfaceManager, "GetFontWidth",
				TextSurfaceManager_v7_GetFontWidth);
}

HLL_LIBRARY(TextSurfaceManager,
	    HLL_EXPORT(_PreLink, TextSurfaceManager_PreLink),
	    HLL_EXPORT(SetFontType, TextSurfaceManager_SetFontType),
	    HLL_EXPORT(SetFontSize, TextSurfaceManager_SetFontSize),
	    HLL_EXPORT(SetFontColor, TextSurfaceManager_SetFontColor),
	    HLL_EXPORT(SetFontBoldWeight, TextSurfaceManager_SetFontBoldWeight),
	    HLL_EXPORT(SetFontEdgeWeight, TextSurfaceManager_SetFontEdgeWeight),
	    HLL_EXPORT(SetFontEdgeColor, TextSurfaceManager_SetFontEdgeColor),
	    HLL_EXPORT(GetFontWidth, TextSurfaceManager_GetFontWidth));
