/*
 * SpaceFM vfs-volume-nohal.c
 *
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 *
 * License: See COPYING file
 *
 * udev & mount monitor code by IgnorantGuru
 * device info code uses code excerpts from freedesktop's udisks v1.0.4
 */

#include <string>
#include <filesystem>

#include <ctime>

#include <libudev.h>
#include <fcntl.h>
#include <linux/kdev_t.h>

#include "main-window.hxx"

#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-location-view.hxx"

#include "logger.hxx"
#include "utils.hxx"

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

struct VFSVolumeCallbackData
{
    VFSVolumeCallback cb;
    void* user_data;
};

static GList* volumes = nullptr;
static GArray* callbacks = nullptr;
static bool global_inhibit_auto = false;

struct devmount_t
{
    unsigned int major;
    unsigned int minor;
    char* mount_points;
    char* fstype;
    GList* mounts;
};

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

static char*
_dupv8(const char* s)
{
    const char* end_valid;

    if (!g_utf8_validate(s, -1, &end_valid))
    {
        LOG_INFO("The string '{}' is not valid UTF-8", s);
        LOG_INFO("Invalid characters begins at '{}'", end_valid);
        return g_strndup(s, end_valid - s);
    }
    else
    {
        return g_strdup(s);
    }
}

/* unescapes things like \x20 to " " and ensures the returned string is valid UTF-8.
 *
 * see volume_id_encode_string() in extras/volume_id/lib/volume_id.c in the
 * udev tree for the encoder
 */
static char*
decode_udev_encoded_string(const char* str)
{
    GString* s = g_string_new(nullptr);
    unsigned int n;
    for (n = 0; str[n] != '\0'; n++)
    {
        if (str[n] == '\\')
        {
            int val;

            if (str[n + 1] != 'x' || str[n + 2] == '\0' || str[n + 3] == '\0')
            {
                LOG_INFO("malformed encoded string '{}'", str);
                break;
            }

            val = (g_ascii_xdigit_value(str[n + 2]) << 4) | g_ascii_xdigit_value(str[n + 3]);

            g_string_append_c(s, val);

            n += 3;
        }
        else
        {
            g_string_append_c(s, str[n]);
        }
    }

    char* ret;
    const char* end_valid;
    if (!g_utf8_validate(s->str, -1, &end_valid))
    {
        LOG_INFO("The string '{}' is not valid UTF-8", s->str);
        LOG_INFO("Invalid characters begins at '{}'", end_valid);
        ret = g_strndup(s->str, end_valid - s->str);
        g_string_free(s, true);
    }
    else
    {
        ret = g_string_free(s, false);
    }

    return ret;
}

static int
ptr_str_array_compare(const char** a, const char** b)
{
    return g_strcmp0(*a, *b);
}

static double
sysfs_get_double(const char* dir, const char* attribute)
{
    char* contents;
    double result = 0.0;
    char* filename = g_build_filename(dir, attribute, nullptr);
    if (g_file_get_contents(filename, &contents, nullptr, nullptr))
    {
        result = atof(contents);
        g_free(contents);
    }
    g_free(filename);

    return result;
}

static char*
sysfs_get_string(const char* dir, const char* attribute)
{
    char* result = nullptr;
    char* filename = g_build_filename(dir, attribute, nullptr);
    if (!g_file_get_contents(filename, &result, nullptr, nullptr))
    {
        result = g_strdup("");
    }
    g_free(filename);

    return result;
}

static int
sysfs_get_int(const char* dir, const char* attribute)
{
    int result = 0;
    char* contents;
    char* filename = g_build_filename(dir, attribute, nullptr);
    if (g_file_get_contents(filename, &contents, nullptr, nullptr))
    {
        result = strtol(contents, nullptr, 0);
        g_free(contents);
    }
    g_free(filename);

    return result;
}

static uint64_t
sysfs_get_uint64(const char* dir, const char* attribute)
{
    char* contents;
    uint64_t result = 0;
    char* filename = g_build_filename(dir, attribute, nullptr);
    if (g_file_get_contents(filename, &contents, nullptr, nullptr))
    {
        result = strtoll(contents, nullptr, 0);
        g_free(contents);
    }
    g_free(filename);

    return result;
}

static bool
sysfs_file_exists(const char* dir, const char* attribute)
{
    bool result = false;
    char* filename = g_build_filename(dir, attribute, nullptr);
    if (std::filesystem::exists(filename))
    {
        result = true;
    }
    g_free(filename);

    return result;
}

