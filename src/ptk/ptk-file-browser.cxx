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

#include <optional>
#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <span>

#include <array>
#include <vector>

#include <cassert>

#include <malloc.h>

#include <fcntl.h>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <exo/exo.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <magic_enum.hpp>

#include "xset/xset.hxx"
#include "xset/xset-context.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-event-handler.hxx"

#include "ptk/ptk-error.hxx"

#include "ptk/ptk-file-actions-open.hxx"
#include "ptk/ptk-file-actions-rename.hxx"

#include "ptk/ptk-bookmark-view.hxx"
#include "ptk/ptk-file-properties.hxx"
#include "ptk/ptk-location-view.hxx"
#include "ptk/ptk-dir-tree-view.hxx"
#include "ptk/ptk-dir-tree.hxx"

#include "ptk/ptk-file-list.hxx"

#include "ptk/ptk-clipboard.hxx"

#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-path-entry.hxx"
#include "main-window.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "settings/app.hxx"

#include "signals.hxx"

#include "settings.hxx"
#include "utils.hxx"
#include "type-conversion.hxx"

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

static void on_dir_file_listed(PtkFileBrowser* file_browser, bool is_cancelled);

static void ptk_file_browser_update_model(PtkFileBrowser* file_browser);

/* Get GtkTreePath of the item at coordinate x, y */
static GtkTreePath* folder_view_get_tree_path_at_pos(PtkFileBrowser* file_browser, i32 x, i32 y);

/* signal handlers */
static void on_folder_view_item_activated(ExoIconView* iconview, GtkTreePath* path,
                                          PtkFileBrowser* file_browser);
static void on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path,
                                         GtkTreeViewColumn* col, PtkFileBrowser* file_browser);
static void on_folder_view_item_sel_change(ExoIconView* iconview, PtkFileBrowser* file_browser);

static bool on_folder_view_button_press_event(GtkWidget* widget, GdkEventButton* event,
                                              PtkFileBrowser* file_browser);
static bool on_folder_view_button_release_event(GtkWidget* widget, GdkEventButton* event,
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
static void focus_folder_view(PtkFileBrowser* file_browser);
static void enable_toolbar(PtkFileBrowser* file_browser);
static void show_thumbnails(PtkFileBrowser* file_browser, PtkFileList* list, bool is_big,
                            i32 max_file_size);

static i32 file_list_order_from_sort_order(ptk::file_browser::sort_order order);

static GtkPanedClass* parent_class = nullptr;

static void rebuild_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser);
static void rebuild_side_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser);

static u32 folder_view_auto_scroll_timer = 0;
static GtkDirectionType folder_view_auto_scroll_direction = GtkDirectionType::GTK_DIR_TAB_FORWARD;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {{ztd::strdup("text/uri-list"), 0, 0}};

#define GDK_ACTION_ALL                                                              \
    GdkDragAction(GdkDragAction::GDK_ACTION_MOVE | GdkDragAction::GDK_ACTION_COPY | \
                  GdkDragAction::GDK_ACTION_LINK)

// instance-wide command history
std::vector<std::string> xset_cmd_history;

// must match main-window.c  main_window_socket_command
inline constexpr std::array<const std::string_view, 6> column_titles{
    "Name",
    "Size",
    "Type",
    "Permission",
    "Owner",
    "Modified",
};

inline constexpr std::array<xset::panel, 6> column_names{
    xset::panel::detcol_name,
    xset::panel::detcol_size,
    xset::panel::detcol_type,
    xset::panel::detcol_perm,
    xset::panel::detcol_owner,
    xset::panel::detcol_date,
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
ptk_file_browser_slider_release(GtkWidget* widget, GdkEventButton* event,
                                PtkFileBrowser* file_browser)
{
    (void)event;

    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    const panel_t p = file_browser->mypanel;
    const xset::main_window_panel mode = main_window->panel_context.at(p);

    xset_t set = xset_get_panel_mode(p, xset::panel::slider_positions, mode);

    if (widget == file_browser->hpane)
    {
        const i32 pos = gtk_paned_get_position(GTK_PANED(file_browser->hpane));
        if (!main_window->fullscreen)
        {
            set->x = std::to_string(pos);
        }
        main_window->panel_slide_x[p - 1] = pos;
        // ztd::logger::info("    slide_x = {}", pos);
    }
    else
    {
        i32 pos;
        // ztd::logger::info("ptk_file_browser_slider_release fb={:p}  (panel {})  mode = {}",
        // file_browser, p, fmt::ptr(mode));
        pos = gtk_paned_get_position(GTK_PANED(file_browser->side_vpane_top));
        if (!main_window->fullscreen)
        {
            set->y = std::to_string(pos);
        }
        main_window->panel_slide_y[p - 1] = pos;
        // ztd::logger::info("    slide_y = {}  ", pos);

        pos = gtk_paned_get_position(GTK_PANED(file_browser->side_vpane_bottom));
        if (!main_window->fullscreen)
        {
            set->s = std::to_string(pos);
        }
        main_window->panel_slide_s[p - 1] = pos;
        // ztd::logger::info("slide_s = {}", pos);
    }
    return false;
}

void
ptk_file_browser_select_file(PtkFileBrowser* file_browser, const std::filesystem::path& path)
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model = nullptr;
    vfs::file_info file;

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(file_browser->file_list);
    if (file_browser->view_mode == ptk::file_browser::view_mode::icon_view ||
        file_browser->view_mode == ptk::file_browser::view_mode::compact_view)
    {
        exo_icon_view_unselect_all(EXO_ICON_VIEW(file_browser->folder_view));
        model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
    }
    else if (file_browser->view_mode == ptk::file_browser::view_mode::list_view)
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
        gtk_tree_selection_unselect_all(tree_sel);
    }
    if (!model)
    {
        return;
    }

    if (gtk_tree_model_get_iter_first(model, &it))
    {
        const std::string name = path.filename();

        do
        {
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (file)
            {
                const std::string& file_name = file->get_name();
                if (ztd::same(file_name, name))
                {
                    GtkTreePath* tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (file_browser->view_mode == ptk::file_browser::view_mode::icon_view ||
                        file_browser->view_mode == ptk::file_browser::view_mode::compact_view)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                  tree_path);
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                     tree_path,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    else if (file_browser->view_mode == ptk::file_browser::view_mode::list_view)
                    {
                        gtk_tree_selection_select_path(tree_sel, tree_path);
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                     tree_path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    gtk_tree_path_free(tree_path);
                    vfs_file_info_unref(file);
                    break;
                }
                vfs_file_info_unref(file);
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
}

static void
save_command_history(GtkEntry* entry)
{
    const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));

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
    ptk_file_browser_focus_me(file_browser);
    return false;
}

static void
on_address_bar_activate(GtkWidget* entry, PtkFileBrowser* file_browser)
{
    const char* text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!text || std::strlen(text) == 0)
    {
        return;
    }

    gtk_editable_select_region(GTK_EDITABLE(entry), 0, 0); // clear selection

    // network path
    if ((!ztd::startswith(text, "/") && ztd::contains(text, ":/")) || ztd::startswith(text, "//"))
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
        if (!std::filesystem::equivalent(dir_path, ptk_file_browser_get_cwd(file_browser)))
        {
            ptk_file_browser_chdir(file_browser,
                                   dir_path,
                                   ptk::file_browser::chdir_mode::add_history);
        }
    }
    else if (std::filesystem::is_regular_file(dir_path))
    { // open dir and select file
        const auto dirname_path = dir_path.parent_path();
        if (!std::filesystem::equivalent(dirname_path, ptk_file_browser_get_cwd(file_browser)))
        {
            std::free(file_browser->select_path);
            file_browser->select_path = ztd::strdup(dir_path);
            ptk_file_browser_chdir(file_browser,
                                   dirname_path,
                                   ptk::file_browser::chdir_mode::add_history);
        }
        else
        {
            ptk_file_browser_select_file(file_browser, dir_path);
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

    gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
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
ptk_file_browser_add_toolbar_widget(xset_t set, GtkWidget* widget)
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

void
ptk_file_browser_update_toolbar_widgets(PtkFileBrowser* file_browser, xset_t set,
                                        xset::tool tool_type)
{
    (void)tool_type;

    assert(set != nullptr);
    assert(file_browser != nullptr);

    // if (!PTK_IS_FILE_BROWSER(file_browser))
    // {
    //     return;
    // }

    if (set && !set->lock && set->menu_style == xset::menu::check &&
        set->tool == xset::tool::custom)
    {
        // a custom checkbox is being updated
        for (GSList* l = file_browser->toolbar_widgets[7]; l; l = g_slist_next(l))
        {
            xset_t test_set =
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
        ztd::logger::warn("ptk_file_browser_update_toolbar_widget widget not found for set");
        return;
    }
    else if (set)
    {
        ztd::logger::warn("ptk_file_browser_update_toolbar_widget invalid set");
        return;
    }
}

void
ptk_file_browser_update_toolbar_widgets(PtkFileBrowser* file_browser, xset::tool tool_type)
{
    assert(file_browser != nullptr);

    // if (!PTK_IS_FILE_BROWSER(file_browser))
    // {
    //     return;
    // }

    // builtin tool
    bool b = false;
    unsigned char x;

    switch (tool_type)
    {
        case xset::tool::up:
            x = 0;
            b = !std::filesystem::equivalent(ptk_file_browser_get_cwd(file_browser), "/");
            break;
        case xset::tool::back:
        case xset::tool::back_menu:
            x = 1;
            b = file_browser->curHistory && file_browser->curHistory->prev;
            break;
        case xset::tool::fwd:
        case xset::tool::fwd_menu:
            x = 2;
            b = file_browser->curHistory && file_browser->curHistory->next;
            break;
        case xset::tool::devices:
            x = 3;
            b = !!file_browser->side_dev;
            break;
        case xset::tool::bookmarks:
            x = 4;
            break;
        case xset::tool::tree:
            x = 5;
            b = !!file_browser->side_dir;
            break;
        case xset::tool::show_hidden:
            x = 6;
            b = file_browser->show_hidden_files;
            break;
        case xset::tool::show_thumb:
            x = 8;
            b = app_settings.get_show_thumbnail();
            break;
        case xset::tool::large_icons:
            x = 9;
            b = file_browser->large_icons;
            break;
        case xset::tool::NOT:
        case xset::tool::custom:
        case xset::tool::home:
        case xset::tool::DEFAULT:
        case xset::tool::refresh:
        case xset::tool::new_tab:
        case xset::tool::new_tab_here:
        case xset::tool::invalid:
            ztd::logger::warn("ptk_file_browser_update_toolbar_widget invalid tool_type");
            return;
    }

    // update all widgets in list
    for (GSList* l = file_browser->toolbar_widgets[x]; l; l = g_slist_next(l))
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
            ztd::logger::warn("ptk_file_browser_update_toolbar_widget invalid widget");
        }
    }
}

static void
enable_toolbar(PtkFileBrowser* file_browser)
{
    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::back);
    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::fwd);
    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::up);
    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::devices);
    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::tree);
    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::show_hidden);
    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::show_thumb);
    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::large_icons);
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

    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    const panel_t p = file_browser->mypanel;
    const xset::main_window_panel mode = main_window->panel_context.at(p);

    const bool show_tooltips = !xset_get_b_panel(1, xset::panel::tool_l);

    // destroy
    if (file_browser->toolbar)
    {
        if (GTK_IS_WIDGET(file_browser->toolbar))
        {
            gtk_widget_destroy(file_browser->toolbar);
        }
        file_browser->toolbar = nullptr;
        file_browser->path_bar = nullptr;
    }

    if (!file_browser->path_bar)
    {
        file_browser->path_bar = ptk_path_entry_new(file_browser);
        g_signal_connect(file_browser->path_bar,
                         "activate",
                         G_CALLBACK(on_address_bar_activate),
                         file_browser);
        g_signal_connect(file_browser->path_bar,
                         "focus-in-event",
                         G_CALLBACK(on_address_bar_focus_in),
                         file_browser);
    }

    // create toolbar
    file_browser->toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(file_browser->toolbox), file_browser->toolbar, true, true, 0);
    gtk_toolbar_set_style(GTK_TOOLBAR(file_browser->toolbar), GtkToolbarStyle::GTK_TOOLBAR_ICONS);
    if (app_settings.get_icon_size_tool() > 0 &&
        app_settings.get_icon_size_tool() <= GtkIconSize::GTK_ICON_SIZE_DIALOG)
    {
        gtk_toolbar_set_icon_size(GTK_TOOLBAR(file_browser->toolbar),
                                  (GtkIconSize)app_settings.get_icon_size_tool());
    }

    // fill left toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->toolbar,
                      xset_get_panel(p, xset::panel::tool_l),
                      show_tooltips);

    // add pathbar
    GtkWidget* hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    GtkToolItem* toolitem = gtk_tool_item_new();
    gtk_tool_item_set_expand(toolitem, true);
    gtk_toolbar_insert(GTK_TOOLBAR(file_browser->toolbar), toolitem, -1);
    gtk_container_add(GTK_CONTAINER(toolitem), hbox);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(file_browser->path_bar), true, true, 5);

    // fill right toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->toolbar,
                      xset_get_panel(p, xset::panel::tool_r),
                      show_tooltips);

    // show
    if (xset_get_b_panel_mode(p, xset::panel::show_toolbox, mode))
    {
        gtk_widget_show_all(file_browser->toolbox);
    }
}

static void
rebuild_side_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    (void)widget;
    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    const panel_t p = file_browser->mypanel;
    const xset::main_window_panel mode =
        main_window ? main_window->panel_context.at(p) : xset::main_window_panel::panel_neither;

    const bool show_tooltips = !xset_get_b_panel(1, xset::panel::tool_l);

    // destroy
    if (file_browser->side_toolbar)
    {
        gtk_widget_destroy(file_browser->side_toolbar);
    }

    // create side toolbar
    file_browser->side_toolbar = gtk_toolbar_new();

    gtk_box_pack_start(GTK_BOX(file_browser->side_toolbox),
                       file_browser->side_toolbar,
                       true,
                       true,
                       0);
    gtk_toolbar_set_style(GTK_TOOLBAR(file_browser->side_toolbar),
                          GtkToolbarStyle::GTK_TOOLBAR_ICONS);
    if (app_settings.get_icon_size_tool() > 0 &&
        app_settings.get_icon_size_tool() <= GtkIconSize::GTK_ICON_SIZE_DIALOG)
    {
        gtk_toolbar_set_icon_size(GTK_TOOLBAR(file_browser->side_toolbar),
                                  (GtkIconSize)app_settings.get_icon_size_tool());
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
        gtk_widget_show_all(file_browser->side_toolbox);
    }
}

void
ptk_file_browser_rebuild_toolbars(PtkFileBrowser* file_browser)
{
    for (auto& toolbar_widget : file_browser->toolbar_widgets)
    {
        g_slist_free(toolbar_widget);
        toolbar_widget = nullptr;
    }
    if (file_browser->toolbar)
    {
        rebuild_toolbox(nullptr, file_browser);
        const auto cwd = ptk_file_browser_get_cwd(file_browser);
        gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), cwd.c_str());
    }
    if (file_browser->side_toolbar)
    {
        rebuild_side_toolbox(nullptr, file_browser);
    }

    enable_toolbar(file_browser);
}

