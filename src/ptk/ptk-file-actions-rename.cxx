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

#include <array>
#include <vector>

#include <algorithm>
#include <ranges>

#include <fmt/format.h>

#include <glib.h>
#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-error.hxx"
#include "ptk/ptk-keyboard.hxx"

#include "ptk/ptk-utils.hxx"

#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-clipboard.hxx"
#include "ptk/ptk-utils.hxx"

#include "vfs/vfs-file-info.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "settings/app.hxx"

#include "utils.hxx"

#include "ptk/ptk-file-actions-rename.hxx"

struct MoveSet
{
    MoveSet();
    ~MoveSet();

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

MoveSet::MoveSet()
{
    this->full_path = nullptr;
    this->old_path = nullptr;
    this->new_path = nullptr;
    this->desc = nullptr;
    this->is_dir = false;
    this->is_link = false;
    this->clip_copy = false;
    // this->create_new;

    this->dlg = nullptr;
    this->parent = nullptr;
    this->browser = nullptr;

    this->label_type = nullptr;
    this->label_mime = nullptr;
    this->hbox_type = nullptr;
    this->mime_type = nullptr;

    this->label_target = nullptr;
    this->entry_target = nullptr;
    this->hbox_target = nullptr;
    this->browse_target = nullptr;

    this->label_template = nullptr;
    this->combo_template = nullptr;
    this->combo_template_dir = nullptr;
    this->hbox_template = nullptr;
    this->browse_template = nullptr;

    this->label_name = nullptr;
    this->scroll_name = nullptr;
    this->input_name = nullptr;
    this->buf_name = nullptr;
    this->blank_name = nullptr;

    this->hbox_ext = nullptr;
    this->label_ext = nullptr;
    this->entry_ext = nullptr;

    this->label_full_name = nullptr;
    this->scroll_full_name = nullptr;
    this->input_full_name = nullptr;
    this->buf_full_name = nullptr;
    this->blank_full_name = nullptr;

    this->label_path = nullptr;
    this->scroll_path = nullptr;
    this->input_path = nullptr;
    this->buf_path = nullptr;
    this->blank_path = nullptr;

    this->label_full_path = nullptr;
    this->scroll_full_path = nullptr;
    this->input_full_path = nullptr;
    this->buf_full_path = nullptr;

    this->opt_move = nullptr;
    this->opt_copy = nullptr;
    this->opt_link = nullptr;
    this->opt_copy_target = nullptr;
    this->opt_link_target = nullptr;
    this->opt_as_root = nullptr;

    this->opt_new_file = nullptr;
    this->opt_new_folder = nullptr;
    this->opt_new_link = nullptr;

    this->options = nullptr;
    this->browse = nullptr;
    this->revert = nullptr;
    this->cancel = nullptr;
    this->next = nullptr;
    this->open = nullptr;

    this->last_widget = nullptr;

    this->full_path_exists = false;
    this->full_path_exists_dir = false;
    this->full_path_same = false;
    this->path_missing = false;
    this->path_exists_file = false;
    this->mode_change = false;
    this->is_move = false;
}

MoveSet::~MoveSet()
{
    if (this->full_path)
        free(this->full_path);
    if (this->new_path)
        free(this->new_path);
    if (this->desc)
        free(this->desc);
    if (this->mime_type)
        free(this->mime_type);
}

AutoOpenCreate::AutoOpenCreate()
{
    this->path = nullptr;
    this->file_browser = nullptr;
    this->callback = nullptr;
    this->open_file = false;
}

AutoOpenCreate::~AutoOpenCreate()
{
    if (this->path)
        free(this->path);
}

static void on_toggled(GtkMenuItem* item, MoveSet* mset);
static const std::string get_template_dir();

static bool
on_move_keypress(GtkWidget* widget, GdkEventKey* event, MoveSet* mset)
{
    (void)widget;
    u32 keymod = ptk_get_keymod(event->state);

    if (keymod == 0)
    {
        switch (event->keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                if (gtk_widget_get_sensitive(GTK_WIDGET(mset->next)))
                    gtk_dialog_response(GTK_DIALOG(mset->dlg), GtkResponseType::GTK_RESPONSE_OK);
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
    u32 keymod = ptk_get_keymod(event->state);

    if (keymod == 0)
    {
        switch (event->keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                if (gtk_widget_get_sensitive(GTK_WIDGET(mset->next)))
                    gtk_dialog_response(GTK_DIALOG(mset->dlg), GtkResponseType::GTK_RESPONSE_OK);
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

    std::string full_path;
    std::string path;
    GtkTextIter iter, siter;

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
        { // ignore leading dot in extension field
            ext++;
        }

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
        if (ztd::same(path, "."))
        {
            path = Glib::path_get_dirname(mset->full_path);
        }
        else if (ztd::same(path, ".."))
        {
            const std::string cwd = Glib::path_get_dirname(mset->full_path);
            path = Glib::path_get_dirname(cwd);
        }

        if (ztd::startswith(path, "/"))
        {
            full_path = Glib::build_filename(path, full_name);
        }
        else
        {
            const std::string cwd = Glib::path_get_dirname(mset->full_path);
            full_path = Glib::build_filename(cwd, path, full_name);
        }
        gtk_text_buffer_set_text(mset->buf_full_path, full_path.c_str(), -1);
    }
    else if (widget == GTK_WIDGET(mset->buf_full_name))
    {
        mset->last_widget = GTK_WIDGET(mset->input_full_name);

        // update name & ext
        gtk_text_buffer_get_start_iter(mset->buf_full_name, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_name, &iter);
        const std::string full_name =
            gtk_text_buffer_get_text(mset->buf_full_name, &siter, &iter, false);

        const auto namepack = get_name_extension(full_name);
        const std::string name = namepack.first;
        const std::string ext = namepack.second;

        gtk_text_buffer_set_text(mset->buf_name, name.c_str(), -1);
        if (!ext.empty())
            gtk_entry_set_text(mset->entry_ext, ext.c_str());
        else
            gtk_entry_set_text(mset->entry_ext, "");

        // update full_path
        gtk_text_buffer_get_start_iter(mset->buf_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_path, &iter);
        path = gtk_text_buffer_get_text(mset->buf_path, &siter, &iter, false);
        if (ztd::same(path, "."))
        {
            path = Glib::path_get_dirname(mset->full_path);
        }
        else if (ztd::same(path, ".."))
        {
            const std::string cwd = Glib::path_get_dirname(mset->full_path);
            path = Glib::path_get_dirname(cwd);
        }

        if (ztd::startswith(path, "/"))
        {
            full_path = Glib::build_filename(path, full_name);
        }
        else
        {
            const std::string cwd = Glib::path_get_dirname(mset->full_path);
            full_path = Glib::build_filename(cwd, path, full_name);
        }
        gtk_text_buffer_set_text(mset->buf_full_path, full_path.c_str(), -1);
    }
    else if (widget == GTK_WIDGET(mset->buf_path))
    {
        mset->last_widget = GTK_WIDGET(mset->input_path);
        // update full_path
        gtk_text_buffer_get_start_iter(mset->buf_full_name, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_name, &iter);
        const std::string full_name =
            gtk_text_buffer_get_text(mset->buf_full_name, &siter, &iter, false);

        gtk_text_buffer_get_start_iter(mset->buf_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_path, &iter);
        path = gtk_text_buffer_get_text(mset->buf_path, &siter, &iter, false);
        if (ztd::same(path, "."))
        {
            path = Glib::path_get_dirname(mset->full_path);
        }
        else if (ztd::same(path, ".."))
        {
            const std::string cwd = Glib::path_get_dirname(mset->full_path);
            path = Glib::path_get_dirname(cwd);
        }

        if (ztd::startswith(path, "/"))
        {
            full_path = Glib::build_filename(path, full_name);
        }
        else
        {
            const std::string cwd = Glib::path_get_dirname(mset->full_path);
            full_path = Glib::build_filename(cwd, path, full_name);
        }
        gtk_text_buffer_set_text(mset->buf_full_path, full_path.c_str(), -1);
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
            full_name = Glib::path_get_basename(full_path);

        path = Glib::path_get_dirname(full_path);
        if (ztd::same(path, "."))
        {
            path = Glib::path_get_dirname(mset->full_path);
        }
        else if (ztd::same(path, ".."))
        {
            const std::string cwd = Glib::path_get_dirname(mset->full_path);
            path = Glib::path_get_dirname(cwd);
        }
        else if (!ztd::startswith(path, "/"))
        {
            const std::string cwd = Glib::path_get_dirname(mset->full_path);
            path = Glib::build_filename(cwd, path);
        }

        const auto namepack = get_name_extension(full_name);
        const std::string name = namepack.first;
        const std::string ext = namepack.second;

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
        gtk_text_buffer_set_text(mset->buf_path, path.c_str(), -1);

        if (full_path[0] != '/')
        {
            // update full_path for tests below
            const std::string cwd = Glib::path_get_dirname(mset->full_path);
            full_path = Glib::build_filename(cwd, full_path);
        }
    }

    // change relative path to absolute
    if (!ztd::startswith(path, "/"))
    {
        path = Glib::path_get_dirname(full_path);
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

    if (ztd::same(full_path, mset->full_path))
    {
        full_path_same = true;
        if (mset->create_new && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link)))
        {
            if (lstat(full_path.c_str(), &statbuf) == 0)
            {
                full_path_exists = true;
                if (std::filesystem::is_directory(full_path))
                    full_path_exists_dir = true;
            }
        }
    }
    else
    {
        if (lstat(full_path.c_str(), &statbuf) == 0)
        {
            full_path_exists = true;
            if (std::filesystem::is_directory(full_path))
                full_path_exists_dir = true;
        }
        else if (lstat(path.c_str(), &statbuf) == 0)
        {
            if (!std::filesystem::is_directory(path))
                path_exists_file = true;
        }
        else
        {
            path_missing = true;
        }

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move)))
        {
            is_move = strcmp(path.c_str(), mset->old_path);
        }
    }

