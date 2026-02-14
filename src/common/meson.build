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

if get_option('gtk3')
vfs_gtk3_sources = files(
    'vfs/app-desktop.cxx',
    'vfs/bookmarks.cxx',
    'vfs/clipboard.cxx',
    'vfs/device.cxx',
    'vfs/dir.cxx',
    'vfs/error.cxx',
    'vfs/execute.cxx',
    'vfs/file.cxx',
    'vfs/file-task.cxx',
    'vfs/mime-type.cxx',
    'vfs/mime-monitor.cxx',
    'vfs/terminals.cxx',
    'vfs/thumbnailer.cxx',
    'vfs/trash-can.cxx',
    'vfs/user-dirs.cxx',
    'vfs/volume.cxx',

    'vfs/utils/editor.cxx',
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
    'vfs/notify-cpp/event.cxx',
    'vfs/notify-cpp/file_system_event.cxx',
    'vfs/notify-cpp/notification.cxx',
    'vfs/notify-cpp/notify.cxx',
)

vfs_gtk3_dependencies = [
    gtk3_dependencies,

    logger_dep,
    libudev_dep,
    pugixml_dep,
    botan_dep,
]

vfs_gtk3_lib = static_library(
    'vfs_gtk3',
    vfs_gtk3_sources,
    include_directories : include_directories(['.', '../gtk3']),
    dependencies : vfs_gtk3_dependencies,
    install: false,
    cpp_pch: '../../pch/pch.hxx',
)

vfs_gtk3_dep = declare_dependency(
    link_with : vfs_gtk3_lib,
    include_directories : include_directories('.'),
    dependencies : vfs_gtk3_dependencies,
)
endif

vfs_gtk4_sources = files(
    'vfs/app-desktop.cxx',
    'vfs/bookmarks.cxx',
    # 'vfs/clipboard.cxx',
    'vfs/device.cxx',
    'vfs/dir.cxx',
    'vfs/error.cxx',
    'vfs/execute.cxx',
    'vfs/file.cxx',
    'vfs/mime-type.cxx',
    'vfs/mime-monitor.cxx',
    'vfs/terminals.cxx',
    'vfs/thumbnailer.cxx',
    'vfs/trash-can.cxx',
    'vfs/user-dirs.cxx',
    # 'vfs/volume.cxx', TODO

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
    'vfs/notify-cpp/event.cxx',
    'vfs/notify-cpp/file_system_event.cxx',
    'vfs/notify-cpp/notification.cxx',
    'vfs/notify-cpp/notify.cxx',

    'vfs/task/chmod.cxx',
    'vfs/task/chown.cxx',
    'vfs/task/copy.cxx',
    'vfs/task/create-directory.cxx',
    'vfs/task/create-file.cxx',
    'vfs/task/create-hardlink.cxx',
    'vfs/task/create-symlink.cxx',
    'vfs/task/move.cxx',
    'vfs/task/remove.cxx',
    'vfs/task/task.cxx',
)

vfs_gtk4_dependencies = [
    gtk4_dependencies,

    logger_dep,
    libudev_dep,
    pugixml_dep,
    botan_dep,
]

vfs_gtk4_lib = static_library(
    'vfs_gtk4',
    vfs_gtk4_sources,
    include_directories : include_directories('.'),
    dependencies : vfs_gtk4_dependencies,
    install: false,
    cpp_pch: '../../pch/pch.hxx',
)

vfs_gtk4_dep = declare_dependency(
    link_with : vfs_gtk4_lib,
    include_directories : include_directories('.'),
    dependencies : vfs_gtk4_dependencies,
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
