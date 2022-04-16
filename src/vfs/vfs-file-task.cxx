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

#include <vector>

#include <iostream>
#include <fstream>

#include <chrono>

#include <fcntl.h>
#include <utime.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "main-window.hxx"
#include "vfs/vfs-volume.hxx"

#include "utils.hxx"

#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-file-task.hxx"
#include "vfs/vfs-file-trash.hxx"

const mode_t chmod_flags[] = {S_IRUSR,
                              S_IWUSR,
                              S_IXUSR,
                              S_IRGRP,
                              S_IWGRP,
                              S_IXGRP,
                              S_IROTH,
                              S_IWOTH,
                              S_IXOTH,
                              S_ISUID,
                              S_ISGID,
                              S_ISVTX};

/*
 * void get_total_size_of_dir( const char* path, off_t* size )
 * Recursively count total size of all files in the specified directory.
 * If the path specified is a file, the size of the file is directly returned.
 * cancel is used to cancel the operation. This function will check the value
 * pointed by cancel in every iteration. If cancel is set to true, the
 * calculation is cancelled.
 * NOTE: *size should be set to zero before calling this function.
 */
static void get_total_size_of_dir(VFSFileTask* task, const char* path, off_t* size,
                                  struct stat* have_stat);
static void vfs_file_task_error(VFSFileTask* task, int errnox, const char* action,
                                const char* target);
static void vfs_file_task_exec_error(VFSFileTask* task, int errnox, const std::string& action);
static void add_task_dev(VFSFileTask* task, dev_t dev);
static bool should_abort(VFSFileTask* task);

static void
vfs_file_task_init(VFSFileTask* task)
{
    task->mutex = (GMutex*)g_malloc(sizeof(GMutex));
    g_mutex_init(task->mutex);
}

void
vfs_file_task_lock(VFSFileTask* task)
{
    g_mutex_lock(task->mutex);
}

void
vfs_file_task_unlock(VFSFileTask* task)
{
    g_mutex_unlock(task->mutex);
}

static void
vfs_file_task_clear(VFSFileTask* task)
{
    g_mutex_clear(task->mutex);
    free(task->mutex);
}

static void
append_add_log(VFSFileTask* task, const std::string& msg)
{
    vfs_file_task_lock(task);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(task->add_log_buf, &iter, task->add_log_end);
    gtk_text_buffer_insert(task->add_log_buf, &iter, msg.c_str(), msg.length());
    vfs_file_task_unlock(task);
}

static void
call_state_callback(VFSFileTask* task, VFSFileTaskState state)
{
    task->state = state;
    if (task->state_cb)
    {
        if (!task->state_cb(task, state, nullptr, task->state_cb_data))
        {
            task->abort = true;
            if (task->type == VFS_FILE_TASK_EXEC && task->exec_cond)
            {
                // this is used only if exec task run in non-main loop thread
                vfs_file_task_lock(task);
                g_cond_broadcast(task->exec_cond);
                vfs_file_task_unlock(task);
            }
        }
        else
            task->state = VFS_FILE_TASK_RUNNING;
    }
}

static bool
should_abort(VFSFileTask* task)
{
    if (task->state_pause != VFS_FILE_TASK_RUNNING)
    {
        // paused or queued - suspend thread
        vfs_file_task_lock(task);
        g_timer_stop(task->timer);
        task->pause_cond = g_cond_new();
        g_cond_wait(task->pause_cond, task->mutex);
        // resume
        g_cond_free(task->pause_cond);
        task->pause_cond = nullptr;
        task->last_elapsed = g_timer_elapsed(task->timer, nullptr);
        task->last_progress = task->progress;
        task->last_speed = 0;
        g_timer_continue(task->timer);
        task->state_pause = VFS_FILE_TASK_RUNNING;
        vfs_file_task_unlock(task);
    }
    return task->abort;
}

char*
vfs_file_task_get_unique_name(const char* dest_dir, const char* base_name, const char* ext)
{ // returns nullptr if all names used; otherwise newly allocated string
    struct stat dest_stat;
    char* new_name = g_strdup_printf("%s%s%s", base_name, ext && ext[0] ? "." : "", ext ? ext : "");
    char* new_dest_file = g_build_filename(dest_dir, new_name, nullptr);
    free(new_name);
    unsigned int n = 1;
    while (n && lstat(new_dest_file, &dest_stat) == 0)
    {
        free(new_dest_file);
        new_name = g_strdup_printf("%s-%s%d%s%s",
                                   base_name,
                                   "copy",
                                   ++n,
                                   ext && ext[0] ? "." : "",
                                   ext ? ext : "");
        new_dest_file = g_build_filename(dest_dir, new_name, nullptr);
        free(new_name);
    }
    if (n == 0)
    {
        free(new_dest_file);
        return nullptr;
    }
    return new_dest_file;
}

/*
 * Check if the destination file exists.
 * If the dest_file exists, let the user choose a new destination,
 * skip/overwrite/auto-rename/all, pause, or cancel.
 * The returned string is the new destination file chosen by the user
 */
static bool
check_overwrite(VFSFileTask* task, const char* dest_file, bool* dest_exists, char** new_dest_file)
{
    struct stat dest_stat;
    char* new_dest;

    while (true)
    {
        *new_dest_file = nullptr;
        if (task->overwrite_mode == VFS_FILE_TASK_OVERWRITE_ALL)
        {
            *dest_exists = !lstat(dest_file, &dest_stat);
            if (ztd::same(task->current_file, task->current_dest))
            {
                // src and dest are same file - don't overwrite (truncates)
                // occurs if user pauses task and changes overwrite mode
                return false;
            }
            return true;
        }
        if (task->overwrite_mode == VFS_FILE_TASK_SKIP_ALL)
        {
            *dest_exists = !lstat(dest_file, &dest_stat);
            return !*dest_exists;
        }
        if (task->overwrite_mode == VFS_FILE_TASK_AUTO_RENAME)
        {
            *dest_exists = !lstat(dest_file, &dest_stat);
            if (!*dest_exists)
                return !task->abort;

            // auto-rename
            std::string ext;
            char* old_name = g_path_get_basename(dest_file);
            char* dest_dir = g_path_get_dirname(dest_file);
            std::string base_name = get_name_extension(old_name, ext);
            free(old_name);
            *new_dest_file =
                vfs_file_task_get_unique_name(dest_dir, base_name.c_str(), ext.c_str());
            *dest_exists = false;
            free(dest_dir);
            if (*new_dest_file)
                return !task->abort;
            // else ran out of names - fall through to query user
        }

        *dest_exists = !lstat(dest_file, &dest_stat);
        if (!*dest_exists)
            return !task->abort;

        // dest exists - query user
        if (!task->state_cb) // failsafe
            return false;
        const char* use_dest_file = dest_file;
        do
        {
            // destination file exists
            *dest_exists = true;
            task->state = VFS_FILE_TASK_QUERY_OVERWRITE;
            new_dest = nullptr;

            // query user
            if (!task->state_cb(task,
                                VFS_FILE_TASK_QUERY_OVERWRITE,
                                &new_dest,
                                task->state_cb_data))
                // task->abort is actually set in query_overwrite_response
                // VFS_FILE_TASK_QUERY_OVERWRITE never returns false
                task->abort = true;
            task->state = VFS_FILE_TASK_RUNNING;

            // may pause here - user may change overwrite mode
            if (should_abort(task))
            {
                free(new_dest);
                return false;
            }

            if (task->overwrite_mode != VFS_FILE_TASK_RENAME)
            {
                free(new_dest);
                new_dest = nullptr;
                switch (task->overwrite_mode)
                {
                    case VFS_FILE_TASK_OVERWRITE:
                    case VFS_FILE_TASK_OVERWRITE_ALL:
                        *dest_exists = !lstat(dest_file, &dest_stat);
                        if (ztd::same(task->current_file, task->current_dest))
                        {
                            // src and dest are same file - don't overwrite (truncates)
                            // occurs if user pauses task and changes overwrite mode
                            return false;
                        }
                        return true;
                        break;
                    case VFS_FILE_TASK_AUTO_RENAME:
                        break;
                    default:
                        return false;
                }
            }
            // user renamed file or pressed Pause btn
            if (new_dest)
                // user renamed file - test if new name exists
                use_dest_file = new_dest;
        } while (lstat(use_dest_file, &dest_stat) != -1);
        if (new_dest)
        {
            // user renamed file to unique name
            *dest_exists = false;
            *new_dest_file = new_dest;
            return !task->abort;
        }
    }
}

