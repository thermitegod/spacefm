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

#include <array>
#include <vector>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-file-misc.hxx"

#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-file-properties.hxx"
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-clipboard.hxx"
#include "ptk/ptk-file-archiver.hxx"
#include "ptk/ptk-location-view.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "ptk/ptk-handler.hxx"
#include "utils.hxx"

struct ParentInfo
{
    PtkFileBrowser* file_browser;
    const char* cwd;
};

struct MoveSet
{
    char* full_path;
    const char* old_path;
    char* new_path;
    char* desc;
    bool is_dir;
    bool is_link;
    bool clip_copy;
    PtkRenameMode create_new;

    GtkWidget* dlg;
    GtkWidget* parent;
    PtkFileBrowser* browser;

    GtkLabel* label_type;
    GtkLabel* label_mime;
    GtkWidget* hbox_type;
    char* mime_type;

    GtkLabel* label_target;
    GtkEntry* entry_target;
    GtkWidget* hbox_target;
    GtkWidget* browse_target;

    GtkLabel* label_template;
    GtkComboBox* combo_template;
    GtkComboBox* combo_template_dir;
    GtkWidget* hbox_template;
    GtkWidget* browse_template;

    GtkLabel* label_name;
    GtkWidget* scroll_name;
    GtkWidget* input_name;
    GtkTextBuffer* buf_name;
    GtkLabel* blank_name;

    GtkWidget* hbox_ext;
    GtkLabel* label_ext;
    GtkEntry* entry_ext;

    GtkLabel* label_full_name;
    GtkWidget* scroll_full_name;
    GtkWidget* input_full_name;
    GtkTextBuffer* buf_full_name;
    GtkLabel* blank_full_name;

    GtkLabel* label_path;
    GtkWidget* scroll_path;
    GtkWidget* input_path;
    GtkTextBuffer* buf_path;
    GtkLabel* blank_path;

    GtkLabel* label_full_path;
    GtkWidget* scroll_full_path;
    GtkWidget* input_full_path;
    GtkTextBuffer* buf_full_path;

    GtkWidget* opt_move;
    GtkWidget* opt_copy;
    GtkWidget* opt_link;
    GtkWidget* opt_copy_target;
    GtkWidget* opt_link_target;
    GtkWidget* opt_as_root;

    GtkWidget* opt_new_file;
    GtkWidget* opt_new_folder;
    GtkWidget* opt_new_link;

    GtkWidget* options;
    GtkWidget* browse;
    GtkWidget* revert;
    GtkWidget* cancel;
    GtkWidget* next;
    GtkWidget* open;

    GtkWidget* last_widget;

    bool full_path_exists;
    bool full_path_exists_dir;
    bool full_path_same;
    bool path_missing;
    bool path_exists_file;
    bool mode_change;
    bool is_move;
};

static void on_toggled(GtkMenuItem* item, MoveSet* mset);
static char* get_template_dir();

void
ptk_delete_files(GtkWindow* parent_win, const char* cwd, GList* sel_files, GtkTreeView* task_view)
{
    if (!sel_files)
        return;

    if (!app_settings.no_confirm)
    {
        // count
        int count = g_list_length(sel_files);
        std::string msg = fmt::format("Delete {} selected item ?", count);
        GtkWidget* dlg = gtk_message_dialog_new(parent_win,
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_WARNING,
                                                GTK_BUTTONS_YES_NO,
                                                msg.c_str(),
                                                nullptr);
        gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_YES); // MOD
        gtk_window_set_title(GTK_WINDOW(dlg), "Confirm Delete");
        xset_set_window_icon(GTK_WINDOW(dlg));

        int ret = gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        if (ret != GTK_RESPONSE_YES)
            return;
    }

    std::vector<std::string> file_list;
    GList* sel;
    for (sel = sel_files; sel; sel = g_list_next(sel))
    {
        VFSFileInfo* file = static_cast<VFSFileInfo*>(sel->data);
        char* file_path = g_build_filename(cwd, vfs_file_info_get_name(file), nullptr);
        file_list.push_back(file_path);
    }
    /* file_list = g_list_reverse( file_list ); */
    PtkFileTask* ptask = ptk_file_task_new(VFSFileTaskType::VFS_FILE_TASK_DELETE,
                                           file_list,
                                           nullptr,
                                           parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                           GTK_WIDGET(task_view));
    ptk_file_task_run(ptask);
}

void
ptk_trash_files(GtkWindow* parent_win, const char* cwd, GList* sel_files, GtkTreeView* task_view)
{
    if (!sel_files)
        return;

    if (!app_settings.no_confirm_trash)
    {
        // count
        int count = g_list_length(sel_files);
        std::string msg = fmt::format("Trash {} selected item ?", count);
        GtkWidget* dlg = gtk_message_dialog_new(parent_win,
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_WARNING,
                                                GTK_BUTTONS_YES_NO,
                                                msg.c_str(),
                                                nullptr);
        gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_YES); // MOD
        gtk_window_set_title(GTK_WINDOW(dlg), "Confirm Trash");
        xset_set_window_icon(GTK_WINDOW(dlg));

        int ret = gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        if (ret != GTK_RESPONSE_YES)
            return;
    }

    std::vector<std::string> file_list;
    GList* sel;
    for (sel = sel_files; sel; sel = g_list_next(sel))
    {
        VFSFileInfo* file = static_cast<VFSFileInfo*>(sel->data);
        char* file_path = g_build_filename(cwd, vfs_file_info_get_name(file), nullptr);
        file_list.push_back(file_path);
    }
    /* file_list = g_list_reverse( file_list ); */
    PtkFileTask* ptask = ptk_file_task_new(VFSFileTaskType::VFS_FILE_TASK_TRASH,
                                           file_list,
                                           nullptr,
                                           parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                           GTK_WIDGET(task_view));
    ptk_file_task_run(ptask);
}

char*
get_real_link_target(const char* link_path)
{
    char buf[PATH_MAX + 1];
    char* target_path;

    if (!link_path)
        return nullptr;

    // canonicalize target
    if (!(target_path = ztd::strdup(realpath(link_path, buf))))
    {
        /* fall back to immediate target if canonical target
         * missing.
         * g_file_read_link() does not behave like readlink,
         * gives nothing if final target missing */
        ssize_t len = readlink(link_path, buf, PATH_MAX);
        if (len > 0)
            target_path = g_strndup(buf, len);
    }
    return target_path;
}

static bool
on_move_keypress(GtkWidget* widget, GdkEventKey* event, MoveSet* mset)
{
    (void)widget;
    unsigned int keymod = ptk_get_keymod(event->state);

    if (keymod == 0)
    {
        switch (event->keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                if (gtk_widget_get_sensitive(GTK_WIDGET(mset->next)))
                    gtk_dialog_response(GTK_DIALOG(mset->dlg), GTK_RESPONSE_OK);
                return true;
            default:
                break;
        }
    }
    return false;
}

static bool
on_move_entry_keypress(GtkWidget* widget, GdkEventKey* event, MoveSet* mset)
{
    (void)widget;
    unsigned int keymod = ptk_get_keymod(event->state);

    if (keymod == 0)
    {
        switch (event->keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                if (gtk_widget_get_sensitive(GTK_WIDGET(mset->next)))
                    gtk_dialog_response(GTK_DIALOG(mset->dlg), GTK_RESPONSE_OK);
                return true;
            default:
                break;
        }
    }
    return false;
}

