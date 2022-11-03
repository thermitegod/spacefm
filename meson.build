project(
  'spacefm', ['c', 'cpp'],
  version : '3.0.0-dev',
  license : 'GPL3',
  default_options : [
    'cpp_std=c++20',
    'warning_level=3',
    'optimization=2',
    'prefix=/usr',
    'sysconfdir=/etc',
  ],
  meson_version : '>= 0.62.0'
)

result = [ 'Passed options:' ]

c = meson.get_compiler('c')
c_id = c.get_id()

cpp = meson.get_compiler('cpp')
cpp_id = cpp.get_id()

#message(c_id)
#message(cpp_id)

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
libdir = join_paths(prefix, get_option('libdir'), package)
sysconfdir = get_option('sysconfdir')
localedir = join_paths(prefix, get_option('localedir'))
mandir = join_paths(prefix, get_option('mandir'))

ldflags_add = []
cflags_add = []

ldflags_opt = []
cflags_opt = []

cflags_opt += [
  #[ '-ggdb' ],

  # enabled by warning_level=3
  #[ '-Wall' ],
  #[ '-Wextra' ],
  #[ '-Werror' ],

  # Not enabled by Wall
  [ '-finput-charset=UTF-8' ],

  #[ '-Weffc++' ],
  [ '-pedantic' ],
  [ '-pedantic-errors' ],
  # [ '-Wfatal-errors' ],
  #[ '-Waggregate-return' ],
  #[ '-Wcast-qual' ],
  [ '-Wchar-subscripts' ],
  [ '-Wcomment' ],
  #[ '-Wconversion' ],
  [ '-Wdisabled-optimization' ],
  [ '-Wdouble-promotion' ],
  [ '-Wfloat-equal' ],
  [ '-Wformat' ],
  [ '-Wformat=2' ],
  [ '-Wformat-nonliteral' ],
  [ '-Wformat-security' ],
  [ '-Wformat-y2k' ],
  [ '-Wimplicit-fallthrough' ],
  [ '-Wimport' ],
  #[ '-Wint-conversion' ],
  [ '-Winit-self' ],
  #[ '-Winline' ],
  #[ '-Wlong-long' ],
  #[ '-Wmissing-braces' ],
  [ '-Wmissing-field-initializers' ],
  [ '-Wmissing-format-attribute' ],
  [ '-Wmissing-include-dirs' ],
  [ '-Wmissing-noreturn' ],
  [ '-Wparentheses' ],
  [ '-Wpointer-arith' ],
  #[ '-Wredundant-decls' ],
  [ '-Wsequence-point' ],
  [ '-Wshadow' ],
  [ '-Wsign-compare' ],
  [ '-Wstack-protector' ],
  [ '-Wstrict-aliasing' ],
  [ '-Wstrict-aliasing=2' ],
  [ '-Wswitch' ],
  [ '-Wswitch-default' ],
  [ '-Wswitch-enum' ],
  [ '-Wtrigraphs' ],
  [ '-Wundef' ],
  [ '-Wuninitialized' ],
  [ '-Wunknown-pragmas' ],
  [ '-Wunreachable-code' ],
  [ '-Wunused' ],
  [ '-Wunused-function' ],
  [ '-Wunused-label' ],
  [ '-Wunused-parameter' ],
  [ '-Wunused-value' ],
  [ '-Wunused-result' ],
  [ '-Wunused-variable' ],
  [ '-Wunused-macros' ],
  [ '-Wvariadic-macros' ],
  [ '-Wvla' ],
  [ '-Wwrite-strings' ],
  [ '-Wignored-qualifiers' ],
  #[ '-Wstrict-overflow=5' ],
  [ '-Wstring-compare' ],
  [ '-Walloca' ],
  [ '-Wdate-time' ],

  # Disabled Warnings
  [ '-Wno-format-nonliteral' ],
  [ '-Wno-unused-parameter' ],

  # TODO - fix these warnings
  [ '-Wno-deprecated-declarations' ],

  # Sanitizer
  #[ '-fsanitize=undefined,address' ],
]

