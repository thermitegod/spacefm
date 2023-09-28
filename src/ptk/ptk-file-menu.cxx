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

#include <fcntl.h>

#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"

#include "write.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-mime-monitor.hxx"

#include "ptk/ptk-dialog.hxx"

#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-archiver.hxx"
#include "ptk/ptk-bookmark-view.hxx"
#include "ptk/ptk-file-actions-open.hxx"
#include "ptk/ptk-file-actions-misc.hxx"
#include "ptk/ptk-file-actions-rename.hxx"
#include "ptk/ptk-file-properties.hxx"
#include "ptk/ptk-clipboard.hxx"
#include "ptk/ptk-task-view.hxx"

#include "settings/app.hxx"

#include "types.hxx"

#include "settings.hxx"
#include "main-window.hxx"

#include "ptk/ptk-file-list.hxx"

#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-keyboard.hxx"

#define PTK_FILE_MENU(obj) (static_cast<PtkFileMenu*>(obj))

static bool on_app_button_press(GtkWidget* item, GdkEventButton* event, PtkFileMenu* data);
static bool app_menu_keypress(GtkWidget* widget, GdkEventKey* event, PtkFileMenu* data);
static void show_app_menu(GtkWidget* menu, GtkWidget* app_item, PtkFileMenu* data, u32 button,
                          std::time_t time);

/* Signal handlers for popup menu */
static void on_popup_open_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_open_with_another_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_run_app(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_open_in_new_tab_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_new_bookmark(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_cut_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_copy_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_paste_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_paste_link_activate(GtkMenuItem* menuitem, PtkFileMenu* data);   // MOD added
static void on_popup_paste_target_activate(GtkMenuItem* menuitem, PtkFileMenu* data); // MOD added
static void on_popup_copy_text_activate(GtkMenuItem* menuitem, PtkFileMenu* data);    // MOD added
static void on_popup_copy_name_activate(GtkMenuItem* menuitem, PtkFileMenu* data);    // MOD added
static void on_popup_copy_parent_activate(GtkMenuItem* menuitem, PtkFileMenu* data);  // MOD added

static void on_popup_paste_as_activate(GtkMenuItem* menuitem, PtkFileMenu* data); // sfm added

static void on_popup_trash_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_delete_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_rename_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_compress_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_extract_here_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_extract_to_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_extract_open_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_new_folder_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_new_text_file_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_new_link_activate(GtkMenuItem* menuitem, PtkFileMenu* data);

static void on_popup_file_properties_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_file_attributes_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_file_permissions_activate(GtkMenuItem* menuitem, PtkFileMenu* data);

static void on_popup_open_all(GtkMenuItem* menuitem, PtkFileMenu* data);

static void on_popup_canon(GtkMenuItem* menuitem, PtkFileMenu* data);

PtkFileMenu::~PtkFileMenu()
{
    if (this->accel_group)
    {
        g_object_unref(this->accel_group);
    }
}

void
on_popup_list_large(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    const panel_t p = browser->panel();
    const MainWindow* main_window = browser->main_window();
    const xset::main_window_panel mode = main_window->panel_context.at(p);

    xset_set_b_panel_mode(p,
                          xset::panel::list_large,
                          mode,
                          xset_get_b_panel(p, xset::panel::list_large));
    update_views_all_windows(nullptr, browser);
}

void
on_popup_list_detailed(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    const panel_t p = browser->panel();

    if (xset_get_b_panel(p, xset::panel::list_detailed))
    {
        // setting b to xset::b::XSET_B_UNSET does not work here
        xset_set_b_panel(p, xset::panel::list_icons, false);
        xset_set_b_panel(p, xset::panel::list_compact, false);
    }
    else
    {
        if (!xset_get_b_panel(p, xset::panel::list_icons) &&
            !xset_get_b_panel(p, xset::panel::list_compact))
        {
            xset_set_b_panel(p, xset::panel::list_icons, true);
        }
    }
    update_views_all_windows(nullptr, browser);
}

void
on_popup_list_icons(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    const panel_t p = browser->panel();

    if (xset_get_b_panel(p, xset::panel::list_icons))
    {
        // setting b to xset::b::XSET_B_UNSET does not work here
        xset_set_b_panel(p, xset::panel::list_detailed, false);
        xset_set_b_panel(p, xset::panel::list_compact, false);
    }
    else
    {
        if (!xset_get_b_panel(p, xset::panel::list_detailed) &&
            !xset_get_b_panel(p, xset::panel::list_compact))
        {
            xset_set_b_panel(p, xset::panel::list_detailed, true);
        }
    }
    update_views_all_windows(nullptr, browser);
}

void
on_popup_list_compact(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    const panel_t p = browser->panel();

    if (xset_get_b_panel(p, xset::panel::list_compact))
    {
        // setting b to xset::b::XSET_B_UNSET does not work here
        xset_set_b_panel(p, xset::panel::list_detailed, false);
        xset_set_b_panel(p, xset::panel::list_icons, false);
    }
    else
    {
        if (!xset_get_b_panel(p, xset::panel::list_icons) &&
            !xset_get_b_panel(p, xset::panel::list_detailed))
        {
            xset_set_b_panel(p, xset::panel::list_detailed, true);
        }
    }
    update_views_all_windows(nullptr, browser);
}

static void
on_popup_show_hidden(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    if (browser)
    {
        browser->show_hidden_files(xset_get_b_panel(browser->panel(), xset::panel::show_hidden));
    }
}

static void
on_copycmd(GtkMenuItem* menuitem, PtkFileMenu* data, xset_t set2)
{
    xset_t set;
    if (menuitem)
    {
        set = xset_get(static_cast<const char*>(g_object_get_data(G_OBJECT(menuitem), "set")));
    }
    else
    {
        set = set2;
    }
    if (!set)
    {
        return;
    }
    if (data->browser)
    {
        data->browser->copycmd(data->sel_files, data->cwd, set->xset_name);
    }
}

static void
on_popup_select_pattern(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->select_pattern();
    }
}

static void
on_open_in_tab(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    const tab_t tab = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "tab"));
    if (data->browser)
    {
        data->browser->open_in_tab(data->file_path, tab);
    }
}

static void
on_open_in_panel(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    const panel_t panel_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "panel"));
    if (data->browser)
    {
        main_window_open_in_panel(data->browser, panel_num, data->file_path);
    }
}

static void
on_file_edit(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    xset_edit(GTK_WIDGET(data->browser), data->file_path);
}

static void
on_popup_sort_extra(GtkMenuItem* menuitem, PtkFileBrowser* file_browser, xset_t set2)
{
    xset_t set;
    if (menuitem)
    {
        set = xset_get(static_cast<const char*>(g_object_get_data(G_OBJECT(menuitem), "set")));
    }
    else
    {
        set = set2;
    }
    file_browser->set_sort_extra(set->xset_name);
}

void
on_popup_sortby(GtkMenuItem* menuitem, PtkFileBrowser* file_browser, i32 order)
{
    i32 v;

    i32 sort_order;
    if (menuitem)
    {
        sort_order = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "sortorder"));
    }
    else
    {
        sort_order = order;
    }

    if (sort_order < 0)
    {
        if (sort_order == -1)
        {
            v = GtkSortType::GTK_SORT_ASCENDING;
        }
        else
        {
            v = GtkSortType::GTK_SORT_DESCENDING;
        }
        xset_set_panel(file_browser->panel(),
                       xset::panel::list_detailed,
                       xset::var::y,
                       std::to_string(v));
        file_browser->set_sort_type((GtkSortType)v);
    }
    else
    {
        xset_set_panel(file_browser->panel(),
                       xset::panel::list_detailed,
                       xset::var::x,
                       std::to_string(sort_order));
        file_browser->set_sort_order((ptk::file_browser::sort_order)sort_order);
    }
}

static void
on_popup_detailed_column(GtkMenuItem* menuitem, PtkFileBrowser* file_browser)
{
    (void)menuitem;
    if (file_browser->is_view_mode(ptk::file_browser::view_mode::list_view))
    {
        // get visiblity for correct mode
        const MainWindow* main_window = file_browser->main_window();
        const panel_t p = file_browser->panel();
        const xset::main_window_panel mode = main_window->panel_context.at(p);

        xset_t set;

        // size
        set = xset_get_panel_mode(p, xset::panel::detcol_size, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_size)->b;
        // size in bytes
        set = xset_get_panel_mode(p, xset::panel::detcol_bytes, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_bytes)->b;
        // type
        set = xset_get_panel_mode(p, xset::panel::detcol_type, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_type)->b;
        // MIME type
        set = xset_get_panel_mode(p, xset::panel::detcol_mime, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_mime)->b;
        // perm
        set = xset_get_panel_mode(p, xset::panel::detcol_perm, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_perm)->b;
        // owner
        set = xset_get_panel_mode(p, xset::panel::detcol_owner, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_owner)->b;
        // group
        set = xset_get_panel_mode(p, xset::panel::detcol_group, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_group)->b;
        // atime
        set = xset_get_panel_mode(p, xset::panel::detcol_atime, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_atime)->b;
        // btime
        set = xset_get_panel_mode(p, xset::panel::detcol_btime, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_btime)->b;
        // ctime
        set = xset_get_panel_mode(p, xset::panel::detcol_ctime, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_ctime)->b;
        // mtime
        set = xset_get_panel_mode(p, xset::panel::detcol_mtime, mode);
        set->b = xset_get_panel(p, xset::panel::detcol_mtime)->b;

        update_views_all_windows(nullptr, file_browser);
    }
}

static void
on_popup_toggle_view(GtkMenuItem* menuitem, PtkFileBrowser* file_browser)
{
    (void)menuitem;
    // get visiblity for correct mode
    const MainWindow* main_window = file_browser->main_window();
    const panel_t p = file_browser->panel();
    const xset::main_window_panel mode = main_window->panel_context.at(p);

    xset_t set;
    set = xset_get_panel_mode(p, xset::panel::show_toolbox, mode);
    set->b = xset_get_panel(p, xset::panel::show_toolbox)->b;
    set = xset_get_panel_mode(p, xset::panel::show_devmon, mode);
    set->b = xset_get_panel(p, xset::panel::show_devmon)->b;
    set = xset_get_panel_mode(p, xset::panel::show_dirtree, mode);
    set->b = xset_get_panel(p, xset::panel::show_dirtree)->b;
    set = xset_get_panel_mode(p, xset::panel::show_sidebar, mode);
    set->b = xset_get_panel(p, xset::panel::show_sidebar)->b;

    update_views_all_windows(nullptr, file_browser);
}

