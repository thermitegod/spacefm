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

#include <vector>

#include <chrono>

#include <fcntl.h>
#include <utime.h>
#include <sys/types.h>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-dialog.hxx"

#include "terminal-handlers.hxx"

#include "main-window.hxx"
#include "vfs/vfs-volume.hxx"

#include "scripts.hxx"
#include "write.hxx"
#include "utils.hxx"

#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-file-task.hxx"
#include "vfs/vfs-file-trash.hxx"

/*
 * source_files sould be a newly allocated list, and it will be
 * freed after file operation has been completed
 */
vfs::file_task
vfs_task_new(VFSFileTaskType type, const std::vector<std::string>& src_files,
             std::string_view dest_dir)
{
    vfs::file_task task = new VFSFileTask(type, src_files, dest_dir);

    return task;
}

void
vfs_file_task_free(vfs::file_task task)
{
    delete task;
}

VFSFileTask::VFSFileTask(VFSFileTaskType type, const std::vector<std::string>& src_files,
                         std::string_view dest_dir)
{
    this->type = type;
    this->src_paths = src_files;
    if (!dest_dir.empty())
        this->dest_dir = dest_dir.data();

    this->is_recursive =
        (this->type == VFSFileTaskType::COPY || this->type == VFSFileTaskType::DELETE);

    // Init GMutex
    this->mutex = (GMutex*)g_malloc(sizeof(GMutex));
    g_mutex_init(this->mutex);

    GtkTextIter iter;
    this->add_log_buf = gtk_text_buffer_new(nullptr);
    this->add_log_end = gtk_text_mark_new(nullptr, false);
    gtk_text_buffer_get_end_iter(this->add_log_buf, &iter);
    gtk_text_buffer_add_mark(this->add_log_buf, this->add_log_end, &iter);

    this->start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    this->timer = ztd::timer();
}

VFSFileTask::~VFSFileTask()
{
    g_slist_free(this->devs);

    if (this->chmod_actions)
        g_slice_free1(sizeof(unsigned char) * magic_enum::enum_count<ChmodActionType>(),
                      this->chmod_actions);

    g_mutex_clear(this->mutex);
    free(this->mutex);

    gtk_text_buffer_set_text(this->add_log_buf, "", -1);
    g_object_unref(this->add_log_buf);
}

void
VFSFileTask::lock()
{
    g_mutex_lock(this->mutex);
}

void
VFSFileTask::unlock()
{
    g_mutex_unlock(this->mutex);
}

void
VFSFileTask::set_state_callback(VFSFileTaskStateCallback cb, void* user_data)
{
    this->state_cb = cb;
    this->state_cb_data = user_data;
}

// Set some actions for chmod, this array will be copied
// and stored in VFSFileTask
void
VFSFileTask::set_chmod(unsigned char* new_chmod_actions)
{
    this->chmod_actions = (unsigned char*)g_slice_alloc(sizeof(unsigned char) *
                                                        magic_enum::enum_count<ChmodActionType>());
    memcpy((void*)this->chmod_actions,
           (void*)new_chmod_actions,
           sizeof(unsigned char) * magic_enum::enum_count<ChmodActionType>());
}

void
VFSFileTask::set_chown(uid_t new_uid, gid_t new_gid)
{
    this->uid = new_uid;
    this->gid = new_gid;
}

void
VFSFileTask::set_recursive(bool recursive)
{
    this->is_recursive = recursive;
}

void
VFSFileTask::set_overwrite_mode(VFSFileTaskOverwriteMode mode)
{
    this->overwrite_mode = mode;
}

void
VFSFileTask::append_add_log(std::string_view msg)
{
    this->lock();
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(this->add_log_buf, &iter, this->add_log_end);
    gtk_text_buffer_insert(this->add_log_buf, &iter, msg.data(), msg.length());
    this->unlock();
}

bool
VFSFileTask::should_abort()
{
    if (this->state_pause != VFSFileTaskState::RUNNING)
    {
        // paused or queued - suspend thread
        this->lock();
        this->timer.stop();
        this->pause_cond = g_cond_new();
        g_cond_wait(this->pause_cond, this->mutex);
        // resume
        g_cond_free(this->pause_cond);
        this->pause_cond = nullptr;
        this->last_elapsed = this->timer.elapsed();
        this->last_progress = this->progress;
        this->last_speed = 0;
        this->timer.start();
        this->state_pause = VFSFileTaskState::RUNNING;
        this->unlock();
    }
    return this->abort;
}

const std::string
vfs_file_task_get_unique_name(std::string_view dest_dir, std::string_view base_name,
                              std::string_view ext)
{ // returns nullptr if all names used; otherwise newly allocated string
    std::string new_name;
    if (ext.empty())
        new_name = base_name.data();
    else
        new_name = fmt::format("{}.{}", base_name, ext);
    std::string new_dest_file = Glib::build_filename(dest_dir.data(), new_name);

    u32 n = 1;
    while (std::filesystem::exists(new_dest_file))
    {
        if (ext.empty())
            new_name = fmt::format("{}-copy{}", base_name, ++n);
        else
            new_name = fmt::format("{}-copy{}.{}", base_name, ++n, ext);

        new_dest_file = Glib::build_filename(dest_dir.data(), new_name);
    }

    return new_dest_file;
}

/*
 * Check if the destination file exists.
 * If the dest_file exists, let the user choose a new destination,
 * skip/overwrite/auto-rename/all, pause, or cancel.
 * The returned string is the new destination file chosen by the user
 */
