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

#include <limits.h>
#include <math.h>
#include <cglm/cglm.h>

#include "system4.h"

#include "asset_manager.h"
#include "audio.h"
#include "input.h"
#include "parts.h"
#include "xsystem4.h"
#include "parts_internal.h"
#include "system4/ain.h"
#include "system4/string.h"
#include "vm.h"
#include "xsystem4.h"
#include "profile.h"

// true between calls to BeginClick and EndClick
bool parts_began_click = false;
// v14 BeginInput/EndInput scopes can nest inside synchronous scene callbacks.
static unsigned input_depth;

// the mouse position at last update
static Point parts_prev_pos = {0};
// true when the left mouse button was down last update
static bool prev_clicking = false;
// the last (fully) clicked parts number
static int clicked_parts = 0;
// the current (partially) clicked parts number
static int click_down_parts = 0;
// ain v7: KeyPress repeating while the button or key is held.
#define KEY_REPEAT_DELAY_MS 500
#define KEY_REPEAT_INTERVAL_MS 100
static int click_repeat_ms;
// v14: the parts under the cursor when the left button went down within the
// scope, the one its release clicks: the original's left slot (+0x1ec),
// which holds it from the press until the update of the release; see
// v14_update_input
static int v14_press_parts;
// v14: the parts under the cursor at the last left press within the scope,
// standing in for the original's focus (+0x1b4 of the element, set by
// 0x546890 at a press only). It is not one of the manager's slots: it stays
// after the release and after the end of a scope, until a press elsewhere
// or a BeginInput (LostFocus); GetActiveParts reports it within a scope
// (PE_GetActivePartsNumber)
static int v14_active_parts;
void parts_enqueue_message_vars(int type, int parts_no, int delegate_index,
		int unique_id, int nr_vars, const int *vars);

// drag state
static struct parts *drag_parts = NULL;
static bool is_dragging = false;
static Point drag_initial_pos;
static Point drag_start_cursor;
static struct parts *drop_target = NULL;

bool parts_screen_to_box(struct parts *parts, struct parts_common *c, float sx, float sy,
		float *bx, float *by)
{
	return profile.parts.transforms->screen_to_box(parts, c, sx, sy, bx, by);
}

/*
 * The original's point-in-triangle test (0x57aa60): the point is on the
 * centroid's side of each edge, or on the edge. Three equal points hold
 * nothing; three points on one line hold every point (each side value of
 * the centroid is 0).
 */
static bool parts_point_in_triangle(const float p1[2], const float p2[2], const float p3[2],
		float mx, float my)
{
	if (p1[0] == p2[0] && p1[1] == p2[1] && p1[0] == p3[0] && p1[1] == p3[1])
		return false;
	float gx = (p2[0] + p1[0] + p3[0]) * (1.0f / 3.0f);
	float gy = (p2[1] + p1[1] + p3[1]) * (1.0f / 3.0f);
	float e = p1[1] - p2[1];
	if (((p1[0] - gx) * e + (gy - p1[1]) * (p1[0] - p2[0]))
			* ((p1[0] - mx) * e + (my - p1[1]) * (p1[0] - p2[0])) < 0.0f)
		return false;
	e = p2[1] - p3[1];
	if (((gy - p2[1]) * (p2[0] - p3[0]) + (p2[0] - gx) * e)
			* ((my - p2[1]) * (p2[0] - p3[0]) + (p2[0] - mx) * e) < 0.0f)
		return false;
	e = p3[1] - p1[1];
	return ((gy - p3[1]) * (p3[0] - p1[0]) + (p3[0] - gx) * e)
		* ((my - p3[1]) * (p3[0] - p1[0]) + (p3[0] - mx) * e) >= 0.0f;
}

/*
 * The rectangle hit test of a box with depth (an X or Y rotation along the
 * chain): the four corners of the box through its matrix and the camera
 * used for drawing, and the point in the triangle of the first three
 * corners or in the one of the last three (top-left, top-right,
 * bottom-left; bottom-left, top-right, bottom-right), with the triangle
 * test of 0x57aa60. This is the port's choice for every kind of parts. The
 * original differs by kind: text (21) and hit-rectangle (25) parts go
 * through 0x57a6b0 -> 0x5c3910, which projects without the drawing's
 * centre shift, so their vanishing point is pixel (0,0); CG parts (19)
 * take the bounding rectangle of the stored drawn corners (0x59c460 ->
 * 0x5b11d0 -> 0x5b13d0). The game has no clickable parts that turn, so
 * the difference was not seen. Boxes without depth keep the upright box at
 * the anchor (parts_screen_hitbox).
 */
