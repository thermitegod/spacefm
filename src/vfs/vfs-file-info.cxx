/*
 *  C Implementation: vfs-file-info
 *
 * Description: File information
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#include <grp.h>
#include <pwd.h>

#include "settings.hxx"

#include "vfs-app-desktop.hxx"
#include "vfs-thumbnail-loader.hxx"
#include "vfs-utils.hxx"
#include "vfs-user-dir.hxx"

#include "vfs-file-info.hxx"

static int big_thumb_size = 48, small_thumb_size = 20;
static bool utf8_file_name = false;
static const char* desktop_dir = nullptr; // MOD added

void
vfs_file_info_set_utf8_filename(bool is_utf8)
{
    utf8_file_name = is_utf8;
}

VFSFileInfo*
vfs_file_info_new()
{
    VFSFileInfo* fi = g_slice_new0(VFSFileInfo);
    fi->n_ref = 1;
    return fi;
}

static void
vfs_file_info_clear(VFSFileInfo* fi)
{
    if (fi->disp_name && fi->disp_name != fi->name)
    {
        g_free(fi->disp_name);
        fi->disp_name = nullptr;
    }
    if (fi->name)
    {
        g_free(fi->name);
        fi->name = nullptr;
    }
    if (fi->collate_key) // sfm
    {
        g_free(fi->collate_key);
        fi->collate_key = nullptr;
    }
    if (fi->collate_icase_key) // sfm
    {
        g_free(fi->collate_icase_key);
        fi->collate_icase_key = nullptr;
    }
    if (fi->disp_size)
    {
        g_free(fi->disp_size);
        fi->disp_size = nullptr;
    }
    if (fi->disp_owner)
    {
        g_free(fi->disp_owner);
        fi->disp_owner = nullptr;
    }
    if (fi->disp_mtime)
    {
        g_free(fi->disp_mtime);
        fi->disp_mtime = nullptr;
    }
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

    fi->disp_perm[0] = '\0';

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
    g_atomic_int_inc(&fi->n_ref);
    return fi;
}

void
vfs_file_info_unref(VFSFileInfo* fi)
{
    if (g_atomic_int_dec_and_test(&fi->n_ref))
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
        fi->name = g_strdup(base_name);
    else
        fi->name = g_path_get_basename(file_path);

    if (lstat(file_path, &file_stat) == 0)
    {
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

        if (G_LIKELY(utf8_file_name && g_utf8_validate(fi->name, -1, nullptr)))
        {
            fi->disp_name = fi->name; /* Don't duplicate the name and save memory */
        }
        else
        {
            fi->disp_name = g_filename_display_name(fi->name);
        }
        fi->mime_type = vfs_mime_type_get_from_file(file_path, fi->disp_name, &file_stat);
        // sfm get collate keys
        fi->collate_key = g_utf8_collate_key_for_filename(fi->disp_name, -1);
        char* str = g_utf8_casefold(fi->disp_name, -1);
        fi->collate_icase_key = g_utf8_collate_key_for_filename(str, -1);
        g_free(str);
        return true;
    }
    else
        fi->mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
    return false;
}

const char*
vfs_file_info_get_name(VFSFileInfo* fi)
{
    return fi->name;
}

/* Get displayed name encoded in UTF-8 */
const char*
vfs_file_info_get_disp_name(VFSFileInfo* fi)
{
    return fi->disp_name;
}

void
vfs_file_info_set_disp_name(VFSFileInfo* fi, const char* name)
{
    if (fi->disp_name && fi->disp_name != fi->name)
        g_free(fi->disp_name);
    fi->disp_name = g_strdup(name);
    // sfm get new collate keys
    g_free(fi->collate_key);
    g_free(fi->collate_icase_key);
    fi->collate_key = g_utf8_collate_key_for_filename(fi->disp_name, -1);
    char* str = g_utf8_casefold(fi->disp_name, -1);
    fi->collate_icase_key = g_utf8_collate_key_for_filename(str, -1);
    g_free(str);
}

off_t
vfs_file_info_get_size(VFSFileInfo* fi)
{
    return fi->size;
}

const char*
vfs_file_info_get_disp_size(VFSFileInfo* fi)
{
    if (G_UNLIKELY(!fi->disp_size))
    {
        char buf[64];
        vfs_file_size_to_string_format(buf, fi->size, true);
        fi->disp_size = g_strdup(buf);
    }
    return fi->disp_size;
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
    fi->mime_type = vfs_mime_type_get_from_file(full_path, fi->name, &file_stat);
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

        if (ABS(MAX(w, h) - icon_size) > 2)
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
    char uid_str_buf[32];
    char* user_name;
    char gid_str_buf[32];
    char* group_name;

    /* FIXME: user names should be cached */
    if (!fi->disp_owner)
    {
        puser = getpwuid(fi->uid);
        if (puser && puser->pw_name && *puser->pw_name)
            user_name = puser->pw_name;
        else
        {
            g_snprintf(uid_str_buf, sizeof(uid_str_buf), "%d", fi->uid);
            user_name = uid_str_buf;
        }

        pgroup = getgrgid(fi->gid);
        if (pgroup && pgroup->gr_name && *pgroup->gr_name)
            group_name = pgroup->gr_name;
        else
        {
            g_snprintf(gid_str_buf, sizeof(gid_str_buf), "%d", fi->gid);
            group_name = gid_str_buf;
        }
        fi->disp_owner = g_strdup_printf("%s:%s", user_name, group_name);
    }
    return fi->disp_owner;
}