bool
VFSFileTask::check_overwrite(std::string_view dest_file, bool* dest_exists, char** new_dest_file)
{
    char* new_dest;
    const auto dest_file_stat = ztd::lstat(dest_file);

    while (true)
    {
        *new_dest_file = nullptr;
        if (this->overwrite_mode == VFSFileTaskOverwriteMode::OVERWRITE_ALL)
        {
            *dest_exists = dest_file_stat.is_valid();
            if (ztd::same(this->current_file, this->current_dest))
            {
                // src and dest are same file - do not overwrite (truncates)
                // occurs if user pauses task and changes overwrite mode
                return false;
            }
            return true;
        }
        else if (this->overwrite_mode == VFSFileTaskOverwriteMode::SKIP_ALL)
        {
            *dest_exists = dest_file_stat.is_valid();
            return !*dest_exists;
        }
        else if (this->overwrite_mode == VFSFileTaskOverwriteMode::AUTO_RENAME)
        {
            *dest_exists = dest_file_stat.is_valid();
            if (!*dest_exists)
                return !this->abort;

            // auto-rename
            const std::string old_name = Glib::path_get_basename(dest_file.data());
            const std::string dest_file_dir = Glib::path_get_dirname(dest_file.data());

            const auto namepack = get_name_extension(old_name);
            const std::string name = namepack.first;
            const std::string ext = namepack.second;

            *new_dest_file = ztd::strdup(vfs_file_task_get_unique_name(dest_file_dir, name, ext));
            *dest_exists = false;
            if (*new_dest_file)
                return !this->abort;
            // else ran out of names - fall through to query user
        }

        *dest_exists = dest_file_stat.is_valid();
        if (!*dest_exists)
            return !this->abort;

        // dest exists - query user
        if (!this->state_cb) // failsafe
            return false;

        const char* use_dest_file = ztd::strdup(dest_file.data());
        do
        {
            // destination file exists
            *dest_exists = true;
            this->state = VFSFileTaskState::QUERY_OVERWRITE;
            new_dest = nullptr;

            // query user
            if (!this->state_cb(this,
                                VFSFileTaskState::QUERY_OVERWRITE,
                                &new_dest,
                                this->state_cb_data))
                // this->abort is actually set in query_overwrite_response
                // VFSFileTaskState::QUERY_OVERWRITE never returns false
                this->abort = true;
            this->state = VFSFileTaskState::RUNNING;

            // may pause here - user may change overwrite mode
            if (this->should_abort())
            {
                free(new_dest);
                return false;
            }

            if (this->overwrite_mode != VFSFileTaskOverwriteMode::RENAME)
            {
                free(new_dest);
                new_dest = nullptr;
                switch (this->overwrite_mode)
                {
                    case VFSFileTaskOverwriteMode::OVERWRITE:
                    case VFSFileTaskOverwriteMode::OVERWRITE_ALL:
                        *dest_exists = dest_file_stat.is_valid();
                        if (ztd::same(this->current_file, this->current_dest))
                        {
                            // src and dest are same file - do not overwrite (truncates)
                            // occurs if user pauses task and changes overwrite mode
                            return false;
                        }
                        return true;
                        break;
                    case VFSFileTaskOverwriteMode::SKIP_ALL:
                    case VFSFileTaskOverwriteMode::AUTO_RENAME:
                    case VFSFileTaskOverwriteMode::SKIP:
                    case VFSFileTaskOverwriteMode::RENAME:
                        return false;
                }
            }
            // user renamed file or pressed Pause btn
            if (new_dest)
                // user renamed file - test if new name exists
                use_dest_file = new_dest;
        } while (std::filesystem::exists(use_dest_file));
        if (new_dest)
        {
            // user renamed file to unique name
            *dest_exists = false;
            *new_dest_file = new_dest;
            return !this->abort;
        }
    }
}

bool
VFSFileTask::check_dest_in_src(std::string_view src_dir)
{
    if (this->dest_dir.empty())
        return false;

    const std::string real_dest_path = std::filesystem::canonical(this->dest_dir);
    const std::string real_src_path = std::filesystem::canonical(src_dir);

    if (!ztd::startswith(real_dest_path, real_src_path))
        return false;

    // source is contained in destination dir
    const std::string disp_src = Glib::filename_display_name(src_dir.data());
    const std::string disp_dest = Glib::filename_display_name(this->dest_dir);
    const std::string err =
        fmt::format("Destination directory \"{}\" is contained in source \"{}\"",
                    disp_dest,
                    disp_src);
    this->append_add_log(err);
    if (this->state_cb)
        this->state_cb(this, VFSFileTaskState::ERROR, nullptr, this->state_cb_data);
    this->state = VFSFileTaskState::RUNNING;

    return true;
}

static void
update_file_display(std::string_view path)
{
    // for devices like nfs, emit created and flush to avoid a
    // blocking stat call in GUI thread during writes
    const std::string dir_path = Glib::path_get_dirname(path.data());
    vfs::dir vdir = vfs_dir_get_by_path_soft(dir_path);
    if (vdir && vdir->avoid_changes)
    {
        vfs::file_info file = vfs_file_info_new();
        vfs_file_info_get(file, path);
        vfs_dir_emit_file_created(vdir, file->get_name(), true);
        vfs_file_info_unref(file);
        vfs_dir_flush_notify_cache();
    }
    if (vdir)
    {
        g_object_unref(vdir);
    }
}

void
VFSFileTask::file_copy(std::string_view src_file)
{
    const std::string file_name = Glib::path_get_basename(src_file.data());
    const std::string dest_file = Glib::build_filename(this->dest_dir, file_name);

    this->do_file_copy(src_file, dest_file);
}

