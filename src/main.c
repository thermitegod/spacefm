/*
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

/* turn on to debug GDK_THREADS_ENTER/GDK_THREADS_LEAVE related deadlocks */
#undef _DEBUG_THREAD

#include <stdbool.h>

#include "private.h"

#include <gtk/gtk.h>
#include <glib.h>

#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <sys/sysmacros.h>

#include <signal.h>

#include <unistd.h>

#include <locale.h>

#include <linux/limits.h>

#include "main-window.h"

#include "vfs/vfs-file-info.h"
#include "vfs/vfs-mime-type.h"
#include "vfs/vfs-app-desktop.h"

#include "vfs/vfs-file-monitor.h"
#include "vfs/vfs-volume.h"
#include "vfs/vfs-thumbnail-loader.h"

#include "ptk/ptk-utils.h"
#include "ptk/ptk-app-chooser.h"
#include "ptk/ptk-location-view.h"

#include "find-files.h"
#include "pref-dialog.h"
#include "settings.h"

#include "utils.h"

#include "cust-dialog.h"

// bool startup_mode = TRUE;  //MOD
// bool design_mode = TRUE;  //MOD

typedef enum SocketEvent
{
    CMD_OPEN = 1,
    CMD_OPEN_TAB,
    CMD_REUSE_TAB,
    CMD_DAEMON_MODE,
    CMD_PREF,
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
} SocketEvent;

static bool folder_initialized = FALSE;
static bool daemon_initialized = FALSE;

static int sock;
static GIOChannel* io_channel = NULL;

static bool daemon_mode = FALSE;

static char* default_files[2] = {NULL, NULL};
static char** files = NULL;

static bool new_tab = TRUE;
static bool reuse_tab = FALSE; // sfm
static bool no_tabs = FALSE;   // sfm
static bool new_window = FALSE;
static bool custom_dialog = FALSE; // sfm
static bool socket_cmd = FALSE;    // sfm
static bool version_opt = FALSE;   // sfm
static bool sdebug = FALSE;        // sfm
static bool socket_daemon = FALSE; // sfm

static int show_pref = 0;
static int panel = -1;

static bool find_files = FALSE;
static char* config_dir = NULL;
static bool disable_git_settings = FALSE;

static int n_pcmanfm_ref = 0;

// clang-format off
static GOptionEntry opt_entries[] =
{
    {"new-tab", 't', 0, G_OPTION_ARG_NONE, &new_tab, N_("Open directories in new tab of last window (default)"), NULL},
    {"reuse-tab", 'r', 0, G_OPTION_ARG_NONE, &reuse_tab, N_("Open directory in current tab of last used window"), NULL},
    {"no-saved-tabs", 'n', 0, G_OPTION_ARG_NONE, &no_tabs, N_("Don't load saved tabs"), NULL},
    {"new-window", 'w', 0, G_OPTION_ARG_NONE, &new_window, N_("Open directories in new window"), NULL},
    {"panel", 'p', 0, G_OPTION_ARG_INT, &panel, N_("Open directories in panel 'P' (1-4)"), "P"},
    {"show-pref", '\0', 0, G_OPTION_ARG_INT, &show_pref, N_("Show Preferences ('N' is the Pref tab number)"), "N"},
    {"daemon-mode", 'd', 0, G_OPTION_ARG_NONE, &daemon_mode, N_("Run as a daemon"), NULL},
    {"config", 'c', 0, G_OPTION_ARG_STRING, &config_dir, N_("Use DIR as configuration directory"), "DIR"},
    {"disable-git", 'G', 0, G_OPTION_ARG_NONE, &disable_git_settings, N_("Don't use git to keep session history"), NULL},
    {"find-files", 'f', 0, G_OPTION_ARG_NONE, &find_files, N_("Show File Search"), NULL},
    {"dialog", 'g', 0, G_OPTION_ARG_NONE, &custom_dialog, N_("Show a custom dialog (See -g help)"), NULL},
    {"socket-cmd", 's', 0, G_OPTION_ARG_NONE, &socket_cmd, N_("Send a socket command (See -s help)"), NULL},
    {"version", 'v', 0, G_OPTION_ARG_NONE, &version_opt, N_("Show version information"), NULL},
    {"sdebug", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &sdebug, NULL, NULL},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, N_("[DIR | FILE | URL]...")},
    {NULL}
};
// clang-format on

