project('system4', 'c',
        meson_version: '>= 0.59',
        default_options : ['c_std=c11'])
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')
add_project_arguments('-DXSYS4_DATA_DIR="' + get_option('prefix') / get_option('datadir') / 'xsystem4"', language : 'c')

if get_option('buildtype').startswith('release')
    add_project_arguments('-DRELEASE', language : 'c')
endif

static_libs = false
if host_machine.system() == 'windows'
    static_libs = true
endif

zlib = dependency('zlib', static : static_libs)
libm = meson.get_compiler('c').find_library('m', required: false)
sdl2 = dependency('sdl2', static : static_libs)
ft2 = dependency('freetype2', static : static_libs)
ffi = dependency('libffi', static : static_libs)
tj = dependency('libturbojpeg', static : static_libs)
webp = dependency('libwebp', static : static_libs)
png = dependency('libpng', static : static_libs)
cglm = dependency('cglm', version : '>=0.9.2', fallback : ['cglm', 'cglm_dep'])
sndfile = dependency('sndfile', static : static_libs)

avcodec = dependency('libavcodec', version : '>=59.37.100', required: false, static : static_libs)
avformat = dependency('libavformat', required: false, static : static_libs)
avutil = dependency('libavutil', version : '>=57.28.100', required: false, static : static_libs)
swscale = dependency('libswscale', required: false, static : static_libs)

if get_option('metal').enabled()
    # The GL subset of include/gfx/gl_metal.h on Metal (src/metal/gl_metal.m).
    # No 3D: the 3D and dungeon HLLs stop with an error when used.
    add_languages('objc', native : false)
    gles = dependency('', required : false)
    gl_deps = [dependency('appleframeworks', modules : ['Metal', 'QuartzCore', 'Foundation'])]
    add_project_arguments('-DUSE_METAL', language : ['c', 'objc'])
    add_project_arguments('-D_DEFAULT_SOURCE', '-fobjc-arc', language : 'objc')
elif host_machine.system() == 'darwin' and not get_option('opengles').disabled()
    # macOS has no OpenGL ES of its own and its OpenGL is deprecated, so the
    # default build targets OpenGL ES on ANGLE, which runs on Metal (see
    # angle_setup in src/video.c). ANGLE comes from pkg-config (glesv2) or from
    # angle_dir as laid out by the port's tools/fetch_angle.sh. Apple's OpenGL:
    # -Dopengles=disabled.
    gles = dependency('glesv2', version : '>=3', required : false)
    if not gles.found()
        angle_dir = meson.project_source_root() / get_option('angle_dir')
        angle_lib = angle_dir / (host_machine.cpu_family() == 'aarch64' ? 'arm64-dev' : 'x86_64-dev')
        glesv2 = meson.get_compiler('c').find_library('GLESv2', dirs : [angle_lib], required : false)
        if not glesv2.found()
            error('no ANGLE in ' + angle_lib + ': run tools/fetch_angle.sh in the haharanman-port repo, '
                  + 'set -Dangle_dir=DIR, or build for Apple OpenGL with -Dopengles=disabled')
        endif
        gles = declare_dependency(dependencies : glesv2,
                                  compile_args : ['-I' + angle_dir / 'include'],
                                  link_args : ['-Wl,-rpath,' + angle_lib])
    endif
else
    gles = dependency('glesv2', version : '>=3', required: get_option('opengles'))
endif
if get_option('metal').enabled()
elif gles.found()
    gl_deps = [gles]
    add_project_arguments('-DUSE_GLES', language : 'c')
else
    gl = dependency('GL')
    glew = dependency('glew')
    gl_deps = [gl, glew]
endif

chibi = dependency('chibi-scheme', required : false, static : static_libs)
readline = dependency('readline', required : false, static : static_libs)

flex = find_program('flex')
bison = find_program('bison')

incdir = include_directories('include')

libsys4_proj = subproject('libsys4')
libsys4_dep = libsys4_proj.get_variable('libsys4_dep')

subdir('src')

install_subdir('shaders', install_dir : get_option('datadir') / 'xsystem4')
install_subdir('fonts', install_dir : get_option('datadir') / 'xsystem4')
install_data('debugger.scm', install_dir : get_option('datadir') / 'xsystem4')
