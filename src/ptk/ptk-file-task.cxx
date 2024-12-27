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

#include <algorithm>

#include <chrono>

#include <sys/wait.h>

#include <glibmm.h>

#if __has_include(<magic_enum/magic_enum.hpp>)
// >=magic_enum-0.9.7
#include <magic_enum/magic_enum.hpp>
#else
// <=magic_enum-0.9.6
#include <magic_enum.hpp>
#endif

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "compat/gtk4-porting.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"

#include "utils/strdup.hxx"

#include "vfs/utils/vfs-utils.hxx"

#include "ptk/ptk-file-task-view.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/utils/multi-input.hxx"
#include "ptk/utils/ptk-utils.hxx"

static bool on_vfs_file_task_state_cb(const std::shared_ptr<vfs::file_task>& task,
                                      const vfs::file_task::state state, void* state_data,
                                      void* user_data) noexcept;

ptk::file_task::file_task(const vfs::file_task::type type,
                          const std::span<const std::filesystem::path> src_files,
                          const std::filesystem::path& dest_dir, GtkWindow* parent_window,
                          GtkWidget* task_view) noexcept
    : parent_window_(parent_window), task_view_(task_view)
{
    this->task = vfs::file_task::create(type, src_files, dest_dir);

    this->task->set_state_callback(on_vfs_file_task_state_cb, this);
    if (xset_get_b(xset::name::task_err_any))
    {
        this->err_mode_ = ptk::file_task::ptask_error::any;
    }
    else if (xset_get_b(xset::name::task_err_first))
    {
        this->err_mode_ = ptk::file_task::ptask_error::first;
    }
    else
    {
        this->err_mode_ = ptk::file_task::ptask_error::cont;
    }

    GtkTextIter iter;
    this->log_buf_ = gtk_text_buffer_new(nullptr);
    this->log_end_ = gtk_text_mark_new(nullptr, false);
    gtk_text_buffer_get_end_iter(this->log_buf_, &iter);
    gtk_text_buffer_add_mark(this->log_buf_, this->log_end_, &iter);

    // queue task
    if (this->task->exec_sync && this->task->type_ != vfs::file_task::type::exec &&
        this->task->type_ != vfs::file_task::type::link &&
        this->task->type_ != vfs::file_task::type::chmod_chown &&
        xset_get_b(xset::name::task_q_new))
    {
        this->pause(vfs::file_task::state::queue);
    }

    // GThread *self = g_thread_self();
    // logger::info<logger::domain::ptk>("GUI_THREAD = {}", logger::utils::ptr(self));
    // logger::info<logger::domain::ptk>("ptk::file_task::file_task({}) DONE", logger::utils::ptr(this));
}

ptk::file_task::~file_task() noexcept
{
    // logger::info<logger::domain::ptk>("ptk::file_task::~file_task({})", logger::utils::ptr(this));
    if (this->timeout_)
    {
        g_source_remove(this->timeout_);
        this->timeout_ = 0;
    }
    if (this->progress_timer_)
    {
        g_source_remove(this->progress_timer_);
        this->progress_timer_ = 0;
    }
    ptk::view::file_task::remove_task(this);
    ptk::view::file_task::start_queued(this->task_view_, nullptr);

    if (this->progress_dlg_)
    {
        this->save_progress_dialog_size();

        if (this->overwrite_combo_)
        {
            gtk_combo_box_popdown(GTK_COMBO_BOX(this->overwrite_combo_));
        }
        if (this->error_combo_)
        {
            gtk_combo_box_popdown(GTK_COMBO_BOX(this->error_combo_));
        }
        gtk_widget_destroy(this->progress_dlg_);
        this->progress_dlg_ = nullptr;
    }

    gtk_text_buffer_set_text(this->log_buf_, "", -1);
    g_object_unref(this->log_buf_);

    // logger::info<logger::domain::ptk>("ptk::file_task::~file_task({}) DONE", logger::utils::ptr(this));
}

void
ptk::file_task::lock() noexcept
{
    g_mutex_lock(this->task->mutex);
}

void
ptk::file_task::unlock() noexcept
{
    g_mutex_unlock(this->task->mutex);
}

bool
ptk::file_task::trylock() noexcept
{
    return g_mutex_trylock(this->task->mutex);
}

bool
ptk::file_task::is_completed() const noexcept
{
    return this->complete_;
}

bool
ptk::file_task::is_aborted() const noexcept
{
    return this->aborted_;
}

std::string_view
ptk::file_task::display_file_count() const noexcept
{
    return this->display_file_count_;
}

std::string_view
ptk::file_task::display_size_tally() const noexcept
{
    return this->display_size_tally_;
}

std::string_view
ptk::file_task::display_elapsed() const noexcept
{
    return this->display_elapsed_;
}

std::string_view
ptk::file_task::display_current_speed() const noexcept
{
    return this->display_current_speed_;
}

std::string_view
ptk::file_task::display_current_estimate() const noexcept
{
    return this->display_current_estimate_;
}

std::string_view
ptk::file_task::display_average_speed() const noexcept
{
    return this->display_average_speed_;
}

std::string_view
ptk::file_task::display_average_estimate() const noexcept
{
    return this->display_average_estimate_;
}

ptk::file_task*
ptk_file_task_new(const vfs::file_task::type type,
                  const std::span<const std::filesystem::path> src_files, GtkWindow* parent_window,
                  GtkWidget* task_view) noexcept
{
    auto* const ptask = new ptk::file_task(type, src_files, "", parent_window, task_view);

    return ptask;
}

ptk::file_task*
ptk_file_task_new(const vfs::file_task::type type,
                  const std::span<const std::filesystem::path> src_files,
                  const std::filesystem::path& dest_dir, GtkWindow* parent_window,
                  GtkWidget* task_view) noexcept
{
    auto* const ptask = new ptk::file_task(type, src_files, dest_dir, parent_window, task_view);

    return ptask;
}

ptk::file_task*
ptk_file_exec_new(const std::string_view item_name, GtkWidget* parent,
                  GtkWidget* task_view) noexcept
{
    GtkWidget* parent_win = nullptr;
    if (parent)
    {
#if (GTK_MAJOR_VERSION == 4)
        parent_win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(parent)));
#elif (GTK_MAJOR_VERSION == 3)
        parent_win = gtk_widget_get_toplevel(GTK_WIDGET(parent));
#endif
    }

    const std::vector<std::filesystem::path> file_list{item_name.data()};
    auto* const ptask = new ptk::file_task(vfs::file_task::type::exec,
                                           file_list,
                                           "",
                                           GTK_WINDOW(parent_win),
                                           task_view);

    return ptask;
}

ptk::file_task*
ptk_file_exec_new(const std::string_view item_name, const std::filesystem::path& dest_dir,
                  GtkWidget* parent, GtkWidget* task_view) noexcept
{
    GtkWidget* parent_win = nullptr;
    if (parent)
    {
#if (GTK_MAJOR_VERSION == 4)
        parent_win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(parent)));
#elif (GTK_MAJOR_VERSION == 3)
        parent_win = gtk_widget_get_toplevel(GTK_WIDGET(parent));
#endif
    }

    const std::vector<std::filesystem::path> file_list{item_name.data()};
    auto* const ptask = new ptk::file_task(vfs::file_task::type::exec,
                                           file_list,
                                           dest_dir,
                                           GTK_WINDOW(parent_win),
                                           task_view);

    return ptask;
}

void
ptk::file_task::save_progress_dialog_size() const noexcept
{
#if 0
    // save dialog size  - do this here now because as of GTK 3.8,
    // allocation == 1,1 in destroy event
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(this->progress_dlg), &allocation);

    const std::string width = std::format("{}", allocation.width);
    if (this->task->type_ == vfs::file_task::type::exec)
    {
        xset_set(xset::name::task_pop_top, xset::var::s, width);
    }
    else
    {
        xset_set(xset::name::task_pop_top, xset::var::x, width);
    }

    const std::string height = std::format("{}", allocation.height);
    if (this->task->type_ == vfs::file_task::type::exec)
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
ptk::file_task::set_complete_notify(GFunc callback, void* user_data) noexcept
{
    this->complete_notify_ = callback;
    this->user_data_ = user_data;
}

