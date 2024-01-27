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

#include <string>
#include <string_view>

#include <span>

#include <array>

#include <memory>

#include <gtkmm.h>
#include <glibmm.h>

#include "vfs/vfs-file-task.hxx"

#define PTK_FILE_TASK(obj) (static_cast<ptk::file_task*>(obj))

namespace ptk
{
struct file_task
{
    enum class ptask_error
    {
        first,
        any,
        cont
    };

    enum response
    {
        overwrite = 1 << 0,
        overwrite_all = 1 << 1,
        rename = 1 << 2,
        skip = 1 << 3,
        skip_all = 1 << 4,
        auto_rename = 1 << 5,
        auto_rename_all = 1 << 6,
        pause = 1 << 7,
    };

    file_task() = delete;
    ~file_task();

    file_task(const vfs::file_task::type type,
              const std::span<const std::filesystem::path> src_files,
              const std::filesystem::path& dest_dir, GtkWindow* parent_window,
              GtkWidget* task_view);

    std::shared_ptr<vfs::file_task> task{nullptr};

    GtkWidget* progress_dlg{nullptr};
    GtkButton* progress_btn_close{nullptr};
    GtkButton* progress_btn_stop{nullptr};
    GtkButton* progress_btn_pause{nullptr};
    GtkWindow* parent_window{nullptr};
    GtkWidget* task_view{nullptr};
    GtkLabel* from{nullptr};
    GtkLabel* to{nullptr};
    GtkLabel* src_dir{nullptr};
    GtkLabel* current{nullptr};
    GtkProgressBar* progress_bar{nullptr};
    GtkLabel* errors{nullptr};
    GtkWidget* error_view{nullptr};
    GtkScrolledWindow* scroll{nullptr};
    GtkWidget* overwrite_combo{nullptr};
    GtkWidget* error_combo{nullptr};

    GtkTextBuffer* log_buf{nullptr};
    GtkTextMark* log_end{nullptr};
    bool log_appended{false};
    i32 err_count{0};
    ptk::file_task::ptask_error err_mode{ptk::file_task::ptask_error::first};

    bool complete{false};
    bool aborted{false};
    bool pause_change{false};
    bool pause_change_view{true};

    /* <private> */
    u32 timeout{0};
    bool restart_timeout{false};
    u32 progress_timer{0};
    u8 progress_count{0};
    GFunc complete_notify{nullptr};
    void* user_data{nullptr};
    bool keep_dlg{false};
    bool pop_detail{false};

    GCond* query_cond{nullptr};
    GCond* query_cond_last{nullptr};
    char** query_new_dest{nullptr};
    bool query_ret{false};

    std::string dsp_file_count{};
    std::string dsp_size_tally{};
    std::string dsp_elapsed{};
    std::string dsp_curspeed{};
    std::string dsp_curest{};
    std::string dsp_avgspeed{};
    std::string dsp_avgest{};
};
} // namespace ptk

void ptk_file_task_lock(ptk::file_task* ptask);
void ptk_file_task_unlock(ptk::file_task* ptask);

ptk::file_task* ptk_file_task_new(const vfs::file_task::type type,
                                  const std::span<const std::filesystem::path> src_files,
                                  GtkWindow* parent_window, GtkWidget* task_view);

ptk::file_task* ptk_file_task_new(const vfs::file_task::type type,
                                  const std::span<const std::filesystem::path> src_files,
                                  const std::filesystem::path& dest_dir, GtkWindow* parent_window,
                                  GtkWidget* task_view);

ptk::file_task* ptk_file_exec_new(const std::string_view item_name, GtkWidget* parent,
                                  GtkWidget* task_view);
ptk::file_task* ptk_file_exec_new(const std::string_view item_name,
                                  const std::filesystem::path& dest_dir, GtkWidget* parent,
                                  GtkWidget* task_view);

void ptk_file_task_set_complete_notify(ptk::file_task* ptask, GFunc callback, void* user_data);

void ptk_file_task_set_chmod(ptk::file_task* ptask, std::array<u8, 12> chmod_actions);

void ptk_file_task_set_chown(ptk::file_task* ptask, uid_t uid, gid_t gid);

void ptk_file_task_set_recursive(ptk::file_task* ptask, bool recursive);

void ptk_file_task_run(ptk::file_task* ptask);

bool ptk_file_task_cancel(ptk::file_task* ptask);

void ptk_file_task_pause(ptk::file_task* ptask, const vfs::file_task::state state);

void ptk_file_task_progress_open(ptk::file_task* ptask);
