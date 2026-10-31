# xsystem4 on iPad

xsystem4 runs on iPadOS 17 and later with the Metal renderer. The app has no game inside: the
player copies a game folder (the one holding the `.ain` file) into the app's folder with the
Files app or Finder file sharing, and the app starts the first game it finds there. Save files
dropped into `Documents/SaveData` are imported at start-up.

Touch: tap = click, two-finger tap or a tap outside the picture = right click, 1 s hold = Ctrl
(skip), two-finger swipe = wheel.

## Build

Needs Xcode (with the iOS platform installed: Xcode > Settings > Components) and Homebrew's
`bison`, `flex` and `cmake` (libsys4's parser uses bison 3).

The libraries come from `ios/build_deps.sh PLATFORM...` (`device`, `sim-x86_64`, `sim-arm64`):
it downloads pinned, sha256-checked releases of SDL2, freetype, libffi, libjpeg-turbo, libpng,
libwebp, ogg, vorbis, FLAC, opus, libsndfile and cglm and builds them as static libraries into
`ios-deps/PLATFORM` (about 2½ minutes per platform; `XSYS4_IOS_DEPS` puts `ios-deps`
elsewhere). A library is rebuilt when the script changes; when nothing changed it returns at once.

**Xcode:** open `ios/xsystem4.xcodeproj`, choose your team under Signing & Capabilities and
run. Targets: `xsystem4` (the app), `sys4` (libsys4) and `ios-deps`, which runs
`ios/build_deps.sh` for the platform being built, so the first build also builds the
libraries. Debug builds keep the `XSYS4_*` test hooks, Release builds drop them.

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
  "icons": "Icons",
  "resources": {"locale/zh_CN.po": "../zh_CN.po"},
  "translations": {"zh": "locale/zh_CN.po"}
}
```

`translations` maps a language prefix to a `.po` file in the app: when the device's first
language starts with the prefix, the dialogue comes from that file (Info.plist key
`XSys4Translations`). `icons` is a folder with `AppIcon60x60@2x.png`,
`AppIcon76x76@2x~ipad.png` and `AppIcon83.5x83.5@2x~ipad.png`.

```sh
python3 -I ios/app.py xcodeproj path/to/mygame.json build-ios-device   # writes MyGame.xcodeproj beside the config
ios/make_app.sh path/to/mygame.json build-ios-device OUT
```

The Xcode project takes its source lists from a meson iOS build (`-Ddebugger=disabled`):
regenerate it, and `ios/xsystem4.xcodeproj` from `ios/xsystem4.json`, when source files are
added or removed.

## Notes

- The icon is given as PNG files rather than an asset catalog: Xcode's `actool` refuses to
  run unless a Simulator runtime matching the SDK is installed.
- Xcode compiles with `-ffile-prefix-map`, and `build_deps.sh` likewise, so `__FILE__` strings
  in the app hold no paths from the build machine.
