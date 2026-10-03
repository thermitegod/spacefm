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

#include <array>
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
    add_section(std::string_view header) noexcept
    {
        auto* label = Gtk::make_managed<Gtk::Label>();
        label->set_markup(std::format("<b>{}</b>", header));
        label->set_xalign(0.0f);
        box_.append(*label);
    }

    void
    add_row(std::string_view left_item_name, Gtk::Widget& right_item) noexcept
    {
        auto* left_item = Gtk::make_managed<Gtk::Label>(std::string(left_item_name));

        auto [left_box, right_box] = create_split_vboxes();
        left_box->append(*left_item);
        right_box->append(right_item);
    }

    void
    add_row(Gtk::Widget& left_item, Gtk::Widget& right_item) noexcept
    {
        auto [left_box, right_box] = create_split_vboxes();
        left_box->append(left_item);
        right_box->append(right_item);
    }

    void
    add_row(Gtk::Widget& item) noexcept
    {
        box_.append(item);
    }

    void
    add_checkbox(std::string_view label, bool& option) noexcept
    {
        auto* button = Gtk::make_managed<Gtk::CheckButton>(std::string(label));
        button->set_active(option);
        button->set_focus_on_click(false);

        button->signal_toggled().connect([button, &option]() { option = button->get_active(); });

        add_row(*button);
    }

    template<typename Enum, std::size_t N>
    void
    add_dropdown(std::string_view label,
                 const std::array<std::pair<Enum, std::string_view>, N>& data, Enum& opt) noexcept
    {
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(sigc::mem_fun(*this, &preference_page::on_setup_item));
        factory->signal_bind().connect(sigc::mem_fun(*this, &preference_page::on_bind_item));

        auto store = Gio::ListStore<ListColumns>::create();
        for (const auto& [value, label] : data)
        {
            store->append(
                ListColumns::create(label, static_cast<std::uint32_t>(std::to_underlying(value))));
        }

        auto it = std::ranges::find_if(data, [opt](const auto& pair) { return pair.first == opt; });
        const auto index = it != data.end() ? std::distance(data.begin(), it) : 0;

        auto drop = Gtk::make_managed<Gtk::DropDown>();
        drop->set_model(store);
        drop->set_factory(factory);
        drop->set_selected(static_cast<std::uint32_t>(index));

        drop->property_selected_item().signal_changed().connect(
            [&opt, data, drop]() { opt = data[drop->get_selected()].first; });

        add_row(label, *drop);
    }

  private:
    class ListColumns : public Glib::Object
    {
      public:
        std::string entry_;
        std::uint32_t value_;

        static Glib::RefPtr<ListColumns>
        create(std::string_view entry, const std::uint32_t value) noexcept
        {
            return Glib::make_refptr_for_instance<ListColumns>(new ListColumns(entry, value));
        }

      protected:
        explicit ListColumns(std::string_view entry, const std::uint32_t value) noexcept
            : entry_(entry), value_(value)
        {
        }
    };

    void
    on_setup_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
    {
        auto* label = Gtk::make_managed<Gtk::Label>();
        item->set_child(*label);
    }

    void
    on_bind_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
    {
        if (auto* label = dynamic_cast<Gtk::Label*>(item->get_child()))
        {
            if (auto info = std::dynamic_pointer_cast<ListColumns>(item->get_item()))
            {
                label->set_label(info->entry_);
            }
        }
    }

    std::array<Gtk::Box*, 2>
    create_split_vboxes() noexcept
    {
        auto* left_box = Gtk::make_managed<Gtk::Box>();
        left_box->set_spacing(6);
        left_box->set_homogeneous(false);

        auto* right_box = Gtk::make_managed<Gtk::Box>();
        right_box->set_spacing(6);
        right_box->set_homogeneous(false);

        auto* hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
        hbox->append(*left_box);
        hbox->append(*right_box);

        box_.append(*hbox);

        return {left_box, right_box};
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
    init_behavior_tab();
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
gui::dialog::preferences::init_general_tab() noexcept
{
    auto* page = Gtk::make_managed<preference_page>();
    notebook_.append_page(*page, "General");

    page->add_checkbox("Click Can Execute", settings_->general.click_executes);
    page->add_checkbox("Single Click Execute", settings_->general.single_click_executes);
    page->add_checkbox("Single Click Activate", settings_->general.single_click_activate);
    page->add_checkbox("Show Confirm Dialog For Some Tasks", settings_->general.confirm);
    page->add_checkbox("Show Confirm Dialog Before Delete", settings_->general.confirm_delete);
    page->add_checkbox("Show Confirm Dialog Before Trash", settings_->general.confirm_trash);
    page->add_checkbox("Auto Open Mounted Volumes", settings_->general.auto_open_mounted_volumes);
    page->add_checkbox("Save Tabs", settings_->general.load_saved_tabs);
    page->add_checkbox("Use SI Units", settings_->general.use_si_prefix);
}

void
gui::dialog::preferences::init_interface_tab() noexcept
{
    auto* page = Gtk::make_managed<preference_page>();
    notebook_.append_page(*page, "Interface");

    page->add_section("Sidebar");

    page->add_checkbox("Show Sidebar", settings_->interface.show_sidebar);

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

        page->add_row("Sidebar default width", *button);
    }

    page->add_checkbox("Show Config, Data, and Cache", settings_->interface.sidebar_show_advanced);
    page->add_checkbox("Show Desktop", settings_->interface.sidebar_show_desktop);
    page->add_checkbox("Show Documents", settings_->interface.sidebar_show_documents);
    page->add_checkbox("Show Download", settings_->interface.sidebar_show_download);
    page->add_checkbox("Show Music", settings_->interface.sidebar_show_music);
    page->add_checkbox("Show Pictures", settings_->interface.sidebar_show_pictures);
    page->add_checkbox("Show Videos", settings_->interface.sidebar_show_videos);

    page->add_section("Tabs");

    page->add_checkbox("Always Show Tabs", settings_->interface.always_show_tabs);
    page->add_checkbox("Tabs Close Button", settings_->interface.show_tab_close_button);
    page->add_checkbox("New Tab Here", settings_->interface.new_tab_here);

    page->add_section("Toolbar");

    page->add_checkbox("Show Home Button", settings_->interface.show_toolbar_home);
    page->add_checkbox("Show Refresh Button", settings_->interface.show_toolbar_refresh);
    page->add_checkbox("Show Search Bar", settings_->interface.show_toolbar_search);
}

