/* PartsEngine.c: ain v7 with activities (妻みぐい３). See docs/game-profiles.md. */

/* void SetCheckBoxColor(int Number, int Red, int Green, int Blue) */
static void PE_v7_SetCheckBoxColor(int number, int r, int g, int b)
{
	struct parts *parts = pe_v14_checkbox(number);
	if (!parts)
		return;
	struct parts_checkbox *cb = parts_get_checkbox(parts);
	if (cb->show_color && cb->r == r && cb->g == g && cb->b == b)
		return;
	cb->r = r;
	cb->g = g;
	cb->b = b;
	cb->show_color = true;
	parts_checkbox_rebuild(parts);
	parts_dirty(parts);
}

// ain v7: PlayPartsMovie(Number, State), from the start.
static bool PE_v7_PlayPartsMovie(int number, int state)
{
	return PE_stub_PlayPartsMovie(number, 0, state);
}

/* The keyboard focus of a text box: this engine has no text box to give it. */
static void PE_v7_SetFocus(possibly_unused int parts_no)
{
}

/* Swipes are touch input, which this engine does not send: none to stop. */
static void PE_v7_StopSwipe(void)
{
}

static void PE_v7_SetWantSaveBackScene(int parts_no, bool enable)
{
	PE_SetWantSaveBackScene(parts_no, enable);
}

static bool PE_v7_IsWantSaveBackScene(int parts_no)
{
	return PE_IsWantSaveBackScene(parts_no);
}

// The greatest global Z of a parts and its subtree.
static int parts_max_global_z(struct parts *parts)
{
	int z = parts->global.z;
	struct parts *child;
	TAILQ_FOREACH(child, &parts->children, child_list_entry) {
		int cz = parts_max_global_z(child);
		if (cz > z)
			z = cz;
	}
	return z;
}

static int PE_v7_GetComponentAbsoluteMaxPosZ(int parts_no)
{
	struct parts *parts = parts_try_get(parts_no);
	return parts ? parts_max_global_z(parts) : 0;
}

// No active parts: the input of the engines before v14 keeps none.
static int PE_v7_GetActiveParts(void)
{
	return 0;
}

// The transforms are combined at each update already.
static void PE_v7_UpdateMatrix(possibly_unused bool message_window_show)
{
}

static bool PE_v7_AddPartsConstructionProcess(struct page **ints_page, struct page **floats_page,
		struct page **strings_page)
{
	struct page *ip = *ints_page, *fp = *floats_page, *sp = *strings_page;
	if (!ip || !fp || !sp || ip->nr_vars < CP_NR_INTS || fp->nr_vars < 2 || sp->nr_vars < 2) {
		WARNING("AddPartsConstructionProcess: unexpected array sizes");
		return false;
	}
	// 33 ints: 母爛漫's successors (は～とふるママン) pack GetA2 after GetB2.
	const bool has_a2 = ip->nr_vars > CP_NR_INTS;
#define I(n) (ip->values[CP_##n + (has_a2 && CP_##n >= CP_CHAR_SPACE)].i)
#define A2 (has_a2 ? ip->values[CP_CHAR_SPACE].i : 255)
	int no = I(NUMBER), state = I(STATE);
	float bold = fp->values[0].f, edge = fp->values[1].f;
	struct string *text = heap_get_string(sp->values[0].i);
	struct string *cg = heap_get_string(sp->values[1].i);
#define TEXT_ARGS no, I(DX), I(DY), text, I(FONT_TYPE), I(FONT_SIZE), I(TEXT_R), I(TEXT_G), I(TEXT_B), \
		bold, I(EDGE_R), I(EDGE_G), I(EDGE_B), edge, I(CHAR_SPACE), I(LINE_SPACE), state
	switch (I(COMMAND)) {
	case 0: return PE_AddCreateToPartsConstructionProcess(no, I(DW), I(DH), state);
	case 1: return PE_AddCreatePixelOnlyToPartsConstructionProcess(no, I(DW), I(DH), state);
	case 2: return PE_AddCreateCGToProcess(no, cg, state);
	case 3: return PE_AddFillToPartsConstructionProcess(no, I(DX), I(DY), I(DW), I(DH),
			I(R), I(G), I(B), state);
	case 4: return PE_AddFillAlphaColorToPartsConstructionProcess(no, I(DX), I(DY), I(DW), I(DH),
			I(R), I(G), I(B), I(A), state);
	case 5: return PE_AddFillAMapToPartsConstructionProcess(no, I(DX), I(DY), I(DW), I(DH), I(A), state);
	case 6: return PE_AddFillWithAlphaToPartsConstructionProcess(no, I(DX), I(DY), I(DW), I(DH),
			I(R), I(G), I(B), I(A), state);
	case 7: return PE_AddDrawTextToPartsConstructionProcess(TEXT_ARGS);
	case 8: return PE_AddCopyTextToPartsConstructionProcess(TEXT_ARGS);
	case 10: return PE_AddDrawRectToPartsConstructionProcess(no, I(DX), I(DY), I(DW), I(DH),
			I(R), I(G), I(B), state);
	case 11: return PE_AddDrawCutCGToPartsConstructionProcess(no, cg, I(DX), I(DY), I(DW), I(DH),
			I(SX), I(SY), I(DW), I(DH), I(INTERP), state);
	case 12: return PE_AddCopyCutCGToPartsConstructionProcess(no, cg, I(DX), I(DY), I(DW), I(DH),
			I(SX), I(SY), I(DW), I(DH), I(INTERP), state);
	case 13: return PE_AddDrawCutCGToPartsConstructionProcess(no, cg, I(DX), I(DY), I(DW), I(DH),
			I(SX), I(SY), I(SW), I(SH), I(INTERP), state);
	case 14: return PE_AddCopyCutCGToPartsConstructionProcess(no, cg, I(DX), I(DY), I(DW), I(DH),
			I(SX), I(SY), I(SW), I(SH), I(INTERP), state);
	case 15: return PE_AddGrayFilterToPartsConstructionProcess(no, I(DX), I(DY), I(DW), I(DH),
			I(FULL_SIZE), state);
	case 16: return PE_AddAddFilterToPartsConstructionProcess(no, I(DX), I(DY), I(DW), I(DH),
			I(R), I(G), I(B), I(FULL_SIZE), state);
	case 17: return PE_AddMulFilterToPartsConstructionProcess(no, I(DX), I(DY), I(DW), I(DH),
			I(R), I(G), I(B), I(FULL_SIZE), state);
	case 18: return PE_AddDrawLineToPartsConstructionProcess(no, I(DX), I(DY), I(DX2), I(DY2),
			I(R), I(G), I(B), I(A), state);
	case 23: return PE_AddAlphaBlendTextToPartsConstructionProcess(TEXT_ARGS);
	// MulAMapGradationHorizon: the alpha A at the top to A2 at the bottom;
	// Vertical: A at the left to A2 at the right.
	case 25: return PE_AddMulAMapGradationToPartsConstructionProcess(no, false,
			I(DX), I(DY), I(DW), I(DH), false, I(A), A2, state);
	case 26: return PE_AddMulAMapGradationToPartsConstructionProcess(no, true,
			I(DX), I(DY), I(DW), I(DH), false, I(A), A2, state);
	case 24: return PE_AddOnlyAlphaTextToPartsConstructionProcess(TEXT_ARGS);
	default:
		// 9 gradation, 19-22 the alpha CG cuts: no script path seen.
		WARNING("AddPartsConstructionProcess: command %d not implemented", I(COMMAND));
		return false;
	}
#undef TEXT_ARGS
#undef A2
#undef I
}

