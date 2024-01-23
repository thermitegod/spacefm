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

#include <string>
#include <string_view>

#include <optional>

#include <memory>

#include <ztd/ztd.hxx>

#include "vfs/libudevpp/libudevpp.hxx"

namespace vfs
{
struct device
{
    device() = delete;
    device(const libudev::device& udevice);
    ~device() = default;

    [[nodiscard]] static const std::shared_ptr<vfs::device>
    create(const libudev::device& udevice) noexcept;

    libudev::device udevice;

    [[nodiscard]] dev_t devnum() const noexcept;

    [[nodiscard]] const std::string_view devnode() const noexcept;
    [[nodiscard]] const std::string_view native_path() const noexcept;
    [[nodiscard]] const std::string_view mount_points() const noexcept;

    [[nodiscard]] bool is_valid() const noexcept;

    [[nodiscard]] bool is_system_internal() const noexcept;
    [[nodiscard]] bool is_removable() const noexcept;
    [[nodiscard]] bool is_media_available() const noexcept;
    [[nodiscard]] bool is_optical_disc() const noexcept;
    [[nodiscard]] bool is_mounted() const noexcept;
    [[nodiscard]] bool is_media_ejectable() const noexcept;

    [[nodiscard]] const std::string_view id() const noexcept;
    [[nodiscard]] const std::string_view id_label() const noexcept;
    [[nodiscard]] u64 size() const noexcept;
    [[nodiscard]] u64 block_size() const noexcept;
    [[nodiscard]] const std::string_view fstype() const noexcept;

  private:
    dev_t devnum_{0};

    std::string devnode_{};
    std::string native_path_{};
    std::string mount_points_{};

    bool is_valid_{false};

    bool is_system_internal_{true};
    bool is_removable_{false};
    bool is_media_available_{false};
    bool is_optical_disc_{false};
    bool is_mounted_{false};
    bool is_media_ejectable_{false};

    std::string id_{};
    u64 size_{0};
    u64 block_size_{0};
    std::string id_label_{};
    std::string fstype_{};

  private:
    [[nodiscard]] const std::optional<std::string> info_mount_points() const noexcept;
    [[nodiscard]] bool device_get_info() noexcept;
};
} // namespace vfs
