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
#include <string_view>

#include <filesystem>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <magic_enum.hpp>

#include "vfs/vfs-user-dir.hxx"

#include "ptk/ptk-path-entry.hxx"

#include "main-window.hxx"

#include "ptk/ptk-bookmark-view.hxx"
#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-location-view.hxx"

static void on_changed(GtkEntry* entry, void* user_data);

enum PTKPathEntryCol
{
    COL_NAME,
    COL_PATH,
};

EntryData::EntryData(PtkFileBrowser* file_browser)
{
    this->browser = file_browser;
    this->seek_timer = 0;
}

static char*
get_cwd(GtkEntry* entry)
{
    const char* path = gtk_entry_get_text(entry);
    if (path[0] == '/')
    {
        const std::string cwd = Glib::path_get_dirname(path);
        return ztd::strdup(cwd);
    }
    else if (path[0] != '$' && path[0] != '+' && path[0] != '&' && path[0] != '!' &&
             path[0] != '\0' && path[0] != ' ')
    {
        EntryData* edata = ENTRY_DATA(g_object_get_data(G_OBJECT(entry), "edata"));
        if (edata && edata->browser)
        {
            const std::string real_path = std::filesystem::absolute(path);
            const std::string ret = Glib::path_get_dirname(real_path);
            return ztd::strdup(ret);
        }
    }
    return nullptr;
}

static bool
seek_path(GtkEntry* entry)
{
    if (!GTK_IS_ENTRY(entry))
        return false;
    EntryData* edata = ENTRY_DATA(g_object_get_data(G_OBJECT(entry), "edata"));
    if (!(edata && edata->browser))
        return false;
    if (edata->seek_timer)
    {
        g_source_remove(edata->seek_timer);
        edata->seek_timer = 0;
    }

    if (!xset_get_b(XSetName::PATH_SEEK))
        return false;

    char* seek_dir;
    std::string seek_name;
    const char* path = gtk_entry_get_text(entry);
    if (!path || path[0] == '$' || path[0] == '+' || path[0] == '&' || path[0] == '!' ||
        path[0] == '\0' || path[0] == ' ' || path[0] == '%')
        return false;

    // get dir and name prefix
    seek_dir = get_cwd(entry);
    if (!(seek_dir && std::filesystem::is_directory(seek_dir)))
    {
        // entry does not contain a valid dir
        free(seek_dir);
        return false;
    }
    if (!ztd::endswith(path, "/"))
    {
        // get name prefix
        seek_name = Glib::path_get_basename(path);
        const std::string test_path = Glib::build_filename(seek_dir, seek_name);
        if (std::filesystem::is_directory(test_path))
        {
            // complete dir path is in entry - is it unique?
            int count = 0;

            std::string file_name;
            for (const auto& file: std::filesystem::directory_iterator(seek_dir))
            {
                if (count == 2)
                    break;

                file_name = std::filesystem::path(file).filename();

                if (ztd::startswith(file_name, seek_name))
                {
                    const std::string full_path = Glib::build_filename(seek_dir, file_name);
                    if (std::filesystem::is_directory(full_path))
                        count++;
                }
            }

            if (count == 1)
            {
                // is unique - use as seek dir
                free(seek_dir);
                seek_dir = ztd::strdup(test_path);
            }
        }
    }
    if (strcmp(seek_dir, "/") && ztd::endswith(seek_dir, "/"))
    {
        // strip trialing slash
        seek_dir[std::strlen(seek_dir) - 1] = '\0';
    }
    ptk_file_browser_seek_path(edata->browser, seek_dir, seek_name.c_str());
    free(seek_dir);
    return false;
}

static void
seek_path_delayed(GtkEntry* entry, unsigned int delay)
{
    EntryData* edata = ENTRY_DATA(g_object_get_data(G_OBJECT(entry), "edata"));
    if (!(edata && edata->browser))
        return;
    // user is still typing - restart timer
    if (edata->seek_timer)
        g_source_remove(edata->seek_timer);
    edata->seek_timer = g_timeout_add(delay ? delay : 250, (GSourceFunc)seek_path, entry);
}

