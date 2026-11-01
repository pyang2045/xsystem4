/* parts.c: engines before v14 (and what ain v7 shares). Included at the end of parts.c. See docs/game-profiles.md. */

/*
 * Before v14: a child's position relative to its parent's anchor (the
 * parent's global position). The parent's reverse flags mirror it around
 * that anchor and the parent's accumulated scale scales it, each level
 * rounded to the pixel. Rotation does not move the children.
 */
static Point parts_child_pos(const struct parts_params *parent, Point local)
{
	float x = parent->scale.x * local.x, y = parent->scale.y * local.y;
	return (Point) {
		parent->pos.x + (int)lroundf(parent->reverse_lr ? -x : x),
		parent->pos.y + (int)lroundf(parent->reverse_tb ? -y : y)
	};
}

// Where the parts' anchor is on screen, from the parent's current transform.
static Point classic_anchor_pos(struct parts *parts)
{
	Point anchor = parts_placed_pos(parts);
	if (parts->parent)
		anchor = parts_child_pos(&parts->parent->global, anchor);
	return anchor;
}

static void classic_place_global(struct parts *parts, const struct parts_params *parent)
{
	Point pos = parts_placed_pos(parts);
	if (parent) {
		parts->global.pos = parts_child_pos(parent, pos);
	} else {
		parts->global.pos = (Point) { root_pos.x + pos.x, root_pos.y + pos.y };
	}
}

// After a change of this parts' own transform: itself and its subtree.
// Before v14 the parts' own scale and flips do not move its anchor, only
// its children's.
static void classic_update_transform(struct parts *parts)
{
	struct parts *child;
	PARTS_FOREACH_CHILD(child, parts) {
		parts_update_global_pos(child, &parts->global);
	}
}

/*
 * The transform and the add colour sum of a parts without a parent, from
 * its local parameters only; neither is saved. After PE_Load (the parent is
 * attached by the update PE_Load ends with, which then recombines the
 * children) and in that update for a top-level parts. Before v14 nothing is
 * derived: the global parameters are saved and loaded as they are.
 */
static void classic_load_transform(struct parts *parts)
{
}

static Point classic_screen_upper_left(struct parts *parts, struct parts_common *common)
{
	// As it was: the corner's offset from the anchor, mirrored by the
	// flips and not scaled.
	Point anchor = parts_anchor_pos(parts);
	int rx = common->hitbox.x - parts->local.pos.x, ry = common->hitbox.y - parts->local.pos.y;
	return (Point) {
		anchor.x + (parts->global.reverse_lr ? -rx : rx),
		anchor.y + (parts->global.reverse_tb ? -ry : ry)
	};
}

// Before v14 a rotation does not move the children.
static void classic_after_rotation(possibly_unused struct parts *parts)
{
}

static void classic_combine_pos(struct parts_params *parent, possibly_unused struct parts_params *child,
		Point pos, struct parts_params *out)
{
	out->pos = parts_child_pos(parent, pos);
}

static void classic_set_gauge_cg(struct parts *parts, struct parts_gauge *g, struct cg *cg)
{
	gfx_init_texture_with_cg(&g->cg, cg);
	gfx_init_texture_with_cg(&g->common.texture, cg);

	gfx_init_texture_rgba(&g->common.texture, g->cg.w, g->cg.h, (SDL_Color){0,0,0,255});
	gfx_copy_with_alpha_map(&g->common.texture, 0, 0, &g->cg, 0, 0, g->cg.w, g->cg.h);

	parts_set_dims(parts, &g->common, g->cg.w, g->cg.h);

	parts_dirty(parts);
}

static void classic_hgauge_set_rate(struct parts *parts, struct parts_gauge *g, float rate)
{
	if (!g->common.texture.handle) {
		WARNING("HGauge texture uninitialized");
		return;
	}
	int pixels = rate * g->cg.w;
	gfx_copy_with_alpha_map(&g->common.texture, 0, 0, &g->cg, 0, 0, pixels, g->cg.h);
	gfx_fill_amap(&g->common.texture, pixels, 0, g->cg.w - pixels, g->cg.h, 0);
	parts_dirty(parts);
	g->rate = rate;
}

