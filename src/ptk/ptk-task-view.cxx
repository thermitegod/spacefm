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

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <array>
#include <unordered_map>
#include <vector>

#include <chrono>

#include <optional>

#include <ranges>

#include <cstring>
#include <cmath>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "ptk/ptk-file-browser.hxx"

#include "main-window.hxx"

#include "ptk/ptk-dialog.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"

#include "vfs/vfs-file-task.hxx"
#include "vfs/vfs-utils.hxx"

#include "ptk/ptk-task-view.hxx"

const std::unordered_map<task_view_column, const std::string_view> task_titles{
    // If you change "Status", also change it in on_task_button_press_event
    {task_view_column::status, "Status"},
    {task_view_column::count, "#"},
    {task_view_column::path, "Directory"},
    {task_view_column::file, "Item"},
    {task_view_column::to, "To"},
    {task_view_column::progress, "Progress"},
    {task_view_column::total, "Total"},
    {task_view_column::started, "Started"},
    {task_view_column::elapsed, "Elapsed"},
    {task_view_column::curspeed, "Current"},
    {task_view_column::curest, "CRemain"},
    {task_view_column::avgspeed, "Average"},
    {task_view_column::avgest, "Remain"},
    {task_view_column::starttime, "StartTime"},
};

inline constexpr std::array<xset::name, 13> task_names{
    xset::name::task_col_status,
    xset::name::task_col_count,
    xset::name::task_col_path,
    xset::name::task_col_file,
    xset::name::task_col_to,
    xset::name::task_col_progress,
    xset::name::task_col_total,
    xset::name::task_col_started,
    xset::name::task_col_elapsed,
    xset::name::task_col_curspeed,
    xset::name::task_col_curest,
    xset::name::task_col_avgspeed,
    xset::name::task_col_avgest,
};

void
on_reorder(GtkWidget* item, GtkWidget* parent)
{
    (void)item;
    ptk_show_message(
        GTK_WINDOW(parent),
        GtkMessageType::GTK_MESSAGE_INFO,
        "Reorder Columns Help",
        GtkButtonsType::GTK_BUTTONS_OK,
        "To change the order of the columns, drag the column header to the desired location.");
}

static MainWindow*
get_task_view_window(GtkWidget* view)
{
    for (MainWindow* window : main_window_get_all())
    {
        if (window->task_view == view)
        {
            return window;
        }
    }
    return nullptr;
}

static void
on_task_columns_changed(GtkWidget* view, void* user_data)
{
    (void)user_data;
    MainWindow* main_window = get_task_view_window(view);
    if (!main_window || !view)
    {
        return;
    }

    for (const auto i : std::views::iota(0uz, task_names.size()))
    {
        GtkTreeViewColumn* col = gtk_tree_view_get_column(GTK_TREE_VIEW(view), static_cast<i32>(i));
        if (!col)
        {
            return;
        }
        const char* title = gtk_tree_view_column_get_title(col);
        for (const auto [index, value] : std::views::enumerate(task_names))
        {
            if (title == task_titles.at(task_view_column(index)))
            {
                const xset_t set = xset_get(value);
                // save column position
                xset_set_var(set, xset::var::x, std::to_string(i));
                // if the window was opened maximized and stayed maximized, or the
                // window is unmaximized and not fullscreen, save the columns
                if ((!main_window->maximized || main_window->opened_maximized) &&
                    !main_window->fullscreen)
                {
                    const i32 width = gtk_tree_view_column_get_width(col);
                    if (width) // manager unshown, all widths are zero
                    {
                        // save column width
                        xset_set_var(set, xset::var::y, std::to_string(width));
                    }
                }
                // set column visibility
                gtk_tree_view_column_set_visible(col, xset_get_b(value));

                break;
            }
        }
    }
}

static void
on_task_destroy(GtkWidget* view, void* user_data)
{
    (void)user_data;
    const u32 id = g_signal_lookup("columns-changed", G_TYPE_FROM_INSTANCE(view));
    if (id)
    {
        const u64 hand = g_signal_handler_find((void*)view,
                                               GSignalMatchType::G_SIGNAL_MATCH_ID,
                                               id,
                                               0,
                                               nullptr,
                                               nullptr,
                                               nullptr);
        if (hand)
        {
            g_signal_handler_disconnect((void*)view, hand);
        }
    }
    on_task_columns_changed(view, nullptr); // save widths
}

static void
on_task_column_selected(GtkMenuItem* item, GtkWidget* view)
{
    (void)item;
    on_task_columns_changed(view, nullptr);
}

void
ptk_task_view_column_selected(GtkWidget* view)
{
    on_task_columns_changed(view, nullptr);
}

bool
ptk_task_view_is_main_tasks_running(GtkWidget* task_view)
{
    if (!task_view || !GTK_IS_TREE_VIEW(task_view))
    {
        return false;
    }
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(task_view));
    GtkTreeIter it;
    return gtk_tree_model_get_iter_first(model, &it);
}

