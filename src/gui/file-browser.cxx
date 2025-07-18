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

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <cassert>
#include <cmath>
#include <cstring>

#include <fnmatch.h>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include "datatypes/external-dialog.hxx"

#include "gui/utils/history.hxx"

#if defined(USE_EXO) && (GTK_MAJOR_VERSION == 3)
#include <exo/exo.h>
#endif

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "compat/gtk4-porting.hxx"

#include "datatypes/datatypes.hxx"

#include "utils/permissions.hxx"
#include "utils/strdup.hxx"

#include "settings/settings.hxx"

#include "xset/xset-context-menu.hxx"
#include "xset/xset.hxx"

#include "gui/clipboard.hxx"
#include "gui/dir-tree.hxx"
#include "gui/file-browser.hxx"
#include "gui/file-list.hxx"
#include "gui/file-menu.hxx"
#include "gui/main-window.hxx"
#include "gui/path-bar.hxx"
#include "gui/search-bar.hxx"
#include "gui/utils/utils.hxx"

#include "gui/action/open.hxx"
#include "gui/action/paste.hxx"

#include "gui/view/bookmark.hxx"
#include "gui/view/dir-tree.hxx"
#include "gui/view/file-task.hxx"
#include "gui/view/location.hxx"

#include "gui/dialog/properties.hxx"
#include "gui/dialog/rename-batch.hxx"
#include "gui/dialog/rename.hxx"
#include "gui/dialog/text.hxx"

#include "vfs/dir.hxx"
#include "vfs/file.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/utils/utils.hxx"

#include "autosave.hxx"
#include "logger.hxx"
#include "package.hxx"
#include "types.hxx"

#if defined(USE_EXO) && (GTK_MAJOR_VERSION == 4)
#undef USE_EXO
#endif

struct PtkFileBrowserClass
{
    GtkPanedClass parent;
};

static GType gui_browser_get_type() noexcept;

#define PTK_TYPE_FILE_BROWSER (gui_browser_get_type())

static void gui_browser_class_init(PtkFileBrowserClass* klass) noexcept;
static void gui_browser_init(gui::browser* browser) noexcept;
static void gui_browser_finalize(GObject* obj) noexcept;
static void gui_browser_get_property(GObject* obj, std::uint32_t prop_id, GValue* value,
                                     GParamSpec* pspec) noexcept;
static void gui_browser_set_property(GObject* obj, std::uint32_t prop_id, const GValue* value,
                                     GParamSpec* pspec) noexcept;

/* Utility functions */
static GtkWidget* create_folder_view(gui::browser* browser,
                                     gui::browser::view_mode view_mode) noexcept;
static void init_list_view(gui::browser* browser, GtkTreeView* list_view) noexcept;
static GtkWidget* gui_browser_create_dir_tree(gui::browser* browser) noexcept;

/* Get GtkTreePath of the item at coordinate x, y */
static GtkTreePath* folder_view_get_tree_path_at_pos(gui::browser* browser, std::int32_t x,
                                                     std::int32_t y) noexcept;

/* signal handlers */

#if defined(USE_EXO)
static void on_folder_view_item_activated(ExoIconView* iconview, GtkTreePath* path,
                                          gui::browser* browser) noexcept;
static void on_folder_view_item_sel_change(ExoIconView* iconview, gui::browser* browser) noexcept;
#else
static void on_folder_view_item_activated(GtkIconView* iconview, GtkTreePath* path,
                                          gui::browser* browser) noexcept;
static void on_folder_view_item_sel_change(GtkIconView* iconview, gui::browser* browser) noexcept;
#endif

static void on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path,
                                         GtkTreeViewColumn* col, gui::browser* browser) noexcept;

static bool on_folder_view_button_press_event(GtkWidget* widget, GdkEvent* event,
                                              gui::browser* browser) noexcept;
static bool on_folder_view_button_release_event(GtkWidget* widget, GdkEvent* event,
                                                gui::browser* browser) noexcept;
static bool on_folder_view_popup_menu(GtkWidget* widget, gui::browser* browser) noexcept;

static void on_dir_tree_row_activated(GtkTreeView* view, GtkTreePath* path,
                                      GtkTreeViewColumn* column, gui::browser* browser) noexcept;

/* Drag & Drop */
static void on_folder_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context,
                                              std::int32_t x, std::int32_t y,
                                              GtkSelectionData* sel_data, std::uint32_t info,
                                              std::time_t time, void* user_data) noexcept;
static void on_folder_view_drag_data_get(GtkWidget* widget, GdkDragContext* drag_context,
                                         GtkSelectionData* sel_data, std::uint32_t info,
                                         std::time_t time, gui::browser* browser) noexcept;
static void on_folder_view_drag_begin(GtkWidget* widget, GdkDragContext* drag_context,
                                      gui::browser* browser) noexcept;
static bool on_folder_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context,
                                       std::int32_t x, std::int32_t y, std::time_t time,
                                       gui::browser* browser) noexcept;
static bool on_folder_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context,
                                      std::time_t time, gui::browser* browser) noexcept;
static bool on_folder_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context,
                                     std::int32_t x, std::int32_t y, std::time_t time,
                                     gui::browser* browser) noexcept;
static void on_folder_view_drag_end(GtkWidget* widget, GdkDragContext* drag_context,
                                    gui::browser* browser) noexcept;

static gui::file_list::column
file_list_order_from_sort_order(const gui::browser::sort_order order) noexcept;

static GtkPanedClass* parent_class = nullptr;

static u32 folder_view_auto_scroll_timer = 0;
static GtkDirectionType folder_view_auto_scroll_direction = GtkDirectionType::GTK_DIR_TAB_FORWARD;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {{utils::strdup("text/uri-list"), 0, 0}};

struct column_data
{
    std::string_view title;
    xset::panel xset_name;
    gui::file_list::column column;
};

namespace global
{
// history of closed tabs
static std::unordered_map<panel_t, std::vector<std::filesystem::path>> closed_tabs_restore{};

// must match ipc-command.cxx run_ipc_command()
static constexpr std::array<column_data, 12> columns{
    {{
         "Name",
         xset::panel::detcol_name,
         gui::file_list::column::name,
     },
     {
         "Size",
         xset::panel::detcol_size,
         gui::file_list::column::size,
     },
     {
         "Size in Bytes",
         xset::panel::detcol_bytes,
         gui::file_list::column::bytes,
     },
     {
         "Type",
         xset::panel::detcol_type,
         gui::file_list::column::type,
     },
     {
         "MIME Type",
         xset::panel::detcol_mime,
         gui::file_list::column::mime,
     },
     {
         "Permissions",
         xset::panel::detcol_perm,
         gui::file_list::column::perm,
     },
     {
         "Owner",
         xset::panel::detcol_owner,
         gui::file_list::column::owner,
     },
     {
         "Group",
         xset::panel::detcol_group,
         gui::file_list::column::group,
     },
     {
         "Date Accessed",
         xset::panel::detcol_atime,
         gui::file_list::column::atime,
     },
     {
         "Date Created",
         xset::panel::detcol_btime,
         gui::file_list::column::btime,
     },
     {
         "Date Metadata Change",
         xset::panel::detcol_ctime,
         gui::file_list::column::ctime,
     },
     {
         "Date Modified",
         xset::panel::detcol_mtime,
         gui::file_list::column::mtime,
     }},
};
} // namespace global

GType
gui_browser_get_type() noexcept
{
    static GType type = G_TYPE_INVALID;
    if (type == G_TYPE_INVALID)
    {
        static const GTypeInfo info = {
            sizeof(PtkFileBrowserClass),
            nullptr,
            nullptr,
            (GClassInitFunc)gui_browser_class_init,
            nullptr,
            nullptr,
            sizeof(gui::browser),
            0,
            (GInstanceInitFunc)gui_browser_init,
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
gui_browser_class_init(PtkFileBrowserClass* klass) noexcept
{
    GObjectClass* object_class = (GObjectClass*)klass;
    parent_class = (GtkPanedClass*)g_type_class_peek_parent(klass);

    object_class->set_property = gui_browser_set_property;
    object_class->get_property = gui_browser_get_property;
    object_class->finalize = gui_browser_finalize;
}

bool
gui::browser::using_large_icons() const noexcept
{
    return this->large_icons_;
}

bool
gui::browser::pending_drag_status_tree() const noexcept
{
    return this->pending_drag_status_tree_;
}

void
gui::browser::pending_drag_status_tree(bool val) noexcept
{
    this->pending_drag_status_tree_ = val;
}

bool
gui::browser::is_sort_type(GtkSortType type) const noexcept
{
    return this->sort_type_ == type;
}

bool
gui::browser::is_sort_order(const gui::browser::sort_order type) const noexcept
{
    return this->sort_order_ == type;
}

bool
gui::browser::is_view_mode(const gui::browser::view_mode type) const noexcept
{
    return this->view_mode_ == type;
}

GtkWidget*
gui::browser::folder_view() const noexcept
{
    return this->folder_view_;
}

void
gui::browser::folder_view(GtkWidget* new_folder_view) noexcept
{
    this->folder_view_ = new_folder_view;
}

GtkScrolledWindow*
gui::browser::folder_view_scroll() const noexcept
{
    return this->folder_view_scroll_;
}

GtkCellRenderer*
gui::browser::icon_render() const noexcept
{
    return this->icon_render_;
}

panel_t
gui::browser::panel() const noexcept
{
    return this->panel_;
}

GtkWidget*
gui::browser::task_view() const noexcept
{
    return this->task_view_;
}

MainWindow*
gui::browser::main_window() const noexcept
{
    return this->main_window_;
}

GtkEntry*
gui::browser::path_bar() const noexcept
{
    return this->path_bar_;
}

GtkEntry*
gui::browser::search_bar() const noexcept
{
    return this->search_bar_;
}

static bool
on_search_bar_focus_in(GtkWidget* entry, GdkEventFocus* evt, gui::browser* browser)
{
    (void)entry;
    (void)evt;
    browser->focus_me();
    return false;
}

static void
on_search_bar_activate(GtkWidget* entry, gui::browser* browser)
{
    (void)browser;

    const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (text.empty())
    {
        return;
    }
}

static bool
on_address_bar_focus_in(GtkWidget* entry, GdkEventFocus* evt, gui::browser* browser) noexcept
{
    (void)entry;
    (void)evt;
    browser->focus_me();
    return false;
}

static void
on_address_bar_activate(GtkWidget* entry, gui::browser* browser) noexcept
{
    const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (text.empty())
    {
        return;
    }

    gtk_editable_select_region(GTK_EDITABLE(entry), 0, 0); // clear selection

    // network path
    if ((!text.starts_with('/') && text.contains(":/")) || text.starts_with("//"))
    {
        gui::view::location::mount_network(browser, text, false, false);
        return;
    }

    if (!std::filesystem::exists(text))
    {
        return;
    }
    const auto dir_path = std::filesystem::canonical(text);

    if (std::filesystem::is_directory(dir_path))
    { // open dir
        if (!std::filesystem::equivalent(dir_path, browser->cwd()))
        {
            browser->chdir(dir_path);
        }
    }
    else if (std::filesystem::is_regular_file(dir_path))
    { // open dir and select file
        const auto dirname_path = dir_path.parent_path();
        if (!std::filesystem::equivalent(dirname_path, browser->cwd()))
        {
            browser->chdir(dirname_path);
        }
        else
        {
            browser->select_file(dir_path);
        }
    }
    else if (std::filesystem::is_block_file(dir_path))
    { // open block device
        // logger::info<logger::domain::gui>("opening block device: {}", dir_path);
        gui::view::location::open_block(dir_path, false);
    }
    else
    { // do nothing for other special files
        // logger::info<logger::domain::gui>("special file ignored: {}", dir_path);
        // return;
    }

    gtk_widget_grab_focus(GTK_WIDGET(browser->folder_view_));
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
}

static bool
on_tool_icon_button_press(GtkWidget* widget, GdkEvent* event, const xset_t& set) noexcept
{
    const auto button = gdk_button_event_get_button(event);
    // logger::info<logger::domain::gui>("on_tool_icon_button_press  {}   button = {}", set->menu.label.value_or(""), button);

    const auto type = gdk_event_get_event_type(event);
    if (type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }
    const auto keymod = gui::utils::get_keymod(gdk_event_get_modifier_state(event));

    // get and focus browser
    auto* browser = static_cast<gui::browser*>(g_object_get_data(G_OBJECT(widget), "browser"));
    browser->focus_me();
    set->browser = browser;

    if (button == GDK_BUTTON_PRIMARY && keymod == 0)
    { // left click and no modifier
        // logger::debug<logger::domain::gui>("set={}  menu={}", set->name(), magic_enum::enum_name(set->menu.type));
        set->browser->on_action(set->xset_name);
        return true;
    }
    return true;
}

static GtkButton*
add_toolbar_item(gui::browser* browser, GtkBox* toolbar, const xset::name item) noexcept
{
    const auto set = xset::set::get(item);
    set->browser = browser;

    const auto icon_size = browser->settings_->icon_size_tool;

    // get real icon size from gtk icon size
    i32 icon_w = 0;
    i32 icon_h = 0;
    gtk_icon_size_lookup((GtkIconSize)icon_size.data(), icon_w.unwrap(), icon_h.unwrap());

    GtkWidget* image = nullptr;

    // builtin tool item
    if (set->icon)
    {
        image = gtk_image_new_from_icon_name(set->icon->data(), (GtkIconSize)icon_size.data());
    }
    else
    {
        logger::warn<logger::domain::gui>("set missing icon {}", set->name());
        image =
            gtk_image_new_from_icon_name("application-x-executable", (GtkIconSize)icon_size.data());
    }

    GtkButton* button = GTK_BUTTON(gtk_button_new());
    gtk_widget_show(GTK_WIDGET(image));
    gtk_button_set_image(button, image);
    gtk_button_set_always_show_image(button, true);
    gtk_button_set_relief(button, GTK_RELIEF_NONE);

    // clang-format off
        g_signal_connect(G_OBJECT(button), "button-press-event", G_CALLBACK(on_tool_icon_button_press), set.get());
    // clang-format on

    g_object_set_data(G_OBJECT(button), "browser", browser);

    gtk_box_pack_start(toolbar, GTK_WIDGET(button), false, false, 0);

    return button;
}

void
gui::browser::rebuild_toolbox() noexcept
{
    // logger::info<logger::domain::gui>("rebuild_toolbox");

    this->path_bar_ = gui::path_bar_new(this);
    // clang-format off
    g_signal_connect(G_OBJECT(this->path_bar_), "activate", G_CALLBACK(on_address_bar_activate), this);
    g_signal_connect(G_OBJECT(this->path_bar_), "focus-in-event", G_CALLBACK(on_address_bar_focus_in), this);
    // clang-format on

    this->search_bar_ = gui::search_bar_new(this);
    // clang-format off
    g_signal_connect(G_OBJECT(this->path_bar_), "activate", G_CALLBACK(on_search_bar_activate), this);
    g_signal_connect(G_OBJECT(this->path_bar_), "focus-in-event", G_CALLBACK(on_search_bar_focus_in), this);
    // clang-format on

    this->toolbar_back = add_toolbar_item(this, this->toolbar, xset::name::go_back);
    this->toolbar_forward = add_toolbar_item(this, this->toolbar, xset::name::go_forward);
    this->toolbar_up = add_toolbar_item(this, this->toolbar, xset::name::go_up);
    this->toolbar_home = add_toolbar_item(this, this->toolbar, xset::name::go_home);
    this->toolbar_refresh = add_toolbar_item(this, this->toolbar, xset::name::view_refresh);

    // add pathbar
    gtk_box_pack_start(this->toolbar, GTK_WIDGET(this->path_bar_), true, true, 5);

    // add searchbar
    gtk_widget_set_size_request(GTK_WIDGET(this->search_bar_), 300, -1);
    gtk_box_pack_start(GTK_BOX(this->toolbar), GTK_WIDGET(this->search_bar_), false, true, 5);
}

static bool
on_status_bar_button_press(GtkWidget* widget, GdkEvent* event, gui::browser* browser) noexcept
{
    (void)widget;
    browser->focus_folder_view();

    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);

    if (type == GdkEventType::GDK_BUTTON_PRESS)
    {
        if (button == GDK_BUTTON_MIDDLE)
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
                    const auto selected_files = browser->selected_files();
                    if (selected_files.empty())
                    {
                        return true;
                    }

                    if (i == 0)
                    {
                        gui::clipboard::copy_name(selected_files);
                    }
                    else
                    {
                        gui::clipboard::copy_as_text(selected_files);
                    }
                }
                else if (i == 2)
                { // Scroll Wheel click
                    gui::dialog::properties(browser->cwd(), browser->selected_files(), 0);
                }
                else if (i == 3)
                {
                    browser->main_window_->focus_panel(panel_control_code_hide);
                }
            }
            return true;
        }
    }
    return false;
}