static bool single_instance_check();
static void single_instance_finalize();
static void get_socket_name(char* buf, int len);
static bool on_socket_event(GIOChannel* ioc, GIOCondition cond, void* data);

static void init_folder();

static bool handle_parsed_commandline_args();

static void open_file(const char* path);

static char* dup_to_absolute_file_path(char** file);
static void receive_socket_command(int client, GString* args); // sfm

static char* get_inode_tag()
{
    struct stat stat_buf;

    const char* path = g_get_home_dir();
    if (!path || stat(path, &stat_buf) == -1)
        return g_strdup_printf("%d=", getuid());
    return g_strdup_printf("%d=%d:%d-%ld",
                           getuid(),
                           major(stat_buf.st_dev),
                           minor(stat_buf.st_dev),
                           stat_buf.st_ino);
}

static bool on_socket_event(GIOChannel* ioc, GIOCondition cond, void* data)
{
    if (cond & G_IO_IN)
    {
        socklen_t addr_len = 0;
        struct sockaddr_un client_addr = {0};

        int client =
            accept(g_io_channel_unix_get_fd(ioc), (struct sockaddr*)&client_addr, &addr_len);
        if (client != -1)
        {
            static char buf[1024];
            GString* args = g_string_new_len(NULL, 2048);
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

            new_tab = TRUE;
            panel = 0;
            reuse_tab = FALSE;
            no_tabs = FALSE;
            socket_daemon = FALSE;

            int argx = 0;
            if (args->str[argx] == CMD_NO_TABS)
            {
                reuse_tab = FALSE;
                no_tabs = TRUE;
                argx++; // another command follows CMD_NO_TABS
            }
            if (args->str[argx] == CMD_REUSE_TAB)
            {
                reuse_tab = TRUE;
                new_tab = FALSE;
                argx++; // another command follows CMD_REUSE_TAB
            }

            switch (args->str[argx])
            {
                case CMD_PANEL1:
                    panel = 1;
                    break;
                case CMD_PANEL2:
                    panel = 2;
                    break;
                case CMD_PANEL3:
                    panel = 3;
                    break;
                case CMD_PANEL4:
                    panel = 4;
                    break;
                case CMD_OPEN:
                    new_tab = FALSE;
                    break;
                case CMD_OPEN_PANEL1:
                    new_tab = FALSE;
                    panel = 1;
                    break;
                case CMD_OPEN_PANEL2:
                    new_tab = FALSE;
                    panel = 2;
                    break;
                case CMD_OPEN_PANEL3:
                    new_tab = FALSE;
                    panel = 3;
                    break;
                case CMD_OPEN_PANEL4:
                    new_tab = FALSE;
                    panel = 4;
                    break;
                case CMD_DAEMON_MODE:
                    socket_daemon = daemon_mode = TRUE;
                    g_string_free(args, TRUE);
                    return TRUE;
                case CMD_PREF:
                    fm_edit_preference(NULL, (unsigned char)args->str[1] - 1);
                    g_string_free(args, TRUE);
                    return TRUE;
                case CMD_FIND_FILES:
                    find_files = TRUE;
                    __attribute__((fallthrough));
                case CMD_SOCKET_CMD:
                    g_string_free(args, TRUE);
                    return TRUE;
                default:
                    break;
            }

            if (args->str[argx + 1])
                files = g_strsplit(args->str + argx + 1, "\n", 0);
            else
                files = NULL;
            g_string_free(args, TRUE);

            if (files)
            {
                char** file;
                for (file = files; *file; ++file)
                {
                    if (!**file) /* remove empty string at tail */
                        *file = NULL;
                }
            }
            handle_parsed_commandline_args();
            app_settings.load_saved_tabs = TRUE;
            socket_daemon = FALSE;
        }
    }

    return TRUE;
}

