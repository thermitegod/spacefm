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
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "vfs/utils/vfs-utils.hxx"
#include "vfs/vfs-file.hxx"

#include "datatypes.hxx"
#include "logger.hxx"
#include "rename.hxx"

RenameDialog::RenameDialog(const std::string_view json_data)
{
    (void)json_data;

    const auto data = glz::read_json<datatype::rename::request>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    const auto& opts = data.value();

    const std::filesystem::path cwd = opts.cwd;
    this->file_ = vfs::file::create(opts.file);
    const auto dest_dir = opts.dest_dir;
    this->clip_copy_ = opts.clip_copy;

    this->settings_ = opts.settings;

    bool target_missing = false;

    //////////////////////

    this->set_size_request(800, 500);
    this->set_resizable(false);

    this->box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->set_child(box_);

    if (cwd.empty() || !std::filesystem::exists(cwd))
    {
        this->on_button_cancel_clicked();
    }

    const std::string_view original_filename = this->file_->name();

    this->is_dir_ = this->file_->is_directory();
    this->is_link_ = this->file_->is_symlink();
    this->clip_copy_ = opts.clip_copy;
    this->full_path_ = cwd / original_filename;
    if (!dest_dir.empty())
    {
        this->new_path_ = std::filesystem::path() / dest_dir / original_filename;
    }
    else
    {
        this->new_path_ = this->full_path_;
    }

    this->old_path_ = cwd;

    this->full_path_exists_ = false;
    this->full_path_exists_dir_ = false;
    this->full_path_same_ = false;
    this->path_missing_ = false;
    this->path_exists_file_ = false;
    this->is_move_ = false;

    // Dialog
    if (this->is_link_)
    {
        this->desc_ = "Link";
    }
    else if (this->is_dir_)
    {
        this->desc_ = "Directory";
    }
    else
    {
        this->desc_ = "File";
    }

    // Buttons
    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_next_ = Gtk::Button("_Rename", true);
    this->button_next_.set_focus_on_click(false);
    this->button_cancel_ = Gtk::Button("Cancel", true);
    this->button_cancel_.set_focus_on_click(false);
    this->button_revert_ = Gtk::Button("Re_vert", true);
    this->button_revert_.set_focus_on_click(false);
    this->button_options_ = Gtk::Button("Opt_ions", true);
    this->button_options_.set_focus_on_click(false);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_options_);
    this->button_box_.append(this->button_revert_);
    this->button_box_.append(this->button_cancel_);
    this->button_box_.append(this->button_next_);

    this->button_next_.signal_clicked().connect(
        sigc::mem_fun(*this, &RenameDialog::on_button_ok_clicked));
    this->button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &RenameDialog::on_button_cancel_clicked));
    this->button_revert_.signal_clicked().connect(
        sigc::mem_fun(*this, &RenameDialog::on_button_revert_clicked));
    this->button_options_.signal_clicked().connect(
        sigc::mem_fun(*this, &RenameDialog::on_button_options_clicked));

    // Entries

    // Type
    std::string type;
    this->label_type_.set_markup_with_mnemonic("<b>Type:</b>");
    if (this->is_link_)
    {
        try
        {
            const auto target_path = std::filesystem::read_symlink(this->full_path_);

            this->mime_type_ = target_path;
            if (std::filesystem::exists(target_path))
            {
                type = std::format("Link-> {}", target_path.string());
            }
            else
            {
                type = std::format("!Link-> {} (missing)", target_path.string());
                target_missing = true;
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            this->mime_type_ = "inode/symlink";
            type = "symbolic link ( inode/symlink )";
        }
    }
    else if (this->file_)
    {
        this->mime_type_ = this->file_->mime_type()->type();
        type = std::format(" {} ( {} )", this->file_->mime_type()->description(), this->mime_type_);
    }
    else // create
    {
        this->mime_type_ = "?";
        type = this->mime_type_;
    }
    this->label_mime_.set_label(type);
    this->label_mime_.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    this->label_mime_.set_selectable(true);
    this->label_mime_.set_halign(Gtk::Align::START);
    this->label_mime_.set_valign(Gtk::Align::START);
    this->label_mime_.set_margin(5);
    this->label_type_.set_selectable(true);

    // Target
    if (this->is_link_)
    {
        this->label_target_.set_markup_with_mnemonic("<b>_Target:</b>");
        this->label_target_.set_halign(Gtk::Align::START);
        this->label_target_.set_valign(Gtk::Align::END);
        this->label_target_.set_mnemonic_widget(this->entry_target_);
        this->label_target_.set_selectable(true);
        this->entry_target_.set_hexpand(true);

        this->entry_target_.set_text(this->mime_type_.data());
        this->entry_target_.set_editable(false);

        { // Target Signals
            auto s = this->entry_target_.signal_changed().connect(
                sigc::bind(sigc::mem_fun(*this, &RenameDialog::on_move_change),
                           this->buf_full_path_));
            this->on_move_change_signals_.push_back(s);
        }
    }

    // Filename
    this->label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>");
    this->label_full_name_.set_halign(Gtk::Align::START);
    this->label_full_name_.set_valign(Gtk::Align::START);
    this->label_full_name_.set_margin(4);
    this->label_full_name_.set_mnemonic_widget(this->input_full_name_);
    this->label_full_name_.set_selectable(true);
    this->buf_full_name_ = Gtk::TextBuffer::create();
    this->input_full_name_.set_buffer(this->buf_full_name_);
    this->input_full_name_.set_wrap_mode(Gtk::WrapMode::CHAR);
    this->input_full_name_.set_monospace(true);
    this->scroll_full_name_.set_child(this->input_full_name_);
    this->scroll_full_name_.set_expand(true);

    { // Filename Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(
            sigc::mem_fun(*this, &RenameDialog::on_key_press),
            false);

        this->input_full_name_.add_controller(key_controller);

        auto s = this->buf_full_name_->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &RenameDialog::on_move_change), this->buf_full_name_));
        this->on_move_change_signals_.push_back(s);
    }

    // Parent
    this->label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
    this->label_path_.set_halign(Gtk::Align::START);
    this->label_path_.set_valign(Gtk::Align::START);
    this->label_path_.set_margin(4);
    this->label_path_.set_mnemonic_widget(this->input_path_);
    this->label_path_.set_selectable(true);
    this->buf_path_ = Gtk::TextBuffer::create();
    this->input_path_.set_buffer(this->buf_path_);
    this->input_path_.set_wrap_mode(Gtk::WrapMode::CHAR);
    this->input_path_.set_monospace(true);
    this->scroll_path_.set_child(this->input_path_);
    this->scroll_path_.set_expand(true);

    { // Parent Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(
            sigc::mem_fun(*this, &RenameDialog::on_key_press),
            false);

        this->input_path_.add_controller(key_controller);

        auto s = this->buf_path_->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &RenameDialog::on_move_change), this->buf_path_));
        this->on_move_change_signals_.push_back(s);
    }

    // Path
    this->label_full_path_.set_markup_with_mnemonic("<b>P_ath:</b>");
    this->label_full_path_.set_halign(Gtk::Align::START);
    this->label_full_path_.set_valign(Gtk::Align::START);
    this->label_full_path_.set_margin(4);
    this->label_full_path_.set_mnemonic_widget(this->input_full_path_);
    this->label_full_path_.set_selectable(true);
    this->buf_full_path_ = Gtk::TextBuffer::create();
    this->buf_full_path_->set_text(this->new_path_.string());
    this->input_full_path_.set_buffer(this->buf_full_path_);
    this->input_full_path_.set_wrap_mode(Gtk::WrapMode::CHAR);
    this->input_full_path_.set_monospace(true);
    this->scroll_full_path_.set_child(this->input_full_path_);
    this->scroll_full_path_.set_expand(true);

    { // Path Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(
            sigc::mem_fun(*this, &RenameDialog::on_key_press),
            false);

        this->input_full_path_.add_controller(key_controller);

        auto s = this->buf_full_path_->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &RenameDialog::on_move_change), this->buf_full_path_));
        this->on_move_change_signals_.push_back(s);
    }

    // Options
    this->opt_move_.set_label("Move");
    this->opt_copy_.set_label("Copy");
    this->opt_link_.set_label("Link");
    this->opt_copy_target_.set_label("Copy Target");
    this->opt_link_target_.set_label("Link Target");
    this->opt_copy_.set_group(this->opt_move_);
    this->opt_link_.set_group(this->opt_move_);
    this->opt_copy_target_.set_group(this->opt_move_);
    this->opt_link_target_.set_group(this->opt_move_);

    this->opt_move_.set_focus_on_click(false);
    this->opt_copy_.set_focus_on_click(false);
    this->opt_link_.set_focus_on_click(false);
    this->opt_copy_target_.set_focus_on_click(false);
    this->opt_link_target_.set_focus_on_click(false);

    this->opt_copy_target_.set_sensitive(this->is_link_ && !target_missing);
    this->opt_link_target_.set_sensitive(this->is_link_);

    // Options Context Menu
    auto submenu_model = Gio::Menu::create();
    submenu_model->append("Copy", "app.copy");
    submenu_model->append("Link", "app.link");
    submenu_model->append("Copy Target", "app.copy_target");
    submenu_model->append("Link Target", "app.link_target");

    auto menu_model = Gio::Menu::create();
    menu_model->append("Filename", "app.filename");
    menu_model->append("Path", "app.path");
    menu_model->append("Parent", "app.parent");
    menu_model->append("Type", "app.type");
    menu_model->append("Target", "app.target");
    menu_model->append_submenu("Options", submenu_model);
    menu_model->append_section("", Gio::Menu::create()); // Separator
    menu_model->append("Create Parents", "app.confirm");

    this->context_menu_.set_menu_model(menu_model);
    this->context_menu_.set_parent(this->button_options_);
    // this->context_menu_.unparent();

    this->context_action_group_ = Gio::SimpleActionGroup::create();

    this->action_filename_ = Gio::SimpleAction::create("filename");
    this->action_filename_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.filename = !this->settings_.filename;
            this->on_toggled();
        });
    this->context_action_group_->add_action(this->action_filename_);

    this->action_path_ = Gio::SimpleAction::create("path");
    this->action_path_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.path = !this->settings_.path;
            this->on_toggled();
        });
    this->context_action_group_->add_action(this->action_path_);

    this->action_parent_ = Gio::SimpleAction::create("parent");
    this->action_parent_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.parent = !this->settings_.parent;
            this->on_toggled();
        });
    this->context_action_group_->add_action(this->action_parent_);

    this->action_type_ = Gio::SimpleAction::create("type");
    this->action_type_->set_enabled(!this->is_link_);
    this->action_type_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.parent = !this->settings_.parent;
            this->on_toggled();
        });
    this->context_action_group_->add_action(this->action_type_);

    this->action_target_ = Gio::SimpleAction::create("target");
    this->action_target_->set_enabled(this->is_link_);
    this->action_target_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.parent = !this->settings_.parent;
            this->on_toggled();
        });
    this->context_action_group_->add_action(this->action_target_);

    // Option submenu
    this->action_copy_ = Gio::SimpleAction::create("copy");
    this->action_copy_->set_enabled(!this->clip_copy_);
    this->action_copy_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.parent = !this->settings_.parent;
            this->on_toggled();
        });
    this->context_action_group_->add_action(this->action_copy_);

    this->action_link_ = Gio::SimpleAction::create("link");
    this->action_link_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.parent = !this->settings_.parent;
            this->on_toggled();
        });
    this->context_action_group_->add_action(this->action_link_);

    this->action_copy_target_ = Gio::SimpleAction::create("copy_target");
    this->action_copy_target_->set_enabled(this->is_link_);
    this->action_copy_target_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.parent = !this->settings_.parent;
            this->on_toggled();
        });
    this->context_action_group_->add_action(this->action_copy_target_);

    this->action_link_target_ = Gio::SimpleAction::create("link_target");
    this->action_link_target_->set_enabled(this->is_link_);
    this->action_link_target_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.parent = !this->settings_.parent;
            this->on_toggled();
        });
    this->context_action_group_->add_action(this->action_link_target_);

    this->action_confirm_ = Gio::SimpleAction::create_bool("confirm", this->settings_.confirm);
    this->action_confirm_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            this->settings_.confirm = !this->settings_.confirm;
            this->action_confirm_->set_state(Glib::Variant<bool>::create(this->settings_.confirm));
        });
    this->context_action_group_->add_action(this->action_confirm_);

    this->insert_action_group("app", this->context_action_group_);

    // Pack
    this->set_margin(10);
    this->box_.set_margin(10);

    this->box_.append(this->label_full_name_);
    this->box_.append(this->scroll_full_name_);

    this->box_.append(this->label_path_);
    this->box_.append(this->scroll_path_);

    this->box_.append(this->label_full_path_);
    this->box_.append(this->scroll_full_path_);

    this->hbox_type_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 0);
    this->hbox_type_.append(this->label_type_);
    this->hbox_type_.append(this->label_mime_);
    this->box_.append(this->hbox_type_);

    this->hbox_target_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 0);
    this->hbox_target_.append(this->label_target_);
    this->hbox_target_.append(this->entry_target_);
    this->hbox_target_.set_hexpand(true);
    this->hbox_target_.set_margin(3);
    this->box_.append(this->hbox_target_);

    this->radio_button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 4);
    this->radio_button_box_.append(this->opt_move_);
    this->radio_button_box_.append(this->opt_copy_);
    this->radio_button_box_.append(this->opt_link_);
    this->radio_button_box_.append(this->opt_copy_target_);
    this->radio_button_box_.append(this->opt_link_target_);
    this->box_.append(this->radio_button_box_);
    this->box_.append(this->button_box_);

    // show
    this->set_visible(true);
    this->on_toggled();
    if (this->clip_copy_)
    {
        this->opt_copy_.set_active(true);
    }
    else
    {
        this->opt_move_.set_active(true);
    }

    this->opt_move_.signal_toggled().connect(sigc::mem_fun(*this, &RenameDialog::on_opt_toggled));
    this->opt_copy_.signal_toggled().connect(sigc::mem_fun(*this, &RenameDialog::on_opt_toggled));
    this->opt_link_.signal_toggled().connect(sigc::mem_fun(*this, &RenameDialog::on_opt_toggled));
    this->opt_copy_target_.signal_toggled().connect(
        sigc::mem_fun(*this, &RenameDialog::on_opt_toggled));
    this->opt_link_target_.signal_toggled().connect(
        sigc::mem_fun(*this, &RenameDialog::on_opt_toggled));

    // init
    this->on_move_change(this->buf_full_path_);
    this->on_opt_toggled();

    // select filename text widget
    this->select_input();
    this->input_full_name_.grab_focus();
}

