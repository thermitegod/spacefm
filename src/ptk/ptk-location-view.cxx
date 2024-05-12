/**
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
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

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <vector>

#include <chrono>

#include <memory>

#include <ranges>

#include <cmath>

#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "utils/shell-quote.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"
#include "xset/xset-dialog.hxx"

#include "types.hxx"

#include "ptk/ptk-dialog.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-location-view.hxx"
#include "ptk/utils/ptk-utils.hxx"

#include "main-window.hxx"

#include "vfs/vfs-volume.hxx"
#include "vfs/utils/vfs-utils.hxx"

#include "vfs/linux/self.hxx"

#include "settings/settings.hxx"

// This limits the small icon size for side panes and task list
static constexpr i32 PANE_MAX_ICON_SIZE = 48;

static GtkTreeModel* model = nullptr;

static void ptk_location_view_init_model(GtkListStore* list) noexcept;

static void on_volume_event(const std::shared_ptr<vfs::volume>& vol, const vfs::volume::state state,
                            void* user_data) noexcept;

static void add_volume(const std::shared_ptr<vfs::volume>& vol, bool set_icon) noexcept;
static void remove_volume(const std::shared_ptr<vfs::volume>& vol) noexcept;
static void update_volume(const std::shared_ptr<vfs::volume>& vol) noexcept;

static bool on_button_press_event(GtkTreeView* view, GdkEvent* event, void* user_data) noexcept;
static bool on_key_press_event(GtkWidget* w, GdkEvent* event, ptk::browser* browser) noexcept;

static bool try_mount(GtkTreeView* view, const std::shared_ptr<vfs::volume>& vol) noexcept;

static void on_open_tab(GtkMenuItem* item, const std::shared_ptr<vfs::volume>& vol,
                        GtkWidget* view2) noexcept;
static void on_open(GtkMenuItem* item, const std::shared_ptr<vfs::volume>& vol,
                    GtkWidget* view2) noexcept;

namespace ptk::location_view
{
enum class column
{
    icon,
    name,
    path,
    data,
};
}

struct AutoOpen
{
    AutoOpen() = delete;
    AutoOpen(ptk::browser* browser) noexcept;
    ~AutoOpen() noexcept;
    AutoOpen(const AutoOpen& other) = delete;
    AutoOpen(AutoOpen&& other) = delete;
    AutoOpen& operator=(const AutoOpen& other) = delete;
    AutoOpen& operator=(AutoOpen&& other) = delete;

    ptk::browser* browser{nullptr};
    dev_t devnum{0};
    char* device_file{nullptr};
    char* mount_point{nullptr};
    bool keep_point{false};
    ptk::browser::open_action job{ptk::browser::open_action::dir};
};

AutoOpen::AutoOpen(ptk::browser* browser) noexcept : browser(browser) {}

AutoOpen::~AutoOpen() noexcept
{
    if (this->device_file)
    {
        std::free(this->device_file);
    }
    if (this->mount_point)
    {
        std::free(this->mount_point);
    }
}

static bool volume_is_visible(const std::shared_ptr<vfs::volume>& vol) noexcept;
static void update_all() noexcept;

/*  Drag & Drop/Clipboard targets  */
// static GtkTargetEntry drag_targets[] = {{utils::strdup("text/uri-list"), 0, 0}};

static void
on_model_destroy(void* data, GObject* object) noexcept
{
    (void)data;
    vfs::volume_remove_callback(on_volume_event, (void*)object);

    model = nullptr;
}

void
ptk::view::location::update_volume_icons() noexcept
{
    if (!model)
    {
        return;
    }

    GtkTreeIter it;

    // GtkListStore* list = GTK_LIST_STORE( model );
    i32 icon_size = config::settings.icon_size_small;
    if (icon_size > PANE_MAX_ICON_SIZE)
    {
        icon_size = PANE_MAX_ICON_SIZE;
    }

    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            std::shared_ptr<vfs::volume> vol = nullptr;
            gtk_tree_model_get(model, &it, ptk::location_view::column::data, &vol, -1);
            if (vol)
            {
                GdkPixbuf* icon = vfs::utils::load_icon(vol->icon(), icon_size);
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   ptk::location_view::column::icon,
                                   icon,
                                   -1);
                if (icon)
                {
                    g_object_unref(icon);
                }
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
}

static void
update_change_detection() noexcept
{
    // update all windows/all panels/all browsers
    for (MainWindow* window : main_window_get_all())
    {
        for (const panel_t p : PANELS)
        {
            GtkNotebook* notebook = window->get_panel_notebook(p);
            const i32 num_pages = gtk_notebook_get_n_pages(notebook);
            for (const auto i : std::views::iota(0z, num_pages))
            {
                ptk::browser* browser =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                if (browser)
                {
                    // update current dir change detection
                    browser->dir_->update_avoid_changes();
                    // update thumbnail visibility
                    browser->show_thumbnails(
                        config::settings.show_thumbnails ? config::settings.thumbnail_max_size : 0);
                }
            }
        }
    }
}

static void
update_all() noexcept
{
    if (!model)
    {
        return;
    }

    std::shared_ptr<vfs::volume> v = nullptr;

    for (const auto& volume : vfs::volume_get_all_volumes())
    {
        if (volume)
        {
            bool havevol = false;

            // search model for volume vol
            GtkTreeIter it;
            if (gtk_tree_model_get_iter_first(model, &it))
            {
                do
                {
                    gtk_tree_model_get(model, &it, ptk::location_view::column::data, &v, -1);
                } while (v != volume && gtk_tree_model_iter_next(model, &it));
                havevol = (v == volume);
            }

            if (volume_is_visible(volume))
            {
                if (havevol)
                {
                    update_volume(volume);
                }
                else
                {
                    add_volume(volume, true);
                }
            }
            else if (havevol)
            {
                remove_volume(volume);
            }
        }
    }
}

