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
  # '-fsave-optimization-record',
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
  '-Wcast-qual',
  '-Wchar-subscripts',
  '-Wcomment',
  '-Wdisabled-optimization',
  # '-Wdouble-promotion', # warns when using NAN
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
  '-Wconversion',
  '-Wno-sign-conversion', # lots of errors in glibmm
  '-Wno-shorten-64-to-32',

  # '-Wno-deprecated-declarations',
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
    '-Wno-shadow', # gcc is a lot stricter than clang
    '-Wno-maybe-uninitialized',
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

## system dependencies

# magic_enum use system or subproject
magic_enum_dep = dependency('magic_enum', required : false)
if not magic_enum_dep.found() # using subproject
  magic_enum_proj = subproject('magic_enum', required : true)
  magic_enum_dep = magic_enum_proj.get_variable('magic_enum_dep')
endif

# CLI11 use system or subproject
cli11_dep = dependency('CLI11', required : false)
if not cli11_dep.found() # using subproject
  cli11_dep_proj = subproject('cli11', required : true)
  cli11_dep_dep = cli11_dep_proj.get_variable('CLI11_dep')
endif

# concurrencpp use system or subproject
concurrencpp_dep = dependency('concurrencpp', required : false)
if not concurrencpp_dep.found() # using subproject
  cmake = import('cmake')
  concurrencpp_proj = cmake.subproject('concurrencpp', required : true)
  concurrencpp_dep = concurrencpp_proj.dependency('concurrencpp')
endif

libudev_dep = dependency('libudev', required : true)
libffmpegthumbnailer_dep = dependency('libffmpegthumbnailer', required : true)
openssl_dep = dependency('openssl', required : true)

spdlog_dep = dependency('spdlog', required : true)
sigc_dep = dependency('sigc++-3.0', required : true)
pugixml_dep = dependency('pugixml', required : true)

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

  magic_enum_dep,

  spdlog_dep,
  sigc_dep,
  pugixml_dep,
  cli11_dep,
  concurrencpp_dep,

  libudev_dep,
  libffmpegthumbnailer_dep,
  openssl_dep,
]

## Source files
sources = files()

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
]

foreach a : preprocessor
  add_project_arguments(a, language : ['c', 'cpp'])
endforeach

## preprocessor features
features = []

if get_option('deprecated')
  features += '-DHAVE_DEPRECATED'
endif

if get_option('exo')
  exo_dep = dependency('exo-2', required : true)
  dependencies += exo_dep
  features += '-DUSE_EXO'
endif

if get_option('with-system-toml11')
  toml11_dep = dependency('toml11', required : true)
  dependencies += toml11_dep
else
  # using subproject
  cmake = import('cmake')
  # Hard requires having a cpp standard set
  cmake_opt_var = cmake.subproject_options()
  cmake_opt_var.add_cmake_defines({'CMAKE_CXX_STANDARD': '23'})
  toml11_proj = cmake.subproject('toml11', required : true, options: cmake_opt_var)
  toml11_dep = toml11_proj.dependency('toml11')
  dependencies += toml11_dep
endif

if get_option('with-system-ztd')
  ztd_dep = dependency('ztd', required : true)
  dependencies += ztd_dep
else
  # using subproject
  ztd_proj = subproject('ztd', required : true)
  ztd_dep = ztd_proj.get_variable('ztd_dep')
  dependencies += ztd_dep
endif

if get_option('socket')
  nlohmann_json_dep = dependency('nlohmann_json', required : true)

  # zmqpp does not provide a pkgconfig file
  libzmq_dep = dependency('libzmq', required : true)
  libzmqpp_dep = cc.find_library('zmqpp', has_headers: ['zmqpp/zmqpp.hpp'], required: true)

  dependencies += [
    nlohmann_json_dep,

    libzmq_dep,
    libzmqpp_dep,
  ]

  features += '-DHAVE_SOCKET'
  features += '-DSOCKET_PORT="@0@"'.format(get_option('zmp_port'))

  sources += files(
    'src/commandline/socket.cxx',
    'src/commandline/socket/subcommands.cxx',
    'src/commandline/socket/get/subcommands.cxx',
    'src/commandline/socket/get-task/subcommands.cxx',
    'src/commandline/socket/run-task/subcommands.cxx',
    'src/commandline/socket/set/subcommands.cxx',
    'src/commandline/socket/set-task/subcommands.cxx',

    'src/socket/commands.cxx',
    'src/socket/server.cxx',
  )
