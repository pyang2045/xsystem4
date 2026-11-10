/* input.c: v14. Included at the end of input.c. See docs/game-profiles.md. */

bool v14_screen_to_box(struct parts *parts, struct parts_common *c, float sx, float sy,
		float *bx, float *by)
{
	mat4 m;
	parts_box_transform(parts, c, m);
	if (!parts_matrix_is_flat(m)) {
		// With depth (an X or Y rotation along the chain) the box reaches
		// the screen through the camera (parts_perspective): the plane of
		// the box maps to the screen by the homography H made of the
		// columns x, y and w of persp * m, and a screen point goes back by
		// its inverse.
		mat4 persp, f;
		parts_perspective(persp);
		glm_mat4_mul(persp, m, f);
		mat3 h = {
			{ f[0][0], f[0][1], f[0][3] },
			{ f[1][0], f[1][1], f[1][3] },
			{ f[3][0], f[3][1], f[3][3] },
		};
		if (!isfinite(glm_mat3_det(h)) || glm_mat3_det(h) == 0.0f)
			return false;
		mat3 inv;
		glm_mat3_inv(h, inv);
		vec3 q;
		glm_mat3_mulv(inv, (vec3){ sx, sy, 1.0f }, q);
		// q is (u, v, 1) / w: a point behind the eye (w <= 0) is none of
		// the box's.
		if (!isfinite(q[2]) || !(q[2] > 0.0f))
			return false;
		*bx = q[0] / q[2];
		*by = q[1] / q[2];
		return true;
	}
	// The 2D affine part: screen = (a c; b d) * box + (tx, ty).
	float a = m[0][0], b = m[0][1], cc = m[1][0], d = m[1][1];
	float det = a * d - b * cc;
	if (!isfinite(det) || det == 0.0f)
		return false;
	float x = sx - m[3][0];
	float y = sy - m[3][1];
	*bx = (d * x - cc * y) / det;
	*by = (a * y - b * x) / det;
	return true;
}

static bool parts_takes_input(struct parts *parts)
{
	return parts->global.show && parts->global.alpha != 0;
}

static bool v14_in_scope(const struct parts *parts)
{
	return v14_in_scope_of(parts, v14_scope);
}

/*
 * v14: the one parts under the cursor that takes the mouse at a press. The
 * original's input update (0x546890) finds it at a press only, with
 * 0x545e10 and the predicate 0x546e20, and keeps it both as the focus (on a
 * change the old one gets LostFocus and the new one Focus, the events
 * 0x80f5a4 and 0x80f580 of message types 20 and 19) and as the pressed
 * one: walking the list from its end (presumably front to back), the first
 * parts that is clickable (+0x1a4) or does not pass the cursor (+0x1a5,
 * オン指針透過 0), is shown and not fully transparent (+0x2f0 and +0x2ec,
 * presumably) and whose area contains the point (vtable +0x4c). A parts
 * that passes the cursor and is not clickable is skipped, so the pactex
 * decorations marked オン指針透過 1 (the title characters, the worker
 * card's texts and numbers, the portraits on the choice cards) block
 * neither the hover nor the click of the button behind them. The hover is
 * the cursor walk's (v14_cursor_walk), which enters this parts and every
 * parts in front of it. (0x546e20 also
 * takes an element whose +0x1dc is not negative, presumably one in the
 * middle of a swipe; swipes are not implemented.) The area is always the
 * default state's: at a press the parts has already switched to its down
 * state, and a ＣＧ判定部件 detector (FooterButton) defines its area only in
 * the normal state (on-cursor and down are CG parts without a CG). Only the
 * parts of the update's controller are candidates (v14_in_scope).
 */
static struct parts *v14_input_target(Point pos)
{
	struct parts *parts;
	PARTS_LIST_FOREACH_REVERSE(parts) {
		if (!v14_in_scope(parts))
			continue;
		// A scroll bar stands for its knob and its track, the original's
		// inner parts, which the bar's own flags do not decide alone: the
		// track can always be clicked (scrollbar.c).
		if (parts_scrollbar_has_look(parts)) {
			if (parts_takes_input(parts)
					&& parts_scrollbar_hittest(parts, pos) != PARTS_SCROLLBAR_MISS)
				return parts;
			continue;
		}
		if (!parts->clickable && parts->pass_cursor)
			continue;
		if (!parts_takes_input(parts))
			continue;
		if (parts_hittest(parts, PARTS_STATE_DEFAULT, pos))
			return parts;
	}
	return NULL;
}

