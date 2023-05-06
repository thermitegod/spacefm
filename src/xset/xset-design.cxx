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

#include <vector>

#include <cassert>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-context.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-design.hxx"
#include "xset/xset-design-clipboard.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-plugins.hxx"
#include "xset/xset-static-strings.hxx"

#include "item-prop.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "ptk/ptk-error.hxx"
#include "ptk/ptk-keyboard.hxx"

#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-location-view.hxx"

#include "types.hxx"

#include "settings/app.hxx"
#include "settings/disk-format.hxx"

#include "autosave.hxx"
#include "write.hxx"
#include "utils.hxx"

#include "settings.hxx"

#include "main-window.hxx"

static void
xset_design_job_set_key(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    xset_set_key(parent, set);
}

static void
xset_design_job_set_icon(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    xset_t mset = xset_get_plugin_mirror(set);
    char* old_icon;
    old_icon = ztd::strdup(mset->icon);
    // Note: xset_text_dialog uses the title passed to know this is an
    // icon chooser, so it adds a Choose button.  If you change the title,
    // change xset_text_dialog.
    xset_text_dialog(parent, "Set Icon", icon_desc, "", mset->icon, &mset->icon, "", false);
    if (set->lock && !ztd::same(old_icon, mset->icon))
    {
        // built-in icon has been changed from default, save it
        set->keep_terminal = true;
    }
    std::free(old_icon);
}

static void
xset_design_job_set_edit(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const auto cmd_type = XSetCMD(xset_get_int(set, XSetVar::X));
    if (cmd_type == XSetCMD::SCRIPT)
    {
        // script
        char* cscript = xset_custom_get_script(set, !set->plugin);
        if (!cscript)
        {
            return;
        }
        xset_edit(parent, cscript, false, true);
        std::free(cscript);
    }
}

static void
xset_design_job_set_edit_root(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const auto cmd_type = XSetCMD(xset_get_int(set, XSetVar::X));
    if (cmd_type == XSetCMD::SCRIPT)
    {
        // script
        char* cscript = xset_custom_get_script(set, !set->plugin);
        if (!cscript)
        {
            return;
        }
        xset_edit(parent, cscript, true, false);
        std::free(cscript);
    }
}

static void
xset_design_job_set_copyname(xset_t set)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    const auto cmd_type = XSetCMD(xset_get_int(set, XSetVar::X));
    if (cmd_type == XSetCMD::LINE)
    {
        // line
        gtk_clipboard_set_text(clip, set->line, -1);
    }
    else if (cmd_type == XSetCMD::SCRIPT)
    {
        // script
        char* cscript = xset_custom_get_script(set, true);
        if (!cscript)
        {
            return;
        }
        gtk_clipboard_set_text(clip, cscript, -1);
        std::free(cscript);
    }
    else if (cmd_type == XSetCMD::APP)
    {
        // custom
        gtk_clipboard_set_text(clip, set->z, -1);
    }
}

static void
xset_design_job_set_line(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const bool response = xset_text_dialog(parent,
                                           "Edit Command Line",
                                           enter_command_line,
                                           "",
                                           set->line,
                                           &set->line,
                                           "",
                                           false);
    if (response)
    {
        xset_set_var(set, XSetVar::X, "0");
    }
}

static void
xset_design_job_set_script(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    xset_set_var(set, XSetVar::X, "1");
    char* cscript = xset_custom_get_script(set, true);
    if (!cscript)
    {
        return;
    }
    xset_edit(parent, cscript, false, false);
    std::free(cscript);
}

static void
xset_design_job_set_custom(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    std::string file;
    std::string folder;

    if (set->z)
    {
        folder = std::filesystem::path(set->z).parent_path();
        file = std::filesystem::path(set->z).filename();
    }
    else
    {
        folder = "/usr/bin";
    }

    char* custom_file = xset_file_dialog(parent,
                                         GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "Choose Custom Executable",
                                         folder.data(),
                                         file.data());

    if (custom_file)
    {
        xset_set_var(set, XSetVar::X, "2");
        xset_set_var(set, XSetVar::Z, custom_file);
        std::free(custom_file);
    }
}

