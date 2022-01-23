/*
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

// turn on to debug GDK_THREADS_ENTER/GDK_THREADS_LEAVE related deadlocks
#undef _DEBUG_THREAD

#include <csignal>

#include <string>
#include <filesystem>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "main-window.hxx"
#include "window-reference.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-location-view.hxx"

#include "git-version.h"

#include "autosave.hxx"
#include "find-files.hxx"
#include "pref-dialog.hxx"
#include "socket.hxx"
#include "settings.hxx"

#include "utils.hxx"

static bool folder_initialized = false;
static bool daemon_initialized = false;

// clang-format off
static GOptionEntry opt_entries[] =
{
    {"new-tab", 't', 0, G_OPTION_ARG_NONE, &cli_flags.new_tab, "Open directories in new tab of last window (default)", nullptr},
    {"reuse-tab", 'r', 0, G_OPTION_ARG_NONE, &cli_flags.reuse_tab, "Open directory in current tab of last used window", nullptr},
    {"no-saved-tabs", 'n', 0, G_OPTION_ARG_NONE, &cli_flags.no_tabs, "Don't load saved tabs", nullptr},
    {"new-window", 'w', 0, G_OPTION_ARG_NONE, &cli_flags.new_window, "Open directories in new window", nullptr},
    {"panel", 'p', 0, G_OPTION_ARG_INT, &cli_flags.panel, "Open directories in panel 'P' (1-4)", "P"},
    {"daemon-mode", 'd', 0, G_OPTION_ARG_NONE, &cli_flags.daemon_mode, "Run as a daemon", nullptr},
    {"config", 'c', 0, G_OPTION_ARG_STRING, &cli_flags.config_dir, "Use DIR as configuration directory", "DIR"},
    {"disable-git", 'G', 0, G_OPTION_ARG_NONE, &cli_flags.disable_git_settings, "Don't use git to keep session history", nullptr},
    {"find-files", 'f', 0, G_OPTION_ARG_NONE, &cli_flags.find_files, "Show File Search", nullptr},
    {"socket-cmd", 's', 0, G_OPTION_ARG_NONE, &cli_flags.socket_cmd, "Send a socket command (See -s help)", nullptr},
    {"version", 'v', 0, G_OPTION_ARG_NONE, &cli_flags.version_opt, "Show version information", nullptr},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &cli_flags.files, nullptr, "[DIR | FILE | URL]..."},
    {GOptionEntry()}
};
// clang-format on

static bool handle_parsed_commandline_args();
static void open_file(const char* path);

#ifdef _DEBUG_THREAD
G_LOCK_DEFINE(gdk_lock);
void
debug_gdk_threads_enter(const char* message)
{
    LOG_DEBUG("Thread {:p} tries to get GDK lock: {}", (void*)g_thread_self(), message);
    G_LOCK(gdk_lock);
    LOG_DEBUG("Thread {:p} got GDK lock: {}", (void*)g_thread_self(), message);
}

static void
_debug_gdk_threads_enter()
{
    debug_gdk_threads_enter("called from GTK+ internal");
}

void
debug_gdk_threads_leave(const char* message)
{
    LOG_DEBUG("Thread {:p} tries to release GDK lock: {}", (void*)g_thread_self(), message);
    G_UNLOCK(gdk_lock);
    LOG_DEBUG("Thread {:p} released GDK lock: {}", (void*)g_thread_self(), message);
}

static void
_debug_gdk_threads_leave()
{
    debug_gdk_threads_leave("called from GTK+ internal");
}
#endif

static void
init_folder()
{
    if (folder_initialized)
        return;

    vfs_volume_init();
    vfs_thumbnail_init();

    vfs_mime_type_set_icon_size(app_settings.big_icon_size, app_settings.small_icon_size);
    vfs_file_info_set_thumbnail_size(app_settings.big_icon_size, app_settings.small_icon_size);

    folder_initialized = true;
}

static void
exit_from_signal(int sig)
{
    (void)sig;
    gtk_main_quit();
}

static void
init_daemon()
{
    if (daemon_initialized)
        return;

    init_folder();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, exit_from_signal);
    signal(SIGINT, exit_from_signal);
    signal(SIGTERM, exit_from_signal);

    daemon_initialized = true;
}

static void
open_in_tab(FMMainWindow** main_window, const char* real_path)
{
    XSet* set;
    int p;
    // create main window if needed
    if (G_UNLIKELY(!*main_window))
    {
        // initialize things required by folder view
        if (G_UNLIKELY(!cli_flags.daemon_mode))
            init_folder();

        // preload panel?
        if (cli_flags.panel > 0 && cli_flags.panel < 5)
            // user specified panel
            p = cli_flags.panel;
        else
        {
            // use first visible panel
            for (p = 1; p < 5; p++)
            {
                if (xset_get_b_panel(p, "show"))
                    break;
            }
        }
        if (p == 5)
            p = 1; // no panels were visible (unlikely)

        // set panel to load real_path on window creation
        set = xset_get_panel(p, "show");
        set->ob1 = g_strdup(real_path);
        set->b = XSET_B_TRUE;

        // create new window
        fm_main_window_store_positions(nullptr);
        *main_window = FM_MAIN_WINDOW(fm_main_window_new());
    }
    else
    {
        // existing window
        bool tab_added = false;
        if (cli_flags.panel > 0 && cli_flags.panel < 5)
        {
            // change to user-specified panel
            if (!gtk_notebook_get_n_pages(GTK_NOTEBOOK((*main_window)->panel[cli_flags.panel - 1])))
            {
                // set panel to load real_path on panel load
                set = xset_get_panel(cli_flags.panel, "show");
                set->ob1 = g_strdup(real_path);
                tab_added = true;
                set->b = XSET_B_TRUE;
                show_panels_all_windows(nullptr, *main_window);
            }
            else if (!gtk_widget_get_visible((*main_window)->panel[cli_flags.panel - 1]))
            {
                // show panel
                set = xset_get_panel(cli_flags.panel, "show");
                set->b = XSET_B_TRUE;
                show_panels_all_windows(nullptr, *main_window);
            }
            (*main_window)->curpanel = cli_flags.panel;
            (*main_window)->notebook = (*main_window)->panel[cli_flags.panel - 1];
        }
        if (!tab_added)
        {
            if (cli_flags.reuse_tab)
            {
                main_window_open_path_in_current_tab(*main_window, real_path);
                cli_flags.reuse_tab = false;
            }
            else
                fm_main_window_add_new_tab(*main_window, real_path);
        }
    }
    gtk_window_present(GTK_WINDOW(*main_window));
}

static bool
handle_parsed_commandline_args()
{
    bool ret = true;
    char* default_files[2] = {nullptr, nullptr};

    app_settings.load_saved_tabs = !cli_flags.no_tabs;

    // no files were specified from cli
    if (G_LIKELY(!cli_flags.files))
        cli_flags.files = default_files;

    // get the last active window on this desktop, if available
    FMMainWindow* main_window = nullptr;
    if (cli_flags.new_tab || cli_flags.reuse_tab)
    {
        main_window = fm_main_window_get_on_current_desktop();
        // LOG_INFO("fm_main_window_get_on_current_desktop = {:p}  {} {}",
        //          (void*)main_window,
        //          cli_flags.new_tab ? "new_tab" : "",
        //          cli_flags.reuse_tab ? "reuse_tab" : "");
    }

    if (cli_flags.find_files)
    {
        // find files
        init_folder();
        fm_find_files((const char**)cli_flags.files);
        cli_flags.find_files = false;
    }
    else if (cli_flags.daemon_mode)
    {
        // start daemon mode
        init_daemon();
    }
    else if (cli_flags.files != default_files)
    {
        // open files passed in command line arguments
        ret = false;
        char** file;
        for (file = cli_flags.files; *file; ++file)
        {
            char* real_path;

            // skip empty string
            if (!**file)
                continue;

            real_path = dup_to_absolute_file_path(file);

            if (std::filesystem::is_directory(real_path))
            {
                open_in_tab(&main_window, real_path);
                ret = true;
            }
            else if (std::filesystem::exists(real_path))
            {
                struct stat statbuf;
                if (stat(real_path, &statbuf) == 0 && S_ISBLK(statbuf.st_mode))
                {
                    // open block device eg /dev/sda1
                    if (!main_window)
                    {
                        open_in_tab(&main_window, "/");
                        ptk_location_view_open_block(real_path, false);
                    }
                    else
                        ptk_location_view_open_block(real_path, true);
                    ret = true;
                    gtk_window_present(GTK_WINDOW(main_window));
                }
                else
                    open_file(real_path);
            }
            else if ((*file[0] != '/' && strstr(*file, ":/")) || g_str_has_prefix(*file, "//"))
            {
                if (main_window)
                    main_window_open_network(main_window, *file, true);
                else
                {
                    open_in_tab(&main_window, "/");
                    main_window_open_network(main_window, *file, false);
                }
                ret = true;
                gtk_window_present(GTK_WINDOW(main_window));
            }
            else
            {
                std::string err_msg = fmt::format("File doesn't exist:\n\n{}", real_path);
                ptk_show_error(nullptr, "Error", err_msg);
            }
            g_free(real_path);
        }
    }
    else if (!check_socket_daemon())
    {
        // no files specified, just create window with default tabs
        if (G_UNLIKELY(!main_window))
        {
            // initialize things required by folder view
            if (G_UNLIKELY(!cli_flags.daemon_mode))
                init_folder();
            fm_main_window_store_positions(nullptr);
            main_window = FM_MAIN_WINDOW(fm_main_window_new());
        }
        gtk_window_present(GTK_WINDOW(main_window));

        if (cli_flags.panel > 0 && cli_flags.panel < 5)
        {
            // user specified a panel with no file, let's show the panel
            if (!gtk_widget_get_visible(main_window->panel[cli_flags.panel - 1]))
            {
                // show panel
                XSet* set;
                set = xset_get_panel(cli_flags.panel, "show");
                set->b = XSET_B_TRUE;
                show_panels_all_windows(nullptr, main_window);
            }
            focus_panel(nullptr, (void*)main_window, cli_flags.panel);
        }
    }

    // LOG_INFO("    handle_parsed_commandline_args mw = {:p}", main_window);

    if (cli_flags.files != default_files)
        g_strfreev(cli_flags.files);
    cli_flags.files = nullptr;

    return ret;
}

static void
tmp_clean()
{
    std::string tmp = xset_get_user_tmp_dir();
    std::filesystem::remove_all(tmp);
    LOG_INFO("Removed {}", tmp);
}

int
main(int argc, char* argv[])
{
    // logging init
    ztd::Logger->initialize();

    // load spacefm.conf
    load_conf();

    // separate instance options
    if (argc > 1)
    {
        // socket_command?
        if (!strcmp(argv[1], "-s") || !strcmp(argv[1], "--socket-cmd"))
        {
            if (argv[2] && (!strcmp(argv[2], "help") || !strcmp(argv[2], "--help")))
            {
                fmt::print("For help run, man spacefm-socket\n");
                return EXIT_SUCCESS;
            }
            char* reply = nullptr;
            int ret = send_socket_command(argc, argv, &reply);
            if (reply && reply[0])
                fmt::print("{}", reply);
            g_free(reply);
            return ret;
        }
    }

    // initialize GTK+ and parse the command line arguments
    GError* err = nullptr;
    if (G_UNLIKELY(!gtk_init_with_args(&argc, &argv, "", opt_entries, nullptr, &err)))
    {
        LOG_INFO("{}", err->message);
        g_error_free(err);
        return EXIT_FAILURE;
    }

    // ref counter needs to know if in daemon_mode
    WindowReference::set_daemon(cli_flags.daemon_mode);

    // socket command with other options?
    if (cli_flags.socket_cmd)
    {
        LOG_ERROR("socket-cmd must be first option");
        return EXIT_FAILURE;
    }

    // --disable-git
    config_settings.git_backed_settings = !cli_flags.disable_git_settings;

    // --version
    if (cli_flags.version_opt)
    {
        fmt::print("{} {}\n", PACKAGE_NAME_FANCY, GIT_VERSION);
        return EXIT_SUCCESS;
    }

#ifdef _DEBUG_THREAD
    // Initialize multithreading
    // No matter we use threads or not, it's safer to initialize this earlier.
    gdk_threads_set_lock_functions(_debug_gdk_threads_enter, _debug_gdk_threads_leave);
#endif

    // ensure that there is only one instance of spacefm.
    // if there is an existing instance, command line arguments
    // will be passed to the existing instance, and exit() will be called here.
    single_instance_check();
    // If we reach this point, we are the first instance.
    // Subsequent processes will exit() inside single_instance_check
    // and won't reach here.

    // initialize the file alteration monitor
    if (G_UNLIKELY(!vfs_file_monitor_init()))
    {
        ptk_show_error(nullptr,
                       "Error",
                       "Error: Unable to initialize inotify file change monitor.\n\nDo you have "
                       "an inotify-capable kernel?");
        vfs_file_monitor_clean();
        // free_settings();
        return EXIT_FAILURE;
    }

    // Seed RNG
    // using the current time is a good enough seed
    std::time_t epoch = std::time(nullptr);
    std::asctime(std::localtime(&epoch));
    srand48(epoch);

    // Initialize our mime-type system
    vfs_mime_type_init();

    // load config file
    load_settings(cli_flags.config_dir);

    // start autosave thread
    autosave_init();

    std::atexit(ztd::Logger->shutdown);

    main_window_event(nullptr, nullptr, "evt_start", 0, 0, nullptr, 0, 0, 0, false);

    // handle the parsed result of command line args
    if (handle_parsed_commandline_args())
    {
        app_settings.load_saved_tabs = true;

        // run the main loop
        gtk_main();
    }

    main_window_event(nullptr, nullptr, "evt_exit", 0, 0, nullptr, 0, 0, 0, false);

    single_instance_finalize();
    vfs_volume_finalize();
    vfs_mime_type_clean();
    vfs_file_monitor_clean();
    autosave_terminate();
    tmp_clean();
    free_settings();

    return EXIT_SUCCESS;
}

static void
open_file(const char* path)
{
    VFSFileInfo* file = vfs_file_info_new();
    vfs_file_info_get(file, path, nullptr);
    VFSMimeType* mime_type = vfs_file_info_get_mime_type(file);
    bool opened = false;
    GError* err = nullptr;

    char* app_name = vfs_mime_type_get_default_action(mime_type);
    if (app_name)
    {
        opened = vfs_file_info_open_file(file, path, &err);
        g_free(app_name);
    }
    else
    {
        app_name = ptk_choose_app_for_mime_type(nullptr, mime_type, true, true, true, false);
        if (app_name)
        {
            VFSAppDesktop* desktop = vfs_app_desktop_new(app_name);
            if (!vfs_app_desktop_get_exec(desktop))
                desktop->exec = g_strdup(app_name); /* This is a command line */
            GList* files = g_list_prepend(nullptr, (void*)path);
            opened =
                vfs_app_desktop_open_files(gdk_screen_get_default(), nullptr, desktop, files, &err);
            g_free(files->data);
            g_list_free(files);
            vfs_app_desktop_unref(desktop);
            g_free(app_name);
        }
        else
            opened = true;
    }

    if (!opened)
    {
        const char* error_msg;
        if (err && err->message)
        {
            error_msg = err->message;
        }
        else
            error_msg = "Don't know how to open the file";
        char* disp_path = g_filename_display_name(path);
        char* msg = g_strdup_printf("Unable to open file:\n\"%s\"\n%s", disp_path, error_msg);
        g_free(disp_path);
        ptk_show_error(nullptr, "Error", msg);
        g_free(msg);
        if (err)
            g_error_free(err);
    }
    vfs_mime_type_unref(mime_type);
    vfs_file_info_unref(file);
}