static bool
on_status_bar_button_press(GtkWidget* widget, GdkEventButton* event, PtkFileBrowser* file_browser)
{
    (void)widget;
    focus_folder_view(file_browser);
    if (event->type == GdkEventType::GDK_BUTTON_PRESS)
    {
        if ((event_handler->win_click->s || event_handler->win_click->ob2_data) &&
            main_window_event(file_browser->main_window,
                              event_handler->win_click,
                              xset::name::evt_win_click,
                              0,
                              0,
                              "statusbar",
                              0,
                              event->button,
                              event->state,
                              true))
        {
            return true;
        }
        if (event->button == 2)
        {
            static constexpr std::array<xset::name, 4> setnames{
                xset::name::status_name,
                xset::name::status_path,
                xset::name::status_info,
                xset::name::status_hide,
            };

            for (const auto i : ztd::range(setnames.size()))
            {
                if (!xset_get_b(setnames.at(i)))
                {
                    continue;
                }

                if (i < 2)
                {
                    const std::vector<vfs::file_info> sel_files =
                        ptk_file_browser_get_selected_files(file_browser);
                    if (sel_files.empty())
                    {
                        return true;
                    }

                    if (i == 0)
                    {
                        ptk_clipboard_copy_name(ptk_file_browser_get_cwd(file_browser), sel_files);
                    }
                    else
                    {
                        ptk_clipboard_copy_as_text(ptk_file_browser_get_cwd(file_browser),
                                                   sel_files);
                    }

                    vfs_file_info_list_free(sel_files);
                }
                else if (i == 2)
                {
                    ptk_file_browser_file_properties(file_browser, 0);
                }
                else if (i == 3)
                {
                    focus_panel(nullptr, file_browser->main_window, panel_control_code_hide);
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
on_status_middle_click_config(GtkMenuItem* menuitem, xset_t set)
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
    const xset_context_t context = xset_context_new();
    main_context_fill(file_browser, context);
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    const std::string desc =
        std::format("separator panel{}_icon_status status_middle", file_browser->mypanel);

    xset_set_cb_panel(file_browser->mypanel,
                      xset::panel::icon_status,
                      (GFunc)on_status_effect_change,
                      file_browser);
    xset_t set = xset_get(xset::name::status_name);
    xset_set_cb(xset::name::status_name, (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, nullptr);
    xset_t set_radio = set;
    set = xset_get(xset::name::status_path);
    xset_set_cb(xset::name::status_path, (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set = xset_get(xset::name::status_info);
    xset_set_cb(xset::name::status_info, (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set = xset_get(xset::name::status_hide);
    xset_set_cb(xset::name::status_hide, (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, set_radio->name.data());

    xset_add_menu(file_browser, menu, accel_group, desc);
    gtk_widget_show_all(menu);
    g_signal_connect(menu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
}

static void
ptk_file_browser_init(PtkFileBrowser* file_browser)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(file_browser),
                                   GtkOrientation::GTK_ORIENTATION_VERTICAL);

    file_browser->mypanel = 0; // do not load font yet in ptk_path_entry_new
    file_browser->path_bar = ptk_path_entry_new(file_browser);
    g_signal_connect(file_browser->path_bar,
                     "activate",
                     G_CALLBACK(on_address_bar_activate),
                     file_browser);
    g_signal_connect(file_browser->path_bar,
                     "focus-in-event",
                     G_CALLBACK(on_address_bar_focus_in),
                     file_browser);

    // toolbox
    file_browser->toolbar = nullptr;
    file_browser->toolbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(file_browser), file_browser->toolbox, false, false, 0);

    // lists area
    file_browser->hpane = gtk_paned_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL);
    file_browser->side_vbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(file_browser->side_vbox, 140, -1);
    file_browser->folder_view_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_paned_pack1(GTK_PANED(file_browser->hpane), file_browser->side_vbox, false, false);
    gtk_paned_pack2(GTK_PANED(file_browser->hpane), file_browser->folder_view_scroll, true, true);

    // fill side
    file_browser->side_toolbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    file_browser->side_toolbar = nullptr;
    file_browser->side_vpane_top = gtk_paned_new(GtkOrientation::GTK_ORIENTATION_VERTICAL);
    file_browser->side_vpane_bottom = gtk_paned_new(GtkOrientation::GTK_ORIENTATION_VERTICAL);
    file_browser->side_dir_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    file_browser->side_dev_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_box_pack_start(GTK_BOX(file_browser->side_vbox),
                       file_browser->side_toolbox,
                       false,
                       false,
                       0);
    gtk_box_pack_start(GTK_BOX(file_browser->side_vbox),
                       file_browser->side_vpane_top,
                       true,
                       true,
                       0);
    // see https://github.com/BwackNinja/spacefm/issues/21
    gtk_paned_pack1(GTK_PANED(file_browser->side_vpane_top),
                    file_browser->side_dev_scroll,
                    false,
                    false);
    gtk_paned_pack2(GTK_PANED(file_browser->side_vpane_top),
                    file_browser->side_vpane_bottom,
                    true,
                    false);
    gtk_paned_pack2(GTK_PANED(file_browser->side_vpane_bottom),
                    file_browser->side_dir_scroll,
                    true,
                    false);

    // status bar
    file_browser->status_bar = gtk_statusbar_new();

    GList* children = gtk_container_get_children(GTK_CONTAINER(file_browser->status_bar));
    file_browser->status_frame = GTK_FRAME(children->data);
    g_list_free(children);
    children = gtk_container_get_children(
        GTK_CONTAINER(gtk_statusbar_get_message_area(GTK_STATUSBAR(file_browser->status_bar))));
    file_browser->status_label = GTK_LABEL(children->data);
    g_list_free(children);
    // do not know panel yet
    file_browser->status_image = xset_get_image("gtk-yes", GtkIconSize::GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(file_browser->status_bar),
                       file_browser->status_image,
                       false,
                       false,
                       0);
    // required for button event
    gtk_label_set_selectable(file_browser->status_label, true);
    gtk_widget_set_can_focus(GTK_WIDGET(file_browser->status_label), false);
    gtk_widget_set_hexpand(GTK_WIDGET(file_browser->status_label), true);
    gtk_widget_set_halign(GTK_WIDGET(file_browser->status_label), GtkAlign::GTK_ALIGN_FILL);
    gtk_widget_set_halign(GTK_WIDGET(file_browser->status_label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(file_browser->status_label), GtkAlign::GTK_ALIGN_CENTER);

    g_signal_connect(G_OBJECT(file_browser->status_label),
                     "button-press-event",
                     G_CALLBACK(on_status_bar_button_press),
                     file_browser);
    g_signal_connect(G_OBJECT(file_browser->status_label),
                     "populate-popup",
                     G_CALLBACK(on_status_bar_popup),
                     file_browser);

    // pack fb vbox
    gtk_box_pack_start(GTK_BOX(file_browser), file_browser->hpane, true, true, 0);
    // TODO pack task frames
    gtk_box_pack_start(GTK_BOX(file_browser), file_browser->status_bar, false, false, 0);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->side_dir_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->side_dev_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);

    g_signal_connect(file_browser->hpane,
                     "button-release-event",
                     G_CALLBACK(ptk_file_browser_slider_release),
                     file_browser);
    g_signal_connect(file_browser->side_vpane_top,
                     "button-release-event",
                     G_CALLBACK(ptk_file_browser_slider_release),
                     file_browser);
    g_signal_connect(file_browser->side_vpane_bottom,
                     "button-release-event",
                     G_CALLBACK(ptk_file_browser_slider_release),
                     file_browser);
}

static void
ptk_file_browser_finalize(GObject* obj)
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(obj);
    // ztd::logger::info("ptk_file_browser_finalize");
    if (file_browser->dir)
    {
        g_signal_handlers_disconnect_matched(file_browser->dir,
                                             GSignalMatchType::G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(file_browser->dir);
    }

    /* Remove all idle handlers which are not called yet. */
    do
    {
    } while (g_source_remove_by_user_data(file_browser));

    if (file_browser->file_list)
    {
        g_signal_handlers_disconnect_matched(file_browser->file_list,
                                             GSignalMatchType::G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(G_OBJECT(file_browser->file_list));
    }

    std::free(file_browser->status_bar_custom);
    std::free(file_browser->seek_name);
    file_browser->seek_name = nullptr;
    std::free(file_browser->book_set_name);
    file_browser->book_set_name = nullptr;
    std::free(file_browser->select_path);
    file_browser->select_path = nullptr;
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
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
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

void
ptk_file_browser_update_views(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    // ztd::logger::info("ptk_file_browser_update_views fb={:p}  (panel {})", file_browser,
    // file_browser->mypanel);

    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    // hide/show browser widgets based on user settings
    const panel_t p = file_browser->mypanel;
    const xset::main_window_panel mode = main_window->panel_context.at(p);
    bool need_enable_toolbar = false;

    if (xset_get_b_panel_mode(p, xset::panel::show_toolbox, mode))
    {
        if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
            (!file_browser->toolbar || !gtk_widget_get_visible(file_browser->toolbox)))
        {
            main_window_event(main_window,
                              event_handler->pnl_show,
                              xset::name::evt_pnl_show,
                              0,
                              0,
                              "toolbar",
                              0,
                              0,
                              0,
                              true);
        }
        if (!file_browser->toolbar)
        {
            rebuild_toolbox(nullptr, file_browser);
            need_enable_toolbar = true;
        }
        gtk_widget_show_all(file_browser->toolbox);
    }
    else
    {
        if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
            file_browser->toolbox && gtk_widget_get_visible(file_browser->toolbox))
        {
            main_window_event(main_window,
                              event_handler->pnl_show,
                              xset::name::evt_pnl_show,
                              0,
                              0,
                              "toolbar",
                              0,
                              0,
                              0,
                              false);
        }
        gtk_widget_hide(file_browser->toolbox);
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_sidebar, mode))
    {
        if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
            (!file_browser->side_toolbox || !gtk_widget_get_visible(file_browser->side_toolbox)))
        {
            main_window_event(main_window,
                              event_handler->pnl_show,
                              xset::name::evt_pnl_show,
                              0,
                              0,
                              "sidetoolbar",
                              0,
                              0,
                              0,
                              true);
        }
        if (!file_browser->side_toolbar)
        {
            rebuild_side_toolbox(nullptr, file_browser);
            need_enable_toolbar = true;
        }
        gtk_widget_show_all(file_browser->side_toolbox);
    }
    else
    {
        if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
            file_browser->side_toolbar && file_browser->side_toolbox &&
            gtk_widget_get_visible(file_browser->side_toolbox))
        {
            main_window_event(main_window,
                              event_handler->pnl_show,
                              xset::name::evt_pnl_show,
                              0,
                              0,
                              "sidetoolbar",
                              0,
                              0,
                              0,
                              false);
        }
        /*  toolboxes must be destroyed together for toolbar_widgets[]
        if ( file_browser->side_toolbar )
        {
            gtk_widget_destroy( file_browser->side_toolbar );
            file_browser->side_toolbar = nullptr;
        }
        */
        gtk_widget_hide(file_browser->side_toolbox);
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
    {
        if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
            (!file_browser->side_dir_scroll ||
             !gtk_widget_get_visible(file_browser->side_dir_scroll)))
        {
            main_window_event(main_window,
                              event_handler->pnl_show,
                              xset::name::evt_pnl_show,
                              0,
                              0,
                              "dirtree",
                              0,
                              0,
                              0,
                              true);
        }
        if (!file_browser->side_dir)
        {
            file_browser->side_dir = ptk_file_browser_create_dir_tree(file_browser);
            gtk_container_add(GTK_CONTAINER(file_browser->side_dir_scroll), file_browser->side_dir);
        }
        gtk_widget_show_all(file_browser->side_dir_scroll);
        if (file_browser->side_dir && file_browser->file_list)
        {
            ptk_dir_tree_view_chdir(GTK_TREE_VIEW(file_browser->side_dir),
                                    ptk_file_browser_get_cwd(file_browser));
        }
    }
    else
    {
        if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
            file_browser->side_dir_scroll && gtk_widget_get_visible(file_browser->side_dir_scroll))
        {
            main_window_event(main_window,
                              event_handler->pnl_show,
                              xset::name::evt_pnl_show,
                              0,
                              0,
                              "dirtree",
                              0,
                              0,
                              0,
                              false);
        }
        gtk_widget_hide(file_browser->side_dir_scroll);
        if (file_browser->side_dir)
        {
            gtk_widget_destroy(file_browser->side_dir);
        }
        file_browser->side_dir = nullptr;
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_devmon, mode))
    {
        if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
            (!file_browser->side_dev_scroll ||
             !gtk_widget_get_visible(file_browser->side_dev_scroll)))
        {
            main_window_event(main_window,
                              event_handler->pnl_show,
                              xset::name::evt_pnl_show,
                              0,
                              0,
                              "devices",
                              0,
                              0,
                              0,
                              true);
        }
        if (!file_browser->side_dev)
        {
            file_browser->side_dev = ptk_location_view_new(file_browser);
            gtk_container_add(GTK_CONTAINER(file_browser->side_dev_scroll), file_browser->side_dev);
        }
        gtk_widget_show_all(file_browser->side_dev_scroll);
    }
    else
    {
        if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
            file_browser->side_dev_scroll && gtk_widget_get_visible(file_browser->side_dev_scroll))
        {
            main_window_event(main_window,
                              event_handler->pnl_show,
                              xset::name::evt_pnl_show,
                              0,
                              0,
                              "devices",
                              0,
                              0,
                              0,
                              false);
        }
        gtk_widget_hide(file_browser->side_dev_scroll);
        if (file_browser->side_dev)
        {
            gtk_widget_destroy(file_browser->side_dev);
        }
        file_browser->side_dev = nullptr;
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
    {
        gtk_widget_show(file_browser->side_vpane_bottom);
    }
    else
    {
        gtk_widget_hide(file_browser->side_vpane_bottom);
    }

    if (xset_get_b_panel_mode(p, xset::panel::show_devmon, mode) ||
        xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
    {
        gtk_widget_show(file_browser->side_vbox);
    }
    else
    {
        gtk_widget_hide(file_browser->side_vbox);
    }

    if (need_enable_toolbar)
    {
        enable_toolbar(file_browser);
    }
    else
    {
        // toggle sidepane toolbar buttons
        ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::devices);
        ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::tree);
    }

    // set slider positions

    // hpane
    i32 pos = main_window->panel_slide_x[p - 1];
    if (pos < 100)
    {
        pos = -1;
    }
    // ztd::logger::info("    set slide_x = {}", pos);
    if (pos > 0)
    {
        gtk_paned_set_position(GTK_PANED(file_browser->hpane), pos);
    }

    // side_vpane_top
    pos = main_window->panel_slide_y[p - 1];
    if (pos < 20)
    {
        pos = -1;
    }
    // ztd::logger::info("    slide_y = {}", pos);
    gtk_paned_set_position(GTK_PANED(file_browser->side_vpane_top), pos);

    // side_vpane_bottom
    pos = main_window->panel_slide_s[p - 1];
    if (pos < 20)
    {
        pos = -1;
    }
    // ztd::logger::info( "slide_s = {}", pos);
    gtk_paned_set_position(GTK_PANED(file_browser->side_vpane_bottom), pos);

    // Large Icons - option for Detailed and Compact list views
    const bool large_icons = xset_get_b_panel(p, xset::panel::list_icons) ||
                             xset_get_b_panel_mode(p, xset::panel::list_large, mode);
    if (large_icons != !!file_browser->large_icons)
    {
        if (file_browser->folder_view)
        {
            // force rebuild of folder_view for icon size change
            gtk_widget_destroy(file_browser->folder_view);
            file_browser->folder_view = nullptr;
        }
        file_browser->large_icons = large_icons;
        ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::large_icons);
    }

    // List Styles
    if (xset_get_b_panel(p, xset::panel::list_detailed))
    {
        ptk_file_browser_view_as_list(file_browser);

        // Set column widths for this panel context
        xset_t set;

        if (GTK_IS_TREE_VIEW(file_browser->folder_view))
        {
            // ztd::logger::info("    set widths   mode = {}", mode);
            for (const auto i : ztd::range(column_titles.size()))
            {
                GtkTreeViewColumn* col =
                    gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view),
                                             static_cast<i32>(i));
                if (!col)
                {
                    break;
                }
                const char* title = gtk_tree_view_column_get_title(col);
                for (const auto [index, value] : ztd::enumerate(column_titles))
                {
                    if (ztd::same(title, value))
                    {
                        // get column width for this panel context
                        set = xset_get_panel_mode(p, column_names.at(index), mode);
                        const i32 width = set->y ? std::stoi(set->y.value()) : 100;
                        // ztd::logger::info("        {}\t{}", width, title );
                        if (width)
                        {
                            gtk_tree_view_column_set_fixed_width(col, width);
                            // ztd::logger::info("upd set_width {} {}", column_names.at(j), width);
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
        ptk_file_browser_view_as_icons(file_browser);
    }
    else if (xset_get_b_panel(p, xset::panel::list_compact))
    {
        ptk_file_browser_view_as_compact_list(file_browser);
    }
    else
    {
        xset_set_panel(p, xset::panel::list_detailed, xset::var::b, "1");
        ptk_file_browser_view_as_list(file_browser);
    }

    // Show Hidden
    ptk_file_browser_show_hidden_files(file_browser, xset_get_b_panel(p, xset::panel::show_hidden));

    // ztd::logger::info("ptk_file_browser_update_views fb={:p} DONE", file_browser);
}

GtkWidget*
ptk_file_browser_new(i32 curpanel, GtkWidget* notebook, GtkWidget* task_view, void* main_window)
{
    ptk::file_browser::view_mode view_mode;
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(g_object_new(PTK_TYPE_FILE_BROWSER, nullptr));

    file_browser->mypanel = curpanel;
    file_browser->mynotebook = notebook;
    file_browser->main_window = main_window;
    file_browser->task_view = task_view;
    file_browser->sel_change_idle = 0;
    file_browser->inhibit_focus = file_browser->busy = false;
    file_browser->seek_name = nullptr;
    file_browser->book_set_name = nullptr;

    for (auto& toolbar_widget : file_browser->toolbar_widgets)
    {
        toolbar_widget = nullptr;
    }

    if (xset_get_b_panel(curpanel, xset::panel::list_detailed))
    {
        view_mode = ptk::file_browser::view_mode::list_view;
    }
    else if (xset_get_b_panel(curpanel, xset::panel::list_icons))
    {
        view_mode = ptk::file_browser::view_mode::icon_view;
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC);
    }
    else if (xset_get_b_panel(curpanel, xset::panel::list_compact))
    {
        view_mode = ptk::file_browser::view_mode::compact_view;
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                       GtkPolicyType::GTK_POLICY_AUTOMATIC);
    }
    else
    {
        xset_set_panel(curpanel, xset::panel::list_detailed, xset::var::b, "1");
        view_mode = ptk::file_browser::view_mode::list_view;
    }

    file_browser->view_mode = view_mode; // sfm was after next line
    // Large Icons - option for Detailed and Compact list views
    file_browser->large_icons =
        view_mode == ptk::file_browser::view_mode::icon_view ||
        xset_get_b_panel_mode(file_browser->mypanel,
                              xset::panel::list_large,
                              (MAIN_WINDOW(main_window))->panel_context.at(file_browser->mypanel));
    file_browser->folder_view = create_folder_view(file_browser, view_mode);

    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);

    file_browser->side_dir = nullptr;
    file_browser->side_dev = nullptr;

    file_browser->select_path = nullptr;
    file_browser->status_bar_custom = nullptr;

    // gtk_widget_show_all( file_browser->folder_view_scroll );

    // set status bar icon
    std::string icon_name;
    xset_t set = xset_get_panel(curpanel, xset::panel::icon_status);
    if (set->icon)
    {
        icon_name = set->icon.value();
    }
    else
    {
        icon_name = "gtk-yes";
    }
    gtk_image_set_from_icon_name(GTK_IMAGE(file_browser->status_image),
                                 icon_name.c_str(),
                                 GtkIconSize::GTK_ICON_SIZE_MENU);

    gtk_widget_show_all(GTK_WIDGET(file_browser));

    return GTK_IS_WIDGET(file_browser) ? GTK_WIDGET(file_browser) : nullptr;
}

