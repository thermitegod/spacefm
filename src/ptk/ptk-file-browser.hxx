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

#include <string>
#include <string_view>

#include <filesystem>

#include <span>

#include <sigc++/sigc++.h>

#include <gtk/gtk.h>

#include "vfs/vfs-dir.hxx"
#include "settings.hxx"

#include "types.hxx"

#define PTK_FILE_BROWSER(obj)             (static_cast<PtkFileBrowser*>(obj))
#define PTK_FILE_BROWSER_REINTERPRET(obj) (reinterpret_cast<PtkFileBrowser*>(obj))

#define PTK_TYPE_FILE_BROWSER    (ptk_file_browser_get_type())
#define PTK_IS_FILE_BROWSER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_FILE_BROWSER))

namespace ptk
{
    enum class open_action
    {
        dir,
        new_tab,
        new_window,
        terminal,
        file,
    };
}

namespace ptk::file_browser
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
        mtime,
        ctime,
    };

    enum class chdir_mode
    {
        normal,
        add_history,
        no_history,
        back,
        forward,
    };

} // namespace ptk::file_browser

// forward declare
struct PtkFileBrowser;
struct MainWindow;

struct PtkFileBrowser
{
    /* parent class */
    GtkBox parent;

    /* <private> */
    GList* history{nullptr};
    GList* curHistory{nullptr};
    GList* histsel{nullptr};
    GList* curhistsel{nullptr};

    vfs::dir dir{nullptr};
    GtkTreeModel* file_list{nullptr};
    i32 max_thumbnail{0};
    i32 n_sel_files{0};
    off_t sel_size{0};
    off_t sel_disk_size{0};
    u32 sel_change_idle{0};

    // path bar auto seek
    bool inhibit_focus{false};
    char* seek_name{nullptr};

    /* side pane */
    GtkWidget* side_pane_buttons{nullptr};
    GtkToggleToolButton* location_btn{nullptr};
    GtkToggleToolButton* dir_tree_btn{nullptr};

    GtkSortType sort_type;
    ptk::file_browser::sort_order sort_order{ptk::file_browser::sort_order::perm};
    ptk::file_browser::view_mode view_mode{ptk::file_browser::view_mode::compact_view};

    bool single_click{true};
    bool show_hidden_files{true};
    bool large_icons{true};
    bool busy{true};
    bool pending_drag_status{true};
    dev_t drag_source_dev{0};
    ino_t drag_source_inode{0};
    i32 drag_x{0};
    i32 drag_y{0};
    bool pending_drag_status_tree{true};
    dev_t drag_source_dev_tree{0};
    bool is_drag{true};
    bool skip_release{true};
    bool menu_shown{true};
    char* book_set_name{nullptr};

    /* directory view */
    GtkWidget* folder_view{nullptr};
    GtkWidget* folder_view_scroll{nullptr};
    GtkCellRenderer* icon_render{nullptr};
    u32 single_click_timeout{0};

    // MOD
    panel_t mypanel{0};
    GtkWidget* mynotebook{nullptr};
    GtkWidget* task_view{nullptr};
    void* main_window{nullptr};
    GtkWidget* toolbox{nullptr};
    GtkWidget* path_bar{nullptr};
    GtkWidget* hpane{nullptr};
    GtkWidget* side_vbox{nullptr};
    GtkWidget* side_toolbox{nullptr};
    GtkWidget* side_vpane_top{nullptr};
    GtkWidget* side_vpane_bottom{nullptr};
    GtkWidget* side_dir_scroll{nullptr};
    GtkWidget* side_dev_scroll{nullptr};
    GtkWidget* side_dir{nullptr};
    GtkWidget* side_dev{nullptr};
    GtkWidget* status_bar{nullptr};
    GtkFrame* status_frame{nullptr};
    GtkLabel* status_label{nullptr};
    GtkWidget* status_image{nullptr};
    GtkWidget* toolbar{nullptr};
    GtkWidget* side_toolbar{nullptr};
    GSList* toolbar_widgets[10];

    GtkTreeIter book_iter_inserted;
    char* select_path{nullptr};
    char* status_bar_custom{nullptr};

    // Signals
  public:
    // Signals function types
    using evt_chdir_before_t = void(PtkFileBrowser*, MainWindow*);
    using evt_chdir_begin_t = void(PtkFileBrowser*, MainWindow*);
    using evt_chdir_after_t = void(PtkFileBrowser*, MainWindow*);
    using evt_open_file_t = void(PtkFileBrowser*, const std::filesystem::path&, ptk::open_action,
                                 MainWindow*);
    using evt_change_content_t = void(PtkFileBrowser*, MainWindow*);
    using evt_change_sel_t = void(PtkFileBrowser*, MainWindow*);
    using evt_change_pane_mode_t = void(PtkFileBrowser*, MainWindow*);