static bool
on_progress_timer(ptk::file_task* ptask) noexcept
{
    // GThread *self = g_thread_self ();
    // logger::info<logger::domain::ptk>("PROGRESS_TIMER_THREAD = {}", logger::utils::ptr(self));

    // query condition?
    if (ptask->query_cond_ && ptask->query_cond_ != ptask->query_cond_last_)
    {
        // logger::info<logger::domain::ptk>("QUERY = {}  mutex = {}", logger::utils::ptr(ptask->query_cond), logger::utils::ptr(ptask->task->mutex));
        ptask->restart_timeout_ = (ptask->timeout_ != 0);
        if (ptask->timeout_)
        {
            g_source_remove(ptask->timeout_);
            ptask->timeout_ = 0;
        }
        if (ptask->progress_timer_)
        {
            g_source_remove(ptask->progress_timer_);
            ptask->progress_timer_ = 0;
        }

        ptask->lock();
        ptask->query_overwrite();
        ptask->unlock();
        return false;
    }

    // start new queued task
    if (ptask->task->queue_start)
    {
        ptask->task->queue_start = false;
        if (ptask->task->state_pause_ == vfs::file_task::state::running)
        {
            ptask->pause(vfs::file_task::state::running);
        }
        else
        {
            ptk::view::file_task::start_queued(ptask->task_view_, ptask);
        }
        if (ptask->timeout_ && ptask->task->state_pause_ != vfs::file_task::state::running &&
            ptask->task->state_ == vfs::file_task::state::running)
        {
            // task is waiting in queue so list it
            g_source_remove(ptask->timeout_);
            ptask->timeout_ = 0;
        }
    }

    // only update every 300ms (6 * 50ms)
    if (++ptask->progress_count_ < 6)
    {
        return true;
    }
    ptask->progress_count_ = 0;
    // logger::info<logger::domain::ptk>("on_progress_timer ptask={}", logger::utils::ptr(ptask));

    if (ptask->is_completed())
    {
        if (ptask->progress_timer_)
        {
            g_source_remove(ptask->progress_timer_);
            ptask->progress_timer_ = 0;
        }
        if (ptask->complete_notify_)
        {
            ptask->complete_notify_(ptask->task.get(), ptask->user_data_);
            ptask->complete_notify_ = nullptr;
        }
        ptk::view::file_task::remove_task(ptask);
        ptk::view::file_task::start_queued(ptask->task_view_, nullptr);
    }
    else if (ptask->task->state_pause_ != vfs::file_task::state::running && !ptask->pause_change_ &&
             ptask->task->type_ != vfs::file_task::type::exec)
    {
        return true;
    }

    ptask->update();

    if (ptask->is_completed())
    {
        if (!ptask->progress_dlg_ || (!ptask->err_count_ && !ptask->keep_dlg_))
        {
            delete ptask;
            // logger::info<logger::domain::ptk>("on_progress_timer DONE false-COMPLETE ptask={}", logger::utils::ptr(ptask));
            return false;
        }
        else if (ptask->progress_dlg_ && ptask->err_count_)
        {
            gtk_window_present(GTK_WINDOW(ptask->progress_dlg_));
        }
    }
    // logger::info<logger::domain::ptk>("on_progress_timer DONE true ptask={}", logger::utils::ptr(ptask));
    return !ptask->is_completed();
}

static bool
ptk_file_task_add_main(ptk::file_task* ptask) noexcept
{
    // logger::info<logger::domain::ptk>("ptk_file_task_add_main ptask={}", logger::utils::ptr(ptask));
    if (ptask->timeout_)
    {
        g_source_remove(ptask->timeout_);
        ptask->timeout_ = 0;
    }

    if (ptask->task->exec_popup || xset_get_b(xset::name::task_pop_all))
    {
        // keep dlg if Popup Task is explicitly checked, otherwise close if no
        // error
        ptask->keep_dlg_ = ptask->keep_dlg_ || ptask->task->exec_popup;
        ptask->progress_open();
    }

    if (ptask->task->state_pause_ != vfs::file_task::state::running && !ptask->pause_change_)
    {
        ptask->pause_change_ = ptask->pause_change_view_ = true;
    }

    on_progress_timer(ptask);

    // logger::info<logger::domain::ptk>("ptk_file_task_add_main DONE ptask={}", logger::utils::ptr(ptask));
    return false;
}

void
ptk::file_task::run() noexcept
{
    // logger::info<logger::domain::ptk>("ptk::file_task::run({})", logger::utils::ptr(this));
    // wait this long to first show task in manager, popup
    this->timeout_ = g_timeout_add(500, (GSourceFunc)ptk_file_task_add_main, this);
    this->progress_timer_ = 0;
    this->task->run_task();
    if (this->task->type_ == vfs::file_task::type::exec)
    {
        if ((this->complete_ || !this->task->exec_sync) && this->timeout_)
        {
            g_source_remove(this->timeout_);
            this->timeout_ = 0;
        }
    }
    this->progress_timer_ = g_timeout_add(50, (GSourceFunc)on_progress_timer, this);
    // logger::info<logger::domain::ptk>("ptk::file_task::run({}) DONE", logger::utils::ptr(this));
}

bool
ptk::file_task::cancel() noexcept
{
    // GThread *self = g_thread_self ();
    // logger::info<logger::domain::ptk>("CANCEL_THREAD = {}", logger::utils::ptr(self));
    if (this->timeout_)
    {
        g_source_remove(this->timeout_);
        this->timeout_ = 0;
    }
    this->aborted_ = true;
    if (this->task->type_ == vfs::file_task::type::exec)
    {
        this->keep_dlg_ = true;

        // resume task for task list responsiveness
        if (this->task->state_pause_ != vfs::file_task::state::running)
        {
            this->pause(vfs::file_task::state::running);
        }

        this->task->abort_task();

        // no pid (exited)
        // user pressed Stop on an exited process, remove task
        // this may be needed because if process is killed, channels may not
        // receive HUP and may remain open, leaving the task listed
        this->complete_ = true;

        if (this->task->exec_cond)
        {
            // this is used only if exec task run in non-main loop thread
            this->lock();
            if (this->task->exec_cond)
            {
                g_cond_broadcast(this->task->exec_cond);
            }
            this->unlock();
        }
    }
    else
    {
        this->task->try_abort_task();
    }
    return false;
}

void
ptk::file_task::set_button_states() noexcept
{
    if (!this->progress_dlg_)
    {
        return;
    }

    std::string label;

    switch (this->task->state_pause_)
    {
        case vfs::file_task::state::pause:
            label = "Q_ueue";
            // iconset = ::utils::strdup("task_que");
            //  icon = "list-add";
            break;
        case vfs::file_task::state::queue:
            label = "Res_ume";
            // iconset = ::utils::strdup("task_resume");
            //  icon = "media-playback-start";
            break;
        case vfs::file_task::state::running:
        case vfs::file_task::state::size_timeout:
        case vfs::file_task::state::query_overwrite:
        case vfs::file_task::state::error:
        case vfs::file_task::state::finish:
            label = "Pa_use";
            // iconset = ::utils::strdup("task_pause");
            //  icon = "media-playback-pause";
            break;
    }
    const bool sens = !this->complete_ && !(this->task->type_ == vfs::file_task::type::exec);

    gtk_widget_set_sensitive(GTK_WIDGET(this->progress_btn_pause_), sens);
    gtk_button_set_label(this->progress_btn_pause_, label.data());
    gtk_widget_set_sensitive(GTK_WIDGET(this->progress_btn_close_),
                             this->complete_ || !!this->task_view_);
}

void
ptk::file_task::pause(const vfs::file_task::state state) noexcept
{
    if (state == vfs::file_task::state::pause)
    {
        this->task->state_pause_ = vfs::file_task::state::pause;
    }
    else if (state == vfs::file_task::state::queue)
    {
        this->task->state_pause_ = vfs::file_task::state::queue;
    }
    else
    {
        // Resume
        if (this->task->pause_cond)
        {
            this->lock();
            g_cond_broadcast(this->task->pause_cond);
            this->unlock();
        }
        this->task->state_pause_ = vfs::file_task::state::running;
    }
    this->set_button_states();
    this->pause_change_ = this->pause_change_view_ = true;
    this->progress_count_ = 50; // trigger fast display
}

static bool
on_progress_dlg_delete_event(GtkWidget* widget, GdkEvent* event, ptk::file_task* ptask) noexcept
{
    (void)widget;
    (void)event;
    ptask->save_progress_dialog_size();
    return !(ptask->is_completed() || ptask->task_view_);
}

static void
on_progress_dlg_response(GtkDialog* dlg, i32 response, ptk::file_task* ptask) noexcept
{
    (void)dlg;
    ptask->save_progress_dialog_size();
    if (ptask->is_completed() && !ptask->complete_notify_)
    {
        delete ptask;
        return;
    }
    switch (response)
    {
        case GtkResponseType::GTK_RESPONSE_CANCEL: // Stop btn
            ptask->keep_dlg_ = false;
            if (ptask->overwrite_combo_)
            {
                gtk_combo_box_popdown(GTK_COMBO_BOX(ptask->overwrite_combo_));
            }
            if (ptask->error_combo_)
            {
                gtk_combo_box_popdown(GTK_COMBO_BOX(ptask->error_combo_));
            }
            gtk_widget_destroy(ptask->progress_dlg_);
            ptask->progress_dlg_ = nullptr;
            ptask->cancel();
            break;
        case GtkResponseType::GTK_RESPONSE_NO: // Pause btn
            if (ptask->task->state_pause_ == vfs::file_task::state::pause)
            {
                ptask->pause(vfs::file_task::state::queue);
            }
            else if (ptask->task->state_pause_ == vfs::file_task::state::queue)
            {
                ptask->pause(vfs::file_task::state::running);
            }
            else
            {
                ptask->pause(vfs::file_task::state::pause);
            }
            ptk::view::file_task::start_queued(ptask->task_view_, nullptr);
            break;
        case GtkResponseType::GTK_RESPONSE_OK:
        case GtkResponseType::GTK_RESPONSE_NONE:
            ptask->keep_dlg_ = false;
            if (ptask->overwrite_combo_)
            {
                gtk_combo_box_popdown(GTK_COMBO_BOX(ptask->overwrite_combo_));
            }
            if (ptask->error_combo_)
            {
                gtk_combo_box_popdown(GTK_COMBO_BOX(ptask->error_combo_));
            }
            gtk_widget_destroy(ptask->progress_dlg_);
            ptask->progress_dlg_ = nullptr;
            break;
        default:
            break;
    }
}