static void
on_archive_default(GtkMenuItem* menuitem, xset_t set)
{
    (void)menuitem;
    static constexpr std::array<xset::name, 4> arcnames{
        xset::name::archive_default_open_with_app,
        xset::name::archive_default_extract,
        xset::name::archive_default_extract_to,
        xset::name::archive_default_open_with_archiver,
    };

    for (const xset::name arcname : arcnames)
    {
        if (set->xset_name == arcname)
        {
            set->b = xset::b::xtrue;
        }
        else
        {
            xset_set_b(arcname, false);
        }
    }
}

static void
on_hide_file(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->hide_selected(data->sel_files, data->cwd);
    }
}

static void
on_permission(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    if (data->browser)
    {
        data->browser->on_permission(menuitem, data->sel_files, data->cwd);
    }
}

void
ptk_file_menu_add_panel_view_menu(PtkFileBrowser* browser, GtkWidget* menu,
                                  GtkAccelGroup* accel_group)
{
    xset_t set;
    xset_t set_radio;
    std::string desc;

    std::vector<xset::name> context_menu_entries;

    if (!browser || !menu || !browser->file_list_)
    {
        return;
    }
    const panel_t p = browser->panel();

    const MainWindow* main_window = browser->main_window();
    const xset::main_window_panel mode = main_window->panel_context.at(p);

    bool show_side = false;
    xset_set_cb(xset::name::view_refresh, (GFunc)ptk_file_browser_refresh, browser);
    set = xset_get_panel(p, xset::panel::show_toolbox);
    xset_set_cb(set, (GFunc)on_popup_toggle_view, browser);
    set->b = xset_get_panel_mode(p, xset::panel::show_toolbox, mode)->b;
    set = xset_get_panel(p, xset::panel::show_devmon);
    xset_set_cb(set, (GFunc)on_popup_toggle_view, browser);
    set->b = xset_get_panel_mode(p, xset::panel::show_devmon, mode)->b;
    if (set->b == xset::b::xtrue)
    {
        show_side = true;
    }
    set = xset_get_panel(p, xset::panel::show_dirtree);
    xset_set_cb(set, (GFunc)on_popup_toggle_view, browser);
    set->b = xset_get_panel_mode(p, xset::panel::show_dirtree, mode)->b;
    if (set->b == xset::b::xtrue)
    {
        show_side = true;
    }
    set = xset_get_panel(p, xset::panel::show_sidebar);
    xset_set_cb(set, (GFunc)on_popup_toggle_view, browser);
    set->b = xset_get_panel_mode(p, xset::panel::show_sidebar, mode)->b;
    set->disable = !show_side;
    xset_set_cb_panel(p, xset::panel::show_hidden, (GFunc)on_popup_show_hidden, browser);

    if (browser->is_view_mode(ptk::file_browser::view_mode::list_view))
    {
        // size
        set = xset_get_panel(p, xset::panel::detcol_size);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_size, mode)->b;
        // size in bytes
        set = xset_get_panel(p, xset::panel::detcol_bytes);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_bytes, mode)->b;
        // type
        set = xset_get_panel(p, xset::panel::detcol_type);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_type, mode)->b;
        // MIME type
        set = xset_get_panel(p, xset::panel::detcol_mime);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_mime, mode)->b;
        // perm
        set = xset_get_panel(p, xset::panel::detcol_perm);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_perm, mode)->b;
        // owner
        set = xset_get_panel(p, xset::panel::detcol_owner);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_owner, mode)->b;
        // group
        set = xset_get_panel(p, xset::panel::detcol_group);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_group, mode)->b;
        // atime
        set = xset_get_panel(p, xset::panel::detcol_atime);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_atime, mode)->b;
        // btime
        set = xset_get_panel(p, xset::panel::detcol_btime);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_btime, mode)->b;
        // ctime
        set = xset_get_panel(p, xset::panel::detcol_ctime);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_ctime, mode)->b;
        // mtime
        set = xset_get_panel(p, xset::panel::detcol_mtime);
        xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, xset::panel::detcol_mtime, mode)->b;

        xset_set_cb(xset::name::view_reorder_col, (GFunc)on_reorder, browser);

        set = xset_get(xset::name::view_columns);
        xset_set_var(set, xset::var::disable, "0");
        if (p == panel_1)
        {
            context_menu_entries = {
                xset::name::panel1_detcol_size,
                xset::name::panel1_detcol_bytes,
                xset::name::panel1_detcol_type,
                xset::name::panel1_detcol_mime,
                xset::name::panel1_detcol_perm,
                xset::name::panel1_detcol_owner,
                xset::name::panel1_detcol_group,
                xset::name::panel1_detcol_atime,
                xset::name::panel1_detcol_btime,
                xset::name::panel1_detcol_ctime,
                xset::name::panel1_detcol_mtime,
                xset::name::separator,
                xset::name::view_reorder_col,
            };
        }
        else if (p == panel_2)
        {
            context_menu_entries = {
                xset::name::panel2_detcol_size,
                xset::name::panel2_detcol_bytes,
                xset::name::panel2_detcol_type,
                xset::name::panel2_detcol_mime,
                xset::name::panel2_detcol_perm,
                xset::name::panel2_detcol_owner,
                xset::name::panel2_detcol_group,
                xset::name::panel2_detcol_atime,
                xset::name::panel2_detcol_btime,
                xset::name::panel2_detcol_ctime,
                xset::name::panel2_detcol_mtime,
                xset::name::separator,
                xset::name::view_reorder_col,
            };
        }
        else if (p == panel_3)
        {
            context_menu_entries = {
                xset::name::panel3_detcol_size,
                xset::name::panel3_detcol_bytes,
                xset::name::panel3_detcol_type,
                xset::name::panel3_detcol_mime,
                xset::name::panel3_detcol_perm,
                xset::name::panel3_detcol_owner,
                xset::name::panel3_detcol_group,
                xset::name::panel3_detcol_atime,
                xset::name::panel3_detcol_btime,
                xset::name::panel3_detcol_ctime,
                xset::name::panel3_detcol_mtime,
                xset::name::separator,
                xset::name::view_reorder_col,
            };
        }
        else if (p == panel_4)
        {
            context_menu_entries = {
                xset::name::panel4_detcol_size,
                xset::name::panel4_detcol_bytes,
                xset::name::panel4_detcol_type,
                xset::name::panel4_detcol_mime,
                xset::name::panel4_detcol_perm,
                xset::name::panel4_detcol_owner,
                xset::name::panel4_detcol_group,
                xset::name::panel4_detcol_atime,
                xset::name::panel4_detcol_btime,
                xset::name::panel4_detcol_ctime,
                xset::name::panel4_detcol_mtime,
                xset::name::separator,
                xset::name::view_reorder_col,
            };
        }
        xset_set_submenu(set, context_menu_entries);

        set = xset_get(xset::name::rubberband);
        xset_set_cb(set, (GFunc)main_window_rubberband_all, nullptr);
        set->disable = false;
    }
    else
    {
        xset_set(xset::name::view_columns, xset::var::disable, "1");
        xset_set(xset::name::rubberband, xset::var::disable, "1");
    }

    set = xset_get(xset::name::view_thumb);
    xset_set_cb(set, (GFunc)main_window_toggle_thumbnails_all_windows, nullptr);
    set->b = app_settings.show_thumbnail() ? xset::b::xtrue : xset::b::unset;

    if (browser->is_view_mode(ptk::file_browser::view_mode::icon_view))
    {
        set = xset_get_panel(p, xset::panel::list_large);
        xset_set_b(set, true);
        set->disable = true;
    }
    else
    {
        set = xset_get_panel(p, xset::panel::list_large);
        xset_set_cb(set, (GFunc)on_popup_list_large, browser);
        set->disable = false;
        set->b = xset_get_panel_mode(p, xset::panel::list_large, mode)->b;
    }

    set = xset_get_panel(p, xset::panel::list_detailed);
    xset_set_cb(set, (GFunc)on_popup_list_detailed, browser);
    xset_set_ob2(set, nullptr, nullptr);
    set_radio = set;
    set = xset_get_panel(p, xset::panel::list_icons);
    xset_set_cb(set, (GFunc)on_popup_list_icons, browser);
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set = xset_get_panel(p, xset::panel::list_compact);
    xset_set_cb(set, (GFunc)on_popup_list_compact, browser);
    xset_set_ob2(set, nullptr, set_radio->name.data());

    // name
    set = xset_get(xset::name::sortby_name);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::name));
    xset_set_ob2(set, nullptr, nullptr);
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::name) ? xset::b::xtrue
                                                                         : xset::b::xfalse;
    set_radio = set;
    // size
    set = xset_get(xset::name::sortby_size);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::size));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::size) ? xset::b::xtrue
                                                                         : xset::b::xfalse;
    // size in bytes
    set = xset_get(xset::name::sortby_bytes);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::bytes));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::bytes) ? xset::b::xtrue
                                                                          : xset::b::xfalse;
    // type
    set = xset_get(xset::name::sortby_type);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::type));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::type) ? xset::b::xtrue
                                                                         : xset::b::xfalse;
    // MIME type
    set = xset_get(xset::name::sortby_mime);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::mime));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::mime) ? xset::b::xtrue
                                                                         : xset::b::xfalse;
    // perm
    set = xset_get(xset::name::sortby_perm);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::perm));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::perm) ? xset::b::xtrue
                                                                         : xset::b::xfalse;
    // owner
    set = xset_get(xset::name::sortby_owner);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::owner));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::owner) ? xset::b::xtrue
                                                                          : xset::b::xfalse;
    // group
    set = xset_get(xset::name::sortby_group);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::group));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::group) ? xset::b::xtrue
                                                                          : xset::b::xfalse;
    // atime
    set = xset_get(xset::name::sortby_atime);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::atime));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::atime) ? xset::b::xtrue
                                                                          : xset::b::xfalse;
    // btime
    set = xset_get(xset::name::sortby_btime);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::btime));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::btime) ? xset::b::xtrue
                                                                          : xset::b::xfalse;
    // ctime
    set = xset_get(xset::name::sortby_ctime);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::ctime));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::ctime) ? xset::b::xtrue
                                                                          : xset::b::xfalse;
    // mtime
    set = xset_get(xset::name::sortby_mtime);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set,
                     "sortorder",
                     magic_enum::enum_integer(ptk::file_browser::sort_order::mtime));
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b = browser->is_sort_order(ptk::file_browser::sort_order::mtime) ? xset::b::xtrue
                                                                          : xset::b::xfalse;

    set = xset_get(xset::name::sortby_ascend);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", -1);
    xset_set_ob2(set, nullptr, nullptr);
    set->b =
        browser->is_sort_type(GtkSortType::GTK_SORT_ASCENDING) ? xset::b::xtrue : xset::b::xfalse;
    set_radio = set;
    set = xset_get(xset::name::sortby_descend);
    xset_set_cb(set, (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", -2);
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set->b =
        browser->is_sort_type(GtkSortType::GTK_SORT_DESCENDING) ? xset::b::xtrue : xset::b::xfalse;

    // this crashes if !browser->file_list so do not allow
    if (browser->file_list_)
    {
        set = xset_get(xset::name::sortx_alphanum);
        xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
        set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_alphanum ? xset::b::xtrue
                                                                               : xset::b::xfalse;
        set = xset_get(xset::name::sortx_case);
        xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
        set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_case ? xset::b::xtrue
                                                                           : xset::b::xfalse;
        set->disable = !PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_alphanum;

#if 0
        set = xset_get(xset::name::sortx_natural);
        xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
        set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list)->sort_natural ? xset::b::XSET_B_TRUE : xset::b::XSET_B_FALSE;
        set = xset_get(xset::name::SORTX_CASE);
        xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
        set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list)->sort_case ? xset::b::XSET_B_TRUE : xset::b::XSET_B_FALSE;
        set->disable = !PTK_FILE_LIST_REINTERPRET(browser->file_list)->sort_natural;
#endif

        set = xset_get(xset::name::sortx_directories);
        xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, nullptr);
        set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_dir ==
                         ptk::file_list::sort_dir::first
                     ? xset::b::xtrue
                     : xset::b::xfalse;
        set_radio = set;
        set = xset_get(xset::name::sortx_files);
        xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, set_radio->name.data());
        set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_dir ==
                         ptk::file_list::sort_dir::last
                     ? xset::b::xtrue
                     : xset::b::xfalse;
        set = xset_get(xset::name::sortx_mix);
        xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, set_radio->name.data());
        set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_dir ==
                         ptk::file_list::sort_dir::mixed
                     ? xset::b::xtrue
                     : xset::b::xfalse;

        set = xset_get(xset::name::sortx_hidfirst);
        xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, nullptr);
        set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_hidden_first
                     ? xset::b::xtrue
                     : xset::b::xfalse;
        set_radio = set;
        set = xset_get(xset::name::sortx_hidlast);
        xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, set_radio->name.data());
        set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_hidden_first ? xset::b::xfalse
                                                                                   : xset::b::xtrue;
    }

    set = xset_get(xset::name::view_list_style);
    if (p == panel_1)
    {
        context_menu_entries = {
            xset::name::panel1_list_detailed,
            xset::name::panel1_list_compact,
            xset::name::panel1_list_icons,
            xset::name::separator,
            xset::name::view_thumb,
            xset::name::panel1_list_large,
            xset::name::rubberband,
        };
    }
    else if (p == panel_2)
    {
        context_menu_entries = {
            xset::name::panel2_list_detailed,
            xset::name::panel2_list_compact,
            xset::name::panel2_list_icons,
            xset::name::separator,
            xset::name::view_thumb,
            xset::name::panel2_list_large,
            xset::name::rubberband,
        };
    }
    else if (p == panel_3)
    {
        context_menu_entries = {
            xset::name::panel3_list_detailed,
            xset::name::panel3_list_compact,
            xset::name::panel3_list_icons,
            xset::name::separator,
            xset::name::view_thumb,
            xset::name::panel3_list_large,
            xset::name::rubberband,
        };
    }
    else if (p == panel_4)
    {
        context_menu_entries = {
            xset::name::panel4_list_detailed,
            xset::name::panel4_list_compact,
            xset::name::panel4_list_icons,
            xset::name::separator,
            xset::name::view_thumb,
            xset::name::panel4_list_large,
            xset::name::rubberband,
        };
    }
    xset_set_submenu(set, context_menu_entries);

    set = xset_get(xset::name::con_view);
    set->disable = !browser->file_list_;
    if (p == panel_1)
    {
        context_menu_entries = {
            xset::name::panel1_show_toolbox,
            xset::name::panel1_show_sidebar,
            xset::name::panel1_show_devmon,
            xset::name::panel1_show_dirtree,
            xset::name::separator,
            xset::name::panel1_show_hidden,
            xset::name::view_list_style,
            xset::name::view_sortby,
            xset::name::view_columns,
            xset::name::separator,
            xset::name::view_refresh,
        };
    }
    else if (p == panel_2)
    {
        context_menu_entries = {
            xset::name::panel2_show_toolbox,
            xset::name::panel2_show_sidebar,
            xset::name::panel2_show_devmon,
            xset::name::panel2_show_dirtree,
            xset::name::separator,
            xset::name::panel2_show_hidden,
            xset::name::view_list_style,
            xset::name::view_sortby,
            xset::name::view_columns,
            xset::name::separator,
            xset::name::view_refresh,
        };
    }
    else if (p == panel_3)
    {
        context_menu_entries = {
            xset::name::panel3_show_toolbox,
            xset::name::panel3_show_sidebar,
            xset::name::panel3_show_devmon,
            xset::name::panel3_show_dirtree,
            xset::name::separator,
            xset::name::panel3_show_hidden,
            xset::name::view_list_style,
            xset::name::view_sortby,
            xset::name::view_columns,
            xset::name::separator,
            xset::name::view_refresh,
        };
    }
    else if (p == panel_4)
    {
        context_menu_entries = {
            xset::name::panel4_show_toolbox,
            xset::name::panel4_show_sidebar,
            xset::name::panel4_show_devmon,
            xset::name::panel4_show_dirtree,
            xset::name::separator,
            xset::name::panel4_show_hidden,
            xset::name::view_list_style,
            xset::name::view_sortby,
            xset::name::view_columns,
            xset::name::separator,
            xset::name::view_refresh,
        };
    }
    xset_set_submenu(set, context_menu_entries);

    xset_add_menuitem(browser, menu, accel_group, set);
}

