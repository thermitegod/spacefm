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
#include "ptk/ptk-file-action-create.hxx"
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
    std::filesystem::path new_path;
    bool is_dir{false};
    bool is_link{false};
    ptk::action::create_mode mode;

    GtkWidget* dlg{nullptr};
    GtkWidget* parent{nullptr};
    ptk::browser* browser{nullptr};

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

    GtkWidget* opt_new_file{nullptr};
    GtkWidget* opt_new_folder{nullptr};
    GtkWidget* opt_new_link{nullptr};

    GtkWidget* options{nullptr};
    GtkWidget* revert{nullptr};
    GtkWidget* cancel{nullptr};
    GtkWidget* next{nullptr};
    GtkWidget* open{nullptr};

    bool full_path_exists{false};
    bool full_path_exists_dir{false};
    bool full_path_same{false};
    bool path_missing{false};
    bool path_exists_file{false};
    bool mode_change{false};
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

    // change is_dir to reflect state of new directory or link option
    const bool new_folder = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
    const bool new_link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));

    const std::string text = gtk_entry_get_text(GTK_ENTRY(mset->entry_target));

    if (new_folder || (new_link && std::filesystem::is_directory(text) && text.starts_with('/')))
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

    std::error_code ec;
    const bool equivalent = std::filesystem::equivalent(full_path, mset->full_path, ec);
    if (equivalent && !ec)
    {
        full_path_same = true;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link)))
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

        if (full_path_same && mset->mode == ptk::action::create_mode::link)
        {
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

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link)))
    {
        path = gtk_entry_get_text(GTK_ENTRY(mset->entry_target));

        gtk_widget_set_sensitive(mset->next,
                                 (!(full_path_same && full_path_exists) && !full_path_exists_dir));
    }

    if (mset->open)
    {
        gtk_widget_set_sensitive(mset->open, gtk_widget_get_sensitive(mset->next));
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

        const auto filename_parts = vfs::utils::split_basename_extension(full_name);
        gtk_text_buffer_get_iter_at_offset(buf, &iter, filename_parts.basename.size());
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

    const bool new_file = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file));
    const bool new_folder = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
    const bool new_link = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));

    std::string desc;
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

    // Window Icon
    gtk_window_set_icon_name(GTK_WINDOW(mset->dlg), "gtk-new");

    // title
    const auto title = std::format("Create New {}", desc);
    gtk_window_set_title(GTK_WINDOW(mset->dlg), title.data());

    mset->full_path_same = false;
    mset->mode_change = true;
    on_move_change(GTK_WIDGET(mset->buf_full_path), mset);
    on_toggled(nullptr, mset);
}

static void
on_toggled(GtkMenuItem* item, const std::shared_ptr<MoveSet>& mset) noexcept
{
    (void)item;
    bool someone_is_visible = false;

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

    [[maybe_unused]] const bool new_file =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_file));
    [[maybe_unused]] const bool new_folder =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder));
    [[maybe_unused]] const bool new_link =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mset->opt_new_link));

    if (new_link || (mset->is_link && xset_get_b(xset::name::move_target)))
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
        const auto set = xset::set::get(xset::name::separator);
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::move_dlg_confirm_create);
        xset_add_menuitem(mset->browser, popup, accel_group, set);
    }

    gtk_widget_show_all(GTK_WIDGET(popup));
    g_signal_connect(G_OBJECT(popup), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
}

