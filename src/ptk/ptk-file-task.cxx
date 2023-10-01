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

#include <span>

#include <array>
#include <vector>

#include <optional>

#include <sys/wait.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "xset/xset.hxx"
#include "xset/xset-dialog.hxx"

#include "settings.hxx"

#include "utils.hxx"

#include "vfs/vfs-time.hxx"
#include "vfs/vfs-utils.hxx"

#include "ptk/ptk-task-view.hxx"

#include "ptk/ptk-file-task.hxx"

static bool on_vfs_file_task_state_cb(vfs::file_task task, vfs::file_task_state state,
                                      void* state_data, void* user_data);

static void query_overwrite(PtkFileTask* ptask);

static void ptk_file_task_update(PtkFileTask* ptask);
static bool ptk_file_task_add_main(PtkFileTask* ptask);
static void on_progress_dlg_response(GtkDialog* dlg, i32 response, PtkFileTask* ptask);
static void save_progress_dialog_size(PtkFileTask* ptask);

PtkFileTask::PtkFileTask(vfs::file_task_type type,
                         const std::span<const std::filesystem::path> src_files,
                         const std::filesystem::path& dest_dir, GtkWindow* parent_window,
                         GtkWidget* task_view)
{
    this->task = vfs_task_new(type, src_files, dest_dir);

    this->task->set_state_callback(on_vfs_file_task_state_cb, this);
    this->parent_window = parent_window;
    this->task_view = task_view;
    this->task->exec_ptask = (void*)this;
    this->progress_dlg = nullptr;
    this->complete = false;
    this->aborted = false;
    this->pause_change = false;
    this->pause_change_view = true;
    this->force_scroll = false;
    this->keep_dlg = false;
    this->err_count = 0;
    if (xset_get_b(xset::name::task_err_any))
    {
        this->err_mode = ptk::file_task::ptask_error::any;
    }
    else if (xset_get_b(xset::name::task_err_first))
    {
        this->err_mode = ptk::file_task::ptask_error::first;
    }
    else
    {
        this->err_mode = ptk::file_task::ptask_error::cont;
    }

    GtkTextIter iter;
    this->log_buf = gtk_text_buffer_new(nullptr);
    this->log_end = gtk_text_mark_new(nullptr, false);
    gtk_text_buffer_get_end_iter(this->log_buf, &iter);
    gtk_text_buffer_add_mark(this->log_buf, this->log_end, &iter);
    this->log_appended = false;
    this->restart_timeout = false;

    this->complete_notify = nullptr;
    this->user_data = nullptr;

    this->progress_count = 0;
    this->pop_handler = nullptr;

    this->query_cond = nullptr;
    this->query_cond_last = nullptr;
    this->query_new_dest = nullptr;

    // queue task
    if (this->task->exec_sync && this->task->type != vfs::file_task_type::exec &&
        this->task->type != vfs::file_task_type::link &&
        this->task->type != vfs::file_task_type::chmod_chown && xset_get_b(xset::name::task_q_new))
    {
        ptk_file_task_pause(this, vfs::file_task_state::queue);
    }

    // GThread *self = g_thread_self ();
    // ztd::logger::info("GUI_THREAD = {}", fmt::ptr(self));
    // ztd::logger::info("ptk_file_task_new DONE ptask={}", fmt::ptr(this));
}

PtkFileTask::~PtkFileTask()
{
    // ztd::logger::info("ptk_file_task_destroy ptask={}", fmt::ptr(ptask));
    if (this->timeout)
    {
        g_source_remove(this->timeout);
        this->timeout = 0;
    }
    if (this->progress_timer)
    {
        g_source_remove(this->progress_timer);
        this->progress_timer = 0;
    }
    main_task_view_remove_task(this);
    main_task_start_queued(this->task_view, nullptr);

    if (this->progress_dlg)
    {
        save_progress_dialog_size(this);

        if (this->overwrite_combo)
        {
            gtk_combo_box_popdown(GTK_COMBO_BOX(this->overwrite_combo));
        }
        if (this->error_combo)
        {
            gtk_combo_box_popdown(GTK_COMBO_BOX(this->error_combo));
        }
        gtk_widget_destroy(this->progress_dlg);
        this->progress_dlg = nullptr;
    }
    if (this->task->type == vfs::file_task_type::exec)
    {
        // ztd::logger::info("    g_io_channel_shutdown");
        // channel shutdowns are needed to stop channel reads after task ends.
        // Cannot be placed in cb_exec_child_watch because it causes single
        // line output to be lost
        if (this->task->exec_channel_out)
        {
            g_io_channel_shutdown(this->task->exec_channel_out, true, nullptr);
        }
        if (this->task->exec_channel_err)
        {
            g_io_channel_shutdown(this->task->exec_channel_err, true, nullptr);
        }
        this->task->exec_channel_out = this->task->exec_channel_err = nullptr;
        if (this->task->child_watch)
        {
            g_source_remove(this->task->child_watch);
            this->task->child_watch = 0;
        }
        // ztd::logger::info("    g_io_channel_shutdown DONE");
    }

    if (this->task)
    {
        vfs_file_task_free(this->task);
    }

    gtk_text_buffer_set_text(this->log_buf, "", -1);
    g_object_unref(this->log_buf);

    if (this->pop_handler)
    {
        std::free(this->pop_handler);
    }

    // ztd::logger::info("ptk_file_task_destroy DONE ptask={}", fmt::ptr(ptask));
}

void
ptk_file_task_lock(PtkFileTask* ptask)
{
    g_mutex_lock(ptask->task->mutex);
}

void
ptk_file_task_unlock(PtkFileTask* ptask)
{
    g_mutex_unlock(ptask->task->mutex);
}

static bool
ptk_file_task_trylock(PtkFileTask* ptask)
{
    return g_mutex_trylock(ptask->task->mutex);
}

PtkFileTask*
ptk_file_task_new(vfs::file_task_type type, const std::span<const std::filesystem::path> src_files,
                  GtkWindow* parent_window, GtkWidget* task_view)
{
    const auto ptask = new PtkFileTask(type, src_files, "", parent_window, task_view);

    return ptask;
}

PtkFileTask*
ptk_file_task_new(vfs::file_task_type type, const std::span<const std::filesystem::path> src_files,
                  const std::filesystem::path& dest_dir, GtkWindow* parent_window,
                  GtkWidget* task_view)
{
    const auto ptask = new PtkFileTask(type, src_files, dest_dir, parent_window, task_view);

    return ptask;
}

PtkFileTask*
ptk_file_exec_new(const std::string_view item_name, GtkWidget* parent, GtkWidget* task_view)
{
    GtkWidget* parent_win = nullptr;
    if (parent)
    {
        parent_win = gtk_widget_get_toplevel(GTK_WIDGET(parent));
    }

    const std::vector<std::filesystem::path> file_list{item_name.data()};
    const auto ptask = new PtkFileTask(vfs::file_task_type::exec,
                                       file_list,
                                       "",
                                       GTK_WINDOW(parent_win),
                                       task_view);

    return ptask;
}

PtkFileTask*
ptk_file_exec_new(const std::string_view item_name, const std::filesystem::path& dest_dir,
                  GtkWidget* parent, GtkWidget* task_view)
{
    GtkWidget* parent_win = nullptr;
    if (parent)
    {
        parent_win = gtk_widget_get_toplevel(GTK_WIDGET(parent));
    }

    const std::vector<std::filesystem::path> file_list{item_name.data()};
    const auto ptask = new PtkFileTask(vfs::file_task_type::exec,
                                       file_list,
                                       dest_dir,
                                       GTK_WINDOW(parent_win),
                                       task_view);

    return ptask;
}

static void
save_progress_dialog_size(PtkFileTask* ptask)
{
    (void)ptask;
#if 0
    // save dialog size  - do this here now because as of GTK 3.8,
    // allocation == 1,1 in destroy event
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(ptask->progress_dlg), &allocation);

    const std::string width = std::to_string(allocation.width);
    if (ptask->task->type == vfs::file_task_type::EXEC)
    {
        xset_set(xset::name::task_pop_top, xset::var::s, width);
    }
    else
    {
        xset_set(xset::name::task_pop_top, xset::var::x, width);
    }

    const std::string height = std::to_string(allocation.height);
    if (ptask->task->type == vfs::file_task_type::EXEC)
    {
        xset_set(xset::name::task_pop_top, xset::var::z, height);
    }
    else
    {
        xset_set(xset::name::task_pop_top, xset::var::y, height);
    }
#endif
}

void
ptk_file_task_set_complete_notify(PtkFileTask* ptask, GFunc callback, void* user_data)
{
    ptask->complete_notify = callback;
    ptask->user_data = user_data;
}

static bool
on_progress_timer(PtkFileTask* ptask)
{
    // GThread *self = g_thread_self ();
    // ztd::logger::info("PROGRESS_TIMER_THREAD = {}", fmt::ptr(self));

    // query condition?
    if (ptask->query_cond && ptask->query_cond != ptask->query_cond_last)
    {
        // ztd::logger::info("QUERY = {}  mutex = {}", fmt::ptr(ptask->query_cond), fmt::ptr(ptask->task->mutex));
        ptask->restart_timeout = (ptask->timeout != 0);
        if (ptask->timeout)
        {
            g_source_remove(ptask->timeout);
            ptask->timeout = 0;
        }
        if (ptask->progress_timer)
        {
            g_source_remove(ptask->progress_timer);
            ptask->progress_timer = 0;
        }

        ptk_file_task_lock(ptask);
        query_overwrite(ptask);
        ptk_file_task_unlock(ptask);
        return false;
    }

    // start new queued task
    if (ptask->task->queue_start)
    {
        ptask->task->queue_start = false;
        if (ptask->task->state_pause == vfs::file_task_state::running)
        {
            ptk_file_task_pause(ptask, vfs::file_task_state::running);
        }
        else
        {
            main_task_start_queued(ptask->task_view, ptask);
        }
        if (ptask->timeout && ptask->task->state_pause != vfs::file_task_state::running &&
            ptask->task->state == vfs::file_task_state::running)
        {
            // task is waiting in queue so list it
            g_source_remove(ptask->timeout);
            ptask->timeout = 0;
        }
    }

    // only update every 300ms (6 * 50ms)
    if (++ptask->progress_count < 6)
    {
        return true;
    }
    ptask->progress_count = 0;
    // ztd::logger::info("on_progress_timer ptask={}", fmt::ptr(ptask));

    if (ptask->complete)
    {
        if (ptask->progress_timer)
        {
            g_source_remove(ptask->progress_timer);
            ptask->progress_timer = 0;
        }
        if (ptask->complete_notify)
        {
            ptask->complete_notify(ptask->task, ptask->user_data);
            ptask->complete_notify = nullptr;
        }
        main_task_view_remove_task(ptask);
        main_task_start_queued(ptask->task_view, nullptr);
    }
    else if (ptask->task->state_pause != vfs::file_task_state::running && !ptask->pause_change &&
             ptask->task->type != vfs::file_task_type::exec)
    {
        return true;
    }

    ptk_file_task_update(ptask);

    if (ptask->complete)
    {
        if (!ptask->progress_dlg || (!ptask->err_count && !ptask->keep_dlg))
        {
            delete ptask;
            // ztd::logger::info("on_progress_timer DONE false-COMPLETE ptask={}", fmt::ptr(ptask));
            return false;
        }
        else if (ptask->progress_dlg && ptask->err_count)
        {
            gtk_window_present(GTK_WINDOW(ptask->progress_dlg));
        }
    }
    // ztd::logger::info("on_progress_timer DONE true ptask={}", fmt::ptr(ptask));
    return !ptask->complete;
}

