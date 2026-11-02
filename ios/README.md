# xsystem4 on iPad

xsystem4 runs on iPadOS 17 and later with the Metal renderer. The app has no game inside: the
player copies a game folder (the one holding the `.ain` file) into the app's folder with the
Files app or Finder file sharing, and the app starts a game from there, asking which if there are several. Saves are
kept beside them in `SaveData`, where the Files app and Finder show them (see below).

Touch: tap = click, two-finger tap or a tap outside the picture = right click, 1 s hold = Ctrl
(skip), two-finger swipe = wheel.

## Game and save files

The app's folder (its `Documents`) is "On My iPad > APP NAME" in the Files app, or the app
under the iPad's Files tab in Finder. For a game `MyGame`:

```
On My iPad/xsystem4/              the app's folder (Documents)
├── MyGame/                       the game folder, copied whole from the PC
│   ├── AliceStart.ini            or System40.ini
│   ├── MyGame.ain
│   ├── MyGameCG.afa
│   ├── MyGameSound.afa
│   ├── ...                       the other .afa, .ex and .fnl files
│   ├── locale/zh_CN.po           optional: a language for this game
│   └── Data/
└── SaveData/                     the saves (made at the first start)
    └── MyGame/                   one folder per game, shared by its languages
        ├── MyGame_User.sav
        └── ...
```

- The game folder may have any name. The `.ain` file can also sit directly in the app's
  folder. With several game folders or a `locale/*.po`, a list asks at each launch which to start.
- The save folder is named by the .ini's `SaveFolder`, `SaveData` by default. The game reads
  and writes it directly: copy saves in or out with the Files app or Finder while the app is
  not running. Deleting a file there deletes that save.
- Saves from xsystem4 on a computer are in `~/.xsystem4/GAME NAME/SaveData/` (or under
  `$XSYSTEM4_HOME`): copy the files in it into the app's `SaveData/GAME FOLDER`, and back.

## Build

Needs Xcode (with the iOS platform installed: Xcode > Settings > Components) and Homebrew's
`bison`, `flex` and `cmake` (libsys4's parser uses bison 3).

The libraries come from `ios/build_deps.sh PLATFORM...` (`device`, `sim-x86_64`, `sim-arm64`):
it downloads pinned, sha256-checked releases of SDL2, freetype, libffi, libjpeg-turbo, libpng,
libwebp, ogg, vorbis, FLAC, opus, libsndfile and cglm and builds them as static libraries into
`ios-deps/PLATFORM` (about 2½ minutes per platform; `XSYS4_IOS_DEPS` puts `ios-deps`
elsewhere). A library is rebuilt when the script changes; when nothing changed it returns at once.

`ios/xcframeworks.sh` packages them as XCFrameworks in `ios-deps/xcframeworks/`: per library
the device build and one Simulator build holding both architectures, with the headers
xsystem4 uses (a header that differs between the Simulator architectures is merged under
`#if defined(__x86_64__)`).

**Xcode:** open `ios/xsystem4.xcodeproj`, choose your team under Signing & Capabilities and
run. Targets: `xsystem4` (the app, linking the XCFrameworks), `sys4` (libsys4) and `ios-deps`,
which runs `ios/build_deps.sh device sim-x86_64 sim-arm64` and `ios/xcframeworks.sh`. Xcode
reads the XCFrameworks while it plans a build, so the scheme also runs that step as a
pre-action, before planning: the first build of a checkout spends about 8 minutes on
"Running pre-actions" (log: `ios-deps/prebuild.log`); later builds skip it. Any Simulator or
device can be chosen, including "Any iOS Simulator Device" (both architectures).
Debug builds keep the `XSYS4_*` test hooks, Release builds drop them.

**Without Xcode's IDE:**

```sh
ios/build_deps.sh device
ios/cross.sh device > ios-deps/cross-device.ini
meson setup --cross-file ios-deps/cross-device.ini -Ddebugger=disabled build-ios-device
ninja -C build-ios-device
ios/make_app.sh ios/xsystem4.json build-ios-device OUT    # OUT/xsystem4.app, ad-hoc signed
```

Zip `Payload/xsystem4.app` as an `.ipa` for a sideloading tool, or sign for a device with
`ios/provision.sh CONFIG TEAM_ID` and `ios/make_app.sh CONFIG BUILD OUT "Apple Development" PROFILE`.

## An app for one game

A JSON config names the app and adds its icon and files (see `ios/app.py` for every key):

```json
{
  "name": "MyGame",
  "display_name": "My Game",
  "bundle_id": "com.example.mygame",
  "assets": "Assets.xcassets",
  "resources": {"fonts/NotoSansCJKsc-Regular.otf": "fonts/NotoSansCJKsc-Regular.otf"},
  "fetch": {"fonts/NotoSansCJKsc-Regular.otf": {"url": "https://...", "sha256": "..."}}
}
```

Languages are not part of the app: each game folder holds its own `locale/<lang>.po`, and the
start list shows each one. `fonts/NotoSansCJKsc-Regular.otf`, if the app has it, draws translated
text. `fetch` downloads a file too big for git, and checks it, when the project or the app is
made. `assets` is an asset catalog whose `AppIcon` set holds the icon (one
1024×1024 PNG is enough).

```sh
python3 -I ios/app.py xcodeproj path/to/mygame.json   # writes MyGame.xcodeproj beside the config
ios/make_app.sh path/to/mygame.json build-ios-device OUT
```

A game's project takes its source lists from `ios/xsystem4.xcodeproj`, so it can stay out of
the game's repository and be generated after a checkout or an engine update (choose the team
again afterwards). When engine source files are added or removed, regenerate
`ios/xsystem4.xcodeproj` from a meson iOS build (`-Ddebugger=disabled`):
`python3 -I ios/app.py xcodeproj ios/xsystem4.json build-ios-device`.

## Notes

- Xcode's `actool` (asset catalogs) and scheme builds need the iOS platform and a Simulator
  runtime matching the SDK installed (Xcode > Settings > Components).
- Xcode compiles with `-ffile-prefix-map`, and `build_deps.sh` likewise, so `__FILE__` strings
  in the app hold no paths from the build machine.
