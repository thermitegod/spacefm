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

#include <string>
#include <filesystem>

#include <cassert>

#include <fcntl.h>

#include <glibmm.h>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-volume.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"
#include "utils.hxx"

#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-dir.hxx"

static void vfs_dir_class_init(VFSDirClass* klass);
static void vfs_dir_init(VFSDir* dir);
static void vfs_dir_finalize(GObject* obj);
static void vfs_dir_set_property(GObject* obj, unsigned int prop_id, const GValue* value,
                                 GParamSpec* pspec);
static void vfs_dir_get_property(GObject* obj, unsigned int prop_id, GValue* value,
                                 GParamSpec* pspec);

static char* gethidden(const char* path);                        // MOD added
static bool ishidden(const char* hidden, const char* file_name); // MOD added

/* constructor is private */
static VFSDir* vfs_dir_new(const char* path);

static void vfs_dir_load(VFSDir* dir);
static void* vfs_dir_load_thread(VFSAsyncTask* task, VFSDir* dir);

static void vfs_dir_monitor_callback(VFSFileMonitor* fm, VFSFileMonitorEvent event,
                                     const char* file_name, void* user_data);

static void on_mime_type_reload(void* user_data);

static void update_changed_files(void* key, void* data, void* user_data);
static bool notify_file_change(void* user_data);
static bool update_file_info(VFSDir* dir, VFSFileInfo* file);

static void on_list_task_finished(VFSAsyncTask* task, bool is_cancelled, VFSDir* dir);

enum VFSDirSignal
{
    FILE_CREATED_SIGNAL,
    FILE_DELETED_SIGNAL,
    FILE_CHANGED_SIGNAL,
    THUMBNAIL_LOADED_SIGNAL,
    FILE_LISTED_SIGNAL,
    N_SIGNALS
};

static unsigned int signals[N_SIGNALS] = {0};
static GObjectClass* parent_class = nullptr;

static GHashTable* dir_hash = nullptr;
static GList* mime_cb = nullptr;
static unsigned int change_notify_timeout = 0;
static unsigned int theme_change_notify = 0;

GType
vfs_dir_get_type()
{
    static GType type = G_TYPE_INVALID;
    if (type == G_TYPE_INVALID)
    {
        static const GTypeInfo info = {
            sizeof(VFSDirClass),
            nullptr,
            nullptr,
            (GClassInitFunc)vfs_dir_class_init,
            nullptr,
            nullptr,
            sizeof(VFSDir),
            0,
            (GInstanceInitFunc)vfs_dir_init,
            nullptr,
        };
        type = g_type_register_static(G_TYPE_OBJECT, "VFSDir", &info, (GTypeFlags)0);
    }
    return type;
}

