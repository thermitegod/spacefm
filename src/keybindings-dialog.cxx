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
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>

#include <glaze/glaze.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "datatypes/datatypes.hxx"

#include "utils/shell-quote.hxx"

#include "xset/utils/xset-utils.hxx"
#include "xset/xset.hxx"

#include "autosave.hxx"
#include "keybindings-dialog.hxx"
#include "logger.hxx"

void
show_keybindings_dialog(GtkWindow* parent) noexcept
{
    (void)parent;

    // Create JSON data for keybindings
    std::vector<datatype::keybinding::request> request;
    for (const xset_t& set : xset::sets())
    {
        if (set->keybinding.type == xset::set::keybinding_type::invalid)
        {
            continue;
        }

        request.push_back(datatype::keybinding::request{
            .name = set->name().data(),
            .label = xset::utils::clean_label(set->menu.label.value_or("")),
            .category = magic_enum::enum_name(set->keybinding.type).data(),
            .shared_key = set->shared_key ? set->shared_key->name().data() : "",
            .key = set->keybinding.key,
            .modifier = set->keybinding.modifier});
    }

    const auto buffer = glz::write_json(request);
    if (!buffer)
    {
        logger::error("Failed to create json: {}", glz::format_error(buffer));
        return;
    }

    // Run command

#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_KEYBINDINGS);
#else
    const auto binary = Glib::find_program_in_path(DIALOG_KEYBINDINGS);
#endif
    if (binary.empty())
    {
        logger::error("Failed to find keybinding dialog binary: {}", DIALOG_KEYBINDINGS);
        return;
    }

    const auto command = std::format(R"({} --json {})", binary, utils::shell_quote(buffer.value()));

    std::int32_t exit_status = 0;
    std::string standard_output;
    Glib::spawn_command_line_sync(command, &standard_output, nullptr, &exit_status);
#if defined(HAVE_DEV) && __has_feature(address_sanitizer)
    // AddressSanitizer does not like GTK
    if (standard_output.empty())
#else
    if (exit_status != 0 || standard_output.empty())
#endif
    {
        return;
    }

    const auto response =
        glz::read_json<std::vector<datatype::keybinding::response>>(standard_output);
    if (!response)
    {
        logger::error<logger::domain::ptk>("Failed to decode json: {}",
                                           glz::format_error(response.error(), standard_output));
        return;
    }

    // Update xset keybindings
    for (const auto& r : response.value())
    {
        const auto set = xset::set::get(r.name);
        set->keybinding.key = r.key;
        set->keybinding.modifier = r.modifier;
    }

    autosave::request_add();
}