static void
on_progress_dlg_destroy(GtkDialog* dlg, ptk::file_task* ptask) noexcept
{
    (void)dlg;
    ptask->progress_dlg_ = nullptr;
}

static void
on_view_popup(GtkTextView* entry, GtkMenu* menu, void* user_data) noexcept
{
    (void)entry;
    (void)user_data;

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

    const auto set = xset::set::get(xset::name::separator);
    set->browser = nullptr;
    xset_add_menuitem(nullptr, GTK_WIDGET(menu), accel_group, set);
    gtk_widget_show_all(GTK_WIDGET(menu));
}

void
ptk::file_task::set_progress_icon() noexcept
{
    std::string icon;

    if (this->task->state_pause_ != vfs::file_task::state::running)
    {
        icon = "media-playback-pause";
    }
    else if (this->task->err_count)
    {
        icon = "error";
    }
    else if (this->task->type_ == vfs::file_task::type::move ||
             this->task->type_ == vfs::file_task::type::copy ||
             this->task->type_ == vfs::file_task::type::link ||
             this->task->type_ == vfs::file_task::type::trash)
    {
        icon = "stock_copy";
    }
    else if (this->task->type_ == vfs::file_task::type::del)
    {
        icon = "stock_delete";
    }
    else if (this->task->type_ == vfs::file_task::type::exec && !this->task->exec_icon.empty())
    {
        icon = this->task->exec_icon;
    }
    else
    {
        icon = "gtk-execute";
    }
    gtk_window_set_icon_name(GTK_WINDOW(this->progress_dlg_), icon.data());
}

static void
on_overwrite_combo_changed(GtkComboBox* box, ptk::file_task* ptask) noexcept
{
    i32 overwrite_mode = gtk_combo_box_get_active(box);
    overwrite_mode = std::max(overwrite_mode, 0);
    ptask->task->set_overwrite_mode(vfs::file_task::overwrite_mode(overwrite_mode));
}

static void
on_error_combo_changed(GtkComboBox* box, ptk::file_task* ptask) noexcept
{
    i32 error_mode = gtk_combo_box_get_active(box);
    error_mode = std::max(error_mode, 0);
    ptask->err_mode_ = ptk::file_task::ptask_error(error_mode);
}