void
main_task_pause_all_queued(PtkFileTask* ptask)
{
    if (!ptask->task_view)
    {
        return;
    }

    PtkFileTask* qtask = nullptr;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ptask->task_view));
    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, task_view_column::data, &qtask, -1);
            if (qtask && qtask != ptask && qtask->task && !qtask->complete &&
                qtask->task->state_pause_ == vfs::file_task::state::queue)
            {
                ptk_file_task_pause(qtask, vfs::file_task::state::pause);
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
}

void
main_task_start_queued(GtkWidget* view, PtkFileTask* new_ptask)
{
    if (!GTK_IS_TREE_VIEW(view))
    {
        return;
    }

    std::vector<PtkFileTask*> running;
    std::vector<PtkFileTask*> queued;

    GtkTreeIter it;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            PtkFileTask* qtask = nullptr;
            gtk_tree_model_get(model, &it, task_view_column::data, &qtask, -1);
            if (qtask && qtask->task && !qtask->complete &&
                qtask->task->state_ == vfs::file_task::state::running)
            {
                if (qtask->task->state_pause_ == vfs::file_task::state::queue)
                {
                    queued.push_back(qtask);
                }
                else if (qtask->task->state_pause_ == vfs::file_task::state::running)
                {
                    running.push_back(qtask);
                }
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }

    if (new_ptask && new_ptask->task && !new_ptask->complete &&
        new_ptask->task->state_pause_ == vfs::file_task::state::queue &&
        new_ptask->task->state_ == vfs::file_task::state::running)
    {
        queued.push_back(new_ptask);
    }

    const bool smart = xset_get_b(xset::name::task_q_smart);
    if (queued.empty() || (!smart && running.empty()))
    {
        return;
    }

    if (!smart)
    {
        ptk_file_task_pause(queued.back(), vfs::file_task::state::running);
        return;
    }

    // smart
    for (PtkFileTask* qtask : queued)
    {
        if (qtask)
        {
            // qtask has no devices so run it
            running.push_back(qtask);
            ptk_file_task_pause(qtask, vfs::file_task::state::running);
            continue;
        }
    }
}

static void
on_task_stop(GtkMenuItem* item, GtkWidget* view, const xset_t& set2, PtkFileTask* ptask2)
{
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    PtkFileTask* ptask = nullptr;
    xset_t set;

    enum class main_window_job
    {
        stop,
        pause,
        queue,
        resume
    };
    main_window_job job;

    if (item)
    {
        set = xset_get(static_cast<const char*>(g_object_get_data(G_OBJECT(item), "set")));
    }
    else
    {
        set = set2;
    }

    if (!set || !set->name.starts_with("task_"))
    {
        return;
    }

    if (set->name.starts_with("task_stop"))
    {
        job = main_window_job::stop;
    }
    else if (set->name.starts_with("task_pause"))
    {
        job = main_window_job::pause;
    }
    else if (set->name.starts_with("task_que"))
    {
        job = main_window_job::queue;
    }
    else if (set->name.starts_with("task_resume"))
    {
        job = main_window_job::resume;
    }
    else
    {
        return;
    }

    const bool all = set->name.ends_with("_all");

    if (all)
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    }
    else
    {
        if (item)
        {
            ptask = PTK_FILE_TASK(g_object_get_data(G_OBJECT(item), "task"));
        }
        else
        {
            ptask = ptask2;
        }
        if (!ptask)
        {
            return;
        }
    }

    if (!model || gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            if (model)
            {
                gtk_tree_model_get(model, &it, task_view_column::data, &ptask, -1);
            }
            if (ptask && ptask->task && !ptask->complete &&
                (ptask->task->type_ != vfs::file_task::type::exec || job == main_window_job::stop))
            {
                switch (job)
                {
                    case main_window_job::stop:
                        ptk_file_task_cancel(ptask);
                        break;
                    case main_window_job::pause:
                        ptk_file_task_pause(ptask, vfs::file_task::state::pause);
                        break;
                    case main_window_job::queue:
                        ptk_file_task_pause(ptask, vfs::file_task::state::queue);
                        break;
                    case main_window_job::resume:
                        ptk_file_task_pause(ptask, vfs::file_task::state::running);
                        break;
                }
            }
        } while (model && gtk_tree_model_iter_next(model, &it));
    }
    main_task_start_queued(view, nullptr);
}

void
ptk_task_view_task_stop(GtkWidget* view, const xset_t& set2, PtkFileTask* ptask2)
{
    on_task_stop(nullptr, view, set2, ptask2);
}