static void
on_move_change(GtkWidget* widget, MoveSet* mset)
{
    char* full_path;
    char* path;
    char* cwd;
    char* str;
    GtkTextIter iter, siter;

    g_signal_handlers_block_matched(mset->entry_ext,
                                    G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);
    g_signal_handlers_block_matched(mset->buf_name,
                                    G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);
    g_signal_handlers_block_matched(mset->buf_full_name,
                                    G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);
    g_signal_handlers_block_matched(mset->buf_path,
                                    G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);
    g_signal_handlers_block_matched(mset->buf_full_path,
                                    G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);

    // change is_dir to reflect state of new directory or link option
    if (mset->create_new)
    {
        bool new_folder = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
        bool new_link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));
        if (new_folder ||
            (new_link &&
             std::filesystem::is_directory(gtk_entry_get_text(GTK_ENTRY(mset->entry_target))) &&
             gtk_entry_get_text(GTK_ENTRY(mset->entry_target))[0] == '/'))
        {
            if (!mset->is_dir)
                mset->is_dir = true;
        }
        else if (mset->is_dir)
            mset->is_dir = false;
        if (mset->is_dir && gtk_widget_is_focus(GTK_WIDGET(mset->entry_ext)))
            gtk_widget_grab_focus(GTK_WIDGET(mset->input_name));
        gtk_widget_set_sensitive(GTK_WIDGET(mset->entry_ext), !mset->is_dir);
        gtk_widget_set_sensitive(GTK_WIDGET(mset->label_ext), !mset->is_dir);
    }

    if (widget == GTK_WIDGET(mset->buf_name) || widget == GTK_WIDGET(mset->entry_ext))
    {
        std::string full_name;
        char* name;
        char* ext;

        if (widget == GTK_WIDGET(mset->buf_name))
            mset->last_widget = GTK_WIDGET(mset->input_name);
        else
            mset->last_widget = GTK_WIDGET(mset->entry_ext);

        gtk_text_buffer_get_start_iter(mset->buf_name, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_name, &iter);
        name = gtk_text_buffer_get_text(mset->buf_name, &siter, &iter, false);
        if (name[0] == '\0')
        {
            free(name);
            name = nullptr;
        }
        ext = (char*)gtk_entry_get_text(mset->entry_ext);
        if (ext[0] == '\0')
        {
            ext = nullptr;
        }
        else if (ext[0] == '.')
            ext++; // ignore leading dot in extension field

        // update full_name
        if (name && ext)
            full_name = fmt::format("{}.{}", name, ext);
        else if (name && !ext)
            full_name = name;
        else if (!name && ext)
            full_name = ext;
        else
            full_name = "";
        if (name)
            free(name);
        gtk_text_buffer_set_text(mset->buf_full_name, full_name.c_str(), -1);

        // update full_path
        gtk_text_buffer_get_start_iter(mset->buf_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_path, &iter);
        path = gtk_text_buffer_get_text(mset->buf_path, &siter, &iter, false);
        if (!strcmp(path, "."))
        {
            free(path);
            path = g_path_get_dirname(mset->full_path);
        }
        else if (!strcmp(path, ".."))
        {
            free(path);
            cwd = g_path_get_dirname(mset->full_path);
            path = g_path_get_dirname(cwd);
            free(cwd);
        }

        if (path[0] == '/')
            full_path = g_build_filename(path, full_name.c_str(), nullptr);
        else
        {
            cwd = g_path_get_dirname(mset->full_path);
            full_path = g_build_filename(cwd, path, full_name.c_str(), nullptr);
            free(cwd);
        }
        gtk_text_buffer_set_text(mset->buf_full_path, full_path, -1);
    }
    else if (widget == GTK_WIDGET(mset->buf_full_name))
    {
        char* full_name;

        mset->last_widget = GTK_WIDGET(mset->input_full_name);
        // update name & ext
        std::string name;
        std::string ext;

        gtk_text_buffer_get_start_iter(mset->buf_full_name, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_name, &iter);
        full_name = gtk_text_buffer_get_text(mset->buf_full_name, &siter, &iter, false);
        name = get_name_extension(full_name, ext);
        gtk_text_buffer_set_text(mset->buf_name, name.c_str(), -1);
        if (!ext.empty())
            gtk_entry_set_text(mset->entry_ext, ext.c_str());
        else
            gtk_entry_set_text(mset->entry_ext, "");

        // update full_path
        gtk_text_buffer_get_start_iter(mset->buf_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_path, &iter);
        path = gtk_text_buffer_get_text(mset->buf_path, &siter, &iter, false);
        if (!strcmp(path, "."))
        {
            free(path);
            path = g_path_get_dirname(mset->full_path);
        }
        else if (!strcmp(path, ".."))
        {
            free(path);
            cwd = g_path_get_dirname(mset->full_path);
            path = g_path_get_dirname(cwd);
            free(cwd);
        }

        if (path[0] == '/')
            full_path = g_build_filename(path, full_name, nullptr);
        else
        {
            cwd = g_path_get_dirname(mset->full_path);
            full_path = g_build_filename(cwd, path, full_name, nullptr);
            free(cwd);
        }
        gtk_text_buffer_set_text(mset->buf_full_path, full_path, -1);

        free(full_name);
    }
    else if (widget == GTK_WIDGET(mset->buf_path))
    {
        char* full_name;

        mset->last_widget = GTK_WIDGET(mset->input_path);
        // update full_path
        gtk_text_buffer_get_start_iter(mset->buf_full_name, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_name, &iter);
        full_name = gtk_text_buffer_get_text(mset->buf_full_name, &siter, &iter, false);

        gtk_text_buffer_get_start_iter(mset->buf_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_path, &iter);
        path = gtk_text_buffer_get_text(mset->buf_path, &siter, &iter, false);
        if (!strcmp(path, "."))
        {
            free(path);
            path = g_path_get_dirname(mset->full_path);
        }
        else if (!strcmp(path, ".."))
        {
            free(path);
            cwd = g_path_get_dirname(mset->full_path);
            path = g_path_get_dirname(cwd);
            free(cwd);
        }

        if (path[0] == '/')
            full_path = g_build_filename(path, full_name, nullptr);
        else
        {
            cwd = g_path_get_dirname(mset->full_path);
            full_path = g_build_filename(cwd, path, full_name, nullptr);
            free(cwd);
        }
        gtk_text_buffer_set_text(mset->buf_full_path, full_path, -1);

        free(full_name);
    }
    else // if ( widget == GTK_WIDGET( mset->buf_full_path ) )
    {
        std::string full_name;

        mset->last_widget = GTK_WIDGET(mset->input_full_path);
        gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
        full_path = gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);

        // update name & ext
        if (full_path[0] == '\0')
            full_name = "";
        else
            full_name = g_path_get_basename(full_path);

        path = g_path_get_dirname(full_path);
        if (!strcmp(path, "."))
        {
            free(path);
            path = g_path_get_dirname(mset->full_path);
        }
        else if (!strcmp(path, ".."))
        {
            free(path);
            cwd = g_path_get_dirname(mset->full_path);
            path = g_path_get_dirname(cwd);
            free(cwd);
        }
        else if (path[0] != '/')
        {
            cwd = g_path_get_dirname(mset->full_path);
            str = path;
            path = g_build_filename(cwd, str, nullptr);
            free(str);
            free(cwd);
        }
        std::string name;
        std::string ext;

        name = get_name_extension(full_name, ext);
        gtk_text_buffer_set_text(mset->buf_name, name.c_str(), -1);
        if (!ext.empty())
            gtk_entry_set_text(mset->entry_ext, ext.c_str());
        else
            gtk_entry_set_text(mset->entry_ext, "");

        // update full_name
        if (!name.empty() && !ext.empty())
            full_name = fmt::format("{}.{}", name, ext);
        else if (!name.empty() && ext.empty())
            full_name = name;
        else if (name.empty() && !ext.empty())
            full_name = ext;
        else
            full_name = "";
        gtk_text_buffer_set_text(mset->buf_full_name, full_name.c_str(), -1);

        // update path
        gtk_text_buffer_set_text(mset->buf_path, path, -1);

        if (full_path[0] != '/')
        {
            // update full_path for tests below
            cwd = g_path_get_dirname(mset->full_path);
            str = full_path;
            full_path = g_build_filename(cwd, str, nullptr);
            free(str);
            free(cwd);
        }
    }

    // change relative path to absolute
    if (path[0] != '/')
    {
        free(path);
        path = g_path_get_dirname(full_path);
    }

    // LOG_INFO("path={}   full={}", path, full_path);

    // tests
    struct stat statbuf;
    bool full_path_exists = false;
    bool full_path_exists_dir = false;
    bool full_path_same = false;
    bool path_missing = false;
    bool path_exists_file = false;
    bool is_move = false;

    if (!strcmp(full_path, mset->full_path))
    {
        full_path_same = true;
        if (mset->create_new && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link)))
        {
            if (lstat(full_path, &statbuf) == 0)
            {
                full_path_exists = true;
                if (std::filesystem::is_directory(full_path))
                    full_path_exists_dir = true;
            }
        }
    }
    else
    {
        if (lstat(full_path, &statbuf) == 0)
        {
            full_path_exists = true;
            if (std::filesystem::is_directory(full_path))
                full_path_exists_dir = true;
        }
        else if (lstat(path, &statbuf) == 0)
        {
            if (!std::filesystem::is_directory(path))
                path_exists_file = true;
        }
        else
            path_missing = true;

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move)))
        {
            is_move = strcmp(path, mset->old_path);
        }
    }
    free(path);
    free(full_path);

    /*
        LOG_INFO("TEST")
        LOG_INFO( "  full_path_same {} {}", full_path_same, mset->full_path_same);
        LOG_INFO( "  full_path_exists {} {}", full_path_exists, mset->full_path_exists);
        LOG_INFO( "  full_path_exists_dir {} {}", full_path_exists_dir, mset->full_path_exists_dir);
        LOG_INFO( "  path_missing {} {}", path_missing, mset->path_missing);
        LOG_INFO( "  path_exists_file {} {}", path_exists_file, mset->path_exists_file);
    */
    // update display
    if (mset->full_path_same != full_path_same || mset->full_path_exists != full_path_exists ||
        mset->full_path_exists_dir != full_path_exists_dir || mset->path_missing != path_missing ||
        mset->path_exists_file != path_exists_file || mset->mode_change)
    {
        // state change
        mset->full_path_exists = full_path_exists;
        mset->full_path_exists_dir = full_path_exists_dir;
        mset->path_missing = path_missing;
        mset->path_exists_file = path_exists_file;
        mset->full_path_same = full_path_same;
        mset->mode_change = false;

        if (full_path_same && (mset->create_new == PtkRenameMode::PTK_RENAME ||
                               mset->create_new == PtkRenameMode::PTK_RENAME_NEW_LINK))
        {
            gtk_widget_set_sensitive(
                mset->next,
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move)));
            gtk_label_set_markup_with_mnemonic(mset->label_full_path,
                                               "<b>P_ath:</b>   <i>original</i>");
            gtk_label_set_markup_with_mnemonic(mset->label_name, "<b>_Name:</b>   <i>original</i>");
            gtk_label_set_markup_with_mnemonic(mset->label_full_name,
                                               "<b>_Filename:</b>   <i>original</i>");
            gtk_label_set_markup_with_mnemonic(mset->label_path,
                                               "<b>_Parent:</b>   <i>original</i>");
        }
        else if (full_path_exists_dir)
        {
            gtk_widget_set_sensitive(mset->next, false);
            gtk_label_set_markup_with_mnemonic(mset->label_full_path,
                                               "<b>P_ath:</b>   <i>exists as directory</i>");
            gtk_label_set_markup_with_mnemonic(mset->label_name,
                                               "<b>_Name:</b>   <i>exists as directory</i>");
            gtk_label_set_markup_with_mnemonic(mset->label_full_name,
                                               "<b>_Filename:</b>   <i>exists as directory</i>");
            gtk_label_set_markup_with_mnemonic(mset->label_path, "<b>_Parent:</b>");
        }
        else if (full_path_exists)
        {
            if (mset->is_dir)
            {
                gtk_widget_set_sensitive(mset->next, false);
                gtk_label_set_markup_with_mnemonic(mset->label_full_path,
                                                   "<b>P_ath:</b>   <i>exists as file</i>");
                gtk_label_set_markup_with_mnemonic(mset->label_name,
                                                   "<b>_Name:</b>   <i>exists as file</i>");
                gtk_label_set_markup_with_mnemonic(mset->label_full_name,
                                                   "<b>_Filename:</b>   <i>exists as file</i>");
                gtk_label_set_markup_with_mnemonic(mset->label_path, "<b>_Parent:</b>");
            }
            else
            {
                gtk_widget_set_sensitive(mset->next, true);
                gtk_label_set_markup_with_mnemonic(
                    mset->label_full_path,
                    "<b>P_ath:</b>   <i>* overwrite existing file</i>");
                gtk_label_set_markup_with_mnemonic(
                    mset->label_name,
                    "<b>_Name:</b>   <i>* overwrite existing file</i>");
                gtk_label_set_markup_with_mnemonic(
                    mset->label_full_name,
                    "<b>_Filename:</b>   <i>* overwrite existing file</i>");
                gtk_label_set_markup_with_mnemonic(mset->label_path, "<b>_Parent:</b>");
            }
        }
        else if (path_exists_file)
        {
            gtk_widget_set_sensitive(mset->next, false);
            gtk_label_set_markup_with_mnemonic(mset->label_full_path,
                                               "<b>P_ath:</b>   <i>parent exists as file</i>");
            gtk_label_set_markup_with_mnemonic(mset->label_name, "<b>_Name:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_full_name, "<b>_Filename:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_path,
                                               "<b>_Parent:</b>   <i>parent exists as file</i>");
        }
        else if (path_missing)
        {
            gtk_widget_set_sensitive(mset->next, true);
            gtk_label_set_markup_with_mnemonic(mset->label_full_path,
                                               "<b>P_ath:</b>   <i>* create parent</i>");
            gtk_label_set_markup_with_mnemonic(mset->label_name, "<b>_Name:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_full_name, "<b>_Filename:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_path,
                                               "<b>_Parent:</b>   <i>* create parent</i>");
        }
        else
        {
            gtk_widget_set_sensitive(mset->next, true);
            gtk_label_set_markup_with_mnemonic(mset->label_full_path, "<b>P_ath:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_name, "<b>_Name:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_full_name, "<b>_Filename:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_path, "<b>_Parent:</b>");
        }
    }

    if (is_move != mset->is_move && !mset->create_new)
    {
        mset->is_move = is_move;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move)))
            gtk_button_set_label(GTK_BUTTON(mset->next), is_move != 0 ? "_Move" : "_Rename");
    }

    if (mset->create_new && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link)))
    {
        path = ztd::strdup(gtk_entry_get_text(GTK_ENTRY(mset->entry_target)));
        g_strstrip(path);
        gtk_widget_set_sensitive(mset->next,
                                 (path && path[0] != '\0' &&
                                  !(full_path_same && full_path_exists) && !full_path_exists_dir));
        free(path);
    }

    if (mset->open)
        gtk_widget_set_sensitive(mset->open, gtk_widget_get_sensitive(mset->next));

    g_signal_handlers_unblock_matched(mset->entry_ext,
                                      G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
    g_signal_handlers_unblock_matched(mset->buf_name,
                                      G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
    g_signal_handlers_unblock_matched(mset->buf_full_name,
                                      G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
    g_signal_handlers_unblock_matched(mset->buf_path,
                                      G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
    g_signal_handlers_unblock_matched(mset->buf_full_path,
                                      G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
}

static void
select_input(GtkWidget* widget, MoveSet* mset)
{
    if (GTK_IS_EDITABLE(widget))
        gtk_editable_select_region(GTK_EDITABLE(widget), 0, -1);
    else if (GTK_IS_COMBO_BOX(widget))
        gtk_editable_select_region(GTK_EDITABLE(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(widget)))),
                                   0,
                                   -1);
    else
    {
        GtkTextIter iter, siter;
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
        if (widget == GTK_WIDGET(mset->input_full_name) &&
            !gtk_widget_get_visible(gtk_widget_get_parent(mset->input_name)))
        {
            // name is not visible so select name in filename
            gtk_text_buffer_get_start_iter(mset->buf_full_name, &siter);
            gtk_text_buffer_get_end_iter(mset->buf_full_name, &iter);
            char* full_name = gtk_text_buffer_get_text(mset->buf_full_name, &siter, &iter, false);
            std::string ext;
            std::string name = get_name_extension(full_name, ext);
            free(full_name);
            gtk_text_buffer_get_iter_at_offset(buf, &iter, g_utf8_strlen(name.c_str(), -1));
        }
        else
            gtk_text_buffer_get_end_iter(buf, &iter);

        gtk_text_buffer_get_start_iter(buf, &siter);
        gtk_text_buffer_select_range(buf, &iter, &siter);
    }
}

static bool
on_focus(GtkWidget* widget, GtkDirectionType direction, MoveSet* mset)
{
    (void)direction;
    select_input(widget, mset);
    return false;
}

static bool
on_button_focus(GtkWidget* widget, GtkDirectionType direction, MoveSet* mset)
{
    if (direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD)
    {
        if (widget == mset->options || widget == mset->opt_move || widget == mset->opt_new_file)
        {
            GtkWidget* input = nullptr;
            if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_name)))
                input = mset->input_name;
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_name)))
                input = mset->input_full_name;
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_path)))
                input = mset->input_path;
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_path)))
                input = mset->input_full_path;
            else if (gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(mset->entry_target))))
                input = GTK_WIDGET(mset->entry_target);
            else if (gtk_widget_get_visible(
                         gtk_widget_get_parent(GTK_WIDGET(mset->combo_template))))
                input = GTK_WIDGET(mset->combo_template);
            else if (gtk_widget_get_visible(
                         gtk_widget_get_parent(GTK_WIDGET(mset->combo_template_dir))))
                input = GTK_WIDGET(mset->combo_template_dir);
            if (input)
            {
                select_input(input, mset);
                gtk_widget_grab_focus(input);
            }
        }
        else
        {
            GtkWidget* input = nullptr;
            if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_path)))
                input = mset->input_full_path;
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_path)))
                input = mset->input_path;
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_name)))
                input = mset->input_full_name;
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_name)))
                input = mset->input_name;
            if (input)
            {
                select_input(input, mset);
                gtk_widget_grab_focus(input);
            }
        }
        return true;
    }
    return false;
}

static void
on_revert_button_press(GtkWidget* widget, MoveSet* mset)
{
    (void)widget;
    GtkWidget* temp = mset->last_widget;
    gtk_text_buffer_set_text(mset->buf_full_path, mset->new_path, -1);
    mset->last_widget = temp;
    select_input(mset->last_widget, mset);
    gtk_widget_grab_focus(mset->last_widget);
}

static void
on_create_browse_button_press(GtkWidget* widget, MoveSet* mset)
{
    int action;
    const char* title;
    const char* text;
    char* dir;
    char* name;

    if (widget == GTK_WIDGET(mset->browse_target))
    {
        title = "Select Link Target";
        action = GTK_FILE_CHOOSER_ACTION_OPEN;
        text = gtk_entry_get_text(mset->entry_target);
        if (text[0] == '/')
        {
            dir = g_path_get_dirname(text);
            name = g_path_get_basename(text);
        }
        else
        {
            dir = g_path_get_dirname(mset->full_path);
            name = text[0] != '\0' ? ztd::strdup(text) : nullptr;
        }
    }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
    {
        title = "Select Template File";
        action = GTK_FILE_CHOOSER_ACTION_OPEN;
        text = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(mset->combo_template))));
        if (text && text[0] == '/')
        {
            dir = g_path_get_dirname(text);
            name = g_path_get_basename(text);
        }
        else
        {
            dir = get_template_dir();
            if (!dir)
                dir = g_path_get_dirname(mset->full_path);
            name = ztd::strdup(text);
        }
    }
    else
    {
        title = "Select Template Directory";
        action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        text = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(mset->combo_template))));
        if (text && text[0] == '/')
        {
            dir = g_path_get_dirname(text);
            name = g_path_get_basename(text);
        }
        else
        {
            dir = get_template_dir();
            if (!dir)
                dir = g_path_get_dirname(mset->full_path);
            name = ztd::strdup(text);
        }
    }

    GtkWidget* dlg = gtk_file_chooser_dialog_new(title,
                                                 mset->parent ? GTK_WINDOW(mset->parent) : nullptr,
                                                 (GtkFileChooserAction)action,
                                                 "Cancel",
                                                 GTK_RESPONSE_CANCEL,
                                                 "OK",
                                                 GTK_RESPONSE_OK,
                                                 nullptr);

    xset_set_window_icon(GTK_WINDOW(dlg));

    if (!name)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), dir);
    else
    {
        char* path = g_build_filename(dir, name, nullptr);
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), path);
        free(path);
    }
    free(dir);
    free(name);

    int width = xset_get_int("move_dlg_help", "x");
    int height = xset_get_int("move_dlg_help", "y");
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(dlg);
        gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_resize(GTK_WINDOW(dlg), width, height);
        while (gtk_events_pending())
            gtk_main_iteration();
        gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
    }

    int response = gtk_dialog_run(GTK_DIALOG(dlg));
    if (response == GTK_RESPONSE_OK)
    {
        char* new_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        char* path = new_path;
        GtkWidget* w;
        if (widget == GTK_WIDGET(mset->browse_target))
            w = GTK_WIDGET(mset->entry_target);
        else
        {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
                w = gtk_bin_get_child(GTK_BIN(mset->combo_template));
            else
                w = gtk_bin_get_child(GTK_BIN(mset->combo_template_dir));
            dir = get_template_dir();
            if (dir)
            {
                if (Glib::str_has_prefix(new_path, dir) && new_path[std::strlen(dir)] == '/')
                    path = new_path + std::strlen(dir) + 1;
                free(dir);
            }
        }
        gtk_entry_set_text(GTK_ENTRY(w), path);
        free(new_path);
    }

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if (width && height)
    {
        xset_set("move_dlg_help", "x", std::to_string(width).c_str());
        xset_set("move_dlg_help", "y", std::to_string(height).c_str());
    }

    gtk_widget_destroy(dlg);
}

enum PTKFileMiscMode
{
    MODE_FILENAME,
    MODE_PARENT,
    MODE_PATH
};

