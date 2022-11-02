/**
 * Copyright (C) 2005 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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

#pragma once

#include <string>

#include <array>
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include <glib.h>
#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <magic_enum.hpp>

enum VFSFileTaskType
{
    VFS_FILE_TASK_MOVE,
    VFS_FILE_TASK_COPY,
    VFS_FILE_TASK_TRASH,
    VFS_FILE_TASK_DELETE,
    VFS_FILE_TASK_LINK,
    VFS_FILE_TASK_CHMOD_CHOWN, // These two kinds of operation have lots in common,
                               // so put them together to reduce duplicated disk I/O
    VFS_FILE_TASK_EXEC,        // MOD
    VFS_FILE_TASK_LAST
};

enum ChmodActionType
{
    OWNER_R,
    OWNER_W,
    OWNER_X,
    GROUP_R,
    GROUP_W,
    GROUP_X,
    OTHER_R,
    OTHER_W,
    OTHER_X,
    SET_UID,
    SET_GID,
    STICKY,
};

inline constexpr std::array<mode_t, 12> chmod_flags{
    S_IRUSR,
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
    S_ISVTX,
};

enum VFSFileTaskState
{
    VFS_FILE_TASK_RUNNING,
    VFS_FILE_TASK_SIZE_TIMEOUT,
    VFS_FILE_TASK_QUERY_OVERWRITE,
    VFS_FILE_TASK_ERROR,
    VFS_FILE_TASK_PAUSE,
    VFS_FILE_TASK_QUEUE,
    VFS_FILE_TASK_FINISH
};

enum VFSFileTaskOverwriteMode
{
    // do not reposition first four values
    VFS_FILE_TASK_OVERWRITE,     // Overwrite current dest file / Ask
    VFS_FILE_TASK_OVERWRITE_ALL, // Overwrite all existing files without prompt
    VFS_FILE_TASK_SKIP_ALL,      // Do not try to overwrite any files
    VFS_FILE_TASK_AUTO_RENAME,   // Assign a new unique name
    VFS_FILE_TASK_SKIP,          // Do not overwrite current file
    VFS_FILE_TASK_RENAME         // Rename file
};

enum VFSExecType
{
    VFS_EXEC_NORMAL,
    VFS_EXEC_CUSTOM,
};

struct VFSFileTask;

using VFSFileTaskStateCallback = bool (*)(VFSFileTask* task, VFSFileTaskState state,
                                          void* state_data, void* user_data);

struct VFSFileTask
{
    VFSFileTaskType type;
    std::vector<std::string> src_paths; // All source files. This list will be freed
                                        // after file operation is completed.
    std::string dest_dir;               // Destinaton directory
    bool avoid_changes;
    GSList* devs{nullptr};

    VFSFileTaskOverwriteMode overwrite_mode;
    bool recursive; // Apply operation to all files under directories
                    // recursively. This is default to copy/delete,
                    // and should be set manually for chown/chmod.

    // For chown
    uid_t uid;
    gid_t gid;

    // For chmod
    unsigned char* chmod_actions; // If chmod is not needed, this should be nullptr

    off_t total_size; // Total size of the files to be processed, in bytes
    off_t progress;   // Total size of current processed files, in btytes
    int percent{0};   // progress (percentage)
    bool custom_percent{false};
    off_t last_speed{0};
    off_t last_progress{0};
    double last_elapsed{0.0};
    unsigned int current_item{0};

    ztd::timer timer;
    std::time_t start_time;

    std::string current_file; // copy of Current processed file
    std::string current_dest; // copy of Current destination file

    int err_count{0};
    int error{0};
    bool error_first{true};

    GThread* thread;
    VFSFileTaskState state;
    VFSFileTaskState state_pause{VFSFileTaskState::VFS_FILE_TASK_RUNNING};
    bool abort{false};
    GCond* pause_cond{nullptr};
    bool queue_start{false};

    VFSFileTaskStateCallback state_cb;
    void* state_cb_data;

    GMutex* mutex;

    // sfm write directly to gtk buffer for speed
    GtkTextBuffer* add_log_buf;
    GtkTextMark* add_log_end;

    // MOD run task
    VFSExecType exec_type{VFSExecType::VFS_EXEC_NORMAL};
    std::string exec_action;
    std::string exec_command;
    bool exec_sync{true};
    bool exec_popup{false};
    bool exec_show_output{false};
    bool exec_show_error{false};
    bool exec_terminal{false};
    bool exec_keep_terminal{false};
    bool exec_export{false};
    bool exec_direct{false};
    std::vector<std::string> exec_argv; // for exec_direct, command ignored
                                        // for su commands, must use bash -c
                                        // as su does not execute binaries
    std::string exec_script;
    bool exec_keep_tmp{false}; // diagnostic to keep temp files
    void* exec_browser{nullptr};
    void* exec_desktop{nullptr};
    std::string exec_as_user;
    std::string exec_icon;
    Glib::Pid exec_pid;
    int exec_exit_status{0};
    unsigned int child_watch{0};
    bool exec_is_error{false};
    GIOChannel* exec_channel_out;
    GIOChannel* exec_channel_err;
    bool exec_scroll_lock{false};
    bool exec_checksum{false};
    void* exec_set{nullptr};
    GCond* exec_cond{nullptr};
    void* exec_ptask{nullptr};
};

VFSFileTask* vfs_task_new(VFSFileTaskType task_type, const std::vector<std::string>& src_files,
                          const char* dest_dir);

void vfs_file_task_lock(VFSFileTask* task);
void vfs_file_task_unlock(VFSFileTask* task);

/* Set some actions for chmod, this array will be copied
 * and stored in VFSFileTask */
void vfs_file_task_set_chmod(VFSFileTask* task, unsigned char* chmod_actions);

void vfs_file_task_set_chown(VFSFileTask* task, uid_t uid, gid_t gid);

void vfs_file_task_set_state_callback(VFSFileTask* task, VFSFileTaskStateCallback cb,
                                      void* user_data);

void vfs_file_task_set_recursive(VFSFileTask* task, bool recursive);

void vfs_file_task_set_overwrite_mode(VFSFileTask* task, VFSFileTaskOverwriteMode mode);

void vfs_file_task_run(VFSFileTask* task);

void vfs_file_task_try_abort(VFSFileTask* task);

void vfs_file_task_abort(VFSFileTask* task);

void vfs_file_task_free(VFSFileTask* task);

char* vfs_file_task_get_cpids(Glib::Pid pid);
void vfs_file_task_kill_cpids(char* cpids, int signal);
char* vfs_file_task_get_unique_name(const std::string& dest_dir, const std::string& base_name,
                                    const std::string& ext);