static void
vfs_dir_class_init(VFSDirClass* klass)
{
    GObjectClass* object_class;

    object_class = (GObjectClass*)klass;
    parent_class = (GObjectClass*)g_type_class_peek_parent(klass);

    object_class->set_property = vfs_dir_set_property;
    object_class->get_property = vfs_dir_get_property;
    object_class->finalize = vfs_dir_finalize;

    /*
     * file-created is emitted when there is a new file created in the dir.
     * The param is VFSFileInfo of the newly created file.
     */
    signals[FILE_CREATED_SIGNAL] = g_signal_new("file-created",
                                                G_TYPE_FROM_CLASS(klass),
                                                G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET(VFSDirClass, file_created),
                                                nullptr,
                                                nullptr,
                                                g_cclosure_marshal_VOID__POINTER,
                                                G_TYPE_NONE,
                                                1,
                                                G_TYPE_POINTER);

    /*
     * file-deleted is emitted when there is a file deleted in the dir.
     * The param is VFSFileInfo of the newly created file.
     */
    signals[FILE_DELETED_SIGNAL] = g_signal_new("file-deleted",
                                                G_TYPE_FROM_CLASS(klass),
                                                G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET(VFSDirClass, file_deleted),
                                                nullptr,
                                                nullptr,
                                                g_cclosure_marshal_VOID__POINTER,
                                                G_TYPE_NONE,
                                                1,
                                                G_TYPE_POINTER);

    /*
     * file-changed is emitted when there is a file changed in the dir.
     * The param is VFSFileInfo of the newly created file.
     */
    signals[FILE_CHANGED_SIGNAL] = g_signal_new("file-changed",
                                                G_TYPE_FROM_CLASS(klass),
                                                G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET(VFSDirClass, file_changed),
                                                nullptr,
                                                nullptr,
                                                g_cclosure_marshal_VOID__POINTER,
                                                G_TYPE_NONE,
                                                1,
                                                G_TYPE_POINTER);

    signals[THUMBNAIL_LOADED_SIGNAL] = g_signal_new("thumbnail-loaded",
                                                    G_TYPE_FROM_CLASS(klass),
                                                    G_SIGNAL_RUN_FIRST,
                                                    G_STRUCT_OFFSET(VFSDirClass, thumbnail_loaded),
                                                    nullptr,
                                                    nullptr,
                                                    g_cclosure_marshal_VOID__POINTER,
                                                    G_TYPE_NONE,
                                                    1,
                                                    G_TYPE_POINTER);

    signals[FILE_LISTED_SIGNAL] = g_signal_new("file-listed",
                                               G_TYPE_FROM_CLASS(klass),
                                               G_SIGNAL_RUN_FIRST,
                                               G_STRUCT_OFFSET(VFSDirClass, file_listed),
                                               nullptr,
                                               nullptr,
                                               g_cclosure_marshal_VOID__BOOLEAN,
                                               G_TYPE_NONE,
                                               1,
                                               G_TYPE_BOOLEAN);
}

/* constructor */
static void
vfs_dir_init(VFSDir* dir)
{
    dir->mutex = (GMutex*)g_malloc(sizeof(GMutex));
    g_mutex_init(dir->mutex);
}

void
vfs_dir_lock(VFSDir* dir)
{
    g_mutex_lock(dir->mutex);
}

void
vfs_dir_unlock(VFSDir* dir)
{
    g_mutex_unlock(dir->mutex);
}

static void
vfs_dir_clear(VFSDir* dir)
{
    g_mutex_clear(dir->mutex);
    g_free(dir->mutex);
}

/* destructor */

