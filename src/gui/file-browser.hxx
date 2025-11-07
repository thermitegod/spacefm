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
#include <vector>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <magic_enum/magic_enum.hpp>

#include "settings/settings.hxx"

#include "xset/xset.hxx"

#include "gui/utils/history.hxx"

#include "vfs/dir.hxx"

#include "logger.hxx"
#include "types.hxx"

#define PTK_FILE_BROWSER_REINTERPRET(obj) (reinterpret_cast<gui::browser*>(obj))

// forward declare
struct MainWindow;

namespace gui
{
struct browser
{
    enum class view_mode : std::uint8_t
    {
        icon_view,
        list_view,
        compact_view,
    };

    enum class sort_order : std::uint8_t
    {
        name,
        size,
        bytes,
        type,
        mime,
        perm,
        owner,
        group,
        atime,
        btime,
        ctime,
        mtime,
    };

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

    /* parent class */
    GtkBox parent;

    /* <private> */
    std::shared_ptr<vfs::dir> dir_{nullptr};
    GtkTreeModel* file_list_{nullptr};
    u32 max_thumbnail_{0};
    u64 n_selected_files_{0};
    u64 sel_size_{0};
    u64 sel_disk_size_{0};
    u32 sel_change_idle_{0};

    std::shared_ptr<config::settings> settings_;

    // path bar auto seek
    bool inhibit_focus_{false};
    std::optional<std::filesystem::path> seek_name_{std::nullopt};

    // sorting
    GtkSortType sort_type_;
    gui::browser::sort_order sort_order_{gui::browser::sort_order::perm};
    gui::browser::view_mode view_mode_{gui::browser::view_mode::compact_view};

    bool show_hidden_files_{true};
    bool large_icons_{true};
    bool pending_drag_status_{true};
    dev_t drag_source_dev_{0};
    ino_t drag_source_inode_{0};
    i32 drag_x_{0};
    i32 drag_y_{0};
    bool pending_drag_status_tree_{true};
    dev_t drag_source_dev_tree_{0};
    bool is_drag_{true};
    bool skip_release_{true};
    bool menu_shown_{true};

    /* directory view */
    GtkWidget* folder_view_{nullptr};
    GtkScrolledWindow* folder_view_scroll_{nullptr};
    GtkCellRenderer* icon_render_{nullptr};

    panel_t panel_{0};

    MainWindow* main_window_{nullptr};
    GtkNotebook* notebook_{nullptr};
    GtkWidget* task_view_{nullptr};
    GtkPaned* hpane{nullptr};
    GtkBox* side_vbox{nullptr};
    GtkBox* side_toolbox{nullptr};
    GtkPaned* side_vpane_top{nullptr};
    GtkPaned* side_vpane_bottom{nullptr};
    GtkScrolledWindow* side_dir_scroll{nullptr};
    GtkScrolledWindow* side_dev_scroll{nullptr};
    GtkWidget* side_dir{nullptr};
    GtkWidget* side_dev{nullptr};
    GtkStatusbar* statusbar{nullptr};
    GtkLabel* statusbar_label{nullptr};

    GtkBox* toolbar{nullptr};
    GtkButton* toolbar_back{nullptr};
    GtkButton* toolbar_forward{nullptr};
    GtkButton* toolbar_up{nullptr};
    GtkButton* toolbar_home{nullptr};
    GtkButton* toolbar_refresh{nullptr};
    GtkEntry* path_bar_{nullptr};
    GtkEntry* search_bar_{nullptr};

    // private:
    std::unique_ptr<gui::utils::history> history_;

  public:
    bool chdir(const std::filesystem::path& new_path,
               const gui::utils::history::mode mode = gui::utils::history::mode::normal) noexcept;

    const std::filesystem::path& cwd() const noexcept;
    void canon(const std::filesystem::path& path) noexcept;

    std::optional<std::filesystem::path> tab_cwd(const tab_t tab_num) const noexcept;
    std::optional<std::filesystem::path> panel_cwd(const panel_t panel_num) const noexcept;

