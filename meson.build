project(
  'spacefm', ['c', 'cpp'],
  version : '3.0.0-dev',
  license : 'GPL2 LGPL',
  default_options : [
    'cpp_std=c++20',
    'warning_level=3',
    'prefix=/usr',
    'sysconfdir=/etc',
  ],
  meson_version : '>= 0.62.0'
)

result = [ 'Passed options:' ]

c = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
c_id = c.get_id()
cross = meson.is_cross_build()
sh = find_program('sh')
sed = find_program('sed')
echo = find_program('echo')
incdir = include_directories('src')

builddir = meson.current_build_dir()
sourcedir = meson.current_source_dir()
package = meson.project_name()
version = meson.project_version()
prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
sysconfdir = get_option('sysconfdir')
localedir = join_paths(prefix, get_option('localedir'))
mandir = join_paths(prefix, get_option('mandir'))

ldflags_add = []
cflags_add = []

ldflags_opt = []
cflags_opt = []

cflags_opt += [
  [ '-Wimplicit-fallthrough' ],
  [ '-Wvla' ],
  [ '-fomit-frame-pointer' ],
  [ '-fno-common' ],
]
ldflags_opt += [
  [ '-Wl,-O2' ],
  [ '-Wl,--as-needed' ],
]

add_global_arguments(cflags_opt, language: ['c', 'cpp'])
add_global_link_arguments(ldflags_opt, language: ['c', 'cpp'])


# TODO data paths

data_args = []

data_args += '-DDATADIR="/usr/share"'
data_args += '-DSYSCONFDIR="/etc"'
data_args += '-DPACKAGE_UI_DIR="/usr/share/spacefm/ui"'

foreach a : data_args
  add_project_arguments(a, language : ['c', 'cpp'])
endforeach


# preprocessor related - start

pre_args = [
    '-DPACKAGE_NAME="spacefm"',
    '-DPACKAGE_NAME_FANCY="SpaceFM"',
    '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
    '-DPACKAGE_BUGREPORT="TODO"',
    '-DPACKAGE_ONLINE_DOCS="TODO"',
    '-DBASHPATH="/bin/bash"', # TODO - set in meson_options.txt
    '-DSHELL_SETTINGS="set -o pipefail;shopt -s failglob"',
    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_CHECKS',
    '-DG_LOG_DOMAIN="SpaceFM"',
    ]

if get_option('nonlatin')
  pre_args += '-DHAVE_NONLATIN'
endif

if get_option('sn')
  pre_args += '-DHAVE_SN'
endif

if get_option('deprecated-hw')
  pre_args += '-DHAVE_DEPRECATED_HW'
endif

if get_option('mmap')
  pre_args += '-DHAVE_MMAP'
endif

if get_option('xxhash')
  pre_args += '-DUSE_XXHASH'
  lib_xxhash = meson.get_compiler('c').find_library('xxhash', required : false)
else
  lib_xxhash = meson.get_compiler('c').find_library('xxhash', required : false)
endif

foreach a : pre_args
  add_project_arguments(a, language : ['c', 'cpp'])
endforeach

# preprocessor related - end

lib_math = meson.get_compiler('c').find_library('libm', required : true)
lib_libbsd = meson.get_compiler('c').find_library('libbsd', required : true)
lib_udev = meson.get_compiler('c').find_library('libudev', required : true)
lib_ffmpeg = meson.get_compiler('c').find_library('libffmpegthumbnailer', required : true)

dep_exo = dependency('exo-2')
dep_x11 = dependency('x11')
dep_glib = dependency('glib-2.0')
dep_gtk = dependency('gtk+-3.0')
dep_spdlog = dependency('spdlog')


sources = [
  'src/autosave.cxx',
  'src/find-files.cxx',
  'src/item-prop.cxx',
  'src/logger.cxx',
  'src/main.cxx',
  'src/main-window.cxx',
  'src/pref-dialog.cxx',
  'src/settings.cxx',
  'src/utils.cxx',
  'src/window-reference.cxx',

  'src/mime-type/mime-action.cxx',
  'src/mime-type/mime-cache.cxx',
  'src/mime-type/mime-type.cxx',

  'src/ptk/ptk-app-chooser.cxx',
  'src/ptk/ptk-clipboard.cxx',
  'src/ptk/ptk-dir-tree.cxx',
  'src/ptk/ptk-dir-tree-view.cxx',
  'src/ptk/ptk-file-archiver.cxx',
  'src/ptk/ptk-file-browser.cxx',
  'src/ptk/ptk-file-list.cxx',
  'src/ptk/ptk-file-menu.cxx',
  'src/ptk/ptk-file-misc.cxx',
  'src/ptk/ptk-file-properties.cxx',
  'src/ptk/ptk-file-task.cxx',
  'src/ptk/ptk-handler.cxx',
  'src/ptk/ptk-location-view.cxx',
  'src/ptk/ptk-path-entry.cxx',
  'src/ptk/ptk-utils.cxx',

  'src/vfs/vfs-app-desktop.cxx',
  'src/vfs/vfs-async-task.cxx',
  'src/vfs/vfs-dir.cxx',
  'src/vfs/vfs-execute.cxx',
  'src/vfs/vfs-file-info.cxx',
  'src/vfs/vfs-file-monitor.cxx',
  'src/vfs/vfs-file-task.cxx',
  'src/vfs/vfs-mime-type.cxx',
  'src/vfs/vfs-thumbnail-loader.cxx',
  'src/vfs/vfs-utils.cxx',
  'src/vfs/vfs-volume.cxx',
]

target_name = 'spacefm'
target_type = 'executable'

spacefm = build_target(
  target_name,
  sources,
  target_type: target_type,
  include_directories: incdir,
  install : true,
  install_dir : bindir,
  dependencies: [
    dep_glib,
    dep_gtk,
    dep_x11,
    dep_exo,
    dep_spdlog,
    lib_math,
    lib_libbsd,
    lib_udev,
    lib_ffmpeg,
    lib_xxhash,
  ],
  # c_pch:   'src/pch/c_pch.h',
  cpp_pch: 'src/pch/cxx_pch.hxx',
)

install_data('src/spacefm-auth', install_dir : bindir)
install_data('etc/spacefm.conf', install_dir : sysconfdir / 'spacefm')

install_subdir('data/applications', install_dir : datadir)
install_subdir('data/ui', install_dir : datadir / target_name)
install_subdir('data/icons', install_dir : datadir)