static void get_socket_name(char* buf, int len)
{
    char* dpy = g_strdup(g_getenv("DISPLAY"));
    if (dpy && !strcmp(dpy, ":0.0"))
    {
        // treat :0.0 as :0 to prevent multiple instances on screen 0
        g_free(dpy);
        dpy = g_strdup(":0");
    }
    g_snprintf(buf, len, "/run/spacefm-%s%s.socket", g_get_user_name(), dpy);
    g_free(dpy);
}

static bool single_instance_check()
{
    int ret;

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "spacefm: socket init failure\n");
        ret = 1;
        goto _exit;
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

        if (no_tabs)
        {
            cmd = CMD_NO_TABS;
            write(sock, &cmd, sizeof(char));
            // another command always follows CMD_NO_TABS
            cmd = CMD_OPEN_TAB;
        }
        if (reuse_tab)
        {
            cmd = CMD_REUSE_TAB;
            write(sock, &cmd, sizeof(char));
            // another command always follows CMD_REUSE_TAB
            cmd = CMD_OPEN;
        }

        if (daemon_mode)
            cmd = CMD_DAEMON_MODE;
        else if (new_window)
        {
            if (panel > 0 && panel < 5)
                cmd = CMD_OPEN_PANEL1 + panel - 1;
            else
                cmd = CMD_OPEN;
        }
        else if (show_pref > 0)
            cmd = CMD_PREF;
        else if (find_files)
            cmd = CMD_FIND_FILES;
        else if (panel > 0 && panel < 5)
            cmd = CMD_PANEL1 + panel - 1;

        // open a new window if no file spec
        if (cmd == CMD_OPEN_TAB && !files)
            cmd = CMD_OPEN;

        write(sock, &cmd, sizeof(char));
        if (G_UNLIKELY(show_pref > 0))
        {
            cmd = (unsigned char)show_pref;
            write(sock, &cmd, sizeof(char));
        }
        else
        {
            if (files)
            {
                char** file;
                for (file = files; *file; ++file)
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
        }
        if (config_dir)
            g_warning(_("Option --config ignored - an instance is already running"));
        shutdown(sock, 2);
        close(sock);
        ret = 0;
        goto _exit;
    }

    /* There is no existing server, and we are in the first instance. */
    unlink(addr.sun_path); /* delete old socket file if it exists. */
    int reuse = 1;
    ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(sock, (struct sockaddr*)&addr, addr_len) == -1)
    {
        g_warning("could not create socket %s", addr.sun_path);
        // can still run partially without this
        // ret = 1;
        // goto _exit;
    }
    else
    {
        io_channel = g_io_channel_unix_new(sock);
        g_io_channel_set_encoding(io_channel, NULL, NULL);
        g_io_channel_set_buffered(io_channel, FALSE);

        g_io_add_watch(io_channel, G_IO_IN, (GIOFunc)on_socket_event, NULL);

        if (listen(sock, 5) == -1)
        {
            g_warning("could not listen to socket");
            // ret = 1;
            // goto _exit;
        }
    }

    return TRUE;

_exit:

    gdk_notify_startup_complete();
    exit(ret);
}

static void single_instance_finalize()
{
    char lock_file[256];

    shutdown(sock, 2);
    g_io_channel_unref(io_channel);
    close(sock);

    get_socket_name(lock_file, sizeof(lock_file));
    unlink(lock_file);
}

static void receive_socket_command(int client, GString* args) // sfm
{
    char** argv;
    char cmd;
    char* reply = NULL;

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
        argv = NULL;

    /*
        if ( argv )
        {
            printf( "receive:\n");
            for ( arg = argv; *arg; ++arg )
            {
                if ( ! **arg )  // skip empty string
                {
                    printf( "    (skipped empty)\n");
                    continue;
                }
                printf( "    %s\n", *arg );
            }
        }
    */

    // check inode tag - was socket command sent from the same filesystem?
    // eg this helps deter use of socket commands sent from a chroot jail
    // or from another user or system
    char* inode_tag = get_inode_tag();
    if (argv && strcmp(inode_tag, argv[0]))
    {
        reply = g_strdup("spacefm: invalid socket command user\n");
        cmd = 1;
        g_warning("invalid socket command user");
    }
    else
    {
        // process command and get reply
        cmd = main_window_socket_command(argv ? argv + 1 : NULL, &reply);
    }
    g_strfreev(argv);
    g_free(inode_tag);

    // send response
    write(client, &cmd, sizeof(char)); // send exit status
    if (reply && reply[0])
        write(client, reply, strlen(reply)); // send reply or error msg
    g_free(reply);
}