static void
vfs_dir_finalize(GObject* obj)
{
    VFSDir* dir = VFS_DIR(obj);
    // LOG_INFO("vfs_dir_finalize  {}", dir->path);
    do
    {
    } while (g_source_remove_by_user_data(dir));

    if (dir->task)
    {
        g_signal_handlers_disconnect_by_func(dir->task, (void*)on_list_task_finished, dir);
        /* FIXME: should we generate a "file-list" signal to indicate the dir loading was cancelled?
         */
        // LOG_INFO("vfs_dir_finalize -> vfs_async_task_cancel");
        vfs_async_task_cancel(dir->task);
        g_object_unref(dir->task);
        dir->task = nullptr;
    }
    if (dir->monitor)
    {
        vfs_file_monitor_remove(dir->monitor, vfs_dir_monitor_callback, dir);
    }
    if (dir->path)
    {
        if (dir_hash)
        {
            g_hash_table_remove(dir_hash, dir->path);

            /* There is no VFSDir instance */
            if (g_hash_table_size(dir_hash) == 0)
            {
                g_hash_table_destroy(dir_hash);
                dir_hash = nullptr;

                vfs_mime_type_remove_reload_cb(mime_cb);
                mime_cb = nullptr;

                if (change_notify_timeout)
                {
                    g_source_remove(change_notify_timeout);
                    change_notify_timeout = 0;
                }

                g_signal_handler_disconnect(gtk_icon_theme_get_default(), theme_change_notify);
                theme_change_notify = 0;
            }
        }
        g_free(dir->path);
        g_free(dir->disp_path);
        dir->path = dir->disp_path = nullptr;
    }
    // LOG_DEBUG("dir->thumbnail_loader: {:p}", dir->thumbnail_loader);
    if (dir->thumbnail_loader)
    {
        // LOG_DEBUG("FREE THUMBNAIL LOADER IN VFSDIR");
        vfs_thumbnail_loader_free(dir->thumbnail_loader);
        dir->thumbnail_loader = nullptr;
    }

    if (dir->file_list)
    {
        g_list_foreach(dir->file_list, (GFunc)vfs_file_info_unref, nullptr);
        g_list_free(dir->file_list);
        dir->file_list = nullptr;
        dir->n_files = 0;
    }

    if (!dir->changed_files.empty())
    {
        for (VFSFileInfo* file: dir->changed_files)
        {
            vfs_file_info_unref(file);
        }
        dir->changed_files.clear();
    }

    if (dir->created_files)
    {
        g_slist_foreach(dir->created_files, (GFunc)g_free, nullptr);
        g_slist_free(dir->created_files);
        dir->created_files = nullptr;
    }

    vfs_dir_clear(dir);
    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
vfs_dir_get_property(GObject* obj, unsigned int prop_id, GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static void
vfs_dir_set_property(GObject* obj, unsigned int prop_id, const GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static GList*
vfs_dir_find_file(VFSDir* dir, const char* file_name, VFSFileInfo* file)
{
    GList* l;
    for (l = dir->file_list; l; l = l->next)
    {
        VFSFileInfo* file2 = static_cast<VFSFileInfo*>(l->data);
        if (file == file2)
            return l;
        if (ztd::same(file2->name, file_name))
            return l;
    }
    return nullptr;
}

/* signal handlers */
void
vfs_dir_emit_file_created(VFSDir* dir, const char* file_name, bool force)
{
    (void)force;
    // Ignore avoid_changes for creation of files
    // if ( !force && dir->avoid_changes )
    //    return;

    if (!strcmp(file_name, dir->path))
    {
        // Special Case: The directory itself was created?
        return;
    }

    dir->created_files = g_slist_append(dir->created_files, g_strdup(file_name));
    if (change_notify_timeout == 0)
    {
        change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                   200,
                                                   (GSourceFunc)notify_file_change,
                                                   nullptr,
                                                   nullptr);
    }
}

void
vfs_dir_emit_file_deleted(VFSDir* dir, const char* file_name, VFSFileInfo* file)
{
    if (!strcmp(file_name, dir->path))
    {
        /* Special Case: The directory itself was deleted... */
        file = nullptr;
        /* clear the whole list */
        vfs_dir_lock(dir);
        g_list_foreach(dir->file_list, (GFunc)vfs_file_info_unref, nullptr);
        g_list_free(dir->file_list);
        dir->file_list = nullptr;
        vfs_dir_unlock(dir);

        g_signal_emit(dir, signals[FILE_DELETED_SIGNAL], 0, file);
        return;
    }

    GList* l = vfs_dir_find_file(dir, file_name, file);
    if (l)
    {
        VFSFileInfo* file_found = vfs_file_info_ref(static_cast<VFSFileInfo*>(l->data));
        if (!std::count(dir->changed_files.begin(), dir->changed_files.end(), file_found))
        {
            dir->changed_files.push_back(file_found);
            if (change_notify_timeout == 0)
            {
                change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                           200,
                                                           (GSourceFunc)notify_file_change,
                                                           nullptr,
                                                           nullptr);
            }
        }
        else
            vfs_file_info_unref(file_found);
    }
}

void
vfs_dir_emit_file_changed(VFSDir* dir, const char* file_name, VFSFileInfo* file, bool force)
{
    // LOG_INFO("vfs_dir_emit_file_changed dir={} file_name={} avoid={}", dir->path, file_name,
    // dir->avoid_changes ? "true" : "false");

    if (!force && dir->avoid_changes)
        return;

    if (!strcmp(file_name, dir->path))
    {
        // Special Case: The directory itself was changed
        g_signal_emit(dir, signals[FILE_CHANGED_SIGNAL], 0, nullptr);
        return;
    }

    vfs_dir_lock(dir);

    GList* l = vfs_dir_find_file(dir, file_name, file);
    if (l)
    {
        file = vfs_file_info_ref(static_cast<VFSFileInfo*>(l->data));
        if (!std::count(dir->changed_files.begin(), dir->changed_files.end(), file))
        {
            if (force)
            {
                dir->changed_files.push_back(file);
                if (change_notify_timeout == 0)
                {
                    change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                               100,
                                                               (GSourceFunc)notify_file_change,
                                                               nullptr,
                                                               nullptr);
                }
            }
            else if (update_file_info(dir, file)) // update file info the first time
            {
                dir->changed_files.push_back(file);
                if (change_notify_timeout == 0)
                {
                    change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                               500,
                                                               (GSourceFunc)notify_file_change,
                                                               nullptr,
                                                               nullptr);
                }
                g_signal_emit(dir, signals[FILE_CHANGED_SIGNAL], 0, file);
            }
        }
        else
            vfs_file_info_unref(file);
    }

    vfs_dir_unlock(dir);
}

