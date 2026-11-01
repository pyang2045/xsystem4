# Versions and games

The engine runs games from ain v1 to v14. Their APIs differ, and a change for one game must not
reach another. Four rules keep that true.

## 1. A version decides which function runs, not what a function does

Three mechanisms, in the order to try them:

**A different declaration: bind another function.** `link_static_library` binds an AIN declaration to
a C function by name. When a game declares a function differently, or it behaves differently as a
whole, register another function in the library's `_PreLink` or `_PostLink`
(`static_library_replace`, `static_library_register`), or give the library another name
(`MainEXFile_v7` in `resolve_library_name`). The shape selectors in `src/hll/hll_shape_select.c`
choose by declaration. PartsEngine's generations have their own `prelink`, `postlink` and `select` in
the `bindings` of their row (`src/hll/PartsEngine.c`).

Order does not decide a binding. `static_library_fill` binds a placeholder, which stays only if nothing
else binds that declaration; `static_library_register` binds a real function, takes a fill's place, and
warns if another real function is bound already (`bound already`). Fix that warning: one of the two is
dead code.

**A whole function differs and other engine code calls it too: an ops table.** Write one function per
generation (`v14_begin_input`, `v7_begin_input`, `classic_begin_input`) in that generation's folder,
`src/parts/v14/`, `src/parts/v7/` or `src/parts/classic/`, and list it in the generation's table there. The
folders are included at the end of `parts.c`, `input.c`, `render.c` and `layoutbox.c`, so they see those
files' statics. A change for 妻みぐい３ should then show in the diff as `src/parts/v7/`, `src/hll/pe_v7.h` or
its row, and nothing else; anything more reaches another game. The `PE_` function only calls `profile.parts.input->begin_input()`.
Each generation has its own table even where its entries are another's functions, so a change for one
generation replaces an entry in its table and leaves the shared function alone.

**A few lines differ inside shared logic: a named behaviour.** Add a field to `include/profile.h`,
turn it on in the rows of `src/profile.c` that have it, and test `profile.parts.<name>`.

## 2. Only `src/profile.c` looks at the version

`src/profile.c` is a table with a row per engine generation: how to recognise it, then its PartsEngine,
HLL and language behaviours. `profile_init` only picks the first row that matches and copies it, and
sets nothing by version itself. A field left out of a row is false, and a new field is added to the rows
that want it, so a change for 妻みぐい３ cannot reach Haharanman or an older game. A game that fits no row
gets a new row; one that declares activities or v14 messages on the last row gets a warning at startup.

Name a field for what it does (`controller_ids`), not when it applies (`is_v14`).

`tools/check-version-gates.sh` (`meson test`) fails on a version read outside `src/profile.c` and the
files of rule 3, when the number of checks in those files changes, and on comparing a game name outside
`src/hacks.c`.

A row also has the version of its parts saves (`save_version`). A format change for one generation moves
that row alone, to a number above every row's, so another game's saves stay readable.

## 3. The VM follows the bytecode

`src/vm.c`, `src/page.c`, `src/heap.c`, `src/savedata.c`, `src/resume.c` and the calling convention in
`src/ffi.c` keep their version checks: those are the format of the ain, the heap and the save, which do
follow the version. They are not API behaviour; don't add API behaviour there.

## 4. A game's oddity is a row in `src/hacks.c`

`game_hacks` maps a game name to the function that patches its ain or sets its `profile.quirks`.
Shared code tests the quirk, never the name.

## Checking a change

- `XSYS4_LINK_REPORT=<file> xsystem4 <game>`, then `tools/link-report.py <binary> <file>`: the game's row
  with its behaviours as bytes (`#profile`), and what every declaration is bound to. Diff the report before and after a change, for each game; a
  change for one game should not move another's.
- `xsystem4 test/Run/test.ain`: the classic row (`All tests passed`).
- The scripted runs of each game (see the port's `CLAUDE.md`).