static bool
ptk_file_task_add_main(PtkFileTask* ptask)
{
    // ztd::logger::info("ptk_file_task_add_main ptask={}", fmt::ptr(ptask));
    if (ptask->timeout)
    {
        g_source_remove(ptask->timeout);
        ptask->timeout = 0;
    }

    if (ptask->task->exec_popup || xset_get_b(xset::name::task_pop_all))
    {
        // keep dlg if Popup Task is explicitly checked, otherwise close if no
        // error
        ptask->keep_dlg = ptask->keep_dlg || ptask->task->exec_popup;
        ptk_file_task_progress_open(ptask);
    }

    if (ptask->task->state_pause != vfs::file_task_state::running && !ptask->pause_change)
    {
        ptask->pause_change = ptask->pause_change_view = true;
    }

    on_progress_timer(ptask);

    // ztd::logger::info("ptk_file_task_add_main DONE ptask={}", fmt::ptr(ptask));
    return false;
}

void
ptk_file_task_run(PtkFileTask* ptask)
{
    // ztd::logger::info("ptk_file_task_run ptask={}", fmt::ptr(ptask));
    // wait this long to first show task in manager, popup
    ptask->timeout = g_timeout_add(500, (GSourceFunc)ptk_file_task_add_main, ptask);
    ptask->progress_timer = 0;
    ptask->task->run_task();
    if (ptask->task->type == vfs::file_task_type::exec)
    {
        if ((ptask->complete || !ptask->task->exec_sync) && ptask->timeout)
        {
            g_source_remove(ptask->timeout);
            ptask->timeout = 0;
        }
    }
    ptask->progress_timer = g_timeout_add(50, (GSourceFunc)on_progress_timer, ptask);
    // ztd::logger::info("ptk_file_task_run DONE ptask={}", fmt::ptr(ptask));
}

bool
ptk_file_task_cancel(PtkFileTask* ptask)
{
    // GThread *self = g_thread_self ();
    // ztd::logger::info("CANCEL_THREAD = {}", fmt::ptr(self));
    if (ptask->timeout)
    {
        g_source_remove(ptask->timeout);
        ptask->timeout = 0;
    }
    ptask->aborted = true;
    if (ptask->task->type == vfs::file_task_type::exec)
    {
        ptask->keep_dlg = true;

        // resume task for task list responsiveness
        if (ptask->task->state_pause != vfs::file_task_state::running)
        {
            ptk_file_task_pause(ptask, vfs::file_task_state::running);
        }

        ptask->task->abort_task();

        if (ptask->task->exec_pid)
        {
            // ztd::logger::info("SIGTERM {}", ptask->task->exec_pid);

            const std::string command =
                std::format("/usr/bin/kill -{} {}", SIGTERM, ptask->task->exec_pid);
            ztd::logger::info("COMMAND={}", command);
            Glib::spawn_command_line_async(command);

            const std::string command2 =
                std::format("sleep 5 && /usr/bin/kill -{} {}", SIGKILL, ptask->task->exec_pid);
            ztd::logger::info("COMMAND={}", command2);
            Glib::spawn_command_line_async(command2);
        }
        else
        {
            // no pid (exited)
            // user pressed Stop on an exited process, remove task
            // this may be needed because if process is killed, channels may not
            // receive HUP and may remain open, leaving the task listed
            ptask->complete = true;
        }

        if (ptask->task->exec_cond)
        {
            // this is used only if exec task run in non-main loop thread
            ptk_file_task_lock(ptask);
            if (ptask->task->exec_cond)
            {
                g_cond_broadcast(ptask->task->exec_cond);
            }
            ptk_file_task_unlock(ptask);
        }
    }
    else
    {
        ptask->task->try_abort_task();
    }
    return false;
}

static void
set_button_states(PtkFileTask* ptask)
{
    if (!ptask->progress_dlg)
    {
        return;
    }

    std::string label;

    switch (ptask->task->state_pause)
    {
        case vfs::file_task_state::pause:
            label = "Q_ueue";
            // iconset = ztd::strdup("task_que");
            //  icon = "list-add";
            break;
        case vfs::file_task_state::queue:
            label = "Res_ume";
            // iconset = ztd::strdup("task_resume");
            //  icon = "media-playback-start";
            break;
        case vfs::file_task_state::running:
        case vfs::file_task_state::size_timeout:
        case vfs::file_task_state::query_overwrite:
        case vfs::file_task_state::error:
        case vfs::file_task_state::finish:
            label = "Pa_use";
            // iconset = ztd::strdup("task_pause");
            //  icon = "media-playback-pause";
            break;
    }
    const bool sens = !ptask->complete &&
                      !(ptask->task->type == vfs::file_task_type::exec && !ptask->task->exec_pid);

    gtk_widget_set_sensitive(GTK_WIDGET(ptask->progress_btn_pause), sens);
    gtk_button_set_label(ptask->progress_btn_pause, label.data());
    gtk_widget_set_sensitive(GTK_WIDGET(ptask->progress_btn_close),
                             ptask->complete || !!ptask->task_view);
}

void
ptk_file_task_pause(PtkFileTask* ptask, vfs::file_task_state state)
{
    if (ptask->task->type == vfs::file_task_type::exec)
    {
        // exec task
        // ptask->keep_dlg = true;
        i32 sig;
        if (state == vfs::file_task_state::pause ||
            (ptask->task->state_pause == vfs::file_task_state::running &&
             state == vfs::file_task_state::queue))
        {
            sig = SIGSTOP;
            ptask->task->state_pause = (vfs::file_task_state)state;
            ptask->task->timer.stop();
        }
        else if (state == vfs::file_task_state::queue)
        {
            sig = 0;
            ptask->task->state_pause = (vfs::file_task_state)state;
        }
        else
        {
            sig = SIGCONT;
            ptask->task->state_pause = vfs::file_task_state::running;
            ptask->task->timer.start();
        }

        if (sig && ptask->task->exec_pid)
        {
            // send signal
            const std::string command =
                std::format("/usr/bin/kill -{} {}", sig, ptask->task->exec_pid);
            ztd::logger::info("COMMAND={}", command);
            Glib::spawn_command_line_async(command);
        }
    }
    else if (state == vfs::file_task_state::pause)
    {
        ptask->task->state_pause = vfs::file_task_state::pause;
    }
    else if (state == vfs::file_task_state::queue)
    {
        ptask->task->state_pause = vfs::file_task_state::queue;
    }
    else
    {
        // Resume
        if (ptask->task->pause_cond)
        {
            ptk_file_task_lock(ptask);
            g_cond_broadcast(ptask->task->pause_cond);
            ptk_file_task_unlock(ptask);
        }
        ptask->task->state_pause = vfs::file_task_state::running;
    }
    set_button_states(ptask);
    ptask->pause_change = ptask->pause_change_view = true;
    ptask->progress_count = 50; // trigger fast display
}

static bool
on_progress_dlg_delete_event(GtkWidget* widget, GdkEvent* event, PtkFileTask* ptask)
{
    (void)widget;
    (void)event;
    save_progress_dialog_size(ptask);
    return !(ptask->complete || ptask->task_view);
}

static void
on_progress_dlg_response(GtkDialog* dlg, i32 response, PtkFileTask* ptask)
{
    (void)dlg;
    save_progress_dialog_size(ptask);
    if (ptask->complete && !ptask->complete_notify)
    {
        delete ptask;
        return;
    }
    switch (response)
    {
        case GtkResponseType::GTK_RESPONSE_CANCEL: // Stop btn
            ptask->keep_dlg = false;
            if (ptask->overwrite_combo)
            {
                gtk_combo_box_popdown(GTK_COMBO_BOX(ptask->overwrite_combo));
            }
            if (ptask->error_combo)
            {
                gtk_combo_box_popdown(GTK_COMBO_BOX(ptask->error_combo));
            }
            gtk_widget_destroy(ptask->progress_dlg);
            ptask->progress_dlg = nullptr;
            ptk_file_task_cancel(ptask);
            break;
        case GtkResponseType::GTK_RESPONSE_NO: // Pause btn
            if (ptask->task->state_pause == vfs::file_task_state::pause)
            {
                ptk_file_task_pause(ptask, vfs::file_task_state::queue);
            }
            else if (ptask->task->state_pause == vfs::file_task_state::queue)
            {
                ptk_file_task_pause(ptask, vfs::file_task_state::running);
            }
            else
            {
                ptk_file_task_pause(ptask, vfs::file_task_state::pause);
            }
            main_task_start_queued(ptask->task_view, nullptr);
            break;
        case GtkResponseType::GTK_RESPONSE_OK:
        case GtkResponseType::GTK_RESPONSE_NONE:
            ptask->keep_dlg = false;
            if (ptask->overwrite_combo)
            {
                gtk_combo_box_popdown(GTK_COMBO_BOX(ptask->overwrite_combo));
            }
            if (ptask->error_combo)
            {
                gtk_combo_box_popdown(GTK_COMBO_BOX(ptask->error_combo));
            }
            gtk_widget_destroy(ptask->progress_dlg);
            ptask->progress_dlg = nullptr;
            break;
        default:
            break;
    }
}

static void
on_progress_dlg_destroy(GtkDialog* dlg, PtkFileTask* ptask)
{
    (void)dlg;
    ptask->progress_dlg = nullptr;
}

static void
on_view_popup(GtkTextView* entry, GtkMenu* menu, void* user_data)
{
    (void)entry;
    (void)user_data;

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

    xset_t set = xset_get(xset::name::separator);
    set->browser = nullptr;
    xset_add_menuitem(nullptr, GTK_WIDGET(menu), accel_group, set);
    gtk_widget_show_all(GTK_WIDGET(menu));
}

