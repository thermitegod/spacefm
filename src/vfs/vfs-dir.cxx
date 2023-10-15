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

#include <algorithm>

#include <mutex>

#include <optional>

#include <fstream>

#include <cassert>

#include <glibmm.h>

#include <malloc.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "write.hxx"
#include "utils.hxx"

#include "vfs/vfs-volume.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "vfs/vfs-async-thread.hxx"

#include "vfs/vfs-dir.hxx"

#define VFS_DIR_REINTERPRET(obj) (reinterpret_cast<VFSDir*>(obj))

#define VFS_TYPE_DIR (vfs_dir_get_type())

struct VFSDirClass
{
    GObjectClass parent;

    /* Default signal handlers */
    void (*file_created)(vfs::dir dir, const vfs::file_info& file);
    void (*file_deleted)(vfs::dir dir, const vfs::file_info& file);
    void (*file_changed)(vfs::dir dir, const vfs::file_info& file);
    void (*thumbnail_loaded)(vfs::dir dir, const vfs::file_info& file);
    void (*file_listed)(vfs::dir dir);
    void (*load_complete)(vfs::dir dir);
    // void (*need_reload)(vfs::dir dir);
    // void (*update_mime)(vfs::dir dir);
};

static void vfs_dir_class_init(VFSDirClass* klass);
static void vfs_dir_finalize(GObject* obj);
static void vfs_dir_set_property(GObject* obj, u32 prop_id, const GValue* value, GParamSpec* pspec);
static void vfs_dir_get_property(GObject* obj, u32 prop_id, GValue* value, GParamSpec* pspec);

static void vfs_dir_monitor_callback(const vfs::file_monitor& monitor,
                                     vfs::file_monitor_event event,
                                     const std::filesystem::path& file_name, void* user_data);

static GObjectClass* parent_class = nullptr;

static std::map<const char*, vfs::dir> dir_map;
// static std::map<std::string, vfs::dir> dir_map; // breaks multiple tabs with save VFSDir, reason unknown
// static std::map<std::filesystem::path, vfs::dir> dir_map; // breaks multiple tabs with save VFSDir, reason unknown

static u32 change_notify_timeout = 0;

/*
 * gobject
 */

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
        dir->task = nullptr;
    }
    if (dir->monitor)
    {
        vfs_file_monitor_remove(dir->monitor, vfs_dir_monitor_callback, dir);
    }

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

    // ztd::logger::trace("dir->thumbnail_loader: {}", fmt::ptr(dir->thumbnail_loader));
    dir->thumbnail_loader = nullptr;

    dir->file_list.clear();
    dir->changed_files.clear();

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

/* methods */

/* constructor is private */
static vfs::dir
vfs_dir_new(const std::string_view path)
{
    vfs::dir dir = VFS_DIR(g_object_new(VFS_TYPE_DIR, nullptr));
    assert(dir != nullptr);

    assert(path.empty() != true);
    dir->path = path.data();
    dir->avoid_changes = vfs_volume_dir_avoid_changes(path);

    // ztd::logger::info("vfs_dir_new {}  avoid_changes={}", dir->path, dir->avoid_changes);

    return dir;
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
    vfs::dir dir = vfs_dir_get_by_path_soft(path);
    if (dir == nullptr)
    {
        dir = vfs_dir_new(path.c_str());
        assert(dir != nullptr);
        dir->load(); /* asynchronous operation */
        dir_map.insert({dir->path.c_str(), dir});
    }
    return dir;
}

void
on_list_task_finished(vfs::dir dir, bool is_cancelled)
{
    dir->task = nullptr;
    dir->run_event<spacefm::signal::file_listed>(is_cancelled);
    dir->file_listed = true;
    dir->load_complete = true;
}

static const std::optional<std::vector<std::filesystem::path>>
get_hidden_files(const std::filesystem::path& path) noexcept
{
    std::vector<std::filesystem::path> hidden;

    // Read .hidden into string
    const auto hidden_path = path / ".hidden";

    if (!std::filesystem::is_regular_file(hidden_path))
    {
        return std::nullopt;
    }

    // test access first because open() on missing file may cause
    // long delay on nfs
    if (!have_rw_access(hidden_path))
    {
        return std::nullopt;
    }

    std::ifstream file(hidden_path);
    if (!file)
    {
        ztd::logger::error("Failed to open the file: {}", hidden_path.string());
        return std::nullopt;
    }

    std::string line;
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
    file.close();

    return hidden;
}