void
gui::dialog::preferences::init_behavior_tab() noexcept
{
    auto* page = Gtk::make_managed<preference_page>();
    notebook_.append_page(*page, "Behavior");

    page->add_section("Tab Switching");

    page->add_checkbox("Always Switch To A New Tabs", settings_->behavior.switch_to_new_tabs);
    page->add_checkbox("Always Switch To A Restored Tabs",
                       settings_->behavior.switch_to_restored_tabs);
}

void
gui::dialog::preferences::init_dialog_tab() noexcept
{
    auto* page = Gtk::make_managed<preference_page>();
    notebook_.append_page(*page, "Dialog");

    page->add_section("Create");

    page->add_checkbox("Filename", settings_->dialog.create.filename);
    page->add_checkbox("Parent", settings_->dialog.create.parent);
    page->add_checkbox("Path", settings_->dialog.create.path);
    page->add_checkbox("Target", settings_->dialog.create.target);
    page->add_checkbox("Confirm", settings_->dialog.create.confirm);

    page->add_section("Rename");

    page->add_checkbox("Copy", settings_->dialog.rename.copy);
    page->add_checkbox("CopyT", settings_->dialog.rename.copyt);
    page->add_checkbox("Filename", settings_->dialog.rename.filename);
    page->add_checkbox("Link", settings_->dialog.rename.link);
    page->add_checkbox("LinkT", settings_->dialog.rename.linkt);
    page->add_checkbox("Parent", settings_->dialog.rename.parent);
    page->add_checkbox("Path", settings_->dialog.rename.path);
    page->add_checkbox("Target", settings_->dialog.rename.target);
    page->add_checkbox("Type", settings_->dialog.rename.type);
    page->add_checkbox("Confirm", settings_->dialog.rename.confirm);

    page->add_section("Properties Checksums");

    page->add_checkbox("MD5", settings_->dialog.properties.hash_md5);
    page->add_checkbox("SHA-1", settings_->dialog.properties.hash_sha1);
    page->add_checkbox("SHA-256", settings_->dialog.properties.hash_sha256);
    page->add_checkbox("SHA-512", settings_->dialog.properties.hash_sha512);
    page->add_checkbox("SHA-3(256)", settings_->dialog.properties.hash_sha_3_256);
    page->add_checkbox("SHA-3(512)", settings_->dialog.properties.hash_sha_3_512);
    page->add_checkbox("BLAKE2b(256)", settings_->dialog.properties.hash_blake2b_256);
    page->add_checkbox("BLAKE2b(512)", settings_->dialog.properties.hash_blake2b_512);
    page->add_checkbox("Whirlpool", settings_->dialog.properties.hash_whirlpool);
    page->add_checkbox("CRC32", settings_->dialog.properties.hash_crc32);
}

