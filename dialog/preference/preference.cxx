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

#include <string_view>

#include <print>

#include <ranges>
#include <limits>

#include <gtkmm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "datatypes.hxx"

#include "preference.hxx"

// TODO
//  - reset

class PreferencePage : public Gtk::Box
{
  public:
    PreferencePage()
    {
        this->set_orientation(Gtk::Orientation::VERTICAL);
        // this->set_spacing(12);
        this->set_margin(6);
        this->set_homogeneous(false);
        this->set_vexpand(true);
    }

    void
    add_section(const std::string_view header) noexcept
    {
        Gtk::Label label;
        label.set_markup(std::format("<b>{}</b>", header.data()));
        label.set_xalign(0.0f);
        this->append(label);
    }

    void
    add_row(const std::string_view left_item_name, Gtk::Widget& right_item) noexcept
    {
        Gtk::Label left_item(left_item_name.data());

        Gtk::Box left_box;
        Gtk::Box right_box;
        this->new_split_vboxes(left_box, right_box);
        left_box.append(left_item);
        right_box.append(right_item);
    }

    void
    add_row(Gtk::Label& left_item, Gtk::Widget& right_item) noexcept
    {
        Gtk::Box left_box;
        Gtk::Box right_box;
        this->new_split_vboxes(left_box, right_box);
        left_box.append(left_item);
        right_box.append(right_item);
    }

    void
    add_row(Gtk::Widget& item) noexcept
    {
        this->append(item);
    }

    void
    add_entry(const std::string_view left_item_name, const std::string_view text,
              const bool selectable = true) noexcept
    {
        Gtk::Label left_item(left_item_name.data());

        Gtk::Entry entry;
        entry.set_text(text.data());
        entry.set_editable(false);
        entry.set_hexpand(true);
        if (!selectable)
        {
            entry.set_can_focus(false);
            entry.set_sensitive(false);
        }

        Gtk::Box left_box;
        Gtk::Box right_box;
        this->new_split_vboxes(left_box, right_box);
        left_box.append(left_item);
        right_box.append(entry);
    }

  private:
    void
    new_split_vboxes(Gtk::Box& left_box, Gtk::Box& right_box) noexcept
    {
        left_box.set_spacing(6);
        left_box.set_homogeneous(false);

        right_box.set_spacing(6);
        right_box.set_homogeneous(false);

        Gtk::Box hbox = Gtk::Box(Gtk::Orientation::HORIZONTAL, 12);
        hbox.append(left_box);
        hbox.append(right_box);
        this->append(hbox);
    }
};

PreferenceDialog::PreferenceDialog(const std::string_view json_data)
{
    const auto data = glz::read_json<datatype::settings_extended>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    this->settings_ = data.value();

    this->set_size_request(470, 400);
    this->set_title("Preferences");
    this->set_resizable(false);

    // Content //

    this->vbox_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->vbox_.set_margin_start(5);
    this->vbox_.set_margin_end(5);
    this->vbox_.set_margin_top(5);
    this->vbox_.set_margin_bottom(5);

    this->vbox_.append(this->notebook_);

    this->init_general_tab();
    this->init_interface_tab();
    this->init_advanced_tab();

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(
        sigc::mem_fun(*this, &PreferenceDialog::on_key_press),
        false);
    this->add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_cancel_ = Gtk::Button("_Cancel", true);
    this->button_reset_ = Gtk::Button("_Reset", true);
    this->button_apply_ = Gtk::Button("_Apply", true);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_cancel_);
    // this->button_box_.append(this->button_reset_); // TODO
    this->button_box_.append(this->button_apply_);
    this->vbox_.append(this->button_box_);

    this->button_apply_.signal_clicked().connect(
        sigc::mem_fun(*this, &PreferenceDialog::on_button_apply_clicked));
    this->button_reset_.signal_clicked().connect(
        sigc::mem_fun(*this, &PreferenceDialog::on_button_reset_clicked));
    this->button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &PreferenceDialog::on_button_cancel_clicked));

    this->set_child(this->vbox_);

    this->set_visible(true);
}

bool
PreferenceDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Escape)
    {
        this->on_button_cancel_clicked();
    }
    return false;
}

