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
#include "gui/dialog/widgets/button-box.hxx"
#include "gui/dialog/widgets/radio-button-box.hxx"

#include "vfs/file.hxx"

#include "vfs/utils/utils.hxx"

#include "logger.hxx"

gui::dialog::rename::rename(Gtk::ApplicationWindow& parent,
                            const std::shared_ptr<config::settings>& settings,
                            const std::filesystem::path& cwd,
                            const std::span<const std::shared_ptr<vfs::file>>& files)
    : settings_(settings), files_(files.begin(), files.end()), cwd_(cwd)
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

    next_file();

    // Buttons //
    auto* buttons = gui::widget::ButtonBox::create({
        {"Options", [this] { on_button_options_clicked(); }, &button_options_},
        {"Revert", [this] { on_button_revert_clicked(); }, &button_revert_},
        {"Skip", [this] { on_button_skip_clicked(); }, &button_skip_},
        {"Cancel", [this] { on_button_cancel_clicked(); }, &button_cancel_},
        {"Rename", [this] { on_button_ok_clicked(); }, &button_ok_},
    });

    // Entries

    // Type
    label_mime_.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    label_mime_.set_selectable(true);
    label_mime_.set_halign(Gtk::Align::START);
    label_mime_.set_valign(Gtk::Align::START);
    label_mime_.set_margin(5);
    label_type_.set_selectable(true);

    // Target
    label_target_.set_markup_with_mnemonic("<b>_Target:</b>");
    label_target_.set_halign(Gtk::Align::START);
    label_target_.set_valign(Gtk::Align::END);
    label_target_.set_mnemonic_widget(entry_target_);
    label_target_.set_selectable(true);
    entry_target_.set_hexpand(true);
    entry_target_.set_editable(false);

    { // Target Signals
        entry_target_.signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &rename::on_move_change), false));
    }

    // Filename
    filename_entry_ = Gtk::make_managed<gui::widget::TextEntry>("<b>Filename:</b>");

    { // Filename Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &rename::on_key_press),
                                                     false);

        filename_entry_->get_text_view().add_controller(key_controller);

        filename_entry_->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &rename::on_move_change), false));
    }

    // Parent
    parent_entry_ = Gtk::make_managed<gui::widget::TextEntry>("<b>Parent:</b>");

    { // Parent Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &rename::on_key_press),
                                                     false);

        parent_entry_->get_text_view().add_controller(key_controller);

        parent_entry_->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &rename::on_move_change), false));
    }

    // Path
    path_entry_ = Gtk::make_managed<gui::widget::TextEntry>("<b>Path:</b>");

    { // Path Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &rename::on_key_press),
                                                     false);

        path_entry_->get_text_view().add_controller(key_controller);

        path_entry_->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &rename::on_move_change), true));
    }

    // Options //
    auto* radio_buttons = gui::widget::RadioButtonBox::create({
        {"Link Target", [this] { on_opt_toggled(); }, &opt_link_target_},
        {"Copy Target", [this] { on_opt_toggled(); }, &opt_copy_target_},
        {"Link", [this] { on_opt_toggled(); }, &opt_link_},
        {"Copy", [this] { on_opt_toggled(); }, &opt_copy_},
        {"Move", [this] { on_opt_toggled(); }, &opt_move_},
    });

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
    context_menu_.set_parent(*button_options_);
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
    action_type_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.type = !settings_->dialog.rename.type;
            on_toggled();
        });
    context_action_group_->add_action(action_type_);

    action_target_ = Gio::SimpleAction::create("target");
    action_target_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.target = !settings_->dialog.rename.target;
            on_toggled();
        });
    context_action_group_->add_action(action_target_);

    // Option submenu
    action_copy_ = Gio::SimpleAction::create("copy");
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
    action_copy_target_->signal_activate().connect(
        [this](const auto& /*value*/)
        {
            settings_->dialog.rename.copyt = !settings_->dialog.rename.copyt;
            on_toggled();
        });
    context_action_group_->add_action(action_copy_target_);

    action_link_target_ = Gio::SimpleAction::create("link_target");
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

    box_.append(*filename_entry_);
    box_.append(*parent_entry_);
    box_.append(*path_entry_);

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

    box_.append(*radio_buttons);
    box_.append(*buttons);

    // show
    update();
    set_visible(true);
    on_toggled();
    opt_move_->set_active(true);

    // init
    on_move_change(true);
    on_opt_toggled();

    // select filename text widget
    select_input();
    filename_entry_->grab_focus();
}

