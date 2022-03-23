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
#include <filesystem>

#include <array>
#include <vector>

#include <iostream>
#include <fstream>

#include <fcntl.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <glib.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "utils.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "ptk/ptk-file-misc.hxx"
#include "ptk/ptk-file-archiver.hxx"
#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-clipboard.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "settings.hxx"
#include "item-prop.hxx"
#include "main-window.hxx"
#include "ptk/ptk-location-view.hxx"
#include "ptk/ptk-file-list.hxx"

#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-utils.hxx"

static bool on_app_button_press(GtkWidget* item, GdkEventButton* event, PtkFileMenu* data);
static bool app_menu_keypress(GtkWidget* widget, GdkEventKey* event, PtkFileMenu* data);
static void show_app_menu(GtkWidget* menu, GtkWidget* app_item, PtkFileMenu* data,
                          unsigned int button, uint32_t time);

/* Signal handlers for popup menu */
static void on_popup_open_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_open_with_another_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_run_app(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_open_in_new_tab_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_new_bookmark(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_handlers_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
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
static void on_popup_extract_list_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_new_folder_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_new_text_file_activate(GtkMenuItem* menuitem, PtkFileMenu* data);
static void on_popup_new_link_activate(GtkMenuItem* menuitem, PtkFileMenu* data);

static void on_popup_file_properties_activate(GtkMenuItem* menuitem, PtkFileMenu* data);

static void on_popup_file_permissions_activate(GtkMenuItem* menuitem, PtkFileMenu* data);

static void on_popup_open_all(GtkMenuItem* menuitem, PtkFileMenu* data);

static void on_popup_canon(GtkMenuItem* menuitem, PtkFileMenu* data);

void
on_popup_list_large(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    int p = browser->mypanel;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(browser->main_window);
    char mode = main_window->panel_context[p - 1];

    xset_set_b_panel_mode(p, "list_large", mode, xset_get_b_panel(p, "list_large"));
    update_views_all_windows(nullptr, browser);
}

void
on_popup_list_detailed(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    int p = browser->mypanel;

    if (xset_get_b_panel(p, "list_detailed"))
    {
        // setting b to XSetB::XSET_B_UNSET does not work here
        xset_set_b_panel(p, "list_icons", false);
        xset_set_b_panel(p, "list_compact", false);
    }
    else
    {
        if (!xset_get_b_panel(p, "list_icons") && !xset_get_b_panel(p, "list_compact"))
            xset_set_b_panel(p, "list_icons", true);
    }
    update_views_all_windows(nullptr, browser);
}

void
on_popup_list_icons(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    int p = browser->mypanel;

    if (xset_get_b_panel(p, "list_icons"))
    {
        // setting b to XSetB::XSET_B_UNSET does not work here
        xset_set_b_panel(p, "list_detailed", false);
        xset_set_b_panel(p, "list_compact", false);
    }
    else
    {
        if (!xset_get_b_panel(p, "list_detailed") && !xset_get_b_panel(p, "list_compact"))
            xset_set_b_panel(p, "list_detailed", true);
    }
    update_views_all_windows(nullptr, browser);
}

void
on_popup_list_compact(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    int p = browser->mypanel;

    if (xset_get_b_panel(p, "list_compact"))
    {
        // setting b to XSetB::XSET_B_UNSET does not work here
        xset_set_b_panel(p, "list_detailed", false);
        xset_set_b_panel(p, "list_icons", false);
    }
    else
    {
        if (!xset_get_b_panel(p, "list_icons") && !xset_get_b_panel(p, "list_detailed"))
            xset_set_b_panel(p, "list_detailed", true);
    }
    update_views_all_windows(nullptr, browser);
}

static void
on_popup_show_hidden(GtkMenuItem* menuitem, PtkFileBrowser* browser)
{
    (void)menuitem;
    if (browser)
        ptk_file_browser_show_hidden_files(browser,
                                           xset_get_b_panel(browser->mypanel, "show_hidden"));
}

static void
on_copycmd(GtkMenuItem* menuitem, PtkFileMenu* data, XSet* set2)
{
    XSet* set;
    if (menuitem)
        set = XSET(g_object_get_data(G_OBJECT(menuitem), "set"));
    else
        set = set2;
    if (!set)
        return;
    if (data->browser)
        ptk_file_browser_copycmd(data->browser, data->sel_files, data->cwd, set->name);
}

static void
on_popup_rootcmd_activate(GtkMenuItem* menuitem, PtkFileMenu* data, XSet* set2)
{
    XSet* set;
    if (menuitem)
        set = XSET(g_object_get_data(G_OBJECT(menuitem), "set"));
    else
        set = set2;
    if (set)
        ptk_file_misc_rootcmd(data->browser, data->sel_files, data->cwd, set->name);
}

static void
on_popup_select_pattern(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser)
        ptk_file_browser_select_pattern(nullptr, data->browser, nullptr);
}

static void
on_open_in_tab(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    int tab_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "tab_num"));
    if (data->browser)
        ptk_file_browser_open_in_tab(data->browser, tab_num, data->file_path);
}

static void
on_open_in_panel(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    int panel_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "panel_num"));
    if (data->browser)
        main_window_open_in_panel(data->browser, panel_num, data->file_path);
}

static void
on_file_edit(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    xset_edit(GTK_WIDGET(data->browser), data->file_path, false, true);
}

static void
on_file_root_edit(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    xset_edit(GTK_WIDGET(data->browser), data->file_path, true, false);
}

static void
on_popup_sort_extra(GtkMenuItem* menuitem, PtkFileBrowser* file_browser, XSet* set2)
{
    XSet* set;
    if (menuitem)
        set = XSET(g_object_get_data(G_OBJECT(menuitem), "set"));
    else
        set = set2;
    ptk_file_browser_set_sort_extra(file_browser, set->name);
}

void
on_popup_sortby(GtkMenuItem* menuitem, PtkFileBrowser* file_browser, int order)
{
    int v;

    int sort_order;
    if (menuitem)
        sort_order = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "sortorder"));
    else
        sort_order = order;

    if (sort_order < 0)
    {
        if (sort_order == -1)
            v = GTK_SORT_ASCENDING;
        else
            v = GTK_SORT_DESCENDING;
        xset_set_panel(file_browser->mypanel,
                       "list_detailed",
                       XSetSetSet::Y,
                       std::to_string(v).c_str());
        ptk_file_browser_set_sort_type(file_browser, (GtkSortType)v);
    }
    else
    {
        xset_set_panel(file_browser->mypanel,
                       "list_detailed",
                       XSetSetSet::X,
                       std::to_string(sort_order).c_str());
        ptk_file_browser_set_sort_order(file_browser, (PtkFBSortOrder)sort_order);
    }
}

static void
on_popup_detailed_column(GtkMenuItem* menuitem, PtkFileBrowser* file_browser)
{
    (void)menuitem;
    if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
    {
        // get visiblity for correct mode
        FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
        int p = file_browser->mypanel;
        char mode = main_window->panel_context[p - 1];

        XSet* set = xset_get_panel_mode(p, "detcol_size", mode);
        set->b = xset_get_panel(p, "detcol_size")->b;
        set = xset_get_panel_mode(p, "detcol_type", mode);
        set->b = xset_get_panel(p, "detcol_type")->b;
        set = xset_get_panel_mode(p, "detcol_perm", mode);
        set->b = xset_get_panel(p, "detcol_perm")->b;
        set = xset_get_panel_mode(p, "detcol_owner", mode);
        set->b = xset_get_panel(p, "detcol_owner")->b;
        set = xset_get_panel_mode(p, "detcol_date", mode);
        set->b = xset_get_panel(p, "detcol_date")->b;

        update_views_all_windows(nullptr, file_browser);
    }
}

static void
on_popup_toggle_view(GtkMenuItem* menuitem, PtkFileBrowser* file_browser)
{
    (void)menuitem;
    // get visiblity for correct mode
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p - 1];

    XSet* set = xset_get_panel_mode(p, "show_toolbox", mode);
    set->b = xset_get_panel(p, "show_toolbox")->b;
    set = xset_get_panel_mode(p, "show_devmon", mode);
    set->b = xset_get_panel(p, "show_devmon")->b;
    set = xset_get_panel_mode(p, "show_dirtree", mode);
    set->b = xset_get_panel(p, "show_dirtree")->b;
    set = xset_get_panel_mode(p, "show_book", mode);
    set->b = xset_get_panel(p, "show_book")->b;
    set = xset_get_panel_mode(p, "show_sidebar", mode);
    set->b = xset_get_panel(p, "show_sidebar")->b;

    update_views_all_windows(nullptr, file_browser);
}

static void
on_archive_default(GtkMenuItem* menuitem, XSet* set)
{
    (void)menuitem;
    const std::array<std::string, 4> arcnames{"arc_def_open",
                                              "arc_def_ex",
                                              "arc_def_exto",
                                              "arc_def_list"};
    for (const std::string& arcname: arcnames)
    {
        if (ztd::same(set->name, arcname))
            set->b = XSetB::XSET_B_TRUE;
        else
            xset_set_b(arcname, false);
    }
}

static void
on_archive_show_config(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    ptk_handler_show_config(PtkHandlerMode::HANDLER_MODE_ARC, data->browser, nullptr);
}

static void
on_hide_file(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser)
        ptk_file_browser_hide_selected(data->browser, data->sel_files, data->cwd);
}

static void
on_permission(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    if (data->browser)
        ptk_file_browser_on_permission(menuitem, data->browser, data->sel_files, data->cwd);
}

