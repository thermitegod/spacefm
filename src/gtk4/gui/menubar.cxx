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

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include "gui/menubar.hxx"

gui::menubar::menubar()
{
    auto menu = Gio::Menu::create();
    menu->append_submenu("File", create_file());
    menu->append_submenu("View", create_view());
    menu->append_submenu("Device", create_device());
    menu->append_submenu("Bookmarks", create_bookmarks());
    menu->append_submenu("Help", create_help());
    set_menu_model(menu);
}

Glib::RefPtr<Gio::Menu>
gui::menubar::create_file() noexcept
{
    auto menu = Gio::Menu::create();

    {
        auto section = Gio::Menu::create();
        section->append("Open URL", "app.open");
        section->append("File Search", "app.search");

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        Glib::RefPtr<Gio::MenuItem> item;

        item = Gio::MenuItem::create("Terminal", "app.terminal");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("F4"));
        section->append_item(item);

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        Glib::RefPtr<Gio::MenuItem> item;

        item = Gio::MenuItem::create("New Window", "app.new_window");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>N"));
        section->append_item(item);

        section->append("Close Window", "app.close");

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        Glib::RefPtr<Gio::MenuItem> item;

        item = Gio::MenuItem::create("Exit", "app.quit");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>Q"));
        section->append_item(item);

        menu->append_section(section);
    }

    return menu;
}

Glib::RefPtr<Gio::Menu>
gui::menubar::create_view() noexcept
{
    auto menu = Gio::Menu::create();

    {
        auto section = Gio::Menu::create();
        Glib::RefPtr<Gio::MenuItem> item;

        item = Gio::MenuItem::create("Panel 1", "app.panel_1");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>1"));
        section->append_item(item);

        item = Gio::MenuItem::create("Panel 2", "app.panel_2");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>2"));
        section->append_item(item);

        item = Gio::MenuItem::create("Panel 3", "app.panel_3");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>3"));
        section->append_item(item);

        item = Gio::MenuItem::create("Panel 4", "app.panel_4");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>4"));
        section->append_item(item);

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        section->append("Focus", "app.todo"); // TODO SUBMENU
        section->append("View", "app.todo");  // TODO SUBMENU

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        section->append("Task Manager", "app.todo"); // TODO SUBMENU

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        section->append("Window Title", "app.title");
        section->append("Fullscreen", "app.fullscreen");

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        Glib::RefPtr<Gio::MenuItem> item;

        section->append("Keybindings", "app.keybindings");

        item = Gio::MenuItem::create("Preferences", "app.preferences");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("F12"));
        section->append_item(item);

        menu->append_section(section);
    }

    return menu;
}

Glib::RefPtr<Gio::Menu>
gui::menubar::create_device() noexcept
{
    auto menu = Gio::Menu::create();

    {
        auto section = Gio::Menu::create();
        section->append("TODO", "app.todo");
        section->append("TODO", "app.todo");
        section->append("TODO", "app.todo");
        section->append("TODO", "app.todo");

        menu->append_section(section);
    }

    return menu;
}

Glib::RefPtr<Gio::Menu>
gui::menubar::create_bookmarks() noexcept
{
    auto menu = Gio::Menu::create();
    Glib::RefPtr<Gio::MenuItem> item;

    item = Gio::MenuItem::create("Add Bookmark", "app.bookmark_add");
    item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>D"));
    menu->append_item(item);

    item = Gio::MenuItem::create("Open Bookmark Manager", "app.bookmark_manager");
    item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Shift><Control>O"));
    menu->append_item(item);

    return menu;
}

Glib::RefPtr<Gio::Menu>
gui::menubar::create_help() noexcept
{
    auto menu = Gio::Menu::create();
    Glib::RefPtr<Gio::MenuItem> item;

    item = Gio::MenuItem::create("About", "app.about");
    item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("F1"));
    menu->append_item(item);

    menu->append("Donate", "app.donate");

    return menu;
}
