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

#include <string_view>

#include <filesystem>

#include <span>
#include <unordered_map>
#include <vector>

#include <optional>

#include <memory>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <xset/xset.hxx>

#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "types.hxx"

#define PTK_FILE_BROWSER(obj)             (static_cast<ptk::browser*>(obj))
#define PTK_FILE_BROWSER_REINTERPRET(obj) (reinterpret_cast<ptk::browser*>(obj))

// forward declare
struct MainWindow;

namespace ptk
{
struct browser
{
    enum class view_mode
    {
        icon_view,
        list_view,
        compact_view,
    };

    enum class sort_order
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

    enum class chdir_mode
    {
        normal,
        back,
        forward,
    };

    enum class open_action
    {
        dir,
        new_tab,
        new_window,
        terminal,
        file,
    };

    /* parent class */
    GtkBox parent;

    /* <private> */
    std::shared_ptr<vfs::dir> dir_{nullptr};
    GtkTreeModel* file_list_{nullptr};
    u32 max_thumbnail_{0};
    u64 n_sel_files_{0};
    u64 sel_size_{0};
    u64 sel_disk_size_{0};
    u32 sel_change_idle_{0};

    // path bar auto seek
    bool inhibit_focus_{false};
    std::optional<std::filesystem::path> seek_name_{std::nullopt};

    // sorting
    GtkSortType sort_type_;
    ptk::browser::sort_order sort_order_{ptk::browser::sort_order::perm};
    ptk::browser::view_mode view_mode_{ptk::browser::view_mode::compact_view};

    bool single_click_{true};
    bool show_hidden_files_{true};
    bool large_icons_{true};
    bool busy_{false};
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
    GtkBox* toolbox_{nullptr};
    GtkEntry* path_bar_{nullptr};
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
    GtkToolbar* toolbar{nullptr};
    GSList* toolbar_widgets[10];

    // private:
    struct navigation_history_data
    {
        void go_back() noexcept;
        [[nodiscard]] bool has_back() const noexcept;
        [[nodiscard]] const std::span<const std::filesystem::path> get_back() const noexcept;

        void go_forward() noexcept;
        [[nodiscard]] bool has_forward() const noexcept;
        [[nodiscard]] const std::span<const std::filesystem::path> get_forward() const noexcept;

        void new_forward(const std::filesystem::path& path) noexcept;

        void reset() noexcept;

        [[nodiscard]] const std::filesystem::path&
        path(const ptk::browser::chdir_mode mode = ptk::browser::chdir_mode::normal) const noexcept;

      private:
        std::vector<std::filesystem::path> forward_{};
        std::vector<std::filesystem::path> back_{};
        std::filesystem::path current_{vfs::user::home()};
    };
    std::unique_ptr<navigation_history_data> navigation_history;

    struct selection_history_data
    {
        std::unordered_map<std::filesystem::path, std::vector<std::filesystem::path>>
            selection_history{};
    };
    std::unique_ptr<selection_history_data> selection_history;

  public:
    bool chdir(const std::filesystem::path& new_path,
               const ptk::browser::chdir_mode mode = ptk::browser::chdir_mode::normal) noexcept;

    const std::filesystem::path& cwd() const noexcept;
    void canon(const std::filesystem::path& path) noexcept;

    const std::optional<std::filesystem::path> tab_cwd(const tab_t tab_num) const noexcept;
    const std::optional<std::filesystem::path> panel_cwd(const panel_t panel_num) const noexcept;

    void open_in_panel(const panel_t panel_num, const std::filesystem::path& file_path) noexcept;
    bool is_panel_visible(const panel_t panel) const noexcept;

    u64 get_n_all_files() const noexcept;
    u64 get_n_visible_files() const noexcept;
    u64 get_n_sel(u64* sel_size, u64* sel_disk_size) const noexcept;

