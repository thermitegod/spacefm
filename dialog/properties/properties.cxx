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

#include <chrono>
#include <filesystem>
#include <print>
#include <string_view>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "vfs/utils/vfs-utils.hxx"
#include "vfs/vfs-file.hxx"

#include "datatypes.hxx"
#include "media/metadata.hxx"
#include "properties.hxx"

class PropertiesPage : public Gtk::Box
{
  public:
    PropertiesPage()
    {
        this->set_orientation(Gtk::Orientation::VERTICAL);
        this->set_margin(5);
        this->set_homogeneous(false);
        this->set_vexpand(true);
    }

    void
    add_row(const std::string_view left_item_name, const std::string_view right_item_name) noexcept
    {
        Gtk::Label left_item(left_item_name.data());
        Gtk::Label right_item(right_item_name.data());

        Gtk::Box left_box;
        Gtk::Box right_box;
        this->new_split_vboxes(left_box, right_box);
        left_box.append(left_item);
        right_box.append(right_item);
    }

    void
    add_row(const std::string_view left_item_name, Gtk::Widget& right_item) noexcept
    {
        Gtk::Label left_item(left_item_name.data());

        Gtk::Box left_box;
        Gtk::Box right_box;
        this->new_split_vboxes(left_box, right_box);
        left_box.append(left_item);
        right_box.append(right_item);
    }

    void
    add_row(Gtk::Label& left_item, Gtk::Widget& right_item) noexcept
    {
        Gtk::Box left_box;
        Gtk::Box right_box;
        this->new_split_vboxes(left_box, right_box);
        left_box.append(left_item);
        right_box.append(right_item);
    }

    void
    add_row(Gtk::Widget& item) noexcept
    {
        this->append(item);
    }

    void
    add_entry(const std::string_view left_item_name, const std::string_view text,
              const bool selectable = true) noexcept
    {
        Gtk::Label left_item(left_item_name.data());

        Gtk::Entry entry;
        entry.set_margin(2);
        entry.set_text(text.data());
        entry.set_editable(false);
        entry.set_hexpand(true);
        if (!selectable)
        {
            entry.set_can_focus(false);
            entry.set_sensitive(false);
        }

        Gtk::Box left_box;
        Gtk::Box right_box;
        this->new_split_vboxes(left_box, right_box);
        left_box.append(left_item);
        right_box.append(entry);
    }

  private:
    void
    new_split_vboxes(Gtk::Box& left_box, Gtk::Box& right_box) noexcept
    {
        left_box.set_spacing(6);
        left_box.set_homogeneous(false);

        right_box.set_spacing(6);
        right_box.set_homogeneous(false);

        Gtk::Box hbox = Gtk::Box(Gtk::Orientation::HORIZONTAL, 12);
        hbox.append(left_box);
        hbox.append(right_box);
        this->append(hbox);
    }
};

PropertiesDialog::PropertiesDialog(const std::string_view json_data)
{
    this->executor_ = global::runtime.thread_executor();

    const auto data = glz::read_json<datatype::properties::request>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    const auto& opts = data.value();
    this->cwd_ = opts.cwd;

    for (const auto& file : opts.files)
    {
        this->file_list_.push_back(vfs::file::create(file));
    }

    this->set_size_request(470, 400);
    this->set_title("File Properties");
    this->set_resizable(false);

    // Content //

    this->box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->box_.set_margin(5);

    this->box_.append(this->notebook_);

    this->init_file_info_tab();
    this->init_media_info_tab();
    this->init_attributes_tab();
    this->init_permissions_tab();

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(
        sigc::mem_fun(*this, &PropertiesDialog::on_key_press),
        false);
    this->add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_close_ = Gtk::Button("_Close", true);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_close_);
    this->box_.append(this->button_box_);

    this->button_close_.signal_clicked().connect(
        sigc::mem_fun(*this, &PropertiesDialog::on_button_close_clicked));

    this->set_child(this->box_);

    this->set_visible(true);

    this->notebook_.set_current_page(static_cast<std::int32_t>(opts.page));
}

