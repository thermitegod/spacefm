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

#include <format>

#include <filesystem>

#include <span>
#include <vector>

#include <memory>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "utils/strdup.hxx"
#include "utils/shell-quote.hxx"

#include "ptk/ptk-dialog.hxx"
#include "ptk/ptk-file-task.hxx"

#include "ptk/ptk-clipboard.hxx"

#if (GTK_MAJOR_VERSION == 4)

// https://docs.gtk.org/gtk4/migrating-3to4.html#replace-gtkclipboard-with-gdkclipboard

void
ptk::clipboard::cut_or_copy_files(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                                  bool copy) noexcept
{
    (void)selected_files;
    (void)copy;
    ztd::logger::debug("TODO - PORT - GdkClipboard");
}

void
ptk::clipboard::copy_as_text(
    const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    (void)selected_files;
    ztd::logger::debug("TODO - PORT - GdkClipboard");
}

void
ptk::clipboard::copy_name(const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    (void)selected_files;
    ztd::logger::debug("TODO - PORT - GdkClipboard");
}

void
ptk::clipboard::paste_files(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                            GtkTreeView* task_view, GFunc callback,
                            GtkWindow* callback_win) noexcept
{
    (void)parent_win;
    (void)dest_dir;
    (void)task_view;
    (void)callback;
    (void)callback_win;
    ztd::logger::debug("TODO - PORT - GdkClipboard");
}

void
ptk::clipboard::paste_links(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                            GtkTreeView* task_view, GFunc callback,
                            GtkWindow* callback_win) noexcept
{
    (void)parent_win;
    (void)dest_dir;
    (void)task_view;
    (void)callback;
    (void)callback_win;
    ztd::logger::debug("TODO - PORT - GdkClipboard");
}

void
ptk::clipboard::paste_targets(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                              GtkTreeView* task_view, GFunc callback,
                              GtkWindow* callback_win) noexcept
{
    (void)parent_win;
    (void)dest_dir;
    (void)task_view;
    (void)callback;
    (void)callback_win;
    ztd::logger::debug("TODO - PORT - GdkClipboard");
}

void
ptk::clipboard::copy_text(const std::string_view text) noexcept
{
    (void)text;
}

void
ptk::clipboard::cut_or_copy_file_list(const std::span<const std::string> selected_files,
                                      bool copy) noexcept
{
    (void)selected_files;
    (void)copy;
    ztd::logger::debug("TODO - PORT - GdkClipboard");
}

const std::vector<std::filesystem::path>
ptk::clipboard::get_file_paths(const std::filesystem::path& cwd, bool* is_cut,
                               i32* missing_targets) noexcept
{
    (void)cwd;
    (void)is_cut;
    (void)missing_targets;
    ztd::logger::debug("TODO - PORT - GdkClipboard");
    return {};
}

#elif (GTK_MAJOR_VERSION == 3)

static GdkDragAction clipboard_action = GdkDragAction::GDK_ACTION_DEFAULT;
static std::vector<std::filesystem::path> clipboard_file_list;

static const std::vector<std::string>
uri_list_extract_uris(const char* uri_list_str) noexcept
{
    std::vector<std::string> uri_list;

    char** c_uri = nullptr;
    char** c_uri_list = nullptr;
    c_uri = c_uri_list = g_uri_list_extract_uris(uri_list_str);

    for (; *c_uri; ++c_uri)
    {
        uri_list.emplace_back(*c_uri);
    }
    g_strfreev(c_uri_list);

    return uri_list;
}

