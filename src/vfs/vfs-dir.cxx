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

#include <format>

#include <filesystem>

#include <vector>
#include <map>

#include <iostream>
#include <fstream>

#include <algorithm>
#include <ranges>

#include <mutex>

#include <optional>

#include <cassert>

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

#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-dir.hxx"

#define VFS_DIR_REINTERPRET(obj) (reinterpret_cast<VFSDir*>(obj))

#define VFS_TYPE_DIR (vfs_dir_get_type())

struct VFSDirClass
{
    GObjectClass parent;

    /* Default signal handlers */
    void (*file_created)(vfs::dir dir, vfs::file_info file);
    void (*file_deleted)(vfs::dir dir, vfs::file_info file);
    void (*file_changed)(vfs::dir dir, vfs::file_info file);
    void (*thumbnail_loaded)(vfs::dir dir, vfs::file_info file);
    void (*file_listed)(vfs::dir dir);
    void (*load_complete)(vfs::dir dir);
    // void (*need_reload)(vfs::dir dir);
    // void (*update_mime)(vfs::dir dir);
};

static void vfs_dir_class_init(VFSDirClass* klass);
static void vfs_dir_finalize(GObject* obj);
static void vfs_dir_set_property(GObject* obj, u32 prop_id, const GValue* value, GParamSpec* pspec);
static void vfs_dir_get_property(GObject* obj, u32 prop_id, GValue* value, GParamSpec* pspec);

static const std::optional<std::vector<std::filesystem::path>>
gethidden(const std::filesystem::path& path);

static void vfs_dir_load(vfs::dir dir);
static void* vfs_dir_load_thread(vfs::async_task task, vfs::dir dir);

static void vfs_dir_monitor_callback(const vfs::file_monitor& monitor,
                                     vfs::file_monitor_event event,
                                     const std::filesystem::path& file_name, void* user_data);

static bool notify_file_change(void* user_data);
static bool update_file_info(vfs::dir dir, vfs::file_info file);

static void on_list_task_finished(vfs::dir dir, bool is_cancelled);

static GObjectClass* parent_class = nullptr;

static std::map<const char*, vfs::dir> dir_map;
// static std::map<std::string, vfs::dir> dir_map; // breaks multiple tabs with save VFSDir, reason unknown
// static std::map<std::filesystem::path, vfs::dir> dir_map; // breaks multiple tabs with save VFSDir, reason unknown

static u32 change_notify_timeout = 0;

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
            nullptr,
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