void
vfs_dir_emit_thumbnail_loaded(VFSDir* dir, VFSFileInfo* file)
{
    vfs_dir_lock(dir);
    GList* l = vfs_dir_find_file(dir, file->name.c_str(), file);
    if (l)
    {
        assert(file == static_cast<VFSFileInfo*>(l->data));
        file = vfs_file_info_ref(static_cast<VFSFileInfo*>(l->data));
    }
    else
        file = nullptr;
    vfs_dir_unlock(dir);

    if (file)
    {
        g_signal_emit(dir, signals[THUMBNAIL_LOADED_SIGNAL], 0, file);
        vfs_file_info_unref(file);
    }
}

/* methods */

static VFSDir*
vfs_dir_new(const char* path)
{
    VFSDir* dir = static_cast<VFSDir*>(g_object_new(VFS_TYPE_DIR, nullptr));
    dir->path = g_strdup(path);

    dir->avoid_changes = vfs_volume_dir_avoid_changes(path);
    // LOG_INFO("vfs_dir_new {}  avoid_changes={}", dir->path, dir->avoid_changes ? "true" :
    // "false");
    return dir;
}

void
on_list_task_finished(VFSAsyncTask* task, bool is_cancelled, VFSDir* dir)
{
    (void)task;
    g_object_unref(dir->task);
    dir->task = nullptr;
    g_signal_emit(dir, signals[FILE_LISTED_SIGNAL], 0, is_cancelled);
    dir->file_listed = true;
    dir->load_complete = true;
}

static char*
gethidden(const char* path) // MOD added
{
    // Read .hidden into string
    char* hidden_path = g_build_filename(path, ".hidden", nullptr);

    // test access first because open() on missing file may cause
    // long delay on nfs
    if (faccessat(0, hidden_path, R_OK, AT_EACCESS) != 0)
    {
        g_free(hidden_path);
        return nullptr;
    }

    int fd = open(hidden_path, O_RDONLY);
    g_free(hidden_path);
    if (fd != -1)
    {
        struct stat s; // skip stat
        if (fstat(fd, &s) != -1)
        {
            char* buf = static_cast<char*>(g_malloc(s.st_size + 1));
            if ((s.st_size = read(fd, buf, s.st_size)) != -1)
            {
                buf[s.st_size] = 0;
                close(fd);
                return buf;
            }
            else
                g_free(buf);
        }
        close(fd);
    }
    return nullptr;
}

