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

#include <ztd/ztd.hxx>

#include "datatypes.hxx"

#include "vfs/vfs-mime-type.hxx"

#include "app-chooser.hxx"

AppChooserDialog::AppChooserDialog(const std::string_view json_data)
{
    const auto data = glz::read_json<datatype::app_chooser::request>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    const auto& opts = data.value();

    this->type_ = vfs::mime_type::create_from_type(opts.mime_type);

    this->set_size_request(600, 600);
    this->set_title("File Properties");
    this->set_resizable(false);

    // Content //

    this->vbox_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->vbox_.set_margin_start(5);
    this->vbox_.set_margin_end(5);
    this->vbox_.set_margin_top(5);
    this->vbox_.set_margin_bottom(5);

    this->title_.set_label("Choose an application or enter a command:");
    this->title_.set_xalign(0.0f);
    this->vbox_.append(this->title_);

    this->file_type_hbox_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->file_type_label_.set_label("File Type:");
    this->file_type_.set_label(
        std::format(" {}\n ( {} )", this->type_->description(), this->type_->type()));
    this->file_type_hbox_.append(this->file_type_label_);
    this->file_type_hbox_.append(this->file_type_);
    this->vbox_.append(this->file_type_hbox_);

    if (opts.show_command)
    {
        this->entry_hbox_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
        this->entry_label_.set_label("Command:");
        this->entry_.set_placeholder_text("Command...");
        this->entry_.set_hexpand(true);
        this->entry_hbox_.append(this->entry_label_);
        this->entry_hbox_.append(this->entry_);
        this->vbox_.append(this->entry_hbox_);
    }

    this->vbox_.append(this->notebook_);

    this->page_associated_.init(this, this->type_);
    this->page_all_.init(this, nullptr);

    this->notebook_.append_page(this->page_associated_, this->label_associated_);
    this->notebook_.append_page(this->page_all_, this->label_all_);

    this->btn_open_in_terminal_.set_label("Open in a terminal");
    this->vbox_.append(this->btn_open_in_terminal_);
    if (opts.show_default)
    {
        this->btn_set_as_default_.set_label("Set as the default application for this file type");
        this->vbox_.append(this->btn_set_as_default_);
    }

    //////////////////

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(
        sigc::mem_fun(*this, &AppChooserDialog::on_key_press),
        false);
    this->add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_ok_ = Gtk::Button("_Ok", true);
    this->button_close_ = Gtk::Button("_Close", true);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_close_);
    this->button_box_.append(this->button_ok_);
    this->vbox_.append(this->button_box_);

    this->button_ok_.signal_clicked().connect(
        sigc::mem_fun(*this, &AppChooserDialog::on_button_ok_clicked));
    this->button_close_.signal_clicked().connect(
        sigc::mem_fun(*this, &AppChooserDialog::on_button_close_clicked));

    this->set_child(this->vbox_);

    this->set_visible(true);

    this->set_focus(this->notebook_);
    this->notebook_.set_current_page(opts.focus_all_apps ? 1 : 0);
}

bool
AppChooserDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        this->on_button_ok_clicked();
    }
    if (keyval == GDK_KEY_Escape)
    {
        this->on_button_close_clicked();
    }
    return false;
}

void
AppChooserDialog::on_button_ok_clicked()
{
    std::string app;
    bool is_desktop = false;

    if (this->entry_.get_text_length() > 0)
    { // use command text
        is_desktop = false;
        app = this->entry_.get_text();
    }
    else
    { // use selected app
        is_desktop = true;

        const auto page = this->notebook_.get_current_page();
        auto* app_page =
            dynamic_cast<AppChooserDialog::AppPage*>(this->notebook_.get_nth_page(page));
        auto* list = dynamic_cast<Gtk::ListView*>(app_page->get_child());
        auto item = std::dynamic_pointer_cast<Gio::ListModel>(list->get_model())
                        ->get_object(app_page->position_);
        if (auto app_info = std::dynamic_pointer_cast<Gio::AppInfo>(item))
        {
            app = app_info->get_id();
        }
    }

    if (app.empty())
    {
        this->on_button_close_clicked();
    }

    const auto buffer = glz::write_json(
        datatype::app_chooser::response{.app = app,
                                        .is_desktop = is_desktop,
                                        .set_default = this->btn_set_as_default_.get_active()});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
AppChooserDialog::on_button_close_clicked()
{
    this->close();
}

void
AppChooserDialog::AppPage::init(AppChooserDialog* parent,
                                const std::shared_ptr<vfs::mime_type>& mime_type)
{
    this->parent_ = parent;

    this->set_expand(true);

    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::mem_fun(*this, &AppChooserDialog::AppPage::setup_listitem));
    factory->signal_bind().connect(sigc::mem_fun(*this, &AppChooserDialog::AppPage::bind_listitem));

    auto model = create_application_list(mime_type);
    this->list_ = Gtk::make_managed<Gtk::ListView>(Gtk::SingleSelection::create(model), factory);
    this->list_->signal_activate().connect(
        sigc::mem_fun(*this, &AppChooserDialog::AppPage::activate));

    this->set_child(*this->list_);
}

Glib::RefPtr<Gio::ListModel>
AppChooserDialog::AppPage::create_application_list(const std::shared_ptr<vfs::mime_type>& mime_type)
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
AppChooserDialog::AppPage::setup_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item)
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
AppChooserDialog::AppPage::bind_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item)
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
AppChooserDialog::AppPage::activate(const std::uint32_t position)
{
    this->position_ = position;
    this->parent_->on_button_ok_clicked();
}
