/*
 *  C Implementation: vfs-file-task
 *
 * Description: modified and redesigned for SpaceFM
 *
 *
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#include <string>
#include <filesystem>

#include <ctime>

#include <fcntl.h>
#include <utime.h>

#include "main-window.hxx"
#include "vfs/vfs-volume.hxx"

#include "utils.hxx"

#include "vfs/vfs-file-task.hxx"
#include "vfs/vfs-file-trash.hxx"

#include "logger.hxx"

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
static void vfs_file_task_exec_error(VFSFileTask* task, int errnox, char* action);
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
    g_free(task->mutex);
}

static void
append_add_log(VFSFileTask* task, const char* msg, int msg_len)
{
    vfs_file_task_lock(task);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(task->add_log_buf, &iter, task->add_log_end);
    gtk_text_buffer_insert(task->add_log_buf, &iter, msg, msg_len);
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
    g_free(new_name);
    unsigned int n = 1;
    while (n && lstat(new_dest_file, &dest_stat) == 0)
    {
        g_free(new_dest_file);
        new_name = g_strdup_printf("%s-%s%d%s%s",
                                   base_name,
                                   "copy",
                                   ++n,
                                   ext && ext[0] ? "." : "",
                                   ext ? ext : "");
        new_dest_file = g_build_filename(dest_dir, new_name, nullptr);
        g_free(new_name);
    }
    if (n == 0)
    {
        g_free(new_dest_file);
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
            if (!g_strcmp0(task->current_file, task->current_dest))
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
            char* ext;
            char* old_name = g_path_get_basename(dest_file);
            char* dest_dir = g_path_get_dirname(dest_file);
            char* base_name = get_name_extension(old_name, S_ISDIR(dest_stat.st_mode), &ext);
            g_free(old_name);
            *new_dest_file = vfs_file_task_get_unique_name(dest_dir, base_name, ext);
            *dest_exists = false;
            g_free(dest_dir);
            g_free(base_name);
            g_free(ext);
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
                g_free(new_dest);
                return false;
            }

            if (task->overwrite_mode != VFS_FILE_TASK_RENAME)
            {
                g_free(new_dest);
                new_dest = nullptr;
                switch (task->overwrite_mode)
                {
                    case VFS_FILE_TASK_OVERWRITE:
                    case VFS_FILE_TASK_OVERWRITE_ALL:
                        *dest_exists = !lstat(dest_file, &dest_stat);
                        if (!g_strcmp0(task->current_file, task->current_dest))
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

    if (!(task->dest_dir && realpath(task->dest_dir, real_dest_path)))
        return false;
    if (realpath(src_dir, real_src_path) && g_str_has_prefix(real_dest_path, real_src_path) &&
        (len = strlen(real_src_path)) &&
        (real_dest_path[len] == '/' || real_dest_path[len] == '\0'))
    {
        // source is contained in destination dir
        char* disp_src = g_filename_display_name(src_dir);
        char* disp_dest = g_filename_display_name(task->dest_dir);
        char* err =
            g_strdup_printf("Destination directory \"%1$s\" is contained in source \"%2$s\"",
                            disp_dest,
                            disp_src);
        append_add_log(task, err, -1);
        g_free(err);
        g_free(disp_src);
        g_free(disp_dest);
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
    g_free(dir_path);
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
    const char* file_name;
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
    string_copy_free(&task->current_file, src_file);
    string_copy_free(&task->current_dest, dest_file);
    task->current_item++;
    vfs_file_task_unlock(task);

    if (lstat(src_file, &file_stat) == -1)
    {
        vfs_file_task_error(task, errno, "Accessing", src_file);
        return false;
    }

    int result = 0;
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
            string_copy_free(&task->current_dest, dest_file);
            vfs_file_task_unlock(task);
        }

        if (!dest_exists)
            result = mkdir(dest_file, file_stat.st_mode | 0700);

        if (result == 0)
        {
            struct utimbuf times;
            vfs_file_task_lock(task);
            task->progress += file_stat.st_size;
            vfs_file_task_unlock(task);

            GError* error = nullptr;
            GDir* dir = g_dir_open(src_file, 0, &error);
            if (dir)
            {
                while ((file_name = g_dir_read_name(dir)))
                {
                    if (should_abort(task))
                        break;
                    char* sub_src_file = g_build_filename(src_file, file_name, nullptr);
                    char* sub_dest_file = g_build_filename(dest_file, file_name, nullptr);
                    if (!vfs_file_task_do_copy(task, sub_src_file, sub_dest_file) && !copy_fail)
                        copy_fail = true;
                    g_free(sub_dest_file);
                    g_free(sub_src_file);
                }
                g_dir_close(dir);
            }
            else if (error)
            {
                char* msg = g_strdup_printf("\n%s\n", error->message);
                g_error_free(error);
                vfs_file_task_exec_error(task, 0, msg);
                g_free(msg);
                copy_fail = true;
                if (should_abort(task))
                    goto _return_;
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
                if ((result = rmdir(src_file)))
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
                string_copy_free(&task->current_dest, dest_file);
                vfs_file_task_unlock(task);
            }

            // MOD delete it first to prevent exists error
            if (dest_exists)
            {
                result = unlink(dest_file);
                if (result && errno != 2 /* no such file */)
                {
                    vfs_file_task_error(task, errno, "Removing", dest_file);
                    goto _return_;
                }
            }

            if ((wfd = symlink(buffer, dest_file)) == 0)
            {
                /* Move files to different device: Need to delete source files */
                if ((task->type == VFS_FILE_TASK_MOVE) && !copy_fail)
                {
                    result = unlink(src_file);
                    if (result)
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
                string_copy_free(&task->current_dest, dest_file);
                vfs_file_task_unlock(task);
            }

            // MOD if dest is a symlink, delete it first to prevent overwriting target!
            if (std::filesystem::is_symlink(dest_file))
            {
                result = unlink(dest_file);
                if (result)
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
                    result = unlink(dest_file);
                    if (result && errno != 2 /* no such file */)
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
                        result = unlink(src_file);
                        if (result)
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
        g_free(new_dest_file);
    if (!copy_fail && task->error_first)
        task->error_first = false;
    return !copy_fail;
_return_:

    if (new_dest_file)
        g_free(new_dest_file);
    return false;
}

static void
vfs_file_task_copy(char* src_file, VFSFileTask* task)
{
    char* file_name;
    char* dest_file;

    file_name = g_path_get_basename(src_file);
    dest_file = g_build_filename(task->dest_dir, file_name, nullptr);
    g_free(file_name);
    vfs_file_task_do_copy(task, src_file, dest_file);
    g_free(dest_file);
}

static int
vfs_file_task_do_move(VFSFileTask* task, const char* src_file,
                      const char* dest_file) // MOD void to int
{
    if (should_abort(task))
        return 0;

    vfs_file_task_lock(task);
    string_copy_free(&task->current_file, src_file);
    string_copy_free(&task->current_dest, dest_file);
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
        string_copy_free(&task->current_dest, dest_file);
        vfs_file_task_unlock(task);
    }

    if (S_ISDIR(file_stat.st_mode) && std::filesystem::is_directory(dest_file))
    {
        // moving a directory onto a directory that exists
        GError* error = nullptr;
        GDir* dir = g_dir_open(src_file, 0, &error);
        if (dir)
        {
            const char* file_name;
            char* sub_src_file;
            char* sub_dest_file;
            while ((file_name = g_dir_read_name(dir)))
            {
                if (should_abort(task))
                    break;
                sub_src_file = g_build_filename(src_file, file_name, nullptr);
                sub_dest_file = g_build_filename(dest_file, file_name, nullptr);
                vfs_file_task_do_move(task, sub_src_file, sub_dest_file);
                g_free(sub_dest_file);
                g_free(sub_src_file);
            }
            g_dir_close(dir);
            // remove moved src dir if empty
            if (!should_abort(task))
                rmdir(src_file);
        }
        else if (error)
        {
            char* msg = g_strdup_printf("\n%s\n", error->message);
            g_error_free(error);
            vfs_file_task_exec_error(task, 0, msg);
            g_free(msg);
        }
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
            g_free(new_dest_file);
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
        g_free(new_dest_file);
    return 0;
}

static void
vfs_file_task_move(char* src_file, VFSFileTask* task)
{
    if (should_abort(task))
        return;

    vfs_file_task_lock(task);
    string_copy_free(&task->current_file, src_file);
    vfs_file_task_unlock(task);

    char* file_name = g_path_get_basename(src_file);

    char* dest_file = g_build_filename(task->dest_dir, file_name, nullptr);

    g_free(file_name);

    struct stat src_stat;
    struct stat dest_stat;
    if (lstat(src_file, &src_stat) == 0 && stat(task->dest_dir, &dest_stat) == 0)
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
    string_copy_free(&task->current_file, src_file);
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
vfs_file_task_delete(char* src_file, VFSFileTask* task)
{
    if (should_abort(task))
        return;

    vfs_file_task_lock(task);
    string_copy_free(&task->current_file, src_file);
    task->current_item++;
    vfs_file_task_unlock(task);

    struct stat file_stat;
    if (lstat(src_file, &file_stat) == -1)
    {
        vfs_file_task_error(task, errno, "Accessing", src_file);
        return;
    }

    int result;
    if (S_ISDIR(file_stat.st_mode))
    {
        GError* error = nullptr;
        GDir* dir = g_dir_open(src_file, 0, &error);
        if (dir)
        {
            const char* file_name;
            while ((file_name = g_dir_read_name(dir)))
            {
                if (should_abort(task))
                    break;
                char* sub_src_file = g_build_filename(src_file, file_name, nullptr);
                vfs_file_task_delete(sub_src_file, task);
                g_free(sub_src_file);
            }
            g_dir_close(dir);
        }
        else if (error)
        {
            char* msg = g_strdup_printf("\n%s\n", error->message);
            g_error_free(error);
            vfs_file_task_exec_error(task, 0, msg);
            g_free(msg);
        }

        if (should_abort(task))
            return;
        result = rmdir(src_file);
        if (result != 0)
        {
            vfs_file_task_error(task, errno, "Removing", src_file);
            return;
        }
    }
    else
    {
        result = unlink(src_file);
        if (result != 0)
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
    char* old_dest_file = g_build_filename(task->dest_dir, file_name, nullptr);
    g_free(file_name);
    char* dest_file = old_dest_file;

    // MOD  setup task for check overwrite
    if (should_abort(task))
        return;
    vfs_file_task_lock(task);
    string_copy_free(&task->current_file, src_file);
    string_copy_free(&task->current_dest, old_dest_file);
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
        string_copy_free(&task->current_dest, dest_file);
        vfs_file_task_unlock(task);
    }

    // MOD if dest exists, delete it first to prevent exists error
    int result;
    if (dest_exists)
    {
        result = unlink(dest_file);
        if (result)
        {
            vfs_file_task_error(task, errno, "Removing", dest_file);
            return;
        }
    }

    result = symlink(src_file, dest_file);
    if (result)
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
        g_free(new_dest_file);
    g_free(old_dest_file);
}

static void
vfs_file_task_chown_chmod(char* src_file, VFSFileTask* task)
{
    if (should_abort(task))
        return;

    vfs_file_task_lock(task);
    string_copy_free(&task->current_file, src_file);
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
            GError* error = nullptr;
            GDir* dir = g_dir_open(src_file, 0, &error);
            if (dir)
            {
                const char* file_name;
                while ((file_name = g_dir_read_name(dir)))
                {
                    if (should_abort(task))
                        break;
                    char* sub_src_file = g_build_filename(src_file, file_name, nullptr);
                    vfs_file_task_chown_chmod(sub_src_file, task);
                    g_free(sub_src_file);
                }
                g_dir_close(dir);
            }
            else if (error)
            {
                char* msg = g_strdup_printf("\n%s\n", error->message);
                g_error_free(error);
                vfs_file_task_exec_error(task, 0, msg);
                g_free(msg);
                if (should_abort(task))
                    return;
            }
            else
            {
                vfs_file_task_error(task, errno, "Accessing", src_file);
                if (should_abort(task))
                    return;
            }
        }
    }
    if (task->error_first)
        task->error_first = false;
}

