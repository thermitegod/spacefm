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
#include <optional>
#include <span>
#include <string_view>

#include <cstdint>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include "settings/settings.hxx"

#include "gui/lib/history.hxx"
#include "gui/tab/files/grid.hxx"
#include "gui/tab/files/list.hxx"
#include "gui/tab/statusbar.hxx"
#include "gui/tab/toolbar.hxx"

#include "gui/dialog/create.hxx"

#include "vfs/dir.hxx"

namespace gui
{
class tab final : public Gtk::Box
{
  public:
    enum class open_action : std::uint8_t
    {
        dir,
        new_tab,
        new_window,
        terminal,
        file,
    };

    enum class focus_widget : std::uint8_t
    {
        invalid,
        path_bar,
        search_bar,
        filelist,
        dirtree,
        device,
    };

    tab(Gtk::ApplicationWindow& parent, const config::tab_state& state,
        const std::shared_ptr<config::settings>& settings) noexcept;
    ~tab();

    config::tab_state get_tab_state() const noexcept;

    bool chdir(const std::filesystem::path& path,
               const gui::lib::history::mode mode = gui::lib::history::mode::normal) noexcept;
    void canon(const std::filesystem::path& path) noexcept;

    void show_hidden_files(bool show) noexcept;

    [[nodiscard]] std::filesystem::path cwd() const noexcept;

    void on_copy() const noexcept;
    void on_cut() const noexcept;
    void on_paste() const noexcept;
    void on_trash() const noexcept;
    void on_delete() const noexcept;

  private:
    void add_shortcuts() noexcept;
    void add_actions() noexcept;
    void add_context_menu() noexcept;
    void update_model(const std::string_view pattern = "") noexcept;

    Glib::RefPtr<Gio::Menu> create_context_menu_model() noexcept;
    void enable_all_actions() noexcept;

    void on_path_bar_activate(const std::string_view text) noexcept;
    void on_button_back();
    void on_button_forward();
    void on_button_up();
    void on_button_home();
    void on_button_refresh(const bool update_selected_files = true);

    void on_file_list_item_activated(std::uint32_t position) noexcept;
    void on_update_statusbar() noexcept;

    void on_dir_file_listed() noexcept;

    void on_copy_name() const noexcept;
    void on_copy_parent() const noexcept;
    void on_copy_path() const noexcept;
    void on_paste_link() const noexcept;
    void on_paste_target() const noexcept;
    void on_paste_as() const noexcept;

    void on_hide_files() const noexcept;

    void set_files_view(const config::view_mode view_mode) noexcept;
    void files_grab_focus() const noexcept;

    void set_sorting(const config::sorting& sorting, bool full_update = false) noexcept;
    void set_grid_state(const config::grid_state& state, const bool update_model = false) noexcept;
    void set_list_state(const config::list_state& state, const bool update_model = false) noexcept;

    [[nodiscard]] std::vector<std::shared_ptr<vfs::file>> selected_files() const noexcept;

    void select_all() const noexcept;
    void unselect_all() const noexcept;
    void select_last() const noexcept;
    void select_file(const std::filesystem::path& filename,
                     const bool unselect_others = true) const noexcept;
    void select_files(const std::span<const std::filesystem::path> select_filenames) const noexcept;
    void unselect_file(const std::filesystem::path& filename) const noexcept;
    void select_pattern(const std::string_view search_key = "") noexcept;
    void invert_selection() noexcept;

    void open_selected_files() noexcept;
    void open_selected_files_with_app(const std::string_view app_desktop = "") noexcept;
    void open_selected_files_execute(const bool in_terminal) noexcept;

    void update_selection_history() noexcept;

    void on_open_in_tab(std::int32_t tab, const std::filesystem::path& path) noexcept;
    void on_copy_to_tab(std::int32_t tab) noexcept;
    void on_move_to_tab(std::int32_t tab) noexcept;

    void on_copy_to_select_path() noexcept;
    void on_move_to_select_path() noexcept;

    void on_copy_to_last_path() noexcept;
    void on_move_to_last_path() noexcept;

    void archive_create() noexcept;
    void archive_extract() noexcept;
    void archive_extract_to() noexcept;
    void archive_open() noexcept;

    void show_rename_dialog() noexcept;
    void show_rename_batch_dialog() noexcept;
    void show_pattern_dialog() noexcept;
    void show_properites_dialog(std::int32_t page) noexcept;
    void show_create_dialog(dialog::create_mode mode) noexcept;
    void show_app_chooser_dialog() noexcept;

