/**
 * Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
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

// FIXME: Currently this only works with GNU find-utils.
// Compatibility with other systems like BSD, need to be improved.

#include <string>
#include <filesystem>

#include <array>
#include <vector>

#include <chrono>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <sys/wait.h>

#include <exo/exo.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "window-reference.hxx"

#include "vfs/vfs-volume.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "main-window.hxx"

#include "ptk/ptk-file-misc.hxx"
#include "ptk/ptk-utils.hxx"

#include "type-conversion.hxx"

#include "find-files.hxx"

enum FindFilesCol
{
    COL_ICON,
    COL_NAME,
    COL_DIR,
    COL_SIZE,
    COL_TYPE,
    COL_MTIME,
    COL_INFO,
    N_RES_COLS
};

struct FindFile
{
    FindFile();

    GtkWidget* win;
    GtkWidget* search_criteria;

    GtkWidget* fn_pattern;
    GtkWidget* fn_pattern_entry;
    GtkWidget* fn_case_sensitive;

    /* file content */
    GtkWidget* fc_pattern;
    GtkWidget* fc_case_sensitive;
    GtkWidget* fc_use_regexp;

    /* advanced options */
    GtkWidget* search_hidden;

    /* size & date */
    GtkWidget* use_size_lower;
    GtkWidget* use_size_upper;
    GtkWidget* size_lower;
    GtkWidget* size_upper;
    GtkWidget* size_lower_unit;
    GtkWidget* size_upper_unit;

    GtkWidget* date_limit;
    GtkWidget* date1;
    GtkWidget* date2;

    /* file types */
    GtkWidget* all_files;
    GtkWidget* text_files;
    GtkWidget* img_files;
    GtkWidget* audio_files;
    GtkWidget* video_files;

    /* places */
    GtkListStore* places_list;
    GtkWidget* places_view;
    GtkWidget* add_directory_btn;
    GtkWidget* remove_directory_btn;
    GtkWidget* include_sub;

    /* search result pane */
    GtkWidget* search_result;
    GtkWidget* result_view;
    GtkListStore* result_list;

    /* buttons */
    GtkWidget* start_btn;
    GtkWidget* stop_btn;
    GtkWidget* again_btn;

    Glib::Pid pid;
    int stdo;

    VFSAsyncTask* task;
};

FindFile::FindFile()
{
    this->win = nullptr;
    this->search_criteria = nullptr;

    this->fn_pattern = nullptr;
    this->fn_pattern_entry = nullptr;
    this->fn_case_sensitive = nullptr;

    /* file content */
    this->fc_pattern = nullptr;
    this->fc_case_sensitive = nullptr;
    this->fc_use_regexp = nullptr;

    /* advanced options */
    this->search_hidden = nullptr;

    /* size & date */
    this->use_size_lower = nullptr;
    this->use_size_upper = nullptr;
    this->size_lower = nullptr;
    this->size_upper = nullptr;
    this->size_lower_unit = nullptr;
    this->size_upper_unit = nullptr;

    this->date_limit = nullptr;
    this->date1 = nullptr;
    this->date2 = nullptr;

    /* file types */
    this->all_files = nullptr;
    this->text_files = nullptr;
    this->img_files = nullptr;
    this->audio_files = nullptr;
    this->video_files = nullptr;

    /* places */
    this->places_list = nullptr;
    this->places_view = nullptr;
    this->add_directory_btn = nullptr;
    this->remove_directory_btn = nullptr;
    this->include_sub = nullptr;

    /* search result pane */
    this->search_result = nullptr;
    this->result_view = nullptr;
    this->result_list = nullptr;

    /* buttons */
    this->start_btn = nullptr;
    this->stop_btn = nullptr;
    this->again_btn = nullptr;

    this->pid = 0;
    this->stdo = 0;

    this->task = nullptr;
}

struct FoundFile
{
    FoundFile(VFSFileInfo* fi, const std::string& dir_path);
    // ~FoundFile();

    VFSFileInfo* fi;
    std::string dir_path;
};

FoundFile::FoundFile(VFSFileInfo* fi, const std::string& dir_path)
{
    this->fi = fi;
    this->dir_path = dir_path;
}

#define FOUND_FILE(obj) (static_cast<FoundFile*>(obj))

static const char menu_def[] = "<ui>"
                               "<popup name=\"Popup\">"
                               "<menuitem name=\"Open\" action=\"OpenAction\" />"
                               "<menuitem name=\"OpenDirectory\" action=\"OpenDirectoryAction\" />"
                               "</popup>"
                               "</ui>";

