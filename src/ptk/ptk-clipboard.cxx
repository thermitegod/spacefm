/*
 *  C Implementation: ptk-clipboard
 *
 * Description:
 *
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#include "ptk-file-task.hxx"
#include "ptk-file-misc.hxx"
#include "ptk-utils.hxx"

#include "utils.hxx"

#include "ptk-clipboard.hxx"

static GdkDragAction clipboard_action = GDK_ACTION_DEFAULT;
static GList* clipboard_file_list = NULL;

static void clipboard_get_data(GtkClipboard* clipboard, GtkSelectionData* selection_data,
                               unsigned int info, void* user_data)
{
    GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", FALSE);
    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", FALSE);

    bool use_uri = FALSE;

    if (!clipboard_file_list)
        return;

    GString* list = g_string_sized_new(8192);

    if (gtk_selection_data_get_target(selection_data) == gnome_target)
    {
        const char* action = clipboard_action == GDK_ACTION_MOVE ? "cut\n" : "copy\n";
        g_string_append(list, action);
        use_uri = TRUE;
    }
    else if (gtk_selection_data_get_target(selection_data) == uri_list_target)
        use_uri = TRUE;

    GList* l;
    for (l = clipboard_file_list; l; l = l->next)
    {
        char* file_name;
        if (use_uri)
        {
            file_name = g_filename_to_uri((char*)l->data, NULL, NULL);
        }
        else
        {
            file_name = g_filename_display_name((char*)l->data);
        }
        g_string_append(list, file_name);
        g_free(file_name);

        if (gtk_selection_data_get_target(selection_data) != uri_list_target)
            g_string_append_c(list, '\n');
        else
            g_string_append(list, "\r\n");
    }

    gtk_selection_data_set(selection_data,
                           gtk_selection_data_get_target(selection_data),
                           8,
                           (unsigned char*)list->str,
                           list->len + 1);
    /* g_debug( "clipboard data:\n%s\n\n", list->str ); */
    g_string_free(list, TRUE);
}

static void clipboard_clean_data(GtkClipboard* clipboard, void* user_data)
{
    /* g_debug( "clean clipboard!\n" ); */
    if (clipboard_file_list)
    {
        g_list_foreach(clipboard_file_list, (GFunc)g_free, NULL);
        g_list_free(clipboard_file_list);
        clipboard_file_list = NULL;
    }
    clipboard_action = GDK_ACTION_DEFAULT;
}

void ptk_clipboard_copy_as_text(const char* working_dir,
                                GList* files) // MOD added
{                                             // aka copy path
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    GList* l;

    char* file_text = g_strdup("");
    for (l = files; l; l = l->next)
    {
        VFSFileInfo* file = (VFSFileInfo*)l->data;
        char* file_path = g_build_filename(working_dir, vfs_file_info_get_name(file), NULL);
        char* quoted = bash_quote(file_path);
        char* str = file_text;
        file_text = g_strdup_printf("%s %s", str, quoted);
        g_free(str);
        g_free(quoted);
        g_free(file_path);
    }
    gtk_clipboard_set_text(clip, file_text, -1);
    gtk_clipboard_set_text(clip_primary, file_text, -1);
    g_free(file_text);
}

void ptk_clipboard_copy_name(const char* working_dir,
                             GList* files) // MOD added
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    GList* l;
    int fcount = 0;

    char* file_text = g_strdup("");
    for (l = files; l; l = l->next)
    {
        VFSFileInfo* file = (VFSFileInfo*)l->data;
        char* str = file_text;
        if (fcount == 0)
            file_text = g_strdup_printf("%s", vfs_file_info_get_name(file));
        else if (fcount == 1)
            file_text = g_strdup_printf("%s\n%s\n", file_text, vfs_file_info_get_name(file));
        else
            file_text = g_strdup_printf("%s%s\n", file_text, vfs_file_info_get_name(file));
        fcount++;
        g_free(str);
    }
    gtk_clipboard_set_text(clip, file_text, -1);
    gtk_clipboard_set_text(clip_primary, file_text, -1);
    g_free(file_text);
}

void ptk_clipboard_copy_text(const char* text) // MOD added
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text(clip, text, -1);
    gtk_clipboard_set_text(clip_primary, text, -1);
}

void ptk_clipboard_cut_or_copy_files(const char* working_dir, GList* files, bool copy)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetList* target_list = gtk_target_list_new(NULL, 0);
    int n_targets;
    GList* l;
    GList* file_list = NULL;

    gtk_target_list_add_text_targets(target_list, 0);
    GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list, &n_targets);
    n_targets += 2;
    targets = g_renew(GtkTargetEntry, targets, n_targets);
    GtkTargetEntry* new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = g_strdup("x-special/gnome-copied-files");
    memmove(&(targets[n_targets - 2]), new_target, sizeof(GtkTargetEntry));
    new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = g_strdup("text/uri-list");
    memmove(&(targets[n_targets - 1]), new_target, sizeof(GtkTargetEntry));

    gtk_target_list_unref(target_list);

    for (l = g_list_last(files); l; l = l->prev) // sfm was reverse order
    {
        VFSFileInfo* file = (VFSFileInfo*)l->data;
        char* file_path = g_build_filename(working_dir, vfs_file_info_get_name(file), NULL);
        file_list = g_list_prepend(file_list, file_path);
    }

    gtk_clipboard_set_with_data(clip,
                                targets,
                                n_targets,
                                clipboard_get_data,
                                clipboard_clean_data,
                                NULL);

    g_free(targets);

    clipboard_file_list = file_list;
    clipboard_action = copy ? GDK_ACTION_COPY : GDK_ACTION_MOVE;
}

