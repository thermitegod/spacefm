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

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-file-misc.hxx"
#include "ptk/ptk-utils.hxx"

#include "utils.hxx"

#include "ptk/ptk-clipboard.hxx"

static GdkDragAction clipboard_action = GDK_ACTION_DEFAULT;
static GList* clipboard_file_list = nullptr;

static void
clipboard_get_data(GtkClipboard* clipboard, GtkSelectionData* selection_data, unsigned int info,
                   void* user_data)
{
    (void)clipboard;
    (void)info;
    (void)user_data;
    GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);

    bool use_uri = false;

    if (!clipboard_file_list)
        return;

    GString* list = g_string_sized_new(8192);

    if (gtk_selection_data_get_target(selection_data) == gnome_target)
    {
        const char* action = clipboard_action == GDK_ACTION_MOVE ? "cut\n" : "copy\n";
        g_string_append(list, action);
        use_uri = true;
    }
    else if (gtk_selection_data_get_target(selection_data) == uri_list_target)
        use_uri = true;

    GList* l;
    for (l = clipboard_file_list; l; l = l->next)
    {
        char* file_name;
        if (use_uri)
        {
            file_name = g_filename_to_uri((char*)l->data, nullptr, nullptr);
        }
        else
        {
            file_name = g_filename_display_name((char*)l->data);
        }
        g_string_append(list, file_name);
        free(file_name);

        if (gtk_selection_data_get_target(selection_data) != uri_list_target)
            g_string_append_c(list, '\n');
        else
            g_string_append(list, "\n");
    }

    gtk_selection_data_set(selection_data,
                           gtk_selection_data_get_target(selection_data),
                           8,
                           (unsigned char*)list->str,
                           list->len + 1);
    // LOG_DEBUG("clipboard data: \n\n{}\n\n", list->str);
    g_string_free(list, true);
}

static void
clipboard_clean_data(GtkClipboard* clipboard, void* user_data)
{
    (void)clipboard;
    (void)user_data;
    // LOG_DEBUG("clean clipboard!");
    if (clipboard_file_list)
    {
        g_list_foreach(clipboard_file_list, (GFunc)free, nullptr);
        g_list_free(clipboard_file_list);
        clipboard_file_list = nullptr;
    }
    clipboard_action = GDK_ACTION_DEFAULT;
}

void
ptk_clipboard_copy_as_text(const char* working_dir,
                           GList* files) // MOD added
{                                        // aka copy path
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    GList* l;

    std::string file_text = "";
    for (l = files; l; l = l->next)
    {
        VFSFileInfo* file = static_cast<VFSFileInfo*>(l->data);
        std::string file_path =
            g_build_filename(working_dir, vfs_file_info_get_name(file), nullptr);
        std::string quoted = bash_quote(file_path);
        file_text = fmt::format("{} {}", file_text, quoted);
    }
    gtk_clipboard_set_text(clip, file_text.c_str(), -1);
    gtk_clipboard_set_text(clip_primary, file_text.c_str(), -1);
}

void
ptk_clipboard_copy_name(const char* working_dir,
                        GList* files) // MOD added
{
    (void)working_dir;
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    GList* l;
    int fcount = 0;

    std::string file_text;
    for (l = files; l; l = l->next)
    {
        VFSFileInfo* file = static_cast<VFSFileInfo*>(l->data);
        if (fcount == 0)
            file_text = fmt::format("{}", vfs_file_info_get_name(file));
        else if (fcount == 1)
            file_text = fmt::format("{}\n{}\n", file_text, vfs_file_info_get_name(file));
        else
            file_text = fmt::format("{}{}\n", file_text, vfs_file_info_get_name(file));
        fcount++;
    }
    gtk_clipboard_set_text(clip, file_text.c_str(), -1);
    gtk_clipboard_set_text(clip_primary, file_text.c_str(), -1);
}

void
ptk_clipboard_copy_text(const char* text) // MOD added
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text(clip, text, -1);
    gtk_clipboard_set_text(clip_primary, text, -1);
}

void
ptk_clipboard_cut_or_copy_files(const char* working_dir, GList* files, bool copy)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetList* target_list = gtk_target_list_new(nullptr, 0);
    int n_targets;
    GList* l;
    GList* file_list = nullptr;

    gtk_target_list_add_text_targets(target_list, 0);
    GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list, &n_targets);
    n_targets += 2;
    targets = g_renew(GtkTargetEntry, targets, n_targets);
    GtkTargetEntry* new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = ztd::strdup("x-special/gnome-copied-files");
    memmove(&(targets[n_targets - 2]), new_target, sizeof(GtkTargetEntry));
    new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = ztd::strdup("text/uri-list");
    memmove(&(targets[n_targets - 1]), new_target, sizeof(GtkTargetEntry));

    gtk_target_list_unref(target_list);

    for (l = g_list_last(files); l; l = l->prev) // sfm was reverse order
    {
        VFSFileInfo* file = static_cast<VFSFileInfo*>(l->data);
        char* file_path = g_build_filename(working_dir, vfs_file_info_get_name(file), nullptr);
        file_list = g_list_prepend(file_list, file_path);
    }

    gtk_clipboard_set_with_data(clip,
                                targets,
                                n_targets,
                                clipboard_get_data,
                                clipboard_clean_data,
                                nullptr);

    free(targets);

    clipboard_file_list = file_list;
    clipboard_action = copy ? GDK_ACTION_COPY : GDK_ACTION_MOVE;
}

