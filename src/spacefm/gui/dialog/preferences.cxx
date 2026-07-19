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
#include "gui/dialog/widgets/button-box.hxx"

class preference_page : public Gtk::ScrolledWindow
{
  public:
    explicit preference_page() noexcept
    {
        box_.set_orientation(Gtk::Orientation::VERTICAL);
        box_.set_margin(6);
        box_.set_homogeneous(false);
        box_.set_vexpand(true);

        set_child(box_);
    }

    void
    add_section(const std::string_view header) noexcept
    {
        Gtk::Label label;
        label.set_markup(std::format("<b>{}</b>", header.data()));
        label.set_xalign(0.0f);
        box_.append(label);
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
        box_.append(item);
    }

    void
    add_checkbox(const std::string_view label, bool& option) noexcept
    {
        auto* button = Gtk::make_managed<Gtk::CheckButton>(std::format("{}", label));
        button->set_active(option);
        button->set_focus_on_click(false);

        button->signal_toggled().connect([button, &option]() { option = button->get_active(); });

        add_row(*button);
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

        box_.append(hbox);
    }

    Gtk::Box box_;
};

gui::dialog::preferences::preferences(Gtk::ApplicationWindow& parent,
                                      const std::shared_ptr<config::settings>& settings) noexcept
    : settings_(settings)
{
    set_transient_for(parent);
    set_modal(true);

    set_size_request(600, 600);
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

    // Buttons //
    auto* buttons = gui::widget::ButtonBox::create({
        {"Close", [this] { on_button_close_clicked(); }, &button_close_},
    });
    box_.append(*buttons);

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
gui::dialog::preferences::on_setup_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto* label = Gtk::make_managed<Gtk::Label>();
    item->set_child(*label);
}

void
gui::dialog::preferences::on_bind_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    if (auto* label = dynamic_cast<Gtk::Label*>(item->get_child()))
    {
        if (auto info = std::dynamic_pointer_cast<ListColumns>(item->get_item()))
        {
            label->set_label(info->entry_);
        }
    }
}

void
gui::dialog::preferences::init_general_tab() noexcept
{
    auto page = preference_page();

    page.add_checkbox("Click Can Execute", settings_->general.click_executes);
    page.add_checkbox("Single Click Execute", settings_->general.single_click_executes);
    page.add_checkbox("Single Click Activate", settings_->general.single_click_activate);
    page.add_checkbox("Show Confirm Dialog For Some Tasks", settings_->general.confirm);
    page.add_checkbox("Show Confirm Dialog Before Delete", settings_->general.confirm_delete);
    page.add_checkbox("Show Confirm Dialog Before Trash", settings_->general.confirm_trash);
    page.add_checkbox("Auto Open Mounted Volumes", settings_->general.auto_open_mounted_volumes);
    page.add_checkbox("Save Tabs", settings_->general.load_saved_tabs);
    page.add_checkbox("Use SI Units", settings_->general.use_si_prefix);

    auto tab_label = Gtk::Label("General");
    notebook_.append_page(page, tab_label);
}

void
gui::dialog::preferences::init_interface_tab() noexcept
{
    auto page = preference_page();

    page.add_section("Sidebar");

    page.add_checkbox("Show Sidebar", settings_->interface.show_sidebar);

    {
        auto& opt = settings_->interface.sidebar_width;

        auto adjust = Gtk::Adjustment::create(opt, 1, 512);
        adjust->set_step_increment(1);
        adjust->set_page_increment(1);
        adjust->signal_value_changed().connect(
            [&opt, adjust]() { opt = static_cast<std::int32_t>(adjust->get_value()); });

        auto button = Gtk::make_managed<Gtk::SpinButton>();
        button->set_value(opt);
        button->set_adjustment(adjust);

        page.add_row("Sidebar default width", *button);
    }

    page.add_checkbox("Show Config, Data, and Cache", settings_->interface.sidebar_show_advanced);
    page.add_checkbox("Show Desktop", settings_->interface.sidebar_show_desktop);
    page.add_checkbox("Show Documents", settings_->interface.sidebar_show_documents);
    page.add_checkbox("Show Download", settings_->interface.sidebar_show_download);
    page.add_checkbox("Show Music", settings_->interface.sidebar_show_music);
    page.add_checkbox("Show Pictures", settings_->interface.sidebar_show_pictures);
    page.add_checkbox("Show Videos", settings_->interface.sidebar_show_videos);

    page.add_section("Tabs");

    page.add_checkbox("Always Show Tabs", settings_->interface.always_show_tabs);
    page.add_checkbox("Tabs Close Button", settings_->interface.show_tab_close_button);
    page.add_checkbox("New Tab Here", settings_->interface.new_tab_here);

    page.add_section("Toolbar");

    page.add_checkbox("Show Home Button", settings_->interface.show_toolbar_home);
    page.add_checkbox("Show Refresh Button", settings_->interface.show_toolbar_refresh);
    page.add_checkbox("Show Search Bar", settings_->interface.show_toolbar_search);

    auto tab_label = Gtk::Label("Interface");
    notebook_.append_page(page, tab_label);
}

