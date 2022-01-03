/*
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

/* turn on to debug GDK_THREADS_ENTER/GDK_THREADS_LEAVE related deadlocks */
#undef _DEBUG_THREAD

#include <csignal>

#include <string>
#include <filesystem>

#include <iostream>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/sysmacros.h>

#include "main-window.hxx"

#include "window-reference.hxx"

#include "vfs/vfs-file-info.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-app-desktop.hxx"

#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-file-monitor.hxx"
#include "vfs/vfs-volume.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-location-view.hxx"

#include "autosave.hxx"
#include "find-files.hxx"
#include "pref-dialog.hxx"
#include "settings.hxx"

#include "logger.hxx"
#include "utils.hxx"

// bool startup_mode = true;  //MOD
// bool design_mode = true;  //MOD

enum SocketEvent
{
    CMD_OPEN = 1,
    CMD_OPEN_TAB,
    CMD_REUSE_TAB,
    CMD_DAEMON_MODE,
    CMD_FIND_FILES,
    CMD_OPEN_PANEL1,
    CMD_OPEN_PANEL2,
    CMD_OPEN_PANEL3,
    CMD_OPEN_PANEL4,
    CMD_PANEL1,
    CMD_PANEL2,
    CMD_PANEL3,
    CMD_PANEL4,
    CMD_NO_TABS,
    CMD_SOCKET_CMD,
};

static bool folder_initialized = false;
static bool daemon_initialized = false;

static int sock;
static GIOChannel* io_channel = nullptr;

static bool socket_daemon = false; // sfm

static char* default_files[2] = {nullptr, nullptr};

struct CliFlags
{
    char** files{nullptr};
    bool new_tab{true};
    bool reuse_tab{false};
    bool no_tabs{false};
    bool new_window{false};
    bool socket_cmd{false};
    bool version_opt{false};

    bool daemon_mode{false};

    int panel{0};

    bool find_files{false};
    char* config_dir{nullptr};
    bool disable_git_settings{false};
};

CliFlags cli_flags;

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

static void init_folder();
static bool handle_parsed_commandline_args();
static void open_file(const char* path);
static char* dup_to_absolute_file_path(char** file);

// SOCKET START

static bool single_instance_check();
static void single_instance_finalize();
static void get_socket_name(char* buf, int len);
static bool on_socket_event(GIOChannel* ioc, GIOCondition cond, void* data);
static void receive_socket_command(int client, GString* args);

static char*
get_inode_tag()
{
    struct stat stat_buf;

    const char* path = vfs_user_home_dir();
    if (!path || stat(path, &stat_buf) == -1)
        return g_strdup_printf("%d=", getuid());
    return g_strdup_printf("%d=%d:%d-%ld",
                           getuid(),
                           major(stat_buf.st_dev),
                           minor(stat_buf.st_dev),
                           stat_buf.st_ino);
}