static void
ptk_file_menu_free(PtkFileMenu* data)
{
    delete data;
}

/* Retrieve popup menu for selected file(s) */
GtkWidget*
ptk_file_menu_new(PtkFileBrowser* browser)
{
    return ptk_file_menu_new(browser, {});
}

GtkWidget*
ptk_file_menu_new(PtkFileBrowser* browser, const std::span<const vfs::file_info> sel_files)
{
    assert(browser != nullptr);

    std::filesystem::path file_path;
    vfs::file_info file = nullptr;
    if (!sel_files.empty())
    {
        file = sel_files.front();
        file_path = file->path();
    }

    const auto cwd = browser->cwd();

    const auto data = new PtkFileMenu;
    data->cwd = cwd;
    data->browser = browser;
    data->file_path = file_path;
    data->file = file;
    data->sel_files = std::vector<vfs::file_info>(sel_files.begin(), sel_files.end());
    data->accel_group = gtk_accel_group_new();

    GtkWidget* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    g_object_weak_ref(G_OBJECT(popup), (GWeakNotify)ptk_file_menu_free, data);
    g_signal_connect_after((void*)popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);

    const bool is_dir = (file && file->is_directory());
    // Note: network filesystems may become unresponsive here
    const bool is_text = file && file->is_text();

    // test R/W access to cwd instead of selected file
    // Note: network filesystems may become unresponsive here
    // const i32 no_read_access = faccessat(0, cwd, R_OK, AT_EACCESS);
    const i32 no_write_access = faccessat(0, cwd.c_str(), W_OK, AT_EACCESS);

    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    bool is_clip;
    if (!gtk_clipboard_wait_is_target_available(
            clip,
            gdk_atom_intern("x-special/gnome-copied-files", false)) &&
        !gtk_clipboard_wait_is_target_available(clip, gdk_atom_intern("text/uri-list", false)))
    {
        is_clip = false;
    }
    else
    {
        is_clip = true;
    }

    const panel_t p = browser->panel();

    const auto counts = main_window_get_counts(browser);
    const panel_t panel_count = counts.panel_count;
    const tab_t tab_count = counts.tab_count;
    const tab_t tab_num = counts.tab_num;

    // Get mime type and apps
    vfs::mime_type mime_type = nullptr;
    std::vector<std::string> apps{};
    if (file)
    {
        mime_type = file->mime_type();
        apps = mime_type->actions();
    }

    xset_t set_radio;
    xset_t set;
    xset_t set2;
    GtkMenuItem* item;

    // Open >
    const bool set_disable = sel_files.empty();

    set = xset_get(xset::name::con_open);
    set->disable = set_disable;
    item = GTK_MENU_ITEM(xset_add_menuitem(browser, popup, accel_group, set));
    if (!sel_files.empty())
    {
        GtkWidget* submenu = gtk_menu_item_get_submenu(item);

        // Execute
        if (!is_dir && file &&
            (file->flags() & vfs::file_info_flags::desktop_entry ||
             // Note: network filesystems may become unresponsive here
             file->is_executable()))
        {
            set = xset_get(xset::name::open_execute);
            xset_set_cb(set, (GFunc)on_popup_open_activate, data);
            xset_add_menuitem(browser, submenu, accel_group, set);
        }

        // Prepare archive commands
        xset_t set_archive_extract = nullptr;
        xset_t set_archive_extract_to = nullptr;
        xset_t set_archive_open = nullptr;

        const auto is_archive = [](const vfs::file_info file) { return file->is_archive(); };
        if (mime_type && std::ranges::all_of(sel_files, is_archive))
        {
            set_archive_extract = xset_get(xset::name::archive_extract);
            xset_set_cb(set_archive_extract, (GFunc)on_popup_extract_here_activate, data);
            set_archive_extract->disable = no_write_access;

            set_archive_extract_to = xset_get(xset::name::archive_extract_to);
            xset_set_cb(set_archive_extract_to, (GFunc)on_popup_extract_to_activate, data);

            set_archive_open = xset_get(xset::name::archive_open);
            xset_set_cb(set_archive_open, (GFunc)on_popup_extract_open_activate, data);

            set = xset_get(xset::name::archive_default_open_with_app);
            // do NOT use set = xset_set_cb here or wrong set is passed
            xset_set_cb(xset::name::archive_default_open_with_app, (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, nullptr);
            set_radio = set;

            set = xset_get(xset::name::archive_default_extract);
            xset_set_cb(xset::name::archive_default_extract, (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio->name.data());

            set = xset_get(xset::name::archive_default_extract_to);
            xset_set_cb(xset::name::archive_default_extract_to, (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio->name.data());

            set = xset_get(xset::name::archive_default_open_with_archiver);
            xset_set_cb(xset::name::archive_default_open_with_archiver,
                        (GFunc)on_archive_default,
                        set);
            xset_set_ob2(set, nullptr, set_radio->name.data());

            if (!xset_get_b(xset::name::archive_default_open_with_app))
            {
                // archives are not set to open with app, so list archive
                // functions before file handlers and associated apps

                // list active function first
                if (xset_get_b(xset::name::archive_default_extract))
                {
                    xset_add_menuitem(browser, submenu, accel_group, set_archive_extract);
                    set_archive_extract = nullptr;
                }
                else if (xset_get_b(xset::name::archive_default_extract_to))
                {
                    xset_add_menuitem(browser, submenu, accel_group, set_archive_extract_to);
                    set_archive_extract_to = nullptr;
                }
                else
                {
                    xset_add_menuitem(browser, submenu, accel_group, set_archive_open);
                    set_archive_open = nullptr;
                }

                // add others
                if (set_archive_extract)
                {
                    xset_add_menuitem(browser, submenu, accel_group, set_archive_extract);
                }
                if (set_archive_extract_to)
                {
                    xset_add_menuitem(browser, submenu, accel_group, set_archive_extract_to);
                }
                if (set_archive_open)
                {
                    xset_add_menuitem(browser, submenu, accel_group, set_archive_open);
                }
                xset_add_menuitem(browser,
                                  submenu,
                                  accel_group,
                                  xset_get(xset::name::archive_default));
                set_archive_extract = nullptr;

                // separator
                item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
                gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));
            }
        }

        GtkWidget* app_menu_item;

        i32 icon_w;
        i32 icon_h;

        // add apps
        gtk_icon_size_lookup(GtkIconSize::GTK_ICON_SIZE_MENU, &icon_w, &icon_h);
        if (is_text)
        {
            vfs::mime_type txt_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_PLAIN_TEXT);
            const std::vector<std::string> txt_apps = txt_type->actions();
            if (!txt_apps.empty())
            {
                apps = ztd::merge(apps, txt_apps);
            }
        }
        if (!apps.empty())
        {
            for (const std::string_view app : apps)
            {
                const vfs::desktop desktop = vfs_get_desktop(app);
                const auto app_name = desktop->display_name();
                if (!app_name.empty())
                {
                    app_menu_item = gtk_menu_item_new_with_label(app_name.data());
                }
                else
                {
                    app_menu_item = gtk_menu_item_new_with_label(app.data());
                }

                gtk_container_add(GTK_CONTAINER(submenu), app_menu_item);
                g_signal_connect(G_OBJECT(app_menu_item),
                                 "activate",
                                 G_CALLBACK(on_popup_run_app),
                                 (void*)data);
                g_object_set_data(G_OBJECT(app_menu_item), "menu", submenu);
                g_signal_connect(G_OBJECT(app_menu_item),
                                 "button-press-event",
                                 G_CALLBACK(on_app_button_press),
                                 (void*)data);
                g_signal_connect(G_OBJECT(app_menu_item),
                                 "button-release-event",
                                 G_CALLBACK(on_app_button_press),
                                 (void*)data);
                g_object_set_data_full(G_OBJECT(app_menu_item),
                                       "desktop_file",
                                       ztd::strdup(app.data()),
                                       free);
            }
        }

        // Edit / Dir
        if ((is_dir && browser) || (is_text && sel_files.size() == 1))
        {
            item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));

            if (is_text)
            {
                // Edit
                set = xset_get(xset::name::open_edit);
                xset_set_cb(set, (GFunc)on_file_edit, data);
                xset_add_menuitem(browser, submenu, accel_group, set);
            }
            else if (browser && is_dir)
            {
                // Open Dir
                set = xset_get(xset::name::opentab_prev);
                xset_set_cb(set, (GFunc)on_open_in_tab, data);
                xset_set_ob1_int(set, "tab", tab_control_code_prev);
                set->disable = (tab_num == 1);
                set = xset_get(xset::name::opentab_next);
                xset_set_cb(set, (GFunc)on_open_in_tab, data);
                xset_set_ob1_int(set, "tab", tab_control_code_next);
                set->disable = (tab_num == tab_count);
                set = xset_get(xset::name::opentab_new);
                xset_set_cb(set, (GFunc)on_popup_open_in_new_tab_activate, data);
                for (tab_t tab : TABS)
                {
                    const std::string name = std::format("opentab_{}", tab);
                    set = xset_get(name);
                    xset_set_cb(set, (GFunc)on_open_in_tab, data);
                    xset_set_ob1_int(set, "tab", tab);
                    set->disable = (tab > tab_count) || (tab == tab_num);
                }

                set = xset_get(xset::name::open_in_panelprev);
                xset_set_cb(set, (GFunc)on_open_in_panel, data);
                xset_set_ob1_int(set, "panel", panel_control_code_prev);
                set->disable = (panel_count == 1);
                set = xset_get(xset::name::open_in_panelnext);
                xset_set_cb(set, (GFunc)on_open_in_panel, data);
                xset_set_ob1_int(set, "panel", panel_control_code_next);
                set->disable = (panel_count == 1);

                for (panel_t panel : PANELS)
                {
                    const std::string name = std::format("open_in_panel{}", panel);
                    set = xset_get(name);
                    xset_set_cb(set, (GFunc)on_open_in_panel, data);
                    xset_set_ob1_int(set, "panel", panel);
                    // set->disable = ( p == i );
                }

                set = xset_get(xset::name::open_in_tab);
                xset_add_menuitem(browser, submenu, accel_group, set);
                set = xset_get(xset::name::open_in_panel);
                xset_add_menuitem(browser, submenu, accel_group, set);
            }
        }

        if (set_archive_extract)
        {
            // archives are set to open with app, so list archive
            // functions after associated apps

            // separator
            item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));

            xset_add_menuitem(browser, submenu, accel_group, set_archive_extract);
            xset_add_menuitem(browser, submenu, accel_group, set_archive_extract_to);
            xset_add_menuitem(browser, submenu, accel_group, set_archive_open);
            xset_add_menuitem(browser, submenu, accel_group, xset_get(xset::name::archive_default));
        }

        // Choose, open with other app
        item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));

        set = xset_get(xset::name::open_other);
        xset_set_cb(set, (GFunc)on_popup_open_with_another_activate, data);
        xset_add_menuitem(browser, submenu, accel_group, set);

        // Default
        std::string plain_type;
        if (mime_type)
        {
            plain_type = mime_type->type().data();
        }
        plain_type = ztd::replace(plain_type, "-", "_");
        plain_type = ztd::replace(plain_type, " ", "");
        plain_type = std::format("open_all_type_{}", plain_type);
        set = xset_get(plain_type.data());
        xset_set_cb(set, (GFunc)on_popup_open_all, data);
        set->lock = true;
        set->menu_style = xset::menu::normal;
        set->shared_key = xset::get_name_from_xsetname(xset::name::open_all);
        set2 = xset_get(xset::name::open_all);
        set->menu_label = set2->menu_label;
        set->context = std::nullopt;
        item = GTK_MENU_ITEM(xset_add_menuitem(browser, submenu, accel_group, set));
        set->menu_label = std::nullopt; // do not bother to save this

        //
        g_signal_connect(submenu, "key-press-event", G_CALLBACK(app_menu_keypress), data);
    }

    // Go >
    if (browser)
    {
        set = xset_get(xset::name::go_back);
        xset_set_cb(set, (GFunc)ptk_file_browser_go_back, browser);
        set->disable = !(browser->curHistory_ && browser->curHistory_->prev);
        set = xset_get(xset::name::go_forward);
        xset_set_cb(set, (GFunc)ptk_file_browser_go_forward, browser);
        set->disable = !(browser->curHistory_ && browser->curHistory_->next);
        set = xset_get(xset::name::go_up);
        xset_set_cb(set, (GFunc)ptk_file_browser_go_up, browser);
        set->disable = std::filesystem::equivalent(cwd, "/");
        xset_set_cb(xset::name::go_home, (GFunc)ptk_file_browser_go_home, browser);
        xset_set_cb(xset::name::go_default, (GFunc)ptk_file_browser_go_default, browser);
        xset_set_cb(xset::name::go_set_default,
                    (GFunc)ptk_file_browser_set_default_folder,
                    browser);
        xset_set_cb(xset::name::edit_canon, (GFunc)on_popup_canon, data);
        xset_set_cb("go_refresh", (GFunc)ptk_file_browser_refresh, browser);
        set = xset_get(xset::name::focus_path_bar);
        xset_set_cb(set, (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 0);
        set = xset_get(xset::name::focus_filelist);
        xset_set_cb(set, (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 4);
        set = xset_get(xset::name::focus_dirtree);
        xset_set_cb(set, (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 1);
        set = xset_get(xset::name::focus_book);
        xset_set_cb(set, (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 2);
        set = xset_get(xset::name::focus_device);
        xset_set_cb(set, (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 3);

        // Go > Tab >
        set = xset_get(xset::name::tab_prev);
        xset_set_cb(set, (GFunc)ptk_file_browser_go_tab, browser);
        xset_set_ob1_int(set, "tab", tab_control_code_prev);
        set->disable = (tab_count < 2);
        set = xset_get(xset::name::tab_next);
        xset_set_cb(set, (GFunc)ptk_file_browser_go_tab, browser);
        xset_set_ob1_int(set, "tab", tab_control_code_next);
        set->disable = (tab_count < 2);
        set = xset_get(xset::name::tab_close);
        xset_set_cb(set, (GFunc)ptk_file_browser_go_tab, browser);
        xset_set_ob1_int(set, "tab", tab_control_code_close);
        set = xset_get(xset::name::tab_restore);
        xset_set_cb(set, (GFunc)ptk_file_browser_go_tab, browser);
        xset_set_ob1_int(set, "tab", tab_control_code_restore);

        for (tab_t tab : TABS)
        {
            const std::string name = std::format("tab_{}", tab);
            set = xset_get(name);
            xset_set_cb(set, (GFunc)ptk_file_browser_go_tab, browser);
            xset_set_ob1_int(set, "tab", tab);
            set->disable = (tab > tab_count) || (tab == tab_num);
        }
        set = xset_get(xset::name::con_go);
        xset_add_menuitem(browser, popup, accel_group, set);

        // New >
        xset_set_cb(xset::name::new_file, (GFunc)on_popup_new_text_file_activate, data);
        xset_set_cb(xset::name::new_directory, (GFunc)on_popup_new_folder_activate, data);
        xset_set_cb(xset::name::new_link, (GFunc)on_popup_new_link_activate, data);
        set = xset_get(xset::name::new_archive);
        xset_set_cb(set, (GFunc)on_popup_compress_activate, data);
        set->disable = set_disable;

        set = xset_get(xset::name::tab_new);
        xset_set_cb(set, (GFunc)ptk_file_browser_new_tab, browser);
        set->disable = !browser;
        set = xset_get(xset::name::tab_new_here);
        xset_set_cb(set, (GFunc)on_popup_open_in_new_tab_here, data);
        set->disable = !browser;
        set = xset_get(xset::name::new_bookmark);
        xset_set_cb(set, (GFunc)on_new_bookmark, data);
        set->disable = !browser;

        set = xset_get(xset::name::open_new);
        xset_add_menuitem(browser, popup, accel_group, set);

        set = xset_get(xset::name::separator);
        xset_add_menuitem(browser, popup, accel_group, set);

        // Edit
        set = xset_get(xset::name::copy_name);
        xset_set_cb(set, (GFunc)on_popup_copy_name_activate, data);
        set->disable = set_disable;
        set = xset_get(xset::name::copy_path);
        xset_set_cb(set, (GFunc)on_popup_copy_text_activate, data);
        set->disable = set_disable;
        set = xset_get(xset::name::copy_parent);
        xset_set_cb(set, (GFunc)on_popup_copy_parent_activate, data);
        set->disable = set_disable;
        set = xset_get(xset::name::paste_link);
        xset_set_cb(set, (GFunc)on_popup_paste_link_activate, data);
        set->disable = !is_clip || no_write_access;
        set = xset_get(xset::name::paste_target);
        xset_set_cb(set, (GFunc)on_popup_paste_target_activate, data);
        set->disable = !is_clip || no_write_access;

        set = xset_get(xset::name::paste_as);
        xset_set_cb(set, (GFunc)on_popup_paste_as_activate, data);
        set->disable = !is_clip;

        set = xset_get(xset::name::edit_hide);
        xset_set_cb(set, (GFunc)on_hide_file, data);
        set->disable = set_disable || no_write_access || !browser;

        xset_set_cb(xset::name::select_all, (GFunc)ptk_file_browser_select_all, data->browser);
        set = xset_get(xset::name::select_un);
        xset_set_cb(set, (GFunc)ptk_file_browser_unselect_all, browser);
        set->disable = set_disable;
        xset_set_cb(xset::name::select_invert, (GFunc)ptk_file_browser_invert_selection, browser);
        xset_set_cb(xset::name::select_patt, (GFunc)on_popup_select_pattern, data);

        static constexpr std::array<xset::name, 40> copycmds{
            xset::name::copy_loc,        xset::name::copy_loc_last,   xset::name::copy_tab_prev,
            xset::name::copy_tab_next,   xset::name::copy_tab_1,      xset::name::copy_tab_2,
            xset::name::copy_tab_3,      xset::name::copy_tab_4,      xset::name::copy_tab_5,
            xset::name::copy_tab_6,      xset::name::copy_tab_7,      xset::name::copy_tab_8,
            xset::name::copy_tab_9,      xset::name::copy_tab_10,     xset::name::copy_panel_prev,
            xset::name::copy_panel_next, xset::name::copy_panel_1,    xset::name::copy_panel_3,
            xset::name::copy_panel_3,    xset::name::copy_panel_4,    xset::name::move_loc,
            xset::name::move_loc_last,   xset::name::move_tab_prev,   xset::name::move_tab_next,
            xset::name::move_tab_1,      xset::name::move_tab_2,      xset::name::move_tab_3,
            xset::name::move_tab_4,      xset::name::move_tab_5,      xset::name::move_tab_6,
            xset::name::move_tab_7,      xset::name::move_tab_8,      xset::name::move_tab_9,
            xset::name::move_tab_10,     xset::name::move_panel_prev, xset::name::move_panel_next,
            xset::name::move_panel_1,    xset::name::move_panel_2,    xset::name::move_panel_3,
            xset::name::move_panel_4,
        };

        for (const xset::name copycmd : copycmds)
        {
            set = xset_get(copycmd);
            xset_set_cb(set, (GFunc)on_copycmd, data);
            xset_set_ob1(set, "set", set);
        }

        // enables
        set = xset_get(xset::name::copy_loc_last);
        set2 = xset_get(xset::name::move_loc_last);

        set = xset_get(xset::name::copy_tab_prev);
        set->disable = (tab_num == 1);
        set = xset_get(xset::name::copy_tab_next);
        set->disable = (tab_num == tab_count);
        set = xset_get(xset::name::move_tab_prev);
        set->disable = (tab_num == 1);
        set = xset_get(xset::name::move_tab_next);
        set->disable = (tab_num == tab_count);

        set = xset_get(xset::name::copy_panel_prev);
        set->disable = (panel_count < 2);
        set = xset_get(xset::name::copy_panel_next);
        set->disable = (panel_count < 2);
        set = xset_get(xset::name::move_panel_prev);
        set->disable = (panel_count < 2);
        set = xset_get(xset::name::move_panel_next);
        set->disable = (panel_count < 2);

        for (tab_t tab : TABS)
        {
            const auto copy_tab = std::format("copy_tab_{}", tab);
            set = xset_get(copy_tab);
            set->disable = (tab > tab_count) || (tab == tab_num);

            const auto move_tab = std::format("move_tab_{}", tab);
            set = xset_get(move_tab);
            set->disable = (tab > tab_count) || (tab == tab_num);
        }

        for (panel_t panel : PANELS)
        {
            const bool b = main_window_panel_is_visible(browser, panel);

            const auto copy_panel = std::format("copy_panel_{}", panel);
            set = xset_get(copy_panel);
            set->disable = (panel == p) || !b;

            const auto move_panel = std::format("move_panel_{}", panel);
            set = xset_get(move_panel);
            set->disable = (panel == p) || !b;
        }

        set = xset_get(xset::name::copy_to);
        set->disable = set_disable;

        set = xset_get(xset::name::move_to);
        set->disable = set_disable;

        set = xset_get(xset::name::edit_submenu);
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    set = xset_get(xset::name::edit_cut);
    xset_set_cb(set, (GFunc)on_popup_cut_activate, data);
    set->disable = set_disable;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_get(xset::name::edit_copy);
    xset_set_cb(set, (GFunc)on_popup_copy_activate, data);
    set->disable = set_disable;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_get(xset::name::edit_paste);
    xset_set_cb(set, (GFunc)on_popup_paste_activate, data);
    set->disable = !is_clip || no_write_access;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_get(xset::name::edit_rename);
    xset_set_cb(set, (GFunc)on_popup_rename_activate, data);
    set->disable = set_disable;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_get(xset::name::edit_trash);
    xset_set_cb(set, (GFunc)on_popup_trash_activate, data);
    set->disable = set_disable || no_write_access;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_get(xset::name::edit_delete);
    xset_set_cb(set, (GFunc)on_popup_delete_activate, data);
    set->disable = set_disable || no_write_access;
    xset_add_menuitem(browser, popup, accel_group, set);

    set = xset_get(xset::name::separator);
    xset_add_menuitem(browser, popup, accel_group, set);

    if (browser)
    {
        // View >
        ptk_file_menu_add_panel_view_menu(browser, popup, accel_group);

        // Properties >
        xset_set_cb(xset::name::prop_info, (GFunc)on_popup_file_properties_activate, data);
        xset_set_cb(xset::name::prop_attr, (GFunc)on_popup_file_attributes_activate, data);
        xset_set_cb(xset::name::prop_perm, (GFunc)on_popup_file_permissions_activate, data);

        static constexpr std::array<xset::name, 22> permcmds{
            xset::name::perm_r,         xset::name::perm_rw,        xset::name::perm_rwx,
            xset::name::perm_r_r,       xset::name::perm_rw_r,      xset::name::perm_rw_rw,
            xset::name::perm_rwxr_x,    xset::name::perm_rwxrwx,    xset::name::perm_r_r_r,
            xset::name::perm_rw_r_r,    xset::name::perm_rw_rw_rw,  xset::name::perm_rwxr_r,
            xset::name::perm_rwxr_xr_x, xset::name::perm_rwxrwxrwx, xset::name::perm_rwxrwxrwt,
            xset::name::perm_unstick,   xset::name::perm_stick,     xset::name::perm_go_w,
            xset::name::perm_go_rwx,    xset::name::perm_ugo_w,     xset::name::perm_ugo_rx,
            xset::name::perm_ugo_rwx,
        };

        for (const xset::name permcmd : permcmds)
        {
            set = xset_get(permcmd);
            xset_set_cb(set, (GFunc)on_permission, data);
            xset_set_ob1(set, "set", set);
        }

        set = xset_get(xset::name::prop_quick);
        set->disable = no_write_access || set_disable;

        set = xset_get(xset::name::con_prop);
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    gtk_widget_show_all(GTK_WIDGET(popup));

    g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    return popup;
}

static void
on_popup_open_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    ptk_open_files_with_app(data->cwd, data->sel_files, "", data->browser, true, false);
}

static void
on_popup_open_with_another_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    vfs::mime_type mime_type;

    if (data->file)
    {
        mime_type = data->file->mime_type();
        if (!mime_type)
        {
            mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
        }
    }
    else
    {
        mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_DIRECTORY);
    }

    GtkWindow* parent_win = nullptr;
    if (data->browser)
    {
        parent_win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->browser)));
    }

    const auto check_app =
        ptk_choose_app_for_mime_type(parent_win, mime_type, false, true, true, false);
    if (check_app)
    {
        const auto& app = check_app.value();
        ptk_open_files_with_app(data->cwd, data->sel_files, app, data->browser, false, false);
    }
}

