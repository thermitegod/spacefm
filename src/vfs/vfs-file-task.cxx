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

#include <memory>

#include <ranges>

#include <fcntl.h>
#include <utime.h>

#include <malloc.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-dialog.hxx"

#include "xset/xset.hxx"

#include "terminal-handlers.hxx"

#include "main-window.hxx"
#include "vfs/vfs-volume.hxx"
#include "vfs/vfs-utils.hxx"

#include "write.hxx"
#include "utils.hxx"

#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-trash-can.hxx"
#include "vfs/vfs-file-task.hxx"

inline constexpr std::array<std::filesystem::perms, 12> chmod_flags{
    // User
    std::filesystem::perms::owner_read,
    std::filesystem::perms::owner_write,
    std::filesystem::perms::owner_exec,
    // Group
    std::filesystem::perms::group_read,
    std::filesystem::perms::group_write,
    std::filesystem::perms::group_exec,

    // Other
    std::filesystem::perms::others_read,
    std::filesystem::perms::others_write,
    std::filesystem::perms::others_exec,

    // uid/gid
    std::filesystem::perms::set_uid,
    std::filesystem::perms::set_gid,

    // sticky bit
    std::filesystem::perms::sticky_bit,
};

const std::shared_ptr<vfs::file_task>
vfs::file_task::create(const vfs::file_task::type task_type,
                       const std::span<const std::filesystem::path> src_files,
                       const std::filesystem::path& dest_dir) noexcept
{
    return std::make_shared<vfs::file_task>(task_type, src_files, dest_dir);
}

vfs::file_task::file_task(const vfs::file_task::type task_type,
                          const std::span<const std::filesystem::path> src_files,
                          const std::filesystem::path& dest_dir)
{
    this->type_ = task_type;
    this->src_paths = std::vector<std::filesystem::path>(src_files.begin(), src_files.end());
    if (!dest_dir.empty())
    {
        this->dest_dir = dest_dir;
    }

    this->is_recursive =
        (this->type_ == vfs::file_task::type::copy || this->type_ == vfs::file_task::type::del);

    // Init GMutex
    this->mutex = (GMutex*)malloc(sizeof(GMutex));
    g_mutex_init(this->mutex);

    GtkTextIter iter;
    this->add_log_buf = gtk_text_buffer_new(nullptr);
    this->add_log_end = gtk_text_mark_new(nullptr, false);
    gtk_text_buffer_get_end_iter(this->add_log_buf, &iter);
    gtk_text_buffer_add_mark(this->add_log_buf, this->add_log_end, &iter);

    this->start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    this->timer = ztd::timer();
}

vfs::file_task::~file_task()
{
    g_mutex_clear(this->mutex);
    std::free(this->mutex);

    gtk_text_buffer_set_text(this->add_log_buf, "", -1);
    g_object_unref(this->add_log_buf);
}

void
vfs::file_task::lock()
{
    g_mutex_lock(this->mutex);
}

void
vfs::file_task::unlock()
{
    g_mutex_unlock(this->mutex);
}

void
vfs::file_task::set_state_callback(const state_callback_t& cb, void* user_data)
{
    this->state_cb = cb;
    this->state_cb_data = user_data;
}

void
vfs::file_task::set_chmod(std::array<u8, 12> new_chmod_actions)
{
    this->chmod_actions = new_chmod_actions;
}

void
vfs::file_task::set_chown(uid_t new_uid, gid_t new_gid)
{
    this->uid = new_uid;
    this->gid = new_gid;
}

void
vfs::file_task::set_recursive(bool recursive)
{
    this->is_recursive = recursive;
}

void
vfs::file_task::set_overwrite_mode(const vfs::file_task::overwrite_mode mode)
{
    this->overwrite_mode_ = mode;
}

void
vfs::file_task::append_add_log(const std::string_view msg)
{
    this->lock();
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(this->add_log_buf, &iter, this->add_log_end);
    gtk_text_buffer_insert(this->add_log_buf, &iter, msg.data(), msg.length());
    this->unlock();
}