static bool
on_socket_event(GIOChannel* ioc, GIOCondition cond, void* data)
{
    (void)data;
    if (cond & G_IO_IN)
    {
        socklen_t addr_len = 0;
        struct sockaddr_un client_addr;

        int client =
            accept(g_io_channel_unix_get_fd(ioc), (struct sockaddr*)&client_addr, &addr_len);
        if (client != -1)
        {
            static char buf[1024];
            GString* args = g_string_new_len(nullptr, 2048);
            int r;
            while ((r = read(client, buf, sizeof(buf))) > 0)
            {
                g_string_append_len(args, buf, r);
                if (args->str[0] == CMD_SOCKET_CMD && args->len > 1 &&
                    args->str[args->len - 2] == '\n' && args->str[args->len - 1] == '\n')
                    // because CMD_SOCKET_CMD doesn't immediately close the socket
                    // data is terminated by two linefeeds to prevent read blocking
                    break;
            }
            if (args->str[0] == CMD_SOCKET_CMD)
                receive_socket_command(client, args);
            shutdown(client, 2);
            close(client);

            cli_flags.new_tab = true;
            cli_flags.panel = 0;
            cli_flags.reuse_tab = false;
            cli_flags.no_tabs = false;
            socket_daemon = false;

            int argx = 0;
            if (args->str[argx] == CMD_NO_TABS)
            {
                cli_flags.reuse_tab = false;
                cli_flags.no_tabs = true;
                argx++; // another command follows CMD_NO_TABS
            }
            if (args->str[argx] == CMD_REUSE_TAB)
            {
                cli_flags.reuse_tab = true;
                cli_flags.new_tab = false;
                argx++; // another command follows CMD_REUSE_TAB
            }

            switch (args->str[argx])
            {
                case CMD_PANEL1:
                    cli_flags.panel = 1;
                    break;
                case CMD_PANEL2:
                    cli_flags.panel = 2;
                    break;
                case CMD_PANEL3:
                    cli_flags.panel = 3;
                    break;
                case CMD_PANEL4:
                    cli_flags.panel = 4;
                    break;
                case CMD_OPEN:
                    cli_flags.new_tab = false;
                    break;
                case CMD_OPEN_PANEL1:
                    cli_flags.new_tab = false;
                    cli_flags.panel = 1;
                    break;
                case CMD_OPEN_PANEL2:
                    cli_flags.new_tab = false;
                    cli_flags.panel = 2;
                    break;
                case CMD_OPEN_PANEL3:
                    cli_flags.new_tab = false;
                    cli_flags.panel = 3;
                    break;
                case CMD_OPEN_PANEL4:
                    cli_flags.new_tab = false;
                    cli_flags.panel = 4;
                    break;
                case CMD_DAEMON_MODE:
                    socket_daemon = cli_flags.daemon_mode = true;
                    g_string_free(args, true);
                    return true;
                case CMD_FIND_FILES:
                    cli_flags.find_files = true;
                    __attribute__((fallthrough));
                case CMD_SOCKET_CMD:
                    g_string_free(args, true);
                    return true;
                default:
                    break;
            }

            if (args->str[argx + 1])
                cli_flags.files = g_strsplit(args->str + argx + 1, "\n", 0);
            else
                cli_flags.files = nullptr;
            g_string_free(args, true);

            if (cli_flags.files)
            {
                char** file;
                for (file = cli_flags.files; *file; ++file)
                {
                    if (!**file) /* remove empty string at tail */
                        *file = nullptr;
                }
            }
            handle_parsed_commandline_args();
            app_settings.load_saved_tabs = true;
            socket_daemon = false;
        }
    }

    return true;
}

static void
get_socket_name(char* buf, int len)
{
    char* dpy = g_strdup(g_getenv("DISPLAY"));
    if (dpy && !strcmp(dpy, ":0.0"))
    {
        // treat :0.0 as :0 to prevent multiple instances on screen 0
        g_free(dpy);
        dpy = g_strdup(":0");
    }
    g_snprintf(buf, len, "%s/spacefm-%s%s.socket", vfs_user_runtime_dir(), g_get_user_name(), dpy);
    g_free(dpy);
}

static void
single_instance_check_fatal(int ret)
{
    gdk_notify_startup_complete();
    exit(ret);
}

