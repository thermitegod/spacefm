project(
  'spacefm', ['c', 'cpp'],
  version : '3.0.0-dev',
  license : 'GPL2 LGPL',
  default_options : [
      'warning_level=3',
      'prefix=/usr',
      'sysconfdir=/etc',
  ],
  meson_version : '>= 0.49'
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

data_args += '-DHTMLDIR="/usr/share"'
data_args += '-DDATADIR="/usr/share"'
data_args += '-DSYSCONFDIR="/etc"'
data_args += '-DPACKAGE_UI_DIR="/usr/share/spacefm/ui"'

#htmldir_path = get_option('htmldir')
#if htmldir_path == ''
#  htmldir_path = join_paths(get_option('libdir'), 'dri')
#endif

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

if get_option('nls')
  pre_args += '-DHAVE_NLS'
endif

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


sources = [
  'src/autosave.c',
  'src/find-files.c',
  'src/go-dialog.c',
  'src/item-prop.c',
  'src/main.c',
  'src/main-window.c',
  'src/pref-dialog.c',
  'src/settings.c',
  'src/spacefm.c',
  'src/utils.c',
  'src/mime-type/mime-action.c',
  'src/mime-type/mime-cache.c',
  'src/mime-type/mime-type.c',
  'src/ptk/ptk-app-chooser.c',
  'src/ptk/ptk-clipboard.c',
  'src/ptk/ptk-dir-tree.c',
  'src/ptk/ptk-dir-tree-view.c',
  'src/ptk/ptk-file-archiver.c',
  'src/ptk/ptk-file-browser.c',
  'src/ptk/ptk-file-list.c',
  'src/ptk/ptk-file-menu.c',
  'src/ptk/ptk-file-misc.c',
  'src/ptk/ptk-file-properties.c',
  'src/ptk/ptk-file-task.c',
  'src/ptk/ptk-handler.c',
  'src/ptk/ptk-location-view.c',
  'src/ptk/ptk-path-entry.c',
  'src/ptk/ptk-utils.c',
  'src/vfs/vfs-app-desktop.c',
  'src/vfs/vfs-async-task.c',
  'src/vfs/vfs-dir.c',
  'src/vfs/vfs-execute.c',
  'src/vfs/vfs-file-info.c',
  'src/vfs/vfs-file-monitor.c',
  'src/vfs/vfs-file-task.c',
  'src/vfs/vfs-mime-type.c',
  'src/vfs/vfs-thumbnail-loader.c',
  'src/vfs/vfs-utils.c',
  'src/vfs/vfs-volume.c',
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
    lib_math,
    lib_libbsd,
    lib_udev,
    lib_ffmpeg,
    lib_xxhash,
  ],
  c_pch:   'src/pch/c_pch.h',
  # cpp_pch: 'src/pch/cxx_pch.hxx',
)

install_data('src/spacefm-auth', install_dir : bindir)
install_data('etc/spacefm.conf', install_dir : sysconfdir / 'spacefm')

install_subdir('data/applications', install_dir : datadir)
install_subdir('data/ui', install_dir : datadir / target_name)
install_subdir('data/icons', install_dir : datadir)