static bool parts_projected_box_hit(struct parts *parts, struct parts_common *c, mat4 m, Point pos)
{
	if (c->w <= 0 || c->h <= 0)
		return false;
	// The box of the hit rectangle in the box frame of parts_box_transform
	// (the rectangle may be set apart from the texture's size).
	float x0 = c->hitbox.x - parts->local.pos.x - c->origin_offset.x;
	float y0 = c->hitbox.y - parts->local.pos.y - c->origin_offset.y;
	float x1 = x0 + c->hitbox.w, y1 = y0 + c->hitbox.h;
	const float corner_box[4][2] = { { x0, y0 }, { x1, y0 }, { x0, y1 }, { x1, y1 } };
	float corner[4][2];
	for (int i = 0; i < 4; i++) {
		vec4 p;
		glm_mat4_mulv(m, (vec4){ corner_box[i][0], corner_box[i][1], 0.0f, 1.0f }, p);
		parts_project_point(p, &corner[i][0], &corner[i][1]);
	}
	return parts_point_in_triangle(corner[0], corner[1], corner[2], pos.x, pos.y)
		|| parts_point_in_triangle(corner[2], corner[1], corner[3], pos.x, pos.y);
}

static bool parts_hittest(struct parts *parts, int state, Point pos)
{
	// v14: a layout box is never under the cursor (its widget's hit test,
	// vtable +0x4c = 0x49c5b0, returns false); its state now has the box's
	// size (layoutbox.c).
	if (profile.parts.activity_widgets && parts->states[0].type == PARTS_LAYOUT_BOX)
		return false;
	// v14: a parts with an alpha clipper (+0x2fc) is under the cursor only
	// where its clipper is, the clipper tested by its pixels (CGUIPartsView's
	// hit test 0x568df0 calls the clipper's with +0x1ad, the pixel decision,
	// set). So Haharanman's title film filter, a full-screen CG clipped to
	// the film frame, does not cover the menu buttons.
	if (profile.parts.activity_widgets && parts->alpha_clipper_parts_no
			&& parts->alpha_clipper_parts_no != parts->no) {
		static int depth;	// a clipper clipped by its own clippee
		struct parts *clipper = parts_try_get(parts->alpha_clipper_parts_no);
		if (clipper && depth < 4) {
			bool pixel = clipper->pixel_hittest;
			clipper->pixel_hittest = true;
			depth++;
			bool hit = parts_hittest(clipper, PARTS_STATE_DEFAULT, pos);
			depth--;
			clipper->pixel_hittest = pixel;
			if (!hit)
				return false;
		}
	}
	struct parts_common *c = &parts->states[state].common;
	bool use_pixels = parts->pixel_hittest && c->texture.handle && c->texture.w > 0
		&& c->texture.h > 0 && c->w > 0 && c->h > 0;
	if (profile.parts.native_transforms) {
		mat4 m;
		parts_box_transform(parts, c, m);
		if (!parts_matrix_is_flat(m) && !parts_projected_box_hit(parts, c, m, pos))
			return false;
		if (!parts_matrix_is_flat(m) && !use_pixels)
			return true;
	}
	Rectangle hitbox = parts_screen_hitbox(parts, c);
	if (!use_pixels)
		return SDL_PointInRect(&pos, &hitbox);

	// Use the full image origin: surface_area clips pixels rather than
	// scaling the complete image into the smaller hit rectangle. A mirrored
	// box maps a pixel's left edge to the right edge of a texel, so sample
	// the pixel centre there, as the rasterizer does; unmirrored parts keep
	// the previous corner sampling (the same texel at scale 1).
	bool mirrored = parts->global.reverse_lr || parts->global.reverse_tb
		|| parts->local.reverse_lr || parts->local.reverse_tb;
	float half = mirrored ? 0.5f : 0.0f, x, y;
	if (!parts_screen_to_box(parts, c, (float)pos.x + half, (float)pos.y + half, &x, &y))
		return false;
	if (!isfinite(x) || !isfinite(y) || x < 0 || y < 0 || x >= c->w || y >= c->h)
		return false;
	int tx = (int)((double)x * c->texture.w / c->w);
	int ty = (int)((double)y * c->texture.h / c->h);
	const int deform = parts_cg_deform(parts, state);
	if (deform == 1)
		tx = c->texture.w - 1 - tx;
	else if (deform == 2)
		ty = c->texture.h - 1 - ty;
	if (c->surface_area.w || c->surface_area.h) {
		Point texel = { tx, ty };
		if (!SDL_PointInRect(&texel, &c->surface_area))
			return false;
	}

	// A CG changes only through its setter, which invalidates this mask.
	// Other parts (gauges, animation, movies, construction) can mutate their
	// texture in place, so sample them directly instead of caching stale alpha.
	if (parts->states[state].type != PARTS_CG)
		return gfx_get_pixel(&c->texture, tx, ty).a != 0;
	if (!c->hit_mask) {
		uint8_t *pixels = gfx_get_pixels(&c->texture);
		if (!pixels)
			return true;
		size_t count = (size_t)c->texture.w * c->texture.h;
		c->hit_mask = xmalloc(count);
		for (size_t i = 0; i < count; i++)
			c->hit_mask[i] = pixels[i * 4 + 3];
		free(pixels);
	}
	return c->hit_mask[(size_t)ty * c->texture.w + tx] != 0;
}
static void drag_state_reset(void)
{
	drag_parts = NULL;
	is_dragging = false;
	drop_target = NULL;
}