void
ptk::file_task::progress_open() noexcept
{
    static constexpr ztd::map<vfs::file_task::type, std::string_view, 7> job_actions{{
        {vfs::file_task::type::move, "Move: "},
        {vfs::file_task::type::copy, "Copy: "},
        {vfs::file_task::type::trash, "Trash: "},
        {vfs::file_task::type::del, "Delete: "},
        {vfs::file_task::type::link, "Link: "},
        {vfs::file_task::type::chmod_chown, "Change: "},
        {vfs::file_task::type::exec, "Run: "},
    }};
    static constexpr ztd::map<vfs::file_task::type, std::string_view, 7> job_titles{{
        {vfs::file_task::type::move, "Moving..."},
        {vfs::file_task::type::copy, "Copying..."},
        {vfs::file_task::type::trash, "Trashing..."},
        {vfs::file_task::type::del, "Deleting..."},
        {vfs::file_task::type::link, "Linking..."},
        {vfs::file_task::type::chmod_chown, "Changing..."},
        {vfs::file_task::type::exec, "Running..."},
    }};

    if (this->progress_dlg_)
    {
        return;
    }

    // logger::info<logger::domain::ptk>("ptk::file_task::progress_open");

    // create dialog
    this->progress_dlg_ =
        gtk_dialog_new_with_buttons(job_titles.at(this->task->type_).data(),
                                    this->parent_window_,
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    nullptr,
                                    nullptr);

    gtk_window_set_resizable(GTK_WINDOW(this->progress_dlg_), false);

    // cache this value for speed
    this->pop_detail_ = xset_get_b(xset::name::task_pop_detail);

    // Buttons
    // Pause
    // xset_t set = xset::set::get(xset::name::TASK_PAUSE);

    this->progress_btn_pause_ = GTK_BUTTON(gtk_button_new_with_mnemonic("Pa_use"));

    gtk_dialog_add_action_widget(GTK_DIALOG(this->progress_dlg_),
                                 GTK_WIDGET(this->progress_btn_pause_),
                                 GtkResponseType::GTK_RESPONSE_NO);
    gtk_widget_set_focus_on_click(GTK_WIDGET(this->progress_btn_pause_), false);
    // Stop
    this->progress_btn_stop_ = GTK_BUTTON(gtk_button_new_with_label("Stop"));
    gtk_dialog_add_action_widget(GTK_DIALOG(this->progress_dlg_),
                                 GTK_WIDGET(this->progress_btn_stop_),
                                 GtkResponseType::GTK_RESPONSE_CANCEL);
    gtk_widget_set_focus_on_click(GTK_WIDGET(this->progress_btn_stop_), false);
    // Close
    this->progress_btn_close_ = GTK_BUTTON(gtk_button_new_with_label("Close"));
    gtk_dialog_add_action_widget(GTK_DIALOG(this->progress_dlg_),
                                 GTK_WIDGET(this->progress_btn_close_),
                                 GtkResponseType::GTK_RESPONSE_OK);
    gtk_widget_set_sensitive(GTK_WIDGET(this->progress_btn_close_), !!this->task_view_);

    this->set_button_states();

    GtkGrid* grid = GTK_GRID(gtk_grid_new());

    gtk_widget_set_margin_start(GTK_WIDGET(grid), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(grid), 5);
    gtk_widget_set_margin_top(GTK_WIDGET(grid), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(grid), 5);

    gtk_grid_set_row_spacing(grid, 6);
    gtk_grid_set_column_spacing(grid, 4);
    i32 row = 0;

    /* Copy/Move/Link: */
    GtkLabel* label = GTK_LABEL(gtk_label_new(job_actions.at(this->task->type_).data()));
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
    this->from_ = GTK_LABEL(
        gtk_label_new(this->complete_ ? "" : this->task->current_file.value_or("").c_str()));
    gtk_widget_set_halign(GTK_WIDGET(this->from_), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(this->from_), GtkAlign::GTK_ALIGN_CENTER);
    gtk_label_set_ellipsize(this->from_, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable(this->from_, true);
    gtk_grid_attach(grid, GTK_WIDGET(this->from_), 1, row, 1, 1);

    if (this->task->type_ != vfs::file_task::type::exec)
    {
        // From: <src directory>
        row++;
        label = GTK_LABEL(gtk_label_new("From:"));
        gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
        gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
        this->src_dir_ = GTK_LABEL(gtk_label_new(nullptr));
        gtk_widget_set_halign(GTK_WIDGET(this->src_dir_), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(this->src_dir_), GtkAlign::GTK_ALIGN_CENTER);
        gtk_label_set_ellipsize(this->src_dir_, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_selectable(this->src_dir_, true);
        gtk_grid_attach(grid, GTK_WIDGET(this->src_dir_), 1, row, 1, 1);
        if (this->task->dest_dir)
        {
            const auto dest_dir = this->task->dest_dir.value();
            /* To: <Destination directory>
            ex. Copy file to..., Move file to...etc. */
            row++;
            label = GTK_LABEL(gtk_label_new("To:"));
            gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
            gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
            gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
            this->to_ = GTK_LABEL(gtk_label_new(dest_dir.c_str()));
            gtk_widget_set_halign(GTK_WIDGET(this->to_), GtkAlign::GTK_ALIGN_START);
            gtk_widget_set_valign(GTK_WIDGET(this->to_), GtkAlign::GTK_ALIGN_CENTER);
            gtk_label_set_ellipsize(this->to_, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
            gtk_label_set_selectable(this->to_, true);
            gtk_grid_attach(grid, GTK_WIDGET(this->to_), 1, row, 1, 1);
        }
        else
        {
            this->to_ = nullptr;
        }

        // Stats
        row++;
        label = GTK_LABEL(gtk_label_new("Progress:  "));
        gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
        gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
        this->current_ = GTK_LABEL(gtk_label_new(""));
        gtk_widget_set_halign(GTK_WIDGET(this->current_), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(this->current_), GtkAlign::GTK_ALIGN_CENTER);
        gtk_label_set_ellipsize(this->current_, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_selectable(this->current_, true);
        gtk_grid_attach(grid, GTK_WIDGET(this->current_), 1, row, 1, 1);
    }
    else
    {
        this->src_dir_ = nullptr;
        this->to_ = nullptr;
    }

    // Status
    row++;
    label = GTK_LABEL(gtk_label_new("Status:  "));
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    gtk_grid_attach(grid, GTK_WIDGET(label), 0, row, 1, 1);
    std::string status;
    if (this->task->state_pause_ == vfs::file_task::state::pause)
    {
        status = "Paused";
    }
    else if (this->task->state_pause_ == vfs::file_task::state::queue)
    {
        status = "Queued";
    }
    else
    {
        status = "Running...";
    }
    this->errors_ = GTK_LABEL(gtk_label_new(status.data()));
    gtk_widget_set_halign(GTK_WIDGET(this->errors_), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(this->errors_), GtkAlign::GTK_ALIGN_CENTER);
    gtk_label_set_ellipsize(this->errors_, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable(this->errors_, true);
    gtk_grid_attach(grid, GTK_WIDGET(this->errors_), 1, row, 1, 1);

    /* Progress: */
    row++;
    this->progress_bar_ = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(this->progress_bar_), true);
    gtk_progress_bar_set_pulse_step(this->progress_bar_, 0.08);
    gtk_grid_attach(grid, GTK_WIDGET(this->progress_bar_), 0, row, 1, 1);

    // Error log
    this->scroll_ = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    gtk_widget_set_halign(GTK_WIDGET(this->scroll_), GtkAlign::GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(this->scroll_), GtkAlign::GTK_ALIGN_END);
    // gtk_widget_set_hexpand(GTK_WIDGET(this->scroll), false);
    // gtk_widget_set_vexpand(GTK_WIDGET(this->scroll), false);
    gtk_widget_set_margin_start(GTK_WIDGET(this->scroll_), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(this->scroll_), 5);
    gtk_widget_set_margin_top(GTK_WIDGET(this->scroll_), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(this->scroll_), 0);
    this->error_view_ = gtk_text_view_new_with_buffer(this->log_buf_);
    // gtk_widget_set_halign(GTK_WIDGET(this->error_view), GtkAlign::GTK_ALIGN_END);
    // gtk_widget_set_valign(GTK_WIDGET(this->error_view), GtkAlign::GTK_ALIGN_END);
    // gtk_widget_set_hexpand(GTK_WIDGET(this->error_view), false);
    // gtk_widget_set_vexpand(GTK_WIDGET(this->error_view), false);
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request(GTK_WIDGET(this->error_view_), 600, 300);
    gtk_widget_set_size_request(GTK_WIDGET(this->scroll_), 600, 300);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->scroll_),
                                  GTK_WIDGET(this->error_view_));
    gtk_scrolled_window_set_policy(this->scroll_,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(this->error_view_), GtkWrapMode::GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(this->error_view_), false);

    // clang-format off
    g_signal_connect(G_OBJECT(this->error_view_), "populate-popup", G_CALLBACK(on_view_popup), nullptr);
    // clang-format on

    // Overwrite & Error
    GtkBox* overwrite_box = nullptr;
    if (this->task->type_ != vfs::file_task::type::exec)
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

        const bool overtask = this->task->type_ == vfs::file_task::type::move ||
                              this->task->type_ == vfs::file_task::type::copy ||
                              this->task->type_ == vfs::file_task::type::link;
        this->overwrite_combo_ = gtk_combo_box_text_new();
        gtk_widget_set_focus_on_click(GTK_WIDGET(this->overwrite_combo_), false);
        gtk_widget_set_sensitive(this->overwrite_combo_, overtask);
        for (const std::string_view overwrite_option : overwrite_options)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(this->overwrite_combo_),
                                           overwrite_option.data());
        }
        if (overtask)
        {
            gtk_combo_box_set_active(
                GTK_COMBO_BOX(this->overwrite_combo_),
                (this->task->overwrite_mode_ == vfs::file_task::overwrite_mode::overwrite ||
                 this->task->overwrite_mode_ == vfs::file_task::overwrite_mode::overwrite_all ||
                 this->task->overwrite_mode_ == vfs::file_task::overwrite_mode::skip_all ||
                 this->task->overwrite_mode_ == vfs::file_task::overwrite_mode::auto_rename)
                    ? magic_enum::enum_integer(this->task->overwrite_mode_)
                    : 0);
        }
        // clang-format off
        g_signal_connect(G_OBJECT(this->overwrite_combo_), "changed", G_CALLBACK(on_overwrite_combo_changed), this);
        // clang-format on

        this->error_combo_ = gtk_combo_box_text_new();
        gtk_widget_set_focus_on_click(GTK_WIDGET(this->error_combo_), false);
        for (const std::string_view error_option : error_options)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(this->error_combo_),
                                           error_option.data());
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(this->error_combo_),
                                 magic_enum::enum_integer(this->err_mode_));

        // clang-format off
        g_signal_connect(G_OBJECT(this->error_combo_), "changed", G_CALLBACK(on_error_combo_changed), this);
        // clang-format on

        overwrite_box = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 20));
        gtk_box_pack_start(overwrite_box, GTK_WIDGET(this->overwrite_combo_), false, true, 0);
        gtk_box_pack_start(overwrite_box, GTK_WIDGET(this->error_combo_), false, true, 0);

        gtk_widget_set_halign(GTK_WIDGET(overwrite_box), GtkAlign::GTK_ALIGN_END);
        gtk_widget_set_valign(GTK_WIDGET(overwrite_box), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_hexpand(GTK_WIDGET(overwrite_box), true);
        gtk_widget_set_vexpand(GTK_WIDGET(overwrite_box), true);
        gtk_widget_set_margin_start(GTK_WIDGET(overwrite_box), 5);
        gtk_widget_set_margin_end(GTK_WIDGET(overwrite_box), 5);
        gtk_widget_set_margin_top(GTK_WIDGET(overwrite_box), 0);
        gtk_widget_set_margin_bottom(GTK_WIDGET(overwrite_box), 0);
    }
    else
    {
        this->overwrite_combo_ = nullptr;
        this->error_combo_ = nullptr;
    }

    // Pack
    GtkBox* progress_dlg = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(this->progress_dlg_)));
    gtk_widget_set_hexpand(GTK_WIDGET(progress_dlg), true);
    gtk_widget_set_vexpand(GTK_WIDGET(progress_dlg), true);

    gtk_box_pack_start(GTK_BOX(progress_dlg), GTK_WIDGET(grid), false, true, 0);
    gtk_box_pack_start(GTK_BOX(progress_dlg), GTK_WIDGET(this->scroll_), true, true, 0);

    if (overwrite_box)
    {
        gtk_box_pack_start(GTK_BOX(progress_dlg), GTK_WIDGET(overwrite_box), false, true, 5);
    }

#if 0
    i32 win_width, win_height;
    if (this->task->type_ == vfs::file_task::type::exec)
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
    gtk_window_set_default_size(GTK_WINDOW(this->progress_dlg), win_width, win_height);
#endif

#if (GTK_MAJOR_VERSION == 3)
    if (xset_get_b(xset::name::task_pop_top))
    {
        gtk_window_set_type_hint(GTK_WINDOW(this->progress_dlg_),
                                 GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
    }
    else
    {
        gtk_window_set_type_hint(GTK_WINDOW(this->progress_dlg_),
                                 GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_NORMAL);
    }
    if (xset_get_b(xset::name::task_pop_above))
    {
        gtk_window_set_keep_above(GTK_WINDOW(this->progress_dlg_), true);
    }
    if (xset_get_b(xset::name::task_pop_stick))
    {
        gtk_window_stick(GTK_WINDOW(this->progress_dlg_));
    }
    gtk_window_set_gravity(GTK_WINDOW(this->progress_dlg_), GdkGravity::GDK_GRAVITY_NORTH_EAST);
    gtk_window_set_position(GTK_WINDOW(this->progress_dlg_), GtkWindowPosition::GTK_WIN_POS_CENTER);
#endif

    // clang-format off
    // gtk_dialog_set_default_response(this->progress_dlg, GtkResponseType::GTK_RESPONSE_OK);
    g_signal_connect(G_OBJECT(this->progress_dlg_), "response", G_CALLBACK(on_progress_dlg_response), this);
    g_signal_connect(G_OBJECT(this->progress_dlg_), "destroy", G_CALLBACK(on_progress_dlg_destroy), this);
    g_signal_connect(G_OBJECT(this->progress_dlg_), "delete-event", G_CALLBACK(on_progress_dlg_delete_event), this);
    // g_signal_connect(G_OBJECT(this->progress_dlg), "configure-event", G_CALLBACK(on_progress_configure_event), this);
    // clang-format on

    gtk_widget_show_all(GTK_WIDGET(this->progress_dlg_));
    if (this->overwrite_combo_ && !xset_get_b(xset::name::task_pop_over))
    {
        gtk_widget_hide(GTK_WIDGET(this->overwrite_combo_));
    }
    if (this->error_combo_ && !xset_get_b(xset::name::task_pop_err))
    {
        gtk_widget_hide(GTK_WIDGET(this->error_combo_));
    }
    if (overwrite_box && !gtk_widget_get_visible(this->overwrite_combo_) &&
        !gtk_widget_get_visible(this->error_combo_))
    {
        gtk_widget_hide(GTK_WIDGET(overwrite_box));
    }
    gtk_widget_grab_focus(GTK_WIDGET(this->progress_btn_close_));

    // icon
    this->set_progress_icon();

    // auto scroll - must be after show_all
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(this->error_view_),
                                 this->log_end_,
                                 0.0,
                                 false,
                                 0,
                                 0);

    this->progress_count_ = 50; // trigger fast display
    // logger::info<logger::domain::ptk>("ptk::file_task::progress_open DONE");
}

