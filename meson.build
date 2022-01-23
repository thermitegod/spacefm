project(
  'spacefm', ['c', 'cpp'],
  version : '3.0.0-dev',
  license : 'GPL3',
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
  #[ '-ggdb' ],

  #[ '-Wall' ],
  #[ '-Wextra' ],
  #[ '-Wshadow' ],
  #[ '-Wconversion' ],
  #[ '-fsanitize=undefined,address' ],

  [ '-Wno-deprecated-declarations' ],
  [ '-Wno-unused-variable' ],
  [ '-Wno-format-pedantic' ],

  #[ '-Wstring-conversion' ],
  #[ '-Wint-conversion' ],
  [ '-Wimplicit-fallthrough' ],
  [ '-Wvla' ],
  #[ '-fomit-frame-pointer' ],
  #[ '-fno-common' ],
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
    #'-DG_DEBUG=fatal_warnings'
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

libztd_dep = dependency('ztd', required : true)

libudev_dep = dependency('libudev', required : true)
libffmpegthumbnailer_dep = dependency('libffmpegthumbnailer', required : true)

exo_dep = dependency('exo-2', required : true)
x11_dep = dependency('x11', required : true)
spdlog_dep = dependency('spdlog', required : true)
fmt_dep = dependency('fmt', required : true)
sigc_dep = dependency('sigc++-3.0', required : true)

gtk_dep = dependency('gtk+-3.0', required : true)
glib_dep = dependency('glib-2.0', required : true)


sources = [
  'src/autosave.cxx',
  'src/find-files.cxx',
  'src/item-prop.cxx',
  'src/main.cxx',
  'src/main-window.cxx',
  'src/pref-dialog.cxx',
  'src/settings.cxx',
  'src/socket.cxx',
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
  'src/vfs/vfs-file-trash.cxx',
  'src/vfs/vfs-mime-type.cxx',
  'src/vfs/vfs-thumbnail-loader.cxx',
  'src/vfs/vfs-user-dir.cxx',
  'src/vfs/vfs-utils.cxx',
  'src/vfs/vfs-volume.cxx',
]

# Git current commit
git_version_h = vcs_tag(
  command : ['git', 'describe', '--tags'],
  input : 'src/git-version.h.in',
  output :'git-version.h',
  replace_string:'@GIT_VERSION@',
)

target_name = 'spacefm'
target_type = 'executable'

spacefm = build_target(
  target_name,
  sources,
  git_version_h,
  target_type: target_type,
  include_directories: incdir,
  install : true,
  install_dir : bindir,
  dependencies: [
    glib_dep,
    gtk_dep,

    x11_dep,
    exo_dep,

    lib_xxhash,

    spdlog_dep,
    fmt_dep,
    sigc_dep,

    libudev_dep,
    libffmpegthumbnailer_dep,

    libztd_dep,
  ],
  cpp_pch: 'pch/cxx_pch.hxx',
)

install_data('scripts/spacefm-auth', install_dir : bindir)
install_data('etc/spacefm.conf', install_dir : sysconfdir / 'spacefm')

install_subdir('data/applications', install_dir : datadir)
install_subdir('data/ui', install_dir : datadir / target_name)
install_subdir('data/icons', install_dir : datadir)