static bool
open_file(char* dir, GList* files, PtkFileBrowser* file_browser)
{
    if (!files)
        return false;

    std::vector<VFSFileInfo*> sel_files = glist_to_vector_VFSFileInfo(files);

    /*igtodo test passing file_browser here? */
    ptk_open_files_with_app(dir, sel_files, nullptr, nullptr, false, true);

    // sfm open selected dirs
    if (file_browser)
    {
        GList* l;
        VFSFileInfo* file;

        for (l = files; l; l = l->next)
        {
            file = VFS_FILE_INFO(l->data);
            if (!file)
                continue;

            std::string full_path = Glib::build_filename(dir, vfs_file_info_get_name(file));
            if (std::filesystem::is_directory(full_path))
            {
                ptk_file_browser_emit_open(file_browser,
                                           full_path.c_str(),
                                           PtkOpenAction::PTK_OPEN_NEW_TAB);
            }
        }
    }

    vfs_file_info_list_free(sel_files);
    return true;
}

static void
open_dir(char* dir, GList* files, FMMainWindow* w)
{
    (void)files;
    fm_main_window_add_new_tab(w, dir);
}

static void
on_open_files(GAction* action, FindFile* data)
{
    GtkTreeModel* model;
    GtkTreeSelection* sel;
    GtkTreeIter it;
    GList* row;
    GList* rows;
    GHashTable* hash;
    GtkWidget* w;
    VFSFileInfo* fi;
    bool open_files_has_dir = false;        // sfm
    PtkFileBrowser* file_browser = nullptr; // sfm
    bool open_files = true;

    if (action)
        open_files = (!strcmp(g_action_get_name(action), "OpenAction"));

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->result_view));
    rows = gtk_tree_selection_get_selected_rows(sel, &model);
    if (!rows)
        return;

    // sfm this frees list when new value inserted - caused segfault
    // hash = g_hash_table_new_full( g_str_hash, g_str_equal, (GDestroyNotify)free, open_files ?
    // (GDestroyNotify)vfs_file_info_list_free : nullptr );
    hash = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)free, nullptr);

    for (row = rows; row; row = row->next)
    {
        char* dir;
        GtkTreePath* tp = (GtkTreePath*)row->data;

        if (gtk_tree_model_get_iter(model, &it, tp))
        {
            if (open_files) /* open files */
                gtk_tree_model_get(model,
                                   &it,
                                   FindFilesCol::COL_INFO,
                                   &fi,
                                   FindFilesCol::COL_DIR,
                                   &dir,
                                   -1);
            else /* open containing directories */
                gtk_tree_model_get(model, &it, FindFilesCol::COL_DIR, &dir, -1);

            if (open_files)
            {
                GList* l;
                l = (GList*)g_hash_table_lookup(hash, dir);
                l = g_list_prepend(l, vfs_file_info_ref(fi));
                g_hash_table_insert(hash, dir, l); // sfm caused segfault with destroy function
                if (vfs_file_info_is_dir(fi))      // sfm
                    open_files_has_dir = true;
            }
            else
            {
                if (g_hash_table_lookup(hash, dir))
                    free(dir);
                g_hash_table_insert(hash, dir, nullptr);
            }
        }
        gtk_tree_path_free(tp);
    }
    g_list_free(rows);

    if (open_files)
    {
        if (open_files_has_dir)
        {
            w = GTK_WIDGET(fm_main_window_get_on_current_desktop());
            if (!w)
            {
                w = fm_main_window_new();
                // now done in fm_main_window_new
                // gtk_window_set_default_size( GTK_WINDOW( w ), app_settings.width,
                // app_settings.height );
            }
            gtk_window_present(GTK_WINDOW(w));
            file_browser = PTK_FILE_BROWSER_REINTERPRET(
                fm_main_window_get_current_file_browser(FM_MAIN_WINDOW_REINTERPRET(w)));
        }
        g_hash_table_foreach_steal(hash, (GHRFunc)open_file, file_browser);
    }
    else
    {
        w = GTK_WIDGET(fm_main_window_get_on_current_desktop());
        if (!w)
        {
            w = fm_main_window_new();
            // now done in fm_main_window_new
            // gtk_window_set_default_size( GTK_WINDOW( w ), app_settings.width, app_settings.height
            // );
        }

        g_hash_table_foreach(hash, (GHFunc)open_dir, w);
        gtk_window_present(GTK_WINDOW(w));
    }

    g_hash_table_destroy(hash);
}

static GtkActionEntry menu_actions[] = {
    {"OpenAction", "document-open", "_Open", nullptr, nullptr, G_CALLBACK(on_open_files)},
    {"OpenDirectoryAction",
     "document-open",
     "Open Containing _Directory",
     nullptr,
     nullptr,
     G_CALLBACK(on_open_files)}};