static void
on_popup_open_all(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    ptk_open_files_with_app(data->cwd, data->sel_files, "", data->browser, false, true);
}

static void
on_popup_run_app(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    const char* desktop_file =
        static_cast<const char*>(g_object_get_data(G_OBJECT(menuitem), "desktop_file"));
    const vfs::desktop desktop = vfs_get_desktop(desktop_file);

    ptk_open_files_with_app(data->cwd,
                            data->sel_files,
                            desktop->name(),
                            data->browser,
                            false,
                            false);
}

namespace ptk::file_menu
{
    enum class app_job
    {
        default_action,
        edit,
        edit_list,
        browse,
        browse_shared,
        edit_type,
        view,
        view_type,
        view_over,
        update,
        browse_mime,
        browse_mime_usr,
        usr
    };
}

static const std::optional<std::filesystem::path>
get_shared_desktop_file_location(const std::string_view name)
{
    for (const std::string_view sys_dir : vfs::user_dirs->system_data_dirs())
    {
        auto ret = vfs_mime_type_locate_desktop_file(sys_dir, name);
        if (ret)
        {
            return ret;
        }
    }
    return std::nullopt;
}

void
app_job(GtkWidget* item, GtkWidget* app_item)
{
    char* str;
    std::string str2;

    std::string command;

    const std::string desktop_file =
        CONST_CHAR(g_object_get_data(G_OBJECT(app_item), "desktop_file"));
    const vfs::desktop desktop = vfs_get_desktop(desktop_file);
    if (desktop->name().empty())
    {
        return;
    }

    const i32 job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job"));
    PtkFileMenu* data = PTK_FILE_MENU(g_object_get_data(G_OBJECT(item), "data"));
    if (!(data && data->file))
    {
        return;
    }

    vfs::mime_type mime_type = data->file->mime_type();
    if (!mime_type)
    {
        mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
    }

    switch (ptk::file_menu::app_job(job))
    {
        case ptk::file_menu::app_job::default_action:
        {
            mime_type->set_default_action(desktop->name());
            break;
        }
        case ptk::file_menu::app_job::edit:
        {
            const auto path = vfs::user_dirs->data_dir() / "applications" / desktop->name();
            if (!std::filesystem::exists(path))
            {
                const auto check_share_desktop = vfs_mime_type_locate_desktop_file(desktop->name());
                if (!check_share_desktop ||
                    std::filesystem::equivalent(check_share_desktop.value(), path))
                {
                    return;
                }
                const auto& share_desktop = check_share_desktop.value();

                const auto response = ptk_show_message(
                    GTK_WINDOW(data->browser),
                    GtkMessageType::GTK_MESSAGE_QUESTION,
                    "Copy Desktop File",
                    GtkButtonsType::GTK_BUTTONS_YES_NO,
                    std::format("The file '{0}' does not exist.\n\nBy copying '{1}' to '{0}' and "
                                "editing it, you can adjust the behavior and appearance of this "
                                "application for the current user.\n\nCreate this copy now?",
                                path.string(),
                                share_desktop.string()));

                if (response != GtkResponseType::GTK_RESPONSE_YES)
                {
                    break;
                }

                // need to copy
                command = std::format("cp -a  {} {}", share_desktop.string(), path.string());
                Glib::spawn_command_line_sync(command);
                if (!std::filesystem::exists(path))
                {
                    return;
                }
            }
            xset_edit(GTK_WIDGET(data->browser), path);
            break;
        }
        case ptk::file_menu::app_job::view:
        {
            const auto desktop_path = get_shared_desktop_file_location(desktop->name());
            if (desktop_path)
            {
                xset_edit(GTK_WIDGET(data->browser), desktop_path.value());
            }
            break;
        }
        case ptk::file_menu::app_job::edit_list:
        {
            // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
            const auto path = vfs::user_dirs->config_dir() / "mimeapps.list";
            xset_edit(GTK_WIDGET(data->browser), path);
            break;
        }
        case ptk::file_menu::app_job::browse:
        {
            const auto path = vfs::user_dirs->data_dir() / "applications";
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);

            if (data->browser)
            {
                data->browser->run_event<spacefm::signal::open_item>(path,
                                                                     ptk::open_action::new_tab);
            }
            break;
        }
        case ptk::file_menu::app_job::browse_shared:
        {
            std::filesystem::path path;
            const auto desktop_path = get_shared_desktop_file_location(desktop->name());
            if (desktop_path)
            {
                path = desktop_path.value().parent_path();
            }
            else
            {
                path = "/usr/share/applications";
            }
            if (data->browser)
            {
                data->browser->run_event<spacefm::signal::open_item>(path,
                                                                     ptk::open_action::new_tab);
            }
        }
        break;
        case ptk::file_menu::app_job::edit_type:
        {
            const auto mime_path = vfs::user_dirs->data_dir() / "mime" / "packages";
            std::filesystem::create_directories(mime_path);
            std::filesystem::permissions(mime_path, std::filesystem::perms::owner_all);
            str2 = ztd::replace(mime_type->type(), "/", "-");
            str2 = std::format("{}.xml", str2);
            const auto mime_file = vfs::user_dirs->data_dir() / "mime" / "packages" / str2;
            if (!std::filesystem::exists(mime_file))
            {
                std::string msg;
                const std::string xml_file = std::format("{}.xml", mime_type->type());
                const auto usr_path = std::filesystem::path() / "/usr/share/mime" / xml_file;

                if (std::filesystem::exists(usr_path))
                {
                    msg =
                        std::format("The file '{0}' does not exist.\n\nBy copying '{1}' to '{0}' "
                                    "and editing it, you can adjust how MIME type '{2}' files are "
                                    "recognized for the current user.\n\nCreate this copy now?",
                                    mime_file.string(),
                                    usr_path.string(),
                                    mime_type->type());
                }
                else
                {
                    msg =
                        std::format("The file '{0}' does not exist.\n\nBy creating new file '{0}' "
                                    "and editing it, you can define how MIME type '{1}' files are "
                                    "recognized for the current user.\n\nCreate this file now?",
                                    mime_file.string(),
                                    mime_type->type());
                }

                const auto response = ptk_show_message(GTK_WINDOW(data->browser),
                                                       GtkMessageType::GTK_MESSAGE_QUESTION,
                                                       "Create New XML",
                                                       GtkButtonsType::GTK_BUTTONS_YES_NO,
                                                       msg);

                if (response != GtkResponseType::GTK_RESPONSE_YES)
                {
                    break;
                }

                // need to create

                // clang-format off
                msg = std::format(
                    "<?xml version='1.0' encoding='utf-8'?>\n"
                    "<mime-info xmlns='http://www.freedesktop.org/standards/shared-mime-info'>\n"
                    "<mime-type type='{}'>\n\n"
                    "<!-- This file was generated by SpaceFM to allow you to change the name or icon\n"
                    "     of the above mime type and to change the filename or magic patterns that\n"
                    "     define this type.\n\n"
                    "     IMPORTANT:  After saving this file, restart SpaceFM. You may need to run:\n"
                    "     update-mime-database ~/.local/share/mime\n\n"
                    "     Delete this file from ~/.local/share/mime/packages/ to revert to default.\n\n"
                    "     To make this definition file apply to all users, copy this file to\n"
                    "     /usr/share/mime/packages/ and:  sudo update-mime-database /usr/share/mime\n\n"
                    "     For help editing this file:\n"
                    "     http://standards.freedesktop.org/shared-mime-info-spec/latest/ar01s02.html\n"
                    "     http://www.freedesktop.org/wiki/Specifications/AddingMIMETutor\n\n"
                    "     Example to define the name/icon of PNG files (with optional translation):\n\n"
                    "        <comment>Portable Network Graphics file</comment>\n"
                    "        <comment xml:lang=\"en\">Portable Network Graphics file</comment>\n"
                    "        <icon name=\"spacefm\"/>\n\n"
                    "     Example to detect PNG files by glob pattern:\n\n"
                    "        <glob pattern=\"*.png\"/>\n\n"
                    "     Example to detect PNG files by file contents:\n\n"
                    "        <magic priority=\"50\">\n"
                    "            <match type=\"string\" value=\"\\x89PNG\" offset=\"0\"/>\n"
                    "        </magic>\n"
                    "-->",
                    mime_type->type());
                // clang-format on

                // build from /usr/share/mime type ?

                std::string contents;
                try
                {
                    contents = Glib::file_get_contents(usr_path);
                }
                catch (const Glib::FileError& e)
                {
                    const std::string what = e.what();
                    ztd::logger::warn("Error reading {}: {}", usr_path.string(), what);
                }

                if (!contents.empty())
                {
                    char* contents2 = ztd::strdup(contents);
                    char* start = nullptr;
                    str = strstr(contents2, "\n<mime-type ");
                    if (str)
                    {
                        str = strstr(str, ">\n");
                        if (str)
                        {
                            str[1] = '\0';
                            start = contents2;
                            str = strstr(str + 2, "<!--Created automatically");
                            if (str)
                            {
                                str = strstr(str, "-->");
                                if (str)
                                {
                                    start = str + 4;
                                }
                            }
                        }
                    }
                    if (start)
                    {
                        contents = std::format("{}\n\n{}</mime-info>\n", msg, start);
                    }
                }

                if (contents.empty())
                {
                    contents = std::format("{}\n\n<!-- insert your patterns below "
                                           "-->\n\n\n</mime-type>\n</mime-info>\n\n",
                                           msg);
                }

                write_file(mime_file, contents);
            }
            if (std::filesystem::exists(mime_file))
            {
                xset_edit(GTK_WIDGET(data->browser), mime_file);
            }

            vfs_mime_monitor();
            break;
        }
        case ptk::file_menu::app_job::view_type:
        {
            str2 = std::format("{}.xml", mime_type->type());
            const auto path = std::filesystem::path() / "/usr/share/mime" / str2;
            if (std::filesystem::exists(path))
            {
                xset_edit(GTK_WIDGET(data->browser), path);
            }
            break;
        }
        case ptk::file_menu::app_job::view_over:
        {
            const std::filesystem::path path = "/usr/share/mime/packages/Overrides.xml";
            xset_edit(GTK_WIDGET(data->browser), path);
            break;
        }
        case ptk::file_menu::app_job::browse_mime_usr:
        {
            if (data->browser)
            {
                const std::filesystem::path path = "/usr/share/mime/packages";
                data->browser->run_event<spacefm::signal::open_item>(path,
                                                                     ptk::open_action::new_tab);
            }
            break;
        }
        case ptk::file_menu::app_job::browse_mime:
        {
            const auto path = vfs::user_dirs->data_dir() / "mime" / "packages";
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
            if (data->browser)
            {
                data->browser->run_event<spacefm::signal::open_item>(path,
                                                                     ptk::open_action::new_tab);
            }
            vfs_mime_monitor();
            break;
        }
        case ptk::file_menu::app_job::update:
        {
            const auto data_dir = vfs::user_dirs->data_dir();
            command = std::format("update-mime-database {}/mime", data_dir.string());
            ztd::logger::info("COMMAND={}", command);
            Glib::spawn_command_line_async(command);

            command = std::format("update-desktop-database {}/applications", data_dir.string());
            ztd::logger::info("COMMAND={}", command);
            Glib::spawn_command_line_async(command);
            break;
        }
        case ptk::file_menu::app_job::usr:
            break;
    }
}

