project(
    'spacefm',
    ['c', 'cpp'],
    version: '4.0.0-dev',
    license: 'GPL3',
    default_options: [
        'c_std=c23',
        'cpp_std=c++26',
        # 'cpp_debugstl=true',
        # 'cpp_importstd=true',
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
bindir = prefix / get_option('bindir')
datadir = prefix / get_option('datadir')
libdir = prefix / get_option('libdir') / package
sysconfdir = get_option('sysconfdir')
localedir = prefix / get_option('localedir')
mandir = prefix / get_option('mandir')

## Compiler stuff

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

# message(cc.get_id())
# message(cpp.get_id())

flags = [
    # modules still dont work
    # '-fmodules',
    # '--compile-std-module',

    # very slow builds
    # '-freflection',
]

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
        '-Wno-c2y-extensions', # doctest.h
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

add_project_arguments(flags, language: ['c', 'cpp'])
add_project_link_arguments(link_flags, language: ['c', 'cpp'])

## Dependencies

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

gtkmm_dep = dependency('gtkmm-4.0', required: true, version: '>=4.18.0')
glibmm_dep = dependency('glibmm-2.68', required: true)
giomm_dep = dependency('giomm-2.68', required: true)
sigc_dep = dependency('sigc++-3.0', required: true)
glycin_dep = dependency('glycin-gtk4-2', required: true)

gtk_dependencies = [
    gtkmm_dep,
    glibmm_dep,
    giomm_dep,
    sigc_dep,
    glycin_dep,
]

if get_option('with-system-glaze')
    base_dependencies += dependency('glaze', required: true, version: '>=6.3.0')
else
    incdir += include_directories('subprojects/glaze/include')
endif

if get_option('with-system-ztd')
    base_dependencies += dependency('ztd', required: true)
else
    ztd_proj = subproject('ztd', required: true)
    base_dependencies += ztd_proj.get_variable('ztd_dep')
endif

## Preprocessor / Features

preprocessor = [
    '-DPACKAGE_NAME="spacefm"',
    '-DPACKAGE_NAME_FANCY="SpaceFM"',
    '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
    '-DPACKAGE_GITHUB="https://www.github.com/thermitegod/spacefm"',
    '-DPACKAGE_BUGREPORT="https://www.github.com/thermitegod/spacefm/issues"',
    '-DPACKAGE_ONLINE_DOCS="TODO"',

    # data paths
    '-DPACKAGE_IMAGES="@0@"'.format(datadir / 'spacefm' / 'images'),
    '-DPACKAGE_IMAGES_LOCAL="@0@"'.format(meson.current_source_dir() / 'data/images'),

    # test data
    '-DTEST_DATA_PATH="@0@"'.format(meson.current_source_dir() / 'tests/test_data'),
]

if get_option('dev')
    preprocessor += [
        # GTKMM
        '-DGTKMM_DISABLE_DEPRECATED',
        '-DGDKMM_DISABLE_DEPRECATED',
        '-DGLIBMM_DISABLE_DEPRECATED',
        '-DGIOMM_DISABLE_DEPRECATED',
    ]
endif

if get_option('dev')
    preprocessor += '-DDEV_MODE'
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

    media_dependencies += [
        libavcodec_dep,
        # libavdevice_dep,
        # libavfilter_dep,
        libavformat_dep,
        libavutil_dep,
        # libswresample_dep,
        # libswscale_dep,
        gexiv_dep,
    ]

    preprocessor += '-DHAVE_MEDIA'
endif

add_project_arguments(preprocessor, language: ['c', 'cpp'])

## Build

subdir('src/core')
subdir('src/spacefm')

if get_option('tests')
    subdir('tests')
endif

if get_option('tools')
    subdir('tools')
endif

## Install

install_subdir('share/applications', install_dir: datadir)
install_subdir('share/icons', install_dir: datadir)
install_subdir('share/spacefm', install_dir: datadir)
