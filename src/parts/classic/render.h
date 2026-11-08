/* render.c: engines before v14 (and what ain v7 shares). Included at the end of render.c. See docs/game-profiles.md. */

/*
 * The transform around a parts' anchor:
 *
 *   parent's matrix * T(placed pos) * Rz(angle) * Rx * Ry * S(own scale) * own reverse
 *
 * (Rx and Ry: the parts' own X and Y rotations, parts_rotate_xy.)
 *
 * The original builds each level as S(reverse) * T(-origin) * S(scale) * R *
 * T(pos) (0x4e6d80, row vectors; 0x5c3ae0 builds S * Ry * Rx * Rz and
 * writes the translation last) on top of the parent's accumulated matrix,
 * so a parts' flip mirrors its box around its anchor before the scale,
 * rotation and translation, and everything above it (the ancestors' flips,
 * scales and rotations) moves the result with the parent: a gauge's fill
 * bar and number under a GaugeBase rotated 315 degrees lie along it, and a
 * MapNodeView name under its rotated MainText rectangle is rotated with it.
 * The parent's matrix is parts_params.matrix (parts_update_global_pos).
 * Without rotation or scale above the parts this is the transform used
 * before. angle is in radians; with rotate_scale false the parts' own
 * rotation and scale are left out (the FLAT root; RI2's TEXT path).
 *
 * That is v14. Before it the transform is the one those engines had:
 *
 *   T(global pos) * ancestors' reverse * Rz(angle) * S(global scale) * own reverse
 *
 * where the global position follows the ancestors' scales and flips but not
 * their rotation (parts_child_pos) and the scale is the product down the
 * tree. No earlier engine was read or compared for the matrix rule.
 */
void classic_anchor_transform(struct parts *parts, float angle, bool rotate_scale, mat4 out)
{
	glm_mat4_identity(out);
	glm_translate(out, (vec3) { parts->global.pos.x, parts->global.pos.y, 0 });
	parts_reverse_scale(out, parts->global.reverse_lr != parts->local.reverse_lr,
			parts->global.reverse_tb != parts->local.reverse_tb);
	if (rotate_scale) {
		glm_rotate_z(out, angle, out);
		// The X and Y angles down the tree, as v14 turns them
		// (parts_rotate_xy); the renderer then projects through the
		// original's camera.
		if (profile.parts.xy_rotation)
			parts_rotate_xy(out, &parts->global);
		glm_scale(out, (vec3){ parts->global.scale.x, parts->global.scale.y, 1.0 });
	}
	parts_reverse_scale(out, parts->local.reverse_lr, parts->local.reverse_tb);
}

/* v14 ClipArea is an integer, screen-aligned rectangle. Native 0x535a27
 * projects the origin, then adds local-scale offsets; 0x579ec0 intersects
 * the inherited rectangle. It does not rotate the rectangle's four corners.
 * Return 0 for no clip, -1 for an empty clip, 1 with screen-to-unit-clip. */
int classic_clip_area_transform(struct parts *parts, mat4 out)
{
	return 0;
}
