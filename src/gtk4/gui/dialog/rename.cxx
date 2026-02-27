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

#include <vector>

#include <cstring>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "gui/dialog/rename.hxx"

#include "vfs/file.hxx"

#include "vfs/utils/utils.hxx"

#include "logger.hxx"

gui::dialog::rename::rename(Gtk::ApplicationWindow& parent,
                            const std::shared_ptr<config::settings>& settings,
                            const std::filesystem::path& cwd,
                            const std::shared_ptr<vfs::file>& file,
                            const std::filesystem::path& destination, const bool clip_copy)
    : settings_(settings), file_(file)
{
    set_transient_for(parent);
    set_modal(true);

    set_size_request(800, 500);
    set_resizable(false);

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    set_child(box_);

    if (cwd.empty() || !std::filesystem::exists(cwd))
    {
        on_button_cancel_clicked();
    }

    const std::string_view original_filename = file_->name();

    bool target_missing = false;

    is_dir_ = file_->is_directory();
    is_link_ = file_->is_symlink();
    clip_copy_ = clip_copy;
    full_path_ = cwd / original_filename;
    if (!destination.empty())
    {
        new_path_ = std::filesystem::path() / destination / original_filename;
    }
    else
    {
        new_path_ = full_path_;
    }

    old_path_ = cwd;

    full_path_exists_ = false;
    full_path_exists_dir_ = false;
    full_path_same_ = false;
    path_missing_ = false;
    path_exists_file_ = false;
    is_move_ = false;

    // Dialog
    if (is_link_)
    {
        desc_ = "Link";
    }
    else if (is_dir_)
    {
        desc_ = "Directory";
    }
    else
    {
        desc_ = "File";
    }

    // Buttons
    button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_next_ = Gtk::Button("_Rename", true);
    button_next_.set_focus_on_click(false);
    button_cancel_ = Gtk::Button("Cancel", true);
    button_cancel_.set_focus_on_click(false);
    button_revert_ = Gtk::Button("Re_vert", true);
    button_revert_.set_focus_on_click(false);
    button_options_ = Gtk::Button("Opt_ions", true);
    button_options_.set_focus_on_click(false);

    button_next_.signal_clicked().connect([this]() { on_button_ok_clicked(); });
    button_cancel_.signal_clicked().connect([this]() { on_button_cancel_clicked(); });
    button_revert_.signal_clicked().connect([this]() { on_button_revert_clicked(); });
    button_options_.signal_clicked().connect([this]() { on_button_options_clicked(); });

    button_box_.set_halign(Gtk::Align::END);
    button_box_.append(button_options_);
    button_box_.append(button_revert_);
    button_box_.append(button_cancel_);
    button_box_.append(button_next_);

    // Entries

    // Type
    std::string type;
    label_type_.set_markup_with_mnemonic("<b>Type:</b>");
    if (is_link_)
    {
        try
        {
            const auto target_path = std::filesystem::read_symlink(full_path_);

            mime_type_ = target_path;
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
            mime_type_ = "inode/symlink";
            type = "symbolic link ( inode/symlink )";
        }
    }
    else if (file_)
    {
        mime_type_ = file_->mime_type()->type();
        type = std::format(" {} ( {} )", file_->mime_type()->description(), mime_type_);
    }
    else // create
    {
        mime_type_ = "?";
        type = mime_type_;
    }
    label_mime_.set_label(type);
    label_mime_.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    label_mime_.set_selectable(true);
    label_mime_.set_halign(Gtk::Align::START);
    label_mime_.set_valign(Gtk::Align::START);
    label_mime_.set_margin(5);
    label_type_.set_selectable(true);

    // Target
    if (is_link_)
    {
        label_target_.set_markup_with_mnemonic("<b>_Target:</b>");
        label_target_.set_halign(Gtk::Align::START);
        label_target_.set_valign(Gtk::Align::END);
        label_target_.set_mnemonic_widget(entry_target_);
        label_target_.set_selectable(true);
        entry_target_.set_hexpand(true);

        entry_target_.set_text(mime_type_.data());
        entry_target_.set_editable(false);

        { // Target Signals
            auto s = entry_target_.signal_changed().connect(
                sigc::bind(sigc::mem_fun(*this, &rename::on_move_change), buf_full_path_));
            on_move_change_signals_.push_back(s);
        }
    }

    // Filename
    label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>");
    label_full_name_.set_halign(Gtk::Align::START);
    label_full_name_.set_valign(Gtk::Align::START);
    label_full_name_.set_margin(4);
    label_full_name_.set_mnemonic_widget(input_full_name_);
    label_full_name_.set_selectable(true);
    buf_full_name_ = Gtk::TextBuffer::create();
    input_full_name_.set_buffer(buf_full_name_);
    input_full_name_.set_wrap_mode(Gtk::WrapMode::CHAR);
    input_full_name_.set_monospace(true);
    scroll_full_name_.set_child(input_full_name_);
    scroll_full_name_.set_expand(true);

    { // Filename Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &rename::on_key_press),
                                                     false);

        input_full_name_.add_controller(key_controller);

        auto s = buf_full_name_->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &rename::on_move_change), buf_full_name_));
        on_move_change_signals_.push_back(s);
    }

    // Parent
    label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
    label_path_.set_halign(Gtk::Align::START);
    label_path_.set_valign(Gtk::Align::START);
    label_path_.set_margin(4);
    label_path_.set_mnemonic_widget(input_path_);
    label_path_.set_selectable(true);
    buf_path_ = Gtk::TextBuffer::create();
    input_path_.set_buffer(buf_path_);
    input_path_.set_wrap_mode(Gtk::WrapMode::CHAR);
    input_path_.set_monospace(true);
    scroll_path_.set_child(input_path_);
    scroll_path_.set_expand(true);

    { // Parent Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &rename::on_key_press),
                                                     false);

        input_path_.add_controller(key_controller);

        auto s = buf_path_->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &rename::on_move_change), buf_path_));
        on_move_change_signals_.push_back(s);
    }

    // Path
    label_full_path_.set_markup_with_mnemonic("<b>P_ath:</b>");
    label_full_path_.set_halign(Gtk::Align::START);
    label_full_path_.set_valign(Gtk::Align::START);
    label_full_path_.set_margin(4);
    label_full_path_.set_mnemonic_widget(input_full_path_);
    label_full_path_.set_selectable(true);
    buf_full_path_ = Gtk::TextBuffer::create();
    buf_full_path_->set_text(new_path_.string());
    input_full_path_.set_buffer(buf_full_path_);
    input_full_path_.set_wrap_mode(Gtk::WrapMode::CHAR);
    input_full_path_.set_monospace(true);
    scroll_full_path_.set_child(input_full_path_);
    scroll_full_path_.set_expand(true);

    { // Path Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &rename::on_key_press),
                                                     false);

        input_full_path_.add_controller(key_controller);

        auto s = buf_full_path_->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &rename::on_move_change), buf_full_path_));
        on_move_change_signals_.push_back(s);
    }

    // Options
    opt_move_.set_label("Move");
    opt_copy_.set_label("Copy");
    opt_link_.set_label("Link");
    opt_copy_target_.set_label("Copy Target");
    opt_link_target_.set_label("Link Target");
    opt_copy_.set_group(opt_move_);
    opt_link_.set_group(opt_move_);
    opt_copy_target_.set_group(opt_move_);
    opt_link_target_.set_group(opt_move_);

    opt_move_.set_focus_on_click(false);
    opt_copy_.set_focus_on_click(false);
    opt_link_.set_focus_on_click(false);
    opt_copy_target_.set_focus_on_click(false);
    opt_link_target_.set_focus_on_click(false);

    opt_copy_target_.set_sensitive(is_link_ && !target_missing);
    opt_link_target_.set_sensitive(is_link_);

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

    context_menu_.set_menu_model(menu_model);
    context_menu_.set_parent(button_options_);
    // context_menu_.unparent();

    context_action_group_ = Gio::SimpleActionGroup::create();

    action_filename_ = Gio::SimpleAction::create("filename");
    action_filename_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.filename = !settings_->dialog.rename.filename;
            on_toggled();
        });
    context_action_group_->add_action(action_filename_);

    action_path_ = Gio::SimpleAction::create("path");
    action_path_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.path = !settings_->dialog.rename.path;
            on_toggled();
        });
    context_action_group_->add_action(action_path_);

    action_parent_ = Gio::SimpleAction::create("parent");
    action_parent_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.parent = !settings_->dialog.rename.parent;
            on_toggled();
        });
    context_action_group_->add_action(action_parent_);

    action_type_ = Gio::SimpleAction::create("type");
    action_type_->set_enabled(!is_link_);
    action_type_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.type = !settings_->dialog.rename.type;
            on_toggled();
        });
    context_action_group_->add_action(action_type_);

    action_target_ = Gio::SimpleAction::create("target");
    action_target_->set_enabled(is_link_);
    action_target_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.target = !settings_->dialog.rename.target;
            on_toggled();
        });
    context_action_group_->add_action(action_target_);

    // Option submenu
    action_copy_ = Gio::SimpleAction::create("copy");
    action_copy_->set_enabled(!clip_copy_);
    action_copy_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.copy = !settings_->dialog.rename.copy;
            on_toggled();
        });
    context_action_group_->add_action(action_copy_);

    action_link_ = Gio::SimpleAction::create("link");
    action_link_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.link = !settings_->dialog.rename.link;
            on_toggled();
        });
    context_action_group_->add_action(action_link_);

    action_copy_target_ = Gio::SimpleAction::create("copy_target");
    action_copy_target_->set_enabled(is_link_);
    action_copy_target_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.copyt = !settings_->dialog.rename.copyt;
            on_toggled();
        });
    context_action_group_->add_action(action_copy_target_);

    action_link_target_ = Gio::SimpleAction::create("link_target");
    action_link_target_->set_enabled(is_link_);
    action_link_target_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.linkt = !settings_->dialog.rename.linkt;
            on_toggled();
        });
    context_action_group_->add_action(action_link_target_);

    action_confirm_ = Gio::SimpleAction::create_bool("confirm", settings_->dialog.rename.confirm);
    action_confirm_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.confirm = !settings_->dialog.rename.confirm;
            action_confirm_->set_state(
                Glib::Variant<bool>::create(settings_->dialog.rename.confirm));
        });
    context_action_group_->add_action(action_confirm_);

    insert_action_group("app", context_action_group_);

    // Pack
    box_.set_margin(5);

    box_.append(label_full_name_);
    box_.append(scroll_full_name_);

    box_.append(label_path_);
    box_.append(scroll_path_);

    box_.append(label_full_path_);
    box_.append(scroll_full_path_);

    hbox_type_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 0);
    hbox_type_.append(label_type_);
    hbox_type_.append(label_mime_);
    box_.append(hbox_type_);

    hbox_target_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 0);
    hbox_target_.append(label_target_);
    hbox_target_.append(entry_target_);
    hbox_target_.set_hexpand(true);
    hbox_target_.set_margin(3);
    box_.append(hbox_target_);

    radio_button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 4);
    radio_button_box_.append(opt_move_);
    radio_button_box_.append(opt_copy_);
    radio_button_box_.append(opt_link_);
    radio_button_box_.append(opt_copy_target_);
    radio_button_box_.append(opt_link_target_);
    box_.append(radio_button_box_);
    box_.append(button_box_);

    // show
    set_visible(true);
    on_toggled();
    if (clip_copy_)
    {
        opt_copy_.set_active(true);
    }
    else
    {
        opt_move_.set_active(true);
    }

    opt_move_.signal_toggled().connect([this]() { on_opt_toggled(); });
    opt_copy_.signal_toggled().connect([this]() { on_opt_toggled(); });
    opt_link_.signal_toggled().connect([this]() { on_opt_toggled(); });
    opt_copy_target_.signal_toggled().connect([this]() { on_opt_toggled(); });
    opt_link_target_.signal_toggled().connect([this]() { on_opt_toggled(); });

    // init
    on_move_change(buf_full_path_);
    on_opt_toggled();

    // select filename text widget
    select_input();
    input_full_name_.grab_focus();
}