static void classic_vgauge_set_rate(struct parts *parts, struct parts_gauge *g, float rate)
{
	if (!g->common.texture.handle) {
		WARNING("VGauge texture uninitialized");
		return;
	}
	int pixels = rate * g->cg.h;
	gfx_copy_with_alpha_map(&g->common.texture, 0, pixels, &g->cg, 0, pixels, g->cg.w, g->cg.h - pixels);
	gfx_fill_amap(&g->common.texture, 0, 0, g->cg.w, pixels, 0);
	parts_dirty(parts);
	g->rate = rate;
}

static bool classic_set_hgauge_rate(int parts_no, float numerator, float denominator, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_gauge *g = parts_get_hgauge(parts, state);
parts_hgauge_set_rate(parts, g, numerator/denominator);
	return true;
}

static bool classic_set_vgauge_rate(int parts_no, float numerator, float denominator, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_gauge *g = parts_get_vgauge(parts, state);
parts_vgauge_set_rate(parts, g, (float)numerator/(float)denominator);
	return true;
}

static bool classic_set_hgauge_area(int parts_no, int x, int y, int w, int h, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_gauge *g = parts_get_hgauge(parts, state);
	parts_set_surface_area(parts, &g->common, x, y, w, h);
	return true;
}

static bool classic_set_vgauge_area(int parts_no, int x, int y, int w, int h, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_gauge *g = parts_get_vgauge(parts, state);
	parts_set_surface_area(parts, &g->common, x, y, w, h);
	return true;
}

static bool classic_set_numeral_cg(int parts_no, struct string *cg_name, int state)
{
	if (!parts_state_valid(--state))
		return false;
	struct parts_numeral *n = parts_get_numeral(parts_get(parts_no), state);
	n->font_no = parts_load_numeral_font_separate_string(cg_name);
	return true;
}

static bool classic_set_numeral_linked_cg(int parts_no, struct string *cg_name,
		int w0, int w1, int w2, int w3, int w4, int w5, int w6, int w7, int w8,
		int w9, int w_minus, int w_comma, int state)
{
	if (!parts_state_valid(--state))
		return false;

	int w[12] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w_minus, w_comma };
	int no;
	struct cg *cg = asset_cg_load_by_name(cg_name->text, &no);
	if (!cg)
		return false;

	struct parts_numeral *n = parts_get_numeral(parts_get(parts_no), state);
	n->font_no = parts_load_numeral_font_combined(cg, no, w);
	return true;
}

static bool classic_set_numeral_length(int parts_no, int length, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_numeral *num = parts_get_numeral(parts, state);
	if (num->length == length)
		return true;

	num->length = max(1, length);
	num->have_num = true;
	parts_numeral_update(parts, num);
	return true;
}

static bool classic_set_numeral_area(int parts_no, int x, int y, int w, int h, int state)
{
	if (!parts_state_valid(--state))
		return false;

	struct parts *parts = parts_get(parts_no);
	struct parts_numeral *n = parts_get_numeral(parts, state);
	parts_set_surface_area(parts, &n->common, x, y, w, h);
	return true;
}

static void classic_set_parent(int parts_no, int parent_parts_no)
{
	struct parts *parts = parts_get(parts_no);
	parts->pending_parent = parent_parts_no;
	parts_component_dirty(parts);
}

static void classic_set_component_type(int parts_no, int type, int state)
{
	// v14 (Dohna Dohna): component types are a game-level widget taxonomy
	// used by bytecode vtable dispatch via
	// GetComponentType, not a 1:1 mapping onto engine parts states. Store
	// the raw value; the actual parts state is built by the pactex loader
	// and the Create* calls.
	if (!parts_state_valid(--state))
		return;
	struct parts *parts = parts_get(parts_no);
	// The widgets of the older games with the v14 parts library (ain v7
	// 妻みぐい３: CButtonParts and the like): 0 to 9 as v14.
	if (type >= 0 && type <= 9 && type != 8) {
		parts->component_type = type;
		parts->has_widget_type = true;
		parts_dirty(parts);
		return;
	}
	parts->has_widget_type = false;
	enum parts_type pt = PARTS_UNINITIALIZED;
	switch (type) {
	case 8:  pt = PARTS_LAYOUT_BOX; break;
	case 11: pt = PARTS_CG; break;
	case 12: pt = PARTS_ANIMATION; break;
	case 13: pt = PARTS_TEXT; break;
	case 14: pt = PARTS_HGAUGE; break;
	case 15: pt = PARTS_VGAUGE; break;
	case 16: pt = PARTS_NUMERAL; break;
	case 17: pt = PARTS_RECT_DETECTION; break;
	case 18: pt = PARTS_CONSTRUCTION_PROCESS; break;
	case 20: pt = PARTS_FLAT; break;
	case 21: pt = PARTS_3DLAYER; break;
	case 22: pt = PARTS_MOVIE; break;
	default:
		VM_ERROR("unknown component type %d", type);
	}
	if (parts->states[state].type != pt)
		parts_state_reset(&parts->states[state], pt);
}

