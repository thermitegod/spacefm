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

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "vfs/libudevpp/libudevpp.hxx"

namespace vfs
{
class device;

class volume : public std::enable_shared_from_this<volume>
{
  public:
    enum class device_type : std::uint8_t
    {
        block,
        network,
        other, // eg fuseiso mounted file
    };

  private:
    volume() = delete;
    volume(const std::shared_ptr<vfs::device>& device) noexcept;
    ~volume() = default;
    volume(const volume& other) = delete;
    volume(volume&& other) = delete;
    volume& operator=(const volume& other) = delete;
    volume& operator=(volume&& other) = delete;

  public:
    [[nodiscard]] static std::shared_ptr<vfs::volume>
    create(const std::shared_ptr<vfs::device>& device) noexcept;

    void update_from_device(const std::shared_ptr<vfs::device>& device) noexcept;

    [[nodiscard]] std::string_view display_name() const noexcept;
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

    [[nodiscard]] bool is_device_type(const vfs::volume::device_type type) const noexcept;

    [[nodiscard]] bool is_mounted() const noexcept;
    [[nodiscard]] bool is_removable() const noexcept;
    [[nodiscard]] bool is_mountable() const noexcept;

    [[nodiscard]] bool is_user_visible() const noexcept;

    [[nodiscard]] bool is_optical() const noexcept;
    [[nodiscard]] bool requires_eject() const noexcept;

    [[nodiscard]] bool ever_mounted() const noexcept;

  private:
    void set_info() noexcept;

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

  public:
    [[nodiscard]] auto
    signal_mounted() noexcept
    {
        return signal_mounted_;
    }

    [[nodiscard]] auto
    signal_unmounted() noexcept
    {
        return signal_unmounted_;
    }

  private:
    // Mount point
    sigc::signal<void(std::filesystem::path)> signal_mounted_;
    sigc::signal<void(std::filesystem::path)> signal_unmounted_;
};

class volume_manager
{
  private:
    volume_manager();

  public:
    [[nodiscard]] static std::shared_ptr<vfs::volume_manager> create() noexcept;

    [[nodiscard]] std::span<const std::shared_ptr<vfs::volume>> get_all() noexcept;

    [[nodiscard]] bool avoid_changes(const std::filesystem::path& dir) const noexcept;

  private:
    class device_mount
    {
      protected:
        device_mount() = delete;
        device_mount(dev_t major, dev_t minor) noexcept;
        ~device_mount() = default;
        device_mount(const device_mount& other) = delete;
        device_mount(device_mount&& other) = delete;
        device_mount& operator=(const device_mount& other) = delete;
        device_mount& operator=(device_mount&& other) = delete;

      public:
        [[nodiscard]] static std::shared_ptr<device_mount> create(dev_t major,
                                                                  dev_t minor) noexcept;

        dev_t major{0};
        dev_t minor{0};
        std::string mount_points;
        std::string fstype;
        std::vector<std::string> mounts;
    };

    [[nodiscard]] std::optional<std::string> devmount_fstype(const dev_t device) const noexcept;
    [[nodiscard]] std::shared_ptr<vfs::volume>
    read_by_device(const libudev::device& udevice) const noexcept;

    void parse_mounts(const bool report) noexcept;

    std::vector<std::shared_ptr<vfs::volume>> volumes_;

    libudev::udev udev_;
    libudev::monitor umonitor_;

    Glib::RefPtr<Glib::IOChannel> uchannel_ = nullptr;
    Glib::RefPtr<Glib::IOChannel> mchannel_ = nullptr;

    std::vector<std::shared_ptr<device_mount>> devmounts_;

  public:
    [[nodiscard]] auto
    signal_added() noexcept
    {
        return signal_added_;
    }

    [[nodiscard]] auto
    signal_removed() noexcept
    {
        return signal_removed_;
    }

    [[nodiscard]] auto
    signal_changed() noexcept
    {
        return signal_changed_;
    }

    [[nodiscard]] auto
    signal_eject() noexcept
    {
        return signal_eject_;
    }

    [[nodiscard]] auto
    signal_mounted() noexcept
    {
        return signal_mounted_;
    }

    [[nodiscard]] auto
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
