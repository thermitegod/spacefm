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

#include <array>
#include <chrono>
#include <filesystem>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "gui/dialog/media/metadata.hxx"
#include "gui/dialog/properties.hxx"
#include "gui/dialog/widgets/button-box.hxx"
#include "gui/dialog/widgets/checksum.hxx"
#include "gui/dialog/widgets/paste-button.hxx"

#include "vfs/file.hxx"

#include "vfs/utils/permissions.hxx"
#include "vfs/utils/utils.hxx"

class properties_grid : public Gtk::ScrolledWindow
{
  public:
    properties_grid()
    {
        set_expand(true);

        box_.set_orientation(Gtk::Orientation::VERTICAL);
        // box_.set_spacing(5);
        // box_.set_margin(5);

        set_child(box_);

        new_grid();
    }

    void
    new_grid() noexcept
    {
        grid_ = Gtk::make_managed<Gtk::Grid>();
        grid_->set_column_spacing(10);
        // grid_->set_row_spacing(5);
        grid_->set_margin(5);
        grid_->set_hexpand(true);
        // grid_->set_expand(true);

        box_.append(*grid_);
        current_row_ = 0;
    }

    void
    add_separator() noexcept
    {
        auto* separator = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
        separator->set_margin_top(6);
        separator->set_margin_bottom(6);

        box_.append(*separator);

        new_grid();
    }

    void
    add_row(Gtk::Widget& widget) noexcept
    {
        grid_->attach(widget, 0, current_row_);

        current_row_ += 1;
    }

    void
    add_item(std::string_view label, std::string_view description) noexcept
    {
        auto* item_label = Gtk::make_managed<Gtk::Label>(std::string(label));
        item_label->set_xalign(0.0f);
        item_label->set_valign(Gtk::Align::CENTER);

        auto* description_label = Gtk::make_managed<Gtk::Label>(std::string(description));
        description_label->set_xalign(0.0f);
        description_label->set_valign(Gtk::Align::CENTER);

        grid_->attach(*item_label, 0, current_row_);
        grid_->attach(*description_label, 1, current_row_);

        current_row_ += 1;
    }

    void
    add_item(std::string_view label, Gtk::Widget& widget) noexcept
    {
        auto* item_label = Gtk::make_managed<Gtk::Label>(std::string(label));
        item_label->set_xalign(0.0f);
        item_label->set_valign(Gtk::Align::CENTER);

        grid_->attach(*item_label, 0, current_row_);
        grid_->attach(widget, 1, current_row_);

        current_row_ += 1;
    }

    void
    add_item(Gtk::Widget& label, Gtk::Widget& widget) noexcept
    {
        grid_->attach(label, 0, current_row_);
        grid_->attach(widget, 1, current_row_);

        current_row_ += 1;
    }

    void
    add_check_button(std::string_view label, bool active, std::string_view button_text = "",
                     bool sensitive = false) noexcept
    {
        auto* cb = Gtk::make_managed<Gtk::CheckButton>(std::string(button_text));
        cb->set_sensitive(sensitive);
        cb->set_active(active);

        add_item(label, *cb);
    }

    void
    add_media_item(std::string_view key, std::string_view value) noexcept
    {
        auto* key_label = Gtk::make_managed<Gtk::Label>(std::string(key));
        key_label->set_xalign(0.0f);
        key_label->set_valign(Gtk::Align::START);
        key_label->add_css_class("dim-label");

        auto* value_label = Gtk::make_managed<Gtk::Label>(std::string(value));
        value_label->set_xalign(0.0f);
        value_label->set_selectable(true);
        value_label->set_wrap(true);
        value_label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
        value_label->set_hexpand(true);

        grid_->attach(*key_label, 0, current_row_);
        grid_->attach(*value_label, 1, current_row_);

        current_row_ += 1;
    }

    void
    add_entry(std::string_view label, std::string_view entry_text,
              const bool selectable = true) noexcept
    {
        auto* item_label = Gtk::make_managed<Gtk::Label>(std::string(label));
        item_label->set_xalign(0.0f);
        item_label->set_valign(Gtk::Align::CENTER);

        auto* entry = Gtk::make_managed<Gtk::Entry>();
        entry->set_margin(2);
        entry->set_text(std::string(entry_text));
        entry->set_editable(false);
        entry->set_hexpand(true);
        if (!selectable)
        {
            entry->set_can_focus(false);
            entry->set_sensitive(false);
        }

        grid_->attach(*item_label, 0, current_row_);
        grid_->attach(*entry, 1, current_row_);

        current_row_ += 1;
    }