/* The parts is being released: the input forgets it (v14: the original's
 * 0x53c140 drops a released parts from the scope's active parts). */
void parts_input_release(struct parts *parts)
{
	if (parts == drag_parts)
		drag_state_reset();
	else if (parts == drop_target)
		drop_target = NULL;
	// Programs before v14 keep a press on a released parts as it was.
	if (!profile.parts.input_sessions)
		return;
	if (parts->no == click_down_parts)
		click_down_parts = 0;
	if (parts->no == v14_press_parts)
		v14_press_parts = 0;
	if (parts->no == v14_active_parts)
		v14_active_parts = 0;
}

/* A disabled v14 button has no cursor or click sound: native 0x5285b0 empties
 * the inner parts' sound names every frame while +0xb0 is false. */
static int parts_cursor_sound(struct parts *parts)
{
	return parts->button_disabled ? -1 : parts->on_cursor_sound;
}

static int parts_click_sound(struct parts *parts)
{
	return parts->button_disabled ? -1 : parts->on_click_sound;
}

/*
 * v14: the input update reads one controller, the script's layer. The parts
 * engine's update (0x53e690) runs the update of every controller of the
 * stack (+0x88..+0x8c) and of the system overlay (+0x84), 0x53c5b0, and
 * gives the input flag to one of them: the designated active controller
 * (+0x94) or, while none is designated, the top of the stack (0x53e640);
 * the overlay only while it is the designated one (and only while +0xb4 is
 * set, SetEnableInput, which the game never calls). Only that controller
 * runs the input update (0x547c90 from 0x53c64d), with its own manager
 * (ctrl+0x18) on the elements of its own tree: 0x5385b0 collects every
 * parts below the controller's root (pre-order), 0x546520 turns them into
 * their widgets' elements. The time since entered (0x546620), the slots
 * (0x547c00), the press target and the focus (0x546890, 0x545c70), the
 * cursor walk (0x5471d0), the local wheel (0x547000) and GetClickNumber
 * (0x547610) go over that list alone. The parts of every other controller
 * are not touched: not entered, not left, no message, their time stands
 * still. So a button under a child scene (a confirmation box opens a layer
 * of its own: SceneContext@0 -> AFL_Parts_AddLayer(-1)) keeps its hover look
 * and, when the box goes with the cursor still on it, is not entered again.
 * GetActiveParts reads the active controller's own focus (0x57bc90 ->
 * 0x53c6c0: +0x280, refreshed for every controller by 0x53c240).
 *
 * Here parts->controller_no follows the parent chain (parts_set_controller),
 * which is the original's "below the controller's root", and the controller
 * is PE_get_active_controller(), the choice of 0x53e690. In the
 * single-controller mode (engines without AddController) every parts is in
 * the scope, as before.
 *
 * The port's strategy, not the original's:
 *  - one input manager for all the controllers (the original has one per
 *    controller): the stored position, the sampled buttons, the slots and
 *    the focus. BeginInput and EndInput reset every one of the original's
 *    managers (0x53e8b0, 0x53e830) as they reset this one, so the two
 *    differ only while the active controller changes between them: e.g. a
 *    controller added inside a scope begins its own manager at once
 *    (0x53d3a0 -> 0x546030), whose first updates send the whole-screen
 *    MouseMove, which is not sent here when the cursor has not moved; and
 *    the old controller's manager keeps its slots and its last buttons for
 *    when it acts again, which here are emptied (v14_drop_slots_out_of_scope)
 *    and shared;
 *  - one focus (v14_active_parts) where the original has a flag on every
 *    element (+0x1b4) and looks it up in the list (0x545c70). The wheel's
 *    fallback and GetActiveParts take it only when it is in the scope; a
 *    press in the scope still sends it LostFocus when it is another
 *    controller's (v14_press), where the original sends that at the next
 *    0x53c750 (BeginInput; a confirmation box's BeginInput comes before any
 *    press in it);
 *  - SetEnableInputProcess (+0x90 false leaves an element out of the list)
 *    and SetEnableInput (+0xb4) stay stubs; the order is the drawing order
 *    (z), not the tree's pre-order as 0x53c830 leaves it (not followed);
 *  - a dragged scroll knob of another controller still follows the cursor
 *    (the original's knob follows its focus' key events, which the other
 *    controller's manager no longer sends; not followed further).
 */