static void v14_send(struct parts *parts, int type, int nr_vars, const int *vars)
{
	parts_enqueue_message_vars(type, parts->no, parts->delegate_index, parts->unique_id,
			nr_vars, vars);
}

// Trace (XSYS4_STAGE2_TRACE): what the walk entered and left.
static void v14_trace_hover(const char *what, struct parts *parts, Point pos)
{
	static unsigned traced;
	if (input_stage2_trace() && traced < 5000) {
		traced++;
		NOTICE("S2 %s ms=%u pos=%d,%d parts=%d", what, (uint32_t)SDL_GetTicks(), pos.x, pos.y, parts->no);
	}
}

static void v14_hover_changed(struct parts *parts)
{
	struct parts *link = parts->linked_from >= 0 ? parts_try_get(parts->linked_from) : NULL;
	if (link)
		parts_dirty(link);
}

/* 0x577b90: the walk entered the element, or it stays entered. */
static void v14_enter(struct parts *parts, Point pos, bool cur_clicking)
{
	int vars[3] = { pos.x, pos.y, 0 };
	if (!parts->is_hovered) {
		v14_send(parts, PARTS_V14_MSG_MOUSE_ENTER, 2, vars);
		// 0x564f20, the cursor sound (Parts_PlaySound(n, 2)), at every
		// new enter (0x577c17), whether or not a button is down: the
		// sound of the name in slot 2 (parts_set_sound_name). Nothing is
		// played without a number.
		if (parts_cursor_sound(parts) >= 0)
			audio_play_sound(parts_cursor_sound(parts));
		parts->hover_time = 0;
		v14_trace_hover("enter", parts, pos);
	}
	// +4: the left button went down on it, and it was clickable then
	// (0x578580); click_down_parts is that parts while the button is held.
	bool down = cur_clicking && click_down_parts == parts->no && parts->clickable;
	parts_set_state(parts, down ? PARTS_STATE_CLICKED : PARTS_STATE_HOVERED);
	vars[2] = parts->hover_time;
	v14_send(parts, PARTS_V14_MSG_ON_CURSOR, 3, vars);
	if (pos.x != parts->hover_pos.x || pos.y != parts->hover_pos.y)
		v14_send(parts, PARTS_V14_MSG_MOUSE_MOVE, 2, vars);
	parts->hover_pos = pos;
	if (!parts->is_hovered) {
		parts->is_hovered = true;
		v14_hover_changed(parts);
	}
}

/* 0x577cf0: the element is not entered (or no longer). */
static void v14_leave(struct parts *parts, Point pos)
{
	if (parts->is_hovered) {
		int vars[2] = { pos.x, pos.y };
		v14_send(parts, PARTS_V14_MSG_MOUSE_LEAVE, 2, vars);
		v14_trace_hover("leave", parts, pos);
		parts->is_hovered = false;
		v14_hover_changed(parts);
	}
	parts_set_state(parts, PARTS_STATE_DEFAULT);
}