    void open_in_panel(const panel_t panel_num, const std::filesystem::path& file_path) noexcept;
    bool is_panel_visible(const panel_t panel) const noexcept;

    struct browser_count_data
    {
        panel_t panel_count;
        tab_t tab_count;
        tab_t tab_num;
    };
    browser_count_data get_tab_panel_counts() const noexcept;

    void go_home() noexcept;
    void go_tab(tab_t tab) noexcept;
    void go_back() noexcept;
    void go_forward() noexcept;
    void go_up() noexcept;
    void refresh(const bool update_selected_files = true) noexcept;

    void show_hidden_files(bool show) noexcept;

    void new_tab() noexcept;
    void new_tab_here() noexcept;
    void close_tab() noexcept;
    void restore_tab() noexcept;
    void open_in_tab(const std::filesystem::path& file_path, const tab_t tab) const noexcept;

    std::vector<std::shared_ptr<vfs::file>> selected_files() const noexcept;
    std::vector<GtkTreePath*> selected_items(GtkTreeModel** model) const noexcept;

    void open_selected_files() noexcept;
    void open_selected_files_with_app(const std::string_view app_desktop = "") noexcept;

    void rename_selected_files(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                               const std::filesystem::path& cwd) noexcept;
    void
    batch_rename_selected_files(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                                const std::filesystem::path& cwd) noexcept;

    void hide_selected(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                       const std::filesystem::path& cwd) noexcept;

    void copycmd(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                 const std::filesystem::path& cwd, xset::name setname) noexcept;

    void set_sort_order(gui::browser::sort_order order) noexcept;
    void set_sort_type(GtkSortType order) noexcept;
    void set_sort_extra(xset::name setname) const noexcept;

    void paste_link() const noexcept;
    void paste_target() const noexcept;

    void update_selection_history() const noexcept;

    void select_all() const noexcept;
    void unselect_all() const noexcept;
    void select_last() const noexcept;
    void select_file(const std::filesystem::path& filename,
                     const bool unselect_others = true) const noexcept;
    void select_files(const std::span<const std::filesystem::path> select_filenames) const noexcept;
    void unselect_file(const std::filesystem::path& filename,
                       const bool unselect_others = true) const noexcept;
    void select_pattern(const std::string_view search_key = "") noexcept;
    void invert_selection() noexcept;

    void view_as_icons() noexcept;
    void view_as_compact_list() noexcept;
    void view_as_list() noexcept;

    void
    show_thumbnails(const u32 max_file_size) noexcept; // sets icons size using this->large_icons_
    void show_thumbnails(const u32 max_file_size, const bool large_icons) noexcept;

    void update_views() noexcept;
    void focus(const gui::browser::focus_widget item) noexcept;
    void focus_me() noexcept;
    void save_column_widths() const noexcept;
    bool slider_release(GtkPaned* pane) const noexcept;

    void rebuild_toolbox() noexcept;
    void rebuild_toolbars() const noexcept;

    void seek_path(const std::filesystem::path& seek_dir,
                   const std::filesystem::path& seek_name) noexcept;

    void update_statusbar() const noexcept;

    void on_permission(GtkMenuItem* item,
                       const std::span<const std::shared_ptr<vfs::file>> selected_files,
                       const std::filesystem::path& cwd) noexcept;
    void on_action(const xset::name setname) noexcept;

    void focus_folder_view() noexcept;

    void update_tab_label() noexcept;

    ////////////////

    /**
     * @param[in] pattern Only show files matching the pattern, an empty pattern shows all files.
     */
    void update_model(const std::string_view pattern = "") noexcept;

    bool using_large_icons() const noexcept;

    bool pending_drag_status_tree() const noexcept;
    void pending_drag_status_tree(bool val) noexcept;