bool
VFSFileTask::do_file_copy(std::string_view src_file, std::string_view dest_file)
{
    if (this->should_abort())
        return false;

    // LOG_INFO("vfs_file_task_do_copy( {}, {} )", src_file, dest_file);
    this->lock();
    this->current_file = src_file;
    this->current_dest = dest_file;
    this->current_item++;
    this->unlock();

    const auto file_stat = ztd::lstat(src_file);
    if (!file_stat.is_valid())
    {
        this->task_error(errno, "Accessing", src_file);
        return false;
    }

    char* new_dest_file = nullptr;
    bool dest_exists;
    bool copy_fail = false;

    std::string actual_dest_file = dest_file.data();

    if (std::filesystem::is_directory(src_file))
    {
        if (this->check_dest_in_src(src_file))
        {
            if (new_dest_file)
                free(new_dest_file);
            return false;
        }
        if (!this->check_overwrite(actual_dest_file, &dest_exists, &new_dest_file))
        {
            if (new_dest_file)
                free(new_dest_file);
            return false;
        }
        if (new_dest_file)
        {
            actual_dest_file = new_dest_file;
            this->lock();
            this->current_dest = actual_dest_file;
            this->unlock();
        }

        if (!dest_exists)
        {
            std::filesystem::create_directories(actual_dest_file);
            std::filesystem::permissions(actual_dest_file, std::filesystem::perms::owner_all);
        }

        if (std::filesystem::is_directory(src_file))
        {
            struct utimbuf times;
            this->lock();
            this->progress += file_stat.size();
            this->unlock();

            for (const auto& file : std::filesystem::directory_iterator(src_file))
            {
                const std::string file_name = std::filesystem::path(file).filename();
                if (this->should_abort())
                    break;
                const std::string sub_src_file = Glib::build_filename(src_file.data(), file_name);
                const std::string sub_dest_file = Glib::build_filename(actual_dest_file, file_name);
                if (!this->do_file_copy(sub_src_file, sub_dest_file) && !copy_fail)
                    copy_fail = true;
            }

            chmod(actual_dest_file.data(), file_stat.mode());
            times.actime = file_stat.atime();
            times.modtime = file_stat.mtime();
            utime(actual_dest_file.data(), &times);

            if (this->avoid_changes)
                update_file_display(actual_dest_file);

            /* Move files to different device: Need to delete source dir */
            if ((this->type == VFSFileTaskType::MOVE) && !this->should_abort() && !copy_fail)
            {
                std::filesystem::remove_all(src_file);
                if (std::filesystem::exists(src_file))
                {
                    this->task_error(errno, "Removing", src_file);
                    copy_fail = true;
                    if (this->should_abort())
                        if (new_dest_file)
                            free(new_dest_file);
                    return false;
                }
            }
        }
        else
        {
            this->task_error(errno, "Creating Dir", actual_dest_file);
            copy_fail = true;
        }
    }
    else if (std::filesystem::is_symlink(src_file))
    {
        bool read_symlink = false;

        std::string target_path;
        try
        {
            target_path = std::filesystem::read_symlink(src_file);
            read_symlink = true;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            LOG_WARN("{}", e.what());
        }

        if (read_symlink)
        {
            if (!this->check_overwrite(actual_dest_file, &dest_exists, &new_dest_file))
            {
                if (new_dest_file)
                    free(new_dest_file);
                return false;
            }

            if (new_dest_file)
            {
                actual_dest_file = new_dest_file;
                this->lock();
                this->current_dest = actual_dest_file;
                this->unlock();
            }

            // MOD delete it first to prevent exists error
            if (dest_exists)
            {
                std::filesystem::remove(actual_dest_file);
                if (std::filesystem::exists(actual_dest_file) && errno != 2 /* no such file */)
                {
                    this->task_error(errno, "Removing", actual_dest_file);
                    if (new_dest_file)
                        free(new_dest_file);
                    return false;
                }
            }

            std::filesystem::create_symlink(target_path, actual_dest_file);
            if (std::filesystem::is_symlink(actual_dest_file))
            {
                /* Move files to different device: Need to delete source files */
                if ((this->type == VFSFileTaskType::MOVE) && !copy_fail)
                {
                    std::filesystem::remove(src_file);
                    if (std::filesystem::exists(src_file))
                    {
                        this->task_error(errno, "Removing", src_file);
                        copy_fail = true;
                    }
                }
                this->lock();
                this->progress += file_stat.size();
                this->unlock();
            }
            else
            {
                this->task_error(errno, "Creating Link", actual_dest_file);
                copy_fail = true;
            }
        }
        else
        {
            this->task_error(errno, "Accessing", src_file);
            copy_fail = true;
        }
    }
    else
    {
        char buffer[4096];
        i32 rfd;
        i32 wfd;

        if ((rfd = open(src_file.data(), O_RDONLY)) >= 0)
        {
            if (!this->check_overwrite(actual_dest_file, &dest_exists, &new_dest_file))
            {
                close(rfd);
                if (new_dest_file)
                    free(new_dest_file);
                return false;
            }

            if (new_dest_file)
            {
                actual_dest_file = new_dest_file;
                this->lock();
                this->current_dest = actual_dest_file;
                this->unlock();
            }

            // MOD if dest is a symlink, delete it first to prevent overwriting target!
            if (std::filesystem::is_symlink(actual_dest_file))
            {
                std::filesystem::remove(actual_dest_file);
                if (std::filesystem::exists(src_file))
                {
                    this->task_error(errno, "Removing", actual_dest_file);
                    close(rfd);
                    if (new_dest_file)
                        free(new_dest_file);
                    return false;
                }
            }

            if ((wfd = creat(actual_dest_file.data(), file_stat.mode() | S_IWUSR)) >= 0)
            {
                // sshfs becomes unresponsive with this, nfs is okay with it
                // if (this->avoid_changes)
                //    emit_created(actual_dest_file);
                struct utimbuf times;
                isize rsize;
                while ((rsize = read(rfd, buffer, sizeof(buffer))) > 0)
                {
                    if (this->should_abort())
                    {
                        copy_fail = true;
                        break;
                    }

                    if (write(wfd, buffer, rsize) > 0)
                    {
                        this->lock();
                        this->progress += rsize;
                        this->unlock();
                    }
                    else
                    {
                        this->task_error(errno, "Writing", actual_dest_file);
                        copy_fail = true;
                        break;
                    }
                }
                close(wfd);
                if (copy_fail)
                {
                    std::filesystem::remove(actual_dest_file);
                    if (std::filesystem::exists(src_file) && errno != 2 /* no such file */)
                    {
                        this->task_error(errno, "Removing", actual_dest_file);
                        copy_fail = true;
                    }
                }
                else
                {
                    // MOD do not chmod link
                    if (!std::filesystem::is_symlink(actual_dest_file))
                    {
                        chmod(actual_dest_file.data(), file_stat.mode());
                        times.actime = file_stat.atime();
                        times.modtime = file_stat.mtime();
                        utime(actual_dest_file.data(), &times);
                    }
                    if (this->avoid_changes)
                        update_file_display(actual_dest_file);

                    /* Move files to different device: Need to delete source files */
                    if ((this->type == VFSFileTaskType::MOVE) && !this->should_abort())
                    {
                        std::filesystem::remove(src_file);
                        if (std::filesystem::exists(src_file))
                        {
                            this->task_error(errno, "Removing", src_file);
                            copy_fail = true;
                        }
                    }
                }
            }
            else
            {
                this->task_error(errno, "Creating", actual_dest_file);
                copy_fail = true;
            }
            close(rfd);
        }
        else
        {
            this->task_error(errno, "Accessing", src_file);
            copy_fail = true;
        }
    }

    if (new_dest_file)
        free(new_dest_file);
    if (!copy_fail && this->error_first)
        this->error_first = false;
    return !copy_fail;
}

void
VFSFileTask::file_move(std::string_view src_file)
{
    if (this->should_abort())
        return;

    this->lock();
    this->current_file = src_file;
    this->unlock();

    const std::string file_name = Glib::path_get_basename(src_file.data());
    const std::string dest_file = Glib::build_filename(this->dest_dir, file_name);

    const auto src_stat = ztd::lstat(src_file);
    const auto dest_stat = ztd::stat(this->dest_dir);

    if (src_stat.is_valid() && dest_stat.is_valid())
    {
        /* Not on the same device */
        if (src_stat.dev() != dest_stat.dev())
        {
            // LOG_INFO("not on the same dev: {}", src_file);
            this->do_file_copy(src_file, dest_file);
        }
        else
        {
            // LOG_INFO("on the same dev: {}", src_file);
            if (this->do_file_move(src_file, dest_file) == EXDEV)
            {
                // MOD Invalid cross-device link (st_dev not always accurate test)
                // so now redo move as copy
                this->do_file_copy(src_file, dest_file);
            }
        }
    }
    else
    {
        this->task_error(errno, "Accessing", src_file);
    }
}

i32
VFSFileTask::do_file_move(std::string_view src_file, std::string_view dest_path)
{
    if (this->should_abort())
        return 0;

    std::string dest_file = dest_path.data();

    this->lock();
    this->current_file = src_file;
    this->current_dest = dest_file;
    this->current_item++;
    this->unlock();

    // LOG_DEBUG("move '{}' to '{}'", src_file, dest_file);
    const auto file_stat = ztd::lstat(src_file);
    if (!file_stat.is_valid())
    {
        this->task_error(errno, "Accessing", src_file);
        return 0;
    }

    if (this->should_abort())
        return 0;

    if (file_stat.is_directory() && this->check_dest_in_src(src_file))
        return 0;

    char* new_dest_file = nullptr;
    bool dest_exists;
    if (!this->check_overwrite(dest_file, &dest_exists, &new_dest_file))
        return 0;

    if (new_dest_file)
    {
        dest_file = new_dest_file;
        this->lock();
        this->current_dest = dest_file;
        this->unlock();
    }

    if (std::filesystem::is_directory(dest_file))
    {
        // moving a directory onto a directory that exists
        for (const auto& file : std::filesystem::directory_iterator(src_file))
        {
            const std::string file_name = std::filesystem::path(file).filename();
            if (this->should_abort())
                break;
            const std::string sub_src_file = Glib::build_filename(src_file.data(), file_name);
            const std::string sub_dest_file = Glib::build_filename(dest_file, file_name);
            this->do_file_move(sub_src_file, sub_dest_file);
        }
        // remove moved src dir if empty
        if (!this->should_abort())
            std::filesystem::remove_all(src_file);

        return 0;
    }

    std::error_code err;
    std::filesystem::rename(src_file, dest_file, err);

    if (err.value() != 0)
    {
        if (err.value() == -1 && errno == EXDEV) // Invalid cross-link device
            return 18;

        this->task_error(errno, "Renaming", src_file);
        if (this->should_abort())
        {
            free(new_dest_file);
            return 0;
        }
    }
    else if (!std::filesystem::is_symlink(dest_file))
    { // do not chmod link
        chmod(dest_file.data(), file_stat.mode());
    }

    this->lock();
    this->progress += file_stat.size();
    if (this->error_first)
        this->error_first = false;
    this->unlock();

    if (new_dest_file)
        free(new_dest_file);
    return 0;
}