static void
xset_design_job_set_user(xset_t set)
{
    if (set->plugin)
    {
        return;
    }

    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    xset_text_dialog(parent,
                     "Run As User",
                     "Run this command as username:\n\n( Leave blank for current user )",
                     "",
                     set->y,
                     &set->y,
                     "",
                     false);
}

static void
xset_design_job_set_app(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    if (ztd::startswith(set->name, "open_all_type_"))
    {
        const std::string name = ztd::removeprefix(set->name, "open_all_type_");
        const std::string msg =
            fmt::format("You are adding a custom command to the Default menu item.  This item will "
                        "automatically have a pre-context - it will only appear when the MIME type "
                        "of the first selected file matches the current type '{}'.\n\nAdd commands "
                        "or menus here which you only want to appear for this one MIME type.",
                        name.empty() ? "(none)" : name);

        const i32 response = xset_msg_dialog(parent,
                                             GtkMessageType::GTK_MESSAGE_INFO,
                                             "New Context Command",
                                             GtkButtonsType::GTK_BUTTONS_OK_CANCEL,
                                             msg);

        if (response != GtkResponseType::GTK_RESPONSE_OK)
        {
            return;
        }
    }

    vfs::mime_type mime_type = vfs_mime_type_get_from_type(
        xset_context && !xset_context->var[ItemPropContext::CONTEXT_MIME].empty()
            ? xset_context->var[ItemPropContext::CONTEXT_MIME].data()
            : XDG_MIME_TYPE_UNKNOWN);
    char* file =
        ptk_choose_app_for_mime_type(GTK_WINDOW(parent), mime_type, true, false, false, false);

    if (!(file && file[0]))
    {
        std::free(file);
    }

    // add new menu item
    xset_t newset = xset_custom_new();
    xset_custom_insert_after(set, newset);

    newset->z = file;
    newset->menu_label = set->name;
    newset->browser = set->browser;
    if (newset->x)
    {
        std::free(newset->x);
    }
    newset->x = ztd::strdup("2"); // XSetCMD::APP
    // unset these to save session space
    newset->task = false;
    newset->task_err = false;
    newset->task_out = false;
    newset->keep_terminal = false;
}

static void
xset_design_job_set_command(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    if (ztd::startswith(set->name, "open_all_type_"))
    {
        const std::string name = ztd::removeprefix(set->name, "open_all_type_");
        const std::string msg =
            fmt::format("You are adding a custom command to the Default menu item.  This item will "
                        "automatically have a pre-context - it will only appear when the MIME type "
                        "of the first selected file matches the current type '{}'.\n\nAdd commands "
                        "or menus here which you only want to appear for this one MIME type.",
                        name.empty() ? "(none)" : name);

        const i32 response = xset_msg_dialog(parent,
                                             GtkMessageType::GTK_MESSAGE_INFO,
                                             "New Context Command",
                                             GtkButtonsType::GTK_BUTTONS_OK_CANCEL,
                                             msg);

        if (response != GtkResponseType::GTK_RESPONSE_OK)
        {
            return;
        }
    }

    char* name = ztd::strdup("New _Command");
    const bool response =
        xset_text_dialog(parent, "Set Item Name", enter_menu_name_new, "", name, &name, "", false);
    if (!response)
    {
        std::free(name);
    }

    // add new menu item
    xset_t newset = xset_custom_new();
    xset_custom_insert_after(set, newset);

    newset->menu_label = name;
    newset->browser = set->browser;

    xset_item_prop_dlg(xset_context, newset, 2);
}