static void
on_browse_mode_toggled(GtkMenuItem* item, GtkWidget* dlg)
{
    (void)item;
    int i;
    GtkWidget** mode = (GtkWidget**)g_object_get_data(G_OBJECT(dlg), "mode");

    for (i = MODE_FILENAME; i <= MODE_PATH; i++)
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode[i])))
        {
            int action = i == MODE_PARENT ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                                          : GTK_FILE_CHOOSER_ACTION_SAVE;
            GtkAllocation allocation;
            gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
            int width = allocation.width;
            int height = allocation.height;
            gtk_file_chooser_set_action(GTK_FILE_CHOOSER(dlg), (GtkFileChooserAction)action);
            if (width && height)
            {
                // under some circumstances, changing the action changes the size
                gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
                gtk_window_resize(GTK_WINDOW(dlg), width, height);
                while (gtk_events_pending())
                    gtk_main_iteration();
                gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
            }
            return;
        }
    }
}

static void
on_browse_button_press(GtkWidget* widget, MoveSet* mset)
{
    (void)widget;
    GtkTextIter iter;
    GtkTextIter siter;
    int mode_default = MODE_PARENT;

    XSet* set = xset_get("move_dlg_help");
    if (set->z)
        mode_default = xset_get_int("move_dlg_help", "z");

    // action create directory does not work properly so not used:
    //  it creates a directory by default with no way to stop it
    //  it gives 'directory already exists' error popup
    GtkWidget* dlg = gtk_file_chooser_dialog_new("Browse",
                                                 mset->parent ? GTK_WINDOW(mset->parent) : nullptr,
                                                 mode_default == MODE_PARENT
                                                     ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                                                     : GTK_FILE_CHOOSER_ACTION_SAVE,
                                                 "Cancel",
                                                 GTK_RESPONSE_CANCEL,
                                                 "OK",
                                                 GTK_RESPONSE_OK,
                                                 nullptr);
    gtk_window_set_role(GTK_WINDOW(dlg), "file_dialog");

    gtk_text_buffer_get_start_iter(mset->buf_path, &siter);
    gtk_text_buffer_get_end_iter(mset->buf_path, &iter);
    char* path = gtk_text_buffer_get_text(mset->buf_path, &siter, &iter, false);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), path);
    free(path);

    if (mode_default != MODE_PARENT)
    {
        gtk_text_buffer_get_start_iter(mset->buf_full_name, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_name, &iter);
        path = gtk_text_buffer_get_text(mset->buf_full_name, &siter, &iter, false);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), path);
        free(path);
    }

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), false);

    // Mode
    int i;
    GtkWidget* mode[3];
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    mode[MODE_FILENAME] = gtk_radio_button_new_with_mnemonic(nullptr, "Fil_ename");
    mode[MODE_PARENT] =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mode[MODE_FILENAME]),
                                                       "Pa_rent");
    mode[MODE_PATH] =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mode[MODE_FILENAME]),
                                                       "P_ath");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mode[mode_default]), true);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Insert as"), false, true, 2);
    for (i = MODE_FILENAME; i <= MODE_PATH; i++)
    {
        gtk_widget_set_focus_on_click(GTK_WIDGET(mode[i]), false);
        g_signal_connect(G_OBJECT(mode[i]), "toggled", G_CALLBACK(on_browse_mode_toggled), dlg);
        gtk_box_pack_start(GTK_BOX(hbox), mode[i], false, true, 2);
    }
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), hbox, false, true, 6);
    g_object_set_data(G_OBJECT(dlg), "mode", mode);
    gtk_widget_show_all(hbox);

    int width = xset_get_int("move_dlg_help", "x");
    int height = xset_get_int("move_dlg_help", "y");
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(dlg);
        gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_resize(GTK_WINDOW(dlg), width, height);
        while (gtk_events_pending())
            gtk_main_iteration();
        gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
    }

    int response = gtk_dialog_run(GTK_DIALOG(dlg));
    // bogus GTK warning here: Unable to retrieve the file info for...
    if (response == GTK_RESPONSE_OK)
    {
        for (i = MODE_FILENAME; i <= MODE_PATH; i++)
        {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode[i])))
            {
                char* str;
                switch (i)
                {
                    case MODE_FILENAME:
                        path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
                        str = g_path_get_basename(path);
                        gtk_text_buffer_set_text(mset->buf_full_name, str, -1);
                        free(str);
                        break;
                    case MODE_PARENT:
                        path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dlg));
                        gtk_text_buffer_set_text(mset->buf_path, path, -1);
                        break;
                    default:
                        path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
                        gtk_text_buffer_set_text(mset->buf_full_path, path, -1);
                        break;
                }
                free(path);
                break;
            }
        }
    }

    // save size
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if (width && height)
    {
        xset_set("move_dlg_help", "x", std::to_string(width).c_str());
        xset_set("move_dlg_help", "y", std::to_string(height).c_str());
    }

    // save mode
    for (i = MODE_FILENAME; i <= MODE_PATH; i++)
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode[i])))
        {
            xset_set("move_dlg_help", "z", std::to_string(i).c_str());
            break;
        }
    }

    gtk_widget_destroy(dlg);
}

static void
on_opt_toggled(GtkMenuItem* item, MoveSet* mset)
{
    (void)item;
    const char* action;
    char* btn_label = nullptr;

    bool move = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move));
    bool copy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy));
    bool link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link));
    bool copy_target = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy_target));
    bool link_target = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link_target));
    bool as_root = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_as_root));

    bool new_file = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file));
    bool new_folder = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
    bool new_link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));

    const char* desc = nullptr;
    if (mset->create_new)
    {
        btn_label = ztd::strdup("Create");
        action = ztd::strdup("Create New");
        if (new_file)
            desc = ztd::strdup("File");
        else if (new_folder)
            desc = ztd::strdup("Directory");
        else if (new_link)
            desc = ztd::strdup("Link");
    }
    else
    {
        GtkTextIter iter;
        GtkTextIter siter;
        gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
        char* full_path = gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);
        char* new_path = g_path_get_dirname(full_path);

        bool rename = (!strcmp(mset->old_path, new_path) || !strcmp(new_path, "."));
        free(new_path);
        free(full_path);

        if (move)
        {
            btn_label = rename ? ztd::strdup("Rename") : ztd::strdup("Move");
            action = ztd::strdup("Move");
        }
        else if (copy)
        {
            btn_label = ztd::strdup("C_opy");
            action = ztd::strdup("Copy");
        }
        else if (link)
        {
            btn_label = ztd::strdup("_Link");
            action = ztd::strdup("Create Link To");
        }
        else if (copy_target)
        {
            btn_label = ztd::strdup("C_opy");
            action = ztd::strdup("Copy");
            desc = ztd::strdup("Link Target");
        }
        else if (link_target)
        {
            btn_label = ztd::strdup("_Link");
            action = ztd::strdup("Create Link To");
            desc = ztd::strdup("Target");
        }
    }

    const char* root_msg;
    if (as_root)
        root_msg = ztd::strdup(" As Root");
    else
        root_msg = ztd::strdup("");

    // Window Icon
    const char* win_icon;
    if (as_root)
        win_icon = ztd::strdup("gtk-dialog-warning");
    else if (mset->create_new)
        win_icon = ztd::strdup("gtk-new");
    else
        win_icon = ztd::strdup("gtk-edit");
    GdkPixbuf* pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                                 win_icon,
                                                 16,
                                                 GTK_ICON_LOOKUP_USE_BUILTIN,
                                                 nullptr);
    if (pixbuf)
        gtk_window_set_icon(GTK_WINDOW(mset->dlg), pixbuf);

    // title
    if (!desc)
        desc = mset->desc;
    std::string title = fmt::format("{} {}{}", action, desc, root_msg);
    gtk_window_set_title(GTK_WINDOW(mset->dlg), title.c_str());

    if (btn_label)
        gtk_button_set_label(GTK_BUTTON(mset->next), btn_label);

    mset->full_path_same = false;
    mset->mode_change = true;
    on_move_change(GTK_WIDGET(mset->buf_full_path), mset);
    if (mset->create_new)
        on_toggled(nullptr, mset);
}

static void
on_toggled(GtkMenuItem* item, MoveSet* mset)
{
    (void)item;
    bool someone_is_visible = false;
    bool opts_visible = false;

    // opts
    if (xset_get_b("move_copy") || mset->clip_copy)
        gtk_widget_show(mset->opt_copy);
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        gtk_widget_hide(mset->opt_copy);
    }

    if (xset_get_b("move_link"))
    {
        gtk_widget_show(mset->opt_link);
    }
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        gtk_widget_hide(mset->opt_link);
    }

    if (xset_get_b("move_copyt") && mset->is_link)
        gtk_widget_show(mset->opt_copy_target);
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy_target)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        gtk_widget_hide(mset->opt_copy_target);
    }

    if (xset_get_b("move_linkt") && mset->is_link)
        gtk_widget_show(mset->opt_link_target);
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link_target)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        gtk_widget_hide(mset->opt_link_target);
    }

    if (xset_get_b("move_as_root"))
        gtk_widget_show(mset->opt_as_root);
    else
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_as_root), false);
        gtk_widget_hide(mset->opt_as_root);
    }

    if (!gtk_widget_get_visible(mset->opt_copy) && !gtk_widget_get_visible(mset->opt_link) &&
        !gtk_widget_get_visible(mset->opt_copy_target) &&
        !gtk_widget_get_visible(mset->opt_link_target))
    {
        gtk_widget_hide(mset->opt_move);
        opts_visible = gtk_widget_get_visible(mset->opt_as_root);
    }
    else
    {
        gtk_widget_show(mset->opt_move);
        opts_visible = true;
    }

    // entries
    if (xset_get_b("move_name"))
    {
        someone_is_visible = true;

        gtk_widget_show(GTK_WIDGET(mset->label_name));
        gtk_widget_show(GTK_WIDGET(mset->scroll_name));
        gtk_widget_show(GTK_WIDGET(mset->hbox_ext));
        gtk_widget_show(GTK_WIDGET(mset->blank_name));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->label_name));
        gtk_widget_hide(GTK_WIDGET(mset->scroll_name));
        gtk_widget_hide(GTK_WIDGET(mset->hbox_ext));
        gtk_widget_hide(GTK_WIDGET(mset->blank_name));
    }

    if (xset_get_b("move_filename"))
    {
        someone_is_visible = true;

        gtk_widget_show(GTK_WIDGET(mset->label_full_name));
        gtk_widget_show(GTK_WIDGET(mset->scroll_full_name));
        gtk_widget_show(GTK_WIDGET(mset->blank_full_name));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->label_full_name));
        gtk_widget_hide(GTK_WIDGET(mset->scroll_full_name));
        gtk_widget_hide(GTK_WIDGET(mset->blank_full_name));
    }

    if (xset_get_b("move_parent"))
    {
        someone_is_visible = true;

        gtk_widget_show(GTK_WIDGET(mset->label_path));
        gtk_widget_show(GTK_WIDGET(mset->scroll_path));
        gtk_widget_show(GTK_WIDGET(mset->blank_path));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->label_path));
        gtk_widget_hide(GTK_WIDGET(mset->scroll_path));
        gtk_widget_hide(GTK_WIDGET(mset->blank_path));
    }

    if (xset_get_b("move_path"))
    {
        someone_is_visible = true;

        gtk_widget_show(GTK_WIDGET(mset->label_full_path));
        gtk_widget_show(GTK_WIDGET(mset->scroll_full_path));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->label_full_path));
        gtk_widget_hide(GTK_WIDGET(mset->scroll_full_path));
    }

    if (!mset->is_link && !mset->create_new && xset_get_b("move_type"))
    {
        gtk_widget_show(mset->hbox_type);
    }
    else
    {
        gtk_widget_hide(mset->hbox_type);
    }

    bool new_file = false;
    bool new_folder = false;
    bool new_link = false;
    if (mset->create_new)
    {
        new_file = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file));
        new_folder = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
        new_link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));
    }

    if (new_link || (mset->is_link && xset_get_b("move_target")))
    {
        gtk_widget_show(GTK_WIDGET(mset->hbox_target));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->hbox_target));
    }

    if ((new_file || new_folder) && xset_get_b("move_template"))
    {
        if (new_file)
        {
            gtk_widget_show(GTK_WIDGET(mset->combo_template));
            gtk_label_set_mnemonic_widget(mset->label_template, GTK_WIDGET(mset->combo_template));
            gtk_widget_hide(GTK_WIDGET(mset->combo_template_dir));
        }
        else
        {
            gtk_widget_show(GTK_WIDGET(mset->combo_template_dir));
            gtk_label_set_mnemonic_widget(mset->label_template,
                                          GTK_WIDGET(mset->combo_template_dir));
            gtk_widget_hide(GTK_WIDGET(mset->combo_template));
        }

        gtk_widget_show(GTK_WIDGET(mset->hbox_template));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->hbox_template));
    }

    if (!someone_is_visible)
    {
        xset_set_b("move_filename", true);
        on_toggled(nullptr, mset);
    }

    if (opts_visible)
    {
        if (gtk_widget_get_visible(mset->hbox_type))
        {
        }
        else if (gtk_widget_get_visible(GTK_WIDGET(mset->label_full_path)))
        {
        }
        else if (gtk_widget_get_visible(GTK_WIDGET(mset->blank_path)))
            gtk_widget_hide(GTK_WIDGET(mset->blank_path));
        else if (gtk_widget_get_visible(GTK_WIDGET(mset->blank_full_name)))
            gtk_widget_hide(GTK_WIDGET(mset->blank_full_name));
        else if (gtk_widget_get_visible(GTK_WIDGET(mset->blank_name)))
            gtk_widget_hide(GTK_WIDGET(mset->blank_name));
    }
}

static bool
on_mnemonic_activate(GtkWidget* widget, bool arg1, MoveSet* mset)
{
    (void)arg1;
    select_input(widget, mset);
    return false;
}

static void
on_options_button_press(GtkWidget* btn, MoveSet* mset)
{
    (void)btn;
    GtkWidget* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_context_new();

    XSet* set = xset_set_cb("move_name", (GFunc)on_toggled, mset);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb("move_filename", (GFunc)on_toggled, mset);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb("move_parent", (GFunc)on_toggled, mset);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb("move_path", (GFunc)on_toggled, mset);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb("move_type", (GFunc)on_toggled, mset);
    set->disable = (mset->create_new || mset->is_link);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb("move_target", (GFunc)on_toggled, mset);
    set->disable = mset->create_new || !mset->is_link;
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb("move_template", (GFunc)on_toggled, mset);
    set->disable = !mset->create_new;
    xset_add_menuitem(mset->browser, popup, accel_group, set);

    set = xset_set_cb("move_copy", (GFunc)on_toggled, mset);
    set->disable = mset->clip_copy || mset->create_new;
    set = xset_set_cb("move_link", (GFunc)on_toggled, mset);
    set->disable = mset->create_new;
    set = xset_set_cb("move_copyt", (GFunc)on_toggled, mset);
    set->disable = !mset->is_link;
    set = xset_set_cb("move_linkt", (GFunc)on_toggled, mset);
    set->disable = !mset->is_link;
    xset_set_cb("move_as_root", (GFunc)on_toggled, mset);
    set = xset_get("move_option");
    xset_add_menuitem(mset->browser, popup, accel_group, set);

    set = xset_get("separator");
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_get("move_dlg_confirm_create");
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_get("separator");
    xset_add_menuitem(mset->browser, popup, accel_group, set);

    gtk_widget_show_all(GTK_WIDGET(popup));
    g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
}

