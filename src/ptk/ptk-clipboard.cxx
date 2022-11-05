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

#include <vector>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-error.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-file-actions-rename.hxx"
#include "ptk/ptk-utils.hxx"

#include "utils.hxx"

#include "ptk/ptk-clipboard.hxx"

static GdkDragAction clipboard_action = GdkDragAction::GDK_ACTION_DEFAULT;
static std::vector<std::string> clipboard_file_list;

static const std::vector<std::string>
uri_list_extract_uris(const char* uri_list_str)
{
    std::vector<std::string> uri_list;

    char** c_uri;
    char** c_uri_list;
    c_uri = c_uri_list = g_uri_list_extract_uris(uri_list_str);

    for (; *c_uri; ++c_uri)
    {
        uri_list.push_back(*c_uri);
    }
    g_strfreev(c_uri_list);

    return uri_list;
}

static void
clipboard_get_data(GtkClipboard* clipboard, GtkSelectionData* selection_data, u32 info,
                   void* user_data)
{
    (void)clipboard;
    (void)info;
    (void)user_data;
    GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);

    bool use_uri = false;

    if (clipboard_file_list.empty())
        return;

    std::string uri_list;

    if (gtk_selection_data_get_target(selection_data) == gnome_target)
    {
        const char* action =
            clipboard_action == GdkDragAction::GDK_ACTION_MOVE ? "cut\n" : "copy\n";
        uri_list.append(action);
        use_uri = true;
    }
    else if (gtk_selection_data_get_target(selection_data) == uri_list_target)
    {
        use_uri = true;
    }

    for (std::string_view clipboard_file: clipboard_file_list)
    {
        std::string file_name;
        if (use_uri)
        {
            file_name = Glib::filename_to_uri(clipboard_file.data());
        }
        else
        {
            file_name = Glib::filename_display_name(clipboard_file.data());
        }

        uri_list.append(fmt::format("{}\n", file_name));
    }

    gtk_selection_data_set(selection_data,
                           gtk_selection_data_get_target(selection_data),
                           8,
                           (const unsigned char*)uri_list.c_str(),
                           uri_list.size());
    // LOG_DEBUG("clipboard data: \n\n{}\n\n", list);
}

static void
clipboard_clean_data(GtkClipboard* clipboard, void* user_data)
{
    (void)clipboard;
    (void)user_data;
    // LOG_DEBUG("clean clipboard!");
    clipboard_file_list.clear();
    clipboard_action = GdkDragAction::GDK_ACTION_DEFAULT;
}

void
ptk_clipboard_copy_as_text(const char* working_dir, const std::vector<vfs::file_info>& sel_files)
{ // aka copy path
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);

    std::string file_text;
    for (vfs::file_info file: sel_files)
    {
        const std::string file_path = Glib::build_filename(working_dir, file->get_name());
        const std::string quoted = bash_quote(file_path);
        file_text = fmt::format("{} {}", file_text, quoted);
    }
    gtk_clipboard_set_text(clip, file_text.c_str(), -1);
    gtk_clipboard_set_text(clip_primary, file_text.c_str(), -1);
}

void
ptk_clipboard_copy_name(const char* working_dir, const std::vector<vfs::file_info>& sel_files)
{
    (void)working_dir;
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    i32 fcount = 0;

    std::string file_text;
    for (vfs::file_info file: sel_files)
    {
        if (fcount == 0)
            file_text = fmt::format("{}", file->get_name());
        else if (fcount == 1)
            file_text = fmt::format("{}\n{}\n", file_text, file->get_name());
        else
            file_text = fmt::format("{}{}\n", file_text, file->get_name());
        fcount++;
    }
    gtk_clipboard_set_text(clip, file_text.c_str(), -1);
    gtk_clipboard_set_text(clip_primary, file_text.c_str(), -1);
}

void
ptk_clipboard_copy_text(const char* text)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text(clip, text, -1);
    gtk_clipboard_set_text(clip_primary, text, -1);
}

void
ptk_clipboard_cut_or_copy_files(const char* working_dir,
                                const std::vector<vfs::file_info>& sel_files, bool copy)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetList* target_list = gtk_target_list_new(nullptr, 0);
    i32 n_targets;
    std::vector<std::string> file_list;

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

    for (vfs::file_info file: sel_files)
    {
        const std::string file_path = Glib::build_filename(working_dir, file->get_name());
        file_list.push_back(file_path);
    }

    gtk_clipboard_set_with_data(clip,
                                targets,
                                n_targets,
                                clipboard_get_data,
                                clipboard_clean_data,
                                nullptr);

    free(targets);

    clipboard_file_list = file_list;
    clipboard_action = copy ? GdkDragAction::GDK_ACTION_COPY : GdkDragAction::GDK_ACTION_MOVE;
}

