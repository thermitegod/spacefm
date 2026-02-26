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

#include <cassert>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/browser.hxx"
#include "gui/layout.hxx"
#include "gui/main-window.hxx"

#include "gui/dialog/about.hxx"
#include "gui/dialog/bookmarks.hxx"
#include "gui/dialog/donate.hxx"
#include "gui/dialog/preferences.hxx"
#include "gui/dialog/text.hxx"

#include "logger.hxx"

gui::main_window::main_window(const Glib::RefPtr<Gtk::Application>& app)
{
    set_application(app);
    assert(get_application() != nullptr);

    logger::debug("gui::main_window::main_window({})", get_id());

    config_manager_->signal_load_error().connect(
        [this](const std::string& msg)
        {
            auto dialog = Gtk::AlertDialog::create("Config Load Error");
            dialog->set_detail(msg);
            dialog->set_modal(true);
            dialog->show(*this);
        });
    config_manager_->signal_save_error().connect(
        [this](const std::string& msg)
        {
            auto dialog = Gtk::AlertDialog::create("Config Save Error");
            dialog->set_detail(msg);
            dialog->set_modal(true);
            dialog->show(*this);
        });
    config_manager_->load();

    bookmark_manager_->signal_load_error().connect(
        [this](const std::string& msg)
        {
            auto dialog = Gtk::AlertDialog::create("Bookmark Load Error");
            dialog->set_detail(msg);
            dialog->set_modal(true);
            dialog->show(*this);
        });
    bookmark_manager_->signal_save_error().connect(
        [this](const std::string& msg)
        {
            auto dialog = Gtk::AlertDialog::create("Bookmark Save Error");
            dialog->set_detail(msg);
            dialog->set_modal(true);
            dialog->show(*this);
        });
    bookmark_manager_->load();

    set_title(PACKAGE_NAME_FANCY);

    add_shortcuts();

    set_size_request(500, 500);
#if 0 // defined(DEV_MODE)
    // Avoids having the window get tiled and messing up the dev layout
    set_size_request(1000, 1000);
    set_resizable(false);
#endif

    box_.set_orientation(Gtk::Orientation::VERTICAL);
    set_child(box_);

    box_.append(menubar_);
    box_.append(layout_);
    box_.append(tasks_);
    tasks_.set_visible(false);

    app->add_action("open", [this]() { on_open(); });
    app->add_action("search", [this]() { on_open_search(); });
    app->add_action("close", [this]() { on_close(); });
    app->add_action("quit", [this]() { on_quit(); });
    app->add_action("terminal", [this]() { on_open_terminal(); });
    app->add_action("new_window", [this]() { on_open_new_window(); });
    app->add_action("title", [this]() { on_set_title(); });
    app->add_action("fullscreen", [this]() { on_fullscreen(); });
    app->add_action("keybindings", [this]() { on_open_keybindings(); });
    app->add_action("preferences", [this]() { on_open_preferences(); });
    app->add_action("donate", [this]() { on_open_donate(); });
    app->add_action("about", [this]() { on_open_about(); });
    app->add_action("bookmark_manager", [this]() { on_open_bookmark_manager(); });

    const std::vector<std::pair<config::panel_id, Glib::ustring>> panel_map = {
        {config::panel_id::panel_1, "panel_1"},
        {config::panel_id::panel_2, "panel_2"},
        {config::panel_id::panel_3, "panel_3"},
        {config::panel_id::panel_4, "panel_4"}};
    for (const auto& [id, name] : panel_map)
    {
        app->add_action_bool(
            name,
            [this, app, id, name]()
            {
                const bool state = !settings_->window.state[id].is_visible;
                settings_->window.state[id].is_visible = state;
                settings_->signal_autosave_request().emit();

                layout_.set_pane_visible(id, state);

                auto action = app->lookup_action(name);
                auto simple_action = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
                if (simple_action)
                {
                    simple_action->set_state(Glib::Variant<bool>::create(state));
                }
            },
            settings_->window.state[id].is_visible);
    }

    app->add_action("todo",
                    [this]()
                    {
                        auto alert = Gtk::AlertDialog::create("Not Implemented");
                        alert->set_detail("TODO");
                        alert->set_modal(true);
                        alert->show(*this);
                    });

    // Load panels / tabs
    for (const auto [id, state] : settings_->window.state)
    {
        if (state.is_visible)
        {
            layout_.set_pane_visible(id, true);
        }
    }

    set_visible(true);
}

