/*
 *  C Implementation: pref_dialog
 *
 * Description:
 *
 *
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#include <ctime>

#include "window-reference.hxx"

#include <gtk/gtk.h>

#include "pref-dialog.hxx"
#include "main-window.hxx"

#include "extern.hxx"

#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-location-view.hxx"

struct FMPrefDlg
{
    GtkWidget* dlg;
    GtkWidget* notebook;
    GtkWidget* encoding;
    GtkWidget* bm_open_method;
    GtkWidget* max_thumb_size;
    GtkWidget* show_thumbnail;
    GtkWidget* thumb_label1;
    GtkWidget* thumb_label2;
    GtkWidget* terminal;
    GtkWidget* big_icon_size;
    GtkWidget* small_icon_size;
    GtkWidget* tool_icon_size;
    GtkWidget* single_click;
    GtkWidget* single_hover;
    GtkWidget* use_si_prefix;
    GtkWidget* root_bar;
    GtkWidget* drag_action;

    /* Interface tab */
    GtkWidget* always_show_tabs;
    GtkWidget* hide_close_tab_buttons;

    // MOD
    GtkWidget* confirm_delete;
    GtkWidget* click_exec;
    GtkWidget* su_command;
    GtkWidget* date_format;
    GtkWidget* date_display;
    GtkWidget* editor;
    GtkWidget* editor_terminal;
    GtkWidget* root_editor;
    GtkWidget* root_editor_terminal;
};

static FMPrefDlg* data = nullptr;
static const int tool_icon_sizes[] = {0,
                                      GTK_ICON_SIZE_MENU,
                                      GTK_ICON_SIZE_SMALL_TOOLBAR,
                                      GTK_ICON_SIZE_LARGE_TOOLBAR,
                                      GTK_ICON_SIZE_BUTTON,
                                      GTK_ICON_SIZE_DND,
                                      GTK_ICON_SIZE_DIALOG};
// also change max_icon_size in settings.c & lists in prefdlg.ui prefdlg2.ui
// see create_size in vfs-thumbnail-loader.c:_vfs_thumbnail_load()
static const int big_icon_sizes[] = {512, 384, 256, 192, 128, 96, 72, 64, 48, 36, 32, 24, 22};
static const int small_icon_sizes[] =
    {512, 384, 256, 192, 128, 96, 72, 64, 48, 36, 32, 24, 22, 16, 12};
static const char* date_formats[] = {"%Y-%m-%d %H:%M", "%Y-%m-%d", "%Y-%m-%d %H:%M:%S"};
static const int drag_actions[] = {0, 1, 2, 3};

static void
dir_unload_thumbnails(const char* path, VFSDir* dir, void* user_data)
{
    vfs_dir_unload_thumbnails(dir, GPOINTER_TO_INT(user_data));
}

static void
on_response(GtkDialog* dlg, int response, FMPrefDlg* user_data)

