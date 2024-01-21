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

#include <optional>

#include <nlohmann/json.hpp>

#include <zmqpp/zmqpp.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "socket/commands.hxx"
#include "socket/server.hxx"

void
socket::server_thread() noexcept
{
    const zmqpp::context context;
    zmqpp::socket server(context, zmqpp::socket_type::pair);

    server.bind(std::format("tcp://*:{}", SOCKET_PORT));

    while (true)
    {
        // Wait for a command to be received
        zmqpp::message request;
        server.receive(request);

        // Process the command and generate a response
        std::string command;
        request >> command;

        ztd::logger::info("SOCKET({})", command);
        const auto [ret, response] = socket::command(command);

        nlohmann::json json;
        json["exit"] = ret;
        json["response"] = response;

        // Send the response back to the sender
        zmqpp::message reply;
        reply << json.dump();
        server.send(reply);
    }
}

bool
socket::send_command(zmqpp::socket& socket, const std::string_view message) noexcept
{
    zmqpp::message msg;
    msg << message.data();

    try
    {
        socket.send(msg);
    }
    catch (const zmqpp::exception& e)
    {
        return false;
    }
    return true;
}

const std::optional<std::string>
socket::receive_response(zmqpp::socket& socket) noexcept
{
    zmqpp::message msg;
    socket.receive(msg);

    std::string response;
    try
    {
        msg >> response;
    }
    catch (const zmqpp::exception& e)
    {
        return std::nullopt;
    }
    return response;
}
