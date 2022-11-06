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

#include <fmt/format.h>

#include <glibmm.h>

#include <gtk/gtk.h>

#include <glib.h>

#include <exo/exo.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "settings.hxx"

#include "ptk/ptk-error.hxx"

#include "vfs/vfs-user-dir.hxx"

#include "xset/xset.hxx"
#include "xset/xset-dialog.hxx"

char*
multi_input_get_text(GtkWidget* input)
{ // returns a new allocated string or nullptr if input is empty
    GtkTextIter iter, siter;

    if (!GTK_IS_TEXT_VIEW(input))
        return nullptr;

    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(input));
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* ret = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (ret && ret[0] == '\0')
    {
        free(ret);
        ret = nullptr;
    }
    return ret;
}

static void
on_multi_input_insert(GtkTextBuffer* buf)
{ // remove linefeeds from pasted text
    GtkTextIter iter, siter;
    // bool changed = false;
    i32 x;

    // buffer contains linefeeds?
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* all = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (!strchr(all, '\n'))
    {
        free(all);
        return;
    }
    free(all);

    // delete selected text that was pasted over
    if (gtk_text_buffer_get_selection_bounds(buf, &siter, &iter))
        gtk_text_buffer_delete(buf, &siter, &iter);

    GtkTextMark* insert = gtk_text_buffer_get_insert(buf);
    gtk_text_buffer_get_iter_at_mark(buf, &iter, insert);
    gtk_text_buffer_get_start_iter(buf, &siter);
    char* b = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    gtk_text_buffer_get_end_iter(buf, &siter);
    char* a = gtk_text_buffer_get_text(buf, &iter, &siter, false);

    if (strchr(b, '\n'))
    {
        x = 0;
        while (b[x] != '\0')
        {
            if (b[x] == '\n')
                b[x] = ' ';
            x++;
        }
    }
    if (strchr(a, '\n'))
    {
        x = 0;
        while (a[x] != '\0')
        {
            if (a[x] == '\n')
                a[x] = ' ';
            x++;
        }
    }

    g_signal_handlers_block_matched(buf,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_multi_input_insert,
                                    nullptr);

    gtk_text_buffer_set_text(buf, b, -1);
    gtk_text_buffer_get_end_iter(buf, &iter);
    GtkTextMark* mark = gtk_text_buffer_create_mark(buf, nullptr, &iter, true);
    gtk_text_buffer_get_end_iter(buf, &iter);
    gtk_text_buffer_insert(buf, &iter, a, -1);
    gtk_text_buffer_get_iter_at_mark(buf, &iter, mark);
    gtk_text_buffer_place_cursor(buf, &iter);

    g_signal_handlers_unblock_matched(buf,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_multi_input_insert,
                                      nullptr);

    free(a);
    free(b);
}

GtkTextView*
multi_input_new(GtkScrolledWindow* scrolled, const char* text)
{
    GtkTextIter iter;

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    GtkTextView* input = GTK_TEXT_VIEW(gtk_text_view_new());
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request(GTK_WIDGET(input), -1, 50);
    gtk_widget_set_size_request(GTK_WIDGET(scrolled), -1, 50);

    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(input));
    GtkTextBuffer* buf = gtk_text_view_get_buffer(input);
    gtk_text_view_set_wrap_mode(input,
                                GtkWrapMode::GTK_WRAP_CHAR); // GtkWrapMode::GTK_WRAP_WORD_CHAR

    if (text)
        gtk_text_buffer_set_text(buf, text, -1);
    gtk_text_buffer_get_end_iter(buf, &iter);
    gtk_text_buffer_place_cursor(buf, &iter);
    GtkTextMark* insert = gtk_text_buffer_get_insert(buf);
    gtk_text_view_scroll_to_mark(input, insert, 0.0, false, 0, 0);
    gtk_text_view_set_accepts_tab(input, false);

    g_signal_connect_after(G_OBJECT(buf),
                           "insert-text",
                           G_CALLBACK(on_multi_input_insert),
                           nullptr);

    return input;
}

static bool
on_input_keypress(GtkWidget* widget, GdkEventKey* event, GtkWidget* dlg)
{
    (void)widget;
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
    {
        gtk_dialog_response(GTK_DIALOG(dlg), GtkResponseType::GTK_RESPONSE_OK);
        return true;
    }
    return false;
}