bool
PropertiesDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Escape)
    {
        this->on_button_close_clicked();
    }
    return false;
}

void
PropertiesDialog::on_button_close_clicked()
{
    {
        auto guard = this->lock_.lock(this->executor_);
        this->abort_ = true;
    }
    this->condition_.notify_all();

    this->close();
}

// Recursively count total size of all files in the specified directory.
// If the path specified is a file, the size of the file is directly returned.
// cancel is used to cancel the operation. This function will check the value
// pointed by cancel in every iteration. If cancel is set to true, the
// calculation is cancelled.
void
PropertiesDialog::calc_total_size_of_files(const std::filesystem::path& path) noexcept
{
    if (this->abort_)
    {
        return;
    }

    std::error_code ec;
    const auto file_stat = ztd::lstat(path, ec);
    if (ec)
    {
        return;
    }

    this->total_size_ += file_stat.size();
    this->size_on_disk_ += file_stat.size_on_disk();

    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
    {
        if (this->abort_)
        {
            return;
        }

        const auto stat = ztd::lstat(entry);

        this->total_size_ += stat.size();
        this->size_on_disk_ += stat.size_on_disk();
        if (stat.is_directory())
        {
            ++this->total_count_dir_;
        }
        else
        {
            ++this->total_count_file_;
        }
    }
}

concurrencpp::result<void>
PropertiesDialog::calc_size() noexcept
{
    for (const auto& file : this->file_list_)
    {
        auto guard = co_await this->lock_.lock(this->executor_);

        if (this->abort_)
        {
            break;
        }

        if (file->is_directory())
        {
            ++this->total_count_dir_;
        }
        else
        {
            ++this->total_count_file_;
        }

        calc_total_size_of_files(file->path());

        on_update_labels();
    }

    co_return;
}

void
PropertiesDialog::on_update_labels() noexcept
{
    this->total_size_label_.set_label(std::format("{} ( {:L} bytes )",
                                                  vfs::utils::format_file_size(this->total_size_),
                                                  this->total_size_));

    this->size_on_disk_label_.set_label(
        std::format("{} ( {:L} bytes )",
                    vfs::utils::format_file_size(this->size_on_disk_),
                    this->size_on_disk_));

    this->count_label_.set_label(std::format("{:L} files, {:L} directories",
                                             this->total_count_file_,
                                             this->total_count_dir_));
}