void
VFSFileTask::file_trash(std::string_view src_file)
{
    if (this->should_abort())
        return;

    this->lock();
    this->current_file = src_file;
    this->current_item++;
    this->unlock();

    const auto file_stat = ztd::lstat(src_file);
    if (!file_stat.is_valid())
    {
        this->task_error(errno, "Accessing", src_file);
        return;
    }

    if (!have_rw_access(src_file))
    {
        // this->task_error(errno, "Trashing", src_file);
        LOG_ERROR("Trashing failed missing RW permissions '{}'", src_file);
        return;
    }

    const bool result = VFSTrash::trash(src_file);

    if (!result)
    {
        this->task_error(errno, "Trashing", src_file);
        return;
    }

    this->lock();
    this->progress += file_stat.size();
    if (this->error_first)
        this->error_first = false;
    this->unlock();
}

void
VFSFileTask::file_delete(std::string_view src_file)
{
    if (this->should_abort())
        return;

    this->lock();
    this->current_file = src_file.data();
    this->current_item++;
    this->unlock();

    const auto file_stat = ztd::lstat(src_file);
    if (!file_stat.is_valid())
    {
        this->task_error(errno, "Accessing", src_file);
        return;
    }

    if (file_stat.is_directory())
    {
        for (const auto& file : std::filesystem::directory_iterator(src_file))
        {
            const std::string file_name = std::filesystem::path(file).filename();
            if (this->should_abort())
                break;
            const std::string sub_src_file = Glib::build_filename(src_file.data(), file_name);
            this->file_delete(sub_src_file);
        }

        if (this->should_abort())
            return;
        std::filesystem::remove_all(src_file);
        if (std::filesystem::exists(src_file))
        {
            this->task_error(errno, "Removing", src_file);
            return;
        }
    }
    else
    {
        std::filesystem::remove(src_file);
        if (std::filesystem::exists(src_file))
        {
            this->task_error(errno, "Removing", src_file);
            return;
        }
    }
    this->lock();
    this->progress += file_stat.size();
    if (this->error_first)
        this->error_first = false;
    this->unlock();
}

void
VFSFileTask::file_link(std::string_view src_file)
{
    if (this->should_abort())
        return;

    const std::string file_name = Glib::path_get_basename(src_file.data());
    const std::string old_dest_file = Glib::build_filename(this->dest_dir, file_name);
    std::string dest_file = old_dest_file;

    // MOD  setup task for check overwrite
    if (this->should_abort())
        return;

    this->lock();
    this->current_file = src_file.data();
    this->current_dest = old_dest_file;
    this->current_item++;
    this->unlock();

    const auto src_stat = ztd::stat(src_file);
    if (!src_stat.is_valid())
    {
        // MOD allow link to broken symlink
        if (errno != 2 || !std::filesystem::is_symlink(src_file))
        {
            this->task_error(errno, "Accessing", src_file);
            if (this->should_abort())
                return;
        }
    }

    /* FIXME: Check overwrite!! */ // MOD added check overwrite
    bool dest_exists;
    char* new_dest_file = nullptr;
    if (!this->check_overwrite(dest_file, &dest_exists, &new_dest_file))
        return;

    if (new_dest_file)
    {
        dest_file = new_dest_file;
        this->lock();
        this->current_dest = dest_file;
        this->unlock();
    }

    // MOD if dest exists, delete it first to prevent exists error
    if (dest_exists)
    {
        std::filesystem::remove(dest_file);
        if (std::filesystem::exists(src_file))
        {
            this->task_error(errno, "Removing", dest_file);
            return;
        }
    }

    std::filesystem::create_symlink(src_file, dest_file);
    if (!std::filesystem::is_symlink(dest_file))
    {
        this->task_error(errno, "Creating Link", dest_file);
        if (this->should_abort())
            return;
    }

    this->lock();
    this->progress += src_stat.size();
    if (this->error_first)
        this->error_first = false;
    this->unlock();

    if (new_dest_file)
        free(new_dest_file);
}

void
VFSFileTask::file_chown_chmod(std::string_view src_file)
{
    if (this->should_abort())
        return;

    this->lock();
    this->current_file = src_file;
    this->current_item++;
    this->unlock();
    // LOG_DEBUG("chmod_chown: {}", src_file);

    const auto src_stat = ztd::lstat(src_file);
    if (src_stat.is_valid())
    {
        /* chown */
        i32 result;
        if (!this->uid || !this->gid)
        {
            result = chown(src_file.data(), this->uid, this->gid);
            if (result != 0)
            {
                this->task_error(errno, "chown", src_file);
                if (this->should_abort())
                    return;
            }
        }

        /* chmod */
        if (this->chmod_actions)
        {
            std::filesystem::perms new_perms;
            std::filesystem::perms orig_perms = std::filesystem::status(src_file).permissions();

            for (usize i = 0; i < magic_enum::enum_count<ChmodActionType>(); ++i)
            {
                if (this->chmod_actions[i] == 2) /* Do not change */
                    continue;
                if (this->chmod_actions[i] == 0) /* Remove this bit */
                    new_perms &= ~chmod_flags.at(i);
                else /* Add this bit */
                    new_perms |= chmod_flags.at(i);
            }
            if (new_perms != orig_perms)
            {
                std::error_code ec;
                std::filesystem::permissions(src_file, new_perms, ec);
                if (ec)
                {
                    this->task_error(errno, "chmod", src_file);
                    if (this->should_abort())
                        return;
                }
            }
        }

        this->lock();
        this->progress += src_stat.size();
        this->unlock();

        if (this->avoid_changes)
            update_file_display(src_file);

        if (src_stat.is_directory() && this->is_recursive)
        {
            for (const auto& file : std::filesystem::directory_iterator(src_file))
            {
                const std::string file_name = std::filesystem::path(file).filename();
                if (this->should_abort())
                    break;
                const std::string sub_src_file = Glib::build_filename(src_file.data(), file_name);
                this->file_chown_chmod(sub_src_file);
            }
        }
    }
    if (this->error_first)
        this->error_first = false;
}