static void
on_status_effect_change(GtkMenuItem* item, gui::browser* browser) noexcept
{
    (void)item;
    set_panel_focus(nullptr, browser);
}

static void
on_status_middle_click_config(GtkMenuItem* menuitem, const xset_t& set) noexcept
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
            set->b = xset::set::enabled::yes;
        }
        else
        {
            xset_set_b(setname, false);
        }
    }
}

static void
on_status_bar_popup(GtkWidget* widget, GtkWidget* menu, gui::browser* browser) noexcept
{
    (void)widget;

    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_t set_radio = nullptr;

    {
        const auto set = xset::set::get(xset::name::status_name);
        xset_set_cb(xset::name::status_name, (GFunc)on_status_middle_click_config, set.get());
        set->menu.radio_set = nullptr;
        set_radio = set;
    }

    {
        const auto set = xset::set::get(xset::name::status_path);
        xset_set_cb(xset::name::status_path, (GFunc)on_status_middle_click_config, set.get());
        set->menu.radio_set = set_radio;
    }

    {
        const auto set = xset::set::get(xset::name::status_info);
        xset_set_cb(xset::name::status_info, (GFunc)on_status_middle_click_config, set.get());
        set->menu.radio_set = set_radio;
    }

    {
        const auto set = xset::set::get(xset::name::status_hide);
        xset_set_cb(xset::name::status_hide, (GFunc)on_status_middle_click_config, set.get());
        set->menu.radio_set = set_radio;
    }

    xset_add_menu(browser, menu, accel_group, {xset::name::separator, xset::name::status_middle});
    gtk_widget_show_all(GTK_WIDGET(menu));
}

static void
gui_browser_init(gui::browser* browser) noexcept
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(browser),
                                   GtkOrientation::GTK_ORIENTATION_VERTICAL);

    browser->panel_ = 0; // do not load font yet in gui_path_entry_new

    // toolbox
    browser->toolbar = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_margin_start(GTK_WIDGET(browser->toolbar), 0);
    gtk_widget_set_margin_end(GTK_WIDGET(browser->toolbar), 0);
    gtk_widget_set_margin_top(GTK_WIDGET(browser->toolbar), 2);
    gtk_widget_set_margin_bottom(GTK_WIDGET(browser->toolbar), 2);
    gtk_box_pack_start(GTK_BOX(browser), GTK_WIDGET(browser->toolbar), false, false, 0);

    // lists area
    browser->hpane = GTK_PANED(gtk_paned_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL));
    browser->side_vbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_size_request(GTK_WIDGET(browser->side_vbox), 140, -1);
    browser->folder_view_scroll_ = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    gtk_paned_pack1(browser->hpane, GTK_WIDGET(browser->side_vbox), false, false);
    gtk_paned_pack2(browser->hpane, GTK_WIDGET(browser->folder_view_scroll_), true, true);

    // fill side
    browser->side_toolbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    browser->side_vpane_top = GTK_PANED(gtk_paned_new(GtkOrientation::GTK_ORIENTATION_VERTICAL));
    browser->side_vpane_bottom = GTK_PANED(gtk_paned_new(GtkOrientation::GTK_ORIENTATION_VERTICAL));
    browser->side_dir_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    browser->side_dev_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    gtk_box_pack_start(browser->side_vbox, GTK_WIDGET(browser->side_toolbox), false, false, 0);
    gtk_box_pack_start(browser->side_vbox, GTK_WIDGET(browser->side_vpane_top), true, true, 0);
    // see https://github.com/BwackNinja/spacefm/issues/21
    gtk_paned_pack1(browser->side_vpane_top, GTK_WIDGET(browser->side_dev_scroll), false, false);
    gtk_paned_pack2(browser->side_vpane_top, GTK_WIDGET(browser->side_vpane_bottom), true, false);
    gtk_paned_pack2(browser->side_vpane_bottom, GTK_WIDGET(browser->side_dir_scroll), true, false);

    // status bar
    browser->statusbar = GTK_STATUSBAR(gtk_statusbar_new());
    // too much padding
    gtk_widget_set_margin_top(GTK_WIDGET(browser->statusbar), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(browser->statusbar), 0);
    browser->statusbar_label = GTK_LABEL(gtk_label_new(""));

    // required for button event
    gtk_label_set_selectable(browser->statusbar_label, true);
    gtk_widget_set_can_focus(GTK_WIDGET(browser->statusbar_label), false);
    gtk_widget_set_hexpand(GTK_WIDGET(browser->statusbar_label), true);
    gtk_widget_set_halign(GTK_WIDGET(browser->statusbar_label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(browser->statusbar_label), GtkAlign::GTK_ALIGN_CENTER);

    // clang-format off
    g_signal_connect(G_OBJECT(browser->statusbar_label), "button-press-event", G_CALLBACK(on_status_bar_button_press), browser);
    g_signal_connect(G_OBJECT(browser->statusbar_label), "populate-popup", G_CALLBACK(on_status_bar_popup), browser);
    // clang-format on

    // pack fb vbox
    gtk_box_pack_start(GTK_BOX(browser), GTK_WIDGET(browser->hpane), true, true, 0);
    // TODO pack task frames
    gtk_box_pack_start(GTK_BOX(browser), GTK_WIDGET(browser->statusbar), false, false, 0);

    gtk_scrolled_window_set_policy(browser->folder_view_scroll_,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_policy(browser->side_dir_scroll,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_policy(browser->side_dev_scroll,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);

    // clang-format off
    g_signal_connect(G_OBJECT(browser->hpane), "button-release-event", G_CALLBACK(gui::wrapper::browser::slider_release), browser);
    g_signal_connect(G_OBJECT(browser->side_vpane_top), "button-release-event", G_CALLBACK(gui::wrapper::browser::slider_release), browser);
    g_signal_connect(G_OBJECT(browser->side_vpane_bottom), "button-release-event", G_CALLBACK(gui::wrapper::browser::slider_release), browser);
    // clang-format on

    browser->history_ = std::make_unique<gui::utils::history>();
}

static void
gui_browser_finalize(GObject* obj) noexcept
{
    gui::browser* browser = PTK_FILE_BROWSER_REINTERPRET(obj);
    // logger::info<logger::domain::gui>("gui_browser_finalize");

    browser->dir_ = nullptr;

    /* Remove all idle handlers which are not called yet. */
    do
    {
    } while (g_source_remove_by_user_data(browser));

    if (browser->file_list_)
    {
        g_signal_handlers_disconnect_matched(browser->file_list_,
                                             GSignalMatchType::G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             browser);
        g_object_unref(G_OBJECT(browser->file_list_));
    }

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
gui_browser_get_property(GObject* obj, std::uint32_t prop_id, GValue* value,
                         GParamSpec* pspec) noexcept
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static void
gui_browser_set_property(GObject* obj, std::uint32_t prop_id, const GValue* value,
                         GParamSpec* pspec) noexcept
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

GtkWidget*
gui_browser_new(i32 curpanel, GtkNotebook* notebook, GtkWidget* task_view, MainWindow* main_window,
                const std::shared_ptr<config::settings>& settings) noexcept
{
    auto* browser = static_cast<gui::browser*>(g_object_new(PTK_TYPE_FILE_BROWSER, nullptr));

    browser->settings_ = settings;

    browser->panel_ = curpanel;
    browser->notebook_ = notebook;
    browser->task_view_ = task_view;
    browser->main_window_ = main_window;

    if (xset_get_b_panel(curpanel, xset::panel::list_detailed))
    {
        browser->view_mode_ = gui::browser::view_mode::list_view;
    }
    else if (xset_get_b_panel(curpanel, xset::panel::list_icons))
    {
        browser->view_mode_ = gui::browser::view_mode::icon_view;
        gtk_scrolled_window_set_policy(browser->folder_view_scroll_,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC);
    }
    else if (xset_get_b_panel(curpanel, xset::panel::list_compact))
    {
        browser->view_mode_ = gui::browser::view_mode::compact_view;
        gtk_scrolled_window_set_policy(browser->folder_view_scroll_,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC);
    }
    else
    {
        browser->view_mode_ = gui::browser::view_mode::list_view;
        xset_set_panel(curpanel, xset::panel::list_detailed, xset::var::b, "1");
    }

    // Large Icons - option for Detailed and Compact list views
    browser->large_icons_ = browser->view_mode_ == gui::browser::view_mode::icon_view ||
                            xset_get_b_panel_mode(browser->panel_,
                                                  xset::panel::list_large,
                                                  main_window->panel_context.at(browser->panel_));
    browser->folder_view(create_folder_view(browser, browser->view_mode_));

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(browser->folder_view_scroll_),
                                  GTK_WIDGET(browser->folder_view_));

    browser->rebuild_toolbox();
    browser->rebuild_toolbars();

    gtk_widget_show_all(GTK_WIDGET(browser));

    if (!browser->settings_->show_toolbar_home)
    {
        gtk_widget_hide(GTK_WIDGET(browser->toolbar_home));
    }
    if (!browser->settings_->show_toolbar_refresh)
    {
        gtk_widget_hide(GTK_WIDGET(browser->toolbar_refresh));
    }
    if (!browser->settings_->show_toolbar_search)
    {
        gtk_widget_hide(GTK_WIDGET(browser->search_bar_));
    }

    return GTK_IS_WIDGET(browser) ? GTK_WIDGET(browser) : nullptr;
}

void
gui::browser::update_tab_label() noexcept
{
    GtkEventBox* ebox =
        GTK_EVENT_BOX(gtk_notebook_get_tab_label(this->notebook_, GTK_WIDGET(this)));
    GtkBox* box = GTK_BOX(g_object_get_data(G_OBJECT(ebox), "box"));

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

void
gui::browser::on_folder_content_changed(const std::shared_ptr<vfs::file>& file) noexcept
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
        this->signal_change_content_.emit(this);
    }
}

static void
on_sort_col_changed(GtkTreeSortable* sortable, gui::browser* browser) noexcept
{
    std::int32_t col = 0;
    gtk_tree_sortable_get_sort_column_id(sortable, &col, &browser->sort_type_);

    const auto column = gui::file_list::column(col);

    static_assert(magic_enum::enum_integer(gui::browser::sort_order::name) ==
                  magic_enum::enum_integer(gui::file_list::column::name) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::size) ==
                  magic_enum::enum_integer(gui::file_list::column::size) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::bytes) ==
                  magic_enum::enum_integer(gui::file_list::column::bytes) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::type) ==
                  magic_enum::enum_integer(gui::file_list::column::type) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::mime) ==
                  magic_enum::enum_integer(gui::file_list::column::mime) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::perm) ==
                  magic_enum::enum_integer(gui::file_list::column::perm) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::owner) ==
                  magic_enum::enum_integer(gui::file_list::column::owner) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::group) ==
                  magic_enum::enum_integer(gui::file_list::column::group) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::atime) ==
                  magic_enum::enum_integer(gui::file_list::column::atime) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::btime) ==
                  magic_enum::enum_integer(gui::file_list::column::btime) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::ctime) ==
                  magic_enum::enum_integer(gui::file_list::column::ctime) - 2);
    static_assert(magic_enum::enum_integer(gui::browser::sort_order::mtime) ==
                  magic_enum::enum_integer(gui::file_list::column::mtime) - 2);
    assert(column != gui::file_list::column::big_icon);
    assert(column != gui::file_list::column::small_icon);
    assert(column != gui::file_list::column::info);

    browser->sort_order_ = gui::browser::sort_order(magic_enum::enum_integer(column) - 2);

    xset_set_panel(browser->panel_,
                   xset::panel::list_detailed,
                   xset::var::x,
                   std::format("{}", magic_enum::enum_integer(browser->sort_order_)));
    xset_set_panel(browser->panel_,
                   xset::panel::list_detailed,
                   xset::var::y,
                   std::format("{}", magic_enum::enum_integer(browser->sort_type_)));
}

void
gui::browser::update_model(const std::string_view pattern) noexcept
{
    gui::file_list* list =
        gui::file_list::create(this->dir_, this->show_hidden_files_, pattern.data());
    assert(list != nullptr);
    GtkTreeModel* old_list = this->file_list_;
    this->file_list_ = GTK_TREE_MODEL(list);
    if (old_list)
    {
        g_object_unref(G_OBJECT(old_list));
    }

    // set file sorting settings
    list->sort_natural = xset_get_b_panel(this->panel_, xset::panel::sort_extra);
    list->sort_case =
        xset_get_int_panel(this->panel_, xset::panel::sort_extra, xset::var::x).data() ==
        xset::set::enabled::yes;
    list->sort_dir_ = gui::file_list::sort_dir(
        xset_get_int_panel(this->panel_, xset::panel::sort_extra, xset::var::y).data());
    list->sort_hidden_first =
        xset_get_int_panel(this->panel_, xset::panel::sort_extra, xset::var::z).data() ==
        xset::set::enabled::yes;

    gtk_tree_sortable_set_sort_column_id(
        GTK_TREE_SORTABLE(list),
        magic_enum::enum_integer(file_list_order_from_sort_order(this->sort_order_)),
        this->sort_type_);

    this->show_thumbnails(this->max_thumbnail_);

    // clang-format off
    g_signal_connect(G_OBJECT(list), "sort-column-changed", G_CALLBACK(on_sort_col_changed), this);
    // clang-format on

    switch (this->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            exo_icon_view_set_model(EXO_ICON_VIEW(this->folder_view_), GTK_TREE_MODEL(list));
#else
            gtk_icon_view_set_model(GTK_ICON_VIEW(this->folder_view_), GTK_TREE_MODEL(list));
#endif
            break;
        case gui::browser::view_mode::list_view:
            gtk_tree_view_set_model(GTK_TREE_VIEW(this->folder_view_), GTK_TREE_MODEL(list));
            break;
    }
}

void
gui::browser::on_dir_file_listed() noexcept
{
    this->n_selected_files_ = 0;

    this->signal_file_created_.disconnect();
    this->signal_file_changed_.disconnect();
    this->signal_file_deleted_.disconnect();

    this->signal_file_created_ = this->dir_->signal_file_created().connect(
        [this](auto a) { this->on_folder_content_changed(a); });
    this->signal_file_changed_ = this->dir_->signal_file_changed().connect(
        [this](auto a) { this->on_folder_content_changed(a); });
    this->signal_file_deleted_ = this->dir_->signal_file_deleted().connect(
        [this](auto a) { this->on_folder_content_changed(a); });

    this->update_model();

    this->signal_chdir_after_.emit(this);
    this->signal_change_content_.emit(this);
    this->signal_change_selection_.emit(this);

    if (this->side_dir)
    {
        gui::view::dir_tree::chdir(GTK_TREE_VIEW(this->side_dir), this->cwd());
    }

    if (this->side_dev)
    {
        gui::view::location::chdir(GTK_TREE_VIEW(this->side_dev), this->cwd());
    }
}

/* signal handlers */

static void
on_folder_view_item_activated(
#if defined(USE_EXO)
    ExoIconView* iconview,
#else
    GtkIconView* iconview,
#endif
    GtkTreePath* path, gui::browser* browser) noexcept
{
    (void)iconview;
    (void)path;
    browser->open_selected_files();
}

static void
on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* col,
                             gui::browser* browser) noexcept
{
    (void)tree_view;
    (void)path;
    (void)col;
    // browser->button_press = false;
    browser->open_selected_files();
}

static bool
on_folder_view_item_sel_change_idle(gui::browser* browser) noexcept
{
    if (!GTK_IS_WIDGET(browser))
    {
        return false;
    }

    browser->n_selected_files_ = 0;
    browser->sel_size_ = 0;
    browser->sel_disk_size_ = 0;

    GtkTreeModel* model = nullptr;
    const std::vector<GtkTreePath*> selected_files = browser->selected_items(&model);

    for (GtkTreePath* sel : selected_files)
    {
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, sel))
        {
            std::shared_ptr<vfs::file> file;
            gtk_tree_model_get(model, &it, gui::file_list::column::info, &file, -1);
            if (file)
            {
                browser->sel_size_ += file->size();
                browser->sel_disk_size_ += file->size_on_disk();
            }
        }
    }

    browser->n_selected_files_ = selected_files.size();

    std::ranges::for_each(selected_files, gtk_tree_path_free);

    browser->signal_change_selection().emit(browser);
    browser->sel_change_idle_ = 0;
    return false;
}

static void
on_folder_view_item_sel_change(
#if defined(USE_EXO)
    ExoIconView* iconview,
#else
    GtkIconView* iconview,
#endif
    gui::browser* browser) noexcept
{
    (void)iconview;
    /* //sfm on_folder_view_item_sel_change fires for each selected file
     * when a file is clicked - causes hang if thousands of files are selected
     * So add only one g_idle_add at a time
     */
    if (browser->sel_change_idle_ != 0)
    {
        return;
    }

    browser->sel_change_idle_ =
        g_idle_add((GSourceFunc)on_folder_view_item_sel_change_idle, browser);
}

