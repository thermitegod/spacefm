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

#include <format>

#include <cassert>

#include <gtk/gtk.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-context.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-design.hxx"
#include "xset/xset-design-clipboard.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-static-strings.hxx"

#include "item-prop.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "ptk/ptk-dialog.hxx"
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

    const std::string old_icon = set->icon.value_or("");
    // Note: xset_text_dialog uses the title passed to know this is an
    // icon chooser, so it adds a Choose button.  If you change the title,
    // change xset_text_dialog.
    const auto [response, answer] =
        xset_text_dialog(parent, "Set Icon", icon_desc, "", set->icon.value_or(""), "", false);

    set->icon = answer;
    if (set->lock && !ztd::same(old_icon, set->icon.value()))
    {
        // built-in icon has been changed from default, save it
        set->keep_terminal = true;
    }
}

static void
xset_design_job_set_edit(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const auto cmd_type = xset::cmd(xset_get_int(set, xset::var::x));
    if (cmd_type == xset::cmd::script)
    {
        // script
        char* cscript = xset_custom_get_script(set, true);
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

    const auto cmd_type = xset::cmd(xset_get_int(set, xset::var::x));
    if (cmd_type == xset::cmd::script)
    {
        // script
        char* cscript = xset_custom_get_script(set, true);
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

    const auto cmd_type = xset::cmd(xset_get_int(set, xset::var::x));
    if (cmd_type == xset::cmd::line)
    {
        // line
        gtk_clipboard_set_text(clip, set->line.value().data(), -1);
    }
    else if (cmd_type == xset::cmd::script)
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
    else if (cmd_type == xset::cmd::app)
    {
        // custom
        if (set->z)
        {
            gtk_clipboard_set_text(clip, set->z.value().c_str(), -1);
        }
        else
        {
            gtk_clipboard_set_text(clip, "", -1);
        }
    }
}

static void
xset_design_job_set_line(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const auto [response, answer] = xset_text_dialog(parent,
                                                     "Edit Command Line",
                                                     enter_command_line,
                                                     "",
                                                     set->line.value(),
                                                     "",
                                                     false);
    set->line = answer;
    if (response)
    {
        xset_set_var(set, xset::var::x, "0");
    }
}

static void
xset_design_job_set_script(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    xset_set_var(set, xset::var::x, "1");
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
        const auto z = set->z.value();
        folder = std::filesystem::path(z).parent_path();
        file = std::filesystem::path(z).filename();
    }
    else
    {
        folder = "/usr/bin";
    }

    const auto custom_file = xset_file_dialog(parent,
                                              GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN,
                                              "Choose Custom Executable",
                                              folder,
                                              file);

    if (custom_file)
    {
        xset_set_var(set, xset::var::x, "2");
        xset_set_var(set, xset::var::z, custom_file.value().string());
    }
}

static void
xset_design_job_set_user(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const auto [response, answer] =
        xset_text_dialog(parent,
                         "Run As User",
                         "Run this command as username:\n\n( Leave blank for current user )",
                         "",
                         set->y.value(),
                         "",
                         false);
    set->y = answer;
}

static void
xset_design_job_set_app(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    if (ztd::startswith(set->name, "open_all_type_"))
    {
        const std::string name = ztd::removeprefix(set->name, "open_all_type_");

        const auto response = ptk_show_message(
            GTK_WINDOW(parent),
            GtkMessageType::GTK_MESSAGE_INFO,
            "New Context Command",
            GtkButtonsType::GTK_BUTTONS_OK_CANCEL,
            std::format("You are adding a custom command to the Default menu item.  This item will "
                        "automatically have a pre-context - it will only appear when the MIME type "
                        "of the first selected file matches the current type '{}'.\n\nAdd commands "
                        "or menus here which you only want to appear for this one MIME type.",
                        name.empty() ? "(none)" : name));

        if (response != GtkResponseType::GTK_RESPONSE_OK)
        {
            return;
        }
    }

    vfs::mime_type mime_type = vfs_mime_type_get_from_type(
        xset_context && !xset_context->var[item_prop::context::item::mime].empty()
            ? xset_context->var[item_prop::context::item::mime].data()
            : XDG_MIME_TYPE_UNKNOWN);
    const auto app =
        ptk_choose_app_for_mime_type(GTK_WINDOW(parent), mime_type, true, false, false, false);

    // add new menu item
    xset_t newset = xset_custom_new();
    xset_custom_insert_after(set, newset);

    newset->z = app;
    newset->menu_label = set->name;
    newset->browser = set->browser;
    newset->x = std::to_string(magic_enum::enum_integer(xset::cmd::app));
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

        const auto response = ptk_show_message(
            GTK_WINDOW(parent),
            GtkMessageType::GTK_MESSAGE_INFO,
            "New Context Command",
            GtkButtonsType::GTK_BUTTONS_OK_CANCEL,
            std::format("You are adding a custom command to the Default menu item.  This item will "
                        "automatically have a pre-context - it will only appear when the MIME type "
                        "of the first selected file matches the current type '{}'.\n\nAdd commands "
                        "or menus here which you only want to appear for this one MIME type.",
                        name.empty() ? "(none)" : name));

        if (response != GtkResponseType::GTK_RESPONSE_OK)
        {
            return;
        }
    }

    const auto [response, answer] = xset_text_dialog(parent,
                                                     "Set Item Name",
                                                     enter_menu_name_new,
                                                     "",
                                                     "New _Command",
                                                     "",
                                                     false);

    const std::string name = answer;

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

        const auto response = ptk_show_message(
            GTK_WINDOW(parent),
            GtkMessageType::GTK_MESSAGE_INFO,
            "New Context Submenu",
            GtkButtonsType::GTK_BUTTONS_OK_CANCEL,
            std::format("You are adding a custom submenu to the Default menu item.  This item will "
                        "automatically have a pre-context - it will only appear when the MIME type "
                        "of the first selected file matches the current type '{}'.\n\nAdd commands "
                        "or menus here which you only want to appear for this one MIME type.",
                        name.empty() ? "(none)" : name));

        if (response != GtkResponseType::GTK_RESPONSE_OK)
        {
            return;
        }
    }

    const auto [response, answer] =
        xset_text_dialog(parent,
                         "Set Submenu Name",
                         "Enter submenu name:\n\nPrecede a character with an underscore (_) "
                         "to underline that character as a shortcut key if desired.",
                         "",
                         "New _Submenu",
                         "",
                         false);
    const std::string name = answer;
    if (!response || name.empty())
    {
        return;
    }

    // add new submenu
    xset_t newset = xset_custom_new();
    newset->menu_label = name;
    newset->menu_style = xset::menu::submenu;
    xset_custom_insert_after(set, newset);

    // add submenu child
    xset_t childset = xset_custom_new();
    newset->child = childset->name;
    childset->parent = newset->name;
    childset->menu_label = "New _Command";
}

