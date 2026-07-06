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

    // Filename Text Box
    name_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    label_name_.set_markup(is_dir_ ? "<b>Directory:</b>" : "<b>Filename:</b>");
    label_name_.set_halign(Gtk::Align::START);
    label_name_.set_valign(Gtk::Align::START);
    label_name_.set_margin(4);
    label_name_.set_mnemonic_widget(input_name_);
    label_name_.set_selectable(false);
    label_name_state_.set_margin_start(10);
    label_name_state_.set_selectable(false);
    name_box_.append(label_name_);
    name_box_.append(label_name_state_);
    buf_name_ = Gtk::TextBuffer::create();
    buf_name_->set_text(source_.filename().string());
    input_name_.set_buffer(buf_name_);
    input_name_.set_wrap_mode(Gtk::WrapMode::CHAR);
    input_name_.set_monospace(true);
    scroll_name_.set_child(input_name_);
    scroll_name_.set_expand(true);

    { // Filename Signals
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &overwrite::on_key_press),
                                                     false);

        input_name_.add_controller(key_controller);

        auto s = buf_name_->signal_changed().connect([this]() { on_filename_update(); });
        on_filename_update_signals_.push_back(s);
    }

    // Buttons
    button_rename_ = Gtk::Button("Rename", true);
    button_rename_.set_focus_on_click(false);
    button_rename_.signal_clicked().connect([this]() { on_button_rename_clicked(); });

    button_rename_auto_ = Gtk::Button("Rename Auto", true);
    button_rename_auto_.set_focus_on_click(false);
    button_rename_auto_.signal_clicked().connect([this]() { on_button_rename_auto_clicked(); });

    button_rename_all_ = Gtk::Button("Rename All", true);
    button_rename_all_.set_focus_on_click(false);
    button_rename_all_.signal_clicked().connect([this]() { on_button_rename_all_clicked(); });

    button_box1_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_box1_.set_halign(Gtk::Align::END);
    button_box1_.append(button_rename_);
    button_box1_.append(button_rename_auto_);
    button_box1_.append(button_rename_all_);

    button_overwrite_ = Gtk::Button("Overwrite", true);
    button_overwrite_.set_focus_on_click(false);
    button_overwrite_.signal_clicked().connect([this]() { on_button_overwrite_clicked(); });

    button_overwrite_all_ = Gtk::Button("Overwrite All", true);
    button_overwrite_all_.set_focus_on_click(false);
    button_overwrite_all_.signal_clicked().connect([this]() { on_button_overwrite_all_clicked(); });

    button_pause_ = Gtk::Button("Pause", true);
    button_pause_.set_focus_on_click(false);
    button_pause_.signal_clicked().connect([this]() { on_button_pause_clicked(); });

    button_skip_ = Gtk::Button("Skip", true);
    button_skip_.set_focus_on_click(false);
    button_skip_.signal_clicked().connect([this]() { on_button_skip_clicked(); });

    button_skip_all_ = Gtk::Button("Skip All", true);
    button_skip_all_.set_focus_on_click(false);
    button_skip_all_.signal_clicked().connect([this]() { on_button_skip_all_clicked(); });

    button_cancel_ = Gtk::Button("Cancel", true);
    button_cancel_.set_focus_on_click(false);
    button_cancel_.signal_clicked().connect([this]() { on_button_cancel_clicked(); });

    button_box2_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_box2_.set_halign(Gtk::Align::END);
    button_box2_.append(button_overwrite_);
    button_box2_.append(button_overwrite_all_);
    button_box2_.append(button_pause_);
    button_box2_.append(button_skip_);
    button_box2_.append(button_skip_all_);
    button_box2_.append(button_cancel_);

    box_.append(label_message_);
    box_.append(label_from_);
    box_.append(label_to_);
    box_.append(label_file_info_);
    box_.append(name_box_);
    box_.append(scroll_name_);
    box_.append(button_box1_);
    box_.append(button_box2_);

    set_visible(true);
    on_filename_update();
    input_name_.grab_focus();
}

bool
gui::dialog::overwrite::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                                     Gdk::ModifierType state) noexcept
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        if (button_rename_.get_sensitive())
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
    const std::string filename = buf_name_->get_text(false);
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
    for (auto& s : on_filename_update_signals_)
    {
        s.block();
    }

    // Reset button sensitive
    button_rename_.set_sensitive(true);
    button_overwrite_.set_sensitive(true);
    button_overwrite_all_.set_sensitive(true);
    label_name_state_.set_text("");

    const std::string filename = buf_name_->get_text(false);
    const std::filesystem::path new_path = destination_.parent_path() / filename;

    if (std::filesystem::exists(new_path))
    {
        button_rename_.set_sensitive(false);
    }
    else
    {
        button_overwrite_.set_sensitive(false);
        button_overwrite_all_.set_sensitive(false);
    }

    ///////////////////////

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
        // button_rename_.set_sensitive(false);
        // button_overwrite_.set_sensitive(false);
        // button_overwrite_all_.set_sensitive(false);

        if (full_path_exists_dir)
        {
            label_name_state_.set_markup("<i>exists as directory</i>");
        }
        else if (is_dir_)
        {
            label_name_state_.set_markup("<i>exists as file</i>");
        }
        else
        {
            label_name_state_.set_markup("<i>* overwrite existing file</i>");
        }
    }

    for (auto& s : on_filename_update_signals_)
    {
        s.unblock();
    }
}
