/* layoutbox.c: engines before v14 (and what ain v7 shares). Included at the end of layoutbox.c. See docs/game-profiles.md. */

static void parts_get_layout_size(struct parts *parts, int *w, int *h)
{
	struct parts_state *state = &parts->states[parts->state];
	if (profile.parts.box_origin_layout && parts->states[0].type == PARTS_LAYOUT_BOX) {
		lb_v14_box_size(parts, w, h);
		return;
	}
	if (state->type == PARTS_FLAT && state->flat.flat) {
		// The layout size of Flat is specified in the header, and this may
		// differ from the hitbox size.
		*w = state->flat.flat->hdr.width;
		*h = state->flat.flat->hdr.height;
	} else {
		*w = state->common.w;
		*h = state->common.h;
	}
}

// Calculates the total size of a horizontal layout box by summing child widths per row.
static void parts_layout_calc_size_horizontal(struct parts_layout_box *lb, struct parts *parts,
		int *out_w, int *out_h)
{
	int row_w = 0, max_row_w = 0, max_row_h = 0, total_h = 0;
	struct parts *child;
	PARTS_FOREACH_CHILD(child, parts) {
		int child_w, child_h;
		parts_get_layout_size(child, &child_w, &child_h);
		int cw = child->margin_left + child_w + child->margin_right;
		int ch = child->margin_top + child_h + child->margin_bottom;
		if (lb->wrap && row_w + cw > lb->wrap_size && row_w > 0) {
			if (row_w > max_row_w)
				max_row_w = row_w;
			total_h += max_row_h;
			row_w = 0;
			max_row_h = 0;
		}
		row_w += cw;
		if (ch > max_row_h)
			max_row_h = ch;
	}
	if (row_w > max_row_w)
		max_row_w = row_w;
	total_h += max_row_h;
	*out_w = max_row_w + lb->padding_left + lb->padding_right;
	*out_h = total_h + lb->padding_top + lb->padding_bottom;
}

// Calculates the total size of a vertical layout box by summing child heights per column.
static void parts_layout_calc_size_vertical(struct parts_layout_box *lb, struct parts *parts,
		int *out_w, int *out_h)
{
	int col_h = 0, max_col_h = 0, max_col_w = 0, total_w = 0;
	struct parts *child;
	PARTS_FOREACH_CHILD(child, parts) {
		int child_w, child_h;
		parts_get_layout_size(child, &child_w, &child_h);
		int cw = child->margin_left + child_w + child->margin_right;
		int ch = child->margin_top + child_h + child->margin_bottom;
		if (lb->wrap && col_h + ch > lb->wrap_size && col_h > 0) {
			if (col_h > max_col_h)
				max_col_h = col_h;
			total_w += max_col_w;
			col_h = 0;
			max_col_w = 0;
		}
		col_h += ch;
		if (cw > max_col_w)
			max_col_w = cw;
	}
	if (col_h > max_col_h)
		max_col_h = col_h;
	total_w += max_col_w;
	*out_w = total_w + lb->padding_left + lb->padding_right;
	*out_h = max_col_h + lb->padding_top + lb->padding_bottom;
}

/*
 * v14: something that can change the size a box gives `parts` (its normal
 * state's size, its shown flags, its margins, its parent) changed: the
 * nearest vertical/horizontal box above lays out again. A free box or a user
 * component in between passes it on, as their size follows their children.
 * A free box with an origin mode other than 1 lays out again as well: its
 * children's offset follows its size, which also reads their positions and
 * origin modes.
 * A box whose own size changes this way reports it from parts_do_layout.
 */
void classic_layout_size_changed(struct parts *parts)
{
	// profile.parts.box_origin_layout: the boxes above, up to the first parts
	// that is not a box, size their children again.
	if (!profile.parts.box_origin_layout)
		return;
	for (struct parts *a = parts->parent; a && a->states[0].type == PARTS_LAYOUT_BOX; a = a->parent)
		parts_component_dirty(a);
}

static void lb_v14_free_layout(struct parts *box);

void classic_do_layout(struct parts *parts)
{
	if (parts->states[0].type != PARTS_LAYOUT_BOX)
		return;
	struct parts_layout_box *lb = &parts->states[0].layout_box;
	if (lb->layout_type == PARTS_LAYOUT_FREE) {
		if (profile.parts.box_origin_layout)
			lb_v14_free_layout(parts);
		return;
	}

	int total_w, total_h;
	if (lb->layout_type == PARTS_LAYOUT_HORIZONTAL)
		parts_layout_calc_size_horizontal(lb, parts, &total_w, &total_h);
	else
		parts_layout_calc_size_vertical(lb, parts, &total_w, &total_h);

	int align = lb->align;
	bool is_left   = (align == 1 || align == 4 || align == 7);
	bool is_right  = (align == 3 || align == 6 || align == 9);
	bool is_top    = (align == 1 || align == 2 || align == 3);
	bool is_bottom = (align == 7 || align == 8 || align == 9);

	int origin_x = align_offset_x(parts->origin_mode, total_w);
	int origin_y = align_offset_y(parts->origin_mode, total_h);
	int align_x = align_offset_x(align, total_w);
	int align_y = align_offset_y(align, total_h);

	int start_x = (is_left ? lb->padding_left : -lb->padding_right)
		+ (origin_x - align_x);
	int start_y = (is_top ? lb->padding_top : -lb->padding_bottom)
		+ (origin_y - align_y);

	int x_dir = is_right ? -1 : 1;
	int y_dir = is_bottom ? -1 : 1;

	int cur_x = start_x;
	int cur_y = start_y;
	int max_cross = 0;

	struct parts *child;
	PARTS_FOREACH_CHILD(child, parts) {
		int child_w, child_h;
		parts_get_layout_size(child, &child_w, &child_h);
		int cw = child->margin_left + child_w + child->margin_right;
		int ch = child->margin_top + child_h + child->margin_bottom;

		// The layout box forces the child's origin_mode to match its own align.
		if (child->origin_mode != align) {
			child->origin_mode = align;
			parts_recalculate_hitbox(child);
		}

		if (lb->layout_type == PARTS_LAYOUT_HORIZONTAL) {
			if (lb->wrap && abs((cur_x + cw * x_dir) - start_x) > lb->wrap_size
					&& max_cross > 0) {
				cur_y += max_cross * y_dir;
				max_cross = 0;
				cur_x = start_x;
			}
		} else {
			if (lb->wrap && abs((cur_y + ch * y_dir) - start_y) > lb->wrap_size
					&& max_cross > 0) {
				cur_x += max_cross * x_dir;
				max_cross = 0;
				cur_y = start_y;
			}
		}

		// margin offset depends on alignment direction
		int mx = is_left ? child->margin_left : is_right ? -child->margin_right : 0;
		int my = is_top ? child->margin_top : is_bottom ? -child->margin_bottom : 0;

		int pos_x = cur_x + mx;
		int pos_y = cur_y + my;
		if (child->local.pos.x != pos_x || child->local.pos.y != pos_y) {
			child->local.pos.x = pos_x;
			child->local.pos.y = pos_y;
			parts_recalculate_hitbox(child);
			parts_component_dirty(child);
		}

		if (lb->layout_type == PARTS_LAYOUT_HORIZONTAL) {
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