/* destructor */
static void
vfs_dir_finalize(GObject* obj)
{
    vfs::dir dir = VFS_DIR_REINTERPRET(obj);
    // ztd::logger::info("vfs_dir_finalize  {}", dir->path);
    do
    {
    } while (g_source_remove_by_user_data(dir));

    if (dir->task)
    {
        dir->signal_task_load_dir.disconnect();
        // FIXME: should we generate a "file-list" signal to indicate the dir loading was cancelled?

        // ztd::logger::info("vfs_dir_finalize -> vfs_async_task_cancel");
        dir->task->cancel();
        g_object_unref(dir->task);
        dir->task = nullptr;
    }
    if (dir->monitor)
    {
        vfs_file_monitor_remove(dir->monitor, vfs_dir_monitor_callback, dir);
    }
    if (!dir->path.empty())
    {
        dir_map.erase(dir->path.c_str());

        /* There is no VFSDir instance */
        if (dir_map.size() == 0)
        {
            if (change_notify_timeout)
            {
                g_source_remove(change_notify_timeout);
                change_notify_timeout = 0;
            }
        }
    }
    // ztd::logger::debug("dir->thumbnail_loader: {:p}", dir->thumbnail_loader);
    if (dir->thumbnail_loader)
    {
        // ztd::logger::debug("FREE THUMBNAIL LOADER IN VFSDIR");
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

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
vfs_dir_get_property(GObject* obj, u32 prop_id, GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static void
vfs_dir_set_property(GObject* obj, u32 prop_id, const GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static vfs::file_info
vfs_dir_find_file(vfs::dir dir, const std::filesystem::path& file_name, vfs::file_info file)
{
    for (vfs::file_info file2 : dir->file_list)
    {
        if (file == file2)
        {
            return file2;
        }
        if (ztd::same(file2->name, file_name.string()))
        {
            return file2;
        }
    }
    return nullptr;
}

/* signal handlers */
void
vfs_dir_emit_file_created(vfs::dir dir, const std::filesystem::path& file_name, bool force)
{
    (void)force;
    // Ignore avoid_changes for creation of files
    // if ( !force && dir->avoid_changes )
    //    return;

    if (std::filesystem::equivalent(file_name, dir->path))
    { // Special Case: The directory itself was created?
        return;
    }

    dir->created_files.emplace_back(file_name);
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
vfs_dir_emit_file_deleted(vfs::dir dir, const std::filesystem::path& file_name, vfs::file_info file)
{
    std::lock_guard<std::mutex> lock(dir->mutex);

    if (std::filesystem::equivalent(file_name, dir->path))
    {
        /* Special Case: The directory itself was deleted... */
        file = nullptr;

        /* clear the whole list */
        vfs_file_info_list_free(dir->file_list);
        dir->file_list.clear();

        dir->run_event<spacefm::signal::file_deleted>(file);

        return;
    }

    vfs::file_info file_found = vfs_dir_find_file(dir, file_name, file);
    if (file_found)
    {
        file_found = vfs_file_info_ref(file_found);
        if (!ztd::contains(dir->changed_files, file_found))
        {
            dir->changed_files.emplace_back(file_found);
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
vfs_dir_emit_file_changed(vfs::dir dir, const std::filesystem::path& file_name, vfs::file_info file,
                          bool force)
{
    std::lock_guard<std::mutex> lock(dir->mutex);

    // ztd::logger::info("vfs_dir_emit_file_changed dir={} file_name={} avoid={}", dir->path,
    // file_name, dir->avoid_changes ? "true" : "false");

    if (!force && dir->avoid_changes)
    {
        return;
    }

    if (std::filesystem::equivalent(file_name, dir->path))
    {
        // Special Case: The directory itself was changed
        dir->run_event<spacefm::signal::file_changed>(nullptr);
        return;
    }

    vfs::file_info file_found = vfs_dir_find_file(dir, file_name, file);
    if (file_found)
    {
        file_found = vfs_file_info_ref(file_found);

        if (!ztd::contains(dir->changed_files, file_found))
        {
            if (force)
            {
                dir->changed_files.emplace_back(file_found);
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
                dir->changed_files.emplace_back(file_found);
                if (change_notify_timeout == 0)
                {
                    change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                               500,
                                                               (GSourceFunc)notify_file_change,
                                                               nullptr,
                                                               nullptr);
                }
                dir->run_event<spacefm::signal::file_changed>(file_found);
            }
        }
        else
        {
            vfs_file_info_unref(file_found);
        }
    }
}

void
vfs_dir_emit_thumbnail_loaded(vfs::dir dir, vfs::file_info file)
{
    std::lock_guard<std::mutex> lock(dir->mutex);

    vfs::file_info file_found = vfs_dir_find_file(dir, file->name, file);
    if (file_found)
    {
        assert(file == file_found);
        file = vfs_file_info_ref(file_found);
    }
    else
    {
        file = nullptr;
    }

    if (file)
    {
        dir->run_event<spacefm::signal::file_thumbnail_loaded>(file);
        vfs_file_info_unref(file);
    }
}

/* methods */

/* constructor is private */
static vfs::dir
vfs_dir_new(const std::string_view path)
{
    vfs::dir dir = VFS_DIR(g_object_new(VFS_TYPE_DIR, nullptr));
    assert(dir != nullptr);

    dir->path = path.data();
    dir->avoid_changes = vfs_volume_dir_avoid_changes(path);

    // ztd::logger::info("vfs_dir_new {}  avoid_changes={}", dir->path, dir->avoid_changes);

    return dir;
}

void
on_list_task_finished(vfs::dir dir, bool is_cancelled)
{
    g_object_unref(dir->task);
    dir->task = nullptr;
    dir->run_event<spacefm::signal::file_listed>(is_cancelled);
    dir->file_listed = true;
    dir->load_complete = true;
}

static const std::optional<std::vector<std::filesystem::path>>
gethidden(const std::filesystem::path& path)
{
    std::vector<std::filesystem::path> hidden;

    // Read .hidden into string
    const auto hidden_path = path / ".hidden";

    if (std::filesystem::is_regular_file(hidden_path))
    {
        return std::nullopt;
    }

    // test access first because open() on missing file may cause
    // long delay on nfs
    if (!have_rw_access(hidden_path))
    {
        return std::nullopt;
    }

    std::string line;
    std::ifstream file(hidden_path);
    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            const auto hidden_file = std::filesystem::path(ztd::strip(line));
            if (hidden_file.is_absolute())
            {
                ztd::logger::warn("Absolute path ignored in {}", hidden_path.string());
                continue;
            }

            hidden.push_back(hidden_file);
        }
    }
    file.close();

    return hidden;
}

bool
vfs_dir_add_hidden(const std::filesystem::path& path, const std::filesystem::path& file_name)
{
    const auto file_path = path / ".hidden";
    const std::string data = std::format("{}\n", file_name.string());

    const bool result = write_file(file_path, data);
    if (!result)
    {
        return false;
    }
    return true;
}

static void
vfs_dir_load(vfs::dir dir)
{
    if (dir->path.empty())
    {
        return;
    }
    // ztd::logger::info("dir->path={}", dir->path);

    dir->task = vfs_async_task_new((VFSAsyncFunc)vfs_dir_load_thread, dir);

    dir->signal_task_load_dir =
        dir->task->add_event<spacefm::signal::task_finish>(on_list_task_finished, dir);

    dir->task->run_thread();
}

static void*
vfs_dir_load_thread(vfs::async_task task, vfs::dir dir)
{
    (void)task;

    std::lock_guard<std::mutex> lock(dir->mutex);

    dir->file_listed = false;
    dir->load_complete = false;
    dir->xhidden_count = 0;
    if (dir->path.empty())
    {
        return nullptr;
    }

    /* Install file alteration monitor */
    dir->monitor = vfs_file_monitor_add(dir->path, vfs_dir_monitor_callback, dir);

    // MOD  dir contains .hidden file?
    const auto hidden_files = gethidden(dir->path);

    for (const auto& dfile : std::filesystem::directory_iterator(dir->path))
    {
        if (dir->task->is_cancelled())
        {
            break;
        }

        const auto file_name = dfile.path().filename();
        const auto full_path = std::filesystem::path() / dir->path / file_name;

        // MOD ignore if in .hidden
        if (hidden_files)
        {
            bool hide_file = false;
            for (const auto& hidden_file : hidden_files.value())
            {
                // if (ztd::same(hidden_file.string(), file_name.string()))
                std::error_code ec;
                const bool equivalent = std::filesystem::equivalent(hidden_file, file_name, ec);
                if (!ec && equivalent)
                {
                    hide_file = true;
                    dir->xhidden_count++;
                    break;
                }
            }
            if (hide_file)
            {
                continue;
            }
        }

        vfs::file_info file = vfs_file_info_new(full_path);

        /* Special processing for desktop directory */
        file->load_special_info(full_path);

        dir->file_list.emplace_back(file);
    }

    return nullptr;
}

bool
vfs_dir_is_file_listed(vfs::dir dir)
{
    return dir->file_listed;
}

static bool
update_file_info(vfs::dir dir, vfs::file_info file)
{
    bool ret = false;

    const auto full_path = std::filesystem::path() / dir->path / file->name;

    const bool is_file_valid = file->update(full_path);
    if (is_file_valid)
    {
        ret = true;
        file->load_special_info(full_path);
    }
    else /* The file does not exist */
    {
        if (ztd::contains(dir->file_list, file))
        {
            ztd::remove(dir->file_list, file);
            if (file)
            {
                dir->run_event<spacefm::signal::file_deleted>(file);
                vfs_file_info_unref(file);
            }
        }
        ret = false;
    }

    return ret;
}

static void
update_changed_files(const std::filesystem::path& key, vfs::dir dir)
{
    (void)key;

    std::lock_guard<std::mutex> lock(dir->mutex);

    if (dir->changed_files.empty())
    {
        return;
    }

    for (vfs::file_info file : dir->changed_files)
    {
        if (update_file_info(dir, file))
        {
            dir->run_event<spacefm::signal::file_changed>(file);
            vfs_file_info_unref(file);
        }
        // else was deleted, signaled, and unrefed in update_file_info
    }
    dir->changed_files.clear();
}

static void
update_created_files(const std::filesystem::path& key, vfs::dir dir)
{
    (void)key;

    std::lock_guard<std::mutex> lock(dir->mutex);

    if (dir->created_files.empty())
    {
        return;
    }

    for (const std::string_view created_file : dir->created_files)
    {
        vfs::file_info file_found = vfs_dir_find_file(dir, created_file, nullptr);
        if (!file_found)
        {
            // file is not in dir file_list
            const auto full_path = std::filesystem::path() / dir->path / created_file;
            if (std::filesystem::exists(full_path))
            {
                vfs::file_info file = vfs_file_info_new(full_path);
                // add new file to dir file_list
                file->load_special_info(full_path);
                dir->file_list.emplace_back(vfs_file_info_ref(file));

                dir->run_event<spacefm::signal::file_created>(file);

                vfs_file_info_unref(file);
            }
            // else file does not exist in filesystem
        }
        else
        {
            // file already exists in dir file_list
            vfs::file_info file = vfs_file_info_ref(file_found);
            if (update_file_info(dir, file))
            {
                dir->run_event<spacefm::signal::file_changed>(file);
                vfs_file_info_unref(file);
            }
            // else was deleted, signaled, and unrefed in update_file_info
        }
    }
    dir->created_files.clear();
}

static bool
notify_file_change(void* user_data)
{
    (void)user_data;

    for (const auto& [path, dir] : dir_map)
    {
        update_changed_files(path, dir);
        update_created_files(path, dir);
    }
    /* remove the timeout */
    change_notify_timeout = 0;
    return false;
}

void
vfs_dir_flush_notify_cache()
{
    if (change_notify_timeout)
    {
        g_source_remove(change_notify_timeout);
    }
    change_notify_timeout = 0;

    for (const auto& [path, dir] : dir_map)
    {
        update_changed_files(path, dir);
        update_created_files(path, dir);
    }
}

/* Callback function which will be called when monitored events happen */
static void
vfs_dir_monitor_callback(const vfs::file_monitor& monitor, vfs::file_monitor_event event,
                         const std::filesystem::path& file_name, void* user_data)
{
    (void)monitor;
    vfs::dir dir = VFS_DIR(user_data);

    switch (event)
    {
        case vfs::file_monitor_event::created:
            vfs_dir_emit_file_created(dir, file_name, false);
            break;
        case vfs::file_monitor_event::deleted:
            vfs_dir_emit_file_deleted(dir, file_name, nullptr);
            break;
        case vfs::file_monitor_event::changed:
            vfs_dir_emit_file_changed(dir, file_name, nullptr, false);
            break;
    }
}

vfs::dir
vfs_dir_get_by_path_soft(const std::filesystem::path& path)
{
    vfs::dir dir = nullptr;
    if (dir_map.contains(path.c_str()))
    {
        dir = dir_map.at(path.c_str());
        assert(dir != nullptr);
        g_object_ref(dir);
    }
    return dir;
}

vfs::dir
vfs_dir_get_by_path(const std::filesystem::path& path)
{
    if (dir_map.contains(path.c_str()))
    {
        vfs::dir dir = dir_map.at(path.c_str());
        assert(dir != nullptr);
        g_object_ref(dir);
        return dir;
    }

    vfs::dir dir = vfs_dir_new(path.c_str());
    assert(dir != nullptr);
    vfs_dir_load(dir); /* asynchronous operation */
    dir_map.insert({dir->path.c_str(), dir});

    return dir;
}

static void
reload_mime_type(const std::filesystem::path& key, vfs::dir dir)
{
    (void)key;

    std::lock_guard<std::mutex> lock(dir->mutex);

    if (!dir || dir->file_list.empty())
    {
        return;
    }

    for (vfs::file_info file : dir->file_list)
    {
        const auto full_path = std::filesystem::path() / dir->path / file->get_name();
        file->reload_mime_type(full_path);
        // ztd::logger::debug("reload {}", full_path);
    }

    const auto action = [dir](vfs::file_info file)
    { dir->run_event<spacefm::signal::file_changed>(file); };
    std::ranges::for_each(dir->file_list, action);
}

void
vfs_dir_mime_type_reload()
{
    // ztd::logger::debug("reload mime-type");
    const auto action = [](const auto& dir) { reload_mime_type(dir.first, dir.second); };
    std::ranges::for_each(dir_map, action);
}

void
vfs_dir_foreach(VFSDirForeachFunc func, bool user_data)
{
    // ztd::logger::debug("reload mime-type");
    const auto action = [func, user_data](const auto& dir) { func(dir.second, user_data); };
    std::ranges::for_each(dir_map, action);
}

void
vfs_dir_unload_thumbnails(vfs::dir dir, bool is_big)
{
    std::lock_guard<std::mutex> lock(dir->mutex);

    for (vfs::file_info file : dir->file_list)
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
        if (file->flags & vfs::file_info_flags::desktop_entry)
        {
            const auto file_path = std::filesystem::path() / dir->path / file->name;
            file->load_special_info(file_path);
        }
    }

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
}

// sfm added mime change timer
static u32 mime_change_timer = 0;
static vfs::dir mime_dir = nullptr;

static bool
on_mime_change_timer(void* user_data)
{
    (void)user_data;

    // ztd::logger::info("MIME-UPDATE on_timer");
    const auto data_dir = vfs::user_dirs->data_dir();
    const std::string command1 = std::format("update-mime-database {}/mime", data_dir.string());
    ztd::logger::info("COMMAND={}", command1);
    Glib::spawn_command_line_async(command1);

    const std::string command2 =
        std::format("update-desktop-database {}/applications", data_dir.string());
    ztd::logger::info("COMMAND={}", command2);
    Glib::spawn_command_line_async(command2);

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
        // ztd::logger::info("MIME-UPDATE already set");
        return;
    }
    if (mime_dir)
    {
        // update mime database in 2 seconds
        mime_change_timer = g_timeout_add_seconds(2, (GSourceFunc)on_mime_change_timer, nullptr);
        // ztd::logger::info("MIME-UPDATE timer started");
    }
}

void
vfs_dir_monitor_mime()
{
    // start watching for changes
    if (mime_dir)
    {
        return;
    }

    const auto path = vfs::user_dirs->data_dir() / "mime" / "packages";
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    mime_dir = vfs_dir_get_by_path(path);
    if (!mime_dir)
    {
        return;
    }

    // ztd::logger::info("MIME-UPDATE watch started");
    mime_dir->add_event<spacefm::signal::file_listed>(mime_change);
    mime_dir->add_event<spacefm::signal::file_changed>(mime_change);
    mime_dir->add_event<spacefm::signal::file_deleted>(mime_change);
    mime_dir->add_event<spacefm::signal::file_changed>(mime_change);
}