/*
 * v14: the cursor walk at the end of 0x546890 (0x5471d0, called at 0x546dc7
 * with the enter functor 0x80f7a0 -> 0x548cf0 -> 0x577b90 and the leave
 * functor 0x80f6c8 -> 0x548bc0 -> 0x577cf0). It goes through the list from
 * its end, front to back: an element is entered when nothing before it
 * stopped the walk, its parts is shown (+0x2f0) and not fully transparent
 * (the alpha +0x2ec as a float above 0.0, [0x813018]) and its area contains
 * the point (vtable +0x4c, the default state's); after an entered element
 * that does not pass the cursor (+0x1a5 0) the walk stops. Every element
 * not entered is left: those behind the one that stopped the walk and those
 * before it the cursor is not on. Unlike the input target
 * (v14_input_target) the walk does not ask whether a parts is clickable: a
 * clickable parts that passes the cursor is entered and the walk goes on
 * behind it. So the input target is always entered (every parts in front
 * of it passes the cursor), and the decorations marked オン指針透過 1 in
 * front of a button are entered with it.
 *
 * Entering (0x577b90), every update while the element is entered:
 *  - the first time (+0x1ac 0): MouseEnter (x, y), the cursor sound
 *    (0x564f20) and the time since entered set to 0 (+0x1b0);
 *  - the down state when the left button went down on it (+4, set by
 *    0x578580 for a clickable entered element at the press; 3), the
 *    on-cursor state otherwise (2; vtable +0x34);
 *  - OnCursor (x, y, the time since entered);
 *  - MouseMove (x, y) when the cursor is not where the element last saw it
 *    (+0x1b8/+0x1bc), which is then the cursor's position; so also when it
 *    is entered again elsewhere.
 * Leaving (0x577cf0), every update while the element is not entered: a
 * MouseLeave (x, y) when it was entered, and the default state (1). The
 * original skips both while the element is a drop target (+0x1e3); drag and
 * drop is not sent in v14 here.
 *
 * The messages go to the element's own emitter (+0x1e4), so they carry the
 * parts' number, delegate index and event ID. The coordinates go through
 * 0x696e00, the window's scale, which mouse_get_pos has already applied.
 *
 * A scroll bar stands for its knob and its track (the original's inner
 * elements), as in v14_input_target: entered when one of them is under the
 * cursor, and it stops the walk.
 *
 * The list is the update's controller's (v14_in_scope): the parts of the
 * other controllers are neither entered nor left, so they keep their
 * entered flag, their state and their time.
 */
static void v14_cursor_walk(Point pos, bool cur_clicking)
{
	bool stopped = false;
	struct parts *parts;
	PARTS_LIST_FOREACH_REVERSE(parts) {
		if (!v14_in_scope(parts))
			continue;
		bool hit = false, stops = false;
		if (!stopped && parts_takes_input(parts)) {
			if (parts_scrollbar_has_look(parts)) {
				hit = parts_scrollbar_hittest(parts, pos) != PARTS_SCROLLBAR_MISS;
				stops = true;
			} else {
				hit = parts_hittest(parts, PARTS_STATE_DEFAULT, pos);
				stops = !parts->pass_cursor;
			}
		}
		if (hit) {
			v14_enter(parts, pos, cur_clicking);
			stopped = stops;
		} else {
			v14_leave(parts, pos);
		}
	}
}

/* 0x546890 at a press: the pressed parts and the focus, and the focus'
 * messages when it changes (0x546a4e..0x546aa2). The old focus gets its
 * LostFocus here also when it is another controller's; the original looks
 * the old focus up in the list (0x545c70) and leaves one outside it to the
 * next 0x53c750 (see v14_in_scope: one focus is the port's strategy). */
static void v14_press(Point cur_pos, struct parts *target)
{
	int no = target ? target->no : 0;
	if (no != v14_active_parts) {
		struct parts *old = v14_active_parts ? parts_try_get(v14_active_parts) : NULL;
		if (old)
			v14_send(old, PARTS_V14_MSG_LOST_FOCUS, 0, NULL);
		if (target)
			v14_send(target, PARTS_V14_MSG_FOCUS, 0, NULL);
	}
	v14_press_parts = no;
	v14_active_parts = no;
	// The click and a drag start on the target when it takes the click
	// (it is clickable or blocks the cursor; a scroll bar may do neither).
	if (target && (target->clickable || !target->pass_cursor)) {
		click_down_parts = target->no;
		drag_parts = target;
		drag_initial_pos = target->local.pos;
		drag_start_cursor = cur_pos;
	}
	if (dev_getenv("XSYS4_STAGE2_TRACE")) {
		unsigned candidates = 0;
		struct parts *parts;
		NOTICE("S2 parts press ms=%u key=%d pos=%d,%d target=%d", (uint32_t)SDL_GetTicks(),
				VK_LBUTTON, cur_pos.x, cur_pos.y, v14_press_parts);
		PARTS_LIST_FOREACH_REVERSE(parts) {
			if (!parts->clickable || !parts->global.show || !parts->global.alpha || candidates++ >= 32)
				continue;
			Rectangle box = parts->states[PARTS_STATE_DEFAULT].common.hitbox;
			NOTICE("S2 candidate no=%d state=%d pass=%d pixel=%d hit=%d default_hit=%d box=%d,%d,%d,%d global=%d,%d scale=%.3f,%.3f parent=%d",
				parts->no, parts->state, parts->pass_cursor, parts->pixel_hittest,
				parts_hittest(parts, parts->state, cur_pos), parts_hittest(parts, PARTS_STATE_DEFAULT, cur_pos),
				box.x, box.y, box.w, box.h, parts->global.pos.x, parts->global.pos.y,
				parts->global.scale.x, parts->global.scale.y, parts->parent ? parts->parent->no : 0);
		}
	}
}