static int classic_get_component_type(int parts_no, int state)
{
	// v14: recognized low-level pactex states have their own native type.
	// Other widgets retain the raw value stored by PE_SetComponentType.
	// -1 is a valid "no parts" sentinel used by bytecode vtable dispatch.
	if (!parts_state_valid(--state))
		return -1;
	struct parts *parts = parts_try_get(parts_no);
	if (!parts)
		return -1;
	if (parts->has_widget_type)
		return parts->component_type;

	switch (parts->states[state].type) {
	case PARTS_LAYOUT_BOX: return 8;
	case PARTS_UNINITIALIZED:  // defaluts to CG
	case PARTS_CG:
		return 11;
	case PARTS_ANIMATION: return 12;
	case PARTS_TEXT: return 13;
	case PARTS_HGAUGE: return 14;
	case PARTS_VGAUGE: return 15;
	case PARTS_NUMERAL: return 16;
	case PARTS_RECT_DETECTION: return 17;
	case PARTS_CONSTRUCTION_PROCESS: return 18;
	case PARTS_FLAT: return 20;
	case PARTS_3DLAYER: return 21;
	case PARTS_MOVIE: return 22;
	case PARTS_FLASH:
		break;
	}
	VM_ERROR("unsupported component type %d", parts->states[state].type);
}

static int classic_alloc_controller_id(void)
{
	int id = 0;
	while (parts_controller_index(id) >= 0)
		id++;
	return id;
}

static int classic_controller_default_id(int position)
{
	return position;
}

// Before v14 a controller has no root parts.
static bool classic_is_controller_root(possibly_unused struct parts *parts)
{
	return false;
}

static struct parts *classic_controller_root(possibly_unused int id)
{
	return NULL;
}

static void classic_relink_roots(void)
{
}

static void classic_controller_added(possibly_unused int id)
{
}

static void classic_parts_released(possibly_unused int parts_no)
{
}

static void classic_remove_controller(struct page **erase_number_list, int index)
{
	if (index == -1)
		index = ctrl_stack.active;
	if (index < 0 || index >= ctrl_stack.nr_controllers)
		VM_ERROR("invalid controller index %d (nr_controllers=%d)",
		         index, ctrl_stack.nr_controllers);

	// Collect and release parts belonging to this controller; renumber
	// parts owned by controllers above it.
	ctrl_release_parts(erase_number_list, index);
	struct parts *p;
	PARTS_LIST_FOREACH(p) {
		if (p->controller_no > index
				&& p->controller_no < PARTS_CONTROLLER_SYSTEM_OVERLAY)
			p->controller_no--;
	}

	// ID == position: the IDs left are 0..nr_controllers-1 again.
	ctrl_stack.nr_controllers--;
	if (ctrl_stack.nr_controllers == 0) {
		PE_AddController(-1);
	} else if (ctrl_stack.active == index) {
		ctrl_stack.active = ctrl_stack.nr_controllers - 1;
	} else if (ctrl_stack.active > index
			&& ctrl_stack.active != PARTS_CONTROLLER_SYSTEM_OVERLAY) {
		ctrl_stack.active--;
	}
}

static void classic_set_active_controller(int controller_no)
{
	if (controller_no == PARTS_CONTROLLER_SYSTEM_OVERLAY ||
			(controller_no >= 0 && controller_no < ctrl_stack.nr_controllers))
		ctrl_stack.active = controller_no;
	else
		VM_ERROR("Invalid controller number: %d", controller_no);
}

static int classic_get_active_controller(void)
{
	return ctrl_stack.active;
}

const struct parts_widget_ops parts_widget_classic = {
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

const struct parts_transform_ops parts_transform_classic = {
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

const struct parts_controller_ops parts_controller_classic = {
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
