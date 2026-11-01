/* render.c: v14. Included at the end of render.c. See docs/game-profiles.md. */

void v14_anchor_transform(struct parts *parts, float angle, bool rotate_scale, mat4 out)
{
	parts_parent_transform(parts, out);
	Point pos = parts_placed_pos(parts);
	glm_translate(out, (vec3) { pos.x, pos.y, 0 });
	if (rotate_scale) {
		if (angle != 0.0f)
			glm_rotate_z(out, angle, out);
		// The X and Y rotations follow the Z one (parts_rotate_xy); the
		// depth they give is projected when drawn (parts_render_texture).
		parts_rotate_xy(out, &parts->local);
		glm_scale(out, (vec3){ parts->local.scale.x, parts->local.scale.y, 1.0 });
	}
	parts_reverse_scale(out, parts->local.reverse_lr, parts->local.reverse_tb);
}

int v14_clip_area_transform(struct parts *parts, mat4 out)
{
	bool enabled = false;
	double left = 0, top = 0, right = 0, bottom = 0;
	for (struct parts *p = parts; p; p = p->parent) {
		if (!p->clip_enabled)
			continue;
		mat4 box;
		parts_anchor_transform(p, glm_rad(p->local.rotation.z), true, box);
		float x = box[3][0] + p->clip_area.x * p->local.scale.x;
		float y = box[3][1] + p->clip_area.y * p->local.scale.y;
		float w = p->clip_area.w * p->local.scale.x;
		float h = p->clip_area.h * p->local.scale.y;
		// Avoid undefined float-to-int conversions and singular matrices.
		if (!isfinite(x) || !isfinite(y) || !isfinite(w) || !isfinite(h)
				|| x < INT_MIN || (double)x > INT_MAX
				|| y < INT_MIN || (double)y > INT_MAX
				|| w < 1 || (double)w > INT_MAX || h < 1 || (double)h > INT_MAX)
			return -1;
		double l = (int)x, t = (int)y, r = l + (int)w, b = t + (int)h;
		if (!enabled) {
			left = l; top = t; right = r; bottom = b;
			enabled = true;
		} else {
			left = fmax(left, l); top = fmax(top, t);
			right = fmin(right, r); bottom = fmin(bottom, b);
		}
		if (right <= left || bottom <= top)
			return -1;
	}
	if (!enabled)
		return 0;
	glm_mat4_identity(out);
	out[0][0] = 1.0 / (right - left);
	out[1][1] = 1.0 / (bottom - top);
	out[3][0] = -left / (right - left);
	out[3][1] = -top / (bottom - top);
	return 1;
}
