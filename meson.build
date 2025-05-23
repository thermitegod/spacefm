project(
    'spacefm',
    ['c', 'cpp'],
    version: '3.0.0-dev',
    license: 'GPL3',
    default_options: [
        'c_std=c23',
        'cpp_std=c++23',
        'buildtype=debugoptimized',
        'warning_level=3',
        'optimization=2',
        'prefix=/usr',
        'sysconfdir=/etc',
    ],
    meson_version: '>= 1.0.0',
)

bash = find_program('bash', required: true)
python = find_program('python3', required: true)

build_root = meson.project_build_root()
source_root = meson.project_source_root()

incdir = []
incdir += include_directories('src')

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
        '-Wno-cast-function-type',
        '-Wno-cast-qual',
        '-Wno-maybe-uninitialized',
        '-Wno-shadow',  # gcc is a lot stricter than clang
    ])
endif

# Disabled Clang Warnings
if cpp.get_id() == 'clang'
    flags += cpp.get_supported_arguments([])
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

libudev_dep = dependency('libudev', required: true)
botan_dep = dependency('botan-3', required: true)

sigc_dep = dependency('sigc++-3.0', required: true)
pugixml_dep = dependency('pugixml', required: true)

# Gtk3 CPP
gtkmm3_dep = dependency('gtkmm-3.0', required: true)
glibmm3_dep = dependency('glibmm-2.4', required: true)
giomm3_dep = dependency('giomm-2.4', required: true)

# Gtk4 CPP
gtkmm4_dep = dependency('gtkmm-4.0', required: true)
glibmm4_dep = dependency('glibmm-2.68', required: true)
giomm4_dep = dependency('giomm-2.68', required: true)

dependencies = [
    gtkmm3_dep,
    glibmm3_dep,
    giomm3_dep,
    # gtkmm4_dep,
    # glibmm4_dep,
    # giomm4_dep,
    sigc_dep,

    libudev_dep,
    pugixml_dep,
    botan_dep,
]

## Source files
sources = files()

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
    '-DDIALOG_BUILD_ROOT="@0@/dialog"'.format(build_root),

    # data paths
    '-DPACKAGE_IMAGES="/usr/share/spacefm/images"',
    '-DPACKAGE_IMAGES_LOCAL="@0@"'.format(join_paths(meson.current_source_dir(), 'data/images')),
    '-DDEV_SCRIPTS_PATH="@0@"'.format(join_paths(meson.current_source_dir(), 'data/scripts')),
]

