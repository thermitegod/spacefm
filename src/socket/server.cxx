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
#include <optional>
#include <string>
#include <string_view>

#include <zmq.hpp>

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "socket/commands.hxx"
#include "socket/datatypes.hxx"
#include "socket/server.hxx"

#include "logger.hxx"

void
spacefm::server::server_thread() noexcept
{
    zmq::context_t context{1};
    zmq::socket_t server(context, zmq::socket_type::pair);

    server.bind(std::format("tcp://localhost:{}", SOCKET_PORT));

    logger::debug<logger::domain::socket>("starting socket thread {}",
                                          std::format("tcp://localhost:{}", SOCKET_PORT));

    while (true)
    {
        // Wait for a command to be received
        zmq::message_t request;
        if (server.recv(request, zmq::recv_flags::none))
        {
            // Process the command and generate a response
            const std::string command(static_cast<char*>(request.data()), request.size());

            logger::info<logger::domain::socket>("request: {}", command);
            const auto [ret, result] = socket::command(command);

            const auto response = socket::socket_response_data{ret.data(), result};
            const auto buffer = glz::write_json(response);
            if (!buffer)
            {
                logger::info<logger::domain::socket>("Failed to create response: {}",
                                                     glz::format_error(buffer));
                continue;
            }
            logger::info<logger::domain::socket>("result : {}", buffer.value());

            // Send the response back to the sender
            zmq::message_t reply(buffer->size());
            std::memcpy(reply.data(), buffer->data(), buffer->size());
            server.send(reply, zmq::send_flags::none);
        }
    }
}

bool
spacefm::server::send_command(zmq::socket_t& socket, const std::string_view command) noexcept
{
    zmq::message_t message(command.size());
    std::memcpy(message.data(), command.data(), command.size());
    return socket.send(message, zmq::send_flags::none) != std::nullopt;
}

std::optional<std::string>
spacefm::server::receive_response(zmq::socket_t& socket) noexcept
{
    zmq::message_t message;
    if (socket.recv(message, zmq::recv_flags::none))
    {
        return std::string(static_cast<char*>(message.data()), message.size());
    }
    return std::nullopt;
}