static int v14_scope;	/* the controller of this input update */

static bool v14_in_scope_of(const struct parts *parts, int controller)
{
	return !parts_multi_controller || parts->controller_no == controller;
}

/*
 * v14 (Dohna Dohna): the original's input update (0x547c90, run by the parts
 * update 0x53c5b0 alone, for the active controller alone: v14_in_scope; the
 * update does nothing outside a BeginInput/EndInput scope, 0x547cba). In
 * its order:
 *
 *  0. The slots whose element is not in the controller's list are emptied
 *     silently (0x547c00 -> 0x548530; v14_drop_slots_out_of_scope).
 *  1. Every listed element's time since entered grows by the passed time
 *     (0x546620 at 0x547df2, whether the element is entered or not), and
 *     GetClickNumber is cleared (+0x14 at 0x547e23).
 *  2. The cursor's position is read through the same call as the HLL
 *     Mouse_GetPos (0x547e50; 0x498b95). When it gives none, the update goes
 *     on with the last position it stored (+0x18/+0x1c, 0x547e5f). It sends
 *     no whole-screen move/click (0x548024), takes no press and runs no
 *     cursor walk (0x5482a7 skips 0x546890). Wheel events still go out;
 *     only the left slot's parts is
 *     left (0x548339 -> 0x5476b0 -> 0x577cf0) when it is the focus, that is
 *     while the left button is still held on the parts it went down on. The
 *     slot is empty after the release (6. below), and a release without a
 *     position empties it before (0x5481cb -> 0x5462b0), so a parts clicked
 *     or released is not left here: it stays entered, as the other parts do,
 *     until the pointer is back. Here the position counts as given while
 *     the pointer is in the window (mouse_in_window: the scheduled pointer,
 *     or SDL's mouse focus). What the original's call checks was not
 *     followed (it is behind an interface); the port's Mouse_GetPos also asks
 *     for the keyboard focus, which is not asked here, so that a window in
 *     the background shows the hover as before.
 *  3. Wheel counts read from the input interface (0x547fd1): when either
 *     is nonzero, the whole-screen MouseWheel (forward, back), through
 *     the manager's own emitter at 0x54800f. It precedes the check for a
 *     position and is sent even when the two nonzero counts are equal.
 *  4. With a position, when it is not the one stored: the whole-screen
 *     MouseMove (x, y), through the manager's own emitter (0x548054; its
 *     source is null, so the message's parts number is 0, and its mask
 *     0x54562a leaves type 1 through: of the types 0..29 it lets 1, 3, 4, 5,
 *     9 and 15..18 through). The game hooks it only in its debugging tools
 *     (AFL_Parts_AddWholeMouseMoveEvent).
 *  5. With a position, a release: the whole-screen MouseClick (x, y, key 1),
 *     whatever is under the cursor (0x548064, the same emitter). Also when the
 *     button was already down at BeginInput: 0x546030 samples the buttons
 *     into +0x40..+0x42, and EndInput samples them again for the outer scope
 *     (0x58a750 -> 0x53e830).
 *  6. With a position, 0x546890 (at 0x5482f9): at a press (down now, up at
 *     the last update; 0x547f2d..0x547fb3) the parts under the cursor,
 *     v14_input_target, becomes the pressed parts (the left slot) and the
 *     focus; when the focus changes, the old one gets LostFocus and the new
 *     one Focus (0x546a4e..0x546aa2: 0x80f5a4, 0x80f580, each only when the
 *     element's +0x1b4 says so). Without a press and with the left button
 *     up, the update of the release included, the left slot is emptied
 *     (0x5468d6..0x5468fb -> 0x547460: +0x1ec and the drop target +0x210),
 *     before the click of 8. goes out. Then the cursor walk
 *     (v14_cursor_walk), every update.
 *  7. Local MouseWheel (0x547000 at 0x548362): the first eligible element
 *     under the cursor in reverse order, or the updated focus/root
 *     fallback, receives the same two counts. This also uses the stored
 *     position when the position query failed; see v14_dispatch_wheel.
 *  8. The focused element's keys (0x578580 at 0x54836a): at the press a
 *     clickable entered element goes to its down state (0x5785f8); at the
 *     release the pressed parts' own MouseClick (0x5788d0, key 1), only when
 *     it is clickable (+0x1a4), entered (+0x1ac, this update's walk) and the
 *     position is given (0x578955; without one an entered parts keeps its
 *     state); the
 *     click sound (0x5789bc: the state 3 sound, as Parts_PlaySound(n, 3))
 *     and GetClickNumber come with it. GetClickNumber follows the left
 *     button's click of a parts and nothing else: its source is the element's
 *     +5, written at 0x5789cb in the key 1 branch alone, which 0x547610 looks
 *     up (0x545d40) and stores at +0x14. A parts the cursor left gets nothing
 *     (0x5789dd); dragged back, it is clicked. A button already down at
 *     BeginInput is not a press (and BeginInput clears the focus, 0x53e830 ->
 *     0x53c750), so no parts is clicked and only the whole-screen click goes
 *     out. A pactex-clickable parts without an event gets its MouseClick as
 *     well, which the AIN drops (CPartsMessageManager@CallDelegate); the
 *     whole-screen click is what closes the dialog over PartySkillView's
 *     Button. A check box flips here, after its MouseClick (0x52cdd0 ->
 *     0x52f030: the cursor still on the widget, key 1).
 *  9. The position is stored (+0x18/+0x1c) when this update and the last one
 *     both had one (0x548381..0x548395; +0x43).
 *
 * The original has no "pressed parts" for a click. It sends the parts'
 * click to the focused element (0x548367 calls 0x578580 on it), which finds
 * the released keys in the element's own key table (0x574d50 at 0x57861f)
 * and runs 0x5788d0 for each. Its per-button slots (+0x1ec left, +0x200
 * middle, +0x208 right; 0x546890 empties the three at a press and fills
 * one, in that priority: 0x546988, 0x5469db, 0x5469fe) are the sources of
 * a drag and drop. v14_press_parts stands for "the focus, with the left
 * button gone down on it", the left slot, the same parts as long as only
 * the left button is used, emptied in the update of the release;
 * v14_active_parts for the focus, which stays after the release. Only the
 * slots are reset at the end of a scope (v14_reset_input_session), so a
 * parts just clicked keeps its entered flag and its state there.
 *
 * The right and the middle button are not done: here their press and their
 * release do nothing to the parts' input, as before the left button was
 * reworked. In the original
 *  - a release sends the whole-screen click with key 2 or 4 (0x5480be,
 *    0x5480fb) and the focused element's MouseClick with that key (0x5788d0
 *    from 0x5789e8: clickable and the cursor on it), with no sound and
 *    without GetClickNumber (that branch does not write +5);
 *  - a press of any of the three moves the focus (0x546a1f..0x546aa2), with
 *    its Focus and LostFocus; here only a left press does;
 *  - a right or middle press with a parts in the left slot calls 0x5462b0
 *    (0x54816d), which cancels the drag and drop: it puts a draggable
 *    parts back where it was (0x5462d8), sends the drag's events (0x5781b0,
 *    and 0x80ffa8 for an element being dragged, +0x1e0), leaves the drop
 *    target (0x5784b0), empties the slot and sets +0x1fc (0x5463ad). While
 *    +0x1fc is set, until all three buttons are up (0x548206), the update
 *    skips 0x546890 (0x548278), so no new press is taken and no cursor walk
 *    runs. 0x5462b0 writes neither the focus nor the element's key table,
 *    so, read statically, the left button's click of the focused parts
 *    still goes out (not compared with the original running).
 * Doing them right takes the focus and the key table as the model and a
 * comparison with the original; that is left to a later round.
 *
 * A disabled button (SetButtonEnable false) gets no MouseClick, no
 * GetClickNumber and no sound here. This is the port's expedient and has no
 * counterpart in the original's input update: the button's per-frame update
 * makes its inner parts clickable whatever the enable flag (0x528690, ahead
 * of the read of +0xb0 at 0x5286df), and a disabled button only has its two
 * sound names emptied (the branch at 0x528787), so 0x5788d0 still sends
 * that parts its MouseClick and sets its +5. The original does not react to
 * its greyed buttons (成员/商店 on the base screen) while the AIN click
 * handlers do not check Enable, so the click is presumably stopped in
 * another layer; which one was not looked up. The whole-screen click is
 * sent as for any release. The hover of a disabled button is sent as for
 * any parts (0x577b90 does not ask).
 *
 * Not done: the KeyTrigger/KeyDown/KeyPress/KeyUp messages (types 15-18,
 * 0x578580), the double click (0x4e6bd0), the drag and drop messages and
 * the swipe (an element in a swipe, +0x1dc not negative, stops the walk and
 * keeps its focus out of it: 0x5472e9, 0x548294).
 */
