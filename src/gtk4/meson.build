sources = files(
    'main.cxx',

    'commandline/commandline.cxx',

    'gui/archiver.cxx',
    'gui/browser.cxx',
    'gui/layout.cxx',
    'gui/main-window.cxx',
    'gui/menubar.cxx',
    'gui/task.cxx',

    'gui/tab/files.cxx',
    'gui/tab/path.cxx',
    'gui/tab/search.cxx',
    'gui/tab/side_default.cxx',
    'gui/tab/side_device.cxx',
    'gui/tab/side_tree.cxx',
    'gui/tab/statusbar.cxx',
    'gui/tab/tab.cxx',
    'gui/tab/toolbar.cxx',

    'gui/action/delete.cxx',
    'gui/action/open.cxx',
    'gui/action/trash.cxx',

    'gui/dialog/about.cxx',
    'gui/dialog/action.cxx',
    'gui/dialog/app-chooser.cxx',
    'gui/dialog/bookmarks.cxx',
    'gui/dialog/create.cxx',
    'gui/dialog/donate.cxx',
    'gui/dialog/pattern.cxx',
    'gui/dialog/rename.cxx',
    'gui/dialog/preferences.cxx',
    'gui/dialog/properties.cxx',
    'gui/dialog/text.cxx',

    'gui/dialog/media/audio-video-metadata.cxx',
    'gui/dialog/media/image-metadata.cxx',

    'gui/lib/history.cxx',
    'gui/lib/clipboard.cxx',

    'settings/config.cxx',
)

spacefm = build_target(
    'spacefm4',
    sources,
    target_type: 'executable',
    include_directories : include_directories('.'),
    install: true,
    install_dir: bindir,
    dependencies: [
        base_dependencies,
        gtk4_dependencies,
        media_dependencies,
        natsort_dep,
        vfs_gtk4_dep,
    ],
    cpp_pch: '../../pch/pch.hxx',
)