static bool
match_func_cmd(GtkEntryCompletion* completion, const char* key, GtkTreeIter* it, void* user_data)
{
    (void)user_data;
    char* name = nullptr;
    GtkTreeModel* model = gtk_entry_completion_get_model(completion);
    gtk_tree_model_get(model, it, PTKPathEntryCol::COL_NAME, &name, -1);

    if (name && key && ztd::startswith(name, key))
    {
        free(name);
        return true;
    }
    free(name);
    return false;
}

static bool
match_func(GtkEntryCompletion* completion, const char* key, GtkTreeIter* it, void* user_data)
{
    (void)user_data;
    char* name = nullptr;
    GtkTreeModel* model = gtk_entry_completion_get_model(completion);

    key = (const char*)g_object_get_data(G_OBJECT(completion), "fn");
    gtk_tree_model_get(model, it, PTKPathEntryCol::COL_NAME, &name, -1);

    if (name)
    {
        if (*key == 0 || g_ascii_strncasecmp(name, key, std::strlen(key)) == 0)
        {
            free(name);
            return true;
        }
        free(name);
    }
    return false;
}

static void
update_completion(GtkEntry* entry, GtkEntryCompletion* completion)
{
    GtkListStore* list;
    GtkTreeIter it;

    const char* text = gtk_entry_get_text(entry);
    if (text &&
        (text[0] == '$' || text[0] == '+' || text[0] == '&' || text[0] == '!' || text[0] == '%' ||
         (text[0] != '/' && strstr(text, ":/")) || ztd::startswith(text, "//")))
    {
        // command history
        list = GTK_LIST_STORE(gtk_entry_completion_get_model(completion));
        gtk_list_store_clear(list);
        for (std::string_view cmd: xset_cmd_history)
        {
            gtk_list_store_append(list, &it);
            gtk_list_store_set(list,
                               &it,
                               PTKPathEntryCol::COL_NAME,
                               cmd.data(),
                               PTKPathEntryCol::COL_PATH,
                               cmd.data(),
                               -1);
        }
        gtk_entry_completion_set_match_func(completion,
                                            (GtkEntryCompletionMatchFunc)match_func_cmd,
                                            nullptr,
                                            nullptr);
    }
    else
    {
        // dir completion
        char* fn;

        const char* sep = strrchr(text, '/');
        if (sep)
            fn = (char*)sep + 1;
        else
            fn = (char*)text;

        g_object_set_data_full(G_OBJECT(completion), "fn", ztd::strdup(fn), (GDestroyNotify)free);

        char* new_dir = get_cwd(entry);
        const char* old_dir = (const char*)g_object_get_data(G_OBJECT(completion), "cwd");
        if (old_dir && new_dir && g_ascii_strcasecmp(old_dir, new_dir) == 0)
        {
            free(new_dir);
            return;
        }
        g_object_set_data_full(G_OBJECT(completion), "cwd", new_dir, free);
        list = GTK_LIST_STORE(gtk_entry_completion_get_model(completion));
        gtk_list_store_clear(list);
        if (new_dir)
        {
            if (std::filesystem::is_directory(new_dir))
            {
                GSList* name_list = nullptr;

                for (const auto& file: std::filesystem::directory_iterator(new_dir))
                {
                    const std::string file_name = std::filesystem::path(file).filename();
                    const std::string full_path = Glib::build_filename(new_dir, file_name);
                    if (std::filesystem::is_directory(full_path))
                        name_list = g_slist_prepend(name_list, ztd::strdup(full_path));
                }

                // add sorted list to liststore
                name_list = g_slist_sort(name_list, (GCompareFunc)g_strcmp0);
                for (GSList* l = name_list; l; l = l->next)
                {
                    const std::string disp_name = Glib::filename_display_basename((char*)l->data);
                    gtk_list_store_append(list, &it);
                    gtk_list_store_set(list,
                                       &it,
                                       PTKPathEntryCol::COL_NAME,
                                       disp_name.c_str(),
                                       PTKPathEntryCol::COL_PATH,
                                       (char*)l->data,
                                       -1);
                    free((char*)l->data);
                }
                g_slist_free(name_list);

                gtk_entry_completion_set_match_func(completion,
                                                    (GtkEntryCompletionMatchFunc)match_func,
                                                    nullptr,
                                                    nullptr);
            }
            else
            {
                gtk_entry_completion_set_match_func(completion, nullptr, nullptr, nullptr);
            }
        }
    }
}

