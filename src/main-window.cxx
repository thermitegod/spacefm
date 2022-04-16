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

#include <string>
#include <filesystem>

#include <vector>

#include <glibmm.h>

#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include <malloc.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-location-view.hxx"

#include "window-reference.hxx"
#include "main-window.hxx"

#include "ptk/ptk-utils.hxx"

#include "pref-dialog.hxx"
#include "ptk/ptk-file-menu.hxx"

#include "settings.hxx"
#include "item-prop.hxx"
#include "find-files.hxx"

#include "autosave.hxx"
#include "utils.hxx"

/* FIXME: statvfs support should be moved to src/vfs */
#include <sys/statvfs.h>

#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-file-task.hxx"

#include "ptk/ptk-clipboard.hxx"
#include "ptk/ptk-handler.hxx"

static void rebuild_menus(FMMainWindow* main_window);
static void fm_main_window_preference(FMMainWindow* main_window);

static void fm_main_window_class_init(FMMainWindowClass* klass);
static void fm_main_window_init(FMMainWindow* main_window);
static void fm_main_window_finalize(GObject* obj);
static void fm_main_window_get_property(GObject* obj, unsigned int prop_id, GValue* value,
                                        GParamSpec* pspec);
static void fm_main_window_set_property(GObject* obj, unsigned int prop_id, const GValue* value,
                                        GParamSpec* pspec);
static gboolean fm_main_window_delete_event(GtkWidget* widget, GdkEventAny* event);

static gboolean fm_main_window_window_state_event(GtkWidget* widget, GdkEventWindowState* event);

static void on_folder_notebook_switch_pape(GtkNotebook* notebook, GtkWidget* page,
                                           unsigned int page_num, void* user_data);
static void on_file_browser_begin_chdir(PtkFileBrowser* file_browser, FMMainWindow* main_window);
static void on_file_browser_open_item(PtkFileBrowser* file_browser, const char* path,
                                      PtkOpenAction action, FMMainWindow* main_window);
static void on_file_browser_after_chdir(PtkFileBrowser* file_browser, FMMainWindow* main_window);
static void on_file_browser_content_change(PtkFileBrowser* file_browser, FMMainWindow* main_window);
static void on_file_browser_sel_change(PtkFileBrowser* file_browser, FMMainWindow* main_window);
static void on_file_browser_panel_change(PtkFileBrowser* file_browser, FMMainWindow* main_window);
static bool on_tab_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                               unsigned int time, PtkFileBrowser* file_browser);
static bool on_main_window_focus(GtkWidget* main_window, GdkEventFocus* event, void* user_data);

static bool on_main_window_keypress(FMMainWindow* main_window, GdkEventKey* event, XSet* known_set);
static bool on_main_window_keypress_found_key(FMMainWindow* main_window, XSet* set);
static bool on_window_button_press_event(GtkWidget* widget, GdkEventButton* event,
                                         FMMainWindow* main_window); // sfm
static void on_new_window_activate(GtkMenuItem* menuitem, void* user_data);
static void fm_main_window_close(FMMainWindow* main_window);

static void show_panels(GtkMenuItem* item, FMMainWindow* main_window);

static GtkWidget* main_task_view_new(FMMainWindow* main_window);
static void on_task_popup_show(GtkMenuItem* item, FMMainWindow* main_window, char* name2);
static bool main_tasks_running(FMMainWindow* main_window);
static void on_task_stop(GtkMenuItem* item, GtkWidget* view, XSet* set2, PtkFileTask* task2);
static void on_preference_activate(GtkMenuItem* menuitem, void* user_data);
static void main_task_prepare_menu(FMMainWindow* main_window, GtkWidget* menu,
                                   GtkAccelGroup* accel_group);
static void on_task_columns_changed(GtkWidget* view, void* user_data);
static PtkFileTask* get_selected_task(GtkWidget* view);
static void fm_main_window_update_status_bar(FMMainWindow* main_window,
                                             PtkFileBrowser* file_browser);
static void set_window_title(FMMainWindow* main_window, PtkFileBrowser* file_browser);
static void on_task_column_selected(GtkMenuItem* item, GtkWidget* view);
static void on_task_popup_errset(GtkMenuItem* item, FMMainWindow* main_window, char* name2);
static void show_task_dialog(GtkWidget* widget, GtkWidget* view);
static void on_about_activate(GtkMenuItem* menuitem, void* user_data);
static void update_window_title(GtkMenuItem* item, FMMainWindow* main_window);
static void on_toggle_panelbar(GtkWidget* widget, FMMainWindow* main_window);
static void on_fullscreen_activate(GtkMenuItem* menuitem, FMMainWindow* main_window);
static bool delayed_focus(GtkWidget* widget);
static bool delayed_focus_file_browser(PtkFileBrowser* file_browser);
static bool idle_set_task_height(FMMainWindow* main_window);

static GtkWindowClass* parent_class = nullptr;

static int n_windows = 0;

static std::vector<FMMainWindow*> all_windows;

static std::vector<std::string> closed_tabs_restore;

static unsigned int theme_change_notify = 0;

//  Drag & Drop/Clipboard targets
static GtkTargetEntry drag_targets[] = {{ztd::strdup("text/uri-list"), 0, 0}};

GType
fm_main_window_get_type()
{
    static GType type = G_TYPE_INVALID;
    if (type == G_TYPE_INVALID)
    {
        static const GTypeInfo info = {
            sizeof(FMMainWindowClass),
            nullptr,
            nullptr,
            (GClassInitFunc)fm_main_window_class_init,
            nullptr,
            nullptr,
            sizeof(FMMainWindow),
            0,
            (GInstanceInitFunc)fm_main_window_init,
            nullptr,
        };
        type = g_type_register_static(GTK_TYPE_WINDOW, "FMMainWindow", &info, (GTypeFlags)0);
    }
    return type;
}

static void
fm_main_window_class_init(FMMainWindowClass* klass)
{
    GObjectClass* object_class;
    GtkWidgetClass* widget_class;

    object_class = (GObjectClass*)klass;
    parent_class = (GtkWindowClass*)g_type_class_peek_parent(klass);

    object_class->set_property = fm_main_window_set_property;
    object_class->get_property = fm_main_window_get_property;
    object_class->finalize = fm_main_window_finalize;

    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->delete_event = fm_main_window_delete_event;
    widget_class->window_state_event = fm_main_window_window_state_event;

    /*  this works but desktop_window doesn't
    g_signal_new ( "task-notify",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       0,
                       nullptr, nullptr,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );
    */
}

static bool
on_configure_evt_timer(FMMainWindow* main_window)
{
    if (all_windows.empty())
        return false;

    // verify main_window still valid
    for (FMMainWindow* window: all_windows)
    {
        if (window == main_window)
            break;
    }

    if (main_window->configure_evt_timer)
    {
        g_source_remove(main_window->configure_evt_timer);
        main_window->configure_evt_timer = 0;
    }
    main_window_event(main_window,
                      event_handler.win_move,
                      "evt_win_move",
                      0,
                      0,
                      nullptr,
                      0,
                      0,
                      0,
                      true);
    return false;
}

static bool
on_window_configure_event(GtkWindow* window, GdkEvent* event, FMMainWindow* main_window)
{
    (void)window;
    (void)event;
    // use timer to prevent rapid events during resize
    if ((event_handler.win_move->s || event_handler.win_move->ob2_data) &&
        !main_window->configure_evt_timer)
        main_window->configure_evt_timer =
            g_timeout_add(200, (GSourceFunc)on_configure_evt_timer, main_window);
    return false;
}

static void
on_plugin_install(GtkMenuItem* item, FMMainWindow* main_window, XSet* set2)
{
    XSet* set;
    char* path = nullptr;
    const char* deffolder;
    char* plug_dir = nullptr;
    std::string msg;
    int job = PLUGIN_JOB_INSTALL;

    if (!item)
        set = set2;
    else
        set = XSET(g_object_get_data(G_OBJECT(item), "set"));
    if (!set)
        return;

    if (g_str_has_suffix(set->name, "cfile") || g_str_has_suffix(set->name, "curl"))
        job = PLUGIN_JOB_COPY;

    if (g_str_has_suffix(set->name, "file"))
    {
        // get file path
        XSet* save = xset_get("plug_ifile");
        if (save->s) //&& std::filesystem::is_directory(save->s)
            deffolder = save->s;
        else
        {
            if (!(deffolder = xset_get_s("go_set_default")))
                deffolder = ztd::strdup("/");
        }
        path = xset_file_dialog(GTK_WIDGET(main_window),
                                GTK_FILE_CHOOSER_ACTION_OPEN,
                                "Choose Plugin File",
                                deffolder,
                                nullptr);
        if (!path)
            return;
        if (save->s)
            free(save->s);
        save->s = g_path_get_dirname(path);
    }

    switch (job)
    {
        case PLUGIN_JOB_INSTALL:
        {
            // install job
            char* filename = g_path_get_basename(path);
            char* ext = strstr(filename, ".spacefm-plugin");
            if (!ext)
                ext = strstr(filename, ".tar.gz");
            if (ext)
                ext[0] = '\0';
            char* plug_dir_name = filename;
            if (ext)
                ext[0] = '.';
            if (!plug_dir_name)
            {
                msg = "This plugin's filename is invalid.  Please rename it using "
                      "alpha-numeric ASCII characters and try again.";
                xset_msg_dialog(GTK_WIDGET(main_window),
                                GTK_MESSAGE_ERROR,
                                "Invalid Plugin Filename",
                                GTK_BUTTONS_OK,
                                msg);
                {
                    free(plug_dir_name);
                    free(path);
                    return;
                }
            }

            plug_dir = g_build_filename(DATADIR, "spacefm", "plugins", plug_dir_name, nullptr);

            if (std::filesystem::exists(plug_dir))
            {
                msg = fmt::format(
                    "There is already a plugin installed as '{}'.  Overwrite ?\n\nTip: You can "
                    "also rename this plugin file to install it under a different name.",
                    plug_dir_name);
                if (xset_msg_dialog(GTK_WIDGET(main_window),
                                    GTK_MESSAGE_WARNING,
                                    "Overwrite Plugin ?",
                                    GTK_BUTTONS_YES_NO,
                                    msg) != GTK_RESPONSE_YES)
                {
                    free(plug_dir_name);
                    free(plug_dir);
                    free(path);
                    return;
                }
            }
            free(plug_dir_name);
            break;
        }
        case PLUGIN_JOB_COPY:
        {
            // copy job
            const char* user_tmp = xset_get_user_tmp_dir();
            if (!user_tmp)
            {
                xset_msg_dialog(GTK_WIDGET(main_window),
                                GTK_MESSAGE_ERROR,
                                "Error Creating Temp Directory",
                                GTK_BUTTONS_OK,
                                "Unable to create temporary directory");
                free(path);
                return;
            }
            while (!plug_dir || std::filesystem::exists(plug_dir))
            {
                char* hex8 = randhex8();
                if (plug_dir)
                    free(plug_dir);
                plug_dir = g_build_filename(user_tmp, hex8, nullptr);
                free(hex8);
            }
            break;
        }
        default:
            break;
    }

    install_plugin_file(main_window, nullptr, path, plug_dir, job, nullptr);
    free(path);
    free(plug_dir);
}

static GtkWidget*
create_plugins_menu(FMMainWindow* main_window)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    GtkWidget* plug_menu = gtk_menu_new();
    if (!file_browser)
        return plug_menu;

    std::vector<XSet*> plugins;

    XSet* set;

    set = xset_set_cb("plug_ifile", (GFunc)on_plugin_install, main_window);
    xset_set_ob1(set, "set", set);
    set = xset_set_cb("plug_cfile", (GFunc)on_plugin_install, main_window);
    xset_set_ob1(set, "set", set);

    set = xset_get("plug_install");
    xset_add_menuitem(file_browser, plug_menu, accel_group, set);
    set = xset_get("plug_copy");
    xset_add_menuitem(file_browser, plug_menu, accel_group, set);

    GtkWidget* item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(plug_menu), item);

    plugins = xset_get_plugins();
    for (XSet* set2: plugins)
        xset_add_menuitem(file_browser, plug_menu, accel_group, set2);
    if (!plugins.empty())
        xset_clear_plugins(plugins);

    gtk_widget_show_all(plug_menu);
    return plug_menu;
}

static void
on_devices_show(GtkMenuItem* item, FMMainWindow* main_window)
{
    (void)item;
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (!file_browser)
        return;
    int mode = main_window->panel_context[file_browser->mypanel - 1];

    xset_set_b_panel_mode(file_browser->mypanel, "show_devmon", mode, !file_browser->side_dev);
    update_views_all_windows(nullptr, file_browser);
    if (file_browser->side_dev)
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->side_dev));
}

static GtkWidget*
create_devices_menu(FMMainWindow* main_window)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    GtkWidget* dev_menu = gtk_menu_new();
    if (!file_browser)
        return dev_menu;

    XSet* set;

    set = xset_set_cb("main_dev", (GFunc)on_devices_show, main_window);
    set->b = file_browser->side_dev ? XSET_B_TRUE : XSET_B_UNSET;
    xset_add_menuitem(file_browser, dev_menu, accel_group, set);

    set = xset_get("separator");
    xset_add_menuitem(file_browser, dev_menu, accel_group, set);

    ptk_location_view_dev_menu(GTK_WIDGET(file_browser), file_browser, dev_menu);

    set = xset_get("separator");
    xset_add_menuitem(file_browser, dev_menu, accel_group, set);

    set = xset_get("dev_menu_settings");
    xset_add_menuitem(file_browser, dev_menu, accel_group, set);

    // show all
    gtk_widget_show_all(dev_menu);

    return dev_menu;
}

static void
on_open_url(GtkWidget* widget, FMMainWindow* main_window)
{
    (void)widget;
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    char* url = xset_get_s("main_save_session");
    if (file_browser && url && url[0])
        ptk_location_view_mount_network(file_browser, url, true, true);
}

static void
on_find_file_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(user_data);
    const char* dirs[2];
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    const char* cwd = ptk_file_browser_get_cwd(file_browser);

    dirs[0] = cwd;
    dirs[1] = nullptr;
    fm_find_files(dirs);
}

static void
on_open_current_folder_as_root(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(user_data);
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (!file_browser)
        return;
    // root task
    PtkFileTask* task = ptk_file_exec_new("Open Root Window",
                                          ptk_file_browser_get_cwd(file_browser),
                                          GTK_WIDGET(file_browser),
                                          file_browser->task_view);
    const std::string exe = get_prog_executable();
    const std::string cwd = bash_quote(ptk_file_browser_get_cwd(file_browser));
    task->task->exec_command = fmt::format("HOME=/root {} {}", exe, cwd);
    task->task->exec_as_user = "root";
    task->task->exec_sync = false;
    task->task->exec_export = false;
    task->task->exec_browser = nullptr;
    ptk_file_task_run(task);
}

static void
main_window_open_terminal(FMMainWindow* main_window, bool as_root)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (!file_browser)
        return;
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
    char* main_term = xset_get_s("main_terminal");
    if (!main_term || main_term[0] == '\0')
    {
        ptk_show_error(GTK_WINDOW(parent),
                       "Terminal Not Available",
                       "Please set your terminal program in View|Preferences|Advanced");
        fm_edit_preference(GTK_WINDOW(parent), PREF_ADVANCED);
        main_term = xset_get_s("main_terminal");
        if (!main_term || main_term[0] == '\0')
            return;
    }

    // task
    PtkFileTask* task = ptk_file_exec_new("Open Terminal",
                                          ptk_file_browser_get_cwd(file_browser),
                                          GTK_WIDGET(file_browser),
                                          file_browser->task_view);

    task->task->exec_command = Glib::find_program_in_path(main_term);
    if (as_root)
        task->task->exec_as_user = "root";
    task->task->exec_sync = false;
    task->task->exec_export = true;
    task->task->exec_browser = file_browser;
    ptk_file_task_run(task);
}

static void
on_open_terminal_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(user_data);
    main_window_open_terminal(main_window, false);
}

static void
on_open_root_terminal_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(user_data);
    main_window_open_terminal(main_window, true);
}

static void
on_quit_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    fm_main_window_delete_event(GTK_WIDGET(user_data), nullptr);
    // fm_main_window_close( GTK_WIDGET( user_data ) );
}

void
main_window_rubberband_all()
{
    for (FMMainWindow* window: all_windows)
    {
        int p;
        for (p = 1; p < 5; p++)
        {
            GtkWidget* notebook = window->panel[p - 1];
            int num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            int i;
            for (i = 0; i < num_pages; i++)
            {
                PtkFileBrowser* a_browser =
                    PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i));
                if (a_browser->view_mode == PTK_FB_LIST_VIEW)
                {
                    gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(a_browser->folder_view),
                                                     xset_get_b("rubberband"));
                }
            }
        }
    }
}

void
main_window_refresh_all()
{
    for (FMMainWindow* window: all_windows)
    {
        for (int p = 1; p < 5; p++)
        {
            int64_t notebook = (int64_t)window->panel[p - 1];
            int num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            for (int i = 0; i < num_pages; i++)
            {
                PtkFileBrowser* a_browser =
                    PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i));
                ptk_file_browser_refresh(nullptr, a_browser);
            }
        }
    }
}

static void
update_window_icon(GtkWindow* window, GtkIconTheme* theme)
{
    const char* name;
    GError* error = nullptr;

    XSet* set = xset_get("main_icon");
    if (set->icon)
        name = set->icon;
    else if (geteuid() == 0)
        name = ztd::strdup("spacefm-root");
    else
        name = ztd::strdup("spacefm");

    GdkPixbuf* icon = gtk_icon_theme_load_icon(theme, name, 48, (GtkIconLookupFlags)0, &error);
    if (icon)
    {
        gtk_window_set_icon(window, icon);
        g_object_unref(icon);
    }
    else if (error != nullptr)
    {
        // An error occured on loading the icon
        LOG_ERROR("spacefm: Unable to load the window icon '{}' in - update_window_icon - {}",
                  name,
                  error->message);
        g_error_free(error);
    }
}

static void
update_window_icons(GtkIconTheme* theme, GtkWindow* window)
{
    (void)window;
    for (FMMainWindow* window2: all_windows)
    {
        update_window_icon(GTK_WINDOW(window2), theme);
    }
}

static void
on_main_icon()
{
    for (FMMainWindow* window: all_windows)
    {
        update_window_icon(GTK_WINDOW(window), gtk_icon_theme_get_default());
    }
}

static void
main_design_mode(GtkMenuItem* menuitem, FMMainWindow* main_window)
{
    (void)menuitem;
    xset_msg_dialog(
        GTK_WIDGET(main_window),
        GTK_MESSAGE_INFO,
        "Design Mode Help",
        GTK_BUTTONS_OK,
        "Design Mode allows you to change the name, shortcut key and icon of menu, toolbar and "
        "bookmark items, show help for an item, and add your own custom commands and "
        "applications.\n\nTo open the Design Menu, simply right-click on a menu item, bookmark, "
        "or toolbar item.  To open the Design Menu for a submenu, first close the submenu (by "
        "clicking on it).\n\nFor more information, click the Help button below.");
}

void
main_window_bookmark_changed(const char* changed_set_name)
{
    for (FMMainWindow* window: all_windows)
    {
        int p;
        for (p = 1; p < 5; p++)
        {
            GtkWidget* notebook = window->panel[p - 1];
            int num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            int i;
            for (i = 0; i < num_pages; i++)
            {
                PtkFileBrowser* a_browser =
                    PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i));
                if (a_browser->side_book)
                    ptk_bookmark_view_xset_changed(GTK_TREE_VIEW(a_browser->side_book),
                                                   a_browser,
                                                   changed_set_name);
            }
        }
    }
}

void
main_window_refresh_all_tabs_matching(const char* path)
{
    (void)path;
    // This function actually closes the tabs because refresh doesn't work.
    // dir objects have multiple refs and unreffing them all wouldn't finalize
    // the dir object for unknown reason.

    // This breaks auto open of tabs on automount
}

void
main_window_rebuild_all_toolbars(PtkFileBrowser* file_browser)
{
    // LOG_INFO("main_window_rebuild_all_toolbars");

    // do this browser first
    if (file_browser)
        ptk_file_browser_rebuild_toolbars(file_browser);

    // do all windows all panels all tabs
    for (FMMainWindow* window: all_windows)
    {
        int p;
        for (p = 1; p < 5; p++)
        {
            GtkWidget* notebook = window->panel[p - 1];
            int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            int cur_tabx;
            for (cur_tabx = 0; cur_tabx < pages; cur_tabx++)
            {
                PtkFileBrowser* a_browser =
                    PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), cur_tabx));
                if (a_browser != file_browser)
                    ptk_file_browser_rebuild_toolbars(a_browser);
            }
        }
    }
    autosave_request();
}

void
main_window_update_all_bookmark_views()
{
    // do all windows all panels all tabs
    for (FMMainWindow* window: all_windows)
    {
        int p;
        for (p = 1; p < 5; p++)
        {
            GtkWidget* notebook = window->panel[p - 1];
            int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            int cur_tabx;
            for (cur_tabx = 0; cur_tabx < pages; cur_tabx++)
            {
                PtkFileBrowser* a_browser =
                    PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), cur_tabx));
                if (a_browser->side_book)
                    ptk_bookmark_view_update_icons(nullptr, a_browser);
            }
        }
    }
    main_window_rebuild_all_toolbars(nullptr); // toolbar uses bookmark icon
}

void
update_views_all_windows(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    // LOG_INFO("update_views_all_windows");
    // do this browser first
    if (!file_browser)
        return;
    int p = file_browser->mypanel;

    ptk_file_browser_update_views(nullptr, file_browser);

    // do other windows
    for (FMMainWindow* window: all_windows)
    {
        if (gtk_widget_get_visible(window->panel[p - 1]))
        {
            GtkWidget* notebook = window->panel[p - 1];
            int cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
            if (cur_tabx != -1)
            {
                PtkFileBrowser* a_browser =
                    PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), cur_tabx));
                if (a_browser != file_browser)
                    ptk_file_browser_update_views(nullptr, a_browser);
            }
        }
    }
    autosave_request();
}

void
main_window_toggle_thumbnails_all_windows()
{
    // toggle
    app_settings.show_thumbnail = !app_settings.show_thumbnail;

    // update all windows/all panels/all browsers
    for (FMMainWindow* window: all_windows)
    {
        int p;
        for (p = 1; p < 5; p++)
        {
            GtkNotebook* notebook = GTK_NOTEBOOK(window->panel[p - 1]);
            int n = gtk_notebook_get_n_pages(notebook);
            int i;
            for (i = 0; i < n; ++i)
            {
                PtkFileBrowser* file_browser =
                    PTK_FILE_BROWSER(gtk_notebook_get_nth_page(notebook, i));
                ptk_file_browser_show_thumbnails(
                    file_browser,
                    app_settings.show_thumbnail ? app_settings.max_thumb_size : 0);
            }
        }
    }

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
}

void
focus_panel(GtkMenuItem* item, void* mw, int p)
{
    int panel;
    int hidepanel;
    int panel_num;

    FMMainWindow* main_window = static_cast<FMMainWindow*>(mw);

    if (item)
        panel_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "panel_num"));
    else
        panel_num = p;

    switch (panel_num)
    {
        case -1:
            // prev
            panel = main_window->curpanel - 1;
            do
            {
                if (panel < 1)
                    panel = 4;
                if (xset_get_b_panel(panel, "show"))
                    break;
                panel--;
            } while (panel != main_window->curpanel - 1);
            break;
        case -2:
            // next
            panel = main_window->curpanel + 1;
            do
            {
                if (panel > 4)
                    panel = 1;
                if (xset_get_b_panel(panel, "show"))
                    break;
                panel++;
            } while (panel != main_window->curpanel + 1);
            break;
        case -3:
            // hide
            hidepanel = main_window->curpanel;
            panel = main_window->curpanel + 1;
            do
            {
                if (panel > 4)
                    panel = 1;
                if (xset_get_b_panel(panel, "show"))
                    break;
                panel++;
            } while (panel != hidepanel);
            if (panel == hidepanel)
                panel = 0;
            break;
        default:
            panel = panel_num;
            break;
    }

    if (panel > 0 && panel < 5)
    {
        if (gtk_widget_get_visible(main_window->panel[panel - 1]))
        {
            gtk_widget_grab_focus(GTK_WIDGET(main_window->panel[panel - 1]));
            main_window->curpanel = panel;
            main_window->notebook = main_window->panel[panel - 1];
            PtkFileBrowser* file_browser =
                PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
            if (file_browser)
            {
                gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
                set_panel_focus(main_window, file_browser);
            }
        }
        else if (panel_num != -3)
        {
            xset_set_b_panel(panel, "show", true);
            show_panels_all_windows(nullptr, main_window);
            gtk_widget_grab_focus(GTK_WIDGET(main_window->panel[panel - 1]));
            main_window->curpanel = panel;
            main_window->notebook = main_window->panel[panel - 1];
            PtkFileBrowser* file_browser =
                PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
            if (file_browser)
            {
                gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
                set_panel_focus(main_window, file_browser);
            }
        }
        if (panel_num == -3)
        {
            xset_set_b_panel(hidepanel, "show", false);
            show_panels_all_windows(nullptr, main_window);
        }
    }
}

void
show_panels_all_windows(GtkMenuItem* item, FMMainWindow* main_window)
{
    (void)item;
    // do this window first
    main_window->panel_change = true;
    show_panels(nullptr, main_window);

    // do other windows
    main_window->panel_change = false; // don't save columns for other windows
    for (FMMainWindow* window: all_windows)
    {
        if (main_window != window)
            show_panels(nullptr, window);
    }

    autosave_request();
}

