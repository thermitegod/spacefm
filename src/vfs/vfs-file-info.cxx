/**
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

#include <string>
#include <vector>

#include <grp.h>
#include <pwd.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-file-info.hxx"

static int big_thumb_size = 48, small_thumb_size = 20;

VFSFileInfo*
vfs_file_info_new()
{
    VFSFileInfo* fi = g_slice_new0(VFSFileInfo);
    fi->ref_inc();
    return fi;
}

static void
vfs_file_info_clear(VFSFileInfo* fi)
{
    if (!fi->disp_name.empty() && !ztd::same(fi->disp_name, fi->name))
        fi->disp_name.clear();
    if (!fi->name.empty())
        fi->name.clear();
    if (!fi->collate_key.empty())
        fi->collate_key.clear();
    if (!fi->collate_icase_key.empty())
        fi->collate_icase_key.clear();
    if (!fi->disp_size.empty())
        fi->disp_size.clear();
    if (!fi->disp_owner.empty())
        fi->disp_owner.clear();
    if (!fi->disp_mtime.empty())
        fi->disp_mtime.clear();
    if (!fi->disp_perm.empty())
        fi->disp_perm.clear();
    if (fi->big_thumbnail)
    {
        g_object_unref(fi->big_thumbnail);
        fi->big_thumbnail = nullptr;
    }
    if (fi->small_thumbnail)
    {
        g_object_unref(fi->small_thumbnail);
        fi->small_thumbnail = nullptr;
    }
    if (fi->mime_type)
    {
        vfs_mime_type_unref(fi->mime_type);
        fi->mime_type = nullptr;
    }
    fi->flags = VFS_FILE_INFO_NONE;
}

VFSFileInfo*
vfs_file_info_ref(VFSFileInfo* fi)
{
    fi->ref_inc();
    return fi;
}

void
vfs_file_info_unref(VFSFileInfo* fi)
{
    fi->ref_dec();
    if (fi->ref_count() == 0)
    {
        vfs_file_info_clear(fi);
        g_slice_free(VFSFileInfo, fi);
    }
}

bool
vfs_file_info_get(VFSFileInfo* fi, const char* file_path, const char* base_name)
{
    struct stat file_stat;
    vfs_file_info_clear(fi);

    if (base_name)
    {
        fi->name = base_name;
        fi->disp_name = base_name;
    }
    else
    {
        fi->name = g_path_get_basename(file_path);
        fi->disp_name = g_path_get_basename(file_path);
    }

    if (lstat(file_path, &file_stat) == 0)
    {
        // LOG_INFO("VFSFileInfo {}", fi->name);
        /* This is time-consuming but can save much memory */
        fi->mode = file_stat.st_mode;
        fi->dev = file_stat.st_dev;
        fi->uid = file_stat.st_uid;
        fi->gid = file_stat.st_gid;
        fi->size = file_stat.st_size;
        // LOG_INFO("size {} {}", fi->name, fi->size);
        fi->mtime = file_stat.st_mtime;
        fi->atime = file_stat.st_atime;
        fi->blksize = file_stat.st_blksize;
        fi->blocks = file_stat.st_blocks;

        fi->disp_name = fi->name;

        fi->mime_type = vfs_mime_type_get_from_file(file_path, fi->disp_name.c_str(), &file_stat);
        // sfm get collate keys
        fi->collate_key = g_utf8_collate_key_for_filename(fi->disp_name.c_str(), -1);
        std::string str = g_utf8_casefold(fi->disp_name.c_str(), -1);
        fi->collate_icase_key = g_utf8_collate_key_for_filename(str.c_str(), -1);
        return true;
    }
    else
        fi->mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
    return false;
}

const char*
vfs_file_info_get_name(VFSFileInfo* fi)
{
    return fi->name.c_str();
}

/* Get displayed name encoded in UTF-8 */
const char*
vfs_file_info_get_disp_name(VFSFileInfo* fi)
{
    return fi->disp_name.c_str();
}

