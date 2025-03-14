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

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "gui/dialog/action.hxx"

#include "vfs/file.hxx"

gui::dialog::action::action(Gtk::ApplicationWindow& parent, std::string_view title,
                            const std::span<const std::shared_ptr<vfs::file>>& files)
    : files_(files.begin(), files.end())
{
    set_transient_for(parent);
    set_modal(true);

    set_size_request(800, 800);
    set_title(title.data());
    set_resizable(false);

    // Content //

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.set_margin(5);
    set_child(box_);

    label_ = Gtk::Label(title.data());
    box_.append(label_);

    scrolled_window_.set_has_frame(true);
    scrolled_window_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scrolled_window_.set_expand(true);
    box_.append(scrolled_window_);

    create_model();

    // create column view
    selection_model_ = Gtk::SingleSelection::create(liststore_);
    selection_model_->set_autoselect(false);
    selection_model_->set_can_unselect(true);
    columnview_.set_model(selection_model_);
    columnview_.set_reorderable(false);
    columnview_.add_css_class("data-table");
    add_columns();
    scrolled_window_.set_child(columnview_);

    const auto total_size =
        std::format("Total Size: {}", ztd::format_filesize(total_size_, ztd::base::iec));
    total_size_label_ = Gtk::Label(total_size);
    box_.append(total_size_label_);

    // keybindings
    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &action::on_key_press),
                                                 false);
    add_controller(key_controller);

    // Buttons //

    button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_ok_ = Gtk::Button("Confirm", true);
    button_cancel_ = Gtk::Button("Cancel", true);

    box_.append(button_box_);
    button_box_.set_halign(Gtk::Align::END);
    button_box_.append(button_cancel_);
    button_box_.append(button_ok_);

    button_ok_.signal_clicked().connect(sigc::mem_fun(*this, &action::on_button_ok_clicked));
    button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &action::on_button_cancel_clicked));

    set_child(box_);

    set_visible(true);

    // Set 'Ok' button as default
    button_ok_.grab_focus();
}

bool
gui::dialog::action::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                                  Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        on_button_ok_clicked();
    }
    if (keyval == GDK_KEY_Escape)
    {
        on_button_cancel_clicked();
    }
    return false;
}

void
gui::dialog::action::on_button_ok_clicked()
{
    signal_confirm().emit();

    close();
}

void
gui::dialog::action::on_button_cancel_clicked()
{
    close();
}

void
gui::dialog::action::create_model()
{
    liststore_ = Gio::ListStore<ModelColumns>::create();

    for (const auto& file : files_)
    {
        total_size_ += file->size().data();
        liststore_add_item(file->name(), file->size().data(), file->is_directory());
    }
}

void
gui::dialog::action::liststore_add_item(const std::string_view name, const std::uint64_t size,
                                        const bool is_dir)
{
    liststore_->append(
        ModelColumns::create(name, ztd::format_filesize(size, ztd::base::iec), is_dir));
}

void
gui::dialog::action::add_columns()
{
    // column for file names
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::bind(sigc::mem_fun(*this, &action::on_setup_label), Gtk::Align::START));
    factory->signal_bind().connect(sigc::mem_fun(*this, &action::on_bind_name));
    auto column = Gtk::ColumnViewColumn::create("Name", factory);
    column->set_expand(true);
    columnview_.append_column(column);

    // column for file sizes
    factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::bind(sigc::mem_fun(*this, &action::on_setup_label), Gtk::Align::END));
    factory->signal_bind().connect(sigc::mem_fun(*this, &action::on_bind_size));
    column = Gtk::ColumnViewColumn::create("Size", factory);
    columnview_.append_column(column);
}

void
gui::dialog::action::on_setup_label(const Glib::RefPtr<Gtk::ListItem>& list_item, Gtk::Align halign)
{
    list_item->set_child(*Gtk::make_managed<Gtk::Label>("", halign));
}

void
gui::dialog::action::on_bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item)
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
gui::dialog::action::on_bind_size(const Glib::RefPtr<Gtk::ListItem>& list_item)
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