static bool
ishidden(const char* hidden, const char* file_name) // MOD added
{                                                   // assumes hidden,file_name != nullptr
    char c;
    char* str = (char*)strstr(hidden, file_name);
    while (str)
    {
        if (str == hidden)
        {
            // file_name is at start of buffer
            c = hidden[strlen(file_name)];
            if (c == '\n' || c == '\0')
                return true;
        }
        else
        {
            c = str[strlen(file_name)];
            if (str[-1] == '\n' && (c == '\n' || c == '\0'))
                return true;
        }
        str = strstr(++str, file_name);
    }
    return false;
}

bool
vfs_dir_add_hidden(const char* path, const char* file_name)
{
    bool ret = true;
    char* hidden = gethidden(path);

    if (!(hidden && ishidden(hidden, file_name)))
    {
        char* buf = g_strdup_printf("%s\n", file_name);
        char* file_path = g_build_filename(path, ".hidden", nullptr);
        int fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        g_free(file_path);

        if (fd != -1)
        {
            if (write(fd, buf, strlen(buf)) == -1)
                ret = false;
            close(fd);
        }
        else
            ret = false;

        g_free(buf);
    }

    if (hidden)
        g_free(hidden);
    return ret;
}

static void
vfs_dir_load(VFSDir* dir)
{
    if (dir->path)
    {
        dir->disp_path = g_filename_display_name(dir->path);
        dir->task = vfs_async_task_new((VFSAsyncFunc)vfs_dir_load_thread, dir);
        g_signal_connect(dir->task, "finish", G_CALLBACK(on_list_task_finished), dir);
        vfs_async_task_execute(dir->task);
    }
}

static void*
vfs_dir_load_thread(VFSAsyncTask* task, VFSDir* dir)
{
    (void)task;
    const char* file_name;
    char* full_path;

    dir->file_listed = false;
    dir->load_complete = false;
    dir->xhidden_count = 0; // MOD
    if (dir->path)
    {
        /* Install file alteration monitor */
        dir->monitor = vfs_file_monitor_add(dir->path, vfs_dir_monitor_callback, dir);

        GDir* dir_content = g_dir_open(dir->path, 0, nullptr);

        if (dir_content)
        {
            // MOD  dir contains .hidden file?
            char* hidden = gethidden(dir->path);

            while (!vfs_async_task_is_cancelled(dir->task) &&
                   (file_name = g_dir_read_name(dir_content)))
            {
                full_path = g_build_filename(dir->path, file_name, nullptr);
                if (!full_path)
                    continue;

                // MOD ignore if in .hidden
                if (hidden && ishidden(hidden, file_name))
                {
                    dir->xhidden_count++;
                    continue;
                }
                VFSFileInfo* file = vfs_file_info_new();
                if (vfs_file_info_get(file, full_path, file_name))
                {
                    vfs_dir_lock(dir);

                    /* Special processing for desktop directory */
                    vfs_file_info_load_special_info(file, full_path);

                    dir->file_list = g_list_prepend(dir->file_list, file);
                    vfs_dir_unlock(dir);
                    ++dir->n_files;
                }
                else
                {
                    vfs_file_info_unref(file);
                }
                g_free(full_path);
            }
            g_dir_close(dir_content);
            if (hidden)
                g_free(hidden);
        }
    }
    return nullptr;
}

bool
vfs_dir_is_file_listed(VFSDir* dir)
{
    return dir->file_listed;
}