static void
on_changed(GtkEntry* entry, void* user_data)
{
    (void)user_data;
    GtkEntryCompletion* completion = gtk_entry_get_completion(entry);
    update_completion(entry, completion);
    gtk_entry_completion_complete(gtk_entry_get_completion(GTK_ENTRY(entry)));
    seek_path_delayed(GTK_ENTRY(entry), 0);
}

static void
insert_complete(GtkEntry* entry)
{
    // find a real completion
    const char* prefix = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!prefix)
        return;

    const char* dir_path = get_cwd(entry);
    if (!std::filesystem::is_directory(dir_path))
        return;

    // find longest common prefix
    int count = 0;
    int i;
    std::string last_path;
    std::string prefix_name;
    std::string full_path;
    std::string long_prefix;

    if (!ztd::endswith(prefix, "/"))
        prefix_name = Glib::path_get_basename(prefix);

    for (const auto& file: std::filesystem::directory_iterator(dir_path))
    {
        const std::string file_name = std::filesystem::path(file).filename();

        full_path = Glib::build_filename(dir_path, file_name);
        if (std::filesystem::is_directory(full_path))
        {
            if (prefix_name.empty())
            {
                // full match
                last_path = full_path;
                full_path = nullptr;
                if (++count > 1)
                    break;
            }
            else if (ztd::startswith(file_name, prefix_name))
            {
                // prefix matches
                count++;
                if (long_prefix.empty())
                    long_prefix = ztd::strdup(file_name);
                else
                {
                    i = 0;
                    while (file_name[i] && file_name[i] == long_prefix[i])
                        i++;
                    if (i && long_prefix[i])
                    {
                        // shorter prefix found
                        long_prefix = file_name;
                    }
                }
            }
        }
    }

    std::string new_prefix;
    if (prefix_name.empty() && count == 1)
        new_prefix = fmt::format("{}/", last_path);
    else if (!long_prefix.empty())
    {
        full_path = Glib::build_filename(dir_path, long_prefix);
        if (count == 1 && std::filesystem::is_directory(full_path))
            new_prefix = fmt::format("{}/", full_path);
        else
            new_prefix = full_path;
    }
    if (!new_prefix.empty())
    {
        g_signal_handlers_block_matched(G_OBJECT(entry),
                                        G_SIGNAL_MATCH_FUNC,
                                        0,
                                        0,
                                        nullptr,
                                        (void*)on_changed,
                                        nullptr);
        gtk_entry_set_text(GTK_ENTRY(entry), new_prefix.c_str());
        gtk_editable_set_position(GTK_EDITABLE(entry), -1);
        g_signal_handlers_unblock_matched(G_OBJECT(entry),
                                          G_SIGNAL_MATCH_FUNC,
                                          0,
                                          0,
                                          nullptr,
                                          (void*)on_changed,
                                          nullptr);
    }
}