void
PropertiesDialog::init_file_info_tab() noexcept
{
    // FIXME using spaces to align the right GtkWiget with the label
    // This works but should be fixed with an alignment solution

    auto page = PropertiesPage();

    const auto& file = this->file_list_.front();
    const bool multiple_files = this->file_list_.size() > 1;

    if (multiple_files)
    {
        page.add_entry("File Name:   ", "( multiple files )", false);
    }
    else
    {
        if (file->is_symlink())
        {
            page.add_entry("Link Name:   ", file->name());
        }
        else if (file->is_directory())
        {
            page.add_entry("Directory:   ", file->name());
        }
        else
        {
            page.add_entry("File Name:   ", file->name());
        }
    }

    if (file->is_directory())
    {
        page.add_entry("Location:    ", this->cwd_.parent_path().string());
    }
    else
    {
        page.add_entry("Location:    ", this->cwd_.string());
    }

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

        page.add_entry("Link Target: ", target);
    }

    bool same_type = true;
    const auto initial_type = this->file_list_.front()->mime_type();
    for (const auto& selected_file : this->file_list_)
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
        Gtk::Label type_label(file_type.data());
        type_label.set_xalign(0.0f);
        type_label.set_yalign(0.5f);

        page.add_row("Type:        ", type_label);
    }
    else
    {
        Gtk::Label type_label("( multiple types )");
        type_label.set_xalign(0.0f);
        type_label.set_yalign(0.5f);

        page.add_row("Type:        ", type_label);
    }

    this->total_size_label_.set_label("Calculating...");
    this->total_size_label_.set_xalign(0.0f);
    this->total_size_label_.set_yalign(0.5f);
    page.add_row("Total Size:  ", this->total_size_label_);

    this->size_on_disk_label_.set_label("Calculating...");
    this->size_on_disk_label_.set_xalign(0.0f);
    this->size_on_disk_label_.set_yalign(0.5f);
    page.add_row("Size On Disk:", this->size_on_disk_label_);

    this->count_label_.set_label("Calculating...");
    this->count_label_.set_xalign(0.0f);
    this->count_label_.set_yalign(0.5f);
    page.add_row("Count:       ", this->count_label_);

    bool need_calc_size = true;
    if (!multiple_files && !file->is_directory())
    {
        need_calc_size = false;

        this->total_size_label_.set_text(
            std::format("{}  ( {:L} bytes )", file->display_size(), file->size()));

        this->size_on_disk_label_.set_text(
            std::format("{}  ( {:L} bytes )", file->display_size_on_disk(), file->size_on_disk()));

        this->count_label_.set_text("1 file");
    }
    if (need_calc_size)
    {
        this->executor_result_ = this->executor_->submit([this] { return this->calc_size(); });
    }

    if (multiple_files)
    {
        page.add_entry("Accessed:    ", "( multiple timestamps )", false);
        page.add_entry("Created:     ", "( multiple timestamps )", false);
        page.add_entry("Metadata:    ", "( multiple timestamps )", false);
        page.add_entry("Modified:    ", "( multiple timestamps )", false);
    }
    else
    {
        page.add_entry("Accessed:    ",
                       std::format("{}", std::chrono::floor<std::chrono::seconds>(file->atime())));
        page.add_entry("Created:     ",
                       std::format("{}", std::chrono::floor<std::chrono::seconds>(file->btime())));
        page.add_entry("Metadata:    ",
                       std::format("{}", std::chrono::floor<std::chrono::seconds>(file->ctime())));
        page.add_entry("Modified:    ",
                       std::format("{}", std::chrono::floor<std::chrono::seconds>(file->mtime())));
    }

    auto tab_label = Gtk::Label("Info");
    this->notebook_.append_page(page, tab_label);
}

void
PropertiesDialog::init_media_info_tab() noexcept
{
#if defined(HAVE_MEDIA)
    auto page = PropertiesPage();

    const auto& file = this->file_list_.front();
    const bool multiple_files = this->file_list_.size() > 1;

    if (multiple_files)
    {
        return;
    }

    std::vector<metadata_data> metadata;
    if (file->mime_type()->is_image())
    {
        metadata = image_metadata(file->path());
    }
    else if (file->mime_type()->is_video() || file->mime_type()->is_audio())
    {
        metadata = audio_video_metadata(file->path());
    }

    if (metadata.empty())
    {
        return;
    }

    for (const auto& item : metadata)
    {
        // logger::debug<logger::domain::ptk>("description={}   value={}", item.description, item.value);
        Gtk::Label description_label(item.description.data());
        Gtk::Label value_label(item.value.data());
        value_label.set_xalign(1.0f);
        value_label.set_yalign(0.5f);

        page.add_row(description_label, value_label);
    }

    auto tab_label = Gtk::Label("Media");
    this->notebook_.append_page(page, tab_label);
#endif
}

