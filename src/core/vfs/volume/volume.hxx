/**
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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
#include <string>
#include <string_view>

#include <sys/types.h>

#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

namespace vfs
{
class device;

class volume : public std::enable_shared_from_this<volume>
{
  private:
    volume(const std::shared_ptr<vfs::device>& device) noexcept;

  public:
    volume() = delete;
    ~volume() = default;
    volume(const volume& other) = delete;
    volume(volume&& other) = delete;
    volume& operator=(const volume& other) = delete;
    volume& operator=(volume&& other) = delete;

    enum class device_type : std::uint8_t
    {
        block,
        network,
        other, // eg fuseiso mounted file
    };

    [[nodiscard]] static std::shared_ptr<vfs::volume>
    create(const std::shared_ptr<vfs::device>& device) noexcept;

    void update_from_device(const std::shared_ptr<vfs::device>& device) noexcept;

    [[nodiscard]] std::string display_name() const noexcept;
    [[nodiscard]] std::string_view mount_point() const noexcept;
    [[nodiscard]] std::string_view device_file() const noexcept;
    [[nodiscard]] std::string_view fstype() const noexcept;
    [[nodiscard]] std::string_view icon() const noexcept;
    [[nodiscard]] std::string_view udi() const noexcept;
    [[nodiscard]] std::string_view label() const noexcept;

    [[nodiscard]] dev_t devnum() const noexcept;
    [[nodiscard]] u64 size() const noexcept;

    [[nodiscard]] std::optional<std::string> device_mount_cmd() noexcept;
    [[nodiscard]] std::optional<std::string> device_unmount_cmd() noexcept;

    [[nodiscard]] bool is_device_type(const device_type type) const noexcept;

    [[nodiscard]] bool is_mounted() const noexcept;
    [[nodiscard]] bool is_removable() const noexcept;
    [[nodiscard]] bool is_mountable() const noexcept;

    [[nodiscard]] bool is_user_visible() const noexcept;

    [[nodiscard]] bool is_optical() const noexcept;
    [[nodiscard]] bool requires_eject() const noexcept;

    [[nodiscard]] bool ever_mounted() const noexcept;

  private:
    std::shared_ptr<device> device_;

    std::string mount_point_;

    device_type device_type_{device_type::block};

    bool ever_mounted_{false};

  public:
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
    // Mount point
    sigc::signal<void(std::filesystem::path)> signal_mounted_;
    sigc::signal<void(std::filesystem::path)> signal_unmounted_;
};
} // namespace vfs
