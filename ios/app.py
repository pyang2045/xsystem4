#!/usr/bin/env python3
"""xsystem4 as an iPad app: Info.plist, bundle resources and the Xcode project, from a config.

    python3 -I ios/app.py get CONFIG KEY             name, display_name, bundle_id, version, assets
    python3 -I ios/app.py plist CONFIG [BUILD PLATFORM]
    python3 -I ios/app.py resources CONFIG           "BUNDLE_PATH<TAB>SOURCE" lines
    python3 -I ios/app.py xcodeproj CONFIG [MESON_BUILD_DIR]

CONFIG is a JSON file; relative paths in it are relative to its folder, which is also where
the Xcode project (NAME.xcodeproj) and its Info.plist (NAME/Info.plist) are written:

    {
      "name": "xsystem4",                  executable and product name (letters, digits, -_)
      "display_name": "xsystem4",          name under the icon
      "development_region": "en",          optional, default en
      "bundle_id": "org.xsystem4.xsystem4",
      "version": "1.0",                    optional; default: last version tag (1.2.3 or v1.2.3)
                                           of the config's git repo
      "assets": "Assets.xcassets",         optional asset catalog; its AppIcon set is the icon
      "resources": {"fonts/x.otf": "x.otf"}  optional: bundle path -> file (same base name)
    }

ios/xsystem4.json is the plain app: no icon; any games in Documents.
`plist` without BUILD prints the Info.plist for Xcode (with $(...) build variables); with
BUILD (CFBundleVersion) and PLATFORM (iPhoneOS or iPhoneSimulator) one for make_app.sh.
`xcodeproj` takes its source lists from ios/xsystem4.xcodeproj, or from an iOS meson build
(ios/cross.sh, -Ddebugger=disabled) when one is given: regenerate ios/xsystem4.xcodeproj that
way when source files are added or removed, then the projects made from it.
"""
import hashlib
import json
import os
import plistlib
import re
import subprocess
import sys
from xml.sax.saxutils import escape

XS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
MIN_IOS = "17.0"
# ios/xcframeworks.sh output, in link order
LIBS = ["SDL2", "SDL2main", "freetype", "ffi", "cglm", "sndfile", "FLAC", "vorbisenc", "vorbis", "ogg",
        "opus", "turbojpeg", "webp", "sharpyuv", "png16"]
SYS4_LIBS = ["png16", "turbojpeg", "webp"]  # the ones libsys4 includes headers from


# The libraries for every platform, packaged as XCFrameworks; both scripts return at once when
# up to date. A clean environment: Xcode's exported settings would steer CMake and configure.
DEPS_SH = r'''exec env -i HOME="$HOME" DEVELOPER_DIR="$DEVELOPER_DIR" XSYS4_IOS_DEPS="$XS_DEPS_ROOT" \
	PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
	/bin/sh -c '"$1/build_deps.sh" device sim-x86_64 sim-arm64 && "$1/xcframeworks.sh"' sh "$XS_SRC/ios"
'''
# Scheme pre-action: Xcode reads every XCFramework while planning the build, before any target
# runs, so on a fresh checkout they must be made first. Its output goes to a log; the ios-deps
# target runs the same step inside the build, where errors show.
PREBUILD_SH = '''mkdir -p "$XS_DEPS_ROOT"
exec >"$XS_DEPS_ROOT/prebuild.log" 2>&1
''' + DEPS_SH


def deps_dir():
    return os.path.abspath(os.environ.get("XSYS4_IOS_DEPS") or os.path.join(XS, "ios-deps"))


