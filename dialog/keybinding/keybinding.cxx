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

#include <print>
#include <string_view>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include "datatypes.hxx"
#include "keybinding.hxx"
#include "utils.hxx"

// TODO
// - breakup GUI/Logic code
// - update keybinding view after key change

KeybindingDialog::KeybindingDialog(const std::string_view json_data)
{
    const auto data = glz::read_json<decltype(this->keybindings_data_)>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    this->keybindings_data_ = data.value();

    this->set_size_request(800, 800);
    this->set_title("Set Keybindings");
    this->set_resizable(false);

    // Content //

    this->box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->box_.set_margin(5);

    // this->notebook_.set_margin(5);
    this->notebook_.set_vexpand(true);
    this->notebook_.set_hexpand(true);
    this->box_.append(this->notebook_);

    this->page_navigation_.init(this, this->keybindings_data_, "navigation");
    this->page_editing_.init(this, this->keybindings_data_, "editing");
    this->page_view_.init(this, this->keybindings_data_, "view");
    this->page_tabs_.init(this, this->keybindings_data_, "tabs");
    this->page_general_.init(this, this->keybindings_data_, "general");
    this->page_opening_.init(this, this->keybindings_data_, "opening");

    this->notebook_.append_page(this->page_navigation_, this->label_navigation_);
    this->notebook_.append_page(this->page_editing_, this->label_editing_);
    this->notebook_.append_page(this->page_view_, this->label_view_);
    this->notebook_.append_page(this->page_tabs_, this->label_tabs_);
    this->notebook_.append_page(this->page_general_, this->label_general_);
    this->notebook_.append_page(this->page_opening_, this->label_opening_);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(
        sigc::mem_fun(*this, &KeybindingDialog::on_key_press),
        false);
    this->box_.add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_ok_ = Gtk::Button("_Apply", true);
    this->button_ok_.set_sensitive(false);
    this->button_cancel_ = Gtk::Button("_Close", true);

    this->box_.append(this->button_box_);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_cancel_);
    this->button_box_.append(this->button_ok_);

    this->button_ok_.signal_clicked().connect(
        sigc::mem_fun(*this, &KeybindingDialog::on_button_ok_clicked));
    this->button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &KeybindingDialog::on_button_cancel_clicked));

    this->set_child(this->box_);

    this->set_visible(true);
}

bool
KeybindingDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        this->on_button_ok_clicked();
    }
    if (keyval == GDK_KEY_Escape)
    {
        this->on_button_cancel_clicked();
    }
    return false;
}

void
KeybindingDialog::on_button_ok_clicked()
{
    const auto buffer = glz::write_json(this->changed_keybindings_data_);
    if (buffer)
    {
        std::println("{}", buffer.value());
    }
    this->close();
}

void
KeybindingDialog::on_button_cancel_clicked()
{
    this->close();
}

