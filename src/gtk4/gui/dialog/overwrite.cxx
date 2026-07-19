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

#include <filesystem>
#include <format>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "gui/dialog/overwrite.hxx"
#include "gui/dialog/widgets/button-box.hxx"

gui::dialog::overwrite::overwrite(Gtk::ApplicationWindow& parent,
                                  const std::shared_ptr<vfs::task_collision>& collision)
    : collision_(collision)
{
    is_copy_ = false; // TODO
    source_ = collision_->source;
    destination_ = collision_->destination;
    is_dir_ = std::filesystem::is_directory(source_);

    set_transient_for(parent);
    set_modal(true);

    set_size_request(800, 500);
    set_resizable(false);
    set_title(std::format("{} Exists", is_dir_ ? "Directory" : "Filename"));

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.set_margin(5);
    set_child(box_);

    ////////////////

    auto source_stat = ztd::statx::create(source_);
    auto source_size = source_stat->size();
    auto source_mtime = source_stat->mtime();
    auto destination_stat = ztd::statx::create(destination_);
    auto destination_size = destination_stat->size();
    auto destination_mtime = destination_stat->mtime();

    // Info Labels
    label_message_.set_markup(
        std::format("<b>{} already exists.</b> Please rename or select an action.",
                    is_dir_ ? "Directory" : "Filename"));
    label_message_.set_halign(Gtk::Align::START);
    label_message_.set_valign(Gtk::Align::START);
    label_message_.set_margin(4);

    label_from_.set_markup(
        std::format("{} from directory:\n{}\n    {}",
                    is_copy_ ? "Copying" : "Moving",
                    source_.parent_path(),
                    std::format("{} ({})",
                                std::chrono::floor<std::chrono::seconds>(source_mtime),
                                ztd::format_filesize(source_size))));
    label_from_.set_halign(Gtk::Align::START);
    label_from_.set_valign(Gtk::Align::START);
    label_from_.set_margin(4);

    label_to_.set_markup(
        std::format("To directory:\n{}\n    {}",
                    destination_.parent_path(),
                    std::format("{} ({})",
                                std::chrono::floor<std::chrono::seconds>(destination_mtime),
                                ztd::format_filesize(destination_size))));
    label_to_.set_halign(Gtk::Align::START);
    label_to_.set_valign(Gtk::Align::START);
    label_to_.set_margin(4);

    const auto size_status = (source_size == destination_size)  ? "Same Size"
                             : (source_size > destination_size) ? "Source Larger"
                                                                : "Destination Larger";
    const auto time_status = (source_mtime == destination_mtime)  ? "Same MTime"
                             : (source_mtime < destination_mtime) ? "Source Newer"
                                                                  : "Destination Newer";
    label_file_info_.set_markup(std::format("<b>{}, {}</b>", size_status, time_status));
    label_file_info_.set_halign(Gtk::Align::START);
    label_file_info_.set_valign(Gtk::Align::START);

    name_entry_ = Gtk::make_managed<gui::widget::TextEntry>(is_dir_ ? "<b>Directory:</b>"
                                                                    : "<b>Filename:</b>");
    name_entry_->set_text(source_.filename().string());

    { // Filename Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &overwrite::on_key_press),
                                                     false);

        name_entry_->get_text_view().add_controller(key_controller);

        name_entry_->signal_changed().connect([this]() { on_filename_update(); });
    }

    box_.append(label_message_);
    box_.append(label_from_);
    box_.append(label_to_);
    box_.append(label_file_info_);
    box_.append(*name_entry_);

    // Buttons //
    auto* buttons1 = gui::widget::ButtonBox::create({
        {"Rename All", [this] { on_button_rename_all_clicked(); }, &button_rename_all_},
        {"Rename Auto", [this] { on_button_rename_auto_clicked(); }, &button_rename_auto_},
        {"Rename", [this] { on_button_rename_clicked(); }, &button_rename_},
    });
    box_.append(*buttons1);

    auto* buttons2 = gui::widget::ButtonBox::create({
        {"Cancel", [this] { on_button_cancel_clicked(); }, &button_cancel_},
        {"Skip All", [this] { on_button_skip_all_clicked(); }, &button_skip_all_},
        {"Skip", [this] { on_button_skip_clicked(); }, &button_skip_},
        {"Pause", [this] { on_button_pause_clicked(); }, &button_pause_},
        {"Overwrite All", [this] { on_button_overwrite_all_clicked(); }, &button_overwrite_all_},
        {"Overwrite", [this] { on_button_overwrite_clicked(); }, &button_overwrite_},
    });
    box_.append(*buttons2);

    set_visible(true);
    on_filename_update();

    name_entry_->grab_focus();
}