static void
show_popup_menu(gui::browser* browser, GdkEvent* event) noexcept
{
    (void)event;

    const auto selected_files = browser->selected_files();

    GtkWidget* popup = gui_file_menu_new(browser, selected_files);
    if (popup)
    {
        gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
    }
}

/* invoke popup menu via shortcut key */
static bool
on_folder_view_popup_menu(GtkWidget* widget, gui::browser* browser) noexcept
{
    (void)widget;
    show_popup_menu(browser, nullptr);
    return true;
}

static bool
on_folder_view_button_press_event(GtkWidget* widget, GdkEvent* event,
                                  gui::browser* browser) noexcept
{
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;
    GtkTreeViewColumn* col = nullptr;
    GtkTreeSelection* selection = nullptr;
    bool ret = false;

    if (browser->menu_shown_)
    {
        browser->menu_shown_ = false;
    }

    const auto keymod = gui::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);

    if (type == GdkEventType::GDK_BUTTON_PRESS)
    {
        browser->focus_folder_view();
        // browser->button_press = true;

        if (button == 4 || button == 5 || button == 8 || button == 9)
        {
            if (button == 4 || button == 8)
            {
                browser->go_back();
            }
            else
            {
                browser->go_forward();
            }
            return true;
        }

        // Alt - Left/Right Click
        if ((keymod.data() == GdkModifierType::GDK_MOD1_MASK) &&
            (button == GDK_BUTTON_PRIMARY || button == GDK_BUTTON_SECONDARY))
        {
            if (button == GDK_BUTTON_PRIMARY)
            {
                browser->go_back();
            }
            else
            {
                browser->go_forward();
            }
            return true;
        }

        double x = NAN;
        double y = NAN;
        gdk_event_get_position(event, &x, &y);

        switch (browser->view_mode_)
        {
            case gui::browser::view_mode::icon_view:
            case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
                tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget),
                                                          static_cast<std::int32_t>(x),
                                                          static_cast<std::int32_t>(y));
                model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
#else
                tree_path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(widget), x, y);
                model = gtk_icon_view_get_model(GTK_ICON_VIEW(widget));
#endif
                /* deselect selected files when right click on blank area */
                if (!tree_path && button == GDK_BUTTON_SECONDARY)
                {
#if defined(USE_EXO)
                    exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
#else
                    gtk_icon_view_unselect_all(GTK_ICON_VIEW(widget));
#endif
                }
                break;
            case gui::browser::view_mode::list_view:
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              static_cast<std::int32_t>(x),
                                              static_cast<std::int32_t>(y),
                                              &tree_path,
                                              &col,
                                              nullptr,
                                              nullptr);
                selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

                if (col &&
                    gui::file_list::column(gtk_tree_view_column_get_sort_column_id(col)) !=
                        gui::file_list::column::name &&
                    tree_path)
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
            gtk_tree_model_get(model, &it, gui::file_list::column::info, &file, -1);
        }

        /* middle button */
        if (file && button == GDK_BUTTON_MIDDLE) /* middle click on a item */
        {
            /* open in new tab if its a directory */
            if (file->is_directory())
            {
                browser->signal_open_file().emit(browser,
                                                 file->path(),
                                                 gui::browser::open_action::new_tab);
            }
            ret = true;
        }
        else if (button == GDK_BUTTON_SECONDARY) /* right click */
        {
            /* cancel all selection, and select the item if it is not selected */
            switch (browser->view_mode_)
            {
                case gui::browser::view_mode::icon_view:
                case gui::browser::view_mode::compact_view:
                    if (tree_path &&
#if defined(USE_EXO)
                        !exo_icon_view_path_is_selected(EXO_ICON_VIEW(widget), tree_path)
#else
                        !gtk_icon_view_path_is_selected(GTK_ICON_VIEW(widget), tree_path)
#endif
                    )
                    {
#if defined(USE_EXO)
                        exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                        exo_icon_view_select_path(EXO_ICON_VIEW(widget), tree_path);
#else
                        gtk_icon_view_unselect_all(GTK_ICON_VIEW(widget));
                        gtk_icon_view_select_path(GTK_ICON_VIEW(widget), tree_path);
#endif
                    }
                    break;
                case gui::browser::view_mode::list_view:
                    if (tree_path && !gtk_tree_selection_path_is_selected(selection, tree_path))
                    {
                        gtk_tree_selection_unselect_all(selection);
                        gtk_tree_selection_select_path(selection, tree_path);
                    }
                    break;
            }

            show_popup_menu(browser, event);
            /* FIXME if approx 5000 are selected, right-click sometimes unselects all
             * after this button_press function returns - why?  a gtk or exo bug?
             * Always happens with above show_popup_menu call disabled
             * Only when this occurs, cursor is automatically set to current row and
             * treesel 'changed' signal fires
             * Stopping changed signal had no effect
             * Using connect rather than connect_after had no effect
             * Removing signal connect had no effect
             * FIX: inhibit button release */
            ret = browser->menu_shown_ = true;
        }
        gtk_tree_path_free(tree_path);
    }
    else if (type == GdkEventType::GDK_2BUTTON_PRESS && button == GDK_BUTTON_PRIMARY)
    {
        // double click event -  button = 0
        if (browser->view_mode_ == gui::browser::view_mode::list_view)
        {
            /* set ret true to prevent drag_begin starting in this tab after
             * fuseiso mount.  Why?
             * row_activated occurs before GdkEventType::GDK_2BUTTON_PRESS so use
             * browser->button_press to determine if row was already
             * activated or user clicked on non-row */
            ret = true;
        }
        else
        {
            /* sfm 1.0.6 set skip_release for Icon/Compact to prevent file
             * under cursor being selected when entering dir with double-click.
             * Also see conditional reset of skip_release in
             * browser->chdir(). See also
             * on_folder_view_button_release_event() */
            browser->skip_release_ = true;
        }
    }
    return ret;
}

static bool
on_folder_view_button_release_event(GtkWidget* widget, GdkEvent* event,
                                    gui::browser* browser) noexcept
{ // on left-click release on file, if not dnd or rubberbanding, unselect files
    (void)widget;
    GtkTreePath* tree_path = nullptr;

    const auto keymod = gui::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto button = gdk_button_event_get_button(event);

    if (browser->is_drag_ || button != 1 || browser->skip_release_ ||
        (keymod.data() & (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK |
                          GdkModifierType::GDK_MOD1_MASK)))
    {
        if (browser->skip_release_)
        {
            browser->skip_release_ = false;
        }
        // this fixes bug where right-click shows menu and release unselects files
        const bool ret = browser->menu_shown_ && button != 1;
        if (browser->menu_shown_)
        {
            browser->menu_shown_ = false;
        }
        return ret;
    }

    gtk_tree_path_free(tree_path);
    return false;
}

static bool
on_dir_tree_update_sel(gui::browser* browser) noexcept
{
    if (!browser->side_dir)
    {
        return false;
    }

    const auto dir_path = gui::view::dir_tree::selected_dir(GTK_TREE_VIEW(browser->side_dir));
    if (dir_path)
    {
        if (!std::filesystem::equivalent(dir_path.value(), browser->cwd()))
        {
            if (browser->chdir(dir_path.value()))
            {
                gtk_entry_set_text(GTK_ENTRY(browser->path_bar_), dir_path->c_str());
            }
        }
    }
    return false;
}

void
on_dir_tree_row_activated(GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column,
                          gui::browser* browser) noexcept
{
    (void)view;
    (void)path;
    (void)column;
    g_idle_add((GSourceFunc)on_dir_tree_update_sel, browser);
}

static void
on_folder_view_columns_changed(GtkTreeView* view, gui::browser* browser) noexcept
{
    // user dragged a column to a different position - save positions
    if (!(GTK_IS_WIDGET(browser) && GTK_IS_TREE_VIEW(view)))
    {
        return;
    }

    if (browser->view_mode_ != gui::browser::view_mode::list_view)
    {
        return;
    }

    for (const auto i : std::views::iota(0uz, global::columns.size()))
    {
        GtkTreeViewColumn* col = gtk_tree_view_get_column(view, static_cast<std::int32_t>(i));
        if (!col)
        {
            return;
        }
        const char* title = gtk_tree_view_column_get_title(col);
        for (const auto column : global::columns)
        {
            if (title == column.title)
            {
                // save column position
                const auto set = xset::set::get(column.xset_name, browser->panel_);
                set->x = std::format("{}", i);

                break;
            }
        }
    }
}

static void
on_folder_view_destroy(GtkTreeView* view, gui::browser* browser) noexcept
{
    (void)browser;
    const auto id = g_signal_lookup("columns-changed", G_TYPE_FROM_INSTANCE(view));
    if (id)
    {
        const auto hand = g_signal_handler_find((void*)view,
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

static GtkWidget*
create_folder_view(gui::browser* browser, gui::browser::view_mode view_mode) noexcept
{
    GtkWidget* folder_view = nullptr;
    GtkTreeSelection* selection = nullptr;
    GtkCellRenderer* renderer = nullptr;

    std::int32_t icon_size = 0;
    const std::int32_t big_icon_size = browser->settings_->icon_size_big.data();
    const std::int32_t small_icon_size = browser->settings_->icon_size_small.data();

    PangoAttrList* attr_list = pango_attr_list_new();
    pango_attr_list_insert(attr_list, pango_attr_insert_hyphens_new(false));

    switch (view_mode)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            folder_view = exo_icon_view_new();
#else
            folder_view = gtk_icon_view_new();
#endif

            if (view_mode == gui::browser::view_mode::compact_view)
            {
                icon_size = browser->large_icons_ ? big_icon_size : small_icon_size;

#if defined(USE_EXO)
                exo_icon_view_set_layout_mode(EXO_ICON_VIEW(folder_view),
                                              EXO_ICON_VIEW_LAYOUT_COLS);
                exo_icon_view_set_orientation(EXO_ICON_VIEW(folder_view),
                                              GtkOrientation::GTK_ORIENTATION_HORIZONTAL);
#else
                // gtk_icon_view_set_layout_mode(GTK_ICON_VIEW(folder_view),
                //                               GTK_ICON_VIEW_LAYOUT_COLS);
                gtk_icon_view_set_item_orientation(GTK_ICON_VIEW(folder_view),
                                                   GtkOrientation::GTK_ORIENTATION_HORIZONTAL);
#endif
            }
            else
            {
                icon_size = big_icon_size;

#if defined(USE_EXO)
                exo_icon_view_set_column_spacing(EXO_ICON_VIEW(folder_view), 4);
                exo_icon_view_set_item_width(EXO_ICON_VIEW(folder_view),
                                             icon_size < 110 ? 110 : icon_size);
#else
                gtk_icon_view_set_column_spacing(GTK_ICON_VIEW(folder_view), 4);
                gtk_icon_view_set_item_width(GTK_ICON_VIEW(folder_view),
                                             icon_size < 110 ? 110 : icon_size);
#endif
            }

#if defined(USE_EXO)
            exo_icon_view_set_selection_mode(EXO_ICON_VIEW(folder_view),
                                             GtkSelectionMode::GTK_SELECTION_MULTIPLE);
#else
            gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(folder_view),
                                             GtkSelectionMode::GTK_SELECTION_MULTIPLE);
#endif

            // search
#if defined(USE_EXO)
            exo_icon_view_set_enable_search(EXO_ICON_VIEW(folder_view), false);
#endif

            gtk_cell_layout_clear(GTK_CELL_LAYOUT(folder_view));

            browser->icon_render_ = renderer = gtk_cell_renderer_pixbuf_new();

            /* add the icon renderer */
            g_object_set(G_OBJECT(renderer), "follow_state", true, nullptr);
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(folder_view), renderer, false);
            gtk_cell_layout_add_attribute(
                GTK_CELL_LAYOUT(folder_view),
                renderer,
                "pixbuf",
                browser->large_icons_
                    ? magic_enum::enum_integer(gui::file_list::column::big_icon)
                    : magic_enum::enum_integer(gui::file_list::column::small_icon));

            /* add the name renderer */
            renderer = gtk_cell_renderer_text_new();

            if (view_mode == gui::browser::view_mode::compact_view)
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
                                          magic_enum::enum_integer(gui::file_list::column::name));

#if defined(USE_EXO)
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
#else
            gtk_icon_view_enable_model_drag_source(
                GTK_ICON_VIEW(folder_view),
                GdkModifierType(GdkModifierType::GDK_CONTROL_MASK |
                                GdkModifierType::GDK_BUTTON1_MASK |
                                GdkModifierType::GDK_BUTTON3_MASK),
                drag_targets,
                G_N_ELEMENTS(drag_targets),
                GDK_ACTION_ALL);

            gtk_icon_view_enable_model_drag_dest(GTK_ICON_VIEW(folder_view),
                                                 drag_targets,
                                                 G_N_ELEMENTS(drag_targets),
                                                 GDK_ACTION_ALL);
#endif

            // clang-format off
            g_signal_connect(G_OBJECT(folder_view), "item-activated", G_CALLBACK(on_folder_view_item_activated), browser);
            g_signal_connect_after(G_OBJECT(folder_view), "selection-changed", G_CALLBACK(on_folder_view_item_sel_change), browser);
            // clang-format on

            break;
        case gui::browser::view_mode::list_view:
            folder_view = gtk_tree_view_new();

            init_list_view(browser, GTK_TREE_VIEW(folder_view));

            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(folder_view));
            gtk_tree_selection_set_mode(selection, GtkSelectionMode::GTK_SELECTION_MULTIPLE);

            if (xset_get_b(xset::name::rubberband))
            {
                gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(folder_view), true);
            }

            // Search
            gtk_tree_view_set_enable_search(GTK_TREE_VIEW(folder_view), false);

            icon_size = browser->large_icons_ ? big_icon_size : small_icon_size;

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
            g_signal_connect(G_OBJECT(folder_view), "row_activated", G_CALLBACK(on_folder_view_row_activated), browser);
            g_signal_connect_after(G_OBJECT(selection), "changed", G_CALLBACK(on_folder_view_item_sel_change), browser);
            g_signal_connect(G_OBJECT(folder_view), "columns-changed", G_CALLBACK(on_folder_view_columns_changed), browser);
            g_signal_connect(G_OBJECT(folder_view), "destroy", G_CALLBACK(on_folder_view_destroy), browser);
            // clang-format on
            break;
    }

    gtk_cell_renderer_set_fixed_size(browser->icon_render_, icon_size, icon_size);

    // clang-format off
    g_signal_connect(G_OBJECT(folder_view), "button-press-event", G_CALLBACK(on_folder_view_button_press_event), browser);
    g_signal_connect(G_OBJECT(folder_view), "button-release-event", G_CALLBACK(on_folder_view_button_release_event), browser);
    g_signal_connect(G_OBJECT(folder_view), "popup-menu", G_CALLBACK(on_folder_view_popup_menu), browser);
    // init drag & drop support
    g_signal_connect(G_OBJECT(folder_view), "drag-data-received", G_CALLBACK(on_folder_view_drag_data_received), browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-data-get", G_CALLBACK(on_folder_view_drag_data_get), browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-begin", G_CALLBACK(on_folder_view_drag_begin), browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-motion", G_CALLBACK(on_folder_view_drag_motion), browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-leave", G_CALLBACK(on_folder_view_drag_leave), browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-drop", G_CALLBACK(on_folder_view_drag_drop), browser);
    g_signal_connect(G_OBJECT(folder_view), "drag-end", G_CALLBACK(on_folder_view_drag_end), browser);
    // clang-format on

    return folder_view;
}

