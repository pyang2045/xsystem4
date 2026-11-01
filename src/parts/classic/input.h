/* input.c: engines before v14 (and what ain v7 shares). Included at the end of input.c. See docs/game-profiles.md. */

/*
 * Invert parts_box_transform (the parts' accumulated matrix, which now
 * carries the ancestors' rotation and scale as well, and the origin offset).
 * Returns the point in the parts' box (pixels from its top-left corner), or
 * false when the transform is singular (a scale of 0).
 *
 * Before v14 it is taken apart step by step as it was: translation, the
 * ancestors' reverse flags, rotation, scale, the parts' own reverse flags
 * and origin (parts_anchor_transform's earlier form).
 */
bool classic_screen_to_box(struct parts *parts, struct parts_common *c, float sx, float sy,
		float *bx, float *by)
{
	float x = sx - parts->global.pos.x;
	float y = sy - parts->global.pos.y;
	if (parts->global.reverse_lr != parts->local.reverse_lr)
		x = -x;
	if (parts->global.reverse_tb != parts->local.reverse_tb)
		y = -y;
	if (parts->local.rotation.z != 0.0f) {
		float angle = parts->local.rotation.z * (3.14159265358979323846f / 180.0f);
		float cs = cosf(angle), sn = sinf(angle);
		float rx = cs * x + sn * y;
		y = -sn * x + cs * y;
		x = rx;
	}
	if (!parts->global.scale.x || !parts->global.scale.y)
		return false;
	x = x / parts->global.scale.x;
	y = y / parts->global.scale.y;
	if (parts->local.reverse_lr)
		x = -x;
	if (parts->local.reverse_tb)
		y = -y;
	*bx = x - c->origin_offset.x;
	*by = y - c->origin_offset.y;
	return true;
}

/* Programs before v14: the hover and the click of one parts (v14 has
 * v14_update_input). */