static void
ptk_file_browser_update_tab_label(PtkFileBrowser* file_browser)
{
    GtkWidget* label;
    GtkContainer* hbox;
    // GtkImage* icon;
    GtkLabel* text;
    GList* children;

    label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(file_browser->mynotebook),
                                       GTK_WIDGET(file_browser));
    hbox = GTK_CONTAINER(gtk_bin_get_child(GTK_BIN(label)));
    children = gtk_container_get_children(hbox);
    // icon = GTK_IMAGE(children->data);
    text = GTK_LABEL(children->next->data);
    g_list_free(children);

    /* TODO: Change the icon */

    const std::string name = ptk_file_browser_get_cwd(file_browser).filename();
    gtk_label_set_text(text, name.data());
    gtk_label_set_ellipsize(text, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
    if (name.size() < 30)
    {
        gtk_label_set_ellipsize(text, PangoEllipsizeMode::PANGO_ELLIPSIZE_NONE);
        gtk_label_set_width_chars(text, -1);
    }
    else
    {
        gtk_label_set_width_chars(text, 30);
    }
}

void
ptk_file_browser_select_last(PtkFileBrowser* file_browser) // MOD added
{
    // ztd::logger::info("ptk_file_browser_select_last");
    // select one file?
    if (file_browser->select_path)
    {
        ptk_file_browser_select_file(file_browser, file_browser->select_path);
        std::free(file_browser->select_path);
        file_browser->select_path = nullptr;
        return;
    }

    // select previously selected files
    i32 elementn = -1;
    GList* l;
    GList* element = nullptr;
    // ztd::logger::info("    search for {}", (char*)file_browser->curHistory->data);

    if (file_browser->history && file_browser->histsel && file_browser->curHistory &&
        (l = g_list_last(file_browser->history)))
    {
        if (l->data && ztd::same((char*)l->data, (char*)file_browser->curHistory->data))
        {
            elementn = g_list_position(file_browser->history, l);
            if (elementn != -1)
            {
                element = g_list_nth(file_browser->histsel, elementn);
                // skip the current history item if sellist empty since it was just created
                if (!element->data)
                {
                    // ztd::logger::info("        found current empty");
                    element = nullptr;
                }
                // else ztd::logger::info("        found current NON-empty");
            }
        }
        if (!element)
        {
            while ((l = g_list_previous(l)))
            {
                if (l->data && ztd::same((char*)l->data, (char*)file_browser->curHistory->data))
                {
                    elementn = g_list_position(file_browser->history, l);
                    // ztd::logger::info("        found elementn={}", elementn);
                    if (elementn != -1)
                    {
                        element = g_list_nth(file_browser->histsel, elementn);
                    }
                    break;
                }
            }
        }
    }

    /*
        if ( element )
        {
            g_debug ("element OK" );
            if ( element->data )
                g_debug ("element->data OK" );
            else
                g_debug ("element->data nullptr" );
        }
        else
            g_debug ("element nullptr" );
        g_debug ("histsellen=%d", g_list_length( file_browser->histsel ) );
    */
    if (element && element->data)
    {
        // ztd::logger::info("    select files");
        PtkFileList* list = PTK_FILE_LIST_REINTERPRET(file_browser->file_list);
        GtkTreeSelection* tree_sel;
        bool firstsel = true;
        if (file_browser->view_mode == ptk::file_browser::view_mode::list_view)
        {
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
        }

        for (l = (GList*)element->data; l; l = g_list_next(l))
        {
            if (l->data)
            {
                // g_debug ("find a file");
                GtkTreeIter it;
                GtkTreePath* tp;
                vfs::file_info file = VFS_FILE_INFO(l->data);
                if (ptk_file_list_find_iter(list, &it, file))
                {
                    // g_debug ("found file");
                    tp = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (file_browser->view_mode == ptk::file_browser::view_mode::icon_view ||
                        file_browser->view_mode == ptk::file_browser::view_mode::compact_view)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), tp);
                        if (firstsel)
                        {
                            exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                     tp,
                                                     nullptr,
                                                     false);
                            exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                         tp,
                                                         true,
                                                         .25,
                                                         0);
                            firstsel = false;
                        }
                    }
                    else if (file_browser->view_mode == ptk::file_browser::view_mode::list_view)
                    {
                        gtk_tree_selection_select_path(tree_sel, tp);
                        if (firstsel)
                        {
                            gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                     tp,
                                                     nullptr,
                                                     false);
                            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                         tp,
                                                         nullptr,
                                                         true,
                                                         .25,
                                                         0);
                            firstsel = false;
                        }
                    }
                    gtk_tree_path_free(tp);
                }
            }
        }
    }
}

bool
ptk_file_browser_chdir(PtkFileBrowser* file_browser, const std::filesystem::path& folder_path,
                       ptk::file_browser::chdir_mode mode)
{
    GtkWidget* folder_view = file_browser->folder_view;
    // ztd::logger::info("ptk_file_browser_chdir");

    const bool inhibit_focus = file_browser->inhibit_focus;
    // file_browser->button_press = false;
    file_browser->is_drag = false;
    file_browser->menu_shown = false;
    if (file_browser->view_mode == ptk::file_browser::view_mode::list_view ||
        app_settings.get_single_click())
    {
        /* sfm 1.0.6 do not reset skip_release for Icon/Compact to prevent file
           under cursor being selected when entering dir with double-click.
           Reset is conditional here to avoid possible but unlikely unintended
           breakage elsewhere. */
        file_browser->skip_release = false;
    }

    if (!std::filesystem::exists(folder_path))
    {
        return false;
    }

    const auto path = std::filesystem::canonical(folder_path);

    if (!std::filesystem::is_directory(path))
    {
        if (!inhibit_focus)
        {
            const std::string msg = std::format("Directory does not exist\n\n{}", path.string());
            ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                           "Error",
                           msg);
        }
        return false;
    }

    if (!have_x_access(path))
    {
        if (!inhibit_focus)
        {
            const std::string errno_msg = std::strerror(errno);
            const std::string msg =
                std::format("Unable to access {}\n\n{}", path.string(), errno_msg);
            ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                           "Error",
                           msg);
        }
        return false;
    }

    // bool cancel;
    // file_browser->run_event<spacefm::signal::chdir_before>();
    // if (cancel)
    //     return false;

    // MOD remember selected files
    // ztd::logger::debug("@@@@@@@@@@@ remember: {}", ptk_file_browser_get_cwd(file_browser));
    if (file_browser->curhistsel && file_browser->curhistsel->data)
    {
        // g_debug ("free curhistsel");
        g_list_foreach((GList*)file_browser->curhistsel->data, (GFunc)vfs_file_info_unref, nullptr);
        g_list_free((GList*)file_browser->curhistsel->data);
    }
    if (file_browser->curhistsel)
    {
        file_browser->curhistsel->data =
            vector_to_glist_VFSFileInfo(ptk_file_browser_get_selected_files(file_browser));

        // ztd::logger::debug("set curhistsel {}", g_list_position(file_browser->histsel,
        // file_browser->curhistsel)); if (file_browser->curhistsel->data)
        //    ztd::logger::debug("curhistsel->data OK");
        // else
        //    ztd::logger::debug("curhistsel->data nullptr");
    }

    switch (mode)
    {
        case ptk::file_browser::chdir_mode::add_history:
            if (!file_browser->curHistory ||
                !std::filesystem::equivalent(
                    static_cast<const char*>(file_browser->curHistory->data),
                    path))
            {
                /* Has forward history */
                if (file_browser->curHistory && file_browser->curHistory->next)
                {
                    /* clear old forward history */
                    g_list_foreach(file_browser->curHistory->next, (GFunc)std::free, nullptr);
                    g_list_free(file_browser->curHistory->next);
                    file_browser->curHistory->next = nullptr;
                }
                // MOD added - make histsel shadow file_browser->history
                if (file_browser->curhistsel && file_browser->curhistsel->next)
                {
                    // ztd::logger::debug("@@@@@@@@@@@ free forward");
                    for (GList* l = file_browser->curhistsel->next; l; l = g_list_next(l))
                    {
                        if (l->data)
                        {
                            // ztd::logger::debug("free forward item");
                            g_list_foreach((GList*)l->data, (GFunc)vfs_file_info_unref, nullptr);
                            g_list_free((GList*)l->data);
                        }
                    }
                    g_list_free(file_browser->curhistsel->next);
                    file_browser->curhistsel->next = nullptr;
                }
                /* Add path to history if there is no forward history */
                file_browser->history = g_list_append(file_browser->history, ztd::strdup(path));
                file_browser->curHistory = g_list_last(file_browser->history);
                // MOD added - make histsel shadow file_browser->history
                GList* sellist = nullptr;
                file_browser->histsel = g_list_append(file_browser->histsel, sellist);
                file_browser->curhistsel = g_list_last(file_browser->histsel);
            }
            break;
        case ptk::file_browser::chdir_mode::back:
            file_browser->curHistory = file_browser->curHistory->prev;
            file_browser->curhistsel = file_browser->curhistsel->prev;
            break;
        case ptk::file_browser::chdir_mode::forward:
            file_browser->curHistory = file_browser->curHistory->next;
            file_browser->curhistsel = file_browser->curhistsel->next;
            break;
        case ptk::file_browser::chdir_mode::normal:
        case ptk::file_browser::chdir_mode::no_history:
            break;
    }

    // remove old dir object
    if (file_browser->dir)
    {
        g_signal_handlers_disconnect_matched(file_browser->dir,
                                             GSignalMatchType::G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(file_browser->dir);
    }

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_set_model(EXO_ICON_VIEW(folder_view), nullptr);
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_set_model(GTK_TREE_VIEW(folder_view), nullptr);
            break;
    }

    // load new dir
    file_browser->busy = true;
    file_browser->dir = vfs_dir_get_by_path(path);

    file_browser->run_event<spacefm::signal::chdir_begin>();

    if (vfs_dir_is_file_listed(file_browser->dir))
    {
        on_dir_file_listed(file_browser, false);
        file_browser->busy = false;
    }
    else
    {
        file_browser->busy = true;
    }

    file_browser->signal_file_listed =
        file_browser->dir->add_event<spacefm::signal::file_listed>(on_dir_file_listed,
                                                                   file_browser);

    ptk_file_browser_update_tab_label(file_browser);

    const auto disp_path = ptk_file_browser_get_cwd(file_browser);
    if (!inhibit_focus)
    {
        gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), disp_path.c_str());
    }

    enable_toolbar(file_browser);
    return true;
}

static void
on_history_menu_item_activate(GtkWidget* menu_item, PtkFileBrowser* file_browser)
{
    GList* l = (GList*)g_object_get_data(G_OBJECT(menu_item), "path");
    GList* tmp = file_browser->curHistory;
    file_browser->curHistory = l;

    if (!ptk_file_browser_chdir(file_browser,
                                (char*)l->data,
                                ptk::file_browser::chdir_mode::no_history))
    {
        file_browser->curHistory = tmp;
    }
    else
    {
        // MOD sync curhistsel
        i32 elementn = -1;
        elementn = g_list_position(file_browser->history, file_browser->curHistory);
        if (elementn != -1)
        {
            file_browser->curhistsel = g_list_nth(file_browser->histsel, elementn);
        }
        else
        {
            ztd::logger::debug("missing history item - ptk-file-browser.cxx");
        }
    }
}

static GtkWidget*
add_history_menu_item(PtkFileBrowser* file_browser, GtkWidget* menu, GList* l)
{
    GtkWidget* menu_item;
    // GtkWidget* folder_image;
    const auto disp_name = std::filesystem::path((char*)l->data).filename();
    menu_item = gtk_menu_item_new_with_label(disp_name.c_str());
    g_object_set_data(G_OBJECT(menu_item), "path", l);
    // folder_image = gtk_image_new_from_icon_name("gnome-fs-directory",
    // GtkIconSize::GTK_ICON_SIZE_MENU);
    g_signal_connect(menu_item,
                     "activate",
                     G_CALLBACK(on_history_menu_item_activate),
                     file_browser);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    return menu_item;
}

void
ptk_file_browser_show_history_menu(PtkFileBrowser* file_browser, bool is_back_history,
                                   GdkEventButton* event)
{
    (void)event;
    GtkWidget* menu = gtk_menu_new();
    bool has_items = false;

    if (is_back_history)
    {
        // back history
        for (GList* l = g_list_previous(file_browser->curHistory); l != nullptr;
             l = g_list_previous(l))
        {
            add_history_menu_item(file_browser, GTK_WIDGET(menu), l);
            if (!has_items)
            {
                has_items = true;
            }
        }
    }
    else
    {
        // forward history
        for (GList* l = g_list_next(file_browser->curHistory); l != nullptr; l = g_list_next(l))
        {
            add_history_menu_item(file_browser, GTK_WIDGET(menu), l);
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

static bool
ptk_file_browser_content_changed(PtkFileBrowser* file_browser)
{
    file_browser->run_event<spacefm::signal::change_content>();
    return false;
}

static void
on_folder_content_changed(vfs::file_info file, PtkFileBrowser* file_browser)
{
    if (file == nullptr)
    {
        // The current directory itself changed
        if (!std::filesystem::is_directory(ptk_file_browser_get_cwd(file_browser)))
        {
            // current directory does not exist - was renamed
            on_close_notebook_page(nullptr, file_browser);
        }
    }
    else
    {
        g_idle_add((GSourceFunc)ptk_file_browser_content_changed, file_browser);
    }
}

static void
on_file_deleted(vfs::file_info file, PtkFileBrowser* file_browser)
{
    if (file == nullptr)
    {
        // The directory itself was deleted
        on_close_notebook_page(nullptr, file_browser);
        // ptk_file_browser_chdir( file_browser, vfs::user_dirs->home_dir(),
        // ptk::file_browser::chdir_mode::PTK_FB_CHDIR_ADD_HISTORY);
    }
    else
    {
        on_folder_content_changed(file, file_browser);
    }
}

static void
on_sort_col_changed(GtkTreeSortable* sortable, PtkFileBrowser* file_browser)
{
    i32 col;
    gtk_tree_sortable_get_sort_column_id(sortable, &col, &file_browser->sort_type);

    ptk::file_list::column column = ptk::file_list::column(col);
    ptk::file_browser::sort_order sort_order = ptk::file_browser::sort_order::name;
    switch (column)
    {
        case ptk::file_list::column::name:
            sort_order = ptk::file_browser::sort_order::name;
            break;
        case ptk::file_list::column::size:
            sort_order = ptk::file_browser::sort_order::size;
            break;
        case ptk::file_list::column::mtime:
            sort_order = ptk::file_browser::sort_order::mtime;
            break;
        case ptk::file_list::column::desc:
            sort_order = ptk::file_browser::sort_order::type;
            break;
        case ptk::file_list::column::perm:
            sort_order = ptk::file_browser::sort_order::perm;
            break;
        case ptk::file_list::column::owner:
            sort_order = ptk::file_browser::sort_order::owner;
            break;
        case ptk::file_list::column::big_icon:
            [[fallthrough]];
        case ptk::file_list::column::small_icon:
            [[fallthrough]];
        case ptk::file_list::column::info:
            break;
    }
    file_browser->sort_order = sort_order;
    // MOD enable following to make column click permanent sort
    //    app_settings.set_sort_order(col);
    //    if (file_browser)
    //        ptk_file_browser_set_sort_order(file_browser),
    //        app_settings.get_sort_order());

    xset_set_panel(file_browser->mypanel,
                   xset::panel::list_detailed,
                   xset::var::x,
                   std::to_string(col));
    xset_set_panel(file_browser->mypanel,
                   xset::panel::list_detailed,
                   xset::var::y,
                   std::to_string(file_browser->sort_type));
}

static void
ptk_file_browser_update_model(PtkFileBrowser* file_browser)
{
    PtkFileList* list = ptk_file_list_new(file_browser->dir, file_browser->show_hidden_files);
    GtkTreeModel* old_list = file_browser->file_list;
    file_browser->file_list = GTK_TREE_MODEL(list);
    if (old_list)
    {
        g_object_unref(G_OBJECT(old_list));
    }

    ptk_file_browser_read_sort_extra(file_browser); // sfm
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list),
                                         file_list_order_from_sort_order(file_browser->sort_order),
                                         file_browser->sort_type);

    show_thumbnails(file_browser, list, file_browser->large_icons, file_browser->max_thumbnail);
    g_signal_connect(list, "sort-column-changed", G_CALLBACK(on_sort_col_changed), file_browser);

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_set_model(EXO_ICON_VIEW(file_browser->folder_view), GTK_TREE_MODEL(list));
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_set_model(GTK_TREE_VIEW(file_browser->folder_view), GTK_TREE_MODEL(list));
            break;
    }

    // try to smooth list bounce created by delayed re-appearance of column headers
    // while (g_main_context_pending(nullptr))
    //    g_main_context_iteration(nullptr, true);
}