endif

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

  dependencies += [
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

  sources += files(
    'src/vfs/media/audio-video-metadata.cxx',
    'src/vfs/media/image-metadata.cxx',
  )
endif

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

## Required Base Source files

sources += files(
  'src/about.cxx',
  'src/autosave.cxx',
  'src/bookmarks.cxx',
  'src/concurrency.cxx',
  'src/file-search.cxx',
  'src/keybindings-dialog.cxx',
  'src/main.cxx',
  'src/main-window.cxx',
  'src/preference-dialog.cxx',
  'src/scripts.cxx',
  'src/settings.cxx',
  'src/single-instance.cxx',
  'src/terminal-handlers.cxx',
  'src/types.cxx',

  'src/compat/gtk4-porting.cxx',
  'src/compat/type-conversion.cxx',

  'src/commandline/commandline.cxx',

  'src/xset/xset.cxx',
  'src/xset/xset-context-menu.cxx',
  'src/xset/xset-defaults.cxx',
  'src/xset/xset-dialog.cxx',
  'src/xset/xset-keyboard.cxx',
  'src/xset/xset-lookup.cxx',

  'src/xset/utils/xset-utils.cxx',

  'src/settings/config-load.cxx',
  'src/settings/config-save.cxx',
  'src/settings/settings.cxx',

  'src/settings/upgrade/config-upgrade.cxx',

  'src/utils/memory.cxx',
  'src/utils/shell-quote.cxx',
  'src/utils/strdup.cxx',
  'src/utils/misc.cxx',

  'src/ptk/ptk-app-chooser.cxx',
  'src/ptk/ptk-archiver.cxx',
  'src/ptk/ptk-bookmark-view.cxx',
  'src/ptk/ptk-clipboard.cxx',
  'src/ptk/ptk-dialog.cxx',
  'src/ptk/ptk-dir-tree.cxx',
  'src/ptk/ptk-dir-tree-view.cxx',
  'src/ptk/ptk-file-action-misc.cxx',
  'src/ptk/ptk-file-action-open.cxx',
  'src/ptk/ptk-file-action-paste.cxx',
  'src/ptk/ptk-file-action-rename.cxx',
  'src/ptk/ptk-file-browser.cxx',
  'src/ptk/ptk-file-list.cxx',
  'src/ptk/ptk-file-menu.cxx',
  'src/ptk/ptk-file-properties.cxx',
  'src/ptk/ptk-file-task.cxx',
  'src/ptk/ptk-file-task-view.cxx',
  'src/ptk/ptk-location-view.cxx',
  'src/ptk/ptk-path-bar.cxx',
  'src/ptk/ptk-search-bar.cxx',

  'src/ptk/utils/multi-input.cxx',
  'src/ptk/utils/ptk-utils.cxx',

  'src/ptk/natsort/strnatcmp.cxx',

  'src/ptk/deprecated/async-task.cxx',

  'src/vfs/vfs-app-desktop.cxx',
  'src/vfs/vfs-device.cxx',
  'src/vfs/vfs-dir.cxx',
  'src/vfs/vfs-file.cxx',
  'src/vfs/vfs-file-task.cxx',
  'src/vfs/vfs-mime-type.cxx',
  'src/vfs/vfs-mime-monitor.cxx',
  'src/vfs/vfs-monitor.cxx',
  'src/vfs/vfs-thumbnailer.cxx',
  'src/vfs/vfs-trash-can.cxx',
  'src/vfs/vfs-user-dirs.cxx',
  'src/vfs/vfs-volume.cxx',

  'src/vfs/utils/vfs-editor.cxx',
  'src/vfs/utils/vfs-utils.cxx',

  'src/vfs/thumbnails/thumbnails.cxx',

  'src/vfs/libudevpp/udev.cxx',
  'src/vfs/libudevpp/udev_device.cxx',
  'src/vfs/libudevpp/udev_enumerate.cxx',
  'src/vfs/libudevpp/udev_monitor.cxx',

  'src/vfs/linux/procfs.cxx',
  'src/vfs/linux/self.cxx',
  'src/vfs/linux/sysfs.cxx',

  'src/vfs/mime-type/mime-action.cxx',
  'src/vfs/mime-type/mime-type.cxx',
  'src/vfs/mime-type/chrome/mime-utils.cxx',
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
