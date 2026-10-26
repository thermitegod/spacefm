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
        section->append("Open URL", "win.open");
        section->append("File Search", "win.search");

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        Glib::RefPtr<Gio::MenuItem> item;

        item = Gio::MenuItem::create("Terminal", "win.terminal");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("F4"));
        section->append_item(item);

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        Glib::RefPtr<Gio::MenuItem> item;

        item = Gio::MenuItem::create("New Window", "win.new_window");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>N"));
        section->append_item(item);

        section->append("Close Window", "win.close");

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        Glib::RefPtr<Gio::MenuItem> item;

        item = Gio::MenuItem::create("Exit", "win.quit");
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

        item = Gio::MenuItem::create("Panel 1", "win.panel_1");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>1"));
        section->append_item(item);

        item = Gio::MenuItem::create("Panel 2", "win.panel_2");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>2"));
        section->append_item(item);

        item = Gio::MenuItem::create("Panel 3", "win.panel_3");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>3"));
        section->append_item(item);

        item = Gio::MenuItem::create("Panel 4", "win.panel_4");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>4"));
        section->append_item(item);

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        section->append("Focus", "win.todo"); // TODO SUBMENU
        section->append("View", "win.todo");  // TODO SUBMENU

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        section->append("Task Manager", "win.todo"); // TODO SUBMENU

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        section->append("Window Title", "win.title");
        section->append("Fullscreen", "win.fullscreen");

        menu->append_section(section);
    }

    {
        auto section = Gio::Menu::create();
        Glib::RefPtr<Gio::MenuItem> item;

        section->append("Keybindings", "win.keybindings");

        item = Gio::MenuItem::create("Preferences", "win.preferences");
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
        section->append("TODO", "win.todo");
        section->append("TODO", "win.todo");
        section->append("TODO", "win.todo");
        section->append("TODO", "win.todo");

        menu->append_section(section);
    }

    return menu;
}

Glib::RefPtr<Gio::Menu>
gui::menubar::create_bookmarks() noexcept
{
    auto menu = Gio::Menu::create();
    Glib::RefPtr<Gio::MenuItem> item;

    item = Gio::MenuItem::create("Add Bookmark", "win.bookmark_add");
    item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>D"));
    menu->append_item(item);

    item = Gio::MenuItem::create("Open Bookmark Manager", "win.bookmark_manager");
    item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Shift><Control>O"));
    menu->append_item(item);

    return menu;
}

Glib::RefPtr<Gio::Menu>
gui::menubar::create_help() noexcept
{
    auto menu = Gio::Menu::create();
    Glib::RefPtr<Gio::MenuItem> item;

    item = Gio::MenuItem::create("About", "win.about");
    item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("F1"));
    menu->append_item(item);

    menu->append("Donate", "win.donate");

    return menu;
}