static void
call_state_callback(vfs::file_task task, VFSFileTaskState state)
{
    task->state = state;

    if (!task->state_cb)
        return;

    if (!task->state_cb(task, state, nullptr, task->state_cb_data))
    {
        task->abort = true;
        if (task->type == VFSFileTaskType::EXEC && task->exec_cond)
        {
            // this is used only if exec task run in non-main loop thread
            task->lock();
            g_cond_broadcast(task->exec_cond);
            task->unlock();
        }
    }
    else
    {
        task->state = VFSFileTaskState::RUNNING;
    }
}

static void
cb_exec_child_cleanup(pid_t pid, i32 status, char* tmp_file)
{ // delete tmp files after async task terminates
    // LOG_INFO("cb_exec_child_cleanup pid={} status={} file={}", pid, status, tmp_file );
    g_spawn_close_pid(pid);
    if (tmp_file)
    {
        std::filesystem::remove(tmp_file);
        free(tmp_file);
    }
    LOG_INFO("async child finished  pid={} status={}", pid, status);
}

static void
cb_exec_child_watch(pid_t pid, i32 status, vfs::file_task task)
{
    bool bad_status = false;
    g_spawn_close_pid(pid);
    task->exec_pid = 0;
    task->child_watch = 0;

    if (status)
    {
        if (WIFEXITED(status))
        {
            task->exec_exit_status = WEXITSTATUS(status);
        }
        else
        {
            bad_status = true;
            task->exec_exit_status = -1;
        }
    }
    else
        task->exec_exit_status = 0;

    if (!task->exec_keep_tmp)
    {
        if (std::filesystem::exists(task->exec_script))
            std::filesystem::remove(task->exec_script);
    }

    LOG_INFO("child finished  pid={} exit_status={}",
             pid,
             bad_status ? -1 : task->exec_exit_status);

    if (!task->exec_exit_status && !bad_status)
    {
        if (!task->custom_percent)
            task->percent = 100;
    }
    else
    {
        call_state_callback(task, VFSFileTaskState::ERROR);
    }

    if (bad_status || (!task->exec_channel_out && !task->exec_channel_err))
        call_state_callback(task, VFSFileTaskState::FINISH);
}

static bool
cb_exec_out_watch(GIOChannel* channel, GIOCondition cond, vfs::file_task task)
{
    if ((cond & G_IO_NVAL))
    {
        g_io_channel_unref(channel);
        return false;
    }
    else if (!(cond & G_IO_IN))
    {
        if ((cond & G_IO_HUP))
        {
            g_io_channel_unref(channel);
            if (channel == task->exec_channel_out)
            {
                task->exec_channel_out = nullptr;
            }
            else if (channel == task->exec_channel_err)
            {
                task->exec_channel_err = nullptr;
            }
            if (!task->exec_channel_out && !task->exec_channel_err && !task->exec_pid)
            {
                call_state_callback(task, VFSFileTaskState::FINISH);
            }
            return false;
        }
        else
        {
            return true;
        }
    }
    else if (!(fcntl(g_io_channel_unix_get_fd(channel), F_GETFL) != -1 || errno != EBADF))
    {
        // bad file descriptor - occurs with stop on fast output
        g_io_channel_unref(channel);
        if (channel == task->exec_channel_out)
        {
            task->exec_channel_out = nullptr;
        }
        else if (channel == task->exec_channel_err)
        {
            task->exec_channel_err = nullptr;
        }
        if (!task->exec_channel_out && !task->exec_channel_err && !task->exec_pid)
        {
            call_state_callback(task, VFSFileTaskState::FINISH);
        }
        return false;
    }

    // GError *error = nullptr;
    u64 size;
    char buf[2048];
    if (g_io_channel_read_chars(channel, buf, sizeof(buf), &size, nullptr) == G_IO_STATUS_NORMAL &&
        size > 0)
    {
        task->append_add_log(buf);
    }
    else
    {
        LOG_INFO("cb_exec_out_watch: g_io_channel_read_chars != G_IO_STATUS_NORMAL");
    }

    return true;
}