gui::dialog::rename::~rename()
{
    // fix warning on close
    // Finalizing gtkmm__GtkButton 0x51400003b9b0, but it still has children left:
    //    - GtkPopoverMenu 0x516000036e70
    context_menu_.unparent();
}

bool
gui::dialog::rename::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                                  Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        if (button_next_.get_sensitive())
        {
            on_button_ok_clicked();
        }
        return true;
    }
    if (keyval == GDK_KEY_Escape)
    {
        on_button_cancel_clicked();
    }
    return false;
}

void
gui::dialog::rename::on_button_ok_clicked()
{
    const std::string text = buf_full_path_->get_text(false);
    if (text.contains('\n') || text.contains("\\n"))
    {
        auto alert = Gtk::AlertDialog::create("Error");
        alert->set_detail("Path contains linefeeds");
        alert->set_modal(true);
        alert->show();

        return;
    }

    auto full_path = std::filesystem::path(text);
    if (!full_path.is_absolute())
    {
        // update full_path to absolute
        full_path = full_path_.parent_path() / full_path;
    }
    // const auto full_name = full_path.filename();
    const auto path = full_path.parent_path();
    const auto old_path = full_path_.parent_path();

    if (full_path_same_ || full_path == full_path_)
    {
        // not changed, proceed to next file
        signal_confirm().emit(rename_response{
            .source = "",
            .destination = "",
            .mode = rename_mode::skip,
            .overwrite = false,
        });

        close();
    }

    // determine job
    // const bool move = opt_move_.get_active();
    const bool copy = opt_copy_.get_active();
    const bool link = opt_link_.get_active();
    const bool copy_target = opt_copy_target_.get_active();
    const bool link_target = opt_link_target_.get_active();

    if (!std::filesystem::exists(path))
    {
        // create parent directory
        if (!settings_->dialog.rename.confirm)
        {
            auto alert = Gtk::AlertDialog::create("Create Parent Directory Error");
            alert->set_detail("The parent directory does not exist.\n\n"
                              "To enable creating missing parent directories enable the "
                              "\"Create Parents\" option.");
            alert->set_modal(true);
            alert->show();

            return;
        }

        std::filesystem::create_directories(path);
        if (std::filesystem::is_directory(path))
        {
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }
        else
        {
            auto alert = Gtk::AlertDialog::create("Mkdir Error");
            alert->set_detail(
                std::format("Error creating parent directory\n\n{}", std::strerror(errno)));
            alert->set_modal(true);
            alert->show();

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

        if (!overwrite_)
        {
            auto alert = Gtk::AlertDialog::create("Overwrite Existing File");
            alert->set_detail("OVERWRITE WARNING\n\n"
                              "The file path exists. Overwrite existing file?");
            alert->set_modal(true);
            alert->set_buttons({"Cancel", "Confirm"});
            alert->set_cancel_button(0);
            alert->set_default_button(0);

            alert->choose(
                *this,
                [this, alert](Glib::RefPtr<Gio::AsyncResult>& result) mutable
                {
                    try
                    {
                        switch (const auto response = alert->choose_finish(result))
                        {
                            case 0: // Cancel
                                overwrite_ = false;
                                break;
                            case 1: // Confirm
                                overwrite_ = true;
                                on_button_ok_clicked();
                                break;
                            default:
                                logger::error<logger::gui>("Unexpected response: {}", response);
                                break;
                        }
                    }
                    catch (const Gtk::DialogError& err)
                    {
                        logger::error<logger::gui>("Gtk::AlertDialog error: {}", err.what());
                    }
                    catch (const Glib::Error& err)
                    {
                        logger::error<logger::gui>("Unexpected exception: {}", err.what());
                    }
                });

            return;
        }
    }

    if (copy || copy_target)
    { // copy task
        std::string source;
        std::string destination = full_path.string();
        if (copy || !is_link_)
        {
            source = full_path_.string();
        }
        else
        {
            const auto real_path = std::filesystem::read_symlink(full_path_);
            if (real_path == full_path_)
            {
                auto alert = Gtk::AlertDialog::create("Copy Target Error");
                alert->set_detail("Error determining link's target");
                alert->set_modal(true);
                alert->show();

                return;
            }
            source = real_path.string();
        }

        signal_confirm().emit(rename_response{
            .source = source,
            .destination = destination,
            .mode = rename_mode::copy,
            .overwrite = overwrite_,
        });
    }
    else if (link || link_target)
    { // link task
        std::string source;
        std::string destination;
        if (link || !is_link_)
        {
            source = full_path_.string();
        }
        else
        {
            const auto real_path = std::filesystem::read_symlink(full_path_);
            if (real_path == full_path_)
            {
                auto alert = Gtk::AlertDialog::create("Link Target Error");
                alert->set_detail("Error determining link's target");
                alert->set_modal(true);
                alert->show();

                return;
            }
            source = real_path.string();
        }
        destination = full_path.string();

        signal_confirm().emit(rename_response{
            .source = source,
            .destination = destination,
            .mode = rename_mode::link,
            .overwrite = overwrite_,
        });
    }
    else if (old_path != path)
    { // need move?
        std::string source = full_path_.string();
        std::string destination = full_path.string();

        signal_confirm().emit(rename_response{
            .source = source,
            .destination = destination,
            .mode = rename_mode::move,
            .overwrite = overwrite_,
        });
    }
    else
    { // rename (does overwrite)

        std::string source = full_path_.string();
        std::string destination = full_path.string();

        signal_confirm().emit(rename_response{
            .source = source,
            .destination = destination,
            .mode = rename_mode::rename,
            .overwrite = overwrite_,
        });
    }

    close();
}

void
gui::dialog::rename::on_button_cancel_clicked()
{
    signal_confirm().emit(rename_response{
        .source = "",
        .destination = "",
        .mode = rename_mode::cancel,
        .overwrite = false,
    });

    close();
}

void
gui::dialog::rename::on_button_revert_clicked()
{
    buf_full_path_->set_text(new_path_.c_str());
    input_full_name_.grab_focus();
}

void
gui::dialog::rename::on_button_options_clicked()
{
    context_menu_.popup();
}

void
gui::dialog::rename::on_move_change(Glib::RefPtr<Gtk::TextBuffer>& widget)
{
    for (auto& s : on_move_change_signals_)
    {
        s.block();
    }

    std::filesystem::path full_path;
    std::filesystem::path path;
    if (widget == buf_full_name_ || widget == buf_path_)
    {
        const std::string full_name = buf_full_name_->get_text(false);

        // update full_path
        path = buf_path_->get_text(false);
        if (path == ".")
        {
            path = full_path_.parent_path();
        }
        else if (path == "..")
        {
            path = full_path_.parent_path().parent_path();
        }

        if (path.is_absolute())
        {
            full_path = path / full_name;
        }
        else
        {
            full_path = full_path_.parent_path() / path / full_name;
        }
        buf_full_path_->set_text(full_path.c_str());
    }
    else // if (widget == buf_full_path_)
    {
        full_path = buf_full_path_->get_text(false);

        std::string full_name;
        if (!full_path.empty())
        {
            full_name = full_path.filename();
        }

        path = full_path.parent_path();
        if (path == ".")
        {
            path = full_path_.parent_path();
        }
        else if (path == "..")
        {
            path = full_path_.parent_path().parent_path();
        }
        else if (!path.is_absolute())
        {
            path = full_path_.parent_path() / path;
        }

        buf_full_name_->set_text(full_name.data());

        // update path
        buf_path_->set_text(path.c_str());

        if (!full_path.is_absolute())
        {
            // update full_path for tests below
            full_path = full_path_.parent_path() / full_path;
        }
    }

    // change relative path to absolute
    if (!path.is_absolute())
    {
        path = full_path.parent_path();
    }

    // logger::info<logger::gui>("path={}   full={}", path, full_path);

    // tests
    bool full_path_exists = false;
    bool full_path_exists_dir = false;
    bool full_path_same = false;
    bool path_missing = false;
    bool path_exists_file = false;
    bool is_move = false;

    if (full_path == full_path_)
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

        if (opt_move_.get_active())
        {
            is_move = path != old_path_;
        }
    }

    // clang-format off
    // logger::trace("TEST")
    // logger::trace( "  full_path_same {} {}", full_path_same, full_path_same);
    // logger::trace( "  full_path_exists {} {}", full_path_exists, full_path_exists);
    // logger::trace( "  full_path_exists_dir {} {}", full_path_exists_dir, full_path_exists_dir);
    // logger::trace( "  path_missing {} {}", path_missing, path_missing);
    // logger::trace( "  path_exists_file {} {}", path_exists_file, path_exists_file);
    // clang-format on

    // update display
    if (full_path_same_ != full_path_same || full_path_exists_ != full_path_exists ||
        full_path_exists_dir_ != full_path_exists_dir || path_missing_ != path_missing ||
        path_exists_file_ != path_exists_file || mode_change_)
    {
        // state change
        full_path_exists_ = full_path_exists;
        full_path_exists_dir_ = full_path_exists_dir;
        path_missing_ = path_missing;
        path_exists_file_ = path_exists_file;
        full_path_same_ = full_path_same;
        mode_change_ = false;

        button_revert_.set_sensitive(!full_path_same);

        if (full_path_same)
        {
            button_next_.set_sensitive(opt_move_.get_active());

            label_full_path_.set_markup_with_mnemonic("<b>P_ath:</b>   <i>original</i>");
            label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>   <i>original</i>");
            label_path_.set_markup_with_mnemonic("<b>_Parent:</b>   <i>original</i>");
        }
        else if (full_path_exists_dir)
        {
            button_next_.set_sensitive(false);
            label_full_path_.set_markup_with_mnemonic("<b>P_ath:</b>   <i>exists as directory</i>");
            label_full_name_.set_markup_with_mnemonic(
                "<b>_Filename:</b>   <i>exists as directory</i>");
            label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
        }
        else if (full_path_exists)
        {
            if (is_dir_)
            {
                button_next_.set_sensitive(false);
                label_full_path_.set_markup_with_mnemonic("<b>P_ath:</b>   <i>exists as file</i>");
                label_full_name_.set_markup_with_mnemonic(
                    "<b>_Filename:</b>   <i>exists as file</i>");
                label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
            }
            else
            {
                button_next_.set_sensitive(true);
                label_full_path_.set_markup_with_mnemonic(
                    "<b>P_ath:</b>   <i>* overwrite existing file</i>");
                label_full_name_.set_markup_with_mnemonic(
                    "<b>_Filename:</b>   <i>* overwrite existing file</i>");
                label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
            }
        }
        else if (path_exists_file)
        {
            button_next_.set_sensitive(false);
            label_full_path_.set_markup_with_mnemonic(
                "<b>P_ath:</b>   <i>parent exists as file</i>");
            label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>");
            label_path_.set_markup_with_mnemonic("<b>_Parent:</b>   <i>parent exists as file</i>");
        }
        else if (path_missing)
        {
            button_next_.set_sensitive(true);
            label_full_path_.set_markup_with_mnemonic("<b>P_ath:</b>   <i>* create parent</i>");
            label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>");
            label_path_.set_markup_with_mnemonic("<b>_Parent:</b>   <i>* create parent</i>");
        }
        else
        {
            button_next_.set_sensitive(true);
            label_full_path_.set_markup_with_mnemonic("<b>P_ath:</b>");
            label_full_name_.set_markup_with_mnemonic("<b>_Filename:</b>");
            label_path_.set_markup_with_mnemonic("<b>_Parent:</b>");
        }
    }

    if (is_move != is_move_)
    {
        is_move_ = is_move;
        if (opt_move_.get_active())
        {
            button_next_.set_label(is_move != 0 ? "_Move" : "_Rename");
        }
    }

    for (auto& s : on_move_change_signals_)
    {
        s.unblock();
    }
}