static void*
vfs_dir_load_thread(const vfs::async_thread_t& task, vfs::dir dir)
{
    std::lock_guard<std::mutex> lock(dir->mutex);

    dir->file_listed = false;
    dir->load_complete = false;
    dir->xhidden_count = 0;

    /* Install file alteration monitor */
    dir->monitor = vfs_file_monitor_add(dir->path, vfs_dir_monitor_callback, dir);

    // MOD  dir contains .hidden file?
    const auto hidden_files = get_hidden_files(dir->path);

    for (const auto& dfile : std::filesystem::directory_iterator(dir->path))
    {
        if (task->is_canceled())
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

        dir->file_list.emplace_back(file);
    }

    return nullptr;
}

static bool
notify_file_change(void* user_data)
{
    (void)user_data;

    for (const auto& [path, dir] : dir_map)
    {
        dir->update_changed_files(path);
        dir->update_created_files(path);
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
        dir->update_changed_files(path);
        dir->update_created_files(path);
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
            dir->emit_file_created(file_name, false);
            break;
        case vfs::file_monitor_event::deleted:
            dir->emit_file_deleted(file_name, nullptr);
            break;
        case vfs::file_monitor_event::changed:
            dir->emit_file_changed(file_name, nullptr, false);
            break;
        case vfs::file_monitor_event::other:
            break;
    }
}

