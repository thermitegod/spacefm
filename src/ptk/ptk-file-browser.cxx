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
#include <unordered_map>

#include <optional>

#include <memory>

#include <functional>

#include <ranges>

#include <cassert>

#include <fmt/format.h>

#include <gtkmm.h>
#include <gdkmm.h>
#include <glibmm.h>

#include <exo/exo.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-toolbar.hxx"
#include "xset/xset-misc.hxx"

#include "ptk/ptk-dialog.hxx"

#include "ptk/ptk-file-actions-open.hxx"
#include "ptk/ptk-file-actions-rename.hxx"
#include "ptk/ptk-file-actions-paste.hxx"

#include "ptk/ptk-bookmark-view.hxx"
#include "ptk/ptk-file-properties.hxx"
#include "ptk/ptk-location-view.hxx"
#include "ptk/ptk-dir-tree-view.hxx"
#include "ptk/ptk-dir-tree.hxx"
#include "ptk/ptk-task-view.hxx"

#include "ptk/ptk-file-list.hxx"
#include "ptk/ptk-keyboard.hxx"
#include "ptk/ptk-clipboard.hxx"

#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-path-entry.hxx"
#include "main-window.hxx"

#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-file.hxx"

#include "settings/app.hxx"

#include "utils/memory.hxx"

#include "signals.hxx"

#include "types.hxx"

#include "autosave.hxx"
#include "utils.hxx"

#include "ptk/ptk-file-browser.hxx"

static void ptk_file_browser_class_init(PtkFileBrowserClass* klass);
static void ptk_file_browser_init(PtkFileBrowser* file_browser);
static void ptk_file_browser_finalize(GObject* obj);
static void ptk_file_browser_get_property(GObject* obj, u32 prop_id, GValue* value,
                                          GParamSpec* pspec);
static void ptk_file_browser_set_property(GObject* obj, u32 prop_id, const GValue* value,
                                          GParamSpec* pspec);

/* Utility functions */
static GtkWidget* create_folder_view(PtkFileBrowser* file_browser,
                                     ptk::file_browser::view_mode view_mode);

static void init_list_view(PtkFileBrowser* file_browser, GtkTreeView* list_view);

static GtkWidget* ptk_file_browser_create_dir_tree(PtkFileBrowser* file_browser);

/* Get GtkTreePath of the item at coordinate x, y */
static GtkTreePath* folder_view_get_tree_path_at_pos(PtkFileBrowser* file_browser, i32 x, i32 y);

/* signal handlers */
static void on_folder_view_item_activated(ExoIconView* iconview, GtkTreePath* path,
                                          PtkFileBrowser* file_browser);
static void on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path,
                                         GtkTreeViewColumn* col, PtkFileBrowser* file_browser);
static void on_folder_view_item_sel_change(ExoIconView* iconview, PtkFileBrowser* file_browser);

static bool on_folder_view_button_press_event(GtkWidget* widget, GdkEvent* event,
                                              PtkFileBrowser* file_browser);
static bool on_folder_view_button_release_event(GtkWidget* widget, GdkEvent* event,
                                                PtkFileBrowser* file_browser);
static bool on_folder_view_popup_menu(GtkWidget* widget, PtkFileBrowser* file_browser);

void on_dir_tree_row_activated(GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column,
                               PtkFileBrowser* file_browser);

/* Drag & Drop */
static void on_folder_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context,
                                              i32 x, i32 y, GtkSelectionData* sel_data, u32 info,
                                              u32 time, void* user_data);

static void on_folder_view_drag_data_get(GtkWidget* widget, GdkDragContext* drag_context,
                                         GtkSelectionData* sel_data, u32 info, u32 time,
                                         PtkFileBrowser* file_browser);

static void on_folder_view_drag_begin(GtkWidget* widget, GdkDragContext* drag_context,
                                      PtkFileBrowser* file_browser);

static bool on_folder_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, i32 x,
                                       i32 y, u32 time, PtkFileBrowser* file_browser);

static bool on_folder_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context, u32 time,
                                      PtkFileBrowser* file_browser);

static bool on_folder_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y,
                                     u32 time, PtkFileBrowser* file_browser);

static void on_folder_view_drag_end(GtkWidget* widget, GdkDragContext* drag_context,
                                    PtkFileBrowser* file_browser);

/* Default signal handlers */
static void ptk_file_browser_before_chdir(PtkFileBrowser* file_browser,
                                          const std::filesystem::path& path);
static void ptk_file_browser_after_chdir(PtkFileBrowser* file_browser);
static void ptk_file_browser_content_change(PtkFileBrowser* file_browser);
static void ptk_file_browser_sel_change(PtkFileBrowser* file_browser);
static void ptk_file_browser_open_item(PtkFileBrowser* file_browser,
                                       const std::filesystem::path& path, i32 action);
static void ptk_file_browser_pane_mode_change(PtkFileBrowser* file_browser);

static i32 file_list_order_from_sort_order(ptk::file_browser::sort_order order);

static GtkPanedClass* parent_class = nullptr;

static void rebuild_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser);
static void rebuild_side_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser);

static u32 folder_view_auto_scroll_timer = 0;
static GtkDirectionType folder_view_auto_scroll_direction = GtkDirectionType::GTK_DIR_TAB_FORWARD;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {{ztd::strdup("text/uri-list"), 0, 0}};

#define GDK_ACTION_ALL                                                                             \
    GdkDragAction(GdkDragAction::GDK_ACTION_MOVE | GdkDragAction::GDK_ACTION_COPY |                \
                  GdkDragAction::GDK_ACTION_LINK)

// instance-wide command history
std::vector<std::string> xset_cmd_history;

// history of closed tabs
static std::unordered_map<panel_t, std::vector<std::filesystem::path>> closed_tabs_restore{};

struct selection_history_data
{
    std::unordered_map<std::filesystem::path, std::vector<std::filesystem::path>>
        selection_history{};
};

void
navigation_history_data::go_back() noexcept
{
    if (this->back_.empty())
    {
        // ztd::logger::debug("Back navigation history is empty");
        return;
    }
    this->forward_.emplace_back(this->current_);
    this->current_ = this->back_.back();
    this->back_.pop_back();
}

bool
navigation_history_data::has_back() const noexcept
{
    return !this->back_.empty();
}

const std::vector<std::filesystem::path>&
navigation_history_data::get_back() const noexcept
{
    return this->back_;
}

void
navigation_history_data::go_forward() noexcept
{
    if (this->forward_.empty())
    {
        // ztd::logger::debug("Forward navigation history is empty");
        return;
    }
    this->back_.emplace_back(this->current_);
    this->current_ = this->forward_.back();
    this->forward_.pop_back();
}

bool
navigation_history_data::has_forward() const noexcept
{
    return !this->forward_.empty();
}

const std::vector<std::filesystem::path>&
navigation_history_data::get_forward() const noexcept
{
    return this->forward_;
}

void
navigation_history_data::new_forward(const std::filesystem::path& path) noexcept
{
    // ztd::logger::debug("New Forward navigation history");
    if (!this->current_.empty())
    {
        this->back_.emplace_back(this->current_);
    }
    this->current_ = path;
    this->forward_.clear();
}

void
navigation_history_data::reset() noexcept
{
    this->back_.clear();
    this->forward_.clear();
}

const std::filesystem::path&
navigation_history_data::path(const ptk::file_browser::chdir_mode mode) const noexcept
{
    switch (mode)
    {
        case ptk::file_browser::chdir_mode::normal:
            return this->current_;
        case ptk::file_browser::chdir_mode::back:
            assert(!this->back_.empty());
            return this->back_.back();
        case ptk::file_browser::chdir_mode::forward:
            assert(!this->forward_.empty());
            return this->forward_.back();
    }
}

struct column_data
{
    std::string_view title;
    xset::panel xset_name;
    ptk::file_list::column column;
};

// must match ipc-command.cxx run_ipc_command()
static constexpr std::array<column_data, 12> columns{
    {{
         "Name",
         xset::panel::detcol_name,
         ptk::file_list::column::name,
     },
     {
         "Size",
         xset::panel::detcol_size,
         ptk::file_list::column::size,
     },
     {
         "Size in Bytes",
         xset::panel::detcol_bytes,
         ptk::file_list::column::bytes,
     },
     {
         "Type",
         xset::panel::detcol_type,
         ptk::file_list::column::type,
     },
     {
         "MIME Type",
         xset::panel::detcol_mime,
         ptk::file_list::column::mime,
     },
     {
         "Permissions",
         xset::panel::detcol_perm,
         ptk::file_list::column::perm,
     },
     {
         "Owner",
         xset::panel::detcol_owner,
         ptk::file_list::column::owner,
     },
     {
         "Group",
         xset::panel::detcol_group,
         ptk::file_list::column::group,
     },
     {
         "Date Accessed",
         xset::panel::detcol_atime,
         ptk::file_list::column::atime,
     },
     {
         "Date Created",
         xset::panel::detcol_btime,
         ptk::file_list::column::btime,
     },
     {
         "Date Metadata Change",
         xset::panel::detcol_ctime,
         ptk::file_list::column::ctime,
     },
     {
         "Date Modified",
         xset::panel::detcol_mtime,
         ptk::file_list::column::mtime,
     }},
};

GType
ptk_file_browser_get_type()
{
    static GType type = G_TYPE_INVALID;
    if (type == G_TYPE_INVALID)
    {
        static const GTypeInfo info = {
            sizeof(PtkFileBrowserClass),
            nullptr,
            nullptr,
            (GClassInitFunc)ptk_file_browser_class_init,
            nullptr,
            nullptr,
            sizeof(PtkFileBrowser),
            0,
            (GInstanceInitFunc)ptk_file_browser_init,
            nullptr,
        };
        type = g_type_register_static(GTK_TYPE_BOX,
                                      "PtkFileBrowser",
                                      &info,
                                      GTypeFlags::G_TYPE_FLAG_NONE);
    }
    return type;
}

static void
ptk_file_browser_class_init(PtkFileBrowserClass* klass)
{
    GObjectClass* object_class = (GObjectClass*)klass;
    parent_class = (GtkPanedClass*)g_type_class_peek_parent(klass);

    object_class->set_property = ptk_file_browser_set_property;
    object_class->get_property = ptk_file_browser_get_property;
    object_class->finalize = ptk_file_browser_finalize;

    /* Signals */

    klass->before_chdir = ptk_file_browser_before_chdir;
    klass->after_chdir = ptk_file_browser_after_chdir;
    klass->open_item = ptk_file_browser_open_item;
    klass->content_change = ptk_file_browser_content_change;
    klass->sel_change = ptk_file_browser_sel_change;
    klass->pane_mode_change = ptk_file_browser_pane_mode_change;
}

bool
PtkFileBrowser::using_large_icons() const noexcept
{
    return this->large_icons_;
}

bool
PtkFileBrowser::is_busy() const noexcept
{
    return this->busy_;
}

bool
PtkFileBrowser::pending_drag_status_tree() const noexcept
{
    return this->pending_drag_status_tree_;
}

void
PtkFileBrowser::pending_drag_status_tree(bool val) noexcept
{
    this->pending_drag_status_tree_ = val;
}

GtkSortType
PtkFileBrowser::sort_type() const noexcept
{
    return this->sort_type_;
}

bool
PtkFileBrowser::is_sort_type(GtkSortType type) const noexcept
{
    return this->sort_type_ == type;
}

ptk::file_browser::sort_order
PtkFileBrowser::sort_order() const noexcept
{
    return this->sort_order_;
}

bool
PtkFileBrowser::is_sort_order(ptk::file_browser::sort_order type) const noexcept
{
    return this->sort_order_ == type;
}

ptk::file_browser::view_mode
PtkFileBrowser::view_mode() const noexcept
{
    return this->view_mode_;
}

bool
PtkFileBrowser::is_view_mode(ptk::file_browser::view_mode type) const noexcept
{
    return this->view_mode_ == type;
}

GtkWidget*
PtkFileBrowser::folder_view() const noexcept
{
    return this->folder_view_;
}

void
PtkFileBrowser::folder_view(GtkWidget* new_folder_view) noexcept
{
    this->folder_view_ = new_folder_view;
}

GtkScrolledWindow*
PtkFileBrowser::folder_view_scroll() const noexcept
{
    return this->folder_view_scroll_;
}

GtkCellRenderer*
PtkFileBrowser::icon_render() const noexcept
{
    return this->icon_render_;
}

panel_t
PtkFileBrowser::panel() const noexcept
{
    return this->panel_;
}

GtkWidget*
PtkFileBrowser::task_view() const noexcept
{
    return this->task_view_;
}

MainWindow*
PtkFileBrowser::main_window() const noexcept
{
    return this->main_window_;
}

GtkEntry*
PtkFileBrowser::path_bar() const noexcept
{
    return this->path_bar_;
}

static void
save_command_history(GtkEntry* entry)
{
#if (GTK_MAJOR_VERSION == 4)
    const std::string text = gtk_editable_get_text(GTK_EDITABLE(entry));
#elif (GTK_MAJOR_VERSION == 3)
    const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));
#endif

    if (text.empty())
    {
        return;
    }

    xset_cmd_history.emplace_back(text);

    // shorten to 200 entries
    while (xset_cmd_history.size() > 200)
    {
        xset_cmd_history.erase(xset_cmd_history.begin());
    }
}

static bool
on_address_bar_focus_in(GtkWidget* entry, GdkEventFocus* evt, PtkFileBrowser* file_browser)
{
    (void)entry;
    (void)evt;
    file_browser->focus_me();
    return false;
}

static void
on_address_bar_activate(GtkWidget* entry, PtkFileBrowser* file_browser)
{
#if (GTK_MAJOR_VERSION == 4)
    const std::string text = gtk_editable_get_text(GTK_EDITABLE(entry));
#elif (GTK_MAJOR_VERSION == 3)
    const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));
#endif

    if (text.empty())
    {
        return;
    }

    gtk_editable_select_region(GTK_EDITABLE(entry), 0, 0); // clear selection

    // network path
    if ((!text.starts_with('/') && text.contains(":/")) || text.starts_with("//"))
    {
        save_command_history(GTK_ENTRY(entry));
        ptk_location_view_mount_network(file_browser, text, false, false);
        return;
    }

    if (!std::filesystem::exists(text))
    {
        return;
    }
    const auto dir_path = std::filesystem::canonical(text);

    if (std::filesystem::is_directory(dir_path))
    { // open dir
        if (!std::filesystem::equivalent(dir_path, file_browser->cwd()))
        {
            file_browser->chdir(dir_path);
        }
    }
    else if (std::filesystem::is_regular_file(dir_path))
    { // open dir and select file
        const auto dirname_path = dir_path.parent_path();
        if (!std::filesystem::equivalent(dirname_path, file_browser->cwd()))
        {
            file_browser->chdir(dirname_path);
        }
        else
        {
            file_browser->select_file(dir_path);
        }
    }
    else if (std::filesystem::is_block_file(dir_path))
    { // open block device
        // ztd::logger::info("opening block device: {}", dir_path);
        ptk_location_view_open_block(dir_path, false);
    }
    else
    { // do nothing for other special files
        // ztd::logger::info("special file ignored: {}", dir_path);
        // return;
    }

    gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view_));
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);

    // inhibit auto seek because if multiple completions will change dir
    EntryData* edata = ENTRY_DATA(g_object_get_data(G_OBJECT(entry), "edata"));
    if (edata && edata->seek_timer)
    {
        g_source_remove(edata->seek_timer);
        edata->seek_timer = 0;
    }
}

void
ptk_file_browser_add_toolbar_widget(const xset_t& set, GtkWidget* widget)
{ // store the toolbar widget created by set for later change of status
    assert(set != nullptr);

    if (!(set && !set->lock && set->browser && set->tool != xset::tool::NOT &&
          GTK_IS_WIDGET(widget)))
    {
        return;
    }

    unsigned char x;

    switch (set->tool)
    {
        case xset::tool::up:
            x = 0;
            break;
        case xset::tool::back:
        case xset::tool::back_menu:
            x = 1;
            break;
        case xset::tool::fwd:
        case xset::tool::fwd_menu:
            x = 2;
            break;
        case xset::tool::devices:
            x = 3;
            break;
        case xset::tool::bookmarks:
            // Deprecated - bookmark
            x = 4;
            break;
        case xset::tool::tree:
            x = 5;
            break;
        case xset::tool::show_hidden:
            x = 6;
            break;
        case xset::tool::custom:
            if (set->menu_style == xset::menu::check)
            {
                x = 7;
                // attach set pointer to custom checkboxes so we can find it
                g_object_set_data(G_OBJECT(widget), "set", set->name.data());
            }
            else
            {
                return;
            }
            break;
        case xset::tool::show_thumb:
            x = 8;
            break;
        case xset::tool::large_icons:
            x = 9;
            break;
        case xset::tool::NOT:
        case xset::tool::home:
        case xset::tool::DEFAULT:
        case xset::tool::refresh:
        case xset::tool::new_tab:
        case xset::tool::new_tab_here:
        case xset::tool::invalid:
            return;
    }

    set->browser->toolbar_widgets[x] = g_slist_append(set->browser->toolbar_widgets[x], widget);
}

static void
rebuild_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    (void)widget;
    // ztd::logger::info("rebuild_toolbox");
    if (!file_browser)
    {
        return;
    }

    const panel_t p = file_browser->panel_;
    const xset::main_window_panel mode = file_browser->main_window_->panel_context.at(p);

    const bool show_tooltips = !xset_get_b_panel(1, xset::panel::tool_l);

    // destroy
    if (file_browser->toolbar)
    {
        if (GTK_IS_WIDGET(file_browser->toolbar))
        {
            gtk_widget_destroy(GTK_WIDGET(file_browser->toolbar));
        }
        file_browser->toolbar = nullptr;
        file_browser->path_bar_ = nullptr;
    }

    if (!file_browser->path_bar_)
    {
        file_browser->path_bar_ = ptk_path_entry_new(file_browser);

        // clang-format off
        g_signal_connect(G_OBJECT(file_browser->path_bar_), "activate", G_CALLBACK(on_address_bar_activate), file_browser);
        g_signal_connect(G_OBJECT(file_browser->path_bar_), "focus-in-event", G_CALLBACK(on_address_bar_focus_in), file_browser);
        // clang-format on
    }

    // create toolbar
    file_browser->toolbar = GTK_TOOLBAR(gtk_toolbar_new());
    gtk_box_pack_start(file_browser->toolbox_, GTK_WIDGET(file_browser->toolbar), true, true, 0);
    gtk_toolbar_set_style(file_browser->toolbar, GtkToolbarStyle::GTK_TOOLBAR_ICONS);
    if (app_settings.icon_size_tool() > 0 &&
        app_settings.icon_size_tool() <= GtkIconSize::GTK_ICON_SIZE_DIALOG)
    {
        gtk_toolbar_set_icon_size(file_browser->toolbar,
                                  (GtkIconSize)app_settings.icon_size_tool());
    }

    // fill left toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->toolbar,
                      xset_get_panel(p, xset::panel::tool_l),
                      show_tooltips);

    // add pathbar
    GtkBox* hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    GtkToolItem* toolitem = gtk_tool_item_new();
    gtk_tool_item_set_expand(toolitem, true);
    gtk_toolbar_insert(file_browser->toolbar, toolitem, -1);
    gtk_container_add(GTK_CONTAINER(toolitem), GTK_WIDGET(hbox));
    gtk_box_pack_start(hbox, GTK_WIDGET(file_browser->path_bar_), true, true, 5);

    // fill right toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->toolbar,
                      xset_get_panel(p, xset::panel::tool_r),
                      show_tooltips);

    // show
    if (xset_get_b_panel_mode(p, xset::panel::show_toolbox, mode))
    {
        gtk_widget_show_all(GTK_WIDGET(file_browser->toolbox_));
    }
}