  private:
    Gtk::Box box_;
    Gtk::Grid* grid_{nullptr};
    std::int32_t current_row_{0};
};

gui::dialog::properties::properties(Gtk::ApplicationWindow& parent,
                                    gui::dialog::properties::page page,
                                    const std::filesystem::path& cwd,
                                    const std::span<const std::shared_ptr<vfs::file>>& files,
                                    const std::shared_ptr<config::settings>& settings) noexcept
    : settings_(settings), files_(files.begin(), files.end()), cwd_(cwd)
{
    set_transient_for(parent);
    set_modal(false);

    set_size_request(480, 410);
    set_title("File Properties");
    set_resizable(false);

    // Content //

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.set_margin(5);

    box_.append(notebook_);

    init_file_info_tab();
    init_media_info_tab();
    init_checksum_tab();
    init_attributes_tab();
    init_permissions_tab();

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &properties::on_key_press),
                                                 false);
    add_controller(key_controller);

    // Buttons //
    auto* buttons = gui::widget::ButtonBox::create({
        {"Close", [this] { on_button_close_clicked(); }, &button_close_},
    });
    box_.append(*buttons);

    set_child(box_);

    set_visible(true);

    notebook_.set_current_page(std::to_underlying(page));
}

bool
gui::dialog::properties::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
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
gui::dialog::properties::on_button_close_clicked() noexcept
{
    if (calc_worker_)
    {
        calc_worker_->thread.request_stop();
        if (calc_worker_->thread.joinable())
        {
            calc_worker_->thread.detach();
        }
    }

    if (metadata_worker_)
    {
        metadata_worker_->thread.request_stop();
        if (metadata_worker_->thread.joinable())
        {
            metadata_worker_->thread.detach();
        }
    }

    close();
}

void
gui::dialog::properties::calc_worker::calc_total_size_of_files(
    const std::stop_token& stoken, const std::filesystem::path& path) noexcept
{
    if (stoken.stop_requested())
    {
        return;
    }

    std::error_code ec;
    const auto file_stat = ztd::lstat(path, ec);
    if (ec)
    {
        return;
    }

    total_size += file_stat.size().data();
    size_on_disk += file_stat.size_on_disk().data();

    if (!std::filesystem::is_directory(path) || !vfs::utils::check_directory_permissions(path))
    {
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec))
    {
        if (stoken.stop_requested())
        {
            return;
        }

        const auto stat = ztd::lstat(entry, ec);
        if (ec)
        {
            continue;
        }

        total_size += stat.size().data();
        size_on_disk += stat.size_on_disk().data();

        if (stat.is_directory())
        {
            total_count_dir += 1;
            dispatcher.emit();
        }
        else
        {
            total_count_file += 1;
        }
    }
}

void
gui::dialog::properties::calc_worker::calc_size(const std::stop_token& stoken) noexcept
{
    for (const auto& file : files)
    {
        if (stoken.stop_requested())
        {
            break;
        }

        if (file->is_directory())
        {
            total_count_dir += 1;
        }
        else
        {
            total_count_file += 1;
        }

        calc_total_size_of_files(stoken, file->path());

        dispatcher.emit();
    }
}

void
gui::dialog::properties::on_size_update() noexcept
{
    if (!calc_worker_)
    {
        return;
    }

    const std::uint64_t total_size = calc_worker_->total_size.load(std::memory_order_relaxed);
    const std::uint64_t size_on_disk = calc_worker_->size_on_disk.load(std::memory_order_relaxed);
    const std::uint64_t total_files =
        calc_worker_->total_count_file.load(std::memory_order_relaxed);
    const std::uint64_t total_dirs = calc_worker_->total_count_dir.load(std::memory_order_relaxed);

    total_size_label_.set_label(
        std::format("{} ( {:L} bytes )", vfs::utils::format_file_size(total_size), total_size));

    size_on_disk_label_.set_label(
        std::format("{} ( {:L} bytes )", vfs::utils::format_file_size(size_on_disk), size_on_disk));

    count_label_.set_label(std::format("{:L} files, {:L} directories", total_files, total_dirs));
}

