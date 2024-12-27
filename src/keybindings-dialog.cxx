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

#include <gtkmm.h>
#include <glibmm.h>

#if __has_include(<magic_enum/magic_enum.hpp>)
// >=magic_enum-0.9.7
#include <magic_enum/magic_enum.hpp>
#else
// <=magic_enum-0.9.6
#include <magic_enum.hpp>
#endif

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "autosave.hxx"

#include "datatypes/datatypes.hxx"

#include "xset/xset.hxx"
#include "xset/utils/xset-utils.hxx"

#include "keybindings-dialog.hxx"

void
show_keybindings_dialog(GtkWindow* parent) noexcept
{
    (void)parent;

    // Create JSON data for keybindings
    std::vector<datatype::keybinding_dialog::request> request;
    for (const xset_t& set : xset::sets())
    {
        if (set->keybinding.type == xset::set::keybinding_type::invalid)
        {
            continue;
        }

        request.push_back(datatype::keybinding_dialog::request{
            .name = set->name().data(),
            .label = xset::utils::clean_label(set->menu.label.value_or("")),
            .category = magic_enum::enum_name(set->keybinding.type).data(),
            .shared_key = set->shared_key ? set->shared_key->name().data() : "",
            .key = set->keybinding.key,
            .modifier = set->keybinding.modifier});
    }

    std::string buffer;
    auto ec = glz::write_json(request, buffer);
    if (ec)
    {
        logger::error("Failed to create JSON: {}", glz::format_error(ec, buffer));
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

    const auto command = std::format(R"({} --json '{}')", binary, buffer);

    std::int32_t exit_status = 0;
    std::string standard_output;
    Glib::spawn_command_line_sync(command, &standard_output, nullptr, &exit_status);
    if (exit_status != 0 || standard_output.empty())
    {
        return;
    }

    std::vector<datatype::keybinding_dialog::response> response;
    ec = glz::read_json(response, standard_output);
    if (ec)
    {
        logger::error<logger::domain::ptk>("Failed to decode json: {}",
                                           glz::format_error(ec, standard_output));
        return;
    }

    // Update xset keybindings
    for (const auto& data : response)
    {
        const auto set = xset::set::get(data.name);
        set->keybinding.key = data.key;
        set->keybinding.modifier = data.modifier;
    }

    autosave::request_add();
}