bool
RenameDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        if (this->button_next_.get_sensitive())
        {
            this->on_button_ok_clicked();
        }
        return true;
    }
    if (keyval == GDK_KEY_Escape)
    {
        this->on_button_cancel_clicked();
    }
    return false;
}

void
RenameDialog::on_button_ok_clicked()
{
    const std::string text = this->buf_full_path_->get_text(false);
    if (text.contains('\n') || text.contains("\\n"))
    {
        auto dialog = Gtk::AlertDialog::create("Error");
        dialog->set_detail("Path contains linefeeds");
        dialog->set_modal(true);
        dialog->show();

        return;
    }

    auto full_path = std::filesystem::path(text);
    if (!full_path.is_absolute())
    {
        // update full_path to absolute
        full_path = this->full_path_.parent_path() / full_path;
    }
    // const auto full_name = full_path.filename();
    const auto path = full_path.parent_path();
    const auto old_path = this->full_path_.parent_path();

    if (this->full_path_same_ || full_path == this->full_path_)
    {
        // not changed, proceed to next file
        const auto buffer =
            glz::write_json(datatype::rename::response{.source = "", // unused in this mode
                                                       .dest = "",   // unused in this mode
                                                       .mode = datatype::rename::mode::skip,
                                                       .overwrite = false,
                                                       .settings = this->settings_});
        if (buffer)
        {
            std::println("{}", buffer.value());
        }
    }

    // determine job
    // bool move = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move));
    const bool copy = this->opt_copy_.get_active();
    const bool link = this->opt_link_.get_active();
    const bool copy_target = this->opt_copy_target_.get_active();
    const bool link_target = this->opt_link_target_.get_active();

    if (!std::filesystem::exists(path))
    {
        // create parent directory
        if (!this->settings_.confirm)
        {
            auto dialog = Gtk::AlertDialog::create("Create Parent Directory Error");
            dialog->set_detail("The parent directory does not exist.\n\n"
                               "To enable creating missing parent directories enable the "
                               "\"Create Parents\" option.");
            dialog->set_modal(true);
            dialog->show();

            return;
        }

        std::filesystem::create_directories(path);
        if (std::filesystem::is_directory(path))
        {
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }
        else
        {
            auto dialog = Gtk::AlertDialog::create("Mkdir Error");
            dialog->set_detail(
                std::format("Error creating parent directory\n\n{}", std::strerror(errno)));
            dialog->set_modal(true);
            dialog->show();

            return;
        }
    }
    else if (std::filesystem::exists(full_path)) // need to see broken symlinks
    {
        // overwrite
        if (std::filesystem::is_directory(full_path))
        {
            // just in case
            return;
        }

        if (!this->overwrite_)
        {
            auto dialog = Gtk::AlertDialog::create("Overwrite Existing File");
            dialog->set_detail("OVERWRITE WARNING\n\n"
                               "The file path exists. Overwrite existing file?");
            dialog->set_modal(true);
            dialog->set_buttons({"Cancel", "Confirm"});
            dialog->set_cancel_button(0);
            dialog->set_default_button(0);

            dialog->choose(
                *this,
                [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) mutable
                {
                    try
                    {
                        switch (const auto response = dialog->choose_finish(result))
                        {
                            case 0: // Cancel
                                this->overwrite_ = false;
                                break;
                            case 1: // Confirm
                                this->overwrite_ = true;
                                this->on_button_ok_clicked();
                                break;
                            default:
                                std::println(stderr, "Unexpected response: {}", response);
                                break;
                        }
                    }
                    catch (const Gtk::DialogError& err)
                    {
                        std::println(stderr, "Gtk::AlertDialog error: {}", err.what());
                    }
                    catch (const Glib::Error& err)
                    {
                        std::println(stderr, "Unexpected exception: {}", err.what());
                    }
                });

            return;
        }
    }

    if (copy || copy_target)
    { // copy task
        std::string source;
        std::string dest = full_path.string();
        if (copy || !this->is_link_)
        {
            source = this->full_path_.string();
        }
        else
        {
            const auto real_path = std::filesystem::read_symlink(this->full_path_);
            if (real_path == this->full_path_)
            {
                auto dialog = Gtk::AlertDialog::create("Copy Target Error");
                dialog->set_detail("Error determining link's target");
                dialog->set_modal(true);
                dialog->show();

                return;
            }
            source = real_path.string();
        }

        const auto buffer =
            glz::write_json(datatype::rename::response{.source = source,
                                                       .dest = dest,
                                                       .mode = datatype::rename::mode::copy,
                                                       .overwrite = this->overwrite_,
                                                       .settings = this->settings_});
        if (buffer)
        {
            std::println("{}", buffer.value());
        }
    }
    else if (link || link_target)
    { // link task
        std::string source;
        std::string dest;
        if (link || !this->is_link_)
        {
            source = this->full_path_.string();
        }
        else
        {
            const auto real_path = std::filesystem::read_symlink(this->full_path_);
            if (real_path == this->full_path_)
            {
                auto dialog = Gtk::AlertDialog::create("Link Target Error");
                dialog->set_detail("Error determining link's target");
                dialog->set_modal(true);
                dialog->show();

                return;
            }
            source = real_path.string();
        }
        dest = full_path.string();

        const auto buffer =
            glz::write_json(datatype::rename::response{.source = source,
                                                       .dest = dest,
                                                       .mode = datatype::rename::mode::link,
                                                       .overwrite = this->overwrite_,
                                                       .settings = this->settings_});
        if (buffer)
        {
            std::println("{}", buffer.value());
        }
    }
    else if (old_path != path)
    { // need move?
        std::string source = this->full_path_.string();
        std::string dest = full_path.string();

        const auto buffer =
            glz::write_json(datatype::rename::response{.source = source,
                                                       .dest = dest,
                                                       .mode = datatype::rename::mode::move,
                                                       .overwrite = this->overwrite_,
                                                       .settings = this->settings_});
        if (buffer)
        {
            std::println("{}", buffer.value());
        }
    }
    else
    { // rename (does overwrite)

        std::string source = this->full_path_.string();
        std::string dest = full_path.string();

        const auto buffer =
            glz::write_json(datatype::rename::response{.source = source,
                                                       .dest = dest,
                                                       .mode = datatype::rename::mode::rename,
                                                       .overwrite = this->overwrite_,
                                                       .settings = this->settings_});
        if (buffer)
        {
            std::println("{}", buffer.value());
        }
    }

    // fix warning on close
    // Finalizing gtkmm__GtkButton 0x51400003b9b0, but it still has children left:
    //    - GtkPopoverMenu 0x516000036e70
    this->context_menu_.unparent();

    this->close();
}