void
ptk::file_task::progress_update() noexcept
{
    if (!this->progress_dlg_)
    {
        if (this->pause_change_)
        {
            this->pause_change_ = false; // stop elapsed timer
        }
        return;
    }

    // logger::info<logger::domain::ptk>("ptk::file_task::progress_update({})", logger::utils::ptr(this));

    std::string ufile_path;

    // current file
    std::filesystem::path usrc_dir;
    std::filesystem::path udest;

    if (this->complete_)
    {
        gtk_widget_set_sensitive(GTK_WIDGET(this->progress_btn_stop_), false);
        gtk_widget_set_sensitive(GTK_WIDGET(this->progress_btn_pause_), false);
        gtk_widget_set_sensitive(GTK_WIDGET(this->progress_btn_close_), true);
        if (this->overwrite_combo_)
        {
            gtk_widget_set_sensitive(this->overwrite_combo_, false);
        }
        if (this->error_combo_)
        {
            gtk_widget_set_sensitive(this->error_combo_, false);
        }

        if (this->task->type_ == vfs::file_task::type::exec)
        {
            if (this->task->current_file)
            {
                const auto current_file = this->task->current_file.value();

                const std::string escaped_markup = Glib::Markup::escape_text(current_file.string());
                ufile_path = std::format("<b>{}</b>", escaped_markup);
            }
        }

        std::string window_title;
        if (this->aborted_)
        {
            window_title = "Stopped";
        }
        else
        {
            if (this->task->err_count)
            {
                window_title = "Errors";
            }
            else
            {
                window_title = "Done";
            }
        }
        gtk_window_set_title(GTK_WINDOW(this->progress_dlg_), window_title.c_str());
        if (ufile_path.empty())
        {
            const std::string escaped_markup = Glib::Markup::escape_text(window_title);
            ufile_path = std::format("<b>( {} )</b>", escaped_markup);
        }
    }
    else if (this->task->current_file)
    {
        const auto current_file = this->task->current_file.value();

        if (this->task->type_ != vfs::file_task::type::exec)
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
            if (this->task->current_dest)
            {
                const auto current_dest = this->task->current_dest.value();

                const auto current_file_filename = current_file.filename();
                const auto current_dest_filename = current_dest.filename();
                if (current_file_filename != current_dest_filename)
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

    if (udest.empty() && !this->complete_ && this->task->dest_dir)
    {
        udest = this->task->dest_dir.value();
    }
    gtk_label_set_markup(this->from_, ufile_path.c_str());
    if (this->src_dir_)
    {
        gtk_label_set_text(this->src_dir_, usrc_dir.c_str());
    }
    if (this->to_)
    {
        gtk_label_set_text(this->to_, udest.c_str());
    }

    // progress bar
    if (this->task->type_ != vfs::file_task::type::exec || this->task->custom_percent)
    {
        if (this->task->percent >= 0)
        {
            this->task->percent = std::min(this->task->percent, 100);
            gtk_progress_bar_set_fraction(this->progress_bar_, ((f64)this->task->percent) / 100);
            const std::string percent_str = std::format("{} %", this->task->percent);
            gtk_progress_bar_set_text(this->progress_bar_, percent_str.data());
        }
        else
        {
            gtk_progress_bar_set_fraction(this->progress_bar_, 0);
        }
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(this->progress_bar_), true);
    }
    else if (this->complete_)
    {
        if (!this->task->custom_percent)
        {
            if (this->aborted_)
            {
                gtk_progress_bar_set_fraction(this->progress_bar_, 0);
            }
            else
            {
                gtk_progress_bar_set_fraction(this->progress_bar_, 1);
            }
            gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(this->progress_bar_), true);
        }
    }
    else if (this->task->type_ == vfs::file_task::type::exec &&
             this->task->state_pause_ == vfs::file_task::state::running)
    {
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(this->progress_bar_), false);
        gtk_progress_bar_pulse(this->progress_bar_);
    }

    // progress
    if (this->task->type_ != vfs::file_task::type::exec)
    {
        std::string stats;

        if (this->complete_)
        {
            if (this->pop_detail_)
            {
                stats = std::format("#{}  ({}) [{}] @avg {}",
                                    this->display_file_count_,
                                    this->display_size_tally_,
                                    this->display_elapsed_,
                                    this->display_average_speed_);
            }
            else
            {
                stats =
                    std::format("{} ({})", this->display_size_tally_, this->display_average_speed_);
            }
        }
        else
        {
            if (this->pop_detail_)
            {
                stats = std::format("#{} ({}) [{}] @cur {} ({}) @avg {} ({})",
                                    this->display_file_count_,
                                    this->display_size_tally_,
                                    this->display_elapsed_,
                                    this->display_current_speed_,
                                    this->display_current_estimate_,
                                    this->display_average_speed_,
                                    this->display_average_estimate_);
            }
            else
            {
                stats = std::format("{}  ({})  {} remaining",
                                    this->display_size_tally_,
                                    this->display_average_speed_,
                                    this->display_average_estimate_);
            }
        }
        gtk_label_set_text(this->current_, stats.data());
    }

    // error/output log
    if (this->log_appended_)
    {
        // scroll to end if scrollbar is mostly down

        // logger::info<logger::domain::ptk>("    scroll to end line {}", this->log_end,
        // gtk_text_buffer_get_line_count(this->log_buf));
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(this->error_view_),
                                     this->log_end_,
                                     0.0,
                                     false,
                                     0,
                                     0);
        this->log_appended_ = false;
    }

    // icon
    if (this->pause_change_ || this->err_count_ != this->task->err_count)
    {
        this->pause_change_ = false;
        this->err_count_ = this->task->err_count;
        this->set_progress_icon();
    }

    // status
    std::string errs;
    if (this->complete_)
    {
        if (this->aborted_)
        {
            if (this->task->err_count && this->task->type_ != vfs::file_task::type::exec)
            {
                if (this->err_mode_ == ptk::file_task::ptask_error::first)
                {
                    errs = "Error  ( Stop If First )";
                }
                else if (this->err_mode_ == ptk::file_task::ptask_error::any)
                {
                    errs = "Error  ( Stop On Any )";
                }
                else
                {
                    errs = std::format("Stopped with {} error", this->task->err_count);
                }
            }
            else
            {
                errs = "Stopped";
            }
        }
        else
        {
            if (this->task->type_ != vfs::file_task::type::exec)
            {
                if (this->task->err_count)
                {
                    errs = std::format("Finished with {} error", this->task->err_count);
                }
                else
                {
                    errs = "Done";
                }
            }
            else
            {
                errs = "Done";
            }
        }
    }
    else if (this->task->state_pause_ == vfs::file_task::state::pause)
    {
        errs = "Paused";
    }
    else if (this->task->state_pause_ == vfs::file_task::state::queue)
    {
        errs = "Queued";
    }
    else
    {
        if (this->task->err_count)
        {
            errs = std::format("Running with {} error", this->task->err_count);
        }
        else
        {
            errs = "Running...";
        }
    }
    gtk_label_set_text(this->errors_, errs.data());
    // logger::info<logger::domain::ptk>("ptk::file_task::progress_update({}) DONE", logger::utils::ptr(this));
}

void
ptk::file_task::set_chmod(const std::array<u8, 12> chmod_actions) noexcept
{
    this->task->set_chmod(chmod_actions);
}

void
ptk::file_task::set_chown(const uid_t uid, const gid_t gid) noexcept
{
    this->task->set_chown(uid, gid);
}

void
ptk::file_task::set_recursive(const bool recursive) noexcept
{
    this->task->set_recursive(recursive);
}