void ptk_clipboard_copy_file_list(char** path, bool copy)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetList* target_list = gtk_target_list_new(NULL, 0);
    int n_targets;
    GList* file_list = NULL;

    gtk_target_list_add_text_targets(target_list, 0);
    GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list, &n_targets);
    n_targets += 2;
    targets = g_renew(GtkTargetEntry, targets, n_targets);
    GtkTargetEntry* new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = g_strdup("x-special/gnome-copied-files");
    memmove(&(targets[n_targets - 2]), new_target, sizeof(GtkTargetEntry));
    new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = g_strdup("text/uri-list");
    memmove(&(targets[n_targets - 1]), new_target, sizeof(GtkTargetEntry));

    gtk_target_list_unref(target_list);

    char** file_path = path;
    while (*file_path)
    {
        if (*file_path[0] == '/')
            file_list = g_list_append(file_list, g_strdup(*file_path));
        file_path++;
    }

    gtk_clipboard_set_with_data(clip,
                                targets,
                                n_targets,
                                clipboard_get_data,
                                clipboard_clean_data,
                                NULL);
    g_free(targets);

    clipboard_file_list = file_list;
    clipboard_action = copy ? GDK_ACTION_COPY : GDK_ACTION_MOVE;
}

void ptk_clipboard_paste_files(GtkWindow* parent_win, const char* dest_dir, GtkTreeView* task_view,
                               GFunc callback, GtkWindow* callback_win)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    VFSFileTaskType action;
    char* uri_list_str;

    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", FALSE);
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
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", FALSE);
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
        GList* files = NULL;
        puri = uri_list = g_uri_list_extract_uris(uri_list_str);
        while (*puri)
        {
            char* file_path = g_filename_from_uri(*puri, NULL, NULL);
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
                                              parent_win ? GTK_WINDOW(parent_win) : NULL,
                                              GTK_WIDGET(task_view));
        if (callback && callback_win)
            ptk_file_task_set_complete_notify(task, callback, (void*)callback_win);
        ptk_file_task_run(task);
    }
    gtk_selection_data_free(sel_data);
}

void ptk_clipboard_paste_links(GtkWindow* parent_win, const char* dest_dir, GtkTreeView* task_view,
                               GFunc callback, GtkWindow* callback_win)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    VFSFileTaskType action;
    char* uri_list_str;

    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", FALSE);
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
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", FALSE);
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
        GList* files = NULL;
        puri = uri_list = g_uri_list_extract_uris(uri_list_str);
        while (*puri)
        {
            char* file_path = g_filename_from_uri(*puri, NULL, NULL);
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
                                              parent_win ? GTK_WINDOW(parent_win) : NULL,
                                              task_view ? GTK_WIDGET(task_view) : NULL);
        if (callback && callback_win)
            ptk_file_task_set_complete_notify(task, callback, (void*)callback_win);
        ptk_file_task_run(task);
    }
    gtk_selection_data_free(sel_data);
}

void ptk_clipboard_paste_targets(GtkWindow* parent_win, const char* dest_dir,
                                 GtkTreeView* task_view, GFunc callback, GtkWindow* callback_win)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    VFSFileTaskType action;
    char* uri_list_str;

    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", FALSE);
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
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", FALSE);
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
        GList* files = NULL;
        puri = uri_list = g_uri_list_extract_uris(uri_list_str);
        while (*puri)
        {
            char* file_path = g_filename_from_uri(*puri, NULL, NULL);
            if (file_path)
            {
                if (g_file_test(file_path, G_FILE_TEST_IS_SYMLINK))
                {
                    // canonicalize target
                    char* str = get_real_link_target(file_path);
                    g_free(file_path);
                    file_path = str;
                }
                if (file_path)
                {
                    // do not use g_file_test here - link target may be missing
                    struct stat stat;
                    if (lstat(file_path, &stat) == 0)
                        files = g_list_prepend(files, file_path);
                    else
                    {
                        missing_targets++;
                        g_free(file_path);
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
                                              parent_win ? GTK_WINDOW(parent_win) : NULL,
                                              GTK_WIDGET(task_view));
        if (callback && callback_win)
            ptk_file_task_set_complete_notify(task, callback, (void*)callback_win);
        ptk_file_task_run(task);

        if (missing_targets > 0)
            ptk_show_error(parent_win ? GTK_WINDOW(parent_win) : NULL,
                           g_strdup_printf("Error"),
                           g_strdup_printf("%i target%s missing",
                                           missing_targets,
                                           missing_targets > 1 ? g_strdup_printf("s are")
                                                               : g_strdup_printf(" is")));
    }
    gtk_selection_data_free(sel_data);
}

GList* ptk_clipboard_get_file_paths(const char* cwd, bool* is_cut, int* missing_targets)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    char* uri_list_str;

    *is_cut = FALSE;
    *missing_targets = 0;

    // get files from clip
    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", FALSE);
    GtkSelectionData* sel_data = gtk_clipboard_wait_for_contents(clip, gnome_target);
    if (sel_data)
    {
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return NULL;
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
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", FALSE);
        sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
        if (!sel_data)
            return NULL;
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return NULL;
        }
        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
    }

    if (!uri_list_str)
    {
        gtk_selection_data_free(sel_data);
        return NULL;
    }

    // create file list
    char** puri;
    char** uri_list;
    GList* files = NULL;
    puri = uri_list = g_uri_list_extract_uris(uri_list_str);
    while (*puri)
    {
        char* file_path = g_filename_from_uri(*puri, NULL, NULL);
        if (file_path)
        {
            if (g_file_test(file_path, G_FILE_TEST_EXISTS))
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
