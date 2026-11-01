/* input.c: ain v7 with activities (妻みぐい３). Included at the end of input.c. See docs/game-profiles.md. */

static void v7_reset_input(void)
{
	parts_set_input_depth(0);
}

// The scopes nest, as on v14.
static void v7_begin_input(void)
{
	input_depth++;
	parts_began_click = true;
}

// The end of a child (the CG viewer, a scene of 回想) leaves the list that
// opened it taking input.
static void v7_end_input(void)
{
	if (input_depth)
		input_depth--;
	parts_began_click = input_depth > 0;
	clicked_parts = 0;
	drag_state_reset();
}

const struct parts_input_ops parts_input_v7 = {
	.begin_input = v7_begin_input,
	.end_input = v7_end_input,
	.reset_input = v7_reset_input,
	.update_input_state = classic_update_input_state,
	.set_show_link = classic_set_show_link,
};