void
ptk::file_task::update() noexcept
{
    // logger::info<logger::domain::ptk>("ptk::file_task::update({})", logger::utils::ptr(this));
    // calculate updated display data

    if (!this->trylock())
    {
        // logger::info<logger::domain::ptk>("UPDATE LOCKED");
        return;
    }

    u64 cur_speed = 0;
    const auto elapsed = task->timer.elapsed();

    if (this->task->type_ != vfs::file_task::type::exec)
    {
        // cur speed
        if (this->task->state_pause_ == vfs::file_task::state::running)
        {
            const auto since_last = elapsed - this->task->last_elapsed;
            if (since_last >= std::chrono::seconds(2))
            {
                cur_speed = (this->task->progress - this->task->last_progress) / since_last.count();
                // logger::info<logger::domain::ptk>("( {} - {} ) / {} = {}", task->progress, task->last_progress, since_last, cur_speed);
                this->task->last_elapsed = elapsed;
                this->task->last_speed = cur_speed;
                this->task->last_progress = this->task->progress;
            }
            else if (since_last > std::chrono::milliseconds(100))
            {
                cur_speed = (this->task->progress - this->task->last_progress) / since_last.count();
            }
            else
            {
                cur_speed = 0;
            }
        }
        // calc percent
        i32 ipercent = 0;
        if (this->task->total_size)
        {
            ipercent = (this->task->progress * 100) / this->task->total_size;
        }
        else
        {
            ipercent = 50; // total_size calculation timed out
        }
        if (ipercent != this->task->percent)
        {
            this->task->percent = ipercent;
        }
    }

    // elapsed
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(elapsed);
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(elapsed - hours);
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(elapsed - hours - minutes);

    std::string dsp_elapsed;
    if (hours >= std::chrono::hours(1))
    {
        dsp_elapsed = std::format("{}:{}:{}", hours.count(), minutes.count(), seconds.count());
    }
    else if (minutes >= std::chrono::minutes(1))
    {
        dsp_elapsed = std::format("{}:{}", minutes.count(), seconds.count());
    }
    else
    {
        dsp_elapsed = std::format("{}", seconds.count());
    }

    this->display_elapsed_ = dsp_elapsed;

    if (this->task->type_ != vfs::file_task::type::exec)
    {
        std::string speed_current;
        std::string speed_average;
        std::string remaining_current;
        std::string remaining_average;

        std::string size_current;
        std::string size_average;

        // count
        const std::string file_count = std::format("{}", this->task->current_item);
        // size
        size_current = vfs::utils::format_file_size(this->task->progress);
        if (this->task->total_size)
        {
            size_average = vfs::utils::format_file_size(this->task->total_size);
        }
        else
        {
            size_average = "??"; // total_size calculation timed out
        }
        const std::string size_tally = std::format("{} / {}", size_current, size_average);
        // cur speed display
        if (this->task->last_speed != 0)
        {
            // use speed of last 2 sec interval if available
            cur_speed = this->task->last_speed;
        }
        if (cur_speed == 0 || this->task->state_pause_ != vfs::file_task::state::running)
        {
            if (this->task->state_pause_ == vfs::file_task::state::pause)
            {
                speed_current = "paused";
            }
            else if (this->task->state_pause_ == vfs::file_task::state::queue)
            {
                speed_current = "queued";
            }
            else
            {
                speed_current = "stalled";
            }
        }
        else
        {
            size_current = vfs::utils::format_file_size(cur_speed);
            speed_current = std::format("{}/s", size_current);
        }
        // avg speed
        u64 avg_speed = 0;
        if (elapsed > std::chrono::seconds::zero())
        {
            avg_speed = this->task->progress / elapsed.count();
        }
        else
        {
            avg_speed = 0;
        }
        size_average = vfs::utils::format_file_size(avg_speed);
        speed_average = std::format("{}/s", size_average);

        // remain cur
        std::chrono::seconds remaining_seconds;
        if (cur_speed > 0 && this->task->total_size != 0)
        {
            remaining_seconds =
                std::chrono::seconds((this->task->total_size - this->task->progress) / cur_speed);
        }
        else
        {
            remaining_seconds = std::chrono::seconds::zero();
        }
        if (remaining_seconds <= std::chrono::seconds::zero())
        {
            remaining_current = ""; // n/a
        }
        else if (remaining_seconds > std::chrono::hours(1))
        {
            const auto remaining_hours =
                std::chrono::duration_cast<std::chrono::hours>(remaining_seconds);

            remaining_current = std::format("{}/h", remaining_hours);
        }
        else if (remaining_seconds > std::chrono::minutes(1))
        {
            const auto remaining_hours =
                std::chrono::duration_cast<std::chrono::hours>(remaining_seconds);
            const auto remaining_minutes = std::chrono::duration_cast<std::chrono::minutes>(
                remaining_seconds - remaining_hours);

            remaining_current = std::format("{}:{}", remaining_seconds, remaining_minutes);
        }
        else
        {
            remaining_current = std::format("0:{}", remaining_seconds);
        }

        // remain avg
        if (avg_speed > 0 && this->task->total_size != 0)
        {
            remaining_seconds =
                std::chrono::seconds((this->task->total_size - this->task->progress) / avg_speed);
        }
        else
        {
            remaining_seconds = std::chrono::seconds::zero();
        }
        if (remaining_seconds <= std::chrono::seconds::zero())
        {
            remaining_average = ""; // n/a
        }
        else if (remaining_seconds > std::chrono::hours(1))
        {
            const auto remaining_hours =
                std::chrono::duration_cast<std::chrono::hours>(remaining_seconds);

            remaining_average = std::format("{}/h", remaining_hours);
        }
        else if (remaining_seconds > std::chrono::minutes(1))
        {
            const auto remaining_hours =
                std::chrono::duration_cast<std::chrono::hours>(remaining_seconds);
            const auto remaining_minutes = std::chrono::duration_cast<std::chrono::minutes>(
                remaining_seconds - remaining_hours);

            remaining_average = std::format("{}:{}", remaining_seconds, remaining_minutes);
        }
        else
        {
            remaining_average = std::format("0:{}", remaining_seconds);
        }

        this->display_file_count_ = file_count;
        this->display_size_tally_ = size_tally;
        this->display_current_speed_ = speed_current;
        this->display_average_speed_ = speed_average;
        this->display_current_estimate_ = remaining_current;
        this->display_average_estimate_ = remaining_average;
    }

    // move log lines from add_log_buf to log_buf
    if (gtk_text_buffer_get_char_count(this->task->add_log_buf))
    {
        GtkTextIter iter;
        GtkTextIter siter;
        // get add_log text and delete
        gtk_text_buffer_get_start_iter(this->task->add_log_buf, &siter);
        gtk_text_buffer_get_iter_at_mark(this->task->add_log_buf, &iter, this->task->add_log_end);
        g_autofree char* text =
            gtk_text_buffer_get_text(this->task->add_log_buf, &siter, &iter, false);
        gtk_text_buffer_delete(this->task->add_log_buf, &siter, &iter);
        // insert into log
        gtk_text_buffer_get_iter_at_mark(this->log_buf_, &iter, this->log_end_);
        gtk_text_buffer_insert(this->log_buf_, &iter, text, -1);
        this->log_appended_ = true;

        // trim log ?  (less than 64K and 800 lines)
        if (gtk_text_buffer_get_char_count(this->log_buf_) > 64000 ||
            gtk_text_buffer_get_line_count(this->log_buf_) > 800)
        {
            if (gtk_text_buffer_get_char_count(this->log_buf_) > 64000)
            {
                // trim to 50000 characters - handles single line flood
                gtk_text_buffer_get_iter_at_offset(this->log_buf_,
                                                   &iter,
                                                   gtk_text_buffer_get_char_count(this->log_buf_) -
                                                       50000);
            }
            else
            {
                // trim to 700 lines
                gtk_text_buffer_get_iter_at_line(this->log_buf_,
                                                 &iter,
                                                 gtk_text_buffer_get_line_count(this->log_buf_) -
                                                     700);
            }
            gtk_text_buffer_get_start_iter(this->log_buf_, &siter);
            gtk_text_buffer_delete(this->log_buf_, &siter, &iter);
            gtk_text_buffer_get_start_iter(this->log_buf_, &siter);
            if (this->task->type_ == vfs::file_task::type::exec)
            {
                gtk_text_buffer_insert(
                    this->log_buf_,
                    &siter,
                    "[ SNIP - additional output above has been trimmed from this log ]\n",
                    -1);
            }
            else
            {
                gtk_text_buffer_insert(
                    this->log_buf_,
                    &siter,
                    "[ SNIP - additional errors above have been trimmed from this log ]\n",
                    -1);
            }
        }

        if (this->task->type_ == vfs::file_task::type::exec && this->task->exec_show_output)
        {
            if (!this->keep_dlg_)
            {
                this->keep_dlg_ = true;
            }
            if (!this->progress_dlg_)
            {
                // disable this line to open every time output occurs
                this->task->exec_show_output = false;
                this->progress_open();
            }
        }
    }

    if (!this->progress_dlg_)
    {
        if (this->task->type_ != vfs::file_task::type::exec &&
            this->err_count_ != this->task->err_count)
        {
            this->keep_dlg_ = true;
            this->progress_open();
        }
        else if (this->task->type_ == vfs::file_task::type::exec &&
                 this->err_count_ != this->task->err_count)
        {
            if (!this->aborted_ && this->task->exec_show_error)
            {
                this->keep_dlg_ = true;
                this->progress_open();
            }
        }
    }
    else
    {
        if (this->task->type_ != vfs::file_task::type::exec &&
            this->err_count_ != this->task->err_count)
        {
            this->keep_dlg_ = true;
            if (this->complete_ || this->err_mode_ == ptk::file_task::ptask_error::any ||
                (this->task->error_first && this->err_mode_ == ptk::file_task::ptask_error::first))
            {
                gtk_window_present(GTK_WINDOW(this->progress_dlg_));
            }
        }
        else if (this->task->type_ == vfs::file_task::type::exec &&
                 this->err_count_ != this->task->err_count)
        {
            if (!this->aborted_ && this->task->exec_show_error)
            {
                this->keep_dlg_ = true;
                gtk_window_present(GTK_WINDOW(this->progress_dlg_));
            }
        }
    }

    this->progress_update();

    if (!this->timeout_ && !this->complete_)
    {
        ptk::view::file_task::update_task(this);
    }

    this->task->unlock();
    // logger::info<logger::domain::ptk>("ptk::file_task::update({}) DONE", logger::utils::ptr(this));
}