static bool
idle_set_task_height(MainWindow* main_window)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);

    // set new config panel sizes to half of window
    if (!xset_is(xset::name::panel_sliders))
    {
        // this is not perfect because panel half-width is set before user
        // adjusts window size
        const xset_t set = xset_get(xset::name::panel_sliders);
        set->x = std::to_string(allocation.width / 2);
        set->y = std::to_string(allocation.width / 2);
        set->s = std::to_string(allocation.height / 2);
    }

    // restore height (in case window height changed)
    i32 taskh = xset_get_int(xset::name::task_show_manager, xset::var::x); // task height >=0.9.2
    if (taskh == 0)
    {
        // use pre-0.9.2 slider pos to calculate height
        const i32 pos = xset_get_int(xset::name::panel_sliders, xset::var::z); // < 0.9.2 slider pos
        if (pos == 0)
        {
            taskh = 200;
        }
        else
        {
            taskh = allocation.height - pos;
        }
    }
    if (taskh > allocation.height / 2)
    {
        taskh = allocation.height / 2;
    }
    if (taskh < 1)
    {
        taskh = 90;
    }
    // ztd::logger::info("SHOW  win {}x{}    task height {}   slider {}", allocation.width,
    // allocation.height, taskh, allocation.height - taskh);
    gtk_paned_set_position(main_window->task_vpane, allocation.height - taskh);
    return false;
}

static void
show_task_manager(MainWindow* main_window, bool show)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);

    if (show)
    {
        if (!gtk_widget_get_visible(GTK_WIDGET(main_window->task_scroll)))
        {
            gtk_widget_show(GTK_WIDGET(main_window->task_scroll));
            // allow vpane to auto-adjust before setting new slider pos
            g_idle_add((GSourceFunc)idle_set_task_height, main_window);
        }
    }
    else
    {
        // save height
        if (gtk_widget_get_visible(GTK_WIDGET(main_window->task_scroll)))
        {
            const i32 pos = gtk_paned_get_position(main_window->task_vpane);
            if (pos)
            {
                // save slider pos for version < 0.9.2 (in case of downgrade)
                xset_set(xset::name::panel_sliders, xset::var::z, std::to_string(pos));
                // save absolute height introduced v0.9.2
                xset_set(xset::name::task_show_manager,
                         xset::var::x,
                         std::to_string(allocation.height - pos));
                // ztd::logger::info("HIDE  win {}x{}    task height {}   slider {}",
                // allocation.width, allocation.height, allocation.height - pos, pos);
            }
        }
        // hide
        const bool tasks_has_focus = gtk_widget_is_focus(GTK_WIDGET(main_window->task_view));
        gtk_widget_hide(GTK_WIDGET(main_window->task_scroll));
        if (tasks_has_focus)
        {
            // focus the file list
            ptk::browser* file_browser = main_window->current_file_browser();
            if (file_browser)
            {
                gtk_widget_grab_focus(file_browser->folder_view());
            }
        }
    }
}

static void
on_task_popup_show(GtkMenuItem* item, MainWindow* main_window, const char* name2)
{
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    const char* name = nullptr;

    if (item)
    {
        name = static_cast<const char*>(g_object_get_data(G_OBJECT(item), "name"));
    }
    else
    {
        name = name2;
    }

    if (name)
    {
        const xset::name xset_name = xset::get_xsetname_from_name(name);

        if (xset_name == xset::name::task_show_manager)
        {
            if (xset_get_b(xset::name::task_show_manager))
            {
                xset_set_b(xset::name::task_hide_manager, false);
            }
            else
            {
                xset_set_b(xset::name::task_hide_manager, true);
                xset_set_b(xset::name::task_show_manager, false);
            }
        }
        else
        {
            if (xset_get_b(xset::name::task_hide_manager))
            {
                xset_set_b(xset::name::task_show_manager, false);
            }
            else
            {
                xset_set_b(xset::name::task_hide_manager, false);
                xset_set_b(xset::name::task_show_manager, true);
            }
        }
    }

    if (xset_get_b(xset::name::task_show_manager))
    {
        show_task_manager(main_window, true);
    }
    else
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            show_task_manager(main_window, true);
        }
        else if (xset_get_b(xset::name::task_hide_manager))
        {
            show_task_manager(main_window, false);
        }
    }
}

void
ptk_task_view_popup_show(MainWindow* main_window, const std::string_view name)
{
    on_task_popup_show(nullptr, main_window, name.data());
}

static void
on_task_popup_errset(GtkMenuItem* item, MainWindow* main_window, const char* name2)
{
    (void)main_window;
    const char* name = nullptr;
    if (item)
    {
        name = static_cast<const char*>(g_object_get_data(G_OBJECT(item), "name"));
    }
    else
    {
        name = name2;
    }

    if (!name)
    {
        return;
    }

    const xset::name xset_name = xset::get_xsetname_from_name(name);

    if (xset_name == xset::name::task_err_first)
    {
        if (xset_get_b(xset::name::task_err_first))
        {
            xset_set_b(xset::name::task_err_any, false);
            xset_set_b(xset::name::task_err_cont, false);
        }
        else
        {
            xset_set_b(xset::name::task_err_any, false);
            xset_set_b(xset::name::task_err_cont, true);
        }
    }
    else if (xset_name == xset::name::task_err_any)
    {
        if (xset_get_b(xset::name::task_err_any))
        {
            xset_set_b(xset::name::task_err_first, false);
            xset_set_b(xset::name::task_err_cont, false);
        }
        else
        {
            xset_set_b(xset::name::task_err_first, false);
            xset_set_b(xset::name::task_err_cont, true);
        }
    }
    else
    {
        if (xset_get_b(xset::name::task_err_cont))
        {
            xset_set_b(xset::name::task_err_first, false);
            xset_set_b(xset::name::task_err_any, false);
        }
        else
        {
            xset_set_b(xset::name::task_err_first, true);
            xset_set_b(xset::name::task_err_any, false);
        }
    }
}