static bool v14_prev_have_pos;

void PE_UpdateInputState(int passed_time)
{
	profile.parts.input->update_input_state(passed_time);
}

void PE_SetPassCursor(int parts_no, bool pass)
{
	parts_get(parts_no)->pass_cursor = !!pass;
}

bool PE_GetPartsPassCursor(int parts_no)
{
	return parts_get(parts_no)->pass_cursor;
}

void PE_SetClickable(int parts_no, bool clickable)
{
	parts_get(parts_no)->clickable = !!clickable;
}

bool PE_GetPartsClickable(int parts_no)
{
	return parts_get(parts_no)->clickable;
}

void PE_SetPartsGroupDecideOnCursor(possibly_unused int group_no, possibly_unused bool decide_on_cursor)
{
	UNIMPLEMENTED("(%d, %s)", group_no, decide_on_cursor ? "true" : "false");
}

void PE_SetPartsGroupDecideClick(possibly_unused int group_no, possibly_unused bool decide_click)
{
	UNIMPLEMENTED("(%d, %s)", group_no, decide_click ? "true" : "false");
}

/*
 * v14 オン指針表示連動: a parts shown while another (its link) is entered.
 * The original keeps the link's number at the component's +0x124, 0 for
 * none (the constructor, 0x550927): Parts_SetOnCursorShowLinkPartsNumber
 * (0x58e6d0) gets or makes the component and writes the number (and +0xb4,
 * its fade, 0), Parts_GetOnCursorShowLinkPartsNumber (0x58e770) reads it,
 * 0 without a component; the pactex loader resolves the name of the key in
 * the activity (pactex_resolve_show_links). The link is a number: nothing
 * is made for it, and a number without a parts shows the parts as if it
 * had no link (0x535c90; see parts_link_shown). Here no link is -1, as for
 * the engines before v14 (linked_to), and a number of 0 or less is no link;
 * linked_from is the last parts linked to a target (the input marks the
 * screen dirty through it). The original fades the parts in and out over
 * +0x44 (100) ms; this port shows and hides it at once.
 */
