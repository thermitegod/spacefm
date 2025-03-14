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

#include <filesystem>
#include <string_view>

#include <cstdint>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "gui/dialog/donate.hxx"

gui::dialog::donate::page::page() noexcept
{
    set_orientation(Gtk::Orientation::VERTICAL);
    set_spacing(5);

    img_.set_hexpand(true);
    img_.set_vexpand(true);

    label_.set_selectable(true);
    label_.set_margin(5);

    append(img_);
    append(label_);
}

void
gui::dialog::donate::page::set_image(const std::filesystem::path& path) noexcept
{
    try
    {
        img_.set_paintable(Gdk::Texture::create_from_filename(path));
    }
    catch (const Glib::Error& ex)
    {
        return;
    }
}

void
gui::dialog::donate::page::set_label(const std::string_view text) noexcept
{
    label_.set_markup(std::format("<big>{}</big>", text));
}

gui::dialog::donate::donate(Gtk::ApplicationWindow& parent) noexcept
{
    set_transient_for(parent);
    set_modal(true);

    set_size_request(500, 500);
    set_resizable(false);

    set_title("Support Dialog");

    // Content //

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.append(notebook_);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &donate::on_key_press),
                                                 false);
    add_controller(key_controller);

    { // BTC //
        auto img = std::filesystem::path(PACKAGE_IMAGES) / "btc.png";
        if (!std::filesystem::exists(img))
        { // Could not find image in system path, use image in repo.
            img = std::filesystem::path(PACKAGE_IMAGES_LOCAL) / "btc.png";
        }

        auto page = donate::page();
        page.set_image(img);
        page.set_label("bc1qzus6vvyzvgqjxw8mxnj65fapjrmwuzvtlmpw72");

        notebook_.append_page(page, "BTC");
    }

    { // ETH //
        auto img = std::filesystem::path(PACKAGE_IMAGES) / "eth.png";
        if (!std::filesystem::exists(img))
        { // Could not find image in system path, use image in repo.
            img = std::filesystem::path(PACKAGE_IMAGES_LOCAL) / "eth.png";
        }

        auto page = donate::page();
        page.set_image(img);
        page.set_label("0x056d6eC68806Ab139C15B4Dd5736C45295AF0d32");

        notebook_.append_page(page, "ETH");
    }

    // Buttons //

    button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 0);
    button_close_ = Gtk::Button("_Close", true);
    button_close_.signal_clicked().connect([this]() { on_button_close_clicked(); });

    box_.append(button_box_);
    button_box_.set_halign(Gtk::Align::END);
    button_box_.append(button_close_);
    button_box_.set_margin(5);

    set_child(box_);

    present();
}

bool
gui::dialog::donate::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                                  Gdk::ModifierType state) noexcept
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Escape)
    {
        on_button_close_clicked();
    }
    return false;
}

void
gui::dialog::donate::on_button_close_clicked() noexcept
{
    close();
}
