## Test only dependencies

doctest_dep = dependency('doctest', required : true)

## Test Source files

test_sources = files()

# Spacefm Source Files
test_sources += files(
    # SFM
    'spacefm/concurrency.cxx',
    'spacefm/types.cxx',

    'spacefm/utils/permissions.cxx',

    # Settings
    'spacefm/settings/settings.cxx',

    # PTK
    'spacefm/ptk/natsort/strnatcmp.cxx',
    'spacefm/ptk/utils/history.cxx',

    # VFS
    'spacefm/vfs/mime-type/chrome/mime-utils.cxx',
    'spacefm/vfs/utils/vfs-utils.cxx',
    'spacefm/vfs/vfs-error.cxx',
    'spacefm/vfs/vfs-user-dirs.cxx',
)

# Test Source Files
test_sources += files(
    'src/main.cxx',

    'src/utils/test-permissions.cxx',

    # SFM
    'src/base/test-types.cxx',

    # PTK
    'src/ptk/natsort/test-natsort.cxx',
    'src/ptk/utils/test-history.cxx',

    # VFS
    'src/vfs/mime-type/chrome/mime-utils.cxx',
    'src/vfs/utils/test-vfs-utils.cxx',
    'src/vfs/utils/test-file-ops.cxx',
    'src/vfs/test-vfs-error.cxx',
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