static void
on_dir_file_listed(PtkFileBrowser* file_browser, bool is_cancelled)
{
    vfs::dir dir = file_browser->dir;

    file_browser->n_sel_files = 0;

    if (!is_cancelled)
    {
        file_browser->signal_file_created =
            dir->add_event<spacefm::signal::file_created>(on_folder_content_changed, file_browser);
        file_browser->signal_file_deleted =
            dir->add_event<spacefm::signal::file_deleted>(on_file_deleted, file_browser);
        file_browser->signal_file_changed =
            dir->add_event<spacefm::signal::file_changed>(on_folder_content_changed, file_browser);
    }

    ptk_file_browser_update_model(file_browser);
    file_browser->busy = false;

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility that changing the directory results in
     * thousands of large thumbnails being freed, but the memory not actually
     * released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif

    file_browser->run_event<spacefm::signal::chdir_after>();
    file_browser->run_event<spacefm::signal::change_content>();
    file_browser->run_event<spacefm::signal::change_sel>();

    if (file_browser->side_dir)
    {
        ptk_dir_tree_view_chdir(GTK_TREE_VIEW(file_browser->side_dir),
                                ptk_file_browser_get_cwd(file_browser));
    }

    if (file_browser->side_dev)
    {
        ptk_location_view_chdir(GTK_TREE_VIEW(file_browser->side_dev),
                                ptk_file_browser_get_cwd(file_browser));
    }

    // FIXME:  This is already done in update_model, but is there any better way to
    //            reduce unnecessary code?
    if (file_browser->view_mode == ptk::file_browser::view_mode::compact_view)
    { // sfm why is this needed for compact view???
        if (!is_cancelled && file_browser->file_list)
        {
            show_thumbnails(file_browser,
                            PTK_FILE_LIST_REINTERPRET(file_browser->file_list),
                            file_browser->large_icons,
                            file_browser->max_thumbnail);
        }
    }
}

void
ptk_file_browser_canon(PtkFileBrowser* file_browser, const std::filesystem::path& path)
{
    const auto cwd = ptk_file_browser_get_cwd(file_browser);
    const auto canon = std::filesystem::canonical(path);
    if (std::filesystem::equivalent(canon, cwd) || std::filesystem::equivalent(canon, path))
    {
        return;
    }

    if (std::filesystem::is_directory(canon))
    {
        // open dir
        ptk_file_browser_chdir(file_browser, canon, ptk::file_browser::chdir_mode::add_history);
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
    }
    else if (std::filesystem::exists(canon))
    {
        // open dir and select file
        const auto dir_path = canon.parent_path();
        if (!std::filesystem::equivalent(dir_path, cwd))
        {
            std::free(file_browser->select_path);
            file_browser->select_path = ztd::strdup(canon);
            ptk_file_browser_chdir(file_browser,
                                   dir_path,
                                   ptk::file_browser::chdir_mode::add_history);
        }
        else
        {
            ptk_file_browser_select_file(file_browser, canon);
        }
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
    }
}

const std::filesystem::path
ptk_file_browser_get_cwd(PtkFileBrowser* file_browser)
{
    if (!file_browser->curHistory)
    {
        return vfs::user_dirs->home_dir();
    }
    return (const char*)file_browser->curHistory->data;
}

void
ptk_file_browser_go_back(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    /* there is no back history */
    if (!file_browser->curHistory || !file_browser->curHistory->prev)
    {
        return;
    }
    const char* path = (const char*)file_browser->curHistory->prev->data;
    ptk_file_browser_chdir(file_browser, path, ptk::file_browser::chdir_mode::back);
}

void
ptk_file_browser_go_forward(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    /* If there is no forward history */
    if (!file_browser->curHistory || !file_browser->curHistory->next)
    {
        return;
    }
    const char* path = (const char*)file_browser->curHistory->next->data;
    ptk_file_browser_chdir(file_browser, path, ptk::file_browser::chdir_mode::forward);
}

void
ptk_file_browser_go_up(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    const auto parent_dir = ptk_file_browser_get_cwd(file_browser).parent_path();
    if (!std::filesystem::equivalent(parent_dir, ptk_file_browser_get_cwd(file_browser)))
    {
        ptk_file_browser_chdir(file_browser,
                               parent_dir,
                               ptk::file_browser::chdir_mode::add_history);
    }
}

void
ptk_file_browser_go_home(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    ptk_file_browser_chdir(file_browser,
                           vfs::user_dirs->home_dir(),
                           ptk::file_browser::chdir_mode::add_history);
}

void
ptk_file_browser_go_default(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    const auto default_path = xset_get_s(xset::name::go_set_default);
    if (default_path)
    {
        ptk_file_browser_chdir(file_browser,
                               default_path.value(),
                               ptk::file_browser::chdir_mode::add_history);
    }
    else
    {
        ptk_file_browser_chdir(file_browser,
                               vfs::user_dirs->home_dir(),
                               ptk::file_browser::chdir_mode::add_history);
    }
}

void
ptk_file_browser_set_default_folder(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    xset_set(xset::name::go_set_default,
             xset::var::s,
             ptk_file_browser_get_cwd(file_browser).string());
}

void
ptk_file_browser_select_all(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_select_all(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case ptk::file_browser::view_mode::list_view:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            gtk_tree_selection_select_all(tree_sel);
            break;
    }
}

void
ptk_file_browser_unselect_all(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_unselect_all(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case ptk::file_browser::view_mode::list_view:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            gtk_tree_selection_unselect_all(tree_sel);
            break;
    }
}

static bool
invert_selection(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* it,
                 PtkFileBrowser* file_browser)
{
    (void)model;
    (void)it;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view), path))
            {
                exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view), path);
            }
            else
            {
                exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);
            }
            break;
        case ptk::file_browser::view_mode::list_view:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            if (gtk_tree_selection_path_is_selected(tree_sel, path))
            {
                gtk_tree_selection_unselect_path(tree_sel, path);
            }
            else
            {
                gtk_tree_selection_select_path(tree_sel, path);
            }
            break;
    }
    return false;
}

void
ptk_file_browser_invert_selection(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)invert_selection, file_browser);
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case ptk::file_browser::view_mode::list_view:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)invert_selection, file_browser);
            g_signal_handlers_unblock_matched(tree_sel,
                                              GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(tree_sel), file_browser);
            break;
    }
}

void
ptk_file_browser_select_pattern(GtkWidget* item, PtkFileBrowser* file_browser,
                                const char* search_key)
{
    (void)item;
    GtkTreeModel* model;
    GtkTreePath* path;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    vfs::file_info file;
    const char* key;

    if (search_key)
    {
        key = search_key;
    }
    else
    {
        // get pattern from user  (store in ob1 so it is not saved)
        xset_t set = xset_get(xset::name::select_patt);
        const auto [response, answer] = xset_text_dialog(
            GTK_WIDGET(file_browser),
            "Select By Pattern",
            "Enter pattern to select files and directories:\n\nIf your pattern contains any "
            "uppercase characters, the matching will be case sensitive.\n\nExample:  "
            "*sp*e?m*\n\nTIP: You can also enter '%% PATTERN' in the path bar.",
            "",
            set->ob1,
            "",
            false);

        set->ob1 = ztd::strdup(answer);
        if (!response || !set->ob1)
        {
            return;
        }
        key = set->ob1;
    }

    // case insensitive search ?
    bool icase = false;
    char* lower_key = g_utf8_strdown(key, -1);
    if (ztd::same(lower_key, key))
    {
        // key is all lowercase so do icase search
        icase = true;
    }
    std::free(lower_key);

    // get model, treesel, and stop signals
    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            break;
        case ptk::file_browser::view_mode::list_view:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
    }

    // test rows
    bool first_select = true;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            // get file
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (!file)
            {
                continue;
            }

            // test name
            std::string name = file->get_disp_name();
            if (icase)
            {
                name = ztd::lower(name);
            }

            const bool select = ztd::fnmatch(key, name);

            // do selection and scroll to first selected
            path = gtk_tree_model_get_path(
                GTK_TREE_MODEL(PTK_FILE_LIST_REINTERPRET(file_browser->file_list)),
                &it);

            switch (file_browser->view_mode)
            {
                case ptk::file_browser::view_mode::icon_view:
                case ptk::file_browser::view_mode::compact_view:
                    // select
                    if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view),
                                                       path))
                    {
                        if (!select)
                        {
                            exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                        path);
                        }
                    }
                    else if (select)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);
                    }

                    // scroll to first and set cursor
                    if (first_select && select)
                    {
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                     path,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                case ptk::file_browser::view_mode::list_view:
                    // select
                    if (gtk_tree_selection_path_is_selected(tree_sel, path))
                    {
                        if (!select)
                        {
                            gtk_tree_selection_unselect_path(tree_sel, path);
                        }
                    }
                    else if (select)
                    {
                        gtk_tree_selection_select_path(tree_sel, path);
                    }

                    // scroll to first and set cursor
                    if (first_select && select)
                    {
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
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

    // restore signals and trigger sel change
    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case ptk::file_browser::view_mode::list_view:
            g_signal_handlers_unblock_matched(tree_sel,
                                              GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(tree_sel), file_browser);
            break;
    }
    focus_folder_view(file_browser);
}

void
ptk_file_browser_select_file_list(PtkFileBrowser* file_browser, char** filename, bool do_select)
{
    // If do_select, select all filenames, unselect others
    // if !do_select, unselect filenames, leave others unchanged
    // If !*filename select or unselect all
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;

    if (!filename || !*filename)
    {
        if (do_select)
        {
            ptk_file_browser_select_all(nullptr, file_browser);
        }
        else
        {
            ptk_file_browser_unselect_all(nullptr, file_browser);
        }
        return;
    }

    // get model, treesel, and stop signals
    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            break;
        case ptk::file_browser::view_mode::list_view:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
    }

    // test rows
    bool first_select = true;
    GtkTreeIter it;
    vfs::file_info file;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        bool select;
        do
        {
            // get file
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (!file)
            {
                continue;
            }

            // test name
            const std::string name = file->get_disp_name();
            char** test_name = filename;
            while (*test_name)
            {
                if (ztd::same(*test_name, name))
                {
                    break;
                }
                test_name++;
            }
            if (*test_name)
            {
                select = do_select;
            }
            else
            {
                select = !do_select;
            }

            // do selection and scroll to first selected
            GtkTreePath* path = gtk_tree_model_get_path(
                GTK_TREE_MODEL(PTK_FILE_LIST_REINTERPRET(file_browser->file_list)),
                &it);

            switch (file_browser->view_mode)
            {
                case ptk::file_browser::view_mode::icon_view:
                case ptk::file_browser::view_mode::compact_view:
                    // select
                    if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view),
                                                       path))
                    {
                        if (!select)
                        {
                            exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                        path);
                        }
                    }
                    else if (select && do_select)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);
                    }

                    // scroll to first and set cursor
                    if (first_select && select && do_select)
                    {
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                     path,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                case ptk::file_browser::view_mode::list_view:
                    // select
                    if (gtk_tree_selection_path_is_selected(tree_sel, path))
                    {
                        if (!select)
                        {
                            gtk_tree_selection_unselect_path(tree_sel, path);
                        }
                    }
                    else if (select && do_select)
                    {
                        gtk_tree_selection_select_path(tree_sel, path);
                    }

                    // scroll to first and set cursor
                    if (first_select && select && do_select)
                    {
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
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

    // restore signals and trigger sel change
    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case ptk::file_browser::view_mode::list_view:
            g_signal_handlers_unblock_matched(tree_sel,
                                              GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(tree_sel), file_browser);
            break;
    }
    focus_folder_view(file_browser);
}

static void
ptk_file_browser_restore_sig(PtkFileBrowser* file_browser, GtkTreeSelection* tree_sel)
{
    // restore signals and trigger sel change
    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case ptk::file_browser::view_mode::list_view:
            g_signal_handlers_unblock_matched(tree_sel,
                                              GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(tree_sel), file_browser);
            break;
    }
}

void
ptk_file_browser_seek_path(PtkFileBrowser* file_browser, const std::filesystem::path& seek_dir,
                           const std::filesystem::path& seek_name)
{
    // change to dir seek_dir if needed; select first dir or else file with
    // prefix seek_name
    const auto cwd = ptk_file_browser_get_cwd(file_browser);

    if (!std::filesystem::equivalent(cwd, seek_dir))
    {
        // change dir
        std::free(file_browser->seek_name);
        file_browser->seek_name = ztd::strdup(seek_name.string());
        file_browser->inhibit_focus = true;
        if (!ptk_file_browser_chdir(file_browser,
                                    seek_dir,
                                    ptk::file_browser::chdir_mode::add_history))
        {
            file_browser->inhibit_focus = false;
            std::free(file_browser->seek_name);
            file_browser->seek_name = nullptr;
        }
        // return here to allow dir to load
        // finishes seek in main-window.c on_file_browser_after_chdir()
        return;
    }

    // no change dir was needed or was called from on_file_browser_after_chdir()
    // select seek name
    ptk_file_browser_unselect_all(nullptr, file_browser);

    if (seek_name.empty())
    {
        return;
    }

    // get model, treesel, and stop signals
    GtkTreeModel* model;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            break;
        case ptk::file_browser::view_mode::list_view:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
    }

    if (!GTK_IS_TREE_MODEL(model))
    {
        ptk_file_browser_restore_sig(file_browser, tree_sel);
        return;
    }

    // test rows - give preference to matching dir, else match file
    GtkTreeIter it_file;
    GtkTreeIter it_dir;
    it_file.stamp = 0;
    it_dir.stamp = 0;
    vfs::file_info file;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            // get file
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (!file)
            {
                continue;
            }

            // test name
            const std::string name = file->get_disp_name();
            if (std::filesystem::equivalent(name, seek_name))
            {
                // exact match (may be file or dir)
                it_dir = it;
                break;
            }
            if (ztd::startswith(name, seek_name.string()))
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
        ptk_file_browser_restore_sig(file_browser, tree_sel);
        return;
    }

    // do selection and scroll to selected
    GtkTreePath* path;
    path =
        gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST_REINTERPRET(file_browser->file_list)),
                                &it);
    if (!path)
    {
        ptk_file_browser_restore_sig(file_browser, tree_sel);
        return;
    }

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            // select
            exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);

            // scroll and set cursor
            exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                     path,
                                     nullptr,
                                     false);
            exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                         path,
                                         true,
                                         .25,
                                         0);
            break;
        case ptk::file_browser::view_mode::list_view:
            // select
            gtk_tree_selection_select_path(tree_sel, path);

            // scroll and set cursor
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                     path,
                                     nullptr,
                                     false);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                         path,
                                         nullptr,
                                         true,
                                         .25,
                                         0);
            break;
    }
    gtk_tree_path_free(path);

    ptk_file_browser_restore_sig(file_browser, tree_sel);
}

/* signal handlers */

static void
on_folder_view_item_activated(ExoIconView* iconview, GtkTreePath* path,
                              PtkFileBrowser* file_browser)
{
    (void)iconview;
    (void)path;
    ptk_file_browser_open_selected_files(file_browser);
}

static void
on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* col,
                             PtkFileBrowser* file_browser)
{
    (void)tree_view;
    (void)path;
    (void)col;
    // file_browser->button_press = false;
    ptk_file_browser_open_selected_files(file_browser);
}

static bool
on_folder_view_item_sel_change_idle(PtkFileBrowser* file_browser)
{
    if (!GTK_IS_WIDGET(file_browser))
    {
        return false;
    }

    file_browser->n_sel_files = 0;
    file_browser->sel_size = 0;
    file_browser->sel_disk_size = 0;

    GtkTreeModel* model;
    GList* sel_files = folder_view_get_selected_items(file_browser, &model);

    for (GList* sel = sel_files; sel; sel = g_list_next(sel))
    {
        vfs::file_info file;
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, (GtkTreePath*)sel->data))
        {
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (file)
            {
                file_browser->sel_size += file->get_size();
                file_browser->sel_disk_size += file->get_disk_size();
                vfs_file_info_unref(file);
            }
            ++file_browser->n_sel_files;
        }
    }

    g_list_foreach(sel_files, (GFunc)gtk_tree_path_free, nullptr);
    g_list_free(sel_files);

    file_browser->run_event<spacefm::signal::change_sel>();
    file_browser->sel_change_idle = 0;
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
    if (file_browser->sel_change_idle)
    {
        return;
    }

    file_browser->sel_change_idle =
        g_idle_add((GSourceFunc)on_folder_view_item_sel_change_idle, file_browser);
}

