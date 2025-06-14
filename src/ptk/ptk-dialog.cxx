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

#include <filesystem>
#include <optional>
#include <print>
#include <string_view>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "datatypes/datatypes.hxx"
#include "datatypes/external-dialog.hxx"

#include "ptk/ptk-dialog.hxx"

#include "package.hxx"

std::tuple<bool, std::string>
ptk::dialog::text(GtkWidget* parent, const std::string_view title, const std::string_view message,
                  const std::string_view defstring, const std::string_view defreset) noexcept
{
    (void)parent;

    const auto response = datatype::run_dialog_sync<datatype::text::response>(
        spacefm::package.dialog.text,
        datatype::text::request{.title = title.data(),
                                .message = message.data(),
                                .text = defreset.data(),
                                .text_default = defreset.data()});
    if (!response)
    {
        return {false, defstring.data()};
    }

    return {true, response->text};
}

std::optional<std::filesystem::path>
ptk::dialog::file_chooser(GtkWidget* parent, GtkFileChooserAction action,
                          const std::string_view title,
                          const std::optional<std::filesystem::path>& deffolder,
                          const std::optional<std::filesystem::path>& deffile) noexcept
{
    (void)parent;

    const auto response = datatype::run_dialog_sync<datatype::file_chooser::response>(
        spacefm::package.dialog.file_chooser,
        datatype::file_chooser::request{
            .title = title.data(),
            .mode = (action == GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
                        ? datatype::file_chooser::mode::dir
                        : datatype::file_chooser::mode::file,
            .default_path = deffolder.value_or(""),
            .default_file = deffile.value_or("")});
    if (!response)
    {
        return std::nullopt;
    }

    return response->path;
}

void
ptk::dialog::error(GtkWindow* parent, const std::string_view title,
                   const std::string_view message) noexcept
{
    (void)parent;

    datatype::run_dialog_async(
        spacefm::package.dialog.error,
        datatype::error::request{.title = title.data(), .message = message.data()});
}

GtkResponseType
ptk::dialog::message(GtkWindow* parent, GtkMessageType action, const std::string_view title,
                     GtkButtonsType buttons, const std::string_view message,
                     const std::string_view secondary_message) noexcept
{
    (void)parent;
    (void)action;

    assert(buttons != GtkButtonsType::GTK_BUTTONS_NONE);

    const auto response = datatype::run_dialog_sync<datatype::message::response>(
        spacefm::package.dialog.message,
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
