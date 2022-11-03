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
#include <string_view>

#include <filesystem>

#include <map>

#include <iostream>
#include <fstream>

#include <cassert>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <fcntl.h>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <magic_enum.hpp>

#include "write.hxx"
#include "utils.hxx"

#include "vfs/vfs-volume.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-dir.hxx"

struct VFSDirClass
{
    GObjectClass parent;

    /* Default signal handlers */
    void (*file_created)(VFSDir* dir, VFSFileInfo* file);
    void (*file_deleted)(VFSDir* dir, VFSFileInfo* file);
    void (*file_changed)(VFSDir* dir, VFSFileInfo* file);
    void (*thumbnail_loaded)(VFSDir* dir, VFSFileInfo* file);
    void (*file_listed)(VFSDir* dir);
    void (*load_complete)(VFSDir* dir);
    // void (*need_reload)(VFSDir* dir);
    // void (*update_mime)(VFSDir* dir);
};

static void vfs_dir_class_init(VFSDirClass* klass);
static void vfs_dir_init(VFSDir* dir);
static void vfs_dir_finalize(GObject* obj);
static void vfs_dir_set_property(GObject* obj, unsigned int prop_id, const GValue* value,
                                 GParamSpec* pspec);
static void vfs_dir_get_property(GObject* obj, unsigned int prop_id, GValue* value,
                                 GParamSpec* pspec);

static const std::string gethidden(std::string_view path);
static bool ishidden(std::string_view hidden, std::string_view file_name);

/* constructor is private */
static VFSDir* vfs_dir_new(std::string_view path);

static void vfs_dir_load(VFSDir* dir);
static void* vfs_dir_load_thread(VFSAsyncTask* task, VFSDir* dir);

static void vfs_dir_monitor_callback(VFSFileMonitor* monitor, VFSFileMonitorEvent event,
                                     const char* file_name, void* user_data);

static void on_mime_type_reload(void* user_data);

static bool notify_file_change(void* user_data);
static bool update_file_info(VFSDir* dir, VFSFileInfo* file);

static void on_list_task_finished(VFSDir* dir, bool is_cancelled);

static GObjectClass* parent_class = nullptr;

static std::map<const char*, VFSDir*> dir_map;

static GList* mime_cb = nullptr;
static unsigned int change_notify_timeout = 0;

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
        type = g_type_register_static(G_TYPE_OBJECT, "VFSDir", &info, GTypeFlags::G_TYPE_FLAG_NONE);
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
    free(dir->mutex);
}

/* destructor */