void
RenameDialog::on_button_cancel_clicked()
{
    // Sending a response on cancel to be able
    // to save any changed settings.

    const auto buffer =
        glz::write_json(datatype::rename::response{.source = "", // unused in this mode
                                                   .dest = "",   // unused in this mode
                                                   .mode = datatype::rename::mode::cancel,
                                                   .overwrite = false,
                                                   .settings = this->settings_});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    // fix warning on close
    // Finalizing gtkmm__GtkButton 0x51400003b9b0, but it still has children left:
    //    - GtkPopoverMenu 0x516000036e70
    this->context_menu_.unparent();

    this->close();
}

void
RenameDialog::on_button_revert_clicked()
{
    this->buf_full_path_->set_text(this->new_path_.c_str());
    this->input_full_name_.grab_focus();
}

void
RenameDialog::on_button_options_clicked()
{
    this->context_menu_.popup();
}

void
RenameDialog::on_move_change(Glib::RefPtr<Gtk::TextBuffer>& widget)
{
    for (auto& s : this->on_move_change_signals_)
    {
        s.block();
    }

    std::filesystem::path full_path;
    std::filesystem::path path;
    if (widget == this->buf_full_name_ || widget == this->buf_path_)
    {
        const std::string full_name = this->buf_full_name_->get_text(false);

        // update full_path
        path = this->buf_path_->get_text(false);
        if (path == ".")
        {
            path = this->full_path_.parent_path();
        }
        else if (path == "..")
        {
            path = this->full_path_.parent_path().parent_path();
        }

        if (path.is_absolute())
        {
            full_path = path / full_name;
        }
        else
        {
            full_path = this->full_path_.parent_path() / path / full_name;
        }
        this->buf_full_path_->set_text(full_path.c_str());
    }
    else // if (widget == this->buf_full_path_)
    {
        full_path = this->buf_full_path_->get_text(false);

        std::string full_name;
        if (!full_path.empty())
        {
            full_name = full_path.filename();
        }

        path = full_path.parent_path();
        if (path == ".")
        {
            path = this->full_path_.parent_path();
        }
        else if (path == "..")
        {
            path = this->full_path_.parent_path().parent_path();
        }
        else if (!path.is_absolute())
        {
            path = this->full_path_.parent_path() / path;
        }

        this->buf_full_name_->set_text(full_name.data());

        // update path
        this->buf_path_->set_text(path.c_str());

        if (!full_path.is_absolute())
        {
            // update full_path for tests below
            full_path = this->full_path_.parent_path() / full_path;
        }
    }

    // change relative path to absolute
    if (!path.is_absolute())
    {
        path = full_path.parent_path();
    }

    // logger::info<logger::domain::ptk>("path={}   full={}", path, full_path);

    // tests
    bool full_path_exists = false;
    bool full_path_exists_dir = false;
    bool full_path_same = false;
    bool path_missing = false;
    bool path_exists_file = false;
    bool is_move = false;

    if (full_path == this->full_path_)
    {
        full_path_same = true;
    }
    else
    {
        if (std::filesystem::exists(full_path))
        {
            full_path_exists = true;
            if (std::filesystem::is_directory(full_path))
            {
                full_path_exists_dir = true;
            }
        }
        else if (std::filesystem::exists(path))
        {
            if (!std::filesystem::is_directory(path))
            {
                path_exists_file = true;
            }
        }
        else
        {
            path_missing = true;
        }

        if (this->opt_move_.get_active())
        {
            is_move = path != this->old_path_;
        }
    }

    // clang-format off
    // logger::trace("TEST")
    // logger::trace( "  full_path_same {} {}", full_path_same, this->full_path_same);
    // logger::trace( "  full_path_exists {} {}", full_path_exists, this->full_path_exists);
    // logger::trace( "  full_path_exists_dir {} {}", full_path_exists_dir, this->full_path_exists_dir);
    // logger::trace( "  path_missing {} {}", path_missing, this->path_missing);
    // logger::trace( "  path_exists_file {} {}", path_exists_file, this->path_exists_file);
    // clang-format on

    // update display
    if (this->full_path_same_ != full_path_same || this->full_path_exists_ != full_path_exists ||
        this->full_path_exists_dir_ != full_path_exists_dir ||
        this->path_missing_ != path_missing || this->path_exists_file_ != path_exists_file ||
        this->mode_change_)
    {
        // state change
        this->full_path_exists_ = full_path_exists;
        this->full_path_exists_dir_ = full_path_exists_dir;
        this->path_missing_ = path_missing;
        this->path_exists_file_ = path_exists_file;
        this->full_path_same_ = full_path_same;
        this->mode_change_ = false;

        this->button_revert_.set_sensitive(!full_path_same);

        if (full_path_same)
        {
            this->button_next_.set_sensitive(this->opt_move_.get_active());

            this->label_full_path_.set_markup_with_mnemonic("<b>P_ath:</b>   <i>original</i>");
            this->label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>   <i>original</i>");
            this->label_path_.set_markup_with_mnemonic("<b>_Parent:</b>   <i>original</i>");
        }
        else if (full_path_exists_dir)
        {
            this->button_next_.set_sensitive(false);
            this->label_full_path_.set_markup_with_mnemonic(
                "<b>P_ath:</b>   <i>exists as directory</i>");
            this->label_full_name_.set_markup_with_mnemonic(
                "<b>_Filename:</b>   <i>exists as directory</i>");
            this->label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
        }
        else if (full_path_exists)
        {
            if (this->is_dir_)
            {
                this->button_next_.set_sensitive(false);
                this->label_full_path_.set_markup_with_mnemonic(
                    "<b>P_ath:</b>   <i>exists as file</i>");
                this->label_full_name_.set_markup_with_mnemonic(
                    "<b>_Filename:</b>   <i>exists as file</i>");
                this->label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
            }
            else
            {
                this->button_next_.set_sensitive(true);
                this->label_full_path_.set_markup_with_mnemonic(
                    "<b>P_ath:</b>   <i>* overwrite existing file</i>");
                this->label_full_name_.set_markup_with_mnemonic(
                    "<b>_Filename:</b>   <i>* overwrite existing file</i>");
                this->label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
            }
        }
        else if (path_exists_file)
        {
            this->button_next_.set_sensitive(false);
            this->label_full_path_.set_markup_with_mnemonic(
                "<b>P_ath:</b>   <i>parent exists as file</i>");
            this->label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>");
            this->label_path_.set_markup_with_mnemonic(
                "<b>_Parent:</b>   <i>parent exists as file</i>");
        }
        else if (path_missing)
        {
            this->button_next_.set_sensitive(true);
            this->label_full_path_.set_markup_with_mnemonic(
                "<b>P_ath:</b>   <i>* create parent</i>");
            this->label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>");
            this->label_path_.set_markup_with_mnemonic("<b>_Parent:</b>   <i>* create parent</i>");
        }
        else
        {
            this->button_next_.set_sensitive(true);
            this->label_full_path_.set_markup_with_mnemonic("<b>P_ath:</b>");
            this->label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>");
            this->label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
        }
    }

    if (is_move != this->is_move_)
    {
        this->is_move_ = is_move;
        if (this->opt_move_.get_active())
        {
            this->button_next_.set_label(is_move != 0 ? "_Move" : "_Rename");
        }
    }

    for (auto& s : this->on_move_change_signals_)
    {
        s.unblock();
    }
}