void
PreferenceDialog::on_button_apply_clicked()
{
    datatype::settings_extended new_settings{
        .settings =
            {
                .show_thumbnails = this->btn_show_thumbnails_.get_active(),
                .thumbnail_size_limit = this->btn_thumbnail_size_limit_.get_active(),
                .thumbnail_max_size = static_cast<decltype(datatype::settings::thumbnail_max_size)>(
                    this->btn_thumbnail_max_size_.get_value_as_int() * 1024 * 1024),

                .icon_size_big =
                    [this]()
                {
                    auto object = this->icon_size_big_.get_selected_item();
                    if (auto selected = std::dynamic_pointer_cast<ListColumns>(object))
                    {
                        return (std::int32_t)selected->value_;
                    }
                    return (std::int32_t)0; // failed to get value
                }(),
                .icon_size_small =
                    [this]()
                {
                    auto object = this->icon_size_small_.get_selected_item();
                    if (auto selected = std::dynamic_pointer_cast<ListColumns>(object))
                    {
                        return (std::int32_t)selected->value_;
                    }
                    return (std::int32_t)0; // failed to get value
                }(),
                .icon_size_tool =
                    [this]()
                {
                    auto object = this->icon_size_tool_.get_selected_item();
                    if (auto selected = std::dynamic_pointer_cast<ListColumns>(object))
                    {
                        return (std::int32_t)selected->value_;
                    }
                    return (std::int32_t)0; // failed to get value
                }(),

                .click_executes = this->btn_click_executes_.get_active(),

                .confirm = this->btn_confirm_.get_active(),
                .confirm_delete = this->btn_confirm_delete_.get_active(),
                .confirm_trash = this->btn_confirm_trash_.get_active(),

                .load_saved_tabs = false, // UNUSED
                .maximized = false,       // UNUSED

                // Interface
                .always_show_tabs = this->btn_always_show_tabs_.get_active(),
                .show_close_tab_buttons = this->btn_show_close_tab_buttons_.get_active(),
                .new_tab_here = this->btn_new_tab_here_.get_active(),

                .show_toolbar_home = this->btn_show_toolbar_home_.get_active(),
                .show_toolbar_refresh = this->btn_show_toolbar_refresh_.get_active(),
                .show_toolbar_search = this->btn_show_toolbar_search_.get_active(),

                .use_si_prefix = this->btn_use_si_prefix_.get_active(),
            },
        .drag_action =
            [this]()
        {
            auto object = this->drag_action_.get_selected_item();
            if (auto selected = std::dynamic_pointer_cast<ListColumns>(object))
            {
                return (std::int32_t)selected->value_;
            }
            return (std::int32_t)0; // failed to get value
        }(),
        .editor = this->editor_.get_text(),
        .terminal =
            [this]()
        {
            auto object = this->terminal_.get_selected_item();
            if (auto selected = std::dynamic_pointer_cast<ListColumns>(object))
            {
                return selected->entry_;
            }
            return this->settings_.terminal; // failed to get value
        }(),
        .details =
            {
                .supported_terminals = {} // UNUSED
            }};

    const auto buffer = glz::write_json(new_settings);
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
PreferenceDialog::on_button_reset_clicked()
{
    // This does not work
    // this->notebook_.remove_page(0);
    // this->notebook_.remove_page(1);
    // this->notebook_.remove_page(2);
    // this->init_general_tab();
    // this->init_interface_tab();
    // this->init_advanced_tab();

    std::println("TODO reset");
}

void
PreferenceDialog::on_button_cancel_clicked()
{
    this->close();
}

void
PreferenceDialog::setup_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item)
{
    auto* label = Gtk::make_managed<Gtk::Label>();
    list_item->set_child(*label);
}

void
PreferenceDialog::bind_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item)
{
    if (auto* label = dynamic_cast<Gtk::Label*>(list_item->get_child()))
    {
        if (auto info = std::dynamic_pointer_cast<ListColumns>(list_item->get_item()))
        {
            label->set_label(info->entry_);
        }
    }
}

