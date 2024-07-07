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

#include <array>
#include <vector>

#include <optional>

#include <ranges>
#include <algorithm>

#include <memory>

#include <system_error>

#include <cstring>

#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"
#include "xset/xset-dialog.hxx"

#include "ptk/ptk-dialog.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-file-browser.hxx"
#include "ptk/utils/ptk-utils.hxx"

#include "vfs/vfs-file.hxx"
#include "vfs/vfs-user-dirs.hxx"
#include "vfs/utils/vfs-utils.hxx"

#include "utils/shell-quote.hxx"

#include "ptk/ptk-file-action-rename.hxx"

struct MoveSet : public std::enable_shared_from_this<MoveSet>
{
    MoveSet() = delete;
    MoveSet(const std::shared_ptr<vfs::file>& file) noexcept;
    ~MoveSet() = default;
    MoveSet(const MoveSet& other) = delete;
    MoveSet(MoveSet&& other) = delete;
    MoveSet& operator=(const MoveSet& other) = delete;
    MoveSet& operator=(MoveSet&& other) = delete;

    std::shared_ptr<vfs::file> file;

    std::filesystem::path full_path;
    std::filesystem::path old_path;
    std::filesystem::path new_path;
    std::string desc;
    bool is_dir{false};
    bool is_link{false};
    bool clip_copy{false};
    ptk::action::rename_mode create_new;

    GtkWidget* dlg{nullptr};
    GtkWidget* parent{nullptr};
    ptk::browser* browser{nullptr};

    GtkLabel* label_type{nullptr};
    GtkLabel* label_mime{nullptr};
    GtkBox* hbox_type{nullptr};
    std::string mime_type;

    GtkLabel* label_target{nullptr};
    GtkEntry* entry_target{nullptr};
    GtkBox* hbox_target{nullptr};
    GtkWidget* browse_target{nullptr};

    GtkLabel* label_template{nullptr};
    GtkComboBox* combo_template{nullptr};
    GtkComboBox* combo_template_dir{nullptr};
    GtkBox* hbox_template{nullptr};
    GtkWidget* browse_template{nullptr};

    GtkLabel* label_name{nullptr};
    GtkScrolledWindow* scroll_name{nullptr};
    GtkWidget* input_name{nullptr};
    GtkTextBuffer* buf_name{nullptr};
    GtkLabel* blank_name{nullptr};

    GtkBox* hbox_ext{nullptr};
    GtkLabel* label_ext{nullptr};
    GtkEntry* entry_ext{nullptr};

    GtkLabel* label_full_name{nullptr};
    GtkScrolledWindow* scroll_full_name{nullptr};
    GtkWidget* input_full_name{nullptr};
    GtkTextBuffer* buf_full_name{nullptr};
    GtkLabel* blank_full_name{nullptr};

    GtkLabel* label_path{nullptr};
    GtkScrolledWindow* scroll_path{nullptr};
    GtkWidget* input_path{nullptr};
    GtkTextBuffer* buf_path{nullptr};
    GtkLabel* blank_path{nullptr};

    GtkLabel* label_full_path{nullptr};
    GtkScrolledWindow* scroll_full_path{nullptr};
    GtkWidget* input_full_path{nullptr};
    GtkTextBuffer* buf_full_path{nullptr};

    GtkWidget* opt_move{nullptr};
    GtkWidget* opt_copy{nullptr};
    GtkWidget* opt_link{nullptr};
    GtkWidget* opt_copy_target{nullptr};
    GtkWidget* opt_link_target{nullptr};

    GtkWidget* opt_new_file{nullptr};
    GtkWidget* opt_new_folder{nullptr};
    GtkWidget* opt_new_link{nullptr};

    GtkWidget* options{nullptr};
    GtkWidget* browse{nullptr};
    GtkWidget* revert{nullptr};
    GtkWidget* cancel{nullptr};
    GtkWidget* next{nullptr};
    GtkWidget* open{nullptr};

    GtkWidget* last_widget{nullptr};

    bool full_path_exists{false};
    bool full_path_exists_dir{false};
    bool full_path_same{false};
    bool path_missing{false};
    bool path_exists_file{false};
    bool mode_change{false};
    bool is_move{false};
};

MoveSet::MoveSet(const std::shared_ptr<vfs::file>& file) noexcept : file(file) {}

static void on_toggled(GtkMenuItem* item, const std::shared_ptr<MoveSet>& mset) noexcept;
static const std::optional<std::filesystem::path> get_template_dir() noexcept;

static bool
on_move_keypress(GtkWidget* widget, GdkEvent* event, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)widget;
    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto keyval = gdk_key_event_get_keyval(event);

    if (keymod == 0)
    {
        switch (keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                if (gtk_widget_get_sensitive(GTK_WIDGET(mset->next)))
                {
                    gtk_dialog_response(GTK_DIALOG(mset->dlg), GtkResponseType::GTK_RESPONSE_OK);
                }
                return true;
            default:
                break;
        }
    }
    return false;
}

static bool
on_move_entry_keypress(GtkWidget* widget, GdkEvent* event,
                       const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)widget;
    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto keyval = gdk_key_event_get_keyval(event);

    if (keymod == 0)
    {
        switch (keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                if (gtk_widget_get_sensitive(GTK_WIDGET(mset->next)))
                {
                    gtk_dialog_response(GTK_DIALOG(mset->dlg), GtkResponseType::GTK_RESPONSE_OK);
                }
                return true;
            default:
                break;
        }
    }
    return false;
}

static void
on_move_change(GtkWidget* widget, const std::shared_ptr<MoveSet>& mset) noexcept
{
    g_signal_handlers_block_matched(mset->entry_ext,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);
    g_signal_handlers_block_matched(mset->buf_name,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);
    g_signal_handlers_block_matched(mset->buf_full_name,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);
    g_signal_handlers_block_matched(mset->buf_path,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);
    g_signal_handlers_block_matched(mset->buf_full_path,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_move_change,
                                    nullptr);

    // change is_dir to reflect state of new directory or link option
    if (mset->create_new != ptk::action::rename_mode::rename)
    {
        const bool new_folder =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
        const bool new_link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));

#if (GTK_MAJOR_VERSION == 4)
        const std::string text = gtk_editable_get_text(GTK_EDITABLE(mset->entry_target));
#elif (GTK_MAJOR_VERSION == 3)
        const std::string text = gtk_entry_get_text(GTK_ENTRY(mset->entry_target));
