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

#include "gui/tab/toolbar.hxx"

#include "vfs/user-dirs.hxx"

gui::toolbar::toolbar(const std::shared_ptr<config::settings>& settings) : settings_(settings)
{
    set_orientation(Gtk::Orientation::HORIZONTAL);
    set_margin_top(2);
    set_margin_bottom(2);

    button_back_.set_image_from_icon_name("go-previous-symbolic");
    button_forward_.set_image_from_icon_name("go-next-symbolic");
    button_up_.set_image_from_icon_name("go-up-symbolic");
    button_home_.set_image_from_icon_name("go-home-symbolic");
    button_refresh_.set_image_from_icon_name("view-refresh-symbolic");

    button_back_.set_has_frame(false);
    button_forward_.set_has_frame(false);
    button_up_.set_has_frame(false);
    button_home_.set_has_frame(false);
    button_refresh_.set_has_frame(false);

    append(button_back_);
    append(button_forward_);
    append(button_up_);
    append(button_home_);
    append(button_refresh_);

    button_home_.signal_clicked().connect([this]() { signal_chdir().emit(vfs::user::home()); });

    append(path_);
    path_.set_margin_start(5);
    path_.signal_confirm().connect([this](const std::filesystem::path& path)
                                   { signal_chdir().emit(path); });

    append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));

    append(search_);
    search_.set_margin_start(5);
    search_.set_margin_end(5);
    search_.signal_confirm().connect([this](const std::string& text)
                                     { signal_filter().emit(text); });

    button_home_.set_visible(settings_->interface.show_toolbar_home);
    button_refresh_.set_visible(settings_->interface.show_toolbar_refresh);
    search_.set_visible(settings_->interface.show_toolbar_search);
}

void
gui::toolbar::update(const std::filesystem::path& path, bool has_back, bool has_forward,
                     bool has_up) noexcept
{
    path_.set_text(path.string());

    button_back_.set_sensitive(has_back);
    button_forward_.set_sensitive(has_forward);
    button_up_.set_sensitive(has_up);
}

void
gui::toolbar::focus_path() noexcept
{
    path_.grab_focus();
}

void
gui::toolbar::focus_search() noexcept
{
    search_.grab_focus();
}