/* 0x578580 -> 0x5788d0 at a release: the pressed parts' own click, when it
 * is entered and the position is given (0x578944, 0x578955). */
static void v14_release(int pressed_no, Point cur_pos, bool have_pos)
{
	int vars[3] = { cur_pos.x, cur_pos.y, VK_LBUTTON };
	struct parts *target = pressed_no ? parts_try_get(pressed_no) : NULL;
	bool on_target = target && target->is_hovered && target->clickable && have_pos;
	if (dev_getenv("XSYS4_STAGE2_TRACE"))
		NOTICE("S2 click ms=%u key=%d pos=%d,%d target=%d%s%s", (uint32_t)SDL_GetTicks(), VK_LBUTTON,
				cur_pos.x, cur_pos.y, target ? target->no : 0,
				target && !on_target ? " (left)" : "",
				on_target && target->button_disabled ? " (disabled)" : "");
	if (!on_target || target->button_disabled)
		return;
	if (parts_click_sound(target) >= 0)
		audio_play_sound(parts_click_sound(target));
	clicked_parts = target->no;
	// type 4 = MouseClick (CallEvent3: x,y,keyCode) in
	// CPartsMessageManager's SWITCH (bytecode-verified; type 5
	// is DoubleClick/CallEvent0 and dispatches an empty slot)
	v14_send(target, PARTS_V14_MSG_MOUSE_CLICK, 3, vars);
	// Only a parts that was made a check box has the values: the
	// pactex loader's legacy guess also gives a parts type 1.
	if (target->component_type == PARTS_COMPONENT_CHECKBOX && target->checkbox)
		parts_checkbox_set_checked(target, !target->checkbox->checked);
}

/*
 * Native 0x5350c0: types 5/7/15 accept wheel input; 2/3/6/11/12/13 do
 * when focused. The next source is an internal component owner (+0x7c),
 * then the parts' own flag. That owner is not the visual parent set at
 * 0x53b098 (+0x13c): ordinary named parts are constructed with a null owner
 * (0x578cb0 -> 0x4f3c20 -> 0x533da0). Do not walk parts->parent here.
 * The port has one input element per parts, so its active ID is the focus
 * here; native composite widgets' internal owner and focus are not rebuilt.
 */
static bool v14_wheelable(struct parts *parts)
{
	switch (parts->component_type) {
	case 5: case 7: case 15:
		return true;
	case 2: case 3: case 6: case 11: case 12: case 13:
		if (parts->no == v14_active_parts)
			return true;
		break;
	}
	return parts->wheelable;
}

/*
 * Native 0x547000: the first wheelable, shown, nontransparent element hit
 * in reverse order receives one event. Cursor blocking, click permission
 * and disabled-button state do not take part in this walk. A scrollbar
 * stands for its inner track and knob, as in the existing input walk.
 * Without a hit, the updated focus receives it, otherwise the manager's
 * root. Here the active controller's root represents that fallback; the
 * system overlay has no root and cannot supply such an element. The
 * candidates and the focus are the update's controller's (0x545c70 finds
 * the focus in the list, else the root): a focus held in another
 * controller does not take it.
 */
