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

#include <glib.h>
#include <gtk/gtk.h>

#include "vfs/vfs-file-task.hxx"

#include "settings.hxx"

#define PTK_FILE_TASK(obj) (static_cast<PtkFileTask*>(obj))

enum PTKFileTaskPtaskError
{
    PTASK_ERROR_FIRST,
    PTASK_ERROR_ANY,
    PTASK_ERROR_CONT
};

struct PtkFileTask
{
    PtkFileTask() = delete;
    ~PtkFileTask();

    PtkFileTask(VFSFileTaskType type, const std::span<const std::filesystem::path> src_files,
                const std::filesystem::path& dest_dir, GtkWindow* parent_window,
                GtkWidget* task_view);

    vfs::file_task task{nullptr};

    GtkWidget* progress_dlg{nullptr};
    GtkWidget* progress_btn_close{nullptr};
    GtkWidget* progress_btn_stop{nullptr};
    GtkWidget* progress_btn_pause{nullptr};
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
    PTKFileTaskPtaskError err_mode{PTKFileTaskPtaskError::PTASK_ERROR_FIRST};

    bool complete{false};
    bool aborted{false};
    bool pause_change{false};
    bool pause_change_view{false};
    bool force_scroll{false};

    /* <private> */
    u32 timeout{0};
    bool restart_timeout{false};
    u32 progress_timer{0};
    char progress_count{false};
    GFunc complete_notify{};
    void* user_data{nullptr};
    bool keep_dlg{false};
    bool pop_detail{false};
    char* pop_handler{nullptr};

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

void ptk_file_task_lock(PtkFileTask* ptask);
void ptk_file_task_unlock(PtkFileTask* ptask);

PtkFileTask* ptk_file_task_new(VFSFileTaskType type,
                               const std::span<const std::filesystem::path> src_files,
                               GtkWindow* parent_window, GtkWidget* task_view);

PtkFileTask* ptk_file_task_new(VFSFileTaskType type,
                               const std::span<const std::filesystem::path> src_files,
                               const std::filesystem::path& dest_dir, GtkWindow* parent_window,
                               GtkWidget* task_view);

PtkFileTask* ptk_file_exec_new(const std::string_view item_name, GtkWidget* parent,
                               GtkWidget* task_view);
PtkFileTask* ptk_file_exec_new(const std::string_view item_name,
                               const std::filesystem::path& dest_dir, GtkWidget* parent,
                               GtkWidget* task_view);

void ptk_file_task_set_complete_notify(PtkFileTask* ptask, GFunc callback, void* user_data);

void ptk_file_task_set_chmod(PtkFileTask* ptask, unsigned char* chmod_actions);

void ptk_file_task_set_chown(PtkFileTask* ptask, uid_t uid, gid_t gid);

void ptk_file_task_set_recursive(PtkFileTask* ptask, bool recursive);

void ptk_file_task_run(PtkFileTask* ptask);

bool ptk_file_task_cancel(PtkFileTask* ptask);

void ptk_file_task_pause(PtkFileTask* ptask, i32 state);

void ptk_file_task_progress_open(PtkFileTask* ptask);