static void pe_v7_register(int libno)
{
	struct static_library *lib = &lib_PartsEngine;
	static_library_register(lib, "Parts_GetParentPartsNumber", PE_v14_GetParentPartsNumber);
	static_library_register(lib, "GetComponentAbsoluteMaxPosZ", PE_v7_GetComponentAbsoluteMaxPosZ);
	static_library_register(lib, "GetActiveParts", PE_v7_GetActiveParts);
	static_library_register(lib, "UpdateMatrix", PE_v7_UpdateMatrix);
	static_library_register(lib, "SaveThumbnail", PE_SaveThumbnail);
	static_library_register(lib, "Parts_GetPartsSize", PE_GetPartsSize);
	static_library_register(lib, "SetButtonEnable", PE_SetButtonEnable);
	static_library_register(lib, "IsButtonEnable", PE_IsButtonEnable);
	static_library_register(lib, "SetButtonCGName", PE_SetButtonCGName);
	struct ain_hll_function *cp = get_fun(libno, "AddPartsConstructionProcess");
	if (cp && cp->nr_arguments == 3 && cp->arguments[0].type.data == AIN_REF_ARRAY_INT)
		static_library_register(lib, "AddPartsConstructionProcess", PE_v7_AddPartsConstructionProcess);
	static_library_register(lib, "SetWantSaveBackScene", PE_v7_SetWantSaveBackScene);
	static_library_register(lib, "IsWantSaveBackScene", PE_v7_IsWantSaveBackScene);
	struct ain_hll_function *fun = get_fun(libno, "SaveBackScene");
	if (fun && fun->nr_arguments == 1 && fun->arguments[0].type.data == AIN_REF_ARRAY_INT) {
		static_library_register(lib, "SaveBackScene", PE_SaveBackScene);
		static_library_register(lib, "LoadBackScene", PE_LoadBackScene);
		static_library_register(lib, "ClearBackScene", PE_ClearBackScene);
		static_library_register(lib, "SetFontColorForBackScene", PE_SetFontColorForBackScene);
		static_library_register(lib, "SetMulColorForBackScene", PE_SetMulColorForBackScene);
		static_library_register(lib, "SetAlphaForBackScene", PE_SetAlphaForBackScene);
		static_library_register(lib, "SetShowForBackScene", PE_SetShowForBackScene);
	}
	fun = get_fun(libno, "PlayPartsMovie");
	if (fun && fun->nr_arguments == 2) {
		static_library_register(lib, "CreatePartsMovie", PE_stub_CreatePartsMovie);
		static_library_register(lib, "ReleasePartsMovie", PE_stub_ReleasePartsMovie);
		static_library_register(lib, "PlayPartsMovie", PE_v7_PlayPartsMovie);
		static_library_register(lib, "IsEndPartsMovie", PE_stub_IsEndPartsMovie);
	}
	fun = get_fun(libno, "Parts_SetSoundName");
	if (fun && fun->nr_arguments == 3 && fun->arguments[0].type.data == AIN_INT
	    && fun->arguments[1].type.data == AIN_STRING && fun->arguments[2].type.data == AIN_INT)
		static_library_register(lib, "Parts_SetSoundName", PE_SetSoundName);
	if (get_fun(libno, "SetCheckBoxColor"))
		static_library_register(lib, "SetCheckBoxColor", PE_v7_SetCheckBoxColor);
	pe_v14_register_checkbox(libno);
	pe_v14_register_backlog();
	static_library_register(lib, "PauseMotion", PE_PauseMotion);
	static_library_register(lib, "SetFocus", PE_v7_SetFocus);
	static_library_register(lib, "Parts_StopSwipe", PE_v7_StopSwipe);
}