void
ptk_clipboard_copy_file_list(char** path, bool copy)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetList* target_list = gtk_target_list_new(nullptr, 0);
    int n_targets;
    GList* file_list = nullptr;

    gtk_target_list_add_text_targets(target_list, 0);
    GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list, &n_targets);
    n_targets += 2;
    targets = g_renew(GtkTargetEntry, targets, n_targets);
    GtkTargetEntry* new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = ztd::strdup("x-special/gnome-copied-files");
    memmove(&(targets[n_targets - 2]), new_target, sizeof(GtkTargetEntry));
    new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = ztd::strdup("text/uri-list");
    memmove(&(targets[n_targets - 1]), new_target, sizeof(GtkTargetEntry));

    gtk_target_list_unref(target_list);

    char** file_path = path;
    while (*file_path)
    {
        if (*file_path[0] == '/')
            file_list = g_list_append(file_list, ztd::strdup(*file_path));
        file_path++;
    }

    gtk_clipboard_set_with_data(clip,
                                targets,
                                n_targets,
                                clipboard_get_data,
                                clipboard_clean_data,
                                nullptr);
    free(targets);

    clipboard_file_list = file_list;
    clipboard_action = copy ? GDK_ACTION_COPY : GDK_ACTION_MOVE;
}

void
ptk_clipboard_paste_files(GtkWindow* parent_win, const char* dest_dir, GtkTreeView* task_view,
                          GFunc callback, GtkWindow* callback_win)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    VFSFileTaskType action;
    char* uri_list_str;

    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);
    GtkSelectionData* sel_data = gtk_clipboard_wait_for_contents(clip, gnome_target);
    if (sel_data)
    {
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return;
        }

        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
        if (!strncmp((char*)gtk_selection_data_get_data(sel_data), "cut", 3))
            action = VFS_FILE_TASK_MOVE;
        else
            action = VFS_FILE_TASK_COPY;

        if (uri_list_str)
        {
            while (*uri_list_str && *uri_list_str != '\n')
                ++uri_list_str;
        }
    }
    else
    {
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
        sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
        if (!sel_data)
            return;
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return;
        }
        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);

        if (clipboard_action == GDK_ACTION_MOVE)
            action = VFS_FILE_TASK_MOVE;
        else
            action = VFS_FILE_TASK_COPY;
    }

    if (uri_list_str)
    {
        char** puri;
        char** uri_list;
        GList* files = nullptr;
        puri = uri_list = g_uri_list_extract_uris(uri_list_str);
        while (*puri)
        {
            char* file_path = g_filename_from_uri(*puri, nullptr, nullptr);
            if (file_path)
            {
                files = g_list_prepend(files, file_path);
            }
            ++puri;
        }
        g_strfreev(uri_list);

        // sfm
        if (files)
            files = g_list_reverse(files);

        /*
         * If only one item is selected and the item is a
         * directory, paste the files in that directory;
         * otherwise, paste the file in current directory.
         */

        PtkFileTask* task = ptk_file_task_new(action,
                                              files,
                                              dest_dir,
                                              parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                              GTK_WIDGET(task_view));
        if (callback && callback_win)
            ptk_file_task_set_complete_notify(task, callback, (void*)callback_win);
        ptk_file_task_run(task);
    }
    gtk_selection_data_free(sel_data);
}