static bool
on_label_focus(GtkWidget* widget, GtkDirectionType direction, MoveSet* mset)
{
    GtkWidget* input = nullptr;
    GtkWidget* input2 = nullptr;
    GtkWidget* first_input = nullptr;

    switch (direction)
    {
        case GTK_DIR_TAB_FORWARD:
            if (widget == GTK_WIDGET(mset->label_name))
                input = mset->input_name;
            else if (widget == GTK_WIDGET(mset->label_ext))
                input = GTK_WIDGET(mset->entry_ext);
            else if (widget == GTK_WIDGET(mset->label_full_name))
                input = mset->input_full_name;
            else if (widget == GTK_WIDGET(mset->label_path))
                input = mset->input_path;
            else if (widget == GTK_WIDGET(mset->label_full_path))
                input = mset->input_full_path;
            else if (widget == GTK_WIDGET(mset->label_type))
            {
                on_button_focus(mset->options, GTK_DIR_TAB_FORWARD, mset);
                return true;
            }
            else if (widget == GTK_WIDGET(mset->label_target))
                input = GTK_WIDGET(mset->entry_target);
            else if (widget == GTK_WIDGET(mset->label_template))
            {
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
                    input = GTK_WIDGET(mset->combo_template);
                else
                    input = GTK_WIDGET(mset->combo_template_dir);
            }
            break;
        case GTK_DIR_TAB_BACKWARD:
            if (widget == GTK_WIDGET(mset->label_name))
            {
                if (mset->combo_template_dir)
                    input = GTK_WIDGET(mset->combo_template_dir);
                else if (mset->combo_template)
                    input = GTK_WIDGET(mset->combo_template);
                else if (mset->entry_target)
                    input = GTK_WIDGET(mset->entry_target);
                else
                    input = mset->input_full_path;
            }
            else if (widget == GTK_WIDGET(mset->label_ext))
                input = mset->input_name;
            else if (widget == GTK_WIDGET(mset->label_full_name))
            {
                if (gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(mset->entry_ext))) &&
                    gtk_widget_get_sensitive(GTK_WIDGET(mset->entry_ext)))
                    input = GTK_WIDGET(mset->entry_ext);
                else
                    input = mset->input_name;
            }
            else if (widget == GTK_WIDGET(mset->label_path))
                input = mset->input_full_name;
            else if (widget == GTK_WIDGET(mset->label_full_path))
                input = mset->input_path;
            else
                input = mset->input_full_path;

            first_input = input;
            while (input && !gtk_widget_get_visible(gtk_widget_get_parent(input)))
            {
                input2 = nullptr;
                if (input == GTK_WIDGET(mset->combo_template_dir))
                {
                    if (mset->combo_template)
                        input2 = GTK_WIDGET(mset->combo_template);
                    else if (mset->entry_target)
                        input2 = GTK_WIDGET(mset->entry_target);
                    else
                        input2 = mset->input_full_path;
                }
                else if (input == GTK_WIDGET(mset->combo_template))
                {
                    if (mset->entry_target)
                        input2 = GTK_WIDGET(mset->entry_target);
                    else
                        input2 = mset->input_full_path;
                }
                else if (input == GTK_WIDGET(mset->entry_target))
                    input2 = mset->input_full_path;
                else if (input == mset->input_full_path)
                    input2 = mset->input_path;
                else if (input == mset->input_path)
                    input2 = mset->input_full_name;
                else if (input == mset->input_full_name)
                {
                    if (gtk_widget_get_visible(
                            gtk_widget_get_parent(GTK_WIDGET(mset->entry_ext))) &&
                        gtk_widget_get_sensitive(GTK_WIDGET(mset->entry_ext)))
                        input2 = GTK_WIDGET(mset->entry_ext);
                    else
                        input2 = mset->input_name;
                }
                else if (input == GTK_WIDGET(mset->entry_ext))
                    input2 = mset->input_name;
                else if (input == mset->input_name)
                {
                    if (mset->combo_template_dir)
                        input2 = GTK_WIDGET(mset->combo_template_dir);
                    else if (mset->combo_template)
                        input2 = GTK_WIDGET(mset->combo_template);
                    else if (mset->entry_target)
                        input2 = GTK_WIDGET(mset->entry_target);
                    else
                        input2 = mset->input_full_path;
                }

                if (input2 == first_input)
                    input = nullptr;
                else
                    input = input2;
            }
            break;
        case GTK_DIR_UP:
        case GTK_DIR_DOWN:
        case GTK_DIR_LEFT:
        case GTK_DIR_RIGHT:
        default:
            break;
    }

    if (input == GTK_WIDGET(mset->label_mime))
    {
        gtk_label_select_region(mset->label_mime, 0, -1);
        gtk_widget_grab_focus(GTK_WIDGET(mset->label_mime));
    }
    else if (input)
    {
        select_input(input, mset);
        gtk_widget_grab_focus(input);
    }
    return true;
}

static void
copy_entry_to_clipboard(GtkWidget* widget, MoveSet* mset)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTextBuffer* buf = nullptr;

    if (widget == GTK_WIDGET(mset->label_name))
        buf = mset->buf_name;
    else if (widget == GTK_WIDGET(mset->label_ext))
    {
        gtk_clipboard_set_text(clip, gtk_entry_get_text(mset->entry_ext), -1);
        return;
    }
    else if (widget == GTK_WIDGET(mset->label_full_name))
        buf = mset->buf_full_name;
    else if (widget == GTK_WIDGET(mset->label_path))
        buf = mset->buf_path;
    else if (widget == GTK_WIDGET(mset->label_full_path))
        buf = mset->buf_full_path;
    else if (widget == GTK_WIDGET(mset->label_type))
    {
        gtk_clipboard_set_text(clip, mset->mime_type, -1);
        return;
    }
    else if (widget == GTK_WIDGET(mset->label_target))
    {
        gtk_clipboard_set_text(clip, gtk_entry_get_text(mset->entry_target), -1);
        return;
    }
    else if (widget == GTK_WIDGET(mset->label_template))
    {
        GtkWidget* w;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
            w = gtk_bin_get_child(GTK_BIN(mset->combo_template));
        else
            w = gtk_bin_get_child(GTK_BIN(mset->combo_template_dir));
        gtk_clipboard_set_text(clip, gtk_entry_get_text(GTK_ENTRY(w)), -1);
    }

    if (!buf)
        return;

    GtkTextIter iter;
    GtkTextIter siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    gtk_clipboard_set_text(clip, text, -1);
    free(text);
}

static bool
on_label_button_press(GtkWidget* widget, GdkEventButton* event, MoveSet* mset)
{
    if (event->type == GDK_BUTTON_PRESS)
    {
        if (event->button == 1 || event->button == 2)
        {
            GtkWidget* input = nullptr;
            if (widget == GTK_WIDGET(mset->label_name))
                input = mset->input_name;
            else if (widget == GTK_WIDGET(mset->label_ext))
                input = GTK_WIDGET(mset->entry_ext);
            else if (widget == GTK_WIDGET(mset->label_full_name))
                input = mset->input_full_name;
            else if (widget == GTK_WIDGET(mset->label_path))
                input = mset->input_path;
            else if (widget == GTK_WIDGET(mset->label_full_path))
                input = mset->input_full_path;
            else if (widget == GTK_WIDGET(mset->label_type))
            {
                gtk_label_select_region(mset->label_mime, 0, -1);
                gtk_widget_grab_focus(GTK_WIDGET(mset->label_mime));
                if (event->button == 2)
                    copy_entry_to_clipboard(widget, mset);
                return true;
            }
            else if (widget == GTK_WIDGET(mset->label_target))
                input = GTK_WIDGET(mset->entry_target);
            else if (widget == GTK_WIDGET(mset->label_template))
            {
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
                    input = GTK_WIDGET(mset->combo_template);
                else
                    input = GTK_WIDGET(mset->combo_template_dir);
            }

            if (input)
            {
                select_input(input, mset);
                gtk_widget_grab_focus(input);
                if (event->button == 2)
                    copy_entry_to_clipboard(widget, mset);
            }
        }
    }
    else if (event->type == GDK_2BUTTON_PRESS)
    {
        copy_entry_to_clipboard(widget, mset);
    }

    return true;
}

static char*
get_unique_name(const char* dir, const char* ext)
{
    std::string name;
    std::string path;

    std::string base = "new";
    if (ext && ext[0] != '\0')
    {
        name = fmt::format("{}.{}", base, ext);
        path = Glib::build_filename(dir, name);
    }
    else
        path = Glib::build_filename(dir, base);

    int n = 2;
    struct stat statbuf;
    while (lstat(path.c_str(), &statbuf) == 0) // need to see broken symlinks
    {
        if (n == 1000)
            return ztd::strdup(base);
        if (ext && ext[0] != '\0')
            name = fmt::format("{}{}.{}", base, n++, ext);
        else
            name = fmt::format("{}{}", base, n++);
        path = Glib::build_filename(dir, name);
    }
    return ztd::strdup(path);
}

static char*
get_template_dir()
{
    std::string templates_path = vfs_user_template_dir();

    if (ztd::same(templates_path, vfs_user_home_dir()))
    {
        /* If $XDG_TEMPLATES_DIR == $HOME this means it is disabled. Do not
         * recurse it as this is too many files/directories and may slow
         * dialog open and cause filesystem find loops.
         * https://wiki.freedesktop.org/www/Software/xdg-user-dirs/ */
        return nullptr;
    }

    if (!dir_has_files(templates_path))
    {
        templates_path = Glib::build_filename(vfs_user_home_dir(), "Templates");
        if (!dir_has_files(templates_path))
        {
            templates_path = Glib::build_filename(vfs_user_home_dir(), ".templates");
            if (!dir_has_files(templates_path))
            {
                templates_path = nullptr;
            }
        }
    }
    return ztd::strdup(templates_path);
}

static GList*
get_templates(const char* templates_dir, const char* subdir, GList* templates, bool getdir)
{
    char* templates_path;

    if (!templates_dir)
    {
        templates_path = get_template_dir();
        if (templates_path)
            templates = get_templates(templates_path, nullptr, templates, getdir);
        free(templates_path);
        return templates;
    }

    templates_path = g_build_filename(templates_dir, subdir, nullptr);

    if (std::filesystem::is_directory(templates_path))
    {
        std::string file_name;
        for (const auto& file: std::filesystem::directory_iterator(templates_path))
        {
            file_name = std::filesystem::path(file).filename();

            char* subsubdir;
            char* path = g_build_filename(templates_path, file_name.c_str(), nullptr);
            if (getdir)
            {
                if (std::filesystem::is_directory(path))
                {
                    if (subdir)
                        subsubdir = g_build_filename(subdir, file_name.c_str(), nullptr);
                    else
                        subsubdir = ztd::strdup(file_name);
                    std::string subsubdir_fmt = fmt::format("{}/", subsubdir);
                    templates = g_list_prepend(templates, ztd::strdup(subsubdir_fmt));
                    // prevent filesystem loops during recursive find
                    if (!std::filesystem::is_symlink(path))
                        templates = get_templates(templates_dir, subsubdir, templates, getdir);
                    free(subsubdir);
                }
            }
            else
            {
                if (std::filesystem::is_regular_file(path))
                {
                    if (subdir)
                        templates =
                            g_list_prepend(templates,
                                           g_build_filename(subdir, file_name.c_str(), nullptr));
                    else
                        templates = g_list_prepend(templates, ztd::strdup(file_name));
                }
                else if (std::filesystem::is_directory(path) &&
                         // prevent filesystem loops during recursive find
                         !std::filesystem::is_symlink(path))
                {
                    if (subdir)
                    {
                        subsubdir = g_build_filename(subdir, file_name.c_str(), nullptr);
                        templates = get_templates(templates_dir, subsubdir, templates, getdir);
                        free(subsubdir);
                    }
                    else
                        templates =
                            get_templates(templates_dir, file_name.c_str(), templates, getdir);
                }
            }
        }
    }
    free(templates_path);
    return templates;
}

static void
on_template_changed(GtkWidget* widget, MoveSet* mset)
{
    (void)widget;
    char* str = nullptr;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
        return;
    char* text = ztd::strdup(
        gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(mset->combo_template)))));
    if (text)
    {
        g_strstrip(text);
        str = text;
        char* str2;
        while ((str2 = strchr(str, '/')))
            str = str2 + 1;
        if (str[0] == '.')
            str++;
        if ((str2 = strchr(str, '.')))
            str = str2 + 1;
        else
            str = nullptr;
    }
    gtk_entry_set_text(mset->entry_ext, str ? str : "");

    // need new name due to extension added?
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
    gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
    char* full_path = gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);
    struct stat statbuf;
    if (lstat(full_path, &statbuf) == 0) // need to see broken symlinks
    {
        char* dir = g_path_get_dirname(full_path);
        free(full_path);
        full_path = get_unique_name(dir, str);
        if (full_path)
        {
            gtk_text_buffer_set_text(mset->buf_full_path, full_path, -1);
        }
        free(dir);
    }
    free(full_path);

    free(text);
}

static bool
update_new_display_delayed(char* path)
{
    char* dir_path = g_path_get_dirname(path);
    VFSDir* vdir = vfs_dir_get_by_path_soft(dir_path);
    free(dir_path);
    if (vdir && vdir->avoid_changes)
    {
        VFSFileInfo* file = vfs_file_info_new();
        vfs_file_info_get(file, path, nullptr);
        vfs_dir_emit_file_created(vdir, vfs_file_info_get_name(file), true);
        vfs_file_info_unref(file);
        vfs_dir_flush_notify_cache();
    }
    if (vdir)
        g_object_unref(vdir);
    free(path);
    return false;
}

static void
update_new_display(const char* path)
{
    // for devices like nfs, emit created so the new file is shown
    // update now
    update_new_display_delayed(ztd::strdup(path));
    // update a little later for exec tasks
    g_timeout_add(1500, (GSourceFunc)update_new_display_delayed, ztd::strdup(path));
}