static int
get_date_offset(GtkCalendar* calendar)
{
    int offset;

    std::time_t timeval = std::time(nullptr);
    struct tm* lt = std::localtime(&timeval);

    unsigned int d, m, y;
    gtk_calendar_get_date(calendar, &y, &m, &d);

    Glib::Date date = Glib::Date(Glib::Date::Day(d), Glib::Date::Month(m), Glib::Date::Year(y));
    Glib::Date today = Glib::Date(Glib::Date::Day(lt->tm_mday),
                                  Glib::Date::Month(lt->tm_mon),
                                  Glib::Date::Year(lt->tm_year + 1900));

    offset = today.days_between(date);

    return std::abs(offset);
}

static std::vector<std::string>
compose_command(FindFile* data)
{
    std::vector<std::string> argv;
    std::string tmp;

    GtkTreeIter it;
    std::array<char, 4> size_units{'c', 'k', 'M', 'G'};

    argv.push_back("find");
    argv.push_back("-H");

    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(data->places_list), &it))
    {
        do
        {
            char* arg;
            gtk_tree_model_get(GTK_TREE_MODEL(data->places_list), &it, 0, &arg, -1);
            if (arg && *arg)
            {
                argv.push_back(arg);
                free(arg);
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(data->places_list), &it));
    }

    /* if include sub is excluded */ // MOD added
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->include_sub)))
    {
        argv.push_back("-maxdepth");
        argv.push_back("1");
    }

    /* if hidden files is excluded */
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->search_hidden)))
    {
        argv.push_back("-name");
        argv.push_back(".");
        argv.push_back("-prune");
        argv.push_back("-or");
    }

    /* if lower limit of file size is set */
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->use_size_lower)))
    {
        argv.push_back("-size");
        tmp = fmt::format(
            "+{}{}",
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->size_lower)),
            size_units.at(gtk_combo_box_get_active(GTK_COMBO_BOX(data->size_lower_unit))));
        argv.push_back(tmp);
    }

    /* if upper limit of file size is set */
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->use_size_upper)))
    {
        argv.push_back("-size");
        tmp = fmt::format(
            "-{}{}",
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->size_upper)),
            size_units.at(gtk_combo_box_get_active(GTK_COMBO_BOX(data->size_upper_unit))));
        argv.push_back(tmp);
    }

    /* If -name is used */
    tmp = ztd::null_check(gtk_entry_get_text(GTK_ENTRY(data->fn_pattern_entry)));
    if (ztd::contains(tmp, "*"))
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->fn_case_sensitive)))
            argv.push_back("-name");
        else
            argv.push_back("-iname");

        argv.push_back(tmp);
    }

    /* match by mtime */
    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(data->date_limit));
    if (idx > 0)
    {
        if (idx == 5) /* range */
        {
            argv.push_back("(");

            argv.push_back("-mtime");

            /* date1 */
            tmp = fmt::format("-{}", get_date_offset(GTK_CALENDAR(data->date1)));
            argv.push_back(tmp);

            argv.push_back("-mtime");

            /* date2 */
            tmp = fmt::format("+{}", get_date_offset(GTK_CALENDAR(data->date2)));
            argv.push_back(tmp);

            argv.push_back(")");
        }
        else
        {
            argv.push_back("-mtime");

            switch (idx)
            {
                case 1: /* within one day */
                    argv.push_back("-1");
                    break;
                case 2: /* within one week */
                    argv.push_back("-7");
                    break;
                case 3: /* within one month */
                    argv.push_back("-30");
                    break;
                case 4: /* within one year */
                    argv.push_back("-365");
                    break;
                default:
                    argv.push_back("-1");
                    break;
            }
        }
    }

    /* grep text inside files */
    bool print = false;
    tmp = ztd::null_check(gtk_entry_get_text(GTK_ENTRY(data->fc_pattern)));
    if (!tmp.empty())
    {
        print = true;

        /* ensure we only call 'grep' on regular files */
        argv.push_back("-type");
        argv.push_back("f");

        argv.push_back("-exec");
        argv.push_back("grep");

        if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->fc_case_sensitive)))
            argv.push_back("-i");

        argv.push_back("--files-with-matches");

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->fc_use_regexp)))
            argv.push_back("--regexp");
        else
            argv.push_back("--fixed-strings");

        argv.push_back(tmp);

        argv.push_back("{}");
        argv.push_back(";");
    }

    if (!print)
        argv.push_back("-print");

    return argv;
}

static void
finish_search(FindFile* data)
{
    if (data->pid)
    {
        int status;
        kill(data->pid, SIGTERM);
        waitpid(data->pid, &status, 0);
        data->pid = 0;
        LOG_DEBUG("find process is killed!");
    }
    if (data->task)
    {
        g_object_unref(data->task);
        data->task = nullptr;
    }
    gdk_window_set_cursor(gtk_widget_get_window(data->search_result), nullptr);
    gtk_widget_hide(data->stop_btn);
    gtk_widget_show(data->again_btn);
}

