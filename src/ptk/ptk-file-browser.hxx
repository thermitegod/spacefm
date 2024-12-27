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

#if __has_include(<magic_enum/magic_enum.hpp>)
// >=magic_enum-0.9.7
#include <magic_enum/magic_enum.hpp>
#else
// <=magic_enum-0.9.6
#include <magic_enum.hpp>
#endif

#include "logger.hxx"

#include "xset/xset.hxx"

#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "types.hxx"

#define PTK_FILE_BROWSER_REINTERPRET(obj) (reinterpret_cast<ptk::browser*>(obj))

// forward declare
struct MainWindow;

namespace ptk
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

    enum class chdir_mode : std::uint8_t
    {
        normal,
        back,
        forward,
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
    struct navigation_history_data
    {
        void go_back() noexcept;
        [[nodiscard]] bool has_back() const noexcept;
        [[nodiscard]] std::span<const std::filesystem::path> get_back() const noexcept;

        void go_forward() noexcept;
        [[nodiscard]] bool has_forward() const noexcept;
        [[nodiscard]] std::span<const std::filesystem::path> get_forward() const noexcept;

        void new_forward(const std::filesystem::path& path) noexcept;

        void reset() noexcept;

        [[nodiscard]] const std::filesystem::path&
        path(const ptk::browser::chdir_mode mode = ptk::browser::chdir_mode::normal) const noexcept;

      private:
        std::vector<std::filesystem::path> forward_;
        std::vector<std::filesystem::path> back_;
        std::filesystem::path current_{vfs::user::home()};
    };
    std::unique_ptr<navigation_history_data> navigation_history;

    struct selection_history_data
    {
        std::unordered_map<std::filesystem::path, std::vector<std::filesystem::path>>
            selection_history;
    };
    std::unique_ptr<selection_history_data> selection_history;

  public:
    bool chdir(const std::filesystem::path& new_path,
               const ptk::browser::chdir_mode mode = ptk::browser::chdir_mode::normal) noexcept;

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
    void set_single_click(bool single_click) noexcept;

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
    void hide_selected(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                       const std::filesystem::path& cwd) noexcept;

    void copycmd(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                 const std::filesystem::path& cwd, xset::name setname) noexcept;

    void set_sort_order(ptk::browser::sort_order order) noexcept;
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
    void focus(const ptk::browser::focus_widget item) noexcept;
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
    GtkEntry* search_bar() const noexcept;

  private:
    u64 get_n_all_files() const noexcept;
    u64 get_n_visible_files() const noexcept;

  public:
    // signal
    void on_folder_content_changed(const std::shared_ptr<vfs::file>& file) noexcept;
    void on_dir_file_listed() noexcept;

    // Signals

    template<spacefm::signal evt, typename bind_fun>
    sigc::connection
    add_event(bind_fun fun) noexcept
    {
        logger::trace<logger::domain::signals>("Connect({}): {}",
                                               logger::utils::ptr(this),
                                               magic_enum::enum_name(evt));

        if constexpr (evt == spacefm::signal::chdir_before)
        {
            return this->evt_chdir_before.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::chdir_begin)
        {
            return this->evt_chdir_begin.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::chdir_after)
        {
            return this->evt_chdir_after.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::open_item)
        {
            return this->evt_open_file.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::change_content)
        {
            return this->evt_change_content.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::change_sel)
        {
            return this->evt_change_sel.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::change_pane)
        {
            return this->evt_change_pane_mode.connect(fun);
        }
        else
        {
            static_assert(always_false<bind_fun>::value, "Unsupported signal type");
        }
    }

    template<spacefm::signal evt, typename... Args>
    void
    run_event(Args&&... args) noexcept
    {
        logger::trace<logger::domain::signals>("Execute({}): {}",
                                               logger::utils::ptr(this),
                                               magic_enum::enum_name(evt));

        if constexpr (evt == spacefm::signal::chdir_before)
        {
            static_assert(sizeof...(args) == 0);
            this->evt_chdir_before.emit(this);
        }
        else if constexpr (evt == spacefm::signal::chdir_begin)
        {
            static_assert(sizeof...(args) == 0);
            this->evt_chdir_begin.emit(this);
        }
        else if constexpr (evt == spacefm::signal::chdir_after)
        {
            static_assert(sizeof...(args) == 0);
            this->evt_chdir_after.emit(this);
        }
        else if constexpr (evt == spacefm::signal::open_item)
        {
            static_assert(
                sizeof...(args) == 2 &&
                std::is_constructible_v<
                    std::filesystem::path,
                    std::decay_t<typename std::tuple_element<0, std::tuple<Args...>>::type>> &&
                std::is_same_v<
                    ptk::browser::open_action,
                    std::decay_t<typename std::tuple_element<1, std::tuple<Args...>>::type>>);
            this->evt_open_file.emit(this, std::forward<Args>(args)...);
        }
        else if constexpr (evt == spacefm::signal::change_content)
        {
            static_assert(sizeof...(args) == 0);
            this->evt_change_content.emit(this);
        }
        else if constexpr (evt == spacefm::signal::change_sel)
        {
            static_assert(sizeof...(args) == 0);
            this->evt_change_sel.emit(this);
        }
        else if constexpr (evt == spacefm::signal::change_pane)
        {
            static_assert(sizeof...(args) == 0);
            this->evt_change_pane_mode.emit(this);
        }
        else
        {
            static_assert(always_false<decltype(evt)>::value,
                          "Unsupported signal type or incorrect number of arguments.");
        }
    }

  private:
    // Signals
    template<typename T> struct always_false : std::false_type
    {
    };

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

GtkWidget* ptk_browser_new(i32 curpanel, GtkNotebook* notebook, GtkWidget* task_view,
                           MainWindow* main_window) noexcept;

bool ptk_browser_delay_focus(ptk::browser* browser) noexcept;

// xset callback wrapper functions
namespace ptk::wrapper::browser
{
void go_home(GtkWidget* item, ptk::browser* browser) noexcept;
void go_tab(GtkMenuItem* item, ptk::browser* browser) noexcept;
void go_back(GtkWidget* item, ptk::browser* browser) noexcept;
void go_forward(GtkWidget* item, ptk::browser* browser) noexcept;
void go_up(GtkWidget* item, ptk::browser* browser) noexcept;

void refresh(GtkWidget* item, ptk::browser* browser) noexcept;

void new_tab(GtkMenuItem* item, ptk::browser* browser) noexcept;
void new_tab_here(GtkMenuItem* item, ptk::browser* browser) noexcept;
void close_tab(GtkMenuItem* item, ptk::browser* browser) noexcept;
void restore_tab(GtkMenuItem* item, ptk::browser* browser) noexcept;

void select_all(GtkWidget* item, ptk::browser* browser) noexcept;
void unselect_all(GtkWidget* item, ptk::browser* browser) noexcept;
void invert_selection(GtkWidget* item, ptk::browser* browser) noexcept;

void focus(GtkMenuItem* item, ptk::browser* browser) noexcept;
bool slider_release(GtkWidget* widget, GdkEvent* event, ptk::browser* browser) noexcept;
} // namespace ptk::wrapper::browser
