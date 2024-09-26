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

#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <system_error>

#include <cstring>

#include <glibmm.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "compat/gtk4-porting.hxx"

#include "utils/shell-quote.hxx"

#include "xset/xset-context-menu.hxx"
#include "xset/xset.hxx"

#include "ptk/ptk-dialog.hxx"
#include "ptk/ptk-file-action-rename.hxx"
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/utils/multi-input.hxx"
#include "ptk/utils/ptk-utils.hxx"

#include "vfs/utils/vfs-utils.hxx"
#include "vfs/vfs-file.hxx"

namespace
{
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

    GtkLabel* label_full_name{nullptr};
    GtkScrolledWindow* scroll_full_name{nullptr};
    GtkTextView* input_full_name{nullptr};
    GtkTextBuffer* buf_full_name{nullptr};

    GtkLabel* label_path{nullptr};
    GtkScrolledWindow* scroll_path{nullptr};
    GtkTextView* input_path{nullptr};
    GtkTextBuffer* buf_path{nullptr};

    GtkLabel* label_full_path{nullptr};
    GtkScrolledWindow* scroll_full_path{nullptr};
    GtkTextView* input_full_path{nullptr};
    GtkTextBuffer* buf_full_path{nullptr};

    GtkWidget* opt_move{nullptr};
    GtkWidget* opt_copy{nullptr};
    GtkWidget* opt_link{nullptr};
    GtkWidget* opt_copy_target{nullptr};
    GtkWidget* opt_link_target{nullptr};

    GtkWidget* options{nullptr};
    GtkWidget* revert{nullptr};
    GtkWidget* cancel{nullptr};
    GtkWidget* next{nullptr};

    bool full_path_exists{false};
    bool full_path_exists_dir{false};
    bool full_path_same{false};
    bool path_missing{false};
    bool path_exists_file{false};
    bool mode_change{false};
    bool is_move{false};
};
} // namespace

MoveSet::MoveSet(const std::shared_ptr<vfs::file>& file) noexcept : file(file) {}

static void on_toggled(GtkMenuItem* item, const std::shared_ptr<MoveSet>& mset) noexcept;