static void
update_names() noexcept
{
    std::shared_ptr<vfs::volume> v = nullptr;
    for (const auto& volume : vfs::volume_get_all_volumes())
    {
        if (!volume)
        {
            continue;
        }

        volume->set_info();

        // search model for volume vol
        GtkTreeIter it;
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, ptk::location_view::column::data, &v, -1);
            } while (v != volume && gtk_tree_model_iter_next(model, &it));
            if (v == volume)
            {
                update_volume(volume);
            }
        }
    }
}

bool
ptk::view::location::chdir(GtkTreeView* location_view,
                           const std::filesystem::path& current_path) noexcept
{
    if (current_path.empty() || !GTK_IS_TREE_VIEW(location_view))
    {
        return false;
    }

    GtkTreeSelection* selection = gtk_tree_view_get_selection(location_view);
    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            std::shared_ptr<vfs::volume> vol;
            gtk_tree_model_get(model, &it, ptk::location_view::column::data, &vol, -1);
            const auto mount_point = vol->mount_point();
            if (std::filesystem::equivalent(current_path, mount_point))
            {
                gtk_tree_selection_select_iter(selection, &it);
                GtkTreePath* path = gtk_tree_model_get_path(model, &it);
                if (path)
                {
                    gtk_tree_view_scroll_to_cell(location_view, path, nullptr, true, .25, 0);
                    gtk_tree_path_free(path);
                }
                return true;
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
    gtk_tree_selection_unselect_all(selection);
    return false;
}

const std::shared_ptr<vfs::volume>
ptk::view::location::selected_volume(GtkTreeView* location_view) noexcept
{
    // ztd::logger::info("ptk::view::location::selected_volume    view = {}", location_view);
    GtkTreeIter it;

    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(location_view));
    if (gtk_tree_selection_get_selected(selection, nullptr, &it))
    {
        std::shared_ptr<vfs::volume> vol;
        gtk_tree_model_get(model, &it, ptk::location_view::column::data, &vol, -1);
        return vol;
    }
    return nullptr;
}

static void
on_row_activated(GtkTreeView* view, GtkTreePath* tree_path, GtkTreeViewColumn* col,
                 ptk::browser* browser) noexcept
{
    (void)col;
    // ztd::logger::info("on_row_activated   view = {}", view);
    if (!browser)
    {
        return;
    }

    GtkTreeIter it;
    if (!gtk_tree_model_get_iter(model, &it, tree_path))
    {
        return;
    }

    std::shared_ptr<vfs::volume> vol;
    gtk_tree_model_get(model, &it, ptk::location_view::column::data, &vol, -1);
    if (!vol)
    {
        return;
    }

    if (!vol->is_mounted() && vol->is_device_type(vfs::volume::device_type::block))
    {
        try_mount(view, vol);
        if (vol->is_mounted())
        {
            const auto mount_point = vol->mount_point();
            if (!mount_point.empty())
            {
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   ptk::location_view::column::path,
                                   mount_point.data(),
                                   -1);
            }
        }
    }
    if (vol->is_mounted() && !vol->mount_point().empty())
    {
        if (xset_get_b(xset::name::dev_newtab))
        {
            browser->run_event<spacefm::signal::open_item>(vol->mount_point(),
                                                           ptk::browser::open_action::new_tab);
            ptk::view::location::chdir(view, browser->cwd());
        }
        else
        {
            if (!std::filesystem::equivalent(vol->mount_point(), browser->cwd()))
            {
                browser->chdir(vol->mount_point());
            }
        }
    }
}

bool
ptk::view::location::open_block(const std::filesystem::path& block, const bool new_tab) noexcept
{
    // open block device file if in volumes list

    // may be link so get real path
    const auto canon = std::filesystem::canonical(block);

    for (const auto& volume : vfs::volume_get_all_volumes())
    {
        if (!volume)
        {
            continue;
        }

        if (volume->device_file() == canon.string())
        {
            if (new_tab)
            {
                on_open_tab(nullptr, volume, nullptr);
            }
            else
            {
                on_open(nullptr, volume, nullptr);
            }
            return true;
        }
    }
    return false;
}

static void
ptk_location_view_init_model(GtkListStore* list) noexcept
{
    (void)list;

    vfs::volume_add_callback(on_volume_event, model);

    for (const auto& volume : vfs::volume_get_all_volumes())
    {
        if (!volume)
        {
            continue;
        }

        add_volume(volume, false);
    }
    ptk::view::location::update_volume_icons();
}

GtkWidget*
ptk::view::location::create(ptk::browser* browser) noexcept
{
    if (!model)
    {
        GtkListStore* list =
            gtk_list_store_new(magic_enum::enum_count<ptk::location_view::column>(),
                               GDK_TYPE_PIXBUF,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_POINTER);
        g_object_weak_ref(G_OBJECT(list), on_model_destroy, nullptr);
        model = GTK_TREE_MODEL(list);
        ptk_location_view_init_model(list);
    }
    else
    {
        g_object_ref(G_OBJECT(model));
    }

    GtkWidget* view = gtk_tree_view_new_with_model(model);
    g_object_unref(G_OBJECT(model));
    // ztd::logger::info("ptk::view::location::create   view = {}", view);
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_set_mode(selection, GtkSelectionMode::GTK_SELECTION_SINGLE);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), false);

    GtkTreeViewColumn* col = gtk_tree_view_column_new();
    GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, false);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "pixbuf",
                                        ptk::location_view::column::icon,
                                        nullptr);

    renderer = gtk_cell_renderer_text_new();
    // g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(on_bookmark_edited), view);
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        ptk::location_view::column::name,
                                        nullptr);
    gtk_tree_view_column_set_min_width(col, 10);

    if (GTK_IS_TREE_SORTABLE(model))
    { // why is this needed to stop error on new tab?
        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(model),
            magic_enum::enum_integer(ptk::location_view::column::name),
            GtkSortType::GTK_SORT_ASCENDING);
    }

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    gtk_tree_view_column_set_sizing(col, GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    g_object_set_data(G_OBJECT(view), "browser", browser);

    // clang-format off
    g_signal_connect(G_OBJECT(view), "row-activated", G_CALLBACK(on_row_activated), browser);
    g_signal_connect(G_OBJECT(view), "button-press-event", G_CALLBACK(on_button_press_event), nullptr);
    g_signal_connect(G_OBJECT(view), "key-press-event", G_CALLBACK(on_key_press_event), browser);
    // clang-format on

    return view;
}