    struct browser_count_data
    {
        panel_t panel_count;
        tab_t tab_count;
        tab_t tab_num;
    };
    const browser_count_data get_tab_panel_counts() const noexcept;

    void go_home() noexcept;
    void go_default() noexcept;
    void go_tab(tab_t tab) noexcept;
    void go_back() noexcept;
    void go_forward() noexcept;
    void go_up() noexcept;
    void refresh(const bool update_selected_files = true) noexcept;

    void show_hidden_files(bool show) noexcept;
    void set_single_click(bool single_click) noexcept;

    void new_tab() noexcept;
    void new_tab_here() noexcept;
    void close_tab() noexcept;
    void restore_tab() noexcept;
    void open_in_tab(const std::filesystem::path& file_path, const tab_t tab) const noexcept;
    void set_default_folder() const noexcept;

    const std::vector<std::shared_ptr<vfs::file>> selected_files() const noexcept;
    const std::vector<GtkTreePath*> selected_items(GtkTreeModel** model) const noexcept;

    void open_selected_files() noexcept;
    void open_selected_files_with_app(const std::string_view app_desktop = "") noexcept;

    void rename_selected_files(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                               const std::filesystem::path& cwd) noexcept;
    void hide_selected(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                       const std::filesystem::path& cwd) noexcept;

    void copycmd(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                 const std::filesystem::path& cwd, xset::name setname) noexcept;

    void set_sort_order(ptk::browser::sort_order order) noexcept;
    void set_sort_type(GtkSortType order) noexcept;
    void set_sort_extra(xset::name setname) const noexcept;
    void read_sort_extra() const noexcept;

    void paste_link() const noexcept;
    void paste_target() const noexcept;

    void update_selection_history() const noexcept;

    void select_all() const noexcept;
    void unselect_all() const noexcept;
    void select_last() noexcept;
    void select_file(const std::filesystem::path& filename,
                     const bool unselect_others = true) const noexcept;
    void select_files(const std::span<std::filesystem::path> select_filenames) const noexcept;
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
    void focus(i32 job) noexcept;
    void focus_me() noexcept;
    void save_column_widths() const noexcept;
    bool slider_release(GtkPaned* pane) const noexcept;

    void rebuild_toolbars() noexcept;

    void seek_path(const std::filesystem::path& seek_dir,
                   const std::filesystem::path& seek_name) noexcept;
    void update_toolbar_widgets(xset::tool tool_type) noexcept;
    void update_toolbar_widgets(const xset_t& set_ptr, xset::tool tool_type) noexcept;

    void show_history_menu(bool is_back_history, GdkEvent* event) noexcept;

    void update_statusbar() const noexcept;

    void on_permission(GtkMenuItem* item,
                       const std::span<const std::shared_ptr<vfs::file>> selected_files,
                       const std::filesystem::path& cwd) noexcept;
    void on_action(const xset::name setname) noexcept;

    void focus_folder_view() noexcept;
    void enable_toolbar() noexcept;

    void update_tab_label() noexcept;

    ////////////////

    void update_model() noexcept;

    bool using_large_icons() const noexcept;
    bool is_busy() const noexcept;

    bool pending_drag_status_tree() const noexcept;
    void pending_drag_status_tree(bool val) noexcept;

    // sorting
    bool is_sort_type(GtkSortType type) const noexcept;
    bool is_sort_order(const ptk::browser::sort_order type) const noexcept;
    bool is_view_mode(const ptk::browser::view_mode type) const noexcept;

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

  public:
    // signal
    void on_folder_content_changed(const std::shared_ptr<vfs::file>& file);
    void on_dir_file_listed(bool is_cancelled);

    // Signals
  public:
    // Signals Add Event
    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::chdir_before, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::chdir_before");
        return this->evt_chdir_before.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::chdir_begin, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::chdir_begin");
        return this->evt_chdir_begin.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::chdir_after, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::chdir_after");
        return this->evt_chdir_after.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::open_item, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::open_item");
        return this->evt_open_file.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::change_content, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::change_content");
        return this->evt_change_content.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::change_sel, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::change_sel");
        return this->evt_change_sel.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::change_pane, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::change_pane");
        return this->evt_change_pane_mode.connect(fun);
    }

