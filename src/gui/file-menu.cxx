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

#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glibmm.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "compat/gtk4-porting.hxx"

#include "utils/strdup.hxx"

#include "xset/xset-context-menu.hxx"
#include "xset/xset.hxx"

#include "gui/archiver.hxx"
#include "gui/bookmark-view.hxx"
#include "gui/clipboard.hxx"
#include "gui/file-action-open.hxx"
#include "gui/file-action-paste.hxx"
#include "gui/file-list.hxx"
#include "gui/file-menu.hxx"
#include "gui/file-task-view.hxx"
#include "gui/main-window.hxx"
#include "gui/utils/utils.hxx"

#include "gui/dialog/app-chooser.hxx"
#include "gui/dialog/create.hxx"
#include "gui/dialog/properties.hxx"
#include "gui/dialog/rename.hxx"
#include "gui/dialog/text.hxx"
#include "gui/dialog/trash-delete.hxx"

#include "vfs/utils/file-ops.hxx"
#include "vfs/utils/vfs-editor.hxx"
#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-mime-monitor.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "logger.hxx"
#include "types.hxx"

#define PTK_FILE_MENU(obj) (static_cast<ptk::file_menu*>(obj))

static bool on_app_button_press(GtkWidget* item, GdkEvent* event, ptk::file_menu* data) noexcept;
static bool app_menu_keypress(GtkWidget* menu, GdkEvent* event, ptk::file_menu* data) noexcept;
static void show_app_menu(GtkWidget* menu, GtkWidget* app_item, ptk::file_menu* data, u32 button,
                          const std::chrono::system_clock::time_point time_point) noexcept;

/* Signal handlers for popup menu */
static void on_popup_open_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_open_with_another_activate(GtkMenuItem* menuitem,
                                                ptk::file_menu* data) noexcept;
static void on_popup_run_app(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_open_in_new_tab_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_new_bookmark(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_cut_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_copy_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_paste_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_paste_link_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_paste_target_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_copy_text_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_copy_name_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_copy_parent_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;

static void on_popup_paste_as_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;

static void on_popup_trash_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_delete_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_rename_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_batch_rename_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_compress_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_extract_here_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_extract_to_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_extract_open_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_new_folder_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_new_text_file_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_new_link_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;

static void on_popup_file_properties_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_file_attributes_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_file_permissions_activate(GtkMenuItem* menuitem,
                                               ptk::file_menu* data) noexcept;

static void on_popup_open_all(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_popup_canon(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept;
static void on_autoopen_create_cb(void* task, AutoOpenCreate* ao) noexcept;

ptk::file_menu::~file_menu() noexcept
{
    if (this->accel_group)
    {
        g_object_unref(this->accel_group);
    }
}

AutoOpenCreate::AutoOpenCreate(ptk::browser* browser, bool open_file) noexcept
    : browser(browser), open_file(open_file)
{
    if (this->browser)
    {
        this->callback = (GFunc)on_autoopen_create_cb;
    }
}

void
on_popup_list_large(GtkMenuItem* menuitem, ptk::browser* browser) noexcept
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
on_popup_list_detailed(GtkMenuItem* menuitem, ptk::browser* browser) noexcept
{
    (void)menuitem;
    const panel_t p = browser->panel();

    if (xset_get_b_panel(p, xset::panel::list_detailed))
    {
        // setting b to xset::set::enabled::unset does not work here
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
on_popup_list_icons(GtkMenuItem* menuitem, ptk::browser* browser) noexcept
{
    (void)menuitem;
    const panel_t p = browser->panel();

    if (xset_get_b_panel(p, xset::panel::list_icons))
    {
        // setting b to xset::set::enabled::unset does not work here
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
on_popup_list_compact(GtkMenuItem* menuitem, ptk::browser* browser) noexcept
{
    (void)menuitem;
    const panel_t p = browser->panel();

    if (xset_get_b_panel(p, xset::panel::list_compact))
    {
        // setting b to xset::set::enabled::unset does not work here
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
on_popup_show_hidden(GtkMenuItem* menuitem, ptk::browser* browser) noexcept
{
    (void)menuitem;
    if (browser)
    {
        browser->show_hidden_files(xset_get_b_panel(browser->panel(), xset::panel::show_hidden));
    }
}

static void
on_copycmd(GtkMenuItem* menuitem, ptk::file_menu* data, const xset_t& set2) noexcept
{
    xset_t set;
    if (menuitem)
    {
        set =
            xset::set::get(static_cast<const char*>(g_object_get_data(G_OBJECT(menuitem), "set")));
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
        data->browser->copycmd(data->selected_files, data->cwd, set->xset_name);
    }
}

static void
on_popup_select_pattern(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->select_pattern();
    }
}

static void
on_open_in_tab(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    const tab_t tab = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "tab"));
    if (data->browser)
    {
        data->browser->open_in_tab(data->file_path, tab);
    }
}

static void
on_open_in_panel(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    const panel_t panel_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "panel"));
    if (data->browser)
    {
        data->browser->open_in_panel(panel_num, data->file_path);
    }
}

static void
on_file_edit(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    vfs::utils::open_editor(data->file_path);
}

static void
on_popup_sort_extra(GtkMenuItem* menuitem, ptk::browser* browser, const xset_t& set2) noexcept
{
    xset_t set;
    if (menuitem)
    {
        set =
            xset::set::get(static_cast<const char*>(g_object_get_data(G_OBJECT(menuitem), "set")));
    }
    else
    {
        set = set2;
    }
    browser->set_sort_extra(set->xset_name);
}

void
on_popup_sortby(GtkMenuItem* menuitem, ptk::browser* browser, i32 order) noexcept
{
    GtkSortType v = GtkSortType::GTK_SORT_ASCENDING;
    i32 sort_order = 0;
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
        xset_set_panel(browser->panel(),
                       xset::panel::list_detailed,
                       xset::var::y,
                       std::format("{}", static_cast<std::int32_t>(v)));
        browser->set_sort_type(v);
    }
    else
    {
        xset_set_panel(browser->panel(),
                       xset::panel::list_detailed,
                       xset::var::x,
                       std::format("{}", sort_order));
        browser->set_sort_order(static_cast<enum ptk::browser::sort_order>(sort_order.data()));
    }
}

static void
on_popup_detailed_column(GtkMenuItem* menuitem, ptk::browser* browser) noexcept
{
    (void)menuitem;
    if (browser->is_view_mode(ptk::browser::view_mode::list_view))
    {
        // get visiblity for correct mode
        const MainWindow* main_window = browser->main_window();
        const panel_t p = browser->panel();
        const xset::main_window_panel mode = main_window->panel_context.at(p);

        { // size
            const auto set = xset::set::get(xset::panel::detcol_size, p, mode);
            set->b = xset::set::get(xset::panel::detcol_size, p)->b;
        }

        { // size in bytes
            const auto set = xset::set::get(xset::panel::detcol_bytes, p, mode);
            set->b = xset::set::get(xset::panel::detcol_bytes, p)->b;
        }

        { // type
            const auto set = xset::set::get(xset::panel::detcol_type, p, mode);
            set->b = xset::set::get(xset::panel::detcol_type, p)->b;
        }

        { // MIME type
            const auto set = xset::set::get(xset::panel::detcol_mime, p, mode);
            set->b = xset::set::get(xset::panel::detcol_mime, p)->b;
        }

        { // perm
            const auto set = xset::set::get(xset::panel::detcol_perm, p, mode);
            set->b = xset::set::get(xset::panel::detcol_perm, p)->b;
        }

        { // owner
            const auto set = xset::set::get(xset::panel::detcol_owner, p, mode);
            set->b = xset::set::get(xset::panel::detcol_owner, p)->b;
        }

        { // group
            const auto set = xset::set::get(xset::panel::detcol_group, p, mode);
            set->b = xset::set::get(xset::panel::detcol_group, p)->b;
        }

        { // atime
            const auto set = xset::set::get(xset::panel::detcol_atime, p, mode);
            set->b = xset::set::get(xset::panel::detcol_atime, p)->b;
        }

        { // btime
            const auto set = xset::set::get(xset::panel::detcol_btime, p, mode);
            set->b = xset::set::get(xset::panel::detcol_btime, p)->b;
        }

        { // ctime
            const auto set = xset::set::get(xset::panel::detcol_ctime, p, mode);
            set->b = xset::set::get(xset::panel::detcol_ctime, p)->b;
        }

        { // mtime
            const auto set = xset::set::get(xset::panel::detcol_mtime, p, mode);
            set->b = xset::set::get(xset::panel::detcol_mtime, p)->b;
        }

        update_views_all_windows(nullptr, browser);
    }
}

static void
on_popup_toggle_view(GtkMenuItem* menuitem, ptk::browser* browser) noexcept
{
    (void)menuitem;
    // get visiblity for correct mode
    const MainWindow* main_window = browser->main_window();
    const panel_t p = browser->panel();
    const xset::main_window_panel mode = main_window->panel_context.at(p);

    {
        const auto set = xset::set::get(xset::panel::show_toolbox, p, mode);
        set->b = xset::set::get(xset::panel::show_toolbox, p)->b;
    }

    {
        const auto set = xset::set::get(xset::panel::show_devmon, p, mode);
        set->b = xset::set::get(xset::panel::show_devmon, p)->b;
    }

    {
        const auto set = xset::set::get(xset::panel::show_dirtree, p, mode);
        set->b = xset::set::get(xset::panel::show_dirtree, p)->b;
    }

    update_views_all_windows(nullptr, browser);
}

static void
on_archive_default(GtkMenuItem* menuitem, const xset_t& set) noexcept
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
            set->b = xset::set::enabled::yes;
        }
        else
        {
            xset_set_b(arcname, false);
        }
    }
}

static void
on_hide_file(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->hide_selected(data->selected_files, data->cwd);
    }
}