i32
ptk::action::create_files(ptk::browser* browser, const std::filesystem::path& cwd,
                          const std::shared_ptr<vfs::file>& file,
                          const ptk::action::create_mode mode, AutoOpenCreate* auto_open) noexcept
{
    GtkWidget* task_view = nullptr;
    i32 ret = 1;

    if (cwd.empty() || !std::filesystem::exists(cwd))
    {
        return 0;
    }

    const auto mset = std::make_shared<MoveSet>(file);

    if (mode == ptk::action::create_mode::link && file)
    {
        const auto full_name = file->name();
        mset->full_path = cwd / full_name;
        mset->new_path = mset->full_path;
        mset->is_dir = file->is_directory(); // is_dir is dynamic for create
        mset->is_link = file->is_symlink();
    }
    else
    {
        mset->full_path = vfs::utils::unique_path(cwd, "new");
        mset->new_path = mset->full_path;
        mset->is_dir = false; // is_dir is dynamic for create
        mset->is_link = false;
    }

    mset->mode = mode;

    mset->full_path_exists = false;
    mset->full_path_exists_dir = false;
    mset->full_path_same = false;
    mset->path_missing = false;
    mset->path_exists_file = false;

    // Dialog
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

    mset->next = gtk_button_new_with_mnemonic("Create");
    gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                 mset->next,
                                 GtkResponseType::GTK_RESPONSE_OK);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->next), false);

    if (auto_open)
    {
        mset->open = gtk_button_new_with_mnemonic("_Open");
        gtk_dialog_add_action_widget(GTK_DIALOG(mset->dlg),
                                     mset->open,
                                     GtkResponseType::GTK_RESPONSE_APPLY);
        gtk_widget_set_focus_on_click(GTK_WIDGET(mset->open), false);
    }

    // Window
    gtk_widget_set_size_request(GTK_WIDGET(mset->dlg), 800, 500);
    gtk_window_set_resizable(GTK_WINDOW(mset->dlg), true);
    gtk_widget_show_all(GTK_WIDGET(mset->dlg));

    // Entries

    // Target
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

    if (!mset->new_path.empty() && file)
    {
        gtk_entry_set_text(GTK_ENTRY(mset->entry_target), mset->new_path.c_str());
    }
    // clang-format off
    g_signal_connect(G_OBJECT(mset->entry_target), "changed", G_CALLBACK(on_move_change), mset.get());
    // clang-format on

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
    mset->opt_new_file = gtk_radio_button_new_with_mnemonic(nullptr, "Fil_e");
    mset->opt_new_folder =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_new_file),
                                                       "Dir_ectory");
    mset->opt_new_link =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(mset->opt_new_file),
                                                       "_Link");

    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_new_file), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_new_folder), false);
    gtk_widget_set_focus_on_click(GTK_WIDGET(mset->opt_new_link), false);

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

    mset->hbox_target = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
    if (mset->label_target)
    {
        gtk_box_pack_start(mset->hbox_target, GTK_WIDGET(mset->label_target), false, true, 0);
        gtk_box_pack_start(mset->hbox_target, GTK_WIDGET(mset->entry_target), true, true, 3);
        gtk_box_pack_start(dlg_vbox, GTK_WIDGET(mset->hbox_target), false, true, 5);
    }

    GtkBox* hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4));
    gtk_box_pack_start(hbox, GTK_WIDGET(gtk_label_new("New")), false, true, 3);
    gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_new_file), false, true, 3);
    gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_new_folder), false, true, 3);
    gtk_box_pack_start(hbox, GTK_WIDGET(mset->opt_new_link), false, true, 3);
    gtk_box_pack_start(hbox, GTK_WIDGET(gtk_label_new("  ")), false, true, 3);
    gtk_box_pack_start(dlg_vbox, GTK_WIDGET(hbox), false, true, 10);

    // show
    gtk_widget_show_all(GTK_WIDGET(mset->dlg));
    on_toggled(nullptr, mset);
    if (mode == ptk::action::create_mode::dir)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_folder), true);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_file), false);
    }
    else if (mode == ptk::action::create_mode::link)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_link), true);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mset->opt_new_file), false);
    }

    // signals
    // clang-format off
    g_signal_connect(G_OBJECT(mset->opt_new_file), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_new_folder), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
    g_signal_connect(G_OBJECT(mset->opt_new_link), "toggled", G_CALLBACK(on_opt_toggled), mset.get());
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
            bool overwrite = false;

            if (response == GtkResponseType::GTK_RESPONSE_APPLY)
            {
                ret = 2;
            }

            // determine job
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

            if (new_link)
            {
                // new link task
                ptk::file_task* ptask = ptk_file_exec_new("Create Link", mset->parent, task_view);

                std::string entry_text = gtk_entry_get_text(GTK_ENTRY(mset->entry_target));

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
            else if (new_file)
            {
                // new file task
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
            else if (new_folder)
            {
                // new directory task
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