static void v14_dispatch_wheel(Point pos, const int *counts)
{
	struct parts *target = NULL, *parts;
	PARTS_LIST_FOREACH_REVERSE(parts) {
		if (!v14_in_scope(parts) || !v14_wheelable(parts) || !parts_takes_input(parts))
			continue;
		bool hit = parts_scrollbar_has_look(parts)
			? parts_scrollbar_hittest(parts, pos) != PARTS_SCROLLBAR_MISS
			: parts_hittest(parts, PARTS_STATE_DEFAULT, pos);
		if (hit) {
			target = parts;
			break;
		}
	}
	bool fallback = !target;
	if (!target && v14_active_parts) {
		struct parts *focus = parts_try_get(v14_active_parts);
		if (focus && v14_in_scope(focus))
			target = focus;
	}
	if (!target)
		target = parts_controller_root(PE_get_active_controller());
	if (target)
		v14_send(target, PARTS_V14_MSG_MOUSE_WHEEL, 2, counts);
	static unsigned traced;
	if (input_stage2_trace() && traced++ < 2000)
		NOTICE("S2 wheel-dispatch ms=%u pos=%d,%d forward=%d back=%d target=%d fallback=%d",
				(uint32_t)SDL_GetTicks(), pos.x, pos.y, counts[0], counts[1],
				target ? target->no : 0, fallback);
}

/* 0x547c90 -> 0x547c00 -> 0x548530 for each of the five slots: a slot whose
 * element is not in this update's list is emptied, with no message and no
 * change to the element (0x577af0 is not called). Of the slots the left one
 * is followed here, with the drag that starts on it: a parts held down in
 * another controller is no longer the pressed one, so the release clicks
 * nothing and only the whole-screen click goes out. (In the original a
 * controller begun inside the scope has a manager of its own that never had
 * the press; the old one keeps its slot.) */
static void v14_drop_slots_out_of_scope(void)
{
	struct parts *held = v14_press_parts ? parts_try_get(v14_press_parts) : NULL;
	if (held && !v14_in_scope(held))
		v14_press_parts = 0;
	struct parts *down = click_down_parts ? parts_try_get(click_down_parts) : NULL;
	if (down && !v14_in_scope(down))
		click_down_parts = 0;
	if (drag_parts && !v14_in_scope(drag_parts))
		drag_state_reset();
}

