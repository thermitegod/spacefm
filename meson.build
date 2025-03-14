project(
    'spacefm',
    ['c', 'cpp'],
    version: '4.0.0-dev',
    license: 'GPL3',
    default_options: [
        'c_std=c23',
        'cpp_std=c++26',
        'buildtype=debugoptimized',
        'warning_level=3',
        'optimization=2',
        'prefix=/usr',
        'sysconfdir=/etc',
    ],
    meson_version: '>= 1.7.0',
)

bash = find_program('bash', required: true)
python = find_program('python3', required: true)

build_root = meson.project_build_root()
source_root = meson.project_source_root()

package = meson.project_name()
version = meson.project_version()
prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
libdir = join_paths(prefix, get_option('libdir'), package)
sysconfdir = get_option('sysconfdir')
localedir = join_paths(prefix, get_option('localedir'))
mandir = join_paths(prefix, get_option('mandir'))

## compiler stuff

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

# message(cc.get_id())
# message(cpp.get_id())

flags = []

if get_option('buildtype') == 'debug' or get_option('buildtype') == 'debugoptimized'
    flags += cpp.get_supported_arguments([
        '-ggdb3',
        '-pedantic',
        '-pedantic-errors',
        '-fno-omit-frame-pointer',
        '-mno-omit-leaf-frame-pointer',
        '-fasynchronous-unwind-tables',
        # '-ftime-trace',
        # '-fsave-optimization-record',

        '-Wfatal-errors',

        # '-Werror',
        '-Werror=date-time',
        '-Werror=unguarded-availability-new',
    ])
endif

if get_option('buildtype') == 'release'
    flags += cpp.get_supported_arguments([])
endif

# Default
flags += cpp.get_supported_arguments([
    '-fexceptions',

    '-finput-charset=UTF-8',
    '-fexec-charset=UTF-8',

    '-fno-delete-null-pointer-checks',
    '-fno-strict-aliasing',
    '-fno-strict-overflow',

    '-ftrivial-auto-var-init=zero',
    '-fzero-init-padding-bits=all',
])

# Hardening
if get_option('buildtype') != 'debug' and false
    flags += cpp.get_supported_arguments([
        '-Wformat=2',
        '-Wformat-overflow=2',
        '-Wformat-truncation',
        '-Werror=format-security',

        '-fstack-protector-strong',
        '-fstack-clash-protection',
        '-fcf-protection=full',

        '-Wp,-U_FORTIFY_SOURCE,-D_FORTIFY_SOURCE=3',

        # GCC libstdc++
        # '-Wp,-D_GLIBCXX_DEBUG',
        '-Wp,-D_GLIBCXX_ASSERTIONS',

        # LLVM libc++
        # '-Wp,-D_LIBCPP_ASSERT',
        # '-Wp,-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST',
    ])
endif

# Warnings
flags += cpp.get_supported_arguments([
    '-Walloca',
    '-Walloc-zero',
    '-Warray-bounds=2',
    '-Warray-compare',
    '-Wattribute-alias=2',
    '-Wbidi-chars=any',
    '-Wcast-align',
    '-Wcast-qual',
    '-Wchar-subscripts',
    '-Wcomment',
    '-Wconversion',
    '-Wdate-time',
    '-Wdisabled-optimization',
    '-Wdouble-promotion',
    '-Wduplicated-branches',
    '-Wduplicated-cond',
    # '-Wextra-semi', // TODO warnings from glibmm
    '-Wfloat-equal',
    '-Wignored-qualifiers',
    '-Wimplicit-fallthrough',
    '-Wimport',
    '-Winit-self',
    '-Wint-conversion',
    '-Wmissing-field-initializers',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wmissing-noreturn',
    '-Wnon-virtual-dtor',
    '-Wnull-dereference',
    # '-Wold-style-cast', // TODO warnings from glibmm
    '-Woverloaded-virtual',
    '-Wparentheses',
    '-Wpointer-arith',
    '-Wsequence-point',
    '-Wshadow',
    '-Wsign-compare',
    '-Wsign-conversion',
    '-Wstack-protector',
    '-Wstrict-aliasing',
    '-Wstring-compare',
    # '-Wstringop-overflow=4',
    '-Wswitch',
    '-Wswitch-enum',
    '-Wtrampolines',
    '-Wtrigraphs',
    '-Wundef',
    '-Wuninitialized',
    '-Wunknown-pragmas',
    '-Wunreachable-code',
    '-Wunused',
    '-Wunused-function',
    '-Wunused-label',
    '-Wunused-macros',
    '-Wunused-parameter',
    '-Wunused-result',
    '-Wunused-value',
    '-Wunused-variable',
    '-Wvariadic-macros',
    '-Wvla',
    '-Wwrite-strings',
    '-Wzero-length-bounds',
])