static int send_socket_command(int argc, char* argv[], char** reply) // sfm
{
    *reply = NULL;
    if (argc < 3)
    {
        fprintf(stderr, _("spacefm: --socket-cmd requires an argument\n"));
        return 1;
    }

    // create socket
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, _("spacefm: could not create socket\n"));
        return 1;
    }

    // open socket
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    get_socket_name(addr.sun_path, sizeof(addr.sun_path));
    int addr_len = SUN_LEN(&addr);

    if (connect(sock, (struct sockaddr*)&addr, addr_len) != 0)
    {
        fprintf(stderr,
                _("spacefm: could not connect to socket (not running? or DISPLAY not set?)\n"));
        return 1;
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
    GString* sock_reply = g_string_new_len(NULL, 2048);
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
        fprintf(stderr, _("spacefm: invalid response from socket\n"));
        ret = 1;
    }
    g_string_free(sock_reply, TRUE);
    return ret;
}

#ifdef _DEBUG_THREAD

G_LOCK_DEFINE(gdk_lock);
void debug_gdk_threads_enter(const char* message)
{
    g_debug("Thread %p tries to get GDK lock: %s", g_thread_self(), message);
    G_LOCK(gdk_lock);
    g_debug("Thread %p got GDK lock: %s", g_thread_self(), message);
}

static void _debug_gdk_threads_enter()
{
    debug_gdk_threads_enter("called from GTK+ internal");
}

void debug_gdk_threads_leave(const char* message)
{
    g_debug("Thread %p tries to release GDK lock: %s", g_thread_self(), message);
    G_UNLOCK(gdk_lock);
    g_debug("Thread %p released GDK lock: %s", g_thread_self(), message);
}

static void _debug_gdk_threads_leave()
{
    debug_gdk_threads_leave("called from GTK+ internal");
}
#endif

static void init_folder()
{
    if (G_LIKELY(folder_initialized))
        return;

    vfs_volume_init();
    vfs_thumbnail_init();

    vfs_mime_type_set_icon_size(app_settings.big_icon_size, app_settings.small_icon_size);
    vfs_file_info_set_thumbnail_size(app_settings.big_icon_size, app_settings.small_icon_size);

    folder_initialized = TRUE;
}

static void init_daemon()
{
    init_folder();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, (void*)gtk_main_quit);
    signal(SIGINT, (void*)gtk_main_quit);
    signal(SIGTERM, (void*)gtk_main_quit);

    daemon_initialized = TRUE;
}

static char* dup_to_absolute_file_path(char** file)
{
    char* file_path;
    char* real_path;
    char* cwd_path;
    const size_t cwd_size = PATH_MAX;

    if (g_str_has_prefix(*file, "file:")) /* It's a URI */
    {
        file_path = g_filename_from_uri(*file, NULL, NULL);
        g_free(*file);
        *file = file_path;
    }
    else
        file_path = *file;

    cwd_path = malloc(cwd_size);
    if (cwd_path)
    {
        getcwd(cwd_path, cwd_size);
    }

    real_path = vfs_file_resolve_path(cwd_path, file_path);
    free(cwd_path);
    cwd_path = NULL;

    return real_path; /* To free with g_free */
}

