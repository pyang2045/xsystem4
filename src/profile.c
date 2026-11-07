/* The behaviour table: the only place that turns a version, a declaration set
 * or a game into behaviour (see include/profile.h, docs/game-profiles.md).
 *
 * Fields left out of a row are false, so a new behaviour affects exactly the
 * rows it is added to.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "system4.h"
#include "system4/ain.h"

#include "profile.h"

struct game_profile profile;

extern const struct parts_widget_ops parts_widget_v14, parts_widget_v7, parts_widget_classic;
extern const struct parts_transform_ops parts_transform_v14, parts_transform_v7, parts_transform_classic;
extern const struct parts_controller_ops parts_controller_v14, parts_controller_v7, parts_controller_classic;
extern const struct parts_input_ops parts_input_v14, parts_input_v7, parts_input_classic;
extern const struct parts_bindings parts_bindings_v14, parts_bindings_v8, parts_bindings_v7, parts_bindings_classic;

static bool has_parts_function(const struct ain *ain, const char *name)
{
	int libno = ain_get_library((struct ain *)ain, "PartsEngine");
	return libno >= 0 && ain_get_library_function((struct ain *)ain, libno, name) >= 0;
}

static bool is_v14(const struct ain *ain)
{
	return ain->version >= 14;
}

// The message functions of v14 tell a v14 script apart.
static bool is_activities_v7(const struct ain *ain)
{
	return ain->version == 7 && has_parts_function(ain, "CreateActivity")
		&& !has_parts_function(ain, "GetMessageUniqueID");
}

// は～とふるママン: ain v8 with activities and the v7 declarations (ref, not
// wrap), plus v14's message windows and construction processes.
static bool is_activities_v8(const struct ain *ain)
{
	return ain->version == 8 && has_parts_function(ain, "CreateActivity")
		&& !has_parts_function(ain, "GetMessageUniqueID");
}

// Oyako Rankan: PartsEngine next to GUIEngine, with PartsEngine's own stack
// of GUI controllers.
static bool is_gui_parts_v6(const struct ain *ain)
{
	return ain->version == 6 && has_parts_function(ain, "PushGUIController");
}

static const struct {
	const char *name;
	bool (*match)(const struct ain *ain);
	struct parts_features parts;
	struct hll_features hll;
	struct lang_features lang;
	// 1280x720 when AliceStart.ini has no size, if this function is declared.
	const char *view_720p_with;
} rows[] = {
	{
		.name = "v14",
		.match = is_v14,
		.parts = {
			.widgets = &parts_widget_v14,
			.controllers = &parts_controller_v14,
			.transforms = &parts_transform_v14,
			.input = &parts_input_v14,
			.bindings = &parts_bindings_v14,
			.save_version = 14,
			.native_transforms = true,
			.native_compositing = true,
			.typed_components = true,
			.controller_ids = true,
			.input_sessions = true,
			.wheelable_parts = true,
			.native_message_window = true,
			.native_construction = true,
			.update_clock_fallback = true,
			.panel_alpha_gradation = true,
			.pactex_button_text = true,
			.activity_widgets = true,
			.activity_save = true,
		},
		.hll = {
			.adv_log_v14 = true,
			.window_settings_v14 = true,
			.mouse_cursor_configs = true,
			.math_r521 = true,
			.math_clock_seed = true,
			.wheel_notches = true,
			.msgskip_v14 = true,
			.sound_groups = true,
		},
		.lang = { .typed_arrays = true },
		.view_720p_with = "SeekMessage",
	},
	{
		.name = "activities_v7",
		.match = is_activities_v7,
		.parts = {
			.widgets = &parts_widget_v7,
			.controllers = &parts_controller_v7,
			.transforms = &parts_transform_v7,
			.input = &parts_input_v7,
			.bindings = &parts_bindings_v7,
			.save_version = 15,
			.activity_widgets = true,
			.activity_save = true,
			.activity_pointer = true,
			.whole_key_events = true,
			.widget_parts_messages = true,
			.margin_relayout = true,
			.layers_by_position = true,
			.save_activity_fields = true,
			.movies_in_update = true,
		},
		.hll = { .sound_groups = true, .fnl_face_by_scale = true },
		.lang = { .no_method_empty_delegate = true },
	},
	{
		.name = "activities_v8",
		.match = is_activities_v8,
		.parts = {
			.widgets = &parts_widget_v7,
			.controllers = &parts_controller_v7,
			.transforms = &parts_transform_v7,
			.input = &parts_input_v7,
			.bindings = &parts_bindings_v8,
			.save_version = 16,
			.activity_widgets = true,
			.activity_save = true,
			.activity_pointer = true,
			.whole_key_events = true,
			.widget_parts_messages = true,
			.margin_relayout = true,
			.layers_by_position = true,
			.save_activity_fields = true,
			.movies_in_update = true,
			.messages_from_zero = true,
		},
		.hll = { .sound_groups = true, .fnl_face_by_scale = true },
		.lang = { .no_method_empty_delegate = true },
		// 1280x720 (its CGs); AliceStart.ini has no size.
		.view_720p_with = "CreateActivity",
	},
	{
		.name = "gui_parts_v6",
		.match = is_gui_parts_v6,
		.parts = {
			.widgets = &parts_widget_classic,
			.controllers = &parts_controller_classic,
			.transforms = &parts_transform_classic,
			.input = &parts_input_classic,
			.bindings = &parts_bindings_classic,
			.save_version = 3,
		},
		// Its config screens read all three (■入力 tab of the config).
		// The config screens set the volumes and never store them: the
		// engine keeps them in Volume.sav (sys40::CSoundVolumeManager,
		// read 0x463f00, written 0x463ad0, as with the groups).
		.hll = { .mouse_cursor_configs = true, .volume_sav = true },
		// The legacy score fires (41 on the JP script, 39 on the Chinese one, whose
		// text is valid SJIS too), and text with a TrueType face came out as GBK.
		.lang = { .strings_are_sjis = true },
	},
	// Keep last: it takes every game the rows above don't.
	{
		.name = "classic",
		.parts = {
			.widgets = &parts_widget_classic,
			.controllers = &parts_controller_classic,
			.transforms = &parts_transform_classic,
			.input = &parts_input_classic,
			.bindings = &parts_bindings_classic,
			// Older engines retain their byte-for-byte layout.
			.save_version = 3,
		},
	},
};

void profile_init(const struct ain *ain)
{
	size_t i = 0;
	while (rows[i].match && !rows[i].match(ain))
		i++;
	profile.row = rows[i].name;
	profile.parts = rows[i].parts;
	profile.hll = rows[i].hll;
	profile.lang = rows[i].lang;
	profile.default_view_720p = rows[i].view_720p_with
		&& has_parts_function(ain, rows[i].view_720p_with);

	// No row was written for such a game: say so, rather than bind half of one.
	if (!rows[i].match && (has_parts_function(ain, "CreateActivity")
			|| has_parts_function(ain, "GetMessageUniqueID")))
		WARNING("ain v%d declares PartsEngine activities or v14 messages, "
			"but no row of src/profile.c matches it", ain->version);
}

static void report_bytes(FILE *f, const void *p, size_t size)
{
	for (size_t i = 0; i < size; i++)
		fprintf(f, "%02x", ((const unsigned char *)p)[i]);
}

void profile_report(FILE *f)
{
	fprintf(f, "#profile %s:%d:", profile.row, profile.parts.save_version);
	report_bytes(f, &profile.parts.native_transforms,
		sizeof(profile.parts) - offsetof(struct parts_features, native_transforms));
	fputc(':', f);
	report_bytes(f, &profile.hll, sizeof(profile.hll));
	fputc(':', f);
	report_bytes(f, &profile.lang, sizeof(profile.lang));
	fputc(':', f);
	report_bytes(f, &profile.quirks, sizeof(profile.quirks));
	fprintf(f, ":%d -\n", profile.default_view_720p);
}