void parts_set_show_link(struct parts *parts, int link_parts_no)
{
	struct parts *old = parts->linked_to > 0 ? parts_try_get(parts->linked_to) : NULL;
	if (old && old->linked_from == parts->no)
		old->linked_from = -1;
	parts->linked_to = link_parts_no > 0 ? link_parts_no : -1;
	struct parts *target = link_parts_no > 0 ? parts_try_get(link_parts_no) : NULL;
	if (target)
		target->linked_from = parts->no;
	parts_dirty(parts);
}

void PE_SetOnCursorShowLinkPartsNumber(int parts_no, int link_parts_no)
{
	profile.parts.input->set_show_link(parts_no, link_parts_no);
}

/* The engines before v14: -1 is no link. */
int PE_GetOnCursorShowLinkPartsNumber(int parts_no)
{
	return parts_get(parts_no)->linked_to;
}

/* v14 (0x58e770): 0 is no link, and a number without a parts reads 0. */
int PE_v14_GetOnCursorShowLinkPartsNumber(int parts_no)
{
	struct parts *parts = parts_no > 0 ? parts_try_get(parts_no) : NULL;
	return parts && parts->linked_to > 0 ? parts->linked_to : 0;
}

/*
 * v14 sound names. A component has four names (+0x1b0 + i * 0x18).
 * Parts_SetSoundName (0x590230) and Parts_GetSoundName (0x5902e0) write and
 * read slot State of the component the get-or-create lookup gives (0x540250:
 * none for a number of 0 or less), without looking at the name or the
 * State. The original plays a name by name: its low-level parts copy the
 * slots 1..3 at their update (0x565200 -> 0x5b7f20, a name that differs
 * replaces the old one and frees its sound), slots 2 and 3 get a CSound when
 * not empty (0x5b7590 -> 0x5bac00), which plays through PlaySe(3, name, "")
 * (0x5ba830); the enter plays slot 2 (0x564f20), the release that clicks
 * plays slot 3 (0x5b7fc0).
 *
 * Here (a port's strategy): a name of slot 2 or 3 is made the sound
 * archive's number of that name when it is set, on_cursor_sound and
 * on_click_sound, which the input plays as before (audio_play_sound); an
 * empty name, or one the archive does not have, is -1 and plays nothing
 * (the original's PlaySe of such a name fails, silently). A State outside
 * 0..3 writes nothing and reads "" (the original indexes past its slots).
 * The number API of the engines before v14 writes the numbers alone.
 */