static void
show_popup_menu(PtkFileBrowser* file_browser, GdkEventButton* event)
{
    (void)event;
    std::filesystem::path file_path;
    vfs::file_info file;

    const auto cwd = ptk_file_browser_get_cwd(file_browser);
    const std::vector<vfs::file_info> sel_files = ptk_file_browser_get_selected_files(file_browser);
    if (sel_files.empty())
    {
        file = nullptr;
    }
    else
    {
        file = vfs_file_info_ref(sel_files.front());
        file_path = cwd / file->get_name();
    }

    /*
    i32 button;
    std::time_t time;
    if (event)
    {
        button = event->button;
        time = event->time;
    }
    else
    {
        button = 0;
        time = gtk_get_current_event_time();
    }
    */

    char* dir_name = nullptr;
    GtkWidget* popup = ptk_file_menu_new(file_browser,
                                         file_path.c_str(),
                                         file,
                                         dir_name ? dir_name : cwd.c_str(),
                                         sel_files);
    if (popup)
    {
        gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
    }
    if (file)
    {
        vfs_file_info_unref(file);
    }

    if (dir_name)
    {
        std::free(dir_name);
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
on_folder_view_button_press_event(GtkWidget* widget, GdkEventButton* event,
                                  PtkFileBrowser* file_browser)
{
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;
    GtkTreeViewColumn* col = nullptr;
    GtkTreeSelection* tree_sel;
    bool ret = false;

    if (file_browser->menu_shown)
    {
        file_browser->menu_shown = false;
    }

    if (event->type == GdkEventType::GDK_BUTTON_PRESS)
    {
        focus_folder_view(file_browser);
        // file_browser->button_press = true;

        if ((event_handler->win_click->s || event_handler->win_click->ob2_data) &&
            main_window_event(file_browser->main_window,
                              event_handler->win_click,
                              xset::name::evt_win_click,
                              0,
                              0,
                              "filelist",
                              0,
                              event->button,
                              event->state,
                              true))
        {
            file_browser->skip_release = true;
            return true;
        }

        if (event->button == 4 || event->button == 5 || event->button == 8 ||
            event->button == 9) // sfm
        {
            if (event->button == 4 || event->button == 8)
            {
                ptk_file_browser_go_back(nullptr, file_browser);
            }
            else
            {
                ptk_file_browser_go_forward(nullptr, file_browser);
            }
            return true;
        }

        // Alt - Left/Right Click
        if (((event->state & (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK |
                              GdkModifierType::GDK_MOD1_MASK)) == GdkModifierType::GDK_MOD1_MASK) &&
            (event->button == 1 || event->button == 3)) // sfm
        {
            if (event->button == 1)
            {
                ptk_file_browser_go_back(nullptr, file_browser);
            }
            else
            {
                ptk_file_browser_go_forward(nullptr, file_browser);
            }
            return true;
        }

        switch (file_browser->view_mode)
        {
            case ptk::file_browser::view_mode::icon_view:
            case ptk::file_browser::view_mode::compact_view:
                tree_path =
                    exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), event->x, event->y);
                model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));

                /* deselect selected files when right click on blank area */
                if (!tree_path && event->button == 3)
                {
                    exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                }
                break;
            case ptk::file_browser::view_mode::list_view:
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              static_cast<i32>(event->x),
                                              static_cast<i32>(event->y),
                                              &tree_path,
                                              &col,
                                              nullptr,
                                              nullptr);
                tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

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
        vfs::file_info file;
        GtkTreeIter it;
        std::filesystem::path file_path;
        if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
        {
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            file_path = ptk_file_browser_get_cwd(file_browser) / file->get_name();
        }
        else /* no item is clicked */
        {
            file = nullptr;
        }

        /* middle button */
        if (event->button == 2 && !file_path.empty()) /* middle click on a item */
        {
            /* open in new tab if its a directory */
            if (std::filesystem::is_directory(file_path))
            {
                file_browser->run_event<spacefm::signal::open_item>(file_path,
                                                                    ptk::open_action::new_tab);
            }
            ret = true;
        }
        else if (event->button == 3) /* right click */
        {
            /* cancel all selection, and select the item if it is not selected */
            switch (file_browser->view_mode)
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
                    if (tree_path && !gtk_tree_selection_path_is_selected(tree_sel, tree_path))
                    {
                        gtk_tree_selection_unselect_all(tree_sel);
                        gtk_tree_selection_select_path(tree_sel, tree_path);
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
            ret = file_browser->menu_shown = true;
        }
        if (file)
        {
            vfs_file_info_unref(file);
        }
        gtk_tree_path_free(tree_path);
    }
    else if (event->type == GdkEventType::GDK_2BUTTON_PRESS && event->button == 1)
    {
        // f64 click event -  button = 0
        if ((event_handler->win_click->s || event_handler->win_click->ob2_data) &&
            main_window_event(file_browser->main_window,
                              event_handler->win_click,
                              xset::name::evt_win_click,
                              0,
                              0,
                              "filelist",
                              0,
                              0,
                              event->state,
                              true))
        {
            return true;
        }

        if (file_browser->view_mode == ptk::file_browser::view_mode::list_view)
        {
            /* set ret true to prevent drag_begin starting in this tab after
             * fuseiso mount.  Why?
             * row_activated occurs before GdkEventType::GDK_2BUTTON_PRESS so use
             * file_browser->button_press to determine if row was already
             * activated or user clicked on non-row */
            ret = true;
        }
        else if (!app_settings.get_single_click())
        {
            /* sfm 1.0.6 set skip_release for Icon/Compact to prevent file
             * under cursor being selected when entering dir with double-click.
             * Also see conditional reset of skip_release in
             * ptk_file_browser_chdir(). See also
             * on_folder_view_button_release_event() */
            file_browser->skip_release = true;
        }
    }
    return ret;
}

static bool
on_folder_view_button_release_event(GtkWidget* widget, GdkEventButton* event,
                                    PtkFileBrowser* file_browser) // sfm
{ // on left-click release on file, if not dnd or rubberbanding, unselect files
    (void)widget;
    GtkTreePath* tree_path = nullptr;

    if (file_browser->is_drag || event->button != 1 || file_browser->skip_release ||
        (event->state & (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK |
                         GdkModifierType::GDK_MOD1_MASK)))
    {
        if (file_browser->skip_release)
        {
            file_browser->skip_release = false;
        }
        // this fixes bug where right-click shows menu and release unselects files
        const bool ret = file_browser->menu_shown && event->button != 1;
        if (file_browser->menu_shown)
        {
            file_browser->menu_shown = false;
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
             * reset skip_release in ptk_file_browser_chdir(). */
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
    char* dir_path = ptk_dir_tree_view_get_selected_dir(GTK_TREE_VIEW(file_browser->side_dir));

    if (dir_path)
    {
        if (!std::filesystem::equivalent(dir_path, ptk_file_browser_get_cwd(file_browser)))
        {
            if (ptk_file_browser_chdir(file_browser,
                                       dir_path,
                                       ptk::file_browser::chdir_mode::add_history))
            {
                gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), dir_path);
            }
        }
        std::free(dir_path);
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

void
ptk_file_browser_new_tab(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;

    focus_folder_view(file_browser);

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
        file_browser->run_event<spacefm::signal::open_item>("/", ptk::open_action::new_tab);
    }
    else
    {
        file_browser->run_event<spacefm::signal::open_item>(dir_path, ptk::open_action::new_tab);
    }
}

void
ptk_file_browser_new_tab_here(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);

    auto dir_path = ptk_file_browser_get_cwd(file_browser);
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
        file_browser->run_event<spacefm::signal::open_item>("/", ptk::open_action::new_tab);
    }
    else
    {
        file_browser->run_event<spacefm::signal::open_item>(dir_path, ptk::open_action::new_tab);
    }
}

