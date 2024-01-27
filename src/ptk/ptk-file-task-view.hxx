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

#pragma once

#include <string_view>

#include <gtkmm.h>

#include "ptk/ptk-file-task.hxx"

#include "main-window.hxx"

namespace ptk::view::file_task
{
enum class column
{
    status,
    count,
    path,
    file,
    to,
    progress,
    total,
    started,
    elapsed,
    curspeed,
    curest,
    avgspeed,
    avgest,
    starttime,
    icon,
    data
};

void stop(GtkWidget* view, const xset_t& set2, ptk::file_task* ptask2) noexcept;
bool is_task_running(GtkWidget* task_view) noexcept;
void prepare_menu(MainWindow* main_window, GtkWidget* menu) noexcept;
void column_selected(GtkWidget* view) noexcept;
void popup_show(MainWindow* main_window, const std::string_view name) noexcept;
void popup_errset(MainWindow* main_window, const std::string_view name = "") noexcept;
ptk::file_task* selected_task(GtkWidget* view) noexcept;
void show_task_dialog(GtkWidget* view) noexcept;

GtkWidget* create(MainWindow* main_window);
void start_queued(GtkWidget* view, ptk::file_task* new_ptask) noexcept;
void update_task(ptk::file_task* ptask) noexcept;
void remove_task(ptk::file_task* ptask) noexcept;
void pause_all_queued(ptk::file_task* ptask) noexcept;

void on_reorder(GtkWidget* item, GtkWidget* parent) noexcept;
} // namespace ptk::view::file_task