static bool
update_file_info(VFSDir* dir, VFSFileInfo* file)
{
    bool ret = false;

    /* FIXME: Dirty hack: steal the string to prevent memory allocation */
    std::string file_name = file->name;
    if (ztd::same(file->name, file->disp_name))
        file->disp_name.clear();
    file->name.clear();

    char* full_path = g_build_filename(dir->path, file_name.c_str(), nullptr);
    if (full_path)
    {
        if (vfs_file_info_get(file, full_path, file_name.c_str()))
        {
            ret = true;
            vfs_file_info_load_special_info(file, full_path);
        }
        else /* The file doesn't exist */
        {
            GList* l;
            l = g_list_find(dir->file_list, file);
            if (l)
            {
                dir->file_list = g_list_delete_link(dir->file_list, l);
                --dir->n_files;
                if (file)
                {
                    g_signal_emit(dir, signals[FILE_DELETED_SIGNAL], 0, file);
                    vfs_file_info_unref(file);
                }
            }
            ret = false;
        }
        g_free(full_path);
    }
    return ret;
}

static void
update_changed_files(void* key, void* data, void* user_data)
{
    (void)key;
    (void)user_data;
    VFSDir* dir = static_cast<VFSDir*>(data);

    if (!dir->changed_files.empty())
    {
        vfs_dir_lock(dir);
        for (VFSFileInfo* file: dir->changed_files)
        {
            if (update_file_info(dir, file))
            {
                g_signal_emit(dir, signals[FILE_CHANGED_SIGNAL], 0, file);
                vfs_file_info_unref(file);
            }
            // else was deleted, signaled, and unrefed in update_file_info
        }
        dir->changed_files.clear();
        vfs_dir_unlock(dir);
    }
}

static void
update_created_files(void* key, void* data, void* user_data)
{
    (void)key;
    (void)user_data;
    VFSDir* dir = static_cast<VFSDir*>(data);

    if (dir->created_files)
    {
        vfs_dir_lock(dir);
        GSList* l;
        for (l = dir->created_files; l; l = l->next)
        {
            VFSFileInfo* file;
            GList* ll;
            if (!(ll = vfs_dir_find_file(dir, (char*)l->data, nullptr)))
            {
                // file is not in dir file_list
                char* full_path = g_build_filename(dir->path, (char*)l->data, nullptr);
                file = vfs_file_info_new();
                if (vfs_file_info_get(file, full_path, nullptr))
                {
                    // add new file to dir file_list
                    vfs_file_info_load_special_info(file, full_path);
                    dir->file_list = g_list_prepend(dir->file_list, vfs_file_info_ref(file));
                    ++dir->n_files;
                    g_signal_emit(dir, signals[FILE_CREATED_SIGNAL], 0, file);
                }
                // else file doesn't exist in filesystem
                vfs_file_info_unref(file);
                g_free(full_path);
            }
            else
            {
                // file already exists in dir file_list
                file = vfs_file_info_ref(static_cast<VFSFileInfo*>(ll->data));
                if (update_file_info(dir, file))
                {
                    g_signal_emit(dir, signals[FILE_CHANGED_SIGNAL], 0, file);
                    vfs_file_info_unref(file);
                }
                // else was deleted, signaled, and unrefed in update_file_info
            }
            g_free((char*)l->data); // free file_name string
        }
        g_slist_free(dir->created_files);
        dir->created_files = nullptr;
        vfs_dir_unlock(dir);
    }
}

static bool
notify_file_change(void* user_data)
{
    (void)user_data;
    g_hash_table_foreach(dir_hash, update_changed_files, nullptr);
    g_hash_table_foreach(dir_hash, update_created_files, nullptr);
    /* remove the timeout */
    change_notify_timeout = 0;
    return false;
}

void
vfs_dir_flush_notify_cache()
{
    if (change_notify_timeout)
        g_source_remove(change_notify_timeout);
    change_notify_timeout = 0;
    g_hash_table_foreach(dir_hash, update_changed_files, nullptr);
    g_hash_table_foreach(dir_hash, update_created_files, nullptr);
}