char*
vfs_file_task_get_cpids(GPid pid)
{ // get child pids recursively as multi-line string
    if (!pid)
        return nullptr;

    char* stdout = nullptr;

    std::string command = fmt::format("/bin/ps h --ppid {} -o pid", pid);
    print_command(command);
    bool ret = g_spawn_command_line_sync(command.c_str(), &stdout, nullptr, nullptr, nullptr);
    if (ret && stdout && stdout[0] != '\0' && strchr(stdout, '\n'))
    {
        char* cpids = g_strdup(stdout);
        // get grand cpids recursively
        char* pids = stdout;
        char* nl;
        while ((nl = strchr(pids, '\n')))
        {
            nl[0] = '\0';
            char* pida = g_strdup(pids);
            nl[0] = '\n';
            pids = nl + 1;
            GPid pidi = strtol(pida, nullptr, 10);
            g_free(pida);
            if (pidi)
            {
                char* gcpids;
                if ((gcpids = vfs_file_task_get_cpids(pidi)))
                {
                    char* old_cpids = cpids;
                    cpids = g_strdup_printf("%s%s", old_cpids, gcpids);
                    g_free(old_cpids);
                    g_free(gcpids);
                }
            }
        }
        g_free(stdout);
        // LOG_INFO("vfs_file_task_get_cpids {}[{}]", pid, cpids );
        return cpids;
    }
    if (stdout)
        g_free(stdout);
    return nullptr;
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
        char* pida = g_strdup(pids);
        nl[0] = '\n';
        pids = nl + 1;
        GPid pidi = strtol(pida, nullptr, 10);
        g_free(pida);
        if (pidi)
        {
            // LOG_INFO("KILL_CPID pidi={} signal={}", pidi, signal );
            kill(pidi, signal);
        }
    }
}

