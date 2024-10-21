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

#include <filesystem>

#include <tuple>

#include <optional>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "compat/gtk4-porting.hxx"

#include "ptk/ptk-dialog.hxx"
#include "ptk/utils/ptk-utils.hxx"
#include "ptk/utils/multi-input.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "xset/xset.hxx"
#include "xset/xset-dialog.hxx"

static bool
on_input_keypress(GtkWidget* widget, GdkEvent* event, GtkWidget* dlg) noexcept
{
    (void)widget;
    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto keyval = gdk_key_event_get_keyval(event);
    if (keymod == 0)
    {
        switch (keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
            {
                gtk_dialog_response(GTK_DIALOG(dlg), GtkResponseType::GTK_RESPONSE_OK);
                return true;
            }
            default:
                break;
        }
    }
    return false;
}

std::tuple<bool, std::string>
xset_text_dialog(GtkWidget* parent, const std::string_view title, const std::string_view msg1,
                 const std::string_view msg2, const std::string_view defstring,
                 const std::string_view defreset, bool edit_care) noexcept
{
    GtkTextIter iter;
    GtkTextIter siter;
    GtkAllocation allocation;
    GtkWidget* dlgparent = nullptr;

    if (parent)
    {
#if (GTK_MAJOR_VERSION == 4)
        dlgparent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(parent)));
#elif (GTK_MAJOR_VERSION == 3)
        dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
#endif
    }

    GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                                            GtkDialogFlags::GTK_DIALOG_MODAL,
                                            GtkMessageType::GTK_MESSAGE_QUESTION,
                                            GtkButtonsType::GTK_BUTTONS_NONE,
                                            msg1.data(),
                                            nullptr);
    ptk::utils::set_window_icon(GTK_WINDOW(dlg));

    const auto width = xset_get_int(xset::name::text_dlg, xset::var::s);
    const auto height = xset_get_int(xset::name::text_dlg, xset::var::z);
    if (width && height)
    {
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
    }
    else
    {
        gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 400);
    }
    // gtk_widget_set_size_request( GTK_WIDGET( dlg ), 600, 400 );

    gtk_window_set_resizable(GTK_WINDOW(dlg), true);

    if (!msg2.empty())
    {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), msg2.data(), nullptr);
    }

    // input view
    GtkScrolledWindow* scroll_input =
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    GtkTextView* input = ptk::utils::multi_input_new(scroll_input, defstring);
    GtkTextBuffer* buf = gtk_text_view_get_buffer(input);

    GtkBox* content_area = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg)));
    gtk_box_pack_start(GTK_BOX(content_area), GTK_WIDGET(scroll_input), true, true, 4);

    g_signal_connect(G_OBJECT(input), "key-press-event", G_CALLBACK(on_input_keypress), dlg);

    // buttons
    GtkButton* btn_edit = nullptr;
    GtkButton* btn_default = nullptr;
    GtkButton* btn_icon_choose = nullptr;

    if (edit_care)
    {
        btn_edit = GTK_BUTTON(gtk_toggle_button_new_with_mnemonic("_Edit"));
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg),
                                     GTK_WIDGET(btn_edit),
                                     GtkResponseType::GTK_RESPONSE_YES);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_edit), false);
        gtk_text_view_set_editable(input, false);
    }

    /* Special hack to add an icon chooser button when this dialog is called
     * to set icons - see xset_menu_cb() and set init "main_icon"
     * and xset_design_job */
    if (title == "Set Icon" || title == "Set Window Icon")
    {
        btn_icon_choose = GTK_BUTTON(gtk_button_new_with_mnemonic("C_hoose"));
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg),
                                     GTK_WIDGET(btn_icon_choose),
                                     GtkResponseType::GTK_RESPONSE_ACCEPT);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_icon_choose), false);
    }

    if (!defreset.empty())
    {
        btn_default = GTK_BUTTON(gtk_button_new_with_mnemonic("_Default"));
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg),
                                     GTK_WIDGET(btn_default),
                                     GtkResponseType::GTK_RESPONSE_NO);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_default), false);
    }

    GtkButton* btn_cancel = GTK_BUTTON(gtk_button_new_with_label("Cancel"));
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg),
                                 GTK_WIDGET(btn_cancel),
                                 GtkResponseType::GTK_RESPONSE_CANCEL);

    GtkButton* btn_ok = GTK_BUTTON(gtk_button_new_with_label("OK"));
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg),
                                 GTK_WIDGET(btn_ok),
                                 GtkResponseType::GTK_RESPONSE_OK);

    // show
    gtk_widget_show_all(GTK_WIDGET(dlg));

    gtk_window_set_title(GTK_WINDOW(dlg), title.data());

    if (edit_care)
    {
        gtk_widget_grab_focus(GTK_WIDGET(btn_ok));
        if (btn_default)
        {
            gtk_widget_set_sensitive(GTK_WIDGET(btn_default), false);
        }
    }

    std::string answer;
    std::string ans;
    i32 response = 0;
    bool ret = false;
    while ((response = gtk_dialog_run(GTK_DIALOG(dlg))))
    {
        bool exit_loop = false;
        switch (response)
        {
            case GtkResponseType::GTK_RESPONSE_OK:
                gtk_text_buffer_get_start_iter(buf, &siter);
                gtk_text_buffer_get_end_iter(buf, &iter);
                ans = gtk_text_buffer_get_text(buf, &siter, &iter, false);
                if (ans.contains("\n"))
                {
                    ptk::dialog::error(GTK_WINDOW(dlgparent),
                                       "Error",
                                       "Your input is invalid because it contains linefeeds");
                    break;
                }

                ans = ztd::strip(ans);
                if (ans.empty())
                {
                    answer.clear();
                }
                else
                {
                    answer = std::filesystem::path(ans);
                }

                ret = true;
                exit_loop = true;

                break;
            case GtkResponseType::GTK_RESPONSE_YES:
                // btn_edit clicked
                gtk_text_view_set_editable(
                    input,
                    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_edit)));
                if (btn_default)
                {
                    gtk_widget_set_sensitive(
                        GTK_WIDGET(btn_default),
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_edit)));
                }
                exit_loop = true;
                break;
            case GtkResponseType::GTK_RESPONSE_ACCEPT:
                // show icon chooser
                ptk::dialog::error(nullptr, "Removed", "removed xset_icon_chooser_dialog()");

                exit_loop = true;
                break;
            case GtkResponseType::GTK_RESPONSE_NO:
                // btn_default clicked
                gtk_text_buffer_set_text(buf, defreset.data(), -1);
                exit_loop = true;
                break;
            case GtkResponseType::GTK_RESPONSE_CANCEL:
            case GtkResponseType::GTK_RESPONSE_DELETE_EVENT:
            default:
                exit_loop = true;
                break;
        }
        if (exit_loop)
        {
            break;
        }
    }

    // Saving dialog dimensions
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    xset_set(xset::name::text_dlg, xset::var::s, std::format("{}", allocation.width));
    xset_set(xset::name::text_dlg, xset::var::z, std::format("{}", allocation.height));

    gtk_widget_destroy(dlg);

    return std::make_tuple(ret, answer);
}