void
ptk_file_menu_add_panel_view_menu(PtkFileBrowser* browser, GtkWidget* menu,
                                  GtkAccelGroup* accel_group)
{
    XSet* set;
    XSet* set_radio;
    std::string desc;

    if (!browser || !menu || !browser->file_list)
        return;
    int p = browser->mypanel;

    FMMainWindow* main_window = static_cast<FMMainWindow*>(browser->main_window);
    char mode = main_window->panel_context[p - 1];

    bool show_side = false;
    xset_set_cb("view_refresh", (GFunc)ptk_file_browser_refresh, browser);
    set = xset_set_cb_panel(p, "show_toolbox", (GFunc)on_popup_toggle_view, browser);
    set->b = xset_get_panel_mode(p, "show_toolbox", mode)->b;
    set = xset_set_cb_panel(p, "show_devmon", (GFunc)on_popup_toggle_view, browser);
    set->b = xset_get_panel_mode(p, "show_devmon", mode)->b;
    if (set->b == XSetB::XSET_B_TRUE)
        show_side = true;
    set = xset_set_cb_panel(p, "show_dirtree", (GFunc)on_popup_toggle_view, browser);
    set->b = xset_get_panel_mode(p, "show_dirtree", mode)->b;
    if (set->b == XSetB::XSET_B_TRUE)
        show_side = true;
    set = xset_set_cb_panel(p, "show_book", (GFunc)on_popup_toggle_view, browser);
    set->b = xset_get_panel_mode(p, "show_book", mode)->b;
    if (set->b == XSetB::XSET_B_TRUE)
        show_side = true;
    set = xset_set_cb_panel(p, "show_sidebar", (GFunc)on_popup_toggle_view, browser);
    set->b = xset_get_panel_mode(p, "show_sidebar", mode)->b;
    set->disable = !show_side;
    xset_set_cb_panel(p, "show_hidden", (GFunc)on_popup_show_hidden, browser);

    if (browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
    {
        set = xset_set_cb_panel(p, "detcol_size", (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, "detcol_size", mode)->b;
        set = xset_set_cb_panel(p, "detcol_type", (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, "detcol_type", mode)->b;
        set = xset_set_cb_panel(p, "detcol_perm", (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, "detcol_perm", mode)->b;
        set = xset_set_cb_panel(p, "detcol_owner", (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, "detcol_owner", mode)->b;
        set = xset_set_cb_panel(p, "detcol_date", (GFunc)on_popup_detailed_column, browser);
        set->b = xset_get_panel_mode(p, "detcol_date", mode)->b;

        xset_set_cb("view_reorder_col", (GFunc)on_reorder, browser);
        set = xset_set("view_columns", XSetSetSet::DISABLE, "0");
        desc = fmt::format("panel{}_detcol_size panel{}_detcol_type panel{}_detcol_perm "
                           "panel{}_detcol_owner panel{}_detcol_date separator view_reorder_col",
                           p,
                           p,
                           p,
                           p,
                           p);
        xset_set_set(set, XSetSetSet::DESC, desc.c_str());
        set = xset_set_cb("rubberband", (GFunc)main_window_rubberband_all, nullptr);
        set->disable = false;
    }
    else
    {
        xset_set("view_columns", XSetSetSet::DISABLE, "1");
        xset_set("rubberband", XSetSetSet::DISABLE, "1");
    }

    set = xset_set_cb("view_thumb", (GFunc)main_window_toggle_thumbnails_all_windows, nullptr);
    set->b = app_settings.show_thumbnail ? XSetB::XSET_B_TRUE : XSetB::XSET_B_UNSET;

    if (browser->view_mode == PtkFBViewMode::PTK_FB_ICON_VIEW)
    {
        set = xset_set_b_panel(p, "list_large", true);
        set->disable = true;
    }
    else
    {
        set = xset_set_cb_panel(p, "list_large", (GFunc)on_popup_list_large, browser);
        set->disable = false;
        set->b = xset_get_panel_mode(p, "list_large", mode)->b;
    }

    set = xset_set_cb_panel(p, "list_detailed", (GFunc)on_popup_list_detailed, browser);
    xset_set_ob2(set, nullptr, nullptr);
    set_radio = set;
    set = xset_set_cb_panel(p, "list_icons", (GFunc)on_popup_list_icons, browser);
    xset_set_ob2(set, nullptr, set_radio);
    set = xset_set_cb_panel(p, "list_compact", (GFunc)on_popup_list_compact, browser);
    xset_set_ob2(set, nullptr, set_radio);

    set = xset_set_cb("sortby_name", (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", PtkFBSortOrder::PTK_FB_SORT_BY_NAME);
    xset_set_ob2(set, nullptr, nullptr);
    set->b = browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_NAME ? XSetB::XSET_B_TRUE
                                                                        : XSetB::XSET_B_FALSE;
    set_radio = set;
    set = xset_set_cb("sortby_size", (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", PtkFBSortOrder::PTK_FB_SORT_BY_SIZE);
    xset_set_ob2(set, nullptr, set_radio);
    set->b = browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_SIZE ? XSetB::XSET_B_TRUE
                                                                        : XSetB::XSET_B_FALSE;
    set = xset_set_cb("sortby_type", (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", PtkFBSortOrder::PTK_FB_SORT_BY_TYPE);
    xset_set_ob2(set, nullptr, set_radio);
    set->b = browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_TYPE ? XSetB::XSET_B_TRUE
                                                                        : XSetB::XSET_B_FALSE;
    set = xset_set_cb("sortby_perm", (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", PtkFBSortOrder::PTK_FB_SORT_BY_PERM);
    xset_set_ob2(set, nullptr, set_radio);
    set->b = browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_PERM ? XSetB::XSET_B_TRUE
                                                                        : XSetB::XSET_B_FALSE;
    set = xset_set_cb("sortby_owner", (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", PtkFBSortOrder::PTK_FB_SORT_BY_OWNER);
    xset_set_ob2(set, nullptr, set_radio);
    set->b = browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_OWNER ? XSetB::XSET_B_TRUE
                                                                         : XSetB::XSET_B_FALSE;
    set = xset_set_cb("sortby_date", (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", PtkFBSortOrder::PTK_FB_SORT_BY_MTIME);
    xset_set_ob2(set, nullptr, set_radio);
    set->b = browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_MTIME ? XSetB::XSET_B_TRUE
                                                                         : XSetB::XSET_B_FALSE;

    set = xset_set_cb("sortby_ascend", (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", -1);
    xset_set_ob2(set, nullptr, nullptr);
    set->b = browser->sort_type == GTK_SORT_ASCENDING ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;
    set_radio = set;
    set = xset_set_cb("sortby_descend", (GFunc)on_popup_sortby, browser);
    xset_set_ob1_int(set, "sortorder", -2);
    xset_set_ob2(set, nullptr, set_radio);
    set->b = browser->sort_type == GTK_SORT_DESCENDING ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;

    // this crashes if !browser->file_list so do not allow
    if (browser->file_list)
    {
        set = xset_set_cb("sortx_alphanum", (GFunc)on_popup_sort_extra, browser);
        set->b = PTK_FILE_LIST(browser->file_list)->sort_alphanum ? XSetB::XSET_B_TRUE
                                                                  : XSetB::XSET_B_FALSE;
        set = xset_set_cb("sortx_case", (GFunc)on_popup_sort_extra, browser);
        set->b =
            PTK_FILE_LIST(browser->file_list)->sort_case ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;
        set->disable = !PTK_FILE_LIST(browser->file_list)->sort_alphanum;

#if 0
        set = xset_set_cb("sortx_natural", (GFunc)on_popup_sort_extra, browser);
        set->b = PTK_FILE_LIST(browser->file_list)->sort_natural ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;
        set = xset_set_cb("sortx_case", (GFunc)on_popup_sort_extra, browser);
        set->b = PTK_FILE_LIST(browser->file_list)->sort_case ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;
        set->disable = !PTK_FILE_LIST(browser->file_list)->sort_natural;
#endif

        set = xset_set_cb("sortx_directories", (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, nullptr);
        set->b = PTK_FILE_LIST(browser->file_list)->sort_dir ==
                         PTKFileListSortDir::PTK_LIST_SORT_DIR_FIRST
                     ? XSetB::XSET_B_TRUE
                     : XSetB::XSET_B_FALSE;
        set_radio = set;
        set = xset_set_cb("sortx_files", (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, set_radio);
        set->b = PTK_FILE_LIST(browser->file_list)->sort_dir ==
                         PTKFileListSortDir::PTK_LIST_SORT_DIR_LAST
                     ? XSetB::XSET_B_TRUE
                     : XSetB::XSET_B_FALSE;
        set = xset_set_cb("sortx_mix", (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, set_radio);
        set->b = PTK_FILE_LIST(browser->file_list)->sort_dir ==
                         PTKFileListSortDir::PTK_LIST_SORT_DIR_MIXED
                     ? XSetB::XSET_B_TRUE
                     : XSetB::XSET_B_FALSE;

        set = xset_set_cb("sortx_hidfirst", (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, nullptr);
        set->b = PTK_FILE_LIST(browser->file_list)->sort_hidden_first ? XSetB::XSET_B_TRUE
                                                                      : XSetB::XSET_B_FALSE;
        set_radio = set;
        set = xset_set_cb("sortx_hidlast", (GFunc)on_popup_sort_extra, browser);
        xset_set_ob2(set, nullptr, set_radio);
        set->b = PTK_FILE_LIST(browser->file_list)->sort_hidden_first ? XSetB::XSET_B_FALSE
                                                                      : XSetB::XSET_B_TRUE;
    }

    set = xset_get("view_list_style");
    desc = fmt::format("panel{}_list_detailed panel{}_list_compact panel{}_list_icons separator "
                       "view_thumb panel{}_list_large rubberband",
                       p,
                       p,
                       p,
                       p);
    xset_set_set(set, XSetSetSet::DESC, desc.c_str());
    set = xset_get("con_view");
    set->disable = !browser->file_list;
    desc = fmt::format("panel{}_show_toolbox panel{}_show_sidebar panel{}_show_devmon "
                       "panel{}_show_book panel{}_show_dirtree separator panel{}_show_hidden "
                       "view_list_style view_sortby view_columns separator view_refresh",
                       p,
                       p,
                       p,
                       p,
                       p,
                       p);
    xset_set_set(set, XSetSetSet::DESC, desc.c_str());
    xset_add_menuitem(browser, menu, accel_group, set);
}

static void
ptk_file_menu_free(PtkFileMenu* data)
{
    if (data->file_path)
        free(data->file_path);
    if (data->info)
        vfs_file_info_unref(data->info);
    free(data->cwd);
    if (data->sel_files)
        vfs_file_info_list_free(data->sel_files);
    if (data->accel_group)
        g_object_unref(data->accel_group);
    g_slice_free(PtkFileMenu, data);
}

/* Retrieve popup menu for selected file(s) */
GtkWidget*
ptk_file_menu_new(PtkFileBrowser* browser, const char* file_path, VFSFileInfo* info,
                  const char* cwd, GList* sel_files)
{ // either desktop or browser must be non-nullptr

    const char* app_name = nullptr;
    char* desc;
    XSet* set_radio;
    int icon_w;
    int icon_h;
    PtkFileMenu* data;
    int no_write_access = 0;
    int no_read_access = 0;
    XSet* set;
    XSet* set2;
    GtkMenuItem* item;
    GSList* handlers_slist;

    if (!browser)
        return nullptr;

    data = g_slice_new0(PtkFileMenu);

    data->cwd = ztd::strdup(cwd);
    data->browser = browser;

    data->file_path = ztd::strdup(file_path);
    if (info)
        data->info = vfs_file_info_ref(info);
    else
        data->info = nullptr;
    data->sel_files = sel_files;

    data->accel_group = gtk_accel_group_new();

    GtkWidget* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    g_object_weak_ref(G_OBJECT(popup), (GWeakNotify)ptk_file_menu_free, data);
    g_signal_connect_after((void*)popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);

    // is_dir = file_path && std::filesystem::is_directory(file_path);
    bool is_dir = (info && vfs_file_info_is_dir(info));
    // Note: network filesystems may become unresponsive here
    bool is_text = info && file_path && vfs_file_info_is_text(info, file_path);

    // test R/W access to cwd instead of selected file
    // Note: network filesystems may become unresponsive here
    no_read_access = faccessat(0, cwd, R_OK, AT_EACCESS);
    no_write_access = faccessat(0, cwd, W_OK, AT_EACCESS);

    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    bool is_clip;
    if (!gtk_clipboard_wait_is_target_available(
            clip,
            gdk_atom_intern("x-special/gnome-copied-files", false)) &&
        !gtk_clipboard_wait_is_target_available(clip, gdk_atom_intern("text/uri-list", false)))
        is_clip = false;
    else
        is_clip = true;

    int p = 0;
    int tab_count = 0;
    int tab_num = 0;
    int panel_count = 0;
    if (browser)
    {
        p = browser->mypanel;
        main_window_get_counts(browser, &panel_count, &tab_count, &tab_num);
    }

    XSetContext* context = xset_context_new();

    // Get mime type and apps
    VFSMimeType* mime_type;
    std::vector<std::string> apps;
    if (info)
    {
        mime_type = vfs_file_info_get_mime_type(info);
        apps = vfs_mime_type_get_actions(mime_type);
        context->var[ItemPropContext::CONTEXT_MIME] =
            ztd::strdup(vfs_mime_type_get_type(mime_type));
    }
    else
    {
        mime_type = nullptr;
        context->var[ItemPropContext::CONTEXT_MIME] = ztd::strdup("");
    }

    // context
    if (file_path)
        context->var[ItemPropContext::CONTEXT_NAME] = g_path_get_basename(file_path);
    else
        context->var[ItemPropContext::CONTEXT_NAME] = ztd::strdup("");
    context->var[ItemPropContext::CONTEXT_DIR] = ztd::strdup(cwd);
    context->var[ItemPropContext::CONTEXT_READ_ACCESS] =
        no_read_access ? ztd::strdup("false") : ztd::strdup("true");
    context->var[ItemPropContext::CONTEXT_WRITE_ACCESS] =
        no_write_access ? ztd::strdup("false") : ztd::strdup("true");
    context->var[ItemPropContext::CONTEXT_IS_TEXT] =
        is_text ? ztd::strdup("true") : ztd::strdup("false");
    context->var[ItemPropContext::CONTEXT_IS_DIR] =
        is_dir ? ztd::strdup("true") : ztd::strdup("false");
    context->var[ItemPropContext::CONTEXT_MUL_SEL] =
        sel_files && sel_files->next ? ztd::strdup("true") : ztd::strdup("false");
    context->var[ItemPropContext::CONTEXT_CLIP_FILES] =
        is_clip ? ztd::strdup("true") : ztd::strdup("false");
    if (info)
        context->var[ItemPropContext::CONTEXT_IS_LINK] =
            vfs_file_info_is_symlink(info) ? ztd::strdup("true") : ztd::strdup("false");
    else
        context->var[ItemPropContext::CONTEXT_IS_LINK] = ztd::strdup("false");

    if (browser)
        main_context_fill(browser, context);

    if (!context->valid)
    {
        // rare exception due to context_fill hacks - fb was probably destroyed
        LOG_WARN("context_fill rare exception");
        context = xset_context_new();
        g_slice_free(XSetContext, context);
        context = nullptr;
        return nullptr;
    }

    // Open >
    set = xset_get("con_open");
    set->disable = !(sel_files);
    item = GTK_MENU_ITEM(xset_add_menuitem(browser, popup, accel_group, set));
    if (sel_files)
    {
        GtkWidget* submenu = gtk_menu_item_get_submenu(item);

        // Execute
        if (!is_dir && info && file_path &&
            (info->flags & VFSFileInfoFlag::VFS_FILE_INFO_DESKTOP_ENTRY ||
             // Note: network filesystems may become unresponsive here
             vfs_file_info_is_executable(info, file_path)))
        {
            set = xset_set_cb("open_execute", (GFunc)on_popup_open_activate, data);
            xset_add_menuitem(browser, submenu, accel_group, set);
        }

        // Prepare archive commands
        XSet* set_arc_extract = nullptr;
        XSet* set_arc_extractto;
        XSet* set_arc_list;
        handlers_slist = ptk_handler_file_has_handlers(PtkHandlerMode::HANDLER_MODE_ARC,
                                                       PtkHandlerArchive::HANDLER_EXTRACT,
                                                       file_path,
                                                       mime_type,
                                                       false,
                                                       false,
                                                       false);
        if (handlers_slist)
        {
            g_slist_free(handlers_slist);

            set_arc_extract =
                xset_set_cb("arc_extract", (GFunc)on_popup_extract_here_activate, data);
            xset_set_ob1(set_arc_extract, "set", set_arc_extract);
            set_arc_extract->disable = no_write_access;

            set_arc_extractto =
                xset_set_cb("arc_extractto", (GFunc)on_popup_extract_to_activate, data);
            xset_set_ob1(set_arc_extractto, "set", set_arc_extractto);

            set_arc_list = xset_set_cb("arc_list", (GFunc)on_popup_extract_list_activate, data);
            xset_set_ob1(set_arc_list, "set", set_arc_list);

            set = xset_get("arc_def_open");
            // do NOT use set = xset_set_cb here or wrong set is passed
            xset_set_cb("arc_def_open", (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, nullptr);
            set_radio = set;

            set = xset_get("arc_def_ex");
            xset_set_cb("arc_def_ex", (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio);

            set = xset_get("arc_def_exto");
            xset_set_cb("arc_def_exto", (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio);

            set = xset_get("arc_def_list");
            xset_set_cb("arc_def_list", (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio);

            set = xset_get("arc_def_write");
            set->disable = geteuid() == 0 || !xset_get_b("arc_def_parent");

            xset_set_cb("arc_conf2", (GFunc)on_archive_show_config, data);

            if (!xset_get_b("arc_def_open"))
            {
                // archives are not set to open with app, so list archive
                // functions before file handlers and associated apps

                // list active function first
                if (xset_get_b("arc_def_ex"))
                {
                    xset_add_menuitem(browser, submenu, accel_group, set_arc_extract);
                    set_arc_extract = nullptr;
                }
                else if (xset_get_b("arc_def_exto"))
                {
                    xset_add_menuitem(browser, submenu, accel_group, set_arc_extractto);
                    set_arc_extractto = nullptr;
                }
                else
                {
                    xset_add_menuitem(browser, submenu, accel_group, set_arc_list);
                    set_arc_list = nullptr;
                }

                // add others
                if (set_arc_extract)
                    xset_add_menuitem(browser, submenu, accel_group, set_arc_extract);
                if (set_arc_extractto)
                    xset_add_menuitem(browser, submenu, accel_group, set_arc_extractto);
                if (set_arc_list)
                    xset_add_menuitem(browser, submenu, accel_group, set_arc_list);
                xset_add_menuitem(browser, submenu, accel_group, xset_get("arc_default"));
                set_arc_extract = nullptr;

                // separator
                item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
                gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));
            }
        }

        // file handlers
        handlers_slist = ptk_handler_file_has_handlers(PtkHandlerMode::HANDLER_MODE_FILE,
                                                       PtkHandlerMount::HANDLER_MOUNT,
                                                       file_path,
                                                       mime_type,
                                                       false,
                                                       true,
                                                       false);

        GtkWidget* app_menu_item;
        if (handlers_slist)
        {
            GSList* sl;
            for (sl = handlers_slist; sl; sl = sl->next)
            {
                set = XSET(sl->data);
                app_menu_item = gtk_menu_item_new_with_label(set->menu_label);
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
                g_object_set_data(G_OBJECT(app_menu_item), "handler_set", set);
            }
            g_slist_free(handlers_slist);
            // add a separator
            item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
            gtk_widget_show(GTK_WIDGET(item));
            gtk_container_add(GTK_CONTAINER(submenu), GTK_WIDGET(item));
        }

        // add apps
        gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &icon_w, &icon_h);
        if (is_text)
        {
            std::vector<std::string> txt_apps;
            VFSMimeType* txt_type;
            txt_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_PLAIN_TEXT);
            txt_apps = vfs_mime_type_get_actions(txt_type);
            if (!txt_apps.empty())
                apps = ztd::merge(apps, txt_apps);
            vfs_mime_type_unref(txt_type);
        }
        if (!apps.empty())
        {
            for (const std::string& app: apps)
            {
#if 0
                // TODO - FIXME
                if ((app - apps) == 1 && !handlers_slist)
                {
                    // Add a separator after default app if no handlers listed
                    item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
                    gtk_widget_show(GTK_WIDGET(item));
                    gtk_container_add(GTK_CONTAINER(submenu), GTK_WIDGET(item));
                }
#endif

                VFSAppDesktop desktop(app);
                app_name = desktop.get_disp_name();
                if (app_name)
                    app_menu_item = gtk_menu_item_new_with_label(app_name);
                else
                    app_menu_item = gtk_menu_item_new_with_label(app.c_str());

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
                                       ztd::strdup(app),
                                       free);
            }
        }

        // open with other
        item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));

        set = xset_set_cb("open_other", (GFunc)on_popup_open_with_another_activate, data);
        xset_add_menuitem(browser, submenu, accel_group, set);

        set = xset_set_cb("open_hand", (GFunc)on_popup_handlers_activate, data);
        xset_add_menuitem(browser, submenu, accel_group, set);

        // Default
        std::string plain_type = "";
        if (mime_type)
            plain_type = ztd::strdup(vfs_mime_type_get_type(mime_type));
        plain_type = ztd::replace(plain_type, "-", "_");
        plain_type = ztd::replace(plain_type, " ", "");
        plain_type = fmt::format("open_all_type_{}", plain_type);
        set = xset_set_cb(plain_type.c_str(), (GFunc)on_popup_open_all, data);
        set->lock = true;
        set->menu_style = XSetMenu::NORMAL;
        if (set->shared_key)
            free(set->shared_key);
        set->shared_key = ztd::strdup("open_all");
        set2 = xset_get("open_all");
        if (set->menu_label)
            free(set->menu_label);
        set->menu_label = ztd::strdup(set2->menu_label);
        if (set->context)
        {
            free(set->context);
            set->context = nullptr;
        }
        item = GTK_MENU_ITEM(xset_add_menuitem(browser, submenu, accel_group, set));
        if (set->menu_label)
            free(set->menu_label);
        set->menu_label = nullptr; // do not bother to save this

        // Edit / Dir
        if ((is_dir && browser) || (is_text && sel_files && !sel_files->next))
        {
            item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));

            if (is_text)
            {
                // Edit
                set = xset_set_cb("open_edit", (GFunc)on_file_edit, data);
                set->disable = (geteuid() == 0);
                xset_add_menuitem(browser, submenu, accel_group, set);
                set = xset_set_cb("open_edit_root", (GFunc)on_file_root_edit, data);
                xset_add_menuitem(browser, submenu, accel_group, set);
            }
            else if (browser && is_dir)
            {
                // Open Dir
                set = xset_set_cb("opentab_prev", (GFunc)on_open_in_tab, data);
                xset_set_ob1_int(set, "tab_num", -1);
                set->disable = (tab_num == 1);
                set = xset_set_cb("opentab_next", (GFunc)on_open_in_tab, data);
                xset_set_ob1_int(set, "tab_num", -2);
                set->disable = (tab_num == tab_count);
                set = xset_set_cb("opentab_new", (GFunc)on_popup_open_in_new_tab_activate, data);
                for (int i = 1; i < 11; i++)
                {
                    std::string name = fmt::format("opentab_{}", i);
                    set = xset_set_cb(name, (GFunc)on_open_in_tab, data);
                    xset_set_ob1_int(set, "tab_num", i);
                    set->disable = (i > tab_count) || (i == tab_num);
                }

                set = xset_set_cb("open_in_panelprev", (GFunc)on_open_in_panel, data);
                xset_set_ob1_int(set, "panel_num", -1);
                set->disable = (panel_count == 1);
                set = xset_set_cb("open_in_panelnext", (GFunc)on_open_in_panel, data);
                xset_set_ob1_int(set, "panel_num", -2);
                set->disable = (panel_count == 1);

                for (int i = 1; i < 5; i++)
                {
                    std::string name = fmt::format("open_in_panel{}", i);
                    set = xset_set_cb(name, (GFunc)on_open_in_panel, data);
                    xset_set_ob1_int(set, "panel_num", i);
                    // set->disable = ( p == i );
                }

                set = xset_get("open_in_tab");
                xset_add_menuitem(browser, submenu, accel_group, set);
                set = xset_get("open_in_panel");
                xset_add_menuitem(browser, submenu, accel_group, set);
            }
        }

        if (set_arc_extract)
        {
            // archives are set to open with app, so list archive
            // functions after associated apps

            // separator
            item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));

            xset_add_menuitem(browser, submenu, accel_group, set_arc_extract);
            xset_add_menuitem(browser, submenu, accel_group, set_arc_extractto);
            xset_add_menuitem(browser, submenu, accel_group, set_arc_list);
            xset_add_menuitem(browser, submenu, accel_group, xset_get("arc_default"));
        }

        g_signal_connect(submenu, "key-press-event", G_CALLBACK(app_menu_keypress), data);
    }

    if (mime_type)
        vfs_mime_type_unref(mime_type);

    // Go >
    if (browser)
    {
        set = xset_set_cb("go_back", (GFunc)ptk_file_browser_go_back, browser);
        set->disable = !(browser->curHistory && browser->curHistory->prev);
        set = xset_set_cb("go_forward", (GFunc)ptk_file_browser_go_forward, browser);
        set->disable = !(browser->curHistory && browser->curHistory->next);
        set = xset_set_cb("go_up", (GFunc)ptk_file_browser_go_up, browser);
        set->disable = !strcmp(cwd, "/");
        xset_set_cb("go_home", (GFunc)ptk_file_browser_go_home, browser);
        xset_set_cb("go_default", (GFunc)ptk_file_browser_go_default, browser);
        xset_set_cb("go_set_default", (GFunc)ptk_file_browser_set_default_folder, browser);
        xset_set_cb("edit_canon", (GFunc)on_popup_canon, data);
        xset_set_cb("go_refresh", (GFunc)ptk_file_browser_refresh, browser);
        set = xset_set_cb("focus_path_bar", (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 0);
        set = xset_set_cb("focus_filelist", (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 4);
        set = xset_set_cb("focus_dirtree", (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 1);
        set = xset_set_cb("focus_book", (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 2);
        set = xset_set_cb("focus_device", (GFunc)ptk_file_browser_focus, browser);
        xset_set_ob1_int(set, "job", 3);

        // Go > Tab >
        set = xset_set_cb("tab_prev", (GFunc)ptk_file_browser_go_tab, browser);
        xset_set_ob1_int(set, "tab_num", -1);
        set->disable = (tab_count < 2);
        set = xset_set_cb("tab_next", (GFunc)ptk_file_browser_go_tab, browser);
        xset_set_ob1_int(set, "tab_num", -2);
        set->disable = (tab_count < 2);
        set = xset_set_cb("tab_close", (GFunc)ptk_file_browser_go_tab, browser);
        xset_set_ob1_int(set, "tab_num", -3);
        set = xset_set_cb("tab_restore", (GFunc)ptk_file_browser_go_tab, browser);
        xset_set_ob1_int(set, "tab_num", -4);

        for (int i = 1; i < 11; i++)
        {
            std::string name = fmt::format("tab_{}", i);
            set = xset_set_cb(name, (GFunc)ptk_file_browser_go_tab, browser);
            xset_set_ob1_int(set, "tab_num", i);
            set->disable = (i > tab_count) || (i == tab_num);
        }
        set = xset_get("con_go");
        xset_add_menuitem(browser, popup, accel_group, set);

        // New >
        set = xset_set_cb("new_file", (GFunc)on_popup_new_text_file_activate, data);
        set = xset_set_cb("new_directory", (GFunc)on_popup_new_folder_activate, data);
        set = xset_set_cb("new_link", (GFunc)on_popup_new_link_activate, data);
        set = xset_set_cb("new_archive", (GFunc)on_popup_compress_activate, data);
        set->disable = (!sel_files);

        set = xset_set_cb("tab_new", (GFunc)ptk_file_browser_new_tab, browser);
        set->disable = !browser;
        set = xset_set_cb("tab_new_here", (GFunc)on_popup_open_in_new_tab_here, data);
        set->disable = !browser;
        set = xset_set_cb("new_bookmark", (GFunc)on_new_bookmark, data);
        set->disable = !browser;

        set = xset_get("open_new");
        xset_set_set(set,
                     XSetSetSet::DESC,
                     "new_file new_directory new_link new_archive separator tab_new tab_new_here "
                     "new_bookmark");

        xset_add_menuitem(browser, popup, accel_group, set);

        set = xset_get("separator");
        xset_add_menuitem(browser, popup, accel_group, set);

        // Edit
        set = xset_set_cb("copy_name", (GFunc)on_popup_copy_name_activate, data);
        set->disable = !sel_files;
        set = xset_set_cb("copy_path", (GFunc)on_popup_copy_text_activate, data);
        set->disable = !sel_files;
        set = xset_set_cb("copy_parent", (GFunc)on_popup_copy_parent_activate, data);
        set->disable = !sel_files;
        set = xset_set_cb("paste_link", (GFunc)on_popup_paste_link_activate, data);
        set->disable = !is_clip || no_write_access;
        set = xset_set_cb("paste_target", (GFunc)on_popup_paste_target_activate, data);
        set->disable = !is_clip || no_write_access;

        set = xset_set_cb("paste_as", (GFunc)on_popup_paste_as_activate, data);
        set->disable = !is_clip;

        set = xset_set_cb("root_copy_loc", (GFunc)on_popup_rootcmd_activate, data);
        xset_set_ob1(set, "set", set);
        set->disable = !sel_files;
        set = xset_set_cb("root_move2", (GFunc)on_popup_rootcmd_activate, data);
        xset_set_ob1(set, "set", set);
        set->disable = !sel_files;
        set = xset_set_cb("root_delete", (GFunc)on_popup_rootcmd_activate, data);
        xset_set_ob1(set, "set", set);
        set->disable = !sel_files;

        set = xset_set_cb("edit_hide", (GFunc)on_hide_file, data);
        set->disable = !sel_files || no_write_access || !browser;

        xset_set_cb("select_all", (GFunc)ptk_file_browser_select_all, data->browser);
        set = xset_set_cb("select_un", (GFunc)ptk_file_browser_unselect_all, browser);
        set->disable = !sel_files;
        xset_set_cb("select_invert", (GFunc)ptk_file_browser_invert_selection, browser);
        xset_set_cb("select_patt", (GFunc)on_popup_select_pattern, data);

        const std::array<std::string, 40> copycmds{
            "copy_loc",        "copy_loc_last", "copy_tab_prev", "copy_tab_next", "copy_tab_1",
            "copy_tab_2",      "copy_tab_3",    "copy_tab_4",    "copy_tab_5",    "copy_tab_6",
            "copy_tab_7",      "copy_tab_8",    "copy_tab_9",    "copy_tab_10",   "copy_panel_prev",
            "copy_panel_next", "copy_panel_1",  "copy_panel_2",  "copy_panel_3",  "copy_panel_4",
            "move_loc",        "move_loc_last", "move_tab_prev", "move_tab_next", "move_tab_1",
            "move_tab_2",      "move_tab_3",    "move_tab_4",    "move_tab_5",    "move_tab_6",
            "move_tab_7",      "move_tab_8",    "move_tab_9",    "move_tab_10",   "move_panel_prev",
            "move_panel_next", "move_panel_1",  "move_panel_2",  "move_panel_3",  "move_panel_4"};
        for (const std::string& copycmd: copycmds)
        {
            set = xset_set_cb(copycmd, (GFunc)on_copycmd, data);
            xset_set_ob1(set, "set", set);
        }

        // enables
        set = xset_get("copy_loc_last");
        set2 = xset_get("move_loc_last");

        set = xset_get("copy_tab_prev");
        set->disable = (tab_num == 1);
        set = xset_get("copy_tab_next");
        set->disable = (tab_num == tab_count);
        set = xset_get("move_tab_prev");
        set->disable = (tab_num == 1);
        set = xset_get("move_tab_next");
        set->disable = (tab_num == tab_count);

        set = xset_get("copy_panel_prev");
        set->disable = (panel_count < 2);
        set = xset_get("copy_panel_next");
        set->disable = (panel_count < 2);
        set = xset_get("move_panel_prev");
        set->disable = (panel_count < 2);
        set = xset_get("move_panel_next");
        set->disable = (panel_count < 2);

        bool b;
        for (int i = 1; i < 11; i++)
        {
            std::string str;
            str = fmt::format("copy_tab_{}", i);
            set = xset_get(str);
            set->disable = (i > tab_count) || (i == tab_num);

            str = fmt::format("move_tab_{}", i);
            set = xset_get(str);
            set->disable = (i > tab_count) || (i == tab_num);

            if (i > 4)
                continue;

            b = main_window_panel_is_visible(browser, i);

            str = fmt::format("copy_panel_{}", i);
            set = xset_get(str);
            set->disable = (i == p) || !b;

            str = fmt::format("move_panel_{}", i);
            set = xset_get(str);
            set->disable = (i == p) || !b;
        }

        set = xset_get("copy_to");
        set->disable = !sel_files;

        set = xset_get("move_to");
        set->disable = !sel_files;

        set = xset_get("edit_root");
        set->disable = (geteuid() == 0) || !sel_files;

        set = xset_get("edit_submenu");
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    set = xset_set_cb("edit_cut", (GFunc)on_popup_cut_activate, data);
    set->disable = !sel_files;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_set_cb("edit_copy", (GFunc)on_popup_copy_activate, data);
    set->disable = !sel_files;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_set_cb("edit_paste", (GFunc)on_popup_paste_activate, data);
    set->disable = !is_clip || no_write_access;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_set_cb("edit_rename", (GFunc)on_popup_rename_activate, data);
    set->disable = !sel_files;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_set_cb("edit_trash", (GFunc)on_popup_trash_activate, data);
    set->disable = !sel_files || no_write_access;
    xset_add_menuitem(browser, popup, accel_group, set);
    set = xset_set_cb("edit_delete", (GFunc)on_popup_delete_activate, data);
    set->disable = !sel_files || no_write_access;
    xset_add_menuitem(browser, popup, accel_group, set);

    set = xset_get("separator");
    xset_add_menuitem(browser, popup, accel_group, set);

    if (browser)
    {
        // View >
        ptk_file_menu_add_panel_view_menu(browser, popup, accel_group);

        // Properties
        set = xset_set_cb("prop_info", (GFunc)on_popup_file_properties_activate, data);
        set = xset_set_cb("prop_perm", (GFunc)on_popup_file_permissions_activate, data);

        const std::array<std::string, 63> permcmds{
            "perm_r",           "perm_rw",           "perm_rwx",         "perm_r_r",
            "perm_rw_r",        "perm_rw_rw",        "perm_rwxr_x",      "perm_rwxrwx",
            "perm_r_r_r",       "perm_rw_r_r",       "perm_rw_rw_rw",    "perm_rwxr_r",
            "perm_rwxr_xr_x",   "perm_rwxrwxrwx",    "perm_rwxrwxrwt",   "perm_unstick",
            "perm_stick",       "perm_go_w",         "perm_go_rwx",      "perm_ugo_w",
            "perm_ugo_rx",      "perm_ugo_rwx",      "rperm_rw",         "rperm_rwx",
            "rperm_rw_r",       "rperm_rw_rw",       "rperm_rwxr_x",     "rperm_rwxrwx",
            "rperm_rw_r_r",     "rperm_rw_rw_rw",    "rperm_rwxr_r",     "rperm_rwxr_xr_x",
            "rperm_rwxrwxrwx",  "rperm_rwxrwxrwt",   "rperm_unstick",    "rperm_stick",
            "rperm_go_w",       "rperm_go_rwx",      "rperm_ugo_w",      "rperm_ugo_rx",
            "rperm_ugo_rwx",    "own_myuser",        "own_myuser_users", "own_user1",
            "own_user1_users",  "own_user2",         "own_user2_users",  "own_root",
            "own_root_users",   "own_root_myuser",   "own_root_user1",   "own_root_user2",
            "rown_myuser",      "rown_myuser_users", "rown_user1",       "rown_user1_users",
            "rown_user2",       "rown_user2_users",  "rown_root",        "rown_root_users",
            "rown_root_myuser", "rown_root_user1",   "rown_root_user2"};
        for (const std::string& permcmd: permcmds)
        {
            set = xset_set_cb(permcmd, (GFunc)on_permission, data);
            xset_set_ob1(set, "set", set);
        }

        set = xset_get("prop_quick");
        set->disable = no_write_access || !sel_files;

        set = xset_get("prop_root");
        set->disable = !sel_files;

        set = xset_get("con_prop");
        if (geteuid() == 0)
            desc = ztd::strdup("prop_info prop_perm prop_root");
        else
            desc = ztd::strdup("prop_info prop_perm prop_quick prop_root");
        xset_set_set(set, XSetSetSet::DESC, desc);
        free(desc);
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    gtk_widget_show_all(GTK_WIDGET(popup));

    g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    g_signal_connect(popup, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    return popup;
}

static void
on_popup_open_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    GList* sel_files = data->sel_files;
    if (!sel_files)
        sel_files = g_list_prepend(sel_files, data->info);
    ptk_open_files_with_app(data->cwd, sel_files, nullptr, data->browser, true, false);
    if (sel_files != data->sel_files)
        g_list_free(sel_files);
}

static void
on_popup_open_with_another_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    VFSMimeType* mime_type;

    if (data->info)
    {
        mime_type = vfs_file_info_get_mime_type(data->info);
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
        parent_win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->browser)));
    char* app =
        (char*)ptk_choose_app_for_mime_type(parent_win, mime_type, false, true, true, false);
    if (app)
    {
        GList* sel_files = data->sel_files;
        if (!sel_files)
            sel_files = g_list_prepend(sel_files, data->info);
        ptk_open_files_with_app(data->cwd, sel_files, app, data->browser, false, false);
        if (sel_files != data->sel_files)
            g_list_free(sel_files);
        free(app);
    }
    vfs_mime_type_unref(mime_type);
}

static void
on_popup_handlers_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    ptk_handler_show_config(PtkHandlerMode::HANDLER_MODE_FILE, data->browser, nullptr);
}

static void
on_popup_open_all(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (xset_opener(data->browser, 1))
        return;

    GList* sel_files = data->sel_files;
    if (!sel_files)
        sel_files = g_list_prepend(sel_files, data->info);
    ptk_open_files_with_app(data->cwd, sel_files, nullptr, data->browser, false, true);
    if (sel_files != data->sel_files)
        g_list_free(sel_files);
}

static void
on_popup_run_app(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    XSet* handler_set = XSET(g_object_get_data(G_OBJECT(menuitem), "handler_set"));

    const char* desktop_file =
        static_cast<const char*>(g_object_get_data(G_OBJECT(menuitem), "desktop_file"));
    VFSAppDesktop desktop(desktop_file);

    std::string app;

    // is a file handler
    if (handler_set)
        app = fmt::format("###{}", handler_set->name);
    else
        app = desktop.get_name();

    GList* sel_files = data->sel_files;
    if (!sel_files)
        sel_files = g_list_prepend(sel_files, data->info);
    ptk_open_files_with_app(data->cwd, sel_files, app.c_str(), data->browser, false, false);
    if (sel_files != data->sel_files)
        g_list_free(sel_files);
}

enum PTKFileMenuAppJob
{
    APP_JOB_DEFAULT,
    APP_JOB_REMOVE,
    APP_JOB_EDIT,
    APP_JOB_EDIT_LIST,
    APP_JOB_ADD,
    APP_JOB_BROWSE,
    APP_JOB_BROWSE_SHARED,
    APP_JOB_EDIT_TYPE,
    APP_JOB_VIEW,
    APP_JOB_VIEW_TYPE,
    APP_JOB_VIEW_OVER,
    APP_JOB_UPDATE,
    APP_JOB_BROWSE_MIME,
    APP_JOB_BROWSE_MIME_USR,
    APP_JOB_USR
};

static char*
get_shared_desktop_file_location(const char* name)
{
    char* ret;

    for (const std::string& sys_dir: vfs_system_data_dir())
    {
        if ((ret = vfs_mime_type_locate_desktop_file(sys_dir.c_str(), name)))
            return ret;
    }

    return nullptr;
}

void
app_job(GtkWidget* item, GtkWidget* app_item)
{
    std::string msg;
    std::string path;
    char* str;
    std::string str2;

    std::string command;

    const char* desktop_file =
        static_cast<const char*>(g_object_get_data(G_OBJECT(app_item), "desktop_file"));
    VFSAppDesktop desktop(desktop_file);
    if (!desktop.get_name())
        return;

    int job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job"));
    PtkFileMenu* data = static_cast<PtkFileMenu*>(g_object_get_data(G_OBJECT(item), "data"));
    if (!(data && data->info))
        return;

    VFSMimeType* mime_type = vfs_file_info_get_mime_type(data->info);
    if (!mime_type)
        mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);

    switch (job)
    {
        case PTKFileMenuAppJob::APP_JOB_DEFAULT:
            vfs_mime_type_set_default_action(mime_type, desktop.get_name());
            ptk_app_chooser_has_handler_warn(data->browser ? GTK_WIDGET(data->browser) : nullptr,
                                             mime_type);
            break;
        case PTKFileMenuAppJob::APP_JOB_REMOVE:
            // for text files, spacefm displays both the actions for the type
            // and the actions for text/plain, so removing an app may appear to not
            // work if that app is still associated with text/plain
            vfs_mime_type_remove_action(mime_type, desktop.get_name());
            if (strcmp(mime_type->type, "text/plain") &&
                Glib::str_has_prefix(mime_type->type, "text/"))
                xset_msg_dialog(
                    GTK_WIDGET(data->browser),
                    GTK_MESSAGE_INFO,
                    "Remove Text Type Association",
                    GTK_BUTTONS_OK,
                    "NOTE:  When compiling the list of applications to appear in the Open "
                    "submenu for a text file, SpaceFM will include applications associated "
                    "with the MIME type (eg text/html) AND applications associated with "
                    "text/plain.  If you select Remove on an application, it will be removed "
                    "as an associated application for the MIME type (eg text/html), "
                    "but will NOT be removed as an associated application for text/plain "
                    "(unless the MIME type is text/plain).  Thus using Remove may not remove "
                    "the application from the Open submenu for this type, unless you also remove "
                    "it from text/plain.");
            break;
        case PTKFileMenuAppJob::APP_JOB_EDIT:
            path = Glib::build_filename(vfs_user_data_dir(), "applications", desktop.get_name());
            if (!std::filesystem::exists(path))
            {
                char* share_desktop =
                    vfs_mime_type_locate_desktop_file(nullptr, desktop.get_name());
                if (!(share_desktop && ztd::same(share_desktop, path)))
                {
                    free(share_desktop);
                    return;
                }

                msg = fmt::format("The file '{}' does not exist.\n\nBy copying '{}' to '{}' and "
                                  "editing it, you can adjust the behavior and appearance of this "
                                  "application for the current user.\n\nCreate this copy now?",
                                  path,
                                  share_desktop,
                                  path);
                if (xset_msg_dialog(GTK_WIDGET(data->browser),
                                    GTK_MESSAGE_QUESTION,
                                    "Copy Desktop File",
                                    GTK_BUTTONS_YES_NO,
                                    msg) != GTK_RESPONSE_YES)
                {
                    free(share_desktop);
                    break;
                }

                // need to copy
                command = fmt::format("cp -a  {} {}", share_desktop, path);
                Glib::spawn_command_line_sync(command);
                free(share_desktop);
                if (!std::filesystem::exists(path))
                    return;
            }
            xset_edit(GTK_WIDGET(data->browser), path.c_str(), false, false);
            break;
        case PTKFileMenuAppJob::APP_JOB_VIEW:
            str = get_shared_desktop_file_location(desktop.get_name());
            if (str)
                xset_edit(GTK_WIDGET(data->browser), str, false, true);
            free(str);
            break;
        case PTKFileMenuAppJob::APP_JOB_EDIT_LIST:
            // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
            path = Glib::build_filename(vfs_user_config_dir(), "mimeapps.list");
            if (!std::filesystem::exists(path))
            {
                // try old location
                // $XDG_DATA_HOME=[~/.local]/applications/mimeapps.list
                path = Glib::build_filename(vfs_user_data_dir(), "applications", "mimeapps.list");
            }
            xset_edit(GTK_WIDGET(data->browser), path.c_str(), false, true);
            break;
        case PTKFileMenuAppJob::APP_JOB_ADD:
            str = ptk_choose_app_for_mime_type(
                data->browser ? GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->browser)))
                              : GTK_WINDOW(data->browser),
                mime_type,
                false,
                true,
                true,
                true);
            // ptk_choose_app_for_mime_type returns either a bare command that
            // was already set as default, or a (custom or shared) desktop file
            if (!str)
                break;

            path = str;
            if (ztd::endswith(path, ".desktop") && !ztd::contains(path, "/") && mime_type)
                vfs_mime_type_append_action(mime_type->type, path.c_str());
            free(str);
            break;
        case PTKFileMenuAppJob::APP_JOB_BROWSE:
            path = Glib::build_filename(vfs_user_data_dir(), "applications");
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);

            if (data->browser)
                ptk_file_browser_emit_open(data->browser,
                                           path.c_str(),
                                           PtkOpenAction::PTK_OPEN_NEW_TAB);
            break;
        case PTKFileMenuAppJob::APP_JOB_BROWSE_SHARED:
            str = get_shared_desktop_file_location(desktop.get_name());
            if (str)
                path = g_path_get_dirname(str);
            else
                path = "/usr/share/applications";
            free(str);
            if (data->browser)
                ptk_file_browser_emit_open(data->browser,
                                           path.c_str(),
                                           PtkOpenAction::PTK_OPEN_NEW_TAB);
            break;
        case PTKFileMenuAppJob::APP_JOB_EDIT_TYPE:
            path = Glib::build_filename(vfs_user_data_dir(), "mime/packages");
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
            str2 = ztd::replace(mime_type->type, "/", "-");
            str2 = fmt::format("{}.xml", str2);
            path = Glib::build_filename(vfs_user_data_dir(), "mime/packages", str2);
            if (!std::filesystem::exists(path))
            {
                std::string xml_file = fmt::format("{}.xml", mime_type->type);
                std::string usr_path = Glib::build_filename("/usr/share/mime", xml_file);

                if (std::filesystem::exists(usr_path))
                    msg = fmt::format("The file '{}' does not exist.\n\nBy copying '{}' to '{}' "
                                      "and editing it, you can adjust how MIME type '{}' files are "
                                      "recognized for the current user.\n\nCreate this copy now?",
                                      path,
                                      usr_path,
                                      path,
                                      mime_type->type);
                else
                    msg = fmt::format("The file '{}' does not exist.\n\nBy creating new file '{}' "
                                      "and editing it, you can define how MIME type '{}' files are "
                                      "recognized for the current user.\n\nCreate this file now?",
                                      path,
                                      path,
                                      mime_type->type);
                if (xset_msg_dialog(GTK_WIDGET(data->browser),
                                    GTK_MESSAGE_QUESTION,
                                    "Create New XML",
                                    GTK_BUTTONS_YES_NO,
                                    msg) != GTK_RESPONSE_YES)
                {
                    break;
                }

                // need to create
                msg = fmt::format(
                    "<?xml version='1.0' encoding='utf-8'?>\n<mime-info "
                    "xmlns='http://www.freedesktop.org/standards/shared-mime-info'>\n<mime-type "
                    "type='{}'>\n\n<!-- This file was generated by SpaceFM to allow you to change "
                    "the name or icon\n     of the above mime type and to change the filename or "
                    "magic patterns that\n     define this type.\n\n     IMPORTANT:  After saving "
                    "this file, restart SpaceFM.  You may need to run:\n     update-mime-database "
                    "~/.local/share/mime\n\n     Delete this file from "
                    "~/.local/share/mime/packages/ "
                    "to revert to default.\n\n     To make this definition file apply to all "
                    "users, "
                    "copy this file to\n     /usr/share/mime/packages/ and:  sudo "
                    "update-mime-database "
                    "/usr/share/mime\n\n     For help editing this file:\n     "
                    "http://standards.freedesktop.org/shared-mime-info-spec/latest/ar01s02.html\n  "
                    "   "
                    "http://www.freedesktop.org/wiki/Specifications/AddingMIMETutor\n\n     "
                    "Example to "
                    "define the name/icon of PNG files (with optional translation):\n\n        "
                    "<comment>Portable Network Graphics file</comment>\n        <comment "
                    "xml:lang=\"en\">Portable Network Graphics file</comment>\n        <icon "
                    "name=\"spacefm\"/>\n\n     Example to detect PNG files by glob pattern:\n\n   "
                    "    "
                    " <glob pattern=\"*.png\"/>\n\n     Example to detect PNG files by file "
                    "contents:\n\n        <magic priority=\"50\">\n            <match "
                    "type=\"string\" "
                    "value=\"\\x89PNG\" offset=\"0\"/>\n        </magic>\n-->",
                    mime_type->type);

                // build from /usr/share/mime type ?

                std::string contents;
                try
                {
                    contents = Glib::file_get_contents(usr_path);
                }
                catch (const Glib::FileError& e)
                {
                    std::string what = e.what();
                    LOG_WARN("Error reading {}: {}", usr_path, what);
                }

                if (!contents.empty())
                {
                    char* contents2 = ztd::strdup(contents);
                    char* start = nullptr;
                    if ((str = strstr(contents2, "\n<mime-type ")))
                    {
                        if ((str = strstr(str, ">\n")))
                        {
                            str[1] = '\0';
                            start = contents2;
                            if ((str = strstr(str + 2, "<!--Created automatically")))
                            {
                                if ((str = strstr(str, "-->")))
                                    start = str + 4;
                            }
                        }
                    }
                    if (start)
                        contents = fmt::format("{}\n\n{}</mime-info>\n", msg, start);
                }

                if (contents.empty())
                {
                    contents = fmt::format("{}\n\n<!-- insert your patterns below "
                                           "-->\n\n\n</mime-type>\n</mime-info>\n\n",
                                           msg);
                }

                // write file
                std::ofstream file(path);
                if (file.is_open())
                    file << contents;
                file.close();
            }
            if (std::filesystem::exists(path))
                xset_edit(GTK_WIDGET(data->browser), path.c_str(), false, false);

            vfs_dir_monitor_mime();
            break;
        case PTKFileMenuAppJob::APP_JOB_VIEW_TYPE:
            str2 = fmt::format("{}.xml", mime_type->type);
            path = Glib::build_filename("/usr/share/mime", str2);
            if (std::filesystem::exists(path))
                xset_edit(GTK_WIDGET(data->browser), path.c_str(), false, true);
            break;
        case PTKFileMenuAppJob::APP_JOB_VIEW_OVER:
            path = "/usr/share/mime/packages/Overrides.xml";
            xset_edit(GTK_WIDGET(data->browser), path.c_str(), true, false);
            break;
        case PTKFileMenuAppJob::APP_JOB_BROWSE_MIME_USR:
            if (data->browser)
                ptk_file_browser_emit_open(data->browser,
                                           "/usr/share/mime/packages",
                                           PtkOpenAction::PTK_OPEN_NEW_TAB);
            break;
        case PTKFileMenuAppJob::APP_JOB_BROWSE_MIME:
            path = Glib::build_filename(vfs_user_data_dir(), "mime/packages");
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
            if (data->browser)
                ptk_file_browser_emit_open(data->browser,
                                           path.c_str(),
                                           PtkOpenAction::PTK_OPEN_NEW_TAB);
            vfs_dir_monitor_mime();
            break;
        case PTKFileMenuAppJob::APP_JOB_UPDATE:
            command = fmt::format("update-mime-database {}/mime", vfs_user_data_dir());
            print_command(command);
            Glib::spawn_command_line_async(command);

            command = fmt::format("update-desktop-database {}/applications", vfs_user_data_dir());
            print_command(command);
            Glib::spawn_command_line_async(command);
            break;
        default:
            break;
    }
    if (mime_type)
        vfs_mime_type_unref(mime_type);
}

static bool
app_menu_keypress(GtkWidget* menu, GdkEventKey* event, PtkFileMenu* data)
{
    int job = -1;

    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(menu));
    if (!item)
        return false;

    // if original menu, desktop will be set
    const char* desktop_file =
        static_cast<const char*>(g_object_get_data(G_OBJECT(item), "desktop_file"));
    VFSAppDesktop desktop(desktop_file);
    // else if app menu, data will be set
    // PtkFileMenu* app_data = static_cast<PtkFileMenu*>(g_object_get_data(G_OBJECT(item), "data"));

    unsigned int keymod = ptk_get_keymod(event->state);

    if (keymod == 0)
    {
        switch (event->keyval)
        {
            case GDK_KEY_F2:
            case GDK_KEY_Menu:
                show_app_menu(menu, item, data, 0, event->time);
                return true;
            case GDK_KEY_F4:
                job = PTKFileMenuAppJob::APP_JOB_EDIT;
                break;
            case GDK_KEY_Delete:
                job = PTKFileMenuAppJob::APP_JOB_REMOVE;
                break;
            case GDK_KEY_Insert:
                job = PTKFileMenuAppJob::APP_JOB_ADD;
                break;
            default:
                break;
        }
    }
    if (job != -1)
    {
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
        g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(job));
        g_object_set_data(G_OBJECT(item), "data", data);
        app_job(item, item);
        return true;
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
app_menu_additem(GtkWidget* menu, const char* label, const char* stock_icon, int job,
                 GtkWidget* app_item, PtkFileMenu* data)
{
    GtkWidget* item;
    if (stock_icon)
    {
        if (!strcmp(stock_icon, "@check"))
            item = gtk_check_menu_item_new_with_mnemonic(label);
        else
            item = gtk_menu_item_new_with_mnemonic(label);
    }
    else
        item = gtk_menu_item_new_with_mnemonic(label);

    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(job));
    g_object_set_data(G_OBJECT(item), "data", data);
    gtk_container_add(GTK_CONTAINER(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(app_job), app_item);
    return item;
}

static void
show_app_menu(GtkWidget* menu, GtkWidget* app_item, PtkFileMenu* data, unsigned int button,
              uint32_t time)
{
    (void)button;
    (void)time;
    GtkWidget* newitem;
    GtkWidget* submenu;
    std::string str;
    std::string path;
    char* icon;
    const char* type;

    if (!(data && data->info))
        return;

    XSet* handler_set = XSET(g_object_get_data(G_OBJECT(app_item), "handler_set"));
    if (handler_set)
    {
        // is a file handler - open file handler config
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
        ptk_handler_show_config(PtkHandlerMode::HANDLER_MODE_FILE, data->browser, handler_set);
        return;
    }

    VFSMimeType* mime_type = vfs_file_info_get_mime_type(data->info);
    if (mime_type)
    {
        type = vfs_mime_type_get_type(mime_type);
        vfs_mime_type_unref(mime_type);
    }
    else
        type = "unknown";

    const char* desktop_file =
        static_cast<const char*>(g_object_get_data(G_OBJECT(app_item), "desktop_file"));
    VFSAppDesktop desktop(desktop_file);

    GtkWidget* app_menu = gtk_menu_new();

    // Set Default
    newitem = app_menu_additem(app_menu,
                               ztd::strdup("_Set As Default"),
                               ztd::strdup("document-save"),
                               PTKFileMenuAppJob::APP_JOB_DEFAULT,
                               app_item,
                               data);

    // Remove
    newitem = app_menu_additem(app_menu,
                               ztd::strdup("_Remove"),
                               ztd::strdup("edit-delete"),
                               PTKFileMenuAppJob::APP_JOB_REMOVE,
                               app_item,
                               data);

    // Add
    newitem = app_menu_additem(app_menu,
                               ztd::strdup("_Add..."),
                               ztd::strdup("list-add"),
                               PTKFileMenuAppJob::APP_JOB_ADD,
                               app_item,
                               data);

    // Separator
    gtk_container_add(GTK_CONTAINER(app_menu), gtk_separator_menu_item_new());

    // *.desktop (missing)
    if (desktop.get_name())
    {
        path = Glib::build_filename(vfs_user_data_dir(), "applications", desktop.get_name());
        if (std::filesystem::exists(path))
        {
            str = ztd::replace(desktop.get_name(), ".desktop", "._desktop");
            icon = ztd::strdup("Edit");
        }
        else
        {
            str = ztd::replace(desktop.get_name(), ".desktop", "._desktop");
            str = fmt::format("{} (*copy)", str);
            icon = ztd::strdup("document-new");
        }
        newitem = app_menu_additem(app_menu,
                                   str.c_str(),
                                   icon,
                                   PTKFileMenuAppJob::APP_JOB_EDIT,
                                   app_item,
                                   data);
    }

    // mimeapps.list
    newitem = app_menu_additem(app_menu,
                               ztd::strdup("_mimeapps.list"),
                               ztd::strdup("Edit"),
                               PTKFileMenuAppJob::APP_JOB_EDIT_LIST,
                               app_item,
                               data);

    // applications/
    newitem = app_menu_additem(app_menu,
                               ztd::strdup("appli_cations/"),
                               ztd::strdup("folder"),
                               PTKFileMenuAppJob::APP_JOB_BROWSE,
                               app_item,
                               data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem), !!data->browser);

    // Separator
    gtk_container_add(GTK_CONTAINER(app_menu), gtk_separator_menu_item_new());

    // *.xml (missing)
    str = ztd::replace(type, "/", "-");
    str = fmt::format("{}.xml", str);
    path = Glib::build_filename(vfs_user_data_dir(), "mime/packages", str);
    if (std::filesystem::exists(path))
    {
        str = ztd::replace(type, "/", "-");
        str = fmt::format("{}._xml", str);

        icon = ztd::strdup("Edit");
    }
    else
    {
        str = ztd::replace(type, "/", "-");
        str = fmt::format("{}._xml (*new)", str);

        icon = ztd::strdup("document-new");
    }
    newitem = app_menu_additem(app_menu,
                               str.c_str(),
                               icon,
                               PTKFileMenuAppJob::APP_JOB_EDIT_TYPE,
                               app_item,
                               data);

    // mime/packages/
    newitem = app_menu_additem(app_menu,
                               ztd::strdup("mime/pac_kages/"),
                               ztd::strdup("folder"),
                               PTKFileMenuAppJob::APP_JOB_BROWSE_MIME,
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
    g_object_set_data(G_OBJECT(newitem), "job", GINT_TO_POINTER(PTKFileMenuAppJob::APP_JOB_USR));
    g_object_set_data(G_OBJECT(newitem), "data", data);
    g_signal_connect(submenu, "key_press_event", G_CALLBACK(app_menu_keypress), data);

    // View /usr .desktop
    if (desktop.get_name())
    {
        newitem = app_menu_additem(submenu,
                                   desktop.get_name(),
                                   ztd::strdup("text-x-generic"),
                                   PTKFileMenuAppJob::APP_JOB_VIEW,
                                   app_item,
                                   data);

        char* desk_path = get_shared_desktop_file_location(desktop.get_name());
        gtk_widget_set_sensitive(GTK_WIDGET(newitem), !!desk_path);
        free(desk_path);
    }

    // /usr applications/
    newitem = app_menu_additem(submenu,
                               ztd::strdup("appli_cations/"),
                               ztd::strdup("folder"),
                               PTKFileMenuAppJob::APP_JOB_BROWSE_SHARED,
                               app_item,
                               data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem), !!data->browser);

    // Separator
    gtk_container_add(GTK_CONTAINER(submenu), gtk_separator_menu_item_new());

    // /usr *.xml
    str = fmt::format("{}.xml", type);
    path = Glib::build_filename("/usr/share/mime", str);
    str = fmt::format("{}._xml", type);

    newitem = app_menu_additem(submenu,
                               str.c_str(),
                               ztd::strdup("text-x-generic"),
                               PTKFileMenuAppJob::APP_JOB_VIEW_TYPE,
                               app_item,
                               data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem), std::filesystem::exists(path));

    // /usr *Overrides.xml
    newitem = app_menu_additem(submenu,
                               ztd::strdup("_Overrides.xml"),
                               ztd::strdup("Edit"),
                               PTKFileMenuAppJob::APP_JOB_VIEW_OVER,
                               app_item,
                               data);
    gtk_widget_set_sensitive(GTK_WIDGET(newitem),
                             std::filesystem::exists("/usr/share/mime/packages/Overrides.xml"));

    // mime/packages/
    newitem = app_menu_additem(submenu,
                               ztd::strdup("mime/pac_kages/"),
                               ztd::strdup("folder"),
                               PTKFileMenuAppJob::APP_JOB_BROWSE_MIME_USR,
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
    unsigned int keymod = ptk_get_keymod(event->state);

    if (event->type == GDK_BUTTON_RELEASE)
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
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
            gtk_menu_item_activate(GTK_MENU_ITEM(item));
            return true;
        }
        // true for issue #521 where a right-click also left-clicks the first
        // menu item in some GTK2/3 themes.
        return true;
    }
    else if (event->type != GDK_BUTTON_PRESS)
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
    if (data->sel_files)
    {
        GList* sel;
        for (sel = data->sel_files; sel; sel = sel->next)
        {
            VFSFileInfo* file = static_cast<VFSFileInfo*>(sel->data);
            char* full_path = g_build_filename(data->cwd, vfs_file_info_get_name(file), nullptr);
            if (data->browser && std::filesystem::is_directory(full_path))
            {
                ptk_file_browser_emit_open(data->browser,
                                           full_path,
                                           PtkOpenAction::PTK_OPEN_NEW_TAB);
            }
            free(full_path);
        }
    }
    else if (data->browser)
    {
        ptk_file_browser_emit_open(data->browser, data->file_path, PtkOpenAction::PTK_OPEN_NEW_TAB);
    }
}

void
on_popup_open_in_new_tab_here(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser && data->cwd && std::filesystem::is_directory(data->cwd))
        ptk_file_browser_emit_open(data->browser, data->cwd, PtkOpenAction::PTK_OPEN_NEW_TAB);
}

static void
on_new_bookmark(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    // if a single dir or file is selected, bookmark it instead of cwd
    if (data->sel_files && !data->sel_files->next)
    {
        char* full_path = g_build_filename(
            data->cwd,
            vfs_file_info_get_name(static_cast<VFSFileInfo*>(data->sel_files->data)),
            nullptr);
        ptk_bookmark_view_add_bookmark(nullptr, data->browser, full_path);
        free(full_path);
    }
    else
        ptk_bookmark_view_add_bookmark(nullptr, data->browser, nullptr);
}

static void
on_popup_cut_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->sel_files)
        ptk_clipboard_cut_or_copy_files(data->cwd, data->sel_files, false);
}

static void
on_popup_copy_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->sel_files)
        ptk_clipboard_cut_or_copy_files(data->cwd, data->sel_files, true);
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
                                  GTK_TREE_VIEW(data->browser->task_view),
                                  nullptr,
                                  nullptr);
    }
}

static void
on_popup_paste_link_activate(GtkMenuItem* menuitem, PtkFileMenu* data) // MOD added
{
    (void)menuitem;
    if (data->browser)
        ptk_file_browser_paste_link(data->browser);
}

static void
on_popup_paste_target_activate(GtkMenuItem* menuitem, PtkFileMenu* data) // MOD added
{
    (void)menuitem;
    if (data->browser)
        ptk_file_browser_paste_target(data->browser);
}

static void
on_popup_copy_text_activate(GtkMenuItem* menuitem, PtkFileMenu* data) // MOD added
{
    (void)menuitem;
    ptk_clipboard_copy_as_text(data->cwd, data->sel_files);
}

static void
on_popup_copy_name_activate(GtkMenuItem* menuitem, PtkFileMenu* data) // MOD added
{
    (void)menuitem;
    ptk_clipboard_copy_name(data->cwd, data->sel_files);
}

static void
on_popup_copy_parent_activate(GtkMenuItem* menuitem, PtkFileMenu* data) // MOD added
{
    (void)menuitem;
    if (data->cwd)
        ptk_clipboard_copy_text(data->cwd);
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
    if (data->sel_files)
    {
        if (data->browser)
        {
            GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
            ptk_delete_files(GTK_WINDOW(parent_win),
                             data->cwd,
                             data->sel_files,
                             GTK_TREE_VIEW(data->browser->task_view));
        }
    }
}

static void
on_popup_trash_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->sel_files)
    {
        if (data->browser)
        {
            GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
            ptk_trash_files(GTK_WINDOW(parent_win),
                            data->cwd,
                            data->sel_files,
                            GTK_TREE_VIEW(data->browser->task_view));
        }
    }
}

static void
on_popup_rename_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (data->browser)
        ptk_file_browser_rename_selected_files(data->browser, data->sel_files, data->cwd);
}