static bool
single_instance_check()
{
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        LOG_ERROR("socket init failure");
        single_instance_check_fatal(EXIT_FAILURE);
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    get_socket_name(addr.sun_path, sizeof(addr.sun_path));
    int addr_len = SUN_LEN(&addr);

    /* try to connect to existing instance */
    if (sock && connect(sock, (struct sockaddr*)&addr, addr_len) == 0)
    {
        /* connected successfully */
        char cmd = CMD_OPEN_TAB;

        if (cli_flags.no_tabs)
        {
            cmd = CMD_NO_TABS;
            write(sock, &cmd, sizeof(char));
            // another command always follows CMD_NO_TABS
            cmd = CMD_OPEN_TAB;
        }
        if (cli_flags.reuse_tab)
        {
            cmd = CMD_REUSE_TAB;
            write(sock, &cmd, sizeof(char));
            // another command always follows CMD_REUSE_TAB
            cmd = CMD_OPEN;
        }

        if (cli_flags.daemon_mode)
            cmd = CMD_DAEMON_MODE;
        else if (cli_flags.new_window)
        {
            if (cli_flags.panel > 0 && cli_flags.panel < 5)
                cmd = CMD_OPEN_PANEL1 + cli_flags.panel - 1;
            else
                cmd = CMD_OPEN;
        }
        else if (cli_flags.find_files)
            cmd = CMD_FIND_FILES;
        else if (cli_flags.panel > 0 && cli_flags.panel < 5)
            cmd = CMD_PANEL1 + cli_flags.panel - 1;

        // open a new window if no file spec
        if (cmd == CMD_OPEN_TAB && !cli_flags.files)
            cmd = CMD_OPEN;

        write(sock, &cmd, sizeof(char));

        if (cli_flags.files)
        {
            char** file;
            for (file = cli_flags.files; *file; ++file)
            {
                char* real_path;

                if ((*file[0] != '/' && strstr(*file, ":/")) || g_str_has_prefix(*file, "//"))
                    real_path = g_strdup(*file);
                else
                {
                    /* We send absolute paths because with different
                       $PWDs resolution would not work. */
                    real_path = dup_to_absolute_file_path(file);
                }
                write(sock, real_path, strlen(real_path));
                g_free(real_path);
                write(sock, "\n", 1);
            }
        }

        if (cli_flags.config_dir)
            LOG_WARN("Option --config ignored - an instance is already running");
        shutdown(sock, 2);
        close(sock);
        single_instance_check_fatal(EXIT_SUCCESS);
    }

    /* There is no existing server, and we are in the first instance. */
    unlink(addr.sun_path); /* delete old socket file if it exists. */
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(sock, (struct sockaddr*)&addr, addr_len) == -1)
    {
        LOG_WARN("could not create socket {}", addr.sun_path);
        // could still run partially without this
        single_instance_check_fatal(EXIT_FAILURE);
    }
    else
    {
        io_channel = g_io_channel_unix_new(sock);
        g_io_channel_set_encoding(io_channel, nullptr, nullptr);
        g_io_channel_set_buffered(io_channel, false);

        g_io_add_watch(io_channel, G_IO_IN, (GIOFunc)on_socket_event, nullptr);

        if (listen(sock, 5) == -1)
        {
            LOG_WARN("could not listen to socket");
            single_instance_check_fatal(EXIT_FAILURE);
        }
    }

    return true;
}

static void
single_instance_finalize()
{
    char lock_file[256];

    shutdown(sock, 2);
    g_io_channel_unref(io_channel);
    close(sock);

    get_socket_name(lock_file, sizeof(lock_file));
    unlink(lock_file);
}

static void
receive_socket_command(int client, GString* args) // sfm
{
    char** argv;
    char cmd;
    char* reply = nullptr;

    if (args->str[1])
    {
        if (g_str_has_suffix(args->str, "\n\n"))
        {
            // remove empty strings at tail
            args->str[args->len - 1] = '\0';
            args->str[args->len - 2] = '\0';
        }
        argv = g_strsplit(args->str + 1, "\n", 0);
    }
    else
        argv = nullptr;

    // check inode tag - was socket command sent from the same filesystem?
    // eg this helps deter use of socket commands sent from a chroot jail
    // or from another user or system
    char* inode_tag = get_inode_tag();
    if (argv && strcmp(inode_tag, argv[0]))
    {
        reply = g_strdup("spacefm: invalid socket command user\n");
        cmd = 1;
        LOG_WARN("invalid socket command user");
    }
    else
    {
        // process command and get reply
        cmd = main_window_socket_command(argv ? argv + 1 : nullptr, &reply);
    }
    g_strfreev(argv);
    g_free(inode_tag);

    // send response
    write(client, &cmd, sizeof(char)); // send exit status
    if (reply && reply[0])
        write(client, reply, strlen(reply)); // send reply or error msg
    g_free(reply);
}

