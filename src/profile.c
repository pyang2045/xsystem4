/* The behaviour table: the only place that turns a version, a declaration set
 * or a game into behaviour (see include/profile.h, docs/game-profiles.md).
 *
 * Fields left out of a row are false, so a new behaviour affects exactly the
 * rows it is added to.
 */

#include <stdbool.h>

#include "system4/ain.h"

#include "profile.h"

struct game_profile profile;

extern const struct parts_widget_ops parts_widget_v14, parts_widget_classic;
extern const struct parts_controller_ops parts_controller_v14, parts_controller_classic;
extern const struct parts_input_ops parts_input_v14, parts_input_v7, parts_input_classic;

static const struct parts_features parts_v14 = {
	.widgets = &parts_widget_v14,
	.controllers = &parts_controller_v14,
	.input = &parts_input_v14,
	.native_transforms = true,
	.native_compositing = true,
	.typed_components = true,
	.controller_ids = true,
	.input_sessions = true,
	.wheelable_parts = true,
	.native_message_window = true,
	.native_construction = true,
	.update_clock_fallback = true,
	.v14_bindings = true,
	.activity_widgets = true,
	.activity_save = true,
};

static const struct parts_features parts_activities_v7 = {
	.widgets = &parts_widget_classic,
	.controllers = &parts_controller_classic,
	.input = &parts_input_v7,
	.activity_widgets = true,
	.activity_save = true,
	.v7_bindings = true,
	.activity_pointer = true,
	.whole_key_events = true,
	.widget_parts_messages = true,
	.margin_relayout = true,
	.layers_by_position = true,
	.save_activity_fields = true,
};

static const struct parts_features parts_classic = {
	.widgets = &parts_widget_classic,
	.controllers = &parts_controller_classic,
	.input = &parts_input_classic,
};

static bool has_parts_function(const struct ain *ain, const char *name)
{
	int libno = ain_get_library((struct ain *)ain, "PartsEngine");
	return libno >= 0 && ain_get_library_function((struct ain *)ain, libno, name) >= 0;
}

void profile_init(const struct ain *ain)
{
	// An ain below v14 with activities has the v7 declaration set; the
	// message functions of v14 tell a v14 script apart.
	if (ain->version >= 14)
		profile.parts = parts_v14;
	else if (has_parts_function(ain, "CreateActivity") && !has_parts_function(ain, "GetMessageUniqueID"))
		profile.parts = parts_activities_v7;
	else
		profile.parts = parts_classic;
	profile.parts.movies_in_update = ain->version == 7;

	if (ain->version >= 14) {
		profile.hll = (struct hll_features) {
			.adv_log_v14 = true,
			.window_settings_v14 = true,
			.mouse_cursor_configs = true,
			.math_r521 = true,
			.math_clock_seed = true,
			.wheel_notches = true,
			.msgskip_v14 = true,
		};
		profile.lang.typed_arrays = true;
		profile.default_view_720p = has_parts_function(ain, "SeekMessage");
	}
	profile.hll.sound_groups = ain->version == 14 || ain->version == 7;
}