static void
set_progress_icon(PtkFileTask* ptask)
{
    GdkPixbuf* pixbuf;
    vfs::file_task task = ptask->task;
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();

    if (task->state_pause != vfs::file_task_state::running)
    {
        pixbuf = gtk_icon_theme_load_icon(icon_theme,
                                          "media-playback-pause",
                                          16,
                                          GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN,
                                          nullptr);
    }
    else if (task->err_count)
    {
        pixbuf = gtk_icon_theme_load_icon(icon_theme,
                                          "error",
                                          16,
                                          GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN,
                                          nullptr);
    }
    else if (task->type == vfs::file_task_type::move || task->type == vfs::file_task_type::copy ||
             task->type == vfs::file_task_type::link || task->type == vfs::file_task_type::trash)
    {
        pixbuf = gtk_icon_theme_load_icon(icon_theme,
                                          "stock_copy",
                                          16,
                                          GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN,
                                          nullptr);
    }
    else if (task->type == vfs::file_task_type::DELETE)
    {
        pixbuf = gtk_icon_theme_load_icon(icon_theme,
                                          "stock_delete",
                                          16,
                                          GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN,
                                          nullptr);
    }
    else if (task->type == vfs::file_task_type::exec && !task->exec_icon.empty())
    {
        pixbuf = gtk_icon_theme_load_icon(icon_theme,
                                          task->exec_icon.data(),
                                          16,
                                          GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN,
                                          nullptr);
    }
    else
    {
        pixbuf = gtk_icon_theme_load_icon(icon_theme,
                                          "gtk-execute",
                                          16,
                                          GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN,
                                          nullptr);
    }
    gtk_window_set_icon(GTK_WINDOW(ptask->progress_dlg), pixbuf);
}

static void
on_overwrite_combo_changed(GtkComboBox* box, PtkFileTask* ptask)
{
    i32 overwrite_mode = gtk_combo_box_get_active(box);
    if (overwrite_mode < 0)
    {
        overwrite_mode = 0;
    }
    ptask->task->set_overwrite_mode(vfs::file_task_overwrite_mode(overwrite_mode));
}

static void
on_error_combo_changed(GtkComboBox* box, PtkFileTask* ptask)
{
    i32 error_mode = gtk_combo_box_get_active(box);
    if (error_mode < 0)
    {
        error_mode = 0;
    }
    ptask->err_mode = ptk::file_task::ptask_error(error_mode);
}

void
ptk_file_task_progress_open(PtkFileTask* ptask)
{
    const std::map<vfs::file_task_type, const std::string_view> job_actions{
        {vfs::file_task_type::move, "Move: "},
        {vfs::file_task_type::copy, "Copy: "},
        {vfs::file_task_type::trash, "Trash: "},
        {vfs::file_task_type::DELETE, "Delete: "},
        {vfs::file_task_type::link, "Link: "},
        {vfs::file_task_type::chmod_chown, "Change: "},
        {vfs::file_task_type::exec, "Run: "},
    };
    const std::map<vfs::file_task_type, const std::string_view> job_titles{
        {vfs::file_task_type::move, "Moving..."},
        {vfs::file_task_type::copy, "Copying..."},
        {vfs::file_task_type::trash, "Trashing..."},
        {vfs::file_task_type::DELETE, "Deleting..."},
        {vfs::file_task_type::link, "Linking..."},
        {vfs::file_task_type::chmod_chown, "Changing..."},
        {vfs::file_task_type::exec, "Running..."},
    };

    if (ptask->progress_dlg)
    {
        return;
    }

    // ztd::logger::info("ptk_file_task_progress_open");

    vfs::file_task task = ptask->task;

    // create dialog
    ptask->progress_dlg =
        gtk_dialog_new_with_buttons(job_titles.at(task->type).data(),
                                    ptask->parent_window,
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    nullptr,
                                    nullptr);

    gtk_window_set_resizable(GTK_WINDOW(ptask->progress_dlg), false);

    // cache this value for speed
    ptask->pop_detail = xset_get_b(xset::name::task_pop_detail);

    // Buttons
    // Pause
    // xset_t set = xset_get(xset::name::TASK_PAUSE);

    ptask->progress_btn_pause = GTK_BUTTON(gtk_button_new_with_mnemonic("Pa_use"));

    gtk_dialog_add_action_widget(GTK_DIALOG(ptask->progress_dlg),
                                 GTK_WIDGET(ptask->progress_btn_pause),
                                 GtkResponseType::GTK_RESPONSE_NO);
    gtk_widget_set_focus_on_click(GTK_WIDGET(ptask->progress_btn_pause), false);
    // Stop
    ptask->progress_btn_stop = GTK_BUTTON(gtk_button_new_with_label("Stop"));
    gtk_dialog_add_action_widget(GTK_DIALOG(ptask->progress_dlg),
                                 GTK_WIDGET(ptask->progress_btn_stop),
                                 GtkResponseType::GTK_RESPONSE_CANCEL);
    gtk_widget_set_focus_on_click(GTK_WIDGET(ptask->progress_btn_stop), false);
    // Close
    ptask->progress_btn_close = GTK_BUTTON(gtk_button_new_with_label("Close"));
    gtk_dialog_add_action_widget(GTK_DIALOG(ptask->progress_dlg),
                                 GTK_WIDGET(ptask->progress_btn_close),
                                 GtkResponseType::GTK_RESPONSE_OK);
    gtk_widget_set_sensitive(GTK_WIDGET(ptask->progress_btn_close), !!ptask->task_view);

    set_button_states(ptask);

    GtkGrid* grid = GTK_GRID(gtk_grid_new());

    gtk_container_set_border_width(GTK_CONTAINER(grid), 5);
    gtk_grid_set_row_spacing(grid, 6);
    gtk_grid_set_column_spacing(grid, 4);
    i32 row = 0;

    /* Copy/Move/Link: */
    GtkLabel* label = GTK_LABEL(gtk_label_new(job_actions.at(task->type).data()));
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
    ptask->from =
        GTK_LABEL(gtk_label_new(ptask->complete ? "" : task->current_file.value_or("").c_str()));
    gtk_widget_set_halign(GTK_WIDGET(ptask->from), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(ptask->from), GtkAlign::GTK_ALIGN_CENTER);
    gtk_label_set_ellipsize(ptask->from, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable(ptask->from, true);
    gtk_grid_attach(grid, GTK_WIDGET(ptask->from), 1, row, 1, 1);

    if (task->type != vfs::file_task_type::exec)
    {
        // From: <src directory>
        row++;
        label = GTK_LABEL(gtk_label_new("From:"));
        gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
        gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
        ptask->src_dir = GTK_LABEL(gtk_label_new(nullptr));
        gtk_widget_set_halign(GTK_WIDGET(ptask->src_dir), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(ptask->src_dir), GtkAlign::GTK_ALIGN_CENTER);
        gtk_label_set_ellipsize(ptask->src_dir, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_selectable(ptask->src_dir, true);
        gtk_grid_attach(grid, GTK_WIDGET(ptask->src_dir), 1, row, 1, 1);
        if (task->dest_dir)
        {
            const auto dest_dir = task->dest_dir.value();
            /* To: <Destination directory>
            ex. Copy file to..., Move file to...etc. */
            row++;
            label = GTK_LABEL(gtk_label_new("To:"));
            gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
            gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
            gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
            ptask->to = GTK_LABEL(gtk_label_new(dest_dir.c_str()));
            gtk_widget_set_halign(GTK_WIDGET(ptask->to), GtkAlign::GTK_ALIGN_START);
            gtk_widget_set_valign(GTK_WIDGET(ptask->to), GtkAlign::GTK_ALIGN_CENTER);
            gtk_label_set_ellipsize(ptask->to, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
            gtk_label_set_selectable(ptask->to, true);
            gtk_grid_attach(grid, GTK_WIDGET(ptask->to), 1, row, 1, 1);
        }
        else
        {
            ptask->to = nullptr;
        }

        // Stats
        row++;
        label = GTK_LABEL(gtk_label_new("Progress:  "));
        gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
        gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
        ptask->current = GTK_LABEL(gtk_label_new(""));
        gtk_widget_set_halign(GTK_WIDGET(ptask->current), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(ptask->current), GtkAlign::GTK_ALIGN_CENTER);
        gtk_label_set_ellipsize(ptask->current, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_selectable(ptask->current, true);
        gtk_grid_attach(grid, GTK_WIDGET(ptask->current), 1, row, 1, 1);
    }
    else
    {
        ptask->src_dir = nullptr;
        ptask->to = nullptr;
    }

    // Status
    row++;
    label = GTK_LABEL(gtk_label_new("Status:  "));
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
    std::string status;
    if (task->state_pause == vfs::file_task_state::pause)
    {
        status = "Paused";
    }
    else if (task->state_pause == vfs::file_task_state::queue)
    {
        status = "Queued";
    }
    else
    {
        status = "Running...";
    }
    ptask->errors = GTK_LABEL(gtk_label_new(status.data()));
    gtk_widget_set_halign(GTK_WIDGET(ptask->errors), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(ptask->errors), GtkAlign::GTK_ALIGN_CENTER);
    gtk_label_set_ellipsize(ptask->errors, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable(ptask->errors, true);
    gtk_grid_attach(grid, GTK_WIDGET(ptask->errors), 1, row, 1, 1);

    /* Progress: */
    row++;
    ptask->progress_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(ptask->progress_bar), true);
    gtk_progress_bar_set_pulse_step(ptask->progress_bar, 0.08);
    gtk_grid_attach(grid, GTK_WIDGET(ptask->progress_bar), 0, row, 1, 1);

    // Error log
    ptask->scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    gtk_widget_set_halign(GTK_WIDGET(ptask->scroll), GtkAlign::GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(ptask->scroll), GtkAlign::GTK_ALIGN_END);
    // gtk_widget_set_hexpand(GTK_WIDGET(ptask->scroll), false);
    // gtk_widget_set_vexpand(GTK_WIDGET(ptask->scroll), false);
    gtk_widget_set_margin_top(GTK_WIDGET(ptask->scroll), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(ptask->scroll), 0);
    gtk_widget_set_margin_start(GTK_WIDGET(ptask->scroll), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(ptask->scroll), 5);
    ptask->error_view = gtk_text_view_new_with_buffer(ptask->log_buf);
    // gtk_widget_set_halign(GTK_WIDGET(ptask->error_view), GtkAlign::GTK_ALIGN_END);
    // gtk_widget_set_valign(GTK_WIDGET(ptask->error_view), GtkAlign::GTK_ALIGN_END);
    // gtk_widget_set_hexpand(GTK_WIDGET(ptask->error_view), false);
    // gtk_widget_set_vexpand(GTK_WIDGET(ptask->error_view), false);
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request(GTK_WIDGET(ptask->error_view), 600, 300);
    gtk_widget_set_size_request(GTK_WIDGET(ptask->scroll), 600, 300);
    gtk_container_add(GTK_CONTAINER(ptask->scroll), ptask->error_view);
    gtk_scrolled_window_set_policy(ptask->scroll,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ptask->error_view), GtkWrapMode::GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(ptask->error_view), false);

    // clang-format off
    g_signal_connect(G_OBJECT(ptask->error_view), "populate-popup", G_CALLBACK(on_view_popup), nullptr);
    // clang-format on

    // Overwrite & Error
    GtkBox* overwrite_box = nullptr;
    if (task->type != vfs::file_task_type::exec)
    {
        static constexpr std::array<const std::string_view, 4> overwrite_options{
            "Ask",
            "Overwrite All",
            "Skip All",
            "Auto Rename",
        };
        static constexpr std::array<const std::string_view, 3> error_options{
            "Stop If Error First",
            "Stop On Any Error",
            "Continue",
        };

        const bool overtask = task->type == vfs::file_task_type::move ||
                              task->type == vfs::file_task_type::copy ||
                              task->type == vfs::file_task_type::link;
        ptask->overwrite_combo = gtk_combo_box_text_new();
        gtk_widget_set_focus_on_click(GTK_WIDGET(ptask->overwrite_combo), false);
        gtk_widget_set_sensitive(ptask->overwrite_combo, overtask);
        for (const std::string_view overwrite_option : overwrite_options)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ptask->overwrite_combo),
                                           overwrite_option.data());
        }
        if (overtask)
        {
            gtk_combo_box_set_active(
                GTK_COMBO_BOX(ptask->overwrite_combo),
                (task->overwrite_mode == vfs::file_task_overwrite_mode::overwrite ||
                 task->overwrite_mode == vfs::file_task_overwrite_mode::overwrite_all ||
                 task->overwrite_mode == vfs::file_task_overwrite_mode::skip_all ||
                 task->overwrite_mode == vfs::file_task_overwrite_mode::auto_rename)
                    ? magic_enum::enum_integer(task->overwrite_mode)
                    : 0);
        }
        // clang-format off
        g_signal_connect(G_OBJECT(ptask->overwrite_combo), "changed", G_CALLBACK(on_overwrite_combo_changed), ptask);
        // clang-format on

        ptask->error_combo = gtk_combo_box_text_new();
        gtk_widget_set_focus_on_click(GTK_WIDGET(ptask->error_combo), false);
        for (const std::string_view error_option : error_options)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ptask->error_combo),
                                           error_option.data());
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(ptask->error_combo),
                                 magic_enum::enum_integer(ptask->err_mode));

        // clang-format off
        g_signal_connect(G_OBJECT(ptask->error_combo), "changed", G_CALLBACK(on_error_combo_changed), ptask);
        // clang-format on

        overwrite_box = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 20));
        gtk_box_pack_start(overwrite_box, GTK_WIDGET(ptask->overwrite_combo), false, true, 0);
        gtk_box_pack_start(overwrite_box, GTK_WIDGET(ptask->error_combo), false, true, 0);

        gtk_widget_set_halign(GTK_WIDGET(overwrite_box), GtkAlign::GTK_ALIGN_END);
        gtk_widget_set_valign(GTK_WIDGET(overwrite_box), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_hexpand(GTK_WIDGET(overwrite_box), true);
        gtk_widget_set_vexpand(GTK_WIDGET(overwrite_box), true);
        gtk_widget_set_margin_top(GTK_WIDGET(overwrite_box), 0);
        gtk_widget_set_margin_bottom(GTK_WIDGET(overwrite_box), 0);
        gtk_widget_set_margin_start(GTK_WIDGET(overwrite_box), 5);
        gtk_widget_set_margin_end(GTK_WIDGET(overwrite_box), 5);
    }
    else
    {
        ptask->overwrite_combo = nullptr;
        ptask->error_combo = nullptr;
    }

    // Pack
    GtkWidget* progress_dlg = gtk_dialog_get_content_area(GTK_DIALOG(ptask->progress_dlg));
    gtk_widget_set_hexpand(GTK_WIDGET(progress_dlg), true);
    gtk_widget_set_vexpand(GTK_WIDGET(progress_dlg), true);

    gtk_box_pack_start(GTK_BOX(progress_dlg), GTK_WIDGET(grid), false, true, 0);
    gtk_box_pack_start(GTK_BOX(progress_dlg), GTK_WIDGET(ptask->scroll), true, true, 0);

    if (overwrite_box)
    {
        gtk_box_pack_start(GTK_BOX(progress_dlg), GTK_WIDGET(overwrite_box), false, true, 5);
    }