# Disabled Warnings
flags += cpp.get_supported_arguments([
    '-Wno-cast-align',
    '-Wno-double-promotion', # warns when using NAN
    '-Wno-cast-function-type-mismatch',

    # '-Wno-deprecated-declarations',
])

# Disabled GCC Warnings
if cpp.get_id() == 'gcc'
    flags += cpp.get_supported_arguments([
        '-Wno-changes-meaning', # TODO

        '-Wno-cast-function-type',
        '-Wno-cast-qual',
        '-Wno-maybe-uninitialized',
        '-Wno-shadow',  # gcc is a lot stricter than clang
    ])
endif

# Disabled Clang Warnings
if cpp.get_id() == 'clang'
    flags += cpp.get_supported_arguments([
        '-Wno-cast-qual',
    ])
endif

# Linker

link_flags = []
link_flags += cpp.get_supported_link_arguments([
    '-Wl,-O1',
    '-Wl,--as-needed',
    '-Wl,--sort-common',
    '-Wl,-z,relro',
    '-Wl,-z,now',
    '-Wl,-z,pack-relative-relocs',
    '-Wl,-z,noexecstack',
    '-Wl,-z,defs',
])

add_global_arguments(flags, language: ['c', 'cpp'])
add_global_link_arguments(link_flags, language: ['c', 'cpp'])

## system dependencies

cli11_dep = dependency('CLI11', required: true)
magic_enum_dep = dependency('magic_enum', required: true, version: '>=0.9.7')
libudev_dep = dependency('libudev', required: true)
botan_dep = dependency('botan-3', required: true)
pugixml_dep = dependency('pugixml', required: true)

base_dependencies = [
    cli11_dep,
    magic_enum_dep,
    libudev_dep,
    pugixml_dep,
    botan_dep,
]

sigc_dep = dependency('sigc++-3.0', required: true)

# Gtk3
if get_option('gtk3')
gtkmm3_dep = dependency('gtkmm-3.0', required: true, version:  '>=3.24.0')
glibmm3_dep = dependency('glibmm-2.4', required: true)
giomm3_dep = dependency('giomm-2.4', required: true)
exo_dep = dependency('exo-2', required: true)

gtk3_dependencies = [
    gtkmm3_dep,
    glibmm3_dep,
    giomm3_dep,
    sigc_dep,
    exo_dep,
]
endif

# Gtk4
gtkmm4_dep = dependency('gtkmm-4.0', required: true, version: '>=4.18.0')
glibmm4_dep = dependency('glibmm-2.68', required: true)
giomm4_dep = dependency('giomm-2.68', required: true)

gtk4_dependencies = [
    gtkmm4_dep,
    glibmm4_dep,
    giomm4_dep,
    sigc_dep,
]

## preprocessor static