void
VFSFileTask::file_exec(std::string_view src_file)
{
    // this function is now thread safe but is not currently run in
    // another thread because gio adds watches to main loop thread anyway
    std::string su;
    std::string terminal;
    GtkWidget* parent = nullptr;

    // LOG_INFO("vfs_file_task_exec");
    // this->exec_keep_tmp = true;

    this->lock();

    if (this->exec_browser)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(this->exec_browser));
    else if (this->exec_desktop)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(this->exec_desktop));

    this->state = VFSFileTaskState::RUNNING;
    this->current_file = src_file;
    this->total_size = 0;
    this->percent = 0;
    this->unlock();

    if (this->should_abort())
        return;

    // need su?
    if (!this->exec_as_user.empty())
    {
        if (geteuid() == 0 && ztd::same(this->exec_as_user, "root"))
        { // already root so no su
            this->exec_as_user.clear();
        }
        else
        {
            // get su programs
            su = get_valid_su();
            if (su.empty())
            {
                const std::string msg =
                    "Configure a valid Terminal SU command in View|Preferences|Advanced";
                LOG_WARN(msg);
                // do not use xset_msg_dialog if non-main thread
                // this->task_error(0, str);
                xset_msg_dialog(parent,
                                GtkMessageType::GTK_MESSAGE_ERROR,
                                "Terminal SU Not Available",
                                GtkButtonsType::GTK_BUTTONS_OK,
                                msg);
                call_state_callback(this, VFSFileTaskState::FINISH);
                // LOG_INFO("vfs_file_task_exec DONE ERROR");
                return;
            }
        }
    }

    // make tmpdir
    const std::string tmp = vfs_user_get_tmp_dir();
    if (!std::filesystem::is_directory(tmp))
    {
        const std::string msg = "Cannot create temporary directory";
        LOG_WARN(msg);
        // do not use xset_msg_dialog if non-main thread
        // this->task_error(0, str);
        xset_msg_dialog(parent,
                        GtkMessageType::GTK_MESSAGE_ERROR,
                        "Error",
                        GtkButtonsType::GTK_BUTTONS_OK,
                        msg);
        call_state_callback(this, VFSFileTaskState::FINISH);
        // LOG_INFO("vfs_file_task_exec DONE ERROR");
        return;
    }

    // get terminal
    if (!this->exec_terminal && !this->exec_as_user.empty())
    { // using cli tool so run in terminal
        this->exec_terminal = true;
    }
    if (this->exec_terminal)
    {
        // get terminal
        terminal = Glib::find_program_in_path(xset_get_s(XSetName::MAIN_TERMINAL));
        if (terminal.empty())
        {
            const std::string msg =
                "Please set a valid terminal program in View|Preferences|Advanced";
            LOG_WARN(msg);
            // do not use xset_msg_dialog if non-main thread
            // this->task_error(0, str);
            xset_msg_dialog(parent,
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Terminal Not Available",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            msg);

            call_state_callback(this, VFSFileTaskState::FINISH);
            // LOG_INFO("vfs_file_task_exec DONE ERROR");
            return;
        }
    }

    std::string sum_script;

    // Build exec script
    if (!this->exec_direct)
    {
        // get script name
        while (true)
        {
            const std::string hexname = fmt::format("{}.sh", randhex8());
            this->exec_script = Glib::build_filename(tmp, hexname);
            if (!std::filesystem::exists(this->exec_script))
                break;
        }

        // open buffer
        std::string buf = "";

        // build - header
        buf.append(fmt::format("#!{}\n{}\n#tmp exec script\n", BASH_PATH, SHELL_SETTINGS));

        // build - exports
        if (this->exec_export && (this->exec_browser || this->exec_desktop))
        {
            if (this->exec_browser)
            {
                main_write_exports(this, this->current_dest.data(), buf);
            }
            else
            {
                this->task_error(errno, "Error writing temporary file");

                if (!this->exec_keep_tmp)
                {
                    if (std::filesystem::exists(this->exec_script))
                        std::filesystem::remove(this->exec_script);
                }
                call_state_callback(this, VFSFileTaskState::FINISH);
                // LOG_INFO("vfs_file_task_exec DONE ERROR");
                return;
            }
        }
        else
        {
            if (this->exec_export && !this->exec_browser && !this->exec_desktop)
            {
                this->exec_export = false;
                LOG_WARN("exec_export set without exec_browser/exec_desktop");
            }
        }

        // build - run
        // buf.append(fmt::format("#run\nif [ \"$1\" == \"run\" ];then\n\n"));

        // build - export vars
        if (this->exec_export)
            buf.append(fmt::format("export fm_import=\"source {}\"\n", this->exec_script));
        else
            buf.append(fmt::format("export fm_import=\"\"\n"));

        buf.append(fmt::format("export fm_source=\"{}\"\n\n", this->exec_script));

        // build - trap rm
        if (!this->exec_keep_tmp && geteuid() != 0 && ztd::same(this->exec_as_user, "root"))
        {
            // run as root command, clean up
            buf.append(fmt::format("trap \"rm -f {}; exit\" EXIT SIGINT SIGTERM SIGQUIT SIGHUP\n\n",
                                   this->exec_script));
        }

        // build - command
        print_task_command((char*)this->exec_ptask, this->exec_command);

        buf.append(fmt::format("{}\nfm_err=$?\n", this->exec_command));

        // build - press enter to close
        if (!terminal.empty() && this->exec_keep_terminal)
        {
            if (geteuid() == 0 || ztd::same(this->exec_as_user, "root"))
                buf.append(fmt::format("\necho;read -p '[ Finished ]  Press Enter to close: '\n"));
            else
            {
                buf.append(
                    fmt::format("\necho;read -p '[ Finished ]  Press Enter to close or s + "
                                "Enter for a shell: ' "
                                "s\nif [ \"$s\" = 's' ];then\n    if [ \"$(whoami)\" = "
                                "\"root\" ];then\n        "
                                "echo '\n[ {} ]'\n    fi\n    echo\n    You are ROOT\nfi\n\n",
                                BASH_PATH));
            }
        }

        buf.append(fmt::format("\nexit $fm_err\n"));
        // buf.append(fmt::format("\nfi\n"));

        const bool result = write_file(this->exec_script, buf);
        if (!result)
        {
            this->task_error(errno, "Error writing temporary file");

            if (!this->exec_keep_tmp)
            {
                if (std::filesystem::exists(this->exec_script))
                    std::filesystem::remove(this->exec_script);
            }
            call_state_callback(this, VFSFileTaskState::FINISH);
            // LOG_INFO("vfs_file_task_exec DONE ERROR");
            return;
        }

        // set permissions
        chmod(this->exec_script.data(), 0700);

        // use checksum
        if (geteuid() != 0 && (!this->exec_as_user.empty() || this->exec_checksum))
        {
            Glib::Checksum check = Glib::Checksum();
            sum_script =
                check.compute_checksum(Glib::Checksum::Type::MD5, this->exec_script.data());
        }
    }

    this->percent = 50;

    // Spawn
    std::vector<std::string> argv;

    pid_t pid;
    i32 out, err;
    std::string use_su;
    bool single_arg = false;
    std::string auth;

    if (!terminal.empty())
    {
        // terminal
        const auto terminal_args =
            terminal_handlers->get_terminal_args(xset_get_s(XSetName::MAIN_TERMINAL));
        for (std::string_view terminal_arg : terminal_args)
        {
            argv.emplace_back(terminal_arg);
        }

        use_su = su;
    }

    if (!this->exec_as_user.empty())
    {
        // su
        argv.emplace_back(use_su);
        if (!ztd::same(this->exec_as_user, "root"))
        {
            if (!ztd::same(use_su, "/bin/su"))
                argv.emplace_back("-u");
            argv.emplace_back(this->exec_as_user);
        }

        if (ztd::same(use_su, "/bin/su"))
        {
            // /bin/su
            argv.emplace_back("-s");
            argv.emplace_back(BASH_PATH);
            argv.emplace_back("-c");
            single_arg = true;
        }
    }

    if (!sum_script.empty())
    {
        // spacefm-auth exists?
        auth = get_script_path(Scripts::SPACEFM_AUTH);
        if (!script_exists(auth))
            sum_script.clear();
    }

    if (!sum_script.empty() && !auth.empty())
    {
        // spacefm-auth
        if (single_arg)
        {
            const std::string script =
                fmt::format("{} {} {} {} {}",
                            BASH_PATH,
                            auth,
                            ztd::same(this->exec_as_user, "root") ? "root" : "",
                            this->exec_script,
                            sum_script);
            argv.emplace_back(script);
        }
        else
        {
            argv.emplace_back(BASH_PATH);
            argv.emplace_back(auth);
            if (ztd::same(this->exec_as_user, "root"))
                argv.emplace_back("root");
            argv.emplace_back(this->exec_script);
            argv.emplace_back(sum_script);
        }
    }
    else if (this->exec_direct)
    {
        // add direct args - not currently used
        argv = this->exec_argv;
    }
    else
    {
        // argv.emplace_back(BASH_PATH);
        argv.emplace_back(this->exec_script);
        // argv.emplace_back("run");
    }

    try
    {
        if (this->exec_sync)
        {
            Glib::spawn_async_with_pipes(this->dest_dir,
                                         argv,
                                         Glib::SpawnFlags::DO_NOT_REAP_CHILD,
                                         Glib::SlotSpawnChildSetup(),
                                         &pid,
                                         nullptr,
                                         &out,
                                         &err);
        }
        else
        {
            Glib::spawn_async_with_pipes(this->dest_dir,
                                         argv,
                                         Glib::SpawnFlags::DO_NOT_REAP_CHILD,
                                         Glib::SlotSpawnChildSetup(),
                                         &pid,
                                         nullptr,
                                         nullptr,
                                         nullptr);
        }
    }
    catch (const Glib::SpawnError& e)
    {
        LOG_ERROR("    glib_error_code={}", e.code());

        if (errno)
        {
            const std::string errno_msg = std::strerror(errno);
            LOG_INFO("    result={} ( {} )", errno, errno_msg);
        }

        if (!this->exec_keep_tmp && this->exec_sync)
        {
            if (std::filesystem::exists(this->exec_script))
                std::filesystem::remove(this->exec_script);
        }
        const std::string cmd = ztd::join(argv, " ");
        const std::string msg =
            fmt::format("Error executing '{}'\nGlib Spawn Error Code {}, {}\nRun in a terminal "
                        "for full debug info\n",
                        cmd,
                        e.code(),
                        std::string(Glib::strerror(e.code())));
        this->task_error(errno, msg);
        call_state_callback(this, VFSFileTaskState::FINISH);
        // LOG_INFO("vfs_file_task_exec DONE ERROR");
        return;
    }

    print_task_command_spawn(argv, pid);

    if (!this->exec_sync)
    {
        // catch termination to waitpid and delete tmp if needed
        // task can be destroyed while this watch is still active
        g_child_watch_add(pid,
                          (GChildWatchFunc)cb_exec_child_cleanup,
                          !this->exec_keep_tmp && !this->exec_direct && this->exec_script.data()
                              ? ztd::strdup(this->exec_script)
                              : nullptr);
        call_state_callback(this, VFSFileTaskState::FINISH);
        return;
    }

    this->exec_pid = pid;

    // catch termination (always is run in the main loop thread)
    this->child_watch = g_child_watch_add(pid, (GChildWatchFunc)cb_exec_child_watch, this);

    // create channels for output
    fcntl(out, F_SETFL, O_NONBLOCK);
    fcntl(err, F_SETFL, O_NONBLOCK);
    this->exec_channel_out = g_io_channel_unix_new(out);
    this->exec_channel_err = g_io_channel_unix_new(err);
    g_io_channel_set_close_on_unref(this->exec_channel_out, true);
    g_io_channel_set_close_on_unref(this->exec_channel_err, true);

    // Add watches to channels
    // These are run in the main loop thread so use G_PRIORITY_LOW to not
    // interfere with g_idle_add in vfs-dir/vfs-async-task etc
    // "Use this for very low priority background tasks. It is not used within
    // GLib or GTK+."
    g_io_add_watch_full(this->exec_channel_out,
                        G_PRIORITY_LOW,
                        GIOCondition(G_IO_IN | G_IO_HUP | G_IO_NVAL | G_IO_ERR), // want ERR?
                        (GIOFunc)cb_exec_out_watch,
                        this,
                        nullptr);
    g_io_add_watch_full(this->exec_channel_err,
                        G_PRIORITY_LOW,
                        GIOCondition(G_IO_IN | G_IO_HUP | G_IO_NVAL | G_IO_ERR), // want ERR?
                        (GIOFunc)cb_exec_out_watch,
                        this,
                        nullptr);

    // running
    this->state = VFSFileTaskState::RUNNING;

    // LOG_INFO("vfs_file_task_exec DONE");
    return; // exit thread
}