void
ptk_clipboard_paste_links(GtkWindow* parent_win, const char* dest_dir, GtkTreeView* task_view,
                          GFunc callback, GtkWindow* callback_win)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    VFSFileTaskType action;
    char* uri_list_str;

    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);
    GtkSelectionData* sel_data = gtk_clipboard_wait_for_contents(clip, gnome_target);
    if (sel_data)
    {
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return;
        }

        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
        action = VFS_FILE_TASK_LINK;
        if (uri_list_str)
        {
            while (*uri_list_str && *uri_list_str != '\n')
                ++uri_list_str;
        }
    }
    else
    {
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
        sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
        if (!sel_data)
            return;
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return;
        }
        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
        action = VFS_FILE_TASK_LINK;
    }

    if (uri_list_str)
    {
        char** puri;
        char** uri_list;
        GList* files = nullptr;
        puri = uri_list = g_uri_list_extract_uris(uri_list_str);
        while (*puri)
        {
            char* file_path = g_filename_from_uri(*puri, nullptr, nullptr);
            if (file_path)
                files = g_list_prepend(files, file_path);
            ++puri;
        }
        g_strfreev(uri_list);

        // sfm
        if (files)
            files = g_list_reverse(files);

        PtkFileTask* task = ptk_file_task_new(action,
                                              files,
                                              dest_dir,
                                              parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                              task_view ? GTK_WIDGET(task_view) : nullptr);
        if (callback && callback_win)
            ptk_file_task_set_complete_notify(task, callback, (void*)callback_win);
        ptk_file_task_run(task);
    }
    gtk_selection_data_free(sel_data);
}

void
ptk_clipboard_paste_targets(GtkWindow* parent_win, const char* dest_dir, GtkTreeView* task_view,
                            GFunc callback, GtkWindow* callback_win)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    VFSFileTaskType action;
    char* uri_list_str;

    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);
    GtkSelectionData* sel_data = gtk_clipboard_wait_for_contents(clip, gnome_target);
    if (sel_data)
    {
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return;
        }

        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
        action = VFS_FILE_TASK_COPY;
        if (uri_list_str)
        {
            while (*uri_list_str && *uri_list_str != '\n')
                ++uri_list_str;
        }
    }
    else
    {
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
        sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
        if (!sel_data)
            return;
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return;
        }

        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
        action = VFS_FILE_TASK_COPY;
    }

    if (uri_list_str)
    {
        int missing_targets = 0;
        char** puri;
        char** uri_list;
        GList* files = nullptr;
        puri = uri_list = g_uri_list_extract_uris(uri_list_str);
        while (*puri)
        {
            char* file_path = g_filename_from_uri(*puri, nullptr, nullptr);
            if (file_path)
            {
                if (std::filesystem::is_symlink(file_path))
                {
                    // canonicalize target
                    char* str = get_real_link_target(file_path);
                    free(file_path);
                    file_path = str;
                }
                if (file_path)
                {
                    struct stat stat;
                    if (lstat(file_path, &stat) == 0) // need to see broken symlinks
                        files = g_list_prepend(files, file_path);
                    else
                    {
                        missing_targets++;
                        free(file_path);
                    }
                }
            }
            ++puri;
        }
        g_strfreev(uri_list);

        // sfm
        if (files)
            files = g_list_reverse(files);

        PtkFileTask* task = ptk_file_task_new(action,
                                              files,
                                              dest_dir,
                                              parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                              GTK_WIDGET(task_view));
        if (callback && callback_win)
            ptk_file_task_set_complete_notify(task, callback, (void*)callback_win);
        ptk_file_task_run(task);

        if (missing_targets > 0)
        {
            std::string msg =
                fmt::format("{} target{} missing",
                            missing_targets,
                            missing_targets > 1 ? ztd::strdup("s are") : ztd::strdup(" is"));
            ptk_show_error(parent_win ? GTK_WINDOW(parent_win) : nullptr, "Error", msg);
        }
    }
    gtk_selection_data_free(sel_data);
}

GList*
ptk_clipboard_get_file_paths(const char* cwd, bool* is_cut, int* missing_targets)
{
    (void)cwd;
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    char* uri_list_str;

    *is_cut = false;
    *missing_targets = 0;

    // get files from clip
    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);
    GtkSelectionData* sel_data = gtk_clipboard_wait_for_contents(clip, gnome_target);
    if (sel_data)
    {
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return nullptr;
        }

        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
        *is_cut = (!strncmp((char*)gtk_selection_data_get_data(sel_data), "cut", 3));

        if (uri_list_str)
        {
            while (*uri_list_str && *uri_list_str != '\n')
                ++uri_list_str;
        }
    }
    else
    {
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
        sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
        if (!sel_data)
            return nullptr;
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return nullptr;
        }
        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
    }

    if (!uri_list_str)
    {
        gtk_selection_data_free(sel_data);
        return nullptr;
    }

    // create file list
    char** puri;
    char** uri_list;
    GList* files = nullptr;
    puri = uri_list = g_uri_list_extract_uris(uri_list_str);
    while (*puri)
    {
        char* file_path = g_filename_from_uri(*puri, nullptr, nullptr);
        if (file_path)
        {
            if (std::filesystem::exists(file_path))
            {
                files = g_list_prepend(files, file_path);
            }
            else
                // no *missing_targets++ here to avoid -Wunused-value compiler warning
                *missing_targets = *missing_targets + 1;
        }
        ++puri;
    }
    g_strfreev(uri_list);
    gtk_selection_data_free(sel_data);

    return files;
}