static void
process_found_files(FindFile* data, GQueue* queue, const char* path)
{
    GtkTreeIter it;
    VFSFileInfo* fi;
    GdkPixbuf* icon;
    FoundFile* ff;

    if (path)
    {
        const std::string name = Glib::filename_display_basename(path);
        fi = vfs_file_info_new();
        if (vfs_file_info_get(fi, path, name.c_str()))
        {
            ff = new FoundFile(fi, Glib::path_get_dirname(path));
            g_queue_push_tail(queue, ff);
        }
        else
        {
            vfs_file_info_unref(fi);
        }

        /* we queue the found files, and not add them to the tree view direclty.
         * when we queued more than 10 files, we add them at once. I think
         * this can prevent locking gtk+ too frequently and improve responsiveness.
         * FIXME: This could blocked the last queued files and delay their display
         * to the end of the whole search. A better method is needed.
         */
        // MOD disabled this - causes last queued files to not display
        //        if( g_queue_get_length( queue ) < 10 )
        //            return;
    }

    while ((ff = FOUND_FILE(g_queue_pop_head(queue))))
    {
        gtk_list_store_append(data->result_list, &it);
        icon = vfs_file_info_get_small_icon(ff->fi);
        gtk_list_store_set(data->result_list,
                           &it,
                           FindFilesCol::COL_ICON,
                           icon,
                           FindFilesCol::COL_NAME,
                           vfs_file_info_get_disp_name(ff->fi),
                           FindFilesCol::COL_DIR,
                           ff->dir_path.c_str(), /* FIXME: non-UTF8? */
                           FindFilesCol::COL_TYPE,
                           vfs_file_info_get_mime_type_desc(ff->fi),
                           FindFilesCol::COL_SIZE,
                           vfs_file_info_get_disp_size(ff->fi),
                           FindFilesCol::COL_MTIME,
                           vfs_file_info_get_disp_mtime(ff->fi),
                           FindFilesCol::COL_INFO,
                           ff->fi,
                           -1);
        g_object_unref(icon);

        delete ff;
    }
}

static void*
search_thread(VFSAsyncTask* task, FindFile* data)
{
    (void)task;
    ssize_t rlen;
    char buf[4096];
    std::string path;
    GQueue* queue = g_queue_new();

    while (!data->task->cancel && (rlen = read(data->stdo, buf, sizeof(buf) - 1)) > 0)
    {
        char* pbuf;
        char* eol;
        buf[rlen] = '\0';
        pbuf = buf;

        while (!data->task->cancel)
        {
            if ((eol = strchr(pbuf, '\n'))) /* end of line is reached */
            {
                *eol = '\0';
                path.append(pbuf);

                /* we get a complete file path */
                if (!data->task->cancel)
                    process_found_files(data, queue, path.c_str());

                pbuf = eol + 1;  /* start reading the next line */
                path.append(""); /* empty the line buffer */
            }
            else /* end of line is not reached */
            {
                /* store the partial path in the buffer */
                path.append(pbuf);
                break;
            }
        }
    }
    /* end of stream (EOF) is reached */
    if (!path.empty()) /* this is the last line without eol character '\n' */
    {
        if (!data->task->cancel)
        {
            process_found_files(data, queue, path.c_str());
            process_found_files(data, queue, nullptr);
        }
    }

    g_queue_free(queue);
    return nullptr;
}

static void
on_search_finish(VFSAsyncTask* task, bool cancelled, FindFile* data)
{
    (void)task;
    (void)cancelled;
    finish_search(data);
}

static void
on_start_search(GtkWidget* btn, FindFile* data)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(data->win), &allocation);
    int width = allocation.width;
    int height = allocation.height;
    if (width && height)
    {
        xset_set(XSetName::MAIN_SEARCH, XSetSetSet::X, std::to_string(width));
        xset_set(XSetName::MAIN_SEARCH, XSetSetSet::Y, std::to_string(height));
    }

    gtk_widget_hide(data->search_criteria);
    gtk_widget_show(data->search_result);

    gtk_widget_hide(btn);
    gtk_widget_show(data->stop_btn);

    std::vector<std::string> argv = compose_command(data);

    LOG_DEBUG("find command: {}", ztd::join(argv, " "));

    Glib::spawn_async_with_pipes(vfs_user_home_dir(),
                                 argv,
                                 Glib::SpawnFlags::SEARCH_PATH,
                                 Glib::SlotSpawnChildSetup(),
                                 &data->pid,
                                 nullptr,
                                 &data->stdo,
                                 nullptr);

    GdkCursor* busy_cursor;
    data->task = vfs_async_task_new((VFSAsyncFunc)search_thread, data);
    g_signal_connect(data->task, "finish", G_CALLBACK(on_search_finish), data);
    vfs_async_task_execute(data->task);

    busy_cursor = gdk_cursor_new_for_display(nullptr, GDK_WATCH);
    gdk_window_set_cursor(gtk_widget_get_window(data->search_result), busy_cursor);
    g_object_unref(busy_cursor);
}

