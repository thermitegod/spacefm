## Test only dependencies

doctest_dep = dependency('doctest', required : true)

## Spacefm Test Source files

test_sources_sfm = files()

# Spacefm Source Files, do not need to do vfs source
# files they get done through the vfs static_library
# TODO switch to gtk4 gui sources
test_sources_sfm += files(
    # SFM
    '../src/gtk3/types.cxx',

    # Settings
    '../src/gtk3/settings/settings.cxx',

    # Common
    '../src/common/natsort/strnatcmp.cxx',

    # GUI
    '../src/gtk3/gui/utils/history.cxx',
)

# Test Source Files
test_sources_sfm += files(
    'src/main.cxx',

    'src/base/test-types.cxx',

    # gui
    'src/gui/natsort/test-natsort.cxx',
    'src/gui/utils/test-history.cxx',

    # vfs
    # 'src/vfs/test-clipboard.cxx',
    'src/vfs/test-error.cxx',
    'src/vfs/test-execute.cxx',

    'src/vfs/task/chmod.cxx',
    'src/vfs/task/chown.cxx',
    'src/vfs/task/copy.cxx',
    'src/vfs/task/create-directory.cxx',
    'src/vfs/task/create-file.cxx',
    'src/vfs/task/create-hardlink.cxx',
    'src/vfs/task/create-symlink.cxx',
    'src/vfs/task/move.cxx',
    'src/vfs/task/remove.cxx',

    'src/vfs/utils/test-utils.cxx',
    'src/vfs/utils/test-file-ops.cxx',
    'src/vfs/utils/test-permissions.cxx',

    # vfs - chrome
    'src/vfs/mime-type/chrome/test-mime-utils.cxx',
)

## Build

test_suite = build_target(
    'test_suite',
    sources: test_sources_sfm,
    target_type: 'executable',
    include_directories: include_directories(['src', '../src/common', '../src/gtk3']),
    install: false,
    dependencies: [
        doctest_dep,

        base_dependencies,
        gtk4_dependencies,
        vfs_gtk4_dep,
    ],
    cpp_pch: '../pch/pch.hxx',
)

# notify-cpp

# Test Source Files
test_sources_inotify = files(
    'src/main.cxx',

    'src/vfs/notify-cpp/event_handler_test.cxx',
    'src/vfs/notify-cpp/inotify_controller_test.cxx',
)

test_suite_notify_inotify = build_target(
    'test_suite_inotify',
    sources: test_sources_inotify,
    target_type: 'executable',
    include_directories: include_directories(['src', '../src']),
    install: false,
    dependencies: [
        doctest_dep,

        vfs_gtk4_dep,
    ],
    cpp_pch: '../pch/pch.hxx',
)

test('Test Suite - SpaceFM', test_suite)
test('Test Suite - vendor notify-cpp, inotify', test_suite_notify_inotify)
