## Test only dependencies

doctest_dep = dependency('doctest', required : true)

## Test Source files

test_sources = files()

# Spacefm Source Files
test_sources += files(
    # SFM
    'spacefm/concurrency.cxx',
    'spacefm/types.cxx',
    'spacefm/logger.cxx',

    'spacefm/utils/permissions.cxx',
    'spacefm/utils/shell-quote.cxx',

    # Settings
    'spacefm/settings/settings.cxx',

    # GUI
    'spacefm/gui/natsort/strnatcmp.cxx',
    'spacefm/gui/utils/history.cxx',

    # VFS
    'spacefm/vfs/mime-type/chrome/mime-utils.cxx',
    'spacefm/vfs/utils/utils.cxx',
    'spacefm/vfs/clipboard.cxx',
    'spacefm/vfs/error.cxx',
    'spacefm/vfs/user-dirs.cxx',
)

# Test Source Files
test_sources += files(
    'src/main.cxx',

    'src/utils/test-permissions.cxx',

    # SFM
    'src/base/test-types.cxx',

    # GUI
    'src/gui/natsort/test-natsort.cxx',
    'src/gui/utils/test-history.cxx',

    # VFS
    'src/vfs/mime-type/chrome/mime-utils.cxx',
    'src/vfs/utils/test-utils.cxx',
    'src/vfs/utils/test-file-ops.cxx',
    'src/vfs/test-clipboard.cxx',
    'src/vfs/test-error.cxx',
)

## Build

incdir_tests = include_directories(['src', 'spacefm'])

dependencies_tests = [doctest_dep]
dependencies_tests += dependencies

test_suite = build_target(
    'test_suite',
    sources: test_sources,
    target_type: 'executable',
    include_directories: incdir_tests,
    install: false,
    dependencies: dependencies_tests,
    cpp_pch: '../pch/pch.hxx',
)

test('SpaceFM Test Suite', test_suite)