bool
gui::dialog::overwrite::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                                     Gdk::ModifierType state) noexcept
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        if (button_rename_->get_sensitive())
        {
            on_button_rename_clicked();
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
gui::dialog::overwrite::on_button_rename_clicked() noexcept
{
    const std::string filename = name_entry_->get_text();
    const std::filesystem::path new_path = destination_.parent_path() / filename;

    collision_->resolved(collision_->task_id, vfs::collision_resolve::rename, new_path);

    close();
}

void
gui::dialog::overwrite::on_button_rename_auto_clicked() noexcept
{
    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("TODO");
    alert->set_modal(true);
    alert->show(*this);
}

void
gui::dialog::overwrite::on_button_rename_all_clicked() noexcept
{
    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("TODO");
    alert->set_modal(true);
    alert->show(*this);
}

void
gui::dialog::overwrite::on_button_overwrite_clicked() noexcept
{
    collision_->resolved(collision_->task_id, vfs::collision_resolve::overwrite, destination_);
    close();
}

void
gui::dialog::overwrite::on_button_overwrite_all_clicked() noexcept
{
    collision_->resolved(collision_->task_id, vfs::collision_resolve::overwrite_all, destination_);
    close();
}

void
gui::dialog::overwrite::on_button_pause_clicked() noexcept
{
    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("TODO");
    alert->set_modal(true);
    alert->show(*this);
}

void
gui::dialog::overwrite::on_button_skip_clicked() noexcept
{
    collision_->resolved(collision_->task_id, vfs::collision_resolve::skip, {});
    close();
}

void
gui::dialog::overwrite::on_button_skip_all_clicked() noexcept
{
    collision_->resolved(collision_->task_id, vfs::collision_resolve::skip_all, {});
    close();
}

void
gui::dialog::overwrite::on_button_cancel_clicked() noexcept
{
    collision_->resolved(collision_->task_id, vfs::collision_resolve::cancel, {});
    close();
}

void
gui::dialog::overwrite::on_filename_update() noexcept
{
    // Reset button sensitive
    button_rename_->set_sensitive(true);
    button_overwrite_->set_sensitive(true);
    button_overwrite_all_->set_sensitive(true);
    name_entry_->set_status_text("");

    const std::string filename = name_entry_->get_text();
    const std::filesystem::path new_path = destination_.parent_path() / filename;

    if (std::filesystem::exists(new_path))
    {
        button_rename_->set_sensitive(false);
    }
    else
    {
        button_overwrite_->set_sensitive(false);
        button_overwrite_all_->set_sensitive(false);
    }

    ///////////////////////

    if (filename.empty())
    {
        button_rename_->set_sensitive(false);
        button_overwrite_->set_sensitive(false);
        button_overwrite_all_->set_sensitive(false);
        name_entry_->set_status_text("<i>filename cannot be empty</i>");
    }

    // tests
    bool full_path_exists = false;
    bool full_path_exists_dir = false;

    if (std::filesystem::exists(new_path))
    {
        full_path_exists = true;
        if (std::filesystem::is_directory(new_path))
        {
            full_path_exists_dir = true;
        }
    }

    // clang-format off
    // logger::trace("TEST")
    // logger::trace( "  full_path_exists {} {}", full_path_exists, full_path_exists);
    // logger::trace( "  full_path_exists_dir {} {}", full_path_exists_dir, full_path_exists_dir);
    // clang-format on

    // update display
    if (full_path_exists)
    {
        // button_rename_->set_sensitive(false);
        // button_overwrite_->set_sensitive(false);
        // button_overwrite_all_->set_sensitive(false);

        if (full_path_exists_dir)
        {
            name_entry_->set_status_text("<i>exists as directory</i>");
        }
        else if (is_dir_)
        {
            name_entry_->set_status_text("<i>exists as file</i>");
        }
        else
        {
            name_entry_->set_status_text("<i>* overwrite existing file</i>");
        }
    }
}