int
ptk_rename_file(PtkFileBrowser* file_browser, const char* file_dir, VFSFileInfo* file,
                const char* dest_dir, bool clip_copy, PtkRenameMode create_new,
                AutoOpenCreate* auto_open)
{
    // TODO convert to gtk_builder (glade file)

    char* full_name;
    char* full_path;
    char* path;
    char* old_path;
    char* str;
    GtkWidget* task_view = nullptr;
    int ret = 1;
    bool target_missing = false;
    GList* templates;
    struct stat statbuf;

    if (!file_dir)
        return 0;
    MoveSet* mset = g_slice_new0(MoveSet);
    full_name = nullptr;

    if (!create_new)
    {
        if (!file)
            return 0;
        // special processing for files with inconsistent real name and display name
        if (vfs_file_info_is_desktop_entry(file))
            full_name = g_filename_display_name(file->name.c_str());
        if (!full_name)
            full_name = ztd::strdup(vfs_file_info_get_disp_name(file));
        if (!full_name)
            full_name = ztd::strdup(vfs_file_info_get_name(file));

        mset->is_dir = vfs_file_info_is_dir(file);
        mset->is_link = vfs_file_info_is_symlink(file);
        mset->clip_copy = clip_copy;
        mset->full_path = g_build_filename(file_dir, full_name, nullptr);
        if (dest_dir)
            mset->new_path = g_build_filename(dest_dir, full_name, nullptr);
        else
            mset->new_path = ztd::strdup(mset->full_path);
        free(full_name);
        full_name = nullptr;
    }
    else if (create_new == PtkRenameMode::PTK_RENAME_NEW_LINK && file)
    {
        full_name = ztd::strdup(vfs_file_info_get_disp_name(file));
        if (!full_name)
            full_name = ztd::strdup(vfs_file_info_get_name(file));
        mset->full_path = g_build_filename(file_dir, full_name, nullptr);
        mset->new_path = ztd::strdup(mset->full_path);
        mset->is_dir = vfs_file_info_is_dir(file); // is_dir is dynamic for create
        mset->is_link = vfs_file_info_is_symlink(file);
        mset->clip_copy = false;
    }
    else
    {
        mset->full_path = get_unique_name(file_dir, nullptr);
        mset->new_path = ztd::strdup(mset->full_path);
        mset->is_dir = false; // is_dir is dynamic for create
        mset->is_link = false;
        mset->clip_copy = false;
    }

    mset->create_new = create_new;
    mset->old_path = file_dir;

    mset->full_path_exists = false;
    mset->full_path_exists_dir = false;
    mset->full_path_same = false;
    mset->path_missing = false;
    mset->path_exists_file = false;
    mset->is_move = false;

    // Dialog
    const char* root_msg;
    if (mset->is_link)
        mset->desc = ztd::strdup("Link");
    else if (mset->is_dir)
        mset->desc = ztd::strdup("Directory");
    else
        mset->desc = ztd::strdup("File");

    mset->browser = file_browser;

    if (file_browser)
    {
        mset->parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
        task_view = file_browser->task_view;
    }

    mset->dlg = gtk_dialog_new_with_buttons(
        "Move",
        mset->parent ? GTK_WINDOW(mset->parent) : nullptr,
        GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        nullptr,
        nullptr);
    // free( title );
    gtk_window_set_role(GTK_WINDOW(mset->dlg), "rename_dialog");

    // Buttons
    mset->options = gtk_button_new_with_mnemonic("Opt_ions");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg), mset->options, GTK_RESPONSE_YES);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->options), false);
    g_signal_connect(G_OBJECT(mset->options), "clicked", G_CALLBACK(on_options_button_press), mset);

    mset->browse = gtk_button_new_with_mnemonic("_Browse");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg), mset->browse, GTK_RESPONSE_YES);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->browse), false);
    g_signal_connect(G_OBJECT(mset->browse), "clicked", G_CALLBACK(on_browse_button_press), mset);

    mset->revert = gtk_button_new_with_mnemonic("Re_vert");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg), mset->revert, GTK_RESPONSE_NO);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->revert), false);
    g_signal_connect(G_OBJECT(mset->revert), "clicked", G_CALLBACK(on_revert_button_press), mset);

    mset->cancel = gtk_button_new_with_label("Cancel");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg), mset->cancel, GTK_RESPONSE_CANCEL);

    mset->next = gtk_button_new_with_label("OK");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg), mset->next, GTK_RESPONSE_OK);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->next), false);
    gtk_button_set_label(GTK_BUTTON(mset->next), "_Rename");

    if (create_new && auto_open)
    {
        mset->open = gtk_button_new_with_mnemonic("& _Open");
        gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg), mset->open, GTK_RESPONSE_APPLY);
        gtk_widget_set_focus_on_click(GTK_WIDGET(mset->open), false);
    }
    else
        mset->open = nullptr;

    // Window
    gtk_widget_set_size_request(GTK_WIDGET(mset->dlg), 800, 500);
    gtk_window_set_resizable(GTK_WINDOW(mset->dlg), true);
    gtk_window_set_type_hint(GTK_WINDOW(mset->dlg), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_widget_show_all(mset->dlg);

    // Entries

    // Type
    std::string type;
    mset->label_type = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_type, "<b>Type:</b>");
    if (mset->is_link)
    {
        path = g_file_read_link(mset->full_path, nullptr);
        if (path)
        {
            mset->mime_type = path;
            if (std::filesystem::exists(path))
                type = fmt::format("Link-> {}", path);
            else
            {
                type = fmt::format("!Link-> {} (missing)", path);
                target_missing = true;
            }
        }
        else
        {
            mset->mime_type = ztd::strdup("inode/symlink");
            type = ztd::strdup("symbolic link ( inode/symlink )");
        }
    }
    else if (file)
    {
        VFSMimeType* mime_type = vfs_file_info_get_mime_type(file);
        if (mime_type)
        {
            mset->mime_type = ztd::strdup(vfs_mime_type_get_type(mime_type));
            type = fmt::format(" {} ( {} )",
                               vfs_mime_type_get_description(mime_type),
                               mset->mime_type);
            vfs_mime_type_unref(mime_type);
        }
        else
        {
            mset->mime_type = ztd::strdup("?");
            type = mset->mime_type;
        }
    }
    else // create
    {
        mset->mime_type = ztd::strdup("?");
        type = ztd::strdup(mset->mime_type);
    }
    mset->label_mime = GTK_LABEL(gtk_label_new(type.c_str()));
    gtk_label_set_ellipsize(mset->label_mime, PANGO_ELLIPSIZE_MIDDLE);

    gtk_label_set_selectable(mset->label_mime, true);
    gtk_widget_set_halign(GTK_WIDGET(mset->label_mime), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_mime), GTK_ALIGN_START);

    gtk_label_set_selectable(mset->label_type, true);
    g_signal_connect(G_OBJECT(mset->label_type),
                     "button-press-event",
                     G_CALLBACK(on_label_button_press),
                     mset);
    g_signal_connect(G_OBJECT(mset->label_type), "focus", G_CALLBACK(on_label_focus), mset);

    // Target
    if (mset->is_link || create_new)
    {
        mset->label_target = GTK_LABEL(gtk_label_new(nullptr));
        gtk_label_set_markup_with_mnemonic(mset->label_target, "<b>_Target:</b>");
        gtk_widget_set_halign(GTK_WIDGET(mset->label_target), GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(mset->label_target), GTK_ALIGN_END);
        mset->entry_target = GTK_ENTRY(gtk_entry_new());
        gtk_label_set_mnemonic_widget(mset->label_target, GTK_WIDGET(mset->entry_target));
        g_signal_connect(G_OBJECT(mset->entry_target),
                         "mnemonic-activate",
                         G_CALLBACK(on_mnemonic_activate),
                         mset);
        gtk_label_set_selectable(mset->label_target, true);
        g_signal_connect(G_OBJECT(mset->label_target),
                         "button-press-event",
                         G_CALLBACK(on_label_button_press),
                         mset);
        g_signal_connect(G_OBJECT(mset->label_target), "focus", G_CALLBACK(on_label_focus), mset);
        g_signal_connect(G_OBJECT(mset->entry_target),
                         "key-press-event",
                         G_CALLBACK(on_move_entry_keypress),
                         mset);

        if (create_new)
        {
            // Target Browse button
            mset->browse_target = gtk_button_new();
            gtk_widget_set_focus_on_click(GTK_WIDGET(mset->browse_target), false);
            if (mset->new_path && file)
                gtk_entry_set_text(GTK_ENTRY(mset->entry_target), mset->new_path);
            g_signal_connect(G_OBJECT(mset->browse_target),
                             "clicked",
                             G_CALLBACK(on_create_browse_button_press),
                             mset);
        }
        else
        {
            gtk_entry_set_text(GTK_ENTRY(mset->entry_target), mset->mime_type);
            gtk_editable_set_editable(GTK_EDITABLE(mset->entry_target), false);
            mset->browse_target = nullptr;
        }
        g_signal_connect(G_OBJECT(mset->entry_target), "changed", G_CALLBACK(on_move_change), mset);
    }
    else
        mset->label_target = nullptr;

    // Template
    if (create_new)
    {
        mset->label_template = GTK_LABEL(gtk_label_new(nullptr));
        gtk_label_set_markup_with_mnemonic(mset->label_template, "<b>_Template:</b>");
        gtk_widget_set_halign(GTK_WIDGET(mset->label_template), GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(mset->label_template), GTK_ALIGN_END);
        g_signal_connect(G_OBJECT(mset->entry_target),
                         "mnemonic-activate",
                         G_CALLBACK(on_mnemonic_activate),
                         mset);
        gtk_label_set_selectable(mset->label_template, true);
        g_signal_connect(G_OBJECT(mset->label_template),
                         "button-press-event",
                         G_CALLBACK(on_label_button_press),
                         mset);
        g_signal_connect(G_OBJECT(mset->label_template), "focus", G_CALLBACK(on_label_focus), mset);

        // template combo
        mset->combo_template = GTK_COMBO_BOX(gtk_combo_box_text_new_with_entry());
        gtk_widget_set_focus_on_click(GTK_WIDGET(mset->combo_template), false);
        // add entries
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template), "Empty File");
        templates = nullptr;
        templates = get_templates(nullptr, nullptr, templates, false);
        if (templates)
        {
            templates = g_list_sort(templates, (GCompareFunc)g_strcmp0);
            GList* l;
            int x = 0;
            for (l = templates; l && x++ < 500; l = l->next)
            {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template),
                                               (char*)l->data);
                free(l->data);
            }
            g_list_free(templates);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(mset->combo_template), 0);
        g_signal_connect(G_OBJECT(mset->combo_template),
                         "changed",
                         G_CALLBACK(on_template_changed),
                         mset);
        g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN(mset->combo_template))),
                         "key-press-event",
                         G_CALLBACK(on_move_entry_keypress),
                         mset);

        // template_dir combo
        mset->combo_template_dir = GTK_COMBO_BOX(gtk_combo_box_text_new_with_entry());
        gtk_widget_set_focus_on_click(GTK_WIDGET(mset->combo_template_dir), false);

        // add entries
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template_dir),
                                       "Empty Directory");
        templates = nullptr;
        templates = get_templates(nullptr, nullptr, templates, true);
        if (templates)
        {
            templates = g_list_sort(templates, (GCompareFunc)g_strcmp0);
            GList* l;
            for (l = templates; l; l = l->next)
            {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template_dir),
                                               (char*)l->data);
                free(l->data);
            }
            g_list_free(templates);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(mset->combo_template_dir), 0);
        g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN(mset->combo_template_dir))),
                         "key-press-event",
                         G_CALLBACK(on_move_entry_keypress),
                         mset);

        // Template Browse button
        mset->browse_template = gtk_button_new();
        gtk_widget_set_focus_on_click(GTK_WIDGET(mset->browse_template), false);
        g_signal_connect(G_OBJECT(mset->browse_template),
                         "clicked",
                         G_CALLBACK(on_create_browse_button_press),
                         mset);
    }
    else
    {
        mset->label_template = nullptr;
        mset->combo_template = nullptr;
        mset->combo_template_dir = nullptr;
    }

    // Name
    mset->label_name = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_name, "<b>_Name:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_name), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_name), GTK_ALIGN_START);
    mset->scroll_name = gtk_scrolled_window_new(nullptr, nullptr);
    mset->input_name = GTK_WIDGET(multi_input_new(GTK_SCROLLED_WINDOW(mset->scroll_name), nullptr));
    gtk_label_set_mnemonic_widget(mset->label_name, mset->input_name);
    g_signal_connect(G_OBJECT(mset->input_name),
                     "key-press-event",
                     G_CALLBACK(on_move_keypress),
                     mset);
    g_signal_connect(G_OBJECT(mset->input_name),
                     "mnemonic-activate",
                     G_CALLBACK(on_mnemonic_activate),
                     mset);
    gtk_label_set_selectable(mset->label_name, true);
    g_signal_connect(G_OBJECT(mset->label_name),
                     "button-press-event",
                     G_CALLBACK(on_label_button_press),
                     mset);
    g_signal_connect(G_OBJECT(mset->label_name), "focus", G_CALLBACK(on_label_focus), mset);
    mset->buf_name = GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_name)));
    g_signal_connect(G_OBJECT(mset->buf_name), "changed", G_CALLBACK(on_move_change), mset);
    g_signal_connect(G_OBJECT(mset->input_name), "focus", G_CALLBACK(on_focus), mset);
    mset->blank_name = GTK_LABEL(gtk_label_new(nullptr));

    // Ext
    mset->label_ext = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_ext, "<b>E_xtension:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_ext), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_ext), GTK_ALIGN_END);
    mset->entry_ext = GTK_ENTRY(gtk_entry_new());
    gtk_label_set_mnemonic_widget(mset->label_ext, GTK_WIDGET(mset->entry_ext));
    g_signal_connect(G_OBJECT(mset->entry_ext),
                     "mnemonic-activate",
                     G_CALLBACK(on_mnemonic_activate),
                     mset);
    gtk_label_set_selectable(mset->label_ext, true);
    g_signal_connect(G_OBJECT(mset->label_ext),
                     "button-press-event",
                     G_CALLBACK(on_label_button_press),
                     mset);
    g_signal_connect(G_OBJECT(mset->label_ext), "focus", G_CALLBACK(on_label_focus), mset);
    g_signal_connect(G_OBJECT(mset->entry_ext),
                     "key-press-event",
                     G_CALLBACK(on_move_entry_keypress),
                     mset);
    g_signal_connect(G_OBJECT(mset->entry_ext), "changed", G_CALLBACK(on_move_change), mset);
    g_signal_connect_after(G_OBJECT(mset->entry_ext), "focus", G_CALLBACK(on_focus), mset);
    gtk_widget_set_sensitive(GTK_WIDGET(mset->entry_ext), !mset->is_dir);
    gtk_widget_set_sensitive(GTK_WIDGET(mset->label_ext), !mset->is_dir);

    // Filename
    mset->label_full_name = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_full_name, "<b>_Filename:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_full_name), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_full_name), GTK_ALIGN_START);
    mset->scroll_full_name = gtk_scrolled_window_new(nullptr, nullptr);
    mset->input_full_name =
        GTK_WIDGET(multi_input_new(GTK_SCROLLED_WINDOW(mset->scroll_full_name), nullptr));
    gtk_label_set_mnemonic_widget(mset->label_full_name, mset->input_full_name);
    g_signal_connect(G_OBJECT(mset->input_full_name),
                     "mnemonic-activate",
                     G_CALLBACK(on_mnemonic_activate),
                     mset);
    gtk_label_set_selectable(mset->label_full_name, true);
    g_signal_connect(G_OBJECT(mset->label_full_name),
                     "button-press-event",
                     G_CALLBACK(on_label_button_press),
                     mset);
    g_signal_connect(G_OBJECT(mset->label_full_name), "focus", G_CALLBACK(on_label_focus), mset);
    g_signal_connect(G_OBJECT(mset->input_full_name),
                     "key-press-event",
                     G_CALLBACK(on_move_keypress),
                     mset);
    mset->buf_full_name = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_full_name));
    g_signal_connect(G_OBJECT(mset->buf_full_name), "changed", G_CALLBACK(on_move_change), mset);
    g_signal_connect(G_OBJECT(mset->input_full_name), "focus", G_CALLBACK(on_focus), mset);
    mset->blank_full_name = GTK_LABEL(gtk_label_new(nullptr));

    // Parent
    mset->label_path = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_path, "<b>_Parent:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_path), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_path), GTK_ALIGN_START);
    mset->scroll_path = gtk_scrolled_window_new(nullptr, nullptr);
    mset->input_path = GTK_WIDGET(multi_input_new(GTK_SCROLLED_WINDOW(mset->scroll_path), nullptr));
    gtk_label_set_mnemonic_widget(mset->label_path, mset->input_path);
    g_signal_connect(G_OBJECT(mset->input_path),
                     "mnemonic-activate",
                     G_CALLBACK(on_mnemonic_activate),
                     mset);
    gtk_label_set_selectable(mset->label_path, true);
    g_signal_connect(G_OBJECT(mset->label_path),
                     "button-press-event",
                     G_CALLBACK(on_label_button_press),
                     mset);
    g_signal_connect(G_OBJECT(mset->label_path), "focus", G_CALLBACK(on_label_focus), mset);
    g_signal_connect(G_OBJECT(mset->input_path),
                     "key-press-event",
                     G_CALLBACK(on_move_keypress),
                     mset);
    mset->buf_path = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_path));
    g_signal_connect(G_OBJECT(mset->buf_path), "changed", G_CALLBACK(on_move_change), mset);
    g_signal_connect(G_OBJECT(mset->input_path), "focus", G_CALLBACK(on_focus), mset);
    mset->blank_path = GTK_LABEL(gtk_label_new(nullptr));

    // Path
    mset->label_full_path = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_full_path, "<b>P_ath:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_full_path), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_full_path), GTK_ALIGN_START);
    mset->scroll_full_path = gtk_scrolled_window_new(nullptr, nullptr);
    // set initial path
    mset->input_full_path =
        GTK_WIDGET(multi_input_new(GTK_SCROLLED_WINDOW(mset->scroll_full_path), mset->new_path));
    gtk_label_set_mnemonic_widget(mset->label_full_path, mset->input_full_path);
    g_signal_connect(G_OBJECT(mset->input_full_path),
                     "mnemonic-activate",
                     G_CALLBACK(on_mnemonic_activate),
                     mset);
    gtk_label_set_selectable(mset->label_full_path, true);
    g_signal_connect(G_OBJECT(mset->label_full_path),
                     "button-press-event",
                     G_CALLBACK(on_label_button_press),
                     mset);
    g_signal_connect(G_OBJECT(mset->label_full_path), "focus", G_CALLBACK(on_label_focus), mset);
    g_signal_connect(G_OBJECT(mset->input_full_path),
                     "key-press-event",
                     G_CALLBACK(on_move_keypress),
                     mset);
    mset->buf_full_path = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_full_path));
    g_signal_connect(G_OBJECT(mset->buf_full_path), "changed", G_CALLBACK(on_move_change), mset);
    g_signal_connect(G_OBJECT(mset->input_full_path), "focus", G_CALLBACK(on_focus), mset);

    // Options
    mset->opt_move = gtk_radio_button_new_with_mnemonic(nullptr, "Mov_e");
    mset->opt_copy =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_move), "Cop_y");
    mset->opt_link =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_move), "Lin_k");
    mset->opt_copy_target =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_move),
                                                       "Copy _Target");
    mset->opt_link_target =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_move),
                                                       "Link Tar_get");
    mset->opt_as_root = gtk_check_button_new_with_mnemonic("A_s Root");

    mset->opt_new_file = gtk_radio_button_new_with_mnemonic(nullptr, "Fil_e");
    mset->opt_new_folder =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_new_file),
                                                       "Dir_ectory");
    mset->opt_new_link =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_new_file),
                                                       "_Link");

    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_move), false);
    g_signal_connect(G_OBJECT(mset->opt_move), "focus", G_CALLBACK(on_button_focus), mset);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_copy), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_link), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_copy_target), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_link_target), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_as_root), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_new_file), false);
    g_signal_connect(G_OBJECT(mset->opt_new_file), "focus", G_CALLBACK(on_button_focus), mset);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_new_folder), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_new_link), false);
    gtk_widget_set_sensitive(mset->opt_copy_target, mset->is_link && !target_missing);
    gtk_widget_set_sensitive(mset->opt_link_target, mset->is_link);

    // Pack
    GtkWidget* dlg_vbox = gtk_dialog_get_content_area(GTK_DIALOG(mset->dlg));
    gtk_container_set_border_width(GTK_CONTAINER(mset->dlg), 10);

    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->label_name), false, true, 4);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->scroll_name), true, true, 0);

    mset->hbox_ext = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(mset->hbox_ext), GTK_WIDGET(mset->label_ext), false, true, 0);
    gtk_box_pack_start(GTK_BOX(mset->hbox_ext), GTK_WIDGET(gtk_label_new(" ")), false, true, 0);
    gtk_box_pack_start(GTK_BOX(mset->hbox_ext), GTK_WIDGET(mset->entry_ext), true, true, 0);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->hbox_ext), false, true, 5);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->blank_name), false, true, 0);

    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->label_full_name), false, true, 4);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->scroll_full_name), true, true, 0);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->blank_full_name), false, true, 0);

    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->label_path), false, true, 4);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->scroll_path), true, true, 0);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->blank_path), false, true, 0);

    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->label_full_path), false, true, 4);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->scroll_full_path), true, true, 0);

    mset->hbox_type = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(mset->hbox_type), GTK_WIDGET(mset->label_type), false, true, 0);
    gtk_box_pack_start(GTK_BOX(mset->hbox_type), GTK_WIDGET(mset->label_mime), true, true, 5);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->hbox_type), false, true, 5);

    mset->hbox_target = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    if (mset->label_target)
    {
        gtk_box_pack_start(GTK_BOX(mset->hbox_target),
                           GTK_WIDGET(mset->label_target),
                           false,
                           true,
                           0);
        if (!create_new)
            gtk_box_pack_start(GTK_BOX(mset->hbox_target),
                               GTK_WIDGET(gtk_label_new(" ")),
                               false,
                               true,
                               0);
        gtk_box_pack_start(GTK_BOX(mset->hbox_target),
                           GTK_WIDGET(mset->entry_target),
                           true,
                           true,
                           create_new ? 3 : 0);
        if (mset->browse_target)
            gtk_box_pack_start(GTK_BOX(mset->hbox_target),
                               GTK_WIDGET(mset->browse_target),
                               false,
                               true,
                               0);
        gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->hbox_target), false, true, 5);
    }

    mset->hbox_template = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    if (mset->label_template)
    {
        gtk_box_pack_start(GTK_BOX(mset->hbox_template),
                           GTK_WIDGET(mset->label_template),
                           false,
                           true,
                           0);
        gtk_box_pack_start(GTK_BOX(mset->hbox_template),
                           GTK_WIDGET(mset->combo_template),
                           true,
                           true,
                           3);
        gtk_box_pack_start(GTK_BOX(mset->hbox_template),
                           GTK_WIDGET(mset->combo_template_dir),
                           true,
                           true,
                           3);
        gtk_box_pack_start(GTK_BOX(mset->hbox_template),
                           GTK_WIDGET(mset->browse_template),
                           false,
                           true,
                           0);
        gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->hbox_template), false, true, 5);
    }

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    if (create_new)
    {
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(gtk_label_new("New")), false, true, 3);
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(mset->opt_new_file), false, true, 3);
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(mset->opt_new_folder), false, true, 3);
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(mset->opt_new_link), false, true, 3);
    }
    else
    {
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(mset->opt_move), false, true, 3);
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(mset->opt_copy), false, true, 3);
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(mset->opt_link), false, true, 3);
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(mset->opt_copy_target), false, true, 3);
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(mset->opt_link_target), false, true, 3);
    }
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(gtk_label_new("  ")), false, true, 3);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(mset->opt_as_root), false, true, 6);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(hbox), false, true, 10);

    // show
    gtk_widget_show_all(mset->dlg);
    on_toggled(nullptr, mset);
    if (mset->clip_copy)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_copy), true);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), false);
    }
    else if (create_new == PtkRenameMode::PTK_RENAME_NEW_DIR)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder), true);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_file), false);
    }
    else if (create_new == PtkRenameMode::PTK_RENAME_NEW_LINK)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_link), true);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_file), false);
    }

    // signals
    g_signal_connect(G_OBJECT(mset->opt_move), "toggled", G_CALLBACK(on_opt_toggled), mset);
    g_signal_connect(G_OBJECT(mset->opt_copy), "toggled", G_CALLBACK(on_opt_toggled), mset);
    g_signal_connect(G_OBJECT(mset->opt_link), "toggled", G_CALLBACK(on_opt_toggled), mset);
    g_signal_connect(G_OBJECT(mset->opt_copy_target), "toggled", G_CALLBACK(on_opt_toggled), mset);
    g_signal_connect(G_OBJECT(mset->opt_link_target), "toggled", G_CALLBACK(on_opt_toggled), mset);
    g_signal_connect(G_OBJECT(mset->opt_as_root), "toggled", G_CALLBACK(on_opt_toggled), mset);
    g_signal_connect(G_OBJECT(mset->opt_new_file), "toggled", G_CALLBACK(on_opt_toggled), mset);
    g_signal_connect(G_OBJECT(mset->opt_new_folder), "toggled", G_CALLBACK(on_opt_toggled), mset);
    g_signal_connect(G_OBJECT(mset->opt_new_link), "toggled", G_CALLBACK(on_opt_toggled), mset);

    // init
    on_move_change(GTK_WIDGET(mset->buf_full_path), mset);
    on_opt_toggled(nullptr, mset);

    if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_name)))
        mset->last_widget = mset->input_name;
    else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_name)))
        mset->last_widget = mset->input_full_name;
    else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_path)))
        mset->last_widget = mset->input_path;
    else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_path)))
        mset->last_widget = mset->input_full_path;

    // select last widget
    select_input(mset->last_widget, mset);
    gtk_widget_grab_focus(mset->last_widget);

    g_signal_connect(G_OBJECT(mset->options), "focus", G_CALLBACK(on_button_focus), mset);
    g_signal_connect(G_OBJECT(mset->next), "focus", G_CALLBACK(on_button_focus), mset);
    g_signal_connect(G_OBJECT(mset->cancel), "focus", G_CALLBACK(on_button_focus), mset);

    // run
    std::string task_name;
    std::string root_mkdir;
    std::string to_path;
    std::string from_path;
    int response;
    while ((response = gtk_dialog_run(GTK_DIALOG(mset->dlg))))
    {
        if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
        {
            GtkTextIter iter;
            GtkTextIter siter;
            gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
            gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
            full_path = gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);
            if (full_path[0] != '/')
            {
                // update full_path to absolute
                char* cwd = g_path_get_dirname(mset->full_path);
                char* old_path2 = full_path;
                full_path = g_build_filename(cwd, old_path2, nullptr);
                free(cwd);
                free(old_path2);
            }
            if (strchr(full_path, '\n'))
            {
                ptk_show_error(GTK_WINDOW(mset->dlg), "Error", "Path contains linefeeds");
                free(full_path);
                continue;
            }
            full_name = g_path_get_basename(full_path);
            path = g_path_get_dirname(full_path);
            old_path = g_path_get_dirname(mset->full_path);
            bool overwrite = false;

            if (response == GTK_RESPONSE_APPLY)
                ret = 2;

            if (!create_new && (mset->full_path_same || !strcmp(full_path, mset->full_path)))
            {
                // not changed, proceed to next file
                free(full_path);
                free(full_name);
                free(path);
                free(old_path);
                break;
            }

            // determine job
            // bool move = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move));
            bool copy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy));
            bool link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link));
            bool copy_target =
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy_target));
            bool link_target =
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link_target));
            bool as_root = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_as_root));
            bool new_file = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file));
            bool new_folder = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
            bool new_link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));

            if (as_root)
                root_msg = " As Root";
            else
                root_msg = "";

            if (!std::filesystem::exists(path))
            {
                // create parent directory
                if (xset_get_b("move_dlg_confirm_create"))
                {
                    if (xset_msg_dialog(mset->parent,
                                        GTK_MESSAGE_QUESTION,
                                        "Create Parent Directory",
                                        GTK_BUTTONS_YES_NO,
                                        "The parent directory does not exist.  Create it?") !=
                        GTK_RESPONSE_YES)
                    {
                        free(full_path);
                        free(full_name);
                        free(path);
                        free(old_path);
                        continue;
                    }
                }
                if (as_root)
                {
                    to_path = bash_quote(path);
                    root_mkdir = fmt::format("mkdir -p {} && ", to_path);
                }
                else
                {
                    std::filesystem::create_directories(path);
                    std::filesystem::permissions(path, std::filesystem::perms::owner_all);

                    if (std::filesystem::is_directory(path))
                    {
                        std::string errno_msg = Glib::strerror(errno);
                        std::string msg =
                            fmt::format("Error creating parent directory\n\n{}", errno_msg);
                        ptk_show_error(GTK_WINDOW(mset->dlg), "Mkdir Error", msg);

                        free(full_path);
                        free(full_name);
                        free(path);
                        free(old_path);
                        continue;
                    }
                    else
                        update_new_display(path);
                }
            }
            else if (lstat(full_path, &statbuf) == 0)
            {
                // overwrite
                if (std::filesystem::is_directory(full_path))
                {
                    // just in case
                    free(full_path);
                    free(full_name);
                    free(path);
                    free(old_path);
                    continue;
                }
                if (xset_msg_dialog(mset->parent,
                                    GTK_MESSAGE_WARNING,
                                    "Overwrite Existing File",
                                    GTK_BUTTONS_YES_NO,
                                    "OVERWRITE WARNING",
                                    "The file path exists.  Overwrite existing file?") !=
                    GTK_RESPONSE_YES)
                {
                    free(full_path);
                    free(full_name);
                    free(path);
                    free(old_path);
                    continue;
                }
                overwrite = true;
            }

            if (create_new && new_link)
            {
                // new link task
                task_name = fmt::format("Create Link{}", root_msg);
                PtkFileTask* ptask = ptk_file_exec_new(task_name, nullptr, mset->parent, task_view);

                str = ztd::strdup(gtk_entry_get_text(mset->entry_target));
                g_strstrip(str);
                while (Glib::str_has_suffix(str, "/") && str[1] != '\0')
                    str[g_utf8_strlen(str, -1) - 1] = '\0';
                from_path = bash_quote(str);
                free(str);
                to_path = bash_quote(full_path);

                if (overwrite)
                {
                    ptask->task->exec_command =
                        fmt::format("{}ln -sf {} {}", root_mkdir, from_path, to_path);
                }
                else
                {
                    ptask->task->exec_command =
                        fmt::format("{}ln -s {} {}", root_mkdir, from_path, to_path);
                }
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                ptask->task->exec_export = false;
                if (as_root)
                    ptask->task->exec_as_user = "root";
                if (auto_open)
                {
                    auto_open->path = ztd::strdup(full_path);
                    auto_open->open_file = (response == GTK_RESPONSE_APPLY);
                    ptask->complete_notify = auto_open->callback;
                    ptask->user_data = auto_open;
                }
                ptk_file_task_run(ptask);
                update_new_display(full_path);
            }
            else if (create_new && new_file)
            {
                // new file task
                if (gtk_widget_get_visible(
                        gtk_widget_get_parent(GTK_WIDGET(mset->combo_template))) &&
                    (str = gtk_combo_box_text_get_active_text(
                         GTK_COMBO_BOX_TEXT(mset->combo_template))))
                {
                    g_strstrip(str);
                    if (str[0] == '/')
                        from_path = bash_quote(str);
                    // else if (!g_strcmp0("Empty File", str) || str[0] == '\0')
                    //     from_path = "";
                    else
                    {
                        char* tdir = get_template_dir();
                        if (tdir)
                        {
                            from_path = g_build_filename(tdir, str, nullptr);
                            if (!std::filesystem::is_regular_file(from_path))
                            {
                                ptk_show_error(GTK_WINDOW(mset->dlg),
                                               "Template Missing",
                                               "The specified template does not exist");
                                free(str);
                                free(tdir);

                                free(full_path);
                                free(full_name);
                                free(path);
                                free(old_path);
                                continue;
                            }
                            free(tdir);
                            from_path = bash_quote(from_path);
                        }
                    }
                    free(str);
                }
                to_path = bash_quote(full_path);
                std::string over_cmd;
                if (overwrite)
                    over_cmd = fmt::format("rm -f {} && ", to_path);

                task_name = fmt::format("Create New File{}", root_msg);
                PtkFileTask* ptask = ptk_file_exec_new(task_name, nullptr, mset->parent, task_view);
                if (from_path.empty())
                    ptask->task->exec_command =
                        fmt::format("{}{}touch {}", root_mkdir, over_cmd, to_path);
                else
                    ptask->task->exec_command =
                        fmt::format("{}{}cp -f {} {}", root_mkdir, over_cmd, from_path, to_path);
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                ptask->task->exec_export = false;
                if (as_root)
                    ptask->task->exec_as_user = "root";
                if (auto_open)
                {
                    auto_open->path = ztd::strdup(full_path);
                    auto_open->open_file = (response == GTK_RESPONSE_APPLY);
                    ptask->complete_notify = auto_open->callback;
                    ptask->user_data = auto_open;
                }
                ptk_file_task_run(ptask);
                update_new_display(full_path);
            }
            else if (create_new)
            {
                // new directory task
                if (!new_folder)
                {
                    // failsafe
                    free(full_path);
                    free(full_name);
                    free(path);
                    free(old_path);
                    continue;
                }
                if (gtk_widget_get_visible(
                        gtk_widget_get_parent(GTK_WIDGET(mset->combo_template_dir))) &&
                    (str = gtk_combo_box_text_get_active_text(
                         GTK_COMBO_BOX_TEXT(mset->combo_template_dir))))
                {
                    g_strstrip(str);
                    if (str[0] == '/')
                        from_path = bash_quote(str);
                    // else if (!g_strcmp0("Empty Directory", str) || str[0] == '\0')
                    //     from_path = nullptr;
                    else
                    {
                        char* tdir = get_template_dir();
                        if (tdir)
                        {
                            from_path = g_build_filename(tdir, str, nullptr);
                            if (!std::filesystem::is_directory(from_path))
                            {
                                ptk_show_error(GTK_WINDOW(mset->dlg),
                                               "Template Missing",
                                               "The specified template does not exist");
                                free(str);
                                free(tdir);
                                free(full_path);
                                free(full_name);
                                free(path);
                                free(old_path);
                                continue;
                            }
                            free(tdir);
                            from_path = bash_quote(from_path);
                        }
                    }
                    free(str);
                }
                to_path = bash_quote(full_path);

                task_name = fmt::format("Create New Directory{}", root_msg);
                PtkFileTask* ptask = ptk_file_exec_new(task_name, nullptr, mset->parent, task_view);
                if (from_path.empty())
                    ptask->task->exec_command = fmt::format("{}mkdir {}", root_mkdir, to_path);
                else
                    ptask->task->exec_command =
                        fmt::format("{}cp -rL {} {}", root_mkdir, from_path, to_path);
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                ptask->task->exec_export = false;
                if (as_root)
                    ptask->task->exec_as_user = "root";
                if (auto_open)
                {
                    auto_open->path = ztd::strdup(full_path);
                    auto_open->open_file = (response == GTK_RESPONSE_APPLY);
                    ptask->complete_notify = auto_open->callback;
                    ptask->user_data = auto_open;
                }
                ptk_file_task_run(ptask);
                update_new_display(full_path);
            }
            else if (copy || copy_target)
            {
                // copy task
                task_name = fmt::format("Copy{}", root_msg);
                PtkFileTask* ptask = ptk_file_exec_new(task_name, nullptr, mset->parent, task_view);
                char* over_opt = nullptr;
                to_path = bash_quote(full_path);
                if (copy || !mset->is_link)
                    from_path = bash_quote(mset->full_path);
                else
                {
                    str = get_real_link_target(mset->full_path);
                    if (!str)
                    {
                        ptk_show_error(GTK_WINDOW(mset->dlg),
                                       "Copy Target Error",
                                       "Error determining link's target");
                        free(full_path);
                        free(full_name);
                        free(path);
                        free(old_path);
                        continue;
                    }
                    from_path = bash_quote(str);
                    free(str);
                }
                if (overwrite)
                    over_opt = ztd::strdup(" --remove-destination");
                if (!over_opt)
                    over_opt = ztd::strdup("");

                if (mset->is_dir)
                {
                    ptask->task->exec_command =
                        fmt::format("{}cp -Pfr {} {}", root_mkdir, from_path, to_path);
                }
                else
                {
                    ptask->task->exec_command =
                        fmt::format("{}cp -Pf{} {} {}", root_mkdir, over_opt, from_path, to_path);
                }
                free(over_opt);
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                ptask->task->exec_export = false;
                if (as_root)
                    ptask->task->exec_as_user = "root";
                ptk_file_task_run(ptask);
                update_new_display(full_path);
            }
            else if (link || link_target)
            {
                // link task
                task_name = fmt::format("Create Link{}", root_msg);
                PtkFileTask* ptask = ptk_file_exec_new(task_name, nullptr, mset->parent, task_view);
                if (link || !mset->is_link)
                    from_path = bash_quote(mset->full_path);
                else
                {
                    str = get_real_link_target(mset->full_path);
                    if (!str)
                    {
                        ptk_show_error(GTK_WINDOW(mset->dlg),
                                       "Link Target Error",
                                       "Error determining link's target");
                        free(full_path);
                        free(full_name);
                        free(path);
                        free(old_path);
                        continue;
                    }
                    from_path = bash_quote(str);
                    free(str);
                }
                to_path = bash_quote(full_path);
                if (overwrite)
                {
                    ptask->task->exec_command =
                        fmt::format("{}ln -sf {} {}", root_mkdir, from_path, to_path);
                }
                else
                {
                    ptask->task->exec_command =
                        fmt::format("{}ln -s {} {}", root_mkdir, from_path, to_path);
                }
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                ptask->task->exec_export = false;
                if (as_root)
                    ptask->task->exec_as_user = "root";
                ptk_file_task_run(ptask);
                update_new_display(full_path);
            }
            // need move?  (do move as task in case it takes a long time)
            else if (as_root || strcmp(old_path, path))
            {
            _move_task:
                // move task - this is jumped to from the below rename block on
                // EXDEV error
                task_name = fmt::format("Move{}", root_msg);
                PtkFileTask* ptask = ptk_file_exec_new(task_name, nullptr, mset->parent, task_view);
                from_path = bash_quote(mset->full_path);
                to_path = bash_quote(full_path);
                if (overwrite)
                {
                    ptask->task->exec_command =
                        fmt::format("{}mv -f {} {}", root_mkdir, from_path, to_path);
                }
                else
                {
                    ptask->task->exec_command =
                        fmt::format("{}mv {} {}", root_mkdir, from_path, to_path);
                }
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                ptask->task->exec_export = false;
                if (as_root)
                    ptask->task->exec_as_user = "root";
                ptk_file_task_run(ptask);
                update_new_display(full_path);
            }
            else
            {
                // rename (does overwrite)
                if (rename(mset->full_path, full_path) != 0)
                {
                    // Respond to an EXDEV error by switching to a move (e.g. aufs
                    // directory rename fails due to the directory existing in
                    // multiple underlying branches)
                    if (errno == EXDEV)
                        goto _move_task;

                    // Unknown error has occurred - alert user as usual
                    std::string errno_msg = Glib::strerror(errno);
                    std::string msg = fmt::format("Error renaming file\n\n{}", errno_msg);
                    ptk_show_error(GTK_WINDOW(mset->dlg), "Rename Error", msg);
                    free(full_path);
                    free(full_name);
                    free(path);
                    free(old_path);
                    continue;
                }
                else
                    update_new_display(full_path);
            }
            free(full_path);
            free(full_name);
            free(path);
            free(old_path);
            break;
        }
        else if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT)
        {
            ret = 0;
            break;
        }
    }
    if (response == 0)
        ret = 0;

    // save size
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(mset->dlg), &allocation);

    // destroy
    gtk_widget_destroy(mset->dlg);

    free(mset->full_path);
    if (mset->new_path)
        free(mset->new_path);
    if (mset->mime_type)
        free(mset->mime_type);
    g_slice_free(MoveSet, mset);

    return ret;
}