#if 0
    i32 win_width, win_height;
    if (task->type == vfs::file_task_type::EXEC)
    {
        win_width = xset_get_int(xset::name::task_pop_top, xset::var::s);
        win_height = xset_get_int(xset::name::task_pop_top, xset::var::z);
    }
    else
    {
        win_width = xset_get_int(xset::name::task_pop_top, xset::var::x);
        win_height = xset_get_int(xset::name::task_pop_top, xset::var::y);
    }
    if (!win_width)
    {
        win_width = 750;
    }
    if (!win_height)
    {
        win_height = -1;
    }
    gtk_window_set_default_size(GTK_WINDOW(ptask->progress_dlg), win_width, win_height);
#endif

    gtk_button_box_set_layout(GTK_BUTTON_BOX(ptask->progress_dlg),
                              GtkButtonBoxStyle::GTK_BUTTONBOX_END);
    if (xset_get_b(xset::name::task_pop_top))
    {
        gtk_window_set_type_hint(GTK_WINDOW(ptask->progress_dlg),
                                 GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
    }
    else
    {
        gtk_window_set_type_hint(GTK_WINDOW(ptask->progress_dlg),
                                 GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_NORMAL);
    }
    if (xset_get_b(xset::name::task_pop_above))
    {
        gtk_window_set_keep_above(GTK_WINDOW(ptask->progress_dlg), true);
    }
    if (xset_get_b(xset::name::task_pop_stick))
    {
        gtk_window_stick(GTK_WINDOW(ptask->progress_dlg));
    }
    gtk_window_set_gravity(GTK_WINDOW(ptask->progress_dlg), GdkGravity::GDK_GRAVITY_NORTH_EAST);
    gtk_window_set_position(GTK_WINDOW(ptask->progress_dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);

    // clang-format off
    // gtk_dialog_set_default_response(ptask->progress_dlg, GtkResponseType::GTK_RESPONSE_OK);
    g_signal_connect(G_OBJECT(ptask->progress_dlg), "response", G_CALLBACK(on_progress_dlg_response), ptask);
    g_signal_connect(G_OBJECT(ptask->progress_dlg), "destroy", G_CALLBACK(on_progress_dlg_destroy), ptask);
    g_signal_connect(G_OBJECT(ptask->progress_dlg), "delete-event", G_CALLBACK(on_progress_dlg_delete_event), ptask);
    // g_signal_connect(G_OBJECT(ptask->progress_dlg), "configure-event", G_CALLBACK(on_progress_configure_event), ptask);
    // clang-format on

    gtk_widget_show_all(ptask->progress_dlg);
    if (ptask->overwrite_combo && !xset_get_b(xset::name::task_pop_over))
    {
        gtk_widget_hide(GTK_WIDGET(ptask->overwrite_combo));
    }
    if (ptask->error_combo && !xset_get_b(xset::name::task_pop_err))
    {
        gtk_widget_hide(GTK_WIDGET(ptask->error_combo));
    }
    if (overwrite_box && !gtk_widget_get_visible(ptask->overwrite_combo) &&
        !gtk_widget_get_visible(ptask->error_combo))
    {
        gtk_widget_hide(GTK_WIDGET(overwrite_box));
    }
    gtk_widget_grab_focus(GTK_WIDGET(ptask->progress_btn_close));

    // icon
    set_progress_icon(ptask);

    // auto scroll - must be after show_all
    if (!task->exec_scroll_lock)
    {
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(ptask->error_view),
                                     ptask->log_end,
                                     0.0,
                                     false,
                                     0,
                                     0);
    }

    ptask->progress_count = 50; // trigger fast display
    // ztd::logger::info("ptk_file_task_progress_open DONE");
}