static int
send_socket_command(int argc, char* argv[], char** reply) // sfm
{
    *reply = nullptr;
    if (argc < 3)
    {
        LOG_ERROR("socket-cmd requires an argument");
        return EXIT_FAILURE;
    }

    // create socket
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        LOG_ERROR("failed to create socket");
        return EXIT_FAILURE;
    }

    // open socket
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    get_socket_name(addr.sun_path, sizeof(addr.sun_path));
    int addr_len = SUN_LEN(&addr);

    if (connect(sock, (struct sockaddr*)&addr, addr_len) != 0)
    {
        LOG_ERROR("could not connect to socket (not running? or DISPLAY not set?)");
        return EXIT_FAILURE;
    }

    // send command
    char cmd = CMD_SOCKET_CMD;
    write(sock, &cmd, sizeof(char));

    // send inode tag
    char* inode_tag = get_inode_tag();
    write(sock, inode_tag, strlen(inode_tag));
    write(sock, "\n", 1);
    g_free(inode_tag);

    // send arguments
    int i;
    for (i = 2; i < argc; i++)
    {
        write(sock, argv[i], strlen(argv[i]));
        write(sock, "\n", 1);
    }
    write(sock, "\n", 1);

    // get response
    GString* sock_reply = g_string_new_len(nullptr, 2048);
    int r;
    static char buf[1024];

    while ((r = read(sock, buf, sizeof(buf))) > 0)
        g_string_append_len(sock_reply, buf, r);

    // close socket
    shutdown(sock, 2);
    close(sock);

    // set reply
    int ret;
    if (sock_reply->len != 0)
    {
        *reply = g_strdup(sock_reply->str + 1);
        ret = sock_reply->str[0];
    }
    else
    {
        LOG_ERROR("invalid response from socket");
        ret = 1;
    }
    g_string_free(sock_reply, true);
    return ret;
}

// SOCKET END

#ifdef _DEBUG_THREAD

G_LOCK_DEFINE(gdk_lock);
void
debug_gdk_threads_enter(const char* message)
{
    LOG_DEBUG("Thread {:p} tries to get GDK lock: {}", g_thread_self(), message);
    G_LOCK(gdk_lock);
    LOG_DEBUG("Thread {:p} got GDK lock: {}", g_thread_self(), message);
}

static void
_debug_gdk_threads_enter()
{
    debug_gdk_threads_enter("called from GTK+ internal");
}

void
debug_gdk_threads_leave(const char* message)
{
    LOG_DEBUG("Thread {:p} tries to release GDK lock: {}", g_thread_self(), message);
    G_UNLOCK(gdk_lock);
    LOG_DEBUG("Thread {:p} released GDK lock: {}", g_thread_self(), message);
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
    if (G_LIKELY(folder_initialized))
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
    init_folder();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, exit_from_signal);
    signal(SIGINT, exit_from_signal);
    signal(SIGTERM, exit_from_signal);

    daemon_initialized = true;
}

