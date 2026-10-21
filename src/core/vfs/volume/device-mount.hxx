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
#include <string>
#include <string_view>

#include <cstdint>

#include <sys/types.h>

#include <ztd/ztd.hxx>

namespace vfs
{
class device_mount
{
  private:
    explicit device_mount(const ztd::statmount& statmount) noexcept;

  public:
    device_mount() = delete;
    device_mount(const device_mount&) = default;
    device_mount(device_mount&&) noexcept = default;
    device_mount& operator=(const device_mount&) = default;
    device_mount& operator=(device_mount&&) noexcept = default;
    ~device_mount() = default;

    [[nodiscard]] static std::shared_ptr<device_mount>
    create(const ztd::statmount& statmount) noexcept;

    [[nodiscard]] std::uint64_t mount_id() const noexcept;
    [[nodiscard]] std::uint32_t dev_major() const noexcept;
    [[nodiscard]] std::uint32_t dev_minor() const noexcept;
    [[nodiscard]] std::string_view fs_type() const noexcept;
    [[nodiscard]] const std::filesystem::path& mount_point() const noexcept;
    [[nodiscard]] const std::filesystem::path& root() const noexcept;

  private:
    std::uint64_t mount_id_;
    std::uint32_t dev_major_;
    std::uint32_t dev_minor_;
    std::string fs_type_;
    std::filesystem::path mount_point_;
    std::filesystem::path root_;
};
} // namespace vfs