static void
show_panels(GtkMenuItem* item, FMMainWindow* main_window)
{
    (void)item;
    int p;
    int cur_tabx;
    const char* folder_path;
    XSet* set;
    char* tabs;
    char* end;
    char* tab_dir;
    char* tabs_add;
    bool show[5]; // array starts at 1 for clarity
    bool tab_added;
    bool horiz;
    bool vert;
    PtkFileBrowser* file_browser;

    // save column widths and side sliders of visible panels
    if (main_window->panel_change)
    {
        for (p = 1; p < 5; p++)
        {
            if (gtk_widget_get_visible(GTK_WIDGET(main_window->panel[p - 1])))
            {
                cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
                if (cur_tabx != -1)
                {
                    file_browser = PTK_FILE_BROWSER(
                        gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]),
                                                  cur_tabx));
                    if (file_browser)
                    {
                        if (file_browser->view_mode == PTK_FB_LIST_VIEW)
                            ptk_file_browser_save_column_widths(
                                GTK_TREE_VIEW(file_browser->folder_view),
                                file_browser);
                        // ptk_file_browser_slider_release( nullptr, nullptr, file_browser );
                    }
                }
            }
        }
    }

    // show panelbar
    if (!!gtk_widget_get_visible(main_window->panelbar) !=
        !!(!main_window->fullscreen && xset_get_b("main_pbar")))
    {
        if (xset_get_b("main_pbar"))
            gtk_widget_show(GTK_WIDGET(main_window->panelbar));
        else
            gtk_widget_hide(GTK_WIDGET(main_window->panelbar));
    }

    // all panels hidden?
    for (p = 1; p < 5; p++)
    {
        if (xset_get_b_panel(p, "show"))
            break;
    }
    if (p == 5)
        xset_set_b_panel(1, "show", true);

    for (p = 1; p < 5; p++)
        show[p] = xset_get_b_panel(p, "show");

    for (p = 1; p < 5; p++)
    {
        // panel context - how panels share horiz and vert space with other panels
        switch (p)
        {
            case 1:
                horiz = show[2];
                vert = show[3] || show[4];
                break;
            case 2:
                horiz = show[1];
                vert = show[3] || show[4];
                break;
            case 3:
                horiz = show[4];
                vert = show[1] || show[2];
                break;
            default:
                horiz = show[3];
                vert = show[1] || show[2];
                break;
        }

        if (horiz && vert)
            main_window->panel_context[p - 1] = PANEL_BOTH;
        else if (horiz)
            main_window->panel_context[p - 1] = PANEL_HORIZ;
        else if (vert)
            main_window->panel_context[p - 1] = PANEL_VERT;
        else
            main_window->panel_context[p - 1] = PANEL_NEITHER;

        if (show[p])
        {
            // shown
            // test if panel and mode exists
            char* str =
                g_strdup_printf("panel%d_slider_positions%d", p, main_window->panel_context[p - 1]);
            if (!(set = xset_is(str)))
            {
                // no config exists for this panel and mode - copy
                // printf ("no config for %d, %d\n", p, main_window->panel_context[p-1] );
                XSet* set_old;
                char mode = main_window->panel_context[p - 1];
                xset_set_b_panel_mode(p, "show_toolbox", mode, xset_get_b_panel(p, "show_toolbox"));
                xset_set_b_panel_mode(p, "show_devmon", mode, xset_get_b_panel(p, "show_devmon"));
                if (xset_is("show_dirtree"))
                    xset_set_b_panel_mode(p,
                                          "show_dirtree",
                                          mode,
                                          xset_get_b_panel(p, "show_dirtree"));
                xset_set_b_panel_mode(p, "show_book", mode, xset_get_b_panel(p, "show_book"));
                xset_set_b_panel_mode(p, "show_sidebar", mode, xset_get_b_panel(p, "show_sidebar"));
                xset_set_b_panel_mode(p, "detcol_name", mode, xset_get_b_panel(p, "detcol_name"));
                xset_set_b_panel_mode(p, "detcol_size", mode, xset_get_b_panel(p, "detcol_size"));
                xset_set_b_panel_mode(p, "detcol_type", mode, xset_get_b_panel(p, "detcol_type"));
                xset_set_b_panel_mode(p, "detcol_perm", mode, xset_get_b_panel(p, "detcol_perm"));
                xset_set_b_panel_mode(p, "detcol_owner", mode, xset_get_b_panel(p, "detcol_owner"));
                xset_set_b_panel_mode(p, "detcol_date", mode, xset_get_b_panel(p, "detcol_date"));
                set_old = xset_get_panel(p, "slider_positions");
                set = xset_get_panel_mode(p, "slider_positions", mode);
                set->x = ztd::strdup(set_old->x ? set_old->x : "0");
                set->y = ztd::strdup(set_old->y ? set_old->y : "0");
                set->s = ztd::strdup(set_old->s ? set_old->s : "0");
            }
            free(str);
            // load dynamic slider positions for this panel context
            main_window->panel_slide_x[p - 1] = set->x ? strtol(set->x, nullptr, 10) : 0;
            main_window->panel_slide_y[p - 1] = set->y ? strtol(set->y, nullptr, 10) : 0;
            main_window->panel_slide_s[p - 1] = set->s ? strtol(set->s, nullptr, 10) : 0;
            // LOG_INFO("loaded panel {}", p);
            if (!gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[p - 1])))
            {
                main_window->notebook = main_window->panel[p - 1];
                main_window->curpanel = p;
                // load saved tabs
                tab_added = false;
                set = xset_get_panel(p, "show");
                if ((set->s && app_settings.load_saved_tabs) || set->ob1)
                {
                    // set->ob1 is preload path
                    tabs_add = g_strdup_printf("%s%s%s",
                                               set->s && app_settings.load_saved_tabs ? set->s : "",
                                               set->ob1 ? "///" : "",
                                               set->ob1 ? set->ob1 : "");
                    tabs = tabs_add;
                    while (tabs && !strncmp(tabs, "///", 3))
                    {
                        tabs += 3;
                        if (!strncmp(tabs, "////", 4))
                        {
                            tab_dir = ztd::strdup("/");
                            tabs++;
                        }
                        else if ((end = strstr(tabs, "///")))
                        {
                            end[0] = '\0';
                            tab_dir = ztd::strdup(tabs);
                            end[0] = '/';
                            tabs = end;
                        }
                        else
                        {
                            tab_dir = ztd::strdup(tabs);
                            tabs = nullptr;
                        }
                        if (tab_dir[0] != '\0')
                        {
                            // open saved tab
                            if (Glib::str_has_prefix(tab_dir, "~/"))
                            {
                                // convert ~ to /home/user for hacked session files
                                str = g_strdup_printf("%s%s",
                                                      vfs_user_home_dir().c_str(),
                                                      tab_dir + 1);
                                free(tab_dir);
                                tab_dir = str;
                            }
                            if (std::filesystem::is_directory(tab_dir))
                                folder_path = tab_dir;
                            else if (!(folder_path = xset_get_s("go_set_default")))
                            {
                                if (geteuid() != 0)
                                    folder_path = vfs_user_home_dir().c_str();
                                else
                                    folder_path = ztd::strdup("/");
                            }
                            fm_main_window_add_new_tab(main_window, folder_path);
                            tab_added = true;
                        }
                        free(tab_dir);
                    }
                    if (set->x && !set->ob1)
                    {
                        // set current tab
                        cur_tabx = strtol(set->x, nullptr, 10);
                        if (cur_tabx >= 0 && cur_tabx < gtk_notebook_get_n_pages(GTK_NOTEBOOK(
                                                            main_window->panel[p - 1])))
                        {
                            gtk_notebook_set_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]),
                                                          cur_tabx);
                            file_browser = PTK_FILE_BROWSER(
                                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]),
                                                          cur_tabx));
                            // if ( file_browser->folder_view )
                            //      gtk_widget_grab_focus( file_browser->folder_view );
                            // LOG_INFO("call delayed (showpanels) #{} {:p} window={:p}", cur_tabx,
                            // fmt::ptr(file_browser->folder_view), fmt::ptr(main_window));
                            g_idle_add((GSourceFunc)delayed_focus, file_browser->folder_view);
                        }
                    }
                    free(set->ob1);
                    set->ob1 = nullptr;
                    free(tabs_add);
                }
                if (!tab_added)
                {
                    // open default tab
                    if (!(folder_path = xset_get_s("go_set_default")))
                    {
                        if (geteuid() != 0)
                            folder_path = vfs_user_home_dir().c_str();
                        else
                            folder_path = ztd::strdup("/");
                    }
                    fm_main_window_add_new_tab(main_window, folder_path);
                }
            }
            if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
                !gtk_widget_get_visible(GTK_WIDGET(main_window->panel[p - 1])))
                main_window_event(main_window,
                                  event_handler.pnl_show,
                                  "evt_pnl_show",
                                  p,
                                  0,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  true);
            gtk_widget_show(GTK_WIDGET(main_window->panel[p - 1]));
            if (!gtk_toggle_tool_button_get_active(
                    GTK_TOGGLE_TOOL_BUTTON(main_window->panel_btn[p - 1])))
            {
                g_signal_handlers_block_matched(main_window->panel_btn[p - 1],
                                                G_SIGNAL_MATCH_FUNC,
                                                0,
                                                0,
                                                nullptr,
                                                (void*)on_toggle_panelbar,
                                                nullptr);
                gtk_toggle_tool_button_set_active(
                    GTK_TOGGLE_TOOL_BUTTON(main_window->panel_btn[p - 1]),
                    true);
                g_signal_handlers_unblock_matched(main_window->panel_btn[p - 1],
                                                  G_SIGNAL_MATCH_FUNC,
                                                  0,
                                                  0,
                                                  nullptr,
                                                  (void*)on_toggle_panelbar,
                                                  nullptr);
            }
        }
        else
        {
            // not shown
            if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
                gtk_widget_get_visible(GTK_WIDGET(main_window->panel[p - 1])))
                main_window_event(main_window,
                                  event_handler.pnl_show,
                                  "evt_pnl_show",
                                  p,
                                  0,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  false);
            gtk_widget_hide(GTK_WIDGET(main_window->panel[p - 1]));
            if (gtk_toggle_tool_button_get_active(
                    GTK_TOGGLE_TOOL_BUTTON(main_window->panel_btn[p - 1])))
            {
                g_signal_handlers_block_matched(main_window->panel_btn[p - 1],
                                                G_SIGNAL_MATCH_FUNC,
                                                0,
                                                0,
                                                nullptr,
                                                (void*)on_toggle_panelbar,
                                                nullptr);
                gtk_toggle_tool_button_set_active(
                    GTK_TOGGLE_TOOL_BUTTON(main_window->panel_btn[p - 1]),
                    false);
                g_signal_handlers_unblock_matched(main_window->panel_btn[p - 1],
                                                  G_SIGNAL_MATCH_FUNC,
                                                  0,
                                                  0,
                                                  nullptr,
                                                  (void*)on_toggle_panelbar,
                                                  nullptr);
            }
        }
    }
    if (show[1] || show[2])
        gtk_widget_show(GTK_WIDGET(main_window->hpane_top));
    else
        gtk_widget_hide(GTK_WIDGET(main_window->hpane_top));
    if (show[3] || show[4])
        gtk_widget_show(GTK_WIDGET(main_window->hpane_bottom));
    else
        gtk_widget_hide(GTK_WIDGET(main_window->hpane_bottom));

    // current panel hidden?
    if (!xset_get_b_panel(main_window->curpanel, "show"))
    {
        p = main_window->curpanel + 1;
        if (p > 4)
            p = 1;
        while (p != main_window->curpanel)
        {
            if (xset_get_b_panel(p, "show"))
            {
                main_window->curpanel = p;
                main_window->notebook = main_window->panel[p - 1];
                cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->notebook));
                file_browser = PTK_FILE_BROWSER(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->notebook), cur_tabx));
                gtk_widget_grab_focus(file_browser->folder_view);
                break;
            }
            p++;
            if (p > 4)
                p = 1;
        }
    }
    set_panel_focus(main_window, nullptr);

    // update views all panels
    for (p = 1; p < 5; p++)
    {
        if (show[p])
        {
            cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
            if (cur_tabx != -1)
            {
                file_browser = PTK_FILE_BROWSER(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), cur_tabx));
                if (file_browser)
                    ptk_file_browser_update_views(nullptr, file_browser);
            }
        }
    }
}

static void
on_toggle_panelbar(GtkWidget* widget, FMMainWindow* main_window)
{
    // LOG_INFO("on_toggle_panelbar");
    int i;
    for (i = 0; i < 4; i++)
    {
        if (widget == main_window->panel_btn[i])
        {
            xset_set_b_panel(i + 1,
                             "show",
                             gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)));
            break;
        }
    }
    show_panels_all_windows(nullptr, main_window);
}

static bool
on_menu_bar_event(GtkWidget* widget, GdkEvent* event, FMMainWindow* main_window)
{
    (void)widget;
    (void)event;
    rebuild_menus(main_window);
    return false;
}

static void
on_bookmarks_show(GtkMenuItem* item, FMMainWindow* main_window)
{
    (void)item;
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (!file_browser)
        return;
    int mode = main_window->panel_context[file_browser->mypanel - 1];

    xset_set_b_panel_mode(file_browser->mypanel, "show_book", mode, !file_browser->side_book);
    update_views_all_windows(nullptr, file_browser);
    if (file_browser->side_book)
    {
        ptk_bookmark_view_chdir(GTK_TREE_VIEW(file_browser->side_book), file_browser, true);
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->side_book));
    }
}

static void
rebuild_menus(FMMainWindow* main_window)
{
    GtkWidget* newmenu;
    char* menu_elements;
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSet* set;
    XSet* child_set;
    char* str;

    // LOG_INFO("rebuild_menus");
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (!file_browser)
        return;
    XSetContext* context = xset_context_new();
    main_context_fill(file_browser, context);

    // File
    newmenu = gtk_menu_new();
    xset_set_cb("main_new_window", (GFunc)on_new_window_activate, main_window);
    xset_set_cb("main_root_window", (GFunc)on_open_current_folder_as_root, main_window);
    xset_set_cb("main_search", (GFunc)on_find_file_activate, main_window);
    xset_set_cb("main_terminal", (GFunc)on_open_terminal_activate, main_window);
    xset_set_cb("main_root_terminal", (GFunc)on_open_root_terminal_activate, main_window);
    xset_set_cb("main_save_session", (GFunc)on_open_url, main_window);
    xset_set_cb("main_exit", (GFunc)on_quit_activate, main_window);
    menu_elements = ztd::strdup(
        "main_save_session main_search separator main_terminal main_root_terminal "
        "main_new_window main_root_window separator main_save_tabs separator main_exit");
    xset_add_menu(file_browser, newmenu, accel_group, menu_elements);
    free(menu_elements);
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->file_menu_item), newmenu);

    // View
    newmenu = gtk_menu_new();
    xset_set_cb("main_prefs", (GFunc)on_preference_activate, main_window);
    xset_set_cb("main_full", (GFunc)on_fullscreen_activate, main_window);
    xset_set_cb("main_design_mode", (GFunc)main_design_mode, main_window);
    xset_set_cb("main_icon", (GFunc)on_main_icon, nullptr);
    xset_set_cb("main_title", (GFunc)update_window_title, main_window);

    int p;
    int vis_count = 0;
    for (p = 1; p < 5; p++)
    {
        if (xset_get_b_panel(p, "show"))
            vis_count++;
    }
    if (!vis_count)
    {
        xset_set_b_panel(1, "show", true);
        vis_count++;
    }
    set = xset_set_cb("panel1_show", (GFunc)show_panels_all_windows, main_window);
    set->disable = (main_window->curpanel == 1 && vis_count == 1);
    set = xset_set_cb("panel2_show", (GFunc)show_panels_all_windows, main_window);
    set->disable = (main_window->curpanel == 2 && vis_count == 1);
    set = xset_set_cb("panel3_show", (GFunc)show_panels_all_windows, main_window);
    set->disable = (main_window->curpanel == 3 && vis_count == 1);
    set = xset_set_cb("panel4_show", (GFunc)show_panels_all_windows, main_window);
    set->disable = (main_window->curpanel == 4 && vis_count == 1);

    xset_set_cb("main_pbar", (GFunc)show_panels_all_windows, main_window);

    set = xset_set_cb("panel_prev", (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", -1);
    set->disable = (vis_count == 1);
    set = xset_set_cb("panel_next", (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", -2);
    set->disable = (vis_count == 1);
    set = xset_set_cb("panel_hide", (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", -3);
    set->disable = (vis_count == 1);
    set = xset_set_cb("panel_1", (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", 1);
    set->disable = (main_window->curpanel == 1);
    set = xset_set_cb("panel_2", (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", 2);
    set->disable = (main_window->curpanel == 2);
    set = xset_set_cb("panel_3", (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", 3);
    set->disable = (main_window->curpanel == 3);
    set = xset_set_cb("panel_4", (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", 4);
    set->disable = (main_window->curpanel == 4);

    menu_elements =
        ztd::strdup("panel1_show panel2_show panel3_show panel4_show main_pbar main_focus_panel");
    char* menu_elements2 = ztd::strdup(
        "separator main_tasks main_auto separator main_title main_icon main_full separator "
        "main_design_mode main_prefs");

    main_task_prepare_menu(main_window, newmenu, accel_group);
    xset_add_menu(file_browser, newmenu, accel_group, menu_elements);

    // Panel View submenu
    set = xset_get("con_view");
    str = set->menu_label;
    set->menu_label = g_strdup_printf("%s %d %s", "Panel", main_window->curpanel, set->menu_label);
    ptk_file_menu_add_panel_view_menu(file_browser, newmenu, accel_group);
    free(set->menu_label);
    set->menu_label = str;

    xset_add_menu(file_browser, newmenu, accel_group, menu_elements2);
    free(menu_elements);
    free(menu_elements2);
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->view_menu_item), newmenu);

    // Devices
    main_window->dev_menu = create_devices_menu(main_window);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->dev_menu_item), main_window->dev_menu);
    g_signal_connect(main_window->dev_menu,
                     "key-press-event",
                     G_CALLBACK(xset_menu_keypress),
                     nullptr);

    // Bookmarks
    newmenu = gtk_menu_new();
    set = xset_set_cb("book_show", (GFunc)on_bookmarks_show, main_window);
    set->b = file_browser->side_book ? XSET_B_TRUE : XSET_B_UNSET;
    xset_add_menuitem(file_browser, newmenu, accel_group, set);
    set = xset_set_cb("book_add", (GFunc)ptk_bookmark_view_add_bookmark, file_browser);
    set->disable = false;
    xset_add_menuitem(file_browser, newmenu, accel_group, set);
    gtk_menu_shell_append(GTK_MENU_SHELL(newmenu), gtk_separator_menu_item_new());
    xset_add_menuitem(file_browser,
                      newmenu,
                      accel_group,
                      ptk_bookmark_view_get_first_bookmark(nullptr));
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->book_menu_item), newmenu);

    // Plugins
    main_window->plug_menu = create_plugins_menu(main_window);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->plug_menu_item), main_window->plug_menu);
    g_signal_connect(main_window->plug_menu,
                     "key-press-event",
                     G_CALLBACK(xset_menu_keypress),
                     nullptr);

    // Tool
    newmenu = gtk_menu_new();
    set = xset_get("main_tool");
    if (!set->child)
    {
        child_set = xset_custom_new();
        child_set->menu_label = ztd::strdup("New _Command");
        child_set->parent = ztd::strdup("main_tool");
        set->child = ztd::strdup(child_set->name);
    }
    else
        child_set = xset_get(set->child);
    xset_add_menuitem(file_browser, newmenu, accel_group, child_set);
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->tool_menu_item), newmenu);

    // Help
    newmenu = gtk_menu_new();
    xset_set_cb("main_about", (GFunc)on_about_activate, main_window);
    menu_elements = ztd::strdup("main_about");
    xset_add_menu(file_browser, newmenu, accel_group, menu_elements);
    free(menu_elements);
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->help_menu_item), newmenu);
    // LOG_INFO("rebuild_menus  DONE");
}

static void
on_main_window_realize(GtkWidget* widget, FMMainWindow* main_window)
{
    (void)widget;
    // preset the task manager height for no double-resize on first show
    idle_set_task_height(main_window);
}

static void
fm_main_window_init(FMMainWindow* main_window)
{
    int i;
    const char* icon_name;
    XSet* set;

    main_window->configure_evt_timer = 0;
    main_window->fullscreen = false;
    main_window->opened_maximized = main_window->maximized = app_settings.maximized;

    /* this is used to limit the scope of gtk_grab and modal dialogs */
    main_window->wgroup = gtk_window_group_new();
    gtk_window_group_add_window(main_window->wgroup, GTK_WINDOW(main_window));

    /* Add to total window count */
    ++n_windows;
    all_windows.push_back(main_window);

    WindowReference::increase();

    // g_signal_connect( G_OBJECT( main_window ), "task-notify",
    //                            G_CALLBACK( ptk_file_task_notify_handler ), nullptr );

    /* Start building GUI */
    /*
    NOTE: gtk_window_set_icon_name doesn't work under some WMs, such as IceWM.
    gtk_window_set_icon_name( GTK_WINDOW( main_window ),
                              "gnome-fs-directory" ); */
    if (theme_change_notify == 0)
    {
        theme_change_notify = g_signal_connect(gtk_icon_theme_get_default(),
                                               "changed",
                                               G_CALLBACK(update_window_icons),
                                               nullptr);
    }
    update_window_icon(GTK_WINDOW(main_window), gtk_icon_theme_get_default());

    main_window->main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(main_window), main_window->main_vbox);

    // Create menu bar
    main_window->accel_group = gtk_accel_group_new();
    main_window->menu_bar = gtk_menu_bar_new();
    GtkWidget* menu_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(menu_hbox), main_window->menu_bar, true, true, 0);

    // panelbar
    main_window->panelbar = gtk_toolbar_new();
    GtkStyleContext* style_ctx = gtk_widget_get_style_context(main_window->panelbar);
    gtk_style_context_add_class(style_ctx, GTK_STYLE_CLASS_MENUBAR);
    gtk_style_context_remove_class(style_ctx, GTK_STYLE_CLASS_TOOLBAR);
    gtk_toolbar_set_show_arrow(GTK_TOOLBAR(main_window->panelbar), false);
    gtk_toolbar_set_style(GTK_TOOLBAR(main_window->panelbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(main_window->panelbar), GTK_ICON_SIZE_MENU);
    // set pbar background to menu bar background
    // gtk_widget_modify_bg( main_window->panelbar, GTK_STATE_NORMAL,
    //                                    &GTK_WIDGET( main_window )
    //                                    ->style->bg[ GTK_STATE_NORMAL ] );
    for (i = 0; i < 4; i++)
    {
        main_window->panel_btn[i] = GTK_WIDGET(gtk_toggle_tool_button_new());
        icon_name = g_strdup_printf("Show Panel %d", i + 1);
        gtk_tool_button_set_label(GTK_TOOL_BUTTON(main_window->panel_btn[i]), icon_name);
        free((char*)icon_name);
        set = xset_get_panel(i + 1, "icon_status");
        if (set->icon && set->icon[0] != '\0')
            icon_name = set->icon;
        else
            icon_name = ztd::strdup("gtk-yes");
        main_window->panel_image[i] = xset_get_image(icon_name, GTK_ICON_SIZE_MENU);
        gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(main_window->panel_btn[i]),
                                        main_window->panel_image[i]);
        gtk_toolbar_insert(GTK_TOOLBAR(main_window->panelbar),
                           GTK_TOOL_ITEM(main_window->panel_btn[i]),
                           -1);
        g_signal_connect(main_window->panel_btn[i],
                         "toggled",
                         G_CALLBACK(on_toggle_panelbar),
                         main_window);
        if (i == 1)
        {
            GtkToolItem* sep = gtk_separator_tool_item_new();
            gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(sep), true);
            gtk_toolbar_insert(GTK_TOOLBAR(main_window->panelbar), sep, -1);
        }
    }
    gtk_box_pack_start(GTK_BOX(menu_hbox), main_window->panelbar, true, true, 0);
    gtk_box_pack_start(GTK_BOX(main_window->main_vbox), menu_hbox, false, false, 0);

    main_window->file_menu_item = gtk_menu_item_new_with_mnemonic("_File");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->file_menu_item);

    main_window->view_menu_item = gtk_menu_item_new_with_mnemonic("_View");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->view_menu_item);

    main_window->dev_menu_item = gtk_menu_item_new_with_mnemonic("_Devices");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->dev_menu_item);
    main_window->dev_menu = nullptr;

    main_window->book_menu_item = gtk_menu_item_new_with_mnemonic("_Bookmarks");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->book_menu_item);

    main_window->plug_menu_item = gtk_menu_item_new_with_mnemonic("_Plugins");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->plug_menu_item);
    main_window->plug_menu = nullptr;

    main_window->tool_menu_item = gtk_menu_item_new_with_mnemonic("_Tools");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->tool_menu_item);

    main_window->help_menu_item = gtk_menu_item_new_with_mnemonic("_Help");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->help_menu_item);

    rebuild_menus(main_window);

    /* Create client area */
    main_window->task_vpane = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    main_window->vpane = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    main_window->hpane_top = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    main_window->hpane_bottom = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

    for (i = 0; i < 4; i++)
    {
        main_window->panel[i] = gtk_notebook_new();
        gtk_notebook_set_show_border(GTK_NOTEBOOK(main_window->panel[i]), false);
        gtk_notebook_set_scrollable(GTK_NOTEBOOK(main_window->panel[i]), true);
        g_signal_connect(main_window->panel[i],
                         "switch-page",
                         G_CALLBACK(on_folder_notebook_switch_pape),
                         main_window);
    }

    main_window->task_scroll = gtk_scrolled_window_new(nullptr, nullptr);

    gtk_paned_pack1(GTK_PANED(main_window->hpane_top), main_window->panel[0], false, true);
    gtk_paned_pack2(GTK_PANED(main_window->hpane_top), main_window->panel[1], true, true);
    gtk_paned_pack1(GTK_PANED(main_window->hpane_bottom), main_window->panel[2], false, true);
    gtk_paned_pack2(GTK_PANED(main_window->hpane_bottom), main_window->panel[3], true, true);

    gtk_paned_pack1(GTK_PANED(main_window->vpane), main_window->hpane_top, false, true);
    gtk_paned_pack2(GTK_PANED(main_window->vpane), main_window->hpane_bottom, true, true);

    gtk_paned_pack1(GTK_PANED(main_window->task_vpane), main_window->vpane, true, true);
    gtk_paned_pack2(GTK_PANED(main_window->task_vpane), main_window->task_scroll, false, true);

    gtk_box_pack_start(GTK_BOX(main_window->main_vbox),
                       GTK_WIDGET(main_window->task_vpane),
                       true,
                       true,
                       0);

    main_window->notebook = main_window->panel[0];
    main_window->curpanel = 1;

    // Task View
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(main_window->task_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    main_window->task_view = main_task_view_new(main_window);
    gtk_container_add(GTK_CONTAINER(main_window->task_scroll), GTK_WIDGET(main_window->task_view));

    gtk_window_set_role(GTK_WINDOW(main_window), "file_manager");

    gtk_widget_show_all(main_window->main_vbox);

    g_signal_connect(G_OBJECT(main_window->file_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->view_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->dev_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->book_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->plug_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->tool_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->help_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);

    // use this OR widget_class->key_press_event = on_main_window_keypress;
    g_signal_connect(G_OBJECT(main_window),
                     "key-press-event",
                     G_CALLBACK(on_main_window_keypress),
                     nullptr);

    g_signal_connect(main_window, "focus-in-event", G_CALLBACK(on_main_window_focus), nullptr);

    g_signal_connect(G_OBJECT(main_window),
                     "configure-event",
                     G_CALLBACK(on_window_configure_event),
                     main_window);

    g_signal_connect(G_OBJECT(main_window),
                     "button-press-event",
                     G_CALLBACK(on_window_button_press_event),
                     main_window);

    g_signal_connect(G_OBJECT(main_window),
                     "realize",
                     G_CALLBACK(on_main_window_realize),
                     main_window);

    main_window->panel_change = false;
    show_panels(nullptr, main_window);

    gtk_widget_hide(GTK_WIDGET(main_window->task_scroll));
    on_task_popup_show(nullptr, main_window, nullptr);

    // show window
    gtk_window_set_default_size(GTK_WINDOW(main_window), app_settings.width, app_settings.height);
    if (app_settings.maximized)
        gtk_window_maximize(GTK_WINDOW(main_window));
    gtk_widget_show(GTK_WIDGET(main_window));

    // restore panel sliders
    // do this after maximizing/showing window so slider positions are valid
    // in actual window size
    int pos = xset_get_int("panel_sliders", "x");
    if (pos < 200)
        pos = 200;
    gtk_paned_set_position(GTK_PANED(main_window->hpane_top), pos);
    pos = xset_get_int("panel_sliders", "y");
    if (pos < 200)
        pos = 200;
    gtk_paned_set_position(GTK_PANED(main_window->hpane_bottom), pos);
    pos = xset_get_int("panel_sliders", "s");
    if (pos < 200)
        pos = -1;
    gtk_paned_set_position(GTK_PANED(main_window->vpane), pos);

    // build the main menu initially, eg for F10 - Note: file_list is nullptr
    // NOT doing this because it slows down the initial opening of the window
    // and shows a stale menu anyway.
    // rebuild_menus( main_window );

    main_window_event(main_window, nullptr, "evt_win_new", 0, 0, nullptr, 0, 0, 0, true);
}

static void
fm_main_window_finalize(GObject* obj)
{
    all_windows.erase(std::remove(all_windows.begin(), all_windows.end(), FM_MAIN_WINDOW(obj)),
                      all_windows.end());
    --n_windows;

    g_object_unref((FM_MAIN_WINDOW(obj))->wgroup);

    WindowReference::decrease();

    /* Remove the monitor for changes of the bookmarks */
    //    ptk_bookmarks_remove_callback( ( GFunc ) on_bookmarks_change, obj );
    if (n_windows == 0)
    {
        g_signal_handler_disconnect(gtk_icon_theme_get_default(), theme_change_notify);
        theme_change_notify = 0;
    }

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
fm_main_window_get_property(GObject* obj, unsigned int prop_id, GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static void
fm_main_window_set_property(GObject* obj, unsigned int prop_id, const GValue* value,
                            GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static void
fm_main_window_close(FMMainWindow* main_window)
{
    /*
    LOG_INFO("DISC={}", g_signal_handlers_disconnect_by_func(
                            G_OBJECT(main_window),
                            G_CALLBACK(ptk_file_task_notify_handler), nullptr));
    */
    if (event_handler.win_close->s || event_handler.win_close->ob2_data)
        main_window_event(main_window,
                          event_handler.win_close,
                          "evt_win_close",
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          false);
    gtk_widget_destroy(GTK_WIDGET(main_window));
}

static void
on_abort_tasks_response(GtkDialog* dlg, int response, GtkWidget* main_window)
{
    (void)dlg;
    (void)response;
    fm_main_window_close(FM_MAIN_WINDOW(main_window));
}

void
fm_main_window_store_positions(FMMainWindow* main_window)
{
    if (!main_window)
        main_window = fm_main_window_get_last_active();
    if (!main_window)
        return;
    // if the window is not fullscreen (is normal or maximized) save sliders
    // and columns
    if (!main_window->fullscreen)
    {
        // store width/height + sliders
        int pos;
        char* posa;
        GtkAllocation allocation;
        gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);

        if (!main_window->maximized && allocation.width > 0)
        {
            app_settings.width = allocation.width;
            app_settings.height = allocation.height;
        }
        if (GTK_IS_PANED(main_window->hpane_top))
        {
            pos = gtk_paned_get_position(GTK_PANED(main_window->hpane_top));
            if (pos)
            {
                posa = g_strdup_printf("%d", pos);
                xset_set("panel_sliders", "x", posa);
                free(posa);
            }

            pos = gtk_paned_get_position(GTK_PANED(main_window->hpane_bottom));
            if (pos)
            {
                posa = g_strdup_printf("%d", pos);
                xset_set("panel_sliders", "y", posa);
                free(posa);
            }

            pos = gtk_paned_get_position(GTK_PANED(main_window->vpane));
            if (pos)
            {
                posa = g_strdup_printf("%d", pos);
                xset_set("panel_sliders", "s", posa);
                free(posa);
            }
            if (gtk_widget_get_visible(main_window->task_scroll))
            {
                pos = gtk_paned_get_position(GTK_PANED(main_window->task_vpane));
                if (pos)
                {
                    // save absolute height
                    posa = g_strdup_printf("%d", allocation.height - pos);
                    xset_set("task_show_manager", "x", posa);
                    free(posa);
                    // LOG_INFO("CLOS  win {}x{}    task height {}   slider {}", allocation.width,
                    // allocation.height, allocation.height - pos, pos);
                }
            }
        }

        // store task columns
        on_task_columns_changed(main_window->task_view, nullptr);

        // store fb columns
        int p, page_x;
        PtkFileBrowser* a_browser;
        if (main_window->maximized)
            main_window->opened_maximized = true; // force save of columns
        for (p = 1; p < 5; p++)
        {
            page_x = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
            if (page_x != -1)
            {
                a_browser = PTK_FILE_BROWSER(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), page_x));
                if (a_browser && a_browser->view_mode == PTK_FB_LIST_VIEW)
                    ptk_file_browser_save_column_widths(GTK_TREE_VIEW(a_browser->folder_view),
                                                        a_browser);
            }
        }
    }
}

static gboolean
fm_main_window_delete_event(GtkWidget* widget, GdkEventAny* event)
{
    (void)event;
    // LOG_INFO("fm_main_window_delete_event");

    FMMainWindow* main_window = FM_MAIN_WINDOW(widget);

    fm_main_window_store_positions(main_window);

    // save settings
    app_settings.maximized = main_window->maximized;
    autosave_cancel();
    save_settings(main_window);

    // tasks running?
    if (main_tasks_running(main_window))
    {
        GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(widget),
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_QUESTION,
                                                GTK_BUTTONS_YES_NO,
                                                "Stop all tasks running in this window?");
        gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_NO);
        if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES)
        {
            gtk_widget_destroy(dlg);
            dlg = gtk_message_dialog_new(GTK_WINDOW(widget),
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_CLOSE,
                                         "Aborting tasks...");
            g_signal_connect(dlg, "response", G_CALLBACK(on_abort_tasks_response), widget);
            g_signal_connect(dlg, "destroy", G_CALLBACK(gtk_widget_destroy), dlg);
            gtk_widget_show_all(dlg);

            on_task_stop(nullptr, main_window->task_view, xset_get("task_stop_all"), nullptr);
            while (main_tasks_running(main_window))
            {
                while (gtk_events_pending())
                    gtk_main_iteration();
            }
        }
        else
        {
            gtk_widget_destroy(dlg);
            return true;
        }
    }
    fm_main_window_close(main_window);
    return true;
}