static void
rebuild_side_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    (void)widget;
    const panel_t p = file_browser->panel_;
    const xset::main_window_panel mode = file_browser->main_window_
                                             ? file_browser->main_window_->panel_context.at(p)
                                             : xset::main_window_panel::panel_neither;

    const bool show_tooltips = !xset_get_b_panel(1, xset::panel::tool_l);

    // destroy
    if (file_browser->side_toolbar)
    {
        gtk_widget_destroy(GTK_WIDGET(file_browser->side_toolbar));
    }

    // create side toolbar
    file_browser->side_toolbar = GTK_TOOLBAR(gtk_toolbar_new());

    gtk_box_pack_start(file_browser->side_toolbox,
                       GTK_WIDGET(file_browser->side_toolbar),
                       true,
                       true,
                       0);
    gtk_toolbar_set_style(file_browser->side_toolbar, GtkToolbarStyle::GTK_TOOLBAR_ICONS);
    if (app_settings.icon_size_tool() > 0 &&
        app_settings.icon_size_tool() <= GtkIconSize::GTK_ICON_SIZE_DIALOG)
    {
        gtk_toolbar_set_icon_size(file_browser->side_toolbar,
                                  (GtkIconSize)app_settings.icon_size_tool());
    }
    // fill side toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->side_toolbar,
                      xset_get_panel(p, xset::panel::tool_s),
                      show_tooltips);

    // show
    if (xset_get_b_panel_mode(p, xset::panel::show_sidebar, mode))
    {
        gtk_widget_show_all(GTK_WIDGET(file_browser->side_toolbox));
    }
}

static bool
on_status_bar_button_press(GtkWidget* widget, GdkEvent* event, PtkFileBrowser* file_browser)
{
    (void)widget;
    file_browser->focus_folder_view();

    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);

    if (type == GdkEventType::GDK_BUTTON_PRESS)
    {
        if (button == 2)
        {
            static constexpr std::array<xset::name, 4> setnames{
                xset::name::status_name,
                xset::name::status_path,
                xset::name::status_info,
                xset::name::status_hide,
            };

            for (const auto i : std::views::iota(0uz, setnames.size()))
            {
                if (!xset_get_b(setnames.at(i)))
                {
                    continue;
                }

                if (i < 2)
                {
                    const auto selected_files = file_browser->selected_files();
                    if (selected_files.empty())
                    {
                        return true;
                    }

                    if (i == 0)
                    {
                        ptk_clipboard_copy_name(selected_files);
                    }
                    else
                    {
                        ptk_clipboard_copy_as_text(selected_files);
                    }
                }
                else if (i == 2)
                { // Scroll Wheel click
                    ptk_show_file_properties(nullptr,
                                             file_browser->cwd(),
                                             file_browser->selected_files(),
                                             0);
                }
                else if (i == 3)
                {
                    file_browser->main_window_->focus_panel(panel_control_code_hide);
                }
            }
            return true;
        }
    }
    return false;
}

static void
on_status_effect_change(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    set_panel_focus(nullptr, file_browser);
}

static void
on_status_middle_click_config(GtkMenuItem* menuitem, const xset_t& set)
{
    (void)menuitem;

    static constexpr std::array<xset::name, 4> setnames{
        xset::name::status_name,
        xset::name::status_path,
        xset::name::status_info,
        xset::name::status_hide,
    };

    for (const xset::name setname : setnames)
    {
        if (set->xset_name == setname)
        {
            set->b = xset::b::xtrue;
        }
        else
        {
            xset_set_b(setname, false);
        }
    }
}

static void
on_status_bar_popup(GtkWidget* widget, GtkWidget* menu, PtkFileBrowser* file_browser)
{
    (void)widget;

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

    xset_t set;

    set = xset_get(xset::name::status_name);
    xset_set_cb(xset::name::status_name, (GFunc)on_status_middle_click_config, set.get());
    xset_set_ob2(set, nullptr, nullptr);
    const xset_t set_radio = set;
    set = xset_get(xset::name::status_path);
    xset_set_cb(xset::name::status_path, (GFunc)on_status_middle_click_config, set.get());
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set = xset_get(xset::name::status_info);
    xset_set_cb(xset::name::status_info, (GFunc)on_status_middle_click_config, set.get());
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set = xset_get(xset::name::status_hide);
    xset_set_cb(xset::name::status_hide, (GFunc)on_status_middle_click_config, set.get());
    xset_set_ob2(set, nullptr, set_radio->name.data());

    xset_add_menu(file_browser,
                  menu,
                  accel_group,
                  {xset::name::separator, xset::name::status_middle});
    gtk_widget_show_all(GTK_WIDGET(menu));
}

static void
ptk_file_browser_init(PtkFileBrowser* file_browser)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(file_browser),
                                   GtkOrientation::GTK_ORIENTATION_VERTICAL);

    file_browser->panel_ = 0; // do not load font yet in ptk_path_entry_new
    file_browser->path_bar_ = ptk_path_entry_new(file_browser);

    // clang-format off
    g_signal_connect(G_OBJECT(file_browser->path_bar_), "activate", G_CALLBACK(on_address_bar_activate), file_browser);
    g_signal_connect(G_OBJECT(file_browser->path_bar_), "focus-in-event", G_CALLBACK(on_address_bar_focus_in), file_browser);
    // clang-format on

    // toolbox
    file_browser->toolbar = nullptr;
    file_browser->toolbox_ = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_box_pack_start(GTK_BOX(file_browser), GTK_WIDGET(file_browser->toolbox_), false, false, 0);

    // lists area
    file_browser->hpane = GTK_PANED(gtk_paned_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL));
    file_browser->side_vbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_size_request(GTK_WIDGET(file_browser->side_vbox), 140, -1);
    file_browser->folder_view_scroll_ =
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    gtk_paned_pack1(file_browser->hpane, GTK_WIDGET(file_browser->side_vbox), false, false);
    gtk_paned_pack2(file_browser->hpane, GTK_WIDGET(file_browser->folder_view_scroll_), true, true);

    // fill side
    file_browser->side_toolbox =
        GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    file_browser->side_toolbar = nullptr;
    file_browser->side_vpane_top =
        GTK_PANED(gtk_paned_new(GtkOrientation::GTK_ORIENTATION_VERTICAL));
    file_browser->side_vpane_bottom =
        GTK_PANED(gtk_paned_new(GtkOrientation::GTK_ORIENTATION_VERTICAL));
    file_browser->side_dir_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    file_browser->side_dev_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    gtk_box_pack_start(file_browser->side_vbox,
                       GTK_WIDGET(file_browser->side_toolbox),
                       false,
                       false,
                       0);
    gtk_box_pack_start(file_browser->side_vbox,
                       GTK_WIDGET(file_browser->side_vpane_top),
                       true,
                       true,
                       0);
    // see https://github.com/BwackNinja/spacefm/issues/21
    gtk_paned_pack1(file_browser->side_vpane_top,
                    GTK_WIDGET(file_browser->side_dev_scroll),
                    false,
                    false);
    gtk_paned_pack2(file_browser->side_vpane_top,
                    GTK_WIDGET(file_browser->side_vpane_bottom),
                    true,
                    false);
    gtk_paned_pack2(file_browser->side_vpane_bottom,
                    GTK_WIDGET(file_browser->side_dir_scroll),
                    true,
                    false);

    // status bar
    file_browser->statusbar = GTK_STATUSBAR(gtk_statusbar_new());

    GList* children = gtk_container_get_children(GTK_CONTAINER(file_browser->statusbar));
    file_browser->status_frame = GTK_FRAME(children->data);
    g_list_free(children);
    children = gtk_container_get_children(
        GTK_CONTAINER(gtk_statusbar_get_message_area(file_browser->statusbar)));
    file_browser->status_label = GTK_LABEL(children->data);
    g_list_free(children);
    // do not know panel yet
    file_browser->status_image = xset_get_image("gtk-yes", GtkIconSize::GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(file_browser->statusbar),
                       file_browser->status_image,
                       false,
                       false,
                       0);
    // required for button event
    gtk_label_set_selectable(file_browser->status_label, true);
    gtk_widget_set_can_focus(GTK_WIDGET(file_browser->status_label), false);
    gtk_widget_set_hexpand(GTK_WIDGET(file_browser->status_label), true);
    gtk_widget_set_halign(GTK_WIDGET(file_browser->status_label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(file_browser->status_label), GtkAlign::GTK_ALIGN_CENTER);

    // clang-format off
    g_signal_connect(G_OBJECT(file_browser->status_label), "button-press-event", G_CALLBACK(on_status_bar_button_press), file_browser);
    g_signal_connect(G_OBJECT(file_browser->status_label), "populate-popup", G_CALLBACK(on_status_bar_popup), file_browser);
    // clang-format on

    // pack fb vbox
    gtk_box_pack_start(GTK_BOX(file_browser), GTK_WIDGET(file_browser->hpane), true, true, 0);
    // TODO pack task frames
    gtk_box_pack_start(GTK_BOX(file_browser), GTK_WIDGET(file_browser->statusbar), false, false, 0);

    gtk_scrolled_window_set_policy(file_browser->folder_view_scroll_,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_policy(file_browser->side_dir_scroll,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_policy(file_browser->side_dev_scroll,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);

    // clang-format off
    g_signal_connect(G_OBJECT(file_browser->hpane), "button-release-event", G_CALLBACK(ptk_file_browser_slider_release), file_browser);
    g_signal_connect(G_OBJECT(file_browser->side_vpane_top), "button-release-event", G_CALLBACK(ptk_file_browser_slider_release), file_browser);
    g_signal_connect(G_OBJECT(file_browser->side_vpane_bottom), "button-release-event", G_CALLBACK(ptk_file_browser_slider_release), file_browser);
    // clang-format on

    file_browser->selection_history = std::make_unique<selection_history_data>();
    file_browser->navigation_history = std::make_unique<navigation_history_data>();
}

static void
ptk_file_browser_finalize(GObject* obj)
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(obj);
    // ztd::logger::info("ptk_file_browser_finalize");

    file_browser->dir_ = nullptr;

    /* Remove all idle handlers which are not called yet. */
    do
    {
    } while (g_source_remove_by_user_data(file_browser));

    if (file_browser->file_list_)
    {
        g_signal_handlers_disconnect_matched(file_browser->file_list_,
                                             GSignalMatchType::G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(G_OBJECT(file_browser->file_list_));
    }

    for (auto& toolbar_widget : file_browser->toolbar_widgets)
    {
        g_slist_free(toolbar_widget);
        toolbar_widget = nullptr;
    }

    G_OBJECT_CLASS(parent_class)->finalize(obj);

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility that killing the browser results in
     * thousands of large thumbnails being freed, but the memory not actually
     * released by SpaceFM */
    memory_trim();
}

static void
ptk_file_browser_get_property(GObject* obj, u32 prop_id, GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static void
ptk_file_browser_set_property(GObject* obj, u32 prop_id, const GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

GtkWidget*
ptk_file_browser_new(i32 curpanel, GtkNotebook* notebook, GtkWidget* task_view,
                     MainWindow* main_window)
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(g_object_new(PTK_TYPE_FILE_BROWSER, nullptr));

    file_browser->panel_ = curpanel;
    file_browser->notebook_ = notebook;
    file_browser->task_view_ = task_view;
    file_browser->main_window_ = main_window;

    for (auto& toolbar_widget : file_browser->toolbar_widgets)
    {
        toolbar_widget = nullptr;
    }

    if (xset_get_b_panel(curpanel, xset::panel::list_detailed))
    {
        file_browser->view_mode_ = ptk::file_browser::view_mode::list_view;
    }
    else if (xset_get_b_panel(curpanel, xset::panel::list_icons))
    {
        file_browser->view_mode_ = ptk::file_browser::view_mode::icon_view;
        gtk_scrolled_window_set_policy(file_browser->folder_view_scroll_,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC);
    }
    else if (xset_get_b_panel(curpanel, xset::panel::list_compact))
    {
        file_browser->view_mode_ = ptk::file_browser::view_mode::compact_view;
        gtk_scrolled_window_set_policy(file_browser->folder_view_scroll_,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC);
    }
    else
    {
        file_browser->view_mode_ = ptk::file_browser::view_mode::list_view;
        xset_set_panel(curpanel, xset::panel::list_detailed, xset::var::b, "1");
    }

    // Large Icons - option for Detailed and Compact list views
    file_browser->large_icons_ =
        file_browser->view_mode_ == ptk::file_browser::view_mode::icon_view ||
        xset_get_b_panel_mode(file_browser->panel_,
                              xset::panel::list_large,
                              main_window->panel_context.at(file_browser->panel_));
    file_browser->folder_view(create_folder_view(file_browser, file_browser->view_mode_));

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll_),
                                  GTK_WIDGET(file_browser->folder_view_));
    // gtk_widget_show_all(GTK_WIDGET(file_browser->folder_view_scroll_));

    // set status bar icon
    gtk_image_set_from_icon_name(GTK_IMAGE(file_browser->status_image),
                                 "gtk-yes",
                                 GtkIconSize::GTK_ICON_SIZE_MENU);

    gtk_widget_show_all(GTK_WIDGET(file_browser));

    return GTK_IS_WIDGET(file_browser) ? GTK_WIDGET(file_browser) : nullptr;
}

void
PtkFileBrowser::update_tab_label() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    GtkBox* box = GTK_BOX(gtk_notebook_get_tab_label(this->notebook_, GTK_WIDGET(this)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkEventBox* ebox =
        GTK_EVENT_BOX(gtk_notebook_get_tab_label(this->notebook_, GTK_WIDGET(this)));
    GtkBox* box = GTK_BOX(g_object_get_data(G_OBJECT(ebox), "box"));
#endif

    GtkLabel* label = GTK_LABEL(g_object_get_data(G_OBJECT(box), "label"));

    // TODO: Change the icon
    // GtkWidget* icon = GTK_WIDGET(g_object_get_data(G_OBJECT(box), "icon"));

    const auto& cwd = this->cwd();
    const std::string name = std::filesystem::equivalent(cwd, "/") ? "/" : cwd.filename();
    gtk_label_set_text(label, name.data());
    if (name.size() < 30)
    {
        gtk_label_set_ellipsize(label, PangoEllipsizeMode::PANGO_ELLIPSIZE_NONE);
        gtk_label_set_width_chars(label, -1);
    }
    else
    {
        gtk_label_set_ellipsize(label, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_width_chars(label, 30);
    }
}

static void
on_history_menu_item_activate(GtkWidget* menu_item, PtkFileBrowser* file_browser)
{
    const std::filesystem::path path =
        static_cast<const char*>(g_object_get_data(G_OBJECT(menu_item), "path"));
    file_browser->chdir(path);
}

static GtkWidget*
add_history_menu_item(PtkFileBrowser* file_browser, GtkWidget* menu,
                      const std::filesystem::path& path)
{
    GtkWidget* menu_item = gtk_menu_item_new_with_label(path.filename().c_str());
    g_object_set_data_full(G_OBJECT(menu_item),
                           "path",
                           ztd::strdup(path.c_str()),
                           (GDestroyNotify)std::free);

    // clang-format off
    g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(on_history_menu_item_activate), file_browser);
    // clang-format on

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    return menu_item;
}

static bool
ptk_file_browser_content_changed(PtkFileBrowser* file_browser)
{
    file_browser->run_event<spacefm::signal::change_content>();
    return false;
}

void
PtkFileBrowser::on_folder_content_changed(const std::shared_ptr<vfs::file>& file)
{
    if (file == nullptr)
    {
        // The current directory itself changed
        if (!std::filesystem::is_directory(this->cwd()))
        {
            // current directory does not exist - was renamed
            this->close_tab();
        }
    }
    else
    {
        g_idle_add((GSourceFunc)ptk_file_browser_content_changed, this);
    }
}

void
PtkFileBrowser::on_folder_content_deleted(const std::shared_ptr<vfs::file>& file)
{
    if (file == nullptr)
    {
        // The directory itself was deleted
        this->close_tab();
        // file_browser->chdir(vfs::user_dirs->home_dir());
    }
    else
    {
        this->on_folder_content_changed(file);
    }
}

static void
on_sort_col_changed(GtkTreeSortable* sortable, PtkFileBrowser* file_browser)
{
    i32 col;
    gtk_tree_sortable_get_sort_column_id(sortable, &col, &file_browser->sort_type_);

    const auto column = ptk::file_list::column(col);
    auto sort_order = ptk::file_browser::sort_order::name;
    switch (column)
    {
        case ptk::file_list::column::name:
            sort_order = ptk::file_browser::sort_order::name;
            break;
        case ptk::file_list::column::size:
            sort_order = ptk::file_browser::sort_order::size;
            break;
        case ptk::file_list::column::bytes:
            sort_order = ptk::file_browser::sort_order::bytes;
            break;
        case ptk::file_list::column::type:
            sort_order = ptk::file_browser::sort_order::type;
            break;
        case ptk::file_list::column::mime:
            sort_order = ptk::file_browser::sort_order::mime;
            break;
        case ptk::file_list::column::perm:
            sort_order = ptk::file_browser::sort_order::perm;
            break;
        case ptk::file_list::column::owner:
            sort_order = ptk::file_browser::sort_order::owner;
            break;
        case ptk::file_list::column::group:
            sort_order = ptk::file_browser::sort_order::group;
            break;
        case ptk::file_list::column::atime:
            sort_order = ptk::file_browser::sort_order::atime;
            break;
        case ptk::file_list::column::btime:
            sort_order = ptk::file_browser::sort_order::btime;
            break;
        case ptk::file_list::column::ctime:
            sort_order = ptk::file_browser::sort_order::ctime;
            break;
        case ptk::file_list::column::mtime:
            sort_order = ptk::file_browser::sort_order::mtime;
            break;
        case ptk::file_list::column::big_icon:
        case ptk::file_list::column::small_icon:
        case ptk::file_list::column::info:
            break;
    }
    file_browser->sort_order_ = sort_order;

    xset_set_panel(file_browser->panel_,
                   xset::panel::list_detailed,
                   xset::var::x,
                   std::to_string(magic_enum::enum_integer(file_browser->sort_order_)));
    xset_set_panel(file_browser->panel_,
                   xset::panel::list_detailed,
                   xset::var::y,
                   std::to_string(file_browser->sort_type_));
}

void
PtkFileBrowser::update_model() noexcept
{
    PtkFileList* list = ptk_file_list_new(this->dir_, this->show_hidden_files_);
    GtkTreeModel* old_list = this->file_list_;
    this->file_list_ = GTK_TREE_MODEL(list);
    if (old_list)
    {
        g_object_unref(G_OBJECT(old_list));
    }

    this->read_sort_extra();
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list),
                                         file_list_order_from_sort_order(this->sort_order_),
                                         this->sort_type_);

    this->show_thumbnails(this->max_thumbnail_);

    // clang-format off
    g_signal_connect(G_OBJECT(list), "sort-column-changed", G_CALLBACK(on_sort_col_changed), this);
    // clang-format on

    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_set_model(EXO_ICON_VIEW(this->folder_view_), GTK_TREE_MODEL(list));
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_set_model(GTK_TREE_VIEW(this->folder_view_), GTK_TREE_MODEL(list));
            break;
    }

    // try to smooth list bounce created by delayed re-appearance of column headers
    // while (g_main_context_pending(nullptr))
    //    g_main_context_iteration(nullptr, true);
}

void
PtkFileBrowser::on_dir_file_listed(bool is_cancelled)
{
    this->n_sel_files_ = 0;

    if (!is_cancelled)
    {
        this->signal_file_created.disconnect();
        this->signal_file_deleted.disconnect();
        this->signal_file_changed.disconnect();

        this->signal_file_created = this->dir_->add_event<spacefm::signal::file_created>(
            std::bind(&PtkFileBrowser::on_folder_content_changed, this, std::placeholders::_1));
        this->signal_file_deleted = this->dir_->add_event<spacefm::signal::file_deleted>(
            std::bind(&PtkFileBrowser::on_folder_content_deleted, this, std::placeholders::_1));
        this->signal_file_changed = this->dir_->add_event<spacefm::signal::file_changed>(
            std::bind(&PtkFileBrowser::on_folder_content_changed, this, std::placeholders::_1));
    }

    this->update_model();
    this->busy_ = false;

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility that changing the directory results in
     * thousands of large thumbnails being freed, but the memory not actually
     * released by SpaceFM */
    memory_trim();

    this->run_event<spacefm::signal::chdir_after>();
    this->run_event<spacefm::signal::change_content>();
    this->run_event<spacefm::signal::change_sel>();

    if (this->side_dir)
    {
        ptk_dir_tree_view_chdir(GTK_TREE_VIEW(this->side_dir), this->cwd());
    }

    if (this->side_dev)
    {
        ptk_location_view_chdir(GTK_TREE_VIEW(this->side_dev), this->cwd());
    }

    // FIXME:  This is already done in update_model, but is there any better way to
    //            reduce unnecessary code?
    if (this->view_mode_ == ptk::file_browser::view_mode::compact_view)
    { // sfm why is this needed for compact view???
        if (!is_cancelled && this->file_list_)
        {
            this->show_thumbnails(this->max_thumbnail_);
        }
    }
}

