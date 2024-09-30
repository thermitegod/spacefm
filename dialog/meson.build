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

# Pattern
incdir_pattern = include_directories(['pattern', '../src/datatypes'])
sources_pattern = files(
    'pattern/main.cxx',
    'pattern/pattern.cxx',
)

spacefm = build_target(
    'spacefm_pattern',
    sources_pattern,
    target_type: 'executable',
    include_directories: incdir_pattern,
    install: true,
    install_dir: bindir,
    dependencies: [
        gtkmm4_dep,
        glibmm4_dep,
        giomm4_dep,
        sigc_dep,
    ],
    # cpp_pch: 'pch/cxx_pch.hxx',
)