static void
vfs_dir_finalize(GObject* obj)
{
    VFSDir* dir = VFS_DIR_REINTERPRET(obj);
    // LOG_INFO("vfs_dir_finalize  {}", dir->path);
    do
    {
    } while (g_source_remove_by_user_data(dir));

    if (dir->task)
    {
        dir->signal_task_load_dir.disconnect();
        // FIXME: should we generate a "file-list" signal to indicate the dir loading was cancelled?

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
        dir_map.erase(dir->path);

        /* There is no VFSDir instance */
        if (dir_map.size() == 0)
        {
            vfs_mime_type_remove_reload_cb(mime_cb);
            mime_cb = nullptr;

            if (change_notify_timeout)
            {
                g_source_remove(change_notify_timeout);
                change_notify_timeout = 0;
            }
        }

        free(dir->path);
        free(dir->disp_path);
        dir->path = dir->disp_path = nullptr;
    }
    // LOG_DEBUG("dir->thumbnail_loader: {:p}", dir->thumbnail_loader);
    if (dir->thumbnail_loader)
    {
        // LOG_DEBUG("FREE THUMBNAIL LOADER IN VFSDIR");
        vfs_thumbnail_loader_free(dir->thumbnail_loader);
        dir->thumbnail_loader = nullptr;
    }

    if (!dir->file_list.empty())
    {
        vfs_file_info_list_free(dir->file_list);
        dir->file_list.clear();
    }

    if (!dir->changed_files.empty())
    {
        vfs_file_info_list_free(dir->changed_files);
        dir->changed_files.clear();
    }

    if (!dir->created_files.empty())
    {
        dir->created_files.clear();
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

static VFSFileInfo*
vfs_dir_find_file(VFSDir* dir, std::string_view file_name, VFSFileInfo* file)
{
    for (VFSFileInfo* file2: dir->file_list)
    {
        if (file == file2)
            return file2;
        if (ztd::same(file2->name, file_name))
            return file2;
    }
    return nullptr;
}

/* signal handlers */
void
vfs_dir_emit_file_created(VFSDir* dir, std::string_view file_name, bool force)
{
    (void)force;
    // Ignore avoid_changes for creation of files
    // if ( !force && dir->avoid_changes )
    //    return;

    if (ztd::same(file_name, dir->path))
    { // Special Case: The directory itself was created?
        return;
    }

    dir->created_files.push_back(file_name.data());
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
vfs_dir_emit_file_deleted(VFSDir* dir, std::string_view file_name, VFSFileInfo* file)
{
    if (ztd::same(file_name, dir->path))
    {
        /* Special Case: The directory itself was deleted... */
        file = nullptr;
        /* clear the whole list */
        vfs_dir_lock(dir);
        vfs_file_info_list_free(dir->file_list);
        dir->file_list.clear();
        vfs_dir_unlock(dir);

        dir->run_event<EventType::FILE_DELETED>(file);

        return;
    }

    VFSFileInfo* file_found = vfs_dir_find_file(dir, file_name, file);
    if (file_found)
    {
        file_found = vfs_file_info_ref(file_found);
        if (!ztd::contains(dir->changed_files, file_found))
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
        {
            vfs_file_info_unref(file_found);
        }
    }
}

void
vfs_dir_emit_file_changed(VFSDir* dir, std::string_view file_name, VFSFileInfo* file, bool force)
{
    // LOG_INFO("vfs_dir_emit_file_changed dir={} file_name={} avoid={}", dir->path, file_name,
    // dir->avoid_changes ? "true" : "false");

    if (!force && dir->avoid_changes)
        return;

    if (ztd::same(file_name, dir->path))
    {
        // Special Case: The directory itself was changed
        dir->run_event<EventType::FILE_CHANGED>(nullptr);
        return;
    }

    vfs_dir_lock(dir);

    VFSFileInfo* file_found = vfs_dir_find_file(dir, file_name, file);
    if (file_found)
    {
        file_found = vfs_file_info_ref(file_found);

        if (!ztd::contains(dir->changed_files, file_found))
        {
            if (force)
            {
                dir->changed_files.push_back(file_found);
                if (change_notify_timeout == 0)
                {
                    change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                               100,
                                                               (GSourceFunc)notify_file_change,
                                                               nullptr,
                                                               nullptr);
                }
            }
            else if (update_file_info(dir, file_found)) // update file info the first time
            {
                dir->changed_files.push_back(file_found);
                if (change_notify_timeout == 0)
                {
                    change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                               500,
                                                               (GSourceFunc)notify_file_change,
                                                               nullptr,
                                                               nullptr);
                }
                dir->run_event<EventType::FILE_CHANGED>(file_found);
            }
        }
        else
        {
            vfs_file_info_unref(file_found);
        }
    }

    vfs_dir_unlock(dir);
}

void
vfs_dir_emit_thumbnail_loaded(VFSDir* dir, VFSFileInfo* file)
{
    vfs_dir_lock(dir);

    VFSFileInfo* file_found = vfs_dir_find_file(dir, file->name.c_str(), file);
    if (file_found)
    {
        assert(file == file_found);
        file = vfs_file_info_ref(file_found);
    }
    else
    {
        file = nullptr;
    }

    vfs_dir_unlock(dir);

    if (file)
    {
        dir->run_event<EventType::FILE_THUMBNAIL_LOADED>(file);
        vfs_file_info_unref(file);
    }
}

/* methods */

static VFSDir*
vfs_dir_new(std::string_view path)
{
    VFSDir* dir = VFS_DIR(g_object_new(VFS_TYPE_DIR, nullptr));
    dir->path = ztd::strdup(path.data());

    dir->avoid_changes = vfs_volume_dir_avoid_changes(path.data());
    // LOG_INFO("vfs_dir_new {}  avoid_changes={}", dir->path, dir->avoid_changes ? "true" :
    // "false");
    return dir;
}

void
on_list_task_finished(VFSDir* dir, bool is_cancelled)
{
    g_object_unref(dir->task);
    dir->task = nullptr;
    dir->run_event<EventType::FILE_LISTED>(is_cancelled);
    dir->file_listed = true;
    dir->load_complete = true;
}

static const std::string
gethidden(std::string_view path)
{
    std::string hidden;

    // Read .hidden into string
    const std::string hidden_path = Glib::build_filename(path.data(), ".hidden");

    // test access first because open() on missing file may cause
    // long delay on nfs
    if (!have_rw_access(hidden_path))
        return hidden;

    std::string line;
    std::ifstream file(hidden_path);
    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            hidden.append(line + '\n');
        }
    }
    file.close();

    return hidden;
}

static bool
ishidden(std::string_view hidden, std::string_view file_name)
{
    if (ztd::contains(hidden, fmt::format("{}\n", file_name)))
        return true;
    return false;
}