    // Signals Add Event
    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::chdir_before, sigc::connection>::type
    add_event(evt_chdir_before_t fun, MainWindow* window)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::chdir_before");
        this->evt_data_window = window;
        return this->evt_chdir_before.connect(sigc::ptr_fun(fun));
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::chdir_begin, sigc::connection>::type
    add_event(evt_chdir_begin_t fun, MainWindow* window)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::chdir_begin");
        this->evt_data_window = window;
        return this->evt_chdir_begin.connect(sigc::ptr_fun(fun));
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::chdir_after, sigc::connection>::type
    add_event(evt_chdir_after_t fun, MainWindow* window)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::chdir_after");
        this->evt_data_window = window;
        return this->evt_chdir_after.connect(sigc::ptr_fun(fun));
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::open_item, sigc::connection>::type
    add_event(evt_open_file_t fun, MainWindow* window)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::open_item");
        this->evt_data_window = window;
        return this->evt_open_file.connect(sigc::ptr_fun(fun));
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::change_content, sigc::connection>::type
    add_event(evt_change_content_t fun, MainWindow* window)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::change_content");
        this->evt_data_window = window;
        return this->evt_change_content.connect(sigc::ptr_fun(fun));
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::change_sel, sigc::connection>::type
    add_event(evt_change_sel_t fun, MainWindow* window)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::change_sel");
        this->evt_data_window = window;
        return this->evt_change_sel.connect(sigc::ptr_fun(fun));
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::change_pane, sigc::connection>::type
    add_event(evt_change_pane_mode_t fun, MainWindow* window)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::change_pane");
        this->evt_data_window = window;
        return this->evt_change_pane_mode.connect(sigc::ptr_fun(fun));
    }

    // Signals Run Event
    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::chdir_before, void>::type
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::chdir_before");
        this->evt_chdir_before.emit(this, this->evt_data_window);
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::chdir_begin, void>::type
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::chdir_begin");
        this->evt_chdir_begin.emit(this, this->evt_data_window);
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::chdir_after, void>::type
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::chdir_after");
        this->evt_chdir_after.emit(this, this->evt_data_window);
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::open_item, void>::type
    run_event(const std::filesystem::path& path, ptk::open_action action)
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::open_item");
        this->evt_open_file.emit(this, path, action, this->evt_data_window);
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::change_content, void>::type
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::change_content");
        this->evt_change_content.emit(this, this->evt_data_window);
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::change_sel, void>::type
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::change_sel");
        this->evt_change_sel.emit(this, this->evt_data_window);
    }

    template<spacefm::signal evt>
    typename std::enable_if<evt == spacefm::signal::change_pane, void>::type
    run_event()
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::change_pane");
        this->evt_change_pane_mode.emit(this, this->evt_data_window);
    }

    // Signals
  private:
    // Signal types
    sigc::signal<evt_chdir_before_t> evt_chdir_before;
    sigc::signal<evt_chdir_begin_t> evt_chdir_begin;
    sigc::signal<evt_chdir_after_t> evt_chdir_after;
    sigc::signal<evt_open_file_t> evt_open_file;
    sigc::signal<evt_change_content_t> evt_change_content;
    sigc::signal<evt_change_sel_t> evt_change_sel;
    sigc::signal<evt_change_pane_mode_t> evt_change_pane_mode;

  private:
    // Signal data
    // TODO/FIXME has to be a better way to do this
    MainWindow* evt_data_window{nullptr};

  public:
    // Signals we connect to
    sigc::connection signal_file_created;
    sigc::connection signal_file_deleted;
    sigc::connection signal_file_changed;
    sigc::connection signal_file_listed;
};

struct PtkFileBrowserClass
{
    GtkPanedClass parent;

    /* Default signal handlers */
    void (*before_chdir)(PtkFileBrowser* file_browser, const std::filesystem::path& path);
    void (*begin_chdir)(PtkFileBrowser* file_browser);
    void (*after_chdir)(PtkFileBrowser* file_browser);
    void (*open_item)(PtkFileBrowser* file_browser, const std::filesystem::path& path, i32 action);
    void (*content_change)(PtkFileBrowser* file_browser);
    void (*sel_change)(PtkFileBrowser* file_browser);
    void (*pane_mode_change)(PtkFileBrowser* file_browser);
};

GType ptk_file_browser_get_type();

GtkWidget* ptk_file_browser_new(i32 curpanel, GtkWidget* notebook, GtkWidget* task_view,
                                void* main_window);

/*
 * folder_path should be encodede in on-disk encoding
 */
bool ptk_file_browser_chdir(PtkFileBrowser* file_browser, const std::filesystem::path& folder_path,
                            ptk::file_browser::chdir_mode mode);

/*
 * returned path should be encodede in on-disk encoding
 * returned path should be encodede in UTF-8
 */
const std::filesystem::path ptk_file_browser_get_cwd(PtkFileBrowser* file_browser);

u32 ptk_file_browser_get_n_all_files(PtkFileBrowser* file_browser);
u32 ptk_file_browser_get_n_visible_files(PtkFileBrowser* file_browser);

u32 ptk_file_browser_get_n_sel(PtkFileBrowser* file_browser, u64* sel_size, u64* sel_disk_size);

