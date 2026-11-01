/* parts.c: v14. Included at the end of parts.c. See docs/game-profiles.md. */

// The anchor: the transform applied to the origin, rounded to the pixel.
static Point parts_transform_anchor(mat4 m)
{
	return (Point) { (int)lroundf(m[3][0]), (int)lroundf(m[3][1]) };
}

static Point v14_anchor_pos(struct parts *parts)
{
	mat4 m;
	parts_level_transform(parts->parent ? &parts->parent->global : NULL,
			&parts->local, parts_placed_pos(parts), m);
	return parts_transform_anchor(m);
}

static void v14_place_global(struct parts *parts, const struct parts_params *parent)
{
	parts_level_transform(parent, &parts->local, parts_placed_pos(parts), parts->global.matrix);
	parts->global.pos = parts_transform_anchor(parts->global.matrix);
}

static void v14_update_transform(struct parts *parts)
{
	parts_update_global_pos(parts, parts->parent ? &parts->parent->global : NULL);
}

static void v14_load_transform(struct parts *parts)
{
	parts_level_transform(NULL, &parts->local, parts_placed_pos(parts), parts->global.matrix);
	parts_accumulate_add_color(NULL, parts->local.add_color, parts->sub_color_mode, &parts->global);
}

static Point v14_screen_upper_left(struct parts *parts, struct parts_common *common)
{
	// The corner in the box (0 unless a surface area moves it), through the
	// box transform: the flips mirror it around the anchor, and the scales
	// and rotations above the parts move it as they move the drawing.
	float x, y;
	parts_screen_upper_left_float(parts, common, &x, &y);
	return (Point) { (int)lroundf(x), (int)lroundf(y) };
}

static void v14_after_rotation(struct parts *parts)
{
	parts_update_transform(parts);
}

static void v14_combine_pos(struct parts_params *parent, struct parts_params *child,
		Point pos, struct parts_params *out)
{
	parts_level_transform(parent, child, pos, out->matrix);
	out->pos = parts_transform_anchor(out->matrix);
}

static void v14_set_gauge_cg(struct parts *parts, struct parts_gauge *g, struct cg *cg)
{
	gfx_delete_texture(&g->cg);
	gfx_delete_texture(&g->common.texture);
	gfx_init_texture_with_cg(&g->common.texture, cg);
	// v14 draws a cropped quad from the original image; no per-frame
	// render-to-texture copy. cg keeps source dimensions, not an alias.
	g->cg.w = cg->metrics.w; g->cg.h = cg->metrics.h;
	parts_set_dims(parts, &g->common, g->cg.w, g->cg.h);
	parts_dirty(parts);
	return;
}

static void v14_hgauge_set_rate(struct parts *parts, struct parts_gauge *g, float rate)
{
	g->numerator = rate; g->denominator = 1; g->rate = rate;
	parts_dirty(parts);
	return;
}

static void v14_vgauge_set_rate(struct parts *parts, struct parts_gauge *g, float rate)
{
	g->numerator = rate; g->denominator = 1; g->rate = rate;
	parts_dirty(parts);
	return;
}

static bool v14_set_hgauge_rate(int parts_no, float numerator, float denominator, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_gauge *g = parts_get_hgauge(parts, state);
	g->numerator = numerator; g->denominator = denominator;
	g->rate = denominator > 0 ? numerator / denominator : 1;
	parts_dirty(parts);
	return true;
}

static bool v14_set_vgauge_rate(int parts_no, float numerator, float denominator, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_gauge *g = parts_get_vgauge(parts, state);
	g->numerator = numerator; g->denominator = denominator;
	g->rate = denominator > 0 ? numerator / denominator : 1;
	parts_dirty(parts);
	return true;
}

static bool v14_set_hgauge_area(int parts_no, int x, int y, int w, int h, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_gauge *g = parts_get_hgauge(parts, state);
	g->common.surface_area = (Rectangle) {x, y, w, h};
	parts_common_recalculate_hitbox(parts, &g->common);
	parts_dirty(parts);
	// The gauge's size is now the area's.
	if (!state)
		parts_layout_size_changed(parts);
	return true;
}