{
    int i;
    int n;
    int ibig_icon = -1;
    int ismall_icon = -1;
    int itool_icon = -1;

    int max_thumb;
    bool show_thumbnail;
    int big_icon;
    int small_icon;
    int tool_icon;
    bool single_click;
    bool root_bar;
    bool root_set_change = false;
    const GList* l;
    PtkFileBrowser* file_browser;
    bool use_si_prefix;
    GtkNotebook* notebook;
    int p;
    FMMainWindow* a_window;
    char* str;

    GtkWidget* tab_label;
    /* interface settings */
    bool always_show_tabs;
    bool hide_close_tab_buttons;

    /* built-in response codes of GTK+ are all negative */
    if (response >= 0)
        return;

    if (response == GTK_RESPONSE_OK)
    {
        show_thumbnail = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->show_thumbnail));
        max_thumb = ((int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->max_thumb_size))) << 10;

        /* interface settings */

        always_show_tabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->always_show_tabs));
        if (always_show_tabs != app_settings.always_show_tabs)
        {
            app_settings.always_show_tabs = always_show_tabs;
            // update all windows/all panels
            for (l = fm_main_window_get_all(); l; l = l->next)
            {
                a_window = static_cast<FMMainWindow*>(l->data);
                for (p = 1; p < 5; p++)
                {
                    notebook = GTK_NOTEBOOK(a_window->panel[p - 1]);
                    n = gtk_notebook_get_n_pages(notebook);
                    if (always_show_tabs)
                        gtk_notebook_set_show_tabs(notebook, true);
                    else if (n == 1)
                        gtk_notebook_set_show_tabs(notebook, false);
                }
            }
        }
        hide_close_tab_buttons =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->hide_close_tab_buttons));
        if (hide_close_tab_buttons != app_settings.hide_close_tab_buttons)
        {
            app_settings.hide_close_tab_buttons = hide_close_tab_buttons;
            // update all windows/all panels/all browsers
            for (l = fm_main_window_get_all(); l; l = l->next)
            {
                a_window = static_cast<FMMainWindow*>(l->data);
                for (p = 1; p < 5; p++)
                {
                    notebook = GTK_NOTEBOOK(a_window->panel[p - 1]);
                    n = gtk_notebook_get_n_pages(notebook);
                    for (i = 0; i < n; ++i)
                    {
                        file_browser = PTK_FILE_BROWSER(gtk_notebook_get_nth_page(notebook, i));
                        tab_label = fm_main_window_create_tab_label(a_window, file_browser);
                        gtk_notebook_set_tab_label(notebook, GTK_WIDGET(file_browser), tab_label);
                        fm_main_window_update_tab_label(a_window,
                                                        file_browser,
                                                        file_browser->dir->disp_path);
                    }
                }
            }
        }

        // ===============================================================

        /* thumbnail settings are changed */
        if (app_settings.show_thumbnail != show_thumbnail ||
            app_settings.max_thumb_size != max_thumb)
        {
            app_settings.show_thumbnail = !show_thumbnail; // toggle reverses this
            app_settings.max_thumb_size = max_thumb;
            // update all windows/all panels/all browsers + desktop
            main_window_toggle_thumbnails_all_windows();
        }

        /* icon sizes are changed? */
        ibig_icon = gtk_combo_box_get_active(GTK_COMBO_BOX(data->big_icon_size));
        big_icon = ibig_icon >= 0 ? big_icon_sizes[ibig_icon] : app_settings.big_icon_size;
        ismall_icon = gtk_combo_box_get_active(GTK_COMBO_BOX(data->small_icon_size));
        small_icon =
            ismall_icon >= 0 ? small_icon_sizes[ismall_icon] : app_settings.small_icon_size;
        itool_icon = gtk_combo_box_get_active(GTK_COMBO_BOX(data->tool_icon_size));
        if (itool_icon >= 0 && itool_icon <= GTK_ICON_SIZE_DIALOG)
            tool_icon = tool_icon_sizes[itool_icon];

        if (big_icon != app_settings.big_icon_size || small_icon != app_settings.small_icon_size)
        {
            vfs_mime_type_set_icon_size(big_icon, small_icon);
            vfs_file_info_set_thumbnail_size(big_icon, small_icon);

            /* unload old thumbnails (icons of *.desktop files will be unloaded here, too)  */
            if (big_icon != app_settings.big_icon_size)
                vfs_dir_foreach((GHFunc)dir_unload_thumbnails, GINT_TO_POINTER(1));
            if (small_icon != app_settings.small_icon_size)
                vfs_dir_foreach((GHFunc)dir_unload_thumbnails, GINT_TO_POINTER(0));

            app_settings.big_icon_size = big_icon;
            app_settings.small_icon_size = small_icon;

            // update all windows/all panels/all browsers
            for (l = fm_main_window_get_all(); l; l = l->next)
            {
                a_window = static_cast<FMMainWindow*>(l->data);
                for (p = 1; p < 5; p++)
                {
                    notebook = GTK_NOTEBOOK(a_window->panel[p - 1]);
                    n = gtk_notebook_get_n_pages(notebook);
                    for (i = 0; i < n; ++i)
                    {
                        file_browser = PTK_FILE_BROWSER(gtk_notebook_get_nth_page(notebook, i));
                        // update views
                        gtk_widget_destroy(file_browser->folder_view);
                        file_browser->folder_view = nullptr;
                        if (file_browser->side_dir)
                        {
                            gtk_widget_destroy(file_browser->side_dir);
                            file_browser->side_dir = nullptr;
                        }
                        ptk_file_browser_update_views(nullptr, file_browser);
                        if (file_browser->side_book)
                            ptk_bookmark_view_update_icons(nullptr, file_browser);
                    }
                }
            }
            update_volume_icons();
        }

        if (tool_icon != app_settings.tool_icon_size)
        {
            app_settings.tool_icon_size = tool_icon;
            main_window_rebuild_all_toolbars(nullptr);
        }

        /* unit settings changed? */
        bool need_refresh = false;
        use_si_prefix = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->use_si_prefix));
        if (use_si_prefix != app_settings.use_si_prefix)
        {
            app_settings.use_si_prefix = use_si_prefix;
            need_refresh = true;
        }

        // date format
        char* etext =
            g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(data->date_format)))));
        if (g_strcmp0(etext, xset_get_s("date_format")))
        {
            if (etext[0] == '\0')
                xset_set("date_format", "s", "%Y-%m-%d %H:%M");
            else
                xset_set("date_format", "s", etext);
            g_free(etext);
            app_settings.date_format = xset_get_s("date_format");
            need_refresh = true;
        }
        if (need_refresh)
            main_window_refresh_all();

        /* single click changed? */
        single_click = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->single_click));
        if (single_click != app_settings.single_click)
        {
            app_settings.single_click = single_click;
            // update all windows/all panels/all browsers
            for (l = fm_main_window_get_all(); l; l = l->next)
            {
                a_window = static_cast<FMMainWindow*>(l->data);
                for (p = 1; p < 5; p++)
                {
                    notebook = GTK_NOTEBOOK(a_window->panel[p - 1]);
                    n = gtk_notebook_get_n_pages(notebook);
                    for (i = 0; i < n; ++i)
                    {
                        file_browser = PTK_FILE_BROWSER(gtk_notebook_get_nth_page(notebook, i));
                        ptk_file_browser_set_single_click(file_browser, app_settings.single_click);
                    }
                }
            }
        }

        /* single click - hover selects changed? */
        bool no_single_hover = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->single_hover));
        if (no_single_hover != app_settings.no_single_hover)
        {
            app_settings.no_single_hover = no_single_hover;
            // update all windows/all panels/all browsers
            for (l = fm_main_window_get_all(); l; l = l->next)
            {
                a_window = static_cast<FMMainWindow*>(l->data);
                for (p = 1; p < 5; p++)
                {
                    notebook = GTK_NOTEBOOK(a_window->panel[p - 1]);
                    n = gtk_notebook_get_n_pages(notebook);
                    for (i = 0; i < n; ++i)
                    {
                        file_browser = PTK_FILE_BROWSER(gtk_notebook_get_nth_page(notebook, i));
                        ptk_file_browser_set_single_click_timeout(
                            file_browser,
                            app_settings.no_single_hover ? 0 : SINGLE_CLICK_TIMEOUT);
                    }
                }
            }
        }

        // MOD
        app_settings.no_execute =
            !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->click_exec));
        app_settings.no_confirm =
            !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->confirm_delete));

        char* s = g_strdup_printf("%d", gtk_combo_box_get_active(GTK_COMBO_BOX(data->drag_action)));
        xset_set("drag_action", "x", s);
        g_free(s);

        // terminal su command
        char* custom_su = nullptr;
        if (config_settings.terminal_su)
            // get su from /etc/spacefm/spacefm.conf
            custom_su = g_find_program_in_path(config_settings.terminal_su);
        int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(data->su_command));
        if (idx > -1)
        {
            if (custom_su)
            {
                if (idx == 0)
                    xset_set("su_command", "s", custom_su);
                else
                    xset_set("su_command", "s", su_commands[idx - 1]);
                g_free(custom_su);
            }
            else
                xset_set("su_command", "s", su_commands[idx]);
        }

        // MOD editors
        xset_set("editor", "s", gtk_entry_get_text(GTK_ENTRY(data->editor)));
        xset_set_b("editor",
                   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->editor_terminal)));
        const char* root_editor = gtk_entry_get_text(GTK_ENTRY(data->root_editor));
        const char* old_root_editor = xset_get_s("root_editor");
        if (!old_root_editor)
        {
            if (root_editor[0] != '\0')
            {
                xset_set("root_editor", "s", root_editor);
                root_set_change = true;
            }
        }
        else if (strcmp(root_editor, old_root_editor))
        {
            xset_set("root_editor", "s", root_editor);
            root_set_change = true;
        }
        if (!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->root_editor_terminal)) !=
            !!xset_get_b("root_editor"))
        {
            xset_set_b("root_editor",
                       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->root_editor_terminal)));
            root_set_change = true;
        }

        // MOD terminal
        char* old_terminal = xset_get_s("main_terminal");
        char* terminal = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(data->terminal));
        g_strstrip(terminal);
        if (g_strcmp0(terminal, old_terminal))
        {
            xset_set("main_terminal", "s", terminal[0] == '\0' ? nullptr : terminal);
            root_set_change = true;
        }
        // report missing terminal
        if ((str = strchr(terminal, ' ')))
            str[0] = '\0';
        str = g_find_program_in_path(terminal);
        if (!str)
        {
            str = g_strdup_printf("Unable to find terminal program '%s'", terminal);
            ptk_show_error(GTK_WINDOW(dlg), "Error", str);
        }
        g_free(str);
        g_free(terminal);

        /* save to config file */
        save_settings(nullptr);

        if (xset_get_b("main_terminal"))
        {
            root_set_change = true;
            xset_set_b("main_terminal", false);
        }

        // root settings saved?
        if (geteuid() != 0)
        {
            if (root_set_change)
            {
                // task
                char* msg = g_strdup_printf(
                    "You will now be asked for your root password to save the root settings for "
                    "this user to a file in %s/spacefm/  Supplying the password in the next "
                    "window is recommended.  Because SpaceFM runs some commands as root via su, "
                    "these settings are best protected by root.",
                    SYSCONFDIR);
                xset_msg_dialog(GTK_WIDGET(dlg), 0, "Save Root Settings", 0, msg, nullptr);
                g_free(msg);
                PtkFileTask* task =
                    ptk_file_exec_new("Save Root Settings", nullptr, nullptr, nullptr);
                task->task->exec_command = g_strdup_printf("echo");
                task->task->exec_as_user = g_strdup_printf("root");
                task->task->exec_sync = false;
                task->task->exec_export = false;
                task->task->exec_write_root = true;
                ptk_file_task_run(task);
            }
        }
    }
    gtk_widget_destroy(GTK_WIDGET(dlg));
    g_free(data);
    data = nullptr;
    WindowReference::decrease();
}