static void
on_volume_event(const std::shared_ptr<vfs::volume>& vol, const vfs::volume::state state,
                void* user_data) noexcept
{
    (void)user_data;
    switch (state)
    {
        case vfs::volume::state::added:
            add_volume(vol, true);
            break;
        case vfs::volume::state::removed:
            remove_volume(vol);
            break;
        case vfs::volume::state::changed: // CHANGED may occur before ADDED !
            if (!volume_is_visible(vol))
            {
                remove_volume(vol);
            }
            else
            {
                update_volume(vol);
            }
            break;
        case vfs::volume::state::mounted:
        case vfs::volume::state::unmounted:
        case vfs::volume::state::eject:
            break;
    }
}

static void
add_volume(const std::shared_ptr<vfs::volume>& vol, bool set_icon) noexcept
{
    if (!volume_is_visible(vol))
    {
        return;
    }

    // vol already exists?
    std::shared_ptr<vfs::volume> v = nullptr;
    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, ptk::location_view::column::data, &v, -1);
        } while (v != vol && gtk_tree_model_iter_next(model, &it));
    }
    if (v == vol)
    {
        return;
    }

    // get mount point
    const auto mount_point = vol->mount_point();

    // add to model
    gtk_list_store_insert_with_values(GTK_LIST_STORE(model),
                                      &it,
                                      0,
                                      ptk::location_view::column::name,
                                      vol->display_name().data(),
                                      ptk::location_view::column::path,
                                      mount_point.data(),
                                      ptk::location_view::column::data,
                                      vol.get(),
                                      -1);
    if (set_icon)
    {
        i32 icon_size = config::settings.icon_size_small;
        if (icon_size > PANE_MAX_ICON_SIZE)
        {
            icon_size = PANE_MAX_ICON_SIZE;
        }

        GdkPixbuf* icon = vfs::utils::load_icon(vol->icon(), icon_size);
        gtk_list_store_set(GTK_LIST_STORE(model), &it, ptk::location_view::column::icon, icon, -1);
        if (icon)
        {
            g_object_unref(icon);
        }
    }
}

static void
remove_volume(const std::shared_ptr<vfs::volume>& vol) noexcept
{
    if (!vol)
    {
        return;
    }

    GtkTreeIter it;
    std::shared_ptr<vfs::volume> v = nullptr;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, ptk::location_view::column::data, &v, -1);
        } while (v != vol && gtk_tree_model_iter_next(model, &it));
    }
    if (v != vol)
    {
        return;
    }
    gtk_list_store_remove(GTK_LIST_STORE(model), &it);
}

static void
update_volume(const std::shared_ptr<vfs::volume>& vol) noexcept
{
    if (!vol)
    {
        return;
    }

    GtkTreeIter it;
    std::shared_ptr<vfs::volume> v = nullptr;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, ptk::location_view::column::data, &v, -1);
        } while (v != vol && gtk_tree_model_iter_next(model, &it));
    }
    if (v != vol)
    {
        add_volume(vol, true);
        return;
    }

    i32 icon_size = config::settings.icon_size_small;
    if (icon_size > PANE_MAX_ICON_SIZE)
    {
        icon_size = PANE_MAX_ICON_SIZE;
    }

    GdkPixbuf* icon = vfs::utils::load_icon(vol->icon(), icon_size);
    gtk_list_store_set(GTK_LIST_STORE(model),
                       &it,
                       ptk::location_view::column::icon,
                       icon,
                       ptk::location_view::column::name,
                       vol->display_name().data(),
                       ptk::location_view::column::path,
                       vol->mount_point().data(),
                       -1);
    if (icon)
    {
        g_object_unref(icon);
    }
}

void
ptk::view::location::mount_network(ptk::browser* browser, const std::string_view url,
                                   const bool new_tab, const bool force_new_mount) noexcept
{
    (void)browser;
    (void)url;
    (void)new_tab;
    (void)force_new_mount;

    // TODO - rewrite netmount parser and mount code, kept entry point shims.

    ptk::dialog::error(nullptr, "Netmounting is Disabled", "Recommended to mount through a shell");
}

static void
popup_missing_mount(GtkWidget* view, i32 job) noexcept
{
    std::string cmd;
    if (job == 0)
    {
        cmd = "mount";
    }
    else
    {
        cmd = "unmount";
    }

    ptk::dialog::message(
        GTK_WINDOW(view),
        GtkMessageType::GTK_MESSAGE_ERROR,
        "Handler Not Found",
        GtkButtonsType::GTK_BUTTONS_OK,
        std::format("No handler is configured for this device type, or no {} command is set. Add a "
                    "handler in Settings|Device Handlers or Protocol Handlers.",
                    cmd));
}

static void
on_mount(GtkMenuItem* item, const std::shared_ptr<vfs::volume>& vol, GtkWidget* view2) noexcept
{
    GtkWidget* view = nullptr;
    if (!item)
    {
        view = view2;
    }
    else
    {
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    }

    if (!view || !vol)
    {
        return;
    }
    if (vol->device_file().empty())
    {
        return;
    }
    auto* browser = static_cast<ptk::browser*>(g_object_get_data(G_OBJECT(view), "browser"));
    // Note: browser may be nullptr
    if (!GTK_IS_WIDGET(browser))
    {
        browser = nullptr;
    }

    // task
    const auto check_mount_command = vol->device_mount_cmd();
    if (!check_mount_command)
    {
        popup_missing_mount(GTK_WIDGET(view), 0);
        return;
    }
    const auto& mount_command = check_mount_command.value();

    const std::string task_name = std::format("Mount {}", vol->device_file());
    ptk::file_task* ptask =
        ptk_file_exec_new(task_name, view, browser ? browser->task_view() : nullptr);
    ptask->task->exec_command = mount_command;
    ptask->task->exec_sync = true;
    ptask->task->exec_browser = browser;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_terminal = false;
    ptask->task->exec_icon = vol->icon();
    ptask->run();
}