static void parts_update_mouse(struct parts *parts, Point cur_pos, bool cur_clicking,
		int passed_time, bool *hover_consumed, bool *click_consumed)
{
	// Always use DEFAULT state hitbox regardless of the current display state.
	// ain v7 with activities: a hidden parts is not under the cursor
	// (C_SAVE's クリック抑制フィルタ, full-screen, hides itself to let the
	// list be clicked); other games keep the old test.
	bool is_hovered = parts_hittest(parts, PARTS_STATE_DEFAULT, cur_pos) && !*hover_consumed
		&& (!profile.parts.activity_pointer || parts->global.show);

	bool was_hovered = parts->is_hovered;
	parts->is_hovered = is_hovered;

	// !pass_cursor parts consume the cursor for parts behind them
	if (is_hovered && !parts->pass_cursor)
		*hover_consumed = true;

	if (parts->linked_from >= 0 && is_hovered != was_hovered) {
		parts_dirty(parts_get(parts->linked_from));
	}

	if (!parts_began_click)
		return;

	if (is_hovered && !was_hovered) {
		parts_msg_push(parts, PARTS_MSG_MOUSE_ENTER, "ii", cur_pos.x, cur_pos.y);
		parts->hover_time = 0;
	}
	if (!is_hovered && was_hovered) {
		parts_msg_push(parts, PARTS_MSG_MOUSE_LEAVE, "ii", cur_pos.x, cur_pos.y);
	}

	if (is_hovered) {
		// MOUSE_ON message fires every frame while hovering
		parts_msg_push(parts, PARTS_MSG_MOUSE_ON,
				"iii", cur_pos.x, cur_pos.y, parts->hover_time);
		parts->hover_time += passed_time;

		// MOUSE_MOVE message fires when cursor moves while hovering
		if (cur_pos.x != parts_prev_pos.x || cur_pos.y != parts_prev_pos.y) {
			parts_msg_push(parts, PARTS_MSG_MOUSE_MOVE, "ii", cur_pos.x, cur_pos.y);
		}
	}

	if (!is_hovered) {
		parts_set_state(parts, PARTS_STATE_DEFAULT);
		return;
	}

	// ain v7 with activities: a parts that takes no click keeps its normal
	// state under the cursor. 背景 of C_TITLE is a flat with an empty ＣＧ名
	// as its オンカーソル状態: in that state it drew nothing and did not play.
	bool keep_state = profile.parts.activity_pointer && !parts->clickable;
	enum parts_state_type hovered = keep_state ? PARTS_STATE_DEFAULT : PARTS_STATE_HOVERED;

	bool click_eligible = parts->clickable || !parts->pass_cursor;
	if (!click_eligible || *click_consumed) {
		if (!was_hovered)
			audio_play_sound(parts_cursor_sound(parts));
		parts_set_state(parts, hovered);
		return;
	}

	// click down: first eligible part captures the click
	if (cur_clicking && !prev_clicking) {
		click_down_parts = parts->no;
		*click_consumed = true;

		drag_parts = parts;
		drag_initial_pos = parts->local.pos;
		drag_start_cursor = cur_pos;

		// KEY_TRIGGER message fires on press transition
		parts_msg_push(parts, PARTS_MSG_KEY_TRIGGER, "i", VK_LBUTTON);
		if (profile.parts.activity_pointer) {
			parts_msg_push(parts, PARTS_MSG_KEY_PRESS, "i", VK_LBUTTON);
			click_repeat_ms = KEY_REPEAT_DELAY_MS;
		}
	}

	// KEY_DOWN message fires every frame while held (not first frame)
	if (prev_clicking && cur_clicking && click_down_parts == parts->no) {
		parts_msg_push(parts, PARTS_MSG_KEY_DOWN, "i", VK_LBUTTON);
		if (profile.parts.activity_pointer) {
			click_repeat_ms -= passed_time;
			if (click_repeat_ms <= 0) {
				parts_msg_push(parts, PARTS_MSG_KEY_PRESS, "i", VK_LBUTTON);
				click_repeat_ms = KEY_REPEAT_INTERVAL_MS;
			}
		}
	}

	if (cur_clicking && click_down_parts == parts->no) {
		parts_set_state(parts, keep_state ? PARTS_STATE_DEFAULT : PARTS_STATE_CLICKED);
	} else {
		if (!was_hovered) {
			audio_play_sound(parts_cursor_sound(parts));
		}
		parts_set_state(parts, hovered);
	}

	// click event: only if the click down event had same parts number.
	if (parts->clickable && prev_clicking && !cur_clicking
			&& click_down_parts == parts->no) {
		audio_play_sound(parts_click_sound(parts));
		clicked_parts = parts->no;

		parts_msg_push(parts, PARTS_MSG_MOUSE_CLICK,
				"iii", cur_pos.x, cur_pos.y, VK_LBUTTON);
		parts_msg_push(parts, PARTS_MSG_KEY_UP, "i", VK_LBUTTON);
		if (profile.parts.widget_parts_messages && parts->component_type == PARTS_COMPONENT_CHECKBOX
				&& parts->checkbox)
			parts_checkbox_set_checked(parts, !parts->checkbox->checked);
	}
}

/*
 * ain v7: the Whole key events (the mouse buttons go to the parts under the
 * cursor, and the left one to the Whole click). KeyPress repeats while the key is held, at the usual rates: the
 * original's are not known.
 */
static void parts_whole_keys_update(int passed_time)
{
	static bool down[256];
	static int repeat_ms[256];
	for (int vk = 0; vk < 256; vk++) {
		if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON)
			continue;
		bool is_down = key_is_down(vk);
		if (is_down && !down[vk]) {
			parts_msg_push(NULL, PARTS_MSG_KEY_TRIGGER, "i", vk);
			parts_msg_push(NULL, PARTS_MSG_KEY_PRESS, "i", vk);
			repeat_ms[vk] = KEY_REPEAT_DELAY_MS;
		} else if (is_down) {
			parts_msg_push(NULL, PARTS_MSG_KEY_DOWN, "i", vk);
			repeat_ms[vk] -= passed_time;
			if (repeat_ms[vk] <= 0) {
				parts_msg_push(NULL, PARTS_MSG_KEY_PRESS, "i", vk);
				repeat_ms[vk] = KEY_REPEAT_INTERVAL_MS;
			}
		} else if (down[vk]) {
			parts_msg_push(NULL, PARTS_MSG_KEY_UP, "i", vk);
		}
		down[vk] = is_down;
	}
}