static void
init_list_view(gui::browser* browser, GtkTreeView* list_view) noexcept
{
    const panel_t p = browser->panel_;
    const xset::main_window_panel mode = browser->main_window_->panel_context.at(p);

    for (const auto column : global::columns)
    {
        GtkTreeViewColumn* col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable(col, true);

        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();

        // column order
        std::size_t idx = 0;
        for (const auto [order_index, order_value] : std::views::enumerate(global::columns))
        {
            idx = static_cast<std::size_t>(order_index);
            if (xset_get_int_panel(p, global::columns.at(idx).xset_name, xset::var::x) ==
                magic_enum::enum_integer(column.column))
            {
                break;
            }
        }

        // column width
        gtk_tree_view_column_set_min_width(col, 50);
        gtk_tree_view_column_set_sizing(col, GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
        const auto set = xset::set::get(global::columns.at(idx).xset_name, p, mode);
        const auto width = set->y ? i32::create(set->y.value()).value_or(100_i32) : 100_i32;
        if (width != 0)
        {
            if (column.column == gui::file_list::column::name &&
                !browser->settings_->always_show_tabs &&
                browser->view_mode_ == gui::browser::view_mode::list_view &&
                gtk_notebook_get_n_pages(browser->notebook_) == 1)
            {
                // when tabs are added, the width of the notebook decreases
                // by a few pixels, meaning there is not enough space for
                // all columns - this causes a horizontal scrollbar to
                // appear on new and sometimes first tab
                // so shave some pixels off first columns
                gtk_tree_view_column_set_fixed_width(col, width.data() - 6);

                // below causes increasing reduction of column every time new tab is
                // added and closed - undesirable
                gui::browser* first_fb =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(browser->notebook_, 0));

                if (first_fb && first_fb->view_mode_ == gui::browser::view_mode::list_view &&
                    GTK_IS_TREE_VIEW(first_fb->folder_view_))
                {
                    GtkTreeViewColumn* first_col =
                        gtk_tree_view_get_column(GTK_TREE_VIEW(first_fb->folder_view_), 0);
                    if (first_col)
                    {
                        const i32 first_width = gtk_tree_view_column_get_width(first_col);
                        if (first_width > 10)
                        {
                            gtk_tree_view_column_set_fixed_width(first_col, first_width.data() - 6);
                        }
                    }
                }
            }
            else
            {
                gtk_tree_view_column_set_fixed_width(col, width.data());
                // logger::info<logger::domain::gui>("init set_width {} {}", magic_enum::enum_name(global::columns.at(index).xset_name), width);
            }
        }

        if (column.column == gui::file_list::column::name)
        {
            g_object_set(G_OBJECT(renderer),
                         /* "editable", true, */
                         "ellipsize",
                         PangoEllipsizeMode::PANGO_ELLIPSIZE_END,
                         nullptr);

            // g_signal_connect(G_OBJECT(renderer), "editing-started", G_CALLBACK(on_filename_editing_started), nullptr);

            GtkCellRenderer* pix_renderer = nullptr;
            browser->icon_render_ = pix_renderer = gtk_cell_renderer_pixbuf_new();

            gtk_tree_view_column_pack_start(col, pix_renderer, false);
            gtk_tree_view_column_set_attributes(col,
                                                pix_renderer,
                                                "pixbuf",
                                                browser->large_icons_
                                                    ? gui::file_list::column::big_icon
                                                    : gui::file_list::column::small_icon,
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

        if (column.column == gui::file_list::column::size ||
            column.column == gui::file_list::column::bytes)
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
folder_view_get_drop_dir(gui::browser* browser, i32 x, i32 y) noexcept
{
    GtkTreePath* tree_path = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeViewColumn* col = nullptr;
    GtkTreeIter it;

    switch (browser->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
            gtk_icon_view_convert_widget_to_bin_window_coords(GTK_ICON_VIEW(browser->folder_view_),
                                                              x.data(),
                                                              y.data(),
                                                              x.unwrap(),
                                                              y.unwrap());
            tree_path = folder_view_get_tree_path_at_pos(browser, x.data(), y.data());
#if defined(USE_EXO)
            model = exo_icon_view_get_model(EXO_ICON_VIEW(browser->folder_view_));
#else
            model = gtk_icon_view_get_model(GTK_ICON_VIEW(browser->folder_view_));
#endif
            break;
        case gui::browser::view_mode::list_view:
            // if drag is in progress, get the dest row path
            gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(browser->folder_view_),
                                            &tree_path,
                                            nullptr);
            if (!tree_path)
            {
                // no drag in progress, get drop path
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(browser->folder_view_),
                                              x.data(),
                                              y.data(),
                                              nullptr,
                                              &col,
                                              nullptr,
                                              nullptr);
                if (col == gtk_tree_view_get_column(GTK_TREE_VIEW(browser->folder_view_), 0))
                {
                    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(browser->folder_view_),
                                                      x.data(),
                                                      y.data(),
                                                      &tree_path,
                                                      nullptr);
                    model = gtk_tree_view_get_model(GTK_TREE_VIEW(browser->folder_view_));
                }
            }
            else
            {
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(browser->folder_view_));
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
        gtk_tree_model_get(model, &it, gui::file_list::column::info, &file, -1);
        if (file)
        {
            if (file->is_directory())
            {
                dest_path = file->path();
            }
            else /* Drop on a file, not directory */
            {
                /* Return current directory */
                dest_path = browser->cwd();
            }
        }
        gtk_tree_path_free(tree_path);
    }
    else
    {
        dest_path = browser->cwd();
    }
    return ::utils::strdup(dest_path.c_str());
}

static void
on_folder_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context, std::int32_t x,
                                  std::int32_t y, GtkSelectionData* sel_data, std::uint32_t info,
                                  std::time_t time, void* user_data) noexcept
{
    (void)widget;
    (void)x;
    (void)y;
    (void)info;

    auto* browser = static_cast<gui::browser*>(user_data);

    if ((gtk_selection_data_get_length(sel_data) >= 0) &&
        (gtk_selection_data_get_format(sel_data) == 8))
    {
        // (list view) use stored x and y because == 0 for update drag status
        //             when is last row (gtk2&3 bug?)
        // and because exo_icon_view has no get_drag_dest_row
        const char* dest_dir =
            folder_view_get_drop_dir(browser, browser->drag_x_, browser->drag_y_);
        // logger::info<logger::domain::gui>("FB DnD dest_dir = {}", dest_dir );
        if (dest_dir)
        {
            if (browser->pending_drag_status_)
            {
                // logger::debug<logger::domain::gui>("DnD DEFAULT");

                // We only want to update drag status, not really want to drop
                gdk_drag_status(drag_context,
                                GdkDragAction::GDK_ACTION_DEFAULT,
                                static_cast<guint32>(time));

                // DnD is still ongoing, do not continue
                browser->pending_drag_status_ = false;
                return;
            }

            char** list = nullptr;
            char** puri = nullptr;
            puri = list = gtk_selection_data_get_uris(sel_data);

            if (puri)
            {
                // We only want to update drag status, not really want to drop
                const auto dest_dir_stat = ztd::stat::create(dest_dir);
                if (dest_dir_stat)
                {
                    const auto dest_dev = dest_dir_stat->dev();
                    const auto dest_inode = dest_dir_stat->ino();
                    if (browser->drag_source_dev_ == 0)
                    {
                        browser->drag_source_dev_ = dest_dev.data();
                        for (; *puri; ++puri)
                        {
                            const std::filesystem::path file_path = Glib::filename_from_uri(*puri);

                            const auto file_path_stat = ztd::stat::create(file_path);
                            if (file_path_stat)
                            {
                                if (file_path_stat->dev() != dest_dev)
                                {
                                    // different devices - store source device
                                    browser->drag_source_dev_ = file_path_stat->dev().data();
                                    break;
                                }
                                else if (browser->drag_source_inode_ == 0)
                                {
                                    // same device - store source parent inode
                                    const auto src_dir = file_path.parent_path();

                                    const auto src_dir_stat = ztd::stat::create(src_dir);
                                    if (src_dir_stat)
                                    {
                                        browser->drag_source_inode_ = src_dir_stat->ino().data();
                                    }
                                }
                            }
                        }
                    }
                    g_strfreev(list);

                    vfs::file_task::type file_action;

                    if (browser->drag_source_dev_ != dest_dev ||
                        browser->drag_source_inode_ == dest_inode)
                    { // src and dest are on different devices or same dir
                        // logger::debug<logger::domain::gui>("DnD COPY");
                        gdk_drag_status(drag_context,
                                        GdkDragAction::GDK_ACTION_COPY,
                                        static_cast<guint32>(time));
                        file_action = vfs::file_task::type::copy;
                    }
                    else
                    {
                        // logger::debug<logger::domain::gui>("DnD MOVE");
                        gdk_drag_status(drag_context,
                                        GdkDragAction::GDK_ACTION_MOVE,
                                        static_cast<guint32>(time));
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

                        file_list.push_back(file_path);
                    }
                    g_strfreev(list);

                    if (!file_list.empty())
                    {
                        // logger::info<logger::domain::gui>("DnD dest_dir = {}", dest_dir);

                        GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(browser));

                        gui::file_task* ptask = gui_file_task_new(file_action,
                                                                  file_list,
                                                                  dest_dir,
                                                                  GTK_WINDOW(parent),
                                                                  browser->task_view_);
                        ptask->run();
                    }
                    gtk_drag_finish(drag_context, true, false, static_cast<guint32>(time));
                    return;
                }
            }
        }
    }

    /* If we are only getting drag status, not finished. */
    if (browser->pending_drag_status_)
    {
        browser->pending_drag_status_ = false;
        return;
    }
    gtk_drag_finish(drag_context, false, false, static_cast<guint32>(time));
}

static void
on_folder_view_drag_data_get(GtkWidget* widget, GdkDragContext* drag_context,
                             GtkSelectionData* sel_data, std::uint32_t info, std::time_t time,
                             gui::browser* browser) noexcept
{
    (void)widget;
    (void)drag_context;
    (void)info;
    (void)time;
    GdkAtom type = gdk_atom_intern("text/uri-list", false);

    std::string uri_list;
    const auto selected_files = browser->selected_files();
    for (const auto& file : selected_files)
    {
        const std::string uri = Glib::filename_to_uri(file->path());
        uri_list.append(std::format("{}\n", uri));
    }

    gtk_selection_data_set(sel_data,
                           type,
                           8,
                           (const unsigned char*)uri_list.data(),
                           static_cast<std::int32_t>(uri_list.size()));
}

static void
on_folder_view_drag_begin(GtkWidget* widget, GdkDragContext* drag_context,
                          gui::browser* browser) noexcept
{
    (void)widget;
    gtk_drag_set_icon_default(drag_context);
    browser->is_drag_ = true;
}

static GtkTreePath*
folder_view_get_tree_path_at_pos(gui::browser* browser, std::int32_t x, std::int32_t y) noexcept
{
    GtkTreePath* tree_path = nullptr;

    switch (browser->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(browser->folder_view_), x, y);
#else
            tree_path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(browser->folder_view_), x, y);
#endif
            break;
        case gui::browser::view_mode::list_view:
            gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(browser->folder_view_),
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
on_folder_view_auto_scroll(GtkScrolledWindow* scroll) noexcept
{
    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);
    double vpos = gtk_adjustment_get_value(vadj);

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
on_folder_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, std::int32_t x,
                           std::int32_t y, std::time_t time, gui::browser* browser) noexcept
{
    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(gtk_widget_get_parent(widget));

    // GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    if (y < 32)
    {
        /* Auto scroll up */
        if (folder_view_auto_scroll_timer == 0)
        {
            folder_view_auto_scroll_direction = GtkDirectionType::GTK_DIR_UP;
            folder_view_auto_scroll_timer =
                g_timeout_add(150, (GSourceFunc)on_folder_view_auto_scroll, scroll);
        }
    }
    else if (y > (allocation.height - 32))
    {
        if (folder_view_auto_scroll_timer == 0)
        {
            folder_view_auto_scroll_direction = GtkDirectionType::GTK_DIR_DOWN;
            folder_view_auto_scroll_timer =
                g_timeout_add(150, (GSourceFunc)on_folder_view_auto_scroll, scroll);
        }
    }
    else if (folder_view_auto_scroll_timer != 0)
    {
        g_source_remove(folder_view_auto_scroll_timer.data());
        folder_view_auto_scroll_timer = 0;
    }

    GtkTreeViewColumn* col = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;

    switch (browser->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
            // store x and y because exo_icon_view has no get_drag_dest_row
            browser->drag_x_ = x;
            browser->drag_y_ = y;
            gtk_icon_view_convert_widget_to_bin_window_coords(GTK_ICON_VIEW(widget), x, y, &x, &y);

#if defined(USE_EXO)
            tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), x, y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
#else
            tree_path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(widget), x, y);
            model = gtk_icon_view_get_model(GTK_ICON_VIEW(widget));
#endif
            break;
        case gui::browser::view_mode::list_view:
            // store x and y because == 0 for update drag status when is last row
            browser->drag_x_ = x;
            browser->drag_y_ = y;
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
            gtk_tree_model_get(model, &it, gui::file_list::column::info, &file, -1);
            if (!file || !file->is_directory())
            {
                gtk_tree_path_free(tree_path);
                tree_path = nullptr;
            }
        }
    }

    switch (browser->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:

#if defined(USE_EXO)
            exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(widget),
                                             tree_path,
                                             EXO_ICON_VIEW_DROP_INTO);
#else
            gtk_icon_view_set_drag_dest_item(GTK_ICON_VIEW(widget),
                                             tree_path,
                                             GTK_ICON_VIEW_DROP_INTO);
#endif
            break;
        case gui::browser::view_mode::list_view:
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
        gdk_drag_status(drag_context, (GdkDragAction)0, static_cast<guint32>(time));
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

            switch (drag_action.data())
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
                    browser->pending_drag_status_ = true;
                    gtk_drag_get_data(widget, drag_context, target, static_cast<guint32>(time));
                    suggested_action = gdk_drag_context_get_selected_action(drag_context);
                    break;
            }
        }
        gdk_drag_status(drag_context, suggested_action, static_cast<guint32>(time));
    }
    return true;
}

static bool
on_folder_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context, std::time_t time,
                          gui::browser* browser) noexcept
{
    (void)widget;
    (void)drag_context;
    (void)time;

    browser->drag_source_dev_ = 0;
    browser->drag_source_inode_ = 0;

    if (folder_view_auto_scroll_timer != 0)
    {
        g_source_remove(folder_view_auto_scroll_timer.data());
        folder_view_auto_scroll_timer = 0;
    }
    return true;
}

static bool
on_folder_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, std::int32_t x,
                         std::int32_t y, std::time_t time, gui::browser* browser) noexcept
{
    (void)x;
    (void)y;
    (void)browser;
    GdkAtom target = gdk_atom_intern("text/uri-list", false);

    gtk_drag_get_data(widget, drag_context, target, static_cast<guint32>(time));
    return true;
}

static void
on_folder_view_drag_end(GtkWidget* widget, GdkDragContext* drag_context,
                        gui::browser* browser) noexcept
{
    (void)drag_context;
    if (folder_view_auto_scroll_timer != 0)
    {
        g_source_remove(folder_view_auto_scroll_timer.data());
        folder_view_auto_scroll_timer = 0;
    }

    switch (browser->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(widget),
                                             nullptr,
                                             (ExoIconViewDropPosition)0);
#else
            gtk_icon_view_set_drag_dest_item(GTK_ICON_VIEW(widget),
                                             nullptr,
                                             (GtkIconViewDropPosition)0);
#endif
            break;
        case gui::browser::view_mode::list_view:
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget),
                                            nullptr,
                                            (GtkTreeViewDropPosition)0);
            break;
    }
    browser->is_drag_ = false;
}

static bool
on_dir_tree_button_press(GtkWidget* view, GdkEvent* event, gui::browser* browser) noexcept
{
    browser->focus_me();

    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);

    if (type == GdkEventType::GDK_BUTTON_PRESS && button == GDK_BUTTON_MIDDLE) /* middle click */
    {
        /* left and right click handled in gui/dir-tree-view.cxx
         * on_dir_tree_view_button_press() */

        double x = NAN;
        double y = NAN;
        gdk_event_get_position(event, &x, &y);

        GtkTreePath* tree_path = nullptr;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          static_cast<std::int32_t>(x),
                                          static_cast<std::int32_t>(y),
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr))
        {
            GtkTreeIter it;
            if (gtk_tree_model_get_iter(model, &it, tree_path))
            {
                std::shared_ptr<vfs::file> file;
                gtk_tree_model_get(model, &it, gui::dir_tree::column::info, &file, -1);
                if (file)
                {
                    const auto file_path = gui::view::dir_tree::dir_path(model, &it);
                    if (file_path)
                    {
                        browser->signal_open_file().emit(browser,
                                                         file_path.value(),
                                                         gui::browser::open_action::new_tab);
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
gui_browser_create_dir_tree(gui::browser* browser) noexcept
{
    GtkWidget* dir_tree = gui::view::dir_tree::create(browser, browser->show_hidden_files_);

    // clang-format off
    g_signal_connect(G_OBJECT(dir_tree), "row-activated", G_CALLBACK(on_dir_tree_row_activated), browser);
    g_signal_connect(G_OBJECT(dir_tree), "button-press-event", G_CALLBACK(on_dir_tree_button_press), browser);
    // clang-format on

    return dir_tree;
}

static gui::file_list::column
file_list_order_from_sort_order(const gui::browser::sort_order order) noexcept
{
    static_assert(magic_enum::enum_integer(gui::file_list::column::name) ==
                  magic_enum::enum_integer(gui::browser::sort_order::name) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::size) ==
                  magic_enum::enum_integer(gui::browser::sort_order::size) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::bytes) ==
                  magic_enum::enum_integer(gui::browser::sort_order::bytes) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::type) ==
                  magic_enum::enum_integer(gui::browser::sort_order::type) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::mime) ==
                  magic_enum::enum_integer(gui::browser::sort_order::mime) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::perm) ==
                  magic_enum::enum_integer(gui::browser::sort_order::perm) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::owner) ==
                  magic_enum::enum_integer(gui::browser::sort_order::owner) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::group) ==
                  magic_enum::enum_integer(gui::browser::sort_order::group) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::atime) ==
                  magic_enum::enum_integer(gui::browser::sort_order::atime) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::btime) ==
                  magic_enum::enum_integer(gui::browser::sort_order::btime) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::ctime) ==
                  magic_enum::enum_integer(gui::browser::sort_order::ctime) + 2);
    static_assert(magic_enum::enum_integer(gui::file_list::column::mtime) ==
                  magic_enum::enum_integer(gui::browser::sort_order::mtime) + 2);

    return gui::file_list::column(magic_enum::enum_integer(order) + 2);
}

