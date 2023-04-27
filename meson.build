project(
    'spacefm', ['c', 'cpp'],
    version : '3.0.0-dev',
    license : 'GPL3',
    default_options : [
        'cpp_std=c++20',
        'buildtype=debug',
        'warning_level=3',
        'optimization=2',
        'prefix=/usr',
        'sysconfdir=/etc',
    ],
    meson_version : '>= 1.0.0'
)

bash = find_program('bash', required : true)
fish = find_program('fish', required : true)
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
    '-Wno-format-nonliteral',

    # TODO
    # '-Wcast-qual',
    # '-Wconversion',
    # '-Wint-conversion',

    # TODO - fix these warnings
    '-Wno-deprecated-declarations',
    '-Wno-unused-result',

    # Sanitizer
    # '-fsanitize=undefined,address',
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

        # Ignored by clang
        # '-Weffc++',

        # Disabled Warnings

        # TODO
        '-Wno-switch-enum',
        '-Wno-switch-default',
        '-Wno-implicit-fallthrough',
        '-Wno-duplicated-branches',
        '-Wno-duplicated-cond',
        '-Wno-shadow',
        '-Wno-maybe-uninitialized',
        '-Wno-unused-result',

        '-Wno-restrict',
        '-Wno-catch-value',
        '-Wno-cast-function-type',
        '-Wno-unused-macros',

        '-Wno-type-limits',
        '-Wno-strict-aliasing',
    ]

    flags += cpp.get_supported_arguments(default_warning_flags_gcc)
endif