i32
xset_msg_dialog(GtkWidget* parent, GtkMessageType action, std::string_view title,
                GtkButtonsType buttons, std::string_view msg1, std::string_view msg2)
{
    GtkWidget* dlgparent = nullptr;

    if (parent)
        dlgparent = gtk_widget_get_toplevel(parent);

    GtkWidget* dlg =
        gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                               GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                              GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                               action,
                               buttons,
                               msg1.data(),
                               nullptr);

    if (action == GtkMessageType::GTK_MESSAGE_INFO)
        xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "msg_dialog");

    if (!msg2.empty())
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), msg2.data(), nullptr);

    gtk_window_set_title(GTK_WINDOW(dlg), title.data());

    gtk_widget_show_all(dlg);
    i32 res = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    return res;
}

bool
xset_text_dialog(GtkWidget* parent, std::string_view title, std::string_view msg1,
                 std::string_view msg2, const char* defstring, char** answer,
                 std::string_view defreset, bool edit_care)
{
    GtkTextIter iter;
    GtkTextIter siter;
    GtkAllocation allocation;
    i32 width;
    i32 height;
    GtkWidget* dlgparent = nullptr;

    if (parent)
        dlgparent = gtk_widget_get_toplevel(parent);

    GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                                            GtkDialogFlags::GTK_DIALOG_MODAL,
                                            GtkMessageType::GTK_MESSAGE_QUESTION,
                                            GtkButtonsType::GTK_BUTTONS_NONE,
                                            msg1.data(),
                                            nullptr);
    xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "text_dialog");

    width = xset_get_int(XSetName::TEXT_DLG, XSetVar::S);
    height = xset_get_int(XSetName::TEXT_DLG, XSetVar::Z);
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
    else
        gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 400);
    // gtk_widget_set_size_request( GTK_WIDGET( dlg ), 600, 400 );

    gtk_window_set_resizable(GTK_WINDOW(dlg), true);

    if (!msg2.empty())
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), msg2.data(), nullptr);

    // input view
    GtkScrolledWindow* scroll_input =
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    GtkTextView* input = multi_input_new(scroll_input, defstring);
    GtkTextBuffer* buf = gtk_text_view_get_buffer(input);

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))),
                       GTK_WIDGET(scroll_input),
                       true,
                       true,
                       4);

    g_signal_connect(G_OBJECT(input), "key-press-event", G_CALLBACK(on_input_keypress), dlg);

    // buttons
    GtkWidget* btn_edit;
    GtkWidget* btn_default = nullptr;
    GtkWidget* btn_icon_choose = nullptr;

    if (edit_care)
    {
        btn_edit = gtk_toggle_button_new_with_mnemonic("_Edit");
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_edit, GtkResponseType::GTK_RESPONSE_YES);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_edit), false);
        gtk_text_view_set_editable(input, false);
    }

    /* Special hack to add an icon chooser button when this dialog is called
     * to set icons - see xset_menu_cb() and set init "main_icon"
     * and xset_design_job */
    if (ztd::same(title, "Set Icon") || ztd::same(title, "Set Window Icon"))
    {
        btn_icon_choose = gtk_button_new_with_mnemonic("C_hoose");
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg),
                                     btn_icon_choose,
                                     GtkResponseType::GTK_RESPONSE_ACCEPT);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_icon_choose), false);
    }

    if (!defreset.empty())
    {
        btn_default = gtk_button_new_with_mnemonic("_Default");
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg),
                                     btn_default,
                                     GtkResponseType::GTK_RESPONSE_NO);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_default), false);
    }

    GtkWidget* btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_cancel, GtkResponseType::GTK_RESPONSE_CANCEL);

    GtkWidget* btn_ok = gtk_button_new_with_label("OK");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_ok, GtkResponseType::GTK_RESPONSE_OK);

    // show
    gtk_widget_show_all(dlg);

    gtk_window_set_title(GTK_WINDOW(dlg), title.data());

    if (edit_care)
    {
        gtk_widget_grab_focus(btn_ok);
        if (btn_default)
            gtk_widget_set_sensitive(btn_default, false);
    }

    std::string ans;
    i32 response;
    char* icon;
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
                if (ztd::contains(ans, "\n"))
                {
                    ptk_show_error(GTK_WINDOW(dlgparent),
                                   "Error",
                                   "Your input is invalid because it contains linefeeds");
                }
                else
                {
                    if (*answer)
                        free(*answer);

                    ans = ztd::strip(ans);
                    if (ans.empty())
                        *answer = nullptr;
                    else
                        *answer = ztd::strdup(Glib::filename_from_utf8(ans));

                    ret = true;
                    exit_loop = true;
                    break;
                }

                break;
            case GtkResponseType::GTK_RESPONSE_YES:
                // btn_edit clicked
                gtk_text_view_set_editable(
                    input,
                    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_edit)));
                if (btn_default)
                    gtk_widget_set_sensitive(
                        btn_default,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_edit)));
                exit_loop = true;
                break;
            case GtkResponseType::GTK_RESPONSE_ACCEPT:
                // get current icon
                gtk_text_buffer_get_start_iter(buf, &siter);
                gtk_text_buffer_get_end_iter(buf, &iter);
                icon = gtk_text_buffer_get_text(buf, &siter, &iter, false);

                // show icon chooser
                char* new_icon;
                new_icon = xset_icon_chooser_dialog(GTK_WINDOW(dlg), icon);
                free(icon);
                if (new_icon)
                {
                    gtk_text_buffer_set_text(buf, new_icon, -1);
                    free(new_icon);
                }
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
            break;
    }

    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if (width && height)
    {
        xset_set(XSetName::TEXT_DLG, XSetVar::S, std::to_string(width));
        xset_set(XSetName::TEXT_DLG, XSetVar::Z, std::to_string(height));
    }
    gtk_widget_destroy(dlg);
    return ret;
}