static void
xset_design_job_set_submenu(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    if (ztd::startswith(set->name, "open_all_type_"))
    {
        const std::string name = ztd::removeprefix(set->name, "open_all_type_");
        const std::string msg =
            fmt::format("You are adding a custom submenu to the Default menu item.  This item will "
                        "automatically have a pre-context - it will only appear when the MIME type "
                        "of the first selected file matches the current type '{}'.\n\nAdd commands "
                        "or menus here which you only want to appear for this one MIME type.",
                        name.empty() ? "(none)" : name);

        const i32 response = xset_msg_dialog(parent,
                                             GtkMessageType::GTK_MESSAGE_INFO,
                                             "New Context Submenu",
                                             GtkButtonsType::GTK_BUTTONS_OK_CANCEL,
                                             msg);

        if (response != GtkResponseType::GTK_RESPONSE_OK)
        {
            return;
        }
    }

    char* name;
    const bool response =
        xset_text_dialog(parent,
                         "Set Submenu Name",
                         "Enter submenu name:\n\nPrecede a character with an underscore (_) "
                         "to underline that character as a shortcut key if desired.",
                         "",
                         "New _Submenu",
                         &name,
                         "",
                         false);
    if (!response || !name)
    {
        return;
    }

    // add new submenu
    xset_t newset = xset_custom_new();
    newset->menu_label = name;
    newset->menu_style = XSetMenu::SUBMENU;
    xset_custom_insert_after(set, newset);

    // add submenu child
    xset_t childset = xset_custom_new();
    newset->child = ztd::strdup(childset->name);
    childset->parent = ztd::strdup(newset->name);
    childset->menu_label = ztd::strdup("New _Command");

    std::free(name);
}

static void
xset_design_job_set_sep(xset_t set)
{
    xset_t newset = xset_custom_new();
    newset->menu_style = XSetMenu::SEP;
    xset_custom_insert_after(set, newset);
}

static void
xset_design_job_set_add_tool(xset_t set, XSetTool tool_type)
{
    if (tool_type < XSetTool::DEVICES || tool_type >= XSetTool::INVALID ||
        set->tool == XSetTool::NOT)
    {
        return;
    }
    xset_t newset = xset_new_builtin_toolitem(tool_type);
    if (newset)
    {
        xset_custom_insert_after(set, newset);
    }
}

static void
xset_design_job_set_import_file(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const char* folder;

    xset_t save = xset_get(XSetName::PLUG_IFILE);
    if (save->s) //&& std::filesystem::is_directory(save->s)
    {
        folder = save->s;
    }
    else
    {
        if (!(folder = xset_get_s(XSetName::GO_SET_DEFAULT)))
        {
            folder = ztd::strdup("/");
        }
    }
    char* file = xset_file_dialog(GTK_WIDGET(parent),
                                  GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN,
                                  "Choose Plugin File",
                                  folder,
                                  nullptr);
    if (!file)
    {
        return;
    }

    save->s = ztd::strdup(std::filesystem::path(file).parent_path());

    // Make Plugin Dir
    const auto user_tmp = vfs::user_dirs->program_tmp_dir();
    if (!std::filesystem::is_directory(user_tmp))
    {
        xset_msg_dialog(GTK_WIDGET(parent),
                        GtkMessageType::GTK_MESSAGE_ERROR,
                        "Error Creating Temp Directory",
                        GtkButtonsType::GTK_BUTTONS_OK,
                        "Unable to create temporary directory");
        std::free(file);
        return;
    }

    std::filesystem::path plug_dir;
    while (std::filesystem::exists(plug_dir))
    {
        plug_dir = user_tmp / ztd::randhex();
        if (!std::filesystem::exists(plug_dir))
        {
            break;
        }
    }
    install_plugin_file(set->browser ? set->browser->main_window : nullptr,
                        nullptr,
                        file,
                        plug_dir.string(),
                        PluginJob::COPY,
                        set);
    std::free(file);
}

static void
xset_design_job_set_cut(xset_t set)
{
    xset_set_clipboard = set;
    xset_clipboard_is_cut = true;
}

static void
xset_design_job_set_copy(xset_t set)
{
    xset_set_clipboard = set;
    xset_clipboard_is_cut = false;
}