static void
on_permission(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    if (data->browser)
    {
        data->browser->on_permission(menuitem, data->selected_files, data->cwd);
    }
}

#if (GTK_MAJOR_VERSION == 4)
void
ptk_file_menu_add_panel_view_menu(ptk::browser* browser, GtkWidget* menu,
                                  GtkEventController* accel_group) noexcept
#elif (GTK_MAJOR_VERSION == 3)
void
ptk_file_menu_add_panel_view_menu(ptk::browser* browser, GtkWidget* menu,
                                  GtkAccelGroup* accel_group) noexcept
#endif
{
    if (!browser || !menu || !browser->file_list_)
    {
        return;
    }
    const panel_t p = browser->panel();

    const MainWindow* main_window = browser->main_window();
    const xset::main_window_panel mode = main_window->panel_context.at(p);

    xset_set_cb(xset::name::view_refresh, (GFunc)ptk::wrapper::browser::refresh, browser);

    {
        const auto set = xset::set::get(xset::panel::show_toolbox, p);
        xset_set_cb(set, (GFunc)on_popup_toggle_view, browser);
        set->b = xset::set::get(xset::panel::show_toolbox, p, mode)->b;
    }

    {
        const auto set = xset::set::get(xset::panel::show_devmon, p);
        xset_set_cb(set, (GFunc)on_popup_toggle_view, browser);
        set->b = xset::set::get(xset::panel::show_devmon, p, mode)->b;
    }

    {
        const auto set = xset::set::get(xset::panel::show_dirtree, p);
        xset_set_cb(set, (GFunc)on_popup_toggle_view, browser);
        set->b = xset::set::get(xset::panel::show_dirtree, p, mode)->b;
    }

    xset_set_cb_panel(p, xset::panel::show_hidden, (GFunc)on_popup_show_hidden, browser);

    if (browser->is_view_mode(ptk::browser::view_mode::list_view))
    {
        { // size
            const auto set = xset::set::get(xset::panel::detcol_size, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_size, p, mode)->b;
        }

        { // size in bytes
            const auto set = xset::set::get(xset::panel::detcol_bytes, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_bytes, p, mode)->b;
        }

        { // type
            const auto set = xset::set::get(xset::panel::detcol_type, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_type, p, mode)->b;
        }

        { // MIME type
            const auto set = xset::set::get(xset::panel::detcol_mime, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_mime, p, mode)->b;
        }

        { // perm
            const auto set = xset::set::get(xset::panel::detcol_perm, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_perm, p, mode)->b;
        }

        { // owner
            const auto set = xset::set::get(xset::panel::detcol_owner, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_owner, p, mode)->b;
        }

        { // group
            const auto set = xset::set::get(xset::panel::detcol_group, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_group, p, mode)->b;
        }

        { // atime
            const auto set = xset::set::get(xset::panel::detcol_atime, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_atime, p, mode)->b;
        }

        { // btime
            const auto set = xset::set::get(xset::panel::detcol_btime, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_btime, p, mode)->b;
        }

        { // ctime
            const auto set = xset::set::get(xset::panel::detcol_ctime, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_ctime, p, mode)->b;
        }

        { // mtime
            const auto set = xset::set::get(xset::panel::detcol_mtime, p);
            xset_set_cb(set, (GFunc)on_popup_detailed_column, browser);
            set->b = xset::set::get(xset::panel::detcol_mtime, p, mode)->b;
        }

        xset_set_cb(xset::name::view_reorder_col, (GFunc)ptk::view::file_task::on_reorder, browser);

        {
            const auto set = xset::set::get(xset::name::view_columns);
            set->disable = false;

            std::vector<xset::name> context_menu_entries;
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
            set->context_menu_entries = context_menu_entries;
        }

        {
            const auto set = xset::set::get(xset::name::rubberband);
            xset_set_cb(set, (GFunc)main_window_rubberband_all, nullptr);
            set->disable = false;
        }
    }
    else
    {
        {
            const auto set = xset::set::get(xset::name::view_columns);
            set->disable = true;
        }

        {
            const auto set = xset::set::get(xset::name::rubberband);
            set->disable = true;
        }
    }

    {
        const auto set = xset::set::get(xset::name::view_thumb);
        xset_set_cb(set, (GFunc)main_window_toggle_thumbnails_all_windows, nullptr);
        set->b = browser->settings_->show_thumbnails ? xset::set::enabled::yes
                                                     : xset::set::enabled::unset;
    }

    if (browser->is_view_mode(ptk::browser::view_mode::icon_view))
    {
        const auto set = xset::set::get(xset::panel::list_large, p);
        set->b = xset::set::enabled::yes;
        set->disable = true;
    }
    else
    {
        const auto set = xset::set::get(xset::panel::list_large, p);
        xset_set_cb(set, (GFunc)on_popup_list_large, browser);
        set->disable = false;
        set->b = xset::set::get(xset::panel::list_large, p, mode)->b;
    }

    xset_t set_radio = nullptr;

    {
        const auto set = xset::set::get(xset::panel::list_detailed, p);
        xset_set_cb(set, (GFunc)on_popup_list_detailed, browser);
        set->menu.radio_set = nullptr;
        set_radio = set;
    }

    {
        const auto set = xset::set::get(xset::panel::list_icons, p);
        xset_set_cb(set, (GFunc)on_popup_list_icons, browser);
        set->menu.radio_set = set_radio;
    }

    {
        const auto set = xset::set::get(xset::panel::list_compact, p);
        xset_set_cb(set, (GFunc)on_popup_list_compact, browser);
        set->menu.radio_set = set_radio;
    }

    { // name
        const auto set = xset::set::get(xset::name::sortby_name);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::name));
        set->b = browser->is_sort_order(ptk::browser::sort_order::name) ? xset::set::enabled::yes
                                                                        : xset::set::enabled::no;
        set->menu.radio_set = nullptr;
        set_radio = set;
    }

    { // size
        const auto set = xset::set::get(xset::name::sortby_size);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::size));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::size) ? xset::set::enabled::yes
                                                                        : xset::set::enabled::no;
    }

    { // size in bytes
        const auto set = xset::set::get(xset::name::sortby_bytes);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::bytes));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::bytes) ? xset::set::enabled::yes
                                                                         : xset::set::enabled::no;
    }

    { // type
        const auto set = xset::set::get(xset::name::sortby_type);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::type));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::type) ? xset::set::enabled::yes
                                                                        : xset::set::enabled::no;
    }

    { // MIME type
        const auto set = xset::set::get(xset::name::sortby_mime);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::mime));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::mime) ? xset::set::enabled::yes
                                                                        : xset::set::enabled::no;
    }

    { // perm
        const auto set = xset::set::get(xset::name::sortby_perm);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::perm));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::perm) ? xset::set::enabled::yes
                                                                        : xset::set::enabled::no;
    }

    { // owner
        const auto set = xset::set::get(xset::name::sortby_owner);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::owner));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::owner) ? xset::set::enabled::yes
                                                                         : xset::set::enabled::no;
    }

    { // group
        const auto set = xset::set::get(xset::name::sortby_group);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::group));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::group) ? xset::set::enabled::yes
                                                                         : xset::set::enabled::no;
    }

    { // atime
        const auto set = xset::set::get(xset::name::sortby_atime);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::atime));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::atime) ? xset::set::enabled::yes
                                                                         : xset::set::enabled::no;
    }

    { // btime
        const auto set = xset::set::get(xset::name::sortby_btime);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::btime));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::btime) ? xset::set::enabled::yes
                                                                         : xset::set::enabled::no;
    }

    { // ctime
        const auto set = xset::set::get(xset::name::sortby_ctime);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::ctime));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::ctime) ? xset::set::enabled::yes
                                                                         : xset::set::enabled::no;
    }

    { // mtime
        const auto set = xset::set::get(xset::name::sortby_mtime);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", magic_enum::enum_integer(ptk::browser::sort_order::mtime));
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_order(ptk::browser::sort_order::mtime) ? xset::set::enabled::yes
                                                                         : xset::set::enabled::no;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_ascend);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", -1);
        set->b = browser->is_sort_type(GtkSortType::GTK_SORT_ASCENDING) ? xset::set::enabled::yes
                                                                        : xset::set::enabled::no;
        set->menu.radio_set = nullptr;
        set_radio = set;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_descend);
        xset_set_cb(set, (GFunc)on_popup_sortby, browser);
        xset_set_ob(set, "sortorder", -2);
        set->menu.radio_set = set_radio;
        set->b = browser->is_sort_type(GtkSortType::GTK_SORT_DESCENDING) ? xset::set::enabled::yes
                                                                         : xset::set::enabled::no;
    }

    // this crashes if !browser->file_list so do not allow
    if (browser->file_list_)
    {
        {
            const auto set = xset::set::get(xset::name::sortx_natural);
            xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
            set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_natural
                         ? xset::set::enabled::yes
                         : xset::set::enabled::no;
        }

        {
            const auto set = xset::set::get(xset::name::sortx_case);
            xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
            set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_case
                         ? xset::set::enabled::yes
                         : xset::set::enabled::no;
            set->disable = !PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_natural;
        }

        {
            const auto set = xset::set::get(xset::name::sortx_directories);
            xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
            set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_dir_ ==
                             ptk::file_list::sort_dir::first
                         ? xset::set::enabled::yes
                         : xset::set::enabled::no;
            set->menu.radio_set = nullptr;
            set_radio = set;
        }

        {
            const auto set = xset::set::get(xset::name::sortx_files);
            xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
            set->menu.radio_set = set_radio;
            set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_dir_ ==
                             ptk::file_list::sort_dir::last
                         ? xset::set::enabled::yes
                         : xset::set::enabled::no;
        }

        {
            const auto set = xset::set::get(xset::name::sortx_mix);
            xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
            set->menu.radio_set = set_radio;
            set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_dir_ ==
                             ptk::file_list::sort_dir::mixed
                         ? xset::set::enabled::yes
                         : xset::set::enabled::no;
        }

        {
            const auto set = xset::set::get(xset::name::sortx_hidfirst);
            xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
            set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_hidden_first
                         ? xset::set::enabled::yes
                         : xset::set::enabled::no;
            set->menu.radio_set = nullptr;
            set_radio = set;
        }

        {
            const auto set = xset::set::get(xset::name::sortx_hidlast);
            xset_set_cb(set, (GFunc)on_popup_sort_extra, browser);
            set->menu.radio_set = set_radio;
            set->b = PTK_FILE_LIST_REINTERPRET(browser->file_list_)->sort_hidden_first
                         ? xset::set::enabled::no
                         : xset::set::enabled::yes;
        }
    }

    {
        const auto set = xset::set::get(xset::name::view_list_style);

        std::vector<xset::name> context_menu_entries;
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
        set->context_menu_entries = context_menu_entries;
    }

    {
        const auto set = xset::set::get(xset::name::con_view);
        set->disable = !browser->file_list_;

        std::vector<xset::name> context_menu_entries;
        if (p == panel_1)
        {
            context_menu_entries = {
                xset::name::panel1_show_toolbox,
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
        set->context_menu_entries = context_menu_entries;

        xset_add_menuitem(browser, menu, accel_group, set);
    }
}

static void
ptk_file_menu_free(ptk::file_menu* data) noexcept
{
    delete data;
}

/* Retrieve popup menu for selected file(s) */
GtkWidget*
ptk_file_menu_new(ptk::browser* browser,
                  const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    assert(browser != nullptr);

    std::filesystem::path file_path;
    std::shared_ptr<vfs::file> file = nullptr;
    if (!selected_files.empty())
    {
        file = selected_files.front();
        file_path = file->path();
    }

    const auto& cwd = browser->cwd();

    auto* const data = new ptk::file_menu;
    data->cwd = cwd;
    data->browser = browser;
    data->file_path = file_path;
    data->file = file;
    data->selected_files =
        std::vector<std::shared_ptr<vfs::file>>(selected_files.cbegin(), selected_files.cend());

#if (GTK_MAJOR_VERSION == 4)
    data->accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    data->accel_group = gtk_accel_group_new();
#endif

    GtkWidget* popup = gtk_menu_new();

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

    g_object_weak_ref(G_OBJECT(popup), (GWeakNotify)ptk_file_menu_free, data);
    // clang-format off
    g_signal_connect_after(G_OBJECT(popup), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    // clang-format on

    const bool is_dir = (file && file->is_directory());
    // Note: network filesystems may become unresponsive here
    const bool is_text = file && file->mime_type()->is_text();

    // test R/W access to cwd instead of selected file
    // Note: network filesystems may become unresponsive here
    // const auto no_read_access = faccessat(0, cwd, R_OK, AT_EACCESS);
    const auto no_write_access = faccessat(0, cwd.c_str(), W_OK, AT_EACCESS);

#if (GTK_MAJOR_VERSION == 4)
    logger::debug<logger::domain::ptk>("TODO - PORT - GdkClipboard");
    bool is_clip = false;
#elif (GTK_MAJOR_VERSION == 3)
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    const bool is_clip =
        gtk_clipboard_wait_is_target_available(
            clip,
            gdk_atom_intern("x-special/gnome-copied-files", false)) ||
        gtk_clipboard_wait_is_target_available(clip, gdk_atom_intern("text/uri-list", false));
#endif

    const panel_t p = browser->panel();

    const auto counts = browser->get_tab_panel_counts();
    const panel_t panel_count = counts.panel_count;
    const tab_t tab_count = counts.tab_count;
    const tab_t tab_num = counts.tab_num;

    // Get mime type and apps
    std::vector<std::string> apps{};
    if (file)
    {
        apps = file->mime_type()->actions();
    }

    GtkMenuItem* item = nullptr;

    // Open >
    const bool set_disable = selected_files.empty();

    {
        const auto set = xset::set::get(xset::name::con_open);
        set->disable = set_disable;
        item = GTK_MENU_ITEM(xset_add_menuitem(browser, popup, accel_group, set));
    }

    if (!selected_files.empty())
    {
        GtkWidget* submenu = gtk_menu_item_get_submenu(item);

        // Execute
        if (!is_dir && file && (file->is_desktop_entry() || file->mime_type()->is_executable()))
        {
            // Note: network filesystems may become unresponsive here
            const auto set = xset::set::get(xset::name::open_execute);
            xset_set_cb(set, (GFunc)on_popup_open_activate, data);
            xset_add_menuitem(browser, submenu, accel_group, set);
        }

        // Prepare archive commands
        xset_t set_archive_extract = nullptr;
        xset_t set_archive_extract_to = nullptr;
        xset_t set_archive_open = nullptr;

        const auto is_archive = [](const auto& file) { return file->mime_type()->is_archive(); };
        if (std::ranges::all_of(selected_files, is_archive))
        {
            set_archive_extract = xset::set::get(xset::name::archive_extract);
            xset_set_cb(set_archive_extract, (GFunc)on_popup_extract_here_activate, data);
            set_archive_extract->disable = no_write_access;

            set_archive_extract_to = xset::set::get(xset::name::archive_extract_to);
            xset_set_cb(set_archive_extract_to, (GFunc)on_popup_extract_to_activate, data);

            set_archive_open = xset::set::get(xset::name::archive_open);
            xset_set_cb(set_archive_open, (GFunc)on_popup_extract_open_activate, data);

            xset_t set_radio = nullptr;

            {
                const auto set = xset::set::get(xset::name::archive_default_open_with_app);
                // do NOT use set = xset_set_cb here or wrong set is passed
                xset_set_cb(xset::name::archive_default_open_with_app,
                            (GFunc)on_archive_default,
                            set.get());
                set->menu.radio_set = nullptr;
                set_radio = set;
            }

            {
                const auto set = xset::set::get(xset::name::archive_default_extract);
                xset_set_cb(xset::name::archive_default_extract,
                            (GFunc)on_archive_default,
                            set.get());
                set->menu.radio_set = set_radio;
            }

            {
                const auto set = xset::set::get(xset::name::archive_default_extract_to);
                xset_set_cb(xset::name::archive_default_extract_to,
                            (GFunc)on_archive_default,
                            set.get());
                set->menu.radio_set = set_radio;
            }

            {
                const auto set = xset::set::get(xset::name::archive_default_open_with_archiver);
                xset_set_cb(xset::name::archive_default_open_with_archiver,
                            (GFunc)on_archive_default,
                            set.get());
                set->menu.radio_set = set_radio;
            }

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
                                  xset::set::get(xset::name::archive_default));
                set_archive_extract = nullptr;

                // separator
                item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
                gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));
            }
        }

        GtkWidget* app_menu_item = nullptr;

        // add apps
        i32 icon_w = 0;
        i32 icon_h = 0;
        gtk_icon_size_lookup(GtkIconSize::GTK_ICON_SIZE_MENU, icon_w.unwrap(), icon_h.unwrap());
        if (is_text)
        {
            const auto txt_type =
                vfs::mime_type::create_from_type(vfs::constants::mime_type::plain_text);
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
                const auto desktop = vfs::desktop::create(app);
                if (!desktop)
                {
                    continue;
                }

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

                // clang-format off
                g_object_set_data(G_OBJECT(app_menu_item), "menu", submenu);
                g_object_set_data_full(G_OBJECT(app_menu_item), "desktop_file", ::utils::strdup(app.data()), std::free);

                g_signal_connect(G_OBJECT(app_menu_item), "activate", G_CALLBACK(on_popup_run_app), (void*)data);
                g_signal_connect(G_OBJECT(app_menu_item), "button-press-event", G_CALLBACK(on_app_button_press), (void*)data);
                g_signal_connect(G_OBJECT(app_menu_item), "button-release-event", G_CALLBACK(on_app_button_press), (void*)data);
                // clang-format on
            }
        }

        // Edit / Dir
        if ((is_dir && browser) || (is_text && selected_files.size() == 1))
        {
            item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));

            if (is_text)
            {
                // Edit
                const auto set = xset::set::get(xset::name::open_edit);
                xset_set_cb(set, (GFunc)on_file_edit, data);
                xset_add_menuitem(browser, submenu, accel_group, set);
            }
            else if (browser && is_dir)
            {
                // Open Dir
                {
                    const auto set = xset::set::get(xset::name::opentab_prev);
                    xset_set_cb(set, (GFunc)on_open_in_tab, data);
                    xset_set_ob(set, "tab", tab_control_code_prev);
                    set->disable = (tab_num == 1);
                }

                {
                    const auto set = xset::set::get(xset::name::opentab_next);
                    xset_set_cb(set, (GFunc)on_open_in_tab, data);
                    xset_set_ob(set, "tab", tab_control_code_next);
                    set->disable = (tab_num == tab_count);
                }

                {
                    const auto set = xset::set::get(xset::name::opentab_new);
                    xset_set_cb(set, (GFunc)on_popup_open_in_new_tab_activate, data);
                }

                for (tab_t tab : TABS)
                {
                    const std::string name = std::format("opentab_{}", tab);
                    const auto set = xset::set::get(name);
                    xset_set_cb(set, (GFunc)on_open_in_tab, data);
                    xset_set_ob(set, "tab", tab);
                    set->disable = (tab > tab_count) || (tab == tab_num);
                }

                {
                    const auto set = xset::set::get(xset::name::open_in_panel_prev);
                    xset_set_cb(set, (GFunc)on_open_in_panel, data);
                    xset_set_ob(set, "panel", panel_control_code_prev);
                    set->disable = (panel_count == 1);
                }

                {
                    const auto set = xset::set::get(xset::name::open_in_panel_next);
                    xset_set_cb(set, (GFunc)on_open_in_panel, data);
                    xset_set_ob(set, "panel", panel_control_code_next);
                    set->disable = (panel_count == 1);
                }

                for (panel_t panel : PANELS)
                {
                    const std::string name = std::format("open_in_panel_{}", panel);
                    const auto set = xset::set::get(name);
                    xset_set_cb(set, (GFunc)on_open_in_panel, data);
                    xset_set_ob(set, "panel", panel);
                    // set->disable = ( p == i );
                }

                {
                    const auto set = xset::set::get(xset::name::open_in_tab);
                    xset_add_menuitem(browser, submenu, accel_group, set);
                }

                {
                    const auto set = xset::set::get(xset::name::open_in_panel);
                    xset_add_menuitem(browser, submenu, accel_group, set);
                }
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
            xset_add_menuitem(browser,
                              submenu,
                              accel_group,
                              xset::set::get(xset::name::archive_default));
        }

        { // Choose, open with other app
            item = GTK_MENU_ITEM(gtk_separator_menu_item_new());
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), GTK_WIDGET(item));

            const auto set = xset::set::get(xset::name::open_other);
            xset_set_cb(set, (GFunc)on_popup_open_with_another_activate, data);
            xset_add_menuitem(browser, submenu, accel_group, set);
        }

        { // Open With Default
            const auto set = xset::set::get(xset::name::open_all);
            xset_set_cb(set, (GFunc)on_popup_open_all, data);
            item = GTK_MENU_ITEM(xset_add_menuitem(browser, submenu, accel_group, set));
        }

        //
        g_signal_connect(G_OBJECT(submenu), "key-press-event", G_CALLBACK(app_menu_keypress), data);
    }

    // Go >
    if (browser)
    {
        {
            const auto set = xset::set::get(xset::name::go_back);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::go_back, browser);
            set->disable = !browser->history_->has_back();
        }

        {
            const auto set = xset::set::get(xset::name::go_forward);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::go_forward, browser);
            set->disable = !browser->history_->has_forward();
        }

        {
            const auto set = xset::set::get(xset::name::go_up);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::go_up, browser);
            set->disable = std::filesystem::equivalent(cwd, "/");
            xset_set_cb(xset::name::go_home, (GFunc)ptk::wrapper::browser::go_home, browser);
            xset_set_cb(xset::name::edit_canon, (GFunc)on_popup_canon, data);
        }

        {
            const auto set = xset::set::get(xset::name::focus_path_bar);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::focus, browser);
            xset_set_ob(set,
                        "focus",
                        magic_enum::enum_integer(ptk::browser::focus_widget::path_bar));
        }

        {
            const auto set = xset::set::get(xset::name::focus_search_bar);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::focus, browser);
            xset_set_ob(set,
                        "focus",
                        magic_enum::enum_integer(ptk::browser::focus_widget::search_bar));
        }

        {
            const auto set = xset::set::get(xset::name::focus_filelist);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::focus, browser);
            xset_set_ob(set,
                        "focus",
                        magic_enum::enum_integer(ptk::browser::focus_widget::filelist));
        }

        {
            const auto set = xset::set::get(xset::name::focus_dirtree);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::focus, browser);
            xset_set_ob(set,
                        "focus",
                        magic_enum::enum_integer(ptk::browser::focus_widget::dirtree));
        }

        {
            const auto set = xset::set::get(xset::name::focus_device);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::focus, browser);
            xset_set_ob(set, "focus", magic_enum::enum_integer(ptk::browser::focus_widget::device));
        }

        // Go > Tab >
        {
            const auto set = xset::set::get(xset::name::tab_prev);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::go_tab, browser);
            xset_set_ob(set, "tab", tab_control_code_prev);
            set->disable = (tab_count < 2);
        }

        {
            const auto set = xset::set::get(xset::name::tab_next);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::go_tab, browser);
            xset_set_ob(set, "tab", tab_control_code_next);
            set->disable = (tab_count < 2);
        }

        {
            const auto set = xset::set::get(xset::name::tab_close);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::go_tab, browser);
            xset_set_ob(set, "tab", tab_control_code_close);
        }

        {
            const auto set = xset::set::get(xset::name::tab_restore);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::go_tab, browser);
            xset_set_ob(set, "tab", tab_control_code_restore);
        }

        for (tab_t tab : TABS)
        {
            const std::string name = std::format("tab_{}", tab);
            const auto set = xset::set::get(name);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::go_tab, browser);
            xset_set_ob(set, "tab", tab);
            set->disable = (tab > tab_count) || (tab == tab_num);
        }

        {
            const auto set = xset::set::get(xset::name::con_go);
            xset_add_menuitem(browser, popup, accel_group, set);
        }

        // New >
        xset_set_cb(xset::name::new_file, (GFunc)on_popup_new_text_file_activate, data);
        xset_set_cb(xset::name::new_directory, (GFunc)on_popup_new_folder_activate, data);
        xset_set_cb(xset::name::new_link, (GFunc)on_popup_new_link_activate, data);

        {
            const auto set = xset::set::get(xset::name::new_archive);
            xset_set_cb(set, (GFunc)on_popup_compress_activate, data);
            set->disable = set_disable;
        }

        {
            const auto set = xset::set::get(xset::name::tab_new);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::new_tab, browser);
            set->disable = !browser;
        }

        {
            const auto set = xset::set::get(xset::name::tab_new_here);
            xset_set_cb(set, (GFunc)on_popup_open_in_new_tab_here, data);
            set->disable = !browser;
        }

        {
            const auto set = xset::set::get(xset::name::new_bookmark);
            xset_set_cb(set, (GFunc)on_new_bookmark, data);
            set->disable = !browser;
        }

        {
            const auto set = xset::set::get(xset::name::open_new);
            xset_add_menuitem(browser, popup, accel_group, set);
        }

        {
            const auto set = xset::set::get(xset::name::separator);
            xset_add_menuitem(browser, popup, accel_group, set);
        }

        // Edit
        {
            const auto set = xset::set::get(xset::name::copy_name);
            xset_set_cb(set, (GFunc)on_popup_copy_name_activate, data);
            set->disable = set_disable;
        }

        {
            const auto set = xset::set::get(xset::name::copy_path);
            xset_set_cb(set, (GFunc)on_popup_copy_text_activate, data);
            set->disable = set_disable;
        }

        {
            const auto set = xset::set::get(xset::name::copy_parent);
            xset_set_cb(set, (GFunc)on_popup_copy_parent_activate, data);
            set->disable = set_disable;
        }

        {
            const auto set = xset::set::get(xset::name::paste_link);
            xset_set_cb(set, (GFunc)on_popup_paste_link_activate, data);
            set->disable = !is_clip || no_write_access;
        }

        {
            const auto set = xset::set::get(xset::name::paste_target);
            xset_set_cb(set, (GFunc)on_popup_paste_target_activate, data);
            set->disable = !is_clip || no_write_access;
        }

        {
            const auto set = xset::set::get(xset::name::paste_as);
            xset_set_cb(set, (GFunc)on_popup_paste_as_activate, data);
            set->disable = !is_clip;
        }

        {
            const auto set = xset::set::get(xset::name::edit_hide);
            xset_set_cb(set, (GFunc)on_hide_file, data);
            set->disable = set_disable || no_write_access || !browser;
        }

        xset_set_cb(xset::name::select_all,
                    (GFunc)ptk::wrapper::browser::select_all,
                    data->browser);

        {
            const auto set = xset::set::get(xset::name::select_un);
            xset_set_cb(set, (GFunc)ptk::wrapper::browser::unselect_all, browser);
            set->disable = set_disable;
        }

        xset_set_cb(xset::name::select_invert,
                    (GFunc)ptk::wrapper::browser::invert_selection,
                    browser);
        xset_set_cb(xset::name::select_patt, (GFunc)on_popup_select_pattern, data);

        static constexpr std::array<xset::name, 40> copycmds{
            xset::name::copy_loc,        xset::name::copy_loc_last,   xset::name::copy_tab_prev,
            xset::name::copy_tab_next,   xset::name::copy_tab_1,      xset::name::copy_tab_2,
            xset::name::copy_tab_3,      xset::name::copy_tab_4,      xset::name::copy_tab_5,
            xset::name::copy_tab_6,      xset::name::copy_tab_7,      xset::name::copy_tab_8,
            xset::name::copy_tab_9,      xset::name::copy_tab_10,     xset::name::copy_panel_prev,
            xset::name::copy_panel_next, xset::name::copy_panel_1,    xset::name::copy_panel_2,
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
            const auto set = xset::set::get(copycmd);
            xset_set_cb(set, (GFunc)on_copycmd, data);
            xset_set_ob(set, "set", set->name());
        }

        // enables
        {
            const auto set = xset::set::get(xset::name::copy_loc_last);
        }

        {
            const auto set = xset::set::get(xset::name::move_loc_last);
        }

        {
            const auto set = xset::set::get(xset::name::copy_tab_prev);
            set->disable = (tab_num == 1);
        }

        {
            const auto set = xset::set::get(xset::name::copy_tab_next);
            set->disable = (tab_num == tab_count);
        }

        {
            const auto set = xset::set::get(xset::name::move_tab_prev);
            set->disable = (tab_num == 1);
        }

        {
            const auto set = xset::set::get(xset::name::move_tab_next);
            set->disable = (tab_num == tab_count);
        }

        {
            const auto set = xset::set::get(xset::name::copy_panel_prev);
            set->disable = (panel_count < 2);
        }

        {
            const auto set = xset::set::get(xset::name::copy_panel_next);
            set->disable = (panel_count < 2);
        }

        {
            const auto set = xset::set::get(xset::name::move_panel_prev);
            set->disable = (panel_count < 2);
        }

        {
            const auto set = xset::set::get(xset::name::move_panel_next);
            set->disable = (panel_count < 2);
        }

        for (tab_t tab : TABS)
        {
            {
                const auto copy_tab = std::format("copy_tab_{}", tab);
                const auto set = xset::set::get(copy_tab);
                set->disable = (tab > tab_count) || (tab == tab_num);
            }

            {
                const auto move_tab = std::format("move_tab_{}", tab);
                const auto set = xset::set::get(move_tab);
                set->disable = (tab > tab_count) || (tab == tab_num);
            }
        }

        for (panel_t panel : PANELS)
        {
            const bool b = browser->is_panel_visible(panel);

            {
                const auto copy_panel = std::format("copy_panel_{}", panel);
                const auto set = xset::set::get(copy_panel);
                set->disable = (panel == p) || !b;
            }

            {
                const auto move_panel = std::format("move_panel_{}", panel);
                const auto set = xset::set::get(move_panel);
                set->disable = (panel == p) || !b;
            }
        }

        {
            const auto set = xset::set::get(xset::name::copy_to);
            set->disable = set_disable;
        }

        {
            const auto set = xset::set::get(xset::name::move_to);
            set->disable = set_disable;
        }

        {
            const auto set = xset::set::get(xset::name::edit_submenu);
            xset_add_menuitem(browser, popup, accel_group, set);
        }
    }

    {
        const auto set = xset::set::get(xset::name::edit_cut);
        xset_set_cb(set, (GFunc)on_popup_cut_activate, data);
        set->disable = set_disable;
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::edit_copy);
        xset_set_cb(set, (GFunc)on_popup_copy_activate, data);
        set->disable = set_disable;
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::edit_paste);
        xset_set_cb(set, (GFunc)on_popup_paste_activate, data);
        set->disable = !is_clip || no_write_access;
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::edit_rename);
        xset_set_cb(set, (GFunc)on_popup_rename_activate, data);
        set->disable = set_disable;
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::edit_batch_rename);
        xset_set_cb(set, (GFunc)on_popup_batch_rename_activate, data);
        set->disable = set_disable;
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::edit_trash);
        xset_set_cb(set, (GFunc)on_popup_trash_activate, data);
        set->disable = set_disable || no_write_access;
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::edit_delete);
        xset_set_cb(set, (GFunc)on_popup_delete_activate, data);
        set->disable = set_disable || no_write_access;
        xset_add_menuitem(browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::separator);
        xset_add_menuitem(browser, popup, accel_group, set);
    }

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
            const auto set = xset::set::get(permcmd);
            xset_set_cb(set, (GFunc)on_permission, data);
            xset_set_ob(set, "set", set->name());
        }

        {
            const auto set = xset::set::get(xset::name::prop_quick);
            set->disable = no_write_access || set_disable;
        }

        {
            const auto set = xset::set::get(xset::name::con_prop);
            xset_add_menuitem(browser, popup, accel_group, set);
        }
    }

    gtk_widget_show_all(GTK_WIDGET(popup));

    g_signal_connect(G_OBJECT(popup), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    return popup;
}