/**
 * gui::browser
 */

bool
gui::browser::chdir(const std::filesystem::path& new_path,
                    const gui::utils::history::mode mode) noexcept
{
    // logger::debug<logger::domain::gui>("gui::browser::chdir");

    // this->button_press_ = false;
    this->is_drag_ = false;
    this->menu_shown_ = false;
    if (this->view_mode_ == gui::browser::view_mode::list_view)
    {
        /* sfm 1.0.6 do not reset skip_release for Icon/Compact to prevent file
           under cursor being selected when entering dir with double-click.
           Reset is conditional here to avoid possible but unlikely unintended
           breakage elsewhere. */
        this->skip_release_ = false;
    }

    if (!std::filesystem::exists(new_path))
    {
        // logger::error<logger::domain::gui>("Failed to chdir into nonexistent path '{}'", new_path.string());
        return false;
    }
    const auto path = std::filesystem::absolute(new_path);

    if (!std::filesystem::is_directory(path))
    {
        if (!this->inhibit_focus_)
        {
            gui::dialog::error("Error",
                               std::format("Directory does not exist\n\n{}", path.string()));
        }
        return false;
    }

    if (!::utils::check_directory_permissions(path))
    {
        if (!this->inhibit_focus_)
        {
            gui::dialog::error(
                "Error",
                std::format("Unable to access {}\n\n{}", path.string(), std::strerror(errno)));
        }
        return false;
    }

    this->signal_chdir_before_.emit(this);

    this->update_selection_history();

    switch (mode)
    {
        case gui::utils::history::mode::normal:
            if (this->history_->path() != path)
            {
                this->history_->new_forward(path);
            }
            break;
        case gui::utils::history::mode::history_back:
            this->history_->go_back();
            break;
        case gui::utils::history::mode::history_forward:
            this->history_->go_forward();
            break;
    }

    switch (this->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            exo_icon_view_set_model(EXO_ICON_VIEW(this->folder_view_), nullptr);
#else
            gtk_icon_view_set_model(GTK_ICON_VIEW(this->folder_view_), nullptr);
#endif
            break;
        case gui::browser::view_mode::list_view:
            gtk_tree_view_set_model(GTK_TREE_VIEW(this->folder_view_), nullptr);
            break;
    }

    // load new dir

    this->signal_file_listed_.disconnect();
    this->dir_ = vfs::dir::create(path, this->settings_);

    this->signal_chdir_begin_.emit(this);

    this->signal_file_listed_ =
        this->dir_->signal_file_listed().connect([this]() { this->on_dir_file_listed(); });

    if (this->dir_->is_loaded())
    {
        // TODO - if the dir is loaded from cache then it will not run the file_listed signal.
        // this should be a tmp workaround
        this->on_dir_file_listed();
    }

    this->update_tab_label();

    const auto& cwd = this->cwd();
    if (!this->inhibit_focus_)
    {
        gtk_entry_set_text(GTK_ENTRY(this->path_bar_), cwd.c_str());
    }

    gtk_widget_set_sensitive(GTK_WIDGET(this->toolbar_back), this->history_->has_back());
    gtk_widget_set_sensitive(GTK_WIDGET(this->toolbar_forward), this->history_->has_forward());
    gtk_widget_set_sensitive(GTK_WIDGET(this->toolbar_up), this->cwd() != "/");

    return true;
}

const std::filesystem::path&
gui::browser::cwd() const noexcept
{
    return this->history_->path();
}

void
gui::browser::canon(const std::filesystem::path& path) noexcept
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

std::optional<std::filesystem::path>
gui::browser::tab_cwd(const tab_t tab_num) const noexcept
{
    tab_t tab_x = 0;
    GtkNotebook* notebook = this->main_window_->get_panel_notebook(this->panel());
    const auto pages = gtk_notebook_get_n_pages(notebook);
    const auto page_num = gtk_notebook_page_num(notebook, GTK_WIDGET(this));

    switch (tab_num.data())
    {
        case tab_control_code_prev.data():
            // prev
            tab_x = page_num - 1;
            break;
        case tab_control_code_next.data():
            // next
            tab_x = page_num + 1;
            break;
        default:
            // tab_num starts counting at 1
            tab_x = tab_num - 1;
            break;
    }

    if (tab_x > -1 && tab_x < pages)
    {
        const gui::browser* tab_browser =
            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, tab_x.data()));
        return tab_browser->cwd();
    }

    return std::nullopt;
}

std::optional<std::filesystem::path>
gui::browser::panel_cwd(const panel_t panel_num) const noexcept
{
    panel_t panel_x = this->panel();

    switch (panel_num.data())
    {
        case panel_control_code_prev.data():
            // prev
            do
            {
                panel_x -= 1;
                if (panel_x < 1)
                {
                    panel_x = 4;
                }
                if (panel_x == this->panel())
                {
                    return std::nullopt;
                }
            } while (!gtk_widget_get_visible(
                GTK_WIDGET(this->main_window_->get_panel_notebook(panel_x))));
            break;
        case panel_control_code_next.data():
            // next
            do
            {
                panel_x += 1;
                if (!is_valid_panel(panel_x))
                {
                    panel_x = 1;
                }
                if (panel_x == this->panel())
                {
                    return std::nullopt;
                }
            } while (!gtk_widget_get_visible(
                GTK_WIDGET(this->main_window_->get_panel_notebook(panel_x))));
            break;
        default:
            panel_x = panel_num;
            if (!gtk_widget_get_visible(
                    GTK_WIDGET(this->main_window_->get_panel_notebook(panel_x))))
            {
                return std::nullopt;
            }
            break;
    }

    GtkNotebook* notebook = this->main_window_->get_panel_notebook(panel_x);
    const auto page_x = gtk_notebook_get_current_page(notebook);

    const gui::browser* panel_browser =
        PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, page_x));
    return panel_browser->cwd();
}

void
gui::browser::open_in_panel(const panel_t panel_num,
                            const std::filesystem::path& file_path) noexcept
{
    panel_t panel_x = this->panel();

    switch (panel_num.data())
    {
        case panel_control_code_prev.data():
            // prev
            do
            {
                panel_x -= 1;
                if (!is_valid_panel(panel_x))
                { // loop to end
                    panel_x = 4;
                }
                if (panel_x == this->panel())
                {
                    return;
                }
            } while (!gtk_widget_get_visible(
                GTK_WIDGET(this->main_window_->get_panel_notebook(panel_x))));
            break;
        case panel_control_code_next.data():
            // next
            do
            {
                panel_x += 1;
                if (!is_valid_panel(panel_x))
                { // loop to start
                    panel_x = 1;
                }
                if (panel_x == this->panel())
                {
                    return;
                }
            } while (!gtk_widget_get_visible(
                GTK_WIDGET(this->main_window_->get_panel_notebook(panel_x))));
            break;
        default:
            panel_x = panel_num;
            break;
    }

    if (!is_valid_panel(panel_x))
    {
        return;
    }

    // show panel
    if (!gtk_widget_get_visible(GTK_WIDGET(this->main_window_->get_panel_notebook(panel_x))))
    {
        xset_set_b_panel(panel_x, xset::panel::show, true);
        show_panels_all_windows(nullptr, this->main_window_);
    }

    // open in tab in panel
    const auto save_curpanel = this->main_window_->curpanel;

    this->main_window_->curpanel = panel_x;
    this->main_window_->notebook = this->main_window_->get_panel_notebook(panel_x);

    this->main_window_->new_tab(file_path);

    this->main_window_->curpanel = save_curpanel;
    this->main_window_->notebook =
        this->main_window_->get_panel_notebook(this->main_window_->curpanel);

    // focus original panel
    // while(g_main_context_pending(nullptr))
    //    g_main_context_iteration(nullptr, true);
    // gtk_widget_grab_focus(GTK_WIDGET(this->main_window_->notebook));
    // gtk_widget_grab_focus(GTK_WIDGET(browser->folder_view));
    g_idle_add((GSourceFunc)gui_browser_delay_focus, this);
}

bool
gui::browser::is_panel_visible(const panel_t panel) const noexcept
{
    if (!is_valid_panel(panel))
    {
        return false;
    }
    return gtk_widget_get_visible(GTK_WIDGET(this->main_window_->get_panel_notebook(panel)));
}

gui::browser::browser_count_data
gui::browser::get_tab_panel_counts() const noexcept
{
    GtkNotebook* notebook = this->main_window_->get_panel_notebook(this->panel_);
    const tab_t tab_count = gtk_notebook_get_n_pages(notebook);

    // tab_num starts counting from 1
    const tab_t tab_num = gtk_notebook_page_num(notebook, GTK_WIDGET(this)) + 1;
    panel_t panel_count = 0;
    for (const panel_t p : PANELS)
    {
        if (gtk_widget_get_visible(GTK_WIDGET(this->main_window_->get_panel_notebook(p))))
        {
            panel_count += 1;
        }
    }

    return {panel_count, tab_count, tab_num};
}

void
gui::browser::go_home() noexcept
{
    this->focus_folder_view();
    this->chdir(vfs::user::home());
}

void
gui::browser::go_tab(tab_t tab) noexcept
{
    // logger::info<logger::domain::gui>("gui::wrapper::browser::go_tab fb={}", logger::utils::ptr(this));

    switch (tab.data())
    {
        case tab_control_code_prev.data():
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
        case tab_control_code_next.data():
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
        case tab_control_code_close.data():
            // close
            this->close_tab();
            break;
        case tab_control_code_restore.data():
            // restore
            this->restore_tab();
            break;
        default:
            // set tab
            if (tab <= gtk_notebook_get_n_pages(this->notebook_) && tab > 0)
            {
                gtk_notebook_set_current_page(this->notebook_, tab.data() - 1);
            }
            break;
    }
}

void
gui::browser::go_back() noexcept
{
    this->focus_folder_view();
    if (this->history_->has_back())
    {
        const auto mode = gui::utils::history::mode::history_back;
        this->chdir(this->history_->path(mode), mode);
    }
}

void
gui::browser::go_forward() noexcept
{
    this->focus_folder_view();
    if (this->history_->has_forward())
    {
        const auto mode = gui::utils::history::mode::history_forward;
        this->chdir(this->history_->path(mode), mode);
    }
}

void
gui::browser::go_up() noexcept
{
    this->focus_folder_view();
    const auto parent_dir = this->cwd().parent_path();
    if (!std::filesystem::equivalent(parent_dir, this->cwd()))
    {
        this->chdir(parent_dir);
    }
}

void
gui::browser::refresh(const bool update_selected_files) noexcept
{
    if (this->dir_->is_loading())
    {
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

    // begin reload dir
    this->signal_chdir_begin_.emit(this);

    this->dir_->refresh();
}

void
gui::browser::show_hidden_files(bool show) noexcept
{
    if (this->show_hidden_files_ == show)
    {
        return;
    }
    this->show_hidden_files_ = show;

    if (this->file_list_)
    {
        this->update_model();

        this->signal_change_selection_.emit(this);
    }

    if (this->side_dir)
    {
        gui::view::dir_tree::show_hidden_files(GTK_TREE_VIEW(this->side_dir),
                                               this->show_hidden_files_);
    }
}

void
gui::browser::new_tab() noexcept
{
    this->focus_folder_view();

    if (!std::filesystem::is_directory(vfs::user::home()))
    {
        this->signal_open_file_.emit(this, "/", gui::browser::open_action::new_tab);
    }
    else
    {
        this->signal_open_file_.emit(this, vfs::user::home(), gui::browser::open_action::new_tab);
    }
}

void
gui::browser::new_tab_here() noexcept
{
    this->focus_folder_view();

    auto dir_path = this->cwd();
    if (!std::filesystem::is_directory(dir_path))
    {
        dir_path = vfs::user::home();
    }
    if (!std::filesystem::is_directory(dir_path))
    {
        this->signal_open_file_.emit(this, "/", gui::browser::open_action::new_tab);
    }
    else
    {
        this->signal_open_file_.emit(this, dir_path, gui::browser::open_action::new_tab);
    }
}

void
gui::browser::close_tab() noexcept
{
    global::closed_tabs_restore[this->panel_].push_back(this->cwd());
    // logger::info<logger::domain::gui>("close_tab() fb={}, path={}", logger::utils::ptr(this), closed_tabs_restore[this->panel_].back());

    GtkNotebook* notebook =
        GTK_NOTEBOOK(gtk_widget_get_ancestor(GTK_WIDGET(this), GTK_TYPE_NOTEBOOK));

    MainWindow* main_window = this->main_window_;
    main_window->curpanel = this->panel_;
    main_window->notebook = main_window->get_panel_notebook(main_window->curpanel);

    // save solumns and slider positions of tab to be closed
    this->slider_release(nullptr);
    this->save_column_widths();

    // remove page can also be used to destroy - same result
    // gtk_notebook_remove_page(notebook, gtk_notebook_get_current_page(notebook));
    gtk_widget_destroy(GTK_WIDGET(this));

    if (!this->settings_->always_show_tabs)
    {
        if (gtk_notebook_get_n_pages(notebook) == 1)
        {
            gtk_notebook_set_show_tabs(notebook, false);
        }
    }

    if (gtk_notebook_get_n_pages(notebook) == 0)
    {
        main_window->new_tab(vfs::user::home());
        gui::browser* a_browser =
            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, 0));
        a_browser->update_views();
        main_window->set_window_title(a_browser);
        if (xset_get_b(xset::name::main_save_tabs))
        {
            autosave::request_add();
        }
        return;
    }

    // update view of new current tab
    const auto cur_tabx = gtk_notebook_get_current_page(main_window->notebook);
    if (cur_tabx != -1)
    {
        gui::browser* a_browser =
            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, cur_tabx));
        a_browser->update_views();
        a_browser->update_statusbar();
        // g_idle_add((GSourceFunc)delayed_focus, a_browser->folder_view());
    }

    main_window->set_window_title(this);
    if (xset_get_b(xset::name::main_save_tabs))
    {
        autosave::request_add();
    }
}

void
gui::browser::restore_tab() noexcept
{
    if (global::closed_tabs_restore[this->panel_].empty())
    {
        logger::info<logger::domain::gui>("No tabs to restore for panel {}", this->panel_);
        return;
    }

    const auto file_path = global::closed_tabs_restore[this->panel_].back();
    global::closed_tabs_restore[this->panel_].pop_back();
    // logger::info<logger::domain::gui>("restore_tab() fb={}, panel={} path={}", logger::utils::ptr(this), this->panel_, file_path);

    MainWindow* main_window = this->main_window_;

    main_window->new_tab(file_path);

    main_window->set_window_title(this);
    if (xset_get_b(xset::name::main_save_tabs))
    {
        autosave::request_add();
    }
}

void
gui::browser::open_in_tab(const std::filesystem::path& file_path, const tab_t tab) const noexcept
{
    const tab_t cur_page = gtk_notebook_get_current_page(this->notebook_);
    const tab_t pages = gtk_notebook_get_n_pages(this->notebook_);

    tab_t page_x = 0;
    switch (tab.data())
    {
        case tab_control_code_prev.data():
            // prev
            page_x = cur_page - 1;
            break;
        case tab_control_code_next.data():
            // next
            page_x = cur_page + 1;
            break;
        default:
            page_x = tab - 1;
            break;
    }

    if (page_x > -1 && page_x < pages && page_x != cur_page)
    {
        gui::browser* browser =
            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(this->notebook_, page_x.data()));

        browser->chdir(file_path);
    }
}

std::vector<std::shared_ptr<vfs::file>>
gui::browser::selected_files() const noexcept
{
    GtkTreeModel* model = nullptr;
    std::vector<std::shared_ptr<vfs::file>> file_list;
    const std::vector<GtkTreePath*> selected_files = this->selected_items(&model);
    if (selected_files.empty())
    {
        return file_list;
    }

    file_list.reserve(selected_files.size());
    for (GtkTreePath* sel : selected_files)
    {
        GtkTreeIter it;
        std::shared_ptr<vfs::file> file;
        gtk_tree_model_get_iter(model, &it, sel);
        gtk_tree_model_get(model, &it, gui::file_list::column::info, &file, -1);
        file_list.push_back(file);
    }

    std::ranges::for_each(selected_files, gtk_tree_path_free);

    return file_list;
}

void
gui::browser::open_selected_files() noexcept
{
    this->open_selected_files_with_app();
}

void
gui::browser::open_selected_files_with_app(const std::string_view app_desktop) noexcept
{
    const auto selected_files = this->selected_files();

    gui::action::open_files_with_app(this->cwd(), selected_files, app_desktop, this, false, false);
}

