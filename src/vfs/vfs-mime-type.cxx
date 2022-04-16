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
#include <filesystem>

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-file-monitor.hxx"

#include "vfs/vfs-utils.hxx"

static GHashTable* mime_hash = nullptr;
static GRWLock mime_hash_lock;

static unsigned int reload_callback_id = 0;
static GList* reload_cb = nullptr;

static int big_icon_size = 32, small_icon_size = 16;

static VFSFileMonitor** mime_caches_monitor = nullptr;

static unsigned int theme_change_notify = 0;

static void on_icon_theme_changed(GtkIconTheme* icon_theme, void* user_data);

struct VFSMimeReloadCbEnt
{
    GFreeFunc cb;
    void* user_data;
};

static bool
vfs_mime_type_reload(void* user_data)
{
    (void)user_data;
    GList* l;
    /* FIXME: process mime database reloading properly. */
    /* Remove all items in the hash table */

    g_rw_lock_writer_lock(&mime_hash_lock);
    g_hash_table_foreach_remove(mime_hash, (GHRFunc)gtk_true, nullptr);
    g_rw_lock_writer_unlock(&mime_hash_lock);

    g_source_remove(reload_callback_id);
    reload_callback_id = 0;

    // LOG_DEBUG("reload mime-types");

    /* call all registered callbacks */
    for (l = reload_cb; l; l = l->next)
    {
        VFSMimeReloadCbEnt* ent = static_cast<VFSMimeReloadCbEnt*>(l->data);
        ent->cb(ent->user_data);
    }
    return false;
}

static void
on_mime_cache_changed(VFSFileMonitor* fm, VFSFileMonitorEvent event, const char* file_name,
                      void* user_data)
{
    (void)fm;
    (void)file_name;
    MimeCache* cache = static_cast<MimeCache*>(user_data);
    switch (event)
    {
        case VFS_FILE_MONITOR_CREATE:
        case VFS_FILE_MONITOR_DELETE:
            /* NOTE: FAM sometimes generate incorrect "delete" notification for non-existent files.
             *  So if the cache is not loaded originally (the cache file is non-existent), we skip
             * it.
             */
            if (!cache->buffer)
                return;

            FMT_FALLTHROUGH;
        case VFS_FILE_MONITOR_CHANGE:
            mime_cache_reload(cache);
            // LOG_DEBUG("reload cache: {}", file_name);
            if (reload_callback_id == 0)
                reload_callback_id = g_idle_add((GSourceFunc)vfs_mime_type_reload, nullptr);
            break;
        default:
            break;
    }
}

void
vfs_mime_type_init()
{
    mime_type_init();

    /* install file alteration monitor for mime-cache */
    int n_caches;
    MimeCache** caches = mime_type_get_caches(&n_caches);
    mime_caches_monitor = g_new0(VFSFileMonitor*, n_caches);
    int i;
    for (i = 0; i < n_caches; ++i)
    {
        // MOD NOTE1  check to see if path exists - otherwise it later tries to
        //  remove nullptr fm with inotify which caused segfault
        VFSFileMonitor* fm;
        if (std::filesystem::exists(caches[i]->file_path))
            fm = vfs_file_monitor_add(caches[i]->file_path, on_mime_cache_changed, caches[i]);
        else
            fm = nullptr;
        mime_caches_monitor[i] = fm;
    }
    mime_hash = g_hash_table_new_full(g_str_hash, g_str_equal, nullptr, vfs_mime_type_unref);
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    theme_change_notify =
        g_signal_connect(theme, "changed", G_CALLBACK(on_icon_theme_changed), nullptr);
}

void
vfs_mime_type_clean()
{
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    g_signal_handler_disconnect(theme, theme_change_notify);

    /* remove file alteration monitor for mime-cache */
    int n_caches;
    MimeCache** caches = mime_type_get_caches(&n_caches);
    int i;
    for (i = 0; i < n_caches; ++i)
    {
        if (mime_caches_monitor[i]) // MOD added if !nullptr - see NOTE1 above
            vfs_file_monitor_remove(mime_caches_monitor[i], on_mime_cache_changed, caches[i]);
    }
    free(mime_caches_monitor);

    mime_type_finalize();

    g_hash_table_destroy(mime_hash);
}

