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
#include <utility>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/dialog/preferences.hxx"

class preference_page : public Gtk::Box
{
  public:
    explicit preference_page() noexcept
    {
        set_orientation(Gtk::Orientation::VERTICAL);
        set_margin(6);
        set_homogeneous(false);
        set_vexpand(true);
    }

    void
    add_section(const std::string_view header) noexcept
    {
        Gtk::Label label;
        label.set_markup(std::format("<b>{}</b>", header.data()));
        label.set_xalign(0.0f);
        append(label);
    }

    void
    add_row(const std::string_view left_item_name, Gtk::Widget& right_item) noexcept
    {
        Gtk::Label left_item(left_item_name.data());

        Gtk::Box left_box;
        Gtk::Box right_box;
        new_split_vboxes(left_box, right_box);
        left_box.append(left_item);
        right_box.append(right_item);
    }

    void
    add_row(Gtk::Label& left_item, Gtk::Widget& right_item) noexcept
    {
        Gtk::Box left_box;
        Gtk::Box right_box;
        new_split_vboxes(left_box, right_box);
        left_box.append(left_item);
        right_box.append(right_item);
    }

    void
    add_row(Gtk::Widget& item) noexcept
    {
        append(item);
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
        append(hbox);
    }
};

gui::dialog::preferences::preferences(Gtk::ApplicationWindow& parent,
                                      const std::shared_ptr<config::settings>& settings) noexcept
    : settings_(settings)
{
    set_transient_for(parent);
    set_modal(true);

    set_size_request(470, 400);
    set_title("Preferences");
    set_resizable(false);

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.set_margin(5);

    box_.append(notebook_);

    init_general_tab();
    init_interface_tab();
    init_dialog_tab();
    init_defaults_tab();

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(
        sigc::mem_fun(*this, &gui::dialog::preferences::on_key_press),
        false);
    add_controller(key_controller);

    button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_close_ = Gtk::Button("Close", true);
    button_close_.signal_clicked().connect([this]() { on_button_close_clicked(); });
    button_box_.set_halign(Gtk::Align::END);
    button_box_.append(button_close_);
    box_.append(button_box_);

    set_child(box_);

    set_visible(true);
}

bool
gui::dialog::preferences::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
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
gui::dialog::preferences::on_button_close_clicked() noexcept
{
    close();
}

void
gui::dialog::preferences::setup_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept
{
    auto* label = Gtk::make_managed<Gtk::Label>();
    list_item->set_child(*label);
}

void
gui::dialog::preferences::bind_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept
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
gui::dialog::preferences::init_general_tab() noexcept
{
    auto page = preference_page();

    {
        auto& opt = settings_->general.click_executes;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Click Can Execute");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->general.single_click_executes;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Single Click Execute");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->general.single_click_activate;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Single Click Activate");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->general.confirm;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Show Confirm Dialog For Some Tasks");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->general.confirm_delete;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Show Confirm Dialog Before Delete");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->general.confirm_trash;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Show Confirm Dialog Before Trash");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->general.auto_open_mounted_volumes;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Auto Open Mounted Volumes");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->general.load_saved_tabs;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Save Tabs");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->general.use_si_prefix;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Use SI Units");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    auto tab_label = Gtk::Label("General");
    notebook_.append_page(page, tab_label);
}

void
gui::dialog::preferences::init_interface_tab() noexcept
{
    auto page = preference_page();

    page.add_section("Tabs");

    {
        auto& opt = settings_->interface.always_show_tabs;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Always Show Tabs");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->interface.show_tab_close_button;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Tabs Close Button");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->interface.new_tab_here;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("New Tab Here");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    page.add_section("Toolbar");

    {
        auto& opt = settings_->interface.show_toolbar_home;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Show Home Button");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->interface.show_toolbar_refresh;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Show Refresh Button");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->interface.show_toolbar_search;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Show Search Bar");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    auto tab_label = Gtk::Label("Interface");
    notebook_.append_page(page, tab_label);
}