void
PreferenceDialog::init_general_tab() noexcept
{
    auto page = PreferencePage();

    page.add_section("Icons");

    {
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &PreferenceDialog::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &PreferenceDialog::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        store->append(ListColumns::create("22", 22));
        store->append(ListColumns::create("24", 24));
        store->append(ListColumns::create("32", 32));
        store->append(ListColumns::create("36", 36));
        store->append(ListColumns::create("48", 48));
        store->append(ListColumns::create("64", 64));
        store->append(ListColumns::create("72", 72));
        store->append(ListColumns::create("96", 96));
        store->append(ListColumns::create("128", 128));
        store->append(ListColumns::create("192", 192));
        store->append(ListColumns::create("256", 256));
        store->append(ListColumns::create("384", 384));
        store->append(ListColumns::create("512", 512));
        store->append(ListColumns::create("1024", 1024));

        // this takes the icon size and finds the correct
        // position of the matching item in the store
        const auto size_position = [&store](const std::uint32_t size)
        {
            for (std::uint32_t i = 0; i < store->get_n_items(); ++i)
            {
                auto item = store->get_item(i);
                if (item)
                {
                    if (item->value_ == size)
                    {
                        return i;
                    }
                }
            }
            return static_cast<std::uint32_t>(0); // failed
        };

        this->icon_size_big_.set_model(store);
        this->icon_size_big_.set_factory(factory);
        this->icon_size_big_.set_selected(size_position(this->settings_.settings.icon_size_big));

        page.add_row("Large Icons:", this->icon_size_big_);
    }

    {
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &PreferenceDialog::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &PreferenceDialog::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        store->append(ListColumns::create("12", 12));
        store->append(ListColumns::create("16", 16));
        store->append(ListColumns::create("22", 22));
        store->append(ListColumns::create("24", 24));
        store->append(ListColumns::create("32", 32));
        store->append(ListColumns::create("36", 36));
        store->append(ListColumns::create("48", 48));
        store->append(ListColumns::create("64", 64));
        store->append(ListColumns::create("72", 72));
        store->append(ListColumns::create("96", 96));
        store->append(ListColumns::create("128", 128));
        store->append(ListColumns::create("192", 192));
        store->append(ListColumns::create("256", 256));
        store->append(ListColumns::create("384", 384));
        store->append(ListColumns::create("512", 512));

        // this takes the icon size and finds the correct
        // position of the matching item in the store
        const auto size_position = [&store](const std::uint32_t size)
        {
            for (std::uint32_t i = 0; i < store->get_n_items(); ++i)
            {
                auto item = store->get_item(i);
                if (item)
                {
                    if (item->value_ == size)
                    {
                        return i;
                    }
                }
            }
            return static_cast<std::uint32_t>(0); // failed
        };

        this->icon_size_small_.set_model(store);
        this->icon_size_small_.set_factory(factory);
        this->icon_size_small_.set_selected(
            size_position(this->settings_.settings.icon_size_small));

        page.add_row("Small Icons:", this->icon_size_small_);
    }

    {
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &PreferenceDialog::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &PreferenceDialog::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        store->append(ListColumns::create("GTK Default Size", 0));
        store->append(ListColumns::create("Menu", 1));
        store->append(ListColumns::create("Small Toolbar", 2));
        store->append(ListColumns::create("Large Toolbar", 3));
        store->append(ListColumns::create("Button", 4));
        store->append(ListColumns::create("DND", 5));
        store->append(ListColumns::create("Dialog", 6));

        this->icon_size_tool_.set_model(store);
        this->icon_size_tool_.set_factory(factory);
        this->icon_size_tool_.set_selected(this->settings_.settings.icon_size_tool);

        page.add_row("Tool Icons:", this->icon_size_tool_);
    }

    page.add_section("Thumbnails");

    {
        this->btn_show_thumbnails_.set_label("Show Thumbnails");
        this->btn_show_thumbnails_.set_active(this->settings_.settings.show_thumbnails);
        page.add_row(this->btn_show_thumbnails_);
    }

    {
        this->btn_thumbnail_size_limit_.set_label("Thumbnail Size Limits");
        this->btn_thumbnail_size_limit_.set_active(this->settings_.settings.thumbnail_size_limit);
        page.add_row(this->btn_thumbnail_size_limit_);
    }

    {
        const auto value = this->settings_.settings.thumbnail_max_size / (1024 * 1024);

        this->btn_thumbnail_max_size_.set_wrap(false);
        this->btn_thumbnail_max_size_.set_range(0, std::numeric_limits<std::uint32_t>::max());
        this->btn_thumbnail_max_size_.set_increments(1, 10);
        this->btn_thumbnail_max_size_.set_numeric(true);
        this->btn_thumbnail_max_size_.set_value(value);

        page.add_row("Thumbnail Max Image Size", this->btn_thumbnail_max_size_);
    }

    auto tab_label = Gtk::Label("General");
    this->notebook_.append_page(page, tab_label);
}

