/* layoutbox.c: v14. Included at the end of layoutbox.c. See docs/game-profiles.md. */

static bool lb_v14_lays_out(struct parts *parts)
{
	if (!lb_v14_is_box(parts))
		return false;
	enum parts_layout_type type = parts->states[0].layout_box.layout_type;
	return type == PARTS_LAYOUT_VERTICAL || type == PARTS_LAYOUT_HORIZONTAL;
}

static int lb_v14_align(struct parts_layout_box *lb)
{
	int a = lb->align;
	if (lb->layout_type == PARTS_LAYOUT_VERTICAL) {
		if (a == 4 || a == 5 || a == 6 || (lb->wrap && (a == 2 || a == 8)))
			return 1;
	} else if (lb->layout_type == PARTS_LAYOUT_HORIZONTAL) {
		if (a == 2 || a == 5 || a == 8 || (lb->wrap && (a == 4 || a == 6)))
			return 1;
	}
	return a;
}

void v14_layout_size_changed(struct parts *parts)
{
	for (struct parts *p = parts, *a = parts->parent; a; p = a, a = a->parent) {
		if (lb_v14_lays_out(a)) {
			parts_component_dirty(a);
			return;
		}
		if (lb_v14_is_box(a)) {
			if (a->origin_mode != 1)
				parts_component_dirty(a);
			continue;
		}
		if (a->component_type == 17 && TAILQ_FIRST(&a->children) == p)
			continue;
		return;
	}
}

/*
 * 0x4e57d0, the per-frame update of the user component widget (vtable
 * 0x80f0b4 +0x14): the first child (0x4e5780), the content's root, takes the
 * component's origin mode (0x4e5815). A free root then moves the content by
 * it, a vertical/horizontal one starts from it, any other parts draws its
 * own box by it. The widget does nothing else to its children. Here the
 * component is marked when the mode or the child changes (parts.c).
 */
static void lb_v14_user_component(struct parts *uc)
{
	struct parts *first = TAILQ_FIRST(&uc->children);
	if (!first || first->origin_mode == uc->origin_mode)
		return;
	first->origin_mode = uc->origin_mode;
	parts_recalculate_hitbox(first);
	parts_dirty(first);
}

/*
 * 0x5499e0: a free box gives each of its children, shown or not, an offset
 * by its own origin mode and size: 0, -W/2 or -W and 0, -H/2 or -H (jump
 * tables 0x549ba4 and 0x549bbc; 0 for a mode outside 1..9), written to the
 * child's +0x238/+0x23c (0x549b28, 0x549b37) and added to its position where
 * the child is placed (parts_placed_pos). The size is GetSize(1) (0x54a9f0),
 * 0x0 unless both are positive; it does not read the offset (0x54aaf0), so
 * one pass settles. The original's only other writer of the offset is the
 * フォーム widget (type 15, 0x4da23f), which the game's pactex does not have,
 * and nothing clears it when a child leaves the box or gets another parent:
 * the child keeps its last one. Origin mode 1, every free box's own in the
 * pactex, needs no size.
 */
static void lb_v14_free_layout(struct parts *box)
{
	Point offset = { 0, 0 };
	if (box->origin_mode != 1) {
		int w, h;
		lb_v14_box_size(box, &w, &h);
		offset.x = align_offset_x(box->origin_mode, w);
		offset.y = align_offset_y(box->origin_mode, h);
	}
	struct parts *child;
	PARTS_FOREACH_CHILD(child, box) {
		if (child->layout_offset.x == offset.x && child->layout_offset.y == offset.y)
			continue;
		child->layout_offset = offset;
		parts_dirty(child);
	}
}

// 0x549bd0 (vertical) / 0x54a200 (horizontal)
static void lb_v14_flow_layout(struct parts *box)
{
	struct parts_layout_box *lb = &box->states[0].layout_box;
	int total_w, total_h;
	lb_v14_box_size(box, &total_w, &total_h);
	if (lb->common.w != total_w || lb->common.h != total_h)
		parts_set_dims(box, &lb->common, total_w, total_h);

	int align = lb_v14_align(lb);
	bool is_left   = (align == 1 || align == 4 || align == 7);
	bool is_right  = (align == 3 || align == 6 || align == 9);
	bool is_top    = (align == 1 || align == 2 || align == 3);
	bool is_bottom = (align == 7 || align == 8 || align == 9);
	int start_x = (is_left ? lb->padding_left : is_right ? -lb->padding_right : 0)
		+ align_offset_x(box->origin_mode, total_w) - align_offset_x(align, total_w);
	int start_y = (is_top ? lb->padding_top : is_bottom ? -lb->padding_bottom : 0)
		+ align_offset_y(box->origin_mode, total_h) - align_offset_y(align, total_h);
	int x_dir = is_right ? -1 : 1;
	int y_dir = is_bottom ? -1 : 1;
	bool horizontal = lb->layout_type == PARTS_LAYOUT_HORIZONTAL;

	int cur_x = start_x;
	int cur_y = start_y;
	int max_cross = 0;
	struct parts *child;
	PARTS_FOREACH_CHILD(child, box) {
		bool counted = lb_v14_counted(child);
		if (child->origin_mode != align) {
			child->origin_mode = align;
			parts_recalculate_hitbox(child);
		}
		int cw, ch;
		lb_v14_child_size(child, &cw, &ch);
		cw += child->margin_left + child->margin_right;
		ch += child->margin_top + child->margin_bottom;

		if (counted && lb->wrap) {
			if (horizontal && abs(cur_x + cw * x_dir - start_x) > lb->wrap_size) {
				cur_y += max_cross * y_dir;
				cur_x = start_x;
				max_cross = 0;
			} else if (!horizontal && abs(cur_y + ch * y_dir - start_y) > lb->wrap_size) {
				cur_x += max_cross * x_dir;
				cur_y = start_y;
				max_cross = 0;
			}
		}

		int pos_x = cur_x + (is_left ? child->margin_left : is_right ? -child->margin_right : 0);
		int pos_y = cur_y + (is_top ? child->margin_top : is_bottom ? -child->margin_bottom : 0);
		if (child->local.pos.x != pos_x || child->local.pos.y != pos_y) {
			child->local.pos.x = pos_x;
			child->local.pos.y = pos_y;
			parts_recalculate_hitbox(child);
			parts_component_dirty(child);
		}

		if (!counted)
			continue;
		if (horizontal) {
			cur_x += cw * x_dir;
			if (ch > max_cross)
				max_cross = ch;
		} else {
			cur_y += ch * y_dir;
			if (cw > max_cross)
				max_cross = cw;
		}
	}
}

// What a v14 widget does to its children when it is updated (vtable +0x14):
// 0x4e57d0 for a user component, 0x5498e0 -> 0x549960 for a box. A parts made
// a user component while its state is still a box (SetComponentType 17 keeps
// the states; the original replaces the whole widget, 0x5372c0) is a user
// component here, while lb_v14_child_size, parts_layout_size_changed and the
// hit test still see the box. The game's pactex has no such component.
void v14_do_layout(struct parts *parts)
{
	if (parts->component_type == 17)
		lb_v14_user_component(parts);
	else if (lb_v14_lays_out(parts))
		lb_v14_flow_layout(parts);
	else if (lb_v14_is_box(parts) && parts->states[0].layout_box.layout_type == PARTS_LAYOUT_FREE)
		lb_v14_free_layout(parts);
}