static void
on_popup_compress_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    ptk_file_archiver_create(data->browser, data->sel_files, data->cwd);
}

static void
on_popup_extract_to_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    /* If menuitem is set, function was called from GUI so files will contain
     * an archive */
    ptk_file_archiver_extract(data->browser,
                              data->sel_files,
                              data->cwd,
                              nullptr,
                              PtkHandlerArchive::HANDLER_EXTRACT,
                              menuitem ? true : false);
}

static void
on_popup_extract_here_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    /* If menuitem is set, function was called from GUI so files will contain
     * an archive */
    ptk_file_archiver_extract(data->browser,
                              data->sel_files,
                              data->cwd,
                              data->cwd,
                              PtkHandlerArchive::HANDLER_EXTRACT,
                              menuitem ? true : false);
}

static void
on_popup_extract_list_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    /* If menuitem is set, function was called from GUI so files will contain
     * an archive */
    ptk_file_archiver_extract(data->browser,
                              data->sel_files,
                              data->cwd,
                              nullptr,
                              PtkHandlerArchive::HANDLER_LIST,
                              menuitem ? true : false);
}

static void
on_autoopen_create_cb(void* task, AutoOpenCreate* ao)
{
    (void)task;
    if (!ao)
        return;

    if (ao->path && GTK_IS_WIDGET(ao->file_browser) && std::filesystem::exists(ao->path))
    {
        char* cwd = g_path_get_dirname(ao->path);
        VFSFileInfo* file;

        // select file
        if (!g_strcmp0(cwd, ptk_file_browser_get_cwd(ao->file_browser)))
        {
            file = vfs_file_info_new();
            vfs_file_info_get(file, ao->path, nullptr);
            vfs_dir_emit_file_created(ao->file_browser->dir, vfs_file_info_get_name(file), true);
            vfs_file_info_unref(file);
            vfs_dir_flush_notify_cache();
            ptk_file_browser_select_file(ao->file_browser, ao->path);
        }

        // open file
        if (ao->open_file)
        {
            if (std::filesystem::is_directory(ao->path))
            {
                ptk_file_browser_chdir(ao->file_browser,
                                       ao->path,
                                       PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
                ao->path = nullptr;
            }
            else
            {
                file = vfs_file_info_new();
                vfs_file_info_get(file, ao->path, nullptr);
                GList* sel_files = nullptr;
                sel_files = g_list_prepend(sel_files, file);
                ptk_open_files_with_app(cwd, sel_files, nullptr, ao->file_browser, false, true);
                vfs_file_info_unref(file);
                g_list_free(sel_files);
            }
        }

        free(cwd);
    }
    free(ao->path);
    g_slice_free(AutoOpenCreate, ao);
}

static void
create_new_file(PtkFileMenu* data, int create_new)
{
    if (!data->cwd)
        return;

    AutoOpenCreate* ao = g_slice_new0(AutoOpenCreate);
    ao->path = nullptr;
    ao->file_browser = data->browser;
    ao->open_file = false;
    if (data->browser)
        ao->callback = (GFunc)on_autoopen_create_cb;
    int result = ptk_rename_file(data->browser,
                                 data->cwd,
                                 data->sel_files ? static_cast<VFSFileInfo*>(data->sel_files->data)
                                                 : nullptr,
                                 nullptr,
                                 false,
                                 (PtkRenameMode)create_new,
                                 ao);
    if (result == 0)
    {
        ao->file_browser = nullptr;
        free(ao->path);
        ao->path = nullptr;
        g_slice_free(AutoOpenCreate, ao);
    }
}

static void
on_popup_new_folder_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    create_new_file(data, PtkRenameMode::PTK_RENAME_NEW_DIR);
}

