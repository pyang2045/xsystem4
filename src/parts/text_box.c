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

/*
 * An activity's テキストボックス (EPartsType 4, profile.parts.text_box): one
 * line of text the player types. は～とふるママン's save confirmation has
 * one, テキストボックス：コメント編集: the script shows it, gives it the
 * focus (SetFocus) and, on its Fixed event, reads GetTextBoxText.
 *
 * The parts draws as a text state (state 1) over a background with a one
 * pixel frame, the parts' size from the activity data. While it has the
 * focus, is shown and is not read-only, SDL text input is on: the text the
 * input method commits is appended (up to 最大文字数 characters), the text
 * it is composing is drawn after it, Backspace takes the last character and
 * Return sends the Fixed message (PARTS_MSG_FIXED) and gives the focus up.
 * The focus also goes when the parts is hidden or released.
 */

#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include "system4.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "gfx/gfx.h"
#include "input.h"
#include "xsystem4.h"
#include "parts.h"
#include "parts_internal.h"
#include "profile.h"


// The parts with the keyboard focus, 0 for none.
int parts_text_box_focus_no;
#define focus_no parts_text_box_focus_no

static bool text_box_shown(struct parts *parts)
{
	return parts->global.show && parts->local.show;
}

struct parts_text_box *parts_text_box_get(struct parts *parts)
{
	return parts ? parts->text_box : NULL;
}

static void text_box_redraw(struct parts *parts)
{
	struct parts_text_box *tb = parts->text_box;
	struct string *s = string_dup(tb->text);
	if (tb->editing && tb->editing->size)
		string_append(&s, tb->editing);
	PE_SetText(parts->no, s, 1);
	free_string(s);
	// The parts keeps its own size: a click anywhere on the box is its.
	parts_set_dims(parts, &parts->states[0].text.common, tb->w, tb->h);
	parts_dirty(parts);
}

static void text_box_input_end(void);

static void text_box_unfocus(void)
{
	if (!focus_no)
		return;
	struct parts *parts = parts_try_get(focus_no);
	focus_no = 0;
	text_box_input_end();
	if (parts && parts->text_box && parts->text_box->editing) {
		free_string(parts->text_box->editing);
		parts->text_box->editing = NULL;
		text_box_redraw(parts);
	}
}

struct parts *parts_text_box_focused(void)
{
	if (!focus_no)
		return NULL;
	struct parts *parts = parts_try_get(focus_no);
	if (!parts || !parts->text_box || parts->text_box->read_only || !text_box_shown(parts)) {
		text_box_unfocus();
		return NULL;
	}
	return parts;
}

static void on_text(const char *utf8)
{
	struct parts *parts = parts_text_box_focused();
	if (!parts)
		return;
	struct parts_text_box *tb = parts->text_box;
	char *game = utf8_to_game(utf8, 0);
	// One character at a time, so that 最大文字数 cuts between characters.
	for (const char *p = game; *p; ) {
		int n = SJIS_2BYTE(*p) && p[1] ? 2 : 1;
		if (game_charset_is_gbk())
			n = ((unsigned char)*p >= 0x81 && p[1]) ? 2 : 1;
		if (tb->max_chars > 0 && mbcs_count_char(tb->text->text) >= tb->max_chars)
			break;
		string_append_cstr(&tb->text, p, n);
		p += n;
	}
	free(game);
	if (tb->editing) {
		free_string(tb->editing);
		tb->editing = NULL;
	}
	text_box_redraw(parts);
}

static void on_editing(const char *utf8, possibly_unused int start, possibly_unused int length)
{
	struct parts *parts = parts_text_box_focused();
	if (!parts)
		return;
	struct parts_text_box *tb = parts->text_box;
	if (tb->editing)
		free_string(tb->editing);
	tb->editing = NULL;
	if (*utf8) {
		char *game = utf8_to_game(utf8, 0);
		tb->editing = cstr_to_string(game);
		free(game);
	}
	text_box_redraw(parts);
}

static void on_key(int code)
{
	struct parts *parts = parts_text_box_focused();
	if (!parts)
		return;
	struct parts_text_box *tb = parts->text_box;
	// While the input method composes, its keys are its own.
	if (tb->editing && tb->editing->size)
		return;
	if (code == VK_BACK && tb->text->size) {
		game_charset_is_gbk() ? string_pop_back(&tb->text) : string_pop_back_sjis(&tb->text);
		text_box_redraw(parts);
	} else if (code == VK_RETURN) {
		text_box_unfocus();
		parts_msg_push(parts, PARTS_MSG_FIXED, "");
	}
}