void
gui::dialog::rename::select_input() noexcept
{
    auto start_iter = buf_full_name_->begin();
    auto end_iter = buf_full_name_->end();

    const std::string full_name = buf_full_name_->get_text(false);

    if (file_ && !file_->is_directory())
    {
        const auto [stem, extension] = vfs::utils::filename_stem_and_extension(full_name);

        end_iter = buf_full_name_->get_iter_at_offset(static_cast<std::int32_t>(stem.size()));
    }

    buf_full_name_->select_range(start_iter, end_iter);
}

void
gui::dialog::rename::on_opt_toggled()
{
    const bool move = opt_move_.get_active();
    const bool copy = opt_copy_.get_active();
    const bool link = opt_link_.get_active();
    const bool copy_target = opt_copy_target_.get_active();
    const bool link_target = opt_link_target_.get_active();

    std::string action;
    std::string btn_label;
    std::string desc;

    const std::string full_path = buf_full_path_->get_text(false);
    const auto new_path = std::filesystem::path(full_path).parent_path();

    const bool rename = (old_path_ == new_path || new_path == ".");

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
    set_icon_name("document-edit-symbolic");

    // title
    if (desc.empty())
    {
        desc = desc_;
    }
    set_title(std::format("{} {}", action, desc));

    if (!btn_label.empty())
    {
        button_next_.set_label(btn_label);
    }

    full_path_same_ = false;
    mode_change_ = true;
    on_move_change(buf_full_path_);
}