void
gui::browser::rename_selected_files(
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
        const auto result = gui::dialog::rename_files(this, cwd, file, nullptr, false);
        if (result == 0)
        {
            break;
        }
    }
}

void
gui::browser::batch_rename_selected_files(
    const std::span<const std::shared_ptr<vfs::file>> selected_files,
    const std::filesystem::path& cwd) noexcept
{
    if (selected_files.empty())
    {
        return;
    }

    gtk_widget_grab_focus(this->folder_view_);

    gui::dialog::batch_rename_files(this, cwd, selected_files);
}

void
gui::browser::hide_selected(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                            const std::filesystem::path& cwd) noexcept
{
    (void)cwd;

    const auto response = gui::dialog::message(
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

    if (selected_files.empty())
    {
        gui::dialog::error("Error", "No files are selected");
        return;
    }

    for (const auto& file : selected_files)
    {
        if (!this->dir_->add_hidden(file))
        {
            gui::dialog::error("Error", "Error hiding files");
        }
    }

    // refresh from here causes a segfault occasionally
    // browser->refresh();
}

void
gui::browser::copycmd(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                      const std::filesystem::path& cwd, xset::name setname) noexcept
{
    std::optional<std::filesystem::path> copy_dest;
    std::optional<std::filesystem::path> move_dest;

    if (setname == xset::name::copy_tab_prev)
    {
        copy_dest = this->tab_cwd(tab_control_code_prev);
    }
    else if (setname == xset::name::copy_tab_next)
    {
        copy_dest = this->tab_cwd(tab_control_code_next);
    }
    else if (setname == xset::name::copy_tab_1)
    {
        copy_dest = this->tab_cwd(tab_1);
    }
    else if (setname == xset::name::copy_tab_2)
    {
        copy_dest = this->tab_cwd(tab_2);
    }
    else if (setname == xset::name::copy_tab_3)
    {
        copy_dest = this->tab_cwd(tab_3);
    }
    else if (setname == xset::name::copy_tab_4)
    {
        copy_dest = this->tab_cwd(tab_4);
    }
    else if (setname == xset::name::copy_tab_5)
    {
        copy_dest = this->tab_cwd(tab_5);
    }
    else if (setname == xset::name::copy_tab_6)
    {
        copy_dest = this->tab_cwd(tab_6);
    }
    else if (setname == xset::name::copy_tab_7)
    {
        copy_dest = this->tab_cwd(tab_7);
    }
    else if (setname == xset::name::copy_tab_8)
    {
        copy_dest = this->tab_cwd(tab_8);
    }
    else if (setname == xset::name::copy_tab_9)
    {
        copy_dest = this->tab_cwd(tab_9);
    }
    else if (setname == xset::name::copy_tab_10)
    {
        copy_dest = this->tab_cwd(tab_10);
    }
    else if (setname == xset::name::copy_panel_prev)
    {
        copy_dest = this->panel_cwd(panel_control_code_prev);
    }
    else if (setname == xset::name::copy_panel_next)
    {
        copy_dest = this->panel_cwd(panel_control_code_next);
    }
    else if (setname == xset::name::copy_panel_1)
    {
        copy_dest = this->panel_cwd(panel_1);
    }
    else if (setname == xset::name::copy_panel_2)
    {
        copy_dest = this->panel_cwd(panel_2);
    }
    else if (setname == xset::name::copy_panel_3)
    {
        copy_dest = this->panel_cwd(panel_3);
    }
    else if (setname == xset::name::copy_panel_4)
    {
        copy_dest = this->panel_cwd(panel_4);
    }
    else if (setname == xset::name::copy_loc_last)
    {
        const auto set = xset::set::get(xset::name::copy_loc_last);
        copy_dest = set->desc.value();
    }
    else if (setname == xset::name::move_tab_prev)
    {
        move_dest = this->tab_cwd(tab_control_code_prev);
    }
    else if (setname == xset::name::move_tab_next)
    {
        move_dest = this->tab_cwd(tab_control_code_next);
    }
    else if (setname == xset::name::move_tab_1)
    {
        move_dest = this->tab_cwd(tab_1);
    }
    else if (setname == xset::name::move_tab_2)
    {
        move_dest = this->tab_cwd(tab_2);
    }
    else if (setname == xset::name::move_tab_3)
    {
        move_dest = this->tab_cwd(tab_3);
    }
    else if (setname == xset::name::move_tab_4)
    {
        move_dest = this->tab_cwd(tab_4);
    }
    else if (setname == xset::name::move_tab_5)
    {
        move_dest = this->tab_cwd(tab_5);
    }
    else if (setname == xset::name::move_tab_6)
    {
        move_dest = this->tab_cwd(tab_6);
    }
    else if (setname == xset::name::move_tab_7)
    {
        move_dest = this->tab_cwd(tab_7);
    }
    else if (setname == xset::name::move_tab_8)
    {
        move_dest = this->tab_cwd(tab_8);
    }
    else if (setname == xset::name::move_tab_9)
    {
        move_dest = this->tab_cwd(tab_9);
    }
    else if (setname == xset::name::move_tab_10)
    {
        move_dest = this->tab_cwd(tab_10);
    }
    else if (setname == xset::name::move_panel_prev)
    {
        move_dest = this->panel_cwd(panel_control_code_prev);
    }
    else if (setname == xset::name::move_panel_next)
    {
        move_dest = this->panel_cwd(panel_control_code_next);
    }
    else if (setname == xset::name::move_panel_1)
    {
        move_dest = this->panel_cwd(panel_1);
    }
    else if (setname == xset::name::move_panel_2)
    {
        move_dest = this->panel_cwd(panel_2);
    }
    else if (setname == xset::name::move_panel_3)
    {
        move_dest = this->panel_cwd(panel_3);
    }
    else if (setname == xset::name::move_panel_4)
    {
        move_dest = this->panel_cwd(panel_4);
    }
    else if (setname == xset::name::move_loc_last)
    {
        const auto set = xset::set::get(xset::name::copy_loc_last);
        move_dest = set->desc.value();
    }

    if ((setname == xset::name::copy_loc || setname == xset::name::copy_loc_last ||
         setname == xset::name::move_loc || setname == xset::name::move_loc_last) &&
        !copy_dest && !move_dest)
    {
        std::filesystem::path folder;
        const auto set = xset::set::get(xset::name::copy_loc_last);
        if (set->desc)
        {
            folder = set->desc.value();
        }
        else
        {
            folder = cwd;
        }
        const auto path =
            gui::dialog::file_chooser(GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
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

            xset_set(xset::name::copy_loc_last, xset::var::desc, path->string());
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
            gui::dialog::message("Invalid Destination",
                                 GtkButtonsType::GTK_BUTTONS_OK,
                                 "Destination same as source");
            return;
        }

        // rebuild selected_files with full paths
        std::vector<std::filesystem::path> file_list;
        file_list.reserve(selected_files.size());
        for (const auto& file : selected_files)
        {
            file_list.push_back(file->path());
        }

        GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(this));

        // task
        gui::file_task* ptask = gui_file_task_new(file_action,
                                                  file_list,
                                                  dest_dir.value(),
                                                  GTK_WINDOW(parent_win),
                                                  this->task_view_);
        ptask->run();
    }
    else
    {
        gui::dialog::message("Invalid Destination",
                             GtkButtonsType::GTK_BUTTONS_OK,
                             "Invalid destination");
    }
}

void
gui::browser::set_sort_order(gui::browser::sort_order order) noexcept
{
    if (order == this->sort_order_)
    {
        return;
    }

    this->sort_order_ = order;
    if (this->file_list_)
    {
        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(this->file_list_),
            magic_enum::enum_integer(file_list_order_from_sort_order(order)),
            this->sort_type_);
    }
}

void
gui::browser::set_sort_type(GtkSortType order) noexcept
{
    if (order != this->sort_type_)
    {
        this->sort_type_ = order;
        if (this->file_list_)
        {
            std::int32_t col = 0;
            GtkSortType old_order;
            gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(this->file_list_),
                                                 &col,
                                                 &old_order);
            gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(this->file_list_), col, order);
        }
    }
}

void
gui::browser::set_sort_extra(xset::name setname) const noexcept
{
    const auto set = xset::set::get(setname);

    if (!set->name().starts_with("sortx_"))
    {
        return;
    }

    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(this->file_list_);
    if (!list)
    {
        return;
    }

    if (set->xset_name == xset::name::sortx_natural)
    {
        list->sort_natural = set->b == xset::set::enabled::yes;
        xset_set_b_panel(this->panel_, xset::panel::sort_extra, list->sort_natural);
    }
    else if (set->xset_name == xset::name::sortx_case)
    {
        list->sort_case = set->b == xset::set::enabled::yes;
        xset_set_panel(this->panel_,
                       xset::panel::sort_extra,
                       xset::var::x,
                       std::format("{}", magic_enum::enum_integer(set->b)));
    }
    else if (set->xset_name == xset::name::sortx_directories)
    {
        list->sort_dir_ = gui::file_list::sort_dir::first;
        xset_set_panel(
            this->panel_,
            xset::panel::sort_extra,
            xset::var::y,
            std::format("{}", magic_enum::enum_integer(gui::file_list::sort_dir::first)));
    }
    else if (set->xset_name == xset::name::sortx_files)
    {
        list->sort_dir_ = gui::file_list::sort_dir::last;
        xset_set_panel(this->panel_,
                       xset::panel::sort_extra,
                       xset::var::y,
                       std::format("{}", magic_enum::enum_integer(gui::file_list::sort_dir::last)));
    }
    else if (set->xset_name == xset::name::sortx_mix)
    {
        list->sort_dir_ = gui::file_list::sort_dir::mixed;
        xset_set_panel(
            this->panel_,
            xset::panel::sort_extra,
            xset::var::y,
            std::format("{}", magic_enum::enum_integer(gui::file_list::sort_dir::mixed)));
    }
    else if (set->xset_name == xset::name::sortx_hidfirst)
    {
        list->sort_hidden_first = set->b == xset::set::enabled::yes;
        xset_set_panel(this->panel_,
                       xset::panel::sort_extra,
                       xset::var::z,
                       std::format("{}", magic_enum::enum_integer(set->b)));
    }
    else if (set->xset_name == xset::name::sortx_hidlast)
    {
        list->sort_hidden_first = set->b != xset::set::enabled::yes;
        xset_set_panel(this->panel_,
                       xset::panel::sort_extra,
                       xset::var::z,
                       std::format("{}",
                                   set->b == xset::set::enabled::yes
                                       ? magic_enum::enum_integer(xset::set::enabled::no)
                                       : magic_enum::enum_integer(xset::set::enabled::yes)));
    }
    list->sort();
}

void
gui::browser::paste_link() const noexcept
{
    GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(this));

    gui::clipboard::paste_links(GTK_WINDOW(parent_win),
                                this->cwd(),
                                GTK_TREE_VIEW(this->task_view_));
}

void
gui::browser::paste_target() const noexcept
{
    GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(this));

    gui::clipboard::paste_targets(GTK_WINDOW(parent_win),
                                  this->cwd(),
                                  GTK_TREE_VIEW(this->task_view_));
}

void
gui::browser::select_all() const noexcept
{
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            exo_icon_view_select_all(EXO_ICON_VIEW(this->folder_view_));
#else
            gtk_icon_view_select_all(GTK_ICON_VIEW(this->folder_view_));
#endif
            break;
        case gui::browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            gtk_tree_selection_select_all(selection);
            break;
    }
}

void
gui::browser::unselect_all() const noexcept
{
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:

#if defined(USE_EXO)
            exo_icon_view_unselect_all(EXO_ICON_VIEW(this->folder_view_));
#else
            gtk_icon_view_unselect_all(GTK_ICON_VIEW(this->folder_view_));
#endif
            break;
        case gui::browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            gtk_tree_selection_unselect_all(selection);
            break;
    }
}

void
gui::browser::select_last() const noexcept
{
    // logger::debug<logger::domain::gui>("select_last");
    const auto selected_files = this->history_->get_selection(this->cwd());
    if (selected_files && !selected_files->empty())
    {
        this->select_files(*selected_files);
    }
}

static std::string
select_pattern_dialog(GtkWidget* parent) noexcept
{
    (void)parent;

    const auto response = datatype::run_dialog_sync<datatype::pattern::response>(
        spacefm::package.dialog.pattern,
        datatype::pattern::request{.pattern = ""});
    if (!response)
    {
        return "";
    }

    return response->pattern;
}

void
gui::browser::select_pattern(const std::string_view search_key) noexcept
{
    std::string key;
    if (search_key.empty())
    {
        const auto set = xset::set::get(xset::name::select_patt);
        const auto pattern = select_pattern_dialog(GTK_WIDGET(this->main_window_));

        if (pattern.empty())
        {
            return;
        }
        key = pattern;
    }
    else
    {
        key = search_key;
    }

    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    GtkTreeSelection* selection = nullptr;

    // get model, treesel, and stop signals
    switch (this->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
#else
            model = gtk_icon_view_get_model(GTK_ICON_VIEW(this->folder_view_));
#endif
            break;
        case gui::browser::view_mode::list_view:
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
            gtk_tree_model_get(model, &it, gui::file_list::column::info, &file, -1);
            if (!file)
            {
                continue;
            }

            // test name
            const auto name = file->name();
            const bool select = (fnmatch(key.data(), name.data(), 0) == 0);

            // do selection and scroll to first selected
            GtkTreePath* path =
                gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST_REINTERPRET(this->file_list_)),
                                        &it);

            switch (this->view_mode_)
            {
                case gui::browser::view_mode::icon_view:
                case gui::browser::view_mode::compact_view:
                    // select
#if defined(USE_EXO)
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
#else
                    if (gtk_icon_view_path_is_selected(GTK_ICON_VIEW(this->folder_view_), path))
                    {
                        if (!select)
                        {
                            gtk_icon_view_unselect_path(GTK_ICON_VIEW(this->folder_view_), path);
                        }
                    }
                    else if (select)
                    {
                        gtk_icon_view_select_path(GTK_ICON_VIEW(this->folder_view_), path);
                    }
#endif

                    // scroll to first and set cursor
                    if (first_select && select)
                    {
#if defined(USE_EXO)
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(this->folder_view_),
                                                 path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(this->folder_view_),
                                                     path,
                                                     true,
                                                     .25,
                                                     0);
#else
                        gtk_icon_view_set_cursor(GTK_ICON_VIEW(this->folder_view_),
                                                 path,
                                                 nullptr,
                                                 false);
                        gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(this->folder_view_),
                                                     path,
                                                     true,
                                                     .25,
                                                     0);
#endif
                        first_select = false;
                    }
                    break;
                case gui::browser::view_mode::list_view:
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
                 gui::browser* browser) noexcept
{
    (void)model;
    (void)it;
    GtkTreeSelection* selection = nullptr;

    switch (browser->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(browser->folder_view_), path))
            {
                exo_icon_view_unselect_path(EXO_ICON_VIEW(browser->folder_view_), path);
            }
            else
            {
                exo_icon_view_select_path(EXO_ICON_VIEW(browser->folder_view_), path);
            }
#else
            if (gtk_icon_view_path_is_selected(GTK_ICON_VIEW(browser->folder_view_), path))
            {
                gtk_icon_view_unselect_path(GTK_ICON_VIEW(browser->folder_view_), path);
            }
            else
            {
                gtk_icon_view_select_path(GTK_ICON_VIEW(browser->folder_view_), path);
            }
#endif
            break;
        case gui::browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->folder_view_));
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
gui::browser::invert_selection() noexcept
{
    GtkTreeModel* model = nullptr;
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)::invert_selection, this);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(this->folder_view_), this);
#else
            model = gtk_icon_view_get_model(GTK_ICON_VIEW(this->folder_view_));
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)::invert_selection, this);
            on_folder_view_item_sel_change(GTK_ICON_VIEW(this->folder_view_), this);
#endif
            break;
        case gui::browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(this->folder_view_));
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)::invert_selection, this);
#if defined(USE_EXO)
            on_folder_view_item_sel_change(EXO_ICON_VIEW(selection), this);
#else
            on_folder_view_item_sel_change(GTK_ICON_VIEW(selection), this);
#endif
            break;
    }
}

/* FIXME: Do not recreate the view if previous view is compact view */
void
gui::browser::view_as_icons() noexcept
{
    if (this->view_mode_ == gui::browser::view_mode::icon_view && this->folder_view_)
    {
        return;
    }

    this->show_thumbnails(this->max_thumbnail_, true);

    this->view_mode_ = gui::browser::view_mode::icon_view;
    if (this->folder_view_)
    {
        gtk_widget_destroy(this->folder_view_);
    }
    this->folder_view_ = create_folder_view(this, gui::browser::view_mode::icon_view);
#if defined(USE_EXO)
    exo_icon_view_set_model(EXO_ICON_VIEW(this->folder_view_), this->file_list_);
#else
    gtk_icon_view_set_model(GTK_ICON_VIEW(this->folder_view_), this->file_list_);
#endif
    gtk_scrolled_window_set_policy(this->folder_view_scroll_,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_widget_show(GTK_WIDGET(this->folder_view_));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->folder_view_scroll_),
                                  GTK_WIDGET(this->folder_view_));
}