static void
reload_mime_type(const std::filesystem::path& key, vfs::dir dir)
{
    (void)key;

    std::lock_guard<std::mutex> lock(dir->mutex);

    if (!dir || dir->is_directory_empty())
    {
        return;
    }

    for (const vfs::file_info& file : dir->file_list)
    {
        // ztd::logger::debug("reload {}", file->path());
        file->reload_mime_type();
    }

    const auto action = [dir](const vfs::file_info& file)
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

/**
* VFSDir class
*/

vfs::file_info
VFSDir::find_file(const std::filesystem::path& file_name, const vfs::file_info& file) const noexcept
{
    for (const vfs::file_info& file2 : this->file_list)
    {
        if (file == file2)
        {
            return file2;
        }
        if (ztd::same(file2->name(), file_name.string()))
        {
            return file2;
        }
    }
    return nullptr;
}

bool
VFSDir::add_hidden(const vfs::file_info& file) const noexcept
{
    const auto file_path = std::filesystem::path() / this->path / ".hidden";
    const std::string data = std::format("{}\n", file->name());

    return write_file(file_path, data);
}

void
VFSDir::cancel_all_thumbnail_requests() noexcept
{
    this->thumbnail_loader = nullptr;
}

void
VFSDir::load_thumbnail(const vfs::file_info& file, const bool is_big) noexcept
{
    vfs_thumbnail_loader_request(this, file, is_big);
}

void
VFSDir::load() noexcept
{
    if (this->path.empty())
    {
        return;
    }
    // ztd::logger::info("dir->path={}", dir->path);

    this->task = vfs_async_thread_new((vfs::async_thread_function_t)vfs_dir_load_thread, this);

    this->signal_task_load_dir =
        this->task->add_event<spacefm::signal::task_finish>(on_list_task_finished, this);

    this->task->run();
}

bool
VFSDir::is_file_listed() const noexcept
{
    return this->file_listed;
}

bool
VFSDir::is_directory_empty() const noexcept
{
    return this->file_list.empty();
}

bool
VFSDir::update_file_info(const vfs::file_info& file) noexcept
{
    bool ret = false;

    const bool is_file_valid = file->update();
    if (is_file_valid)
    {
        ret = true;
    }
    else /* The file does not exist */
    {
        if (ztd::contains(this->file_list, file))
        {
            ztd::remove(this->file_list, file);
            if (file)
            {
                this->run_event<spacefm::signal::file_deleted>(file);
            }
        }
        ret = false;
    }

    return ret;
}

void
VFSDir::update_changed_files(const std::filesystem::path& key) noexcept
{
    (void)key;

    std::lock_guard<std::mutex> lock(this->mutex);

    if (this->changed_files.empty())
    {
        return;
    }

    for (const vfs::file_info& file : this->changed_files)
    {
        if (this->update_file_info(file))
        {
            this->run_event<spacefm::signal::file_changed>(file);
        }
        // else was deleted, signaled, and unrefed in update_file_info
    }
    this->changed_files.clear();
}

void
VFSDir::update_created_files(const std::filesystem::path& key) noexcept
{
    (void)key;

    std::lock_guard<std::mutex> lock(this->mutex);

    if (this->created_files.empty())
    {
        return;
    }

    for (const auto& created_file : this->created_files)
    {
        vfs::file_info file_found = this->find_file(created_file, nullptr);
        if (!file_found)
        {
            // file is not in dir file_list
            const auto full_path = std::filesystem::path() / this->path / created_file;
            if (std::filesystem::exists(full_path))
            {
                vfs::file_info file = vfs_file_info_new(full_path);
                // add new file to dir file_list
                this->file_list.emplace_back(file);

                this->run_event<spacefm::signal::file_created>(file);
            }
            // else file does not exist in filesystem
        }
        else
        {
            // file already exists in dir file_list
            vfs::file_info file = file_found;
            if (this->update_file_info(file))
            {
                this->run_event<spacefm::signal::file_changed>(file);
            }
            // else was deleted, signaled, and unrefed in update_file_info
        }
    }
    this->created_files.clear();
}

void
VFSDir::unload_thumbnails(bool is_big) noexcept
{
    std::lock_guard<std::mutex> lock(this->mutex);

    for (const vfs::file_info& file : this->file_list)
    {
        if (is_big)
        {
            file->unload_big_thumbnail();
        }
        else
        {
            file->unload_small_thumbnail();
        }
    }

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
    malloc_trim(0);
}

/* signal handlers */
void
VFSDir::emit_file_created(const std::filesystem::path& file_name, bool force) noexcept
{
    (void)force;
    // Ignore avoid_changes for creation of files
    // if ( !force && dir->avoid_changes )
    //    return;

    if (std::filesystem::equivalent(file_name, this->path))
    { // Special Case: The directory itself was created?
        return;
    }

    this->created_files.emplace_back(file_name);
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
VFSDir::emit_file_deleted(const std::filesystem::path& file_name,
                          const vfs::file_info& file) noexcept
{
    std::lock_guard<std::mutex> lock(this->mutex);

    if (std::filesystem::equivalent(file_name, this->path))
    {
        /* Special Case: The directory itself was deleted... */

        /* clear the whole list */
        this->file_list.clear();

        this->run_event<spacefm::signal::file_deleted>(nullptr);

        return;
    }

    vfs::file_info file_found = this->find_file(file_name, file);
    if (file_found)
    {
        if (!ztd::contains(this->changed_files, file_found))
        {
            this->changed_files.emplace_back(file_found);
            if (change_notify_timeout == 0)
            {
                change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                           200,
                                                           (GSourceFunc)notify_file_change,
                                                           nullptr,
                                                           nullptr);
            }
        }
    }
}

void
VFSDir::emit_file_changed(const std::filesystem::path& file_name, const vfs::file_info& file,
                          bool force) noexcept
{
    std::lock_guard<std::mutex> lock(this->mutex);

    // ztd::logger::info("vfs_dir_emit_file_changed dir={} file_name={} avoid={}", dir->path,
    // file_name, this->avoid_changes ? "true" : "false");

    if (!force && this->avoid_changes)
    {
        return;
    }

    if (std::filesystem::equivalent(file_name, this->path))
    {
        // Special Case: The directory itself was changed
        this->run_event<spacefm::signal::file_changed>(nullptr);
        return;
    }

    vfs::file_info file_found = this->find_file(file_name, file);
    if (file_found)
    {
        if (!ztd::contains(this->changed_files, file_found))
        {
            if (force)
            {
                this->changed_files.emplace_back(file_found);
                if (change_notify_timeout == 0)
                {
                    change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                               100,
                                                               (GSourceFunc)notify_file_change,
                                                               nullptr,
                                                               nullptr);
                }
            }
            else if (this->update_file_info(file_found)) // update file info the first time
            {
                this->changed_files.emplace_back(file_found);
                if (change_notify_timeout == 0)
                {
                    change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                               500,
                                                               (GSourceFunc)notify_file_change,
                                                               nullptr,
                                                               nullptr);
                }
                this->run_event<spacefm::signal::file_changed>(file_found);
            }
        }
    }
}

void
VFSDir::emit_thumbnail_loaded(const vfs::file_info& file) noexcept
{
    std::lock_guard<std::mutex> lock(this->mutex);

    vfs::file_info file_found = this->find_file(file->name(), file);
    if (file_found)
    {
        assert(file == file_found);
        this->run_event<spacefm::signal::file_thumbnail_loaded>(file_found);
    }
}