static void
on_umount(GtkMenuItem* item, const std::shared_ptr<vfs::volume>& vol, GtkWidget* view2) noexcept
{
    GtkWidget* view = nullptr;
    if (!item)
    {
        view = view2;
    }
    else
    {
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    }
    auto* browser = static_cast<ptk::browser*>(g_object_get_data(G_OBJECT(view), "browser"));
    // Note: browser may be nullptr
    if (!GTK_IS_WIDGET(browser))
    {
        browser = nullptr;
    }

    // task
    const auto check_unmount_command = vol->device_unmount_cmd();
    if (!check_unmount_command)
    {
        popup_missing_mount(view, 1);
        return;
    }
    const auto& unmount_command = check_unmount_command.value();

    const std::string task_name = std::format("Unmount {}", vol->device_file());
    ptk::file_task* ptask =
        ptk_file_exec_new(task_name, view, browser ? browser->task_view() : nullptr);
    ptask->task->exec_command = unmount_command;
    ptask->task->exec_sync = true;
    ptask->task->exec_browser = browser;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_terminal = false;
    ptask->task->exec_icon = vol->icon();
    ptask->run();
}

static void
on_eject(GtkMenuItem* item, const std::shared_ptr<vfs::volume>& vol, GtkWidget* view2) noexcept
{
    GtkWidget* view = nullptr;
    if (!item)
    {
        view = view2;
    }
    else
    {
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    }
    auto* browser = static_cast<ptk::browser*>(g_object_get_data(G_OBJECT(view), "browser"));
    // Note: browser may be nullptr
    if (!GTK_IS_WIDGET(browser))
    {
        browser = nullptr;
    }

    if (vol->is_mounted())
    {
        // task
        const auto check_unmount_command = vol->device_unmount_cmd();
        if (!check_unmount_command)
        {
            popup_missing_mount(view, 1);
            return;
        }
        const auto& unmount_command = check_unmount_command.value();

        const std::string task_name = std::format("Remove {}", vol->device_file());
        ptk::file_task* ptask =
            ptk_file_exec_new(task_name, view, browser ? browser->task_view() : nullptr);
        ptask->task->exec_command = unmount_command;
        ptask->task->exec_sync = true;
        ptask->task->exec_browser = browser;
        ptask->task->exec_show_error = true;
        ptask->task->exec_terminal = false;
        ptask->task->exec_icon = vol->icon();

        ptask->run();
    }
    else if (vol->is_device_type(vfs::volume::device_type::block) &&
             (vol->is_optical() || vol->requires_eject()))
    {
        // task
        const std::string line = std::format("eject {}", vol->device_file());
        const std::string task_name = std::format("Remove {}", vol->device_file());
        ptk::file_task* ptask =
            ptk_file_exec_new(task_name, view, browser ? browser->task_view() : nullptr);
        ptask->task->exec_command = line;
        ptask->task->exec_sync = false;
        ptask->task->exec_show_error = false;
        ptask->task->exec_icon = vol->icon();

        ptask->run();
    }
    else
    {
        // task
        const std::string line = "sync";
        const std::string task_name = std::format("Remove {}", vol->device_file());
        ptk::file_task* ptask =
            ptk_file_exec_new(task_name, view, browser ? browser->task_view() : nullptr);
        ptask->task->exec_command = line;
        ptask->task->exec_sync = false;
        ptask->task->exec_show_error = false;
        ptask->task->exec_icon = vol->icon();

        ptask->run();
    }
}

static bool
on_autoopen_cb(const std::shared_ptr<vfs::file_task>& task, AutoOpen* ao) noexcept
{
    (void)task;
    // ztd::logger::info("on_autoopen_cb");
    for (const auto& volume : vfs::volume_get_all_volumes())
    {
        if (!volume)
        {
            continue;
        }

        if (volume->devnum() == ao->devnum)
        {
            if (volume->is_mounted())
            {
                ptk::browser* browser = ao->browser;
                if (GTK_IS_WIDGET(browser))
                {
                    browser->run_event<spacefm::signal::open_item>(volume->mount_point(), ao->job);
                }
                else
                {
                    const std::string exe = vfs::linux::proc::self::exe();
                    const std::string qpath = ::utils::shell_quote(volume->mount_point());
                    const std::string command = std::format("{} {}", exe, qpath);
                    ztd::logger::info("COMMAND({})", command);
                    Glib::spawn_command_line_async(command);
                }
            }
            break;
        }
    }
    if (GTK_IS_WIDGET(ao->browser) && ao->job == ptk::browser::open_action::new_tab &&
        ao->browser->side_dev)
    {
        ptk::view::location::chdir(GTK_TREE_VIEW(ao->browser->side_dev), ao->browser->cwd());
    }

    delete ao;
    return false;
}

static bool
try_mount(GtkTreeView* view, const std::shared_ptr<vfs::volume>& vol) noexcept
{
    if (!view || !vol)
    {
        return false;
    }
    auto* browser = static_cast<ptk::browser*>(g_object_get_data(G_OBJECT(view), "browser"));
    if (!browser)
    {
        return false;
    }
    // task
    const auto check_mount_command = vol->device_mount_cmd();
    if (!check_mount_command)
    {
        popup_missing_mount(GTK_WIDGET(view), 0);
        return false;
    }
    const auto& mount_command = check_mount_command.value();

    const std::string task_name = std::format("Mount {}", vol->device_file());
    ptk::file_task* ptask = ptk_file_exec_new(task_name, GTK_WIDGET(view), browser->task_view());
    ptask->task->exec_command = mount_command;
    ptask->task->exec_sync = true;
    ptask->task->exec_browser = browser;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true; // set to true for error on click
    ptask->task->exec_terminal = false;
    ptask->task->exec_icon = vol->icon();

    // autoopen
    auto* const ao = new AutoOpen(browser);
    ao->devnum = vol->devnum();

    if (xset_get_b(xset::name::dev_newtab))
    {
        ao->job = ptk::browser::open_action::new_tab;
    }
    else
    {
        ao->job = ptk::browser::open_action::dir;
    }

    ptask->complete_notify_ = (GFunc)on_autoopen_cb;
    ptask->user_data_ = ao;

    ptask->run();

    return vol->is_mounted();
}

