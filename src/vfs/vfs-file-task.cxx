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

#include <system_error>

#include <cstring>

#include <fcntl.h>
#include <utime.h>

#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "utils/shell-quote.hxx"
#include "utils/misc.hxx"

#include "ptk/ptk-dialog.hxx"

#include "xset/xset.hxx"

#include "terminal-handlers.hxx"

#include "vfs/vfs-volume.hxx"
#include "vfs/utils/vfs-utils.hxx"

#include "vfs/vfs-trash-can.hxx"
#include "vfs/vfs-file-task.hxx"

const std::shared_ptr<vfs::file_task>
vfs::file_task::create(const vfs::file_task::type task_type,
                       const std::span<const std::filesystem::path> src_files,
                       const std::filesystem::path& dest_dir) noexcept
{
    return std::make_shared<vfs::file_task>(task_type, src_files, dest_dir);
}

vfs::file_task::file_task(const vfs::file_task::type task_type,
                          const std::span<const std::filesystem::path> src_files,
                          const std::filesystem::path& dest_dir) noexcept
    : type_(task_type)
{
    this->src_paths = std::vector<std::filesystem::path>(src_files.begin(), src_files.end());
    if (!dest_dir.empty())
    {
        this->dest_dir = dest_dir;
    }

    this->is_recursive =
        (this->type_ == vfs::file_task::type::copy || this->type_ == vfs::file_task::type::del);

    // Init GMutex
    this->mutex = g_new(GMutex, 1);
    g_mutex_init(this->mutex);

    GtkTextIter iter;
    this->add_log_buf = gtk_text_buffer_new(nullptr);
    this->add_log_end = gtk_text_mark_new(nullptr, false);
    gtk_text_buffer_get_end_iter(this->add_log_buf, &iter);
    gtk_text_buffer_add_mark(this->add_log_buf, this->add_log_end, &iter);

    this->start_time = std::chrono::system_clock::now();
    this->timer = ztd::timer();
}

vfs::file_task::~file_task() noexcept
{
    g_mutex_clear(this->mutex);

    gtk_text_buffer_set_text(this->add_log_buf, "", -1);
    g_object_unref(this->add_log_buf);
}

void
vfs::file_task::lock() const noexcept
{
    g_mutex_lock(this->mutex);
}

void
vfs::file_task::unlock() const noexcept
{
    g_mutex_unlock(this->mutex);
}

void
vfs::file_task::set_state_callback(const state_callback_t& cb, void* user_data) noexcept
{
    this->state_cb = cb;
    this->state_cb_data = user_data;
}

void
vfs::file_task::set_chmod(std::array<u8, 12> new_chmod_actions) noexcept
{
    this->chmod_actions = new_chmod_actions;
}

void
vfs::file_task::set_chown(uid_t new_uid, gid_t new_gid) noexcept
{
    this->uid = new_uid;
    this->gid = new_gid;
}

void
vfs::file_task::set_recursive(bool recursive) noexcept
{
    this->is_recursive = recursive;
}

void
vfs::file_task::set_overwrite_mode(const vfs::file_task::overwrite_mode mode) noexcept
{
    this->overwrite_mode_ = mode;
}

void
vfs::file_task::append_add_log(const std::string_view msg) const noexcept
{
    this->lock();
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(this->add_log_buf, &iter, this->add_log_end);
    gtk_text_buffer_insert(this->add_log_buf, &iter, msg.data(), msg.length());
    this->unlock();
}