void
ptk_clipboard_copy_file_list(char** path, bool copy)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetList* target_list = gtk_target_list_new(nullptr, 0);
    i32 n_targets;
    std::vector<std::string> file_list;

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
            file_list.push_back(ztd::strdup(*file_path));
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
    clipboard_action = copy ? GdkDragAction::GDK_ACTION_COPY : GdkDragAction::GDK_ACTION_MOVE;
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
        if (ztd::startswith((const char*)gtk_selection_data_get_data(sel_data), "cut"))
            action = VFSFileTaskType::MOVE;
        else
            action = VFSFileTaskType::COPY;

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

        if (clipboard_action == GdkDragAction::GDK_ACTION_MOVE)
            action = VFSFileTaskType::MOVE;
        else
            action = VFSFileTaskType::COPY;
    }

    if (uri_list_str)
    {
        std::vector<std::string> file_list;

        const std::vector<std::string> uri_list = uri_list_extract_uris(uri_list_str);
        for (std::string_view uri: uri_list)
        {
            std::string file_path;
            try
            {
                file_path = Glib::filename_from_uri(uri.data());
            }
            catch (Glib::ConvertError)
            {
                continue;
            }

            file_list.push_back(file_path);
        }

        /*
         * If only one item is selected and the item is a
         * directory, paste the files in that directory;
         * otherwise, paste the file in current directory.
         */

        PtkFileTask* ptask = new PtkFileTask(action,
                                             file_list,
                                             dest_dir,
                                             parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                             GTK_WIDGET(task_view));
        if (callback && callback_win)
            ptk_file_task_set_complete_notify(ptask, callback, (void*)callback_win);
        ptk_file_task_run(ptask);
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
        action = VFSFileTaskType::LINK;
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
        action = VFSFileTaskType::LINK;
    }

    if (uri_list_str)
    {
        std::vector<std::string> file_list;

        const std::vector<std::string> uri_list = uri_list_extract_uris(uri_list_str);
        for (std::string_view uri: uri_list)
        {
            std::string file_path;
            try
            {
                file_path = Glib::filename_from_uri(uri.data());
            }
            catch (Glib::ConvertError)
            {
                continue;
            }

            file_list.push_back(file_path);
        }

        PtkFileTask* ptask = new PtkFileTask(action,
                                             file_list,
                                             dest_dir,
                                             parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                             task_view ? GTK_WIDGET(task_view) : nullptr);
        if (callback && callback_win)
            ptk_file_task_set_complete_notify(ptask, callback, (void*)callback_win);
        ptk_file_task_run(ptask);
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
        action = VFSFileTaskType::COPY;
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
        action = VFSFileTaskType::COPY;
    }

    if (uri_list_str)
    {
        i32 missing_targets = 0;

        std::vector<std::string> file_list;

        const std::vector<std::string> uri_list = uri_list_extract_uris(uri_list_str);
        for (std::string_view uri: uri_list)
        {
            std::string file_path;
            try
            {
                file_path = Glib::filename_from_uri(uri.data());
            }
            catch (Glib::ConvertError)
            {
                continue;
            }

            if (std::filesystem::is_symlink(file_path))
            { // canonicalize target
                file_path = get_real_link_target(file_path);
            }

            struct stat stat;
            if (lstat(file_path.c_str(), &stat) == 0)
            { // need to see broken symlinks
                file_list.push_back(file_path);
            }
            else
            {
                missing_targets++;
            }
        }

        PtkFileTask* ptask = new PtkFileTask(action,
                                             file_list,
                                             dest_dir,
                                             parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                             GTK_WIDGET(task_view));
        if (callback && callback_win)
            ptk_file_task_set_complete_notify(ptask, callback, (void*)callback_win);
        ptk_file_task_run(ptask);

        if (missing_targets > 0)
        {
            const std::string msg =
                fmt::format("{} target{} missing",
                            missing_targets,
                            missing_targets > 1 ? ztd::strdup("s are") : ztd::strdup(" is"));
            ptk_show_error(parent_win ? GTK_WINDOW(parent_win) : nullptr, "Error", msg);
        }
    }
    gtk_selection_data_free(sel_data);
}

const std::vector<std::string>
ptk_clipboard_get_file_paths(const char* cwd, bool* is_cut, int* missing_targets)
{
    (void)cwd;
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    char* uri_list_str;
    std::vector<std::string> file_list;

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
            return file_list;
        }

        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
        *is_cut = (ztd::startswith((const char*)gtk_selection_data_get_data(sel_data), "cut"));

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
            return file_list;
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return file_list;
        }
        uri_list_str = (char*)gtk_selection_data_get_data(sel_data);
    }

    if (!uri_list_str)
    {
        gtk_selection_data_free(sel_data);
        return file_list;
    }

    // create file list
    const std::vector<std::string> uri_list = uri_list_extract_uris(uri_list_str);
    for (std::string_view uri: uri_list)
    {
        std::string file_path;
        try
        {
            file_path = Glib::filename_from_uri(uri.data());
        }
        catch (Glib::ConvertError)
        {
            continue;
        }

        if (std::filesystem::exists(file_path))
        {
            file_list.push_back(file_path);
        }
        else
        { // no *missing_targets++ here to avoid -Wunused-value compiler warning
            *missing_targets = *missing_targets + 1;
        }
    }

    gtk_selection_data_free(sel_data);

    return file_list;
}