static bool v14_set_vgauge_area(int parts_no, int x, int y, int w, int h, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_gauge *g = parts_get_vgauge(parts, state);
	g->common.surface_area = (Rectangle) {x, y, w, h};
	parts_common_recalculate_hitbox(parts, &g->common);
	parts_dirty(parts);
	// The gauge's size is now the area's.
	if (!state)
		parts_layout_size_changed(parts);
	return true;
}

static bool v14_set_numeral_cg(int parts_no, struct string *cg_name, int state)
{
	if (!parts_state_valid(--state))
		return false;
	// 0x5b49f0: nothing for the same name on a CG numeral; otherwise
	// the numeral becomes 表示タイプ 0.
	struct parts *parts = parts_get(parts_no);
	struct parts_numeral *n = parts_get_numeral(parts, state);
	if (n->show_type == PARTS_NUMERAL_SHOW_CG && !n->unknown_type
			&& !strcmp(n->cg_name ? n->cg_name->text : "", cg_name->text))
		return true;
	if (n->cg_name)
		free_string(n->cg_name);
	n->cg_name = string_dup(cg_name);
	n->show_type = PARTS_NUMERAL_SHOW_CG;
	n->unknown_type = false;
	parts_numeral_resolve_cg(n);
	return parts_numeral_update(parts, n);
}

static bool v14_set_numeral_linked_cg(int parts_no, struct string *cg_name,
		int w0, int w1, int w2, int w3, int w4, int w5, int w6, int w7, int w8,
		int w9, int w_minus, int w_comma, int state)
{
	if (!parts_state_valid(--state))
		return false;

	int w[12] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w_minus, w_comma };
	// 0x5b4ac0: nothing for the same name and widths on a linked CG
	// numeral; otherwise the numeral becomes 表示タイプ 1. True
	// whether the CG exists or not (0x5992f0).
	struct parts *parts = parts_get(parts_no);
	struct parts_numeral *n = parts_get_numeral(parts, state);
	if (n->show_type == PARTS_NUMERAL_SHOW_LINKED_CG
			&& !strcmp(n->cg_name ? n->cg_name->text : "", cg_name->text)
			&& !memcmp(n->cg_widths, w, sizeof(w)))
		return true;
	if (n->cg_name)
		free_string(n->cg_name);
	n->cg_name = string_dup(cg_name);
	memcpy(n->cg_widths, w, sizeof(w));
	n->show_type = PARTS_NUMERAL_SHOW_LINKED_CG;
	n->unknown_type = false;
	parts_numeral_resolve_cg(n);
	return parts_numeral_update(parts, n);
}

static bool v14_set_numeral_length(int parts_no, int length, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	// 0x567690: a length below 0 is 0; the comparison is with the
	// length as given.
	struct parts_numeral *num = parts_get_numeral(parts, state);
	if (num->length == length)
		return true;
	num->length = max(length, 0);
	return parts_numeral_update(parts, num);
}

static bool v14_set_numeral_area(int parts_no, int x, int y, int w, int h, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	// 0x567760 keeps the rectangle as it is given; the numeral is
	// not rebuilt.
	struct parts_numeral *n = parts_get_numeral(parts, state);
	n->common.surface_area = (Rectangle) { x, y, w, h };
	parts_common_recalculate_hitbox(parts, &n->common);
	parts_dirty(parts);
	if (!state)
		parts_layout_size_changed(parts);
	return true;
}

static bool parts_is_ancestor(struct parts *ancestor, struct parts *parts)
{
	for (struct parts *p = parts; p; p = p->parent) {
		if (p == ancestor)
			return true;
	}
	return false;
}

/* v14: the native setter (0x58f060 -> 0x58f310 -> 0x550d90) links the child
 * immediately and 0 puts it back under its controller's root (0x53ab60); an
 * unknown parent is ignored.
 * AIN walks the tree with NumofChild/GetChild in the same call, e.g.
 * activity::detail::CallUserComponentEventWithChild right after ReadFile. */