static void
on_popup_new_text_file_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    create_new_file(data, PtkRenameMode::PTK_RENAME_NEW_FILE);
}

static void
on_popup_new_link_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    create_new_file(data, PtkRenameMode::PTK_RENAME_NEW_LINK);
}

static void
on_popup_file_properties_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    GtkWindow* parent_win = nullptr;
    if (data->browser)
        parent_win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->browser)));

    ptk_show_file_properties(parent_win, data->cwd, data->sel_files, 0);
}

static void
on_popup_file_permissions_activate(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    GtkWindow* parent_win = nullptr;
    if (data->browser)
        parent_win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->browser)));

    ptk_show_file_properties(parent_win, data->cwd, data->sel_files, 1);
}

static void
on_popup_canon(GtkMenuItem* menuitem, PtkFileMenu* data)
{
    (void)menuitem;
    if (!data->browser)
        return;

    ptk_file_browser_canon(data->browser, data->file_path ? data->file_path : data->cwd);
}

void
ptk_file_menu_action(PtkFileBrowser* browser, char* setname)
{
    if (!browser || !setname)
        return;

    const char* cwd;
    char* file_path = nullptr;
    VFSFileInfo* info;
    GList* sel_files = nullptr;

    // setup data
    if (browser)
    {
        cwd = ptk_file_browser_get_cwd(browser);
        sel_files = ptk_file_browser_get_selected_files(browser);
    }
    else
    {
        cwd = vfs_user_desktop_dir().c_str();
    }

    if (!sel_files)
        info = nullptr;
    else
    {
        info = vfs_file_info_ref(static_cast<VFSFileInfo*>(sel_files->data));
        file_path = g_build_filename(cwd, vfs_file_info_get_name(info), nullptr);
    }

    PtkFileMenu* data = g_slice_new0(PtkFileMenu);
    data->cwd = ztd::strdup(cwd);
    data->browser = browser;

    data->file_path = file_path;
    if (info)
        data->info = vfs_file_info_ref(info);
    else
        data->info = nullptr;

    data->sel_files = sel_files;
    data->accel_group = nullptr;

    // action
    XSet* set = xset_get(setname);
    if (Glib::str_has_prefix(set->name, "open_") && !Glib::str_has_prefix(set->name, "open_in_"))
    {
        if (!strcmp(set->name, "open_edit"))
            xset_edit(GTK_WIDGET(data->browser), data->file_path, false, true);
        else if (!strcmp(set->name, "open_edit_root"))
            xset_edit(GTK_WIDGET(data->browser), data->file_path, true, false);
        else if (!strcmp(set->name, "open_other"))
            on_popup_open_with_another_activate(nullptr, data);
        else if (!strcmp(set->name, "open_execute"))
            on_popup_open_activate(nullptr, data);
        else if (!strcmp(set->name, "open_all"))
            on_popup_open_all(nullptr, data);
    }
    else if (Glib::str_has_prefix(set->name, "arc_"))
    {
        if (!strcmp(set->name, "arc_extract"))
            on_popup_extract_here_activate(nullptr, data);
        else if (!strcmp(set->name, "arc_extractto"))
            on_popup_extract_to_activate(nullptr, data);
        else if (!strcmp(set->name, "arc_extract"))
            on_popup_extract_list_activate(nullptr, data);
        else if (!strcmp(set->name, "arc_conf2"))
            on_archive_show_config(nullptr, data);
    }
    else if (Glib::str_has_prefix(set->name, "new_"))
    {
        if (!strcmp(set->name, "new_file"))
            on_popup_new_text_file_activate(nullptr, data);
        else if (!strcmp(set->name, "new_directory"))
            on_popup_new_folder_activate(nullptr, data);
        else if (!strcmp(set->name, "new_link"))
            on_popup_new_link_activate(nullptr, data);
        else if (!strcmp(set->name, "new_bookmark"))
            ptk_bookmark_view_add_bookmark(nullptr, browser, nullptr);
        else if (!strcmp(set->name, "new_archive"))
        {
            if (browser)
                on_popup_compress_activate(nullptr, data);
        }
    }
    else if (!strcmp(set->name, "prop_info"))
        on_popup_file_properties_activate(nullptr, data);
    else if (!strcmp(set->name, "prop_perm"))
        on_popup_file_permissions_activate(nullptr, data);
    else if (Glib::str_has_prefix(set->name, "edit_"))
    {
        if (!strcmp(set->name, "edit_cut"))
            on_popup_cut_activate(nullptr, data);
        else if (!strcmp(set->name, "edit_copy"))
            on_popup_copy_activate(nullptr, data);
        else if (!strcmp(set->name, "edit_paste"))
            on_popup_paste_activate(nullptr, data);
        else if (!strcmp(set->name, "edit_rename"))
            on_popup_rename_activate(nullptr, data);
        else if (!strcmp(set->name, "edit_delete"))
            on_popup_delete_activate(nullptr, data);
        else if (!strcmp(set->name, "edit_trash"))
            on_popup_trash_activate(nullptr, data);
        else if (!strcmp(set->name, "edit_hide"))
            on_hide_file(nullptr, data);
        else if (!strcmp(set->name, "edit_canon"))
        {
            if (browser)
                on_popup_canon(nullptr, data);
        }
    }
    else if (!strcmp(set->name, "copy_name"))
        on_popup_copy_name_activate(nullptr, data);
    else if (!strcmp(set->name, "copy_path"))
        on_popup_copy_text_activate(nullptr, data);
    else if (!strcmp(set->name, "copy_parent"))
        on_popup_copy_parent_activate(nullptr, data);
    else if (Glib::str_has_prefix(set->name, "copy_loc") ||
             Glib::str_has_prefix(set->name, "copy_tab_") ||
             Glib::str_has_prefix(set->name, "copy_panel_") ||
             Glib::str_has_prefix(set->name, "move_loc") ||
             Glib::str_has_prefix(set->name, "move_tab_") ||
             Glib::str_has_prefix(set->name, "move_panel_"))
        on_copycmd(nullptr, data, set);
    else if (Glib::str_has_prefix(set->name, "root_"))
    {
        if (!strcmp(set->name, "root_copy_loc") || !strcmp(set->name, "root_move2") ||
            !strcmp(set->name, "root_delete") || !strcmp(set->name, "root_trash"))
            on_popup_rootcmd_activate(nullptr, data, set);
    }
    else if (browser)
    {
        // browser only
        int i;
        if (Glib::str_has_prefix(set->name, "open_in_panel"))
        {
            if (!strcmp(set->name, "open_in_panel_prev"))
                i = -1;
            else if (!strcmp(set->name, "open_in_panel_next"))
                i = -2;
            else
                i = strtol(set->name, nullptr, 10);
            main_window_open_in_panel(data->browser, i, data->file_path);
        }
        else if (Glib::str_has_prefix(set->name, "opentab_"))
        {
            if (!strcmp(set->name, "opentab_new"))
                on_popup_open_in_new_tab_activate(nullptr, data);
            else
            {
                if (!strcmp(set->name, "opentab_prev"))
                    i = -1;
                else if (!strcmp(set->name, "opentab_next"))
                    i = -2;
                else
                    i = strtol(set->name, nullptr, 10);
                ptk_file_browser_open_in_tab(data->browser, i, data->file_path);
            }
        }
        else if (!strcmp(set->name, "tab_new"))
            ptk_file_browser_new_tab(nullptr, browser);
        else if (!strcmp(set->name, "tab_new_here"))
            on_popup_open_in_new_tab_here(nullptr, data);
    }

    ptk_file_menu_free(data);
}