void
PropertiesDialog::init_attributes_tab() noexcept
{
    auto page = PropertiesPage();

    const auto& selected_file = this->file_list_.front();
    const bool multiple_files = this->file_list_.size() > 1;

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
        for (const auto& file : this->file_list_)
        {
            if (is_same_value_compressed)
            {
                is_same_value_compressed = selected_file->is_compressed() == file->is_compressed();
            }
            if (is_same_value_immutable)
            {
                is_same_value_immutable = selected_file->is_immutable() == file->is_immutable();
            }
            if (is_same_value_append)
            {
                is_same_value_append = selected_file->is_append() == file->is_append();
            }
            if (is_same_value_nodump)
            {
                is_same_value_nodump = selected_file->is_nodump() == file->is_nodump();
            }
            if (is_same_value_encrypted)
            {
                is_same_value_encrypted = selected_file->is_encrypted() == file->is_encrypted();
            }
            if (is_same_value_automount)
            {
                is_same_value_automount = selected_file->is_automount() == file->is_automount();
            }
            if (is_same_value_mount_root)
            {
                is_same_value_mount_root = selected_file->is_mount_root() == file->is_mount_root();
            }
            if (is_same_value_verity)
            {
                is_same_value_verity = selected_file->is_verity() == file->is_verity();
            }
            if (is_same_value_dax)
            {
                is_same_value_dax = selected_file->is_dax() == file->is_dax();
            }
        }

        if (is_same_value_compressed)
        {
            Gtk::CheckButton cb_compressed(" ( All Selected Files ) ");
            cb_compressed.set_sensitive(false);
            cb_compressed.set_active(selected_file->is_compressed());

            page.add_row("Compressed: ", cb_compressed);
        }
        else
        {
            page.add_row("Compressed: ", " ( Multiple Values ) ");
        }

        if (is_same_value_immutable)
        {
            Gtk::CheckButton cb_immutable(" ( All Selected Files ) ");
            cb_immutable.set_sensitive(false);
            cb_immutable.set_active(selected_file->is_immutable());

            page.add_row("Immutable:  ", cb_immutable);
        }
        else
        {
            page.add_row("Immutable:  ", " ( Multiple Values ) ");
        }

        if (is_same_value_append)
        {
            Gtk::CheckButton cb_append(" ( All Selected Files ) ");
            cb_append.set_sensitive(false);
            cb_append.set_active(selected_file->is_append());

            page.add_row("Append:     ", cb_append);
        }
        else
        {
            page.add_row("Append:     ", " ( Multiple Values ) ");
        }

        if (is_same_value_nodump)
        {
            Gtk::CheckButton cb_nodump(" ( All Selected Files ) ");
            cb_nodump.set_sensitive(false);
            cb_nodump.set_active(selected_file->is_nodump());

            page.add_row("Nodump:     ", cb_nodump);
        }
        else
        {
            page.add_row("Nodump:     ", " ( Multiple Values ) ");
        }

        if (is_same_value_encrypted)
        {
            Gtk::CheckButton cb_encrypted(" ( All Selected Files ) ");
            cb_encrypted.set_sensitive(false);
            cb_encrypted.set_active(selected_file->is_encrypted());

            page.add_row("Encrypted:  ", cb_encrypted);
        }
        else
        {
            page.add_row("Encrypted:  ", " ( Multiple Values ) ");
        }

        if (is_same_value_automount)
        {
            Gtk::CheckButton cb_automount(" ( All Selected Files ) ");
            cb_automount.set_sensitive(false);
            cb_automount.set_active(selected_file->is_automount());

            page.add_row("Automount:  ", cb_automount);
        }
        else
        {
            page.add_row("Automount:  ", " ( Multiple Values ) ");
        }

        if (is_same_value_mount_root)
        {
            Gtk::CheckButton cb_mount_root(" ( All Selected Files ) ");
            cb_mount_root.set_sensitive(false);
            cb_mount_root.set_active(selected_file->is_mount_root());

            page.add_row("Mount Root: ", cb_mount_root);
        }
        else
        {
            page.add_row("Mount Root: ", " ( Multiple Values ) ");
        }

        if (is_same_value_verity)
        {
            Gtk::CheckButton cb_verity(" ( All Selected Files ) ");
            cb_verity.set_sensitive(false);
            cb_verity.set_active(selected_file->is_verity());

            page.add_row("Verity:     ", cb_verity);
        }
        else
        {
            page.add_row("Verity:     ", " ( Multiple Values ) ");
        }

        if (is_same_value_dax)
        {
            Gtk::CheckButton cb_dax(" ( All Selected Files ) ");
            cb_dax.set_sensitive(false);
            cb_dax.set_active(selected_file->is_dax());

            page.add_row("Dax:        ", cb_dax);
        }
        else
        {
            page.add_row("Dax:        ", " ( Multiple Values ) ");
        }
    }
    else
    {
        Gtk::CheckButton cb_compressed;
        cb_compressed.set_sensitive(false);
        cb_compressed.set_active(selected_file->is_compressed());
        page.add_row("Compressed: ", cb_compressed);

        Gtk::CheckButton cb_immutable;
        cb_immutable.set_sensitive(false);
        cb_immutable.set_active(selected_file->is_immutable());
        page.add_row("Immutable:  ", cb_immutable);

        Gtk::CheckButton cb_append;
        cb_append.set_sensitive(false);
        cb_append.set_active(selected_file->is_append());
        page.add_row("Append:     ", cb_append);

        Gtk::CheckButton cb_nodump;
        cb_nodump.set_sensitive(false);
        cb_nodump.set_active(selected_file->is_nodump());
        page.add_row("Nodump:     ", cb_nodump);

        Gtk::CheckButton cb_encrypted;
        cb_encrypted.set_sensitive(false);
        cb_encrypted.set_active(selected_file->is_encrypted());
        page.add_row("Encrypted:  ", cb_encrypted);

        Gtk::CheckButton cb_automount;
        cb_automount.set_sensitive(false);
        cb_automount.set_active(selected_file->is_automount());
        page.add_row("Automount:  ", cb_automount);

        Gtk::CheckButton cb_mount_root;
        cb_mount_root.set_sensitive(false);
        cb_mount_root.set_active(selected_file->is_mount_root());
        page.add_row("Mount Root: ", cb_mount_root);

        Gtk::CheckButton cb_verity;
        cb_verity.set_sensitive(false);
        cb_verity.set_active(selected_file->is_verity());
        page.add_row("Verity:     ", cb_verity);

        Gtk::CheckButton cb_dax;
        cb_dax.set_sensitive(false);
        cb_dax.set_active(selected_file->is_dax());
        page.add_row("Dax:        ", cb_dax);
    }

    auto tab_label = Gtk::Label("Attributes");
    this->notebook_.append_page(page, tab_label);
}

