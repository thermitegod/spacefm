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

#include <vector>

#include <cstdlib>

#include <CLI/CLI.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dirs.hxx"

#include "settings/app.hxx"

#include "commandline/socket.hxx"

#include "commandline/commandline.hxx"

void
run_commandline(const commandline_opt_data_t& opt)
{
    (void)opt;

    if (!opt->config_dir.empty())
    {
        vfs::user_dirs->program_config_dir(opt->config_dir);
    }

    if (opt->git_backed_settings != app_settings.get_git_backed_settings())
    {
        app_settings.set_git_backed_settings(opt->git_backed_settings);
    }

    if (opt->version)
    {
        fmt::print("{} {}\n", PACKAGE_NAME_FANCY, PACKAGE_VERSION);
        std::exit(EXIT_SUCCESS);
    }
}

void
setup_commandline(CLI::App& app, const commandline_opt_data_t& opt)
{
    // clang-format off
    app.add_flag("-t,--new-tab", opt->new_tab, "Open directories in new tab of last window (default)");
    app.add_flag("-r,--reuse-tab", opt->reuse_tab, "Open directory in current tab of last used window");
    app.add_flag("-n,--no-saved-tab", opt->no_tabs, "Do not load saved tabs");
    app.add_flag("-w,--new-window", opt->new_window, "Open directories in new window");
    app.add_option("-p,--panel", opt->panel, "Open directories in panel (1-4)")->expected(1);
    app.add_option("-c,--config", opt->config_dir, "Set configuration directory")->expected(1);
    app.add_flag("-g,--no-git-backed-settings", opt->git_backed_settings, "Do not use git to keep session history");
    app.add_flag("-f,--find-files", opt->find_files, "Show File Search");
    app.add_flag("-v,--version", opt->version, "Show version information");
    // clang-format on

    // build socket subcommands
    setup_subcommand_socket(app);

    // Everything else
    app.add_option("files", opt->files, "[DIR | FILE | URL]...")->expected(0, -1);

    app.callback([opt]() { run_commandline(opt); });
}