static void
on_date_format_changed(GtkComboBox* widget, FMPrefDlg* data)
{
    char buf[128];
    const char* etext;

    std::time_t now = std::time(nullptr);
    etext = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(data->date_format))));
    strftime(buf, sizeof(buf), etext, localtime(&now));
    gtk_label_set_text(GTK_LABEL(data->date_display), buf);
}

static void
on_single_click_toggled(GtkWidget* widget, FMPrefDlg* data)
{
    gtk_widget_set_sensitive(data->single_hover,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->single_click)));
}

static void
on_show_thumbnail_toggled(GtkWidget* widget, FMPrefDlg* data)
{
    gtk_widget_set_sensitive(data->max_thumb_size,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->show_thumbnail)));
    gtk_widget_set_sensitive(data->thumb_label1,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->show_thumbnail)));
    gtk_widget_set_sensitive(data->thumb_label2,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->show_thumbnail)));
}

bool
fm_edit_preference(GtkWindow* parent, int page)
{
    int ibig_icon = -1;
    int ismall_icon = -1;
    int itool_icon = -1;
    GtkWidget* dlg;

    if (!data)
    {
        GtkTreeModel* model;
        // this invokes GVFS-RemoteVolumeMonitor via IsSupported
        GtkBuilder* builder = _gtk_builder_new_from_file("prefdlg3.ui");

        if (!builder)
            return false;
        WindowReference::increase();

        data = g_new0(FMPrefDlg, 1);
        dlg = GTK_WIDGET(gtk_builder_get_object(builder, "dlg"));
        if (parent)
            gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);
        xset_set_window_icon(GTK_WINDOW(dlg));

        data->dlg = dlg;
        data->notebook = GTK_WIDGET(gtk_builder_get_object(builder, "notebook"));

        /* Setup 'General' tab */

        data->encoding = GTK_WIDGET(gtk_builder_get_object(builder, "filename_encoding"));
        data->bm_open_method = GTK_WIDGET(gtk_builder_get_object(builder, "bm_open_method"));
        data->show_thumbnail = GTK_WIDGET(gtk_builder_get_object(builder, "show_thumbnail"));
        data->thumb_label1 = GTK_WIDGET(gtk_builder_get_object(builder, "thumb_label1"));
        data->thumb_label2 = GTK_WIDGET(gtk_builder_get_object(builder, "thumb_label2"));
        data->max_thumb_size = GTK_WIDGET(gtk_builder_get_object(builder, "max_thumb_size"));
        data->terminal = GTK_WIDGET(gtk_builder_get_object(builder, "terminal"));
        data->big_icon_size = GTK_WIDGET(gtk_builder_get_object(builder, "big_icon_size"));
        data->small_icon_size = GTK_WIDGET(gtk_builder_get_object(builder, "small_icon_size"));
        data->tool_icon_size = GTK_WIDGET(gtk_builder_get_object(builder, "tool_icon_size"));
        data->single_click = GTK_WIDGET(gtk_builder_get_object(builder, "single_click"));
        data->single_hover = GTK_WIDGET(gtk_builder_get_object(builder, "single_hover"));
        data->use_si_prefix = GTK_WIDGET(gtk_builder_get_object(builder, "use_si_prefix"));
        data->root_bar = GTK_WIDGET(gtk_builder_get_object(builder, "root_bar"));
        data->drag_action = GTK_WIDGET(gtk_builder_get_object(builder, "drag_action"));

        model = GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_STRING));
        gtk_combo_box_set_model(GTK_COMBO_BOX(data->terminal), model);
        gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(data->terminal), 0);
        g_object_unref(model);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->max_thumb_size),
                                  app_settings.max_thumb_size >> 10);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->show_thumbnail),
                                     app_settings.show_thumbnail);
        g_signal_connect(data->show_thumbnail,
                         "toggled",
                         G_CALLBACK(on_show_thumbnail_toggled),
                         data);
        gtk_widget_set_sensitive(data->max_thumb_size, app_settings.show_thumbnail);
        gtk_widget_set_sensitive(data->thumb_label1, app_settings.show_thumbnail);
        gtk_widget_set_sensitive(data->thumb_label2, app_settings.show_thumbnail);

        for (unsigned int i = 0; i < G_N_ELEMENTS(terminal_programs); ++i)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(data->terminal),
                                           terminal_programs[i]);
        }

        char* terminal = xset_get_s("main_terminal");
        if (terminal)
        {
            unsigned int i;
            for (i = 0; i < G_N_ELEMENTS(terminal_programs); ++i)
            {
                if (!strcmp(terminal_programs[i], terminal))
                    break;
            }
            if (i >= G_N_ELEMENTS(terminal_programs))
            { /* Found */
                gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(data->terminal), terminal);
                i = 0;
            }
            gtk_combo_box_set_active(GTK_COMBO_BOX(data->terminal), i);
        }

        for (unsigned int i = 0; i < G_N_ELEMENTS(big_icon_sizes); ++i)
        {
            if (big_icon_sizes[i] == app_settings.big_icon_size)
            {
                ibig_icon = i;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->big_icon_size), ibig_icon);

        for (unsigned int i = 0; i < G_N_ELEMENTS(small_icon_sizes); ++i)
        {
            if (small_icon_sizes[i] == app_settings.small_icon_size)
            {
                ismall_icon = i;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->small_icon_size), ismall_icon);

        // sfm
        itool_icon = 0;
        for (unsigned int i = 0; i < G_N_ELEMENTS(tool_icon_sizes); ++i)
        {
            if (tool_icon_sizes[i] == app_settings.tool_icon_size)
            {
                itool_icon = i;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->tool_icon_size), itool_icon);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->single_click),
                                     app_settings.single_click);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->single_hover),
                                     !app_settings.no_single_hover);
        gtk_widget_set_sensitive(data->single_hover, app_settings.single_click);
        g_signal_connect(data->single_click, "toggled", G_CALLBACK(on_single_click_toggled), data);

        /* Setup 'Interface' tab */

        data->always_show_tabs = GTK_WIDGET(gtk_builder_get_object(builder, "always_show_tabs"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->always_show_tabs),
                                     app_settings.always_show_tabs);

        data->hide_close_tab_buttons =
            GTK_WIDGET(gtk_builder_get_object(builder, "hide_close_tab_buttons"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->hide_close_tab_buttons),
                                     app_settings.hide_close_tab_buttons);

        // MOD Interface
        data->confirm_delete = GTK_WIDGET(gtk_builder_get_object(builder, "confirm_delete"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->confirm_delete),
                                     !app_settings.no_confirm);
        data->click_exec = GTK_WIDGET(gtk_builder_get_object(builder, "click_exec"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->click_exec), !app_settings.no_execute);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->root_bar), xset_get_b("root_bar"));
        gtk_widget_set_sensitive(data->root_bar, geteuid() == 0);

        int drag_action = xset_get_int("drag_action", "x");
        int drag_action_set = 0;
        for (unsigned int i = 0; i < G_N_ELEMENTS(drag_actions); ++i)
        {
            if (drag_actions[i] == drag_action)
            {
                drag_action_set = i;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->drag_action), drag_action_set);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->use_si_prefix),
                                     app_settings.use_si_prefix);

        // Advanced Tab ==================================================

        // terminal su
        int idx;
        GtkTreeIter it;
        char* custom_su = nullptr;
        char* use_su;
        data->su_command = GTK_WIDGET(gtk_builder_get_object(builder, "su_command"));
        use_su = xset_get_s("su_command");
        if (config_settings.terminal_su)
            // get su from /etc/spacefm/spacefm.conf
            custom_su = g_find_program_in_path(config_settings.terminal_su);
        if (custom_su)
        {
            GtkListStore* su_list =
                GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(data->su_command)));
            gtk_list_store_prepend(su_list, &it);
            gtk_list_store_set(GTK_LIST_STORE(su_list), &it, 0, custom_su, -1);
        }
        if (!use_su)
            idx = 0;
        else if (custom_su && !g_strcmp0(custom_su, use_su))
            idx = 0;
        else
        {
            unsigned int i;
            for (i = 0; i < G_N_ELEMENTS(su_commands); i++)
            {
                if (!strcmp(su_commands[i], use_su))
                    break;
            }
            if (i == G_N_ELEMENTS(su_commands))
                idx = 0;
            else if (custom_su)
                idx = i + 1;
            else
                idx = i;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->su_command), idx);
        g_free(custom_su);

        // date format
        data->date_format = GTK_WIDGET(gtk_builder_get_object(builder, "date_format"));
        data->date_display = GTK_WIDGET(gtk_builder_get_object(builder, "label_date_disp"));
        model = GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_STRING));
        gtk_combo_box_set_model(GTK_COMBO_BOX(data->date_format), model);
        gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(data->date_format), 0);
        g_object_unref(model);
        for (unsigned int i = 0; i < G_N_ELEMENTS(date_formats); ++i)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(data->date_format), date_formats[i]);
        }
        char* date_s = xset_get_s("date_format");
        if (date_s)
        {
            unsigned int i;
            for (i = 0; i < G_N_ELEMENTS(date_formats); ++i)
            {
                if (!strcmp(date_formats[i], date_s))
                    break;
            }
            if (i >= G_N_ELEMENTS(date_formats))
            {
                gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(data->date_format), date_s);
                i = 0;
            }
            gtk_combo_box_set_active(GTK_COMBO_BOX(data->date_format), i);
        }
        on_date_format_changed(nullptr, data);
        g_signal_connect(data->date_format, "changed", G_CALLBACK(on_date_format_changed), data);

        // editors
        data->editor = GTK_WIDGET(gtk_builder_get_object(builder, "editor"));
        if (xset_get_s("editor"))
            gtk_entry_set_text(GTK_ENTRY(data->editor), xset_get_s("editor"));
        data->editor_terminal = GTK_WIDGET(gtk_builder_get_object(builder, "editor_terminal"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->editor_terminal),
                                     xset_get_b("editor"));
        data->root_editor = GTK_WIDGET(gtk_builder_get_object(builder, "root_editor"));
        if (xset_get_s("root_editor"))
            gtk_entry_set_text(GTK_ENTRY(data->root_editor), xset_get_s("root_editor"));
        data->root_editor_terminal =
            GTK_WIDGET(gtk_builder_get_object(builder, "root_editor_terminal"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->root_editor_terminal),
                                     xset_get_b("root_editor"));

        g_signal_connect(dlg, "response", G_CALLBACK(on_response), data);
        g_object_unref(builder);
    }

    // Set current Preferences page
    const int desktop_page_num = 2;
    // notebook page number 3 is permanently hidden Volume Management
    if (page > desktop_page_num)
        page++;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(data->notebook), page);

    gtk_window_present(GTK_WINDOW(data->dlg));
    return true;
}