    // sorting
    bool is_sort_type(GtkSortType type) const noexcept;
    bool is_sort_order(const gui::browser::sort_order type) const noexcept;
    bool is_view_mode(const gui::browser::view_mode type) const noexcept;

    // directory view

    GtkWidget* folder_view() const noexcept;
    void folder_view(GtkWidget* new_folder_view) noexcept;

    GtkScrolledWindow* folder_view_scroll() const noexcept;
    GtkCellRenderer* icon_render() const noexcept;

    // other
    panel_t panel() const noexcept;
    GtkWidget* task_view() const noexcept;
    MainWindow* main_window() const noexcept;
    GtkEntry* path_bar() const noexcept;
    GtkEntry* search_bar() const noexcept;

  public:
    // signal
    void on_dir_file_listed() noexcept;

    // Signals

    [[nodiscard]] auto
    signal_chdir_before()
    {
        return this->signal_chdir_before_;
    }

    [[nodiscard]] auto
    signal_chdir_begin()
    {
        return this->signal_chdir_begin_;
    }

    [[nodiscard]] auto
    signal_chdir_after()
    {
        return this->signal_chdir_after_;
    }

    [[nodiscard]] auto
    signal_open_file()
    {
        return this->signal_open_file_;
    }

    [[nodiscard]] auto
    signal_change_content()
    {
        return this->signal_change_content_;
    }

    [[nodiscard]] auto
    signal_change_selection()
    {
        return this->signal_change_selection_;
    }

    [[nodiscard]] auto
    signal_change_pane()
    {
        return this->signal_change_pane_;
    }

  private:
    // Signals
    sigc::signal<void(gui::browser*)> signal_chdir_before_;
    sigc::signal<void(gui::browser*)> signal_chdir_begin_;
    sigc::signal<void(gui::browser*)> signal_chdir_after_;
    sigc::signal<void(gui::browser*, const std::filesystem::path&, gui::browser::open_action)>
        signal_open_file_;
    sigc::signal<void(gui::browser*)> signal_change_content_;
    sigc::signal<void(gui::browser*)> signal_change_selection_;
    sigc::signal<void(gui::browser*)> signal_change_pane_;

  public:
    // Signals we connect to
    sigc::connection signal_file_created_;
    sigc::connection signal_file_deleted_;
    sigc::connection signal_directory_deleted_;
    sigc::connection signal_file_changed_;
    sigc::connection signal_file_listed_;
};
} // namespace gui

GtkWidget* gui_browser_new(i32 curpanel, GtkNotebook* notebook, GtkWidget* task_view,
                           MainWindow* main_window,
                           const std::shared_ptr<config::settings>& settings) noexcept;

bool gui_browser_delay_focus(gui::browser* browser) noexcept;

// xset callback wrapper functions
namespace gui::wrapper::browser
{
void go_home(GtkWidget* item, gui::browser* browser) noexcept;
void go_tab(GtkMenuItem* item, gui::browser* browser) noexcept;
void go_back(GtkWidget* item, gui::browser* browser) noexcept;
void go_forward(GtkWidget* item, gui::browser* browser) noexcept;
void go_up(GtkWidget* item, gui::browser* browser) noexcept;

void refresh(GtkWidget* item, gui::browser* browser) noexcept;

void new_tab(GtkMenuItem* item, gui::browser* browser) noexcept;
void new_tab_here(GtkMenuItem* item, gui::browser* browser) noexcept;
void close_tab(GtkMenuItem* item, gui::browser* browser) noexcept;
void restore_tab(GtkMenuItem* item, gui::browser* browser) noexcept;

void select_all(GtkWidget* item, gui::browser* browser) noexcept;
void unselect_all(GtkWidget* item, gui::browser* browser) noexcept;
void invert_selection(GtkWidget* item, gui::browser* browser) noexcept;

void focus(GtkMenuItem* item, gui::browser* browser) noexcept;
bool slider_release(GtkWidget* widget, GdkEvent* event, gui::browser* browser) noexcept;
} // namespace gui::wrapper::browser