static bool
on_size_timeout(vfs::file_task task)
{
    if (!task->abort)
        task->state = VFSFileTaskState::SIZE_TIMEOUT;
    return false;
}

static void*
vfs_file_task_thread(vfs::file_task task)
{
    u32 size_timeout = 0;
    if (task->type < VFSFileTaskType::MOVE || task->type >= VFSFileTaskType::LAST)
    {
        task->state = VFSFileTaskState::RUNNING;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task);
        }
        if (task->state_cb)
        {
            call_state_callback(task, VFSFileTaskState::FINISH);
        }
        return nullptr;
    }

    task->lock();
    task->state = VFSFileTaskState::RUNNING;
    task->current_file = task->src_paths.at(0);
    task->total_size = 0;
    task->unlock();

    if (task->abort)
    {
        task->state = VFSFileTaskState::RUNNING;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task);
        }

        if (task->state_cb)
        {
            call_state_callback(task, VFSFileTaskState::FINISH);
        }
        return nullptr;
    }

    /* Calculate total size of all files */
    if (task->is_recursive)
    {
        // start timer to limit the amount of time to spend on this - can be
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds(5, (GSourceFunc)on_size_timeout, task);
        for (std::string_view src_path : task->src_paths)
        {
            const auto file_stat = ztd::lstat(src_path);
            if (!file_stat.is_valid())
            {
                // do not report error here since its reported later
                // this->task_error(errno, "Accessing"sv, (char*)l->data);
            }
            else
            {
                const off_t size = task->get_total_size_of_dir(src_path);
                task->lock();
                task->total_size += size;
                task->unlock();
            }
            if (task->abort)
            {
                task->state = VFSFileTaskState::RUNNING;
                if (size_timeout)
                {
                    g_source_remove_by_user_data(task);
                }
                if (task->state_cb)
                {
                    call_state_callback(task, VFSFileTaskState::FINISH);
                }
                return nullptr;
            }
            if (task->state == VFSFileTaskState::SIZE_TIMEOUT)
                break;
        }
    }
    else if (task->type == VFSFileTaskType::TRASH)
    {
    }
    else if (task->type != VFSFileTaskType::EXEC)
    {
        dev_t dest_dev = 0;

        // start timer to limit the amount of time to spend on this - can be
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds(5, (GSourceFunc)on_size_timeout, task);
        if (task->type != VFSFileTaskType::CHMOD_CHOWN)
        {
            const auto file_stat = ztd::lstat(task->dest_dir);
            if (!(!task->dest_dir.empty() && file_stat.is_valid()))
            {
                task->task_error(errno, "Accessing", task->dest_dir);
                task->abort = true;
                task->state = VFSFileTaskState::RUNNING;
                if (size_timeout)
                {
                    g_source_remove_by_user_data(task);
                }
                if (task->state_cb)
                {
                    call_state_callback(task, VFSFileTaskState::FINISH);
                }
                return nullptr;
            }
            dest_dev = file_stat.dev();
        }

        for (std::string_view src_path : task->src_paths)
        {
            const auto file_stat = ztd::lstat(src_path);
            if (!file_stat.is_valid())
            {
                // do not report error here since it is reported later
                // task->error(errno, "Accessing", (char*)l->data);
            }
            else
            {
                if ((task->type == VFSFileTaskType::MOVE) && file_stat.dev() != dest_dev)
                {
                    // recursive size
                    const off_t size = task->get_total_size_of_dir(src_path);
                    task->lock();
                    task->total_size += size;
                    task->unlock();
                }
                else
                {
                    task->lock();
                    task->total_size += file_stat.size();
                    task->unlock();
                }
            }
            if (task->abort)
            {
                task->state = VFSFileTaskState::RUNNING;
                if (size_timeout)
                {
                    g_source_remove_by_user_data(task);
                }

                if (task->state_cb)
                {
                    call_state_callback(task, VFSFileTaskState::FINISH);
                }
                return nullptr;
            }
            if (task->state == VFSFileTaskState::SIZE_TIMEOUT)
                break;
        }
    }

    const auto dest_dir_stat = ztd::stat(task->dest_dir);
    if (!task->dest_dir.empty() && dest_dir_stat.is_valid())
        task->add_task_dev(dest_dir_stat.dev());

    if (task->abort)
    {
        task->state = VFSFileTaskState::RUNNING;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task);
        }
        if (task->state_cb)
        {
            call_state_callback(task, VFSFileTaskState::FINISH);
        }
        return nullptr;
    }

    // cancel the timer
    if (size_timeout)
    {
        g_source_remove_by_user_data(task);
    }

    if (task->state_pause == VFSFileTaskState::QUEUE)
    {
        if (task->state != VFSFileTaskState::SIZE_TIMEOUT && xset_get_b(XSetName::TASK_Q_SMART))
        {
            // make queue exception for smaller tasks
            off_t exlimit;
            switch (task->type)
            {
                case VFSFileTaskType::MOVE:
                case VFSFileTaskType::COPY:
                case VFSFileTaskType::TRASH:
                    exlimit = 10485760; // 10M
                    break;
                case VFSFileTaskType::DELETE:
                    exlimit = 5368709120; // 5G
                    break;
                case VFSFileTaskType::LINK:
                case VFSFileTaskType::CHMOD_CHOWN:
                case VFSFileTaskType::EXEC:
                case VFSFileTaskType::LAST:
                default:
                    exlimit = 0; // always exception for other types
                    break;
            }

            if (!exlimit || task->total_size < exlimit)
                task->state_pause = VFSFileTaskState::RUNNING;
        }
        // device list is populated so signal queue start
        task->queue_start = true;
    }

    if (task->state == VFSFileTaskState::SIZE_TIMEOUT)
    {
        task->append_add_log("Timed out calculating total size\n");
        task->total_size = 0;
    }
    task->state = VFSFileTaskState::RUNNING;
    if (task->should_abort())
    {
        task->state = VFSFileTaskState::RUNNING;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task);
        }
        if (task->state_cb)
        {
            call_state_callback(task, VFSFileTaskState::FINISH);
        }
        return nullptr;
    }

    for (std::string_view src_path : task->src_paths)
    {
        switch (task->type)
        {
            case VFSFileTaskType::MOVE:
                task->file_move(src_path);
                break;
            case VFSFileTaskType::COPY:
                task->file_copy(src_path);
                break;
            case VFSFileTaskType::TRASH:
                task->file_trash(src_path);
                break;
            case VFSFileTaskType::DELETE:
                task->file_delete(src_path);
                break;
            case VFSFileTaskType::LINK:
                task->file_link(src_path);
                break;
            case VFSFileTaskType::CHMOD_CHOWN:
                task->file_chown_chmod(src_path);
                break;
            case VFSFileTaskType::EXEC:
                task->file_exec(src_path);
                break;
            case VFSFileTaskType::LAST:
                break;
        }
    }

    task->state = VFSFileTaskState::RUNNING;
    if (size_timeout)
    {
        g_source_remove_by_user_data(task);
    }
    if (task->state_cb)
    {
        call_state_callback(task, VFSFileTaskState::FINISH);
    }
    return nullptr;
}