static bool
check_dest_in_src(VFSFileTask* task, const char* src_dir)
{
    char real_src_path[PATH_MAX];
    char real_dest_path[PATH_MAX];
    int len;

    if (!(!task->dest_dir.empty() && realpath(task->dest_dir.c_str(), real_dest_path)))
        return false;

    if (realpath(src_dir, real_src_path) && Glib::str_has_prefix(real_dest_path, real_src_path) &&
        (len = std::strlen(real_src_path)) &&
        (real_dest_path[len] == '/' || real_dest_path[len] == '\0'))
    {
        // source is contained in destination dir
        std::string disp_src = g_filename_display_name(src_dir);
        std::string disp_dest = g_filename_display_name(task->dest_dir.c_str());
        std::string err = fmt::format("Destination directory \"{}\" is contained in source \"{}\"",
                                      disp_dest,
                                      disp_src);
        append_add_log(task, err);
        if (task->state_cb)
            task->state_cb(task, VFS_FILE_TASK_ERROR, nullptr, task->state_cb_data);
        task->state = VFS_FILE_TASK_RUNNING;
        return true;
    }
    return false;
}

static void
update_file_display(const char* path)
{
    // for devices like nfs, emit created and flush to avoid a
    // blocking stat call in GUI thread during writes
    char* dir_path = g_path_get_dirname(path);
    VFSDir* vdir = vfs_dir_get_by_path_soft(dir_path);
    free(dir_path);
    if (vdir && vdir->avoid_changes)
    {
        VFSFileInfo* file = vfs_file_info_new();
        vfs_file_info_get(file, path, nullptr);
        vfs_dir_emit_file_created(vdir, vfs_file_info_get_name(file), true);
        vfs_file_info_unref(file);
        vfs_dir_flush_notify_cache();
    }
    if (vdir)
        g_object_unref(vdir);
}

