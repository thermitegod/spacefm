/**
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <string>
#include <filesystem>

#include <sys/socket.h>
#include <sys/un.h>

#include <linux/kdev_t.h>

#include <fmt/format.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <glibmm.h>

#include "vfs/vfs-user-dir.hxx"

#include "main-window.hxx"

#include "socket.hxx"

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

CliFlags cli_flags = CliFlags();

static int sock;
static GIOChannel* io_channel = nullptr;

static bool socket_daemon = false;

static void get_socket_name(char* buf, int len);
static bool on_socket_event(GIOChannel* ioc, GIOCondition cond, void* data);
static void receive_socket_command(int client, GString* args);

bool
check_socket_daemon()
{
    return socket_daemon;
}

static std::string
get_inode_tag()
{
    struct stat stat_buf;
    std::string inode_tag;

    if (stat(vfs_user_home_dir().c_str(), &stat_buf) == -1)
    {
        inode_tag = fmt::format("{}=", getuid());
    }
    else
    {
        inode_tag = fmt::format("{}={}:{}-{}",
                                getuid(),
                                MAJOR(stat_buf.st_dev),
                                MINOR(stat_buf.st_dev),
                                stat_buf.st_ino);
    }

    return inode_tag;
}

static bool
on_socket_event(GIOChannel* ioc, GIOCondition cond, void* data)
{
    (void)data;

    if (!(cond & G_IO_IN))
        return true;

    socklen_t addr_len = 0;
    struct sockaddr_un client_addr;

    int client = accept(g_io_channel_unix_get_fd(ioc), (struct sockaddr*)&client_addr, &addr_len);
    if (client == -1)
        return true;

    static char buf[1024];
    GString* args = g_string_new_len(nullptr, 2048);
    int r;
    while ((r = read(client, buf, sizeof(buf))) > 0)
    {
        g_string_append_len(args, buf, r);
        if (args->str[0] == SocketEvent::CMD_SOCKET_CMD && args->len > 1 &&
            args->str[args->len - 2] == '\n' && args->str[args->len - 1] == '\n')
            // because SocketEvent::CMD_SOCKET_CMD doesn't immediately close the socket
            // data is terminated by two linefeeds to prevent read blocking
            break;
    }
    if (args->str[0] == SocketEvent::CMD_SOCKET_CMD)
        receive_socket_command(client, args);
    shutdown(client, 2);
    close(client);

    cli_flags.new_tab = true;
    cli_flags.panel = 0;
    cli_flags.reuse_tab = false;
    cli_flags.no_tabs = false;
    socket_daemon = false;

    int argx = 0;
    if (args->str[argx] == SocketEvent::CMD_NO_TABS)
    {
        cli_flags.reuse_tab = false;
        cli_flags.no_tabs = true;
        argx++; // another command follows SocketEvent::CMD_NO_TABS
    }
    if (args->str[argx] == SocketEvent::CMD_REUSE_TAB)
    {
        cli_flags.reuse_tab = true;
        cli_flags.new_tab = false;
        argx++; // another command follows SocketEvent::CMD_REUSE_TAB
    }

    switch (args->str[argx])
    {
        case SocketEvent::CMD_PANEL1:
            cli_flags.panel = 1;
            break;
        case SocketEvent::CMD_PANEL2:
            cli_flags.panel = 2;
            break;
        case SocketEvent::CMD_PANEL3:
            cli_flags.panel = 3;
            break;
        case SocketEvent::CMD_PANEL4:
            cli_flags.panel = 4;
            break;
        case SocketEvent::CMD_OPEN:
            cli_flags.new_tab = false;
            break;
        case SocketEvent::CMD_OPEN_PANEL1:
            cli_flags.new_tab = false;
            cli_flags.panel = 1;
            break;
        case SocketEvent::CMD_OPEN_PANEL2:
            cli_flags.new_tab = false;
            cli_flags.panel = 2;
            break;
        case SocketEvent::CMD_OPEN_PANEL3:
            cli_flags.new_tab = false;
            cli_flags.panel = 3;
            break;
        case SocketEvent::CMD_OPEN_PANEL4:
            cli_flags.new_tab = false;
            cli_flags.panel = 4;
            break;
        case SocketEvent::CMD_DAEMON_MODE:
            socket_daemon = cli_flags.daemon_mode = true;
            g_string_free(args, true);
            return true;
        case SocketEvent::CMD_FIND_FILES:
            cli_flags.find_files = true;
            g_string_free(args, true);
            return true;
        case SocketEvent::CMD_SOCKET_CMD:
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
            if (!**file)
            {
                // remove empty string at tail
                *file = nullptr;
            }
        }
    }
    // handle_parsed_commandline_args();
    app_settings.load_saved_tabs = true;
    socket_daemon = false;

    return true;
}

static void
get_socket_name(char* buf, int len)
{
    std::string dpy = ztd::strdup(Glib::getenv("DISPLAY"));
    // treat :0.0 as :0 to prevent multiple instances on screen 0
    if (ztd::same(dpy, ":0.0"))
        dpy = ":0";
    std::string socket_path =
        fmt::format("{}/spacefm-{}{}.socket", vfs_user_runtime_dir(), Glib::get_user_name(), dpy);
    g_snprintf(buf, len, "%s", socket_path.c_str());
}

[[noreturn]] static void
single_instance_check_fatal(int ret)
{
    gdk_notify_startup_complete();
    std::exit(ret);
}

bool
single_instance_check()
{
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        LOG_ERROR("failed to create socket");
        single_instance_check_fatal(EXIT_FAILURE);
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    get_socket_name(addr.sun_path, sizeof(addr.sun_path));
    int addr_len = SUN_LEN(&addr);

    // try to connect to existing instance
    if (sock && connect(sock, (struct sockaddr*)&addr, addr_len) == 0)
    {
        // connected successfully
        char cmd = SocketEvent::CMD_OPEN_TAB;

        if (cli_flags.no_tabs)
        {
            cmd = SocketEvent::CMD_NO_TABS;
            write(sock, &cmd, sizeof(char));
            // another command always follows SocketEvent::CMD_NO_TABS
            cmd = SocketEvent::CMD_OPEN_TAB;
        }
        if (cli_flags.reuse_tab)
        {
            cmd = SocketEvent::CMD_REUSE_TAB;
            write(sock, &cmd, sizeof(char));
            // another command always follows SocketEvent::CMD_REUSE_TAB
            cmd = SocketEvent::CMD_OPEN;
        }

        if (cli_flags.daemon_mode)
            cmd = SocketEvent::CMD_DAEMON_MODE;
        else if (cli_flags.new_window)
        {
            if (cli_flags.panel > 0 && cli_flags.panel < 5)
                cmd = SocketEvent::CMD_OPEN_PANEL1 + cli_flags.panel - 1;
            else
                cmd = SocketEvent::CMD_OPEN;
        }
        else if (cli_flags.find_files)
            cmd = SocketEvent::CMD_FIND_FILES;
        else if (cli_flags.panel > 0 && cli_flags.panel < 5)
            cmd = SocketEvent::CMD_PANEL1 + cli_flags.panel - 1;

        // open a new window if no file spec
        if (cmd == SocketEvent::CMD_OPEN_TAB && !cli_flags.files)
            cmd = SocketEvent::CMD_OPEN;

        write(sock, &cmd, sizeof(char));

        if (cli_flags.files)
        {
            char** file;
            for (file = cli_flags.files; *file; ++file)
            {
                std::string real_path;

                if ((*file[0] != '/' && strstr(*file, ":/")) || Glib::str_has_prefix(*file, "//"))
                    real_path = *file;
                else
                {
                    // We send absolute paths because with different
                    // $PWDs resolution would not work.
                    real_path = std::filesystem::absolute(*file);
                }
                write(sock, real_path.c_str(), real_path.length());
                write(sock, "\n", 1);
            }
        }

        if (cli_flags.config_dir)
            LOG_WARN("Option --config ignored - an instance is already running");

        shutdown(sock, 2);
        close(sock);
        single_instance_check_fatal(EXIT_SUCCESS);
    }

    // There is no existing server, and we are in the first instance.
    if (std::filesystem::exists(addr.sun_path))
    {
        // delete old socket file if it exists.
        std::filesystem::remove(addr.sun_path);
    }
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(sock, (struct sockaddr*)&addr, addr_len) == -1)
    {
        LOG_ERROR("failed to create socket: {}", addr.sun_path);
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

void
single_instance_finalize()
{
    char lock_file[256];

    shutdown(sock, 2);
    g_io_channel_unref(io_channel);
    close(sock);

    get_socket_name(lock_file, sizeof(lock_file));
    std::filesystem::remove(lock_file);
}

static void
receive_socket_command(int client, GString* args)
{
    char** argv;
    char cmd;
    std::string reply;

    if (args->str[1])
    {
        if (Glib::str_has_suffix(args->str, "\n\n"))
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
    std::string inode_tag = get_inode_tag();
    if (argv && strcmp(inode_tag.c_str(), argv[0]))
    {
        reply = ztd::strdup("spacefm: invalid socket command user\n");
        cmd = 1;
        LOG_WARN("invalid socket command user");
    }
    else
    {
        // process command and get reply
        cmd = main_window_socket_command(argv ? argv + 1 : nullptr, reply);
    }
    g_strfreev(argv);

    // send response
    write(client, &cmd, sizeof(char)); // send exit status
    if (!reply.empty())
        write(client, reply.c_str(), reply.size()); // send reply or error msg
}

int
send_socket_command(int argc, char* argv[], char** reply)
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
    char cmd = SocketEvent::CMD_SOCKET_CMD;
    write(sock, &cmd, sizeof(char));

    // send inode tag
    std::string inode_tag = get_inode_tag();
    write(sock, inode_tag.c_str(), std::strlen(inode_tag.c_str()));
    write(sock, "\n", 1);

    // send arguments
    int i;
    for (i = 2; i < argc; i++)
    {
        write(sock, argv[i], std::strlen(argv[i]));
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
        *reply = ztd::strdup(sock_reply->str + 1);
        ret = sock_reply->str[0];
    }
    else
    {
        LOG_ERROR("invalid response from socket");
        ret = EXIT_FAILURE;
    }
    g_string_free(sock_reply, true);
    return ret;
}