void
vfs_file_info_set_disp_name(VFSFileInfo* fi, const char* name)
{
    if (fi->disp_name.c_str() != fi->name.c_str())
        fi->disp_name.clear();

    fi->disp_name = name;
    // sfm get new collate keys
    fi->collate_key = g_utf8_collate_key_for_filename(fi->disp_name.c_str(), -1);
    std::string str = g_utf8_casefold(fi->disp_name.c_str(), -1);
    fi->collate_icase_key = g_utf8_collate_key_for_filename(str.c_str(), -1);
}

off_t
vfs_file_info_get_size(VFSFileInfo* fi)
{
    return fi->size;
}

const char*
vfs_file_info_get_disp_size(VFSFileInfo* fi)
{
    if (fi->disp_size.empty())
    {
        std::string size_str = vfs_file_size_to_string_format(fi->size, true);
        fi->disp_size = size_str;
    }
    return fi->disp_size.c_str();
}

off_t
vfs_file_info_get_blocks(VFSFileInfo* fi)
{
    return fi->blocks;
}

VFSMimeType*
vfs_file_info_get_mime_type(VFSFileInfo* fi)
{
    vfs_mime_type_ref(fi->mime_type);
    return fi->mime_type;
}

void
vfs_file_info_reload_mime_type(VFSFileInfo* fi, const char* full_path)
{
    VFSMimeType* old_mime_type;
    struct stat file_stat;

    /* convert VFSFileInfo to struct stat */
    /* In current implementation, only st_mode is used in
       mime-type detection, so let's save some CPU cycles
       and don't copy unused fields.
    */
    file_stat.st_mode = fi->mode;

    old_mime_type = fi->mime_type;
    fi->mime_type = vfs_mime_type_get_from_file(full_path, fi->name.c_str(), &file_stat);
    vfs_file_info_load_special_info(fi, full_path);
    vfs_mime_type_unref(old_mime_type); /* FIXME: is vfs_mime_type_unref needed ?*/
}

const char*
vfs_file_info_get_mime_type_desc(VFSFileInfo* fi)
{
    return vfs_mime_type_get_description(fi->mime_type);
}

GdkPixbuf*
vfs_file_info_get_big_icon(VFSFileInfo* fi)
{
    /* get special icons for special files, especially for
       some desktop icons */

    if (G_UNLIKELY(fi->flags != VFS_FILE_INFO_NONE))
    {
        int w, h;
        int icon_size;
        vfs_mime_type_get_icon_size(&icon_size, nullptr);
        if (fi->big_thumbnail)
        {
            w = gdk_pixbuf_get_width(fi->big_thumbnail);
            h = gdk_pixbuf_get_height(fi->big_thumbnail);
        }
        else
            w = h = 0;

        if (std::abs(std::max(w, h) - icon_size) > 2)
        {
            char* icon_name = nullptr;
            if (fi->big_thumbnail)
            {
                icon_name = (char*)g_object_steal_data(G_OBJECT(fi->big_thumbnail), "name");
                g_object_unref(fi->big_thumbnail);
                fi->big_thumbnail = nullptr;
            }
            if (G_LIKELY(icon_name))
            {
                if (G_UNLIKELY(icon_name[0] == '/'))
                    fi->big_thumbnail = gdk_pixbuf_new_from_file(icon_name, nullptr);
                else
                    fi->big_thumbnail =
                        vfs_load_icon(gtk_icon_theme_get_default(), icon_name, icon_size);
            }
            if (fi->big_thumbnail)
                g_object_set_data_full(G_OBJECT(fi->big_thumbnail), "name", icon_name, g_free);
            else
                g_free(icon_name);
        }
        return fi->big_thumbnail ? g_object_ref(fi->big_thumbnail) : nullptr;
    }
    if (G_UNLIKELY(!fi->mime_type))
        return nullptr;
    return vfs_mime_type_get_icon(fi->mime_type, true);
}

