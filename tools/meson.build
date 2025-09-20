# Thumbnails
incdir = include_directories(['thumbnails', '../src'])
sources = files(
    'thumbnails/thumbnails.cxx',
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
        vfs_gtk4_dep,
    ],
    cpp_pch: '../pch/pch.hxx',
)
