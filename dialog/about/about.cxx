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
#include <glibmm.h>

#include "about.hxx"

AboutDialog::AboutDialog()
{
    this->set_logo_icon_name("spacefm");

    this->set_program_name(PACKAGE_NAME_FANCY);
    this->set_version(PACKAGE_VERSION);
    this->set_comments("UNOFFICIAL");
    this->set_license_type(Gtk::License::GPL_3_0);

    this->set_website(PACKAGE_GITHUB);
    this->set_website_label(PACKAGE_GITHUB);

    const std::vector<Glib::ustring> authors{
        "",
        "SpaceFM Legacy Code",
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
        "ExoIconView: os-cillation e.K, Anders Carlsson, &amp; Benedikt Meurer",
        "Text and icon renderer uses code from Jonathan Blandford",
        "Desktop icons use code from Brian Tarricone",
        "gnome-vfs, thunar-vfs, libexo, gnome-mount",
    };

    this->set_authors(authors);

    const std::vector<Glib::ustring> artists = {
        "",
        "SpaceFM icons and logo by Goran Simovic",
        "Additional icons from the Tango Desktop Project icon set",
    };
    this->set_artists(artists);

    this->set_visible(true);
}