void
ptk_show_file_properties(GtkWindow* parent_win, const char* cwd, GList* sel_files, int page)
{
    GtkWidget* dlg;

    if (sel_files)
    {
        /* Make a copy of the list */
        sel_files = g_list_copy(sel_files);
        g_list_foreach(sel_files, (GFunc)vfs_file_info_ref, nullptr);

        dlg = file_properties_dlg_new(parent_win, cwd, sel_files, page);
    }
    else
    {
        // no files selected, use cwd as file
        VFSFileInfo* file = vfs_file_info_new();
        vfs_file_info_get(file, cwd, nullptr);
        sel_files = g_list_prepend(nullptr, vfs_file_info_ref(file));
        char* parent_dir = g_path_get_dirname(cwd);
        dlg = file_properties_dlg_new(parent_win, parent_dir, sel_files, page);
    }
    g_signal_connect_swapped(dlg, "destroy", G_CALLBACK(vfs_file_info_list_free), sel_files);
    gtk_widget_show(dlg);
}

static bool
open_archives_with_handler(ParentInfo* parent, GList* sel_files, char* full_path,
                           VFSMimeType* mime_type)
{
    if (xset_get_b("arc_def_open"))
        // user has open archives with app option enabled
        return false; // do not handle these files

    bool extract_here = xset_get_b("arc_def_ex");
    const char* dest_dir = nullptr;
    int cmd;

    // determine default archive action in this dir
    if (extract_here && have_rw_access(parent->cwd))
    {
        // Extract Here
        cmd = PtkHandlerArchive::HANDLER_EXTRACT;
        dest_dir = parent->cwd;
    }
    else if (extract_here || xset_get_b("arc_def_exto"))
    {
        // Extract Here but no write access or Extract To option
        cmd = PtkHandlerArchive::HANDLER_EXTRACT;
    }
    else if (xset_get_b("arc_def_list"))
    {
        // List contents
        cmd = PtkHandlerArchive::HANDLER_LIST;
    }
    else
        return false; // do not handle these files

    // type or pathname has archive handler? - do not test command non-empty
    // here because only applies to first file
    GSList* handlers_slist = ptk_handler_file_has_handlers(PtkHandlerMode::HANDLER_MODE_ARC,
                                                           cmd,
                                                           full_path,
                                                           mime_type,
                                                           false,
                                                           false,
                                                           true);
    if (handlers_slist)
    {
        g_slist_free(handlers_slist);
        ptk_file_archiver_extract(parent->file_browser,
                                  sel_files,
                                  parent->cwd,
                                  dest_dir,
                                  cmd,
                                  true);
        return true; // all files handled
    }
    return false; // do not handle these files
}

