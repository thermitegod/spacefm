# Thumbnails
incdir = include_directories(['thumbnails', '../src'])
sources = files(
    'thumbnails/thumbnails.cxx',

    '../src/logger.cxx',
)

spacefm = build_target(
    'spacefm-tools-thumbnails',
    sources,
    target_type: 'executable',
    include_directories: incdir,
    install: false,
    install_dir: bindir,
    dependencies: [
        cli11_dep,
        dep_vfs_gtk4,
    ],
    cpp_pch: '../pch/pch.hxx',
)