static bool
on_key_press(GtkWidget* widget, GdkEvent* event, const std::shared_ptr<MoveSet>& mset) noexcept
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

    std::filesystem::path full_path;
    std::filesystem::path path;
    GtkTextIter iter;
    GtkTextIter siter;

    if (widget == GTK_WIDGET(mset->buf_full_name) || widget == GTK_WIDGET(mset->buf_path))
    {
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
            path = mset->full_path.parent_path().parent_path();
        }

        if (path.is_absolute())
        {
            full_path = path / full_name;
        }
        else
        {
            full_path = mset->full_path.parent_path() / path / full_name;
        }
        gtk_text_buffer_set_text(mset->buf_full_path, full_path.c_str(), -1);
    }
    else // if (widget == GTK_WIDGET(mset->buf_full_path))
    {
        std::string full_name;

        gtk_text_buffer_get_start_iter(mset->buf_full_path, &siter);
        gtk_text_buffer_get_end_iter(mset->buf_full_path, &iter);
        full_path = gtk_text_buffer_get_text(mset->buf_full_path, &siter, &iter, false);

        // update name & ext
        if (!full_path.empty())
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
            path = mset->full_path.parent_path().parent_path();
        }
        else if (!path.is_absolute())
        {
            path = mset->full_path.parent_path() / path;
        }

        gtk_text_buffer_set_text(mset->buf_full_name, full_name.data(), -1);

        // update path
        gtk_text_buffer_set_text(mset->buf_path, path.c_str(), -1);

        if (!full_path.is_absolute())
        {
            // update full_path for tests below
            full_path = mset->full_path.parent_path() / full_path;
        }
    }

    // change relative path to absolute
    if (!path.is_absolute())
    {
        path = full_path.parent_path();
    }

    // logger::info<logger::domain::ptk>("path={}   full={}", path, full_path);

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
    // logger::info<logger::domain::ptk>("TEST")
    // logger::info<logger::domain::ptk>( "  full_path_same {} {}", full_path_same, mset->full_path_same);
    // logger::info<logger::domain::ptk>( "  full_path_exists {} {}", full_path_exists, mset->full_path_exists);
    // logger::info<logger::domain::ptk>( "  full_path_exists_dir {} {}", full_path_exists_dir, mset->full_path_exists_dir);
    // logger::info<logger::domain::ptk>( "  path_missing {} {}", path_missing, mset->path_missing);
    // logger::info<logger::domain::ptk>( "  path_exists_file {} {}", path_exists_file, mset->path_exists_file);
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

        if (full_path_same)
        {
            gtk_widget_set_sensitive(
                mset->next,
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move)));
            gtk_label_set_markup_with_mnemonic(mset->label_full_path,
                                               "<b>P_ath:</b>   <i>original</i>");
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
            gtk_label_set_markup_with_mnemonic(mset->label_full_name, "<b>_Filename:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_path,
                                               "<b>_Parent:</b>   <i>parent exists as file</i>");
        }
        else if (path_missing)
        {
            gtk_widget_set_sensitive(mset->next, true);
            gtk_label_set_markup_with_mnemonic(mset->label_full_path,
                                               "<b>P_ath:</b>   <i>* create parent</i>");
            gtk_label_set_markup_with_mnemonic(mset->label_full_name, "<b>_Filename:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_path,
                                               "<b>_Parent:</b>   <i>* create parent</i>");
        }
        else
        {
            gtk_widget_set_sensitive(mset->next, true);
            gtk_label_set_markup_with_mnemonic(mset->label_full_path, "<b>P_ath:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_full_name, "<b>_Filename:</b>");
            gtk_label_set_markup_with_mnemonic(mset->label_path, "<b>_Parent:</b>");
        }
    }

    if (is_move != mset->is_move)
    {
        mset->is_move = is_move;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_move)))
        {
            gtk_button_set_label(GTK_BUTTON(mset->next), is_move != 0 ? "_Move" : "_Rename");
        }
    }

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
    GtkTextIter iter;
    GtkTextIter siter;
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
    if (widget == GTK_WIDGET(mset->input_full_name))
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
            const auto [basename, ext, _] = vfs::utils::split_basename_extension(full_name);
            gtk_text_buffer_get_iter_at_offset(buf, &iter, basename.size());
        }
    }
    else
    {
        gtk_text_buffer_get_end_iter(buf, &iter);
    }

    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_select_range(buf, &iter, &siter);
}

static void
on_revert_button_press(GtkWidget* widget, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)widget;
    gtk_text_buffer_set_text(mset->buf_full_path, mset->new_path.c_str(), -1);
    gtk_widget_grab_focus(GTK_WIDGET(mset->input_full_name));
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

    std::string action;
    std::string btn_label;
    std::string desc;

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

    // Window Icon
    gtk_window_set_icon_name(GTK_WINDOW(mset->dlg), "gtk-edit");

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
}

static void
on_toggled(GtkMenuItem* item, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)item;
    bool someone_is_visible = false;

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
    }
    else
    {
        gtk_widget_show(GTK_WIDGET(mset->opt_move));
    }

    // entries

    if (xset_get_b(xset::name::move_filename))
    {
        someone_is_visible = true;

        gtk_widget_show(GTK_WIDGET(mset->label_full_name));
        gtk_widget_show(GTK_WIDGET(mset->scroll_full_name));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->label_full_name));
        gtk_widget_hide(GTK_WIDGET(mset->scroll_full_name));
    }

    if (xset_get_b(xset::name::move_parent))
    {
        someone_is_visible = true;

        gtk_widget_show(GTK_WIDGET(mset->label_path));
        gtk_widget_show(GTK_WIDGET(mset->scroll_path));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->label_path));
        gtk_widget_hide(GTK_WIDGET(mset->scroll_path));
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

    if (!mset->is_link && xset_get_b(xset::name::move_type))
    {
        gtk_widget_show(GTK_WIDGET(mset->hbox_type));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->hbox_type));
    }

    if (mset->is_link && xset_get_b(xset::name::move_target))
    {
        gtk_widget_show(GTK_WIDGET(mset->hbox_target));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(mset->hbox_target));
    }

    if (!someone_is_visible)
    {
        xset_set_b(xset::name::move_filename, true);
        on_toggled(nullptr, mset);
    }
}