static void
on_popup_open_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    ptk::action::open_files_with_app(data->cwd,
                                     data->selected_files,
                                     "",
                                     data->browser,
                                     true,
                                     false);
}

static void
on_popup_open_with_another_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;

    std::shared_ptr<vfs::mime_type> mime_type = nullptr;
    if (data->file)
    {
        mime_type = data->file->mime_type();
    }
    else
    {
        mime_type = vfs::mime_type::create_from_type(vfs::constants::mime_type::directory);
    }

    GtkWidget* parent = nullptr;
    if (data->browser)
    {
#if (GTK_MAJOR_VERSION == 4)
        parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(data->browser)));
#elif (GTK_MAJOR_VERSION == 3)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
#endif
    }

    const auto app =
        ptk_choose_app_for_mime_type(GTK_WINDOW(parent), mime_type, false, true, true, false);
    if (app)
    {
        ptk::action::open_files_with_app(data->cwd,
                                         data->selected_files,
                                         app.value(),
                                         data->browser,
                                         false,
                                         false);
    }
}

static void
on_popup_open_all(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    ptk::action::open_files_with_app(data->cwd,
                                     data->selected_files,
                                     "",
                                     data->browser,
                                     false,
                                     true);
}

static void
on_popup_run_app(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    const char* desktop_file =
        static_cast<const char*>(g_object_get_data(G_OBJECT(menuitem), "desktop_file"));

    const auto desktop = vfs::desktop::create(desktop_file);
    if (!desktop)
    {
        return;
    }

    ptk::action::open_files_with_app(data->cwd,
                                     data->selected_files,
                                     desktop->name(),
                                     data->browser,
                                     false,
                                     false);
}