void
ptk_file_browser_save_column_widths(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    if (!(GTK_IS_WIDGET(file_browser) && GTK_IS_TREE_VIEW(view)))
    {
        return;
    }

    if (file_browser->view_mode != ptk::file_browser::view_mode::list_view)
    {
        return;
    }

    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);

    // if the window was opened maximized and stayed maximized, or the window is
    // unmaximized and not fullscreen, save the columns
    if ((!main_window->maximized || main_window->opened_maximized) && !main_window->fullscreen)
    {
        const panel_t p = file_browser->mypanel;
        const xset::main_window_panel mode = main_window->panel_context.at(p);
        // ztd::logger::info("save_columns  fb={:p} (panel {})  mode = {}", fmt::ptr(file_browser),
        // p, mode);
        for (const auto i : ztd::range(column_titles.size()))
        {
            GtkTreeViewColumn* col = gtk_tree_view_get_column(view, static_cast<i32>(i));
            if (!col)
            {
                return;
            }
            const char* title = gtk_tree_view_column_get_title(col);
            for (const auto [index, value] : ztd::enumerate(column_titles))
            {
                if (ztd::same(title, value))
                {
                    // save column width for this panel context
                    xset_t set = xset_get_panel_mode(p, column_names.at(index), mode);
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

static void
on_folder_view_columns_changed(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    // user dragged a column to a different position - save positions
    if (!(GTK_IS_WIDGET(file_browser) && GTK_IS_TREE_VIEW(view)))
    {
        return;
    }

    if (file_browser->view_mode != ptk::file_browser::view_mode::list_view)
    {
        return;
    }

    for (const auto i : ztd::range(column_titles.size()))
    {
        GtkTreeViewColumn* col = gtk_tree_view_get_column(view, static_cast<i32>(i));
        if (!col)
        {
            return;
        }
        const char* title = gtk_tree_view_column_get_title(col);
        for (const auto [index, value] : ztd::enumerate(column_titles))
        {
            if (ztd::same(title, value))
            {
                // save column position
                xset_t set = xset_get_panel(file_browser->mypanel, column_names.at(index));
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
folder_view_search_equal(GtkTreeModel* model, i32 col, const char* key, GtkTreeIter* it,
                         void* search_data)
{
    (void)search_data;
    char* name;
    char* lower_name = nullptr;
    bool no_match;

    const ptk::file_list::column column = ptk::file_list::column(col);
    if (column != ptk::file_list::column::name)
    {
        return true;
    }

    gtk_tree_model_get(model, it, col, &name, -1);

    if (!name || !key)
    {
        return true;
    }

    char* lower_key = g_utf8_strdown(key, -1);
    if (ztd::same(lower_key, key))
    {
        // key is all lowercase so do icase search
        lower_name = g_utf8_strdown(name, -1);
        name = lower_name;
    }

    if (ztd::contains(key, "*") || ztd::contains(key, "?"))
    {
        const std::string key2 = std::format("*{}*", key);
        no_match = !ztd::fnmatch(key2, name);
    }
    else
    {
        const bool end = ztd::endswith(key, "$");
        bool start = !end && (std::strlen(key) < 3);
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
            no_match = !ztd::contains(name, keyp);
        }
        else if (start)
        {
            no_match = !ztd::startswith(name, keyp);
        }
        else if (end)
        {
            no_match = !ztd::endswith(name, keyp);
        }
        else
        {
            no_match = !ztd::contains(name, key);
        }
        std::free(key2);
    }
    std::free(lower_name);
    std::free(lower_key);
    return no_match; // return false for match
}

static GtkWidget*
create_folder_view(PtkFileBrowser* file_browser, ptk::file_browser::view_mode view_mode)
{
    GtkWidget* folder_view = nullptr;
    GtkTreeSelection* tree_sel;
    GtkCellRenderer* renderer;

    i32 icon_size = 0;
    const i32 big_icon_size = vfs_mime_type_get_icon_size_big();
    const i32 small_icon_size = vfs_mime_type_get_icon_size_small();

    PangoAttrList* attr_list;
    attr_list = pango_attr_list_new();
#if PANGO_VERSION_CHECK(1, 44, 0)
    pango_attr_list_insert(attr_list, pango_attr_insert_hyphens_new(false));
#endif

    switch (view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            folder_view = exo_icon_view_new();

            if (view_mode == ptk::file_browser::view_mode::compact_view)
            {
                icon_size = file_browser->large_icons ? big_icon_size : small_icon_size;

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

            exo_icon_view_set_single_click(EXO_ICON_VIEW(folder_view), file_browser->single_click);
            exo_icon_view_set_single_click_timeout(
                EXO_ICON_VIEW(folder_view),
                app_settings.get_single_hover() ? SINGLE_CLICK_TIMEOUT : 0);

            gtk_cell_layout_clear(GTK_CELL_LAYOUT(folder_view));

            file_browser->icon_render = renderer = gtk_cell_renderer_pixbuf_new();

            /* add the icon renderer */
            g_object_set(G_OBJECT(renderer), "follow_state", true, nullptr);
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(folder_view), renderer, false);
            gtk_cell_layout_add_attribute(
                GTK_CELL_LAYOUT(folder_view),
                renderer,
                "pixbuf",
                file_browser->large_icons
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

            g_signal_connect((void*)folder_view,
                             "item-activated",
                             G_CALLBACK(on_folder_view_item_activated),
                             file_browser);

            g_signal_connect_after((void*)folder_view,
                                   "selection-changed",
                                   G_CALLBACK(on_folder_view_item_sel_change),
                                   file_browser);

            break;
        case ptk::file_browser::view_mode::list_view:
            folder_view = gtk_tree_view_new();

            init_list_view(file_browser, GTK_TREE_VIEW(folder_view));

            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(folder_view));
            gtk_tree_selection_set_mode(tree_sel, GtkSelectionMode::GTK_SELECTION_MULTIPLE);

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

            // gtk_tree_view_set_single_click(GTK_TREE_VIEW(folder_view),
            // file_browser->single_click); gtk_tree_view_set_single_click_timeout(
            //    GTK_TREE_VIEW(folder_view),
            //    app_settings.get_single_hover() ? SINGLE_CLICK_TIMEOUT : 0);

            icon_size = file_browser->large_icons ? big_icon_size : small_icon_size;

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

            g_signal_connect((void*)folder_view,
                             "row_activated",
                             G_CALLBACK(on_folder_view_row_activated),
                             file_browser);

            g_signal_connect_after((void*)tree_sel,
                                   "changed",
                                   G_CALLBACK(on_folder_view_item_sel_change),
                                   file_browser);
            // MOD
            g_signal_connect((void*)folder_view,
                             "columns-changed",
                             G_CALLBACK(on_folder_view_columns_changed),
                             file_browser);
            g_signal_connect((void*)folder_view,
                             "destroy",
                             G_CALLBACK(on_folder_view_destroy),
                             file_browser);
            break;
    }

    gtk_cell_renderer_set_fixed_size(file_browser->icon_render, icon_size, icon_size);

    g_signal_connect((void*)folder_view,
                     "button-press-event",
                     G_CALLBACK(on_folder_view_button_press_event),
                     file_browser);
    g_signal_connect((void*)folder_view,
                     "button-release-event",
                     G_CALLBACK(on_folder_view_button_release_event),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "popup-menu",
                     G_CALLBACK(on_folder_view_popup_menu),
                     file_browser);

    /* init drag & drop support */

    g_signal_connect((void*)folder_view,
                     "drag-data-received",
                     G_CALLBACK(on_folder_view_drag_data_received),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-data-get",
                     G_CALLBACK(on_folder_view_drag_data_get),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-begin",
                     G_CALLBACK(on_folder_view_drag_begin),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-motion",
                     G_CALLBACK(on_folder_view_drag_motion),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-leave",
                     G_CALLBACK(on_folder_view_drag_leave),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-drop",
                     G_CALLBACK(on_folder_view_drag_drop),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-end",
                     G_CALLBACK(on_folder_view_drag_end),
                     file_browser);

    return folder_view;
}

static void
init_list_view(PtkFileBrowser* file_browser, GtkTreeView* list_view)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* pix_renderer;

    static constexpr std::array<ptk::file_list::column, 6> cols{
        ptk::file_list::column::name,
        ptk::file_list::column::size,
        ptk::file_list::column::desc,
        ptk::file_list::column::perm,
        ptk::file_list::column::owner,
        ptk::file_list::column::mtime,
    };

    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    const panel_t p = file_browser->mypanel;
    const xset::main_window_panel mode = main_window->panel_context.at(p);

    for (const auto [index, value] : ztd::enumerate(cols))
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable(col, true);

        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();

        // column order
        usize idx;
        for (const auto [order_index, order_value] : ztd::enumerate(cols))
        {
            idx = order_index;
            if (xset_get_int_panel(p, column_names.at(order_index), xset::var::x) ==
                static_cast<i32>(index))
            {
                break;
            }
        }

        // column width
        gtk_tree_view_column_set_min_width(col, 50);
        gtk_tree_view_column_set_sizing(col, GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
        xset_t set = xset_get_panel_mode(p, column_names.at(idx), mode);
        const i32 width = set->y ? std::stoi(set->y.value()) : 100;
        if (width)
        {
            if (cols.at(idx) == ptk::file_list::column::name &&
                !app_settings.get_always_show_tabs() &&
                file_browser->view_mode == ptk::file_browser::view_mode::list_view &&
                gtk_notebook_get_n_pages(GTK_NOTEBOOK(file_browser->mynotebook)) == 1)
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
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(file_browser->mynotebook), 0));

                if (first_fb && first_fb->view_mode == ptk::file_browser::view_mode::list_view &&
                    GTK_IS_TREE_VIEW(first_fb->folder_view))
                {
                    GtkTreeViewColumn* first_col =
                        gtk_tree_view_get_column(GTK_TREE_VIEW(first_fb->folder_view), 0);
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
                // ztd::logger::info("init set_width {} {}", column_names.at(j), width);
            }
        }

        if (cols.at(idx) == ptk::file_list::column::name)
        {
            g_object_set(G_OBJECT(renderer),
                         /* "editable", true, */
                         "ellipsize",
                         PangoEllipsizeMode::PANGO_ELLIPSIZE_END,
                         nullptr);
            /*
            g_signal_connect( renderer, "editing-started",
                              G_CALLBACK( on_filename_editing_started ), nullptr );
            */
            file_browser->icon_render = pix_renderer = gtk_cell_renderer_pixbuf_new();

            gtk_tree_view_column_pack_start(col, pix_renderer, false);
            gtk_tree_view_column_set_attributes(col,
                                                pix_renderer,
                                                "pixbuf",
                                                file_browser->large_icons
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
            gtk_tree_view_column_set_visible(col,
                                             xset_get_b_panel_mode(p, column_names.at(idx), mode));
        }

        if (cols.at(idx) == ptk::file_list::column::size)
        {
            gtk_cell_renderer_set_alignment(renderer, 1, 0.5);
        }

        gtk_tree_view_column_pack_start(col, renderer, true);
        gtk_tree_view_column_set_attributes(col, renderer, "text", cols.at(idx), nullptr);
        gtk_tree_view_append_column(list_view, col);
        gtk_tree_view_column_set_title(col, column_titles.at(idx).data());
        gtk_tree_view_column_set_sort_indicator(col, true);
        gtk_tree_view_column_set_sort_column_id(col, magic_enum::enum_integer(cols.at(idx)));
        gtk_tree_view_column_set_sort_order(col, GtkSortType::GTK_SORT_DESCENDING);
    }
}

void
ptk_file_browser_refresh(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    if (file_browser->busy)
    {
        // a dir is already loading
        return;
    }

    if (!std::filesystem::is_directory(ptk_file_browser_get_cwd(file_browser)))
    {
        on_close_notebook_page(nullptr, file_browser);
        return;
    }

    // save cursor's file path for later re-selection
    GtkTreePath* tree_path = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    vfs::file_info file;

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_get_cursor(EXO_ICON_VIEW(file_browser->folder_view), &tree_path, nullptr);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_get_cursor(GTK_TREE_VIEW(file_browser->folder_view), &tree_path, nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
    }

    std::filesystem::path cursor_path;
    if (tree_path && model && gtk_tree_model_get_iter(model, &it, tree_path))
    {
        gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
        if (file)
        {
            cursor_path = ptk_file_browser_get_cwd(file_browser) / file->get_name();
        }
    }
    gtk_tree_path_free(tree_path);

    // these steps are similar to chdir
    // remove old dir object
    if (file_browser->dir)
    {
        g_signal_handlers_disconnect_matched(file_browser->dir,
                                             GSignalMatchType::G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(file_browser->dir);
        file_browser->dir = nullptr;
    }

    // destroy file list and create new one
    ptk_file_browser_update_model(file_browser);

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif

    // begin load dir
    file_browser->busy = true;
    file_browser->dir = vfs_dir_get_by_path(ptk_file_browser_get_cwd(file_browser));

    file_browser->run_event<spacefm::signal::chdir_begin>();

    if (vfs_dir_is_file_listed(file_browser->dir))
    {
        on_dir_file_listed(file_browser, false);
        if (std::filesystem::exists(cursor_path))
        {
            ptk_file_browser_select_file(file_browser, cursor_path);
        }
        file_browser->busy = false;
    }
    else
    {
        file_browser->busy = true;
        std::free(file_browser->select_path);
        file_browser->select_path = ztd::strdup(cursor_path);
    }
    file_browser->signal_file_listed =
        file_browser->dir->add_event<spacefm::signal::file_listed>(on_dir_file_listed,
                                                                   file_browser);
}

u32
ptk_file_browser_get_n_all_files(PtkFileBrowser* file_browser)
{
    return file_browser->dir ? file_browser->dir->file_list.size() : 0;
}

u32
ptk_file_browser_get_n_visible_files(PtkFileBrowser* file_browser)
{
    return file_browser->file_list
               ? gtk_tree_model_iter_n_children(file_browser->file_list, nullptr)
               : 0;
}

GList*
folder_view_get_selected_items(PtkFileBrowser* file_browser, GtkTreeModel** model)
{
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            *model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            return exo_icon_view_get_selected_items(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case ptk::file_browser::view_mode::list_view:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            return gtk_tree_selection_get_selected_rows(tree_sel, model);
            break;
    }
    return nullptr;
}

static char*
folder_view_get_drop_dir(PtkFileBrowser* file_browser, i32 x, i32 y)
{
    GtkTreePath* tree_path = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    vfs::file_info file;

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_widget_to_icon_coords(EXO_ICON_VIEW(file_browser->folder_view),
                                                x,
                                                y,
                                                &x,
                                                &y);
            tree_path = folder_view_get_tree_path_at_pos(file_browser, x, y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case ptk::file_browser::view_mode::list_view:
            // if drag is in progress, get the dest row path
            gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(file_browser->folder_view),
                                            &tree_path,
                                            nullptr);
            if (!tree_path)
            {
                // no drag in progress, get drop path
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(file_browser->folder_view),
                                              x,
                                              y,
                                              nullptr,
                                              &col,
                                              nullptr,
                                              nullptr);
                if (col == gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), 0))
                {
                    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(file_browser->folder_view),
                                                      x,
                                                      y,
                                                      &tree_path,
                                                      nullptr);
                    model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
                }
            }
            else
            {
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
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

        gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
        if (file)
        {
            if (file->is_directory())
            {
                dest_path = ptk_file_browser_get_cwd(file_browser) / file->get_name();
            }
            else /* Drop on a file, not directory */
            {
                /* Return current directory */
                dest_path = ptk_file_browser_get_cwd(file_browser);
            }
            vfs_file_info_unref(file);
        }
        gtk_tree_path_free(tree_path);
    }
    else
    {
        dest_path = ptk_file_browser_get_cwd(file_browser);
    }
    return ztd::strdup(dest_path);
}

static void
on_folder_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y,
                                  GtkSelectionData* sel_data, u32 info, u32 time, void* user_data)
{
    (void)x;
    (void)y;
    (void)info;

    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(user_data);
    /* Do not call the default handler */
    g_signal_stop_emission_by_name(widget, "drag-data-received");

    if ((gtk_selection_data_get_length(sel_data) >= 0) &&
        (gtk_selection_data_get_format(sel_data) == 8))
    {
        // (list view) use stored x and y because == 0 for update drag status
        //             when is last row (gtk2&3 bug?)
        // and because exo_icon_view has no get_drag_dest_row
        const char* dest_dir =
            folder_view_get_drop_dir(file_browser, file_browser->drag_x, file_browser->drag_y);
        // ztd::logger::info("FB DnD dest_dir = {}", dest_dir );
        if (dest_dir)
        {
            if (file_browser->pending_drag_status)
            {
                // ztd::logger::debug("DnD DEFAULT");

                // We only want to update drag status, not really want to drop
                gdk_drag_status(drag_context, GdkDragAction::GDK_ACTION_DEFAULT, time);

                // DnD is still ongoing, do not continue
                file_browser->pending_drag_status = false;
                return;
            }

            char** list;
            char** puri;
            puri = list = gtk_selection_data_get_uris(sel_data);

            if (puri)
            {
                // We only want to update drag status, not really want to drop
                const auto dest_dir_stat = ztd::stat(dest_dir);

                const dev_t dest_dev = dest_dir_stat.dev();
                const ino_t dest_inode = dest_dir_stat.ino();
                if (file_browser->drag_source_dev == 0)
                {
                    file_browser->drag_source_dev = dest_dev;
                    for (; *puri; ++puri)
                    {
                        const std::filesystem::path file_path = Glib::filename_from_uri(*puri);

                        const auto file_path_stat = ztd::stat(file_path);
                        if (file_path_stat.is_valid())
                        {
                            if (file_path_stat.dev() != dest_dev)
                            {
                                // different devices - store source device
                                file_browser->drag_source_dev = file_path_stat.dev();
                                break;
                            }
                            else if (file_browser->drag_source_inode == 0)
                            {
                                // same device - store source parent inode
                                const auto src_dir = file_path.parent_path();
                                const auto src_dir_stat = ztd::stat(src_dir);
                                if (src_dir_stat.is_valid())
                                {
                                    file_browser->drag_source_inode = src_dir_stat.ino();
                                }
                            }
                        }
                    }
                }
                g_strfreev(list);

                vfs::file_task_type file_action;

                if (file_browser->drag_source_dev != dest_dev ||
                    file_browser->drag_source_inode == dest_inode)
                { // src and dest are on different devices or same dir
                    // ztd::logger::debug("DnD COPY");
                    gdk_drag_status(drag_context, GdkDragAction::GDK_ACTION_COPY, time);
                    file_action = vfs::file_task_type::copy;
                }
                else
                {
                    // ztd::logger::debug("DnD MOVE");
                    gdk_drag_status(drag_context, GdkDragAction::GDK_ACTION_MOVE, time);
                    file_action = vfs::file_task_type::move;
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

                    GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
                    PtkFileTask* ptask = ptk_file_task_new(file_action,
                                                           file_list,
                                                           dest_dir,
                                                           GTK_WINDOW(parent_win),
                                                           file_browser->task_view);
                    ptk_file_task_run(ptask);
                }
                gtk_drag_finish(drag_context, true, false, time);
                return;
            }
        }
    }

    /* If we are only getting drag status, not finished. */
    if (file_browser->pending_drag_status)
    {
        file_browser->pending_drag_status = false;
        return;
    }
    gtk_drag_finish(drag_context, false, false, time);
}

static void
on_folder_view_drag_data_get(GtkWidget* widget, GdkDragContext* drag_context,
                             GtkSelectionData* sel_data, u32 info, u32 time,
                             PtkFileBrowser* file_browser)
{
    (void)drag_context;
    (void)info;
    (void)time;
    GdkAtom type = gdk_atom_intern("text/uri-list", false);
    std::string uri_list;
    const std::vector<vfs::file_info> sel_files = ptk_file_browser_get_selected_files(file_browser);

    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-data-get");

    // drag_context->suggested_action = GdkDragAction::GDK_ACTION_MOVE;

    for (vfs::file_info file : sel_files)
    {
        const auto full_path = ptk_file_browser_get_cwd(file_browser) / file->get_name();
        const std::string uri = Glib::filename_to_uri(full_path);

        uri_list.append(std::format("{}\n", uri));
    }

    vfs_file_info_list_free(sel_files);
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
    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-begin");
    gtk_drag_set_icon_default(drag_context);
    file_browser->is_drag = true;
}

static GtkTreePath*
folder_view_get_tree_path_at_pos(PtkFileBrowser* file_browser, i32 x, i32 y)
{
    GtkTreePath* tree_path;

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            tree_path =
                exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(file_browser->folder_view), x, y);
            break;
        case ptk::file_browser::view_mode::list_view:
            gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(file_browser->folder_view),
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
    GtkAllocation allocation;

    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-motion");

    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(gtk_widget_get_parent(widget));

    // GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);
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

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            // store x and y because exo_icon_view has no get_drag_dest_row
            file_browser->drag_x = x;
            file_browser->drag_y = y;
            exo_icon_view_widget_to_icon_coords(EXO_ICON_VIEW(widget), x, y, &x, &y);
            tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), x, y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
            break;
        case ptk::file_browser::view_mode::list_view:
            // store x and y because == 0 for update drag status when is last row
            file_browser->drag_x = x;
            file_browser->drag_y = y;
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
            vfs::file_info file;
            gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
            if (!file || !file->is_directory())
            {
                gtk_tree_path_free(tree_path);
                tree_path = nullptr;
            }
            vfs_file_info_unref(file);
        }
    }

    switch (file_browser->view_mode)
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
                    file_browser->pending_drag_status = true;
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
    (void)drag_context;
    (void)time;
    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-leave");
    file_browser->drag_source_dev = 0;
    file_browser->drag_source_inode = 0;

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
    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-drop");

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

    switch (file_browser->view_mode)
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
    file_browser->is_drag = false;
}

void
ptk_file_browser_rename_selected_files(PtkFileBrowser* file_browser,
                                       const std::span<const vfs::file_info> sel_files,
                                       const std::filesystem::path& cwd)
{
    if (!file_browser)
    {
        return;
    }

    if (sel_files.empty())
    {
        return;
    }

    gtk_widget_grab_focus(file_browser->folder_view);
    gtk_widget_get_toplevel(GTK_WIDGET(file_browser));

    for (vfs::file_info file : sel_files)
    {
        if (!ptk_rename_file(file_browser,
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
ptk_file_browser_paste_link(PtkFileBrowser* file_browser) // MOD added
{
    ptk_clipboard_paste_links(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                              ptk_file_browser_get_cwd(file_browser),
                              GTK_TREE_VIEW(file_browser->task_view),
                              nullptr,
                              nullptr);
}

void
ptk_file_browser_paste_target(PtkFileBrowser* file_browser) // MOD added
{
    ptk_clipboard_paste_targets(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                                ptk_file_browser_get_cwd(file_browser),
                                GTK_TREE_VIEW(file_browser->task_view),
                                nullptr,
                                nullptr);
}

const std::vector<vfs::file_info>
ptk_file_browser_get_selected_files(PtkFileBrowser* file_browser)
{
    GtkTreeModel* model;
    std::vector<vfs::file_info> file_list;
    GList* sel_files = folder_view_get_selected_items(file_browser, &model);
    if (!sel_files)
    {
        return file_list;
    }

    file_list.reserve(g_list_length(sel_files));
    for (GList* sel = sel_files; sel; sel = g_list_next(sel))
    {
        GtkTreeIter it;
        vfs::file_info file;
        gtk_tree_model_get_iter(model, &it, (GtkTreePath*)sel->data);
        gtk_tree_model_get(model, &it, ptk::file_list::column::info, &file, -1);
        file_list.emplace_back(file);
    }
    g_list_foreach(sel_files, (GFunc)gtk_tree_path_free, nullptr);
    g_list_free(sel_files);

    return file_list;
}

static void
ptk_file_browser_open_selected_files_with_app(PtkFileBrowser* file_browser,
                                              const std::string_view app_desktop = "")
{
    const std::vector<vfs::file_info> sel_files = ptk_file_browser_get_selected_files(file_browser);

    ptk_open_files_with_app(ptk_file_browser_get_cwd(file_browser),
                            sel_files,
                            app_desktop,
                            file_browser,
                            false,
                            false);

    vfs_file_info_list_free(sel_files);
}

void
ptk_file_browser_open_selected_files(PtkFileBrowser* file_browser)
{
    if (xset_opener(file_browser, 1))
    {
        return;
    }
    ptk_file_browser_open_selected_files_with_app(file_browser);
}

void
ptk_file_browser_copycmd(PtkFileBrowser* file_browser,
                         const std::span<const vfs::file_info> sel_files,
                         const std::filesystem::path& cwd, xset::name setname)
{
    if (!file_browser)
    {
        return;
    }

    std::optional<std::filesystem::path> copy_dest;
    std::optional<std::filesystem::path> move_dest;

    if (setname == xset::name::copy_tab_prev)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_control_code_prev);
    }
    else if (setname == xset::name::copy_tab_next)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_control_code_next);
    }
    else if (setname == xset::name::copy_tab_1)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_1);
    }
    else if (setname == xset::name::copy_tab_2)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_2);
    }
    else if (setname == xset::name::copy_tab_3)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_3);
    }
    else if (setname == xset::name::copy_tab_4)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_4);
    }
    else if (setname == xset::name::copy_tab_5)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_5);
    }
    else if (setname == xset::name::copy_tab_6)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_6);
    }
    else if (setname == xset::name::copy_tab_7)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_7);
    }
    else if (setname == xset::name::copy_tab_8)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_8);
    }
    else if (setname == xset::name::copy_tab_9)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_9);
    }
    else if (setname == xset::name::copy_tab_10)
    {
        copy_dest = main_window_get_tab_cwd(file_browser, tab_10);
    }
    else if (setname == xset::name::copy_panel_prev)
    {
        copy_dest = main_window_get_panel_cwd(file_browser, panel_control_code_prev);
    }
    else if (setname == xset::name::copy_panel_next)
    {
        copy_dest = main_window_get_panel_cwd(file_browser, panel_control_code_next);
    }
    else if (setname == xset::name::copy_panel_1)
    {
        copy_dest = main_window_get_panel_cwd(file_browser, panel_1);
    }
    else if (setname == xset::name::copy_panel_3)
    {
        copy_dest = main_window_get_panel_cwd(file_browser, panel_2);
    }
    else if (setname == xset::name::copy_panel_3)
    {
        copy_dest = main_window_get_panel_cwd(file_browser, panel_3);
    }
    else if (setname == xset::name::copy_panel_4)
    {
        copy_dest = main_window_get_panel_cwd(file_browser, panel_4);
    }
    else if (setname == xset::name::copy_loc_last)
    {
        xset_t set2 = xset_get(xset::name::copy_loc_last);
        copy_dest = set2->desc.value();
    }
    else if (setname == xset::name::move_tab_prev)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_control_code_prev);
    }
    else if (setname == xset::name::move_tab_next)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_control_code_next);
    }
    else if (setname == xset::name::move_tab_1)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_1);
    }
    else if (setname == xset::name::move_tab_2)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_2);
    }
    else if (setname == xset::name::move_tab_3)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_3);
    }
    else if (setname == xset::name::move_tab_4)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_4);
    }
    else if (setname == xset::name::move_tab_5)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_5);
    }
    else if (setname == xset::name::move_tab_6)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_6);
    }
    else if (setname == xset::name::move_tab_7)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_7);
    }
    else if (setname == xset::name::move_tab_8)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_8);
    }
    else if (setname == xset::name::move_tab_9)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_9);
    }
    else if (setname == xset::name::move_tab_10)
    {
        move_dest = main_window_get_tab_cwd(file_browser, tab_10);
    }
    else if (setname == xset::name::move_panel_prev)
    {
        move_dest = main_window_get_panel_cwd(file_browser, panel_control_code_prev);
    }
    else if (setname == xset::name::move_panel_next)
    {
        move_dest = main_window_get_panel_cwd(file_browser, panel_control_code_next);
    }
    else if (setname == xset::name::move_panel_1)
    {
        move_dest = main_window_get_panel_cwd(file_browser, panel_1);
    }
    else if (setname == xset::name::move_panel_2)
    {
        move_dest = main_window_get_panel_cwd(file_browser, panel_2);
    }
    else if (setname == xset::name::move_panel_3)
    {
        move_dest = main_window_get_panel_cwd(file_browser, panel_3);
    }
    else if (setname == xset::name::move_panel_4)
    {
        move_dest = main_window_get_panel_cwd(file_browser, panel_4);
    }
    else if (setname == xset::name::move_loc_last)
    {
        xset_t set2 = xset_get(xset::name::copy_loc_last);
        move_dest = ztd::strdup(set2->desc.value());
    }

    if ((setname == xset::name::copy_loc || setname == xset::name::copy_loc_last ||
         setname == xset::name::move_loc || setname == xset::name::move_loc_last) &&
        !copy_dest && !move_dest)
    {
        std::filesystem::path folder;
        xset_t set2 = xset_get(xset::name::copy_loc_last);
        if (set2->desc)
        {
            folder = set2->desc.value();
        }
        else
        {
            folder = cwd;
        }
        const auto path =
            xset_file_dialog(GTK_WIDGET(file_browser),
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
            set2 = xset_get(xset::name::copy_loc_last);
            xset_set_var(set2, xset::var::desc, path.value().string());
        }
        else
        {
            return;
        }
    }

    if (copy_dest || move_dest)
    {
        vfs::file_task_type file_action;
        std::optional<std::filesystem::path> dest_dir;

        if (copy_dest)
        {
            file_action = vfs::file_task_type::copy;
            dest_dir = copy_dest;
        }
        else
        {
            file_action = vfs::file_task_type::move;
            dest_dir = move_dest;
        }

        if (std::filesystem::equivalent(dest_dir.value(), cwd))
        {
            xset_msg_dialog(GTK_WIDGET(file_browser),
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Invalid Destination",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            "Destination same as source");
            return;
        }

        // rebuild sel_files with full paths
        std::vector<std::filesystem::path> file_list;
        file_list.reserve(sel_files.size());
        for (vfs::file_info file : sel_files)
        {
            const auto file_path = cwd / file->get_name();
            file_list.emplace_back(file_path);
        }

        // task
        PtkFileTask* ptask =
            ptk_file_task_new(file_action,
                              file_list,
                              dest_dir.value(),
                              GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                              file_browser->task_view);
        ptk_file_task_run(ptask);
    }
    else
    {
        xset_msg_dialog(GTK_WIDGET(file_browser),
                        GtkMessageType::GTK_MESSAGE_ERROR,
                        "Invalid Destination",
                        GtkButtonsType::GTK_BUTTONS_OK,
                        "Invalid destination");
    }
}