static bool
xset_design_job_set_paste(xset_t set)
{
    if (!xset_set_clipboard)
    {
        return false;
    }
    if (xset_set_clipboard->tool > XSetTool::CUSTOM && set->tool == XSetTool::NOT)
    {
        // failsafe - disallow pasting a builtin tool to a menu
        return false;
    }

    bool update_toolbars = false;
    if (xset_clipboard_is_cut)
    {
        update_toolbars = !(xset_set_clipboard->tool == XSetTool::NOT);
        if (!update_toolbars && xset_set_clipboard->parent)
        {
            xset_t newset = xset_get(xset_set_clipboard->parent);
            if (!(newset->tool == XSetTool::NOT))
            {
                // we are cutting the first item in a tool submenu
                update_toolbars = true;
            }
        }
        xset_custom_remove(xset_set_clipboard);
        xset_custom_insert_after(set, xset_set_clipboard);

        xset_set_clipboard = nullptr;
    }
    else
    {
        xset_t newset = xset_custom_copy(xset_set_clipboard, false, false);
        xset_custom_insert_after(set, newset);
    }

    return update_toolbars;
}

static void
xset_remove_plugin(GtkWidget* parent, PtkFileBrowser* file_browser, xset_t set)
{
    if (!file_browser || !set || !set->plugin_top || set->plug_dir.empty())
    {
        return;
    }

    if (app_settings.get_confirm())
    {
        const std::string label = clean_label(set->menu_label, false, false);
        const std::string msg =
            fmt::format("Uninstall the '{}' plugin?\n\n( {} )", label, set->plug_dir);

        const i32 response = xset_msg_dialog(parent,
                                             GtkMessageType::GTK_MESSAGE_WARNING,
                                             "Uninstall Plugin",
                                             GtkButtonsType::GTK_BUTTONS_YES_NO,
                                             msg);

        if (response != GtkResponseType::GTK_RESPONSE_YES)
        {
            return;
        }
    }
    PtkFileTask* ptask = ptk_file_exec_new("Uninstall Plugin", parent, file_browser->task_view);

    const std::string plug_dir_q = ztd::shell::quote(set->plug_dir.string());

    ptask->task->exec_command = fmt::format("rm -rf {}", plug_dir_q);
    ptask->task->exec_sync = true;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_export = false;
    ptask->task->exec_as_user = "root";

    const auto plugin_data = new PluginData;
    plugin_data->plug_dir = ztd::strdup(set->plug_dir);
    plugin_data->set = set;
    plugin_data->job = PluginJob::REMOVE;
    ptask->complete_notify = (GFunc)on_install_plugin_cb;
    ptask->user_data = plugin_data;

    ptk_file_task_run(ptask);
}

static bool
xset_design_job_set_remove(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    GtkButtonsType buttons;
    GtkWidget* dlgparent = nullptr;
    GtkWidget* dlg;

    char* name;
    char* prog;

    if (set->plugin)
    {
        xset_remove_plugin(parent, set->browser, set);
        return false;
    }

    const auto cmd_type = XSetCMD(xset_get_int(set, XSetVar::X));

    if (set->menu_label)
    {
        name = ztd::strdup(clean_label(set->menu_label, false, false));
    }
    else
    {
        if (!set->lock && set->z && set->menu_style < XSetMenu::SUBMENU && cmd_type == XSetCMD::APP)
        {
            name = ztd::strdup(set->z);
        }
        else
        {
            name = ztd::strdup("( no name )");
        }
    }

    std::string msg;
    if (set->child && set->menu_style == XSetMenu::SUBMENU)
    {
        msg = fmt::format(
            "Permanently remove the '{}' SUBMENU AND ALL ITEMS WITHIN IT?\n\nThis action "
            "will delete all settings and files associated with these items.",
            name);
        buttons = GtkButtonsType::GTK_BUTTONS_YES_NO;
    }
    else
    {
        msg = fmt::format("Permanently remove the '{}' item?\n\nThis action will delete "
                          "all settings and files associated with this item.",
                          name);
        buttons = GtkButtonsType::GTK_BUTTONS_OK_CANCEL;
    }
    std::free(name);
    const bool is_app = !set->lock && set->menu_style < XSetMenu::SUBMENU &&
                        cmd_type == XSetCMD::APP && set->tool <= XSetTool::CUSTOM;
    if (!(set->menu_style == XSetMenu::SEP) && app_settings.get_confirm() && !is_app &&
        set->tool <= XSetTool::CUSTOM)
    {
        if (parent)
        {
            dlgparent = gtk_widget_get_toplevel(parent);
        }
        dlg = gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                                     GtkDialogFlags::GTK_DIALOG_MODAL,
                                     GtkMessageType::GTK_MESSAGE_WARNING,
                                     buttons,
                                     msg.data(),
                                     nullptr);
        xset_set_window_icon(GTK_WINDOW(dlg));
        gtk_window_set_title(GTK_WINDOW(dlg), "Confirm Remove");
        gtk_widget_show_all(dlg);
        const i32 response = gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        if (response != GtkResponseType::GTK_RESPONSE_OK &&
            response != GtkResponseType::GTK_RESPONSE_YES)
        {
            return false;
        }
    }

    bool update_toolbars;

    // remove
    name = ztd::strdup(set->name);
    prog = ztd::strdup(set->parent);

    xset_t set_next;

    if (set->parent && (set_next = xset_is(set->parent)) && set_next->tool == XSetTool::CUSTOM &&
        set_next->menu_style == XSetMenu::SUBMENU)
    {
        // this set is first item in custom toolbar submenu
        update_toolbars = true;
    }

    xset_custom_remove(set);

    if (!(set->tool == XSetTool::NOT))
    {
        update_toolbars = true;
        std::free(name);
        std::free(prog);
        name = prog = nullptr;
    }
    else
    {
        std::free(prog);
        prog = nullptr;
    }

    xset_custom_delete(set, false);
    set = nullptr;

    if (prog)
    {
        std::free(prog);
    }
    if (name)
    {
        std::free(name);
    }

    return update_toolbars;
}

