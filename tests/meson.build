project(
  'test_suite', ['c', 'cpp'],
  version : '1.0.0',
  license : 'GPL3',
  default_options : [
    'cpp_std=c++23',
    'warning_level=3',
    'optimization=2',
  ],
  meson_version : '>= 1.0.0'
)

build_root = meson.project_build_root()
source_root = meson.project_source_root()

incdir = include_directories(['src', 'spacefm'])

package = meson.project_name()
version = meson.project_version()

## compiler stuff

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

# message(cc.get_id())
# message(cpp.get_id())

flags = []
link_flags = []

debug_flags = [
    '-ggdb3',

    '-pedantic',
    '-pedantic-errors',
    # '-Wfatal-errors',
]

if get_option('buildtype') == 'debug'
    flags += cpp.get_supported_arguments(debug_flags)
endif

release_flags = []

if get_option('buildtype') == 'release'
    flags += cpp.get_supported_arguments(release_flags)
endif

default_warning_flags = [
    '-Wchar-subscripts',
    '-Wcomment',
    '-Wdisabled-optimization',
    '-Wdouble-promotion',
    '-Wfloat-equal',
    '-Wformat=2',
    '-Wimplicit-fallthrough',
    '-Wimport',
    '-Winit-self',
    '-Wmissing-field-initializers',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wmissing-noreturn',
    '-Wparentheses',
    '-Wpointer-arith',
    '-Wsequence-point',
    '-Wshadow',
    '-Wsign-compare',
    '-Wstack-protector',
    '-Wstrict-aliasing',
    '-Wswitch',
    '-Wswitch-default',
    '-Wswitch-enum',
    '-Wtrigraphs',
    '-Wundef',
    '-Wuninitialized',
    '-Wunknown-pragmas',
    '-Wunreachable-code',
    '-Wunused',
    '-Wunused-function',
    '-Wunused-label',
    '-Wunused-parameter',
    '-Wunused-value',
    '-Wunused-result',
    '-Wunused-variable',
    '-Wunused-macros',
    '-Wvariadic-macros',
    '-Wvla',
    '-Wwrite-strings',
    '-Wignored-qualifiers',
    '-Wstring-compare',
    '-Walloca',
    '-Wdate-time',

    # Disabled Warnings
    '-Wno-double-promotion',
]

flags += cpp.get_supported_arguments(default_warning_flags)

if cpp.has_multi_arguments('-Wformat', '-Werror=format-security')
    flags += '-Werror=format-security'
endif

# GCC only warning flags
if cpp.get_id() == 'gcc'
    default_warning_flags_gcc = [
        '-Wformat-overflow=2',
        '-Wformat-truncation',
        '-Warray-bounds=2',
        '-Warray-compare',
        '-Wzero-length-bounds',
        '-Wattribute-alias=2',
        '-Wduplicated-branches',
        '-Wduplicated-cond',
        '-Wtrampolines',
        '-Walloc-zero',
        '-Wstringop-overflow=4',

        # Disabled Warnings
    ]

    flags += cpp.get_supported_arguments(default_warning_flags_gcc)
endif

# Clang only warning flags
if cpp.get_id() == 'clang'
    default_warning_flags_clang = [
        # Disabled Warnings
    ]

    flags += cpp.get_supported_arguments(default_warning_flags_clang)
endif

if cpp.has_link_argument('-Wl,--as-needed')
    link_flags += '-Wl,--as-needed'
endif

noexecstack = false
if cpp.has_link_argument('-Wl,-z,noexecstack')
    link_flags += '-Wl,-z,noexecstack'
    noexecstack = true
endif

add_global_arguments(flags, language: ['c', 'cpp'])
add_global_link_arguments(link_flags, language: ['c', 'cpp'])

## preprocessor

preprocessor = [
  '-DZTD_VERSION=2',
]

foreach a : preprocessor
  add_project_arguments(a, language : ['c', 'cpp'])
endforeach

## dependencies

# ztd use system or subproject
ztd_dep = dependency('ztd', required : false)
if not ztd_dep.found() # using subproject
  ztd_proj = subproject('ztd', required : true)
  ztd_dep = ztd_proj.get_variable('ztd_dep')
endif

# magic_enum use system or subproject
magic_enum_dep = dependency('magic_enum', required : false)
if not magic_enum_dep.found() # using subproject
  magic_enum_proj = subproject('magic_enum', required : true)
  magic_enum_dep = magic_enum_proj.get_variable('magic_enum_dep')
endif

# toml11 use system or subproject
toml11_dep = dependency('toml11', required : false)
if not toml11_dep.found() # using subproject
  cmake = import('cmake')
  toml11_proj = cmake.subproject('toml11', required : true)
  toml11_dep = toml11_proj.dependency('toml11')
endif

# CLI11 use system or subproject
cli11_dep = dependency('CLI11', required : false)
if not cli11_dep.found() # using subproject
  cli11_dep_proj = subproject('cli11', required : true)
  cli11_dep_dep = cli11_dep_proj.get_variable('CLI11_dep')
endif

libudev_dep = dependency('libudev', required : true)
libffmpegthumbnailer_dep = dependency('libffmpegthumbnailer', required : true)
openssl_dep = dependency('openssl', required : true)

exo_dep = dependency('exo-2', required : true)
# x11_dep = dependency('x11', required : true)
spdlog_dep = dependency('spdlog', required : true)
sigc_dep = dependency('sigc++-3.0', required : true)
pugixml_dep = dependency('pugixml', required : true)
nlohmann_json_dep = dependency('nlohmann_json', required : true)

# zmqpp does not provide a pkgconfig file
libzmq_dep = dependency('libzmq', required : true)
libzmqpp_dep = cc.find_library('zmqpp', has_headers: ['zmqpp/zmqpp.hpp'], required: true)

# Gtk3 CPP
gtkmm_dep = dependency('gtkmm-3.0', required : true)
glibmm_dep = dependency('glibmm-2.4', required : true)
giomm_dep = dependency('giomm-2.4', required : true)

# Gtk4 CPP
# gtkmm_dep = dependency('gtkmm-4.0', required : true)
# glibmm_dep = dependency('glibmm-2.68', required : true)
# giomm_dep = dependency('giomm-2.68', required : true)

gtest_dep = dependency('gtest')

dependencies = [
    gtest_dep,

    gtkmm_dep,
    glibmm_dep,
    giomm_dep,

    exo_dep,

    magic_enum_dep,
    toml11_dep,

    spdlog_dep,
    sigc_dep,
    pugixml_dep,
    nlohmann_json_dep,
    cli11_dep,

    libzmq_dep,
    libzmqpp_dep,

    libudev_dep,
    libffmpegthumbnailer_dep,
    openssl_dep,

    ztd_dep,
]

## Source files

sources = files()

# Spacefm Source Files
sources += files(
  'spacefm/ptk/natsort/strnatcmp.cxx',
)

# Test Source Files
sources += files(
  'src/main.cxx',

  # PTK
  'src/ptk/natsort/natsort_test.cxx',

  # VFS
)

## Build
target_name = 'test_suite'
target_type = 'executable'

test_suite = build_target(
  target_name,
  sources: sources,
  target_type: target_type,
  include_directories: incdir,
  install : false,
  dependencies: dependencies,
)

test('SpaceFM Test Suite', test_suite)