static void
on_stop_search(GtkWidget* btn, FindFile* data)
{
    (void)btn;
    if (data->task && !vfs_async_task_is_finished(data->task))
    {
        // see note in vfs-async-task.c: vfs_async_task_real_cancel()
        vfs_async_task_cancel(data->task);
    }
}

static void
on_search_again(GtkWidget* btn, FindFile* data)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(data->win), &allocation);
    int width = allocation.width;
    int height = allocation.height;
    if (width && height)
    {
        xset_set(XSetName::MAIN_SEARCH, XSetSetSet::X, std::to_string(width));
        xset_set(XSetName::MAIN_SEARCH, XSetSetSet::Y, std::to_string(height));
    }

    gtk_widget_show(data->search_criteria);
    gtk_widget_hide(data->search_result);

    gtk_widget_hide(btn);
    gtk_widget_show(data->start_btn);

    g_object_ref(data->result_list);
    gtk_tree_view_set_model(GTK_TREE_VIEW(data->result_view), nullptr);
    gtk_list_store_clear(data->result_list);
    gtk_tree_view_set_model(GTK_TREE_VIEW(data->result_view), GTK_TREE_MODEL(data->result_list));
    g_object_unref(data->result_list);
}

static void
menu_pos(GtkMenu* menu, int* x, int* y, bool* push_in, GtkWidget* btn)
{
    (void)menu;
    GtkAllocation allocation;

    /* FIXME: I'm not sure if this work well in different WMs */
    gdk_window_get_position(gtk_widget_get_window(btn), x, y);
    /*    gdk_window_get_root_origin( btn->window, x, y); */
    gtk_widget_get_allocation(GTK_WIDGET(btn), &allocation);
    *x += allocation.x;
    *y += allocation.y + allocation.height;
    *push_in = false;
}

static void
add_search_dir(FindFile* data, const std::string& path)
{
    GtkTreeIter it;
    gtk_list_store_append(data->places_list, &it);
    gtk_list_store_set(data->places_list, &it, 0, path.c_str(), -1);
}

static void
on_add_search_browse(GtkWidget* menu, FindFile* data)
{
    (void)menu;
    GtkWidget* dlg = gtk_file_chooser_dialog_new("Select a directory",
                                                 GTK_WINDOW(data->win),
                                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                 "Cancel",
                                                 GTK_RESPONSE_CANCEL,
                                                 "document-open",
                                                 GTK_RESPONSE_OK,
                                                 nullptr);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
    {
        char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        add_search_dir(data, path);
        free(path);
    }
    gtk_widget_destroy(dlg);
}

static void
on_add_search_home(GtkWidget* menu, FindFile* data)
{
    (void)menu;
    add_search_dir(data, vfs_user_home_dir());
}

static void
on_add_search_desktop(GtkWidget* menu, FindFile* data)
{
    (void)menu;
    add_search_dir(data, vfs_user_desktop_dir());
}

static void
on_add_search_volumes(GtkWidget* menu, FindFile* data)
{
    (void)menu;
    const char* path;
    const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();
    for (VFSVolume* volume: volumes)
    {
        if (vfs_volume_is_mounted(volume))
        {
            path = vfs_volume_get_mount_point(volume);
            if (path && path[0] != '\0')
                add_search_dir(data, path);
        }
    }
}

static void
on_add_search_folder(GtkWidget* btn, FindFile* data)
{
    GtkWidget* menu = gtk_menu_new();
    GtkWidget* item;

    item = gtk_menu_item_new_with_label("Browse...");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(on_add_search_browse), data);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label(vfs_user_home_dir().c_str());
    // img = gtk_image_new_from_icon_name( "gnome-fs-directory", GTK_ICON_SIZE_MENU );
    // img = xset_get_image("gtk-directory", GTK_ICON_SIZE_MENU);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(on_add_search_home), data);

    item = gtk_menu_item_new_with_label(vfs_user_desktop_dir().c_str());
    // img = gtk_image_new_from_icon_name( "gnome-fs-desktop", GTK_ICON_SIZE_MENU );
    // img = xset_get_image("gtk-directory", GTK_ICON_SIZE_MENU);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(on_add_search_desktop), data);

    item = gtk_menu_item_new_with_label("Local Volumes");
    // img = gtk_image_new_from_icon_name( "gnome-dev-harddisk", GTK_ICON_SIZE_MENU );
    // img = xset_get_image("gtk-harddisk", GTK_ICON_SIZE_MENU);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(on_add_search_volumes), data);

    /* FIXME: Add all volumes */

    /* FIXME: Add all bookmarks */

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu),
                   nullptr,
                   nullptr,
                   (GtkMenuPositionFunc)menu_pos,
                   btn,
                   0,
                   gtk_get_current_event_time());
}