preprocessor = [
    '-DPACKAGE_NAME="spacefm"',
    '-DPACKAGE_NAME_FANCY="SpaceFM"',
    '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
    '-DPACKAGE_GITHUB="https://www.github.com/thermitegod/spacefm"',
    '-DPACKAGE_BUGREPORT="https://www.github.com/thermitegod/spacefm/issues"',
    '-DPACKAGE_ONLINE_DOCS="TODO"',

    # used to run dialogs if they are not installed in the system path
    '-DPACKAGE_BUILD_ROOT="@0@"'.format(build_root),
    '-DDIALOG_BUILD_ROOT="@0@/src/gtk3-dialog"'.format(build_root),

    # data paths
    '-DPACKAGE_IMAGES="@0@"'.format(datadir / 'spacefm'),
    '-DPACKAGE_IMAGES_LOCAL="@0@"'.format(join_paths(meson.current_source_dir(), 'data/images')),
]

if get_option('dev')
    preprocessor += [
        '-DDEV_SCRIPTS_PATH="@0@"'.format(join_paths(meson.current_source_dir(), 'tools/scripts')),

        # GTKMM
        # '-DGTKMM_DISABLE_DEPRECATED', # Gdk::Pixbuf
        # '-DGDKMM_DISABLE_DEPRECATED', # Gdk::Pixbuf
        '-DGLIBMM_DISABLE_DEPRECATED',
        '-DGIOMM_DISABLE_DEPRECATED',
    ]
endif

if get_option('tests')
    preprocessor += '-DTEST_DATA_PATH="@0@"'.format(join_paths(meson.current_source_dir(), 'tests/test_data'))
endif

foreach a: preprocessor
    add_project_arguments(a, language: ['c', 'cpp'])
endforeach

## preprocessor features
features = []

if get_option('dev')
    features += '-DDEV_MODE'
endif

if get_option('with-system-glaze')
    glaze_dep = dependency('glaze', required: true, version: '>=6.3.0')
    base_dependencies += glaze_dep
else
    # TODO meson deprecated doing this
    # incdir += include_directories('subprojects/glaze/include')
endif

if get_option('with-system-ztd')
    ztd_dep = dependency('ztd', required: true)
    base_dependencies += ztd_dep
else
    ztd_proj = subproject('ztd', required: true)
    ztd_dep = ztd_proj.get_variable('ztd_dep')
    base_dependencies += ztd_dep
endif

if get_option('gtk3') and get_option('socket')
    cppzmq_dep = dependency('cppzmq', required: true)

    base_dependencies += [
        cppzmq_dep,
    ]

    features += '-DHAVE_SOCKET'
    features += '-DSOCKET_PORT="@0@"'.format(get_option('zmp_port'))
endif

media_dependencies = []
if get_option('media')
    # ffmpeg
    libavcodec_dep = dependency('libavcodec')
    # libavdevice_dep = dependency('libavdevice')
    # libavfilter_dep = dependency('libavfilter')
    libavformat_dep = dependency('libavformat')
    libavutil_dep = dependency('libavutil')
    # libswresample_dep = dependency('libswresample')
    # libswscale_dep = dependency('libswscale')

    gexiv_dep = dependency('gexiv2')
    gdkpixbuf_dep = dependency('gdk-pixbuf-2.0')

    media_dependencies += [
        libavcodec_dep,
        # libavdevice_dep,
        # libavfilter_dep,
        libavformat_dep,
        libavutil_dep,
        # libswresample_dep,
        # libswscale_dep,
        gexiv_dep,
        gdkpixbuf_dep,
    ]

    features += '-DHAVE_MEDIA'
endif

foreach a: features
    add_project_arguments(a, language: ['c', 'cpp'])
endforeach

# Build

subdir('src')

if get_option('tests')
    subdir('tests')
endif

if get_option('tools')
    subdir('tools')
endif

## Install
install_subdir('data/applications', install_dir: datadir)
install_subdir('data/icons', install_dir: datadir)
if get_option('gtk4')
    install_subdir('data/images', install_dir: datadir / 'spacefm')
endif
