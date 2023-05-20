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

#include <span>

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
#include "vfs/vfs-utils.hxx"

#include "scripts.hxx"
#include "write.hxx"
#include "utils.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "vfs/vfs-file-task.hxx"
#include "vfs/vfs-file-trash.hxx"

/*
 * source_files sould be a newly allocated list, and it will be
 * freed after file operation has been completed
 */
vfs::file_task
vfs_task_new(vfs::file_task_type type, const std::span<const std::filesystem::path> src_files,
             const std::filesystem::path& dest_dir)
{
    const auto task = new VFSFileTask(type, src_files, dest_dir);

    return task;
}

void
vfs_file_task_free(vfs::file_task task)
{
    delete task;
}

VFSFileTask::VFSFileTask(vfs::file_task_type type,
                         const std::span<const std::filesystem::path> src_files,
                         const std::filesystem::path& dest_dir)
{
    this->type = type;
    this->src_paths = std::vector<std::filesystem::path>(src_files.begin(), src_files.end());
    if (!dest_dir.empty())
    {
        this->dest_dir = dest_dir;
    }

    this->is_recursive =
        (this->type == vfs::file_task_type::copy || this->type == vfs::file_task_type::DELETE);

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
    if (this->chmod_actions)
    {
        g_slice_free1(sizeof(unsigned char) * magic_enum::enum_count<vfs::chmod_action>(),
                      this->chmod_actions);
    }

    g_mutex_clear(this->mutex);
    std::free(this->mutex);

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
    this->chmod_actions = (unsigned char*)g_slice_alloc(
        sizeof(unsigned char) * magic_enum::enum_count<vfs::chmod_action>());
    memcpy((void*)this->chmod_actions,
           (void*)new_chmod_actions,
           sizeof(unsigned char) * magic_enum::enum_count<vfs::chmod_action>());
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
VFSFileTask::set_overwrite_mode(vfs::file_task_overwrite_mode mode)
{
    this->overwrite_mode = mode;
}

void
VFSFileTask::append_add_log(const std::string_view msg)
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
    if (this->state_pause != vfs::file_task_state::running)
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
        this->state_pause = vfs::file_task_state::running;
        this->unlock();
    }
    return this->abort;
}

/*
 * Check if the destination file exists.
 * If the dest_file exists, let the user choose a new destination,
 * skip/overwrite/auto-rename/all, pause, or cancel.
 * The returned string is the new destination file chosen by the user
 */