void
PreferenceDialog::init_interface_tab() noexcept
{
    auto page = PreferencePage();

    page.add_section("Toolbar");

    {
        this->btn_show_toolbar_home_.set_label("Show Home Button");
        this->btn_show_toolbar_home_.set_active(this->settings_.settings.show_toolbar_home);
        page.add_row(this->btn_show_toolbar_home_);
    }

    {
        this->btn_show_toolbar_refresh_.set_label("Show Refresh Button");
        this->btn_show_toolbar_refresh_.set_active(this->settings_.settings.show_toolbar_refresh);
        page.add_row(this->btn_show_toolbar_refresh_);
    }

    {
        this->btn_show_toolbar_search_.set_label("Show Search Bar");
        this->btn_show_toolbar_search_.set_active(this->settings_.settings.show_toolbar_search);
        page.add_row(this->btn_show_toolbar_search_);
    }

    page.add_section("Tabs");

    {
        this->btn_always_show_tabs_.set_label("Always Show The Tab Bar");
        this->btn_always_show_tabs_.set_active(this->settings_.settings.always_show_tabs);
        page.add_row(this->btn_always_show_tabs_);
    }

    {
        this->btn_show_close_tab_buttons_.set_label("Hide 'Close Tab' Buttons");
        this->btn_show_close_tab_buttons_.set_active(
            this->settings_.settings.show_close_tab_buttons);
        page.add_row(this->btn_show_close_tab_buttons_);
    }

    {
        this->btn_new_tab_here_.set_label("Create New Tabs at current location");
        this->btn_new_tab_here_.set_active(this->settings_.settings.new_tab_here);
        page.add_row(this->btn_new_tab_here_);
    }

    page.add_section("Confirming");

    {
        this->btn_confirm_.set_label("Confirm Some Actions");
        this->btn_confirm_.set_active(this->settings_.settings.confirm);
        page.add_row(this->btn_confirm_);
    }

    {
        this->btn_confirm_delete_.set_label("Confirm File Delete");
        this->btn_confirm_delete_.set_active(this->settings_.settings.confirm_delete);
        page.add_row(this->btn_confirm_delete_);
    }

    {
        this->btn_confirm_trash_.set_label("Confirm File Trash");
        this->btn_confirm_trash_.set_active(this->settings_.settings.confirm_trash);
        page.add_row(this->btn_confirm_trash_);
    }

    page.add_section("Unit Sizes");

    {
        this->btn_use_si_prefix_.set_label("SI File Sizes (1k = 1000)");
        this->btn_use_si_prefix_.set_active(this->settings_.settings.use_si_prefix);
        page.add_row(this->btn_use_si_prefix_);
    }

    page.add_section("Other");

    {
        this->btn_click_executes_.set_label("Click Runs Executables");
        this->btn_click_executes_.set_active(this->settings_.settings.click_executes);
        page.add_row(this->btn_click_executes_);
    }

    {
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &PreferenceDialog::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &PreferenceDialog::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        store->append(ListColumns::create("Automatic", 0));
        store->append(ListColumns::create("Copy (Ctrl+Drag)", 1));
        store->append(ListColumns::create("Move (Shift+Drag)", 2));
        store->append(ListColumns::create("Link (Ctrl+Shift+Drag)", 3));

        this->drag_action_.set_model(store);
        this->drag_action_.set_factory(factory);
        this->drag_action_.set_selected(this->settings_.drag_action);

        page.add_row("Default Drag Action:", this->drag_action_);
    }

    auto tab_label = Gtk::Label("Interface");
    this->notebook_.append_page(page, tab_label);
}

void
PreferenceDialog::init_advanced_tab() noexcept
{
    auto page = PreferencePage();

    page.add_section("Terminal");

    {
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &PreferenceDialog::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &PreferenceDialog::bind_listitem));

        std::uint32_t position = 0;
        auto store = Gio::ListStore<ListColumns>::create();
        for (const auto [idx, term] :
             std::ranges::enumerate_view(this->settings_.details.supported_terminals))
        {
            store->append(ListColumns::create(term, idx));
            if (this->settings_.terminal == term)
            {
                position = idx;
            }
        }

        this->terminal_.set_model(store);
        this->terminal_.set_factory(factory);
        this->terminal_.set_selected(position);

        page.add_row("Terminal:", this->terminal_);
    }

    page.add_section("Editor");

    {
        this->editor_.set_text(this->settings_.editor);
        this->editor_.set_editable(true);
        this->editor_.set_hexpand(true);
        page.add_row("Editor", this->editor_);
    }

    auto tab_label = Gtk::Label("Advanced");
    this->notebook_.append_page(page, tab_label);
}
