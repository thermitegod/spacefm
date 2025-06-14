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

#include <vector>

#include <gtkmm.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "datatypes/datatypes.hxx"
#include "datatypes/external-dialog.hxx"

#include "xset/utils/xset-utils.hxx"
#include "xset/xset.hxx"

#include "gui/dialog/keybindings.hxx"

#include "autosave.hxx"
#include "package.hxx"

void
show_keybindings_dialog(GtkWindow* parent) noexcept
{
    (void)parent;

    std::vector<datatype::keybinding::request_data> data;
    for (const xset_t& set : xset::sets())
    {
        if (set->keybinding.type == xset::set::keybinding_type::invalid)
        {
            continue;
        }

        data.push_back({.name = set->name().data(),
                        .label = xset::utils::clean_label(set->menu.label.value_or("")),
                        .category = magic_enum::enum_name(set->keybinding.type).data(),
                        .shared_key = set->shared_key ? set->shared_key->name().data() : "",
                        .key = set->keybinding.key.data(),
                        .modifier = set->keybinding.modifier.data()});
    }

    const auto response = datatype::run_dialog_sync<datatype::keybinding::response>(
        spacefm::package.dialog.keybindings,
        datatype::keybinding::request{.data = data});
    if (!response)
    {
        return;
    }

    // Update xset keybindings
    for (const auto& r : response->data)
    {
        const auto set = xset::set::get(r.name);
        set->keybinding.key = r.key;
        set->keybinding.modifier = r.modifier;
    }

    autosave::request_add();
}