static void v14_update_input(int passed_time)
{
	bool cur_clicking = key_is_down(VK_LBUTTON);
	Point cur_pos;
	mouse_get_pos(&cur_pos.x, &cur_pos.y);
	bool have_pos = mouse_in_window();
	// The controller whose parts this update reads (0x53e690); in the
	// single-controller mode every parts is read.
	v14_scope = parts_multi_controller ? PE_get_active_controller() : 0;
	// Trace (XSYS4_STAGE2_TRACE): the controller whenever it changes.
	static int traced_scope = -1;
	static unsigned traced_scopes;
	if (parts_multi_controller && v14_scope != traced_scope && input_stage2_trace()
			&& traced_scopes < 2000) {
		traced_scopes++;
		traced_scope = v14_scope;
		NOTICE("S2 input-scope ms=%u controller=%d began=%d", (uint32_t)SDL_GetTicks(), v14_scope,
				parts_began_click);
	}
	struct parts *target = have_pos ? v14_input_target(cur_pos) : NULL;
	// Trace (XSYS4_STAGE2_TRACE): the input target whenever it changes,
	// so that a run's log shows what the cursor is on also where nothing
	// is clicked (a scheduled move, see src/input.c); also outside an input
	// scope, where nothing else is done.
	static int traced_no = -1;
	static unsigned traced;
	int traced_now = target ? target->no : 0;
	if (have_pos && input_stage2_trace() && traced_now != traced_no && traced < 5000) {
		traced++;
		traced_no = traced_now;
		NOTICE("S2 hover ms=%u pos=%d,%d target=%d", (uint32_t)SDL_GetTicks(), cur_pos.x, cur_pos.y, traced_now);
	}
	if (!parts_began_click)
		return;
	v14_drop_slots_out_of_scope();
	struct parts *parts;
	PARTS_LIST_FOREACH(parts) {
		if (!v14_in_scope(parts))
			continue;
		long long t = (long long)parts->hover_time + passed_time;
		parts->hover_time = t > INT_MAX ? INT_MAX : t < INT_MIN ? INT_MIN : (int)t;
	}
	clicked_parts = 0;
	if (!have_pos)
		cur_pos = parts_prev_pos;
	bool moved = cur_pos.x != parts_prev_pos.x || cur_pos.y != parts_prev_pos.y;
	bool press = cur_clicking && !prev_clicking;
	bool release = !cur_clicking && prev_clicking;
	// The parts whose own click this release sends (the focus' key table,
	// 0x578580): the left slot's, taken before the slot is emptied.
	int released_no = release ? v14_press_parts : 0;

	// Native 0x547fd1..0x54800f: Whole wheel precedes Whole move/click
	// and does not require a current position. Keep both counts: equal
	// nonzero counts still produce an event, consumed differently by scripts.
	int wheel[2];
	mouse_get_wheel(&wheel[0], &wheel[1]);
	bool have_wheel = wheel[0] || wheel[1];
	if (have_wheel)
		parts_enqueue_message_vars(PARTS_V14_MSG_MOUSE_WHEEL, 0, -1, -1, 2, wheel);

	if (have_pos) {
		int vars[3] = { cur_pos.x, cur_pos.y, VK_LBUTTON };
		if (moved)
			parts_enqueue_message_vars(PARTS_V14_MSG_MOUSE_MOVE, 0, -1, -1, 2, vars);
		if (release)
			parts_enqueue_message_vars(PARTS_V14_MSG_MOUSE_CLICK, 0, -1, -1, 3, vars);
		if (press) {
			v14_press(cur_pos, target);
		} else if (!cur_clicking) {
			// 0x5468d6..0x5468fb -> 0x547460: no press and the left
			// button up (the release included): the left slot is
			// emptied; the focus stays.
			v14_press_parts = 0;
			click_down_parts = 0;
		}
		v14_cursor_walk(cur_pos, cur_clicking);
	} else if (!release && v14_press_parts && v14_press_parts == v14_active_parts) {
		// 0x5476b0: the left slot's parts, when it is the focus, that is
		// while the left button is held on the parts it went down on (at
		// a release here 0x5462b0 has emptied the slot first, 0x5481cb).
		// 0x577cf0 reads the position again and, without one, has (0, 0)
		// (its locals are cleared first, 0x577d1d; what the call writes
		// when it fails was not followed).
		struct parts *focus = parts_try_get(v14_active_parts);
		if (focus)
			v14_leave(focus, (Point){ 0, 0 });
	}

	// Drag movement: a draggable pressed parts follows the cursor (the
	// drag messages are not sent in v14).
	if (drag_parts && cur_clicking && moved && drag_parts->draggable) {
		parts_set_pos(drag_parts, (Point){
			drag_initial_pos.x + (cur_pos.x - drag_start_cursor.x),
			drag_initial_pos.y + (cur_pos.y - drag_start_cursor.y) });
		is_dragging = true;
	}

	// The knob of a scroll bar moves in the input update, before the
	// widgets' update that reports it (parts_scrollbar_update). Without a
	// position the bars see the target of the last update.
	static int last_target_no;
	if (have_pos)
		last_target_no = target ? target->no : 0;
	parts_scrollbar_input(cur_pos, cur_clicking, prev_clicking,
			last_target_no ? parts_try_get(last_target_no) : NULL);
	// 0x548362: after hover/focus processing, before the focus' release.
	// A failed position query still uses the manager's previous position.
	if (have_wheel)
		v14_dispatch_wheel(cur_pos, wheel);
	if (release) {
		v14_release(released_no, cur_pos, have_pos);
		drag_state_reset();
		// Without a position the release empties the slot here
		// (0x5462b0).
		v14_press_parts = 0;
		click_down_parts = 0;
	}

	prev_clicking = cur_clicking;
	if (have_pos && v14_prev_have_pos)
		parts_prev_pos = cur_pos;
	v14_prev_have_pos = have_pos;
}

static void v14_update_input_state(int passed_time)
{
	v14_update_input(passed_time);
	// The scheduled input (src/input.c) learns that the input was read.
	input_auto_read();
}

/* Shared with the engines before v14 (GUIEngine, GoatGUIEngine, the older
 * PartsEngine), which keep their behaviour: both parts are got or made, and
 * the number is kept as it is. */
static void v14_set_show_link(int parts_no, int link_parts_no)
{
	if (parts_no > 0)
		parts_set_show_link(parts_get(parts_no), link_parts_no);
	return;
}