static void
cb_exec_child_cleanup(GPid pid, int status, char* tmp_file)
{ // delete tmp files after async task terminates
    // LOG_INFO("cb_exec_child_cleanup pid={} status={} file={}", pid, status, tmp_file );
    g_spawn_close_pid(pid);
    if (tmp_file)
    {
        unlink(tmp_file);
        g_free(tmp_file);
    }
    LOG_INFO("async child finished  pid={} status={}", pid, status);
}

static void
cb_exec_child_watch(GPid pid, int status, VFSFileTask* task)
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
        if (task->exec_script)
            unlink(task->exec_script);
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

    unsigned long size;

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
    char buf[2048];
    if (g_io_channel_read_chars(channel, buf, sizeof(buf), &size, nullptr) == G_IO_STATUS_NORMAL &&
        size > 0)
        append_add_log(task, buf, size);
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
get_xxhash(char* path)
{
    const char* xxhash = g_find_program_in_path("/usr/bin/xxh128sum");
    if (!xxhash)
    {
        LOG_WARN("Missing program xxhash");
        return nullptr;
    }

    char* stdout;
    char* sum = nullptr;
    std::string command = fmt::format("{} {}", xxhash, path);
    print_command(command);
    if (g_spawn_command_line_sync(command.c_str(), &stdout, nullptr, nullptr, nullptr))
    {
        sum = g_strndup(stdout, 64);
        g_free(stdout);
        if (strlen(sum) != 64)
        {
            g_free(sum);
            sum = nullptr;
        }
    }
    return sum;
}