gui::dialog::rename::~rename()
{
    // fix warning on close
    // Finalizing gtkmm__GtkButton 0x51400003b9b0, but it still has children left:
    //    - GtkPopoverMenu 0x516000036e70
    context_menu_.unparent();
}

void
gui::dialog::rename::next_file() noexcept
{
    if (files_.empty())
    {
        on_button_cancel_clicked();
        return;
    }

    file_ = files_.front();
    files_.pop();
}

void
gui::dialog::rename::update() noexcept
{
    const auto original_filename = file_->name();

    bool target_missing = false;

    is_dir_ = file_->is_directory();
    is_link_ = file_->is_symlink();
    full_path_ = cwd_ / original_filename;
    new_path_ = full_path_;

    old_path_ = cwd_;

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
                type = std::format("Link-> {}", target_path);
            }
            else
            {
                type = std::format("!Link-> {} (missing)", target_path);
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

    // Target
    label_target_.set_visible(is_link_);
    entry_target_.set_text(mime_type_.data());

    // Path
    path_entry_->set_text(std::format("{}", new_path_), false);

    // Options
    opt_copy_target_->set_sensitive(is_link_ && !target_missing);
    opt_link_target_->set_sensitive(is_link_);

    // Context Menu
    action_type_->set_enabled(!is_link_);
    action_target_->set_enabled(is_link_);
    action_copy_target_->set_enabled(is_link_);
    action_link_target_->set_enabled(is_link_);

    // select filename text widget
    select_input();
    filename_entry_->grab_focus();
}

bool
gui::dialog::rename::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                                  Gdk::ModifierType state) noexcept
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        if (button_ok_->get_sensitive())
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
gui::dialog::rename::on_button_ok_clicked() noexcept
{
    const std::string text = path_entry_->get_text();
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
        next_file();
        update();
        return;
    }

    // determine job
    // const bool move = opt_move_->get_active();
    const bool copy = opt_copy_->get_active();
    const bool link = opt_link_->get_active();
    const bool copy_target = opt_copy_target_->get_active();
    const bool link_target = opt_link_target_->get_active();

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
        std::filesystem::path source;
        std::filesystem::path destination = full_path;
        if (copy || !is_link_)
        {
            source = full_path_;
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
            source = real_path;
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
        std::filesystem::path source;
        std::filesystem::path destination;
        if (link || !is_link_)
        {
            source = full_path_;
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
            source = real_path;
        }
        destination = full_path;

        signal_confirm().emit(rename_response{
            .source = source,
            .destination = destination,
            .mode = rename_mode::link,
            .overwrite = overwrite_,
        });
    }
    else if (old_path != path)
    { // need move?
        std::filesystem::path source = full_path_;
        std::filesystem::path destination = full_path;

        signal_confirm().emit(rename_response{
            .source = source,
            .destination = destination,
            .mode = rename_mode::move,
            .overwrite = overwrite_,
        });
    }
    else
    { // rename (does overwrite)

        std::filesystem::path source = full_path_;
        std::filesystem::path destination = full_path;

        signal_confirm().emit(rename_response{
            .source = source,
            .destination = destination,
            .mode = rename_mode::rename,
            .overwrite = overwrite_,
        });
    }

    // proceed to next file
    next_file();
    update();
}

void
gui::dialog::rename::on_button_cancel_clicked() noexcept
{
    close();
}

void
gui::dialog::rename::on_button_skip_clicked() noexcept
{
    next_file();
    update();
}

void
gui::dialog::rename::on_button_revert_clicked() noexcept
{
    path_entry_->set_text(std::format("{}", new_path_));
    filename_entry_->grab_focus();
}

void
gui::dialog::rename::on_button_options_clicked() noexcept
{
    context_menu_.popup();
}