/* signal handlers */

static void
on_folder_view_item_activated(ExoIconView* iconview, GtkTreePath* path,
                              PtkFileBrowser* file_browser)
{
    (void)iconview;
    (void)path;
    file_browser->open_selected_files();
}

static void
on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* col,
                             PtkFileBrowser* file_browser)
{
    (void)tree_view;
    (void)path;
    (void)col;
    // file_browser->button_press = false;
    file_browser->open_selected_files();
}

static bool
on_folder_view_item_sel_change_idle(PtkFileBrowser* file_browser)
{
    if (!GTK_IS_WIDGET(file_browser))
    {
        return false;
    }

    file_browser->n_sel_files_ = 0;
    file_browser->sel_size_ = 0;
    file_browser->sel_disk_size_ = 0;

    GtkTreeModel* model;
    GList* selected_files = file_browser->selected_items(&model);

    for (GList* sel = selected_files; sel; sel = g_list_next(sel))
    {
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, (GtkTreePath*)sel->data))
        {
            std::shared_ptr<vfs::file> file;
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (file)
            {
                file_browser->sel_size_ += file->size();
                file_browser->sel_disk_size_ += file->size_on_disk();
            }
        }
    }

    file_browser->n_sel_files_ = g_list_length(selected_files);

    g_list_foreach(selected_files, (GFunc)gtk_tree_path_free, nullptr);
    g_list_free(selected_files);

    file_browser->run_event<spacefm::signal::change_sel>();
    file_browser->sel_change_idle_ = 0;
    return false;
}

static void
on_folder_view_item_sel_change(ExoIconView* iconview, PtkFileBrowser* file_browser)
{
    (void)iconview;
    /* //sfm on_folder_view_item_sel_change fires for each selected file
     * when a file is clicked - causes hang if thousands of files are selected
     * So add only one g_idle_add at a time
     */
    if (file_browser->sel_change_idle_)
    {
        return;
    }

    file_browser->sel_change_idle_ =
        g_idle_add((GSourceFunc)on_folder_view_item_sel_change_idle, file_browser);
}

static void
show_popup_menu(PtkFileBrowser* file_browser, GdkEvent* event)
{
    (void)event;

    const auto selected_files = file_browser->selected_files();

    GtkWidget* popup = ptk_file_menu_new(file_browser, selected_files);
    if (popup)
    {
        gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
    }
}

/* invoke popup menu via shortcut key */
static bool
on_folder_view_popup_menu(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    (void)widget;
    show_popup_menu(file_browser, nullptr);
    return true;
}

static bool
on_folder_view_button_press_event(GtkWidget* widget, GdkEvent* event, PtkFileBrowser* file_browser)
{
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;
    GtkTreeViewColumn* col = nullptr;
    GtkTreeSelection* selection = nullptr;
    bool ret = false;

    if (file_browser->menu_shown_)
    {
        file_browser->menu_shown_ = false;
    }

    const auto keymod = ptk_get_keymod(gdk_event_get_modifier_state(event));
    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);

    if (type == GdkEventType::GDK_BUTTON_PRESS)
    {
        file_browser->focus_folder_view();
        // file_browser->button_press = true;

        if (button == 4 || button == 5 || button == 8 || button == 9)
        {
            if (button == 4 || button == 8)
            {
                file_browser->go_back();
            }
            else
            {
                file_browser->go_forward();
            }
            return true;
        }

        // Alt - Left/Right Click

#if (GTK_MAJOR_VERSION == 4)
        if ((keymod == GdkModifierType::GDK_ALT_MASK) && (button == 1 || button == 3))
#elif (GTK_MAJOR_VERSION == 3)
        if ((keymod == GdkModifierType::GDK_MOD1_MASK) && (button == 1 || button == 3))
#endif
        {
            if (button == 1)
            {
                file_browser->go_back();
            }
            else
            {
                file_browser->go_forward();
            }
            return true;
        }

        f64 x, y;
        gdk_event_get_position(event, &x, &y);

        switch (file_browser->view_mode_)
        {
            case ptk::file_browser::view_mode::icon_view:
            case ptk::file_browser::view_mode::compact_view:
                tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), x, y);
                model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));

                /* deselect selected files when right click on blank area */
                if (!tree_path && button == 3)
                {
                    exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                }
                break;
            case ptk::file_browser::view_mode::list_view:
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              x,
                                              y,
                                              &tree_path,
                                              &col,
                                              nullptr,
                                              nullptr);
                selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

                if (col &&
                    ptk::file_list::column(gtk_tree_view_column_get_sort_column_id(col)) !=
                        ptk::file_list::column::name &&
                    tree_path) // MOD
                {
                    gtk_tree_path_free(tree_path);
                    tree_path = nullptr;
                }
                break;
        }

        /* an item is clicked, get its file path */
        std::shared_ptr<vfs::file> file = nullptr;
        GtkTreeIter it;
        if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
        {
            // item is clicked
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
        }

        /* middle button */
        if (file && button == 2) /* middle click on a item */
        {
            /* open in new tab if its a directory */
            if (file->is_directory())
            {
                file_browser->run_event<spacefm::signal::open_item>(file->path(),
                                                                    ptk::open_action::new_tab);
            }
            ret = true;
        }
        else if (button == 3) /* right click */
        {
            /* cancel all selection, and select the item if it is not selected */
            switch (file_browser->view_mode_)
            {
                case ptk::file_browser::view_mode::icon_view:
                case ptk::file_browser::view_mode::compact_view:
                    if (tree_path &&
                        !exo_icon_view_path_is_selected(EXO_ICON_VIEW(widget), tree_path))
                    {
                        exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                        exo_icon_view_select_path(EXO_ICON_VIEW(widget), tree_path);
                    }
                    break;
                case ptk::file_browser::view_mode::list_view:
                    if (tree_path && !gtk_tree_selection_path_is_selected(selection, tree_path))
                    {
                        gtk_tree_selection_unselect_all(selection);
                        gtk_tree_selection_select_path(selection, tree_path);
                    }
                    break;
            }

            show_popup_menu(file_browser, event);
            /* FIXME if approx 5000 are selected, right-click sometimes unselects all
             * after this button_press function returns - why?  a gtk or exo bug?
             * Always happens with above show_popup_menu call disabled
             * Only when this occurs, cursor is automatically set to current row and
             * treesel 'changed' signal fires
             * Stopping changed signal had no effect
             * Using connect rather than connect_after had no effect
             * Removing signal connect had no effect
             * FIX: inhibit button release */
            ret = file_browser->menu_shown_ = true;
        }
        gtk_tree_path_free(tree_path);
    }
    else if (type == GdkEventType::GDK_2BUTTON_PRESS && button == 1)
    {
        // double click event -  button = 0

        if (file_browser->view_mode_ == ptk::file_browser::view_mode::list_view)
        {
            /* set ret true to prevent drag_begin starting in this tab after
             * fuseiso mount.  Why?
             * row_activated occurs before GdkEventType::GDK_2BUTTON_PRESS so use
             * file_browser->button_press to determine if row was already
             * activated or user clicked on non-row */
            ret = true;
        }
        else if (!app_settings.single_click())
        {
            /* sfm 1.0.6 set skip_release for Icon/Compact to prevent file
             * under cursor being selected when entering dir with double-click.
             * Also see conditional reset of skip_release in
             * file_browser->chdir(). See also
             * on_folder_view_button_release_event() */
            file_browser->skip_release_ = true;
        }
    }
    return ret;
}

static bool
on_folder_view_button_release_event(GtkWidget* widget, GdkEvent* event,
                                    PtkFileBrowser* file_browser)
{ // on left-click release on file, if not dnd or rubberbanding, unselect files
    (void)widget;
    GtkTreePath* tree_path = nullptr;

    const auto keymod = ptk_get_keymod(gdk_event_get_modifier_state(event));
    const auto button = gdk_button_event_get_button(event);

    if (file_browser->is_drag_ || button != 1 || file_browser->skip_release_ ||
        (keymod & (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK |
#if (GTK_MAJOR_VERSION == 4)
                   GdkModifierType::GDK_ALT_MASK
#elif (GTK_MAJOR_VERSION == 3)
                   GdkModifierType::GDK_MOD1_MASK
#endif
                   )))
    {
        if (file_browser->skip_release_)
        {
            file_browser->skip_release_ = false;
        }
        // this fixes bug where right-click shows menu and release unselects files
        const bool ret = file_browser->menu_shown_ && button != 1;
        if (file_browser->menu_shown_)
        {
            file_browser->menu_shown_ = false;
        }
        return ret;
    }

#if 0
    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::PTK_FB_ICON_VIEW:
        case ptk::file_browser::view_mode::compact_view:
            //if (exo_icon_view_is_rubber_banding_active(EXO_ICON_VIEW(widget)))
            //    return false;
            /* Conditional on single_click below was removed 1.0.2 708f0988 bc it
             * caused a left-click to not unselect other files.  However, this
             * caused file under cursor to be selected when entering directory by
             * double-click in Icon/Compact styles.  To correct this, 1.0.6
             * conditionally sets skip_release on GdkEventType::GDK_2BUTTON_PRESS, and does not
             * reset skip_release in file_browser->chdir(). */
            tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), event->x, event->y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
            if (tree_path)
            {
                // unselect all but one file
                exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                exo_icon_view_select_path(EXO_ICON_VIEW(widget), tree_path);
            }
            break;
        case ptk::file_browser::view_mode::PTK_FB_LIST_VIEW:
            if (gtk_tree_view_is_rubber_banding_active(GTK_TREE_VIEW(widget)))
                return false;
            if (app_settings.get_single_click())
            {
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              static_cast<i32>(event->x),
                                              static_cast<i32>(event->y),
                                              &tree_path,
                                              nullptr,
                                              nullptr,
                                              nullptr);
                tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
                if (tree_path && tree_sel && gtk_tree_selection_count_selected_rows(tree_sel) > 1)
                {
                    // unselect all but one file
                    gtk_tree_selection_unselect_all(tree_sel);
                    gtk_tree_selection_select_path(tree_sel, tree_path);
                }
            }
            break;
    }
#endif

    gtk_tree_path_free(tree_path);
    return false;
}

static bool
on_dir_tree_update_sel(PtkFileBrowser* file_browser)
{
    if (!file_browser->side_dir)
    {
        return false;
    }

    const auto check_dir_path =
        ptk_dir_tree_view_get_selected_dir(GTK_TREE_VIEW(file_browser->side_dir));
    if (check_dir_path)
    {
        const auto& dir_path = check_dir_path.value();

        if (!std::filesystem::equivalent(dir_path, file_browser->cwd()))
        {
            if (file_browser->chdir(dir_path))
            {
#if (GTK_MAJOR_VERSION == 4)
                gtk_editable_set_text(GTK_EDITABLE(file_browser->path_bar_), dir_path.c_str());
#elif (GTK_MAJOR_VERSION == 3)
                gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar_), dir_path.c_str());
#endif
            }
        }
    }
    return false;
}

void
on_dir_tree_row_activated(GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column,
                          PtkFileBrowser* file_browser)
{
    (void)view;
    (void)path;
    (void)column;
    g_idle_add((GSourceFunc)on_dir_tree_update_sel, file_browser);
}

static void
on_folder_view_columns_changed(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    // user dragged a column to a different position - save positions
    if (!(GTK_IS_WIDGET(file_browser) && GTK_IS_TREE_VIEW(view)))
    {
        return;
    }

    if (file_browser->view_mode_ != ptk::file_browser::view_mode::list_view)
    {
        return;
    }

    for (const auto i : std::views::iota(0uz, columns.size()))
    {
        GtkTreeViewColumn* col = gtk_tree_view_get_column(view, static_cast<i32>(i));
        if (!col)
        {
            return;
        }
        const char* title = gtk_tree_view_column_get_title(col);
        for (const auto column : columns)
        {
            if (title == column.title)
            {
                // save column position
                xset_t set = xset_get_panel(file_browser->panel_, column.xset_name);
                set->x = std::to_string(i);

                break;
            }
        }
    }
}

static void
on_folder_view_destroy(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    (void)file_browser;
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
}

static bool
folder_view_search_equal(GtkTreeModel* model, i32 col, const char* c_key, GtkTreeIter* it,
                         void* search_data)
{
    (void)search_data;
    bool no_match;

    const auto column = ptk::file_list::column(col);
    if (column != ptk::file_list::column::name)
    {
        return true;
    }

    char* c_name;

    gtk_tree_model_get(model, it, col, &c_name, -1);
    if (!c_name || !c_key)
    {
        return true;
    }

    std::string key = c_key;
    std::string name = c_name;

    if (ztd::lower(key) == key)
    {
        // key is all lowercase so do icase search
        name = ztd::lower(name);
    }

    if (key.contains("*") || key.contains('?'))
    {
        const std::string key2 = std::format("*{}*", key);
        no_match = !ztd::fnmatch(key2, name);
    }
    else
    {
        const bool end = ztd::endswith(key, "$");
        bool start = !end && (key.size() < 3);
        char* key2 = ztd::strdup(key);
        char* keyp = key2;
        if (key[0] == '^')
        {
            keyp++;
            start = true;
        }
        if (end)
        {
            key2[std::strlen(key2) - 1] = '\0';
        }
        if (start && end)
        {
            no_match = !name.contains(keyp);
        }
        else if (start)
        {
            no_match = !name.starts_with(keyp);
        }
        else if (end)
        {
            no_match = !name.ends_with(keyp);
        }
        else
        {
            no_match = !name.contains(key);
        }
        std::free(key2);
    }
    return no_match; // return false for match
}

static GtkWidget*
create_folder_view(PtkFileBrowser* file_browser, ptk::file_browser::view_mode view_mode)
{
    GtkWidget* folder_view = nullptr;
    GtkTreeSelection* selection = nullptr;
    GtkCellRenderer* renderer;

    i32 icon_size = 0;
    const i32 big_icon_size = app_settings.icon_size_big();
    const i32 small_icon_size = app_settings.icon_size_small();

    PangoAttrList* attr_list = pango_attr_list_new();
    pango_attr_list_insert(attr_list, pango_attr_insert_hyphens_new(false));

    switch (view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            folder_view = exo_icon_view_new();

            if (view_mode == ptk::file_browser::view_mode::compact_view)
            {
                icon_size = file_browser->large_icons_ ? big_icon_size : small_icon_size;

                exo_icon_view_set_layout_mode(EXO_ICON_VIEW(folder_view),
                                              EXO_ICON_VIEW_LAYOUT_COLS);
                exo_icon_view_set_orientation(EXO_ICON_VIEW(folder_view),
                                              GtkOrientation::GTK_ORIENTATION_HORIZONTAL);
            }
            else
            {
                icon_size = big_icon_size;

                exo_icon_view_set_column_spacing(EXO_ICON_VIEW(folder_view), 4);
                exo_icon_view_set_item_width(EXO_ICON_VIEW(folder_view),
                                             icon_size < 110 ? 110 : icon_size);
            }

            exo_icon_view_set_selection_mode(EXO_ICON_VIEW(folder_view),
                                             GtkSelectionMode::GTK_SELECTION_MULTIPLE);

            // search
            exo_icon_view_set_enable_search(EXO_ICON_VIEW(folder_view), true);
            exo_icon_view_set_search_column(EXO_ICON_VIEW(folder_view),
                                            magic_enum::enum_integer(ptk::file_list::column::name));
            exo_icon_view_set_search_equal_func(
                EXO_ICON_VIEW(folder_view),
                (ExoIconViewSearchEqualFunc)folder_view_search_equal,
                nullptr,
                nullptr);

            gtk_icon_view_set_activate_on_single_click(GTK_ICON_VIEW(folder_view),
                                                       file_browser->single_click_);

            gtk_cell_layout_clear(GTK_CELL_LAYOUT(folder_view));

            file_browser->icon_render_ = renderer = gtk_cell_renderer_pixbuf_new();

            /* add the icon renderer */
            g_object_set(G_OBJECT(renderer), "follow_state", true, nullptr);
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(folder_view), renderer, false);
            gtk_cell_layout_add_attribute(
                GTK_CELL_LAYOUT(folder_view),
                renderer,
                "pixbuf",
                file_browser->large_icons_
                    ? magic_enum::enum_integer(ptk::file_list::column::big_icon)
                    : magic_enum::enum_integer(ptk::file_list::column::small_icon));

            /* add the name renderer */
            renderer = gtk_cell_renderer_text_new();

            if (view_mode == ptk::file_browser::view_mode::compact_view)
            {
                g_object_set(
                    G_OBJECT(renderer),
                    "xalign",
                    0.0,
                    "yalign",
                    0.5,
                    "font",
                    xset_get_s(xset::name::font_view_compact).value_or("Monospace 9").c_str(),
                    "size-set",
                    true,
                    nullptr);
            }
            else
            {
                g_object_set(G_OBJECT(renderer),
                             "alignment",
                             PangoAlignment::PANGO_ALIGN_CENTER,
                             "wrap-mode",
                             PangoWrapMode::PANGO_WRAP_WORD_CHAR,
                             "wrap-width",
                             105, // FIXME prob shouldnt hard code this
                                  // icon_size < 105 ? 109 : icon_size, // breaks with cjk text
                             "xalign",
                             0.5,
                             "yalign",
                             0.0,
                             "attributes",
                             attr_list,
                             "font",
                             xset_get_s(xset::name::font_view_icon).value_or("Monospace 9").c_str(),
                             "size-set",
                             true,
                             nullptr);
            }
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(folder_view), renderer, true);
            gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(folder_view),
                                          renderer,
                                          "text",
                                          magic_enum::enum_integer(ptk::file_list::column::name));

            exo_icon_view_enable_model_drag_source(
                EXO_ICON_VIEW(folder_view),
                GdkModifierType(GdkModifierType::GDK_CONTROL_MASK |
                                GdkModifierType::GDK_BUTTON1_MASK |
                                GdkModifierType::GDK_BUTTON3_MASK),
                drag_targets,
                G_N_ELEMENTS(drag_targets),
                GDK_ACTION_ALL);

            exo_icon_view_enable_model_drag_dest(EXO_ICON_VIEW(folder_view),
                                                 drag_targets,
                                                 G_N_ELEMENTS(drag_targets),
                                                 GDK_ACTION_ALL);

            // clang-format off
            g_signal_connect(G_OBJECT(folder_view), "item-activated", G_CALLBACK(on_folder_view_item_activated), file_browser);
            g_signal_connect_after(G_OBJECT(folder_view), "selection-changed", G_CALLBACK(on_folder_view_item_sel_change), file_browser);
            // clang-format on

            break;
        case ptk::file_browser::view_mode::list_view:
            folder_view = gtk_tree_view_new();

            init_list_view(file_browser, GTK_TREE_VIEW(folder_view));

            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(folder_view));
            gtk_tree_selection_set_mode(selection, GtkSelectionMode::GTK_SELECTION_MULTIPLE);

            if (xset_get_b(xset::name::rubberband))
            {
                gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(folder_view), true);
            }

            // Search
            gtk_tree_view_set_enable_search(GTK_TREE_VIEW(folder_view), true);
            gtk_tree_view_set_search_column(GTK_TREE_VIEW(folder_view),
                                            magic_enum::enum_integer(ptk::file_list::column::name));
            gtk_tree_view_set_search_equal_func(
                GTK_TREE_VIEW(folder_view),
                (GtkTreeViewSearchEqualFunc)folder_view_search_equal,
                nullptr,
                nullptr);

            gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(folder_view),
                                                       file_browser->single_click_);

            icon_size = file_browser->large_icons_ ? big_icon_size : small_icon_size;

            gtk_tree_view_enable_model_drag_source(
                GTK_TREE_VIEW(folder_view),
                GdkModifierType(GdkModifierType::GDK_CONTROL_MASK |
                                GdkModifierType::GDK_BUTTON1_MASK |
                                GdkModifierType::GDK_BUTTON3_MASK),
                drag_targets,
                G_N_ELEMENTS(drag_targets),
                GDK_ACTION_ALL);

            gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(folder_view),
                                                 drag_targets,
                                                 G_N_ELEMENTS(drag_targets),
                                                 GDK_ACTION_ALL);

            // clang-format off
            g_signal_connect(G_OBJECT(folder_view), "row_activated", G_CALLBACK(on_folder_view_row_activated), file_browser);
            g_signal_connect_after(G_OBJECT(selection), "changed", G_CALLBACK(on_folder_view_item_sel_change), file_browser);
            g_signal_connect(G_OBJECT(folder_view), "columns-changed", G_CALLBACK(on_folder_view_columns_changed), file_browser);
            g_signal_connect(G_OBJECT(folder_view), "destroy", G_CALLBACK(on_folder_view_destroy), file_browser);
            // clang-format on
            break;
    }

    gtk_cell_renderer_set_fixed_size(file_browser->icon_render_, icon_size, icon_size);

    // clang-format off
    g_signal_connect(G_OBJECT(folder_view), "button-press-event", G_CALLBACK(on_folder_view_button_press_event), file_browser);
    g_signal_connect(G_OBJECT(folder_view), "button-release-event", G_CALLBACK(on_folder_view_button_release_event), file_browser);
    g_signal_connect(G_OBJECT(folder_view), "popup-menu", G_CALLBACK(on_folder_view_popup_menu), file_browser);
    // init drag & drop support
    g_signal_connect(G_OBJECT(folder_view), "drag-data-received", G_CALLBACK(on_folder_view_drag_data_received), file_browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-data-get", G_CALLBACK(on_folder_view_drag_data_get), file_browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-begin", G_CALLBACK(on_folder_view_drag_begin), file_browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-motion", G_CALLBACK(on_folder_view_drag_motion), file_browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-leave", G_CALLBACK(on_folder_view_drag_leave), file_browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-drop", G_CALLBACK(on_folder_view_drag_drop), file_browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-end", G_CALLBACK(on_folder_view_drag_end), file_browser);
    // clang-format on

    return folder_view;
}