static int parts_sound_name_number(const struct string *name)
{
	int no = -1;
	if (!name || !name->size || !asset_exists_by_name(ASSET_SOUND, name->text, &no) || no < 0)
		return -1;
	return no;
}

void parts_set_sound_name(struct parts *parts, int slot, const char *text, size_t len)
{
	if (slot < 0 || slot >= PARTS_NR_SOUND_NAMES)
		return;
	if (parts->sound_names[slot])
		free_string(parts->sound_names[slot]);
	parts->sound_names[slot] = text && len ? make_string(text, len) : NULL;
	if (slot == 2)
		parts->on_cursor_sound = parts_sound_name_number(parts->sound_names[slot]);
	else if (slot == 3)
		parts->on_click_sound = parts_sound_name_number(parts->sound_names[slot]);
}

void PE_SetSoundName(int parts_no, struct string *name, int state)
{
	if (parts_no <= 0)
		return;
	parts_set_sound_name(parts_get(parts_no), state, name ? name->text : NULL, name ? name->size : 0);
}

struct string *PE_GetSoundName(int parts_no, int state)
{
	if (parts_no <= 0)
		return string_ref(&EMPTY_STRING);
	struct parts *parts = parts_get(parts_no);
	if (state < 0 || state >= PARTS_NR_SOUND_NAMES || !parts->sound_names[state])
		return string_ref(&EMPTY_STRING);
	return string_dup(parts->sound_names[state]);
}

/* Parts_PlaySound (0x59b220): the component the get-or-create lookup gives
 * (0x53e510); 2 plays the cursor sound (0x564f20), 3 the click sound
 * (0x5b7fc0), any other State nothing. A disabled button plays neither, as
 * at its enter and its click (parts_cursor_sound; read from 0x5285b0, which
 * empties its inner parts' names, not followed for this call). */