void
PropertiesDialog::init_permissions_tab() noexcept
{
    auto page = PropertiesPage();

    const auto& selected_file = this->file_list_.front();

    // Owner
    page.add_entry("Owner:", selected_file->display_owner());

    // Group
    page.add_entry("Group:", selected_file->display_group());

    // Permissions

    Gtk::Grid grid;
    grid.set_row_spacing(5);
    grid.set_column_spacing(5);

    // Create the labels for the first column
    Gtk::Label label_perm_owner("Owner:");
    Gtk::Label label_perm_group("Group:");
    Gtk::Label label_perm_other("Other:");

    grid.attach(label_perm_owner, 0, 0, 1, 1);
    grid.attach(label_perm_group, 0, 1, 1, 1);
    grid.attach(label_perm_other, 0, 2, 1, 1);

    // Create the permissions checked buttons

    // Owner
    Gtk::CheckButton check_button_owner_read("Read");
    Gtk::CheckButton check_button_owner_write("Write");
    Gtk::CheckButton check_button_owner_exec("Execute");
    check_button_owner_read.set_sensitive(false);
    check_button_owner_write.set_sensitive(false);
    check_button_owner_exec.set_sensitive(false);
    grid.attach(check_button_owner_read, 1, 0, 1, 1);
    grid.attach(check_button_owner_write, 2, 0, 1, 1);
    grid.attach(check_button_owner_exec, 3, 0, 1, 1);

    // Group
    Gtk::CheckButton check_button_group_read("Read");
    Gtk::CheckButton check_button_group_write("Write");
    Gtk::CheckButton check_button_group_exec("Execute");
    check_button_group_read.set_sensitive(false);
    check_button_group_write.set_sensitive(false);
    check_button_group_exec.set_sensitive(false);
    grid.attach(check_button_group_read, 1, 1, 1, 1);
    grid.attach(check_button_group_write, 2, 1, 1, 1);
    grid.attach(check_button_group_exec, 3, 1, 1, 1);

    // Other
    Gtk::CheckButton check_button_others_read("Read");
    Gtk::CheckButton check_button_others_write("Write");
    Gtk::CheckButton check_button_others_exec("Execute");
    check_button_others_read.set_sensitive(false);
    check_button_others_write.set_sensitive(false);
    check_button_others_exec.set_sensitive(false);
    grid.attach(check_button_others_read, 1, 2, 1, 1);
    grid.attach(check_button_others_write, 2, 2, 1, 1);
    grid.attach(check_button_others_exec, 3, 2, 1, 1);

    // Special - added at the end of the above
    Gtk::CheckButton check_button_set_uid("Set UID");
    Gtk::CheckButton check_button_set_gid("Set GID");
    Gtk::CheckButton check_button_sticky_bit("Sticky Bit");
    check_button_set_uid.set_sensitive(false);
    check_button_set_gid.set_sensitive(false);
    check_button_sticky_bit.set_sensitive(false);
    grid.attach(check_button_set_uid, 4, 0, 1, 1);
    grid.attach(check_button_set_gid, 4, 1, 1, 1);
    grid.attach(check_button_sticky_bit, 4, 2, 1, 1);

    // Set/unset the permission

    const auto permissions = std::filesystem::symlink_status(selected_file->path()).permissions();

    // Owner
    if ((permissions & std::filesystem::perms::owner_read) != std::filesystem::perms::none)
    {
        check_button_owner_read.set_active(true);
    }
    if ((permissions & std::filesystem::perms::owner_write) != std::filesystem::perms::none)
    {
        check_button_owner_write.set_active(true);
    }
    if ((permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none)
    {
        check_button_owner_exec.set_active(true);
    }

    // Group
    if ((permissions & std::filesystem::perms::group_read) != std::filesystem::perms::none)
    {
        check_button_group_read.set_active(true);
    }
    if ((permissions & std::filesystem::perms::group_write) != std::filesystem::perms::none)
    {
        check_button_group_write.set_active(true);
    }
    if ((permissions & std::filesystem::perms::group_exec) != std::filesystem::perms::none)
    {
        check_button_group_exec.set_active(true);
    }

    // Other
    if ((permissions & std::filesystem::perms::others_read) != std::filesystem::perms::none)
    {
        check_button_others_read.set_active(true);
    }
    if ((permissions & std::filesystem::perms::others_write) != std::filesystem::perms::none)
    {
        check_button_others_write.set_active(true);
    }
    if ((permissions & std::filesystem::perms::others_exec) != std::filesystem::perms::none)
    {
        check_button_others_exec.set_active(true);
    }

    // Special
    if ((permissions & std::filesystem::perms::set_uid) != std::filesystem::perms::none)
    {
        check_button_set_uid.set_active(true);
    }
    if ((permissions & std::filesystem::perms::set_gid) != std::filesystem::perms::none)
    {
        check_button_set_gid.set_active(true);
    }
    if ((permissions & std::filesystem::perms::sticky_bit) != std::filesystem::perms::none)
    {
        check_button_sticky_bit.set_active(true);
    }

    page.add_row(grid);

    auto tab_label = Gtk::Label("Permissions");
    this->notebook_.append_page(page, tab_label);
}