static void
on_remove_search_folder(GtkWidget* btn, FindFile* data)
{
    (void)btn;
    GtkTreeIter it;
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->places_view));
    if (gtk_tree_selection_get_selected(sel, nullptr, &it))
        gtk_list_store_remove(data->places_list, &it);
}

static void
on_date_limit_changed(GtkWidget* date_limit, FindFile* data)
{
    int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(date_limit));
    bool sensitive = (sel == 5); /* date range */
    gtk_widget_set_sensitive(data->date1, sensitive);
    gtk_widget_set_sensitive(data->date2, sensitive);
}

static void
free_data(FindFile* data)
{
    delete data;
}

static void
init_search_result(FindFile* data)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;

    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(data->result_view)),
                                GTK_SELECTION_MULTIPLE);
    data->result_list = gtk_list_store_new(FindFilesCol::N_RES_COLS,
                                           GDK_TYPE_PIXBUF, /* icon */
                                           G_TYPE_STRING,   /* name */
                                           G_TYPE_STRING,   /* dir */
                                           G_TYPE_STRING,   /* type */
                                           G_TYPE_STRING,   /* size */
                                           G_TYPE_STRING,   /* mtime */
                                           G_TYPE_POINTER /* VFSFileInfo */);

    gtk_tree_view_set_model(GTK_TREE_VIEW(data->result_view), GTK_TREE_MODEL(data->result_list));
    g_object_unref(data->result_list);
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Name");
    render = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, render, false);
    gtk_tree_view_column_set_attributes(col, render, "pixbuf", FindFilesCol::COL_ICON, nullptr);
    render = gtk_cell_renderer_text_new();
    g_object_set(render, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);
    gtk_tree_view_column_pack_start(col, render, true);
    gtk_tree_view_column_set_attributes(col, render, "text", FindFilesCol::COL_NAME, nullptr);
    gtk_tree_view_column_set_expand(col, true);
    gtk_tree_view_column_set_min_width(col, 200);
    gtk_tree_view_column_set_resizable(col, true);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->result_view), col);

    render = gtk_cell_renderer_text_new();
    g_object_set(render, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);
    col = gtk_tree_view_column_new_with_attributes("Directory",
                                                   render,
                                                   "text",
                                                   FindFilesCol::COL_DIR,
                                                   nullptr);
    gtk_tree_view_column_set_expand(col, true);
    gtk_tree_view_column_set_resizable(col, true);
    gtk_tree_view_column_set_min_width(col, 200);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->result_view), col);

    col = gtk_tree_view_column_new_with_attributes("Size",
                                                   gtk_cell_renderer_text_new(),
                                                   "text",
                                                   FindFilesCol::COL_SIZE,
                                                   nullptr);
    gtk_tree_view_column_set_resizable(col, true);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->result_view), col);

    col = gtk_tree_view_column_new_with_attributes("Type",
                                                   gtk_cell_renderer_text_new(),
                                                   "text",
                                                   FindFilesCol::COL_TYPE,
                                                   nullptr);
    gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(col, 120);
    gtk_tree_view_column_set_resizable(col, true);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->result_view), col);

    col = gtk_tree_view_column_new_with_attributes("Last Modified",
                                                   gtk_cell_renderer_text_new(),
                                                   "text",
                                                   FindFilesCol::COL_MTIME,
                                                   nullptr);
    gtk_tree_view_column_set_resizable(col, true);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->result_view), col);
}