#endif

        if (new_folder ||
            (new_link && std::filesystem::is_directory(text) && text.starts_with('/')))
        {
            if (!mset->is_dir)
            {
                mset->is_dir = true;
            }
        }
        else if (mset->is_dir)
        {
            mset->is_dir = false;
        }
        if (mset->is_dir && gtk_widget_is_focus(GTK_WIDGET(mset->entry_ext)))
        {
            gtk_widget_grab_focus(GTK_WIDGET(mset->input_name));
        }
        gtk_widget_set_sensitive(GTK_WIDGET(mset->entry_ext), !mset->is_dir);
        gtk_widget_set_sensitive(GTK_WIDGET(mset->label_ext), !mset->is_dir);
    }

    std::filesystem::path full_path;
    std::filesystem::path path;
    GtkTextIter iter;
    GtkTextIter siter;

    if (widget == GTK_WIDGET(mset->buf_name) || widget == GTK_WIDGET(mset->entry_ext))
    {
        std::string full_name;

        if (widget == GTK_WIDGET(mset->buf_name))
        {
            mset->last_widget = GTK_WIDGET(mset->input_name);
        }
        else
        {
            mset->last_widget = GTK_WIDGET(mset->entry_ext);
        }

        gtk_text_buffer_get_start_iter(mset->buf_name, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_name, &iter);
        const char* name = gtk_text_buffer_get_text(mset->buf_name, &siter, &iter, false);

#if (GTK_MAJOR_VERSION == 4)
        std::string ext = gtk_editable_get_text(GTK_EDITABLE(mset->entry_ext));
#elif (GTK_MAJOR_VERSION == 3)
        std::string ext = gtk_entry_get_text(GTK_ENTRY(mset->entry_ext));
#endif

        if (ext.starts_with('.'))
        { // ignore leading dot in extension field
            ext = ztd::removeprefix(ext, ".");
        }

        // update full_name
        if (name && !ext.empty())
        {
            full_name = std::format("{}.{}", name, ext);
        }
        else if (name && ext.empty())
        {
            full_name = name;
        }
        else if (!name && !ext.empty())
        {
            full_name = ext;
        }
        else
        {
            full_name = "";
        }

        gtk_text_buffer_set_text(mset->buf_full_name, full_name.data(), -1);

        // update full_path
        gtk_text_buffer_get_start_iter(mset->buf_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_path, &iter);
        path = gtk_text_buffer_get_text(mset->buf_path, &siter, &iter, false);
        if (std::filesystem::equivalent(path, "."))
        {
            path = mset->full_path.parent_path();
        }
        else if (std::filesystem::equivalent(path, ".."))
        {
            const auto cwd = mset->full_path.parent_path();
            path = cwd.parent_path();
        }

        if (path.is_absolute())
        {
            full_path = path / full_name;
        }
        else
        {
            const auto cwd = mset->full_path.parent_path();
            full_path = cwd / path / full_name;
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

        if (mset->file && mset->file->is_directory())
        {
            gtk_text_buffer_set_text(mset->buf_name, mset->file->name().data(), -1);
#if (GTK_MAJOR_VERSION == 4)
            gtk_editable_set_text(GTK_EDITABLE(mset->entry_ext), "");
#elif (GTK_MAJOR_VERSION == 3)
            gtk_entry_set_text(GTK_ENTRY(mset->entry_ext), "");
#endif
        }
        else
        {
            const auto filename_parts = vfs::utils::split_basename_extension(full_name);
            gtk_text_buffer_set_text(mset->buf_name, filename_parts.basename.data(), -1);
#if (GTK_MAJOR_VERSION == 4)
            gtk_editable_set_text(GTK_EDITABLE(mset->entry_ext), filename_parts.extension.data());
#elif (GTK_MAJOR_VERSION == 3)
            gtk_entry_set_text(GTK_ENTRY(mset->entry_ext), filename_parts.extension.data());
#endif
        }

        // update full_path
        gtk_text_buffer_get_start_iter(mset->buf_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_path, &iter);
        path = gtk_text_buffer_get_text(mset->buf_path, &siter, &iter, false);
        if (std::filesystem::equivalent(path, "."))
        {
            path = mset->full_path.parent_path();
        }
        else if (std::filesystem::equivalent(path, ".."))
        {
            const auto cwd = mset->full_path.parent_path();
            path = cwd.parent_path();
        }

        if (path.is_absolute())
        {
            full_path = path / full_name;
        }
        else
        {
            const auto cwd = mset->full_path.parent_path();
            full_path = cwd / path / full_name;
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
        if (std::filesystem::equivalent(path, "."))
        {
            path = mset->full_path.parent_path();
        }
        else if (std::filesystem::equivalent(path, ".."))
        {
            const auto cwd = mset->full_path.parent_path();
            path = cwd.parent_path();
        }

        if (path.is_absolute())
        {
            full_path = path / full_name;
        }
        else
        {
            const auto cwd = mset->full_path.parent_path();
            full_path = cwd / path / full_name;
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
        if (full_path.empty())
        {
            full_name = "";
        }
        else
        {
            full_name = full_path.filename();
        }

        path = full_path.parent_path();
        if (std::filesystem::equivalent(path, "."))
        {
            path = mset->full_path.parent_path();
        }
        else if (std::filesystem::equivalent(path, ".."))
        {
            const auto cwd = mset->full_path.parent_path();
            path = cwd.parent_path();
        }
        else if (!path.is_absolute())
        {
            const auto cwd = mset->full_path.parent_path();
            path = cwd / path;
        }

        if (mset->file && mset->file->is_directory())
        {
            gtk_text_buffer_set_text(mset->buf_name, mset->file->name().data(), -1);
#if (GTK_MAJOR_VERSION == 4)
            gtk_editable_set_text(GTK_EDITABLE(mset->entry_ext), "");
#elif (GTK_MAJOR_VERSION == 3)
            gtk_entry_set_text(GTK_ENTRY(mset->entry_ext), "");
#endif
        }
        else
        {
            const auto filename_parts = vfs::utils::split_basename_extension(full_name);
            gtk_text_buffer_set_text(mset->buf_name, filename_parts.basename.data(), -1);
#if (GTK_MAJOR_VERSION == 4)
            gtk_editable_set_text(GTK_EDITABLE(mset->entry_ext), filename_parts.extension.data());
#elif (GTK_MAJOR_VERSION == 3)
            gtk_entry_set_text(GTK_ENTRY(mset->entry_ext), filename_parts.extension.data());
#endif

            // update full_name
            if (filename_parts.basename.empty() || filename_parts.extension.empty())
            {
                if (!filename_parts.basename.empty() && filename_parts.extension.empty())
                {
                    full_name = filename_parts.basename;
                }
                else if (filename_parts.basename.empty() && !filename_parts.extension.empty())
                {
                    full_name = filename_parts.extension;
                }
                else
                {
                    full_name = "";
                }
            }
        }

        gtk_text_buffer_set_text(mset->buf_full_name, full_name.data(), -1);

        // update path
        gtk_text_buffer_set_text(mset->buf_path, path.c_str(), -1);

        if (!full_path.is_absolute())
        {
            // update full_path for tests below
            const auto cwd = mset->full_path.parent_path();
            full_path = cwd / full_path;
        }
    }

    // change relative path to absolute
    if (!path.is_absolute())
    {
        path = full_path.parent_path();
    }

    // ztd::logger::info("path={}   full={}", path, full_path);

    // tests
    bool full_path_exists = false;
    bool full_path_exists_dir = false;
    bool full_path_same = false;
    bool path_missing = false;
    bool path_exists_file = false;
    bool is_move = false;

    std::error_code ec;
    const bool equivalent = std::filesystem::equivalent(full_path, mset->full_path, ec);
    if (equivalent && !ec)
    {
        full_path_same = true;
        if (mset->create_new != ptk::action::rename_mode::rename &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link)))
        {
            if (std::filesystem::exists(full_path))
            {
                full_path_exists = true;
                if (std::filesystem::is_directory(full_path))
                {
                    full_path_exists_dir = true;
                }
            }
        }
    }
    else
    {
        if (std::filesystem::exists(full_path))
        {
            full_path_exists = true;
            if (std::filesystem::is_directory(full_path))
            {
                full_path_exists_dir = true;
            }
        }
        else if (std::filesystem::exists(path))
        {
            if (!std::filesystem::is_directory(path))
            {
                path_exists_file = true;
            }
        }
        else
        {
            path_missing = true;
        }

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move)))
        {
            is_move = !std::filesystem::equivalent(path, mset->old_path);
        }
    }

    // clang-format off
    // ztd::logger::info("TEST")
    // ztd::logger::info( "  full_path_same {} {}", full_path_same, mset->full_path_same);
    // ztd::logger::info( "  full_path_exists {} {}", full_path_exists, mset->full_path_exists);
    // ztd::logger::info( "  full_path_exists_dir {} {}", full_path_exists_dir, mset->full_path_exists_dir);
    // ztd::logger::info( "  path_missing {} {}", path_missing, mset->path_missing);
    // ztd::logger::info( "  path_exists_file {} {}", path_exists_file, mset->path_exists_file);
    // clang-format on

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

        gtk_widget_set_sensitive(mset->revert, !full_path_same);

        if (full_path_same && (mset->create_new == ptk::action::rename_mode::rename ||
                               mset->create_new == ptk::action::rename_mode::new_link))
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

    if (is_move != mset->is_move && mset->create_new == ptk::action::rename_mode::rename)
    {
        mset->is_move = is_move;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move)))
        {
            gtk_button_set_label(GTK_BUTTON(mset->next), is_move != 0 ? "_Move" : "_Rename");
        }
    }

    if (mset->create_new != ptk::action::rename_mode::rename &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link)))
    {
#if (GTK_MAJOR_VERSION == 4)
        path = gtk_editable_get_text(GTK_EDITABLE(mset->entry_target));
#elif (GTK_MAJOR_VERSION == 3)
        path = gtk_entry_get_text(GTK_ENTRY(mset->entry_target));
#endif

        gtk_widget_set_sensitive(mset->next,
                                 (!(full_path_same && full_path_exists) && !full_path_exists_dir));
    }

    if (mset->open)
    {
        gtk_widget_set_sensitive(mset->open, gtk_widget_get_sensitive(mset->next));
    }

    g_signal_handlers_unblock_matched(mset->entry_ext,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
    g_signal_handlers_unblock_matched(mset->buf_name,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
    g_signal_handlers_unblock_matched(mset->buf_full_name,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
    g_signal_handlers_unblock_matched(mset->buf_path,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
    g_signal_handlers_unblock_matched(mset->buf_full_path,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_move_change,
                                      nullptr);
}

static void
select_input(GtkWidget* widget, const std::shared_ptr<MoveSet>& mset) noexcept
{
    if (GTK_IS_EDITABLE(widget))
    {
        gtk_editable_select_region(GTK_EDITABLE(widget), 0, -1);
    }
    else if (GTK_IS_COMBO_BOX(widget))
    {
        gtk_editable_select_region(GTK_EDITABLE(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(widget)))),
                                   0,
                                   -1);
    }
    else
    {
        GtkTextIter iter;
        GtkTextIter siter;
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
        if (widget == GTK_WIDGET(mset->input_full_name) &&
            !gtk_widget_get_visible(gtk_widget_get_parent(mset->input_name)))
        {
            // name is not visible so select name in filename
            gtk_text_buffer_get_start_iter(mset->buf_full_name, &siter);
            gtk_text_buffer_get_end_iter(mset->buf_full_name, &iter);
            const std::string full_name =
                gtk_text_buffer_get_text(mset->buf_full_name, &siter, &iter, false);

            if (mset->file && mset->file->is_directory())
            {
                gtk_text_buffer_get_iter_at_offset(buf, &iter, full_name.size());
            }
            else
            {
                const auto filename_parts = vfs::utils::split_basename_extension(full_name);
                gtk_text_buffer_get_iter_at_offset(buf, &iter, filename_parts.basename.size());
            }
        }
        else
        {
            gtk_text_buffer_get_end_iter(buf, &iter);
        }

        gtk_text_buffer_get_start_iter(buf, &siter);
        gtk_text_buffer_select_range(buf, &iter, &siter);
    }
}

static bool
on_focus(GtkWidget* widget, GtkDirectionType direction,
         const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)direction;
    select_input(widget, mset);
    return false;
}

static bool
on_button_focus(GtkWidget* widget, GtkDirectionType direction,
                const std::shared_ptr<MoveSet>& mset) noexcept
{
    if (direction == GtkDirectionType::GTK_DIR_TAB_FORWARD ||
        direction == GtkDirectionType::GTK_DIR_TAB_BACKWARD)
    {
        if (widget == mset->options || widget == mset->opt_move || widget == mset->opt_new_file)
        {
            GtkWidget* input = nullptr;
            if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_name)))
            {
                input = mset->input_name;
            }
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_name)))
            {
                input = mset->input_full_name;
            }
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_path)))
            {
                input = mset->input_path;
            }
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_path)))
            {
                input = mset->input_full_path;
            }
            else if (gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(mset->entry_target))))
            {
                input = GTK_WIDGET(mset->entry_target);
            }
            else if (gtk_widget_get_visible(
                         gtk_widget_get_parent(GTK_WIDGET(mset->combo_template))))
            {
                input = GTK_WIDGET(mset->combo_template);
            }
            else if (gtk_widget_get_visible(
                         gtk_widget_get_parent(GTK_WIDGET(mset->combo_template_dir))))
            {
                input = GTK_WIDGET(mset->combo_template_dir);
            }
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
            {
                input = mset->input_full_path;
            }
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_path)))
            {
                input = mset->input_path;
            }
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_name)))
            {
                input = mset->input_full_name;
            }
            else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_name)))
            {
                input = mset->input_name;
            }
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
on_revert_button_press(GtkWidget* widget, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)widget;
    GtkWidget* temp = mset->last_widget;
    gtk_text_buffer_set_text(mset->buf_full_path, mset->new_path.c_str(), -1);
    mset->last_widget = temp;
    select_input(mset->last_widget, mset);
    gtk_widget_grab_focus(mset->last_widget);
}

static void
on_create_browse_button_press(GtkWidget* widget, const std::shared_ptr<MoveSet>& mset) noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    (void)widget;
    (void)mset;
    ptk::dialog::error(nullptr,
                       "Needs Update",
                       "Gtk4 changed and then deprecated the GtkFileChooser API");
    return;
#elif (GTK_MAJOR_VERSION == 3)
    i32 action = 0;
    const char* title = nullptr;

    std::filesystem::path dir;
    std::filesystem::path name;

    if (widget == GTK_WIDGET(mset->browse_target))
    {
        title = "Select Link Target";
        action = GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN;

#if (GTK_MAJOR_VERSION == 4)
        const std::string text = gtk_editable_get_text(GTK_EDITABLE(mset->entry_target));
#elif (GTK_MAJOR_VERSION == 3)
        const std::string text = gtk_entry_get_text(GTK_ENTRY(mset->entry_target));
#endif

        if (text.starts_with('/'))
        {
            const auto path = std::filesystem::path(text);
            dir = path.parent_path();
            name = path.filename();
        }
        else
        {
            dir = mset->full_path.parent_path();
            if (!text.empty())
            {
                name = text;
            }
        }
    }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
    {
        title = "Select Template File";
        action = GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN;

#if (GTK_MAJOR_VERSION == 4)
        const std::string text = gtk_editable_get_text(GTK_EDITABLE(mset->combo_template));
#elif (GTK_MAJOR_VERSION == 3)
        const std::string text = gtk_entry_get_text(GTK_ENTRY(mset->combo_template));
#endif

        if (text.starts_with('/'))
        {
            const auto path = std::filesystem::path(text);
            dir = path.parent_path();
            name = path.filename();
        }
        else
        {
            const auto valid_dir = get_template_dir();
            if (valid_dir)
            {
                dir = valid_dir.value();
            }
            else
            {
                dir = mset->full_path.parent_path();
            }
            if (!text.empty())
            {
                name = text;
            }
        }
    }
    else
    {
        title = "Select Template Directory";
        action = GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

#if (GTK_MAJOR_VERSION == 4)
        const std::string text = gtk_editable_get_text(GTK_EDITABLE(mset->combo_template));
#elif (GTK_MAJOR_VERSION == 3)
        const std::string text = gtk_entry_get_text(GTK_ENTRY(mset->combo_template));
#endif

        if (text.starts_with('/'))
        {
            const auto path = std::filesystem::path(text);
            dir = path.parent_path();
            name = path.filename();
        }
        else
        {
            const auto valid_dir = get_template_dir();
            if (valid_dir)
            {
                dir = valid_dir.value();
            }
            else
            {
                dir = mset->full_path.parent_path();
            }
            if (!text.empty())
            {
                name = text;
            }
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

    ptk::utils::set_window_icon(GTK_WINDOW(dlg));

    if (name.empty())
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), dir.c_str());
    }
    else
    {
        const auto path = dir / name;
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), path.c_str());
    }

    const auto width = xset_get_int(xset::name::move_dlg_help, xset::var::x);
    const auto height = xset_get_int(xset::name::move_dlg_help, xset::var::y);
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(GTK_WIDGET(dlg));
#if (GTK_MAJOR_VERSION == 3)
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER_ALWAYS);
#endif
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
        while (g_main_context_pending(nullptr))
        {
            g_main_context_iteration(nullptr, true);
        }
