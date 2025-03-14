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

#include "gui/dialog/bookmarks.hxx"

#include "vfs/bookmarks.hxx"

gui::dialog::bookmarks::bookmarks(Gtk::ApplicationWindow& parent,
                                  const std::shared_ptr<vfs::bookmarks>& bookmarks,
                                  const std::shared_ptr<config::settings>& settings) noexcept
    : bookmarks_(bookmarks), settings_(settings)
{
    set_transient_for(parent);
    set_modal(true);

    set_size_request(800, 800);
    set_title("Bookmark Manager");
    set_resizable(false);

    // Content //

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.set_margin(5);
    set_child(box_);

    scrolled_window_.set_has_frame(true);
    scrolled_window_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scrolled_window_.set_expand(true);
    box_.append(scrolled_window_);

    create_model();

    selection_model_ = Gtk::SingleSelection::create(liststore_);
    selection_model_->set_autoselect(true);
    selection_model_->set_can_unselect(true);
    columnview_.set_model(selection_model_);
    columnview_.set_reorderable(false);
    // columnview_.set_single_click_activate(true);
    columnview_.add_css_class("data-table");
    columnview_.signal_activate().connect([this]([[maybe_unused]] auto i)
                                          { on_button_ok_clicked(); });
    add_columns();
    scrolled_window_.set_child(columnview_);

    // keybindings //

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &bookmarks::on_key_press),
                                                 false);
    add_controller(key_controller);

    // Buttons //

    button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_ok_ = Gtk::Button("Open", true);
    button_ok_.signal_clicked().connect([this]() { on_button_ok_clicked(); });
    button_close_ = Gtk::Button("Close", true);
    button_close_.signal_clicked().connect([this]() { on_button_close_clicked(); });
    button_remove_ = Gtk::Button("Remove", true);
    button_remove_.signal_clicked().connect([this]() { on_button_remove_clicked(); });
    button_remove_all_ = Gtk::Button("Remove All", true);
    button_remove_all_.signal_clicked().connect([this]() { on_button_remove_all_clicked(); });

    box_.append(button_box_);
    button_box_.set_halign(Gtk::Align::END);
    button_box_.append(button_remove_all_);
    button_box_.append(button_remove_);
    button_box_.append(button_close_);
    button_box_.append(button_ok_);

    set_child(box_);

    set_visible(true);
}

bool
gui::dialog::bookmarks::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
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
gui::dialog::bookmarks::on_button_ok_clicked() noexcept
{
    if (auto selected =
            std::dynamic_pointer_cast<ModelColumns>(selection_model_->get_selected_item()))
    {
        signal_confirm().emit(selected->path_);
    }

    close();
}

void
gui::dialog::bookmarks::on_button_remove_clicked() noexcept
{
    if (auto selected =
            std::dynamic_pointer_cast<ModelColumns>(selection_model_->get_selected_item()))
    {
        bookmarks_->remove(selected->path_);
        liststore_->remove(selection_model_->get_selected());
    }
}

void
gui::dialog::bookmarks::on_button_remove_all_clicked() noexcept
{
    auto dialog = Gtk::AlertDialog::create("Remove All Bookmarks?");
    dialog->set_detail(
        std::format("This will remove '{}' bookmarks", bookmarks_->get_bookmarks().size()));
    dialog->set_modal(true);
    dialog->set_buttons({"Cancel", "Confirm"});
    dialog->set_cancel_button(0);
    dialog->set_default_button(0);

    auto slot = [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result)
    {
        try
        {
            const auto response = dialog->choose_finish(result);
            if (response == 1)
            { // Confirm Button
                bookmarks_->remove_all();
                liststore_->remove_all();
            }
        }
        catch (...)
        {
        }
    };
    dialog->choose(*this, slot);
}

void
gui::dialog::bookmarks::on_button_close_clicked() noexcept
{
    close();
}

void
gui::dialog::bookmarks::create_model() noexcept
{
    liststore_ = Gio::ListStore<ModelColumns>::create();

    for (const auto& data : bookmarks_->get_bookmarks())
    {
        liststore_add_item(data.name, data.path, data.created);
    }
}

void
gui::dialog::bookmarks::liststore_add_item(
    const std::string_view name, const std::filesystem::path& path,
    const std::chrono::system_clock::time_point created) noexcept
{
    liststore_->append(ModelColumns::create(name, path, created));
}

void
gui::dialog::bookmarks::add_columns() noexcept
{
    { // name
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &bookmarks::on_setup_label), Gtk::Align::START));
        factory->signal_bind().connect(sigc::mem_fun(*this, &bookmarks::on_bind_name));
        auto column = Gtk::ColumnViewColumn::create("Name", factory);
        column->set_expand(true);
        columnview_.append_column(column);
    }

    { // path
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &bookmarks::on_setup_label), Gtk::Align::START));
        factory->signal_bind().connect(sigc::mem_fun(*this, &bookmarks::on_bind_path));
        auto column = Gtk::ColumnViewColumn::create("Path", factory);
        column->set_expand(true);
        columnview_.append_column(column);
    }

    { // create date
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &bookmarks::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &bookmarks::on_bind_created));
        auto column = Gtk::ColumnViewColumn::create("Created", factory);
        columnview_.append_column(column);
    }
}

void
gui::dialog::bookmarks::on_setup_label(const Glib::RefPtr<Gtk::ListItem>& list_item,
                                       Gtk::Align halign) noexcept
{
    list_item->set_child(*Gtk::make_managed<Gtk::Label>("", halign));
}

void
gui::dialog::bookmarks::on_bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept
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
gui::dialog::bookmarks::on_bind_path(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept
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
    label->set_text(col->path_.string());
}

void
gui::dialog::bookmarks::on_bind_created(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept
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
    label->set_text(std::format("{}", std::chrono::floor<std::chrono::seconds>(col->created_)));
}
