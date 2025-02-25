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

#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>

#include <CLI/CLI.hpp>

#include "commandline/socket.hxx"
#include "commandline/socket/run-task/subcommands.hxx"

#include "socket/datatypes.hxx"

/*
 * subcommand cmd
 */

static void
run_subcommand_cmd(const socket_subcommand_data_t& opt,
                   const std::shared_ptr<socket::socket_task_data>& task_opt) noexcept
{
    const auto buffer = glz::write_json(task_opt);
    if (!buffer)
    {
        std::println("Failed to create socket task json: {}", glz::format_error(buffer));
        return;
    }
    // std::println("JSON : {}", buffer);

    opt->property = "cmd";
    opt->data = {buffer.value()};
}

void
commandline::socket::run_task::cmd(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto task_opt = std::make_shared<::socket::socket_task_data>();

    auto* sub = app->add_subcommand("cmd", "Run task cmd task");

    // named flags
    sub->add_flag("--task", task_opt->task);
    sub->add_flag("--popup", task_opt->task);
    sub->add_flag("--terminal", task_opt->terminal);
    sub->add_option("--user", task_opt->user)->expected(1);
    sub->add_option("--title", task_opt->title)->expected(1);
    sub->add_option("--icon", task_opt->icon)->expected(1);
    sub->add_option("--dir", task_opt->cwd)->expected(1);

    sub->add_option("command", task_opt->cmd, "cmd to run")->required(true)->expected(1, -1);

    const auto run_subcommand = [&opt, &task_opt]() { run_subcommand_cmd(opt, task_opt); };
    sub->callback(run_subcommand);
}

/*
 * subcommand edit
 */

void
commandline::socket::run_task::edit(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("edit", "Run task edit");

    sub->add_option("value", opt->data, "File to edit")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "edit"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand mount
 */

void
commandline::socket::run_task::mount(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("mount", "Run task mount");

    sub->add_option("value", opt->data, "Device to mount")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "mount"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand umount
 */

void
commandline::socket::run_task::umount(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("umount", "Run task mount");

    sub->add_option("value", opt->data, "Device to umount")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "umount"; };
    sub->callback(run_subcommand);
}

/*
 * this is shared with all file actions
 */

static void
run_subcommand_file_action(const socket_subcommand_data_t& opt,
                           const std::shared_ptr<socket::socket_file_task_data>& file_opt,
                           const std::string_view command) noexcept
{
    const auto buffer = glz::write_json(file_opt);
    if (!buffer)
    {
        std::println("Failed to create socket file task json: {}", glz::format_error(buffer));
        return;
    }
    // std::println("JSON : {}", buffer);

    opt->command = command;
    opt->data = {buffer.value()};
}

/*
 * subcommand copy
 */

void
commandline::socket::run_task::copy(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto file_opt = std::make_shared<::socket::socket_file_task_data>();

    auto* sub = app->add_subcommand("copy", "Run task copy");

    sub->add_option("--dir", file_opt->dir)->expected(1);

    sub->add_option("FILES", file_opt->files, "Files to copy")->required(true)->expected(1, -1);

    const auto run_subcommand = [&opt, &file_opt]()
    { run_subcommand_file_action(opt, file_opt, "copy"); };
    sub->callback(run_subcommand);
}

/*
 * subcommand move
 */

void
commandline::socket::run_task::move(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto file_opt = std::make_shared<::socket::socket_file_task_data>();

    auto* sub = app->add_subcommand("move", "Run task move");

    sub->add_option("--dir", file_opt->dir)->expected(1);

    sub->add_option("FILES", file_opt->files, "Files to move")->required(true)->expected(1, -1);

    const auto run_subcommand = [&opt, &file_opt]()
    { run_subcommand_file_action(opt, file_opt, "move"); };
    sub->callback(run_subcommand);
}

/*
 * subcommand link
 */

void
commandline::socket::run_task::link(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto file_opt = std::make_shared<::socket::socket_file_task_data>();

    auto* sub = app->add_subcommand("link", "Run task link");

    sub->add_option("--dir", file_opt->dir)->expected(1);

    sub->add_option("FILES", file_opt->files, "Files to link")->required(true)->expected(1, -1);

    const auto run_subcommand = [&opt, &file_opt]()
    { run_subcommand_file_action(opt, file_opt, "link"); };
    sub->callback(run_subcommand);
}

/*
 * subcommand delete
 */

void
commandline::socket::run_task::del(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto file_opt = std::make_shared<::socket::socket_file_task_data>();

    auto* sub = app->add_subcommand("delete", "Run task delete");

    sub->add_option("--dir", file_opt->dir)->expected(1);

    sub->add_option("FILES", file_opt->files, "Files to delete")->required(true)->expected(1, -1);

    const auto run_subcommand = [&opt, &file_opt]()
    { run_subcommand_file_action(opt, file_opt, "delete"); };
    sub->callback(run_subcommand);
}

/*
 * subcommand trash
 */

void
commandline::socket::run_task::trash(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto file_opt = std::make_shared<::socket::socket_file_task_data>();

    auto* sub = app->add_subcommand("trash", "Run task trash");

    sub->add_option("--dir", file_opt->dir)->expected(1);

    sub->add_option("FILES", file_opt->files, "Files to trash")->required(true)->expected(1, -1);

    const auto run_subcommand = [&opt, &file_opt]()
    { run_subcommand_file_action(opt, file_opt, "trash"); };
    sub->callback(run_subcommand);
}