bool
VFSFileTask::check_overwrite(const std::filesystem::path& dest_file, bool* dest_exists,
                             char** new_dest_file)
{
    const auto dest_file_stat = ztd::lstat(dest_file);

    while (true)
    {
        *new_dest_file = nullptr;
        if (this->overwrite_mode == vfs::file_task_overwrite_mode::overwrite_all)
        {
            const auto checked_current_file = this->current_file.value_or("");
            const auto checked_current_dest = this->current_dest.value_or("");

            *dest_exists = dest_file_stat.is_valid();
            if (std::filesystem::equivalent(checked_current_file, checked_current_dest))
            {
                // src and dest are same file - do not overwrite (truncates)
                // occurs if user pauses task and changes overwrite mode
                return false;
            }
            return true;
        }
        else if (this->overwrite_mode == vfs::file_task_overwrite_mode::skip_all)
        {
            *dest_exists = dest_file_stat.is_valid();
            return !*dest_exists;
        }
        else if (this->overwrite_mode == vfs::file_task_overwrite_mode::auto_rename)
        {
            *dest_exists = dest_file_stat.is_valid();
            if (!*dest_exists)
            {
                return !this->abort;
            }

            // auto-rename
            const auto old_name = dest_file.filename();
            const auto dest_file_dir = dest_file.parent_path();

            const auto [filename_no_extension, filename_extension] = get_name_extension(old_name);

            *new_dest_file = ztd::strdup(
                vfs_get_unique_name(dest_file_dir, filename_no_extension, filename_extension));
            *dest_exists = false;
            if (*new_dest_file)
            {
                return !this->abort;
            }
            // else ran out of names - fall through to query user
        }

        *dest_exists = dest_file_stat.is_valid();
        if (!*dest_exists)
        {
            return !this->abort;
        }

        // dest exists - query user
        if (!this->state_cb)
        { // failsafe
            return false;
        }

        char* new_dest;
        std::filesystem::path use_dest_file = dest_file;
        do
        {
            // destination file exists
            *dest_exists = true;
            this->state = vfs::file_task_state::query_overwrite;
            new_dest = nullptr;

            // query user
            if (!this->state_cb(this,
                                vfs::file_task_state::query_overwrite,
                                &new_dest,
                                this->state_cb_data))
            {
                // this->abort is actually set in query_overwrite_response
                // vfs::file_task_state::QUERY_OVERWRITE never returns false
                this->abort = true;
            }
            this->state = vfs::file_task_state::running;

            // may pause here - user may change overwrite mode
            if (this->should_abort())
            {
                std::free(new_dest);
                return false;
            }

            if (this->overwrite_mode != vfs::file_task_overwrite_mode::rename)
            {
                std::free(new_dest);
                new_dest = nullptr;

                if (this->overwrite_mode == vfs::file_task_overwrite_mode::overwrite ||
                    this->overwrite_mode == vfs::file_task_overwrite_mode::overwrite_all)
                {
                    const auto checked_current_file = this->current_file.value_or("");
                    const auto checked_current_dest = this->current_dest.value_or("");

                    *dest_exists = dest_file_stat.is_valid();
                    if (std::filesystem::equivalent(checked_current_file, checked_current_dest))
                    {
                        // src and dest are same file - do not overwrite (truncates)
                        // occurs if user pauses task and changes overwrite mode
                        return false;
                    }
                    return true;
                }
                else
                {
                    return false;
                }
            }
            // user renamed file or pressed Pause btn
            if (new_dest)
            { // user renamed file - test if new name exists
                use_dest_file = new_dest;
            }
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
VFSFileTask::check_dest_in_src(const std::filesystem::path& src_dir)
{
    if (!this->dest_dir)
    {
        return false;
    }

    const auto checked_dest_dir = this->dest_dir.value();

    const auto real_dest_path = std::filesystem::canonical(checked_dest_dir);
    const auto real_src_path = std::filesystem::canonical(src_dir);

    // Need to have the + '/' to avoid erroring when moving a dir
    // into another dir when the target starts the whole name of the source.
    // i.e. moving './new' into './new2'
    if (!ztd::startswith(real_dest_path.string() + '/', real_src_path.string() + '/'))
    {
        return false;
    }

    // source is contained in destination dir
    const std::string err =
        std::format("Destination directory \"{}\" is contained in source \"{}\"",
                    checked_dest_dir.string(),
                    src_dir.string());
    this->append_add_log(err);
    if (this->state_cb)
    {
        this->state_cb(this, vfs::file_task_state::error, nullptr, this->state_cb_data);
    }
    this->state = vfs::file_task_state::running;

    return true;
}

static void
update_file_display(const std::filesystem::path& path)
{
    // for devices like nfs, emit created and flush to avoid a
    // blocking stat call in GUI thread during writes
    const auto dir_path = path.parent_path();
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
VFSFileTask::file_copy(const std::filesystem::path& src_file)
{
    const auto file_name = src_file.filename();
    const auto dest_file = this->dest_dir.value() / file_name;

    this->do_file_copy(src_file, dest_file);
}

bool
VFSFileTask::do_file_copy(const std::filesystem::path& src_file,
                          const std::filesystem::path& dest_file)
{
    if (this->should_abort())
    {
        return false;
    }

    // ztd::logger::info("vfs_file_task_do_copy( {}, {} )", src_file, dest_file);
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

    std::filesystem::path actual_dest_file = dest_file;

    if (std::filesystem::is_directory(src_file))
    {
        if (this->check_dest_in_src(src_file))
        {
            if (new_dest_file)
            {
                std::free(new_dest_file);
            }
            return false;
        }
        if (!this->check_overwrite(actual_dest_file, &dest_exists, &new_dest_file))
        {
            if (new_dest_file)
            {
                std::free(new_dest_file);
            }
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
                const auto file_name = file.path().filename();
                if (this->should_abort())
                {
                    break;
                }
                const auto sub_src_file = src_file / file_name;
                const auto sub_dest_file = actual_dest_file / file_name;
                if (!this->do_file_copy(sub_src_file, sub_dest_file) && !copy_fail)
                {
                    copy_fail = true;
                }
            }

            chmod(actual_dest_file.c_str(), file_stat.mode());
            times.actime = file_stat.atime();
            times.modtime = file_stat.mtime();
            utime(actual_dest_file.c_str(), &times);

            if (this->avoid_changes)
            {
                update_file_display(actual_dest_file);
            }

            /* Move files to different device: Need to delete source dir */
            if ((this->type == vfs::file_task_type::move) && !this->should_abort() && !copy_fail)
            {
                std::filesystem::remove_all(src_file);
                if (std::filesystem::exists(src_file))
                {
                    this->task_error(errno, "Removing", src_file);
                    copy_fail = true;
                    if (this->should_abort())
                    {
                        if (new_dest_file)
                        {
                            std::free(new_dest_file);
                        }
                    }
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

        std::filesystem::path target_path;
        try
        {
            target_path = std::filesystem::read_symlink(src_file);
            read_symlink = true;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            ztd::logger::warn("{}", e.what());
        }

        if (read_symlink)
        {
            if (!this->check_overwrite(actual_dest_file, &dest_exists, &new_dest_file))
            {
                if (new_dest_file)
                {
                    std::free(new_dest_file);
                }
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
                    {
                        std::free(new_dest_file);
                    }
                    return false;
                }
            }

            std::filesystem::create_symlink(target_path, actual_dest_file);
            if (std::filesystem::is_symlink(actual_dest_file))
            {
                /* Move files to different device: Need to delete source files */
                if ((this->type == vfs::file_task_type::move) && !copy_fail)
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
        const i32 rfd = open(src_file.c_str(), O_RDONLY);
        if (rfd >= 0)
        {
            if (!this->check_overwrite(actual_dest_file, &dest_exists, &new_dest_file))
            {
                close(rfd);
                if (new_dest_file)
                {
                    std::free(new_dest_file);
                }
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
                    {
                        std::free(new_dest_file);
                    }
                    return false;
                }
            }

            const i32 wfd = creat(actual_dest_file.c_str(), file_stat.mode() | S_IWUSR);
            if (wfd >= 0)
            {
                // sshfs becomes unresponsive with this, nfs is okay with it
                // if (this->avoid_changes)
                //    emit_created(actual_dest_file);
                char buffer[4096];
                isize rsize;
                while ((rsize = read(rfd, buffer, sizeof(buffer))) > 0)
                {
                    if (this->should_abort())
                    {
                        copy_fail = true;
                        break;
                    }

                    const auto length = write(wfd, buffer, rsize);
                    if (length > 0)
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
                        chmod(actual_dest_file.c_str(), file_stat.mode());
                        struct utimbuf times;
                        times.actime = file_stat.atime();
                        times.modtime = file_stat.mtime();
                        utime(actual_dest_file.c_str(), &times);
                    }
                    if (this->avoid_changes)
                    {
                        update_file_display(actual_dest_file);
                    }

                    /* Move files to different device: Need to delete source files */
                    if ((this->type == vfs::file_task_type::move) && !this->should_abort())
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
    {
        std::free(new_dest_file);
    }
    if (!copy_fail && this->error_first)
    {
        this->error_first = false;
    }
    return !copy_fail;
}

void
VFSFileTask::file_move(const std::filesystem::path& src_file)
{
    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
    this->unlock();

    const auto file_name = src_file.filename();
    const auto dest_file = this->dest_dir.value() / file_name;

    const auto src_stat = ztd::lstat(src_file);
    const auto dest_stat = ztd::stat(this->dest_dir.value());

    if (src_stat.is_valid() && dest_stat.is_valid())
    {
        /* Not on the same device */
        if (src_stat.dev() != dest_stat.dev())
        {
            // ztd::logger::info("not on the same dev: {}", src_file);
            this->do_file_copy(src_file, dest_file);
        }
        else
        {
            // ztd::logger::info("on the same dev: {}", src_file);
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
VFSFileTask::do_file_move(const std::filesystem::path& src_file,
                          const std::filesystem::path& dest_path)
{
    if (this->should_abort())
    {
        return 0;
    }

    std::filesystem::path dest_file = dest_path;

    this->lock();
    this->current_file = src_file;
    this->current_dest = dest_file;
    this->current_item++;
    this->unlock();

    // ztd::logger::debug("move '{}' to '{}'", src_file, dest_file);
    const auto file_stat = ztd::lstat(src_file);
    if (!file_stat.is_valid())
    {
        this->task_error(errno, "Accessing", src_file);
        return 0;
    }

    if (this->should_abort())
    {
        return 0;
    }

    if (file_stat.is_directory() && this->check_dest_in_src(src_file))
    {
        return 0;
    }

    char* new_dest_file = nullptr;
    bool dest_exists;
    if (!this->check_overwrite(dest_file, &dest_exists, &new_dest_file))
    {
        return 0;
    }

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
            const auto file_name = file.path().filename();
            if (this->should_abort())
            {
                break;
            }
            const auto sub_src_file = src_file / file_name;
            const auto sub_dest_file = dest_file / file_name;
            this->do_file_move(sub_src_file, sub_dest_file);
        }
        // remove moved src dir if empty
        if (!this->should_abort())
        {
            std::filesystem::remove_all(src_file);
        }

        return 0;
    }

    std::error_code err;
    std::filesystem::rename(src_file, dest_file, err);

    if (err.value() != 0)
    {
        if (err.value() == -1 && errno == EXDEV)
        { // Invalid cross-link device
            return 18;
        }

        this->task_error(errno, "Renaming", src_file);
        if (this->should_abort())
        {
            std::free(new_dest_file);
            return 0;
        }
    }
    else if (!std::filesystem::is_symlink(dest_file))
    { // do not chmod link
        chmod(dest_file.c_str(), file_stat.mode());
    }

    this->lock();
    this->progress += file_stat.size();
    if (this->error_first)
    {
        this->error_first = false;
    }
    this->unlock();

    if (new_dest_file)
    {
        std::free(new_dest_file);
    }
    return 0;
}

void
VFSFileTask::file_trash(const std::filesystem::path& src_file)
{
    if (this->should_abort())
    {
        return;
    }

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
        ztd::logger::error("Trashing failed missing RW permissions '{}'", src_file.string());
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
    {
        this->error_first = false;
    }
    this->unlock();
}

void
VFSFileTask::file_delete(const std::filesystem::path& src_file)
{
    if (this->should_abort())
    {
        return;
    }

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

    if (file_stat.is_directory())
    {
        for (const auto& file : std::filesystem::directory_iterator(src_file))
        {
            const auto file_name = file.path().filename();
            if (this->should_abort())
            {
                break;
            }
            const auto sub_src_file = src_file / file_name;
            this->file_delete(sub_src_file);
        }

        if (this->should_abort())
        {
            return;
        }
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
    {
        this->error_first = false;
    }
    this->unlock();
}

void
VFSFileTask::file_link(const std::filesystem::path& src_file)
{
    if (this->should_abort())
    {
        return;
    }

    const auto file_name = src_file.filename();
    const auto old_dest_file = this->dest_dir.value() / file_name;
    auto dest_file = old_dest_file;

    // MOD  setup task for check overwrite
    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
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
            {
                return;
            }
        }
    }

    /* FIXME: Check overwrite!! */ // MOD added check overwrite
    bool dest_exists;
    char* new_dest_file = nullptr;
    if (!this->check_overwrite(dest_file, &dest_exists, &new_dest_file))
    {
        return;
    }

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
        {
            return;
        }
    }

    this->lock();
    this->progress += src_stat.size();
    if (this->error_first)
    {
        this->error_first = false;
    }
    this->unlock();

    if (new_dest_file)
    {
        std::free(new_dest_file);
    }
}

void
VFSFileTask::file_chown_chmod(const std::filesystem::path& src_file)
{
    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
    this->current_item++;
    this->unlock();
    // ztd::logger::debug("chmod_chown: {}", src_file);

    const auto src_stat = ztd::lstat(src_file);
    if (src_stat.is_valid())
    {
        /* chown */
        if (!this->uid || !this->gid)
        {
            const i32 result = chown(src_file.c_str(), this->uid, this->gid);
            if (result != 0)
            {
                this->task_error(errno, "chown", src_file);
                if (this->should_abort())
                {
                    return;
                }
            }
        }

        /* chmod */
        if (this->chmod_actions)
        {
            std::filesystem::perms new_perms;
            const std::filesystem::perms orig_perms =
                std::filesystem::status(src_file).permissions();

            for (const auto i : ztd::range(magic_enum::enum_count<vfs::chmod_action>()))
            {
                if (this->chmod_actions[i] == 2)
                { /* Do not change */
                    continue;
                }
                if (this->chmod_actions[i] == 0)
                { /* Remove this bit */
                    new_perms &= ~chmod_flags.at(i);
                }
                else
                { /* Add this bit */
                    new_perms |= chmod_flags.at(i);
                }
            }
            if (new_perms != orig_perms)
            {
                std::error_code ec;
                std::filesystem::permissions(src_file, new_perms, ec);
                if (ec)
                {
                    this->task_error(errno, "chmod", src_file);
                    if (this->should_abort())
                    {
                        return;
                    }
                }
            }
        }

        this->lock();
        this->progress += src_stat.size();
        this->unlock();

        if (this->avoid_changes)
        {
            update_file_display(src_file);
        }

        if (src_stat.is_directory() && this->is_recursive)
        {
            for (const auto& file : std::filesystem::directory_iterator(src_file))
            {
                const auto file_name = file.path().filename();
                if (this->should_abort())
                {
                    break;
                }
                const auto sub_src_file = src_file / file_name;
                this->file_chown_chmod(sub_src_file);
            }
        }
    }
    if (this->error_first)
    {
        this->error_first = false;
    }
}

static void
call_state_callback(vfs::file_task task, vfs::file_task_state state)
{
    task->state = state;

    if (!task->state_cb)
    {
        return;
    }

    if (!task->state_cb(task, state, nullptr, task->state_cb_data))
    {
        task->abort = true;
        if (task->type == vfs::file_task_type::exec && task->exec_cond)
        {
            // this is used only if exec task run in non-main loop thread
            task->lock();
            g_cond_broadcast(task->exec_cond);
            task->unlock();
        }
    }
    else
    {
        task->state = vfs::file_task_state::running;
    }
}

static void
cb_exec_child_cleanup(pid_t pid, i32 status, char* tmp_file)
{ // delete tmp files after async task terminates
    // ztd::logger::info("cb_exec_child_cleanup pid={} status={} file={}", pid, status, tmp_file );
    g_spawn_close_pid(pid);
    if (tmp_file)
    {
        std::filesystem::remove(tmp_file);
        std::free(tmp_file);
    }
    ztd::logger::info("async child finished  pid={} status={}", pid, status);
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
    {
        task->exec_exit_status = 0;
    }

    if (!task->exec_keep_tmp && task->exec_script)
    {
        const auto checked_exec_script = task->exec_script.value();
        if (std::filesystem::exists(checked_exec_script))
        {
            std::filesystem::remove(checked_exec_script);
        }
    }

    ztd::logger::info("child finished  pid={} exit_status={}",
                      pid,
                      bad_status ? -1 : task->exec_exit_status);

    if (!task->exec_exit_status && !bad_status)
    {
        if (!task->custom_percent)
        {
            task->percent = 100;
        }
    }
    else
    {
        call_state_callback(task, vfs::file_task_state::error);
    }

    if (bad_status || (!task->exec_channel_out && !task->exec_channel_err))
    {
        call_state_callback(task, vfs::file_task_state::finish);
    }
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
                call_state_callback(task, vfs::file_task_state::finish);
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
            call_state_callback(task, vfs::file_task_state::finish);
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
        ztd::logger::info("cb_exec_out_watch: g_io_channel_read_chars != G_IO_STATUS_NORMAL");
    }

    return true;
}

void
VFSFileTask::file_exec(const std::filesystem::path& src_file)
{
    // this function is now thread safe but is not currently run in
    // another thread because gio adds watches to main loop thread anyway

    // ztd::logger::info("vfs_file_task_exec");
    // this->exec_keep_tmp = true;

    this->lock();

    GtkWidget* parent = nullptr;
    if (this->exec_browser)
    {
        parent = gtk_widget_get_toplevel(GTK_WIDGET(this->exec_browser));
    }
    else if (this->exec_desktop)
    {
        parent = gtk_widget_get_toplevel(GTK_WIDGET(this->exec_desktop));
    }

    this->state = vfs::file_task_state::running;
    this->current_file = src_file;
    this->total_size = 0;
    this->percent = 0;
    this->unlock();

    if (this->should_abort())
    {
        return;
    }

    // need su?
    std::string su;
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
                ztd::logger::warn(msg);
                // do not use xset_msg_dialog if non-main thread
                // this->task_error(0, str);
                xset_msg_dialog(parent,
                                GtkMessageType::GTK_MESSAGE_ERROR,
                                "Terminal SU Not Available",
                                GtkButtonsType::GTK_BUTTONS_OK,
                                msg);
                call_state_callback(this, vfs::file_task_state::finish);
                // ztd::logger::info("vfs_file_task_exec DONE ERROR");
                return;
            }
        }
    }

    // make tmpdir
    const auto tmp = vfs::user_dirs->program_tmp_dir();
    if (!std::filesystem::is_directory(tmp))
    {
        const std::string msg = "Cannot create temporary directory";
        ztd::logger::warn(msg);
        // do not use xset_msg_dialog if non-main thread
        // this->task_error(0, str);
        xset_msg_dialog(parent,
                        GtkMessageType::GTK_MESSAGE_ERROR,
                        "Error",
                        GtkButtonsType::GTK_BUTTONS_OK,
                        msg);
        call_state_callback(this, vfs::file_task_state::finish);
        // ztd::logger::info("vfs_file_task_exec DONE ERROR");
        return;
    }

    // get terminal
    if (!this->exec_terminal && !this->exec_as_user.empty())
    { // using cli tool so run in terminal
        this->exec_terminal = true;
    }

    std::string terminal;
    if (this->exec_terminal)
    {
        // get terminal
        const auto terminal_s = xset_get_s(xset::name::main_terminal);
        if (terminal_s)
        {
            terminal = Glib::find_program_in_path(terminal_s.value());
        }

        if (terminal.empty())
        {
            const std::string msg =
                "Please set a valid terminal program in View|Preferences|Advanced";
            ztd::logger::warn(msg);
            // do not use xset_msg_dialog if non-main thread
            // this->task_error(0, str);
            xset_msg_dialog(parent,
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Terminal Not Available",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            msg);

            call_state_callback(this, vfs::file_task_state::finish);
            // ztd::logger::info("vfs_file_task_exec DONE ERROR");
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
            const std::filesystem::path hexname = std::format("{}.fish", ztd::randhex());
            const auto new_exec_script = tmp / hexname;
            if (!std::filesystem::exists(new_exec_script))
            {
                this->exec_script = new_exec_script;
                break;
            }
        }

        const auto checked_exec_script = this->exec_script.value();

        // open buffer
        std::string buf;

        // build - header
        buf.append(std::format("#!{}\nsource {}\n\n", FISH_PATH, FISH_FMLIB));

        // build - exports
        if (this->exec_export && (this->exec_browser || this->exec_desktop))
        {
            if (!this->exec_browser)
            {
                this->task_error(errno, "Error writing temporary file");

                if (!this->exec_keep_tmp && this->exec_script)
                {
                    if (std::filesystem::exists(checked_exec_script))
                    {
                        std::filesystem::remove(checked_exec_script);
                    }
                }
                call_state_callback(this, vfs::file_task_state::finish);
                // ztd::logger::info("vfs_file_task_exec DONE ERROR");
                return;
            }

            if (this->current_dest)
            {
                const auto checked_current_dest = this->current_dest.value();
                buf.append(main_write_exports(this, checked_current_dest.string()));
            }
        }
        else
        {
            if (this->exec_export && !this->exec_browser && !this->exec_desktop)
            {
                this->exec_export = false;
                ztd::logger::warn("exec_export set without exec_browser/exec_desktop");
            }
        }

        // build - export vars
        if (this->exec_export)
        {
            buf.append(
                std::format("set fm_import {}\n", ztd::shell::quote(checked_exec_script.string())));
        }
        else
        {
            buf.append(std::format("set fm_import\n"));
        }

        buf.append(
            std::format("set fm_source {}\n\n", ztd::shell::quote(checked_exec_script.string())));

        // build - trap rm
        if (!this->exec_keep_tmp && geteuid() != 0 && ztd::same(this->exec_as_user, "root"))
        {
            // run as root command, clean up
            buf.append(std::format("trap \"rm -f {}; exit\" EXIT SIGINT SIGTERM SIGQUIT SIGHUP\n\n",
                                   checked_exec_script.string()));
        }

        // build - command
        ztd::logger::info("TASK_COMMAND({:p})={}", fmt::ptr(this->exec_ptask), this->exec_command);

        buf.append(std::format("{}\n\n", this->exec_command));
        buf.append(std::format("set fm_err $status\n\n"));

        // build - press enter to close
        if (!terminal.empty() && this->exec_keep_terminal)
        {
            if (geteuid() == 0 || ztd::same(this->exec_as_user, "root"))
            {
                buf.append("fm_enter_to_close\n\n");
            }
            else
            {
                buf.append("fm_enter_for_shell\n\n");
            }
        }

        buf.append(std::format("exit $fm_err\n"));
        // ztd::logger::debug(buf);

        const bool result = write_file(this->exec_script.value(), buf);
        if (!result)
        {
            this->task_error(errno, "Error writing temporary file");

            if (!this->exec_keep_tmp)
            {
                if (std::filesystem::exists(checked_exec_script))
                {
                    std::filesystem::remove(checked_exec_script);
                }
            }
            call_state_callback(this, vfs::file_task_state::finish);
            // ztd::logger::info("vfs_file_task_exec DONE ERROR");
            return;
        }

        // set permissions
        chmod(checked_exec_script.c_str(), 0700);

        // use checksum
        if (geteuid() != 0 && (!this->exec_as_user.empty() || this->exec_checksum))
        {
            sum_script =
                ztd::compute_checksum(ztd::checksum::type::md5, checked_exec_script.string());
        }
    }

    this->percent = 50;

    // Spawn
    std::vector<std::string> argv;

    std::string use_su;
    bool single_arg = false;

    if (!terminal.empty())
    {
        // terminal
        const auto terminal_s = xset_get_s(xset::name::main_terminal);
        if (terminal_s)
        {
            const auto terminal_args = terminal_handlers->get_terminal_args(terminal_s.value());
            argv.reserve(terminal_args.size());
            for (const std::string_view terminal_arg : terminal_args)
            {
                argv.emplace_back(terminal_arg);
            }
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
            {
                argv.emplace_back("-u");
            }
            argv.emplace_back(this->exec_as_user);
        }

        if (ztd::same(use_su, "/bin/su"))
        {
            // /bin/su
            argv.emplace_back("-s");
            argv.emplace_back(FISH_PATH);
            argv.emplace_back("-c");
            single_arg = true;
        }
    }

    std::string auth;
    if (!sum_script.empty())
    {
        // spacefm-auth exists?
        auth = get_script_path(spacefm::script::spacefm_auth);
        if (!script_exists(auth))
        {
            sum_script.clear();
        }
    }

    if (!sum_script.empty() && !auth.empty())
    {
        // spacefm-auth
        if (single_arg)
        {
            const std::string script =
                std::format("{} {} {} {} {}",
                            FISH_PATH,
                            auth,
                            ztd::same(this->exec_as_user, "root") ? "root" : "",
                            this->exec_script.value().string(),
                            sum_script);
            argv.emplace_back(script);
        }
        else
        {
            argv.emplace_back(FISH_PATH);
            argv.emplace_back(auth);
            if (ztd::same(this->exec_as_user, "root"))
            {
                argv.emplace_back("root");
            }
            argv.emplace_back(this->exec_script.value());
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
        argv.emplace_back(this->exec_script.value());
    }

    pid_t pid;
    i32 out, err;
    try
    {
        std::filesystem::path working_directory;
        if (this->dest_dir)
        {
            working_directory = this->dest_dir.value();
        }
        else
        {
            working_directory = vfs::user_dirs->home_dir();
        }

        Glib::spawn_async_with_pipes(working_directory,
                                     argv,
                                     Glib::SpawnFlags::DO_NOT_REAP_CHILD,
                                     Glib::SlotSpawnChildSetup(),
                                     &pid,
                                     nullptr,
                                     this->exec_sync ? &out : nullptr,
                                     this->exec_sync ? &err : nullptr);
    }
    catch (const Glib::SpawnError& e)
    {
        ztd::logger::error("    glib_error_code={}", e.code());

        if (errno)
        {
            const std::string errno_msg = std::strerror(errno);
            ztd::logger::info("    result={} ( {} )", errno, errno_msg);
        }

        if (!this->exec_keep_tmp && this->exec_sync && this->exec_script)
        {
            const auto checked_exec_script = this->exec_script.value();
            if (std::filesystem::exists(checked_exec_script))
            {
                std::filesystem::remove(checked_exec_script);
            }
        }
        const std::string cmd = ztd::join(argv, " ");
        const std::string msg =
            std::format("Error executing '{}'\nGlib Spawn Error Code {}, {}\nRun in a terminal "
                        "for full debug info\n",
                        cmd,
                        magic_enum::enum_integer(e.code()),
                        Glib::strerror(e.code()).c_str());
        this->task_error(errno, msg);
        call_state_callback(this, vfs::file_task_state::finish);
        // ztd::logger::info("vfs_file_task_exec DONE ERROR");
        return;
    }

    ztd::logger::info("SPAWN=\"{}\" pid={}", ztd::join(argv, " "), pid);

    if (!this->exec_sync)
    {
        // catch termination to waitpid and delete tmp if needed
        // task can be destroyed while this watch is still active
        if (!this->exec_keep_tmp && !this->exec_direct && this->exec_script)
        {
            const auto checked_exec_script = this->exec_script.value();
            g_child_watch_add(pid,
                              (GChildWatchFunc)cb_exec_child_cleanup,
                              ztd::strdup(checked_exec_script));
        }
        else
        {
            g_child_watch_add(pid, (GChildWatchFunc)cb_exec_child_cleanup, nullptr);
        }
        call_state_callback(this, vfs::file_task_state::finish);
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
    this->state = vfs::file_task_state::running;

    // ztd::logger::info("vfs_file_task_exec DONE");
    return; // exit thread
}

static bool
on_size_timeout(vfs::file_task task)
{
    if (!task->abort)
    {
        task->state = vfs::file_task_state::size_timeout;
    }
    return false;
}

static void*
vfs_file_task_thread(vfs::file_task task)
{
    u32 size_timeout = 0;
    if (task->type < vfs::file_task_type::move || task->type >= vfs::file_task_type::last)
    {
        task->state = vfs::file_task_state::running;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task);
        }
        if (task->state_cb)
        {
            call_state_callback(task, vfs::file_task_state::finish);
        }
        return nullptr;
    }

    task->lock();
    task->state = vfs::file_task_state::running;
    task->current_file = task->src_paths.at(0);
    task->total_size = 0;
    task->unlock();

    if (task->abort)
    {
        task->state = vfs::file_task_state::running;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task);
        }

        if (task->state_cb)
        {
            call_state_callback(task, vfs::file_task_state::finish);
        }
        return nullptr;
    }

    /* Calculate total size of all files */
    if (task->is_recursive)
    {
        // start timer to limit the amount of time to spend on this - can be
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds(5, (GSourceFunc)on_size_timeout, task);
        for (const auto& src_path : task->src_paths)
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
                task->state = vfs::file_task_state::running;
                if (size_timeout)
                {
                    g_source_remove_by_user_data(task);
                }
                if (task->state_cb)
                {
                    call_state_callback(task, vfs::file_task_state::finish);
                }
                return nullptr;
            }
            if (task->state == vfs::file_task_state::size_timeout)
            {
                break;
            }
        }
    }
    else if (task->type == vfs::file_task_type::trash)
    {
    }
    else if (task->type != vfs::file_task_type::exec)
    {
        dev_t dest_dev = 0;

        // start timer to limit the amount of time to spend on this - can be
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds(5, (GSourceFunc)on_size_timeout, task);
        if (task->type != vfs::file_task_type::chmod_chown)
        {
            const auto file_stat = ztd::lstat(task->dest_dir.value());
            if (!(task->dest_dir && file_stat.is_valid()))
            {
                task->task_error(errno, "Accessing", task->dest_dir.value());
                task->abort = true;
                task->state = vfs::file_task_state::running;
                if (size_timeout)
                {
                    g_source_remove_by_user_data(task);
                }
                if (task->state_cb)
                {
                    call_state_callback(task, vfs::file_task_state::finish);
                }
                return nullptr;
            }
            dest_dev = file_stat.dev();
        }

        for (const auto& src_path : task->src_paths)
        {
            const auto file_stat = ztd::lstat(src_path);
            if (!file_stat.is_valid())
            {
                // do not report error here since it is reported later
                // task->error(errno, "Accessing", (char*)l->data);
            }
            else
            {
                if ((task->type == vfs::file_task_type::move) && file_stat.dev() != dest_dev)
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
                task->state = vfs::file_task_state::running;
                if (size_timeout)
                {
                    g_source_remove_by_user_data(task);
                }

                if (task->state_cb)
                {
                    call_state_callback(task, vfs::file_task_state::finish);
                }
                return nullptr;
            }
            if (task->state == vfs::file_task_state::size_timeout)
            {
                break;
            }
        }
    }

    if (task->abort)
    {
        task->state = vfs::file_task_state::running;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task);
        }
        if (task->state_cb)
        {
            call_state_callback(task, vfs::file_task_state::finish);
        }
        return nullptr;
    }

    // cancel the timer
    if (size_timeout)
    {
        g_source_remove_by_user_data(task);
    }

    if (task->state_pause == vfs::file_task_state::queue)
    {
        if (task->state != vfs::file_task_state::size_timeout &&
            xset_get_b(xset::name::task_q_smart))
        {
            // make queue exception for smaller tasks
            off_t exlimit;
            switch (task->type)
            {
                case vfs::file_task_type::move:
                case vfs::file_task_type::copy:
                case vfs::file_task_type::trash:
                    exlimit = 10485760; // 10M
                    break;
                case vfs::file_task_type::DELETE:
                    exlimit = 5368709120; // 5G
                    break;
                case vfs::file_task_type::link:
                case vfs::file_task_type::chmod_chown:
                case vfs::file_task_type::exec:
                case vfs::file_task_type::last:
                    exlimit = 0; // always exception for other types
                    break;
            }

            if (!exlimit || task->total_size < exlimit)
            {
                task->state_pause = vfs::file_task_state::running;
            }
        }
        // device list is populated so signal queue start
        task->queue_start = true;
    }

    if (task->state == vfs::file_task_state::size_timeout)
    {
        task->append_add_log("Timed out calculating total size\n");
        task->total_size = 0;
    }
    task->state = vfs::file_task_state::running;
    if (task->should_abort())
    {
        task->state = vfs::file_task_state::running;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task);
        }
        if (task->state_cb)
        {
            call_state_callback(task, vfs::file_task_state::finish);
        }
        return nullptr;
    }

    for (const auto& src_path : task->src_paths)
    {
        switch (task->type)
        {
            case vfs::file_task_type::move:
                task->file_move(src_path);
                break;
            case vfs::file_task_type::copy:
                task->file_copy(src_path);
                break;
            case vfs::file_task_type::trash:
                task->file_trash(src_path);
                break;
            case vfs::file_task_type::DELETE:
                task->file_delete(src_path);
                break;
            case vfs::file_task_type::link:
                task->file_link(src_path);
                break;
            case vfs::file_task_type::chmod_chown:
                task->file_chown_chmod(src_path);
                break;
            case vfs::file_task_type::exec:
                task->file_exec(src_path);
                break;
            case vfs::file_task_type::last:
                break;
        }
    }

    task->state = vfs::file_task_state::running;
    if (size_timeout)
    {
        g_source_remove_by_user_data(task);
    }
    if (task->state_cb)
    {
        call_state_callback(task, vfs::file_task_state::finish);
    }
    return nullptr;
}