void
gui::dialog::preferences::init_dialog_tab() noexcept
{
    auto page = preference_page();

    page.add_section("Create");

    page.add_checkbox("Filename", settings_->dialog.create.filename);
    page.add_checkbox("Parent", settings_->dialog.create.parent);
    page.add_checkbox("Path", settings_->dialog.create.path);
    page.add_checkbox("Target", settings_->dialog.create.target);
    page.add_checkbox("Confirm", settings_->dialog.create.confirm);

    page.add_section("Rename");

    page.add_checkbox("Copy", settings_->dialog.rename.copy);
    page.add_checkbox("CopyT", settings_->dialog.rename.copyt);
    page.add_checkbox("Filename", settings_->dialog.rename.filename);
    page.add_checkbox("Link", settings_->dialog.rename.link);
    page.add_checkbox("LinkT", settings_->dialog.rename.linkt);
    page.add_checkbox("Parent", settings_->dialog.rename.parent);
    page.add_checkbox("Path", settings_->dialog.rename.path);
    page.add_checkbox("Target", settings_->dialog.rename.target);
    page.add_checkbox("Type", settings_->dialog.rename.type);
    page.add_checkbox("Confirm", settings_->dialog.rename.confirm);

    auto tab_label = Gtk::Label("Dialog");
    notebook_.append_page(page, tab_label);
}

void
gui::dialog::preferences::init_defaults_tab() noexcept
{
    auto page = preference_page();

    page.add_section("Sorting");

    page.add_checkbox("Show Hidden Files", settings_->defaults.sorting.show_hidden);
    page.add_checkbox("Sort Natural", settings_->defaults.sorting.sort_natural);
    page.add_checkbox("Sort Case Sensitive", settings_->defaults.sorting.sort_case);

    {
        auto& opt = settings_->defaults.sorting.sort_by;

        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::on_setup_item));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::on_bind_item));

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
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::on_setup_item));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::on_bind_item));

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
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::on_setup_item));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::on_bind_item));

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
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::on_setup_item));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::on_bind_item));

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
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::on_setup_item));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::on_bind_item));

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
        factory->signal_setup().connect(sigc::mem_fun(*this, &preferences::on_setup_item));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preferences::on_bind_item));

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

    page.add_checkbox("Thumbnails", settings_->defaults.grid.thumbnails);

    page.add_section("List");

    page.add_checkbox("Compact", settings_->defaults.list.compact);
    page.add_checkbox("Name", settings_->defaults.list.name);
    page.add_checkbox("Size", settings_->defaults.list.size);
    page.add_checkbox("Bytes", settings_->defaults.list.bytes);
    page.add_checkbox("Type", settings_->defaults.list.type);
    page.add_checkbox("Mime", settings_->defaults.list.mime);
    page.add_checkbox("Perm", settings_->defaults.list.perm);
    page.add_checkbox("Owner", settings_->defaults.list.owner);
    page.add_checkbox("Group", settings_->defaults.list.group);
    page.add_checkbox("Date Accessed", settings_->defaults.list.atime);
    page.add_checkbox("Date Created", settings_->defaults.list.btime);
    page.add_checkbox("Date Metadata", settings_->defaults.list.ctime);
    page.add_checkbox("Date Modified", settings_->defaults.list.mtime);

    auto tab_label = Gtk::Label("Defaults");
    notebook_.append_page(page, tab_label);
}