static char*
sysfs_resolve_link(const char* sysfs_path, const char* name)
{
    char link_path[PATH_MAX];
    char resolved_path[PATH_MAX];
    bool found_it = false;

    char* full_path = g_build_filename(sysfs_path, name, nullptr);

    // g_debug ("name='%s'", name);
    // g_debug ("full_path='%s'", full_path);
    ssize_t num = readlink(full_path, link_path, sizeof(link_path) - 1);
    if (num != -1)
    {
        char* absolute_path;

        link_path[num] = '\0';

        // g_debug ("link_path='%s'", link_path);
        absolute_path = g_build_filename(sysfs_path, link_path, nullptr);
        // g_debug ("absolute_path='%s'", absolute_path);
        if (realpath(absolute_path, resolved_path) != nullptr)
        {
            // g_debug ("resolved_path='%s'", resolved_path);
            found_it = true;
        }
        g_free(absolute_path);
    }
    g_free(full_path);

    if (found_it)
        return g_strdup(resolved_path);
    else
        return nullptr;
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
    char* s = g_strdup(device->native_path);
    do
    {
        char* model;
        char* p = sysfs_resolve_link(s, "subsystem");
        if (!device->device_is_removable && sysfs_get_int(s, "removable") != 0)
            device->device_is_removable = true;
        if (p != nullptr)
        {
            char* subsystem = g_path_get_basename(p);
            g_free(p);

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
                    /* Don't overwrite what we set earlier from ID_VENDOR */
                    if (device->drive_vendor == nullptr)
                    {
                        device->drive_vendor = _dupv8(vendor);
                    }
                    g_free(vendor);
                }

                model = sysfs_get_string(s, "model");
                if (model != nullptr)
                {
                    g_strstrip(model);
                    /* Don't overwrite what we set earlier from ID_MODEL */
                    if (device->drive_model == nullptr)
                    {
                        device->drive_model = _dupv8(model);
                    }
                    g_free(model);
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
                    /* Don't overwrite what we set earlier from ID_MODEL */
                    if (device->drive_model == nullptr)
                    {
                        device->drive_model = _dupv8(model);
                    }
                    g_free(model);
                }

                char* serial = sysfs_get_string(s, "serial");
                if (serial != nullptr)
                {
                    g_strstrip(serial);
                    /* Don't overwrite what we set earlier from ID_SERIAL */
                    if (device->drive_serial == nullptr)
                    {
                        /* this is formatted as a hexnumber; drop the leading 0x */
                        device->drive_serial = _dupv8(serial + 2);
                    }
                    g_free(serial);
                }

                /* TODO: use hwrev and fwrev files? */
                char* revision = sysfs_get_string(s, "date");
                if (revision != nullptr)
                {
                    g_strstrip(revision);
                    /* Don't overwrite what we set earlier from ID_REVISION */
                    if (device->drive_revision == nullptr)
                    {
                        device->drive_revision = _dupv8(revision);
                    }
                    g_free(revision);
                }

                /* TODO: interface speed; the kernel driver knows; would be nice
                 * if it could export it */
            }
            else if (!strcmp(subsystem, "platform"))
            {
                const char* sysfs_name;

                sysfs_name = g_strrstr(s, "/");
                if (g_str_has_prefix(sysfs_name + 1, "floppy.") && device->drive_vendor == nullptr)
                {
                    device->drive_vendor = g_strdup("Floppy Drive");
                    connection_interface = "platform";
                }
            }

            g_free(subsystem);
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
        device->drive_connection_interface = g_strdup(connection_interface);
        device->drive_connection_speed = connection_speed;
    }

    g_free(s);
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
    char* decoded_string;
    unsigned int n;
    const char* value;

    // drive identification
    device->device_is_drive = sysfs_file_exists(device->native_path, "range");

    // vendor
    if ((value = udev_device_get_property_value(device->udevice, "ID_VENDOR_ENC")))
    {
        decoded_string = decode_udev_encoded_string(value);
        g_strstrip(decoded_string);
        device->drive_vendor = decoded_string;
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_VENDOR")))
    {
        device->drive_vendor = g_strdup(value);
    }

    // model
    if ((value = udev_device_get_property_value(device->udevice, "ID_MODEL_ENC")))
    {
        decoded_string = decode_udev_encoded_string(value);
        g_strstrip(decoded_string);
        device->drive_model = decoded_string;
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_MODEL")))
    {
        device->drive_model = g_strdup(value);
    }

    // revision
    device->drive_revision =
        g_strdup(udev_device_get_property_value(device->udevice, "ID_REVISION"));

    // serial
    if ((value = udev_device_get_property_value(device->udevice, "ID_SCSI_SERIAL")))
    {
        /* scsi_id sometimes use the WWN as the serial - annoying - see
         * http://git.kernel.org/?p=linux/hotplug/udev.git;a=commit;h=4e9fdfccbdd16f0cfdb5c8fa8484a8ba0f2e69d3
         * for details
         */
        device->drive_serial = g_strdup(value);
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_SERIAL_SHORT")))
    {
        device->drive_serial = g_strdup(value);
    }

    // wwn
    if ((value = udev_device_get_property_value(device->udevice, "ID_WWN_WITH_EXTENSION")))
    {
        device->drive_wwn = g_strdup(value + 2);
    }
    else if ((value = udev_device_get_property_value(device->udevice, "ID_WWN")))
    {
        device->drive_wwn = g_strdup(value + 2);
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
    /* special handling for SDIO since we don't yet have a sdio_id helper in udev to set properties
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
        g_free(type);
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
            // should this be media_mapping[n] ?  doesn't matter, same?
            media_in_drive = drive_media_mapping[n].media_name;
            break;
        }
        /* If the media isn't set (from e.g. udev rules), just pick the first one in media_compat -
         * note that this may be nullptr (if we don't know what media is compatible with the drive)
         * which is OK.
         */
        if (media_in_drive == nullptr)
            media_in_drive = ((const char**)media_compat_array->pdata)[0];
    }
    device->drive_media = g_strdup(media_in_drive);
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

    device->native_path = g_strdup(udev_device_get_syspath(device->udevice));
    device->devnode = g_strdup(udev_device_get_devnode(device->udevice));
    device->devnum = udev_device_get_devnum(device->udevice);
    // device->major = g_strdup( udev_device_get_property_value( device->udevice, "MAJOR") );
    // device->minor = g_strdup( udev_device_get_property_value( device->udevice, "MINOR") );
    if (!device->native_path || !device->devnode || device->devnum == 0)
    {
        if (device->native_path)
            g_free(device->native_path);
        device->native_path = nullptr;
        device->devnum = 0;
        return;
    }

    // by id - would need to read symlinks in /dev/disk/by-id

    // is_removable may also be set in info_drive_connection walking up sys tree
    // clang-format off
    device->device_is_removable = sysfs_get_int(device->native_path, "removable");

    device->device_presentation_hide = g_strdup(udev_device_get_property_value(device->udevice, "UDISKS_PRESENTATION_HIDE"));
    device->device_presentation_nopolicy = g_strdup(udev_device_get_property_value(device->udevice, "UDISKS_PRESENTATION_NOPOLICY"));
    device->device_presentation_name = g_strdup(udev_device_get_property_value(device->udevice, "UDISKS_PRESENTATION_NAME"));
    device->device_presentation_icon_name = g_strdup(udev_device_get_property_value(device->udevice, "UDISKS_PRESENTATION_ICON_NAME"));
    device->device_automount_hint = g_strdup(udev_device_get_property_value(device->udevice, "UDISKS_AUTOMOUNT_HINT"));
    // clang-format on

    // filesystem properties
    char* decoded_string;
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
        device->id_usage = g_strdup(udev_device_get_property_value(device->udevice, "ID_FS_USAGE"));
        device->id_type = g_strdup(udev_device_get_property_value(device->udevice, "ID_FS_TYPE"));
        device->id_version = g_strdup(udev_device_get_property_value(device->udevice, "ID_FS_VERSION"));
        device->id_uuid = g_strdup(udev_device_get_property_value(device->udevice, "ID_FS_UUID"));
        // clang-format on

        if ((value = udev_device_get_property_value(device->udevice, "ID_FS_LABEL_ENC")))
        {
            decoded_string = decode_udev_encoded_string(value);
            g_strstrip(decoded_string);
            device->id_label = decoded_string;
        }
        else if ((value = udev_device_get_property_value(device->udevice, "ID_FS_LABEL")))
        {
            device->id_label = g_strdup(value);
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
    else if (g_str_has_prefix(device->devnode, "/dev/loop"))
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
         * Another way to get this information is the BLKSSZGET ioctl but we don't want
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
        if (entry_name && (g_str_has_prefix(entry_name, "/dev/disk/by-id/") ||
                           g_str_has_prefix(entry_name, "/dev/disk/by-uuid/")))
        {
            device->device_by_id = g_strdup(entry_name);
            break;
        }
        entry = udev_list_entry_get_next(entry);
    }
}

static char*
info_mount_points(device_t* device)
{
    GList* mounts = nullptr;

    unsigned int dmajor = MAJOR(device->devnum);
    unsigned int dminor = MINOR(device->devnum);

    // if we have the mount point list, use this instead of reading mountinfo
    if (devmounts)
    {
        GList* l;
        for (l = devmounts; l; l = l->next)
        {
            if ((static_cast<devmount_t*>(l->data))->major == dmajor &&
                (static_cast<devmount_t*>(l->data))->minor == dminor)
            {
                return g_strdup((static_cast<devmount_t*>(l->data))->mount_points);
            }
        }
        return nullptr;
    }

    char* contents = nullptr;
    char** lines = nullptr;

    GError* error = nullptr;
    if (!g_file_get_contents(MOUNTINFO, &contents, nullptr, &error))
    {
        LOG_WARN("Error reading {}: {}", MOUNTINFO, error->message);
        g_error_free(error);
        return nullptr;
    }

    /* See Documentation/filesystems/proc.txt for the format of /proc/self/mountinfo
     *
     * Note that things like space are encoded as \020.
     */

    lines = g_strsplit(contents, "\n", 0);
    unsigned int n;
    for (n = 0; lines[n] != nullptr; n++)
    {
        unsigned int mount_id;
        unsigned int parent_id;
        unsigned int major, minor;
        char encoded_root[PATH_MAX];
        char encoded_mount_point[PATH_MAX];
        char* mount_point;
        // dev_t dev;

        if (strlen(lines[n]) == 0)
            continue;

        if (sscanf(lines[n],
                   "%d %d %d:%d %s %s",
                   &mount_id,
                   &parent_id,
                   &major,
                   &minor,
                   encoded_root,
                   encoded_mount_point) != 6)
        {
            LOG_WARN("Error reading /proc/self/mountinfo: Error parsing line '{}'", lines[n]);
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
                g_free(mount_point);
        }
    }
    g_free(contents);
    g_strfreev(lines);

    if (mounts)
    {
        char* points;
        char* old_points;
        GList* l;
        // Sort the list to ensure that shortest mount paths appear first
        mounts = g_list_sort(mounts, (GCompareFunc)g_strcmp0);
        points = g_strdup((char*)mounts->data);
        l = mounts;
        while ((l = l->next))
        {
            old_points = points;
            points = g_strdup_printf("%s, %s", old_points, (char*)l->data);
            g_free(old_points);
        }
        g_list_foreach(mounts, (GFunc)g_free, nullptr);
        g_list_free(mounts);
        return points;
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
        device->partition_table_scheme = g_strdup(udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_TABLE_SCHEME"));
        device->partition_table_count = g_strdup(udev_device_get_property_value(device->udevice, "UDISKS_PARTITION_TABLE_COUNT"));
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
    if (!is_partition_table)
    {
        char* s;
        GDir* dir;

        s = g_path_get_basename(device->native_path);
        if ((dir = g_dir_open(device->native_path, 0, nullptr)) != nullptr)
        {
            unsigned int partition_count;
            const char* name;

            partition_count = 0;
            while ((name = g_dir_read_name(dir)) != nullptr)
            {
                if (g_str_has_prefix(name, s))
                {
                    partition_count++;
                }
            }
            g_dir_close(dir);

            if (partition_count > 0)
            {
                device->partition_table_scheme = g_strdup("");
                device->partition_table_count = g_strdup_printf("%d", partition_count);
                is_partition_table = true;
            }
        }
        g_free(s);
    }

    device->device_is_partition_table = is_partition_table;
    if (!is_partition_table)
    {
        if (device->partition_table_scheme)
            g_free(device->partition_table_scheme);
        device->partition_table_scheme = nullptr;
        if (device->partition_table_count)
            g_free(device->partition_table_count);
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
            device->partition_scheme = g_strdup(scheme);
            device->partition_size = g_strdup(size);
            device->partition_type = g_strdup(type);
            device->partition_label = g_strdup(label);
            device->partition_uuid = g_strdup(uuid);
            device->partition_flags = g_strdup(flags);
            device->partition_offset = g_strdup(offset);
            device->partition_alignment_offset = g_strdup(alignment_offset);
            device->partition_number = g_strdup(number);
            is_partition = true;
        }
    }

    /* Also handle the case where we are partitioned by the kernel and don't have
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

        device->partition_size = g_strdup_printf("%lu", size * 512);
        device->partition_alignment_offset = g_strdup_printf("%lu", alignment_offset);

        uint64_t offset =
            sysfs_get_uint64(device->native_path, "start") * device->device_block_size;
        device->partition_offset = g_strdup_printf("%lu", offset);

        char* s = device->native_path;
        unsigned int n;
        for (n = strlen(s) - 1; n >= 0 && g_ascii_isdigit(s[n]); n--)
            ;
        device->partition_number = g_strdup_printf("%ld", strtol(s + n + 1, nullptr, 0));
        /*
      s = g_strdup (device->priv->native_path);
      for (n = strlen (s) - 1; n >= 0 && s[n] != '/'; n--)
        s[n] = '\0';
      s[n] = '\0';
      device_set_partition_slave (device, compute_object_path (s));
      g_free (s);
        */
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
        device->optical_disc_num_tracks = g_strdup(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_TRACK_COUNT"));
        device->optical_disc_num_audio_tracks = g_strdup(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_TRACK_COUNT_AUDIO"));
        device->optical_disc_num_sessions = g_strdup(udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_SESSION_COUNT"));

        const char* cdrom_disc_state = udev_device_get_property_value(device->udevice, "ID_CDROM_MEDIA_STATE");

        device->optical_disc_is_blank = (!g_strcmp0(cdrom_disc_state, "blank"));
        device->optical_disc_is_appendable = (!g_strcmp0(cdrom_disc_state, "appendable"));
        device->optical_disc_is_closed = (!g_strcmp0(cdrom_disc_state, "complete"));
        // clang-format on
    }
    else
        device->device_is_optical_disc = false;
}

static void
device_free(device_t* device)
{
    if (!device)
        return;

    g_free(device->native_path);
    g_free(device->mount_points);
    g_free(device->devnode);

    g_free(device->device_presentation_hide);
    g_free(device->device_presentation_nopolicy);
    g_free(device->device_presentation_name);
    g_free(device->device_presentation_icon_name);
    g_free(device->device_automount_hint);
    g_free(device->device_by_id);
    g_free(device->id_usage);
    g_free(device->id_type);
    g_free(device->id_version);
    g_free(device->id_uuid);
    g_free(device->id_label);

    g_free(device->drive_vendor);
    g_free(device->drive_model);
    g_free(device->drive_revision);
    g_free(device->drive_serial);
    g_free(device->drive_wwn);
    g_free(device->drive_connection_interface);
    g_free(device->drive_media_compatibility);
    g_free(device->drive_media);

    g_free(device->partition_scheme);
    g_free(device->partition_number);
    g_free(device->partition_type);
    g_free(device->partition_label);
    g_free(device->partition_uuid);
    g_free(device->partition_flags);
    g_free(device->partition_offset);
    g_free(device->partition_size);
    g_free(device->partition_alignment_offset);

    g_free(device->partition_table_scheme);
    g_free(device->partition_table_count);

    g_free(device->optical_disc_num_tracks);
    g_free(device->optical_disc_num_audio_tracks);
    g_free(device->optical_disc_num_sessions);
    g_slice_free(device_t, device);
}

static device_t*
device_alloc(struct udev_device* udevice)
{
    device_t* device = g_slice_new0(device_t);
    device->udevice = udevice;

    device->native_path = nullptr;
    device->mount_points = nullptr;
    device->devnode = nullptr;

    device->device_is_system_internal = true;
    device->device_is_partition = false;
    device->device_is_partition_table = false;
    device->device_is_removable = false;
    device->device_is_media_available = false;
    device->device_is_read_only = false;
    device->device_is_drive = false;
    device->device_is_optical_disc = false;
    device->device_is_mounted = false;
    device->device_presentation_hide = nullptr;
    device->device_presentation_nopolicy = nullptr;
    device->device_presentation_name = nullptr;
    device->device_presentation_icon_name = nullptr;
    device->device_automount_hint = nullptr;
    device->device_by_id = nullptr;
    device->device_size = 0;
    device->device_block_size = 0;
    device->id_usage = nullptr;
    device->id_type = nullptr;
    device->id_version = nullptr;
    device->id_uuid = nullptr;
    device->id_label = nullptr;

    device->drive_vendor = nullptr;
    device->drive_model = nullptr;
    device->drive_revision = nullptr;
    device->drive_serial = nullptr;
    device->drive_wwn = nullptr;
    device->drive_connection_interface = nullptr;
    device->drive_connection_speed = 0;
    device->drive_media_compatibility = nullptr;
    device->drive_media = nullptr;
    device->drive_is_media_ejectable = false;
    device->drive_can_detach = false;

    device->partition_scheme = nullptr;
    device->partition_number = nullptr;
    device->partition_type = nullptr;
    device->partition_label = nullptr;
    device->partition_uuid = nullptr;
    device->partition_flags = nullptr;
    device->partition_offset = nullptr;
    device->partition_size = nullptr;
    device->partition_alignment_offset = nullptr;

    device->partition_table_scheme = nullptr;
    device->partition_table_count = nullptr;

    device->optical_disc_is_blank = false;
    device->optical_disc_is_appendable = false;
    device->optical_disc_is_closed = false;
    device->optical_disc_num_tracks = nullptr;
    device->optical_disc_num_audio_tracks = nullptr;
    device->optical_disc_num_sessions = nullptr;

    return device;
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
    info.append(fmt::format("  device:                      {}:{}\n", (unsigned int)MAJOR(device->devnum), (unsigned int)MINOR(device->devnum)));
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
        if (device->drive_connection_interface == nullptr || strlen(device->drive_connection_interface) == 0)
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
    char* contents = nullptr;
    char** lines = nullptr;
    struct udev_device* udevice;
    dev_t devnum;
    char* str;

    GError* error = nullptr;
    if (!g_file_get_contents(MOUNTINFO, &contents, nullptr, &error))
    {
        LOG_WARN("Error reading {}: {}", MOUNTINFO, error->message);
        g_error_free(error);
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
    lines = g_strsplit(contents, "\n", 0);
    unsigned int n;
    for (n = 0; lines[n] != nullptr; n++)
    {
        unsigned int mount_id;
        unsigned int parent_id;
        unsigned int major, minor;
        char encoded_root[PATH_MAX];
        char encoded_mount_point[PATH_MAX];
        char* mount_point;
        char* fstype;

        if (strlen(lines[n]) == 0)
            continue;

        if (sscanf(lines[n],
                   "%d %d %d:%d %s %s",
                   &mount_id,
                   &parent_id,
                   &major,
                   &minor,
                   encoded_root,
                   encoded_mount_point) != 6)
        {
            LOG_WARN("Error reading /proc/self/mountinfo: Error parsing line '{}'", lines[n]);
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
            sep = strstr(lines[n], " - ");
            if (sep && sscanf(sep + 3, "%s %s", typebuf, mount_source) == 2)
            {
                // LOG_INFO("    source={}", mount_source);
                if (g_str_has_prefix(mount_source, "/dev/"))
                {
                    /* is a subdir mount on a local device, eg a bind mount
                     * so don't include this mount point */
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
            g_free(mount_point);
            continue;
        }

        // fstype
        fstype = strstr(lines[n], " - ");
        if (fstype)
        {
            fstype += 3;
            // modifies lines[n]
            if ((str = strchr(fstype, ' ')))
                str[0] = '\0';
        }

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
                        g_free(mount_point);
                        continue;
                    }
                }
                devmount = g_slice_new0(devmount_t);
                devmount->major = major;
                devmount->minor = minor;
                devmount->mount_points = nullptr;
                devmount->fstype = g_strdup(fstype);
                devmount->mounts = nullptr;
                newmounts = g_list_prepend(newmounts, devmount);
            }
            else
            {
                // initial load !report don't add non-block devices
                devnum = makedev(major, minor);
                udevice = udev_device_new_from_devnum(udev, 'b', devnum);
                if (udevice)
                {
                    // is block device
                    udev_device_unref(udevice);
                    if (subdir_mount)
                    {
                        // is block device with subdir mount - ignore
                        g_free(mount_point);
                        continue;
                    }
                    // add
                    devmount = g_slice_new0(devmount_t);
                    devmount->major = major;
                    devmount->minor = minor;
                    devmount->mount_points = nullptr;
                    devmount->fstype = g_strdup(fstype);
                    devmount->mounts = nullptr;
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
            g_free(mount_point);
    }
    g_free(contents);
    g_strfreev(lines);
    // LOG_INFO("LINES DONE");
    // translate each mount points list to string
    char* points;
    char* old_points;
    GList* m;
    for (l = newmounts; l; l = l->next)
    {
        devmount = static_cast<devmount_t*>(l->data);
        // Sort the list to ensure that shortest mount paths appear first
        devmount->mounts = g_list_sort(devmount->mounts, (GCompareFunc)g_strcmp0);
        m = devmount->mounts;
        points = g_strdup((char*)m->data);
        while ((m = m->next))
        {
            old_points = points;
            points = g_strdup_printf("%s, %s", old_points, (char*)m->data);
            g_free(old_points);
        }
        g_list_foreach(devmount->mounts, (GFunc)g_free, nullptr);
        g_list_free(devmount->mounts);
        devmount->mounts = nullptr;
        devmount->mount_points = points;
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
                    g_free(devmount->mount_points);
                    g_free(devmount->fstype);
                    devmounts = g_list_remove(devmounts, devmount);
                    g_slice_free(devmount_t, devmount);
                }
            }
            else
            {
                // new mount
                // LOG_INFO("    new mount {}:{} {}", devmount->major, devmount->minor,
                // devmount->mount_points);
                devmount_t* devcopy = g_slice_new0(devmount_t);
                devcopy->major = devmount->major;
                devcopy->minor = devmount->minor;
                devcopy->mount_points = g_strdup(devmount->mount_points);
                devcopy->fstype = g_strdup(devmount->fstype);
                devcopy->mounts = nullptr;
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
        {
            changed = g_list_prepend(changed, devmount);
        }
        else
        {
            g_free(devmount->mount_points);
            g_free(devmount->fstype);
            g_slice_free(devmount_t, devmount);
        }
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
                devnode = g_strdup(udev_device_get_devnode(udevice));
            if (devnode)
            {
                // block device
                LOG_INFO("mount changed: {}", devnode);
                if ((volume = vfs_volume_read_by_device(udevice)))
                    vfs_volume_device_added(volume, true); // frees volume if needed
                g_free(devnode);
            }
            else
            {
                // not a block device
                if ((volume = vfs_volume_read_by_mount(devnum, devmount->mount_points)))
                {
                    LOG_INFO("special mount changed: {} ({}:{}) on {}",
                             volume->device_file,
                             (unsigned int)MAJOR(volume->devnum),
                             (unsigned int)MINOR(volume->devnum),
                             devmount->mount_points);
                    vfs_volume_device_added(volume, false); // frees volume if needed
                }
                else
                    vfs_volume_nonblock_removed(devnum);
            }
            udev_device_unref(udevice);
            g_free(devmount->mount_points);
            g_free(devmount->fstype);
            g_slice_free(devmount_t, devmount);
        }
        g_list_free(changed);
    }
    // printf ( "END PARSE\n");
}

static void
free_devmounts()
{
    GList* l;
    devmount_t* devmount;

    if (!devmounts)
        return;
    for (l = devmounts; l; l = l->next)
    {
        devmount = static_cast<devmount_t*>(l->data);
        g_free(devmount->mount_points);
        g_free(devmount->fstype);
        g_slice_free(devmount_t, devmount);
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
    if (cond & ~G_IO_ERR)
        return true;

    // printf ("@@@ /proc/self/mountinfo changed\n");
    parse_mounts(true);

    return true;
}

static bool
cb_udev_monitor_watch(GIOChannel* channel, GIOCondition cond, void* user_data)
{
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
        char* devnode = g_strdup(udev_device_get_devnode(udevice));
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
        g_free(devnode);
        udev_device_unref(udevice);
    }
    return true;
}

static void
vfs_free_volume_members(VFSVolume* volume)
{
    g_free(volume->device_file);
    g_free(volume->udi);
    g_free(volume->label);
    g_free(volume->mount_point);
    g_free(volume->disp_name);
    g_free(volume->icon);
}

void
vfs_volume_set_info(VFSVolume* volume)
{
    char* parameter;
    char* value;
    char* valuec;
    char* lastcomma;
    char* disp_device;
    char* disp_label;
    char* disp_size;
    char* disp_mount;
    char* disp_fstype;
    char* disp_devnum;
    char* disp_id = nullptr;
    char size_str[64];

    if (!volume)
        return;

    // set device icon
    switch (volume->device_type)
    {
        case DEVICE_TYPE_BLOCK:
            if (volume->is_audiocd)
                volume->icon = g_strdup_printf("dev_icon_audiocd");
            else if (volume->is_optical)
            {
                if (volume->is_mounted)
                    volume->icon = g_strdup_printf("dev_icon_optical_mounted");
                else if (volume->is_mountable)
                    volume->icon = g_strdup_printf("dev_icon_optical_media");
                else
                    volume->icon = g_strdup_printf("dev_icon_optical_nomedia");
            }
            else if (volume->is_floppy)
            {
                if (volume->is_mounted)
                    volume->icon = g_strdup_printf("dev_icon_floppy_mounted");
                else
                    volume->icon = g_strdup_printf("dev_icon_floppy_unmounted");
                volume->is_mountable = true;
            }
            else if (volume->is_removable)
            {
                if (volume->is_mounted)
                    volume->icon = g_strdup_printf("dev_icon_remove_mounted");
                else
                    volume->icon = g_strdup_printf("dev_icon_remove_unmounted");
            }
            else
            {
                if (volume->is_mounted)
                {
                    if (g_str_has_prefix(volume->device_file, "/dev/loop"))
                        volume->icon = g_strdup_printf("dev_icon_file");
                    else
                        volume->icon = g_strdup_printf("dev_icon_internal_mounted");
                }
                else
                    volume->icon = g_strdup_printf("dev_icon_internal_unmounted");
            }
            break;
        case DEVICE_TYPE_NETWORK:
            volume->icon = g_strdup_printf("dev_icon_network");
            break;
        case DEVICE_TYPE_OTHER:
            volume->icon = g_strdup_printf("dev_icon_file");
            break;
        default:
            break;
    }

    // set disp_id using by-id
    switch (volume->device_type)
    {
        case DEVICE_TYPE_BLOCK:
            if (volume->is_floppy && !volume->udi)
                disp_id = g_strdup_printf(":floppy");
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
                    disp_id = g_strdup_printf(":%.16s", lastcomma);
                }
            }
            else if (volume->is_optical)
                disp_id = g_strdup_printf(":optical");
            if (!disp_id)
                disp_id = g_strdup("");
            // table type
            if (volume->is_table)
            {
                if (volume->fs_type && volume->fs_type[0] == '\0')
                {
                    g_free(volume->fs_type);
                    volume->fs_type = nullptr;
                }
                if (!volume->fs_type)
                    volume->fs_type = g_strdup("table");
            }
            break;
        default:
            disp_id = g_strdup(volume->udi);
            break;
    }

    // set display name
    if (volume->is_mounted)
    {
        if (volume->label && volume->label[0] != '\0')
            disp_label = g_strdup_printf("%s", volume->label);
        else
            disp_label = g_strdup("");

        if (volume->size > 0)
        {
            vfs_file_size_to_string_format(size_str, volume->size, false);
            disp_size = g_strdup_printf("%s", size_str);
        }
        else
            disp_size = g_strdup("");
        if (volume->mount_point && volume->mount_point[0] != '\0')
            disp_mount = g_strdup_printf("%s", volume->mount_point);
        else
            disp_mount = g_strdup_printf("???");
    }
    else if (volume->is_mountable) // has_media
    {
        if (volume->is_blank)
            disp_label = g_strdup_printf("[blank]");
        else if (volume->label && volume->label[0] != '\0')
            disp_label = g_strdup_printf("%s", volume->label);
        else if (volume->is_audiocd)
            disp_label = g_strdup_printf("[audio]");
        else
            disp_label = g_strdup("");
        if (volume->size > 0)
        {
            vfs_file_size_to_string_format(size_str, volume->size, false);
            disp_size = g_strdup_printf("%s", size_str);
        }
        else
            disp_size = g_strdup("");
        disp_mount = g_strdup_printf("---");
    }
    else
    {
        disp_label = g_strdup_printf("[no media]");
        disp_size = g_strdup("");
        disp_mount = g_strdup("");
    }
    if (!strncmp(volume->device_file, "/dev/", 5))
        disp_device = g_strdup(volume->device_file + 5);
    else if (g_str_has_prefix(volume->device_file, "curlftpfs#"))
        disp_device = g_strdup(volume->device_file + 10);
    else
        disp_device = g_strdup(volume->device_file);
    if (volume->fs_type && volume->fs_type[0] != '\0')
        disp_fstype = g_strdup(volume->fs_type); // g_strdup_printf( "-%s", volume->fs_type );
    else
        disp_fstype = g_strdup("");
    disp_devnum = g_strdup_printf("%u:%u",
                                  (unsigned int)MAJOR(volume->devnum),
                                  (unsigned int)MINOR(volume->devnum));

    char* fmt = xset_get_s("dev_dispname");
    if (!fmt)
        parameter = g_strdup_printf("%s %s %s %s %s %s",
                                    disp_device,
                                    disp_size,
                                    disp_fstype,
                                    disp_label,
                                    disp_mount,
                                    disp_id);
    else
    {
        value = replace_string(fmt, "%v", disp_device, false);
        valuec = replace_string(value, "%s", disp_size, false);
        g_free(value);
        value = replace_string(valuec, "%t", disp_fstype, false);
        g_free(valuec);
        valuec = replace_string(value, "%l", disp_label, false);
        g_free(value);
        value = replace_string(valuec, "%m", disp_mount, false);
        g_free(valuec);
        valuec = replace_string(value, "%n", disp_devnum, false);
        g_free(value);
        parameter = replace_string(valuec, "%i", disp_id, false);
        g_free(valuec);
    }

    // volume->disp_name = g_filename_to_utf8( parameter, -1, nullptr, nullptr, nullptr );
    while (strstr(parameter, "  "))
    {
        value = parameter;
        parameter = replace_string(value, "  ", " ", false);
        g_free(value);
    }
    while (parameter[0] == ' ')
    {
        value = parameter;
        parameter = g_strdup(parameter + 1);
        g_free(value);
    }
    volume->disp_name = g_filename_display_name(parameter);

    g_free(parameter);
    g_free(disp_label);
    g_free(disp_size);
    g_free(disp_mount);
    g_free(disp_device);
    g_free(disp_fstype);
    g_free(disp_id);
    g_free(disp_devnum);

    if (!volume->udi)
        volume->udi = g_strdup(volume->device_file);
}

static VFSVolume*
vfs_volume_read_by_device(struct udev_device* udevice)
{ // uses udev to read device parameters into returned volume
    VFSVolume* volume = nullptr;

    if (!udevice)
        return nullptr;
    device_t* device = device_alloc(udevice);
    if (!device_get_info(device) || !device->devnode || device->devnum == 0 ||
        !g_str_has_prefix(device->devnode, "/dev/"))
    {
        device_free(device);
        return nullptr;
    }

    // translate device info to VFSVolume
    volume = g_slice_new0(VFSVolume);
    volume->devnum = device->devnum;
    volume->device_type = DEVICE_TYPE_BLOCK;
    volume->device_file = g_strdup(device->devnode);
    volume->udi = g_strdup(device->device_by_id);
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
            volume->mount_point = g_strdup(device->mount_points);
            comma[0] = ',';
        }
        else
            volume->mount_point = g_strdup(device->mount_points);
    }
    volume->size = device->device_size;
    volume->label = g_strdup(device->id_label);
    volume->fs_type = g_strdup(device->id_type);
    volume->disp_name = nullptr;
    volume->icon = nullptr;
    volume->automount_time = 0;
    volume->inhibit_auto = false;

    device_free(device);

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

    char* contents = nullptr;
    char** lines = nullptr;
    GError* error = nullptr;

    char* mtab_path = g_build_filename(SYSCONFDIR, "mtab", nullptr);

    if (mtab_file)
    {
        // read from a custom mtab file, eg ~/.mtab.fuseiso
        if (!g_file_get_contents(mtab_file, &contents, nullptr, nullptr))
        {
            g_free(mtab_path);
            return false;
        }
    }
    else if (!g_file_get_contents(MTAB, &contents, nullptr, nullptr))
    {
        if (!g_file_get_contents(mtab_path, &contents, nullptr, &error))
        {
            LOG_WARN("Error reading {}: {}", mtab_path, error->message);
            g_error_free(error);
            g_free(mtab_path);
            return false;
        }
    }
    g_free(mtab_path);
    lines = g_strsplit(contents, "\n", 0);
    unsigned int n;
    for (n = 0; lines[n] != nullptr; n++)
    {
        if (lines[n][0] == '\0')
            continue;

        if (sscanf(lines[n], "%s %s %s ", encoded_file, encoded_point, encoded_fstype) != 3)
        {
            LOG_WARN("Error parsing mtab line '{}'", lines[n]);
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
        g_free(point);
    }
    g_free(contents);
    g_strfreev(lines);
    return ret;
}

bool
mtab_fstype_is_handled_by_protocol(const char* mtab_fstype)
{
    bool found = false;
    const char* handlers_list;
    if (mtab_fstype && mtab_fstype[0] && (handlers_list = xset_get_s("dev_net_cnf")))
    {
        // is the mtab_fstype handled by a protocol handler?
        GSList* values = nullptr;
        values = g_slist_prepend(values, g_strdup(mtab_fstype));
        values = g_slist_prepend(values, g_strconcat("mtab_fs=", mtab_fstype, nullptr));
        char** handlers = g_strsplit(handlers_list, " ", 0);
        if (handlers)
        {
            // test handlers
            XSet* set;
            int i;
            for (i = 0; handlers[i]; i++)
            {
                if (!handlers[i][0] || !(set = xset_is(handlers[i])) ||
                    set->b != XSET_B_TRUE /* disabled */)
                    continue;
                // test whitelist
                if (g_strcmp0(set->s, "*") && ptk_handler_values_in_list(set->s, values, nullptr))
                {
                    found = true;
                    break;
                }
            }
            g_strfreev(handlers);
        }
        g_slist_foreach(values, (GFunc)g_free, nullptr);
        g_slist_free(values);
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
    char* orig_url = g_strdup(url);
    char* xurl = orig_url;
    netmount_t* nm = g_slice_new0(netmount_t);
    nm->fstype = nullptr; // protocol
    nm->host = nullptr;
    nm->ip = nullptr;
    nm->port = nullptr;
    nm->user = nullptr;
    nm->pass = nullptr;
    nm->path = nullptr;
    nm->url = g_strdup(url);

    // get protocol
    bool is_colon;
    if (g_str_has_prefix(xurl, "//"))
    { // //host...
        if (xurl[2] == '\0')
            goto _net_free;
        nm->fstype = g_strdup("smb");
        is_colon = false;
    }
    else if ((str = strstr(xurl, "://")))
    { // protocol://host...
        if (xurl[0] == ':' || xurl[0] == '/')
            goto _net_free;
        str[0] = '\0';
        nm->fstype = g_strstrip(g_strdup(xurl));
        if (nm->fstype[0] == '\0')
            goto _net_free;
        str[0] = ':';
        is_colon = true;

        // remove ...# from start of protocol
        // eg curlftpfs mtab url looks like: curlftpfs#ftp://hostname/
        if (nm->fstype && (str = strchr(nm->fstype, '#')))
        {
            str2 = nm->fstype;
            nm->fstype = g_strdup(str + 1);
            g_free(str2);
        }
    }
    else if ((str = strstr(xurl, ":/")))
    { // host:/path
        // note: sshfs also uses this URL format in mtab, but mtab_fstype == fuse.sshfs
        if (xurl[0] == ':' || xurl[0] == '/')
            goto _net_free;
        nm->fstype = g_strdup("nfs");
        str[0] = '\0';
        nm->host = g_strdup(xurl);
        str[0] = ':';
        is_colon = true;
    }
    else
        goto _net_free;

    ret = 1;

    // parse
    if (is_colon && (str = strchr(xurl, ':')))
    {
        xurl = str + 1;
    }
    while (xurl[0] == '/')
        xurl++;

    char* trim_url;
    trim_url = g_strdup(xurl);

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
                nm->pass = g_strdup(str2 + 1);
        }
        if (xurl[0] != '\0')
            nm->user = g_strdup(xurl);
        xurl = str + 1;
    }

    // path
    if ((str = strchr(xurl, '/')))
    {
        nm->path = g_strdup(str);
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
                    nm->host = g_strdup(xurl + 1);
                if (str[1] == ':' && str[2] != '\0')
                    nm->port = g_strdup(str + 1);
            }
        }
        else if (xurl[0] != '\0')
        {
            if ((str = strchr(xurl, ':')))
            {
                str[0] = '\0';
                if (str[1] != '\0')
                    nm->port = g_strdup(str + 1);
            }
            nm->host = g_strdup(xurl);
        }
    }
    g_free(trim_url);
    g_free(orig_url);

    // check host
    if (!nm->host)
        nm->host = g_strdup("");

    /*
    // check user pass port
    if ( ( nm->user && strchr( nm->user, ' ' ) )
            || ( nm->pass && strchr( nm->pass, ' ' ) )
            || ( nm->port && strchr( nm->port, ' ' ) ) )
        ret = 2;
    */

    *netmount = nm;
    return ret;