static void
on_open_tab(GtkMenuItem* item, const std::shared_ptr<vfs::volume>& vol, GtkWidget* view2) noexcept
{
    ptk::browser* browser = nullptr;
    GtkWidget* view = nullptr;

    if (!item)
    {
        view = view2;
    }
    else
    {
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    }
    if (view)
    {
        browser = static_cast<ptk::browser*>(g_object_get_data(G_OBJECT(view), "browser"));
    }
    else
    {
        browser = main_window_get_current_browser();
    }

    if (!browser || !vol)
    {
        return;
    }

    if (!vol->is_mounted())
    {
        // get mount command
        const auto check_mount_command = vol->device_mount_cmd();
        if (!check_mount_command)
        {
            popup_missing_mount(view, 0);
            return;
        }
        const auto& mount_command = check_mount_command.value();

        // task
        const std::string task_name = std::format("Mount {}", vol->device_file());
        ptk::file_task* ptask = ptk_file_exec_new(task_name, view, browser->task_view());
        ptask->task->exec_command = mount_command;
        ptask->task->exec_sync = true;
        ptask->task->exec_browser = browser;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        ptask->task->exec_terminal = false;
        ptask->task->exec_icon = vol->icon();

        // autoopen
        auto* const ao = new AutoOpen(browser);
        ao->devnum = vol->devnum();
        ao->job = ptk::browser::open_action::new_tab;

        ptask->complete_notify_ = (GFunc)on_autoopen_cb;
        ptask->user_data_ = ao;

        ptask->run();
    }
    else
    {
        browser->run_event<spacefm::signal::open_item>(vol->mount_point(),
                                                       ptk::browser::open_action::new_tab);
    }
}

static void
on_open(GtkMenuItem* item, const std::shared_ptr<vfs::volume>& vol, GtkWidget* view2) noexcept
{
    ptk::browser* browser = nullptr;
    GtkWidget* view = nullptr;
    if (!item)
    {
        view = view2;
    }
    else
    {
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    }
    if (view)
    {
        browser = static_cast<ptk::browser*>(g_object_get_data(G_OBJECT(view), "browser"));
    }
    else
    {
        browser = main_window_get_current_browser();
    }

    if (!vol)
    {
        return;
    }

    // Note: browser may be nullptr
    if (!GTK_IS_WIDGET(browser))
    {
        browser = nullptr;
    }

    if (!vol->is_mounted())
    {
        // get mount command
        const auto check_mount_command = vol->device_mount_cmd();
        if (!check_mount_command)
        {
            popup_missing_mount(view, 0);
            return;
        }
        const auto& mount_command = check_mount_command.value();

        // task
        const std::string task_name = std::format("Mount {}", vol->device_file());
        ptk::file_task* ptask =
            ptk_file_exec_new(task_name, view, browser ? browser->task_view() : nullptr);
        ptask->task->exec_command = mount_command;
        ptask->task->exec_sync = true;
        ptask->task->exec_browser = browser;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        ptask->task->exec_terminal = false;
        ptask->task->exec_icon = vol->icon();

        // autoopen
        auto* const ao = new AutoOpen(browser);
        ao->devnum = vol->devnum();
        ao->job = ptk::browser::open_action::dir;

        ptask->complete_notify_ = (GFunc)on_autoopen_cb;
        ptask->user_data_ = ao;

        ptask->run();
    }
    else if (browser)
    {
        browser->run_event<spacefm::signal::open_item>(vol->mount_point(),
                                                       ptk::browser::open_action::dir);
    }
    else
    {
        const std::string exe = vfs::linux::proc::self::exe();
        const std::string qpath = ::utils::shell_quote(vol->mount_point());
        const std::string command = std::format("{} {}", exe, qpath);
        ztd::logger::info("COMMAND({})", command);
        Glib::spawn_command_line_async(command);
    }
}

static void
on_showhide(GtkMenuItem* item, const std::shared_ptr<vfs::volume>& vol, GtkWidget* view2) noexcept
{
    std::string msg;
    GtkWidget* view = nullptr;
    if (!item)
    {
        view = view2;
    }
    else
    {
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    }

    const auto set = xset::set::get(xset::name::dev_show_hide_volumes);
    if (vol)
    {
        const std::string devid = ztd::removeprefix(vol->udi(), "/");

        msg = std::format("{}Currently Selected Device: {}\nVolume Label: {}\nDevice ID: {}",
                          set->desc.value(),
                          vol->device_file(),
                          vol->label(),
                          devid);
    }
    else
    {
        msg = set->desc.value();
    }
    const auto [response, answer] =
        xset_text_dialog(view, set->title.value(), msg, "", set->s.value(), "", false);
    set->s = answer;
    if (response)
    {
        update_all();
    }
}

static void
on_automountlist(GtkMenuItem* item, const std::shared_ptr<vfs::volume>& vol,
                 GtkWidget* view2) noexcept
{
    std::string msg;
    GtkWidget* view = nullptr;
    if (!item)
    {
        view = view2;
    }
    else
    {
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    }

    const auto set = xset::set::get(xset::name::dev_automount_volumes);
    if (vol)
    {
        const std::string devid = ztd::removeprefix(vol->udi(), "/");

        msg = std::format("{}Currently Selected Device: {}\nVolume Label: {}\nDevice ID: {}",
                          set->desc.value(),
                          vol->device_file(),
                          vol->label(),
                          devid);
    }
    else
    {
        msg = set->desc.value();
    }

    const auto [response, answer] =
        xset_text_dialog(view, set->title.value(), msg, "", set->s.value(), "", false);
    set->s = answer;
    if (response)
    {
        // update view / automount all?
    }
}