void
ptk_file_browser_hide_selected(PtkFileBrowser* file_browser,
                               const std::span<const vfs::file_info> sel_files,
                               const std::filesystem::path& cwd)
{
    const i32 response = xset_msg_dialog(
        GTK_WIDGET(file_browser),
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

    if (sel_files.empty())
    {
        ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                       "Error",
                       "No files are selected");
        return;
    }

    for (vfs::file_info file : sel_files)
    {
        if (!vfs_dir_add_hidden(cwd, file->get_name()))
        {
            ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                           "Error",
                           "Error hiding files");
        }
    }

    // refresh from here causes a segfault occasionally
    // ptk_file_browser_refresh( nullptr, file_browser );
}

void
ptk_file_browser_file_properties(PtkFileBrowser* file_browser, i32 page)
{
    if (!file_browser)
    {
        return;
    }

    std::string dir_name;
    std::vector<vfs::file_info> sel_files = ptk_file_browser_get_selected_files(file_browser);
    const auto cwd = ptk_file_browser_get_cwd(file_browser);
    if (sel_files.empty())
    {
        vfs::file_info file = vfs_file_info_new(ptk_file_browser_get_cwd(file_browser));
        sel_files.emplace_back(file);
        dir_name = cwd.parent_path();
    }
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));

    gtk_orientable_set_orientation(GTK_ORIENTABLE(parent),
                                   GtkOrientation::GTK_ORIENTATION_VERTICAL);

    ptk_show_file_properties(GTK_WINDOW(parent),
                             !dir_name.empty() ? dir_name : cwd.string(),
                             sel_files,
                             page);
    vfs_file_info_list_free(sel_files);
}

void
on_popup_file_properties_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    GObject* popup = G_OBJECT(user_data);
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(g_object_get_data(popup, "PtkFileBrowser"));
    ptk_file_browser_file_properties(file_browser, 0);
}

void
ptk_file_browser_show_hidden_files(PtkFileBrowser* file_browser, bool show)
{
    if (!!file_browser->show_hidden_files == show)
    {
        return;
    }
    file_browser->show_hidden_files = show;

    if (file_browser->file_list)
    {
        ptk_file_browser_update_model(file_browser);

        file_browser->run_event<spacefm::signal::change_sel>();
    }

    if (file_browser->side_dir)
    {
        ptk_dir_tree_view_show_hidden_files(GTK_TREE_VIEW(file_browser->side_dir),
                                            file_browser->show_hidden_files);
    }

    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::show_hidden);
}

static bool
on_dir_tree_button_press(GtkWidget* view, GdkEventButton* event, PtkFileBrowser* file_browser)
{
    ptk_file_browser_focus_me(file_browser);

    if ((event_handler->win_click->s || event_handler->win_click->ob2_data) &&
        main_window_event(file_browser->main_window,
                          event_handler->win_click,
                          xset::name::evt_win_click,
                          0,
                          0,
                          "dirtree",
                          0,
                          event->button,
                          event->state,
                          true))
    {
        return false;
    }

    if (event->type == GdkEventType::GDK_BUTTON_PRESS && event->button == 2) /* middle click */
    {
        /* left and right click handled in ptk-dir-tree-view.c
         * on_dir_tree_view_button_press() */
        GtkTreePath* tree_path;
        GtkTreeIter it;

        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          static_cast<i32>(event->x),
                                          static_cast<i32>(event->y),
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr))
        {
            if (gtk_tree_model_get_iter(model, &it, tree_path))
            {
                vfs::file_info file;
                gtk_tree_model_get(model, &it, ptk::dir_tree::column::info, &file, -1);
                if (file)
                {
                    const auto file_path = ptk_dir_view_get_dir_path(model, &it);
                    file_browser->run_event<spacefm::signal::open_item>(file_path,
                                                                        ptk::open_action::new_tab);
                    vfs_file_info_unref(file);
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
    GtkWidget* dir_tree = ptk_dir_tree_view_new(file_browser, file_browser->show_hidden_files);
    // GtkTreeSelection* dir_tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dir_tree));
    g_signal_connect(dir_tree,
                     "row-activated",
                     G_CALLBACK(on_dir_tree_row_activated),
                     file_browser);
    g_signal_connect(dir_tree,
                     "button-press-event",
                     G_CALLBACK(on_dir_tree_button_press),
                     file_browser);

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
        case ptk::file_browser::sort_order::mtime:
            col = ptk::file_list::column::mtime;
            break;
        case ptk::file_browser::sort_order::type:
            col = ptk::file_list::column::desc;
            break;
        case ptk::file_browser::sort_order::perm:
            col = ptk::file_list::column::perm;
            break;
        case ptk::file_browser::sort_order::owner:
            col = ptk::file_list::column::owner;
            break;
    }
    return magic_enum::enum_integer(col);
}

void
ptk_file_browser_read_sort_extra(PtkFileBrowser* file_browser)
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(file_browser->file_list);
    if (!list)
    {
        return;
    }

    list->sort_alphanum = xset_get_b_panel(file_browser->mypanel, xset::panel::sort_extra);
#if 0
    list->sort_natural = xset_get_b_panel(file_browser->mypanel, xset::panel::SORT_EXTRA);
#endif
    list->sort_case =
        xset_get_int_panel(file_browser->mypanel, xset::panel::sort_extra, xset::var::x) ==
        xset::b::xtrue;
    list->sort_dir = ptk::file_list::sort_dir(
        xset_get_int_panel(file_browser->mypanel, xset::panel::sort_extra, xset::var::y));
    list->sort_hidden_first =
        xset_get_int_panel(file_browser->mypanel, xset::panel::sort_extra, xset::var::z) ==
        xset::b::xtrue;
}

void
ptk_file_browser_set_sort_extra(PtkFileBrowser* file_browser, xset::name setname)
{
    if (!file_browser)
    {
        return;
    }

    xset_t set = xset_get(setname);

    if (!ztd::startswith(set->name, "sortx_"))
    {
        return;
    }

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(file_browser->file_list);
    if (!list)
    {
        return;
    }
    const panel_t panel = file_browser->mypanel;

    if (set->xset_name == xset::name::sortx_alphanum)
    {
        list->sort_alphanum = set->b == xset::b::xtrue;
        xset_set_b_panel(panel, xset::panel::sort_extra, list->sort_alphanum);
    }
#if 0
    else if (set->xset_name ==  xset::name::sortx_natural)
    {
        list->sort_natural = set->b == xset::b::XSET_B_TRUE;
        xset_set_b_panel(panel, xset::panel::SORT_EXTRA, list->sort_natural);
    }
#endif
    else if (set->xset_name == xset::name::sortx_case)
    {
        list->sort_case = set->b == xset::b::xtrue;
        xset_set_panel(panel, xset::panel::sort_extra, xset::var::x, std::to_string(set->b));
    }
    else if (set->xset_name == xset::name::sortx_directories)
    {
        list->sort_dir = ptk::file_list::sort_dir::first;
        xset_set_panel(panel,
                       xset::panel::sort_extra,
                       xset::var::y,
                       std::to_string(magic_enum::enum_integer(ptk::file_list::sort_dir::first)));
    }
    else if (set->xset_name == xset::name::sortx_files)
    {
        list->sort_dir = ptk::file_list::sort_dir::last;
        xset_set_panel(panel,
                       xset::panel::sort_extra,
                       xset::var::y,
                       std::to_string(magic_enum::enum_integer(ptk::file_list::sort_dir::last)));
    }
    else if (set->xset_name == xset::name::sortx_mix)
    {
        list->sort_dir = ptk::file_list::sort_dir::mixed;
        xset_set_panel(panel,
                       xset::panel::sort_extra,
                       xset::var::y,
                       std::to_string(magic_enum::enum_integer(ptk::file_list::sort_dir::mixed)));
    }
    else if (set->xset_name == xset::name::sortx_hidfirst)
    {
        list->sort_hidden_first = set->b == xset::b::xtrue;
        xset_set_panel(panel, xset::panel::sort_extra, xset::var::z, std::to_string(set->b));
    }
    else if (set->xset_name == xset::name::sortx_hidlast)
    {
        list->sort_hidden_first = set->b != xset::b::xtrue;
        xset_set_panel(panel,
                       xset::panel::sort_extra,
                       xset::var::z,
                       std::to_string(set->b == xset::b::xtrue ? xset::b::xfalse : xset::b::xtrue));
    }
    ptk_file_list_sort(list);
}

void
ptk_file_browser_set_sort_order(PtkFileBrowser* file_browser, ptk::file_browser::sort_order order)
{
    if (order == file_browser->sort_order)
    {
        return;
    }

    file_browser->sort_order = order;
    const i32 col = file_list_order_from_sort_order(order);

    if (file_browser->file_list)
    {
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(file_browser->file_list),
                                             col,
                                             file_browser->sort_type);
    }
}

void
ptk_file_browser_set_sort_type(PtkFileBrowser* file_browser, GtkSortType order)
{
    if (order != file_browser->sort_type)
    {
        file_browser->sort_type = order;
        if (file_browser->file_list)
        {
            i32 col;
            GtkSortType old_order;
            gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(file_browser->file_list),
                                                 &col,
                                                 &old_order);
            gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(file_browser->file_list),
                                                 col,
                                                 order);
        }
    }
}

/* FIXME: Do not recreate the view if previous view is compact view */
void
ptk_file_browser_view_as_icons(PtkFileBrowser* file_browser)
{
    if (file_browser->view_mode == ptk::file_browser::view_mode::icon_view &&
        file_browser->folder_view)
    {
        return;
    }

    show_thumbnails(file_browser,
                    PTK_FILE_LIST_REINTERPRET(file_browser->file_list),
                    true,
                    file_browser->max_thumbnail);

    file_browser->view_mode = ptk::file_browser::view_mode::icon_view;
    if (file_browser->folder_view)
    {
        gtk_widget_destroy(file_browser->folder_view);
    }
    file_browser->folder_view =
        create_folder_view(file_browser, ptk::file_browser::view_mode::icon_view);
    exo_icon_view_set_model(EXO_ICON_VIEW(file_browser->folder_view), file_browser->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_widget_show(file_browser->folder_view);
    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);
}

/* FIXME: Do not recreate the view if previous view is icon view */
void
ptk_file_browser_view_as_compact_list(PtkFileBrowser* file_browser)
{
    if (file_browser->view_mode == ptk::file_browser::view_mode::compact_view &&
        file_browser->folder_view)
    {
        return;
    }

    show_thumbnails(file_browser,
                    PTK_FILE_LIST_REINTERPRET(file_browser->file_list),
                    file_browser->large_icons,
                    file_browser->max_thumbnail);

    file_browser->view_mode = ptk::file_browser::view_mode::compact_view;
    if (file_browser->folder_view)
    {
        gtk_widget_destroy(file_browser->folder_view);
    }
    file_browser->folder_view =
        create_folder_view(file_browser, ptk::file_browser::view_mode::compact_view);
    exo_icon_view_set_model(EXO_ICON_VIEW(file_browser->folder_view), file_browser->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_widget_show(file_browser->folder_view);
    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);
}

void
ptk_file_browser_view_as_list(PtkFileBrowser* file_browser)
{
    if (file_browser->view_mode == ptk::file_browser::view_mode::list_view &&
        file_browser->folder_view)
    {
        return;
    }

    show_thumbnails(file_browser,
                    PTK_FILE_LIST_REINTERPRET(file_browser->file_list),
                    file_browser->large_icons,
                    file_browser->max_thumbnail);

    file_browser->view_mode = ptk::file_browser::view_mode::list_view;
    if (file_browser->folder_view)
    {
        gtk_widget_destroy(file_browser->folder_view);
    }
    file_browser->folder_view =
        create_folder_view(file_browser, ptk::file_browser::view_mode::list_view);
    gtk_tree_view_set_model(GTK_TREE_VIEW(file_browser->folder_view), file_browser->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_ALWAYS);
    gtk_widget_show(file_browser->folder_view);
    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);
}

u32
ptk_file_browser_get_n_sel(PtkFileBrowser* file_browser, u64* sel_size, u64* sel_disk_size)
{
    if (sel_size)
    {
        *sel_size = file_browser->sel_size;
    }
    if (sel_disk_size)
    {
        *sel_disk_size = file_browser->sel_disk_size;
    }
    return file_browser->n_sel_files;
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

static void
show_thumbnails(PtkFileBrowser* file_browser, PtkFileList* list, bool is_big, i32 max_file_size)
{
    /* This function collects all calls to ptk_file_list_show_thumbnails()
     * and disables them if change detection is blacklisted on current device */
    if (!(file_browser && file_browser->dir))
    {
        max_file_size = 0;
    }
    else if (file_browser->dir->avoid_changes)
    {
        max_file_size = 0;
    }
    ptk_file_list_show_thumbnails(list, is_big, max_file_size);
    ptk_file_browser_update_toolbar_widgets(file_browser, xset::tool::show_thumb);
}

void
ptk_file_browser_show_thumbnails(PtkFileBrowser* file_browser, i32 max_file_size)
{
    file_browser->max_thumbnail = max_file_size;
    if (file_browser->file_list)
    {
        show_thumbnails(file_browser,
                        PTK_FILE_LIST_REINTERPRET(file_browser->file_list),
                        file_browser->large_icons,
                        max_file_size);
    }
}

void
ptk_file_browser_set_single_click(PtkFileBrowser* file_browser, bool single_click)
{
    if (single_click == file_browser->single_click)
    {
        return;
    }

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_set_single_click(EXO_ICON_VIEW(file_browser->folder_view), single_click);
            break;
        case ptk::file_browser::view_mode::list_view:
            // gtk_tree_view_set_single_click(GTK_TREE_VIEW(file_browser->folder_view),
            // single_click);
            break;
    }

    file_browser->single_click = single_click;
}