GdkPixbuf*
vfs_file_info_get_small_icon(VFSFileInfo* fi)
{
    if (fi->flags & VFS_FILE_INFO_DESKTOP_ENTRY && fi->small_thumbnail) // sfm
        return g_object_ref(fi->small_thumbnail);

    if (G_UNLIKELY(!fi->mime_type))
        return nullptr;
    return vfs_mime_type_get_icon(fi->mime_type, false);
}

GdkPixbuf*
vfs_file_info_get_big_thumbnail(VFSFileInfo* fi)
{
    return fi->big_thumbnail ? g_object_ref(fi->big_thumbnail) : nullptr;
}

GdkPixbuf*
vfs_file_info_get_small_thumbnail(VFSFileInfo* fi)
{
    return fi->small_thumbnail ? g_object_ref(fi->small_thumbnail) : nullptr;
}

const char*
vfs_file_info_get_disp_owner(VFSFileInfo* fi)
{
    struct passwd* puser;
    struct group* pgroup;

    std::string user_name;
    std::string group_name;

    /* FIXME: user names should be cached */
    if (fi->disp_owner.empty())
    {
        puser = getpwuid(fi->uid);
        if (puser && puser->pw_name && *puser->pw_name)
            user_name = puser->pw_name;
        else
            user_name = fmt::format("{}", fi->uid);

        pgroup = getgrgid(fi->gid);
        if (pgroup && pgroup->gr_name && *pgroup->gr_name)
            group_name = pgroup->gr_name;
        else
            group_name = fmt::format("{}", fi->gid);

        std::string str = fmt::format("{}:{}", user_name, group_name);
        fi->disp_owner = str;
    }
    return fi->disp_owner.c_str();
}

const char*
vfs_file_info_get_disp_mtime(VFSFileInfo* fi)
{
    if (fi->disp_mtime.empty())
    {
        char buf[64];
        strftime(buf,
                 sizeof(buf),
                 app_settings.date_format.c_str(), //"%Y-%m-%d %H:%M",
                 localtime(&fi->mtime));
        fi->disp_mtime = buf;
    }
    return fi->disp_mtime.c_str();
}

time_t*
vfs_file_info_get_mtime(VFSFileInfo* fi)
{
    return &fi->mtime;
}

time_t*
vfs_file_info_get_atime(VFSFileInfo* fi)
{
    return &fi->atime;
}

static void
get_file_perm_string(std::string& perm, mode_t mode)
{
    // Special Permissions
    if (S_ISREG(mode))
        perm.append("-");
    else if (S_ISDIR(mode))
        perm.append("d");
    else if (S_ISLNK(mode))
        perm.append("l");
    else if (G_UNLIKELY(S_ISCHR(mode)))
        perm.append("c");
    else if (G_UNLIKELY(S_ISBLK(mode)))
        perm.append("b");
    else if (G_UNLIKELY(S_ISFIFO(mode)))
        perm.append("p");
    else if (G_UNLIKELY(S_ISSOCK(mode)))
        perm.append("s");
    else
        perm.append("-");

    // Owner
    if (mode & S_IRUSR)
        perm.append("r");
    else
        perm.append("-");
    if (mode & S_IWUSR)
        perm.append("w");
    else
        perm.append("-");

    if (G_UNLIKELY(mode & S_ISUID))
    {
        if (G_LIKELY(mode & S_IXUSR))
            perm.append("s");
        else
            perm.append("S");
    }
    else
    {
        if (mode & S_IXUSR)
            perm.append("x");
        else
            perm.append("-");
    }

    // Group
    if (mode & S_IRGRP)
        perm.append("r");
    else
        perm.append("-");
    if (mode & S_IWGRP)
        perm.append("w");
    else
        perm.append("-");
    if (G_UNLIKELY(mode & S_ISGID))
    {
        if (G_LIKELY(mode & S_IXGRP))
            perm.append("s");
        else
            perm.append("S");
    }
    else
    {
        if (mode & S_IXGRP)
            perm.append("x");
        else
            perm.append("-");
    }

    // Other
    if (mode & S_IROTH)
        perm.append("r");
    else
        perm.append("-");
    if (mode & S_IWOTH)
        perm.append("w");
    else
        perm.append("-");
    if (G_UNLIKELY(mode & S_ISVTX))
    {
        if (G_LIKELY(mode & S_IXOTH))
            perm.append("t");
        else
            perm.append("T");
    }
    else
    {
        if (mode & S_IXOTH)
            perm.append("x");
        else
            perm.append("-");
    }
}