bool
vfs_dir_add_hidden(std::string_view path, std::string_view file_name)
{
    const std::string file_path = Glib::build_filename(path.data(), ".hidden");
    const std::string data = fmt::format("{}\n", file_name);

    const bool result = write_file(file_path, data);
    if (!result)
        return false;
    return true;
}

static void
vfs_dir_load(VFSDir* dir)
{
    if (!dir->path)
        return;

    dir->disp_path = ztd::strdup(Glib::filename_display_name(dir->path));
    dir->task = vfs_async_task_new((VFSAsyncFunc)vfs_dir_load_thread, dir);

    dir->signal_task_load_dir =
        dir->task->add_event<EventType::TASK_FINISH>(on_list_task_finished, dir);

    vfs_async_task_execute(dir->task);
}

static void*
vfs_dir_load_thread(VFSAsyncTask* task, VFSDir* dir)
{
    (void)task;

    dir->file_listed = false;
    dir->load_complete = false;
    dir->xhidden_count = 0; // MOD
    if (!dir->path)
        return nullptr;

    /* Install file alteration monitor */
    dir->monitor = vfs_file_monitor_add(dir->path, vfs_dir_monitor_callback, dir);

    // MOD  dir contains .hidden file?
    const std::string hidden = gethidden(dir->path);

    for (const auto& file: std::filesystem::directory_iterator(dir->path))
    {
        if (vfs_async_task_is_cancelled(dir->task))
            break;

        const std::string file_name = std::filesystem::path(file).filename();
        const std::string full_path = Glib::build_filename(dir->path, file_name);

        // MOD ignore if in .hidden
        if (ishidden(hidden, file_name))
        {
            dir->xhidden_count++;
            continue;
        }

        VFSFileInfo* fi = vfs_file_info_new();
        if (vfs_file_info_get(fi, full_path))
        {
            vfs_dir_lock(dir);

            /* Special processing for desktop directory */
            vfs_file_info_load_special_info(fi, full_path.c_str());

            dir->file_list.push_back(fi);

            vfs_dir_unlock(dir);
        }
        else
        {
            vfs_file_info_unref(fi);
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
    const std::string file_name = file->name;
    if (ztd::same(file->name, file->disp_name))
        file->disp_name.clear();
    file->name.clear();

    const std::string full_path = Glib::build_filename(dir->path, file_name);

    if (vfs_file_info_get(file, full_path))
    {
        ret = true;
        vfs_file_info_load_special_info(file, full_path.c_str());
    }
    else /* The file does not exist */
    {
        if (ztd::contains(dir->file_list, file))
        {
            ztd::remove(dir->file_list, file);
            if (file)
            {
                dir->run_event<EventType::FILE_DELETED>(file);
                vfs_file_info_unref(file);
            }
        }
        ret = false;
    }

    return ret;
}

static void
update_changed_files(std::string_view key, VFSDir* dir)
{
    (void)key;

    if (dir->changed_files.empty())
        return;

    vfs_dir_lock(dir);
    for (VFSFileInfo* file: dir->changed_files)
    {
        if (update_file_info(dir, file))
        {
            dir->run_event<EventType::FILE_CHANGED>(file);
            vfs_file_info_unref(file);
        }
        // else was deleted, signaled, and unrefed in update_file_info
    }
    dir->changed_files.clear();
    vfs_dir_unlock(dir);
}

static void
update_created_files(std::string_view key, VFSDir* dir)
{
    (void)key;

    if (dir->created_files.empty())
        return;

    vfs_dir_lock(dir);
    for (std::string_view created_file: dir->created_files)
    {
        VFSFileInfo* file;
        VFSFileInfo* file_found = vfs_dir_find_file(dir, created_file, nullptr);
        if (!file_found)
        {
            // file is not in dir file_list
            const std::string full_path = Glib::build_filename(dir->path, created_file.data());
            file = vfs_file_info_new();
            if (vfs_file_info_get(file, full_path))
            {
                // add new file to dir file_list
                vfs_file_info_load_special_info(file, full_path.c_str());
                dir->file_list.push_back(vfs_file_info_ref(file));

                dir->run_event<EventType::FILE_CREATED>(file);
            }
            // else file does not exist in filesystem
            vfs_file_info_unref(file);
        }
        else
        {
            // file already exists in dir file_list
            file = vfs_file_info_ref(file_found);
            if (update_file_info(dir, file))
            {
                dir->run_event<EventType::FILE_CHANGED>(file);
                vfs_file_info_unref(file);
            }
            // else was deleted, signaled, and unrefed in update_file_info
        }
    }
    dir->created_files.clear();
    vfs_dir_unlock(dir);
}

static bool
notify_file_change(void* user_data)
{
    (void)user_data;

    for (const auto& dir: dir_map)
    {
        update_changed_files(dir.first, dir.second);
        update_created_files(dir.first, dir.second);
    }
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

    for (const auto& dir: dir_map)
    {
        update_changed_files(dir.first, dir.second);
        update_created_files(dir.first, dir.second);
    }
}

/* Callback function which will be called when monitored events happen */
static void
vfs_dir_monitor_callback(VFSFileMonitor* monitor, VFSFileMonitorEvent event, const char* file_name,
                         void* user_data)
{
    (void)monitor;
    VFSDir* dir = VFS_DIR(user_data);

    switch (event)
    {
        case VFSFileMonitorEvent::VFS_FILE_MONITOR_CREATE:
            vfs_dir_emit_file_created(dir, file_name, false);
            break;
        case VFSFileMonitorEvent::VFS_FILE_MONITOR_DELETE:
            vfs_dir_emit_file_deleted(dir, file_name, nullptr);
            break;
        case VFSFileMonitorEvent::VFS_FILE_MONITOR_CHANGE:
            vfs_dir_emit_file_changed(dir, file_name, nullptr, false);
            break;
        default:
            LOG_WARN("Error: unrecognized file monitor signal!");
    }
}

VFSDir*
vfs_dir_get_by_path_soft(std::string_view path)
{
    VFSDir* dir = nullptr;

    try
    {
        dir = dir_map.at(path.data());
    }
    catch (std::out_of_range)
    {
        dir = nullptr;
    }

    if (dir)
        g_object_ref(dir);
    return dir;
}

VFSDir*
vfs_dir_get_by_path(std::string_view path)
{
    VFSDir* dir = nullptr;

    if (!dir_map.empty())
    {
        try
        {
            dir = dir_map.at(path.data());
        }
        catch (std::out_of_range)
        {
            dir = nullptr;
        }
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
        dir_map.insert({dir->path, dir});
    }
    return dir;
}

static void
reload_mime_type(std::string_view key, VFSDir* dir)
{
    (void)key;

    if (!dir || dir->file_list.empty())
        return;

    vfs_dir_lock(dir);

    for (VFSFileInfo* file: dir->file_list)
    {
        const std::string full_path = Glib::build_filename(dir->path, vfs_file_info_get_name(file));
        vfs_file_info_reload_mime_type(file, full_path.c_str());
        // LOG_DEBUG("reload {}", full_path);
    }

    for (VFSFileInfo* file: dir->file_list)
    {
        dir->run_event<EventType::FILE_CHANGED>(file);
    }

    vfs_dir_unlock(dir);
}

static void
on_mime_type_reload(void* user_data)
{
    (void)user_data;
    // LOG_DEBUG("reload mime-type");
    for (const auto& dir: dir_map)
    {
        reload_mime_type(dir.first, dir.second);
    }
}

void
vfs_dir_foreach(VFSDirForeachFunc func, bool user_data)
{
    // LOG_DEBUG("reload mime-type");
    for (const auto& dir: dir_map)
    {
        func(dir.second, user_data);
    }
}

void
vfs_dir_unload_thumbnails(VFSDir* dir, bool is_big)
{
    vfs_dir_lock(dir);
    for (VFSFileInfo* file: dir->file_list)
    {
        if (is_big)
        {
            if (file->big_thumbnail)
            {
                g_object_unref(file->big_thumbnail);
                file->big_thumbnail = nullptr;
            }
        }
        else
        {
            if (file->small_thumbnail)
            {
                g_object_unref(file->small_thumbnail);
                file->small_thumbnail = nullptr;
            }
        }

        /* This is a desktop entry file, so the icon needs reload
             FIXME: This is not a good way to do things, but there is no better way now.  */
        if (file->flags & VFSFileInfoFlag::VFS_FILE_INFO_DESKTOP_ENTRY)
        {
            const std::string file_path = Glib::build_filename(dir->path, file->name);
            vfs_file_info_load_special_info(file, file_path.c_str());
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
mime_change()
{
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

    const std::string path = Glib::build_filename(vfs_user_data_dir(), "mime/packages");
    if (!std::filesystem::is_directory(path))
        return;

    mime_dir = vfs_dir_get_by_path(path);
    if (!mime_dir)
        return;

    // LOG_INFO("MIME-UPDATE watch started");
    mime_dir->add_event<EventType::FILE_LISTED>(mime_change);
    mime_dir->add_event<EventType::FILE_CHANGED>(mime_change);
    mime_dir->add_event<EventType::FILE_DELETED>(mime_change);
    mime_dir->add_event<EventType::FILE_CHANGED>(mime_change);
}
