# logger static library

spdlog_dep = dependency('spdlog', required: true)

logger_lib = static_library(
    'logger',
    files('logger/logger.cxx'),
    include_directories: include_directories('./logger'),
    dependencies: spdlog_dep,
    install: false,
)

logger_dep = declare_dependency(
    link_with: logger_lib,
    include_directories: include_directories('./logger'),
    dependencies: spdlog_dep,
)

# natsort static library

natsort_lib = static_library(
    'natsort',
    files('natsort/natsort.cxx'),
    include_directories: include_directories('.'),
    install: false,
)

natsort_dep = declare_dependency(
    link_with: natsort_lib,
    include_directories: include_directories('.'),
)

# glycin wrapper static library

glycin_wrapper_lib = static_library(
    'glycin_wrapper',
    files('glycin/glycin.cxx'),
    include_directories: include_directories('.'),
    dependencies: gtk_dependencies,
    install: false,
)

glycin_wrapper_dep = declare_dependency(
    link_with: glycin_wrapper_lib,
    include_directories: include_directories('.'),
    dependencies: gtk_dependencies,
)

# libudevpp static library

libudevpp_lib = static_library(
    'libudevpp',
    files(
        'libudevpp/udev.cxx',
        'libudevpp/udev_device.cxx',
        'libudevpp/udev_enumerate.cxx',
        'libudevpp/udev_monitor.cxx',
    ),
    include_directories: include_directories('.'),
    dependencies: libudev_dep,
    install: false,
)

libudevpp_dep = declare_dependency(
    link_with: libudevpp_lib,
    include_directories: include_directories('.'),
    dependencies: libudev_dep,
)

# notify static library

notify_lib = static_library(
    'notify',
    files(
        'notify-cpp/controller.cxx',
        'notify-cpp/notify.cxx',
    ),
    include_directories: include_directories('.'),
    dependencies: sigc_dep,
    install: false,
)

notify_dep = declare_dependency(
    link_with: notify_lib,
    include_directories: include_directories('.'),
    dependencies: sigc_dep,
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
    'vfs/thumbnail_manager.cxx',
    'vfs/trash-can.cxx',
    'vfs/user-dirs.cxx',
    'vfs/volume-manager.cxx',

    # 'vfs/utils/editor.cxx', # TODO move to gui/utils
    'vfs/utils/icon.cxx',
    'vfs/utils/permissions.cxx',
    'vfs/utils/utils.cxx',

    'vfs/thumbnails/thumbnails.cxx',

    'vfs/linux/mountinfo.cxx',
    'vfs/linux/self.cxx',
    'vfs/linux/sysfs.cxx',

    'vfs/mime-type/mime-action.cxx',
    'vfs/mime-type/mime-type.cxx',
    'vfs/mime-type/chrome/mime-utils.cxx',
)

vfs_dependencies = [
    gtk_dependencies,

    logger_dep,
    pugixml_dep,
    botan_dep,
    glycin_wrapper_dep,
    libudevpp_dep,
    notify_dep,
]

vfs_lib = static_library(
    'vfs',
    vfs_sources,
    include_directories: include_directories('.'),
    dependencies: vfs_dependencies,
    install: false,
    cpp_pch: '../../pch/pch.hxx',
)

vfs_dep = declare_dependency(
    link_with: vfs_lib,
    include_directories: include_directories('.'),
    dependencies: vfs_dependencies,
)