void
ptk_task_view_popup_errset(MainWindow* main_window, const std::string_view name)
{
    on_task_popup_errset(nullptr, main_window, name.data());
}

void
ptk_task_view_prepare_menu(MainWindow* main_window, GtkWidget* menu)
{
    (void)menu;
    xset_t set;
    xset_t set_radio;

    GtkWidget* parent = main_window->task_view;
    set = xset_get(xset::name::task_show_manager);
    xset_set_cb(set, (GFunc)on_task_popup_show, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, nullptr);
    set_radio = set;
    set = xset_get(xset::name::task_hide_manager);
    xset_set_cb(set, (GFunc)on_task_popup_show, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, set_radio->name.data());

    xset_set_cb(xset::name::task_col_count, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_path, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_file, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_to, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_progress, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_total, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_started, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_elapsed, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_curspeed, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_curest, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_avgspeed, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_avgest, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_reorder, (GFunc)on_reorder, parent);

    set = xset_get(xset::name::task_err_first);
    xset_set_cb(set, (GFunc)on_task_popup_errset, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, nullptr);
    set_radio = set;
    set = xset_get(xset::name::task_err_any);
    xset_set_cb(set, (GFunc)on_task_popup_errset, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set = xset_get(xset::name::task_err_cont);
    xset_set_cb(set, (GFunc)on_task_popup_errset, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, set_radio->name.data());
}

PtkFileTask*
ptk_task_view_get_selected_task(GtkWidget* view)
{
    GtkTreeModel* model = nullptr;
    GtkTreeSelection* selection = nullptr;
    GtkTreeIter it;
    PtkFileTask* ptask = nullptr;

    if (!view)
    {
        return nullptr;
    }
    MainWindow* main_window = get_task_view_window(view);
    if (main_window == nullptr)
    {
        return nullptr;
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    if (gtk_tree_selection_get_selected(selection, nullptr, &it))
    {
        gtk_tree_model_get(model, &it, task_view_column::data, &ptask, -1);
    }
    return ptask;
}

void
ptk_task_view_show_task_dialog(GtkWidget* view)
{
    PtkFileTask* ptask = ptk_task_view_get_selected_task(view);
    if (!ptask)
    {
        return;
    }

    ptk_file_task_lock(ptask);
    ptk_file_task_progress_open(ptask);
    if (ptask->task->state_pause_ != vfs::file_task::state::running)
    {
        // update dlg
        ptask->pause_change = true;
        ptask->progress_count = 50; // trigger fast display
    }
    if (ptask->progress_dlg)
    {
        gtk_window_present(GTK_WINDOW(ptask->progress_dlg));
    }
    ptk_file_task_unlock(ptask);
}

static bool
on_task_button_press_event(GtkWidget* view, GdkEvent* event, MainWindow* main_window)
{
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;
    GtkTreeViewColumn* col = nullptr;
    GtkTreeIter it;
    PtkFileTask* ptask = nullptr;
    xset_t set;
    bool is_tasks = false;

    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);

    if (type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }

    f64 x = NAN;
    f64 y = NAN;
    gdk_event_get_position(event, &x, &y);

    switch (button)
    {
        case 1:
        case 2:
        { // left or middle click
            // get selected task
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
            // ztd::logger::info("x = {}  y = {}", event->x, event->y);
            // due to bug in gtk_tree_view_get_path_at_pos (gtk 2.24), a click
            // on the column header resize divider registers as a click on the
            // first row first column.  So if event->x < 7 ignore
            if (x < 7)
            {
                return false;
            }
            if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                               x,
                                               y,
                                               &tree_path,
                                               &col,
                                               nullptr,
                                               nullptr))
            {
                return false;
            }
            if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
            {
                gtk_tree_model_get(model, &it, task_view_column::data, &ptask, -1);
            }
            gtk_tree_path_free(tree_path);

            if (!ptask)
            {
                return false;
            }
            if (button == 1 && std::strcmp(gtk_tree_view_column_get_title(col), "Status") != 0)
            {
                return false;
            }
            xset::name sname;
            switch (ptask->task->state_pause_)
            {
                case vfs::file_task::state::pause:
                    sname = xset::name::task_que;
                    break;
                case vfs::file_task::state::queue:
                    sname = xset::name::task_resume;
                    break;
                case vfs::file_task::state::running:
                case vfs::file_task::state::size_timeout:
                case vfs::file_task::state::query_overwrite:
                case vfs::file_task::state::error:
                case vfs::file_task::state::finish:
                    sname = xset::name::task_pause;
            }
            set = xset_get(sname);
            on_task_stop(nullptr, view, set, ptask);
            return true;
            break;
        }
        case 3:
        {
            // get selected task
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
            is_tasks = gtk_tree_model_get_iter_first(model, &it);
            if (is_tasks)
            {
                if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                                  x,
                                                  y,
                                                  &tree_path,
                                                  &col,
                                                  nullptr,
                                                  nullptr))
                {
                    if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
                    {
                        gtk_tree_model_get(model, &it, task_view_column::data, &ptask, -1);
                    }
                    gtk_tree_path_free(tree_path);
                }
            }

            // build popup
            ptk::browser* file_browser = nullptr;
            file_browser = main_window->current_file_browser();
            if (!file_browser)
            {
                return false;
            }
            GtkWidget* popup = nullptr;

            popup = gtk_menu_new();

            set = xset_get(xset::name::task_stop);
            xset_set_cb(set, (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = !ptask;

            set = xset_get(xset::name::task_pause);
            xset_set_cb(set, (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = (!ptask || ptask->task->state_pause_ == vfs::file_task::state::pause ||
                            ptask->task->type_ == vfs::file_task::type::exec);

            set = xset_get(xset::name::task_que);
            xset_set_cb(set, (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = (!ptask || ptask->task->state_pause_ == vfs::file_task::state::queue ||
                            ptask->task->type_ == vfs::file_task::type::exec);

            set = xset_get(xset::name::task_resume);
            xset_set_cb(set, (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = (!ptask || ptask->task->state_pause_ == vfs::file_task::state::running ||
                            ptask->task->type_ == vfs::file_task::type::exec);

            xset_set_cb(xset::name::task_stop_all, (GFunc)on_task_stop, view);
            xset_set_cb(xset::name::task_pause_all, (GFunc)on_task_stop, view);
            xset_set_cb(xset::name::task_que_all, (GFunc)on_task_stop, view);
            xset_set_cb(xset::name::task_resume_all, (GFunc)on_task_stop, view);
            set = xset_get(xset::name::task_all);
            set->disable = !is_tasks;

            const std::vector<xset::name> context_menu_entries = {
                xset::name::task_stop,
                xset::name::separator,
                xset::name::task_pause,
                xset::name::task_que,
                xset::name::task_resume,
                xset::name::task_all,
                xset::name::separator,
                xset::name::task_show_manager,
                xset::name::task_hide_manager,
                xset::name::separator,
                xset::name::task_columns,
                xset::name::task_popups,
                xset::name::task_errors,
                xset::name::task_queue,
            };

#if (GTK_MAJOR_VERSION == 4)
            GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
            GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

            ptk_task_view_prepare_menu(main_window, popup);

            xset_add_menu(file_browser, popup, accel_group, context_menu_entries);

            gtk_widget_show_all(GTK_WIDGET(popup));

            // clang-format off
            g_signal_connect(G_OBJECT(popup), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
            // clang-format on

            gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
            // right click
            break;
        }
        default:
            break;
    }

    return false;
}

static void
on_task_row_activated(GtkWidget* view, GtkTreePath* tree_path, GtkTreeViewColumn* col,
                      void* user_data)
{
    (void)col;
    (void)user_data;
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    PtkFileTask* ptask = nullptr;

    MainWindow* main_window = get_task_view_window(view);
    if (main_window == nullptr)
    {
        return;
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (!gtk_tree_model_get_iter(model, &it, tree_path))
    {
        return;
    }

    gtk_tree_model_get(model, &it, task_view_column::data, &ptask, -1);
    if (ptask)
    {
        // show normal dialog
        ptk_task_view_show_task_dialog(view);
    }
}

void
main_task_view_remove_task(PtkFileTask* ptask)
{
    // ztd::logger::info("main_task_view_remove_task  ptask={}", ptask);

    GtkWidget* view = ptask->task_view;
    if (!view)
    {
        return;
    }

    MainWindow* main_window = get_task_view_window(view);
    if (main_window == nullptr)
    {
        return;
    }

    PtkFileTask* ptaskt = nullptr;
    GtkTreeIter it;

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, task_view_column::data, &ptaskt, -1);
        } while (ptaskt != ptask && gtk_tree_model_iter_next(model, &it));
    }
    if (ptaskt == ptask)
    {
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);
    }

    if (!gtk_tree_model_get_iter_first(model, &it))
    {
        if (xset_get_b(xset::name::task_hide_manager))
        {
            show_task_manager(main_window, false);
        }
    }

    // ztd::logger::info("main_task_view_remove_task DONE ptask={}", ptask);
}

void
main_task_view_update_task(PtkFileTask* ptask)
{
    PtkFileTask* ptaskt = nullptr;
    GtkWidget* view = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    xset_t set;

    // ztd::logger::info("main_task_view_update_task  ptask={}", ptask);
    const std::unordered_map<vfs::file_task::type, const std::string_view> job_titles{
        {vfs::file_task::type::move, "moving"},
        {vfs::file_task::type::copy, "copying"},
        {vfs::file_task::type::trash, "trashing"},
        {vfs::file_task::type::del, "deleting"},
        {vfs::file_task::type::link, "linking"},
        {vfs::file_task::type::chmod_chown, "changing"},
        {vfs::file_task::type::exec, "running"},
    };

    if (!ptask)
    {
        return;
    }

    view = ptask->task_view;
    if (!view)
    {
        return;
    }

    MainWindow* main_window = get_task_view_window(view);
    if (main_window == nullptr)
    {
        return;
    }

    std::filesystem::path dest_dir;
    if (ptask->task->type_ != vfs::file_task::type::exec)
    {
        if (ptask->task->dest_dir)
        {
            dest_dir = ptask->task->dest_dir.value();
        }
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, task_view_column::data, &ptaskt, -1);
        } while (ptaskt != ptask && gtk_tree_model_iter_next(model, &it));
    }
    if (ptaskt != ptask)
    {
        // new row
        const auto point = ptask->task->start_time;
        const auto midnight = point - std::chrono::floor<std::chrono::days>(point);
        const auto hours = std::chrono::duration_cast<std::chrono::hours>(midnight);
        const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(midnight - hours);
        const auto started = std::format("{0:%H}:{1:%M}", hours, minutes);

        gtk_list_store_insert_with_values(
            GTK_LIST_STORE(model),
            &it,
            0,
            task_view_column::to,
            dest_dir.empty() ? nullptr : dest_dir.c_str(),
            task_view_column::started,
            started.data(),
            task_view_column::starttime,
            std::chrono::system_clock::to_time_t(ptask->task->start_time),
            task_view_column::data,
            ptask,
            -1);
    }

    if (ptask->task->state_pause_ == vfs::file_task::state::running || ptask->pause_change_view)
    {
        // update row
        std::string path;
        std::string file;

        i32 percent = ptask->task->percent;
        if (percent < 0)
        {
            percent = 0;
        }
        else if (percent > 100)
        {
            percent = 100;
        }
        if (ptask->task->type_ != vfs::file_task::type::exec)
        {
            if (ptask->task->current_file)
            {
                const auto current_file = ptask->task->current_file.value();
                path = current_file.parent_path();
                file = current_file.filename();
            }
        }
        else
        {
            const auto current_file = ptask->task->current_file.value();

            path = ptask->task->dest_dir.value(); // cwd
            file = std::format("( {} )", current_file.string());
        }

        // status
        std::string status;
        std::string status_final;
        if (ptask->task->type_ != vfs::file_task::type::exec)
        {
            if (!ptask->err_count)
            {
                status = job_titles.at(ptask->task->type_);
            }
            else
            {
                status =
                    std::format("{} error {}", ptask->err_count, job_titles.at(ptask->task->type_));
            }
        }
        else
        {
            // exec task
            if (!ptask->task->exec_action.empty())
            {
                status = ptask->task->exec_action;
            }
            else
            {
                status = job_titles.at(ptask->task->type_);
            }
        }

        if (ptask->task->state_pause_ == vfs::file_task::state::pause)
        {
            status_final = std::format("paused {}", status);
        }
        else if (ptask->task->state_pause_ == vfs::file_task::state::queue)
        {
            status_final = std::format("queued {}", status);
        }
        else
        {
            status_final = status;
        }

        // update icon if queue state changed
        GdkPixbuf* pixbuf = nullptr;
        if (ptask->pause_change_view)
        {
            // icon
            if (ptask->task->state_pause_ == vfs::file_task::state::pause)
            {
                set = xset_get(xset::name::task_pause);
                pixbuf = vfs_load_icon(set->icon.value_or("media-playback-pause"), 22);
            }
            else if (ptask->task->state_pause_ == vfs::file_task::state::queue)
            {
                set = xset_get(xset::name::task_que);
                pixbuf = vfs_load_icon(set->icon.value_or("list-add"), 22);
            }
            else if (ptask->err_count && ptask->task->type_ != vfs::file_task::type::exec)
            {
                pixbuf = vfs_load_icon("error", 22);
            }
            else if (ptask->task->type_ == vfs::file_task::type::move ||
                     ptask->task->type_ == vfs::file_task::type::copy ||
                     ptask->task->type_ == vfs::file_task::type::link)
            {
                pixbuf = vfs_load_icon("stock_copy", 22);
            }
            else if (ptask->task->type_ == vfs::file_task::type::trash ||
                     ptask->task->type_ == vfs::file_task::type::del)
            {
                pixbuf = vfs_load_icon("stock_delete", 22);
            }
            else if (ptask->task->type_ == vfs::file_task::type::exec &&
                     !ptask->task->exec_icon.empty())
            {
                pixbuf = vfs_load_icon(ptask->task->exec_icon, 22);
            }
            else
            {
                pixbuf = vfs_load_icon("gtk-execute", 22);
            }

            if (!pixbuf)
            {
                pixbuf = vfs_load_icon("gtk-execute", 22);
            }
            ptask->pause_change_view = false;
        }

        if (ptask->task->type_ != vfs::file_task::type::exec || ptaskt != ptask /* new task */)
        {
            if (pixbuf)
            {
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   task_view_column::icon,
                                   pixbuf,
                                   task_view_column::status,
                                   status_final.data(),
                                   task_view_column::count,
                                   ptask->dsp_file_count.data(),
                                   task_view_column::path,
                                   path.data(),
                                   task_view_column::file,
                                   file.data(),
                                   task_view_column::progress,
                                   percent,
                                   task_view_column::total,
                                   ptask->dsp_size_tally.data(),
                                   task_view_column::elapsed,
                                   ptask->dsp_elapsed.data(),
                                   task_view_column::curspeed,
                                   ptask->dsp_curspeed.data(),
                                   task_view_column::curest,
                                   ptask->dsp_curest.data(),
                                   task_view_column::avgspeed,
                                   ptask->dsp_avgspeed.data(),
                                   task_view_column::avgest,
                                   ptask->dsp_avgest.data(),
                                   -1);
            }
            else
            {
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   task_view_column::status,
                                   status_final.data(),
                                   task_view_column::count,
                                   ptask->dsp_file_count.data(),
                                   task_view_column::path,
                                   path.data(),
                                   task_view_column::file,
                                   file.data(),
                                   task_view_column::progress,
                                   percent,
                                   task_view_column::total,
                                   ptask->dsp_size_tally.data(),
                                   task_view_column::elapsed,
                                   ptask->dsp_elapsed.data(),
                                   task_view_column::curspeed,
                                   ptask->dsp_curspeed.data(),
                                   task_view_column::curest,
                                   ptask->dsp_curest.data(),
                                   task_view_column::avgspeed,
                                   ptask->dsp_avgspeed.data(),
                                   task_view_column::avgest,
                                   ptask->dsp_avgest.data(),
                                   -1);
            }
        }
        else if (pixbuf)
        {
            gtk_list_store_set(GTK_LIST_STORE(model),
                               &it,
                               task_view_column::icon,
                               pixbuf,
                               task_view_column::status,
                               status_final.data(),
                               task_view_column::progress,
                               percent,
                               task_view_column::elapsed,
                               ptask->dsp_elapsed.data(),
                               -1);
        }
        else
        {
            gtk_list_store_set(GTK_LIST_STORE(model),
                               &it,
                               task_view_column::status,
                               status_final.data(),
                               task_view_column::progress,
                               percent,
                               task_view_column::elapsed,
                               ptask->dsp_elapsed.data(),
                               -1);
        }

        // Clearing up
        if (pixbuf)
        {
            g_object_unref(pixbuf);
        }

        if (!gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(view))))
        {
            show_task_manager(main_window, true);
        }
    }
    else
    {
        // task is paused
        gtk_list_store_set(GTK_LIST_STORE(model),
                           &it,
                           task_view_column::total,
                           ptask->dsp_size_tally.data(),
                           task_view_column::elapsed,
                           ptask->dsp_elapsed.data(),
                           task_view_column::curspeed,
                           ptask->dsp_curspeed.data(),
                           task_view_column::curest,
                           ptask->dsp_curest.data(),
                           task_view_column::avgspeed,
                           ptask->dsp_avgspeed.data(),
                           task_view_column::avgest,
                           ptask->dsp_avgest.data(),
                           -1);
    }
    // ztd::logger::info("DONE main_task_view_update_task");
}