static bool
app_menu_keypress(GtkWidget* menu, GdkEventKey* event, PtkFileMenu* data)
{
    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(menu));
    if (!item)
    {
        return false;
    }

    // if original menu, desktop will be set
    const std::string desktop_file = CONST_CHAR(g_object_get_data(G_OBJECT(item), "desktop_file"));
    const vfs::desktop desktop = vfs_get_desktop(desktop_file);
    // else if app menu, data will be set
    // PtkFileMenu* app_data = PTK_FILE_MENU(g_object_get_data(G_OBJECT(item), "data"));

    const auto keymod = ptk_get_keymod(event->state);
    if (keymod == 0)
    {
        if (event->keyval == GDK_KEY_F2 || event->keyval == GDK_KEY_Menu)
        {
            show_app_menu(menu, item, data, 0, event->time);
            return true;
        }
    }
    return false;
}

static void
on_app_menu_hide(GtkWidget* widget, GtkWidget* app_menu)
{
    gtk_widget_set_sensitive(widget, true);
    gtk_menu_shell_deactivate(GTK_MENU_SHELL(app_menu));
}

static GtkWidget*
app_menu_additem(GtkWidget* menu, const std::string_view label, ptk::file_menu::app_job job,
                 GtkWidget* app_item, PtkFileMenu* data)
{
    GtkWidget* item = gtk_menu_item_new_with_mnemonic(label.data());

    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(magic_enum::enum_integer(job)));
    g_object_set_data(G_OBJECT(item), "data", data);
    gtk_container_add(GTK_CONTAINER(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(app_job), app_item);
    return item;
}