static void parts_set_parent_now(struct parts *parts, int parent_parts_no)
{
	// A controller's root stays where it is (0x53b018 for a parent,
	// 0x53ab93 for 0).
	if (parts_is_controller_root(parts))
		return;
	struct parts *parent = NULL;
	if (parent_parts_no != 0) {
		parent = parts_try_get(parent_parts_no);
		if (!parent || parts_is_ancestor(parts, parent))
			return;
	} else {
		// 0: under the root at the top of the parts' parent chain
		// (0x53ab60 -> 0x534f00). A tree without one (the system
		// overlay's, which has no root here) leaves the parts without a
		// parent.
		struct parts *top = parts;
		while (top->parent)
			top = top->parent;
		parent = parts_is_controller_root(top) ? top
			: parts_controller_root(parts->controller_no);
	}
	parts->pending_parent = -1;
	if (parts->parent == parent)
		return;
	if (parts->parent) {
		struct parts *old = parts->parent;
		// Before leaving: the boxes above the old parent lose its size.
		parts_layout_size_changed(parts);
		TAILQ_REMOVE(&old->children, parts, child_list_entry);
		if (old->states[0].type == PARTS_LAYOUT_BOX)
			parts_component_dirty(old);
		parts_user_component_dirty(old);
	}
	parts->parent = parent;
	if (parent) {
		if (profile.parts.controller_ids && parent->controller_no != parts->controller_no) {
			static unsigned traced;
			if (dev_getenv("XSYS4_STAGE2_TRACE") && traced++ < 60)
				WARNING("S2 layer-root reparent: parts %d (controller %d) under %d (controller %d)",
					parts->no, parts->controller_no, parent->no, parent->controller_no);
			parts_set_controller(parts, parent->controller_no);
		}
		TAILQ_INSERT_TAIL(&parent->children, parts, child_list_entry);
		if (parent->states[0].type == PARTS_LAYOUT_BOX)
			parts_component_dirty(parent);
		parts_user_component_dirty(parent);
		// E.g. a user component's content root (the component's size).
		parts_layout_size_changed(parts);
	}
	parts_component_dirty(parts);
}

static void v14_set_parent(int parts_no, int parent_parts_no)
{
	struct parts *parts = parts_get(parts_no);
	parts_set_parent_now(parts, parent_parts_no);
	return;
}

static void v14_set_component_type(int parts_no, int type, int state)
{
	// v14 (Dohna Dohna): component types are a game-level widget taxonomy
	// used by bytecode vtable dispatch via
	// GetComponentType, not a 1:1 mapping onto engine parts states. Store
	// the raw value; the actual parts state is built by the pactex loader
	// and the Create* calls.
	struct parts *parts = parts_get(parts_no);
	// Native 0x535e20: the widget already has the type -> nothing
	// (0x536540). A low-level widget (outer type 18) takes a state type
	// (19 and up) into that state (0x5653d0) and stays low-level, so
	// e.g. GetCGDetection's wrapper keeps a detection CG undrawn.
	// Any other type replaces the widget (raw value, state types
	// cleared).
	if (PE_GetComponentType(parts_no, state) == type)
		return;
	if (type > 18 && parts->component_type == 18 && state >= 1 && state <= PARTS_NR_STATES) {
		switch (type) {
		case 19: case 27: parts_get_cg(parts, state - 1); break;
		case 21: parts_get_text(parts, state - 1); break;
		case 22: parts_get_hgauge(parts, state - 1); break;
		case 23: parts_get_vgauge(parts, state - 1); break;
		case 24: parts_get_numeral(parts, state - 1); break;
		case 25:
			if (parts->states[state - 1].type != PARTS_RECT_DETECTION)
				parts_state_reset(&parts->states[state - 1], PARTS_RECT_DETECTION);
			break;
		case 26: parts_get_construction_process(parts, state - 1); break;
		default: break;
		}
		parts->component_state_type[state - 1] = type;
		parts_dirty(parts);
		return;
	}
	const int old_type = parts->component_type;
	parts->component_type = type;
	// A check box, a radio box or a 橫滾動條 that was another widget
	// has its constructor's values (checkbox.c).
	parts_widget_type_changed(parts, old_type);
	// Preserve explicit raw setters over the loader's inferred state types.
	memset(parts->component_state_type, 0, sizeof(parts->component_state_type));
	// A box a script makes (AFL_Parts_CreateLayoutBox ->
	// CLayoutBoxParts@0 -> SetComponentType 8) is the box widget with
	// its constructor's values (vertical, 配置 1; layoutbox.c): it lays
	// out the children AddChild gives it, as a pactex box lays out
	// its own. CBackLogUnitList@Init makes the backlog's lines one.
	if (type == 8)
		parts_get_layout_box(parts);
	parts_user_component_dirty(parts);
	return;
}

