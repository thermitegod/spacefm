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

#include <filesystem>

#include <vector>
#include <unordered_map>

#include <memory>

#include <optional>

#include <libudev.h>

namespace libudev
{
struct monitor;
struct device;
struct enumerate;

/**
 * Class representing a udev context
 */
struct udev
{
  public:
    explicit udev() noexcept : handle(udev_new(), &udev_unref) {}
    udev(const udev& other) = default;
    udev(udev&& other) noexcept : handle(std::move(other.handle)) {}
    udev& operator=(const udev& other) = default;
    udev& operator=(udev&& other) = default;
    ~udev() = default;

    enum class netlink_type
    {
        udev,
        kernel,
    };

    /**
     * Create new udev monitor for netlink described by @name.
     * @param name Name can be "udev" or "kernel" (default is "udev")
     * @return A {@link monitor} instance
     */
    [[nodiscard]] const std::optional<monitor>
    monitor_new_from_netlink(const std::string_view name = "udev") const noexcept;
    [[nodiscard]] const std::optional<monitor>
    monitor_new_from_netlink(const netlink_type name = netlink_type::udev) const noexcept;

    [[nodiscard]] const std::optional<device>
    device_from_syspath(const std::filesystem::path& syspath) const noexcept;

    enum class device_type
    {
        block,
        character,
    };

    [[nodiscard]] const std::optional<device> device_from_devnum(const char type,
                                                                 const dev_t devnum) const noexcept;
    [[nodiscard]] const std::optional<device> device_from_devnum(const device_type type,
                                                                 const dev_t devnum) const noexcept;

    /**
     * Create new udev enumerator
     * @return A {@link enumerator} instance which can be used to enumerate devices known to
     * udev
     */
    [[nodiscard]] enumerate enumerate_new() const noexcept;

    [[nodiscard]] bool is_initialized() const noexcept;

  private:
    std::shared_ptr<struct ::udev> handle;
};

/**
 * Class that encapsulates monitoring functionality provided by udev
 */
struct monitor
{
  public:
    monitor() = default;
    monitor(struct ::udev_monitor* device) noexcept : handle(device, &udev_monitor_unref) {}
    monitor(const monitor& other) = default;
    monitor(monitor&& other) noexcept : handle(std::move(other.handle)) {}
    monitor& operator=(const monitor& other) = default;
    monitor& operator=(monitor&& other) = default;
    ~monitor() = default;

    [[nodiscard]] bool enable_receiving() const noexcept;
    [[nodiscard]] int get_fd() const noexcept;

    [[nodiscard]] const std::optional<device> receive_device() const noexcept;

    [[nodiscard]] bool
    filter_add_match_subsystem_devtype(const std::string_view subsystem) const noexcept;
    [[nodiscard]] bool
    filter_add_match_subsystem_devtype(const std::string_view subsystem,
                                       const std::string_view devtype) const noexcept;

    [[nodiscard]] bool filter_add_match_tag(const std::string_view tag) const noexcept;

    [[nodiscard]] bool is_initialized() const noexcept;

  private:
    std::shared_ptr<struct ::udev_monitor> handle;
};

/**
 * Class that encapsulated enumeration functionality provided by udev
 */
struct enumerate
{
  public:
    enumerate() = default;
    enumerate(struct ::udev_enumerate* enumerate) noexcept
        : handle(enumerate, &udev_enumerate_unref)
    {
    }
    enumerate(const enumerate& other) = default;
    enumerate(enumerate&& other) noexcept : handle(std::move(other.handle)) {}
    enumerate& operator=(const enumerate& other) = default;
    enumerate& operator=(enumerate&& other) = default;
    ~enumerate() = default;

    [[nodiscard]] bool is_initialized() const noexcept;

    void add_match_subsystem(const std::string_view subsystem) const noexcept;
    void add_nomatch_subsystem(const std::string_view subsystem) const noexcept;

    void add_match_sysattr(const std::string_view sysattr,
                           const std::string_view value = "") const noexcept;
    void add_nomatch_sysattr(const std::string_view sysattr,
                             const std::string_view value = "") const noexcept;

    void add_match_property(const std::string_view property,
                            const std::string_view value) const noexcept;

    void add_match_tag(const std::string_view tag) const noexcept;