class Config:
    def __init__(self, path):
        self.path = os.path.abspath(path)
        self.dir = os.path.dirname(self.path)
        c = json.load(open(self.path))
        self.name = c["name"]
        if not re.fullmatch(r"[A-Za-z0-9_-]+", self.name):
            sys.exit("%s: name must be letters, digits, - or _" % path)
        self.display_name = c.get("display_name", self.name)
        self.region = c.get("development_region", "en")
        self.bundle_id = c["bundle_id"]
        self.version = c.get("version") or self.git_version()
        self.assets = os.path.normpath(os.path.join(self.dir, c["assets"])) if c.get("assets") else None
        if self.assets and not os.path.isfile(os.path.join(self.assets, "AppIcon.appiconset", "Contents.json")):
            sys.exit("%s: %s has no AppIcon.appiconset" % (path, self.assets))
        self.resources = {k: os.path.normpath(os.path.join(self.dir, v))
                          for k, v in c.get("resources", {}).items()}
        for dest, src in self.resources.items():
            if os.path.basename(dest) != os.path.basename(src):
                sys.exit("%s: resource %s must keep the name of %s" % (path, dest, src))

    def git_version(self):
        r = subprocess.run(["git", "-C", self.dir, "describe", "--tags", "--abbrev=0",
                            "--match", "[0-9]*", "--match", "v[0-9]*"],
                           capture_output=True, text=True)
        m = re.search(r"\d+(\.\d+)*", r.stdout)
        return m.group(0) if r.returncode == 0 and m else "0.0.0"


def info_plist(cfg, build=None, platform=None):
    p = {
        "CFBundleDevelopmentRegion": cfg.region,
        "CFBundleDisplayName": cfg.display_name,
        "CFBundleExecutable": "$(EXECUTABLE_NAME)",
        "CFBundleIdentifier": "$(PRODUCT_BUNDLE_IDENTIFIER)",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": "$(PRODUCT_NAME)",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "$(MARKETING_VERSION)",
        "CFBundleVersion": "$(CURRENT_PROJECT_VERSION)",
        "LSRequiresIPhoneOS": True,
        "UIDeviceFamily": [2],
        "UIRequiresFullScreen": True,
        "UIStatusBarHidden": True,
        "UILaunchScreen": {},
        "UISupportedInterfaceOrientations~ipad": ["UIInterfaceOrientationLandscapeLeft",
                                                  "UIInterfaceOrientationLandscapeRight"],
        "UIApplicationSupportsIndirectInputEvents": True,
        "UIFileSharingEnabled": True,  # the game goes into Documents through Finder or Files
        "LSSupportsOpeningDocumentsInPlace": True,
    }
    if build:  # filled in for make_app.sh; Xcode does this itself
        p.update(CFBundleExecutable=cfg.name, CFBundleIdentifier=cfg.bundle_id, CFBundleName=cfg.name,
                 CFBundleShortVersionString=cfg.version, CFBundleVersion=build,
                 CFBundleSupportedPlatforms=[platform], MinimumOSVersion=MIN_IOS)
    return plistlib.dumps(p, sort_keys=True).decode()


def resources(cfg):
    return sorted(cfg.resources.items())


# --- Xcode project -----------------------------------------------------------------------