void
gui::dialog::properties::init_file_info_tab() noexcept
{
    auto* page = Gtk::make_managed<properties_grid>();
    notebook_.append_page(*page, "Info");

    const auto& file = files_.front();
    const bool multiple_files = files_.size() > 1;

    if (multiple_files)
    {
        page->add_entry("File Name:", "( multiple files )", false);
    }
    else
    {
        if (file->is_symlink())
        {
            page->add_entry("Link Name:", file->name());
        }
        else if (file->is_directory())
        {
            page->add_entry("Directory:", file->name());
        }
        else
        {
            page->add_entry("File Name:", file->name());
        }
    }

    page->add_entry("Location:", std::format("{}", cwd_));

    if (file->is_symlink())
    {
        std::string target;
        try
        {
            target = std::filesystem::read_symlink(file->path());
            if (!std::filesystem::exists(target))
            {
                target = "( broken link )";
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            target = "( read link error )";
        }

        page->add_entry("Link Target:", target);
    }

    bool same_type = true;
    const auto initial_type = files_.front()->mime_type();
    for (const auto& selected_file : files_)
    {
        const auto type = selected_file->mime_type();
        if (type->type() != initial_type->type())
        {
            same_type = false;
            break;
        }
    }

    if (same_type)
    {
        const auto mime = file->mime_type();
        const auto file_type = std::format("{}\n{}", mime->description(), mime->type());
        page->add_item("Type:", file_type);
    }
    else
    {
        page->add_item("Type:", "( multiple types )");
    }

    total_size_label_.set_label("Calculating...");
    total_size_label_.set_xalign(0.0f);
    total_size_label_.set_yalign(0.5f);
    page->add_item("Total Size:", total_size_label_);

    size_on_disk_label_.set_label("Calculating...");
    size_on_disk_label_.set_xalign(0.0f);
    size_on_disk_label_.set_yalign(0.5f);
    page->add_item("Size On Disk:", size_on_disk_label_);

    count_label_.set_label("Calculating...");
    count_label_.set_xalign(0.0f);
    count_label_.set_yalign(0.5f);
    page->add_item("Count:", count_label_);

    bool need_calc_size = true;
    if (!multiple_files && !file->is_directory())
    {
        need_calc_size = false;

        total_size_label_.set_text(
            std::format("{}  ( {:L} bytes )", file->display_size(), file->size()));

        size_on_disk_label_.set_text(
            std::format("{}  ( {:L} bytes )", file->display_size_on_disk(), file->size_on_disk()));

        count_label_.set_text("1 file");
    }

    if (need_calc_size)
    {
        calc_worker_ = std::make_unique<calc_worker>(files_);

        calc_worker_->dispatcher.connect(sigc::mem_fun(*this, &properties::on_size_update));

        calc_worker_->thread =
            std::jthread([worker = calc_worker_.get()](const std::stop_token& stoken)
                         { worker->calc_size(stoken); });
    }

    if (multiple_files)
    {
        static constexpr std::string_view multiple_timestamps = "( multiple timestamps )";

        page->add_entry("Accessed:", multiple_timestamps);
        page->add_entry("Created:", multiple_timestamps);
        page->add_entry("Metadata:", multiple_timestamps);
        page->add_entry("Modified:", multiple_timestamps);
    }
    else
    {
        page->add_entry("Accessed:",
                        std::format("{}", std::chrono::floor<std::chrono::seconds>(file->atime())));
        page->add_entry("Created:",
                        std::format("{}", std::chrono::floor<std::chrono::seconds>(file->btime())));
        page->add_entry("Metadata:",
                        std::format("{}", std::chrono::floor<std::chrono::seconds>(file->ctime())));
        page->add_entry("Modified:",
                        std::format("{}", std::chrono::floor<std::chrono::seconds>(file->mtime())));
    }
}

void
gui::dialog::properties::metadata_worker::extract_metadata(const std::stop_token& stoken) noexcept
{
    if (stoken.stop_requested())
    {
        return;
    }

    const auto mime = file->mime_type();
    if (mime->is_image())
    {
        result = image_metadata(file->path());
    }
    else if (mime->is_video() || mime->is_audio())
    {
        result = audio_video_metadata(file->path());
    }

    if (!stoken.stop_requested())
    {
        dispatcher.emit();
    }
}

void
gui::dialog::properties::init_media_info_tab() noexcept
{
    auto* page = Gtk::make_managed<properties_grid>();
    notebook_.append_page(*page, "Media");

#if defined(HAVE_MEDIA)
    const auto& file = files_.front();
    const bool multiple_files = files_.size() > 1;
    if (!file->is_regular_file() || !file->mime_type()->is_media() || multiple_files)
    {
        page->set_visible(false);
        return;
    }

    metadata_worker_ = std::make_unique<metadata_worker>(file);

    metadata_worker_->dispatcher.connect(
        [this, page]()
        {
            if (!metadata_worker_ || metadata_worker_->result.empty())
            {
                page->set_visible(false);
                return;
            }

            for (const auto& item : metadata_worker_->result)
            {
                // logger::debug<logger::ptk>("description={}   value={}", item.description, item.value);
                page->add_media_item(item.description, item.value);
            }
        });

    metadata_worker_->thread =
        std::jthread([worker = metadata_worker_.get()](const std::stop_token& stoken)
                     { worker->extract_metadata(stoken); });
#else
    page->set_visible(false);
#endif
}
void
gui::dialog::properties::init_attributes_tab() noexcept
{
    auto* page = Gtk::make_managed<properties_grid>();
    notebook_.append_page(*page, "Attributes");

    const auto& file = files_.front();
    const bool multiple_files = files_.size() > 1;

    if (multiple_files)
    {
        bool is_same_value_compressed = true;
        bool is_same_value_immutable = true;
        bool is_same_value_append = true;
        bool is_same_value_nodump = true;
        bool is_same_value_encrypted = true;
        bool is_same_value_automount = true;
        bool is_same_value_mount_root = true;
        bool is_same_value_verity = true;
        bool is_same_value_dax = true;

        // The first file will get checked against itself
        for (const auto& f : files_)
        {
            if (is_same_value_compressed)
            {
                is_same_value_compressed = file->is_compressed() == f->is_compressed();
            }
            if (is_same_value_immutable)
            {
                is_same_value_immutable = file->is_immutable() == f->is_immutable();
            }
            if (is_same_value_append)
            {
                is_same_value_append = file->is_append() == f->is_append();
            }
            if (is_same_value_nodump)
            {
                is_same_value_nodump = file->is_nodump() == f->is_nodump();
            }
            if (is_same_value_encrypted)
            {
                is_same_value_encrypted = file->is_encrypted() == f->is_encrypted();
            }
            if (is_same_value_automount)
            {
                is_same_value_automount = file->is_automount() == f->is_automount();
            }
            if (is_same_value_mount_root)
            {
                is_same_value_mount_root = file->is_mount_root() == f->is_mount_root();
            }
            if (is_same_value_verity)
            {
                is_same_value_verity = file->is_verity() == f->is_verity();
            }
            if (is_same_value_dax)
            {
                is_same_value_dax = file->is_dax() == f->is_dax();
            }
        }

        static constexpr std::string_view selected_same_value = "( All Selected Files )";
        static constexpr std::string_view multiple_values = "( Multiple Values )";

        if (is_same_value_compressed)
        {
            page->add_check_button("Compressed:", file->is_compressed(), selected_same_value);
        }
        else
        {
            page->add_item("Compressed:", multiple_values);
        }

        if (is_same_value_immutable)
        {
            page->add_check_button("Immutable:", file->is_immutable(), selected_same_value);
        }
        else
        {
            page->add_item("Immutable:", multiple_values);
        }

        if (is_same_value_append)
        {
            page->add_check_button("Append:", file->is_append(), selected_same_value);
        }
        else
        {
            page->add_item("Append:", multiple_values);
        }

        if (is_same_value_nodump)
        {
            page->add_check_button("Nodump:", file->is_nodump(), selected_same_value);
        }
        else
        {
            page->add_item("Nodump:", multiple_values);
        }

        if (is_same_value_encrypted)
        {
            page->add_check_button("Encrypted:", file->is_encrypted(), selected_same_value);
        }
        else
        {
            page->add_item("Encrypted:", multiple_values);
        }

        if (is_same_value_automount)
        {
            page->add_check_button("Automount:", file->is_automount(), selected_same_value);
        }
        else
        {
            page->add_item("Automount:", multiple_values);
        }

        if (is_same_value_mount_root)
        {
            page->add_check_button("Mount Root:", file->is_mount_root(), selected_same_value);
        }
        else
        {
            page->add_item("Mount Root:", multiple_values);
        }

        if (is_same_value_verity)
        {
            page->add_check_button("Verity:", file->is_verity(), selected_same_value);
        }
        else
        {
            page->add_item("Verity:", multiple_values);
        }

        if (is_same_value_dax)
        {
            page->add_check_button("Dax:", file->is_dax(), selected_same_value);
        }
        else
        {
            page->add_item("Dax:", multiple_values);
        }
    }
    else
    {
        page->add_check_button("Compressed:", file->is_compressed());
        page->add_check_button("Immutable:", file->is_immutable());
        page->add_check_button("Append:", file->is_append());
        page->add_check_button("Nodump:", file->is_nodump());
        page->add_check_button("Encrypted:", file->is_encrypted());
        page->add_check_button("Automount:", file->is_automount());
        page->add_check_button("Mount Root:", file->is_mount_root());
        page->add_check_button("Verity:", file->is_verity());
        page->add_check_button("Dax:", file->is_dax());
    }
}

void
gui::dialog::properties::init_checksum_tab() noexcept
{
    auto* page = Gtk::make_managed<properties_grid>();
    notebook_.append_page(*page, "Checksums");

    const auto& file = files_.front();
    const bool multiple_files = files_.size() > 1;
    if (!file->is_regular_file() || multiple_files)
    {
        page->set_visible(false);
        return;
    }

    auto* instruction_label = Gtk::make_managed<Gtk::Label>();
    instruction_label->set_text("Copy and paste a checksum in the field below. A checksum is "
                                "usually provided by the website you downloaded this file from.");
    instruction_label->set_wrap(true);
    instruction_label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);

    page->add_row(*instruction_label);

    auto* entry_box = Gtk::make_managed<Gtk::Box>();
    entry_box->set_orientation(Gtk::Orientation::HORIZONTAL);
    auto* paste_button = Gtk::make_managed<gui::widget::PasteButton>();
    auto* entry = Gtk::make_managed<Gtk::Entry>();
    entry->set_hexpand(true);
    entry->set_placeholder_text("Expected checksum goes here");

    paste_button->signal_paste_text().connect([entry](std::string_view text)
                                              { entry->set_text(text.data()); });

    entry_box->append(*entry);
    entry_box->append(*paste_button);

    page->add_row(*entry_box);

    auto* status_label = Gtk::make_managed<Gtk::Label>();
    status_label->set_xalign(0.0f);
    status_label->set_margin_top(4);
    status_label->set_visible(false);
    page->add_row(*status_label);

    page->add_separator();

    auto calculated_checksums =
        std::make_shared<std::unordered_map<std::string_view, std::string>>();

    auto validate_input = [entry, status_label, calculated_checksums]()
    {
        const std::string current_input = entry->get_text();

        if (calculated_checksums->empty() || current_input.empty())
        {
            entry->remove_css_class("error");
            entry->remove_css_class("success");

            status_label->set_visible(false);
            return;
        }

        bool matches = false;
        for (const auto& [algo, hash] : *calculated_checksums)
        {
            if (hash == current_input)
            {
                matches = true;
                break;
            }
        }

        status_label->set_visible(true);
        if (matches)
        {
            status_label->set_text("Checksums match");

            entry->remove_css_class("error");
            entry->add_css_class("success");
        }
        else
        {
            status_label->set_text("Checksums do not match");

            entry->remove_css_class("success");
            entry->add_css_class("error");
        }
    };
    entry->signal_changed().connect(validate_input);

    auto add_checksum = [page, calculated_checksums, validate_input, file](std::string_view algo)
    {
        auto* checksum = Gtk::make_managed<gui::widget::Checksum>(algo, file->path());

        checksum->signal_calculated().connect(
            [algo, calculated_checksums, validate_input](std::string_view result)
            {
                (*calculated_checksums)[algo] = std::string(result);
                validate_input();
            });

        auto* label = Gtk::make_managed<Gtk::Label>(std::format("{}:", algo));
        label->set_xalign(0.0f);
        label->set_halign(Gtk::Align::END);

        page->add_item(*label, *checksum);
    };

    if (settings_->dialog.properties.hash_md5)
    {
        add_checksum("MD5");
    }
    if (settings_->dialog.properties.hash_sha1)
    {
        add_checksum("SHA-1");
    }
    if (settings_->dialog.properties.hash_sha256)
    {
        add_checksum("SHA-256");
    }
    if (settings_->dialog.properties.hash_sha512)
    {
        add_checksum("SHA-512");
    }
    if (settings_->dialog.properties.hash_sha_3_256)
    {
        add_checksum("SHA-3(256)");
    }
    if (settings_->dialog.properties.hash_sha_3_512)
    {
        add_checksum("SHA-3(512)");
    }
    if (settings_->dialog.properties.hash_blake2b_256)
    {
        add_checksum("BLAKE2b(256)");
    }
    if (settings_->dialog.properties.hash_blake2b_512)
    {
        add_checksum("BLAKE2b(512)");
    }
    if (settings_->dialog.properties.hash_whirlpool)
    {
        add_checksum("Whirlpool");
    }
    if (settings_->dialog.properties.hash_crc32)
    {
        add_checksum("CRC32");
    }
}