/* Callback function which will be called when monitored events happen */
static void
vfs_dir_monitor_callback(VFSFileMonitor* fm, VFSFileMonitorEvent event, const char* file_name,
                         void* user_data)
{
    (void)fm;
    VFSDir* dir = static_cast<VFSDir*>(user_data);

    switch (event)
    {
        case VFS_FILE_MONITOR_CREATE:
            vfs_dir_emit_file_created(dir, file_name, false);
            break;
        case VFS_FILE_MONITOR_DELETE:
            vfs_dir_emit_file_deleted(dir, file_name, nullptr);
            break;
        case VFS_FILE_MONITOR_CHANGE:
            vfs_dir_emit_file_changed(dir, file_name, nullptr, false);
            break;
        default:
            LOG_WARN("Error: unrecognized file monitor signal!");
    }
}

/* called on every VFSDir when icon theme got changed */
static void
reload_icons(const char* path, VFSDir* dir, void* user_data)
{
    (void)user_data;
    GList* l;
    for (l = dir->file_list; l; l = l->next)
    {
        VFSFileInfo* fi = static_cast<VFSFileInfo*>(l->data);
        /* It's a desktop entry file */
        if (fi->flags & VFS_FILE_INFO_DESKTOP_ENTRY)
        {
            char* file_path = g_build_filename(path, fi->name.c_str(), nullptr);
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
            vfs_file_info_load_special_info(fi, file_path);
            g_free(file_path);
        }
    }
}

static void
on_theme_changed(GtkIconTheme* icon_theme, void* user_data)
{
    (void)icon_theme;
    (void)user_data;
    g_hash_table_foreach(dir_hash, (GHFunc)reload_icons, nullptr);
}

VFSDir*
vfs_dir_get_by_path_soft(const char* path)
{
    if (!dir_hash || !path)
        return nullptr;

    VFSDir* dir = static_cast<VFSDir*>(g_hash_table_lookup(dir_hash, path));
    if (dir)
        g_object_ref(dir);
    return dir;
}

VFSDir*
vfs_dir_get_by_path(const char* path)
{
    VFSDir* dir = nullptr;

    g_return_val_if_fail(path, nullptr);

    if (!dir_hash)
    {
        dir_hash = g_hash_table_new_full(g_str_hash, g_str_equal, nullptr, nullptr);
        if (theme_change_notify == 0)
            theme_change_notify = g_signal_connect(gtk_icon_theme_get_default(),
                                                   "changed",
                                                   G_CALLBACK(on_theme_changed),
                                                   nullptr);
    }
    else
    {
        dir = static_cast<VFSDir*>(g_hash_table_lookup(dir_hash, path));
    }

    if (!mime_cb)
        mime_cb = vfs_mime_type_add_reload_cb(on_mime_type_reload, nullptr);

    if (dir)
    {
        g_object_ref(dir);
    }
    else
    {
        dir = vfs_dir_new(path);
        vfs_dir_load(dir); /* asynchronous operation */
        g_hash_table_insert(dir_hash, (void*)dir->path, (void*)dir);
    }
    return dir;
}

static void
reload_mime_type(char* key, VFSDir* dir, void* user_data)
{
    (void)key;
    (void)user_data;
    GList* l;
    VFSFileInfo* file;

    if (!dir || !dir->file_list)
        return;
    vfs_dir_lock(dir);
    for (l = dir->file_list; l; l = l->next)
    {
        file = static_cast<VFSFileInfo*>(l->data);
        char* full_path = g_build_filename(dir->path, vfs_file_info_get_name(file), nullptr);
        vfs_file_info_reload_mime_type(file, full_path);
        // LOG_DEBUG("reload {}", full_path);
        g_free(full_path);
    }

    for (l = dir->file_list; l; l = l->next)
    {
        file = static_cast<VFSFileInfo*>(l->data);
        g_signal_emit(dir, signals[FILE_CHANGED_SIGNAL], 0, file);
    }
    vfs_dir_unlock(dir);
}

static void
on_mime_type_reload(void* user_data)
{
    (void)user_data;
    if (!dir_hash)
        return;
    // LOG_DEBUG("reload mime-type");
    g_hash_table_foreach(dir_hash, (GHFunc)reload_mime_type, nullptr);
}