void
KeybindingDialog::KeybindingPage::init(
    KeybindingDialog* parent, const std::span<const datatype::keybinding::request> keybindings,
    const std::string_view category)
{
    this->set_orientation(Gtk::Orientation::VERTICAL);
    this->set_margin(5);

    this->scrolled_window_.set_has_frame(true);
    this->scrolled_window_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    this->scrolled_window_.set_expand(true);
    this->append(this->scrolled_window_);

    // create model
    this->create_model(keybindings, category);

    // create column view
    this->selection_model_ = Gtk::SingleSelection::create(this->liststore_);
    this->selection_model_->set_autoselect(false);
    this->selection_model_->set_can_unselect(true);
    this->columnview_.set_model(this->selection_model_);
    this->columnview_.set_reorderable(false);
    this->columnview_.add_css_class("data-table");
    this->add_columns();
    this->scrolled_window_.set_child(this->columnview_);

    this->columnview_.signal_activate().connect(
        [this, parent](const std::uint32_t position)
        {
            auto item = this->liststore_->get_item(position);

            auto name = item->property_name();
            auto key = item->property_key();
            auto mod = item->property_modifier();

#if 0
            auto name = item->property_name();
            auto key = item->property_key();
            auto mod = item->property_modifier();

            std::println("position: {}, name: {}, key: {}, mod: {}",
                         position,
                         name.get_value(),
                         key.get_value(),
                         mod.get_value());
#endif

#if defined(HAVE_DEV)
            const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_KEYBINDING_SET);
#else
            const auto binary = Glib::find_program_in_path(DIALOG_KEYBINDING_SET);
#endif
            if (binary.empty())
            {
                std::println("Failed to find keybinding set dialog binary: {}",
                             DIALOG_KEYBINDING_SET);
                std::exit(EXIT_FAILURE);
            }

            // Create json data, needs to get recreated every time because keybindings can change.
            const auto buffer = glz::write_json(parent->keybindings_data_);
            if (!buffer)
            {
                std::println("Failed to create json: {}", glz::format_error(buffer));
                std::exit(EXIT_FAILURE);
            }

            const auto command = std::format(R"({} --key-name '{}' --json '{}')",
                                             binary,
                                             name.get_value(),
                                             buffer.value());

            std::int32_t exit_status = 0;
            std::string standard_output;
            Glib::spawn_command_line_sync(command, &standard_output, nullptr, &exit_status);

            if (exit_status != 0 || standard_output.empty())
            {
                return;
            }

            // parse json return value
            const auto response_data =
                glz::read_json<datatype::keybinding::response>(standard_output);
            if (!response_data)
            {
                std::println("Failed to decode json: {}",
                             glz::format_error(response_data.error(), standard_output));
                // std::exit(EXIT_FAILURE);
                return;
            }
            const auto& response = response_data.value();

            // Keybinding update logic

            for (auto& data : parent->keybindings_data_)
            { // clear duplicate key assignments
                if (data.key != 0 && data.key == response.key && data.modifier == response.modifier)
                {
                    data.key = 0;
                    data.modifier = 0;

                    bool key_found = false;
                    for (auto& changed_data : parent->changed_keybindings_data_)
                    {
                        if (changed_data.name == data.name)
                        { // keybinding has already been changed, update
                            changed_data.key = response.key;
                            changed_data.modifier = response.modifier;

                            key_found = true;
                            break;
                        }
                    }
                    if (!key_found)
                    {
                        parent->changed_keybindings_data_.push_back(
                            {data.name, data.key, data.modifier});
                    }

                    break;
                }
            }

            for (auto& data : parent->keybindings_data_)
            { // set new key in global data
                if (response.name != data.name)
                {
                    continue;
                }

                data.key = response.key;
                data.modifier = response.modifier;
                break;
            }

            // update returned key data
            bool key_found = false;
            for (auto& changed_data : parent->changed_keybindings_data_)
            {
                if (changed_data.name == response.name)
                { // keybinding has already been changed, update
                    changed_data.key = response.key;
                    changed_data.modifier = response.modifier;

                    key_found = true;
                    break;
                }
            }
            if (!key_found)
            {
                parent->changed_keybindings_data_.push_back(response);
            }

            // TODO - Update keybinding list view
            key.set_value(response.key);
            mod.set_value(response.modifier);
            item->keybinding_ = keyname(response.key, response.modifier);

            //
            parent->button_ok_.set_sensitive(true);
        });
}

void
KeybindingDialog::KeybindingPage::create_model(
    const std::span<const datatype::keybinding::request> keybindings,
    const std::string_view category)
{
    this->liststore_ = Gio::ListStore<ModelColumns>::create();

    for (const auto& keybinding : keybindings)
    {
        if (keybinding.category != category)
        {
            continue;
        }

        this->liststore_add_item(keybinding.name, keybinding.key, keybinding.modifier);
    }
}

void
KeybindingDialog::KeybindingPage::liststore_add_item(const std::string_view name,
                                                     const std::uint32_t key,
                                                     const std::uint32_t modifier)
{
    this->liststore_->append(ModelColumns::create(name, keyname(key, modifier), key, modifier));
}

void
KeybindingDialog::KeybindingPage::add_columns()
{
    // column for names
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::bind(sigc::mem_fun(*this, &KeybindingPage::on_setup_label), Gtk::Align::START));
    factory->signal_bind().connect(sigc::mem_fun(*this, &KeybindingPage::on_bind_name));
    auto column = Gtk::ColumnViewColumn::create("Name", factory);
    column->set_expand(false);
    this->columnview_.append_column(column);

    // column for keybindings
    factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::bind(sigc::mem_fun(*this, &KeybindingPage::on_setup_label), Gtk::Align::START));
    factory->signal_bind().connect(sigc::mem_fun(*this, &KeybindingPage::on_bind_keybinding));
    column = Gtk::ColumnViewColumn::create("Keybinding", factory);
    column->set_expand(true);
    this->columnview_.append_column(column);
}

void
KeybindingDialog::KeybindingPage::on_setup_label(const Glib::RefPtr<Gtk::ListItem>& list_item,
                                                 Gtk::Align halign)
{
    list_item->set_child(*Gtk::make_managed<Gtk::Label>("", halign));
}

void
KeybindingDialog::KeybindingPage::on_bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item)
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(list_item->get_item());
    if (!col)
    {
        return;
    }
    auto* label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (!label)
    {
        return;
    }
    label->set_text(col->name_);
}

void
KeybindingDialog::KeybindingPage::on_bind_keybinding(const Glib::RefPtr<Gtk::ListItem>& list_item)
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(list_item->get_item());
    if (!col)
    {
        return;
    }
    auto* label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (!label)
    {
        return;
    }
    label->set_text(col->keybinding_);
}
