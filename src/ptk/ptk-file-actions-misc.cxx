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

#include <vector>

#include <fmt/format.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings/app.hxx"

#include "ptk/ptk-file-task.hxx"

#include "ptk/ptk-file-actions-misc.hxx"

static bool
action_dialog(GtkWindow* parent_win, std::string_view title, std::string_view message)
{
    GtkWidget* dlg = gtk_message_dialog_new(parent_win,
                                            GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_WARNING,
                                            GTK_BUTTONS_YES_NO,
                                            message.data(),
                                            nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_YES);
    gtk_window_set_title(GTK_WINDOW(dlg), title.data());
    // xset_set_window_icon(GTK_WINDOW(dlg));

    const int ret = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (ret != GTK_RESPONSE_YES)
        return false;
    return true;
}

void
ptk_delete_files(GtkWindow* parent_win, std::string_view cwd,
                 const std::vector<VFSFileInfo*>& sel_files, GtkTreeView* task_view)
{
    if (sel_files.empty())
        return;

    if (app_settings.get_confirm_delete())
    {
        const std::string msg = fmt::format("Delete {} selected item ?", sel_files.size());
        const bool ret = action_dialog(parent_win, "Confirm Delete", msg);
        if (!ret)
            return;
    }

    std::vector<std::string> file_list;
    for (VFSFileInfo* file: sel_files)
    {
        const std::string file_path =
            Glib::build_filename(cwd.data(), vfs_file_info_get_name(file));
        file_list.push_back(file_path);
    }

    PtkFileTask* ptask = new PtkFileTask(VFSFileTaskType::VFS_FILE_TASK_DELETE,
                                         file_list,
                                         nullptr,
                                         parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                         GTK_WIDGET(task_view));
    ptk_file_task_run(ptask);
}

void
ptk_trash_files(GtkWindow* parent_win, std::string_view cwd,
                const std::vector<VFSFileInfo*>& sel_files, GtkTreeView* task_view)
{
    if (sel_files.empty())
        return;

    if (app_settings.get_confirm_trash())
    {
        const std::string msg = fmt::format("Trash {} selected item ?", sel_files.size());
        const bool ret = action_dialog(parent_win, "Confirm Trash", msg);
        if (!ret)
            return;
    }

    std::vector<std::string> file_list;
    for (VFSFileInfo* file: sel_files)
    {
        const std::string file_path =
            Glib::build_filename(cwd.data(), vfs_file_info_get_name(file));
        file_list.push_back(file_path);
    }

    PtkFileTask* ptask = new PtkFileTask(VFSFileTaskType::VFS_FILE_TASK_TRASH,
                                         file_list,
                                         nullptr,
                                         parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                         GTK_WIDGET(task_view));
    ptk_file_task_run(ptask);
}
