/* What this game's engine does: chosen once at startup, read everywhere else.
 *
 * Code outside src/profile.c asks for a behaviour by name, not for a version
 * or a game: profile.parts.controller_ids, not ain->version >= 14. A behaviour
 * is on only in the rows of src/profile.c that list it, so adding one for a
 * game changes nothing for the others. See docs/game-profiles.md.
 */

#ifndef SYSTEM4_PROFILE_H
#define SYSTEM4_PROFILE_H

#include <stdbool.h>
#include <stdio.h>

struct ain;
struct ain_hll_function;
struct parts_widget_ops;
struct parts_transform_ops;
struct parts_controller_ops;
struct parts_input_ops;

// How a generation binds PartsEngine (src/hll/PartsEngine.c). Any can be NULL.
struct parts_bindings {
	// Before the link: changes the export table.
	void (*prelink)(int libno);
	// After the link: fills what the export table does not have.
	void (*postlink)(int libno);
	// Picks the function for one declaration, or returns dflt.
	void *(*select)(const struct ain_hll_function *f, void *dflt);
};

// PartsEngine. Two generations: v14 (Haharanman, Dohna Dohna) and ain v7 with
// activities (妻みぐい３). Engines older than these have none of it.
struct parts_features {
	// Functions that differ as a whole (src/parts/parts_internal.h).
	const struct parts_widget_ops *widgets;
	const struct parts_controller_ops *controllers;
	const struct parts_transform_ops *transforms;
	const struct parts_input_ops *input;
	const struct parts_bindings *bindings;
	// The version of the parts in a save (src/parts/save.c).
	int save_version;

	/* ---- v14 ---- */
	// Local and global transforms as a tree, anchors, rotation, hit boxes.
	bool native_transforms;
	// Add after multiply, the clipper and draw filter rules, deforms.
	bool native_compositing;
	// Component types 17/18 and up: numerals and gauges kept as values.
	bool typed_components;
	// A controller is identified by the number of its root parts.
	bool controller_ids;
	// Input sessions on a stack of scopes (v14_update_input).
	bool input_sessions;
	// A script's wheelable flag on parts (Parts_SetWheelable, pactex).
	bool wheelable_parts;
	// The message window lays its text out itself.
	bool native_message_window;
	// The construction process commands of v14 pactex.
	bool native_construction;
	// PE_Update with no time passed falls back to the wall clock.
	bool update_clock_fallback;
	// A pactex panel's アルファグラデーション.
	bool panel_alpha_gradation;
	// A pactex button's text.
	bool pactex_button_text;

	/* ---- v14 and ain v7 with activities ---- */
	// Check boxes, scrollbars, layout boxes as widgets of an activity.
	bool activity_widgets;
	// The save section of an activity engine (format version 7 and up).
	bool activity_save;

	/* ---- ain v7 with activities ---- */
	// Hidden parts are not hovered, no hover state without a click, KeyPress.
	bool activity_pointer;
	// The Whole key, wheel and left click events.
	bool whole_key_events;
	// Widgets report through the parts message queue.
	bool widget_parts_messages;
	// Changing a child's margins lays its layout box out again.
	bool margin_relayout;
	// Layers are stack positions, shown through the parts of that number.
	bool layers_by_position;
	// has_widget_type, no_back_scene and the input depth in the save.
	bool save_activity_fields;
	// PartsEngine.Update also advances the parts movies.
	bool movies_in_update;

