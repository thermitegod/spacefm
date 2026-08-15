# Create Thumbnails
sources = files(
    'thumbnails/thumbnails-create.cxx',
)

thumbnails_exe = executable(
    'spacefm-tools-create-thumbnails',
    sources,
    include_directories: include_directories('thumbnails'),
    install: false,
    dependencies: [
        cli11_dep,
        vfs_dep,
    ],
    cpp_pch: '../pch/pch.hxx',
)

# Validate Thumbnails
sources = files(
    'thumbnails/thumbnails-validate.cxx',
)

validate_exe = executable(
    'spacefm-tools-validate-thumbnails',
    sources,
    include_directories: include_directories('thumbnails'),
    install: false,
    dependencies: [
        cli11_dep,
        vfs_dep,
    ],
    cpp_pch: '../pch/pch.hxx',
)

# Stress Testing Tools
sources = files(
    'stress/fs-events.cxx',
)

stress_exe = executable(
    'stress-fs-events',
    sources,
    include_directories: include_directories('stress'),
    install: false,
    dependencies: [
        cli11_dep,
        vfs_dep,
    ],
    # cpp_pch: '../pch/pch.hxx',
)

# Dev Dialog Template
sources = files(
    'template/main.cxx',
    'template/window.cxx',
)

template_exe = executable(
    'template',
    sources,
    include_directories: include_directories('template'),
    install: false,
    dependencies: [
        gtk_dependencies,
    ],
    cpp_pch: '../pch/pch.hxx',
)
