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

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <sys/types.h>

#include <sigc++/sigc++.h>

#include "libudevpp/libudevpp.hxx"

namespace vfs
{
class volume;
class device_mount;

class volume_manager
{
  private:
    volume_manager();

  public:
    volume_manager(const volume_manager& other) = delete;
    volume_manager(volume_manager&& other) = delete;
    volume_manager& operator=(const volume_manager& other) = delete;
    volume_manager& operator=(volume_manager&& other) = delete;
    ~volume_manager();

    [[nodiscard]] static std::shared_ptr<vfs::volume_manager> create() noexcept;

    [[nodiscard]] std::span<const std::shared_ptr<vfs::volume>> get_all() noexcept;

    [[nodiscard]] bool avoid_changes(const std::filesystem::path& dir) const noexcept;

  private:
    [[nodiscard]] std::optional<std::string_view>
    devmount_fstype(const dev_t device) const noexcept;
    [[nodiscard]] std::shared_ptr<vfs::volume>
    read_by_device(const libudev::device& udevice) const noexcept;

    void parse_mounts(const bool report) noexcept;

    std::vector<std::shared_ptr<vfs::volume>> volumes_;

    libudev::udev udev_;
    libudev::monitor umonitor_;

    Glib::RefPtr<Glib::IOChannel> uchannel_ = nullptr;
    Glib::RefPtr<Glib::IOChannel> mchannel_ = nullptr;
    sigc::scoped_connection signal_uchannel_;
    sigc::scoped_connection signal_mchannel_;

    std::vector<std::shared_ptr<device_mount>> devmounts_;

  public:
    [[nodiscard]] auto&
    signal_added() noexcept
    {
        return signal_added_;
    }

    [[nodiscard]] auto&
    signal_removed() noexcept
    {
        return signal_removed_;
    }

    [[nodiscard]] auto&
    signal_changed() noexcept
    {
        return signal_changed_;
    }

    [[nodiscard]] auto&
    signal_eject() noexcept
    {
        return signal_eject_;
    }

    [[nodiscard]] auto&
    signal_mounted() noexcept
    {
        return signal_mounted_;
    }

    [[nodiscard]] auto&
    signal_unmounted() noexcept
    {
        return signal_unmounted_;
    }

  private:
    // Block device
    sigc::signal<void(std::filesystem::path)> signal_added_;
    sigc::signal<void(std::filesystem::path)> signal_removed_;
    sigc::signal<void(std::filesystem::path)> signal_changed_;

    sigc::signal<void(std::filesystem::path)> signal_eject_;

    // Mount point
    sigc::signal<void(std::filesystem::path)> signal_mounted_;
    sigc::signal<void(std::filesystem::path)> signal_unmounted_;
};
} // namespace vfs