static bool
on_view_button_press(GtkTreeView* view, GdkEventButton* evt, FindFile* data)
{
    if (evt->type == GDK_BUTTON_PRESS)
    {
        if (evt->button == 3) /* right single click */
        {
            // sfm if current item not selected, unselect all and select it
            GtkTreePath* tree_path;
            GtkTreeSelection* tree_sel;
            gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          evt->x,
                                          evt->y,
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr);
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

            if (tree_path && tree_sel && !gtk_tree_selection_path_is_selected(tree_sel, tree_path))
            {
                gtk_tree_selection_unselect_all(tree_sel);
                gtk_tree_selection_select_path(tree_sel, tree_path);
            }
            gtk_tree_path_free(tree_path);

            GtkWidget* popup;
            GtkUIManager* menu_mgr;
            GtkActionGroup* action_group = gtk_action_group_new("PopupActions");
            menu_mgr = gtk_ui_manager_new();

            gtk_action_group_add_actions(action_group,
                                         menu_actions,
                                         G_N_ELEMENTS(menu_actions),
                                         data);
            gtk_ui_manager_insert_action_group(menu_mgr, action_group, 0);
            gtk_ui_manager_add_ui_from_string(menu_mgr, menu_def, -1, nullptr);

            popup = gtk_ui_manager_get_widget(menu_mgr, "/Popup");
            g_object_unref(action_group);
            gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);

            /* clean up */
            g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
            g_object_weak_ref(G_OBJECT(popup), (GWeakNotify)g_object_unref, menu_mgr);

            return true;
        }
    }
    else if (evt->type == GDK_2BUTTON_PRESS)
    {
        if (evt->button == 1) /* left double click */
        {
            on_open_files(nullptr, data);
            return true;
        }
    }
    return false;
}

void
on_use_size_lower_toggled(GtkWidget* widget, FindFile* data)
{
    (void)widget;
    gtk_widget_set_sensitive(data->size_lower,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->use_size_lower)));
    gtk_widget_set_sensitive(data->size_lower_unit,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->use_size_lower)));
}

void
on_use_size_upper_toggled(GtkWidget* widget, FindFile* data)
{
    (void)widget;
    gtk_widget_set_sensitive(data->size_upper,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->use_size_upper)));
    gtk_widget_set_sensitive(data->size_upper_unit,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->use_size_upper)));
}