static void text_box_input_begin(void)
{
	register_input_handler(on_text);
	register_editing_handler(on_editing);
	register_key_handler(on_key);
	SDL_StartTextInput();
}

static void text_box_input_end(void)
{
	SDL_StopTextInput();
	clear_input_handler();
	clear_editing_handler();
	clear_key_handler();
}

void parts_text_box_create(struct parts *parts, int w, int h, SDL_Color bg,
		SDL_Color read_only_bg, SDL_Color frame, int max_chars, bool read_only)
{
	struct parts_text_box *tb = parts->text_box;
	if (!tb) {
		tb = xcalloc(1, sizeof(struct parts_text_box));
		tb->text = string_ref(&EMPTY_STRING);
		parts->text_box = tb;
	} else if (tb->textures_valid) {
		gfx_delete_texture(&tb->frame_t);
		gfx_delete_texture(&tb->bg_t);
		gfx_delete_texture(&tb->caret_t);
		tb->textures_valid = false;
	}
	tb->w = w > 2 ? w : 2;
	tb->h = h > 2 ? h : 2;
	tb->bg = bg;
	tb->read_only_bg = read_only_bg;
	tb->frame = frame;
	tb->max_chars = max_chars;
	tb->read_only = read_only;
	text_box_redraw(parts);
}

void parts_text_box_free(struct parts *parts)
{
	struct parts_text_box *tb = parts->text_box;
	if (!tb)
		return;
	if (focus_no == parts->no)
		text_box_unfocus();
	if (tb->textures_valid) {
		gfx_delete_texture(&tb->frame_t);
		gfx_delete_texture(&tb->bg_t);
		gfx_delete_texture(&tb->caret_t);
	}
	free_string(tb->text);
	if (tb->editing)
		free_string(tb->editing);
	free(tb);
	parts->text_box = NULL;
}

/* SetFocus(Number): a text box takes the keyboard, anything else lets it go. */
void parts_text_box_set_focus(int parts_no)
{
	if (parts_no == focus_no)
		return;
	text_box_unfocus();
	struct parts *parts = parts_try_get(parts_no);
	if (!parts || !parts->text_box || parts->text_box->read_only)
		return;
	focus_no = parts_no;
	text_box_input_begin();
}

bool parts_text_box_is_focus(int parts_no)
{
	return focus_no && focus_no == parts_no && parts_text_box_focused();
}

struct string *parts_text_box_get_text(int parts_no)
{
	struct parts *parts = parts_try_get(parts_no);
	if (!parts || !parts->text_box)
		return string_ref(&EMPTY_STRING);
	return string_dup(parts->text_box->text);
}

void parts_text_box_set_text(int parts_no, struct string *text)
{
	struct parts *parts = parts_try_get(parts_no);
	if (!parts || !parts->text_box)
		return;
	free_string(parts->text_box->text);
	parts->text_box->text = string_dup(text);
	text_box_redraw(parts);
}

void parts_text_box_set_read_only(int parts_no, bool read_only)
{
	struct parts *parts = parts_try_get(parts_no);
	if (!parts || !parts->text_box)
		return;
	parts->text_box->read_only = read_only;
	if (read_only && focus_no == parts_no)
		text_box_unfocus();
	parts_dirty(parts);
}

bool parts_text_box_is_read_only(int parts_no)
{
	struct parts *parts = parts_try_get(parts_no);
	return parts && parts->text_box && parts->text_box->read_only;
}

void parts_text_box_set_max_chars(int parts_no, int max_chars)
{
	struct parts *parts = parts_try_get(parts_no);
	if (parts && parts->text_box)
		parts->text_box->max_chars = max_chars;
}

int parts_text_box_get_max_chars(int parts_no)
{
	struct parts *parts = parts_try_get(parts_no);
	return parts && parts->text_box ? parts->text_box->max_chars : 0;
}

/* Each frame: a focused box that was hidden lets the keyboard go. */
void parts_text_box_update(void)
{
	if (focus_no)
		parts_text_box_focused();
}