static bool
on_key_press(GtkWidget* entry, GdkEventKey* evt, EntryData* edata)
{
    (void)edata;

    unsigned int keymod = ptk_get_keymod(evt->state);

    switch (evt->keyval)
    {
        case GDK_KEY_Tab:
            if (!keymod)
            {
                insert_complete(GTK_ENTRY(entry));
                on_changed(GTK_ENTRY(entry), nullptr);
                seek_path_delayed(GTK_ENTRY(entry), 10);
                return true;
            }
            break;
        case GDK_KEY_BackSpace:
            // shift
            if (keymod == 1)
            {
                gtk_entry_set_text(GTK_ENTRY(entry), "");
                return true;
            }
            break;
        default:
            break;
    }

    return false;
}

static bool
on_insert_prefix(GtkEntryCompletion* completion, char* prefix, GtkWidget* entry)
{
    (void)completion;
    (void)prefix;
    (void)entry;
    // do not use the default handler because it inserts partial names
    return true;
}

static bool
on_match_selected(GtkEntryCompletion* completion, GtkTreeModel* model, GtkTreeIter* iter,
                  GtkWidget* entry)
{
    (void)completion;
    char* path = nullptr;
    gtk_tree_model_get(model, iter, PTKPathEntryCol::COL_PATH, &path, -1);
    if (path && path[0])
    {
        g_signal_handlers_block_matched(G_OBJECT(entry),
                                        G_SIGNAL_MATCH_FUNC,
                                        0,
                                        0,
                                        nullptr,
                                        (void*)on_changed,
                                        nullptr);

        gtk_entry_set_text(GTK_ENTRY(entry), path);
        free(path);
        gtk_editable_set_position(GTK_EDITABLE(entry), -1);
        g_signal_handlers_unblock_matched(G_OBJECT(entry),
                                          G_SIGNAL_MATCH_FUNC,
                                          0,
                                          0,
                                          nullptr,
                                          (void*)on_changed,
                                          nullptr);
        on_changed(GTK_ENTRY(entry), nullptr);
        seek_path_delayed(GTK_ENTRY(entry), 10);
    }
    return true;
}

static bool
on_focus_in(GtkWidget* entry, GdkEventFocus* evt, void* user_data)
{
    (void)evt;
    (void)user_data;
    GtkEntryCompletion* completion = gtk_entry_completion_new();
    GtkListStore* list =
        gtk_list_store_new(magic_enum::enum_count<PTKPathEntryCol>(), G_TYPE_STRING, G_TYPE_STRING);

    gtk_entry_completion_set_minimum_key_length(completion, 1);
    gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(list));
    g_object_unref(list);

    /* gtk_entry_completion_set_text_column( completion, PTKPathEntryCol::COL_PATH ); */

    // Following line causes GTK3 to show both columns, so skip this and use
    // custom match-selected handler to insert PTKPathEntryCol::COL_PATH
    // g_object_set( completion, "text-column", PTKPathEntryCol::COL_PATH, nullptr );
    GtkCellRenderer* render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start((GtkCellLayout*)completion, render, true);
    gtk_cell_layout_add_attribute((GtkCellLayout*)completion,
                                  render,
                                  "text",
                                  PTKPathEntryCol::COL_NAME);

    // gtk_entry_completion_set_inline_completion( completion, true );
    gtk_entry_completion_set_popup_set_width(completion, true);
    gtk_entry_set_completion(GTK_ENTRY(entry), completion);
    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(on_changed), nullptr);
    g_signal_connect(G_OBJECT(completion), "match-selected", G_CALLBACK(on_match_selected), entry);
    g_signal_connect(G_OBJECT(completion), "insert-prefix", G_CALLBACK(on_insert_prefix), entry);
    g_object_unref(completion);

    return false;
}

static bool
on_focus_out(GtkWidget* entry, GdkEventFocus* evt, void* user_data)
{
    (void)evt;
    (void)user_data;
    g_signal_handlers_disconnect_by_func(entry, (void*)on_changed, nullptr);
    gtk_entry_set_completion(GTK_ENTRY(entry), nullptr);
    return false;
}