/* 0x577af0 on an element in one of the manager's slots: not entered, no
 * time since entered, no last position, no button down on it and not the
 * focus (+0x1b4). Nothing is sent, and the state stays as it is until the
 * next cursor walk. */
static void v14_reset_slot(int parts_no)
{
	struct parts *parts = parts_no ? parts_try_get(parts_no) : NULL;
	if (!parts)
		return;
	if (parts->is_hovered) {
		parts->is_hovered = false;
		v14_hover_changed(parts);
	}
	parts->hover_time = 0;
	parts->hover_pos = (Point){0};
	if (parts_no == v14_active_parts)
		v14_active_parts = 0;
}

static void v14_reset_input_session(bool begin)
{
	// Native 0x53e830/0x53e8b0 reset the input targets, not the message
	// queue. The end of a scope (0x53e8b0 -> 0x5461a0: EndInput, and
	// BeginInput within a scope) resets the elements in the manager's five
	// slots (0x577af0 on +0x1ec, +0x200, +0x208, +0x210, +0x218) and empties
	// the slots and the stored position (+0x18/+0x1c). Of the slots only the
	// left one is followed here, and it holds a parts only while the left
	// button is held on it: the update of the release has emptied it
	// (0x547460, v14_update_input). The focus is not a slot (it is the
	// element's +0x1b4, found anew by 0x545c70 every update): a parts just
	// clicked keeps its entered flag, its time and its state like every
	// other parts, so a parts the cursor stays on is not entered again in
	// the next scope, and one the cursor is no longer on is left by the
	// next walk. When the held parts is the focus, 0x577af0 clears its
	// +0x1b4 too, silently.
	v14_reset_slot(click_down_parts);
	v14_reset_slot(v14_press_parts);
	if (begin) {
		// The beginning (0x53e830, BeginInput and an EndInput that
		// resumes an outer scope: 0x53c750 -> 0x538060) takes the focus
		// from every widget (vtable +0x1c with false, SpriteParts
		// 0x564cc0, a button's inner parts through 0x4db7e0 -> 0x4f3110):
		// LostFocus to an element whose +0x1b4 is still set, which is
		// cleared; its entered flag (+0x1ac) and state are not touched.
		// The outermost EndInput (0x53e8b0 alone) leaves the focus.
		struct parts *focus = v14_active_parts ? parts_try_get(v14_active_parts) : NULL;
		if (focus)
			v14_send(focus, PARTS_V14_MSG_LOST_FOCUS, 0, NULL);
		v14_active_parts = 0;
		// 0x546030 clears +0x40..+0x43 with one dword (0x5460c9), the
		// last update's "had a position" (+0x43) with the sampled buttons.
		v14_prev_have_pos = false;
	}
	clicked_parts = 0;
	click_down_parts = 0;
	v14_press_parts = 0;
	drag_state_reset();
	parts_scrollbar_reset_input();
	parts_prev_pos = (Point){0};
	// The beginning (0x546030) samples held buttons so returning from a
	// child scene cannot turn its closing press into another DOWN. The left
	// button held here is up at a later update: that release sends the
	// whole-screen click, as the original's sampled buttons make it
	// (0x546030 samples all three into +0x40..+0x42; only the left one is
	// followed here, see v14_update_input).
	prev_clicking = begin && key_is_down(VK_LBUTTON);
	parts_began_click = begin;
}

static void v14_reset_input(void)
{
	input_depth = 0;
	v14_reset_input_session(false);
	v14_active_parts = 0;
	v14_prev_have_pos = false;
}

// Native 0x58a720: reset/restart the session, then increment depth.
static void v14_begin_input(void)
{
	v14_reset_input_session(true);
	input_depth++;
}

// Native 0x58a750: end the child, resume a remaining outer scope, and clamp
// an unmatched EndInput at zero.
static void v14_end_input(void)
{
	if (input_depth)
		input_depth--;
	v14_reset_input_session(input_depth > 0);
}

const struct parts_input_ops parts_input_v14 = {
	.begin_input = v14_begin_input,
	.end_input = v14_end_input,
	.reset_input = v14_reset_input,
	.update_input_state = v14_update_input_state,
	.set_show_link = v14_set_show_link,
};