/*
 * Oyako Rankan's GUI controllers, the stack at +0xac of its PartsEngine:
 * PushGUIController (0x546d80 -> 0x5346f0) adds one, a new parts goes to the
 * top one, and the input goes to the top one alone (UpdateGUIController,
 * 0x543be0 -> 0x51cc30), so an open combo box or dialog is modal.
 * PopGUIController (0x51eb70) drops the top one and makes a new one when none
 * is left. Each has its focus, a parts number (+0x38; 0x546d40, 0x546d60).
 * Other engines stay on controller 0.
 *
 * Here a parts just keeps the number of its level, and the levels below the
 * top take no input; a parts left on a dropped level goes on with the level
 * under it (the scripts delete theirs). Not done: the focus
 * events of 0x51d370 (nothing in the script listens), and setting the focus
 * at a press. The stack is not saved: nothing saves with a combo box open.
 */
#define MAX_GUI_CONTROLLERS 16
static int gui_controller_top;
static int gui_controller_focus[MAX_GUI_CONTROLLERS];

int parts_gui_controller(void)
{
	return gui_controller_top;
}

void parts_gui_controllers_reset(void)
{
	gui_controller_top = 0;
	gui_controller_focus[0] = 0;
}

void PE_PushGUIController(void)
{
	if (gui_controller_top + 1 >= MAX_GUI_CONTROLLERS) {
		WARNING("Too many GUI controllers");
		return;
	}
	gui_controller_focus[++gui_controller_top] = 0;
}

void PE_PopGUIController(void)
{
	if (gui_controller_top > 0)
		gui_controller_top--;
	else
		gui_controller_focus[0] = 0;
}

int PE_GetFocusPartsNumber(void)
{
	return gui_controller_focus[gui_controller_top];
}

void PE_SetFocusPartsNumber(int parts_no)
{
	gui_controller_focus[gui_controller_top] = parts_no;
}

void PE_UpdateGUIController(int passed_time)
{
	PE_UpdateInputState(passed_time);
}

