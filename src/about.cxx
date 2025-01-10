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

#include <gtkmm.h>

#include "about.hxx"
#include "logger.hxx"

void
show_about_dialog(GtkWindow* parent) noexcept
{
    (void)parent;

#if defined(HAVE_DEV)
    const auto command = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_ABOUT);
#else
    const auto command = Glib::find_program_in_path(DIALOG_ABOUT);
#endif
    if (command.empty())
    {
        logger::error("Failed to find about dialog binary: {}", DIALOG_ABOUT);
        return;
    }

    Glib::spawn_command_line_async(command);
}
