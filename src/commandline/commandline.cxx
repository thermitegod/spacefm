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

#include <filesystem>

#include <algorithm>

#include <print>

#include <CLI/CLI.hpp>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "vfs/vfs-user-dirs.hxx"

#include "logger.hxx"
#include "types.hxx"

#include "settings/settings.hxx"

#if defined(HAVE_SOCKET)
#include "commandline/socket.hxx"
#endif

#include "commandline/commandline.hxx"

void
run_commandline(const commandline_opt_data_t& opt) noexcept
{
    (void)opt;

    if (!opt->config_dir.empty())
    {
        if (!std::filesystem::exists(opt->config_dir))
        {
            std::filesystem::create_directories(opt->config_dir);
            std::filesystem::permissions(opt->config_dir, std::filesystem::perms::owner_all);
        }

        vfs::program::config(opt->config_dir);
    }

    if (opt->git_backed_settings != config::settings.git_backed_settings)
    {
        config::settings.git_backed_settings = opt->git_backed_settings;
    }

    if (opt->version)
    {
        std::println("{} {}", PACKAGE_NAME_FANCY, PACKAGE_VERSION);
#if defined(HAVE_SOCKET)
        std::println("Socket Port: {}", SOCKET_PORT);
#endif
        std::exit(EXIT_SUCCESS);
    }

    logger::initialize(opt->log_levels, opt->logfile);
}

void
setup_commandline(CLI::App& app, const commandline_opt_data_t& opt) noexcept
{
    app.add_flag("-t,--new-tab",
                 opt->new_tab,
                 "Open directories in new tab of last window (default)");
    app.add_flag("-r,--reuse-tab",
                 opt->reuse_tab,
                 "Open directory in current tab of last used window");
    app.add_flag("-n,--no-saved-tab", opt->no_tabs, "Do not load saved tabs");
    app.add_flag("-w,--new-window", opt->new_window, "Open directories in new window");

    static constexpr std::array<panel_t, 4> panels = {panel_1, panel_2, panel_3, panel_4};
    app.add_option("-p,--panel", opt->panel, "Open directories in panel")
        ->expected(1)
        ->check(CLI::IsMember(panels));

    const auto is_config_path_valid = CLI::Validator(
        [](const std::filesystem::path& input)
        {
            if (input.is_absolute())
            {
                if (std::filesystem::exists(input) && !std::filesystem::is_directory(input))
                {
                    return std::format("Config path must be a directory: {}", input.string());
                }

                // Validate pass
                return std::string();
            }
            return std::format("Config path must be absolute: {}", input.string());
        },
        "");
    app.add_option("-c,--config", opt->config_dir, "Set configuration directory")
        ->expected(1)
        ->check(is_config_path_valid);

    app.add_flag("-g,--no-git-backed-settings",
                 opt->git_backed_settings,
                 "Do not use git to keep session history");

    app.add_option("--loglevel", opt->raw_log_levels, "Set the loglevel. Format: domain=level")
        ->check(
            [&opt](const auto& value)
            {
                constexpr auto log_levels = magic_enum::enum_names<spdlog::level::level_enum>();
                constexpr auto valid_domains = magic_enum::enum_names<logger::domain>();

                const auto pos = value.find('=');
                if (pos == std::string::npos)
                {
                    return std::string("Must be in format domain=level");
                }

                const auto domain = value.substr(0, pos);
                if (!std::ranges::contains(valid_domains, domain))
                {
                    return std::format("Invalid domain: {}", domain);
                }

                const auto level = value.substr(pos + 1);
                if (!std::ranges::contains(log_levels, level))
                {
                    return std::format("Invalid log level: {}", level);
                }

                opt->log_levels.insert({domain, level});

                return std::string();
            });

    const auto is_logfile_path_valid = CLI::Validator(
        [](const std::filesystem::path& input)
        {
            if (input.is_absolute())
            {
                return std::string();
            }
            return std::format("Logfile path must be absolute: {}", input.string());
        },
        "");
    app.add_option("--logfile", opt->logfile, "absolute path to the logfile")
        ->expected(1)
        ->check(is_logfile_path_valid);

    app.add_flag("-v,--version", opt->version, "Show version information");

#if defined(HAVE_SOCKET)
    // build socket subcommands
    setup_subcommand_socket(app);
#endif

    // Everything else
    app.add_option("files", opt->files, "[DIR | FILE | URL]...")->expected(0, -1);

    app.callback([opt]() { run_commandline(opt); });
}