static void
show_app_menu(GtkWidget* menu, GtkWidget* app_item, PtkFileMenu* data, u32 button, std::time_t time)
{
    (void)button;
    (void)time;
    GtkWidget* newitem;
    GtkWidget* submenu;
    std::string str;

    if (!(data && data->file))
    {
        return;
    }

    std::string type;
    vfs::mime_type mime_type = data->file->mime_type();
    if (mime_type)
    {
        type = mime_type->type();
    }
    else
    {
        type = "unknown";
    }

    const std::string desktop_file =
        CONST_CHAR(g_object_get_data(G_OBJECT(app_item), "desktop_file"));
    const vfs::desktop desktop = vfs_get_desktop(desktop_file);

    GtkWidget* app_menu = gtk_menu_new();

    // Set Default
    newitem = app_menu_additem(app_menu,
                               "_Set As Default",
                               ptk::file_menu::app_job::default_action,
                               app_item,
                               data);

    // Separator
    gtk_container_add(GTK_CONTAINER(app_menu), gtk_separator_menu_item_new());

    // *.desktop (missing)
    if (!desktop->name().empty())
    {
        const auto path = vfs::user_dirs->data_dir() / "applications" / desktop->name();
        if (std::filesystem::exists(path))
        {
            str = ztd::replace(desktop->name(), ".desktop", "._desktop");
        }
        else
        {
            str = ztd::replace(desktop->name(), ".desktop", "._desktop");
            str = std::format("{} (*copy)", str);
        }
        newitem = app_menu_additem(app_menu, str, ptk::file_menu::app_job::edit, app_item, data);
    }

    // mimeapps.list
    newitem = app_menu_additem(app_menu,
                               "_mimeapps.list",
                               ptk::file_menu::app_job::edit_list,
                               app_item,
                               data);

    // applications/
    newitem = app_menu_additem(app_menu,
                               "appli_cations/",
                               ptk::file_menu::app_job::browse,
                               app_item,
                               data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem), !!data->browser);

    // Separator
    gtk_container_add(GTK_CONTAINER(app_menu), gtk_separator_menu_item_new());

    // *.xml (missing)
    str = ztd::replace(type, "/", "-");
    str = std::format("{}.xml", str);
    const auto usr_mime_path = vfs::user_dirs->data_dir() / "mime/packages" / str;
    if (std::filesystem::exists(usr_mime_path))
    {
        str = ztd::replace(type, "/", "-");
        str = std::format("{}._xml", str);
    }
    else
    {
        str = ztd::replace(type, "/", "-");
        str = std::format("{}._xml (*new)", str);
    }
    newitem = app_menu_additem(app_menu, str, ptk::file_menu::app_job::edit_type, app_item, data);

    // mime/packages/
    newitem = app_menu_additem(app_menu,
                               "mime/pac_kages/",
                               ptk::file_menu::app_job::browse_mime,
                               app_item,
                               data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem), !!data->browser);

    // Separator
    gtk_container_add(GTK_CONTAINER(app_menu), gtk_separator_menu_item_new());

    // /usr submenu
    newitem = gtk_menu_item_new_with_mnemonic("/_usr");
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu);
    gtk_container_add(GTK_CONTAINER(app_menu), newitem);
    g_object_set_data(G_OBJECT(newitem), "job", GINT_TO_POINTER(ptk::file_menu::app_job::usr));
    g_object_set_data(G_OBJECT(newitem), "data", data);
    g_signal_connect(submenu, "key_press_event", G_CALLBACK(app_menu_keypress), data);

    // View /usr .desktop
    if (!desktop->name().empty())
    {
        newitem = app_menu_additem(submenu,
                                   desktop->name(),
                                   ptk::file_menu::app_job::view,
                                   app_item,
                                   data);

        const auto desk_path = get_shared_desktop_file_location(desktop->name());
        gtk_widget_set_sensitive(GTK_WIDGET(newitem), !!desk_path);
    }

    // /usr applications/
    newitem = app_menu_additem(submenu,
                               "appli_cations/",
                               ptk::file_menu::app_job::browse_shared,
                               app_item,
                               data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem), !!data->browser);

    // Separator
    gtk_container_add(GTK_CONTAINER(submenu), gtk_separator_menu_item_new());

    // /usr *.xml
    str = std::format("{}.xml", type);
    const auto sys_mime_path = std::filesystem::path() / "/usr/share/mime" / str;
    str = std::format("{}._xml", type);
    newitem = app_menu_additem(submenu, str, ptk::file_menu::app_job::view_type, app_item, data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem), std::filesystem::exists(sys_mime_path));

    // /usr *Overrides.xml
    newitem = app_menu_additem(submenu,
                               "_Overrides.xml",
                               ptk::file_menu::app_job::view_over,
                               app_item,
                               data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem),
                             std::filesystem::exists("/usr/share/mime/packages/Overrides.xml"));

    // mime/packages/
    newitem = app_menu_additem(submenu,
                               "mime/pac_kages/",
                               ptk::file_menu::app_job::browse_mime_usr,
                               app_item,
                               data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem),
                             !!data->browser &&
                                 std::filesystem::is_directory("/usr/share/mime/packages"));

    // Separator
    gtk_container_add(GTK_CONTAINER(app_menu), gtk_separator_menu_item_new());

    // show menu
    gtk_widget_show_all(GTK_WIDGET(app_menu));
    gtk_menu_popup_at_pointer(GTK_MENU(app_menu), nullptr);
    gtk_widget_set_sensitive(GTK_WIDGET(menu), false);

    g_signal_connect(menu, "hide", G_CALLBACK(on_app_menu_hide), app_menu);
    g_signal_connect(app_menu, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    g_signal_connect(app_menu, "key_press_event", G_CALLBACK(app_menu_keypress), data);

    gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(app_menu), true);
    // this is required when showing the menu via F2 or Menu key for focus
    gtk_menu_shell_select_first(GTK_MENU_SHELL(app_menu), true);
}