static void
clipboard_get_data(GtkClipboard* clipboard, GtkSelectionData* selection_data, u32 info,
                   void* user_data) noexcept
{
    (void)clipboard;
    (void)info;
    (void)user_data;
    GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
    GdkAtom gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);

    bool use_uri = false;

    if (clipboard_file_list.empty())
    {
        return;
    }

    std::string uri_list;

    if (gtk_selection_data_get_target(selection_data) == gnome_target)
    {
        const std::string action =
            clipboard_action == GdkDragAction::GDK_ACTION_MOVE ? "cut\n" : "copy\n";
        uri_list.append(action);
        use_uri = true;
    }
    else if (gtk_selection_data_get_target(selection_data) == uri_list_target)
    {
        use_uri = true;
    }

    for (const auto& clipboard_file : clipboard_file_list)
    {
        if (use_uri)
        {
            const std::string uri_name = Glib::filename_to_uri(clipboard_file.string());
            uri_list.append(std::format("{}\n", uri_name));
        }
        else
        {
            // Need to use .string() to avoid fmt adding double quotes when formating
            uri_list.append(std::format("{}\n", clipboard_file.string()));
        }
    }

    gtk_selection_data_set(selection_data,
                           gtk_selection_data_get_target(selection_data),
                           8,
                           (const unsigned char*)uri_list.data(),
                           static_cast<i32>(uri_list.size()));
    // ztd::logger::debug("clipboard data: \n\n{}\n\n", uri_list);
}

static void
clipboard_clean_data(GtkClipboard* clipboard, void* user_data) noexcept
{
    (void)clipboard;
    (void)user_data;
    // ztd::logger::debug("clean clipboard!");
    clipboard_file_list.clear();
    clipboard_action = GdkDragAction::GDK_ACTION_DEFAULT;
}

void
ptk::clipboard::copy_as_text(
    const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{ // aka copy path
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);

    std::string file_text;
    for (const auto& file : selected_files)
    {
        const auto quoted = ::utils::shell_quote(file->path().string());
        file_text = std::format("{} {}", file_text, quoted);
    }
    gtk_clipboard_set_text(clip, file_text.data(), -1);
    gtk_clipboard_set_text(clip_primary, file_text.data(), -1);
}

void
ptk::clipboard::copy_name(const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);

    std::string file_text;
    for (const auto& file : selected_files)
    {
        file_text = std::format("{}{}\n", file_text, file->name());
    }
    file_text = ztd::strip(file_text);

    gtk_clipboard_set_text(clip, file_text.data(), -1);
    gtk_clipboard_set_text(clip_primary, file_text.data(), -1);
}

void
ptk::clipboard::copy_text(const std::string_view text) noexcept
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkClipboard* clip_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text(clip, text.data(), -1);
    gtk_clipboard_set_text(clip_primary, text.data(), -1);
}

void
ptk::clipboard::cut_or_copy_files(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                                  bool copy) noexcept
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetList* target_list = gtk_target_list_new(nullptr, 0);
    i32 n_targets = 0;

    gtk_target_list_add_text_targets(target_list, 0);
    g_autofree GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list, &n_targets);
    n_targets += 2;
    targets = g_renew(GtkTargetEntry, targets, n_targets);
    GtkTargetEntry* new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = ::utils::strdup("x-special/gnome-copied-files");
    memmove(&(targets[n_targets - 2]), new_target, sizeof(GtkTargetEntry));
    new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = ::utils::strdup("text/uri-list");
    memmove(&(targets[n_targets - 1]), new_target, sizeof(GtkTargetEntry));

    gtk_target_list_unref(target_list);

    std::vector<std::filesystem::path> file_list;
    file_list.reserve(selected_files.size());
    for (const auto& file : selected_files)
    {
        file_list.push_back(file->path());
    }

    gtk_clipboard_set_with_data(clip,
                                targets,
                                n_targets,
                                clipboard_get_data,
                                clipboard_clean_data,
                                nullptr);

    clipboard_file_list = file_list;
    clipboard_action = copy ? GdkDragAction::GDK_ACTION_COPY : GdkDragAction::GDK_ACTION_MOVE;
}

