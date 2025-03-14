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

#include <algorithm>
#include <expected>
#include <filesystem>
#include <flat_map>
#include <print>

#include <cstdint>

#include <magic_enum/magic_enum.hpp>

#include <CLI/CLI.hpp>

#include <ztd/ztd.hxx>

#include "commandline/commandline.hxx"

#include "vfs/user-dirs.hxx"

#include "logger.hxx"

struct opts_data final
{
    std::vector<std::filesystem::path> files;

    bool new_tab{true};
    bool reuse_tab{false};
    bool no_tabs{false};
    bool new_window{false};

    std::int32_t panel{0};

    std::filesystem::path config_dir;

    std::vector<std::string> raw_log_levels;
    std::flat_map<std::string, std::string> log_levels;
    // std::filesystem::path logfile{"/tmp/test.log"};
    std::filesystem::path logfile;

    bool build_debug{false};
    bool version{false};
};

static void
run_commandline(const std::shared_ptr<opts_data>& opt) noexcept
{
    if (!opt->config_dir.empty())
    {
        if (!std::filesystem::exists(opt->config_dir))
        {
            std::filesystem::create_directories(opt->config_dir);
            std::filesystem::permissions(opt->config_dir, std::filesystem::perms::owner_all);
        }

        vfs::program::config(opt->config_dir);
    }

    if (opt->version)
    {
        std::println("{} {}", PACKAGE_NAME_FANCY, PACKAGE_VERSION);
        std::exit(EXIT_SUCCESS);
    }

#if defined(DEV_MODE)
    if (opt->build_debug)
    {
        std::println("PACKAGE_NAME          = {}", PACKAGE_NAME);
        std::println("PACKAGE_NAME_FANCY    = {}", PACKAGE_NAME_FANCY);
        std::println("PACKAGE_VERSION       = {}", PACKAGE_VERSION);
        std::println("PACKAGE_GITHUB        = {}", PACKAGE_GITHUB);
        std::println("PACKAGE_BUGREPORT     = {}", PACKAGE_BUGREPORT);
        std::println("PACKAGE_ONLINE_DOCS   = {}", PACKAGE_ONLINE_DOCS);
        std::println("PACKAGE_BUILD_ROOT    = {}", PACKAGE_BUILD_ROOT);
        std::println("DIALOG_BUILD_ROOT     = {}", DIALOG_BUILD_ROOT);
        std::println("PACKAGE_IMAGES        = {}", PACKAGE_IMAGES);
        std::println("PACKAGE_IMAGES_LOCAL  = {}", PACKAGE_IMAGES_LOCAL);
        std::println("DEV_SCRIPTS_PATH      = {}", DEV_SCRIPTS_PATH);
        std::exit(EXIT_SUCCESS);
    }
#endif

    logger::initialize(opt->log_levels, opt->logfile);
}

static void
setup_commandline(CLI::App& app, const std::shared_ptr<opts_data>& opt) noexcept
{
    app.add_flag("-t,--new-tab",
                 opt->new_tab,
                 "Open directories in new tab of last window (default)");
    app.add_flag("-r,--reuse-tab",
                 opt->reuse_tab,
                 "Open directory in current tab of last used window");
    app.add_flag("-n,--no-saved-tab", opt->no_tabs, "Do not load saved tabs");
    app.add_flag("-w,--new-window", opt->new_window, "Open directories in new window");

    app.add_option("-c,--config", opt->config_dir, "Set configuration directory")
        ->expected(1)
        ->check(
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
            });

    app.add_option("--loglevel", opt->raw_log_levels, "Set the loglevel. Format: domain=level")
        ->check(
            [&opt](const auto& value)
            {
                auto log_levels = magic_enum::enum_names<logger::detail::loglevel>();
                auto valid_domains = magic_enum::enum_names<logger::domain>();

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

    app.add_option("--logfile", opt->logfile, "absolute path to the logfile")
        ->expected(1)
        ->check(
            [](const std::filesystem::path& input)
            {
                if (input.is_absolute())
                {
                    return std::string();
                }
                return std::format("Logfile path must be absolute: {}", input.string());
            });

#if defined(DEV_MODE)
    app.add_flag("--build-debug", opt->build_debug, "Show build information");
#endif

    app.add_flag("-v,--version", opt->version, "Show version information");

    // Everything else
    app.add_option("files", opt->files, "[DIR | FILE | URL]...")->expected(0, -1);

    app.callback([opt]() { run_commandline(opt); });
}

std::expected<commandline::opts, std::string>
commandline::run(int argc, char* argv[]) noexcept
{
    CLI::App app{PACKAGE_NAME_FANCY, "A multi-panel tabbed file manager"};

    auto opt = std::make_shared<opts_data>();
    setup_commandline(app, opt);

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return std::unexpected{e.what()};
    }

    return commandline::opts{.files = opt->files,
                             .new_tab = opt->new_tab,
                             .reuse_tab = opt->reuse_tab,
                             .no_tabs = opt->no_tabs,
                             .new_window = opt->new_window,
                             .panel = opt->panel};
}