static char*
dup_to_absolute_file_path(char** file)
{
    char* file_path;
    char* real_path;
    void* cwd_path;
    const size_t cwd_size = PATH_MAX;

    if (g_str_has_prefix(*file, "file:")) /* It's a URI */
    {
        file_path = g_filename_from_uri(*file, nullptr, nullptr);
        g_free(*file);
        *file = file_path;
    }
    else
        file_path = *file;

    cwd_path = malloc(cwd_size);
    if (cwd_path)
    {
        getcwd((char*)cwd_path, cwd_size);
    }

    real_path = vfs_file_resolve_path((char*)cwd_path, file_path);
    free(cwd_path);
    cwd_path = nullptr;

    return real_path; /* To free with g_free */
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
    FMMainWindow* main_window = nullptr;
    char** file;
    bool ret = true;
    XSet* set;
    struct stat statbuf;

    app_settings.load_saved_tabs = !cli_flags.no_tabs;

    // If no files are specified, open home dir by defualt.
    if (G_LIKELY(!cli_flags.files))
    {
        cli_flags.files = default_files;
        // files[0] = (char*)vfs_user_home_dir();
    }

    // get the last active window on this desktop, if available
    if (cli_flags.new_tab || cli_flags.reuse_tab)
    {
        main_window = fm_main_window_get_on_current_desktop();
        // LOG_INFO("    fm_main_window_get_on_current_desktop = {:p}  {} {}", main_window,
        //               new_tab ? "new_tab" : "", reuse_tab ? "reuse_tab" : "");
    }

    if (cli_flags.find_files) /* find files */
    {
        init_folder();
        fm_find_files((const char**)cli_flags.files);
        cli_flags.find_files = false;
    }
    else /* open files/directories */
    {
        if (cli_flags.daemon_mode && !daemon_initialized)
            init_daemon();
        else if (cli_flags.files != default_files)
        {
            /* open files passed in command line arguments */
            ret = false;
            for (file = cli_flags.files; *file; ++file)
            {
                char* real_path;

                if (!**file) /* skip empty string */
                    continue;

                real_path = dup_to_absolute_file_path(file);

                if (std::filesystem::is_directory(real_path))
                {
                    open_in_tab(&main_window, real_path);
                    ret = true;
                }
                else if (std::filesystem::exists(real_path))
                {
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
                    char* err_msg = g_strdup_printf("%s:\n\n%s", "File doesn't exist", real_path);
                    ptk_show_error(nullptr, "Error", err_msg);
                    g_free(err_msg);
                }
                g_free(real_path);
            }
        }
        else if (!socket_daemon)
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
                    set = xset_get_panel(cli_flags.panel, "show");
                    set->b = XSET_B_TRUE;
                    show_panels_all_windows(nullptr, main_window);
                }
                focus_panel(nullptr, (void*)main_window, cli_flags.panel);
            }
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
    std::string command = fmt::format("rm -rf {}", xset_get_user_tmp_dir());
    print_command(command);
    g_spawn_command_line_async(command.c_str(), nullptr);
}

int
main(int argc, char* argv[])
{
    SpaceFM::Logger::Init();

    bool run = false;
    GError* err = nullptr;

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
                std::cout << "For help run, man spacefm-socket" << std::endl;
                return EXIT_SUCCESS;
            }
            char* reply = nullptr;
            int ret = send_socket_command(argc, argv, &reply);
            if (reply && reply[0])
                LOG_ERROR("%s", reply);
            g_free(reply);
            return ret;
        }
    }

    /* initialize GTK+ and parse the command line arguments */
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
        std::cout << fmt::format("{} {} Copyright (C) 2022", PACKAGE_NAME_FANCY, PACKAGE_VERSION)
                  << std::endl;
        return EXIT_SUCCESS;
    }

    /* Initialize multithreading  //sfm moved below parse arguments
         No matter we use threads or not, it's safer to initialize this earlier. */
#ifdef _DEBUG_THREAD
    gdk_threads_set_lock_functions(_debug_gdk_threads_enter, _debug_gdk_threads_leave);
#endif

    /* ensure that there is only one instance of spacefm.
         if there is an existing instance, command line arguments
         will be passed to the existing instance, and exit() will be called here.  */
    single_instance_check();

    /* initialize the file alteration monitor */
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

    /* Initialize our mime-type system */
    vfs_mime_type_init();

    /* load config file */
    // MOD was before vfs_file_monitor_init
    load_settings(cli_flags.config_dir);

    // start autosave thread
    autosave_init();

    /* If we reach this point, we are the first instance.
     * Subsequent processes will exit() inside single_instance_check and won't reach here.
     */

    main_window_event(nullptr, nullptr, "evt_start", 0, 0, nullptr, 0, 0, 0, false);

    /* handle the parsed result of command line args */
    run = handle_parsed_commandline_args();
    app_settings.load_saved_tabs = true;

    if (run) /* run the main loop */
        gtk_main();

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
        app_name = (char*)ptk_choose_app_for_mime_type(nullptr, mime_type, true, true, true, false);
        if (app_name)
        {
            VFSAppDesktop* app = vfs_app_desktop_new(app_name);
            if (!vfs_app_desktop_get_exec(app))
                app->exec = g_strdup(app_name); /* This is a command line */
            GList* files = g_list_prepend(nullptr, (void*)path);
            opened =
                vfs_app_desktop_open_files(gdk_screen_get_default(), nullptr, app, files, &err);
            g_free(files->data);
            g_list_free(files);
            vfs_app_desktop_unref(app);
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