void
gui::dialog::rename::on_toggled()
{
    bool someone_is_visible = false;

    // opts
    if (settings_->dialog.rename.copy || clip_copy_)
    {
        opt_copy_.set_visible(true);
    }
    else
    {
        if (opt_copy_.get_active())
        {
            opt_move_.set_active(true);
        }
        opt_copy_.set_visible(false);
    }

    if (settings_->dialog.rename.link)
    {
        opt_link_.set_visible(true);
    }
    else
    {
        if (opt_link_.get_active())
        {
            opt_move_.set_active(true);
        }
        opt_link_.set_visible(false);
    }

    if (settings_->dialog.rename.copyt && is_link_)
    {
        opt_copy_target_.set_visible(true);
    }
    else
    {
        if (opt_copy_target_.get_active())
        {
            opt_move_.set_active(true);
        }
        opt_copy_target_.set_visible(false);
    }

    if (settings_->dialog.rename.linkt && is_link_)
    {
        opt_link_target_.set_visible(true);
    }
    else
    {
        if (opt_link_target_.get_active())
        {
            opt_move_.set_active(true);
        }
        opt_link_target_.set_visible(false);
    }

    if (!opt_copy_.get_visible() && !opt_link_.get_visible() && !opt_copy_target_.get_visible() &&
        !opt_link_target_.get_visible())
    {
        opt_move_.set_visible(false);
    }
    else
    {
        opt_move_.set_visible(true);
    }

    // entries
    if (settings_->dialog.rename.filename)
    {
        someone_is_visible = true;

        label_full_name_.set_visible(true);
        scroll_full_name_.set_visible(true);
    }
    else
    {
        label_full_name_.set_visible(false);
        scroll_full_name_.set_visible(false);
    }

    if (settings_->dialog.rename.parent)
    {
        someone_is_visible = true;

        label_path_.set_visible(true);
        scroll_path_.set_visible(true);
    }
    else
    {
        label_path_.set_visible(false);
        scroll_path_.set_visible(false);
    }

    if (settings_->dialog.rename.path)
    {
        someone_is_visible = true;

        label_full_path_.set_visible(true);
        scroll_full_path_.set_visible(true);
    }
    else
    {
        label_full_path_.set_visible(false);
        scroll_full_path_.set_visible(false);
    }

    if (!is_link_ && settings_->dialog.rename.type)
    {
        hbox_type_.set_visible(true);
    }
    else
    {
        hbox_type_.set_visible(false);
    }

    if (is_link_ && settings_->dialog.rename.target)
    {
        hbox_target_.set_visible(true);
    }
    else
    {
        hbox_target_.set_visible(false);
    }

    if (!someone_is_visible)
    {
        settings_->dialog.rename.filename = true;
        on_toggled();
    }
}