void
ptk_file_browser_set_single_click_timeout(PtkFileBrowser* file_browser, u32 timeout)
{
    if (timeout == file_browser->single_click_timeout)
    {
        return;
    }

    switch (file_browser->view_mode)
    {
        case ptk::file_browser::view_mode::icon_view:
        case ptk::file_browser::view_mode::compact_view:
            exo_icon_view_set_single_click_timeout(EXO_ICON_VIEW(file_browser->folder_view),
                                                   timeout);
            break;
        case ptk::file_browser::view_mode::list_view:
            // gtk_tree_view_set_single_click_timeout(GTK_TREE_VIEW(file_browser->folder_view),
            //                                       timeout);
            break;
    }

    file_browser->single_click_timeout = timeout;
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

void
ptk_file_browser_focus(GtkMenuItem* item, PtkFileBrowser* file_browser, i32 job2)
{
    GtkWidget* widget;
    i32 job;
    if (item)
    {
        job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job"));
    }
    else
    {
        job = job2;
    }

    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    const panel_t p = file_browser->mypanel;
    const xset::main_window_panel mode = main_window->panel_context.at(p);
    switch (job)
    {
        case 0:
            // path bar
            if (!xset_get_b_panel_mode(p, xset::panel::show_toolbox, mode))
            {
                xset_set_b_panel_mode(p, xset::panel::show_toolbox, mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->path_bar;
            break;
        case 1:
            if (!xset_get_b_panel_mode(p, xset::panel::show_dirtree, mode))
            {
                xset_set_b_panel_mode(p, xset::panel::show_dirtree, mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->side_dir;
            break;
        case 2:
            // Deprecated - bookmark
            widget = nullptr;
            break;
        case 3:
            if (!xset_get_b_panel_mode(p, xset::panel::show_devmon, mode))
            {
                xset_set_b_panel_mode(p, xset::panel::show_devmon, mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->side_dev;
            break;
        case 4:
            widget = file_browser->folder_view;
            break;
        default:
            return;
    }
    if (gtk_widget_get_visible(widget))
    {
        gtk_widget_grab_focus(GTK_WIDGET(widget));
    }
}

static void
focus_folder_view(PtkFileBrowser* file_browser)
{
    gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));

    file_browser->run_event<spacefm::signal::change_pane>();
}

void
ptk_file_browser_focus_me(PtkFileBrowser* file_browser)
{
    file_browser->run_event<spacefm::signal::change_pane>();
}

void
ptk_file_browser_go_tab(GtkMenuItem* item, PtkFileBrowser* file_browser, i32 t)
{
    // ztd::logger::info("ptk_file_browser_go_tab fb={:p}", fmt::ptr(file_browser));
    GtkWidget* notebook = file_browser->mynotebook;
    tab_t tab_num;
    if (item)
    {
        tab_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tab_num"));
    }
    else
    {
        tab_num = t;
    }

    switch (tab_num)
    {
        case tab_control_code_prev:
            // prev
            if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) == 0)
            {
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),
                                              gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) - 1);
            }
            else
            {
                gtk_notebook_prev_page(GTK_NOTEBOOK(notebook));
            }
            break;
        case tab_control_code_next:
            // next
            if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) + 1 ==
                gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)))
            {
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
            }
            else
            {
                gtk_notebook_next_page(GTK_NOTEBOOK(notebook));
            }
            break;
        case tab_control_code_close:
            // close
            on_close_notebook_page(nullptr, file_browser);
            break;
        case tab_control_code_restore:
            // restore
            on_restore_notebook_page(nullptr, file_browser);
            break;
        default:
            // set tab
            if (tab_num <= gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) && tab_num > 0)
            {
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), tab_num - 1);
            }
            break;
    }
}

void
ptk_file_browser_open_in_tab(PtkFileBrowser* file_browser, tab_t tab_num,
                             const std::filesystem::path& file_path)
{
    tab_t page_x;
    GtkWidget* notebook = file_browser->mynotebook;
    const tab_t cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    const tab_t pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));

    switch (tab_num)
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
            page_x = tab_num - 1;
            break;
    }

    if (page_x > -1 && page_x < pages && page_x != cur_page)
    {
        PtkFileBrowser* a_browser =
            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_x));

        ptk_file_browser_chdir(a_browser, file_path, ptk::file_browser::chdir_mode::add_history);
    }
}

void
ptk_file_browser_on_permission(GtkMenuItem* item, PtkFileBrowser* file_browser,
                               const std::span<const vfs::file_info> sel_files,
                               const std::filesystem::path& cwd)
{
    if (sel_files.empty())
    {
        return;
    }

    xset_t set = xset_get(static_cast<const char*>(g_object_get_data(G_OBJECT(item), "set")));
    if (!set || !file_browser)
    {
        return;
    }

    std::string name;
    std::string prog;
    bool as_root = false;

    const std::string user1 = "1000";
    const std::string user2 = "1001";
    const std::string myuser = std::format("{}", geteuid());

    if (ztd::startswith(set->name, "perm_"))
    {
        name = ztd::removeprefix(set->name, "perm_");
        if (ztd::startswith(name, "go") || ztd::startswith(name, "ugo"))
        {
            prog = "chmod -R";
        }
        else
        {
            prog = "chmod";
        }
    }
    else if (ztd::startswith(set->name, "rperm_"))
    {
        name = ztd::removeprefix(set->name, "rperm_");
        if (ztd::startswith(name, "go") || ztd::startswith(name, "ugo"))
        {
            prog = "chmod -R";
        }
        else
        {
            prog = "chmod";
        }
        as_root = true;
    }
    else if (ztd::startswith(set->name, "own_"))
    {
        name = ztd::removeprefix(set->name, "own_");
        prog = "chown";
        as_root = true;
    }
    else if (ztd::startswith(set->name, "rown_"))
    {
        name = ztd::removeprefix(set->name, "rown_");
        prog = "chown -R";
        as_root = true;
    }
    else
    {
        return;
    }

    std::string cmd;
    if (ztd::same(name, "r"))
    {
        cmd = "u+r-wx,go-rwx";
    }
    else if (ztd::same(name, "rw"))
    {
        cmd = "u+rw-x,go-rwx";
    }
    else if (ztd::same(name, "rwx"))
    {
        cmd = "u+rwx,go-rwx";
    }
    else if (ztd::same(name, "r_r"))
    {
        cmd = "u+r-wx,g+r-wx,o-rwx";
    }
    else if (ztd::same(name, "rw_r"))
    {
        cmd = "u+rw-x,g+r-wx,o-rwx";
    }
    else if (ztd::same(name, "rw_rw"))
    {
        cmd = "u+rw-x,g+rw-x,o-rwx";
    }
    else if (ztd::same(name, "rwxr_x"))
    {
        cmd = "u+rwx,g+rx-w,o-rwx";
    }
    else if (ztd::same(name, "rwxrwx"))
    {
        cmd = "u+rwx,g+rwx,o-rwx";
    }
    else if (ztd::same(name, "r_r_r"))
    {
        cmd = "ugo+r,ugo-wx";
    }
    else if (ztd::same(name, "rw_r_r"))
    {
        cmd = "u+rw-x,go+r-wx";
    }
    else if (ztd::same(name, "rw_rw_rw"))
    {
        cmd = "ugo+rw-x";
    }
    else if (ztd::same(name, "rwxr_r"))
    {
        cmd = "u+rwx,go+r-wx";
    }
    else if (ztd::same(name, "rwxr_xr_x"))
    {
        cmd = "u+rwx,go+rx-w";
    }
    else if (ztd::same(name, "rwxrwxrwx"))
    {
        cmd = "ugo+rwx,-t";
    }
    else if (ztd::same(name, "rwxrwxrwt"))
    {
        cmd = "ugo+rwx,+t";
    }
    else if (ztd::same(name, "unstick"))
    {
        cmd = "-t";
    }
    else if (ztd::same(name, "stick"))
    {
        cmd = "+t";
    }
    else if (ztd::same(name, "go_w"))
    {
        cmd = "go-w";
    }
    else if (ztd::same(name, "go_rwx"))
    {
        cmd = "go-rwx";
    }
    else if (ztd::same(name, "ugo_w"))
    {
        cmd = "ugo+w";
    }
    else if (ztd::same(name, "ugo_rx"))
    {
        cmd = "ugo+rX";
    }
    else if (ztd::same(name, "ugo_rwx"))
    {
        cmd = "ugo+rwX";
    }
    else if (ztd::same(name, "myuser"))
    {
        cmd = std::format("{}:{}", myuser, myuser);
    }
    else if (ztd::same(name, "myuser_users"))
    {
        cmd = std::format("{}:users", myuser);
    }
    else if (ztd::same(name, "user1"))
    {
        cmd = std::format("{}:{}", user1, user1);
    }
    else if (ztd::same(name, "user1_users"))
    {
        cmd = std::format("{}:users", user1);
    }
    else if (ztd::same(name, "user2"))
    {
        cmd = std::format("{}:{}", user2, user2);
    }
    else if (ztd::same(name, "user2_users"))
    {
        cmd = std::format("{}:users", user2);
    }
    else if (ztd::same(name, "root"))
    {
        cmd = "root:root";
    }
    else if (ztd::same(name, "root_users"))
    {
        cmd = "root:users";
    }
    else if (ztd::same(name, "root_myuser"))
    {
        cmd = std::format("root:{}", myuser);
    }
    else if (ztd::same(name, "root_user1"))
    {
        cmd = std::format("root:{}", user1);
    }
    else if (ztd::same(name, "root_user2"))
    {
        cmd = std::format("root:{}", user2);
    }
    else
    {
        return;
    }

    std::string file_paths;
    for (vfs::file_info file : sel_files)
    {
        const std::string file_path = ztd::shell::quote(file->get_name());
        file_paths = std::format("{} {}", file_paths, file_path);
    }

    // task
    PtkFileTask* ptask = ptk_file_exec_new(set->menu_label.value(),
                                           cwd,
                                           GTK_WIDGET(file_browser),
                                           file_browser->task_view);
    ptask->task->exec_command = ztd::strdup(std::format("{} {} {}", prog, cmd, file_paths));
    ptask->task->exec_browser = file_browser;
    ptask->task->exec_sync = true;
    ptask->task->exec_show_error = true;
    ptask->task->exec_show_output = false;
    ptask->task->exec_export = false;
    if (as_root)
    {
        ptask->task->exec_as_user = "root";
    }
    ptk_file_task_run(ptask);
}

void
ptk_file_browser_on_action(PtkFileBrowser* browser, xset::name setname)
{
    i32 i = 0;
    xset_t set = xset_get(setname);
    MainWindow* main_window = MAIN_WINDOW(browser->main_window);
    const xset::main_window_panel mode = main_window->panel_context.at(browser->mypanel);

    // ztd::logger::info("ptk_file_browser_on_action {}", set->name);

    if (ztd::startswith(set->name, "book_"))
    {
        if (set->xset_name == xset::name::book_add)
        {
            const char* text = browser->path_bar && gtk_widget_has_focus(browser->path_bar)
                                   ? gtk_entry_get_text(GTK_ENTRY(browser->path_bar))
                                   : nullptr;
            if (text && (std::filesystem::exists(text) || ztd::contains(text, ":/") ||
                         ztd::startswith(text, "//")))
            {
                ptk_bookmark_view_add_bookmark(text);
            }
            else
            {
                ptk_bookmark_view_add_bookmark(browser);
            }
        }
    }
    else if (ztd::startswith(set->name, "go_"))
    {
        if (set->xset_name == xset::name::go_back)
        {
            ptk_file_browser_go_back(nullptr, browser);
        }
        else if (set->xset_name == xset::name::go_forward)
        {
            ptk_file_browser_go_forward(nullptr, browser);
        }
        else if (set->xset_name == xset::name::go_up)
        {
            ptk_file_browser_go_up(nullptr, browser);
        }
        else if (set->xset_name == xset::name::go_home)
        {
            ptk_file_browser_go_home(nullptr, browser);
        }
        else if (set->xset_name == xset::name::go_default)
        {
            ptk_file_browser_go_default(nullptr, browser);
        }
        else if (set->xset_name == xset::name::go_set_default)
        {
            ptk_file_browser_set_default_folder(nullptr, browser);
        }
    }
    else if (ztd::startswith(set->name, "tab_"))
    {
        if (set->xset_name == xset::name::tab_new)
        {
            ptk_file_browser_new_tab(nullptr, browser);
        }
        else if (set->xset_name == xset::name::tab_new_here)
        {
            ptk_file_browser_new_tab_here(nullptr, browser);
        }
        else
        {
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
                i = std::stoi(set->name);
            }
            ptk_file_browser_go_tab(nullptr, browser, i);
        }
    }
    else if (ztd::startswith(set->name, "focus_"))
    {
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
        ptk_file_browser_focus(nullptr, browser, i);
    }
    else if (set->xset_name == xset::name::view_reorder_col)
    {
        on_reorder(nullptr, GTK_WIDGET(browser));
    }
    else if (set->xset_name == xset::name::view_refresh)
    {
        ptk_file_browser_refresh(nullptr, browser);
    }
    else if (set->xset_name == xset::name::view_thumb)
    {
        main_window_toggle_thumbnails_all_windows();
    }
    else if (ztd::startswith(set->name, "sortby_"))
    {
        i = -3;
        if (set->xset_name == xset::name::sortby_name)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::name);
        }
        else if (set->xset_name == xset::name::sortby_size)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::size);
        }
        else if (set->xset_name == xset::name::sortby_type)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::type);
        }
        else if (set->xset_name == xset::name::sortby_perm)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::perm);
        }
        else if (set->xset_name == xset::name::sortby_owner)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::owner);
        }
        else if (set->xset_name == xset::name::sortby_date)
        {
            i = magic_enum::enum_integer(ptk::file_browser::sort_order::mtime);
        }
        else if (set->xset_name == xset::name::sortby_ascend)
        {
            i = -1;
            set->b = browser->sort_type == GtkSortType::GTK_SORT_ASCENDING ? xset::b::xtrue
                                                                           : xset::b::xfalse;
        }
        else if (set->xset_name == xset::name::sortby_descend)
        {
            i = -2;
            set->b = browser->sort_type == GtkSortType::GTK_SORT_DESCENDING ? xset::b::xtrue
                                                                            : xset::b::xfalse;
        }
        if (i > 0)
        {
            set->b = browser->sort_order == ptk::file_browser::sort_order(i) ? xset::b::xtrue
                                                                             : xset::b::xfalse;
        }
        on_popup_sortby(nullptr, browser, i);
    }
    else if (ztd::startswith(set->name, "sortx_"))
    {
        ptk_file_browser_set_sort_extra(browser, set->xset_name);
    }
    else if (ztd::startswith(set->name, "panel"))
    {
        const i32 panel_num = set->name[5];

        // ztd::logger::info("ACTION panelN={}  {}", panel_num, set->name[5]);

        if (i > 0 && i < 5)
        {
            xset_t set2;
            const std::string fullxname = std::format("panel{}_", panel_num);
            const std::string xname = ztd::removeprefix(set->name, fullxname);
            if (ztd::same(xname, "show_hidden")) // shared key
            {
                ptk_file_browser_show_hidden_files(
                    browser,
                    xset_get_b_panel(browser->mypanel, xset::panel::show_hidden));
            }
            else if (ztd::same(xname, "show"))
            { // main View|Panel N
                show_panels_all_windows(nullptr, MAIN_WINDOW(browser->main_window));
            }
            else if (ztd::startswith(xname, "show_")) // shared key
            {
                set2 = xset_get_panel_mode(browser->mypanel, xname, mode);
                set2->b = set2->b == xset::b::xtrue ? xset::b::unset : xset::b::xtrue;
                update_views_all_windows(nullptr, browser);
            }
            else if (ztd::same(xname, "list_detailed"))
            { // shared key
                on_popup_list_detailed(nullptr, browser);
            }
            else if (ztd::same(xname, "list_icons"))
            { // shared key
                on_popup_list_icons(nullptr, browser);
            }
            else if (ztd::same(xname, "list_compact"))
            { // shared key
                on_popup_list_compact(nullptr, browser);
            }
            else if (ztd::same(xname, "list_large")) // shared key
            {
                if (browser->view_mode != ptk::file_browser::view_mode::icon_view)
                {
                    xset_set_b_panel(browser->mypanel,
                                     xset::panel::list_large,
                                     !browser->large_icons);
                    on_popup_list_large(nullptr, browser);
                }
            }
            else if (ztd::startswith(xname, "detcol_") // shared key
                     && browser->view_mode == ptk::file_browser::view_mode::list_view)
            {
                set2 = xset_get_panel_mode(browser->mypanel, xname, mode);
                set2->b = set2->b == xset::b::xtrue ? xset::b::unset : xset::b::xtrue;
                update_views_all_windows(nullptr, browser);
            }
        }
    }
    else if (ztd::startswith(set->name, "status_"))
    {
        if (ztd::same(set->name, "status_border") || ztd::same(set->name, "status_text"))
        {
            on_status_effect_change(nullptr, browser);
        }
        else if (set->xset_name == xset::name::status_name ||
                 set->xset_name == xset::name::status_path ||
                 set->xset_name == xset::name::status_info ||
                 set->xset_name == xset::name::status_hide)
        {
            on_status_middle_click_config(nullptr, set);
        }
    }
    else if (ztd::startswith(set->name, "paste_"))
    {
        if (set->xset_name == xset::name::paste_link)
        {
            ptk_file_browser_paste_link(browser);
        }
        else if (set->xset_name == xset::name::paste_target)
        {
            ptk_file_browser_paste_target(browser);
        }
        else if (set->xset_name == xset::name::paste_as)
        {
            ptk_file_misc_paste_as(browser, ptk_file_browser_get_cwd(browser), nullptr);
        }
    }
    else if (ztd::startswith(set->name, "select_"))
    {
        if (set->xset_name == xset::name::select_all)
        {
            ptk_file_browser_select_all(nullptr, browser);
        }
        else if (set->xset_name == xset::name::select_un)
        {
            ptk_file_browser_unselect_all(nullptr, browser);
        }
        else if (set->xset_name == xset::name::select_invert)
        {
            ptk_file_browser_invert_selection(nullptr, browser);
        }
        else if (set->xset_name == xset::name::select_patt)
        {
            ptk_file_browser_select_pattern(nullptr, browser, nullptr);
        }
    }
    else // all the rest require ptkfilemenu data
    {
        ptk_file_menu_action(browser, set->name);
    }
}
