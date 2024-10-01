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

#include <string_view>

#include <gtkmm.h>

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "datatypes/datatypes.hxx"

#include "logger.hxx"

#if (GTK_MAJOR_VERSION == 4)
#include "compat/gtk4-porting.hxx"
#endif

#include "ptk/ptk-dialog.hxx"

void
ptk::dialog::error(GtkWindow* parent, const std::string_view title,
                   const std::string_view message) noexcept
{
    GtkWidget* dialog =
        gtk_message_dialog_new(GTK_WINDOW(parent),
                               GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                              GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                               GtkMessageType::GTK_MESSAGE_ERROR,
                               GtkButtonsType::GTK_BUTTONS_OK,
                               message.data(),
                               nullptr);

    gtk_window_set_title(GTK_WINDOW(dialog), title.empty() ? "Error" : title.data());

    // g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(g_object_unref), nullptr);
    // gtk_widget_show(GTK_WIDGET(dialog));

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

i32
ptk::dialog::message(GtkWindow* parent, GtkMessageType action, const std::string_view title,
                     GtkButtonsType buttons, const std::string_view message,
                     const std::string_view secondary_message) noexcept
{
    (void)parent;
    (void)action;

    assert(buttons != GtkButtonsType::GTK_BUTTONS_NONE);

#if defined(HAVE_DEV)
    auto command = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_MESSAGE);
#else
    auto command = Glib::find_program_in_path(DIALOG_MESSAGE);
#endif
    if (command.empty())
    {
        logger::error("Failed to find message dialog binary: {}", DIALOG_MESSAGE);
        return GtkResponseType::GTK_RESPONSE_NONE;
    }

    std::string button_flag;
    switch (buttons)
    {
        case GtkButtonsType::GTK_BUTTONS_OK:
            button_flag = "--button-ok";
            break;
        case GtkButtonsType::GTK_BUTTONS_CLOSE:
            button_flag = "--button-close";
            break;
        case GtkButtonsType::GTK_BUTTONS_CANCEL:
            button_flag = "--button-cancel";
            break;
        case GtkButtonsType::GTK_BUTTONS_YES_NO:
            button_flag = "--button-yes-no";
            break;
        case GtkButtonsType::GTK_BUTTONS_OK_CANCEL:
            button_flag = "--button-ok-cancel";
            break;
        case GtkButtonsType::GTK_BUTTONS_NONE:
        default:
            assert(false);
            break;
    }

    command = std::format(R"({} --title "{}" --message "{}" --secondary_message "{}" {})",
                          command,
                          title,
                          message,
                          secondary_message,
                          button_flag);

    std::string standard_output;
    Glib::spawn_command_line_sync(command, &standard_output);

    // Result

    if (standard_output.empty())
    {
        return GtkResponseType::GTK_RESPONSE_NONE;
    }

    datatype::message_dialog::response response;
    const auto ec = glz::read_json(response, standard_output);
    if (ec)
    {
        logger::error<logger::domain::ptk>("Failed to decode json: {}",
                                           glz::format_error(ec, standard_output));
        return GtkResponseType::GTK_RESPONSE_NONE;
    }

    // Send correct gtk response code
    if (response.result == "Ok")
    {
        return GtkResponseType::GTK_RESPONSE_OK;
    }
    else if (response.result == "Close")
    {
        return GtkResponseType::GTK_RESPONSE_CLOSE;
    }
    else if (response.result == "Cancel")
    {
        return GtkResponseType::GTK_RESPONSE_CANCEL;
    }
    else if (response.result == "Yes")
    {
        return GtkResponseType::GTK_RESPONSE_YES;
    }
    else if (response.result == "No")
    {
        return GtkResponseType::GTK_RESPONSE_NO;
    }
    else
    {
        return GtkResponseType::GTK_RESPONSE_NONE;
    }
}