VFSMimeType*
vfs_mime_type_get_from_file_name(const char* ufile_name)
{
    /* type = xdg_mime_get_mime_type_from_file_name( ufile_name ); */
    const char* type = mime_type_get_by_filename(ufile_name, nullptr);
    return vfs_mime_type_get_from_type(type);
}

VFSMimeType*
vfs_mime_type_get_from_file(const char* file_path, const char* base_name, struct stat* pstat)
{
    const char* type = mime_type_get_by_file(file_path, pstat, base_name);
    return vfs_mime_type_get_from_type(type);
}

VFSMimeType*
vfs_mime_type_get_from_type(const char* type)
{
    g_rw_lock_reader_lock(&mime_hash_lock);
    VFSMimeType* mime_type = static_cast<VFSMimeType*>(g_hash_table_lookup(mime_hash, type));
    g_rw_lock_reader_unlock(&mime_hash_lock);

    if (!mime_type)
    {
        mime_type = vfs_mime_type_new(type);
        g_rw_lock_reader_lock(&mime_hash_lock);
        g_hash_table_insert(mime_hash, mime_type->type, mime_type);
        g_rw_lock_writer_unlock(&mime_hash_lock);
    }
    vfs_mime_type_ref(mime_type);
    return mime_type;
}

VFSMimeType*
vfs_mime_type_new(const char* type_name)
{
    VFSMimeType* mime_type = g_slice_new0(VFSMimeType);
    mime_type->type = ztd::strdup(type_name);
    mime_type->ref_inc();
    return mime_type;
}

void
vfs_mime_type_ref(VFSMimeType* mime_type)
{
    mime_type->ref_inc();
}

void
vfs_mime_type_unref(void* mime_type_)
{
    VFSMimeType* mime_type = static_cast<VFSMimeType*>(mime_type_);
    mime_type->ref_dec();
    if (mime_type->ref_count() == 0)
    {
        free(mime_type->type);
        if (mime_type->big_icon)
            g_object_unref(mime_type->big_icon);
        if (mime_type->small_icon)
            g_object_unref(mime_type->small_icon);
        /* g_strfreev( mime_type->actions ); */

        g_slice_free(VFSMimeType, mime_type);
    }
}

