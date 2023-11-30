project(
    'spacefm', ['c', 'cpp'],
    version : '3.0.0-dev',
    license : 'GPL3',
    default_options : [
        # 'c_std=c23',
        'cpp_std=c++23',
        'buildtype=debug',
        'warning_level=3',
        'optimization=2',
        'prefix=/usr',
        'sysconfdir=/etc',
    ],
    meson_version : '>= 1.0.0'
)

bash = find_program('bash', required : true)
python = find_program('python3', required : true)

build_root = meson.project_build_root()
source_root = meson.project_source_root()

incdir = include_directories('src')

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
link_flags = []

debug_flags = [
    '-ggdb3',

    '-pedantic',
    '-pedantic-errors',
    # '-Wfatal-errors',
    # '-ftime-trace',
]

if get_option('buildtype') == 'debug'
    flags += cpp.get_supported_arguments(debug_flags)
endif

release_flags = []

if get_option('buildtype') == 'release'
    flags += cpp.get_supported_arguments(release_flags)
endif

# Warnings

default_warning_flags = [
    # '-Wconversion',
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
    '-Wno-format-nonliteral',

    # TODO
    # '-Wcast-qual',
    # '-Wconversion',

    # TODO - fix these warnings
    '-Wno-deprecated-declarations',
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

        # '-Weffc++',

        # Disabled Warnings
        '-Wno-array-bounds',
        '-Wno-stringop-overflow',
        '-Wno-switch-default',
        '-Wno-shadow',
        '-Wno-maybe-uninitialized',
        '-Wno-unused-result',
        '-Wno-cast-function-type',
    ]

    flags += cpp.get_supported_arguments(default_warning_flags_gcc)
endif

# Clang only warning flags
if cpp.get_id() == 'clang'
    default_warning_flags_clang = [
        '-Wint-conversion',

        # Disabled Warnings
    ]

    flags += cpp.get_supported_arguments(default_warning_flags_clang)
endif

# Codegen

default_codegen_flags = [
    '-fwrapv',
    # '-fwrapv-pointer',
    '-ftrivial-auto-var-init=zero',
]

flags += cpp.get_supported_arguments(default_codegen_flags)

# Linker

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
spdlog_dep = dependency('spdlog', required : true)
fmt_dep = dependency('fmt', required : true)
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