static gboolean
fm_main_window_window_state_event(GtkWidget* widget, GdkEventWindowState* event)
{
    FMMainWindow* main_window = FM_MAIN_WINDOW(widget);

    main_window->maximized = app_settings.maximized =
        ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0);
    if (!main_window->maximized)
    {
        if (main_window->opened_maximized)
            main_window->opened_maximized = false;
        show_panels(nullptr, main_window); // restore columns
    }

    return true;
}

char*
main_window_get_tab_cwd(PtkFileBrowser* file_browser, int tab_num)
{
    if (!file_browser)
        return nullptr;
    int page_x;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    GtkWidget* notebook = main_window->panel[file_browser->mypanel - 1];
    int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    int page_num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), GTK_WIDGET(file_browser));

    switch (tab_num)
    {
        case -1:
            // prev
            page_x = page_num - 1;
            break;
        case -2:
            // next
            page_x = page_num + 1;
            break;
        default:
            // tab_num starts counting at 1
            page_x = tab_num - 1;
            break;
    }

    if (page_x > -1 && page_x < pages)
    {
        return ztd::strdup(ptk_file_browser_get_cwd(
            PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_x))));
    }
    else
        return nullptr;
}

char*
main_window_get_panel_cwd(PtkFileBrowser* file_browser, int panel_num)
{
    if (!file_browser)
        return nullptr;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    int panel_x = file_browser->mypanel;

    switch (panel_num)
    {
        case -1:
            // prev
            do
            {
                if (--panel_x < 1)
                    panel_x = 4;
                if (panel_x == file_browser->mypanel)
                    return nullptr;
            } while (!gtk_widget_get_visible(main_window->panel[panel_x - 1]));
            break;
        case -2:
            // next
            do
            {
                if (++panel_x > 4)
                    panel_x = 1;
                if (panel_x == file_browser->mypanel)
                    return nullptr;
            } while (!gtk_widget_get_visible(main_window->panel[panel_x - 1]));
            break;
        default:
            panel_x = panel_num;
            if (!gtk_widget_get_visible(main_window->panel[panel_x - 1]))
                return nullptr;
            break;
    }

    GtkWidget* notebook = main_window->panel[panel_x - 1];
    int page_x = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    return ztd::strdup(ptk_file_browser_get_cwd(
        PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_x))));
}

void
main_window_open_in_panel(PtkFileBrowser* file_browser, int panel_num, char* file_path)
{
    if (!file_browser)
        return;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    int panel_x = file_browser->mypanel;

    switch (panel_num)
    {
        case -1:
            // prev
            do
            {
                if (--panel_x < 1)
                    panel_x = 4;
                if (panel_x == file_browser->mypanel)
                    return;
            } while (!gtk_widget_get_visible(main_window->panel[panel_x - 1]));
            break;
        case -2:
            // next
            do
            {
                if (++panel_x > 4)
                    panel_x = 1;
                if (panel_x == file_browser->mypanel)
                    return;
            } while (!gtk_widget_get_visible(main_window->panel[panel_x - 1]));
            break;
        default:
            panel_x = panel_num;
            break;
    }

    if (panel_x < 1 || panel_x > 4)
        return;

    // show panel
    if (!gtk_widget_get_visible(main_window->panel[panel_x - 1]))
    {
        xset_set_b_panel(panel_x, "show", true);
        show_panels_all_windows(nullptr, main_window);
    }

    // open in tab in panel
    int save_curpanel = main_window->curpanel;

    main_window->curpanel = panel_x;
    main_window->notebook = main_window->panel[panel_x - 1];

    fm_main_window_add_new_tab(main_window, file_path);

    main_window->curpanel = save_curpanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    // focus original panel
    // while( gtk_events_pending() )
    //    gtk_main_iteration();
    // gtk_widget_grab_focus( GTK_WIDGET( main_window->notebook ) );
    // gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
    g_idle_add((GSourceFunc)delayed_focus_file_browser, file_browser);
}

bool
main_window_panel_is_visible(PtkFileBrowser* file_browser, int panel)
{
    if (panel < 1 || panel > 4)
        return false;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    return gtk_widget_get_visible(main_window->panel[panel - 1]);
}

void
main_window_get_counts(PtkFileBrowser* file_browser, int* panel_count, int* tab_count, int* tab_num)
{
    if (!file_browser)
        return;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    GtkWidget* notebook = main_window->panel[file_browser->mypanel - 1];
    *tab_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    // tab_num starts counting from 1
    *tab_num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), GTK_WIDGET(file_browser)) + 1;
    int count = 0;
    int i;
    for (i = 0; i < 4; i++)
    {
        if (gtk_widget_get_visible(main_window->panel[i]))
            count++;
    }
    *panel_count = count;
}

void
on_restore_notebook_page(GtkButton* btn, PtkFileBrowser* file_browser)
{
    (void)btn;

    if (closed_tabs_restore.empty())
    {
        LOG_INFO("No tabs to restore");
        return;
    }

    std::string file_path = closed_tabs_restore.back();
    closed_tabs_restore.pop_back();
    // LOG_INFO("on_restore_notebook_page path={}", file_path);

    // LOG_INFO("on_restore_notebook_page fb={:p}", fmt::ptr(file_browser));
    if (!GTK_IS_WIDGET(file_browser))
        return;

    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    fm_main_window_add_new_tab(main_window, file_path.c_str());
}

void
on_close_notebook_page(GtkButton* btn, PtkFileBrowser* file_browser)
{
    (void)btn;
    PtkFileBrowser* a_browser;

    closed_tabs_restore.push_back(ptk_file_browser_get_cwd(file_browser));
    // LOG_INFO("on_close_notebook_page path={}", closed_tabs_restore.back());

    // LOG_INFO("on_close_notebook_page fb={:p}", fmt::ptr(file_browser));
    if (!GTK_IS_WIDGET(file_browser))
        return;
    GtkNotebook* notebook =
        GTK_NOTEBOOK(gtk_widget_get_ancestor(GTK_WIDGET(file_browser), GTK_TYPE_NOTEBOOK));
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);

    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    if (event_handler.tab_close->s || event_handler.tab_close->ob2_data)
        main_window_event(
            main_window,
            event_handler.tab_close,
            "evt_tab_close",
            file_browser->mypanel,
            gtk_notebook_page_num(GTK_NOTEBOOK(main_window->notebook), GTK_WIDGET(file_browser)) +
                1,
            nullptr,
            0,
            0,
            0,
            false);

    // save solumns and slider positions of tab to be closed
    ptk_file_browser_slider_release(nullptr, nullptr, file_browser);
    ptk_file_browser_save_column_widths(GTK_TREE_VIEW(file_browser->folder_view), file_browser);

    // without this signal blocked, on_close_notebook_page is called while
    // ptk_file_browser_update_views is still in progress causing segfault
    g_signal_handlers_block_matched(main_window->notebook,
                                    G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_folder_notebook_switch_pape,
                                    nullptr);

    // remove page can also be used to destroy - same result
    // gtk_notebook_remove_page( notebook, gtk_notebook_get_current_page( notebook ) );
    gtk_widget_destroy(GTK_WIDGET(file_browser));

    if (!app_settings.always_show_tabs)
    {
        if (gtk_notebook_get_n_pages(notebook) == 1)
            gtk_notebook_set_show_tabs(notebook, false);
    }
    if (gtk_notebook_get_n_pages(notebook) == 0)
    {
        const char* path = xset_get_s("go_set_default");
        if (!(path && path[0] != '\0'))
        {
            if (geteuid() != 0)
                path = vfs_user_home_dir().c_str();
            else
                path = ztd::strdup("/");
        }
        fm_main_window_add_new_tab(main_window, path);
        a_browser = PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0));
        if (GTK_IS_WIDGET(a_browser))
            ptk_file_browser_update_views(nullptr, a_browser);
        goto _done_close;
    }

    // update view of new current tab
    int cur_tabx;
    cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->notebook));
    if (cur_tabx != -1)
    {
        a_browser = PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), cur_tabx));

        ptk_file_browser_update_views(nullptr, a_browser);
        if (GTK_IS_WIDGET(a_browser))
        {
            fm_main_window_update_status_bar(main_window, a_browser);
            g_idle_add((GSourceFunc)delayed_focus, a_browser->folder_view);
        }
        if (event_handler.tab_focus->s || event_handler.tab_focus->ob2_data)
            main_window_event(main_window,
                              event_handler.tab_focus,
                              "evt_tab_focus",
                              main_window->curpanel,
                              cur_tabx + 1,
                              nullptr,
                              0,
                              0,
                              0,
                              false);
    }

_done_close:
    g_signal_handlers_unblock_matched(main_window->notebook,
                                      G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_folder_notebook_switch_pape,
                                      nullptr);

    update_window_title(nullptr, main_window);
    if (xset_get_b("main_save_tabs"))
        autosave_request();
}

static bool
notebook_clicked(GtkWidget* widget, GdkEventButton* event,
                 PtkFileBrowser* file_browser) // MOD added
{
    (void)widget;
    on_file_browser_panel_change(file_browser,
                                 static_cast<FMMainWindow*>(file_browser->main_window));
    if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
        main_window_event(file_browser->main_window,
                          event_handler.win_click,
                          "evt_win_click",
                          0,
                          0,
                          "tabbar",
                          0,
                          event->button,
                          event->state,
                          true))
        return true;
    // middle-click on tab closes
    if (event->type == GDK_BUTTON_PRESS)
    {
        if (event->button == 2)
        {
            on_close_notebook_page(nullptr, file_browser);
            return true;
        }
        else if (event->button == 3)
        {
            GtkWidget* popup = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();
            XSetContext* context = xset_context_new();
            main_context_fill(file_browser, context);

            XSet* set;

            set = xset_set_cb("tab_close", (GFunc)on_close_notebook_page, file_browser);
            xset_add_menuitem(file_browser, popup, accel_group, set);
            set = xset_set_cb("tab_restore", (GFunc)on_restore_notebook_page, file_browser);
            xset_add_menuitem(file_browser, popup, accel_group, set);
            set = xset_set_cb("tab_new", (GFunc)ptk_file_browser_new_tab, file_browser);
            xset_add_menuitem(file_browser, popup, accel_group, set);
            set = xset_set_cb("tab_new_here", (GFunc)ptk_file_browser_new_tab_here, file_browser);
            xset_add_menuitem(file_browser, popup, accel_group, set);
            gtk_widget_show_all(GTK_WIDGET(popup));
            g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
            g_signal_connect(popup, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
            gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
            return true;
        }
    }
    return false;
}

static void
on_file_browser_begin_chdir(PtkFileBrowser* file_browser, FMMainWindow* main_window)
{
    fm_main_window_update_status_bar(main_window, file_browser);
}

static void
on_file_browser_after_chdir(PtkFileBrowser* file_browser, FMMainWindow* main_window)
{
    // fm_main_window_stop_busy_task( main_window );

    if (fm_main_window_get_current_file_browser(main_window) == GTK_WIDGET(file_browser))
    {
        set_window_title(main_window, file_browser);
        // gtk_entry_set_text( main_window->address_bar, file_browser->dir->disp_path );
        // gtk_statusbar_push( GTK_STATUSBAR( main_window->status_bar ), 0, "" );
        // fm_main_window_update_command_ui( main_window, file_browser );
    }

    // fm_main_window_update_tab_label( main_window, file_browser, file_browser->dir->disp_path );

    if (file_browser->inhibit_focus)
    {
        // complete ptk_file_browser.c ptk_file_browser_seek_path()
        file_browser->inhibit_focus = false;
        if (file_browser->seek_name)
        {
            ptk_file_browser_seek_path(file_browser, nullptr, file_browser->seek_name);
            free(file_browser->seek_name);
            file_browser->seek_name = nullptr;
        }
    }
    else
    {
        ptk_file_browser_select_last(file_browser);                   // MOD restore last selections
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view)); // MOD
    }
    if (xset_get_b("main_save_tabs"))
        autosave_request();

    if (event_handler.tab_chdir->s || event_handler.tab_chdir->ob2_data)
        main_window_event(main_window,
                          event_handler.tab_chdir,
                          "evt_tab_chdir",
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true);
}

GtkWidget*
fm_main_window_create_tab_label(FMMainWindow* main_window, PtkFileBrowser* file_browser)
{
    (void)main_window;
    GtkEventBox* evt_box;
    GtkWidget* tab_label;
    GtkWidget* tab_text = nullptr;
    GtkWidget* tab_icon = nullptr;
    GtkWidget* close_btn;
    GtkWidget* close_icon;
    GdkPixbuf* pixbuf = nullptr;
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();

    /* Create tab label */
    evt_box = GTK_EVENT_BOX(gtk_event_box_new());
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(evt_box), false);

    tab_label = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    XSet* set = xset_get_panel(file_browser->mypanel, "icon_tab");
    if (set->icon)
    {
        pixbuf = vfs_load_icon(icon_theme, set->icon, 16);
        if (pixbuf)
        {
            tab_icon = gtk_image_new_from_pixbuf(pixbuf);
            g_object_unref(pixbuf);
        }
        else
            tab_icon = xset_get_image(set->icon, GTK_ICON_SIZE_MENU);
    }
    if (!tab_icon)
        tab_icon = gtk_image_new_from_icon_name("gtk-directory", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(tab_label), tab_icon, false, false, 4);

    if (ptk_file_browser_get_cwd(file_browser))
    {
        char* name = g_path_get_basename(ptk_file_browser_get_cwd(file_browser));
        if (name)
        {
            tab_text = gtk_label_new(name);
            free(name);
        }
    }
    else
        tab_text = gtk_label_new("");

    gtk_label_set_ellipsize(GTK_LABEL(tab_text), PANGO_ELLIPSIZE_MIDDLE);
    if (strlen(gtk_label_get_text(GTK_LABEL(tab_text))) < 30)
    {
        gtk_label_set_ellipsize(GTK_LABEL(tab_text), PANGO_ELLIPSIZE_NONE);
        gtk_label_set_width_chars(GTK_LABEL(tab_text), -1);
    }
    else
        gtk_label_set_width_chars(GTK_LABEL(tab_text), 30);
    gtk_label_set_max_width_chars(GTK_LABEL(tab_text), 30);
    gtk_box_pack_start(GTK_BOX(tab_label), tab_text, false, false, 4);

    if (app_settings.show_close_tab_buttons)
    {
        close_btn = gtk_button_new();
        gtk_widget_set_focus_on_click(GTK_WIDGET(close_btn), false);
        gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
        close_icon = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_MENU);

        gtk_container_add(GTK_CONTAINER(close_btn), close_icon);
        gtk_box_pack_end(GTK_BOX(tab_label), close_btn, false, false, 0);
        g_signal_connect(G_OBJECT(close_btn),
                         "clicked",
                         G_CALLBACK(on_close_notebook_page),
                         file_browser);
    }

    gtk_container_add(GTK_CONTAINER(evt_box), tab_label);

    gtk_widget_set_events(GTK_WIDGET(evt_box), GDK_ALL_EVENTS_MASK);
    gtk_drag_dest_set(
        GTK_WIDGET(evt_box),
        GTK_DEST_DEFAULT_ALL,
        drag_targets,
        sizeof(drag_targets) / sizeof(GtkTargetEntry),
        GdkDragAction(GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
    g_signal_connect((void*)evt_box, "drag-motion", G_CALLBACK(on_tab_drag_motion), file_browser);

    // MOD  middle-click to close tab
    g_signal_connect(G_OBJECT(evt_box),
                     "button-press-event",
                     G_CALLBACK(notebook_clicked),
                     file_browser);

    gtk_widget_show_all(GTK_WIDGET(evt_box));
    if (!set->icon)
        gtk_widget_hide(tab_icon);
    return GTK_WIDGET(evt_box);
}

void
fm_main_window_update_tab_label(FMMainWindow* main_window, PtkFileBrowser* file_browser,
                                const char* path)
{
    GtkWidget* label;
    GtkContainer* hbox;
    // GtkImage* icon;
    GtkLabel* text;
    GList* children;
    char* name;

    label =
        gtk_notebook_get_tab_label(GTK_NOTEBOOK(main_window->notebook), GTK_WIDGET(file_browser));
    if (label)
    {
        hbox = GTK_CONTAINER(gtk_bin_get_child(GTK_BIN(label)));
        children = gtk_container_get_children(hbox);
        // icon = GTK_IMAGE(children->data);
        text = GTK_LABEL(children->next->data);

        // TODO: Change the icon

        name = g_path_get_basename(path);
        gtk_label_set_text(text, name);
        gtk_label_set_ellipsize(text, PANGO_ELLIPSIZE_MIDDLE);
        if (strlen(name) < 30)
        {
            gtk_label_set_ellipsize(text, PANGO_ELLIPSIZE_NONE);
            gtk_label_set_width_chars(text, -1);
        }
        else
            gtk_label_set_width_chars(text, 30);
        free(name);

        g_list_free(children); // sfm 0.6.0 enabled
    }
}

void
fm_main_window_add_new_tab(FMMainWindow* main_window, const char* folder_path)
{
    GtkWidget* notebook = main_window->notebook;

    PtkFileBrowser* curfb = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (GTK_IS_WIDGET(curfb))
    {
        // save sliders of current fb ( new tab while task manager is shown changes vals )
        ptk_file_browser_slider_release(nullptr, nullptr, curfb);
        // save column widths of fb so new tab has same
        ptk_file_browser_save_column_widths(GTK_TREE_VIEW(curfb->folder_view), curfb);
    }
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(
        ptk_file_browser_new(main_window->curpanel, notebook, main_window->task_view, main_window));
    if (!file_browser)
        return;
    // LOG_INFO("fm_main_window_add_new_tab fb={:p}", fmt::ptr(file_browser));
    ptk_file_browser_set_single_click(file_browser, app_settings.single_click);
    // FIXME: this shouldn't be hard-code
    ptk_file_browser_set_single_click_timeout(file_browser,
                                              app_settings.no_single_hover ? 0
                                                                           : SINGLE_CLICK_TIMEOUT);
    ptk_file_browser_show_thumbnails(file_browser,
                                     app_settings.show_thumbnail ? app_settings.max_thumb_size : 0);

    ptk_file_browser_set_sort_order(
        file_browser,
        (PtkFBSortOrder)xset_get_int_panel(file_browser->mypanel, "list_detailed", "x"));
    ptk_file_browser_set_sort_type(
        file_browser,
        (GtkSortType)xset_get_int_panel(file_browser->mypanel, "list_detailed", "y"));

    gtk_widget_show(GTK_WIDGET(file_browser));

    g_signal_connect(file_browser,
                     "begin-chdir",
                     G_CALLBACK(on_file_browser_begin_chdir),
                     main_window);
    g_signal_connect(file_browser,
                     "content-change",
                     G_CALLBACK(on_file_browser_content_change),
                     main_window);
    g_signal_connect(file_browser,
                     "after-chdir",
                     G_CALLBACK(on_file_browser_after_chdir),
                     main_window);
    g_signal_connect(file_browser, "open-item", G_CALLBACK(on_file_browser_open_item), main_window);
    g_signal_connect(file_browser,
                     "sel-change",
                     G_CALLBACK(on_file_browser_sel_change),
                     main_window);
    g_signal_connect(file_browser,
                     "pane-mode-change",
                     G_CALLBACK(on_file_browser_panel_change),
                     main_window);

    GtkWidget* tab_label = fm_main_window_create_tab_label(main_window, file_browser);
    int idx = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), GTK_WIDGET(file_browser), tab_label);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), GTK_WIDGET(file_browser), true);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), idx);

    if (app_settings.always_show_tabs)
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), true);
    else if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) > 1)
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), true);
    else
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), false);

    if (!ptk_file_browser_chdir(file_browser, folder_path, PTK_FB_CHDIR_ADD_HISTORY))
        ptk_file_browser_chdir(file_browser, "/", PTK_FB_CHDIR_ADD_HISTORY);

    if (event_handler.tab_new->s || event_handler.tab_new->ob2_data)
        main_window_event(main_window,
                          event_handler.tab_new,
                          "evt_tab_new",
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true);

    set_panel_focus(main_window, file_browser);
    //    while( gtk_events_pending() )  // wait for chdir to grab focus
    //        gtk_main_iteration();
    // gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
    // LOG_INFO("focus browser {} {}", idx, file_browser->folder_view);
    // LOG_INFO("call delayed (newtab) #{} {:p}", idx, fmt::ptr(file_browser->folder_view));
    //    g_idle_add( ( GSourceFunc ) delayed_focus, file_browser->folder_view );
}

GtkWidget*
fm_main_window_new()
{
    return GTK_WIDGET(g_object_new(FM_TYPE_MAIN_WINDOW, nullptr));
}

GtkWidget*
fm_main_window_get_current_file_browser(FMMainWindow* main_window)
{
    if (!main_window)
    {
        main_window = fm_main_window_get_last_active();
        if (!main_window)
            return nullptr;
    }
    if (main_window->notebook)
    {
        int idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->notebook));
        if (idx >= 0)
            return gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->notebook), idx);
    }
    return nullptr;
}

static void
on_preference_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(user_data);
    fm_main_window_preference(main_window);
}

static void
fm_main_window_preference(FMMainWindow* main_window)
{
    fm_edit_preference(GTK_WINDOW(main_window), PREF_GENERAL);
}

static void
on_about_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    static GtkWidget* about_dlg = nullptr;
    if (!about_dlg)
    {
        GtkBuilder* builder = gtk_builder_new();

        WindowReference::increase();

        builder = _gtk_builder_new_from_file("about-dlg3.ui");
        about_dlg = GTK_WIDGET(gtk_builder_get_object(builder, "dlg"));
        g_object_unref(builder);
        gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_dlg), PACKAGE_VERSION);

        const char* name;
        XSet* set = xset_get("main_icon");
        if (set->icon)
            name = set->icon;
        else if (geteuid() == 0)
            name = ztd::strdup("spacefm-root");
        else
            name = ztd::strdup("spacefm");
        gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(about_dlg), name);

        // g_object_add_weak_pointer(G_OBJECT(about_dlg), (void*)&about_dlg);
        g_signal_connect(about_dlg, "response", G_CALLBACK(gtk_widget_destroy), nullptr);
        g_signal_connect(about_dlg, "destroy", G_CALLBACK(WindowReference::decrease), nullptr);
    }
    gtk_window_set_transient_for(GTK_WINDOW(about_dlg), GTK_WINDOW(user_data));
    gtk_window_present(GTK_WINDOW(about_dlg));
}

static void
fm_main_window_add_new_window(FMMainWindow* main_window)
{
    if (main_window && !main_window->maximized && !main_window->fullscreen)
    {
        // use current main_window's size for new window
        GtkAllocation allocation;
        gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);
        if (allocation.width > 0)
        {
            app_settings.width = allocation.width;
            app_settings.height = allocation.height;
        }
    }
    GtkWidget* new_win = fm_main_window_new();
}

static void
on_new_window_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(user_data);

    autosave_cancel();
    fm_main_window_store_positions(main_window);
    save_settings(main_window);

    fm_main_window_add_new_window(main_window);
}

static bool
delayed_focus(GtkWidget* widget)
{
    if (GTK_IS_WIDGET(widget))
    {
        // LOG_INFO("delayed_focus {:p}", fmt::ptr(widget));
        if (GTK_IS_WIDGET(widget))
            gtk_widget_grab_focus(widget);
    }
    return false;
}

static bool
delayed_focus_file_browser(PtkFileBrowser* file_browser)
{
    if (GTK_IS_WIDGET(file_browser) && GTK_IS_WIDGET(file_browser->folder_view))
    {
        // LOG_INFO("delayed_focus_file_browser fb={:p}", fmt::ptr(file_browser));
        if (GTK_IS_WIDGET(file_browser) && GTK_IS_WIDGET(file_browser->folder_view))
        {
            gtk_widget_grab_focus(file_browser->folder_view);
            set_panel_focus(nullptr, file_browser);
        }
    }
    return false;
}

void
set_panel_focus(FMMainWindow* main_window, PtkFileBrowser* file_browser)
{
    int p;
    // int pages;
    int cur_tabx;
    // GtkWidget* notebook;

    if (!file_browser && !main_window)
        return;

    FMMainWindow* mw = main_window;
    if (!mw)
        mw = static_cast<FMMainWindow*>(file_browser->main_window);

    for (p = 1; p < 5; p++)
    {
        // notebook = mw->panel[p - 1];
        // pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
        gtk_widget_set_sensitive(mw->panel_image[p - 1], p == mw->curpanel);
    }

    update_window_title(nullptr, mw);
    if (event_handler.pnl_focus->s || event_handler.pnl_focus->ob2_data)
        main_window_event(main_window,
                          event_handler.pnl_focus,
                          "evt_pnl_focus",
                          mw->curpanel,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true);
}

static void
on_fullscreen_activate(GtkMenuItem* menuitem, FMMainWindow* main_window)
{
    (void)menuitem;
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (xset_get_b("main_full"))
    {
        if (file_browser && file_browser->view_mode == PTK_FB_LIST_VIEW)
            ptk_file_browser_save_column_widths(GTK_TREE_VIEW(file_browser->folder_view),
                                                file_browser);
        gtk_widget_hide(main_window->menu_bar);
        gtk_widget_hide(main_window->panelbar);
        gtk_window_fullscreen(GTK_WINDOW(main_window));
        main_window->fullscreen = true;
    }
    else
    {
        main_window->fullscreen = false;
        gtk_window_unfullscreen(GTK_WINDOW(main_window));
        gtk_widget_show(main_window->menu_bar);
        if (xset_get_b("main_pbar"))
            gtk_widget_show(main_window->panelbar);

        if (!main_window->maximized)
            show_panels(nullptr, main_window); // restore columns
    }
}

static void
set_window_title(FMMainWindow* main_window, PtkFileBrowser* file_browser)
{
    char* disp_path;
    char* disp_name;
    char* tab_count = nullptr;
    char* tab_num = nullptr;
    char* panel_count = nullptr;
    char* panel_num = nullptr;

    if (!file_browser || !main_window)
        return;

    if (file_browser->dir && file_browser->dir->disp_path)
    {
        disp_path = ztd::strdup(file_browser->dir->disp_path);
        disp_name = g_path_get_basename(disp_path);
    }
    else
    {
        const char* path = ptk_file_browser_get_cwd(file_browser);
        if (path)
        {
            disp_path = g_filename_display_name(path);
            disp_name = g_path_get_basename(disp_path);
        }
        else
        {
            disp_name = ztd::strdup("");
            disp_path = ztd::strdup("");
        }
    }

    char* orig_fmt = xset_get_s("main_title");
    std::string fmt;
    if (orig_fmt)
        fmt = orig_fmt;
    else
        fmt = "%d";

    std::vector<std::string> keys{"%t", "%T", "%p", "%P"};
    if (ztd::contains(fmt, keys))
    {
        // get panel/tab info
        int ipanel_count = 0, itab_count = 0, itab_num = 0;
        main_window_get_counts(file_browser, &ipanel_count, &itab_count, &itab_num);
        panel_count = g_strdup_printf("%d", ipanel_count);
        tab_count = g_strdup_printf("%d", itab_count);
        tab_num = g_strdup_printf("%d", itab_num);
        panel_count = g_strdup_printf("%d", ipanel_count);
        panel_num = g_strdup_printf("%d", main_window->curpanel);

        fmt = ztd::replace(fmt, "%t", tab_num);
        fmt = ztd::replace(fmt, "%T", tab_count);
        fmt = ztd::replace(fmt, "%p", panel_num);
        fmt = ztd::replace(fmt, "%P", panel_count);

        free(panel_count);
        free(tab_count);
        free(tab_num);
        free(panel_num);
    }
    if (ztd::contains(fmt, "*") && !main_tasks_running(main_window))
        fmt = ztd::replace(fmt, "*", "");
    if (ztd::contains(fmt, "%n"))
        fmt = ztd::replace(fmt, "%n", disp_name);
    if (orig_fmt && ztd::contains(orig_fmt, "%d"))
        fmt = ztd::replace(fmt, "%d", disp_path);

    free(disp_name);
    free(disp_path);

    gtk_window_set_title(GTK_WINDOW(main_window), fmt.c_str());
}

static void
update_window_title(GtkMenuItem* item, FMMainWindow* main_window)
{
    (void)item;
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (file_browser)
        set_window_title(main_window, file_browser);
}

static void
on_folder_notebook_switch_pape(GtkNotebook* notebook, GtkWidget* page, unsigned int page_num,
                               void* user_data)
{
    (void)page;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(user_data);
    PtkFileBrowser* file_browser;

    // save sliders of current fb ( new tab while task manager is shown changes vals )
    PtkFileBrowser* curfb = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (curfb)
    {
        ptk_file_browser_slider_release(nullptr, nullptr, curfb);
        if (curfb->view_mode == PTK_FB_LIST_VIEW)
            ptk_file_browser_save_column_widths(GTK_TREE_VIEW(curfb->folder_view), curfb);
    }

    file_browser = PTK_FILE_BROWSER(gtk_notebook_get_nth_page(notebook, page_num));
    // LOG_INFO("on_folder_notebook_switch_pape fb={:p}   panel={}   page={}", file_browser,
    // file_browser->mypanel, page_num);
    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    fm_main_window_update_status_bar(main_window, file_browser);

    set_window_title(main_window, file_browser);

    if (event_handler.tab_focus->ob2_data || event_handler.tab_focus->s)
        main_window_event(main_window,
                          event_handler.tab_focus,
                          "evt_tab_focus",
                          main_window->curpanel,
                          page_num + 1,
                          nullptr,
                          0,
                          0,
                          0,
                          true);

    ptk_file_browser_update_views(nullptr, file_browser);

    if (GTK_IS_WIDGET(file_browser))
        g_idle_add((GSourceFunc)delayed_focus, file_browser->folder_view);
}

void
main_window_open_path_in_current_tab(FMMainWindow* main_window, const char* path)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (!file_browser)
        return;
    ptk_file_browser_chdir(file_browser, path, PTK_FB_CHDIR_ADD_HISTORY);
}

void
main_window_open_network(FMMainWindow* main_window, const char* path, bool new_tab)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (!file_browser)
        return;
    char* str = ztd::strdup(path);
    ptk_location_view_mount_network(file_browser, str, new_tab, false);
    free(str);
}