static std::optional<std::filesystem::path>
get_shared_desktop_file_location(const std::string_view name) noexcept
{
    for (const std::filesystem::path sys_dir : Glib::get_system_data_dirs())
    {
        auto ret = vfs::mime_type_locate_desktop_file(sys_dir, name);
        if (ret)
        {
            return ret;
        }
    }
    return std::nullopt;
}

static void
app_job(GtkWidget* item, GtkWidget* app_item) noexcept
{
    char* str = nullptr;
    std::string str2;

    std::string command;

    const std::string desktop_file =
        static_cast<const char*>(g_object_get_data(G_OBJECT(app_item), "desktop_file"));

    const auto desktop = vfs::desktop::create(desktop_file);
    if (!desktop)
    {
        return;
    }

    const auto job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job"));
    ptk::file_menu* data = PTK_FILE_MENU(g_object_get_data(G_OBJECT(item), "data"));
    if (!(data && data->file))
    {
        return;
    }

    const auto mime_type = data->file->mime_type();

    switch (ptk::file_menu::app_job(job))
    {
        case ptk::file_menu::app_job::default_action:
        {
            mime_type->set_default_action(desktop->name());
            break;
        }
        case ptk::file_menu::app_job::edit:
        {
            const auto path = vfs::user::data() / "applications" / desktop->name();
            if (!std::filesystem::exists(path))
            {
                const auto share_desktop = vfs::mime_type_locate_desktop_file(desktop->name());
                if (!share_desktop || std::filesystem::equivalent(share_desktop.value(), path))
                {
                    return;
                }

                const auto response = ptk::dialog::message(
                    GTK_WINDOW(data->browser),
                    GtkMessageType::GTK_MESSAGE_QUESTION,
                    "Copy Desktop File",
                    GtkButtonsType::GTK_BUTTONS_YES_NO,
                    std::format("The file '{0}' does not exist.\n\nBy copying '{1}' to '{0}' and "
                                "editing it, you can adjust the behavior and appearance of this "
                                "application for the current user.\n\nCreate this copy now?",
                                path.string(),
                                share_desktop->string()));

                if (response != GtkResponseType::GTK_RESPONSE_YES)
                {
                    break;
                }

                // need to copy
                command = std::format("cp -a  {} {}", share_desktop->string(), path.string());
                Glib::spawn_command_line_sync(command);
                if (!std::filesystem::exists(path))
                {
                    return;
                }
            }
            on_file_edit(nullptr, data);
            vfs::utils::open_editor(path);
            break;
        }
        case ptk::file_menu::app_job::view:
        {
            const auto desktop_path = get_shared_desktop_file_location(desktop->name());
            if (desktop_path)
            {
                vfs::utils::open_editor(desktop_path.value());
            }
            break;
        }
        case ptk::file_menu::app_job::edit_list:
        {
            // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
            const auto path = vfs::user::config() / "mimeapps.list";
            vfs::utils::open_editor(path);
            break;
        }
        case ptk::file_menu::app_job::browse:
        {
            const auto path = vfs::user::data() / "applications";
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);

            if (data->browser)
            {
                data->browser->signal_open_file().emit(data->browser,
                                                       path,
                                                       ptk::browser::open_action::new_tab);
            }
            break;
        }
        case ptk::file_menu::app_job::browse_shared:
        {
            std::filesystem::path path;
            const auto desktop_path = get_shared_desktop_file_location(desktop->name());
            if (desktop_path)
            {
                path = desktop_path->parent_path();
            }
            else
            {
                path = "/usr/share/applications";
            }
            if (data->browser)
            {
                data->browser->signal_open_file().emit(data->browser,
                                                       path,
                                                       ptk::browser::open_action::new_tab);
            }
        }
        break;
        case ptk::file_menu::app_job::edit_type:
        {
            const auto mime_path = vfs::user::data() / "mime" / "packages";
            std::filesystem::create_directories(mime_path);
            std::filesystem::permissions(mime_path, std::filesystem::perms::owner_all);
            str2 = ztd::replace(mime_type->type(), "/", "-");
            str2 = std::format("{}.xml", str2);
            const auto mime_file = vfs::user::data() / "mime" / "packages" / str2;
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

                const auto response = ptk::dialog::message(GTK_WINDOW(data->browser),
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

                const auto buffer = vfs::utils::read_file(usr_path);
                if (buffer)
                {
                    logger::warn<logger::domain::ptk>("Error reading {}: {}",
                                                      usr_path.string(),
                                                      buffer.error().message());
                }

                auto contents = buffer.value();

                if (!contents.empty())
                {
                    char* contents2 = ::utils::strdup(contents);
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

                [[maybe_unused]] auto ec = vfs::utils::write_file(mime_file, contents);
            }
            if (std::filesystem::exists(mime_file))
            {
                vfs::utils::open_editor(mime_file);
            }

            vfs::mime_monitor();
            break;
        }
        case ptk::file_menu::app_job::view_type:
        {
            str2 = std::format("{}.xml", mime_type->type());
            const auto path = std::filesystem::path() / "/usr/share/mime" / str2;
            if (std::filesystem::exists(path))
            {
                vfs::utils::open_editor(path);
            }
            break;
        }
        case ptk::file_menu::app_job::view_over:
        {
            const std::filesystem::path path = "/usr/share/mime/packages/Overrides.xml";
            vfs::utils::open_editor(path);
            break;
        }
        case ptk::file_menu::app_job::browse_mime_usr:
        {
            if (data->browser)
            {
                const std::filesystem::path path = "/usr/share/mime/packages";
                data->browser->signal_open_file().emit(data->browser,
                                                       path,
                                                       ptk::browser::open_action::new_tab);
            }
            break;
        }
        case ptk::file_menu::app_job::browse_mime:
        {
            const auto path = vfs::user::data() / "mime" / "packages";
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
            if (data->browser)
            {
                data->browser->signal_open_file().emit(data->browser,
                                                       path,
                                                       ptk::browser::open_action::new_tab);
            }
            vfs::mime_monitor();
            break;
        }
        case ptk::file_menu::app_job::update:
        {
            const auto data_dir = vfs::user::data();
            command = std::format("update-mime-database {}/mime", data_dir.string());
            logger::info<logger::domain::ptk>("COMMAND({})", command);
            Glib::spawn_command_line_async(command);

            command = std::format("update-desktop-database {}/applications", data_dir.string());
            logger::info<logger::domain::ptk>("COMMAND({})", command);
            Glib::spawn_command_line_async(command);
            break;
        }
        case ptk::file_menu::app_job::usr:
            break;
    }
}