    Gtk::ApplicationWindow& parent_;
    std::shared_ptr<config::settings> settings_;
    config::view_mode view_mode_;
    config::sorting sorting_;
    config::grid_state grid_state_;
    config::list_state list_state_;
    gui::lib::history history_;

    std::shared_ptr<vfs::dir> dir_;

    Gtk::PopoverMenu popover_;
    Glib::RefPtr<Gio::SimpleActionGroup> action_group_;

    Gtk::Paned pane_;
    // Can be set to either device, tree view, or default sidebar.
    Gtk::ScrolledWindow side_view_;
    Gtk::ScrolledWindow file_view_;

    gui::toolbar toolbar_ = gui::toolbar(settings_);
    gui::statusbar statusbar_ = gui::statusbar(settings_);

    // gui::grid file_list_ = gui::grid(settings_);
    // gui::list file_list_ = gui::list(settings_);
    // Gtk::Widget* file_list_;

    gui::grid* view_grid_;
    gui::list* view_list_;

    struct
    {
        // Open
        Glib::RefPtr<Gio::SimpleAction> execute;
        Glib::RefPtr<Gio::SimpleAction> execute_in_terminal;
        Glib::RefPtr<Gio::SimpleAction> open_with;
        Glib::RefPtr<Gio::SimpleAction> open_in_tab;
        Glib::RefPtr<Gio::SimpleAction> open_in_panel;
        Glib::RefPtr<Gio::SimpleAction> archive_extract;
        Glib::RefPtr<Gio::SimpleAction> archive_extract_to;
        Glib::RefPtr<Gio::SimpleAction> archive_open;
        Glib::RefPtr<Gio::SimpleAction> open_choose;
        Glib::RefPtr<Gio::SimpleAction> open_default;
        // Go
        Glib::RefPtr<Gio::SimpleAction> back;
        Glib::RefPtr<Gio::SimpleAction> forward;
        Glib::RefPtr<Gio::SimpleAction> up;
        Glib::RefPtr<Gio::SimpleAction> home;
        // New
        Glib::RefPtr<Gio::SimpleAction> new_file;
        Glib::RefPtr<Gio::SimpleAction> new_directory;
        Glib::RefPtr<Gio::SimpleAction> new_symlink;
        Glib::RefPtr<Gio::SimpleAction> new_hardlink;
        Glib::RefPtr<Gio::SimpleAction> new_archive;
        // Actions
        Glib::RefPtr<Gio::SimpleAction> copy_name;
        Glib::RefPtr<Gio::SimpleAction> copy_parent;
        Glib::RefPtr<Gio::SimpleAction> copy_path;
        Glib::RefPtr<Gio::SimpleAction> paste_link;
        Glib::RefPtr<Gio::SimpleAction> paste_target;
        Glib::RefPtr<Gio::SimpleAction> paste_as;
        Glib::RefPtr<Gio::SimpleAction> hide;
        Glib::RefPtr<Gio::SimpleAction> select_all;
        Glib::RefPtr<Gio::SimpleAction> select_pattern;
        Glib::RefPtr<Gio::SimpleAction> invert_select;
        Glib::RefPtr<Gio::SimpleAction> unselect_all;
        // Actions > Copy To
        Glib::RefPtr<Gio::SimpleAction> copy_to;
        Glib::RefPtr<Gio::SimpleAction> copy_to_last;
        Glib::RefPtr<Gio::SimpleAction> copy_tab;
        Glib::RefPtr<Gio::SimpleAction> copy_panel;
        // Actions > Move To
        Glib::RefPtr<Gio::SimpleAction> move_to;
        Glib::RefPtr<Gio::SimpleAction> move_to_last;
        Glib::RefPtr<Gio::SimpleAction> move_tab;
        Glib::RefPtr<Gio::SimpleAction> move_panel;
        // Other
        Glib::RefPtr<Gio::SimpleAction> cut;
        Glib::RefPtr<Gio::SimpleAction> copy;
        Glib::RefPtr<Gio::SimpleAction> paste;
        Glib::RefPtr<Gio::SimpleAction> rename;
        Glib::RefPtr<Gio::SimpleAction> batch;
        Glib::RefPtr<Gio::SimpleAction> trash;
        Glib::RefPtr<Gio::SimpleAction> remove;
        Glib::RefPtr<Gio::SimpleAction> info;
        Glib::RefPtr<Gio::SimpleAction> attributes;
        Glib::RefPtr<Gio::SimpleAction> permissions;
        // View
        Glib::RefPtr<Gio::SimpleAction> view_mode;
        Glib::RefPtr<Gio::SimpleAction> show_hidden;
        Glib::RefPtr<Gio::SimpleAction> list_compact;
        Glib::RefPtr<Gio::SimpleAction> icon_size;
        // View > Sort
        Glib::RefPtr<Gio::SimpleAction> sort_natural;
        Glib::RefPtr<Gio::SimpleAction> sort_case;
        Glib::RefPtr<Gio::SimpleAction> sort_by;
        Glib::RefPtr<Gio::SimpleAction> sort_type;
        Glib::RefPtr<Gio::SimpleAction> sort_dir;
        Glib::RefPtr<Gio::SimpleAction> sort_hidden;
        // View > Columns
        Glib::RefPtr<Gio::SimpleAction> column_name;
        Glib::RefPtr<Gio::SimpleAction> column_size;
        Glib::RefPtr<Gio::SimpleAction> column_bytes;
        Glib::RefPtr<Gio::SimpleAction> column_type;
        Glib::RefPtr<Gio::SimpleAction> column_mime;
        Glib::RefPtr<Gio::SimpleAction> column_perm;
        Glib::RefPtr<Gio::SimpleAction> column_owner;
        Glib::RefPtr<Gio::SimpleAction> column_group;
        Glib::RefPtr<Gio::SimpleAction> column_atime;
        Glib::RefPtr<Gio::SimpleAction> column_btime;
        Glib::RefPtr<Gio::SimpleAction> column_ctime;
        Glib::RefPtr<Gio::SimpleAction> column_mtime;
    } actions_;

