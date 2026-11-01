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

struct ain;
struct parts_widget_ops;
struct parts_controller_ops;
struct parts_input_ops;

// PartsEngine. Two generations: v14 (Haharanman, Dohna Dohna) and ain v7 with
// activities (妻みぐい３). Engines older than these have none of it.
struct parts_features {
	// Functions that differ as a whole (src/parts/parts_internal.h).
	const struct parts_widget_ops *widgets;
	const struct parts_controller_ops *controllers;
	const struct parts_input_ops *input;

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
	// The declarations written for v14 (gauges, numerals, sound names, ...).
	bool v14_bindings;

	/* ---- v14 and ain v7 with activities ---- */
	// Check boxes, scrollbars, layout boxes as widgets of an activity.
	bool activity_widgets;
	// The save section of an activity engine (format version 7 and up).
	bool activity_save;

	/* ---- ain v7 with activities ---- */
	// The declarations written for ain v7 with activities.
	bool v7_bindings;
	// Hidden parts are not hovered, no hover state without a click, KeyPress.
	bool activity_pointer;
	// The Whole key and wheel events.
	bool whole_key_events;
	// Widgets report through the parts message queue.
	bool widget_parts_messages;
	// Changing a child's margins lays its layout box out again.
	bool margin_relayout;
	// Layers are stack positions, shown through the parts of that number.
	bool layers_by_position;
	// has_widget_type, no_back_scene and the input depth in the save.
	bool save_activity_fields;

	/* ---- other ---- */
	// PartsEngine.Update also advances the parts movies (all of ain v7).
	bool movies_in_update;
};

// Other libraries.
struct hll_features {
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
};

// What the VM's values look like to a library.
struct lang_features {
	// Arrays typed by element, with shared struct pages, and generic options.
	bool typed_arrays;
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

#endif /* SYSTEM4_PROFILE_H */