#if (GTK_MAJOR_VERSION == 3)
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
#endif
    }

    const auto response = gtk_dialog_run(GTK_DIALOG(dlg));
    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        const char* new_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        const char* path = new_path;
        GtkWidget* w = nullptr;
        if (widget == GTK_WIDGET(mset->browse_target))
        {
            w = GTK_WIDGET(mset->entry_target);
        }
        else
        {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
            {
                w = gtk_bin_get_child(GTK_BIN(mset->combo_template));
            }
            else
            {
                w = gtk_bin_get_child(GTK_BIN(mset->combo_template_dir));
            }
            const auto valid_dir = get_template_dir();
            if (valid_dir)
            {
                dir = valid_dir.value();
                if (ztd::startswith(new_path, dir.string()) && ztd::endswith(new_path, "/"))
                {
                    path = new_path + dir.string().size() + 1;
                }
            }
        }
#if (GTK_MAJOR_VERSION == 4)
        gtk_editable_set_text(GTK_EDITABLE(w), path);
#elif (GTK_MAJOR_VERSION == 3)
        gtk_entry_set_text(GTK_ENTRY(w), path);
#endif
    }

    // Saving dialog dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    xset_set(xset::name::move_dlg_help, xset::var::x, std::format("{}", allocation.width));
    xset_set(xset::name::move_dlg_help, xset::var::y, std::format("{}", allocation.height));

    gtk_widget_destroy(dlg);
#endif
}

#if (GTK_MAJOR_VERSION == 3)
enum class file_misc_mode
{
    filename,
    parent,
    path
};

static void
on_browse_mode_toggled(GtkMenuItem* item, GtkWidget* dlg) noexcept
{
    (void)item;

    GtkWidget** mode = (GtkWidget**)g_object_get_data(G_OBJECT(dlg), "mode");

    static constexpr std::array<file_misc_mode, 3> misc_modes{
        file_misc_mode::filename,
        file_misc_mode::parent,
        file_misc_mode::path,
    };

    for (const auto [index, value] : std::views::enumerate(misc_modes))
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode[index])))
        {
            const GtkFileChooserAction action =
                value == file_misc_mode::parent
                    ? GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                    : GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE;
            GtkAllocation allocation;
            gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
            const auto width = allocation.width;
            const auto height = allocation.height;
            gtk_file_chooser_set_action(GTK_FILE_CHOOSER(dlg), action);
            if (width && height)
            {
#if (GTK_MAJOR_VERSION == 3)
                // under some circumstances, changing the action changes the size
                gtk_window_set_position(GTK_WINDOW(dlg),
                                        GtkWindowPosition::GTK_WIN_POS_CENTER_ALWAYS);
#endif
                gtk_window_set_default_size(GTK_WINDOW(dlg), allocation.width, allocation.height);
                while (g_main_context_pending(nullptr))
                {
                    g_main_context_iteration(nullptr, true);
                }
#if (GTK_MAJOR_VERSION == 3)
                gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
#endif
            }
            return;
        }
    }
}
#endif

static void
on_browse_button_press(GtkWidget* widget, const std::shared_ptr<MoveSet>& mset) noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    ptk::dialog::error(nullptr,
                       "Needs Update",
                       "Gtk4 changed and then deprecated the GtkFileChooser API");
    return;
#elif (GTK_MAJOR_VERSION == 3)
    (void)widget;
    GtkTextIter iter;
    GtkTextIter siter;
    file_misc_mode mode_default = file_misc_mode::parent;

    const auto set = xset::set::get(xset::name::move_dlg_help);
    if (set->z)
    {
        mode_default = file_misc_mode(xset_get_int(xset::name::move_dlg_help, xset::var::z));
    }

    // action create directory does not work properly so not used:
    //  it creates a directory by default with no way to stop it
    //  it gives 'directory already exists' error popup
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Browse",
        mset->parent ? GTK_WINDOW(mset->parent) : nullptr,
        mode_default == file_misc_mode::parent
            ? GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
            : GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE,
        "Cancel",
        GtkResponseType::GTK_RESPONSE_CANCEL,
        "OK",
        GtkResponseType::GTK_RESPONSE_OK,
        nullptr);

    gtk_text_buffer_get_start_iter(mset->buf_path, &siter);
    gtk_text_buffer_get_end_iter(mset->buf_path, &iter);
    g_autofree char* path = gtk_text_buffer_get_text(mset->buf_path, &siter, &iter, false);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), path);

    if (mode_default != file_misc_mode::parent)
    {
        gtk_text_buffer_get_start_iter(mset->buf_full_name, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_name, &iter);
        g_autofree char* filename =
            gtk_text_buffer_get_text(mset->buf_full_name, &siter, &iter, false);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), filename);
    }

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), false);

    // Mode
    static constexpr std::array<file_misc_mode, 3> misc_modes{
        file_misc_mode::filename,
        file_misc_mode::parent,
        file_misc_mode::path,
    };

    GtkWidget* mode[3];

    GtkBox* hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4));
    mode[magic_enum::enum_integer(file_misc_mode::filename)] =
        gtk_radio_button_new_with_mnemonic(nullptr, "Fil_ename");
    mode[magic_enum::enum_integer(file_misc_mode::parent)] =
        gtk_radio_button_new_with_mnemonic_from_widget(
            GTK_RADIO_BUTTON(mode[magic_enum::enum_integer(file_misc_mode::filename)]),
            "Pa_rent");
    mode[magic_enum::enum_integer(file_misc_mode::path)] =
        gtk_radio_button_new_with_mnemonic_from_widget(
            GTK_RADIO_BUTTON(mode[magic_enum::enum_integer(file_misc_mode::filename)]),
            "P_ath");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mode[magic_enum::enum_integer(mode_default)]),
                                 true);
    gtk_box_pack_start(hbox, gtk_label_new("Insert as"), false, true, 2);

    for (const auto [index, value] : std::views::enumerate(misc_modes))
    {
        gtk_widget_set_focus_on_click(GTK_WIDGET(mode[index]), false);
        g_signal_connect(G_OBJECT(mode[index]), "toggled", G_CALLBACK(on_browse_mode_toggled), dlg);
        gtk_box_pack_start(hbox, mode[index], false, true, 2);
    }
    GtkBox* content_area = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg)));
    gtk_box_pack_start(GTK_BOX(content_area), GTK_WIDGET(hbox), false, true, 6);
    g_object_set_data(G_OBJECT(dlg), "mode", mode);
    gtk_widget_show_all(GTK_WIDGET(hbox));

    const auto width = xset_get_int(xset::name::move_dlg_help, xset::var::x);
    const auto height = xset_get_int(xset::name::move_dlg_help, xset::var::y);
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(GTK_WIDGET(dlg));
#if (GTK_MAJOR_VERSION == 3)
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER_ALWAYS);
#endif
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
        while (g_main_context_pending(nullptr))
        {
            g_main_context_iteration(nullptr, true);
        }
#if (GTK_MAJOR_VERSION == 3)
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
#endif
    }

    const auto response = gtk_dialog_run(GTK_DIALOG(dlg));
    // bogus GTK warning here: Unable to retrieve the file info for...
    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        for (const auto [index, value] : std::views::enumerate(misc_modes))
        {
            if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode[index])))
            {
                continue;
            }

            switch (value)
            {
                case file_misc_mode::filename:
                {
                    g_autofree char* filename =
                        gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
                    const auto name = std::filesystem::path(filename).filename();
                    gtk_text_buffer_set_text(mset->buf_full_name, name.c_str(), -1);
                    break;
                }
                case file_misc_mode::parent:
                {
                    g_autofree char* filename =
                        gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dlg));
                    gtk_text_buffer_set_text(mset->buf_path, filename, -1);
                    break;
                }
                case file_misc_mode::path:
                {
                    g_autofree char* filename =
                        gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
                    gtk_text_buffer_set_text(mset->buf_full_path, filename, -1);
                    break;
                }
            }
            break;
        }
    }

    // Saving dialog dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    xset_set(xset::name::move_dlg_help, xset::var::x, std::format("{}", allocation.width));
    xset_set(xset::name::move_dlg_help, xset::var::y, std::format("{}", allocation.height));

    // save mode
    for (const auto [index, value] : std::views::enumerate(misc_modes))
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode[index])))
        {
            xset_set(xset::name::move_dlg_help,
                     xset::var::z,
                     std::format("{}", magic_enum::enum_integer(value)));
            break;
        }
    }

    gtk_widget_destroy(dlg);
#endif
}

static void
on_opt_toggled(GtkMenuItem* item, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)item;

    const bool move = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move));
    const bool copy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy));
    const bool link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link));
    const bool copy_target = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy_target));
    const bool link_target = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link_target));

    const bool new_file = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file));
    const bool new_folder = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
    const bool new_link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));

    std::string action;
    std::string btn_label;
    std::string desc;
    if (mset->create_new != ptk::action::rename_mode::rename)
    {
        btn_label = "Create";
        action = "Create New";
        if (new_file)
        {
            desc = "File";
        }
        else if (new_folder)
        {
            desc = "Directory";
        }
        else if (new_link)
        {
            desc = "Link";
        }
    }
    else
    {
        GtkTextIter iter;
        GtkTextIter siter;
        gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
        g_autofree char* full_path =
            gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);
        const auto new_path = std::filesystem::path(full_path).parent_path();

        const bool rename = (std::filesystem::equivalent(mset->old_path, new_path) ||
                             std::filesystem::equivalent(new_path, "."));

        if (move)
        {
            btn_label = rename ? "Rename" : "Move";
            action = "Move";
        }
        else if (copy)
        {
            btn_label = "C_opy";
            action = "Copy";
        }
        else if (link)
        {
            btn_label = "_Link";
            action = "Create Link To";
        }
        else if (copy_target)
        {
            btn_label = "C_opy";
            action = "Copy";
            desc = "Link Target";
        }
        else if (link_target)
        {
            btn_label = "_Link";
            action = "Create Link To";
            desc = "Target";
        }
    }

    // Window Icon
    std::string icon;
    if (mset->create_new != ptk::action::rename_mode::rename)
    {
        icon = "gtk-new";
    }
    else
    {
        icon = "gtk-edit";
    }
    gtk_window_set_icon_name(GTK_WINDOW(mset->dlg), icon.data());

    // title
    if (desc.empty())
    {
        desc = mset->desc;
    }
    const std::string title = std::format("{} {}", action, desc);
    gtk_window_set_title(GTK_WINDOW(mset->dlg), title.data());

    if (!btn_label.empty())
    {
        gtk_button_set_label(GTK_BUTTON(mset->next), btn_label.c_str());
    }

    mset->full_path_same = false;
    mset->mode_change = true;
    on_move_change(GTK_WIDGET(mset->buf_full_path), mset);
    if (mset->create_new != ptk::action::rename_mode::rename)
    {
        on_toggled(nullptr, mset);
    }
}

