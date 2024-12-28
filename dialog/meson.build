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

# App Chooser
incdir_chooser = include_directories(['app-chooser', '../src/datatypes'])
sources_chooser = files(
    'app-chooser/main.cxx',
    'app-chooser/app-chooser.cxx',

    'app-chooser/logger.cxx',

    'app-chooser/utils/misc.cxx',

    'app-chooser/vfs/vfs-mime-type.cxx',
    'app-chooser/vfs/vfs-user-dirs.cxx',

    'app-chooser/vfs/utils/vfs-utils.cxx',

	'app-chooser/vfs/mime-type/chrome/mime-utils.cxx',
	'app-chooser/vfs/mime-type/mime-type.cxx',
)

spacefm = build_target(
    'spacefm_app_chooser',
    sources_chooser,
    target_type: 'executable',
    include_directories: incdir_chooser,
    install: true,
    install_dir: bindir,
    dependencies: [
        gtkmm4_dep,
        glibmm4_dep,
        giomm4_dep,
        sigc_dep,

        concurrencpp_dep,
        pugixml_dep,
    ],
    # cpp_pch: 'pch/cxx_pch.hxx',
)

# Error
incdir_error = include_directories(['error', '../src/datatypes'])
sources_error = files(
    'error/main.cxx',
    'error/error.cxx',

    'error/vfs/utils/icon.cxx',
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

# Keybinding
incdir_keybinding = include_directories(['keybinding', '../src/datatypes'])
sources_keybinding = files(
    'keybinding/main.cxx',
    'keybinding/keybinding.cxx',
    'keybinding/utils.cxx',
)

spacefm = build_target(
    'spacefm_keybindings',
    sources_keybinding,
    target_type: 'executable',
    include_directories: incdir_keybinding,
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

# Keybinding Set Key
incdir_keybinding_set = include_directories(['keybinding_set_key', '../src/datatypes'])
sources_keybinding_set = files(
    'keybinding_set_key/main.cxx',
    'keybinding_set_key/keybinding.cxx',
    'keybinding_set_key/utils.cxx',
)

spacefm = build_target(
    'spacefm_keybinding_set_key',
    sources_keybinding_set,
    target_type: 'executable',
    include_directories: incdir_keybinding_set,
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

# Preference
incdir_preference = include_directories(['preference', '../src/datatypes'])
sources_preference = files(
    'preference/main.cxx',
    'preference/preference.cxx',
)

spacefm = build_target(
    'spacefm_preference',
    sources_preference,
    target_type: 'executable',
    include_directories: incdir_preference,
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

# Properties
incdir_properties = include_directories(['properties', '../src/datatypes'])
sources_properties = files(
    'properties/main.cxx',
    'properties/properties.cxx',

    'properties/concurrency.cxx',
    'properties/logger.cxx',

    'properties/utils/misc.cxx',

    'properties/vfs/vfs-file.cxx',
    'properties/vfs/vfs-mime-type.cxx',
    'properties/vfs/vfs-user-dirs.cxx',

    'properties/vfs/utils/vfs-utils.cxx',

	'properties/vfs/mime-type/chrome/mime-utils.cxx',
	'properties/vfs/mime-type/mime-type.cxx',

    'properties/media/audio-video-metadata.cxx',
    'properties/media/image-metadata.cxx',
)

spacefm = build_target(
    'spacefm_properties',
    sources_properties,
    target_type: 'executable',
    include_directories: incdir_properties,
    install: true,
    install_dir: bindir,
    dependencies: [
        gtkmm4_dep,
        glibmm4_dep,
        giomm4_dep,
        sigc_dep,

        concurrencpp_dep,
        pugixml_dep,
        media_dependencies,
    ],
    # cpp_pch: 'pch/cxx_pch.hxx',
)