static void
xset_design_job_set_sep(xset_t set)
{
    xset_t newset = xset_custom_new();
    newset->menu_style = xset::menu::sep;
    xset_custom_insert_after(set, newset);
}

static void
xset_design_job_set_add_tool(xset_t set, xset::tool tool_type)
{
    if (tool_type < xset::tool::devices || tool_type >= xset::tool::invalid ||
        set->tool == xset::tool::NOT)
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
    if (xset_set_clipboard->tool > xset::tool::custom && set->tool == xset::tool::NOT)
    {
        // failsafe - disallow pasting a builtin tool to a menu
        return false;
    }

    bool update_toolbars = false;
    if (xset_clipboard_is_cut)
    {
        update_toolbars = !(xset_set_clipboard->tool == xset::tool::NOT);
        if (!update_toolbars && xset_set_clipboard->parent)
        {
            xset_t newset = xset_get(xset_set_clipboard->parent.value());
            if (!(newset->tool == xset::tool::NOT))
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
        xset_t newset = xset_custom_copy(xset_set_clipboard, false);
        xset_custom_insert_after(set, newset);
    }

    return update_toolbars;
}

static bool
xset_design_job_set_remove(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    GtkButtonsType buttons;
    GtkWidget* dlgparent = nullptr;

    std::string name;
    std::string prog;

    const auto cmd_type = xset::cmd(xset_get_int(set, xset::var::x));

    if (set->menu_label)
    {
        name = clean_label(set->menu_label.value(), false, false);
    }
    else
    {
        if (!set->lock && set->z && set->menu_style < xset::menu::submenu &&
            cmd_type == xset::cmd::app)
        {
            name = set->z.value();
        }
        else
        {
            name = "( no name )";
        }
    }

    std::string msg;
    if (set->child && set->menu_style == xset::menu::submenu)
    {
        msg = std::format("Permanently remove the '{}' SUBMENU AND ALL ITEMS WITHIN IT?\n\nThis "
                          "action will delete all settings and files associated with these items.",
                          name);
        buttons = GtkButtonsType::GTK_BUTTONS_YES_NO;
    }
    else
    {
        msg = std::format("Permanently remove the '{}' item?\n\nThis action will delete all "
                          "settings and files associated with this item.",
                          name);
        buttons = GtkButtonsType::GTK_BUTTONS_OK_CANCEL;
    }
    const bool is_app = !set->lock && set->menu_style < xset::menu::submenu &&
                        cmd_type == xset::cmd::app && set->tool <= xset::tool::custom;
    if (!(set->menu_style == xset::menu::sep) && app_settings.confirm() && !is_app &&
        set->tool <= xset::tool::custom)
    {
        if (parent)
        {
            dlgparent = gtk_widget_get_toplevel(parent);
        }

        const auto response = ptk_show_message(GTK_WINDOW(dlgparent),
                                               GtkMessageType::GTK_MESSAGE_WARNING,
                                               "Confirm Remove",
                                               buttons,
                                               msg);

        if (response != GtkResponseType::GTK_RESPONSE_OK &&
            response != GtkResponseType::GTK_RESPONSE_YES)
        {
            return false;
        }
    }

    bool update_toolbars;

    // remove
    if (set->parent)
    {
        const xset_t set_next = xset_is(set->parent.value());
        if (set_next && set_next->tool == xset::tool::custom &&
            set_next->menu_style == xset::menu::submenu)
        {
            // this set is first item in custom toolbar submenu
            update_toolbars = true;
        }
    }

    xset_custom_remove(set);

    if (!(set->tool == xset::tool::NOT))
    {
        update_toolbars = true;
    }

    xset_custom_delete(set, false);

    return update_toolbars;
}

static void
xset_design_job_set_normal(xset_t set)
{
    set->menu_style = xset::menu::normal;
}

static void
xset_design_job_set_check(xset_t set)
{
    set->menu_style = xset::menu::check;
}

static void
xset_design_job_set_confirm(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    if (!set->desc)
    {
        set->desc = "Are you sure?";
    }

    const auto [response, answer] = xset_text_dialog(parent,
                                                     "Dialog Message",
                                                     "Enter the message to be displayed in this "
                                                     "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                                                     "",
                                                     set->desc.value(),
                                                     "",
                                                     false);
    set->desc = answer;
    if (response)
    {
        set->menu_style = xset::menu::confirm;
    }
}

static void
xset_design_job_set_dialog(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const auto [response, answer] = xset_text_dialog(parent,
                                                     "Dialog Message",
                                                     "Enter the message to be displayed in this "
                                                     "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                                                     "",
                                                     set->desc.value(),
                                                     "",
                                                     false);
    set->desc = answer;
    if (response)
    {
        set->menu_style = xset::menu::string;
    }
}

static void
xset_design_job_set_message(xset_t set)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    const auto [response, answer] = xset_text_dialog(parent,
                                                     "Dialog Message",
                                                     "Enter the message to be displayed in this "
                                                     "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                                                     "",
                                                     set->desc.value(),
                                                     "",
                                                     false);
    set->desc = answer;
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
    xset_set_b(xset::name::context_dlg, !xset_get_b(xset::name::context_dlg));
}

