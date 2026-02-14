sources = files(
    'autosave.cxx',
    'main.cxx',
    'settings.cxx',
    'single-instance.cxx',
    'types.cxx',

    'compat/gtk4-porting.cxx',

    'commandline/commandline.cxx',

    'xset/xset.cxx',
    'xset/xset-context-menu.cxx',
    'xset/xset-defaults.cxx',
    'xset/xset-lookup.cxx',
    'xset/utils/xset-utils.cxx',

    'settings/config-load.cxx',
    'settings/config-save.cxx',
    'settings/settings.cxx',
    'settings/upgrade/config-upgrade.cxx',

    'utils/strdup.cxx',

    'gui/archiver.cxx',
    'gui/clipboard.cxx',
    'gui/dir-tree.cxx',
    'gui/file-browser.cxx',
    'gui/file-list.cxx',
    'gui/file-menu.cxx',
    'gui/file-task.cxx',

    'gui/main-window.cxx',
    'gui/path-bar.cxx',
    'gui/search-bar.cxx',

    'gui/action/delete.cxx',
    'gui/action/open.cxx',
    'gui/action/paste.cxx',
    'gui/action/trash.cxx',

    'gui/dialog/about.cxx',
    'gui/dialog/action-list.cxx',
    'gui/dialog/app-chooser.cxx',
    'gui/dialog/bookmarks.cxx',
    'gui/dialog/create.cxx',
    'gui/dialog/file-search.cxx',
    'gui/dialog/keybindings.cxx',
    'gui/dialog/preference.cxx',
    'gui/dialog/properties.cxx',
    'gui/dialog/rename.cxx',
    'gui/dialog/rename-batch.cxx',
    'gui/dialog/text.cxx',

    'gui/view/bookmark.cxx',
    'gui/view/dir-tree.cxx',
    'gui/view/file-task.cxx',
    'gui/view/location.cxx',

    'gui/utils/utils.cxx',
    'gui/utils/history.cxx',
)

if get_option('socket')
    sources += files(
        'commandline/socket.cxx',
        'commandline/socket/get-task/subcommands.cxx',
        'commandline/socket/get/subcommands.cxx',
        'commandline/socket/run-task/subcommands.cxx',
        'commandline/socket/set-task/subcommands.cxx',
        'commandline/socket/set/subcommands.cxx',
        'commandline/socket/subcommands.cxx',
        'socket/commands.cxx',
        'socket/server.cxx',
    )
endif

spacefm = build_target(
    'spacefm',
    sources,
    target_type: 'executable',
    include_directories : include_directories('.'),
    install: true,
    install_dir: bindir,
    dependencies: [
        base_dependencies,
        natsort_dep,
        vfs_gtk3_dep,
    ],
    cpp_pch: '../../pch/pch.hxx',
)