static void
init_list_view(PtkFileBrowser* file_browser, GtkTreeView* list_view)
{
    const panel_t p = file_browser->panel_;
    const xset::main_window_panel mode = file_browser->main_window_->panel_context.at(p);

    for (const auto column : columns)
    {
        GtkTreeViewColumn* col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable(col, true);

        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();

        // column order
        usize idx;
        for (const auto [order_index, order_value] : std::views::enumerate(columns))
        {
            idx = order_index;
            if (xset_get_int_panel(p, columns.at(order_index).xset_name, xset::var::x) ==
                magic_enum::enum_integer(column.column))
            {
                break;
            }
        }

        // column width
        gtk_tree_view_column_set_min_width(col, 50);
        gtk_tree_view_column_set_sizing(col, GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
        const xset_t set = xset_get_panel_mode(p, columns.at(idx).xset_name, mode);
        const i32 width = set->y ? std::stoi(set->y.value()) : 100;
        if (width)
        {
            if (column.column == ptk::file_list::column::name && !app_settings.always_show_tabs() &&
                file_browser->view_mode_ == ptk::file_browser::view_mode::list_view &&
                gtk_notebook_get_n_pages(file_browser->notebook_) == 1)
            {
                // when tabs are added, the width of the notebook decreases
                // by a few pixels, meaning there is not enough space for
                // all columns - this causes a horizontal scrollbar to
                // appear on new and sometimes first tab
                // so shave some pixels off first columns
                gtk_tree_view_column_set_fixed_width(col, width - 6);

                // below causes increasing reduction of column every time new tab is
                // added and closed - undesirable
                PtkFileBrowser* first_fb = PTK_FILE_BROWSER_REINTERPRET(
                    gtk_notebook_get_nth_page(file_browser->notebook_, 0));

                if (first_fb && first_fb->view_mode_ == ptk::file_browser::view_mode::list_view &&
                    GTK_IS_TREE_VIEW(first_fb->folder_view_))
                {
                    GtkTreeViewColumn* first_col =
                        gtk_tree_view_get_column(GTK_TREE_VIEW(first_fb->folder_view_), 0);
                    if (first_col)
                    {
                        const i32 first_width = gtk_tree_view_column_get_width(first_col);
                        if (first_width > 10)
                        {
                            gtk_tree_view_column_set_fixed_width(first_col, first_width - 6);
                        }
                    }
                }
            }
            else
            {
                gtk_tree_view_column_set_fixed_width(col, width);
                // ztd::logger::info("init set_width {} {}", magic_enum::enum_name(columns.at(index).xset_name), width);
            }
        }

        if (column.column == ptk::file_list::column::name)
        {
            g_object_set(G_OBJECT(renderer),
                         /* "editable", true, */
                         "ellipsize",
                         PangoEllipsizeMode::PANGO_ELLIPSIZE_END,
                         nullptr);

            // g_signal_connect(G_OBJECT(renderer), "editing-started", G_CALLBACK(on_filename_editing_started), nullptr);

            GtkCellRenderer* pix_renderer;
            file_browser->icon_render_ = pix_renderer = gtk_cell_renderer_pixbuf_new();

            gtk_tree_view_column_pack_start(col, pix_renderer, false);
            gtk_tree_view_column_set_attributes(col,
                                                pix_renderer,
                                                "pixbuf",
                                                file_browser->large_icons_
                                                    ? ptk::file_list::column::big_icon
                                                    : ptk::file_list::column::small_icon,
                                                nullptr);

            gtk_tree_view_column_set_expand(col, true);
            gtk_tree_view_column_set_sizing(col,
                                            GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
            gtk_tree_view_column_set_min_width(col, 150);
            gtk_tree_view_column_set_reorderable(col, false);
            // gtk_tree_view_set_activable_column(GTK_TREE_VIEW(list_view), col);
        }
        else
        {
            gtk_tree_view_column_set_reorderable(col, true);
            gtk_tree_view_column_set_visible(col, xset_get_b_panel_mode(p, column.xset_name, mode));
        }

        if (column.column == ptk::file_list::column::size ||
            column.column == ptk::file_list::column::bytes)
        { // right align text
            gtk_cell_renderer_set_alignment(renderer, 1, 0.5);
        }

        gtk_tree_view_column_pack_start(col, renderer, true);
        gtk_tree_view_column_set_attributes(col, renderer, "text", column.column, nullptr);
        gtk_tree_view_append_column(list_view, col);
        gtk_tree_view_column_set_title(col, column.title.data());
        gtk_tree_view_column_set_sort_indicator(col, true);
        gtk_tree_view_column_set_sort_column_id(col, magic_enum::enum_integer(column.column));
        gtk_tree_view_column_set_sort_order(col, GtkSortType::GTK_SORT_DESCENDING);
    }
}

static char*
folder_view_get_drop_dir(PtkFileBrowser* file_browser, i32 x, i32 y)
{
    GtkTreePath* tree_path = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeViewColumn* col;
    GtkTreeIter it;

    switch (file_browser->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            gtk_icon_view_convert_widget_to_bin_window_coords(
                GTK_ICON_VIEW(file_browser->folder_view_),
                x,
                y,
                &x,
                &y);
            tree_path = folder_view_get_tree_path_at_pos(file_browser, x, y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view_));
            break;
        case ptk::file_browser::view_mode::list_view:
            // if drag is in progress, get the dest row path
            gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(file_browser->folder_view_),
                                            &tree_path,
                                            nullptr);
            if (!tree_path)
            {
                // no drag in progress, get drop path
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(file_browser->folder_view_),
                                              x,
                                              y,
                                              nullptr,
                                              &col,
                                              nullptr,
                                              nullptr);
                if (col == gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view_), 0))
                {
                    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(file_browser->folder_view_),
                                                      x,
                                                      y,
                                                      &tree_path,
                                                      nullptr);
                    model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view_));
                }
            }
            else
            {
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view_));
            }
            break;
    }

    std::filesystem::path dest_path;
    if (tree_path)
    {
        if (!gtk_tree_model_get_iter(model, &it, tree_path))
        {
            return nullptr;
        }

        std::shared_ptr<vfs::file> file;
        gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
        if (file)
        {
            if (file->is_directory())
            {
                dest_path = file->path();
            }
            else /* Drop on a file, not directory */
            {
                /* Return current directory */
                dest_path = file_browser->cwd();
            }
        }
        gtk_tree_path_free(tree_path);
    }
    else
    {
        dest_path = file_browser->cwd();
    }
    return ztd::strdup(dest_path);
}

static void
on_folder_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y,
                                  GtkSelectionData* sel_data, u32 info, u32 time, void* user_data)
{
    (void)widget;
    (void)x;
    (void)y;
    (void)info;

    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(user_data);

    if ((gtk_selection_data_get_length(sel_data) >= 0) &&
        (gtk_selection_data_get_format(sel_data) == 8))
    {
        // (list view) use stored x and y because == 0 for update drag status
        //             when is last row (gtk2&3 bug?)
        // and because exo_icon_view has no get_drag_dest_row
        const char* dest_dir =
            folder_view_get_drop_dir(file_browser, file_browser->drag_x_, file_browser->drag_y_);
        // ztd::logger::info("FB DnD dest_dir = {}", dest_dir );
        if (dest_dir)
        {
            if (file_browser->pending_drag_status_)
            {
                // ztd::logger::debug("DnD DEFAULT");

                // We only want to update drag status, not really want to drop
                gdk_drag_status(drag_context, GdkDragAction::GDK_ACTION_DEFAULT, time);

                // DnD is still ongoing, do not continue
                file_browser->pending_drag_status_ = false;
                return;
            }

            char** list;
            char** puri;
            puri = list = gtk_selection_data_get_uris(sel_data);

            if (puri)
            {
                // We only want to update drag status, not really want to drop
                const auto dest_dir_stat = ztd::statx(dest_dir);

                const dev_t dest_dev = dest_dir_stat.dev();
                const ino_t dest_inode = dest_dir_stat.ino();
                if (file_browser->drag_source_dev_ == 0)
                {
                    file_browser->drag_source_dev_ = dest_dev;
                    for (; *puri; ++puri)
                    {
                        const std::filesystem::path file_path = Glib::filename_from_uri(*puri);

                        const auto file_path_stat = ztd::statx(file_path);
                        if (file_path_stat)
                        {
                            if (file_path_stat.dev() != dest_dev)
                            {
                                // different devices - store source device
                                file_browser->drag_source_dev_ = file_path_stat.dev();
                                break;
                            }
                            else if (file_browser->drag_source_inode_ == 0)
                            {
                                // same device - store source parent inode
                                const auto src_dir = file_path.parent_path();
                                const auto src_dir_stat = ztd::statx(src_dir);
                                if (src_dir_stat)
                                {
                                    file_browser->drag_source_inode_ = src_dir_stat.ino();
                                }
                            }
                        }
                    }
                }
                g_strfreev(list);

                vfs::file_task::type file_action;

                if (file_browser->drag_source_dev_ != dest_dev ||
                    file_browser->drag_source_inode_ == dest_inode)
                { // src and dest are on different devices or same dir
                    // ztd::logger::debug("DnD COPY");
                    gdk_drag_status(drag_context, GdkDragAction::GDK_ACTION_COPY, time);
                    file_action = vfs::file_task::type::copy;
                }
                else
                {
                    // ztd::logger::debug("DnD MOVE");
                    gdk_drag_status(drag_context, GdkDragAction::GDK_ACTION_MOVE, time);
                    file_action = vfs::file_task::type::move;
                }

                std::vector<std::filesystem::path> file_list;
                puri = list = gtk_selection_data_get_uris(sel_data);
                for (; *puri; ++puri)
                {
                    std::filesystem::path file_path;
                    if (**puri == '/')
                    {
                        file_path = *puri;
                    }
                    else
                    {
                        file_path = Glib::filename_from_uri(*puri);
                    }

                    file_list.emplace_back(file_path);
                }
                g_strfreev(list);

                if (!file_list.empty())
                {
                    // ztd::logger::info("DnD dest_dir = {}", dest_dir);

#if (GTK_MAJOR_VERSION == 4)
                    GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(file_browser)));
#elif (GTK_MAJOR_VERSION == 3)
                    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
#endif

                    PtkFileTask* ptask = ptk_file_task_new(file_action,
                                                           file_list,
                                                           dest_dir,
                                                           GTK_WINDOW(parent),
                                                           file_browser->task_view_);
                    ptk_file_task_run(ptask);
                }
                gtk_drag_finish(drag_context, true, false, time);
                return;
            }
        }
    }

    /* If we are only getting drag status, not finished. */
    if (file_browser->pending_drag_status_)
    {
        file_browser->pending_drag_status_ = false;
        return;
    }
    gtk_drag_finish(drag_context, false, false, time);
}

static void
on_folder_view_drag_data_get(GtkWidget* widget, GdkDragContext* drag_context,
                             GtkSelectionData* sel_data, u32 info, u32 time,
                             PtkFileBrowser* file_browser)
{
    (void)widget;
    (void)drag_context;
    (void)info;
    (void)time;
    GdkAtom type = gdk_atom_intern("text/uri-list", false);

    std::string uri_list;
    const auto selected_files = file_browser->selected_files();
    for (const auto& file : selected_files)
    {
        const std::string uri = Glib::filename_to_uri(file->path());
        uri_list.append(std::format("{}\n", uri));
    }

    gtk_selection_data_set(sel_data,
                           type,
                           8,
                           (const unsigned char*)uri_list.data(),
                           static_cast<i32>(uri_list.size()));
}

static void
on_folder_view_drag_begin(GtkWidget* widget, GdkDragContext* drag_context,
                          PtkFileBrowser* file_browser)
{
    (void)widget;
    gtk_drag_set_icon_default(drag_context);
    file_browser->is_drag_ = true;
}

static GtkTreePath*
folder_view_get_tree_path_at_pos(PtkFileBrowser* file_browser, i32 x, i32 y)
{
    GtkTreePath* tree_path;

    switch (file_browser->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            tree_path =
                exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(file_browser->folder_view_), x, y);
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(file_browser->folder_view_),
                                          x,
                                          y,
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr);
            break;
    }

    return tree_path;
}

static bool
on_folder_view_auto_scroll(GtkScrolledWindow* scroll)
{
    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);
    f64 vpos = gtk_adjustment_get_value(vadj);

    if (folder_view_auto_scroll_direction == GtkDirectionType::GTK_DIR_UP)
    {
        vpos -= gtk_adjustment_get_step_increment(vadj);
        if (vpos > gtk_adjustment_get_lower(vadj))
        {
            gtk_adjustment_set_value(vadj, vpos);
        }
        else
        {
            gtk_adjustment_set_value(vadj, gtk_adjustment_get_lower(vadj));
        }
    }
    else
    {
        vpos += gtk_adjustment_get_step_increment(vadj);
        if ((vpos + gtk_adjustment_get_page_size(vadj)) < gtk_adjustment_get_upper(vadj))
        {
            gtk_adjustment_set_value(vadj, vpos);
        }
        else
        {
            gtk_adjustment_set_value(
                vadj,
                (gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj)));
        }
    }
    return true;
}

static bool
on_folder_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y, u32 time,
                           PtkFileBrowser* file_browser)
{
    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(gtk_widget_get_parent(widget));

    // GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    if (y < 32)
    {
        /* Auto scroll up */
        if (!folder_view_auto_scroll_timer)
        {
            folder_view_auto_scroll_direction = GtkDirectionType::GTK_DIR_UP;
            folder_view_auto_scroll_timer =
                g_timeout_add(150, (GSourceFunc)on_folder_view_auto_scroll, scroll);
        }
    }
    else if (y > (allocation.height - 32))
    {
        if (!folder_view_auto_scroll_timer)
        {
            folder_view_auto_scroll_direction = GtkDirectionType::GTK_DIR_DOWN;
            folder_view_auto_scroll_timer =
                g_timeout_add(150, (GSourceFunc)on_folder_view_auto_scroll, scroll);
        }
    }
    else if (folder_view_auto_scroll_timer)
    {
        g_source_remove(folder_view_auto_scroll_timer);
        folder_view_auto_scroll_timer = 0;
    }

    GtkTreeViewColumn* col;
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;

    switch (file_browser->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            // store x and y because exo_icon_view has no get_drag_dest_row
            file_browser->drag_x_ = x;
            file_browser->drag_y_ = y;
            gtk_icon_view_convert_widget_to_bin_window_coords(GTK_ICON_VIEW(widget), x, y, &x, &y);
            tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), x, y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
            break;
        case ptk::file_browser::view_mode::list_view:
            // store x and y because == 0 for update drag status when is last row
            file_browser->drag_x_ = x;
            file_browser->drag_y_ = y;
            if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              x,
                                              y,
                                              nullptr,
                                              &col,
                                              nullptr,
                                              nullptr))
            {
                if (gtk_tree_view_get_column(GTK_TREE_VIEW(widget), 0) == col)
                {
                    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
                                                      x,
                                                      y,
                                                      &tree_path,
                                                      nullptr);
                    model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                }
            }
            break;
    }

    if (tree_path)
    {
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, tree_path))
        {
            std::shared_ptr<vfs::file> file;
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (!file || !file->is_directory())
            {
                gtk_tree_path_free(tree_path);
                tree_path = nullptr;
            }
        }
    }

    switch (file_browser->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(widget),
                                             tree_path,
                                             EXO_ICON_VIEW_DROP_INTO);
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_set_drag_dest_row(
                GTK_TREE_VIEW(widget),
                tree_path,
                GtkTreeViewDropPosition::GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
            break;
    }

    if (tree_path)
    {
        gtk_tree_path_free(tree_path);
    }

    /* FIXME: Creating a new target list everytime is very inefficient,
         but currently gtk_drag_dest_get_target_list always returns nullptr
         due to some strange reason, and cannot be used currently.  */
    GtkTargetList* target_list = gtk_target_list_new(drag_targets, G_N_ELEMENTS(drag_targets));
    GdkAtom target = gtk_drag_dest_find_target(widget, drag_context, target_list);
    gtk_target_list_unref(target_list);

    if (target == GDK_NONE)
    {
        gdk_drag_status(drag_context, (GdkDragAction)0, time);
    }
    else
    {
        GdkDragAction suggested_action;
        /* Only 'move' is available. The user force move action by pressing Shift key */
        if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) ==
            GdkDragAction::GDK_ACTION_MOVE)
        {
            suggested_action = GdkDragAction::GDK_ACTION_MOVE;
            /* Only 'copy' is available. The user force copy action by pressing Ctrl key */
        }
        else if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) ==
                 GdkDragAction::GDK_ACTION_COPY)
        {
            suggested_action = GdkDragAction::GDK_ACTION_COPY;
            /* Only 'link' is available. The user force link action by pressing Shift+Ctrl key */
        }
        else if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) ==
                 GdkDragAction::GDK_ACTION_LINK)
        {
            suggested_action = GdkDragAction::GDK_ACTION_LINK;
            /* Several different actions are available. We have to figure out a good default action.
             */
        }
        else
        {
            const i32 drag_action = xset_get_int(xset::name::drag_action, xset::var::x);

            switch (drag_action)
            {
                case 1:
                    suggested_action = GdkDragAction::GDK_ACTION_COPY;
                    break;
                case 2:
                    suggested_action = GdkDragAction::GDK_ACTION_MOVE;
                    break;
                case 3:
                    suggested_action = GdkDragAction::GDK_ACTION_LINK;
                    break;
                default:
                    // automatic
                    file_browser->pending_drag_status_ = true;
                    gtk_drag_get_data(widget, drag_context, target, time);
                    suggested_action = gdk_drag_context_get_selected_action(drag_context);
                    break;
            }
        }
        gdk_drag_status(drag_context, suggested_action, time);
    }
    return true;
}

static bool
on_folder_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context, u32 time,
                          PtkFileBrowser* file_browser)
{
    (void)widget;
    (void)drag_context;
    (void)time;

    file_browser->drag_source_dev_ = 0;
    file_browser->drag_source_inode_ = 0;

    if (folder_view_auto_scroll_timer)
    {
        g_source_remove(folder_view_auto_scroll_timer);
        folder_view_auto_scroll_timer = 0;
    }
    return true;
}

static bool
on_folder_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y, u32 time,
                         PtkFileBrowser* file_browser)
{
    (void)x;
    (void)y;
    (void)file_browser;
    GdkAtom target = gdk_atom_intern("text/uri-list", false);

    gtk_drag_get_data(widget, drag_context, target, time);
    return true;
}

static void
on_folder_view_drag_end(GtkWidget* widget, GdkDragContext* drag_context,
                        PtkFileBrowser* file_browser)
{
    (void)drag_context;
    if (folder_view_auto_scroll_timer)
    {
        g_source_remove(folder_view_auto_scroll_timer);
        folder_view_auto_scroll_timer = 0;
    }

    switch (file_browser->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(widget),
                                             nullptr,
                                             (ExoIconViewDropPosition)0);
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget),
                                            nullptr,
                                            (GtkTreeViewDropPosition)0);
            break;
    }
    file_browser->is_drag_ = false;
}

