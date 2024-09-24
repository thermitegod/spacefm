# About
incdir_about = include_directories('about')
sources_about = files(
    'about/main.cxx',
    'about/about.cxx',
)

spacefm = build_target(
    'spacefm_about',
    sources_about,
    target_type: 'executable',
    include_directories: incdir_about,
    install: true,
    install_dir: bindir,
    dependencies: [
        gtkmm4_dep,
        glibmm4_dep,
        giomm4_dep,
    ],
    # cpp_pch: 'pch/cxx_pch.hxx',
)