_net_free:
    g_free(nm->url);
    g_free(nm->fstype);
    g_free(nm->host);
    g_free(nm->ip);
    g_free(nm->port);
    g_free(nm->user);
    g_free(nm->pass);
    g_free(nm->path);
    g_slice_free(netmount_t, nm);
    g_free(orig_url);
    return 0;
}

static VFSVolume*
vfs_volume_read_by_mount(dev_t devnum, const char* mount_points)
{ // read a non-block device
    VFSVolume* volume;
    char* str;
    netmount_t* netmount = nullptr;

    if (devnum == 0 || !mount_points)
        return nullptr;

    // get single mount point
    char* point = g_strdup(mount_points);
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

    int i = split_network_url(name, &netmount);
    if (i == 1)
    {
        // network URL
        volume = g_slice_new0(VFSVolume);
        volume->devnum = devnum;
        volume->device_type = DEVICE_TYPE_NETWORK;
        volume->should_autounmount = false;
        volume->udi = netmount->url;
        volume->label = netmount->host;
        volume->fs_type = mtab_fstype ? mtab_fstype : g_strdup("");
        volume->size = 0;
        volume->device_file = name;
        volume->is_mounted = true;
        volume->ever_mounted = true;
        volume->open_main_window = nullptr;
        volume->mount_point = point;
        volume->disp_name = nullptr;
        volume->icon = nullptr;
        volume->automount_time = 0;
        volume->inhibit_auto = false;

        // free unused netmount
        g_free(netmount->ip);
        g_free(netmount->port);
        g_free(netmount->user);
        g_free(netmount->pass);
        g_free(netmount->path);
        g_free(netmount->fstype);
        g_slice_free(netmount_t, netmount);
    }
    else if (!g_strcmp0(mtab_fstype, "fuse.fuseiso"))
    {
        // an ISO file mounted with fuseiso
        /* get device_file from ~/.mtab.fuseiso
         * hack - sleep for 0.2 seconds here because sometimes the
         * .mtab.fuseiso file is not updated until after new device is detected. */
        g_usleep(200000);
        char* mtab_file = g_build_filename(vfs_user_home_dir(), ".mtab.fuseiso", nullptr);
        char* new_name = nullptr;
        if (path_is_mounted_mtab(mtab_file, point, &new_name, nullptr) && new_name && new_name[0])
        {
            g_free(name);
            name = new_name;
            new_name = nullptr;
        }
        g_free(new_name);
        g_free(mtab_file);

        // create a volume
        volume = g_slice_new0(VFSVolume);
        volume->devnum = devnum;
        volume->device_type = DEVICE_TYPE_OTHER;
        volume->device_file = name;
        volume->udi = g_strdup(name);
        volume->should_autounmount = false;
        volume->label = nullptr;
        volume->fs_type = mtab_fstype;
        volume->size = 0;
        volume->is_mounted = true;
        volume->ever_mounted = true;
        volume->open_main_window = nullptr;
        volume->mount_point = point;
        volume->disp_name = nullptr;
        volume->icon = nullptr;
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
            keep = g_str_has_prefix(point, vfs_user_cache_dir()) ||
                   g_str_has_prefix(point, "/media/") || g_str_has_prefix(point, "/run/media/") ||
                   g_str_has_prefix(mtab_fstype, "fuse.");
            if (!keep)
            {
                // in Auto-Mount|Mount Dirs ?
                char* mount_parent = ptk_location_view_get_mount_point_dir(nullptr);
                keep = g_str_has_prefix(point, mount_parent);
                g_free(mount_parent);
            }
        }
        // mount point must be readable
        keep = keep && (geteuid() == 0 || faccessat(0, point, R_OK, AT_EACCESS) == 0);
        if (keep)
        {
            // create a volume
            volume = g_slice_new0(VFSVolume);
            volume->devnum = devnum;
            volume->device_type = DEVICE_TYPE_OTHER;
            volume->device_file = name;
            volume->udi = g_strdup(name);
            volume->should_autounmount = false;
            volume->label = nullptr;
            volume->fs_type = mtab_fstype;
            volume->size = 0;
            volume->is_mounted = true;
            volume->ever_mounted = true;
            volume->open_main_window = nullptr;
            volume->mount_point = point;
            volume->disp_name = nullptr;
            volume->icon = nullptr;
            volume->automount_time = 0;
            volume->inhibit_auto = false;
        }
        else
        {
            g_free(name);
            g_free(mtab_fstype);
            g_free(point);
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
    char* str;
    const char* handlers_list;
    int i;
    XSet* set;
    GSList* values = nullptr;

    if (mount_point)
        *mount_point = nullptr;

    switch (mode)
    {
        case HANDLER_MODE_FS:
            if (!vol)
                return nullptr;

            // fs values
            // change spaces in label to underscores for testing
            // clang-format off
            if (vol->label && vol->label[0])
                values = g_slist_prepend(values, g_strdelimit(g_strconcat("label=", vol->label, nullptr), " ", '_'));
            if (vol->udi && vol->udi[0])
                values = g_slist_prepend(values, g_strconcat("id=", vol->udi, nullptr));
            values = g_slist_prepend(values, g_strconcat("audiocd=", vol->is_audiocd ? "1" : "0", nullptr));
            values = g_slist_prepend(values, g_strconcat("optical=", vol->is_optical ? "1" : "0", nullptr));
            values = g_slist_prepend(values, g_strconcat("removable=", vol->is_removable ? "1" : "0", nullptr));
            values = g_slist_prepend(values, g_strconcat("mountable=", vol->is_mountable && !vol->is_blank ? "1" : "0", nullptr));
            // change spaces in device to underscores for testing - for ISO files
            values = g_slist_prepend(values, g_strdelimit(g_strconcat("dev=", vol->device_file, nullptr), " ", '_'));
            // clang-format on
            if (vol->fs_type)
                values = g_slist_prepend(values, g_strdup(vol->fs_type));
            break;
        case HANDLER_MODE_NET:
            // for DEVICE_TYPE_NETWORK
            if (netmount)
            {
                // net values
                if (netmount->host && netmount->host[0])
                    values = g_slist_prepend(values, g_strconcat("host=", netmount->host, nullptr));
                if (netmount->user && netmount->user[0])
                    values = g_slist_prepend(values, g_strconcat("user=", netmount->user, nullptr));
                if (action != HANDLER_MOUNT && vol && vol->is_mounted)
                {
                    // clang-format off
                    // user-entered url (or mtab url if not available)
                    values = g_slist_prepend(values, g_strconcat("url=", vol->udi, nullptr));
                    // mtab fs type (fuse.ssh)
                    values = g_slist_prepend(values, g_strconcat("mtab_fs=", vol->fs_type, nullptr));
                    // mtab_url == url if mounted
                    values = g_slist_prepend(values, g_strconcat("mtab_url=", vol->device_file, nullptr));
                    // clang-format on
                }
                else if (netmount->url && netmount->url[0])
                    // user-entered url
                    values = g_slist_prepend(values, g_strconcat("url=", netmount->url, nullptr));
                // url-derived protocol
                if (netmount->fstype && netmount->fstype[0])
                    values = g_slist_prepend(values, g_strdup(netmount->fstype));
            }
            else
            {
                // for DEVICE_TYPE_OTHER unmount or prop that has protocol handler in mtab_fs
                if (action != HANDLER_MOUNT && vol && vol->is_mounted)
                {
                    // clang-format off
                    // user-entered url (or mtab url if not available)
                    values = g_slist_prepend(values, g_strconcat("url=", vol->udi, nullptr));
                    // mtab fs type (fuse.ssh)
                    values = g_slist_prepend(values, g_strconcat("mtab_fs=", vol->fs_type, nullptr));
                    // mtab_url == url if mounted
                    values = g_slist_prepend(values, g_strconcat("mtab_url=", vol->device_file, nullptr));
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
        values = g_slist_prepend(values, g_strconcat("point=", vol->mount_point, nullptr));

    // get handlers
    if (!(handlers_list = xset_get_s(mode == HANDLER_MODE_FS ? "dev_fs_cnf" : "dev_net_cnf")))
        return nullptr;
    char** handlers = g_strsplit(handlers_list, " ", 0);
    if (!handlers)
        return nullptr;

    // test handlers
    bool found = false;
    char* msg = nullptr;
    for (i = 0; handlers[i]; i++)
    {
        if (!handlers[i][0] || !(set = xset_is(handlers[i])) ||
            set->b != XSET_B_TRUE /* disabled */)
            continue;
        switch (mode)
        {
            case HANDLER_MODE_FS:
                // test blacklist
                if (ptk_handler_values_in_list(set->x, values, nullptr))
                    break;
                // test whitelist
                if (ptk_handler_values_in_list(set->s, values, &msg))
                {
                    found = true;
                    break;
                }
                break;
            case HANDLER_MODE_NET:
                // test blacklist
                if (ptk_handler_values_in_list(set->x, values, nullptr))
                    break;
                // test whitelist
                if (ptk_handler_values_in_list(set->s, values, &msg))
                {
                    found = true;
                    break;
                }
                break;
            default:
                break;
        }
    }
    g_slist_foreach(values, (GFunc)g_free, nullptr);
    g_slist_free(values);
    g_strfreev(handlers);
    if (!found)
        return nullptr;

    // get command for action
    char* command = nullptr;
    const char* action_s;
    bool terminal;
    char* err_msg = ptk_handler_load_script(mode, action, set, nullptr, &command);
    if (err_msg)
    {
        LOG_WARN("{}", err_msg);
        g_free(err_msg);
        return nullptr;
    }

    switch (action)
    {
        case HANDLER_MOUNT:
            terminal = set->in_terminal == XSET_B_TRUE;
            action_s = "MOUNT";
            break;
        case HANDLER_UNMOUNT:
            terminal = set->keep_terminal == XSET_B_TRUE;
            action_s = "UNMOUNT";
            break;
        case HANDLER_PROP:
            terminal = set->scroll_lock == XSET_B_TRUE;
            action_s = "PROPERTIES";
            break;
        default:
            return nullptr;
    }

    // show selected handler
    LOG_INFO("{} '{}': {}{} {}",
             mode == HANDLER_MODE_FS ? "Selected Device Handler" : "Selected Protocol Handler",
             set->menu_label,
             action_s,
             command ? "" : " (no command)",
             msg);

    if (ptk_handler_command_is_empty(command))
    {
        // empty command
        g_free(command);
        return nullptr;
    }

    // replace sub vars
    char* fileq = nullptr;
    switch (mode)
    {
        case HANDLER_MODE_FS:
            /*
             *      %v  device
             *      %o  volume-specific mount options (use in mount command only)
             *      %t  filesystem type being mounted (eg vfat)
             *      %a  mount point, or create auto mount point
             */
            fileq = bash_quote(vol->device_file); // for iso files
            str = command;
            command = replace_string(command, "%v", fileq, false);
            g_free(fileq);
            g_free(str);
            if (action == HANDLER_MOUNT)
            {
                str = command;
                command = replace_string(command, "%o", options ? options : "", false);
                g_free(str);
                str = command;
                command = replace_string(command, "%t", vol->fs_type ? vol->fs_type : "", false);
                g_free(str);
            }
            if (strstr(command, "%a"))
            {
                if (action == HANDLER_MOUNT)
                {
                    // create mount point
                    char* point_dir = ptk_location_view_create_mount_point(HANDLER_MODE_FS,
                                                                           vol,
                                                                           nullptr,
                                                                           nullptr);
                    str = command;
                    command = replace_string(command, "%a", point_dir, false);
                    g_free(str);
                    if (mount_point)
                        *mount_point = point_dir;
                    else
                        g_free(point_dir);
                }
                else
                {
                    str = command;
                    command =
                        replace_string(command,
                                       "%a",
                                       vol->is_mounted && vol->mount_point && vol->mount_point[0]
                                           ? vol->mount_point
                                           : "",
                                       false);
                    g_free(str);
                }
            }
            // standard sub vars
            str = command;
            command = replace_line_subs(command);
            g_free(str);
            break;
        case HANDLER_MODE_NET:
            // also used for DEVICE_TYPE_OTHER unmount and prop
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
                if (strstr(command, "%url%"))
                {
                    str = command;
                    if (action != HANDLER_MOUNT && vol && vol->is_mounted)
                        // user-entered url (or mtab url if not available)
                        command = replace_string(command, "%url%", vol->udi, false);
                    else
                        // user-entered url
                        command = replace_string(command, "%url%", netmount->url, false);
                    g_free(str);
                }
                if (strstr(command, "%proto%"))
                {
                    str = command;
                    command = replace_string(command, "%proto%", netmount->fstype, false);
                    g_free(str);
                }
                if (strstr(command, "%host%"))
                {
                    str = command;
                    command = replace_string(command, "%host%", netmount->host, false);
                    g_free(str);
                }
                if (strstr(command, "%port%"))
                {
                    str = command;
                    command = replace_string(command, "%port%", netmount->port, false);
                    g_free(str);
                }
                if (strstr(command, "%user%"))
                {
                    str = command;
                    command = replace_string(command, "%user%", netmount->user, false);
                    g_free(str);
                }
                if (strstr(command, "%pass%"))
                {
                    str = command;
                    command = replace_string(command, "%pass%", netmount->pass, false);
                    g_free(str);
                }
                if (strstr(command, "%path%"))
                {
                    str = command;
                    command = replace_string(command, "%path%", netmount->path, false);
                    g_free(str);
                }
            }
            if (strstr(command, "%a"))
            {
                if (action == HANDLER_MOUNT && netmount)
                {
                    // create mount point
                    char* point_dir = ptk_location_view_create_mount_point(HANDLER_MODE_NET,
                                                                           nullptr,
                                                                           netmount,
                                                                           nullptr);
                    str = command;
                    command = replace_string(command, "%a", point_dir, false);
                    g_free(str);
                    if (mount_point)
                        *mount_point = point_dir;
                    else
                        g_free(point_dir);
                }
                else
                {
                    str = command;
                    command = replace_string(command,
                                             "%a",
                                             vol && vol->is_mounted && vol->mount_point &&
                                                     vol->mount_point[0]
                                                 ? vol->mount_point
                                                 : "",
                                             false);
                    g_free(str);
                }
            }

            if (netmount)
            {
                // add bash variables
                // urlq is user-entered url or (if mounted) mtab url
                char* urlq = bash_quote(
                    action != HANDLER_MOUNT && vol && vol->is_mounted ? vol->udi : netmount->url);
                char* protoq = bash_quote(netmount->fstype); // url-derived protocol (ssh)
                char* hostq = bash_quote(netmount->host);
                char* portq = bash_quote(netmount->port);
                char* userq = bash_quote(netmount->user);
                char* passq = bash_quote(netmount->pass);
                char* pathq = bash_quote(netmount->path);
                // mtab fs type (fuse.ssh)
                char* mtabfsq = bash_quote(
                    action != HANDLER_MOUNT && vol && vol->is_mounted ? vol->fs_type : nullptr);
                // urlq and mtaburlq will both be the same mtab url if mounted
                char* mtaburlq = bash_quote(
                    action != HANDLER_MOUNT && vol && vol->is_mounted ? vol->device_file : nullptr);
                str = command;
                command = g_strdup_printf(
                    "fm_url_proto=%s; fm_url=%s; fm_url_host=%s; fm_url_port=%s; fm_url_user=%s; "
                    "fm_url_pass=%s; fm_url_path=%s; fm_mtab_fs=%s; fm_mtab_url=%s\n%s",
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
                g_free(str);
                g_free(urlq);
                g_free(protoq);
                g_free(hostq);
                g_free(portq);
                g_free(userq);
                g_free(passq);
                g_free(pathq);
                g_free(mtabfsq);
                g_free(mtaburlq);
            }
            break;
        default:
            break;
    }

    *run_in_terminal = terminal;
    return command;
}

static bool
vfs_volume_is_automount(VFSVolume* vol)
{ // determine if volume should be automounted or auto-unmounted
    char* test;
    char* value;

    if (vol->should_autounmount)
        // volume is a network or ISO file that was manually mounted -
        // for autounmounting only
        return true;
    if (!vol->is_mountable || vol->is_blank || vol->device_type != DEVICE_TYPE_BLOCK)
        return false;

    char* showhidelist = g_strdup_printf(" %s ", xset_get_s("dev_automount_volumes"));
    int i;
    for (i = 0; i < 3; i++)
    {
        int j;
        for (j = 0; j < 2; j++)
        {
            if (i == 0)
                value = vol->device_file;
            else if (i == 1)
                value = vol->label;
            else
            {
                value = vol->udi;
                value = strrchr(value, '/');
                if (value)
                    value++;
            }
            if (j == 0)
                test = g_strdup_printf(" +%s ", value);
            else
                test = g_strdup_printf(" -%s ", value);
            if (strstr(showhidelist, test))
            {
                g_free(test);
                g_free(showhidelist);
                return (j == 0);
            }
            g_free(test);
        }
    }
    g_free(showhidelist);

    // udisks no?
    if (vol->nopolicy && !xset_get_b("dev_ignore_udisks_nopolicy"))
        return false;

    // table?
    if (vol->is_table)
        return false;

    // optical
    if (vol->is_optical)
        return xset_get_b("dev_automount_optical");

    // internal?
    if (vol->is_removable && xset_get_b("dev_automount_removable"))
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
                 (unsigned int)MAJOR(vol->devnum),
                 (unsigned int)MINOR(vol->devnum));
        info = "( no udev device )";
        return info;
    }

    device_t* device = device_alloc(udevice);
    if (!device_get_info(device))
        return info;

    device_show_info(device, info);
    device_free(device);
    udev_device_unref(udevice);
    return info;
}

char*
vfs_volume_device_mount_cmd(VFSVolume* vol, const char* options, bool* run_in_terminal)
{
    char* command = nullptr;
    char* s1;
    *run_in_terminal = false;
    command = vfs_volume_handler_cmd(HANDLER_MODE_FS,
                                     HANDLER_MOUNT,
                                     vol,
                                     options,
                                     nullptr,
                                     run_in_terminal,
                                     nullptr);
    if (!command)
    {
        // discovery
        if ((s1 = g_find_program_in_path("udevil")))
        {
            // udevil
            if (options && options[0] != '\0')
                command = g_strdup_printf("%s mount %s -o '%s'", s1, vol->device_file, options);
            else
                command = g_strdup_printf("%s mount %s", s1, vol->device_file);
        }
        else if ((s1 = g_find_program_in_path("pmount")))
        {
            // pmount
            command = g_strdup_printf("%s %s", s1, vol->device_file);
        }
        else if ((s1 = g_find_program_in_path("udisksctl")))
        {
            // udisks2
            if (options && options[0] != '\0')
                command = g_strdup_printf("%s mount -b %s -o '%s'", s1, vol->device_file, options);
            else
                command = g_strdup_printf("%s mount -b %s", s1, vol->device_file);
        }
        g_free(s1);
    }
    return command;
}

char*
vfs_volume_device_unmount_cmd(VFSVolume* vol, bool* run_in_terminal)
{
    char* command = nullptr;
    char* s1;
    char* pointq;
    *run_in_terminal = false;

    netmount_t* netmount = nullptr;

    // unmounting a network ?
    switch (vol->device_type)
    {
        case DEVICE_TYPE_NETWORK:
            // is a network - try to get unmount command
            if (split_network_url(vol->udi, &netmount) == 1)
            {
                command = vfs_volume_handler_cmd(HANDLER_MODE_NET,
                                                 HANDLER_UNMOUNT,
                                                 vol,
                                                 nullptr,
                                                 netmount,
                                                 run_in_terminal,
                                                 nullptr);
                g_free(netmount->url);
                g_free(netmount->fstype);
                g_free(netmount->host);
                g_free(netmount->ip);
                g_free(netmount->port);
                g_free(netmount->user);
                g_free(netmount->pass);
                g_free(netmount->path);
                g_slice_free(netmount_t, netmount);

                // igtodo is this redundant?
                // replace mount point sub var
                if (command && strstr(command, "%a"))
                {
                    pointq = bash_quote(vol->is_mounted ? vol->mount_point : nullptr);
                    s1 = command;
                    command = replace_string(command, "%a", pointq, false);
                    g_free(s1);
                    g_free(pointq);
                }
            }
            break;

        case DEVICE_TYPE_OTHER:
            if (mtab_fstype_is_handled_by_protocol(vol->fs_type))
            {
                command = vfs_volume_handler_cmd(HANDLER_MODE_NET,
                                                 HANDLER_UNMOUNT,
                                                 vol,
                                                 nullptr,
                                                 nullptr,
                                                 run_in_terminal,
                                                 nullptr);
                // igtodo is this redundant?
                // replace mount point sub var
                if (command && strstr(command, "%a"))
                {
                    pointq = bash_quote(vol->is_mounted ? vol->mount_point : nullptr);
                    s1 = command;
                    command = replace_string(command, "%a", pointq, false);
                    g_free(s1);
                    g_free(pointq);
                }
            }
            break;
        default:
            command = vfs_volume_handler_cmd(HANDLER_MODE_FS,
                                             HANDLER_UNMOUNT,
                                             vol,
                                             nullptr,
                                             netmount,
                                             run_in_terminal,
                                             nullptr);
            break;
    }

    if (!command)
    {
        // discovery
        pointq = bash_quote(vol->device_type == DEVICE_TYPE_BLOCK || !vol->is_mounted
                                ? vol->device_file
                                : vol->mount_point);
        if ((s1 = g_find_program_in_path("udevil")))
        {
            // udevil
            command = g_strdup_printf("%s umount %s", s1, pointq);
        }
        else if ((s1 = g_find_program_in_path("pumount")))
        {
            // pmount
            command = g_strdup_printf("%s %s", s1, pointq);
        }
        else if ((s1 = g_find_program_in_path("udisksctl")))
        {
            // udisks2
            command = g_strdup_printf("%s unmount -b %s", s1, pointq);
        }
        g_free(pointq);
        g_free(s1);
        *run_in_terminal = false;
    }
    return command;
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
    for (unsigned int i = 0; i < strlen(options); i++)
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
    char* opts = g_strdup_printf(",%s,", news);
    const char* fstype = vfs_volume_get_fstype(vol);
    char newo[16384];
    newo[0] = ',';
    newo[1] = '\0';
    char* newoptr = newo + 1;
    char* ptr = opts + 1;
    char* comma;
    char* single;
    char* singlefs;
    char* plus;
    char* test;
    while (ptr[0] != '\0')
    {
        comma = strchr(ptr, ',');
        single = g_strndup(ptr, comma - ptr);
        if (!strchr(single, '+') && !strchr(single, '-'))
        {
            // pure option, check for -fs option
            test = g_strdup_printf(",%s-%s,", single, fstype);
            if (!strstr(opts, test))
            {
                // add option
                strncpy(newoptr, single, sizeof(char));
                newoptr = newo + strlen(newo);
                newoptr[0] = ',';
                newoptr[1] = '\0';
                newoptr++;
            }
            g_free(test);
        }
        else if ((plus = strchr(single, '+')))
        {
            // opt+fs
            plus[0] = '\0'; // set single to just option
            singlefs = g_strdup_printf("%s+%s", single, fstype);
            plus[0] = '+'; // restore single to option+fs
            if (!strcmp(singlefs, single))
            {
                // correct fstype, check if already in options
                plus[0] = '\0'; // set single to just option
                test = g_strdup_printf(",%s,", single);
                if (!strstr(newo, test))
                {
                    // add +fs option
                    strncpy(newoptr, single, sizeof(char));
                    newoptr = newo + strlen(newo);
                    newoptr[0] = ',';
                    newoptr[1] = '\0';
                    newoptr++;
                }
                g_free(test);
            }
            g_free(singlefs);
        }
        g_free(single);
        ptr = comma + 1;
    }
    newoptr--;
    newoptr[0] = '\0';
    g_free(opts);
    if (newo[1] == '\0')
        return nullptr;
    else
        return g_strdup(newo + 1);
}

char*
vfs_volume_get_mount_command(VFSVolume* vol, char* default_options, bool* run_in_terminal)
{
    char* options = vfs_volume_get_mount_options(vol, default_options);
    char* command = vfs_volume_device_mount_cmd(vol, options, run_in_terminal);
    g_free(options);
    return command;
}

static void
exec_task(const char* command, bool run_in_terminal)
{ // run command as async task with optional terminal
    if (!(command && command[0]))
        return;

    GList* files = g_list_prepend(nullptr, g_strdup("exec_task"));

    PtkFileTask* task = ptk_file_task_new(VFS_FILE_TASK_EXEC, files, "/", nullptr, nullptr);
    task->task->exec_action = g_strdup("exec_task");
    task->task->exec_command = g_strdup(command);
    task->task->exec_sync = false;
    task->task->exec_export = false;
    task->task->exec_terminal = run_in_terminal;
    ptk_file_task_run(task);
}

static void
vfs_volume_exec(VFSVolume* vol, const char* command)
{
    // LOG_INFO("vfs_volume_exec {} {}", vol->device_file, command);
    if (!(command && command[0]) || vol->device_type != DEVICE_TYPE_BLOCK)
        return;

    char* s1 = replace_string(command, "%m", vol->mount_point, true);
    char* s2 = replace_string(s1, "%l", vol->label, true);
    g_free(s1);
    s1 = replace_string(s2, "%v", vol->device_file, false);
    g_free(s2);

    LOG_INFO("Autoexec: {}", s1);
    exec_task(s1, false);
    g_free(s1);
}

static void
vfs_volume_autoexec(VFSVolume* vol)
{
    char* command = nullptr;

    // Note: audiocd is is_mountable
    if (!vol->is_mountable || global_inhibit_auto || vol->device_type != DEVICE_TYPE_BLOCK ||
        !vfs_volume_is_automount(vol))
        return;

    if (vol->is_audiocd)
    {
        command = xset_get_s("dev_exec_audio");
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
                command = xset_get_s("dev_exec_video");
            else
            {
                if (xset_get_b("dev_auto_open"))
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
                        //                                        PTK_OPEN_DIR ); //PTK_OPEN_NEW_TAB
                        // fm_main_window_add_new_tab causes hang without GDK_THREADS_ENTER
                        fm_main_window_add_new_tab(main_window, vol->mount_point);
                        // LOG_INFO("DONE Auto Open Tab for {} in {}", vol->device_file,
                        //                                             vol->mount_point);
                    }
                    else
                    {
                        char* prog = g_find_program_in_path(g_get_prgname());
                        if (!prog)
                            prog = g_strdup(g_get_prgname());
                        if (!prog)
                            prog = g_strdup("spacefm");
                        char* quote_path = bash_quote(vol->mount_point);
                        std::string cmd = fmt::format("{} -t {}", prog, quote_path);
                        print_command(cmd);
                        g_spawn_command_line_async(cmd.c_str(), nullptr);
                        g_free(prog);
                        g_free(quote_path);
                    }
                }
                command = xset_get_s("dev_exec_fs");
            }
            g_free(path);
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
        g_free(line);
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
    char* line =
        vfs_volume_get_mount_command(vol, xset_get_s("dev_mount_options"), &run_in_terminal);
    if (line)
    {
        LOG_INFO("Automount: {}", line);
        exec_task(line, run_in_terminal);
        g_free(line);
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
    GList* l;
    for (l = volumes; l; l = l->next)
    {
        if ((VFS_VOLUME(l->data))->devnum == volume->devnum)
        {
            // update existing volume
            bool was_mounted = (VFS_VOLUME(l->data))->is_mounted;
            bool was_audiocd = (VFS_VOLUME(l->data))->is_audiocd;
            bool was_mountable = (VFS_VOLUME(l->data))->is_mountable;

            // detect changed mount point
            if (!was_mounted && volume->is_mounted)
                changed_mount_point = g_strdup(volume->mount_point);
            else if (was_mounted && !volume->is_mounted)
                changed_mount_point = g_strdup((VFS_VOLUME(l->data))->mount_point);

            vfs_free_volume_members(VFS_VOLUME(l->data));
            (VFS_VOLUME(l->data))->udi = g_strdup(volume->udi);
            (VFS_VOLUME(l->data))->device_file = g_strdup(volume->device_file);
            (VFS_VOLUME(l->data))->label = g_strdup(volume->label);
            (VFS_VOLUME(l->data))->mount_point = g_strdup(volume->mount_point);
            (VFS_VOLUME(l->data))->icon = g_strdup(volume->icon);
            (VFS_VOLUME(l->data))->disp_name = g_strdup(volume->disp_name);
            (VFS_VOLUME(l->data))->is_mounted = volume->is_mounted;
            (VFS_VOLUME(l->data))->is_mountable = volume->is_mountable;
            (VFS_VOLUME(l->data))->is_optical = volume->is_optical;
            (VFS_VOLUME(l->data))->requires_eject = volume->requires_eject;
            (VFS_VOLUME(l->data))->is_removable = volume->is_removable;
            (VFS_VOLUME(l->data))->is_user_visible = volume->is_user_visible;
            (VFS_VOLUME(l->data))->size = volume->size;
            (VFS_VOLUME(l->data))->is_table = volume->is_table;
            (VFS_VOLUME(l->data))->is_floppy = volume->is_floppy;
            (VFS_VOLUME(l->data))->nopolicy = volume->nopolicy;
            (VFS_VOLUME(l->data))->fs_type = volume->fs_type;
            (VFS_VOLUME(l->data))->is_blank = volume->is_blank;
            (VFS_VOLUME(l->data))->is_audiocd = volume->is_audiocd;
            (VFS_VOLUME(l->data))->is_dvd = volume->is_dvd;

            // Mount and ejection detect for automount
            if (volume->is_mounted)
            {
                (VFS_VOLUME(l->data))->ever_mounted = true;
                (VFS_VOLUME(l->data))->automount_time = 0;
            }
            else
            {
                if (volume->is_removable && !volume->is_mountable) // ejected
                {
                    (VFS_VOLUME(l->data))->ever_mounted = false;
                    (VFS_VOLUME(l->data))->automount_time = 0;
                    (VFS_VOLUME(l->data))->inhibit_auto = false;
                }
            }

            call_callbacks(VFS_VOLUME(l->data), VFS_VOLUME_CHANGED);

            vfs_free_volume_members(volume);
            g_slice_free(VFSVolume, volume);

            volume = VFS_VOLUME(l->data);
            if (automount)
            {
                vfs_volume_automount(volume);
                if (!was_mounted && volume->is_mounted)
                    vfs_volume_autoexec(volume);
                else if (was_mounted && !volume->is_mounted)
                {
                    vfs_volume_exec(volume, xset_get_s("dev_exec_unmount"));
                    volume->should_autounmount = false;
                    // remove mount points in case other unmounted
                    ptk_location_view_clean_mount_points();
                }
                else if (!was_audiocd && volume->is_audiocd)
                    vfs_volume_autoexec(volume);

                // media inserted ?
                if (!was_mountable && volume->is_mountable)
                    vfs_volume_exec(volume, xset_get_s("dev_exec_insert"));

                // media ejected ?
                if (was_mountable && !volume->is_mountable && volume->is_mounted &&
                    (volume->is_optical || volume->is_removable))
                    unmount_if_mounted(volume);
            }
            // refresh tabs containing changed mount point
            if (changed_mount_point)
            {
                main_window_refresh_all_tabs_matching(changed_mount_point);
                g_free(changed_mount_point);
            }
            return;
        }
    }

    // add as new volume
    volumes = g_list_append(volumes, volume);
    call_callbacks(volume, VFS_VOLUME_ADDED);
    if (automount)
    {
        vfs_volume_automount(volume);
        vfs_volume_exec(volume, xset_get_s("dev_exec_insert"));
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
    GList* l;
    VFSVolume* volume;

    for (l = volumes; l; l = l->next)
    {
        if ((VFS_VOLUME(l->data))->device_type != DEVICE_TYPE_BLOCK &&
            (VFS_VOLUME(l->data))->devnum == devnum)
        {
            // remove volume
            volume = VFS_VOLUME(l->data);
            LOG_INFO("special mount removed: {} ({}:{}) on {}",
                     volume->device_file,
                     (unsigned int)MAJOR(volume->devnum),
                     (unsigned int)MINOR(volume->devnum),
                     volume->mount_point);
            volumes = g_list_remove(volumes, volume);
            call_callbacks(volume, VFS_VOLUME_REMOVED);
            vfs_free_volume_members(volume);
            g_slice_free(VFSVolume, volume);
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

    GList* l;
    VFSVolume* volume;
    for (l = volumes; l; l = l->next)
    {
        if ((VFS_VOLUME(l->data))->device_type == DEVICE_TYPE_BLOCK &&
            (VFS_VOLUME(l->data))->devnum == devnum)
        {
            // remove volume
            volume = VFS_VOLUME(l->data);
            // LOG_INFO("remove volume {}", volume->device_file);
            vfs_volume_exec(volume, xset_get_s("dev_exec_remove"));
            if (volume->is_mounted && volume->is_removable)
                unmount_if_mounted(volume);
            volumes = g_list_remove(volumes, volume);
            call_callbacks(volume, VFS_VOLUME_REMOVED);
            if (volume->is_mounted && volume->mount_point)
                main_window_refresh_all_tabs_matching(volume->mount_point);
            vfs_free_volume_members(volume);
            g_slice_free(VFSVolume, volume);
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

    char* line = g_strdup_printf("grep -qs '^%s ' %s 2>/dev/nullptr || exit\n%s\n",
                                 vol->device_file,
                                 mtab,
                                 str);
    g_free(str);
    g_free(mtab_path);
    LOG_INFO("Unmount-If-Mounted: {}", line);
    exec_task(line, run_in_terminal);
    g_free(line);
}

static bool
on_cancel_inhibit_timer(void* user_data)
{
    global_inhibit_auto = false;
    return false;
}

bool
vfs_volume_init()
{
    struct udev_device* udevice;
    struct udev_enumerate* enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    VFSVolume* volume;

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
                if ((volume = vfs_volume_read_by_device(udevice)))
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
        goto finish_;
    }
    if (udev_monitor_enable_receiving(umonitor))
    {
        LOG_INFO("cannot enable udev monitor receiving");
        goto finish_;
    }
    if (udev_monitor_filter_add_match_subsystem_devtype(umonitor, "block", nullptr))
    {
        LOG_INFO("cannot set udev filter");
        goto finish_;
    }

    int ufd;
    ufd = udev_monitor_get_fd(umonitor);
    if (ufd == 0)
    {
        LOG_INFO("scannot get udev monitor socket file descriptor");
        goto finish_;
    }
    global_inhibit_auto = true; // don't autoexec during startup

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
    GList* l;
    for (l = volumes; l; l = l->next)
        vfs_volume_automount(VFS_VOLUME(l->data));

    // start resume autoexec timer
    g_timeout_add_seconds(3, (GSourceFunc)on_cancel_inhibit_timer, nullptr);

    return true;
finish_:
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

bool
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
    GList* l;
    bool unmount_all = xset_get_b("dev_unmount_quit");
    if (G_LIKELY(volumes))
    {
        for (l = volumes; l; l = l->next)
        {
            if (unmount_all)
                vfs_volume_autounmount(VFS_VOLUME(l->data));
            vfs_free_volume_members(VFS_VOLUME(l->data));
            g_slice_free(VFSVolume, l->data);
        }
    }
    volumes = nullptr;

    // remove unused mount points
    ptk_location_view_clean_mount_points();

    return true;
}

const GList*
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

    if (G_LIKELY(volumes))
    {
        GList* l;
        for (l = volumes; l; l = l->next)
        {
            VFSVolume* vol = VFS_VOLUME(l->data);
            if (device_file && !strcmp(device_file, vol->device_file))
                return vol;
            if (point && vol->is_mounted && vol->mount_point)
            {
                if (!strcmp(point, vol->mount_point) || (canon && !strcmp(canon, vol->mount_point)))
                    return vol;
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
                          "evt_device",
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
    if (!vol->icon)
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
            int len = strlen(fstype);
            char* ptr;
            if ((ptr = xset_get_s("dev_change")))
            {
                while (ptr[0])
                {
                    while (ptr[0] == ' ' || ptr[0] == ',')
                        ptr++;
                    if (g_str_has_prefix(ptr, fstype) &&
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
        ret = (    g_str_has_prefix( fstype, "nfs" )    // nfs nfs4 etc
                || ( g_str_has_prefix( fstype, "fuse" ) &&
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
    char* native_path = g_strdup(udev_device_get_syspath(udevice));
    udev_device_unref(udevice);

    if (!native_path || !sysfs_file_exists(native_path, "start"))
    {
        // not a partition if no "start"
        g_free(native_path);
        return 0;
    }

    char* parent = g_path_get_dirname(native_path);
    g_free(native_path);
    udevice = udev_device_new_from_syspath(udev, parent);
    g_free(parent);
    if (!udevice)
        return 0;
    dev_t retdev = udev_device_get_devnum(udevice);
    udev_device_unref(udevice);
    return retdev;
}