static bool
volume_is_visible(const std::shared_ptr<vfs::volume>& vol) noexcept
{
    // network
    if (vol->is_device_type(vfs::volume::device_type::network))
    {
        return xset_get_b(xset::name::dev_show_net);
    }

    // other - eg fuseiso mounted file
    if (vol->is_device_type(vfs::volume::device_type::other))
    {
        return xset_get_b(xset::name::dev_show_file);
    }

    // loop
    if (vol->device_file().starts_with("/dev/loop"))
    {
        if (vol->is_mounted() && xset_get_b(xset::name::dev_show_file))
        {
            return true;
        }
        if (!vol->is_mountable() && !vol->is_mounted())
        {
            return false;
        }
        // fall through
    }

    // ramfs CONFIG_BLK_DEV_RAM causes multiple entries of /dev/ram*
    if (!vol->is_mounted() && vol->device_file().starts_with("/dev/ram") && vol->device_file()[8] &&
        g_ascii_isdigit(vol->device_file()[8]))
    {
        return false;
    }

    // internal?
    if (!vol->is_removable() && !xset_get_b(xset::name::dev_show_internal_drives))
    {
        return false;
    }

    // hide?
    if (!vol->is_user_visible())
    {
        return false;
    }

    // has media?
    if (!vol->is_mountable() && !vol->is_mounted() && !xset_get_b(xset::name::dev_show_empty))
    {
        return false;
    }

    return true;
}

void
ptk::view::location::on_action(GtkWidget* view, const xset_t& set) noexcept
{
    // ztd::logger::info("ptk::view::location::on_action");
    if (!view)
    {
        return;
    }
    const auto vol = ptk::view::location::selected_volume(GTK_TREE_VIEW(view));

    if (set->xset_name == xset::name::dev_show_internal_drives ||
        set->xset_name == xset::name::dev_show_empty ||
        set->xset_name == xset::name::dev_show_partition_tables ||
        set->xset_name == xset::name::dev_show_net || set->xset_name == xset::name::dev_show_file ||
        set->xset_name == xset::name::dev_ignore_udisks_hide ||
        set->xset_name == xset::name::dev_show_hide_volumes ||
        set->xset_name == xset::name::dev_automount_optical ||
        set->xset_name == xset::name::dev_automount_removable ||
        set->xset_name == xset::name::dev_ignore_udisks_nopolicy)
    {
        update_all();
    }
    else if (set->xset_name == xset::name::dev_automount_volumes)
    {
        on_automountlist(nullptr, vol, view);
    }
    else if (set->xset_name == xset::name::dev_dispname)
    {
        update_names();
    }
    else if (set->xset_name == xset::name::dev_change)
    {
        update_change_detection();
    }
    else if (!vol)
    {
        return;
    }
    else
    {
        // require vol != nullptr
        if (set->name().starts_with("dev_menu_"))
        {
            if (set->xset_name == xset::name::dev_menu_remove)
            {
                on_eject(nullptr, vol, view);
            }
            else if (set->xset_name == xset::name::dev_menu_unmount)
            {
                on_umount(nullptr, vol, view);
            }
            else if (set->xset_name == xset::name::dev_menu_open)
            {
                on_open(nullptr, vol, view);
            }
            else if (set->xset_name == xset::name::dev_menu_tab)
            {
                on_open_tab(nullptr, vol, view);
            }
            else if (set->xset_name == xset::name::dev_menu_mount)
            {
                on_mount(nullptr, vol, view);
            }
        }
    }
}

static void
show_devices_menu(GtkTreeView* view, const std::shared_ptr<vfs::volume>& vol, ptk::browser* browser,
                  u32 button, const std::chrono::system_clock::time_point time_point) noexcept
{
    (void)button;
    (void)time_point;
    GtkWidget* popup = gtk_menu_new();

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

    {
        const auto set = xset::set::get(xset::name::dev_menu_remove);
        xset_set_cb(set, (GFunc)on_eject, vol.get());
        xset_set_ob(set, "view", view);
        set->disable = !vol;
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_unmount);
        xset_set_cb(set, (GFunc)on_umount, vol.get());
        xset_set_ob(set, "view", view);
        set->disable = !vol; //!( vol && vol->is_mounted );
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_open);
        xset_set_cb(set, (GFunc)on_open, vol.get());
        xset_set_ob(set, "view", view);
        set->disable = !vol;
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_tab);
        xset_set_cb(set, (GFunc)on_open_tab, vol.get());
        xset_set_ob(set, "view", view);
        set->disable = !vol;
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_mount);
        xset_set_cb(set, (GFunc)on_mount, vol.get());
        xset_set_ob(set, "view", view);
        set->disable = !vol; // || ( vol && vol->is_mounted );
    }

    xset_set_cb(xset::name::dev_show_internal_drives, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_show_empty, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_show_partition_tables, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_show_net, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_show_file, (GFunc)update_all, nullptr);
    // set->disable = xset_get_b(xset::name::DEV_SHOW_INTERNAL_DRIVES);
    xset_set_cb(xset::name::dev_ignore_udisks_hide, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_show_hide_volumes, (GFunc)on_showhide, vol.get());
    xset_set_cb(xset::name::dev_automount_optical, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_automount_removable, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_ignore_udisks_nopolicy, (GFunc)update_all, nullptr);

    {
        const auto set = xset::set::get(xset::name::dev_automount_volumes);
        xset_set_cb(set, (GFunc)on_automountlist, vol.get());
        xset_set_ob(set, "view", view);
    }

    std::vector<xset::name> context_menu_entries = {
        xset::name::dev_menu_remove,
        xset::name::dev_menu_unmount,
        xset::name::separator,
        xset::name::dev_menu_open,
        xset::name::dev_menu_tab,
        xset::name::dev_menu_mount,
    };

    if (vol && vol->is_device_type(vfs::volume::device_type::network) &&
        (vol->device_file().starts_with("//") || vol->device_file().contains(":/")))
    {
        context_menu_entries.push_back(xset::name::dev_menu_mark);
    }

    xset_add_menu(browser, popup, accel_group, context_menu_entries);

    xset_set_cb(xset::name::dev_dispname, (GFunc)update_names, nullptr);
    xset_set_cb(xset::name::dev_change, (GFunc)update_change_detection, nullptr);

    {
        const auto set = xset::set::get(xset::name::dev_menu_settings);
        set->context_menu_entries = {
            xset::name::dev_show,
            xset::name::separator,
            xset::name::dev_menu_auto,
            xset::name::dev_change,
            xset::name::separator,
            xset::name::dev_single,
            xset::name::dev_newtab,
        };
    }

    xset_add_menu(browser,
                  popup,
                  accel_group,
                  {
                      xset::name::separator,
                      xset::name::dev_menu_settings,
                  });

    gtk_widget_show_all(GTK_WIDGET(popup));

    g_signal_connect(G_OBJECT(popup), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);

    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
}