static void open_in_tab(FMMainWindow** main_window, const char* real_path)
{
    XSet* set;
    int p;
    // create main window if needed
    if (G_UNLIKELY(!*main_window))
    {
        // initialize things required by folder view
        if (G_UNLIKELY(!daemon_mode))
            init_folder();

        // preload panel?
        if (panel > 0 && panel < 5)
            // user specified panel
            p = panel;
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
        fm_main_window_store_positions(NULL);
        *main_window = FM_MAIN_WINDOW(fm_main_window_new());
    }
    else
    {
        // existing window
        bool tab_added = FALSE;
        if (panel > 0 && panel < 5)
        {
            // change to user-specified panel
            if (!gtk_notebook_get_n_pages(GTK_NOTEBOOK((*main_window)->panel[panel - 1])))
            {
                // set panel to load real_path on panel load
                set = xset_get_panel(panel, "show");
                set->ob1 = g_strdup(real_path);
                tab_added = TRUE;
                set->b = XSET_B_TRUE;
                show_panels_all_windows(NULL, *main_window);
            }
            else if (!gtk_widget_get_visible((*main_window)->panel[panel - 1]))
            {
                // show panel
                set = xset_get_panel(panel, "show");
                set->b = XSET_B_TRUE;
                show_panels_all_windows(NULL, *main_window);
            }
            (*main_window)->curpanel = panel;
            (*main_window)->notebook = (*main_window)->panel[panel - 1];
        }
        if (!tab_added)
        {
            if (reuse_tab)
            {
                main_window_open_path_in_current_tab(*main_window, real_path);
                reuse_tab = FALSE;
            }
            else
                fm_main_window_add_new_tab(*main_window, real_path);
        }
    }
    gtk_window_present(GTK_WINDOW(*main_window));
}