static void
xset_design_job_set_export(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    if ((!set->lock || set->xset_name == XSetName::MAIN_BOOK) && set->tool <= XSetTool::CUSTOM)
    {
        xset_custom_export(parent, set->browser, set);
    }
}

static void
xset_design_job_set_normal(xset_t set)
{
    set->menu_style = XSetMenu::NORMAL;
}

static void
xset_design_job_set_check(xset_t set)
{
    set->menu_style = XSetMenu::CHECK;
}

static void
xset_design_job_set_confirm(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    if (!set->desc)
    {
        set->desc = ztd::strdup("Are you sure?");
    }

    const bool response = xset_text_dialog(parent,
                                           "Dialog Message",
                                           "Enter the message to be displayed in this "
                                           "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                                           "",
                                           set->desc,
                                           &set->desc,
                                           "",
                                           false);
    if (response)
    {
        set->menu_style = XSetMenu::CONFIRM;
    }
}

static void
xset_design_job_set_dialog(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const bool response = xset_text_dialog(parent,
                                           "Dialog Message",
                                           "Enter the message to be displayed in this "
                                           "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                                           "",
                                           set->desc,
                                           &set->desc,
                                           "",
                                           false);
    if (response)
    {
        set->menu_style = XSetMenu::STRING;
    }
}

static void
xset_design_job_set_message(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    xset_text_dialog(parent,
                     "Dialog Message",
                     "Enter the message to be displayed in this "
                     "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                     "",
                     set->desc,
                     &set->desc,
                     "",
                     false);
}

static void
xset_design_job_set_prop(xset_t set)
{
    xset_item_prop_dlg(xset_context, set, 0);
}

static void
xset_design_job_set_prop_cmd(xset_t set)
{
    xset_item_prop_dlg(xset_context, set, 2);
}

static void
xset_design_job_set_ignore_context(xset_t set)
{
    (void)set;
    xset_set_b(XSetName::CONTEXT_DLG, !xset_get_b(XSetName::CONTEXT_DLG));
}

static void
xset_design_job_set_browse_files(xset_t set)
{
    if (set->tool > XSetTool::CUSTOM)
    {
        return;
    }

    std::filesystem::path folder;
    if (set->plugin)
    {
        folder = std::filesystem::path() / set->plug_dir / "files";
        if (!std::filesystem::exists(folder))
        {
            folder = std::filesystem::path() / set->plug_dir / set->plug_name;
        }
    }
    else
    {
        folder = vfs::user_dirs->program_config_dir() / "scripts" / set->name;
    }
    if (!std::filesystem::exists(folder) && !set->plugin)
    {
        std::filesystem::create_directories(folder);
        std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
    }

    if (set->browser)
    {
        set->browser->run_event<EventType::OPEN_ITEM>(folder, PtkOpenAction::PTK_OPEN_DIR);
    }
}