void
gui::dialog::preferences::init_dialog_tab() noexcept
{
    auto page = preference_page();

    page.add_section("Create");

    {
        auto& opt = settings_->dialog.create.filename;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Filename");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.create.parent;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Parent");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.create.path;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Path");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.create.target;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Target");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.create.confirm;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Confirm");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    page.add_section("Rename");

    {
        auto& opt = settings_->dialog.rename.copy;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Copy");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.rename.copyt;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("CopyT");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.rename.filename;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Filename");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.rename.link;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Link");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.rename.linkt;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("LinkT");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.rename.parent;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Parent");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.rename.path;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Path");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.rename.target;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Target");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.rename.type;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Type");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->dialog.rename.confirm;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Confirm");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    auto tab_label = Gtk::Label("Dialog");
    notebook_.append_page(page, tab_label);
}

void
gui::dialog::preferences::init_defaults_tab() noexcept
{
    auto page = preference_page();

    page.add_section("Sorting");

    {
        auto& opt = settings_->defaults.sorting.show_hidden;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Show Hidden Files");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.sorting.sort_natural;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Sort Natural");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.sorting.sort_case;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Sort Case Sensitive");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.sorting.sort_by;

        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        // clang-format off
        store->append(ListColumns::create("Name", std::to_underlying(config::sort_by::name)));
        store->append(ListColumns::create("Size", std::to_underlying(config::sort_by::size)));
        store->append(ListColumns::create("Bytes", std::to_underlying(config::sort_by::bytes)));
        store->append(ListColumns::create("Type", std::to_underlying(config::sort_by::type)));
        store->append(ListColumns::create("MIME Type", std::to_underlying(config::sort_by::mime)));
        store->append(ListColumns::create("Permissions", std::to_underlying(config::sort_by::perm)));
        store->append(ListColumns::create("Owner", std::to_underlying(config::sort_by::owner)));
        store->append(ListColumns::create("Group", std::to_underlying(config::sort_by::group)));
        store->append(ListColumns::create("Date Accessed", std::to_underlying(config::sort_by::atime)));
        store->append(ListColumns::create("Date Created", std::to_underlying(config::sort_by::btime)));
        store->append(ListColumns::create("Date Metadata", std::to_underlying(config::sort_by::ctime)));
        store->append(ListColumns::create("Date Modified", std::to_underlying(config::sort_by::mtime)));
        // clang-format on

        auto drop = Gtk::make_managed<Gtk::DropDown>();
        drop->set_model(store);
        drop->set_factory(factory);
        drop->set_selected(std::to_underlying(opt));

        drop->property_selected_item().signal_changed().connect(
            [&opt, drop]() { opt = static_cast<config::sort_by>(drop->get_selected()); });

        page.add_row("Sort By", *drop);
    }

    {
        auto& opt = settings_->defaults.sorting.sort_dir;

        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        // clang-format off
        store->append(ListColumns::create("Directories First", std::to_underlying(config::sort_dir::first)));
        store->append(ListColumns::create("Files First", std::to_underlying(config::sort_dir::mixed)));
        store->append(ListColumns::create("Mixed", std::to_underlying(config::sort_dir::last)));
        // clang-format on

        auto drop = Gtk::make_managed<Gtk::DropDown>();
        drop->set_model(store);
        drop->set_factory(factory);
        drop->set_selected(std::to_underlying(opt));

        drop->property_selected_item().signal_changed().connect(
            [&opt, drop]() { opt = static_cast<config::sort_dir>(drop->get_selected()); });

        page.add_row("Sort Directories", *drop);
    }

    {
        auto& opt = settings_->defaults.sorting.sort_type;

        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        // clang-format off
        store->append(ListColumns::create("Ascending", std::to_underlying(config::sort_type::ascending)));
        store->append(ListColumns::create("Descending", std::to_underlying(config::sort_type::descending)));
        // clang-format on

        auto drop = Gtk::make_managed<Gtk::DropDown>();
        drop->set_model(store);
        drop->set_factory(factory);
        drop->set_selected(std::to_underlying(opt));

        drop->property_selected_item().signal_changed().connect(
            [&opt, drop]() { opt = static_cast<config::sort_type>(drop->get_selected()); });

        page.add_row("Sort Type", *drop);
    }

    {
        auto& opt = settings_->defaults.sorting.sort_hidden;

        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        // clang-format off
        store->append(ListColumns::create("First", std::to_underlying(config::sort_hidden::first)));
        store->append(ListColumns::create("Last", std::to_underlying(config::sort_hidden::last)));
        // clang-format on

        auto drop = Gtk::make_managed<Gtk::DropDown>();
        drop->set_model(store);
        drop->set_factory(factory);
        drop->set_selected(std::to_underlying(opt));

        drop->property_selected_item().signal_changed().connect(
            [&opt, drop]() { opt = static_cast<config::sort_hidden>(drop->get_selected()); });

        page.add_row("Sort Hidden", *drop);
    }

    page.add_section("View");

    {
        auto& opt = settings_->defaults.view;

        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        // clang-format off
        store->append(ListColumns::create("Grid", std::to_underlying(config::view_mode::grid)));
        store->append(ListColumns::create("List", std::to_underlying(config::view_mode::list)));
        // clang-format on

        auto drop = Gtk::make_managed<Gtk::DropDown>();
        drop->set_model(store);
        drop->set_factory(factory);
        drop->set_selected(std::to_underlying(opt));

        drop->property_selected_item().signal_changed().connect(
            [&opt, drop]() { opt = static_cast<config::view_mode>(drop->get_selected()); });

        page.add_row("Type", *drop);
    }

    page.add_section("Grid");

    {
        auto& opt = settings_->defaults.grid.icon_size;

        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::setup_listitem));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::bind_listitem));

        auto store = Gio::ListStore<ListColumns>::create();
        // clang-format off
        store->append(ListColumns::create("XXX Small Icons", std::to_underlying(config::icon_size::xxx_small)));
        store->append(ListColumns::create("XX Small Icons", std::to_underlying(config::icon_size::xx_small)));
        store->append(ListColumns::create("X Small Icons", std::to_underlying(config::icon_size::x_small)));
        store->append(ListColumns::create("Small Icons", std::to_underlying(config::icon_size::small)));
        store->append(ListColumns::create("Normal Icons", std::to_underlying(config::icon_size::normal)));
        store->append(ListColumns::create("Large Icons", std::to_underlying(config::icon_size::large)));
        store->append(ListColumns::create("X Large Icons", std::to_underlying(config::icon_size::x_large)));
        store->append(ListColumns::create("XX Large Icons", std::to_underlying(config::icon_size::xx_large)));
        store->append(ListColumns::create("XXX Large Icons", std::to_underlying(config::icon_size::xxx_large)));
        // clang-format on

        std::uint32_t index = 0;
        for (std::uint32_t i = 0; i < store->get_n_items(); ++i)
        {
            const auto item = store->get_item(i);
            if (item->value_ == std::to_underlying(opt))
            {
                index = i;
                break;
            }
        }

        auto drop = Gtk::make_managed<Gtk::DropDown>();
        drop->set_model(store);
        drop->set_factory(factory);
        drop->set_selected(index);

        drop->property_selected_item().signal_changed().connect(
            [&opt, drop]() { opt = static_cast<config::icon_size>(drop->get_selected()); });

        page.add_row("View Mode", *drop);
    }

    {
        auto& opt = settings_->defaults.grid.thumbnails;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Thumbnails");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    page.add_section("List");

    {
        auto& opt = settings_->defaults.list.compact;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Compact");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.name;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Name");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.size;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Size");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.bytes;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Bytes");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.type;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Type");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.mime;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Mime Type");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.perm;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Permissions");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.owner;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Owner");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.group;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Group");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.atime;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Date Accessed");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.btime;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Date Created");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.ctime;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Date Metadata");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    {
        auto& opt = settings_->defaults.list.mtime;

        auto button = Gtk::make_managed<Gtk::CheckButton>();
        button->set_label("Date Modified");
        button->set_active(opt);
        button->signal_toggled().connect([&opt]() { opt = !opt; });

        page.add_row(*button);
    }

    auto tab_label = Gtk::Label("Defaults");
    notebook_.append_page(page, tab_label);
}