void
ptk::clipboard::cut_or_copy_file_list(const std::span<const std::string> selected_files,
                                      bool copy) noexcept
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetList* target_list = gtk_target_list_new(nullptr, 0);
    i32 n_targets = 0;

    gtk_target_list_add_text_targets(target_list, 0);
    g_autofree GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list, &n_targets);
    n_targets += 2;
    targets = g_renew(GtkTargetEntry, targets, n_targets);
    GtkTargetEntry* new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = ::utils::strdup("x-special/gnome-copied-files");
    memmove(&(targets[n_targets - 2]), new_target, sizeof(GtkTargetEntry));
    new_target = g_new0(GtkTargetEntry, 1);
    new_target->target = ::utils::strdup("text/uri-list");
    memmove(&(targets[n_targets - 1]), new_target, sizeof(GtkTargetEntry));

    gtk_target_list_unref(target_list);

    std::vector<std::filesystem::path> file_list;
    for (const auto& file : selected_files)
    {
        if (std::filesystem::path(file).is_absolute())
        {
            file_list.emplace_back(file);
        }
    }

    gtk_clipboard_set_with_data(clip,
                                targets,
                                n_targets,
                                clipboard_get_data,
                                clipboard_clean_data,
                                nullptr);

    clipboard_file_list = file_list;
    clipboard_action = copy ? GdkDragAction::GDK_ACTION_COPY : GdkDragAction::GDK_ACTION_MOVE;
}

void
ptk::clipboard::paste_files(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                            GtkTreeView* task_view, GFunc callback,
                            GtkWindow* callback_win) noexcept
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    vfs::file_task::type action;
    const char* uri_list_str = nullptr;

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

        uri_list_str = (const char*)gtk_selection_data_get_data(sel_data);
        if (std::string(uri_list_str).starts_with("cut"))
        {
            action = vfs::file_task::type::move;
        }
        else
        {
            action = vfs::file_task::type::copy;
        }

        if (uri_list_str)
        {
            while (*uri_list_str && *uri_list_str != '\n')
            {
                ++uri_list_str;
            }
        }
    }
    else
    {
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
        sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
        if (!sel_data)
        {
            return;
        }
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return;
        }
        uri_list_str = (const char*)gtk_selection_data_get_data(sel_data);

        if (clipboard_action == GdkDragAction::GDK_ACTION_MOVE)
        {
            action = vfs::file_task::type::move;
        }
        else
        {
            action = vfs::file_task::type::copy;
        }
    }

    if (uri_list_str)
    {
        const std::vector<std::string> uri_list = uri_list_extract_uris(uri_list_str);
        std::vector<std::filesystem::path> file_list;
        file_list.reserve(uri_list.size());
        for (const std::string_view uri : uri_list)
        {
            std::filesystem::path file_path;
            try
            {
                file_path = Glib::filename_from_uri(uri.data());
            }
            catch (const Glib::ConvertError& e)
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

        ptk::file_task* ptask = ptk_file_task_new(action,
                                                  file_list,
                                                  dest_dir,
                                                  parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                                  GTK_WIDGET(task_view));
        if (callback && callback_win)
        {
            ptask->set_complete_notify(callback, (void*)callback_win);
        }
        ptask->run();
    }
    gtk_selection_data_free(sel_data);
}

void
ptk::clipboard::paste_links(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                            GtkTreeView* task_view, GFunc callback,
                            GtkWindow* callback_win) noexcept
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    vfs::file_task::type action;
    const char* uri_list_str = nullptr;

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

        uri_list_str = (const char*)gtk_selection_data_get_data(sel_data);
        action = vfs::file_task::type::link;
        if (uri_list_str)
        {
            while (*uri_list_str && *uri_list_str != '\n')
            {
                ++uri_list_str;
            }
        }
    }
    else
    {
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
        sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
        if (!sel_data)
        {
            return;
        }
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return;
        }
        uri_list_str = (const char*)gtk_selection_data_get_data(sel_data);
        action = vfs::file_task::type::link;
    }

    if (uri_list_str)
    {
        const std::vector<std::string> uri_list = uri_list_extract_uris(uri_list_str);
        std::vector<std::filesystem::path> file_list;
        file_list.reserve(uri_list.size());
        for (const std::string_view uri : uri_list)
        {
            std::filesystem::path file_path;
            try
            {
                file_path = Glib::filename_from_uri(uri.data());
            }
            catch (const Glib::ConvertError& e)
            {
                continue;
            }

            file_list.push_back(file_path);
        }

        ptk::file_task* ptask = ptk_file_task_new(action,
                                                  file_list,
                                                  dest_dir,
                                                  parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                                  task_view ? GTK_WIDGET(task_view) : nullptr);
        if (callback && callback_win)
        {
            ptask->set_complete_notify(callback, (void*)callback_win);
        }
        ptask->run();
    }
    gtk_selection_data_free(sel_data);
}

