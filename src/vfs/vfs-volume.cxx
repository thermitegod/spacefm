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
#include <filesystem>

#include <vector>

#include <chrono>

#include <libudev.h>
#include <fcntl.h>
#include <linux/kdev_t.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "main-window.hxx"

#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-location-view.hxx"

#include "utils.hxx"

#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-volume.hxx"

#define MOUNTINFO "/proc/self/mountinfo"
#define MTAB      "/proc/mounts"
#define HIDDEN_NON_BLOCK_FS                                                                    \
    "devpts proc fusectl pstore sysfs tmpfs devtmpfs ramfs aufs overlayfs cgroup binfmt_misc " \
    "rpc_pipefs fuse.gvfsd-fuse"

static VFSVolume* vfs_volume_read_by_device(struct udev_device* udevice);
static VFSVolume* vfs_volume_read_by_mount(dev_t devnum, const char* mount_points);
static void vfs_volume_device_added(VFSVolume* volume, bool automount);
static void vfs_volume_device_removed(struct udev_device* udevice);
static bool vfs_volume_nonblock_removed(dev_t devnum);
static void call_callbacks(VFSVolume* vol, VFSVolumeState state);
static void unmount_if_mounted(VFSVolume* vol);

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

netmount_t::netmount_t()
{
    this->url = nullptr;
    this->fstype = nullptr;
    this->host = nullptr;
    this->ip = nullptr;
    this->port = nullptr;
    this->user = nullptr;
    this->pass = nullptr;
    this->path = nullptr;
}

netmount_t::~netmount_t()
{
    if (this->url)
        free(this->url);
    if (this->fstype)
        free(this->fstype);
    if (this->host)
        free(this->host);
    if (this->ip)
        free(this->ip);
    if (this->port)
        free(this->port);
    if (this->user)
        free(this->user);
    if (this->pass)
        free(this->pass);
    if (this->path)
        free(this->path);
}

struct VFSVolumeCallbackData
{
    VFSVolumeCallback cb;
    void* user_data;
};

static std::vector<VFSVolume*> volumes;
static GArray* callbacks = nullptr;
static bool global_inhibit_auto = false;

struct devmount_t
{
    devmount_t(dev_t major, dev_t minor);
    ~devmount_t();

    dev_t major;
    dev_t minor;
    char* mount_points;
    char* fstype;
    GList* mounts;
};

devmount_t::devmount_t(dev_t major, dev_t minor)
{
    this->major = major;
    this->minor = minor;
    this->mount_points = nullptr;
    this->fstype = nullptr;
    this->mounts = nullptr;
}

devmount_t::~devmount_t()
{
    if (this->mount_points)
        free(this->mount_points);
    if (this->fstype)
        free(this->fstype);
}

static GList* devmounts = nullptr;
static struct udev* udev = nullptr;
static struct udev_monitor* umonitor = nullptr;
static GIOChannel* uchannel = nullptr;
static GIOChannel* mchannel = nullptr;

/* *************************************************************************
 * device info
 ************************************************************************** */

struct device_t
{
    device_t(udev_device* udevice);
    ~device_t();

    struct udev_device* udevice;
    dev_t devnum;
    char* devnode;
    char* native_path;
    char* mount_points;

    bool device_is_system_internal;
    bool device_is_partition;
    bool device_is_partition_table;
    bool device_is_removable;
    bool device_is_media_available;
    bool device_is_read_only;
    bool device_is_drive;
    bool device_is_optical_disc;
    bool device_is_mounted;
    char* device_presentation_hide;
    char* device_presentation_nopolicy;
    char* device_presentation_name;
    char* device_presentation_icon_name;
    char* device_automount_hint;
    char* device_by_id;
    uint64_t device_size;
    uint64_t device_block_size;
    char* id_usage;
    char* id_type;
    char* id_version;
    char* id_uuid;
    char* id_label;

    char* drive_vendor;
    char* drive_model;
    char* drive_revision;
    char* drive_serial;
    char* drive_wwn;
    char* drive_connection_interface;
    uint64_t drive_connection_speed;
    char* drive_media_compatibility;
    char* drive_media;
    bool drive_is_media_ejectable;
    bool drive_can_detach;

    char* partition_scheme;
    char* partition_number;
    char* partition_type;
    char* partition_label;
    char* partition_uuid;
    char* partition_flags;
    char* partition_offset;
    char* partition_size;
    char* partition_alignment_offset;

    char* partition_table_scheme;
    char* partition_table_count;

    bool optical_disc_is_blank;
    bool optical_disc_is_appendable;
    bool optical_disc_is_closed;
    char* optical_disc_num_tracks;
    char* optical_disc_num_audio_tracks;
    char* optical_disc_num_sessions;
};

device_t::device_t(udev_device* udevice)
{
    this->udevice = udevice;

    this->native_path = nullptr;
    this->mount_points = nullptr;
    this->devnode = nullptr;

    this->device_is_system_internal = true;
    this->device_is_partition = false;
    this->device_is_partition_table = false;
    this->device_is_removable = false;
    this->device_is_media_available = false;
    this->device_is_read_only = false;
    this->device_is_drive = false;
    this->device_is_optical_disc = false;
    this->device_is_mounted = false;
    this->device_presentation_hide = nullptr;
    this->device_presentation_nopolicy = nullptr;
    this->device_presentation_name = nullptr;
    this->device_presentation_icon_name = nullptr;
    this->device_automount_hint = nullptr;
    this->device_by_id = nullptr;
    this->device_size = 0;
    this->device_block_size = 0;
    this->id_usage = nullptr;
    this->id_type = nullptr;
    this->id_version = nullptr;
    this->id_uuid = nullptr;
    this->id_label = nullptr;

    this->drive_vendor = nullptr;
    this->drive_model = nullptr;
    this->drive_revision = nullptr;
    this->drive_serial = nullptr;
    this->drive_wwn = nullptr;
    this->drive_connection_interface = nullptr;
    this->drive_connection_speed = 0;
    this->drive_media_compatibility = nullptr;
    this->drive_media = nullptr;
    this->drive_is_media_ejectable = false;
    this->drive_can_detach = false;

    this->partition_scheme = nullptr;
    this->partition_number = nullptr;
    this->partition_type = nullptr;
    this->partition_label = nullptr;
    this->partition_uuid = nullptr;
    this->partition_flags = nullptr;
    this->partition_offset = nullptr;
    this->partition_size = nullptr;
    this->partition_alignment_offset = nullptr;

    this->partition_table_scheme = nullptr;
    this->partition_table_count = nullptr;

    this->optical_disc_is_blank = false;
    this->optical_disc_is_appendable = false;
    this->optical_disc_is_closed = false;
    this->optical_disc_num_tracks = nullptr;
    this->optical_disc_num_audio_tracks = nullptr;
    this->optical_disc_num_sessions = nullptr;
}

device_t::~device_t()
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

static int
ptr_str_array_compare(const char** a, const char** b)
{
    return g_strcmp0(*a, *b);
}