bool
vfs::file_task::should_abort() noexcept
{
    if (this->state_pause_ != vfs::file_task::state::running)
    {
        // paused or queued - suspend thread
        this->lock();
        this->timer.stop();

        this->pause_cond = g_new(GCond, 1);
        g_cond_init(this->pause_cond);
        g_cond_wait(this->pause_cond, this->mutex);
        // resume
        g_cond_clear(this->pause_cond);
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
vfs::file_task::check_overwrite(const std::filesystem::path& dest_file, bool& dest_exists,
                                std::filesystem::path& new_dest_file) noexcept
{
    const auto exists = std::filesystem::exists(dest_file);

    while (true)
    {
        new_dest_file = "";

        if (this->overwrite_mode_ == vfs::file_task::overwrite_mode::overwrite_all)
        {
            const auto checked_current_file = this->current_file.value_or("");
            const auto checked_current_dest = this->current_dest.value_or("");

            dest_exists = exists;
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
            dest_exists = exists;
            return !dest_exists;
        }
        else if (this->overwrite_mode_ == vfs::file_task::overwrite_mode::auto_rename)
        {
            dest_exists = exists;
            if (!dest_exists)
            {
                return !this->abort;
            }

            // auto-rename
            const auto old_name = dest_file.filename();
            const auto dest_file_dir = dest_file.parent_path();

            new_dest_file = vfs::utils::unique_name(dest_file_dir, old_name);
            dest_exists = false;
            return !this->abort;
        }

        dest_exists = exists;
        if (!dest_exists)
        {
            return !this->abort;
        }

        // dest exists - query user
        if (!this->state_cb)
        { // failsafe
            return false;
        }

        char* new_dest = nullptr;
        std::filesystem::path use_dest_file = dest_file;
        do
        {
            // destination file exists
            dest_exists = true;
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

                    dest_exists = exists;
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
            dest_exists = false;
            new_dest_file = new_dest;
            return !this->abort;
        }
    }
}

bool
vfs::file_task::check_dest_in_src(const std::filesystem::path& src_dir) noexcept
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
    this->append_add_log(std::format("Destination directory '{}' is contained in source '{}'",
                                     checked_dest_dir.string(),
                                     src_dir.string()));

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
vfs::file_task::file_copy(const std::filesystem::path& src_file) noexcept
{
    const auto filename = src_file.filename();
    const auto dest_file = this->dest_dir.value() / filename;

    const auto result = this->do_file_copy(src_file, dest_file);
    if (!result)
    {
        ztd::logger::error("File Copy failed {} -> {}", src_file.string(), dest_file.string());
    }
}

bool
vfs::file_task::do_file_copy(const std::filesystem::path& src_file,
                             const std::filesystem::path& dest_file) noexcept
{
    if (this->should_abort())
    {
        return false;
    }

    // ztd::logger::info("vfs::file_task::do_file_copy( {}, {} )", src_file, dest_file);
    this->lock();
    this->current_file = src_file;
    this->current_dest = dest_file;
    this->current_item++;
    this->unlock();

    std::error_code ec;
    const auto file_stat = ztd::lstat(src_file, ec);
    if (ec)
    {
        this->task_error(errno, "Accessing", src_file);
        return false;
    }

    std::filesystem::path new_dest_file;
    bool dest_exists = false;
    bool copy_fail = false;

    std::filesystem::path actual_dest_file = dest_file;

    if (std::filesystem::is_directory(src_file))
    {
        if (this->check_dest_in_src(src_file))
        {
            return false;
        }
        if (!this->check_overwrite(actual_dest_file, dest_exists, new_dest_file))
        {
            return false;
        }
        if (!new_dest_file.empty())
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
                    ztd::logger::error("File Copy failed {} -> {}",
                                       sub_src_file.string(),
                                       sub_dest_file.string());
                    copy_fail = true;
                }
            }

            chmod(actual_dest_file.c_str(), file_stat.mode());

            times.actime = std::chrono::system_clock::to_time_t(file_stat.atime());
            times.modtime = std::chrono::system_clock::to_time_t(file_stat.mtime());
            utime(actual_dest_file.c_str(), &times);

            /* Move files to different device: Need to delete source dir */
            if ((this->type_ == vfs::file_task::type::move) && !this->should_abort() && !copy_fail)
            {
                std::filesystem::remove_all(src_file);
                if (std::filesystem::exists(src_file))
                {
                    this->task_error(errno, "Removing", src_file);
                    copy_fail = true;
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
            if (!this->check_overwrite(actual_dest_file, dest_exists, new_dest_file))
            {
                return false;
            }

            if (!new_dest_file.empty())
            {
                actual_dest_file = new_dest_file;
                this->lock();
                this->current_dest = actual_dest_file;
                this->unlock();
            }

            // delete it first to prevent exists error
            if (dest_exists)
            {
                std::filesystem::remove(actual_dest_file);
                if (std::filesystem::exists(actual_dest_file) && errno != 2 /* no such file */)
                {
                    this->task_error(errno, "Removing", actual_dest_file);
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
            if (!this->check_overwrite(actual_dest_file, dest_exists, new_dest_file))
            {
                close(rfd);
                return false;
            }

            if (!new_dest_file.empty())
            {
                actual_dest_file = new_dest_file;
                this->lock();
                this->current_dest = actual_dest_file;
                this->unlock();
            }

            // if dest is a symlink, delete it first to prevent overwriting target!
            if (std::filesystem::is_symlink(actual_dest_file))
            {
                std::filesystem::remove(actual_dest_file);
                if (std::filesystem::exists(src_file))
                {
                    this->task_error(errno, "Removing", actual_dest_file);
                    close(rfd);
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
                isize rsize = 0;
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
                    // do not chmod link
                    if (!std::filesystem::is_symlink(actual_dest_file))
                    {
                        chmod(actual_dest_file.c_str(), file_stat.mode());
                        struct utimbuf times;
                        times.actime = std::chrono::system_clock::to_time_t(file_stat.atime());
                        times.modtime = std::chrono::system_clock::to_time_t(file_stat.mtime());
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

    if (!copy_fail && this->error_first)
    {
        this->error_first = false;
    }
    return !copy_fail;
}

void
vfs::file_task::file_move(const std::filesystem::path& src_file) noexcept
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

    std::error_code src_ec;
    const auto src_stat = ztd::lstat(src_file, src_ec);

    std::error_code dest_ec;
    const auto dest_stat = ztd::stat(this->dest_dir.value(), dest_ec);

    if (!src_ec && !dest_ec)
    {
        /* Not on the same device */
        if (src_stat.dev() != dest_stat.dev())
        {
            // ztd::logger::info("not on the same dev: {}", src_file);
            const auto result = this->do_file_copy(src_file, dest_file);
            if (!result)
            {
                ztd::logger::error("File Copy failed {} -> {}",
                                   src_file.string(),
                                   dest_file.string());
            }
        }
        else
        {
            // ztd::logger::info("on the same dev: {}", src_file);
            if (this->do_file_move(src_file, dest_file) == EXDEV)
            {
                // Invalid cross-device link (st_dev not always accurate test)
                // so now redo move as copy
                const auto result = this->do_file_copy(src_file, dest_file);
                if (!result)
                {
                    ztd::logger::error("File Copy failed {} -> {}",
                                       src_file.string(),
                                       dest_file.string());
                }
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
                             const std::filesystem::path& dest_path) noexcept
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
    std::error_code ec;
    const auto file_stat = ztd::lstat(src_file, ec);
    if (ec)
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

    std::filesystem::path new_dest_file;
    bool dest_exists = false;
    if (!this->check_overwrite(dest_file, dest_exists, new_dest_file))
    {
        return 0;
    }

    if (!new_dest_file.empty())
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
            const auto result = this->do_file_move(sub_src_file, sub_dest_file);
            if (!result)
            {
                ztd::logger::error("File Move failed {} -> {}",
                                   sub_src_file.string(),
                                   sub_dest_file.string());
            }
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

    return 0;
}

void
vfs::file_task::file_trash(const std::filesystem::path& src_file) noexcept
{
    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
    this->current_item++;
    this->unlock();

    std::error_code ec;
    const auto file_stat = ztd::lstat(src_file, ec);
    if (ec)
    {
        this->task_error(errno, "Accessing", src_file);
        return;
    }

    if (!::utils::have_rw_access(src_file))
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
vfs::file_task::file_delete(const std::filesystem::path& src_file) noexcept
{
    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
    this->current_item++;
    this->unlock();

    std::error_code ec;
    const auto file_stat = ztd::lstat(src_file, ec);
    if (ec)
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
vfs::file_task::file_link(const std::filesystem::path& src_file) noexcept
{
    if (this->should_abort())
    {
        return;
    }

    const auto filename = src_file.filename();
    const auto old_dest_file = this->dest_dir.value() / filename;
    auto dest_file = old_dest_file;

    // setup task for check overwrite
    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
    this->current_dest = old_dest_file;
    this->current_item++;
    this->unlock();

    std::error_code ec;
    const auto src_stat = ztd::stat(src_file, ec);
    if (ec)
    {
        // allow link to broken symlink
        if (errno != 2 || !std::filesystem::is_symlink(src_file))
        {
            this->task_error(errno, "Accessing", src_file);
            if (this->should_abort())
            {
                return;
            }
        }
    }

    // FIXME: Check overwrite!!
    bool dest_exists = false;
    std::filesystem::path new_dest_file;
    if (!this->check_overwrite(dest_file, dest_exists, new_dest_file))
    {
        return;
    }

    if (!new_dest_file.empty())
    {
        dest_file = new_dest_file;
        this->lock();
        this->current_dest = dest_file;
        this->unlock();
    }

    // if dest exists, delete it first to prevent exists error
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
}

void
vfs::file_task::file_chown_chmod(const std::filesystem::path& src_file) noexcept
{
    static constexpr std::array<std::filesystem::perms, 12> chmod_flags{
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

    if (this->should_abort())
    {
        return;
    }

    this->lock();
    this->current_file = src_file;
    this->current_item++;
    this->unlock();
    // ztd::logger::debug("chmod_chown: {}", src_file);

    std::error_code ec;
    const auto src_stat = ztd::lstat(src_file, ec);
    if (!ec)
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
                std::error_code ec_perm;
                std::filesystem::permissions(src_file, new_perms, ec_perm);
                if (ec_perm)
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
call_state_callback(const std::shared_ptr<vfs::file_task>& task,
                    const vfs::file_task::state state) noexcept
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

void
vfs::file_task::file_exec(const std::filesystem::path& src_file) noexcept
{
    this->state_ = vfs::file_task::state::running;
    this->current_file = src_file;

    if (this->exec_terminal)
    {
        // get terminal
        std::string terminal;
        const auto terminal_s = xset_get_s(xset::name::main_terminal);
        if (terminal_s)
        {
            terminal = Glib::find_program_in_path(terminal_s.value());
        }

        if (terminal.empty())
        {
            ptk::dialog::message(
                nullptr,
                GtkMessageType::GTK_MESSAGE_ERROR,
                "Terminal Not Available",
                GtkButtonsType::GTK_BUTTONS_OK,
                "Please set a valid terminal program in View|Preferences|Advanced");

            call_state_callback(this->shared_from_this(), vfs::file_task::state::finish);
            return;
        }

        std::string term_exec_command;
        const auto terminal_args = terminal_handlers->get_terminal_args(terminal_s.value());
        for (const std::string_view terminal_arg : terminal_args)
        {
            term_exec_command.append(terminal_arg);
            term_exec_command.append(" ");
        }
        term_exec_command.append(::utils::shell_quote(this->exec_command));

        ztd::logger::info("COMMAND({})", term_exec_command);
        Glib::spawn_command_line_async(term_exec_command);
    }
    else
    {
        ztd::logger::info("COMMAND({})", this->exec_command);
        Glib::spawn_command_line_async(this->exec_command);
    }

    call_state_callback(this->shared_from_this(), vfs::file_task::state::finish);
}

static bool
on_size_timeout(const std::shared_ptr<vfs::file_task>& task) noexcept
{
    if (!task->abort)
    {
        task->state_ = vfs::file_task::state::size_timeout;
    }
    return false;
}

static void*
vfs_file_task_thread(const std::shared_ptr<vfs::file_task>& task) noexcept
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
            if (std::filesystem::exists(src_path))
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
            std::error_code ec;
            const auto file_stat = ztd::lstat(task->dest_dir.value(), ec);
            if (!task->dest_dir || ec)
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
            std::error_code ec;
            const auto file_stat = ztd::lstat(src_path, ec);
            if (ec)
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
            u64 exlimit = 0;
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
vfs::file_task::run_task() noexcept
{
    if (this->type_ != vfs::file_task::type::exec)
    {
        if (this->type_ == vfs::file_task::type::chmod_chown && !this->src_paths.empty())
        {
            const auto dir = this->src_paths.at(0).parent_path();
            this->avoid_changes = vfs::volume_dir_avoid_changes(dir);
        }
        else
        {
            if (this->dest_dir)
            {
                const auto checked_dest_dir = this->dest_dir.value();
                this->avoid_changes = vfs::volume_dir_avoid_changes(checked_dest_dir);
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
vfs::file_task::try_abort_task() noexcept
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
vfs::file_task::abort_task() noexcept
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
vfs::file_task::get_total_size_of_dir(const std::filesystem::path& path) noexcept
{
    if (this->abort)
    {
        return 0;
    }

    std::error_code ec;
    const auto file_stat = ztd::lstat(path, ec);
    if (ec)
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
vfs::file_task::task_error(i32 errnox, const std::string_view action) noexcept
{
    if (errnox)
    {
        const std::string errno_msg = std::strerror(errnox);
        this->append_add_log(std::format("{}\n{}\n", action, errno_msg));
    }
    else
    {
        this->append_add_log(std::format("{}\n", action));
    }

    call_state_callback(this->shared_from_this(), vfs::file_task::state::error);
}

void
vfs::file_task::task_error(i32 errnox, const std::string_view action,
                           const std::filesystem::path& target) noexcept
{
    this->error = errnox;
    const std::string errno_msg = std::strerror(errnox);
    this->append_add_log(std::format("\n{} {}\nError: {}\n", action, target.string(), errno_msg));
    call_state_callback(this->shared_from_this(), vfs::file_task::state::error);
}
