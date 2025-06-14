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

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "datatypes/datatypes.hxx"
#include "datatypes/external-dialog.hxx"

#include "gui/dialog/app-chooser.hxx"

#include "vfs/mime-type.hxx"

#include "package.hxx"

std::optional<std::string>
gui_choose_app_for_mime_type(GtkWindow* parent, const std::shared_ptr<vfs::mime_type>& mime_type,
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

    const auto response = datatype::run_dialog_sync<datatype::app_chooser::response>(
        spacefm::package.dialog.app_chooser,
        datatype::app_chooser::request{
            .mime_type = mime_type->type().data(),
            .focus_all_apps = focus_all_apps,
            .show_command = show_command,
            .show_default = show_default,
            .dir_default = dir_default,
        });
    if (!response)
    {
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
