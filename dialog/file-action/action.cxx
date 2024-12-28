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

#include <gtkmm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "datatypes.hxx"

#include "action.hxx"

ActionDialog::ActionDialog(const std::string_view header, const std::string_view json_data)
{
    const auto data = glz::read_json<std::vector<datatype::file_action_dialog::request>>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    this->file_data_ = data.value();

    this->set_size_request(800, 800);
    this->set_title(header.data());
    this->set_resizable(false);

    // Content //

    this->vbox_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->vbox_.set_margin(5);
    this->set_child(this->vbox_);

    this->label_ = Gtk::Label(header.data());
    this->vbox_.append(this->label_);

    this->scrolled_window_.set_has_frame(true);
    this->scrolled_window_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    this->scrolled_window_.set_expand(true);
    this->vbox_.append(this->scrolled_window_);

    // create model
    this->create_model();

    // create column view
    this->selection_model_ = Gtk::SingleSelection::create(this->liststore_);
    this->selection_model_->set_autoselect(false);
    this->selection_model_->set_can_unselect(true);
    this->columnview_.set_model(this->selection_model_);
    this->columnview_.set_reorderable(false);
    this->columnview_.add_css_class("data-table");
    this->add_columns();
    this->scrolled_window_.set_child(this->columnview_);

    const auto total_size =
        std::format("Total Size: {}", ztd::format_filesize(this->total_size_, ztd::base::iec));
    this->total_size_label_ = Gtk::Label(total_size);
    this->vbox_.append(this->total_size_label_);

    // keybindings
    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &ActionDialog::on_key_press),
                                                 false);
    this->add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_ok_ = Gtk::Button("Confirm", true);
    this->button_cancel_ = Gtk::Button("Cancel", true);

    this->vbox_.append(this->button_box_);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_cancel_);
    this->button_box_.append(this->button_ok_);

    this->button_ok_.signal_clicked().connect(
        sigc::mem_fun(*this, &ActionDialog::on_button_ok_clicked));
    this->button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &ActionDialog::on_button_cancel_clicked));

    this->set_child(this->vbox_);

    this->set_visible(true);

    // Set 'Ok' button as default
    this->button_ok_.grab_focus();
}

bool
ActionDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
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
ActionDialog::on_button_ok_clicked()
{
    const auto buffer =
        glz::write_json(datatype::file_action_dialog::response{.result = "Confirm"});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
ActionDialog::on_button_cancel_clicked()
{
    const auto buffer = glz::write_json(datatype::file_action_dialog::response{.result = "Cancel"});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
ActionDialog::create_model()
{
    this->liststore_ = Gio::ListStore<ModelColumns>::create();

    for (const auto& data : file_data_)
    {
        this->total_size_ += data.size;
        this->liststore_add_item(data.name, data.size, data.is_dir);
    }
}

void
ActionDialog::liststore_add_item(const std::string_view name, const std::uint64_t size,
                                 const bool is_dir)
{
    this->liststore_->append(
        ModelColumns::create(name, ztd::format_filesize(size, ztd::base::iec), is_dir));
}

void
ActionDialog::add_columns()
{
    // column for file names
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::bind(sigc::mem_fun(*this, &ActionDialog::on_setup_label), Gtk::Align::START));
    factory->signal_bind().connect(sigc::mem_fun(*this, &ActionDialog::on_bind_name));
    auto column = Gtk::ColumnViewColumn::create("Name", factory);
    column->set_expand(true);
    this->columnview_.append_column(column);

    // column for file sizes
    factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::bind(sigc::mem_fun(*this, &ActionDialog::on_setup_label), Gtk::Align::END));
    factory->signal_bind().connect(sigc::mem_fun(*this, &ActionDialog::on_bind_size));
    column = Gtk::ColumnViewColumn::create("Size", factory);
    this->columnview_.append_column(column);
}

void
ActionDialog::on_setup_label(const Glib::RefPtr<Gtk::ListItem>& list_item, Gtk::Align halign)
{
    list_item->set_child(*Gtk::make_managed<Gtk::Label>("", halign));
}

void
ActionDialog::on_bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item)
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
ActionDialog::on_bind_size(const Glib::RefPtr<Gtk::ListItem>& list_item)
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
    label->set_text(col->size_);
}