static void
on_file_browser_open_item(PtkFileBrowser* file_browser, const char* path, PtkOpenAction action,
                          FMMainWindow* main_window)
{
    if (path)
    {
        switch (action)
        {
            case PTK_OPEN_DIR:
                ptk_file_browser_chdir(file_browser, path, PTK_FB_CHDIR_ADD_HISTORY);
                break;
            case PTK_OPEN_NEW_TAB:
                fm_main_window_add_new_tab(main_window, path);
                break;
            case PTK_OPEN_NEW_WINDOW:
                break;
            case PTK_OPEN_TERMINAL:
                break;
            case PTK_OPEN_FILE:
                break;
            default:
                break;
        }
    }
}

static void
fm_main_window_update_status_bar(FMMainWindow* main_window, PtkFileBrowser* file_browser)
{
    (void)main_window;
    if (!(GTK_IS_WIDGET(file_browser) && GTK_IS_STATUSBAR(file_browser->status_bar)))
        return;

    if (file_browser->status_bar_custom)
    {
        gtk_statusbar_push(GTK_STATUSBAR(file_browser->status_bar),
                           0,
                           file_browser->status_bar_custom);
        return;
    }

    const char* cwd = ptk_file_browser_get_cwd(file_browser);
    if (!cwd)
        return;

    std::string size_str;
    std::string statusbar_txt;

    if (std::filesystem::exists(cwd))
    {
        std::string total_size_str;

        // FIXME: statvfs support should be moved to src/vfs
        struct statvfs fs_stat;
        statvfs(cwd, &fs_stat);

        // calc free space
        size_str = vfs_file_size_to_string_format(fs_stat.f_bsize * fs_stat.f_bavail, true);
        // calc total space
        total_size_str = vfs_file_size_to_string_format(fs_stat.f_frsize * fs_stat.f_blocks, true);

        statusbar_txt.append(fmt::format(" {} / {}   ", size_str, total_size_str));
    }

    // Show Reading... while sill loading
    if (file_browser->busy)
    {
        statusbar_txt.append(fmt::format("Reading {} ...", ptk_file_browser_get_cwd(file_browser)));
        gtk_statusbar_push(GTK_STATUSBAR(file_browser->status_bar), 0, statusbar_txt.c_str());
        return;
    }

    unsigned int num_sel;
    unsigned int num_vis;
    uint64_t total_size;

    // note: total size won't include content changes since last selection change
    num_sel = ptk_file_browser_get_n_sel(file_browser, &total_size);
    num_vis = ptk_file_browser_get_n_visible_files(file_browser);

    if (num_sel > 0)
    {
        GList* files = ptk_file_browser_get_selected_files(file_browser);
        if (!files)
            return;

        VFSFileInfo* file;

        size_str = vfs_file_size_to_string_format(total_size, true);

        statusbar_txt.append(fmt::format("{} / {} ({})", num_sel, num_vis, size_str));

        if (num_sel == 1)
        // display file name or symlink info in status bar if one file selected
        {
            file = vfs_file_info_ref(static_cast<VFSFileInfo*>(files->data));
            if (!file)
                return;

            g_list_foreach(files, (GFunc)vfs_file_info_unref, nullptr);
            g_list_free(files);

            if (vfs_file_info_is_symlink(file))
            {
                std::string full_target;
                std::string target_path;
                std::string file_path;
                std::string target;

                file_path = g_build_filename(cwd, vfs_file_info_get_name(file), nullptr);
                target = std::filesystem::absolute(file_path);
                if (!target.empty())
                {
                    // LOG_INFO("LINK: {}", file_path);
                    if (target.at(0) != '/')
                    {
                        // relative link
                        full_target = g_build_filename(cwd, target.c_str(), nullptr);
                        target_path = full_target;
                    }
                    else
                        target_path = target;

                    if (vfs_file_info_is_dir(file))
                    {
                        if (std::filesystem::exists(target_path))
                            statusbar_txt.append(fmt::format("  Link -> {}/", target));
                        else
                            statusbar_txt.append(fmt::format("  !Link -> {}/ (missing)", target));
                    }
                    else
                    {
                        struct stat results;
                        if (stat(target_path.c_str(), &results) == 0)
                        {
                            std::string lsize =
                                vfs_file_size_to_string_format(results.st_size, true);
                            statusbar_txt.append(fmt::format("  Link -> {} ({})", target, lsize));
                        }
                        else
                            statusbar_txt.append(fmt::format("  !Link -> {} (missing)", target));
                    }
                }
                else
                    statusbar_txt.append(fmt::format("  !Link -> (error reading target)"));
            }
            else
                statusbar_txt.append(fmt::format("  {}", vfs_file_info_get_name(file)));

            vfs_file_info_unref(file);
        }
        else
        {
            unsigned int count_dir = 0;
            unsigned int count_file = 0;
            unsigned int count_symlink = 0;
            unsigned int count_socket = 0;
            unsigned int count_pipe = 0;
            unsigned int count_block = 0;
            unsigned int count_char = 0;

            GList* l;
            for (l = files; l; l = l->next)
            {
                file = vfs_file_info_ref(static_cast<VFSFileInfo*>(l->data));

                if (!file)
                    continue;

                if (vfs_file_info_is_dir(file))
                    ++count_dir;
                else if (vfs_file_info_is_regular_file(file))
                    ++count_file;
                else if (vfs_file_info_is_symlink(file))
                    ++count_symlink;
                else if (vfs_file_info_is_socket(file))
                    ++count_socket;
                else if (vfs_file_info_is_named_pipe(file))
                    ++count_pipe;
                else if (vfs_file_info_is_block_device(file))
                    ++count_block;
                else if (vfs_file_info_is_char_device(file))
                    ++count_char;

                vfs_file_info_unref(file);
            }

            if (count_dir)
                statusbar_txt.append(fmt::format("  Directories ({})", count_dir));
            if (count_file)
                statusbar_txt.append(fmt::format("  Files ({})", count_file));
            if (count_symlink)
                statusbar_txt.append(fmt::format("  Symlinks ({})", count_symlink));
            if (count_socket)
                statusbar_txt.append(fmt::format("  Sockets ({})", count_socket));
            if (count_pipe)
                statusbar_txt.append(fmt::format("  Named Pipes ({})", count_pipe));
            if (count_block)
                statusbar_txt.append(fmt::format("  Block Devices ({})", count_block));
            if (count_char)
                statusbar_txt.append(fmt::format("  Character Devices ({})", count_char));
        }
    }
    else
    {
        // count for .hidden files
        unsigned int num_hid;
        unsigned int num_hidx;

        num_hid = ptk_file_browser_get_n_all_files(file_browser) - num_vis;
        num_hidx = file_browser->dir ? file_browser->dir->xhidden_count : 0;
        if (num_hid || num_hidx)
            statusbar_txt.append(fmt::format("{} visible ({} hidden)", num_vis, num_hid));
        else
            statusbar_txt.append(fmt::format("{} item", num_vis));

        // cur dir is a symlink? canonicalize path
        if (std::filesystem::is_symlink(cwd))
        {
            std::string canon = std::filesystem::read_symlink(cwd);
            statusbar_txt.append(fmt::format("  {} -> {}", cwd, canon));
        }
        else
            statusbar_txt.append(fmt::format("  {}", cwd));
    }

    gtk_statusbar_push(GTK_STATUSBAR(file_browser->status_bar), 0, statusbar_txt.c_str());
}

static void
on_file_browser_panel_change(PtkFileBrowser* file_browser, FMMainWindow* main_window)
{
    // LOG_INFO("panel_change  panel {}", file_browser->mypanel);
    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];
    // set_window_title( main_window, file_browser );
    set_panel_focus(main_window, file_browser);
}

static void
on_file_browser_sel_change(PtkFileBrowser* file_browser, FMMainWindow* main_window)
{
    // LOG_INFO("sel_change  panel {}", file_browser->mypanel);
    if ((event_handler.pnl_sel->ob2_data || event_handler.pnl_sel->s) &&
        main_window_event(main_window,
                          event_handler.pnl_sel,
                          "evt_pnl_sel",
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true))
        return;
    fm_main_window_update_status_bar(main_window, file_browser);
}

static void
on_file_browser_content_change(PtkFileBrowser* file_browser, FMMainWindow* main_window)
{
    // LOG_INFO("content_change  panel {}", file_browser->mypanel);
    fm_main_window_update_status_bar(main_window, file_browser);
}

static bool
on_tab_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, int x, int y, unsigned int time,
                   PtkFileBrowser* file_browser)
{
    (void)widget;
    (void)drag_context;
    (void)x;
    (void)y;
    (void)time;
    GtkNotebook* notebook = GTK_NOTEBOOK(gtk_widget_get_parent(GTK_WIDGET(file_browser)));
    // TODO: Add a timeout here and don't set current page immediately
    int idx = gtk_notebook_page_num(notebook, GTK_WIDGET(file_browser));
    gtk_notebook_set_current_page(notebook, idx);
    return false;
}

static bool
on_window_button_press_event(GtkWidget* widget, GdkEventButton* event,
                             FMMainWindow* main_window) // sfm
{
    (void)widget;
    if (event->type != GDK_BUTTON_PRESS)
        return false;

    // handle mouse back/forward buttons anywhere in the main window
    if (event->button == 4 || event->button == 5 || event->button == 8 || event->button == 9) // sfm
    {
        PtkFileBrowser* file_browser =
            PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
        if (!file_browser)
            return false;
        if (event->button == 4 || event->button == 8)
            ptk_file_browser_go_back(nullptr, file_browser);
        else
            ptk_file_browser_go_forward(nullptr, file_browser);
        return true;
    }
    return false;
}

static bool
on_main_window_focus(GtkWidget* main_window, GdkEventFocus* event, void* user_data)
{
    (void)event;
    (void)user_data;
    // this causes a widget not realized loop by running rebuild_menus while
    // rebuild_menus is already running
    // but this unneeded anyway?  cross-window menu changes seem to work ok
    // rebuild_menus( main_window );  // xset may change in another window
    if (event_handler.win_focus->s || event_handler.win_focus->ob2_data)
        main_window_event(FM_MAIN_WINDOW(main_window),
                          event_handler.win_focus,
                          "evt_win_focus",
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true);
    return false;
}

static bool
on_main_window_keypress(FMMainWindow* main_window, GdkEventKey* event, XSet* known_set)
{
    // LOG_INFO("main_keypress {} {}", event->keyval, event->state);

    PtkFileBrowser* browser;

    if (known_set)
    {
        XSet* set = known_set;
        return on_main_window_keypress_found_key(main_window, set);
    }

    if (event->keyval == 0)
        return false;

    unsigned int keymod;
    keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK |
                              GDK_HYPER_MASK | GDK_META_MASK));

    if ((event->keyval == GDK_KEY_Home && (keymod == 0 || keymod == GDK_SHIFT_MASK)) ||
        (event->keyval == GDK_KEY_End && (keymod == 0 || keymod == GDK_SHIFT_MASK)) ||
        (event->keyval == GDK_KEY_Delete && keymod == 0) ||
        (event->keyval == GDK_KEY_Tab && keymod == 0) ||
        (keymod == 0 && (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)) ||
        (event->keyval == GDK_KEY_Left && (keymod == 0 || keymod == GDK_SHIFT_MASK)) ||
        (event->keyval == GDK_KEY_Right && (keymod == 0 || keymod == GDK_SHIFT_MASK)) ||
        (event->keyval == GDK_KEY_BackSpace && keymod == 0) ||
        (keymod == 0 && event->keyval != GDK_KEY_Escape &&
         gdk_keyval_to_unicode(event->keyval))) // visible char
    {
        browser = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
        if (browser && browser->path_bar && gtk_widget_has_focus(GTK_WIDGET(browser->path_bar)))
            return false; // send to pathbar
    }

#ifdef HAVE_NONLATIN
    unsigned int nonlatin_key;
    nonlatin_key = 0;
    // need to transpose nonlatin keyboard layout ?
    if (!((GDK_KEY_0 <= event->keyval && event->keyval <= GDK_KEY_9) ||
          (GDK_KEY_A <= event->keyval && event->keyval <= GDK_KEY_Z) ||
          (GDK_KEY_a <= event->keyval && event->keyval <= GDK_KEY_z)))
    {
        nonlatin_key = event->keyval;
        transpose_nonlatin_keypress(event);
    }
#endif

    if ((event_handler.win_key->s || event_handler.win_key->ob2_data) &&
        main_window_event(main_window,
                          event_handler.win_key,
                          "evt_win_key",
                          0,
                          0,
                          nullptr,
                          event->keyval,
                          0,
                          keymod,
                          true))
        return true;

    for (XSet* set: xsets)
    {
        if (set->shared_key)
        {
            // set has shared key
#ifdef HAVE_NONLATIN
            // nonlatin key match is for nonlatin keycodes set prior to 1.0.3
            set = xset_get(set->shared_key);
            if ((set->key == event->keyval || (nonlatin_key && set->key == nonlatin_key)) &&
                set->keymod == keymod)
#else
            set = xset_get(set->shared_key);
            if (set->key == event->keyval && set->keymod == keymod)
#endif
            {
                // shared key match
                if (Glib::str_has_prefix(set->name, "panel"))
                {
                    // use current panel's set
                    browser =
                        PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
                    if (browser)
                    {
                        char* new_set_name =
                            g_strdup_printf("panel%d%s", browser->mypanel, set->name + 6);
                        set = xset_get(new_set_name);
                        free(new_set_name);
                    }
                    else
                        return false; // failsafe
                }
                return on_main_window_keypress_found_key(main_window, set);
            }
            else
                continue;
        }
#ifdef HAVE_NONLATIN
        // nonlatin key match is for nonlatin keycodes set prior to 1.0.3
        if (((set->key == event->keyval ||
            (nonlatin_key && set->key == nonlatin_key)) &&
            set->keymod == keymod)
#else
        if (set->key == event->keyval && set->keymod == keymod)
#endif
        {
            return on_main_window_keypress_found_key(main_window, set);
        }
    }

#ifdef HAVE_NONLATIN
    if (nonlatin_key != 0)
    {
        // use literal keycode for pass-thru, eg for find-as-you-type search
        event->keyval = nonlatin_key;
    }
#endif

    if ((event->state & GDK_MOD1_MASK))
        rebuild_menus(main_window);

    return false;
}

static bool
on_main_window_keypress_found_key(FMMainWindow* main_window, XSet* set)
{
    PtkFileBrowser* browser;

    browser = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
    if (!browser)
        return true;

    char* xname;
    int i;

    // special edit items
    if (!strcmp(set->name, "edit_cut") || !strcmp(set->name, "edit_copy") ||
        !strcmp(set->name, "edit_delete") || !strcmp(set->name, "select_all"))
    {
        if (!gtk_widget_is_focus(browser->folder_view))
            return false;
    }
    else if (!strcmp(set->name, "edit_paste"))
    {
        bool side_dir_focus =
            (browser->side_dir && gtk_widget_is_focus(GTK_WIDGET(browser->side_dir)));
        if (!gtk_widget_is_focus(GTK_WIDGET(browser->folder_view)) && !side_dir_focus)
            return false;
    }

    // run menu_cb
    if (set->menu_style < XSET_MENU_SUBMENU)
    {
        set->browser = browser;
        xset_menu_cb(nullptr, set); // also does custom activate
    }
    if (!set->lock)
        return true;

    // handlers
    if (Glib::str_has_prefix(set->name, "dev_"))
        ptk_location_view_on_action(GTK_WIDGET(browser->side_dev), set);

    else if (Glib::str_has_prefix(set->name, "main_"))
    {
        xname = set->name + 5;
        if (!strcmp(xname, "new_window"))
            on_new_window_activate(nullptr, main_window);
        else if (!strcmp(xname, "root_window"))
            on_open_current_folder_as_root(nullptr, main_window);
        else if (!strcmp(xname, "search"))
            on_find_file_activate(nullptr, main_window);
        else if (!strcmp(xname, "terminal"))
            on_open_terminal_activate(nullptr, main_window);
        else if (!strcmp(xname, "root_terminal"))
            on_open_root_terminal_activate(nullptr, main_window);
        else if (!strcmp(xname, "save_session"))
            on_open_url(nullptr, main_window);
        else if (!strcmp(xname, "exit"))
            on_quit_activate(nullptr, main_window);
        else if (!strcmp(xname, "full"))
        {
            xset_set_b("main_full", !main_window->fullscreen);
            on_fullscreen_activate(nullptr, main_window);
        }
        else if (!strcmp(xname, "prefs"))
            on_preference_activate(nullptr, main_window);
        else if (!strcmp(xname, "design_mode"))
            main_design_mode(nullptr, main_window);
        else if (!strcmp(xname, "pbar"))
            show_panels_all_windows(nullptr, main_window);
        else if (!strcmp(xname, "icon"))
            on_main_icon();
        else if (!strcmp(xname, "title"))
            update_window_title(nullptr, main_window);
        else if (!strcmp(xname, "about"))
            on_about_activate(nullptr, main_window);
    }
    else if (Glib::str_has_prefix(set->name, "panel_"))
    {
        xname = set->name + 6;
        if (!strcmp(xname, "prev"))
            i = -1;
        else if (!strcmp(xname, "next"))
            i = -2;
        else if (!strcmp(xname, "hide"))
            i = -3;
        else
            i = strtol(xname, nullptr, 10);
        focus_panel(nullptr, main_window, i);
    }
    else if (Glib::str_has_prefix(set->name, "plug_"))
        on_plugin_install(nullptr, main_window, set);
    else if (Glib::str_has_prefix(set->name, "task_"))
    {
        xname = set->name + 5;
        if (strstr(xname, "_manager"))
            on_task_popup_show(nullptr, main_window, set->name);
        else if (!strcmp(xname, "col_reorder"))
            on_reorder(nullptr, GTK_WIDGET(browser->task_view));
        else if (Glib::str_has_prefix(xname, "col_"))
            on_task_column_selected(nullptr, browser->task_view);
        else if (Glib::str_has_prefix(xname, "stop") || Glib::str_has_prefix(xname, "pause") ||
                 Glib::str_has_prefix(xname, "que_") || !strcmp(xname, "que") ||
                 Glib::str_has_prefix(xname, "resume"))
        {
            PtkFileTask* ptask = get_selected_task(browser->task_view);
            on_task_stop(nullptr, browser->task_view, set, ptask);
        }
        else if (!strcmp(xname, "showout"))
            show_task_dialog(nullptr, browser->task_view);
        else if (Glib::str_has_prefix(xname, "err_"))
            on_task_popup_errset(nullptr, main_window, set->name);
    }
    else if (!strcmp(set->name, "rubberband"))
        main_window_rubberband_all();
    else
        ptk_file_browser_on_action(browser, set->name);

    return true;
}

FMMainWindow*
fm_main_window_get_last_active()
{
    if (!all_windows.empty())
        return all_windows.at(0);
    return nullptr;
}

const std::vector<FMMainWindow*>
fm_main_window_get_all()
{
    return all_windows;
}

static long
get_desktop_index(GtkWindow* win)
{
    long desktop = -1;
    GdkDisplay* display;
    GdkWindow* window = nullptr;

    if (win)
    {
        // get desktop of win
        display = gtk_widget_get_display(GTK_WIDGET(win));
        window = gtk_widget_get_window(GTK_WIDGET(win));
    }
    else
    {
        // get current desktop
        display = gdk_display_get_default();
        if (display)
            window = gdk_x11_window_lookup_for_display(display, gdk_x11_get_default_root_xwindow());
    }

    if (!(GDK_IS_DISPLAY(display) && GDK_IS_WINDOW(window)))
        return desktop;

    // find out what desktop (workspace) window is on   #include <gdk/gdkx.h>
    Atom type;
    int format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* data;
    const char* atom_name = win ? "_NET_WM_DESKTOP" : "_NET_CURRENT_DESKTOP";
    Atom net_wm_desktop = gdk_x11_get_xatom_by_name_for_display(display, atom_name);

    if (net_wm_desktop == None)
        LOG_ERROR("atom not found: {}", atom_name);
    else if (XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display),
                                GDK_WINDOW_XID(window),
                                net_wm_desktop,
                                0,
                                1,
                                False,
                                XA_CARDINAL,
                                (Atom*)&type,
                                &format,
                                &nitems,
                                &bytes_after,
                                &data) != Success ||
             type == None || data == nullptr)
    {
        if (type == None)
            LOG_ERROR("No such property from XGetWindowProperty() {}", atom_name);
        else if (data == nullptr)
            LOG_ERROR("No data returned from XGetWindowProperty() {}", atom_name);
        else
            LOG_ERROR("XGetWindowProperty() {} failed\n", atom_name);
    }
    else
    {
        desktop = *data;
        XFree(data);
    }
    return desktop;
}

FMMainWindow*
fm_main_window_get_on_current_desktop()
{ // find the last used spacefm window on the current desktop
    long desktop;
    long cur_desktop = get_desktop_index(nullptr);
    // LOG_INFO("current_desktop = {}", cur_desktop);
    if (cur_desktop == -1)
        return fm_main_window_get_last_active(); // revert to dumb if no current

    bool invalid = false;
    for (FMMainWindow* window: all_windows)
    {
        desktop = get_desktop_index(GTK_WINDOW(window));
        // LOG_INFO( "    test win {:p} = {}", window, desktop);
        if (desktop == cur_desktop || desktop > 254 /* 255 == all desktops */)
            return window;
        else if (desktop == -1 && !invalid)
            invalid = true;
    }
    // revert to dumb if one or more window desktops unreadable
    return invalid ? fm_main_window_get_last_active() : nullptr;
}

enum MainWindowTaskCol
{
    TASK_COL_STATUS,
    TASK_COL_COUNT,
    TASK_COL_PATH,
    TASK_COL_FILE,
    TASK_COL_TO,
    TASK_COL_PROGRESS,
    TASK_COL_TOTAL,
    TASK_COL_STARTED,
    TASK_COL_ELAPSED,
    TASK_COL_CURSPEED,
    TASK_COL_CUREST,
    TASK_COL_AVGSPEED,
    TASK_COL_AVGEST,
    TASK_COL_STARTTIME,
    TASK_COL_ICON,
    TASK_COL_DATA
};

// clang-format off
static const char* task_titles[] =
{
    // If you change "Status", also change it in on_task_button_press_event
    "Status",
    "#",
    "Directory",
    "Item",
    "To",
    "Progress",
    "Total",
    "Started",
    "Elapsed",
    "Current",
    "CRemain",
    "Average",
    "Remain",
    "StartTime",
};

static const char* task_names[] =
{
    "task_col_status",
    "task_col_count",
    "task_col_path",
    "task_col_file",
    "task_col_to",
    "task_col_progress",
    "task_col_total",
    "task_col_started",
    "task_col_elapsed",
    "task_col_curspeed",
    "task_col_curest",
    "task_col_avgspeed",
    "task_col_avgest",
};
// clang-format on

void
on_reorder(GtkWidget* item, GtkWidget* parent)
{
    (void)item;
    xset_msg_dialog(
        parent,
        GTK_MESSAGE_INFO,
        "Reorder Columns Help",
        GTK_BUTTONS_OK,
        "To change the order of the columns, drag the column header to the desired location.");
}

void
main_context_fill(PtkFileBrowser* file_browser, XSetContext* c)
{
    PtkFileBrowser* a_browser;
    VFSFileInfo* file;
    VFSMimeType* mime_type;
    GtkClipboard* clip = nullptr;
    GList* sel_files;
    VFSVolume* vol;
    PtkFileTask* ptask;
    char* path;
    GtkTreeModel* model;
    GtkTreeModel* model_task;
    GtkTreeIter it;

    c->valid = false;
    if (!GTK_IS_WIDGET(file_browser))
        return;

    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    if (!main_window)
        return;

    if (!c->var[CONTEXT_NAME])
    {
        // if name is set, assume we don't need all selected files info
        c->var[CONTEXT_DIR] = ztd::strdup(ptk_file_browser_get_cwd(file_browser));
        if (!c->var[CONTEXT_DIR])
            c->var[CONTEXT_DIR] = ztd::strdup("");
        else
        {
            c->var[CONTEXT_WRITE_ACCESS] = ptk_file_browser_no_access(c->var[CONTEXT_DIR], nullptr)
                                               ? ztd::strdup("false")
                                               : ztd::strdup("true");
        }

        if ((sel_files = ptk_file_browser_get_selected_files(file_browser)))
            file = vfs_file_info_ref(static_cast<VFSFileInfo*>(sel_files->data));
        else
            file = nullptr;
        if (!file)
        {
            c->var[CONTEXT_NAME] = ztd::strdup("");
            c->var[CONTEXT_IS_DIR] = ztd::strdup("false");
            c->var[CONTEXT_IS_TEXT] = ztd::strdup("false");
            c->var[CONTEXT_IS_LINK] = ztd::strdup("false");
            c->var[CONTEXT_MIME] = ztd::strdup("");
            c->var[CONTEXT_MUL_SEL] = ztd::strdup("false");
        }
        else
        {
            c->var[CONTEXT_NAME] = ztd::strdup(vfs_file_info_get_name(file));
            path = g_build_filename(c->var[CONTEXT_DIR], c->var[CONTEXT_NAME], nullptr);
            c->var[CONTEXT_IS_DIR] = path && std::filesystem::is_directory(path)
                                         ? ztd::strdup("true")
                                         : ztd::strdup("false");
            c->var[CONTEXT_IS_TEXT] =
                vfs_file_info_is_text(file, path) ? ztd::strdup("true") : ztd::strdup("false");
            c->var[CONTEXT_IS_LINK] =
                vfs_file_info_is_symlink(file) ? ztd::strdup("true") : ztd::strdup("false");

            mime_type = vfs_file_info_get_mime_type(file);
            if (mime_type)
            {
                c->var[CONTEXT_MIME] = ztd::strdup(vfs_mime_type_get_type(mime_type));
                vfs_mime_type_unref(mime_type);
            }
            else
                c->var[CONTEXT_MIME] = ztd::strdup("");

            c->var[CONTEXT_MUL_SEL] = sel_files->next ? ztd::strdup("true") : ztd::strdup("false");

            vfs_file_info_unref(file);
            free(path);
        }
        if (sel_files)
        {
            g_list_foreach(sel_files, (GFunc)vfs_file_info_unref, nullptr);
            g_list_free(sel_files);
        }
    }

    if (!c->var[CONTEXT_IS_ROOT])
        c->var[CONTEXT_IS_ROOT] = geteuid() == 0 ? ztd::strdup("true") : ztd::strdup("false");

    if (!c->var[CONTEXT_CLIP_FILES])
    {
        clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        if (!gtk_clipboard_wait_is_target_available(
                clip,
                gdk_atom_intern("x-special/gnome-copied-files", false)) &&
            !gtk_clipboard_wait_is_target_available(clip, gdk_atom_intern("text/uri-list", false)))
            c->var[CONTEXT_CLIP_FILES] = ztd::strdup("false");
        else
            c->var[CONTEXT_CLIP_FILES] = ztd::strdup("true");
    }

    if (!c->var[CONTEXT_CLIP_TEXT])
    {
        if (!clip)
            clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        c->var[CONTEXT_CLIP_TEXT] =
            gtk_clipboard_wait_is_text_available(clip) ? ztd::strdup("true") : ztd::strdup("false");
    }

    // hack - Due to ptk_file_browser_update_views main iteration, fb tab may be destroyed
    // asynchronously - common if gui thread is blocked on stat
    // NOTE:  this is no longer needed
    if (!GTK_IS_WIDGET(file_browser))
        return;

    if (file_browser->side_book)
    {
        if (!GTK_IS_WIDGET(file_browser->side_book))
            return;
        c->var[CONTEXT_BOOKMARK] =
            ztd::strdup(ptk_bookmark_view_get_selected_dir(GTK_TREE_VIEW(file_browser->side_book)));
    }
    if (!c->var[CONTEXT_BOOKMARK])
        c->var[CONTEXT_BOOKMARK] = ztd::strdup("");

    // device
    if (file_browser->side_dev &&
        (vol = ptk_location_view_get_selected_vol(GTK_TREE_VIEW(file_browser->side_dev))))
    {
        c->var[CONTEXT_DEVICE] = ztd::strdup(vol->device_file);
        if (!c->var[CONTEXT_DEVICE])
            c->var[CONTEXT_DEVICE] = ztd::strdup("");
        c->var[CONTEXT_DEVICE_LABEL] = ztd::strdup(vol->label);
        if (!c->var[CONTEXT_DEVICE_LABEL])
            c->var[CONTEXT_DEVICE_LABEL] = ztd::strdup("");
        c->var[CONTEXT_DEVICE_MOUNT_POINT] = ztd::strdup(vol->mount_point);
        if (!c->var[CONTEXT_DEVICE_MOUNT_POINT])
            c->var[CONTEXT_DEVICE_MOUNT_POINT] = ztd::strdup("");
        c->var[CONTEXT_DEVICE_UDI] = ztd::strdup(vol->udi);
        if (!c->var[CONTEXT_DEVICE_UDI])
            c->var[CONTEXT_DEVICE_UDI] = ztd::strdup("");
        c->var[CONTEXT_DEVICE_FSTYPE] = ztd::strdup(vol->fs_type);
        if (!c->var[CONTEXT_DEVICE_FSTYPE])
            c->var[CONTEXT_DEVICE_FSTYPE] = ztd::strdup("");

        char* flags = ztd::strdup("");
        char* old_flags;
        if (vol->is_removable)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s removable", flags);
            free(old_flags);
        }
        else
        {
            old_flags = flags;
            flags = g_strdup_printf("%s internal", flags);
            free(old_flags);
        }

        if (vol->requires_eject)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s ejectable", flags);
            free(old_flags);
        }

        if (vol->is_optical)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s optical", flags);
            free(old_flags);
        }
        if (vol->is_table)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s table", flags);
            free(old_flags);
        }
        if (vol->is_floppy)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s floppy", flags);
            free(old_flags);
        }

        if (!vol->is_user_visible)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s policy_hide", flags);
            free(old_flags);
        }
        if (vol->nopolicy)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s policy_noauto", flags);
            free(old_flags);
        }

        if (vol->is_mounted)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s mounted", flags);
            free(old_flags);
        }
        else if (vol->is_mountable && !vol->is_table)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s mountable", flags);
            free(old_flags);
        }
        else
        {
            old_flags = flags;
            flags = g_strdup_printf("%s no_media", flags);
            free(old_flags);
        }

        if (vol->is_blank)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s blank", flags);
            free(old_flags);
        }
        if (vol->is_audiocd)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s audiocd", flags);
            free(old_flags);
        }
        if (vol->is_dvd)
        {
            old_flags = flags;
            flags = g_strdup_printf("%s dvd", flags);
            free(old_flags);
        }

        c->var[CONTEXT_DEVICE_PROP] = flags;
    }
    else
    {
        c->var[CONTEXT_DEVICE] = ztd::strdup("");
        c->var[CONTEXT_DEVICE_LABEL] = ztd::strdup("");
        c->var[CONTEXT_DEVICE_MOUNT_POINT] = ztd::strdup("");
        c->var[CONTEXT_DEVICE_UDI] = ztd::strdup("");
        c->var[CONTEXT_DEVICE_FSTYPE] = ztd::strdup("");
        c->var[CONTEXT_DEVICE_PROP] = ztd::strdup("");
    }

    // panels
    int i, p;
    int panel_count = 0;
    for (p = 1; p < 5; p++)
    {
        if (!xset_get_b_panel(p, "show"))
            continue;
        i = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
        if (i != -1)
        {
            a_browser = PTK_FILE_BROWSER(
                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), i));
        }
        else
            continue;
        if (!a_browser || !gtk_widget_get_visible(GTK_WIDGET(a_browser)))
            continue;

        panel_count++;
        c->var[CONTEXT_PANEL1_DIR + p - 1] = ztd::strdup(ptk_file_browser_get_cwd(a_browser));

        if (a_browser->side_dev &&
            (vol = ptk_location_view_get_selected_vol(GTK_TREE_VIEW(a_browser->side_dev))))
            c->var[CONTEXT_PANEL1_DEVICE + p - 1] = ztd::strdup(vol->device_file);

        // panel has files selected?
        if (a_browser->view_mode == PTK_FB_ICON_VIEW || a_browser->view_mode == PTK_FB_COMPACT_VIEW)
        {
            sel_files = folder_view_get_selected_items(a_browser, &model);
            if (sel_files)
                c->var[CONTEXT_PANEL1_SEL + p - 1] = ztd::strdup("true");
            else
                c->var[CONTEXT_PANEL1_SEL + p - 1] = ztd::strdup("false");
            g_list_foreach(sel_files, (GFunc)gtk_tree_path_free, nullptr);
            g_list_free(sel_files);
        }
        else if (file_browser->view_mode == PTK_FB_LIST_VIEW)
        {
            GtkTreeSelection* tree_sel =
                gtk_tree_view_get_selection(GTK_TREE_VIEW(a_browser->folder_view));
            if (gtk_tree_selection_count_selected_rows(tree_sel) > 0)
                c->var[CONTEXT_PANEL1_SEL + p - 1] = ztd::strdup("true");
            else
                c->var[CONTEXT_PANEL1_SEL + p - 1] = ztd::strdup("false");
        }
        else
            c->var[CONTEXT_PANEL1_SEL + p - 1] = ztd::strdup("false");

        if (file_browser == a_browser)
        {
            c->var[CONTEXT_TAB] = g_strdup_printf("%d", i + 1);
            c->var[CONTEXT_TAB_COUNT] =
                g_strdup_printf("%d",
                                gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[p - 1])));
        }
    }
    c->var[CONTEXT_PANEL_COUNT] = g_strdup_printf("%d", panel_count);
    c->var[CONTEXT_PANEL] = g_strdup_printf("%d", file_browser->mypanel);
    if (!c->var[CONTEXT_TAB])
        c->var[CONTEXT_TAB] = ztd::strdup("");
    if (!c->var[CONTEXT_TAB_COUNT])
        c->var[CONTEXT_TAB_COUNT] = ztd::strdup("");
    for (p = 1; p < 5; p++)
    {
        if (!c->var[CONTEXT_PANEL1_DIR + p - 1])
            c->var[CONTEXT_PANEL1_DIR + p - 1] = ztd::strdup("");
        if (!c->var[CONTEXT_PANEL1_SEL + p - 1])
            c->var[CONTEXT_PANEL1_SEL + p - 1] = ztd::strdup("false");
        if (!c->var[CONTEXT_PANEL1_DEVICE + p - 1])
            c->var[CONTEXT_PANEL1_DEVICE + p - 1] = ztd::strdup("");
    }

    // tasks
    const char* job_titles[] = {"move", "copy", "trash", "delete", "link", "change", "run"};
    if ((ptask = get_selected_task(file_browser->task_view)))
    {
        c->var[CONTEXT_TASK_TYPE] = ztd::strdup(job_titles[ptask->task->type]);
        if (ptask->task->type == VFS_FILE_TASK_EXEC)
        {
            c->var[CONTEXT_TASK_NAME] = ztd::strdup(ptask->task->current_file);
            c->var[CONTEXT_TASK_DIR] = ztd::strdup(ptask->task->dest_dir);
        }
        else
        {
            c->var[CONTEXT_TASK_NAME] = ztd::strdup("");
            ptk_file_task_lock(ptask);
            if (!ptask->task->current_file.empty())
                c->var[CONTEXT_TASK_DIR] = g_path_get_dirname(ptask->task->current_file.c_str());
            else
                c->var[CONTEXT_TASK_DIR] = ztd::strdup("");
            ptk_file_task_unlock(ptask);
        }
    }
    else
    {
        c->var[CONTEXT_TASK_TYPE] = ztd::strdup("");
        c->var[CONTEXT_TASK_NAME] = ztd::strdup("");
        c->var[CONTEXT_TASK_DIR] = ztd::strdup("");
    }
    if (!main_window->task_view || !GTK_IS_TREE_VIEW(main_window->task_view))
        c->var[CONTEXT_TASK_COUNT] = ztd::strdup("0");
    else
    {
        model_task = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        int task_count = 0;
        if (gtk_tree_model_get_iter_first(model_task, &it))
        {
            task_count++;
            while (gtk_tree_model_iter_next(model_task, &it))
                task_count++;
        }
        c->var[CONTEXT_TASK_COUNT] = g_strdup_printf("%d", task_count);
    }

    c->valid = true;
}