static bool handle_parsed_commandline_args()
{
    FMMainWindow* main_window = NULL;
    char** file;
    bool ret = TRUE;
    XSet* set;
    struct stat statbuf;

    app_settings.load_saved_tabs = !no_tabs;

    // If no files are specified, open home dir by defualt.
    if (G_LIKELY(!files))
    {
        files = default_files;
        // files[0] = (char*)g_get_home_dir();
    }

    // get the last active window on this desktop, if available
    if (new_tab || reuse_tab)
    {
        main_window = fm_main_window_get_on_current_desktop();
        // printf("    fm_main_window_get_on_current_desktop = %p  %s %s\n", main_window,
        //                                                            new_tab ? "new_tab" : "",
        //                                                            reuse_tab ? "reuse_tab" : ""
        //                                                            );
    }

    if (show_pref > 0) /* show preferences dialog */
    {
        fm_edit_preference(GTK_WINDOW(main_window), show_pref - 1);
        show_pref = 0;
    }
    else if (find_files) /* find files */
    {
        init_folder();
        fm_find_files((const char**)files);
        find_files = FALSE;
    }
    else /* open files/directories */
    {
        if (daemon_mode && !daemon_initialized)
            init_daemon();
        else if (files != default_files)
        {
            /* open files passed in command line arguments */
            ret = FALSE;
            for (file = files; *file; ++file)
            {
                char* real_path;

                if (!**file) /* skip empty string */
                    continue;

                real_path = dup_to_absolute_file_path(file);

                if (g_file_test(real_path, G_FILE_TEST_IS_DIR))
                {
                    open_in_tab(&main_window, real_path);
                    ret = TRUE;
                }
                else if (g_file_test(real_path, G_FILE_TEST_EXISTS))
                {
                    if (stat(real_path, &statbuf) == 0 && S_ISBLK(statbuf.st_mode))
                    {
                        // open block device eg /dev/sda1
                        if (!main_window)
                        {
                            open_in_tab(&main_window, "/");
                            ptk_location_view_open_block(real_path, FALSE);
                        }
                        else
                            ptk_location_view_open_block(real_path, TRUE);
                        ret = TRUE;
                        gtk_window_present(GTK_WINDOW(main_window));
                    }
                    else
                        open_file(real_path);
                }
                else if ((*file[0] != '/' && strstr(*file, ":/")) || g_str_has_prefix(*file, "//"))
                {
                    if (main_window)
                        main_window_open_network(main_window, *file, TRUE);
                    else
                    {
                        open_in_tab(&main_window, "/");
                        main_window_open_network(main_window, *file, FALSE);
                    }
                    ret = TRUE;
                    gtk_window_present(GTK_WINDOW(main_window));
                }
                else
                {
                    char* err_msg =
                        g_strdup_printf("%s:\n\n%s", _("File doesn't exist"), real_path);
                    ptk_show_error(NULL, _("Error"), err_msg);
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
                if (G_UNLIKELY(!daemon_mode))
                    init_folder();
                fm_main_window_store_positions(NULL);
                main_window = FM_MAIN_WINDOW(fm_main_window_new());
            }
            gtk_window_present(GTK_WINDOW(main_window));

            if (panel > 0 && panel < 5)
            {
                // user specified a panel with no file, let's show the panel
                if (!gtk_widget_get_visible(main_window->panel[panel - 1]))
                {
                    // show panel
                    set = xset_get_panel(panel, "show");
                    set->b = XSET_B_TRUE;
                    show_panels_all_windows(NULL, main_window);
                }
                focus_panel(NULL, (void*)main_window, panel);
            }
        }
    }
    // printf("    handle_parsed_commandline_args mw = %p\n\n", main_window );

    if (files != default_files)
        g_strfreev(files);

    files = NULL;
    return ret;
}

static void check_locale()
{
    char* name = setlocale(LC_ALL, NULL);
    if (G_UNLIKELY(!name && !(!strcmp(name, "C") || !strcmp(name, "C.UTF-8"))))
    {
        fprintf(stderr, "Non-C locale detected. This is not supported.\n");
        exit(1);
    }
}

static void tmp_clean()
{
    char* command = g_strdup_printf("rm -rf %s", xset_get_user_tmp_dir());
    print_command(command);
    g_spawn_command_line_async(command, NULL);
    g_free(command);
}

int main(int argc, char* argv[])
{
    check_locale();

    bool run = FALSE;
    GError* err = NULL;

#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
#endif

    // load spacefm.conf
    load_conf();

    // separate instance options
    if (argc > 1)
    {
        // dialog mode?
        if (!strcmp(argv[1], "-g") || !strcmp(argv[1], "--dialog"))
        {
            /* initialize the file alteration monitor */
            if (G_UNLIKELY(!vfs_file_monitor_init()))
            {
                ptk_show_error(NULL,
                               _("Error"),
                               _("Error: Unable to initialize inotify file change monitor.\n\nDo "
                                 "you have an inotify-capable kernel?"));
                vfs_file_monitor_clean();
                return 1;
            }
            gtk_init(&argc, &argv);
            int ret = custom_dialog_init(argc, argv);
            if (ret != 0)
            {
                vfs_file_monitor_clean();
                return ret == -1 ? 0 : ret;
            }
            gtk_main();

            vfs_file_monitor_clean();
            return 0;
        }

        // socket_command?
        if (!strcmp(argv[1], "-s") || !strcmp(argv[1], "--socket-cmd"))
        {
#ifdef ENABLE_NLS
            // initialize gettext since gtk_init is not run here
            setlocale(LC_ALL, "");
            bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
            textdomain(GETTEXT_PACKAGE);
#endif
            if (argv[2] && (!strcmp(argv[2], "help") || !strcmp(argv[2], "--help")))
            {
                printf("For help run, man spacefm-socket\n");
                return 0;
            }
            char* reply = NULL;
            int ret = send_socket_command(argc, argv, &reply);
            if (reply && reply[0])
                fprintf(ret ? stderr : stdout, "%s", reply);
            g_free(reply);
            return ret;
        }
    }

    /* initialize GTK+ and parse the command line arguments */
#ifdef ENABLE_NLS
    if (G_UNLIKELY(!gtk_init_with_args(&argc, &argv, "", opt_entries, GETTEXT_PACKAGE, &err)))
#else
    if (G_UNLIKELY(!gtk_init_with_args(&argc, &argv, "", opt_entries, NULL, &err)))
#endif
    {
        printf("spacefm: %s\n", err->message);
        g_error_free(err);
        return 1;
    }

    // dialog mode with other options?
    if (custom_dialog)
    {
        fprintf(stderr, "spacefm: %s\n", _("--dialog must be first option"));
        return 1;
    }

    // socket command with other options?
    if (socket_cmd)
    {
        fprintf(stderr, "spacefm: %s\n", _("--socket-cmd must be first option"));
        return 1;
    }

    // --disable-git
    if (!disable_git_settings)
        config_settings.git_backed_settings = TRUE;
    else
        config_settings.git_backed_settings = FALSE;

    // --version
    if (version_opt)
    {
        printf("%s %s\n", PACKAGE_NAME_FANCY, PACKAGE_VERSION);
        printf("Features: ");
        printf("GTK3 ");
        printf("UDEV ");
        printf("INOTIFY ");
        printf("FFMPEG ");
        printf("STATVFS ");
#ifdef USE_XXHASH
        printf("XXHASH ");
#endif
#ifdef HAVE_SN
        printf("SNOTIFY ");
#endif
#ifdef DEPRECATED_HW
        printf("DEPRECATED_HW ");
#endif
#ifdef HAVE_MMAP
        printf("MMAP ");
#endif
#ifdef _DEBUG_THREAD
        printf("DEBUG_THREAD ");
#endif
        printf("\n");
        return 0;
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
        ptk_show_error(NULL,
                       _("Error"),
                       _("Error: Unable to initialize inotify file change monitor.\n\nDo you have "
                         "an inotify-capable kernel?"));
        vfs_file_monitor_clean();
        // free_settings();
        return 1;
    }

    /* check if the filename encoding is UTF-8 */
    vfs_file_info_set_utf8_filename(g_get_filename_charsets(NULL));

    /* Initialize our mime-type system */
    vfs_mime_type_init();

    /* load config file */
    // MOD was before vfs_file_monitor_init
    load_settings(config_dir);

    app_settings.sdebug = sdebug;

    /* If we reach this point, we are the first instance.
     * Subsequent processes will exit() inside single_instance_check and won't reach here.
     */

    main_window_event(NULL, NULL, "evt_start", 0, 0, NULL, 0, 0, 0, FALSE);

    /* handle the parsed result of command line args */
    run = handle_parsed_commandline_args();
    app_settings.load_saved_tabs = TRUE;

    if (run) /* run the main loop */
        gtk_main();

    main_window_event(NULL, NULL, "evt_exit", 0, 0, NULL, 0, 0, 0, FALSE);

    single_instance_finalize();

    vfs_volume_finalize();
    vfs_mime_type_clean();
    vfs_file_monitor_clean();
    tmp_clean();
    free_settings();

    return 0;
}

static void open_file(const char* path)
{
    VFSFileInfo* file = vfs_file_info_new();
    vfs_file_info_get(file, path, NULL);
    VFSMimeType* mime_type = vfs_file_info_get_mime_type(file);
    bool opened = FALSE;
    GError* err = NULL;

    char* app_name = vfs_mime_type_get_default_action(mime_type);
    if (app_name)
    {
        opened = vfs_file_info_open_file(file, path, &err);
        g_free(app_name);
    }
    else
    {
        app_name = (char*)ptk_choose_app_for_mime_type(NULL, mime_type, TRUE, TRUE, TRUE, FALSE);
        if (app_name)
        {
            VFSAppDesktop* app = vfs_app_desktop_new(app_name);
            if (!vfs_app_desktop_get_exec(app))
                app->exec = g_strdup(app_name); /* This is a command line */
            GList* files = g_list_prepend(NULL, (void*)path);
            opened = vfs_app_desktop_open_files(gdk_screen_get_default(), NULL, app, files, &err);
            g_free(files->data);
            g_list_free(files);
            vfs_app_desktop_unref(app);
            g_free(app_name);
        }
        else
            opened = TRUE;
    }

    if (!opened)
    {
        char* error_msg;
        if (err && err->message)
        {
            error_msg = err->message;
        }
        else
            error_msg = _("Don't know how to open the file");
        char* disp_path = g_filename_display_name(path);
        char* msg = g_strdup_printf(_("Unable to open file:\n\"%s\"\n%s"), disp_path, error_msg);
        g_free(disp_path);
        ptk_show_error(NULL, _("Error"), msg);
        g_free(msg);
        if (err)
            g_error_free(err);
    }
    vfs_mime_type_unref(mime_type);
    vfs_file_info_unref(file);
}

/* After opening any window/dialog/tool, this should be called. */
void pcmanfm_ref()
{
    ++n_pcmanfm_ref;
}

/* After closing any window/dialog/tool, this should be called.
 * If the last window is closed and we are not a deamon, pcmanfm will quit.
 */
bool pcmanfm_unref()
{
    --n_pcmanfm_ref;
    if (n_pcmanfm_ref == 0 && !daemon_mode)
        gtk_main_quit();
    return FALSE;
}