static bool
on_vfs_file_task_state_cb(const std::shared_ptr<vfs::file_task>& task,
                          const vfs::file_task::state state, void* state_data,
                          void* user_data) noexcept
{
    ptk::file_task* ptask = PTK_FILE_TASK(user_data);
    bool ret = true;

    switch (state)
    {
        case vfs::file_task::state::finish:
            // logger::info<logger::domain::ptk>("vfs::file_task::state::finish");

            ptask->complete_ = true;

            task->lock();
            if (task->type_ != vfs::file_task::type::exec)
            {
                task->current_file = std::nullopt;
            }
            ptask->progress_count_ = 50; // trigger fast display
            task->unlock();
            // gtk_signal_emit_by_name(G_OBJECT(ptask->signal_widget), "task-notify", ptask);
            break;
        case vfs::file_task::state::query_overwrite:
            // 0; GThread *self = g_thread_self ();
            // logger::info<logger::domain::ptk>("TASK_THREAD = {}", logger::utils::ptr(self));
            task->lock();
            ptask->query_new_dest_ = (char**)state_data;
            *ptask->query_new_dest_ = nullptr;

            ptask->query_cond_ = g_new(GCond, 1);
            g_cond_init(ptask->query_cond_);
            task->timer.stop();
            g_cond_wait(ptask->query_cond_, task->mutex);
            g_cond_clear(ptask->query_cond_);
            ptask->query_cond_ = nullptr;

            ret = ptask->query_ret_;
            task->last_elapsed = task->timer.elapsed();
            task->last_progress = task->progress;
            task->last_speed = 0;
            task->timer.start();
            task->unlock();
            break;
        case vfs::file_task::state::error:
            // logger::info<logger::domain::ptk>("vfs::file_task::state::error");
            task->lock();
            task->err_count++;
            // logger::info<logger::domain::ptk>("    ptask->item_count = {}", task->current_item );

            if (task->type_ == vfs::file_task::type::exec)
            {
                ret = false;
            }
            else if (ptask->err_mode_ == ptk::file_task::ptask_error::any ||
                     (task->error_first && ptask->err_mode_ == ptk::file_task::ptask_error::first))
            {
                ret = false;
                ptask->aborted_ = true;
            }
            ptask->progress_count_ = 50; // trigger fast display

            task->unlock();

            if (xset_get_b(xset::name::task_q_pause))
            {
                // pause all queued
                ptk::view::file_task::pause_all_queued(ptask);
            }
            break;
        case vfs::file_task::state::running:
        case vfs::file_task::state::size_timeout:
        case vfs::file_task::state::pause:
        case vfs::file_task::state::queue:
            break;
    }

    return ret; /* return true to continue */
}

static bool
on_query_input_keypress(GtkWidget* widget, GdkEvent* event, ptk::file_task* ptask) noexcept
{
    (void)ptask;
    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto keyval = gdk_key_event_get_keyval(event);
    if (keymod == 0)
    {
        switch (keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
            {
                // User pressed enter in rename/overwrite dialog
                const auto new_name = ptk::utils::multi_input_get_text(widget);
                const char* old_name =
                    static_cast<const char*>(g_object_get_data(G_OBJECT(widget), "old_name"));

#if (GTK_MAJOR_VERSION == 4)
                GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(widget)));
#elif (GTK_MAJOR_VERSION == 3)
                GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(widget));
#endif

                if (new_name && old_name && new_name.value() != old_name)
                {
                    gtk_dialog_response(GTK_DIALOG(parent),
                                        magic_enum::enum_integer(ptk::file_task::response::rename));
                }
                else
                {
                    gtk_dialog_response(
                        GTK_DIALOG(parent),
                        magic_enum::enum_integer(ptk::file_task::response::auto_rename));
                }
                return true;
            }
            default:
                break;
        }
    }
    return false;
}

static void
on_multi_input_changed(GtkWidget* input_buf, GtkWidget* query_input) noexcept
{
    (void)input_buf;
    const auto new_name = ptk::utils::multi_input_get_text(query_input);
    const char* old_name =
        static_cast<const char*>(g_object_get_data(G_OBJECT(query_input), "old_name"));
    const bool can_rename = new_name && old_name && (new_name.value() != old_name);

#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(query_input)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(query_input));
#endif

    GtkWidget* rename_button = GTK_WIDGET(g_object_get_data(G_OBJECT(parent), "rename_button"));
    if (GTK_IS_WIDGET(rename_button))
    {
        gtk_widget_set_sensitive(rename_button, can_rename);
    }
    gtk_dialog_set_response_sensitive(GTK_DIALOG(parent),
                                      magic_enum::enum_integer(ptk::file_task::response::overwrite),
                                      !can_rename);
    gtk_dialog_set_response_sensitive(
        GTK_DIALOG(parent),
        magic_enum::enum_integer(ptk::file_task::response::overwrite_all),
        !can_rename);
}

static void
query_overwrite_response(GtkDialog* dlg, const i32 response, ptk::file_task* ptask) noexcept
{
    switch (ptk::file_task::response(response))
    {
        case ptk::file_task::response::overwrite_all:
        {
            ptask->task->set_overwrite_mode(vfs::file_task::overwrite_mode::overwrite_all);
            if (ptask->progress_dlg_)
            {
                gtk_combo_box_set_active(
                    GTK_COMBO_BOX(ptask->overwrite_combo_),
                    magic_enum::enum_integer(vfs::file_task::overwrite_mode::overwrite_all));
            }
            break;
        }
        case ptk::file_task::response::overwrite:
        {
            ptask->task->set_overwrite_mode(vfs::file_task::overwrite_mode::overwrite);
            break;
        }
        case ptk::file_task::response::skip_all:
        {
            ptask->task->set_overwrite_mode(vfs::file_task::overwrite_mode::skip_all);
            if (ptask->progress_dlg_)
            {
                gtk_combo_box_set_active(
                    GTK_COMBO_BOX(ptask->overwrite_combo_),
                    magic_enum::enum_integer(vfs::file_task::overwrite_mode::skip_all));
            }
            break;
        }
        case ptk::file_task::response::skip:
        {
            ptask->task->set_overwrite_mode(vfs::file_task::overwrite_mode::skip);
            break;
        }
        case ptk::file_task::response::auto_rename_all:
        {
            ptask->task->set_overwrite_mode(vfs::file_task::overwrite_mode::auto_rename);
            if (ptask->progress_dlg_)
            {
                gtk_combo_box_set_active(
                    GTK_COMBO_BOX(ptask->overwrite_combo_),
                    magic_enum::enum_integer(vfs::file_task::overwrite_mode::auto_rename));
            }
            break;
        }
        case ptk::file_task::response::auto_rename:
        case ptk::file_task::response::rename:
        {
            std::optional<std::string> str;
            ptask->task->set_overwrite_mode(vfs::file_task::overwrite_mode::rename);
            if (ptk::file_task::response(response) == ptk::file_task::response::auto_rename)
            {
                GtkWidget* auto_button =
                    GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "auto_button"));
                str = gtk_widget_get_tooltip_text(auto_button);
            }
            else
            {
                GtkWidget* query_input =
                    GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "query_input"));
                str = ptk::utils::multi_input_get_text(query_input);
            }
            const auto filename = std::filesystem::path(str.value());
            if (str && !filename.empty() && ptask->task->current_dest)
            {
                const auto current_dest = ptask->task->current_dest.value();

                const auto dir_name = current_dest.parent_path();
                const auto path = dir_name / filename;
                *ptask->query_new_dest_ = ::utils::strdup(path.c_str());
            }
            break;
        }
        case ptk::file_task::response::pause:
        {
            ptask->pause(vfs::file_task::state::pause);
            ptk::view::file_task::start_queued(ptask->task_view_, ptask);
            ptask->task->set_overwrite_mode(vfs::file_task::overwrite_mode::rename);
            ptask->restart_timeout_ = false;
            break;
        }
        case ptk::file_task::response::close:
        {
            if (response == GtkResponseType::GTK_RESPONSE_CANCEL ||
                response == GtkResponseType::GTK_RESPONSE_DELETE_EVENT)
            { // escape was pressed or window closed
                ptask->task->abort = true;
            }
            break;
        }
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
                 std::format("{}", allocation.width));
        xset_set(xset::name::task_popups,
                 has_overwrite_btn ? xset::var::y : xset::var::z,
                 std::format("{}", allocation.height));
    }

    gtk_widget_destroy(GTK_WIDGET(dlg));

    if (ptask->query_cond_)
    {
        ptask->lock();
        ptask->query_ret_ = (response != GtkResponseType::GTK_RESPONSE_CANCEL) &&
                            (response != GtkResponseType::GTK_RESPONSE_DELETE_EVENT);
        // g_cond_broadcast( ptask->query_cond );
        g_cond_signal(ptask->query_cond_);
        ptask->unlock();
    }
    if (ptask->restart_timeout_)
    {
        ptask->timeout_ = g_timeout_add(500, (GSourceFunc)ptk_file_task_add_main, (void*)ptask);
    }
    ptask->progress_count_ = 50;
    ptask->progress_timer_ = g_timeout_add(50, (GSourceFunc)on_progress_timer, ptask);
}