void
VFSFileTask::run_task()
{
    if (this->type != vfs::file_task_type::exec)
    {
        if (this->type == vfs::file_task_type::chmod_chown && !this->src_paths.empty())
        {
            const auto dir = this->src_paths.at(0).parent_path();
            this->avoid_changes = vfs_volume_dir_avoid_changes(dir);
        }
        else
        {
            if (this->dest_dir)
            {
                const auto checked_dest_dir = this->dest_dir.value();
                this->avoid_changes = vfs_volume_dir_avoid_changes(checked_dest_dir);
            }
            else
            {
                this->avoid_changes = false;
            }
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
    this->state_pause = vfs::file_task_state::running;
}

void
VFSFileTask::abort_task()
{
    this->abort = true;
    /* Called from another thread */
    if (this->thread && g_thread_self() != this->thread && this->type != vfs::file_task_type::exec)
    {
        g_thread_join(this->thread);
        this->thread = nullptr;
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
VFSFileTask::get_total_size_of_dir(const std::filesystem::path& path)
{
    if (this->abort)
    {
        return 0;
    }

    const auto file_stat = ztd::lstat(path);
    if (!file_stat.is_valid())
    {
        return 0;
    }

    off_t size = file_stat.size();

    // Do not follow symlinks
    if (file_stat.is_symlink() || !file_stat.is_directory())
    {
        return size;
    }

    for (const auto& file : std::filesystem::directory_iterator(path))
    {
        const auto file_name = file.path().filename();
        if (this->state == vfs::file_task_state::size_timeout || this->abort)
        {
            break;
        }

        const auto full_path = path / file_name;
        if (std::filesystem::exists(full_path))
        {
            const auto dir_file_stat = ztd::lstat(full_path);

            if (dir_file_stat.is_directory())
            {
                size += this->get_total_size_of_dir(full_path);
            }
            else
            {
                size += dir_file_stat.size();
            }
        }
    }

    return size;
}

void
VFSFileTask::task_error(i32 errnox, const std::string_view action)
{
    if (errnox)
    {
        const std::string errno_msg = std::strerror(errnox);
        const std::string msg = std::format("{}\n{}\n", action, errno_msg);
        this->append_add_log(msg);
    }
    else
    {
        const std::string msg = std::format("{}\n", action);
        this->append_add_log(msg);
    }

    call_state_callback(this, vfs::file_task_state::error);
}

void
VFSFileTask::task_error(i32 errnox, const std::string_view action,
                        const std::filesystem::path& target)
{
    this->error = errnox;
    const std::string errno_msg = std::strerror(errnox);
    const std::string msg = std::format("\n{} {}\nError: {}\n", action, target.string(), errno_msg);
    this->append_add_log(msg);
    call_state_callback(this, vfs::file_task_state::error);
}