static void
ptk_file_task_progress_update(PtkFileTask* ptask)
{
    if (!ptask->progress_dlg)
    {
        if (ptask->pause_change)
        {
            ptask->pause_change = false; // stop elapsed timer
        }
        return;
    }

    std::string ufile_path;

    // ztd::logger::info("ptk_file_task_progress_update ptask={}", fmt::ptr(ptask));

    vfs::file_task task = ptask->task;

    // current file
    std::filesystem::path usrc_dir;
    std::filesystem::path udest;

    if (ptask->complete)
    {
        gtk_widget_set_sensitive(GTK_WIDGET(ptask->progress_btn_stop), false);
        gtk_widget_set_sensitive(GTK_WIDGET(ptask->progress_btn_pause), false);
        gtk_widget_set_sensitive(GTK_WIDGET(ptask->progress_btn_close), true);
        if (ptask->overwrite_combo)
        {
            gtk_widget_set_sensitive(ptask->overwrite_combo, false);
        }
        if (ptask->error_combo)
        {
            gtk_widget_set_sensitive(ptask->error_combo, false);
        }

        if (task->type != vfs::file_task_type::exec)
        {
            ufile_path = nullptr;
        }
        else
        {
            if (task->current_file)
            {
                const auto current_file = task->current_file.value();

                const std::string escaped_markup = Glib::Markup::escape_text(current_file.string());
                ufile_path = std::format("<b>{}</b>", escaped_markup);
            }
        }

        std::string window_title;
        if (ptask->aborted)
        {
            window_title = "Stopped";
        }
        else
        {
            if (task->err_count)
            {
                window_title = "Errors";
            }
            else
            {
                window_title = "Done";
            }
        }
        gtk_window_set_title(GTK_WINDOW(ptask->progress_dlg), window_title.c_str());
        if (ufile_path.empty())
        {
            const std::string escaped_markup = Glib::Markup::escape_text(window_title);
            ufile_path = std::format("<b>( {} )</b>", escaped_markup);
        }
    }
    else if (task->current_file)
    {
        const auto current_file = task->current_file.value();

        if (task->type != vfs::file_task_type::exec)
        {
            // Copy: <src basename>
            const auto name = current_file.filename();
            const std::string escaped_markup = Glib::Markup::escape_text(name.string());
            ufile_path = std::format("<b>{}</b>", escaped_markup);

            // From: <src_dir>
            const auto current_parent = current_file.parent_path();
            if (!std::filesystem::equivalent(current_parent, "/"))
            {
                usrc_dir = current_parent;
            }

            // To: <dest_dir> OR <dest_file>
            if (task->current_dest)
            {
                const auto current_dest = task->current_dest.value();

                const auto current_file_filename = current_file.filename();
                const auto current_dest_filename = current_dest.filename();
                if (!std::filesystem::equivalent(current_file_filename, current_dest_filename))
                {
                    // source and dest filenames differ, user renamed - show all
                    udest = current_dest;
                }
                else
                {
                    // source and dest filenames same - show dest dir only
                    udest = current_dest.parent_path();
                }
            }
        }
        else
        {
            const std::string escaped_markup = Glib::Markup::escape_text(current_file.string());
            ufile_path = std::format("<b>{}</b>", escaped_markup);
        }
    }

    if (udest.empty() && !ptask->complete && task->dest_dir)
    {
        udest = task->dest_dir.value();
    }
    gtk_label_set_markup(ptask->from, ufile_path.c_str());
    if (ptask->src_dir)
    {
        gtk_label_set_text(ptask->src_dir, usrc_dir.c_str());
    }
    if (ptask->to)
    {
        gtk_label_set_text(ptask->to, udest.c_str());
    }

    // progress bar
    if (task->type != vfs::file_task_type::exec || ptask->task->custom_percent)
    {
        if (task->percent >= 0)
        {
            if (task->percent > 100)
            {
                task->percent = 100;
            }
            gtk_progress_bar_set_fraction(ptask->progress_bar, ((f64)task->percent) / 100);
            const std::string percent_str = std::format("{} %", task->percent);
            gtk_progress_bar_set_text(ptask->progress_bar, percent_str.data());
        }
        else
        {
            gtk_progress_bar_set_fraction(ptask->progress_bar, 0);
        }
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(ptask->progress_bar), true);
    }
    else if (ptask->complete)
    {
        if (!ptask->task->custom_percent)
        {
            if (task->exec_is_error || ptask->aborted)
            {
                gtk_progress_bar_set_fraction(ptask->progress_bar, 0);
            }
            else
            {
                gtk_progress_bar_set_fraction(ptask->progress_bar, 1);
            }
            gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(ptask->progress_bar), true);
        }
    }
    else if (task->type == vfs::file_task_type::exec &&
             task->state_pause == vfs::file_task_state::running)
    {
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(ptask->progress_bar), false);
        gtk_progress_bar_pulse(ptask->progress_bar);
    }

    // progress
    if (task->type != vfs::file_task_type::exec)
    {
        std::string stats;

        if (ptask->complete)
        {
            if (ptask->pop_detail)
            {
                stats = std::format("#{}  ({}) [{}] @avg {}",
                                    ptask->dsp_file_count,
                                    ptask->dsp_size_tally,
                                    ptask->dsp_elapsed,
                                    ptask->dsp_avgspeed);
            }
            else
            {
                stats = std::format("{} ({})", ptask->dsp_size_tally, ptask->dsp_avgspeed);
            }
        }
        else
        {
            if (ptask->pop_detail)
            {
                stats = std::format("#{} ({}) [{}] @cur {} ({}) @avg {} ({})",
                                    ptask->dsp_file_count,
                                    ptask->dsp_size_tally,
                                    ptask->dsp_elapsed,
                                    ptask->dsp_curspeed,
                                    ptask->dsp_curest,
                                    ptask->dsp_avgspeed,
                                    ptask->dsp_avgest);
            }
            else
            {
                stats = std::format("{}  ({})  {} remaining",
                                    ptask->dsp_size_tally,
                                    ptask->dsp_avgspeed,
                                    ptask->dsp_avgest);
            }
        }
        gtk_label_set_text(ptask->current, stats.data());
    }

    // error/output log
    if (ptask->log_appended || ptask->force_scroll)
    {
        if (!task->exec_scroll_lock)
        {
            // scroll to end if scrollbar is mostly down or force_scroll
            GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment(ptask->scroll);
            if (ptask->force_scroll ||
                gtk_adjustment_get_upper(adj) - gtk_adjustment_get_value(adj) <
                    gtk_adjustment_get_page_size(adj) + 40)
            {
                // ztd::logger::info("    scroll to end line {}", ptask->log_end,
                // gtk_text_buffer_get_line_count(ptask->log_buf));
                gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(ptask->error_view),
                                             ptask->log_end,
                                             0.0,
                                             false,
                                             0,
                                             0);
            }
        }
        ptask->log_appended = false;
    }

    // icon
    if (ptask->pause_change || ptask->err_count != task->err_count)
    {
        ptask->pause_change = false;
        ptask->err_count = task->err_count;
        set_progress_icon(ptask);
    }

    // status
    std::string errs;
    if (ptask->complete)
    {
        if (ptask->aborted)
        {
            if (task->err_count && task->type != vfs::file_task_type::exec)
            {
                if (ptask->err_mode == ptk::file_task::ptask_error::first)
                {
                    errs = "Error  ( Stop If First )";
                }
                else if (ptask->err_mode == ptk::file_task::ptask_error::any)
                {
                    errs = "Error  ( Stop On Any )";
                }
                else
                {
                    errs = std::format("Stopped with {} error", task->err_count);
                }
            }
            else
            {
                errs = "Stopped";
            }
        }
        else
        {
            if (task->type != vfs::file_task_type::exec)
            {
                if (task->err_count)
                {
                    errs = std::format("Finished with {} error", task->err_count);
                }
                else
                {
                    errs = "Done";
                }
            }
            else
            {
                if (task->exec_exit_status)
                {
                    errs = std::format("Finished with error  ( exit status {} )",
                                       task->exec_exit_status);
                }
                else if (task->exec_is_error)
                {
                    errs = "Finished with error";
                }
                else
                {
                    errs = "Done";
                }
            }
        }
    }
    else if (task->state_pause == vfs::file_task_state::pause)
    {
        if (task->type != vfs::file_task_type::exec)
        {
            errs = "Paused";
        }
        else
        {
            if (task->exec_pid)
            {
                errs = std::format("Paused  ( pid {} )", task->exec_pid);
            }
            else
            {
                errs = std::format("Paused  ( exit status {} )", task->exec_exit_status);
                set_button_states(ptask);
            }
        }
    }
    else if (task->state_pause == vfs::file_task_state::queue)
    {
        if (task->type != vfs::file_task_type::exec)
        {
            errs = "Queued";
        }
        else
        {
            if (task->exec_pid)
            {
                errs = std::format("Queued  ( pid {} )", task->exec_pid);
            }
            else
            {
                errs = std::format("Queued  ( exit status {} )", task->exec_exit_status);
                set_button_states(ptask);
            }
        }
    }
    else
    {
        if (task->type != vfs::file_task_type::exec)
        {
            if (task->err_count)
            {
                errs = std::format("Running with {} error", task->err_count);
            }
            else
            {
                errs = "Running...";
            }
        }
        else
        {
            if (task->exec_pid)
            {
                errs = std::format("Running...  ( pid {} )", task->exec_pid);
            }
            else
            {
                errs = std::format("Running...  ( exit status {} )", task->exec_exit_status);
                set_button_states(ptask);
            }
        }
    }
    gtk_label_set_text(ptask->errors, errs.data());
    // ztd::logger::info("ptk_file_task_progress_update DONE ptask={}", fmt::ptr(ptask));
}

void
ptk_file_task_set_chmod(PtkFileTask* ptask, std::array<u8, 12> chmod_actions)
{
    ptask->task->set_chmod(chmod_actions);
}

void
ptk_file_task_set_chown(PtkFileTask* ptask, uid_t uid, gid_t gid)
{
    ptask->task->set_chown(uid, gid);
}

void
ptk_file_task_set_recursive(PtkFileTask* ptask, bool recursive)
{
    ptask->task->set_recursive(recursive);
}

