## Test only dependencies

doctest_dep = dependency('doctest', required: true)

## Spacefm Test Source files

sources = files()

# TODO need something better here
sources += files(
    '../src/spacefm/gui/lib/history.cxx',
)

# Test Source Files
sources += files(
    'src/main.cxx',

    'src/natsort/natsort.cxx',

    # gui
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

test_suite_exe = executable(
    'test_suite',
    sources,
    include_directories: include_directories(['./src', '../src/spacefm']),
    install: false,
    dependencies: [
        doctest_dep,

        base_dependencies,
        gtk_dependencies,
        natsort_dep,
        vfs_dep,
    ],
    cpp_pch: '../pch/pch.hxx',
)

test('Test Suite', test_suite_exe)