static void
open_files_with_handler(ParentInfo* parent, GList* files, XSet* handler_set)
{
    GList* l;
    std::string str;
    std::string command_final;
    std::string name;

    LOG_INFO("Selected File Handler '{}'", handler_set->menu_label);

    // get command - was already checked as non-empty
    std::string error_message;
    std::string command;
    bool error = ptk_handler_load_script(PtkHandlerMode::HANDLER_MODE_FILE,
                                         PtkHandlerMount::HANDLER_MOUNT,
                                         handler_set,
                                         nullptr,
                                         command,
                                         error_message);
    if (error)
    {
        xset_msg_dialog(parent->file_browser ? GTK_WIDGET(parent->file_browser) : nullptr,
                        GTK_MESSAGE_ERROR,
                        "Error Loading Handler",
                        GTK_BUTTONS_OK,
                        error_message);
        return;
    }
    // auto mount point
    if (ztd::contains(command, "%a"))
    {
        name = ptk_location_view_create_mount_point(PtkHandlerMode::HANDLER_MODE_FILE,
                                                    nullptr,
                                                    nullptr,
                                                    files && files->data ? (char*)files->data
                                                                         : nullptr);
        command = ztd::replace(command, "%a", name);
    }

    /* prepare bash vars for just the files being opened by this handler,
     * not necessarily all selected */
    std::string fm_filenames = "fm_filenames=(\n";
    std::string fm_files = "fm_files=(\n";
    // command looks like it handles multiple files ?
    std::array<std::string, 4> keys{"%N", "%F", "fm_files[", "fm_filenames["};
    bool multiple = ztd::contains(command, keys);
    if (multiple)
    {
        for (l = files; l; l = l->next)
        {
            // filename
            std::string quoted;

            name = g_path_get_basename((char*)l->data);
            quoted = bash_quote(name);
            fm_filenames.append(fmt::format("{}\n", quoted));
            // file path
            quoted = bash_quote((char*)l->data);
            fm_filenames.append(fmt::format("{}\n", quoted));
        }
    }
    fm_filenames.append(")\nfm_filename=\"$fm_filenames[0]\"\n");
    fm_files.append(")\nfm_file=\"$fm_files[0]\"\n");
    // replace standard sub vars
    command = replace_line_subs(command);

    // start task(s)
    for (l = files; l; l = l->next)
    {
        if (multiple)
            command_final = fmt::format("{}{}{}", fm_filenames, fm_files, command);
        else
        {
            // add sub vars for single file
            // filename
            std::string quoted;

            name = g_path_get_basename((char*)l->data);
            quoted = bash_quote(name);
            str = fmt::format("fm_filename={}\n", quoted);
            // file path
            quoted = bash_quote((char*)l->data);
            command_final =
                fmt::format("{}{}{}fm_file={}\n{}", fm_filenames, fm_files, str, quoted, command);
        }

        // Run task
        PtkFileTask* ptask =
            ptk_file_exec_new(handler_set->menu_label,
                              parent->cwd,
                              parent->file_browser ? GTK_WIDGET(parent->file_browser) : nullptr,
                              parent->file_browser ? parent->file_browser->task_view : nullptr);
        // do not free cwd!
        ptask->task->exec_browser = parent->file_browser;
        ptask->task->exec_command = command_final;
        if (handler_set->icon)
            ptask->task->exec_icon = handler_set->icon;
        ptask->task->exec_terminal = handler_set->in_terminal;
        ptask->task->exec_keep_terminal = false;
        // file handlers store Run As Task in keep_terminal
        ptask->task->exec_sync = handler_set->keep_terminal;
        ptask->task->exec_show_error = ptask->task->exec_sync;
        ptask->task->exec_export = true;
        ptk_file_task_run(ptask);

        if (multiple)
            break;
    }
}