void
ptk::clipboard::paste_targets(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                              GtkTreeView* task_view, GFunc callback,
                              GtkWindow* callback_win) noexcept
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    vfs::file_task::type action;
    const char* uri_list_str = nullptr;

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

        uri_list_str = (const char*)gtk_selection_data_get_data(sel_data);
        action = vfs::file_task::type::copy;
        if (uri_list_str)
        {
            while (*uri_list_str && *uri_list_str != '\n')
            {
                ++uri_list_str;
            }
        }
    }
    else
    {
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
        sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
        if (!sel_data)
        {
            return;
        }
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return;
        }

        uri_list_str = (const char*)gtk_selection_data_get_data(sel_data);
        action = vfs::file_task::type::copy;
    }

    if (uri_list_str)
    {
        i32 missing_targets = 0;

        const std::vector<std::string> uri_list = uri_list_extract_uris(uri_list_str);
        std::vector<std::filesystem::path> file_list;
        file_list.reserve(uri_list.size());

        for (const std::string_view uri : uri_list)
        {
            std::filesystem::path file_path;
            try
            {
                file_path = Glib::filename_from_uri(uri.data());
            }
            catch (const Glib::ConvertError& e)
            {
                continue;
            }

            if (std::filesystem::is_symlink(file_path))
            { // canonicalize target
                file_path = std::filesystem::read_symlink(file_path);
            }

            if (std::filesystem::exists(file_path))
            { // need to see broken symlinks
                file_list.push_back(file_path);
            }
            else
            {
                missing_targets++;
            }
        }

        ptk::file_task* ptask = ptk_file_task_new(action,
                                                  file_list,
                                                  dest_dir,
                                                  parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                                  GTK_WIDGET(task_view));
        if (callback && callback_win)
        {
            ptask->set_complete_notify(callback, (void*)callback_win);
        }
        ptask->run();

        if (missing_targets > 0)
        {
            ptk::dialog::error(parent_win ? GTK_WINDOW(parent_win) : nullptr,
                               "Error",
                               std::format("{} target{} missing",
                                           missing_targets,
                                           missing_targets > 1 ? "s are" : " is"));
        }
    }
    gtk_selection_data_free(sel_data);
}

const std::vector<std::filesystem::path>
ptk::clipboard::get_file_paths(const std::filesystem::path& cwd, bool* is_cut,
                               i32* missing_targets) noexcept
{
    (void)cwd;

    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    const char* uri_list_str = nullptr;
    std::vector<std::filesystem::path> file_list;

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

        uri_list_str = (const char*)gtk_selection_data_get_data(sel_data);
        *is_cut = (std::string(uri_list_str).starts_with("cut"));

        if (uri_list_str)
        {
            while (*uri_list_str && *uri_list_str != '\n')
            {
                ++uri_list_str;
            }
        }
    }
    else
    {
        GdkAtom uri_list_target = gdk_atom_intern("text/uri-list", false);
        sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
        if (!sel_data)
        {
            return file_list;
        }
        if (gtk_selection_data_get_length(sel_data) <= 0 ||
            gtk_selection_data_get_format(sel_data) != 8)
        {
            gtk_selection_data_free(sel_data);
            return file_list;
        }
        uri_list_str = (const char*)gtk_selection_data_get_data(sel_data);
    }

    if (!uri_list_str)
    {
        gtk_selection_data_free(sel_data);
        return file_list;
    }

    // create file list
    const std::vector<std::string> uri_list = uri_list_extract_uris(uri_list_str);
    file_list.reserve(uri_list.size());
    for (const std::string_view uri : uri_list)
    {
        std::string file_path;
        try
        {
            file_path = Glib::filename_from_uri(uri.data());
        }
        catch (const Glib::ConvertError& e)
        {
            continue;
        }

        if (std::filesystem::exists(file_path))
        {
            file_list.emplace_back(file_path);
        }
        else
        { // no *missing_targets++ here to avoid -Wunused-value compiler warning
            *missing_targets = *missing_targets + 1;
        }
    }

    gtk_selection_data_free(sel_data);

    return file_list;
}

#endif