static bool
on_dir_tree_button_press(GtkWidget* view, GdkEvent* event, PtkFileBrowser* file_browser)
{
    file_browser->focus_me();

    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);

    if (type == GdkEventType::GDK_BUTTON_PRESS && button == 2) /* middle click */
    {
        /* left and right click handled in ptk-dir-tree-view.c
         * on_dir_tree_view_button_press() */

        f64 x, y;
        gdk_event_get_position(event, &x, &y);

        GtkTreePath* tree_path;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          x,
                                          y,
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr))
        {
            GtkTreeIter it;
            if (gtk_tree_model_get_iter(model, &it, tree_path))
            {
                std::shared_ptr<vfs::file> file;
                gtk_tree_model_get(model, &it, ptk::dir_tree::column::info, &file, -1);
                if (file)
                {
                    const auto check_file_path = ptk_dir_view_get_dir_path(model, &it);
                    if (check_file_path)
                    {
                        const auto& file_path = check_file_path.value();
                        file_browser->run_event<spacefm::signal::open_item>(
                            file_path,
                            ptk::open_action::new_tab);
                    }
                }
            }
            gtk_tree_path_free(tree_path);
        }
        return true;
    }
    return false;
}

static GtkWidget*
ptk_file_browser_create_dir_tree(PtkFileBrowser* file_browser)
{
    GtkWidget* dir_tree = ptk_dir_tree_view_new(file_browser, file_browser->show_hidden_files_);

    // clang-format off
    g_signal_connect(G_OBJECT(dir_tree), "row-activated", G_CALLBACK(on_dir_tree_row_activated), file_browser);
    g_signal_connect(G_OBJECT(dir_tree), "button-press-event", G_CALLBACK(on_dir_tree_button_press), file_browser);
    // clang-format on

    return dir_tree;
}

static i32
file_list_order_from_sort_order(ptk::file_browser::sort_order order)
{
    ptk::file_list::column col;
    switch (order)
    {
        case ptk::file_browser::sort_order::name:
            col = ptk::file_list::column::name;
            break;
        case ptk::file_browser::sort_order::size:
            col = ptk::file_list::column::size;
            break;
        case ptk::file_browser::sort_order::bytes:
            col = ptk::file_list::column::bytes;
            break;
        case ptk::file_browser::sort_order::type:
            col = ptk::file_list::column::type;
            break;
        case ptk::file_browser::sort_order::mime:
            col = ptk::file_list::column::mime;
            break;
        case ptk::file_browser::sort_order::perm:
            col = ptk::file_list::column::perm;
            break;
        case ptk::file_browser::sort_order::owner:
            col = ptk::file_list::column::owner;
            break;
        case ptk::file_browser::sort_order::group:
            col = ptk::file_list::column::group;
            break;
        case ptk::file_browser::sort_order::atime:
            col = ptk::file_list::column::atime;
            break;
        case ptk::file_browser::sort_order::btime:
            col = ptk::file_list::column::btime;
            break;
        case ptk::file_browser::sort_order::ctime:
            col = ptk::file_list::column::ctime;
            break;
        case ptk::file_browser::sort_order::mtime:
            col = ptk::file_list::column::mtime;
            break;
    }
    return magic_enum::enum_integer(col);
}

static void
ptk_file_browser_before_chdir(PtkFileBrowser* file_browser, const std::filesystem::path& path)
{
    (void)file_browser;
    (void)path;
}

static void
ptk_file_browser_after_chdir(PtkFileBrowser* file_browser)
{
    (void)file_browser;
}

static void
ptk_file_browser_content_change(PtkFileBrowser* file_browser)
{
    (void)file_browser;
}

static void
ptk_file_browser_sel_change(PtkFileBrowser* file_browser)
{
    (void)file_browser;
}

static void
ptk_file_browser_pane_mode_change(PtkFileBrowser* file_browser)
{
    (void)file_browser;
}

static void
ptk_file_browser_open_item(PtkFileBrowser* file_browser, const std::filesystem::path& path,
                           i32 action)
{
    (void)file_browser;
    (void)path;
    (void)action;
}

////////////////////////////////////////////////////////////////////////////

bool
ptk_file_browser_write_access(const std::filesystem::path& cwd)
{
    const auto status = std::filesystem::status(cwd);
    return ((status.permissions() & std::filesystem::perms::owner_write) !=
            std::filesystem::perms::none);
}

bool
ptk_file_browser_read_access(const std::filesystem::path& cwd)
{
    const auto status = std::filesystem::status(cwd);
    return ((status.permissions() & std::filesystem::perms::owner_read) !=
            std::filesystem::perms::none);
}

/**
* PtkFileBrowser
*/

bool
PtkFileBrowser::chdir(const std::filesystem::path& new_path,
                      const ptk::file_browser::chdir_mode mode) noexcept
{
    // ztd::logger::debug("PtkFileBrowser::chdir");

    // this->button_press_ = false;
    this->is_drag_ = false;
    this->menu_shown_ = false;
    if (this->view_mode_ == ptk::file_browser::view_mode::list_view || app_settings.single_click())
    {
        /* sfm 1.0.6 do not reset skip_release for Icon/Compact to prevent file
           under cursor being selected when entering dir with double-click.
           Reset is conditional here to avoid possible but unlikely unintended
           breakage elsewhere. */
        this->skip_release_ = false;
    }

    if (!std::filesystem::exists(new_path))
    {
        ztd::logger::error("Failed to chdir into nonexistent path '{}'", new_path.string());
        return false;
    }
    const auto path = std::filesystem::absolute(new_path);

    if (!std::filesystem::is_directory(path))
    {
        if (!this->inhibit_focus_)
        {
#if (GTK_MAJOR_VERSION == 4)
            GtkWidget* parent_win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(this)));
#elif (GTK_MAJOR_VERSION == 3)
            GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(this));
#endif

            ptk_show_error(GTK_WINDOW(parent_win),
                           "Error",
                           std::format("Directory does not exist\n\n{}", path.string()));
        }
        return false;
    }

    if (!have_x_access(path))
    {
        if (!this->inhibit_focus_)
        {
#if (GTK_MAJOR_VERSION == 4)
            GtkWidget* parent_win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(this)));
#elif (GTK_MAJOR_VERSION == 3)
            GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(this));
#endif

            ptk_show_error(
                GTK_WINDOW(parent_win),
                "Error",
                std::format("Unable to access {}\n\n{}", path.string(), std::strerror(errno)));
        }
        return false;
    }

    // bool cancel;
    // this->run_event<spacefm::signal::chdir_before>();
    // if (cancel)
    //     return false;

    this->update_selection_history();

    switch (mode)
    {
        case ptk::file_browser::chdir_mode::normal:
            if (!std::filesystem::equivalent(this->navigation_history->path(), path))
            {
                this->navigation_history->new_forward(path);
            }
            break;
        case ptk::file_browser::chdir_mode::back:
            this->navigation_history->go_back();
            break;
        case ptk::file_browser::chdir_mode::forward:
            this->navigation_history->go_forward();
            break;
    }

    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_set_model(EXO_ICON_VIEW(this->folder_view_), nullptr);
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_set_model(GTK_TREE_VIEW(this->folder_view_), nullptr);
            break;
    }

    // load new dir

    this->signal_file_listed.disconnect();
    this->busy_ = true;
    this->dir_ = vfs::dir::create(path);

    this->run_event<spacefm::signal::chdir_begin>();

    if (this->dir_->is_file_listed())
    {
        this->on_dir_file_listed(false);
        this->busy_ = false;
    }
    else
    {
        this->busy_ = true;
    }

    this->signal_file_listed = this->dir_->add_event<spacefm::signal::file_listed>(
        std::bind(&PtkFileBrowser::on_dir_file_listed, this, std::placeholders::_1));

    this->update_tab_label();

    const auto& cwd = this->cwd();
    if (!this->inhibit_focus_)
    {
#if (GTK_MAJOR_VERSION == 4)
        gtk_editable_set_text(GTK_EDITABLE(this->path_bar_), cwd.c_str());
#elif (GTK_MAJOR_VERSION == 3)
        gtk_entry_set_text(GTK_ENTRY(this->path_bar_), cwd.c_str());
#endif
    }

    this->enable_toolbar();

    return true;
}

const std::filesystem::path&
PtkFileBrowser::cwd() const noexcept
{
    return this->navigation_history->path();
}

void
PtkFileBrowser::canon(const std::filesystem::path& path) noexcept
{
    const auto& cwd = this->cwd();
    const auto canon = std::filesystem::canonical(path);
    if (std::filesystem::equivalent(canon, cwd) || std::filesystem::equivalent(canon, path))
    {
        return;
    }

    if (std::filesystem::is_directory(canon))
    {
        // open dir
        this->chdir(canon);
        gtk_widget_grab_focus(GTK_WIDGET(this->folder_view_));
    }
    else if (std::filesystem::exists(canon))
    {
        // open dir and select file
        const auto dir_path = canon.parent_path();
        if (!std::filesystem::equivalent(dir_path, cwd))
        {
            this->chdir(dir_path);
        }
        else
        {
            this->select_file(canon);
        }
        gtk_widget_grab_focus(GTK_WIDGET(this->folder_view_));
    }
}

u64
PtkFileBrowser::get_n_all_files() const noexcept
{
    return this->dir_ ? this->dir_->files().size() : 0;
}

u64
PtkFileBrowser::get_n_visible_files() const noexcept
{
    return this->file_list_ ? gtk_tree_model_iter_n_children(this->file_list_, nullptr) : 0;
}

u64
PtkFileBrowser::get_n_sel(u64* sel_size, u64* sel_disk_size) const noexcept
{
    if (sel_size)
    {
        *sel_size = this->sel_size_;
    }
    if (sel_disk_size)
    {
        *sel_disk_size = this->sel_disk_size_;
    }
    return this->n_sel_files_;
}

void
PtkFileBrowser::go_home() noexcept
{
    this->focus_folder_view();
    this->chdir(vfs::user_dirs->home_dir());
}

void
PtkFileBrowser::go_default() noexcept
{
    this->focus_folder_view();
    const auto default_path = xset_get_s(xset::name::go_set_default);
    if (default_path)
    {
        this->chdir(default_path.value());
    }
    else
    {
        this->chdir(vfs::user_dirs->home_dir());
    }
}

void
PtkFileBrowser::go_tab(tab_t tab) noexcept
{
    // ztd::logger::info("ptk_file_browser_go_tab fb={}", fmt::ptr(this));

    switch (tab)
    {
        case tab_control_code_prev:
            // prev
            if (gtk_notebook_get_current_page(this->notebook_) == 0)
            {
                gtk_notebook_set_current_page(this->notebook_,
                                              gtk_notebook_get_n_pages(this->notebook_) - 1);
            }
            else
            {
                gtk_notebook_prev_page(this->notebook_);
            }
            break;
        case tab_control_code_next:
            // next
            if (gtk_notebook_get_current_page(this->notebook_) + 1 ==
                gtk_notebook_get_n_pages(this->notebook_))
            {
                gtk_notebook_set_current_page(this->notebook_, 0);
            }
            else
            {
                gtk_notebook_next_page(this->notebook_);
            }
            break;
        case tab_control_code_close:
            // close
            this->close_tab();
            break;
        case tab_control_code_restore:
            // restore
            this->restore_tab();
            break;
        default:
            // set tab
            if (tab <= gtk_notebook_get_n_pages(this->notebook_) && tab > 0)
            {
                gtk_notebook_set_current_page(this->notebook_, tab - 1);
            }
            break;
    }
}

void
PtkFileBrowser::go_back() noexcept
{
    this->focus_folder_view();
    if (this->navigation_history->has_back())
    {
        const auto mode = ptk::file_browser::chdir_mode::back;
        this->chdir(this->navigation_history->path(mode), mode);
    }
}

void
PtkFileBrowser::go_forward() noexcept
{
    this->focus_folder_view();
    if (this->navigation_history->has_forward())
    {
        const auto mode = ptk::file_browser::chdir_mode::forward;
        this->chdir(this->navigation_history->path(mode), mode);
    }
}

void
PtkFileBrowser::go_up() noexcept
{
    this->focus_folder_view();
    const auto parent_dir = this->cwd().parent_path();
    if (!std::filesystem::equivalent(parent_dir, this->cwd()))
    {
        this->chdir(parent_dir);
    }
}

void
PtkFileBrowser::refresh(const bool update_selected_files) noexcept
{
    if (this->busy_)
    {
        // a dir is already loading
        return;
    }

    if (!std::filesystem::is_directory(this->cwd()))
    {
        this->close_tab();
        return;
    }

    if (update_selected_files)
    {
        this->update_selection_history();
    }

    // destroy file list and create new one
    this->update_model();

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
    memory_trim();

    // begin reload dir
    this->busy_ = true;

    this->run_event<spacefm::signal::chdir_begin>();

    if (this->dir_->is_file_listed())
    {
        this->on_dir_file_listed(false);
        this->busy_ = false;
    }
    else
    {
        this->busy_ = true;
    }
}

void
PtkFileBrowser::show_hidden_files(bool show) noexcept
{
    if (this->show_hidden_files_ == show)
    {
        return;
    }
    this->show_hidden_files_ = show;

    if (this->file_list_)
    {
        this->update_model();

        this->run_event<spacefm::signal::change_sel>();
    }

    if (this->side_dir)
    {
        ptk_dir_tree_view_show_hidden_files(GTK_TREE_VIEW(this->side_dir),
                                            this->show_hidden_files_);
    }

    this->update_toolbar_widgets(xset::tool::show_hidden);
}

void
PtkFileBrowser::set_single_click(bool single_click) noexcept
{
    if (single_click == this->single_click_)
    {
        return;
    }

    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            gtk_icon_view_set_activate_on_single_click(GTK_ICON_VIEW(this->folder_view_),
                                                       single_click);
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(this->folder_view_),
                                                       single_click);
            break;
    }

    this->single_click_ = single_click;
}

void
PtkFileBrowser::new_tab() noexcept
{
    this->focus_folder_view();

    std::filesystem::path dir_path;
    const auto default_path = xset_get_s(xset::name::go_set_default);
    if (default_path)
    {
        dir_path = default_path.value();
    }
    else
    {
        dir_path = vfs::user_dirs->home_dir();
    }

    if (!std::filesystem::is_directory(dir_path))
    {
        this->run_event<spacefm::signal::open_item>("/", ptk::open_action::new_tab);
    }
    else
    {
        this->run_event<spacefm::signal::open_item>(dir_path, ptk::open_action::new_tab);
    }
}

void
PtkFileBrowser::new_tab_here() noexcept
{
    this->focus_folder_view();

    auto dir_path = this->cwd();
    if (!std::filesystem::is_directory(dir_path))
    {
        const auto default_path = xset_get_s(xset::name::go_set_default);
        if (default_path)
        {
            dir_path = default_path.value();
        }
        else
        {
            dir_path = vfs::user_dirs->home_dir();
        }
    }
    if (!std::filesystem::is_directory(dir_path))
    {
        this->run_event<spacefm::signal::open_item>("/", ptk::open_action::new_tab);
    }
    else
    {
        this->run_event<spacefm::signal::open_item>(dir_path, ptk::open_action::new_tab);
    }
}

void
PtkFileBrowser::close_tab() noexcept
{
    closed_tabs_restore[this->panel_].emplace_back(this->cwd());
    // ztd::logger::info("close_tab() fb={}, path={}", fmt::ptr(this), closed_tabs_restore[this->panel_].back());

    GtkNotebook* notebook =
        GTK_NOTEBOOK(gtk_widget_get_ancestor(GTK_WIDGET(this), GTK_TYPE_NOTEBOOK));

    MainWindow* main_window = this->main_window_;
    main_window->curpanel = this->panel_;
    main_window->notebook = main_window->get_panel_notebook(main_window->curpanel);

    // save solumns and slider positions of tab to be closed
    this->slider_release(nullptr);
    this->save_column_widths(GTK_TREE_VIEW(this->folder_view_));

    // remove page can also be used to destroy - same result
    // gtk_notebook_remove_page(notebook, gtk_notebook_get_current_page(notebook));
    gtk_widget_destroy(GTK_WIDGET(this));

    if (!app_settings.always_show_tabs())
    {
        if (gtk_notebook_get_n_pages(notebook) == 1)
        {
            gtk_notebook_set_show_tabs(notebook, false);
        }
    }

    if (gtk_notebook_get_n_pages(notebook) == 0)
    {
        std::filesystem::path path;
        const auto default_path = xset_get_s(xset::name::go_set_default);
        if (default_path)
        {
            path = default_path.value();
        }
        else
        {
            path = vfs::user_dirs->home_dir();
        }
        main_window->new_tab(path);
        PtkFileBrowser* a_browser =
            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, 0));
        a_browser->update_views();
        main_window->set_window_title(a_browser);
        if (xset_get_b(xset::name::main_save_tabs))
        {
            autosave_request_add();
        }
        return;
    }

    // update view of new current tab
    const tab_t cur_tabx = gtk_notebook_get_current_page(main_window->notebook);
    if (cur_tabx != -1)
    {
        PtkFileBrowser* a_browser =
            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, cur_tabx));
        a_browser->update_views();
        main_window->update_status_bar(a_browser);
        // g_idle_add((GSourceFunc)delayed_focus, a_browser->folder_view());
    }

    main_window->set_window_title(this);
    if (xset_get_b(xset::name::main_save_tabs))
    {
        autosave_request_add();
    }
}

void
PtkFileBrowser::restore_tab() noexcept
{
    if (closed_tabs_restore[this->panel_].empty())
    {
        ztd::logger::info("No tabs to restore for panel {}", this->panel_);
        return;
    }

    const auto file_path = closed_tabs_restore[this->panel_].back();
    closed_tabs_restore[this->panel_].pop_back();
    // ztd::logger::info("restore_tab() fb={}, panel={} path={}", fmt::ptr(this), this->panel_, file_path);

    MainWindow* main_window = this->main_window_;

    main_window->new_tab(file_path);

    main_window->set_window_title(this);
    if (xset_get_b(xset::name::main_save_tabs))
    {
        autosave_request_add();
    }
}

void
PtkFileBrowser::open_in_tab(const std::filesystem::path& file_path, const tab_t tab) const noexcept
{
    const tab_t cur_page = gtk_notebook_get_current_page(this->notebook_);
    const tab_t pages = gtk_notebook_get_n_pages(this->notebook_);

    tab_t page_x;
    switch (tab)
    {
        case tab_control_code_prev:
            // prev
            page_x = cur_page - 1;
            break;
        case tab_control_code_next:
            // next
            page_x = cur_page + 1;
            break;
        default:
            page_x = tab - 1;
            break;
    }

    if (page_x > -1 && page_x < pages && page_x != cur_page)
    {
        PtkFileBrowser* file_browser =
            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(this->notebook_, page_x));

        file_browser->chdir(file_path);
    }
}

void
PtkFileBrowser::set_default_folder() const noexcept
{
    xset_set(xset::name::go_set_default, xset::var::s, this->cwd().string());
}

const std::vector<std::shared_ptr<vfs::file>>
PtkFileBrowser::selected_files() noexcept
{
    GtkTreeModel* model = nullptr;
    std::vector<std::shared_ptr<vfs::file>> file_list;
    GList* selected_files = this->selected_items(&model);
    if (!selected_files)
    {
        return file_list;
    }

    file_list.reserve(g_list_length(selected_files));
    for (GList* sel = selected_files; sel; sel = g_list_next(sel))
    {
        GtkTreeIter it;
        std::shared_ptr<vfs::file> file;
        gtk_tree_model_get_iter(model, &it, (GtkTreePath*)sel->data);
        gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
        file_list.emplace_back(file);
    }
    g_list_foreach(selected_files, (GFunc)gtk_tree_path_free, nullptr);
    g_list_free(selected_files);

    return file_list;
}

void
PtkFileBrowser::open_selected_files() noexcept
{
    this->open_selected_files_with_app();
}

void
PtkFileBrowser::open_selected_files_with_app(const std::string_view app_desktop) noexcept
{
    const auto selected_files = this->selected_files();

    ptk_open_files_with_app(this->cwd(), selected_files, app_desktop, this, false, false);
}