static void
xset_design_job_set_browse_data(xset_t set)
{
    if (set->tool > XSetTool::CUSTOM)
    {
        return;
    }

    std::filesystem::path folder;
    if (set->plugin)
    {
        xset_t mset = xset_get_plugin_mirror(set);
        folder = vfs::user_dirs->program_config_dir() / "plugin-data" / mset->name;
    }
    else
    {
        folder = vfs::user_dirs->program_config_dir() / "plugin-data" / set->name;
    }
    if (!std::filesystem::exists(folder))
    {
        std::filesystem::create_directories(folder);
        std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
    }

    if (set->browser)
    {
        set->browser->run_event<EventType::OPEN_ITEM>(folder, PtkOpenAction::PTK_OPEN_DIR);
    }
}

static void
xset_design_job_set_browse_plugins(xset_t set)
{
    if (set->plugin && !set->plug_dir.empty())
    {
        if (set->browser)
        {
            set->browser->run_event<EventType::OPEN_ITEM>(set->plug_dir,
                                                          PtkOpenAction::PTK_OPEN_DIR);
        }
    }
}

static void
xset_design_job_set_term(xset_t set)
{
    xset_t mset = xset_get_plugin_mirror(set);
    if (mset->in_terminal)
    {
        mset->in_terminal = false;
    }
    else
    {
        mset->in_terminal = true;
        mset->task = false;
    }
}

static void
xset_design_job_set_keep(xset_t set)
{
    xset_t mset = xset_get_plugin_mirror(set);
    if (mset->keep_terminal)
    {
        mset->keep_terminal = false;
    }
    else
    {
        mset->keep_terminal = true;
    }
}

static void
xset_design_job_set_task(xset_t set)
{
    xset_t mset = xset_get_plugin_mirror(set);
    if (mset->task)
    {
        mset->task = false;
    }
    else
    {
        mset->task = true;
    }
}

static void
xset_design_job_set_pop(xset_t set)
{
    xset_t mset = xset_get_plugin_mirror(set);
    if (mset->task_pop)
    {
        mset->task_pop = false;
    }
    else
    {
        mset->task_pop = true;
    }
}

static void
xset_design_job_set_err(xset_t set)
{
    xset_t mset = xset_get_plugin_mirror(set);
    if (mset->task_err)
    {
        mset->task_err = false;
    }
    else
    {
        mset->task_err = true;
    }
}

static void
xset_design_job_set_out(xset_t set)
{
    xset_t mset = xset_get_plugin_mirror(set);
    if (mset->task_out)
    {
        mset->task_out = false;
    }
    else
    {
        mset->task_out = true;
    }
}

static void
xset_design_job_set_scroll(xset_t set)
{
    xset_t mset = xset_get_plugin_mirror(set);
    if (mset->scroll_lock)
    {
        mset->scroll_lock = false;
    }
    else
    {
        mset->scroll_lock = true;
    }
}