# Clang only warning flags
if cpp.get_id() == 'clang'
    default_warning_flags_clang = [
        # Disabled Warnings
        '-Wno-unreachable-code-loop-increment',
        '-Wno-unused-macros',
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

libudev_dep = dependency('libudev', required : true)
libffmpegthumbnailer_dep = dependency('libffmpegthumbnailer', required : true)
openssl_dep = dependency('openssl', required : true)

exo_dep = dependency('exo-2', required : true)
# x11_dep = dependency('x11', required : true)
spdlog_dep = dependency('spdlog', required : true)
fmt_dep = dependency('fmt', required : true)
sigc_dep = dependency('sigc++-3.0', required : true)
pugixml_dep = dependency('pugixml', required : true)

#gtk_dep = dependency('gtk4', required : true)
gtk_dep = dependency('gtk+-3.0', required : true)
glib_dep = dependency('glib-2.0', required : true)

#gtkmm_dep = dependency('gtkmm-4.0', required : true)
glibmm_dep = dependency('glibmm-2.68', required : true)
giomm_dep = dependency('giomm-2.68', required : true)

dependencies = [
    glibmm_dep,
    giomm_dep,

    glib_dep,
    gtk_dep,

    # x11_dep,
    exo_dep,

    magic_enum_dep,
    toml11_dep,

    spdlog_dep,
    fmt_dep,
    sigc_dep,
    pugixml_dep,

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
    '-DPACKAGE_BUGREPORT="TODO"',
    '-DPACKAGE_ONLINE_DOCS="TODO"',

    '-DFISH_PATH="@0@"'.format(fish.full_path()),

    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_CHECKS',
    '-DG_LOG_DOMAIN="SpaceFM"',
    #'-DG_DEBUG=fatal_warnings',

    '-DLIBDIR_PATH="@0@"'.format(libdir),

    '-DGTK_DISABLE_DEPRECATED',
    '-DGDK_DISABLE_DEPRECATED',

    '-DGTKMM_DISABLE_DEPRECATED',
    '-DGDKMM_DISABLE_DEPRECATED',
    '-DGLIBMM_DISABLE_DEPRECATED',
    '-DGIOMM_DISABLE_DEPRECATED',

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

if get_option('deprecated-ini-loading')
  features += '-DHAVE_DEPRECATED_INI_CONFIG_LOADING'
endif

if get_option('nonlatin')
  features += '-DHAVE_NONLATIN_KEYBOARD_SUPPORT'
endif

if get_option('ffmpegthumbnailer-cli')
  features += '-DHAVE_USE_FFMPEGTHUMBNAILER_CLI'
endif

foreach a : features
    add_project_arguments(a, language : ['c', 'cpp'])
endforeach

## data paths

data_args = [
    '-DDATADIR="/usr/share"',
    '-DSYSCONFDIR="/etc"',
    '-DPACKAGE_UI_PATH="/usr/share/spacefm/ui"',
    '-DPACKAGE_SCRIPTS_PATH="/usr/share/spacefm/scripts"',
    '-DFISH_FMLIB="/usr/share/spacefm/lib/fmlib.fish"',
]

foreach a : data_args
  add_project_arguments(a, language : ['c', 'cpp'])
endforeach

## Source files

sources = files(
    'src/autosave.cxx',
    'src/bookmarks.cxx',
    'src/find-files.cxx',
    'src/item-prop.cxx',
    'src/main.cxx',
    'src/main-window.cxx',
    'src/pref-dialog.cxx',
    'src/program-timer.cxx',
    'src/scripts.cxx',
    'src/settings.cxx',
    'src/socket.cxx',
    'src/terminal-handlers.cxx',
    'src/type-conversion.cxx',
    'src/types.cxx',
    'src/utils.cxx',
    'src/window-reference.cxx',

    'src/xset/xset.cxx',
    'src/xset/xset-context.cxx',
    'src/xset/xset-custom.cxx',
    'src/xset/xset-defaults.cxx',
    'src/xset/xset-design.cxx',
    'src/xset/xset-design-clipboard.cxx',
    'src/xset/xset-dialog.cxx',
    'src/xset/xset-event-handler.cxx',
    'src/xset/xset-lookup.cxx',
    'src/xset/xset-plugins.cxx',

    'src/settings/app.cxx',
    'src/settings/config-load.cxx',
    'src/settings/config-save.cxx',
    'src/settings/plugins-load.cxx',
    'src/settings/plugins-save.cxx',

    'src/mime-type/mime-action.cxx',
    'src/mime-type/mime-cache.cxx',
    'src/mime-type/mime-type.cxx',

    'src/ptk/ptk-app-chooser.cxx',
    'src/ptk/ptk-builder.cxx',
    'src/ptk/ptk-bookmark-view.cxx',
    'src/ptk/ptk-clipboard.cxx',
    'src/ptk/ptk-dir-tree.cxx',
    'src/ptk/ptk-dir-tree-view.cxx',
    'src/ptk/ptk-error.cxx',
    'src/ptk/ptk-file-actions-misc.cxx',
    'src/ptk/ptk-file-actions-open.cxx',
    'src/ptk/ptk-file-actions-rename.cxx',
    'src/ptk/ptk-file-archiver.cxx',
    'src/ptk/ptk-file-browser.cxx',
    'src/ptk/ptk-file-list.cxx',
    'src/ptk/ptk-file-menu.cxx',
    'src/ptk/ptk-file-properties.cxx',
    'src/ptk/ptk-file-task.cxx',
    'src/ptk/ptk-handler.cxx',
    'src/ptk/ptk-keyboard.cxx',
    'src/ptk/ptk-location-view.cxx',
    'src/ptk/ptk-path-entry.cxx',

    'src/vfs/vfs-app-desktop.cxx',
    'src/vfs/vfs-async-task.cxx',
    'src/vfs/vfs-dir.cxx',
    'src/vfs/vfs-file-info.cxx',
    'src/vfs/vfs-file-monitor.cxx',
    'src/vfs/vfs-file-task.cxx',
    'src/vfs/vfs-file-trash.cxx',
    'src/vfs/vfs-mime-type.cxx',
    'src/vfs/vfs-thumbnail-loader.cxx',
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
install_subdir('data/ui', install_dir : datadir / target_name)
install_subdir('data/icons', install_dir : datadir)
install_subdir('data/scripts', install_dir : datadir / target_name)