static bool
vfs_file_task_do_copy(VFSFileTask* task, const char* src_file, const char* dest_file)
{
    struct stat file_stat;
    char buffer[4096];
    int rfd;
    int wfd;
    char* new_dest_file = nullptr;
    bool dest_exists;
    bool copy_fail = false;

    if (should_abort(task))
        return false;
    // LOG_INFO("vfs_file_task_do_copy( {}, {} )", src_file, dest_file);
    vfs_file_task_lock(task);
    task->current_file = src_file;
    task->current_dest = dest_file;
    task->current_item++;
    vfs_file_task_unlock(task);

    if (lstat(src_file, &file_stat) == -1)
    {
        vfs_file_task_error(task, errno, "Accessing", src_file);
        return false;
    }

    if (S_ISDIR(file_stat.st_mode))
    {
        if (check_dest_in_src(task, src_file))
            goto _return_;

        if (!check_overwrite(task, dest_file, &dest_exists, &new_dest_file))
            goto _return_;
        if (new_dest_file)
        {
            dest_file = new_dest_file;
            vfs_file_task_lock(task);
            task->current_dest = dest_file;
            vfs_file_task_unlock(task);
        }

        if (!dest_exists)
        {
            std::filesystem::create_directories(dest_file);
            std::filesystem::permissions(dest_file, std::filesystem::perms::owner_all);
        }

        if (std::filesystem::is_directory(src_file))
        {
            struct utimbuf times;
            vfs_file_task_lock(task);
            task->progress += file_stat.st_size;
            vfs_file_task_unlock(task);

            std::string sub_src_file;
            std::string sub_dest_file;
            std::string file_name;
            for (const auto& file: std::filesystem::directory_iterator(src_file))
            {
                file_name = std::filesystem::path(file).filename();
                if (should_abort(task))
                    break;
                sub_src_file = Glib::build_filename(src_file, file_name);
                sub_dest_file = Glib::build_filename(dest_file, file_name);
                if (!vfs_file_task_do_copy(task, sub_src_file.c_str(), sub_dest_file.c_str()) &&
                    !copy_fail)
                    copy_fail = true;
            }

            chmod(dest_file, file_stat.st_mode);
            times.actime = file_stat.st_atime;
            times.modtime = file_stat.st_mtime;
            utime(dest_file, &times);

            if (task->avoid_changes)
                update_file_display(dest_file);

            /* Move files to different device: Need to delete source dir */
            if ((task->type == VFS_FILE_TASK_MOVE) && !should_abort(task) && !copy_fail)
            {
                std::filesystem::remove_all(src_file);
                if (std::filesystem::exists(src_file))
                {
                    vfs_file_task_error(task, errno, "Removing", src_file);
                    copy_fail = true;
                    if (should_abort(task))
                        goto _return_;
                }
            }
        }
        else
        {
            vfs_file_task_error(task, errno, "Creating Dir", dest_file);
            copy_fail = true;
        }
    }
    else if (S_ISLNK(file_stat.st_mode))
    {
        if ((rfd = readlink(src_file, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[rfd] = '\0'; // MOD terminate buffer string
            if (!check_overwrite(task, dest_file, &dest_exists, &new_dest_file))
                goto _return_;

            if (new_dest_file)
            {
                dest_file = new_dest_file;
                vfs_file_task_lock(task);
                task->current_dest = dest_file;
                vfs_file_task_unlock(task);
            }

            // MOD delete it first to prevent exists error
            if (dest_exists)
            {
                std::filesystem::remove(dest_file);
                if (std::filesystem::exists(dest_file) && errno != 2 /* no such file */)
                {
                    vfs_file_task_error(task, errno, "Removing", dest_file);
                    goto _return_;
                }
            }

            std::filesystem::create_symlink(buffer, dest_file);
            if (std::filesystem::is_symlink(dest_file))
            {
                /* Move files to different device: Need to delete source files */
                if ((task->type == VFS_FILE_TASK_MOVE) && !copy_fail)
                {
                    std::filesystem::remove(src_file);
                    if (std::filesystem::exists(src_file))
                    {
                        vfs_file_task_error(task, errno, "Removing", src_file);
                        copy_fail = true;
                    }
                }
                vfs_file_task_lock(task);
                task->progress += file_stat.st_size;
                vfs_file_task_unlock(task);
            }
            else
            {
                vfs_file_task_error(task, errno, "Creating Link", dest_file);
                copy_fail = true;
            }
        }
        else
        {
            vfs_file_task_error(task, errno, "Accessing", src_file);
            copy_fail = true;
        }
    }
    else
    {
        if ((rfd = open(src_file, O_RDONLY)) >= 0)
        {
            if (!check_overwrite(task, dest_file, &dest_exists, &new_dest_file))
            {
                close(rfd);
                goto _return_;
            }

            if (new_dest_file)
            {
                dest_file = new_dest_file;
                vfs_file_task_lock(task);
                task->current_dest = dest_file;
                vfs_file_task_unlock(task);
            }

            // MOD if dest is a symlink, delete it first to prevent overwriting target!
            if (std::filesystem::is_symlink(dest_file))
            {
                std::filesystem::remove(dest_file);
                if (std::filesystem::exists(src_file))
                {
                    vfs_file_task_error(task, errno, "Removing", dest_file);
                    close(rfd);
                    goto _return_;
                }
            }

            if ((wfd = creat(dest_file, file_stat.st_mode | S_IWUSR)) >= 0)
            {
                // sshfs becomes unresponsive with this, nfs is okay with it
                // if ( task->avoid_changes )
                //    emit_created( dest_file );
                struct utimbuf times;
                ssize_t rsize;
                while ((rsize = read(rfd, buffer, sizeof(buffer))) > 0)
                {
                    if (should_abort(task))
                    {
                        copy_fail = true;
                        break;
                    }

                    if (write(wfd, buffer, rsize) > 0)
                    {
                        vfs_file_task_lock(task);
                        task->progress += rsize;
                        vfs_file_task_unlock(task);
                    }
                    else
                    {
                        vfs_file_task_error(task, errno, "Writing", dest_file);
                        copy_fail = true;
                        break;
                    }
                }
                close(wfd);
                if (copy_fail)
                {
                    std::filesystem::remove(dest_file);
                    if (std::filesystem::exists(src_file) && errno != 2 /* no such file */)
                    {
                        vfs_file_task_error(task, errno, "Removing", dest_file);
                        copy_fail = true;
                    }
                }
                else
                {
                    // MOD don't chmod link
                    if (!std::filesystem::is_symlink(dest_file))
                    {
                        chmod(dest_file, file_stat.st_mode);
                        times.actime = file_stat.st_atime;
                        times.modtime = file_stat.st_mtime;
                        utime(dest_file, &times);
                    }
                    if (task->avoid_changes)
                        update_file_display(dest_file);

                    /* Move files to different device: Need to delete source files */
                    if ((task->type == VFS_FILE_TASK_MOVE) && !should_abort(task))
                    {
                        std::filesystem::remove(src_file);
                        if (std::filesystem::exists(src_file))
                        {
                            vfs_file_task_error(task, errno, "Removing", src_file);
                            copy_fail = true;
                        }
                    }
                }
            }
            else
            {
                vfs_file_task_error(task, errno, "Creating", dest_file);
                copy_fail = true;
            }
            close(rfd);
        }
        else
        {
            vfs_file_task_error(task, errno, "Accessing", src_file);
            copy_fail = true;
        }
    }
    if (new_dest_file)
        free(new_dest_file);
    if (!copy_fail && task->error_first)
        task->error_first = false;
    return !copy_fail;
_return_:

    if (new_dest_file)
        free(new_dest_file);
    return false;
}

static void
vfs_file_task_copy(char* src_file, VFSFileTask* task)
{
    char* file_name;
    char* dest_file;

    file_name = g_path_get_basename(src_file);
    dest_file = g_build_filename(task->dest_dir.c_str(), file_name, nullptr);
    free(file_name);
    vfs_file_task_do_copy(task, src_file, dest_file);
    free(dest_file);
}

static int
vfs_file_task_do_move(VFSFileTask* task, const char* src_file,
                      const char* dest_file) // MOD void to int
{
    if (should_abort(task))
        return 0;

    vfs_file_task_lock(task);
    task->current_file = src_file;
    task->current_dest = dest_file;
    task->current_item++;
    vfs_file_task_unlock(task);

    // LOG_DEBUG("move '{}' to '{}'", src_file, dest_file);
    struct stat file_stat;
    if (lstat(src_file, &file_stat) == -1)
    {
        vfs_file_task_error(task, errno, "Accessing", src_file);
        return 0;
    }

    if (should_abort(task))
        return 0;

    if (S_ISDIR(file_stat.st_mode) && check_dest_in_src(task, src_file))
        return 0;

    char* new_dest_file = nullptr;
    bool dest_exists;
    if (!check_overwrite(task, dest_file, &dest_exists, &new_dest_file))
        return 0;

    if (new_dest_file)
    {
        dest_file = new_dest_file;
        vfs_file_task_lock(task);
        task->current_dest = dest_file;
        vfs_file_task_unlock(task);
    }

    if (S_ISDIR(file_stat.st_mode) && std::filesystem::is_directory(dest_file))
    {
        // moving a directory onto a directory that exists
        std::string sub_src_file;
        std::string sub_dest_file;
        std::string file_name;
        for (const auto& file: std::filesystem::directory_iterator(src_file))
        {
            file_name = std::filesystem::path(file).filename();
            if (should_abort(task))
                break;
            sub_src_file = Glib::build_filename(src_file, file_name);
            sub_dest_file = Glib::build_filename(dest_file, file_name);
            vfs_file_task_do_move(task, sub_src_file.c_str(), sub_dest_file.c_str());
        }
        // remove moved src dir if empty
        if (!should_abort(task))
            std::filesystem::remove_all(src_file);

        return 0;
    }

    int result = rename(src_file, dest_file);

    if (result != 0)
    {
        if (result == -1 && errno == EXDEV) // MOD Invalid cross-link device
            return 18;
        vfs_file_task_error(task, errno, "Renaming", src_file);
        if (should_abort(task))
        {
            free(new_dest_file);
            return 0;
        }
    }
    // MOD don't chmod link
    else if (!std::filesystem::is_symlink(dest_file))
        chmod(dest_file, file_stat.st_mode);

    vfs_file_task_lock(task);
    task->progress += file_stat.st_size;
    if (task->error_first)
        task->error_first = false;
    vfs_file_task_unlock(task);

    if (new_dest_file)
        free(new_dest_file);
    return 0;
}

static void
vfs_file_task_move(char* src_file, VFSFileTask* task)
{
    if (should_abort(task))
        return;

    vfs_file_task_lock(task);
    task->current_file = src_file;
    vfs_file_task_unlock(task);

    char* file_name = g_path_get_basename(src_file);

    char* dest_file = g_build_filename(task->dest_dir.c_str(), file_name, nullptr);

    free(file_name);

    struct stat src_stat;
    struct stat dest_stat;
    if (lstat(src_file, &src_stat) == 0 && stat(task->dest_dir.c_str(), &dest_stat) == 0)
    {
        /* Not on the same device */
        if (src_stat.st_dev != dest_stat.st_dev)
        {
            // LOG_INFO("not on the same dev: {}", src_file);
            vfs_file_task_do_copy(task, src_file, dest_file);
        }
        else
        {
            // LOG_INFO("on the same dev: {}", src_file);
            if (vfs_file_task_do_move(task, src_file, dest_file) == EXDEV) // MOD
            {
                // MOD Invalid cross-device link (st_dev not always accurate test)
                // so now redo move as copy
                vfs_file_task_do_copy(task, src_file, dest_file);
            }
        }
    }
    else
        vfs_file_task_error(task, errno, "Accessing", src_file);
}

static void
vfs_file_task_trash(char* src_file, VFSFileTask* task)
{
    if (should_abort(task))
        return;

    vfs_file_task_lock(task);
    task->current_file = src_file;
    task->current_item++;
    vfs_file_task_unlock(task);

    struct stat file_stat;
    if (lstat(src_file, &file_stat) == -1)
    {
        vfs_file_task_error(task, errno, "Accessing", src_file);
        return;
    }

    bool result = Trash::trash(std::string(src_file));

    if (!result)
    {
        vfs_file_task_error(task, errno, "Trashing", src_file);
        return;
    }

    vfs_file_task_lock(task);
    task->progress += file_stat.st_size;
    if (task->error_first)
        task->error_first = false;
    vfs_file_task_unlock(task);
}

static void
vfs_file_task_delete(const char* src_file, VFSFileTask* task)
{
    if (should_abort(task))
        return;

    vfs_file_task_lock(task);
    task->current_file = src_file;
    task->current_item++;
    vfs_file_task_unlock(task);

    struct stat file_stat;
    if (lstat(src_file, &file_stat) == -1)
    {
        vfs_file_task_error(task, errno, "Accessing", src_file);
        return;
    }

    if (S_ISDIR(file_stat.st_mode))
    {
        std::string file_name;
        for (const auto& file: std::filesystem::directory_iterator(src_file))
        {
            file_name = std::filesystem::path(file).filename();
            if (should_abort(task))
                break;
            std::string sub_src_file = Glib::build_filename(src_file, file_name);
            vfs_file_task_delete(sub_src_file.c_str(), task);
        }

        if (should_abort(task))
            return;
        std::filesystem::remove_all(src_file);
        if (std::filesystem::exists(src_file))
        {
            vfs_file_task_error(task, errno, "Removing", src_file);
            return;
        }
    }
    else
    {
        std::filesystem::remove(src_file);
        if (std::filesystem::exists(src_file))
        {
            vfs_file_task_error(task, errno, "Removing", src_file);
            return;
        }
    }
    vfs_file_task_lock(task);
    task->progress += file_stat.st_size;
    if (task->error_first)
        task->error_first = false;
    vfs_file_task_unlock(task);
}

static void
vfs_file_task_link(char* src_file, VFSFileTask* task)
{
    if (should_abort(task))
        return;

    char* file_name = g_path_get_basename(src_file);
    char* old_dest_file = g_build_filename(task->dest_dir.c_str(), file_name, nullptr);
    free(file_name);
    char* dest_file = old_dest_file;

    // MOD  setup task for check overwrite
    if (should_abort(task))
        return;
    vfs_file_task_lock(task);
    task->current_file = src_file;
    task->current_dest = old_dest_file;
    task->current_item++;
    vfs_file_task_unlock(task);

    struct stat src_stat;
    if (stat(src_file, &src_stat) == -1)
    {
        // MOD allow link to broken symlink
        if (errno != 2 || !std::filesystem::is_symlink(src_file))
        {
            vfs_file_task_error(task, errno, "Accessing", src_file);
            if (should_abort(task))
                return;
        }
    }

    /* FIXME: Check overwrite!! */ // MOD added check overwrite
    bool dest_exists;
    char* new_dest_file = nullptr;
    if (!check_overwrite(task, dest_file, &dest_exists, &new_dest_file))
        return;

    if (new_dest_file)
    {
        dest_file = new_dest_file;
        vfs_file_task_lock(task);
        task->current_dest = dest_file;
        vfs_file_task_unlock(task);
    }

    // MOD if dest exists, delete it first to prevent exists error
    if (dest_exists)
    {
        std::filesystem::remove(dest_file);
        if (std::filesystem::exists(src_file))
        {
            vfs_file_task_error(task, errno, "Removing", dest_file);
            return;
        }
    }

    std::filesystem::create_symlink(src_file, dest_file);
    if (!std::filesystem::is_symlink(dest_file))
    {
        vfs_file_task_error(task, errno, "Creating Link", dest_file);
        if (should_abort(task))
            return;
    }

    vfs_file_task_lock(task);
    task->progress += src_stat.st_size;
    if (task->error_first)
        task->error_first = false;
    vfs_file_task_unlock(task);

    if (new_dest_file)
        free(new_dest_file);
    free(old_dest_file);
}

static void
vfs_file_task_chown_chmod(const char* src_file, VFSFileTask* task)
{
    if (should_abort(task))
        return;

    vfs_file_task_lock(task);
    task->current_file = src_file;
    task->current_item++;
    vfs_file_task_unlock(task);
    // LOG_DEBUG("chmod_chown: {}", src_file);

    struct stat src_stat;
    if (lstat(src_file, &src_stat) == 0)
    {
        /* chown */
        int result;
        if (!task->uid || !task->gid)
        {
            result = chown(src_file, task->uid, task->gid);
            if (result != 0)
            {
                vfs_file_task_error(task, errno, "chown", src_file);
                if (should_abort(task))
                    return;
            }
        }

        /* chmod */
        if (task->chmod_actions)
        {
            mode_t new_mode = src_stat.st_mode;
            int i;
            for (i = 0; i < N_CHMOD_ACTIONS; ++i)
            {
                if (task->chmod_actions[i] == 2) /* Don't change */
                    continue;
                if (task->chmod_actions[i] == 0) /* Remove this bit */
                    new_mode &= ~chmod_flags[i];
                else /* Add this bit */
                    new_mode |= chmod_flags[i];
            }
            if (new_mode != src_stat.st_mode)
            {
                result = chmod(src_file, new_mode);
                if (result != 0)
                {
                    vfs_file_task_error(task, errno, "chmod", src_file);
                    if (should_abort(task))
                        return;
                }
            }
        }

        vfs_file_task_lock(task);
        task->progress += src_stat.st_size;
        vfs_file_task_unlock(task);

        if (task->avoid_changes)
            update_file_display(src_file);

        if (S_ISDIR(src_stat.st_mode) && task->recursive)
        {
            std::string file_name;
            for (const auto& file: std::filesystem::directory_iterator(src_file))
            {
                file_name = std::filesystem::path(file).filename();
                if (should_abort(task))
                    break;
                std::string sub_src_file = Glib::build_filename(src_file, file_name);
                vfs_file_task_chown_chmod(sub_src_file.c_str(), task);
            }
        }
    }
    if (task->error_first)
        task->error_first = false;
}

char*
vfs_file_task_get_cpids(Glib::Pid pid)
{
    // get child pids recursively as multi-line string
    if (!pid)
        return nullptr;

    std::string* standard_output = nullptr;
    int exit_status;

    std::string command = fmt::format("/bin/ps h --ppid {} -o pid", pid);
    print_command(command);
    Glib::spawn_command_line_sync(command.c_str(), standard_output, nullptr, &exit_status);

    if (standard_output->empty())
        return nullptr;

    char* cpids = ztd::strdup(standard_output);
    // get grand cpids recursively
    char* pids = ztd::strdup(standard_output);
    char* nl;
    while ((nl = strchr(pids, '\n')))
    {
        nl[0] = '\0';
        char* pida = ztd::strdup(pids);
        nl[0] = '\n';
        pids = nl + 1;
        Glib::Pid pidi = strtol(pida, nullptr, 10);
        free(pida);
        if (pidi)
        {
            char* gcpids;
            if ((gcpids = vfs_file_task_get_cpids(pidi)))
            {
                char* old_cpids = cpids;
                cpids = g_strdup_printf("%s%s", old_cpids, gcpids);
                free(old_cpids);
                free(gcpids);
            }
        }
    }
    free(pids);

    // LOG_INFO("vfs_file_task_get_cpids {}[{}]", pid, cpids );
    return cpids;
}

void
vfs_file_task_kill_cpids(char* cpids, int signal)
{
    if (!signal || !cpids || cpids[0] == '\0')
        return;

    char* pids = cpids;
    char* nl;
    while ((nl = strchr(pids, '\n')))
    {
        nl[0] = '\0';
        char* pida = ztd::strdup(pids);
        nl[0] = '\n';
        pids = nl + 1;
        Glib::Pid pidi = strtol(pida, nullptr, 10);
        free(pida);
        if (pidi)
        {
            // LOG_INFO("KILL_CPID pidi={} signal={}", pidi, signal );
            kill(pidi, signal);
        }
    }
}

static void
cb_exec_child_cleanup(Glib::Pid pid, int status, char* tmp_file)
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
cb_exec_child_watch(Glib::Pid pid, int status, VFSFileTask* task)
{
    bool bad_status = false;
    g_spawn_close_pid(pid);
    task->exec_pid = 0;
    task->child_watch = 0;

    if (status)
    {
        if (WIFEXITED(status))
            task->exec_exit_status = WEXITSTATUS(status);
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
        call_state_callback(task, VFS_FILE_TASK_ERROR);

    if (bad_status || (!task->exec_channel_out && !task->exec_channel_err))
        call_state_callback(task, VFS_FILE_TASK_FINISH);
}

static bool
cb_exec_out_watch(GIOChannel* channel, GIOCondition cond, VFSFileTask* task)
{
    /*
    LOG_INFO("cb_exec_out_watch {:p}", channel);
    if ( cond & G_IO_IN )
        LOG_INFO("    G_IO_IN");
    if ( cond & G_IO_OUT )
        LOG_INFO("    G_IO_OUT");
    if ( cond & G_IO_PRI )
        LOG_INFO("    G_IO_PRI");
    if ( cond & G_IO_ERR )
        LOG_INFO("    G_IO_ERR");
    if ( cond & G_IO_HUP )
        LOG_INFO("    G_IO_HUP");
    if ( cond & G_IO_NVAL )
        LOG_INFO("    G_IO_NVAL");

    if ( !( cond & G_IO_NVAL ) )
    {
        int fd = g_io_channel_unix_get_fd( channel );
        LOG_INFO("    fd={}", fd);
        if ( fcntl(fd, F_GETFL) != -1 || errno != EBADF )
        {
            int flags = g_io_channel_get_flags( channel );
            if ( flags & G_IO_FLAG_IS_READABLE )
                LOG_INFO( "    G_IO_FLAG_IS_READABLE");
        }
        else
            LOG_INFO("    Invalid FD");
    }
    */

    if ((cond & G_IO_NVAL))
    {
        g_io_channel_unref(channel);
        return false;
    }
    else if (!(cond & G_IO_IN))
    {
        if ((cond & G_IO_HUP))
            goto _unref_channel;
        else
            return true;
    }
    else if (!(fcntl(g_io_channel_unix_get_fd(channel), F_GETFL) != -1 || errno != EBADF))
    {
        // bad file descriptor - occurs with stop on fast output
        goto _unref_channel;
    }

    // GError *error = nullptr;
    unsigned long size;
    char buf[2048];
    if (g_io_channel_read_chars(channel, buf, sizeof(buf), &size, nullptr) == G_IO_STATUS_NORMAL &&
        size > 0)
        append_add_log(task, buf);
    else
        LOG_INFO("cb_exec_out_watch: g_io_channel_read_chars != G_IO_STATUS_NORMAL");

    return true;

_unref_channel:
    g_io_channel_unref(channel);
    if (channel == task->exec_channel_out)
        task->exec_channel_out = nullptr;
    else if (channel == task->exec_channel_err)
        task->exec_channel_err = nullptr;
    if (!task->exec_channel_out && !task->exec_channel_err && !task->exec_pid)
        call_state_callback(task, VFS_FILE_TASK_FINISH);
    return false;
}

static char*
get_xxhash(const char* path)
{
    std::string xxhash = Glib::find_program_in_path("xxh128sum");
    if (xxhash.empty())
    {
        LOG_WARN("Missing program xxhash");
        return nullptr;
    }

    std::string* standard_output = nullptr;
    std::string command = fmt::format("{} {}", xxhash, path);
    print_command(command);
    Glib::spawn_command_line_sync(command, standard_output);

    return ztd::strdup(standard_output);
}

static void
vfs_file_task_exec_error(VFSFileTask* task, int errnox, const std::string& action)
{
    std::string msg;

    if (errnox)
    {
        std::string errno_msg = Glib::strerror(errnox);
        msg = fmt::format("{}\n{}\n", action, errno_msg);
    }
    else
    {
        msg = fmt::format("{}\n", action);
    }

    append_add_log(task, msg);
    call_state_callback(task, VFS_FILE_TASK_ERROR);
}

static void
vfs_file_task_exec(char* src_file, VFSFileTask* task)
{
    // this function is now thread safe but is not currently run in
    // another thread because gio adds watches to main loop thread anyway
    std::string tmp;
    std::string su;
    std::string msg;
    std::string terminal;
    char* sum_script = nullptr;
    GtkWidget* parent = nullptr;

    // LOG_INFO("vfs_file_task_exec");
    // task->exec_keep_tmp = true;

    vfs_file_task_lock(task);

    if (task->exec_browser)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(task->exec_browser));
    else if (task->exec_desktop)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(task->exec_desktop));

    task->state = VFS_FILE_TASK_RUNNING;
    task->current_file = src_file;
    task->total_size = 0;
    task->percent = 0;
    vfs_file_task_unlock(task);

    if (should_abort(task))
        return;

    // need su?
    if (!task->exec_as_user.empty())
    {
        if (geteuid() == 0 && ztd::same(task->exec_as_user, "root"))
        {
            // already root so no su
            task->exec_as_user.clear();
        }
        else
        {
            // get su programs
            su = get_valid_su();
            if (su.empty())
            {
                msg = "Configure a valid Terminal SU command in View|Preferences|Advanced";
                LOG_WARN(msg);
                // do not use xset_msg_dialog if non-main thread
                // vfs_file_task_exec_error(task, 0, str);
                xset_msg_dialog(parent,
                                GTK_MESSAGE_ERROR,
                                "Terminal SU Not Available",
                                GTK_BUTTONS_OK,
                                msg);
                call_state_callback(task, VFS_FILE_TASK_FINISH);
                // LOG_INFO("vfs_file_task_exec DONE ERROR");
                return;
            }
        }
    }

    // make tmpdir
    tmp = xset_get_user_tmp_dir();
    if (!std::filesystem::is_directory(tmp))
    {
        msg = "Cannot create temporary directory";
        LOG_WARN(msg);
        // do not use xset_msg_dialog if non-main thread
        // vfs_file_task_exec_error(task, 0, str);
        xset_msg_dialog(parent, GTK_MESSAGE_ERROR, "Error", GTK_BUTTONS_OK, msg);
        call_state_callback(task, VFS_FILE_TASK_FINISH);
        // LOG_INFO("vfs_file_task_exec DONE ERROR");
        return;
    }

    // get terminal
    if (!task->exec_terminal && !task->exec_as_user.empty())
    {
        // using cli tool so run in terminal
        task->exec_terminal = true;
    }
    if (task->exec_terminal)
    {
        // get terminal
        terminal = Glib::find_program_in_path(xset_get_s("main_terminal"));
        if (terminal.empty())
        {
            msg = "Please set a valid terminal program in View|Preferences|Advanced";
            LOG_WARN(msg);
            // do not use xset_msg_dialog if non-main thread
            // vfs_file_task_exec_error(task, 0, str);
            xset_msg_dialog(parent,
                            GTK_MESSAGE_ERROR,
                            "Terminal Not Available",
                            GTK_BUTTONS_OK,
                            msg);

            call_state_callback(task, VFS_FILE_TASK_FINISH);
            // LOG_INFO("vfs_file_task_exec DONE ERROR");
            return;
        }
    }

    // Build exec script
    if (!task->exec_direct)
    {
        // get script name
        do
        {
            std::string hexname = fmt::format("{}-tmp.sh", randhex8());
            task->exec_script = Glib::build_filename(tmp, hexname);
        } while (std::filesystem::exists(task->exec_script));

        // open buffer
        std::string buf = "";

        // build - header
        buf.append(fmt::format("#!{}\n{}\n#tmp exec script\n", BASHPATH, SHELL_SETTINGS));

        // build - exports
        if (task->exec_export && (task->exec_browser || task->exec_desktop))
        {
            if (task->exec_browser)
            {
                main_write_exports(task, task->current_dest.c_str(), buf);
            }
            else
            {
                vfs_file_task_exec_error(task, errno, "Error writing temporary file");

                if (!task->exec_keep_tmp)
                {
                    if (std::filesystem::exists(task->exec_script))
                        std::filesystem::remove(task->exec_script);
                }
                call_state_callback(task, VFS_FILE_TASK_FINISH);
                // LOG_INFO("vfs_file_task_exec DONE ERROR");
                return;
            }
        }
        else
        {
            if (task->exec_export && !task->exec_browser && !task->exec_desktop)
            {
                task->exec_export = false;
                LOG_WARN("exec_export set without exec_browser/exec_desktop");
            }
        }

        // build - run
        buf.append(fmt::format("#run\nif [ \"$1\" == \"run\" ];then\n\n"));

        // build - write root settings
        if (task->exec_write_root && geteuid() != 0)
        {
            const std::string root_set_path =
                fmt::format("{}/spacefm/{}-as-root", SYSCONFDIR, Glib::get_user_name());
            write_root_settings(buf, root_set_path);
        }

        // build - export vars
        if (task->exec_export)
            buf.append(fmt::format("export fm_import=\"source {}\"\n", task->exec_script));
        else
            buf.append(fmt::format("export fm_import=\"\"\n"));

        buf.append(fmt::format("export fm_source=\"{}\"\n\n", task->exec_script));

        // build - trap rm
        if (!task->exec_keep_tmp && geteuid() != 0 && ztd::same(task->exec_as_user, "root"))
        {
            // run as root command, clean up
            buf.append(fmt::format("trap \"rm -f {}; exit\" EXIT SIGINT SIGTERM SIGQUIT SIGHUP\n\n",
                                   task->exec_script));
        }

        // build - command
        print_task_command((char*)task->exec_ptask, task->exec_command.c_str());

        buf.append(fmt::format("{}\nfm_err=$?\n", task->exec_command));

        // build - press enter to close
        if (!terminal.empty() && task->exec_keep_terminal)
        {
            if (geteuid() == 0 || ztd::same(task->exec_as_user, "root"))
                buf.append(fmt::format("\necho;read -p '[ Finished ]  Press Enter to close: '\n"));
            else
            {
                buf.append(
                    fmt::format("\necho;read -p '[ Finished ]  Press Enter to close or s + "
                                "Enter for a shell: ' "
                                "s\nif [ \"$s\" = 's' ];then\n    if [ \"$(whoami)\" = "
                                "\"root\" ];then\n        "
                                "echo '\n[ {} ]'\n    fi\n    echo\n    You are ROOT\nfi\n\n",
                                BASHPATH));
            }
        }

        buf.append(fmt::format("\nexit $fm_err\nfi\n"));

        std::ofstream file(task->exec_script);
        if (file.is_open())
        {
            file << buf;
        }
        else
        {
            vfs_file_task_exec_error(task, errno, "Error writing temporary file");

            if (!task->exec_keep_tmp)
            {
                if (std::filesystem::exists(task->exec_script))
                    std::filesystem::remove(task->exec_script);
            }
            call_state_callback(task, VFS_FILE_TASK_FINISH);
            // LOG_INFO("vfs_file_task_exec DONE ERROR");
            return;
        }

        file.close();

        // set permissions
        chmod(task->exec_script.c_str(), 0700);

        // use checksum
        if (geteuid() != 0 && (!task->exec_as_user.empty() || task->exec_checksum))
            sum_script = get_xxhash(task->exec_script.c_str());
    }

    task->percent = 50;

    // Spawn
    std::vector<std::string> argv;

    Glib::Pid pid;
    int out, err;
    std::string use_su;
    bool single_arg = false;
    std::string auth;
    int i;

    if (!terminal.empty())
    {
        // terminal
        argv.push_back(terminal);

        // automatic terminal options
        if (ztd::contains(terminal, "xfce4-terminal") || ztd::contains(terminal, "/terminal"))
            argv.push_back("--disable-server");

        // add option to execute command in terminal
        if (ztd::contains(terminal, "xfce4-terminal") || ztd::contains(terminal, "terminator") ||
            ztd::endswith(terminal, "/terminal")) // xfce
            argv.push_back("-x");
        else if (ztd::contains(terminal, "sakura"))
        {
            argv.push_back("-x");
            single_arg = true;
        }
        else
            /* Note: Some terminals accept multiple arguments to -e, whereas
             * others needs the entire command quoted and passed as a single
             * argument to -e.  SpaceFM uses spacefm-auth to run commands,
             * so only a single argument is ever used as the command. */
            argv.push_back("-e");

        use_su = su;
    }

    if (!task->exec_as_user.empty())
    {
        // su
        argv.push_back(use_su);
        if (!ztd::same(task->exec_as_user, "root"))
        {
            if (!ztd::same(use_su, "/bin/su"))
                argv.push_back("-u");
            argv.push_back(task->exec_as_user);
        }

        if (ztd::same(use_su, "/bin/su"))
        {
            // /bin/su
            argv.push_back("-s");
            argv.push_back(BASHPATH);
            argv.push_back("-c");
            single_arg = true;
        }
    }

    if (sum_script)
    {
        // spacefm-auth exists?
        auth = Glib::find_program_in_path("spacefm-auth");
        if (auth.empty())
        {
            free(sum_script);
            sum_script = nullptr;
            LOG_WARN("spacefm-auth not found in path - this reduces your security");
        }
    }

    if (sum_script && !auth.empty())
    {
        // spacefm-auth
        if (single_arg)
        {
            tmp = fmt::format("{} {} {} {} {}",
                              BASHPATH,
                              auth,
                              ztd::same(task->exec_as_user, "root") ? "root" : "",
                              task->exec_script,
                              sum_script);
            argv.push_back(tmp);
        }
        else
        {
            argv.push_back(BASHPATH);
            argv.push_back(auth);
            if (ztd::same(task->exec_as_user, "root"))
                argv.push_back("root");
            argv.push_back(task->exec_script);
            argv.push_back(sum_script);
        }
        free(sum_script);
    }
    else if (task->exec_direct)
    {
        // add direct args - not currently used
        argv = task->exec_argv;
    }
    else
    {
        argv.push_back(BASHPATH);
        argv.push_back(task->exec_script);
        argv.push_back("run");
    }

    try
    {
        if (task->exec_sync)
        {
            Glib::spawn_async_with_pipes(task->dest_dir,
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
            Glib::spawn_async_with_pipes(task->dest_dir,
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
            std::string errno_msg = Glib::strerror(errno);
            LOG_INFO("    result={} ( {} )", errno, errno_msg);
        }

        if (!task->exec_keep_tmp && task->exec_sync)
        {
            if (std::filesystem::exists(task->exec_script))
                std::filesystem::remove(task->exec_script);
        }
        std::string cmd = ztd::join(argv, " ");
        msg = fmt::format("Error executing '{}'\nGlib Spawn Error Code {}, {}\nRun in a terminal "
                          "for full debug info\n",
                          cmd,
                          e.code(),
                          std::string(Glib::strerror(e.code())));
        vfs_file_task_exec_error(task, errno, msg);
        call_state_callback(task, VFS_FILE_TASK_FINISH);
        // LOG_INFO("vfs_file_task_exec DONE ERROR");
        return;
    }

    print_task_command_spawn(argv, pid);

    if (!task->exec_sync)
    {
        // catch termination to waitpid and delete tmp if needed
        // task can be destroyed while this watch is still active
        g_child_watch_add(pid,
                          (GChildWatchFunc)cb_exec_child_cleanup,
                          !task->exec_keep_tmp && !task->exec_direct && task->exec_script.c_str()
                              ? ztd::strdup(task->exec_script)
                              : nullptr);
        call_state_callback(task, VFS_FILE_TASK_FINISH);
        return;
    }

    task->exec_pid = pid;

    // catch termination (always is run in the main loop thread)
    task->child_watch = g_child_watch_add(pid, (GChildWatchFunc)cb_exec_child_watch, task);

    // create channels for output
    fcntl(out, F_SETFL, O_NONBLOCK);
    fcntl(err, F_SETFL, O_NONBLOCK);
    task->exec_channel_out = g_io_channel_unix_new(out);
    task->exec_channel_err = g_io_channel_unix_new(err);
    g_io_channel_set_close_on_unref(task->exec_channel_out, true);
    g_io_channel_set_close_on_unref(task->exec_channel_err, true);

    // Add watches to channels
    // These are run in the main loop thread so use G_PRIORITY_LOW to not
    // interfere with g_idle_add in vfs-dir/vfs-async-task etc
    // "Use this for very low priority background tasks. It is not used within
    // GLib or GTK+."
    g_io_add_watch_full(task->exec_channel_out,
                        G_PRIORITY_LOW,
                        GIOCondition(G_IO_IN | G_IO_HUP | G_IO_NVAL | G_IO_ERR), // want ERR?
                        (GIOFunc)cb_exec_out_watch,
                        task,
                        nullptr);
    g_io_add_watch_full(task->exec_channel_err,
                        G_PRIORITY_LOW,
                        GIOCondition(G_IO_IN | G_IO_HUP | G_IO_NVAL | G_IO_ERR), // want ERR?
                        (GIOFunc)cb_exec_out_watch,
                        task,
                        nullptr);

    // running
    task->state = VFS_FILE_TASK_RUNNING;

    // LOG_INFO("vfs_file_task_exec DONE");
    return; // exit thread
}

static bool
on_size_timeout(VFSFileTask* task)
{
    if (!task->abort)
        task->state = VFS_FILE_TASK_SIZE_TIMEOUT;
    return false;
}

static void*
vfs_file_task_thread(VFSFileTask* task)
{
    GList* l;
    struct stat file_stat;
    unsigned int size_timeout = 0;
    dev_t dest_dev = 0;
    off_t size;
    GFunc funcs[] = {(GFunc)vfs_file_task_move,
                     (GFunc)vfs_file_task_copy,
                     (GFunc)vfs_file_task_trash,
                     (GFunc)vfs_file_task_delete,
                     (GFunc)vfs_file_task_link,
                     (GFunc)vfs_file_task_chown_chmod,
                     (GFunc)vfs_file_task_exec};
    if (task->type < VFS_FILE_TASK_MOVE || task->type >= VFS_FILE_TASK_LAST)
        goto _exit_thread;

    vfs_file_task_lock(task);
    task->state = VFS_FILE_TASK_RUNNING;
    task->current_file = task->src_paths ? (char*)task->src_paths->data : nullptr;
    task->total_size = 0;
    vfs_file_task_unlock(task);

    if (task->abort)
        goto _exit_thread;

    /* Calculate total size of all files */
    if (task->recursive)
    {
        // start timer to limit the amount of time to spend on this - can be
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds(5, (GSourceFunc)on_size_timeout, task);
        for (l = task->src_paths; l; l = l->next)
        {
            if (lstat((char*)l->data, &file_stat) == -1)
            {
                // don't report error here since its reported later
                // vfs_file_task_error( task, errno, "Accessing", (char*)l->data );
            }
            else
            {
                size = 0;
                get_total_size_of_dir(task, (char*)l->data, &size, &file_stat);
                vfs_file_task_lock(task);
                task->total_size += size;
                vfs_file_task_unlock(task);
            }
            if (task->abort)
                goto _exit_thread;
            if (task->state == VFS_FILE_TASK_SIZE_TIMEOUT)
                break;
        }
    }
    else if (task->type == VFS_FILE_TASK_TRASH)
    {
    }
    else if (task->type != VFS_FILE_TASK_EXEC)
    {
        // start timer to limit the amount of time to spend on this - can be
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds(5, (GSourceFunc)on_size_timeout, task);
        if (task->type != VFS_FILE_TASK_CHMOD_CHOWN)
        {
            if (!(!task->dest_dir.empty() && stat(task->dest_dir.c_str(), &file_stat) == 0))
            {
                vfs_file_task_error(task, errno, "Accessing", task->dest_dir.c_str());
                task->abort = true;
                goto _exit_thread;
            }
            dest_dev = file_stat.st_dev;
        }

        for (l = task->src_paths; l; l = l->next)
        {
            if (lstat((char*)l->data, &file_stat) == -1)
            {
                // don't report error here since it's reported later
                // vfs_file_task_error( task, errno, "Accessing", ( char* ) l->data );
            }
            else
            {
                if ((task->type == VFS_FILE_TASK_MOVE) && file_stat.st_dev != dest_dev)
                {
                    // recursive size
                    size = 0;
                    get_total_size_of_dir(task, (char*)l->data, &size, &file_stat);
                    vfs_file_task_lock(task);
                    task->total_size += size;
                    vfs_file_task_unlock(task);
                }
                else
                {
                    vfs_file_task_lock(task);
                    task->total_size += file_stat.st_size;
                    vfs_file_task_unlock(task);
                }
            }
            if (task->abort)
                goto _exit_thread;
            if (task->state == VFS_FILE_TASK_SIZE_TIMEOUT)
                break;
        }
    }

    if (!task->dest_dir.empty() && stat(task->dest_dir.c_str(), &file_stat) != -1)
        add_task_dev(task, file_stat.st_dev);

    if (task->abort)
        goto _exit_thread;

    // cancel the timer
    if (size_timeout)
        g_source_remove_by_user_data(task);

    if (task->state_pause == VFS_FILE_TASK_QUEUE)
    {
        if (task->state != VFS_FILE_TASK_SIZE_TIMEOUT && xset_get_b("task_q_smart"))
        {
            // make queue exception for smaller tasks
            off_t exlimit;
            switch (task->type)
            {
                case VFS_FILE_TASK_TRASH:
                case VFS_FILE_TASK_MOVE:
                case VFS_FILE_TASK_COPY:
                    exlimit = 10485760; // 10M
                    break;
                case VFS_FILE_TASK_DELETE:
                    exlimit = 5368709120; // 5G
                    break;
                default:
                    exlimit = 0; // always exception for other types
                    break;
            }

            if (!exlimit || task->total_size < exlimit)
                task->state_pause = VFS_FILE_TASK_RUNNING;
        }
        // device list is populated so signal queue start
        task->queue_start = true;
    }

    if (task->state == VFS_FILE_TASK_SIZE_TIMEOUT)
    {
        append_add_log(task, "Timed out calculating total size\n");
        task->total_size = 0;
    }
    task->state = VFS_FILE_TASK_RUNNING;
    if (should_abort(task))
        goto _exit_thread;

    g_list_foreach(task->src_paths, funcs[task->type], task);

_exit_thread:
    task->state = VFS_FILE_TASK_RUNNING;
    if (size_timeout)
        g_source_remove_by_user_data(task);
    if (task->state_cb)
    {
        call_state_callback(task, VFS_FILE_TASK_FINISH);
    }
    return nullptr;
}

/*
 * source_files sould be a newly allocated list, and it will be
 * freed after file operation has been completed
 */
VFSFileTask*
vfs_task_new(VFSFileTaskType type, GList* src_files, const char* dest_dir)
{
    VFSFileTask* task = g_slice_new0(VFSFileTask);

    task->type = type;
    task->src_paths = src_files;
    if (dest_dir)
        task->dest_dir = dest_dir;
    // task->current_file = nullptr;
    // task->current_dest = nullptr;

    task->recursive = (task->type == VFS_FILE_TASK_COPY || task->type == VFS_FILE_TASK_DELETE);

    task->err_count = 0;
    task->abort = false;
    task->error_first = true;
    task->custom_percent = false;

    task->exec_type = VFS_EXEC_NORMAL;
    // task->exec_action = nullptr;
    // task->exec_command = std::string();
    task->exec_sync = true;
    task->exec_popup = false;
    task->exec_show_output = false;
    task->exec_show_error = false;
    task->exec_terminal = false;
    task->exec_keep_terminal = false;
    task->exec_export = false;
    task->exec_direct = false;
    // task->exec_as_user = nullptr;
    // task->exec_icon = nullptr;
    // task->exec_script = nullptr;
    task->exec_keep_tmp = false;
    task->exec_browser = nullptr;
    task->exec_desktop = nullptr;
    task->exec_pid = 0;
    task->child_watch = 0;
    task->exec_is_error = false;
    task->exec_scroll_lock = false;
    task->exec_write_root = false;
    task->exec_checksum = false;
    task->exec_set = nullptr;
    task->exec_cond = nullptr;
    task->exec_ptask = nullptr;

    task->pause_cond = nullptr;
    task->state_pause = VFS_FILE_TASK_RUNNING;
    task->queue_start = false;
    task->devs = nullptr;

    vfs_file_task_init(task);

    GtkTextIter iter;
    task->add_log_buf = gtk_text_buffer_new(nullptr);
    task->add_log_end = gtk_text_mark_new(nullptr, false);
    gtk_text_buffer_get_end_iter(task->add_log_buf, &iter);
    gtk_text_buffer_add_mark(task->add_log_buf, task->add_log_end, &iter);

    task->start_time = std::time(nullptr);
    task->last_speed = 0;
    task->last_progress = 0;
    task->current_item = 0;
    task->timer = g_timer_new();
    task->last_elapsed = 0;
    return task;
}

/* Set some actions for chmod, this array will be copied
 * and stored in VFSFileTask */
void
vfs_file_task_set_chmod(VFSFileTask* task, unsigned char* chmod_actions)
{
    task->chmod_actions = (unsigned char*)g_slice_alloc(sizeof(unsigned char) * N_CHMOD_ACTIONS);
    memcpy((void*)task->chmod_actions,
           (void*)chmod_actions,
           sizeof(unsigned char) * N_CHMOD_ACTIONS);
}

void
vfs_file_task_set_chown(VFSFileTask* task, uid_t uid, gid_t gid)
{
    task->uid = uid;
    task->gid = gid;
}

void
vfs_file_task_run(VFSFileTask* task)
{
    if (task->type != VFS_FILE_TASK_EXEC)
    {
        if (task->type == VFS_FILE_TASK_CHMOD_CHOWN && task->src_paths->data)
        {
            char* dir = g_path_get_dirname((char*)task->src_paths->data);
            task->avoid_changes = vfs_volume_dir_avoid_changes(dir);
            free(dir);
        }
        else
            task->avoid_changes = vfs_volume_dir_avoid_changes(task->dest_dir.c_str());

        task->thread = g_thread_new("task_run", (GThreadFunc)vfs_file_task_thread, task);
    }
    else
    {
        // don't use another thread for exec since gio adds watches to main
        // loop thread anyway
        task->thread = nullptr;
        vfs_file_task_exec((char*)task->src_paths->data, task);
    }
}

void
vfs_file_task_try_abort(VFSFileTask* task)
{
    task->abort = true;
    if (task->pause_cond)
    {
        vfs_file_task_lock(task);
        g_cond_broadcast(task->pause_cond);
        task->last_elapsed = g_timer_elapsed(task->timer, nullptr);
        task->last_progress = task->progress;
        task->last_speed = 0;
        vfs_file_task_unlock(task);
    }
    else
    {
        vfs_file_task_lock(task);
        task->last_elapsed = g_timer_elapsed(task->timer, nullptr);
        task->last_progress = task->progress;
        task->last_speed = 0;
        vfs_file_task_unlock(task);
    }
    task->state_pause = VFS_FILE_TASK_RUNNING;
}

void
vfs_file_task_abort(VFSFileTask* task)
{
    task->abort = true;
    /* Called from another thread */
    if (task->thread && g_thread_self() != task->thread && task->type != VFS_FILE_TASK_EXEC)
    {
        g_thread_join(task->thread);
        task->thread = nullptr;
    }
}

void
vfs_file_task_free(VFSFileTask* task)
{
    if (task->src_paths)
    {
        g_list_foreach(task->src_paths, (GFunc)g_free, nullptr);
        g_list_free(task->src_paths);
    }
    g_slist_free(task->devs);

    if (task->chmod_actions)
        g_slice_free1(sizeof(unsigned char) * N_CHMOD_ACTIONS, task->chmod_actions);

    vfs_file_task_clear(task);

    gtk_text_buffer_set_text(task->add_log_buf, "", -1);
    g_object_unref(task->add_log_buf);

    g_timer_destroy(task->timer);

    g_slice_free(VFSFileTask, task);
}

static void
add_task_dev(VFSFileTask* task, dev_t dev)
{
    dev_t parent = 0;
    if (!g_slist_find(task->devs, GUINT_TO_POINTER(dev)))
    {
        parent = get_device_parent(dev);
        // LOG_INFO("add_task_dev {}:{}", major(dev), minor(dev));
        vfs_file_task_lock(task);
        task->devs = g_slist_append(task->devs, GUINT_TO_POINTER(dev));
        if (parent && !g_slist_find(task->devs, GUINT_TO_POINTER(parent)))
        {
            // LOG_INFO("add_task_dev PARENT {}:{}", major(parent), minor(parent));
            task->devs = g_slist_append(task->devs, GUINT_TO_POINTER(parent));
        }
        vfs_file_task_unlock(task);
    }
}

/*
 * void get_total_size_of_dir( const char* path, off_t* size )
 * Recursively count total size of all files in the specified directory.
 * If the path specified is a file, the size of the file is directly returned.
 * cancel is used to cancel the operation. This function will check the value
 * pointed by cancel in every iteration. If cancel is set to true, the
 * calculation is cancelled.
 * NOTE: *size should be set to zero before calling this function.
 */
static void
get_total_size_of_dir(VFSFileTask* task, const char* path, off_t* size, struct stat* have_stat)
{
    if (task->abort)
        return;

    struct stat file_stat;

    if (have_stat)
        file_stat = *have_stat;
    else if (lstat(path, &file_stat) == -1)
        return;

    *size += file_stat.st_size;

    // remember device for smart queue
    if (!task->devs)
        add_task_dev(task, file_stat.st_dev);
    else if ((unsigned int)file_stat.st_dev != GPOINTER_TO_UINT(task->devs->data))
        add_task_dev(task, file_stat.st_dev);

    // Don't follow symlinks
    if (S_ISLNK(file_stat.st_mode) || !S_ISDIR(file_stat.st_mode))
        return;

    std::string file_name;
    for (const auto& file: std::filesystem::directory_iterator(path))
    {
        file_name = std::filesystem::path(file).filename();
        if (task->state == VFS_FILE_TASK_SIZE_TIMEOUT || task->abort)
            break;
        std::string full_path = Glib::build_filename(path, file_name);
        if (lstat(full_path.c_str(), &file_stat) != -1)
        {
            if (S_ISDIR(file_stat.st_mode))
                get_total_size_of_dir(task, full_path.c_str(), size, &file_stat);
            else
                *size += file_stat.st_size;
        }
    }
}

void
vfs_file_task_set_recursive(VFSFileTask* task, bool recursive)
{
    task->recursive = recursive;
}

void
vfs_file_task_set_overwrite_mode(VFSFileTask* task, VFSFileTaskOverwriteMode mode)
{
    task->overwrite_mode = mode;
}

void
vfs_file_task_set_state_callback(VFSFileTask* task, VFSFileTaskStateCallback cb, void* user_data)
{
    task->state_cb = cb;
    task->state_cb_data = user_data;
}

static void
vfs_file_task_error(VFSFileTask* task, int errnox, const char* action, const char* target)
{
    task->error = errnox;
    std::string errno_msg = Glib::strerror(errnox);
    std::string msg = fmt::format("\n{} {}\nError: {}\n", action, target, errno_msg);
    append_add_log(task, msg);
    call_state_callback(task, VFS_FILE_TASK_ERROR);
}