static void
ptk_file_task_update(PtkFileTask* ptask)
{
    // ztd::logger::info("ptk_file_task_update ptask={}", fmt::ptr(ptask));
    // calculate updated display data

    if (!ptk_file_task_trylock(ptask))
    {
        // ztd::logger::info("UPDATE LOCKED");
        return;
    }

    vfs::file_task task = ptask->task;
    u64 cur_speed;
    const f64 timer_elapsed = task->timer.elapsed();

    if (task->type == vfs::file_task_type::exec)
    {
        // test for zombie process
        i32 status = 0;
        if (!ptask->complete && task->exec_pid && waitpid(task->exec_pid, &status, WNOHANG))
        {
            // process is no longer running (defunct zombie)
            // glib should detect this but sometimes process goes defunct
            // with no watch callback, so remove it from task list
            if (task->child_watch)
            {
                g_source_remove(task->child_watch);
                task->child_watch = 0;
            }
            g_spawn_close_pid(task->exec_pid);
            if (task->exec_channel_out)
            {
                g_io_channel_shutdown(task->exec_channel_out, true, nullptr);
            }
            if (task->exec_channel_err)
            {
                g_io_channel_shutdown(task->exec_channel_err, true, nullptr);
            }
            task->exec_channel_out = task->exec_channel_err = nullptr;
            if (status)
            {
                if (WIFEXITED(status))
                {
                    task->exec_exit_status = WEXITSTATUS(status);
                }
                else
                {
                    task->exec_exit_status = -1;
                }
            }
            else
            {
                task->exec_exit_status = 0;
            }
            ztd::logger::info("child ZOMBIED  pid={} exit_status={}",
                              task->exec_pid,
                              task->exec_exit_status);
            task->exec_pid = 0;
            ptask->complete = true;
        }
    }
    else
    {
        // cur speed
        if (task->state_pause == vfs::file_task_state::running)
        {
            const f64 since_last = timer_elapsed - task->last_elapsed;
            if (since_last >= 2.0)
            {
                cur_speed = (task->progress - task->last_progress) / since_last;
                // ztd::logger::info("( {} - {} ) / {} = {}", task->progress,
                //                task->last_progress, since_last, cur_speed);
                task->last_elapsed = timer_elapsed;
                task->last_speed = cur_speed;
                task->last_progress = task->progress;
            }
            else if (since_last > 0.1)
            {
                cur_speed = (task->progress - task->last_progress) / since_last;
            }
            else
            {
                cur_speed = 0;
            }
        }
        // calc percent
        i32 ipercent;
        if (task->total_size)
        {
            const f64 dpercent = ((f64)task->progress) / task->total_size;
            ipercent = (i32)(dpercent * 100);
        }
        else
        {
            ipercent = 50; // total_size calculation timed out
        }
        if (ipercent != task->percent)
        {
            task->percent = ipercent;
        }
    }

    // elapsed
    u32 hours = timer_elapsed / 3600.0;
    std::string elapsed;
    std::string elapsed2;
    if (hours != 0)
    {
        elapsed = std::format("{}", hours);
    }
    u32 mins = (timer_elapsed - (hours * 3600)) / 60;
    if (hours > 0)
    {
        elapsed2 = std::format("{}:{:02d}", elapsed, mins);
    }
    else if (mins > 0)
    {
        elapsed2 = std::format("{}", mins);
    }
    else
    {
        elapsed2 = elapsed;
    }
    u32 secs = (timer_elapsed - (hours * 3600) - (mins * 60));
    const std::string elapsed3 = std::format("{}:{:02d}", elapsed2, secs);
    ptask->dsp_elapsed = elapsed3;

    if (task->type != vfs::file_task_type::exec)
    {
        std::string speed1;
        std::string speed2;
        std::string remain1;
        std::string remain2;

        std::string size_str;
        std::string size_str2;

        // count
        const std::string file_count = std::to_string(task->current_item);
        // size
        size_str = vfs_file_size_format(task->progress);
        if (task->total_size)
        {
            size_str2 = vfs_file_size_format(task->total_size);
        }
        else
        {
            size_str2 = "??"; // total_size calculation timed out
        }
        const std::string size_tally = std::format("{} / {}", size_str, size_str2);
        // cur speed display
        if (task->last_speed != 0)
        {
            // use speed of last 2 sec interval if available
            cur_speed = task->last_speed;
        }
        if (cur_speed == 0 || task->state_pause != vfs::file_task_state::running)
        {
            if (task->state_pause == vfs::file_task_state::pause)
            {
                speed1 = "paused";
            }
            else if (task->state_pause == vfs::file_task_state::queue)
            {
                speed1 = "queued";
            }
            else
            {
                speed1 = "stalled";
            }
        }
        else
        {
            size_str = vfs_file_size_format(cur_speed);
            speed1 = std::format("{}/s", size_str);
        }
        // avg speed
        std::time_t avg_speed;
        if (timer_elapsed > 0)
        {
            avg_speed = task->progress / timer_elapsed;
        }
        else
        {
            avg_speed = 0;
        }
        size_str2 = vfs_file_size_format(avg_speed);
        speed2 = std::format("{}/s", size_str2);

        // remain cur
        u64 remain;
        if (cur_speed > 0 && task->total_size != 0)
        {
            remain = (task->total_size - task->progress) / cur_speed;
        }
        else
        {
            remain = 0;
        }
        if (remain <= 0)
        {
            remain1 = ""; // n/a
        }
        else if (remain > 3599)
        {
            hours = remain / 3600;
            if (remain - (hours * 3600) > 1799)
            {
                hours++;
            }
            remain1 = std::format("{}/h", hours);
        }
        else if (remain > 59)
        {
            remain1 =
                std::format("{}:{:02}", remain / 60, remain - ((unsigned int)(remain / 60) * 60));
        }
        else
        {
            remain1 = std::format(":{:02}", remain);
        }

        // remain avg
        if (avg_speed > 0 && task->total_size != 0)
        {
            remain = (task->total_size - task->progress) / avg_speed;
        }
        else
        {
            remain = 0;
        }
        if (remain <= 0)
        {
            remain2 = ""; // n/a
        }
        else if (remain > 3599)
        {
            hours = remain / 3600;
            if (remain - (hours * 3600) > 1799)
            {
                hours++;
            }
            remain2 = std::format("{}/h", hours);
        }
        else if (remain > 59)
        {
            remain2 =
                std::format("{}:{:02}", remain / 60, remain - ((unsigned int)(remain / 60) * 60));
        }
        else
        {
            remain2 = std::format(":{:02}", remain);
        }

        ptask->dsp_file_count = file_count;
        ptask->dsp_size_tally = size_tally;
        ptask->dsp_curspeed = speed1;
        ptask->dsp_avgspeed = speed2;
        ptask->dsp_curest = remain1;
        ptask->dsp_avgest = remain2;
    }

    // move log lines from add_log_buf to log_buf
    if (gtk_text_buffer_get_char_count(task->add_log_buf))
    {
        GtkTextIter iter, siter;
        char* text;
        // get add_log text and delete
        gtk_text_buffer_get_start_iter(task->add_log_buf, &siter);
        gtk_text_buffer_get_iter_at_mark(task->add_log_buf, &iter, task->add_log_end);
        text = gtk_text_buffer_get_text(task->add_log_buf, &siter, &iter, false);
        gtk_text_buffer_delete(task->add_log_buf, &siter, &iter);
        // insert into log
        gtk_text_buffer_get_iter_at_mark(ptask->log_buf, &iter, ptask->log_end);
        gtk_text_buffer_insert(ptask->log_buf, &iter, text, -1);
        std::free(text);
        ptask->log_appended = true;

        // trim log ?  (less than 64K and 800 lines)
        if (gtk_text_buffer_get_char_count(ptask->log_buf) > 64000 ||
            gtk_text_buffer_get_line_count(ptask->log_buf) > 800)
        {
            if (gtk_text_buffer_get_char_count(ptask->log_buf) > 64000)
            {
                // trim to 50000 characters - handles single line flood
                gtk_text_buffer_get_iter_at_offset(ptask->log_buf,
                                                   &iter,
                                                   gtk_text_buffer_get_char_count(ptask->log_buf) -
                                                       50000);
            }
            else
            {
                // trim to 700 lines
                gtk_text_buffer_get_iter_at_line(ptask->log_buf,
                                                 &iter,
                                                 gtk_text_buffer_get_line_count(ptask->log_buf) -
                                                     700);
            }
            gtk_text_buffer_get_start_iter(ptask->log_buf, &siter);
            gtk_text_buffer_delete(ptask->log_buf, &siter, &iter);
            gtk_text_buffer_get_start_iter(ptask->log_buf, &siter);
            if (task->type == vfs::file_task_type::exec)
            {
                gtk_text_buffer_insert(
                    ptask->log_buf,
                    &siter,
                    "[ SNIP - additional output above has been trimmed from this log ]\n",
                    -1);
            }
            else
            {
                gtk_text_buffer_insert(
                    ptask->log_buf,
                    &siter,
                    "[ SNIP - additional errors above have been trimmed from this log ]\n",
                    -1);
            }
        }

        if (task->type == vfs::file_task_type::exec && task->exec_show_output)
        {
            if (!ptask->keep_dlg)
            {
                ptask->keep_dlg = true;
            }
            if (!ptask->progress_dlg)
            {
                // disable this line to open every time output occurs
                task->exec_show_output = false;
                ptk_file_task_progress_open(ptask);
            }
        }
    }

    if (!ptask->progress_dlg)
    {
        if (task->type != vfs::file_task_type::exec && ptask->err_count != task->err_count)
        {
            ptask->keep_dlg = true;
            ptk_file_task_progress_open(ptask);
        }
        else if (task->type == vfs::file_task_type::exec && ptask->err_count != task->err_count)
        {
            if (!ptask->aborted && task->exec_show_error)
            {
                ptask->keep_dlg = true;
                ptk_file_task_progress_open(ptask);
                // If error opens dialog after command finishes, gtk will not
                // scroll to end on initial attempts, so force_scroll
                // ensures it will try to scroll again - still sometimes
                // does not work
                ptask->force_scroll = ptask->complete && !task->exec_scroll_lock;
            }
        }
    }
    else
    {
        if (task->type != vfs::file_task_type::exec && ptask->err_count != task->err_count)
        {
            ptask->keep_dlg = true;
            if (ptask->complete || ptask->err_mode == ptk::file_task::ptask_error::any ||
                (task->error_first && ptask->err_mode == ptk::file_task::ptask_error::first))
            {
                gtk_window_present(GTK_WINDOW(ptask->progress_dlg));
            }
        }
        else if (task->type == vfs::file_task_type::exec && ptask->err_count != task->err_count)
        {
            if (!ptask->aborted && task->exec_show_error)
            {
                ptask->keep_dlg = true;
                gtk_window_present(GTK_WINDOW(ptask->progress_dlg));
            }
        }
    }

    ptk_file_task_progress_update(ptask);

    if (!ptask->timeout && !ptask->complete)
    {
        main_task_view_update_task(ptask);
    }

    task->unlock();
    // ztd::logger::info("ptk_file_task_update DONE ptask={}", fmt::ptr(ptask));
}

static bool
on_vfs_file_task_state_cb(vfs::file_task task, vfs::file_task_state state, void* state_data,
                          void* user_data)
{
    PtkFileTask* ptask = PTK_FILE_TASK(user_data);
    bool ret = true;

    switch (state)
    {
        case vfs::file_task_state::finish:
            // ztd::logger::info("vfs::file_task_state::FINISH");

            ptask->complete = true;

            task->lock();
            if (task->type != vfs::file_task_type::exec)
            {
                task->current_file = std::nullopt;
            }
            ptask->progress_count = 50; // trigger fast display
            task->unlock();
            // gtk_signal_emit_by_name(G_OBJECT(ptask->signal_widget), "task-notify", ptask);
            break;
        case vfs::file_task_state::query_overwrite:
            // 0; GThread *self = g_thread_self ();
            // ztd::logger::info("TASK_THREAD = {}", fmt::ptr(self));
            task->lock();
            ptask->query_new_dest = (char**)state_data;
            *ptask->query_new_dest = nullptr;
            ptask->query_cond = g_cond_new();
            task->timer.stop();
            g_cond_wait(ptask->query_cond, task->mutex);
            g_cond_free(ptask->query_cond);
            ptask->query_cond = nullptr;
            ret = ptask->query_ret;
            task->last_elapsed = task->timer.elapsed();
            task->last_progress = task->progress;
            task->last_speed = 0;
            task->timer.start();
            task->unlock();
            break;
        case vfs::file_task_state::error:
            // ztd::logger::info("vfs::file_task_state::ERROR");
            task->lock();
            task->err_count++;
            // ztd::logger::info("    ptask->item_count = {}", task->current_item );

            if (task->type == vfs::file_task_type::exec)
            {
                task->exec_is_error = true;
                ret = false;
            }
            else if (ptask->err_mode == ptk::file_task::ptask_error::any ||
                     (task->error_first && ptask->err_mode == ptk::file_task::ptask_error::first))
            {
                ret = false;
                ptask->aborted = true;
            }
            ptask->progress_count = 50; // trigger fast display

            task->unlock();

            if (xset_get_b(xset::name::task_q_pause))
            {
                // pause all queued
                main_task_pause_all_queued(ptask);
            }
            break;
        case vfs::file_task_state::running:
        case vfs::file_task_state::size_timeout:
        case vfs::file_task_state::pause:
        case vfs::file_task_state::queue:
            break;
    }

    return ret; /* return true to continue */
}

namespace ptk::file_task
{
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
}

static bool
on_query_input_keypress(GtkWidget* widget, GdkEventKey* event, PtkFileTask* ptask)
{
    (void)ptask;
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
    {
        // User pressed enter in rename/overwrite dialog
        const auto new_name = multi_input_get_text(widget);
        const char* old_name =
            static_cast<const char*>(g_object_get_data(G_OBJECT(widget), "old_name"));
        GtkWidget* dlg = gtk_widget_get_toplevel(widget);
        if (!GTK_IS_DIALOG(dlg))
        {
            return true;
        }
        if (new_name && old_name && !ztd::same(new_name.value(), old_name))
        {
            gtk_dialog_response(GTK_DIALOG(dlg), ptk::file_task::response::rename);
        }
        else
        {
            gtk_dialog_response(GTK_DIALOG(dlg), ptk::file_task::response::auto_rename);
        }
        return true;
    }
    return false;
}

