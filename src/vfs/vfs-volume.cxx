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

// udev & mount monitor code by IgnorantGuru
// device info code uses code excerpts from freedesktop's udisks v1.0.4

#include <string>
#include <string_view>

#include <filesystem>

#include <array>
#include <vector>

#include <ranges>

#include <memory>

#include <chrono>

#include <libudev.h>
#include <fcntl.h>
#include <linux/kdev_t.h>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-event-handler.hxx"

#include "main-window.hxx"

#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-location-view.hxx"

#include "utils.hxx"

#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-volume.hxx"

inline constexpr std::string_view MOUNTINFO{"/proc/self/mountinfo"};
inline constexpr std::string_view MTAB{"/proc/mounts"};

inline constexpr std::array<std::string_view, 15> HIDDEN_NON_BLOCK_FS{
    "devpts",
    "proc",
    "fusectl",
    "pstore",
    "sysfs",
    "tmpfs",
    "devtmpfs",
    "ramfs",
    "aufs",
    "overlayfs",
    "cgroup",
    "binfmt_misc",
    "rpc_pipefs",
    "gvfsd-fuse",
    "fuse.gvfsd-fuse",
};

static vfs::volume vfs_volume_read_by_device(struct udev_device* udevice);
static vfs::volume vfs_volume_read_by_mount(dev_t devnum, const char* mount_points);
static void vfs_volume_device_removed(struct udev_device* udevice);
static bool vfs_volume_nonblock_removed(dev_t devnum);
static void call_callbacks(vfs::volume vol, VFSVolumeState state);

VFSVolume::VFSVolume()
{
    this->device_file = nullptr;
    this->udi = nullptr;
    this->disp_name = nullptr;
    this->mount_point = nullptr;
    this->fs_type = nullptr;
}

VFSVolume::~VFSVolume()
{
    if (this->device_file)
        free(this->device_file);
    if (this->udi)
        free(this->udi);
    if (this->disp_name)
        free(this->disp_name);
    if (this->mount_point)
        free(this->mount_point);
    if (this->fs_type)
        free(this->fs_type);
}

struct VFSVolumeCallbackData
{
    VFSVolumeCallbackData() = delete;
    VFSVolumeCallbackData(VFSVolumeCallback callback, void* callback_data);

    VFSVolumeCallback cb;
    void* user_data;
};

VFSVolumeCallbackData::VFSVolumeCallbackData(VFSVolumeCallback callback, void* callback_data)
{
    this->cb = callback;
    this->user_data = callback_data;
}

using volume_callback_data_t = std::shared_ptr<VFSVolumeCallbackData>;

static std::vector<vfs::volume> volumes;
static std::vector<volume_callback_data_t> callbacks;
static bool global_inhibit_auto = false;

struct Devmount
{
    Devmount() = delete;
    Devmount(dev_t major, dev_t minor);
    ~Devmount();

    dev_t major;
    dev_t minor;
    char* mount_points;
    char* fstype;
    std::vector<std::string> mounts;
};

Devmount::Devmount(dev_t major, dev_t minor)
{
    this->major = major;
    this->minor = minor;
    this->mount_points = nullptr;
    this->fstype = nullptr;
}

Devmount::~Devmount()
{
    if (this->mount_points)
        free(this->mount_points);
    if (this->fstype)
        free(this->fstype);
}

using devmount_t = std::shared_ptr<Devmount>;

static std::vector<devmount_t> devmounts;
static struct udev* udev = nullptr;
static struct udev_monitor* umonitor = nullptr;

static Glib::RefPtr<Glib::IOChannel> uchannel = nullptr;
static Glib::RefPtr<Glib::IOChannel> mchannel = nullptr;

/* *************************************************************************
 * device info
 ************************************************************************** */

struct Device
{
    Device() = delete;
    Device(udev_device* udevice);
    ~Device();

    struct udev_device* udevice;

    dev_t devnum{0};

    char* devnode{nullptr};
    char* native_path{nullptr};
    char* mount_points{nullptr};

    bool device_is_system_internal{true};
    bool device_is_partition{false};
    bool device_is_partition_table{false};
    bool device_is_removable{false};
    bool device_is_media_available{false};
    bool device_is_read_only{false};
    bool device_is_drive{false};
    bool device_is_optical_disc{false};
    bool device_is_mounted{false};
    char* device_presentation_hide{nullptr};
    char* device_presentation_nopolicy{nullptr};
    char* device_presentation_name{nullptr};
    char* device_presentation_icon_name{nullptr};
    char* device_automount_hint{nullptr};
    char* device_by_id{nullptr};
    u64 device_size{0};
    u64 device_block_size{0};
    char* id_usage{nullptr};
    char* id_type{nullptr};
    char* id_version{nullptr};
    char* id_uuid{nullptr};
    char* id_label{nullptr};

    char* drive_vendor{nullptr};
    char* drive_model{nullptr};
    char* drive_revision{nullptr};
    char* drive_serial{nullptr};
    char* drive_wwn{nullptr};
    char* drive_connection_interface{nullptr};
    u64 drive_connection_speed{0};
    char* drive_media_compatibility{nullptr};
    char* drive_media{nullptr};
    bool drive_is_media_ejectable{false};
    bool drive_can_detach{false};

    char* filesystem{nullptr};

    char* partition_scheme{nullptr};
    char* partition_number{nullptr};
    char* partition_type{nullptr};
    char* partition_label{nullptr};
    char* partition_uuid{nullptr};
    char* partition_flags{nullptr};
    char* partition_offset{nullptr};
    char* partition_size{nullptr};
    char* partition_alignment_offset{nullptr};

    char* partition_table_scheme{nullptr};
    char* partition_table_count{nullptr};

    bool optical_disc_is_blank{false};
    bool optical_disc_is_appendable{false};
    bool optical_disc_is_closed{false};
    char* optical_disc_num_tracks{nullptr};
    char* optical_disc_num_audio_tracks{nullptr};
    char* optical_disc_num_sessions{nullptr};
};

Device::Device(udev_device* udevice)
{
    this->udevice = udevice;
}

Device::~Device()
{
    if (this->native_path)
        free(this->native_path);
    if (this->mount_points)
        free(this->mount_points);
    if (this->devnode)
        free(this->devnode);

    if (this->device_presentation_hide)
        free(this->device_presentation_hide);
    if (this->device_presentation_nopolicy)
        free(this->device_presentation_nopolicy);
    if (this->device_presentation_name)
        free(this->device_presentation_name);
    if (this->device_presentation_icon_name)
        free(this->device_presentation_icon_name);
    if (this->device_automount_hint)
        free(this->device_automount_hint);
    if (this->device_by_id)
        free(this->device_by_id);
    if (this->id_usage)
        free(this->id_usage);
    if (this->id_type)
        free(this->id_type);
    if (this->id_version)
        free(this->id_version);
    if (this->id_uuid)
        free(this->id_uuid);
    if (this->id_label)
        free(this->id_label);

    if (this->drive_vendor)
        free(this->drive_vendor);
    if (this->drive_model)
        free(this->drive_model);
    if (this->drive_revision)
        free(this->drive_revision);
    if (this->drive_serial)
        free(this->drive_serial);
    if (this->drive_wwn)
        free(this->drive_wwn);
    if (this->drive_connection_interface)
        free(this->drive_connection_interface);
    if (this->drive_media_compatibility)
        free(this->drive_media_compatibility);
    if (this->drive_media)
        free(this->drive_media);

    if (this->filesystem)
        free(this->filesystem);

    if (this->partition_scheme)
        free(this->partition_scheme);
    if (this->partition_number)
        free(this->partition_number);
    if (this->partition_type)
        free(this->partition_type);
    if (this->partition_label)
        free(this->partition_label);
    if (this->partition_uuid)
        free(this->partition_uuid);
    if (this->partition_flags)
        free(this->partition_flags);
    if (this->partition_offset)
        free(this->partition_offset);
    if (this->partition_size)
        free(this->partition_size);
    if (this->partition_alignment_offset)
        free(this->partition_alignment_offset);

    if (this->partition_table_scheme)
        free(this->partition_table_scheme);
    if (this->partition_table_count)
        free(this->partition_table_count);

    if (this->optical_disc_num_tracks)
        free(this->optical_disc_num_tracks);
    if (this->optical_disc_num_audio_tracks)
        free(this->optical_disc_num_audio_tracks);
    if (this->optical_disc_num_sessions)
        free(this->optical_disc_num_sessions);
}

using device_t = std::shared_ptr<Device>;

static void
free_devmounts()
{
    if (devmounts.empty())
        return;

    devmounts.clear();
}

/**
 * unescapes things like \x20 to " " and ensures the returned string is valid UTF-8.
 *
 * see volume_id_encode_string() in extras/volume_id/lib/volume_id.c in the
 * udev tree for the encoder
 */
static const std::string
decode_udev_encoded_string(const char* str)
{
    if (!str)
        return "";

    const std::string decode = ztd::strip(ztd::replace(str, "\\x20", " "));
    // LOG_DEBUG("udev encoded string: {}", str);
    // LOG_DEBUG("udev decoded string: {}", decode);
    return decode;
}

static i32
ptr_str_array_compare(const char** a, const char** b)
{
    return ztd::compare(*a, *b);
}

static f64
sysfs_get_double(std::string_view dir, std::string_view attribute)
{
    const std::string filename = Glib::build_filename(dir.data(), attribute.data());

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (Glib::FileError)
    {
        return 0.0;
    }
    return std::stod(contents);
}

static const std::string
sysfs_get_string(std::string_view dir, std::string_view attribute)
{
    const std::string filename = Glib::build_filename(dir.data(), attribute.data());

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (Glib::FileError)
    {
        return std::string("");
    }
    return ztd::strip(contents);
}

static i32
sysfs_get_int(std::string_view dir, std::string_view attribute)
{
    const std::string filename = Glib::build_filename(dir.data(), attribute.data());

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (Glib::FileError)
    {
        return 0;
    }
    return std::stoi(contents);
}

static u64
sysfs_get_uint64(std::string_view dir, std::string_view attribute)
{
    const std::string filename = Glib::build_filename(dir.data(), attribute.data());

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (Glib::FileError)
    {
        return 0;
    }
    return std::stoll(contents);
}

static bool
sysfs_file_exists(std::string_view dir, std::string_view attribute)
{
    const std::string filename = Glib::build_filename(dir.data(), attribute.data());
    if (std::filesystem::exists(filename))
        return true;
    return false;
}

static const std::string
sysfs_resolve_link(std::string_view sysfs_path, std::string_view name)
{
    const std::string full_path = Glib::build_filename(sysfs_path.data(), name.data());

    std::string target_path;
    try
    {
        target_path = std::filesystem::read_symlink(full_path);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        // LOG_WARN("{}", e.what());
        return "";
    }
    return target_path;
}

static bool
info_is_system_internal(device_t device)
{
    const char* value;

    if ((value = udev_device_get_property_value(device->udevice, "UDISKS_SYSTEM_INTERNAL")))
        return std::stol(value) != 0;

    /* A Linux MD device is system internal if, and only if
     *
     * - a single component is system internal
     * - there are no components
     * SKIP THIS TEST
     */

    /* a partition is system internal only if the drive it belongs to is system internal */
    // TODO

    /* a LUKS cleartext device is system internal only if the underlying crypto-text
     * device is system internal
     * SKIP THIS TEST
     */

    // devices with removable media are never system internal
    if (device->device_is_removable)
        return false;

    /* devices on certain buses are never system internal */
    if (device->drive_connection_interface != nullptr)
    {
        if (ztd::same(device->drive_connection_interface, "ata_serial_esata") ||
            ztd::same(device->drive_connection_interface, "sdio") ||
            ztd::same(device->drive_connection_interface, "usb") ||
            ztd::same(device->drive_connection_interface, "firewire"))
            return false;
    }
    return true;
}