const char*
vfs_file_info_get_disp_mtime(VFSFileInfo* fi)
{
    if (!fi->disp_mtime)
    {
        char buf[64];
        strftime(buf,
                 sizeof(buf),
                 app_settings.date_format.c_str(), //"%Y-%m-%d %H:%M",
                 localtime(&fi->mtime));
        fi->disp_mtime = g_strdup(buf);
    }
    return fi->disp_mtime;
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
get_file_perm_string(char* perm, mode_t mode)
{
    if (S_ISREG(mode)) // sfm
        perm[0] = '-';
    else if (S_ISDIR(mode))
        perm[0] = 'd';
    else if (S_ISLNK(mode))
        perm[0] = 'l';
    else if (G_UNLIKELY(S_ISCHR(mode)))
        perm[0] = 'c';
    else if (G_UNLIKELY(S_ISBLK(mode)))
        perm[0] = 'b';
    else if (G_UNLIKELY(S_ISFIFO(mode)))
        perm[0] = 'p';
    else if (G_UNLIKELY(S_ISSOCK(mode)))
        perm[0] = 's';
    else
        perm[0] = '-';
    perm[1] = (mode & S_IRUSR) ? 'r' : '-';
    perm[2] = (mode & S_IWUSR) ? 'w' : '-';
    if (G_UNLIKELY(mode & S_ISUID)) // sfm
    {
        if (G_LIKELY(mode & S_IXUSR))
            perm[3] = 's';
        else
            perm[3] = 'S';
    }
    else
        perm[3] = (mode & S_IXUSR) ? 'x' : '-';
    perm[4] = (mode & S_IRGRP) ? 'r' : '-';
    perm[5] = (mode & S_IWGRP) ? 'w' : '-';
    if (G_UNLIKELY(mode & S_ISGID)) // sfm
    {
        if (G_LIKELY(mode & S_IXGRP))
            perm[6] = 's';
        else
            perm[6] = 'S';
    }
    else
        perm[6] = (mode & S_IXGRP) ? 'x' : '-';
    perm[7] = (mode & S_IROTH) ? 'r' : '-';
    perm[8] = (mode & S_IWOTH) ? 'w' : '-';
    if (G_UNLIKELY(mode & S_ISVTX)) // MOD
    {
        if (G_LIKELY(mode & S_IXOTH))
            perm[9] = 't';
        else
            perm[9] = 'T';
    }
    else
        perm[9] = (mode & S_IXOTH) ? 'x' : '-';
    perm[10] = '\0';
}

const char*
vfs_file_info_get_disp_perm(VFSFileInfo* fi)
{
    if (!fi->disp_perm[0])
        get_file_perm_string(fi->disp_perm, fi->mode);
    return fi->disp_perm;
}

void
vfs_file_size_to_string_format(char* buf, uint64_t size, bool decimal)
{
    const char* unit;
    float val;

    if (size > ((uint64_t)1) << 40)
    {
        if (app_settings.use_si_prefix)
        {
            unit = "TB";
            val = ((float)size) / ((float)1000000000000);
        }
        else
        {
            unit = "TiB";
            val = ((float)size) / ((uint64_t)1 << 40);
        }
    }
    else if (size > ((uint64_t)1) << 30)
    {
        if (app_settings.use_si_prefix)
        {
            unit = "GB";
            val = ((float)size) / ((float)1000000000);
        }
        else
        {
            unit = "GiB";
            val = ((float)size) / ((uint64_t)1 << 30);
        }
    }
    else if (size > ((uint64_t)1 << 20))
    {
        if (app_settings.use_si_prefix)
        {
            unit = "MB";
            val = ((float)size) / ((float)1000000);
        }
        else
        {
            unit = "MiB";
            val = ((float)size) / ((uint64_t)1 << 20);
        }
    }
    else if (size > ((uint64_t)1 << 10))
    {
        if (app_settings.use_si_prefix)
        {
            unit = "KB";
            val = ((float)size) / ((float)1000);
        }
        else
        {
            unit = "KiB";
            val = ((float)size) / ((uint64_t)1 << 10);
        }
    }
    else
    {
        unit = "B";
        g_snprintf(buf, 64, "%u %s", (unsigned int)size, unit);
        return;
    }

    if (decimal)
        g_snprintf(buf, 64, "%.1f %s", val, unit);
    else
        g_snprintf(buf, 64, "%.0f %s", val, unit);
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
    char* argv[2];

    if (vfs_file_info_is_executable(fi, file_path))
    {
        argv[0] = (char*)file_path;
        argv[1] = nullptr;
        ret = g_spawn_async(nullptr,
                            argv,
                            nullptr,
                            GSpawnFlags(G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_SEARCH_PATH),
                            nullptr,
                            nullptr,
                            nullptr,
                            err);
    }
    else
    {
        VFSMimeType* mime_type = vfs_file_info_get_mime_type(fi);
        char* app_name = vfs_mime_type_get_default_action(mime_type);
        if (app_name)
        {
            VFSAppDesktop* app = vfs_app_desktop_new(app_name);
            if (!vfs_app_desktop_get_exec(app))
                app->exec = g_strdup(app_name); /* FIXME: app->exec */
            GList* files = nullptr;
            files = g_list_prepend(files, (void*)file_path);
            /* FIXME: working dir is needed */
            ret = vfs_app_desktop_open_files(gdk_screen_get_default(), nullptr, app, files, err);
            g_list_free(files);
            vfs_app_desktop_unref(app);
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
    if (G_UNLIKELY(g_str_has_suffix(fi->name, ".desktop")))
    {
        if (!desktop_dir)
            desktop_dir = vfs_user_desktop_dir();
        char* file_dir = g_path_get_dirname(file_path);

        fi->flags = (VFSFileInfoFlag)(fi->flags | VFS_FILE_INFO_DESKTOP_ENTRY);
        VFSAppDesktop* desktop = vfs_app_desktop_new(file_path);

        // MOD  display real filenames of .desktop files not in desktop directory
        if (desktop_dir && !strcmp(file_dir, desktop_dir))
        {
            if (vfs_app_desktop_get_disp_name(desktop))
            {
                vfs_file_info_set_disp_name(fi, vfs_app_desktop_get_disp_name(desktop));
            }
        }

        if (vfs_app_desktop_get_icon_name(desktop))
        {
            GdkPixbuf* icon;
            int big_size, small_size;
            vfs_mime_type_get_icon_size(&big_size, &small_size);
            if (!fi->big_thumbnail)
            {
                icon = vfs_app_desktop_get_icon(desktop, big_size, false);
                if (G_LIKELY(icon))
                    fi->big_thumbnail = icon;
            }
            if (!fi->small_thumbnail)
            {
                icon = vfs_app_desktop_get_icon(desktop, small_size, false);
                if (G_LIKELY(icon))
                    fi->small_thumbnail = icon;
            }
        }
        vfs_app_desktop_unref(desktop);
        g_free(file_dir);
    }
}

void
vfs_file_info_list_free(GList* list)
{
    g_list_foreach(list, (GFunc)vfs_file_info_unref, nullptr);
    g_list_free(list);
}

char*
vfs_file_resolve_path(const char* cwd, const char* relative_path)
{
    GString* ret = g_string_sized_new(4096);

    g_return_val_if_fail(G_LIKELY(relative_path), nullptr);

    int len = strlen(relative_path);
    bool strip_tail = (len == 0 || relative_path[len - 1] != '/');

    if (G_UNLIKELY(*relative_path != '/')) /* relative path */
    {
        if (G_UNLIKELY(relative_path[0] == '~')) /* home dir */
        {
            g_string_append(ret, vfs_user_home_dir());
            ++relative_path;
        }
        else
        {
            if (!cwd)
            {
                const char* cwd_new = vfs_current_dir();
                g_string_append(ret, cwd_new);
            }
            else
                g_string_append(ret, cwd);
        }
    }

    if (relative_path[0] != '/' && (ret->len == 0 || ret->str[ret->len - 1] != '/'))
        g_string_append_c(ret, '/');

    while (G_LIKELY(*relative_path))
    {
        if (G_UNLIKELY(*relative_path == '.'))
        {
            if (relative_path[1] == '/' || relative_path[1] == '\0') /* current dir */
            {
                relative_path += relative_path[1] ? 2 : 1;
                continue;
            }
            if (relative_path[1] == '.' &&
                (relative_path[2] == '/' || relative_path[2] == '\0')) /* parent dir */
            {
                unsigned long len = ret->len - 2;
                while (ret->str[len] != '/')
                    --len;
                g_string_truncate(ret, len + 1);
                relative_path += relative_path[2] ? 3 : 2;
                continue;
            }
        }

        do
        {
            g_string_append_c(ret, *relative_path);
        } while (G_LIKELY(*(relative_path++) != '/' && *relative_path));
    }

    /* if original path contains tailing '/', preserve it; otherwise, remove it. */
    if (strip_tail && G_LIKELY(ret->len > 1) && G_UNLIKELY(ret->str[ret->len - 1] == '/'))
        g_string_truncate(ret, ret->len - 1);
    return g_string_free(ret, false);
}
