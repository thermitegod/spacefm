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

#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "vfs/dir.hxx"
#include "vfs/file.hxx"

namespace gui
{
class files final : public Gtk::GridView
{
  public:
    files(const std::shared_ptr<config::settings>& settings);
    ~files();

    [[nodiscard]] std::shared_ptr<vfs::file> get_item(u32 position) const noexcept;
    [[nodiscard]] std::vector<std::shared_ptr<vfs::file>> selected_files() const noexcept;

    void update() noexcept;

    void enable_thumbnails() noexcept;
    void disable_thumbnails() noexcept;

    void set_dir(const std::shared_ptr<vfs::dir>& dir, const config::sorting& sorting) noexcept;
    void set_pattern(const std::string_view pattern) noexcept;
    void set_thumbnail_size(const vfs::file::thumbnail_size size) noexcept;
    void set_sorting(const config::sorting& sorting, bool full_update = false) noexcept;

    void select_all() const noexcept;
    void unselect_all() const noexcept;
    void select_file(const std::filesystem::path& filename,
                     const bool unselect_others = true) const noexcept;
    void select_files(const std::span<const std::filesystem::path> select_filenames) const noexcept;
    void unselect_file(const std::filesystem::path& filename) const noexcept;
    void select_pattern(const std::string_view search_key = "") noexcept;
    void invert_selection() noexcept;

  private:
    class ModelColumns : public Glib::Object
    {
      public:
        std::shared_ptr<vfs::file> file;
        // Only used if file is a directory
        Glib::RefPtr<Gtk::DropTarget> drop_target;

        static Glib::RefPtr<ModelColumns>
        create(const std::shared_ptr<vfs::file>& file)
        {
            return Glib::make_refptr_for_instance<ModelColumns>(new ModelColumns(file));
        }

      protected:
        ModelColumns(const std::shared_ptr<vfs::file>& file) : file(file) {}

      public:
        [[nodiscard]] auto
        signal_thumbnail_loaded() const noexcept
        {
            return signal_thumbnail_loaded_;
        }

        [[nodiscard]] auto
        signal_changed() const noexcept
        {
            return signal_changed_;
        }

      private:
        sigc::signal<void()> signal_thumbnail_loaded_;
        sigc::signal<void()> signal_changed_;
    }; // ModelColumns

    std::shared_ptr<config::settings> settings_;
    config::sorting sorting_;

    std::shared_ptr<vfs::dir> dir_;

    Pango::AttrList attrs_;

    Glib::RefPtr<Gio::ListStore<ModelColumns>> dir_model_;
    Glib::RefPtr<Gtk::MultiSelection> selection_model_;
    Glib::RefPtr<Gtk::SignalListItemFactory> factory_;

    Glib::RefPtr<Gtk::DragSource> drag_source_;
    Glib::RefPtr<Gtk::DropTarget> drop_target_;

    std::string pattern_;

    vfs::file::thumbnail_size thumbnail_size_{vfs::file::thumbnail_size::big};
    bool enable_thumbnail_{true};

    void on_setup_listitem(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_listitem(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_unbind_listitem(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;

    void on_background_click(std::int32_t n_press, double x, double y) noexcept;

    Glib::RefPtr<Gdk::ContentProvider> on_drag_prepare(double x, double y) const noexcept;
    bool on_drag_data_received(const Glib::ValueBase& value, double x, double y) noexcept;
    Gdk::DragAction on_drag_motion(double, double) noexcept;

    std::int32_t model_sort(const Glib::RefPtr<const ModelColumns>& a,
                            const Glib::RefPtr<const ModelColumns>& b) const noexcept;

    void sort() noexcept;

    [[nodiscard]] bool is_pattern_match(const std::filesystem::path& filename) const noexcept;

    std::pair<bool, std::uint32_t> find_file(const std::shared_ptr<vfs::file>& file) noexcept;

    void on_files_created(const std::span<const std::shared_ptr<vfs::file>> files) noexcept;
    void on_files_deleted(const std::span<const std::shared_ptr<vfs::file>> files) noexcept;
    void on_files_changed(const std::span<const std::shared_ptr<vfs::file>> files) noexcept;
    void on_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept;

  public:
    [[nodiscard]] auto
    signal_selection_changed() noexcept
    {
        return selection_model_->signal_selection_changed();
    }

  private:
    // Signals we connect to
    sigc::connection signal_files_created;
    sigc::connection signal_files_deleted;
    sigc::connection signal_files_changed;
    sigc::connection signal_thumbnail_loaded;
};
} // namespace gui
