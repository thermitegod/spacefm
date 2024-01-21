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

#include "commandline/socket/set-task/subcommands.hxx"

/*
 * subcommand icon
 */

void
commandline::socket::set_task::icon(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("icon", "Set task icon");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "icon"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand count
 */

void
commandline::socket::set_task::count(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("count", "Set task count");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "count"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand directory
 */

void
commandline::socket::set_task::directory(CLI::App* app,
                                         const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("directory", "Set task directory");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "directory"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand from
 */

void
commandline::socket::set_task::from(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("from", "Set task from");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "from"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand item
 */

void
commandline::socket::set_task::item(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("item", "Set task item");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "item"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand item
 */

void
commandline::socket::set_task::to(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("to", "Set task to");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "to"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand progress
 */

void
commandline::socket::set_task::progress(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("progress", "Set task progress");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "progress"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand total
 */

void
commandline::socket::set_task::total(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("total", "Set task total");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "total"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand curspeed
 */

void
commandline::socket::set_task::curspeed(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("curspeed", "Set task curspeed");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "curspeed"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand curremain
 */

void
commandline::socket::set_task::curremain(CLI::App* app,
                                         const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("curremain", "Set task curremain");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "curremain"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand avgspeed
 */

void
commandline::socket::set_task::avgspeed(CLI::App* app, const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("avgspeed", "Set task avgspeed");

    const auto run_subcommand = [opt]() { opt->property = "avgspeed"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand avgremain
 */

void
commandline::socket::set_task::avgremain(CLI::App* app,
                                         const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("avgremain", "Set task avgremain");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "avgremain"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand queue_state
 */

void
setup_subcommand_set_task_queue_state_run(CLI::App* app,
                                          const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("run", "Set task property run");

    const auto run_subcommand = [opt]() { opt->subproperty = "run"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_task_queue_state_pause(CLI::App* app,
                                            const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("pause", "Set task property pause");

    const auto run_subcommand = [opt]() { opt->subproperty = "pause"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_task_queue_state_queue(CLI::App* app,
                                            const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("queue", "Set task property queue");

    const auto run_subcommand = [opt]() { opt->subproperty = "queue"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_task_queue_state_queued(CLI::App* app,
                                             const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("queued", "Set task property queued");

    const auto run_subcommand = [opt]() { opt->subproperty = "queued"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_task_queue_state_stop(CLI::App* app,
                                           const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("stop", "Set task property stop");

    const auto run_subcommand = [opt]() { opt->subproperty = "stop"; };
    sub->callback(run_subcommand);
}

void
commandline::socket::set_task::queue_state(CLI::App* app,
                                           const socket_subcommand_data_t& opt) noexcept
{
    auto* sub = app->add_subcommand("queue-state", "Set task queue-state");

    const auto run_subcommand = [opt]() { opt->property = "queue_state"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_set_task_queue_state_run(app, opt);
    setup_subcommand_set_task_queue_state_pause(app, opt);
    setup_subcommand_set_task_queue_state_queue(app, opt);
    setup_subcommand_set_task_queue_state_queued(app, opt);
    setup_subcommand_set_task_queue_state_stop(app, opt);
}
