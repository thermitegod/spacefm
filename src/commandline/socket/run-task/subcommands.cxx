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

#include <print>

#include <filesystem>

#include <vector>

#include <memory>

#include <CLI/CLI.hpp>

#include <nlohmann/json.hpp>

#include "commandline/socket.hxx"

#include "commandline/socket/run-task/subcommands.hxx"

struct run_task_file_opt_data
{
    // flags
    std::filesystem::path dir;
    // file list
    std::vector<std::string> files;
};

using run_task_file_opt_data_t = std::shared_ptr<run_task_file_opt_data>;

struct run_task_cmd_opt_data
{
    // flags
    bool task{false};
    bool popup{false};
    bool scroll{false};
    bool terminal{false};
    std::string user;
    std::string title;
    std::string icon;
    std::string cwd;
    // actual command to be run
    std::vector<std::string> cmd;
};

using run_task_cmd_opt_data_t = std::shared_ptr<run_task_cmd_opt_data>;

/*
 * subcommand cmd
 */

void
run_subcommand_cmd(const socket_subcommand_data_t& opt,
                   const run_task_cmd_opt_data_t& task_opt) noexcept
{
    nlohmann::json json;
    // cmd task flags
    json["task"] = task_opt->task;
    json["popup"] = task_opt->popup;
    json["terminal"] = task_opt->terminal;
    json["user"] = task_opt->user;
    json["title"] = task_opt->title;
    json["icon"] = task_opt->icon;
    json["cwd"] = task_opt->cwd;
    // actual command to be run
    json["cmd"] = task_opt->cmd;
    // std::println("JSON : {}", json.dump());

    opt->property = "cmd";
    // cursed but works
    opt->socket_data = {json.dump()};
}

void
commandline::socket::run_task::cmd(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto task_opt = std::make_shared<run_task_cmd_opt_data>();

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

    sub->add_option("value", opt->socket_data, "File to edit")->required(true)->expected(1);

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

    sub->add_option("value", opt->socket_data, "Device to mount")->required(true)->expected(1);

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

    sub->add_option("value", opt->socket_data, "Device to umount")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "umount"; };
    sub->callback(run_subcommand);
}

/*
 * this is shared with all file actions
 */

void
run_subcommand_file_action(const socket_subcommand_data_t& opt,
                           const run_task_file_opt_data_t& file_opt,
                           const std::string_view command) noexcept
{
    nlohmann::json json;
    // file task flags
    json["dir"] = file_opt->dir;
    // actual command to be run
    json["files"] = file_opt->files;
    // std::println("JSON : {}", json.dump());

    opt->command = command;
    // cursed but works
    opt->socket_data = {json.dump()};
}

/*
 * subcommand copy
 */

void
commandline::socket::run_task::copy(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto file_opt = std::make_shared<run_task_file_opt_data>();

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
    auto file_opt = std::make_shared<run_task_file_opt_data>();

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
    auto file_opt = std::make_shared<run_task_file_opt_data>();

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
    auto file_opt = std::make_shared<run_task_file_opt_data>();

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
    auto file_opt = std::make_shared<run_task_file_opt_data>();

    auto* sub = app->add_subcommand("trash", "Run task trash");

    sub->add_option("--dir", file_opt->dir)->expected(1);

    sub->add_option("FILES", file_opt->files, "Files to trash")->required(true)->expected(1, -1);

    const auto run_subcommand = [&opt, &file_opt]()
    { run_subcommand_file_action(opt, file_opt, "trash"); };
    sub->callback(run_subcommand);
}