void
fm_find_files(std::vector<const char*> search_dirs)
{
    FindFile* data = new FindFile;
    GtkTreeIter it;
    GtkTreeViewColumn* col;
    GtkWidget* add_directory_btn;
    GtkWidget* remove_directory_btn;

    GtkBuilder* builder = _gtk_builder_new_from_file("find-files3.ui");
    data->win = GTK_WIDGET(gtk_builder_get_object(builder, "win"));
    g_object_set_data_full(G_OBJECT(data->win), "find-files", data, (GDestroyNotify)free_data);

    GdkPixbuf* icon = nullptr;
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    if (theme)
        icon = gtk_icon_theme_load_icon(theme,
                                        "spacefm-find",
                                        48,
                                        GtkIconLookupFlags::GTK_ICON_LOOKUP_NO_SVG,
                                        nullptr);
    if (icon)
    {
        gtk_window_set_icon(GTK_WINDOW(data->win), icon);
        g_object_unref(icon);
    }
    else
        gtk_window_set_icon_name(GTK_WINDOW(data->win), "Find");

    /* search criteria pane */
    data->search_criteria = GTK_WIDGET(gtk_builder_get_object(builder, "search_criteria"));

    data->fn_pattern = GTK_WIDGET(gtk_builder_get_object(builder, "fn_pattern"));
    data->fn_pattern_entry = gtk_bin_get_child(GTK_BIN(data->fn_pattern));
    data->fn_case_sensitive = GTK_WIDGET(gtk_builder_get_object(builder, "fn_case_sensitive"));
    gtk_entry_set_activates_default(GTK_ENTRY(data->fn_pattern_entry), true);

    /* file content */
    data->fc_pattern = GTK_WIDGET(gtk_builder_get_object(builder, "fc_pattern"));
    data->fc_case_sensitive = GTK_WIDGET(gtk_builder_get_object(builder, "fc_case_sensitive"));
    data->fc_use_regexp = GTK_WIDGET(gtk_builder_get_object(builder, "fc_use_regexp"));

    /* advanced options */
    data->search_hidden = GTK_WIDGET(gtk_builder_get_object(builder, "search_hidden"));

    /* size & date */
    data->use_size_lower = GTK_WIDGET(gtk_builder_get_object(builder, "use_size_lower"));
    data->use_size_upper = GTK_WIDGET(gtk_builder_get_object(builder, "use_size_upper"));
    data->size_lower = GTK_WIDGET(gtk_builder_get_object(builder, "size_lower"));
    data->size_upper = GTK_WIDGET(gtk_builder_get_object(builder, "size_upper"));
    data->size_lower_unit = GTK_WIDGET(gtk_builder_get_object(builder, "size_lower_unit"));
    data->size_upper_unit = GTK_WIDGET(gtk_builder_get_object(builder, "size_upper_unit"));
    g_signal_connect(data->use_size_lower, "toggled", G_CALLBACK(on_use_size_lower_toggled), data);
    g_signal_connect(data->use_size_upper, "toggled", G_CALLBACK(on_use_size_upper_toggled), data);
    on_use_size_lower_toggled(data->use_size_lower, data);
    on_use_size_upper_toggled(data->use_size_upper, data);

    data->date_limit = GTK_WIDGET(gtk_builder_get_object(builder, "date_limit"));
    data->date1 = GTK_WIDGET(gtk_builder_get_object(builder, "date1"));
    data->date2 = GTK_WIDGET(gtk_builder_get_object(builder, "date2"));
    g_signal_connect(data->date_limit, "changed", G_CALLBACK(on_date_limit_changed), data);

    /* file types */
    data->all_files = GTK_WIDGET(gtk_builder_get_object(builder, "all_files"));
    data->text_files = GTK_WIDGET(gtk_builder_get_object(builder, "text_files"));
    data->img_files = GTK_WIDGET(gtk_builder_get_object(builder, "img_files"));
    data->audio_files = GTK_WIDGET(gtk_builder_get_object(builder, "audio_files"));
    data->video_files = GTK_WIDGET(gtk_builder_get_object(builder, "video_files"));

    /* places */
    data->places_list = gtk_list_store_new(1, G_TYPE_STRING);
    data->places_view = GTK_WIDGET(gtk_builder_get_object(builder, "places_view"));
    add_directory_btn = GTK_WIDGET(gtk_builder_get_object(builder, "add_directory_btn"));
    remove_directory_btn = GTK_WIDGET(gtk_builder_get_object(builder, "remove_directory_btn"));
    data->include_sub = GTK_WIDGET(gtk_builder_get_object(builder, "include_sub"));

    for (const char* dir: search_dirs)
    {
        if (std::filesystem::is_directory(dir))
            gtk_list_store_insert_with_values(data->places_list, &it, 0, 0, dir, -1);
    }

    gtk_tree_view_set_model(GTK_TREE_VIEW(data->places_view), GTK_TREE_MODEL(data->places_list));
    g_object_unref(data->places_list);
    col = gtk_tree_view_column_new_with_attributes(nullptr,
                                                   gtk_cell_renderer_text_new(),
                                                   "text",
                                                   0,
                                                   nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->places_view), col);

    g_signal_connect(add_directory_btn, "clicked", G_CALLBACK(on_add_search_folder), data);
    g_signal_connect(remove_directory_btn, "clicked", G_CALLBACK(on_remove_search_folder), data);

    /* search result pane */
    data->search_result = GTK_WIDGET(gtk_builder_get_object(builder, "search_result"));
    /* replace the problematic GtkTreeView with GtkTreeView */
    data->result_view = gtk_tree_view_new();
    /*
    if (app_settings.single_click)
    {
        gtk_tree_view_set_single_click(GTK_TREE_VIEW(data->result_view), true);
        gtk_tree_view_set_single_click_timeout(GTK_TREE_VIEW(data->result_view),
                                               SINGLE_CLICK_TIMEOUT);
    }
     */
    gtk_widget_show(data->result_view);
    gtk_container_add(GTK_CONTAINER(gtk_builder_get_object(builder, "result_scroll")),
                      data->result_view);
    init_search_result(data);
    g_signal_connect(data->result_view,
                     "button-press-event",
                     G_CALLBACK(on_view_button_press),
                     data);

    /* buttons */
    data->start_btn = GTK_WIDGET(gtk_builder_get_object(builder, "start_btn"));
    data->stop_btn = GTK_WIDGET(gtk_builder_get_object(builder, "stop_btn"));
    data->again_btn = GTK_WIDGET(gtk_builder_get_object(builder, "again_btn"));

    g_signal_connect(data->start_btn, "clicked", G_CALLBACK(on_start_search), data);
    g_signal_connect(data->stop_btn, "clicked", G_CALLBACK(on_stop_search), data);
    g_signal_connect(data->again_btn, "clicked", G_CALLBACK(on_search_again), data);

    gtk_entry_set_text(GTK_ENTRY(data->fn_pattern_entry), "*");
    gtk_editable_select_region(GTK_EDITABLE(data->fn_pattern_entry), 0, -1);

    gtk_combo_box_set_active(GTK_COMBO_BOX(data->size_lower_unit), 1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(data->size_lower), 0, G_MAXINT);
    gtk_combo_box_set_active(GTK_COMBO_BOX(data->size_upper_unit), 2);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(data->size_upper), 0, G_MAXINT);

    gtk_combo_box_set_active(GTK_COMBO_BOX(data->date_limit), 0);

    g_signal_connect(data->win, "delete-event", G_CALLBACK(gtk_widget_destroy), nullptr);

    WindowReference::increase();
    g_signal_connect(data->win, "destroy", G_CALLBACK(WindowReference::decrease), nullptr);

    int width = xset_get_int(XSetName::MAIN_SEARCH, XSetSetSet::X);
    int height = xset_get_int(XSetName::MAIN_SEARCH, XSetSetSet::Y);
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(data->win), width, height);

    gtk_widget_show(data->win);
}