static void
on_multi_input_changed(GtkWidget* input_buf, GtkWidget* query_input)
{
    (void)input_buf;
    const auto new_name = multi_input_get_text(query_input);
    const char* old_name =
        static_cast<const char*>(g_object_get_data(G_OBJECT(query_input), "old_name"));
    const bool can_rename = new_name && old_name && (!ztd::same(new_name.value(), old_name));
    GtkWidget* dlg = gtk_widget_get_toplevel(query_input);
    if (!GTK_IS_DIALOG(dlg))
    {
        return;
    }
    GtkWidget* rename_button = GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "rename_button"));
    if (GTK_IS_WIDGET(rename_button))
    {
        gtk_widget_set_sensitive(rename_button, can_rename);
    }
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dlg),
                                      ptk::file_task::response::overwrite,
                                      !can_rename);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dlg),
                                      ptk::file_task::response::overwrite_all,
                                      !can_rename);
}

static void
query_overwrite_response(GtkDialog* dlg, i32 response, PtkFileTask* ptask)
{
    switch (response)
    {
        case ptk::file_task::response::overwrite_all:
        {
            ptask->task->set_overwrite_mode(vfs::file_task_overwrite_mode::overwrite_all);
            if (ptask->progress_dlg)
            {
                gtk_combo_box_set_active(
                    GTK_COMBO_BOX(ptask->overwrite_combo),
                    magic_enum::enum_integer(vfs::file_task_overwrite_mode::overwrite_all));
            }
            break;
        }
        case ptk::file_task::response::overwrite:
        {
            ptask->task->set_overwrite_mode(vfs::file_task_overwrite_mode::overwrite);
            break;
        }
        case ptk::file_task::response::skip_all:
        {
            ptask->task->set_overwrite_mode(vfs::file_task_overwrite_mode::skip_all);
            if (ptask->progress_dlg)
            {
                gtk_combo_box_set_active(
                    GTK_COMBO_BOX(ptask->overwrite_combo),
                    magic_enum::enum_integer(vfs::file_task_overwrite_mode::skip_all));
            }
            break;
        }
        case ptk::file_task::response::skip:
        {
            ptask->task->set_overwrite_mode(vfs::file_task_overwrite_mode::skip);
            break;
        }
        case ptk::file_task::response::auto_rename_all:
        {
            ptask->task->set_overwrite_mode(vfs::file_task_overwrite_mode::auto_rename);
            if (ptask->progress_dlg)
            {
                gtk_combo_box_set_active(
                    GTK_COMBO_BOX(ptask->overwrite_combo),
                    magic_enum::enum_integer(vfs::file_task_overwrite_mode::auto_rename));
            }
            break;
        }
        case ptk::file_task::response::auto_rename:
        case ptk::file_task::response::rename:
        {
            std::optional<std::string> str;
            ptask->task->set_overwrite_mode(vfs::file_task_overwrite_mode::rename);
            if (response == ptk::file_task::response::auto_rename)
            {
                GtkWidget* auto_button =
                    GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "auto_button"));
                str = gtk_widget_get_tooltip_text(auto_button);
            }
            else
            {
                GtkWidget* query_input =
                    GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "query_input"));
                str = multi_input_get_text(query_input);
            }
            const auto file_name = std::filesystem::path(str.value());
            if (str && !file_name.empty() && ptask->task->current_dest)
            {
                const auto current_dest = ptask->task->current_dest.value();

                const auto dir_name = current_dest.parent_path();
                const auto path = dir_name / file_name;
                *ptask->query_new_dest = ztd::strdup(path);
            }
            break;
        }
        case ptk::file_task::response::pause:
        {
            ptk_file_task_pause(ptask, vfs::file_task_state::pause);
            main_task_start_queued(ptask->task_view, ptask);
            ptask->task->set_overwrite_mode(vfs::file_task_overwrite_mode::rename);
            ptask->restart_timeout = false;
            break;
        }
        case GtkResponseType::GTK_RESPONSE_DELETE_EVENT: // escape was pressed or window closed
        case GtkResponseType::GTK_RESPONSE_CANCEL:
        {
            ptask->task->abort = true;
            break;
        }
        default:
            break;
    }

    // save size
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    if (allocation.width && allocation.height)
    {
        const i32 has_overwrite_btn =
            GPOINTER_TO_INT((void*)g_object_get_data(G_OBJECT(dlg), "has_overwrite_btn"));
        xset_set(xset::name::task_popups,
                 has_overwrite_btn ? xset::var::x : xset::var::s,
                 std::to_string(allocation.width));
        xset_set(xset::name::task_popups,
                 has_overwrite_btn ? xset::var::y : xset::var::z,
                 std::to_string(allocation.height));
    }

    gtk_widget_destroy(GTK_WIDGET(dlg));

    if (ptask->query_cond)
    {
        ptk_file_task_lock(ptask);
        ptask->query_ret = (response != GtkResponseType::GTK_RESPONSE_DELETE_EVENT) &&
                           (response != GtkResponseType::GTK_RESPONSE_CANCEL);
        // g_cond_broadcast( ptask->query_cond );
        g_cond_signal(ptask->query_cond);
        ptk_file_task_unlock(ptask);
    }
    if (ptask->restart_timeout)
    {
        ptask->timeout = g_timeout_add(500, (GSourceFunc)ptk_file_task_add_main, (void*)ptask);
    }
    ptask->progress_count = 50;
    ptask->progress_timer = g_timeout_add(50, (GSourceFunc)on_progress_timer, ptask);
}

static void
on_query_button_press(GtkWidget* widget, PtkFileTask* ptask)
{
    GtkWidget* dlg = gtk_widget_get_toplevel(widget);
    if (!GTK_IS_DIALOG(dlg))
    {
        return;
    }
    GtkWidget* rename_button = GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "rename_button"));
    GtkWidget* auto_button = GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "auto_button"));
    if (!rename_button || !auto_button)
    {
        return;
    }
    i32 response;
    if (widget == rename_button)
    {
        response = ptk::file_task::response::rename;
    }
    else if (widget == auto_button)
    {
        response = ptk::file_task::response::auto_rename;
    }
    else
    {
        response = ptk::file_task::response::auto_rename_all;
    }
    query_overwrite_response(GTK_DIALOG(dlg), response, ptask);
}