static bool
on_app_button_press(GtkWidget* item, GdkEventButton* event, PtkFileMenu* data)
{
    GtkWidget* menu = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "menu"));
    const u32 keymod = ptk_get_keymod(event->state);

    if (event->type == GdkEventType::GDK_BUTTON_RELEASE)
    {
        if (event->button == 1 && keymod == 0)
        {
            // user released left button - due to an apparent gtk bug, activate
            // does not always fire on this event so handle it ourselves
            // see also settings.c xset_design_cb()
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
        // true for issue #521 where a right-click also left-clicks the first
        // menu item in some GTK2/3 themes.
        return true;
    }
    else if (event->type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }

    switch (event->button)
    {
        case 1:
        case 3:
            // left or right click
            if (keymod == 0)
            {
                // no modifier
                if (event->button == 3)
                {
                    // right
                    show_app_menu(menu, item, data, event->button, event->time);
                    return true;
                }
            }
            break;
        case 2:
            // middle click
            if (keymod == 0)
            {
                // no modifier
                show_app_menu(menu, item, data, event->button, event->time);
                return true;
            }
            break;
        default:
            break;
    }
    return false; // true will not stop activate on button-press (will on release)
}

static void
on_popup_open_in_new_tab_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;

    if (!data->sel_files.empty())
    {
        for (const vfs::file_info file : data->sel_files)
        {
            const auto full_path = data->cwd / file->name();
            if (data->browser && std::filesystem::is_directory(full_path))
            {
                data->browser->run_event<spacefm::signal::open_item>(full_path,
                                                                     ptk::open_action::new_tab);
            }
        }
    }
    else if (data->browser)
    {
        data->browser->run_event<spacefm::signal::open_item>(data->cwd, ptk::open_action::new_tab);
    }
}

void
on_popup_open_in_new_tab_here(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser && std::filesystem::is_directory(data->cwd))
    {
        data->browser->run_event<spacefm::signal::open_item>(data->file_path,
                                                             ptk::open_action::new_tab);
    }
}

static void
on_new_bookmark(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    ptk_bookmark_view_add_bookmark(data->browser->cwd());
}

static void
on_popup_cut_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->sel_files.empty())
    {
        return;
    }
    ptk_clipboard_cut_or_copy_files(data->sel_files, false);
}

static void
on_popup_copy_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->sel_files.empty())
    {
        return;
    }
    ptk_clipboard_cut_or_copy_files(data->sel_files, true);
}

static void
on_popup_paste_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser)
    {
        GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
        ptk_clipboard_paste_files(GTK_WINDOW(parent_win),
                                  data->cwd,
                                  GTK_TREE_VIEW(data->browser->task_view()),
                                  nullptr,
                                  nullptr);
    }
}

static void
on_popup_paste_link_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->paste_link();
    }
}

static void
on_popup_paste_target_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->paste_target();
    }
}