dependencies = [
    gtkmm_dep,
    glibmm_dep,
    giomm_dep,

    exo_dep,

    magic_enum_dep,
    toml11_dep,

    spdlog_dep,
    fmt_dep,
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

## preprocessor static

preprocessor = [
    '-DPACKAGE_NAME="spacefm"',
    '-DPACKAGE_NAME_FANCY="SpaceFM"',
    '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
    '-DPACKAGE_APPLICATION_NAME="com.thermitegod.spacefm"',
    '-DPACKAGE_BUGREPORT="TODO"',
    '-DPACKAGE_ONLINE_DOCS="TODO"',

    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_CHECKS',
    '-DG_LOG_DOMAIN="SpaceFM"',
    #'-DG_DEBUG=fatal_warnings',

    '-DMAGIC_ENUM_RANGE_MIN=0',
    '-DMAGIC_ENUM_RANGE_MAX=2000',
    # '-DMAGIC_ENUM_RANGE_MAX=32767',

    '-DLIBDIR_PATH="@0@"'.format(libdir),

    '-DGTK_DISABLE_DEPRECATED',
    '-DGDK_DISABLE_DEPRECATED',

    '-DGTKMM_DISABLE_DEPRECATED',
    '-DGDKMM_DISABLE_DEPRECATED',
    '-DGLIBMM_DISABLE_DEPRECATED',
    '-DGIOMM_DISABLE_DEPRECATED',

    # '-DZTD_LOGGER_USE_STD_FORMAT',
    '-DZTD_DISABLE_GLOBAL_LOG_MACROS',
    ]

foreach a : preprocessor
    add_project_arguments(a, language : ['c', 'cpp'])
endforeach

## preprocessor features
features = []

if get_option('deprecated')
    features += '-DHAVE_DEPRECATED'
endif

features += '-DZMQ_PORT="@0@"'.format(get_option('zmp_port'))

foreach a : features
    add_project_arguments(a, language : ['c', 'cpp'])
endforeach

## data paths

data_args = [
    '-DDATADIR="/usr/share"',
    '-DSYSCONFDIR="/etc"',
    '-DPACKAGE_SCRIPTS_PATH="/usr/share/spacefm/scripts"',
]

foreach a : data_args
  add_project_arguments(a, language : ['c', 'cpp'])
endforeach

## Source files

sources = files(
    'src/about.cxx',
    'src/autosave.cxx',
    'src/bookmarks.cxx',
    'src/file-search.cxx',
    'src/ipc.cxx',
    'src/ipc-command.cxx',
    'src/main.cxx',
    'src/main-window.cxx',
    'src/preference-dialog.cxx',
    'src/program-timer.cxx',
    'src/scripts.cxx',
    'src/settings.cxx',
    'src/single-instance.cxx',
    'src/terminal-handlers.cxx',
    'src/types.cxx',
    'src/utils.cxx',

    'src/compat/gtk4-porting.cxx',
    'src/compat/type-conversion.cxx',

    'src/commandline/commandline.cxx',
    'src/commandline/socket.cxx',
    'src/commandline/socket/subcommands.cxx',
    'src/commandline/socket/get/subcommands.cxx',
    'src/commandline/socket/get-task/subcommands.cxx',
    'src/commandline/socket/run-task/subcommands.cxx',
    'src/commandline/socket/set/subcommands.cxx',
    'src/commandline/socket/set-task/subcommands.cxx',

    'src/xset/xset.cxx',
    'src/xset/xset-context-menu.cxx',
    'src/xset/xset-custom.cxx',
    'src/xset/xset-defaults.cxx',
    'src/xset/xset-design.cxx',
    'src/xset/xset-design-clipboard.cxx',
    'src/xset/xset-dialog.cxx',
    'src/xset/xset-keyboard.cxx',
    'src/xset/xset-lookup.cxx',
    'src/xset/xset-misc.cxx',
    'src/xset/xset-toolbar.cxx',

    'src/settings/app.cxx',
    'src/settings/config-load.cxx',
    'src/settings/config-save.cxx',

    'src/settings/upgrade/config-upgrade.cxx',

    'src/utils/memory.cxx',

    'src/mime-type/mime-action.cxx',
    'src/mime-type/mime-cache.cxx',
    'src/mime-type/mime-type.cxx',

    'src/ptk/ptk-app-chooser.cxx',
    'src/ptk/ptk-archiver.cxx',
    'src/ptk/ptk-bookmark-view.cxx',
    'src/ptk/ptk-clipboard.cxx',
    'src/ptk/ptk-dialog.cxx',
    'src/ptk/ptk-dir-tree.cxx',
    'src/ptk/ptk-dir-tree-view.cxx',
    'src/ptk/ptk-file-actions-misc.cxx',
    'src/ptk/ptk-file-actions-open.cxx',
    'src/ptk/ptk-file-actions-rename.cxx',
    'src/ptk/ptk-file-browser.cxx',
    'src/ptk/ptk-file-list.cxx',
    'src/ptk/ptk-file-menu.cxx',
    'src/ptk/ptk-file-properties.cxx',
    'src/ptk/ptk-file-task.cxx',
    'src/ptk/ptk-keyboard.cxx',
    'src/ptk/ptk-location-view.cxx',
    'src/ptk/ptk-path-entry.cxx',
    'src/ptk/ptk-task-view.cxx',
    'src/ptk/ptk-utils.cxx',

    'src/ptk/natsort/strnatcmp.c',

    'src/vfs/vfs-app-desktop.cxx',
    'src/vfs/vfs-async-task.cxx',
    'src/vfs/vfs-async-thread.cxx',
    'src/vfs/vfs-device.cxx',
    'src/vfs/vfs-dir.cxx',
    'src/vfs/vfs-file.cxx',
    'src/vfs/vfs-file-task.cxx',
    'src/vfs/vfs-mime-type.cxx',
    'src/vfs/vfs-mime-monitor.cxx',
    'src/vfs/vfs-monitor.cxx',
    'src/vfs/vfs-thumbnailer.cxx',
    'src/vfs/vfs-time.cxx',
    'src/vfs/vfs-trash-can.cxx',
    'src/vfs/vfs-user-dirs.cxx',
    'src/vfs/vfs-utils.cxx',
    'src/vfs/vfs-volume.cxx',

    'src/vfs/libudevpp/udev.cxx',
    'src/vfs/libudevpp/udev_device.cxx',
    'src/vfs/libudevpp/udev_enumerate.cxx',
    'src/vfs/libudevpp/udev_monitor.cxx',

    'src/vfs/linux/procfs.cxx',
    'src/vfs/linux/sysfs.cxx',
)

target_name = 'spacefm'
target_type = 'executable'

spacefm = build_target(
    target_name,
    sources,
    target_type: target_type,
    include_directories: incdir,
    install : true,
    install_dir : bindir,
    dependencies: dependencies,
    cpp_pch: 'pch/cxx_pch.hxx',
)

## Install

install_subdir('data/applications', install_dir : datadir)
install_subdir('data/icons', install_dir : datadir)
install_subdir('data/scripts', install_dir : datadir / target_name)