const char*
vfs_file_info_get_disp_perm(VFSFileInfo* fi)
{
    if (fi->disp_perm.empty())
        get_file_perm_string(fi->disp_perm, fi->mode);

    return fi->disp_perm.c_str();
}

bool
vfs_file_info_is_dir(VFSFileInfo* fi)
{
    if (S_ISDIR(fi->mode))
        return true;
    if (S_ISLNK(fi->mode) &&
        !strcmp(vfs_mime_type_get_type(fi->mime_type), XDG_MIME_TYPE_DIRECTORY))
    {
        return true;
    }
    return false;
}

bool
vfs_file_info_is_regular_file(VFSFileInfo* fi)
{
    return S_ISREG(fi->mode) ? true : false;
}

bool
vfs_file_info_is_symlink(VFSFileInfo* fi)
{
    return S_ISLNK(fi->mode) ? true : false;
}

bool
vfs_file_info_is_socket(VFSFileInfo* fi)
{
    return S_ISSOCK(fi->mode) ? true : false;
}

bool
vfs_file_info_is_named_pipe(VFSFileInfo* fi)
{
    return S_ISFIFO(fi->mode) ? true : false;
}

bool
vfs_file_info_is_block_device(VFSFileInfo* fi)
{
    return S_ISBLK(fi->mode) ? true : false;
}

bool
vfs_file_info_is_char_device(VFSFileInfo* fi)
{
    return S_ISCHR(fi->mode) ? true : false;
}

bool
vfs_file_info_is_image(VFSFileInfo* fi)
{
    /* FIXME: We had better use functions of xdg_mime to check this */
    if (!strncmp("image/", vfs_mime_type_get_type(fi->mime_type), 6))
        return true;
    return false;
}

bool
vfs_file_info_is_video(VFSFileInfo* fi)
{
    /* FIXME: We had better use functions of xdg_mime to check this */
    if (!strncmp("video/", vfs_mime_type_get_type(fi->mime_type), 6))
        return true;
    return false;
}

bool
vfs_file_info_is_desktop_entry(VFSFileInfo* fi)
{
    return 0 != (fi->flags & VFS_FILE_INFO_DESKTOP_ENTRY);
}

bool
vfs_file_info_is_unknown_type(VFSFileInfo* fi)
{
    if (!strcmp(XDG_MIME_TYPE_UNKNOWN, vfs_mime_type_get_type(fi->mime_type)))
        return true;
    return false;
}

/* full path of the file is required by this function */
bool
vfs_file_info_is_executable(VFSFileInfo* fi, const char* file_path)
{
    return mime_type_is_executable_file(file_path, fi->mime_type->type);
}

/* full path of the file is required by this function */
bool
vfs_file_info_is_text(VFSFileInfo* fi, const char* file_path)
{
    return mime_type_is_text_file(file_path, fi->mime_type->type);
}

/*
 * Run default action of specified file.
 * Full path of the file is required by this function.
 */
