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

#include <memory>

#include <optional>

#include <glibmm.h>

#include <glaze/glaze.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "datatypes/datatypes.hxx"

#include "vfs/vfs-mime-type.hxx"

#include "ptk/ptk-app-chooser.hxx"

std::optional<std::string>
ptk_choose_app_for_mime_type(GtkWindow* parent, const std::shared_ptr<vfs::mime_type>& mime_type,
                             bool focus_all_apps, bool show_command, bool show_default,
                             bool dir_default) noexcept
{
    (void)parent;

    /*
    focus_all_apps      Focus All Apps tab by default
    show_command        Show custom Command entry
    show_default        Show 'Set as default' checkbox
    dir_default         Show 'Set as default' also for type dir
    */

#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_APP_CHOOSER);
#else
    const auto binary = Glib::find_program_in_path(DIALOG_APP_CHOOSER);
#endif
    if (binary.empty())
    {
        logger::error("Failed to find pattern dialog binary: {}", DIALOG_APP_CHOOSER);
        return std::nullopt;
    }

    // clang-format off
    const auto request = datatype::app_chooser::request{
        .mime_type = mime_type->type().data(),
        .focus_all_apps = focus_all_apps,
        .show_command = show_command,
        .show_default = show_default,
        .dir_default = dir_default,
    };
    // clang-format on

    const auto buffer = glz::write_json(request);
    if (!buffer)
    {
        logger::error("Failed to create json: {}", glz::format_error(buffer));
        return std::nullopt;
    }
    // logger::trace("{}", buffer);

    const auto command = std::format(R"({} --json '{}')", binary, buffer.value());

    std::int32_t exit_status = 0;
    std::string standard_output;
    Glib::spawn_command_line_sync(command, &standard_output, nullptr, &exit_status);
    if (exit_status != 0 || standard_output.empty())
    {
        return std::nullopt;
    }

    const auto response = glz::read_json<datatype::app_chooser::response>(standard_output);
    if (!response)
    {
        logger::error<logger::domain::ptk>("Failed to decode json: {}",
                                           glz::format_error(response.error(), standard_output));
        return std::nullopt;
    }

    if (response->is_desktop && response->set_default)
    { // The selected app is set to default action
        mime_type->set_default_action(response->app);
    }
    else if (/* mime_type->get_type() != mime_type::type::unknown && */
             (dir_default || mime_type->type() != vfs::constants::mime_type::directory))
    {
        return mime_type->add_action(response->app);
    }

    return response->app;
}
