xsystem4
========

xsystem4 is an implementation of AliceSoft's System 4 game engine for unix-like
operating systems.

This fork
---------

This fork (branch `haharanman-on-usada`) runs AliceSoft's **母爛漫 (Haharanman, 2019, ain
v14)**, Japanese release, on macOS and iPadOS, and **妻みぐい３ (Tsumamigui 3, ain v7)**,
Traditional Chinese release, on macOS. It builds on
[USADA-KARROT's fork](https://github.com/USADA-KARROT/xsystem4), which adds the ain v14 engine
of Dohna Dohna (the `X_*` opcodes, the v14 HLL libraries, PartsEngine activities, the
KiwiSoundEngine sound groups) and the Chinese GBK releases. Additions in this fork:

* **Haharanman, title to ending.** The v14 code paths now also work with the Japanese (SJIS)
  release: pactex key spellings in one table for both encodings, config sliders and check
  boxes, the EX table's sound groups. Also: v14 construction commands and filters, v15 flats
  and flat origin modes, TALT CG atlases, ruby readings in message windows, ト書き windows.
* **妻みぐい３ (ain v7).** The title, new game and the map, save, load and quick save, the
  settings, the backlog with voice replay, the back scene, the CG, 回想 and music modes, sound
  groups and the ending movie. Each was tested on its own; the game has not been played through
  from start to end.
* **Game text in UTF-8.** A game whose `.ain` strings are UTF-8 is detected at startup and runs
  on the same charset switch as SJIS and GBK: string functions count characters, and the engine's
  own names (pactex keys, parts and mixer names) are looked up in the game's encoding.
* **Versions kept apart.** Games from ain v1 to v14 have different APIs, and a change for one
  must not reach another. `src/profile.c` has one row per generation (`classic`, 妻みぐい３'s
  `activities_v7`, v14) holding its bindings, tables of functions and behaviours; each
  generation's parts code is in `src/parts/{classic,v7,v14}/`. `meson test` rejects version checks
  in API code, and `tools/link-report.py` shows what each of a game's declarations is bound to,
  to compare before and after a change. See [docs/game-profiles.md](docs/game-profiles.md).
* **Saving and loading in v14 games.** Generic arrays, scenes carried across a save (their
  activities, user components and button state), the backlog, appended EX files, quick save,
  and resuming in the right overload when a save was made mid-event.
* **Translations without changing the game's files.** `--locale FILE.po` replaces message
  text from a gettext `.po` file; `--dump-pot FILE` writes the template. A translated line's
  ideographs come from one font; `--locale-font` names a TrueType font for characters the
  game font lacks.
* **Metal renderer** (`-Dmetal=enabled`, the default on macOS): the 2D OpenGL subset
  xsystem4 uses, on Metal, without ANGLE.
* **iPad app** (iPadOS 17 and later): an Xcode project or a meson cross build, libraries from
  pinned sources, a JSON config for a game's own app (name, icon, translation). See
  [ios/README.md](ios/README.md).
* **APEG movies without FFmpeg** (pl_mpeg backend), as used for the Haharanman ending.
* `XSYS4_*` test and trace environment variables behind `-Ddev_hooks` (on by default; off for
  release builds), including which HLL functions a run called (`XSYS4_HLL_CALLED`) and which of
  them are placeholders (`XSYS4_STUB_LOG`).

Compatiblity
------------

See the [game compatibility table](game_compatibility.md) for a list of games
that can be played on xsystem4.

Building
--------

First install the dependencies (corresponding Debian package in parentheses):

* chibi-scheme [optional, for debugger]
* bison (bison)
* cglm (libcglm-dev) [optional, fetched by meson if not available]
* flex (flex)
* freetype (libfreetype-dev)
* glew (libglew-dev)
* meson (meson)
* libffi (libffi-dev)
* libpng (libpng-dev)
* libsndfile (libsndfile-dev)
* libturbojpeg (libturbojpeg0-dev)
* libwebp (libwebp-dev)
* SDL2 (libsdl2-dev)
* zlib (zlib1g-dev)

Then fetch the git submodules,

    git submodule init
    git submodule update

(Alternatively, pass `--recurse-submodules` when cloning this repository)

Then build the xsystem4 executable with meson,

    mkdir build
    meson build
    ninja -C build
    
Finally install it to your system (optional),

    ninja -C build install

### iPad

xsystem4 builds as an iPadOS app (Xcode project or meson cross build); the player copies a
game into the app with the Files app. See [ios/README.md](ios/README.md).

### Windows

xsystem4 can be built on Windows using MSYS2.

First install MSYS2, and then open the MINGW64 shell and run the following command,

    pacman -S make flex bison \
        mingw-w64-x86_64-gcc \
        mingw-w64-x86_64-meson \
        mingw-w64-x86_64-pkg-config \
        mingw-w64-x86_64-SDL2 \
        mingw-w64-x86_64-freetype \
        mingw-w64-x86_64-libjpeg-turbo \
        mingw-w64-x86_64-libwebp \
        mingw-w64-x86_64-libsndfile \
        mingw-w64-x86_64-glew \
        mingw-w64-x86_64-nasm \
        mingw-w64-x86_64-diffutils

To build with FFmpeg support, you must compile FFmpeg as a static library:

    git clone https://github.com/FFmpeg/FFmpeg.git
    cd FFmpeg
    git checkout n7.1.2
    ./configure --disable-everything \
        --enable-decoder=mpegvideo \
        --enable-decoder=mpeg1video \
        --enable-decoder=mpeg2video \
        --enable-decoder=mp2 \
        --enable-parser=mpegaudio \
        --enable-parser=mpegvideo \
        --enable-demuxer=mpegps \
        --enable-demuxer=mpegts \
        --enable-demuxer=mpegtsraw \
        --enable-demuxer=mpegvideo \
        --enable-decoder=vc1 \
        --enable-decoder=wmapro \
        --enable-parser=vc1 \
        --enable-hwaccel=vc1_d3d11va \
        --enable-hwaccel=vc1_d3d11va2 \
        --enable-hwaccel=vc1_dxva2 \
        --enable-demuxer=asf \
        --enable-protocol=file \
        --enable-filter=scale \
        --enable-static \
        --disable-shared \
        --extra-libs=-static \
        --extra-cflags=--static
    make
    make install
    cd ..
    export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"

Then build the xsystem4 executable with meson,

    mkdir build
    meson build
    ninja -C build

To create a portable executable, it is neccessary to copy some DLLs into the same directory as xsystem4.exe.
You can determine the required DLLs with the following command,

    ldd build/src/xsystem4.exe | grep mingw64

The `fonts` and `shaders` directories must also be shipped together with xsystem4.exe.

Installing
----------

If building from source, run:

    ninja -C build install

to install xsystem4 on your system.

### Windows

The recommended way to install on Windows is to copy the xsystem4 directory
into the game directory. E.g. if Sengoku Rance is installed at
`C:\Games\AliceSoft\Sengoku Rance`, then your filesystem should look something
like this:

    C:\
        Games\
            AliceSoft\
                Sengoku Rance\
                    System40.ini
                    xsystem4\
                        xsystem4.exe
                        ...
                    ...

Running
-------

You can run a game by passing the path to its game directory to the xsystem4
executable,

    build/src/xsystem4 /path/to/game_directory

Alternatively, run xsystem4 from within the game directory,

    cd /path/to/game_directory
    xsystem4

### Windows

If you've installed xsystem4 into the game directory as described above, simply
double-click `xsystem4.exe` to run the game.