static void
xset_design_job_set_browse_files(xset_t set)
{
    if (set->tool > xset::tool::custom)
    {
        return;
    }

    const auto folder = vfs::user_dirs->program_config_dir() / "scripts" / set->name;
    if (!std::filesystem::exists(folder))
    {
        std::filesystem::create_directories(folder);
        std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
    }

    if (set->browser)
    {
        set->browser->run_event<spacefm::signal::open_item>(folder, ptk::open_action::dir);
    }
}

static void
xset_design_job_set_term(xset_t set)
{
    if (set->in_terminal)
    {
        set->in_terminal = false;
    }
    else
    {
        set->in_terminal = true;
        set->task = false;
    }
}

static void
xset_design_job_set_keep(xset_t set)
{
    if (set->keep_terminal)
    {
        set->keep_terminal = false;
    }
    else
    {
        set->keep_terminal = true;
    }
}

static void
xset_design_job_set_task(xset_t set)
{
    if (set->task)
    {
        set->task = false;
    }
    else
    {
        set->task = true;
    }
}

static void
xset_design_job_set_pop(xset_t set)
{
    if (set->task_pop)
    {
        set->task_pop = false;
    }
    else
    {
        set->task_pop = true;
    }
}

static void
xset_design_job_set_err(xset_t set)
{
    if (set->task_err)
    {
        set->task_err = false;
    }
    else
    {
        set->task_err = true;
    }
}