void ptk_file_browser_go_back(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_go_forward(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_go_up(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_refresh(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_show_hidden_files(PtkFileBrowser* file_browser, bool show);

void ptk_file_browser_set_single_click(PtkFileBrowser* file_browser, bool single_click);
void ptk_file_browser_set_single_click_timeout(PtkFileBrowser* file_browser, u32 timeout);

/* Sorting files */
void ptk_file_browser_set_sort_order(PtkFileBrowser* file_browser,
                                     ptk::file_browser::sort_order order);

void ptk_file_browser_set_sort_type(PtkFileBrowser* file_browser, GtkSortType order);

void ptk_file_browser_set_sort_extra(PtkFileBrowser* file_browser, xset::name setname);
void ptk_file_browser_read_sort_extra(PtkFileBrowser* file_browser);

const std::vector<vfs::file_info> ptk_file_browser_get_selected_files(PtkFileBrowser* file_browser);

/* Return a list of selected filenames (full paths in on-disk encoding) */
void ptk_file_browser_open_selected_files(PtkFileBrowser* file_browser);

void ptk_file_browser_paste_link(PtkFileBrowser* file_browser);
void ptk_file_browser_paste_target(PtkFileBrowser* file_browser);

void ptk_file_browser_select_all(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_select_last(PtkFileBrowser* file_browser);
void ptk_file_browser_invert_selection(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_unselect_all(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_select_pattern(GtkWidget* item, PtkFileBrowser* file_browser,
                                     const char* search_key);
void ptk_file_browser_canon(PtkFileBrowser* file_browser, const std::filesystem::path& path);

void ptk_file_browser_rename_selected_files(PtkFileBrowser* file_browser,
                                            const std::span<const vfs::file_info> sel_files,
                                            const std::filesystem::path& cwd);

void ptk_file_browser_file_properties(PtkFileBrowser* file_browser, i32 page);

void ptk_file_browser_view_as_icons(PtkFileBrowser* file_browser);
void ptk_file_browser_view_as_compact_list(PtkFileBrowser* file_browser);
void ptk_file_browser_view_as_list(PtkFileBrowser* file_browser);

void ptk_file_browser_hide_selected(PtkFileBrowser* file_browser,
                                    const std::span<const vfs::file_info> sel_files,
                                    const std::filesystem::path& cwd);

void ptk_file_browser_show_thumbnails(PtkFileBrowser* file_browser, i32 max_file_size);

// MOD
bool ptk_file_browser_write_access(const std::filesystem::path& cwd);
bool ptk_file_browser_read_access(const std::filesystem::path& cwd);

void ptk_file_browser_update_views(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_go_home(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_go_default(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_new_tab(GtkMenuItem* item, PtkFileBrowser* file_browser);
void ptk_file_browser_new_tab_here(GtkMenuItem* item, PtkFileBrowser* file_browser);
void ptk_file_browser_set_default_folder(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_go_tab(GtkMenuItem* item, PtkFileBrowser* file_browser, tab_t t);
void ptk_file_browser_focus(GtkMenuItem* item, PtkFileBrowser* file_browser, i32 job2);
void ptk_file_browser_save_column_widths(GtkTreeView* view, PtkFileBrowser* file_browser);

bool ptk_file_browser_slider_release(GtkWidget* widget, GdkEventButton* event,
                                     PtkFileBrowser* file_browser);
void ptk_file_browser_rebuild_toolbars(PtkFileBrowser* file_browser);
void ptk_file_browser_focus_me(PtkFileBrowser* file_browser);
void ptk_file_browser_open_in_tab(PtkFileBrowser* file_browser, tab_t tab_num,
                                  const std::filesystem::path& file_path);
void ptk_file_browser_on_permission(GtkMenuItem* item, PtkFileBrowser* file_browser,
                                    const std::span<const vfs::file_info> sel_files,
                                    const std::filesystem::path& cwd);
void ptk_file_browser_copycmd(PtkFileBrowser* file_browser,
                              const std::span<const vfs::file_info> sel_files,
                              const std::filesystem::path& cwd, xset::name setname);
void ptk_file_browser_on_action(PtkFileBrowser* browser, xset::name setname);
GList* folder_view_get_selected_items(PtkFileBrowser* file_browser, GtkTreeModel** model);
void ptk_file_browser_select_file(PtkFileBrowser* file_browser, const std::filesystem::path& path);
void ptk_file_browser_select_file_list(PtkFileBrowser* file_browser, char** filename,
                                       bool do_select);
void ptk_file_browser_seek_path(PtkFileBrowser* file_browser, const std::filesystem::path& seek_dir,
                                const std::filesystem::path& seek_name);
void ptk_file_browser_add_toolbar_widget(xset_t set, GtkWidget* widget);
void ptk_file_browser_update_toolbar_widgets(PtkFileBrowser* file_browser, xset::tool tool_type);
void ptk_file_browser_update_toolbar_widgets(PtkFileBrowser* file_browser, xset_t set_ptr,
                                             xset::tool tool_type);
void ptk_file_browser_show_history_menu(PtkFileBrowser* file_browser, bool is_back_history,
                                        GdkEventButton* event);