static void
on_popup_copy_text_activate(GtkMenuItem* menuitem, PtkFileMenu* data) // MOD added
{
    (void)menuitem;
    ptk_clipboard_copy_as_text(data->sel_files);
}

static void
on_popup_copy_name_activate(GtkMenuItem* menuitem, PtkFileMenu* data) // MOD added
{
    (void)menuitem;
    ptk_clipboard_copy_name(data->sel_files);
}

static void
on_popup_copy_parent_activate(GtkMenuItem* menuitem, PtkFileMenu* data) // MOD added
{
    (void)menuitem;
    if (!data->cwd.empty())
    {
        ptk_clipboard_copy_text(data->cwd.string());
    }
}

static void
on_popup_paste_as_activate(GtkMenuItem* menuitem, PtkFileMenu* data) // sfm added
{
    (void)menuitem;
    ptk_file_misc_paste_as(data->browser, data->cwd, nullptr);
}

static void
on_popup_delete_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;

    if (data->sel_files.empty())
    {
        return;
    }

    if (data->browser)
    {
        GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
        ptk_delete_files(GTK_WINDOW(parent_win),
                         data->cwd,
                         data->sel_files,
                         GTK_TREE_VIEW(data->browser->task_view()));
    }
}

static void
on_popup_trash_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;

    if (data->sel_files.empty())
    {
        return;
    }

    if (data->browser)
    {
        GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
        ptk_trash_files(GTK_WINDOW(parent_win),
                        data->cwd,
                        data->sel_files,
                        GTK_TREE_VIEW(data->browser->task_view()));
    }
}

static void
on_popup_rename_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->rename_selected_files(data->sel_files, data->cwd);
    }
}

static void
on_popup_compress_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    ptk_archiver_create(data->browser, data->sel_files);
}

static void
on_popup_extract_to_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    ptk_archiver_extract(data->browser, data->sel_files, "");
}

static void
on_popup_extract_here_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    ptk_archiver_extract(data->browser, data->sel_files, data->cwd);
}

static void
on_popup_extract_open_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    // If menuitem is set, function was called from GUI so files will contain an archive
    ptk_archiver_open(data->browser, data->sel_files);
}

static void
on_autoopen_create_cb(void* task, AutoOpenCreate* ao)
{
    (void)task;
    if (!ao)
    {
        return;
    }

    if (GTK_IS_WIDGET(ao->file_browser) && std::filesystem::exists(ao->path))
    {
        const auto cwd = ao->path.parent_path();
        vfs::file_info file;

        // select file
        if (std::filesystem::equivalent(cwd, ao->file_browser->cwd()))
        {
            file = vfs_file_info_new(ao->path);
            ao->file_browser->dir_->emit_file_created(file->name(), true);
            vfs_file_info_unref(file);
            vfs_dir_flush_notify_cache();
            ao->file_browser->select_file(ao->path);
        }

        // open file
        if (ao->open_file)
        {
            if (std::filesystem::is_directory(ao->path))
            {
                ao->file_browser->chdir(ao->path, ptk::file_browser::chdir_mode::add_history);
            }
            else
            {
                file = vfs_file_info_new(ao->path);
                const std::vector<vfs::file_info> sel_files{file};
                ptk_open_files_with_app(cwd, sel_files, "", ao->file_browser, false, true);
                vfs_file_info_unref(file);
            }
        }
    }

    delete ao;
}

static void
create_new_file(PtkFileMenu* data, ptk::rename_mode create_new)
{
    if (data->cwd.empty())
    {
        return;
    }

    const auto ao = new AutoOpenCreate;
    ao->file_browser = data->browser;
    ao->open_file = false;
    if (data->browser)
    {
        ao->callback = (GFunc)on_autoopen_create_cb;
    }

    vfs::file_info file = nullptr;
    if (!data->sel_files.empty())
    {
        file = data->sel_files.front();
    }

    const i32 result =
        ptk_rename_file(data->browser, data->cwd.c_str(), file, nullptr, false, create_new, ao);
    if (result == 0)
    {
        delete ao;
    }
}

static void
on_popup_new_folder_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    create_new_file(data, ptk::rename_mode::new_dir);
}

static void
on_popup_new_text_file_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    create_new_file(data, ptk::rename_mode::new_file);
}

static void
on_popup_new_link_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    create_new_file(data, ptk::rename_mode::new_link);
}

static void
on_popup_file_properties_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    GtkWindow* parent_win = nullptr;
    if (data->browser)
    {
        parent_win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->browser)));
    }
    ptk_show_file_properties(parent_win, data->cwd, data->sel_files, 0);
}

static void
on_popup_file_attributes_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    GtkWindow* parent_win = nullptr;
    if (data->browser)
    {
        parent_win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->browser)));
    }
    ptk_show_file_properties(parent_win, data->cwd, data->sel_files, 1);
}

static void
on_popup_file_permissions_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    GtkWindow* parent_win = nullptr;
    if (data->browser)
    {
        parent_win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->browser)));
    }
    ptk_show_file_properties(parent_win, data->cwd, data->sel_files, 2);
}

static void
on_popup_canon(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (!data->browser)
    {
        return;
    }

    data->browser->canon(!data->file_path.empty() ? data->file_path : data->cwd);
}

void
ptk_file_menu_action(PtkFileBrowser* browser, const std::string_view setname)
{
    if (!browser)
    {
        return;
    }

    std::filesystem::path cwd;
    std::filesystem::path file_path;
    vfs::file_info file;
    std::vector<vfs::file_info> sel_files;

    // setup data
    if (browser)
    {
        cwd = browser->cwd();
        sel_files = browser->selected_files();
    }
    else
    {
        cwd = vfs::user_dirs->desktop_dir();
    }

    if (sel_files.empty())
    {
        file = nullptr;
    }
    else
    {
        file = sel_files.front();
        file_path = cwd / file->name();
    }

    const auto data = new PtkFileMenu;
    data->cwd = cwd;
    data->browser = browser;
    data->sel_files = sel_files;
    data->file_path = file_path;
    if (file)
    {
        data->file = file;
    }

    // action
    xset_t set = xset_get(setname);
    if (ztd::startswith(set->name, "open_") && !ztd::startswith(set->name, "open_in_"))
    {
        if (set->xset_name == xset::name::open_edit)
        {
            xset_edit(GTK_WIDGET(data->browser), data->file_path);
        }
        else if (set->xset_name == xset::name::open_other)
        {
            on_popup_open_with_another_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::open_execute)
        {
            on_popup_open_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::open_all)
        {
            on_popup_open_all(nullptr, data);
        }
    }
    else if (ztd::startswith(set->name, "arc_"))
    {
        if (set->xset_name == xset::name::archive_extract)
        {
            on_popup_extract_here_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::archive_extract_to)
        {
            on_popup_extract_to_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::archive_open)
        {
            on_popup_extract_open_activate(nullptr, data);
        }
    }
    else if (ztd::startswith(set->name, "new_"))
    {
        if (set->xset_name == xset::name::new_file)
        {
            on_popup_new_text_file_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::new_directory)
        {
            on_popup_new_folder_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::new_link)
        {
            on_popup_new_link_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::new_bookmark)
        {
            ptk_bookmark_view_add_bookmark(browser->cwd());
        }
        else if (set->xset_name == xset::name::new_archive)
        {
            if (browser)
            {
                on_popup_compress_activate(nullptr, data);
            }
        }
    }
    else if (set->xset_name == xset::name::prop_info)
    {
        on_popup_file_properties_activate(nullptr, data);
    }
    else if (set->xset_name == xset::name::prop_attr)
    {
        on_popup_file_attributes_activate(nullptr, data);
    }
    else if (set->xset_name == xset::name::prop_perm)
    {
        on_popup_file_permissions_activate(nullptr, data);
    }
    else if (ztd::startswith(set->name, "edit_"))
    {
        if (set->xset_name == xset::name::edit_cut)
        {
            on_popup_cut_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::edit_copy)
        {
            on_popup_copy_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::edit_paste)
        {
            on_popup_paste_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::edit_rename)
        {
            on_popup_rename_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::edit_delete)
        {
            on_popup_delete_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::edit_trash)
        {
            on_popup_trash_activate(nullptr, data);
        }
        else if (set->xset_name == xset::name::edit_hide)
        {
            on_hide_file(nullptr, data);
        }
        else if (set->xset_name == xset::name::edit_canon)
        {
            if (browser)
            {
                on_popup_canon(nullptr, data);
            }
        }
    }
    else if (set->xset_name == xset::name::copy_name)
    {
        on_popup_copy_name_activate(nullptr, data);
    }
    else if (set->xset_name == xset::name::copy_path)
    {
        on_popup_copy_text_activate(nullptr, data);
    }
    else if (set->xset_name == xset::name::copy_parent)
    {
        on_popup_copy_parent_activate(nullptr, data);
    }
    else if (ztd::startswith(set->name, "copy_loc") || ztd::startswith(set->name, "copy_tab_") ||
             ztd::startswith(set->name, "copy_panel_") || ztd::startswith(set->name, "move_loc") ||
             ztd::startswith(set->name, "move_tab_") || ztd::startswith(set->name, "move_panel_"))
    {
        on_copycmd(nullptr, data, set);
    }
    else if (browser)
    {
        // browser only
        if (ztd::startswith(set->name, "open_in_panel"))
        {
            panel_t i;

            if (ztd::same(set->name, "open_in_panel_prev"))
            {
                i = panel_control_code_prev;
            }
            else if (ztd::same(set->name, "open_in_panel_next"))
            {
                i = panel_control_code_next;
            }
            else
            {
                i = std::stol(set->name);
            }
            main_window_open_in_panel(data->browser, i, data->file_path);
        }
        else if (ztd::startswith(set->name, "opentab_"))
        {
            tab_t i;

            if (set->xset_name == xset::name::opentab_new)
            {
                on_popup_open_in_new_tab_activate(nullptr, data);
            }
            else
            {
                if (set->xset_name == xset::name::opentab_prev)
                {
                    i = tab_control_code_prev;
                }
                else if (set->xset_name == xset::name::opentab_next)
                {
                    i = tab_control_code_next;
                }
                else
                {
                    i = std::stol(set->name);
                }
                data->browser->open_in_tab(data->file_path, i);
            }
        }
        else if (set->xset_name == xset::name::tab_new)
        {
            browser->new_tab();
        }
        else if (set->xset_name == xset::name::tab_new_here)
        {
            on_popup_open_in_new_tab_here(nullptr, data);
        }
    }

    ptk_file_menu_free(data);
}