std::optional<std::filesystem::path>
xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const std::string_view title,
                 const std::optional<std::filesystem::path>& deffolder,
                 const std::optional<std::filesystem::path>& deffile) noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    (void)parent;
    (void)action;
    (void)title;
    (void)deffolder;
    (void)deffile;
    ptk::dialog::error(nullptr,
                       "Needs Update",
                       "Gtk4 changed and then deprecated the GtkFileChooser API");
    return std::nullopt;
#elif (GTK_MAJOR_VERSION == 3)
    /*  Actions:
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
     */

#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* dlgparent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(parent)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
#endif

    GtkWidget* dlg = gtk_file_chooser_dialog_new(title.data(),
                                                 dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                                                 action,
                                                 "Cancel",
                                                 GtkResponseType::GTK_RESPONSE_CANCEL,
                                                 "OK",
                                                 GtkResponseType::GTK_RESPONSE_OK,
                                                 nullptr);
    // gtk_file_chooser_set_action(GTK_FILE_CHOOSER(dlg),
    // GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), true);
    ptk::utils::set_window_icon(GTK_WINDOW(dlg));

    if (deffolder)
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), deffolder.value().c_str());
    }
    else
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), vfs::user::home().c_str());
    }
    if (deffile)
    {
        if (action == GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE ||
            action == GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
        {
            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), deffile.value().c_str());
        }
        else
        {
            const auto path2 = deffolder.value() / deffile.value();
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), path2.c_str());
        }
    }

    const auto width = xset_get_int(xset::name::file_dlg, xset::var::x);
    const auto height = xset_get_int(xset::name::file_dlg, xset::var::y);
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(GTK_WIDGET(dlg));
#if (GTK_MAJOR_VERSION == 3)
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER_ALWAYS);
#endif
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
        while (g_main_context_pending(nullptr))
        {
            g_main_context_iteration(nullptr, true);
        }
#if (GTK_MAJOR_VERSION == 3)
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
#endif
    }

    const auto response = gtk_dialog_run(GTK_DIALOG(dlg));

    // Saving dialog dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    xset_set(xset::name::file_dlg, xset::var::x, std::format("{}", allocation.width));
    xset_set(xset::name::file_dlg, xset::var::y, std::format("{}", allocation.height));

    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        const char* dest = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        gtk_widget_destroy(dlg);
        if (dest != nullptr)
        {
            return dest;
        }
        return std::nullopt;
    }
    gtk_widget_destroy(dlg);
    return std::nullopt;
#endif
}