static void
query_overwrite(PtkFileTask* ptask)
{
    // ztd::logger::info("query_overwrite ptask={}", fmt::ptr(ptask));
    GtkWidget* dlg;
    GtkWidget* parent_win;
    GtkTextIter iter;

    bool has_overwrite_btn = true;

    std::string title;
    std::string message;
    std::string from_size_str;
    std::string to_size_str;
    std::string from_disp;

    if (ptask->task->type == vfs::file_task_type::move)
    {
        from_disp = "Moving from directory:";
    }
    else if (ptask->task->type == vfs::file_task_type::link)
    {
        from_disp = "Linking from directory:";
    }
    else
    {
        from_disp = "Copying from directory:";
    }

    if (!ptask->task->current_file || !ptask->task->current_dest)
    {
        return;
    }

    const auto current_file = ptask->task->current_file.value();
    const auto current_dest = ptask->task->current_dest.value();

    const bool different_files = (!std::filesystem::equivalent(current_file, current_dest));

    const auto src_stat = ztd::statx(current_file, ztd::statx::symlink::no_follow);
    const auto dest_stat = ztd::statx(current_dest, ztd::statx::symlink::no_follow);

    const bool is_src_dir = std::filesystem::is_directory(current_file);
    const bool is_dest_dir = std::filesystem::is_directory(current_dest);

    if (different_files && is_dest_dir == is_src_dir)
    {
        if (is_dest_dir)
        {
            /* Ask the user whether to overwrite dir content or not */
            title = "Directory Exists";
            message = "<b>Directory already exists.</b>  Please rename or select an action.";
        }
        else
        {
            /* Ask the user whether to overwrite the file or not */
            std::string src_size;
            std::string src_time;
            std::string src_rel_size;
            std::string src_rel_time;
            std::string src_link;
            std::string dest_link;
            std::string link_warn;

            const bool is_src_sym = std::filesystem::is_symlink(current_file);
            const bool is_dest_sym = std::filesystem::is_symlink(current_dest);

            if (is_src_sym)
            {
                src_link = "\t<b>( link )</b>";
            }
            if (is_dest_sym)
            {
                dest_link = "\t<b>( link )</b>";
            }

            if (is_src_sym && !is_dest_sym)
            {
                link_warn = "\t<b>! overwrite file with link !</b>";
            }
            if (src_stat.size() == dest_stat.size())
            {
                src_size = "<b>( same size )</b>";
            }
            else
            {
                const std::string size_str = vfs_file_size_format(src_stat.size());
                src_size = std::format("{}\t( {:L} bytes )", size_str, src_stat.size());
                if (src_stat.size() > dest_stat.size())
                {
                    src_rel_size = "larger";
                }
                else
                {
                    src_rel_size = "smaller";
                }
            }

            if (src_stat.mtime().tv_sec == dest_stat.mtime().tv_sec)
            {
                src_time = "<b>( same time )</b>\t";
            }
            else
            {
                const time_t src_mtime = src_stat.mtime().tv_sec;
                src_time = vfs_create_display_date(src_mtime);

                if (src_stat.mtime().tv_sec > dest_stat.mtime().tv_sec)
                {
                    src_rel_time = "newer";
                }
                else
                {
                    src_rel_time = "older";
                }
            }

            const std::string size_str = vfs_file_size_format(dest_stat.size());
            const std::string dest_size =
                std::format("{}\t( {:L} bytes )", size_str, dest_stat.size());

            const time_t dest_mtime = dest_stat.mtime().tv_sec;
            const auto dest_time = vfs_create_display_date(dest_mtime);

            std::string src_rel;
            if (src_rel_time.empty())
            {
                src_rel = std::format("<b>( {} )</b>", src_rel_size);
            }
            else if (src_rel_size.empty())
            {
                src_rel = std::format("<b>( {} )</b>", src_rel_time);
            }
            else
            {
                src_rel = std::format("<b>( {} &amp; {} )</b>", src_rel_time, src_rel_size);
            }

            from_size_str = std::format("\t{}\t{}{}{}{}",
                                        src_time,
                                        src_size,
                                        !src_rel.empty() ? "\t" : "",
                                        src_rel,
                                        src_link);
            to_size_str = std::format("\t{}\t{}{}",
                                      dest_time,
                                      dest_size,
                                      !dest_link.empty() ? dest_link : link_warn);

            title = "Filename Exists";
            message = "<b>Filename already exists.</b>  Please rename or select an action.";
        }
    }
    else
    { /* Rename is required */
        has_overwrite_btn = false;
        title = "Rename Required";
        if (!different_files)
        {
            from_disp = "In directory:";
        }
        message = "<b>Filename already exists.</b>  Please rename or select an action.";
    }

    // filenames
    const std::string filename = current_dest.filename();
    const std::string src_dir = current_file.parent_path();
    const std::string dest_dir = current_dest.parent_path();

    const auto filename_parts = split_basename_extension(filename);

    const std::string unique_name =
        vfs_get_unique_name(dest_dir, filename_parts.basename, filename_parts.extension);
    const std::string new_name_plain =
        !unique_name.empty() ? std::filesystem::path(unique_name).filename() : "";
    const std::string new_name = !new_name_plain.empty() ? new_name_plain : "";

    const auto pos = !filename_parts.extension.empty()
                         ? filename.size() - filename_parts.extension.size() - 1
                         : -1;

    // create dialog
    if (ptask->progress_dlg)
    {
        parent_win = GTK_WIDGET(ptask->progress_dlg);
    }
    else
    {
        parent_win = GTK_WIDGET(ptask->parent_window);
    }
    dlg =
        gtk_dialog_new_with_buttons(title.data(),
                                    GTK_WINDOW(parent_win),
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    nullptr,
                                    nullptr);

    g_signal_connect(G_OBJECT(dlg), "response", G_CALLBACK(query_overwrite_response), ptask);
    gtk_window_set_resizable(GTK_WINDOW(dlg), true);
    gtk_window_set_title(GTK_WINDOW(dlg), title.data());
    gtk_window_set_type_hint(GTK_WINDOW(dlg), GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_gravity(GTK_WINDOW(dlg), GdkGravity::GDK_GRAVITY_NORTH_EAST);
    gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);

    gtk_widget_set_halign(GTK_WIDGET(dlg), GtkAlign::GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(dlg), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_hexpand(GTK_WIDGET(dlg), true);
    gtk_widget_set_vexpand(GTK_WIDGET(dlg), true);
    gtk_widget_set_margin_top(GTK_WIDGET(dlg), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(dlg), 0);
    gtk_widget_set_margin_start(GTK_WIDGET(dlg), 0);
    gtk_widget_set_margin_end(GTK_WIDGET(dlg), 0);

    GtkBox* vbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_halign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_hexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_vexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_margin_top(GTK_WIDGET(vbox), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(vbox), 14);
    gtk_widget_set_margin_start(GTK_WIDGET(vbox), 7);
    gtk_widget_set_margin_end(GTK_WIDGET(vbox), 7);

    if (has_overwrite_btn)
    {
        gtk_widget_set_size_request(GTK_WIDGET(vbox), 900, 400);
        gtk_widget_set_size_request(GTK_WIDGET(dlg), 900, -1);
    }
    else
    {
        gtk_widget_set_size_request(GTK_WIDGET(vbox), 600, 300);
        gtk_widget_set_size_request(GTK_WIDGET(dlg), 600, -1);
    }

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))),
                       GTK_WIDGET(vbox),
                       true,
                       true,
                       0);

    // buttons
    if (has_overwrite_btn)
    {
        gtk_dialog_add_buttons(GTK_DIALOG(dlg),
                               "_Overwrite",
                               ptk::file_task::response::overwrite,
                               "Overwrite _All",
                               ptk::file_task::response::overwrite_all,
                               nullptr);
    }

    GtkWidget* btn_pause =
        gtk_dialog_add_button(GTK_DIALOG(dlg), "_Pause", ptk::file_task::response::pause);
    gtk_dialog_add_buttons(GTK_DIALOG(dlg),
                           "_Skip",
                           ptk::file_task::response::skip,
                           "S_kip All",
                           ptk::file_task::response::skip_all,
                           "Cancel",
                           GtkResponseType::GTK_RESPONSE_CANCEL,
                           nullptr);

    // xset_t set = xset_get(xset::name::TASK_PAUSE);
    gtk_widget_set_sensitive(btn_pause, !!ptask->task_view);

    // labels
    gtk_box_pack_start(vbox, gtk_label_new(nullptr), false, true, 0);
    GtkLabel* msg = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup(msg, message.data());
    gtk_widget_set_halign(GTK_WIDGET(msg), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(msg), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_can_focus(GTK_WIDGET(msg), false);
    gtk_box_pack_start(vbox, GTK_WIDGET(msg), false, true, 0);
    gtk_box_pack_start(vbox, gtk_label_new(nullptr), false, true, 0);
    GtkLabel* from_label = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup(from_label, from_disp.data());
    gtk_widget_set_halign(GTK_WIDGET(from_label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(from_label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_can_focus(GTK_WIDGET(from_label), false);
    gtk_box_pack_start(vbox, GTK_WIDGET(from_label), false, true, 0);

    GtkLabel* from_dir = GTK_LABEL(gtk_label_new(src_dir.data()));
    gtk_widget_set_halign(GTK_WIDGET(from_dir), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(from_dir), GtkAlign::GTK_ALIGN_START);
    gtk_label_set_ellipsize(from_dir, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable(from_dir, true);
    gtk_box_pack_start(vbox, GTK_WIDGET(from_dir), false, true, 0);

    if (!from_size_str.empty())
    {
        GtkLabel* from_size = GTK_LABEL(gtk_label_new(nullptr));
        gtk_label_set_markup(from_size, from_size_str.data());
        gtk_widget_set_halign(GTK_WIDGET(from_size), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(from_size), GtkAlign::GTK_ALIGN_END);
        gtk_label_set_selectable(from_size, true);
        gtk_box_pack_start(vbox, GTK_WIDGET(from_size), false, true, 0);
    }

    if (has_overwrite_btn || different_files)
    {
        gtk_box_pack_start(vbox, gtk_label_new(nullptr), false, true, 0);
        GtkLabel* to_label = GTK_LABEL(gtk_label_new(nullptr));
        gtk_label_set_markup(to_label, "To directory:");
        gtk_widget_set_halign(GTK_WIDGET(to_label), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(to_label), GtkAlign::GTK_ALIGN_START);
        gtk_box_pack_start(vbox, GTK_WIDGET(to_label), false, true, 0);

        GtkLabel* to_dir = GTK_LABEL(gtk_label_new(dest_dir.data()));
        gtk_widget_set_halign(GTK_WIDGET(to_dir), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(to_dir), GtkAlign::GTK_ALIGN_START);
        gtk_label_set_ellipsize(to_dir, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_selectable(to_dir, true);
        gtk_box_pack_start(vbox, GTK_WIDGET(to_dir), false, true, 0);

        if (!to_size_str.empty())
        {
            GtkLabel* to_size = GTK_LABEL(gtk_label_new(nullptr));
            gtk_label_set_markup(to_size, to_size_str.data());
            gtk_widget_set_halign(GTK_WIDGET(to_size), GtkAlign::GTK_ALIGN_START);
            gtk_widget_set_valign(GTK_WIDGET(to_size), GtkAlign::GTK_ALIGN_END);
            gtk_label_set_selectable(to_size, true);
            gtk_box_pack_start(vbox, GTK_WIDGET(to_size), false, true, 0);
        }
    }

    gtk_box_pack_start(vbox, gtk_label_new(nullptr), false, true, 0);
    GtkLabel* name_label = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup(name_label, is_dest_dir ? "<b>Directory Name:</b>" : "<b>Filename:</b>");
    gtk_widget_set_halign(GTK_WIDGET(name_label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(name_label), GtkAlign::GTK_ALIGN_START);
    gtk_box_pack_start(vbox, GTK_WIDGET(name_label), false, true, 0);

    // name input
    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    GtkWidget* query_input = GTK_WIDGET(multi_input_new(scroll, filename.data()));
    // clang-format off
    g_signal_connect(G_OBJECT(query_input), "key-press-event", G_CALLBACK(on_query_input_keypress), ptask);
    // clang-format on
    GtkWidget* input_buf = GTK_WIDGET(gtk_text_view_get_buffer(GTK_TEXT_VIEW(query_input)));
    gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(input_buf), &iter, pos);
    gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(input_buf), &iter);
    // clang-format off
    g_signal_connect(G_OBJECT(input_buf), "changed", G_CALLBACK(on_multi_input_changed), query_input);
    // clang-format on
    gtk_widget_set_size_request(GTK_WIDGET(query_input), -1, 60);
    gtk_widget_set_size_request(GTK_WIDGET(scroll), -1, 60);
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(query_input));
    GtkTextMark* mark = gtk_text_buffer_get_insert(buf);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(query_input), mark, 0, true, 0, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(scroll), true, true, 4);

    // extra buttons
    GtkButton* rename_button = GTK_BUTTON(gtk_button_new_with_mnemonic(" _Rename "));
    gtk_widget_set_sensitive(GTK_WIDGET(rename_button), false);
    g_signal_connect(G_OBJECT(rename_button), "clicked", G_CALLBACK(on_query_button_press), ptask);
    GtkButton* auto_button = GTK_BUTTON(gtk_button_new_with_mnemonic(" A_uto Rename "));
    g_signal_connect(G_OBJECT(auto_button), "clicked", G_CALLBACK(on_query_button_press), ptask);
    gtk_widget_set_tooltip_text(GTK_WIDGET(auto_button), new_name.data());
    GtkButton* auto_all_button = GTK_BUTTON(gtk_button_new_with_mnemonic(" Auto Re_name All "));
    // clang-format off
    g_signal_connect(G_OBJECT(auto_all_button), "clicked", G_CALLBACK(on_query_button_press), ptask);
    // clang-format on
    GtkBox* hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 30));
    gtk_widget_set_halign(GTK_WIDGET(hbox), GtkAlign::GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(hbox), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_hexpand(GTK_WIDGET(hbox), false);
    gtk_widget_set_vexpand(GTK_WIDGET(hbox), false);
    gtk_box_pack_start(hbox, GTK_WIDGET(rename_button), false, true, 0);
    gtk_box_pack_start(hbox, GTK_WIDGET(auto_button), false, true, 0);
    gtk_box_pack_start(hbox, GTK_WIDGET(auto_all_button), false, true, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(hbox), false, true, 0);

    // update displays (mutex is already locked)
    ptask->dsp_curspeed = "stalled";
    ptk_file_task_progress_update(ptask);
    if (ptask->task_view &&
        gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(ptask->task_view))))
    {
        main_task_view_update_task(ptask);
    }

    // show dialog
    g_object_set_data(G_OBJECT(dlg), "rename_button", rename_button);
    g_object_set_data(G_OBJECT(dlg), "auto_button", auto_button);
    g_object_set_data(G_OBJECT(dlg), "query_input", query_input);
    g_object_set_data(G_OBJECT(dlg), "has_overwrite_btn", GINT_TO_POINTER(has_overwrite_btn));
    gtk_widget_show_all(GTK_WIDGET(dlg));

    gtk_widget_grab_focus(query_input);

    // cannot run gtk_dialog_run here because it does not unlock a low level
    // mutex when run from inside the timer handler
    return;
}