static void
on_toggled(GtkMenuItem* item, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)item;
    bool someone_is_visible = false;
    bool opts_visible = false;

    // opts
    if (xset_get_b(xset::name::move_copy) || mset->clip_copy)
    {
        gtk_widget_show(GTK_WIDGET(mset->opt_copy));
    }
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy)))
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        }
        gtk_widget_hide(GTK_WIDGET(mset->opt_copy));
    }

    if (xset_get_b(xset::name::move_link))
    {
        gtk_widget_show(GTK_WIDGET(mset->opt_link));
    }
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link)))
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        }
        gtk_widget_hide(GTK_WIDGET(mset->opt_link));
    }

    if (xset_get_b(xset::name::move_copyt) && mset->is_link)
    {
        gtk_widget_show(GTK_WIDGET(mset->opt_copy_target));
    }
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy_target)))
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        }
        gtk_widget_hide(GTK_WIDGET(mset->opt_copy_target));
    }

    if (xset_get_b(xset::name::move_linkt) && mset->is_link)
    {
        gtk_widget_show(GTK_WIDGET(mset->opt_link_target));
    }
    else
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link_target)))
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), true);
        }
        gtk_widget_hide(GTK_WIDGET(mset->opt_link_target));
    }

    if (!gtk_widget_get_visible(mset->opt_copy) && !gtk_widget_get_visible(mset->opt_link) &&
        !gtk_widget_get_visible(mset->opt_copy_target) &&
        !gtk_widget_get_visible(mset->opt_link_target))
    {
        gtk_widget_hide(GTK_WIDGET(mset->opt_move));
        opts_visible = false;
    }
    else
    {
        gtk_widget_show(GTK_WIDGET(mset->opt_move));
        opts_visible = true;
    }

    // entries
    if (xset_get_b(xset::name::move_name))
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

    if (xset_get_b(xset::name::move_filename))
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

    if (xset_get_b(xset::name::move_parent))
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

    if (xset_get_b(xset::name::move_path))
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

    if (!mset->is_link && (mset->create_new == ptk::action::rename_mode::rename) &&
        xset_get_b(xset::name::move_type))
    {
        gtk_widget_show(GTK_WIDGET(mset->hbox_type));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->hbox_type));
    }

    bool new_file = false;
    bool new_folder = false;
    bool new_link = false;
    if (mset->create_new != ptk::action::rename_mode::rename)
    {
        new_file = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file));
        new_folder = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
        new_link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));
    }

    if (new_link || (mset->is_link && xset_get_b(xset::name::move_target)))
    {
        gtk_widget_show(GTK_WIDGET(mset->hbox_target));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->hbox_target));
    }

    if ((new_file || new_folder) && xset_get_b(xset::name::move_template))
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
        xset_set_b(xset::name::move_filename, true);
        on_toggled(nullptr, mset);
    }

    if (opts_visible)
    {
        if (gtk_widget_get_visible(GTK_WIDGET(mset->hbox_type)))
        {
        }
        else if (gtk_widget_get_visible(GTK_WIDGET(mset->label_full_path)))
        {
        }
        else if (gtk_widget_get_visible(GTK_WIDGET(mset->blank_path)))
        {
            gtk_widget_hide(GTK_WIDGET(mset->blank_path));
        }
        else if (gtk_widget_get_visible(GTK_WIDGET(mset->blank_full_name)))
        {
            gtk_widget_hide(GTK_WIDGET(mset->blank_full_name));
        }
        else if (gtk_widget_get_visible(GTK_WIDGET(mset->blank_name)))
        {
            gtk_widget_hide(GTK_WIDGET(mset->blank_name));
        }
    }
}

static bool
on_mnemonic_activate(GtkWidget* widget, bool arg1, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)arg1;
    select_input(widget, mset);
    return false;
}

static void
on_options_button_press(GtkWidget* btn, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)btn;
    GtkWidget* popup = gtk_menu_new();

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

    {
        const auto set = xset::set::get(xset::name::move_name);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_filename);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_parent);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_path);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_type);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        set->disable = (mset->create_new != ptk::action::rename_mode::rename || mset->is_link);
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_target);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        set->disable = mset->create_new != ptk::action::rename_mode::rename || !mset->is_link;
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_template);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        set->disable = mset->create_new == ptk::action::rename_mode::rename;
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_copy);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        set->disable = mset->clip_copy || mset->create_new != ptk::action::rename_mode::rename;
    }

    {
        const auto set = xset::set::get(xset::name::move_link);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        set->disable = mset->create_new != ptk::action::rename_mode::rename;
    }

    {
        const auto set = xset::set::get(xset::name::move_copyt);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        set->disable = !mset->is_link;
    }

    {
        const auto set = xset::set::get(xset::name::move_linkt);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        set->disable = !mset->is_link;
    }

    {
        const auto set = xset::set::get(xset::name::move_option);
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::separator);
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_dlg_confirm_create);
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::separator);
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    gtk_widget_show_all(GTK_WIDGET(popup));
    g_signal_connect(G_OBJECT(popup), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
}

static bool
on_label_focus(GtkWidget* widget, GtkDirectionType direction,
               const std::shared_ptr<MoveSet>& mset) noexcept
{
    GtkWidget* input = nullptr;
    GtkWidget* input2 = nullptr;
    GtkWidget* first_input = nullptr;

    switch (direction)
    {
        case GtkDirectionType::GTK_DIR_TAB_FORWARD:
            if (widget == GTK_WIDGET(mset->label_name))
            {
                input = mset->input_name;
            }
            else if (widget == GTK_WIDGET(mset->label_ext))
            {
                input = GTK_WIDGET(mset->entry_ext);
            }
            else if (widget == GTK_WIDGET(mset->label_full_name))
            {
                input = mset->input_full_name;
            }
            else if (widget == GTK_WIDGET(mset->label_path))
            {
                input = mset->input_path;
            }
            else if (widget == GTK_WIDGET(mset->label_full_path))
            {
                input = mset->input_full_path;
            }
            else if (widget == GTK_WIDGET(mset->label_type))
            {
                on_button_focus(mset->options, GtkDirectionType::GTK_DIR_TAB_FORWARD, mset);
                return true;
            }
            else if (widget == GTK_WIDGET(mset->label_target))
            {
                input = GTK_WIDGET(mset->entry_target);
            }
            else if (widget == GTK_WIDGET(mset->label_template))
            {
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
                {
                    input = GTK_WIDGET(mset->combo_template);
                }
                else
                {
                    input = GTK_WIDGET(mset->combo_template_dir);
                }
            }
            break;
        case GtkDirectionType::GTK_DIR_TAB_BACKWARD:
            if (widget == GTK_WIDGET(mset->label_name))
            {
                if (mset->combo_template_dir)
                {
                    input = GTK_WIDGET(mset->combo_template_dir);
                }
                else if (mset->combo_template)
                {
                    input = GTK_WIDGET(mset->combo_template);
                }
                else if (mset->entry_target)
                {
                    input = GTK_WIDGET(mset->entry_target);
                }
                else
                {
                    input = mset->input_full_path;
                }
            }
            else if (widget == GTK_WIDGET(mset->label_ext))
            {
                input = mset->input_name;
            }
            else if (widget == GTK_WIDGET(mset->label_full_name))
            {
                if (gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(mset->entry_ext))) &&
                    gtk_widget_get_sensitive(GTK_WIDGET(mset->entry_ext)))
                {
                    input = GTK_WIDGET(mset->entry_ext);
                }
                else
                {
                    input = mset->input_name;
                }
            }
            else if (widget == GTK_WIDGET(mset->label_path))
            {
                input = mset->input_full_name;
            }
            else if (widget == GTK_WIDGET(mset->label_full_path))
            {
                input = mset->input_path;
            }
            else
            {
                input = mset->input_full_path;
            }

            first_input = input;
            while (input && !gtk_widget_get_visible(gtk_widget_get_parent(input)))
            {
                input2 = nullptr;
                if (input == GTK_WIDGET(mset->combo_template_dir))
                {
                    if (mset->combo_template)
                    {
                        input2 = GTK_WIDGET(mset->combo_template);
                    }
                    else if (mset->entry_target)
                    {
                        input2 = GTK_WIDGET(mset->entry_target);
                    }
                    else
                    {
                        input2 = mset->input_full_path;
                    }
                }
                else if (input == GTK_WIDGET(mset->combo_template))
                {
                    if (mset->entry_target)
                    {
                        input2 = GTK_WIDGET(mset->entry_target);
                    }
                    else
                    {
                        input2 = mset->input_full_path;
                    }
                }
                else if (input == GTK_WIDGET(mset->entry_target))
                {
                    input2 = mset->input_full_path;
                }
                else if (input == mset->input_full_path)
                {
                    input2 = mset->input_path;
                }
                else if (input == mset->input_path)
                {
                    input2 = mset->input_full_name;
                }
                else if (input == mset->input_full_name)
                {
                    if (gtk_widget_get_visible(
                            gtk_widget_get_parent(GTK_WIDGET(mset->entry_ext))) &&
                        gtk_widget_get_sensitive(GTK_WIDGET(mset->entry_ext)))
                    {
                        input2 = GTK_WIDGET(mset->entry_ext);
                    }
                    else
                    {
                        input2 = mset->input_name;
                    }
                }
                else if (input == GTK_WIDGET(mset->entry_ext))
                {
                    input2 = mset->input_name;
                }
                else if (input == mset->input_name)
                {
                    if (mset->combo_template_dir)
                    {
                        input2 = GTK_WIDGET(mset->combo_template_dir);
                    }
                    else if (mset->combo_template)
                    {
                        input2 = GTK_WIDGET(mset->combo_template);
                    }
                    else if (mset->entry_target)
                    {
                        input2 = GTK_WIDGET(mset->entry_target);
                    }
                    else
                    {
                        input2 = mset->input_full_path;
                    }
                }

                if (input2 == first_input)
                {
                    input = nullptr;
                }
                else
                {
                    input = input2;
                }
            }
            break;
        case GtkDirectionType::GTK_DIR_UP:
        case GtkDirectionType::GTK_DIR_DOWN:
        case GtkDirectionType::GTK_DIR_LEFT:
        case GtkDirectionType::GTK_DIR_RIGHT:
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
copy_entry_to_clipboard(GtkWidget* widget, const std::shared_ptr<MoveSet>& mset) noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    ztd::logger::debug("TODO - PORT - GdkClipboard");
#elif (GTK_MAJOR_VERSION == 3)
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTextBuffer* buf = nullptr;

    if (widget == GTK_WIDGET(mset->label_name))
    {
        buf = mset->buf_name;
    }
    else if (widget == GTK_WIDGET(mset->label_ext))
    {
#if (GTK_MAJOR_VERSION == 4)
        const std::string text = gtk_editable_get_text(GTK_EDITABLE(mset->entry_ext));
#elif (GTK_MAJOR_VERSION == 3)
        const std::string text = gtk_entry_get_text(GTK_ENTRY(mset->entry_ext));
#endif

        gtk_clipboard_set_text(clip, text.data(), -1);
        return;
    }
    else if (widget == GTK_WIDGET(mset->label_full_name))
    {
        buf = mset->buf_full_name;
    }
    else if (widget == GTK_WIDGET(mset->label_path))
    {
        buf = mset->buf_path;
    }
    else if (widget == GTK_WIDGET(mset->label_full_path))
    {
        buf = mset->buf_full_path;
    }
    else if (widget == GTK_WIDGET(mset->label_type))
    {
        gtk_clipboard_set_text(clip, mset->mime_type.data(), -1);
        return;
    }
    else if (widget == GTK_WIDGET(mset->label_target))
    {
#if (GTK_MAJOR_VERSION == 4)
        const std::string text = gtk_editable_get_text(GTK_EDITABLE(mset->entry_target));
#elif (GTK_MAJOR_VERSION == 3)
        const std::string text = gtk_entry_get_text(GTK_ENTRY(mset->entry_target));
#endif

        gtk_clipboard_set_text(clip, text.data(), -1);
        return;
    }
    else if (widget == GTK_WIDGET(mset->label_template))
    {
        GtkWidget* w = nullptr;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
        {
            w = gtk_bin_get_child(GTK_BIN(mset->combo_template));
        }
        else
        {
            w = gtk_bin_get_child(GTK_BIN(mset->combo_template_dir));
        }
#if (GTK_MAJOR_VERSION == 4)
        const std::string text = gtk_editable_get_text(GTK_EDITABLE(w));
#elif (GTK_MAJOR_VERSION == 3)
        const std::string text = gtk_entry_get_text(GTK_ENTRY(w));
#endif

        gtk_clipboard_set_text(clip, text.data(), -1);
    }

    if (!buf)
    {
        return;
    }

    GtkTextIter iter;
    GtkTextIter siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    g_autofree char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    gtk_clipboard_set_text(clip, text, -1);