    void add_match_sysname(const std::string_view sysname) const noexcept;

    void add_match_parent(const device& device) const noexcept;

    void add_match_is_initialized() const noexcept;

    void scan_devices() const noexcept;
    void scan_subsystems() const noexcept;

    [[nodiscard]] const std::vector<device> enumerate_devices() const noexcept;

  private:
    std::shared_ptr<struct ::udev_enumerate> handle;
};

/**
 * Class that encapsulates the concept of a device as described by dev
 */
struct device
{
  public:
    device() = default;
    device(struct ::udev_device* device) noexcept : handle(device, &udev_device_unref) {}
    device(const device& other) = default;
    device(device&& other) noexcept : handle(std::move(other.handle)) {}
    device& operator=(const device& other) = default;
    device& operator=(device&& other) = default;
    ~device() = default;

    [[nodiscard]] bool is_initialized() const noexcept;

    [[nodiscard]] bool has_action() const noexcept;
    [[nodiscard]] const std::optional<std::string> get_action() const noexcept;

    [[nodiscard]] bool has_devnode() const noexcept;
    [[nodiscard]] const std::optional<std::string> get_devnode() const noexcept;

    [[nodiscard]] dev_t get_devnum() const noexcept;

    [[nodiscard]] bool has_devtype() const noexcept;
    [[nodiscard]] const std::optional<std::string> get_devtype() const noexcept;

    [[nodiscard]] bool has_subsystem() const noexcept;
    [[nodiscard]] const std::optional<std::string> get_subsystem() const noexcept;

    [[nodiscard]] bool has_devpath() const noexcept;
    [[nodiscard]] const std::optional<std::string> get_devpath() const noexcept;

    [[nodiscard]] bool has_syspath() const noexcept;
    [[nodiscard]] const std::optional<std::filesystem::path> get_syspath() const noexcept;

    [[nodiscard]] bool has_sysname() const noexcept;
    [[nodiscard]] const std::optional<std::string> get_sysname() const noexcept;

    [[nodiscard]] bool has_sysnum() const noexcept;
    [[nodiscard]] const std::optional<std::string> get_sysnum() const noexcept;

    [[nodiscard]] bool has_driver() const noexcept;
    [[nodiscard]] const std::optional<std::string> get_driver() const noexcept;

    [[nodiscard]] bool has_sysattr(const std::string_view named) const noexcept;
    [[nodiscard]] const std::optional<std::string>
    get_sysattr(const std::string_view named) const noexcept;
    [[nodiscard]] bool set_sysattr(const std::string_view named,
                                   const std::string_view value) const noexcept;
    [[nodiscard]] const std::vector<std::string> get_sysattr_keys() const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, std::string>
    get_sysattr_map() const noexcept;

    [[nodiscard]] const std::vector<std::string> get_devlinks() const noexcept;

    [[nodiscard]] bool has_property(const std::string_view named) const noexcept;
    [[nodiscard]] const std::optional<std::string>
    get_property(const std::string_view named) const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, std::string>
    get_properties() const noexcept;

    [[nodiscard]] bool has_tag(const std::string_view named) const noexcept;
    [[nodiscard]] const std::vector<std::string> get_tags() const noexcept;

    [[nodiscard]] bool has_current_tag(const std::string_view named) const noexcept;
    [[nodiscard]] const std::vector<std::string> get_current_tags() const noexcept;

    [[nodiscard]] const std::optional<device> get_parent_device() const noexcept;
    [[nodiscard]] const std::optional<device>
    get_parent_device(const std::string_view subsystem, const std::string_view type) const noexcept;

    [[nodiscard]] bool is_disk() const noexcept;
    [[nodiscard]] bool is_partition() const noexcept;

    [[nodiscard]] bool is_usb() const noexcept;
    [[nodiscard]] bool is_cdrom() const noexcept;
    [[nodiscard]] bool is_hdd() const noexcept;
    [[nodiscard]] bool is_ssd() const noexcept;
    [[nodiscard]] bool is_nvme() const noexcept;

    [[nodiscard]] bool is_hotswapable() const noexcept;

    [[nodiscard]] bool is_removable() const noexcept;
    [[nodiscard]] bool is_internal() const noexcept;

  private:
    std::shared_ptr<struct ::udev_device> handle;
};
} // namespace libudev