# GCC/Clang only warning flags
if cpp_id == 'gcc'
  cflags_opt+=[
    [ '-Wno-system-headers' ],

    [ '-Wformat-overflow=2' ],
    [ '-Wformat-truncation' ],
    #[ '-Wsuggest-attribute=pure' ],
    #[ '-Wsuggest-attribute=const' ],
    #[ '-Wsuggest-attribute=noreturn' ],
    #[ '-Wsuggest-attribute=malloc' ],
    #[ '-Wsuggest-attribute=format' ],
    #[ '-Wsuggest-attribute=cold' ],
    #[ '-Wmissing-format-attribute' ],
    [ '-Warray-bounds=2' ],
    [ '-Warray-compare' ],
    [ '-Wzero-length-bounds' ],
    [ '-Wattribute-alias=2' ],
    [ '-Wduplicated-branches' ],
    [ '-Wduplicated-cond' ],
    [ '-Wtrampolines' ],
    [ '-Walloc-zero' ],
    [ '-Wstringop-overflow=4' ],

    # Disabled Warnings
    [ '-Wno-switch-enum' ],          # TMP
    [ '-Wno-switch-default' ],       # TMP
    [ '-Wno-implicit-fallthrough' ], # TMP
    [ '-Wno-duplicated-branches' ],  # TMP
    [ '-Wno-shadow' ],              # TODO - fix this warning
    [ '-Wno-maybe-uninitialized' ], # TODO - fix this warning
    [ '-Wno-use-after-free' ],      # TODO - fix this warning
    [ '-Wno-stringop-overflow' ],   # TODO - fix this warning
    [ '-Wno-array-bounds' ],        # TODO - fix this warning
    [ '-Wno-restrict' ],
    [ '-Wno-catch-value' ],
    [ '-Wno-cast-function-type' ],
    [ '-Wno-unused-macros' ],

    [ '-Wno-type-limits' ],
    [ '-Wno-strict-aliasing' ],
  ]
elif cpp_id == 'clang'
  cflags_opt+=[
    # Disabled Warnings
    [ '-Wno-unreachable-code-loop-increment' ],
    [ '-Wno-unused-macros' ],
  ]
endif

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
data_args += '-DPACKAGE_SCRIPTS_PATH="/usr/share/spacefm/scripts"'

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

    '-DBASH_PATH="/bin/bash"',
    '-DSHELL_SETTINGS="set -o pipefail;shopt -s failglob"',

    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_CHECKS',
    '-DG_LOG_DOMAIN="SpaceFM"',
    #'-DG_DEBUG=fatal_warnings',

    '-DLIBDIR_PATH="@0@"'.format(libdir),

    '-DGTKMM_DISABLE_DEPRECATED',
    '-DGDKMM_DISABLE_DEPRECATED',
    '-DGLIBMM_DISABLE_DEPRECATED',
    '-DGIOMM_DISABLE_DEPRECATED',
    ]

if get_option('deprecated-ini-loading')
  pre_args += '-DHAVE_DEPRECATED_INI_LOADING'
endif

if get_option('deprecated-hw')
  pre_args += '-DHAVE_DEPRECATED_HW'
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
magic_enum_dep = dependency('magic_enum', required : true)
toml11_dep = dependency('toml11', required : true)

#gtk_dep = dependency('gtk+-4.0', required : true)
gtk_dep = dependency('gtk+-3.0', required : true)
glib_dep = dependency('glib-2.0', required : true)

#gtkmm_dep = dependency('gtkmm-4.0', required : true)
glibmm_dep = dependency('glibmm-2.68', required : true)
giomm_dep = dependency('giomm-2.68', required : true)


sources = [
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
  'src/type-conversion.cxx',
  'src/types.cxx',
  'src/utils.cxx',
  'src/window-reference.cxx',

  'src/xset.cxx',
  'src/xset-lookup.cxx',

  'src/settings/app.cxx',
  'src/settings/etc.cxx',
  'src/settings/load_etc.cxx',

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
  'src/ptk/ptk-utils.cxx',

  'src/vfs/vfs-app-desktop.cxx',
  'src/vfs/vfs-async-task.cxx',
  'src/vfs/vfs-dir.cxx',
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
    glibmm_dep,
    giomm_dep,

    glib_dep,
    gtk_dep,

    # x11_dep,
    exo_dep,

    spdlog_dep,
    fmt_dep,
    sigc_dep,
    magic_enum_dep,
    toml11_dep,

    libudev_dep,
    libffmpegthumbnailer_dep,

    libztd_dep,
  ],
  cpp_pch: 'pch/cxx_pch.hxx',
)

install_data('etc/spacefm.conf', install_dir : sysconfdir / 'spacefm')

install_subdir('data/applications', install_dir : datadir)
install_subdir('data/ui', install_dir : datadir / target_name)
install_subdir('data/icons', install_dir : datadir)

# SCRIPTS
install_subdir('scripts', install_dir : datadir / target_name / 'scripts')