static void
on_options_button_press(GtkWidget* btn, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)btn;
    GtkWidget* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();

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
        set->disable = mset->is_link;
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_target);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        set->disable = !mset->is_link;
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_copy);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
        set->disable = mset->clip_copy;
    }

    {
        const auto set = xset::set::get(xset::name::move_link);
        xset_set_cb(set, (GFunc)on_toggled, mset.get());
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

i32
ptk::action::rename_files(ptk::browser* browser, const std::filesystem::path& cwd,
                          const std::shared_ptr<vfs::file>& file, const char* dest_dir,
                          bool clip_copy) noexcept
{
    GtkWidget* task_view = nullptr;
    i32 ret = 1;
    bool target_missing = false;

    if (cwd.empty() || !std::filesystem::exists(cwd))
    {
        return 0;
    }

    const auto mset = std::make_shared<MoveSet>(file);

    const std::string_view original_filename = file->name();

    mset->is_dir = file->is_directory();
    mset->is_link = file->is_symlink();
    mset->clip_copy = clip_copy;
    mset->full_path = cwd / original_filename;
    if (dest_dir)
    {
        mset->new_path = std::filesystem::path() / dest_dir / original_filename;
    }
    else
    {
        mset->new_path = mset->full_path;
    }

    mset->old_path = cwd;

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
        mset->parent = gtk_widget_get_toplevel(GTK_WIDGET(browser));
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

    // Window
    gtk_widget_set_size_request(GTK_WIDGET(mset->dlg), 800, 500);
    gtk_window_set_resizable(GTK_WINDOW(mset->dlg), true);
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

    // Target
    if (mset->is_link)
    {
        mset->label_target = GTK_LABEL(gtk_label_new(nullptr));
        gtk_label_set_markup_with_mnemonic(mset->label_target, "<b>_Target:</b>");
        gtk_widget_set_halign(GTK_WIDGET(mset->label_target), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(mset->label_target), GtkAlign::GTK_ALIGN_END);
        mset->entry_target = GTK_ENTRY(gtk_entry_new());
        gtk_label_set_mnemonic_widget(mset->label_target, GTK_WIDGET(mset->entry_target));
        gtk_label_set_selectable(mset->label_target, true);
        // clang-format off
        g_signal_connect(G_OBJECT(mset->entry_target), "key-press-event", G_CALLBACK(on_key_press), mset.get());
        // clang-format on

        gtk_entry_set_text(GTK_ENTRY(mset->entry_target), mset->mime_type.data());
        gtk_editable_set_editable(GTK_EDITABLE(mset->entry_target), false);

        // clang-format off
        g_signal_connect(G_OBJECT(mset->entry_target), "changed", G_CALLBACK(on_move_change), mset.get());
        // clang-format on
    }
    else
    {
        mset->label_target = nullptr;
    }

    // Filename
    mset->label_full_name = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_full_name, "<b>_Filename:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_full_name), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_full_name), GtkAlign::GTK_ALIGN_START);
    mset->scroll_full_name = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    mset->input_full_name = ptk::utils::multi_input_new(mset->scroll_full_name);
    gtk_label_set_mnemonic_widget(mset->label_full_name, GTK_WIDGET(mset->input_full_name));
    gtk_label_set_selectable(mset->label_full_name, true);
    mset->buf_full_name = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_full_name));

    // clang-format off
    g_signal_connect(G_OBJECT(mset->input_full_name), "key-press-event", G_CALLBACK(on_key_press), mset.get());
    g_signal_connect(G_OBJECT(mset->buf_full_name), "changed", G_CALLBACK(on_move_change), mset.get());
    // clang-format on

    // Parent
    mset->label_path = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_path, "<b>_Parent:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_path), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_path), GtkAlign::GTK_ALIGN_START);
    mset->scroll_path = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    mset->input_path = ptk::utils::multi_input_new(mset->scroll_path);
    gtk_label_set_mnemonic_widget(mset->label_path, GTK_WIDGET(mset->input_path));
    gtk_label_set_selectable(mset->label_path, true);
    mset->buf_path = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_path));

    // clang-format off
    g_signal_connect(G_OBJECT(mset->input_path), "key-press-event", G_CALLBACK(on_key_press), mset.get());
    g_signal_connect(G_OBJECT(mset->buf_path), "changed", G_CALLBACK(on_move_change), mset.get());
    // clang-format on

    // Path
    mset->label_full_path = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_markup_with_mnemonic(mset->label_full_path, "<b>P_ath:</b>");
    gtk_widget_set_halign(GTK_WIDGET(mset->label_full_path), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(mset->label_full_path), GtkAlign::GTK_ALIGN_START);
    mset->scroll_full_path = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    // set initial path
    mset->input_full_path =
        ptk::utils::multi_input_new(mset->scroll_full_path, mset->new_path.string());
    gtk_label_set_mnemonic_widget(mset->label_full_path, GTK_WIDGET(mset->input_full_path));
    gtk_label_set_selectable(mset->label_full_path, true);
    mset->buf_full_path = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mset->input_full_path));

    // clang-format off
    g_signal_connect(G_OBJECT(mset->input_full_path), "key-press-event", G_CALLBACK(on_key_press), mset.get());
    g_signal_connect(G_OBJECT(mset->buf_full_path), "changed", G_CALLBACK(on_move_change), mset.get());
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

    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_move), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_copy), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_link), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_copy_target), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_link_target), false);
    gtk_widget_set_sensitive(mset->opt_copy_target, mset->is_link && !target_missing);
    gtk_widget_set_sensitive(mset->opt_link_target, mset->is_link);

    // Pack
    GtkBox* dlg_vbox = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(mset->dlg)));

    gtk_widget_set_margin_start(GTK_WIDGET(dlg_vbox), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(dlg_vbox), 10);
    gtk_widget_set_margin_top(GTK_WIDGET(dlg_vbox), 10);
    gtk_widget_set_margin_bottom(GTK_WIDGET(dlg_vbox), 10);

    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->label_full_name), false, true, 4);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->scroll_full_name), true, true, 0);

    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->label_path), false, true, 4);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->scroll_path), true, true, 0);

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
        gtk_box_pack_start(mset->hbox_target, GTK_WIDGET(gtk_label_new(" ")), false, true, 0);
        gtk_box_pack_start(mset->hbox_target, GTK_WIDGET(mset->entry_target), true, true, 0);
        gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->hbox_target), false, true, 5);
    }

    GtkBox* hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4));
    gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_move), false, true, 3);
    gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_copy), false, true, 3);
    gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_link), false, true, 3);
    gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_copy_target), false, true, 3);
    gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_link_target), false, true, 3);
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

    // signals
    // clang-format off
    g_signal_connect(G_OBJECT(mset->opt_move), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_copy), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_link), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_copy_target), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_link_target), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    // clang-format on

    // init
    on_move_change(GTK_WIDGET(mset->buf_full_path), mset);
    on_opt_toggled(nullptr, mset);

    // select filename text widget
    select_input(GTK_WIDGET(mset->input_full_name), mset);
    gtk_widget_grab_focus(GTK_WIDGET(mset->input_full_name));

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
                full_path = mset->full_path.parent_path() / full_path;
            }
            // const auto full_name = full_path.filename();
            const auto path = full_path.parent_path();
            const auto old_path = mset->full_path.parent_path();
            bool overwrite = false;

            if (response == GtkResponseType::GTK_RESPONSE_APPLY)
            {
                ret = 2;
            }

            if (mset->full_path_same || full_path == mset->full_path)
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

            if (copy || copy_target)
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