void
gui::dialog::properties::init_permissions_tab() noexcept
{
    auto* page = Gtk::make_managed<properties_grid>();
    notebook_.append_page(*page, "Permissions");

    const auto& file = files_.front();

    page->add_entry("Owner:", file->display_owner());
    page->add_entry("Group:", file->display_group());

    page->add_separator();

    auto* permission_grid = Gtk::make_managed<Gtk::Grid>();
    permission_grid->set_row_spacing(6);
    permission_grid->set_column_spacing(12);
    permission_grid->set_hexpand(true);

    auto permission_check = [&](std::string_view label, std::filesystem::perms mask)
    {
        auto* cb = Gtk::make_managed<Gtk::CheckButton>(std::string(label));
        cb->set_sensitive(false);
        cb->set_active(file->has_permissions(mask));
        return cb;
    };

    auto add_permission_row = [&](std::int32_t row,
                                  std::string_view label_text,
                                  std::filesystem::perms read_mask,
                                  std::filesystem::perms write_mask,
                                  std::filesystem::perms exec_mask,
                                  std::filesystem::perms special_mask,
                                  std::string_view special_label)
    {
        auto* row_label = Gtk::make_managed<Gtk::Label>(std::string(label_text));
        row_label->set_xalign(0.0f);
        row_label->set_valign(Gtk::Align::CENTER);

        permission_grid->attach(*row_label, 0, row);
        permission_grid->attach(*permission_check("Read", read_mask), 1, row);
        permission_grid->attach(*permission_check("Write", write_mask), 2, row);
        permission_grid->attach(*permission_check("Execute", exec_mask), 3, row);
        permission_grid->attach(*permission_check(special_label, special_mask), 4, row);
    };

    add_permission_row(0,
                       "Owner:",
                       std::filesystem::perms::owner_read,
                       std::filesystem::perms::owner_write,
                       std::filesystem::perms::owner_exec,
                       std::filesystem::perms::set_uid,
                       "Set UID");

    add_permission_row(1,
                       "Group:",
                       std::filesystem::perms::group_read,
                       std::filesystem::perms::group_write,
                       std::filesystem::perms::group_exec,
                       std::filesystem::perms::set_gid,
                       "Set GID");

    add_permission_row(2,
                       "Other:",
                       std::filesystem::perms::others_read,
                       std::filesystem::perms::others_write,
                       std::filesystem::perms::others_exec,
                       std::filesystem::perms::sticky_bit,
                       "Sticky Bit");

    page->add_row(*permission_grid);
}
