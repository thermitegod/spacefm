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

#include "vfs/file.hxx"

gui::dialog::app_chooser::app_chooser(Gtk::ApplicationWindow& parent,
                                      const std::shared_ptr<vfs::file>& file, bool focus_all_apps,
                                      bool show_command, bool show_default)
    : file_(file)
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

    if (show_command)
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
    if (show_default)
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

    button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_ok_ = Gtk::Button("_Ok", true);
    button_close_ = Gtk::Button("_Close", true);
    button_box_.set_halign(Gtk::Align::END);
    button_box_.append(button_close_);
    button_box_.append(button_ok_);
    box_.append(button_box_);

    button_ok_.signal_clicked().connect(sigc::mem_fun(*this, &app_chooser::on_button_ok_clicked));
    button_close_.signal_clicked().connect(
        sigc::mem_fun(*this, &app_chooser::on_button_close_clicked));

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
    }
    if (keyval == GDK_KEY_Escape)
    {
        on_button_close_clicked();
    }
    return false;
}

void
gui::dialog::app_chooser::on_button_ok_clicked()
{
    std::string app;
    bool is_desktop = false;

    if (entry_.get_text_length() > 0)
    { // use command text
        is_desktop = false;
        app = entry_.get_text();
    }
    else
    { // use selected app
        is_desktop = true;

        const auto current_page = notebook_.get_current_page();
        auto* page = dynamic_cast<app_chooser::page*>(notebook_.get_nth_page(current_page));

        auto* list = dynamic_cast<Gtk::ListView*>(page->get_child());
        auto model = std::dynamic_pointer_cast<Gio::ListModel>(list->get_model());
        auto item = model->get_object(page->position_);
        if (auto app_info = std::dynamic_pointer_cast<Gio::AppInfo>(item))
        {
            app = app_info->get_id();
        }
    }

    if (app.empty())
    {
        on_button_close_clicked();
    }

    signal_confirm().emit(chooser_response{.file = file_,
                                           .app = app,
                                           .is_desktop = is_desktop,
                                           .set_default = btn_set_as_default_.get_active()});

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
    factory->signal_setup().connect(sigc::mem_fun(*this, &app_chooser::page::setup_listitem));
    factory->signal_bind().connect(sigc::mem_fun(*this, &app_chooser::page::bind_listitem));

    auto model = create_application_list(mime_type);
    list_ = Gtk::make_managed<Gtk::ListView>(Gtk::SingleSelection::create(model), factory);
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
gui::dialog::app_chooser::page::setup_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item)
{
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    auto* image = Gtk::make_managed<Gtk::Image>();
    image->set_icon_size(Gtk::IconSize::NORMAL);
    box->append(*image);
    auto* label = Gtk::make_managed<Gtk::Label>();
    box->append(*label);
    list_item->set_child(*box);
}

void
gui::dialog::app_chooser::page::bind_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item)
{
    if (auto* image = dynamic_cast<Gtk::Image*>(list_item->get_child()->get_first_child()))
    {
        if (auto* label = dynamic_cast<Gtk::Label*>(image->get_next_sibling()))
        {
            if (auto app_info = std::dynamic_pointer_cast<Gio::AppInfo>(list_item->get_item()))
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
    position_ = position;
    parent_->on_button_ok_clicked();
}
