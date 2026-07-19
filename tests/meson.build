## Test only dependencies

doctest_dep = dependency('doctest', required : true)

## Spacefm Test Source files

test_sources_sfm = files()

# TODO need something better here
test_sources_sfm += files(
    '../src/spacefm/gui/lib/history.cxx',
)

# Test Source Files
test_sources_sfm += files(
    'src/main.cxx',

    # gui
    'src/gui/natsort/natsort.cxx',
    'src/gui/utils/history.cxx',

    # vfs
    'src/vfs/error.cxx',
    'src/vfs/execute.cxx',
    'src/vfs/task-manager.cxx',
    'src/vfs/trash.cxx',

    'src/vfs/linux/mountinfo.cxx',

    'src/vfs/utils/utils.cxx',
    'src/vfs/utils/file-ops.cxx',
    'src/vfs/utils/permissions.cxx',

    'src/vfs/notify-cpp/controller.cxx',

    # vfs - chrome
    'src/vfs/mime-type/chrome/mime-utils.cxx',
)

## Build

test_suite = build_target(
    'test_suite',
    sources: test_sources_sfm,
    target_type: 'executable',
    include_directories: include_directories(['./src', '../src/spacefm']),
    install: false,
    dependencies: [
        doctest_dep,

        base_dependencies,
        gtk_dependencies,
        vfs_dep,

        natsort_dep,
    ],
    cpp_pch: '../pch/pch.hxx',
)

test('Test Suite', test_suite)