static bool
app_menu_keypress(GtkWidget* menu, GdkEvent* event, ptk::file_menu* data) noexcept
{
    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(menu));
    if (!item)
    {
        return false;
    }

    // if original menu, desktop will be set
    // const std::string desktop_file = static_cast<const char*>(g_object_get_data(G_OBJECT(item), "desktop_file"));
    // const auto desktop = vfs::desktop::create(desktop_file);
    // else if app menu, data will be set
    // ptk::file_menu* app_data = PTK_FILE_MENU(g_object_get_data(G_OBJECT(item), "data"));

    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto keyval = gdk_key_event_get_keyval(event);
    const auto time_point = std::chrono::system_clock::from_time_t(gdk_event_get_time(event));

    if (keymod == 0)
    {
        if (keyval == GDK_KEY_F2 || keyval == GDK_KEY_Menu)
        {
            show_app_menu(menu, item, data, 0, time_point);
            return true;
        }
    }
    return false;
}

static void
on_app_menu_hide(GtkWidget* widget, GtkWidget* app_menu) noexcept
{
    gtk_widget_set_sensitive(widget, true);
    gtk_menu_shell_deactivate(GTK_MENU_SHELL(app_menu));
}

static GtkWidget*
app_menu_additem(GtkWidget* menu, const std::string_view label, ptk::file_menu::app_job job,
                 GtkWidget* app_item, ptk::file_menu* data) noexcept
{
    GtkWidget* item = gtk_menu_item_new_with_mnemonic(label.data());

    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(magic_enum::enum_integer(job)));
    g_object_set_data(G_OBJECT(item), "data", data);
    gtk_container_add(GTK_CONTAINER(menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(app_job), app_item);
    return item;
}