GdkPixbuf*
vfs_mime_type_get_icon(VFSMimeType* mime_type, bool big)
{
    int size;

    if (big)
    {
        if (mime_type->big_icon) /* big icon */
            return g_object_ref(mime_type->big_icon);
        size = big_icon_size;
    }
    else /* small icon */
    {
        if (mime_type->small_icon)
            return g_object_ref(mime_type->small_icon);
        size = small_icon_size;
    }

    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    GdkPixbuf* icon = nullptr;

    if (!strcmp(mime_type->type, XDG_MIME_TYPE_DIRECTORY))
    {
        icon = vfs_load_icon(icon_theme, "gtk-directory", size);
        if (!icon)
            icon = vfs_load_icon(icon_theme, "gnome-fs-directory", size);
        if (!icon)
            icon = vfs_load_icon(icon_theme, "folder", size);
        if (big)
            mime_type->big_icon = icon;
        else
            mime_type->small_icon = icon;
        return icon ? g_object_ref(icon) : nullptr;
    }

    // get description and icon from freedesktop XML - these are fetched
    // together for performance.
    char* xml_icon = nullptr;
    char* xml_desc = mime_type_get_desc_icon(mime_type->type, nullptr, &xml_icon);
    if (xml_icon)
    {
        if (xml_icon[0])
            icon = vfs_load_icon(icon_theme, xml_icon, size);
        free(xml_icon);
    }
    if (xml_desc)
    {
        if (!mime_type->description && xml_desc[0])
            mime_type->description = xml_desc;
        else
            free(xml_desc);
    }
    if (!mime_type->description)
    {
        LOG_WARN("mime-type {} has no description (comment)", mime_type->type);
        VFSMimeType* vfs_mime = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
        if (vfs_mime)
        {
            mime_type->description = ztd::strdup(vfs_mime_type_get_description(vfs_mime));
            vfs_mime_type_unref(vfs_mime);
        }
    }

    if (!icon)
    {
        // guess icon
        const char* sep = strchr(mime_type->type, '/');
        if (sep)
        {
            char icon_name[100];
            /* convert mime-type foo/bar to foo-bar */
            strncpy(icon_name, mime_type->type, sizeof(icon_name));
            icon_name[(sep - mime_type->type)] = '-';
            /* is there an icon named foo-bar? */
            icon = vfs_load_icon(icon_theme, icon_name, size);
            if (!icon)
            {
                /* maybe we can find a legacy icon named gnome-mime-foo-bar */
                strncpy(icon_name, "gnome-mime-", sizeof(icon_name));
                strncat(icon_name, mime_type->type, (sep - mime_type->type));
                strcat(icon_name, "-");
                strcat(icon_name, sep + 1);
                icon = vfs_load_icon(icon_theme, icon_name, size);
            }
            /* try gnome-mime-foo */
            if (!icon)
            {
                icon_name[11] = '\0'; /* std::strlen("gnome-mime-") = 11 */
                strncat(icon_name, mime_type->type, (sep - mime_type->type));
                icon = vfs_load_icon(icon_theme, icon_name, size);
            }
            /* try foo-x-generic */
            if (!icon)
            {
                strncpy(icon_name, mime_type->type, (sep - mime_type->type));
                icon_name[(sep - mime_type->type)] = '\0';
                strcat(icon_name, "-x-generic");
                icon = vfs_load_icon(icon_theme, icon_name, size);
            }
        }
    }

    if (!icon)
    {
        /* prevent endless recursion of XDG_MIME_TYPE_UNKNOWN */
        if (strcmp(mime_type->type, XDG_MIME_TYPE_UNKNOWN))
        {
            /* FIXME: fallback to icon of parent mime-type */
            VFSMimeType* unknown;
            unknown = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
            icon = vfs_mime_type_get_icon(unknown, big);
            vfs_mime_type_unref(unknown);
        }
        else /* unknown */
        {
            icon = vfs_load_icon(icon_theme, "unknown", size);
        }
    }

    if (big)
        mime_type->big_icon = icon;
    else
        mime_type->small_icon = icon;
    return icon ? g_object_ref(icon) : nullptr;
}

static void
free_cached_icons(void* key, void* value, void* user_data)
{
    (void)key;
    VFSMimeType* mime_type = static_cast<VFSMimeType*>(value);
    bool big = GPOINTER_TO_INT(user_data);
    if (big)
    {
        if (mime_type->big_icon)
        {
            g_object_unref(mime_type->big_icon);
            mime_type->big_icon = nullptr;
        }
    }
    else
    {
        if (mime_type->small_icon)
        {
            g_object_unref(mime_type->small_icon);
            mime_type->small_icon = nullptr;
        }
    }
}

void
vfs_mime_type_set_icon_size(int big, int small)
{
    g_rw_lock_reader_lock(&mime_hash_lock);
    if (big != big_icon_size)
    {
        big_icon_size = big;
        /* Unload old cached icons */
        g_hash_table_foreach(mime_hash, free_cached_icons, GINT_TO_POINTER(1));
    }
    if (small != small_icon_size)
    {
        small_icon_size = small;
        /* Unload old cached icons */
        g_hash_table_foreach(mime_hash, free_cached_icons, GINT_TO_POINTER(0));
    }
    g_rw_lock_writer_unlock(&mime_hash_lock);
}

void
vfs_mime_type_get_icon_size(int* big, int* small)
{
    if (big)
        *big = big_icon_size;
    if (small)
        *small = small_icon_size;
}

const char*
vfs_mime_type_get_type(VFSMimeType* mime_type)
{
    return mime_type->type;
}