static void
on_protocol_handlers(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    (void)widget;
    ptk_handler_show_config(PtkHandlerMode::HANDLER_MODE_NET, file_browser, nullptr);
}

static void
on_add_bookmark(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    (void)widget;
    if (!(file_browser && file_browser->path_bar))
        return;
    const char* text = gtk_entry_get_text(GTK_ENTRY(file_browser->path_bar));
    if (text && text[0])
        ptk_bookmark_view_add_bookmark(nullptr, file_browser, text);
}

void
ptk_path_entry_help(GtkWidget* widget, GtkWidget* parent)
{
    (void)widget;
    GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(parent));
    GtkWidget* dlg = gtk_message_dialog_new(
        GTK_WINDOW(parent_win),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "In addition to a directory or file path, commands can be entered in the Path Bar.  "
        "Prefixes:\n\t$\trun as task\n\t&\trun and forget\n\t+\trun in terminal\n\t!\trun as "
        "root\nUse:\n\t%%F\tselected files  or  %%f first selected file\n\t%%N\tselected "
        "filenames  or  %%n first selected filename\n\t%%d\tcurrent directory\n\t%%v\tselected "
        "device (eg /dev/sda1)\n\t%%m\tdevice mount point (eg /media/dvd);  %%l device "
        "label\n\t%%b\tselected bookmark\n\t%%t\tselected task directory;  %%p task "
        "pid\n\t%%a\tmenu item value\n\t$fm_panel, $fm_tab, etc\n\nExample:  $ echo "
        "\"Current Directory: %%d\"\nExample:  +! umount %%v");
    gtk_window_set_title(GTK_WINDOW(dlg), "Path Bar Help");
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static bool
on_button_press(GtkWidget* entry, GdkEventButton* evt, void* user_data)
{
    (void)entry;
    (void)user_data;
    if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
        main_window_event(nullptr,
                          event_handler.win_click,
                          XSetName::EVT_WIN_CLICK,
                          0,
                          0,
                          "pathbar",
                          0,
                          evt->button,
                          evt->state,
                          true))
        return true;
    return false;
}

static bool
on_button_release(GtkEntry* entry, GdkEventButton* evt, void* user_data)
{
    (void)user_data;
    if (GDK_BUTTON_RELEASE != evt->type)
        return false;

    unsigned int keymod = ptk_get_keymod(evt->state);

    if (1 == evt->button && keymod == GDK_CONTROL_MASK)
    {
        const char* text = gtk_entry_get_text(entry);
        if (!(text[0] == '$' || text[0] == '+' || text[0] == '&' || text[0] == '!' ||
              text[0] == '%' || text[0] == '\0'))
        {
            int pos = gtk_editable_get_position(GTK_EDITABLE(entry));
            if (text && *text)
            {
                const char* sep = g_utf8_offset_to_pointer(text, pos);
                if (sep)
                {
                    while (*sep && *sep != '/')
                        sep = g_utf8_next_char(sep);
                    if (sep == text)
                    {
                        if ('/' == *sep)
                            ++sep;
                        else
                            return false;
                    }
                    char* path = strndup(text, (sep - text));
                    gtk_entry_set_text(entry, path);
                    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
                    free(path);
                    gtk_widget_activate(GTK_WIDGET(entry));
                }
            }
        }
    }
    else if (2 == evt->button && keymod == 0)
    {
        /* Middle-click - replace path bar contents with primary clipboard
         * contents and activate */
        GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
        char* clip_text = gtk_clipboard_wait_for_text(clip);
        if (clip_text && clip_text[0])
        {
            const std::string str = ztd::replace(clip_text, "\n", "");
            gtk_entry_set_text(entry, str.c_str());
            gtk_widget_activate(GTK_WIDGET(entry));
        }
        free(clip_text);
        return true;
    }
    return false;
}