#endif
}

static bool
on_label_button_press(GtkWidget* widget, GdkEvent* event,
                      const std::shared_ptr<MoveSet>& mset) noexcept
{
    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);

    if (type == GdkEventType::GDK_BUTTON_PRESS)
    {
        if (button == GDK_BUTTON_PRIMARY || button == GDK_BUTTON_MIDDLE)
        {
            GtkWidget* input = nullptr;
            if (widget == GTK_WIDGET(mset->label_name))
            {
                input = mset->input_name;
            }
            else if (widget == GTK_WIDGET(mset->label_ext))
            {
                input = GTK_WIDGET(mset->entry_ext);
            }
            else if (widget == GTK_WIDGET(mset->label_full_name))
            {
                input = mset->input_full_name;
            }
            else if (widget == GTK_WIDGET(mset->label_path))
            {
                input = mset->input_path;
            }
            else if (widget == GTK_WIDGET(mset->label_full_path))
            {
                input = mset->input_full_path;
            }
            else if (widget == GTK_WIDGET(mset->label_type))
            {
                gtk_label_select_region(mset->label_mime, 0, -1);
                gtk_widget_grab_focus(GTK_WIDGET(mset->label_mime));
                if (button == GDK_BUTTON_MIDDLE)
                {
                    copy_entry_to_clipboard(widget, mset);
                }
                return true;
            }
            else if (widget == GTK_WIDGET(mset->label_target))
            {
                input = GTK_WIDGET(mset->entry_target);
            }
            else if (widget == GTK_WIDGET(mset->label_template))
            {
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
                {
                    input = GTK_WIDGET(mset->combo_template);
                }
                else
                {
                    input = GTK_WIDGET(mset->combo_template_dir);
                }
            }

            if (input)
            {
                select_input(input, mset);
                gtk_widget_grab_focus(input);
                if (button == GDK_BUTTON_MIDDLE)
                {
                    copy_entry_to_clipboard(widget, mset);
                }
            }
        }
    }
    else if (type == GdkEventType::GDK_2BUTTON_PRESS)
    {
        copy_entry_to_clipboard(widget, mset);
    }

    return true;
}

static const std::filesystem::path
get_unique_name(const std::filesystem::path& dir, const std::string_view ext = "") noexcept
{
    const std::string base = "new";

    std::filesystem::path path;

    if (ext.empty())
    {
        path = dir / base;
    }
    else
    {
        const std::string name = std::format("{}.{}", base, ext);
        path = dir / name;
    }

    u32 n = 1;
    while (std::filesystem::exists(path))
    { // need to see broken symlinks
        std::string name;
        if (ext.empty())
        {
            name = std::format("{}{}", base, ++n);
        }
        else
        {
            name = std::format("{}{}.{}", base, ++n, ext);
        }

        path = dir / name;
    }

    return path;
}

static const std::optional<std::filesystem::path>
get_template_dir() noexcept
{
    const auto templates_path = vfs::user::templates();

    std::error_code ec;
    const bool equivalent = std::filesystem::equivalent(vfs::user::home(), templates_path, ec);
    if (ec || equivalent)
    {
        /* If $XDG_TEMPLATES_DIR == $HOME this means it is disabled. Do not
         * recurse it as this is too many files/directories and may slow
         * dialog open and cause filesystem find loops.
         * https://wiki.freedesktop.org/www/Software/xdg-user-dirs/ */
        return std::nullopt;
    }

    return templates_path;
}

static const std::vector<std::filesystem::path>
get_templates(const std::filesystem::path& templates_dir, const std::filesystem::path& subdir,
              bool getdir) noexcept
{
    std::vector<std::filesystem::path> templates;

    const auto templates_path = templates_dir / subdir;

    if (!std::filesystem::is_directory(templates_path))
    {
        return templates;
    }

    for (const auto& file : std::filesystem::directory_iterator(templates_path))
    {
        const auto filename = file.path().filename();
        const auto path = templates_path / filename;
        if (getdir)
        {
            if (std::filesystem::is_directory(path))
            {
                std::filesystem::path subsubdir;
                if (subdir.empty())
                {
                    subsubdir = filename;
                }
                else
                {
                    subsubdir = subdir / filename;
                }

                templates.push_back(subsubdir);

                // prevent filesystem loops during recursive find
                if (!std::filesystem::is_symlink(path))
                {
                    const std::vector<std::filesystem::path> subsubdir_templates =
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
                    templates.push_back(filename);
                }
                else
                {
                    templates.push_back(subdir / filename);
                }
            }
            else if (std::filesystem::is_directory(path) &&
                     // prevent filesystem loops during recursive find
                     !std::filesystem::is_symlink(path))
            {
                if (subdir.empty())
                {
                    const std::vector<std::filesystem::path> subsubdir_templates =
                        get_templates(templates_dir, filename, getdir);

                    templates = ztd::merge(templates, subsubdir_templates);
                }
                else
                {
                    const auto subsubdir = subdir / filename;
                    const std::vector<std::filesystem::path> subsubdir_templates =
                        get_templates(templates_dir, subsubdir, getdir);

                    templates = ztd::merge(templates, subsubdir_templates);
                }
            }
        }
    }

    return templates;
}

static void
on_template_changed(GtkWidget* widget, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)widget;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file)))
    {
        return;
    }

    GtkEntry* entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(mset->combo_template)));

#if (GTK_MAJOR_VERSION == 4)
    const std::string text = gtk_editable_get_text(GTK_EDITABLE(entry));
#elif (GTK_MAJOR_VERSION == 3)
    const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));
#endif

    std::string ext;
    if (!text.empty())
    {
        // ext = ztd::strip(text);
        ext = ztd::rpartition(ext, "/")[2];
        if (ext.contains('.'))
        {
            ext = ztd::rpartition(ext, ".")[2];
        }
        else
        {
            ext = "";
        }
    }
#if (GTK_MAJOR_VERSION == 4)
    gtk_editable_set_text(GTK_EDITABLE(mset->entry_ext), ext.data());
#elif (GTK_MAJOR_VERSION == 3)
    gtk_entry_set_text(GTK_ENTRY(mset->entry_ext), ext.data());
#endif

    // need new name due to extension added?
    GtkTextIter iter;
    GtkTextIter siter;
    gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
    gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
    const char* full_path = gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);

    // need to see broken symlinks
    if (std::filesystem::exists(full_path))
    {
        const auto dir = std::filesystem::path(full_path).parent_path();
        const auto unique_path = get_unique_name(dir, ext);

        gtk_text_buffer_set_text(mset->buf_full_path, unique_path.c_str(), -1);
    }
}

