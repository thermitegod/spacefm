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
#include <memory>
#include <utility>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/browser.hxx"

#include "vfs/user-dirs.hxx"

#include "logger.hxx"

gui::browser::browser(Gtk::ApplicationWindow& parent, config::panel_id panel,
                      const std::shared_ptr<config::settings>& settings)
    : parent_(parent), panel_(panel), settings_(settings)
{
    logger::debug("gui::browser::browser({})", std::to_underlying(panel_));

    action_group_ = Gio::SimpleActionGroup::create();
    action_close_ = action_group_->add_action("close", [this]() { close_tab(); });
    action_restore_ = action_group_->add_action("restore", [this]() { restore_tab(); });
    action_restore_->set_enabled(false);
    action_tab_ = action_group_->add_action("new_tab", [this]() { new_tab(vfs::user::home()); });
    action_tab_here_ = action_group_->add_action("new_tab_here", [this]() { new_tab_here(); });
    insert_action_group("browser", action_group_);

    add_shortcuts();
    set_visible(true);

    // load saved tabs, do this before connecting to notebook signals
    if (settings_->window.state[panel].tabs.empty())
    {
        new_tab(vfs::user::home());
    }
    else
    {
        for (const auto& tab : settings_->window.state[panel].tabs)
        {
            if (std::filesystem::exists(tab.path))
            {
                new_tab(tab.path, tab.sorting);
                set_current_page(settings_->window.state[panel].active_tab);
            }
        }
    }

    signal_page_added_ = signal_page_added().connect([this](auto, auto) { save_tab_state(); });
    signal_page_removed_ = signal_page_removed().connect([this](auto, auto) { save_tab_state(); });
    signal_page_reordered_ =
        signal_page_reordered().connect([this](auto, auto) { save_tab_state(); });
    signal_switch_page_ = signal_switch_page().connect([this](auto, auto) { save_tab_state(); });
}

gui::browser::~browser()
{
    logger::debug("gui::browser::~browser({})", std::to_underlying(panel_));

    signal_page_added_.disconnect();
    signal_page_removed_.disconnect();
    signal_page_reordered_.disconnect();
    signal_switch_page_.disconnect();
}

void
gui::browser::add_shortcuts() noexcept
{
    auto controller = Gtk::ShortcutController::create();

    { // Open Tab
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_T, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create(
            [this](Gtk::Widget&, const Glib::VariantBase&)
            {
                if (settings_->interface.new_tab_here)
                {
                    new_tab_here();
                }
                else
                {
                    new_tab(vfs::user::home());
                }
                set_current_page(get_n_pages() - 1);
                return true;
            });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Restore Tab
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_T,
                                                  Gdk::ModifierType::CONTROL_MASK |
                                                      Gdk::ModifierType::SHIFT_MASK);
        auto action = Gtk::CallbackAction::create(
            [this](Gtk::Widget&, const Glib::VariantBase&)
            {
                restore_tab();
                set_current_page(get_n_pages() - 1);
                return true;
            });

        controller->add_shortcut(Gtk::Shortcut::create(trigger, action));
    }

    { // Close Tab
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_w, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create(
            [this](Gtk::Widget&, const Glib::VariantBase&)
            {
                close_tab();
                return true;
            });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Next Tab
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Tab, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create(
            [this](Gtk::Widget&, const Glib::VariantBase&)
            {
                next_page();
                return true;
            });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Previous Tab
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Tab,
                                                  Gdk::ModifierType::CONTROL_MASK |
                                                      Gdk::ModifierType::SHIFT_MASK);
        auto action = Gtk::CallbackAction::create(
            [this](Gtk::Widget&, const Glib::VariantBase&)
            {
                prev_page();
                return true;
            });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // First Tab
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Home, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create(
            [this](Gtk::Widget&, const Glib::VariantBase&)
            {
                set_current_page(0);
                return true;
            });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Last Tab
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_End, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create(
            [this](Gtk::Widget&, const Glib::VariantBase&)
            {
                set_current_page(get_n_pages() - 1);
                return true;
            });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Switch Tab
        for (std::uint32_t i = 0; i < 9; ++i)
        {
            auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_1 + i, Gdk::ModifierType::ALT_MASK);

            auto action = Gtk::CallbackAction::create(
                [this, i](Gtk::Widget&, const Glib::VariantBase&)
                {
                    set_current_page(static_cast<std::int32_t>(i));
                    return true;
                });

            controller->add_shortcut(Gtk::Shortcut::create(trigger, action));
        }
    }

    add_controller(controller);
}

gui::tab*
gui::browser::current_tab() noexcept
{
    auto current_page = get_current_page();
    auto* tab = dynamic_cast<gui::tab*>(get_nth_page(current_page));

    return tab;
}

bool
gui::browser::set_active_tab(std::int32_t tab) noexcept
{
    const auto pages = get_n_pages();
    if (tab > pages)
    {
        return false;
    }
    set_current_page(tab);
    return true;
}