/* Get human-readable description of mime type */
const char*
vfs_mime_type_get_description(VFSMimeType* mime_type)
{
    if (!mime_type->description)
    {
        mime_type->description = mime_type_get_desc_icon(mime_type->type, nullptr, nullptr);
        if (!mime_type->description || !*mime_type->description)
        {
            LOG_WARN("mime-type {} has no description (comment)", mime_type->type);
            VFSMimeType* vfs_mime = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
            if (vfs_mime)
            {
                mime_type->description = ztd::strdup(vfs_mime_type_get_description(vfs_mime));
                vfs_mime_type_unref(vfs_mime);
            }
        }
    }
    return mime_type->description;
}

std::vector<std::string>
vfs_mime_type_get_actions(VFSMimeType* mime_type)
{
    return mime_type_get_actions(mime_type->type);
}

char*
vfs_mime_type_get_default_action(VFSMimeType* mime_type)
{
    char* def = mime_type_get_default_action(mime_type->type);

    /* FIXME:
     * If default app is not set, choose one from all availble actions.
     * Is there any better way to do this?
     * Should we put this fallback handling here, or at API of higher level?
     */
    if (!def)
    {
        std::vector<std::string> actions = mime_type_get_actions(mime_type->type);
        if (!actions.empty())
            def = ztd::strdup(actions.at(0));
    }
    return def;
}

/*
 * Set default app.desktop for specified file.
 * app can be the name of the desktop file or a command line.
 */
void
vfs_mime_type_set_default_action(VFSMimeType* mime_type, const char* desktop_id)
{
    char* cust_desktop = nullptr;
    /*
        if( ! g_str_has_suffix( desktop_id, ".desktop" ) )
            return;
    */
    vfs_mime_type_add_action(mime_type, desktop_id, &cust_desktop);
    if (cust_desktop)
        desktop_id = cust_desktop;
    mime_type_update_association(mime_type->type, desktop_id, MIME_TYPE_ACTION_DEFAULT);
    free(cust_desktop);
}

void
vfs_mime_type_remove_action(VFSMimeType* mime_type, const char* desktop_id)
{
    mime_type_update_association(mime_type->type, desktop_id, MIME_TYPE_ACTION_REMOVE);
}

/* If user-custom desktop file is created, it's returned in custom_desktop. */
void
vfs_mime_type_add_action(VFSMimeType* mime_type, const char* desktop_id, char** custom_desktop)
{
    // MOD  don't create custom desktop file if desktop_id is not a command
    if (!g_str_has_suffix(desktop_id, ".desktop"))
        mime_type_add_action(mime_type->type, desktop_id, custom_desktop);
    else if (custom_desktop) // sfm
        *custom_desktop = ztd::strdup(desktop_id);
}

static void
on_icon_theme_changed(GtkIconTheme* icon_theme, void* user_data)
{
    (void)icon_theme;
    (void)user_data;
    /* reload_mime_icons */
    g_rw_lock_reader_lock(&mime_hash_lock);

    g_hash_table_foreach(mime_hash, free_cached_icons, GINT_TO_POINTER(1));
    g_hash_table_foreach(mime_hash, free_cached_icons, GINT_TO_POINTER(0));

    g_rw_lock_writer_unlock(&mime_hash_lock);
}

GList*
vfs_mime_type_add_reload_cb(GFreeFunc cb, void* user_data)
{
    VFSMimeReloadCbEnt* ent = g_slice_new(VFSMimeReloadCbEnt);
    ent->cb = cb;
    ent->user_data = user_data;
    reload_cb = g_list_append(reload_cb, ent);
    return g_list_last(reload_cb);
}

void
vfs_mime_type_remove_reload_cb(GList* cb)
{
    g_slice_free(VFSMimeReloadCbEnt, cb->data);
    reload_cb = g_list_delete_link(reload_cb, cb);
}

char*
vfs_mime_type_locate_desktop_file(const char* dir, const char* desktop_id)
{
    return mime_type_locate_desktop_file(dir, desktop_id);
}

void
vfs_mime_type_append_action(const char* type, const char* desktop_id)
{
    mime_type_update_association(type, desktop_id, MIME_TYPE_ACTION_APPEND);
}

void
VFSMimeType::ref_inc()
{
    ++n_ref;
}

void
VFSMimeType::ref_dec()
{
    --n_ref;
}

unsigned int
VFSMimeType::ref_count()
{
    return n_ref;
}
