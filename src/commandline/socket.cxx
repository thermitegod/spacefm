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

#include <format>
#include <memory>
#include <print>
#include <string>

#include <zmq.hpp>

#include <glaze/glaze.hpp>

#include <CLI/CLI.hpp>

#include <ztd/ztd.hxx>

#include "commandline/socket.hxx"
#include "commandline/socket/subcommands.hxx"

#include "socket/datatypes.hxx"
#include "socket/server.hxx"

[[noreturn]] static void
run_subcommand_socket(const socket_subcommand_data_t& opt)
{
    // connect to server
    zmq::context_t context{1};
    zmq::socket_t socket(context, zmq::socket_type::pair);
    socket.set(zmq::sockopt::rcvtimeo, 1000);

    try
    {
        socket.connect(std::format("tcp://localhost:{}", SOCKET_PORT));
    }
    catch (const zmq::error_t& e)
    {
        std::print("Failed to connect to server: ", e.what());
        std::exit(EXIT_FAILURE);
    }

    // server request
    const auto buffer = glz::write_json(opt);
    if (!buffer)
    {
        std::println("Failed to create socket json: {}", glz::format_error(buffer));
        std::exit(EXIT_FAILURE);
    }
    // std::println("JSON : {}", buffer);

    const bool sent = socket::send_command(socket, buffer.value());
    if (!sent)
    {
        std::println("Failed to send command to server");
        std::exit(EXIT_FAILURE);
    }

    // server response
    const auto server_response = socket::receive_response(socket);
    if (!server_response)
    {
        std::println("Failed to receive response from server");
        std::exit(EXIT_FAILURE);
    }

    const auto response = glz::read_json<socket::socket_response_data>(server_response.value());
    if (!response)
    {
        std::println("Failed to decode json: {}",
                     glz::format_error(response.error(), server_response.value()));
        std::exit(EXIT_FAILURE);
    }

    if (!response->message.empty())
    {
        std::println("{}", response->message);
    }

    std::exit(response->exit_status);
}

void
setup_subcommand_socket(CLI::App& app) noexcept
{
    auto opt = std::make_shared<socket::socket_request_data>();
    CLI::App* sub = app.add_subcommand("socket", "Send a socket command (See subcommand help)");

    // named flags
    sub->add_option("-w,--window", opt->window, "Window to use");
    sub->add_option("-p,--panel", opt->panel, "Panel to use");
    sub->add_option("-t,--tab", opt->tab, "tab to use");

    // socket subcommand is used to run other subcommands make sure that they are used.
    sub->require_subcommand();

    setup_subcommand_set(sub, opt);
    setup_subcommand_get(sub, opt);
    setup_subcommand_set_task(sub, opt);
    setup_subcommand_get_task(sub, opt);
    setup_subcommand_run_task(sub, opt);
    setup_subcommand_emit_key(sub, opt);
    setup_subcommand_activate(sub, opt);
    setup_subcommand_add_event(sub, opt);
    setup_subcommand_replace_event(sub, opt);
    setup_subcommand_remove_event(sub, opt);
    setup_subcommand_help(sub, opt);
    setup_subcommand_ping(sub, opt);

    const auto run_subcommand = [opt]() { run_subcommand_socket(opt); };
    sub->callback(run_subcommand);
}