bool
vfs::file_task::should_abort()
{
    if (this->state_pause_ != vfs::file_task::state::running)
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
        this->state_pause_ = vfs::file_task::state::running;
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
vfs::file_task::check_overwrite(const std::filesystem::path& dest_file, bool* dest_exists,
                                char** new_dest_file)
{
    const auto dest_file_stat = ztd::statx(dest_file, ztd::statx::symlink::no_follow);

    while (true)
    {
        *new_dest_file = nullptr;
        if (this->overwrite_mode_ == vfs::file_task::overwrite_mode::overwrite_all)
        {
            const auto checked_current_file = this->current_file.value_or("");
            const auto checked_current_dest = this->current_dest.value_or("");

            *dest_exists = !!dest_file_stat;
            if (std::filesystem::equivalent(checked_current_file, checked_current_dest))
            {
                // src and dest are same file - do not overwrite (truncates)
                // occurs if user pauses task and changes overwrite mode
                return false;
            }
            return true;
        }
        else if (this->overwrite_mode_ == vfs::file_task::overwrite_mode::skip_all)
        {
            *dest_exists = !!dest_file_stat;
            return !*dest_exists;
        }
        else if (this->overwrite_mode_ == vfs::file_task::overwrite_mode::auto_rename)
        {
            *dest_exists = !!dest_file_stat;
            if (!*dest_exists)
            {
                return !this->abort;
            }

            // auto-rename
            const auto old_name = dest_file.filename();
            const auto dest_file_dir = dest_file.parent_path();

            const auto filename_parts = split_basename_extension(old_name);

            *new_dest_file = ztd::strdup(vfs_get_unique_name(dest_file_dir,
                                                             filename_parts.basename,
                                                             filename_parts.extension));
            *dest_exists = false;
            if (*new_dest_file)
            {
                return !this->abort;
            }
            // else ran out of names - fall through to query user
        }

        *dest_exists = !!dest_file_stat;
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
            this->state_ = vfs::file_task::state::query_overwrite;
            new_dest = nullptr;

            // query user
            if (!this->state_cb(this->shared_from_this(),
                                vfs::file_task::state::query_overwrite,
                                &new_dest,
                                this->state_cb_data))
            {
                // this->abort is actually set in query_overwrite_response
                // vfs::file_task::state::QUERY_OVERWRITE never returns false
                this->abort = true;
            }
            this->state_ = vfs::file_task::state::running;

            // may pause here - user may change overwrite mode
            if (this->should_abort())
            {
                std::free(new_dest);
                return false;
            }

            if (this->overwrite_mode_ != vfs::file_task::overwrite_mode::rename)
            {
                std::free(new_dest);
                new_dest = nullptr;

                if (this->overwrite_mode_ == vfs::file_task::overwrite_mode::overwrite ||
                    this->overwrite_mode_ == vfs::file_task::overwrite_mode::overwrite_all)
                {
                    const auto checked_current_file = this->current_file.value_or("");
                    const auto checked_current_dest = this->current_dest.value_or("");

                    *dest_exists = !!dest_file_stat;
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
vfs::file_task::check_dest_in_src(const std::filesystem::path& src_dir)
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
        this->state_cb(this->shared_from_this(),
                       vfs::file_task::state::error,
                       nullptr,
                       this->state_cb_data);
    }
    this->state_ = vfs::file_task::state::running;

    return true;
}

void
vfs::file_task::file_copy(const std::filesystem::path& src_file)
{
    const auto filename = src_file.filename();
    const auto dest_file = this->dest_dir.value() / filename;

    this->do_file_copy(src_file, dest_file);
}

bool
vfs::file_task::do_file_copy(const std::filesystem::path& src_file,
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

    const auto file_stat = ztd::statx(src_file, ztd::statx::symlink::no_follow);
    if (!file_stat)
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
                const auto filename = file.path().filename();
                if (this->should_abort())
                {
                    break;
                }
                const auto sub_src_file = src_file / filename;
                const auto sub_dest_file = actual_dest_file / filename;
                if (!this->do_file_copy(sub_src_file, sub_dest_file) && !copy_fail)
                {
                    copy_fail = true;
                }
            }

            chmod(actual_dest_file.c_str(), file_stat.mode());
            times.actime = file_stat.atime().tv_sec;
            times.modtime = file_stat.mtime().tv_sec;
            utime(actual_dest_file.c_str(), &times);

            /* Move files to different device: Need to delete source dir */
            if ((this->type_ == vfs::file_task::type::move) && !this->should_abort() && !copy_fail)
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
                if ((this->type_ == vfs::file_task::type::move) && !copy_fail)
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
                        times.actime = file_stat.atime().tv_sec;
                        times.modtime = file_stat.mtime().tv_sec;
                        utime(actual_dest_file.c_str(), &times);
                    }

                    /* Move files to different device: Need to delete source files */
                    if ((this->type_ == vfs::file_task::type::move) && !this->should_abort())
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
vfs::file_task::file_move(const std::filesystem::path& src_file)
{
    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
    this->unlock();

    const auto filename = src_file.filename();
    const auto dest_file = this->dest_dir.value() / filename;

    const auto src_stat = ztd::statx(src_file, ztd::statx::symlink::no_follow);
    const auto dest_stat = ztd::statx(this->dest_dir.value());

    if (src_stat && dest_stat)
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
vfs::file_task::do_file_move(const std::filesystem::path& src_file,
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
    const auto file_stat = ztd::statx(src_file, ztd::statx::symlink::no_follow);
    if (!file_stat)
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
            const auto filename = file.path().filename();
            if (this->should_abort())
            {
                break;
            }
            const auto sub_src_file = src_file / filename;
            const auto sub_dest_file = dest_file / filename;
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
vfs::file_task::file_trash(const std::filesystem::path& src_file)
{
    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
    this->current_item++;
    this->unlock();

    const auto file_stat = ztd::statx(src_file, ztd::statx::symlink::no_follow);
    if (!file_stat)
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

    const bool result = vfs::trash_can::trash(src_file);

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
vfs::file_task::file_delete(const std::filesystem::path& src_file)
{
    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
    this->current_item++;
    this->unlock();

    const auto file_stat = ztd::statx(src_file, ztd::statx::symlink::no_follow);
    if (!file_stat)
    {
        this->task_error(errno, "Accessing", src_file);
        return;
    }

    if (file_stat.is_directory())
    {
        for (const auto& file : std::filesystem::directory_iterator(src_file))
        {
            const auto filename = file.path().filename();
            if (this->should_abort())
            {
                break;
            }
            const auto sub_src_file = src_file / filename;
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
vfs::file_task::file_link(const std::filesystem::path& src_file)
{
    if (this->should_abort())
    {
        return;
    }

    const auto filename = src_file.filename();
    const auto old_dest_file = this->dest_dir.value() / filename;
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

    const auto src_stat = ztd::statx(src_file);
    if (!src_stat)
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
vfs::file_task::file_chown_chmod(const std::filesystem::path& src_file)
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

    const auto src_stat = ztd::statx(src_file, ztd::statx::symlink::no_follow);
    if (src_stat)
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

            const auto new_chmod_actions = chmod_actions.value();

            for (const auto i :
                 std::views::iota(0uz, magic_enum::enum_count<vfs::file_task::chmod_action>()))
            {
                if (new_chmod_actions[i] == 2)
                { /* Do not change */
                    continue;
                }
                if (new_chmod_actions[i] == 0)
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

        if (src_stat.is_directory() && this->is_recursive)
        {
            for (const auto& file : std::filesystem::directory_iterator(src_file))
            {
                const auto filename = file.path().filename();
                if (this->should_abort())
                {
                    break;
                }
                const auto sub_src_file = src_file / filename;
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
call_state_callback(const std::shared_ptr<vfs::file_task>& task, const vfs::file_task::state state)
{
    task->state_ = state;

    if (!task->state_cb)
    {
        return;
    }

    if (!task->state_cb(task, state, nullptr, task->state_cb_data))
    {
        task->abort = true;
        if (task->type_ == vfs::file_task::type::exec && task->exec_cond)
        {
            // this is used only if exec task run in non-main loop thread
            task->lock();
            g_cond_broadcast(task->exec_cond);
            task->unlock();
        }
    }
    else
    {
        task->state_ = vfs::file_task::state::running;
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
cb_exec_child_watch(pid_t pid, i32 status, const std::shared_ptr<vfs::file_task>& task)
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
        call_state_callback(task, vfs::file_task::state::error);
    }

    if (bad_status || (!task->exec_channel_out && !task->exec_channel_err))
    {
        call_state_callback(task, vfs::file_task::state::finish);
    }
}

static bool
cb_exec_out_watch(GIOChannel* channel, GIOCondition cond,
                  const std::shared_ptr<vfs::file_task>& task)
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
                call_state_callback(task, vfs::file_task::state::finish);
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
            call_state_callback(task, vfs::file_task::state::finish);
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
vfs::file_task::file_exec(const std::filesystem::path& src_file)
{
    // this function is now thread safe but is not currently run in
    // another thread because gio adds watches to main loop thread anyway

    // ztd::logger::info("vfs_file_task_exec");
    // this->exec_keep_tmp = true;

    this->lock();

    GtkWidget* parent = nullptr;
    if (this->exec_browser)
    {
#if (GTK_MAJOR_VERSION == 4)
        parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(this->exec_browser)));
#elif (GTK_MAJOR_VERSION == 3)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(this->exec_browser));
#endif
    }
    else if (this->exec_desktop)
    {
#if (GTK_MAJOR_VERSION == 4)
        parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(this->exec_desktop)));
#elif (GTK_MAJOR_VERSION == 3)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(this->exec_desktop));
#endif
    }

    this->state_ = vfs::file_task::state::running;
    this->current_file = src_file;
    this->total_size = 0;
    this->percent = 0;
    this->unlock();

    if (this->should_abort())
    {
        return;
    }

    // make tmpdir
    const auto tmp = vfs::user_dirs->program_tmp_dir();
    if (!std::filesystem::is_directory(tmp))
    {
        // do not use ptk_show_message() if non-main thread
        // this->task_error(0, str);
        ptk_show_message(GTK_WINDOW(parent),
                         GtkMessageType::GTK_MESSAGE_ERROR,
                         "Error",
                         GtkButtonsType::GTK_BUTTONS_OK,
                         "Cannot create temporary directory");
        call_state_callback(this->shared_from_this(), vfs::file_task::state::finish);
        // ztd::logger::info("vfs_file_task_exec DONE ERROR");
        return;
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
            // do not use ptk_show_message() if non-main thread
            // this->task_error(0, str);
            ptk_show_message(GTK_WINDOW(parent),
                             GtkMessageType::GTK_MESSAGE_ERROR,
                             "Terminal Not Available",
                             GtkButtonsType::GTK_BUTTONS_OK,
                             "Please set a valid terminal program in View|Preferences|Advanced");

            call_state_callback(this->shared_from_this(), vfs::file_task::state::finish);
            // ztd::logger::info("vfs_file_task_exec DONE ERROR");
            return;
        }
    }

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
                call_state_callback(this->shared_from_this(), vfs::file_task::state::finish);
                // ztd::logger::info("vfs_file_task_exec DONE ERROR");
                return;
            }

            if (this->current_dest)
            {
                const auto checked_current_dest = this->current_dest.value();
                buf.append(
                    main_write_exports(this->shared_from_this(), checked_current_dest.string()));
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

        // build - command
        ztd::logger::info("TASK_COMMAND({})={}", fmt::ptr(this->exec_ptask), this->exec_command);

        buf.append(std::format("{}\n\n", this->exec_command));
        buf.append(std::format("set fm_err $status\n\n"));

        // build - press enter to close
        if (!terminal.empty() && this->exec_keep_terminal)
        {
            buf.append("fm_enter_for_shell\n\n");
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
            call_state_callback(this->shared_from_this(), vfs::file_task::state::finish);
            // ztd::logger::info("vfs_file_task_exec DONE ERROR");
            return;
        }

        // set permissions
        chmod(checked_exec_script.c_str(), 0700);
    }

    this->percent = 50;

    // Spawn
    std::vector<std::string> argv;

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
    }

    if (this->exec_direct)
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
#if (GTK_MAJOR_VERSION == 4)
                                     Glib::SpawnFlags::DO_NOT_REAP_CHILD,
#elif (GTK_MAJOR_VERSION == 3)
                                     Glib::SpawnFlags::SPAWN_DO_NOT_REAP_CHILD,
#endif
                                     Glib::SlotSpawnChildSetup(),
                                     &pid,
                                     nullptr,
                                     this->exec_sync ? &out : nullptr,
                                     this->exec_sync ? &err : nullptr);
    }
    catch (const Glib::SpawnError& e)
    {
        ztd::logger::error("    glib_error_code={}", magic_enum::enum_integer(e.code()));

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
        call_state_callback(this->shared_from_this(), vfs::file_task::state::finish);
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
        call_state_callback(this->shared_from_this(), vfs::file_task::state::finish);
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
    this->state_ = vfs::file_task::state::running;

    // ztd::logger::info("vfs_file_task_exec DONE");
    return; // exit thread
}

static bool
on_size_timeout(const std::shared_ptr<vfs::file_task>& task)
{
    if (!task->abort)
    {
        task->state_ = vfs::file_task::state::size_timeout;
    }
    return false;
}

static void*
vfs_file_task_thread(const std::shared_ptr<vfs::file_task>& task)
{
    u32 size_timeout = 0;
    if (task->type_ < vfs::file_task::type::move || task->type_ >= vfs::file_task::type::last)
    {
        task->state_ = vfs::file_task::state::running;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task.get());
        }
        if (task->state_cb)
        {
            call_state_callback(task, vfs::file_task::state::finish);
        }
        return nullptr;
    }

    task->lock();
    task->state_ = vfs::file_task::state::running;
    task->current_file = task->src_paths.at(0);
    task->total_size = 0;
    task->unlock();

    if (task->abort)
    {
        task->state_ = vfs::file_task::state::running;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task.get());
        }

        if (task->state_cb)
        {
            call_state_callback(task, vfs::file_task::state::finish);
        }
        return nullptr;
    }

    /* Calculate total size of all files */
    if (task->is_recursive)
    {
        // start timer to limit the amount of time to spend on this - can be
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds(5, (GSourceFunc)on_size_timeout, task.get());
        for (const auto& src_path : task->src_paths)
        {
            const auto file_stat = ztd::statx(src_path, ztd::statx::symlink::no_follow);
            if (!file_stat)
            {
                // do not report error here since its reported later
                // this->task_error(errno, "Accessing"sv, (char*)l->data);
            }
            else
            {
                const u64 size = task->get_total_size_of_dir(src_path);
                task->lock();
                task->total_size += size;
                task->unlock();
            }
            if (task->abort)
            {
                task->state_ = vfs::file_task::state::running;
                if (size_timeout)
                {
                    g_source_remove_by_user_data(task.get());
                }
                if (task->state_cb)
                {
                    call_state_callback(task, vfs::file_task::state::finish);
                }
                return nullptr;
            }
            if (task->state_ == vfs::file_task::state::size_timeout)
            {
                break;
            }
        }
    }
    else if (task->type_ == vfs::file_task::type::trash)
    {
    }
    else if (task->type_ != vfs::file_task::type::exec)
    {
        dev_t dest_dev = 0;

        // start timer to limit the amount of time to spend on this - can be
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds(5, (GSourceFunc)on_size_timeout, task.get());
        if (task->type_ != vfs::file_task::type::chmod_chown)
        {
            const auto file_stat =
                ztd::statx(task->dest_dir.value(), ztd::statx::symlink::no_follow);
            if (!(task->dest_dir && file_stat))
            {
                task->task_error(errno, "Accessing", task->dest_dir.value());
                task->abort = true;
                task->state_ = vfs::file_task::state::running;
                if (size_timeout)
                {
                    g_source_remove_by_user_data(task.get());
                }
                if (task->state_cb)
                {
                    call_state_callback(task, vfs::file_task::state::finish);
                }
                return nullptr;
            }
            dest_dev = file_stat.dev();
        }

        for (const auto& src_path : task->src_paths)
        {
            const auto file_stat = ztd::statx(src_path, ztd::statx::symlink::no_follow);
            if (!file_stat)
            {
                // do not report error here since it is reported later
                // task->error(errno, "Accessing", (char*)l->data);
            }
            else
            {
                if ((task->type_ == vfs::file_task::type::move) && file_stat.dev() != dest_dev)
                {
                    // recursive size
                    const u64 size = task->get_total_size_of_dir(src_path);
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
                task->state_ = vfs::file_task::state::running;
                if (size_timeout)
                {
                    g_source_remove_by_user_data(task.get());
                }

                if (task->state_cb)
                {
                    call_state_callback(task, vfs::file_task::state::finish);
                }
                return nullptr;
            }
            if (task->state_ == vfs::file_task::state::size_timeout)
            {
                break;
            }
        }
    }

    if (task->abort)
    {
        task->state_ = vfs::file_task::state::running;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task.get());
        }
        if (task->state_cb)
        {
            call_state_callback(task, vfs::file_task::state::finish);
        }
        return nullptr;
    }

    // cancel the timer
    if (size_timeout)
    {
        g_source_remove_by_user_data(task.get());
    }

    if (task->state_pause_ == vfs::file_task::state::queue)
    {
        if (task->state_ != vfs::file_task::state::size_timeout &&
            xset_get_b(xset::name::task_q_smart))
        {
            // make queue exception for smaller tasks
            u64 exlimit;
            switch (task->type_)
            {
                case vfs::file_task::type::move:
                case vfs::file_task::type::copy:
                case vfs::file_task::type::trash:
                    exlimit = 10485760; // 10M
                    break;
                case vfs::file_task::type::del:
                    exlimit = 5368709120; // 5G
                    break;
                case vfs::file_task::type::link:
                case vfs::file_task::type::chmod_chown:
                case vfs::file_task::type::exec:
                case vfs::file_task::type::last:
                    exlimit = 0; // always exception for other types
                    break;
            }

            if (!exlimit || task->total_size < exlimit)
            {
                task->state_pause_ = vfs::file_task::state::running;
            }
        }
        // device list is populated so signal queue start
        task->queue_start = true;
    }

    if (task->state_ == vfs::file_task::state::size_timeout)
    {
        task->append_add_log("Timed out calculating total size\n");
        task->total_size = 0;
    }
    task->state_ = vfs::file_task::state::running;
    if (task->should_abort())
    {
        task->state_ = vfs::file_task::state::running;
        if (size_timeout)
        {
            g_source_remove_by_user_data(task.get());
        }
        if (task->state_cb)
        {
            call_state_callback(task, vfs::file_task::state::finish);
        }
        return nullptr;
    }

    for (const auto& src_path : task->src_paths)
    {
        switch (task->type_)
        {
            case vfs::file_task::type::move:
                task->file_move(src_path);
                break;
            case vfs::file_task::type::copy:
                task->file_copy(src_path);
                break;
            case vfs::file_task::type::trash:
                task->file_trash(src_path);
                break;
            case vfs::file_task::type::del:
                task->file_delete(src_path);
                break;
            case vfs::file_task::type::link:
                task->file_link(src_path);
                break;
            case vfs::file_task::type::chmod_chown:
                task->file_chown_chmod(src_path);
                break;
            case vfs::file_task::type::exec:
                task->file_exec(src_path);
                break;
            case vfs::file_task::type::last:
                break;
        }
    }

    task->state_ = vfs::file_task::state::running;
    if (size_timeout)
    {
        g_source_remove_by_user_data(task.get());
    }
    if (task->state_cb)
    {
        call_state_callback(task, vfs::file_task::state::finish);
    }
    return nullptr;
}