i32
ptk::action::rename_files(ptk::browser* browser, const std::filesystem::path& file_dir,
                          const std::shared_ptr<vfs::file>& file, const char* dest_dir,
                          bool clip_copy, ptk::action::rename_mode create_new,
                          AutoOpenCreate* auto_open) noexcept
{
    GtkWidget* task_view = nullptr;
    i32 ret = 1;
    bool target_missing = false;

    if (file_dir.empty() || !std::filesystem::exists(file_dir))
    {
        return 0;
    }

    const auto mset = std::make_shared<MoveSet>(file);

    if (create_new == ptk::action::rename_mode::rename)
    {
        const std::string_view original_filename = file->name();

        mset->is_dir = file->is_directory();
        mset->is_link = file->is_symlink();
        mset->clip_copy = clip_copy;
        mset->full_path = file_dir / original_filename;
        if (dest_dir)
        {
            mset->new_path = std::filesystem::path() / dest_dir / original_filename;
        }
        else
        {
            mset->new_path = mset->full_path;
        }
    }
    else if (create_new == ptk::action::rename_mode::new_link && file)
    {
        const auto full_name = file->name();
        mset->full_path = file_dir / full_name;
        mset->new_path = mset->full_path;
        mset->is_dir = file->is_directory(); // is_dir is dynamic for create
        mset->is_link = file->is_symlink();
        mset->clip_copy = false;
    }
    else
    {
        mset->full_path = get_unique_name(file_dir);
        mset->new_path = mset->full_path;
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
    if (mset->is_link)
    {
        mset->desc = "Link";
    }
    else if (mset->is_dir)
    {
        mset->desc = "Directory";
    }
    else
    {
        mset->desc = "File";
    }

    mset->browser = browser;

    if (browser)
    {
#if (GTK_MAJOR_VERSION == 4)
        mset->parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(browser)));
#elif (GTK_MAJOR_VERSION == 3)
        mset->parent = gtk_widget_get_toplevel(GTK_WIDGET(browser));
#endif
        task_view = browser->task_view();
    }

    mset->dlg =
        gtk_dialog_new_with_buttons("Move",
                                    mset->parent ? GTK_WINDOW(mset->parent) : nullptr,
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    nullptr,
                                    nullptr);

    // Buttons
    mset->options = gtk_button_new_with_mnemonic("Opt_ions");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->options,
                                 GtkResponseType::GTK_RESPONSE_YES);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->options), false);
    // clang-format off
    g_signal_connect(G_OBJECT(mset->options), "clicked", G_CALLBACK(on_options_button_press), mset.get());
    // clang-format on

    mset->browse = gtk_button_new_with_mnemonic("_Browse");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->browse,
                                 GtkResponseType::GTK_RESPONSE_YES);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->browse), false);
    // clang-format off
    g_signal_connect(G_OBJECT(mset->browse), "clicked", G_CALLBACK(on_browse_button_press), mset.get());
    // clang-format on

    mset->revert = gtk_button_new_with_mnemonic("Re_vert");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->revert,
                                 GtkResponseType::GTK_RESPONSE_NO);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->revert), false);
    // clang-format off
    g_signal_connect(G_OBJECT(mset->revert), "clicked", G_CALLBACK(on_revert_button_press), mset.get());
    // clang-format on

    mset->cancel = gtk_button_new_with_mnemonic("Cancel");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->cancel,
                                 GtkResponseType::GTK_RESPONSE_CANCEL);

    mset->next = gtk_button_new_with_mnemonic("OK");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->next,
                                 GtkResponseType::GTK_RESPONSE_OK);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->next), false);
    gtk_button_set_label(GTK_BUTTON(mset->next), "_Rename");

    if (create_new != ptk::action::rename_mode::rename && auto_open)
    {
        mset->open = gtk_button_new_with_mnemonic("_Open");
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
#if (GTK_MAJOR_VERSION == 3)
    gtk_window_set_type_hint(GTK_WINDOW(mset->dlg), GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
#endif
    gtk_widget_show_all(GTK_WIDGET(mset->dlg));

    // Entries

    // Type
    std::string type;
    mset->label_type = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_type, "<b>Type:</b>");
    if (mset->is_link)
    {
        try
        {
            const auto target_path = std::filesystem::read_symlink(mset->full_path);

            mset->mime_type = target_path;
            if (std::filesystem::exists(target_path))
            {
                type = std::format("Link-> {}", target_path.string());
            }
            else
            {
                type = std::format("!Link-> {} (missing)", target_path.string());
                target_missing = true;
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            mset->mime_type = "inode/symlink";
            type = "symbolic link ( inode/symlink )";
        }
    }
    else if (file)
    {
        const auto mime_type = file->mime_type();
        mset->mime_type = mime_type->type();
        type = std::format(" {} ( {} )", mime_type->description(), mset->mime_type);
    }
    else // create
    {
        mset->mime_type = "?";
        type = mset->mime_type;
    }
    mset->label_mime = GTK_LABEL(gtk_label_new(type.data()));
    gtk_label_set_ellipsize(mset->label_mime, PANGO_ELLIPSIZE_MIDDLE);

    gtk_label_set_selectable(mset->label_mime, true);
    gtk_widget_set_halign(GTK_WIDGET(mset->label_mime), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_mime), GtkAlign::GTK_ALIGN_START);

    gtk_label_set_selectable(mset->label_type, true);
    // clang-format off
    g_signal_connect(G_OBJECT(mset->label_type), "button-press-event", G_CALLBACK(on_label_button_press), mset.get());
    g_signal_connect(G_OBJECT(mset->label_type), "focus", G_CALLBACK(on_label_focus), mset.get());
    // clang-format on

    // Target
    if (mset->is_link || create_new != ptk::action::rename_mode::rename)
    {
        mset->label_target = GTK_LABEL(gtk_label_new(nullptr));
        gtk_label_set_markup_with_mnemonic(mset->label_target, "<b>_Target:</b>");
        gtk_widget_set_halign(GTK_WIDGET(mset->label_target), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(mset->label_target), GtkAlign::GTK_ALIGN_END);
        mset->entry_target = GTK_ENTRY(gtk_entry_new());
        gtk_label_set_mnemonic_widget(mset->label_target, GTK_WIDGET(mset->entry_target));
        gtk_label_set_selectable(mset->label_target, true);
        // clang-format off
        g_signal_connect(G_OBJECT(mset->entry_target), "mnemonic-activate", G_CALLBACK(on_mnemonic_activate), mset.get());
        g_signal_connect(G_OBJECT(mset->entry_target), "key-press-event", G_CALLBACK(on_move_entry_keypress), mset.get());
        g_signal_connect(G_OBJECT(mset->label_target), "button-press-event", G_CALLBACK(on_label_button_press), mset.get());
        g_signal_connect(G_OBJECT(mset->label_target), "focus", G_CALLBACK(on_label_focus), mset.get());
        // clang-format on

        if (create_new != ptk::action::rename_mode::rename)
        {
            // Target Browse button
            mset->browse_target = gtk_button_new();
            gtk_widget_set_focus_on_click(GTK_WIDGET(mset->browse_target), false);
            if (!mset->new_path.empty() && file)
            {
#if (GTK_MAJOR_VERSION == 4)
                gtk_editable_set_text(GTK_EDITABLE(mset->entry_target),
                                      valumset->new_pathe.c_str());
#elif (GTK_MAJOR_VERSION == 3)
                gtk_entry_set_text(GTK_ENTRY(mset->entry_target), mset->new_path.c_str());
#endif
            }
            // clang-format off
            g_signal_connect(G_OBJECT(mset->browse_target), "clicked", G_CALLBACK(on_create_browse_button_press), mset.get());
            // clang-format on
        }
        else
        {
#if (GTK_MAJOR_VERSION == 4)
            gtk_editable_set_text(GTK_EDITABLE(mset->entry_target), mset->mime_type.data());
#elif (GTK_MAJOR_VERSION == 3)
            gtk_entry_set_text(GTK_ENTRY(mset->entry_target), mset->mime_type.data());
#endif
            gtk_editable_set_editable(GTK_EDITABLE(mset->entry_target), false);
            mset->browse_target = nullptr;
        }
        // clang-format off
        g_signal_connect(G_OBJECT(mset->entry_target), "changed", G_CALLBACK(on_move_change), mset.get());
        // clang-format on
    }
    else
    {
        mset->label_target = nullptr;
    }

    // Template
    if (create_new != ptk::action::rename_mode::rename)
    {
        mset->label_template = GTK_LABEL(gtk_label_new(nullptr));
        gtk_label_set_markup_with_mnemonic(mset->label_template, "<b>_Template:</b>");
        gtk_widget_set_halign(GTK_WIDGET(mset->label_template), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(mset->label_template), GtkAlign::GTK_ALIGN_END);
        gtk_label_set_selectable(mset->label_template, true);

        // clang-format off
        g_signal_connect(G_OBJECT(mset->entry_target), "mnemonic-activate", G_CALLBACK(on_mnemonic_activate), mset.get());
        g_signal_connect(G_OBJECT(mset->label_template), "button-press-event", G_CALLBACK(on_label_button_press), mset.get());
        g_signal_connect(G_OBJECT(mset->label_template), "focus", G_CALLBACK(on_label_focus), mset.get());
        // clang-format on

        // template combo
        mset->combo_template = GTK_COMBO_BOX(gtk_combo_box_text_new_with_entry());
        gtk_widget_set_focus_on_click(GTK_WIDGET(mset->combo_template), false);

        // add entries
        std::vector<std::filesystem::path> templates;

        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template), "Empty File");
        templates = get_templates(get_template_dir().value_or(""), "", false);
        if (!templates.empty())
        {
            std::ranges::sort(templates);
            for (const auto& t : templates)
            {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template), t.c_str());
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(mset->combo_template), 0);
        // clang-format off
        g_signal_connect(G_OBJECT(mset->combo_template), "changed", G_CALLBACK(on_template_changed), mset.get());
        g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN(mset->combo_template))), "key-press-event", G_CALLBACK(on_move_entry_keypress), mset.get());
        // clang-format on

        // template_dir combo
        mset->combo_template_dir = GTK_COMBO_BOX(gtk_combo_box_text_new_with_entry());
        gtk_widget_set_focus_on_click(GTK_WIDGET(mset->combo_template_dir), false);

        // add entries
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template_dir),
                                       "Empty Directory");
        templates.clear();
        templates = get_templates(get_template_dir().value_or(""), "", true);
        if (!templates.empty())
        {
            std::ranges::sort(templates);
            for (const auto& t : templates)
            {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mset->combo_template_dir),
                                               t.c_str());
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(mset->combo_template_dir), 0);
        // clang-format off
        g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN(mset->combo_template_dir))), "key-press-event", G_CALLBACK(on_move_entry_keypress), mset.get());
        // clang-format on

        // Template Browse button
        mset->browse_template = gtk_button_new();
        gtk_widget_set_focus_on_click(GTK_WIDGET(mset->browse_template), false);
        // clang-format off
        g_signal_connect(G_OBJECT(mset->browse_template), "clicked", G_CALLBACK(on_create_browse_button_press), mset.get());
        // clang-format on
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
    mset->scroll_name = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    mset->input_name = GTK_WIDGET(multi_input_new(mset->scroll_name, nullptr));
    gtk_label_set_mnemonic_widget(mset->label_name, mset->input_name);
    gtk_label_set_selectable(mset->label_name, true);
    mset->buf_name = GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_name)));
    mset->blank_name = GTK_LABEL(gtk_label_new(nullptr));

    // clang-format off
    g_signal_connect(G_OBJECT(mset->input_name), "key-press-event", G_CALLBACK(on_move_keypress), mset.get());
    g_signal_connect(G_OBJECT(mset->input_name), "mnemonic-activate", G_CALLBACK(on_mnemonic_activate), mset.get());
    g_signal_connect(G_OBJECT(mset->label_name), "button-press-event", G_CALLBACK(on_label_button_press), mset.get());
    g_signal_connect(G_OBJECT(mset->label_name), "focus", G_CALLBACK(on_label_focus), mset.get());
    g_signal_connect(G_OBJECT(mset->buf_name), "changed", G_CALLBACK(on_move_change), mset.get());
    g_signal_connect(G_OBJECT(mset->input_name), "focus", G_CALLBACK(on_focus), mset.get());
    // clang-format on

    // Ext
    mset->label_ext = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_ext, "<b>E_xtension:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_ext), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_ext), GtkAlign::GTK_ALIGN_END);
    mset->entry_ext = GTK_ENTRY(gtk_entry_new());
    gtk_label_set_mnemonic_widget(mset->label_ext, GTK_WIDGET(mset->entry_ext));
    gtk_label_set_selectable(mset->label_ext, true);
    gtk_widget_set_sensitive(GTK_WIDGET(mset->entry_ext), !mset->is_dir);
    gtk_widget_set_sensitive(GTK_WIDGET(mset->label_ext), !mset->is_dir);

    // clang-format off
    g_signal_connect(G_OBJECT(mset->entry_ext), "mnemonic-activate", G_CALLBACK(on_mnemonic_activate), mset.get());
    g_signal_connect(G_OBJECT(mset->label_ext), "button-press-event", G_CALLBACK(on_label_button_press), mset.get());
    g_signal_connect(G_OBJECT(mset->label_ext), "focus", G_CALLBACK(on_label_focus), mset.get());
    g_signal_connect(G_OBJECT(mset->entry_ext), "key-press-event", G_CALLBACK(on_move_entry_keypress), mset.get());
    g_signal_connect(G_OBJECT(mset->entry_ext), "changed", G_CALLBACK(on_move_change), mset.get());
    g_signal_connect_after(G_OBJECT(mset->entry_ext), "focus", G_CALLBACK(on_focus), mset.get());
    // clang-format on

    // Filename
    mset->label_full_name = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_full_name, "<b>_Filename:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_full_name), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_full_name), GtkAlign::GTK_ALIGN_START);
    mset->scroll_full_name = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    mset->input_full_name = GTK_WIDGET(multi_input_new(mset->scroll_full_name, nullptr));
    gtk_label_set_mnemonic_widget(mset->label_full_name, mset->input_full_name);
    gtk_label_set_selectable(mset->label_full_name, true);
    mset->buf_full_name = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_full_name));
    mset->blank_full_name = GTK_LABEL(gtk_label_new(nullptr));

    // clang-format off
    g_signal_connect(G_OBJECT(mset->input_full_name), "mnemonic-activate", G_CALLBACK(on_mnemonic_activate), mset.get());
    g_signal_connect(G_OBJECT(mset->label_full_name), "button-press-event", G_CALLBACK(on_label_button_press), mset.get());
    g_signal_connect(G_OBJECT(mset->label_full_name), "focus", G_CALLBACK(on_label_focus), mset.get());
    g_signal_connect(G_OBJECT(mset->input_full_name), "key-press-event", G_CALLBACK(on_move_keypress), mset.get());
    g_signal_connect(G_OBJECT(mset->buf_full_name), "changed", G_CALLBACK(on_move_change), mset.get());
    g_signal_connect(G_OBJECT(mset->input_full_name), "focus", G_CALLBACK(on_focus), mset.get());
    // clang-format on

    // Parent
    mset->label_path = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_path, "<b>_Parent:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_path), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_path), GtkAlign::GTK_ALIGN_START);
    mset->scroll_path = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    mset->input_path = GTK_WIDGET(multi_input_new(mset->scroll_path, nullptr));
    gtk_label_set_mnemonic_widget(mset->label_path, mset->input_path);
    gtk_label_set_selectable(mset->label_path, true);
    mset->buf_path = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_path));
    mset->blank_path = GTK_LABEL(gtk_label_new(nullptr));

    // clang-format off
    g_signal_connect(G_OBJECT(mset->input_path), "mnemonic-activate", G_CALLBACK(on_mnemonic_activate), mset.get());
    g_signal_connect(G_OBJECT(mset->label_path), "button-press-event", G_CALLBACK(on_label_button_press), mset.get());
    g_signal_connect(G_OBJECT(mset->label_path), "focus", G_CALLBACK(on_label_focus), mset.get());
    g_signal_connect(G_OBJECT(mset->input_path), "key-press-event", G_CALLBACK(on_move_keypress), mset.get());
    g_signal_connect(G_OBJECT(mset->buf_path), "changed", G_CALLBACK(on_move_change), mset.get());
    g_signal_connect(G_OBJECT(mset->input_path), "focus", G_CALLBACK(on_focus), mset.get());
    // clang-format on

    // Path
    mset->label_full_path = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_full_path, "<b>P_ath:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_full_path), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_full_path), GtkAlign::GTK_ALIGN_START);
    mset->scroll_full_path = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    // set initial path
    mset->input_full_path =
        GTK_WIDGET(multi_input_new(mset->scroll_full_path, mset->new_path.c_str()));
    gtk_label_set_mnemonic_widget(mset->label_full_path, mset->input_full_path);
    gtk_label_set_selectable(mset->label_full_path, true);
    mset->buf_full_path = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_full_path));

    // clang-format off
    g_signal_connect(G_OBJECT(mset->input_full_path), "mnemonic-activate", G_CALLBACK(on_mnemonic_activate), mset.get());
    g_signal_connect(G_OBJECT(mset->label_full_path), "button-press-event", G_CALLBACK(on_label_button_press), mset.get());
    g_signal_connect(G_OBJECT(mset->label_full_path), "focus", G_CALLBACK(on_label_focus), mset.get());
    g_signal_connect(G_OBJECT(mset->input_full_path), "key-press-event", G_CALLBACK(on_move_keypress), mset.get());
    g_signal_connect(G_OBJECT(mset->buf_full_path), "changed", G_CALLBACK(on_move_change), mset.get());
    g_signal_connect(G_OBJECT(mset->input_full_path), "focus", G_CALLBACK(on_focus), mset.get());
    // clang-format on

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

    mset->opt_new_file = gtk_radio_button_new_with_mnemonic(nullptr, "Fil_e");
    mset->opt_new_folder =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_new_file),
                                                       "Dir_ectory");
    mset->opt_new_link =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_new_file),
                                                       "_Link");

    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_move), false);
    g_signal_connect(G_OBJECT(mset->opt_move), "focus", G_CALLBACK(on_button_focus), mset.get());
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_copy), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_link), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_copy_target), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_link_target), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_new_file), false);
    // clang-format off
    g_signal_connect(G_OBJECT(mset->opt_new_file), "focus", G_CALLBACK(on_button_focus), mset.get());
    // clang-format on
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_new_folder), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_new_link), false);
    gtk_widget_set_sensitive(mset->opt_copy_target, mset->is_link && !target_missing);
    gtk_widget_set_sensitive(mset->opt_link_target, mset->is_link);

    // Pack
    GtkBox* dlg_vbox = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(mset->dlg)));

    gtk_widget_set_margin_start(GTK_WIDGET(dlg_vbox), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(dlg_vbox), 10);
    gtk_widget_set_margin_top(GTK_WIDGET(dlg_vbox), 10);
    gtk_widget_set_margin_bottom(GTK_WIDGET(dlg_vbox), 10);

    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->label_name), false, true, 4);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->scroll_name), true, true, 0);

    mset->hbox_ext = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_box_pack_start(mset->hbox_ext, GTK_WIDGET(mset->label_ext), false, true, 0);
    gtk_box_pack_start(mset->hbox_ext, GTK_WIDGET(gtk_label_new(" ")), false, true, 0);
    gtk_box_pack_start(mset->hbox_ext, GTK_WIDGET(mset->entry_ext), true, true, 0);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->hbox_ext), false, true, 5);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->blank_name), false, true, 0);

    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->label_full_name), false, true, 4);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->scroll_full_name), true, true, 0);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->blank_full_name), false, true, 0);

    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->label_path), false, true, 4);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->scroll_path), true, true, 0);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->blank_path), false, true, 0);

    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->label_full_path), false, true, 4);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->scroll_full_path), true, true, 0);

    mset->hbox_type = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_box_pack_start(mset->hbox_type, GTK_WIDGET(mset->label_type), false, true, 0);
    gtk_box_pack_start(mset->hbox_type, GTK_WIDGET(mset->label_mime), true, true, 5);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->hbox_type), false, true, 5);

    mset->hbox_target = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    if (mset->label_target)
    {
        gtk_box_pack_start(mset->hbox_target, GTK_WIDGET(mset->label_target), false, true, 0);
        if (create_new == ptk::action::rename_mode::rename)
        {
            gtk_box_pack_start(mset->hbox_target, GTK_WIDGET(gtk_label_new(" ")), false, true, 0);
        }
        gtk_box_pack_start(mset->hbox_target,
                           GTK_WIDGET(mset->entry_target),
                           true,
                           true,
                           create_new != ptk::action::rename_mode::rename ? 3 : 0);
        if (mset->browse_target)
        {
            gtk_box_pack_start(mset->hbox_target, GTK_WIDGET(mset->browse_target), false, true, 0);
        }
        gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->hbox_target), false, true, 5);
    }

    mset->hbox_template = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    if (mset->label_template)
    {
        gtk_box_pack_start(mset->hbox_template, GTK_WIDGET(mset->label_template), false, true, 0);
        gtk_box_pack_start(mset->hbox_template, GTK_WIDGET(mset->combo_template), true, true, 3);
        gtk_box_pack_start(mset->hbox_template,
                           GTK_WIDGET(mset->combo_template_dir),
                           true,
                           true,
                           3);
        gtk_box_pack_start(mset->hbox_template, GTK_WIDGET(mset->browse_template), false, true, 0);
        gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->hbox_template), false, true, 5);
    }

    GtkBox* hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4));
    if (create_new != ptk::action::rename_mode::rename)
    {
        gtk_box_pack_start(hbox, GTK_WIDGET(gtk_label_new("New")), false, true, 3);
        gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_new_file), false, true, 3);
        gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_new_folder), false, true, 3);
        gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_new_link), false, true, 3);
    }
    else
    {
        gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_move), false, true, 3);
        gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_copy), false, true, 3);
        gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_link), false, true, 3);
        gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_copy_target), false, true, 3);
        gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_link_target), false, true, 3);
    }
    gtk_box_pack_start(hbox, GTK_WIDGET(gtk_label_new("  ")), false, true, 3);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(hbox), false, true, 10);

    // show
    gtk_widget_show_all(GTK_WIDGET(mset->dlg));
    on_toggled(nullptr, mset);
    if (mset->clip_copy)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_copy), true);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_move), false);
    }
    else if (create_new == ptk::action::rename_mode::new_dir)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder), true);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_file), false);
    }
    else if (create_new == ptk::action::rename_mode::new_link)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_link), true);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_file), false);
    }

    // signals
    // clang-format off
    g_signal_connect(G_OBJECT(mset->opt_move), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_copy), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_link), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_copy_target), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_link_target), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_new_file), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_new_folder), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_new_link), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    // clang-format on

    // init
    on_move_change(GTK_WIDGET(mset->buf_full_path), mset);
    on_opt_toggled(nullptr, mset);

    if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_name)))
    {
        mset->last_widget = mset->input_name;
    }
    else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_name)))
    {
        mset->last_widget = mset->input_full_name;
    }
    else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_path)))
    {
        mset->last_widget = mset->input_path;
    }
    else if (gtk_widget_get_visible(gtk_widget_get_parent(mset->input_full_path)))
    {
        mset->last_widget = mset->input_full_path;
    }

    // select last widget
    select_input(mset->last_widget, mset);
    gtk_widget_grab_focus(mset->last_widget);

    g_signal_connect(G_OBJECT(mset->options), "focus", G_CALLBACK(on_button_focus), mset.get());
    g_signal_connect(G_OBJECT(mset->next), "focus", G_CALLBACK(on_button_focus), mset.get());
    g_signal_connect(G_OBJECT(mset->cancel), "focus", G_CALLBACK(on_button_focus), mset.get());

    // run
    std::string to_path;
    std::string from_path;
    i32 response = 0;
    while ((response = gtk_dialog_run(GTK_DIALOG(mset->dlg))))
    {
        if (response == GtkResponseType::GTK_RESPONSE_OK ||
            response == GtkResponseType::GTK_RESPONSE_APPLY)
        {
            GtkTextIter iter;
            GtkTextIter siter;
            gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
            gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
            const std::string text =
                gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);
            if (text.contains("\n"))
            {
                ptk::dialog::error(GTK_WINDOW(mset->dlg), "Error", "Path contains linefeeds");
                continue;
            }
            auto full_path = std::filesystem::path(text);
            if (!full_path.is_absolute())
            {
                // update full_path to absolute
                const auto cwd = mset->full_path.parent_path();
                full_path = cwd / full_path;
            }
            // const auto full_name = full_path.filename();
            const auto path = full_path.parent_path();
            const auto old_path = mset->full_path.parent_path();
            bool overwrite = false;

            if (response == GtkResponseType::GTK_RESPONSE_APPLY)
            {
                ret = 2;
            }

            if (create_new == ptk::action::rename_mode::rename &&
                (mset->full_path_same || full_path == mset->full_path))
            {
                // not changed, proceed to next file
                break;
            }

            // determine job
            // bool move = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move));
            const bool copy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy));
            const bool link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link));
            const bool copy_target =
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_copy_target));
            const bool link_target =
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_link_target));
            const bool new_file =
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file));
            const bool new_folder =
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
            const bool new_link =
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));

            if (!std::filesystem::exists(path))
            {
                // create parent directory
                if (xset_get_b(xset::name::move_dlg_confirm_create))
                {
                    response =
                        ptk::dialog::message(GTK_WINDOW(mset->parent),
                                             GtkMessageType::GTK_MESSAGE_QUESTION,
                                             "Create Parent Directory",
                                             GtkButtonsType::GTK_BUTTONS_YES_NO,
                                             "The parent directory does not exist. Create it?");
                    if (response != GtkResponseType::GTK_RESPONSE_YES)
                    {
                        continue;
                    }
                }
                std::filesystem::create_directories(path);
                std::filesystem::permissions(path, std::filesystem::perms::owner_all);

                if (!std::filesystem::is_directory(path))
                {
                    ptk::dialog::error(
                        GTK_WINDOW(mset->dlg),
                        "Mkdir Error",
                        std::format("Error creating parent directory\n\n{}", std::strerror(errno)));
                    continue;
                }
            }
            else if (std::filesystem::exists(full_path)) // need to see broken symlinks
            {
                // overwrite
                if (std::filesystem::is_directory(full_path))
                {
                    // just in case
                    continue;
                }
                response = ptk::dialog::message(GTK_WINDOW(mset->parent),
                                                GtkMessageType::GTK_MESSAGE_WARNING,
                                                "Overwrite Existing File",
                                                GtkButtonsType::GTK_BUTTONS_YES_NO,
                                                "OVERWRITE WARNING",
                                                "The file path exists.  Overwrite existing file?");

                if (response != GtkResponseType::GTK_RESPONSE_YES)
                {
                    continue;
                }
                overwrite = true;
            }

            if (create_new != ptk::action::rename_mode::rename && new_link)
            {
                // new link task
                ptk::file_task* ptask = ptk_file_exec_new("Create Link", mset->parent, task_view);

#if (GTK_MAJOR_VERSION == 4)
                std::string entry_text = gtk_editable_get_text(GTK_EDITABLE(mset->entry_target));
#elif (GTK_MAJOR_VERSION == 3)
                std::string entry_text = gtk_entry_get_text(GTK_ENTRY(mset->entry_target));
#endif

                while (entry_text.ends_with('/'))
                {
                    if (entry_text.size() == 1)
                    {
                        break;
                    }
                    entry_text = ztd::removesuffix(entry_text, "/");
                }

                from_path = ::utils::shell_quote(entry_text);
                to_path = ::utils::shell_quote(full_path.string());

                if (overwrite)
                {
                    ptask->task->exec_command = std::format("ln -sf {} {}", from_path, to_path);
                }
                else
                {
                    ptask->task->exec_command = std::format("ln -s {} {}", from_path, to_path);
                }
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                if (auto_open)
                {
                    auto_open->path = full_path;
                    auto_open->open_file = (response == GtkResponseType::GTK_RESPONSE_APPLY);
                    ptask->complete_notify_ = auto_open->callback;
                    ptask->user_data_ = auto_open;
                }
                ptask->run();
            }
            else if (create_new != ptk::action::rename_mode::rename && new_file)
            {
                // new file task
                if (gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(mset->combo_template))))
                {
                    const std::string str = gtk_combo_box_text_get_active_text(
                        GTK_COMBO_BOX_TEXT(mset->combo_template));

                    if (str.starts_with('/'))
                    {
                        from_path = ::utils::shell_quote(str);
                    }
                    else
                    {
                        const auto template_path = get_template_dir();
                        if (template_path)
                        {
                            const auto template_file = template_path.value() / str;
                            if (!std::filesystem::is_regular_file(template_file))
                            {
                                ptk::dialog::error(GTK_WINDOW(mset->dlg),
                                                   "Template Missing",
                                                   "The specified template does not exist");
                                continue;
                            }
                            from_path = ::utils::shell_quote(template_file.string());
                        }
                    }
                }
                to_path = ::utils::shell_quote(full_path.string());
                std::string over_cmd;
                if (overwrite)
                {
                    over_cmd = std::format("rm -f {} && ", to_path);
                }

                ptk::file_task* ptask =
                    ptk_file_exec_new("Create New File", mset->parent, task_view);
                if (from_path.empty())
                {
                    ptask->task->exec_command = std::format("{}touch {}", over_cmd, to_path);
                }
                else
                {
                    ptask->task->exec_command =
                        std::format("{}cp -f {} {}", over_cmd, from_path, to_path);
                }
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                if (auto_open)
                {
                    auto_open->path = full_path;
                    auto_open->open_file = (response == GtkResponseType::GTK_RESPONSE_APPLY);
                    ptask->complete_notify_ = auto_open->callback;
                    ptask->user_data_ = auto_open;
                }
                ptask->run();
            }
            else if (create_new != ptk::action::rename_mode::rename)
            {
                // new directory task
                if (!new_folder)
                { // failsafe
                    continue;
                }
                if (gtk_widget_get_visible(
                        gtk_widget_get_parent(GTK_WIDGET(mset->combo_template_dir))))
                {
                    const std::string str = gtk_combo_box_text_get_active_text(
                        GTK_COMBO_BOX_TEXT(mset->combo_template_dir));
                    if (str.starts_with('/'))
                    {
                        from_path = ::utils::shell_quote(str);
                    }
                    else
                    {
                        const auto template_path = get_template_dir();
                        if (template_path)
                        {
                            const auto template_file = template_path.value() / str;
                            if (!std::filesystem::is_directory(template_file))
                            {
                                ptk::dialog::error(GTK_WINDOW(mset->dlg),
                                                   "Template Missing",
                                                   "The specified template does not exist");
                                continue;
                            }
                            from_path = ::utils::shell_quote(template_file.string());
                        }
                    }
                }
                to_path = ::utils::shell_quote(full_path.string());

                ptk::file_task* ptask =
                    ptk_file_exec_new("Create New Directory", mset->parent, task_view);
                if (from_path.empty())
                {
                    ptask->task->exec_command = std::format("mkdir {}", to_path);
                }
                else
                {
                    ptask->task->exec_command = std::format("cp -rL {} {}", from_path, to_path);
                }
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                if (auto_open)
                {
                    auto_open->path = full_path;
                    auto_open->open_file = (response == GtkResponseType::GTK_RESPONSE_APPLY);
                    ptask->complete_notify_ = auto_open->callback;
                    ptask->user_data_ = auto_open;
                }
                ptask->run();
            }
            else if (copy || copy_target)
            {
                // copy task
                ptk::file_task* ptask = ptk_file_exec_new("Copy", mset->parent, task_view);
                to_path = ::utils::shell_quote(full_path.string());
                if (copy || !mset->is_link)
                {
                    from_path = ::utils::shell_quote(mset->full_path.string());
                }
                else
                {
                    const auto real_path = std::filesystem::read_symlink(mset->full_path);
                    if (std::filesystem::equivalent(real_path, mset->full_path))
                    {
                        ptk::dialog::error(GTK_WINDOW(mset->dlg),
                                           "Copy Target Error",
                                           "Error determining link's target");
                        continue;
                    }
                    from_path = ::utils::shell_quote(real_path.string());
                }

                std::string over_opt;
                if (overwrite)
                {
                    over_opt = " --remove-destination";
                }

                if (mset->is_dir)
                {
                    ptask->task->exec_command = std::format("cp -Pfr {} {}", from_path, to_path);
                }
                else
                {
                    ptask->task->exec_command =
                        std::format("cp -Pf{} {} {}", over_opt, from_path, to_path);
                }
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                ptask->run();
            }
            else if (link || link_target)
            {
                // link task
                ptk::file_task* ptask = ptk_file_exec_new("Create Link", mset->parent, task_view);
                if (link || !mset->is_link)
                {
                    from_path = ::utils::shell_quote(mset->full_path.string());
                }
                else
                {
                    const auto real_path = std::filesystem::read_symlink(mset->full_path);
                    if (std::filesystem::equivalent(real_path, mset->full_path))
                    {
                        ptk::dialog::error(GTK_WINDOW(mset->dlg),
                                           "Link Target Error",
                                           "Error determining link's target");
                        continue;
                    }
                    from_path = ::utils::shell_quote(real_path.string());
                }
                to_path = ::utils::shell_quote(full_path.string());
                if (overwrite)
                {
                    ptask->task->exec_command = std::format("ln -sf {} {}", from_path, to_path);
                }
                else
                {
                    ptask->task->exec_command = std::format("ln -s {} {}", from_path, to_path);
                }
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                ptask->run();
            }
            // need move?  (do move as task in case it takes a long time)
            else if (!std::filesystem::equivalent(old_path, path))
            {
                ptk::file_task* ptask = ptk_file_exec_new("Move", mset->parent, task_view);
                from_path = ::utils::shell_quote(mset->full_path.string());
                to_path = ::utils::shell_quote(full_path.string());
                if (overwrite)
                {
                    ptask->task->exec_command = std::format("mv -f {} {}", from_path, to_path);
                }
                else
                {
                    ptask->task->exec_command = std::format("mv {} {}", from_path, to_path);
                }
                ptask->task->exec_sync = true;
                ptask->task->exec_popup = false;
                ptask->task->exec_show_output = false;
                ptask->task->exec_show_error = true;
                ptask->run();
            }
            else
            {
                // rename (does overwrite)
                std::error_code ec;
                std::filesystem::rename(mset->full_path, full_path, ec);
                if (ec)
                {
                    // Unknown error has occurred - alert user as usual
                    ptk::dialog::error(
                        GTK_WINDOW(mset->dlg),
                        "Rename Error",
                        std::format("Error renaming file\n\n{}", std::strerror(errno)));
                    continue;
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
    {
        ret = 0;
    }

    // destroy
    gtk_widget_destroy(mset->dlg);

    return ret;
}