    // LOG_INFO("TEST")
    // LOG_INFO( "  full_path_same {} {}", full_path_same, mset->full_path_same);
    // LOG_INFO( "  full_path_exists {} {}", full_path_exists, mset->full_path_exists);
    // LOG_INFO( "  full_path_exists_dir {} {}", full_path_exists_dir, mset->full_path_exists_dir);
    // LOG_INFO( "  path_missing {} {}", path_missing, mset->path_missing);
    // LOG_INFO( "  path_exists_file {} {}", path_exists_file, mset->path_exists_file);

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
        path = gtk_entry_get_text(GTK_ENTRY(mset->entry_target));
        path = ztd::strip(path);
        gtk_widget_set_sensitive(mset->next,
                                 (!(full_path_same && full_path_exists) && !full_path_exists_dir));
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

            const auto namepack = get_name_extension(full_name);
            const std::string name = namepack.first;
            const std::string ext = namepack.second;

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
    i32 action;
    const char* title;
    const char* text;

    std::string dir;
    std::string name;

    if (widget == GTK_WIDGET(mset->browse_target))
    {
        title = "Select Link Target";
        action = GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN;
        text = gtk_entry_get_text(mset->entry_target);
        if (text[0] == '/')
        {
            dir = Glib::path_get_dirname(text);
            name = Glib::path_get_basename(text);
        }
        else
        {
            dir = Glib::path_get_dirname(mset->full_path);
            if (text)
                name = text;
        }
    }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
    {
        title = "Select Template File";
        action = GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN;
        text = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(mset->combo_template))));
        if (text && text[0] == '/')
        {
            dir = Glib::path_get_dirname(text);
            name = Glib::path_get_basename(text);
        }
        else
        {
            dir = get_template_dir();
            if (dir.empty())
                dir = Glib::path_get_dirname(mset->full_path);
            if (text)
                name = text;
        }
    }
    else
    {
        title = "Select Template Directory";
        action = GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        text = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(mset->combo_template))));
        if (text && text[0] == '/')
        {
            dir = Glib::path_get_dirname(text);
            name = Glib::path_get_basename(text);
        }
        else
        {
            dir = get_template_dir();
            if (dir.empty())
                dir = Glib::path_get_dirname(mset->full_path);
            if (text)
                name = text;
        }
    }

    GtkWidget* dlg = gtk_file_chooser_dialog_new(title,
                                                 mset->parent ? GTK_WINDOW(mset->parent) : nullptr,
                                                 (GtkFileChooserAction)action,
                                                 "Cancel",
                                                 GtkResponseType::GTK_RESPONSE_CANCEL,
                                                 "OK",
                                                 GtkResponseType::GTK_RESPONSE_OK,
                                                 nullptr);

    xset_set_window_icon(GTK_WINDOW(dlg));

    if (name.empty())
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), dir.c_str());
    }
    else
    {
        const std::string path = Glib::build_filename(dir, name);
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), path.c_str());
    }

    i32 width = xset_get_int(XSetName::MOVE_DLG_HELP, XSetVar::X);
    i32 height = xset_get_int(XSetName::MOVE_DLG_HELP, XSetVar::Y);
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(dlg);
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_resize(GTK_WINDOW(dlg), width, height);
        while (gtk_events_pending())
            gtk_main_iteration();
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
    }

    i32 response = gtk_dialog_run(GTK_DIALOG(dlg));
    if (response == GtkResponseType::GTK_RESPONSE_OK)
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
            if (!dir.empty())
            {
                if (ztd::startswith(new_path, dir) && new_path[dir.size()] == '/')
                    path = new_path + dir.size() + 1;
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
        xset_set(XSetName::MOVE_DLG_HELP, XSetVar::X, std::to_string(width));
        xset_set(XSetName::MOVE_DLG_HELP, XSetVar::Y, std::to_string(height));
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

    GtkWidget** mode = (GtkWidget**)g_object_get_data(G_OBJECT(dlg), "mode");

    for (i32 i = MODE_FILENAME; i <= MODE_PATH; ++i)
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode[i])))
        {
            i32 action = i == MODE_PARENT
                             ? GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                             : GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE;
            GtkAllocation allocation;
            gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
            i32 width = allocation.width;
            i32 height = allocation.height;
            gtk_file_chooser_set_action(GTK_FILE_CHOOSER(dlg), (GtkFileChooserAction)action);
            if (width && height)
            {
                // under some circumstances, changing the action changes the size
                gtk_window_set_position(GTK_WINDOW(dlg),
                                        GtkWindowPosition::GTK_WIN_POS_CENTER_ALWAYS);
                gtk_window_resize(GTK_WINDOW(dlg), width, height);
                while (gtk_events_pending())
                    gtk_main_iteration();
                gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
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
    i32 mode_default = MODE_PARENT;

    xset_t set = xset_get(XSetName::MOVE_DLG_HELP);
    if (set->z)
        mode_default = xset_get_int(XSetName::MOVE_DLG_HELP, XSetVar::Z);

    // action create directory does not work properly so not used:
    //  it creates a directory by default with no way to stop it
    //  it gives 'directory already exists' error popup
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Browse",
        mset->parent ? GTK_WINDOW(mset->parent) : nullptr,
        mode_default == MODE_PARENT ? GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                                    : GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE,
        "Cancel",
        GtkResponseType::GTK_RESPONSE_CANCEL,
        "OK",
        GtkResponseType::GTK_RESPONSE_OK,
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
    GtkWidget* mode[3];
    GtkWidget* hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    mode[MODE_FILENAME] = gtk_radio_button_new_with_mnemonic(nullptr, "Fil_ename");
    mode[MODE_PARENT] =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mode[MODE_FILENAME]),
                                                       "Pa_rent");
    mode[MODE_PATH] =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mode[MODE_FILENAME]),
                                                       "P_ath");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mode[mode_default]), true);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Insert as"), false, true, 2);
    for (i32 i = MODE_FILENAME; i <= MODE_PATH; ++i)
    {
        gtk_widget_set_focus_on_click(GTK_WIDGET(mode[i]), false);
        g_signal_connect(G_OBJECT(mode[i]), "toggled", G_CALLBACK(on_browse_mode_toggled), dlg);
        gtk_box_pack_start(GTK_BOX(hbox), mode[i], false, true, 2);
    }
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), hbox, false, true, 6);
    g_object_set_data(G_OBJECT(dlg), "mode", mode);
    gtk_widget_show_all(hbox);

    i32 width = xset_get_int(XSetName::MOVE_DLG_HELP, XSetVar::X);
    i32 height = xset_get_int(XSetName::MOVE_DLG_HELP, XSetVar::Y);
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(dlg);
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_resize(GTK_WINDOW(dlg), width, height);
        while (gtk_events_pending())
            gtk_main_iteration();
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
    }

    i32 response = gtk_dialog_run(GTK_DIALOG(dlg));
    // bogus GTK warning here: Unable to retrieve the file info for...
    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        for (i32 i = MODE_FILENAME; i <= MODE_PATH; ++i)
        {
            if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode[i])))
                continue;

            std::string str;
            switch (i)
            {
                case MODE_FILENAME:
                    path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
                    str = Glib::path_get_basename(path);
                    gtk_text_buffer_set_text(mset->buf_full_name, str.c_str(), -1);
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

    // save size
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if (width && height)
    {
        xset_set(XSetName::MOVE_DLG_HELP, XSetVar::X, std::to_string(width));
        xset_set(XSetName::MOVE_DLG_HELP, XSetVar::Y, std::to_string(height));
    }

    // save mode
    for (i32 i = MODE_FILENAME; i <= MODE_PATH; ++i)
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode[i])))
        {
            xset_set(XSetName::MOVE_DLG_HELP, XSetVar::Z, std::to_string(i));
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
        const std::string new_path = Glib::path_get_dirname(full_path);

        bool rename = (ztd::same(mset->old_path, new_path) || ztd::same(new_path, "."));
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
                                                 GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN,
                                                 nullptr);
    if (pixbuf)
        gtk_window_set_icon(GTK_WINDOW(mset->dlg), pixbuf);

    // title
    if (!desc)
        desc = mset->desc;
    const std::string title = fmt::format("{} {}{}", action, desc, root_msg);
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
    if (xset_get_b(XSetName::MOVE_COPY) || mset->clip_copy)
        gtk_widget_show(mset->opt_copy);
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        gtk_widget_hide(mset->opt_copy);
    }

    if (xset_get_b(XSetName::MOVE_LINK))
    {
        gtk_widget_show(mset->opt_link);
    }
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        gtk_widget_hide(mset->opt_link);
    }

    if (xset_get_b(XSetName::MOVE_COPYT) && mset->is_link)
        gtk_widget_show(mset->opt_copy_target);
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy_target)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        gtk_widget_hide(mset->opt_copy_target);
    }

    if (xset_get_b(XSetName::MOVE_LINKT) && mset->is_link)
        gtk_widget_show(mset->opt_link_target);
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link_target)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        gtk_widget_hide(mset->opt_link_target);
    }

    if (xset_get_b(XSetName::MOVE_AS_ROOT))
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
    if (xset_get_b(XSetName::MOVE_NAME))
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

    if (xset_get_b(XSetName::MOVE_FILENAME))
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

    if (xset_get_b(XSetName::MOVE_PARENT))
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

    if (xset_get_b(XSetName::MOVE_PATH))
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

    if (!mset->is_link && !mset->create_new && xset_get_b(XSetName::MOVE_TYPE))
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

    if (new_link || (mset->is_link && xset_get_b(XSetName::MOVE_TARGET)))
    {
        gtk_widget_show(GTK_WIDGET(mset->hbox_target));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->hbox_target));
    }

    if ((new_file || new_folder) && xset_get_b(XSetName::MOVE_TEMPLATE))
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
        xset_set_b(XSetName::MOVE_FILENAME, true);
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

    xset_t set;

    set = xset_set_cb(XSetName::MOVE_NAME, (GFunc)on_toggled, mset);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb(XSetName::MOVE_FILENAME, (GFunc)on_toggled, mset);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb(XSetName::MOVE_PARENT, (GFunc)on_toggled, mset);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb(XSetName::MOVE_PATH, (GFunc)on_toggled, mset);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb(XSetName::MOVE_TYPE, (GFunc)on_toggled, mset);
    set->disable = (mset->create_new || mset->is_link);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb(XSetName::MOVE_TARGET, (GFunc)on_toggled, mset);
    set->disable = mset->create_new || !mset->is_link;
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_set_cb(XSetName::MOVE_TEMPLATE, (GFunc)on_toggled, mset);
    set->disable = !mset->create_new;
    xset_add_menuitem(mset->browser, popup, accel_group, set);

    set = xset_set_cb(XSetName::MOVE_COPY, (GFunc)on_toggled, mset);
    set->disable = mset->clip_copy || mset->create_new;
    set = xset_set_cb(XSetName::MOVE_LINK, (GFunc)on_toggled, mset);
    set->disable = mset->create_new;
    set = xset_set_cb(XSetName::MOVE_COPYT, (GFunc)on_toggled, mset);
    set->disable = !mset->is_link;
    set = xset_set_cb(XSetName::MOVE_LINKT, (GFunc)on_toggled, mset);
    set->disable = !mset->is_link;
    xset_set_cb(XSetName::MOVE_AS_ROOT, (GFunc)on_toggled, mset);
    set = xset_get(XSetName::MOVE_OPTION);
    xset_add_menuitem(mset->browser, popup, accel_group, set);

    set = xset_get(XSetName::SEPARATOR);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_get(XSetName::MOVE_DLG_CONFIRM_CREATE);
    xset_add_menuitem(mset->browser, popup, accel_group, set);
    set = xset_get(XSetName::SEPARATOR);
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
    if (event->type == GdkEventType::GDK_BUTTON_PRESS)
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
    else if (event->type == GdkEventType::GDK_2BUTTON_PRESS)
    {
        copy_entry_to_clipboard(widget, mset);
    }

    return true;
}