static void
on_query_button_press(GtkWidget* widget, ptk::file_task* ptask) noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(widget)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(widget));
#endif

    GtkWidget* rename_button = GTK_WIDGET(g_object_get_data(G_OBJECT(parent), "rename_button"));
    GtkWidget* auto_button = GTK_WIDGET(g_object_get_data(G_OBJECT(parent), "auto_button"));
    if (!rename_button || !auto_button)
    {
        return;
    }

    ptk::file_task::response response = ptk::file_task::response::close;
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
    query_overwrite_response(GTK_DIALOG(parent), magic_enum::enum_integer(response), ptask);
}

void
ptk::file_task::query_overwrite() noexcept
{
    // logger::info<logger::domain::ptk>("ptk::file_task::query_overwrite({}) ", logger::utils::ptr(this));
    GtkWidget* dlg = nullptr;
    GtkWidget* parent_win = nullptr;
    GtkTextIter iter;

    bool has_overwrite_btn = true;

    std::string title;
    std::string message;
    std::string from_size_str;
    std::string to_size_str;
    std::string from_disp;

    if (this->task->type_ == vfs::file_task::type::move)
    {
        from_disp = "Moving from directory:";
    }
    else if (this->task->type_ == vfs::file_task::type::link)
    {
        from_disp = "Linking from directory:";
    }
    else
    {
        from_disp = "Copying from directory:";
    }

    if (!this->task->current_file || !this->task->current_dest)
    {
        return;
    }

    const auto current_file = this->task->current_file.value();
    const auto current_dest = this->task->current_dest.value();

    const bool different_files = (!std::filesystem::equivalent(current_file, current_dest));

    const auto src_stat = ztd::lstat(current_file);
    const auto dest_stat = ztd::lstat(current_dest);

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
                const std::string size_str = vfs::utils::format_file_size(src_stat.size());
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

            if (src_stat.mtime() == dest_stat.mtime())
            {
                src_time = "<b>( same time )</b>\t";
            }
            else
            {
                src_time =
                    std::format("{}", std::chrono::floor<std::chrono::seconds>(src_stat.mtime()));

                if (src_stat.mtime() > dest_stat.mtime())
                {
                    src_rel_time = "newer";
                }
                else
                {
                    src_rel_time = "older";
                }
            }

            const std::string size_str = vfs::utils::format_file_size(dest_stat.size());
            const std::string dest_size =
                std::format("{}\t( {:L} bytes )", size_str, dest_stat.size());

            const auto dest_time =
                std::format("{}", std::chrono::floor<std::chrono::seconds>(dest_stat.mtime()));

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

    const auto filename_parts = vfs::utils::split_basename_extension(filename);

    const std::string unique_name = vfs::utils::unique_path(dest_dir, filename, "-copy");
    const std::string new_name_plain =
        !unique_name.empty() ? std::filesystem::path(unique_name).filename() : "";
    const std::string new_name = !new_name_plain.empty() ? new_name_plain : "";

    const auto pos = !filename_parts.extension.empty()
                         ? filename.size() - filename_parts.extension.size() - 1
                         : -1;

    // create dialog
    if (this->progress_dlg_)
    {
        parent_win = GTK_WIDGET(this->progress_dlg_);
    }
    else
    {
        parent_win = GTK_WIDGET(this->parent_window_);
    }
    dlg =
        gtk_dialog_new_with_buttons(title.data(),
                                    GTK_WINDOW(parent_win),
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    nullptr,
                                    nullptr);

    g_signal_connect(G_OBJECT(dlg), "response", G_CALLBACK(query_overwrite_response), this);
    gtk_window_set_resizable(GTK_WINDOW(dlg), true);
    gtk_window_set_title(GTK_WINDOW(dlg), title.data());
#if (GTK_MAJOR_VERSION == 3)
    gtk_window_set_type_hint(GTK_WINDOW(dlg), GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_gravity(GTK_WINDOW(dlg), GdkGravity::GDK_GRAVITY_NORTH_EAST);
    gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
#endif

    gtk_widget_set_halign(GTK_WIDGET(dlg), GtkAlign::GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(dlg), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_hexpand(GTK_WIDGET(dlg), true);
    gtk_widget_set_vexpand(GTK_WIDGET(dlg), true);
    gtk_widget_set_margin_start(GTK_WIDGET(dlg), 0);
    gtk_widget_set_margin_end(GTK_WIDGET(dlg), 0);
    gtk_widget_set_margin_top(GTK_WIDGET(dlg), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(dlg), 0);

    GtkBox* vbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_halign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_hexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_vexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_margin_start(GTK_WIDGET(vbox), 7);
    gtk_widget_set_margin_end(GTK_WIDGET(vbox), 7);
    gtk_widget_set_margin_top(GTK_WIDGET(vbox), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(vbox), 14);

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

    GtkBox* content_area = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg)));
    gtk_box_pack_start(GTK_BOX(content_area), GTK_WIDGET(vbox), true, true, 0);

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
        gtk_dialog_add_button(GTK_DIALOG(dlg),
                              "_Pause",
                              magic_enum::enum_integer(ptk::file_task::response::pause));
    gtk_dialog_add_buttons(GTK_DIALOG(dlg),
                           "_Skip",
                           ptk::file_task::response::skip,
                           "S_kip All",
                           ptk::file_task::response::skip_all,
                           "Cancel",
                           GtkResponseType::GTK_RESPONSE_CANCEL,
                           nullptr);

    // xset_t set = xset::set::get(xset::name::TASK_PAUSE);
    gtk_widget_set_sensitive(btn_pause, !!this->task_view_);

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
    GtkWidget* query_input = GTK_WIDGET(ptk::utils::multi_input_new(scroll, filename));
    g_object_set_data_full(G_OBJECT(query_input),
                           "old_name",
                           ::utils::strdup(filename.data()),
                           std::free);
    // clang-format off
    g_signal_connect(G_OBJECT(query_input), "key-press-event", G_CALLBACK(on_query_input_keypress), this);
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
    g_signal_connect(G_OBJECT(rename_button), "clicked", G_CALLBACK(on_query_button_press), this);
    GtkButton* auto_button = GTK_BUTTON(gtk_button_new_with_mnemonic(" A_uto Rename "));
    g_signal_connect(G_OBJECT(auto_button), "clicked", G_CALLBACK(on_query_button_press), this);
    gtk_widget_set_tooltip_text(GTK_WIDGET(auto_button), new_name.data());
    GtkButton* auto_all_button = GTK_BUTTON(gtk_button_new_with_mnemonic(" Auto Re_name All "));
    // clang-format off
    g_signal_connect(G_OBJECT(auto_all_button), "clicked", G_CALLBACK(on_query_button_press), this);
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
    this->display_current_speed_ = "stalled";
    this->progress_update();
    if (this->task_view_ &&
        gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(this->task_view_))))
    {
        ptk::view::file_task::update_task(this);
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
}
