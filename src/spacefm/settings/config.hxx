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

#pragma once

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <stop_token>
#include <thread>

#include <cstdint>

#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "vfs/user-dirs.hxx"

namespace config
{
struct config_file_format final
{
    std::uint64_t version{version};
    config::settings_on_disk settings;
};

class manager
{
  public:
    manager(const std::shared_ptr<config::settings>& settings);
    ~manager();

    manager(const manager&) = delete;
    manager& operator=(const manager&) = delete;
    manager(manager&&) = delete;
    manager& operator=(manager&&) = delete;

    void load() noexcept;
    void save() noexcept;

  private:
    std::shared_ptr<config::settings> settings_;
    std::filesystem::path file_ = vfs::program::config() / "experimental-config.json";
    std::uint64_t version_ = 400; // 4.0.0

    // Autosave
    void run(const std::stop_token& stoken) noexcept;
    void run_once(const std::stop_token& stoken) noexcept;

    void request_add() noexcept;
    void request_cancel() noexcept;

    bool pending_ = false;
    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::chrono::minutes thread_sleep_ = std::chrono::minutes(5);
    std::jthread autosave_thread_;

  public:
    [[nodiscard]] auto
    signal_load_error() noexcept
    {
        return signal_load_error_;
    }

    [[nodiscard]] auto
    signal_save_error() noexcept
    {
        return signal_save_error_;
    }

  private:
    sigc::signal<void(std::string)> signal_load_error_;
    sigc::signal<void(std::string)> signal_save_error_;
};
} // namespace config
