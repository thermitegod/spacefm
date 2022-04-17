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

#include <glib.h>
#include <gtk/gtk.h>

#include "vfs/vfs-file-task.hxx"

#include "settings.hxx"

enum PTKFileTaskPtaskError
{
    PTASK_ERROR_FIRST,
    PTASK_ERROR_ANY,
    PTASK_ERROR_CONT
};

struct PtkFileTask
{
    VFSFileTask* task;

    GtkWidget* progress_dlg;
    GtkWidget* progress_btn_close;
    GtkWidget* progress_btn_stop;
    GtkWidget* progress_btn_pause;
    GtkWindow* parent_window;
    GtkWidget* task_view;
    GtkLabel* from;
    GtkLabel* to;
    GtkLabel* src_dir;
    GtkLabel* current;
    GtkProgressBar* progress_bar;
    GtkLabel* errors;
    GtkWidget* error_view;
    GtkScrolledWindow* scroll;
    GtkWidget* overwrite_combo;
    GtkWidget* error_combo;

    GtkTextBuffer* log_buf;
    GtkTextMark* log_end;
    bool log_appended;
    int err_count;
    PTKFileTaskPtaskError err_mode;

    bool complete;
    bool aborted;
    bool pause_change;
    bool pause_change_view;
    bool force_scroll;

    /* <private> */
    unsigned int timeout;
    bool restart_timeout;
    unsigned int progress_timer;
    char progress_count;
    GFunc complete_notify;
    void* user_data;
    bool keep_dlg;
    bool pop_detail;
    char* pop_handler;

    GCond* query_cond;
    GCond* query_cond_last;
    char** query_new_dest;
    bool query_ret;

    char* dsp_file_count;
    char* dsp_size_tally;
    char* dsp_elapsed;
    char* dsp_curspeed;
    char* dsp_curest;
    char* dsp_avgspeed;
    char* dsp_avgest;
};

void ptk_file_task_lock(PtkFileTask* ptask);
void ptk_file_task_unlock(PtkFileTask* ptask);

PtkFileTask* ptk_file_task_new(VFSFileTaskType type, GList* src_files, const char* dest_dir,
                               GtkWindow* parent_window, GtkWidget* task_view);
PtkFileTask* ptk_file_exec_new(const std::string& item_name, const char* dir, GtkWidget* parent,
                               GtkWidget* task_view);

void ptk_file_task_destroy(PtkFileTask* ptask);

void ptk_file_task_set_complete_notify(PtkFileTask* ptask, GFunc callback, void* user_data);

void ptk_file_task_set_chmod(PtkFileTask* ptask, unsigned char* chmod_actions);

void ptk_file_task_set_chown(PtkFileTask* ptask, uid_t uid, gid_t gid);

void ptk_file_task_set_recursive(PtkFileTask* ptask, bool recursive);

void ptk_file_task_run(PtkFileTask* ptask);

bool ptk_file_task_cancel(PtkFileTask* ptask);

void ptk_file_task_pause(PtkFileTask* ptask, int state);

void ptk_file_task_progress_open(PtkFileTask* ptask);