static int v14_get_component_type(int parts_no, int state)
{
	// v14: recognized low-level pactex states have their own native type.
	// Other widgets retain the raw value stored by PE_SetComponentType.
	// -1 is a valid "no parts" sentinel used by bytecode vtable dispatch.
	if (parts_no < 0)
		return -1;
	struct parts *parts = parts_try_get(parts_no);
	if (parts) {
		int native = state >= 1 && state <= PARTS_NR_STATES
			? parts->component_state_type[state - 1] : 0;
		if (native) {
			// A recognized state follows a later CG/text transition.
			switch (parts->states[state - 1].type) {
			case PARTS_TEXT: return 21;
			case PARTS_CG: return native == 21 || native == 22 || native == 23 ? 19 : native;
			case PARTS_NUMERAL: return 24;
			case PARTS_HGAUGE: return 22;
			case PARTS_VGAUGE: return 23;
			default: return native;
			}
		}
		return parts->component_type;
	}
	// Bytecode may probe numbers allocated by GetFreeNumber before
	// the parts entry exists; auto-create so Wrap/IsValid work.
	if (parts_no >= 1000000000)
		return parts_get(parts_no)->component_type;
	return -1;
}

// The smallest ID that is not in use. The original takes the ID from the
// allocator of free parts numbers, which also hands out the lowest free one
// (0x418650). v14: a parts number from PARTS_CONTROLLER_ID_BASE up, skipping
// a number some parts already has (the script may Wrap a layer ID it kept
// after erasing the layer, which creates a parts of that number); older
// engines: the position, so that pushes and pops keep ID == position.
static int v14_alloc_controller_id(void)
{
	int id = PARTS_CONTROLLER_ID_BASE;
	while (parts_controller_index(id) >= 0 || (parts_table && parts_exists(id)))
		id++;
	return id;
}

static int v14_controller_default_id(int position)
{
	return PARTS_CONTROLLER_ID_BASE + position;
}

static bool v14_is_controller_root(struct parts *parts)
{
	return parts->no >= PARTS_CONTROLLER_ID_BASE && parts->no == parts->controller_no;
}

// v14: the root parts of a controller, numbered with the controller's ID.
// It has no state, so it is neither drawn nor hit, and saves leave it out.
// Made outside parts_get, so that it hangs under nothing (0x53d24d: the
// table's listener is off while 0x53d220 makes the roots).
static struct parts *ctrl_root_create(int id)
{
	struct parts *root = parts_alloc();
	root->no = id;
	root->controller_no = id;
	root->want_save = false;
	// Neutral for its children: a top-level parts keeps the global z it
	// had without a parent (parts_combine_params adds the parent's).
	root->local.z = root->global.z = 0;
	struct ht_slot *slot = ht_put_int(parts_table, id, NULL);
	slot->value = root;
	parts_list_insert(root);
	return root;
}

static struct parts *v14_controller_root(int id)
{
	if (id < PARTS_CONTROLLER_ID_BASE)
		return NULL;
	struct parts *root = parts_try_get(id);
	if (!root && parts_controller_index(id) >= 0)
		root = ctrl_root_create(id);
	return root;
}

static void v14_relink_roots(void)
{
	struct parts *parts;
	PARTS_LIST_FOREACH(parts) {
		if (!parts_is_controller_root(parts))
			ctrl_root_attach(parts);
	}
}