/* FIXME: Do not recreate the view if previous view is icon view */
void
gui::browser::view_as_compact_list() noexcept
{
    if (this->view_mode_ == gui::browser::view_mode::compact_view && this->folder_view_)
    {
        return;
    }

    this->show_thumbnails(this->max_thumbnail_);

    this->view_mode_ = gui::browser::view_mode::compact_view;
    if (this->folder_view_)
    {
        gtk_widget_destroy(this->folder_view_);
    }
    this->folder_view_ = create_folder_view(this, gui::browser::view_mode::compact_view);
#if defined(USE_EXO)
    exo_icon_view_set_model(EXO_ICON_VIEW(this->folder_view_), this->file_list_);
#else
    gtk_icon_view_set_model(GTK_ICON_VIEW(this->folder_view_), this->file_list_);
#endif
    gtk_scrolled_window_set_policy(this->folder_view_scroll_,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_widget_show(GTK_WIDGET(this->folder_view_));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->folder_view_scroll_),
                                  GTK_WIDGET(this->folder_view_));
}

void
gui::browser::view_as_list() noexcept
{
    if (this->view_mode_ == gui::browser::view_mode::list_view && this->folder_view_)
    {
        return;
    }

    this->show_thumbnails(this->max_thumbnail_);

    this->view_mode_ = gui::browser::view_mode::list_view;
    if (this->folder_view_)
    {
        gtk_widget_destroy(this->folder_view_);
    }
    this->folder_view_ = create_folder_view(this, gui::browser::view_mode::list_view);
    gtk_tree_view_set_model(GTK_TREE_VIEW(this->folder_view_), this->file_list_);
    gtk_scrolled_window_set_policy(this->folder_view_scroll_,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_ALWAYS);
    gtk_widget_show(GTK_WIDGET(this->folder_view_));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->folder_view_scroll_),
                                  GTK_WIDGET(this->folder_view_));
}

void
gui::browser::show_thumbnails(const u32 max_file_size) noexcept
{
    this->show_thumbnails(max_file_size, this->large_icons_);
}

void
gui::browser::show_thumbnails(const u32 max_file_size, const bool large_icons) noexcept
{
    this->max_thumbnail_ = max_file_size;
    if (this->file_list_)
    {
        bool thumbs_blacklisted = false;
        if (!this->dir_ || this->dir_->avoid_changes())
        { // this will disable thumbnails if change detection is blacklisted on current device
            thumbs_blacklisted = true;
        }

        gui::file_list* list = PTK_FILE_LIST_REINTERPRET(this->file_list_);
        list->show_thumbnails(large_icons ? vfs::file::thumbnail_size::big
                                          : vfs::file::thumbnail_size::small,
                              thumbs_blacklisted ? 0 : max_file_size.data());
    }
}