void PE_PlaySound(int parts_no, int state)
{
	if (parts_no <= 0)
		return;
	struct parts *parts = parts_get(parts_no);
	int no = state == 2 ? parts_cursor_sound(parts) : state == 3 ? parts_click_sound(parts) : -1;
	if (no >= 0)
		audio_play_sound(no);
}

bool PE_SetPartsOnCursorSoundNumber(int parts_no, int sound_no)
{
	if (!asset_exists(ASSET_SOUND, sound_no)) {
		WARNING("Invalid sound number: %d", sound_no);
		return false;
	}

	struct parts *parts = parts_get(parts_no);
	parts->on_cursor_sound = sound_no;
	return true;
}

bool PE_SetPartsClickSoundNumber(int parts_no, int sound_no)
{
	if (!asset_exists(ASSET_SOUND, sound_no)) {
		WARNING("Invalid sound number: %d", sound_no);
		return false;
	}

	struct parts *parts = parts_get(parts_no);
	parts->on_click_sound = sound_no;
	return true;
}

bool PE_SetClickMissSoundNumber(possibly_unused int sound_no)
{
	UNIMPLEMENTED("(%d)", sound_no);
	return true;
}

void parts_reset_input(void)
{
	profile.parts.input->reset_input();
}

int parts_get_input_depth(void)
{
	return input_depth;
}

// A load resumes the script inside the scopes of the save (ain v7).
void parts_set_input_depth(int depth)
{
	input_depth = depth;
	parts_began_click = depth > 0;
}

void PE_BeginInput(void)
{
	profile.parts.input->begin_input();
}

void PE_EndInput(void)
{
	profile.parts.input->end_input();
}

void PE_SetDrag(int parts_no, bool enable)
{
	parts_get(parts_no)->draggable = !!enable;
}

int PE_GetClickPartsNumber(void)
{
	return clicked_parts;
}

/* v14 GetActiveParts: the parts under the cursor at the last left press
 * within the scope (see v14_update_input and pe_v14_message.c), 0 after a
 * press beside everything, after a reset of the input and when that parts
 * was released. That is the original's meaning too (there the press of any
 * button counts). The timing differs in three ways, which are this port's
 * choice and not taken from the original:
 *  - it is 0 outside a scope here, from the outermost EndInput on. In the
 *    original BeginInput and a nested EndInput walk the widgets to clear
 *    the focus (0x53e830 -> 0x53c750 -> 0x538060), while the outermost
 *    EndInput (0x58a750 at depth 1) runs 0x53e8b0 alone, which resets only
 *    the elements still in the manager's five slots (0x5461a0 -> 0x577af0,
 *    their +0x1b4 with them). The focus itself is kept here as there
 *    (v14_reset_input_session), so the next BeginInput sends it LostFocus.
 *  - it changes at once here. The original reports the scope object's
 *    +0x280 (0x53c6c0), which the parts update refreshes after its input
 *    update (0x53c240 at 0x53c654, after 0x547c90 at 0x53c64d), so a focus
 *    cleared by BeginInput or EndInput shows at the next parts update.
 *  - there is one focus here. The original keeps one value in every
 *    controller and reads the active one's (0x57bc90: +0x94 or, without
 *    one, the top of the stack, 0x53e640; 0x53c6c0 reads its +0x280, 0 when
 *    empty), which 0x53c240 refreshes in every controller's update, input
 *    flag or not, with the first parts of the controller's own tree whose
 *    widget has the focus (0x538180 from its root). Here the focus counts
 *    when it is the active controller's, and is 0 while another controller
 *    acts (e.g. under a confirmation box that is opening, before its
 *    BeginInput takes the focus); see v14_in_scope. */
int PE_GetActivePartsNumber(void)
{
	if (!parts_began_click)
		return 0;
	struct parts *focus = v14_active_parts ? parts_try_get(v14_active_parts) : NULL;
	if (focus && !v14_in_scope_of(focus, parts_multi_controller ? PE_get_active_controller() : 0))
		return 0;
	return v14_active_parts;
}

bool PE_IsCursorIn(int parts_no, int mouse_x, int mouse_y, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_try_get(parts_no);
	if (!parts)
		return false;

	Point mouse_pos = { mouse_x, mouse_y };
	return parts_hittest(parts, state, mouse_pos);
}

// The functions of each generation.
#include "classic/input.h"
#include "v14/input.h"
#include "v7/input.h"