void
vfs::file_task::run_task()
{
    if (this->type_ != vfs::file_task::type::exec)
    {
        if (this->type_ == vfs::file_task::type::chmod_chown && !this->src_paths.empty())
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
vfs::file_task::try_abort_task()
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
    this->state_pause_ = vfs::file_task::state::running;
}

void
vfs::file_task::abort_task()
{
    this->abort = true;
    /* Called from another thread */
    if (this->thread && g_thread_self() != this->thread &&
        this->type_ != vfs::file_task::type::exec)
    {
        g_thread_join(this->thread);
        this->thread = nullptr;
    }
}

/*
 * Recursively count total size of all files in the specified directory.
 * If the path specified is a file, the size of the file is directly returned.
 * cancel is used to cancel the operation. This function will check the value
 * pointed by cancel in every iteration. If cancel is set to true, the
 * calculation is cancelled.
 * NOTE: *size should be set to zero before calling this function.
 */
u64
vfs::file_task::get_total_size_of_dir(const std::filesystem::path& path)
{
    if (this->abort)
    {
        return 0;
    }

    const auto file_stat = ztd::statx(path, ztd::statx::symlink::no_follow);
    if (!file_stat)
    {
        return 0;
    }

    u64 size = file_stat.size();

    // Do not follow symlinks
    if (file_stat.is_symlink() || !file_stat.is_directory())
    {
        return size;
    }

    for (const auto& file : std::filesystem::directory_iterator(path))
    {
        const auto filename = file.path().filename();
        if (this->state_ == vfs::file_task::state::size_timeout || this->abort)
        {
            break;
        }

        const auto full_path = path / filename;
        if (std::filesystem::exists(full_path))
        {
            const auto dir_file_stat = ztd::statx(full_path, ztd::statx::symlink::no_follow);

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
vfs::file_task::task_error(i32 errnox, const std::string_view action)
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

    call_state_callback(this->shared_from_this(), vfs::file_task::state::error);
}

void
vfs::file_task::task_error(i32 errnox, const std::string_view action,
                           const std::filesystem::path& target)
{
    this->error = errnox;
    const std::string errno_msg = std::strerror(errnox);
    const std::string msg = std::format("\n{} {}\nError: {}\n", action, target.string(), errno_msg);
    this->append_add_log(msg);
    call_state_callback(this->shared_from_this(), vfs::file_task::state::error);
}
