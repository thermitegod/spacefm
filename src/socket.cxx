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
#include <string_view>

#include <format>

#include <filesystem>

#include <tuple>

#include <sys/socket.h>
#include <sys/un.h>

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <linux/kdev_t.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dirs.hxx"

#include "settings/app.hxx"

#include "main-window.hxx"

#include "socket.hxx"

enum SocketEvent
{
    CMD_OPEN = 1,
    CMD_OPEN_TAB,
    CMD_REUSE_TAB,
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

static i32 sock_fd = -1;
static Glib::RefPtr<Glib::IOChannel> sock_io_channel = nullptr;

static bool on_socket_event(Glib::IOCondition condition);
static void receive_socket_command(i32 client, const std::string& args);

static const std::string
get_inode_tag()
{
    std::string inode_tag;

    const auto statbuf = ztd::stat(vfs::user_dirs->home_dir());
    if (statbuf.is_valid())
    {
        inode_tag = std::format("{}={}:{}-{}",
                                getuid(),
                                gnu_dev_major(statbuf.dev()),
                                gnu_dev_minor(statbuf.dev()),
                                statbuf.ino());
    }
    else
    {
        inode_tag = std::format("{}=", getuid());
    }

    // ztd::logger::info("inode_tag={}", inode_tag);

    return inode_tag;
}

static bool
on_socket_event(Glib::IOCondition condition)
{
    if (condition != Glib::IOCondition::IO_IN)
    {
        return true;
    }

    socklen_t addr_len = 0;
    struct sockaddr_un client_addr;

    const i32 client = accept(sock_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client == -1)
    {
        return true;
    }

    static char buf[1024];
    std::string args;
    usize r;
    while ((r = read(client, buf, sizeof(buf))) > 0)
    {
        args.append(buf, r);
        if (args[0] == SocketEvent::CMD_SOCKET_CMD && args.size() > 1 &&
            args[args.size() - 2] == '\n' && args[args.size() - 1] == '\n')
        {
            // because SocketEvent::CMD_SOCKET_CMD does not immediately close the socket
            // data is terminated by two linefeeds to prevent read blocking
            break;
        }
    }
    if (args[0] == SocketEvent::CMD_SOCKET_CMD)
    {
        receive_socket_command(client, args);
    }
    shutdown(client, SHUT_RDWR);
    close(client);

    cli_flags.new_tab = true;
    cli_flags.panel = 0;
    cli_flags.reuse_tab = false;
    cli_flags.no_tabs = false;

    usize argx = 0;
    if (args[argx] == SocketEvent::CMD_NO_TABS)
    {
        cli_flags.reuse_tab = false;
        cli_flags.no_tabs = true;
        argx++; // another command follows SocketEvent::CMD_NO_TABS
    }
    if (args[argx] == SocketEvent::CMD_REUSE_TAB)
    {
        cli_flags.reuse_tab = true;
        cli_flags.new_tab = false;
        argx++; // another command follows SocketEvent::CMD_REUSE_TAB
    }

    switch (args[argx])
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
        case SocketEvent::CMD_FIND_FILES:
            cli_flags.find_files = true;
            return true;
        case SocketEvent::CMD_SOCKET_CMD:
            return true;
        default:
            break;
    }

    if (args[argx + 1])
    {
        cli_flags.files = g_strsplit(args.data() + argx + 1, "\n", 0);
    }
    else
    {
        cli_flags.files = nullptr;
    }

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
    app_settings.set_load_saved_tabs(true);

    return true;
}

static const std::filesystem::path
get_socket_name()
{
    std::string dpy = Glib::getenv("DISPLAY");
    // treat :0.0 as :0 to prevent multiple instances on screen 0
    if (ztd::same(dpy, ":0.0"))
    {
        dpy = ":0";
    }

    const std::string socket_file =
        std::format("{}-{}{}.socket", PACKAGE_NAME, Glib::get_user_name(), dpy);

    return vfs::user_dirs->runtime_dir() / socket_file;
}

[[noreturn]] static void
single_instance_check_fatal(i32 ret)
{
    gdk_notify_startup_complete();
    std::exit(ret);
}