static void
on_populate_popup(GtkEntry* entry, GtkMenu* menu, PtkFileBrowser* file_browser)
{
    if (!file_browser)
        return;
    XSetContext* context = xset_context_new();
    main_context_fill(file_browser, context);

    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_t set = xset_get(XSetName::SEPARATOR);
    xset_add_menuitem(file_browser, GTK_WIDGET(menu), accel_group, set);

    // New Bookmark
    set = xset_set_cb(XSetName::BOOK_ADD, (GFunc)on_add_bookmark, file_browser);
    const char* text = gtk_entry_get_text(GTK_ENTRY(entry));
    set->disable = !(text && (std::filesystem::exists(text) || strstr(text, ":/") ||
                              ztd::startswith(text, "//")));
    xset_add_menuitem(file_browser, GTK_WIDGET(menu), accel_group, set);

    set = xset_get(XSetName::PATH_SEEK);
    xset_add_menuitem(file_browser, GTK_WIDGET(menu), accel_group, set);
    set = xset_set_cb(XSetName::PATH_HAND, (GFunc)on_protocol_handlers, file_browser);
    xset_add_menuitem(file_browser, GTK_WIDGET(menu), accel_group, set);
    gtk_widget_show_all(GTK_WIDGET(menu));
    g_signal_connect(menu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
}

static void
on_entry_insert(GtkEntryBuffer* buf, unsigned int position, char* chars, unsigned int n_chars,
                void* user_data)
{
    (void)position;
    (void)chars;
    (void)n_chars;
    (void)user_data;
    const char* text = gtk_entry_buffer_get_text(buf);
    if (!text)
        return;

    char* new_text = nullptr;

    std::string cleaned;

    if (strchr(text, '\n'))
    {
        // remove linefeeds from pasted text
        cleaned = ztd::replace(text, "\n", "");
        text = new_text = ztd::strdup(cleaned);
    }

    // remove leading spaces for test
    while (text[0] == ' ')
        text++;

    if (text[0] == '\'' && ztd::endswith(text, "'") && text[1] != '\0')
    {
        // path is quoted - assume bash quote
        char* unquote = ztd::strdup(text + 1);
        unquote[std::strlen(unquote) - 1] = '\0';
        cleaned = ztd::replace(unquote, "'\\''", "'");
        new_text = ztd::strdup(cleaned);
        free(unquote);
    }

    if (new_text)
        gtk_entry_buffer_set_text(buf, new_text, -1);
}

static void
entry_data_free(EntryData* edata)
{
    delete edata;
}

GtkWidget*
ptk_path_entry_new(PtkFileBrowser* file_browser)
{
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_has_frame(GTK_ENTRY(entry), true);
    gtk_widget_set_size_request(entry, 50, -1);

    EntryData* edata = new EntryData(file_browser);

    g_signal_connect(entry, "focus-in-event", G_CALLBACK(on_focus_in), nullptr);
    g_signal_connect(entry, "focus-out-event", G_CALLBACK(on_focus_out), nullptr);

    /* used to eat the tab key */
    g_signal_connect(entry, "key-press-event", G_CALLBACK(on_key_press), edata);

    /*
        g_signal_connect( entry, "motion-notify-event", G_CALLBACK(on_mouse_move), nullptr );
    */
    g_signal_connect(entry, "button-press-event", G_CALLBACK(on_button_press), nullptr);
    g_signal_connect(entry, "button-release-event", G_CALLBACK(on_button_release), nullptr);
    g_signal_connect(entry, "populate-popup", G_CALLBACK(on_populate_popup), file_browser);

    g_signal_connect_after(G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(entry))),
                           "inserted-text",
                           G_CALLBACK(on_entry_insert),
                           nullptr);

    g_object_weak_ref(G_OBJECT(entry), (GWeakNotify)entry_data_free, edata);
    g_object_set_data(G_OBJECT(entry), "edata", edata);
    return entry;
}