static void
xset_design_job_set_out(xset_t set)
{
    if (set->task_out)
    {
        set->task_out = false;
    }
    else
    {
        set->task_out = true;
    }
}

static void
xset_design_job_set_scroll(xset_t set)
{
    if (set->scroll_lock)
    {
        set->scroll_lock = false;
    }
    else
    {
        set->scroll_lock = true;
    }
}

void
xset_design_job(GtkWidget* item, xset_t set)
{
    assert(set != nullptr);

    bool update_toolbars = false;

    xset::tool tool_type;
    const auto job = xset::job(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job")));

    // ztd::logger::info("activate job {} {}", job, set->name);
    switch (job)
    {
        case xset::job::key:
            xset_design_job_set_key(set);
            break;
        case xset::job::icon:
            xset_design_job_set_icon(set);
            break;
        case xset::job::edit:
            xset_design_job_set_edit(set);
            break;
        case xset::job::edit_root:
            xset_design_job_set_edit_root(set);
            break;
        case xset::job::copyname:
            xset_design_job_set_copyname(set);
            break;
        case xset::job::line:
            xset_design_job_set_line(set);
            break;
        case xset::job::script:
            xset_design_job_set_script(set);
            break;
        case xset::job::custom:
            xset_design_job_set_custom(set);
            break;
        case xset::job::user:
            xset_design_job_set_user(set);
            break;
        case xset::job::bookmark:
            break;
        case xset::job::app:
            xset_design_job_set_app(set);
            break;
        case xset::job::command:
            xset_design_job_set_command(set);
            break;
        case xset::job::submenu:
        case xset::job::submenu_book:
            xset_design_job_set_submenu(set);
            break;
        case xset::job::sep:
            xset_design_job_set_sep(set);
            break;
        case xset::job::add_tool:
            tool_type = xset::tool(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tool_type")));
            xset_design_job_set_add_tool(set, tool_type);
            break;
        case xset::job::cut:
            xset_design_job_set_cut(set);
            break;
        case xset::job::copy:
            xset_design_job_set_copy(set);
            break;
        case xset::job::paste:
            update_toolbars = xset_design_job_set_paste(set);
            break;
        case xset::job::remove:
        case xset::job::remove_book:
            update_toolbars = xset_design_job_set_remove(set);
            break;
        case xset::job::normal:
            xset_design_job_set_normal(set);
            break;
        case xset::job::check:
            xset_design_job_set_check(set);
            break;
        case xset::job::confirm:
            xset_design_job_set_confirm(set);
            break;
        case xset::job::dialog:
            xset_design_job_set_dialog(set);
            break;
        case xset::job::message:
            xset_design_job_set_message(set);
            break;
        case xset::job::prop:
            xset_design_job_set_prop(set);
            break;
        case xset::job::prop_cmd:
            xset_design_job_set_prop_cmd(set);
            break;
        case xset::job::ignore_context:
            xset_design_job_set_ignore_context(set);
            break;
        case xset::job::browse_files:
            xset_design_job_set_browse_files(set);
            break;
        case xset::job::term:
            xset_design_job_set_term(set);
            break;
        case xset::job::keep:
            xset_design_job_set_keep(set);
            break;
        case xset::job::task:
            xset_design_job_set_task(set);
            break;
        case xset::job::pop:
            xset_design_job_set_pop(set);
            break;
        case xset::job::err:
            xset_design_job_set_err(set);
            break;
        case xset::job::out:
            xset_design_job_set_out(set);
            break;
        case xset::job::scroll:
            xset_design_job_set_scroll(set);
            break;
        case xset::job::tooltips:
        case xset::job::label:
        case xset::job::help:
        case xset::job::help_new:
        case xset::job::help_add:
        case xset::job::help_browse:
        case xset::job::help_style:
        case xset::job::help_book:
        case xset::job::invalid:
            break;
    }

    if (set && (!set->lock || set->xset_name == xset::name::main_book))
    {
        if (set->parent)
        {
            const xset_t set_next = xset_is(set->parent.value());
            if (set_next && set_next->tool == xset::tool::custom &&
                set_next->menu_style == xset::menu::submenu)
            {
                // this set is first item in custom toolbar submenu
                update_toolbars = true;
            }
        }
    }

    if ((set && !set->lock && !(set->tool == xset::tool::NOT)) || update_toolbars)
    {
        main_window_rebuild_all_toolbars(set ? set->browser : nullptr);
    }

    // autosave
    autosave_request_add();
}