void
vfs_dir_foreach(GHFunc func, void* user_data)
{
    if (!dir_hash)
        return;
    // LOG_DEBUG("reload mime-type");
    g_hash_table_foreach(dir_hash, (GHFunc)func, user_data);
}

void
vfs_dir_unload_thumbnails(VFSDir* dir, bool is_big)
{
    GList* l;
    VFSFileInfo* file;
    char* file_path;

    vfs_dir_lock(dir);
    if (is_big)
    {
        for (l = dir->file_list; l; l = l->next)
        {
            file = static_cast<VFSFileInfo*>(l->data);
            if (file->big_thumbnail)
            {
                g_object_unref(file->big_thumbnail);
                file->big_thumbnail = nullptr;
            }
            /* This is a desktop entry file, so the icon needs reload
                 FIXME: This is not a good way to do things, but there is no better way now.  */
            if (file->flags & VFS_FILE_INFO_DESKTOP_ENTRY)
            {
                file_path = g_build_filename(dir->path, file->name.c_str(), nullptr);
                vfs_file_info_load_special_info(file, file_path);
                g_free(file_path);
            }
        }
    }
    else
    {
        for (l = dir->file_list; l; l = l->next)
        {
            file = static_cast<VFSFileInfo*>(l->data);
            if (file->small_thumbnail)
            {
                g_object_unref(file->small_thumbnail);
                file->small_thumbnail = nullptr;
            }
            /* This is a desktop entry file, so the icon needs reload
                 FIXME: This is not a good way to do things, but there is no better way now.  */
            if (file->flags & VFS_FILE_INFO_DESKTOP_ENTRY)
            {
                file_path = g_build_filename(dir->path, file->name.c_str(), nullptr);
                vfs_file_info_load_special_info(file, file_path);
                g_free(file_path);
            }
        }
    }
    vfs_dir_unlock(dir);

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
}

// sfm added mime change timer
static unsigned int mime_change_timer = 0;
static VFSDir* mime_dir = nullptr;

static bool
on_mime_change_timer(void* user_data)
{
    (void)user_data;
    std::string command;

    // LOG_INFO("MIME-UPDATE on_timer");
    command = fmt::format("update-mime-database {}/mime", vfs_user_data_dir());
    print_command(command);
    Glib::spawn_command_line_async(command);

    command = fmt::format("update-desktop-database {}/applications", vfs_user_data_dir());
    print_command(command);
    Glib::spawn_command_line_async(command);

    g_source_remove(mime_change_timer);
    mime_change_timer = 0;
    return false;
}

static void
mime_change(void* user_data)
{
    (void)user_data;
    if (mime_change_timer)
    {
        // timer is already running, so ignore request
        // LOG_INFO("MIME-UPDATE already set");
        return;
    }
    if (mime_dir)
    {
        // update mime database in 2 seconds
        mime_change_timer = g_timeout_add_seconds(2, (GSourceFunc)on_mime_change_timer, nullptr);
        // LOG_INFO("MIME-UPDATE timer started");
    }
}

void
vfs_dir_monitor_mime()
{
    // start watching for changes
    if (mime_dir)
        return;
    std::string path = Glib::build_filename(vfs_user_data_dir(), "mime/packages", nullptr);
    if (std::filesystem::is_directory(path))
    {
        mime_dir = vfs_dir_get_by_path(path.c_str());
        if (mime_dir)
        {
            g_signal_connect(mime_dir, "file-listed", G_CALLBACK(mime_change), nullptr);
            g_signal_connect(mime_dir, "file-created", G_CALLBACK(mime_change), nullptr);
            g_signal_connect(mime_dir, "file-deleted", G_CALLBACK(mime_change), nullptr);
            g_signal_connect(mime_dir, "file-changed", G_CALLBACK(mime_change), nullptr);
        }
        // LOG_INFO("MIME-UPDATE watch started");
    }
}