// v14: the ID is the number of the controller's root parts (0x53d220).
static void v14_controller_added(int id)
{
	if (parts_table)
		ctrl_root_create(id);
}

// Removes a controller from the stack, releases all parts belonging to it,
// and returns their delegate indices in `erase_number_list`.
//
// v14 (0x53d5f0 -> 0x53d500): `index` is the controller's ID; with any other
// value, -1 included, the active controller is removed. The last controller
// is never removed. The other controllers keep their IDs. When the active
// controller is the one removed, the one below it becomes active.
//
// Older engines: `index` is the position and -1 means the active controller.
// In practice the game only ever passes -1, and the active controller is
// always the top of the stack at that point, so this degenerates to a simple
// pop.
static void v14_remove_controller(struct page **erase_number_list, int index)
{
	int pos = parts_controller_index(index);
	if (ctrl_stack.nr_controllers <= 1)
		return;
	if (pos < 0)
		pos = ctrl_active_index();
	int id = ctrl_stack.ids[pos];
	ctrl_release_parts(erase_number_list, id);
	ctrl_remove_at(pos);
	if (ctrl_stack.active == id)
		ctrl_stack.active = ctrl_stack.ids[max(pos - 1, 0)];
	ctrl_resort_from(pos);
	return;
}

static void v14_set_active_controller(int controller_no)
{
	// An ID that is not there leaves no controller designated
	// (0x53d870 returns null and the caller stores it, 0x57ba60).
	if (controller_no != PARTS_CONTROLLER_SYSTEM_OVERLAY
			&& parts_controller_index(controller_no) < 0)
		controller_no = -1;
	ctrl_stack.active = controller_no;
	return;
}

static int v14_get_active_controller(void)
{
	if (ctrl_stack.active < 0) {
		// None designated: the top controller acts, and an empty stack
		// gets its default controller first (0x58a6f0 -> 0x53e640).
		if (!ctrl_stack.nr_controllers)
			return PE_AddController(-1);
		return ctrl_stack.ids[ctrl_stack.nr_controllers - 1];
	}
	return ctrl_stack.active;
}

const struct parts_widget_ops parts_widget_v14 = {
	.set_gauge_cg = v14_set_gauge_cg,
	.hgauge_set_rate = v14_hgauge_set_rate,
	.vgauge_set_rate = v14_vgauge_set_rate,
	.set_hgauge_rate = v14_set_hgauge_rate,
	.set_vgauge_rate = v14_set_vgauge_rate,
	.set_hgauge_area = v14_set_hgauge_area,
	.set_vgauge_area = v14_set_vgauge_area,
	.set_numeral_cg = v14_set_numeral_cg,
	.set_numeral_linked_cg = v14_set_numeral_linked_cg,
	.set_numeral_length = v14_set_numeral_length,
	.set_numeral_area = v14_set_numeral_area,
	.set_component_type = v14_set_component_type,
	.get_component_type = v14_get_component_type,
	.set_parent = v14_set_parent,
};

const struct parts_transform_ops parts_transform_v14 = {
	.anchor_pos = v14_anchor_pos,
	.place_global = v14_place_global,
	.update_transform = v14_update_transform,
	.load_transform = v14_load_transform,
	.after_rotation = v14_after_rotation,
	.screen_upper_left = v14_screen_upper_left,
	.combine_pos = v14_combine_pos,
	.screen_to_box = v14_screen_to_box,
	.layout_size_changed = v14_layout_size_changed,
	.do_layout = v14_do_layout,
	.anchor_transform = v14_anchor_transform,
	.clip_area_transform = v14_clip_area_transform,
};

const struct parts_controller_ops parts_controller_v14 = {
	.alloc_id = v14_alloc_controller_id,
	.default_id = v14_controller_default_id,
	.is_root = v14_is_controller_root,
	.root = v14_controller_root,
	.relink_roots = v14_relink_roots,
	.controller_added = v14_controller_added,
	.parts_released = pe_activity_parts_released,
	.remove_controller = v14_remove_controller,
	.set_active_controller = v14_set_active_controller,
	.get_active_controller = v14_get_active_controller,
};