static const std::string
get_unique_name(std::string_view dir, std::string_view ext = "")
{
    const std::string base = "new";

    std::string path;

    if (ext.empty())
    {
        path = Glib::build_filename(dir.data(), base);
    }
    else
    {
        const std::string name = fmt::format("{}.{}", base, ext);
        path = Glib::build_filename(dir.data(), name);
    }

    u32 n = 1;
    while (ztd::lstat(path).is_valid()) // need to see broken symlinks
    {
        std::string name;
        if (ext.empty())
            name = fmt::format("{}{}", base, ++n);
        else
            name = fmt::format("{}{}.{}", base, ++n, ext);

        path = Glib::build_filename(dir.data(), name);
    }

    return path;
}

static const std::string
get_template_dir()
{
    const std::string templates_path = vfs_user_template_dir();

    if (ztd::same(templates_path, vfs_user_home_dir()))
    {
        /* If $XDG_TEMPLATES_DIR == $HOME this means it is disabled. Do not
         * recurse it as this is too many files/directories and may slow
         * dialog open and cause filesystem find loops.
         * https://wiki.freedesktop.org/www/Software/xdg-user-dirs/ */
        return "";
    }

    return templates_path;
}

static const std::vector<std::string>
get_templates(std::string_view templates_dir, std::string_view subdir, bool getdir)
{
    std::vector<std::string> templates;

    const std::string templates_path = Glib::build_filename(templates_dir.data(), subdir.data());

    if (!std::filesystem::is_directory(templates_path))
        return templates;

    for (const auto& file: std::filesystem::directory_iterator(templates_path))
    {
        const std::string file_name = std::filesystem::path(file).filename();

        std::string path = Glib::build_filename(templates_path, file_name);
        if (getdir)
        {
            if (std::filesystem::is_directory(path))
            {
                std::string subsubdir;

                if (subdir.empty())
                    subsubdir = file_name;
                else
                    subsubdir = Glib::build_filename(subdir.data(), file_name);

                templates.push_back(subsubdir);

                // prevent filesystem loops during recursive find
                if (!std::filesystem::is_symlink(path))
                {
                    std::vector<std::string> subsubdir_templates =
                        get_templates(templates_dir, subsubdir, getdir);

                    templates = ztd::merge(templates, subsubdir_templates);
                }
            }
        }
        else
        {
            if (std::filesystem::is_regular_file(path))
            {
                if (subdir.empty())
                {
                    templates.push_back(file_name);
                }
                else
                {
                    templates.push_back(Glib::build_filename(subdir.data(), file_name));
                }
            }
            else if (std::filesystem::is_directory(path) &&
                     // prevent filesystem loops during recursive find
                     !std::filesystem::is_symlink(path))
            {
                if (subdir.empty())
                {
                    std::vector<std::string> subsubdir_templates =
                        get_templates(templates_dir, file_name, getdir);

                    templates = ztd::merge(templates, subsubdir_templates);
                }
                else
                {
                    const std::string subsubdir = Glib::build_filename(subdir.data(), file_name);
                    std::vector<std::string> subsubdir_templates =
                        get_templates(templates_dir, subsubdir, getdir);

                    templates = ztd::merge(templates, subsubdir_templates);
                }
            }
        }
    }

    return templates;
}