	/* ---- ain v8 with activities ---- */
	// Message types from 0: MouseEnter..MouseClick are 0..4, MouseDoubleClick
	// 5, the rest as before; GetMessageType is -1 on an empty queue.
	bool messages_from_zero;
	// The Whole events also have the mouse: a MouseMove when the cursor
	// moves, and the mouse buttons among the Whole key events (the network
	// dialog's KeyTrigger handler takes VK_LBUTTON).
	bool whole_mouse_events;
	// A parts whose parent is set goes, with its subtree, to the parent's
	// controller: the message window's system buttons, made under another
	// layer, are drawn over the window they hang on.
	bool child_takes_parent_controller;
	// An activity's message window (EPartsType 10) answers GetComponentType
	// with 10: Ａ＿システムボタン設定 looks up the button's parents for it.
	bool message_window_type;
	// An activity's テキストボックス (EPartsType 4) is one: it is drawn, takes
	// the keyboard focus and typed text, and sends Fixed on Return.
	bool text_box;
	// SetParentPartsNumber, AddChild and InsertChild attach at once, as the
	// original does: the script reads the tree back in the same call
	// (■Ｐ＿子コピー moves children while NumofChild(src) > 0). Pending until
	// the update, the save confirmation's text box was left behind when its
	// template was deleted, without a parent.
	bool immediate_parents;
	// SetComponentRotateX/Y turn the parts in depth through the original's
	// camera (parts_rotate_xy, parts_perspective) with the classic
	// transforms: C_DOT_PATTERN's tiles flip, CASTaskParts rotate events.
	bool xy_rotation;
	// Boxes by their origin mode as v14 (layoutbox.c): a vertical/horizontal
	// box sizes a child box by its content (lb_v14_box_size), a free box
	// moves its children by its own origin mode and size (lb_v14_free_layout),
	// and the boxes above a parts lay out again when its size changes. The
	// CG and 回想 page tabs (a horizontal box in a vertical box with origin 3,
	// top right, at x 1258) and the CG viewer's counter (a free box with
	// origin 9, bottom right, at 1270,710) were drawn off the screen.
	bool box_origin_layout;
	// A parts' alpha clipper also masks its children, the nearest one up the
	// chain as with native_compositing (parts_effective_alpha_clipper): the
	// day change's rolling digits hang under a window whose clipper hides
	// the old and new rows outside it, and were all drawn.
	bool inherited_alpha_clipper;
};

// Other libraries.
struct hll_features {
	// The mixers keep samples above full scale and a limiter on the output
	// (audio_mixer.c, soft_limit) brings them down, where each mixer clipped
	// them: a loud voice over music no longer crackles on its peaks.
	bool soft_limiter;
	// The log of AnteaterADVEngine as v14 keeps it, reset on a restart.
	bool adv_log_v14;
	// The window settings structure of v14 SystemService.
	bool window_settings_v14;
	// SystemService's table of mouse cursors, not the single one.
	bool mouse_cursor_configs;
	// Math.Rand from the v14 generator (R521).
	bool math_r521;
	// The Mersenne Twister of Math is seeded from the clock.
	bool math_clock_seed;
	// The wheel in notches, for a script reading it as v14 does.
	bool wheel_notches;
	// MsgSkip's flags as v14 stores them.
	bool msgskip_v14;
	// Sound groups, when the game has the table for them.
	bool sound_groups;
	// Volume.sav for the mixers of AliceStart.ini, when there are no groups.
	bool volume_sav;
	// A .fnl size draws from the face of 4 times its height, when there is one.
	bool fnl_face_by_scale;
	// A .fnl text advances by its edges only, not also by its bold width:
	// TextSurfaceManager.GetFontWidth, which the scripts size text
	// surfaces by, leaves the bold out.
	bool fnl_bold_no_advance;
};

// What the VM's values look like to a library.
struct lang_features {
	// Arrays typed by element, with shared struct pages, and generic options.
	bool typed_arrays;
	// The script's strings are SJIS whatever the legacy GB18030 score says.
	bool strings_are_sjis;
	// A delegate set to a method with no function (number below 1) is empty.
	bool no_method_empty_delegate;
};

// One game's oddities, keyed by name in src/hacks.c.
struct game_quirks {
	bool daibanchou_en;
	bool rance02_mg;
	bool rance6_mg;
	bool rance7_mg;
	bool rance8;
	bool rance8_mg;
	bool dungeons_and_dolls;
};

struct game_profile {
	// The row of src/profile.c.
	const char *row;
	struct parts_features parts;
	struct hll_features hll;
	struct lang_features lang;
	struct game_quirks quirks;
	// 1280x720 when AliceStart.ini has no size (the v14 message functions).
	bool default_view_720p;
};

extern struct game_profile profile;

// Call once, as soon as the ain is open.
void profile_init(const struct ain *ain);

// One line for the link report: the row and every behaviour it turned on.
void profile_report(FILE *f);

#endif /* SYSTEM4_PROFILE_H */