bool
vfs_file_info_open_file(VFSFileInfo* fi, const char* file_path, GError** err)
{
    bool ret = false;

    if (vfs_file_info_is_executable(fi, file_path))
    {
        std::string command = fmt::format("{}", file_path);
        Glib::spawn_command_line_async(command);
    }
    else
    {
        VFSMimeType* mime_type = vfs_file_info_get_mime_type(fi);
        char* app_name = vfs_mime_type_get_default_action(mime_type);
        if (app_name)
        {
            VFSAppDesktop desktop(app_name);

            std::string open_file = file_path;
            std::vector<std::string> open_files;
            open_files.push_back(open_file);

            // FIXME: working dir is needed
            ret = desktop.open_files(gdk_screen_get_default(), nullptr, open_files, err);
            g_free(app_name);
        }
        vfs_mime_type_unref(mime_type);
    }
    return ret;
}

mode_t
vfs_file_info_get_mode(VFSFileInfo* fi)
{
    return fi->mode;
}

bool
vfs_file_info_is_thumbnail_loaded(VFSFileInfo* fi, bool big)
{
    if (big)
        return (fi->big_thumbnail != nullptr);
    return (fi->small_thumbnail != nullptr);
}

bool
vfs_file_info_load_thumbnail(VFSFileInfo* fi, const char* full_path, bool big)
{
    if (big)
    {
        if (fi->big_thumbnail)
            return true;
    }
    else
    {
        if (fi->small_thumbnail)
            return true;
    }
    GdkPixbuf* thumbnail =
        vfs_thumbnail_load_for_file(full_path, big ? big_thumb_size : small_thumb_size, fi->mtime);
    if (G_LIKELY(thumbnail))
    {
        if (big)
            fi->big_thumbnail = thumbnail;
        else
            fi->small_thumbnail = thumbnail;
    }
    else /* fallback to mime_type icon */
    {
        if (big)
            fi->big_thumbnail = vfs_file_info_get_big_icon(fi);
        else
            fi->small_thumbnail = vfs_file_info_get_small_icon(fi);
    }
    return (thumbnail != nullptr);
}

void
vfs_file_info_set_thumbnail_size(int big, int small)
{
    big_thumb_size = big;
    small_thumb_size = small;
}

void
vfs_file_info_load_special_info(VFSFileInfo* fi, const char* file_path)
{
    /*if ( G_LIKELY(fi->type) && G_UNLIKELY(fi->type->name, "application/x-desktop") ) */
    if (ztd::same(fi->name, ".desktop"))
    {
        char* file_dir = g_path_get_dirname(file_path);

        fi->flags = (VFSFileInfoFlag)(fi->flags | VFS_FILE_INFO_DESKTOP_ENTRY);
        VFSAppDesktop desktop(file_path);

        // MOD  display real filenames of .desktop files not in desktop directory
        if (ztd::same(file_dir, vfs_user_desktop_dir()))
        {
            if (desktop.get_disp_name())
                vfs_file_info_set_disp_name(fi, desktop.get_disp_name());
        }

        if (desktop.get_icon_name())
        {
            GdkPixbuf* icon;
            int big_size, small_size;
            vfs_mime_type_get_icon_size(&big_size, &small_size);
            if (!fi->big_thumbnail)
            {
                icon = desktop.get_icon(big_size);
                if (G_LIKELY(icon))
                    fi->big_thumbnail = icon;
            }
            if (!fi->small_thumbnail)
            {
                icon = desktop.get_icon(small_size);
                if (G_LIKELY(icon))
                    fi->small_thumbnail = icon;
            }
        }
        g_free(file_dir);
    }
}

void
vfs_file_info_list_free(GList* list)
{
    g_list_foreach(list, (GFunc)vfs_file_info_unref, nullptr);
    g_list_free(list);
}

void
VFSFileInfo::ref_inc()
{
    ++n_ref;
}

void
VFSFileInfo::ref_dec()
{
    --n_ref;
}

unsigned int
VFSFileInfo::ref_count()
{
    return n_ref;
}