static FMMainWindow*
get_task_view_window(GtkWidget* view)
{
    for (FMMainWindow* window: all_windows)
    {
        if (window->task_view == view)
            return window;
    }
    return nullptr;
}

void
main_write_exports(VFSFileTask* vtask, const char* value, std::string& buf)
{
    char* path;
    std::string esc_path;
    PtkFileTask* ptask;

    PtkFileBrowser* file_browser = static_cast<PtkFileBrowser*>(vtask->exec_browser);
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    XSet* set = XSET(vtask->exec_set);

    buf.append("\n#source");
    // buf.append("\n\ncp $0 /tmp\n\n");

    // panels
    int p;
    for (p = 1; p < 5; p++)
    {
        PtkFileBrowser* a_browser;

        if (!xset_get_b_panel(p, "show"))
            continue;
        int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
        if (i != -1)
        {
            a_browser = PTK_FILE_BROWSER(
                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), i));
        }
        else
            continue;
        if (!a_browser || !gtk_widget_get_visible(GTK_WIDGET(a_browser)))
            continue;

        // cwd
        bool cwd_needs_quote;
        const char* cwd = ptk_file_browser_get_cwd(a_browser);
        if ((cwd_needs_quote = !!strchr(cwd, '\'')))
        {
            std::string path2 = bash_quote(cwd);
            buf.append(fmt::format("\nfm_pwd_panel[{}]={}\n", p, path2));
        }
        else
            buf.append(fmt::format("\nfm_pwd_panel[{}]=\"{}\"\n", p, cwd));
        buf.append(fmt::format("\nfm_tab_panel[{}]=\"{}\"\n", p, i + 1));

        // selected files
        GList* sel_files = ptk_file_browser_get_selected_files(a_browser);
        if (sel_files)
        {
            buf.append(fmt::format("fm_panel{}_files=(\n", p));
            GList* l;
            for (l = sel_files; l; l = l->next)
            {
                path = (char*)vfs_file_info_get_name(static_cast<VFSFileInfo*>(l->data));
                if (!cwd_needs_quote && !strchr(path, '"'))
                    buf.append(fmt::format("\"{}{}{}\"\n",
                                           cwd,
                                           (cwd[0] != '\0' && cwd[1] == '\0') ? "" : "/",
                                           path));
                else
                {
                    path = g_build_filename(cwd, path, nullptr);
                    esc_path = bash_quote(path);
                    buf.append(fmt::format("{}\n", esc_path));
                    free(path);
                }
            }
            buf.append(fmt::format(")\n"));

            if (file_browser == a_browser)
            {
                buf.append(fmt::format("fm_filenames=(\n"));
                for (l = sel_files; l; l = l->next)
                {
                    path = (char*)vfs_file_info_get_name(static_cast<VFSFileInfo*>(l->data));
                    if (!strchr(path, '"'))
                        buf.append(fmt::format("\"{}\"\n", path));
                    else
                    {
                        esc_path = bash_quote(path);
                        buf.append(fmt::format("{}\n", esc_path));
                    }
                }
                buf.append(fmt::format(")\n"));
            }

            g_list_foreach(sel_files, (GFunc)vfs_file_info_unref, nullptr);
            g_list_free(sel_files);
        }

        // bookmark
        if (a_browser->side_book)
        {
            path = ptk_bookmark_view_get_selected_dir(GTK_TREE_VIEW(a_browser->side_book));
            if (path)
            {
                esc_path = bash_quote(path);
                if (file_browser == a_browser)
                    buf.append(fmt::format("fm_bookmark={}\n", esc_path));
                buf.append(fmt::format("fm_panel{}_bookmark={}\n", p, esc_path));
            }
        }

        // device
        if (a_browser->side_dev)
        {
            VFSVolume* vol = ptk_location_view_get_selected_vol(GTK_TREE_VIEW(a_browser->side_dev));
            if (vol)
            {
                if (file_browser == a_browser)
                {
                    buf.append(fmt::format("fm_device=\"{}\"\n", vol->device_file));
                    if (vol->udi)
                    {
                        esc_path = bash_quote(vol->udi);
                        buf.append(fmt::format("fm_device_udi={}\n", esc_path));
                    }
                    if (vol->mount_point)
                    {
                        esc_path = bash_quote(vol->mount_point);
                        buf.append(fmt::format("fm_device_mount_point={}\n", esc_path));
                    }
                    if (!vol->label.empty())
                    {
                        esc_path = bash_quote(vol->label);
                        buf.append(fmt::format("fm_device_label={}\n", esc_path));
                    }
                    if (vol->fs_type)
                        buf.append(fmt::format("fm_device_fstype=\"{}\"\n", vol->fs_type));
                    buf.append(fmt::format("fm_device_size=\"{}\"\n", vol->size));
                    if (vol->disp_name)
                    {
                        esc_path = bash_quote(vol->disp_name);
                        buf.append(fmt::format("fm_device_display_name=\"%{}\"\n", esc_path));
                    }
                    // clang-format off
                    buf.append(fmt::format("fm_device_icon=\"{}\"\n", vol->icon));
                    buf.append(fmt::format("fm_device_is_mounted=\"{}\"\n", vol->is_mounted ? 1 : 0));
                    buf.append(fmt::format("fm_device_is_optical=\"{}\"\n", vol->is_optical ? 1 : 0));
                    buf.append(fmt::format("fm_device_is_floppy=\"{}\"\n", vol->is_floppy ? 1 : 0));
                    buf.append(fmt::format("fm_device_is_table=\"{}\"\n", vol->is_table ? 1 : 0));
                    buf.append(fmt::format("fm_device_is_removable=\"{}\"\n", vol->is_removable ? 1 : 0));
                    buf.append(fmt::format("fm_device_is_audiocd=\"{}\"\n", vol->is_audiocd ? 1 : 0));
                    buf.append(fmt::format("fm_device_is_dvd=\"{}\"\n", vol->is_dvd ? 1 : 0));
                    buf.append(fmt::format("fm_device_is_blank=\"{}\"\n", vol->is_blank ? 1 : 0));
                    buf.append(fmt::format("fm_device_is_mountable=\"{}\"\n", vol->is_mountable ? 1 : 0));
                    buf.append(fmt::format("fm_device_nopolicy=\"{}\"\n", vol->nopolicy ? 1 : 0));
                }
                buf.append(fmt::format("fm_panel{}_device=\"{}\"\n", p, vol->device_file));
                if (vol->udi)
                {
                    esc_path = bash_quote(vol->udi);
                    buf.append(fmt::format("fm_panel{}_device_udi={}\n", p, esc_path));
                }
                if (vol->mount_point)
                {
                    esc_path = bash_quote(vol->mount_point);
                    buf.append(fmt::format("fm_panel{}_device_mount_point={}\n", p, esc_path));
                }
                if (!vol->label.empty())
                {
                    esc_path = bash_quote(vol->label);
                    buf.append(fmt::format("fm_panel{}_device_label={}\n", p, esc_path));
                }
                if (vol->fs_type)
                    buf.append(fmt::format("fm_panel{}_device_fstype=\"{}\"\n", p, vol->fs_type));
                buf.append(fmt::format("fm_panel{}_device_size=\"{}\"\n", p, vol->size));
                if (vol->disp_name)
                {
                    esc_path = bash_quote(vol->disp_name);
                    buf.append(fmt::format("fm_panel{}_device_display_name={}\n", p, esc_path));
                }
                buf.append(fmt::format("fm_panel{}_device_icon=\"{}\"\n", p, vol->icon));
                buf.append(fmt::format("fm_panel{}_device_is_mounted=\"{}\"\n", p, vol->is_mounted ? 1 : 0));
                buf.append(fmt::format("fm_panel{}_device_is_optical=\"{}\"\n", p, vol->is_optical ? 1 : 0));
                buf.append(fmt::format("fm_panel{}_device_is_table=\"{}\"\n", p, vol->is_table ? 1 : 0));
                buf.append(fmt::format("fm_panel{}_device_is_floppy=\"{}\"\n", p, vol->is_floppy ? 1 : 0));
                buf.append(fmt::format("fm_panel{}_device_is_removable=\"{}\"\n", p, vol->is_removable ? 1 : 0));
                buf.append(fmt::format("fm_panel{}_device_is_audiocd=\"{}\"\n", p, vol->is_audiocd ? 1 : 0));
                buf.append(fmt::format("fm_panel{}_device_is_dvd=\"{}\"\n", p, vol->is_dvd ? 1 : 0));
                buf.append(fmt::format("fm_panel{}_device_is_blank=\"{}\"\n", p, vol->is_blank ? 1 : 0));
                buf.append(fmt::format("fm_panel{}_device_is_mountable=\"{}\"\n", p, vol->is_mountable ? 1 : 0));
                buf.append(fmt::format("fm_panel{}_device_nopolicy=\"{}\"\n", p, vol->nopolicy ? 1 : 0));
                // clang-format on
            }
        }

        // tabs
        int num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[p - 1]));
        for (i = 0; i < num_pages; i++)
        {
            PtkFileBrowser* t_browser = PTK_FILE_BROWSER(
                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), i));
            std::string path2 = bash_quote(ptk_file_browser_get_cwd(t_browser));
            buf.append(fmt::format("fm_pwd_panel{}_tab[{}]={}\n", p, i + 1, path2));
            if (p == file_browser->mypanel)
            {
                buf.append(fmt::format("fm_pwd_tab[{}]={}\n", i + 1, path2));
            }
            if (file_browser == t_browser)
            {
                // my browser
                buf.append(fmt::format("fm_pwd={}\n", path2));
                buf.append(fmt::format("fm_panel=\"{}\"\n", p));
                buf.append(fmt::format("fm_tab=\"{}\"\n", i + 1));
            }
        }
    }
    // my selected files
    buf.append(fmt::format("\nfm_files=(\"${{fm_panel{}_files[@]}}\")\n", file_browser->mypanel));
    buf.append(fmt::format("fm_file=\"${{fm_panel{}_files[0]}}\"\n", file_browser->mypanel));
    buf.append(fmt::format("fm_filename=\"${{fm_filenames[0]}}\"\n"));

    // user
    std::string this_user = Glib::get_user_name();
    esc_path = bash_quote(this_user);
    buf.append(fmt::format("fm_user={}\n", esc_path));

    // variable value
    if (value)
    {
        esc_path = bash_quote(value);
        buf.append(fmt::format("fm_value={}\n", esc_path));
    }
    if (vtask->exec_ptask)
    {
        buf.append(fmt::format("fm_my_task=\"{:p}\"\n", (void*)vtask->exec_ptask));
        buf.append(fmt::format("fm_my_task_id=\"{:p}\"\n", (void*)vtask->exec_ptask));
    }
    buf.append(fmt::format("fm_my_window=\"{:p}\"\n", (void*)main_window));
    buf.append(fmt::format("fm_my_window_id=\"{:p}\"\n", (void*)main_window));

    // utils
    esc_path = bash_quote(xset_get_s("editor"));
    buf.append(fmt::format("fm_editor={}\n", esc_path));
    buf.append(fmt::format("fm_editor_terminal=\"{}\"\n", xset_get_b("editor") ? 1 : 0));

    // set
    if (set)
    {
        // cmd_dir
        if (set->plugin)
        {
            path = g_build_filename(set->plug_dir, "files", nullptr);
            if (!std::filesystem::exists(path))
            {
                free(path);
                path = g_build_filename(set->plug_dir, set->plug_name, nullptr);
            }
        }
        else
        {
            path = g_build_filename(xset_get_config_dir(), "scripts", set->name, nullptr);
        }
        esc_path = bash_quote(path);
        buf.append(fmt::format("fm_cmd_dir={}\n", esc_path));
        free(path);

        // cmd_data
        if (set->plugin)
        {
            XSet* mset = xset_get_plugin_mirror(set);
            path = g_build_filename(xset_get_config_dir(), "plugin-data", mset->name, nullptr);
        }
        else
            path = g_build_filename(xset_get_config_dir(), "plugin-data", set->name, nullptr);
        esc_path = bash_quote(path);
        buf.append(fmt::format("fm_cmd_data={}\n", esc_path));
        free(path);

        // plugin_dir
        if (set->plugin)
        {
            esc_path = bash_quote(set->plug_dir);
            buf.append(fmt::format("fm_plugin_dir={}\n", esc_path));
        }

        // cmd_name
        if (set->menu_label)
        {
            esc_path = bash_quote(set->menu_label);
            buf.append(fmt::format("fm_cmd_name={}\n", esc_path));
        }
    }

    // tmp
    buf.append(fmt::format("fm_tmp_dir=\"{}\"\n", xset_get_user_tmp_dir()));

    // tasks
    if ((ptask = get_selected_task(file_browser->task_view)))
    {
        const char* job_titles[] = {"move", "copy", "trash", "delete", "link", "change", "run"};
        buf.append(fmt::format("\nfm_task_type=\"{}\"\n", job_titles[ptask->task->type]));
        if (ptask->task->type == VFS_FILE_TASK_EXEC)
        {
            esc_path = bash_quote(ptask->task->dest_dir);
            buf.append(fmt::format("fm_task_pwd={}\n", esc_path));
            esc_path = bash_quote(ptask->task->current_file);
            buf.append(fmt::format("fm_task_name={}\n", esc_path));
            esc_path = bash_quote(ptask->task->exec_command);
            buf.append(fmt::format("fm_task_command={}\n", esc_path));
            if (!ptask->task->exec_as_user.empty())
                buf.append(fmt::format("fm_task_user=\"{}\"\n", ptask->task->exec_as_user));
            if (!ptask->task->exec_icon.empty())
                buf.append(fmt::format("fm_task_icon=\"{}\"\n", ptask->task->exec_icon));
            if (ptask->task->exec_pid)
                buf.append(fmt::format("fm_task_pid=\"{}\"\n", ptask->task->exec_pid));
        }
        else
        {
            esc_path = bash_quote(ptask->task->dest_dir);
            buf.append(fmt::format("fm_task_dest_dir={}\n", esc_path));
            esc_path = bash_quote(ptask->task->current_file);
            buf.append(fmt::format("fm_task_current_src_file={}\n", esc_path));
            esc_path = bash_quote(ptask->task->current_dest);
            buf.append(fmt::format("fm_task_current_dest_file={}\n", esc_path));
        }
        buf.append(fmt::format("fm_task_id=\"{:p}\"\n", (void*)ptask));
        if (ptask->task_view && (main_window = get_task_view_window(ptask->task_view)))
        {
            buf.append(fmt::format("fm_task_window=\"{:p}\"\n", (void*)main_window));
            buf.append(fmt::format("fm_task_window_id=\"{:p}\"\n", (void*)main_window));
        }
    }

    buf.append(fmt::format("\n"));
}

static void
on_task_columns_changed(GtkWidget* view, void* user_data)
{
    (void)user_data;
    FMMainWindow* main_window = get_task_view_window(view);
    if (!main_window || !view)
        return;

    int i;
    for (i = 0; i < 13; i++)
    {
        GtkTreeViewColumn* col = gtk_tree_view_get_column(GTK_TREE_VIEW(view), i);
        if (!col)
            return;
        const char* title = gtk_tree_view_column_get_title(col);
        int j;
        for (j = 0; j < 13; j++)
        {
            if (!strcmp(title, task_titles[j]))
                break;
        }
        if (j != 13)
        {
            XSet* set = xset_get(task_names[j]);
            // save column position
            char* pos = g_strdup_printf("%d", i);
            xset_set_set(set, XSET_SET_SET_X, pos);
            free(pos);
            // if the window was opened maximized and stayed maximized, or the
            // window is unmaximized and not fullscreen, save the columns
            if ((!main_window->maximized || main_window->opened_maximized) &&
                !main_window->fullscreen)
            {
                int width = gtk_tree_view_column_get_width(col);
                if (width) // manager unshown, all widths are zero
                {
                    // save column width
                    pos = g_strdup_printf("%d", width);
                    xset_set_set(set, XSET_SET_SET_Y, pos);
                    free(pos);
                }
            }
            // set column visibility
            gtk_tree_view_column_set_visible(col, xset_get_b(task_names[j]));
        }
    }
}

static void
on_task_destroy(GtkWidget* view, void* user_data)
{
    (void)user_data;
    unsigned int id = g_signal_lookup("columns-changed", G_TYPE_FROM_INSTANCE(view));
    if (id)
    {
        unsigned long hand =
            g_signal_handler_find((void*)view, G_SIGNAL_MATCH_ID, id, 0, nullptr, nullptr, nullptr);
        if (hand)
            g_signal_handler_disconnect((void*)view, hand);
    }
    on_task_columns_changed(view, nullptr); // save widths
}

static void
on_task_column_selected(GtkMenuItem* item, GtkWidget* view)
{
    (void)item;
    on_task_columns_changed(view, nullptr);
}

static bool
main_tasks_running(FMMainWindow* main_window)
{
    if (!main_window->task_view || !GTK_IS_TREE_VIEW(main_window->task_view))
        return false;

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
    GtkTreeIter it;
    bool ret = gtk_tree_model_get_iter_first(model, &it);

    return ret;
}

void
main_task_pause_all_queued(PtkFileTask* ptask)
{
    if (!ptask->task_view)
        return;

    PtkFileTask* qtask;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ptask->task_view));
    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, TASK_COL_DATA, &qtask, -1);
            if (qtask && qtask != ptask && qtask->task && !qtask->complete &&
                qtask->task->state_pause == VFS_FILE_TASK_QUEUE)
                ptk_file_task_pause(qtask, VFS_FILE_TASK_PAUSE);
        } while (gtk_tree_model_iter_next(model, &it));
    }
}

void
main_task_start_queued(GtkWidget* view, PtkFileTask* new_task)
{
    GtkTreeModel* model;
    GtkTreeIter it;
    PtkFileTask* qtask;
    PtkFileTask* rtask;
    GSList* running = nullptr;
    GSList* queued = nullptr;
    bool smart;
    smart = xset_get_b("task_q_smart");
    if (!GTK_IS_TREE_VIEW(view))
        return;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, TASK_COL_DATA, &qtask, -1);
            if (qtask && qtask->task && !qtask->complete &&
                qtask->task->state == VFS_FILE_TASK_RUNNING)
            {
                if (qtask->task->state_pause == VFS_FILE_TASK_QUEUE)
                    queued = g_slist_append(queued, qtask);
                else if (qtask->task->state_pause == VFS_FILE_TASK_RUNNING)
                    running = g_slist_append(running, qtask);
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }

    if (new_task && new_task->task && !new_task->complete &&
        new_task->task->state_pause == VFS_FILE_TASK_QUEUE &&
        new_task->task->state == VFS_FILE_TASK_RUNNING)
        queued = g_slist_append(queued, new_task);

    if (!queued || (!smart && running))
        goto _done;

    if (!smart)
    {
        ptk_file_task_pause(static_cast<PtkFileTask*>(queued->data), VFS_FILE_TASK_RUNNING);
        goto _done;
    }

    // smart
    GSList* d;
    GSList* r;
    GSList* q;
    for (q = queued; q; q = q->next)
    {
        qtask = static_cast<PtkFileTask*>(q->data);
        if (!qtask->task->devs)
        {
            // qtask has no devices so run it
            running = g_slist_append(running, qtask);
            ptk_file_task_pause(qtask, VFS_FILE_TASK_RUNNING);
            continue;
        }
        // does qtask have running devices?
        for (r = running; r; r = r->next)
        {
            rtask = static_cast<PtkFileTask*>(r->data);
            ;
            for (d = qtask->task->devs; d; d = d->next)
            {
                if (g_slist_find(rtask->task->devs, d->data))
                    break;
            }
            if (d)
                break;
        }
        if (!r)
        {
            // qtask has no running devices so run it
            running = g_slist_append(running, qtask);
            ptk_file_task_pause(qtask, VFS_FILE_TASK_RUNNING);
            continue;
        }
    }
_done:
    g_slist_free(queued);
    g_slist_free(running);
}

static void
on_task_stop(GtkMenuItem* item, GtkWidget* view, XSet* set2, PtkFileTask* task2)
{
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    PtkFileTask* ptask;
    XSet* set;
    int job;
    enum MainWindowJob
    {
        JOB_STOP,
        JOB_PAUSE,
        JOB_QUEUE,
        JOB_RESUME
    };

    if (item)
        set = XSET(g_object_get_data(G_OBJECT(item), "set"));
    else
        set = set2;
    if (!set || !Glib::str_has_prefix(set->name, "task_"))
        return;

    char* name = set->name + 5;
    if (Glib::str_has_prefix(name, "stop"))
        job = JOB_STOP;
    else if (Glib::str_has_prefix(name, "pause"))
        job = JOB_PAUSE;
    else if (Glib::str_has_prefix(name, "que"))
        job = JOB_QUEUE;
    else if (Glib::str_has_prefix(name, "resume"))
        job = JOB_RESUME;
    else
        return;
    bool all = (g_str_has_suffix(name, "_all"));

    if (all)
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        ptask = nullptr;
    }
    else
    {
        if (item)
            ptask = static_cast<PtkFileTask*>(g_object_get_data(G_OBJECT(item), "task"));
        else
            ptask = task2;
        if (!ptask)
            return;
    }

    if (!model || gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            if (model)
                gtk_tree_model_get(model, &it, TASK_COL_DATA, &ptask, -1);
            if (ptask && ptask->task && !ptask->complete &&
                (ptask->task->type != VFS_FILE_TASK_EXEC || ptask->task->exec_pid ||
                 job == JOB_STOP))
            {
                switch (job)
                {
                    case JOB_STOP:
                        ptk_file_task_cancel(ptask);
                        break;
                    case JOB_PAUSE:
                        ptk_file_task_pause(ptask, VFS_FILE_TASK_PAUSE);
                        break;
                    case JOB_QUEUE:
                        ptk_file_task_pause(ptask, VFS_FILE_TASK_QUEUE);
                        break;
                    case JOB_RESUME:
                        ptk_file_task_pause(ptask, VFS_FILE_TASK_RUNNING);
                        break;
                    default:
                        break;
                }
            }
        } while (model && gtk_tree_model_iter_next(model, &it));
    }
    main_task_start_queued(view, nullptr);
}

static bool
idle_set_task_height(FMMainWindow* main_window)
{
    GtkAllocation allocation;
    int pos;
    int taskh;

    gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);

    // set new config panel sizes to half of window
    if (!xset_is("panel_sliders"))
    {
        // this isn't perfect because panel half-width is set before user
        // adjusts window size
        XSet* set = xset_get("panel_sliders");
        set->x = g_strdup_printf("%d", allocation.width / 2);
        set->y = g_strdup_printf("%d", allocation.width / 2);
        set->s = g_strdup_printf("%d", allocation.height / 2);
    }

    // restore height (in case window height changed)
    taskh = xset_get_int("task_show_manager", "x"); // task height >=0.9.2
    if (taskh == 0)
    {
        // use pre-0.9.2 slider pos to calculate height
        pos = xset_get_int("panel_sliders", "z"); // < 0.9.2 slider pos
        if (pos == 0)
            taskh = 200;
        else
            taskh = allocation.height - pos;
    }
    if (taskh > allocation.height / 2)
        taskh = allocation.height / 2;
    if (taskh < 1)
        taskh = 90;
    // LOG_INFO("SHOW  win {}x{}    task height {}   slider {}", allocation.width,
    // allocation.height, taskh, allocation.height - taskh);
    gtk_paned_set_position(GTK_PANED(main_window->task_vpane), allocation.height - taskh);
    return false;
}

static void
show_task_manager(FMMainWindow* main_window, bool show)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);

    if (show)
    {
        if (!gtk_widget_get_visible(GTK_WIDGET(main_window->task_scroll)))
        {
            gtk_widget_show(main_window->task_scroll);
            // allow vpane to auto-adjust before setting new slider pos
            g_idle_add((GSourceFunc)idle_set_task_height, main_window);
        }
    }
    else
    {
        // save height
        if (gtk_widget_get_visible(GTK_WIDGET(main_window->task_scroll)))
        {
            int pos = gtk_paned_get_position(GTK_PANED(main_window->task_vpane));
            if (pos)
            {
                // save slider pos for version < 0.9.2 (in case of downgrade)
                char* posa = g_strdup_printf("%d", pos);
                xset_set("panel_sliders", "z", posa);
                free(posa);
                // save absolute height introduced v0.9.2
                posa = g_strdup_printf("%d", allocation.height - pos);
                xset_set("task_show_manager", "x", posa);
                free(posa);
                // LOG_INFO("HIDE  win {}x{}    task height {}   slider {}", allocation.width,
                // allocation.height, allocation.height - pos, pos);
            }
        }
        // hide
        bool tasks_has_focus = gtk_widget_is_focus(GTK_WIDGET(main_window->task_view));
        gtk_widget_hide(GTK_WIDGET(main_window->task_scroll));
        if (tasks_has_focus)
        {
            // focus the file list
            PtkFileBrowser* file_browser =
                PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
            if (file_browser)
                gtk_widget_grab_focus(file_browser->folder_view);
        }
    }
}