class Project:
    def __init__(self, cfg, bld=None):
        self.cfg = cfg
        self.objects = {}
        if bld:
            self.from_meson(bld)
        else:
            self.from_xcodeproj(os.path.join(XS, "ios", "xsystem4.xcodeproj"))
        self.engine_h = self.headers("include", "src")
        self.sys4_h = self.headers("subprojects/libsys4/include", "subprojects/libsys4/src")

    def from_meson(self, bld):
        targets = json.load(open(os.path.join(bld, "meson-info", "intro-targets.json")))
        self.src_root = json.load(open(os.path.join(bld, "meson-info", "meson-info.json")))["directories"]["source"]
        if os.path.realpath(self.src_root) != os.path.realpath(XS):
            sys.exit("%s is a build of %s, not of %s" % (bld, self.src_root, XS))
        opts = {o["name"]: o["value"] for o in json.load(open(os.path.join(bld, "meson-info", "intro-buildoptions.json")))}
        if opts.get("debugger") != "disabled":  # the project sets no debugger defines
            sys.exit("%s: configure it with -Ddebugger=disabled" % bld)

        def sources(name):
            t = next(t for t in targets if t["name"] == name)
            out = []
            for ts in t["target_sources"]:
                out += [os.path.relpath(s, self.src_root) for s in ts.get("sources", [])]
            return sorted(set(out))
        self.engine = sources("xsystem4")
        self.sys4 = sources("sys4") + ["subprojects/libsys4/src/ini_lexer.l", "subprojects/libsys4/src/ini_parser.y"]

    def from_xcodeproj(self, proj):  # the source lists of an earlier project, made from meson
        r = subprocess.run(["plutil", "-convert", "json", "-o", "-", os.path.join(proj, "project.pbxproj")],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit("%s: %s" % (proj, r.stderr.strip()))
        o = json.loads(r.stdout)["objects"]
        base = os.path.dirname(proj)  # paths are SOURCE_ROOT-relative: the project folder

        def sources(name):
            t = next(t for t in o.values() if t["isa"] == "PBXNativeTarget" and t["productName"] == name)
            phase = next(o[p] for p in t["buildPhases"] if o[p]["isa"] == "PBXSourcesBuildPhase")
            return [os.path.relpath(os.path.normpath(os.path.join(base, o[o[f]["fileRef"]]["path"])), XS)
                    for f in phase["files"]]
        self.engine = sources("xsystem4")
        self.sys4 = sources("sys4")

    def headers(self, *dirs):
        out = []
        for d in dirs:
            for root, _, files in os.walk(os.path.join(XS, d)):
                out += [os.path.relpath(os.path.join(root, f), XS) for f in files if f.endswith(".h")]
        return sorted(out)

    # paths as seen from the project folder
    def rel(self, path):
        return os.path.relpath(path, self.cfg.dir)

    def xs(self, p):
        return self.rel(os.path.join(XS, p))

    def oid(self, *key):
        return hashlib.sha1("/".join((self.cfg.name,) + key).encode()).hexdigest()[:24].upper()

    def add(self, key, isa, **fields):
        i = self.oid(*key) if isinstance(key, tuple) else self.oid(key)
        self.objects[i] = dict(isa=isa, **fields)
        return i

    TYPES = {".c": "sourcecode.c.c", ".m": "sourcecode.c.objc", ".h": "sourcecode.c.h",
             ".y": "sourcecode.yacc", ".l": "sourcecode.lex", ".plist": "text.plist.xml", ".png": "image.png"}

    def fileref(self, path, folder=False):
        t = "folder" if folder else self.TYPES.get(os.path.splitext(path)[1], "text")
        return self.add(("ref", path), "PBXFileReference", path=path, name=os.path.basename(path),
                        sourceTree="SOURCE_ROOT", lastKnownFileType=t)

    def group(self, name, children, key):
        return self.add(("group", key), "PBXGroup", name=name, children=children, sourceTree="<group>")

    def tree(self, paths, strip, key):
        root = {}
        for p in paths:
            rel = os.path.relpath(p, strip)
            node = root
            for part in filter(None, os.path.dirname(rel).split(os.sep)):
                node = node.setdefault(part + "/", {})
            node[os.path.basename(rel)] = p

        def build(node, prefix):
            kids = []
            for k in sorted(node, key=lambda k: (not k.endswith("/"), k.lower())):
                if k.endswith("/"):
                    kids.append(self.group(k[:-1], build(node[k], prefix + k), key + "/" + prefix + k))
                else:
                    kids.append(self.fileref(self.xs(node[k])))
            return kids
        return build(root, "")

    def buildfile(self, ref, key, settings=None):
        f = dict(fileRef=ref)
        if settings:
            f["settings"] = settings
        return self.add(("bf", key, ref), "PBXBuildFile", **f)

    def config_list(self, key, debug, release):
        d = self.add(("cfg", key, "Debug"), "XCBuildConfiguration", name="Debug", buildSettings=debug)
        r = self.add(("cfg", key, "Release"), "XCBuildConfiguration", name="Release", buildSettings=release)
        return self.add(("cfglist", key), "XCConfigurationList", buildConfigurations=[d, r],
                        defaultConfigurationIsVisible="0", defaultConfigurationName="Release")

    def phase(self, key, isa, files, **extra):
        return self.add(key, isa, buildActionMask="2147483647", files=files,
                        runOnlyForDeploymentPostprocessing="0", **extra)

    def build(self):
        cfg = self.cfg
        deps = "$(PROJECT_DIR)/" + self.rel(deps_dir())
        xcf = os.path.join(deps_dir(), "xcframeworks")
        common = {
            "SDKROOT": "iphoneos",
            "SUPPORTED_PLATFORMS": "iphoneos iphonesimulator",
            "TARGETED_DEVICE_FAMILY": "2",
            "IPHONEOS_DEPLOYMENT_TARGET": MIN_IOS,
            "XS_DEPS_ROOT": deps,
            "XS_SRC": "$(PROJECT_DIR)/" + self.rel(XS),
            "GCC_C_LANGUAGE_STANDARD": "c11",
            "CLANG_ENABLE_OBJC_ARC": "YES",
            "USE_HEADERMAP": "NO",  # include paths only, as meson
            "ALWAYS_SEARCH_USER_PATHS": "NO",
            "ENABLE_USER_SCRIPT_SANDBOXING": "NO",  # scripts run git and Homebrew's bison
            "WARNING_CFLAGS": ["-Wall"],
            # Xcode passes absolute source paths: keep the builder's folders out of __FILE__
            "OTHER_CFLAGS": ["-ffile-prefix-map=$(XS_SRC:standardizepath)/="],
            "CODE_SIGN_STYLE": "Automatic",
        }
        debug = dict(common, GCC_OPTIMIZATION_LEVEL="0", DEBUG_INFORMATION_FORMAT="dwarf",
                     ENABLE_TESTABILITY="YES", ONLY_ACTIVE_ARCH="YES")
        release = dict(common, GCC_OPTIMIZATION_LEVEL="2", DEBUG_INFORMATION_FORMAT="dwarf-with-dsym",
                       VALIDATE_PRODUCT="YES", ONLY_ACTIVE_ARCH="NO")

        sys4_settings = {
            "PRODUCT_NAME": "sys4",
            "HEADER_SEARCH_PATHS": ["$(XS_SRC)/subprojects/libsys4/include", "$(XS_SRC)/subprojects/libsys4/src",
                                    "$(DERIVED_FILE_DIR)"],
            "GCC_PREPROCESSOR_DEFINITIONS": ["_DEFAULT_SOURCE"],
            "SKIP_INSTALL": "YES",
        }
        def app_settings(dev_hooks):
            return {
                "PRODUCT_NAME": cfg.name,
                "PRODUCT_BUNDLE_IDENTIFIER": cfg.bundle_id,
                "MARKETING_VERSION": cfg.version,
                "CURRENT_PROJECT_VERSION": "1",
                "INFOPLIST_FILE": cfg.name + "/Info.plist",
                "HEADER_SEARCH_PATHS": ["$(DERIVED_FILE_DIR)", "$(XS_SRC)/src", "$(XS_SRC)/include",
                                        "$(XS_SRC)/subprojects/libsys4/include"],
                "GCC_PREPROCESSOR_DEFINITIONS": ["_DEFAULT_SOURCE", "_REENTRANT", "XSYS4_IOS", "USE_METAL",
                                                 "XSYS4_DEV_HOOKS=%d" % dev_hooks,
                                                 'XSYS4_DATA_DIR=\\"/usr/local/share/xsystem4\\"'],
                "WARNING_CFLAGS": ["-Wall", "-Wno-unused-parameter"],
                "OTHER_LDFLAGS": ["-Wl,-stack_size,0x20000000", "-liconv", "-lz"],
                "ASSETCATALOG_COMPILER_APPICON_NAME": "AppIcon" if cfg.assets else "",
            }

        bison = r'''for b in "$BISON" /opt/homebrew/opt/bison/bin/bison /usr/local/opt/bison/bin/bison; do
	[ -n "$b" ] && [ -x "$b" ] && break
done
[ -x "$b" ] || { echo "error: bison 3 not found (brew install bison)"; exit 1; }
"$b" "$INPUT_FILE_PATH" --defines="$DERIVED_FILE_DIR/$INPUT_FILE_BASE.tab.h" --output="$DERIVED_FILE_DIR/$INPUT_FILE_BASE.tab.c"
'''
        flex = r'''f=flex
for b in /opt/homebrew/opt/flex/bin/flex /usr/local/opt/flex/bin/flex; do [ -x "$b" ] && f="$b" && break; done
"$f" -o "$DERIVED_FILE_DIR/$INPUT_FILE_BASE.yy.c" "$INPUT_FILE_PATH"
'''
        version_h = r'''v="$(git -C "$XS_SRC" describe HEAD 2>/dev/null || echo 'unknown version')"
out="$DERIVED_FILE_DIR/version.h"
mkdir -p "$DERIVED_FILE_DIR"
printf '#define XSYSTEM4_VERSION "%s"\n' "$v" > "$out.tmp"
cmp -s "$out.tmp" "$out" && rm "$out.tmp" || mv "$out.tmp" "$out"
'''
        finish = r'''# the licence beside the engine; the shader converter is not needed in the app
res="$TARGET_BUILD_DIR/$UNLOCALIZED_RESOURCES_FOLDER_PATH"
cp "$XS_SRC/COPYING" "$res/COPYING.xsystem4"
rm -f "$res/shaders/metal/glsl2msl.py"
'''
        deps_target = self.add("target-deps", "PBXAggregateTarget", name="ios-deps", productName="ios-deps",
                               buildConfigurationList=self.config_list("deps", {"PRODUCT_NAME": "ios-deps"},
                                                                       {"PRODUCT_NAME": "ios-deps"}),
                               buildPhases=[self.phase("deps-build", "PBXShellScriptBuildPhase", [],
                                                       name="Build libraries (ios/build_deps.sh, ios/xcframeworks.sh)",
                                                       inputPaths=["$(XS_SRC)/ios/build_deps.sh",
                                                                   "$(XS_SRC)/ios/xcframeworks.sh"],
                                                       inputFileListPaths=[], outputFileListPaths=[],
                                                       outputPaths=["$(XS_DEPS_ROOT)/xcframeworks/stamp"],
                                                       shellPath="/bin/sh", shellScript=DEPS_SH)],
                               dependencies=[])

        def depends_on(target, name):
            proxy = self.add(("proxy", name), "PBXContainerItemProxy", containerPortal=self.oid("project"),
                             proxyType="1", remoteGlobalIDString=target, remoteInfo=name)
            return self.add(("dep", name), "PBXTargetDependency", target=target, targetProxy=proxy)

        rules = [
            self.add("rule-bison", "PBXBuildRule", compilerSpec="com.apple.compilers.proxy.script",
                     filePatterns="*.y", fileType="pattern.proxy", inputFiles=[], isEditable="1", script=bison,
                     runOncePerArchitecture="0",  # the generated C is the same for every arch
                     outputFiles=["$(DERIVED_FILE_DIR)/$(INPUT_FILE_BASE).tab.c",
                                  "$(DERIVED_FILE_DIR)/$(INPUT_FILE_BASE).tab.h"]),
            # the lexer includes the parser's header: wait for it
            self.add("rule-flex", "PBXBuildRule", compilerSpec="com.apple.compilers.proxy.script",
                     filePatterns="*.l", fileType="pattern.proxy", inputFiles=["$(DERIVED_FILE_DIR)/ini_parser.tab.h"],
                     isEditable="1", script=flex, outputFiles=["$(DERIVED_FILE_DIR)/$(INPUT_FILE_BASE).yy.c"],
                     runOncePerArchitecture="0"),
        ]

        xcf_refs = {l: self.add(("xcf", l), "PBXFileReference", lastKnownFileType="wrapper.xcframework",
                                name=l + ".xcframework", path=self.rel(os.path.join(xcf, l + ".xcframework")),
                                sourceTree="SOURCE_ROOT") for l in LIBS}
        sys4_product = self.add("sys4-product", "PBXFileReference", explicitFileType="archive.ar",
                                includeInIndex="0", path="libsys4.a", sourceTree="BUILT_PRODUCTS_DIR")
        sys4 = self.add("target-sys4", "PBXNativeTarget", name="sys4", productName="sys4",
                        productReference=sys4_product, productType="com.apple.product-type.library.static",
                        buildConfigurationList=self.config_list("sys4", sys4_settings, sys4_settings),
                        buildPhases=[self.phase("sys4-sources", "PBXSourcesBuildPhase",
                                                [self.buildfile(self.fileref(self.xs(p)), "sys4") for p in self.sys4]),
                                     # for their headers; the app links them
                                     self.phase("sys4-link", "PBXFrameworksBuildPhase",
                                                [self.buildfile(xcf_refs[l], "sys4-link") for l in SYS4_LIBS])],
                        buildRules=rules, dependencies=[depends_on(deps_target, "ios-deps")])

        version_phase = self.phase("app-version", "PBXShellScriptBuildPhase", [], name="Generate version.h",
                                   inputPaths=[], inputFileListPaths=[], outputFileListPaths=[],
                                   outputPaths=["$(DERIVED_FILE_DIR)/version.h"], alwaysOutOfDate="1",
                                   shellPath="/bin/sh", shellScript=version_h)
        src_phase = self.phase("app-sources", "PBXSourcesBuildPhase",
                               [self.buildfile(self.fileref(self.xs(p)), "app") for p in self.engine])

        frameworks = ["AVFoundation", "AudioToolbox", "CoreAudio", "CoreBluetooth", "CoreFoundation",
                      "CoreGraphics", "CoreMotion", "CoreVideo", "Foundation", "Metal", "OpenGLES",
                      "QuartzCore", "UIKit"]
        weak = ["CoreHaptics", "GameController"]
        fw_refs = []
        fw_files = [self.buildfile(sys4_product, "link")] + [self.buildfile(xcf_refs[l], "link") for l in LIBS]
        for fw in frameworks + weak:
            r = self.add(("fw", fw), "PBXFileReference", lastKnownFileType="wrapper.framework",
                         name=fw + ".framework", path="System/Library/Frameworks/%s.framework" % fw,
                         sourceTree="SDKROOT")
            fw_refs.append(r)
            fw_files.append(self.buildfile(r, "link", {"ATTRIBUTES": ["Weak"]} if fw in weak else None))
        link_phase = self.phase("app-link", "PBXFrameworksBuildPhase", fw_files)

        plist_ref = self.fileref(cfg.name + "/Info.plist")
        fonts = self.fileref(self.xs("fonts"), folder=True)
        metal = self.fileref(self.xs("shaders/metal"), folder=True)
        icon_refs = []
        if cfg.assets:
            icon_refs = [self.add(("ref", "assets"), "PBXFileReference", lastKnownFileType="folder.assetcatalog",
                                  name=os.path.basename(cfg.assets), path=self.rel(cfg.assets), sourceTree="SOURCE_ROOT")]
        res_phase = self.phase("app-resources", "PBXResourcesBuildPhase",
                               [self.buildfile(r, "res") for r in [fonts] + icon_refs])
        copies = [self.phase("app-copy-shaders", "PBXCopyFilesBuildPhase", [self.buildfile(metal, "copy")],
                             name="Shaders", dstPath="shaders", dstSubfolderSpec="7")]
        extra_refs = []
        by_dir = {}
        for dest, src in sorted(cfg.resources.items()):
            by_dir.setdefault(os.path.dirname(dest), []).append(self.fileref(self.rel(src)))
        for d, refs in sorted(by_dir.items()):
            extra_refs += refs
            copies.append(self.phase(("app-copy", d), "PBXCopyFilesBuildPhase",
                                     [self.buildfile(r, "copy") for r in refs],
                                     name="Resources: " + (d or "."), dstPath=d, dstSubfolderSpec="7"))
        finish_phase = self.phase("app-finish", "PBXShellScriptBuildPhase", [], name="Licence file",
                                  inputPaths=["$(XS_SRC)/COPYING"], inputFileListPaths=[], outputFileListPaths=[],
                                  outputPaths=["$(TARGET_BUILD_DIR)/$(UNLOCALIZED_RESOURCES_FOLDER_PATH)/COPYING.xsystem4"],
                                  shellPath="/bin/sh", shellScript=finish)

        app_product = self.add("app-product", "PBXFileReference", explicitFileType="wrapper.application",
                               includeInIndex="0", path=cfg.name + ".app", sourceTree="BUILT_PRODUCTS_DIR")
        app = self.add("target-app", "PBXNativeTarget", name=cfg.name, productName=cfg.name,
                       productReference=app_product, productType="com.apple.product-type.application",
                       buildConfigurationList=self.config_list("app", app_settings(1), app_settings(0)),
                       buildPhases=[version_phase, src_phase, link_phase, res_phase] + copies + [finish_phase],
                       buildRules=[], dependencies=[depends_on(sys4, "sys4")])

        main = self.add(("group", "main"), "PBXGroup", sourceTree="<group>", children=[
            self.group(cfg.name, [plist_ref] + icon_refs, "support"),
            self.group("xsystem4", self.tree(self.engine + self.engine_h, ".", "engine"), "engine"),
            self.group("libsys4", self.tree(self.sys4 + self.sys4_h, "subprojects/libsys4", "sys4"), "sys4"),
            self.group("Resources", [fonts, metal] + extra_refs, "resources"),
            self.group("Frameworks", [xcf_refs[l] for l in LIBS] + fw_refs, "frameworks"),
            self.group("Products", [app_product, sys4_product], "products"),
            self.group("Build", [self.fileref(self.xs("ios/build_deps.sh")),
                                 self.fileref(self.xs("ios/xcframeworks.sh"))], "build"),
        ])
        products = self.oid("group", "products")
        self.objects[self.oid("project")] = dict(
            isa="PBXProject",
            attributes=dict(BuildIndependentTargetsInParallel="1", LastUpgradeCheck="1600",
                            TargetAttributes={t: dict(CreatedOnToolsVersion="16.0") for t in (app, sys4, deps_target)}),
            buildConfigurationList=self.config_list("project", debug, release),
            compatibilityVersion="Xcode 14.0", developmentRegion=cfg.region, hasScannedForEncodings="0",
            knownRegions=sorted({cfg.region, "en", "Base"}), mainGroup=main, productRefGroup=products, projectDirPath="",
            projectRoot="", targets=[app, sys4, deps_target])
        return app

    SAFE = re.compile(r"^[A-Za-z0-9_$/.]+$")

    def q(self, s):
        s = str(s)
        if s and self.SAFE.match(s) and "//" not in s:
            return s
        return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t") + '"'

    def ser(self, v, ind):
        pad = "\t" * ind
        if isinstance(v, dict):
            out = "{\n"
            for k in sorted(v, key=lambda k: (k != "isa", k)):
                out += "%s\t%s = %s;\n" % (pad, self.q(k), self.ser(v[k], ind + 1))
            return out + pad + "}"
        if isinstance(v, list):
            return "(\n" + "".join("%s\t%s,\n" % (pad, self.ser(x, ind + 1)) for x in v) + pad + ")"
        return self.q(v)

    def write(self):
        cfg = self.cfg
        app = self.build()
        proj = os.path.join(cfg.dir, cfg.name + ".xcodeproj")
        os.makedirs(proj, exist_ok=True)
        with open(os.path.join(proj, "project.pbxproj"), "w") as f:
            f.write("// !$*UTF8*$!\n{\n\tarchiveVersion = 1;\n\tclasses = {\n\t};\n\tobjectVersion = 56;\n\tobjects = {\n")
            for i in sorted(self.objects):
                f.write("\t\t%s = %s;\n" % (i, self.ser(self.objects[i], 2)))
            f.write("\t};\n\trootObject = %s;\n}\n" % self.oid("project"))
        os.makedirs(os.path.join(cfg.dir, cfg.name), exist_ok=True)
        with open(os.path.join(cfg.dir, cfg.name, "Info.plist"), "w") as f:
            f.write(info_plist(cfg))
        schemes = os.path.join(proj, "xcshareddata", "xcschemes")
        os.makedirs(schemes, exist_ok=True)
        ref = ('<BuildableReference BuildableIdentifier = "primary" BlueprintIdentifier = "%s" '
               'BuildableName = "%s.app" BlueprintName = "%s" ReferencedContainer = "container:%s.xcodeproj">'
               '</BuildableReference>' % (app, cfg.name, cfg.name, cfg.name))
        with open(os.path.join(schemes, cfg.name + ".xcscheme"), "w") as f:
            pre = escape(PREBUILD_SH, {'"': "&quot;", "\n": "&#10;", "\t": "&#9;"})
            f.write(SCHEME % (pre, ref, ref, ref, ref))
        return proj


SCHEME = '''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion = "1600" version = "1.7">
   <BuildAction parallelizeBuildables = "YES" buildImplicitDependencies = "YES">
      <PreActions>
         <ExecutionAction ActionType = "Xcode.IDEStandardExecutionActionsCore.ExecutionActionType.ShellScriptAction">
            <ActionContent title = "Build libraries (ios-deps/prebuild.log)" scriptText = "%s" shellToInvoke = "/bin/sh">
               <EnvironmentBuildable>
                  %s
               </EnvironmentBuildable>
            </ActionContent>
         </ExecutionAction>
      </PreActions>
      <BuildActionEntries>
         <BuildActionEntry buildForTesting = "YES" buildForRunning = "YES" buildForProfiling = "YES" buildForArchiving = "YES" buildForAnalyzing = "YES">
            %s
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <TestAction buildConfiguration = "Debug" selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv = "YES">
   </TestAction>
   <LaunchAction buildConfiguration = "Debug" selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB" launchStyle = "0" useCustomWorkingDirectory = "NO" ignoresPersistentStateOnLaunch = "NO" debugDocumentVersioning = "YES" debugServiceExtension = "internal" allowLocationSimulation = "NO">
      <BuildableProductRunnable runnableDebuggingMode = "0">
         %s
      </BuildableProductRunnable>
   </LaunchAction>
   <ProfileAction buildConfiguration = "Release" shouldUseLaunchSchemeArgsEnv = "YES" savedToolIdentifier = "" useCustomWorkingDirectory = "NO" debugDocumentVersioning = "YES">
      <BuildableProductRunnable runnableDebuggingMode = "0">
         %s
      </BuildableProductRunnable>
   </ProfileAction>
   <AnalyzeAction buildConfiguration = "Debug">
   </AnalyzeAction>
   <ArchiveAction buildConfiguration = "Release" revealArchiveInOrganizer = "YES">
   </ArchiveAction>
</Scheme>
'''


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("get", "plist", "resources", "xcodeproj"):
        sys.exit(__doc__.split("\n\n")[1])
    cmd, cfg = sys.argv[1], Config(sys.argv[2])
    if cmd == "get":
        keys = {"name": cfg.name, "display_name": cfg.display_name, "bundle_id": cfg.bundle_id,
                "version": cfg.version, "assets": cfg.assets or ""}
        print(keys[sys.argv[3]])
    elif cmd == "plist":
        sys.stdout.write(info_plist(cfg, *sys.argv[3:5]) if len(sys.argv) >= 5 else info_plist(cfg))
    elif cmd == "resources":
        for dest, src in resources(cfg):
            print("%s\t%s" % (dest, src))
    else:
        print(Project(cfg, os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else None).write())


if __name__ == "__main__":
    main()