if get_option('dev')
    preprocessor += [
        # GTK
        '-DGTK_DISABLE_DEPRECATED',
        '-DGDK_DISABLE_DEPRECATED',
        '-DG_DISABLE_DEPRECATED',
        # GTKMM
        '-DGTKMM_DISABLE_DEPRECATED',
        '-DGDKMM_DISABLE_DEPRECATED',
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

if get_option('exo')
    exo_dep = dependency('exo-2', required: true)
    dependencies += exo_dep
    features += '-DUSE_EXO'
endif

if get_option('with-system-cli11')
    cli11_dep = dependency('CLI11', required: true)
    dependencies += cli11_dep
else
    cli11_dep_proj = subproject('CLI11', required: true)
    cli11_dep = cli11_dep_proj.get_variable('CLI11_dep')
    dependencies += cli11_dep
endif

if get_option('with-system-concurrencpp')
    concurrencpp_dep = dependency('concurrencpp', required: true)
    dependencies += concurrencpp_dep
else
    cmake = import('cmake')
    concurrencpp_proj = cmake.subproject('concurrencpp', required: true)
    concurrencpp_dep = concurrencpp_proj.dependency('concurrencpp')
    dependencies += concurrencpp_dep
endif

if get_option('with-system-glaze')
    glaze_dep = dependency('glaze', required: true)
    dependencies += glaze_dep
else
    incdir += include_directories('subprojects/glaze/include')
endif

if get_option('with-system-magic-enum')
    magic_enum_dep = dependency('magic_enum', required: true, version: '>=0.9.7')
    dependencies += magic_enum_dep
else
    # TODO this is still building tests
    magic_enum_proj = subproject('magic_enum', required: true, default_options: ['test=false'])
    magic_enum_dep = magic_enum_proj.get_variable('magic_enum_dep')
    dependencies += magic_enum_dep
endif

if get_option('with-system-spdlog')
    spdlog_dep = dependency('spdlog', required: true)
    dependencies += spdlog_dep
else
    cmake = import('cmake')
    # use std::format over bundled fmt::format
    cmake_opt_var = cmake.subproject_options()
    cmake_opt_var.add_cmake_defines({'SPDLOG_USE_STD_FORMAT': '1'})
    spdlog_proj = cmake.subproject('spdlog', required: true, options: cmake_opt_var)
    spdlog_dep = spdlog_proj.dependency('spdlog')
    dependencies += spdlog_dep
endif

if get_option('with-system-ztd')
    ztd_dep = dependency('ztd', required: true)
    dependencies += ztd_dep
else
    ztd_proj = subproject('ztd', required: true)
    ztd_dep = ztd_proj.get_variable('ztd_dep')
    dependencies += ztd_dep
endif

if get_option('socket')
    cppzmq_dep = dependency('cppzmq', required: true)

    dependencies += [
        cppzmq_dep,
    ]

    features += '-DHAVE_SOCKET'
    features += '-DSOCKET_PORT="@0@"'.format(get_option('zmp_port'))

    sources += files(
        'src/commandline/socket.cxx',
        'src/commandline/socket/get-task/subcommands.cxx',
        'src/commandline/socket/get/subcommands.cxx',
        'src/commandline/socket/run-task/subcommands.cxx',
        'src/commandline/socket/set-task/subcommands.cxx',
        'src/commandline/socket/set/subcommands.cxx',
        'src/commandline/socket/subcommands.cxx',
        'src/socket/commands.cxx',
        'src/socket/server.cxx',
    )
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

    dependencies += media_dependencies
    features += '-DHAVE_MEDIA'
endif

foreach a: features
    add_project_arguments(a, language: ['c', 'cpp'])
endforeach

## Required Base Source files

sources += files(
    'src/about.cxx',
    'src/autosave.cxx',
    'src/bookmarks.cxx',
    'src/concurrency.cxx',
    'src/file-search.cxx',
    'src/keybindings-dialog.cxx',
    'src/logger.cxx',
    'src/main.cxx',
    'src/main-window.cxx',
    'src/preference-dialog.cxx',
    'src/settings.cxx',
    'src/single-instance.cxx',
    'src/types.cxx',

    'src/compat/gtk4-porting.cxx',

    'src/commandline/commandline.cxx',

    'src/xset/xset.cxx',
    'src/xset/xset-context-menu.cxx',
    'src/xset/xset-defaults.cxx',
    'src/xset/xset-lookup.cxx',

    'src/xset/utils/xset-utils.cxx',

    'src/settings/config-load.cxx',
    'src/settings/config-save.cxx',
    'src/settings/settings.cxx',

    'src/settings/upgrade/config-upgrade.cxx',

    'src/utils/permissions.cxx',
    'src/utils/shell-quote.cxx',
    'src/utils/strdup.cxx',

    'src/ptk/ptk-app-chooser.cxx',
    'src/ptk/ptk-archiver.cxx',
    'src/ptk/ptk-bookmark-view.cxx',
    'src/ptk/ptk-clipboard.cxx',
    'src/ptk/ptk-dialog.cxx',
    'src/ptk/ptk-dir-tree.cxx',
    'src/ptk/ptk-dir-tree-view.cxx',
    'src/ptk/ptk-file-action-create.cxx',
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

    'src/ptk/utils/ptk-utils.cxx',
    'src/ptk/utils/history.cxx',

    'src/ptk/natsort/strnatcmp.cxx',

    'src/vfs/vfs-app-desktop.cxx',
    'src/vfs/vfs-device.cxx',
    'src/vfs/vfs-dir.cxx',
    'src/vfs/vfs-error.cxx',
    'src/vfs/vfs-file.cxx',
    'src/vfs/vfs-file-task.cxx',
    'src/vfs/vfs-mime-type.cxx',
    'src/vfs/vfs-mime-monitor.cxx',
    'src/vfs/vfs-monitor.cxx',
    'src/vfs/vfs-terminals.cxx',
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

spacefm = build_target(
    'spacefm',
    sources,
    target_type: 'executable',
    include_directories: incdir,
    install: true,
    install_dir: bindir,
    dependencies: dependencies,
    cpp_pch: 'pch/pch.hxx',
)

## Standalone Dialogs

subdir('dialog')

## Tests

if get_option('tests')
    subdir('tests')
endif

## Install

install_subdir('data/applications', install_dir: datadir)
install_subdir('data/icons', install_dir: datadir)