static void
on_task_popup_show(GtkMenuItem* item, FMMainWindow* main_window, char* name2)
{
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    char* name = nullptr;

    if (item)
        name = (char*)g_object_get_data(G_OBJECT(item), "name");
    else
        name = name2;

    if (name)
    {
        if (!strcmp(name, "task_show_manager"))
        {
            if (xset_get_b("task_show_manager"))
                xset_set_b("task_hide_manager", false);
            else
            {
                xset_set_b("task_hide_manager", true);
                xset_set_b("task_show_manager", false);
            }
        }
        else
        {
            if (xset_get_b("task_hide_manager"))
                xset_set_b("task_show_manager", false);
            else
            {
                xset_set_b("task_hide_manager", false);
                xset_set_b("task_show_manager", true);
            }
        }
    }

    if (xset_get_b("task_show_manager"))
        show_task_manager(main_window, true);
    else
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
            show_task_manager(main_window, true);
        else if (xset_get_b("task_hide_manager"))
            show_task_manager(main_window, false);
    }
}

static void
on_task_popup_errset(GtkMenuItem* item, FMMainWindow* main_window, char* name2)
{
    (void)main_window;
    char* name;
    if (item)
        name = (char*)g_object_get_data(G_OBJECT(item), "name");
    else
        name = name2;

    if (!name)
        return;

    if (!strcmp(name, "task_err_first"))
    {
        if (xset_get_b("task_err_first"))
        {
            xset_set_b("task_err_any", false);
            xset_set_b("task_err_cont", false);
        }
        else
        {
            xset_set_b("task_err_any", false);
            xset_set_b("task_err_cont", true);
        }
    }
    else if (!strcmp(name, "task_err_any"))
    {
        if (xset_get_b("task_err_any"))
        {
            xset_set_b("task_err_first", false);
            xset_set_b("task_err_cont", false);
        }
        else
        {
            xset_set_b("task_err_first", false);
            xset_set_b("task_err_cont", true);
        }
    }
    else
    {
        if (xset_get_b("task_err_cont"))
        {
            xset_set_b("task_err_first", false);
            xset_set_b("task_err_any", false);
        }
        else
        {
            xset_set_b("task_err_first", true);
            xset_set_b("task_err_any", false);
        }
    }
}

static void
main_task_prepare_menu(FMMainWindow* main_window, GtkWidget* menu, GtkAccelGroup* accel_group)
{
    (void)menu;
    (void)accel_group;
    XSet* set;
    XSet* set_radio;

    GtkWidget* parent = main_window->task_view;
    set = xset_set_cb("task_show_manager", (GFunc)on_task_popup_show, main_window);
    xset_set_ob1(set, "name", set->name);
    xset_set_ob2(set, nullptr, nullptr);
    set_radio = set;
    set = xset_set_cb("task_hide_manager", (GFunc)on_task_popup_show, main_window);
    xset_set_ob1(set, "name", set->name);
    xset_set_ob2(set, nullptr, set_radio);

    xset_set_cb("task_col_count", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_path", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_file", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_to", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_progress", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_total", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_started", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_elapsed", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_curspeed", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_curest", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_avgspeed", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_avgest", (GFunc)on_task_column_selected, parent);
    xset_set_cb("task_col_reorder", (GFunc)on_reorder, parent);

    set = xset_set_cb("task_err_first", (GFunc)on_task_popup_errset, main_window);
    xset_set_ob1(set, "name", set->name);
    xset_set_ob2(set, nullptr, nullptr);
    set_radio = set;
    set = xset_set_cb("task_err_any", (GFunc)on_task_popup_errset, main_window);
    xset_set_ob1(set, "name", set->name);
    xset_set_ob2(set, nullptr, set_radio);
    set = xset_set_cb("task_err_cont", (GFunc)on_task_popup_errset, main_window);
    xset_set_ob1(set, "name", set->name);
    xset_set_ob2(set, nullptr, set_radio);
}

static PtkFileTask*
get_selected_task(GtkWidget* view)
{
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;
    GtkTreeIter it;
    PtkFileTask* ptask = nullptr;

    if (!view)
        return nullptr;
    FMMainWindow* main_window = get_task_view_window(view);
    if (!main_window)
        return nullptr;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    if (gtk_tree_selection_get_selected(tree_sel, nullptr, &it))
    {
        gtk_tree_model_get(model, &it, TASK_COL_DATA, &ptask, -1);
    }
    return ptask;
}

static void
show_task_dialog(GtkWidget* widget, GtkWidget* view)
{
    (void)widget;
    PtkFileTask* ptask = get_selected_task(view);
    if (!ptask)
        return;

    ptk_file_task_lock(ptask);
    ptk_file_task_progress_open(ptask);
    if (ptask->task->state_pause != VFS_FILE_TASK_RUNNING)
    {
        // update dlg
        ptask->pause_change = true;
        ptask->progress_count = 50; // trigger fast display
    }
    if (ptask->progress_dlg)
        gtk_window_present(GTK_WINDOW(ptask->progress_dlg));
    ptk_file_task_unlock(ptask);
}

static bool
on_task_button_press_event(GtkWidget* view, GdkEventButton* event, FMMainWindow* main_window)
{
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path;
    GtkTreeViewColumn* col = nullptr;
    GtkTreeIter it;
    PtkFileTask* ptask = nullptr;
    XSet* set;
    bool is_tasks;

    if (event->type != GDK_BUTTON_PRESS)
        return false;

    if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
        main_window_event(main_window,
                          event_handler.win_click,
                          "evt_win_click",
                          0,
                          0,
                          "tasklist",
                          0,
                          event->button,
                          event->state,
                          true))
        return false;

    switch (event->button)
    {
        case 1:
        case 2:
            // left or middle click
            // get selected task
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
            // LOG_INFO("x = {}  y = {}", event->x, event->y);
            // due to bug in gtk_tree_view_get_path_at_pos (gtk 2.24), a click
            // on the column header resize divider registers as a click on the
            // first row first column.  So if event->x < 7 ignore
            if (event->x < 7)
                return false;
            if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                               event->x,
                                               event->y,
                                               &tree_path,
                                               &col,
                                               nullptr,
                                               nullptr))
                return false;
            if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
                gtk_tree_model_get(model, &it, TASK_COL_DATA, &ptask, -1);
            gtk_tree_path_free(tree_path);

            if (!ptask)
                return false;
            if (event->button == 1 && g_strcmp0(gtk_tree_view_column_get_title(col), "Status"))
                return false;
            const char* sname;
            switch (ptask->task->state_pause)
            {
                case VFS_FILE_TASK_PAUSE:
                    sname = ztd::strdup("task_que");
                    break;
                case VFS_FILE_TASK_QUEUE:
                    sname = ztd::strdup("task_resume");
                    break;
                default:
                    sname = ztd::strdup("task_pause");
            }
            set = xset_get(sname);
            on_task_stop(nullptr, view, set, ptask);
            return true;
            break;
        case 3:
            // get selected task
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
            if ((is_tasks = gtk_tree_model_get_iter_first(model, &it)))
            {
                if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                                  event->x,
                                                  event->y,
                                                  &tree_path,
                                                  &col,
                                                  nullptr,
                                                  nullptr))
                {
                    if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
                        gtk_tree_model_get(model, &it, TASK_COL_DATA, &ptask, -1);
                    gtk_tree_path_free(tree_path);
                }
            }

            // build popup
            PtkFileBrowser* file_browser;
            file_browser = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
            if (!file_browser)
                return false;
            GtkWidget* popup;
            GtkAccelGroup* accel_group;
            XSetContext* context;
            popup = gtk_menu_new();
            accel_group = gtk_accel_group_new();
            context = xset_context_new();
            main_context_fill(file_browser, context);

            set = xset_set_cb("task_stop", (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = !ptask;

            set = xset_set_cb("task_pause", (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = (!ptask || ptask->task->state_pause == VFS_FILE_TASK_PAUSE ||
                            (ptask->task->type == VFS_FILE_TASK_EXEC && !ptask->task->exec_pid));

            set = xset_set_cb("task_que", (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = (!ptask || ptask->task->state_pause == VFS_FILE_TASK_QUEUE ||
                            (ptask->task->type == VFS_FILE_TASK_EXEC && !ptask->task->exec_pid));

            set = xset_set_cb("task_resume", (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = (!ptask || ptask->task->state_pause == VFS_FILE_TASK_RUNNING ||
                            (ptask->task->type == VFS_FILE_TASK_EXEC && !ptask->task->exec_pid));

            xset_set_cb("task_stop_all", (GFunc)on_task_stop, view);
            xset_set_cb("task_pause_all", (GFunc)on_task_stop, view);
            xset_set_cb("task_que_all", (GFunc)on_task_stop, view);
            xset_set_cb("task_resume_all", (GFunc)on_task_stop, view);
            set = xset_get("task_all");
            set->disable = !is_tasks;

            const char* showout;
            showout = ztd::strdup("");
            if (ptask && ptask->pop_handler)
            {
                xset_set_cb("task_showout", (GFunc)show_task_dialog, view);
                showout = ztd::strdup(" task_showout");
            }

            main_task_prepare_menu(main_window, popup, accel_group);

            char* menu_elements;
            menu_elements = g_strdup_printf(
                "task_stop separator task_pause task_que task_resume%s task_all separator "
                "task_show_manager "
                "task_hide_manager separator task_columns task_popups task_errors task_queue",
                showout);
            xset_add_menu(file_browser, popup, accel_group, menu_elements);
            free(menu_elements);

            gtk_widget_show_all(GTK_WIDGET(popup));
            g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
            g_signal_connect(popup, "key_press_event", G_CALLBACK(xset_menu_keypress), nullptr);
            gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
            // right click
            break;
        default:
            break;
    }

    return false;
}

static void
on_task_row_activated(GtkWidget* view, GtkTreePath* tree_path, GtkTreeViewColumn* col,
                      void* user_data)
{
    (void)col;
    (void)user_data;
    GtkTreeModel* model;
    GtkTreeIter it;
    PtkFileTask* ptask;

    FMMainWindow* main_window = get_task_view_window(view);
    if (!main_window)
        return;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (!gtk_tree_model_get_iter(model, &it, tree_path))
        return;

    gtk_tree_model_get(model, &it, TASK_COL_DATA, &ptask, -1);
    if (ptask)
    {
        if (ptask->pop_handler)
        {
            // show custom dialog
            LOG_INFO("TASK_POPUP >>> {}", ptask->pop_handler);
            std::string command = fmt::format("bash -c {}", ptask->pop_handler);
            Glib::spawn_command_line_async(command);
        }
        else
        {
            // show normal dialog
            show_task_dialog(nullptr, view);
        }
    }
}

void
main_task_view_remove_task(PtkFileTask* ptask)
{
    // LOG_INFO("main_task_view_remove_task  ptask={}", ptask);

    GtkWidget* view = ptask->task_view;
    if (!view)
        return;

    FMMainWindow* main_window = get_task_view_window(view);
    if (!main_window)
        return;

    PtkFileTask* ptaskt = nullptr;
    GtkTreeIter it;

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, TASK_COL_DATA, &ptaskt, -1);
        } while (ptaskt != ptask && gtk_tree_model_iter_next(model, &it));
    }
    if (ptaskt == ptask)
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);

    if (!gtk_tree_model_get_iter_first(model, &it))
    {
        if (xset_get_b("task_hide_manager"))
            show_task_manager(main_window, false);
    }

    update_window_title(nullptr, main_window);
    // LOG_INFO("main_task_view_remove_task DONE ptask={}", ptask);
}

void
main_task_view_update_task(PtkFileTask* ptask)
{
    PtkFileTask* ptaskt = nullptr;
    GtkWidget* view;
    GtkTreeModel* model;
    GtkTreeIter it;
    GdkPixbuf* pixbuf;
    const char* dest_dir;
    XSet* set;

    // LOG_INFO("main_task_view_update_task  ptask={}", ptask);
    // clang-format off
    const char* job_titles[] = {"moving",
                                "copying",
                                "trashing",
                                "deleting",
                                "linking",
                                "changing",
                                "running"};
    // clang-format on

    if (!ptask)
        return;

    view = ptask->task_view;
    if (!view)
        return;

    FMMainWindow* main_window = get_task_view_window(view);
    if (!main_window)
        return;

    if (ptask->task->type != VFS_FILE_TASK_EXEC)
        dest_dir = ptask->task->dest_dir.c_str();
    else
        dest_dir = nullptr;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, TASK_COL_DATA, &ptaskt, -1);
        } while (ptaskt != ptask && gtk_tree_model_iter_next(model, &it));
    }
    if (ptaskt != ptask)
    {
        // new row
        char buf[64];
        strftime(buf, sizeof(buf), "%H:%M", localtime(&ptask->task->start_time));
        char* started = ztd::strdup(buf);
        gtk_list_store_insert_with_values(GTK_LIST_STORE(model),
                                          &it,
                                          0,
                                          TASK_COL_TO,
                                          dest_dir,
                                          TASK_COL_STARTED,
                                          started,
                                          TASK_COL_STARTTIME,
                                          (int64_t)ptask->task->start_time,
                                          TASK_COL_DATA,
                                          ptask,
                                          -1);
        free(started);
    }

    if (ptask->task->state_pause == VFS_FILE_TASK_RUNNING || ptask->pause_change_view)
    {
        // update row
        const char* path;
        const char* file;

        int percent = ptask->task->percent;
        if (percent < 0)
            percent = 0;
        else if (percent > 100)
            percent = 100;
        if (ptask->task->type != VFS_FILE_TASK_EXEC)
        {
            if (!ptask->task->current_file.empty())
            {
                path = g_path_get_dirname(ptask->task->current_file.c_str());
                file = g_path_get_basename(ptask->task->current_file.c_str());
            }
        }
        else
        {
            path = ptask->task->dest_dir.c_str(); // cwd
            file = g_strdup_printf("( %s )", ptask->task->current_file.c_str());
        }

        // status
        const char* status;
        char* status2 = nullptr;
        char* status3;
        if (ptask->task->type != VFS_FILE_TASK_EXEC)
        {
            if (!ptask->err_count)
                status = job_titles[ptask->task->type];
            else
            {
                status2 =
                    g_strdup_printf("%d error %s", ptask->err_count, job_titles[ptask->task->type]);
                status = status2;
            }
        }
        else
        {
            // exec task
            if (!ptask->task->exec_action.empty())
                status = ptask->task->exec_action.c_str();
            else
                status = job_titles[ptask->task->type];
        }
        if (ptask->task->state_pause == VFS_FILE_TASK_PAUSE)
            status3 = g_strdup_printf("%s %s", "paused", status);
        else if (ptask->task->state_pause == VFS_FILE_TASK_QUEUE)
            status3 = g_strdup_printf("%s %s", "queued", status);
        else
            status3 = ztd::strdup(status);

        // update icon if queue state changed
        pixbuf = nullptr;
        if (ptask->pause_change_view)
        {
            // icon
            std::string iname;
            if (ptask->task->state_pause == VFS_FILE_TASK_PAUSE)
            {
                set = xset_get("task_pause");
                iname = set->icon ? set->icon : "media-playback-pause";
            }
            else if (ptask->task->state_pause == VFS_FILE_TASK_QUEUE)
            {
                set = xset_get("task_que");
                iname = set->icon ? set->icon : "list-add";
            }
            else if (ptask->err_count && ptask->task->type != VFS_FILE_TASK_EXEC)
                iname = "error";
            else if (ptask->task->type == 0 || ptask->task->type == 1 || ptask->task->type == 4)
                iname = "stock_copy";
            else if (ptask->task->type == 2 || ptask->task->type == 3)
                iname = "stock_delete";
            else if (ptask->task->type == VFS_FILE_TASK_EXEC && !ptask->task->exec_icon.empty())
                iname = ptask->task->exec_icon;
            else
                iname = "gtk-execute";

            int icon_size = app_settings.small_icon_size;
            if (icon_size > PANE_MAX_ICON_SIZE)
                icon_size = PANE_MAX_ICON_SIZE;

            pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                              iname.c_str(),
                                              icon_size,
                                              (GtkIconLookupFlags)GTK_ICON_LOOKUP_USE_BUILTIN,
                                              nullptr);
            if (!pixbuf)
                pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                                  "gtk-execute",
                                                  icon_size,
                                                  (GtkIconLookupFlags)GTK_ICON_LOOKUP_USE_BUILTIN,
                                                  nullptr);
            ptask->pause_change_view = false;
        }

        if (ptask->task->type != VFS_FILE_TASK_EXEC || ptaskt != ptask /* new task */)
        {
            if (pixbuf)
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   TASK_COL_ICON,
                                   pixbuf,
                                   TASK_COL_STATUS,
                                   status3,
                                   TASK_COL_COUNT,
                                   ptask->dsp_file_count,
                                   TASK_COL_PATH,
                                   path,
                                   TASK_COL_FILE,
                                   file,
                                   TASK_COL_PROGRESS,
                                   percent,
                                   TASK_COL_TOTAL,
                                   ptask->dsp_size_tally,
                                   TASK_COL_ELAPSED,
                                   ptask->dsp_elapsed,
                                   TASK_COL_CURSPEED,
                                   ptask->dsp_curspeed,
                                   TASK_COL_CUREST,
                                   ptask->dsp_curest,
                                   TASK_COL_AVGSPEED,
                                   ptask->dsp_avgspeed,
                                   TASK_COL_AVGEST,
                                   ptask->dsp_avgest,
                                   -1);
            else
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   TASK_COL_STATUS,
                                   status3,
                                   TASK_COL_COUNT,
                                   ptask->dsp_file_count,
                                   TASK_COL_PATH,
                                   path,
                                   TASK_COL_FILE,
                                   file,
                                   TASK_COL_PROGRESS,
                                   percent,
                                   TASK_COL_TOTAL,
                                   ptask->dsp_size_tally,
                                   TASK_COL_ELAPSED,
                                   ptask->dsp_elapsed,
                                   TASK_COL_CURSPEED,
                                   ptask->dsp_curspeed,
                                   TASK_COL_CUREST,
                                   ptask->dsp_curest,
                                   TASK_COL_AVGSPEED,
                                   ptask->dsp_avgspeed,
                                   TASK_COL_AVGEST,
                                   ptask->dsp_avgest,
                                   -1);
        }
        else if (pixbuf)
            gtk_list_store_set(GTK_LIST_STORE(model),
                               &it,
                               TASK_COL_ICON,
                               pixbuf,
                               TASK_COL_STATUS,
                               status3,
                               TASK_COL_PROGRESS,
                               percent,
                               TASK_COL_ELAPSED,
                               ptask->dsp_elapsed,
                               -1);
        else
            gtk_list_store_set(GTK_LIST_STORE(model),
                               &it,
                               TASK_COL_STATUS,
                               status3,
                               TASK_COL_PROGRESS,
                               percent,
                               TASK_COL_ELAPSED,
                               ptask->dsp_elapsed,
                               -1);

        // Clearing up
        free(status2);
        free(status3);
        if (pixbuf)
            g_object_unref(pixbuf);

        if (!gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(view))))
            show_task_manager(main_window, true);

        update_window_title(nullptr, main_window);
    }
    else
    {
        // task is paused
        gtk_list_store_set(GTK_LIST_STORE(model),
                           &it,
                           TASK_COL_TOTAL,
                           ptask->dsp_size_tally,
                           TASK_COL_ELAPSED,
                           ptask->dsp_elapsed,
                           TASK_COL_CURSPEED,
                           ptask->dsp_curspeed,
                           TASK_COL_CUREST,
                           ptask->dsp_curest,
                           TASK_COL_AVGSPEED,
                           ptask->dsp_avgspeed,
                           TASK_COL_AVGEST,
                           ptask->dsp_avgest,
                           -1);
    }
    // LOG_INFO("DONE main_task_view_update_task");
}

static GtkWidget*
main_task_view_new(FMMainWindow* main_window)
{
    int i;
    int j;
    int width;
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    GtkCellRenderer* pix_renderer;

    int cols[] = {TASK_COL_STATUS,
                  TASK_COL_COUNT,
                  TASK_COL_PATH,
                  TASK_COL_FILE,
                  TASK_COL_TO,
                  TASK_COL_PROGRESS,
                  TASK_COL_TOTAL,
                  TASK_COL_STARTED,
                  TASK_COL_ELAPSED,
                  TASK_COL_CURSPEED,
                  TASK_COL_CUREST,
                  TASK_COL_AVGSPEED,
                  TASK_COL_AVGEST,
                  TASK_COL_STARTTIME,
                  TASK_COL_ICON,
                  TASK_COL_DATA};
    int num_cols = G_N_ELEMENTS(cols);

    // Model
    GtkListStore* list = gtk_list_store_new(num_cols,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_INT,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_INT64,
                                            GDK_TYPE_PIXBUF,
                                            G_TYPE_POINTER);

    // View
    GtkWidget* view = gtk_tree_view_new();
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(list));
    // gtk_tree_view_set_model adds a ref
    g_object_unref(list);
    // gtk_tree_view_set_single_click(GTK_TREE_VIEW(view), true);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), false);
    // gtk_tree_view_set_single_click_timeout(GTK_TREE_VIEW(view), SINGLE_CLICK_TIMEOUT);

    // Columns
    for (i = 0; i < 13; i++)
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable(col, true);
        gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_min_width(col, 20);

        // column order
        for (j = 0; j < 13; j++)
        {
            if (xset_get_int(task_names[j], "x") == i)
                break;
        }
        if (j == 13)
            j = i; // failsafe
        else
        {
            // column width
            width = xset_get_int(task_names[j], "y");
            if (width == 0)
                width = 80;
            gtk_tree_view_column_set_fixed_width(col, width);
        }

        switch (cols[j])
        {
            case TASK_COL_STATUS:
                // Icon and Text
                renderer = gtk_cell_renderer_text_new();
                pix_renderer = gtk_cell_renderer_pixbuf_new();
                gtk_tree_view_column_pack_start(col, pix_renderer, false);
                gtk_tree_view_column_pack_end(col, renderer, true);
                gtk_tree_view_column_set_attributes(col,
                                                    pix_renderer,
                                                    "pixbuf",
                                                    TASK_COL_ICON,
                                                    nullptr);
                gtk_tree_view_column_set_attributes(col,
                                                    renderer,
                                                    "text",
                                                    TASK_COL_STATUS,
                                                    nullptr);
                gtk_tree_view_column_set_expand(col, false);
                gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
                gtk_tree_view_column_set_min_width(col, 60);
                break;
            case TASK_COL_PROGRESS:
                // Progress Bar
                renderer = gtk_cell_renderer_progress_new();
                gtk_tree_view_column_pack_start(col, renderer, true);
                gtk_tree_view_column_set_attributes(col, renderer, "value", cols[j], nullptr);
                break;
            default:
                // Text Column
                renderer = gtk_cell_renderer_text_new();
                gtk_tree_view_column_pack_start(col, renderer, true);
                gtk_tree_view_column_set_attributes(col, renderer, "text", cols[j], nullptr);

                // ellipsize some columns
                if (cols[j] == TASK_COL_FILE || cols[j] == TASK_COL_PATH || cols[j] == TASK_COL_TO)
                {
                    GValue val = GValue();
                    g_value_init(&val, G_TYPE_CHAR);
                    g_value_set_schar(&val, PANGO_ELLIPSIZE_MIDDLE);
                    g_object_set_property(G_OBJECT(renderer), "ellipsize", &val);
                    g_value_unset(&val);
                }
                break;
        }

        gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
        gtk_tree_view_column_set_title(col, task_titles[j]);
        gtk_tree_view_column_set_reorderable(col, true);
        gtk_tree_view_column_set_visible(col, xset_get_b(task_names[j]));
        if (j == TASK_COL_FILE) //|| j == TASK_COL_PATH || j == TASK_COL_TO
        {
            gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
            gtk_tree_view_column_set_min_width(col, 20);
            // If set_expand is true, columns flicker and adjustment is
            // difficult during high i/o load on some systems
            gtk_tree_view_column_set_expand(col, false);
        }
    }

    // invisible Starttime col for sorting
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, true);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col, renderer, "text", TASK_COL_STARTTIME, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    gtk_tree_view_column_set_title(col, "StartTime");
    gtk_tree_view_column_set_reorderable(col, false);
    gtk_tree_view_column_set_visible(col, false);

    // Sort
    if (GTK_IS_TREE_SORTABLE(list))
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list),
                                             TASK_COL_STARTTIME,
                                             GTK_SORT_ASCENDING);

    g_signal_connect(view, "row-activated", G_CALLBACK(on_task_row_activated), nullptr);
    g_signal_connect(view, "columns-changed", G_CALLBACK(on_task_columns_changed), nullptr);
    g_signal_connect(view, "destroy", G_CALLBACK(on_task_destroy), nullptr);
    g_signal_connect(view,
                     "button-press-event",
                     G_CALLBACK(on_task_button_press_event),
                     main_window);

    return view;
}

// ============== socket commands

static bool
get_bool(const std::string& value)
{
    if (ztd::same(ztd::lower(value), "yes") || ztd::same(value, "1"))
        return true;
    else if (ztd::same(ztd::lower(value), "no") || ztd::same(value, "0"))
        return false;

    // throw std::logic_error("");
    LOG_WARN("socket command defaulting to false, invalid value: {}", value);
    LOG_INFO("supported socket command values are 'yes|1|no|0");
    return false;
}

static char*
unescape(const char* t)
{
    if (!t)
        return nullptr;

    char* s = ztd::strdup(t);

    int i = 0, j = 0;
    while (t[i])
    {
        switch (t[i])
        {
            case '\\':
                switch (t[++i])
                {
                    case 'n':
                        s[j] = '\n';
                        break;
                    case 't':
                        s[j] = '\t';
                        break;
                    case '\\':
                        s[j] = '\\';
                        break;
                    case '\"':
                        s[j] = '\"';
                        break;
                    default:
                        // copy
                        s[j++] = '\\';
                        s[j] = t[i];
                }
                break;
            default:
                s[j] = t[i];
        }
        ++i;
        ++j;
    }
    s[j] = t[i]; // null char
    return s;
}