GtkWidget*
main_task_view_new(MainWindow* main_window)
{
    GtkTreeViewColumn* col = nullptr;
    GtkCellRenderer* renderer = nullptr;
    GtkCellRenderer* pix_renderer = nullptr;

    static constexpr std::array<task_view_column, 16> cols{
        task_view_column::status,
        task_view_column::count,
        task_view_column::path,
        task_view_column::file,
        task_view_column::to,
        task_view_column::progress,
        task_view_column::total,
        task_view_column::started,
        task_view_column::elapsed,
        task_view_column::curspeed,
        task_view_column::curest,
        task_view_column::avgspeed,
        task_view_column::avgest,
        task_view_column::starttime,
        task_view_column::icon,
        task_view_column::data,
    };

    // Model
    GtkListStore* list = gtk_list_store_new(cols.size(),
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_INT,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_INT64,
                                            GDK_TYPE_PIXBUF,
                                            G_TYPE_POINTER);

    // View
    GtkWidget* view = gtk_tree_view_new();
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(list));
    // gtk_tree_view_set_model adds a ref
    g_object_unref(list);
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(view), true);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), false);

    // Columns
    for (const auto i : std::views::iota(0uz, task_names.size()))
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable(col, true);
        gtk_tree_view_column_set_sizing(col, GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_min_width(col, 20);

        // column order
        i64 j = 0;
        for (const auto [index, value] : std::views::enumerate(task_names))
        {
            if (xset_get_int(value, xset::var::x) == static_cast<i32>(i))
            {
                // column width
                i32 width = xset_get_int(value, xset::var::y);
                if (width == 0)
                {
                    width = 80;
                }
                gtk_tree_view_column_set_fixed_width(col, width);

                j = index;

                break;
            }
        }

        GValue val;

        switch (cols.at(j))
        {
            case task_view_column::status:
                // Icon and Text
                renderer = gtk_cell_renderer_text_new();
                pix_renderer = gtk_cell_renderer_pixbuf_new();
                gtk_tree_view_column_pack_start(col, pix_renderer, false);
                gtk_tree_view_column_pack_end(col, renderer, true);
                gtk_tree_view_column_set_attributes(col,
                                                    pix_renderer,
                                                    "pixbuf",
                                                    task_view_column::icon,
                                                    nullptr);
                gtk_tree_view_column_set_attributes(col,
                                                    renderer,
                                                    "text",
                                                    task_view_column::status,
                                                    nullptr);
                gtk_tree_view_column_set_expand(col, false);
                gtk_tree_view_column_set_sizing(
                    col,
                    GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
                gtk_tree_view_column_set_min_width(col, 60);
                break;
            case task_view_column::progress:
                // Progress Bar
                renderer = gtk_cell_renderer_progress_new();
                gtk_tree_view_column_pack_start(col, renderer, true);
                gtk_tree_view_column_set_attributes(col, renderer, "value", cols.at(j), nullptr);
                break;
            case task_view_column::path:
            case task_view_column::file:
            case task_view_column::to:
                // Text Column
                renderer = gtk_cell_renderer_text_new();
                gtk_tree_view_column_pack_start(col, renderer, true);
                gtk_tree_view_column_set_attributes(col, renderer, "text", cols.at(j), nullptr);

                // ellipsize
                val = GValue();
                g_value_init(&val, G_TYPE_CHAR);
                g_value_set_schar(&val, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
                g_object_set_property(G_OBJECT(renderer), "ellipsize", &val);
                g_value_unset(&val);
                break;
            case task_view_column::count:
            case task_view_column::total:
            case task_view_column::started:
            case task_view_column::elapsed:
            case task_view_column::curspeed:
            case task_view_column::curest:
            case task_view_column::avgspeed:
            case task_view_column::avgest:
            case task_view_column::starttime:
            case task_view_column::icon:
            case task_view_column::data:
                // Text Column
                renderer = gtk_cell_renderer_text_new();
                gtk_tree_view_column_pack_start(col, renderer, true);
                gtk_tree_view_column_set_attributes(col, renderer, "text", cols.at(j), nullptr);
                break;
        }

        gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
        gtk_tree_view_column_set_title(col, task_titles.at(task_view_column(j)).data());
        gtk_tree_view_column_set_reorderable(col, true);
        gtk_tree_view_column_set_visible(col, xset_get_b(task_names.at(j)));
        if (j == task_view_column::file) //|| j == task_view_column::path || j
                                         //== task_view_column::to
        {
            gtk_tree_view_column_set_sizing(col,
                                            GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
            gtk_tree_view_column_set_min_width(col, 20);
            // If set_expand is true, columns flicker and adjustment is
            // difficult during high i/o load on some systems
            gtk_tree_view_column_set_expand(col, false);
        }
    }

    // invisible Starttime col for sorting
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, true);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        task_view_column::starttime,
                                        nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    gtk_tree_view_column_set_title(col, "StartTime");
    gtk_tree_view_column_set_reorderable(col, false);
    gtk_tree_view_column_set_visible(col, false);

    // Sort
    if (GTK_IS_TREE_SORTABLE(list))
    {
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list),
                                             task_view_column::starttime,
                                             GtkSortType::GTK_SORT_ASCENDING);
    }

    // clang-format off
    g_signal_connect(G_OBJECT(view), "row-activated", G_CALLBACK(on_task_row_activated), nullptr);
    g_signal_connect(G_OBJECT(view), "columns-changed", G_CALLBACK(on_task_columns_changed), nullptr);
    g_signal_connect(G_OBJECT(view), "destroy", G_CALLBACK(on_task_destroy), nullptr);
    g_signal_connect(G_OBJECT(view), "button-press-event", G_CALLBACK(on_task_button_press_event), main_window);
    // clang-format on

    return view;
}