std::string
gui::browser::display_filename(const std::filesystem::path& path) noexcept
{
    if (path.string() == "/")
    {
        // special case, using std::filesystem::path::filename() on the root
        // directory returns an empty string.
        return "/";
    }
    else
    {
        return path.filename().string();
    }
}

void
gui::browser::new_tab(const std::filesystem::path& path) noexcept
{
    new_tab(path, settings_->default_sorting);
}

void
gui::browser::new_tab(const std::filesystem::path& path, const config::sorting& sorting) noexcept
{
    auto* label = Gtk::make_managed<Gtk::Label>(display_filename(path));
    auto* tab = Gtk::make_managed<gui::tab>(parent_, path, sorting, settings_);
    tab->signal_sorting_changed().connect([this]() { save_tab_state(); });
    tab->signal_chdir_after().connect(
        [this, tab, label]()
        {
            label->set_label(display_filename(tab->cwd()));
            label->set_tooltip_text(tab->cwd().string());

            save_tab_state();
        });
    tab->signal_close_tab().connect([this]() { close_tab(); });
    tab->signal_new_tab().connect([this](const std::filesystem::path& path) { new_tab(path); });
    tab->signal_switch_tab_with_paste().connect(
        [this](std::int32_t tab)
        {
            const auto switched = set_active_tab(tab);
            if (switched)
            {
                current_tab()->on_paste();
            }
            else
            {
                auto alert = Gtk::AlertDialog::create("Tab Switch Failed");
                alert->set_detail(std::format("Failed to change to tab {}", tab));
                alert->set_modal(true);
                alert->show(parent_);
            }
        });

    auto menu = Gio::Menu::create();
    Glib::RefPtr<Gio::MenuItem> item;

    item = Gio::MenuItem::create("Close", "browser.close");
    item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>W"));
    menu->append_item(item);

    item = Gio::MenuItem::create("Restore", "browser.restore");
    item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Shift><Control>T"));
    menu->append_item(item);

    if (settings_->interface.new_tab_here)
    {
        menu->append("Tab", "browser.new_tab");

        item = Gio::MenuItem::create("Tab Here", "browser.new_tab_here");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>T"));
        menu->append_item(item);
    }
    else
    {
        item = Gio::MenuItem::create("Tab", "browser.new_tab");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>T"));
        menu->append_item(item);

        menu->append("Tab Here", "browser.new_tab_here");
    }

    auto* popover = Gtk::make_managed<Gtk::PopoverMenu>(menu);
    popover->set_parent(*label);
    popover->set_has_arrow(false);
    popover->set_flags(Gtk::PopoverMenu::Flags::NESTED);

    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_SECONDARY);
    gesture->signal_pressed().connect(
        [this, popover](std::int32_t, double x, double y)
        {
            action_restore_->set_enabled(!restore_tabs_.empty());

            popover->set_pointing_to(
                Gdk::Rectangle{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), 0, 0});
            popover->popup();
        });
    label->add_controller(gesture);

    append_page(*tab, *label);
    set_tab_reorderable(*tab, true);
}

void
gui::browser::new_tab_here() noexcept
{
    gui::browser::new_tab(current_tab()->cwd());
}

void
gui::browser::close_tab() noexcept
{
    const auto* tab = current_tab();
    restore_tabs_.push({tab->get_sorting_settings(), tab->cwd()});

    if (get_n_pages() == 1)
    {
        current_tab()->chdir(vfs::user::home());
    }
    else
    {
        remove_page(get_current_page());
    }
}

void
gui::browser::restore_tab() noexcept
{
    if (!restore_tabs_.empty())
    {
        const auto state = restore_tabs_.back();
        restore_tabs_.pop();

        new_tab(state.path, state.sorting);
    }
}

void
gui::browser::open_in_tab(const std::filesystem::path& path, std::int32_t tab) noexcept
{
    const auto switched = set_active_tab(tab);
    if (switched)
    {
        gui::browser::current_tab()->chdir(path);
    }
}

void
gui::browser::freeze_state() noexcept
{
    state_frozen_ = true;
}

void
gui::browser::unfreeze_state() noexcept
{
    state_frozen_ = false;
}

void
gui::browser::save_tab_state() noexcept
{
    if (state_frozen_)
    {
        return;
    }

    const auto n_tabs = get_n_pages();
    const auto current_page = get_current_page();

    std::vector<config::tab_state> tabs;
    tabs.reserve(static_cast<std::size_t>(n_tabs));

    for (std::int32_t i = 0; i < n_tabs; ++i)
    {
        auto* tab = dynamic_cast<gui::tab*>(get_nth_page(i));
        tabs.push_back({tab->get_sorting_settings(), tab->cwd()});
    }

    settings_->window.state[panel_].tabs = tabs;
    settings_->window.state[panel_].active_tab = current_page;
    settings_->signal_autosave_request().emit();
}