static void
on_template_changed(GtkWidget* widget, MoveSet* mset)
{
    (void)widget;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
        return;

    std::string ext;

    const char* text =
        gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(mset->combo_template))));
    if (text)
    {
        // ext = ztd::strip(text);
        ext = ztd::rpartition(ext, "/")[2];
        if (ztd::contains(ext, "."))
            ext = ztd::rpartition(ext, ".")[2];
        else
            ext = "";
    }
    gtk_entry_set_text(mset->entry_ext, ext.c_str());

    // need new name due to extension added?
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
    gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
    const char* full_path = gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);

    if (std::filesystem::exists(full_path) ||
        std::filesystem::is_symlink(full_path)) // need to see broken symlinks
    {
        const std::string dir = Glib::path_get_dirname(full_path);
        const std::string unique_path = get_unique_name(dir, ext);

        gtk_text_buffer_set_text(mset->buf_full_path, unique_path.c_str(), -1);
    }
}

static bool
update_new_display_delayed(char* path)
{
    const std::string dir_path = Glib::path_get_dirname(path);
    vfs::dir vdir = vfs_dir_get_by_path_soft(dir_path.c_str());
    if (vdir && vdir->avoid_changes)
    {
        vfs::file_info file = vfs_file_info_new();
        vfs_file_info_get(file, path);
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

i32
ptk_rename_file(PtkFileBrowser* file_browser, const char* file_dir, vfs::file_info file,
                const char* dest_dir, bool clip_copy, PtkRenameMode create_new,
                AutoOpenCreate* auto_open)
{
    // TODO convert to gtk_builder (glade file)

    char* str;
    GtkWidget* task_view = nullptr;
    i32 ret = 1;
    bool target_missing = false;
    struct stat statbuf;

    if (!file_dir)
        return 0;

    MoveSet* mset = new MoveSet;

    if (!create_new)
    {
        if (!file)
            return 0;

        std::string full_name;
        // special processing for files with inconsistent real name and display name
        if (vfs_file_info_is_desktop_entry(file))
            full_name = Glib::filename_display_name(file->name);
        if (full_name.empty())
            full_name = vfs_file_info_get_disp_name(file);
        if (full_name.empty())
            full_name = vfs_file_info_get_name(file);

        mset->is_dir = vfs_file_info_is_dir(file);
        mset->is_link = vfs_file_info_is_symlink(file);
        mset->clip_copy = clip_copy;
        mset->full_path = ztd::strdup(Glib::build_filename(file_dir, full_name));
        if (dest_dir)
            mset->new_path = ztd::strdup(Glib::build_filename(dest_dir, full_name));
        else
            mset->new_path = ztd::strdup(mset->full_path);
    }
    else if (create_new == PtkRenameMode::PTK_RENAME_NEW_LINK && file)
    {
        std::string full_name = vfs_file_info_get_disp_name(file);
        if (full_name.empty())
            full_name = ztd::strdup(vfs_file_info_get_name(file));
        mset->full_path = ztd::strdup(Glib::build_filename(file_dir, full_name));
        mset->new_path = ztd::strdup(mset->full_path);
        mset->is_dir = vfs_file_info_is_dir(file); // is_dir is dynamic for create
        mset->is_link = vfs_file_info_is_symlink(file);
        mset->clip_copy = false;
    }
    else
    {
        mset->full_path = ztd::strdup(get_unique_name(file_dir));
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

    mset->dlg =
        gtk_dialog_new_with_buttons("Move",
                                    mset->parent ? GTK_WINDOW(mset->parent) : nullptr,
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    nullptr,
                                    nullptr);
    // free( title );
    gtk_window_set_role(GTK_WINDOW(mset->dlg), "rename_dialog");

    // Buttons
    mset->options = gtk_button_new_with_mnemonic("Opt_ions");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->options,
                                 GtkResponseType::GTK_RESPONSE_YES);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->options), false);
    g_signal_connect(G_OBJECT(mset->options), "clicked", G_CALLBACK(on_options_button_press), mset);

    mset->browse = gtk_button_new_with_mnemonic("_Browse");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->browse,
                                 GtkResponseType::GTK_RESPONSE_YES);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->browse), false);
    g_signal_connect(G_OBJECT(mset->browse), "clicked", G_CALLBACK(on_browse_button_press), mset);

    mset->revert = gtk_button_new_with_mnemonic("Re_vert");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->revert,
                                 GtkResponseType::GTK_RESPONSE_NO);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->revert), false);
    g_signal_connect(G_OBJECT(mset->revert), "clicked", G_CALLBACK(on_revert_button_press), mset);

    mset->cancel = gtk_button_new_with_label("Cancel");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->cancel,
                                 GtkResponseType::GTK_RESPONSE_CANCEL);

    mset->next = gtk_button_new_with_label("OK");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->next,
                                 GtkResponseType::GTK_RESPONSE_OK);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->next), false);
    gtk_button_set_label(GTK_BUTTON(mset->next), "_Rename");

    if (create_new && auto_open)
    {
        mset->open = gtk_button_new_with_mnemonic("& _Open");
        gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                     mset->open,
                                     GtkResponseType::GTK_RESPONSE_APPLY);
        gtk_widget_set_focus_on_click(GTK_WIDGET(mset->open), false);
    }
    else
    {
        mset->open = nullptr;
    }

    // Window
    gtk_widget_set_size_request(GTK_WIDGET(mset->dlg), 800, 500);
    gtk_window_set_resizable(GTK_WINDOW(mset->dlg), true);
    gtk_window_set_type_hint(GTK_WINDOW(mset->dlg), GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_widget_show_all(mset->dlg);

    // Entries

    // Type
    std::string type;
    mset->label_type = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_type, "<b>Type:</b>");
    if (mset->is_link)
    {
        try
        {
            const std::string target_path = std::filesystem::read_symlink(mset->full_path);

            mset->mime_type = ztd::strdup(target_path);
            if (std::filesystem::exists(target_path))
                type = fmt::format("Link-> {}", target_path);
            else
            {
                type = fmt::format("!Link-> {} (missing)", target_path);
                target_missing = true;
            }
        }
        catch (std::filesystem::filesystem_error)
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
    gtk_widget_set_halign(GTK_WIDGET(mset->label_mime), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_mime), GtkAlign::GTK_ALIGN_START);

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
        gtk_widget_set_halign(GTK_WIDGET(mset->label_target), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(mset->label_target), GtkAlign::GTK_ALIGN_END);
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
        gtk_widget_set_halign(GTK_WIDGET(mset->label_template), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(mset->label_template), GtkAlign::GTK_ALIGN_END);
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
        std::vector<std::string> templates;

        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template), "Empty File");
        templates.clear();
        templates = get_templates(get_template_dir(), "", false);
        if (!templates.empty())
        {
            std::ranges::sort(templates);
            for (std::string_view t: templates)
            {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template), t.data());
            }
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
        templates.clear();
        templates = get_templates(get_template_dir(), "", true);
        if (!templates.empty())
        {
            std::ranges::sort(templates);
            for (std::string_view t: templates)
            {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template_dir),
                                               t.data());
            }
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
    gtk_widget_set_halign(GTK_WIDGET(mset->label_name), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_name), GtkAlign::GTK_ALIGN_START);
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
    gtk_widget_set_halign(GTK_WIDGET(mset->label_ext), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_ext), GtkAlign::GTK_ALIGN_END);
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
    gtk_widget_set_halign(GTK_WIDGET(mset->label_full_name), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_full_name), GtkAlign::GTK_ALIGN_START);
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
    gtk_widget_set_halign(GTK_WIDGET(mset->label_path), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_path), GtkAlign::GTK_ALIGN_START);
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
    gtk_widget_set_halign(GTK_WIDGET(mset->label_full_path), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_full_path), GtkAlign::GTK_ALIGN_START);
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

    mset->hbox_ext = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
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

    mset->hbox_type = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(mset->hbox_type), GTK_WIDGET(mset->label_type), false, true, 0);
    gtk_box_pack_start(GTK_BOX(mset->hbox_type), GTK_WIDGET(mset->label_mime), true, true, 5);
    gtk_box_pack_start(GTK_BOX(dlg_vbox), GTK_WIDGET(mset->hbox_type), false, true, 5);

    mset->hbox_target = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
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

    mset->hbox_template = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
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

    GtkWidget* hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
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
    std::string full_path;
    i32 response;
    while ((response = gtk_dialog_run(GTK_DIALOG(mset->dlg))))
    {
        if (response == GtkResponseType::GTK_RESPONSE_OK ||
            response == GtkResponseType::GTK_RESPONSE_APPLY)
        {
            GtkTextIter iter;
            GtkTextIter siter;
            gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
            gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
            full_path = gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);
            if (full_path[0] != '/')
            {
                // update full_path to absolute
                const std::string cwd = Glib::path_get_dirname(mset->full_path);
                full_path = Glib::build_filename(cwd, full_path);
            }
            if (ztd::contains(full_path, "\n"))
            {
                ptk_show_error(GTK_WINDOW(mset->dlg), "Error", "Path contains linefeeds");
                continue;
            }
            // const std::string full_name = Glib::path_get_basename(full_path);
            const std::string path = Glib::path_get_dirname(full_path);
            const std::string old_path = Glib::path_get_dirname(mset->full_path);
            bool overwrite = false;

            if (response == GtkResponseType::GTK_RESPONSE_APPLY)
                ret = 2;

            if (!create_new && (mset->full_path_same || ztd::same(full_path, mset->full_path)))
            {
                // not changed, proceed to next file
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
                if (xset_get_b(XSetName::MOVE_DLG_CONFIRM_CREATE))
                {
                    if (xset_msg_dialog(mset->parent,
                                        GtkMessageType::GTK_MESSAGE_QUESTION,
                                        "Create Parent Directory",
                                        GtkButtonsType::GTK_BUTTONS_YES_NO,
                                        "The parent directory does not exist.  Create it?") !=
                        GtkResponseType::GTK_RESPONSE_YES)
                    {
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
                        const std::string errno_msg = std::strerror(errno);
                        const std::string msg =
                            fmt::format("Error creating parent directory\n\n{}", errno_msg);
                        ptk_show_error(GTK_WINDOW(mset->dlg), "Mkdir Error", msg);

                        continue;
                    }
                    else
                    {
                        update_new_display(path.c_str());
                    }
                }
            }
            else if (lstat(full_path.c_str(), &statbuf) == 0)
            {
                // overwrite
                if (std::filesystem::is_directory(full_path))
                {
                    // just in case
                    continue;
                }
                if (xset_msg_dialog(mset->parent,
                                    GtkMessageType::GTK_MESSAGE_WARNING,
                                    "Overwrite Existing File",
                                    GtkButtonsType::GTK_BUTTONS_YES_NO,
                                    "OVERWRITE WARNING",
                                    "The file path exists.  Overwrite existing file?") !=
                    GtkResponseType::GTK_RESPONSE_YES)
                {
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
                while (ztd::endswith(str, "/") && str[1] != '\0')
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
                    auto_open->open_file = (response == GtkResponseType::GTK_RESPONSE_APPLY);
                    ptask->complete_notify = auto_open->callback;
                    ptask->user_data = auto_open;
                }
                ptk_file_task_run(ptask);
                update_new_display(full_path.c_str());
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
                        const std::string templates_path = get_template_dir();
                        if (!templates_path.empty())
                        {
                            from_path = Glib::build_filename(templates_path, str);
                            if (!std::filesystem::is_regular_file(from_path))
                            {
                                ptk_show_error(GTK_WINDOW(mset->dlg),
                                               "Template Missing",
                                               "The specified template does not exist");
                                free(str);
                                continue;
                            }
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
                    auto_open->open_file = (response == GtkResponseType::GTK_RESPONSE_APPLY);
                    ptask->complete_notify = auto_open->callback;
                    ptask->user_data = auto_open;
                }
                ptk_file_task_run(ptask);
                update_new_display(full_path.c_str());
            }
            else if (create_new)
            {
                // new directory task
                if (!new_folder)
                {
                    // failsafe
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
                        const std::string templates_path = get_template_dir();
                        if (!templates_path.empty())
                        {
                            from_path = Glib::build_filename(templates_path, str);
                            if (!std::filesystem::is_directory(from_path))
                            {
                                ptk_show_error(GTK_WINDOW(mset->dlg),
                                               "Template Missing",
                                               "The specified template does not exist");
                                free(str);
                                continue;
                            }
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
                    auto_open->open_file = (response == GtkResponseType::GTK_RESPONSE_APPLY);
                    ptask->complete_notify = auto_open->callback;
                    ptask->user_data = auto_open;
                }
                ptk_file_task_run(ptask);
                update_new_display(full_path.c_str());
            }
            else if (copy || copy_target)
            {
                // copy task
                task_name = fmt::format("Copy{}", root_msg);
                PtkFileTask* ptask = ptk_file_exec_new(task_name, nullptr, mset->parent, task_view);
                char* over_opt = nullptr;
                to_path = bash_quote(full_path);
                if (copy || !mset->is_link)
                {
                    from_path = bash_quote(mset->full_path);
                }
                else
                {
                    const std::string real_path = get_real_link_target(mset->full_path);
                    if (ztd::same(real_path, mset->full_path))
                    {
                        ptk_show_error(GTK_WINDOW(mset->dlg),
                                       "Copy Target Error",
                                       "Error determining link's target");
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
                update_new_display(full_path.c_str());
            }
            else if (link || link_target)
            {
                // link task
                task_name = fmt::format("Create Link{}", root_msg);
                PtkFileTask* ptask = ptk_file_exec_new(task_name, nullptr, mset->parent, task_view);
                if (link || !mset->is_link)
                {
                    from_path = bash_quote(mset->full_path);
                }
                else
                {
                    const std::string real_path = get_real_link_target(mset->full_path);
                    if (ztd::same(real_path, mset->full_path))
                    {
                        ptk_show_error(GTK_WINDOW(mset->dlg),
                                       "Link Target Error",
                                       "Error determining link's target");
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
                update_new_display(full_path.c_str());
            }
            // need move?  (do move as task in case it takes a long time)
            else if (as_root || !ztd::same(old_path, path))
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
                update_new_display(full_path.c_str());
            }
            else
            {
                // rename (does overwrite)
                if (rename(mset->full_path, full_path.c_str()) != 0)
                {
                    // Respond to an EXDEV error by switching to a move (e.g. aufs
                    // directory rename fails due to the directory existing in
                    // multiple underlying branches)
                    if (errno == EXDEV)
                        goto _move_task;

                    // Unknown error has occurred - alert user as usual
                    const std::string errno_msg = std::strerror(errno);
                    const std::string msg = fmt::format("Error renaming file\n\n{}", errno_msg);
                    ptk_show_error(GTK_WINDOW(mset->dlg), "Rename Error", msg);
                    continue;
                }
                else
                {
                    update_new_display(full_path.c_str());
                }
            }
            break;
        }
        else if (response == GtkResponseType::GTK_RESPONSE_CANCEL ||
                 response == GtkResponseType::GTK_RESPONSE_DELETE_EVENT)
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

    delete mset;

    return ret;
}

/////////////////////////////////////////////////////////////

void
ptk_file_misc_paste_as(PtkFileBrowser* file_browser, const char* cwd, GFunc callback)
{
    (void)callback;
    bool is_cut = false;
    i32 missing_targets;
    vfs::file_info file;
    std::string file_dir;

    const std::vector<std::string> files =
        ptk_clipboard_get_file_paths(cwd, &is_cut, &missing_targets);

    for (std::string_view file_path: files)
    {
        file = vfs_file_info_new();
        vfs_file_info_get(file, file_path);
        file_dir = std::filesystem::path(file_path).parent_path();

        if (!ptk_rename_file(file_browser,
                             file_dir.c_str(),
                             file,
                             cwd,
                             !is_cut,
                             PtkRenameMode::PTK_RENAME,
                             nullptr))
        {
            vfs_file_info_unref(file);
            missing_targets = 0;
            break;
        }
        vfs_file_info_unref(file);
    }

    if (missing_targets > 0)
    {
        GtkWindow* parent = nullptr;
        if (file_browser)
            parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser)));

        const std::string msg = fmt::format("{} target{} missing",
                                            missing_targets,
                                            missing_targets > 1 ? "s are" : " is");
        ptk_show_error(parent, "Error", msg);
    }
}