void
gui::browser::update_views() noexcept
{
    // logger::debug<logger::domain::gui>("gui::browser::update_views fb={}  (panel {})", logger::utils::ptr(this), this->mypanel);

    // hide/show browser widgets based on user settings
    const panel_t p = this->panel_;
    const xset::main_window_panel mode = this->main_window_->panel_context.at(p);

    if (xset_get_b_panel_mode(p, xset::panel::show_toolbox, mode))
    {
        gtk_widget_show(GTK_WIDGET(this->toolbar));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(this->toolbar));
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
    {
        if (!this->side_dir)
        {
            this->side_dir = gui_browser_create_dir_tree(this);
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(this->side_dir_scroll),
                                          GTK_WIDGET(this->side_dir));
        }
        gtk_widget_show_all(GTK_WIDGET(this->side_dir_scroll));
        if (this->side_dir && this->file_list_)
        {
            gui::view::dir_tree::chdir(GTK_TREE_VIEW(this->side_dir), this->cwd());
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
            this->side_dev = gui::view::location::create(this);
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

    // set slider positions

    i32 pos = 0;

    // hpane
    pos = this->main_window_->panel_slide_x[p];
    if (pos < 100)
    {
        pos = -1;
    }
    // logger::info<logger::domain::gui>("    set slide_x = {}", pos);
    if (pos > 0)
    {
        gtk_paned_set_position(this->hpane, pos.data());
    }

    // side_vpane_top
    pos = this->main_window_->panel_slide_y[p];
    if (pos < 20)
    {
        pos = -1;
    }
    // logger::info<logger::domain::gui>("    slide_y = {}", pos);
    gtk_paned_set_position(this->side_vpane_top, pos.data());

    // side_vpane_bottom
    pos = this->main_window_->panel_slide_s[p];
    if (pos < 20)
    {
        pos = -1;
    }
    // logger::info<logger::domain::gui>( "    slide_s = {}", pos);
    gtk_paned_set_position(this->side_vpane_bottom, pos.data());

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
    }

    // List Styles
    if (xset_get_b_panel(p, xset::panel::list_detailed))
    {
        this->view_as_list();

        // Set column widths for this panel context
        if (GTK_IS_TREE_VIEW(this->folder_view_))
        {
            // logger::info<logger::domain::gui>("    set widths   mode = {}", mode);
            for (const auto i : std::views::iota(0uz, global::columns.size()))
            {
                GtkTreeViewColumn* col = gtk_tree_view_get_column(GTK_TREE_VIEW(this->folder_view_),
                                                                  static_cast<std::int32_t>(i));
                if (!col)
                {
                    break;
                }
                const char* title = gtk_tree_view_column_get_title(col);
                for (const auto [index, column] : std::views::enumerate(global::columns))
                {
                    if (title == column.title)
                    {
                        // get column width for this panel context
                        const auto set = xset::set::get(column.xset_name, p, mode);
                        const auto width =
                            set->y ? i32::create(set->y.value()).value_or(100_i32) : 100_i32;
                        // logger::info<logger::domain::gui>("        {}\t{}", width, title );
                        if (width != 0)
                        {
                            gtk_tree_view_column_set_fixed_width(col, width.data());
                            // logger::info<logger::domain::gui>("upd set_width {} {}", magic_enum::enum_name(global::columns.at(j).xset_name), width);
                        }
                        // set column visibility
                        gtk_tree_view_column_set_visible(col,
                                                         set->b == xset::set::enabled::yes ||
                                                             index == 0);

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

    // logger::info<logger::domain::gui>("gui::browser::update_views fb={} DONE", logger::utils::ptr(this));
}

void
gui::browser::focus(const gui::browser::focus_widget item) noexcept
{
    GtkWidget* widget = nullptr;

    const panel_t p = this->panel_;
    const xset::main_window_panel mode = this->main_window_->panel_context.at(p);
    switch (item)
    {
        case gui::browser::focus_widget::path_bar:
            // path bar
            if (!xset_get_b_panel_mode(p, xset::panel::show_toolbox, mode))
            {
                xset_set_b_panel_mode(p, xset::panel::show_toolbox, mode, true);
                update_views_all_windows(nullptr, this);
            }
            widget = GTK_WIDGET(this->path_bar_);
            break;
        case gui::browser::focus_widget::dirtree:
            if (!xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
            {
                xset_set_b_panel_mode(p, xset::panel::show_dirtree, mode, true);
                update_views_all_windows(nullptr, this);
            }
            widget = this->side_dir;
            break;
        case gui::browser::focus_widget::device:
            if (!xset_get_b_panel_mode(p, xset::panel::show_devmon, mode))
            {
                xset_set_b_panel_mode(p, xset::panel::show_devmon, mode, true);
                update_views_all_windows(nullptr, this);
            }
            widget = this->side_dev;
            break;
        case gui::browser::focus_widget::filelist:
            widget = this->folder_view_;
            break;
        case gui::browser::focus_widget::search_bar:
            widget = GTK_WIDGET(this->search_bar_);
            break;
        case gui::browser::focus_widget::invalid:
            return;
    }
    if (gtk_widget_get_visible(widget))
    {
        gtk_widget_grab_focus(GTK_WIDGET(widget));
    }
}

void
gui::browser::focus_me() noexcept
{
    this->signal_change_pane_.emit(this);
}

void
gui::browser::save_column_widths() const noexcept
{
    GtkTreeView* view = GTK_TREE_VIEW(this->folder_view_);
    if (!GTK_IS_TREE_VIEW(view))
    {
        return;
    }

    if (this->view_mode_ != gui::browser::view_mode::list_view)
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
        // logger::debug<logger::domain::gui>("save_columns  fb={} (panel {})  mode = {}", logger::utils::ptr(this), p, mode);
        for (const auto i : std::views::iota(0uz, global::columns.size()))
        {
            GtkTreeViewColumn* col = gtk_tree_view_get_column(view, static_cast<std::int32_t>(i));
            if (!col)
            {
                return;
            }
            const char* title = gtk_tree_view_column_get_title(col);
            for (const auto column : global::columns)
            {
                if (title == column.title)
                {
                    // save column width for this panel context
                    const auto set = xset::set::get(column.xset_name, p, mode);
                    const i32 width = gtk_tree_view_column_get_width(col);
                    if (width > 0)
                    {
                        set->y = std::format("{}", width);
                        // logger::info<logger::domain::gui>("        {}\t{}", width, title);
                    }

                    break;
                }
            }
        }
    }
}

bool
gui::browser::slider_release(GtkPaned* pane) const noexcept
{
    const panel_t p = this->panel_;
    const xset::main_window_panel mode = this->main_window_->panel_context.at(p);

    const auto set = xset::set::get(xset::panel::slider_positions, p, mode);

    if (pane == this->hpane)
    {
        const i32 pos = gtk_paned_get_position(this->hpane);
        if (!this->main_window_->fullscreen)
        {
            set->x = std::format("{}", pos);
        }
        this->main_window_->panel_slide_x[p] = pos;
        // logger::debug<logger::domain::gui>("    slide_x = {}", pos);
    }
    else
    {
        i32 pos = 0;
        // logger::debug<logger::domain::gui>("gui::browser::slider_release fb={}  (panel {})  mode = {}", logger::utils::ptr(this), p, logger::utils::ptr(mode));
        pos = gtk_paned_get_position(this->side_vpane_top);
        if (!this->main_window_->fullscreen)
        {
            set->y = std::format("{}", pos);
        }
        this->main_window_->panel_slide_y[p] = pos;
        // logger::debug<logger::domain::gui>("    slide_y = {}  ", pos);

        pos = gtk_paned_get_position(this->side_vpane_bottom);
        if (!this->main_window_->fullscreen)
        {
            set->s = std::format("{}", pos);
        }
        this->main_window_->panel_slide_s[p] = pos;
        // logger::debug<logger::domain::gui>("    slide_s = {}", pos);
    }
    return false;
}

void
gui::browser::rebuild_toolbars() const noexcept
{
    const auto& cwd = this->cwd();
    gtk_entry_set_text(GTK_ENTRY(this->path_bar_), cwd.c_str());
}

void
gui::browser::update_selection_history() const noexcept
{
    // logger::debug<logger::domain::gui>("selection history: {}", cwd.string());

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
    this->history_->set_selection(this->cwd(), selected_filenames);
}

std::vector<GtkTreePath*>
gui::browser::selected_items(GtkTreeModel** model) const noexcept
{
    GList* selected = nullptr;
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            *model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
            selected = exo_icon_view_get_selected_items(EXO_ICON_VIEW(this->folder_view_));
#else
            *model = gtk_icon_view_get_model(GTK_ICON_VIEW(this->folder_view_));
            selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(this->folder_view_));
#endif
            break;
        case gui::browser::view_mode::list_view:
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(this->folder_view_));
            selected = gtk_tree_selection_get_selected_rows(selection, model);
            break;
    }

    std::vector<GtkTreePath*> selected_items;
    selected_items.reserve(g_list_length(selected));
    for (GList* sel = selected; sel; sel = g_list_next(sel))
    {
        selected_items.push_back((GtkTreePath*)sel->data);
    }
    return selected_items;
}

void
gui::browser::select_file(const std::filesystem::path& filename,
                          const bool unselect_others) const noexcept
{
    GtkTreeSelection* tree_sel = nullptr;
    GtkTreeModel* model = nullptr;

    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(this->file_list_);
    if (this->view_mode_ == gui::browser::view_mode::icon_view ||
        this->view_mode_ == gui::browser::view_mode::compact_view)
    {
        if (unselect_others)
        {
#if defined(USE_EXO)
            exo_icon_view_unselect_all(EXO_ICON_VIEW(this->folder_view_));
#else
            gtk_icon_view_unselect_all(GTK_ICON_VIEW(this->folder_view_));
#endif
        }
#if defined(USE_EXO)
        model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
#else
        model = gtk_icon_view_get_model(GTK_ICON_VIEW(this->folder_view_));
#endif
    }
    else if (this->view_mode_ == gui::browser::view_mode::list_view)
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
            gtk_tree_model_get(model, &it, gui::file_list::column::info, &file, -1);
            if (file)
            {
                if (file->name() == select_filename)
                {
                    GtkTreePath* tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (this->view_mode_ == gui::browser::view_mode::icon_view ||
                        this->view_mode_ == gui::browser::view_mode::compact_view)
                    {
#if defined(USE_EXO)
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
#else
                        gtk_icon_view_select_path(GTK_ICON_VIEW(this->folder_view_), tree_path);
                        gtk_icon_view_set_cursor(GTK_ICON_VIEW(this->folder_view_),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(this->folder_view_),
                                                     tree_path,
                                                     true,
                                                     .25,
                                                     0);
#endif
                    }
                    else if (this->view_mode_ == gui::browser::view_mode::list_view)
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
gui::browser::select_files(
    const std::span<const std::filesystem::path> select_filenames) const noexcept
{
    this->unselect_all();

    for (const std::filesystem::path& select_filename : select_filenames)
    {
        this->select_file(select_filename.filename(), false);
    }
}

void
gui::browser::unselect_file(const std::filesystem::path& filename,
                            const bool unselect_others) const noexcept
{
    GtkTreeSelection* tree_sel = nullptr;
    GtkTreeModel* model = nullptr;

    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(this->file_list_);
    if (this->view_mode_ == gui::browser::view_mode::icon_view ||
        this->view_mode_ == gui::browser::view_mode::compact_view)
    {
        if (unselect_others)
        {
#if defined(USE_EXO)
            exo_icon_view_unselect_all(EXO_ICON_VIEW(this->folder_view_));
#else
            gtk_icon_view_unselect_all(GTK_ICON_VIEW(this->folder_view_));
#endif
        }
#if defined(USE_EXO)
        model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
#else
        model = gtk_icon_view_get_model(GTK_ICON_VIEW(this->folder_view_));
#endif
    }
    else if (this->view_mode_ == gui::browser::view_mode::list_view)
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
            gtk_tree_model_get(model, &it, gui::file_list::column::info, &file, -1);
            if (file)
            {
                if (file->name() == unselect_filename)
                {
                    GtkTreePath* tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (this->view_mode_ == gui::browser::view_mode::icon_view ||
                        this->view_mode_ == gui::browser::view_mode::compact_view)
                    {
#if defined(USE_EXO)
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
#else
                        gtk_icon_view_unselect_path(GTK_ICON_VIEW(this->folder_view_), tree_path);
                        gtk_icon_view_set_cursor(GTK_ICON_VIEW(this->folder_view_),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(this->folder_view_),
                                                     tree_path,
                                                     true,
                                                     .25,
                                                     0);
#endif
                    }
                    else if (this->view_mode_ == gui::browser::view_mode::list_view)
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
gui::browser::seek_path(const std::filesystem::path& seek_dir,
                        const std::filesystem::path& seek_name) noexcept
{
    // change to dir seek_dir if needed; select first dir or else file with
    // prefix seek_name
    const auto& cwd = this->cwd();

    if (cwd != seek_dir)
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
        // finishes seek in main-window.c on_browser_after_chdir()
        return;
    }

    // no change dir was needed or was called from on_browser_after_chdir()
    // select seek name
    this->unselect_all();

    if (seek_name.empty())
    {
        return;
    }

    // get model, treesel, and stop signals
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    GtkTreeSelection* selection = nullptr;

    switch (this->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            model = exo_icon_view_get_model(EXO_ICON_VIEW(this->folder_view_));
#else
            model = gtk_icon_view_get_model(GTK_ICON_VIEW(this->folder_view_));
#endif
            break;
        case gui::browser::view_mode::list_view:
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
            gtk_tree_model_get(model, &it, gui::file_list::column::info, &file, -1);
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
    GtkTreePath* path = nullptr;
    path =
        gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST_REINTERPRET(this->file_list_)), &it);
    if (!path)
    {
        return;
    }

    switch (this->view_mode_)
    {
        case gui::browser::view_mode::icon_view:
        case gui::browser::view_mode::compact_view:
#if defined(USE_EXO)
            // select
            exo_icon_view_select_path(EXO_ICON_VIEW(this->folder_view_), path);

            // scroll and set cursor
            exo_icon_view_set_cursor(EXO_ICON_VIEW(this->folder_view_), path, nullptr, false);
            exo_icon_view_scroll_to_path(EXO_ICON_VIEW(this->folder_view_), path, true, .25, 0);

#else
            // select
            gtk_icon_view_select_path(GTK_ICON_VIEW(this->folder_view_), path);

            // scroll and set cursor
            gtk_icon_view_set_cursor(GTK_ICON_VIEW(this->folder_view_), path, nullptr, false);
            gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(this->folder_view_), path, true, .25, 0);

#endif
            break;
        case gui::browser::view_mode::list_view:
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
gui::browser::update_statusbar() const noexcept
{
    if (!this->statusbar)
    {
        return;
    }

    std::string statusbar_txt;

    const auto cwd = this->cwd();
    if (cwd.empty())
    {
        // browser has just been created / is still loading
        return;
    }

    if (std::filesystem::exists(cwd))
    {
        const auto fs_stat = ztd::statvfs(cwd);

        // calc free space
        const std::string free_size =
            vfs::utils::format_file_size(fs_stat.bsize() * fs_stat.bavail());
        // calc total space
        const std::string disk_size =
            vfs::utils::format_file_size(fs_stat.frsize() * fs_stat.blocks());

        statusbar_txt.append(std::format(" {} / {}   ", free_size, disk_size));
    }

    // Show Reading... while sill loading
    if (!this->dir_ || this->dir_->is_loading())
    {
        statusbar_txt.append(std::format("Reading {} ...", cwd.string()));
        gtk_statusbar_pop(this->statusbar, 0);
        gtk_statusbar_push(this->statusbar, 0, statusbar_txt.data());
        return;
    }

    const u64 total_files = this->dir_->files().size();
    const u64 total_hidden = this->dir_->hidden_files();
    const u64 total_visible = this->show_hidden_files_ ? total_files : total_files - total_hidden;

    if (this->n_selected_files_ > 0)
    {
        const auto selected_files = this->selected_files();
        if (selected_files.empty())
        {
            return;
        }

        const std::string file_size = vfs::utils::format_file_size(this->sel_size_);
        const std::string disk_size = vfs::utils::format_file_size(this->sel_disk_size_);

        statusbar_txt.append(std::format("{:L} / {:L} ({} / {})",
                                         this->n_selected_files_,
                                         total_visible,
                                         file_size,
                                         disk_size));

        if (this->n_selected_files_ == 1)
        // display file name or symlink info in status bar if one file selected
        {
            const auto& file = selected_files.front();
            if (!file)
            {
                return;
            }

            if (file->is_symlink())
            {
                const auto target = std::filesystem::absolute(file->path());
                if (!target.empty())
                {
                    std::filesystem::path target_path;

                    // logger::info<logger::domain::gui>("LINK: {}", file->path());
                    if (!target.is_absolute())
                    {
                        // relative link
                        target_path = cwd / target;
                    }
                    else
                    {
                        target_path = target;
                    }

                    if (file->is_directory())
                    {
                        if (std::filesystem::exists(target_path))
                        {
                            statusbar_txt.append(std::format("  Link -> {}/", target.string()));
                        }
                        else
                        {
                            statusbar_txt.append(
                                std::format("  !Link -> {}/ (missing)", target.string()));
                        }
                    }
                    else
                    {
                        const auto results = ztd::stat::create(target_path);
                        if (results)
                        {
                            const auto lsize = vfs::utils::format_file_size(results->size());
                            statusbar_txt.append(
                                std::format("  Link -> {} ({})", target.string(), lsize));
                        }
                        else
                        {
                            statusbar_txt.append(
                                std::format("  !Link -> {} (missing)", target.string()));
                        }
                    }
                }
                else
                {
                    statusbar_txt.append(std::format("  !Link -> (error reading target)"));
                }
            }
            else
            {
                statusbar_txt.append(std::format("  {}", file->name()));
            }
        }
        else
        {
            u32 count_dir = 0;
            u32 count_file = 0;
            u32 count_symlink = 0;
            u32 count_socket = 0;
            u32 count_pipe = 0;
            u32 count_block = 0;
            u32 count_char = 0;

            for (const auto& file : selected_files)
            {
                if (!file)
                {
                    continue;
                }

                if (file->is_directory())
                {
                    count_dir += 1;
                }
                else if (file->is_regular_file())
                {
                    count_file += 1;
                }
                else if (file->is_symlink())
                {
                    count_symlink += 1;
                }
                else if (file->is_socket())
                {
                    count_socket += 1;
                }
                else if (file->is_fifo())
                {
                    count_pipe += 1;
                }
                else if (file->is_block_file())
                {
                    count_block += 1;
                }
                else if (file->is_character_file())
                {
                    count_char += 1;
                }
            }

            if (count_dir != 0)
            {
                statusbar_txt.append(std::format("  Directories ({:L})", count_dir));
            }
            if (count_file != 0)
            {
                statusbar_txt.append(std::format("  Files ({:L})", count_file));
            }
            if (count_symlink != 0)
            {
                statusbar_txt.append(std::format("  Symlinks ({:L})", count_symlink));
            }
            if (count_socket != 0)
            {
                statusbar_txt.append(std::format("  Sockets ({:L})", count_socket));
            }
            if (count_pipe != 0)
            {
                statusbar_txt.append(std::format("  Named Pipes ({:L})", count_pipe));
            }
            if (count_block != 0)
            {
                statusbar_txt.append(std::format("  Block Devices ({:L})", count_block));
            }
            if (count_char != 0)
            {
                statusbar_txt.append(std::format("  Character Devices ({:L})", count_char));
            }
        }
    }
    else
    {
        // size of files in dir, does not get subdir size
        u64 disk_size_bytes = 0;
        u64 disk_size_disk = 0;
        if (this->dir_ && this->dir_->is_loaded())
        {
            for (const auto& file : this->dir_->files())
            {
                disk_size_bytes += file->size();
                disk_size_disk += file->size_on_disk();
            }
        }
        const std::string file_size = vfs::utils::format_file_size(disk_size_bytes);
        const std::string disk_size = vfs::utils::format_file_size(disk_size_disk);

        // count for .hidden files
        if (!this->show_hidden_files_ && total_hidden != 0)
        {
            statusbar_txt.append(std::format("{:L} visible ({:L} hidden)  ({} / {})",
                                             total_visible,
                                             total_hidden,
                                             file_size,
                                             disk_size));
        }
        else
        {
            statusbar_txt.append(std::format("{:L} {}  ({} / {})",
                                             total_visible,
                                             total_visible == 1 ? "item" : "items",
                                             file_size,
                                             disk_size));
        }

        // cur dir is a symlink? canonicalize path
        if (std::filesystem::is_symlink(cwd))
        {
            const auto canon = std::filesystem::read_symlink(cwd);
            statusbar_txt.append(std::format("  {} -> {}", cwd.string(), canon.string()));
        }
        else
        {
            statusbar_txt.append(std::format("  {}", cwd.string()));
        }
    }

    gtk_statusbar_pop(this->statusbar, 0);
    gtk_statusbar_push(this->statusbar, 0, statusbar_txt.data());
}

void
gui::browser::on_permission(GtkMenuItem* item,
                            const std::span<const std::shared_ptr<vfs::file>> selected_files,
                            const std::filesystem::path& cwd) noexcept
{
    if (selected_files.empty())
    {
        return;
    }

    const auto set =
        xset::set::get(static_cast<const char*>(g_object_get_data(G_OBJECT(item), "set")));
    if (!set)
    {
        return;
    }

    if (!set->name().starts_with("perm_"))
    {
        return;
    }

    std::string prog;
    if (set->name().starts_with("perm_go") || set->name().starts_with("perm_ugo"))
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
        file_paths = std::format("{} {}", file_paths, vfs::execute::quote(file->name()));
    }

    // task
    gui::file_task* ptask =
        gui_file_exec_new(set->menu.label.value(), cwd, GTK_WIDGET(this), this->task_view_);
    ptask->task->exec_command = std::format("{} {} {}", prog, cmd, file_paths);
    ptask->task->exec_browser = this;
    ptask->task->exec_sync = true;
    ptask->task->exec_show_error = true;
    ptask->task->exec_show_output = false;
    ptask->run();
}

void
gui::browser::on_action(const xset::name setname) noexcept
{
    const auto set = xset::set::get(setname);
    // logger::info<logger::domain::gui>("gui::browser::on_action {}", set->name());

    if (set->name().starts_with("book_"))
    {
        if (set->xset_name == xset::name::book_add)
        {
            gui::view::bookmark::add(this->cwd());
        }
    }
    else if (set->name().starts_with("go_"))
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
    }
    else if (set->name().starts_with("tab_"))
    {
        if (set->xset_name == xset::name::tab_new || set->xset_name == xset::name::tab_new_here)
        {
            if (this->settings_->new_tab_here)
            {
                this->new_tab_here();
            }
            else
            {
                this->new_tab();
            }
        }
        else
        {
            tab_t i = INVALID_TAB;
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
                const auto tab = ztd::removeprefix(set->name(), "tab_");
                i = tab_t::create(tab).value_or(INVALID_TAB);
            }
            this->go_tab(i);
        }
    }
    else if (set->name().starts_with("focus_"))
    {
        gui::browser::focus_widget widget = gui::browser::focus_widget::invalid;
        if (set->xset_name == xset::name::focus_path_bar)
        {
            widget = gui::browser::focus_widget::path_bar;
        }
        else if (set->xset_name == xset::name::focus_search_bar)
        {
            widget = gui::browser::focus_widget::search_bar;
        }
        else if (set->xset_name == xset::name::focus_filelist)
        {
            widget = gui::browser::focus_widget::filelist;
        }
        else if (set->xset_name == xset::name::focus_dirtree)
        {
            widget = gui::browser::focus_widget::dirtree;
        }
        else if (set->xset_name == xset::name::focus_device)
        {
            widget = gui::browser::focus_widget::device;
        }
        this->focus(widget);
    }
    else if (set->xset_name == xset::name::view_reorder_col)
    {
        gui::view::file_task::on_reorder(nullptr, GTK_WIDGET(this));
    }
    else if (set->xset_name == xset::name::view_refresh)
    {
        this->refresh();
    }
    else if (set->xset_name == xset::name::view_thumb)
    {
        main_window_toggle_thumbnails_all_windows();
    }
    else if (set->name().starts_with("sortby_"))
    {
        std::int32_t i = -3;
        if (set->xset_name == xset::name::sortby_name)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::name);
        }
        else if (set->xset_name == xset::name::sortby_size)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::size);
        }
        else if (set->xset_name == xset::name::sortby_bytes)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::bytes);
        }
        else if (set->xset_name == xset::name::sortby_type)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::type);
        }
        else if (set->xset_name == xset::name::sortby_mime)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::mime);
        }
        else if (set->xset_name == xset::name::sortby_perm)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::perm);
        }
        else if (set->xset_name == xset::name::sortby_owner)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::owner);
        }
        else if (set->xset_name == xset::name::sortby_group)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::group);
        }
        else if (set->xset_name == xset::name::sortby_atime)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::atime);
        }
        else if (set->xset_name == xset::name::sortby_btime)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::btime);
        }
        else if (set->xset_name == xset::name::sortby_ctime)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::ctime);
        }
        else if (set->xset_name == xset::name::sortby_mtime)
        {
            i = magic_enum::enum_integer(gui::browser::sort_order::mtime);
        }
        else if (set->xset_name == xset::name::sortby_ascend)
        {
            i = -1;
            set->b = this->sort_type_ == GtkSortType::GTK_SORT_ASCENDING ? xset::set::enabled::yes
                                                                         : xset::set::enabled::no;
        }
        else if (set->xset_name == xset::name::sortby_descend)
        {
            i = -2;
            set->b = this->sort_type_ == GtkSortType::GTK_SORT_DESCENDING ? xset::set::enabled::yes
                                                                          : xset::set::enabled::no;
        }
        if (i > 0)
        { // always want to show name
            set->b = this->sort_order_ == gui::browser::sort_order(i) ? xset::set::enabled::yes
                                                                      : xset::set::enabled::no;
        }
        on_popup_sortby(nullptr, this, i);
    }
    else if (set->name().starts_with("sortx_"))
    {
        this->set_sort_extra(set->xset_name);
    }
    else if (set->name().starts_with("panel"))
    {
        const auto mode = this->main_window_->panel_context.at(this->panel_);

        const auto panel_num = ztd::removeprefix(set->name(), "panel_");
        const auto panel = panel_t::create(panel_num).value_or(INVALID_PANEL);
        // logger::debug<logger::domain::gui>("ACTION panel={}", panel);

        if (is_valid_panel(panel))
        {
            const std::string fullxname = std::format("panel{}_", panel);
            const std::string xname = ztd::removeprefix(set->name(), fullxname);
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
                const auto set2 = xset::set::get(xname, this->panel_, mode);
                set2->b = set2->b == xset::set::enabled::yes ? xset::set::enabled::unset
                                                             : xset::set::enabled::yes;
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
                if (this->view_mode_ != gui::browser::view_mode::icon_view)
                {
                    xset_set_b_panel(this->panel_, xset::panel::list_large, !this->large_icons_);
                    on_popup_list_large(nullptr, this);
                }
            }
            else if (xname.starts_with("detcol_") // shared key
                     && this->view_mode_ == gui::browser::view_mode::list_view)
            {
                const auto set2 = xset::set::get(xname, this->panel_, mode);
                set2->b = set2->b == xset::set::enabled::yes ? xset::set::enabled::unset
                                                             : xset::set::enabled::yes;
                update_views_all_windows(nullptr, this);
            }
        }
    }
    else if (set->name().starts_with("status_"))
    {
        if (set->name() == "status_border" || set->name() == "status_text")
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
    else if (set->name().starts_with("paste_"))
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
            gui::action::paste_files(this, this->cwd());
        }
    }
    else if (set->name().starts_with("select_"))
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
    else // all the rest require filemenu data
    {
        gui_file_menu_action(this, set);
    }
}

// Default signal handlers

void
gui::browser::focus_folder_view() noexcept
{
    gtk_widget_grab_focus(GTK_WIDGET(this->folder_view_));

    this->signal_change_pane_.emit(this);
}

bool
gui_browser_delay_focus(gui::browser* browser) noexcept
{
    if (GTK_IS_WIDGET(browser) && GTK_IS_WIDGET(browser->folder_view()))
    {
        // logger::info<logger::domain::gui>("delayed_focus_browser fb={}", logger::utils::ptr(browser));
        if (GTK_IS_WIDGET(browser) && GTK_IS_WIDGET(browser->folder_view()))
        {
            gtk_widget_grab_focus(browser->folder_view());
            set_panel_focus(nullptr, browser);
        }
    }
    return false;
}

// xset callback wrapper functions

void
gui::wrapper::browser::go_home(GtkWidget* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->go_home();
}

void
gui::wrapper::browser::go_tab(GtkMenuItem* item, gui::browser* browser) noexcept
{
    const tab_t tab = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tab"));
    browser->go_tab(tab);
}

void
gui::wrapper::browser::go_back(GtkWidget* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->go_back();
}

void
gui::wrapper::browser::go_forward(GtkWidget* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->go_forward();
}

void
gui::wrapper::browser::go_up(GtkWidget* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->go_up();
}

void
gui::wrapper::browser::refresh(GtkWidget* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->refresh();
}

void
gui::wrapper::browser::new_tab(GtkMenuItem* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->new_tab();
}

void
gui::wrapper::browser::new_tab_here(GtkMenuItem* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->new_tab_here();
}

void
gui::wrapper::browser::close_tab(GtkMenuItem* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->close_tab();
}

void
gui::wrapper::browser::restore_tab(GtkMenuItem* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->restore_tab();
}

void
gui::wrapper::browser::select_all(GtkWidget* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->select_all();
}

void
gui::wrapper::browser::unselect_all(GtkWidget* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->unselect_all();
}

void
gui::wrapper::browser::invert_selection(GtkWidget* item, gui::browser* browser) noexcept
{
    (void)item;
    browser->invert_selection();
}

void
gui::wrapper::browser::focus(GtkMenuItem* item, gui::browser* browser) noexcept
{
    const auto job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "focus"));

    const auto enum_value = magic_enum::enum_cast<gui::browser::focus_widget>((std::uint8_t)job);
    if (enum_value.has_value())
    {
        browser->focus(enum_value.value());
    }
}

bool
gui::wrapper::browser::slider_release(GtkWidget* widget, GdkEvent* event,
                                      gui::browser* browser) noexcept
{
    (void)event;
    return browser->slider_release(GTK_PANED(widget));
}