static void
show_app_menu(GtkWidget* menu, GtkWidget* app_item, ptk::file_menu* data, u32 button,
              const std::chrono::system_clock::time_point time_point) noexcept
{
    (void)button;
    (void)time_point;
    GtkWidget* newitem = nullptr;
    GtkWidget* submenu = nullptr;
    std::string str;

    if (!(data && data->file))
    {
        return;
    }

    const auto type = data->file->mime_type()->type();

    const std::string desktop_file =
        static_cast<const char*>(g_object_get_data(G_OBJECT(app_item), "desktop_file"));

    const auto desktop = vfs::desktop::create(desktop_file);
    if (!desktop)
    {
        return;
    }

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
        const auto path = vfs::user::data() / "applications" / desktop->name();
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
    const auto usr_mime_path = vfs::user::data() / "mime/packages" / str;
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
    g_signal_connect(G_OBJECT(submenu), "key_press_event", G_CALLBACK(app_menu_keypress), data);

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

    g_signal_connect(G_OBJECT(menu), "hide", G_CALLBACK(on_app_menu_hide), app_menu);
    g_signal_connect(G_OBJECT(app_menu), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    g_signal_connect(G_OBJECT(app_menu), "key_press_event", G_CALLBACK(app_menu_keypress), data);

    gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(app_menu), true);
    // this is required when showing the menu via F2 or Menu key for focus
    gtk_menu_shell_select_first(GTK_MENU_SHELL(app_menu), true);
}