static void
info_drive_connection(device_t device)
{
    std::string connection_interface;
    u64 connection_speed = 0;

    /* walk up the device tree to figure out the subsystem */
    if (!device->native_path)
        return;
    std::string s = device->native_path;

    while (true)
    {
        if (!device->device_is_removable && sysfs_get_int(s, "removable") != 0)
            device->device_is_removable = true;

        const std::string p = sysfs_resolve_link(s, "subsystem");
        if (!p.empty())
        {
            const std::string subsystem = Glib::path_get_basename(p);

            if (ztd::same(subsystem, "scsi"))
            {
                connection_interface = "scsi";
                connection_speed = 0;

                /* continue walking up the chain; we just use scsi as a fallback */

                /* grab the names from SCSI since the names from udev currently
                 *  - replaces whitespace with _
                 *  - is missing for e.g. Firewire
                 */
                const std::string vendor = sysfs_get_string(s, "vendor");
                if (!vendor.empty())
                {
                    /* Do not overwrite what we set earlier from ID_VENDOR */
                    if (device->drive_vendor == nullptr)
                    {
                        device->drive_vendor = ztd::strdup(vendor);
                    }
                }

                const std::string model = sysfs_get_string(s, "model");
                if (!model.empty())
                {
                    /* Do not overwrite what we set earlier from ID_MODEL */
                    if (device->drive_model == nullptr)
                    {
                        device->drive_model = ztd::strdup(model);
                    }
                }

                /* TODO: need to improve this code; we probably need the kernel to export more
                 *       information before we can properly get the type and speed.
                 */

                if (device->drive_vendor != nullptr && ztd::same(device->drive_vendor, "ATA"))
                {
                    connection_interface = "ata";
                    break;
                }
            }
            else if (ztd::same(subsystem, "usb"))
            {
                f64 usb_speed;

                /* both the interface and the device will be 'usb'. However only
                 * the device will have the 'speed' property.
                 */
                usb_speed = sysfs_get_double(s, "speed");
                if (usb_speed > 0)
                {
                    connection_interface = "usb";
                    connection_speed = usb_speed * (1000 * 1000);
                    break;
                }
            }
            else if (ztd::same(subsystem, "firewire") || ztd::same(subsystem, "ieee1394"))
            {
                /* TODO: krh has promised a speed file in sysfs; theoretically, the speed can
                 *       be anything from 100, 200, 400, 800 and 3200. Till then we just hardcode
                 *       a resonable default of 400 Mbit/s.
                 */

                connection_interface = "firewire";
                connection_speed = 400 * (1000 * 1000);
                break;
            }
            else if (ztd::same(subsystem, "mmc"))
            {
                /* TODO: what about non-SD, e.g. MMC? Is that another bus? */
                connection_interface = "sdio";

                /* Set vendor name. According to this MMC document
                 *
                 * http://www.mmca.org/membership/IAA_Agreement_10_12_06.pdf
                 *
                 *  - manfid: the manufacturer id
                 *  - oemid: the customer of the manufacturer
                 *
                 * Apparently these numbers are kept secret. It would be nice
                 * to map these into names for setting the manufacturer of the drive,
                 * e.g. Panasonic, Sandisk etc.
                 */

                const std::string model = sysfs_get_string(s, "name");
                if (!model.empty())
                {
                    /* Do not overwrite what we set earlier from ID_MODEL */
                    if (device->drive_model == nullptr)
                    {
                        device->drive_model = ztd::strdup(model);
                    }
                }

                const std::string serial = sysfs_get_string(s, "serial");
                if (!serial.empty())
                {
                    /* Do not overwrite what we set earlier from ID_SERIAL */
                    if (device->drive_serial == nullptr)
                    {
                        /* this is formatted as a hexnumber; drop the leading 0x */
                        device->drive_serial = ztd::strdup(ztd::removeprefix(serial, "0x"));
                    }
                }

                /* TODO: use hwrev and fwrev files? */
                const std::string revision = sysfs_get_string(s, "date");
                if (!revision.empty())
                {
                    /* Do not overwrite what we set earlier from ID_REVISION */
                    if (device->drive_revision == nullptr)
                    {
                        device->drive_revision = ztd::strdup(revision);
                    }
                }

                /* TODO: interface speed; the kernel driver knows; would be nice
                 * if it could export it */
            }
            else if (ztd::same(subsystem, "platform"))
            {
                const std::string sysfs_name = ztd::rpartition(s, "/")[0];
                if (ztd::startswith(sysfs_name, "floppy.") && device->drive_vendor == nullptr)
                {
                    device->drive_vendor = ztd::strdup("Floppy Drive");
                    connection_interface = "platform";
                }
            }
        }

        /* advance up the chain */
        if (!ztd::contains(s, "/"))
            break;
        s = ztd::rpartition(s, "/")[0];

        /* but stop at the root */
        if (ztd::same(s, "/sys/devices"))
            break;
    }

    if (!connection_interface.empty())
    {
        device->drive_connection_interface = ztd::strdup(connection_interface);
        device->drive_connection_speed = connection_speed;
    }
}

static const struct
{
    const char* udev_property;
    const char* media_name;
} drive_media_mapping[] = {
    {"ID_DRIVE_FLASH", "flash"},
    {"ID_DRIVE_FLASH_CF", "flash_cf"},
    {"ID_DRIVE_FLASH_MS", "flash_ms"},
    {"ID_DRIVE_FLASH_SM", "flash_sm"},
    {"ID_DRIVE_FLASH_SD", "flash_sd"},
    {"ID_DRIVE_FLASH_SDHC", "flash_sdhc"},
    {"ID_DRIVE_FLASH_MMC", "flash_mmc"},
    {"ID_DRIVE_FLOPPY", "floppy"},
    {"ID_DRIVE_FLOPPY_ZIP", "floppy_zip"},
    {"ID_DRIVE_FLOPPY_JAZ", "floppy_jaz"},
    {"ID_CDROM", "optical_cd"},
    {"ID_CDROM_CD_R", "optical_cd_r"},
    {"ID_CDROM_CD_RW", "optical_cd_rw"},
    {"ID_CDROM_DVD", "optical_dvd"},
    {"ID_CDROM_DVD_R", "optical_dvd_r"},
    {"ID_CDROM_DVD_RW", "optical_dvd_rw"},
    {"ID_CDROM_DVD_RAM", "optical_dvd_ram"},
    {"ID_CDROM_DVD_PLUS_R", "optical_dvd_plus_r"},
    {"ID_CDROM_DVD_PLUS_RW", "optical_dvd_plus_rw"},
    {"ID_CDROM_DVD_PLUS_R_DL", "optical_dvd_plus_r_dl"},
    {"ID_CDROM_DVD_PLUS_RW_DL", "optical_dvd_plus_rw_dl"},
    {"ID_CDROM_BD", "optical_bd"},
    {"ID_CDROM_BD_R", "optical_bd_r"},
    {"ID_CDROM_BD_RE", "optical_bd_re"},
    {"ID_CDROM_HDDVD", "optical_hddvd"},
    {"ID_CDROM_HDDVD_R", "optical_hddvd_r"},
    {"ID_CDROM_HDDVD_RW", "optical_hddvd_rw"},
    {"ID_CDROM_MO", "optical_mo"},
    {"ID_CDROM_MRW", "optical_mrw"},
    {"ID_CDROM_MRW_W", "optical_mrw_w"},
    {nullptr, nullptr},
};

static const struct
{
    const char* udev_property;
    const char* media_name;
} media_mapping[] = {
    {"ID_DRIVE_MEDIA_FLASH", "flash"},
    {"ID_DRIVE_MEDIA_FLASH_CF", "flash_cf"},
    {"ID_DRIVE_MEDIA_FLASH_MS", "flash_ms"},
    {"ID_DRIVE_MEDIA_FLASH_SM", "flash_sm"},
    {"ID_DRIVE_MEDIA_FLASH_SD", "flash_sd"},
    {"ID_DRIVE_MEDIA_FLASH_SDHC", "flash_sdhc"},
    {"ID_DRIVE_MEDIA_FLASH_MMC", "flash_mmc"},
    {"ID_DRIVE_MEDIA_FLOPPY", "floppy"},
    {"ID_DRIVE_MEDIA_FLOPPY_ZIP", "floppy_zip"},
    {"ID_DRIVE_MEDIA_FLOPPY_JAZ", "floppy_jaz"},
    {"ID_CDROM_MEDIA_CD", "optical_cd"},
    {"ID_CDROM_MEDIA_CD_R", "optical_cd_r"},
    {"ID_CDROM_MEDIA_CD_RW", "optical_cd_rw"},
    {"ID_CDROM_MEDIA_DVD", "optical_dvd"},
    {"ID_CDROM_MEDIA_DVD_R", "optical_dvd_r"},
    {"ID_CDROM_MEDIA_DVD_RW", "optical_dvd_rw"},
    {"ID_CDROM_MEDIA_DVD_RAM", "optical_dvd_ram"},
    {"ID_CDROM_MEDIA_DVD_PLUS_R", "optical_dvd_plus_r"},
    {"ID_CDROM_MEDIA_DVD_PLUS_RW", "optical_dvd_plus_rw"},
    {"ID_CDROM_MEDIA_DVD_PLUS_R_DL", "optical_dvd_plus_r_dl"},
    {"ID_CDROM_MEDIA_DVD_PLUS_RW_DL", "optical_dvd_plus_rw_dl"},
    {"ID_CDROM_MEDIA_BD", "optical_bd"},
    {"ID_CDROM_MEDIA_BD_R", "optical_bd_r"},
    {"ID_CDROM_MEDIA_BD_RE", "optical_bd_re"},
    {"ID_CDROM_MEDIA_HDDVD", "optical_hddvd"},
    {"ID_CDROM_MEDIA_HDDVD_R", "optical_hddvd_r"},
    {"ID_CDROM_MEDIA_HDDVD_RW", "optical_hddvd_rw"},
    {"ID_CDROM_MEDIA_MO", "optical_mo"},
    {"ID_CDROM_MEDIA_MRW", "optical_mrw"},
    {"ID_CDROM_MEDIA_MRW_W", "optical_mrw_w"},
    {nullptr, nullptr},
};

