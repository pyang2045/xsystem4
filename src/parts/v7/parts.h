/* parts.c: ain v7 with activities (妻みぐい３). Included at the end of parts.c. See docs/game-profiles.md. */

const struct parts_transform_ops parts_transform_v7 = {
	.anchor_pos = classic_anchor_pos,
	.place_global = classic_place_global,
	.update_transform = classic_update_transform,
	.load_transform = classic_load_transform,
	.after_rotation = classic_after_rotation,
	.screen_upper_left = classic_screen_upper_left,
	.combine_pos = classic_combine_pos,
	.screen_to_box = classic_screen_to_box,
	.layout_size_changed = classic_layout_size_changed,
	.do_layout = classic_do_layout,
	.anchor_transform = classic_anchor_transform,
	.clip_area_transform = classic_clip_area_transform,
};

// ain v7 with activities: as classic so far. A change for these games replaces
// an entry here with a v7_ function, and classic_ stays as it is.
const struct parts_widget_ops parts_widget_v7 = {
	.set_gauge_cg = classic_set_gauge_cg,
	.hgauge_set_rate = classic_hgauge_set_rate,
	.vgauge_set_rate = classic_vgauge_set_rate,
	.set_hgauge_rate = classic_set_hgauge_rate,
	.set_vgauge_rate = classic_set_vgauge_rate,
	.set_hgauge_area = classic_set_hgauge_area,
	.set_vgauge_area = classic_set_vgauge_area,
	.set_numeral_cg = classic_set_numeral_cg,
	.set_numeral_linked_cg = classic_set_numeral_linked_cg,
	.set_numeral_length = classic_set_numeral_length,
	.set_numeral_area = classic_set_numeral_area,
	.set_component_type = classic_set_component_type,
	.get_component_type = classic_get_component_type,
	.set_parent = classic_set_parent,
};

const struct parts_controller_ops parts_controller_v7 = {
	.alloc_id = classic_alloc_controller_id,
	.default_id = classic_controller_default_id,
	.is_root = classic_is_controller_root,
	.root = classic_controller_root,
	.relink_roots = classic_relink_roots,
	.controller_added = classic_controller_added,
	.parts_released = classic_parts_released,
	.remove_controller = classic_remove_controller,
	.set_active_controller = classic_set_active_controller,
	.get_active_controller = classic_get_active_controller,
};