void
PtkFileBrowser::rename_selected_files(
    const std::span<const std::shared_ptr<vfs::file>> selected_files,
    const std::filesystem::path& cwd) noexcept
{
    if (selected_files.empty())
    {
        return;
    }

    gtk_widget_grab_focus(this->folder_view_);

    for (const auto& file : selected_files)
    {
        if (!ptk_rename_file(this,
                             cwd.c_str(),
                             file,
                             nullptr,
                             false,
                             ptk::rename_mode::rename,
                             nullptr))
        {
            break;
        }
    }
}

void
PtkFileBrowser::hide_selected(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                              const std::filesystem::path& cwd) noexcept
{
    (void)cwd;

    const auto response = ptk_show_message(
        GTK_WINDOW(this),
        GtkMessageType::GTK_MESSAGE_INFO,
        "Hide File",
        GtkButtonsType::GTK_BUTTONS_OK_CANCEL,
        "The names of the selected files will be added to the '.hidden' file located in this "
        "directory, which will hide them from view in SpaceFM.  You may need to refresh the "
        "view or restart SpaceFM for the files to disappear.\n\nTo unhide a file, open the "
        ".hidden file in your text editor, remove the name of the file, and refresh.");

    if (response != GtkResponseType::GTK_RESPONSE_OK)
    {
        return;
    }

#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* parent_win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(this)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(this));
#endif

    if (selected_files.empty())
    {
        ptk_show_error(GTK_WINDOW(parent_win), "Error", "No files are selected");
        return;
    }

    for (const auto& file : selected_files)
    {
        if (!this->dir_->add_hidden(file))
        {
            ptk_show_error(GTK_WINDOW(parent_win), "Error", "Error hiding files");
        }
    }

    // refresh from here causes a segfault occasionally
    // file_browser->refresh();
}

void
PtkFileBrowser::copycmd(const std::span<const std::shared_ptr<vfs::file>> sel_files,
                        const std::filesystem::path& cwd, xset::name setname) noexcept
{
    std::optional<std::filesystem::path> copy_dest;
    std::optional<std::filesystem::path> move_dest;

    if (setname == xset::name::copy_tab_prev)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_control_code_prev);
    }
    else if (setname == xset::name::copy_tab_next)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_control_code_next);
    }
    else if (setname == xset::name::copy_tab_1)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_1);
    }
    else if (setname == xset::name::copy_tab_2)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_2);
    }
    else if (setname == xset::name::copy_tab_3)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_3);
    }
    else if (setname == xset::name::copy_tab_4)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_4);
    }
    else if (setname == xset::name::copy_tab_5)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_5);
    }
    else if (setname == xset::name::copy_tab_6)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_6);
    }
    else if (setname == xset::name::copy_tab_7)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_7);
    }
    else if (setname == xset::name::copy_tab_8)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_8);
    }
    else if (setname == xset::name::copy_tab_9)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_9);
    }
    else if (setname == xset::name::copy_tab_10)
    {
        copy_dest = main_window_get_tab_cwd(this, tab_10);
    }
    else if (setname == xset::name::copy_panel_prev)
    {
        copy_dest = main_window_get_panel_cwd(this, panel_control_code_prev);
    }
    else if (setname == xset::name::copy_panel_next)
    {
        copy_dest = main_window_get_panel_cwd(this, panel_control_code_next);
    }
    else if (setname == xset::name::copy_panel_1)
    {
        copy_dest = main_window_get_panel_cwd(this, panel_1);
    }
    else if (setname == xset::name::copy_panel_2)
    {
        copy_dest = main_window_get_panel_cwd(this, panel_2);
    }
    else if (setname == xset::name::copy_panel_3)
    {
        copy_dest = main_window_get_panel_cwd(this, panel_3);
    }
    else if (setname == xset::name::copy_panel_4)
    {
        copy_dest = main_window_get_panel_cwd(this, panel_4);
    }
    else if (setname == xset::name::copy_loc_last)
    {
        const xset_t set = xset_get(xset::name::copy_loc_last);
        copy_dest = set->desc.value();
    }
    else if (setname == xset::name::move_tab_prev)
    {
        move_dest = main_window_get_tab_cwd(this, tab_control_code_prev);
    }
    else if (setname == xset::name::move_tab_next)
    {
        move_dest = main_window_get_tab_cwd(this, tab_control_code_next);
    }
    else if (setname == xset::name::move_tab_1)
    {
        move_dest = main_window_get_tab_cwd(this, tab_1);
    }
    else if (setname == xset::name::move_tab_2)
    {
        move_dest = main_window_get_tab_cwd(this, tab_2);
    }
    else if (setname == xset::name::move_tab_3)
    {
        move_dest = main_window_get_tab_cwd(this, tab_3);
    }
    else if (setname == xset::name::move_tab_4)
    {
        move_dest = main_window_get_tab_cwd(this, tab_4);
    }
    else if (setname == xset::name::move_tab_5)
    {
        move_dest = main_window_get_tab_cwd(this, tab_5);
    }
    else if (setname == xset::name::move_tab_6)
    {
        move_dest = main_window_get_tab_cwd(this, tab_6);
    }
    else if (setname == xset::name::move_tab_7)
    {
        move_dest = main_window_get_tab_cwd(this, tab_7);
    }
    else if (setname == xset::name::move_tab_8)
    {
        move_dest = main_window_get_tab_cwd(this, tab_8);
    }
    else if (setname == xset::name::move_tab_9)
    {
        move_dest = main_window_get_tab_cwd(this, tab_9);
    }
    else if (setname == xset::name::move_tab_10)
    {
        move_dest = main_window_get_tab_cwd(this, tab_10);
    }
    else if (setname == xset::name::move_panel_prev)
    {
        move_dest = main_window_get_panel_cwd(this, panel_control_code_prev);
    }
    else if (setname == xset::name::move_panel_next)
    {
        move_dest = main_window_get_panel_cwd(this, panel_control_code_next);
    }
    else if (setname == xset::name::move_panel_1)
    {
        move_dest = main_window_get_panel_cwd(this, panel_1);
    }
    else if (setname == xset::name::move_panel_2)
    {
        move_dest = main_window_get_panel_cwd(this, panel_2);
    }
    else if (setname == xset::name::move_panel_3)
    {
        move_dest = main_window_get_panel_cwd(this, panel_3);
    }
    else if (setname == xset::name::move_panel_4)
    {
        move_dest = main_window_get_panel_cwd(this, panel_4);
    }
    else if (setname == xset::name::move_loc_last)
    {
        const xset_t set = xset_get(xset::name::copy_loc_last);
        move_dest = set->desc.value();
    }

    if ((setname == xset::name::copy_loc || setname == xset::name::copy_loc_last ||
         setname == xset::name::move_loc || setname == xset::name::move_loc_last) &&
        !copy_dest && !move_dest)
    {
        std::filesystem::path folder;
        xset_t set;

        set = xset_get(xset::name::copy_loc_last);
        if (set->desc)
        {
            folder = set->desc.value();
        }
        else
        {
            folder = cwd;
        }
        const auto path =
            xset_file_dialog(GTK_WIDGET(this),
                             GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                             "Choose Location",
                             folder,
                             std::nullopt);
        if (path && std::filesystem::is_directory(path.value()))
        {
            if (setname == xset::name::copy_loc || setname == xset::name::copy_loc_last)
            {
                copy_dest = path;
            }
            else
            {
                move_dest = path;
            }
            set = xset_get(xset::name::copy_loc_last);
            xset_set_var(set, xset::var::desc, path.value().string());
        }
        else
        {
            return;
        }
    }

    if (copy_dest || move_dest)
    {
        vfs::file_task::type file_action;
        std::optional<std::filesystem::path> dest_dir;

        if (copy_dest)
        {
            file_action = vfs::file_task::type::copy;
            dest_dir = copy_dest;
        }
        else
        {
            file_action = vfs::file_task::type::move;
            dest_dir = move_dest;
        }

        if (std::filesystem::equivalent(dest_dir.value(), cwd))
        {
            ptk_show_message(GTK_WINDOW(this),
                             GtkMessageType::GTK_MESSAGE_ERROR,
                             "Invalid Destination",
                             GtkButtonsType::GTK_BUTTONS_OK,
                             "Destination same as source");
            return;
        }

        // rebuild sel_files with full paths
        std::vector<std::filesystem::path> file_list;
        file_list.reserve(sel_files.size());
        for (const auto& file : sel_files)
        {
            file_list.emplace_back(file->path());
        }

#if (GTK_MAJOR_VERSION == 4)
        GtkWidget* parent_win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(this)));
#elif (GTK_MAJOR_VERSION == 3)
        GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(this));
#endif

        // task
        PtkFileTask* ptask = ptk_file_task_new(file_action,
                                               file_list,
                                               dest_dir.value(),
                                               GTK_WINDOW(parent_win),
                                               this->task_view_);
        ptk_file_task_run(ptask);
    }
    else
    {
        ptk_show_message(GTK_WINDOW(this),
                         GtkMessageType::GTK_MESSAGE_ERROR,
                         "Invalid Destination",
                         GtkButtonsType::GTK_BUTTONS_OK,
                         "Invalid destination");
    }
}

void
PtkFileBrowser::set_sort_order(ptk::file_browser::sort_order order) noexcept
{
    if (order == this->sort_order_)
    {
        return;
    }

    this->sort_order_ = order;
    const i32 col = file_list_order_from_sort_order(order);

    if (this->file_list_)
    {
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(this->file_list_),
                                             col,
                                             this->sort_type_);
    }
}

void
PtkFileBrowser::set_sort_type(GtkSortType order) noexcept
{
    if (order != this->sort_type_)
    {
        this->sort_type_ = order;
        if (this->file_list_)
        {
            i32 col;
            GtkSortType old_order;
            gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(this->file_list_),
                                                 &col,
                                                 &old_order);
            gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(this->file_list_), col, order);
        }
    }
}

void
PtkFileBrowser::set_sort_extra(xset::name setname) const noexcept
{
    const xset_t set = xset_get(setname);

    if (!set->name.starts_with("sortx_"))
    {
        return;
    }

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(this->file_list_);
    if (!list)
    {
        return;
    }

    if (set->xset_name == xset::name::sortx_natural)
    {
        list->sort_natural = set->b == xset::b::xtrue;
        xset_set_b_panel(this->panel_, xset::panel::sort_extra, list->sort_natural);
    }
    else if (set->xset_name == xset::name::sortx_case)
    {
        list->sort_case = set->b == xset::b::xtrue;
        xset_set_panel(this->panel_, xset::panel::sort_extra, xset::var::x, std::to_string(set->b));
    }
    else if (set->xset_name == xset::name::sortx_directories)
    {
        list->sort_dir = ptk::file_list::sort_dir::first;
        xset_set_panel(this->panel_,
                       xset::panel::sort_extra,
                       xset::var::y,
                       std::to_string(magic_enum::enum_integer(ptk::file_list::sort_dir::first)));
    }
    else if (set->xset_name == xset::name::sortx_files)
    {
        list->sort_dir = ptk::file_list::sort_dir::last;
        xset_set_panel(this->panel_,
                       xset::panel::sort_extra,
                       xset::var::y,
                       std::to_string(magic_enum::enum_integer(ptk::file_list::sort_dir::last)));
    }
    else if (set->xset_name == xset::name::sortx_mix)
    {
        list->sort_dir = ptk::file_list::sort_dir::mixed;
        xset_set_panel(this->panel_,
                       xset::panel::sort_extra,
                       xset::var::y,
                       std::to_string(magic_enum::enum_integer(ptk::file_list::sort_dir::mixed)));
    }
    else if (set->xset_name == xset::name::sortx_hidfirst)
    {
        list->sort_hidden_first = set->b == xset::b::xtrue;
        xset_set_panel(this->panel_, xset::panel::sort_extra, xset::var::z, std::to_string(set->b));
    }
    else if (set->xset_name == xset::name::sortx_hidlast)
    {
        list->sort_hidden_first = set->b != xset::b::xtrue;
        xset_set_panel(this->panel_,
                       xset::panel::sort_extra,
                       xset::var::z,
                       std::to_string(set->b == xset::b::xtrue ? xset::b::xfalse : xset::b::xtrue));
    }
    ptk_file_list_sort(list);
}

void
PtkFileBrowser::read_sort_extra() const noexcept
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(this->file_list_);
    if (!list)
    {
        return;
    }

    list->sort_natural = xset_get_b_panel(this->panel_, xset::panel::sort_extra);
    list->sort_case =
        xset_get_int_panel(this->panel_, xset::panel::sort_extra, xset::var::x) == xset::b::xtrue;
    list->sort_dir = ptk::file_list::sort_dir(
        xset_get_int_panel(this->panel_, xset::panel::sort_extra, xset::var::y));
    list->sort_hidden_first =
        xset_get_int_panel(this->panel_, xset::panel::sort_extra, xset::var::z) == xset::b::xtrue;
}

void
PtkFileBrowser::paste_link() const noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* parent_win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(this)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(this));
#endif

    ptk_clipboard_paste_links(GTK_WINDOW(parent_win),
                              this->cwd(),
                              GTK_TREE_VIEW(this->task_view_),
                              nullptr,
                              nullptr);
}

void
PtkFileBrowser::paste_target() const noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* parent_win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(this)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(this));
#endif

    ptk_clipboard_paste_targets(GTK_WINDOW(parent_win),
                                this->cwd(),
                                GTK_TREE_VIEW(this->task_view_),
                                nullptr,
                                nullptr);
}

void
PtkFileBrowser::select_all() const noexcept
{
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_select_all(EXO_ICON_VIEW(this->folder_view_));
            break;
        case ptk::file_browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            gtk_tree_selection_select_all(selection);
            break;
    }
}

void
PtkFileBrowser::unselect_all() const noexcept
{
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_unselect_all(EXO_ICON_VIEW(this->folder_view_));
            break;
        case ptk::file_browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            gtk_tree_selection_unselect_all(selection);
            break;
    }
}

void
PtkFileBrowser::select_last() noexcept
{
    // ztd::logger::debug("select_last");
    const auto& cwd = this->cwd();
    if (this->selection_history->selection_history.contains(cwd))
    {
        this->select_files(this->selection_history->selection_history.at(cwd));
    }
    this->selection_history->selection_history.erase(cwd);
}

static bool
on_input_keypress(GtkWidget* widget, GdkEvent* event, GtkWidget* dlg) noexcept
{
    (void)widget;
    const auto keyval = gdk_key_event_get_keyval(event);
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        gtk_dialog_response(GTK_DIALOG(dlg), GtkResponseType::GTK_RESPONSE_OK);
        return true;
    }
    return false;
}

// stolen from the fnmatch man page
#define FNMATCH_HELP                                                                               \
    "'?(pattern-list)'\n"                                                                          \
    "The pattern matches if zero or one occurrences of any of the patterns in the pattern-list "   \
    "match the input string.\n\n"                                                                  \
    "'*(pattern-list)'\n"                                                                          \
    "The pattern matches if zero or more occurrences of any of the patterns in the pattern-list "  \
    "match the input string.\n\n"                                                                  \
    "'+(pattern-list)'\n"                                                                          \
    "The pattern matches if one or more occurrences of any of the patterns in the pattern-list "   \
    "match the input string.\n\n"                                                                  \
    "'@(pattern-list)'\n"                                                                          \
    "The pattern matches if exactly one occurrence of any of the patterns in the pattern-list "    \
    "match the input string.\n\n"                                                                  \
    "'!(pattern-list)'\n"                                                                          \
    "The pattern matches if the input string cannot be matched with any of the patterns in the "   \
    "pattern-list.\n"

static const std::tuple<bool, std::string>
select_pattern_dialog(GtkWidget* parent, const std::string_view default_pattern) noexcept
{
    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               GtkDialogFlags::GTK_DIALOG_MODAL,
                                               GtkMessageType::GTK_MESSAGE_QUESTION,
                                               GtkButtonsType::GTK_BUTTONS_OK_CANCEL,
                                               "Enter a pattern to select files and directories");

    gtk_window_set_title(GTK_WINDOW(dialog), "Select By Pattern");

    gtk_widget_set_size_request(GTK_WIDGET(dialog), 600, 400);
    gtk_window_set_resizable(GTK_WINDOW(dialog), true);

    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), FNMATCH_HELP);

    GtkBox* content_area = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));

    GtkBox* vbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 10));

    gtk_widget_set_margin_start(GTK_WIDGET(vbox), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(vbox), 5);
    gtk_widget_set_margin_top(GTK_WIDGET(vbox), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(vbox), 5);

#if (GTK_MAJOR_VERSION == 4)
    gtk_box_prepend(GTK_BOX(content_area), GTK_WIDGET(vbox));
#elif (GTK_MAJOR_VERSION == 3)
    gtk_container_add(GTK_CONTAINER(content_area), GTK_WIDGET(vbox));
#endif

    GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
#if (GTK_MAJOR_VERSION == 4)
    gtk_editable_set_text(GTK_EDITABLE(entry), default_pattern.data());
#elif (GTK_MAJOR_VERSION == 3)
    gtk_entry_set_text(GTK_ENTRY(entry), default_pattern.data());
#endif
    gtk_editable_set_editable(GTK_EDITABLE(entry), true);

    gtk_widget_set_margin_start(GTK_WIDGET(entry), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(entry), 5);
    gtk_widget_set_margin_top(GTK_WIDGET(entry), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(entry), 5);

    gtk_box_pack_start(vbox, GTK_WIDGET(entry), true, true, 4);

    g_signal_connect(G_OBJECT(entry), "key-press-event", G_CALLBACK(on_input_keypress), dialog);

    // show
    gtk_widget_show_all(GTK_WIDGET(dialog));

    const auto response = gtk_dialog_run(GTK_DIALOG(dialog));

    std::string pattern;
    bool ret = false;
    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
#if (GTK_MAJOR_VERSION == 4)
        pattern = gtk_editable_get_text(GTK_EDITABLE(entry));
#elif (GTK_MAJOR_VERSION == 3)
        pattern = gtk_entry_get_text(GTK_ENTRY(entry));
#endif

        ret = true;
    }

    gtk_widget_destroy(dialog);

    return std::make_tuple(ret, pattern);
}

void
PtkFileBrowser::select_pattern(const std::string_view search_key) noexcept
{
    std::string_view key;
    if (search_key.empty())
    {
        // get pattern from user (store in ob1 so it is not saved)
        const xset_t set = xset_get(xset::name::select_patt);
        const auto [response, answer] =
            select_pattern_dialog(GTK_WIDGET(this->main_window_), set->ob1 ? set->ob1 : "");

        set->ob1 = ztd::strdup(answer);
        if (!response || !set->ob1)
        {
            return;
        }
        key = set->ob1;
    }
    else
    {
        key = search_key;
    }

    GtkTreeModel* model;
    GtkTreeIter it;
    GtkTreeSelection* selection = nullptr;

    // get model, treesel, and stop signals
    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
            break;
        case ptk::file_browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(this->folder_view_));
            break;
    }

    // test rows
    bool first_select = true;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            // get file
            std::shared_ptr<vfs::file> file;
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (!file)
            {
                continue;
            }

            // test name
            const auto name = file->name();
            const bool select = ztd::fnmatch(key, name);

            // do selection and scroll to first selected
            GtkTreePath* path =
                gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST_REINTERPRET(this->file_list_)),
                                        &it);

            switch (this->view_mode_)
            {
                case ptk::file_browser::view_mode::icon_view:
                case ptk::file_browser::view_mode::compact_view:
                    // select
                    if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(this->folder_view_), path))
                    {
                        if (!select)
                        {
                            exo_icon_view_unselect_path(EXO_ICON_VIEW(this->folder_view_), path);
                        }
                    }
                    else if (select)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(this->folder_view_), path);
                    }

                    // scroll to first and set cursor
                    if (first_select && select)
                    {
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(this->folder_view_),
                                                 path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(this->folder_view_),
                                                     path,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                case ptk::file_browser::view_mode::list_view:
                    // select
                    if (gtk_tree_selection_path_is_selected(selection, path))
                    {
                        if (!select)
                        {
                            gtk_tree_selection_unselect_path(selection, path);
                        }
                    }
                    else if (select)
                    {
                        gtk_tree_selection_select_path(selection, path);
                    }

                    // scroll to first and set cursor
                    if (first_select && select)
                    {
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(this->folder_view_),
                                                 path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(this->folder_view_),
                                                     path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
            }
            gtk_tree_path_free(path);
        } while (gtk_tree_model_iter_next(model, &it));
    }

    this->focus_folder_view();
}