void
ptk_file_misc_rootcmd(PtkFileBrowser* file_browser, const std::vector<vfs::file_info>& sel_files,
                      const char* cwd, const char* setname)
{
    /*
     * root_copy_loc    copy to location
     * root_move2       move to
     * root_delete      delete
     */
    if (!setname || !file_browser)
        return;
    xset_t set;
    char* path;
    std::string cmd;
    std::string task_name;

    GtkWidget* parent = GTK_WIDGET(file_browser);
    std::string file_paths;
    std::string file_path_q;
    i32 item_count = 0;
    for (vfs::file_info file: sel_files)
    {
        const std::string file_path = Glib::build_filename(cwd, vfs_file_info_get_name(file));
        file_path_q = bash_quote(file_path);
        file_paths = fmt::format("{} {}", file_paths, file_path_q);
        item_count++;
    }

    if (!strcmp(setname, "root_delete"))
    {
        if (app_settings.get_confirm_delete())
        {
            const std::string msg = fmt::format("Delete {} selected item as root ?", item_count);
            if (xset_msg_dialog(parent,
                                GtkMessageType::GTK_MESSAGE_WARNING,
                                "Confirm Delete As Root",
                                GtkButtonsType::GTK_BUTTONS_YES_NO,
                                "DELETE AS ROOT",
                                msg) != GtkResponseType::GTK_RESPONSE_YES)
            {
                return;
            }
        }
        cmd = fmt::format("rm -r {}", file_paths);
        task_name = "Delete As Root";
    }
    else
    {
        const char* folder;
        set = xset_get(setname);
        if (set->desc)
            folder = set->desc;
        else
            folder = cwd;
        path = xset_file_dialog(parent,
                                GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                "Choose Location",
                                folder,
                                nullptr);
        if (path && std::filesystem::is_directory(path))
        {
            xset_set_var(set, XSetVar::DESC, path);
            const std::string quote_path = bash_quote(path);

            if (!strcmp(setname, "root_move2"))
            {
                task_name = "Move As Root";
                // problem: no warning if already exists
                cmd = fmt::format("mv -f {} {}", file_paths, quote_path);
            }
            else
            {
                task_name = "Copy As Root";
                // problem: no warning if already exists
                cmd = fmt::format("cp -r {} {}", file_paths, quote_path);
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
