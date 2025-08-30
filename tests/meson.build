## Test only dependencies

doctest_dep = dependency('doctest', required : true)

## Test Source files

test_sources = files()

# Spacefm Source Files
test_sources += files(
    # SFM
    '../src/types.cxx',
    '../src/logger.cxx',

    # Settings
    '../src/settings/settings.cxx',

    # GUI
    '../src/gui/natsort/strnatcmp.cxx',
    '../src/gui/utils/history.cxx',
)

# Test Source Files
test_sources += files(
    'src/main.cxx',


    # SFM
    'src/base/test-types.cxx',

    # GUI
    'src/gui/natsort/test-natsort.cxx',
    'src/gui/utils/test-history.cxx',

    # VFS
    'src/vfs/test-clipboard.cxx',
    'src/vfs/test-error.cxx',
    'src/vfs/test-execute.cxx',

    'src/vfs/utils/test-utils.cxx',
    'src/vfs/utils/test-file-ops.cxx',
    'src/vfs/utils/test-permissions.cxx',

    'src/vfs/mime-type/chrome/test-mime-utils.cxx',
)

## Build

incdir_tests = include_directories(['src', '../src'])

test_suite = build_target(
    'test_suite',
    sources: test_sources,
    target_type: 'executable',
    include_directories: incdir_tests,
    install: false,
    dependencies: [
        doctest_dep,

        base_dependencies,
        gtk4_dependencies,
        dep_vfs_gtk4,
    ],
    cpp_pch: '../pch/pch.hxx',
)

test('SpaceFM Test Suite', test_suite)
