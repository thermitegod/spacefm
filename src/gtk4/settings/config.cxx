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
#include <mutex>
#include <string>

#include <glaze/glaze.hpp>

#include <ztd/extra/glaze.hxx>
#include <ztd/ztd.hxx>

#include "settings/config.hxx"
#include "settings/settings.hxx"

#include "logger.hxx"

config::manager::manager(const std::shared_ptr<config::settings>& settings) : settings_(settings)
{
    logger::trace<logger::autosave>("starting autosave thread");

    settings_->signal_autosave_request().connect([this]() { request_add(); });
    settings_->signal_autosave_cancel().connect([this]() { request_cancel(); });

    autosave_thread_ = std::jthread([&](const std::stop_token& stoken) { run(stoken); });
    pthread_setname_np(autosave_thread_.native_handle(), "autosave");
}

config::manager::~manager()
{
    autosave_thread_.request_stop();
    autosave_thread_.join();
}

static void
parse_settings(const u64 version, const config::settings_on_disk& loaded,
               const std::shared_ptr<config::settings>& settings) noexcept
{
    (void)version;

    static_cast<config::settings_on_disk&>(*settings) = loaded;
}

void
config::manager::load() noexcept
{
    if (!std::filesystem::exists(file_))
    {
        return;
    }

    config_file_format config_data{};
    std::string buffer;
    const auto ec = glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(config_data,
                                                                                   file_.c_str(),
                                                                                   buffer);

    if (ec)
    {
        // logger::error("Failed to load config file: {}", glz::format_error(ec, buffer));

        signal_load_error().emit(glz::format_error(ec, buffer));
        return;
    }

    parse_settings(config_data.version, config_data.settings, settings_);
}

[[nodiscard]] static config::settings_on_disk
pack_settings(const std::shared_ptr<config::settings>& settings) noexcept
{
    config::settings_on_disk s = *settings;

    return s;
}

void
config::manager::save() noexcept
{
    if (!std::filesystem::exists(file_.parent_path()))
    {
        std::filesystem::create_directories(file_.parent_path());
    }

    const auto config_data = config_file_format{version_, pack_settings(settings_)};

    std::string buffer;
    const auto ec =
        glz::write_file_json<glz::opts{.prettify = true}>(config_data, file_.c_str(), buffer);

    if (ec)
    {
        // logger::error("Failed to write config file: {}", glz::format_error(ec, buffer));

        signal_save_error().emit(glz::format_error(ec, buffer));
    }
}

void
config::manager::run(const std::stop_token& stoken) noexcept
{
    while (!stoken.stop_requested())
    {
        run_once(stoken);
    }
}

void
config::manager::run_once(const std::stop_token& stoken) noexcept
{
    {
        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, stoken, thread_sleep_, [this]() { return pending_; });

        if (stoken.stop_requested())
        {
            return;
        }
    }

    logger::trace<logger::autosave>("checking for pending autosave requests");
    if (pending_)
    {
        std::scoped_lock lock(mutex_);

        logger::trace<logger::autosave>("autosave request, saving settings");

        save();

        pending_ = false;
    }
}

void
config::manager::request_add() noexcept
{
    std::scoped_lock lock(mutex_);

    logger::trace<logger::autosave>("adding request");
    pending_ = true;
}

void
config::manager::request_cancel() noexcept
{
    std::scoped_lock lock(mutex_);

    logger::trace<logger::autosave>("canceling request");
    pending_ = false;
}