static bool
on_app_button_press(GtkWidget* item, GdkEvent* event, ptk::file_menu* data) noexcept
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
        // true for issue #521 where a right-click also left-clicks the first
        // menu item in some GTK2/3 themes.
        return true;
    }
    else if (type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }

    switch (button)
    {
        case GDK_BUTTON_PRIMARY:
        case GDK_BUTTON_SECONDARY:
            // left or right click
            if (keymod == 0)
            {
                // no modifier
                if (button == GDK_BUTTON_SECONDARY)
                {
                    // right
                    show_app_menu(menu, item, data, button, time_point);
                    return true;
                }
            }
            break;
        case GDK_BUTTON_MIDDLE:
            // middle click
            if (keymod == 0)
            {
                // no modifier
                show_app_menu(menu, item, data, button, time_point);
                return true;
            }
            break;
        default:
            break;
    }
    return false; // true will not stop activate on button-press (will on release)
}

static void
on_popup_open_in_new_tab_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;

    if (!data->selected_files.empty())
    {
        for (const auto& file : data->selected_files)
        {
            if (data->browser && std::filesystem::is_directory(file->path()))
            {
                data->browser->signal_open_file().emit(data->browser,
                                                       file->path(),
                                                       ptk::browser::open_action::new_tab);
            }
        }
    }
    else if (data->browser)
    {
        data->browser->signal_open_file().emit(data->browser,
                                               data->cwd,
                                               ptk::browser::open_action::new_tab);
    }
}

void
on_popup_open_in_new_tab_here(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->browser && std::filesystem::is_directory(data->cwd))
    {
        data->browser->signal_open_file().emit(data->browser,
                                               data->file_path,
                                               ptk::browser::open_action::new_tab);
    }
}

static void
on_new_bookmark(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    ptk::view::bookmark::add(data->browser->cwd());
}

static void
on_popup_cut_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->selected_files.empty())
    {
        return;
    }
    ptk::clipboard::cut_or_copy_files(data->selected_files, false);
}

static void
on_popup_copy_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->selected_files.empty())
    {
        return;
    }
    ptk::clipboard::cut_or_copy_files(data->selected_files, true);
}

static void
on_popup_paste_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->browser)
    {
#if (GTK_MAJOR_VERSION == 4)
        GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(data->browser)));
#elif (GTK_MAJOR_VERSION == 3)
        GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
#endif

        ptk::clipboard::paste_files(GTK_WINDOW(parent),
                                    data->cwd,
                                    GTK_TREE_VIEW(data->browser->task_view()),
                                    nullptr,
                                    nullptr);
    }
}

