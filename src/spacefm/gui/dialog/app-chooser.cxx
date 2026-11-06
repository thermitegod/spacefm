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

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "gui/dialog/app-chooser.hxx"
#include "gui/dialog/widgets/button-box.hxx"

#include "vfs/file.hxx"

gui::dialog::app_chooser::app_chooser(Gtk::ApplicationWindow& parent,
                                      const std::shared_ptr<vfs::file>& file, bool focus_all_apps,
                                      bool show_command, bool show_default)
    : file_(file),
      show_command_(show_command),
      show_default_(show_default)
{
    set_transient_for(parent);
    set_modal(true);

    set_size_request(600, 600);
    set_title("File Properties");
    set_resizable(false);

    auto type = file_->mime_type();

    // Content //

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.set_margin(5);

    title_.set_label("Choose an application or enter a command:");
    title_.set_xalign(0.0f);
    box_.append(title_);

    file_type_hbox_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    file_type_label_.set_label("File Type:");
    file_type_.set_label(std::format(" {}\n ( {} )", type->description(), type->type()));
    file_type_hbox_.append(file_type_label_);
    file_type_hbox_.append(file_type_);
    box_.append(file_type_hbox_);

    if (show_command_)
    {
        entry_hbox_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
        entry_label_.set_label("Command:");
        entry_.set_placeholder_text("Command...");
        entry_.set_hexpand(true);
        entry_hbox_.append(entry_label_);
        entry_hbox_.append(entry_);
        box_.append(entry_hbox_);
    }

    box_.append(notebook_);

    page_associated_.init(this, type);
    page_all_.init(this, nullptr);

    notebook_.append_page(page_associated_, label_associated_);
    notebook_.append_page(page_all_, label_all_);

    btn_open_in_terminal_.set_label("Open in a terminal");
    box_.append(btn_open_in_terminal_);
    if (show_default_)
    {
        btn_set_as_default_.set_label("Set as the default application for this file type");
        box_.append(btn_set_as_default_);
    }

    //////////////////

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &app_chooser::on_key_press),
                                                 false);
    add_controller(key_controller);

    // Buttons //
    auto* buttons = gui::widget::ButtonBox::create({
        {"Close", [this] { on_button_close_clicked(); }, &button_close_},
        {"Ok", [this] { on_button_ok_clicked(); }, &button_ok_},
    });
    box_.append(*buttons);

    set_child(box_);

    set_visible(true);

    set_focus(notebook_);
    notebook_.set_current_page(focus_all_apps ? 1 : 0);
}

bool
gui::dialog::app_chooser::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                                       Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        on_button_ok_clicked();
        return true;
    }
    if (keyval == GDK_KEY_Escape)
    {
        on_button_close_clicked();
        return true;
    }
    return false;
}

void
gui::dialog::app_chooser::on_button_ok_clicked()
{
    std::string app;
    bool is_desktop = false;

    const auto command = entry_.get_text();

    if (show_command_ && !command.empty())
    { // use command text
        app = command;
    }
    else
    { // use selected app
        const auto current_page = notebook_.get_current_page();

        auto* selected_page = dynamic_cast<page*>(notebook_.get_nth_page(current_page));

        if (!selected_page)
        {
            return;
        }

        const auto app_info = selected_page->selected_application();

        if (!app_info)
        {
            return;
        }

        app = app_info->get_id();

        if (app.empty())
        {
            return;
        }

        is_desktop = true;
    }

    signal_confirm_.emit(chooser_response{
        .file = file_,
        .app = std::move(app),
        .is_desktop = is_desktop,
        .set_default = show_default_ && btn_set_as_default_.get_active(),
    });

    close();
}

void
gui::dialog::app_chooser::on_button_close_clicked()
{
    close();
}

void
gui::dialog::app_chooser::page::init(app_chooser* parent,
                                     const std::shared_ptr<vfs::mime_type>& mime_type)
{
    parent_ = parent;

    set_expand(true);

    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(sigc::mem_fun(*this, &app_chooser::page::on_setup_item));
    factory->signal_bind().connect(sigc::mem_fun(*this, &app_chooser::page::on_bind_item));

    auto model = create_application_list(mime_type);

    selection_model_ = Gtk::SingleSelection::create(model);
    selection_model_->set_autoselect(false);

    list_ = Gtk::make_managed<Gtk::ListView>(selection_model_, factory);
    list_->signal_activate().connect(sigc::mem_fun(*this, &app_chooser::page::activate));

    set_child(*list_);
}

Glib::RefPtr<Gio::ListModel>
gui::dialog::app_chooser::page::create_application_list(
    const std::shared_ptr<vfs::mime_type>& mime_type)
{
    auto store = Gio::ListStore<Gio::AppInfo>::create();

    if (mime_type == nullptr)
    { // load all apps
        for (const auto& app : Gio::AppInfo::get_all())
        {
            store->append(app);
        }
    }
    else
    {
        for (const auto& app : Gio::AppInfo::get_all_for_type(mime_type->type().data()))
        {
            store->append(app);
        }
    }

    return store;
}

void
gui::dialog::app_chooser::page::on_setup_item(const Glib::RefPtr<Gtk::ListItem>& item)
{
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    auto* image = Gtk::make_managed<Gtk::Image>();
    image->set_icon_size(Gtk::IconSize::NORMAL);
    box->append(*image);
    auto* label = Gtk::make_managed<Gtk::Label>();
    box->append(*label);
    item->set_child(*box);
}

void
gui::dialog::app_chooser::page::on_bind_item(const Glib::RefPtr<Gtk::ListItem>& item)
{
    if (auto* image = dynamic_cast<Gtk::Image*>(item->get_child()->get_first_child()))
    {
        if (auto* label = dynamic_cast<Gtk::Label*>(image->get_next_sibling()))
        {
            if (auto app_info = std::dynamic_pointer_cast<Gio::AppInfo>(item->get_item()))
            {
                image->set(app_info->get_icon());
                label->set_label(app_info->get_display_name());
            }
        }
    }
}

void
gui::dialog::app_chooser::page::activate(const std::uint32_t position)
{
    if (!selection_model_ || position >= selection_model_->get_model()->get_n_items())
    {
        return;
    }

    selection_model_->set_selected(position);
    parent_->on_button_ok_clicked();
}

Glib::RefPtr<Gio::AppInfo>
gui::dialog::app_chooser::page::selected_application() const noexcept
{
    if (!selection_model_)
    {
        return {};
    }

    const auto position = selection_model_->get_selected();
    if (position == GTK_INVALID_LIST_POSITION)
    {
        return {};
    }

    return std::dynamic_pointer_cast<Gio::AppInfo>(
        selection_model_->get_model()->get_object(position));
}