static void classic_update_input_state(int passed_time)
{
	Point cur_pos;
	bool cur_clicking = key_is_down(VK_LBUTTON);
	mouse_get_pos(&cur_pos.x, &cur_pos.y);

	bool hover_consumed = false;
	bool click_consumed = false;
	struct parts *parts;
	// The Whole click, whatever is under the cursor (C_CG_MODE_BROWSE goes to
	// the next CG on it). Before the parts' messages: the thumbnail's click
	// opens the viewer, whose handler would otherwise get this same release.
	if (profile.parts.whole_key_events && prev_clicking && !cur_clicking)
		parts_msg_push(NULL, PARTS_MSG_MOUSE_CLICK, "iii", cur_pos.x, cur_pos.y, VK_LBUTTON);
	// Iterate front-to-back (highest z first) for proper cursor consumption
	PARTS_LIST_FOREACH_REVERSE(parts) {
		if (parts->gui_controller < gui_controller_top)
			continue;
		parts_update_mouse(parts, cur_pos, cur_clicking, passed_time,
				&hover_consumed, &click_consumed);
	}

	if (profile.parts.whole_key_events) {
		parts_whole_keys_update(passed_time);
		// The Whole wheel event, once a notch: the wheel stays set until a
		// script clears it, as the message wait polls it too.
		static bool wheel_sent;
		int forward, back;
		mouse_get_wheel(&forward, &back);
		if (!forward && !back) {
			wheel_sent = false;
		} else if (!wheel_sent) {
			parts_msg_push(NULL, PARTS_MSG_MOUSE_WHEEL, "ii", forward, back);
			wheel_sent = true;
		}
		// The bars take their own clicks and drags (scrollbar.c).
		struct parts *bar = NULL;
		PARTS_LIST_FOREACH_REVERSE(parts) {
			if (parts->gui_controller >= gui_controller_top
					&& parts->global.show && parts_scrollbar_has_look(parts)
					&& parts_scrollbar_hittest(parts, cur_pos) != PARTS_SCROLLBAR_MISS) {
				bar = parts;
				break;
			}
		}
		parts_scrollbar_input(cur_pos, cur_clicking, prev_clicking, bar);
	}

	// Drag movement processing
	if (drag_parts && cur_clicking) {
		bool cursor_moved = (cur_pos.x != parts_prev_pos.x ||
				cur_pos.y != parts_prev_pos.y);
		if (cursor_moved && drag_parts->draggable) {
			Point new_pos = {
				drag_initial_pos.x + (cur_pos.x - drag_start_cursor.x),
				drag_initial_pos.y + (cur_pos.y - drag_start_cursor.y)
			};
			parts_set_pos(drag_parts, new_pos);
			if (!is_dragging) {
				parts_msg_push(drag_parts, PARTS_MSG_DRAG_BEGIN, "");
			}
			parts_msg_push(drag_parts, PARTS_MSG_DRAGGING, "iiii",
					drag_start_cursor.x, drag_start_cursor.y, cur_pos.x, cur_pos.y);
			is_dragging = true;
		}

		// Drop target tracking
		if (cursor_moved && is_dragging) {
			struct parts *new_drop = NULL;
			PARTS_LIST_FOREACH_REVERSE(parts) {
				if (parts == drag_parts || parts->gui_controller < gui_controller_top)
					continue;
				if (parts_hittest(parts, PARTS_STATE_DEFAULT, cur_pos)) {
					new_drop = parts;
					if (!parts->pass_cursor)
						break;
				}
			}
			if (new_drop != drop_target) {
				if (drop_target) {
					parts_msg_push(drop_target, PARTS_MSG_DROP_LEAVE,
							"i", drag_parts->no);
				}
				if (new_drop) {
					parts_msg_push(new_drop, PARTS_MSG_DROP_ENTER,
							"i", drag_parts->no);
				}
				drop_target = new_drop;
			} else if (drop_target) {
				parts_msg_push(drop_target, PARTS_MSG_DROP_ON,
						"iii", drag_parts->no, cur_pos.x, cur_pos.y);
			}
		}
	}

	// Release handling
	if (prev_clicking && !cur_clicking) {
		if (is_dragging && drag_parts && drag_parts->draggable) {
			parts_msg_push(drag_parts, PARTS_MSG_DRAG_END, "");
		}
		if (drop_target && drag_parts) {
			parts_msg_push(drop_target, PARTS_MSG_DROPPED,
					"iii", drag_parts->no, cur_pos.x, cur_pos.y);
			parts_msg_push(drop_target, PARTS_MSG_DROP_LEAVE, "i", drag_parts->no);
		}
		drag_state_reset();

		if (!click_down_parts) {
			// TODO: play misclick sound
		}
		click_down_parts = 0;
	}


	prev_clicking = cur_clicking;
	parts_prev_pos = cur_pos;
	// The scheduled input (src/input.c) learns that the input was read.
	input_auto_read();
}

static void classic_set_show_link(int parts_no, int link_parts_no)
{
	struct parts *parts = parts_get(parts_no);
	struct parts *link_parts = parts_get(link_parts_no);
	parts->linked_to = link_parts_no;
	link_parts->linked_from = parts_no;
}

static void classic_reset_input(void)
{
}

static void classic_begin_input(void)
{
	parts_began_click = true;
}

static void classic_end_input(void)
{
	parts_began_click = false;
	clicked_parts = 0;
	drag_state_reset();
}

const struct parts_input_ops parts_input_classic = {
	.begin_input = classic_begin_input,
	.end_input = classic_end_input,
	.reset_input = classic_reset_input,
	.update_input_state = classic_update_input_state,
	.set_show_link = classic_set_show_link,
};