static void
on_popup_paste_link_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->paste_link();
    }
}

static void
on_popup_paste_target_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->paste_target();
    }
}

static void
on_popup_copy_text_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    ptk::clipboard::copy_as_text(data->selected_files);
}

static void
on_popup_copy_name_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    ptk::clipboard::copy_name(data->selected_files);
}

static void
on_popup_copy_parent_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (!data->cwd.empty())
    {
        ptk::clipboard::copy_text(data->cwd.string());
    }
}

static void
on_popup_paste_as_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    ptk::action::paste_files(data->browser, data->cwd);
}

static void
on_popup_delete_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;

    if (data->selected_files.empty())
    {
        return;
    }

    if (data->browser)
    {
#if (GTK_MAJOR_VERSION == 4)
        GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(data->browser)));
#elif (GTK_MAJOR_VERSION == 3)
        GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
#endif

        ptk::action::delete_files(GTK_WINDOW(parent),
                                  data->cwd,
                                  data->selected_files,
                                  GTK_TREE_VIEW(data->browser->task_view()));
    }
}

static void
on_popup_trash_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;

    if (data->selected_files.empty())
    {
        return;
    }

    if (data->browser)
    {
#if (GTK_MAJOR_VERSION == 4)
        GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(data->browser)));
#elif (GTK_MAJOR_VERSION == 3)
        GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
#endif

        ptk::action::trash_files(GTK_WINDOW(parent),
                                 data->cwd,
                                 data->selected_files,
                                 GTK_TREE_VIEW(data->browser->task_view()));
    }
}

static void
on_popup_rename_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->rename_selected_files(data->selected_files, data->cwd);
    }
}

static void
on_popup_batch_rename_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (data->browser)
    {
        data->browser->batch_rename_selected_files(data->selected_files, data->cwd);
    }
}

static void
on_popup_compress_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    ptk::archiver::create(data->browser, data->selected_files);
}

static void
on_popup_extract_to_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    ptk::archiver::extract(data->browser, data->selected_files, "");
}

static void
on_popup_extract_here_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    ptk::archiver::extract(data->browser, data->selected_files, data->cwd);
}

static void
on_popup_extract_open_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    // If menuitem is set, function was called from GUI so files will contain an archive
    ptk::archiver::open(data->browser, data->selected_files);
}

static void
on_autoopen_create_cb(void* task, AutoOpenCreate* ao) noexcept
{
    (void)task;
    if (!ao)
    {
        return;
    }

    if (GTK_IS_WIDGET(ao->browser) && std::filesystem::exists(ao->path))
    {
        const auto cwd = ao->path.parent_path();

        // select file
        if (std::filesystem::equivalent(cwd, ao->browser->cwd()))
        {
            const auto file = vfs::file::create(ao->path);
            ao->browser->dir_->emit_file_created(file->name(), true);
            ao->browser->select_file(ao->path);
        }

        // open file
        if (ao->open_file)
        {
            if (std::filesystem::is_directory(ao->path))
            {
                ao->browser->chdir(ao->path);
            }
            else
            {
                const auto file = vfs::file::create(ao->path);
                const std::vector<std::shared_ptr<vfs::file>> selected_files{file};
                ptk::action::open_files_with_app(cwd, selected_files, "", ao->browser, false, true);
            }
        }
    }

    delete ao;
}

static void
create_new_file(ptk::file_menu* data, ptk::action::create_mode mode) noexcept
{
    if (data->cwd.empty())
    {
        return;
    }

    auto* const ao = new AutoOpenCreate(data->browser, false);

    std::shared_ptr<vfs::file> file = nullptr;
    if (!data->selected_files.empty())
    {
        file = data->selected_files.front();
    }

    ptk::action::create_files(data->browser, data->cwd, file, mode, ao);
}

static void
on_popup_new_folder_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    create_new_file(data, ptk::action::create_mode::dir);
}

static void
on_popup_new_text_file_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    create_new_file(data, ptk::action::create_mode::file);
}

static void
on_popup_new_link_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    create_new_file(data, ptk::action::create_mode::link);
}

static void
on_popup_file_properties_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    GtkWidget* parent = nullptr;
    if (data->browser)
    {
#if (GTK_MAJOR_VERSION == 4)
        parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(data->browser)));
#elif (GTK_MAJOR_VERSION == 3)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
#endif
    }
    ptk_show_file_properties(GTK_WINDOW(parent), data->cwd, data->selected_files, 0);
}

static void
on_popup_file_attributes_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    GtkWidget* parent = nullptr;
    if (data->browser)
    {
#if (GTK_MAJOR_VERSION == 4)
        parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(data->browser)));
#elif (GTK_MAJOR_VERSION == 3)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
#endif
    }
    ptk_show_file_properties(GTK_WINDOW(parent), data->cwd, data->selected_files, 1);
}

static void
on_popup_file_permissions_activate(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    GtkWidget* parent = nullptr;
    if (data->browser)
    {
#if (GTK_MAJOR_VERSION == 4)
        parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(data->browser)));
#elif (GTK_MAJOR_VERSION == 3)
        parent = gtk_widget_get_toplevel(GTK_WIDGET(data->browser));
#endif
    }
    ptk_show_file_properties(GTK_WINDOW(parent), data->cwd, data->selected_files, 2);
}

static void
on_popup_canon(GtkMenuItem* menuitem, ptk::file_menu* data) noexcept
{
    (void)menuitem;
    if (!data->browser)
    {
        return;
    }

    data->browser->canon(!data->file_path.empty() ? data->file_path : data->cwd);
}

void
ptk_file_menu_action(ptk::browser* browser, const xset_t& set) noexcept
{
    assert(set != nullptr);
    assert(browser != nullptr);
    // logger::debug<logger::domain::ptk>("ptk_file_menu_action()={}", set->name());

    // setup data
    const auto& cwd = browser->cwd();
    const auto selected_files = browser->selected_files();

    std::shared_ptr<vfs::file> file = nullptr;
    std::filesystem::path file_path;
    if (!selected_files.empty())
    {
        file = selected_files.front();
        file_path = file->path();
    }

    auto* const data = new ptk::file_menu;
    data->cwd = cwd;
    data->browser = browser;
    data->selected_files = selected_files;
    data->file_path = file_path;
    if (file)
    {
        data->file = file;
    }

    // action
    if (set->name().starts_with("open_") && !set->name().starts_with("open_in_"))
    {
        if (set->xset_name == xset::name::open_edit)
        {
            vfs::utils::open_editor(data->file_path);
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
    else if (set->name().starts_with("arc_"))
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
    else if (set->name().starts_with("new_"))
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
            ptk::view::bookmark::add(browser->cwd());
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
    else if (set->name().starts_with("edit_"))
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
        else if (set->xset_name == xset::name::edit_batch_rename)
        {
            on_popup_batch_rename_activate(nullptr, data);
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
    else if (set->name().starts_with("copy_loc") || set->name().starts_with("copy_tab_") ||
             set->name().starts_with("copy_panel_") || set->name().starts_with("move_loc") ||
             set->name().starts_with("move_tab_") || set->name().starts_with("move_panel_"))
    {
        on_copycmd(nullptr, data, set);
    }
    if (set->name().starts_with("open_in_panel"))
    {
        panel_t i = INVALID_PANEL;
        if (set->xset_name == xset::name::open_in_panel_prev)
        {
            i = panel_control_code_prev;
        }
        else if (set->xset_name == xset::name::open_in_panel_next)
        {
            i = panel_control_code_next;
        }
        else
        {
            const auto panel = ztd::removeprefix(set->name(), "open_in_panel_");
            i = panel_t::create(panel).value_or(INVALID_PANEL);
        }
        data->browser->open_in_panel(i, data->file_path);
    }
    else if (set->name().starts_with("opentab_"))
    {
        tab_t i = INVALID_TAB;
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
                const auto tab = ztd::removeprefix(set->name(), "opentab_");
                i = tab_t::create(tab).value_or(INVALID_TAB);
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
        browser->new_tab_here();
    }

    ptk_file_menu_free(data);
}
