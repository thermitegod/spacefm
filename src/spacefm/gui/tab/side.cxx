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
#include <sigc++/sigc++.h>

#include "gui/tab/side.hxx"

#include "vfs/user-dirs.hxx"

// TODO
// - add config to choose which dirs get shown
// - context menu for, open, open new tab, open new window?, open properties dialog
// - add block device entries or other mounts
// - better separator, use css?
// - add sections Recent, Starred, use Gtk::Expander?

gui::side::side(const std::shared_ptr<config::settings>& settings) : settings_(settings)
{
    set_orientation(Gtk::Orientation::VERTICAL);
    set_halign(Gtk::Align::START);
    set_valign(Gtk::Align::START);
    set_expand(true);
    set_spacing(5);

    add_location("Home", vfs::user::home(), "user-home-symbolic");
    add_separator();

    if (settings_->interface.sidebar_show_advanced)
    {
        add_location("Cache", vfs::user::cache(), "folder-symbolic");
        add_location("Config", vfs::user::config(), "folder-symbolic");
        add_location("Data", vfs::user::data(), "folder-symbolic");
        add_separator();
    }

    if (settings_->interface.sidebar_show_desktop)
    {
        add_location("Desktop", vfs::user::desktop(), "user-desktop-symbolic");
    }
    if (settings_->interface.sidebar_show_documents)
    {
        add_location("Documents", vfs::user::documents(), "folder-documents-symbolic");
    }
    if (settings_->interface.sidebar_show_download)
    {
        add_location("Download", vfs::user::download(), "folder-download-symbolic");
    }
    if (settings_->interface.sidebar_show_music)
    {
        add_location("Music", vfs::user::music(), "folder-music-symbolic");
    }
    if (settings_->interface.sidebar_show_pictures)
    {
        add_location("Pictures", vfs::user::pictures(), "folder-pictures-symbolic");
    }
    if (settings_->interface.sidebar_show_videos)
    {
        add_location("Videos", vfs::user::videos(), "folder-videos-symbolic");
    }

    set_visible(true);
}

void
gui::side::add_location(const std::string_view name, const std::filesystem::path& path,
                        const std::string_view icon_name) noexcept
{
    auto box = Gtk::make_managed<Gtk::Box>();
    auto icon = Gtk::make_managed<Gtk::Image>();
    auto label = Gtk::make_managed<Gtk::Label>();
    auto button = Gtk::make_managed<Gtk::Button>();

    label->set_text(name.data());
    icon->set_from_icon_name(icon_name.data());

    box->set_orientation(Gtk::Orientation::HORIZONTAL);
    box->set_spacing(10);
    box->set_margin(10);
    box->append(*icon);
    box->append(*label);

    // this is kind of a hack to get the button
    // clickbox to expand and fill all the horizontal space
    button->set_size_request(1000, -1);
    button->set_hexpand(true);
    button->set_halign(Gtk::Align::FILL);
    button->set_has_frame(false);
    button->set_child(*box);

    button->signal_clicked().connect([this, path]() { signal_chdir().emit(path); });

    append(*button);
}

void
gui::side::add_separator() noexcept
{
    auto separator = Gtk::make_managed<Gtk::Separator>();

    append(*separator);
}
