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

#include <glibmm.h>
#include <gtkmm.h>

#include "gui/dialog/about.hxx"

gui::dialog::about::about(Gtk::ApplicationWindow& parent)
{
    set_transient_for(parent);
    set_modal(true);

    set_logo_icon_name(PACKAGE_NAME);

    set_program_name(PACKAGE_NAME_FANCY);
    set_version(PACKAGE_VERSION);
    set_comments("This is an unofficial fork");
    set_license_type(Gtk::License::GPL_3_0);

    set_website(PACKAGE_GITHUB);
    set_website_label(PACKAGE_GITHUB);

    const std::vector<Glib::ustring> authors{
        "",
        "SpaceFM v4",
        "Developer:   Brandon Zorn (thermitegod) <43489010+thermitegod@users.noreply.github.com>",
        "   C++, Gtk 4, lots of bugs",
        "",
        "Includes Code From:",
        "libudevpp <https://github.com/zeroping/libudevpp>",
        "mcomix-lite <https://github.com/thermitegod/mcomix-lite>",
        "natsort <https://github.com/sourcefrog/natsort>",
        "notify-cpp <https://github.com/sizeofvoid/notify-cpp>",
        "",
        "SpaceFM v2 Legacy Code",
        "Developer:   IgnorantGuru <ignorantguru@gmx.com>",
        "Contributor: BwackNinja <BwackNinja@gmail.com>",
        "Contributor: OmegaPhil <OmegaPhil@startmail.com>",
        "",
        "PCManFM Legacy Code:",
        "Developer:   (Hong Jen Yee, aka PCMan) <pcman.tw@gmail.com>",
        "Contributor: Cantona <cantona@cantona.net>",
        "Contributor: Orlando Fiol <fiolorlando@gmail.com>",
        "Contributor: Jim Huang (jserv) <jserv.tw@gmail.com>",
        "Contributor: Ying-Chun Liu (PaulLiu) <grandpaul@gmail.com>",
        "Contributor: Mamoru Tasaka <mtasaka@ioa.s.u-tokyo.ac.jp>",
        "Contributor: Kirilov Georgi <kirilov-georgi@users.sourceforge.net>",
        "Contributor: Martijn Dekker <mcdutchie@users.sourceforge.net>",
        "Contributor: Eugene Arshinov (statc) <earshinov@gmail.com>",
        "",
        "Special thanks to:",
        "Jean-Philippe Fleury",
        "Vladimir Kudrya",
        "VastOne",
        "Hasufell",
        "",
        "External source code taken from other projects:",
        "pcmanfm-mod: IgnorantGuru <ignorantguru@gmx.com>",
        "pcmanfm v0.5.2: (Hong Jen Yee, aka PCMan) <pcman.tw@gmail.com>",
        "Working area detection: Gary Kramlich",
        "Text and icon renderer uses code from Jonathan Blandford",
        "Desktop icons use code from Brian Tarricone",
        "gnome-vfs, thunar-vfs, libexo, gnome-mount",
    };
    set_authors(authors);

    const std::vector<Glib::ustring> artists = {
        "",
        "SpaceFM icons and logo by Goran Simovic",
        "Additional icons from the Tango Desktop Project icon set",
    };
    set_artists(artists);

    present();
}
