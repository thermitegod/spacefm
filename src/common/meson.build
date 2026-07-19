# logger static library

spdlog_dep = dependency('spdlog', required: true)

logger_lib = static_library(
    'logger',
    files('logger.cxx'),
    include_directories : include_directories('.'),
    dependencies : spdlog_dep,
    install: false,
)

logger_dep = declare_dependency(
    link_with : logger_lib,
    include_directories : include_directories('.'),
    dependencies : spdlog_dep,
)

# vfs static library

vfs_sources = files(
    'vfs/app-desktop.cxx',
    'vfs/bookmarks.cxx',
    'vfs/device.cxx',
    'vfs/dir.cxx',
    'vfs/error.cxx',
    'vfs/file.cxx',
    'vfs/mime-type.cxx',
    'vfs/mime-monitor.cxx',
    'vfs/terminals.cxx',
    'vfs/task-manager.cxx',
    'vfs/thumbnailer.cxx',
    'vfs/trash-can.cxx',
    'vfs/user-dirs.cxx',
    'vfs/volume-manager.cxx',

    # 'vfs/utils/editor.cxx', # TODO move to gui/utils
    'vfs/utils/icon.cxx',
    'vfs/utils/permissions.cxx',
    'vfs/utils/utils.cxx',

    'vfs/thumbnails/thumbnails.cxx',

    'vfs/libudevpp/udev.cxx',
    'vfs/libudevpp/udev_device.cxx',
    'vfs/libudevpp/udev_enumerate.cxx',
    'vfs/libudevpp/udev_monitor.cxx',

    'vfs/linux/procfs.cxx',
    'vfs/linux/self.cxx',
    'vfs/linux/sysfs.cxx',

    'vfs/mime-type/mime-action.cxx',
    'vfs/mime-type/mime-type.cxx',
    'vfs/mime-type/chrome/mime-utils.cxx',

    'vfs/notify-cpp/controller.cxx',
    'vfs/notify-cpp/notify.cxx',
)

vfs_dependencies = [
    gtk_dependencies,

    logger_dep,
    libudev_dep,
    pugixml_dep,
    botan_dep,
]

vfs_lib = static_library(
    'vfs',
    vfs_sources,
    include_directories : include_directories('.'),
    dependencies : vfs_dependencies,
    install: false,
    cpp_pch: '../../pch/pch.hxx',
)

vfs_dep = declare_dependency(
    link_with : vfs_lib,
    include_directories : include_directories('.'),
    dependencies : vfs_dependencies,
)

# natsort static library

natsort_sources = files(
    'natsort/strnatcmp.cxx',
)

natsort_lib = static_library(
    'natsort',
    natsort_sources,
    include_directories : include_directories('.'),
    install: false,
)

natsort_dep = declare_dependency(
    link_with : natsort_lib,
    include_directories : include_directories('.'),
)