void
RenameDialog::select_input() noexcept
{
    auto start_iter = this->buf_full_name_->begin();
    auto end_iter = this->buf_full_name_->end();

    const std::string full_name = this->buf_full_name_->get_text(false);

    if (this->file_ && !this->file_->is_directory())
    {
        const auto [basename, ext, _] = vfs::utils::split_basename_extension(full_name);

        end_iter = buf_full_name_->get_iter_at_offset(basename.size());
    }

    this->buf_full_name_->select_range(start_iter, end_iter);
}

void
RenameDialog::on_opt_toggled()
{
    const bool move = this->opt_move_.get_active();
    const bool copy = this->opt_copy_.get_active();
    const bool link = this->opt_link_.get_active();
    const bool copy_target = this->opt_copy_target_.get_active();
    const bool link_target = this->opt_link_target_.get_active();

    std::string action;
    std::string btn_label;
    std::string desc;

    const std::string full_path = this->buf_full_path_->get_text(false);
    const auto new_path = std::filesystem::path(full_path).parent_path();

    const bool rename = (this->old_path_ == new_path || new_path == ".");

    if (move)
    {
        btn_label = rename ? "Rename" : "Move";
        action = "Move";
    }
    else if (copy)
    {
        btn_label = "C_opy";
        action = "Copy";
    }
    else if (link)
    {
        btn_label = "_Link";
        action = "Create Link To";
    }
    else if (copy_target)
    {
        btn_label = "C_opy";
        action = "Copy";
        desc = "Link Target";
    }
    else if (link_target)
    {
        btn_label = "_Link";
        action = "Create Link To";
        desc = "Target";
    }

    // Window Icon
    this->set_icon_name("gtk-edit");

    // title
    if (desc.empty())
    {
        desc = this->desc_;
    }
    this->set_title(std::format("{} {}", action, desc));

    if (!btn_label.empty())
    {
        this->button_next_.set_label(btn_label);
    }

    this->full_path_same_ = false;
    this->mode_change_ = true;
    this->on_move_change(this->buf_full_path_);
}

