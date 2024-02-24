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

void
show_about_dialog(GtkWindow* parent) noexcept
{
    (void)parent;

    Gtk::AboutDialog dialog;

    // dialog.set_transient_for(parent);

    dialog.set_program_name(PACKAGE_NAME_FANCY);
    dialog.set_version(PACKAGE_VERSION);
    dialog.set_logo_icon_name("spacefm");
    dialog.set_comments("(unofficial)");
    dialog.set_website("https://github.com/thermitegod/spacefm");
    dialog.set_website_label("Github");
#if (GTK_MAJOR_VERSION == 4)
    dialog.set_license_type(Gtk::License::GPL_3_0);
#elif (GTK_MAJOR_VERSION == 3)
    dialog.set_license_type(Gtk::License::LICENSE_GPL_3_0);
#endif

#if (GTK_MAJOR_VERSION == 4)
    // TODO
#elif (GTK_MAJOR_VERSION == 3)
    dialog.run();
#endif
}