static bool
on_button_press_event(GtkTreeView* view, GdkEvent* event, void* user_data) noexcept
{
    (void)user_data;
    const std::shared_ptr<vfs::volume>& vol = nullptr;
    bool ret = false;

    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);
    const auto time_point = std::chrono::system_clock::from_time_t(gdk_event_get_time(event));

    if (type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }

    // ztd::logger::info("on_button_press_event   view = {}", view);
    auto* browser = static_cast<ptk::browser*>(g_object_get_data(G_OBJECT(view), "browser"));
    browser->focus_me();

    // get selected vol

    f64 x = NAN;
    f64 y = NAN;
    gdk_event_get_position(event, &x, &y);

    GtkTreePath* tree_path = nullptr;
    if (gtk_tree_view_get_path_at_pos(view,
                                      static_cast<i32>(x),
                                      static_cast<i32>(y),
                                      &tree_path,
                                      nullptr,
                                      nullptr,
                                      nullptr))
    {
        GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, tree_path))
        {
            gtk_tree_selection_select_iter(selection, &it);
            gtk_tree_model_get(model, &it, ptk::location_view::column::data, &vol, -1);
        }
    }

    switch (button)
    {
        case 1:
            // left button
            if (vol)
            {
                if (xset_get_b(xset::name::dev_single))
                {
                    gtk_tree_view_row_activated(view, tree_path, nullptr);
                    ret = true;
                }
            }
            else
            {
                gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(view));
                ret = true;
            }
            break;
        case 2:
            // middle button
            on_eject(nullptr, vol, GTK_WIDGET(view));
            ret = true;
            break;
        case 3:
            // right button
            show_devices_menu(view, vol, browser, button, time_point);
            ret = true;
            break;
    }

    if (tree_path)
    {
        gtk_tree_path_free(tree_path);
    }
    return ret;
}

static bool
on_key_press_event(GtkWidget* w, GdkEvent* event, ptk::browser* browser) noexcept
{
    (void)w;
    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto keyval = gdk_key_event_get_keyval(event);
    const auto time_point = std::chrono::system_clock::from_time_t(gdk_event_get_time(event));

    if (keyval == GDK_KEY_Menu ||
        (keyval == GDK_KEY_F10 && keymod == GdkModifierType::GDK_SHIFT_MASK))
    {
        // simulate right-click (menu)
        show_devices_menu(GTK_TREE_VIEW(browser->side_dev),
                          ptk::view::location::selected_volume(GTK_TREE_VIEW(browser->side_dev)),
                          browser,
                          3,
                          time_point);
        return true;
    }
    return false;
}

static void
on_dev_menu_hide(GtkWidget* widget, GtkWidget* dev_menu) noexcept
{
    gtk_widget_set_sensitive(widget, true);
    gtk_menu_shell_deactivate(GTK_MENU_SHELL(dev_menu));
}

static void
show_dev_design_menu(GtkWidget* menu, GtkWidget* dev_item, const std::shared_ptr<vfs::volume>& vol,
                     u32 button, const std::chrono::system_clock::time_point time_point) noexcept
{
    (void)dev_item;
    (void)time_point;
    ptk::browser* browser = nullptr;

    // validate vol
    for (const auto& volume : vfs::volume_get_all_volumes())
    {
        if (!volume)
        {
            continue;
        }

        if (volume == vol)
        {
            break;
        }
    }

    GtkWidget* view = GTK_WIDGET(g_object_get_data(G_OBJECT(menu), "parent"));
    if (xset_get_b(xset::name::dev_newtab))
    {
        browser = static_cast<ptk::browser*>(g_object_get_data(G_OBJECT(view), "browser"));
    }
    else
    {
        browser = nullptr;
    }

    // NOTE: browser may be nullptr
    switch (button)
    {
        case 1:
            // left-click - mount & open
            // device opener?  note that context may be based on devices list sel
            // will not work for desktop because no DesktopWindow currently available

            if (browser)
            {
                on_open_tab(nullptr, vol, view);
            }
            else
            {
                on_open(nullptr, vol, view);
            }
            return;
        case 2:
            // middle-click - Remove / Eject
            on_eject(nullptr, vol, view);
            return;
        default:
            break;
    }

    // create menu
    GtkWidget* popup = gtk_menu_new();

    {
        const auto set = xset::set::get(xset::name::dev_menu_remove);
        GtkWidget* item = gtk_menu_item_new_with_mnemonic(set->menu.label.value().data());
        g_object_set_data(G_OBJECT(item), "view", view);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_eject), vol.get());
        gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_unmount);
        GtkWidget* item = gtk_menu_item_new_with_mnemonic(set->menu.label.value().data());
        g_object_set_data(G_OBJECT(item), "view", view);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_umount), vol.get());
        gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
        gtk_widget_set_sensitive(item, !!vol);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(popup), gtk_separator_menu_item_new());

    {
        const auto set = xset::set::get(xset::name::dev_menu_open);
        GtkWidget* item = gtk_menu_item_new_with_mnemonic(set->menu.label.value().data());
        g_object_set_data(G_OBJECT(item), "view", view);
        gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
        if (browser)
        {
            g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_open_tab), vol.get());
        }
        else
        {
            g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_open), vol.get());
        }
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_mount);
        GtkWidget* item = gtk_menu_item_new_with_mnemonic(set->menu.label.value().data());
        g_object_set_data(G_OBJECT(item), "view", view);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_mount), vol.get());
        gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
        gtk_widget_set_sensitive(item, !!vol);
    }

    // show menu
    gtk_widget_show_all(GTK_WIDGET(popup));
    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
    gtk_widget_set_sensitive(GTK_WIDGET(menu), false);
    g_signal_connect(G_OBJECT(menu), "hide", G_CALLBACK(on_dev_menu_hide), popup);
    g_signal_connect(G_OBJECT(popup), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);

    gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(popup), true);
    // this is required when showing the menu via F2 or Menu key for focus
    gtk_menu_shell_select_first(GTK_MENU_SHELL(popup), true);
}

