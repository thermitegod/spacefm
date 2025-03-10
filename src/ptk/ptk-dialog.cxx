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

#include <ztd/ztd.hxx>

#include "datatypes/datatypes.hxx"
#include "datatypes/external-dialog.hxx"

#include "ptk/ptk-dialog.hxx"

void
ptk::dialog::error(GtkWindow* parent, const std::string_view title,
                   const std::string_view message) noexcept
{
    (void)parent;

    datatype::run_dialog_async(
        DIALOG_ERROR,
        datatype::error::request{.title = title.data(), .message = message.data()});
}

i32
ptk::dialog::message(GtkWindow* parent, GtkMessageType action, const std::string_view title,
                     GtkButtonsType buttons, const std::string_view message,
                     const std::string_view secondary_message) noexcept
{
    (void)parent;
    (void)action;

    assert(buttons != GtkButtonsType::GTK_BUTTONS_NONE);

    const auto response = datatype::run_dialog_sync<datatype::message::response>(
        DIALOG_MESSAGE,
        datatype::message::request{.title = title.data(),
                                   .message = message.data(),
                                   .secondary_message = secondary_message.data(),
                                   .button_ok = buttons == GtkButtonsType::GTK_BUTTONS_OK,
                                   .button_cancel = buttons == GtkButtonsType::GTK_BUTTONS_CANCEL,
                                   .button_close = buttons == GtkButtonsType::GTK_BUTTONS_CLOSE,
                                   .button_yes_no = buttons == GtkButtonsType::GTK_BUTTONS_YES_NO,
                                   .button_ok_cancel =
                                       buttons == GtkButtonsType::GTK_BUTTONS_OK_CANCEL});
    if (!response)
    {
        return GtkResponseType::GTK_RESPONSE_NONE;
    }

    // Send correct gtk response code
    if (response->result == "Ok")
    {
        return GtkResponseType::GTK_RESPONSE_OK;
    }
    else if (response->result == "Close")
    {
        return GtkResponseType::GTK_RESPONSE_CLOSE;
    }
    else if (response->result == "Cancel")
    {
        return GtkResponseType::GTK_RESPONSE_CANCEL;
    }
    else if (response->result == "Yes")
    {
        return GtkResponseType::GTK_RESPONSE_YES;
    }
    else if (response->result == "No")
    {
        return GtkResponseType::GTK_RESPONSE_NO;
    }
    else
    {
        return GtkResponseType::GTK_RESPONSE_NONE;
    }
}