static const char*
check_desktop_name(const char* app_desktop)
{
    // Check whether this is an app desktop file or just a command line
    if (Glib::str_has_suffix(app_desktop, ".desktop"))
        return app_desktop;

    // Not a desktop entry name
    // If we are lucky enough, there might be a desktop entry
    // for this program
    std::string name = fmt::format("{}.desktop", app_desktop);
    if (std::filesystem::exists(name))
        return ztd::strdup(name);

    // fallback
    return app_desktop;
}

static bool
open_files_with_app(ParentInfo* parent, GList* files, const char* app_desktop)
{
    XSet* handler_set;

    if (app_desktop && Glib::str_has_prefix(app_desktop, "###") &&
        (handler_set = xset_is(app_desktop + 3)) && files)
    {
        // is a handler
        open_files_with_handler(parent, files, handler_set);
        return true;
    }
    else if (app_desktop)
    {
        VFSAppDesktop desktop(check_desktop_name(app_desktop));

        LOG_INFO("EXEC({})={}", desktop.get_full_path(), desktop.get_exec());

        //////////
        std::vector<std::string> open_files;
        GList* l;
        for (l = files; l; l = l->next)
        {
            std::string open_file = (char*)(l->data);
            open_files.push_back(open_file);
        }
        //////////

        try
        {
            desktop.open_files(parent->cwd, open_files);
        }
        catch (const VFSAppDesktopException& e)
        {
            GtkWidget* toplevel = parent->file_browser
                                      ? gtk_widget_get_toplevel(GTK_WIDGET(parent->file_browser))
                                      : nullptr;
            ptk_show_error(GTK_WINDOW(toplevel), "Error", e.what());
        }
    }
    return true;
}

static void
open_files_with_each_app(void* key, void* value, void* user_data)
{
    char* app_desktop = (char*)key; // is const unless handler
    GList* files = (GList*)value;
    ParentInfo* parent = static_cast<ParentInfo*>(user_data);
    open_files_with_app(parent, files, app_desktop);
}

static void
free_file_list_hash(void* key, void* value, void* user_data)
{
    (void)key;
    (void)user_data;
    GList* files = (GList*)value;
    g_list_foreach(files, (GFunc)free, nullptr);
    g_list_free(files);
}

void
ptk_open_files_with_app(const char* cwd, GList* sel_files, const char* app_desktop,
                        PtkFileBrowser* file_browser, bool xforce, bool xnever)
{
    // if xnever, never execute an executable
    // if xforce, force execute of executable ignoring app_settings.no_execute

    char* full_path = nullptr;
    GList* files_to_open = nullptr;
    GHashTable* file_list_hash = nullptr;
    char* new_dir = nullptr;
    GtkWidget* toplevel;

    ParentInfo* parent = g_slice_new0(ParentInfo);
    parent->file_browser = file_browser;
    parent->cwd = cwd;

    GList* l;
    for (l = sel_files; l; l = l->next)
    {
        VFSFileInfo* file = static_cast<VFSFileInfo*>(l->data);
        if (!file)
            continue;

        full_path = g_build_filename(cwd, vfs_file_info_get_name(file), nullptr);
        if (full_path)
        {
            if (app_desktop)
                // specified app to open all files
                files_to_open = g_list_append(files_to_open, full_path);
            else
            {
                // No app specified - Use default app for each file

                // Is a dir?  Open in browser
                if (file_browser && std::filesystem::is_directory(full_path))
                {
                    if (!new_dir)
                        new_dir = full_path;
                    else
                    {
                        if (file_browser)
                        {
                            ptk_file_browser_emit_open(file_browser,
                                                       full_path,
                                                       PtkOpenAction::PTK_OPEN_NEW_TAB);
                        }
                    }
                    continue;
                }

                /* If this file is an executable file, run it. */
                if (!xnever && vfs_file_info_is_executable(file, full_path) &&
                    (!app_settings.no_execute || xforce))
                {
                    Glib::spawn_command_line_async(full_path);
                    if (file_browser)
                        ptk_file_browser_emit_open(file_browser,
                                                   full_path,
                                                   PtkOpenAction::PTK_OPEN_FILE);
                    free(full_path);
                    continue;
                }

                /* Find app to open this file and place copy in alloc_desktop.
                 * This string is freed when hash table is destroyed. */
                char* alloc_desktop = nullptr;

                VFSMimeType* mime_type = vfs_file_info_get_mime_type(file);

                // has archive handler?
                if (l == sel_files /* test first file only */ &&
                    open_archives_with_handler(parent, sel_files, full_path, mime_type))
                {
                    // all files were handled by open_archives_with_handler
                    vfs_mime_type_unref(mime_type);
                    break;
                }

                // if has file handler, set alloc_desktop = ###XSETNAME
                GSList* handlers_slist =
                    ptk_handler_file_has_handlers(PtkHandlerMode::HANDLER_MODE_FILE,
                                                  PtkHandlerMount::HANDLER_MOUNT,
                                                  full_path,
                                                  mime_type,
                                                  true,
                                                  false,
                                                  true);
                if (handlers_slist)
                {
                    XSet* handler_set = XSET(handlers_slist->data);
                    g_slist_free(handlers_slist);
                    alloc_desktop = g_strconcat("###", handler_set->name, nullptr);
                }

                /* The file itself is a desktop entry file. */
                /* was: if( Glib::str_has_suffix( vfs_file_info_get_name( file ), ".desktop" ) )
                 */
                if (!alloc_desktop)
                {
                    if (file->flags & VFSFileInfoFlag::VFS_FILE_INFO_DESKTOP_ENTRY &&
                        (!app_settings.no_execute || xforce))
                        alloc_desktop = full_path;
                    else
                        alloc_desktop = vfs_mime_type_get_default_action(mime_type);
                }

                if (!alloc_desktop && mime_type_is_text_file(full_path, mime_type->type))
                {
                    /* FIXME: special handling for plain text file */
                    vfs_mime_type_unref(mime_type);
                    mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_PLAIN_TEXT);
                    alloc_desktop = vfs_mime_type_get_default_action(mime_type);
                }

                vfs_mime_type_unref(mime_type);

                if (!alloc_desktop && vfs_file_info_is_symlink(file))
                {
                    // broken link?
                    char* target_path = g_file_read_link(full_path, nullptr);
                    if (target_path)
                    {
                        if (!std::filesystem::exists(target_path))
                        {
                            std::string msg = fmt::format("This symlink's target is missing or "
                                                          "you do not have permission "
                                                          "to access it:\n{}\n\nTarget: {}",
                                                          full_path,
                                                          target_path);
                            toplevel = file_browser
                                           ? gtk_widget_get_toplevel(GTK_WIDGET(file_browser))
                                           : nullptr;
                            ptk_show_error(GTK_WINDOW(toplevel), "Broken Link", msg.c_str());
                            free(full_path);
                            free(target_path);
                            continue;
                        }
                        free(target_path);
                    }
                }
                if (!alloc_desktop)
                {
                    /* Let the user choose an application */
                    toplevel =
                        file_browser ? gtk_widget_get_toplevel(GTK_WIDGET(file_browser)) : nullptr;
                    alloc_desktop = ptk_choose_app_for_mime_type(GTK_WINDOW(toplevel),
                                                                 mime_type,
                                                                 true,
                                                                 true,
                                                                 true,
                                                                 !file_browser);
                }
                if (!alloc_desktop)
                {
                    free(full_path);
                    continue;
                }

                // add full_path to list, update hash table
                files_to_open = nullptr;
                if (!file_list_hash)
                    /* this will free the keys (alloc_desktop) when hash table
                     * destroyed or new key inserted/replaced */
                    file_list_hash = g_hash_table_new_full(g_str_hash, g_str_equal, free, nullptr);
                else
                    // get existing file list for this app
                    files_to_open = (GList*)g_hash_table_lookup(file_list_hash, alloc_desktop);

                if (alloc_desktop != full_path)
                    /* it is not a desktop file itself - add file to list.
                     * Otherwise use full_path as hash table key, which will
                     * be freed when hash table is destroyed. */
                    files_to_open = g_list_append(files_to_open, full_path);
                // update file list in hash table
                g_hash_table_replace(file_list_hash, alloc_desktop, files_to_open);
            }
        }
    }

    if (app_desktop && files_to_open)
    {
        // specified app to open all files
        open_files_with_app(parent, files_to_open, app_desktop);
        g_list_foreach(files_to_open, (GFunc)free, nullptr);
        g_list_free(files_to_open);
    }
    else if (file_list_hash)
    {
        // No app specified - Use default app to open each associated list of files
        // free_file_list_hash frees each file list and its strings
        g_hash_table_foreach(file_list_hash, open_files_with_each_app, parent);
        g_hash_table_foreach(file_list_hash, free_file_list_hash, nullptr);
        g_hash_table_destroy(file_list_hash);
    }

    if (new_dir)
    {
        if (file_browser)
            ptk_file_browser_emit_open(file_browser, full_path, PtkOpenAction::PTK_OPEN_DIR);
        free(new_dir);
    }
    g_slice_free(ParentInfo, parent);
}

void
ptk_file_misc_paste_as(PtkFileBrowser* file_browser, const char* cwd, GFunc callback)
{
    (void)callback;
    char* file_path;
    bool is_cut = false;
    int missing_targets;
    VFSFileInfo* file;
    char* file_dir;

    GList* files = ptk_clipboard_get_file_paths(cwd, &is_cut, &missing_targets);

    GList* l;

    for (l = files; l; l = l->next)
    {
        file_path = (char*)l->data;
        file = vfs_file_info_new();
        vfs_file_info_get(file, file_path, nullptr);
        file_dir = g_path_get_dirname(file_path);
        if (!ptk_rename_file(file_browser,
                             file_dir,
                             file,
                             cwd,
                             !is_cut,
                             PtkRenameMode::PTK_RENAME,
                             nullptr))
        {
            vfs_file_info_unref(file);
            free(file_dir);
            missing_targets = 0;
            break;
        }
        vfs_file_info_unref(file);
        free(file_dir);
    }
    g_list_foreach(files, (GFunc)free, nullptr);
    g_list_free(files);

    if (missing_targets > 0)
    {
        GtkWindow* parent = nullptr;
        if (file_browser)
            parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser)));

        std::string msg = fmt::format("{} target{} missing",
                                      missing_targets,
                                      missing_targets > 1 ? "s are" : " is");
        ptk_show_error(parent, "Error", msg);
    }
}

void
ptk_file_misc_rootcmd(PtkFileBrowser* file_browser, GList* sel_files, char* cwd, char* setname)
{
    /*
     * root_copy_loc    copy to location
     * root_move2       move to
     * root_delete      delete
     */
    if (!setname || !file_browser || !sel_files)
        return;
    XSet* set;
    char* path;
    std::string cmd;
    std::string task_name;

    GtkWidget* parent = GTK_WIDGET(file_browser);
    std::string file_paths;
    GList* sel;
    std::string file_path_q;
    int item_count = 0;
    for (sel = sel_files; sel; sel = sel->next)
    {
        std::string file_path;
        file_path =
            Glib::build_filename(cwd, vfs_file_info_get_name(static_cast<VFSFileInfo*>(sel->data)));
        file_path_q = bash_quote(file_path);
        file_paths = fmt::format("{} {}", file_paths, file_path_q);
        item_count++;
    }

    if (!strcmp(setname, "root_delete"))
    {
        if (!app_settings.no_confirm)
        {
            std::string msg = fmt::format("Delete {} selected item as root ?", item_count);
            if (xset_msg_dialog(parent,
                                GTK_MESSAGE_WARNING,
                                "Confirm Delete As Root",
                                GTK_BUTTONS_YES_NO,
                                "DELETE AS ROOT",
                                msg) != GTK_RESPONSE_YES)
            {
                return;
            }
        }
        cmd = fmt::format("rm -r {}", file_paths);
        task_name = "Delete As Root";
    }
    else
    {
        char* folder;
        set = xset_get(setname);
        if (set->desc)
            folder = set->desc;
        else
            folder = cwd;
        path = xset_file_dialog(parent,
                                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                "Choose Location",
                                folder,
                                nullptr);
        if (path && std::filesystem::is_directory(path))
        {
            xset_set_set(set, XSetSetSet::XSET_SET_SET_DESC, path);
            std::string quote_path = bash_quote(path);

            if (!strcmp(setname, "root_move2"))
            {
                task_name = "Move As Root";
                // problem: no warning if already exists
                cmd = fmt::format("mv -f {} {}", file_paths, quote_path.c_str());
            }
            else
            {
                task_name = "Copy As Root";
                // problem: no warning if already exists
                cmd = fmt::format("cp -r {} {}", file_paths, quote_path.c_str());
            }

            free(path);
        }
        else
        {
            return;
        }
    }

    // root task
    PtkFileTask* ptask =
        ptk_file_exec_new(task_name, cwd, parent, file_browser ? file_browser->task_view : nullptr);
    ptask->task->exec_command = cmd;
    ptask->task->exec_sync = true;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_export = false;
    ptask->task->exec_as_user = "root";
    ptk_file_task_run(ptask);
}