void
gui::dialog::rename::on_move_change(const bool update_full_path) noexcept
{
    std::filesystem::path full_path;
    std::filesystem::path path;
    if (!update_full_path)
    {
        const std::string full_name = filename_entry_->get_text();

        // update full_path
        path = parent_entry_->get_text();
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
        path_entry_->set_text(std::format("{}", full_path), false);
    }
    else
    {
        full_path = path_entry_->get_text();

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

        filename_entry_->set_text(full_name, false);

        // update path
        parent_entry_->set_text(std::format("{}", path), false);

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

        if (opt_move_->get_active())
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

        button_revert_->set_sensitive(!full_path_same);

        if (full_path_same)
        {
            button_ok_->set_sensitive(opt_move_->get_active());

            filename_entry_->set_status_text("<i>original</i>");
            parent_entry_->set_status_text("<i>original</i>");
            path_entry_->set_status_text("<i>original</i>");
        }
        else if (full_path_exists_dir)
        {
            button_ok_->set_sensitive(false);

            filename_entry_->set_status_text("<i>exists as directory</i>");
            parent_entry_->set_status_text("");
            path_entry_->set_status_text("<i>exists as directory</i>");
        }
        else if (full_path_exists)
        {
            if (is_dir_)
            {
                button_ok_->set_sensitive(false);

                filename_entry_->set_status_text("<i>exists as file</i>");
                parent_entry_->set_status_text("");
                path_entry_->set_status_text("<i>exists as file</i>");
            }
            else
            {
                button_ok_->set_sensitive(true);

                filename_entry_->set_status_text("<i>* overwrite existing file</i>");
                parent_entry_->set_status_text("");
                path_entry_->set_status_text("<i>* overwrite existing file</i>");
            }
        }
        else if (path_exists_file)
        {
            button_ok_->set_sensitive(false);

            filename_entry_->set_status_text("");
            parent_entry_->set_status_text("<i>parent exists as file</i>");
            path_entry_->set_status_text("<i>parent exists as file</i>");
        }
        else if (path_missing)
        {
            button_ok_->set_sensitive(true);

            filename_entry_->set_status_text("");
            parent_entry_->set_status_text("<i>* create parent</i>");
            path_entry_->set_status_text("<i>* create parent</i>");
        }
        else
        {
            button_ok_->set_sensitive(true);

            filename_entry_->set_status_text("");
            parent_entry_->set_status_text("");
            path_entry_->set_status_text("");
        }
    }

    if (is_move != is_move_)
    {
        is_move_ = is_move;
        if (opt_move_->get_active())
        {
            button_ok_->set_label(is_move != 0 ? "_Move" : "_Rename");
        }
    }
}

void
gui::dialog::rename::select_input() noexcept
{
    const std::string full_name = filename_entry_->get_text();

    if (file_ && !file_->is_directory())
    {
        const auto [stem, extension] = vfs::utils::filename_stem_and_extension(full_name);

        filename_entry_->select_range(stem.size());
    }
    else
    {
        filename_entry_->select_range();
    }
}

void
gui::dialog::rename::on_opt_toggled() noexcept
{
    const bool move = opt_move_->get_active();
    const bool copy = opt_copy_->get_active();
    const bool link = opt_link_->get_active();
    const bool copy_target = opt_copy_target_->get_active();
    const bool link_target = opt_link_target_->get_active();

    std::string action;
    std::string btn_label;
    std::string desc;

    const std::string full_path = path_entry_->get_text();
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
        button_ok_->set_label(btn_label);
    }

    full_path_same_ = false;
    mode_change_ = true;
    on_move_change(true);
}

void
gui::dialog::rename::on_toggled() noexcept
{
    bool someone_is_visible = false;

    // opts
    if (settings_->dialog.rename.copy)
    {
        opt_copy_->set_visible(true);
    }
    else
    {
        if (opt_copy_->get_active())
        {
            opt_move_->set_active(true);
        }
        opt_copy_->set_visible(false);
    }

    if (settings_->dialog.rename.link)
    {
        opt_link_->set_visible(true);
    }
    else
    {
        if (opt_link_->get_active())
        {
            opt_move_->set_active(true);
        }
        opt_link_->set_visible(false);
    }

    if (settings_->dialog.rename.copyt && is_link_)
    {
        opt_copy_target_->set_visible(true);
    }
    else
    {
        if (opt_copy_target_->get_active())
        {
            opt_move_->set_active(true);
        }
        opt_copy_target_->set_visible(false);
    }

    if (settings_->dialog.rename.linkt && is_link_)
    {
        opt_link_target_->set_visible(true);
    }
    else
    {
        if (opt_link_target_->get_active())
        {
            opt_move_->set_active(true);
        }
        opt_link_target_->set_visible(false);
    }

    if (!opt_copy_->get_visible() && !opt_link_->get_visible() &&
        !opt_copy_target_->get_visible() && !opt_link_target_->get_visible())
    {
        opt_move_->set_visible(false);
    }
    else
    {
        opt_move_->set_visible(true);
    }

    // entries
    if (settings_->dialog.rename.filename)
    {
        someone_is_visible = true;

        filename_entry_->set_visible(true);
    }
    else
    {
        filename_entry_->set_visible(false);
    }

    if (settings_->dialog.rename.parent)
    {
        someone_is_visible = true;

        parent_entry_->set_visible(true);
    }
    else
    {
        parent_entry_->set_visible(false);
    }

    if (settings_->dialog.rename.path)
    {
        someone_is_visible = true;

        path_entry_->set_visible(true);
    }
    else
    {
        path_entry_->set_visible(false);
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