void
RenameDialog::on_toggled()
{
    bool someone_is_visible = false;

    // opts
    if (this->settings_.copy || this->clip_copy_)
    {
        this->opt_copy_.set_visible(true);
    }
    else
    {
        if (this->opt_copy_.get_active())
        {
            this->opt_move_.set_active(true);
        }
        this->opt_copy_.set_visible(false);
    }

    if (this->settings_.link)
    {
        this->opt_link_.set_visible(true);
    }
    else
    {
        if (this->opt_link_.get_active())
        {
            this->opt_move_.set_active(true);
        }
        this->opt_link_.set_visible(false);
    }

    if (this->settings_.copyt && this->is_link_)
    {
        this->opt_copy_target_.set_visible(true);
    }
    else
    {
        if (this->opt_copy_target_.get_active())
        {
            this->opt_move_.set_active(true);
        }
        this->opt_copy_target_.set_visible(false);
    }

    if (this->settings_.linkt && this->is_link_)
    {
        this->opt_link_target_.set_visible(true);
    }
    else
    {
        if (this->opt_link_target_.get_active())
        {
            this->opt_move_.set_active(true);
        }
        this->opt_link_target_.set_visible(false);
    }

    if (!this->opt_copy_.get_visible() && !this->opt_link_.get_visible() &&
        !this->opt_copy_target_.get_visible() && !this->opt_link_target_.get_visible())
    {
        this->opt_move_.set_visible(false);
    }
    else
    {
        this->opt_move_.set_visible(true);
    }

    // entries
    if (this->settings_.filename)
    {
        someone_is_visible = true;

        this->label_full_name_.set_visible(true);
        this->scroll_full_name_.set_visible(true);
    }
    else
    {
        this->label_full_name_.set_visible(false);
        this->scroll_full_name_.set_visible(false);
    }

    if (this->settings_.parent)
    {
        someone_is_visible = true;

        this->label_path_.set_visible(true);
        this->scroll_path_.set_visible(true);
    }
    else
    {
        this->label_path_.set_visible(false);
        this->scroll_path_.set_visible(false);
    }

    if (this->settings_.path)
    {
        someone_is_visible = true;

        this->label_full_path_.set_visible(true);
        this->scroll_full_path_.set_visible(true);
    }
    else
    {
        this->label_full_path_.set_visible(false);
        this->scroll_full_path_.set_visible(false);
    }

    if (!this->is_link_ && this->settings_.type)
    {
        this->hbox_type_.set_visible(true);
    }
    else
    {
        this->hbox_type_.set_visible(false);
    }

    if (this->is_link_ && this->settings_.target)
    {
        this->hbox_target_.set_visible(true);
    }
    else
    {
        this->hbox_target_.set_visible(false);
    }

    if (!someone_is_visible)
    {
        this->settings_.filename = true;
        this->on_toggled();
    }
}