static bool
delayed_show_menu(GtkWidget* menu)
{
    FMMainWindow* main_window = fm_main_window_get_last_active();
    if (main_window)
        gtk_window_present(GTK_WINDOW(main_window));
    gtk_widget_show_all(GTK_WIDGET(menu));
    gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
    g_signal_connect(G_OBJECT(menu), "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    return false;
}

char
main_window_socket_command(char* argv[], char** reply)
{
    int j;
    int panel = 0;
    int tab = 0;
    char* window = nullptr;
    char* str;
    FMMainWindow* main_window;
    PtkFileBrowser* file_browser;
    GList* l;
    int height;
    int width;
    GtkWidget* widget;
    // must match file-browser.c
    const char* column_titles[] = {"Name", "Size", "Type", "Permission", "Owner", "Modified"};

    *reply = nullptr;
    if (!(argv && argv[0]))
    {
        *reply = ztd::strdup("spacefm: invalid socket command\n");
        return 1;
    }

    // cmd options
    int i = 1;
    while (argv[i] && argv[i][0] == '-')
    {
        if (!strcmp(argv[i], "--window"))
        {
            if (!argv[i + 1])
                goto _missing_arg;
            window = argv[i + 1];
            i += 2;
            continue;
        }
        else if (!strcmp(argv[i], "--panel"))
        {
            if (!argv[i + 1])
                goto _missing_arg;
            panel = strtol(argv[i + 1], nullptr, 10);
            i += 2;
            continue;
        }
        else if (!strcmp(argv[i], "--tab"))
        {
            if (!argv[i + 1])
                goto _missing_arg;
            tab = strtol(argv[i + 1], nullptr, 10);
            i += 2;
            continue;
        }
        *reply = g_strdup_printf("spacefm: invalid option '%s'\n", argv[i]);
        return 1;
    _missing_arg:
        *reply = g_strdup_printf("spacefm: option %s requires an argument\n", argv[i]);
        return 1;
    }

    // window
    if (!window)
    {
        if (!(main_window = fm_main_window_get_last_active()))
        {
            *reply = ztd::strdup("spacefm: invalid window\n");
            return 2;
        }
    }
    else
    {
        main_window = nullptr;
        for (FMMainWindow* window2: all_windows)
        {
            str = g_strdup_printf("%p", window2);
            if (!strcmp(str, window))
            {
                main_window = window2;
                free(str);
                break;
            }
            free(str);
        }
        if (!main_window)
        {
            *reply = g_strdup_printf("spacefm: invalid window %s\n", window);
            return 2;
        }
    }

    // panel
    if (!panel)
        panel = main_window->curpanel;
    if (panel < 1 || panel > 4)
    {
        *reply = g_strdup_printf("spacefm: invalid panel %d\n", panel);
        return 2;
    }
    if (!xset_get_b_panel(panel, "show") ||
        gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[panel - 1])) == -1)
    {
        *reply = g_strdup_printf("spacefm: panel %d is not visible\n", panel);
        return 2;
    }

    // tab
    if (!tab)
    {
        tab = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[panel - 1])) + 1;
    }
    if (tab < 1 || tab > gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[panel - 1])))
    {
        *reply = g_strdup_printf("spacefm: invalid tab %d\n", tab);
        return 2;
    }
    file_browser = PTK_FILE_BROWSER(
        gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[panel - 1]), tab - 1));

    // command
    if (!strcmp(argv[0], "set"))
    {
        if (!argv[i])
        {
            *reply = ztd::strdup("spacefm: command set requires an argument\n");
            return 1;
        }
        if (!strcmp(argv[i], "window_size") || !strcmp(argv[i], "window_position"))
        {
            height = width = 0;
            if (argv[i + 1])
            {
                str = strchr(argv[i + 1], 'x');
                if (!str)
                {
                    if (argv[i + 2])
                    {
                        width = strtol(argv[i + 1], nullptr, 10);
                        height = strtol(argv[i + 2], nullptr, 10);
                    }
                }
                else
                {
                    str[0] = '\0';
                    width = strtol(argv[i + 1], nullptr, 10);
                    height = strtol(str + 1, nullptr, 10);
                }
            }
            if (height < 1 || width < 1)
            {
                *reply = g_strdup_printf("spacefm: invalid %s value\n", argv[i]);
                return 2;
            }
            if (!strcmp(argv[i], "window_size"))
                gtk_window_resize(GTK_WINDOW(main_window), width, height);
            else
                gtk_window_move(GTK_WINDOW(main_window), width, height);
        }
        else if (!strcmp(argv[i], "window_maximized"))
        {
            if (get_bool(argv[i + 1]))
                gtk_window_maximize(GTK_WINDOW(main_window));
            else
                gtk_window_unmaximize(GTK_WINDOW(main_window));
        }
        else if (!strcmp(argv[i], "window_fullscreen"))
        {
            xset_set_b("main_full", get_bool(argv[i + 1]));
            on_fullscreen_activate(nullptr, main_window);
        }
        else if (!strcmp(argv[i], "screen_size"))
        {
        }
        else if (!strcmp(argv[i], "window_vslider_top") ||
                 !strcmp(argv[i], "window_vslider_bottom") || !strcmp(argv[i], "window_hslider") ||
                 !strcmp(argv[i], "window_tslider"))
        {
            width = -1;
            if (argv[i + 1])
                width = strtol(argv[i + 1], nullptr, 10);
            if (width < 0)
            {
                *reply = ztd::strdup("spacefm: invalid slider value\n");
                return 2;
            }
            if (!strcmp(argv[i] + 7, "vslider_top"))
                widget = main_window->hpane_top;
            else if (!strcmp(argv[i] + 7, "vslider_bottom"))
                widget = main_window->hpane_bottom;
            else if (!strcmp(argv[i] + 7, "hslider"))
                widget = main_window->vpane;
            else
                widget = main_window->task_vpane;
            gtk_paned_set_position(GTK_PANED(widget), width);
        }
        else if (!strcmp(argv[i], "focused_panel"))
        {
            width = 0;
            if (argv[i + 1])
            {
                if (!strcmp(argv[i + 1], "prev"))
                    width = -1;
                else if (!strcmp(argv[i + 1], "next"))
                    width = -2;
                else if (!strcmp(argv[i + 1], "hide"))
                    width = -3;
                else
                    width = strtol(argv[i + 1], nullptr, 10);
            }
            if (width == 0 || width < -3 || width > 4)
            {
                *reply = ztd::strdup("spacefm: invalid panel number\n");
                return 2;
            }
            focus_panel(nullptr, (void*)main_window, width);
        }
        else if (!strcmp(argv[i], "focused_pane"))
        {
            widget = nullptr;
            if (argv[i + 1])
            {
                if (!strcmp(argv[i + 1], "filelist"))
                    widget = file_browser->folder_view;
                else if (!strcmp(argv[i + 1], "devices"))
                    widget = file_browser->side_dev;
                else if (!strcmp(argv[i + 1], "bookmarks"))
                    widget = file_browser->side_book;
                else if (!strcmp(argv[i + 1], "dirtree"))
                    widget = file_browser->side_dir;
                else if (!strcmp(argv[i + 1], "pathbar"))
                    widget = file_browser->path_bar;
            }
            if (GTK_IS_WIDGET(widget))
                gtk_widget_grab_focus(widget);
        }
        else if (!strcmp(argv[i], "current_tab"))
        {
            width = 0;
            if (argv[i + 1])
            {
                if (!strcmp(argv[i + 1], "prev"))
                    width = -1;
                else if (!strcmp(argv[i + 1], "next"))
                    width = -2;
                else if (!strcmp(argv[i + 1], "close"))
                    width = -3;
                else
                    width = strtol(argv[i + 1], nullptr, 10);
            }
            if (width == 0 || width < -3 ||
                width > gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[panel - 1])))
            {
                *reply = ztd::strdup("spacefm: invalid tab number\n");
                return 2;
            }
            ptk_file_browser_go_tab(nullptr, file_browser, width);
        }
        else if (!strcmp(argv[i], "tab_count"))
        {
        }
        else if (!strcmp(argv[i], "new_tab"))
        {
            focus_panel(nullptr, (void*)main_window, panel);
            if (!(argv[i + 1] && std::filesystem::is_directory(argv[i + 1])))
                ptk_file_browser_new_tab(nullptr, file_browser);
            else
                fm_main_window_add_new_tab(main_window, argv[i + 1]);
            main_window_get_counts(file_browser, &i, &tab, &j);
            *reply = g_strdup_printf("#!%s\n%s\nnew_tab_window=%p\nnew_tab_panel=%d\n"
                                     "new_tab_number=%d\n",
                                     BASHPATH,
                                     SHELL_SETTINGS,
                                     main_window,
                                     panel,
                                     tab);
        }
        else if (g_str_has_suffix(argv[i], "_visible"))
        {
            bool use_mode = false;
            if (Glib::str_has_prefix(argv[i], "devices_"))
            {
                str = ztd::strdup("show_devmon");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "bookmarks_"))
            {
                str = ztd::strdup("show_book");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "dirtree_"))
            {
                str = ztd::strdup("show_dirtree");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "toolbar_"))
            {
                str = ztd::strdup("show_toolbox");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "sidetoolbar_"))
            {
                str = ztd::strdup("show_sidebar");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "hidden_files_"))
                str = ztd::strdup("show_hidden");
            else if (Glib::str_has_prefix(argv[i], "panel"))
            {
                j = argv[i][5] - 48;
                if (j < 1 || j > 4)
                {
                    *reply = g_strdup_printf("spacefm: invalid property %s\n", argv[i]);
                    return 2;
                }
                xset_set_b_panel(j, "show", get_bool(argv[i + 1]));
                show_panels_all_windows(nullptr, main_window);
                return 0;
            }
            else
                str = nullptr;
            if (!str)
                goto _invalid_set;
            if (use_mode)
                xset_set_b_panel_mode(panel,
                                      str,
                                      main_window->panel_context[panel - 1],
                                      get_bool(argv[i + 1]));
            else
                xset_set_b_panel(panel, str, get_bool(argv[i + 1]));
            update_views_all_windows(nullptr, file_browser);
        }
        else if (!strcmp(argv[i], "panel_hslider_top") ||
                 !strcmp(argv[i], "panel_hslider_bottom") || !strcmp(argv[i], "panel_vslider"))
        {
            width = -1;
            if (argv[i + 1])
                width = strtol(argv[i + 1], nullptr, 10);
            if (width < 0)
            {
                *reply = ztd::strdup("spacefm: invalid slider value\n");
                return 2;
            }
            if (!strcmp(argv[i] + 6, "hslider_top"))
                widget = file_browser->side_vpane_top;
            else if (!strcmp(argv[i] + 6, "hslider_bottom"))
                widget = file_browser->side_vpane_bottom;
            else
                widget = file_browser->hpane;
            gtk_paned_set_position(GTK_PANED(widget), width);
            ptk_file_browser_slider_release(nullptr, nullptr, file_browser);
            update_views_all_windows(nullptr, file_browser);
        }
        else if (!strcmp(argv[i], "column_width"))
        { // COLUMN WIDTH
            width = 0;
            if (argv[i + 1] && argv[i + 2])
                width = strtol(argv[i + 2], nullptr, 10);
            if (width < 1)
            {
                *reply = ztd::strdup("spacefm: invalid column width\n");
                return 2;
            }
            if (file_browser->view_mode == PTK_FB_LIST_VIEW)
            {
                GtkTreeViewColumn* col;
                for (j = 0; j < 6; j++)
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), j);
                    if (!col)
                        continue;
                    str = (char*)gtk_tree_view_column_get_title(col);
                    if (!g_strcmp0(argv[i + 1], str))
                        break;
                    if (!g_strcmp0(argv[i + 1], "name") && !g_strcmp0(str, column_titles[0]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "size") && !g_strcmp0(str, column_titles[1]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "type") && !g_strcmp0(str, column_titles[2]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "permission") &&
                             !g_strcmp0(str, column_titles[3]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "owner") && !g_strcmp0(str, column_titles[4]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "modified") &&
                             !g_strcmp0(str, column_titles[5]))
                        break;
                }
                if (j == 6)
                {
                    *reply = g_strdup_printf("spacefm: invalid column name '%s'\n", argv[i + 1]);
                    return 2;
                }
                gtk_tree_view_column_set_fixed_width(col, width);
            }
        }
        else if (!strcmp(argv[i], "sort_by"))
        { // COLUMN
            j = -10;
            if (!argv[i + 1])
            {
            }
            else if (!strcmp(argv[i + 1], "name"))
                j = PTK_FB_SORT_BY_NAME;
            else if (!strcmp(argv[i + 1], "size"))
                j = PTK_FB_SORT_BY_SIZE;
            else if (!strcmp(argv[i + 1], "type"))
                j = PTK_FB_SORT_BY_TYPE;
            else if (!strcmp(argv[i + 1], "permission"))
                j = PTK_FB_SORT_BY_PERM;
            else if (!strcmp(argv[i + 1], "owner"))
                j = PTK_FB_SORT_BY_OWNER;
            else if (!strcmp(argv[i + 1], "modified"))
                j = PTK_FB_SORT_BY_MTIME;

            if (j == -10)
            {
                *reply = g_strdup_printf("spacefm: invalid column name '%s'\n", argv[i + 1]);
                return 2;
            }
            ptk_file_browser_set_sort_order(file_browser, (PtkFBSortOrder)j);
        }
        else if (Glib::str_has_prefix(argv[i], "sort_"))
        {
            if (!strcmp(argv[i] + 5, "ascend"))
            {
                ptk_file_browser_set_sort_type(file_browser,
                                               get_bool(argv[i + 1]) ? GTK_SORT_ASCENDING
                                                                     : GTK_SORT_DESCENDING);
                return 0;
            }
            else if (!strcmp(argv[i] + 5, "alphanum"))
            {
                str = ztd::strdup("sortx_alphanum");
                xset_set_b(str, get_bool(argv[i + 1]));
            }
            else if (!strcmp(argv[i] + 5, "natural"))
            {
                str = ztd::strdup("sortx_natural");
                xset_set_b(str, get_bool(argv[i + 1]));
            }
            else if (!strcmp(argv[i] + 5, "case"))
            {
                str = ztd::strdup("sortx_case");
                xset_set_b(str, get_bool(argv[i + 1]));
            }
            else if (!strcmp(argv[i] + 5, "hidden_first"))
            {
                if (get_bool(argv[i + 1]))
                    str = ztd::strdup("sortx_hidfirst");
                else
                    str = ztd::strdup("sortx_hidlast");
                xset_set_b(str, true);
            }
            else if (!strcmp(argv[i] + 5, "first"))
            {
                if (!g_strcmp0(argv[i + 1], "files"))
                    str = ztd::strdup("sortx_files");
                else if (!g_strcmp0(argv[i + 1], "directories"))
                    str = ztd::strdup("sortx_directories");
                else if (!g_strcmp0(argv[i + 1], "mixed"))
                    str = ztd::strdup("sortx_mix");
                else
                {
                    *reply = g_strdup_printf("spacefm: invalid %s value\n", argv[i]);
                    return 2;
                }
            }
            else
                goto _invalid_set;
            ptk_file_browser_set_sort_extra(file_browser, str);
        }
        else if (!strcmp(argv[i], "show_thumbnails"))
        {
            if (app_settings.show_thumbnail != get_bool(argv[i + 1]))
                main_window_toggle_thumbnails_all_windows();
        }
        else if (!strcmp(argv[i], "large_icons"))
        {
            if (file_browser->view_mode != PTK_FB_ICON_VIEW)
            {
                xset_set_b_panel_mode(panel,
                                      "list_large",
                                      main_window->panel_context[panel - 1],
                                      get_bool(argv[i + 1]));
                update_views_all_windows(nullptr, file_browser);
            }
        }
        else if (!strcmp(argv[i], "statusbar_text"))
        {
            if (!(argv[i + 1] && argv[i + 1][0]))
            {
                free(file_browser->status_bar_custom);
                file_browser->status_bar_custom = nullptr;
            }
            else
            {
                free(file_browser->status_bar_custom);
                file_browser->status_bar_custom = ztd::strdup(argv[i + 1]);
            }
            fm_main_window_update_status_bar(main_window, file_browser);
        }
        else if (!strcmp(argv[i], "pathbar_text"))
        { // TEXT [[SELSTART] SELEND]
            if (!GTK_IS_WIDGET(file_browser->path_bar))
                return 0;
            if (!(argv[i + 1] && argv[i + 1][0]))
            {
                gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), "");
            }
            else
            {
                gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), argv[i + 1]);
                if (!argv[i + 2])
                {
                    width = 0;
                    height = -1;
                }
                else
                {
                    width = strtol(argv[i + 2], nullptr, 10);
                    height = argv[i + 3] ? strtol(argv[i + 3], nullptr, 10) : -1;
                }
                gtk_editable_set_position(GTK_EDITABLE(file_browser->path_bar), -1);
                gtk_editable_select_region(GTK_EDITABLE(file_browser->path_bar), width, height);
                gtk_widget_grab_focus(file_browser->path_bar);
            }
        }
        else if (!strcmp(argv[i], "clipboard_text") || !strcmp(argv[i], "clipboard_primary_text"))
        {
            if (argv[i + 1] && !g_utf8_validate(argv[i + 1], -1, nullptr))
            {
                *reply = ztd::strdup("spacefm: text is not valid UTF-8\n");
                return 2;
            }
            GtkClipboard* clip =
                gtk_clipboard_get(!strcmp(argv[i], "clipboard_text") ? GDK_SELECTION_CLIPBOARD
                                                                     : GDK_SELECTION_PRIMARY);
            str = unescape(argv[i + 1] ? argv[i + 1] : "");
            gtk_clipboard_set_text(clip, str, -1);
            free(str);
        }
        else if (!strcmp(argv[i], "clipboard_from_file") ||
                 !strcmp(argv[i], "clipboard_primary_from_file"))
        {
            if (!argv[i + 1])
            {
                *reply = g_strdup_printf("spacefm: %s requires a file path\n", argv[i]);
                return 1;
            }
            if (!g_file_get_contents(argv[i + 1], &str, nullptr, nullptr))
            {
                *reply = g_strdup_printf("spacefm: error reading file '%s'\n", argv[i + 1]);
                return 2;
            }
            if (!g_utf8_validate(str, -1, nullptr))
            {
                *reply = g_strdup_printf("spacefm: file '%s' does not contain valid UTF-8 text\n",
                                         argv[i + 1]);
                free(str);
                return 2;
            }
            GtkClipboard* clip =
                gtk_clipboard_get(!strcmp(argv[i], "clipboard_from_file") ? GDK_SELECTION_CLIPBOARD
                                                                          : GDK_SELECTION_PRIMARY);
            gtk_clipboard_set_text(clip, str, -1);
            free(str);
        }
        else if (!strcmp(argv[i], "clipboard_cut_files") ||
                 !strcmp(argv[i], "clipboard_copy_files"))
        {
            ptk_clipboard_copy_file_list(argv + i + 1, !strcmp(argv[i], "clipboard_copy_files"));
        }
        else if (!strcmp(argv[i], "selected_filenames") || !strcmp(argv[i], "selected_files"))
        {
            if (!argv[i + 1] || argv[i + 1][0] == '\0')
                // unselect all
                ptk_file_browser_select_file_list(file_browser, nullptr, false);
            else
                ptk_file_browser_select_file_list(file_browser, argv + i + 1, true);
        }
        else if (!strcmp(argv[i], "selected_pattern"))
        {
            if (!argv[i + 1])
                // unselect all
                ptk_file_browser_select_file_list(file_browser, nullptr, false);
            else
                ptk_file_browser_select_pattern(nullptr, file_browser, argv[i + 1]);
        }
        else if (!strcmp(argv[i], "current_dir"))
        {
            if (!argv[i + 1])
            {
                *reply = g_strdup_printf("spacefm: %s requires a directory path\n", argv[i]);
                return 1;
            }
            if (!std::filesystem::is_directory(argv[i + 1]))
            {
                *reply = g_strdup_printf("spacefm: directory '%s' does not exist\n", argv[i + 1]);
                return 1;
            }
            ptk_file_browser_chdir(file_browser, argv[i + 1], PTK_FB_CHDIR_ADD_HISTORY);
        }
        else
        {
        _invalid_set:
            *reply = g_strdup_printf("spacefm: invalid property %s\n", argv[i]);
            return 1;
        }
    }
    else if (!strcmp(argv[0], "get"))
    {
        // get
        if (!argv[i])
        {
            *reply = g_strdup_printf("spacefm: command %s requires an argument\n", argv[0]);
            return 1;
        }
        if (!strcmp(argv[i], "window_size") || !strcmp(argv[i], "window_position"))
        {
            if (!strcmp(argv[i], "window_size"))
                gtk_window_get_size(GTK_WINDOW(main_window), &width, &height);
            else
                gtk_window_get_position(GTK_WINDOW(main_window), &width, &height);
            *reply = g_strdup_printf("%dx%d\n", width, height);
        }
        else if (!strcmp(argv[i], "window_maximized"))
        {
            *reply = g_strdup_printf("%d\n", !!main_window->maximized);
        }
        else if (!strcmp(argv[i], "window_fullscreen"))
        {
            *reply = g_strdup_printf("%d\n", !!main_window->fullscreen);
        }
        else if (!strcmp(argv[i], "screen_size"))
        {
            GdkRectangle workarea = GdkRectangle();
            gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()),
                                     &workarea);
            *reply = g_strdup_printf("%dx%d\n", workarea.width, workarea.height);
        }
        else if (!strcmp(argv[i], "window_vslider_top") ||
                 !strcmp(argv[i], "window_vslider_bottom") || !strcmp(argv[i], "window_hslider") ||
                 !strcmp(argv[i], "window_tslider"))
        {
            if (!strcmp(argv[i] + 7, "vslider_top"))
                widget = main_window->hpane_top;
            else if (!strcmp(argv[i] + 7, "vslider_bottom"))
                widget = main_window->hpane_bottom;
            else if (!strcmp(argv[i] + 7, "hslider"))
                widget = main_window->vpane;
            else
                widget = main_window->task_vpane;
            *reply = g_strdup_printf("%d\n", gtk_paned_get_position(GTK_PANED(widget)));
        }
        else if (!strcmp(argv[i], "focused_panel"))
        {
            *reply = g_strdup_printf("%d\n", main_window->curpanel);
        }
        else if (!strcmp(argv[i], "focused_pane"))
        {
            if (file_browser->folder_view && gtk_widget_is_focus(file_browser->folder_view))
                str = ztd::strdup("filelist");
            else if (file_browser->side_dev && gtk_widget_is_focus(file_browser->side_dev))
                str = ztd::strdup("devices");
            else if (file_browser->side_book && gtk_widget_is_focus(file_browser->side_book))
                str = ztd::strdup("bookmarks");
            else if (file_browser->side_dir && gtk_widget_is_focus(file_browser->side_dir))
                str = ztd::strdup("dirtree");
            else if (file_browser->path_bar && gtk_widget_is_focus(file_browser->path_bar))
                str = ztd::strdup("pathbar");
            else
                str = nullptr;
            if (str)
                *reply = g_strdup_printf("%s\n", str);
        }
        else if (!strcmp(argv[i], "current_tab"))
        {
            *reply =
                g_strdup_printf("%d\n",
                                gtk_notebook_page_num(GTK_NOTEBOOK(main_window->panel[panel - 1]),
                                                      GTK_WIDGET(file_browser)) +
                                    1);
        }
        else if (!strcmp(argv[i], "tab_count"))
        {
            main_window_get_counts(file_browser, &panel, &tab, &j);
            *reply = g_strdup_printf("%d\n", tab);
        }
        else if (!strcmp(argv[i], "new_tab"))
        {
        }
        else if (g_str_has_suffix(argv[i], "_visible"))
        {
            bool use_mode = false;
            if (Glib::str_has_prefix(argv[i], "devices_"))
            {
                str = ztd::strdup("show_devmon");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "bookmarks_"))
            {
                str = ztd::strdup("show_book");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "dirtree_"))
            {
                str = ztd::strdup("show_dirtree");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "toolbar_"))
            {
                str = ztd::strdup("show_toolbox");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "sidetoolbar_"))
            {
                str = ztd::strdup("show_sidebar");
                use_mode = true;
            }
            else if (Glib::str_has_prefix(argv[i], "hidden_files_"))
                str = ztd::strdup("show_hidden");
            else if (Glib::str_has_prefix(argv[i], "panel"))
            {
                j = argv[i][5] - 48;
                if (j < 1 || j > 4)
                {
                    *reply = g_strdup_printf("spacefm: invalid property %s\n", argv[i]);
                    return 2;
                }
                *reply = g_strdup_printf("%d\n", xset_get_b_panel(j, "show"));
                return 0;
            }
            else
                str = nullptr;
            if (!str)
                goto _invalid_get;
            if (use_mode)
                *reply = g_strdup_printf(
                    "%d\n",
                    !!xset_get_b_panel_mode(panel, str, main_window->panel_context[panel - 1]));
            else
                *reply = g_strdup_printf("%d\n", !!xset_get_b_panel(panel, str));
        }
        else if (!strcmp(argv[i], "panel_hslider_top") ||
                 !strcmp(argv[i], "panel_hslider_bottom") || !strcmp(argv[i], "panel_vslider"))
        {
            if (!strcmp(argv[i] + 6, "hslider_top"))
                widget = file_browser->side_vpane_top;
            else if (!strcmp(argv[i] + 6, "hslider_bottom"))
                widget = file_browser->side_vpane_bottom;
            else
                widget = file_browser->hpane;
            *reply = g_strdup_printf("%d\n", gtk_paned_get_position(GTK_PANED(widget)));
        }
        else if (!strcmp(argv[i], "column_width"))
        { // COLUMN
            if (file_browser->view_mode == PTK_FB_LIST_VIEW)
            {
                GtkTreeViewColumn* col;
                for (j = 0; j < 6; j++)
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), j);
                    if (!col)
                        continue;
                    str = (char*)gtk_tree_view_column_get_title(col);
                    if (!g_strcmp0(argv[i + 1], str))
                        break;
                    if (!g_strcmp0(argv[i + 1], "name") && !g_strcmp0(str, column_titles[0]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "size") && !g_strcmp0(str, column_titles[1]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "type") && !g_strcmp0(str, column_titles[2]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "permission") &&
                             !g_strcmp0(str, column_titles[3]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "owner") && !g_strcmp0(str, column_titles[4]))
                        break;
                    else if (!g_strcmp0(argv[i + 1], "modified") &&
                             !g_strcmp0(str, column_titles[5]))
                        break;
                }
                if (j == 6)
                {
                    *reply = g_strdup_printf("spacefm: invalid column name '%s'\n", argv[i + 1]);
                    return 2;
                }
                *reply = g_strdup_printf("%d\n", gtk_tree_view_column_get_width(col));
            }
        }
        else if (!strcmp(argv[i], "sort_by"))
        { // COLUMN
            switch (file_browser->sort_order)
            {
                case PTK_FB_SORT_BY_NAME:
                    str = ztd::strdup("name");
                    break;
                case PTK_FB_SORT_BY_SIZE:
                    str = ztd::strdup("size");
                    break;
                case PTK_FB_SORT_BY_TYPE:
                    str = ztd::strdup("type");
                    break;
                case PTK_FB_SORT_BY_PERM:
                    str = ztd::strdup("permission");
                    break;
                case PTK_FB_SORT_BY_OWNER:
                    str = ztd::strdup("owner");
                    break;
                case PTK_FB_SORT_BY_MTIME:
                    str = ztd::strdup("modified");
                    break;
                default:
                    return 0;
            }
            *reply = g_strdup_printf("%s\n", str);
        }
        else if (Glib::str_has_prefix(argv[i], "sort_"))
        {
            if (!strcmp(argv[i] + 5, "ascend"))
                *reply =
                    g_strdup_printf("%d\n", file_browser->sort_type == GTK_SORT_ASCENDING ? 1 : 0);
#if 0
            else if (!strcmp(argv[i] + 5, "natural"))
#endif
            else if (!strcmp(argv[i] + 5, "alphanum"))
                *reply =
                    g_strdup_printf("%d\n",
                                    xset_get_b_panel(file_browser->mypanel, "sort_extra") ? 1 : 0);
            else if (!strcmp(argv[i] + 5, "case"))
                *reply = g_strdup_printf("%d\n",
                                         xset_get_b_panel(file_browser->mypanel, "sort_extra") &&
                                                 xset_get_int_panel(file_browser->mypanel,
                                                                    "sort_extra",
                                                                    "x") == XSET_B_TRUE
                                             ? 1
                                             : 0);
            else if (!strcmp(argv[i] + 5, "hidden_first"))
                *reply = g_strdup_printf(
                    "%d\n",
                    xset_get_int_panel(file_browser->mypanel, "sort_extra", "z") == XSET_B_TRUE
                        ? 1
                        : 0);
            else if (!strcmp(argv[i] + 5, "first"))
            {
                switch (xset_get_int_panel(file_browser->mypanel, "sort_extra", "y"))
                {
                    case 0:
                        str = ztd::strdup("mixed");
                        break;
                    case 1:
                        str = ztd::strdup("directories");
                        break;
                    case 2:
                        str = ztd::strdup("files");
                        break;
                    default:
                        return 0; // failsafe for known
                }
                *reply = g_strdup_printf("%s\n", str);
            }
            else
                goto _invalid_get;
        }
        else if (!strcmp(argv[i], "show_thumbnails"))
        {
            *reply = g_strdup_printf("%d\n", app_settings.show_thumbnail ? 1 : 0);
        }
        else if (!strcmp(argv[i], "large_icons"))
        {
            *reply = g_strdup_printf("%d\n", file_browser->large_icons ? 1 : 0);
        }
        else if (!strcmp(argv[i], "statusbar_text"))
        {
            *reply =
                g_strdup_printf("%s\n", gtk_label_get_text(GTK_LABEL(file_browser->status_label)));
        }
        else if (!strcmp(argv[i], "pathbar_text"))
        {
            if (GTK_IS_WIDGET(file_browser->path_bar))
                *reply =
                    g_strdup_printf("%s\n", gtk_entry_get_text(GTK_ENTRY(file_browser->path_bar)));
        }
        else if (!strcmp(argv[i], "clipboard_text") || !strcmp(argv[i], "clipboard_primary_text"))
        {
            GtkClipboard* clip =
                gtk_clipboard_get(!strcmp(argv[i], "clipboard_text") ? GDK_SELECTION_CLIPBOARD
                                                                     : GDK_SELECTION_PRIMARY);
            *reply = gtk_clipboard_wait_for_text(clip);
        }
        else if (!strcmp(argv[i], "clipboard_from_file") ||
                 !strcmp(argv[i], "clipboard_primary_from_file"))
        {
        }
        else if (!strcmp(argv[i], "clipboard_cut_files") ||
                 !strcmp(argv[i], "clipboard_copy_files"))
        {
            GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            GdkAtom gnome_target;
            GdkAtom uri_list_target;
            GtkSelectionData* sel_data;

            gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);
            sel_data = gtk_clipboard_wait_for_contents(clip, gnome_target);
            if (!sel_data)
            {
                uri_list_target = gdk_atom_intern("text/uri-list", false);
                sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
                if (!sel_data)
                    return 0;
            }
            if (gtk_selection_data_get_length(sel_data) <= 0 ||
                gtk_selection_data_get_format(sel_data) != 8)
            {
                gtk_selection_data_free(sel_data);
                return 0;
            }
            if (!strncmp((char*)gtk_selection_data_get_data(sel_data), "cut", 3))
            {
                if (!strcmp(argv[i], "clipboard_copy_files"))
                {
                    gtk_selection_data_free(sel_data);
                    return 0;
                }
            }
            else if (!strcmp(argv[i], "clipboard_cut_files"))
            {
                gtk_selection_data_free(sel_data);
                return 0;
            }
            str = gtk_clipboard_wait_for_text(clip);
            gtk_selection_data_free(sel_data);
            if (!(str && str[0]))
            {
                free(str);
                return 0;
            }
            // build bash array
            char** pathv = g_strsplit(str, "\n", 0);
            free(str);
            std::string str2 = "(";
            j = 0;
            while (pathv[j])
            {
                if (pathv[j][0])
                {
                    std::string str3 = bash_quote(pathv[j]);
                    str2.append(fmt::format("{} ", str2));
                }
                j++;
            }
            g_strfreev(pathv);
            str2.append(")\n");

            *reply = (char*)str2.c_str();
        }
        else if (!strcmp(argv[i], "selected_filenames") || !strcmp(argv[i], "selected_files"))
        {
            GList* sel_files;

            sel_files = ptk_file_browser_get_selected_files(file_browser);
            if (!sel_files)
                return 0;

            // build bash array
            std::string str2 = "(";
            for (l = sel_files; l; l = l->next)
            {
                VFSFileInfo* file = vfs_file_info_ref(static_cast<VFSFileInfo*>(l->data));
                if (file)
                {
                    std::string str3 = bash_quote(vfs_file_info_get_name(file));
                    str2.append(fmt::format("{} ", str3));
                    vfs_file_info_unref(file);
                }
            }
            vfs_file_info_list_free(sel_files);
            str2.append(")\n");
            *reply = (char*)str2.c_str();
        }
        else if (!strcmp(argv[i], "selected_pattern"))
        {
        }
        else if (!strcmp(argv[i], "current_dir"))
        {
            *reply = g_strdup_printf("%s\n", ptk_file_browser_get_cwd(file_browser));
        }
        else
        {
        _invalid_get:
            *reply = g_strdup_printf("spacefm: invalid property %s\n", argv[i]);
            return 1;
        }
    }
    else if (!strcmp(argv[0], "set-task"))
    { // TASKNUM PROPERTY [VALUE]
        if (!(argv[i] && argv[i + 1]))
        {
            *reply = g_strdup_printf("spacefm: %s requires two arguments\n", argv[0]);
            return 1;
        }

        // find task
        GtkTreeIter it;
        PtkFileTask* ptask = nullptr;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, TASK_COL_DATA, &ptask, -1);
                str = g_strdup_printf("%p", ptask);
                if (!strcmp(str, argv[i]))
                {
                    free(str);
                    break;
                }
                free(str);
                ptask = nullptr;
            } while (gtk_tree_model_iter_next(model, &it));
        }
        if (!ptask)
        {
            *reply = g_strdup_printf("spacefm: invalid task '%s'\n", argv[i]);
            return 2;
        }
        if (ptask->task->type != VFS_FILE_TASK_EXEC)
        {
            *reply = g_strdup_printf("spacefm: internal task %s is read-only\n", argv[i]);
            return 2;
        }

        // set model value
        if (!strcmp(argv[i + 1], "icon"))
        {
            ptk_file_task_lock(ptask);
            ptask->task->exec_icon = argv[i + 2];
            ptask->pause_change_view = ptask->pause_change = true;
            ptk_file_task_unlock(ptask);
            return 0;
        }
        else if (!strcmp(argv[i + 1], "count"))
            j = TASK_COL_COUNT;
        else if (!strcmp(argv[i + 1], "directory") || !strcmp(argv[i + 1], "from"))
            j = TASK_COL_PATH;
        else if (!strcmp(argv[i + 1], "item"))
            j = TASK_COL_FILE;
        else if (!strcmp(argv[i + 1], "to"))
            j = TASK_COL_TO;
        else if (!strcmp(argv[i + 1], "progress"))
        {
            if (!argv[i + 2])
                ptask->task->percent = 50;
            else
            {
                j = strtol(argv[i + 2], nullptr, 10);
                if (j < 0)
                    j = 0;
                if (j > 100)
                    j = 100;
                ptask->task->percent = j;
            }
            ptask->task->custom_percent = !!argv[i + 2];
            ptask->pause_change_view = ptask->pause_change = true;
            return 0;
        }
        else if (!strcmp(argv[i + 1], "total"))
            j = TASK_COL_TOTAL;
        else if (!strcmp(argv[i + 1], "curspeed"))
            j = TASK_COL_CURSPEED;
        else if (!strcmp(argv[i + 1], "curremain"))
            j = TASK_COL_CUREST;
        else if (!strcmp(argv[i + 1], "avgspeed"))
            j = TASK_COL_AVGSPEED;
        else if (!strcmp(argv[i + 1], "avgremain"))
            j = TASK_COL_AVGEST;
        else if (!strcmp(argv[i + 1], "elapsed") || !strcmp(argv[i + 1], "started") ||
                 !strcmp(argv[i + 1], "status"))
        {
            *reply = g_strdup_printf("spacefm: task property '%s' is read-only\n", argv[i + 1]);
            return 2;
        }
        else if (!strcmp(argv[i + 1], "queue_state"))
        {
            if (!argv[i + 2] || !g_strcmp0(argv[i + 2], "run"))
                ptk_file_task_pause(ptask, VFS_FILE_TASK_RUNNING);
            else if (!strcmp(argv[i + 2], "pause"))
                ptk_file_task_pause(ptask, VFS_FILE_TASK_PAUSE);
            else if (!strcmp(argv[i + 2], "queue") || !strcmp(argv[i + 2], "queued"))
                ptk_file_task_pause(ptask, VFS_FILE_TASK_QUEUE);
            else if (!strcmp(argv[i + 2], "stop"))
                on_task_stop(nullptr, main_window->task_view, xset_get("task_stop_all"), nullptr);
            else
            {
                *reply = g_strdup_printf("spacefm: invalid queue_state '%s'\n", argv[i + 2]);
                return 2;
            }
            main_task_start_queued(main_window->task_view, nullptr);
            return 0;
        }
        else if (!strcmp(argv[i + 1], "popup_handler"))
        {
            free(ptask->pop_handler);
            if (argv[i + 2] && argv[i + 2][0] != '\0')
                ptask->pop_handler = ztd::strdup(argv[i + 2]);
            else
                ptask->pop_handler = nullptr;
            return 0;
        }
        else
        {
            *reply = g_strdup_printf("spacefm: invalid task property '%s'\n", argv[i + 1]);
            return 2;
        }
        gtk_list_store_set(GTK_LIST_STORE(model), &it, j, argv[i + 2], -1);
    }
    else if (!strcmp(argv[0], "get-task"))
    { // TASKNUM PROPERTY
        if (!(argv[i] && argv[i + 1]))
        {
            *reply = g_strdup_printf("spacefm: %s requires two arguments\n", argv[0]);
            return 1;
        }

        // find task
        GtkTreeIter it;
        PtkFileTask* ptask = nullptr;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, TASK_COL_DATA, &ptask, -1);
                str = g_strdup_printf("%p", ptask);
                if (!strcmp(str, argv[i]))
                {
                    free(str);
                    break;
                }
                free(str);
                ptask = nullptr;
            } while (gtk_tree_model_iter_next(model, &it));
        }
        if (!ptask)
        {
            *reply = g_strdup_printf("spacefm: invalid task '%s'\n", argv[i]);
            return 2;
        }

        // get model value
        if (!strcmp(argv[i + 1], "icon"))
        {
            ptk_file_task_lock(ptask);
            if (!ptask->task->exec_icon.empty())
                *reply = g_strdup_printf("%s\n", ptask->task->exec_icon.c_str());
            ptk_file_task_unlock(ptask);
            return 0;
        }
        else if (!strcmp(argv[i + 1], "count"))
            j = TASK_COL_COUNT;
        else if (!strcmp(argv[i + 1], "directory") || !strcmp(argv[i + 1], "from"))
            j = TASK_COL_PATH;
        else if (!strcmp(argv[i + 1], "item"))
            j = TASK_COL_FILE;
        else if (!strcmp(argv[i + 1], "to"))
            j = TASK_COL_TO;
        else if (!strcmp(argv[i + 1], "progress"))
        {
            *reply = g_strdup_printf("%d\n", ptask->task->percent);
            return 0;
        }
        else if (!strcmp(argv[i + 1], "total"))
            j = TASK_COL_TOTAL;
        else if (!strcmp(argv[i + 1], "curspeed"))
            j = TASK_COL_CURSPEED;
        else if (!strcmp(argv[i + 1], "curremain"))
            j = TASK_COL_CUREST;
        else if (!strcmp(argv[i + 1], "avgspeed"))
            j = TASK_COL_AVGSPEED;
        else if (!strcmp(argv[i + 1], "avgremain"))
            j = TASK_COL_AVGEST;
        else if (!strcmp(argv[i + 1], "elapsed"))
            j = TASK_COL_ELAPSED;
        else if (!strcmp(argv[i + 1], "started"))
            j = TASK_COL_STARTED;
        else if (!strcmp(argv[i + 1], "status"))
            j = TASK_COL_STATUS;
        else if (!strcmp(argv[i + 1], "queue_state"))
        {
            if (ptask->task->state_pause == VFS_FILE_TASK_RUNNING)
                str = ztd::strdup("run");
            else if (ptask->task->state_pause == VFS_FILE_TASK_PAUSE)
                str = ztd::strdup("pause");
            else if (ptask->task->state_pause == VFS_FILE_TASK_QUEUE)
                str = ztd::strdup("queue");
            else
                str = ztd::strdup("stop"); // failsafe
            *reply = g_strdup_printf("%s\n", str);
            return 0;
        }
        else if (!strcmp(argv[i + 1], "popup_handler"))
        {
            if (ptask->pop_handler)
                *reply = g_strdup_printf("%s\n", ptask->pop_handler);
            return 0;
        }
        else
        {
            *reply = g_strdup_printf("spacefm: invalid task property '%s'\n", argv[i + 1]);
            return 2;
        }
        gtk_tree_model_get(model, &it, j, &str, -1);
        if (str)
            *reply = g_strdup_printf("%s\n", str);
        free(str);
    }
    else if (!strcmp(argv[0], "run-task"))
    { // TYPE [OPTIONS] ...
        if (!(argv[i] && argv[i + 1]))
        {
            *reply = g_strdup_printf("spacefm: %s requires two arguments\n", argv[0]);
            return 1;
        }
        if (!strcmp(argv[i], "cmd") || !strcmp(argv[i], "command"))
        {
            // custom command task
            // cmd [--task [--popup] [--scroll]] [--terminal]
            //                     [--user USER] [--title TITLE]
            //                     [--icon ICON] [--dir DIR] COMMAND
            // get opts
            bool opt_task = false;
            bool opt_popup = false;
            bool opt_scroll = false;
            bool opt_terminal = false;
            const char* opt_user = nullptr;
            const char* opt_title = nullptr;
            const char* opt_icon = nullptr;
            const char* opt_cwd = nullptr;
            for (j = i + 1; argv[j] && argv[j][0] == '-'; j++)
            {
                if (!strcmp(argv[j], "--task"))
                    opt_task = true;
                else if (!strcmp(argv[j], "--popup"))
                    opt_popup = opt_task = true;
                else if (!strcmp(argv[j], "--scroll"))
                    opt_scroll = opt_task = true;
                else if (!strcmp(argv[j], "--terminal"))
                    opt_terminal = true;
                /* disabled due to potential misuse of password caching su programs
                else if ( !strcmp( argv[j], "--user" ) )
                    opt_user = argv[++j];
                */
                else if (!strcmp(argv[j], "--title"))
                    opt_title = argv[++j];
                else if (!strcmp(argv[j], "--icon"))
                    opt_icon = argv[++j];
                else if (!strcmp(argv[j], "--dir"))
                {
                    opt_cwd = argv[++j];
                    if (!(opt_cwd && opt_cwd[0] == '/' && std::filesystem::is_directory(opt_cwd)))
                    {
                        *reply = g_strdup_printf("spacefm: no such directory '%s'\n", opt_cwd);
                        return 2;
                    }
                }
                else
                {
                    *reply =
                        g_strdup_printf("spacefm: invalid %s task option '%s'\n", argv[i], argv[j]);
                    return 2;
                }
            }
            if (!argv[j])
            {
                *reply = g_strdup_printf("spacefm: %s requires two arguments\n", argv[0]);
                return 1;
            }
            std::string cmd = "";
            while (argv[++j])
                cmd.append(fmt::format(" {}", argv[j]));

            PtkFileTask* ptask =
                ptk_file_exec_new(opt_title ? opt_title : cmd.c_str(),
                                  opt_cwd ? opt_cwd : ptk_file_browser_get_cwd(file_browser),
                                  GTK_WIDGET(file_browser),
                                  file_browser->task_view);
            ptask->task->exec_browser = file_browser;
            ptask->task->exec_command = cmd;
            ptask->task->exec_as_user = opt_user;
            ptask->task->exec_icon = opt_icon;
            ptask->task->exec_terminal = opt_terminal;
            ptask->task->exec_keep_terminal = false;
            ptask->task->exec_sync = opt_task;
            ptask->task->exec_popup = opt_popup;
            ptask->task->exec_show_output = opt_popup;
            ptask->task->exec_show_error = true;
            ptask->task->exec_scroll_lock = !opt_scroll;
            ptask->task->exec_export = true;
            if (opt_popup)
                gtk_window_present(GTK_WINDOW(main_window));
            ptk_file_task_run(ptask);
            if (opt_task)
                *reply =
                    g_strdup_printf("#!%s\n%s\n# Note: $new_task_id not valid until approx one "
                                    "half second after task start\nnew_task_window=%p\n"
                                    "new_task_id=%p\n",
                                    BASHPATH,
                                    SHELL_SETTINGS,
                                    main_window,
                                    ptask);
        }
        else if (!strcmp(argv[i], "edit") || !strcmp(argv[i], "web"))
        {
            // edit or web
            // edit [--as-root] FILE
            // web URL
            bool opt_root = false;
            for (j = i + 1; argv[j] && argv[j][0] == '-'; j++)
            {
                if (!strcmp(argv[i], "edit") && !strcmp(argv[j], "--as-root"))
                    opt_root = true;
                else
                {
                    *reply =
                        g_strdup_printf("spacefm: invalid %s task option '%s'\n", argv[i], argv[j]);
                    return 2;
                }
            }
            if (!argv[j])
            {
                *reply = g_strdup_printf("spacefm: %s requires two arguments\n", argv[0]);
                return 1;
            }
            if (!strcmp(argv[i], "edit"))
            {
                if (!(argv[j][0] == '/' && std::filesystem::exists(argv[j])))
                {
                    *reply = g_strdup_printf("spacefm: no such file '%s'\n", argv[j]);
                    return 2;
                }
                xset_edit(GTK_WIDGET(file_browser), argv[j], opt_root, !opt_root);
            }
        }
        else if (!strcmp(argv[i], "mount") || !strcmp(argv[i], "unmount"))
        {
            // mount or unmount TARGET
            for (j = i + 1; argv[j] && argv[j][0] == '-'; j++)
            {
                *reply =
                    g_strdup_printf("spacefm: invalid %s task option '%s'\n", argv[i], argv[j]);
                return 2;
            }
            if (!argv[j])
            {
                *reply =
                    g_strdup_printf("spacefm: task type %s requires TARGET argument\n", argv[0]);
                return 1;
            }

            // Resolve TARGET
            struct stat statbuf;
            char* real_path = argv[j];
            char* device_file = nullptr;
            VFSVolume* vol = nullptr;
            netmount_t* netmount = nullptr;
            if (!strcmp(argv[i], "unmount") && std::filesystem::is_directory(real_path))
            {
                // unmount DIR
                if (path_is_mounted_mtab(nullptr, real_path, &device_file, nullptr) && device_file)
                {
                    if (!(stat(device_file, &statbuf) == 0 && S_ISBLK(statbuf.st_mode)))
                    {
                        // NON-block device - try to find vol by mount point
                        if (!(vol = vfs_volume_get_by_device_or_point(device_file, real_path)))
                        {
                            *reply = g_strdup_printf("spacefm: invalid TARGET '%s'\n", argv[j]);
                            return 2;
                        }
                        free(device_file);
                        device_file = nullptr;
                    }
                }
            }
            else if (stat(real_path, &statbuf) == 0 && S_ISBLK(statbuf.st_mode))
            {
                // block device eg /dev/sda1
                device_file = ztd::strdup(real_path);
            }
            else if (!strcmp(argv[i], "mount") &&
                     ((real_path[0] != '/' && strstr(real_path, ":/")) ||
                      Glib::str_has_prefix(real_path, "//")))
            {
                // mount URL
                if (split_network_url(real_path, &netmount) != 1)
                {
                    // not a valid url
                    *reply = g_strdup_printf("spacefm: invalid TARGET '%s'\n", argv[j]);
                    return 2;
                }
            }

            if (device_file)
            {
                // block device - get vol
                vol = vfs_volume_get_by_device_or_point(device_file, nullptr);
                free(device_file);
                device_file = nullptr;
            }

            // Create command
            bool run_in_terminal = false;
            std::string cmd;
            if (vol)
            {
                // mount/unmount vol
                if (!strcmp(argv[i], "mount"))
                    cmd = vfs_volume_get_mount_command(vol,
                                                       xset_get_s("dev_mount_options"),
                                                       &run_in_terminal);
                else
                    cmd = vfs_volume_device_unmount_cmd(vol, &run_in_terminal);
            }
            else if (netmount)
            {
                // URL mount only
                cmd = vfs_volume_handler_cmd(HANDLER_MODE_NET,
                                             HANDLER_MOUNT,
                                             nullptr,
                                             nullptr,
                                             netmount,
                                             &run_in_terminal,
                                             nullptr);
                free(netmount->url);
                free(netmount->fstype);
                free(netmount->host);
                free(netmount->ip);
                free(netmount->port);
                free(netmount->user);
                free(netmount->pass);
                free(netmount->path);
                g_slice_free(netmount_t, netmount);
            }
            if (cmd.empty())
            {
                *reply = g_strdup_printf("spacefm: invalid TARGET '%s'\n", argv[j]);
                return 2;
            }
            // Task
            PtkFileTask* ptask = ptk_file_exec_new(argv[i],
                                                   ptk_file_browser_get_cwd(file_browser),
                                                   GTK_WIDGET(file_browser),
                                                   file_browser->task_view);
            ptask->task->exec_browser = file_browser;
            ptask->task->exec_command = cmd;
            ptask->task->exec_terminal = run_in_terminal;
            ptask->task->exec_keep_terminal = false;
            ptask->task->exec_sync = true;
            ptask->task->exec_export = false;
            ptask->task->exec_show_error = true;
            ptask->task->exec_scroll_lock = false;
            ptk_file_task_run(ptask);
        }
        else if (!strcmp(argv[i], "copy") || !strcmp(argv[i], "move") || !strcmp(argv[i], "link") ||
                 !strcmp(argv[i], "delete") || !strcmp(argv[i], "trash"))
        {
            // built-in task
            // copy SOURCE FILENAME [...] TARGET
            // move SOURCE FILENAME [...] TARGET
            // link SOURCE FILENAME [...] TARGET
            // delete SOURCE FILENAME [...]
            // get opts
            const char* opt_cwd = nullptr;
            for (j = i + 1; argv[j] && argv[j][0] == '-'; j++)
            {
                if (!strcmp(argv[j], "--dir"))
                {
                    opt_cwd = argv[++j];
                    if (!(opt_cwd && opt_cwd[0] == '/' && std::filesystem::is_directory(opt_cwd)))
                    {
                        *reply = g_strdup_printf("spacefm: no such directory '%s'\n", opt_cwd);
                        return 2;
                    }
                }
                else
                {
                    *reply =
                        g_strdup_printf("spacefm: invalid %s task option '%s'\n", argv[i], argv[j]);
                    return 2;
                }
            }
            l = nullptr; // file list
            char* target_dir = nullptr;
            for (; argv[j]; j++)
            {
                if (strcmp(argv[i], "delete") && !argv[j + 1])
                {
                    // last argument - use as TARGET
                    if (argv[j][0] != '/')
                    {
                        *reply = g_strdup_printf("spacefm: no such directory '%s'\n", argv[j]);
                        g_list_foreach(l, (GFunc)free, nullptr);
                        g_list_free(l);
                        return 2;
                    }
                    target_dir = argv[j];
                    break;
                }
                else
                {
                    if (argv[j][0] == '/')
                        // absolute path
                        str = ztd::strdup(argv[j]);
                    else
                    {
                        // relative path
                        if (!opt_cwd)
                        {
                            *reply = g_strdup_printf(
                                "spacefm: relative path '%s' requires %s option --dir DIR\n",
                                argv[j],
                                argv[i]);
                            g_list_foreach(l, (GFunc)free, nullptr);
                            g_list_free(l);
                            return 2;
                        }
                        str = g_build_filename(opt_cwd, argv[j], nullptr);
                    }
                    l = g_list_prepend(l, str);
                }
            }
            if (!l || (strcmp(argv[i], "delete") && !target_dir))
            {
                *reply =
                    g_strdup_printf("spacefm: task type %s requires FILE argument(s)\n", argv[i]);
                return 2;
            }
            l = g_list_reverse(l);
            if (!strcmp(argv[i], "copy"))
                j = VFS_FILE_TASK_COPY;
            else if (!strcmp(argv[i], "move"))
                j = VFS_FILE_TASK_MOVE;
            else if (!strcmp(argv[i], "link"))
                j = VFS_FILE_TASK_LINK;
            else if (!strcmp(argv[i], "delete"))
                j = VFS_FILE_TASK_DELETE;
            else if (!strcmp(argv[i], "trash"))
                j = VFS_FILE_TASK_TRASH;
            else
                return 1; // failsafe
            PtkFileTask* ptask =
                ptk_file_task_new((VFSFileTaskType)j,
                                  l,
                                  target_dir,
                                  GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                                  file_browser->task_view);
            ptk_file_task_run(ptask);
            *reply = g_strdup_printf("#!%s\n%s\n# Note: $new_task_id not valid until approx one "
                                     "half second after task  start\nnew_task_window=%p\n"
                                     "new_task_id=%p\n",
                                     BASHPATH,
                                     SHELL_SETTINGS,
                                     main_window,
                                     ptask);
        }
        else
        {
            *reply = g_strdup_printf("spacefm: invalid task type '%s'\n", argv[i]);
            return 2;
        }
    }
    else if (!strcmp(argv[0], "emit-key"))
    { // KEYCODE [KEYMOD]
        if (!argv[i])
        {
            *reply = g_strdup_printf("spacefm: command %s requires an argument\n", argv[0]);
            return 1;
        }
        // this only handles keys assigned to menu items
        GdkEventKey* event = (GdkEventKey*)gdk_event_new(GDK_KEY_PRESS);
        event->keyval = (unsigned int)strtol(argv[i], nullptr, 0);
        event->state = argv[i + 1] ? (unsigned int)strtol(argv[i + 1], nullptr, 0) : 0;
        if (event->keyval)
        {
            gtk_window_present(GTK_WINDOW(main_window));
            on_main_window_keypress(main_window, event, nullptr);
        }
        else
        {
            *reply = g_strdup_printf("spacefm: invalid keycode '%s'\n", argv[i]);
            gdk_event_free((GdkEvent*)event);
            return 2;
        }
        gdk_event_free((GdkEvent*)event);
    }
    else if (!strcmp(argv[0], "activate"))
    {
        if (!argv[i])
        {
            *reply = g_strdup_printf("spacefm: command %s requires an argument\n", argv[0]);
            return 1;
        }
        XSet* set = xset_find_custom(argv[i]);
        if (!set)
        {
            *reply =
                g_strdup_printf("spacefm: custom command or submenu '%s' not found\n", argv[i]);
            return 2;
        }
        XSetContext* context = xset_context_new();
        main_context_fill(file_browser, context);
        if (context && context->valid)
        {
            if (!xset_get_b("context_dlg") &&
                xset_context_test(context, set->context, false) != CONTEXT_SHOW)
            {
                *reply =
                    g_strdup_printf("spacefm: item '%s' context hidden or disabled\n", argv[i]);
                return 2;
            }
        }
        if (set->menu_style == XSET_MENU_SUBMENU)
        {
            // show submenu as popup menu
            set = xset_get(set->child);
            widget = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();

            xset_add_menuitem(file_browser, GTK_WIDGET(widget), accel_group, set);
            g_idle_add((GSourceFunc)delayed_show_menu, widget);
        }
        else
        {
            // activate item
            on_main_window_keypress(nullptr, nullptr, set);
        }
    }
    else if (!strcmp(argv[0], "add-event") || !strcmp(argv[0], "replace-event") ||
             !strcmp(argv[0], "remove-event"))
    {
        XSet* set;

        if (!(argv[i] && argv[i + 1]))
        {
            *reply = g_strdup_printf("spacefm: %s requires two arguments\n", argv[0]);
            return 1;
        }
        if (!(set = xset_is(argv[i])))
        {
            *reply = g_strdup_printf("spacefm: invalid event type '%s'\n", argv[i]);
            return 2;
        }
        // build command
        std::string str2 = (!strcmp(argv[0], "replace-event") ? "*" : "");
        for (j = i + 1; argv[j]; j++)
            str2.append(fmt::format("{}{}", j == i + 1 ? "" : " ", argv[j]));
        str = (char*)str2.c_str();
        // modify list
        if (!strcmp(argv[0], "remove-event"))
        {
            l = g_list_find_custom((GList*)set->ob2_data, str, (GCompareFunc)g_strcmp0);
            if (!l)
            {
                // remove replace event
                char* str3 = str;
                str = g_strdup_printf("*%s", str3);
                free(str3);
                l = g_list_find_custom((GList*)set->ob2_data, str, (GCompareFunc)g_strcmp0);
            }
            free(str);
            if (!l)
            {
                *reply = g_strdup_printf("spacefm: event handler not found\n");
                return 2;
            }
            l = g_list_remove((GList*)set->ob2_data, l->data);
        }
        else
            l = g_list_append((GList*)set->ob2_data, str);
        set->ob2_data = (void*)l;
    }
    else
    {
        *reply = g_strdup_printf("spacefm: invalid socket method '%s'\n", argv[0]);
        return 1;
    }
    return 0;
}