static double
sysfs_get_double(const char* dir, const char* attribute)
{
    std::string contents;
    std::string filename = Glib::build_filename(dir, attribute);
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

static char*
sysfs_get_string(const char* dir, const char* attribute)
{
    std::string result;
    std::string filename = Glib::build_filename(dir, attribute);
    try
    {
        result = Glib::file_get_contents(filename);
    }
    catch (Glib::FileError)
    {
        return ztd::strdup("");
    }
    return ztd::strdup(result);
}

static int
sysfs_get_int(const char* dir, const char* attribute)
{
    std::string contents;
    std::string filename = Glib::build_filename(dir, attribute);
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

static uint64_t
sysfs_get_uint64(const char* dir, const char* attribute)
{
    std::string contents;
    std::string filename = Glib::build_filename(dir, attribute);
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
sysfs_file_exists(const char* dir, const char* attribute)
{
    std::string filename = Glib::build_filename(dir, attribute);
    if (std::filesystem::exists(filename))
        return true;
    return false;
}

static char*
sysfs_resolve_link(const char* sysfs_path, const char* name)
{
    std::string target_path;
    std::string full_path = Glib::build_filename(sysfs_path, name);
    try
    {
        target_path = std::filesystem::read_symlink(full_path);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        // LOG_WARN("{}", e.what());
        return nullptr;
    }
    return ztd::strdup(target_path);
}

static bool
info_is_system_internal(device_t* device)
{
    const char* value;

    if ((value = udev_device_get_property_value(device->udevice, "UDISKS_SYSTEM_INTERNAL")))
        return strtol(value, nullptr, 10) != 0;

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
        if (!strcmp(device->drive_connection_interface, "ata_serial_esata") ||
            !strcmp(device->drive_connection_interface, "sdio") ||
            !strcmp(device->drive_connection_interface, "usb") ||
            !strcmp(device->drive_connection_interface, "firewire"))
            return false;
    }
    return true;
}

static void
info_drive_connection(device_t* device)
{
    const char* connection_interface = nullptr;
    uint64_t connection_speed = 0;

    /* walk up the device tree to figure out the subsystem */
    char* s = ztd::strdup(device->native_path);
    do
    {
        char* model;
        char* p = sysfs_resolve_link(s, "subsystem");
        if (!device->device_is_removable && sysfs_get_int(s, "removable") != 0)
            device->device_is_removable = true;
        if (p != nullptr)
        {
            char* subsystem = g_path_get_basename(p);
            free(p);

            if (!strcmp(subsystem, "scsi"))
            {
                connection_interface = "scsi";
                connection_speed = 0;

                /* continue walking up the chain; we just use scsi as a fallback */

                /* grab the names from SCSI since the names from udev currently
                 *  - replaces whitespace with _
                 *  - is missing for e.g. Firewire
                 */
                char* vendor = sysfs_get_string(s, "vendor");
                if (vendor != nullptr)
                {
                    g_strstrip(vendor);
                    /* Do not overwrite what we set earlier from ID_VENDOR */
                    if (device->drive_vendor == nullptr)
                    {
                        device->drive_vendor = ztd::strdup(vendor);
                    }
                    free(vendor);
                }

                model = sysfs_get_string(s, "model");
                if (model != nullptr)
                {
                    g_strstrip(model);
                    /* Do not overwrite what we set earlier from ID_MODEL */
                    if (device->drive_model == nullptr)
                    {
                        device->drive_model = ztd::strdup(model);
                    }
                    free(model);
                }

                /* TODO: need to improve this code; we probably need the kernel to export more
                 *       information before we can properly get the type and speed.
                 */

                if (device->drive_vendor != nullptr && !strcmp(device->drive_vendor, "ATA"))
                {
                    connection_interface = "ata";
                    break;
                }
            }
            else if (!strcmp(subsystem, "usb"))
            {
                double usb_speed;

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
            else if (!strcmp(subsystem, "firewire") || !strcmp(subsystem, "ieee1394"))
            {
                /* TODO: krh has promised a speed file in sysfs; theoretically, the speed can
                 *       be anything from 100, 200, 400, 800 and 3200. Till then we just hardcode
                 *       a resonable default of 400 Mbit/s.
                 */

                connection_interface = "firewire";
                connection_speed = 400 * (1000 * 1000);
                break;
            }
            else if (!strcmp(subsystem, "mmc"))
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

                model = sysfs_get_string(s, "name");
                if (model != nullptr)
                {
                    g_strstrip(model);
                    /* Do not overwrite what we set earlier from ID_MODEL */
                    if (device->drive_model == nullptr)
                    {
                        device->drive_model = ztd::strdup(model);
                    }
                    free(model);
                }

                char* serial = sysfs_get_string(s, "serial");
                if (serial != nullptr)
                {
                    g_strstrip(serial);
                    /* Do not overwrite what we set earlier from ID_SERIAL */
                    if (device->drive_serial == nullptr)
                    {
                        /* this is formatted as a hexnumber; drop the leading 0x */
                        device->drive_serial = ztd::strdup(serial + 2);
                    }
                    free(serial);
                }

                /* TODO: use hwrev and fwrev files? */
                char* revision = sysfs_get_string(s, "date");
                if (revision != nullptr)
                {
                    g_strstrip(revision);
                    /* Do not overwrite what we set earlier from ID_REVISION */
                    if (device->drive_revision == nullptr)
                    {
                        device->drive_revision = ztd::strdup(revision);
                    }
                    free(revision);
                }

                /* TODO: interface speed; the kernel driver knows; would be nice
                 * if it could export it */
            }
            else if (!strcmp(subsystem, "platform"))
            {
                const char* sysfs_name;

                sysfs_name = g_strrstr(s, "/");
                if (Glib::str_has_prefix(sysfs_name + 1, "floppy.") &&
                    device->drive_vendor == nullptr)
                {
                    device->drive_vendor = ztd::strdup("Floppy Drive");
                    connection_interface = "platform";
                }
            }

            free(subsystem);
        }

        /* advance up the chain */
        p = g_strrstr(s, "/");
        if (p == nullptr)
            break;
        *p = '\0';

        /* but stop at the root */
        if (!strcmp(s, "/sys/devices"))
            break;

    } while (true);

    if (connection_interface != nullptr)
    {
        device->drive_connection_interface = ztd::strdup(connection_interface);
        device->drive_connection_speed = connection_speed;
    }

    free(s);
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
info_drive_properties(device_t* device)
{
    GPtrArray* media_compat_array;
    const char* media_in_drive;
    bool drive_is_ejectable;
    bool drive_can_detach;
    unsigned int n;
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

    /* pick up some things (vendor, model, connection_interface, connection_speed)
     * not (yet) exported by udev helpers
     */
    // update_drive_properties_from_sysfs (device);
    info_drive_connection(device);

    // is_ejectable
    if ((value = udev_device_get_property_value(device->udevice, "ID_DRIVE_EJECTABLE")))
    {
        drive_is_ejectable = strtol(value, nullptr, 10) != 0;
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
    for (n = 0; drive_media_mapping[n].udev_property != nullptr; n++)
    {
        if (udev_device_get_property_value(device->udevice, drive_media_mapping[n].udev_property) ==
            nullptr)
            continue;

        g_ptr_array_add(media_compat_array, (void*)drive_media_mapping[n].media_name);
    }
    /* special handling for SDIO since we do not yet have a sdio_id helper in udev to set properties
     */
    if (!g_strcmp0(device->drive_connection_interface, "sdio"))
    {
        char* type;

        type = sysfs_get_string(device->native_path, "../../type");
        g_strstrip(type);
        if (!g_strcmp0(type, "MMC"))
        {
            g_ptr_array_add(media_compat_array, (void*)"flash_mmc");
        }
        else if (!g_strcmp0(type, "SD"))
        {
            g_ptr_array_add(media_compat_array, (void*)"flash_sd");
        }
        else if (!g_strcmp0(type, "SDHC"))
        {
            g_ptr_array_add(media_compat_array, (void*)"flash_sdhc");
        }
        free(type);
    }
    g_ptr_array_sort(media_compat_array, (GCompareFunc)ptr_str_array_compare);
    g_ptr_array_add(media_compat_array, nullptr);
    device->drive_media_compatibility = g_strjoinv(" ", (char**)media_compat_array->pdata);

    // drive_media
    media_in_drive = nullptr;
    if (device->device_is_media_available)
    {
        for (n = 0; media_mapping[n].udev_property != nullptr; n++)
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
    if (!g_strcmp0(device->drive_connection_interface, "usb"))
    {
        drive_can_detach = true;
    }
    if ((value = udev_device_get_property_value(device->udevice, "ID_DRIVE_DETACHABLE")))
    {
        drive_can_detach = strtol(value, nullptr, 10) != 0;
    }
    device->drive_can_detach = drive_can_detach;
}

static void
info_device_properties(device_t* device)
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
    const char* partition_scheme;
    int partition_type = 0;

    partition_scheme = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_SCHEME");
    if ((value = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_TYPE")))
        partition_type = strtol(value, nullptr, 10);
    if (!g_strcmp0(partition_scheme, "mbr") &&
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
    else if (Glib::str_has_prefix(device->devnode, "/dev/loop"))
        media_available = false;
    else if (device->device_is_removable)
    {
        bool is_cd, is_floppy;
        if ((value = udev_device_get_property_value(device->udevice, "ID_CDROM")))
            is_cd = strtol(value, nullptr, 10) != 0;
        else
            is_cd = false;

        if ((value = udev_device_get_property_value(device->udevice, "ID_DRIVE_FLOPPY")))
            is_floppy = strtol(value, nullptr, 10) != 0;
        else
            is_floppy = false;

        if (!is_cd && !is_floppy)
        {
            // this test is limited for non-root - user may not have read
            // access to device file even if media is present
            int fd;
            fd = open(device->devnode, O_RDONLY);
            if (fd >= 0)
            {
                media_available = true;
                close(fd);
            }
        }
        else if ((value = udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA")))
            media_available = (strtol(value, nullptr, 10) == 1);
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA")))
        media_available = (strtol(value, nullptr, 10) == 1);
    else
        media_available = true;
    device->device_is_media_available = media_available;

    /* device_size, device_block_size and device_is_read_only properties */
    if (device->device_is_media_available)
    {
        uint64_t block_size;

        device->device_size = sysfs_get_uint64(device->native_path, "size") * ((uint64_t)512);
        device->device_is_read_only = (sysfs_get_int(device->native_path, "ro") != 0);
        /* This is not available on all devices so fall back to 512 if unavailable.
         *
         * Another way to get this information is the BLKSSZGET ioctl but we do not want
         * to open the device. Ideally vol_id would export it.
         */
        block_size = sysfs_get_uint64(device->native_path, "queue/hw_sector_size");
        if (block_size == 0)
            block_size = 512;
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
        if (entry_name && (Glib::str_has_prefix(entry_name, "/dev/disk/by-id/") ||
                           Glib::str_has_prefix(entry_name, "/dev/disk/by-uuid/")))
        {
            device->device_by_id = ztd::strdup(entry_name);
            break;
        }
        entry = udev_list_entry_get_next(entry);
    }
}

static char*
info_mount_points(device_t* device)
{
    GList* mounts = nullptr;

    dev_t dmajor = MAJOR(device->devnum);
    dev_t dminor = MINOR(device->devnum);

    // if we have the mount point list, use this instead of reading mountinfo
    if (devmounts)
    {
        GList* l;
        for (l = devmounts; l; l = l->next)
        {
            if ((static_cast<devmount_t*>(l->data))->major == dmajor &&
                (static_cast<devmount_t*>(l->data))->minor == dminor)
            {
                return ztd::strdup((static_cast<devmount_t*>(l->data))->mount_points);
            }
        }
        return nullptr;
    }

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(MOUNTINFO);
    }
    catch (const Glib::FileError& e)
    {
        std::string what = e.what();
        LOG_WARN("Error reading {}: {}", MOUNTINFO, what);
        return nullptr;
    }

    /* See Documentation/filesystems/proc.txt for the format of /proc/self/mountinfo
     *
     * Note that things like space are encoded as \020.
     */

    std::vector<std::string> lines = ztd::split(contents, "\n");
    for (const std::string& line: lines)
    {
        unsigned int mount_id;
        unsigned int parent_id;
        dev_t major, minor;
        char encoded_root[PATH_MAX];
        char encoded_mount_point[PATH_MAX];
        char* mount_point;

        if (line.size() == 0)
            continue;

        if (sscanf(line.c_str(),
                   "%d %d %lu:%lu %s %s",
                   &mount_id,
                   &parent_id,
                   &major,
                   &minor,
                   encoded_root,
                   encoded_mount_point) != 6)
        {
            LOG_WARN("Error reading /proc/self/mountinfo: Error parsing line '{}'", line);
            continue;
        }

        /* ignore mounts where only a subtree of a filesystem is mounted
         * this function is only used for block devices. */
        if (g_strcmp0(encoded_root, "/") != 0)
            continue;

        if (major != dmajor || minor != dminor)
            continue;

        mount_point = g_strcompress(encoded_mount_point);
        if (mount_point && mount_point[0] != '\0')
        {
            if (!g_list_find(mounts, mount_point))
            {
                mounts = g_list_prepend(mounts, mount_point);
            }
            else
                free(mount_point);
        }
    }

    if (mounts)
    {
        std::string points;
        GList* l;
        // Sort the list to ensure that shortest mount paths appear first
        mounts = g_list_sort(mounts, (GCompareFunc)g_strcmp0);
        points = ztd::strdup((char*)mounts->data);
        l = mounts;
        while ((l = l->next))
        {
            points = fmt::format("{}, {}", points, (char*)l->data);
        }
        g_list_foreach(mounts, (GFunc)free, nullptr);
        g_list_free(mounts);
        return ztd::strdup(points);
    }
    else
        return nullptr;
}

static void
info_partition_table(device_t* device)
{
    bool is_partition_table = false;
    const char* value;

    /* Check if udisks-part-id identified the device as a partition table.. this includes
     * identifying partition tables set up by kpartx for multipath etc.
     */
    if ((value = udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_TABLE")) &&
        strtol(value, nullptr, 10) == 1)
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
        const char* s = g_path_get_basename(device->native_path);

        unsigned int partition_count;

        partition_count = 0;

        std::string file_name;
        for (const auto& file: std::filesystem::directory_iterator(device->native_path))
        {
            file_name = std::filesystem::path(file).filename();

            if (Glib::str_has_prefix(file_name, s))
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
info_partition(device_t* device)
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
            strtol(number, nullptr, 10) > 0)
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
        uint64_t size = sysfs_get_uint64(device->native_path, "size");
        uint64_t alignment_offset = sysfs_get_uint64(device->native_path, "alignment_offset");

        device->partition_size = ztd::strdup(size * 512);
        device->partition_alignment_offset = ztd::strdup(alignment_offset);

        uint64_t offset =
            sysfs_get_uint64(device->native_path, "start") * device->device_block_size;
        device->partition_offset = ztd::strdup(offset);

        char* s = device->native_path;
        unsigned int n;
        for (n = std::strlen(s) - 1; n >= 0 && g_ascii_isdigit(s[n]); n--)
            ;
        device->partition_number = ztd::strdup(strtol(s + n + 1, nullptr, 0));
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
info_optical_disc(device_t* device)
{
    const char* optical_state = udev_device_get_property_value(device->udevice, "ID_CDROM");

    if (optical_state && strtol(optical_state, nullptr, 10) != 0)
    {
        device->device_is_optical_disc = true;

        // clang-format off
        device->optical_disc_num_tracks = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_TRACK_COUNT"));
        device->optical_disc_num_audio_tracks = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_TRACK_COUNT_AUDIO"));
        device->optical_disc_num_sessions = ztd::strdup(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_SESSION_COUNT"));

        const char* cdrom_disc_state = udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_STATE");

        device->optical_disc_is_blank = (!g_strcmp0(cdrom_disc_state, "blank"));
        device->optical_disc_is_appendable = (!g_strcmp0(cdrom_disc_state, "appendable"));
        device->optical_disc_is_closed = (!g_strcmp0(cdrom_disc_state, "complete"));
        // clang-format on
    }
    else
        device->device_is_optical_disc = false;
}

static bool
device_get_info(device_t* device)
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

static void
device_show_info(device_t* device, std::string& info)
{
    // clang-format off
    info.append(fmt::format("Showing information for {}\n", device->devnode));
    info.append(fmt::format("  native-path:                 {}\n", device->native_path));
    info.append(fmt::format("  device:                      {}:{}\n", MAJOR(device->devnum), MINOR(device->devnum)));
    info.append(fmt::format("  device-file:                 {}\n", device->devnode));
    info.append(fmt::format("    presentation:              {}\n", device->devnode));
    if (device->device_by_id)
        info.append(fmt::format("    by-id:                     {}\n", device->device_by_id));
    info.append(fmt::format("  system internal:             {}\n", device->device_is_system_internal));
    info.append(fmt::format("  removable:                   {}\n", device->device_is_removable));
    info.append(fmt::format("  has media:                   {}\n", device->device_is_media_available));
    info.append(fmt::format("  is read only:                {}\n", device->device_is_read_only));
    info.append(fmt::format("  is mounted:                  {}\n", device->device_is_mounted));
    info.append(fmt::format("  mount paths:                 {}\n", device->mount_points ? device->mount_points : ""));
    info.append(fmt::format("  presentation hide:           {}\n", device->device_presentation_hide ? device->device_presentation_hide : "0"));
    info.append(fmt::format("  presentation nopolicy:       {}\n", device->device_presentation_nopolicy ? device->device_presentation_nopolicy : "0"));
    info.append(fmt::format("  presentation name:           {}\n", device->device_presentation_name ? device->device_presentation_name : ""));
    info.append(fmt::format("  presentation icon:           {}\n", device->device_presentation_icon_name ? device->device_presentation_icon_name : ""));
    info.append(fmt::format("  automount hint:              {}\n", device->device_automount_hint ? device->device_automount_hint : ""));
    info.append(fmt::format("  size:                        {}\n", device->device_size));
    info.append(fmt::format("  block size:                  {}\n", device->device_block_size));
    info.append(fmt::format("  usage:                       {}\n", device->id_usage ? device->id_usage : ""));
    info.append(fmt::format("  type:                        {}\n", device->id_type ? device->id_type : ""));
    info.append(fmt::format("  version:                     {}\n", device->id_version ? device->id_version : ""));
    info.append(fmt::format("  uuid:                        {}\n", device->id_uuid ? device->id_uuid : ""));
    info.append(fmt::format("  label:                       {}\n", device->id_label ? device->id_label : ""));
    if (device->device_is_partition_table)
    {
        info.append(fmt::format("  partition table:\n"));
        info.append(fmt::format("    scheme:                    {}\n", device->partition_table_scheme ? device->partition_table_scheme : ""));
        info.append(fmt::format("    count:                     {}\n", device->partition_table_count ? device->partition_table_count : "0"));
    }
    if (device->device_is_partition)
    {
        info.append(fmt::format("  partition:\n"));
        info.append(fmt::format("    scheme:                    {}\n", device->partition_scheme ? device->partition_scheme : ""));
        info.append(fmt::format("    number:                    {}\n", device->partition_number ? device->partition_number : ""));
        info.append(fmt::format("    type:                      {}\n", device->partition_type ? device->partition_type : ""));
        info.append(fmt::format("    flags:                     {}\n", device->partition_flags ? device->partition_flags : ""));
        info.append(fmt::format("    offset:                    {}\n", device->partition_offset ? device->partition_offset : ""));
        info.append(fmt::format("    alignment offset:          {}\n", device->partition_alignment_offset ? device->partition_alignment_offset : ""));
        info.append(fmt::format("    size:                      {}\n", device->partition_size ? device->partition_size : ""));
        info.append(fmt::format("    label:                     {}\n", device->partition_label ? device->partition_label : ""));
        info.append(fmt::format("    uuid:                      {}\n", device->partition_uuid ? device->partition_uuid : ""));
    }
    if (device->device_is_optical_disc)
    {
        info.append(fmt::format("  optical disc:\n"));
        info.append(fmt::format("    blank:                     {}\n", device->optical_disc_is_blank));
        info.append(fmt::format("    appendable:                {}\n", device->optical_disc_is_appendable));
        info.append(fmt::format("    closed:                    {}\n", device->optical_disc_is_closed));
        info.append(fmt::format("    num tracks:                {}\n", device->optical_disc_num_tracks ? device->optical_disc_num_tracks : "0"));
        info.append(fmt::format("    num audio tracks:          {}\n", device->optical_disc_num_audio_tracks ? device->optical_disc_num_audio_tracks : "0"));
        info.append(fmt::format("    num sessions:              {}\n", device->optical_disc_num_sessions ? device->optical_disc_num_sessions : "0"));
    }
    if (device->device_is_drive)
    {
        info.append(fmt::format("  drive:\n"));
        info.append(fmt::format("    vendor:                    {}\n", device->drive_vendor ? device->drive_vendor : ""));
        info.append(fmt::format("    model:                     {}\n", device->drive_model ? device->drive_model : ""));
        info.append(fmt::format("    revision:                  {}\n", device->drive_revision ? device->drive_revision : ""));
        info.append(fmt::format("    serial:                    {}\n", device->drive_serial ? device->drive_serial : ""));
        info.append(fmt::format("    WWN:                       {}\n", device->drive_wwn ? device->drive_wwn : ""));
        info.append(fmt::format("    detachable:                {}\n", device->drive_can_detach));
        info.append(fmt::format("    ejectable:                 {}\n", device->drive_is_media_ejectable));
        info.append(fmt::format("    media:                     {}\n", device->drive_media ? device->drive_media : ""));
        info.append(fmt::format("      compat:                  {}\n", device->drive_media_compatibility ? device->drive_media_compatibility : ""));
        if (device->drive_connection_interface == nullptr || std::strlen(device->drive_connection_interface) == 0)
            info.append(fmt::format("    interface:                 (unknown)\n"));
        else
            info.append(fmt::format("    interface:                 {}\n", device->drive_connection_interface));
        if (device->drive_connection_speed == 0)
            info.append(fmt::format("    if speed:                  (unknown)\n"));
        else
            info.append(fmt::format("    if speed:                  {} bits/s\n", device->drive_connection_speed));
    }
    // clang-format on
}

/* ************************************************************************
 * udev & mount monitors
 * ************************************************************************ */

static int
cmp_devmounts(devmount_t* a, devmount_t* b)
{
    if (!a && !b)
        return 0;
    if (!a || !b)
        return 1;
    if (a->major == b->major && a->minor == b->minor)
        return 0;
    return 1;
}

static void
parse_mounts(bool report)
{
    // LOG_INFO("@@@@@@@@@@@@@ parse_mounts {}", report ? "true" : "false");
    struct udev_device* udevice;
    dev_t devnum;

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(MOUNTINFO);
    }
    catch (const Glib::FileError& e)
    {
        std::string what = e.what();
        LOG_WARN("Error reading {}: {}", MOUNTINFO, what);
        return;
    }

    // get all mount points for all devices
    GList* newmounts = nullptr;
    GList* l;
    GList* changed = nullptr;
    devmount_t* devmount;
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

    std::vector<std::string> lines = ztd::split(contents, "\n");
    for (const std::string& line: lines)
    {
        unsigned int mount_id;
        unsigned int parent_id;
        dev_t major, minor;
        char encoded_root[PATH_MAX];
        char encoded_mount_point[PATH_MAX];
        char* mount_point;

        if (line.size() == 0)
            continue;

        if (sscanf(line.c_str(),
                   "%d %d %lu:%lu %s %s",
                   &mount_id,
                   &parent_id,
                   &major,
                   &minor,
                   encoded_root,
                   encoded_mount_point) != 6)
        {
            LOG_WARN("Error reading /proc/self/mountinfo: Error parsing line '{}'", line);
            continue;
        }

        /* mount where only a subtree of a filesystem is mounted? */
        subdir_mount = (g_strcmp0(encoded_root, "/") != 0);

        if (subdir_mount)
        {
            // get mount source
            // LOG_INFO("subdir_mount {}:{} {} root={}", major, minor, encoded_mount_point,
            // encoded_root);
            char typebuf[PATH_MAX];
            char mount_source[PATH_MAX];
            const char* sep;
            sep = strstr(line.c_str(), " - ");
            if (sep && sscanf(sep + 3, "%s %s", typebuf, mount_source) == 2)
            {
                // LOG_INFO("    source={}", mount_source);
                if (Glib::str_has_prefix(mount_source, "/dev/"))
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

        mount_point = g_strcompress(encoded_mount_point);
        if (!mount_point || mount_point[0] == '\0')
        {
            free(mount_point);
            continue;
        }

        // fstype
        std::string fstype;
        if (ztd::contains(line, " - "))
            fstype = ztd::rpartition(line, " - ")[2];

        // LOG_INFO("mount_point({}:{})={}", major, minor, mount_point);
        devmount = nullptr;
        for (l = newmounts; l; l = l->next)
        {
            if ((static_cast<devmount_t*>(l->data))->major == major &&
                (static_cast<devmount_t*>(l->data))->minor == minor)
            {
                devmount = static_cast<devmount_t*>(l->data);
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
                        free(mount_point);
                        continue;
                    }
                }
                devmount = new devmount_t(major, minor);
                devmount->fstype = ztd::strdup(fstype);

                newmounts = g_list_prepend(newmounts, devmount);
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
                        free(mount_point);
                        continue;
                    }
                    // add
                    devmount = new devmount_t(major, minor);
                    devmount->fstype = ztd::strdup(fstype);

                    newmounts = g_list_prepend(newmounts, devmount);
                }
            }
        }

        if (devmount && !g_list_find(devmount->mounts, mount_point))
        {
            // LOG_INFO("    prepended");
            devmount->mounts = g_list_prepend(devmount->mounts, mount_point);
        }
        else
            free(mount_point);
    }
    // LOG_INFO("LINES DONE");
    // translate each mount points list to string
    std::string points;
    for (l = newmounts; l; l = l->next)
    {
        devmount = static_cast<devmount_t*>(l->data);
        // Sort the list to ensure that shortest mount paths appear first
        devmount->mounts = g_list_sort(devmount->mounts, (GCompareFunc)g_strcmp0);
        GList* m = devmount->mounts;
        points = ztd::strdup((char*)m->data);
        while ((m = m->next))
        {
            points = fmt::format("{}, {}", points, (char*)m->data);
        }
        g_list_foreach(devmount->mounts, (GFunc)free, nullptr);
        g_list_free(devmount->mounts);
        devmount->mounts = nullptr;
        devmount->mount_points = ztd::strdup(points);
        // LOG_INFO("translate {}:{} {}", devmount->major, devmount->minor, points);
    }

    // compare old and new lists
    GList* found;
    if (report)
    {
        for (l = newmounts; l; l = l->next)
        {
            devmount = static_cast<devmount_t*>(l->data);
            // LOG_INFO("finding {}:{}", devmount->major, devmount->minor);
            found =
                g_list_find_custom(devmounts, (const void*)devmount, (GCompareFunc)cmp_devmounts);
            if (found)
            {
                // LOG_INFO("    found");
                if (!g_strcmp0((static_cast<devmount_t*>(found->data))->mount_points,
                               devmount->mount_points))
                {
                    // LOG_INFO("    freed");
                    // no change to mount points, so remove from old list
                    devmount = static_cast<devmount_t*>(found->data);
                    devmounts = g_list_remove(devmounts, devmount);
                    delete devmount;
                }
            }
            else
            {
                // new mount
                // LOG_INFO("    new mount {}:{} {}", devmount->major, devmount->minor,
                // devmount->mount_points);
                devmount_t* devcopy = new devmount_t(devmount->major, devmount->minor);
                devcopy->mount_points = ztd::strdup(devmount->mount_points);
                devcopy->fstype = ztd::strdup(devmount->fstype);

                changed = g_list_prepend(changed, devcopy);
            }
        }
    }
    // LOG_INFO("REMAINING");
    // any remaining devices in old list have changed mount status
    for (l = devmounts; l; l = l->next)
    {
        devmount = static_cast<devmount_t*>(l->data);
        // LOG_INFO("remain {}:{}", devmount->major, devmount->minor );
        if (report)
            changed = g_list_prepend(changed, devmount);
        else
            delete devmount;
    }
    g_list_free(devmounts);
    devmounts = newmounts;

    // report
    if (report && changed)
    {
        VFSVolume* volume;
        char* devnode;
        for (l = changed; l; l = l->next)
        {
            devnode = nullptr;
            devmount = static_cast<devmount_t*>(l->data);
            devnum = makedev(devmount->major, devmount->minor);
            udevice = udev_device_new_from_devnum(udev, 'b', devnum);
            if (udevice)
                devnode = ztd::strdup(udev_device_get_devnode(udevice));
            if (devnode)
            {
                // block device
                LOG_INFO("mount changed: {}", devnode);
                if ((volume = vfs_volume_read_by_device(udevice)))
                    vfs_volume_device_added(volume, true); // frees volume if needed
                free(devnode);
            }
            else
            {
                // not a block device
                if ((volume = vfs_volume_read_by_mount(devnum, devmount->mount_points)))
                {
                    LOG_INFO("special mount changed: {} ({}:{}) on {}",
                             volume->device_file,
                             MAJOR(volume->devnum),
                             MINOR(volume->devnum),
                             devmount->mount_points);
                    vfs_volume_device_added(volume, false); // frees volume if needed
                }
                else
                    vfs_volume_nonblock_removed(devnum);
            }
            udev_device_unref(udevice);
            delete devmount;
        }
        g_list_free(changed);
    }
    // printf ( "END PARSE\n");
}

static void
free_devmounts()
{
    if (!devmounts)
        return;

    GList* l;
    for (l = devmounts; l; l = l->next)
    {
        devmount_t* devmount = static_cast<devmount_t*>(l->data);
        delete devmount;
    }
    g_list_free(devmounts);
    devmounts = nullptr;
}

static const char*
get_devmount_fstype(unsigned int major, unsigned int minor)
{
    GList* l;

    for (l = devmounts; l; l = l->next)
    {
        if ((static_cast<devmount_t*>(l->data))->major == major &&
            (static_cast<devmount_t*>(l->data))->minor == minor)
            return (static_cast<devmount_t*>(l->data))->fstype;
    }
    return nullptr;
}

static bool
cb_mount_monitor_watch(GIOChannel* channel, GIOCondition cond, void* user_data)
{
    (void)channel;
    (void)user_data;
    if (cond & ~G_IO_ERR)
        return true;

    // printf ("@@@ /proc/self/mountinfo changed\n");
    parse_mounts(true);

    return true;
}

static bool
cb_udev_monitor_watch(GIOChannel* channel, GIOCondition cond, void* user_data)
{
    (void)user_data;

    /*
    LOG_INFO("cb_monitor_watch {}", channel);
    if ( cond & G_IO_IN )
        LOG_INFO("    G_IO_IN");
    if ( cond & G_IO_OUT )
        LOG_INFO("    G_IO_OUT");
    if ( cond & G_IO_PRI )
        LOG_INFO("    G_IO_PRI");
    if ( cond & G_IO_ERR )
        LOG_INFO("    G_IO_ERR");
    if ( cond & G_IO_HUP )
        LOG_INFO("    G_IO_HUP");
    if ( cond & G_IO_NVAL )
        LOG_INFO("    G_IO_NVAL");

    if ( !( cond & G_IO_NVAL ) )
    {
        int fd = g_io_channel_unix_get_fd( channel );
        LOG_INFO("    fd={}", fd);
        if ( fcntl(fd, F_GETFL) != -1 || errno != EBADF )
        {
            int flags = g_io_channel_get_flags( channel );
            if ( flags & G_IO_FLAG_IS_READABLE )
                LOG_INFO( "    G_IO_FLAG_IS_READABLE");
        }
        else
            LOG_INFO("    Invalid FD");
    }
    */

    if ((cond & G_IO_NVAL))
    {
        LOG_WARN("udev g_io_channel_unref G_IO_NVAL");
        g_io_channel_unref(channel);
        return false;
    }
    else if (!(cond & G_IO_IN))
    {
        if ((cond & G_IO_HUP))
        {
            LOG_WARN("udev g_io_channel_unref !G_IO_IN && G_IO_HUP");
            g_io_channel_unref(channel);
            return false;
        }
        else
            return true;
    }
    else if (!(fcntl(g_io_channel_unix_get_fd(channel), F_GETFL) != -1 || errno != EBADF))
    {
        // bad file descriptor
        LOG_WARN("udev g_io_channel_unref BAD_FD");
        g_io_channel_unref(channel);
        return false;
    }

    struct udev_device* udevice;
    const char* acted = nullptr;
    VFSVolume* volume;
    if ((udevice = udev_monitor_receive_device(umonitor)))
    {
        const char* action = udev_device_get_action(udevice);
        char* devnode = ztd::strdup(udev_device_get_devnode(udevice));
        if (action)
        {
            // print action
            if (!strcmp(action, "add"))
                acted = "added:   ";
            else if (!strcmp(action, "remove"))
                acted = "removed: ";
            else if (!strcmp(action, "change"))
                acted = "changed: ";
            else if (!strcmp(action, "move"))
                acted = "moved:   ";
            if (acted)
                LOG_INFO("udev {}{}", acted, devnode);

            // add/remove volume
            if (!strcmp(action, "add") || !strcmp(action, "change"))
            {
                if ((volume = vfs_volume_read_by_device(udevice)))
                    vfs_volume_device_added(volume, true); // frees volume if needed
            }
            else if (!strcmp(action, "remove"))
                vfs_volume_device_removed(udevice);
            // what to do for move action?
        }
        free(devnode);
        udev_device_unref(udevice);
    }
    return true;
}

void
vfs_volume_set_info(VFSVolume* volume)
{
    char* lastcomma;
    std::string disp_device;
    std::string disp_label;
    std::string disp_size;
    std::string disp_mount;
    std::string disp_fstype;
    std::string disp_devnum;
    std::string disp_id;

    if (!volume)
        return;

    // set device icon
    switch (volume->device_type)
    {
        case VFSVolumeDeviceType::DEVICE_TYPE_BLOCK:
            if (volume->is_audiocd)
                volume->icon = "dev_icon_audiocd";
            else if (volume->is_optical)
            {
                if (volume->is_mounted)
                    volume->icon = "dev_icon_optical_mounted";
                else if (volume->is_mountable)
                    volume->icon = "dev_icon_optical_media";
                else
                    volume->icon = "dev_icon_optical_nomedia";
            }
            else if (volume->is_floppy)
            {
                if (volume->is_mounted)
                    volume->icon = "dev_icon_floppy_mounted";
                else
                    volume->icon = "dev_icon_floppy_unmounted";
                volume->is_mountable = true;
            }
            else if (volume->is_removable)
            {
                if (volume->is_mounted)
                    volume->icon = "dev_icon_remove_mounted";
                else
                    volume->icon = "dev_icon_remove_unmounted";
            }
            else
            {
                if (volume->is_mounted)
                {
                    if (Glib::str_has_prefix(volume->device_file, "/dev/loop"))
                        volume->icon = "dev_icon_file";
                    else
                        volume->icon = "dev_icon_internal_mounted";
                }
                else
                    volume->icon = "dev_icon_internal_unmounted";
            }
            break;
        case VFSVolumeDeviceType::DEVICE_TYPE_NETWORK:
            volume->icon = "dev_icon_network";
            break;
        case VFSVolumeDeviceType::DEVICE_TYPE_OTHER:
            volume->icon = "dev_icon_file";
            break;
        default:
            break;
    }

    // set disp_id using by-id
    switch (volume->device_type)
    {
        case VFSVolumeDeviceType::DEVICE_TYPE_BLOCK:
            if (volume->is_floppy && !volume->udi)
                disp_id = ":floppy";
            else if (volume->udi)
            {
                if ((lastcomma = strrchr(volume->udi, '/')))
                {
                    lastcomma++;
                    if (!strncmp(lastcomma, "usb-", 4))
                        lastcomma += 4;
                    else if (!strncmp(lastcomma, "ata-", 4))
                        lastcomma += 4;
                    else if (!strncmp(lastcomma, "scsi-", 5))
                        lastcomma += 5;
                }
                else
                    lastcomma = volume->udi;
                if (lastcomma[0] != '\0')
                {
                    disp_id = fmt::format(":{:.16s}", lastcomma);
                }
            }
            else if (volume->is_optical)
                disp_id = ":optical";
            // table type
            if (volume->is_table)
            {
                if (volume->fs_type && volume->fs_type[0] == '\0')
                {
                    free(volume->fs_type);
                    volume->fs_type = nullptr;
                }
                if (!volume->fs_type)
                    volume->fs_type = ztd::strdup("table");
            }
            break;
        default:
            disp_id = volume->udi;
            break;
    }

    std::string size_str;

    // set display name
    if (volume->is_mounted)
    {
        if (!volume->label.empty())
            disp_label = volume->label;
        else
            disp_label = "";

        if (volume->size > 0)
        {
            size_str = vfs_file_size_to_string_format(volume->size, false);
            disp_size = fmt::format("{}", size_str);
        }
        if (volume->mount_point && volume->mount_point[0] != '\0')
            disp_mount = fmt::format("{}", volume->mount_point);
        else
            disp_mount = "???";
    }
    else if (volume->is_mountable) // has_media
    {
        if (volume->is_blank)
            disp_label = "[blank]";
        else if (!volume->label.empty())
            disp_label = volume->label;
        else if (volume->is_audiocd)
            disp_label = "[audio]";
        else
            disp_label = "";
        if (volume->size > 0)
        {
            size_str = vfs_file_size_to_string_format(volume->size, false);
            disp_size = fmt::format("{}", size_str);
        }
        disp_mount = "---";
    }
    else
    {
        disp_label = "[no media]";
    }
    if (!strncmp(volume->device_file, "/dev/", 5))
        disp_device = ztd::strdup(volume->device_file + 5);
    else if (Glib::str_has_prefix(volume->device_file, "curlftpfs#"))
        disp_device = ztd::removeprefix(volume->device_file, "curlftpfs#");
    else
        disp_device = volume->device_file;
    if (volume->fs_type && volume->fs_type[0] != '\0')
        disp_fstype = volume->fs_type;
    disp_devnum = fmt::format("{}:{}", MAJOR(volume->devnum), MINOR(volume->devnum));

    std::string value;
    std::string parameter;
    char* fmt = xset_get_s(XSetName::DEV_DISPNAME);
    if (!fmt)
        parameter = fmt::format("{} {} {} {} {} {}",
                                disp_device,
                                disp_size,
                                disp_fstype,
                                disp_label,
                                disp_mount,
                                disp_id);
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

    // volume->disp_name = g_filename_to_utf8( parameter, -1, nullptr, nullptr, nullptr );
    while (ztd::contains(parameter, "  "))
    {
        parameter = ztd::replace(parameter, "  ", " ");
    }

    // remove leading spaces
    parameter = ztd::lstrip(parameter);

    volume->disp_name = g_filename_display_name(parameter.c_str());

    if (!volume->udi)
        volume->udi = ztd::strdup(volume->device_file);
}

static VFSVolume*
vfs_volume_read_by_device(struct udev_device* udevice)
{ // uses udev to read device parameters into returned volume
    VFSVolume* volume = nullptr;

    if (!udevice)
        return nullptr;

    device_t* device = new device_t(udevice);
    if (!device_get_info(device) || !device->devnode || device->devnum == 0 ||
        !Glib::str_has_prefix(device->devnode, "/dev/"))
    {
        delete device;
        return nullptr;
    }

    // translate device info to VFSVolume
    volume = new VFSVolume;
    volume->devnum = device->devnum;
    volume->device_type = VFSVolumeDeviceType::DEVICE_TYPE_BLOCK;
    volume->device_file = ztd::strdup(device->devnode);
    volume->udi = ztd::strdup(device->device_by_id);
    volume->should_autounmount = false;
    volume->is_optical = device->device_is_optical_disc;
    volume->is_table = device->device_is_partition_table;
    volume->is_floppy =
        (device->drive_media_compatibility && !strcmp(device->drive_media_compatibility, "floppy"));
    volume->is_removable = !device->device_is_system_internal;
    volume->requires_eject = device->drive_is_media_ejectable;
    volume->is_mountable = device->device_is_media_available;
    volume->is_audiocd = (device->device_is_optical_disc && device->optical_disc_num_audio_tracks &&
                          strtol(device->optical_disc_num_audio_tracks, nullptr, 10) > 0);
    volume->is_dvd = (device->drive_media && strstr(device->drive_media, "optical_dvd"));
    volume->is_blank = (device->device_is_optical_disc && device->optical_disc_is_blank);
    volume->is_mounted = device->device_is_mounted;
    volume->is_user_visible = device->device_presentation_hide
                                  ? !strtol(device->device_presentation_hide, nullptr, 10)
                                  : true;
    volume->ever_mounted = false;
    volume->open_main_window = nullptr;
    volume->nopolicy = device->device_presentation_nopolicy
                           ? strtol(device->device_presentation_nopolicy, nullptr, 10)
                           : false;
    volume->mount_point = nullptr;
    if (device->mount_points && device->mount_points[0] != '\0')
    {
        char* comma;
        if ((comma = strchr(device->mount_points, ',')))
        {
            comma[0] = '\0';
            volume->mount_point = ztd::strdup(device->mount_points);
            comma[0] = ',';
        }
        else
            volume->mount_point = ztd::strdup(device->mount_points);
    }
    volume->size = device->device_size;
    if (device->id_label)
        volume->label = device->id_label;
    volume->fs_type = ztd::strdup(device->id_type);
    volume->disp_name = nullptr;
    volume->automount_time = 0;
    volume->inhibit_auto = false;

    delete device;

    // adjustments
    volume->ever_mounted = volume->is_mounted;
    // if ( volume->is_blank )
    //    volume->is_mountable = false;  //has_media is now used for showing
    if (volume->is_dvd)
        volume->is_audiocd = false;

    vfs_volume_set_info(volume);
    /*
        LOG_INFO( "====devnum={}:{}", MAJOR(volume->devnum), MINOR(volume->devnum));
        LOG_INFO( "    device_file={}", volume->device_file);
        LOG_INFO( "    udi={}", volume->udi);
        LOG_INFO( "    label={}", volume->label);
        LOG_INFO( "    icon={}", volume->icon);
        LOG_INFO( "    is_mounted={}", volume->is_mounted);
        LOG_INFO( "    is_mountable={}", volume->is_mountable);
        LOG_INFO( "    is_optical={}", volume->is_optical);
        LOG_INFO( "    is_audiocd={}", volume->is_audiocd);
        LOG_INFO( "    is_blank={}", volume->is_blank);
        LOG_INFO( "    is_floppy={}", volume->is_floppy);
        LOG_INFO( "    is_table={}", volume->is_table);
        LOG_INFO( "    is_removable={}", volume->is_removable);
        LOG_INFO( "    requires_eject={}", volume->requires_eject);
        LOG_INFO( "    is_user_visible={}", volume->is_user_visible);
        LOG_INFO( "    mount_point={}", volume->mount_point);
        LOG_INFO( "    size={}", volume->size);
        LOG_INFO( "    disp_name={}", volume->disp_name);
    */
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

    std::string mtab_path = Glib::build_filename(SYSCONFDIR, "mtab");

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
            std::string what = e.what();
            LOG_WARN("Error reading {}: {}", mtab_file, what);
            return false;
        }
    }
    else
    {
        try
        {
            contents = Glib::file_get_contents(MTAB);
        }
        catch (const Glib::FileError& e)
        {
            std::string what = e.what();
            LOG_WARN("Error reading {}: {}", MTAB, what);
            return false;
        }
    }

    std::vector<std::string> lines = ztd::split(contents, "\n");
    for (const std::string& line: lines)
    {
        if (line.size() == 0)
            continue;

        if (sscanf(line.c_str(), "%s %s %s ", encoded_file, encoded_point, encoded_fstype) != 3)
        {
            LOG_WARN("Error parsing mtab line '{}'", line);
            continue;
        }

        char* point = g_strcompress(encoded_point);
        if (!g_strcmp0(point, path))
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
        values.push_back(mtab_fstype);
        values.push_back(fmt::format("mtab_fs={}", mtab_fstype));
        char** handlers = g_strsplit(handlers_list, " ", 0);
        if (handlers)
        {
            // test handlers
            std::string msg;
            XSet* set;
            int i;
            for (i = 0; handlers[i]; i++)
            {
                if (!handlers[i][0] || !(set = xset_is(handlers[i])) ||
                    set->b != XSetB::XSET_B_TRUE /* disabled */)
                    continue;
                // test whitelist
                if (g_strcmp0(set->s, "*") && ptk_handler_values_in_list(set->s, values, msg))
                {
                    found = true;
                    break;
                }
            }
            g_strfreev(handlers);
        }
    }
    return found;
}

int
split_network_url(const char* url, netmount_t** netmount)
{ // returns 0=not a network url  1=valid network url  2=invalid network url
    if (!url || !netmount)
        return 0;

    int ret;
    char* str;
    char* str2;
    char* orig_url = ztd::strdup(url);
    char* xurl = orig_url;

    netmount_t* nm = new netmount_t;
    nm->url = ztd::strdup(url);

    // get protocol
    bool is_colon;
    if (Glib::str_has_prefix(xurl, "//"))
    { // //host...
        if (xurl[2] == '\0')
        {
            delete nm;
            free(orig_url);
            return 0;
        }
        nm->fstype = ztd::strdup("smb");
        is_colon = false;
    }
    else if ((str = strstr(xurl, "://")))
    { // protocol://host...
        if (xurl[0] == ':' || xurl[0] == '/')
        {
            delete nm;
            free(orig_url);
            return 0;
        }
        str[0] = '\0';
        nm->fstype = g_strstrip(ztd::strdup(xurl));
        if (nm->fstype[0] == '\0')
        {
            delete nm;
            free(orig_url);
            return 0;
        }
        str[0] = ':';
        is_colon = true;

        // remove ...# from start of protocol
        // eg curlftpfs mtab url looks like: curlftpfs#ftp://hostname/
        if (nm->fstype && (str = strchr(nm->fstype, '#')))
        {
            str2 = nm->fstype;
            nm->fstype = ztd::strdup(str + 1);
            free(str2);
        }
    }
    else if ((str = strstr(xurl, ":/")))
    { // host:/path
        // note: sshfs also uses this URL format in mtab, but mtab_fstype == fuse.sshfs
        if (xurl[0] == ':' || xurl[0] == '/')

        {
            delete nm;
            free(orig_url);
            return 0;
        }
        nm->fstype = ztd::strdup("nfs");
        str[0] = '\0';
        nm->host = ztd::strdup(xurl);
        str[0] = ':';
        is_colon = true;
    }
    else
    {
        delete nm;
        free(orig_url);
        return 0;
    }

    ret = 1;

    // parse
    if (is_colon && (str = strchr(xurl, ':')))
    {
        xurl = str + 1;
    }
    while (xurl[0] == '/')
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
        ret = 2;
#endif

    *netmount = nm;
    return ret;
}

static VFSVolume*
vfs_volume_read_by_mount(dev_t devnum, const char* mount_points)
{ // read a non-block device
    VFSVolume* volume;
    char* str;

    if (devnum == 0 || !mount_points)
        return nullptr;

    // get single mount point
    char* point = ztd::strdup(mount_points);
    if ((str = strchr(point, ',')))
        str[0] = '\0';
    g_strstrip(point);
    if (!(point && point[0] == '/'))
        return nullptr;

    // get device name
    char* name = nullptr;
    char* mtab_fstype = nullptr;
    if (!(path_is_mounted_mtab(nullptr, point, &name, &mtab_fstype) && name && name[0] != '\0'))
        return nullptr;

    netmount_t* netmount = new netmount_t;
    if (split_network_url(name, &netmount) == 1)
    {
        // network URL
        volume = new VFSVolume;
        volume->devnum = devnum;
        volume->device_type = VFSVolumeDeviceType::DEVICE_TYPE_NETWORK;
        volume->should_autounmount = false;
        volume->udi = netmount->url;
        volume->label = netmount->host;
        volume->fs_type = mtab_fstype ? mtab_fstype : ztd::strdup("");
        volume->size = 0;
        volume->device_file = name;
        volume->is_mounted = true;
        volume->ever_mounted = true;
        volume->open_main_window = nullptr;
        volume->mount_point = point;
        volume->disp_name = nullptr;
        volume->automount_time = 0;
        volume->inhibit_auto = false;

        // free unused netmount
        delete netmount;
    }
    else if (!g_strcmp0(mtab_fstype, "fuse.fuseiso"))
    {
        // an ISO file mounted with fuseiso
        /* get device_file from ~/.mtab.fuseiso
         * hack - sleep for 0.2 seconds here because sometimes the
         * .mtab.fuseiso file is not updated until after new device is detected. */
        Glib::usleep(200000);
        std::string mtab_file = Glib::build_filename(vfs_user_home_dir(), ".mtab.fuseiso");
        char* new_name = nullptr;
        if (path_is_mounted_mtab(mtab_file.c_str(), point, &new_name, nullptr) && new_name &&
            new_name[0])
        {
            free(name);
            name = new_name;
            new_name = nullptr;
        }
        free(new_name);

        // create a volume
        volume = new VFSVolume;
        volume->devnum = devnum;
        volume->device_type = VFSVolumeDeviceType::DEVICE_TYPE_OTHER;
        volume->device_file = name;
        volume->udi = ztd::strdup(name);
        volume->should_autounmount = false;
        volume->fs_type = mtab_fstype;
        volume->size = 0;
        volume->is_mounted = true;
        volume->ever_mounted = true;
        volume->open_main_window = nullptr;
        volume->mount_point = point;
        volume->disp_name = nullptr;
        volume->automount_time = 0;
        volume->inhibit_auto = false;
    }
    else
    {
        // a non-block device is mounted - do we want to include it?
        // is a protocol handler present?
        bool keep = mtab_fstype_is_handled_by_protocol(mtab_fstype);
        if (!keep && !strstr(HIDDEN_NON_BLOCK_FS, mtab_fstype))
        {
            // no protocol handler and not blacklisted - show anyway?
            keep = Glib::str_has_prefix(point, vfs_user_cache_dir().c_str()) ||
                   Glib::str_has_prefix(point, "/media/") ||
                   Glib::str_has_prefix(point, "/run/media/") ||
                   Glib::str_has_prefix(mtab_fstype, "fuse.");
            if (!keep)
            {
                // in Auto-Mount|Mount Dirs ?
                char* mount_parent = ptk_location_view_get_mount_point_dir(nullptr);
                keep = Glib::str_has_prefix(point, mount_parent);
                free(mount_parent);
            }
        }
        // mount point must be readable
        keep = keep && (geteuid() == 0 || faccessat(0, point, R_OK, AT_EACCESS) == 0);
        if (keep)
        {
            // create a volume
            volume = new VFSVolume;
            volume->devnum = devnum;
            volume->device_type = VFSVolumeDeviceType::DEVICE_TYPE_OTHER;
            volume->device_file = name;
            volume->udi = ztd::strdup(name);
            volume->should_autounmount = false;
            volume->fs_type = mtab_fstype;
            volume->size = 0;
            volume->is_mounted = true;
            volume->ever_mounted = true;
            volume->open_main_window = nullptr;
            volume->mount_point = point;
            volume->disp_name = nullptr;
            volume->automount_time = 0;
            volume->inhibit_auto = false;
        }
        else
        {
            free(name);
            free(mtab_fstype);
            free(point);
            return nullptr;
        }
    }
    vfs_volume_set_info(volume);
    return volume;
}

char*
vfs_volume_handler_cmd(int mode, int action, VFSVolume* vol, const char* options,
                       netmount_t* netmount, bool* run_in_terminal, char** mount_point)
{
    const char* handlers_list;
    XSet* set;
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
                values.push_back(fmt::format("label={}", vol->label));
            if (vol->udi && vol->udi[0])
                values.push_back(fmt::format("id={}", vol->udi));
            values.push_back(fmt::format("audiocd={}", vol->is_audiocd ? "1" : "0"));
            values.push_back(fmt::format("optical={}", vol->is_optical ? "1" : "0"));
            values.push_back(fmt::format("removable={}", vol->is_removable ? "1" : "0"));
            values.push_back(fmt::format("mountable={}", vol->is_mountable && !vol->is_blank ? "1" : "0"));
            // change spaces in device to underscores for testing - for ISO files
            values.push_back(fmt::format("dev={}", vol->device_file));
            // clang-format on
            if (vol->fs_type)
                values.push_back(fmt::format("{}", vol->fs_type));
            break;
        case PtkHandlerMode::HANDLER_MODE_NET:
            // for VFSVolumeDeviceType::DEVICE_TYPE_NETWORK
            if (netmount)
            {
                // net values
                if (netmount->host && netmount->host[0])
                    values.push_back(fmt::format("host={}", netmount->host));
                if (netmount->user && netmount->user[0])
                    values.push_back(fmt::format("user={}", netmount->user));
                if (action != PtkHandlerMount::HANDLER_MOUNT && vol && vol->is_mounted)
                {
                    // clang-format off
                    // user-entered url (or mtab url if not available)
                    values.push_back(fmt::format("url={}", vol->udi));
                    // mtab fs type (fuse.ssh)
                    values.push_back(fmt::format("mtab_fs={}", vol->fs_type));
                    // mtab_url == url if mounted
                    values.push_back(fmt::format("mtab_url={}", vol->device_file));
                    // clang-format on
                }
                else if (netmount->url && netmount->url[0])
                    // user-entered url
                    values.push_back(fmt::format("url={}", netmount->url));
                // url-derived protocol
                if (netmount->fstype && netmount->fstype[0])
                    values.push_back(fmt::format("{}", netmount->fstype));
            }
            else
            {
                // for VFSVolumeDeviceType::DEVICE_TYPE_OTHER unmount or prop that has protocol
                // handler in mtab_fs
                if (action != PtkHandlerMount::HANDLER_MOUNT && vol && vol->is_mounted)
                {
                    // clang-format off
                    // user-entered url (or mtab url if not available)
                    values.push_back(fmt::format("url={}", vol->udi));
                    // mtab fs type (fuse.ssh)
                    values.push_back(fmt::format("mtab_fs={}", vol->fs_type));
                    // mtab_url == url if mounted
                    values.push_back(fmt::format("mtab_url={}", vol->device_file));
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
        values.push_back(fmt::format("point={}", vol->mount_point));

    // get handlers
    if (!(handlers_list =
              xset_get_s(mode == PtkHandlerMode::HANDLER_MODE_FS ? XSetName::DEV_FS_CNF
                                                                 : XSetName::DEV_NET_CNF)))
        return nullptr;
    char** handlers = g_strsplit(handlers_list, " ", 0);
    if (!handlers)
        return nullptr;

    // test handlers
    int i;
    bool found = false;
    std::string msg;
    for (i = 0; handlers[i]; i++)
    {
        if (!handlers[i][0] || !(set = xset_is(handlers[i])) ||
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
    g_strfreev(handlers);
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
            if (ztd::contains(command.c_str(), "%a"))
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
            // also used for VFSVolumeDeviceType::DEVICE_TYPE_OTHER unmount and prop
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
                std::string urlq =
                    bash_quote(action != PtkHandlerMount::HANDLER_MOUNT && vol && vol->is_mounted
                                   ? vol->udi
                                   : netmount->url);
                std::string protoq = bash_quote(netmount->fstype); // url-derived protocol (ssh)
                std::string hostq = bash_quote(netmount->host);
                std::string portq = bash_quote(netmount->port);
                std::string userq = bash_quote(netmount->user);
                std::string passq = bash_quote(netmount->pass);
                std::string pathq = bash_quote(netmount->path);
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

static bool
vfs_volume_is_automount(VFSVolume* vol)
{ // determine if volume should be automounted or auto-unmounted
    std::string test;
    char* value;

    if (vol->should_autounmount)
        // volume is a network or ISO file that was manually mounted -
        // for autounmounting only
        return true;
    if (!vol->is_mountable || vol->is_blank ||
        vol->device_type != VFSVolumeDeviceType::DEVICE_TYPE_BLOCK)
        return false;

    std::string showhidelist =
        fmt::format(" {} ", ztd::null_check(xset_get_s(XSetName::DEV_AUTOMOUNT_VOLUMES)));
    int i;
    for (i = 0; i < 3; i++)
    {
        int j;
        for (j = 0; j < 2; j++)
        {
            if (i == 0)
                value = vol->device_file;
            else if (i == 1)
                value = (char*)vol->label.c_str();
            else
            {
                value = vol->udi;
                value = strrchr(value, '/');
                if (value)
                    value++;
            }
            if (j == 0)
                test = fmt::format(" +{} ", ztd::null_check(value));
            else
                test = fmt::format(" -{} ", ztd::null_check(value));
            if (ztd::contains(showhidelist, test))
                return (j == 0);
        }
    }

    // udisks no?
    if (vol->nopolicy && !xset_get_b(XSetName::DEV_IGNORE_UDISKS_NOPOLICY))
        return false;

    // table?
    if (vol->is_table)
        return false;

    // optical
    if (vol->is_optical)
        return xset_get_b(XSetName::DEV_AUTOMOUNT_OPTICAL);

    // internal?
    if (vol->is_removable && xset_get_b(XSetName::DEV_AUTOMOUNT_REMOVABLE))
        return true;

    return false;
}

const std::string
vfs_volume_device_info(VFSVolume* vol)
{
    std::string info = "";

    struct udev_device* udevice = udev_device_new_from_devnum(udev, 'b', vol->devnum);
    if (udevice == nullptr)
    {
        LOG_WARN("No udev device for device {} ({}:{})",
                 vol->device_file,
                 MAJOR(vol->devnum),
                 MINOR(vol->devnum));
        info = "( no udev device )";
        return info;
    }

    device_t* device = new device_t(udevice);
    if (!device_get_info(device))
        return info;

    device_show_info(device, info);
    delete device;
    udev_device_unref(udevice);
    return info;
}

char*
vfs_volume_device_mount_cmd(VFSVolume* vol, const char* options, bool* run_in_terminal)
{
    char* command = nullptr;
    *run_in_terminal = false;

    command = vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_FS,
                                     PtkHandlerMount::HANDLER_MOUNT,
                                     vol,
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
            command2 = fmt::format("{} mount {} -o '{}'", path, vol->device_file, options);
        else
            command2 = fmt::format("{} mount {}", path, vol->device_file);

        return ztd::strdup(command2);
    }

    path = Glib::find_program_in_path("pmount");
    if (!path.empty())
    {
        // pmount
        command2 = fmt::format("{} {}", path, vol->device_file);

        return ztd::strdup(command2);
    }

    path = Glib::find_program_in_path("udisksctl");
    if (!path.empty())
    {
        // udisks2
        if (options && options[0] != '\0')
            command2 = fmt::format("{} mount -b {} -o '{}'", path, vol->device_file, options);
        else
            command2 = fmt::format("{} mount -b {}", path, vol->device_file);

        return ztd::strdup(command2);
    }

    return nullptr;
}

char*
vfs_volume_device_unmount_cmd(VFSVolume* vol, bool* run_in_terminal)
{
    char* command = nullptr;
    std::string pointq = "";
    *run_in_terminal = false;

    netmount_t* netmount = new netmount_t;

    // unmounting a network ?
    switch (vol->device_type)
    {
        case VFSVolumeDeviceType::DEVICE_TYPE_NETWORK:
            // is a network - try to get unmount command
            if (split_network_url(vol->udi, &netmount) == 1)
            {
                command = vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_NET,
                                                 PtkHandlerMount::HANDLER_UNMOUNT,
                                                 vol,
                                                 nullptr,
                                                 netmount,
                                                 run_in_terminal,
                                                 nullptr);
                delete netmount;

                // igtodo is this redundant?
                // replace mount point sub var
                if (command && strstr(command, "%a"))
                {
                    if (vol->is_mounted)
                        pointq = bash_quote(vol->mount_point);
                    std::string command2 = ztd::replace(command, "%a", pointq);
                    command = ztd::strdup(command2);
                }
            }
            break;

        case VFSVolumeDeviceType::DEVICE_TYPE_OTHER:
            if (mtab_fstype_is_handled_by_protocol(vol->fs_type))
            {
                command = vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_NET,
                                                 PtkHandlerMount::HANDLER_UNMOUNT,
                                                 vol,
                                                 nullptr,
                                                 nullptr,
                                                 run_in_terminal,
                                                 nullptr);
                // igtodo is this redundant?
                // replace mount point sub var
                if (command && strstr(command, "%a"))
                {
                    if (vol->is_mounted)
                        pointq = bash_quote(vol->mount_point);
                    std::string command2 = ztd::replace(command, "%a", pointq);
                    command = ztd::strdup(command2);
                }
            }
            break;
        default:
            command = vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_FS,
                                             PtkHandlerMount::HANDLER_UNMOUNT,
                                             vol,
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

    pointq =
        bash_quote(vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_BLOCK || !vol->is_mounted
                       ? vol->device_file
                       : vol->mount_point);

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

char*
vfs_volume_get_mount_options(VFSVolume* vol, char* options)
{
    if (!options)
        return nullptr;
    // change spaces to commas
    bool leading = true;
    bool trailing = false;
    int j = -1;
    char news[16384];
    for (unsigned int i = 0; i < std::strlen(options); i++)
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
    std::string opts = fmt::format(",{},", news);
    const char* fstype = vfs_volume_get_fstype(vol);
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

char*
vfs_volume_get_mount_command(VFSVolume* vol, char* default_options, bool* run_in_terminal)
{
    char* options = vfs_volume_get_mount_options(vol, default_options);
    char* command = vfs_volume_device_mount_cmd(vol, options, run_in_terminal);
    free(options);
    return command;
}

static void
exec_task(const char* command, bool run_in_terminal)
{ // run command as async task with optional terminal
    if (!(command && command[0]))
        return;

    std::vector<std::string> file_list;
    file_list.push_back("exec_task");

    PtkFileTask* ptask =
        new PtkFileTask(VFSFileTaskType::VFS_FILE_TASK_EXEC, file_list, "/", nullptr, nullptr);
    ptask->task->exec_action = "exec_task";
    ptask->task->exec_command = command;
    ptask->task->exec_sync = false;
    ptask->task->exec_export = false;
    ptask->task->exec_terminal = run_in_terminal;
    ptk_file_task_run(ptask);
}

static void
vfs_volume_exec(VFSVolume* vol, const char* command)
{
    // LOG_INFO("vfs_volume_exec {} {}", vol->device_file, command);
    if (!(command && command[0]) || vol->device_type != VFSVolumeDeviceType::DEVICE_TYPE_BLOCK)
        return;

    std::string cmd = command;

    std::string quoted_mount = bash_quote(vol->mount_point);
    std::string quoted_label = bash_quote(vol->label);
    cmd = ztd::replace(cmd, "%m", quoted_mount);
    cmd = ztd::replace(cmd, "%l", quoted_label);
    cmd = ztd::replace(cmd, "%v", vol->device_file);

    LOG_INFO("Autoexec: {}", cmd);
    exec_task(cmd.c_str(), false);
}

static void
vfs_volume_autoexec(VFSVolume* vol)
{
    char* command = nullptr;

    // Note: audiocd is is_mountable
    if (!vol->is_mountable || global_inhibit_auto ||
        vol->device_type != VFSVolumeDeviceType::DEVICE_TYPE_BLOCK || !vfs_volume_is_automount(vol))
        return;

    if (vol->is_audiocd)
    {
        command = xset_get_s(XSetName::DEV_EXEC_AUDIO);
    }
    else if (vol->is_mounted && vol->mount_point && vol->mount_point[0] != '\0')
    {
        if (vol->inhibit_auto)
        {
            // user manually mounted this vol, so no autoexec this time
            vol->inhibit_auto = false;
            return;
        }
        else
        {
            char* path = g_build_filename(vol->mount_point, "VIDEO_TS", nullptr);
            if (vol->is_dvd && std::filesystem::is_directory(path))
                command = xset_get_s(XSetName::DEV_EXEC_VIDEO);
            else
            {
                if (xset_get_b(XSetName::DEV_AUTO_OPEN))
                {
                    FMMainWindow* main_window = fm_main_window_get_last_active();
                    if (main_window)
                    {
                        LOG_INFO("Auto Open Tab for {} in {}", vol->device_file, vol->mount_point);
                        // PtkFileBrowser* file_browser =
                        //        (PtkFileBrowser*)fm_main_window_get_current_file_browser(
                        //                                                main_window );
                        // if ( file_browser )
                        //    ptk_file_browser_emit_open( file_browser, vol->mount_point,
                        //                                        PtkOpenAction::PTK_OPEN_DIR );
                        //                                        //PtkOpenAction::PTK_OPEN_NEW_TAB
                        // fm_main_window_add_new_tab causes hang without GDK_THREADS_ENTER
                        fm_main_window_add_new_tab(main_window, vol->mount_point);
                        // LOG_INFO("DONE Auto Open Tab for {} in {}", vol->device_file,
                        //                                             vol->mount_point);
                    }
                    else
                    {
                        std::string exe = get_prog_executable();
                        std::string quote_path = bash_quote(vol->mount_point);
                        std::string cmd = fmt::format("{} -t {}", exe, quote_path);
                        print_command(cmd);
                        Glib::spawn_command_line_async(cmd);
                    }
                }
                command = xset_get_s(XSetName::DEV_EXEC_FS);
            }
            free(path);
        }
    }
    vfs_volume_exec(vol, command);
}

static void
vfs_volume_autounmount(VFSVolume* vol)
{
    if (!vol->is_mounted || !vfs_volume_is_automount(vol))
        return;
    bool run_in_terminal;
    char* line = vfs_volume_device_unmount_cmd(vol, &run_in_terminal);
    if (line)
    {
        LOG_INFO("Auto-Unmount: {}", line);
        exec_task(line, run_in_terminal);
        free(line);
    }
    else
        LOG_INFO("Auto-Unmount: error: no unmount command available");
}

void
vfs_volume_automount(VFSVolume* vol)
{
    if (vol->is_mounted || vol->ever_mounted || vol->is_audiocd || vol->should_autounmount ||
        !vfs_volume_is_automount(vol))
        return;

    if (vol->automount_time && std::time(nullptr) - vol->automount_time < 5)
        return;
    vol->automount_time = std::time(nullptr);

    bool run_in_terminal;
    char* line = vfs_volume_get_mount_command(vol,
                                              xset_get_s(XSetName::DEV_MOUNT_OPTIONS),
                                              &run_in_terminal);
    if (line)
    {
        LOG_INFO("Automount: {}", line);
        exec_task(line, run_in_terminal);
        free(line);
    }
    else
        LOG_INFO("Automount: error: no mount command available");
}

static void
vfs_volume_device_added(VFSVolume* volume, bool automount)
{ // frees volume if needed
    char* changed_mount_point = nullptr;

    if (!volume || !volume->udi || !volume->device_file)
        return;

    // check if we already have this volume device file
    for (VFSVolume* volume2: volumes)
    {
        if (volume2->devnum == volume->devnum)
        {
            // update existing volume
            bool was_mounted = volume2->is_mounted;
            bool was_audiocd = volume2->is_audiocd;
            bool was_mountable = volume2->is_mountable;

            // detect changed mount point
            if (!was_mounted && volume->is_mounted)
                changed_mount_point = ztd::strdup(volume->mount_point);
            else if (was_mounted && !volume->is_mounted)
                changed_mount_point = ztd::strdup(volume2->mount_point);

            volume2->udi = ztd::strdup(volume->udi);
            volume2->device_file = ztd::strdup(volume->device_file);
            volume2->label = volume->label;
            volume2->mount_point = ztd::strdup(volume->mount_point);
            volume2->icon = volume->icon;
            volume2->disp_name = ztd::strdup(volume->disp_name);
            volume2->is_mounted = volume->is_mounted;
            volume2->is_mountable = volume->is_mountable;
            volume2->is_optical = volume->is_optical;
            volume2->requires_eject = volume->requires_eject;
            volume2->is_removable = volume->is_removable;
            volume2->is_user_visible = volume->is_user_visible;
            volume2->size = volume->size;
            volume2->is_table = volume->is_table;
            volume2->is_floppy = volume->is_floppy;
            volume2->nopolicy = volume->nopolicy;
            volume2->fs_type = volume->fs_type;
            volume2->is_blank = volume->is_blank;
            volume2->is_audiocd = volume->is_audiocd;
            volume2->is_dvd = volume->is_dvd;

            // Mount and ejection detect for automount
            if (volume->is_mounted)
            {
                volume2->ever_mounted = true;
                volume2->automount_time = 0;
            }
            else
            {
                if (volume->is_removable && !volume->is_mountable) // ejected
                {
                    volume2->ever_mounted = false;
                    volume2->automount_time = 0;
                    volume2->inhibit_auto = false;
                }
            }

            call_callbacks(volume2, VFSVolumeState::VFS_VOLUME_CHANGED);

            delete volume;

            if (automount)
            {
                vfs_volume_automount(volume);
                if (!was_mounted && volume->is_mounted)
                    vfs_volume_autoexec(volume);
                else if (was_mounted && !volume->is_mounted)
                {
                    vfs_volume_exec(volume, xset_get_s(XSetName::DEV_EXEC_UNMOUNT));
                    volume->should_autounmount = false;
                    // remove mount points in case other unmounted
                    ptk_location_view_clean_mount_points();
                }
                else if (!was_audiocd && volume->is_audiocd)
                    vfs_volume_autoexec(volume);

                // media inserted ?
                if (!was_mountable && volume->is_mountable)
                    vfs_volume_exec(volume, xset_get_s(XSetName::DEV_EXEC_INSERT));

                // media ejected ?
                if (was_mountable && !volume->is_mountable && volume->is_mounted &&
                    (volume->is_optical || volume->is_removable))
                    unmount_if_mounted(volume);
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
    volumes.push_back(volume);
    call_callbacks(volume, VFSVolumeState::VFS_VOLUME_ADDED);
    if (automount)
    {
        vfs_volume_automount(volume);
        vfs_volume_exec(volume, xset_get_s(XSetName::DEV_EXEC_INSERT));
        if (volume->is_audiocd)
            vfs_volume_autoexec(volume);
    }
    // refresh tabs containing changed mount point
    if (volume->is_mounted && volume->mount_point)
        main_window_refresh_all_tabs_matching(volume->mount_point);
}

static bool
vfs_volume_nonblock_removed(dev_t devnum)
{
    for (VFSVolume* volume: volumes)
    {
        if (volume->device_type != VFSVolumeDeviceType::DEVICE_TYPE_BLOCK &&
            volume->devnum == devnum)
        {
            // remove volume
            LOG_INFO("special mount removed: {} ({}:{}) on {}",
                     volume->device_file,
                     MAJOR(volume->devnum),
                     MINOR(volume->devnum),
                     volume->mount_point);
            volumes.erase(std::remove(volumes.begin(), volumes.end(), volume), volumes.end());
            call_callbacks(volume, VFSVolumeState::VFS_VOLUME_REMOVED);
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

    for (VFSVolume* volume: volumes)
    {
        if (volume->device_type == VFSVolumeDeviceType::DEVICE_TYPE_BLOCK &&
            volume->devnum == devnum)
        {
            // remove volume
            // LOG_INFO("remove volume {}", volume->device_file);
            vfs_volume_exec(volume, xset_get_s(XSetName::DEV_EXEC_REMOVE));
            if (volume->is_mounted && volume->is_removable)
                unmount_if_mounted(volume);
            volumes.erase(std::remove(volumes.begin(), volumes.end(), volume), volumes.end());
            call_callbacks(volume, VFSVolumeState::VFS_VOLUME_REMOVED);
            if (volume->is_mounted && volume->mount_point)
                main_window_refresh_all_tabs_matching(volume->mount_point);
            delete volume;
            break;
        }
    }
    ptk_location_view_clean_mount_points();
}

static void
unmount_if_mounted(VFSVolume* vol)
{
    if (!vol->device_file)
        return;
    bool run_in_terminal;
    char* str = vfs_volume_device_unmount_cmd(vol, &run_in_terminal);
    if (!str)
        return;

    char* mtab_path = g_build_filename(SYSCONFDIR, "mtab", nullptr);

    const char* mtab = MTAB;
    if (!std::filesystem::exists(mtab))
        mtab = mtab_path;

    std::string line =
        fmt::format("grep -qs '^{} ' {} 2>/dev/nullptr || exit\n{}\n", vol->device_file, mtab, str);
    free(str);
    free(mtab_path);
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
                VFSVolume* volume = vfs_volume_read_by_device(udevice);
                if (volume)
                    vfs_volume_device_added(volume, false); // frees volume if needed
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

    int ufd;
    ufd = udev_monitor_get_fd(umonitor);
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

    uchannel = g_io_channel_unix_new(ufd);
    g_io_channel_set_flags(uchannel, G_IO_FLAG_NONBLOCK, nullptr);
    g_io_channel_set_close_on_unref(uchannel, true);
    g_io_add_watch(uchannel,
                   GIOCondition(G_IO_IN | G_IO_HUP), // | G_IO_NVAL | G_IO_ERR,
                   (GIOFunc)cb_udev_monitor_watch,
                   nullptr);

    // start mount monitor
    GError* error;
    error = nullptr;
    mchannel = g_io_channel_new_file(MOUNTINFO, "r", &error);
    if (mchannel != nullptr)
    {
        g_io_channel_set_close_on_unref(mchannel, true);
        g_io_add_watch(mchannel, G_IO_ERR, (GIOFunc)cb_mount_monitor_watch, nullptr);
    }
    else
    {
        free_devmounts();
        LOG_INFO("error monitoring {}: {}", MOUNTINFO, error->message);
        g_error_free(error);
    }

    // do startup automounts
    for (VFSVolume* volume: volumes)
        vfs_volume_automount(volume);

    // start resume autoexec timer
    g_timeout_add_seconds(3, (GSourceFunc)on_cancel_inhibit_timer, nullptr);

    return true;
}

void
vfs_volume_finalize()
{
    // stop mount monitor
    if (mchannel)
    {
        g_io_channel_unref(mchannel);
        mchannel = nullptr;
    }
    free_devmounts();

    // stop udev monitor
    if (uchannel)
    {
        g_io_channel_unref(uchannel);
        uchannel = nullptr;
    }
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
    if (callbacks)
        g_array_free(callbacks, true);

    // free volumes / unmount all ?
    bool unmount_all = xset_get_b(XSetName::DEV_UNMOUNT_QUIT);
    while (true)
    {
        if (volumes.empty())
            break;

        VFSVolume* volume = volumes.back();
        volumes.pop_back();

        if (unmount_all)
            vfs_volume_autounmount(volume);

        delete volume;
    }

    // remove unused mount points
    ptk_location_view_clean_mount_points();
}

const std::vector<VFSVolume*>
vfs_volume_get_all_volumes()
{
    return volumes;
}

VFSVolume*
vfs_volume_get_by_device_or_point(const char* device_file, const char* point)
{
    if (!point && !device_file)
        return nullptr;

    // canonicalize point
    char buf[PATH_MAX + 1];
    char* canon = nullptr;
    if (point)
    {
        canon = realpath(point, buf);
        if (canon && !strcmp(canon, point))
            canon = nullptr;
    }

    if (!volumes.empty())
    {
        for (VFSVolume* volume: volumes)
        {
            if (device_file && !strcmp(device_file, volume->device_file))
                return volume;
            if (point && volume->is_mounted && volume->mount_point)
            {
                if (!strcmp(point, volume->mount_point) ||
                    (canon && !strcmp(canon, volume->mount_point)))
                    return volume;
            }
        }
    }
    return nullptr;
}

static void
call_callbacks(VFSVolume* vol, VFSVolumeState state)
{
    if (callbacks)
    {
        VFSVolumeCallbackData* e = VFS_VOLUME_CALLBACK_DATA(callbacks->data);
        for (unsigned int i = 0; i < callbacks->len; ++i)
            (*e[i].cb)(vol, state, e[i].user_data);
    }

    if (event_handler.device->s || event_handler.device->ob2_data)
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

    if (!callbacks)
        callbacks = g_array_sized_new(false, false, sizeof(VFSVolumeCallbackData), 8);

    VFSVolumeCallbackData e;
    e.cb = cb;
    e.user_data = user_data;
    callbacks = g_array_append_val(callbacks, e);
}

void
vfs_volume_remove_callback(VFSVolumeCallback cb, void* user_data)
{
    if (!callbacks)
        return;

    VFSVolumeCallbackData* e = VFS_VOLUME_CALLBACK_DATA(callbacks->data);
    for (unsigned int i = 0; i < callbacks->len; ++i)
    {
        if (e[i].cb == cb && e[i].user_data == user_data)
        {
            callbacks = g_array_remove_index_fast(callbacks, i);
            if (callbacks->len > 8)
                g_array_set_size(callbacks, 8);
            break;
        }
    }
}

const char*
vfs_volume_get_disp_name(VFSVolume* vol)
{
    return vol->disp_name;
}

const char*
vfs_volume_get_mount_point(VFSVolume* vol)
{
    return vol->mount_point;
}

const char*
vfs_volume_get_device(VFSVolume* vol)
{
    return vol->device_file;
}
const char*
vfs_volume_get_fstype(VFSVolume* vol)
{
    return vol->fs_type;
}

const char*
vfs_volume_get_icon(VFSVolume* vol)
{
    if (vol->icon.empty())
        return nullptr;
    XSet* set = xset_get(vol->icon);
    return set->icon;
}

bool
vfs_volume_is_mounted(VFSVolume* vol)
{
    return vol->is_mounted;
}

bool
vfs_volume_dir_avoid_changes(const char* dir)
{
    // determines if file change detection should be disabled for this
    // dir (eg nfs stat calls block when a write is in progress so file
    // change detection is unwanted)
    // return false to detect changes in this dir, true to avoid change detection
    const char* devnode;
    bool ret;

    // LOG_INFO("vfs_volume_dir_avoid_changes( {} )", dir);
    if (!udev || !dir)
        return false;

    // canonicalize path
    char buf[PATH_MAX + 1];
    char* canon = realpath(dir, buf);
    if (!canon)
        return false;

    // get devnum
    struct stat stat_buf; // skip stat
    if (stat(canon, &stat_buf) == -1)
        return false;
    // LOG_INFO("    stat_buf.st_dev = {}:{}", major(stat_buf.st_dev), minor(stat_buf.st_dev));

    struct udev_device* udevice = udev_device_new_from_devnum(udev, 'b', stat_buf.st_dev);
    if (udevice)
        devnode = udev_device_get_devnode(udevice);
    else
        devnode = nullptr;

    if (devnode == nullptr)
    {
        // not a block device
        const char* fstype = get_devmount_fstype(major(stat_buf.st_dev), minor(stat_buf.st_dev));
        // LOG_INFO("    !udevice || !devnode  fstype={}", fstype);
        ret = false;
        if (fstype && fstype[0])
        {
            // fstype listed in change detection blacklist?
            int len = std::strlen(fstype);
            char* ptr;
            if ((ptr = xset_get_s(XSetName::DEV_CHANGE)))
            {
                while (ptr[0])
                {
                    while (ptr[0] == ' ' || ptr[0] == ',')
                        ptr++;
                    if (Glib::str_has_prefix(ptr, fstype) &&
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
        /* blacklist these types for no change detection (if not block device)
        ret = (    Glib::str_has_prefix( fstype, "nfs" )    // nfs nfs4 etc
                || ( Glib::str_has_prefix( fstype, "fuse" ) &&
                            strcmp( fstype, "fuseblk" ) ) // fuse fuse.sshfs curlftpfs(fuse) etc
                || strstr( fstype, "cifs" )
                || !strcmp( fstype, "smbfs" )
                || !strcmp( fstype, "ftpfs" ) );
        // whitelist
                !g_strcmp0( fstype, "tmpfs" ) || !g_strcmp0( fstype, "ramfs" )
               || !g_strcmp0( fstype, "aufs" )  || !g_strcmp0( fstype, "devtmpfs" )
               || !g_strcmp0( fstype, "overlayfs" ) );
        */
    }
    else
        // block device
        ret = false;

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

    char* parent = g_path_get_dirname(native_path);
    free(native_path);
    udevice = udev_device_new_from_syspath(udev, parent);
    free(parent);
    if (!udevice)
        return 0;
    dev_t retdev = udev_device_get_devnum(udevice);
    udev_device_unref(udevice);
    return retdev;
}