static bool
invert_selection(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* it,
                 PtkFileBrowser* file_browser) noexcept
{
    (void)model;
    (void)it;
    GtkTreeSelection* selection = nullptr;

    switch (file_browser->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view_), path))
            {
                exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view_), path);
            }
            else
            {
                exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view_), path);
            }
            break;
        case ptk::file_browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view_));
            if (gtk_tree_selection_path_is_selected(selection, path))
            {
                gtk_tree_selection_unselect_path(selection, path);
            }
            else
            {
                gtk_tree_selection_select_path(selection, path);
            }
            break;
    }
    return false;
}

void
PtkFileBrowser::invert_selection() noexcept
{
    GtkTreeModel* model;
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)::invert_selection, this);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(this->folder_view_), this);
            break;
        case ptk::file_browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(this->folder_view_));
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)::invert_selection, this);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(selection), this);
            break;
    }
}

/* FIXME: Do not recreate the view if previous view is compact view */
void
PtkFileBrowser::view_as_icons() noexcept
{
    if (this->view_mode_ == ptk::file_browser::view_mode::icon_view && this->folder_view_)
    {
        return;
    }

    this->show_thumbnails(this->max_thumbnail_, true);

    this->view_mode_ = ptk::file_browser::view_mode::icon_view;
    if (this->folder_view_)
    {
        gtk_widget_destroy(this->folder_view_);
    }
    this->folder_view_ = create_folder_view(this, ptk::file_browser::view_mode::icon_view);
    exo_icon_view_set_model(EXO_ICON_VIEW(this->folder_view_), this->file_list_);
    gtk_scrolled_window_set_policy(this->folder_view_scroll_,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_widget_show(GTK_WIDGET(this->folder_view_));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->folder_view_scroll_),
                                  GTK_WIDGET(this->folder_view_));
}

/* FIXME: Do not recreate the view if previous view is icon view */
void
PtkFileBrowser::view_as_compact_list() noexcept
{
    if (this->view_mode_ == ptk::file_browser::view_mode::compact_view && this->folder_view_)
    {
        return;
    }

    this->show_thumbnails(this->max_thumbnail_);

    this->view_mode_ = ptk::file_browser::view_mode::compact_view;
    if (this->folder_view_)
    {
        gtk_widget_destroy(this->folder_view_);
    }
    this->folder_view_ = create_folder_view(this, ptk::file_browser::view_mode::compact_view);
    exo_icon_view_set_model(EXO_ICON_VIEW(this->folder_view_), this->file_list_);
    gtk_scrolled_window_set_policy(this->folder_view_scroll_,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_widget_show(GTK_WIDGET(this->folder_view_));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->folder_view_scroll_),
                                  GTK_WIDGET(this->folder_view_));
}

void
PtkFileBrowser::view_as_list() noexcept
{
    if (this->view_mode_ == ptk::file_browser::view_mode::list_view && this->folder_view_)
    {
        return;
    }

    this->show_thumbnails(this->max_thumbnail_);

    this->view_mode_ = ptk::file_browser::view_mode::list_view;
    if (this->folder_view_)
    {
        gtk_widget_destroy(this->folder_view_);
    }
    this->folder_view_ = create_folder_view(this, ptk::file_browser::view_mode::list_view);
    gtk_tree_view_set_model(GTK_TREE_VIEW(this->folder_view_), this->file_list_);
    gtk_scrolled_window_set_policy(this->folder_view_scroll_,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_ALWAYS);
    gtk_widget_show(GTK_WIDGET(this->folder_view_));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->folder_view_scroll_),
                                  GTK_WIDGET(this->folder_view_));
}

void
PtkFileBrowser::show_thumbnails(i32 max_file_size) noexcept
{
    this->show_thumbnails(max_file_size, this->large_icons_);
}

void
PtkFileBrowser::show_thumbnails(i32 max_file_size, bool large_icons) noexcept
{
    this->max_thumbnail_ = max_file_size;
    if (this->file_list_)
    {
        if (!this->dir_ || this->dir_->avoid_changes())
        { // this will disable thumbnails if change detection is blacklisted on current device
            max_file_size = 0;
        }

        ptk_file_list_show_thumbnails(PTK_FILE_LIST_REINTERPRET(this->file_list_),
                                      large_icons,
                                      max_file_size);
        this->update_toolbar_widgets(xset::tool::show_thumb);
    }
}

void
PtkFileBrowser::update_views() noexcept
{
    // ztd::logger::debug("PtkFileBrowser::update_views fb={}  (panel {})", fmt::ptr(this), this->mypanel);

    // hide/show browser widgets based on user settings
    const panel_t p = this->panel_;
    const xset::main_window_panel mode = this->main_window_->panel_context.at(p);
    bool need_enable_toolbar = false;

    if (xset_get_b_panel_mode(p, xset::panel::show_toolbox, mode))
    {
        if (!this->toolbar)
        {
            rebuild_toolbox(nullptr, this);
            need_enable_toolbar = true;
        }
        gtk_widget_show_all(GTK_WIDGET(this->toolbox_));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(this->toolbox_));
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_sidebar, mode))
    {
        if (!this->side_toolbar)
        {
            rebuild_side_toolbox(nullptr, this);
            need_enable_toolbar = true;
        }
        gtk_widget_show_all(GTK_WIDGET(this->side_toolbox));
    }
    else
    {
        //  toolboxes must be destroyed together for toolbar_widgets[]
#if 0
        if ( this->side_toolbar )
        {
            gtk_widget_destroy(GTK_WIDGET(this->side_toolbar));
            this->side_toolbar = nullptr;
        }
#endif
        gtk_widget_hide(GTK_WIDGET(this->side_toolbox));
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
    {
        if (!this->side_dir)
        {
            this->side_dir = ptk_file_browser_create_dir_tree(this);
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->side_dir_scroll),
                                          GTK_WIDGET(this->side_dir));
        }
        gtk_widget_show_all(GTK_WIDGET(this->side_dir_scroll));
        if (this->side_dir && this->file_list_)
        {
            ptk_dir_tree_view_chdir(GTK_TREE_VIEW(this->side_dir), this->cwd());
        }
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(this->side_dir_scroll));
        if (this->side_dir)
        {
            gtk_widget_destroy(this->side_dir);
        }
        this->side_dir = nullptr;
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_devmon, mode))
    {
        if (!this->side_dev)
        {
            this->side_dev = ptk_location_view_new(this);
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->side_dir_scroll),
                                          GTK_WIDGET(this->side_dir));
        }
        gtk_widget_show_all(GTK_WIDGET(this->side_dev_scroll));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(this->side_dev_scroll));
        if (this->side_dev)
        {
            gtk_widget_destroy(this->side_dev);
        }
        this->side_dev = nullptr;
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
    {
        gtk_widget_show(GTK_WIDGET(this->side_vpane_bottom));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(this->side_vpane_bottom));
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_devmon, mode) ||
        xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
    {
        gtk_widget_show(GTK_WIDGET(this->side_vbox));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(this->side_vbox));
    }

    if (need_enable_toolbar)
    {
        this->enable_toolbar();
    }
    else
    {
        // toggle sidepane toolbar buttons
        this->update_toolbar_widgets(xset::tool::devices);
        this->update_toolbar_widgets(xset::tool::tree);
    }

    // set slider positions

    i32 pos;

    // hpane
    pos = this->main_window_->panel_slide_x[p];
    if (pos < 100)
    {
        pos = -1;
    }
    // ztd::logger::info("    set slide_x = {}", pos);
    if (pos > 0)
    {
        gtk_paned_set_position(this->hpane, pos);
    }

    // side_vpane_top
    pos = this->main_window_->panel_slide_y[p];
    if (pos < 20)
    {
        pos = -1;
    }
    // ztd::logger::info("    slide_y = {}", pos);
    gtk_paned_set_position(this->side_vpane_top, pos);

    // side_vpane_bottom
    pos = this->main_window_->panel_slide_s[p];
    if (pos < 20)
    {
        pos = -1;
    }
    // ztd::logger::info( "    slide_s = {}", pos);
    gtk_paned_set_position(this->side_vpane_bottom, pos);

    // Large Icons - option for Detailed and Compact list views
    const bool large_icons = xset_get_b_panel(p, xset::panel::list_icons) ||
                             xset_get_b_panel_mode(p, xset::panel::list_large, mode);
    if (large_icons != !!this->large_icons_)
    {
        if (this->folder_view_)
        {
            // force rebuild of folder_view for icon size change
            gtk_widget_destroy(this->folder_view_);
            this->folder_view(nullptr);
        }
        this->large_icons_ = large_icons;
        this->update_toolbar_widgets(xset::tool::large_icons);
    }

    // List Styles
    if (xset_get_b_panel(p, xset::panel::list_detailed))
    {
        this->view_as_list();

        // Set column widths for this panel context
        xset_t set;

        if (GTK_IS_TREE_VIEW(this->folder_view_))
        {
            // ztd::logger::info("    set widths   mode = {}", mode);
            for (const auto i : std::views::iota(0uz, columns.size()))
            {
                GtkTreeViewColumn* col = gtk_tree_view_get_column(GTK_TREE_VIEW(this->folder_view_),
                                                                  static_cast<i32>(i));
                if (!col)
                {
                    break;
                }
                const char* title = gtk_tree_view_column_get_title(col);
                for (const auto [index, column] : std::views::enumerate(columns))
                {
                    if (title == column.title)
                    {
                        // get column width for this panel context
                        set = xset_get_panel_mode(p, column.xset_name, mode);
                        const i32 width = set->y ? std::stoi(set->y.value()) : 100;
                        // ztd::logger::info("        {}\t{}", width, title );
                        if (width)
                        {
                            gtk_tree_view_column_set_fixed_width(col, width);
                            // ztd::logger::info("upd set_width {} {}", magic_enum::enum_name(columns.at(j).xset_name), width);
                        }
                        // set column visibility
                        gtk_tree_view_column_set_visible(col,
                                                         set->b == xset::b::xtrue || index == 0);

                        break;
                    }
                }
            }
        }
    }
    else if (xset_get_b_panel(p, xset::panel::list_icons))
    {
        this->view_as_icons();
    }
    else if (xset_get_b_panel(p, xset::panel::list_compact))
    {
        this->view_as_compact_list();
    }
    else
    {
        xset_set_panel(p, xset::panel::list_detailed, xset::var::b, "1");
        this->view_as_list();
    }

    // Show Hidden
    this->show_hidden_files(xset_get_b_panel(p, xset::panel::show_hidden));

    // ztd::logger::info("PtkFileBrowser::update_views fb={} DONE", fmt::ptr(this));
}

void
PtkFileBrowser::focus(i32 job) noexcept
{
    GtkWidget* widget;

    const panel_t p = this->panel_;
    const xset::main_window_panel mode = this->main_window_->panel_context.at(p);
    switch (job)
    {
        case 0:
            // path bar
            if (!xset_get_b_panel_mode(p, xset::panel::show_toolbox, mode))
            {
                xset_set_b_panel_mode(p, xset::panel::show_toolbox, mode, true);
                update_views_all_windows(nullptr, this);
            }
            widget = GTK_WIDGET(this->path_bar_);
            break;
        case 1:
            if (!xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
            {
                xset_set_b_panel_mode(p, xset::panel::show_dirtree, mode, true);
                update_views_all_windows(nullptr, this);
            }
            widget = this->side_dir;
            break;
        case 2:
            // Deprecated - bookmark
            widget = nullptr;
            break;
        case 3:
            if (!xset_get_b_panel_mode(p, xset::panel::show_devmon, mode))
            {
                xset_set_b_panel_mode(p, xset::panel::show_devmon, mode, true);
                update_views_all_windows(nullptr, this);
            }
            widget = this->side_dev;
            break;
        case 4:
            widget = this->folder_view_;
            break;
        default:
            return;
    }
    if (gtk_widget_get_visible(widget))
    {
        gtk_widget_grab_focus(GTK_WIDGET(widget));
    }
}

void
PtkFileBrowser::focus_me() noexcept
{
    this->run_event<spacefm::signal::change_pane>();
}

void
PtkFileBrowser::save_column_widths(GtkTreeView* view) noexcept
{
    if (!GTK_IS_TREE_VIEW(view))
    {
        return;
    }

    if (this->view_mode_ != ptk::file_browser::view_mode::list_view)
    {
        return;
    }

    // if the window was opened maximized and stayed maximized, or the window is
    // unmaximized and not fullscreen, save the columns
    if ((!this->main_window_->maximized || this->main_window_->opened_maximized) &&
        !this->main_window_->fullscreen)
    {
        const panel_t p = this->panel_;
        const xset::main_window_panel mode = this->main_window_->panel_context.at(p);
        // ztd::logger::debug("save_columns  fb={} (panel {})  mode = {}", fmt::ptr(this), p, mode);
        for (const auto i : std::views::iota(0uz, columns.size()))
        {
            GtkTreeViewColumn* col = gtk_tree_view_get_column(view, static_cast<i32>(i));
            if (!col)
            {
                return;
            }
            const char* title = gtk_tree_view_column_get_title(col);
            for (const auto column : columns)
            {
                if (title == column.title)
                {
                    // save column width for this panel context
                    const xset_t set = xset_get_panel_mode(p, column.xset_name, mode);
                    const i32 width = gtk_tree_view_column_get_width(col);
                    if (width > 0)
                    {
                        set->y = std::to_string(width);
                        // ztd::logger::info("        {}\t{}", width, title);
                    }

                    break;
                }
            }
        }
    }
}

bool
PtkFileBrowser::slider_release(GtkPaned* pane) noexcept
{
    const panel_t p = this->panel_;
    const xset::main_window_panel mode = this->main_window_->panel_context.at(p);

    const xset_t set = xset_get_panel_mode(p, xset::panel::slider_positions, mode);

    if (pane == this->hpane)
    {
        const i32 pos = gtk_paned_get_position(this->hpane);
        if (!this->main_window_->fullscreen)
        {
            set->x = std::to_string(pos);
        }
        this->main_window_->panel_slide_x[p] = pos;
        // ztd::logger::debug("    slide_x = {}", pos);
    }
    else
    {
        i32 pos;
        // ztd::logger::debug("PtkFileBrowser::slider_release fb={}  (panel {})  mode = {}", fmt::ptr(this), p, fmt::ptr(mode));
        pos = gtk_paned_get_position(this->side_vpane_top);
        if (!this->main_window_->fullscreen)
        {
            set->y = std::to_string(pos);
        }
        this->main_window_->panel_slide_y[p] = pos;
        // ztd::logger::debug("    slide_y = {}  ", pos);

        pos = gtk_paned_get_position(this->side_vpane_bottom);
        if (!this->main_window_->fullscreen)
        {
            set->s = std::to_string(pos);
        }
        this->main_window_->panel_slide_s[p] = pos;
        // ztd::logger::debug("    slide_s = {}", pos);
    }
    return false;
}

void
PtkFileBrowser::rebuild_toolbars() noexcept
{
    for (auto& toolbar_widget : this->toolbar_widgets)
    {
        g_slist_free(toolbar_widget);
        toolbar_widget = nullptr;
    }
    if (this->toolbar)
    {
        rebuild_toolbox(nullptr, this);
        const auto& cwd = this->cwd();
#if (GTK_MAJOR_VERSION == 4)
        gtk_editable_set_text(GTK_EDITABLE(this->path_bar_), cwd.c_str());
#elif (GTK_MAJOR_VERSION == 3)
        gtk_entry_set_text(GTK_ENTRY(this->path_bar_), cwd.c_str());
#endif
    }
    if (this->side_toolbar)
    {
        rebuild_side_toolbox(nullptr, this);
    }

    this->enable_toolbar();
}

void
PtkFileBrowser::update_selection_history() noexcept
{
    const auto& cwd = this->cwd();
    // ztd::logger::debug("selection history: {}", cwd.string());
    this->selection_history->selection_history.contains(cwd);
    {
        this->selection_history->selection_history.erase(cwd);
    }

    const auto selected_files = this->selected_files();
    if (selected_files.empty())
    {
        return;
    }

    std::vector<std::filesystem::path> selected_filenames;
    selected_filenames.reserve(selected_files.size());

    for (const auto& file : selected_files)
    {
        selected_filenames.emplace_back(file->name());
    }
    this->selection_history->selection_history.insert({cwd, selected_filenames});
}

GList*
PtkFileBrowser::selected_items(GtkTreeModel** model) noexcept
{
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            *model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
            return exo_icon_view_get_selected_items(EXO_ICON_VIEW(this->folder_view_));
            break;
        case ptk::file_browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            return gtk_tree_selection_get_selected_rows(selection, model);
            break;
    }
    return nullptr;
}

void
PtkFileBrowser::select_file(const std::filesystem::path& filename,
                            const bool unselect_others) noexcept
{
    GtkTreeSelection* tree_sel = nullptr;
    GtkTreeModel* model = nullptr;

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(this->file_list_);
    if (this->view_mode_ == ptk::file_browser::view_mode::icon_view ||
        this->view_mode_ == ptk::file_browser::view_mode::compact_view)
    {
        if (unselect_others)
        {
            exo_icon_view_unselect_all(EXO_ICON_VIEW(this->folder_view_));
        }
        model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
    }
    else if (this->view_mode_ == ptk::file_browser::view_mode::list_view)
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(this->folder_view_));
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
        if (unselect_others)
        {
            gtk_tree_selection_unselect_all(tree_sel);
        }
    }
    if (!model)
    {
        return;
    }

    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        const std::string select_filename = filename.filename();

        do
        {
            std::shared_ptr<vfs::file> file;
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (file)
            {
                if (file->name() == select_filename)
                {
                    GtkTreePath* tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (this->view_mode_ == ptk::file_browser::view_mode::icon_view ||
                        this->view_mode_ == ptk::file_browser::view_mode::compact_view)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(this->folder_view_), tree_path);
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(this->folder_view_),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(this->folder_view_),
                                                     tree_path,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    else if (this->view_mode_ == ptk::file_browser::view_mode::list_view)
                    {
                        gtk_tree_selection_select_path(tree_sel, tree_path);
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(this->folder_view_),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(this->folder_view_),
                                                     tree_path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    gtk_tree_path_free(tree_path);
                    break;
                }
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
}

void
PtkFileBrowser::select_files(const std::span<std::filesystem::path> select_filenames) noexcept
{
    this->unselect_all();

    for (const std::filesystem::path& select_filename : select_filenames)
    {
        this->select_file(select_filename.filename(), false);
    }
}

void
PtkFileBrowser::unselect_file(const std::filesystem::path& filename,
                              const bool unselect_others) noexcept
{
    GtkTreeSelection* tree_sel = nullptr;
    GtkTreeModel* model = nullptr;

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(this->file_list_);
    if (this->view_mode_ == ptk::file_browser::view_mode::icon_view ||
        this->view_mode_ == ptk::file_browser::view_mode::compact_view)
    {
        if (unselect_others)
        {
            exo_icon_view_unselect_all(EXO_ICON_VIEW(this->folder_view_));
        }
        model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
    }
    else if (this->view_mode_ == ptk::file_browser::view_mode::list_view)
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(this->folder_view_));
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
        if (unselect_others)
        {
            gtk_tree_selection_unselect_all(tree_sel);
        }
    }
    if (!model)
    {
        return;
    }

    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        const std::string unselect_filename = filename.filename();

        do
        {
            std::shared_ptr<vfs::file> file;
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (file)
            {
                if (file->name() == unselect_filename)
                {
                    GtkTreePath* tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (this->view_mode_ == ptk::file_browser::view_mode::icon_view ||
                        this->view_mode_ == ptk::file_browser::view_mode::compact_view)
                    {
                        exo_icon_view_unselect_path(EXO_ICON_VIEW(this->folder_view_), tree_path);
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(this->folder_view_),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(this->folder_view_),
                                                     tree_path,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    else if (this->view_mode_ == ptk::file_browser::view_mode::list_view)
                    {
                        gtk_tree_selection_unselect_path(tree_sel, tree_path);
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(this->folder_view_),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(this->folder_view_),
                                                     tree_path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    gtk_tree_path_free(tree_path);
                    break;
                }
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
}

void
PtkFileBrowser::seek_path(const std::filesystem::path& seek_dir,
                          const std::filesystem::path& seek_name) noexcept
{
    // change to dir seek_dir if needed; select first dir or else file with
    // prefix seek_name
    const auto& cwd = this->cwd();

    if (!std::filesystem::equivalent(cwd, seek_dir))
    {
        // change dir
        this->seek_name_ = seek_name;
        this->inhibit_focus_ = true;
        if (!this->chdir(seek_dir))
        {
            this->inhibit_focus_ = false;
            this->seek_name_ = std::nullopt;
        }
        // return here to allow dir to load
        // finishes seek in main-window.c on_file_browser_after_chdir()
        return;
    }

    // no change dir was needed or was called from on_file_browser_after_chdir()
    // select seek name
    this->unselect_all();

    if (seek_name.empty())
    {
        return;
    }

    // get model, treesel, and stop signals
    GtkTreeModel* model;
    GtkTreeIter it;
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
            break;
        case ptk::file_browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(this->folder_view_));
            break;
    }

    // test rows - give preference to matching dir, else match file
    GtkTreeIter it_file;
    GtkTreeIter it_dir;
    it_file.stamp = 0;
    it_dir.stamp = 0;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            // get file
            std::shared_ptr<vfs::file> file;
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (!file)
            {
                continue;
            }

            // test name
            const auto name = file->name();
            if (std::filesystem::equivalent(name, seek_name))
            {
                // exact match (may be file or dir)
                it_dir = it;
                break;
            }
            if (name.starts_with(seek_name.string()))
            {
                // prefix found
                if (file->is_directory())
                {
                    if (!it_dir.stamp)
                    {
                        it_dir = it;
                    }
                }
                else if (!it_file.stamp)
                {
                    it_file = it;
                }
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }

    if (it_dir.stamp)
    {
        it = it_dir;
    }
    else
    {
        it = it_file;
    }

    if (!it.stamp)
    {
        return;
    }

    // do selection and scroll to selected
    GtkTreePath* path;
    path =
        gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST_REINTERPRET(this->file_list_)), &it);
    if (!path)
    {
        return;
    }

    switch (this->view_mode_)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            // select
            exo_icon_view_select_path(EXO_ICON_VIEW(this->folder_view_), path);

            // scroll and set cursor
            exo_icon_view_set_cursor(EXO_ICON_VIEW(this->folder_view_), path, nullptr, false);
            exo_icon_view_scroll_to_path(EXO_ICON_VIEW(this->folder_view_), path, true, .25, 0);
            break;
        case ptk::file_browser::view_mode::list_view:
            // select
            gtk_tree_selection_select_path(selection, path);

            // scroll and set cursor
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(this->folder_view_), path, nullptr, false);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(this->folder_view_),
                                         path,
                                         nullptr,
                                         true,
                                         .25,
                                         0);
            break;
    }
    gtk_tree_path_free(path);
}

