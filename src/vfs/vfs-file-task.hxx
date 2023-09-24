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

#include <optional>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"

namespace vfs
{
    enum class file_task_type
    {
        move,
        copy,
        trash,
        DELETE,
        link,
        chmod_chown, // These two kinds of operation have lots in common,
                     // so put them together to reduce duplicated disk I/O
        exec,
        last,
    };

    enum class file_task_state
    {
        running,
        size_timeout,
        query_overwrite,
        error,
        pause,
        queue,
        finish,
    };

    enum class file_task_overwrite_mode
    {                  // do not reposition first four values
        overwrite,     // Overwrite current dest file / Ask
        overwrite_all, // Overwrite all existing files without prompt
        skip_all,      // Do not try to overwrite any files
        auto_rename,   // Assign a new unique name
        skip,          // Do not overwrite current file
        rename,        // Rename file
    };

    enum class exec_type
    {
        normal,
        custom,
    };

    enum chmod_action
    {
        owner_r,
        owner_w,
        owner_x,
        group_r,
        group_w,
        group_x,
        other_r,
        other_w,
        other_x,
        set_uid,
        set_gid,
        sticky,
    };
} // namespace vfs

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

class VFSFileTask;

namespace vfs
{
    using file_task = ztd::raw_ptr<VFSFileTask>;
}

using VFSFileTaskStateCallback = bool (*)(vfs::file_task task, vfs::file_task_state state,
                                          void* state_data, void* user_data);

class VFSFileTask
{
  public:
    VFSFileTask() = delete;
    VFSFileTask(vfs::file_task_type type, const std::span<const std::filesystem::path> src_files,
                const std::filesystem::path& dest_dir);
    ~VFSFileTask();

    void lock();
    void unlock();

    void set_state_callback(VFSFileTaskStateCallback cb, void* user_data);

    void set_chmod(std::array<u8, 12> new_chmod_actions);
    void set_chown(uid_t new_uid, gid_t new_gid);

    void set_recursive(bool recursive);
    void set_overwrite_mode(vfs::file_task_overwrite_mode mode);

    void run_task();
    void try_abort_task();
    void abort_task();

  public: // private: // TODO
    bool check_overwrite(const std::filesystem::path& dest_file, bool* dest_exists,
                         char** new_dest_file);
    bool check_dest_in_src(const std::filesystem::path& src_dir);

    void file_copy(const std::filesystem::path& src_file);
    bool do_file_copy(const std::filesystem::path& src_file,
                      const std::filesystem::path& dest_file);

    void file_move(const std::filesystem::path& src_file);
    i32 do_file_move(const std::filesystem::path& src_file, const std::filesystem::path& dest_path);

    void file_trash(const std::filesystem::path& src_file);
    void file_delete(const std::filesystem::path& src_file);
    void file_link(const std::filesystem::path& src_file);
    void file_chown_chmod(const std::filesystem::path& src_file);
    void file_exec(const std::filesystem::path& src_file);

    bool should_abort();

    u64 get_total_size_of_dir(const std::filesystem::path& path);

    void append_add_log(const std::string_view msg);

    void task_error(i32 errnox, const std::string_view action);
    void task_error(i32 errnox, const std::string_view action, const std::filesystem::path& target);

  public:
    vfs::file_task_type type;
    std::vector<std::filesystem::path> src_paths{};  // All source files. This list will be freed
                                                     // after file operation is completed.
    std::optional<std::filesystem::path> dest_dir{}; // Destinaton directory
    bool avoid_changes{false};

    vfs::file_task_overwrite_mode overwrite_mode;
    bool is_recursive{false}; // Apply operation to all files under directories
                              // recursively. This is default to copy/delete,
                              // and should be set manually for chown/chmod.

    // For chown
    uid_t uid{0};
    gid_t gid{0};

    // For chmod. If chmod is not needed, this should be nullptr
    std::optional<std::array<u8, 12>> chmod_actions{std::nullopt};

    u64 total_size{0}; // Total size of the files to be processed, in bytes
    u64 progress{0};   // Total size of current processed files, in btytes
    i32 percent{0};    // progress (percentage)
    bool custom_percent{false};
    u64 last_speed{0};
    u64 last_progress{0};
    f64 last_elapsed{0.0};
    u32 current_item{0};

    ztd::timer timer;
    std::time_t start_time;

    // copy of Current processed file
    std::optional<std::filesystem::path> current_file{std::nullopt};
    // copy of Current destination file
    std::optional<std::filesystem::path> current_dest{std::nullopt};

    i32 err_count{0};
    i32 error{0};
    bool error_first{true};

    GThread* thread{nullptr};
    vfs::file_task_state state;
    vfs::file_task_state state_pause{vfs::file_task_state::running};
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
    vfs::exec_type exec_type{vfs::exec_type::normal};
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
    std::optional<std::filesystem::path> exec_script{};
    bool exec_keep_tmp{false}; // diagnostic to keep temp files
    void* exec_browser{nullptr};
    void* exec_desktop{nullptr};
    std::string exec_icon{};
    pid_t exec_pid{0};
    i32 exec_exit_status{0};
    u32 child_watch{0};
    bool exec_is_error{false};
    GIOChannel* exec_channel_out{nullptr};
    GIOChannel* exec_channel_err{nullptr};
    bool exec_scroll_lock{false};
    xset_t exec_set{nullptr};
    GCond* exec_cond{nullptr};
    void* exec_ptask{nullptr};
};

vfs::file_task vfs_task_new(vfs::file_task_type task_type,
                            const std::span<const std::filesystem::path> src_files,
                            const std::filesystem::path& dest_dir);

void vfs_file_task_free(vfs::file_task task);