    // Signals Run Event
    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::chdir_before, void>
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::chdir_before");
        this->evt_chdir_before.emit(this);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::chdir_begin, void>
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::chdir_begin");
        this->evt_chdir_begin.emit(this);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::chdir_after, void>
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::chdir_after");
        this->evt_chdir_after.emit(this);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::open_item, void>
    run_event(const std::filesystem::path& path, ptk::browser::open_action action)
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::open_item");
        this->evt_open_file.emit(this, path, action);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::change_content, void>
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::change_content");
        this->evt_change_content.emit(this);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::change_sel, void>
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::change_sel");
        this->evt_change_sel.emit(this);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::change_pane, void>
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::change_pane");
        this->evt_change_pane_mode.emit(this);
    }

    // Signals
  private:
    // Signal types
    sigc::signal<void(ptk::browser*)> evt_chdir_before;
    sigc::signal<void(ptk::browser*)> evt_chdir_begin;
    sigc::signal<void(ptk::browser*)> evt_chdir_after;
    sigc::signal<void(ptk::browser*, const std::filesystem::path&, ptk::browser::open_action)>
        evt_open_file;
    sigc::signal<void(ptk::browser*)> evt_change_content;
    sigc::signal<void(ptk::browser*)> evt_change_sel;
    sigc::signal<void(ptk::browser*)> evt_change_pane_mode;

  public:
    // Signals we connect to
    sigc::connection signal_file_created;
    sigc::connection signal_file_deleted;
    sigc::connection signal_file_changed;
    sigc::connection signal_file_listed;
};
} // namespace ptk

GtkWidget* ptk_file_browser_new(i32 curpanel, GtkNotebook* notebook, GtkWidget* task_view,
                                MainWindow* main_window);

void ptk_file_browser_add_toolbar_widget(const xset_t& set, GtkWidget* widget);

bool ptk_file_browser_delay_focus(ptk::browser* file_browser);

// xset callback wrapper functions
void ptk_file_browser_go_home(GtkWidget* item, ptk::browser* file_browser);
void ptk_file_browser_go_default(GtkWidget* item, ptk::browser* file_browser);
void ptk_file_browser_go_tab(GtkMenuItem* item, ptk::browser* file_browser);
void ptk_file_browser_go_back(GtkWidget* item, ptk::browser* file_browser);
void ptk_file_browser_go_forward(GtkWidget* item, ptk::browser* file_browser);
void ptk_file_browser_go_up(GtkWidget* item, ptk::browser* file_browser);

void ptk_file_browser_refresh(GtkWidget* item, ptk::browser* file_browser);

void ptk_file_browser_new_tab(GtkMenuItem* item, ptk::browser* file_browser);
void ptk_file_browser_new_tab_here(GtkMenuItem* item, ptk::browser* file_browser);
void ptk_file_browser_close_tab(GtkMenuItem* item, ptk::browser* file_browser);
void ptk_file_browser_restore_tab(GtkMenuItem* item, ptk::browser* file_browser);
void ptk_file_browser_set_default_folder(GtkWidget* item, ptk::browser* file_browser);

void ptk_file_browser_select_all(GtkWidget* item, ptk::browser* file_browser);
void ptk_file_browser_unselect_all(GtkWidget* item, ptk::browser* file_browser);
void ptk_file_browser_invert_selection(GtkWidget* item, ptk::browser* file_browser);

void ptk_file_browser_focus(GtkMenuItem* item, ptk::browser* file_browser);
bool ptk_file_browser_slider_release(GtkWidget* widget, GdkEvent* event,
                                     ptk::browser* file_browser);