void
PtkFileBrowser::update_toolbar_widgets(const xset_t& set, xset::tool tool_type) noexcept
{
    (void)tool_type;

    assert(set != nullptr);

    if (set && !set->lock && set->menu_style == xset::menu::check &&
        set->tool == xset::tool::custom)
    {
        // a custom checkbox is being updated
        for (GSList* l = this->toolbar_widgets[7]; l; l = g_slist_next(l))
        {
            const xset_t test_set =
                xset_get(static_cast<const char*>(g_object_get_data(G_OBJECT(l->data), "set")));
            if (set == test_set)
            {
                GtkWidget* widget = GTK_WIDGET(l->data);
                if (GTK_IS_TOGGLE_BUTTON(widget))
                {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
                                                 set->b == xset::b::xtrue);
                    return;
                }
            }
        }
        ztd::logger::warn("PtkFileBrowser::update_toolbar_widget widget not found for set");
        return;
    }
    else if (set)
    {
        ztd::logger::warn("PtkFileBrowser::update_toolbar_widget invalid set");
        return;
    }
}

void
PtkFileBrowser::update_toolbar_widgets(xset::tool tool_type) noexcept
{
    // builtin tool
    bool b = false;
    unsigned char x;

    switch (tool_type)
    {
        case xset::tool::up:
            x = 0;
            b = !std::filesystem::equivalent(this->cwd(), "/");
            break;
        case xset::tool::back:
        case xset::tool::back_menu:
            x = 1;
            b = this->navigation_history->has_back();
            break;
        case xset::tool::fwd:
        case xset::tool::fwd_menu:
            x = 2;
            b = this->navigation_history->has_forward();
            break;
        case xset::tool::devices:
            x = 3;
            b = !!this->side_dev;
            break;
        case xset::tool::bookmarks:
            x = 4;
            break;
        case xset::tool::tree:
            x = 5;
            b = !!this->side_dir;
            break;
        case xset::tool::show_hidden:
            x = 6;
            b = this->show_hidden_files_;
            break;
        case xset::tool::show_thumb:
            x = 8;
            b = app_settings.show_thumbnail();
            break;
        case xset::tool::large_icons:
            x = 9;
            b = this->large_icons_;
            break;
        case xset::tool::NOT:
        case xset::tool::custom:
        case xset::tool::home:
        case xset::tool::DEFAULT:
        case xset::tool::refresh:
        case xset::tool::new_tab:
        case xset::tool::new_tab_here:
        case xset::tool::invalid:
            ztd::logger::warn("PtkFileBrowser::update_toolbar_widget invalid tool_type");
            return;
    }

    // update all widgets in list
    for (GSList* l = this->toolbar_widgets[x]; l; l = g_slist_next(l))
    {
        GtkWidget* widget = GTK_WIDGET(l->data);
        if (GTK_IS_TOGGLE_BUTTON(widget))
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), b);
        }
        else if (GTK_IS_WIDGET(widget))
        {
            gtk_widget_set_sensitive(widget, b);
        }
        else
        {
            ztd::logger::warn("PtkFileBrowser::update_toolbar_widget invalid widget");
        }
    }
}

void
PtkFileBrowser::show_history_menu(bool is_back_history, GdkEvent* event) noexcept
{
    (void)event;

    GtkWidget* menu = gtk_menu_new();
    bool has_items = false;

    if (is_back_history)
    {
        // back history
        for (const auto& back_history : this->navigation_history->get_back())
        {
            add_history_menu_item(this, GTK_WIDGET(menu), back_history);
            if (!has_items)
            {
                has_items = true;
            }
        }
    }
    else
    {
        // forward history
        for (const auto& forward_history : this->navigation_history->get_back())
        {
            add_history_menu_item(this, GTK_WIDGET(menu), forward_history);
            if (!has_items)
            {
                has_items = true;
            }
        }
    }
    if (has_items)
    {
        gtk_widget_show_all(GTK_WIDGET(menu));
        gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
    }
    else
    {
        gtk_widget_destroy(menu);
    }
}

void
PtkFileBrowser::on_permission(GtkMenuItem* item,
                              const std::span<const std::shared_ptr<vfs::file>> selected_files,
                              const std::filesystem::path& cwd) noexcept
{
    if (selected_files.empty())
    {
        return;
    }

    const xset_t set = xset_get(static_cast<const char*>(g_object_get_data(G_OBJECT(item), "set")));
    if (!set)
    {
        return;
    }

    if (!set->name.starts_with("perm_"))
    {
        return;
    }

    std::string prog;
    if (set->name.starts_with("perm_go") || set->name.starts_with("perm_ugo"))
    {
        prog = "chmod -R";
    }
    else
    {
        prog = "chmod";
    }

    std::string cmd;
    if (set->xset_name == xset::name::perm_r)
    {
        cmd = "u+r-wx,go-rwx";
    }
    else if (set->xset_name == xset::name::perm_rw)
    {
        cmd = "u+rw-x,go-rwx";
    }
    else if (set->xset_name == xset::name::perm_rwx)
    {
        cmd = "u+rwx,go-rwx";
    }
    else if (set->xset_name == xset::name::perm_r_r)
    {
        cmd = "u+r-wx,g+r-wx,o-rwx";
    }
    else if (set->xset_name == xset::name::perm_rw_r)
    {
        cmd = "u+rw-x,g+r-wx,o-rwx";
    }
    else if (set->xset_name == xset::name::perm_rw_rw)
    {
        cmd = "u+rw-x,g+rw-x,o-rwx";
    }
    else if (set->xset_name == xset::name::perm_rwxr_x)
    {
        cmd = "u+rwx,g+rx-w,o-rwx";
    }
    else if (set->xset_name == xset::name::perm_rwxrwx)
    {
        cmd = "u+rwx,g+rwx,o-rwx";
    }
    else if (set->xset_name == xset::name::perm_r_r_r)
    {
        cmd = "ugo+r,ugo-wx";
    }
    else if (set->xset_name == xset::name::perm_rw_r_r)
    {
        cmd = "u+rw-x,go+r-wx";
    }
    else if (set->xset_name == xset::name::perm_rw_rw_rw)
    {
        cmd = "ugo+rw-x";
    }
    else if (set->xset_name == xset::name::perm_rwxr_r)
    {
        cmd = "u+rwx,go+r-wx";
    }
    else if (set->xset_name == xset::name::perm_rwxr_xr_x)
    {
        cmd = "u+rwx,go+rx-w";
    }
    else if (set->xset_name == xset::name::perm_rwxrwxrwx)
    {
        cmd = "ugo+rwx,-t";
    }
    else if (set->xset_name == xset::name::perm_rwxrwxrwt)
    {
        cmd = "ugo+rwx,+t";
    }
    else if (set->xset_name == xset::name::perm_unstick)
    {
        cmd = "-t";
    }
    else if (set->xset_name == xset::name::perm_stick)
    {
        cmd = "+t";
    }
    else if (set->xset_name == xset::name::perm_go_w)
    {
        cmd = "go-w";
    }
    else if (set->xset_name == xset::name::perm_go_rwx)
    {
        cmd = "go-rwx";
    }
    else if (set->xset_name == xset::name::perm_ugo_w)
    {
        cmd = "ugo+w";
    }
    else if (set->xset_name == xset::name::perm_ugo_rx)
    {
        cmd = "ugo+rX";
    }
    else if (set->xset_name == xset::name::perm_ugo_rwx)
    {
        cmd = "ugo+rwX";
    }
    else
    {
        return;
    }

    std::string file_paths;
    for (const auto& file : selected_files)
    {
        const std::string file_path = ztd::shell::quote(file->name());
        file_paths = std::format("{} {}", file_paths, file_path);
    }

    // task
    PtkFileTask* ptask =
        ptk_file_exec_new(set->menu_label.value(), cwd, GTK_WIDGET(this), this->task_view_);
    ptask->task->exec_command = std::format("{} {} {}", prog, cmd, file_paths);
    ptask->task->exec_browser = this;
    ptask->task->exec_sync = true;
    ptask->task->exec_show_error = true;
    ptask->task->exec_show_output = false;
    ptk_file_task_run(ptask);
}

void
PtkFileBrowser::on_action(const xset::name setname) noexcept
{
    const xset_t set = xset_get(setname);
    // ztd::logger::info("PtkFileBrowser::on_action {}", set->name);

    if (set->name.starts_with("book_"))
    {
        if (set->xset_name == xset::name::book_add)
        {
            ptk_bookmark_view_add_bookmark(this->cwd());
        }
    }
    else if (set->name.starts_with("go_"))
    {
        if (set->xset_name == xset::name::go_back)
        {
            this->go_back();
        }
        else if (set->xset_name == xset::name::go_forward)
        {
            this->go_forward();
        }
        else if (set->xset_name == xset::name::go_up)
        {
            this->go_up();
        }
        else if (set->xset_name == xset::name::go_home)
        {
            this->go_home();
        }
        else if (set->xset_name == xset::name::go_default)
        {
            this->go_default();
        }
        else if (set->xset_name == xset::name::go_set_default)
        {
            this->set_default_folder();
        }
    }
    else if (set->name.starts_with("tab_"))
    {
        if (set->xset_name == xset::name::tab_new)
        {
            this->new_tab();
        }
        else if (set->xset_name == xset::name::tab_new_here)
        {
            this->new_tab_here();
        }
        else
        {
            i32 i = 0;
            if (set->xset_name == xset::name::tab_prev)
            {
                i = tab_control_code_prev;
            }
            else if (set->xset_name == xset::name::tab_next)
            {
                i = tab_control_code_next;
            }
            else if (set->xset_name == xset::name::tab_close)
            {
                i = tab_control_code_close;
            }
            else if (set->xset_name == xset::name::tab_restore)
            {
                i = tab_control_code_restore;
            }
            else
            {
                i = std::stoi(ztd::removeprefix(set->name, "tab_"));
            }
            this->go_tab(i);
        }
    }
    else if (set->name.starts_with("focus_"))
    {
        i32 i = 0;
        if (set->xset_name == xset::name::focus_path_bar)
        {
            i = 0;
        }
        else if (set->xset_name == xset::name::focus_filelist)
        {
            i = 4;
        }
        else if (set->xset_name == xset::name::focus_dirtree)
        {
            i = 1;
        }
        else if (set->xset_name == xset::name::focus_book)
        {
            i = 2;
        }
        else if (set->xset_name == xset::name::focus_device)
        {
            i = 3;
        }
        this->focus(i);
    }
    else if (set->xset_name == xset::name::view_reorder_col)
    {
        on_reorder(nullptr, GTK_WIDGET(this));
    }
    else if (set->xset_name == xset::name::view_refresh)
    {
        this->refresh();
    }
    else if (set->xset_name == xset::name::view_thumb)
    {
        main_window_toggle_thumbnails_all_windows();
    }
    else if (set->name.starts_with("sortby_"))
    {
        i32 i = -3;
        if (set->xset_name == xset::name::sortby_name)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::name);
        }
        else if (set->xset_name == xset::name::sortby_size)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::size);
        }
        else if (set->xset_name == xset::name::sortby_bytes)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::bytes);
        }
        else if (set->xset_name == xset::name::sortby_type)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::type);
        }
        else if (set->xset_name == xset::name::sortby_mime)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::mime);
        }
        else if (set->xset_name == xset::name::sortby_perm)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::perm);
        }
        else if (set->xset_name == xset::name::sortby_owner)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::owner);
        }
        else if (set->xset_name == xset::name::sortby_group)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::group);
        }
        else if (set->xset_name == xset::name::sortby_atime)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::atime);
        }
        else if (set->xset_name == xset::name::sortby_btime)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::btime);
        }
        else if (set->xset_name == xset::name::sortby_ctime)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::ctime);
        }
        else if (set->xset_name == xset::name::sortby_mtime)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::mtime);
        }
        else if (set->xset_name == xset::name::sortby_ascend)
        {
            i = -1;
            set->b = this->sort_type_ == GtkSortType::GTK_SORT_ASCENDING ? xset::b::xtrue
                                                                         : xset::b::xfalse;
        }
        else if (set->xset_name == xset::name::sortby_descend)
        {
            i = -2;
            set->b = this->sort_type_ == GtkSortType::GTK_SORT_DESCENDING ? xset::b::xtrue
                                                                          : xset::b::xfalse;
        }
        if (i > 0)
        { // always want to show name
            set->b = this->sort_order_ == ptk::file_browser::sort_order(i) ? xset::b::xtrue
                                                                           : xset::b::xfalse;
        }
        on_popup_sortby(nullptr, this, i);
    }
    else if (set->name.starts_with("sortx_"))
    {
        this->set_sort_extra(set->xset_name);
    }
    else if (set->name.starts_with("panel"))
    {
        const auto mode = this->main_window_->panel_context.at(this->panel_);

        const panel_t panel_num = std::stoi(set->name.substr(5, 1));
        // ztd::logger::debug("ACTION panel={}", panel_num);

        if (is_valid_panel(panel_num))
        {
            xset_t set2;
            const std::string fullxname = std::format("panel{}_", panel_num);
            const std::string xname = ztd::removeprefix(set->name, fullxname);
            if (xname == "show_hidden") // shared key
            {
                this->show_hidden_files(xset_get_b_panel(this->panel_, xset::panel::show_hidden));
            }
            else if (xname == "show")
            { // main View|Panel N
                show_panels_all_windows(nullptr, this->main_window_);
            }
            else if (xname.starts_with("show_")) // shared key
            {
                set2 = xset_get_panel_mode(this->panel_, xname, mode);
                set2->b = set2->b == xset::b::xtrue ? xset::b::unset : xset::b::xtrue;
                update_views_all_windows(nullptr, this);
            }
            else if (xname == "list_detailed")
            { // shared key
                on_popup_list_detailed(nullptr, this);
            }
            else if (xname == "list_icons")
            { // shared key
                on_popup_list_icons(nullptr, this);
            }
            else if (xname == "list_compact")
            { // shared key
                on_popup_list_compact(nullptr, this);
            }
            else if (xname == "list_large") // shared key
            {
                if (this->view_mode_ != ptk::file_browser::view_mode::icon_view)
                {
                    xset_set_b_panel(this->panel_, xset::panel::list_large, !this->large_icons_);
                    on_popup_list_large(nullptr, this);
                }
            }
            else if (xname.starts_with("detcol_") // shared key
                     && this->view_mode_ == ptk::file_browser::view_mode::list_view)
            {
                set2 = xset_get_panel_mode(this->panel_, xname, mode);
                set2->b = set2->b == xset::b::xtrue ? xset::b::unset : xset::b::xtrue;
                update_views_all_windows(nullptr, this);
            }
        }
    }
    else if (set->name.starts_with("status_"))
    {
        if (set->name == "status_border" || set->name == "status_text")
        {
            on_status_effect_change(nullptr, this);
        }
        else if (set->xset_name == xset::name::status_name ||
                 set->xset_name == xset::name::status_path ||
                 set->xset_name == xset::name::status_info ||
                 set->xset_name == xset::name::status_hide)
        {
            on_status_middle_click_config(nullptr, set);
        }
    }
    else if (set->name.starts_with("paste_"))
    {
        if (set->xset_name == xset::name::paste_link)
        {
            this->paste_link();
        }
        else if (set->xset_name == xset::name::paste_target)
        {
            this->paste_target();
        }
        else if (set->xset_name == xset::name::paste_as)
        {
            ptk_paste_file(this, this->cwd());
        }
    }
    else if (set->name.starts_with("select_"))
    {
        if (set->xset_name == xset::name::select_all)
        {
            this->select_all();
        }
        else if (set->xset_name == xset::name::select_un)
        {
            this->unselect_all();
        }
        else if (set->xset_name == xset::name::select_invert)
        {
            this->invert_selection();
        }
        else if (set->xset_name == xset::name::select_patt)
        {
            this->select_pattern();
        }
    }
    else // all the rest require ptkfilemenu data
    {
        ptk_file_menu_action(this, set);
    }
}

// Default signal handlers

void
PtkFileBrowser::focus_folder_view() noexcept
{
    gtk_widget_grab_focus(GTK_WIDGET(this->folder_view_));

    this->run_event<spacefm::signal::change_pane>();
}

void
PtkFileBrowser::enable_toolbar() noexcept
{
    this->update_toolbar_widgets(xset::tool::back);
    this->update_toolbar_widgets(xset::tool::fwd);
    this->update_toolbar_widgets(xset::tool::up);
    this->update_toolbar_widgets(xset::tool::devices);
    this->update_toolbar_widgets(xset::tool::tree);
    this->update_toolbar_widgets(xset::tool::show_hidden);
    this->update_toolbar_widgets(xset::tool::show_thumb);
    this->update_toolbar_widgets(xset::tool::large_icons);
}

// xset callback wrapper functions

void
ptk_file_browser_go_home(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->go_home();
}

void
ptk_file_browser_go_default(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->go_default();
}

void
ptk_file_browser_go_tab(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    const tab_t tab = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tab"));
    file_browser->go_tab(tab);
}

void
ptk_file_browser_go_back(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->go_back();
}

void
ptk_file_browser_go_forward(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->go_forward();
}

void
ptk_file_browser_go_up(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->go_up();
}

void
ptk_file_browser_refresh(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->refresh();
}

void
ptk_file_browser_new_tab(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->new_tab();
}

void
ptk_file_browser_new_tab_here(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->new_tab_here();
}

void
ptk_file_browser_close_tab(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->close_tab();
}

void
ptk_file_browser_restore_tab(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->restore_tab();
}

void
ptk_file_browser_set_default_folder(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->set_default_folder();
}

void
ptk_file_browser_select_all(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->select_all();
}

void
ptk_file_browser_unselect_all(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->unselect_all();
}

void
ptk_file_browser_invert_selection(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    file_browser->invert_selection();
}

void
ptk_file_browser_focus(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    const i32 job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job"));
    file_browser->focus(job);
}

bool
ptk_file_browser_slider_release(GtkWidget* widget, GdkEvent* event, PtkFileBrowser* file_browser)
{
    (void)event;
    return file_browser->slider_release(GTK_PANED(widget));
}