static bool
on_dev_menu_keypress(GtkWidget* menu, GdkEvent* event, void* user_data) noexcept
{
    (void)user_data;
    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(menu));
    if (item)
    {
        const auto keyval = gdk_key_event_get_keyval(event);
        const auto time_point = std::chrono::system_clock::from_time_t(gdk_event_get_time(event));

        // vfs::volume vol = VFS_VOLUME(g_object_get_data(G_OBJECT(item), "vol"));
        void* object_vol = g_object_get_data(G_OBJECT(item), "vol");
        const auto vol = static_cast<vfs::volume*>(object_vol)->shared_from_this();

        if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter || keyval == GDK_KEY_space)
        {
            // simulate left-click (mount)
            show_dev_design_menu(menu, item, vol, 1, time_point);
            return true;
        }
        else if (keyval == GDK_KEY_Menu || keyval == GDK_KEY_F2)
        {
            // simulate right-click (menu)
            show_dev_design_menu(menu, item, vol, 3, time_point);
        }
    }
    return false;
}

static bool
on_dev_menu_button_press(GtkWidget* item, GdkEvent* event,
                         const std::shared_ptr<vfs::volume>& vol) noexcept
{
    GtkWidget* menu = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "menu"));
    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);
    const auto time_point = std::chrono::system_clock::from_time_t(gdk_event_get_time(event));

    if (type == GdkEventType::GDK_BUTTON_RELEASE)
    {
        if (button == GDK_BUTTON_PRIMARY && keymod == 0)
        {
            // user released left button - due to an apparent gtk bug, activate
            // does not always fire on this event so handle it ourselves
            // test: gtk2 Crux theme with touchpad on Edit|Copy To|Location
            // https://github.com/IgnorantGuru/spacefm/issues/31
            // https://github.com/IgnorantGuru/spacefm/issues/228
            if (menu)
            {
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
            }

            gtk_menu_item_activate(GTK_MENU_ITEM(item));
            return true;
        }
        return false;
    }
    else if (type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }

    show_dev_design_menu(menu, item, vol, button, time_point);
    return true;
}

#if 0
static i32
cmp_dev_name(const std::shared_ptr<vfs::volume>& a, const std::shared_ptr<vfs::volume>& b) noexcept
{
    return a->display_name().compare(b->display_name());
}
#endif

void
ptk::view::location::dev_menu(GtkWidget* parent, ptk::browser* browser, GtkWidget* menu) noexcept
{ // add currently visible devices to menu with dev design mode callback
    g_object_set_data(G_OBJECT(menu), "parent", parent);
    // browser may be nullptr
    g_object_set_data(G_OBJECT(parent), "browser", browser);

    for (const auto& volume : vfs::volume_get_all_volumes())
    {
        if (!volume || !volume_is_visible(volume))
        {
            continue;
        }

        GtkWidget* item = gtk_menu_item_new_with_label(volume->display_name().data());
        g_object_set_data(G_OBJECT(item), "menu", menu);
        g_object_set_data(G_OBJECT(item), "vol", volume.get());
        // clang-format off
        g_signal_connect(G_OBJECT(item), "button-press-event", G_CALLBACK(on_dev_menu_button_press), volume.get());
        g_signal_connect(G_OBJECT(item), "button-release-event", G_CALLBACK(on_dev_menu_button_press), volume.get());
        // clang-format on
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    g_signal_connect(G_OBJECT(menu), "key_press_event", G_CALLBACK(on_dev_menu_keypress), nullptr);

    xset_set_cb(xset::name::dev_show_internal_drives, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_show_empty, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_show_partition_tables, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_show_net, (GFunc)update_all, nullptr);
    xset_set_cb(xset::name::dev_show_file, (GFunc)update_all, nullptr);
    // set->disable = xset_get_b(xset::name::DEV_SHOW_INTERNAL_DRIVES);
    xset_set_cb(xset::name::dev_ignore_udisks_hide, (GFunc)update_all, nullptr);
    // xset_set_cb(xset::name::dev_show_hide_volumes, (GFunc)on_showhide, vol.get());
    xset_set_cb(xset::name::dev_automount_optical, (GFunc)update_all, nullptr);
    // bool auto_optical = set->b == xset::set::enabled::yes;
    xset_set_cb(xset::name::dev_automount_removable, (GFunc)update_all, nullptr);
    // bool auto_removable = set->b == xset::set::enabled::yes;
    xset_set_cb(xset::name::dev_ignore_udisks_nopolicy, (GFunc)update_all, nullptr);
    // xset_set_cb(xset::name::dev_automount_volumes, (GFunc)on_automountlist, vol.get());
    xset_set_cb(xset::name::dev_change, (GFunc)update_change_detection, nullptr);

    const auto set = xset::set::get(xset::name::dev_menu_settings);
    set->context_menu_entries = {
        xset::name::dev_show,
        xset::name::separator,
        xset::name::dev_menu_auto,
        xset::name::dev_change,
        xset::name::dev_newtab,
    };
}
