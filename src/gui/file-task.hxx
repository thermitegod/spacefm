/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <glibmm.h>
#include <gtkmm.h>

#include "vfs/vfs-file-task.hxx"

#define PTK_FILE_TASK(obj) (static_cast<ptk::file_task*>(obj))

namespace ptk
{
struct file_task
{
    enum class ptask_error : std::uint8_t
    {
        first,
        any,
        cont
    };

    enum class response : std::uint8_t
    {
        close, // close window
        overwrite,
        overwrite_all,
        rename,
        skip,
        skip_all,
        auto_rename,
        auto_rename_all,
        pause,
    };

    file_task() = delete;
    file_task(const vfs::file_task::type type,
              const std::span<const std::filesystem::path> src_files,
              const std::filesystem::path& dest_dir, GtkWindow* parent_window,
              GtkWidget* task_view) noexcept;
    ~file_task() noexcept;
    file_task(const file_task& other) = delete;
    file_task(file_task&& other) = delete;
    file_task& operator=(const file_task& other) = delete;
    file_task& operator=(file_task&& other) = delete;

    std::shared_ptr<vfs::file_task> task{nullptr};

    GtkWidget* progress_dlg_{nullptr};
    GtkButton* progress_btn_close_{nullptr};
    GtkButton* progress_btn_stop_{nullptr};
    GtkButton* progress_btn_pause_{nullptr};
    GtkWindow* parent_window_{nullptr};
    GtkWidget* task_view_{nullptr};
    GtkLabel* from_{nullptr};
    GtkLabel* to_{nullptr};
    GtkLabel* src_dir_{nullptr};
    GtkLabel* current_{nullptr};
    GtkProgressBar* progress_bar_{nullptr};
    GtkLabel* errors_{nullptr};
    GtkWidget* error_view_{nullptr};
    GtkScrolledWindow* scroll_{nullptr};
    GtkWidget* overwrite_combo_{nullptr};
    GtkWidget* error_combo_{nullptr};

    GtkTextBuffer* log_buf_{nullptr};
    GtkTextMark* log_end_{nullptr};
    bool log_appended_{false};
    i32 err_count_{0};
    ptk::file_task::ptask_error err_mode_{ptk::file_task::ptask_error::first};

    bool complete_{false};
    bool aborted_{false};
    bool pause_change_{false};
    bool pause_change_view_{true};

    /* <private> */
    u32 timeout_{0};
    bool restart_timeout_{false};
    u32 progress_timer_{0};
    u8 progress_count_{0};
    GFunc complete_notify_{nullptr};
    void* user_data_{nullptr};
    bool keep_dlg_{false};
    bool pop_detail_{false};

    GCond* query_cond_{nullptr};
    GCond* query_cond_last_{nullptr};
    char** query_new_dest_{nullptr};
    bool query_ret_{false};

    [[nodiscard]] bool is_completed() const noexcept;
    [[nodiscard]] bool is_aborted() const noexcept;

    [[nodiscard]] std::string_view display_file_count() const noexcept;
    [[nodiscard]] std::string_view display_size_tally() const noexcept;
    [[nodiscard]] std::string_view display_elapsed() const noexcept;
    [[nodiscard]] std::string_view display_current_speed() const noexcept;
    [[nodiscard]] std::string_view display_current_estimate() const noexcept;
    [[nodiscard]] std::string_view display_average_speed() const noexcept;
    [[nodiscard]] std::string_view display_average_estimate() const noexcept;

    void set_complete_notify(GFunc callback, void* user_data) noexcept;
    void set_chmod(const std::array<u8, 12> chmod_actions) noexcept;
    void set_chown(const uid_t uid, const gid_t gid) noexcept;
    void set_recursive(const bool recursive) noexcept;
    void run() noexcept;
    bool cancel() noexcept;
    void pause(const vfs::file_task::state state) noexcept;
    void progress_open() noexcept;

    void lock() noexcept;
    bool trylock() noexcept;
    void unlock() noexcept;

    void save_progress_dialog_size() const noexcept; // private

    void query_overwrite() noexcept;

    void update() noexcept;

  private:
    void set_button_states() noexcept;
    void set_progress_icon() noexcept;
    void progress_update() noexcept;

    std::string display_file_count_;
    std::string display_size_tally_;
    std::string display_elapsed_;
    std::string display_current_speed_;
    std::string display_current_estimate_;
    std::string display_average_speed_;
    std::string display_average_estimate_;
};
} // namespace ptk

ptk::file_task* ptk_file_task_new(const vfs::file_task::type type,
                                  const std::span<const std::filesystem::path> src_files,
                                  GtkWindow* parent_window, GtkWidget* task_view) noexcept;

ptk::file_task* ptk_file_task_new(const vfs::file_task::type type,
                                  const std::span<const std::filesystem::path> src_files,
                                  const std::filesystem::path& dest_dir, GtkWindow* parent_window,
                                  GtkWidget* task_view) noexcept;

ptk::file_task* ptk_file_exec_new(const std::string_view item_name, GtkWidget* parent,
                                  GtkWidget* task_view) noexcept;
ptk::file_task* ptk_file_exec_new(const std::string_view item_name,
                                  const std::filesystem::path& dest_dir, GtkWidget* parent,
                                  GtkWidget* task_view) noexcept;
