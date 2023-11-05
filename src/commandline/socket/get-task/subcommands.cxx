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

#include <CLI/CLI.hpp>

#include "commandline/socket.hxx"

#include "commandline/socket/get-task/subcommands.hxx"

/*
 * subcommand icon
 */

void
commandline::socket::get_task::icon(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("icon", "Get task icon");

    const auto run_subcommand = [&opt]() { opt->property = "icon"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand count
 */

void
commandline::socket::get_task::count(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("count", "Get task count");

    const auto run_subcommand = [&opt]() { opt->property = "count"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand directory
 */

void
commandline::socket::get_task::directory(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("directory", "Get task directory");

    const auto run_subcommand = [&opt]() { opt->property = "directory"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand from
 */

void
commandline::socket::get_task::from(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("from", "Get task from");

    const auto run_subcommand = [&opt]() { opt->property = "from"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand item
 */

void
commandline::socket::get_task::item(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("item", "Get task item");

    const auto run_subcommand = [&opt]() { opt->property = "item"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand item
 */

void
commandline::socket::get_task::to(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("to", "Get task to");

    const auto run_subcommand = [&opt]() { opt->property = "to"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand progress
 */

void
commandline::socket::get_task::progress(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("progress", "Get task progress");

    const auto run_subcommand = [&opt]() { opt->property = "progress"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand total
 */

void
commandline::socket::get_task::total(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("total", "Get task total");

    const auto run_subcommand = [&opt]() { opt->property = "total"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand curspeed
 */

void
commandline::socket::get_task::curspeed(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("curspeed", "Get task curspeed");

    const auto run_subcommand = [&opt]() { opt->property = "curspeed"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand curremain
 */

void
commandline::socket::get_task::curremain(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("curremain", "Get task curremain");

    const auto run_subcommand = [&opt]() { opt->property = "curremain"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand avgspeed
 */

void
commandline::socket::get_task::avgspeed(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("avgspeed", "Get task avgspeed");

    const auto run_subcommand = [&opt]() { opt->property = "avgspeed"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand avgremain
 */

void
commandline::socket::get_task::avgremain(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("avgremain", "Get task avgremain");

    const auto run_subcommand = [&opt]() { opt->property = "avgremain"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand elapsed
 */

void
commandline::socket::get_task::elapsed(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("elapsed", "Get task elapsed");

    const auto run_subcommand = [&opt]() { opt->property = "elapsed"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand started
 */

void
commandline::socket::get_task::started(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("started", "Get task started");

    const auto run_subcommand = [&opt]() { opt->property = "started"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand status
 */

void
commandline::socket::get_task::status(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("status", "Get task status");

    const auto run_subcommand = [&opt]() { opt->property = "status"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand queue_state
 */

void
commandline::socket::get_task::queue_state(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("queue-state", "Get task queue-state");

    const auto run_subcommand = [&opt]() { opt->property = "queue_state"; };
    sub->callback(run_subcommand);
}