void
gui::dialog::preferences::init_defaults_tab() noexcept
{
    auto* page = Gtk::make_managed<preference_page>();
    notebook_.append_page(*page, "Defaults");

    page->add_section("Sorting");

    page->add_checkbox("Show Hidden Files", settings_->defaults.sorting.show_hidden);
    page->add_checkbox("Sort Natural", settings_->defaults.sorting.sort_natural);
    page->add_checkbox("Sort Case Sensitive", settings_->defaults.sorting.sort_case);

    {
        constexpr std::array<std::pair<config::sort_by, std::string_view>, 12> data = {{
            {config::sort_by::name, "Name"},
            {config::sort_by::size, "Size"},
            {config::sort_by::bytes, "Bytes"},
            {config::sort_by::type, "Type"},
            {config::sort_by::mime, "MIME Type"},
            {config::sort_by::perm, "Permissions"},
            {config::sort_by::owner, "Owner"},
            {config::sort_by::group, "Group"},
            {config::sort_by::atime, "Date Accessed"},
            {config::sort_by::btime, "Date Created"},
            {config::sort_by::ctime, "Date Metadata"},
            {config::sort_by::mtime, "Date Modified"},
        }};
        page->add_dropdown("Sort By", data, settings_->defaults.sorting.sort_by);
    }

    {
        constexpr std::array<std::pair<config::sort_dir, std::string_view>, 3> data = {{
            {config::sort_dir::first, "Directories First"},
            {config::sort_dir::mixed, "Files First"},
            {config::sort_dir::last, "Mixed"},
        }};
        page->add_dropdown("Sort Directories", data, settings_->defaults.sorting.sort_dir);
    }

    {
        constexpr std::array<std::pair<config::sort_type, std::string_view>, 2> data = {{
            {config::sort_type::ascending, "Ascending"},
            {config::sort_type::descending, "Descending"},
        }};
        page->add_dropdown("Sort Type", data, settings_->defaults.sorting.sort_type);
    }

    {
        constexpr std::array<std::pair<config::sort_hidden, std::string_view>, 2> data = {{
            {config::sort_hidden::first, "First"},
            {config::sort_hidden::last, "Last"},
        }};
        page->add_dropdown("Sort Hidden", data, settings_->defaults.sorting.sort_hidden);
    }

    page->add_section("View");

    {
        constexpr std::array<std::pair<config::view_mode, std::string_view>, 2> data = {{
            {config::view_mode::grid, "Grid"},
            {config::view_mode::list, "List"},
        }};
        page->add_dropdown("View Mode", data, settings_->defaults.view);
    }

    page->add_section("Grid");

    {
        constexpr std::array<std::pair<config::icon_size, std::string_view>, 9> data = {{
            {config::icon_size::xxx_small, "XXX Small Icons"},
            {config::icon_size::xx_small, "XX Small Icons"},
            {config::icon_size::x_small, "X Small Icons"},
            {config::icon_size::small, "Small Icons"},
            {config::icon_size::normal, "Normal Icons"},
            {config::icon_size::large, "Large Icons"},
            {config::icon_size::x_large, "X Large Icons"},
            {config::icon_size::xx_large, "XX Large Icons"},
            {config::icon_size::xxx_large, "XXX Large Icons"},
        }};
        page->add_dropdown("Icon Size", data, settings_->defaults.grid.icon_size);
    }

    page->add_checkbox("Thumbnails", settings_->defaults.grid.thumbnails);

    page->add_section("List");

    page->add_checkbox("Compact", settings_->defaults.list.compact);
    page->add_checkbox("Name", settings_->defaults.list.name);
    page->add_checkbox("Size", settings_->defaults.list.size);
    page->add_checkbox("Bytes", settings_->defaults.list.bytes);
    page->add_checkbox("Type", settings_->defaults.list.type);
    page->add_checkbox("Mime", settings_->defaults.list.mime);
    page->add_checkbox("Perm", settings_->defaults.list.perm);
    page->add_checkbox("Owner", settings_->defaults.list.owner);
    page->add_checkbox("Group", settings_->defaults.list.group);
    page->add_checkbox("Date Accessed", settings_->defaults.list.atime);
    page->add_checkbox("Date Created", settings_->defaults.list.btime);
    page->add_checkbox("Date Metadata", settings_->defaults.list.ctime);
    page->add_checkbox("Date Modified", settings_->defaults.list.mtime);
}
