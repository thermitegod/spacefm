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

#include <string>
#include <string_view>

#include <span>

#include <optional>

#include <memory>

#include <ztd/ztd.hxx>

// #include "logger.hxx"

namespace vfs
{
struct device;

struct volume : public std::enable_shared_from_this<volume>
{
    enum class state
    {
        added,
        removed,
        mounted,   // Not implemented
        unmounted, // Not implemented
        eject,
        changed,
    };

    enum class device_type
    {
        block,
        network,
        other, // eg fuseiso mounted file
    };

    volume() = delete;
    volume(const std::shared_ptr<vfs::device>& device) noexcept;
    ~volume() = default;
    // ~volume() { logger::debug<logger::domain::vfs>("vfs::volume::~volume({})", logger::utils::ptr(this)); };
    volume(const volume& other) = delete;
    volume(volume&& other) = delete;
    volume& operator=(const volume& other) = delete;
    volume& operator=(volume&& other) = delete;

    [[nodiscard]] static const std::shared_ptr<vfs::volume>
    create(const std::shared_ptr<vfs::device>& device) noexcept;

    using callback_t = void (*)(const std::shared_ptr<vfs::volume>& volume,
                                const vfs::volume::state state, void* user_data);

  public:
    [[nodiscard]] const std::string_view display_name() const noexcept;
    [[nodiscard]] const std::string_view mount_point() const noexcept;
    [[nodiscard]] const std::string_view device_file() const noexcept;
    [[nodiscard]] const std::string_view fstype() const noexcept;
    [[nodiscard]] const std::string_view icon() const noexcept;
    [[nodiscard]] const std::string_view udi() const noexcept;
    [[nodiscard]] const std::string_view label() const noexcept;

    [[nodiscard]] dev_t devnum() const noexcept;
    [[nodiscard]] u64 size() const noexcept;

    [[nodiscard]] const std::optional<std::string> device_mount_cmd() noexcept;
    [[nodiscard]] const std::optional<std::string> device_unmount_cmd() noexcept;

    [[nodiscard]] bool is_device_type(const vfs::volume::device_type type) const noexcept;

    [[nodiscard]] bool is_mounted() const noexcept;
    [[nodiscard]] bool is_removable() const noexcept;
    [[nodiscard]] bool is_mountable() const noexcept;

    [[nodiscard]] bool is_user_visible() const noexcept;

    [[nodiscard]] bool is_optical() const noexcept;
    [[nodiscard]] bool requires_eject() const noexcept;

    [[nodiscard]] bool ever_mounted() const noexcept;

    // private:
    void set_info() noexcept;

    void device_added() noexcept;

  private:
    dev_t devnum_{0};
    std::string device_file_;
    std::string udi_;
    std::string disp_name_;
    std::string icon_;
    std::string mount_point_;
    u64 size_{0};
    std::string label_;
    std::string fstype_;

    vfs::volume::device_type device_type_{vfs::volume::device_type::block};

    bool is_mounted_{false};
    bool is_removable_{false};
    bool is_mountable_{false};

    bool is_user_visible_{false};

    bool is_optical_{false};
    bool requires_eject_{false};

    bool ever_mounted_{false};
};

bool volume_init() noexcept;
void volume_finalize() noexcept;

const std::span<const std::shared_ptr<vfs::volume>> volume_get_all_volumes() noexcept;

void volume_add_callback(vfs::volume::callback_t cb, void* user_data) noexcept;
void volume_remove_callback(vfs::volume::callback_t cb, void* user_data) noexcept;

const std::shared_ptr<vfs::volume>
volume_get_by_device(const std::string_view device_file) noexcept;

bool volume_dir_avoid_changes(const std::filesystem::path& dir) noexcept;

bool is_path_mountpoint(const std::filesystem::path& path) noexcept;
} // namespace vfs
