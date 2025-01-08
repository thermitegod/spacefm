## Test only dependencies

doctest_dep = dependency('doctest', required : true)

## Test Source files

test_sources = files()

# Spacefm Source Files
test_sources += files(
    # SFM
    'spacefm/concurrency.cxx',
    'spacefm/types.cxx',

    # Settings
    'spacefm/settings/settings.cxx',

    # PTK
    'spacefm/ptk/natsort/strnatcmp.cxx',

    # VFS
    'spacefm/vfs/utils/vfs-utils.cxx',
)

# Test Source Files
test_sources += files(
    'src/main.cxx',

    # SFM
    'src/base/test-types.cxx',

    # PTK
    'src/ptk/natsort/test-natsort.cxx',

    # VFS
    'src/vfs/utils/test-vfs-utils.cxx',
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
)

test('SpaceFM Test Suite', test_suite)
