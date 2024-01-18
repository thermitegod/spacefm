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
#include <ztd/ztd_logger.hxx>

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
    volume(const std::shared_ptr<vfs::device>& device);
    ~volume() = default;
    // ~volume() { ztd::logger::debug("vfs::volume::~volume({})", ztd::logger::utils::ptr(this)); };

    static const std::shared_ptr<vfs::volume>
    create(const std::shared_ptr<vfs::device>& device) noexcept;

    using callback_t = void (*)(const std::shared_ptr<vfs::volume>& volume,
                                const vfs::volume::state state, void* user_data);

  public:
    const std::string_view display_name() const noexcept;
    const std::string_view mount_point() const noexcept;
    const std::string_view device_file() const noexcept;
    const std::string_view fstype() const noexcept;
    const std::string_view icon() const noexcept;
    const std::string_view udi() const noexcept;
    const std::string_view label() const noexcept;

    dev_t devnum() const noexcept;
    u64 size() const noexcept;

    const std::optional<std::string> device_mount_cmd() noexcept;
    const std::optional<std::string> device_unmount_cmd() noexcept;

    bool is_device_type(const vfs::volume::device_type type) const noexcept;

    bool is_mounted() const noexcept;
    bool is_removable() const noexcept;
    bool is_mountable() const noexcept;

    bool is_user_visible() const noexcept;

    bool is_optical() const noexcept;
    bool requires_eject() const noexcept;

    bool ever_mounted() const noexcept;

    // private:
    void set_info() noexcept;

    void device_added() noexcept;

  private:
    dev_t devnum_{0};
    std::string device_file_{};
    std::string udi_{};
    std::string disp_name_{};
    std::string icon_{};
    std::string mount_point_{};
    u64 size_{0};
    std::string label_{};
    std::string fstype_{};

    vfs::volume::device_type device_type_{vfs::volume::device_type::block};

    bool is_mounted_{false};
    bool is_removable_{false};
    bool is_mountable_{false};

    bool is_user_visible_{false};

    bool is_optical_{false};
    bool requires_eject_{false};

    bool ever_mounted_{false};
};
} // namespace vfs

bool vfs_volume_init();
void vfs_volume_finalize();

const std::span<const std::shared_ptr<vfs::volume>> vfs_volume_get_all_volumes();

void vfs_volume_add_callback(vfs::volume::callback_t cb, void* user_data);
void vfs_volume_remove_callback(vfs::volume::callback_t cb, void* user_data);

const std::shared_ptr<vfs::volume> vfs_volume_get_by_device(const std::string_view device_file);

bool vfs_volume_dir_avoid_changes(const std::filesystem::path& dir);

bool is_path_mountpoint(const std::filesystem::path& path);