void
VFSFileTask::run_task()
{
    if (this->type != VFSFileTaskType::EXEC)
    {
        if (this->type == VFSFileTaskType::CHMOD_CHOWN && !this->src_paths.empty())
        {
            const std::string dir = Glib::path_get_dirname(this->src_paths.at(0));
            this->avoid_changes = vfs_volume_dir_avoid_changes(dir);
        }
        else
        {
            this->avoid_changes = vfs_volume_dir_avoid_changes(this->dest_dir);
        }

        this->thread = g_thread_new("task_run", (GThreadFunc)vfs_file_task_thread, this);
    }
    else
    {
        // do not use another thread for exec since gio adds watches to main
        // loop thread anyway
        this->thread = nullptr;
        this->file_exec(this->src_paths.at(0));
    }
}

void
VFSFileTask::try_abort_task()
{
    this->abort = true;
    if (this->pause_cond)
    {
        this->lock();
        g_cond_broadcast(this->pause_cond);
        this->last_elapsed = this->timer.elapsed();
        this->last_progress = this->progress;
        this->last_speed = 0;
        this->unlock();
    }
    else
    {
        this->lock();
        this->last_elapsed = this->timer.elapsed();
        this->last_progress = this->progress;
        this->last_speed = 0;
        this->unlock();
    }
    this->state_pause = VFSFileTaskState::RUNNING;
}

void
VFSFileTask::abort_task()
{
    this->abort = true;
    /* Called from another thread */
    if (this->thread && g_thread_self() != this->thread && this->type != VFSFileTaskType::EXEC)
    {
        g_thread_join(this->thread);
        this->thread = nullptr;
    }
}

void
VFSFileTask::add_task_dev(dev_t dev)
{
    if (!g_slist_find(this->devs, GUINT_TO_POINTER(dev)))
    {
        const dev_t parent = get_device_parent(dev);
        // LOG_INFO("add_task_dev {}:{}", major(dev), minor(dev));
        this->lock();
        this->devs = g_slist_append(this->devs, GUINT_TO_POINTER(dev));
        if (parent && !g_slist_find(this->devs, GUINT_TO_POINTER(parent)))
        {
            // LOG_INFO("add_task_dev PARENT {}:{}", major(parent), minor(parent));
            this->devs = g_slist_append(this->devs, GUINT_TO_POINTER(parent));
        }
        this->unlock();
    }
}

/*
 * void get_total_size_of_dir(const char* path, off_t* size)
 * Recursively count total size of all files in the specified directory.
 * If the path specified is a file, the size of the file is directly returned.
 * cancel is used to cancel the operation. This function will check the value
 * pointed by cancel in every iteration. If cancel is set to true, the
 * calculation is cancelled.
 * NOTE: *size should be set to zero before calling this function.
 */
off_t
VFSFileTask::get_total_size_of_dir(std::string_view path)
{
    if (this->abort)
        return 0;

    const auto file_stat = ztd::lstat(path);
    if (!file_stat.is_valid())
        return 0;

    off_t size = file_stat.size();

    // remember device for smart queue
    if (!this->devs)
        this->add_task_dev(file_stat.dev());
    else if (file_stat.dev() != GPOINTER_TO_UINT(this->devs->data))
        this->add_task_dev(file_stat.dev());

    // Do not follow symlinks
    if (file_stat.is_symlink() || !file_stat.is_directory())
        return size;

    for (const auto& file : std::filesystem::directory_iterator(path))
    {
        const std::string file_name = file.path().filename();
        if (this->state == VFSFileTaskState::SIZE_TIMEOUT || this->abort)
            break;

        const std::string full_path = Glib::build_filename(path.data(), file_name);
        if (std::filesystem::exists(full_path))
        {
            const auto dir_file_stat = ztd::lstat(full_path);

            if (dir_file_stat.is_directory())
                size += this->get_total_size_of_dir(full_path);
            else
                size += dir_file_stat.size();
        }
    }

    return size;
}

void
VFSFileTask::task_error(i32 errnox, std::string_view action)
{
    if (errnox)
    {
        const std::string errno_msg = std::strerror(errnox);
        const std::string msg = fmt::format("{}\n{}\n", action, errno_msg);
        this->append_add_log(msg);
    }
    else
    {
        const std::string msg = fmt::format("{}\n", action);
        this->append_add_log(msg);
    }

    call_state_callback(this, VFSFileTaskState::ERROR);
}

void
VFSFileTask::task_error(i32 errnox, std::string_view action, std::string_view target)
{
    this->error = errnox;
    const std::string errno_msg = std::strerror(errnox);
    const std::string msg = fmt::format("\n{} {}\nError: {}\n", action, target, errno_msg);
    this->append_add_log(msg);
    call_state_callback(this, VFSFileTaskState::ERROR);
}
