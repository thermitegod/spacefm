## Test only dependencies

doctest_dep = dependency('doctest', required : true)

## Spacefm Test Source files

test_sources_sfm = files()

# Spacefm Source Files, do not need to do vfs source
# files they get done through the vfs static_library
# TODO switch to gtk4 gui sources
test_sources_sfm += files(
    # Common
    '../src/common/natsort/strnatcmp.cxx',

    # GUI
    '../src/gtk4/gui/lib/history.cxx',
)

# Test Source Files
test_sources_sfm += files(
    'src/main.cxx',

    # gui
    'src/gui/natsort/natsort.cxx',
    'src/gui/utils/history.cxx',

    # vfs
    # 'src/vfs/clipboard.cxx',
    # 'src/vfs/error.cxx',
    'src/vfs/execute.cxx',

    'src/vfs/utils/utils.cxx',
    'src/vfs/utils/file-ops.cxx',
    'src/vfs/utils/permissions.cxx',

    # vfs - chrome
    'src/vfs/mime-type/chrome/mime-utils.cxx',
)

## Build

test_suite = build_target(
    'test_suite',
    sources: test_sources_sfm,
    target_type: 'executable',
    include_directories: include_directories(['../src/common', '../src/gtk4']),
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