static bool
run_event(FMMainWindow* main_window, PtkFileBrowser* file_browser, XSet* preset, const char* event,
          int panel, int tab, const char* focus, int keyval, int button, int state, bool visible,
          XSet* set, char* ucmd)
{
    bool inhibit;
    int exit_status;

    std::string command;

    if (!ucmd)
        return false;

    if (ucmd[0] == '*')
    {
        ucmd++;
        inhibit = true;
    }
    else
        inhibit = false;

    if (!preset &&
        (!strcmp(event, "evt_start") || !strcmp(event, "evt_exit") || !strcmp(event, "evt_device")))
    {
        std::string cmd = ucmd;

        cmd = ztd::replace(cmd, "%e", event);
        if (!strcmp(event, "evt_device"))
        {
            if (!focus)
                return false;
            cmd = ztd::replace(cmd, "%f", focus);
            const char* change;
            switch (state)
            {
                case VFS_VOLUME_ADDED:
                    change = ztd::strdup("added");
                    break;
                case VFS_VOLUME_REMOVED:
                    change = ztd::strdup("removed");
                    break;
                default:
                    change = ztd::strdup("changed");
                    break;
            }
            cmd = ztd::replace(cmd, "%v", change);
        }
        LOG_INFO("EVENT {} >>> {}", event, cmd);
        command = fmt::format("bash -c {}", cmd);
        Glib::spawn_command_line_async(command);
        return false;
    }

    if (!main_window)
        return false;

    // replace vars
    const char* replace;

    if (set == event_handler.win_click)
    {
        replace = ztd::strdup("ewptfbm");
        state = (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK |
                          GDK_HYPER_MASK | GDK_META_MASK));
    }
    else if (set == event_handler.win_key)
        replace = ztd::strdup("ewptkm");
    else if (set == event_handler.pnl_show)
        replace = ztd::strdup("ewptfv");
    else if (set == event_handler.tab_chdir)
        replace = ztd::strdup("ewptd");
    else
        replace = ztd::strdup("ewpt");

    char* str;
    std::string rep;
    char var[3];
    var[0] = '%';
    var[2] = '\0';
    int i = 0;
    std::string cmd = ucmd;
    while (replace[i])
    {
        /*
        %w  windowid
        %p  panel
        %t  tab
        %f  focus
        %e  event
        %k  keycode
        %m  modifier
        %b  button
        %v  visible
        %d  cwd
        */
        var[1] = replace[i];
        if (var[1] == 'e')
            cmd = ztd::replace(cmd, var, event);
        else if (ztd::contains(cmd, var))
        {
            switch (var[1])
            {
                case 'f':
                    if (!focus)
                    {
                        rep = fmt::format("panel{}", panel);
                        cmd = ztd::replace(cmd, var, rep);
                    }
                    else
                        cmd = ztd::replace(cmd, var, focus);
                    break;
                case 'w':
                    rep = fmt::format("{:p}", (void*)main_window);
                    cmd = ztd::replace(cmd, var, rep);
                    break;
                case 'p':
                    rep = fmt::format("{}", panel);
                    cmd = ztd::replace(cmd, var, rep);
                    break;
                case 't':
                    rep = fmt::format("{}", tab);
                    cmd = ztd::replace(cmd, var, rep);
                    break;
                case 'v':
                    cmd = ztd::replace(cmd, var, visible ? "1" : "0");
                    break;
                case 'k':
                    rep = fmt::format("{:#x}", keyval);
                    cmd = ztd::replace(cmd, var, rep);
                    break;
                case 'b':
                    rep = fmt::format("{}", button);
                    cmd = ztd::replace(cmd, var, rep);
                    break;
                case 'm':
                    rep = fmt::format("{:#x}", state);
                    cmd = ztd::replace(cmd, var, rep);
                    break;
                case 'd':
                    if (file_browser)
                    {
                        rep = bash_quote(ptk_file_browser_get_cwd(file_browser));
                        cmd = ztd::replace(cmd, var, rep);
                    }
                    break;
                default:
                    // failsafe
                    return false;
                    break;
            }
        }
        i++;
    }

    if (!inhibit)
    {
        LOG_INFO("EVENT {} >>> {}", event, cmd);
        if (!strcmp(event, "evt_tab_close"))
        {
            command = fmt::format("bash -c {}", cmd);
            // file_browser becomes invalid so spawn
            Glib::spawn_command_line_async(command);
        }
        else
        {
            // task
            PtkFileTask* task = ptk_file_exec_new(event,
                                                  ptk_file_browser_get_cwd(file_browser),
                                                  GTK_WIDGET(file_browser),
                                                  main_window->task_view);
            task->task->exec_browser = file_browser;
            task->task->exec_command = cmd;
            task->task->exec_icon = set->icon;
            task->task->exec_sync = false;
            task->task->exec_export = true;
            ptk_file_task_run(task);
        }
        return false;
    }

    LOG_INFO("REPLACE_EVENT {} >>> {}", event, cmd);

    inhibit = false;
    command = fmt::format("bash -c {}", cmd);
    Glib::spawn_command_line_sync(command, nullptr, nullptr, &exit_status);

    if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0)
        inhibit = true;

    LOG_INFO("REPLACE_EVENT ? {}", inhibit ? "true" : "false");
    return inhibit;
}

bool
main_window_event(void* mw, XSet* preset, const char* event, int panel, int tab, const char* focus,
                  int keyval, int button, int state, bool visible)
{
    XSet* set;
    bool inhibit = false;

    // LOG_INFO("main_window_event {}", event);
    // get set
    if (preset)
        set = preset;
    else
    {
        set = xset_get(event);
        if (!set->s && !set->ob2_data)
            return false;
    }

    // get main_window, panel, and tab
    FMMainWindow* main_window;
    PtkFileBrowser* file_browser;
    if (!mw)
        main_window = fm_main_window_get_last_active();
    else
        main_window = static_cast<FMMainWindow*>(mw);
    if (main_window)
    {
        file_browser = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
        if (!file_browser)
            return false;
        if (!panel)
            panel = main_window->curpanel;
        if (!tab)
        {
            tab = gtk_notebook_page_num(GTK_NOTEBOOK(main_window->panel[file_browser->mypanel - 1]),
                                        GTK_WIDGET(file_browser)) +
                  1;
        }
    }
    else
        file_browser = nullptr;

    // dynamic handlers
    if (set->ob2_data)
    {
        GList* l;
        for (l = (GList*)set->ob2_data; l; l = l->next)
        {
            if (run_event(main_window,
                          file_browser,
                          preset,
                          event,
                          panel,
                          tab,
                          focus,
                          keyval,
                          button,
                          state,
                          visible,
                          set,
                          (char*)l->data))
                inhibit = true;
        }
    }

    // Events menu handler
    return (run_event(main_window,
                      file_browser,
                      preset,
                      event,
                      panel,
                      tab,
                      focus,
                      keyval,
                      button,
                      state,
                      visible,
                      set,
                      set->s) ||
            inhibit);
}