bool
single_instance_check()
{
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        ztd::logger::error("failed to create socket");
        single_instance_check_fatal(EXIT_FAILURE);
    }

    // open socket
    sockaddr_un addr;
    std::memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    std::strcpy(addr.sun_path, get_socket_name().c_str());
    const socklen_t addr_len = SUN_LEN(&addr);

    // try to connect to existing instance
    if (sock_fd && connect(sock_fd, (struct sockaddr*)&addr, addr_len) == 0)
    {
        // connected successfully
        char cmd = SocketEvent::CMD_OPEN_TAB;

        if (cli_flags.no_tabs)
        {
            cmd = SocketEvent::CMD_NO_TABS;
            (void)write(sock_fd, &cmd, sizeof(char));
            // another command always follows SocketEvent::CMD_NO_TABS
            cmd = SocketEvent::CMD_OPEN_TAB;
        }
        if (cli_flags.reuse_tab)
        {
            cmd = SocketEvent::CMD_REUSE_TAB;
            (void)write(sock_fd, &cmd, sizeof(char));
            // another command always follows SocketEvent::CMD_REUSE_TAB
            cmd = SocketEvent::CMD_OPEN;
        }

        if (cli_flags.new_window)
        {
            if (cli_flags.panel > 0 && cli_flags.panel < 5)
            {
                cmd = SocketEvent::CMD_OPEN_PANEL1 + cli_flags.panel - 1;
            }
            else
            {
                cmd = SocketEvent::CMD_OPEN;
            }
        }
        else if (cli_flags.find_files)
        {
            cmd = SocketEvent::CMD_FIND_FILES;
        }
        else if (cli_flags.panel > 0 && cli_flags.panel < 5)
        {
            cmd = SocketEvent::CMD_PANEL1 + cli_flags.panel - 1;
        }

        // open a new window if no file spec
        if (cmd == SocketEvent::CMD_OPEN_TAB && !cli_flags.files)
        {
            cmd = SocketEvent::CMD_OPEN;
        }

        (void)write(sock_fd, &cmd, sizeof(char));

        if (cli_flags.files)
        {
            char** file;
            for (file = cli_flags.files; *file; ++file)
            {
                std::string real_path;

                if ((*file[0] != '/' && ztd::contains(*file, ":/")) || ztd::startswith(*file, "//"))
                {
                    real_path = *file;
                }
                else
                {
                    // We send absolute paths because with different
                    // $PWDs resolution would not work.
                    real_path = std::filesystem::absolute(*file);
                }
                (void)write(sock_fd, real_path.data(), real_path.length());
                (void)write(sock_fd, "\n", 1);
            }
        }

        if (cli_flags.config_dir)
        {
            ztd::logger::warn("Option --config ignored - an instance is already running");
        }

        shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
        single_instance_check_fatal(EXIT_SUCCESS);
    }

    // There is no existing server, and we are in the first instance.
    if (std::filesystem::exists(addr.sun_path))
    {
        // delete old socket file if it exists.
        std::filesystem::remove(addr.sun_path);
    }
    i32 reuse = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(sock_fd, (struct sockaddr*)&addr, addr_len) == -1)
    {
        ztd::logger::error("failed to create socket: {}", addr.sun_path);
        single_instance_check_fatal(EXIT_FAILURE);
    }
    else
    {
        sock_io_channel = Glib::IOChannel::create_from_fd(sock_fd);
        sock_io_channel->set_buffered(false);

        Glib::signal_io().connect(sigc::ptr_fun(on_socket_event),
                                  sock_io_channel,
                                  Glib::IOCondition::IO_IN);

        if (listen(sock_fd, 5) == -1)
        {
            ztd::logger::warn("could not listen to socket");
            single_instance_check_fatal(EXIT_FAILURE);
        }
    }

    return true;
}

void
single_instance_finalize()
{
    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);

    const auto lock_file = get_socket_name();
    std::filesystem::remove(lock_file);
}

static void
write_socket_response(i32 client, char cmd, const std::string_view reply)
{
    // send response and exit status
    const auto length = write(client, &cmd, sizeof(char));
    if (length == -1)
    {
        ztd::logger::error("failed to write to fd {}", sock_fd);
        return;
    }

    if (!reply.empty())
    {
        // send reply or error msg
        const auto reply_length = write(client, reply.data(), reply.size());
        if (reply_length == -1)
        {
            ztd::logger::error("failed to write to fd {}", sock_fd);
            return;
        }
    }
}

static void
receive_socket_command(i32 client, const std::string& args)
{
    char** argv = nullptr;
    if (!args.empty())
    {
        argv = g_strsplit(args.data() + 1, "\n", 0);
    }

    // check inode tag - was socket command sent from the same filesystem?
    // eg this helps deter use of socket commands sent from a chroot jail
    // or from another user or system
    const std::string inode_tag = get_inode_tag();
    if (argv && !ztd::same(inode_tag, argv[0]))
    {
        char cmd = 1;
        const std::string reply = "invalid socket command user";
        ztd::logger::warn("{}", reply);
        write_socket_response(client, cmd, reply);
    }
    else
    {
        // process command and get reply
        const auto [cmd, reply] = main_window_socket_command(argv ? argv + 1 : nullptr);
        write_socket_response(client, cmd, reply);
    }
    g_strfreev(argv);
}

const std::tuple<i32, std::string>
send_socket_command(std::span<const std::string_view> args)
{
    if (args.size() < 3)
    {
        return {EXIT_FAILURE, "socket-cmd requires an argument"};
    }

    // create socket
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        return {EXIT_FAILURE, "failed to create socket"};
    }

    // open socket
    sockaddr_un addr;
    std::memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    std::strcpy(addr.sun_path, get_socket_name().c_str());
    const socklen_t addr_len = SUN_LEN(&addr);

    if (connect(sock_fd, (struct sockaddr*)&addr, addr_len) != 0)
    {
        return {EXIT_FAILURE,
                std::format("failed to connect to socket ({})\nnot running or $DISPLAY not set",
                            addr.sun_path)};
    }

    // send command
    char cmd = SocketEvent::CMD_SOCKET_CMD;
    (void)write(sock_fd, &cmd, sizeof(char));

    // send inode tag
    const std::string inode_tag = get_inode_tag();
    (void)write(sock_fd, inode_tag.data(), inode_tag.size());
    (void)write(sock_fd, "\n", 1);

    // send arguments
    for (const auto i : ztd::range(2, args.size()))
    {
        (void)write(sock_fd, args[i].data(), std::strlen(args[i].data()));
        (void)write(sock_fd, "\n", 1);
    }
    (void)write(sock_fd, "\n", 1);

    // get response
    std::string sock_reply;
    usize r;
    char buf[1024];
    while ((r = read(sock_fd, buf, sizeof(buf))) > 0)
    {
        sock_reply.append(buf, r);
    }

    // close socket
    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);

    // set reply
    if (sock_reply.size() == 0)
    {
        ztd::logger::error("invalid response from socket");
        return {EXIT_FAILURE, ""};
    }

    return {sock_reply[0], sock_reply.substr(1)};
}
