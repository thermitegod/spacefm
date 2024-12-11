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

# Error
incdir_error = include_directories('error')
sources_error = files(
    'error/main.cxx',
    'error/error.cxx',
)

spacefm = build_target(
    'spacefm_error',
    sources_error,
    target_type: 'executable',
    include_directories: incdir_error,
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

# File Action
incdir_action = include_directories(['file-action', '../src/datatypes'])
sources_action = files(
    'file-action/main.cxx',
    'file-action/action.cxx',
)

spacefm = build_target(
    'spacefm_action',
    sources_action,
    target_type: 'executable',
    include_directories: incdir_action,
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

# Message
incdir_message = include_directories(['message', '../src/datatypes'])
sources_message = files(
    'message/main.cxx',
    'message/message.cxx',
)

spacefm = build_target(
    'spacefm_message',
    sources_message,
    target_type: 'executable',
    include_directories: incdir_message,
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