void
xset_design_job(GtkWidget* item, xset_t set)
{
    assert(set != nullptr);

    bool update_toolbars = false;

    XSetTool tool_type;
    const auto job = XSetJob(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job")));

    // ztd::logger::info("activate job {} {}", job, set->name);
    switch (job)
    {
        case XSetJob::KEY:
            xset_design_job_set_key(set);
            break;
        case XSetJob::ICON:
            xset_design_job_set_icon(set);
            break;
        case XSetJob::EDIT:
            xset_design_job_set_edit(set);
            break;
        case XSetJob::EDIT_ROOT:
            xset_design_job_set_edit_root(set);
            break;
        case XSetJob::COPYNAME:
            xset_design_job_set_copyname(set);
            break;
        case XSetJob::LINE:
            xset_design_job_set_line(set);
            break;
        case XSetJob::SCRIPT:
            xset_design_job_set_script(set);
            break;
        case XSetJob::CUSTOM:
            xset_design_job_set_custom(set);
            break;
        case XSetJob::USER:
            xset_design_job_set_user(set);
            break;
        case XSetJob::BOOKMARK:
            break;
        case XSetJob::APP:
            xset_design_job_set_app(set);
            break;
        case XSetJob::COMMAND:
            xset_design_job_set_command(set);
            break;
        case XSetJob::SUBMENU:
        case XSetJob::SUBMENU_BOOK:
            xset_design_job_set_submenu(set);
            break;
        case XSetJob::SEP:
            xset_design_job_set_sep(set);
            break;
        case XSetJob::ADD_TOOL:
            tool_type = XSetTool(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tool_type")));
            xset_design_job_set_add_tool(set, tool_type);
            break;
        case XSetJob::IMPORT_FILE:
            xset_design_job_set_import_file(set);
            break;
        case XSetJob::CUT:
            xset_design_job_set_cut(set);
            break;
        case XSetJob::COPY:
            xset_design_job_set_copy(set);
            break;
        case XSetJob::PASTE:
            update_toolbars = xset_design_job_set_paste(set);
            break;
        case XSetJob::REMOVE:
        case XSetJob::REMOVE_BOOK:
            update_toolbars = xset_design_job_set_remove(set);
            break;
        case XSetJob::EXPORT:
            xset_design_job_set_export(set);
            break;
        case XSetJob::NORMAL:
            xset_design_job_set_normal(set);
            break;
        case XSetJob::CHECK:
            xset_design_job_set_check(set);
            break;
        case XSetJob::CONFIRM:
            xset_design_job_set_confirm(set);
            break;
        case XSetJob::DIALOG:
            xset_design_job_set_dialog(set);
            break;
        case XSetJob::MESSAGE:
            xset_design_job_set_message(set);
            break;
        case XSetJob::PROP:
            xset_design_job_set_prop(set);
            break;
        case XSetJob::PROP_CMD:
            xset_design_job_set_prop_cmd(set);
            break;
        case XSetJob::IGNORE_CONTEXT:
            xset_design_job_set_ignore_context(set);
            break;
        case XSetJob::BROWSE_FILES:
            xset_design_job_set_browse_files(set);
            break;
        case XSetJob::BROWSE_DATA:
            xset_design_job_set_browse_data(set);
            break;
        case XSetJob::BROWSE_PLUGIN:
            xset_design_job_set_browse_plugins(set);
            break;
        case XSetJob::TERM:
            xset_design_job_set_term(set);
            break;
        case XSetJob::KEEP:
            xset_design_job_set_keep(set);
            break;
        case XSetJob::TASK:
            xset_design_job_set_task(set);
            break;
        case XSetJob::POP:
            xset_design_job_set_pop(set);
            break;
        case XSetJob::ERR:
            xset_design_job_set_err(set);
            break;
        case XSetJob::OUT:
            xset_design_job_set_out(set);
            break;
        case XSetJob::SCROLL:
            xset_design_job_set_scroll(set);
            break;
        case XSetJob::TOOLTIPS:
        case XSetJob::LABEL:
        case XSetJob::HELP:
        case XSetJob::HELP_NEW:
        case XSetJob::HELP_ADD:
        case XSetJob::HELP_BROWSE:
        case XSetJob::HELP_STYLE:
        case XSetJob::HELP_BOOK:
        case XSetJob::INVALID:
        default:
            break;
    }

    xset_t set_next;
    if (set && (!set->lock || set->xset_name == XSetName::MAIN_BOOK))
    {
        if (set->parent && (set_next = xset_is(set->parent)) &&
            set_next->tool == XSetTool::CUSTOM && set_next->menu_style == XSetMenu::SUBMENU)
        {
            // this set is first item in custom toolbar submenu
            update_toolbars = true;
        }
    }

    if ((set && !set->lock && !(set->tool == XSetTool::NOT)) || update_toolbars)
    {
        main_window_rebuild_all_toolbars(set ? set->browser : nullptr);
    }

    // autosave
    autosave_request_add();
}
