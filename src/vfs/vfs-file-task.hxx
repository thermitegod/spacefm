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
#include <string_view>

#include <filesystem>

#include <array>
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include <glib.h>
#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <magic_enum.hpp>

#include "xset/xset.hxx"

enum VFSFileTaskType
{
    MOVE,
    COPY,
    TRASH,
    DELETE,
    LINK,
    CHMOD_CHOWN, // These two kinds of operation have lots in common,
                 // so put them together to reduce duplicated disk I/O
    EXEC,
    LAST,
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

enum VFSFileTaskState
{
    RUNNING,
    SIZE_TIMEOUT,
    QUERY_OVERWRITE,
    ERROR,
    PAUSE,
    QUEUE,
    FINISH,
};

enum VFSFileTaskOverwriteMode
{
    // do not reposition first four values
    OVERWRITE,     // Overwrite current dest file / Ask
    OVERWRITE_ALL, // Overwrite all existing files without prompt
    SKIP_ALL,      // Do not try to overwrite any files
    AUTO_RENAME,   // Assign a new unique name
    SKIP,          // Do not overwrite current file
    RENAME,        // Rename file
};

enum VFSExecType
{
    NORMAL,
    CUSTOM,
};

class VFSFileTask;

namespace vfs
{
    using file_task = ztd::raw_ptr<VFSFileTask>;
}

using VFSFileTaskStateCallback = bool (*)(vfs::file_task task, VFSFileTaskState state,
                                          void* state_data, void* user_data);

class VFSFileTask
{
  public:
    VFSFileTask() = delete;
    VFSFileTask(VFSFileTaskType type, const std::span<const std::string> src_files,
                std::string_view dest_dir);
    ~VFSFileTask();

    void lock();
    void unlock();

    void set_state_callback(VFSFileTaskStateCallback cb, void* user_data);

    void set_chmod(unsigned char* new_chmod_actions);
    void set_chown(uid_t new_uid, gid_t new_gid);

    void set_recursive(bool recursive);
    void set_overwrite_mode(VFSFileTaskOverwriteMode mode);

    void run_task();
    void try_abort_task();
    void abort_task();

  public: // private: // TODO
    bool check_overwrite(std::string_view dest_file, bool* dest_exists, char** new_dest_file);
    bool check_dest_in_src(std::string_view src_dir);

    void file_copy(std::string_view src_file);
    bool do_file_copy(std::string_view src_file, std::string_view dest_file);

    void file_move(std::string_view src_file);
    i32 do_file_move(std::string_view src_file, std::string_view dest_path);

    void file_trash(std::string_view src_file);
    void file_delete(std::string_view src_file);
    void file_link(std::string_view src_file);
    void file_chown_chmod(std::string_view src_file);
    void file_exec(std::string_view src_file);

    bool should_abort();

    off_t get_total_size_of_dir(std::string_view path);

    void append_add_log(std::string_view msg);

    void task_error(i32 errnox, std::string_view action);
    void task_error(i32 errnox, std::string_view action, std::string_view target);

  public:
    VFSFileTaskType type;
    std::vector<std::string> src_paths{}; // All source files. This list will be freed
                                          // after file operation is completed.
    std::string dest_dir{};               // Destinaton directory
    bool avoid_changes{false};

    VFSFileTaskOverwriteMode overwrite_mode;
    bool is_recursive{false}; // Apply operation to all files under directories
                              // recursively. This is default to copy/delete,
                              // and should be set manually for chown/chmod.

    // For chown
    uid_t uid{0};
    gid_t gid{0};

    // For chmod. If chmod is not needed, this should be nullptr
    unsigned char* chmod_actions{nullptr};

    off_t total_size{0}; // Total size of the files to be processed, in bytes
    off_t progress{0};   // Total size of current processed files, in btytes
    i32 percent{0};      // progress (percentage)
    bool custom_percent{false};
    off_t last_speed{0};
    off_t last_progress{0};
    f64 last_elapsed{0.0};
    u32 current_item{0};

    ztd::timer timer;
    std::time_t start_time;

    std::string current_file{}; // copy of Current processed file
    std::string current_dest{}; // copy of Current destination file

    i32 err_count{0};
    i32 error{0};
    bool error_first{true};

    GThread* thread{nullptr};
    VFSFileTaskState state;
    VFSFileTaskState state_pause{VFSFileTaskState::RUNNING};
    bool abort{false};
    GCond* pause_cond{nullptr};
    bool queue_start{false};

    VFSFileTaskStateCallback state_cb{nullptr};
    void* state_cb_data{nullptr};

    GMutex* mutex{nullptr};

    // sfm write directly to gtk buffer for speed
    GtkTextBuffer* add_log_buf{nullptr};
    GtkTextMark* add_log_end{nullptr};

    // MOD run task
    VFSExecType exec_type{VFSExecType::NORMAL};
    std::string exec_action{};
    std::string exec_command{};
    bool exec_sync{true};
    bool exec_popup{false};
    bool exec_show_output{false};
    bool exec_show_error{false};
    bool exec_terminal{false};
    bool exec_keep_terminal{false};
    bool exec_export{false};
    bool exec_direct{false};
    std::vector<std::string> exec_argv{}; // for exec_direct, command ignored
                                          // for su commands, must use fish -c
                                          // as su does not execute binaries
    std::string exec_script{};
    bool exec_keep_tmp{false}; // diagnostic to keep temp files
    void* exec_browser{nullptr};
    void* exec_desktop{nullptr};
    std::string exec_as_user{};
    std::string exec_icon{};
    pid_t exec_pid{0};
    i32 exec_exit_status{0};
    u32 child_watch{0};
    bool exec_is_error{false};
    GIOChannel* exec_channel_out{nullptr};
    GIOChannel* exec_channel_err{nullptr};
    bool exec_scroll_lock{false};
    bool exec_checksum{false};
    xset_t exec_set{nullptr};
    GCond* exec_cond{nullptr};
    void* exec_ptask{nullptr};
};

vfs::file_task vfs_task_new(VFSFileTaskType task_type, const std::span<const std::string> src_files,
                            std::string_view dest_dir);

void vfs_file_task_free(vfs::file_task task);