    std::optional<std::filesystem::path> last_path_;

    bool show_hidden_files_{true};
    bool large_icons_{true};

  public:
    [[nodiscard]] auto
    signal_chdir_before() noexcept
    {
        return signal_chdir_before_;
    }

    [[nodiscard]] auto
    signal_chdir_begin() noexcept
    {
        return signal_chdir_begin_;
    }

    [[nodiscard]] auto
    signal_chdir_after() noexcept
    {
        return signal_chdir_after_;
    }

    [[nodiscard]] auto
    signal_open_file() noexcept
    {
        return signal_open_file_;
    }

    [[nodiscard]] auto
    signal_change_content() noexcept
    {
        return signal_change_content_;
    }

    [[nodiscard]] auto
    signal_change_selection() noexcept
    {
        return signal_change_selection_;
    }

    [[nodiscard]] auto
    signal_change_pane() noexcept
    {
        return signal_change_pane_;
    }

    [[nodiscard]] auto
    signal_new_tab() noexcept
    {
        return signal_new_tab_;
    }

    [[nodiscard]] auto
    signal_close_tab() noexcept
    {
        return signal_close_tab_;
    }

    [[nodiscard]] auto
    signal_open_in_tab() noexcept
    {
        return signal_open_in_tab_;
    }

    [[nodiscard]] auto
    signal_switch_tab_with_paste() noexcept
    {
        return signal_switch_tab_with_paste_;
    }

    [[nodiscard]] auto
    signal_state_changed() noexcept
    {
        return signal_state_changed_;
    }

  private:
    sigc::signal<void()> signal_chdir_before_;
    sigc::signal<void()> signal_chdir_begin_;
    sigc::signal<void()> signal_chdir_after_;
    sigc::signal<void(const std::filesystem::path&, open_action)> signal_open_file_;
    sigc::signal<void()> signal_change_content_;
    sigc::signal<void()> signal_change_selection_;
    sigc::signal<void()> signal_change_pane_;

    sigc::signal<void()> signal_state_changed_;

    sigc::signal<void()> signal_close_tab_;
    sigc::signal<void(const std::filesystem::path&)> signal_new_tab_;

    sigc::signal<void(std::int32_t, const std::filesystem::path&)> signal_open_in_tab_;
    sigc::signal<void(std::int32_t)> signal_switch_tab_with_paste_;

    // Signals we connect to
    sigc::connection signal_file_created_;
    sigc::connection signal_file_deleted_;
    sigc::connection signal_file_changed_;
    sigc::connection signal_file_listed_;
};
} // namespace gui