static void
vfs_file_task_exec_error(VFSFileTask* task, int errnox, char* action)
{
    char* msg;

    if (errnox)
        msg = g_strdup_printf("%s\n%s\n", action, g_strerror(errnox));
    else
        msg = g_strdup_printf("%s\n", action);

    append_add_log(task, msg, -1);
    g_free(msg);
    call_state_callback(task, VFS_FILE_TASK_ERROR);
}

static void
vfs_file_task_exec(char* src_file, VFSFileTask* task)
{
    // this function is now thread safe but is not currently run in
    // another thread because gio adds watches to main loop thread anyway
    char* su = nullptr;
    char* str;
    char* hexname;
    int result;
    char* terminal = nullptr;
    char** terminalv = nullptr;
    char* sum_script = nullptr;
    GtkWidget* parent = nullptr;
    char buf[PATH_MAX + 1];

    // LOG_INFO("vfs_file_task_exec");
    // task->exec_keep_tmp = true;

    vfs_file_task_lock(task);
    const char* value = task->current_dest; // variable value temp storage
    task->current_dest = nullptr;

    if (task->exec_browser)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(task->exec_browser));
    else if (task->exec_desktop)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(task->exec_desktop));

    task->state = VFS_FILE_TASK_RUNNING;
    string_copy_free(&task->current_file, src_file);
    task->total_size = 0;
    task->percent = 0;
    vfs_file_task_unlock(task);

    if (should_abort(task))
        return;

    // need su?
    if (task->exec_as_user)
    {
        if (geteuid() == 0 && !strcmp(task->exec_as_user, "root"))
        {
            // already root so no su
            g_free(task->exec_as_user);
            task->exec_as_user = nullptr;
        }
        else
        {
            // get su programs
            su = get_valid_su();
            if (!su)
            {
                str = g_strdup(
                    "Please configure a valid Terminal SU command in View|Preferences|Advanced");
                LOG_WARN("{}", str);
                // do not use xset_msg_dialog if non-main thread
                // vfs_file_task_exec_error( task, 0, str );
                xset_msg_dialog(parent,
                                GTK_MESSAGE_ERROR,
                                "Terminal SU Not Available",
                                0,
                                str,
                                nullptr);
                goto _exit_with_error_lean;
            }
        }
    }

    // make tmpdir
    const char* tmp;
    tmp = xset_get_user_tmp_dir();

    if (!tmp || !std::filesystem::is_directory(tmp))
    {
        str = g_strdup("Cannot create temporary directory");
        LOG_WARN("{}", str);
        // do not use xset_msg_dialog if non-main thread
        // vfs_file_task_exec_error( task, 0, str );
        xset_msg_dialog(parent, GTK_MESSAGE_ERROR, "Error", 0, str, nullptr);
        goto _exit_with_error_lean;
    }

    // get terminal
    if (!task->exec_terminal && task->exec_as_user)
    {
        // using cli tool so run in terminal
        task->exec_terminal = true;
    }
    if (task->exec_terminal)
    {
        // get terminal
        str = g_strdup(xset_get_s("main_terminal"));
        g_strstrip(str);
        terminalv = g_strsplit(str, " ", 0);
        g_free(str);
        if (terminalv && terminalv[0] && terminalv[0][0] != '\0')
            terminal = g_find_program_in_path(terminalv[0]);
        if (!(terminal && terminal[0] == '/'))
        {
            str = g_strdup("Please set a valid terminal program in View|Preferences|Advanced");
            LOG_WARN("{}", str);
            // do not use xset_msg_dialog if non-main thread
            // vfs_file_task_exec_error( task, 0, str );
            xset_msg_dialog(parent, GTK_MESSAGE_ERROR, "Terminal Not Available", 0, str, nullptr);
            goto _exit_with_error_lean;
        }
        // resolve x-terminal-emulator link (may be recursive link)
        else if (strstr(terminal, "x-terminal-emulator") && realpath(terminal, buf) != nullptr)
        {
            g_free(terminal);
            g_free(terminalv[0]);
            terminal = g_strdup(buf);
            terminalv[0] = g_strdup(buf);
        }
    }

    // Build exec script
    if (!task->exec_direct)
    {
        // get script name
        do
        {
            if (task->exec_script)
                g_free(task->exec_script);
            hexname = g_strdup_printf("%s-tmp.sh", randhex8());
            task->exec_script = g_build_filename(tmp, hexname, nullptr);
            g_free(hexname);
        } while (std::filesystem::exists(task->exec_script));

        // open buffer
        GString* buf = g_string_sized_new(524288); // 500K

        // build - header
        g_string_append_printf(buf, "#!%s\n%s\n#tmp exec script\n", BASHPATH, SHELL_SETTINGS);

        // build - exports
        if (task->exec_export && (task->exec_browser || task->exec_desktop))
        {
            if (task->exec_browser)
                main_write_exports(task, value, buf);
            else
                goto _exit_with_error;
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
        g_string_append_printf(buf, "#run\nif [ \"$1\" == \"run\" ];then\n\n");

        // build - write root settings
        if (task->exec_write_root && geteuid() != 0)
        {
            const char* this_user = g_get_user_name();
            if (this_user && this_user[0] != '\0')
            {
                char* root_set_path =
                    g_strdup_printf("%s/spacefm/%s-as-root", SYSCONFDIR, this_user);
                write_root_settings(buf, root_set_path);
                g_free(root_set_path);
            }
            else
            {
                char* root_set_path =
                    g_strdup_printf("%s/spacefm/%d-as-root", SYSCONFDIR, geteuid());
                write_root_settings(buf, root_set_path);
                g_free(root_set_path);
            }
        }

        // build - export vars
        if (task->exec_export)
            g_string_append_printf(buf, "export fm_import=\"source %s\"\n", task->exec_script);
        else
            g_string_append_printf(buf, "export fm_import=\"\"\n");

        g_string_append_printf(buf, "export fm_source=\"%s\"\n\n", task->exec_script);

        // build - trap rm
        if (!task->exec_keep_tmp && geteuid() != 0 && task->exec_as_user &&
            !strcmp(task->exec_as_user, "root"))
        {
            // run as root command, clean up
            g_string_append_printf(buf,
                                   "trap \"rm -f %s; exit\" EXIT SIint SIGTERM SIGQUIT SIGHUP\n\n",
                                   task->exec_script);
        }

        // build - command
        print_task_command((char*)task->exec_ptask, task->exec_command);

        g_string_append_printf(buf, "%s\nfm_err=$?\n", task->exec_command);

        // build - press enter to close
        if (terminal && task->exec_keep_terminal)
        {
            if (geteuid() == 0 || (task->exec_as_user && !strcmp(task->exec_as_user, "root")))
                g_string_append_printf(buf,
                                       "\necho;read -p '[ Finished ]  Press Enter to close: '\n");
            else
            {
                g_string_append_printf(buf,
                                       "\necho;read -p '[ Finished ]  Press Enter to close or s + "
                                       "Enter for a shell: ' "
                                       "s\nif [ \"$s\" = 's' ];then\n    if [ \"$(whoami)\" = "
                                       "\"root\" ];then\n        "
                                       "echo '\n[ %s ]'\n    fi\n    echo\n    %s\nfi\n\n",
                                       "You are ROOT",
                                       BASHPATH);
            }
        }

        g_string_append_printf(buf, "\nexit $fm_err\nfi\n");

        if (!g_file_set_contents(task->exec_script, buf->str, buf->len, nullptr))
            goto _exit_with_error;
        g_string_free(buf, true);

        // set permissions
        chmod(task->exec_script, 0700);

        // use checksum
        if (geteuid() != 0 && (task->exec_as_user || task->exec_checksum))
            sum_script = get_xxhash(task->exec_script);
    }

    task->percent = 50;

    // Spawn
    GPid pid;
    char* argv[35];
    int out, err;
    int a;
    char* use_su;
    bool single_arg;
    char* auth;
    int i;

    a = 0;
    single_arg = false;
    auth = nullptr;

    if (terminal)
    {
        // terminal
        argv[a++] = terminal;

        // terminal options - add <=9 options
        for (i = 0; terminalv[i]; i++)
        {
            if (i == 0 || a > 9 || terminalv[i][0] == '\0')
                g_free(terminalv[i]);
            else
                argv[a++] = terminalv[i]; // steal
        }
        g_free(terminalv); // all strings freed or stolen
        terminalv = nullptr;

        // automatic terminal options
        if (strstr(terminal, "xfce4-terminal") || g_str_has_suffix(terminal, "/terminal"))
            argv[a++] = g_strdup_printf("--disable-server");

        // add option to execute command in terminal
        if (strstr(terminal, "xfce4-terminal") || strstr(terminal, "terminator") ||
            g_str_has_suffix(terminal, "/terminal")) // xfce
            argv[a++] = g_strdup("-x");
        else if (strstr(terminal, "sakura"))
        {
            argv[a++] = g_strdup("-x");
            single_arg = true;
        }
        else
            /* Note: Some terminals accept multiple arguments to -e, whereas
             * others needs the entire command quoted and passed as a single
             * argument to -e.  SpaceFM uses spacefm-auth to run commands,
             * so only a single argument is ever used as the command. */
            argv[a++] = g_strdup("-e");

        use_su = su;
    }

    if (task->exec_as_user)
    {
        // su
        argv[a++] = g_strdup(use_su);
        if (strcmp(task->exec_as_user, "root"))
        {
            if (strcmp(use_su, "/bin/su"))
                argv[a++] = g_strdup("-u");
            argv[a++] = g_strdup(task->exec_as_user);
        }

        if (!strcmp(use_su, "/bin/su"))
        {
            // /bin/su
            argv[a++] = g_strdup("-s");
            argv[a++] = g_strdup(BASHPATH); // shell spec
            argv[a++] = g_strdup("-c");
            single_arg = true;
        }
    }

    if (sum_script)
    {
        // spacefm-auth exists?
        auth = g_find_program_in_path("spacefm-auth");
        if (!auth)
        {
            g_free(sum_script);
            sum_script = nullptr;
            LOG_WARN("spacefm-auth not found in path - this reduces your security");
        }
    }

    if (sum_script && auth)
    {
        // spacefm-auth
        if (single_arg)
        {
            argv[a++] = g_strdup_printf("%s %s%s %s %s",
                                        BASHPATH,
                                        auth,
                                        !g_strcmp0(task->exec_as_user, "root") ? " root" : "",
                                        task->exec_script,
                                        sum_script);
            g_free(auth);
        }
        else
        {
            argv[a++] = g_strdup(BASHPATH);
            argv[a++] = auth;
            if (!g_strcmp0(task->exec_as_user, "root"))
                argv[a++] = g_strdup("root");
            argv[a++] = g_strdup(task->exec_script);
            argv[a++] = g_strdup(sum_script);
        }
        g_free(sum_script);
    }
    else if (task->exec_direct)
    {
        // add direct args - not currently used
        if (single_arg)
        {
            argv[a++] = g_strjoinv(" ", &task->exec_argv[0]);
            for (i = 0; i < 7; i++)
            {
                if (!task->exec_argv[i])
                    break;
                g_free(task->exec_argv[i]);
            }
        }
        else
        {
            for (i = 0; i < 7; i++)
            {
                if (!task->exec_argv[i])
                    break;
                argv[a++] = task->exec_argv[i];
            }
        }
    }
    else
    {
        if (single_arg)
        {
            argv[a++] = g_strdup_printf("%s %s run", BASHPATH, task->exec_script);
        }
        else
        {
            argv[a++] = g_strdup(BASHPATH);
            argv[a++] = g_strdup(task->exec_script);
            argv[a++] = g_strdup("run");
        }
    }

    argv[a++] = nullptr;
    if (su)
        g_free(su);

    char* first_arg;
    first_arg = g_strdup(argv[0]);
    if (task->exec_sync)
    {
        result = g_spawn_async_with_pipes(task->dest_dir,
                                          argv,
                                          nullptr,
                                          G_SPAWN_DO_NOT_REAP_CHILD,
                                          nullptr,
                                          nullptr,
                                          &pid,
                                          nullptr,
                                          &out,
                                          &err,
                                          nullptr);
    }
    else
    {
        result = g_spawn_async_with_pipes(task->dest_dir,
                                          argv,
                                          nullptr,
                                          G_SPAWN_DO_NOT_REAP_CHILD,
                                          nullptr,
                                          nullptr,
                                          &pid,
                                          nullptr,
                                          nullptr,
                                          nullptr,
                                          nullptr);
    }

    print_task_command_spawn(argv, pid);

    if (!result)
    {
        if (errno)
            LOG_INFO("    result={} ( {} )", errno, g_strerror(errno));
        if (!task->exec_keep_tmp && task->exec_sync)
        {
            if (task->exec_script)
                unlink(task->exec_script);
        }
        str = g_strdup_printf(
            "Error executing '%s'\nSee stdout (run spacefm in a terminal) for debug info",
            first_arg);
        g_free(first_arg);
        vfs_file_task_exec_error(task, errno, str);
        g_free(str);
        call_state_callback(task, VFS_FILE_TASK_FINISH);
        return;
    }
    g_free(first_arg);

    if (!task->exec_sync)
    {
        // catch termination to waitpid and delete tmp if needed
        // task can be destroyed while this watch is still active
        g_child_watch_add(pid,
                          (GChildWatchFunc)cb_exec_child_cleanup,
                          !task->exec_keep_tmp && !task->exec_direct && task->exec_script
                              ? g_strdup(task->exec_script)
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

    // out and err can/should be closed too?

_exit_with_error:
    vfs_file_task_exec_error(task, errno, g_strdup("Error writing temporary file"));
    g_string_free((GString*)buf, true);

    if (!task->exec_keep_tmp)
    {
        if (task->exec_script)
            unlink(task->exec_script);
    }
_exit_with_error_lean:
    g_strfreev(terminalv);
    g_free(terminal);
    g_free(su);
    call_state_callback(task, VFS_FILE_TASK_FINISH);
    // LOG_INFO("vfs_file_task_exec DONE ERROR");
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
    string_copy_free(&task->current_file, task->src_paths ? (char*)task->src_paths->data : nullptr);
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
            if (!(task->dest_dir && stat(task->dest_dir, &file_stat) == 0))
            {
                vfs_file_task_error(task, errno, "Accessing", task->dest_dir);
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

    if (task->dest_dir && stat(task->dest_dir, &file_stat) != -1)
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
        append_add_log(task, "Timed out calculating total size\n", -1);
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
    task->dest_dir = g_strdup(dest_dir);
    task->current_file = nullptr;
    task->current_dest = nullptr;

    task->recursive = (task->type == VFS_FILE_TASK_COPY || task->type == VFS_FILE_TASK_DELETE);

    task->err_count = 0;
    task->abort = false;
    task->error_first = true;
    task->custom_percent = false;

    task->exec_type = VFS_EXEC_NORMAL;
    task->exec_action = nullptr;
    task->exec_command = nullptr;
    task->exec_sync = true;
    task->exec_popup = false;
    task->exec_show_output = false;
    task->exec_show_error = false;
    task->exec_terminal = false;
    task->exec_keep_terminal = false;
    task->exec_export = false;
    task->exec_direct = false;
    task->exec_as_user = nullptr;
    task->exec_icon = nullptr;
    task->exec_script = nullptr;
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
            g_free(dir);
        }
        else
            task->avoid_changes = vfs_volume_dir_avoid_changes(task->dest_dir);

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
    g_free(task->dest_dir);
    g_free(task->current_file);
    g_free(task->current_dest);
    g_slist_free(task->devs);

    if (task->chmod_actions)
        g_slice_free1(sizeof(unsigned char) * N_CHMOD_ACTIONS, task->chmod_actions);

    if (task->exec_action)
        g_free(task->exec_action);
    if (task->exec_as_user)
        g_free(task->exec_as_user);
    if (task->exec_command)
        g_free(task->exec_command);
    if (task->exec_script)
        g_free(task->exec_script);

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

    GDir* dir = g_dir_open(path, 0, nullptr);
    if (dir)
    {
        const char* name;
        while ((name = g_dir_read_name(dir)))
        {
            if (task->state == VFS_FILE_TASK_SIZE_TIMEOUT || task->abort)
                break;
            char* full_path = g_build_filename(path, name, nullptr);
            if (lstat(full_path, &file_stat) != -1)
            {
                if (S_ISDIR(file_stat.st_mode))
                    get_total_size_of_dir(task, full_path, size, &file_stat);
                else
                    *size += file_stat.st_size;
            }
            g_free(full_path);
        }
        g_dir_close(dir);
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
    char* msg = g_strdup_printf("\n%s %s\nError: %s\n", action, target, g_strerror(errnox));
    append_add_log(task, msg, -1);
    g_free(msg);
    call_state_callback(task, VFS_FILE_TASK_ERROR);
}