static void
info_drive_properties(device_t device)
{
    GPtrArray* media_compat_array;
    const char* media_in_drive;
    bool drive_is_ejectable;
    bool drive_can_detach;
    const char* value;

    // drive identification
    device->device_is_drive = sysfs_file_exists(device->native_path, "range");

    // vendor
    if ((value = udev_device_get_property_value(device->udevice, "ID_VENDOR_ENC")))
    {
        device->drive_vendor = ztd::strdup(decode_udev_encoded_string(value));
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_VENDOR")))
    {
        device->drive_vendor = ztd::strdup(value);
    }

    // model
    if ((value = udev_device_get_property_value(device->udevice, "ID_MODEL_ENC")))
    {
        device->drive_model = ztd::strdup(decode_udev_encoded_string(value));
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_MODEL")))
    {
        device->drive_model = ztd::strdup(value);
    }

    // revision
    device->drive_revision =
        ztd::strdup(udev_device_get_property_value(device->udevice, "ID_REVISION"));

    // serial
    if ((value = udev_device_get_property_value(device->udevice, "ID_SCSI_SERIAL")))
    {
        /* scsi_id sometimes use the WWN as the serial - annoying - see
         * http://git.kernel.org/?p=linux/hotplug/udev.git;a=commit;h=4e9fdfccbdd16f0cfdb5c8fa8484a8ba0f2e69d3
         * for details
         */
        device->drive_serial = ztd::strdup(value);
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_SERIAL_SHORT")))
    {
        device->drive_serial = ztd::strdup(value);
    }

    // wwn
    if ((value = udev_device_get_property_value(device->udevice, "ID_WWN_WITH_EXTENSION")))
    {
        device->drive_wwn = ztd::strdup(value + 2);
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_WWN")))
    {
        device->drive_wwn = ztd::strdup(value + 2);
    }

    // filesystem
    if ((value = udev_device_get_property_value(device->udevice, "ID_FS_TYPE")))
    {
        device->filesystem = ztd::strdup(value);
    }

    /* pick up some things (vendor, model, connection_interface, connection_speed)
     * not (yet) exported by udev helpers
     */
    // update_drive_properties_from_sysfs (device);
    info_drive_connection(device);

    // is_ejectable
    if ((value = udev_device_get_property_value(device->udevice, "ID_DRIVE_EJECTABLE")))
    {
        drive_is_ejectable = std::stol(value) != 0;
    }
    else
    {
        // clang-format off
        drive_is_ejectable = false;
        drive_is_ejectable |= (udev_device_get_property_value(device->udevice, "ID_CDROM") != nullptr);
        drive_is_ejectable |= (udev_device_get_property_value(device->udevice, "ID_DRIVE_FLOPPY_ZIP") != nullptr);
        drive_is_ejectable |= (udev_device_get_property_value(device->udevice, "ID_DRIVE_FLOPPY_JAZ") != nullptr);
        // clang-format on
    }
    device->drive_is_media_ejectable = drive_is_ejectable;

    // drive_media_compatibility
    media_compat_array = g_ptr_array_new();
    for (usize n = 0; drive_media_mapping[n].udev_property != nullptr; ++n)
    {
        if (udev_device_get_property_value(device->udevice, drive_media_mapping[n].udev_property) ==
            nullptr)
            continue;

        g_ptr_array_add(media_compat_array, (void*)drive_media_mapping[n].media_name);
    }
    /* special handling for SDIO since we do not yet have a sdio_id helper in udev to set properties
     */
    if (device->drive_connection_interface && ztd::same(device->drive_connection_interface, "sdio"))
    {
        const std::string type = sysfs_get_string(device->native_path, "../../type");

        if (ztd::same(type, "MMC"))
        {
            g_ptr_array_add(media_compat_array, (void*)"flash_mmc");
        }
        else if (ztd::same(type, "SD"))
        {
            g_ptr_array_add(media_compat_array, (void*)"flash_sd");
        }
        else if (ztd::same(type, "SDHC"))
        {
            g_ptr_array_add(media_compat_array, (void*)"flash_sdhc");
        }
    }
    g_ptr_array_sort(media_compat_array, (GCompareFunc)ptr_str_array_compare);
    g_ptr_array_add(media_compat_array, nullptr);
    device->drive_media_compatibility = g_strjoinv(" ", (char**)media_compat_array->pdata);

    // drive_media
    media_in_drive = nullptr;
    if (device->device_is_media_available)
    {
        for (usize n = 0; media_mapping[n].udev_property != nullptr; ++n)
        {
            if (udev_device_get_property_value(device->udevice, media_mapping[n].udev_property) ==
                nullptr)
                continue;
            // should this be media_mapping[n] ?  does not matter, same?
            media_in_drive = drive_media_mapping[n].media_name;
            break;
        }
        /* If the media is not set (from e.g. udev rules), just pick the first one in media_compat -
         * note that this may be nullptr (if we do not know what media is compatible with the drive)
         * which is OK.
         */
        if (media_in_drive == nullptr)
            media_in_drive = ((const char**)media_compat_array->pdata)[0];
    }
    device->drive_media = ztd::strdup(media_in_drive);
    g_ptr_array_free(media_compat_array, true);

    // drive_can_detach
    // right now, we only offer to detach USB devices
    drive_can_detach = false;
    if (device->drive_connection_interface && ztd::same(device->drive_connection_interface, "usb"))
    {
        drive_can_detach = true;
    }
    if ((value = udev_device_get_property_value(device->udevice, "ID_DRIVE_DETACHABLE")))
    {
        drive_can_detach = std::stol(value) != 0;
    }
    device->drive_can_detach = drive_can_detach;
}

static void
info_device_properties(device_t device)
{
    const char* value;

    device->native_path = ztd::strdup(udev_device_get_syspath(device->udevice));
    device->devnode = ztd::strdup(udev_device_get_devnode(device->udevice));
    device->devnum = udev_device_get_devnum(device->udevice);
    // device->major = ztd::strdup( udev_device_get_property_value( device->udevice, "MAJOR") );
    // device->minor = ztd::strdup( udev_device_get_property_value( device->udevice, "MINOR") );
    if (!device->native_path || !device->devnode || device->devnum == 0)
    {
        if (device->native_path)
            free(device->native_path);
        device->native_path = nullptr;
        device->devnum = 0;
        return;
    }

    // by id - would need to read symlinks in /dev/disk/by-id

    // is_removable may also be set in info_drive_connection walking up sys tree
    // clang-format off
    device->device_is_removable = sysfs_get_int(device->native_path, "removable");

    device->device_presentation_hide = ztd::strdup(udev_device_get_property_value(device->udevice, "UDISKS_PRESENTATION_HIDE"));
    device->device_presentation_nopolicy = ztd::strdup(udev_device_get_property_value(device->udevice, "UDISKS_PRESENTATION_NOPOLICY"));
    device->device_presentation_name = ztd::strdup(udev_device_get_property_value(device->udevice, "UDISKS_PRESENTATION_NAME"));
    device->device_presentation_icon_name = ztd::strdup(udev_device_get_property_value(device->udevice, "UDISKS_PRESENTATION_ICON_NAME"));
    device->device_automount_hint = ztd::strdup(udev_device_get_property_value(device->udevice, "UDISKS_AUTOMOUNT_HINT"));
    // clang-format on

    // filesystem properties
    i32 partition_type = 0;

    const std::string partition_scheme =
        ztd::null_check(udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_SCHEME"));
    if ((value = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_TYPE")))
        partition_type = std::stol(value);
    if (ztd::same(partition_scheme, "mbr") &&
        (partition_type == 0x05 || partition_type == 0x0f || partition_type == 0x85))
    {
    }
    else
    {
        // clang-format off
        device->id_usage = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_FS_USAGE"));
        device->id_type = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_FS_TYPE"));
        device->id_version = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_FS_VERSION"));
        device->id_uuid = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_FS_UUID"));
        // clang-format on

        if ((value = udev_device_get_property_value(device->udevice, "ID_FS_LABEL_ENC")))
        {
            device->id_label = ztd::strdup(decode_udev_encoded_string(value));
        }
        else if ((value = udev_device_get_property_value(device->udevice, "ID_FS_LABEL")))
        {
            device->id_label = ztd::strdup(value);
        }
    }

    // device_is_media_available
    bool media_available = false;

    if ((device->id_usage && device->id_usage[0] != '\0') ||
        (device->id_type && device->id_type[0] != '\0') ||
        (device->id_uuid && device->id_uuid[0] != '\0') ||
        (device->id_label && device->id_label[0] != '\0'))
    {
        media_available = true;
    }
    else if (ztd::startswith(device->devnode, "/dev/loop"))
        media_available = false;
    else if (device->device_is_removable)
    {
        bool is_cd, is_floppy;
        if ((value = udev_device_get_property_value(device->udevice, "ID_CDROM")))
            is_cd = std::stol(value) != 0;
        else
            is_cd = false;

        if ((value = udev_device_get_property_value(device->udevice, "ID_DRIVE_FLOPPY")))
            is_floppy = std::stol(value) != 0;
        else
            is_floppy = false;

        if (!is_cd && !is_floppy)
        {
            // this test is limited for non-root - user may not have read
            // access to device file even if media is present
            const i32 fd = open(device->devnode, O_RDONLY);
            if (fd >= 0)
            {
                media_available = true;
                close(fd);
            }
        }
        else if ((value = udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA")))
            media_available = (std::stol(value) == 1);
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA")))
        media_available = (std::stol(value) == 1);
    else
        media_available = true;
    device->device_is_media_available = media_available;

    /* device_size, device_block_size and device_is_read_only properties */
    if (device->device_is_media_available)
    {
        device->device_size = sysfs_get_uint64(device->native_path, "size") * ztd::BLOCK_SIZE;
        device->device_is_read_only = (sysfs_get_int(device->native_path, "ro") != 0);
        /* This is not available on all devices so fall back to 512 if unavailable.
         *
         * Another way to get this information is the BLKSSZGET ioctl but we do not want
         * to open the device. Ideally vol_id would export it.
         */
        u64 block_size = sysfs_get_uint64(device->native_path, "queue/hw_sector_size");
        if (block_size == 0)
            block_size = ztd::BLOCK_SIZE;
        device->device_block_size = block_size;
    }
    else
    {
        device->device_size = device->device_block_size = 0;
        device->device_is_read_only = false;
    }

    // links
    struct udev_list_entry* entry = udev_device_get_devlinks_list_entry(device->udevice);
    while (entry)
    {
        const char* entry_name = udev_list_entry_get_name(entry);
        if (entry_name && (ztd::startswith(entry_name, "/dev/disk/by-id/") ||
                           ztd::startswith(entry_name, "/dev/disk/by-uuid/")))
        {
            device->device_by_id = ztd::strdup(entry_name);
            break;
        }
        entry = udev_list_entry_get_next(entry);
    }
}

static char*
info_mount_points(device_t device)
{
    std::vector<std::string> mounts;

    dev_t dmajor = MAJOR(device->devnum);
    dev_t dminor = MINOR(device->devnum);

    // if we have the mount point list, use this instead of reading mountinfo
    if (!devmounts.empty())
    {
        for (devmount_t devmount : devmounts)
        {
            if (devmount->major == dmajor && devmount->minor == dminor)
                return ztd::strdup(devmount->mount_points);
        }
        return nullptr;
    }

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(MOUNTINFO.data());
    }
    catch (const Glib::FileError& e)
    {
        LOG_WARN("Error reading {}: {}", MOUNTINFO, e.what());
        return nullptr;
    }

    /* See Documentation/filesystems/proc.txt for the format of /proc/self/mountinfo
     *
     * Note that things like space are encoded as \020.
     */

    const std::vector<std::string> lines = ztd::split(contents, "\n");
    for (std::string_view line : lines)
    {
        u32 mount_id;
        u32 parent_id;
        dev_t major, minor;
        char encoded_root[PATH_MAX];
        char encoded_mount_point[PATH_MAX];

        if (line.size() == 0)
            continue;

        if (sscanf(line.data(),
                   "%u %u %lu:%lu %s %s",
                   &mount_id,
                   &parent_id,
                   &major,
                   &minor,
                   encoded_root,
                   encoded_mount_point) != 6)
        {
            LOG_WARN("Error reading {}: Error parsing line '{}'", MOUNTINFO, line);
            continue;
        }

        // ignore mounts where only a subtree of a filesystem is mounted
        // this function is only used for block devices.
        if (ztd::same(encoded_root, "/"))
            continue;

        if (major != dmajor || minor != dminor)
            continue;

        const std::string mount_point = g_strcompress(encoded_mount_point);
        if (!ztd::contains(mounts, mount_point))
            mounts.emplace_back(mount_point);
    }

    if (mounts.empty())
        return nullptr;

    // Sort the list to ensure that shortest mount paths appear first
    std::ranges::sort(mounts, ztd::compare);

    const std::string points = ztd::join(mounts, ",");

    return ztd::strdup(points);
}

static void
info_partition_table(device_t device)
{
    bool is_partition_table = false;
    const char* value;

    /* Check if udisks-part-id identified the device as a partition table.. this includes
     * identifying partition tables set up by kpartx for multipath etc.
     */
    if ((value = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_TABLE")) &&
        std::stol(value) == 1)
    {
        // clang-format off
        device->partition_table_scheme = ztd::strdup(udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_TABLE_SCHEME"));
        device->partition_table_count = ztd::strdup(udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_TABLE_COUNT"));
        // clang-format on
        is_partition_table = true;
    }

    /* Note that udisks-part-id might not detect all partition table
     * formats.. so in the negative case, also double check with
     * information in sysfs.
     *
     * The kernel guarantees that all childs are created before the
     * uevent for the parent is created. So if we have childs, we must
     * be a partition table.
     *
     * To detect a child we check for the existance of a subdir that has
     * the parents name as a prefix (e.g. for parent sda then sda1,
     * sda2, sda3 ditto md0, md0p1 etc. etc. will work).
     */

    if (!is_partition_table && std::filesystem::is_directory(device->native_path))
    {
        const std::string s = Glib::path_get_basename(device->native_path);

        u32 partition_count;

        partition_count = 0;

        for (const auto& file : std::filesystem::directory_iterator(device->native_path))
        {
            const std::string file_name = std::filesystem::path(file).filename();

            if (ztd::startswith(file_name, s))
                partition_count++;
        }

        if (partition_count > 0)
        {
            device->partition_table_scheme = ztd::strdup("");
            device->partition_table_count = ztd::strdup(partition_count);
            is_partition_table = true;
        }
    }

    device->device_is_partition_table = is_partition_table;
    if (!is_partition_table)
    {
        if (device->partition_table_scheme)
            free(device->partition_table_scheme);
        device->partition_table_scheme = nullptr;
        if (device->partition_table_count)
            free(device->partition_table_count);
        device->partition_table_count = nullptr;
    }
}

static void
info_partition(device_t device)
{
    bool is_partition = false;

    /* Check if udisks-part-id identified the device as a partition.. this includes
     * identifying partitions set up by kpartx for multipath
     */
    if (udev_device_get_property_value(device->udevice, "UDISKS_PARTITION"))
    {
        // clang-format off
        const char* scheme = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_SCHEME");
        const char* size = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_SIZE");
        const char* type = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_TYPE");
        const char* label = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_LABEL");
        const char* uuid = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_UUID");
        const char* flags = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_FLAGS");
        const char* offset = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_OFFSET");
        const char* alignment_offset = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_ALIGNMENT_OFFSET");
        const char* number = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_NUMBER");
        const char* slave_sysfs_path = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_SLAVE");
        // clang-format on

        if (slave_sysfs_path != nullptr && scheme != nullptr && number != nullptr &&
            std::stol(number) > 0)
        {
            device->partition_scheme = ztd::strdup(scheme);
            device->partition_size = ztd::strdup(size);
            device->partition_type = ztd::strdup(type);
            device->partition_label = ztd::strdup(label);
            device->partition_uuid = ztd::strdup(uuid);
            device->partition_flags = ztd::strdup(flags);
            device->partition_offset = ztd::strdup(offset);
            device->partition_alignment_offset = ztd::strdup(alignment_offset);
            device->partition_number = ztd::strdup(number);
            is_partition = true;
        }
    }

    /* Also handle the case where we are partitioned by the kernel and do not have
     * any UDISKS_PARTITION_* properties.
     *
     * This works without any udev UDISKS_PARTITION_* properties and is
     * there for maximum compatibility since udisks-part-id only knows a
     * limited set of partition table formats.
     */
    if (!is_partition && sysfs_file_exists(device->native_path, "start"))
    {
        u64 size = sysfs_get_uint64(device->native_path, "size");
        u64 alignment_offset = sysfs_get_uint64(device->native_path, "alignment_offset");

        device->partition_size = ztd::strdup(size * ztd::BLOCK_SIZE);
        device->partition_alignment_offset = ztd::strdup(alignment_offset);

        u64 offset = sysfs_get_uint64(device->native_path, "start") * device->device_block_size;
        device->partition_offset = ztd::strdup(offset);

        char* s = device->native_path;
        usize n;
        for (n = std::strlen(s) - 1; n >= 0 && g_ascii_isdigit(s[n]); --n)
            ;
        device->partition_number = ztd::strdup(std::stol(s + n + 1, nullptr, 0));
        is_partition = true;
    }

    device->device_is_partition = is_partition;

    if (!is_partition)
    {
        device->partition_scheme = nullptr;
        device->partition_size = nullptr;
        device->partition_type = nullptr;
        device->partition_label = nullptr;
        device->partition_uuid = nullptr;
        device->partition_flags = nullptr;
        device->partition_offset = nullptr;
        device->partition_alignment_offset = nullptr;
        device->partition_number = nullptr;
    }
    else
    {
        device->device_is_drive = false;
    }
}

static void
info_optical_disc(device_t device)
{
    const char* optical_state = udev_device_get_property_value(device->udevice, "ID_CDROM");

    if (optical_state && std::stol(optical_state) != 0)
    {
        device->device_is_optical_disc = true;

        // clang-format off
        device->optical_disc_num_tracks = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_TRACK_COUNT"));
        device->optical_disc_num_audio_tracks = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_TRACK_COUNT_AUDIO"));
        device->optical_disc_num_sessions = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_SESSION_COUNT"));

        const std::string cdrom_disc_state = ztd::null_check(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_STATE"));

        device->optical_disc_is_blank = (ztd::same(cdrom_disc_state, "blank"));
        device->optical_disc_is_appendable = (ztd::same(cdrom_disc_state, "appendable"));
        device->optical_disc_is_closed = (ztd::same(cdrom_disc_state, "complete"));
        // clang-format on
    }
    else
    {
        device->device_is_optical_disc = false;
    }
}

static bool
device_get_info(device_t device)
{
    info_device_properties(device);
    if (!device->native_path || device->devnum == 0)
        return false;
    info_drive_properties(device);
    device->device_is_system_internal = info_is_system_internal(device);
    device->mount_points = info_mount_points(device);
    device->device_is_mounted = (device->mount_points != nullptr);
    info_partition_table(device);
    info_partition(device);
    info_optical_disc(device);
    return true;
}

/* ************************************************************************
 * udev & mount monitors
 * ************************************************************************ */

static void
parse_mounts(bool report)
{
    // LOG_INFO("@@@@@@@@@@@@@ parse_mounts {}", report ? "true" : "false");
    struct udev_device* udevice;
    dev_t devnum;

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(MOUNTINFO.data());
    }
    catch (const Glib::FileError& e)
    {
        LOG_WARN("Error reading {}: {}", MOUNTINFO, e.what());
        return;
    }

    // get all mount points for all devices
    std::vector<devmount_t> newmounts;
    std::vector<devmount_t> changed;

    bool subdir_mount;

    /* See Documentation/filesystems/proc.txt for the format of /proc/self/mountinfo
     *
     * Note that things like space are encoded as \020.
     *
     * This file contains lines of the form:
     * 36 35 98:0 /mnt1 /mnt2 rw,noatime master:1 - ext3 /dev/root rw,errors=continue
     * (1)(2)(3)   (4)   (5)      (6)      (7)   (8) (9)   (10)         (11)
     * (1) mount ID:  unique identifier of the mount (may be reused after umount)
     * (2) parent ID:  ID of parent (or of self for the top of the mount tree)
     * (3) major:minor:  value of st_dev for files on filesystem
     * (4) root:  root of the mount within the filesystem
     * (5) mount point:  mount point relative to the process's root
     * (6) mount options:  per mount options
     * (7) optional fields:  zero or more fields of the form "tag[:value]"
     * (8) separator:  marks the end of the optional fields
     * (9) filesystem type:  name of filesystem of the form "type[.subtype]"
     * (10) mount source:  filesystem specific information or "none"
     * (11) super options:  per super block options
     * Parsers should ignore all unrecognised optional fields.
     */

    const std::vector<std::string> lines = ztd::split(contents, "\n");
    for (std::string_view line : lines)
    {
        u32 mount_id;
        u32 parent_id;
        dev_t major, minor;
        char encoded_root[PATH_MAX];
        char encoded_mount_point[PATH_MAX];

        if (line.size() == 0)
            continue;

        if (sscanf(line.data(),
                   "%u %u %lu:%lu %s %s",
                   &mount_id,
                   &parent_id,
                   &major,
                   &minor,
                   encoded_root,
                   encoded_mount_point) != 6)
        {
            LOG_WARN("Error reading {}: Error parsing line '{}'", MOUNTINFO, line);
            continue;
        }

        /* mount where only a subtree of a filesystem is mounted? */
        subdir_mount = !ztd::same(encoded_root, "/");

        if (subdir_mount)
        {
            // get mount source
            // LOG_INFO("subdir_mount {}:{} {} root={}", major, minor, encoded_mount_point,
            // encoded_root);
            char typebuf[PATH_MAX];
            char mount_source[PATH_MAX];
            const char* sep;
            sep = strstr(line.data(), " - ");
            if (sep && sscanf(sep + 3, "%s %s", typebuf, mount_source) == 2)
            {
                // LOG_INFO("    source={}", mount_source);
                if (ztd::startswith(mount_source, "/dev/"))
                {
                    /* is a subdir mount on a local device, eg a bind mount
                     * so do not include this mount point */
                    // LOG_INFO("        local");
                    continue;
                }
            }
            else
                continue;
        }

        const std::string mount_point = g_strcompress(encoded_mount_point);
        if (mount_point.empty())
            continue;

        // fstype
        std::string fstype;
        if (ztd::contains(line.data(), " - "))
            fstype = ztd::rpartition(line, " - ")[2];

        // LOG_INFO("mount_point({}:{})={}", major, minor, mount_point);
        devmount_t devmount = nullptr;
        for (devmount_t search : newmounts)
        {
            if (search->major == major && search->minor == minor)
            {
                devmount = search;
                break;
            }
        }
        if (!devmount)
        {
            // LOG_INFO("     new devmount {}", mount_point);
            if (report)
            {
                if (subdir_mount)
                {
                    // is a subdir mount - ignore if block device
                    devnum = makedev(major, minor);
                    udevice = udev_device_new_from_devnum(udev, 'b', devnum);
                    if (udevice)
                    {
                        // is block device with subdir mount - ignore
                        udev_device_unref(udevice);
                        continue;
                    }
                }
                devmount = std::make_shared<Devmount>(major, minor);
                devmount->fstype = ztd::strdup(fstype);

                newmounts.emplace_back(devmount);
            }
            else
            {
                // initial load !report do not add non-block devices
                devnum = makedev(major, minor);
                udevice = udev_device_new_from_devnum(udev, 'b', devnum);
                if (udevice)
                {
                    // is block device
                    udev_device_unref(udevice);
                    if (subdir_mount)
                    {
                        // is block device with subdir mount - ignore
                        continue;
                    }
                    // add
                    devmount = std::make_shared<Devmount>(major, minor);
                    devmount->fstype = ztd::strdup(fstype);

                    newmounts.emplace_back(devmount);
                }
            }
        }

        if (devmount && !ztd::contains(devmount->mounts, mount_point))
        {
            // LOG_INFO("    prepended");
            devmount->mounts.emplace_back(mount_point);
        }
    }
    // LOG_INFO("LINES DONE");
    // translate each mount points list to string
    for (devmount_t devmount : newmounts)
    {
        // Sort the list to ensure that shortest mount paths appear first
        std::ranges::sort(devmount->mounts, ztd::compare);

        const std::string points = ztd::join(devmount->mounts, ",");

        devmount->mounts.clear();
        devmount->mount_points = ztd::strdup(points);
        // LOG_INFO("translate {}:{} {}", devmount->major, devmount->minor, points);
    }

    // compare old and new lists
    if (report)
    {
        for (devmount_t devmount : newmounts)
        {
            // LOG_INFO("finding {}:{}", devmount->major, devmount->minor);

            devmount_t found = nullptr;

            for (devmount_t search : devmounts)
            {
                if (devmount->major == search->major && devmount->minor == search->minor)
                    found = search;
            }

            if (found)
            {
                // LOG_INFO("    found");
                if (ztd::same(found->mount_points, devmount->mount_points))
                {
                    // LOG_INFO("    freed");
                    // no change to mount points, so remove from old list
                    devmount = found;
                    ztd::remove(devmounts, devmount);
                }
            }
            else
            {
                // new mount
                // LOG_INFO("    new mount {}:{} {}", devmount->major, devmount->minor,
                // devmount->mount_points);
                devmount_t devcopy = std::make_shared<Devmount>(devmount->major, devmount->minor);
                devcopy->mount_points = ztd::strdup(devmount->mount_points);
                devcopy->fstype = ztd::strdup(devmount->fstype);

                changed.emplace_back(devcopy);
            }
        }
    }
    // LOG_INFO("REMAINING");
    // any remaining devices in old list have changed mount status
    for (devmount_t devmount : devmounts)
    {
        // LOG_INFO("remain {}:{}", devmount->major, devmount->minor );
        if (report)
            changed.emplace_back(devmount);
    }

    free_devmounts();

    devmounts = newmounts;

    // report
    if (report && !changed.empty())
    {
        for (devmount_t devmount : changed)
        {
            char* devnode = nullptr;
            devnum = makedev(devmount->major, devmount->minor);
            udevice = udev_device_new_from_devnum(udev, 'b', devnum);
            if (udevice)
                devnode = ztd::strdup(udev_device_get_devnode(udevice));
            if (devnode)
            {
                // block device
                LOG_INFO("mount changed: {}", devnode);

                vfs::volume volume = vfs_volume_read_by_device(udevice);
                if (volume)
                    volume->device_added(true); // frees volume if needed
                free(devnode);
            }
            else
            {
                // not a block device
                vfs::volume volume = vfs_volume_read_by_mount(devnum, devmount->mount_points);
                if (volume)
                {
                    LOG_INFO("special mount changed: {} ({}:{}) on {}",
                             volume->device_file,
                             MAJOR(volume->devnum),
                             MINOR(volume->devnum),
                             devmount->mount_points);
                    volume->device_added(false); // frees volume if needed
                }
                else
                {
                    vfs_volume_nonblock_removed(devnum);
                }
            }
            udev_device_unref(udevice);
        }
    }
    // printf ( "END PARSE\n");
}

static const char*
get_devmount_fstype(u32 major, u32 minor)
{
    for (devmount_t devmount : devmounts)
    {
        if (devmount->major == major && devmount->minor == minor)
            return devmount->fstype;
    }
    return nullptr;
}

static bool
cb_mount_monitor_watch(Glib::IOCondition condition)
{
    if (condition == Glib::IOCondition::IO_ERR)
        return true;

    // LOG_INFO("@@@ {} changed", MOUNTINFO);
    parse_mounts(true);

    return true;
}

static bool
cb_udev_monitor_watch(Glib::IOCondition condition)
{
    if (condition == Glib::IOCondition::IO_NVAL)
    {
        return false;
    }
    else if (condition != Glib::IOCondition::IO_IN)
    {
        if (condition == Glib::IOCondition::IO_HUP)
            return false;
        return true;
    }

    struct udev_device* udevice;
    vfs::volume volume;
    if ((udevice = udev_monitor_receive_device(umonitor)))
    {
        const char* action = udev_device_get_action(udevice);
        char* devnode = ztd::strdup(udev_device_get_devnode(udevice));
        if (action)
        {
            // print action
            if (ztd::same(action, "add"))
                LOG_INFO("udev added:   {}", devnode);
            else if (ztd::same(action, "remove"))
                LOG_INFO("udev removed: {}", devnode);
            else if (ztd::same(action, "change"))
                LOG_INFO("udev changed: {}", devnode);
            else if (ztd::same(action, "move"))
                LOG_INFO("udev moved:   {}", devnode);

            // add/remove volume
            if (ztd::same(action, "add") || ztd::same(action, "change"))
            {
                if ((volume = vfs_volume_read_by_device(udevice)))
                    volume->device_added(true); // frees volume if needed
            }
            else if (ztd::same(action, "remove"))
            {
                vfs_volume_device_removed(udevice);
            }
            // what to do for move action?
        }
        free(devnode);
        udev_device_unref(udevice);
    }
    return true;
}

void
VFSVolume::set_info() noexcept
{
    char* lastcomma;
    std::string disp_device;
    std::string disp_label;
    std::string disp_size;
    std::string disp_mount;
    std::string disp_fstype;
    std::string disp_devnum;
    std::string disp_id;

    // set device icon
    switch (this->device_type)
    {
        case VFSVolumeDeviceType::BLOCK:
            if (this->is_audiocd)
                this->icon = "dev_icon_audiocd";
            else if (this->is_optical)
            {
                if (this->is_mounted)
                    this->icon = "dev_icon_optical_mounted";
                else if (this->is_mountable)
                    this->icon = "dev_icon_optical_media";
                else
                    this->icon = "dev_icon_optical_nomedia";
            }
            else if (this->is_floppy)
            {
                if (this->is_mounted)
                    this->icon = "dev_icon_floppy_mounted";
                else
                    this->icon = "dev_icon_floppy_unmounted";
                this->is_mountable = true;
            }
            else if (this->is_removable)
            {
                if (this->is_mounted)
                    this->icon = "dev_icon_remove_mounted";
                else
                    this->icon = "dev_icon_remove_unmounted";
            }
            else
            {
                if (this->is_mounted)
                {
                    if (ztd::startswith(this->device_file, "/dev/loop"))
                        this->icon = "dev_icon_file";
                    else
                        this->icon = "dev_icon_internal_mounted";
                }
                else
                    this->icon = "dev_icon_internal_unmounted";
            }
            break;
        case VFSVolumeDeviceType::NETWORK:
            this->icon = "dev_icon_network";
            break;
        case VFSVolumeDeviceType::OTHER:
            this->icon = "dev_icon_file";
            break;
        default:
            break;
    }

    // set disp_id using by-id
    if (this->device_type == VFSVolumeDeviceType::BLOCK)
    {
        if (this->is_floppy && !this->udi)
        {
            disp_id = ":floppy";
        }
        else if (this->udi)
        {
            if ((lastcomma = strrchr(this->udi, '/')))
            {
                lastcomma++;
                if (ztd::startswith(lastcomma, "usb-"))
                    lastcomma += 4;
                else if (ztd::startswith(lastcomma, "ata-"))
                    lastcomma += 4;
                else if (ztd::startswith(lastcomma, "scsi-"))
                    lastcomma += 5;
            }
            else
                lastcomma = this->udi;
            if (lastcomma[0] != '\0')
            {
                disp_id = fmt::format(":{:.16s}", lastcomma);
            }
        }
        else if (this->is_optical)
            disp_id = ":optical";
        // table type
        if (this->is_table)
        {
            if (this->fs_type)
            {
                free(this->fs_type);
                this->fs_type = nullptr;
            }
            if (!this->fs_type)
                this->fs_type = ztd::strdup("table");
        }
    }
    else
    {
        disp_id = ztd::null_check(this->udi);
    }

    std::string size_str;

    // set display name
    if (this->is_mounted)
    {
        if (!this->label.empty())
            disp_label = this->label;
        else
            disp_label = "";

        if (this->size > 0)
        {
            size_str = vfs_file_size_format(this->size, false);
            disp_size = fmt::format("{}", size_str);
        }
        if (this->mount_point && this->mount_point[0] != '\0')
            disp_mount = fmt::format("{}", this->mount_point);
        else
            disp_mount = "???";
    }
    else if (this->is_mountable) // has_media
    {
        if (this->is_blank)
            disp_label = "[blank]";
        else if (!this->label.empty())
            disp_label = this->label;
        else if (this->is_audiocd)
            disp_label = "[audio]";
        else
            disp_label = "";
        if (this->size > 0)
        {
            size_str = vfs_file_size_format(this->size, false);
            disp_size = fmt::format("{}", size_str);
        }
        disp_mount = "---";
    }
    else
    {
        disp_label = "[no media]";
    }
    if (ztd::startswith(this->device_file, "/dev/"))
        disp_device = ztd::strdup(this->device_file + 5);
    else if (ztd::startswith(this->device_file, "curlftpfs#"))
        disp_device = ztd::removeprefix(this->device_file, "curlftpfs#");
    else
        disp_device = this->device_file;
    if (this->fs_type && this->fs_type[0] != '\0')
        disp_fstype = this->fs_type;
    disp_devnum = fmt::format("{}:{}", MAJOR(this->devnum), MINOR(this->devnum));

    std::string value;
    std::string parameter;
    const char* fmt = xset_get_s(XSetName::DEV_DISPNAME);
    if (!fmt)
    {
        parameter = fmt::format("{} {} {} {} {} {}",
                                disp_device,
                                disp_size,
                                disp_fstype,
                                disp_label,
                                disp_mount,
                                disp_id);
    }
    else
    {
        value = fmt;

        value = ztd::replace(value, "%v", disp_device);
        value = ztd::replace(value, "%s", disp_size);
        value = ztd::replace(value, "%t", disp_fstype);
        value = ztd::replace(value, "%l", disp_label);
        value = ztd::replace(value, "%m", disp_mount);
        value = ztd::replace(value, "%n", disp_devnum);
        value = ztd::replace(value, "%i", disp_id);

        parameter = value;
    }

    while (ztd::contains(parameter, "  "))
    {
        parameter = ztd::replace(parameter, "  ", " ");
    }

    // remove leading spaces
    parameter = ztd::lstrip(parameter);

    this->disp_name = ztd::strdup(Glib::filename_display_name(parameter));

    if (!this->udi)
        this->udi = ztd::strdup(this->device_file);
}

static vfs::volume
vfs_volume_read_by_device(struct udev_device* udevice)
{ // uses udev to read device parameters into returned volume
    if (!udevice)
        return nullptr;

    device_t device = std::make_shared<Device>(udevice);
    if (!device_get_info(device) || !device->devnode || device->devnum == 0 ||
        !ztd::startswith(device->devnode, "/dev/"))
    {
        return nullptr;
    }

    // translate device info to VFSVolume
    vfs::volume volume = new VFSVolume;
    volume->devnum = device->devnum;
    volume->device_type = VFSVolumeDeviceType::BLOCK;
    volume->device_file = ztd::strdup(device->devnode);
    volume->udi = ztd::strdup(device->device_by_id);
    volume->should_autounmount = false;
    volume->is_optical = device->device_is_optical_disc;
    volume->is_table = device->device_is_partition_table;
    volume->is_floppy = (device->drive_media_compatibility &&
                         ztd::same(device->drive_media_compatibility, "floppy"));
    volume->is_removable = !device->device_is_system_internal;
    volume->requires_eject = device->drive_is_media_ejectable;
    volume->is_mountable = device->device_is_media_available;
    volume->is_audiocd = (device->device_is_optical_disc && device->optical_disc_num_audio_tracks &&
                          std::stol(device->optical_disc_num_audio_tracks) > 0);
    volume->is_dvd = (device->drive_media && ztd::contains(device->drive_media, "optical_dvd"));
    volume->is_blank = (device->device_is_optical_disc && device->optical_disc_is_blank);
    volume->is_mounted = device->device_is_mounted;
    volume->is_user_visible =
        device->device_presentation_hide ? !std::stol(device->device_presentation_hide) : true;
    volume->ever_mounted = false;
    volume->open_main_window = nullptr;
    volume->nopolicy = device->device_presentation_nopolicy
                           ? std::stol(device->device_presentation_nopolicy)
                           : false;
    volume->mount_point = nullptr;
    if (device->mount_points)
    {
        char* comma;
        if ((comma = strchr(device->mount_points, ',')))
        {
            comma[0] = '\0';
            volume->mount_point = ztd::strdup(device->mount_points);
            comma[0] = ',';
        }
        else
        {
            volume->mount_point = ztd::strdup(device->mount_points);
        }
    }
    volume->size = device->device_size;
    if (device->id_label)
        volume->label = device->id_label;
    volume->fs_type = ztd::strdup(device->id_type);
    volume->disp_name = nullptr;
    volume->automount_time = 0;
    volume->inhibit_auto = false;

    // adjustments
    volume->ever_mounted = volume->is_mounted;
    // if ( volume->is_blank )
    //    volume->is_mountable = false;  //has_media is now used for showing
    if (volume->is_dvd)
        volume->is_audiocd = false;

    volume->set_info();

    // LOG_INFO( "====devnum={}:{}", MAJOR(volume->devnum), MINOR(volume->devnum));
    // LOG_INFO( "    device_file={}", volume->device_file);
    // LOG_INFO( "    udi={}", volume->udi);
    // LOG_INFO( "    label={}", volume->label);
    // LOG_INFO( "    icon={}", volume->icon);
    // LOG_INFO( "    is_mounted={}", volume->is_mounted);
    // LOG_INFO( "    is_mountable={}", volume->is_mountable);
    // LOG_INFO( "    is_optical={}", volume->is_optical);
    // LOG_INFO( "    is_audiocd={}", volume->is_audiocd);
    // LOG_INFO( "    is_blank={}", volume->is_blank);
    // LOG_INFO( "    is_floppy={}", volume->is_floppy);
    // LOG_INFO( "    is_table={}", volume->is_table);
    // LOG_INFO( "    is_removable={}", volume->is_removable);
    // LOG_INFO( "    requires_eject={}", volume->requires_eject);
    // LOG_INFO( "    is_user_visible={}", volume->is_user_visible);
    // LOG_INFO( "    mount_point={}", volume->mount_point);
    // LOG_INFO( "    size={}", volume->size);
    // LOG_INFO( "    disp_name={}", volume->disp_name);

    return volume;
}

bool
path_is_mounted_mtab(const char* mtab_file, const char* path, char** device_file, char** fs_type)
{
    if (!path)
        return false;

    bool ret = false;
    char encoded_file[PATH_MAX];
    char encoded_point[PATH_MAX];
    char encoded_fstype[PATH_MAX];

    const std::string mtab_path = Glib::build_filename(SYSCONFDIR, "mtab");

    std::string contents;

    if (mtab_file)
    {
        // read from a custom mtab file, eg ~/.mtab.fuseiso
        try
        {
            contents = Glib::file_get_contents(mtab_file);
        }
        catch (const Glib::FileError& e)
        {
            LOG_WARN("Error reading {}: {}", mtab_file, e.what());
            return false;
        }
    }
    else
    {
        try
        {
            contents = Glib::file_get_contents(MTAB.data());
        }
        catch (const Glib::FileError& e)
        {
            LOG_WARN("Error reading {}: {}", MTAB, e.what());
            return false;
        }
    }

    const std::vector<std::string> lines = ztd::split(contents, "\n");
    for (std::string_view line : lines)
    {
        if (line.size() == 0)
            continue;

        if (sscanf(line.data(), "%s %s %s ", encoded_file, encoded_point, encoded_fstype) != 3)
        {
            LOG_WARN("Error parsing mtab line '{}'", line);
            continue;
        }

        char* point = g_strcompress(encoded_point);
        if (ztd::same(point, path))
        {
            if (device_file)
                *device_file = g_strcompress(encoded_file);
            if (fs_type)
                *fs_type = g_strcompress(encoded_fstype);
            ret = true;
            break;
        }
        free(point);
    }
    return ret;
}

bool
mtab_fstype_is_handled_by_protocol(const char* mtab_fstype)
{
    bool found = false;
    const char* handlers_list;
    if (mtab_fstype && mtab_fstype[0] && (handlers_list = xset_get_s(XSetName::DEV_NET_CNF)))
    {
        // is the mtab_fstype handled by a protocol handler?
        std::vector<std::string> values;
        values.emplace_back(mtab_fstype);
        values.emplace_back(fmt::format("mtab_fs={}", mtab_fstype));
        const std::vector<std::string> handlers = ztd::split(handlers_list, " ");
        if (!handlers.empty())
        {
            // test handlers
            xset_t set;
            for (std::string_view handler : handlers)
            {
                if (handlers.empty() || !(set = xset_is(handler)) ||
                    set->b != XSetB::XSET_B_TRUE /* disabled */)
                    continue;
                // test whitelist
                std::string msg;
                if (!ztd::same(set->s, "*") && ptk_handler_values_in_list(set->s, values, msg))
                {
                    found = true;
                    break;
                }
            }
        }
    }
    return found;
}

SplitNetworkURL
split_network_url(std::string_view url, netmount_t netmount)
{
    if (url.empty() || !netmount)
        return SplitNetworkURL::NOT_A_NETWORK_URL;

    char* str;
    char* str2;
    char* orig_url = ztd::strdup(url.data());
    char* xurl = orig_url;

    netmount_t nm = std::make_shared<Netmount>();
    nm->url = ztd::strdup(url.data());

    // get protocol
    bool is_colon;
    if (ztd::startswith(xurl, "//"))
    { // //host...
        if (xurl[2] == '\0')
        {
            free(orig_url);
            return SplitNetworkURL::NOT_A_NETWORK_URL;
        }
        nm->fstype = ztd::strdup("smb");
        is_colon = false;
    }
    else if ((str = strstr(xurl, "://")))
    { // protocol://host...
        if (xurl[0] == ':' || xurl[0] == '/')
        {
            free(orig_url);
            return SplitNetworkURL::NOT_A_NETWORK_URL;
        }
        str[0] = '\0';
        nm->fstype = ztd::strdup(ztd::strip(xurl));
        if (nm->fstype[0] == '\0')
        {
            free(orig_url);
            return SplitNetworkURL::NOT_A_NETWORK_URL;
        }
        str[0] = ':';
        is_colon = true;

        // remove ...# from start of protocol
        // eg curlftpfs mtab url looks like: curlftpfs#ftp://hostname/
        if (nm->fstype && ztd::contains(nm->fstype, "#"))
            nm->fstype = ztd::strdup(ztd::rpartition(nm->fstype, "#")[2]);
    }
    else if ((str = strstr(xurl, ":/")))
    { // host:/path
        // note: sshfs also uses this URL format in mtab, but mtab_fstype == fuse.sshfs
        if (xurl[0] == ':' || xurl[0] == '/')

        {
            free(orig_url);
            return SplitNetworkURL::NOT_A_NETWORK_URL;
        }
        nm->fstype = ztd::strdup("nfs");
        str[0] = '\0';
        nm->host = ztd::strdup(xurl);
        str[0] = ':';
        is_colon = true;
    }
    else
    {
        free(orig_url);
        return SplitNetworkURL::NOT_A_NETWORK_URL;
    }

    SplitNetworkURL ret = SplitNetworkURL::VALID_NETWORK_URL;

    // parse
    if (is_colon && (str = strchr(xurl, ':')))
    {
        xurl = str + 1;
    }
    while (ztd::startswith(xurl, "/"))
        xurl++;

    char* trim_url;
    trim_url = ztd::strdup(xurl);

    // user:pass
    if ((str = strchr(xurl, '@')))
    {
        if ((str2 = strchr(str + 1, '@')))
        {
            // there is a second @ - assume username contains email address
            str = str2;
        }
        str[0] = '\0';
        if ((str2 = strchr(xurl, ':')))
        {
            str2[0] = '\0';
            if (str2[1] != '\0')
                nm->pass = ztd::strdup(str2 + 1);
        }
        if (xurl[0] != '\0')
            nm->user = ztd::strdup(xurl);
        xurl = str + 1;
    }

    // path
    if ((str = strchr(xurl, '/')))
    {
        nm->path = ztd::strdup(str);
        str[0] = '\0';
    }

    // host:port
    if (!nm->host)
    {
        if (xurl[0] == '[')
        {
            // ipv6 literal
            if ((str = strchr(xurl, ']')))
            {
                str[0] = '\0';
                if (xurl[1] != '\0')
                    nm->host = ztd::strdup(xurl + 1);
                if (str[1] == ':' && str[2] != '\0')
                    nm->port = ztd::strdup(str + 1);
            }
        }
        else if (xurl[0] != '\0')
        {
            if ((str = strchr(xurl, ':')))
            {
                str[0] = '\0';
                if (str[1] != '\0')
                    nm->port = ztd::strdup(str + 1);
            }
            nm->host = ztd::strdup(xurl);
        }
    }
    free(trim_url);
    free(orig_url);

    // check host
    if (!nm->host)
        nm->host = ztd::strdup("");

#if 0
    // check user pass port
    if ((nm->user && strchr(nm->user, ' ')) || (nm->pass && strchr(nm->pass, ' ')) ||
        (nm->port && strchr(nm->port, ' ')))
        ret = SplitNetworkURL::INVALID_NETWORK_URL;
#endif

    netmount = nm;
    return ret;
}

static vfs::volume
vfs_volume_read_by_mount(dev_t devnum, const char* mount_points)
{ // read a non-block device
    if (devnum == 0 || !mount_points)
        return nullptr;

    vfs::volume volume;

    // check mount path has path sep
    std::string point = mount_points;
    if (!ztd::contains(point, "/"))
        return nullptr;

    // get single mount point
    if (ztd::contains(point, ","))
        point = ztd::partition(point, ",")[0];

    // LOG_INFO("vfs_volume_read_by_mount point: {}", point);

    // get device name
    char* name = nullptr;
    char* mtab_fstype = nullptr;
    if (!(path_is_mounted_mtab(nullptr, point.c_str(), &name, &mtab_fstype) && name &&
          name[0] != '\0'))
        return nullptr;

    netmount_t netmount = std::make_shared<Netmount>();
    if (split_network_url(name, netmount) == SplitNetworkURL::VALID_NETWORK_URL)
    {
        // network URL
        volume = new VFSVolume;
        volume->devnum = devnum;
        volume->device_type = VFSVolumeDeviceType::NETWORK;
        volume->should_autounmount = false;
        volume->udi = ztd::strdup(netmount->url);
        volume->label = ztd::null_check(netmount->host);
        volume->fs_type = mtab_fstype ? mtab_fstype : ztd::strdup("");
        volume->size = 0;
        volume->device_file = name;
        volume->is_mounted = true;
        volume->ever_mounted = true;
        volume->open_main_window = nullptr;
        volume->mount_point = ztd::strdup(point);
        volume->disp_name = nullptr;
        volume->automount_time = 0;
        volume->inhibit_auto = false;
    }
    else if (ztd::same(mtab_fstype, "fuse.fuseiso"))
    {
        // an ISO file mounted with fuseiso
        /* get device_file from ~/.mtab.fuseiso
         * hack - sleep for 0.2 seconds here because sometimes the
         * .mtab.fuseiso file is not updated until after new device is detected. */
        Glib::usleep(200000);
        const std::string mtab_file = Glib::build_filename(vfs_user_home_dir(), ".mtab.fuseiso");
        char* new_name = nullptr;
        if (path_is_mounted_mtab(mtab_file.c_str(), point.c_str(), &new_name, nullptr) &&
            new_name && new_name[0])
        {
            free(name);
            name = new_name;
            new_name = nullptr;
        }
        free(new_name);

        // create a volume
        volume = new VFSVolume;
        volume->devnum = devnum;
        volume->device_type = VFSVolumeDeviceType::OTHER;
        volume->device_file = name;
        volume->udi = ztd::strdup(name);
        volume->should_autounmount = false;
        volume->fs_type = mtab_fstype;
        volume->size = 0;
        volume->is_mounted = true;
        volume->ever_mounted = true;
        volume->open_main_window = nullptr;
        volume->mount_point = ztd::strdup(point);
        volume->disp_name = nullptr;
        volume->automount_time = 0;
        volume->inhibit_auto = false;
    }
    else
    {
        // a non-block device is mounted - do we want to include it?
        // is a protocol handler present?
        bool keep = mtab_fstype_is_handled_by_protocol(mtab_fstype);
        if (!keep && ztd::contains(HIDDEN_NON_BLOCK_FS, std::string_view(mtab_fstype)))
        {
            // no protocol handler and not blacklisted - show anyway?
            keep = ztd::startswith(point, vfs_user_cache_dir()) ||
                   ztd::startswith(point, "/media/") || ztd::startswith(point, "/run/media/") ||
                   ztd::startswith(mtab_fstype, "fuse.");
            if (!keep)
            {
                // in Auto-Mount|Mount Dirs ?
                char* mount_parent = ptk_location_view_get_mount_point_dir(nullptr);
                keep = ztd::startswith(point, mount_parent);
                free(mount_parent);
            }
        }
        // mount point must be readable
        keep = keep && (geteuid() == 0 || faccessat(0, point.c_str(), R_OK, AT_EACCESS) == 0);
        if (keep)
        {
            // create a volume
            volume = new VFSVolume;
            volume->devnum = devnum;
            volume->device_type = VFSVolumeDeviceType::OTHER;
            volume->device_file = name;
            volume->udi = ztd::strdup(name);
            volume->should_autounmount = false;
            volume->fs_type = mtab_fstype;
            volume->size = 0;
            volume->is_mounted = true;
            volume->ever_mounted = true;
            volume->open_main_window = nullptr;
            volume->mount_point = ztd::strdup(point);
            volume->disp_name = nullptr;
            volume->automount_time = 0;
            volume->inhibit_auto = false;
        }
        else
        {
            free(name);
            free(mtab_fstype);
            return nullptr;
        }
    }
    volume->set_info();
    return volume;
}

char*
vfs_volume_handler_cmd(i32 mode, i32 action, vfs::volume vol, const char* options,
                       netmount_t netmount, bool* run_in_terminal, char** mount_point)
{
    const char* handlers_list;
    xset_t set;
    std::vector<std::string> values;

    if (mount_point)
        *mount_point = nullptr;

    switch (mode)
    {
        case PtkHandlerMode::HANDLER_MODE_FS:
            if (!vol)
                return nullptr;

            // fs values
            // change spaces in label to underscores for testing
            // clang-format off
            if (!vol->label.empty())
                values.emplace_back(fmt::format("label={}", vol->label));
            if (vol->udi && vol->udi[0])
                values.emplace_back(fmt::format("id={}", vol->udi));
            values.emplace_back(fmt::format("audiocd={}", vol->is_audiocd ? "1" : "0"));
            values.emplace_back(fmt::format("optical={}", vol->is_optical ? "1" : "0"));
            values.emplace_back(fmt::format("removable={}", vol->is_removable ? "1" : "0"));
            values.emplace_back(fmt::format("mountable={}", vol->is_mountable && !vol->is_blank ? "1" : "0"));
            // change spaces in device to underscores for testing - for ISO files
            values.emplace_back(fmt::format("dev={}", vol->device_file));
            // clang-format on
            if (vol->fs_type)
                values.emplace_back(fmt::format("{}", vol->fs_type));
            break;
        case PtkHandlerMode::HANDLER_MODE_NET:
            // for VFSVolumeDeviceType::NETWORK
            if (netmount)
            {
                // net values
                if (netmount->host && netmount->host[0])
                    values.emplace_back(fmt::format("host={}", netmount->host));
                if (netmount->user && netmount->user[0])
                    values.emplace_back(fmt::format("user={}", netmount->user));
                if (action != PtkHandlerMount::HANDLER_MOUNT && vol && vol->is_mounted)
                {
                    // clang-format off
                    // user-entered url (or mtab url if not available)
                    values.emplace_back(fmt::format("url={}", vol->udi));
                    // mtab fs type (fuse.ssh)
                    values.emplace_back(fmt::format("mtab_fs={}", vol->fs_type));
                    // mtab_url == url if mounted
                    values.emplace_back(fmt::format("mtab_url={}", vol->device_file));
                    // clang-format on
                }
                else if (netmount->url && netmount->url[0])
                    // user-entered url
                    values.emplace_back(fmt::format("url={}", netmount->url));
                // url-derived protocol
                if (netmount->fstype && netmount->fstype[0])
                    values.emplace_back(fmt::format("{}", netmount->fstype));
            }
            else
            {
                // for VFSVolumeDeviceType::OTHER unmount or prop that has protocol
                // handler in mtab_fs
                if (action != PtkHandlerMount::HANDLER_MOUNT && vol && vol->is_mounted)
                {
                    // clang-format off
                    // user-entered url (or mtab url if not available)
                    values.emplace_back(fmt::format("url={}", vol->udi));
                    // mtab fs type (fuse.ssh)
                    values.emplace_back(fmt::format("mtab_fs={}", vol->fs_type));
                    // mtab_url == url if mounted
                    values.emplace_back(fmt::format("mtab_url={}", vol->device_file));
                    // clang-format on
                }
            }
            break;
        default:
            LOG_WARN("vfs_volume_handler_cmd invalid mode {}", mode);
            return nullptr;
    }

    // universal values
    if (vol && vol->is_mounted && vol->mount_point && vol->mount_point[0])
        values.emplace_back(fmt::format("point={}", vol->mount_point));

    // get handlers
    if (!(handlers_list =
              xset_get_s(mode == PtkHandlerMode::HANDLER_MODE_FS ? XSetName::DEV_FS_CNF
                                                                 : XSetName::DEV_NET_CNF)))
        return nullptr;
    const std::vector<std::string> handlers = ztd::split(handlers_list, " ");
    if (handlers.empty())
        return nullptr;

    // test handlers
    bool found = false;
    std::string msg;
    for (std::string_view handler : handlers)
    {
        if (handler.empty() || !(set = xset_is(handler)) ||
            set->b != XSetB::XSET_B_TRUE /* disabled */)
            continue;
        switch (mode)
        {
            case PtkHandlerMode::HANDLER_MODE_FS:
                // test blacklist
                if (ptk_handler_values_in_list(set->x, values, msg))
                    break;
                // test whitelist
                if (ptk_handler_values_in_list(set->s, values, msg))
                {
                    found = true;
                    break;
                }
                break;
            case PtkHandlerMode::HANDLER_MODE_NET:
                // test blacklist
                if (ptk_handler_values_in_list(set->x, values, msg))
                    break;
                // test whitelist
                if (ptk_handler_values_in_list(set->s, values, msg))
                {
                    found = true;
                    break;
                }
                break;
            default:
                break;
        }
    }
    if (!found)
        return nullptr;

    // get command for action
    std::string command;
    std::string error_message;
    const char* action_s;
    bool terminal;
    bool error = ptk_handler_load_script(mode, action, set, nullptr, command, error_message);

    if (error)
    {
        LOG_ERROR(error_message);
        return nullptr;
    }

    // empty command
    if (ptk_handler_command_is_empty(command))
        return nullptr;

    switch (action)
    {
        case PtkHandlerMount::HANDLER_MOUNT:
            terminal = set->in_terminal;
            action_s = "MOUNT";
            break;
        case PtkHandlerMount::HANDLER_UNMOUNT:
            terminal = set->keep_terminal;
            action_s = "UNMOUNT";
            break;
        case PtkHandlerMount::HANDLER_PROP:
            terminal = set->scroll_lock;
            action_s = "PROPERTIES";
            break;
        default:
            return nullptr;
    }

    // show selected handler
    LOG_INFO("{} '{}': {}{} {}",
             mode == PtkHandlerMode::HANDLER_MODE_FS ? "Selected Device Handler"
                                                     : "Selected Protocol Handler",
             set->menu_label,
             action_s,
             !command.empty() ? "" : " (no command)",
             msg);

    // replace sub vars
    std::string fileq;
    switch (mode)
    {
        case PtkHandlerMode::HANDLER_MODE_FS:
            /*
             *      %v  device
             *      %o  volume-specific mount options (use in mount command only)
             *      %t  filesystem type being mounted (eg vfat)
             *      %a  mount point, or create auto mount point
             */
            fileq = bash_quote(vol->device_file); // for iso files
            command = ztd::replace(command, "%v", fileq);
            if (action == PtkHandlerMount::HANDLER_MOUNT)
            {
                command = ztd::replace(command, "%o", options ? options : "");
                command = ztd::replace(command, "%t", vol->fs_type ? vol->fs_type : "");
            }
            if (ztd::contains(command, "%a"))
            {
                if (action == PtkHandlerMount::HANDLER_MOUNT)
                {
                    // create mount point
                    char* point_dir =
                        ptk_location_view_create_mount_point(PtkHandlerMode::HANDLER_MODE_FS,
                                                             vol,
                                                             nullptr,
                                                             nullptr);
                    command = ztd::replace(command, "%a", point_dir);
                    if (mount_point)
                        *mount_point = point_dir;
                    else
                        free(point_dir);
                }
                else
                {
                    command =
                        ztd::replace(command,
                                     "%a",
                                     vol->is_mounted && vol->mount_point && vol->mount_point[0]
                                         ? vol->mount_point
                                         : "");
                }
            }
            // standard sub vars
            command = replace_line_subs(command);
            break;
        case PtkHandlerMode::HANDLER_MODE_NET:
            // also used for VFSVolumeDeviceType::OTHER unmount and prop
            /*
             *       %url%     $fm_url
             *       %proto%   $fm_url_proto
             *       %host%    $fm_url_host
             *       %port%    $fm_url_port
             *       %user%    $fm_url_user
             *       %pass%    $fm_url_pass
             *       %path%    $fm_url_path
             *       %a        mount point, or create auto mount point
             */

            if (netmount)
            {
                // replace sub vars
                if (ztd::contains(command, "%url%"))
                {
                    if (action != PtkHandlerMount::HANDLER_MOUNT && vol && vol->is_mounted)
                        // user-entered url (or mtab url if not available)
                        command = ztd::replace(command, "%url%", vol->udi);
                    else
                        // user-entered url
                        command = ztd::replace(command, "%url%", netmount->url);
                }
                if (ztd::contains(command, "%proto%"))
                    command = ztd::replace(command, "%proto%", netmount->fstype);
                if (ztd::contains(command, "%host%"))
                    command = ztd::replace(command, "%host%", netmount->host);
                if (ztd::contains(command, "%port%"))
                    command = ztd::replace(command, "%port%", netmount->port);
                if (ztd::contains(command, "%user%"))
                    command = ztd::replace(command, "%user%", netmount->user);
                if (ztd::contains(command, "%pass%"))
                    command = ztd::replace(command, "%pass%", netmount->pass);
                if (ztd::contains(command, "%path%"))
                    command = ztd::replace(command, "%path%", netmount->path);
            }
            if (ztd::contains(command, "%a"))
            {
                if (action == PtkHandlerMount::HANDLER_MOUNT && netmount)
                {
                    // create mount point
                    char* point_dir =
                        ptk_location_view_create_mount_point(PtkHandlerMode::HANDLER_MODE_NET,
                                                             nullptr,
                                                             netmount,
                                                             nullptr);
                    command = ztd::replace(command, "%a", point_dir);
                    if (mount_point)
                        *mount_point = point_dir;
                    else
                        free(point_dir);
                }
                else
                {
                    command = ztd::replace(command,
                                           "%a",
                                           vol && vol->is_mounted && vol->mount_point &&
                                                   vol->mount_point[0]
                                               ? vol->mount_point
                                               : "");
                }
            }

            if (netmount)
            {
                // add bash variables
                // urlq is user-entered url or (if mounted) mtab url
                const std::string urlq =
                    bash_quote(action != PtkHandlerMount::HANDLER_MOUNT && vol && vol->is_mounted
                                   ? vol->udi
                                   : netmount->url);
                const std::string protoq =
                    bash_quote(netmount->fstype); // url-derived protocol (ssh)
                const std::string hostq = bash_quote(netmount->host);
                const std::string portq = bash_quote(netmount->port);
                const std::string userq = bash_quote(netmount->user);
                const std::string passq = bash_quote(netmount->pass);
                const std::string pathq = bash_quote(netmount->path);
                // mtab fs type (fuse.ssh)
                std::string mtabfsq;
                std::string mtaburlq;
                // urlq and mtaburlq will both be the same mtab url if mounted
                if (action != PtkHandlerMount::HANDLER_MOUNT && vol && vol->is_mounted)
                {
                    mtabfsq = bash_quote(vol->fs_type);
                    mtaburlq = bash_quote(vol->device_file);
                }
                command = fmt::format(
                    "fm_url_proto={}; fm_url={}; fm_url_host={}; fm_url_port={}; fm_url_user={}; "
                    "fm_url_pass={}; fm_url_path={}; fm_mtab_fs={}; fm_mtab_url={}\n{}",
                    protoq,
                    urlq,
                    hostq,
                    portq,
                    userq,
                    passq,
                    pathq,
                    mtabfsq,
                    mtaburlq,
                    command);
            }
            break;
        default:
            break;
    }

    *run_in_terminal = terminal;
    return ztd::strdup(command);
}

bool
VFSVolume::is_automount() const noexcept
{ // determine if volume should be automounted or auto-unmounted
    if (this->should_autounmount)
        // volume is a network or ISO file that was manually mounted -
        // for autounmounting only
        return true;
    if (!this->is_mountable || this->is_blank || this->device_type != VFSVolumeDeviceType::BLOCK)
        return false;

    const std::string showhidelist =
        fmt::format(" {} ", ztd::null_check(xset_get_s(XSetName::DEV_AUTOMOUNT_VOLUMES)));
    for (i32 i = 0; i < 3; ++i)
    {
        for (i32 j = 0; j < 2; ++j)
        {
            std::string value;
            if (i == 0)
                value = this->device_file;
            else if (i == 1)
                value = this->label;
            else
                value = ztd::rpartition(this->udi, "/")[2];

            std::string test;
            if (j == 0)
                test = fmt::format(" +{} ", value);
            else
                test = fmt::format(" -{} ", value);
            if (ztd::contains(showhidelist, test))
                return (j == 0);
        }
    }

    // udisks no?
    if (this->nopolicy && !xset_get_b(XSetName::DEV_IGNORE_UDISKS_NOPOLICY))
        return false;

    // table?
    if (this->is_table)
        return false;

    // optical
    if (this->is_optical)
        return xset_get_b(XSetName::DEV_AUTOMOUNT_OPTICAL);

    // internal?
    if (this->is_removable && xset_get_b(XSetName::DEV_AUTOMOUNT_REMOVABLE))
        return true;

    return false;
}

const char*
VFSVolume::device_mount_cmd(const char* options, bool* run_in_terminal) noexcept
{
    char* command = nullptr;
    *run_in_terminal = false;

    command = vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_FS,
                                     PtkHandlerMount::HANDLER_MOUNT,
                                     this,
                                     options,
                                     nullptr,
                                     run_in_terminal,
                                     nullptr);
    if (command)
        return command;

    // discovery
    std::string command2;
    std::string path;

    path = Glib::find_program_in_path("udevil");
    if (!path.empty())
    {
        // udevil
        if (options && options[0] != '\0')
            command2 = fmt::format("{} mount {} -o '{}'", path, this->device_file, options);
        else
            command2 = fmt::format("{} mount {}", path, this->device_file);

        return ztd::strdup(command2);
    }

    path = Glib::find_program_in_path("pmount");
    if (!path.empty())
    {
        // pmount
        command2 = fmt::format("{} {}", path, this->device_file);

        return ztd::strdup(command2);
    }

    path = Glib::find_program_in_path("udisksctl");
    if (!path.empty())
    {
        // udisks2
        if (options && options[0] != '\0')
            command2 = fmt::format("{} mount -b {} -o '{}'", path, this->device_file, options);
        else
            command2 = fmt::format("{} mount -b {}", path, this->device_file);

        return ztd::strdup(command2);
    }

    return nullptr;
}

const char*
VFSVolume::device_unmount_cmd(bool* run_in_terminal) noexcept
{
    char* command = nullptr;
    std::string pointq = "";
    *run_in_terminal = false;

    netmount_t netmount = std::make_shared<Netmount>();

    // unmounting a network ?
    switch (this->device_type)
    {
        case VFSVolumeDeviceType::NETWORK:
            // is a network - try to get unmount command
            if (split_network_url(this->udi, netmount) == SplitNetworkURL::VALID_NETWORK_URL)
            {
                command = vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_NET,
                                                 PtkHandlerMount::HANDLER_UNMOUNT,
                                                 this,
                                                 nullptr,
                                                 netmount,
                                                 run_in_terminal,
                                                 nullptr);

                // igtodo is this redundant?
                // replace mount point sub var
                if (command && ztd::contains(command, "%a"))
                {
                    if (this->is_mounted)
                        pointq = bash_quote(this->mount_point);
                    const std::string command2 = ztd::replace(command, "%a", pointq);
                    command = ztd::strdup(command2);
                }
            }
            break;

        case VFSVolumeDeviceType::OTHER:
            if (mtab_fstype_is_handled_by_protocol(this->fs_type))
            {
                command = vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_NET,
                                                 PtkHandlerMount::HANDLER_UNMOUNT,
                                                 this,
                                                 nullptr,
                                                 nullptr,
                                                 run_in_terminal,
                                                 nullptr);
                // igtodo is this redundant?
                // replace mount point sub var
                if (command && ztd::contains(command, "%a"))
                {
                    if (this->is_mounted)
                        pointq = bash_quote(this->mount_point);
                    const std::string command2 = ztd::replace(command, "%a", pointq);
                    command = ztd::strdup(command2);
                }
            }
            break;
        case VFSVolumeDeviceType::BLOCK:

        default:
            command = vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_FS,
                                             PtkHandlerMount::HANDLER_UNMOUNT,
                                             this,
                                             nullptr,
                                             netmount,
                                             run_in_terminal,
                                             nullptr);
            break;
    }

    if (command)
        return command;

    // discovery
    std::string command2;
    std::string path;

    pointq = bash_quote(this->device_type == VFSVolumeDeviceType::BLOCK || !this->is_mounted
                            ? this->device_file
                            : this->mount_point);

    // udevil
    path = Glib::find_program_in_path("udevil");
    if (!path.empty())
        command2 = fmt::format("{} umount {}", path, pointq);

    // pmount
    path = Glib::find_program_in_path("pumount");
    if (!path.empty())
        command2 = fmt::format("{} {}", path, pointq);

    // udisks2
    path = Glib::find_program_in_path("udisksctl");
    if (!path.empty())
        command2 = fmt::format("{} unmount -b {}", path, pointq);

    *run_in_terminal = false;

    return ztd::strdup(command2);
}

const char*
VFSVolume::get_mount_command(const char* default_options, bool* run_in_terminal) noexcept
{
    const char* options = this->get_mount_options(default_options);
    const char* command = this->device_mount_cmd(options, run_in_terminal);

    return command;
}

const char*
VFSVolume::get_mount_options(const char* options) const noexcept
{
    if (!options)
        return nullptr;
    // change spaces to commas
    bool leading = true;
    bool trailing = false;
    i32 j = -1;
    char news[16384];
    for (usize i = 0; i < std::strlen(options); ++i)
    {
        if (leading && (options[i] == ' ' || options[i] == ','))
            continue;
        if (options[i] != ' ' && options[i] != ',')
        {
            j++;
            news[j] = options[i];
            trailing = true;
            leading = false;
        }
        else if (trailing)
        {
            j++;
            news[j] = ',';
            trailing = false;
        }
    }
    if (news[j] == ',')
        news[j] = '\0';
    else
    {
        j++;
        news[j] = '\0';
    }

    // no options
    if (news[0] == '\0')
        return nullptr;

    // parse options with fs type
    // nosuid,sync+vfat,utf+vfat,nosuid-ext4
    const std::string opts = fmt::format(",{},", news);
    const char* fstype = this->get_fstype();
    char newo[16384];
    newo[0] = ',';
    newo[1] = '\0';
    char* newoptr = newo + 1;
    char* ptr = (char*)opts.data() + 1;
    char* comma;
    char* single;
    std::string singlefs;
    char* plus;
    std::string test;
    while (ptr[0] != '\0')
    {
        comma = strchr(ptr, ',');
        single = strndup(ptr, comma - ptr);
        if (!strchr(single, '+') && !strchr(single, '-'))
        {
            // pure option, check for -fs option
            test = fmt::format(",{}-{},", single, fstype);
            if (!ztd::contains(opts, test))
            {
                // add option
                strncpy(newoptr, single, sizeof(char));
                newoptr = newo + std::strlen(newo);
                newoptr[0] = ',';
                newoptr[1] = '\0';
                newoptr++;
            }
        }
        else if ((plus = strchr(single, '+')))
        {
            // opt+fs
            plus[0] = '\0'; // set single to just option
            singlefs = fmt::format("{}+{}", single, fstype);
            plus[0] = '+'; // restore single to option+fs
            if (ztd::same(singlefs, single))
            {
                // correct fstype, check if already in options
                plus[0] = '\0'; // set single to just option
                test = fmt::format(",{},", single);
                if (!ztd::contains(newo, test))
                {
                    // add +fs option
                    strncpy(newoptr, single, sizeof(char));
                    newoptr = newo + std::strlen(newo);
                    newoptr[0] = ',';
                    newoptr[1] = '\0';
                    newoptr++;
                }
            }
        }
        free(single);
        ptr = comma + 1;
    }
    newoptr--;
    newoptr[0] = '\0';
    if (newo[1] == '\0')
        return nullptr;
    else
        return ztd::strdup(newo + 1);
}

static void
exec_task(const char* command, bool run_in_terminal)
{ // run command as async task with optional terminal
    if (!(command && command[0]))
        return;

    const std::vector<std::string> file_list{"exec_task"};

    PtkFileTask* ptask = ptk_file_task_new(VFSFileTaskType::EXEC, file_list, "/", nullptr, nullptr);
    ptask->task->exec_action = "exec_task";
    ptask->task->exec_command = command;
    ptask->task->exec_sync = false;
    ptask->task->exec_export = false;
    ptask->task->exec_terminal = run_in_terminal;
    ptk_file_task_run(ptask);
}

void
VFSVolume::exec(const char* command) const noexcept
{
    // LOG_INFO("vfs_volume_exec {} {}", vol->device_file, command);
    if (!(command && command[0]) || this->device_type != VFSVolumeDeviceType::BLOCK)
        return;

    std::string cmd = command;

    const std::string quoted_mount = bash_quote(this->mount_point);
    const std::string quoted_label = bash_quote(this->label);
    cmd = ztd::replace(cmd, "%m", quoted_mount);
    cmd = ztd::replace(cmd, "%l", quoted_label);
    cmd = ztd::replace(cmd, "%v", this->device_file);

    LOG_INFO("Autoexec: {}", cmd);
    exec_task(cmd.c_str(), false);
}

void
VFSVolume::autoexec() noexcept
{
    const char* command = nullptr;

    // Note: audiocd is is_mountable
    if (!this->is_mountable || global_inhibit_auto ||
        this->device_type != VFSVolumeDeviceType::BLOCK || !this->is_automount())
        return;

    if (this->is_audiocd)
    {
        command = xset_get_s(XSetName::DEV_EXEC_AUDIO);
    }
    else if (this->is_mounted && this->mount_point)
    {
        if (this->inhibit_auto)
        {
            // user manually mounted this vol, so no autoexec this time
            this->inhibit_auto = false;
            return;
        }
        else
        {
            const std::string path = Glib::build_filename(this->mount_point, "VIDEO_TS");
            if (this->is_dvd && std::filesystem::is_directory(path))
            {
                command = xset_get_s(XSetName::DEV_EXEC_VIDEO);
            }
            else
            {
                if (xset_get_b(XSetName::DEV_AUTO_OPEN))
                {
                    MainWindow* main_window = main_window_get_last_active();
                    if (main_window)
                    {
                        LOG_INFO("Auto Open Tab for {} in {}",
                                 this->device_file,
                                 this->mount_point);
                        // PtkFileBrowser* file_browser =
                        //        (PtkFileBrowser*)main_window_get_current_file_browser(
                        //                                                main_window);
                        // if (file_browser)
                        //     file_browser->run_event<EventType::OPEN_ITEM>(this->mount_point,
                        //                                                   PtkOpenAction::PTK_OPEN_DIR);
                        // main_window_add_new_tab causes hang without GDK_THREADS_ENTER
                        main_window_add_new_tab(main_window, this->mount_point);
                        // LOG_INFO("DONE Auto Open Tab for {} in {}", this->device_file,
                        //                                             this->mount_point);
                    }
                    else
                    {
                        const std::string exe = get_prog_executable();
                        const std::string quote_path = bash_quote(this->mount_point);
                        const std::string cmd = fmt::format("{} -t {}", exe, quote_path);
                        print_command(cmd);
                        Glib::spawn_command_line_async(cmd);
                    }
                }
                command = xset_get_s(XSetName::DEV_EXEC_FS);
            }
        }
    }
    this->exec(command);
}

void
VFSVolume::autounmount() noexcept
{
    if (!this->is_mounted || !this->is_automount())
        return;
    bool run_in_terminal;
    const char* line = this->device_unmount_cmd(&run_in_terminal);
    if (line)
    {
        LOG_INFO("Auto-Unmount: {}", line);
        exec_task(line, run_in_terminal);
    }
    else
    {
        LOG_INFO("Auto-Unmount: error: no unmount command available");
    }
}

void
VFSVolume::automount() noexcept
{
    if (this->is_mounted || this->ever_mounted || this->is_audiocd || this->should_autounmount ||
        !this->is_automount())
        return;

    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    if (this->automount_time && now - this->automount_time < 5)
        return;
    this->automount_time = now;

    bool run_in_terminal;
    const char* line =
        this->get_mount_command(xset_get_s(XSetName::DEV_MOUNT_OPTIONS), &run_in_terminal);
    if (line)
    {
        LOG_INFO("Automount: {}", line);
        exec_task(line, run_in_terminal);
    }
    else
    {
        LOG_INFO("Automount: error: no mount command available");
    }
}

void
VFSVolume::device_added(bool automount) noexcept
{ // frees volume if needed
    char* changed_mount_point = nullptr;

    if (!this->udi || !this->device_file)
        return;

    // check if we already have this volume device file
    for (vfs::volume volume : volumes)
    {
        if (volume->devnum == this->devnum)
        {
            // update existing volume
            bool was_mounted = volume->is_mounted;
            bool was_audiocd = volume->is_audiocd;
            bool was_mountable = volume->is_mountable;

            // detect changed mount point
            if (!was_mounted && this->is_mounted)
                changed_mount_point = ztd::strdup(this->mount_point);
            else if (was_mounted && !this->is_mounted)
                changed_mount_point = ztd::strdup(volume->mount_point);

            volume->udi = ztd::strdup(this->udi);
            volume->device_file = ztd::strdup(this->device_file);
            volume->label = this->label;
            volume->mount_point = ztd::strdup(this->mount_point);
            volume->icon = this->icon;
            volume->disp_name = ztd::strdup(this->disp_name);
            volume->is_mounted = this->is_mounted;
            volume->is_mountable = this->is_mountable;
            volume->is_optical = this->is_optical;
            volume->requires_eject = this->requires_eject;
            volume->is_removable = this->is_removable;
            volume->is_user_visible = this->is_user_visible;
            volume->size = this->size;
            volume->is_table = this->is_table;
            volume->is_floppy = this->is_floppy;
            volume->nopolicy = this->nopolicy;
            volume->fs_type = this->fs_type;
            volume->is_blank = this->is_blank;
            volume->is_audiocd = this->is_audiocd;
            volume->is_dvd = this->is_dvd;

            // Mount and ejection detect for automount
            if (this->is_mounted)
            {
                volume->ever_mounted = true;
                volume->automount_time = 0;
            }
            else
            {
                if (this->is_removable && !this->is_mountable) // ejected
                {
                    volume->ever_mounted = false;
                    volume->automount_time = 0;
                    volume->inhibit_auto = false;
                }
            }

            call_callbacks(volume, VFSVolumeState::CHANGED);

            if (automount)
            {
                this->automount();
                if (!was_mounted && this->is_mounted)
                    this->autoexec();
                else if (was_mounted && !this->is_mounted)
                {
                    this->exec(xset_get_s(XSetName::DEV_EXEC_UNMOUNT));
                    this->should_autounmount = false;
                    // remove mount points in case other unmounted
                    ptk_location_view_clean_mount_points();
                }
                else if (!was_audiocd && this->is_audiocd)
                    this->autoexec();

                // media inserted ?
                if (!was_mountable && this->is_mountable)
                    this->exec(xset_get_s(XSetName::DEV_EXEC_INSERT));

                // media ejected ?
                if (was_mountable && !this->is_mountable && this->is_mounted &&
                    (this->is_optical || this->is_removable))
                    this->unmount_if_mounted();
            }
            // refresh tabs containing changed mount point
            if (changed_mount_point)
            {
                main_window_refresh_all_tabs_matching(changed_mount_point);
                free(changed_mount_point);
            }
            return;
        }
    }

    // add as new volume
    volumes.emplace_back(this);
    call_callbacks(this, VFSVolumeState::ADDED);
    if (automount)
    {
        this->automount();
        this->exec(xset_get_s(XSetName::DEV_EXEC_INSERT));
        if (this->is_audiocd)
            this->autoexec();
    }
    // refresh tabs containing changed mount point
    if (this->is_mounted && this->mount_point)
        main_window_refresh_all_tabs_matching(this->mount_point);
}

static bool
vfs_volume_nonblock_removed(dev_t devnum)
{
    for (vfs::volume volume : volumes)
    {
        if (volume->device_type != VFSVolumeDeviceType::BLOCK && volume->devnum == devnum)
        {
            // remove volume
            LOG_INFO("special mount removed: {} ({}:{}) on {}",
                     volume->device_file,
                     MAJOR(volume->devnum),
                     MINOR(volume->devnum),
                     volume->mount_point);
            volumes.erase(std::remove(volumes.begin(), volumes.end(), volume), volumes.end());
            call_callbacks(volume, VFSVolumeState::REMOVED);
            delete volume;
            ptk_location_view_clean_mount_points();
            return true;
        }
    }
    return false;
}

static void
vfs_volume_device_removed(struct udev_device* udevice)
{
    if (!udevice)
        return;

    dev_t devnum = udev_device_get_devnum(udevice);

    for (vfs::volume volume : volumes)
    {
        if (volume->device_type == VFSVolumeDeviceType::BLOCK && volume->devnum == devnum)
        {
            // remove volume
            // LOG_INFO("remove volume {}", volume->device_file);
            volume->exec(xset_get_s(XSetName::DEV_EXEC_REMOVE));
            if (volume->is_mounted && volume->is_removable)
                volume->unmount_if_mounted();
            volumes.erase(std::remove(volumes.begin(), volumes.end(), volume), volumes.end());
            call_callbacks(volume, VFSVolumeState::REMOVED);
            if (volume->is_mounted && volume->mount_point)
                main_window_refresh_all_tabs_matching(volume->mount_point);
            delete volume;
            break;
        }
    }
    ptk_location_view_clean_mount_points();
}

void
VFSVolume::unmount_if_mounted() noexcept
{
    if (!this->device_file)
        return;

    bool run_in_terminal;

    const char* str = this->device_unmount_cmd(&run_in_terminal);
    if (!str)
        return;

    const std::string line =
        fmt::format("grep -qs '^{} ' {} 2>/dev/null || exit\n{}\n", this->device_file, MTAB, str);
    LOG_INFO("Unmount-If-Mounted: {}", line);
    exec_task(line.c_str(), run_in_terminal);
}

static bool
on_cancel_inhibit_timer(void* user_data)
{
    (void)user_data;
    global_inhibit_auto = false;
    return false;
}

bool
vfs_volume_init()
{
    struct udev_device* udevice;
    struct udev_enumerate* enumerate;
    struct udev_list_entry *devices, *dev_list_entry;

    // remove unused mount points
    ptk_location_view_clean_mount_points();

    // create udev
    udev = udev_new();
    if (!udev)
    {
        LOG_INFO("unable to initialize udev");
        return true;
    }

    // read all block mount points
    parse_mounts(false);

    // enumerate devices
    enumerate = udev_enumerate_new(udev);
    if (enumerate)
    {
        udev_enumerate_add_match_subsystem(enumerate, "block");
        udev_enumerate_scan_devices(enumerate);
        devices = udev_enumerate_get_list_entry(enumerate);

        udev_list_entry_foreach(dev_list_entry, devices)
        {
            const char* syspath = udev_list_entry_get_name(dev_list_entry);
            udevice = udev_device_new_from_syspath(udev, syspath);
            if (udevice)
            {
                vfs::volume volume = vfs_volume_read_by_device(udevice);
                if (volume)
                    volume->device_added(false); // frees volume if needed
                udev_device_unref(udevice);
            }
        }
        udev_enumerate_unref(enumerate);
    }

    // enumerate non-block
    parse_mounts(true);

    // start udev monitor
    umonitor = udev_monitor_new_from_netlink(udev, "udev");
    if (!umonitor)
    {
        LOG_INFO("cannot create udev monitor");
        if (umonitor)
        {
            udev_monitor_unref(umonitor);
            umonitor = nullptr;
        }
        if (udev)
        {
            udev_unref(udev);
            udev = nullptr;
        }
        return true;
    }
    if (udev_monitor_enable_receiving(umonitor))
    {
        LOG_INFO("cannot enable udev monitor receiving");
        if (umonitor)
        {
            udev_monitor_unref(umonitor);
            umonitor = nullptr;
        }
        if (udev)
        {
            udev_unref(udev);
            udev = nullptr;
        }
        return true;
    }
    if (udev_monitor_filter_add_match_subsystem_devtype(umonitor, "block", nullptr))
    {
        LOG_INFO("cannot set udev filter");
        if (umonitor)
        {
            udev_monitor_unref(umonitor);
            umonitor = nullptr;
        }
        if (udev)
        {
            udev_unref(udev);
            udev = nullptr;
        }
        return true;
    }

    const i32 ufd = udev_monitor_get_fd(umonitor);
    if (ufd == 0)
    {
        LOG_INFO("scannot get udev monitor socket file descriptor");
        if (umonitor)
        {
            udev_monitor_unref(umonitor);
            umonitor = nullptr;
        }
        if (udev)
        {
            udev_unref(udev);
            udev = nullptr;
        }
        return true;
    }
    global_inhibit_auto = true; // do not autoexec during startup

    uchannel = Glib::IOChannel::create_from_fd(ufd);
    uchannel->set_flags(Glib::IOFlags::NONBLOCK);
    uchannel->set_close_on_unref(true);

    Glib::signal_io().connect(sigc::ptr_fun(cb_udev_monitor_watch),
                              uchannel,
                              Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);

    // start mount monitor
    mchannel = Glib::IOChannel::create_from_file(MOUNTINFO.data(), "r");
    mchannel->set_close_on_unref(true);

    Glib::signal_io().connect(sigc::ptr_fun(cb_mount_monitor_watch),
                              mchannel,
                              Glib::IOCondition::IO_ERR);

    // do startup automounts
    for (vfs::volume volume : volumes)
        volume->automount();

    // start resume autoexec timer
    g_timeout_add_seconds(3, (GSourceFunc)on_cancel_inhibit_timer, nullptr);

    return true;
}

void
vfs_volume_finalize()
{
    // stop mount monitor
    if (mchannel)
        mchannel = nullptr;

    free_devmounts();

    // stop udev monitor
    if (uchannel)
        uchannel = nullptr;

    if (umonitor)
    {
        udev_monitor_unref(umonitor);
        umonitor = nullptr;
    }
    if (udev)
    {
        udev_unref(udev);
        udev = nullptr;
    }

    // free callbacks
    callbacks.clear(); // free all shared_ptr

    // free volumes / unmount all ?
    bool unmount_all = xset_get_b(XSetName::DEV_UNMOUNT_QUIT);
    while (true)
    {
        if (volumes.empty())
            break;

        vfs::volume volume = volumes.back();
        volumes.pop_back();

        if (unmount_all)
            volume->autounmount();

        delete volume;
    }

    // remove unused mount points
    ptk_location_view_clean_mount_points();
}

const std::vector<vfs::volume>
vfs_volume_get_all_volumes()
{
    return volumes;
}

vfs::volume
vfs_volume_get_by_device(std::string_view device_file)
{
    if (device_file.empty())
        return nullptr;

    if (volumes.empty())
        return nullptr;

    for (vfs::volume volume : volumes)
    {
        if (ztd::same(device_file, volume->device_file))
            return volume;
    }

    return nullptr;
}

vfs::volume
vfs_volume_get_by_device_or_point(std::string_view device_file, std::string_view point)
{
    if (point.empty() && device_file.empty())
        return nullptr;

    if (volumes.empty())
        return nullptr;

    // canonicalize point
    const std::string canon = std::filesystem::canonical(point);

    for (vfs::volume volume : volumes)
    {
        if (ztd::same(device_file, volume->device_file))
            return volume;

        if (volume->is_mounted && volume->mount_point)
        {
            if (ztd::same(point, volume->mount_point) || ztd::same(canon, volume->mount_point))
                return volume;
        }
    }

    return nullptr;
}

static void
call_callbacks(vfs::volume vol, VFSVolumeState state)
{
    if (!callbacks.empty())
    {
        for (volume_callback_data_t callback : callbacks)
        {
            callback->cb(vol, state, callback->user_data);
        }
    }

    if (event_handler->device->s || event_handler->device->ob2_data)
    {
        main_window_event(nullptr,
                          nullptr,
                          XSetName::EVT_DEVICE,
                          0,
                          0,
                          vol->device_file,
                          0,
                          0,
                          state,
                          false);
    }
}

void
vfs_volume_add_callback(VFSVolumeCallback cb, void* user_data)
{
    if (!cb)
        return;

    volume_callback_data_t data = std::make_shared<VFSVolumeCallbackData>(cb, user_data);

    callbacks.emplace_back(data);
}

void
vfs_volume_remove_callback(VFSVolumeCallback cb, void* user_data)
{
    if (callbacks.empty())
        return;

    for (auto callback : callbacks)
    {
        if (callback->cb == cb && callback->user_data == user_data)
        {
            ztd::remove(callbacks, callback);
            break;
        }
    }
}

const char*
VFSVolume::get_disp_name() const noexcept
{
    return this->disp_name;
}

const char*
VFSVolume::get_mount_point() const noexcept
{
    return this->mount_point;
}

const char*
VFSVolume::get_device() const noexcept
{
    return this->device_file;
}
const char*
VFSVolume::get_fstype() const noexcept
{
    return this->fs_type;
}

const char*
VFSVolume::get_icon() const noexcept
{
    if (this->icon.empty())
        return nullptr;
    xset_t set = xset_get(this->icon);
    return set->icon;
}

bool
vfs_volume_dir_avoid_changes(std::string_view dir)
{
    // determines if file change detection should be disabled for this
    // dir (eg nfs stat calls block when a write is in progress so file
    // change detection is unwanted)
    // return false to detect changes in this dir, true to avoid change detection
    const char* devnode;
    bool ret;

    // LOG_INFO("vfs_volume_dir_avoid_changes( {} )", dir);
    if (!udev || dir.empty())
        return false;

    // canonicalize path
    const std::string canon = std::filesystem::canonical(dir);

    // get devnum
    const auto statbuf = ztd::stat(canon);
    if (!statbuf.is_valid())
        return false;
    // LOG_INFO("    statbuf.dev() = {}:{}", major(statbuf.dev()), minor(statbuf.dev()));

    struct udev_device* udevice = udev_device_new_from_devnum(udev, 'b', statbuf.dev());
    if (udevice)
        devnode = udev_device_get_devnode(udevice);
    else
        devnode = nullptr;

    if (devnode == nullptr)
    {
        // not a block device
        const char* fstype = get_devmount_fstype(major(statbuf.dev()), minor(statbuf.dev()));
        // LOG_INFO("    !udevice || !devnode  fstype={}", fstype);
        ret = false;
        if (fstype && fstype[0])
        {
            // fstype listed in change detection blacklist?
            i32 len = std::strlen(fstype);
            char* ptr = ztd::strdup(xset_get_s(XSetName::DEV_CHANGE));
            if (ptr)
            {
                while (ptr[0])
                {
                    while (ptr[0] == ' ' || ptr[0] == ',')
                        ptr++;
                    if (ztd::startswith(ptr, fstype) &&
                        (ptr[len] == ' ' || ptr[len] == ',' || ptr[len] == '\0'))
                    {
                        LOG_INFO("Change Detection Blacklist: fstype '{}' on {}", fstype, dir);
                        ret = true;
                        break;
                    }
                    while (ptr[0] != ' ' && ptr[0] != ',' && ptr[0])
                        ptr++;
                }
            }
        }
    }
    else
    { // block device
        ret = false;
    }

    if (udevice)
        udev_device_unref(udevice);
    // LOG_INFO("    avoid_changes = {}", ret ? "true" : "false");
    return ret;
}

dev_t
get_device_parent(dev_t dev)
{
    if (!udev)
        return 0;

    struct udev_device* udevice = udev_device_new_from_devnum(udev, 'b', dev);
    if (!udevice)
        return 0;
    char* native_path = ztd::strdup(udev_device_get_syspath(udevice));
    udev_device_unref(udevice);

    if (!native_path || !sysfs_file_exists(native_path, "start"))
    {
        // not a partition if no "start"
        free(native_path);
        return 0;
    }

    const std::string parent = Glib::path_get_dirname(native_path);
    free(native_path);
    udevice = udev_device_new_from_syspath(udev, parent.c_str());
    if (!udevice)
        return 0;
    dev_t retdev = udev_device_get_devnum(udevice);
    udev_device_unref(udevice);
    return retdev;
}
