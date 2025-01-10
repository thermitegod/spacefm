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

#include <chrono>

#include <optional>

#include <memory>

#include <functional>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>

namespace vfs
{
class file_task final : public std::enable_shared_from_this<file_task>
{
  public:
    enum class type : std::uint8_t
    {
        move,
        copy,
        trash,
        del, // cannot be named delete
        link,
        chmod_chown, // These two kinds of operation have lots in common,
                     // so put them together to reduce duplicated disk I/O
        exec,
        last,
    };

    enum class state : std::uint8_t
    {
        running,
        size_timeout,
        query_overwrite,
        error,
        pause,
        queue,
        finish,
    };

    enum class overwrite_mode : std::uint8_t
    {                  // do not reposition first four values
        overwrite,     // Overwrite current dest file / Ask
        overwrite_all, // Overwrite all existing files without prompt
        skip_all,      // Do not try to overwrite any files
        auto_rename,   // Assign a new unique name
        skip,          // Do not overwrite current file
        rename,        // Rename file
    };

    enum chmod_action : std::uint8_t
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

    file_task() = delete;
    file_task(const type type, const std::span<const std::filesystem::path> src_files,
              const std::filesystem::path& dest_dir) noexcept;
    ~file_task() noexcept;
    file_task(const file_task& other) = delete;
    file_task(file_task&& other) = delete;
    file_task& operator=(const file_task& other) = delete;
    file_task& operator=(file_task&& other) = delete;

    [[nodiscard]] static std::shared_ptr<vfs::file_task>
    create(const type type, const std::span<const std::filesystem::path> src_files,
           const std::filesystem::path& dest_dir) noexcept;

    void lock() const noexcept;
    void unlock() const noexcept;

    using state_callback_t =
        std::function<bool(const std::shared_ptr<vfs::file_task>& task,
                           const vfs::file_task::state state, void* state_data, void* user_data)>;

    void set_state_callback(const state_callback_t& cb, void* user_data) noexcept;

    void set_chmod(std::array<u8, 12> new_chmod_actions) noexcept;
    void set_chown(uid_t new_uid, gid_t new_gid) noexcept;

    void set_recursive(bool recursive) noexcept;
    void set_overwrite_mode(const vfs::file_task::overwrite_mode mode) noexcept;

    void run_task() noexcept;
    void try_abort_task() noexcept;
    void abort_task() noexcept;

    // TODO private:
    void file_copy(const std::filesystem::path& src_file) noexcept;
    void file_move(const std::filesystem::path& src_file) noexcept;
    void file_trash(const std::filesystem::path& src_file) noexcept;
    void file_delete(const std::filesystem::path& src_file) noexcept;
    void file_link(const std::filesystem::path& src_file) noexcept;
    void file_chown_chmod(const std::filesystem::path& src_file) noexcept;
    void file_exec(const std::filesystem::path& src_file) noexcept;

    [[nodiscard]] bool should_abort() noexcept;

    [[nodiscard]] u64 get_total_size_of_dir(const std::filesystem::path& path) noexcept;

    void append_add_log(const std::string_view msg) const noexcept;

    void task_error(i32 errnox, const std::string_view action) noexcept;
    void task_error(i32 errnox, const std::string_view action,
                    const std::filesystem::path& target) noexcept;

  private:
    [[nodiscard]] bool check_overwrite(const std::filesystem::path& dest_file, bool& dest_exists,
                                       std::filesystem::path& new_dest_file) noexcept;
    [[nodiscard]] bool check_dest_in_src(const std::filesystem::path& src_dir) noexcept;

    [[nodiscard]] i32 do_file_move(const std::filesystem::path& src_file,
                                   const std::filesystem::path& dest_path) noexcept;
    [[nodiscard]] bool do_file_copy(const std::filesystem::path& src_file,
                                    const std::filesystem::path& dest_file) noexcept;

  public:
    vfs::file_task::type type_;
    std::vector<std::filesystem::path> src_paths;  // All source files
    std::optional<std::filesystem::path> dest_dir; // Destinaton directory

    vfs::file_task::overwrite_mode overwrite_mode_;
    bool is_recursive{false}; // Apply operation to all files under directories
                              // recursively. This is default to copy/delete,
                              // and should be set manually for chown/chmod.

    u64 total_size{0}; // Total size of the files to be processed, in bytes
    u64 progress{0};   // Total size of current processed files, in btytes
    i32 percent{0};    // progress (percentage)
    bool custom_percent{false};
    u64 last_speed{0};
    u64 last_progress{0};
    std::chrono::seconds last_elapsed{std::chrono::seconds::zero()};
    u32 current_item{0};

    ztd::timer<std::chrono::milliseconds> timer;
    std::chrono::system_clock::time_point start_time;

    // copy of Current processed file
    std::optional<std::filesystem::path> current_file{std::nullopt};
    // copy of Current destination file
    std::optional<std::filesystem::path> current_dest{std::nullopt};

    i32 err_count{0};
    i32 error{0};
    bool error_first{true};

    GThread* thread{nullptr};
    vfs::file_task::state state_;
    vfs::file_task::state state_pause_{vfs::file_task::state::running};
    bool abort{false};
    GCond* pause_cond{nullptr};
    bool queue_start{false};

    state_callback_t state_cb{nullptr};
    void* state_cb_data{nullptr};

    GMutex* mutex{nullptr};

    // write directly to gtk buffer for speed
    GtkTextBuffer* add_log_buf{nullptr};
    GtkTextMark* add_log_end{nullptr};

    // run task
    std::string exec_action;
    std::string exec_command;
    bool exec_sync{true};
    bool exec_popup{false};
    bool exec_show_output{false};
    bool exec_show_error{false};
    bool exec_terminal{false};
    void* exec_browser{nullptr};
    std::string exec_icon;
    GCond* exec_cond{nullptr};

  private:
    bool avoid_changes_{false};

    // For chown
    uid_t uid_{0};
    gid_t gid_{0};

    // For chmod. If chmod is not needed, this should be nullopt
    std::optional<std::array<u8, 12>> chmod_actions_{std::nullopt};
};
} // namespace vfs