char*
xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const char* title,
                 const char* deffolder, const char* deffile)
{
    char* path;
    /*  Actions:
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
     */
    GtkWidget* dlgparent = parent ? gtk_widget_get_toplevel(parent) : nullptr;
    GtkWidget* dlg = gtk_file_chooser_dialog_new(title,
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
    xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "file_dialog");

    if (deffolder)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), deffolder);
    else
    {
        path = xset_get_s(XSetName::GO_SET_DEFAULT);
        if (path && path[0] != '\0')
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), path);
        else
        {
            path = (char*)vfs_user_home_dir().data();
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), path);
        }
    }
    if (deffile)
    {
        if (action == GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE ||
            action == GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), deffile);
        else
        {
            const std::string path2 = Glib::build_filename(deffolder, deffile);
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), path2.data());
        }
    }

    i32 width = xset_get_int(XSetName::FILE_DLG, XSetVar::X);
    i32 height = xset_get_int(XSetName::FILE_DLG, XSetVar::Y);
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(dlg);
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_resize(GTK_WINDOW(dlg), width, height);
        while (gtk_events_pending())
            gtk_main_iteration();
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
    }

    i32 response = gtk_dialog_run(GTK_DIALOG(dlg));

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if (width && height)
    {
        xset_set(XSetName::FILE_DLG, XSetVar::X, std::to_string(width));
        xset_set(XSetName::FILE_DLG, XSetVar::Y, std::to_string(height));
    }

    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        char* dest = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        gtk_widget_destroy(dlg);
        return dest;
    }
    gtk_widget_destroy(dlg);
    return nullptr;
}

char*
xset_icon_chooser_dialog(GtkWindow* parent, const char* def_icon)
{
    GtkAllocation allocation;
    char* icon = nullptr;

    // set busy cursor
    GdkCursor* cursor = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(parent)),
                                                   GdkCursorType::GDK_WATCH);
    if (cursor)
    {
        gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(parent)), cursor);
        g_object_unref(cursor);
        while (gtk_events_pending())
            gtk_main_iteration();
    }

    // btn_icon_choose clicked - preparing the exo icon chooser dialog
    GtkWidget* icon_chooser = exo_icon_chooser_dialog_new("Choose Icon",
                                                          GTK_WINDOW(parent),
                                                          "Cancel",
                                                          GtkResponseType::GTK_RESPONSE_CANCEL,
                                                          "OK",
                                                          GtkResponseType::GTK_RESPONSE_ACCEPT,
                                                          nullptr);
    // Set icon chooser dialog size
    i32 width = xset_get_int(XSetName::MAIN_ICON, XSetVar::X);
    i32 height = xset_get_int(XSetName::MAIN_ICON, XSetVar::Y);
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(icon_chooser), width, height);

    // Load current icon
    if (def_icon && def_icon[0])
        exo_icon_chooser_dialog_set_icon(EXO_ICON_CHOOSER_DIALOG(icon_chooser), def_icon);

    // Prompting user to pick icon
    i32 response_icon_chooser = gtk_dialog_run(GTK_DIALOG(icon_chooser));
    if (response_icon_chooser == GtkResponseType::GTK_RESPONSE_ACCEPT)
    {
        /* Fetching selected icon */
        icon = exo_icon_chooser_dialog_get_icon(EXO_ICON_CHOOSER_DIALOG(icon_chooser));
    }

    // Save icon chooser dialog size
    gtk_widget_get_allocation(GTK_WIDGET(icon_chooser), &allocation);
    if (allocation.width && allocation.height)
    {
        xset_set(XSetName::MAIN_ICON, XSetVar::X, std::to_string(allocation.width));
        xset_set(XSetName::MAIN_ICON, XSetVar::Y, std::to_string(allocation.height));
    }
    gtk_widget_destroy(icon_chooser);

    // remove busy cursor
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(parent)), nullptr);

    return icon;
}