gui::main_window::~main_window()
{
    config_manager_->save();
}

void
gui::main_window::add_shortcuts() noexcept
{
    auto controller = Gtk::ShortcutController::create();

    { // Open Terminal
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_F4);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.terminal"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // New Window
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_n, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.new_window"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Toggle Panel 1
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_1, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.panel_1"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Toggle Panel 2
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_2, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.panel_2"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Toggle Panel 3
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_3, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.panel_3"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Toggle Panel 4
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_4, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.panel_4"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Add Bookmark
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_d, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.bookmark_add"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Bookmark Manager
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_O,
                                                  Gdk::ModifierType::CONTROL_MASK |
                                                      Gdk::ModifierType::SHIFT_MASK);
        auto action =
            Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                        { return activate_action("app.bookmark_manager"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Quit
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_q, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.quit"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Preferences
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_F12);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.preferences"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Fullscreen
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_F11);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.fullscreen"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // About
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_F1);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("app.about"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    add_controller(controller);
}

void
gui::main_window::on_close() noexcept
{
    // TODO only close current window if multiple windows are open, otherwise quit.

    // config_manager_->save();

    close();
}

void
gui::main_window::on_quit() noexcept
{
    // config_manager_->save();

    close();
}

void
gui::main_window::on_open() noexcept
{
    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("gui::main_window::on_open()");
    alert->set_modal(true);
    alert->show(*this);
}

void
gui::main_window::on_open_search() noexcept
{
    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("gui::main_window::on_open_search()");
    alert->set_modal(true);
    alert->show(*this);
}

void
gui::main_window::on_open_terminal() noexcept
{
    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("gui::main_window::on_open_terminal()");
    alert->set_modal(true);
    alert->show(*this);
}

void
gui::main_window::on_open_new_window() noexcept
{
    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("gui::main_window::on_open_new_window()");
    alert->set_modal(true);
    alert->show(*this);
}

void
gui::main_window::on_set_title() noexcept
{
    auto* dialog = Gtk::make_managed<gui::dialog::text>(
        *this,
        "Set Window Title Format",
        "Set window title format:\n\nUse:\n\t%n\tcurrent directory name (eg bin)\n\t%d\tcurrent "
        "directory path (eg /usr/bin)\n\t%t\tcurrent tab number",
        settings_->interface.window_title,
        "%d");
    dialog->signal_confirm().connect(
        [this](const dialog::text_response& response)
        {
            settings_->interface.window_title = response.text;
            on_update_window_title();
        });
}

void
gui::main_window::on_fullscreen() noexcept
{
    if (is_fullscreen())
    {
        fullscreen();
    }
    else
    {
        unfullscreen();
    }
}

void
gui::main_window::on_open_keybindings() noexcept
{
    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("gui::main_window::on_open_keybindings()");
    alert->set_modal(true);
    alert->show(*this);
}

void
gui::main_window::on_open_preferences() noexcept
{
    Gtk::make_managed<gui::dialog::preferences>(*this, settings_);
}

void
gui::main_window::on_open_about() noexcept
{
    Gtk::make_managed<gui::dialog::about>(*this);
}

void
gui::main_window::on_open_donate() noexcept
{
    Gtk::make_managed<gui::dialog::donate>(*this);
}

void
gui::main_window::on_open_bookmark_manager() noexcept
{
    auto* dialog = Gtk::make_managed<gui::dialog::bookmarks>(*this, bookmark_manager_, settings_);
    dialog->signal_confirm().connect(
        [this](const auto& path)
        {
            // TODO choose which panel to open path in, or move this
            // into gui::browser and open in that panel.
            auto* browser = layout_.get_browser(config::panel_id::panel_1);
            if (browser)
            {
                browser->new_tab(path);
            }
        });
}

void
gui::main_window::on_update_window_title() noexcept
{
#if 0
    std::string title = settings_->interface.window_title;
    if (title.empty())
    {
        title = "%d";
    }

    if (title.contains("%n"))
    {
        title = ztd::replace(title, "%n", "TODO");
    }
    if (title.contains("%d"))
    {
        title = ztd::replace(title, "%d", "TODO");
    }

    set_title(title.data());
#else
    set_title(PACKAGE_NAME_FANCY);
#endif
}
